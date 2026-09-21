# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 785/1149 lines (68.32%)

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
|      - |   10 | `#include <time.h> /* touch() resolves php's "now" default here, not in the driver */` |
|      - |   11 |  |
|      - |   12 | `#ifdef __UNIXES__` |
|      - |   13 | `#include <unistd.h>` |
|      - |   14 | `#include <sys/wait.h>` |
|      - |   15 | `#include <fcntl.h>` |
|      - |   16 | `#include <signal.h>` |
|      - |   17 | `#endif` |
|      - |   18 | `/*` |
|      - |   19 | ` * This file implement a virtual file systems (VFS) for the PH7 engine.` |
|      - |   20 | ` */` |
|      - |   21 | `/*` |
|      - |   22 | ` * Given a string containing the path of a file or directory, this function` |
|      - |   23 | ` * return the parent directory's path.` |
|      - |   24 | ` */` |
|  13970 |   25 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)` |
|      5 |   26 | `{` |
|      - |   27 | `	/* php_dirname: strip any trailing separators, cut at the last remaining one,` |
|      - |   28 | `	 * then strip trailing separators off the parent too. The previous version` |
|      - |   29 | `	 * scanned back to the first separator and stopped, so it never coped with a` |
|      - |   30 | `	 * trailing separator or a run of them: dirname("/a/") answered "/a" instead` |
|      - |   31 | `	 * of "/", dirname("a//b") answered "a/", and dirname("///") answered "//".` |
|      - |   32 | `	 * It also answered "." for the empty string, where php answers "". */` |
|      - |   33 | `	int c,d,iEnd,i;` |
|      - |   34 | `#ifdef __WINNT__` |
|      5 |   35 | `	const char *zRoot = "\\";` |
|      - |   36 | `#else` |
|  13970 |   37 | `	const char *zRoot = "/";` |
|      - |   38 | `#endif` |
|  13975 |   39 | `	c = d = '/';` |
|      - |   40 | `#ifdef __WINNT__` |
|      5 |   41 | `	d = '\\';` |
|      - |   42 | `#endif` |
|      - |   43 | `#define DIR_IS_SEP(x) ( (int)(x) == c \|\| (int)(x) == d )` |
|  13975 |   44 | `	if( nByte < 1 ){` |
|      - |   45 | `		/* php returns the empty string for the empty path */` |
|      5 |   46 | `		*pLen = 0;` |
|      5 |   47 | `		return "";` |
|      - |   48 | `	}` |
|  13971 |   49 | `	iEnd = nByte;` |
|  20982 |   50 | `	while( iEnd > 0 && DIR_IS_SEP(zPath[iEnd - 1]) ){` |
|     29 |   51 | `		iEnd--;` |
|      1 |   52 | `	}` |
|  13971 |   53 | `	if( iEnd == 0 ){` |
|      - |   54 | `		/* The path is nothing but separators: the root is its own parent */` |
|     17 |   55 | `		*pLen = (int)sizeof(char);` |
|     17 |   56 | `		return zRoot;` |
|      - |   57 | `	}` |
|      - |   58 | `	/* Walk back to the separator that ends the parent directory */` |
|  13955 |   59 | `	i = iEnd;` |
| 368249 |   60 | `	while( i > 0 && !DIR_IS_SEP(zPath[i - 1]) ){` |
| 354299 |   61 | `		i--;` |
|      5 |   62 | `	}` |
|  13955 |   63 | `	if( i == 0 ){` |
|      - |   64 | `		/* No separator at all,return "." as the current directory */` |
|     70 |   65 | `		*pLen = (int)sizeof(char);` |
|     70 |   66 | `		return ".";` |
|      - |   67 | `	}` |
|      - |   68 | `	/* Drop the separator, plus any that repeat before it */` |
|  34700 |   69 | `	while( i > 1 && DIR_IS_SEP(zPath[i - 1]) ){` |
|  13877 |   70 | `		i--;` |
|      5 |   71 | `	}` |
|  13887 |   72 | `	if( i == 1 && DIR_IS_SEP(zPath[0]) ){` |
|     13 |   73 | `		*pLen = (int)sizeof(char);` |
|     13 |   74 | `		return zRoot;` |
|      - |   75 | `	}` |
|  13875 |   76 | `	*pLen = i;` |
|  13875 |   77 | `	return zPath;` |
|      - |   78 | `#undef DIR_IS_SEP` |
|   6990 |   79 | `}` |
|      - |   80 | `/*` |
|      - |   81 | ` * php_basename: drop any trailing separators, then answer what follows the last` |
|      - |   82 | ` * remaining one. Shared by basename() and pathinfo() — they used to hand-roll the` |
|      - |   83 | ` * same walk separately, and pathinfo()'s copy kept the trailing separator run` |
|      - |   84 | `` * (`pathinfo("/var/www/")` answered basename "" where php answers "www").`` |
|      - |   85 | ` */` |
|  13814 |   86 | `PH7_PRIVATE const char * PH7_ExtractBaseName(const char *zPath,int nByte,int *pLen)` |
|      5 |   87 | `{` |
|      - |   88 | `	int c,d,iEnd,i;` |
|  13819 |   89 | `	c = d = '/';` |
|      - |   90 | `#ifdef __WINNT__` |
|      5 |   91 | `	d = '\\';` |
|      - |   92 | `#endif` |
|      - |   93 | `#define DIR_IS_SEP(x) ( (int)(x) == c \|\| (int)(x) == d )` |
|  13819 |   94 | `	iEnd = nByte;` |
|  20744 |   95 | `	while( iEnd > 0 && DIR_IS_SEP(zPath[iEnd - 1]) ){` |
|     19 |   96 | `		iEnd--;` |
|      1 |   97 | `	}` |
|  13819 |   98 | `	if( iEnd < 1 ){` |
|      - |   99 | `		/* Empty, or nothing but separators: php answers the empty string */` |
|     17 |  100 | `		*pLen = 0;` |
|     17 |  101 | `		return "";` |
|      - |  102 | `	}` |
|  13803 |  103 | `	i = iEnd;` |
| 366863 |  104 | `	while( i > 0 && !DIR_IS_SEP(zPath[i - 1]) ){` |
| 353065 |  105 | `		i--;` |
|      5 |  106 | `	}` |
|  13803 |  107 | `	*pLen = iEnd - i;` |
|  13803 |  108 | `	return &zPath[i];` |
|      - |  109 | `#undef DIR_IS_SEP` |
|   6912 |  110 | `}` |
|      - |  111 | `/*` |
|      - |  112 | ` * Compile the VFS implementations when builtins are enabled OR when disk I/O` |
|      - |  113 | ` * is explicitly enabled (i.e. PH7_DISABLE_DISK_IO is NOT defined).` |
|      - |  114 | ` */` |
|      - |  115 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  116 | `/*` |
|      - |  117 | ` * strerror() trips MSVC's C4996 "may be unsafe" deprecation under /WX. It is a` |
|      - |  118 | ` * standard C function we use deliberately to mirror php's IO error text; wrap it` |
|      - |  119 | ` * once with the deprecation suppressed. The pragma is _MSC_VER-guarded so the` |
|      - |  120 | ` * GCC/-Werror Linux build never sees an unknown-pragma warning.` |
|      - |  121 | ` */` |
|  20952 |  122 | `PH7_PRIVATE const char * VfsStrerror(int iErr)` |
|      5 |  123 | `{` |
|      - |  124 | `#if defined(_MSC_VER)` |
|      - |  125 | `#pragma warning(push)` |
|      - |  126 | `#pragma warning(disable:4996)` |
|      - |  127 | `#endif` |
|  20957 |  128 | `	return strerror(iErr);` |
|      - |  129 | `#if defined(_MSC_VER)` |
|      - |  130 | `#pragma warning(pop)` |
|      - |  131 | `#endif` |
|      5 |  132 | `}` |
|      - |  133 | `/*` |
|      - |  134 | ` * php's non-open IO failures: "unlink(/nope): No such file or directory".` |
|      - |  135 | ` * PH7 returned FALSE in SILENCE for unlink/rmdir/mkdir/rename/chdir/opendir/scandir and` |
|      - |  136 | ` * filesize, so a script could not tell a failed operation from a successful one without` |
|      - |  137 | ` * checking the return value it never got told to check.` |
|      - |  138 | ` */` |
|  20926 |  139 | `static void VfsThrowSysWarning(ph7_context *pCtx,const char *zPath)` |
|      5 |  140 | `{` |
|  31394 |  141 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s",` |
|  20926 |  142 | `		ph7_function_name(pCtx),zPath ? zPath : "",VfsStrerror(errno));` |
|  20931 |  143 | `}` |
|     14 |  144 | `PH7_PRIVATE void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile)` |
|      2 |  145 | `{` |
|     23 |  146 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): Failed to open stream: %s",` |
|     14 |  147 | `		ph7_function_name(pCtx),zFile ? zFile : "",VfsStrerror(errno));` |
|     16 |  148 | `}` |
|      - |  149 | `/*` |
|      - |  150 | ` * bool chdir(string $directory)` |
|      - |  151 | ` *  Change the current directory.` |
|      - |  152 | ` * Parameters` |
|      - |  153 | ` *  $directory` |
|      - |  154 | ` *   The new current directory` |
|      - |  155 | ` * Return` |
|      - |  156 | ` *  TRUE on success or FALSE on failure.` |
|      - |  157 | ` */` |
|  13234 |  158 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  159 | `{` |
|      - |  160 | `	const char *zPath;` |
|      - |  161 | `	ph7_vfs *pVfs;` |
|      - |  162 | `	int rc;` |
|      - |  163 | `	/* Only the ARITY is checked here: php coerces a scalar $directory to string,` |
|      - |  164 | `	 * so chdir(123) attempts "123" and warns that it does not exist. Requiring a` |
|      - |  165 | `	 * string outright made that call return FALSE silently, with no diagnostic. */` |
|  13239 |  166 | `	if( nArg < 1 ){` |
|      - |  167 | `		/* Missing argument,return FALSE */` |
|    ! 0 |  168 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  169 | `		return PH7_OK;` |
|      - |  170 | `	}` |
|      - |  171 | `	/* Point to the underlying vfs */` |
|  13239 |  172 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  13239 |  173 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |  174 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  175 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  176 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  177 | `			ph7_function_name(pCtx)` |
|      - |  178 | `			);` |
|    ! 0 |  179 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  180 | `		return PH7_OK;` |
|      - |  181 | `	}` |
|      - |  182 | `	/* Point to the desired directory */` |
|  13239 |  183 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  184 | `	/* Perform the requested operation */` |
|  13239 |  185 | `	errno = 0;` |
|  13239 |  186 | `	rc = pVfs->xChdir(zPath);` |
|  13239 |  187 | `	if( rc != PH7_OK ){` |
|      - |  188 | `		/* chdir has its own php shape: no path, and the errno spelled out. */` |
|      8 |  189 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|      4 |  190 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|      2 |  191 | `	}` |
|      - |  192 | `	/* IO return value */` |
|  13239 |  193 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  13239 |  194 | `	return PH7_OK;` |
|   6622 |  195 | `}` |
|      - |  196 | `/*` |
|      - |  197 | ` * bool chroot(string $directory)` |
|      - |  198 | ` *  Change the root directory.` |
|      - |  199 | ` * Parameters` |
|      - |  200 | ` *  $directory` |
|      - |  201 | ` *   The path to change the root directory to` |
|      - |  202 | ` * Return` |
|      - |  203 | ` *  TRUE on success or FALSE on failure.` |
|      - |  204 | ` */` |
|      6 |  205 | `static int PH7_vfs_chroot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  206 | `{` |
|      - |  207 | `	const char *zPath;` |
|      - |  208 | `	ph7_vfs *pVfs;` |
|      - |  209 | `	int rc;` |
|      7 |  210 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  211 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 |  212 | `		ph7_result_bool(pCtx,0);` |
|      5 |  213 | `		return PH7_OK;` |
|      - |  214 | `	}` |
|      - |  215 | `	/* Point to the underlying vfs */` |
|      3 |  216 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  217 | `	if( pVfs == 0 \|\| pVfs->xChroot == 0 ){` |
|      - |  218 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  219 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  220 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  221 | `			ph7_function_name(pCtx)` |
|      - |  222 | `			);` |
|    ! 0 |  223 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  224 | `		return PH7_OK;` |
|      - |  225 | `	}` |
|      - |  226 | `	/* Point to the desired directory */` |
|      3 |  227 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  228 | `	/* Perform the requested operation */` |
|      3 |  229 | `	rc = pVfs->xChroot(zPath);` |
|      - |  230 | `	/* IO return value */` |
|      3 |  231 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 |  232 | `	return PH7_OK;` |
|      4 |  233 | `}` |
|      - |  234 | `/*` |
|      - |  235 | ` * string getcwd(void)` |
|      - |  236 | ` *  Gets the current working directory.` |
|      - |  237 | ` * Parameters` |
|      - |  238 | ` *  None` |
|      - |  239 | ` * Return` |
|      - |  240 | ` *  Returns the current working directory on success, or FALSE on failure.` |
|      - |  241 | ` */` |
|     18 |  242 | `static int PH7_vfs_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  243 | `{` |
|      - |  244 | `	ph7_vfs *pVfs;` |
|      - |  245 | `	int rc;` |
|      - |  246 | `	/* Point to the underlying vfs */` |
|     23 |  247 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     23 |  248 | `	if( pVfs == 0 \|\| pVfs->xGetcwd == 0 ){` |
|    ! 0 |  249 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 |  250 | `		SXUNUSED(apArg);` |
|      - |  251 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  252 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  253 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  254 | `			ph7_function_name(pCtx)` |
|      - |  255 | `			);` |
|    ! 0 |  256 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  257 | `		return PH7_OK;` |
|      - |  258 | `	}` |
|     23 |  259 | `	ph7_result_string(pCtx,"",0);` |
|      - |  260 | `	/* Perform the requested operation */` |
|     23 |  261 | `	rc = pVfs->xGetcwd(pCtx);` |
|     23 |  262 | `	if( rc != PH7_OK ){` |
|      - |  263 | `		/* Error,return FALSE */` |
|    ! 0 |  264 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  265 | `	}` |
|     23 |  266 | `	return PH7_OK;` |
|     14 |  267 | `}` |
|      - |  268 | `/*` |
|      - |  269 | ` * bool rmdir(string $directory)` |
|      - |  270 | ` *  Removes directory.` |
|      - |  271 | ` * Parameters` |
|      - |  272 | ` *  $directory` |
|      - |  273 | ` *   The path to the directory` |
|      - |  274 | ` * Return` |
|      - |  275 | ` *  TRUE on success or FALSE on failure.` |
|      - |  276 | ` */` |
|     52 |  277 | `static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  278 | `{` |
|      - |  279 | `	const char *zPath;` |
|      - |  280 | `	ph7_vfs *pVfs;` |
|      - |  281 | `	int rc;` |
|     55 |  282 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  283 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  284 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  285 | `		return PH7_OK;` |
|      - |  286 | `	}` |
|      - |  287 | `	/* Point to the underlying vfs */` |
|     55 |  288 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     55 |  289 | `	if( pVfs == 0 \|\| pVfs->xRmdir == 0 ){` |
|      - |  290 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  291 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  292 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  293 | `			ph7_function_name(pCtx)` |
|      - |  294 | `			);` |
|    ! 0 |  295 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  296 | `		return PH7_OK;` |
|      - |  297 | `	}` |
|      - |  298 | `	/* Point to the desired directory */` |
|     55 |  299 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  300 | `	/* Perform the requested operation */` |
|     55 |  301 | `	errno = 0;` |
|     55 |  302 | `	rc = pVfs->xRmdir(zPath);` |
|     55 |  303 | `	if( rc != PH7_OK ){` |
|      8 |  304 | `		VfsThrowSysWarning(pCtx,zPath);` |
|      3 |  305 | `	}` |
|      - |  306 | `	/* IO return value */` |
|     55 |  307 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     55 |  308 | `	return PH7_OK;` |
|     29 |  309 | `}` |
|      - |  310 | `/*` |
|      - |  311 | ` * bool is_dir(string $filename)` |
|      - |  312 | ` *  Tells whether the given filename is a directory.` |
|      - |  313 | ` * Parameters` |
|      - |  314 | ` *  $filename` |
|      - |  315 | ` *   Path to the file.` |
|      - |  316 | ` * Return` |
|      - |  317 | ` *  TRUE on success or FALSE on failure.` |
|      - |  318 | ` */` |
|   9048 |  319 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  320 | `{` |
|      - |  321 | `	const char *zPath;` |
|      - |  322 | `	ph7_vfs *pVfs;` |
|      - |  323 | `	int rc;` |
|   9053 |  324 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  325 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  326 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  327 | `		return PH7_OK;` |
|      - |  328 | `	}` |
|      - |  329 | `	/* Point to the underlying vfs */` |
|   9053 |  330 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   9053 |  331 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  332 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  333 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  334 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  335 | `			ph7_function_name(pCtx)` |
|      - |  336 | `			);` |
|    ! 0 |  337 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  338 | `		return PH7_OK;` |
|      - |  339 | `	}` |
|      - |  340 | `	/* Point to the desired directory */` |
|   9053 |  341 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  342 | `	/* Perform the requested operation */` |
|   9053 |  343 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  344 | `	/* IO return value */` |
|   9053 |  345 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   9053 |  346 | `	return PH7_OK;` |
|   4529 |  347 | `}` |
|      - |  348 | `/*` |
|      - |  349 | ` * bool mkdir(string $pathname[,int $mode = 0777 [,bool $recursive = false])` |
|      - |  350 | ` *  Make a directory.` |
|      - |  351 | ` * Parameters` |
|      - |  352 | ` *  $pathname` |
|      - |  353 | ` *   The directory path.` |
|      - |  354 | ` * $mode` |
|      - |  355 | ` *  The mode is 0777 by default, which means the widest possible access.` |
|      - |  356 | ` *  Note:` |
|      - |  357 | ` *   mode is ignored on Windows.` |
|      - |  358 | ` *   Note that you probably want to specify the mode as an octal number, which means` |
|      - |  359 | ` *   it should have a leading zero. The mode is also modified by the current umask` |
|      - |  360 | ` *   which you can change using umask().` |
|      - |  361 | ` * $recursive` |
|      - |  362 | ` *  Allows the creation of nested directories specified in the pathname.` |
|      - |  363 | ` *  Defaults to FALSE. (Not used)` |
|      - |  364 | ` * Return` |
|      - |  365 | ` *  TRUE on success or FALSE on failure.` |
|      - |  366 | ` */` |
|     52 |  367 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  368 | `{` |
|     56 |  369 | `	int iRecursive = 0;` |
|      - |  370 | `	const char *zPath;` |
|      - |  371 | `	ph7_vfs *pVfs;` |
|      - |  372 | `	int iMode,rc;` |
|     56 |  373 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  374 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  375 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  376 | `		return PH7_OK;` |
|      - |  377 | `	}` |
|      - |  378 | `	/* Point to the underlying vfs */` |
|     56 |  379 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     56 |  380 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  381 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  382 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  383 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  384 | `			ph7_function_name(pCtx)` |
|      - |  385 | `			);` |
|    ! 0 |  386 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  387 | `		return PH7_OK;` |
|      - |  388 | `	}` |
|      - |  389 | `	/* Point to the desired directory */` |
|     56 |  390 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  391 | `#ifdef __WINNT__` |
|      4 |  392 | `	iMode = 0;` |
|      - |  393 | `#else` |
|      - |  394 | `	/* Assume UNIX */` |
|     52 |  395 | `	iMode = 0777;` |
|      - |  396 | `#endif` |
|     56 |  397 | `	if( nArg > 1 ){` |
|    ! 0 |  398 | `		iMode = ph7_value_to_int(apArg[1]);` |
|    ! 0 |  399 | `		if( nArg > 2 ){` |
|    ! 0 |  400 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|    ! 0 |  401 | `		}` |
|    ! 0 |  402 | `	}` |
|      - |  403 | `	/* Perform the requested operation */` |
|     56 |  404 | `	errno = 0;` |
|     56 |  405 | `	rc = pVfs->xMkdir(zPath,iMode,iRecursive);` |
|     56 |  406 | `	if( rc != PH7_OK ){` |
|      - |  407 | `		/* php does NOT name the path for mkdir: "mkdir(): File exists" */` |
|    ! 0 |  408 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|    ! 0 |  409 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|    ! 0 |  410 | `	}` |
|      - |  411 | `	/* IO return value */` |
|     56 |  412 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     56 |  413 | `	return PH7_OK;` |
|     30 |  414 | `}` |
|      - |  415 | `/*` |
|      - |  416 | ` * bool rename(string $oldname,string $newname)` |
|      - |  417 | ` *  Attempts to rename oldname to newname.` |
|      - |  418 | ` * Parameters` |
|      - |  419 | ` *  $oldname` |
|      - |  420 | ` *   Old name.` |
|      - |  421 | ` *  $newname` |
|      - |  422 | ` *   New name.` |
|      - |  423 | ` * Return` |
|      - |  424 | ` *  TRUE on success or FALSE on failure.` |
|      - |  425 | ` */` |
|      2 |  426 | `static int PH7_vfs_rename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  427 | `{` |
|      - |  428 | `	const char *zOld,*zNew;` |
|      - |  429 | `	ph7_vfs *pVfs;` |
|      - |  430 | `	int rc;` |
|      3 |  431 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - |  432 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  433 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  434 | `		return PH7_OK;` |
|      - |  435 | `	}` |
|      - |  436 | `	/* Point to the underlying vfs */` |
|      3 |  437 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  438 | `	if( pVfs == 0 \|\| pVfs->xRename == 0 ){` |
|      - |  439 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  440 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  441 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  442 | `			ph7_function_name(pCtx)` |
|      - |  443 | `			);` |
|    ! 0 |  444 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  445 | `		return PH7_OK;` |
|      - |  446 | `	}` |
|      - |  447 | `	/* Perform the requested operation */` |
|      3 |  448 | `	zOld = ph7_value_to_string(apArg[0],0);` |
|      3 |  449 | `	zNew = ph7_value_to_string(apArg[1],0);` |
|      3 |  450 | `	errno = 0;` |
|      3 |  451 | `	rc = pVfs->xRename(zOld,zNew);` |
|      3 |  452 | `	if( rc != PH7_OK ){` |
|      - |  453 | `		/* php names BOTH paths here */` |
|    ! 0 |  454 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s,%s): %s",` |
|    ! 0 |  455 | `			ph7_function_name(pCtx),zOld,zNew,VfsStrerror(errno));` |
|    ! 0 |  456 | `	}` |
|      - |  457 | `	/* IO result */` |
|      3 |  458 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 |  459 | `	return PH7_OK;` |
|      2 |  460 | `}` |
|      - |  461 | `/*` |
|      - |  462 | ` * string realpath(string $path)` |
|      - |  463 | ` *  Returns canonicalized absolute pathname.` |
|      - |  464 | ` * Parameters` |
|      - |  465 | ` *  $path` |
|      - |  466 | ` *   Target path.` |
|      - |  467 | ` * Return` |
|      - |  468 | ` *  Canonicalized absolute pathname on success. or FALSE on failure.` |
|      - |  469 | ` */` |
|      6 |  470 | `static int PH7_vfs_realpath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  471 | `{` |
|      - |  472 | `	const char *zPath;` |
|      - |  473 | `	ph7_vfs *pVfs;` |
|      - |  474 | `        int rc;` |
|      8 |  475 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  476 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  477 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  478 | `		return PH7_OK;` |
|      - |  479 | `	}` |
|      - |  480 | `	/* Point to the underlying vfs */` |
|      8 |  481 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      8 |  482 | `	if( pVfs == 0 \|\| pVfs->xRealpath == 0 ){` |
|      - |  483 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  484 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  485 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  486 | `			ph7_function_name(pCtx)` |
|      - |  487 | `			);` |
|    ! 0 |  488 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  489 | `		return PH7_OK;` |
|      - |  490 | `	}` |
|      - |  491 | `	/* Set an empty string untnil the underlying OS interface change that */` |
|      8 |  492 | `	ph7_result_string(pCtx,"",0);` |
|      - |  493 | `	/* Perform the requested operation */` |
|      8 |  494 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      8 |  495 | `	rc = pVfs->xRealpath(zPath,pCtx);` |
|      8 |  496 | `	if( rc != PH7_OK ){` |
|      2 |  497 | `	 ph7_result_bool(pCtx,0);` |
|      1 |  498 | `	}` |
|      8 |  499 | `	return PH7_OK;` |
|      5 |  500 | `}` |
|      - |  501 | `/*` |
|      - |  502 | ` * int sleep(int $seconds)` |
|      - |  503 | ` *  Delays the program execution for the given number of seconds.` |
|      - |  504 | ` * Parameters` |
|      - |  505 | ` *  $seconds` |
|      - |  506 | ` *   Halt time in seconds.` |
|      - |  507 | ` * Return` |
|      - |  508 | ` *  Zero on success or FALSE on failure.` |
|      - |  509 | ` */` |
|     10 |  510 | `static int PH7_vfs_sleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  511 | `{` |
|      - |  512 | `	ph7_vfs *pVfs;` |
|      - |  513 | `	int rc,nSleep;` |
|     11 |  514 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  515 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  516 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  517 | `		return PH7_OK;` |
|      - |  518 | `	}` |
|      - |  519 | `	/* Point to the underlying vfs */` |
|     11 |  520 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 |  521 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  522 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  523 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  524 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  525 | `			ph7_function_name(pCtx)` |
|      - |  526 | `			);` |
|    ! 0 |  527 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  528 | `		return PH7_OK;` |
|      - |  529 | `	}` |
|      - |  530 | `	/* Amount to sleep */` |
|     11 |  531 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|     11 |  532 | `	if( nSleep < 0 ){` |
|      - |  533 | `		/* php raises rather than sleeping for an unsigned-wrapped eternity */` |
|      3 |  534 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  535 | `			"sleep(): Argument #1 ($seconds) must be greater than or equal to 0");` |
|      - |  536 | `	}` |
|      - |  537 | `	/* Perform the requested operation (Microseconds) */` |
|      9 |  538 | `	rc = pVfs->xSleep((unsigned int)(nSleep * SX_USEC_PER_SEC));` |
|      9 |  539 | `	if( rc != PH7_OK ){` |
|      - |  540 | `		/* Return FALSE */` |
|    ! 0 |  541 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  542 | `	}else{` |
|      - |  543 | `		/* Return zero */` |
|      9 |  544 | `		ph7_result_int(pCtx,0);` |
|      - |  545 | `	}` |
|      9 |  546 | `	return PH7_OK;` |
|      6 |  547 | `}` |
|      - |  548 | `/*` |
|      - |  549 | ` * void usleep(int $micro_seconds)` |
|      - |  550 | ` *  Delays program execution for the given number of micro seconds.` |
|      - |  551 | ` * Parameters` |
|      - |  552 | ` *  $micro_seconds` |
|      - |  553 | ` *   Halt time in micro seconds. A micro second is one millionth of a second.` |
|      - |  554 | ` * Return` |
|      - |  555 | ` *  None.` |
|      - |  556 | ` */` |
|     58 |  557 | `static int PH7_vfs_usleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  558 | `{` |
|      - |  559 | `	ph7_vfs *pVfs;` |
|      - |  560 | `	int nSleep;` |
|     59 |  561 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  562 | `		/* Missing/Invalid argument,return immediately */` |
|    ! 0 |  563 | `		return PH7_OK;` |
|      - |  564 | `	}` |
|      - |  565 | `	/* Point to the underlying vfs */` |
|     59 |  566 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     59 |  567 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  568 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  569 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  570 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 |  571 | `			ph7_function_name(pCtx)` |
|      - |  572 | `			);` |
|    ! 0 |  573 | `		return PH7_OK;` |
|      - |  574 | `	}` |
|      - |  575 | `	/* Amount to sleep */` |
|     59 |  576 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|     59 |  577 | `	if( nSleep < 0 ){` |
|      - |  578 | `		/* php raises rather than sleeping for an unsigned-wrapped eternity */` |
|      3 |  579 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  580 | `			"usleep(): Argument #1 ($microseconds) must be greater than or equal to 0");` |
|      - |  581 | `	}` |
|      - |  582 | `	/* Perform the requested operation (Microseconds) */` |
|     57 |  583 | `	pVfs->xSleep((unsigned int)nSleep);` |
|     57 |  584 | `	return PH7_OK;` |
|     30 |  585 | `}` |
|      - |  586 | `/*` |
|      - |  587 | ` * bool unlink (string $filename)` |
|      - |  588 | ` *  Delete a file.` |
|      - |  589 | ` * Parameters` |
|      - |  590 | ` *  $filename` |
|      - |  591 | ` *   Path to the file.` |
|      - |  592 | ` * Return` |
|      - |  593 | ` *  TRUE on success or FALSE on failure.` |
|      - |  594 | ` */` |
|  34490 |  595 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  596 | `{` |
|      - |  597 | `	const char *zPath;` |
|      - |  598 | `	ph7_vfs *pVfs;` |
|      - |  599 | `	int rc;` |
|  34495 |  600 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  601 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  602 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  603 | `		return PH7_OK;` |
|      - |  604 | `	}` |
|      - |  605 | `	/* Point to the underlying vfs */` |
|  34495 |  606 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  34495 |  607 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  608 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  609 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  610 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  611 | `			ph7_function_name(pCtx)` |
|      - |  612 | `			);` |
|    ! 0 |  613 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  614 | `		return PH7_OK;` |
|      - |  615 | `	}` |
|      - |  616 | `	/* Point to the desired directory */` |
|  34495 |  617 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  618 | `	/* Perform the requested operation */` |
|  34495 |  619 | `	errno = 0;` |
|  34495 |  620 | `	rc = pVfs->xUnlink(zPath);` |
|  34495 |  621 | `	if( rc != PH7_OK ){` |
|  20925 |  622 | `		VfsThrowSysWarning(pCtx,zPath);` |
|  10460 |  623 | `	}` |
|      - |  624 | `	/* IO return value */` |
|  34495 |  625 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  34495 |  626 | `	return PH7_OK;` |
|  17250 |  627 | `}` |
|      - |  628 | `/*` |
|      - |  629 | ` * bool chmod(string $filename,int $mode)` |
|      - |  630 | ` *  Attempts to change the mode of the specified file to that given in mode.` |
|      - |  631 | ` * Parameters` |
|      - |  632 | ` *  $filename` |
|      - |  633 | ` *   Path to the file.` |
|      - |  634 | ` * $mode` |
|      - |  635 | ` *   Mode (Must be an integer)` |
|      - |  636 | ` * Return` |
|      - |  637 | ` *  TRUE on success or FALSE on failure.` |
|      - |  638 | ` */` |
|    154 |  639 | `static int PH7_vfs_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  640 | `{` |
|      - |  641 | `	const char *zPath;` |
|      - |  642 | `	ph7_vfs *pVfs;` |
|      - |  643 | `	int iMode;` |
|      - |  644 | `	int rc;` |
|    156 |  645 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  646 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  647 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  648 | `		return PH7_OK;` |
|      - |  649 | `	}` |
|      - |  650 | `	/* Point to the underlying vfs */` |
|    156 |  651 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    156 |  652 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - |  653 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  654 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  655 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  656 | `			ph7_function_name(pCtx)` |
|      - |  657 | `			);` |
|    ! 0 |  658 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  659 | `		return PH7_OK;` |
|      - |  660 | `	}` |
|      - |  661 | `	/* Point to the desired directory */` |
|    156 |  662 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  663 | `	/* Extract the mode */` |
|    156 |  664 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - |  665 | `	/* Perform the requested operation */` |
|    156 |  666 | `	rc = pVfs->xChmod(zPath,iMode);` |
|      - |  667 | `	/* IO return value */` |
|    156 |  668 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    156 |  669 | `	return PH7_OK;` |
|     79 |  670 | `}` |
|      - |  671 | `/*` |
|      - |  672 | ` * bool chown(string $filename,string $user)` |
|      - |  673 | ` *  Attempts to change the owner of the file filename to user user.` |
|      - |  674 | ` * Parameters` |
|      - |  675 | ` *  $filename` |
|      - |  676 | ` *   Path to the file.` |
|      - |  677 | ` * $user` |
|      - |  678 | ` *   Username.` |
|      - |  679 | ` * Return` |
|      - |  680 | ` *  TRUE on success or FALSE on failure.` |
|      - |  681 | ` */` |
|      6 |  682 | `static int PH7_vfs_chown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  683 | `{` |
|      - |  684 | `	const char *zPath,*zUser;` |
|      - |  685 | `	ph7_vfs *pVfs;` |
|      - |  686 | `	int rc;` |
|      7 |  687 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  688 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  689 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  690 | `		return PH7_OK;` |
|      - |  691 | `	}` |
|      - |  692 | `	/* Point to the underlying vfs */` |
|      7 |  693 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 |  694 | `	if( pVfs == 0 \|\| pVfs->xChown == 0 ){` |
|      - |  695 | `		/* IO routine not implemented,return NULL */` |
|      1 |  696 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  697 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  698 | `			ph7_function_name(pCtx)` |
|      - |  699 | `			);` |
|      1 |  700 | `		ph7_result_bool(pCtx,0);` |
|      1 |  701 | `		return PH7_OK;` |
|      - |  702 | `	}` |
|      - |  703 | `	/* Point to the desired directory */` |
|      6 |  704 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  705 | `	/* Extract the user */` |
|      6 |  706 | `	zUser = ph7_value_to_string(apArg[1],0);` |
|      - |  707 | `	/* Perform the requested operation */` |
|      6 |  708 | `	errno = 0;` |
|      6 |  709 | `	rc = pVfs->xChown(zPath,zUser);` |
|      6 |  710 | `	if( rc != PH7_OK ){` |
|      - |  711 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - |  712 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - |  713 | `		 * "chown(): Operation not permitted". */` |
|      6 |  714 | `		if( rc == -2 ){` |
|      3 |  715 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find uid for %s",` |
|      1 |  716 | `				ph7_function_name(pCtx),zUser);` |
|      1 |  717 | `		}else{` |
|      6 |  718 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 |  719 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - |  720 | `		}` |
|      3 |  721 | `	}` |
|      - |  722 | `	/* IO return value */` |
|      6 |  723 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 |  724 | `	return PH7_OK;` |
|      4 |  725 | `}` |
|      - |  726 | `/*` |
|      - |  727 | ` * bool chgrp(string $filename,string $group)` |
|      - |  728 | ` *  Attempts to change the group of the file filename to group.` |
|      - |  729 | ` * Parameters` |
|      - |  730 | ` *  $filename` |
|      - |  731 | ` *   Path to the file.` |
|      - |  732 | ` * $group` |
|      - |  733 | ` *   groupname.` |
|      - |  734 | ` * Return` |
|      - |  735 | ` *  TRUE on success or FALSE on failure.` |
|      - |  736 | ` */` |
|      6 |  737 | `static int PH7_vfs_chgrp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  738 | `{` |
|      - |  739 | `	const char *zPath,*zGroup;` |
|      - |  740 | `	ph7_vfs *pVfs;` |
|      - |  741 | `	int rc;` |
|      7 |  742 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  743 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  744 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  745 | `		return PH7_OK;` |
|      - |  746 | `	}` |
|      - |  747 | `	/* Point to the underlying vfs */` |
|      7 |  748 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 |  749 | `	if( pVfs == 0 \|\| pVfs->xChgrp == 0 ){` |
|      - |  750 | `		/* IO routine not implemented,return NULL */` |
|      1 |  751 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  752 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  753 | `			ph7_function_name(pCtx)` |
|      - |  754 | `			);` |
|      1 |  755 | `		ph7_result_bool(pCtx,0);` |
|      1 |  756 | `		return PH7_OK;` |
|      - |  757 | `	}` |
|      - |  758 | `	/* Point to the desired directory */` |
|      6 |  759 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  760 | `	/* Extract the user */` |
|      6 |  761 | `	zGroup = ph7_value_to_string(apArg[1],0);` |
|      - |  762 | `	/* Perform the requested operation */` |
|      6 |  763 | `	errno = 0;` |
|      6 |  764 | `	rc = pVfs->xChgrp(zPath,zGroup);` |
|      6 |  765 | `	if( rc != PH7_OK ){` |
|      - |  766 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - |  767 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - |  768 | `		 * "chown(): Operation not permitted". */` |
|      6 |  769 | `		if( rc == -2 ){` |
|      3 |  770 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find gid for %s",` |
|      1 |  771 | `				ph7_function_name(pCtx),zGroup);` |
|      1 |  772 | `		}else{` |
|      6 |  773 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 |  774 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - |  775 | `		}` |
|      3 |  776 | `	}` |
|      - |  777 | `	/* IO return value */` |
|      6 |  778 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 |  779 | `	return PH7_OK;` |
|      4 |  780 | `}` |
|      - |  781 | `/*` |
|      - |  782 | ` * int64 disk_free_space(string $directory)` |
|      - |  783 | ` *  Returns available space on filesystem or disk partition.` |
|      - |  784 | ` * Parameters` |
|      - |  785 | ` *  $directory` |
|      - |  786 | ` *   A directory of the filesystem or disk partition.` |
|      - |  787 | ` * Return` |
|      - |  788 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  789 | ` */` |
|      4 |  790 | `static int PH7_vfs_disk_free_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  791 | `{` |
|      - |  792 | `	const char *zPath;` |
|      - |  793 | `	ph7_int64 iSize;` |
|      - |  794 | `	ph7_vfs *pVfs;` |
|      5 |  795 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  796 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  797 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  798 | `		return PH7_OK;` |
|      - |  799 | `	}` |
|      - |  800 | `	/* Point to the underlying vfs */` |
|      5 |  801 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  802 | `	if( pVfs == 0 \|\| pVfs->xFreeSpace == 0 ){` |
|      - |  803 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  804 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  805 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  806 | `			ph7_function_name(pCtx)` |
|      - |  807 | `			);` |
|    ! 0 |  808 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  809 | `		return PH7_OK;` |
|      - |  810 | `	}` |
|      - |  811 | `	/* Point to the desired directory */` |
|      5 |  812 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  813 | `	/* Perform the requested operation */` |
|      5 |  814 | `	iSize = pVfs->xFreeSpace(zPath);` |
|      - |  815 | `	/* IO return value */` |
|      5 |  816 | `	ph7_result_int64(pCtx,iSize);` |
|      5 |  817 | `	return PH7_OK;` |
|      3 |  818 | `}` |
|      - |  819 | `/*` |
|      - |  820 | ` * int64 disk_total_space(string $directory)` |
|      - |  821 | ` *  Returns the total size of a filesystem or disk partition.` |
|      - |  822 | ` * Parameters` |
|      - |  823 | ` *  $directory` |
|      - |  824 | ` *   A directory of the filesystem or disk partition.` |
|      - |  825 | ` * Return` |
|      - |  826 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  827 | ` */` |
|      4 |  828 | `static int PH7_vfs_disk_total_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  829 | `{` |
|      - |  830 | `	const char *zPath;` |
|      - |  831 | `	ph7_int64 iSize;` |
|      - |  832 | `	ph7_vfs *pVfs;` |
|      5 |  833 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  834 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  835 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  836 | `		return PH7_OK;` |
|      - |  837 | `	}` |
|      - |  838 | `	/* Point to the underlying vfs */` |
|      5 |  839 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  840 | `	if( pVfs == 0 \|\| pVfs->xTotalSpace == 0 ){` |
|      - |  841 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  842 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  843 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  844 | `			ph7_function_name(pCtx)` |
|      - |  845 | `			);` |
|    ! 0 |  846 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  847 | `		return PH7_OK;` |
|      - |  848 | `	}` |
|      - |  849 | `	/* Point to the desired directory */` |
|      5 |  850 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  851 | `	/* Perform the requested operation */` |
|      5 |  852 | `	iSize = pVfs->xTotalSpace(zPath);` |
|      - |  853 | `	/* IO return value */` |
|      5 |  854 | `	ph7_result_int64(pCtx,iSize);` |
|      5 |  855 | `	return PH7_OK;` |
|      3 |  856 | `}` |
|      - |  857 | `/*` |
|      - |  858 | ` * bool file_exists(string $filename)` |
|      - |  859 | ` *  Checks whether a file or directory exists.` |
|      - |  860 | ` * Parameters` |
|      - |  861 | ` *  $filename` |
|      - |  862 | ` *   Path to the file.` |
|      - |  863 | ` * Return` |
|      - |  864 | ` *  TRUE on success or FALSE on failure.` |
|      - |  865 | ` */` |
|    206 |  866 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  867 | `{` |
|      - |  868 | `	const char *zPath;` |
|      - |  869 | `	ph7_vfs *pVfs;` |
|      - |  870 | `	int rc;` |
|    211 |  871 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  872 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  873 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  874 | `		return PH7_OK;` |
|      - |  875 | `	}` |
|      - |  876 | `	/* Point to the underlying vfs */` |
|    211 |  877 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    211 |  878 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - |  879 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  880 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  881 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  882 | `			ph7_function_name(pCtx)` |
|      - |  883 | `			);` |
|    ! 0 |  884 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  885 | `		return PH7_OK;` |
|      - |  886 | `	}` |
|      - |  887 | `	/* Point to the desired directory */` |
|    211 |  888 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  889 | `	/* Perform the requested operation */` |
|    211 |  890 | `	rc = pVfs->xFileExists(zPath);` |
|      - |  891 | `	/* IO return value */` |
|    211 |  892 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    211 |  893 | `	return PH7_OK;` |
|    108 |  894 | `}` |
|      - |  895 | `/*` |
|      - |  896 | ` * int64 file_size(string $filename)` |
|      - |  897 | ` *  Gets the size for the given file.` |
|      - |  898 | ` * Parameters` |
|      - |  899 | ` *  $filename` |
|      - |  900 | ` *   Path to the file.` |
|      - |  901 | ` * Return` |
|      - |  902 | ` *  File size on success or FALSE on failure.` |
|      - |  903 | ` */` |
|     10 |  904 | `static int PH7_vfs_file_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  905 | `{` |
|      - |  906 | `	const char *zPath;` |
|      - |  907 | `	ph7_int64 iSize;` |
|      - |  908 | `	ph7_vfs *pVfs;` |
|     11 |  909 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  910 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  911 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  912 | `		return PH7_OK;` |
|      - |  913 | `	}` |
|      - |  914 | `	/* Point to the underlying vfs */` |
|     11 |  915 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 |  916 | `	if( pVfs == 0 \|\| pVfs->xFileSize == 0 ){` |
|      - |  917 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  918 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  919 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  920 | `			ph7_function_name(pCtx)` |
|      - |  921 | `			);` |
|    ! 0 |  922 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  923 | `		return PH7_OK;` |
|      - |  924 | `	}` |
|      - |  925 | `	/* Point to the desired directory */` |
|     11 |  926 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  927 | `	/* Perform the requested operation */` |
|     11 |  928 | `	iSize = pVfs->xFileSize(zPath);` |
|     11 |  929 | `	if( iSize < 0 ){` |
|      - |  930 | `		/* php: a stat failure warns and returns FALSE -- PH7 returned int(-1), which is` |
|      - |  931 | `		 * truthy and compares equal to nothing a caller would test for. */` |
|    ! 0 |  932 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): stat failed for %s",` |
|    ! 0 |  933 | `			ph7_function_name(pCtx),zPath);` |
|    ! 0 |  934 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  935 | `		return PH7_OK;` |
|      - |  936 | `	}` |
|      - |  937 | `	/* IO return value */` |
|     11 |  938 | `	ph7_result_int64(pCtx,iSize);` |
|     11 |  939 | `	return PH7_OK;` |
|      6 |  940 | `}` |
|      - |  941 | `/*` |
|      - |  942 | ` * int64 fileatime(string $filename)` |
|      - |  943 | ` *  Gets the last access time of the given file.` |
|      - |  944 | ` * Parameters` |
|      - |  945 | ` *  $filename` |
|      - |  946 | ` *   Path to the file.` |
|      - |  947 | ` * Return` |
|      - |  948 | ` *  File atime on success or FALSE on failure.` |
|      - |  949 | ` */` |
|      4 |  950 | `static int PH7_vfs_file_atime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  951 | `{` |
|      - |  952 | `	const char *zPath;` |
|      - |  953 | `	ph7_int64 iTime;` |
|      - |  954 | `	ph7_vfs *pVfs;` |
|      5 |  955 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  956 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  957 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  958 | `		return PH7_OK;` |
|      - |  959 | `	}` |
|      - |  960 | `	/* Point to the underlying vfs */` |
|      5 |  961 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  962 | `	if( pVfs == 0 \|\| pVfs->xFileAtime == 0 ){` |
|      - |  963 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  964 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  965 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  966 | `			ph7_function_name(pCtx)` |
|      - |  967 | `			);` |
|    ! 0 |  968 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  969 | `		return PH7_OK;` |
|      - |  970 | `	}` |
|      - |  971 | `	/* Point to the desired directory */` |
|      5 |  972 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  973 | `	/* Perform the requested operation */` |
|      5 |  974 | `	iTime = pVfs->xFileAtime(zPath);` |
|      - |  975 | `	/* IO return value */` |
|      5 |  976 | `	ph7_result_int64(pCtx,iTime);` |
|      5 |  977 | `	return PH7_OK;` |
|      3 |  978 | `}` |
|      - |  979 | `/*` |
|      - |  980 | ` * int64 filemtime(string $filename)` |
|      - |  981 | ` *  Gets file modification time.` |
|      - |  982 | ` * Parameters` |
|      - |  983 | ` *  $filename` |
|      - |  984 | ` *   Path to the file.` |
|      - |  985 | ` * Return` |
|      - |  986 | ` *  File mtime on success or FALSE on failure.` |
|      - |  987 | ` */` |
|      6 |  988 | `static int PH7_vfs_file_mtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  989 | `{` |
|      - |  990 | `	const char *zPath;` |
|      - |  991 | `	ph7_int64 iTime;` |
|      - |  992 | `	ph7_vfs *pVfs;` |
|      7 |  993 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  994 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  995 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  996 | `		return PH7_OK;` |
|      - |  997 | `	}` |
|      - |  998 | `	/* Point to the underlying vfs */` |
|      7 |  999 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 1000 | `	if( pVfs == 0 \|\| pVfs->xFileMtime == 0 ){` |
|      - | 1001 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1002 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1003 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1004 | `			ph7_function_name(pCtx)` |
|      - | 1005 | `			);` |
|    ! 0 | 1006 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1007 | `		return PH7_OK;` |
|      - | 1008 | `	}` |
|      - | 1009 | `	/* Point to the desired directory */` |
|      7 | 1010 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1011 | `	/* Perform the requested operation */` |
|      7 | 1012 | `	iTime = pVfs->xFileMtime(zPath);` |
|      - | 1013 | `	/* IO return value */` |
|      7 | 1014 | `	ph7_result_int64(pCtx,iTime);` |
|      7 | 1015 | `	return PH7_OK;` |
|      4 | 1016 | `}` |
|      - | 1017 | `/*` |
|      - | 1018 | ` * int64 filectime(string $filename)` |
|      - | 1019 | ` *  Gets inode change time of file.` |
|      - | 1020 | ` * Parameters` |
|      - | 1021 | ` *  $filename` |
|      - | 1022 | ` *   Path to the file.` |
|      - | 1023 | ` * Return` |
|      - | 1024 | ` *  File ctime on success or FALSE on failure.` |
|      - | 1025 | ` */` |
|      2 | 1026 | `static int PH7_vfs_file_ctime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1027 | `{` |
|      - | 1028 | `	const char *zPath;` |
|      - | 1029 | `	ph7_int64 iTime;` |
|      - | 1030 | `	ph7_vfs *pVfs;` |
|      3 | 1031 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1032 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1033 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1034 | `		return PH7_OK;` |
|      - | 1035 | `	}` |
|      - | 1036 | `	/* Point to the underlying vfs */` |
|      3 | 1037 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1038 | `	if( pVfs == 0 \|\| pVfs->xFileCtime == 0 ){` |
|      - | 1039 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1040 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1041 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1042 | `			ph7_function_name(pCtx)` |
|      - | 1043 | `			);` |
|    ! 0 | 1044 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1045 | `		return PH7_OK;` |
|      - | 1046 | `	}` |
|      - | 1047 | `	/* Point to the desired directory */` |
|      3 | 1048 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1049 | `	/* Perform the requested operation */` |
|      3 | 1050 | `	iTime = pVfs->xFileCtime(zPath);` |
|      - | 1051 | `	/* IO return value */` |
|      3 | 1052 | `	ph7_result_int64(pCtx,iTime);` |
|      3 | 1053 | `	return PH7_OK;` |
|      2 | 1054 | `}` |
|      - | 1055 | `/*` |
|      - | 1056 | ` * bool is_file(string $filename)` |
|      - | 1057 | ` *  Tells whether the filename is a regular file.` |
|      - | 1058 | ` * Parameters` |
|      - | 1059 | ` *  $filename` |
|      - | 1060 | ` *   Path to the file.` |
|      - | 1061 | ` * Return` |
|      - | 1062 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1063 | ` */` |
|   6918 | 1064 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1065 | `{` |
|      - | 1066 | `	const char *zPath;` |
|      - | 1067 | `	ph7_vfs *pVfs;` |
|      - | 1068 | `	int rc;` |
|   6923 | 1069 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1070 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1071 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1072 | `		return PH7_OK;` |
|      - | 1073 | `	}` |
|      - | 1074 | `	/* Point to the underlying vfs */` |
|   6923 | 1075 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   6923 | 1076 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - | 1077 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1078 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1079 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1080 | `			ph7_function_name(pCtx)` |
|      - | 1081 | `			);` |
|    ! 0 | 1082 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1083 | `		return PH7_OK;` |
|      - | 1084 | `	}` |
|      - | 1085 | `	/* Point to the desired directory */` |
|   6923 | 1086 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1087 | `	/* Perform the requested operation */` |
|   6923 | 1088 | `	rc = pVfs->xIsfile(zPath);` |
|      - | 1089 | `	/* IO return value */` |
|   6923 | 1090 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   6923 | 1091 | `	return PH7_OK;` |
|   3464 | 1092 | `}` |
|      - | 1093 | `/*` |
|      - | 1094 | ` * bool is_link(string $filename)` |
|      - | 1095 | ` *  Tells whether the filename is a symbolic link.` |
|      - | 1096 | ` * Parameters` |
|      - | 1097 | ` *  $filename` |
|      - | 1098 | ` *   Path to the file.` |
|      - | 1099 | ` * Return` |
|      - | 1100 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1101 | ` */` |
|      4 | 1102 | `static int PH7_vfs_is_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1103 | `{` |
|      - | 1104 | `	const char *zPath;` |
|      - | 1105 | `	ph7_vfs *pVfs;` |
|      - | 1106 | `	int rc;` |
|      4 | 1107 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1108 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1109 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1110 | `		return PH7_OK;` |
|      - | 1111 | `	}` |
|      - | 1112 | `	/* Point to the underlying vfs */` |
|      4 | 1113 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 | 1114 | `	if( pVfs == 0 \|\| pVfs->xIslink == 0 ){` |
|      - | 1115 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1116 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1117 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1118 | `			ph7_function_name(pCtx)` |
|      - | 1119 | `			);` |
|    ! 0 | 1120 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1121 | `		return PH7_OK;` |
|      - | 1122 | `	}` |
|      - | 1123 | `	/* Point to the desired directory */` |
|      4 | 1124 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1125 | `	/* Perform the requested operation */` |
|      4 | 1126 | `	rc = pVfs->xIslink(zPath);` |
|      - | 1127 | `	/* IO return value */` |
|      4 | 1128 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 | 1129 | `	return PH7_OK;` |
|      2 | 1130 | `}` |
|      - | 1131 | `/*` |
|      - | 1132 | ` * bool is_readable(string $filename)` |
|      - | 1133 | ` *  Tells whether a file exists and is readable.` |
|      - | 1134 | ` * Parameters` |
|      - | 1135 | ` *  $filename` |
|      - | 1136 | ` *   Path to the file.` |
|      - | 1137 | ` * Return` |
|      - | 1138 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1139 | ` */` |
|      2 | 1140 | `static int PH7_vfs_is_readable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1141 | `{` |
|      - | 1142 | `	const char *zPath;` |
|      - | 1143 | `	ph7_vfs *pVfs;` |
|      - | 1144 | `	int rc;` |
|      3 | 1145 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1146 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1147 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1148 | `		return PH7_OK;` |
|      - | 1149 | `	}` |
|      - | 1150 | `	/* Point to the underlying vfs */` |
|      3 | 1151 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1152 | `	if( pVfs == 0 \|\| pVfs->xReadable == 0 ){` |
|      - | 1153 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1154 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1155 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1156 | `			ph7_function_name(pCtx)` |
|      - | 1157 | `			);` |
|    ! 0 | 1158 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1159 | `		return PH7_OK;` |
|      - | 1160 | `	}` |
|      - | 1161 | `	/* Point to the desired directory */` |
|      3 | 1162 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1163 | `	/* Perform the requested operation */` |
|      3 | 1164 | `	rc = pVfs->xReadable(zPath);` |
|      - | 1165 | `	/* IO return value */` |
|      3 | 1166 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 1167 | `	return PH7_OK;` |
|      2 | 1168 | `}` |
|      - | 1169 | `/*` |
|      - | 1170 | ` * bool is_writable(string $filename)` |
|      - | 1171 | ` *  Tells whether the filename is writable.` |
|      - | 1172 | ` * Parameters` |
|      - | 1173 | ` *  $filename` |
|      - | 1174 | ` *   Path to the file.` |
|      - | 1175 | ` * Return` |
|      - | 1176 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1177 | ` */` |
|      4 | 1178 | `static int PH7_vfs_is_writable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1179 | `{` |
|      - | 1180 | `	const char *zPath;` |
|      - | 1181 | `	ph7_vfs *pVfs;` |
|      - | 1182 | `	int rc;` |
|      5 | 1183 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1184 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1185 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1186 | `		return PH7_OK;` |
|      - | 1187 | `	}` |
|      - | 1188 | `	/* Point to the underlying vfs */` |
|      5 | 1189 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1190 | `	if( pVfs == 0 \|\| pVfs->xWritable == 0 ){` |
|      - | 1191 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1192 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1193 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1194 | `			ph7_function_name(pCtx)` |
|      - | 1195 | `			);` |
|    ! 0 | 1196 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1197 | `		return PH7_OK;` |
|      - | 1198 | `	}` |
|      - | 1199 | `	/* Point to the desired directory */` |
|      5 | 1200 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1201 | `	/* Perform the requested operation */` |
|      5 | 1202 | `	rc = pVfs->xWritable(zPath);` |
|      - | 1203 | `	/* IO return value */` |
|      5 | 1204 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 1205 | `	return PH7_OK;` |
|      3 | 1206 | `}` |
|      - | 1207 | `/*` |
|      - | 1208 | ` * bool is_executable(string $filename)` |
|      - | 1209 | ` *  Tells whether the filename is executable.` |
|      - | 1210 | ` * Parameters` |
|      - | 1211 | ` *  $filename` |
|      - | 1212 | ` *   Path to the file.` |
|      - | 1213 | ` * Return` |
|      - | 1214 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1215 | ` */` |
|      2 | 1216 | `static int PH7_vfs_is_executable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1217 | `{` |
|      - | 1218 | `	const char *zPath;` |
|      - | 1219 | `	ph7_vfs *pVfs;` |
|      - | 1220 | `	int rc;` |
|      3 | 1221 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1222 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1223 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1224 | `		return PH7_OK;` |
|      - | 1225 | `	}` |
|      - | 1226 | `	/* Point to the underlying vfs */` |
|      3 | 1227 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1228 | `	if( pVfs == 0 \|\| pVfs->xExecutable == 0 ){` |
|      - | 1229 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1230 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1231 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1232 | `			ph7_function_name(pCtx)` |
|      - | 1233 | `			);` |
|    ! 0 | 1234 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1235 | `		return PH7_OK;` |
|      - | 1236 | `	}` |
|      - | 1237 | `	/* Point to the desired directory */` |
|      3 | 1238 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1239 | `	/* Perform the requested operation */` |
|      3 | 1240 | `	rc = pVfs->xExecutable(zPath);` |
|      - | 1241 | `	/* IO return value */` |
|      3 | 1242 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 1243 | `	return PH7_OK;` |
|      2 | 1244 | `}` |
|      - | 1245 | `/*` |
|      - | 1246 | ` * string filetype(string $filename)` |
|      - | 1247 | ` *  Gets file type.` |
|      - | 1248 | ` * Parameters` |
|      - | 1249 | ` *  $filename` |
|      - | 1250 | ` *   Path to the file.` |
|      - | 1251 | ` * Return` |
|      - | 1252 | ` *  The type of the file. Possible values are fifo, char, dir, block, link` |
|      - | 1253 | ` *  file, socket and unknown.` |
|      - | 1254 | ` */` |
|      4 | 1255 | `static int PH7_vfs_filetype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1256 | `{` |
|      - | 1257 | `	const char *zPath;` |
|      - | 1258 | `	ph7_vfs *pVfs;` |
|      5 | 1259 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1260 | `		/* Missing/Invalid argument,return 'unknown' */` |
|    ! 0 | 1261 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 1262 | `		return PH7_OK;` |
|      - | 1263 | `	}` |
|      - | 1264 | `	/* Point to the underlying vfs */` |
|      5 | 1265 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1266 | `	if( pVfs == 0 \|\| pVfs->xFiletype == 0 ){` |
|      - | 1267 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1268 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1269 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1270 | `			ph7_function_name(pCtx)` |
|      - | 1271 | `			);` |
|    ! 0 | 1272 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1273 | `		return PH7_OK;` |
|      - | 1274 | `	}` |
|      - | 1275 | `	/* Point to the desired directory */` |
|      5 | 1276 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1277 | `	/* Set the empty string as the default return value */` |
|      5 | 1278 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1279 | `	/* Perform the requested operation */` |
|      5 | 1280 | `	pVfs->xFiletype(zPath,pCtx);` |
|      5 | 1281 | `	return PH7_OK;` |
|      3 | 1282 | `}` |
|      - | 1283 | `/*` |
|      - | 1284 | ` * array stat(string $filename)` |
|      - | 1285 | ` *  Gives information about a file.` |
|      - | 1286 | ` * Parameters` |
|      - | 1287 | ` *  $filename` |
|      - | 1288 | ` *   Path to the file.` |
|      - | 1289 | ` * Return` |
|      - | 1290 | ` *  An associative array on success holding the following entries on success` |
|      - | 1291 | ` *  0   dev     device number` |
|      - | 1292 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1293 | ` * 2    mode    inode protection mode` |
|      - | 1294 | ` * 3    nlink   number of links` |
|      - | 1295 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1296 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1297 | ` * 6    rdev    device type, if inode device` |
|      - | 1298 | ` * 7    size    size in bytes` |
|      - | 1299 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1300 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1301 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1302 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1303 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1304 | ` * Note:` |
|      - | 1305 | ` *  FALSE is returned on failure.` |
|      - | 1306 | ` */` |
|     10 | 1307 | `static int PH7_vfs_stat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1308 | `{` |
|      - | 1309 | `	ph7_value *pArray,*pValue;` |
|      - | 1310 | `	const char *zPath;` |
|      - | 1311 | `	ph7_vfs *pVfs;` |
|      - | 1312 | `	int rc;` |
|     11 | 1313 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1314 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1315 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1316 | `		return PH7_OK;` |
|      - | 1317 | `	}` |
|      - | 1318 | `	/* Point to the underlying vfs */` |
|     11 | 1319 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 1320 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|      - | 1321 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1322 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1323 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1324 | `			ph7_function_name(pCtx)` |
|      - | 1325 | `			);` |
|    ! 0 | 1326 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1327 | `		return PH7_OK;` |
|      - | 1328 | `	}` |
|      - | 1329 | `	/* Create the array and the working value */` |
|     11 | 1330 | `	pArray = ph7_context_new_array(pCtx);` |
|     11 | 1331 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     11 | 1332 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1333 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1334 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1335 | `		return PH7_OK;` |
|      - | 1336 | `	}` |
|      - | 1337 | `	/* Extract the file path */` |
|     11 | 1338 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1339 | `	/* Perform the requested operation */` |
|     11 | 1340 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|     11 | 1341 | `	if( rc != PH7_OK ){` |
|      - | 1342 | `		/* IO error,return FALSE */` |
|    ! 0 | 1343 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1344 | `	}else{` |
|      - | 1345 | `		/* Return the associative array */` |
|     11 | 1346 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1347 | `	}` |
|      - | 1348 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1349 | `	 * automatically as soon we return from this function. */` |
|     11 | 1350 | `	return PH7_OK;` |
|      6 | 1351 | `}` |
|      - | 1352 | `/*` |
|      - | 1353 | ` * array lstat(string $filename)` |
|      - | 1354 | ` *  Gives information about a file or symbolic link.` |
|      - | 1355 | ` * Parameters` |
|      - | 1356 | ` *  $filename` |
|      - | 1357 | ` *   Path to the file.` |
|      - | 1358 | ` * Return` |
|      - | 1359 | ` *  An associative array on success holding the following entries on success` |
|      - | 1360 | ` *  0   dev     device number` |
|      - | 1361 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1362 | ` * 2    mode    inode protection mode` |
|      - | 1363 | ` * 3    nlink   number of links` |
|      - | 1364 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1365 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1366 | ` * 6    rdev    device type, if inode device` |
|      - | 1367 | ` * 7    size    size in bytes` |
|      - | 1368 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1369 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1370 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1371 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1372 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1373 | ` * Note:` |
|      - | 1374 | ` *  FALSE is returned on failure.` |
|      - | 1375 | ` */` |
|      2 | 1376 | `static int PH7_vfs_lstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1377 | `{` |
|      - | 1378 | `	ph7_value *pArray,*pValue;` |
|      - | 1379 | `	const char *zPath;` |
|      - | 1380 | `	ph7_vfs *pVfs;` |
|      - | 1381 | `	int rc;` |
|      3 | 1382 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1383 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1384 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1385 | `		return PH7_OK;` |
|      - | 1386 | `	}` |
|      - | 1387 | `	/* Point to the underlying vfs */` |
|      3 | 1388 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1389 | `	if( pVfs == 0 \|\| pVfs->xlStat == 0 ){` |
|      - | 1390 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1391 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1392 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1393 | `			ph7_function_name(pCtx)` |
|      - | 1394 | `			);` |
|    ! 0 | 1395 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1396 | `		return PH7_OK;` |
|      - | 1397 | `	}` |
|      - | 1398 | `	/* Create the array and the working value */` |
|      3 | 1399 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 1400 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 1401 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1402 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1403 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1404 | `		return PH7_OK;` |
|      - | 1405 | `	}` |
|      - | 1406 | `	/* Extract the file path */` |
|      3 | 1407 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1408 | `	/* Perform the requested operation */` |
|      3 | 1409 | `	rc = pVfs->xlStat(zPath,pArray,pValue);` |
|      3 | 1410 | `	if( rc != PH7_OK ){` |
|      - | 1411 | `		/* IO error,return FALSE */` |
|    ! 0 | 1412 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1413 | `	}else{` |
|      - | 1414 | `		/* Return the associative array */` |
|      3 | 1415 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1416 | `	}` |
|      - | 1417 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1418 | `	 * automatically as soon we return from this function. */` |
|      3 | 1419 | `	return PH7_OK;` |
|      2 | 1420 | `}` |
|      - | 1421 | `/*` |
|      - | 1422 | ` * string getenv(string $varname)` |
|      - | 1423 | ` *  Gets the value of an environment variable.` |
|      - | 1424 | ` * Parameters` |
|      - | 1425 | ` *  $varname` |
|      - | 1426 | ` *   The variable name.` |
|      - | 1427 | ` * Return` |
|      - | 1428 | ` *  Returns the value of the environment variable varname, or FALSE if the environment` |
|      - | 1429 | ` * variable varname does not exist.` |
|      - | 1430 | ` */` |
|     56 | 1431 | `static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1432 | `{` |
|      - | 1433 | `	const char *zEnv;` |
|      - | 1434 | `	ph7_vfs *pVfs;` |
|      - | 1435 | `	int iLen;` |
|     60 | 1436 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1437 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1438 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1439 | `		return PH7_OK;` |
|      - | 1440 | `	}` |
|      - | 1441 | `	/* Point to the underlying vfs */` |
|     60 | 1442 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     60 | 1443 | `	if( pVfs == 0 \|\| pVfs->xGetenv == 0 ){` |
|      - | 1444 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1445 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1446 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1447 | `			ph7_function_name(pCtx)` |
|      - | 1448 | `			);` |
|    ! 0 | 1449 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1450 | `		return PH7_OK;` |
|      - | 1451 | `	}` |
|      - | 1452 | `	/* Extract the environment variable */` |
|     60 | 1453 | `	zEnv = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1454 | `	/* Set a boolean FALSE as the default return value */` |
|     60 | 1455 | `	ph7_result_bool(pCtx,0);` |
|     60 | 1456 | `	if( iLen < 1 ){` |
|      - | 1457 | `		/* Empty string */` |
|    ! 0 | 1458 | `		return PH7_OK;` |
|      - | 1459 | `	}` |
|      - | 1460 | `	/* Perform the requested operation */` |
|     60 | 1461 | `	pVfs->xGetenv(zEnv,pCtx);` |
|     60 | 1462 | `	return PH7_OK;` |
|     32 | 1463 | `}` |
|      - | 1464 | `/*` |
|      - | 1465 | ` * bool putenv(string $settings)` |
|      - | 1466 | ` *  Set the value of an environment variable.` |
|      - | 1467 | ` * Parameters` |
|      - | 1468 | ` *  $setting` |
|      - | 1469 | ` *   The setting, like "FOO=BAR"` |
|      - | 1470 | ` * Return` |
|      - | 1471 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1472 | ` */` |
|      6 | 1473 | `static int PH7_vfs_putenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1474 | `{` |
|      - | 1475 | `	const char *zName,*zValue;` |
|      - | 1476 | `	char *zSettings,*zEnd;` |
|      - | 1477 | `	ph7_vfs *pVfs;` |
|      - | 1478 | `	int iLen,rc;` |
|      7 | 1479 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1480 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1481 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1482 | `		return PH7_OK;` |
|      - | 1483 | `	}` |
|      - | 1484 | `	/* Extract the setting variable */` |
|      7 | 1485 | `	zSettings = (char *)ph7_value_to_string(apArg[0],&iLen);` |
|      7 | 1486 | `	if( iLen < 1 ){` |
|      - | 1487 | `		/* Empty string,return FALSE */` |
|    ! 0 | 1488 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1489 | `		return PH7_OK;` |
|      - | 1490 | `	}` |
|      - | 1491 | `	/* Parse the setting */` |
|      7 | 1492 | `	zEnd = &zSettings[iLen];` |
|      7 | 1493 | `	zValue = 0;` |
|      7 | 1494 | `	zName = zSettings;` |
|    127 | 1495 | `	while( zSettings < zEnd ){` |
|    127 | 1496 | `		if( zSettings[0] == '=' ){` |
|      - | 1497 | `			/* Null terminate the name */` |
|      7 | 1498 | `			zSettings[0] = 0;` |
|      7 | 1499 | `			zValue = &zSettings[1];` |
|      7 | 1500 | `			break;` |
|      - | 1501 | `		}` |
|    121 | 1502 | `		zSettings++;` |
|      1 | 1503 | `	}` |
|      - | 1504 | `	/* Install the environment variable in the $_Env array */` |
|      7 | 1505 | `	if( zValue == 0 \|\| zName[0] == 0 \|\| zValue >= zEnd \|\| zName >= zValue ){` |
|      - | 1506 | `		/* Invalid settings,retun FALSE */` |
|      5 | 1507 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1508 | `		if( zSettings  < zEnd ){` |
|      5 | 1509 | `			zSettings[0] = '=';` |
|      2 | 1510 | `		}` |
|      5 | 1511 | `		return PH7_OK;` |
|      - | 1512 | `	}` |
|      3 | 1513 | `	ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_ENV_ATTR,zName,zValue,(int)(zEnd-zValue));` |
|      - | 1514 | `	/* Point to the underlying vfs */` |
|      3 | 1515 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1516 | `	if( pVfs == 0 \|\| pVfs->xSetenv == 0 ){` |
|      - | 1517 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1518 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1519 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1520 | `			ph7_function_name(pCtx)` |
|      - | 1521 | `			);` |
|    ! 0 | 1522 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1523 | `		zSettings[0] = '=';` |
|    ! 0 | 1524 | `		return PH7_OK;` |
|      - | 1525 | `	}` |
|      - | 1526 | `	/* Perform the requested operation */` |
|      3 | 1527 | `	rc = pVfs->xSetenv(zName,zValue);` |
|      3 | 1528 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 1529 | `	zSettings[0] = '=';` |
|      3 | 1530 | `	return PH7_OK;` |
|      4 | 1531 | `}` |
|      - | 1532 | `/*` |
|      - | 1533 | ` * bool touch(string $filename[,int64 $time = time()[,int64 $atime]])` |
|      - | 1534 | ` *  Sets access and modification time of file.` |
|      - | 1535 | ` * Note: On windows` |
|      - | 1536 | ` *   If the file does not exists,it will not be created.` |
|      - | 1537 | ` * Parameters` |
|      - | 1538 | ` *  $filename` |
|      - | 1539 | ` *   The name of the file being touched.` |
|      - | 1540 | ` *  $time` |
|      - | 1541 | ` *   The touch time. If time is not supplied, the current system time is used.` |
|      - | 1542 | ` * $atime` |
|      - | 1543 | ` *   If present, the access time of the given filename is set to the value of atime.` |
|      - | 1544 | ` *   Otherwise, it is set to the value passed to the time parameter. If neither are` |
|      - | 1545 | ` *   present, the current system time is used.` |
|      - | 1546 | ` * Return` |
|      - | 1547 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1548 | `*/` |
|     12 | 1549 | `static int PH7_vfs_touch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1550 | `{` |
|      - | 1551 | `	ph7_int64 nTime,nAccess;` |
|      - | 1552 | `	const char *zFile;` |
|      - | 1553 | `	ph7_vfs *pVfs;` |
|      - | 1554 | `	int rc;` |
|     13 | 1555 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1556 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1557 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1558 | `		return PH7_OK;` |
|      - | 1559 | `	}` |
|      - | 1560 | `	/* Point to the underlying vfs */` |
|     13 | 1561 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     13 | 1562 | `	if( pVfs == 0 \|\| pVfs->xTouch == 0 ){` |
|      - | 1563 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1564 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1565 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1566 | `			ph7_function_name(pCtx)` |
|      - | 1567 | `			);` |
|    ! 0 | 1568 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1569 | `		return PH7_OK;` |
|      - | 1570 | `	}` |
|      - | 1571 | `	/* Resolve php's defaults HERE, so the driver only ever sees real timestamps: a` |
|      - | 1572 | ``	 * NEGATIVE stamp is perfectly legal to php (`touch($f, -100)` is 1969), so it`` |
|      - | 1573 | `	 * cannot double as the "not given" sentinel the drivers used to read it as. An` |
|      - | 1574 | `	 * omitted/null $mtime is NOW; an omitted/null $atime follows $mtime. $atime also` |
|      - | 1575 | `	 * used to be read from apArg[1] — the mtime — so touch($f, $m, $a) silently` |
|      - | 1576 | `	 * stamped the modification time onto both. */` |
|     13 | 1577 | `	zFile = ph7_value_to_string(apArg[0],0);` |
|     13 | 1578 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) && nArg > 1 && ph7_value_is_null(apArg[1]) ){` |
|    ! 0 | 1579 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1580 | `			"touch(): Argument #2 ($mtime) cannot be null when argument #3 ($atime) "` |
|      - | 1581 | `			"is an integer");` |
|      - | 1582 | `	}` |
|     13 | 1583 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      3 | 1584 | `		nTime = ph7_value_to_int64(apArg[1]);` |
|      2 | 1585 | `	}else{` |
|      - | 1586 | `		time_t tNow;` |
|     11 | 1587 | `		time(&tNow);` |
|     11 | 1588 | `		nTime = (ph7_int64)tNow;` |
|      - | 1589 | `	}` |
|     13 | 1590 | `	nAccess = nTime;` |
|     13 | 1591 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      3 | 1592 | `		nAccess = ph7_value_to_int64(apArg[2]);` |
|      1 | 1593 | `	}` |
|     13 | 1594 | `	rc = pVfs->xTouch(zFile,nTime,nAccess);` |
|      - | 1595 | `	/* IO result */` |
|     13 | 1596 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     13 | 1597 | `	return PH7_OK;` |
|      7 | 1598 | `}` |
|      - | 1599 | `/*` |
|      - | 1600 | ` * Path processing functions that do not need access to the VFS layer` |
|      - | 1601 | ` * Status:` |
|      - | 1602 | ` *    Stable.` |
|      - | 1603 | ` */` |
|      - | 1604 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1605 | `/*` |
|      - | 1606 | ` * string dirname(string $path)` |
|      - | 1607 |  |
|      - | 1608 | ` *  Returns parent directory's path.` |
|      - | 1609 | ` * Parameters` |
|      - | 1610 | ` * $path` |
|      - | 1611 | ` *  Target path.` |
|      - | 1612 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1613 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1614 | ` * Return` |
|      - | 1615 | ` *  The path of the parent directory. If there are no slashes in path, a dot ('.')` |
|      - | 1616 | ` *  is returned, indicating the current directory.` |
|      - | 1617 | ` */` |
|     92 | 1618 | `static int PH7_builtin_dirname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1619 | `{` |
|      - | 1620 | `	const char *zPath,*zDir;` |
|      - | 1621 | `	int iLen,iDirlen;` |
|     97 | 1622 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1623 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 1624 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1625 | `		return PH7_OK;` |
|      - | 1626 | `	}` |
|      - | 1627 | `	/* Point to the target path */` |
|     97 | 1628 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     97 | 1629 | `	if( iLen < 1 ){` |
|      - | 1630 | `		/* php answers "" for the empty path, not "." */` |
|      3 | 1631 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1632 | `		return PH7_OK;` |
|      - | 1633 | `	}` |
|      - | 1634 | `	/* $levels (php 7.0) was ACCEPTED AND IGNORED, so dirname($p, 3) silently answered` |
|      - | 1635 | `	 * the one-level parent — the caller's own answer, one or more levels too deep. Each` |
|      - | 1636 | `	 * level re-runs php_dirname on the previous result and stops as soon as the answer` |
|      - | 1637 | `	 * stops moving (the filesystem root, or "." for a relative path), which is what php` |
|      - | 1638 | `	 * does; php also rejects a level below 1 outright. */` |
|     95 | 1639 | `	zDir = zPath;` |
|     95 | 1640 | `	iDirlen = iLen;` |
|     95 | 1641 | `	if( nArg > 1 ){` |
|     51 | 1642 | `		ph7_int64 nLevels = ph7_value_to_int64(apArg[1]);` |
|      - | 1643 | `		ph7_int64 i;` |
|     51 | 1644 | `		if( nLevels < 1 ){` |
|      7 | 1645 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1646 | `				"dirname(): Argument #2 ($levels) must be greater than or equal to 1");` |
|      - | 1647 | `		}` |
|    125 | 1648 | `		for( i = 0 ; i < nLevels ; ++i ){` |
|    105 | 1649 | `			int iPrevLen = iDirlen;` |
|    105 | 1650 | `			const char *zPrev = zDir;` |
|    105 | 1651 | `			zDir = PH7_ExtractDirName(zPrev,iPrevLen,&iDirlen);` |
|    105 | 1652 | `			if( iDirlen == iPrevLen && SyMemcmp(zDir,zPrev,(sxu32)iDirlen) == 0 ){` |
|     25 | 1653 | `				break; /* fixed point: "/" and "." are their own parents */` |
|      - | 1654 | `			}` |
|     41 | 1655 | `		}` |
|     23 | 1656 | `	}else{` |
|     45 | 1657 | `		zDir = PH7_ExtractDirName(zPath,iLen,&iDirlen);` |
|      - | 1658 | `	}` |
|      - | 1659 | `	/* Return directory name */` |
|     89 | 1660 | `	ph7_result_string(pCtx,zDir,iDirlen);` |
|     89 | 1661 | `	return PH7_OK;` |
|     51 | 1662 | `}` |
|      - | 1663 | `/*` |
|      - | 1664 | ` * string basename(string $path[, string $suffix ])` |
|      - | 1665 | ` *  Returns trailing name component of path.` |
|      - | 1666 | ` * Parameters` |
|      - | 1667 | ` * $path` |
|      - | 1668 | ` *  Target path.` |
|      - | 1669 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1670 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1671 | ` * $suffix` |
|      - | 1672 | ` *  If the name component ends in suffix this will also be cut off.` |
|      - | 1673 | ` * Return` |
|      - | 1674 | ` *  The base name of the given path.` |
|      - | 1675 | ` */` |
|     46 | 1676 | `static int PH7_builtin_basename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1677 | `{` |
|      - | 1678 | `	const char *zPath,*zBase;` |
|      - | 1679 | `	int iLen,nBase;` |
|     47 | 1680 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1681 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1682 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1683 | `		return PH7_OK;` |
|      - | 1684 | `	}` |
|      - | 1685 | `	/* Point to the target path */` |
|     47 | 1686 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1687 | `	/* php_basename, shared with pathinfo(): the hand-rolled walk this used to carry` |
|      - | 1688 | `	 * kept a leading separator on a single-component path (basename("/a") answered` |
|      - | 1689 | `	 * "/a", basename("/.") answered "/.") because it stopped one byte short. */` |
|     47 | 1690 | `	zBase = PH7_ExtractBaseName(zPath,iLen,&nBase);` |
|     47 | 1691 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 1692 | `		const char *zSuffix;` |
|      - | 1693 | `		int nSuffix;` |
|      - | 1694 | `		/* Strip suffix — php leaves the basename alone when it IS the suffix */` |
|      5 | 1695 | `		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);` |
|      4 | 1696 | `		if( nSuffix > 0 && nSuffix < nBase` |
|      5 | 1697 | `		 && SyMemcmp(&zBase[nBase - nSuffix],zSuffix,nSuffix) == 0 ){` |
|      5 | 1698 | `			nBase -= nSuffix;` |
|      2 | 1699 | `		}` |
|      2 | 1700 | `	}` |
|      - | 1701 | `	/* Store the basename */` |
|     47 | 1702 | `	ph7_result_string(pCtx,zBase,nBase);` |
|     47 | 1703 | `	return PH7_OK;` |
|     24 | 1704 | `}` |
|      - | 1705 | `/*` |
|      - | 1706 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1707 | ` *  Returns information about a file path.` |
|      - | 1708 | ` * Parameter` |
|      - | 1709 | ` *  $path` |
|      - | 1710 | ` *   The path to be parsed.` |
|      - | 1711 | ` *  $options` |
|      - | 1712 | ` *    If present, specifies a specific element to be returned; one of` |
|      - | 1713 | ` *      PATHINFO_DIRNAME, PATHINFO_BASENAME, PATHINFO_EXTENSION or PATHINFO_FILENAME.` |
|      - | 1714 | ` * Return` |
|      - | 1715 | ` *  If the options parameter is not passed, an associative array containing the following` |
|      - | 1716 | ` *  elements is returned: dirname, basename, extension (if any), and filename.` |
|      - | 1717 | ` *  If options is present, returns a string containing the requested element.` |
|      - | 1718 | ` */` |
|      - | 1719 | `typedef struct path_info path_info;` |
|      - | 1720 | `struct path_info` |
|      - | 1721 | `{` |
|      - | 1722 | `	SyString sDir; /* Directory [i.e: /var/www] */` |
|      - | 1723 | `	SyString sBasename; /* Basename [i.e httpd.conf] */` |
|      - | 1724 | `	SyString sExtension; /* File extension [i.e xml,pdf..] */` |
|      - | 1725 | `	SyString sFilename;  /* Filename */` |
|      - | 1726 | `	int iPresent;        /* Which components php would EMIT (PH7_PATHINFO_* bits) */` |
|      - | 1727 | `};` |
|      - | 1728 | `/*` |
|      - | 1729 | ` * Extract path fields exactly as php's pathinfo() assembles them.` |
|      - | 1730 | ` *` |
|      - | 1731 | ` * Two things this has to get right beyond the values themselves:` |
|      - | 1732 | ` *` |
|      - | 1733 | ` *  - php looks for the LAST dot ANYWHERE in the basename, a leading one included,` |
|      - | 1734 | `` *    so `.bashrc` has extension "bashrc" and filename "" (PH7 stopped the scan`` |
|      - | 1735 | ` *    before the first byte, so it reported no extension and filename ".bashrc").` |
|      - | 1736 | ` *  - EMPTY is not the same as ABSENT. php always emits basename and filename when` |
|      - | 1737 | `` *    they are asked for, emits extension whenever a dot exists (even for `x.`,`` |
|      - | 1738 | ` *    whose extension is ""), and emits dirname only when it is non-empty. The` |
|      - | 1739 | ` *    scalar form answers with the first EMITTED component, so conflating the two` |
|      - | 1740 | `` *    makes `pathinfo("x.", PATHINFO_EXTENSION\|PATHINFO_FILENAME)` fall through to`` |
|      - | 1741 | ` *    the filename ("x") where php answers "" — iPresent keeps them apart.` |
|      - | 1742 | ` *` |
|      - | 1743 | ` * dirname and basename come from the shared php_dirname/php_basename helpers` |
|      - | 1744 | ` * rather than a third hand-rolled walk, so the trailing-separator and` |
|      - | 1745 | ` * relative-path rules ("file.txt" -> ".", "/var/www/" -> "/var" + "www") cannot` |
|      - | 1746 | ` * drift between the two builtins and this one.` |
|      - | 1747 | ` */` |
|  13768 | 1748 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 1749 | `{` |
|      - | 1750 | `	const char *zBase,*zDir,*zDot;` |
|      - | 1751 | `	int nBase,nDir,i;` |
|      - | 1752 | `	/* Zero the structure */` |
|  13773 | 1753 | `	SyZero(pOut,sizeof(path_info));` |
|  13773 | 1754 | `	zDir = PH7_ExtractDirName(zPath,nByte,&nDir);` |
|  13773 | 1755 | `	if( nDir > 0 ){` |
|  13769 | 1756 | `		SyStringInitFromBuf(&pOut->sDir,zDir,nDir);` |
|  13769 | 1757 | `		pOut->iPresent \|= PH7_PATHINFO_DIRNAME;` |
|   6882 | 1758 | `	}` |
|  13773 | 1759 | `	zBase = PH7_ExtractBaseName(zPath,nByte,&nBase);` |
|  13773 | 1760 | `	SyStringInitFromBuf(&pOut->sBasename,zBase,nBase);` |
|  13773 | 1761 | `	pOut->iPresent \|= PH7_PATHINFO_BASENAME\|PH7_PATHINFO_FILENAME;` |
|      - | 1762 | `	/* Last dot anywhere in the basename splits filename from extension */` |
|  13773 | 1763 | `	zDot = 0;` |
|  68787 | 1764 | `	for( i = nBase ; i > 0 ; --i ){` |
|  68767 | 1765 | `		if( zBase[i - 1] == '.' ){` |
|  13753 | 1766 | `			zDot = &zBase[i - 1];` |
|  13753 | 1767 | `			break;` |
|      - | 1768 | `		}` |
|  27512 | 1769 | `	}` |
|  13773 | 1770 | `	if( zDot ){` |
|  13753 | 1771 | `		SyStringInitFromBuf(&pOut->sExtension,zDot + 1,(int)(&zBase[nBase] - (zDot + 1)));` |
|  13753 | 1772 | `		pOut->iPresent \|= PH7_PATHINFO_EXTENSION;` |
|  13753 | 1773 | `		SyStringInitFromBuf(&pOut->sFilename,zBase,(int)(zDot - zBase));` |
|   6879 | 1774 | `	}else{` |
|     21 | 1775 | `		SyStringInitFromBuf(&pOut->sFilename,zBase,nBase);` |
|      - | 1776 | `	}` |
|  13773 | 1777 | `	return SXRET_OK;` |
|      5 | 1778 | `}` |
|      - | 1779 | `/*` |
|      - | 1780 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1781 | ` *  See block comment above.` |
|      - | 1782 | ` */` |
|  13768 | 1783 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1784 | `{` |
|      - | 1785 | `	const char *zPath;` |
|      - | 1786 | `	path_info sInfo;` |
|      - | 1787 | `	int iLen;` |
|  13773 | 1788 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1789 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1790 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1791 | `		return PH7_OK;` |
|      - | 1792 | `	}` |
|      - | 1793 | `	/* Point to the target path. The EMPTY path is not a special case: php still` |
|      - | 1794 | ``	 * answers with the array `["basename" => "", "filename" => ""]` (and "" for a`` |
|      - | 1795 | `	 * scalar request), where PH7 short-circuited to "" and returned the wrong TYPE. */` |
|  13773 | 1796 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1797 | `	/* Extract path info */` |
|  13773 | 1798 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|      - | 1799 | ``	/* Read the mask at 64-bit width: ph7_value_to_int() truncates to `int`, so a`` |
|      - | 1800 | `	 * flags value congruent to PATHINFO_ALL mod 2^32 (4294967311, -4294967281 …)` |
|      - | 1801 | `	 * would take the ARRAY branch and answer with the wrong TYPE. */` |
|  13768 | 1802 | `	if( nArg > 1 && ph7_value_is_int(apArg[1])` |
|  20637 | 1803 | `	 && ph7_value_to_int64(apArg[1]) != (ph7_int64)PH7_PATHINFO_ALL ){` |
|      - | 1804 | `		/* $flags is a BITMASK, not an enum: php assembles the requested components in` |
|      - | 1805 | `		 * the fixed order below and, for anything other than PATHINFO_ALL, hands back` |
|      - | 1806 | `		 * the FIRST one it EMITTED (zend_hash_get_current_data on the fresh array).` |
|      - | 1807 | ``		 * So `PATHINFO_DIRNAME\|PATHINFO_BASENAME` answers the dirname, and an unknown`` |
|      - | 1808 | `		 * bit that happens to carry a known one along (99 = 1\|2\|32\|64) answers as if` |
|      - | 1809 | `		 * only the known ones were passed. PH7 numbered the components 1/2/3/4 and` |
|      - | 1810 | `		 * switched on the whole value, so it read a two-flag mask as a different single` |
|      - | 1811 | `		 * component and answered "" for everything else. Emission is iPresent, NOT` |
|      - | 1812 | `		 * "non-empty": an emitted-but-empty component ends the search with "". */` |
|  13757 | 1813 | `		ph7_int64 nComp = ph7_value_to_int64(apArg[1]);` |
|      - | 1814 | `		static const int aBit[4] = {` |
|      - | 1815 | `			PH7_PATHINFO_DIRNAME,PH7_PATHINFO_BASENAME,` |
|      - | 1816 | `			PH7_PATHINFO_EXTENSION,PH7_PATHINFO_FILENAME` |
|      - | 1817 | `		};` |
|      - | 1818 | `		SyString *apComp[4];` |
|      - | 1819 | `		int i;` |
|  13757 | 1820 | `		apComp[0] = &sInfo.sDir;` |
|  13757 | 1821 | `		apComp[1] = &sInfo.sBasename;` |
|  13757 | 1822 | `		apComp[2] = &sInfo.sExtension;` |
|  13757 | 1823 | `		apComp[3] = &sInfo.sFilename;` |
|      - | 1824 | `		/* Expand the empty string unless a requested component is emitted */` |
|  13757 | 1825 | `		ph7_result_string(pCtx,"",0);` |
|  48085 | 1826 | `		for( i = 0 ; i < 4 ; ++i ){` |
|  48077 | 1827 | `			if( (nComp & aBit[i]) == aBit[i] && (sInfo.iPresent & aBit[i]) ){` |
|  13749 | 1828 | `				ph7_result_string(pCtx,apComp[i]->zString,(int)apComp[i]->nByte);` |
|  13749 | 1829 | `				break;` |
|      - | 1830 | `			}` |
|  17169 | 1831 | `		}` |
|   6881 | 1832 | `	}else{` |
|      - | 1833 | `		/* Return an associative array */` |
|      - | 1834 | `		ph7_value *pArray,*pValue;` |
|     17 | 1835 | `		pArray = ph7_context_new_array(pCtx);` |
|     17 | 1836 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     17 | 1837 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1838 | `			/* Out of mem,return NULL */` |
|    ! 0 | 1839 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1840 | `			return PH7_OK;` |
|      - | 1841 | `		}` |
|      - | 1842 | ``		/* Emitted-but-EMPTY components are in the array too (php keys `basename` and`` |
|      - | 1843 | ``		 * `filename` for "/" with ""), so this walks iPresent, not the lengths. */`` |
|      - | 1844 | `		{` |
|      - | 1845 | `		static const char *azKey[4] = {"dirname","basename","extension","filename"};` |
|      - | 1846 | `		static const int aBit[4] = {` |
|      - | 1847 | `			PH7_PATHINFO_DIRNAME,PH7_PATHINFO_BASENAME,` |
|      - | 1848 | `			PH7_PATHINFO_EXTENSION,PH7_PATHINFO_FILENAME` |
|      - | 1849 | `		};` |
|      - | 1850 | `		SyString *apComp[4];` |
|      - | 1851 | `		int i;` |
|     17 | 1852 | `		apComp[0] = &sInfo.sDir;` |
|     17 | 1853 | `		apComp[1] = &sInfo.sBasename;` |
|     17 | 1854 | `		apComp[2] = &sInfo.sExtension;` |
|     17 | 1855 | `		apComp[3] = &sInfo.sFilename;` |
|     81 | 1856 | `		for( i = 0 ; i < 4 ; ++i ){` |
|     65 | 1857 | `			if( (sInfo.iPresent & aBit[i]) == 0 ){` |
|      9 | 1858 | `				continue;` |
|      - | 1859 | `			}` |
|     57 | 1860 | `			ph7_value_reset_string_cursor(pValue);` |
|     57 | 1861 | `			ph7_value_string(pValue,apComp[i]->zString,(int)apComp[i]->nByte);` |
|     57 | 1862 | `			ph7_array_add_strkey_elem(pArray,azKey[i],pValue); /* Will make it's own copy */` |
|     29 | 1863 | `		}` |
|      - | 1864 | `		}` |
|      - | 1865 | `		/* Return the created array */` |
|     17 | 1866 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1867 | `		/* Don't worry about freeing memory, everything will be released` |
|      - | 1868 | `		 * automatically as soon we return from this foreign function.` |
|      - | 1869 | `		 */` |
|      - | 1870 | `	}` |
|  13773 | 1871 | `	return PH7_OK;` |
|   6889 | 1872 | `}` |
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
|     56 | 1945 | `static int patternCompare(` |
|      - | 1946 | `  const u8 *zPattern,              /* The glob pattern */` |
|      - | 1947 | `  const u8 *zString,               /* The string to compare against the glob */` |
|      - | 1948 | `  const int esc,                    /* The escape character */` |
|      - | 1949 | `  int noCase` |
|      1 | 1950 | `){` |
|      - | 1951 | `  int c, c2;` |
|      - | 1952 | `  int invert;` |
|      - | 1953 | `  int seen;` |
|     57 | 1954 | `  u8 matchOne = '?';` |
|     57 | 1955 | `  u8 matchAll = '*';` |
|     57 | 1956 | `  u8 matchSet = '[';` |
|     57 | 1957 | `  int prevEscape = 0;     /* True if the previous character was 'escape' */` |
|      - | 1958 |  |
|     57 | 1959 | `  if( !zPattern \|\| !zString ) return 0;` |
|     93 | 1960 | `  while( (c = PH7_Utf8Read(zPattern,0,&zPattern))!=0 ){` |
|     85 | 1961 | `    if( !prevEscape && c==matchAll ){` |
|     68 | 1962 | `      while( (c=PH7_Utf8Read(zPattern,0,&zPattern)) == matchAll` |
|     35 | 1963 | `               \|\| c == matchOne ){` |
|    ! 0 | 1964 | `        if( c==matchOne && PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1965 | `          return 0;` |
|      - | 1966 | `        }` |
|    ! 0 | 1967 | `      }` |
|     35 | 1968 | `      if( c==0 ){` |
|     27 | 1969 | `        return 1;` |
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
|     51 | 1999 | `    }else if( !prevEscape && c==matchOne ){` |
|    ! 0 | 2000 | `      if( PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 2001 | `        return 0;` |
|    ! 0 | 2002 | `      }` |
|     51 | 2003 | `    }else if( c==matchSet ){` |
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
|     51 | 2035 | `    }else if( esc==c && !prevEscape ){` |
|    ! 0 | 2036 | `      prevEscape = 1;` |
|    ! 0 | 2037 | `    }else{` |
|     51 | 2038 | `      c2 = PH7_Utf8Read(zString, 0, &zString);` |
|     51 | 2039 | `      if( noCase ){` |
|      7 | 2040 | `        GlogUpperToLower(c);` |
|      7 | 2041 | `        GlogUpperToLower(c2);` |
|      3 | 2042 | `      }` |
|     51 | 2043 | `      if( c!=c2 ){` |
|     15 | 2044 | `        return 0;` |
|      - | 2045 | `      }` |
|     37 | 2046 | `      prevEscape = 0;` |
|      - | 2047 | `    }` |
|      1 | 2048 | `  }` |
|      9 | 2049 | `  return *zString==0;` |
|     29 | 2050 | `}` |
|      - | 2051 | `/* SPDX-SnippetEnd */` |
|      - | 2052 | `/*` |
|      - | 2053 | ` * Wrapper around patternCompare() defined above.` |
|      - | 2054 | ` * See block comment above for more information.` |
|      - | 2055 | ` */` |
|     48 | 2056 | `static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,int CaseCompare)` |
|      1 | 2057 | `{` |
|      - | 2058 | `	int rc;` |
|     49 | 2059 | `	if( iEsc < 0 ){` |
|    ! 0 | 2060 | `		iEsc = '\\';` |
|    ! 0 | 2061 | `	}` |
|     49 | 2062 | `	rc = patternCompare(zPattern,zString,iEsc,CaseCompare);` |
|     49 | 2063 | `	return rc;` |
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
|     40 | 2124 | `static int PH7_builtin_strglob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2125 | `{` |
|      - | 2126 | `	const char *zString,*zPattern;` |
|     41 | 2127 | `	int iEsc = '\\';` |
|      - | 2128 | `	int rc;` |
|     41 | 2129 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2130 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2131 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2132 | `		return PH7_OK;` |
|      - | 2133 | `	}` |
|      - | 2134 | `	/* Extract the pattern and the string */` |
|     41 | 2135 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|     41 | 2136 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2137 | `	/* Go globbing */` |
|     41 | 2138 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0);` |
|      - | 2139 | `	/* Globbing result */` |
|     41 | 2140 | `	ph7_result_bool(pCtx,rc);` |
|     41 | 2141 | `	return PH7_OK;` |
|     21 | 2142 | `}` |
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
|    236 | 2270 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2271 | `{` |
|      - | 2272 | `	ph7_vfs *pVfs;` |
|      - | 2273 | `	/* Set the empty string as the default return value */` |
|    240 | 2274 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2275 | `	/* Point to the underlying vfs */` |
|    240 | 2276 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    240 | 2277 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
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
|    240 | 2288 | `	pVfs->xTempDir(pCtx);` |
|    240 | 2289 | `	return PH7_OK;` |
|    122 | 2290 | `}` |
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
|     92 | 2328 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2329 | `{` |
|      - | 2330 | `	ph7_int64 nProcessId;` |
|      - | 2331 | `	ph7_vfs *pVfs;` |
|      - | 2332 | `	/* Point to the underlying vfs */` |
|     95 | 2333 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     95 | 2334 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
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
|     95 | 2346 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 2347 | `	/* Set the result */` |
|     95 | 2348 | `	ph7_result_int64(pCtx,nProcessId);` |
|     95 | 2349 | `	return PH7_OK;` |
|     49 | 2350 | `}` |
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
|   4142 | 2611 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 2612 | `{` |
|      - | 2613 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 2614 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 2615 | `	return &null_vfs;` |
|      - | 2616 | `#else` |
|      - | 2617 | `#ifdef __WINNT__` |
|      5 | 2618 | `	return &sWinVfs;` |
|      - | 2619 | `#elif defined(__UNIXES__)` |
|   4142 | 2620 | `	return &sUnixVfs;` |
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
|   3646 | 2636 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
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
|   3651 | 2754 | `	const ph7_io_stream *pFileStream = 0;` |
|   3651 | 2755 | `	sxu32 n = 0;` |
|      - | 2756 | `	/* Register disk-related functions */` |
| 178659 | 2757 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 175013 | 2758 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
|  87509 | 2759 | `	}` |
| 189597 | 2760 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 185951 | 2761 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  92978 | 2762 | `	}` |
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
|  21881 | 2780 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  18235 | 2781 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|   9120 | 2782 | `	}` |
|      - | 2783 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2784 |  |
|      - | 2785 | `	/* Install streams if disk I/O is enabled */` |
|      - | 2786 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 2787 | `#ifdef __WINNT__` |
|      5 | 2788 | `	pFileStream = &sWinFileStream;` |
|      - | 2789 | `#elif defined(__UNIXES__)` |
|   3646 | 2790 | `	pFileStream = &sUnixFileStream;` |
|      - | 2791 | `#endif` |
|      - | 2792 | `	/* Install the php:// stream */` |
|   3651 | 2793 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   3651 | 2794 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);` |
|      - | 2795 | `#ifdef PH7_ENABLE_NET` |
|   3651 | 2796 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);` |
|      - | 2797 | `#endif` |
|   3651 | 2798 | `	if( pFileStream ){` |
|      - | 2799 | `		/* Install the file:// stream */` |
|   3651 | 2800 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   1823 | 2801 | `	}` |
|      - | 2802 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 2803 |  |
|   3651 | 2804 | `	return SXRET_OK;` |
|      5 | 2805 | `}` |
|      - | 2806 |  |
