# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2047/3112 lines (65.78%)

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
|      - |    8 | `#ifdef __UNIXES__` |
|      - |    9 | `#include <unistd.h>` |
|      - |   10 | `#include <sys/wait.h>` |
|      - |   11 | `#endif` |
|      - |   12 | `/*` |
|      - |   13 | ` * This file implement a virtual file systems (VFS) for the PH7 engine.` |
|      - |   14 | ` */` |
|      - |   15 | `/*` |
|      - |   16 | ` * Given a string containing the path of a file or directory, this function` |
|      - |   17 | ` * return the parent directory's path.` |
|      - |   18 | ` */` |
|     52 |   19 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)` |
|      5 |   20 | `{` |
|     57 |   21 | `	const char *zEnd = &zPath[nByte - 1];` |
|      - |   22 | `	int c,d;` |
|     57 |   23 | `	c = d = '/';` |
|      - |   24 | `#ifdef __WINNT__` |
|      5 |   25 | `	d = '\\';` |
|      - |   26 | `#endif` |
|   1121 |   27 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   1043 |   28 | `		zEnd--;` |
|      5 |   29 | `	}` |
|     57 |   30 | `	*pLen = (int)(zEnd-zPath);` |
|      - |   31 | `#ifdef __WINNT__` |
|      5 |   32 | `	if( (*pLen) == (int)sizeof(char) && zPath[0] == '/' ){` |
|      - |   33 | `		/* Normalize path on windows */` |
|    ! 0 |   34 | `		return "\\";` |
|      - |   35 | `	}` |
|      - |   36 | `#endif` |
|     57 |   37 | `	if( zEnd == zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d) ){` |
|      - |   38 | `		/* No separator,return "." as the current directory */` |
|      8 |   39 | `		*pLen = sizeof(char);` |
|      8 |   40 | `		return ".";` |
|      - |   41 | `	}` |
|     51 |   42 | `	if( (*pLen) == 0 ){` |
|      2 |   43 | `		*pLen = sizeof(char);` |
|      - |   44 | `#ifdef __WINNT__` |
|    ! 0 |   45 | `		return "\\";` |
|      - |   46 | `#else` |
|      2 |   47 | `		return "/";` |
|      - |   48 | `#endif` |
|      - |   49 | `	}` |
|     49 |   50 | `	return zPath;` |
|     31 |   51 | `}` |
|      - |   52 | `/*` |
|      - |   53 | ` * Compile the VFS implementations when builtins are enabled OR when disk I/O` |
|      - |   54 | ` * is explicitly enabled (i.e. PH7_DISABLE_DISK_IO is NOT defined).` |
|      - |   55 | ` */` |
|      - |   56 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |   57 | `/*` |
|      - |   58 | ` * bool chdir(string $directory)` |
|      - |   59 | ` *  Change the current directory.` |
|      - |   60 | ` * Parameters` |
|      - |   61 | ` *  $directory` |
|      - |   62 | ` *   The new current directory` |
|      - |   63 | ` * Return` |
|      - |   64 | ` *  TRUE on success or FALSE on failure.` |
|      - |   65 | ` */` |
|  13634 |   66 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |   67 | `{` |
|      - |   68 | `	const char *zPath;` |
|      - |   69 | `	ph7_vfs *pVfs;` |
|      - |   70 | `	int rc;` |
|  13639 |   71 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |   72 | `		/* Missing/Invalid argument,return FALSE */` |
|      6 |   73 | `		ph7_result_bool(pCtx,0);` |
|      6 |   74 | `		return PH7_OK;` |
|      - |   75 | `	}` |
|      - |   76 | `	/* Point to the underlying vfs */` |
|  13635 |   77 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  13635 |   78 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |   79 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |   80 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |   81 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |   82 | `			ph7_function_name(pCtx)` |
|      - |   83 | `			);` |
|    ! 0 |   84 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |   85 | `		return PH7_OK;` |
|      - |   86 | `	}` |
|      - |   87 | `	/* Point to the desired directory */` |
|  13635 |   88 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |   89 | `	/* Perform the requested operation */` |
|  13635 |   90 | `	rc = pVfs->xChdir(zPath);` |
|      - |   91 | `	/* IO return value */` |
|  13635 |   92 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  13635 |   93 | `	return PH7_OK;` |
|   6822 |   94 | `}` |
|      - |   95 | `/*` |
|      - |   96 | ` * bool chroot(string $directory)` |
|      - |   97 | ` *  Change the root directory.` |
|      - |   98 | ` * Parameters` |
|      - |   99 | ` *  $directory` |
|      - |  100 | ` *   The path to change the root directory to` |
|      - |  101 | ` * Return` |
|      - |  102 | ` *  TRUE on success or FALSE on failure.` |
|      - |  103 | ` */` |
|      6 |  104 | `static int PH7_vfs_chroot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  105 | `{` |
|      - |  106 | `	const char *zPath;` |
|      - |  107 | `	ph7_vfs *pVfs;` |
|      - |  108 | `	int rc;` |
|      7 |  109 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  110 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 |  111 | `		ph7_result_bool(pCtx,0);` |
|      5 |  112 | `		return PH7_OK;` |
|      - |  113 | `	}` |
|      - |  114 | `	/* Point to the underlying vfs */` |
|      2 |  115 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 |  116 | `	if( pVfs == 0 \|\| pVfs->xChroot == 0 ){` |
|      - |  117 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  118 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  119 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  120 | `			ph7_function_name(pCtx)` |
|      - |  121 | `			);` |
|    ! 0 |  122 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  123 | `		return PH7_OK;` |
|      - |  124 | `	}` |
|      - |  125 | `	/* Point to the desired directory */` |
|      2 |  126 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  127 | `	/* Perform the requested operation */` |
|      2 |  128 | `	rc = pVfs->xChroot(zPath);` |
|      - |  129 | `	/* IO return value */` |
|      2 |  130 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      2 |  131 | `	return PH7_OK;` |
|      4 |  132 | `}` |
|      - |  133 | `/*` |
|      - |  134 | ` * string getcwd(void)` |
|      - |  135 | ` *  Gets the current working directory.` |
|      - |  136 | ` * Parameters` |
|      - |  137 | ` *  None` |
|      - |  138 | ` * Return` |
|      - |  139 | ` *  Returns the current working directory on success, or FALSE on failure.` |
|      - |  140 | ` */` |
|     20 |  141 | `static int PH7_vfs_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  142 | `{` |
|      - |  143 | `	ph7_vfs *pVfs;` |
|      - |  144 | `	int rc;` |
|      - |  145 | `	/* Point to the underlying vfs */` |
|     25 |  146 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     25 |  147 | `	if( pVfs == 0 \|\| pVfs->xGetcwd == 0 ){` |
|    ! 0 |  148 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 |  149 | `		SXUNUSED(apArg);` |
|      - |  150 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  151 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  152 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  153 | `			ph7_function_name(pCtx)` |
|      - |  154 | `			);` |
|    ! 0 |  155 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  156 | `		return PH7_OK;` |
|      - |  157 | `	}` |
|     25 |  158 | `	ph7_result_string(pCtx,"",0);` |
|      - |  159 | `	/* Perform the requested operation */` |
|     25 |  160 | `	rc = pVfs->xGetcwd(pCtx);` |
|     25 |  161 | `	if( rc != PH7_OK ){` |
|      - |  162 | `		/* Error,return FALSE */` |
|    ! 0 |  163 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  164 | `	}` |
|     25 |  165 | `	return PH7_OK;` |
|     15 |  166 | `}` |
|      - |  167 | `/*` |
|      - |  168 | ` * bool rmdir(string $directory)` |
|      - |  169 | ` *  Removes directory.` |
|      - |  170 | ` * Parameters` |
|      - |  171 | ` *  $directory` |
|      - |  172 | ` *   The path to the directory` |
|      - |  173 | ` * Return` |
|      - |  174 | ` *  TRUE on success or FALSE on failure.` |
|      - |  175 | ` */` |
|     30 |  176 | `static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  177 | `{` |
|      - |  178 | `	const char *zPath;` |
|      - |  179 | `	ph7_vfs *pVfs;` |
|      - |  180 | `	int rc;` |
|     31 |  181 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  182 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  183 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  184 | `		return PH7_OK;` |
|      - |  185 | `	}` |
|      - |  186 | `	/* Point to the underlying vfs */` |
|     31 |  187 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     31 |  188 | `	if( pVfs == 0 \|\| pVfs->xRmdir == 0 ){` |
|      - |  189 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  190 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  191 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  192 | `			ph7_function_name(pCtx)` |
|      - |  193 | `			);` |
|    ! 0 |  194 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  195 | `		return PH7_OK;` |
|      - |  196 | `	}` |
|      - |  197 | `	/* Point to the desired directory */` |
|     31 |  198 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  199 | `	/* Perform the requested operation */` |
|     31 |  200 | `	rc = pVfs->xRmdir(zPath);` |
|      - |  201 | `	/* IO return value */` |
|     31 |  202 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     31 |  203 | `	return PH7_OK;` |
|     16 |  204 | `}` |
|      - |  205 | `/*` |
|      - |  206 | ` * bool is_dir(string $filename)` |
|      - |  207 | ` *  Tells whether the given filename is a directory.` |
|      - |  208 | ` * Parameters` |
|      - |  209 | ` *  $filename` |
|      - |  210 | ` *   Path to the file.` |
|      - |  211 | ` * Return` |
|      - |  212 | ` *  TRUE on success or FALSE on failure.` |
|      - |  213 | ` */` |
|   8494 |  214 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  215 | `{` |
|      - |  216 | `	const char *zPath;` |
|      - |  217 | `	ph7_vfs *pVfs;` |
|      - |  218 | `	int rc;` |
|   8499 |  219 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  220 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  221 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  222 | `		return PH7_OK;` |
|      - |  223 | `	}` |
|      - |  224 | `	/* Point to the underlying vfs */` |
|   8499 |  225 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   8499 |  226 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  227 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  228 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  229 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  230 | `			ph7_function_name(pCtx)` |
|      - |  231 | `			);` |
|    ! 0 |  232 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  233 | `		return PH7_OK;` |
|      - |  234 | `	}` |
|      - |  235 | `	/* Point to the desired directory */` |
|   8499 |  236 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  237 | `	/* Perform the requested operation */` |
|   8499 |  238 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  239 | `	/* IO return value */` |
|   8499 |  240 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   8499 |  241 | `	return PH7_OK;` |
|   4252 |  242 | `}` |
|      - |  243 | `/*` |
|      - |  244 | ` * bool mkdir(string $pathname[,int $mode = 0777 [,bool $recursive = false])` |
|      - |  245 | ` *  Make a directory.` |
|      - |  246 | ` * Parameters` |
|      - |  247 | ` *  $pathname` |
|      - |  248 | ` *   The directory path.` |
|      - |  249 | ` * $mode` |
|      - |  250 | ` *  The mode is 0777 by default, which means the widest possible access.` |
|      - |  251 | ` *  Note:` |
|      - |  252 | ` *   mode is ignored on Windows.` |
|      - |  253 | ` *   Note that you probably want to specify the mode as an octal number, which means` |
|      - |  254 | ` *   it should have a leading zero. The mode is also modified by the current umask` |
|      - |  255 | ` *   which you can change using umask().` |
|      - |  256 | ` * $recursive` |
|      - |  257 | ` *  Allows the creation of nested directories specified in the pathname.` |
|      - |  258 | ` *  Defaults to FALSE. (Not used)` |
|      - |  259 | ` * Return` |
|      - |  260 | ` *  TRUE on success or FALSE on failure.` |
|      - |  261 | ` */` |
|     32 |  262 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  263 | `{` |
|     34 |  264 | `	int iRecursive = 0;` |
|      - |  265 | `	const char *zPath;` |
|      - |  266 | `	ph7_vfs *pVfs;` |
|      - |  267 | `	int iMode,rc;` |
|     34 |  268 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  269 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  270 | `		ph7_result_bool(pCtx,0);` |
|      3 |  271 | `		return PH7_OK;` |
|      - |  272 | `	}` |
|      - |  273 | `	/* Point to the underlying vfs */` |
|     32 |  274 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     32 |  275 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  276 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  277 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  278 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  279 | `			ph7_function_name(pCtx)` |
|      - |  280 | `			);` |
|    ! 0 |  281 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  282 | `		return PH7_OK;` |
|      - |  283 | `	}` |
|      - |  284 | `	/* Point to the desired directory */` |
|     32 |  285 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  286 | `#ifdef __WINNT__` |
|      2 |  287 | `	iMode = 0;` |
|      - |  288 | `#else` |
|      - |  289 | `	/* Assume UNIX */` |
|     30 |  290 | `	iMode = 0777;` |
|      - |  291 | `#endif` |
|     32 |  292 | `	if( nArg > 1 ){` |
|    ! 0 |  293 | `		iMode = ph7_value_to_int(apArg[1]);` |
|    ! 0 |  294 | `		if( nArg > 2 ){` |
|    ! 0 |  295 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|    ! 0 |  296 | `		}` |
|    ! 0 |  297 | `	}` |
|      - |  298 | `	/* Perform the requested operation */` |
|     32 |  299 | `	rc = pVfs->xMkdir(zPath,iMode,iRecursive);` |
|      - |  300 | `	/* IO return value */` |
|     32 |  301 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     32 |  302 | `	return PH7_OK;` |
|     18 |  303 | `}` |
|      - |  304 | `/*` |
|      - |  305 | ` * bool rename(string $oldname,string $newname)` |
|      - |  306 | ` *  Attempts to rename oldname to newname.` |
|      - |  307 | ` * Parameters` |
|      - |  308 | ` *  $oldname` |
|      - |  309 | ` *   Old name.` |
|      - |  310 | ` *  $newname` |
|      - |  311 | ` *   New name.` |
|      - |  312 | ` * Return` |
|      - |  313 | ` *  TRUE on success or FALSE on failure.` |
|      - |  314 | ` */` |
|      4 |  315 | `static int PH7_vfs_rename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  316 | `{` |
|      - |  317 | `	const char *zOld,*zNew;` |
|      - |  318 | `	ph7_vfs *pVfs;` |
|      - |  319 | `	int rc;` |
|      5 |  320 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - |  321 | `		/* Missing/Invalid arguments,return FALSE */` |
|      3 |  322 | `		ph7_result_bool(pCtx,0);` |
|      3 |  323 | `		return PH7_OK;` |
|      - |  324 | `	}` |
|      - |  325 | `	/* Point to the underlying vfs */` |
|      3 |  326 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  327 | `	if( pVfs == 0 \|\| pVfs->xRename == 0 ){` |
|      - |  328 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  329 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  330 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  331 | `			ph7_function_name(pCtx)` |
|      - |  332 | `			);` |
|    ! 0 |  333 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  334 | `		return PH7_OK;` |
|      - |  335 | `	}` |
|      - |  336 | `	/* Perform the requested operation */` |
|      3 |  337 | `	zOld = ph7_value_to_string(apArg[0],0);` |
|      3 |  338 | `	zNew = ph7_value_to_string(apArg[1],0);` |
|      3 |  339 | `	rc = pVfs->xRename(zOld,zNew);` |
|      - |  340 | `	/* IO result */` |
|      3 |  341 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 |  342 | `	return PH7_OK;` |
|      3 |  343 | `}` |
|      - |  344 | `/*` |
|      - |  345 | ` * string realpath(string $path)` |
|      - |  346 | ` *  Returns canonicalized absolute pathname.` |
|      - |  347 | ` * Parameters` |
|      - |  348 | ` *  $path` |
|      - |  349 | ` *   Target path.` |
|      - |  350 | ` * Return` |
|      - |  351 | ` *  Canonicalized absolute pathname on success. or FALSE on failure.` |
|      - |  352 | ` */` |
|      6 |  353 | `static int PH7_vfs_realpath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  354 | `{` |
|      - |  355 | `	const char *zPath;` |
|      - |  356 | `	ph7_vfs *pVfs;` |
|      - |  357 | `        int rc;` |
|      7 |  358 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  359 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  360 | `		ph7_result_bool(pCtx,0);` |
|      3 |  361 | `		return PH7_OK;` |
|      - |  362 | `	}` |
|      - |  363 | `	/* Point to the underlying vfs */` |
|      5 |  364 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  365 | `	if( pVfs == 0 \|\| pVfs->xRealpath == 0 ){` |
|      - |  366 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  367 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  368 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  369 | `			ph7_function_name(pCtx)` |
|      - |  370 | `			);` |
|    ! 0 |  371 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  372 | `		return PH7_OK;` |
|      - |  373 | `	}` |
|      - |  374 | `	/* Set an empty string untnil the underlying OS interface change that */` |
|      5 |  375 | `	ph7_result_string(pCtx,"",0);` |
|      - |  376 | `	/* Perform the requested operation */` |
|      5 |  377 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      5 |  378 | `	rc = pVfs->xRealpath(zPath,pCtx);` |
|      5 |  379 | `	if( rc != PH7_OK ){` |
|      2 |  380 | `	 ph7_result_bool(pCtx,0);` |
|      1 |  381 | `	}` |
|      5 |  382 | `	return PH7_OK;` |
|      4 |  383 | `}` |
|      - |  384 | `/*` |
|      - |  385 | ` * int sleep(int $seconds)` |
|      - |  386 | ` *  Delays the program execution for the given number of seconds.` |
|      - |  387 | ` * Parameters` |
|      - |  388 | ` *  $seconds` |
|      - |  389 | ` *   Halt time in seconds.` |
|      - |  390 | ` * Return` |
|      - |  391 | ` *  Zero on success or FALSE on failure.` |
|      - |  392 | ` */` |
|      6 |  393 | `static int PH7_vfs_sleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  394 | `{` |
|      - |  395 | `	ph7_vfs *pVfs;` |
|      - |  396 | `	int rc,nSleep;` |
|      7 |  397 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  398 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  399 | `		ph7_result_bool(pCtx,0);` |
|      3 |  400 | `		return PH7_OK;` |
|      - |  401 | `	}` |
|      - |  402 | `	/* Point to the underlying vfs */` |
|      5 |  403 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  404 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  405 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  406 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  407 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  408 | `			ph7_function_name(pCtx)` |
|      - |  409 | `			);` |
|    ! 0 |  410 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  411 | `		return PH7_OK;` |
|      - |  412 | `	}` |
|      - |  413 | `	/* Amount to sleep */` |
|      5 |  414 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|      5 |  415 | `	if( nSleep < 0 ){` |
|      - |  416 | `		/* Invalid value,return FALSE */` |
|      3 |  417 | `		ph7_result_bool(pCtx,0);` |
|      3 |  418 | `		return PH7_OK;` |
|      - |  419 | `	}` |
|      - |  420 | `	/* Perform the requested operation (Microseconds) */` |
|      3 |  421 | `	rc = pVfs->xSleep((unsigned int)(nSleep * SX_USEC_PER_SEC));` |
|      3 |  422 | `	if( rc != PH7_OK ){` |
|      - |  423 | `		/* Return FALSE */` |
|    ! 0 |  424 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  425 | `	}else{` |
|      - |  426 | `		/* Return zero */` |
|      3 |  427 | `		ph7_result_int(pCtx,0);` |
|      - |  428 | `	}` |
|      3 |  429 | `	return PH7_OK;` |
|      4 |  430 | `}` |
|      - |  431 | `/*` |
|      - |  432 | ` * void usleep(int $micro_seconds)` |
|      - |  433 | ` *  Delays program execution for the given number of micro seconds.` |
|      - |  434 | ` * Parameters` |
|      - |  435 | ` *  $micro_seconds` |
|      - |  436 | ` *   Halt time in micro seconds. A micro second is one millionth of a second.` |
|      - |  437 | ` * Return` |
|      - |  438 | ` *  None.` |
|      - |  439 | ` */` |
|     48 |  440 | `static int PH7_vfs_usleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  441 | `{` |
|      - |  442 | `	ph7_vfs *pVfs;` |
|      - |  443 | `	int nSleep;` |
|     49 |  444 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  445 | `		/* Missing/Invalid argument,return immediately */` |
|    ! 0 |  446 | `		return PH7_OK;` |
|      - |  447 | `	}` |
|      - |  448 | `	/* Point to the underlying vfs */` |
|     49 |  449 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     49 |  450 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  451 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  452 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  453 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 |  454 | `			ph7_function_name(pCtx)` |
|      - |  455 | `			);` |
|    ! 0 |  456 | `		return PH7_OK;` |
|      - |  457 | `	}` |
|      - |  458 | `	/* Amount to sleep */` |
|     49 |  459 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|     49 |  460 | `	if( nSleep < 0 ){` |
|      - |  461 | `		/* Invalid value,return immediately */` |
|      3 |  462 | `		return PH7_OK;` |
|      - |  463 | `	}` |
|      - |  464 | `	/* Perform the requested operation (Microseconds) */` |
|     47 |  465 | `	pVfs->xSleep((unsigned int)nSleep);` |
|     47 |  466 | `	return PH7_OK;` |
|     25 |  467 | `}` |
|      - |  468 | `/*` |
|      - |  469 | ` * bool unlink (string $filename)` |
|      - |  470 | ` *  Delete a file.` |
|      - |  471 | ` * Parameters` |
|      - |  472 | ` *  $filename` |
|      - |  473 | ` *   Path to the file.` |
|      - |  474 | ` * Return` |
|      - |  475 | ` *  TRUE on success or FALSE on failure.` |
|      - |  476 | ` */` |
|  32700 |  477 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  478 | `{` |
|      - |  479 | `	const char *zPath;` |
|      - |  480 | `	ph7_vfs *pVfs;` |
|      - |  481 | `	int rc;` |
|  32705 |  482 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  483 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  484 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  485 | `		return PH7_OK;` |
|      - |  486 | `	}` |
|      - |  487 | `	/* Point to the underlying vfs */` |
|  32705 |  488 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  32705 |  489 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  490 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  491 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  492 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  493 | `			ph7_function_name(pCtx)` |
|      - |  494 | `			);` |
|    ! 0 |  495 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  496 | `		return PH7_OK;` |
|      - |  497 | `	}` |
|      - |  498 | `	/* Point to the desired directory */` |
|  32705 |  499 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  500 | `	/* Perform the requested operation */` |
|  32705 |  501 | `	rc = pVfs->xUnlink(zPath);` |
|      - |  502 | `	/* IO return value */` |
|  32705 |  503 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  32705 |  504 | `	return PH7_OK;` |
|  16355 |  505 | `}` |
|      - |  506 | `/*` |
|      - |  507 | ` * bool chmod(string $filename,int $mode)` |
|      - |  508 | ` *  Attempts to change the mode of the specified file to that given in mode.` |
|      - |  509 | ` * Parameters` |
|      - |  510 | ` *  $filename` |
|      - |  511 | ` *   Path to the file.` |
|      - |  512 | ` * $mode` |
|      - |  513 | ` *   Mode (Must be an integer)` |
|      - |  514 | ` * Return` |
|      - |  515 | ` *  TRUE on success or FALSE on failure.` |
|      - |  516 | ` */` |
|     10 |  517 | `static int PH7_vfs_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  518 | `{` |
|      - |  519 | `	const char *zPath;` |
|      - |  520 | `	ph7_vfs *pVfs;` |
|      - |  521 | `	int iMode;` |
|      - |  522 | `	int rc;` |
|     10 |  523 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  524 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  525 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  526 | `		return PH7_OK;` |
|      - |  527 | `	}` |
|      - |  528 | `	/* Point to the underlying vfs */` |
|     10 |  529 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     10 |  530 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - |  531 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  532 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  533 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  534 | `			ph7_function_name(pCtx)` |
|      - |  535 | `			);` |
|    ! 0 |  536 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  537 | `		return PH7_OK;` |
|      - |  538 | `	}` |
|      - |  539 | `	/* Point to the desired directory */` |
|     10 |  540 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  541 | `	/* Extract the mode */` |
|     10 |  542 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - |  543 | `	/* Perform the requested operation */` |
|     10 |  544 | `	rc = pVfs->xChmod(zPath,iMode);` |
|      - |  545 | `	/* IO return value */` |
|     10 |  546 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     10 |  547 | `	return PH7_OK;` |
|      5 |  548 | `}` |
|      - |  549 | `/*` |
|      - |  550 | ` * bool chown(string $filename,string $user)` |
|      - |  551 | ` *  Attempts to change the owner of the file filename to user user.` |
|      - |  552 | ` * Parameters` |
|      - |  553 | ` *  $filename` |
|      - |  554 | ` *   Path to the file.` |
|      - |  555 | ` * $user` |
|      - |  556 | ` *   Username.` |
|      - |  557 | ` * Return` |
|      - |  558 | ` *  TRUE on success or FALSE on failure.` |
|      - |  559 | ` */` |
|      6 |  560 | `static int PH7_vfs_chown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  561 | `{` |
|      - |  562 | `	const char *zPath,*zUser;` |
|      - |  563 | `	ph7_vfs *pVfs;` |
|      - |  564 | `	int rc;` |
|      7 |  565 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  566 | `		/* Missing/Invalid arguments,return FALSE */` |
|      3 |  567 | `		ph7_result_bool(pCtx,0);` |
|      3 |  568 | `		return PH7_OK;` |
|      - |  569 | `	}` |
|      - |  570 | `	/* Point to the underlying vfs */` |
|      4 |  571 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 |  572 | `	if( pVfs == 0 \|\| pVfs->xChown == 0 ){` |
|      - |  573 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  574 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  575 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  576 | `			ph7_function_name(pCtx)` |
|      - |  577 | `			);` |
|    ! 0 |  578 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  579 | `		return PH7_OK;` |
|      - |  580 | `	}` |
|      - |  581 | `	/* Point to the desired directory */` |
|      4 |  582 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  583 | `	/* Extract the user */` |
|      4 |  584 | `	zUser = ph7_value_to_string(apArg[1],0);` |
|      - |  585 | `	/* Perform the requested operation */` |
|      4 |  586 | `	rc = pVfs->xChown(zPath,zUser);` |
|      - |  587 | `	/* IO return value */` |
|      4 |  588 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 |  589 | `	return PH7_OK;` |
|      4 |  590 | `}` |
|      - |  591 | `/*` |
|      - |  592 | ` * bool chgrp(string $filename,string $group)` |
|      - |  593 | ` *  Attempts to change the group of the file filename to group.` |
|      - |  594 | ` * Parameters` |
|      - |  595 | ` *  $filename` |
|      - |  596 | ` *   Path to the file.` |
|      - |  597 | ` * $group` |
|      - |  598 | ` *   groupname.` |
|      - |  599 | ` * Return` |
|      - |  600 | ` *  TRUE on success or FALSE on failure.` |
|      - |  601 | ` */` |
|      6 |  602 | `static int PH7_vfs_chgrp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  603 | `{` |
|      - |  604 | `	const char *zPath,*zGroup;` |
|      - |  605 | `	ph7_vfs *pVfs;` |
|      - |  606 | `	int rc;` |
|      7 |  607 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  608 | `		/* Missing/Invalid arguments,return FALSE */` |
|      3 |  609 | `		ph7_result_bool(pCtx,0);` |
|      3 |  610 | `		return PH7_OK;` |
|      - |  611 | `	}` |
|      - |  612 | `	/* Point to the underlying vfs */` |
|      4 |  613 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 |  614 | `	if( pVfs == 0 \|\| pVfs->xChgrp == 0 ){` |
|      - |  615 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  616 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  617 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  618 | `			ph7_function_name(pCtx)` |
|      - |  619 | `			);` |
|    ! 0 |  620 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  621 | `		return PH7_OK;` |
|      - |  622 | `	}` |
|      - |  623 | `	/* Point to the desired directory */` |
|      4 |  624 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  625 | `	/* Extract the user */` |
|      4 |  626 | `	zGroup = ph7_value_to_string(apArg[1],0);` |
|      - |  627 | `	/* Perform the requested operation */` |
|      4 |  628 | `	rc = pVfs->xChgrp(zPath,zGroup);` |
|      - |  629 | `	/* IO return value */` |
|      4 |  630 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 |  631 | `	return PH7_OK;` |
|      4 |  632 | `}` |
|      - |  633 | `/*` |
|      - |  634 | ` * int64 disk_free_space(string $directory)` |
|      - |  635 | ` *  Returns available space on filesystem or disk partition.` |
|      - |  636 | ` * Parameters` |
|      - |  637 | ` *  $directory` |
|      - |  638 | ` *   A directory of the filesystem or disk partition.` |
|      - |  639 | ` * Return` |
|      - |  640 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  641 | ` */` |
|      2 |  642 | `static int PH7_vfs_disk_free_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  643 | `{` |
|      - |  644 | `	const char *zPath;` |
|      - |  645 | `	ph7_int64 iSize;` |
|      - |  646 | `	ph7_vfs *pVfs;` |
|      3 |  647 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  648 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  649 | `		ph7_result_bool(pCtx,0);` |
|      3 |  650 | `		return PH7_OK;` |
|      - |  651 | `	}` |
|      - |  652 | `	/* Point to the underlying vfs */` |
|    ! 0 |  653 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    ! 0 |  654 | `	if( pVfs == 0 \|\| pVfs->xFreeSpace == 0 ){` |
|      - |  655 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  656 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  657 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  658 | `			ph7_function_name(pCtx)` |
|      - |  659 | `			);` |
|    ! 0 |  660 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  661 | `		return PH7_OK;` |
|      - |  662 | `	}` |
|      - |  663 | `	/* Point to the desired directory */` |
|    ! 0 |  664 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  665 | `	/* Perform the requested operation */` |
|    ! 0 |  666 | `	iSize = pVfs->xFreeSpace(zPath);` |
|      - |  667 | `	/* IO return value */` |
|    ! 0 |  668 | `	ph7_result_int64(pCtx,iSize);` |
|    ! 0 |  669 | `	return PH7_OK;` |
|      2 |  670 | `}` |
|      - |  671 | `/*` |
|      - |  672 | ` * int64 disk_total_space(string $directory)` |
|      - |  673 | ` *  Returns the total size of a filesystem or disk partition.` |
|      - |  674 | ` * Parameters` |
|      - |  675 | ` *  $directory` |
|      - |  676 | ` *   A directory of the filesystem or disk partition.` |
|      - |  677 | ` * Return` |
|      - |  678 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  679 | ` */` |
|      2 |  680 | `static int PH7_vfs_disk_total_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  681 | `{` |
|      - |  682 | `	const char *zPath;` |
|      - |  683 | `	ph7_int64 iSize;` |
|      - |  684 | `	ph7_vfs *pVfs;` |
|      2 |  685 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  686 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  687 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  688 | `		return PH7_OK;` |
|      - |  689 | `	}` |
|      - |  690 | `	/* Point to the underlying vfs */` |
|      2 |  691 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 |  692 | `	if( pVfs == 0 \|\| pVfs->xTotalSpace == 0 ){` |
|      - |  693 | `		/* IO routine not implemented,return NULL */` |
|      3 |  694 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  695 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|      1 |  696 | `			ph7_function_name(pCtx)` |
|      - |  697 | `			);` |
|      2 |  698 | `		ph7_result_bool(pCtx,0);` |
|      2 |  699 | `		return PH7_OK;` |
|      - |  700 | `	}` |
|      - |  701 | `	/* Point to the desired directory */` |
|    ! 0 |  702 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  703 | `	/* Perform the requested operation */` |
|    ! 0 |  704 | `	iSize = pVfs->xTotalSpace(zPath);` |
|      - |  705 | `	/* IO return value */` |
|    ! 0 |  706 | `	ph7_result_int64(pCtx,iSize);` |
|    ! 0 |  707 | `	return PH7_OK;` |
|      1 |  708 | `}` |
|      - |  709 | `/*` |
|      - |  710 | ` * bool file_exists(string $filename)` |
|      - |  711 | ` *  Checks whether a file or directory exists.` |
|      - |  712 | ` * Parameters` |
|      - |  713 | ` *  $filename` |
|      - |  714 | ` *   Path to the file.` |
|      - |  715 | ` * Return` |
|      - |  716 | ` *  TRUE on success or FALSE on failure.` |
|      - |  717 | ` */` |
|     54 |  718 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  719 | `{` |
|      - |  720 | `	const char *zPath;` |
|      - |  721 | `	ph7_vfs *pVfs;` |
|      - |  722 | `	int rc;` |
|     56 |  723 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  724 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  725 | `		ph7_result_bool(pCtx,0);` |
|      3 |  726 | `		return PH7_OK;` |
|      - |  727 | `	}` |
|      - |  728 | `	/* Point to the underlying vfs */` |
|     54 |  729 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     54 |  730 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - |  731 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  732 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  733 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  734 | `			ph7_function_name(pCtx)` |
|      - |  735 | `			);` |
|    ! 0 |  736 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  737 | `		return PH7_OK;` |
|      - |  738 | `	}` |
|      - |  739 | `	/* Point to the desired directory */` |
|     54 |  740 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  741 | `	/* Perform the requested operation */` |
|     54 |  742 | `	rc = pVfs->xFileExists(zPath);` |
|      - |  743 | `	/* IO return value */` |
|     54 |  744 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     54 |  745 | `	return PH7_OK;` |
|     29 |  746 | `}` |
|      - |  747 | `/*` |
|      - |  748 | ` * int64 file_size(string $filename)` |
|      - |  749 | ` *  Gets the size for the given file.` |
|      - |  750 | ` * Parameters` |
|      - |  751 | ` *  $filename` |
|      - |  752 | ` *   Path to the file.` |
|      - |  753 | ` * Return` |
|      - |  754 | ` *  File size on success or FALSE on failure.` |
|      - |  755 | ` */` |
|     26 |  756 | `static int PH7_vfs_file_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  757 | `{` |
|      - |  758 | `	const char *zPath;` |
|      - |  759 | `	ph7_int64 iSize;` |
|      - |  760 | `	ph7_vfs *pVfs;` |
|     27 |  761 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  762 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  763 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  764 | `		return PH7_OK;` |
|      - |  765 | `	}` |
|      - |  766 | `	/* Point to the underlying vfs */` |
|     27 |  767 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     27 |  768 | `	if( pVfs == 0 \|\| pVfs->xFileSize == 0 ){` |
|      - |  769 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  770 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  771 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  772 | `			ph7_function_name(pCtx)` |
|      - |  773 | `			);` |
|    ! 0 |  774 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  775 | `		return PH7_OK;` |
|      - |  776 | `	}` |
|      - |  777 | `	/* Point to the desired directory */` |
|     27 |  778 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  779 | `	/* Perform the requested operation */` |
|     27 |  780 | `	iSize = pVfs->xFileSize(zPath);` |
|      - |  781 | `	/* IO return value */` |
|     27 |  782 | `	ph7_result_int64(pCtx,iSize);` |
|     27 |  783 | `	return PH7_OK;` |
|     14 |  784 | `}` |
|      - |  785 | `/*` |
|      - |  786 | ` * int64 fileatime(string $filename)` |
|      - |  787 | ` *  Gets the last access time of the given file.` |
|      - |  788 | ` * Parameters` |
|      - |  789 | ` *  $filename` |
|      - |  790 | ` *   Path to the file.` |
|      - |  791 | ` * Return` |
|      - |  792 | ` *  File atime on success or FALSE on failure.` |
|      - |  793 | ` */` |
|      2 |  794 | `static int PH7_vfs_file_atime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  795 | `{` |
|      - |  796 | `	const char *zPath;` |
|      - |  797 | `	ph7_int64 iTime;` |
|      - |  798 | `	ph7_vfs *pVfs;` |
|      3 |  799 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  800 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  801 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  802 | `		return PH7_OK;` |
|      - |  803 | `	}` |
|      - |  804 | `	/* Point to the underlying vfs */` |
|      3 |  805 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  806 | `	if( pVfs == 0 \|\| pVfs->xFileAtime == 0 ){` |
|      - |  807 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  808 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  809 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  810 | `			ph7_function_name(pCtx)` |
|      - |  811 | `			);` |
|    ! 0 |  812 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  813 | `		return PH7_OK;` |
|      - |  814 | `	}` |
|      - |  815 | `	/* Point to the desired directory */` |
|      3 |  816 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  817 | `	/* Perform the requested operation */` |
|      3 |  818 | `	iTime = pVfs->xFileAtime(zPath);` |
|      - |  819 | `	/* IO return value */` |
|      3 |  820 | `	ph7_result_int64(pCtx,iTime);` |
|      3 |  821 | `	return PH7_OK;` |
|      2 |  822 | `}` |
|      - |  823 | `/*` |
|      - |  824 | ` * int64 filemtime(string $filename)` |
|      - |  825 | ` *  Gets file modification time.` |
|      - |  826 | ` * Parameters` |
|      - |  827 | ` *  $filename` |
|      - |  828 | ` *   Path to the file.` |
|      - |  829 | ` * Return` |
|      - |  830 | ` *  File mtime on success or FALSE on failure.` |
|      - |  831 | ` */` |
|      4 |  832 | `static int PH7_vfs_file_mtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  833 | `{` |
|      - |  834 | `	const char *zPath;` |
|      - |  835 | `	ph7_int64 iTime;` |
|      - |  836 | `	ph7_vfs *pVfs;` |
|      5 |  837 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  838 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  839 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  840 | `		return PH7_OK;` |
|      - |  841 | `	}` |
|      - |  842 | `	/* Point to the underlying vfs */` |
|      5 |  843 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  844 | `	if( pVfs == 0 \|\| pVfs->xFileMtime == 0 ){` |
|      - |  845 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  846 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  847 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  848 | `			ph7_function_name(pCtx)` |
|      - |  849 | `			);` |
|    ! 0 |  850 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  851 | `		return PH7_OK;` |
|      - |  852 | `	}` |
|      - |  853 | `	/* Point to the desired directory */` |
|      5 |  854 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  855 | `	/* Perform the requested operation */` |
|      5 |  856 | `	iTime = pVfs->xFileMtime(zPath);` |
|      - |  857 | `	/* IO return value */` |
|      5 |  858 | `	ph7_result_int64(pCtx,iTime);` |
|      5 |  859 | `	return PH7_OK;` |
|      3 |  860 | `}` |
|      - |  861 | `/*` |
|      - |  862 | ` * int64 filectime(string $filename)` |
|      - |  863 | ` *  Gets inode change time of file.` |
|      - |  864 | ` * Parameters` |
|      - |  865 | ` *  $filename` |
|      - |  866 | ` *   Path to the file.` |
|      - |  867 | ` * Return` |
|      - |  868 | ` *  File ctime on success or FALSE on failure.` |
|      - |  869 | ` */` |
|      2 |  870 | `static int PH7_vfs_file_ctime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  871 | `{` |
|      - |  872 | `	const char *zPath;` |
|      - |  873 | `	ph7_int64 iTime;` |
|      - |  874 | `	ph7_vfs *pVfs;` |
|      3 |  875 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  876 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  877 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  878 | `		return PH7_OK;` |
|      - |  879 | `	}` |
|      - |  880 | `	/* Point to the underlying vfs */` |
|      3 |  881 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  882 | `	if( pVfs == 0 \|\| pVfs->xFileCtime == 0 ){` |
|      - |  883 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  884 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  885 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  886 | `			ph7_function_name(pCtx)` |
|      - |  887 | `			);` |
|    ! 0 |  888 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  889 | `		return PH7_OK;` |
|      - |  890 | `	}` |
|      - |  891 | `	/* Point to the desired directory */` |
|      3 |  892 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  893 | `	/* Perform the requested operation */` |
|      3 |  894 | `	iTime = pVfs->xFileCtime(zPath);` |
|      - |  895 | `	/* IO return value */` |
|      3 |  896 | `	ph7_result_int64(pCtx,iTime);` |
|      3 |  897 | `	return PH7_OK;` |
|      2 |  898 | `}` |
|      - |  899 | `/*` |
|      - |  900 | ` * bool is_file(string $filename)` |
|      - |  901 | ` *  Tells whether the filename is a regular file.` |
|      - |  902 | ` * Parameters` |
|      - |  903 | ` *  $filename` |
|      - |  904 | ` *   Path to the file.` |
|      - |  905 | ` * Return` |
|      - |  906 | ` *  TRUE on success or FALSE on failure.` |
|      - |  907 | ` */` |
|   6512 |  908 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  909 | `{` |
|      - |  910 | `	const char *zPath;` |
|      - |  911 | `	ph7_vfs *pVfs;` |
|      - |  912 | `	int rc;` |
|   6517 |  913 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  914 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  915 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  916 | `		return PH7_OK;` |
|      - |  917 | `	}` |
|      - |  918 | `	/* Point to the underlying vfs */` |
|   6517 |  919 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   6517 |  920 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - |  921 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  922 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  923 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  924 | `			ph7_function_name(pCtx)` |
|      - |  925 | `			);` |
|    ! 0 |  926 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  927 | `		return PH7_OK;` |
|      - |  928 | `	}` |
|      - |  929 | `	/* Point to the desired directory */` |
|   6517 |  930 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  931 | `	/* Perform the requested operation */` |
|   6517 |  932 | `	rc = pVfs->xIsfile(zPath);` |
|      - |  933 | `	/* IO return value */` |
|   6517 |  934 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   6517 |  935 | `	return PH7_OK;` |
|   3261 |  936 | `}` |
|      - |  937 | `/*` |
|      - |  938 | ` * bool is_link(string $filename)` |
|      - |  939 | ` *  Tells whether the filename is a symbolic link.` |
|      - |  940 | ` * Parameters` |
|      - |  941 | ` *  $filename` |
|      - |  942 | ` *   Path to the file.` |
|      - |  943 | ` * Return` |
|      - |  944 | ` *  TRUE on success or FALSE on failure.` |
|      - |  945 | ` */` |
|      4 |  946 | `static int PH7_vfs_is_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  947 | `{` |
|      - |  948 | `	const char *zPath;` |
|      - |  949 | `	ph7_vfs *pVfs;` |
|      - |  950 | `	int rc;` |
|      4 |  951 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  952 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  953 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  954 | `		return PH7_OK;` |
|      - |  955 | `	}` |
|      - |  956 | `	/* Point to the underlying vfs */` |
|      4 |  957 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 |  958 | `	if( pVfs == 0 \|\| pVfs->xIslink == 0 ){` |
|      - |  959 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  960 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  961 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  962 | `			ph7_function_name(pCtx)` |
|      - |  963 | `			);` |
|    ! 0 |  964 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  965 | `		return PH7_OK;` |
|      - |  966 | `	}` |
|      - |  967 | `	/* Point to the desired directory */` |
|      4 |  968 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  969 | `	/* Perform the requested operation */` |
|      4 |  970 | `	rc = pVfs->xIslink(zPath);` |
|      - |  971 | `	/* IO return value */` |
|      4 |  972 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 |  973 | `	return PH7_OK;` |
|      2 |  974 | `}` |
|      - |  975 | `/*` |
|      - |  976 | ` * bool is_readable(string $filename)` |
|      - |  977 | ` *  Tells whether a file exists and is readable.` |
|      - |  978 | ` * Parameters` |
|      - |  979 | ` *  $filename` |
|      - |  980 | ` *   Path to the file.` |
|      - |  981 | ` * Return` |
|      - |  982 | ` *  TRUE on success or FALSE on failure.` |
|      - |  983 | ` */` |
|      2 |  984 | `static int PH7_vfs_is_readable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  985 | `{` |
|      - |  986 | `	const char *zPath;` |
|      - |  987 | `	ph7_vfs *pVfs;` |
|      - |  988 | `	int rc;` |
|      2 |  989 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  990 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  991 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  992 | `		return PH7_OK;` |
|      - |  993 | `	}` |
|      - |  994 | `	/* Point to the underlying vfs */` |
|      2 |  995 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 |  996 | `	if( pVfs == 0 \|\| pVfs->xReadable == 0 ){` |
|      - |  997 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  998 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  999 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1000 | `			ph7_function_name(pCtx)` |
|      - | 1001 | `			);` |
|    ! 0 | 1002 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1003 | `		return PH7_OK;` |
|      - | 1004 | `	}` |
|      - | 1005 | `	/* Point to the desired directory */` |
|      2 | 1006 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1007 | `	/* Perform the requested operation */` |
|      2 | 1008 | `	rc = pVfs->xReadable(zPath);` |
|      - | 1009 | `	/* IO return value */` |
|      2 | 1010 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      2 | 1011 | `	return PH7_OK;` |
|      1 | 1012 | `}` |
|      - | 1013 | `/*` |
|      - | 1014 | ` * bool is_writable(string $filename)` |
|      - | 1015 | ` *  Tells whether the filename is writable.` |
|      - | 1016 | ` * Parameters` |
|      - | 1017 | ` *  $filename` |
|      - | 1018 | ` *   Path to the file.` |
|      - | 1019 | ` * Return` |
|      - | 1020 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1021 | ` */` |
|      4 | 1022 | `static int PH7_vfs_is_writable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1023 | `{` |
|      - | 1024 | `	const char *zPath;` |
|      - | 1025 | `	ph7_vfs *pVfs;` |
|      - | 1026 | `	int rc;` |
|      4 | 1027 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1028 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1029 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1030 | `		return PH7_OK;` |
|      - | 1031 | `	}` |
|      - | 1032 | `	/* Point to the underlying vfs */` |
|      4 | 1033 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 | 1034 | `	if( pVfs == 0 \|\| pVfs->xWritable == 0 ){` |
|      - | 1035 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1036 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1037 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1038 | `			ph7_function_name(pCtx)` |
|      - | 1039 | `			);` |
|    ! 0 | 1040 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1041 | `		return PH7_OK;` |
|      - | 1042 | `	}` |
|      - | 1043 | `	/* Point to the desired directory */` |
|      4 | 1044 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1045 | `	/* Perform the requested operation */` |
|      4 | 1046 | `	rc = pVfs->xWritable(zPath);` |
|      - | 1047 | `	/* IO return value */` |
|      4 | 1048 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 | 1049 | `	return PH7_OK;` |
|      2 | 1050 | `}` |
|      - | 1051 | `/*` |
|      - | 1052 | ` * bool is_executable(string $filename)` |
|      - | 1053 | ` *  Tells whether the filename is executable.` |
|      - | 1054 | ` * Parameters` |
|      - | 1055 | ` *  $filename` |
|      - | 1056 | ` *   Path to the file.` |
|      - | 1057 | ` * Return` |
|      - | 1058 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1059 | ` */` |
|      2 | 1060 | `static int PH7_vfs_is_executable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1061 | `{` |
|      - | 1062 | `	const char *zPath;` |
|      - | 1063 | `	ph7_vfs *pVfs;` |
|      - | 1064 | `	int rc;` |
|      2 | 1065 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1066 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1067 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1068 | `		return PH7_OK;` |
|      - | 1069 | `	}` |
|      - | 1070 | `	/* Point to the underlying vfs */` |
|      2 | 1071 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 1072 | `	if( pVfs == 0 \|\| pVfs->xExecutable == 0 ){` |
|      - | 1073 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1074 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1075 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1076 | `			ph7_function_name(pCtx)` |
|      - | 1077 | `			);` |
|    ! 0 | 1078 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1079 | `		return PH7_OK;` |
|      - | 1080 | `	}` |
|      - | 1081 | `	/* Point to the desired directory */` |
|      2 | 1082 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1083 | `	/* Perform the requested operation */` |
|      2 | 1084 | `	rc = pVfs->xExecutable(zPath);` |
|      - | 1085 | `	/* IO return value */` |
|      2 | 1086 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      2 | 1087 | `	return PH7_OK;` |
|      1 | 1088 | `}` |
|      - | 1089 | `/*` |
|      - | 1090 | ` * string filetype(string $filename)` |
|      - | 1091 | ` *  Gets file type.` |
|      - | 1092 | ` * Parameters` |
|      - | 1093 | ` *  $filename` |
|      - | 1094 | ` *   Path to the file.` |
|      - | 1095 | ` * Return` |
|      - | 1096 | ` *  The type of the file. Possible values are fifo, char, dir, block, link` |
|      - | 1097 | ` *  file, socket and unknown.` |
|      - | 1098 | ` */` |
|      4 | 1099 | `static int PH7_vfs_filetype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1100 | `{` |
|      - | 1101 | `	const char *zPath;` |
|      - | 1102 | `	ph7_vfs *pVfs;` |
|      5 | 1103 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1104 | `		/* Missing/Invalid argument,return 'unknown' */` |
|    ! 0 | 1105 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 1106 | `		return PH7_OK;` |
|      - | 1107 | `	}` |
|      - | 1108 | `	/* Point to the underlying vfs */` |
|      5 | 1109 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1110 | `	if( pVfs == 0 \|\| pVfs->xFiletype == 0 ){` |
|      - | 1111 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1112 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1113 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1114 | `			ph7_function_name(pCtx)` |
|      - | 1115 | `			);` |
|    ! 0 | 1116 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1117 | `		return PH7_OK;` |
|      - | 1118 | `	}` |
|      - | 1119 | `	/* Point to the desired directory */` |
|      5 | 1120 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1121 | `	/* Set the empty string as the default return value */` |
|      5 | 1122 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1123 | `	/* Perform the requested operation */` |
|      5 | 1124 | `	pVfs->xFiletype(zPath,pCtx);` |
|      5 | 1125 | `	return PH7_OK;` |
|      3 | 1126 | `}` |
|      - | 1127 | `/*` |
|      - | 1128 | ` * array stat(string $filename)` |
|      - | 1129 | ` *  Gives information about a file.` |
|      - | 1130 | ` * Parameters` |
|      - | 1131 | ` *  $filename` |
|      - | 1132 | ` *   Path to the file.` |
|      - | 1133 | ` * Return` |
|      - | 1134 | ` *  An associative array on success holding the following entries on success` |
|      - | 1135 | ` *  0   dev     device number` |
|      - | 1136 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1137 | ` * 2    mode    inode protection mode` |
|      - | 1138 | ` * 3    nlink   number of links` |
|      - | 1139 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1140 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1141 | ` * 6    rdev    device type, if inode device` |
|      - | 1142 | ` * 7    size    size in bytes` |
|      - | 1143 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1144 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1145 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1146 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1147 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1148 | ` * Note:` |
|      - | 1149 | ` *  FALSE is returned on failure.` |
|      - | 1150 | ` */` |
|      4 | 1151 | `static int PH7_vfs_stat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1152 | `{` |
|      - | 1153 | `	ph7_value *pArray,*pValue;` |
|      - | 1154 | `	const char *zPath;` |
|      - | 1155 | `	ph7_vfs *pVfs;` |
|      - | 1156 | `	int rc;` |
|      5 | 1157 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1158 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1159 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1160 | `		return PH7_OK;` |
|      - | 1161 | `	}` |
|      - | 1162 | `	/* Point to the underlying vfs */` |
|      5 | 1163 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1164 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|      - | 1165 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1166 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1167 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1168 | `			ph7_function_name(pCtx)` |
|      - | 1169 | `			);` |
|    ! 0 | 1170 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1171 | `		return PH7_OK;` |
|      - | 1172 | `	}` |
|      - | 1173 | `	/* Create the array and the working value */` |
|      5 | 1174 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 1175 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 1176 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1177 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1178 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1179 | `		return PH7_OK;` |
|      - | 1180 | `	}` |
|      - | 1181 | `	/* Extract the file path */` |
|      5 | 1182 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1183 | `	/* Perform the requested operation */` |
|      5 | 1184 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|      5 | 1185 | `	if( rc != PH7_OK ){` |
|      - | 1186 | `		/* IO error,return FALSE */` |
|    ! 0 | 1187 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1188 | `	}else{` |
|      - | 1189 | `		/* Return the associative array */` |
|      5 | 1190 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1191 | `	}` |
|      - | 1192 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1193 | `	 * automatically as soon we return from this function. */` |
|      5 | 1194 | `	return PH7_OK;` |
|      3 | 1195 | `}` |
|      - | 1196 | `/*` |
|      - | 1197 | ` * array lstat(string $filename)` |
|      - | 1198 | ` *  Gives information about a file or symbolic link.` |
|      - | 1199 | ` * Parameters` |
|      - | 1200 | ` *  $filename` |
|      - | 1201 | ` *   Path to the file.` |
|      - | 1202 | ` * Return` |
|      - | 1203 | ` *  An associative array on success holding the following entries on success` |
|      - | 1204 | ` *  0   dev     device number` |
|      - | 1205 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1206 | ` * 2    mode    inode protection mode` |
|      - | 1207 | ` * 3    nlink   number of links` |
|      - | 1208 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1209 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1210 | ` * 6    rdev    device type, if inode device` |
|      - | 1211 | ` * 7    size    size in bytes` |
|      - | 1212 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1213 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1214 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1215 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1216 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1217 | ` * Note:` |
|      - | 1218 | ` *  FALSE is returned on failure.` |
|      - | 1219 | ` */` |
|      2 | 1220 | `static int PH7_vfs_lstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1221 | `{` |
|      - | 1222 | `	ph7_value *pArray,*pValue;` |
|      - | 1223 | `	const char *zPath;` |
|      - | 1224 | `	ph7_vfs *pVfs;` |
|      - | 1225 | `	int rc;` |
|      2 | 1226 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1227 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1228 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1229 | `		return PH7_OK;` |
|      - | 1230 | `	}` |
|      - | 1231 | `	/* Point to the underlying vfs */` |
|      2 | 1232 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 1233 | `	if( pVfs == 0 \|\| pVfs->xlStat == 0 ){` |
|      - | 1234 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1235 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1236 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1237 | `			ph7_function_name(pCtx)` |
|      - | 1238 | `			);` |
|    ! 0 | 1239 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1240 | `		return PH7_OK;` |
|      - | 1241 | `	}` |
|      - | 1242 | `	/* Create the array and the working value */` |
|      2 | 1243 | `	pArray = ph7_context_new_array(pCtx);` |
|      2 | 1244 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      2 | 1245 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1246 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1247 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1248 | `		return PH7_OK;` |
|      - | 1249 | `	}` |
|      - | 1250 | `	/* Extract the file path */` |
|      2 | 1251 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1252 | `	/* Perform the requested operation */` |
|      2 | 1253 | `	rc = pVfs->xlStat(zPath,pArray,pValue);` |
|      2 | 1254 | `	if( rc != PH7_OK ){` |
|      - | 1255 | `		/* IO error,return FALSE */` |
|    ! 0 | 1256 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1257 | `	}else{` |
|      - | 1258 | `		/* Return the associative array */` |
|      2 | 1259 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1260 | `	}` |
|      - | 1261 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1262 | `	 * automatically as soon we return from this function. */` |
|      2 | 1263 | `	return PH7_OK;` |
|      1 | 1264 | `}` |
|      - | 1265 | `/*` |
|      - | 1266 | ` * string getenv(string $varname)` |
|      - | 1267 | ` *  Gets the value of an environment variable.` |
|      - | 1268 | ` * Parameters` |
|      - | 1269 | ` *  $varname` |
|      - | 1270 | ` *   The variable name.` |
|      - | 1271 | ` * Return` |
|      - | 1272 | ` *  Returns the value of the environment variable varname, or FALSE if the environment` |
|      - | 1273 | ` * variable varname does not exist.` |
|      - | 1274 | ` */` |
|     48 | 1275 | `static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1276 | `{` |
|      - | 1277 | `	const char *zEnv;` |
|      - | 1278 | `	ph7_vfs *pVfs;` |
|      - | 1279 | `	int iLen;` |
|     53 | 1280 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1281 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1282 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1283 | `		return PH7_OK;` |
|      - | 1284 | `	}` |
|      - | 1285 | `	/* Point to the underlying vfs */` |
|     53 | 1286 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     53 | 1287 | `	if( pVfs == 0 \|\| pVfs->xGetenv == 0 ){` |
|      - | 1288 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1289 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1290 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1291 | `			ph7_function_name(pCtx)` |
|      - | 1292 | `			);` |
|    ! 0 | 1293 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1294 | `		return PH7_OK;` |
|      - | 1295 | `	}` |
|      - | 1296 | `	/* Extract the environment variable */` |
|     53 | 1297 | `	zEnv = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1298 | `	/* Set a boolean FALSE as the default return value */` |
|     53 | 1299 | `	ph7_result_bool(pCtx,0);` |
|     53 | 1300 | `	if( iLen < 1 ){` |
|      - | 1301 | `		/* Empty string */` |
|    ! 0 | 1302 | `		return PH7_OK;` |
|      - | 1303 | `	}` |
|      - | 1304 | `	/* Perform the requested operation */` |
|     53 | 1305 | `	pVfs->xGetenv(zEnv,pCtx);` |
|     53 | 1306 | `	return PH7_OK;` |
|     29 | 1307 | `}` |
|      - | 1308 | `/*` |
|      - | 1309 | ` * bool putenv(string $settings)` |
|      - | 1310 | ` *  Set the value of an environment variable.` |
|      - | 1311 | ` * Parameters` |
|      - | 1312 | ` *  $setting` |
|      - | 1313 | ` *   The setting, like "FOO=BAR"` |
|      - | 1314 | ` * Return` |
|      - | 1315 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1316 | ` */` |
|      6 | 1317 | `static int PH7_vfs_putenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1318 | `{` |
|      - | 1319 | `	const char *zName,*zValue;` |
|      - | 1320 | `	char *zSettings,*zEnd;` |
|      - | 1321 | `	ph7_vfs *pVfs;` |
|      - | 1322 | `	int iLen,rc;` |
|      7 | 1323 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1324 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1325 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1326 | `		return PH7_OK;` |
|      - | 1327 | `	}` |
|      - | 1328 | `	/* Extract the setting variable */` |
|      7 | 1329 | `	zSettings = (char *)ph7_value_to_string(apArg[0],&iLen);` |
|      7 | 1330 | `	if( iLen < 1 ){` |
|      - | 1331 | `		/* Empty string,return FALSE */` |
|    ! 0 | 1332 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1333 | `		return PH7_OK;` |
|      - | 1334 | `	}` |
|      - | 1335 | `	/* Parse the setting */` |
|      7 | 1336 | `	zEnd = &zSettings[iLen];` |
|      7 | 1337 | `	zValue = 0;` |
|      7 | 1338 | `	zName = zSettings;` |
|    127 | 1339 | `	while( zSettings < zEnd ){` |
|    127 | 1340 | `		if( zSettings[0] == '=' ){` |
|      - | 1341 | `			/* Null terminate the name */` |
|      7 | 1342 | `			zSettings[0] = 0;` |
|      7 | 1343 | `			zValue = &zSettings[1];` |
|      7 | 1344 | `			break;` |
|      - | 1345 | `		}` |
|    121 | 1346 | `		zSettings++;` |
|      1 | 1347 | `	}` |
|      - | 1348 | `	/* Install the environment variable in the $_Env array */` |
|      7 | 1349 | `	if( zValue == 0 \|\| zName[0] == 0 \|\| zValue >= zEnd \|\| zName >= zValue ){` |
|      - | 1350 | `		/* Invalid settings,retun FALSE */` |
|      5 | 1351 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1352 | `		if( zSettings  < zEnd ){` |
|      5 | 1353 | `			zSettings[0] = '=';` |
|      2 | 1354 | `		}` |
|      5 | 1355 | `		return PH7_OK;` |
|      - | 1356 | `	}` |
|      3 | 1357 | `	ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_ENV_ATTR,zName,zValue,(int)(zEnd-zValue));` |
|      - | 1358 | `	/* Point to the underlying vfs */` |
|      3 | 1359 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1360 | `	if( pVfs == 0 \|\| pVfs->xSetenv == 0 ){` |
|      - | 1361 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1362 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1363 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1364 | `			ph7_function_name(pCtx)` |
|      - | 1365 | `			);` |
|    ! 0 | 1366 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1367 | `		zSettings[0] = '=';` |
|    ! 0 | 1368 | `		return PH7_OK;` |
|      - | 1369 | `	}` |
|      - | 1370 | `	/* Perform the requested operation */` |
|      3 | 1371 | `	rc = pVfs->xSetenv(zName,zValue);` |
|      3 | 1372 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 1373 | `	zSettings[0] = '=';` |
|      3 | 1374 | `	return PH7_OK;` |
|      4 | 1375 | `}` |
|      - | 1376 | `/*` |
|      - | 1377 | ` * bool touch(string $filename[,int64 $time = time()[,int64 $atime]])` |
|      - | 1378 | ` *  Sets access and modification time of file.` |
|      - | 1379 | ` * Note: On windows` |
|      - | 1380 | ` *   If the file does not exists,it will not be created.` |
|      - | 1381 | ` * Parameters` |
|      - | 1382 | ` *  $filename` |
|      - | 1383 | ` *   The name of the file being touched.` |
|      - | 1384 | ` *  $time` |
|      - | 1385 | ` *   The touch time. If time is not supplied, the current system time is used.` |
|      - | 1386 | ` * $atime` |
|      - | 1387 | ` *   If present, the access time of the given filename is set to the value of atime.` |
|      - | 1388 | ` *   Otherwise, it is set to the value passed to the time parameter. If neither are` |
|      - | 1389 | ` *   present, the current system time is used.` |
|      - | 1390 | ` * Return` |
|      - | 1391 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1392 | `*/` |
|      4 | 1393 | `static int PH7_vfs_touch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1394 | `{` |
|      - | 1395 | `	ph7_int64 nTime,nAccess;` |
|      - | 1396 | `	const char *zFile;` |
|      - | 1397 | `	ph7_vfs *pVfs;` |
|      - | 1398 | `	int rc;` |
|      5 | 1399 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1400 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1401 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1402 | `		return PH7_OK;` |
|      - | 1403 | `	}` |
|      - | 1404 | `	/* Point to the underlying vfs */` |
|      5 | 1405 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1406 | `	if( pVfs == 0 \|\| pVfs->xTouch == 0 ){` |
|      - | 1407 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1408 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1409 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1410 | `			ph7_function_name(pCtx)` |
|      - | 1411 | `			);` |
|    ! 0 | 1412 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1413 | `		return PH7_OK;` |
|      - | 1414 | `	}` |
|      - | 1415 | `	/* Perform the requested operation */` |
|      5 | 1416 | `	nTime = nAccess = -1;` |
|      5 | 1417 | `	zFile = ph7_value_to_string(apArg[0],0);` |
|      5 | 1418 | `	if( nArg > 1 ){` |
|      2 | 1419 | `		nTime = ph7_value_to_int64(apArg[1]);` |
|      2 | 1420 | `		if( nArg > 2 ){` |
|      2 | 1421 | `			nAccess = ph7_value_to_int64(apArg[1]);` |
|      1 | 1422 | `		}else{` |
|    ! 0 | 1423 | `			nAccess = nTime;` |
|      - | 1424 | `		}` |
|      1 | 1425 | `	}` |
|      5 | 1426 | `	rc = pVfs->xTouch(zFile,nTime,nAccess);` |
|      - | 1427 | `	/* IO result */` |
|      5 | 1428 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 1429 | `	return PH7_OK;` |
|      3 | 1430 | `}` |
|      - | 1431 | `/*` |
|      - | 1432 | ` * Path processing functions that do not need access to the VFS layer` |
|      - | 1433 | ` * Status:` |
|      - | 1434 | ` *    Stable.` |
|      - | 1435 | ` */` |
|      - | 1436 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1437 | `/*` |
|      - | 1438 | ` * string dirname(string $path)` |
|      - | 1439 |  |
|      - | 1440 | ` *  Returns parent directory's path.` |
|      - | 1441 | ` * Parameters` |
|      - | 1442 | ` * $path` |
|      - | 1443 | ` *  Target path.` |
|      - | 1444 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1445 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1446 | ` * Return` |
|      - | 1447 | ` *  The path of the parent directory. If there are no slashes in path, a dot ('.')` |
|      - | 1448 | ` *  is returned, indicating the current directory.` |
|      - | 1449 | ` */` |
|     14 | 1450 | `static int PH7_builtin_dirname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1451 | `{` |
|      - | 1452 | `	const char *zPath,*zDir;` |
|      - | 1453 | `	int iLen,iDirlen;` |
|     19 | 1454 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1455 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 1456 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1457 | `		return PH7_OK;` |
|      - | 1458 | `	}` |
|      - | 1459 | `	/* Point to the target path */` |
|     19 | 1460 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     19 | 1461 | `	if( iLen < 1 ){` |
|      - | 1462 | `		/* Reuturn "." */` |
|      2 | 1463 | `		ph7_result_string(pCtx,".",sizeof(char));` |
|      2 | 1464 | `		return PH7_OK;` |
|      - | 1465 | `	}` |
|      - | 1466 | `	/* Perform the requested operation */` |
|     17 | 1467 | `	zDir = PH7_ExtractDirName(zPath,iLen,&iDirlen);` |
|      - | 1468 | `	/* Return directory name */` |
|     17 | 1469 | `	ph7_result_string(pCtx,zDir,iDirlen);` |
|     17 | 1470 | `	return PH7_OK;` |
|     12 | 1471 | `}` |
|      - | 1472 | `/*` |
|      - | 1473 | ` * string basename(string $path[, string $suffix ])` |
|      - | 1474 | ` *  Returns trailing name component of path.` |
|      - | 1475 | ` * Parameters` |
|      - | 1476 | ` * $path` |
|      - | 1477 | ` *  Target path.` |
|      - | 1478 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1479 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1480 | ` * $suffix` |
|      - | 1481 | ` *  If the name component ends in suffix this will also be cut off.` |
|      - | 1482 | ` * Return` |
|      - | 1483 | ` *  The base name of the given path.` |
|      - | 1484 | ` */` |
|     36 | 1485 | `static int PH7_builtin_basename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1486 | `{` |
|      - | 1487 | `	const char *zPath,*zBase,*zEnd;` |
|      - | 1488 | `	int c,d,iLen;` |
|     37 | 1489 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1490 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1491 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1492 | `		return PH7_OK;` |
|      - | 1493 | `	}` |
|     37 | 1494 | `	c = d = '/';` |
|      - | 1495 | `#ifdef __WINNT__` |
|      1 | 1496 | `	d = '\\';` |
|      - | 1497 | `#endif` |
|      - | 1498 | `	/* Point to the target path */` |
|     37 | 1499 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     37 | 1500 | `	if( iLen < 1 ){` |
|      - | 1501 | `		/* Empty string */` |
|      3 | 1502 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1503 | `		return PH7_OK;` |
|      - | 1504 | `	}` |
|      - | 1505 | `	/* Perform the requested operation */` |
|     35 | 1506 | `	zEnd = &zPath[iLen - 1];` |
|      - | 1507 | `	/* Ignore trailing '/' */` |
|     57 | 1508 | `	while( zEnd > zPath && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      6 | 1509 | `		zEnd--;` |
|      1 | 1510 | `	}` |
|     35 | 1511 | `	iLen = (int)(&zEnd[1]-zPath);` |
|    758 | 1512 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    707 | 1513 | `		zEnd--;` |
|      1 | 1514 | `	}` |
|     35 | 1515 | `	zBase = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|     35 | 1516 | `	zEnd = &zPath[iLen];` |
|     35 | 1517 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 1518 | `		const char *zSuffix;` |
|      - | 1519 | `		int nSuffix;` |
|      - | 1520 | `		/* Strip suffix */` |
|      5 | 1521 | `		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);` |
|      5 | 1522 | `		if( nSuffix > 0 && nSuffix < iLen && SyMemcmp(&zEnd[-nSuffix],zSuffix,nSuffix) == 0 ){` |
|      5 | 1523 | `			zEnd -= nSuffix;` |
|      2 | 1524 | `		}` |
|      2 | 1525 | `	}` |
|      - | 1526 | `	/* Store the basename */` |
|     35 | 1527 | `	ph7_result_string(pCtx,zBase,(int)(zEnd-zBase));` |
|     35 | 1528 | `	return PH7_OK;` |
|     19 | 1529 | `}` |
|      - | 1530 | `/*` |
|      - | 1531 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1532 | ` *  Returns information about a file path.` |
|      - | 1533 | ` * Parameter` |
|      - | 1534 | ` *  $path` |
|      - | 1535 | ` *   The path to be parsed.` |
|      - | 1536 | ` *  $options` |
|      - | 1537 | ` *    If present, specifies a specific element to be returned; one of` |
|      - | 1538 | ` *      PATHINFO_DIRNAME, PATHINFO_BASENAME, PATHINFO_EXTENSION or PATHINFO_FILENAME.` |
|      - | 1539 | ` * Return` |
|      - | 1540 | ` *  If the options parameter is not passed, an associative array containing the following` |
|      - | 1541 | ` *  elements is returned: dirname, basename, extension (if any), and filename.` |
|      - | 1542 | ` *  If options is present, returns a string containing the requested element.` |
|      - | 1543 | ` */` |
|      - | 1544 | `typedef struct path_info path_info;` |
|      - | 1545 | `struct path_info` |
|      - | 1546 | `{` |
|      - | 1547 | `	SyString sDir; /* Directory [i.e: /var/www] */` |
|      - | 1548 | `	SyString sBasename; /* Basename [i.e httpd.conf] */` |
|      - | 1549 | `	SyString sExtension; /* File extension [i.e xml,pdf..] */` |
|      - | 1550 | `	SyString sFilename;  /* Filename */` |
|      - | 1551 | `};` |
|      - | 1552 | `/*` |
|      - | 1553 | ` * Extract path fields.` |
|      - | 1554 | ` */` |
|  13014 | 1555 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 1556 | `{` |
|  13019 | 1557 | `	const char *zPtr,*zEnd = &zPath[nByte - 1];` |
|      - | 1558 | `	SyString *pCur;` |
|      - | 1559 | `	int c,d;` |
|  13019 | 1560 | `	c = d = '/';` |
|      - | 1561 | `#ifdef __WINNT__` |
|      5 | 1562 | `	d = '\\';` |
|      - | 1563 | `#endif` |
|      - | 1564 | `	/* Zero the structure */` |
|  13019 | 1565 | `	SyZero(pOut,sizeof(path_info));` |
|      - | 1566 | `	/* Handle special case */` |
|  13019 | 1567 | `	if( nByte == sizeof(char) && ( (int)zPath[0] == c \|\| (int)zPath[0] == d ) ){` |
|      - | 1568 | `#ifdef __WINNT__` |
|    ! 0 | 1569 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1570 | `#else` |
|    ! 0 | 1571 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1572 | `#endif` |
|    ! 0 | 1573 | `		return SXRET_OK;` |
|      - | 1574 | `	}` |
|      - | 1575 | `	/* Extract the basename */` |
| 351174 | 1576 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
| 331653 | 1577 | `		zEnd--;` |
|      5 | 1578 | `	}` |
|  13019 | 1579 | `	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|  13019 | 1580 | `	zEnd = &zPath[nByte];` |
|      - | 1581 | `	/* dirname */` |
|  13019 | 1582 | `	pCur = &pOut->sDir;` |
|  13019 | 1583 | `	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);` |
|  13019 | 1584 | `	if( pCur->nByte > 1 ){` |
|  26033 | 1585 | `		SyStringTrimTrailingChar(pCur,'/');` |
|      - | 1586 | `#ifdef __WINNT__` |
|      5 | 1587 | `		SyStringTrimTrailingChar(pCur,'\\');` |
|      - | 1588 | `#endif` |
|   6512 | 1589 | `	}else if( (int)zPath[0] == c \|\| (int)zPath[0] == d ){` |
|      - | 1590 | `#ifdef __WINNT__` |
|    ! 0 | 1591 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1592 | `#else` |
|    ! 0 | 1593 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1594 | `#endif` |
|    ! 0 | 1595 | `	}` |
|      - | 1596 | `	/* basename/filename */` |
|  13019 | 1597 | `	pCur = &pOut->sBasename;` |
|  13019 | 1598 | `	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);` |
|  13019 | 1599 | `	SyStringTrimLeadingChar(pCur,'/');` |
|      - | 1600 | `#ifdef __WINNT__` |
|      5 | 1601 | `	SyStringTrimLeadingChar(pCur,'\\');` |
|      - | 1602 | `#endif` |
|  13019 | 1603 | `	SyStringDupPtr(&pOut->sFilename,pCur);` |
|  13019 | 1604 | `	if( pCur->nByte > 0 ){` |
|      - | 1605 | `		/* extension */` |
|  13019 | 1606 | `		zEnd--;` |
|  65069 | 1607 | `		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){` |
|  52055 | 1608 | `			zEnd--;` |
|      5 | 1609 | `		}` |
|  13019 | 1610 | `		if( zEnd > pCur->zString ){` |
|  13017 | 1611 | `			zEnd++; /* Jump leading dot */` |
|  13017 | 1612 | `			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);` |
|      - | 1613 | `			/* Fix filename */` |
|  13017 | 1614 | `			pCur = &pOut->sFilename;` |
|  13017 | 1615 | `			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){` |
|  13017 | 1616 | `				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);` |
|   6506 | 1617 | `			}` |
|   6506 | 1618 | `		}` |
|   6507 | 1619 | `	}` |
|  13019 | 1620 | `	return SXRET_OK;` |
|   6512 | 1621 | `}` |
|      - | 1622 | `/*` |
|      - | 1623 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1624 | ` *  See block comment above.` |
|      - | 1625 | ` */` |
|  13014 | 1626 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1627 | `{` |
|      - | 1628 | `	const char *zPath;` |
|      - | 1629 | `	path_info sInfo;` |
|      - | 1630 | `	SyString *pComp;` |
|      - | 1631 | `	int iLen;` |
|  13019 | 1632 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1633 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1634 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1635 | `		return PH7_OK;` |
|      - | 1636 | `	}` |
|      - | 1637 | `	/* Point to the target path */` |
|  13019 | 1638 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|  13019 | 1639 | `	if( iLen < 1 ){` |
|      - | 1640 | `		/* Empty string */` |
|    ! 0 | 1641 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1642 | `		return PH7_OK;` |
|      - | 1643 | `	}` |
|      - | 1644 | `	/* Extract path info */` |
|  13019 | 1645 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|  19525 | 1646 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1647 | `		/* Return path component */` |
|  13017 | 1648 | `		int nComp = ph7_value_to_int(apArg[1]);` |
|  13017 | 1649 | `		switch(nComp){` |
|      1 | 1650 | `		case 1: /* PATHINFO_DIRNAME */` |
|      3 | 1651 | `			pComp = &sInfo.sDir;` |
|      3 | 1652 | `			if( pComp->nByte > 0 ){` |
|      3 | 1653 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1654 | `			}else{` |
|      - | 1655 | `				/* Expand the empty string */` |
|    ! 0 | 1656 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1657 | `			}` |
|      3 | 1658 | `			break;` |
|      1 | 1659 | `		case 2: /*PATHINFO_BASENAME*/` |
|      3 | 1660 | `			pComp = &sInfo.sBasename;` |
|      3 | 1661 | `			if( pComp->nByte > 0 ){` |
|      3 | 1662 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1663 | `			}else{` |
|      - | 1664 | `				/* Expand the empty string */` |
|    ! 0 | 1665 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1666 | `			}` |
|      3 | 1667 | `			break;` |
|   3254 | 1668 | `		case 3: /*PATHINFO_EXTENSION*/` |
|   6513 | 1669 | `			pComp = &sInfo.sExtension;` |
|   6513 | 1670 | `			if( pComp->nByte > 0 ){` |
|   6511 | 1671 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3258 | 1672 | `			}else{` |
|      - | 1673 | `				/* Expand the empty string */` |
|      3 | 1674 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1675 | `			}` |
|   6513 | 1676 | `			break;` |
|   3250 | 1677 | `		case 4: /*PATHINFO_FILENAME*/` |
|   6505 | 1678 | `			pComp = &sInfo.sFilename;` |
|   6505 | 1679 | `			if( pComp->nByte > 0 ){` |
|   6505 | 1680 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3255 | 1681 | `			}else{` |
|      - | 1682 | `				/* Expand the empty string */` |
|    ! 0 | 1683 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1684 | `			}` |
|   6505 | 1685 | `			break;` |
|    ! 0 | 1686 | `		default:` |
|      - | 1687 | `			/* Expand the empty string */` |
|    ! 0 | 1688 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1689 | `			break;` |
|      - | 1690 | `		}` |
|   6511 | 1691 | `	}else{` |
|      - | 1692 | `		/* Return an associative array */` |
|      - | 1693 | `		ph7_value *pArray,*pValue;` |
|      3 | 1694 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 1695 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 1696 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1697 | `			/* Out of mem,return NULL */` |
|    ! 0 | 1698 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1699 | `			return PH7_OK;` |
|      - | 1700 | `		}` |
|      - | 1701 | `		/* dirname */` |
|      3 | 1702 | `		pComp = &sInfo.sDir;` |
|      3 | 1703 | `		if( pComp->nByte > 0 ){` |
|      3 | 1704 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1705 | `			/* Perform the insertion */` |
|      3 | 1706 | `			ph7_array_add_strkey_elem(pArray,"dirname",pValue); /* Will make it's own copy */` |
|      1 | 1707 | `		}` |
|      - | 1708 | `		/* Reset the string cursor */` |
|      3 | 1709 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1710 | `		/* basername */` |
|      3 | 1711 | `		pComp = &sInfo.sBasename;` |
|      3 | 1712 | `		if( pComp->nByte > 0 ){` |
|      3 | 1713 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1714 | `			/* Perform the insertion */` |
|      3 | 1715 | `			ph7_array_add_strkey_elem(pArray,"basename",pValue); /* Will make it's own copy */` |
|      1 | 1716 | `		}` |
|      - | 1717 | `		/* Reset the string cursor */` |
|      3 | 1718 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1719 | `		/* extension */` |
|      3 | 1720 | `		pComp = &sInfo.sExtension;` |
|      3 | 1721 | `		if( pComp->nByte > 0 ){` |
|      3 | 1722 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1723 | `			/* Perform the insertion */` |
|      3 | 1724 | `			ph7_array_add_strkey_elem(pArray,"extension",pValue); /* Will make it's own copy */` |
|      1 | 1725 | `		}` |
|      - | 1726 | `		/* Reset the string cursor */` |
|      3 | 1727 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1728 | `		/* filename */` |
|      3 | 1729 | `		pComp = &sInfo.sFilename;` |
|      3 | 1730 | `		if( pComp->nByte > 0 ){` |
|      3 | 1731 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1732 | `			/* Perform the insertion */` |
|      3 | 1733 | `			ph7_array_add_strkey_elem(pArray,"filename",pValue); /* Will make it's own copy */` |
|      1 | 1734 | `		}` |
|      - | 1735 | `		/* Return the created array */` |
|      3 | 1736 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1737 | `		/* Don't worry about freeing memory, everything will be released` |
|      - | 1738 | `		 * automatically as soon we return from this foreign function.` |
|      - | 1739 | `		 */` |
|      - | 1740 | `	}` |
|  13019 | 1741 | `	return PH7_OK;` |
|   6512 | 1742 | `}` |
|      - | 1743 | `/* SPDX-SnippetBegin */` |
|      - | 1744 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 1745 | `/* SPDX-License-Identifier: blessing */` |
|      - | 1746 | `/*` |
|      - | 1747 | ` * Globbing implementation extracted from the sqlite3 source tree.` |
|      - | 1748 |  |
|      - | 1749 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|      - | 1750 | ` * Status: Public Domain` |
|      - | 1751 | ` */` |
|      - | 1752 | `typedef unsigned char u8;` |
|      - | 1753 | `/* An array to map all upper-case characters into their corresponding` |
|      - | 1754 | `** lower-case character.` |
|      - | 1755 | `**` |
|      - | 1756 | `** SQLite only considers US-ASCII (or EBCDIC) characters.  We do not` |
|      - | 1757 | `** handle case conversions for the UTF character set since the tables` |
|      - | 1758 | `** involved are nearly as big or bigger than SQLite itself.` |
|      - | 1759 | `*/` |
|      - | 1760 | `static const unsigned char sqlite3UpperToLower[] = {` |
|      - | 1761 | `      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,` |
|      - | 1762 | `     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,` |
|      - | 1763 | `     36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,` |
|      - | 1764 | `     54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,` |
|      - | 1765 | `    104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,` |
|      - | 1766 | `    122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,` |
|      - | 1767 | `    108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,` |
|      - | 1768 | `    126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,` |
|      - | 1769 | `    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,` |
|      - | 1770 | `    162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,` |
|      - | 1771 | `    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,` |
|      - | 1772 | `    198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,` |
|      - | 1773 | `    216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,` |
|      - | 1774 | `    234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,` |
|      - | 1775 | `    252,253,254,255` |
|      - | 1776 | `};` |
|      - | 1777 | `#define GlogUpperToLower(A)     if( A<0x80 ){ A = sqlite3UpperToLower[A]; }` |
|      - | 1778 | `/*` |
|      - | 1779 | `** Assuming zIn points to the first byte of a UTF-8 character,` |
|      - | 1780 | `** advance zIn to point to the first byte of the next UTF-8 character.` |
|      - | 1781 | `*/` |
|      - | 1782 | `#define SQLITE_SKIP_UTF8(zIn) {                        \` |
|      - | 1783 | `  if( (*(zIn++))>=0xc0 ){                              \` |
|      - | 1784 | `    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \` |
|      - | 1785 | `  }                                                    \` |
|      - | 1786 | `}` |
|      - | 1787 | `/*` |
|      - | 1788 | `** Compare two UTF-8 strings for equality where the first string can` |
|      - | 1789 | `** potentially be a "glob" expression.  Return true (1) if they` |
|      - | 1790 | `** are the same and false (0) if they are different.` |
|      - | 1791 | `**` |
|      - | 1792 | `** Globbing rules:` |
|      - | 1793 | `**` |
|      - | 1794 | `**      '*'       Matches any sequence of zero or more characters.` |
|      - | 1795 | `**` |
|      - | 1796 | `**      '?'       Matches exactly one character.` |
|      - | 1797 | `**` |
|      - | 1798 | `**     [...]      Matches one character from the enclosed list of` |
|      - | 1799 | `**                characters.` |
|      - | 1800 | `**` |
|      - | 1801 | `**     [^...]     Matches one character not in the enclosed list.` |
|      - | 1802 | `**` |
|      - | 1803 | `** With the [...] and [^...] matching, a ']' character can be included` |
|      - | 1804 | `** in the list by making it the first character after '[' or '^'.  A` |
|      - | 1805 | `** range of characters can be specified using '-'.  Example:` |
|      - | 1806 | `** "[a-z]" matches any single lower-case letter.  To match a '-', make` |
|      - | 1807 | `** it the last character in the list.` |
|      - | 1808 | `**` |
|      - | 1809 | `** This routine is usually quick, but can be N**2 in the worst case.` |
|      - | 1810 | `**` |
|      - | 1811 | `** Hints: to match '*' or '?', put them in "[]".  Like this:` |
|      - | 1812 | `**` |
|      - | 1813 | `**         abc[*]xyz        Matches "abc*xyz" only` |
|      - | 1814 | `*/` |
|     20 | 1815 | `static int patternCompare(` |
|      - | 1816 | `  const u8 *zPattern,              /* The glob pattern */` |
|      - | 1817 | `  const u8 *zString,               /* The string to compare against the glob */` |
|      - | 1818 | `  const int esc,                    /* The escape character */` |
|      - | 1819 | `  int noCase` |
|      1 | 1820 | `){` |
|      - | 1821 | `  int c, c2;` |
|      - | 1822 | `  int invert;` |
|      - | 1823 | `  int seen;` |
|     21 | 1824 | `  u8 matchOne = '?';` |
|     21 | 1825 | `  u8 matchAll = '*';` |
|     21 | 1826 | `  u8 matchSet = '[';` |
|     21 | 1827 | `  int prevEscape = 0;     /* True if the previous character was 'escape' */` |
|      - | 1828 |  |
|     21 | 1829 | `  if( !zPattern \|\| !zString ) return 0;` |
|     51 | 1830 | `  while( (c = PH7_Utf8Read(zPattern,0,&zPattern))!=0 ){` |
|     43 | 1831 | `    if( !prevEscape && c==matchAll ){` |
|     16 | 1832 | `      while( (c=PH7_Utf8Read(zPattern,0,&zPattern)) == matchAll` |
|      9 | 1833 | `               \|\| c == matchOne ){` |
|    ! 0 | 1834 | `        if( c==matchOne && PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1835 | `          return 0;` |
|      - | 1836 | `        }` |
|    ! 0 | 1837 | `      }` |
|      9 | 1838 | `      if( c==0 ){` |
|    ! 0 | 1839 | `        return 1;` |
|      9 | 1840 | `      }else if( c==esc ){` |
|    ! 0 | 1841 | `        c = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1842 | `        if( c==0 ){` |
|    ! 0 | 1843 | `          return 0;` |
|    ! 0 | 1844 | `        }` |
|      9 | 1845 | `      }else if( c==matchSet ){` |
|    ! 0 | 1846 | `	  if( (esc==0) \|\| (matchSet<0x80) ) return 0;` |
|    ! 0 | 1847 | `	  while( *zString && patternCompare(&zPattern[-1],zString,esc,noCase)==0 ){` |
|    ! 0 | 1848 | `          SQLITE_SKIP_UTF8(zString);` |
|    ! 0 | 1849 | `        }` |
|    ! 0 | 1850 | `        return *zString!=0;` |
|      - | 1851 | `      }` |
|     11 | 1852 | `      while( (c2 = PH7_Utf8Read(zString,0,&zString))!=0 ){` |
|     11 | 1853 | `        if( noCase ){` |
|      3 | 1854 | `          GlogUpperToLower(c2);` |
|      3 | 1855 | `          GlogUpperToLower(c);` |
|     11 | 1856 | `          while( c2 != 0 && c2 != c ){` |
|      9 | 1857 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      9 | 1858 | `            GlogUpperToLower(c2);` |
|      1 | 1859 | `          }` |
|      2 | 1860 | `        }else{` |
|     47 | 1861 | `          while( c2 != 0 && c2 != c ){` |
|     39 | 1862 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      1 | 1863 | `          }` |
|      - | 1864 | `        }` |
|     11 | 1865 | `        if( c2==0 ) return 0;` |
|      9 | 1866 | `		if( patternCompare(zPattern,zString,esc,noCase) ) return 1;` |
|      1 | 1867 | `      }` |
|    ! 0 | 1868 | `      return 0;` |
|     35 | 1869 | `    }else if( !prevEscape && c==matchOne ){` |
|    ! 0 | 1870 | `      if( PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1871 | `        return 0;` |
|    ! 0 | 1872 | `      }` |
|     35 | 1873 | `    }else if( c==matchSet ){` |
|    ! 0 | 1874 | `      int prior_c = 0;` |
|    ! 0 | 1875 | `      if( esc == 0 ) return 0;` |
|    ! 0 | 1876 | `      seen = 0;` |
|    ! 0 | 1877 | `      invert = 0;` |
|    ! 0 | 1878 | `      c = PH7_Utf8Read(zString, 0, &zString);` |
|    ! 0 | 1879 | `      if( c==0 ) return 0;` |
|    ! 0 | 1880 | `      c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1881 | `      if( c2=='^' ){` |
|    ! 0 | 1882 | `        invert = 1;` |
|    ! 0 | 1883 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1884 | `      }` |
|    ! 0 | 1885 | `      if( c2==']' ){` |
|    ! 0 | 1886 | `        if( c==']' ) seen = 1;` |
|    ! 0 | 1887 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1888 | `      }` |
|    ! 0 | 1889 | `      while( c2 && c2!=']' ){` |
|    ! 0 | 1890 | `        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){` |
|    ! 0 | 1891 | `          c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1892 | `          if( c>=prior_c && c<=c2 ) seen = 1;` |
|    ! 0 | 1893 | `          prior_c = 0;` |
|    ! 0 | 1894 | `        }else{` |
|    ! 0 | 1895 | `          if( c==c2 ){` |
|    ! 0 | 1896 | `            seen = 1;` |
|    ! 0 | 1897 | `          }` |
|    ! 0 | 1898 | `          prior_c = c2;` |
|      - | 1899 | `        }` |
|    ! 0 | 1900 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1901 | `      }` |
|    ! 0 | 1902 | `      if( c2==0 \|\| (seen ^ invert)==0 ){` |
|    ! 0 | 1903 | `        return 0;` |
|    ! 0 | 1904 | `      }` |
|     35 | 1905 | `    }else if( esc==c && !prevEscape ){` |
|    ! 0 | 1906 | `      prevEscape = 1;` |
|    ! 0 | 1907 | `    }else{` |
|     35 | 1908 | `      c2 = PH7_Utf8Read(zString, 0, &zString);` |
|     35 | 1909 | `      if( noCase ){` |
|      7 | 1910 | `        GlogUpperToLower(c);` |
|      7 | 1911 | `        GlogUpperToLower(c2);` |
|      3 | 1912 | `      }` |
|     35 | 1913 | `      if( c!=c2 ){` |
|      5 | 1914 | `        return 0;` |
|      - | 1915 | `      }` |
|     31 | 1916 | `      prevEscape = 0;` |
|      - | 1917 | `    }` |
|      1 | 1918 | `  }` |
|      9 | 1919 | `  return *zString==0;` |
|     11 | 1920 | `}` |
|      - | 1921 | `/* SPDX-SnippetEnd */` |
|      - | 1922 | `/*` |
|      - | 1923 | ` * Wrapper around patternCompare() defined above.` |
|      - | 1924 | ` * See block comment above for more information.` |
|      - | 1925 | ` */` |
|     12 | 1926 | `static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,int CaseCompare)` |
|      1 | 1927 | `{` |
|      - | 1928 | `	int rc;` |
|     13 | 1929 | `	if( iEsc < 0 ){` |
|    ! 0 | 1930 | `		iEsc = '\\';` |
|    ! 0 | 1931 | `	}` |
|     13 | 1932 | `	rc = patternCompare(zPattern,zString,iEsc,CaseCompare);` |
|     13 | 1933 | `	return rc;` |
|      1 | 1934 | `}` |
|      - | 1935 | `/*` |
|      - | 1936 | ` * bool fnmatch(string $pattern,string $string[,int $flags = 0 ])` |
|      - | 1937 | ` *  Match filename against a pattern.` |
|      - | 1938 | ` * Parameters` |
|      - | 1939 | ` *  $pattern` |
|      - | 1940 | ` *   The shell wildcard pattern.` |
|      - | 1941 | ` * $string` |
|      - | 1942 | ` *  The tested string.` |
|      - | 1943 | ` * $flags` |
|      - | 1944 | ` *   A list of possible flags:` |
|      - | 1945 | ` *    FNM_NOESCAPE 	Disable backslash escaping.` |
|      - | 1946 | ` *    FNM_PATHNAME 	Slash in string only matches slash in the given pattern.` |
|      - | 1947 | ` *    FNM_PERIOD 	Leading period in string must be exactly matched by period in the given pattern.` |
|      - | 1948 | ` *    FNM_CASEFOLD 	Caseless match.` |
|      - | 1949 | ` * Return` |
|      - | 1950 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 1951 | ` */` |
|      8 | 1952 | `static int PH7_builtin_fnmatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1953 | `{` |
|      - | 1954 | `	const char *zString,*zPattern;` |
|      9 | 1955 | `	int iEsc = '\\';` |
|      9 | 1956 | `	int noCase = 0;` |
|      - | 1957 | `	int rc;` |
|      9 | 1958 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 1959 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1960 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1961 | `		return PH7_OK;` |
|      - | 1962 | `	}` |
|      - | 1963 | `	/* Extract the pattern and the string */` |
|      9 | 1964 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|      9 | 1965 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 1966 | `	/* Extract the flags if avaialble */` |
|      9 | 1967 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      7 | 1968 | `		rc = ph7_value_to_int(apArg[2]);` |
|      7 | 1969 | `		if( rc & 0x01 /*FNM_NOESCAPE*/){` |
|    ! 0 | 1970 | `			iEsc = 0;` |
|    ! 0 | 1971 | `		}` |
|      7 | 1972 | `		if( rc & 0x08 /*FNM_CASEFOLD*/){` |
|      3 | 1973 | `			noCase = 1;` |
|      1 | 1974 | `		}` |
|      3 | 1975 | `	}` |
|      - | 1976 | `	/* Go globbing */` |
|      9 | 1977 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,noCase);` |
|      - | 1978 | `	/* Globbing result */` |
|      9 | 1979 | `	ph7_result_bool(pCtx,rc);` |
|      9 | 1980 | `	return PH7_OK;` |
|      5 | 1981 | `}` |
|      - | 1982 | `/*` |
|      - | 1983 | ` * bool strglob(string $pattern,string $string)` |
|      - | 1984 | ` *  Match string against a pattern.` |
|      - | 1985 | ` * Parameters` |
|      - | 1986 | ` *  $pattern` |
|      - | 1987 | ` *   The shell wildcard pattern.` |
|      - | 1988 | ` * $string` |
|      - | 1989 | ` *  The tested string.` |
|      - | 1990 | ` * Return` |
|      - | 1991 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 1992 | ` * Note that this a symisc eXtension.` |
|      - | 1993 | ` */` |
|      4 | 1994 | `static int PH7_builtin_strglob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1995 | `{` |
|      - | 1996 | `	const char *zString,*zPattern;` |
|      5 | 1997 | `	int iEsc = '\\';` |
|      - | 1998 | `	int rc;` |
|      5 | 1999 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2000 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2001 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2002 | `		return PH7_OK;` |
|      - | 2003 | `	}` |
|      - | 2004 | `	/* Extract the pattern and the string */` |
|      5 | 2005 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|      5 | 2006 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2007 | `	/* Go globbing */` |
|      5 | 2008 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0);` |
|      - | 2009 | `	/* Globbing result */` |
|      5 | 2010 | `	ph7_result_bool(pCtx,rc);` |
|      5 | 2011 | `	return PH7_OK;` |
|      3 | 2012 | `}` |
|      - | 2013 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2014 | `/*` |
|      - | 2015 | ` * bool link(string $target,string $link)` |
|      - | 2016 |  |
|      - | 2017 | ` *  Create a hard link.` |
|      - | 2018 | ` * Parameters` |
|      - | 2019 | ` *  $target` |
|      - | 2020 | ` *   Target of the link.` |
|      - | 2021 | ` *  $link` |
|      - | 2022 | ` *   The link name.` |
|      - | 2023 | ` * Return` |
|      - | 2024 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2025 | ` */` |
|      2 | 2026 | `static int PH7_vfs_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2027 | `{` |
|      - | 2028 | `	const char *zTarget,*zLink;` |
|      - | 2029 | `	ph7_vfs *pVfs;` |
|      - | 2030 | `	int rc;` |
|      2 | 2031 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2032 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2033 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2034 | `		return PH7_OK;` |
|      - | 2035 | `	}` |
|      - | 2036 | `	/* Point to the underlying vfs */` |
|      2 | 2037 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 2038 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2039 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2040 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2041 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2042 | `			ph7_function_name(pCtx)` |
|      - | 2043 | `			);` |
|    ! 0 | 2044 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2045 | `		return PH7_OK;` |
|      - | 2046 | `	}` |
|      - | 2047 | `	/* Extract the given arguments */` |
|      2 | 2048 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      2 | 2049 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2050 | `	/* Perform the requested operation */` |
|      2 | 2051 | `	rc = pVfs->xLink(zTarget,zLink,0/*Not a symbolic link */);` |
|      - | 2052 | `	/* IO result */` |
|      2 | 2053 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      2 | 2054 | `	return PH7_OK;` |
|      1 | 2055 | `}` |
|      - | 2056 | `/*` |
|      - | 2057 | ` * bool symlink(string $target,string $link)` |
|      - | 2058 | ` *  Creates a symbolic link.` |
|      - | 2059 | ` * Parameters` |
|      - | 2060 | ` *  $target` |
|      - | 2061 | ` *   Target of the link.` |
|      - | 2062 | ` *  $link` |
|      - | 2063 | ` *   The link name.` |
|      - | 2064 | ` * Return` |
|      - | 2065 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2066 | ` */` |
|      6 | 2067 | `static int PH7_vfs_symlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2068 | `{` |
|      - | 2069 | `	const char *zTarget,*zLink;` |
|      - | 2070 | `	ph7_vfs *pVfs;` |
|      - | 2071 | `	int rc;` |
|      6 | 2072 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2073 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2074 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2075 | `		return PH7_OK;` |
|      - | 2076 | `	}` |
|      - | 2077 | `	/* Point to the underlying vfs */` |
|      6 | 2078 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      6 | 2079 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2080 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2081 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2082 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2083 | `			ph7_function_name(pCtx)` |
|      - | 2084 | `			);` |
|    ! 0 | 2085 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2086 | `		return PH7_OK;` |
|      - | 2087 | `	}` |
|      - | 2088 | `	/* Extract the given arguments */` |
|      6 | 2089 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      6 | 2090 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2091 | `	/* Perform the requested operation */` |
|      6 | 2092 | `	rc = pVfs->xLink(zTarget,zLink,1/*A symbolic link */);` |
|      - | 2093 | `	/* IO result */` |
|      6 | 2094 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      6 | 2095 | `	return PH7_OK;` |
|      3 | 2096 | `}` |
|      - | 2097 | `/*` |
|      - | 2098 | ` * int umask([ int $mask ])` |
|      - | 2099 | ` *  Changes the current umask.` |
|      - | 2100 | ` * Parameters` |
|      - | 2101 | ` *  $mask` |
|      - | 2102 | ` *   The new umask.` |
|      - | 2103 | ` * Return` |
|      - | 2104 | ` *  umask() without arguments simply returns the current umask.` |
|      - | 2105 | ` *  Otherwise the old umask is returned.` |
|      - | 2106 | ` */` |
|      8 | 2107 | `static int PH7_vfs_umask(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2108 | `{` |
|      - | 2109 | `	int iOld,iNew;` |
|      - | 2110 | `	ph7_vfs *pVfs;` |
|      - | 2111 | `	/* Point to the underlying vfs */` |
|      8 | 2112 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      8 | 2113 | `	if( pVfs == 0 \|\| pVfs->xUmask == 0 ){` |
|      - | 2114 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2115 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2116 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2117 | `			ph7_function_name(pCtx)` |
|      - | 2118 | `			);` |
|    ! 0 | 2119 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2120 | `		return PH7_OK;` |
|      - | 2121 | `	}` |
|      8 | 2122 | `	iNew = 0;` |
|      8 | 2123 | `	if( nArg > 0 ){` |
|      4 | 2124 | `		iNew = ph7_value_to_int(apArg[0]);` |
|      2 | 2125 | `	}` |
|      - | 2126 | `	/* Perform the requested operation */` |
|      8 | 2127 | `	iOld = pVfs->xUmask(iNew);` |
|      - | 2128 | `	/* Old mask */` |
|      8 | 2129 | `	ph7_result_int(pCtx,iOld);` |
|      8 | 2130 | `	return PH7_OK;` |
|      4 | 2131 | `}` |
|      - | 2132 | `/*` |
|      - | 2133 | ` * string sys_get_temp_dir()` |
|      - | 2134 | ` *  Returns directory path used for temporary files.` |
|      - | 2135 | ` * Parameters` |
|      - | 2136 | ` *  None` |
|      - | 2137 | ` * Return` |
|      - | 2138 | ` *  Returns the path of the temporary directory.` |
|      - | 2139 | ` */` |
|    210 | 2140 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2141 | `{` |
|      - | 2142 | `	ph7_vfs *pVfs;` |
|      - | 2143 | `	/* Set the empty string as the default return value */` |
|    214 | 2144 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2145 | `	/* Point to the underlying vfs */` |
|    214 | 2146 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    214 | 2147 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
|    ! 0 | 2148 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2149 | `		SXUNUSED(apArg);` |
|      - | 2150 | `		/* IO routine not implemented,return "" */` |
|    ! 0 | 2151 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2152 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2153 | `			ph7_function_name(pCtx)` |
|      - | 2154 | `			);` |
|    ! 0 | 2155 | `		return PH7_OK;` |
|      - | 2156 | `	}` |
|      - | 2157 | `	/* Perform the requested operation */` |
|    214 | 2158 | `	pVfs->xTempDir(pCtx);` |
|    214 | 2159 | `	return PH7_OK;` |
|    109 | 2160 | `}` |
|      - | 2161 | `/*` |
|      - | 2162 | ` * string get_current_user()` |
|      - | 2163 | ` *  Returns the name of the current working user.` |
|      - | 2164 | ` * Parameters` |
|      - | 2165 | ` *  None` |
|      - | 2166 | ` * Return` |
|      - | 2167 | ` *  Returns the name of the current working user.` |
|      - | 2168 | ` */` |
|      2 | 2169 | `static int PH7_vfs_get_current_user(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2170 | `{` |
|      - | 2171 | `	ph7_vfs *pVfs;` |
|      - | 2172 | `	/* Point to the underlying vfs */` |
|      3 | 2173 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2174 | `	if( pVfs == 0 \|\| pVfs->xUsername == 0 ){` |
|    ! 0 | 2175 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2176 | `		SXUNUSED(apArg);` |
|      - | 2177 | `		/* IO routine not implemented */` |
|    ! 0 | 2178 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2179 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2180 | `			ph7_function_name(pCtx)` |
|      - | 2181 | `			);` |
|      - | 2182 | `		/* Set a dummy username */` |
|    ! 0 | 2183 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 2184 | `		return PH7_OK;` |
|      - | 2185 | `	}` |
|      - | 2186 | `	/* Perform the requested operation */` |
|      3 | 2187 | `	pVfs->xUsername(pCtx);` |
|      3 | 2188 | `	return PH7_OK;` |
|      2 | 2189 | `}` |
|      - | 2190 | `/*` |
|      - | 2191 | ` * int64 getmypid()` |
|      - | 2192 | ` *  Gets process ID.` |
|      - | 2193 | ` * Parameters` |
|      - | 2194 | ` *  None` |
|      - | 2195 | ` * Return` |
|      - | 2196 | ` *  Returns the process ID.` |
|      - | 2197 | ` */` |
|     66 | 2198 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2199 | `{` |
|      - | 2200 | `	ph7_int64 nProcessId;` |
|      - | 2201 | `	ph7_vfs *pVfs;` |
|      - | 2202 | `	/* Point to the underlying vfs */` |
|     68 | 2203 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     68 | 2204 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
|    ! 0 | 2205 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2206 | `		SXUNUSED(apArg);` |
|      - | 2207 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2208 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2209 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2210 | `			ph7_function_name(pCtx)` |
|      - | 2211 | `			);` |
|    ! 0 | 2212 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2213 | `		return PH7_OK;` |
|      - | 2214 | `	}` |
|      - | 2215 | `	/* Perform the requested operation */` |
|     68 | 2216 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 2217 | `	/* Set the result */` |
|     68 | 2218 | `	ph7_result_int64(pCtx,nProcessId);` |
|     68 | 2219 | `	return PH7_OK;` |
|     35 | 2220 | `}` |
|      - | 2221 | `/*` |
|      - | 2222 | ` * int getmyuid()` |
|      - | 2223 | ` *  Get user ID.` |
|      - | 2224 | ` * Parameters` |
|      - | 2225 | ` *  None` |
|      - | 2226 | ` * Return` |
|      - | 2227 | ` *  Returns the user ID.` |
|      - | 2228 | ` */` |
|      2 | 2229 | `static int PH7_vfs_getmyuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2230 | `{` |
|      - | 2231 | `	ph7_vfs *pVfs;` |
|      - | 2232 | `	int nUid;` |
|      - | 2233 | `	/* Point to the underlying vfs */` |
|      2 | 2234 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 2235 | `	if( pVfs == 0 \|\| pVfs->xUid == 0 ){` |
|    ! 0 | 2236 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2237 | `		SXUNUSED(apArg);` |
|      - | 2238 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2239 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2240 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2241 | `			ph7_function_name(pCtx)` |
|      - | 2242 | `			);` |
|    ! 0 | 2243 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2244 | `		return PH7_OK;` |
|      - | 2245 | `	}` |
|      - | 2246 | `	/* Perform the requested operation */` |
|      2 | 2247 | `	nUid = pVfs->xUid();` |
|      - | 2248 | `	/* Set the result */` |
|      2 | 2249 | `	ph7_result_int(pCtx,nUid);` |
|      2 | 2250 | `	return PH7_OK;` |
|      1 | 2251 | `}` |
|      - | 2252 | `/*` |
|      - | 2253 | ` * int getmygid()` |
|      - | 2254 | ` *  Get group ID.` |
|      - | 2255 | ` * Parameters` |
|      - | 2256 | ` *  None` |
|      - | 2257 | ` * Return` |
|      - | 2258 | ` *  Returns the group ID.` |
|      - | 2259 | ` */` |
|      2 | 2260 | `static int PH7_vfs_getmygid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2261 | `{` |
|      - | 2262 | `	ph7_vfs *pVfs;` |
|      - | 2263 | `	int nGid;` |
|      - | 2264 | `	/* Point to the underlying vfs */` |
|      2 | 2265 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 2266 | `	if( pVfs == 0 \|\| pVfs->xGid == 0 ){` |
|    ! 0 | 2267 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2268 | `		SXUNUSED(apArg);` |
|      - | 2269 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2270 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2271 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2272 | `			ph7_function_name(pCtx)` |
|      - | 2273 | `			);` |
|    ! 0 | 2274 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2275 | `		return PH7_OK;` |
|      - | 2276 | `	}` |
|      - | 2277 | `	/* Perform the requested operation */` |
|      2 | 2278 | `	nGid = pVfs->xGid();` |
|      - | 2279 | `	/* Set the result */` |
|      2 | 2280 | `	ph7_result_int(pCtx,nGid);` |
|      2 | 2281 | `	return PH7_OK;` |
|      1 | 2282 | `}` |
|      - | 2283 | `#ifdef __WINNT__` |
|      - | 2284 | `#include <Windows.h>` |
|      - | 2285 | `#elif defined(__UNIXES__)` |
|      - | 2286 | `#include <sys/utsname.h>` |
|      - | 2287 | `#endif` |
|      - | 2288 | `/*` |
|      - | 2289 | ` * string php_uname([ string $mode = "a" ])` |
|      - | 2290 | ` *  Returns information about the host operating system.` |
|      - | 2291 | ` * Parameters` |
|      - | 2292 | ` *  $mode` |
|      - | 2293 | ` *   mode is a single character that defines what information is returned:` |
|      - | 2294 | ` *    'a': This is the default. Contains all modes in the sequence "s n r v m".` |
|      - | 2295 | ` *    's': Operating system name. eg. FreeBSD.` |
|      - | 2296 | ` *    'n': Host name. eg. localhost.example.com.` |
|      - | 2297 | ` *    'r': Release name. eg. 5.1.2-RELEASE.` |
|      - | 2298 | ` *    'v': Version information. Varies a lot between operating systems.` |
|      - | 2299 | ` *    'm': Machine type. eg. i386.` |
|      - | 2300 | ` * Return` |
|      - | 2301 | ` *  OS description as a string.` |
|      - | 2302 | ` */` |
|      4 | 2303 | `static int PH7_vfs_ph7_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2304 | `{` |
|      - | 2305 | `#if defined(__WINNT__)` |
|      1 | 2306 | `	const char *zName = "Microsoft Windows";` |
|      - | 2307 | `	OSVERSIONINFOW sVer;` |
|      - | 2308 | `#elif defined(__UNIXES__)` |
|      - | 2309 | `	struct utsname sName;` |
|      - | 2310 | `#endif` |
|      5 | 2311 | `	const char *zMode = "a";` |
|      5 | 2312 | `	if( nArg > 0 && ph7_value_is_string(apArg[0]) ){` |
|      - | 2313 | `		/* Extract the desired mode */` |
|    ! 0 | 2314 | `		zMode = ph7_value_to_string(apArg[0],0);` |
|    ! 0 | 2315 | `	}` |
|      - | 2316 | `#if defined(__WINNT__)` |
|      1 | 2317 | `	sVer.dwOSVersionInfoSize = sizeof(sVer);` |
|      - | 2318 | `	/* GetVersionExW is deprecated in modern MSVC. Suppress deprecation for this call. */` |
|      - | 2319 | `#if defined(_MSC_VER)` |
|      - | 2320 | `#pragma warning(push)` |
|      - | 2321 | `#pragma warning(disable:4996)` |
|      - | 2322 | `#endif` |
|      1 | 2323 | `	if( TRUE != GetVersionExW(&sVer)){` |
|      - | 2324 | `#if defined(_MSC_VER)` |
|      - | 2325 | `#pragma warning(pop)` |
|      - | 2326 | `#endif` |
|    ! 0 | 2327 | `		ph7_result_string(pCtx,zName,-1);` |
|    ! 0 | 2328 | `		return PH7_OK;` |
|      - | 2329 | `	}` |
|      1 | 2330 | `	if( sVer.dwPlatformId == VER_PLATFORM_WIN32_NT ){` |
|      1 | 2331 | `		if( sVer.dwMajorVersion <= 4 ){` |
|    ! 0 | 2332 | `			zName = "Microsoft Windows NT";` |
|      1 | 2333 | `		}else if( sVer.dwMajorVersion == 5 ){` |
|    ! 0 | 2334 | `			switch(sVer.dwMinorVersion){` |
|    ! 0 | 2335 | `				case 0:	zName = "Microsoft Windows 2000"; break;` |
|    ! 0 | 2336 | `				case 1: zName = "Microsoft Windows XP";   break;` |
|    ! 0 | 2337 | `				case 2: zName = "Microsoft Windows Server 2003"; break;` |
|      - | 2338 | `			}` |
|    ! 0 | 2339 | `		}else if( sVer.dwMajorVersion == 6){` |
|      1 | 2340 | `				switch(sVer.dwMinorVersion){` |
|    ! 0 | 2341 | `					case 0: zName = "Microsoft Windows Vista"; break;` |
|    ! 0 | 2342 | `					case 1: zName = "Microsoft Windows 7"; break;` |
|      1 | 2343 | `					case 2: zName = "Microsoft Windows Server 2008"; break;` |
|    ! 0 | 2344 | `					case 3: zName = "Microsoft Windows 8"; break;` |
|      - | 2345 | `					default: break;` |
|      - | 2346 | `				}` |
|      - | 2347 | `		}` |
|      - | 2348 | `	}` |
|      1 | 2349 | `	switch(zMode[0]){` |
|      - | 2350 | `	case 's':` |
|      - | 2351 | `		/* Operating system name */` |
|    ! 0 | 2352 | `		ph7_result_string(pCtx,zName,-1/* Compute length automatically*/);` |
|    ! 0 | 2353 | `		break;` |
|      - | 2354 | `	case 'n':` |
|      - | 2355 | `		/* Host name */` |
|    ! 0 | 2356 | `		ph7_result_string(pCtx,"localhost",(int)sizeof("localhost")-1);` |
|    ! 0 | 2357 | `		break;` |
|      - | 2358 | `	case 'r':` |
|      - | 2359 | `	case 'v':` |
|      - | 2360 | `		/* Version information. */` |
|    ! 0 | 2361 | `		ph7_result_string_format(pCtx,"%u.%u build %u",` |
|      - | 2362 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2363 | `			);` |
|    ! 0 | 2364 | `		break;` |
|      - | 2365 | `	case 'm':` |
|      - | 2366 | `		/* Machine name */` |
|    ! 0 | 2367 | `		ph7_result_string(pCtx,"x86",(int)sizeof("x86")-1);` |
|    ! 0 | 2368 | `		break;` |
|      - | 2369 | `	default:` |
|      1 | 2370 | `		ph7_result_string_format(pCtx,"%s localhost %u.%u build %u x86",` |
|      - | 2371 | `			zName,` |
|      - | 2372 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2373 | `			);` |
|      - | 2374 | `		break;` |
|      - | 2375 | `	}` |
|      - | 2376 | `#elif defined(__UNIXES__)` |
|      4 | 2377 | `	if( uname(&sName) != 0 ){` |
|    ! 0 | 2378 | `		ph7_result_string(pCtx,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 | 2379 | `		return PH7_OK;` |
|      - | 2380 | `	}` |
|      4 | 2381 | `	switch(zMode[0]){` |
|    ! 0 | 2382 | `	case 's':` |
|      - | 2383 | `		/* Operating system name */` |
|    ! 0 | 2384 | `		ph7_result_string(pCtx,sName.sysname,-1/* Compute length automatically*/);` |
|    ! 0 | 2385 | `		break;` |
|    ! 0 | 2386 | `	case 'n':` |
|      - | 2387 | `		/* Host name */` |
|    ! 0 | 2388 | `		ph7_result_string(pCtx,sName.nodename,-1/* Compute length automatically*/);` |
|    ! 0 | 2389 | `		break;` |
|    ! 0 | 2390 | `	case 'r':` |
|      - | 2391 | `		/* Release information */` |
|    ! 0 | 2392 | `		ph7_result_string(pCtx,sName.release,-1/* Compute length automatically*/);` |
|    ! 0 | 2393 | `		break;` |
|    ! 0 | 2394 | `	case 'v':` |
|      - | 2395 | `		/* Version information. */` |
|    ! 0 | 2396 | `		ph7_result_string(pCtx,sName.version,-1/* Compute length automatically*/);` |
|    ! 0 | 2397 | `		break;` |
|    ! 0 | 2398 | `	case 'm':` |
|      - | 2399 | `		/* Machine name */` |
|    ! 0 | 2400 | `		ph7_result_string(pCtx,sName.machine,-1/* Compute length automatically*/);` |
|    ! 0 | 2401 | `		break;` |
|      2 | 2402 | `	default:` |
|      6 | 2403 | `		ph7_result_string_format(pCtx,` |
|      - | 2404 | `			"%s %s %s %s %s",` |
|      2 | 2405 | `			sName.sysname,` |
|      2 | 2406 | `			sName.release,` |
|      2 | 2407 | `			sName.version,` |
|      2 | 2408 | `			sName.nodename,` |
|      2 | 2409 | `			sName.machine` |
|      - | 2410 | `			);` |
|      4 | 2411 | `		break;` |
|      - | 2412 | `	}` |
|      - | 2413 | `#else` |
|      - | 2414 | `	ph7_result_string(pCtx,"Unknown Operating System",(int)sizeof("Unknown Operating System")-1);` |
|      - | 2415 | `#endif` |
|      5 | 2416 | `	return PH7_OK;` |
|      3 | 2417 | `}` |
|      - | 2418 | `/*` |
|      - | 2419 | ` * Section:` |
|      - | 2420 | ` *    IO stream implementation.` |
|      - | 2421 | ` * Status:` |
|      - | 2422 | ` *    Stable.` |
|      - | 2423 | ` */` |
|      - | 2424 | `typedef struct io_private io_private;` |
|      - | 2425 | `struct io_private` |
|      - | 2426 | `{` |
|      - | 2427 | `	const ph7_io_stream *pStream; /* Underlying IO device */` |
|      - | 2428 | `	void *pHandle; /* IO handle */` |
|      - | 2429 | `	/* Unbuffered IO */` |
|      - | 2430 | `	SyBlob sBuffer; /* Working buffer */` |
|      - | 2431 | `	sxu32 nOfft;    /* Current read offset */` |
|      - | 2432 | `	sxu32 iMagic;   /* Sanity check to avoid misuse */` |
|      - | 2433 | `};` |
|      - | 2434 | `#define IO_PRIVATE_MAGIC 0xFEAC14` |
|      - | 2435 | `/* Stream-device predicates (devices defined later in this file) */` |
|      - | 2436 | `static int is_php_stream(const ph7_io_stream *pStream);` |
|      - | 2437 | `static int is_data_stream(const ph7_io_stream *pStream);` |
|      - | 2438 | `/* Make sure we are dealing with a valid io_private instance */` |
|      - | 2439 | `#define IO_PRIVATE_INVALID(IO) ( IO == 0 \|\| IO->iMagic != IO_PRIVATE_MAGIC )` |
|      - | 2440 | `/* Forward declaration */` |
|      - | 2441 | `static void ResetIOPrivate(io_private *pDev);` |
|      - | 2442 | `/*` |
|      - | 2443 | ` * Return the PHP resource-type name for a raw resource handle.` |
|      - | 2444 | ` * Every IO handle this VFS hands out (fopen/tmpfile/popen/opendir and the` |
|      - | 2445 | ` * STDIN/STDOUT/STDERR constants) is an io_private, which PHP reports as` |
|      - | 2446 | ` * "stream"; anything else is "Unknown". The magic probe mirrors the` |
|      - | 2447 | ` * IO_PRIVATE_INVALID check the rest of this file uses to validate handles.` |
|      - | 2448 | ` */` |
|      4 | 2449 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      1 | 2450 | `{` |
|      5 | 2451 | `	io_private *pDev = (io_private *)pResource;` |
|      5 | 2452 | `	if( !IO_PRIVATE_INVALID(pDev) ){` |
|      5 | 2453 | `		return "stream";` |
|      - | 2454 | `	}` |
|    ! 0 | 2455 | `	return "Unknown";` |
|      3 | 2456 | `}` |
|      - | 2457 | `/*` |
|      - | 2458 | ` * bool ftruncate(resource $handle,int64 $size)` |
|      - | 2459 | ` *  Truncates a file to a given length.` |
|      - | 2460 | ` * Parameters` |
|      - | 2461 | ` *  $handle` |
|      - | 2462 | ` *   The file pointer.` |
|      - | 2463 | ` *   Note:` |
|      - | 2464 | ` *    The handle must be open for writing.` |
|      - | 2465 | ` * $size` |
|      - | 2466 | ` *   The size to truncate to.` |
|      - | 2467 | ` * Return` |
|      - | 2468 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2469 | ` */` |
|      6 | 2470 | `static int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2471 | `{` |
|      - | 2472 | `	const ph7_io_stream *pStream;` |
|      - | 2473 | `	io_private *pDev;` |
|      - | 2474 | `	int rc;` |
|      7 | 2475 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2476 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2477 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2478 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2479 | `		return PH7_OK;` |
|      - | 2480 | `	}` |
|      - | 2481 | `	/* Extract our private data */` |
|      7 | 2482 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2483 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 2484 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2485 | `		/*Expecting an IO handle */` |
|    ! 0 | 2486 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2487 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2488 | `		return PH7_OK;` |
|      - | 2489 | `	}` |
|      - | 2490 | `	/* Point to the target IO stream device */` |
|      7 | 2491 | `	pStream = pDev->pStream;` |
|      7 | 2492 | `	if( pStream == 0  \|\| pStream->xTrunc == 0){` |
|    ! 0 | 2493 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2494 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2495 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2496 | `			);` |
|    ! 0 | 2497 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2498 | `		return PH7_OK;` |
|      - | 2499 | `	}` |
|      - | 2500 | `	/* Perform the requested operation */` |
|      7 | 2501 | `	rc = pStream->xTrunc(pDev->pHandle,ph7_value_to_int64(apArg[1]));` |
|      7 | 2502 | `	if( rc == PH7_OK ){` |
|      - | 2503 | `		/* Discard buffered data */` |
|      7 | 2504 | `		ResetIOPrivate(pDev);` |
|      3 | 2505 | `	}` |
|      - | 2506 | `	/* IO result */` |
|      7 | 2507 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      7 | 2508 | `	return PH7_OK;` |
|      4 | 2509 | `}` |
|      - | 2510 | `/*` |
|      - | 2511 | ` * int fseek(resource $handle,int $offset[,int $whence = SEEK_SET ])` |
|      - | 2512 | ` *  Seeks on a file pointer.` |
|      - | 2513 | ` * Parameters` |
|      - | 2514 | ` *  $handle` |
|      - | 2515 | ` *   A file system pointer resource that is typically created using fopen().` |
|      - | 2516 | ` * $offset` |
|      - | 2517 | ` *   The offset.` |
|      - | 2518 | ` *   To move to a position before the end-of-file, you need to pass a negative` |
|      - | 2519 | ` *   value in offset and set whence to SEEK_END.` |
|      - | 2520 | ` *   whence` |
|      - | 2521 | ` *   whence values are:` |
|      - | 2522 | ` *    SEEK_SET - Set position equal to offset bytes.` |
|      - | 2523 | ` *    SEEK_CUR - Set position to current location plus offset.` |
|      - | 2524 | ` *    SEEK_END - Set position to end-of-file plus offset.` |
|      - | 2525 | ` * Return` |
|      - | 2526 | ` *  0 on success,-1 on failure` |
|      - | 2527 | ` */` |
|      8 | 2528 | `static int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2529 | `{` |
|      - | 2530 | `	const ph7_io_stream *pStream;` |
|      - | 2531 | `	io_private *pDev;` |
|      - | 2532 | `	ph7_int64 iOfft;` |
|      - | 2533 | `	int whence;` |
|      - | 2534 | `	int rc;` |
|      9 | 2535 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2536 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2537 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2538 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2539 | `		return PH7_OK;` |
|      - | 2540 | `	}` |
|      - | 2541 | `	/* Extract our private data */` |
|      9 | 2542 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2543 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      9 | 2544 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2545 | `		/*Expecting an IO handle */` |
|    ! 0 | 2546 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2547 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2548 | `		return PH7_OK;` |
|      - | 2549 | `	}` |
|      - | 2550 | `	/* Point to the target IO stream device */` |
|      9 | 2551 | `	pStream = pDev->pStream;` |
|      9 | 2552 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2553 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2554 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2555 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2556 | `			);` |
|    ! 0 | 2557 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2558 | `		return PH7_OK;` |
|      - | 2559 | `	}` |
|      - | 2560 | `	/* Extract the offset */` |
|      9 | 2561 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|      9 | 2562 | `	whence = 0;/* SEEK_SET */` |
|      9 | 2563 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      3 | 2564 | `		whence = ph7_value_to_int(apArg[2]);` |
|      1 | 2565 | `	}` |
|      - | 2566 | `	/* Perform the requested operation */` |
|      9 | 2567 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|      9 | 2568 | `	if( rc == PH7_OK ){` |
|      - | 2569 | `		/* Ignore buffered data */` |
|      9 | 2570 | `		ResetIOPrivate(pDev);` |
|      4 | 2571 | `	}` |
|      - | 2572 | `	/* IO result */` |
|      9 | 2573 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|      9 | 2574 | `	return PH7_OK;` |
|      5 | 2575 | `}` |
|      - | 2576 | `/*` |
|      - | 2577 | ` * int64 ftell(resource $handle)` |
|      - | 2578 | ` *  Returns the current position of the file read/write pointer.` |
|      - | 2579 | ` * Parameters` |
|      - | 2580 | ` *  $handle` |
|      - | 2581 | ` *   The file pointer.` |
|      - | 2582 | ` * Return` |
|      - | 2583 | ` *  Returns the position of the file pointer referenced by handle` |
|      - | 2584 | ` *  as an integer; i.e., its offset into the file stream.` |
|      - | 2585 | ` *  FALSE is returned on failure.` |
|      - | 2586 | ` */` |
|     10 | 2587 | `static int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2588 | `{` |
|      - | 2589 | `	const ph7_io_stream *pStream;` |
|      - | 2590 | `	io_private *pDev;` |
|      - | 2591 | `	ph7_int64 iOfft;` |
|     11 | 2592 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2593 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2594 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2595 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2596 | `		return PH7_OK;` |
|      - | 2597 | `	}` |
|      - | 2598 | `	/* Extract our private data */` |
|     11 | 2599 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2600 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     11 | 2601 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2602 | `		/*Expecting an IO handle */` |
|    ! 0 | 2603 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2604 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2605 | `		return PH7_OK;` |
|      - | 2606 | `	}` |
|      - | 2607 | `	/* Point to the target IO stream device */` |
|     11 | 2608 | `	pStream = pDev->pStream;` |
|     11 | 2609 | `	if( pStream == 0  \|\| pStream->xTell == 0){` |
|    ! 0 | 2610 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2611 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2612 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2613 | `			);` |
|    ! 0 | 2614 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2615 | `		return PH7_OK;` |
|      - | 2616 | `	}` |
|      - | 2617 | `	/* Perform the requested operation */` |
|     11 | 2618 | `	iOfft = pStream->xTell(pDev->pHandle);` |
|      - | 2619 | `	/* IO result */` |
|     11 | 2620 | `	ph7_result_int64(pCtx,iOfft);` |
|     11 | 2621 | `	return PH7_OK;` |
|      6 | 2622 | `}` |
|      - | 2623 | `/*` |
|      - | 2624 | ` * bool rewind(resource $handle)` |
|      - | 2625 | ` *  Rewind the position of a file pointer.` |
|      - | 2626 | ` * Parameters` |
|      - | 2627 | ` *  $handle` |
|      - | 2628 | ` *   The file pointer.` |
|      - | 2629 | ` * Return` |
|      - | 2630 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2631 | ` */` |
|     14 | 2632 | `static int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2633 | `{` |
|      - | 2634 | `	const ph7_io_stream *pStream;` |
|      - | 2635 | `	io_private *pDev;` |
|      - | 2636 | `	int rc;` |
|     15 | 2637 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2638 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2639 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2640 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2641 | `		return PH7_OK;` |
|      - | 2642 | `	}` |
|      - | 2643 | `	/* Extract our private data */` |
|     15 | 2644 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2645 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     15 | 2646 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2647 | `		/*Expecting an IO handle */` |
|    ! 0 | 2648 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2649 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2650 | `		return PH7_OK;` |
|      - | 2651 | `	}` |
|      - | 2652 | `	/* Point to the target IO stream device */` |
|     15 | 2653 | `	pStream = pDev->pStream;` |
|     15 | 2654 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2655 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2656 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2657 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2658 | `			);` |
|    ! 0 | 2659 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2660 | `		return PH7_OK;` |
|      - | 2661 | `	}` |
|      - | 2662 | `	/* Perform the requested operation */` |
|     15 | 2663 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|     15 | 2664 | `	if( rc == PH7_OK ){` |
|      - | 2665 | `		/* Ignore buffered data */` |
|     15 | 2666 | `		ResetIOPrivate(pDev);` |
|      7 | 2667 | `	}` |
|      - | 2668 | `	/* IO result */` |
|     15 | 2669 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     15 | 2670 | `	return PH7_OK;` |
|      8 | 2671 | `}` |
|      - | 2672 | `/*` |
|      - | 2673 | ` * bool fflush(resource $handle)` |
|      - | 2674 | ` *  Flushes the output to a file.` |
|      - | 2675 | ` * Parameters` |
|      - | 2676 | ` *  $handle` |
|      - | 2677 | ` *   The file pointer.` |
|      - | 2678 | ` * Return` |
|      - | 2679 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2680 | ` */` |
|      2 | 2681 | `static int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2682 | `{` |
|      - | 2683 | `	const ph7_io_stream *pStream;` |
|      - | 2684 | `	io_private *pDev;` |
|      - | 2685 | `	int rc;` |
|      3 | 2686 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2687 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2688 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2689 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2690 | `		return PH7_OK;` |
|      - | 2691 | `	}` |
|      - | 2692 | `	/* Extract our private data */` |
|      3 | 2693 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2694 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 2695 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2696 | `		/*Expecting an IO handle */` |
|    ! 0 | 2697 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2698 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2699 | `		return PH7_OK;` |
|      - | 2700 | `	}` |
|      - | 2701 | `	/* Point to the target IO stream device */` |
|      3 | 2702 | `	pStream = pDev->pStream;` |
|      3 | 2703 | `	if( pStream == 0 \|\| pStream->xSync == 0){` |
|    ! 0 | 2704 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2705 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2706 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2707 | `			);` |
|    ! 0 | 2708 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2709 | `		return PH7_OK;` |
|      - | 2710 | `	}` |
|      - | 2711 | `	/* Perform the requested operation */` |
|      3 | 2712 | `	rc = pStream->xSync(pDev->pHandle);` |
|      - | 2713 | `	/* IO result */` |
|      3 | 2714 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 2715 | `	return PH7_OK;` |
|      2 | 2716 | `}` |
|      - | 2717 | `/*` |
|      - | 2718 | ` * bool feof(resource $handle)` |
|      - | 2719 | ` *  Tests for end-of-file on a file pointer.` |
|      - | 2720 | ` * Parameters` |
|      - | 2721 | ` *  $handle` |
|      - | 2722 | ` *   The file pointer.` |
|      - | 2723 | ` * Return` |
|      - | 2724 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|      - | 2725 | ` */` |
|  10654 | 2726 | `static int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2727 | `{` |
|      - | 2728 | `	const ph7_io_stream *pStream;` |
|      - | 2729 | `	io_private *pDev;` |
|      - | 2730 | `	int rc;` |
|  10659 | 2731 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2732 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 2733 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2734 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2735 | `		return PH7_OK;` |
|      - | 2736 | `	}` |
|      - | 2737 | `	/* Extract our private data */` |
|  10659 | 2738 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2739 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  10659 | 2740 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2741 | `		/*Expecting an IO handle */` |
|    ! 0 | 2742 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2743 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2744 | `		return PH7_OK;` |
|      - | 2745 | `	}` |
|      - | 2746 | `	/* Point to the target IO stream device */` |
|  10659 | 2747 | `	pStream = pDev->pStream;` |
|  10659 | 2748 | `	if( pStream == 0 ){` |
|    ! 0 | 2749 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2750 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2751 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2752 | `			);` |
|    ! 0 | 2753 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2754 | `		return PH7_OK;` |
|      - | 2755 | `	}` |
|  10659 | 2756 | `	rc = SXERR_EOF;` |
|      - | 2757 | `	/* Perform the requested operation */` |
|  10659 | 2758 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2759 | `		/* Data is available */` |
|   4911 | 2760 | `		rc = PH7_OK;` |
|   2458 | 2761 | `	}else{` |
|      - | 2762 | `		char zBuf[4096];` |
|      - | 2763 | `		ph7_int64 n;` |
|      - | 2764 | `		/* Perform a buffered read */` |
|   5753 | 2765 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|   5753 | 2766 | `		if( n > 0 ){` |
|      - | 2767 | `			/* Copy buffered data */` |
|   1817 | 2768 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|   1817 | 2769 | `			rc = PH7_OK;` |
|    906 | 2770 | `		}` |
|      - | 2771 | `	}` |
|      - | 2772 | `	/* EOF or not */` |
|  10659 | 2773 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|  10659 | 2774 | `	return PH7_OK;` |
|   5332 | 2775 | `}` |
|      - | 2776 | `/*` |
|      - | 2777 | ` * Read n bytes from the underlying IO stream device.` |
|      - | 2778 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|      - | 2779 | ` * [i.e: IO error ] or EOF.` |
|      - | 2780 | ` */` |
|     30 | 2781 | `static ph7_int64 StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      2 | 2782 | `{` |
|     32 | 2783 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|     32 | 2784 | `	char *zBuf = (char *)pBuf;` |
|      - | 2785 | `	ph7_int64 n,nRead;` |
|     32 | 2786 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     32 | 2787 | `	if( n > 0 ){` |
|    ! 0 | 2788 | `		if( n > nLen ){` |
|    ! 0 | 2789 | `			n = nLen;` |
|    ! 0 | 2790 | `		}` |
|      - | 2791 | `		/* Copy the buffered data */` |
|    ! 0 | 2792 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|      - | 2793 | `		/* Update the read offset */` |
|    ! 0 | 2794 | `		pDev->nOfft += (sxu32)n;` |
|    ! 0 | 2795 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2796 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|    ! 0 | 2797 | `			SyBlobReset(&pDev->sBuffer);` |
|    ! 0 | 2798 | `			pDev->nOfft = 0;` |
|    ! 0 | 2799 | `		}` |
|    ! 0 | 2800 | `		nLen -= n;` |
|    ! 0 | 2801 | `		if( nLen < 1 ){` |
|      - | 2802 | `			/* All done */` |
|    ! 0 | 2803 | `			return n;` |
|      - | 2804 | `		}` |
|      - | 2805 | `		/* Advance the cursor */` |
|    ! 0 | 2806 | `		zBuf += n;` |
|    ! 0 | 2807 | `	}` |
|      - | 2808 | `	/* Read without buffering */` |
|     32 | 2809 | `	nRead = pStream->xRead(pDev->pHandle,zBuf,nLen);` |
|     32 | 2810 | `	if( nRead > 0 ){` |
|     30 | 2811 | `		n += nRead;` |
|     17 | 2812 | `	}else if( n < 1 ){` |
|      - | 2813 | `		/* EOF or IO error */` |
|      3 | 2814 | `		return nRead;` |
|      - | 2815 | `	}` |
|     30 | 2816 | `	return n;` |
|     17 | 2817 | `}` |
|      - | 2818 | `/*` |
|      - | 2819 | ` * Extract a single line from the buffered input.` |
|      - | 2820 | ` */` |
|   6780 | 2821 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      5 | 2822 | `{` |
|      - | 2823 | `	const char *zIn,*zEnd,*zPtr;` |
|   6785 | 2824 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|   6785 | 2825 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|   6785 | 2826 | `	zPtr = zIn;` |
| 393650 | 2827 | `	while( zIn < zEnd ){` |
| 393550 | 2828 | `		if( zIn[0] == '\n' ){` |
|      - | 2829 | `			/* Line found */` |
|   6685 | 2830 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|   6685 | 2831 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|   6685 | 2832 | `			*pzLine = zPtr;` |
|   6685 | 2833 | `			return SXRET_OK;` |
|      - | 2834 | `		}` |
| 386870 | 2835 | `		zIn++;` |
|      5 | 2836 | `	}` |
|      - | 2837 | `	/* No line were found */` |
|    105 | 2838 | `	return SXERR_NOTFOUND;` |
|   3395 | 2839 | `}` |
|      - | 2840 | `/*` |
|      - | 2841 | ` * Read a single line from the underlying IO stream device.` |
|      - | 2842 | ` */` |
|   6784 | 2843 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      5 | 2844 | `{` |
|   6789 | 2845 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - | 2846 | `	char zBuf[8192];` |
|      - | 2847 | `	ph7_int64 n;` |
|      - | 2848 | `	sxi32 rc;` |
|   6789 | 2849 | `	n = 0;` |
|   6789 | 2850 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2851 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     67 | 2852 | `		SyBlobReset(&pDev->sBuffer);` |
|     67 | 2853 | `		pDev->nOfft = 0;` |
|     31 | 2854 | `	}` |
|   6789 | 2855 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2856 | `		/* Check if there is a line */` |
|   6727 | 2857 | `		rc = GetLine(pDev,&n,pzData);` |
|   6727 | 2858 | `		if( rc == SXRET_OK ){` |
|      - | 2859 | `			/* Got line,update the cursor  */` |
|   6631 | 2860 | `			pDev->nOfft += (sxu32)n;` |
|   6631 | 2861 | `			return n;` |
|      - | 2862 | `		}` |
|     48 | 2863 | `	}` |
|      - | 2864 | `	/* Perform the read operation until a new line is extracted or length` |
|      - | 2865 | `	 * limit is reached.` |
|      - | 2866 | `	 */` |
|     81 | 2867 | `	for(;;){` |
|    167 | 2868 | `		n = pStream->xRead(pDev->pHandle,zBuf, (nMaxLen > 0 && nMaxLen < (ph7_int64)sizeof(zBuf)) ? nMaxLen : (ph7_int64)sizeof(zBuf));` |
|    167 | 2869 | `		if( n < 1 ){` |
|      - | 2870 | `			/* EOF or IO error */` |
|    109 | 2871 | `			break;` |
|      - | 2872 | `		}` |
|      - | 2873 | `		/* Append the data just read */` |
|     61 | 2874 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - | 2875 | `		/* Try to extract a line */` |
|     61 | 2876 | `		rc = GetLine(pDev,&n,pzData);` |
|     61 | 2877 | `		if( rc == SXRET_OK ){` |
|      - | 2878 | `			/* Got one,return immediately */` |
|     57 | 2879 | `			pDev->nOfft += (sxu32)n;` |
|     57 | 2880 | `			return n;` |
|      - | 2881 | `		}` |
|      5 | 2882 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
|      - | 2883 | `			/* Read limit reached,return the available data */` |
|    ! 0 | 2884 | `			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    ! 0 | 2885 | `			n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 2886 | `			/* Reset the working buffer */` |
|    ! 0 | 2887 | `			SyBlobReset(&pDev->sBuffer);` |
|    ! 0 | 2888 | `			pDev->nOfft = 0;` |
|    ! 0 | 2889 | `			return n;` |
|      - | 2890 | `		}` |
|      1 | 2891 | `	}` |
|    109 | 2892 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2893 | `		/* Read limit reached,return the available data */` |
|    105 | 2894 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    105 | 2895 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 2896 | `		/* Reset the working buffer */` |
|    105 | 2897 | `		SyBlobReset(&pDev->sBuffer);` |
|    105 | 2898 | `		pDev->nOfft = 0;` |
|     50 | 2899 | `	}` |
|    109 | 2900 | `	return n;` |
|   3397 | 2901 | `}` |
|      - | 2902 | `/*` |
|      - | 2903 | ` * Open an IO stream handle.` |
|      - | 2904 | ` * Notes on stream:` |
|      - | 2905 | ` * According to the PHP reference manual.` |
|      - | 2906 | ` * In its simplest definition, a stream is a resource object which exhibits streamable behavior.` |
|      - | 2907 | ` * That is, it can be read from or written to in a linear fashion, and may be able to fseek()` |
|      - | 2908 | ` * to an arbitrary locations within the stream.` |
|      - | 2909 | ` * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.` |
|      - | 2910 | ` * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file` |
|      - | 2911 | ` * on a remote server.` |
|      - | 2912 | ` * A stream is referenced as: scheme://target` |
|      - | 2913 | ` *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...` |
|      - | 2914 | ` *   If no wrapper is specified, the function default is used (typically file://).` |
|      - | 2915 | ` *   target - Depends on the wrapper used. For filesystem related streams this is typically a path` |
|      - | 2916 | ` *  and filename of the desired file. For network related streams this is typically a hostname, often` |
|      - | 2917 | ` *  with a path appended.` |
|      - | 2918 | ` *` |
|      - | 2919 | ` * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.` |
|      - | 2920 | ` * Please refer to the official documentation for a full discussion.` |
|      - | 2921 | ` * This function return a handle on success. Otherwise null.` |
|      - | 2922 | ` */` |
|  30312 | 2923 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - | 2924 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      5 | 2925 | `{` |
|  30317 | 2926 | `	void *pHandle = 0; /* cc warning */` |
|      - | 2927 | `	SyString sFile;` |
|      - | 2928 | `	ph7_value sDummy;` |
|      - | 2929 | `	int rc;` |
|  30317 | 2930 | `	if( pStream == 0 ){` |
|      - | 2931 | `		/* No such stream device */` |
|    ! 0 | 2932 | `		return 0;` |
|      - | 2933 | `	}` |
|  30317 | 2934 | `	if( pResource == 0 && (is_php_stream(pStream) \|\| is_data_stream(pStream)) ){` |
|      - | 2935 | `		/* These devices reach the VM through pResource->pVm (their xOpen has` |
|      - | 2936 | `		 * no vm parameter); callers like file_get_contents pass no resource,` |
|      - | 2937 | `		 * so hand them a synthesized stack value carrying the VM — xOpen only` |
|      - | 2938 | `		 * reads it during the call. */` |
|      7 | 2939 | `		PH7_MemObjInit(pVm,&sDummy);` |
|      7 | 2940 | `		pResource = &sDummy;` |
|      3 | 2941 | `	}` |
|  30317 | 2942 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  30317 | 2943 | `	if( use_include ){` |
|   9798 | 2944 | `		if(	sFile.zString[0] == '/' \|\|` |
|      - | 2945 | `#ifdef __WINNT__` |
|      - | 2946 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - | 2947 | `#endif` |
|   9784 | 2948 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   9780 | 2949 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - | 2950 | `				/*  Open the file directly */` |
|     19 | 2951 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|     10 | 2952 | `		}else{` |
|      - | 2953 | `			SyString *pPath;` |
|      - | 2954 | `			SyBlob sWorker;` |
|      - | 2955 | `#ifdef __WINNT__` |
|      - | 2956 | `			static const int c = '\\';` |
|      - | 2957 | `#else` |
|      - | 2958 | `			static const int c = '/';` |
|      - | 2959 | `#endif` |
|      - | 2960 | `			/* Init the path builder working buffer */` |
|   9784 | 2961 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - | 2962 | `			/* Build a path from the set of include path */` |
|   9784 | 2963 | `			SySetResetCursor(&pVm->aPaths);` |
|   9784 | 2964 | `			rc = SXERR_IO;` |
|   9790 | 2965 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - | 2966 | `				/* Build full path */` |
|   9784 | 2967 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - | 2968 | `				/* Append null terminator */` |
|   9784 | 2969 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 2970 | `					continue;` |
|      - | 2971 | `				}` |
|      - | 2972 | `				/* Try to open the file */` |
|   9784 | 2973 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   9784 | 2974 | `				if( rc == PH7_OK ){` |
|   9778 | 2975 | `					if( bPushInclude ){` |
|      - | 2976 | `						/* Mark as included */` |
|   9778 | 2977 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   4887 | 2978 | `					}` |
|   9778 | 2979 | `					break;` |
|      - | 2980 | `				}` |
|      - | 2981 | `				/* Reset the working buffer */` |
|      8 | 2982 | `				SyBlobReset(&sWorker);` |
|      - | 2983 | `				/* Check the next path */` |
|      2 | 2984 | `			}` |
|   9784 | 2985 | `			SyBlobRelease(&sWorker);` |
|      - | 2986 | `		}` |
|   9802 | 2987 | `		if( rc == PH7_OK ){` |
|   9796 | 2988 | `			if( bPushInclude ){` |
|      - | 2989 | `				/* Mark as included */` |
|   9796 | 2990 | `				PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|   4896 | 2991 | `			}` |
|   4896 | 2992 | `		}` |
|   4903 | 2993 | `	}else{` |
|      - | 2994 | `		/* Open the URI direcly */` |
|  20519 | 2995 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - | 2996 | `	}` |
|  30317 | 2997 | `	if( rc != PH7_OK ){` |
|      - | 2998 | `		/* IO error */` |
|     15 | 2999 | `		return 0;` |
|      - | 3000 | `	}` |
|      - | 3001 | `	/* Return the file handle */` |
|  30305 | 3002 | `	return pHandle;` |
|  15161 | 3003 | `}` |
|      - | 3004 | `/*` |
|      - | 3005 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - | 3006 | ` * Store the read data in the given BLOB (last argument).` |
|      - | 3007 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - | 3008 | ` */` |
|   9786 | 3009 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      4 | 3010 | `{` |
|      - | 3011 | `	ph7_int64 nRead;` |
|      - | 3012 | `	char zBuf[8192]; /* 8K */` |
|      - | 3013 | `	int rc;` |
|      - | 3014 | `	/* Perform the requested operation */` |
|   9786 | 3015 | `	for(;;){` |
|  19576 | 3016 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  19576 | 3017 | `		if( nRead < 1 ){` |
|      - | 3018 | `			/* EOF or IO error */` |
|   9790 | 3019 | `			break;` |
|      - | 3020 | `		}` |
|      - | 3021 | `		/* Append contents */` |
|   9790 | 3022 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   9790 | 3023 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3024 | `			break;` |
|      - | 3025 | `		}` |
|      4 | 3026 | `	}` |
|   9790 | 3027 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      4 | 3028 | `}` |
|      - | 3029 | `/*` |
|      - | 3030 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - | 3031 | ` */` |
|  30386 | 3032 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      5 | 3033 | `{` |
|  30391 | 3034 | `	if( pStream->xClose ){` |
|  30391 | 3035 | `		pStream->xClose(pHandle);` |
|  15193 | 3036 | `	}` |
|  30391 | 3037 | `}` |
|      - | 3038 | `/*` |
|      - | 3039 | ` * string fgetc(resource $handle)` |
|      - | 3040 | ` *  Gets a character from the given file pointer.` |
|      - | 3041 | ` * Parameters` |
|      - | 3042 | ` *  $handle` |
|      - | 3043 | ` *   The file pointer.` |
|      - | 3044 | ` * Return` |
|      - | 3045 | ` *  Returns a string containing a single character read from the file` |
|      - | 3046 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|      - | 3047 | ` * WARNING` |
|      - | 3048 | ` *  This operation is extremely slow.Avoid using it.` |
|      - | 3049 | ` */` |
|      4 | 3050 | `static int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3051 | `{` |
|      - | 3052 | `	const ph7_io_stream *pStream;` |
|      - | 3053 | `	io_private *pDev;` |
|      - | 3054 | `	int c,n;` |
|      5 | 3055 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3056 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3057 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3058 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3059 | `		return PH7_OK;` |
|      - | 3060 | `	}` |
|      - | 3061 | `	/* Extract our private data */` |
|      5 | 3062 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3063 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 3064 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3065 | `		/*Expecting an IO handle */` |
|    ! 0 | 3066 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3067 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3068 | `		return PH7_OK;` |
|      - | 3069 | `	}` |
|      - | 3070 | `	/* Point to the target IO stream device */` |
|      5 | 3071 | `	pStream = pDev->pStream;` |
|      5 | 3072 | `	if( pStream == 0  ){` |
|    ! 0 | 3073 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3074 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3075 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3076 | `			);` |
|    ! 0 | 3077 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3078 | `		return PH7_OK;` |
|      - | 3079 | `	}` |
|      - | 3080 | `	/* Perform the requested operation */` |
|      5 | 3081 | `	n = (int)StreamRead(pDev,(void *)&c,sizeof(char));` |
|      - | 3082 | `	/* IO result */` |
|      5 | 3083 | `	if( n < 1 ){` |
|      - | 3084 | `		/* EOF or error,return FALSE */` |
|    ! 0 | 3085 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3086 | `	}else{` |
|      - | 3087 | `		/* Return the string holding the character */` |
|      5 | 3088 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|      - | 3089 | `	}` |
|      5 | 3090 | `	return PH7_OK;` |
|      3 | 3091 | `}` |
|      - | 3092 | `/*` |
|      - | 3093 | ` * string fgets(resource $handle[,int64 $length ])` |
|      - | 3094 | ` *  Gets line from file pointer.` |
|      - | 3095 | ` * Parameters` |
|      - | 3096 | ` *  $handle` |
|      - | 3097 | ` *   The file pointer.` |
|      - | 3098 | ` * $length` |
|      - | 3099 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3100 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3101 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3102 | ` *  the end of the line.` |
|      - | 3103 | ` * Return` |
|      - | 3104 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3105 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3106 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3107 | ` */` |
|   6774 | 3108 | `static int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3109 | `{` |
|      - | 3110 | `	const ph7_io_stream *pStream;` |
|      - | 3111 | `	const char *zLine;` |
|      - | 3112 | `	io_private *pDev;` |
|      - | 3113 | `	ph7_int64 n,nLen;` |
|   6779 | 3114 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3115 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3116 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3117 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3118 | `		return PH7_OK;` |
|      - | 3119 | `	}` |
|      - | 3120 | `	/* Extract our private data */` |
|   6779 | 3121 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3122 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   6779 | 3123 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3124 | `		/*Expecting an IO handle */` |
|    ! 0 | 3125 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3126 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3127 | `		return PH7_OK;` |
|      - | 3128 | `	}` |
|      - | 3129 | `	/* Point to the target IO stream device */` |
|   6779 | 3130 | `	pStream = pDev->pStream;` |
|   6779 | 3131 | `	if( pStream == 0  ){` |
|    ! 0 | 3132 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3133 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3134 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3135 | `			);` |
|    ! 0 | 3136 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3137 | `		return PH7_OK;` |
|      - | 3138 | `	}` |
|   6779 | 3139 | `	nLen = -1;` |
|   6779 | 3140 | `	if( nArg > 1 ){` |
|      - | 3141 | `		/* Maximum data to read */` |
|    ! 0 | 3142 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3143 | `	}` |
|      - | 3144 | `	/* Perform the requested operation */` |
|   6779 | 3145 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|   6779 | 3146 | `	if( n < 1 ){` |
|      - | 3147 | `		/* EOF or IO error,return FALSE */` |
|      7 | 3148 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3149 | `	}else{` |
|      - | 3150 | `		/* Return the freshly extracted line */` |
|   6777 | 3151 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - | 3152 | `	}` |
|   6779 | 3153 | `	return PH7_OK;` |
|   3392 | 3154 | `}` |
|      - | 3155 | `/*` |
|      - | 3156 | ` * string fread(resource $handle,int64 $length)` |
|      - | 3157 | ` *  Binary-safe file read.` |
|      - | 3158 | ` * Parameters` |
|      - | 3159 | ` *  $handle` |
|      - | 3160 | ` *   The file pointer.` |
|      - | 3161 | ` * $length` |
|      - | 3162 | ` *  Up to length number of bytes read.` |
|      - | 3163 | ` * Return` |
|      - | 3164 | ` *  The data readen on success or FALSE on failure.` |
|      - | 3165 | ` */` |
|     22 | 3166 | `static int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3167 | `{` |
|      - | 3168 | `	const ph7_io_stream *pStream;` |
|      - | 3169 | `	io_private *pDev;` |
|      - | 3170 | `	ph7_int64 nRead;` |
|      - | 3171 | `	void *pBuf;` |
|      - | 3172 | `	int nLen;` |
|     24 | 3173 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3174 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3175 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3176 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3177 | `		return PH7_OK;` |
|      - | 3178 | `	}` |
|      - | 3179 | `	/* Extract our private data */` |
|     24 | 3180 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3181 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     24 | 3182 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3183 | `		/*Expecting an IO handle */` |
|    ! 0 | 3184 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3185 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3186 | `		return PH7_OK;` |
|      - | 3187 | `	}` |
|      - | 3188 | `	/* Point to the target IO stream device */` |
|     24 | 3189 | `	pStream = pDev->pStream;` |
|     24 | 3190 | `	if( pStream == 0  ){` |
|    ! 0 | 3191 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3192 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3193 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3194 | `			);` |
|    ! 0 | 3195 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3196 | `		return PH7_OK;` |
|      - | 3197 | `	}` |
|     24 | 3198 | `        nLen = 4096;` |
|     24 | 3199 | `	if( nArg > 1 ){` |
|     24 | 3200 | ` 	  nLen = ph7_value_to_int(apArg[1]);` |
|     24 | 3201 | `	  if( nLen < 1 ){` |
|      - | 3202 | `		/* Invalid length,set a default length */` |
|    ! 0 | 3203 | `		nLen = 4096;` |
|    ! 0 | 3204 | `	  }` |
|     11 | 3205 | `        }` |
|      - | 3206 | `	/* Allocate enough buffer */` |
|     24 | 3207 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     24 | 3208 | `	if( pBuf == 0 ){` |
|    ! 0 | 3209 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3210 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3211 | `		return PH7_OK;` |
|      - | 3212 | `	}` |
|      - | 3213 | `	/* Perform the requested operation */` |
|     24 | 3214 | `	nRead = StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     24 | 3215 | `	if( nRead < 1 ){` |
|      - | 3216 | `		/* Nothing read,return FALSE */` |
|    ! 0 | 3217 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3218 | `	}else{` |
|      - | 3219 | `		/* Make a copy of the data just read */` |
|     24 | 3220 | `		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);` |
|      - | 3221 | `	}` |
|      - | 3222 | `	/* Release the buffer */` |
|     24 | 3223 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     24 | 3224 | `	return PH7_OK;` |
|     13 | 3225 | `}` |
|      - | 3226 | `/*` |
|      - | 3227 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|      - | 3228 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|      - | 3229 | ` * Gets line from file pointer and parse for CSV fields.` |
|      - | 3230 | ` * Parameters` |
|      - | 3231 | ` * $handle` |
|      - | 3232 | ` *   The file pointer.` |
|      - | 3233 | ` * $length` |
|      - | 3234 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3235 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3236 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3237 | ` *  the end of the line.` |
|      - | 3238 | ` * $delimiter` |
|      - | 3239 | ` *   Set the field delimiter (one character only).` |
|      - | 3240 | ` * $enclosure` |
|      - | 3241 | ` *   Set the field enclosure character (one character only).` |
|      - | 3242 | ` * $escape` |
|      - | 3243 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 3244 | ` * Return` |
|      - | 3245 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3246 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3247 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3248 | ` */` |
|      2 | 3249 | `static int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3250 | `{` |
|      - | 3251 | `	const ph7_io_stream *pStream;` |
|      - | 3252 | `	const char *zLine;` |
|      - | 3253 | `	io_private *pDev;` |
|      - | 3254 | `	ph7_int64 n,nLen;` |
|      3 | 3255 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3256 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3257 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3258 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3259 | `		return PH7_OK;` |
|      - | 3260 | `	}` |
|      - | 3261 | `	/* Extract our private data */` |
|      3 | 3262 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3263 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3264 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3265 | `		/*Expecting an IO handle */` |
|    ! 0 | 3266 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3267 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3268 | `		return PH7_OK;` |
|      - | 3269 | `	}` |
|      - | 3270 | `	/* Point to the target IO stream device */` |
|      3 | 3271 | `	pStream = pDev->pStream;` |
|      3 | 3272 | `	if( pStream == 0  ){` |
|    ! 0 | 3273 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3274 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3275 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3276 | `			);` |
|    ! 0 | 3277 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3278 | `		return PH7_OK;` |
|      - | 3279 | `	}` |
|      3 | 3280 | `	nLen = -1;` |
|      3 | 3281 | `	if( nArg > 1 ){` |
|      - | 3282 | `		/* Maximum data to read */` |
|      3 | 3283 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      1 | 3284 | `	}` |
|      - | 3285 | `	/* Perform the requested operation */` |
|      3 | 3286 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3287 | `	if( n < 1 ){` |
|      - | 3288 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3289 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3290 | `	}else{` |
|      - | 3291 | `		ph7_value *pArray;` |
|      3 | 3292 | `		int delim  = ',';   /* Delimiter */` |
|      3 | 3293 | `		int encl   = '"' ;  /* Enclosure */` |
|      3 | 3294 | `		int escape = '\\';  /* Escape character */` |
|      3 | 3295 | `		if( nArg > 2 ){` |
|      - | 3296 | `			const char *zPtr;` |
|      - | 3297 | `			int i;` |
|      3 | 3298 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 3299 | `				/* Extract the delimiter */` |
|      3 | 3300 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 3301 | `				if( i > 0 ){` |
|      3 | 3302 | `					delim = zPtr[0];` |
|      1 | 3303 | `				}` |
|      1 | 3304 | `			}` |
|      3 | 3305 | `			if( nArg > 3 ){` |
|      3 | 3306 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 3307 | `					/* Extract the enclosure */` |
|      3 | 3308 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 3309 | `					if( i > 0 ){` |
|      3 | 3310 | `						encl = zPtr[0];` |
|      1 | 3311 | `					}` |
|      1 | 3312 | `				}` |
|      3 | 3313 | `				if( nArg > 4 ){` |
|      3 | 3314 | `					if( ph7_value_is_string(apArg[4]) ){` |
|      - | 3315 | `						/* Extract the escape character */` |
|      3 | 3316 | `						zPtr = ph7_value_to_string(apArg[4],&i);` |
|      3 | 3317 | `						if( i > 0 ){` |
|      3 | 3318 | `							escape = zPtr[0];` |
|      1 | 3319 | `						}` |
|      1 | 3320 | `					}` |
|      1 | 3321 | `				}` |
|      1 | 3322 | `			}` |
|      1 | 3323 | `		}` |
|      - | 3324 | `		/* Create our array */` |
|      3 | 3325 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 3326 | `		if( pArray == 0 ){` |
|    ! 0 | 3327 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3328 | `			ph7_result_null(pCtx);` |
|    ! 0 | 3329 | `			return PH7_OK;` |
|      - | 3330 | `		}` |
|      - | 3331 | `		/* Parse the raw input */` |
|      3 | 3332 | `		PH7_ProcessCsv(zLine,(int)n,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 3333 | `		/* Return the freshly created array  */` |
|      3 | 3334 | `		ph7_result_value(pCtx,pArray);` |
|      - | 3335 | `	}` |
|      3 | 3336 | `	return PH7_OK;` |
|      2 | 3337 | `}` |
|      - | 3338 | `/*` |
|      - | 3339 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|      - | 3340 | ` *  Gets line from file pointer and strip HTML tags.` |
|      - | 3341 | ` * Parameters` |
|      - | 3342 | ` * $handle` |
|      - | 3343 | ` *   The file pointer.` |
|      - | 3344 | ` * $length` |
|      - | 3345 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3346 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3347 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3348 | ` *  the end of the line.` |
|      - | 3349 | ` * $allowable_tags` |
|      - | 3350 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 3351 | ` * Return` |
|      - | 3352 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|      - | 3353 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|      - | 3354 | ` */` |
|      2 | 3355 | `static int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3356 | `{` |
|      - | 3357 | `	const ph7_io_stream *pStream;` |
|      - | 3358 | `	const char *zLine;` |
|      - | 3359 | `	io_private *pDev;` |
|      - | 3360 | `	ph7_int64 n,nLen;` |
|      3 | 3361 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3362 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3363 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3364 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3365 | `		return PH7_OK;` |
|      - | 3366 | `	}` |
|      - | 3367 | `	/* Extract our private data */` |
|      3 | 3368 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3369 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3370 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3371 | `		/*Expecting an IO handle */` |
|    ! 0 | 3372 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3373 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3374 | `		return PH7_OK;` |
|      - | 3375 | `	}` |
|      - | 3376 | `	/* Point to the target IO stream device */` |
|      3 | 3377 | `	pStream = pDev->pStream;` |
|      3 | 3378 | `	if( pStream == 0  ){` |
|    ! 0 | 3379 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3380 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3381 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3382 | `			);` |
|    ! 0 | 3383 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3384 | `		return PH7_OK;` |
|      - | 3385 | `	}` |
|      3 | 3386 | `	nLen = -1;` |
|      3 | 3387 | `	if( nArg > 1 ){` |
|      - | 3388 | `		/* Maximum data to read */` |
|    ! 0 | 3389 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3390 | `	}` |
|      - | 3391 | `	/* Perform the requested operation */` |
|      3 | 3392 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3393 | `	if( n < 1 ){` |
|      - | 3394 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3395 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3396 | `	}else{` |
|      3 | 3397 | `		const char *zTaglist = 0;` |
|      3 | 3398 | `		int nTaglen = 0;` |
|      3 | 3399 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|      - | 3400 | `			/* Allowed tag */` |
|    ! 0 | 3401 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|    ! 0 | 3402 | `		}` |
|      - | 3403 | `		/* Process data just read */` |
|      3 | 3404 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen);` |
|      - | 3405 | `	}` |
|      3 | 3406 | `	return PH7_OK;` |
|      2 | 3407 | `}` |
|      - | 3408 | `/*` |
|      - | 3409 | ` * string readdir(resource $dir_handle)` |
|      - | 3410 | ` *   Read entry from directory handle.` |
|      - | 3411 | ` * Parameter` |
|      - | 3412 | ` *  $dir_handle` |
|      - | 3413 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3414 | ` * Return` |
|      - | 3415 | ` *  Returns the filename on success or FALSE on failure.` |
|      - | 3416 | ` */` |
|   8492 | 3417 | `static int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3418 | `{` |
|      - | 3419 | `	const ph7_io_stream *pStream;` |
|      - | 3420 | `	io_private *pDev;` |
|      - | 3421 | `	int rc;` |
|   8497 | 3422 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3423 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3424 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3425 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3426 | `		return PH7_OK;` |
|      - | 3427 | `	}` |
|      - | 3428 | `	/* Extract our private data */` |
|   8497 | 3429 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3430 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   8497 | 3431 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3432 | `		/*Expecting an IO handle */` |
|    ! 0 | 3433 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3434 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3435 | `		return PH7_OK;` |
|      - | 3436 | `	}` |
|      - | 3437 | `	/* Point to the target IO stream device */` |
|   8497 | 3438 | `	pStream = pDev->pStream;` |
|   8497 | 3439 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 3440 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3441 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3442 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3443 | `			);` |
|    ! 0 | 3444 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3445 | `		return PH7_OK;` |
|      - | 3446 | `	}` |
|   8497 | 3447 | `	ph7_result_bool(pCtx,0);` |
|      - | 3448 | `	/* Perform the requested operation */` |
|   8497 | 3449 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|   8497 | 3450 | `	if( rc != PH7_OK ){` |
|      - | 3451 | `		/* Return FALSE */` |
|    997 | 3452 | `		ph7_result_bool(pCtx,0);` |
|    496 | 3453 | `	}` |
|   8497 | 3454 | `	return PH7_OK;` |
|   4251 | 3455 | `}` |
|      - | 3456 | `/*` |
|      - | 3457 | ` * void rewinddir(resource $dir_handle)` |
|      - | 3458 | ` *   Rewind directory handle.` |
|      - | 3459 | ` * Parameter` |
|      - | 3460 | ` *  $dir_handle` |
|      - | 3461 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3462 | ` * Return` |
|      - | 3463 | ` *  FALSE on failure.` |
|      - | 3464 | ` */` |
|      2 | 3465 | `static int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3466 | `{` |
|      - | 3467 | `	const ph7_io_stream *pStream;` |
|      - | 3468 | `	io_private *pDev;` |
|      3 | 3469 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3470 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3471 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3472 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3473 | `		return PH7_OK;` |
|      - | 3474 | `	}` |
|      - | 3475 | `	/* Extract our private data */` |
|      3 | 3476 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3477 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3478 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3479 | `		/*Expecting an IO handle */` |
|    ! 0 | 3480 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3481 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3482 | `		return PH7_OK;` |
|      - | 3483 | `	}` |
|      - | 3484 | `	/* Point to the target IO stream device */` |
|      3 | 3485 | `	pStream = pDev->pStream;` |
|      3 | 3486 | `	if( pStream == 0  \|\| pStream->xRewindDir == 0 ){` |
|    ! 0 | 3487 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3488 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3489 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3490 | `			);` |
|    ! 0 | 3491 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3492 | `		return PH7_OK;` |
|      - | 3493 | `	}` |
|      - | 3494 | `	/* Perform the requested operation */` |
|      3 | 3495 | `	pStream->xRewindDir(pDev->pHandle);` |
|      3 | 3496 | `	return PH7_OK;` |
|      2 | 3497 | ` }` |
|      - | 3498 | `/* Forward declaration */` |
|      - | 3499 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut);` |
|      - | 3500 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|      - | 3501 | `/*` |
|      - | 3502 | ` * void closedir(resource $dir_handle)` |
|      - | 3503 | ` *   Close directory handle.` |
|      - | 3504 | ` * Parameter` |
|      - | 3505 | ` *  $dir_handle` |
|      - | 3506 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3507 | ` * Return` |
|      - | 3508 | ` *  FALSE on failure.` |
|      - | 3509 | ` */` |
|    996 | 3510 | `static int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3511 | `{` |
|      - | 3512 | `	const ph7_io_stream *pStream;` |
|      - | 3513 | `	io_private *pDev;` |
|   1001 | 3514 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3515 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3516 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3517 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3518 | `		return PH7_OK;` |
|      - | 3519 | `	}` |
|      - | 3520 | `	/* Extract our private data */` |
|   1001 | 3521 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3522 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1001 | 3523 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3524 | `		/*Expecting an IO handle */` |
|    ! 0 | 3525 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3526 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3527 | `		return PH7_OK;` |
|      - | 3528 | `	}` |
|      - | 3529 | `	/* Point to the target IO stream device */` |
|   1001 | 3530 | `	pStream = pDev->pStream;` |
|   1001 | 3531 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 3532 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3533 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3534 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3535 | `			);` |
|    ! 0 | 3536 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3537 | `		return PH7_OK;` |
|      - | 3538 | `	}` |
|      - | 3539 | `	/* Perform the requested operation */` |
|   1001 | 3540 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 3541 | `	/* Release the private stucture */` |
|   1001 | 3542 | `	ReleaseIOPrivate(pCtx,pDev);` |
|   1001 | 3543 | `	PH7_MemObjRelease(apArg[0]);` |
|   1001 | 3544 | `	return PH7_OK;` |
|    503 | 3545 | ` }` |
|      - | 3546 | `/*` |
|      - | 3547 | ` * resource opendir(string $path[,resource $context])` |
|      - | 3548 | ` *  Open directory handle.` |
|      - | 3549 | ` * Parameters` |
|      - | 3550 | ` * $path` |
|      - | 3551 | ` *   The directory path that is to be opened.` |
|      - | 3552 | ` * $context` |
|      - | 3553 | ` *   A context stream resource.` |
|      - | 3554 | ` * Return` |
|      - | 3555 | ` *  A directory handle resource on success,or FALSE on failure.` |
|      - | 3556 | ` */` |
|    996 | 3557 | `static int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3558 | `{` |
|      - | 3559 | `	const ph7_io_stream *pStream;` |
|      - | 3560 | `	const char *zPath;` |
|      - | 3561 | `	io_private *pDev;` |
|      - | 3562 | `	int iLen,rc;` |
|   1001 | 3563 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3564 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3565 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 3566 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3567 | `		return PH7_OK;` |
|      - | 3568 | `	}` |
|      - | 3569 | `	/* Extract the target path */` |
|   1001 | 3570 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 3571 | `	/* Try to extract a stream */` |
|   1001 | 3572 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|   1001 | 3573 | `	if( pStream == 0 ){` |
|    ! 0 | 3574 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3575 | `			"No stream device is associated with the given path(%s)",zPath);` |
|    ! 0 | 3576 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3577 | `		return PH7_OK;` |
|      - | 3578 | `	}` |
|   1001 | 3579 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 3580 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3581 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 3582 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 3583 | `			);` |
|    ! 0 | 3584 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3585 | `		return PH7_OK;` |
|      - | 3586 | `	}` |
|      - | 3587 | `	/* Allocate a new IO private instance */` |
|   1001 | 3588 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|   1001 | 3589 | `	if( pDev == 0 ){` |
|    ! 0 | 3590 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3591 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3592 | `		return PH7_OK;` |
|      - | 3593 | `	}` |
|      - | 3594 | `	/* Initialize the structure */` |
|   1001 | 3595 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 3596 | `	/* Open the target directory */` |
|   1001 | 3597 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|   1001 | 3598 | `	if( rc != PH7_OK ){` |
|      - | 3599 | `		/* IO error,return FALSE */` |
|    ! 0 | 3600 | `		ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 3601 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3602 | `	}else{` |
|      - | 3603 | `		/* Return the handle as a resource */` |
|   1001 | 3604 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 3605 | `	}` |
|   1001 | 3606 | `	return PH7_OK;` |
|    503 | 3607 | `}` |
|      - | 3608 | `/*` |
|      - | 3609 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|      - | 3610 | ` *  Reads a file and writes it to the output buffer.` |
|      - | 3611 | ` * Parameters` |
|      - | 3612 | ` *  $filename` |
|      - | 3613 | ` *   The filename being read.` |
|      - | 3614 | ` *  $use_include_path` |
|      - | 3615 | ` *   You can use the optional second parameter and set it to` |
|      - | 3616 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3617 | ` *  $context` |
|      - | 3618 | ` *   A context stream resource.` |
|      - | 3619 | ` * Return` |
|      - | 3620 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|      - | 3621 | ` */` |
|      2 | 3622 | `static int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3623 | `{` |
|      3 | 3624 | `	int use_include  = FALSE;` |
|      - | 3625 | `	const ph7_io_stream *pStream;` |
|      - | 3626 | `	ph7_int64 n,nRead;` |
|      - | 3627 | `	const char *zFile;` |
|      - | 3628 | `	char zBuf[8192];` |
|      - | 3629 | `	void *pHandle;` |
|      - | 3630 | `	int rc,nLen;` |
|      3 | 3631 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3632 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3633 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3634 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3635 | `		return PH7_OK;` |
|      - | 3636 | `	}` |
|      - | 3637 | `	/* Extract the file path */` |
|      3 | 3638 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3639 | `	/* Point to the target IO stream device */` |
|      3 | 3640 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3641 | `	if( pStream == 0 ){` |
|    ! 0 | 3642 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3643 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3644 | `		return PH7_OK;` |
|      - | 3645 | `	}` |
|      3 | 3646 | `	if( nArg > 1 ){` |
|    ! 0 | 3647 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3648 | `	}` |
|      - | 3649 | `	/* Try to open the file in read-only mode */` |
|      4 | 3650 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|      1 | 3651 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 3652 | `	if( pHandle == 0 ){` |
|    ! 0 | 3653 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3654 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3655 | `		return PH7_OK;` |
|      - | 3656 | `	}` |
|      - | 3657 | `	/* Perform the requested operation */` |
|      3 | 3658 | `	nRead = 0;` |
|      2 | 3659 | `	for(;;){` |
|      5 | 3660 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3661 | `		if( n < 1 ){` |
|      - | 3662 | `			/* EOF or IO error,break immediately */` |
|      3 | 3663 | `			break;` |
|      - | 3664 | `		}` |
|      - | 3665 | `		/* Output data */` |
|      3 | 3666 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      3 | 3667 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 3668 | `			break;` |
|      - | 3669 | `		}` |
|      - | 3670 | `		/* Increment counter */` |
|      3 | 3671 | `		nRead += n;` |
|      1 | 3672 | `	}` |
|      - | 3673 | `	/* Close the stream */` |
|      3 | 3674 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3675 | `	/* Total number of bytes readen */` |
|      3 | 3676 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 3677 | `	return PH7_OK;` |
|      2 | 3678 | `}` |
|      - | 3679 | `/*` |
|      - | 3680 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|      - | 3681 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|      - | 3682 | ` *  Reads entire file into a string.` |
|      - | 3683 | ` * Parameters` |
|      - | 3684 | ` *  $filename` |
|      - | 3685 | ` *   The filename being read.` |
|      - | 3686 | ` *  $use_include_path` |
|      - | 3687 | ` *   You can use the optional second parameter and set it to` |
|      - | 3688 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3689 | ` *  $context` |
|      - | 3690 | ` *   A context stream resource.` |
|      - | 3691 | ` *  $offset` |
|      - | 3692 | ` *   The offset where the reading starts on the original stream.` |
|      - | 3693 | ` *  $maxlen` |
|      - | 3694 | ` *    Maximum length of data read. The default is to read until end of file` |
|      - | 3695 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|      - | 3696 | ` * Return` |
|      - | 3697 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3698 | ` */` |
|   6550 | 3699 | `static int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3700 | `{` |
|      - | 3701 | `	const ph7_io_stream *pStream;` |
|      - | 3702 | `	ph7_int64 n,nRead,nMaxlen;` |
|   6555 | 3703 | `	int use_include  = FALSE;` |
|      - | 3704 | `	const char *zFile;` |
|      - | 3705 | `	char zBuf[8192];` |
|      - | 3706 | `	void *pHandle;` |
|      - | 3707 | `	int nLen;` |
|      - | 3708 |  |
|   6555 | 3709 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3710 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3711 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3712 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3713 | `		return PH7_OK;` |
|      - | 3714 | `	}` |
|      - | 3715 | `	/* Extract the file path */` |
|   6555 | 3716 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3717 | `	/* Point to the target IO stream device */` |
|   6555 | 3718 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   6555 | 3719 | `	if( pStream == 0 ){` |
|    ! 0 | 3720 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3721 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3722 | `		return PH7_OK;` |
|      - | 3723 | `	}` |
|   6555 | 3724 | `	nMaxlen = -1;` |
|   6555 | 3725 | `	if( nArg > 1 ){` |
|      5 | 3726 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|      2 | 3727 | `	}` |
|      - | 3728 | `	/* Try to open the file in read-only mode */` |
|   6555 | 3729 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|   6555 | 3730 | `	if( pHandle == 0 ){` |
|    ! 0 | 3731 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3732 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3733 | `		return PH7_OK;` |
|      - | 3734 | `	}` |
|   6555 | 3735 | `	if( nArg > 3 ){` |
|      - | 3736 | `		/* Extract the offset */` |
|      5 | 3737 | `		n = ph7_value_to_int64(apArg[3]);` |
|      5 | 3738 | `		if( n > 0 ){` |
|    ! 0 | 3739 | `			if( pStream->xSeek ){` |
|      - | 3740 | `				/* Seek to the desired offset */` |
|    ! 0 | 3741 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|    ! 0 | 3742 | `			}` |
|    ! 0 | 3743 | `		}` |
|      5 | 3744 | `		if( nArg > 4 ){` |
|      - | 3745 | `			/* Maximum data to read */` |
|      5 | 3746 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|      2 | 3747 | `		}` |
|      2 | 3748 | `	}` |
|      - | 3749 | `	/* Perform the requested operation */` |
|   6555 | 3750 | `	nRead = 0;` |
|   6548 | 3751 | `	for(;;){` |
|  19652 | 3752 | `		n = pStream->xRead(pHandle,zBuf,` |
|   6551 | 3753 | `			(nMaxlen > 0 && (nMaxlen < (ph7_int64)sizeof(zBuf))) ? nMaxlen : (ph7_int64)sizeof(zBuf));` |
|  13101 | 3754 | `		if( n < 1 ){` |
|      - | 3755 | `			/* EOF or IO error,break immediately */` |
|   6553 | 3756 | `			break;` |
|      - | 3757 | `		}` |
|      - | 3758 | `		/* Append data */` |
|   6553 | 3759 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 3760 | `		/* Increment read counter */` |
|   6553 | 3761 | `		nRead += n;` |
|   6553 | 3762 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 3763 | `			/* Read limit reached */` |
|      3 | 3764 | `			break;` |
|      - | 3765 | `		}` |
|      5 | 3766 | `	}` |
|      - | 3767 | `	/* Close the stream */` |
|   6555 | 3768 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3769 | `	/* Check if we have read something */` |
|   6555 | 3770 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 3771 | `		/* Nothing read,return FALSE */` |
|      3 | 3772 | `		ph7_result_bool(pCtx,0);` |
|      1 | 3773 | `	}` |
|   6555 | 3774 | `	return PH7_OK;` |
|   3280 | 3775 | `}` |
|      - | 3776 | `/*` |
|      - | 3777 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 3778 | ` *  Write a string to a file.` |
|      - | 3779 | ` * Parameters` |
|      - | 3780 | ` *  $filename` |
|      - | 3781 | ` *  Path to the file where to write the data.` |
|      - | 3782 | ` * $data` |
|      - | 3783 | ` *  The data to write(Must be a string).` |
|      - | 3784 | ` * $flags` |
|      - | 3785 | ` *  The value of flags can be any combination of the following` |
|      - | 3786 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 3787 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 3788 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 3789 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 3790 | ` * context` |
|      - | 3791 | ` *  A context stream resource.` |
|      - | 3792 | ` * Return` |
|      - | 3793 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 3794 | ` */` |
|  13868 | 3795 | `static int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3796 | `{` |
|  13873 | 3797 | `	int use_include  = FALSE;` |
|      - | 3798 | `	const ph7_io_stream *pStream;` |
|      - | 3799 | `	const char *zFile;` |
|      - | 3800 | `	const char *zData;` |
|      - | 3801 | `	int iOpenFlags;` |
|      - | 3802 | `	void *pHandle;` |
|      - | 3803 | `	int iFlags;` |
|      - | 3804 | `	int nLen;` |
|      - | 3805 |  |
|  13873 | 3806 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3807 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3808 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3809 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3810 | `		return PH7_OK;` |
|      - | 3811 | `	}` |
|      - | 3812 | `	/* Extract the file path */` |
|  13873 | 3813 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3814 | `	/* Point to the target IO stream device */` |
|  13873 | 3815 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  13873 | 3816 | `	if( pStream == 0 ){` |
|    ! 0 | 3817 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3818 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3819 | `		return PH7_OK;` |
|      - | 3820 | `	}` |
|      - | 3821 | `	/* Data to write */` |
|  13873 | 3822 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 3823 | `	/* Try to open the file in read-write mode */` |
|  13873 | 3824 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 3825 | `	/* Extract the flags */` |
|  13873 | 3826 | `	iFlags = 0;` |
|  13873 | 3827 | `	if( nArg > 2 ){` |
|    ! 0 | 3828 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3829 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 3830 | `			use_include = TRUE;` |
|    ! 0 | 3831 | `		}` |
|    ! 0 | 3832 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 3833 | `			/* If the file already exists, append the data to the file` |
|      - | 3834 | `			 * instead of overwriting it.` |
|      - | 3835 | `			 */` |
|    ! 0 | 3836 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 3837 | `			/* Append mode */` |
|    ! 0 | 3838 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 3839 | `		}` |
|    ! 0 | 3840 | `	}` |
|  20807 | 3841 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   6934 | 3842 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|  13873 | 3843 | `	if( pHandle == 0 ){` |
|    ! 0 | 3844 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3845 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3846 | `		return PH7_OK;` |
|      - | 3847 | `	}` |
|  13873 | 3848 | `	if( nLen < 1 ){` |
|      - | 3849 | `		/* Empty data, file is created/truncated */` |
|      7 | 3850 | `		ph7_result_int64(pCtx,0);` |
|      7 | 3851 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|      7 | 3852 | `		return PH7_OK;` |
|      - | 3853 | `	}` |
|  13867 | 3854 | `	if( pStream->xWrite ){` |
|      - | 3855 | `		ph7_int64 n;` |
|  13867 | 3856 | `		if( (iFlags & 0x01/* LOCK_EX */) && pStream->xLock ){` |
|      - | 3857 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 3858 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 3859 | `		}` |
|      - | 3860 | `		/* Perform the write operation */` |
|  13867 | 3861 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  13867 | 3862 | `		if( n < 0 ){` |
|      - | 3863 | `			/* IO error,return FALSE */` |
|    ! 0 | 3864 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3865 | `		}else{` |
|      - | 3866 | `			/* Total number of bytes written */` |
|  13867 | 3867 | `			ph7_result_int64(pCtx,n);` |
|      - | 3868 | `		}` |
|   6936 | 3869 | `	}else{` |
|      - | 3870 | `		/* Read-only stream */` |
|    ! 0 | 3871 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 3872 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 3873 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 3874 | `			);` |
|    ! 0 | 3875 | `		ph7_result_bool(pCtx,0);` |
|      - | 3876 | `	}` |
|      - | 3877 | `	/* Close the handle */` |
|  13867 | 3878 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  13867 | 3879 | `	return PH7_OK;` |
|   6939 | 3880 | `}` |
|      - | 3881 | `/*` |
|      - | 3882 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 3883 | ` *  Reads entire file into an array.` |
|      - | 3884 | ` * Parameters` |
|      - | 3885 | ` *  $filename` |
|      - | 3886 | ` *   The filename being read.` |
|      - | 3887 | ` *  $flags` |
|      - | 3888 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 3889 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 3890 | ` *       Search for the file in the include_path.` |
|      - | 3891 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 3892 | ` *       Do not add newline at the end of each array element` |
|      - | 3893 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 3894 | ` *       Skip empty lines` |
|      - | 3895 | ` *  $context` |
|      - | 3896 | ` *   A context stream resource.` |
|      - | 3897 | ` * Return` |
|      - | 3898 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3899 | ` */` |
|      6 | 3900 | `static int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3901 | `{` |
|      - | 3902 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 3903 | `	ph7_value *pArray,*pLine;` |
|      - | 3904 | `	const ph7_io_stream *pStream;` |
|      8 | 3905 | `	int use_include = 0;` |
|      - | 3906 | `	io_private *pDev;` |
|      - | 3907 | `	ph7_int64 n;` |
|      - | 3908 | `	int iFlags;` |
|      - | 3909 | `	int nLen;` |
|      - | 3910 |  |
|      8 | 3911 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3912 | `		/* Missing/Invalid arguments,return FALSE */` |
|      3 | 3913 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|      3 | 3914 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3915 | `		return PH7_OK;` |
|      - | 3916 | `	}` |
|      - | 3917 | `	/* Extract the file path */` |
|      6 | 3918 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3919 | `	/* Point to the target IO stream device */` |
|      6 | 3920 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 3921 | `	if( pStream == 0 ){` |
|    ! 0 | 3922 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3923 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3924 | `		return PH7_OK;` |
|      - | 3925 | `	}` |
|      - | 3926 | `	/* Allocate a new IO private instance */` |
|      6 | 3927 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      6 | 3928 | `	if( pDev == 0 ){` |
|    ! 0 | 3929 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3930 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3931 | `		return PH7_OK;` |
|      - | 3932 | `	}` |
|      - | 3933 | `	/* Initialize the structure */` |
|      6 | 3934 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      6 | 3935 | `	iFlags = 0;` |
|      6 | 3936 | `	if( nArg > 1 ){` |
|    ! 0 | 3937 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 3938 | `	}` |
|      6 | 3939 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|    ! 0 | 3940 | `		use_include = TRUE;` |
|    ! 0 | 3941 | `	}` |
|      - | 3942 | `	/* Create the array and the working value */` |
|      6 | 3943 | `	pArray = ph7_context_new_array(pCtx);` |
|      6 | 3944 | `	pLine = ph7_context_new_scalar(pCtx);` |
|      6 | 3945 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 3946 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3947 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3948 | `		return PH7_OK;` |
|      - | 3949 | `	}` |
|      - | 3950 | `	/* Try to open the file in read-only mode */` |
|      6 | 3951 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 3952 | `	if( pDev->pHandle == 0 ){` |
|      3 | 3953 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|      3 | 3954 | `		ph7_result_bool(pCtx,0);` |
|      - | 3955 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 3956 | `		 * as soon we return from this function.` |
|      - | 3957 | `		 */` |
|      3 | 3958 | `		return PH7_OK;` |
|      - | 3959 | `	}` |
|      - | 3960 | `	/* Perform the requested operation */` |
|      3 | 3961 | `	for(;;){` |
|      - | 3962 | `		/* Try to extract a line */` |
|      7 | 3963 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|      7 | 3964 | `		if( n < 1 ){` |
|      - | 3965 | `			/* EOF or IO error */` |
|      3 | 3966 | `			break;` |
|      - | 3967 | `		}` |
|      - | 3968 | `		/* Reset the cursor */` |
|      5 | 3969 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 3970 | `		/* Remove line ending if requested by the caller */` |
|      5 | 3971 | `		zPtr = zBuf;` |
|      5 | 3972 | `		zEnd = &zBuf[n];` |
|      5 | 3973 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 3974 | `			/* Ignore trailig lines */` |
|    ! 0 | 3975 | `			while( zPtr < zEnd && (zEnd[-1] == '\n'` |
|      - | 3976 | `#ifdef __WINNT__` |
|      - | 3977 | `				\|\| zEnd[-1] == '\r'` |
|      - | 3978 | `#endif` |
|      - | 3979 | `				)){` |
|    ! 0 | 3980 | `					n--;` |
|    ! 0 | 3981 | `					zEnd--;` |
|    ! 0 | 3982 | `			}` |
|    ! 0 | 3983 | `		}` |
|      5 | 3984 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 3985 | `			/* Ignore empty lines */` |
|    ! 0 | 3986 | `			while( zPtr < zEnd && (unsigned char)zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) ){` |
|    ! 0 | 3987 | `				zPtr++;` |
|    ! 0 | 3988 | `			}` |
|    ! 0 | 3989 | `			if( zPtr >= zEnd ){` |
|      - | 3990 | `				/* Empty line */` |
|    ! 0 | 3991 | `				continue;` |
|      - | 3992 | `			}` |
|    ! 0 | 3993 | `		}` |
|      5 | 3994 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 3995 | `		/* Insert line */` |
|      5 | 3996 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 3997 | `	}` |
|      - | 3998 | `	/* Close the stream */` |
|      3 | 3999 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 4000 | `	/* Release the io_private instance */` |
|      3 | 4001 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 4002 | `	/* Return the created array */` |
|      3 | 4003 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4004 | `	return PH7_OK;` |
|      5 | 4005 | `}` |
|      - | 4006 | `/*` |
|      - | 4007 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 4008 | ` *  Makes a copy of the file source to dest.` |
|      - | 4009 | ` * Parameters` |
|      - | 4010 | ` *  $source` |
|      - | 4011 | ` *   Path to the source file.` |
|      - | 4012 | ` *  $dest` |
|      - | 4013 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 4014 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 4015 | ` *  $context` |
|      - | 4016 | ` *   A context stream resource.` |
|      - | 4017 | ` * Return` |
|      - | 4018 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4019 | ` */` |
|     10 | 4020 | `static int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4021 | `{` |
|      - | 4022 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 4023 | `	const char *zFile;` |
|      - | 4024 | `	char zBuf[8192];` |
|      - | 4025 | `	void *pIn,*pOut;` |
|      - | 4026 | `	ph7_int64 n;` |
|      - | 4027 | `	int nLen;` |
|     12 | 4028 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 4029 | `		/* Missing/Invalid arguments,return FALSE */` |
|      7 | 4030 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|      7 | 4031 | `		ph7_result_bool(pCtx,0);` |
|      7 | 4032 | `		return PH7_OK;` |
|      - | 4033 | `	}` |
|      - | 4034 | `	/* Extract the source name */` |
|      6 | 4035 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4036 | `	/* Point to the target IO stream device */` |
|      6 | 4037 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 4038 | `	if( pSin == 0 ){` |
|    ! 0 | 4039 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4040 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4041 | `		return PH7_OK;` |
|      - | 4042 | `	}` |
|      - | 4043 | `	/* Try to open the source file in a read-only mode */` |
|      6 | 4044 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 4045 | `	if( pIn == 0 ){` |
|      3 | 4046 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening source: '%s'",zFile);` |
|      3 | 4047 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4048 | `		return PH7_OK;` |
|      - | 4049 | `	}` |
|      - | 4050 | `	/* Extract the destination name */` |
|      3 | 4051 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 4052 | `	/* Point to the target IO stream device */` |
|      3 | 4053 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4054 | `	if( pSout == 0 ){` |
|    ! 0 | 4055 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4056 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4057 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4058 | `		return PH7_OK;` |
|      - | 4059 | `	}` |
|      3 | 4060 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 4061 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4062 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4063 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 4064 | `			);` |
|    ! 0 | 4065 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4066 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4067 | `		return PH7_OK;` |
|      - | 4068 | `	}` |
|      - | 4069 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 4070 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 4071 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 4072 | `	if( pOut == 0 ){` |
|    ! 0 | 4073 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening destination: '%s'",zFile);` |
|    ! 0 | 4074 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4075 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4076 | `		return PH7_OK;` |
|      - | 4077 | `	}` |
|      - | 4078 | `	/* Perform the requested operation */` |
|      2 | 4079 | `	for(;;){` |
|      - | 4080 | `		/* Read from source */` |
|      5 | 4081 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 4082 | `		if( n < 1 ){` |
|      - | 4083 | `			/* EOF or IO error,break immediately */` |
|      3 | 4084 | `			break;` |
|      - | 4085 | `		}` |
|      - | 4086 | `		/* Write to dest */` |
|      3 | 4087 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 4088 | `		if( n < 1 ){` |
|      - | 4089 | `			/* IO error,break immediately */` |
|    ! 0 | 4090 | `			break;` |
|      - | 4091 | `		}` |
|      1 | 4092 | `	}` |
|      - | 4093 | `	/* Close the streams */` |
|      3 | 4094 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 4095 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 4096 | `	/* Return TRUE */` |
|      3 | 4097 | `	ph7_result_bool(pCtx,1);` |
|      3 | 4098 | `	return PH7_OK;` |
|      7 | 4099 | `}` |
|      - | 4100 | `/*` |
|      - | 4101 | ` * array fstat(resource $handle)` |
|      - | 4102 | ` *  Gets information about a file using an open file pointer.` |
|      - | 4103 | ` * Parameters` |
|      - | 4104 | ` *  $handle` |
|      - | 4105 | ` *   The file pointer.` |
|      - | 4106 | ` * Return` |
|      - | 4107 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 4108 | ` */` |
|      2 | 4109 | `static int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4110 | `{` |
|      - | 4111 | `	ph7_value *pArray,*pValue;` |
|      - | 4112 | `	const ph7_io_stream *pStream;` |
|      - | 4113 | `	io_private *pDev;` |
|      3 | 4114 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4115 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4116 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4117 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4118 | `		return PH7_OK;` |
|      - | 4119 | `	}` |
|      - | 4120 | `	/* Extract our private data */` |
|      3 | 4121 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4122 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4123 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4124 | `		/* Expecting an IO handle */` |
|    ! 0 | 4125 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4126 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4127 | `		return PH7_OK;` |
|      - | 4128 | `	}` |
|      - | 4129 | `	/* Point to the target IO stream device */` |
|      3 | 4130 | `	pStream = pDev->pStream;` |
|      3 | 4131 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 4132 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4133 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4134 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4135 | `			);` |
|    ! 0 | 4136 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4137 | `		return PH7_OK;` |
|      - | 4138 | `	}` |
|      - | 4139 | `	/* Create the array and the working value */` |
|      3 | 4140 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4141 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4142 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4143 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4144 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4145 | `		return PH7_OK;` |
|      - | 4146 | `	}` |
|      - | 4147 | `	/* Perform the requested operation */` |
|      3 | 4148 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 4149 | `	/* Return the freshly created array */` |
|      3 | 4150 | `	ph7_result_value(pCtx,pArray);` |
|      - | 4151 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 4152 | `	 * released automatically as soon we return from this function.` |
|      - | 4153 | `	 */` |
|      3 | 4154 | `	return PH7_OK;` |
|      2 | 4155 | `}` |
|      - | 4156 | `/*` |
|      - | 4157 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 4158 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 4159 | ` * Parameters` |
|      - | 4160 | ` *  $handle` |
|      - | 4161 | ` *   The file pointer.` |
|      - | 4162 | ` *  $string` |
|      - | 4163 | ` *   The string that is to be written.` |
|      - | 4164 | ` *  $length` |
|      - | 4165 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 4166 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 4167 | ` * Return` |
|      - | 4168 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 4169 | ` */` |
|     16 | 4170 | `static int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4171 | `{` |
|      - | 4172 | `	const ph7_io_stream *pStream;` |
|      - | 4173 | `	const char *zString;` |
|      - | 4174 | `	io_private *pDev;` |
|      - | 4175 | `	int nLen,n;` |
|     17 | 4176 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4177 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4178 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4179 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4180 | `		return PH7_OK;` |
|      - | 4181 | `	}` |
|      - | 4182 | `	/* Extract our private data */` |
|     17 | 4183 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4184 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     17 | 4185 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4186 | `		/* Expecting an IO handle */` |
|    ! 0 | 4187 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4188 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4189 | `		return PH7_OK;` |
|      - | 4190 | `	}` |
|      - | 4191 | `	/* Point to the target IO stream device */` |
|     17 | 4192 | `	pStream = pDev->pStream;` |
|     17 | 4193 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4194 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4195 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4196 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4197 | `			);` |
|    ! 0 | 4198 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4199 | `		return PH7_OK;` |
|      - | 4200 | `	}` |
|      - | 4201 | `	/* Extract the data to write */` |
|     17 | 4202 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|     17 | 4203 | `	if( nArg > 2 ){` |
|      - | 4204 | `		/* Maximum data length to write */` |
|    ! 0 | 4205 | `		n = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 4206 | `		if( n >= 0 && n < nLen ){` |
|    ! 0 | 4207 | `			nLen = n;` |
|    ! 0 | 4208 | `		}` |
|    ! 0 | 4209 | `	}` |
|     17 | 4210 | `	if( nLen < 1 ){` |
|      - | 4211 | `		/* Nothing to write */` |
|    ! 0 | 4212 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4213 | `		return PH7_OK;` |
|      - | 4214 | `	}` |
|      - | 4215 | `	/* Perform the requested operation */` |
|     17 | 4216 | `	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);` |
|     17 | 4217 | `	if( n <  0 ){` |
|      - | 4218 | `		/* IO error,return FALSE */` |
|    ! 0 | 4219 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4220 | `	}else{` |
|      - | 4221 | `		/* #Bytes written */` |
|     17 | 4222 | `		ph7_result_int(pCtx,n);` |
|      - | 4223 | `	}` |
|     17 | 4224 | `	return PH7_OK;` |
|      9 | 4225 | `}` |
|      - | 4226 | `/*` |
|      - | 4227 | ` * bool flock(resource $handle,int $operation)` |
|      - | 4228 | ` *  Portable advisory file locking.` |
|      - | 4229 | ` * Parameters` |
|      - | 4230 | ` *  $handle` |
|      - | 4231 | ` *   The file pointer.` |
|      - | 4232 | ` *  $operation` |
|      - | 4233 | ` *   operation is one of the following:` |
|      - | 4234 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 4235 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 4236 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 4237 | ` * Return` |
|      - | 4238 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 4239 | ` */` |
|      4 | 4240 | `static int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 4241 | `{` |
|      - | 4242 | `	const ph7_io_stream *pStream;` |
|      - | 4243 | `	io_private *pDev;` |
|      - | 4244 | `	int nLock;` |
|      - | 4245 | `	int rc;` |
|      4 | 4246 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4247 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4248 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4249 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4250 | `		return PH7_OK;` |
|      - | 4251 | `	}` |
|      - | 4252 | `	/* Extract our private data */` |
|      4 | 4253 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4254 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      4 | 4255 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4256 | `		/*Expecting an IO handle */` |
|    ! 0 | 4257 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4258 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4259 | `		return PH7_OK;` |
|      - | 4260 | `	}` |
|      - | 4261 | `	/* Point to the target IO stream device */` |
|      4 | 4262 | `	pStream = pDev->pStream;` |
|      4 | 4263 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|    ! 0 | 4264 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4265 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4266 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4267 | `			);` |
|    ! 0 | 4268 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4269 | `		return PH7_OK;` |
|      - | 4270 | `	}` |
|      - | 4271 | `	/* Requested lock operation */` |
|      4 | 4272 | `	nLock = ph7_value_to_int(apArg[1]);` |
|      - | 4273 | `	/* Lock operation */` |
|      4 | 4274 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 4275 | `	/* IO result */` |
|      4 | 4276 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 | 4277 | `	return PH7_OK;` |
|      2 | 4278 | `}` |
|      - | 4279 | `/*` |
|      - | 4280 | ` * int fpassthru(resource $handle)` |
|      - | 4281 | ` *  Output all remaining data on a file pointer.` |
|      - | 4282 | ` * Parameters` |
|      - | 4283 | ` *  $handle` |
|      - | 4284 | ` *   The file pointer.` |
|      - | 4285 | ` * Return` |
|      - | 4286 | ` *  Total number of characters read from handle and passed through` |
|      - | 4287 | ` *  to the output on success or FALSE on failure.` |
|      - | 4288 | ` */` |
|      2 | 4289 | `static int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4290 | `{` |
|      - | 4291 | `	const ph7_io_stream *pStream;` |
|      - | 4292 | `	io_private *pDev;` |
|      - | 4293 | `	ph7_int64 n,nRead;` |
|      - | 4294 | `	char zBuf[8192];` |
|      - | 4295 | `	int rc;` |
|      3 | 4296 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4297 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4298 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4299 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4300 | `		return PH7_OK;` |
|      - | 4301 | `	}` |
|      - | 4302 | `	/* Extract our private data */` |
|      3 | 4303 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4304 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4305 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4306 | `		/*Expecting an IO handle */` |
|    ! 0 | 4307 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4308 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4309 | `		return PH7_OK;` |
|      - | 4310 | `	}` |
|      - | 4311 | `	/* Point to the target IO stream device */` |
|      3 | 4312 | `	pStream = pDev->pStream;` |
|      3 | 4313 | `	if( pStream == 0  ){` |
|    ! 0 | 4314 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4315 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4316 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4317 | `			);` |
|    ! 0 | 4318 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4319 | `		return PH7_OK;` |
|      - | 4320 | `	}` |
|      - | 4321 | `	/* Perform the requested operation */` |
|      3 | 4322 | `	nRead = 0;` |
|      2 | 4323 | `	for(;;){` |
|      5 | 4324 | `		n = StreamRead(pDev,zBuf,sizeof(zBuf));` |
|      5 | 4325 | `		if( n < 1 ){` |
|      - | 4326 | `			/* Error or EOF */` |
|      3 | 4327 | `			break;` |
|      - | 4328 | `		}` |
|      - | 4329 | `		/* Increment the read counter */` |
|      3 | 4330 | `		nRead += n;` |
|      - | 4331 | `		/* Output data */` |
|      3 | 4332 | `		rc = ph7_context_output(pCtx,zBuf,(int)nRead /* FIXME: 64-bit issues */);` |
|      3 | 4333 | `		if( rc == PH7_ABORT ){` |
|      - | 4334 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 4335 | `			break;` |
|      - | 4336 | `		}` |
|      1 | 4337 | `	}` |
|      - | 4338 | `	/* Total number of bytes readen */` |
|      3 | 4339 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 4340 | `	return PH7_OK;` |
|      2 | 4341 | `}` |
|      - | 4342 | `/* CSV reader/writer private data */` |
|      - | 4343 | `struct csv_data` |
|      - | 4344 | `{` |
|      - | 4345 | `	int delimiter;    /* Delimiter. Default ',' */` |
|      - | 4346 | `	int enclosure;    /* Enclosure. Default '"'*/` |
|      - | 4347 | `	io_private *pDev; /* Open stream handle */` |
|      - | 4348 | `	int iCount;       /* Counter */` |
|      - | 4349 | `};` |
|      - | 4350 | `/*` |
|      - | 4351 | ` * The following callback is used by the fputcsv() function inorder to iterate` |
|      - | 4352 | ` * throw array entries and output CSV data based on the current key and it's` |
|      - | 4353 | ` * associated data.` |
|      - | 4354 | ` */` |
|      6 | 4355 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 4356 | `{` |
|      7 | 4357 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 4358 | `	const char *zData;` |
|      - | 4359 | `	int nLen,c2;` |
|      - | 4360 | `	sxu32 n;` |
|      - | 4361 | `	/* Point to the raw data */` |
|      7 | 4362 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      7 | 4363 | `	if( nLen < 1 ){` |
|      - | 4364 | `		/* Nothing to write */` |
|    ! 0 | 4365 | `		return PH7_OK;` |
|      - | 4366 | `	}` |
|      7 | 4367 | `	if( pData->iCount > 0 ){` |
|      - | 4368 | `		/* Write the delimiter */` |
|      5 | 4369 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->delimiter,sizeof(char));` |
|      2 | 4370 | `	}` |
|      7 | 4371 | `	n = 1;` |
|      7 | 4372 | `	c2 = 0;` |
|     10 | 4373 | `	if( SyByteFind(zData,(sxu32)nLen,pData->delimiter,0) == SXRET_OK \|\|` |
|      6 | 4374 | `		SyByteFind(zData,(sxu32)nLen,pData->enclosure,&n) == SXRET_OK ){` |
|    ! 0 | 4375 | `			c2 = 1;` |
|    ! 0 | 4376 | `			if( n == 0 ){` |
|    ! 0 | 4377 | `				c2 = 2;` |
|    ! 0 | 4378 | `			}` |
|      - | 4379 | `			/* Write the enclosure */` |
|    ! 0 | 4380 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4381 | `			if( c2 > 1 ){` |
|    ! 0 | 4382 | `				pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4383 | `			}` |
|    ! 0 | 4384 | `	}` |
|      - | 4385 | `	/* Write the data */` |
|      7 | 4386 | `	if( pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)zData,(ph7_int64)nLen) < 1 ){` |
|    ! 0 | 4387 | `		SXUNUSED(pKey); /* cc warning */` |
|    ! 0 | 4388 | `		return PH7_ABORT;` |
|      - | 4389 | `	}` |
|      7 | 4390 | `	if( c2 > 0 ){` |
|      - | 4391 | `		/* Write the enclosure */` |
|    ! 0 | 4392 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4393 | `		if( c2 > 1 ){` |
|    ! 0 | 4394 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4395 | `		}` |
|    ! 0 | 4396 | `	}` |
|      7 | 4397 | `	pData->iCount++;` |
|      7 | 4398 | `	return PH7_OK;` |
|      4 | 4399 | `}` |
|      - | 4400 | `/*` |
|      - | 4401 | ` * int fputcsv(resource $handle,array $fields[,string $delimiter = ','[,string $enclosure = '"' ]])` |
|      - | 4402 | ` *  Format line as CSV and write to file pointer.` |
|      - | 4403 | ` * Parameters` |
|      - | 4404 | ` *  $handle` |
|      - | 4405 | ` *   Open file handle.` |
|      - | 4406 | ` * $fields` |
|      - | 4407 | ` *   An array of values.` |
|      - | 4408 | ` * $delimiter` |
|      - | 4409 | ` *   The optional delimiter parameter sets the field delimiter (one character only).` |
|      - | 4410 | ` * $enclosure` |
|      - | 4411 | ` *  The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 4412 | ` */` |
|      2 | 4413 | `static int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4414 | `{` |
|      - | 4415 | `	const ph7_io_stream *pStream;` |
|      - | 4416 | `	struct csv_data sCsv;` |
|      - | 4417 | `	io_private *pDev;` |
|      - | 4418 | `	char *zEol;` |
|      - | 4419 | `	int eolen;` |
|      3 | 4420 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4421 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4422 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|    ! 0 | 4423 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4424 | `		return PH7_OK;` |
|      - | 4425 | `	}` |
|      - | 4426 | `	/* Extract our private data */` |
|      3 | 4427 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4428 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4429 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4430 | `		/*Expecting an IO handle */` |
|    ! 0 | 4431 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4432 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4433 | `		return PH7_OK;` |
|      - | 4434 | `	}` |
|      - | 4435 | `	/* Point to the target IO stream device */` |
|      3 | 4436 | `	pStream = pDev->pStream;` |
|      3 | 4437 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4438 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4439 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4440 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4441 | `			);` |
|    ! 0 | 4442 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4443 | `		return PH7_OK;` |
|      - | 4444 | `	}` |
|      - | 4445 | `	/* Set default csv separator */` |
|      3 | 4446 | `	sCsv.delimiter = ',';` |
|      3 | 4447 | `	sCsv.enclosure = '"';` |
|      3 | 4448 | `	sCsv.pDev = pDev;` |
|      3 | 4449 | `	sCsv.iCount = 0;` |
|      3 | 4450 | `	if( nArg > 2 ){` |
|      - | 4451 | `		/* User delimiter */` |
|      - | 4452 | `		const char *z;` |
|      - | 4453 | `		int n;` |
|      3 | 4454 | `		z = ph7_value_to_string(apArg[2],&n);` |
|      3 | 4455 | `		if( n > 0 ){` |
|      3 | 4456 | `			sCsv.delimiter = z[0];` |
|      1 | 4457 | `		}` |
|      3 | 4458 | `		if( nArg > 3 ){` |
|      3 | 4459 | `			z = ph7_value_to_string(apArg[3],&n);` |
|      3 | 4460 | `			if( n > 0 ){` |
|      3 | 4461 | `				sCsv.enclosure = z[0];` |
|      1 | 4462 | `			}` |
|      1 | 4463 | `		}` |
|      1 | 4464 | `	}` |
|      - | 4465 | `	/* Iterate throw array entries and write csv data */` |
|      3 | 4466 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      - | 4467 | `	/* Write a line ending */` |
|      - | 4468 | `#ifdef __WINNT__` |
|      1 | 4469 | `	zEol = "\r\n";` |
|      1 | 4470 | `	eolen = (int)sizeof("\r\n")-1;` |
|      - | 4471 | `#else` |
|      - | 4472 | `	/* Assume UNIX LF */` |
|      2 | 4473 | `	zEol = "\n";` |
|      2 | 4474 | `	eolen = (int)sizeof(char);` |
|      - | 4475 | `#endif` |
|      3 | 4476 | `	pDev->pStream->xWrite(pDev->pHandle,(const void *)zEol,eolen);` |
|      3 | 4477 | `	return PH7_OK;` |
|      2 | 4478 | `}` |
|      - | 4479 | `/*` |
|      - | 4480 | ` * fprintf,vfprintf private data.` |
|      - | 4481 | ` * An instance of the following structure is passed to the formatted` |
|      - | 4482 | ` * input consumer callback defined below.` |
|      - | 4483 | ` */` |
|      - | 4484 | `typedef struct fprintf_data fprintf_data;` |
|      - | 4485 | `struct fprintf_data` |
|      - | 4486 | `{` |
|      - | 4487 | `	io_private *pIO;        /* IO stream */` |
|      - | 4488 | `	ph7_int64 nCount;       /* Total number of bytes written */` |
|      - | 4489 | `};` |
|      - | 4490 | `/*` |
|      - | 4491 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 4492 | ` */` |
|     30 | 4493 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4494 | `{` |
|     31 | 4495 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 4496 | `	ph7_int64 n;` |
|      - | 4497 | `	/* Write the formatted data */` |
|     31 | 4498 | `	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);` |
|     31 | 4499 | `	if( n < 1 ){` |
|    ! 0 | 4500 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 4501 | `		/* IO error,abort immediately */` |
|    ! 0 | 4502 | `		return SXERR_ABORT;` |
|      - | 4503 | `	}` |
|      - | 4504 | `	/* Increment counter */` |
|     31 | 4505 | `	pFdata->nCount += n;` |
|     31 | 4506 | `	return PH7_OK;` |
|     16 | 4507 | `}` |
|      - | 4508 | `/*` |
|      - | 4509 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 4510 | ` *  Write a formatted string to a stream.` |
|      - | 4511 | ` * Parameters` |
|      - | 4512 | ` *  $handle` |
|      - | 4513 | ` *   The file pointer.` |
|      - | 4514 | ` *  $format` |
|      - | 4515 | ` *   String format (see sprintf()).` |
|      - | 4516 | ` * Return` |
|      - | 4517 | ` *  The length of the written string.` |
|      - | 4518 | ` */` |
|     16 | 4519 | `static int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4520 | `{` |
|      - | 4521 | `	fprintf_data sFdata;` |
|      - | 4522 | `	const char *zFormat;` |
|      - | 4523 | `	io_private *pDev;` |
|      - | 4524 | `	int nLen;` |
|     17 | 4525 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4526 | `		/* Missing/Invalid arguments,return zero */` |
|    ! 0 | 4527 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4528 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4529 | `		return PH7_OK;` |
|      - | 4530 | `	}` |
|      - | 4531 | `	/* Extract our private data */` |
|     17 | 4532 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4533 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     17 | 4534 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4535 | `		/*Expecting an IO handle */` |
|    ! 0 | 4536 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4537 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4538 | `		return PH7_OK;` |
|      - | 4539 | `	}` |
|      - | 4540 | `	/* Point to the target IO stream device */` |
|     17 | 4541 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4542 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4543 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4544 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4545 | `			);` |
|    ! 0 | 4546 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4547 | `		return PH7_OK;` |
|      - | 4548 | `	}` |
|      - | 4549 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */` |
|      - | 4550 | `	{` |
|     17 | 4551 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|     17 | 4552 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4553 | `			return rcf;` |
|      - | 4554 | `		}` |
|      - | 4555 | `	}` |
|      - | 4556 | `	/* Extract the string format (scalars/null coerce). */` |
|     17 | 4557 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     17 | 4558 | `	if( nLen < 1 ){` |
|      - | 4559 | `		/* Empty string,return zero */` |
|    ! 0 | 4560 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4561 | `		return PH7_OK;` |
|      - | 4562 | `	}` |
|      - | 4563 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4564 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4565 | `	{` |
|     17 | 4566 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     17 | 4567 | `		if( rcv != PH7_OK ){` |
|      3 | 4568 | `			return rcv;` |
|      - | 4569 | `		}` |
|      - | 4570 | `	}` |
|      - | 4571 | `	/* Prepare our private data */` |
|     15 | 4572 | `	sFdata.nCount = 0;` |
|     15 | 4573 | `	sFdata.pIO = pDev;` |
|      - | 4574 | `	/* Format the string */` |
|     15 | 4575 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 4576 | `	/* Return total number of bytes written */` |
|     15 | 4577 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|     15 | 4578 | `	return PH7_OK;` |
|      9 | 4579 | `}` |
|      - | 4580 | `/*` |
|      - | 4581 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 4582 | ` *  Write a formatted string to a stream.` |
|      - | 4583 | ` * Parameters` |
|      - | 4584 | ` *  $handle` |
|      - | 4585 | ` *   The file pointer.` |
|      - | 4586 | ` *  $format` |
|      - | 4587 | ` *   String format (see sprintf()).` |
|      - | 4588 | ` * $args` |
|      - | 4589 | ` *   User arguments.` |
|      - | 4590 | ` * Return` |
|      - | 4591 | ` *  The length of the written string.` |
|      - | 4592 | ` */` |
|      4 | 4593 | `static int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4594 | `{` |
|      - | 4595 | `	fprintf_data sFdata;` |
|      - | 4596 | `	const char *zFormat;` |
|      - | 4597 | `	ph7_hashmap *pMap;` |
|      - | 4598 | `	io_private *pDev;` |
|      - | 4599 | `	SySet sArg;` |
|      - | 4600 | `	int n,nLen;` |
|      5 | 4601 | `	if( nArg < 3 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4602 | `		/* Missing/Invalid arguments,return zero */` |
|      3 | 4603 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|      3 | 4604 | `		ph7_result_int(pCtx,0);` |
|      3 | 4605 | `		return PH7_OK;` |
|      - | 4606 | `	}` |
|      - | 4607 | `	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */` |
|      - | 4608 | `	{` |
|      3 | 4609 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      3 | 4610 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4611 | `			return rcf;` |
|      - | 4612 | `		}` |
|      - | 4613 | `	}` |
|      3 | 4614 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|      - | 4615 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4616 | `		char zBuf[64];` |
|    ! 0 | 4617 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4618 | `			"vfprintf(): Argument #3 ($values) must be of type array, %s given",` |
|    ! 0 | 4619 | `			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|      - | 4620 | `	}` |
|      - | 4621 | `	/* Extract our private data */` |
|      3 | 4622 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4623 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4624 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4625 | `		/*Expecting an IO handle */` |
|    ! 0 | 4626 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4627 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4628 | `		return PH7_OK;` |
|      - | 4629 | `	}` |
|      - | 4630 | `	/* Point to the target IO stream device */` |
|      3 | 4631 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4632 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4633 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4634 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4635 | `			);` |
|    ! 0 | 4636 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4637 | `		return PH7_OK;` |
|      - | 4638 | `	}` |
|      - | 4639 | `	/* Extract the string format */` |
|      3 | 4640 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      3 | 4641 | `	if( nLen < 1 ){` |
|      - | 4642 | `		/* Empty string,return zero */` |
|    ! 0 | 4643 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4644 | `		return PH7_OK;` |
|      - | 4645 | `	}` |
|      - | 4646 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4647 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4648 | `	{` |
|      3 | 4649 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 4650 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4651 | `			return rcv;` |
|      - | 4652 | `		}` |
|      - | 4653 | `	}` |
|      - | 4654 | `	/* Point to hashmap */` |
|      3 | 4655 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 4656 | `	/* Extract arguments from the hashmap */` |
|      3 | 4657 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4658 | `	/* Prepare our private data */` |
|      3 | 4659 | `	sFdata.nCount = 0;` |
|      3 | 4660 | `	sFdata.pIO = pDev;` |
|      - | 4661 | `	/* Format the string */` |
|      3 | 4662 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 4663 | `	/* Return total number of bytes written*/` |
|      3 | 4664 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      3 | 4665 | `	SySetRelease(&sArg);` |
|      3 | 4666 | `	return PH7_OK;` |
|      3 | 4667 | `}` |
|      - | 4668 | `/*` |
|      - | 4669 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 4670 | ` * According to the PHP reference manual:` |
|      - | 4671 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 4672 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 4673 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 4674 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 4675 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 4676 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 4677 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 4678 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 4679 | ` *         exist, attempt to create it.` |
|      - | 4680 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 4681 | ` *          not exist, attempt to create it.` |
|      - | 4682 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 4683 | ` *         already exists,` |
|      - | 4684 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 4685 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 4686 | ` *         the underlying open(2) system call.` |
|      - | 4687 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 4688 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 4689 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 4690 | ` *          is positioned on the beginning of the file.` |
|      - | 4691 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 4692 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 4693 | ` *          be used after the lock is requested).` |
|      - | 4694 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 4695 | ` */` |
|     76 | 4696 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      2 | 4697 | `{` |
|     78 | 4698 | `	const char *zEnd = &zMode[nLen];` |
|     78 | 4699 | `	int iFlag = 0;` |
|      - | 4700 | `	int c;` |
|     78 | 4701 | `	if( nLen < 1 ){` |
|      - | 4702 | `		/* Open in a read-only mode */` |
|    ! 0 | 4703 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 4704 | `	}` |
|     78 | 4705 | `	c = zMode[0];` |
|     78 | 4706 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 4707 | `		/* Read-only access */` |
|     50 | 4708 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|     50 | 4709 | `		zMode++; /* Advance */` |
|     50 | 4710 | `		if( zMode < zEnd ){` |
|     13 | 4711 | `			c = zMode[0];` |
|     13 | 4712 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 4713 | `				/* Read+Write access */` |
|     13 | 4714 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|      6 | 4715 | `			}` |
|      8 | 4716 | `		}` |
|     53 | 4717 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 4718 | `		/* Overwrite mode.` |
|      - | 4719 | `		 * If the file does not exists,try to create it` |
|      - | 4720 | `		 */` |
|     29 | 4721 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     29 | 4722 | `		zMode++; /* Advance */` |
|     29 | 4723 | `		if( zMode < zEnd ){` |
|      5 | 4724 | `			c = zMode[0];` |
|      5 | 4725 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4726 | `				/* Read+Write access */` |
|      5 | 4727 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|      5 | 4728 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|      2 | 4729 | `			}` |
|      3 | 4730 | `		}` |
|     14 | 4731 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 4732 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 4733 | `		 * Create the file if it does not exists.` |
|      - | 4734 | `		 */` |
|    ! 0 | 4735 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4736 | `		zMode++; /* Advance */` |
|    ! 0 | 4737 | `		if( zMode < zEnd ){` |
|    ! 0 | 4738 | `			c = zMode[0];` |
|    ! 0 | 4739 | `			if( c == '+' ){` |
|      - | 4740 | `				/* Read-Write access */` |
|    ! 0 | 4741 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4742 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4743 | `			}` |
|    ! 0 | 4744 | `		}` |
|    ! 0 | 4745 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 4746 | `		/* Exclusive access.` |
|      - | 4747 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 4748 | `		 * Otherwise create a new file.` |
|      - | 4749 | `		 */` |
|    ! 0 | 4750 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|    ! 0 | 4751 | `		zMode++; /* Advance */` |
|    ! 0 | 4752 | `		if( zMode < zEnd ){` |
|    ! 0 | 4753 | `			c = zMode[0];` |
|    ! 0 | 4754 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4755 | `				/* Read-Write access */` |
|    ! 0 | 4756 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4757 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4758 | `			}` |
|    ! 0 | 4759 | `		}` |
|    ! 0 | 4760 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 4761 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 4762 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4763 | `		zMode++; /* Advance */` |
|    ! 0 | 4764 | `		if( zMode < zEnd ){` |
|    ! 0 | 4765 | `			c = zMode[0];` |
|    ! 0 | 4766 | `			if( c == '+' ){` |
|      - | 4767 | `				/* Read-Write access */` |
|    ! 0 | 4768 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4769 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4770 | `			}` |
|    ! 0 | 4771 | `		}` |
|    ! 0 | 4772 | `	}else{` |
|      - | 4773 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 4774 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 4775 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 4776 | `	}` |
|     94 | 4777 | `	while( zMode < zEnd ){` |
|     17 | 4778 | `		c = zMode[0];` |
|     17 | 4779 | `		if( c == 'b' \|\| c == 'B' ){` |
|    ! 0 | 4780 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4781 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|     17 | 4782 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 4783 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 4784 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4785 | `		}` |
|     17 | 4786 | `		zMode++;` |
|      1 | 4787 | `	}` |
|     78 | 4788 | `	return iFlag;` |
|     40 | 4789 | `}` |
|      - | 4790 | `/*` |
|      - | 4791 | ` * Initialize the IO private structure.` |
|      - | 4792 | ` */` |
|   5092 | 4793 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      5 | 4794 | `{` |
|   5097 | 4795 | `	pOut->pStream = pStream;` |
|   5097 | 4796 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   5097 | 4797 | `	pOut->nOfft = 0;` |
|      - | 4798 | `	/* Set the magic number */` |
|   5097 | 4799 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   5097 | 4800 | `}` |
|      - | 4801 | `/*` |
|      - | 4802 | ` * Release the IO private structure.` |
|      - | 4803 | ` */` |
|   5056 | 4804 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      5 | 4805 | `{` |
|   5061 | 4806 | `	SyBlobRelease(&pDev->sBuffer);` |
|   5061 | 4807 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 4808 | `	/* Release the whole structure */` |
|   5061 | 4809 | `	ph7_context_free_chunk(pCtx,pDev);` |
|   5061 | 4810 | `}` |
|      - | 4811 | `/*` |
|      - | 4812 | ` * Reset the IO private structure.` |
|      - | 4813 | ` */` |
|     28 | 4814 | `static void ResetIOPrivate(io_private *pDev)` |
|      1 | 4815 | `{` |
|     29 | 4816 | `	SyBlobReset(&pDev->sBuffer);` |
|     29 | 4817 | `	pDev->nOfft = 0;` |
|     29 | 4818 | `}` |
|      - | 4819 | `/* Forward declaration */` |
|      - | 4820 |  |
|      - | 4821 | `/*` |
|      - | 4822 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 4823 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 4824 | ` * Parameters` |
|      - | 4825 | ` *  $filename` |
|      - | 4826 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 4827 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 4828 | ` *   then a regular file is assumed.` |
|      - | 4829 | ` *  $mode` |
|      - | 4830 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 4831 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 4832 | ` *   modes.` |
|      - | 4833 | ` *  $use_include_path` |
|      - | 4834 | ` *   You can use the optional second parameter and set it to` |
|      - | 4835 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 4836 | ` *  $context` |
|      - | 4837 | ` *   A context stream resource.` |
|      - | 4838 | ` * Return` |
|      - | 4839 | ` *  File handle on success or FALSE on failure.` |
|      - | 4840 | ` */` |
|      - | 4841 | `/*` |
|      - | 4842 | ` * string\|false stream_get_contents(resource $stream, int $maxLength = -1,` |
|      - | 4843 | ` *                                  int $offset = -1)` |
|      - | 4844 | ` *  Read the remaining contents of a stream into a string.` |
|      - | 4845 | ` */` |
|      8 | 4846 | `static int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4847 | `{` |
|      - | 4848 | `	const ph7_io_stream *pStream;` |
|      - | 4849 | `	io_private *pDev;` |
|      9 | 4850 | `	ph7_int64 nMax = -1;` |
|      - | 4851 | `	char zBuf[4096];` |
|      - | 4852 | `	ph7_int64 nRead;` |
|      9 | 4853 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 4854 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4855 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4856 | `		return PH7_OK;` |
|      - | 4857 | `	}` |
|      9 | 4858 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      9 | 4859 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 4860 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4861 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4862 | `		return PH7_OK;` |
|      - | 4863 | `	}` |
|      9 | 4864 | `	pStream = pDev->pStream;` |
|      9 | 4865 | `	if( pStream == 0 \|\| pStream->xRead == 0 ){` |
|    ! 0 | 4866 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4867 | `		return PH7_OK;` |
|      - | 4868 | `	}` |
|      9 | 4869 | `	if( nArg > 1 ){` |
|      5 | 4870 | `		nMax = ph7_value_to_int64(apArg[1]);` |
|      2 | 4871 | `	}` |
|      9 | 4872 | `	if( nArg > 2 ){` |
|      5 | 4873 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|      5 | 4874 | `		if( iOfft >= 0 && pStream->xSeek ){` |
|      5 | 4875 | `			pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/);` |
|      2 | 4876 | `		}` |
|      2 | 4877 | `	}` |
|      9 | 4878 | `	ph7_result_string(pCtx,"",0); /* seed an empty string result */` |
|     17 | 4879 | `	while( nMax != 0 ){` |
|     15 | 4880 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|     15 | 4881 | `		if( nMax > 0 && nMax < nAsk ){` |
|      3 | 4882 | `			nAsk = nMax;` |
|      1 | 4883 | `		}` |
|     15 | 4884 | `		nRead = pStream->xRead(pDev->pHandle,zBuf,nAsk);` |
|     15 | 4885 | `		if( nRead < 1 ){` |
|      7 | 4886 | `			break;` |
|      - | 4887 | `		}` |
|      9 | 4888 | `		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */` |
|      9 | 4889 | `		if( nMax > 0 ){` |
|      3 | 4890 | `			nMax -= nRead;` |
|      1 | 4891 | `		}` |
|      1 | 4892 | `	}` |
|      9 | 4893 | `	return PH7_OK;` |
|      5 | 4894 | `}` |
|      - | 4895 | `/*` |
|      - | 4896 | ` * array stream_get_wrappers(void) — names of the registered stream devices.` |
|      - | 4897 | ` */` |
|      2 | 4898 | `static int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4899 | `{` |
|      - | 4900 | `	ph7_value *pArr,*pV;` |
|      - | 4901 | `	ph7_io_stream **apDev;` |
|      - | 4902 | `	sxu32 n;` |
|      1 | 4903 | `	SXUNUSED(nArg);` |
|      1 | 4904 | `	SXUNUSED(apArg);` |
|      3 | 4905 | `	pArr = ph7_context_new_array(pCtx);` |
|      3 | 4906 | `	pV = ph7_context_new_scalar(pCtx);` |
|      3 | 4907 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 4908 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4909 | `		return PH7_OK;` |
|      - | 4910 | `	}` |
|      3 | 4911 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|      9 | 4912 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|      7 | 4913 | `		ph7_value_string(pV,apDev[n]->zName,-1);` |
|      7 | 4914 | `		ph7_array_add_elem(pArr,0,pV);` |
|      7 | 4915 | `		ph7_value_reset_string_cursor(pV);` |
|      4 | 4916 | `	}` |
|      3 | 4917 | `	ph7_result_value(pCtx,pArr);` |
|      3 | 4918 | `	return PH7_OK;` |
|      2 | 4919 | `}` |
|      - | 4920 | `/*` |
|      - | 4921 | ` * array stream_get_meta_data(resource $stream) — best-effort php shape over` |
|      - | 4922 | ` * the io_private state (uri/wrapper_type/seekable/eof; recorded approximation).` |
|      - | 4923 | ` */` |
|      2 | 4924 | `static int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4925 | `{` |
|      - | 4926 | `	io_private *pDev;` |
|      - | 4927 | `	ph7_value *pArr,*pV;` |
|      3 | 4928 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 4929 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4930 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4931 | `		return PH7_OK;` |
|      - | 4932 | `	}` |
|      3 | 4933 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      3 | 4934 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 4935 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4936 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4937 | `		return PH7_OK;` |
|      - | 4938 | `	}` |
|      3 | 4939 | `	pArr = ph7_context_new_array(pCtx);` |
|      3 | 4940 | `	pV = ph7_context_new_scalar(pCtx);` |
|      3 | 4941 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 4942 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4943 | `		return PH7_OK;` |
|      - | 4944 | `	}` |
|      3 | 4945 | `	ph7_value_bool(pV,0);` |
|      3 | 4946 | `	ph7_array_add_strkey_elem(pArr,"timed_out",pV);` |
|      3 | 4947 | `	ph7_value_bool(pV,1);` |
|      3 | 4948 | `	ph7_array_add_strkey_elem(pArr,"blocked",pV);` |
|      - | 4949 | `	/* eof is best-effort: a read probe would consume state on unseekable` |
|      - | 4950 | `	 * devices, so report FALSE and let feof() answer properly */` |
|      3 | 4951 | `	ph7_value_bool(pV,0);` |
|      3 | 4952 | `	ph7_array_add_strkey_elem(pArr,"eof",pV);` |
|      3 | 4953 | `	ph7_value_int(pV,0);` |
|      3 | 4954 | `	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);` |
|      3 | 4955 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 4956 | `	ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);` |
|      3 | 4957 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 4958 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 4959 | `	ph7_array_add_strkey_elem(pArr,"stream_type",pV);` |
|      3 | 4960 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 4961 | `	ph7_value_bool(pV,pDev->pStream && pDev->pStream->xSeek != 0);` |
|      3 | 4962 | `	ph7_array_add_strkey_elem(pArr,"seekable",pV);` |
|      3 | 4963 | `	ph7_result_value(pCtx,pArr);` |
|      3 | 4964 | `	return PH7_OK;` |
|      2 | 4965 | `}` |
|      - | 4966 | `/*` |
|      - | 4967 | ` * stream_context_create([array $options[, array $params]]) — INERT: PHL has` |
|      - | 4968 | ` * no context plumbing yet; the options array itself is returned so code that` |
|      - | 4969 | ` * creates and passes contexts keeps working (recorded divergence: not a` |
|      - | 4970 | ` * resource, options unconsumed).` |
|      - | 4971 | ` */` |
|      2 | 4972 | `static int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4973 | `{` |
|      3 | 4974 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|      3 | 4975 | `		ph7_result_value(pCtx,apArg[0]);` |
|      2 | 4976 | `	}else{` |
|    ! 0 | 4977 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 4978 | `		if( pArr == 0 ){` |
|    ! 0 | 4979 | `			ph7_result_null(pCtx);` |
|    ! 0 | 4980 | `			return PH7_OK;` |
|      - | 4981 | `		}` |
|    ! 0 | 4982 | `		ph7_result_value(pCtx,pArr);` |
|      - | 4983 | `	}` |
|      3 | 4984 | `	return PH7_OK;` |
|      2 | 4985 | `}` |
|     76 | 4986 | `static int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4987 | `{` |
|      - | 4988 | `	const ph7_io_stream *pStream;` |
|      - | 4989 | `	const char *zUri,*zMode;` |
|      - | 4990 | `	ph7_value *pResource;` |
|      - | 4991 | `	io_private *pDev;` |
|      - | 4992 | `	int iLen,imLen;` |
|      - | 4993 | `	int iOpenFlags;` |
|     78 | 4994 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4995 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4996 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 4997 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4998 | `		return PH7_OK;` |
|      - | 4999 | `	}` |
|      - | 5000 | `	/* Extract the URI and the desired access mode */` |
|     78 | 5001 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|     78 | 5002 | `	if( nArg > 1 ){` |
|     78 | 5003 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|     40 | 5004 | `	}else{` |
|      - | 5005 | `		/* Set a default read-only mode */` |
|    ! 0 | 5006 | `		zMode = "r";` |
|    ! 0 | 5007 | `		imLen = (int)sizeof(char);` |
|      - | 5008 | `	}` |
|      - | 5009 | `	/* Try to extract a stream */` |
|     78 | 5010 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|     78 | 5011 | `	if( pStream == 0 ){` |
|    ! 0 | 5012 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5013 | `			"No stream device is associated with the given URI(%s)",zUri);` |
|    ! 0 | 5014 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5015 | `		return PH7_OK;` |
|      - | 5016 | `	}` |
|      - | 5017 | `	/* Allocate a new IO private instance */` |
|     78 | 5018 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     78 | 5019 | `	if( pDev == 0 ){` |
|    ! 0 | 5020 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5021 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5022 | `		return PH7_OK;` |
|      - | 5023 | `	}` |
|     78 | 5024 | `	pResource = 0;` |
|     78 | 5025 | `	if( nArg > 3 ){` |
|    ! 0 | 5026 | `		pResource = apArg[3];` |
|     78 | 5027 | `	}else if( is_php_stream(pStream) \|\| is_data_stream(pStream) ){` |
|      - | 5028 | `		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to` |
|      - | 5029 | `		 * access the underlying virtual machine.` |
|      - | 5030 | `		 */` |
|     15 | 5031 | `		pResource = apArg[0];` |
|      7 | 5032 | `	}` |
|      - | 5033 | `	/* Initialize the structure */` |
|     78 | 5034 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 5035 | `	/* Convert open mode to PH7 flags */` |
|     78 | 5036 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|      - | 5037 | `	/* Try to get a handle */` |
|    116 | 5038 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|     38 | 5039 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);` |
|     78 | 5040 | `	if( pDev->pHandle == 0 ){` |
|    ! 0 | 5041 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zUri);` |
|    ! 0 | 5042 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5043 | `		ph7_context_free_chunk(pCtx,pDev);` |
|    ! 0 | 5044 | `		return PH7_OK;` |
|      - | 5045 | `	}` |
|      - | 5046 | `	/* All done,return the io_private instance as a resource */` |
|     78 | 5047 | `	ph7_result_resource(pCtx,pDev);` |
|     78 | 5048 | `	return PH7_OK;` |
|     40 | 5049 | `}` |
|      - | 5050 | `/*` |
|      - | 5051 | ` * bool fclose(resource $handle)` |
|      - | 5052 | ` *  Closes an open file pointer` |
|      - | 5053 | ` * Parameters` |
|      - | 5054 | ` *  $handle` |
|      - | 5055 | ` *   The file pointer.` |
|      - | 5056 | ` * Return` |
|      - | 5057 | ` *  TRUE on success or FALSE on failure.` |
|      - | 5058 | ` */` |
|    162 | 5059 | `static int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5060 | `{` |
|      - | 5061 | `	const ph7_io_stream *pStream;` |
|      - | 5062 | `	io_private *pDev;` |
|      - | 5063 | `	ph7_vm *pVm;` |
|    167 | 5064 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5065 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5066 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5067 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5068 | `		return PH7_OK;` |
|      - | 5069 | `	}` |
|      - | 5070 | `	/* Extract our private data */` |
|    167 | 5071 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 5072 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    167 | 5073 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 5074 | `		/*Expecting an IO handle */` |
|    ! 0 | 5075 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5076 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5077 | `		return PH7_OK;` |
|      - | 5078 | `	}` |
|      - | 5079 | `	/* Point to the target IO stream device */` |
|    167 | 5080 | `	pStream = pDev->pStream;` |
|    167 | 5081 | `	if( pStream == 0 ){` |
|    ! 0 | 5082 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5083 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 5084 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 5085 | `			);` |
|    ! 0 | 5086 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5087 | `		return PH7_OK;` |
|      - | 5088 | `	}` |
|      - | 5089 | `	/* Point to the VM that own this context */` |
|    167 | 5090 | `	pVm = pCtx->pVm;` |
|      - | 5091 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|    167 | 5092 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 5093 | `		/* Perform the requested operation */` |
|    167 | 5094 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 5095 | `		/* Release the IO private structure */` |
|    167 | 5096 | `		ReleaseIOPrivate(pCtx,pDev);` |
|      - | 5097 | `		/* Invalidate the resource handle */` |
|    167 | 5098 | `		ph7_value_release(apArg[0]);` |
|     81 | 5099 | `	}` |
|      - | 5100 | `	/* Return TRUE */` |
|    167 | 5101 | `	ph7_result_bool(pCtx,1);` |
|    167 | 5102 | `	return PH7_OK;` |
|     86 | 5103 | `}` |
|      - | 5104 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5105 | `/*` |
|      - | 5106 | ` * MD5/SHA1 digest consumer.` |
|      - | 5107 | ` */` |
|     72 | 5108 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 5109 | `{` |
|      - | 5110 | `	/* Append hex chunk verbatim */` |
|     73 | 5111 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     73 | 5112 | `	return SXRET_OK;` |
|      1 | 5113 | `}` |
|      - | 5114 | `/*` |
|      - | 5115 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 5116 | ` *  Calculates the md5 hash of a given file.` |
|      - | 5117 | ` * Parameters` |
|      - | 5118 | ` *  $uri` |
|      - | 5119 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 5120 | ` *  $raw_output` |
|      - | 5121 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 5122 | ` * Return` |
|      - | 5123 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 5124 | ` */` |
|      2 | 5125 | `static int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5126 | `{` |
|      - | 5127 | `	const ph7_io_stream *pStream;` |
|      - | 5128 | `	unsigned char zDigest[16];` |
|      3 | 5129 | `	int raw_output  = FALSE;` |
|      - | 5130 | `	const char *zFile;` |
|      - | 5131 | `	MD5Context sCtx;` |
|      - | 5132 | `	char zBuf[8192];` |
|      - | 5133 | `	void *pHandle;` |
|      - | 5134 | `	ph7_int64 n;` |
|      - | 5135 | `	int nLen;` |
|      3 | 5136 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5137 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5138 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5139 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5140 | `		return PH7_OK;` |
|      - | 5141 | `	}` |
|      - | 5142 | `	/* Extract the file path */` |
|      3 | 5143 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5144 | `	/* Point to the target IO stream device */` |
|      3 | 5145 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5146 | `	if( pStream == 0 ){` |
|    ! 0 | 5147 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5148 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5149 | `		return PH7_OK;` |
|      - | 5150 | `	}` |
|      3 | 5151 | `	if( nArg > 1 ){` |
|    ! 0 | 5152 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 5153 | `	}` |
|      - | 5154 | `	/* Try to open the file in read-only mode */` |
|      3 | 5155 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5156 | `	if( pHandle == 0 ){` |
|    ! 0 | 5157 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5158 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5159 | `		return PH7_OK;` |
|      - | 5160 | `	}` |
|      - | 5161 | `	/* Init the MD5 context */` |
|      3 | 5162 | `	MD5Init(&sCtx);` |
|      - | 5163 | `	/* Perform the requested operation */` |
|      2 | 5164 | `	for(;;){` |
|      5 | 5165 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5166 | `		if( n < 1 ){` |
|      - | 5167 | `			/* EOF or IO error,break immediately */` |
|      3 | 5168 | `			break;` |
|      - | 5169 | `		}` |
|      3 | 5170 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5171 | `	}` |
|      - | 5172 | `	/* Close the stream */` |
|      3 | 5173 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5174 | `	/* Extract the digest */` |
|      3 | 5175 | `	MD5Final(zDigest,&sCtx);` |
|      3 | 5176 | `	if( raw_output ){` |
|      - | 5177 | `		/* Output raw digest */` |
|    ! 0 | 5178 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5179 | `	}else{` |
|      - | 5180 | `		/* Perform a binary to hex conversion */` |
|      3 | 5181 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5182 | `	}` |
|      3 | 5183 | `	return PH7_OK;` |
|      2 | 5184 | `}` |
|      - | 5185 | `/*` |
|      - | 5186 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 5187 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 5188 | ` * Parameters` |
|      - | 5189 | ` *  $uri` |
|      - | 5190 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 5191 | ` *  $raw_output` |
|      - | 5192 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 5193 | ` * Return` |
|      - | 5194 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 5195 | ` */` |
|      2 | 5196 | `static int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5197 | `{` |
|      - | 5198 | `	const ph7_io_stream *pStream;` |
|      - | 5199 | `	unsigned char zDigest[20];` |
|      3 | 5200 | `	int raw_output  = FALSE;` |
|      - | 5201 | `	const char *zFile;` |
|      - | 5202 | `	SHA1Context sCtx;` |
|      - | 5203 | `	char zBuf[8192];` |
|      - | 5204 | `	void *pHandle;` |
|      - | 5205 | `	ph7_int64 n;` |
|      - | 5206 | `	int nLen;` |
|      3 | 5207 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5208 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5209 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5210 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5211 | `		return PH7_OK;` |
|      - | 5212 | `	}` |
|      - | 5213 | `	/* Extract the file path */` |
|      3 | 5214 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5215 | `	/* Point to the target IO stream device */` |
|      3 | 5216 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5217 | `	if( pStream == 0 ){` |
|    ! 0 | 5218 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5219 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5220 | `		return PH7_OK;` |
|      - | 5221 | `	}` |
|      3 | 5222 | `	if( nArg > 1 ){` |
|    ! 0 | 5223 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 5224 | `	}` |
|      - | 5225 | `	/* Try to open the file in read-only mode */` |
|      3 | 5226 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5227 | `	if( pHandle == 0 ){` |
|    ! 0 | 5228 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5229 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5230 | `		return PH7_OK;` |
|      - | 5231 | `	}` |
|      - | 5232 | `	/* Init the SHA1 context */` |
|      3 | 5233 | `	SHA1Init(&sCtx);` |
|      - | 5234 | `	/* Perform the requested operation */` |
|      2 | 5235 | `	for(;;){` |
|      5 | 5236 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5237 | `		if( n < 1 ){` |
|      - | 5238 | `			/* EOF or IO error,break immediately */` |
|      3 | 5239 | `			break;` |
|      - | 5240 | `		}` |
|      3 | 5241 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5242 | `	}` |
|      - | 5243 | `	/* Close the stream */` |
|      3 | 5244 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5245 | `	/* Extract the digest */` |
|      3 | 5246 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 5247 | `	if( raw_output ){` |
|      - | 5248 | `		/* Output raw digest */` |
|    ! 0 | 5249 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5250 | `	}else{` |
|      - | 5251 | `		/* Perform a binary to hex conversion */` |
|      3 | 5252 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5253 | `	}` |
|      3 | 5254 | `	return PH7_OK;` |
|      2 | 5255 | `}` |
|      - | 5256 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5257 | `/*` |
|      - | 5258 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 5259 | ` *  Parse a configuration file.` |
|      - | 5260 | ` * Parameters` |
|      - | 5261 | ` * $filename` |
|      - | 5262 | ` *  The filename of the ini file being parsed.` |
|      - | 5263 | ` * $process_sections` |
|      - | 5264 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 5265 | ` *  with the section names and settings included.` |
|      - | 5266 | ` *  The default for process_sections is FALSE.` |
|      - | 5267 | ` * $scanner_mode` |
|      - | 5268 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 5269 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 5270 | ` * Return` |
|      - | 5271 | ` *  The settings are returned as an associative array on success.` |
|      - | 5272 | ` *  Otherwise is returned.` |
|      - | 5273 | ` */` |
|      2 | 5274 | `static int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5275 | `{` |
|      - | 5276 | `	const ph7_io_stream *pStream;` |
|      - | 5277 | `	const char *zFile;` |
|      - | 5278 | `	SyBlob sContents;` |
|      - | 5279 | `	void *pHandle;` |
|      - | 5280 | `	int nLen;` |
|      3 | 5281 | `	sxi32 rc = PH7_OK;` |
|      3 | 5282 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5283 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5284 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5285 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5286 | `		return PH7_OK;` |
|      - | 5287 | `	}` |
|      - | 5288 | `	/* Extract the file path */` |
|      3 | 5289 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5290 | `	/* Point to the target IO stream device */` |
|      3 | 5291 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5292 | `	if( pStream == 0 ){` |
|    ! 0 | 5293 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5294 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5295 | `		return PH7_OK;` |
|      - | 5296 | `	}` |
|      - | 5297 | `	/* Try to open the file in read-only mode */` |
|      3 | 5298 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5299 | `	if( pHandle == 0 ){` |
|    ! 0 | 5300 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5301 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5302 | `		return PH7_OK;` |
|      - | 5303 | `	}` |
|      3 | 5304 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 5305 | `	/* Read the whole file */` |
|      3 | 5306 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      3 | 5307 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 5308 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 5309 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5310 | `	}else{` |
|      - | 5311 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|      5 | 5312 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      2 | 5313 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0);` |
|      - | 5314 | `	}` |
|      - | 5315 | `	/* Close the stream */` |
|      3 | 5316 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5317 | `	/* Release the working buffer */` |
|      3 | 5318 | `	SyBlobRelease(&sContents);` |
|      - | 5319 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      3 | 5320 | `	return rc;` |
|      2 | 5321 | `}` |
|      - | 5322 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 5323 | `#else /* PH7_DISABLE_DISK_IO */` |
|      - | 5324 | `/*` |
|      - | 5325 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|      - | 5326 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|      - | 5327 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|      - | 5328 | ` * to any non-VFS resource).` |
|      - | 5329 | ` */` |
|      - | 5330 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      - | 5331 | `{` |
|      - | 5332 | `	SXUNUSED(pResource);` |
|      - | 5333 | `	return "Unknown";` |
|      - | 5334 | `}` |
|      - | 5335 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 5336 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 5337 | `#if defined(_MSC_VER)` |
|      - | 5338 | `static const ph7_vfs null_vfs = {` |
|      - | 5339 | `#else` |
|      - | 5340 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 5341 | `#endif` |
|      - | 5342 | `	"null_vfs",` |
|      - | 5343 | `	PH7_VFS_VERSION,` |
|      - | 5344 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 5345 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 5346 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 5347 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 5348 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 5349 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 5350 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 5351 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 5352 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 5353 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 5354 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 5355 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 5356 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 5357 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 5358 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 5359 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 5360 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 5361 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 5362 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 5363 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 5364 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 5365 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 5366 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 5367 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 5368 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 5369 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 5370 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 5371 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 5372 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 5373 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 5374 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 5375 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 5376 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 5377 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 5378 | `	0, /* int (*xUmask)(int) */` |
|      - | 5379 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 5380 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 5381 | `	0, /* int (*xUid)(void) */` |
|      - | 5382 | `	0, /* int (*xGid)(void) */` |
|      - | 5383 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 5384 | `	0  /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 5385 | `};` |
|      - | 5386 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 5387 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 5388 | `/*` |
|      - | 5389 | ` * Export the builtin vfs.` |
|      - | 5390 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 5391 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 5392 | ` * Note:` |
|      - | 5393 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 5394 | ` * Note:` |
|      - | 5395 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 5396 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 5397 | ` */` |
|   3950 | 5398 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 5399 | `{` |
|      - | 5400 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 5401 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 5402 | `	return &null_vfs;` |
|      - | 5403 | `#else` |
|      - | 5404 | `#ifdef __WINNT__` |
|      5 | 5405 | `	return &sWinVfs;` |
|      - | 5406 | `#elif defined(__UNIXES__)` |
|   3950 | 5407 | `	return &sUnixVfs;` |
|      - | 5408 | `#else` |
|      - | 5409 | `	return &null_vfs;` |
|      - | 5410 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 5411 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 5412 | `#else` |
|      - | 5413 | `	return &null_vfs;` |
|      - | 5414 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 5415 | `}` |
|      - | 5416 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 5417 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 5418 | `/*` |
|      - | 5419 | ` * The following defines are mostly used by the UNIX built and have` |
|      - | 5420 | ` * no particular meaning on windows.` |
|      - | 5421 | ` */` |
|      - | 5422 | `#ifndef STDIN_FILENO` |
|      - | 5423 | `#define STDIN_FILENO	0` |
|      - | 5424 | `#endif` |
|      - | 5425 | `#ifndef STDOUT_FILENO` |
|      - | 5426 | `#define STDOUT_FILENO	1` |
|      - | 5427 | `#endif` |
|      - | 5428 | `#ifndef STDERR_FILENO` |
|      - | 5429 | `#define STDERR_FILENO	2` |
|      - | 5430 | `#endif` |
|      - | 5431 | `/*` |
|      - | 5432 | ` * php:// Accessing various I/O streams` |
|      - | 5433 | ` * According to the PHP langage reference manual` |
|      - | 5434 | ` * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input` |
|      - | 5435 | ` * and output streams, the standard input, output and error file descriptors.` |
|      - | 5436 | ` * php://stdin, php://stdout and php://stderr:` |
|      - | 5437 | ` *  Allow direct access to the corresponding input or output stream of the PHP process.` |
|      - | 5438 | ` *  The stream references a duplicate file descriptor, so if you open php://stdin and later` |
|      - | 5439 | ` *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.` |
|      - | 5440 | ` *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.` |
|      - | 5441 | ` * php://output` |
|      - | 5442 | ` *  php://output is a write-only stream that allows you to write to the output buffer` |
|      - | 5443 | ` *  mechanism in the same way as print and echo.` |
|      - | 5444 | ` */` |
|      - | 5445 | `typedef struct ph7_stream_data ph7_stream_data;` |
|      - | 5446 | `/* Supported IO streams */` |
|      - | 5447 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|      - | 5448 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|      - | 5449 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|      - | 5450 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|      - | 5451 | `#define PH7_IO_STREAM_MEMORY 5 /* php://memory, php://temp, and data:// payloads */` |
|      - | 5452 | ` /* The following structure is the private data associated with the php:// stream */` |
|      - | 5453 | `struct ph7_stream_data` |
|      - | 5454 | `{` |
|      - | 5455 | `	ph7_vm *pVm; /* VM that own this instance */` |
|      - | 5456 | `	int iType;   /* Stream type */` |
|      - | 5457 | `	union{` |
|      - | 5458 | `		void *pHandle; /* Stream handle */` |
|      - | 5459 | `		ph7_output_consumer sConsumer; /* VM output consumer */` |
|      - | 5460 | `	}x;` |
|      - | 5461 | `	SyBlob sMem;     /* MEMORY type: backing buffer */` |
|      - | 5462 | `	sxu32 nCur;      /* MEMORY type: read/write cursor */` |
|      - | 5463 | `	int bReadOnly;   /* MEMORY type: TRUE for data:// payloads */` |
|      - | 5464 | `};` |
|      - | 5465 | `/*` |
|      - | 5466 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|      - | 5467 | ` */` |
|     26 | 5468 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|      1 | 5469 | `{` |
|      - | 5470 | `	ph7_stream_data *pData;` |
|     27 | 5471 | `	if( pVm == 0 ){` |
|    ! 0 | 5472 | `		return 0;` |
|      - | 5473 | `	}` |
|      - | 5474 | `	/* Allocate a new instance */` |
|     27 | 5475 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|     27 | 5476 | `	if( pData == 0 ){` |
|    ! 0 | 5477 | `		return 0;` |
|      - | 5478 | `	}` |
|      - | 5479 | `	/* Zero the structure */` |
|     27 | 5480 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|      - | 5481 | `	/* Initialize fields */` |
|     27 | 5482 | `	pData->iType = iType;` |
|     27 | 5483 | `	SyBlobInit(&pData->sMem,&pVm->sAllocator);` |
|     27 | 5484 | `	pData->nCur = 0;` |
|     27 | 5485 | `	pData->bReadOnly = 0;` |
|     27 | 5486 | `	if( iType == PH7_IO_STREAM_MEMORY ){` |
|      - | 5487 | `		/* Nothing else to set up: the buffer is the stream */` |
|     18 | 5488 | `	}else if( iType == PH7_IO_STREAM_OUTPUT ){` |
|      - | 5489 | `		/* Point to the default VM consumer routine. */` |
|      3 | 5490 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|      2 | 5491 | `	}else{` |
|      - | 5492 | `#ifdef __WINNT__` |
|      - | 5493 | `		DWORD nChannel;` |
|      1 | 5494 | `		switch(iType){` |
|      1 | 5495 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|      1 | 5496 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|      - | 5497 | `		default:` |
|      1 | 5498 | `			nChannel = STD_INPUT_HANDLE;` |
|      - | 5499 | `			break;` |
|      - | 5500 | `		}` |
|      1 | 5501 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|      - | 5502 | `#else` |
|      - | 5503 | `		/* Assume an UNIX system */` |
|      6 | 5504 | `		int ifd = STDIN_FILENO;` |
|      6 | 5505 | `		switch(iType){` |
|      2 | 5506 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|      2 | 5507 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|      1 | 5508 | `		default:` |
|      2 | 5509 | `			break;` |
|      - | 5510 | `		}` |
|      6 | 5511 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|      - | 5512 | `#endif` |
|      - | 5513 | `	}` |
|     27 | 5514 | `	pData->pVm = pVm;` |
|     27 | 5515 | `	return pData;` |
|     14 | 5516 | `}` |
|      - | 5517 | `/*` |
|      - | 5518 | ` * Implementation of the php:// IO streams routines` |
|      - | 5519 | ` * Status:` |
|      - | 5520 | ` *   Stable.` |
|      - | 5521 | ` */` |
|      - | 5522 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|     10 | 5523 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 5524 | `{` |
|      - | 5525 | `	ph7_stream_data *pData;` |
|      - | 5526 | `	SyString sStream;` |
|     11 | 5527 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|      - | 5528 | `	/* Trim leading and trailing white spaces */` |
|     11 | 5529 | `	SyStringFullTrim(&sStream);` |
|      - | 5530 | `	/* Stream to open */` |
|     11 | 5531 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|    ! 0 | 5532 | `		iMode = PH7_IO_STREAM_STDIN;` |
|     11 | 5533 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|      3 | 5534 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|     10 | 5535 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|    ! 0 | 5536 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|      9 | 5537 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|    ! 0 | 5538 | `		iMode = PH7_IO_STREAM_STDERR;` |
|      8 | 5539 | `	}else if( SyStrnicmp(sStream.zString,"memory",sizeof("memory")-1) == 0` |
|      6 | 5540 | `	       \|\| SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0 ){` |
|      - | 5541 | `		/* php://memory and php://temp (PHL keeps temp fully in memory —` |
|      - | 5542 | `		 * php's 2MB disk spill is a memory-pressure detail, recorded) */` |
|      9 | 5543 | `		iMode = PH7_IO_STREAM_MEMORY;` |
|      5 | 5544 | `	}else{` |
|      - | 5545 | `		/* unknown stream name */` |
|    ! 0 | 5546 | `		return -1;` |
|      - | 5547 | `	}` |
|      - | 5548 | `	/* Create our handle */` |
|     11 | 5549 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|     11 | 5550 | `	if( pData == 0 ){` |
|    ! 0 | 5551 | `		return -1;` |
|      - | 5552 | `	}` |
|      - | 5553 | `	/* Make the handle public */` |
|     11 | 5554 | `	*ppHandle = (void *)pData;` |
|     11 | 5555 | `	return PH7_OK;` |
|      6 | 5556 | `}` |
|      - | 5557 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|     42 | 5558 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|      1 | 5559 | `{` |
|     43 | 5560 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     43 | 5561 | `	if( pData == 0 ){` |
|    ! 0 | 5562 | `		return -1;` |
|      - | 5563 | `	}` |
|     43 | 5564 | `	if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|     43 | 5565 | `		sxu32 nAvail = SyBlobLength(&pData->sMem);` |
|      - | 5566 | `		sxu32 nRead;` |
|     43 | 5567 | `		if( pData->nCur >= nAvail ){` |
|     15 | 5568 | `			return 0; /* EOF */` |
|      - | 5569 | `		}` |
|     29 | 5570 | `		nRead = nAvail - pData->nCur;` |
|     29 | 5571 | `		if( (ph7_int64)nRead > nDatatoRead ){` |
|      7 | 5572 | `			nRead = (sxu32)nDatatoRead;` |
|      3 | 5573 | `		}` |
|     29 | 5574 | `		SyMemcpy((const char *)SyBlobData(&pData->sMem) + pData->nCur,pBuffer,nRead);` |
|     29 | 5575 | `		pData->nCur += nRead;` |
|     29 | 5576 | `		return (ph7_int64)nRead;` |
|      - | 5577 | `	}` |
|    ! 0 | 5578 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|      - | 5579 | `		/* Forbidden */` |
|    ! 0 | 5580 | `		return -1;` |
|      - | 5581 | `	}` |
|      - | 5582 | `#ifdef __WINNT__` |
|      - | 5583 | `	{` |
|      - | 5584 | `		DWORD nRd;` |
|      - | 5585 | `		BOOL rc;` |
|    ! 0 | 5586 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    ! 0 | 5587 | `		if( !rc ){` |
|      - | 5588 | `			/* IO error */` |
|    ! 0 | 5589 | `			return -1;` |
|      - | 5590 | `		}` |
|    ! 0 | 5591 | `		return (ph7_int64)nRd;` |
|      - | 5592 | `	}` |
|      - | 5593 | `#elif defined(__UNIXES__)` |
|      - | 5594 | `	{` |
|      - | 5595 | `		ssize_t nRd;` |
|      - | 5596 | `		int fd;` |
|    ! 0 | 5597 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 5598 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|    ! 0 | 5599 | `		if( nRd < 1 ){` |
|    ! 0 | 5600 | `			return -1;` |
|      - | 5601 | `		}` |
|    ! 0 | 5602 | `		return (ph7_int64)nRd;` |
|      - | 5603 | `	}` |
|      - | 5604 | `#else` |
|      - | 5605 | `	return -1;` |
|      - | 5606 | `#endif` |
|     22 | 5607 | `}` |
|      - | 5608 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|     12 | 5609 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 5610 | `{` |
|     13 | 5611 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     13 | 5612 | `	if( pData == 0 ){` |
|    ! 0 | 5613 | `		return -1;` |
|      - | 5614 | `	}` |
|     13 | 5615 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|      - | 5616 | `		/* Forbidden */` |
|    ! 0 | 5617 | `		return -1;` |
|     13 | 5618 | `	}else if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|      - | 5619 | `		sxu32 nLen,nEnd;` |
|     11 | 5620 | `		if( pData->bReadOnly ){` |
|    ! 0 | 5621 | `			return -1;` |
|      - | 5622 | `		}` |
|     11 | 5623 | `		nLen = SyBlobLength(&pData->sMem);` |
|     11 | 5624 | `		if( pData->nCur > nLen ){` |
|      - | 5625 | `			/* seek past end: php zero-fills the gap */` |
|      - | 5626 | `			static const char zZero[64] = {0};` |
|    ! 0 | 5627 | `			while( SyBlobLength(&pData->sMem) < pData->nCur ){` |
|    ! 0 | 5628 | `				sxu32 nPad = pData->nCur - SyBlobLength(&pData->sMem);` |
|    ! 0 | 5629 | `				if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|    ! 0 | 5630 | `				if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|    ! 0 | 5631 | `					return -1;` |
|      - | 5632 | `				}` |
|    ! 0 | 5633 | `			}` |
|    ! 0 | 5634 | `			nLen = SyBlobLength(&pData->sMem);` |
|    ! 0 | 5635 | `		}` |
|     11 | 5636 | `		nEnd = pData->nCur + (sxu32)nWrite;` |
|     11 | 5637 | `		if( pData->nCur < nLen ){` |
|      - | 5638 | `			/* overwrite in place up to the current end */` |
|      3 | 5639 | `			sxu32 nOver = nLen - pData->nCur;` |
|      3 | 5640 | `			if( nOver > (sxu32)nWrite ){ nOver = (sxu32)nWrite; }` |
|      3 | 5641 | `			SyMemcpy(pBuf,(char *)SyBlobData(&pData->sMem) + pData->nCur,nOver);` |
|      3 | 5642 | `			if( nEnd > nLen ){` |
|    ! 0 | 5643 | `				if( SyBlobAppend(&pData->sMem,(const char *)pBuf + nOver,nEnd - nLen) != SXRET_OK ){` |
|    ! 0 | 5644 | `					return -1;` |
|      - | 5645 | `				}` |
|    ! 0 | 5646 | `			}` |
|      2 | 5647 | `		}else{` |
|      9 | 5648 | `			if( SyBlobAppend(&pData->sMem,pBuf,(sxu32)nWrite) != SXRET_OK ){` |
|    ! 0 | 5649 | `				return -1;` |
|      - | 5650 | `			}` |
|      - | 5651 | `		}` |
|     11 | 5652 | `		pData->nCur = nEnd;` |
|     11 | 5653 | `		return nWrite;` |
|      3 | 5654 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|      3 | 5655 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|      - | 5656 | `		int rc;` |
|      - | 5657 | `		/* Call the vm output consumer */` |
|      3 | 5658 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|      3 | 5659 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 5660 | `			return -1;` |
|      - | 5661 | `		}` |
|      3 | 5662 | `		return nWrite;` |
|      - | 5663 | `	}` |
|      - | 5664 | `#ifdef __WINNT__` |
|      - | 5665 | `	{` |
|      - | 5666 | `		DWORD nWr;` |
|      - | 5667 | `		BOOL rc;` |
|    ! 0 | 5668 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|    ! 0 | 5669 | `		if( !rc ){` |
|      - | 5670 | `			/* IO error */` |
|    ! 0 | 5671 | `			return -1;` |
|      - | 5672 | `		}` |
|    ! 0 | 5673 | `		return (ph7_int64)nWr;` |
|      - | 5674 | `	}` |
|      - | 5675 | `#elif defined(__UNIXES__)` |
|      - | 5676 | `	{` |
|      - | 5677 | `		ssize_t nWr;` |
|      - | 5678 | `		int fd;` |
|    ! 0 | 5679 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 5680 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|    ! 0 | 5681 | `		if( nWr < 1 ){` |
|    ! 0 | 5682 | `			return -1;` |
|      - | 5683 | `		}` |
|    ! 0 | 5684 | `		return (ph7_int64)nWr;` |
|      - | 5685 | `	}` |
|      - | 5686 | `#else` |
|      - | 5687 | `	return -1;` |
|      - | 5688 | `#endif` |
|      7 | 5689 | `}` |
|      - | 5690 | `/* void (*xClose)(void *) */` |
|     16 | 5691 | `static void PHPStreamData_Close(void *pHandle)` |
|      1 | 5692 | `{` |
|     17 | 5693 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 5694 | `	ph7_vm *pVm;` |
|     17 | 5695 | `	if( pData == 0 ){` |
|    ! 0 | 5696 | `		return;` |
|      - | 5697 | `	}` |
|     17 | 5698 | `	pVm = pData->pVm;` |
|     17 | 5699 | `	SyBlobRelease(&pData->sMem);` |
|      - | 5700 | `	/* Free the instance */` |
|     17 | 5701 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|      9 | 5702 | `}` |
|      - | 5703 | `/* int (*xSeek)(void *,ph7_int64,int); MEMORY type only */` |
|     20 | 5704 | `static int PHPStreamData_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 5705 | `{` |
|     21 | 5706 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 5707 | `	ph7_int64 iNew;` |
|     21 | 5708 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|    ! 0 | 5709 | `		return -1;` |
|      - | 5710 | `	}` |
|     21 | 5711 | `	switch(whence){` |
|    ! 0 | 5712 | `	case 1/*SEEK_CUR*/: iNew = (ph7_int64)pData->nCur + iOfft; break;` |
|      3 | 5713 | `	case 2/*SEEK_END*/: iNew = (ph7_int64)SyBlobLength(&pData->sMem) + iOfft; break;` |
|     19 | 5714 | `	default:            iNew = iOfft; break;` |
|      - | 5715 | `	}` |
|     21 | 5716 | `	if( iNew < 0 ){` |
|    ! 0 | 5717 | `		return -1;` |
|      - | 5718 | `	}` |
|     21 | 5719 | `	pData->nCur = (sxu32)iNew;` |
|     21 | 5720 | `	return PH7_OK;` |
|     11 | 5721 | `}` |
|      - | 5722 | `/* ph7_int64 (*xTell)(void *); MEMORY type only */` |
|      4 | 5723 | `static ph7_int64 PHPStreamData_Tell(void *pHandle)` |
|      1 | 5724 | `{` |
|      5 | 5725 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      5 | 5726 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|    ! 0 | 5727 | `		return -1;` |
|      - | 5728 | `	}` |
|      5 | 5729 | `	return (ph7_int64)pData->nCur;` |
|      3 | 5730 | `}` |
|      - | 5731 | `/* int (*xTrunc)(void *,ph7_int64); MEMORY type only */` |
|    ! 0 | 5732 | `static int PHPStreamData_Trunc(void *pHandle,ph7_int64 nLen)` |
|    ! 0 | 5733 | `{` |
|    ! 0 | 5734 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    ! 0 | 5735 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY \|\| pData->bReadOnly ){` |
|    ! 0 | 5736 | `		return -1;` |
|      - | 5737 | `	}` |
|    ! 0 | 5738 | `	if( nLen < (ph7_int64)SyBlobLength(&pData->sMem) ){` |
|      - | 5739 | `		/* shrink in place: the blob keeps its allocation */` |
|    ! 0 | 5740 | `		pData->sMem.nByte = (sxu32)nLen;` |
|    ! 0 | 5741 | `	}else{` |
|      - | 5742 | `		static const char zZero[64] = {0};` |
|    ! 0 | 5743 | `		while( (ph7_int64)SyBlobLength(&pData->sMem) < nLen ){` |
|    ! 0 | 5744 | `			sxu32 nPad = (sxu32)(nLen - SyBlobLength(&pData->sMem));` |
|    ! 0 | 5745 | `			if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|    ! 0 | 5746 | `			if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|    ! 0 | 5747 | `				return -1;` |
|      - | 5748 | `			}` |
|    ! 0 | 5749 | `		}` |
|      - | 5750 | `	}` |
|    ! 0 | 5751 | `	return PH7_OK;` |
|    ! 0 | 5752 | `}` |
|      - | 5753 | `/*` |
|      - | 5754 | ` * data:// stream: read-only in-memory payloads parsed from RFC 2397 URIs` |
|      - | 5755 | ` * (data://[mediatype][;base64],payload — the payload percent-decodes unless` |
|      - | 5756 | ` * base64). Shares the MEMORY machinery above.` |
|      - | 5757 | ` */` |
|      8 | 5758 | `static sxi32 DataStreamB64Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 5759 | `{` |
|      9 | 5760 | `	return SyBlobAppend((SyBlob *)pUserData,pData,nLen);` |
|      1 | 5761 | `}` |
|     10 | 5762 | `static int DataStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 5763 | `{` |
|      - | 5764 | `	ph7_stream_data *pData;` |
|     11 | 5765 | `	const char *zIn = zName;` |
|     11 | 5766 | `	const char *zEnd = &zName[SyStrlen(zName)];` |
|     11 | 5767 | `	const char *zComma = 0;` |
|     11 | 5768 | `	int bBase64 = 0;` |
|      5 | 5769 | `	SXUNUSED(iMode);` |
|      - | 5770 | `	/* Find the comma separating the mediatype from the payload */` |
|    105 | 5771 | `	while( zIn < zEnd ){` |
|    105 | 5772 | `		if( zIn[0] == ',' ){` |
|     11 | 5773 | `			zComma = zIn;` |
|     11 | 5774 | `			break;` |
|      - | 5775 | `		}` |
|     95 | 5776 | `		zIn++;` |
|      1 | 5777 | `	}` |
|     11 | 5778 | `	if( zComma == 0 ){` |
|    ! 0 | 5779 | `		return -1;` |
|      - | 5780 | `	}` |
|     10 | 5781 | `	if( zComma - zName >= (int)sizeof(";base64")-1` |
|     10 | 5782 | `	 && SyStrnicmp(&zComma[-((int)sizeof(";base64")-1)],";base64",sizeof(";base64")-1) == 0 ){` |
|      3 | 5783 | `		bBase64 = 1;` |
|      1 | 5784 | `	}` |
|     11 | 5785 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,PH7_IO_STREAM_MEMORY);` |
|     11 | 5786 | `	if( pData == 0 ){` |
|    ! 0 | 5787 | `		return -1;` |
|      - | 5788 | `	}` |
|     11 | 5789 | `	pData->bReadOnly = 1;` |
|     11 | 5790 | `	zIn = &zComma[1];` |
|     11 | 5791 | `	if( bBase64 ){` |
|      3 | 5792 | `		if( SyBase64Decode(zIn,(sxu32)(zEnd - zIn),DataStreamB64Consumer,&pData->sMem) != SXRET_OK ){` |
|    ! 0 | 5793 | `			SyBlobRelease(&pData->sMem);` |
|    ! 0 | 5794 | `			SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|    ! 0 | 5795 | `			return -1;` |
|      - | 5796 | `		}` |
|      2 | 5797 | `	}else{` |
|      - | 5798 | `		/* percent-decode the payload */` |
|     71 | 5799 | `		while( zIn < zEnd ){` |
|     63 | 5800 | `			char c = zIn[0];` |
|     63 | 5801 | `			if( c == '%' && zIn + 2 < zEnd && SyisHex(zIn[1]) && SyisHex(zIn[2]) ){` |
|      3 | 5802 | `				int hi = SyHexToint(zIn[1]);` |
|      3 | 5803 | `				int lo = SyHexToint(zIn[2]);` |
|      3 | 5804 | `				c = (char)((hi << 4) \| lo);` |
|      3 | 5805 | `				zIn += 3;` |
|      2 | 5806 | `			}else{` |
|     61 | 5807 | `				zIn++;` |
|      - | 5808 | `			}` |
|     63 | 5809 | `			if( SyBlobAppend(&pData->sMem,&c,1) != SXRET_OK ){` |
|    ! 0 | 5810 | `				SyBlobRelease(&pData->sMem);` |
|    ! 0 | 5811 | `				SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|    ! 0 | 5812 | `				return -1;` |
|      - | 5813 | `			}` |
|      1 | 5814 | `		}` |
|      - | 5815 | `	}` |
|     11 | 5816 | `	*ppHandle = (void *)pData;` |
|     11 | 5817 | `	return PH7_OK;` |
|      6 | 5818 | `}` |
|      - | 5819 | `/* data:// rejects writes outright */` |
|    ! 0 | 5820 | `static ph7_int64 DataStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 5821 | `{` |
|    ! 0 | 5822 | `	SXUNUSED(pHandle);` |
|    ! 0 | 5823 | `	SXUNUSED(pBuf);` |
|    ! 0 | 5824 | `	SXUNUSED(nWrite);` |
|    ! 0 | 5825 | `	return -1;` |
|    ! 0 | 5826 | `}` |
|      - | 5827 | `static const ph7_io_stream sDATA_Stream = {` |
|      - | 5828 | `	"data",` |
|      - | 5829 | `	PH7_IO_STREAM_VERSION,` |
|      - | 5830 | `	DataStreamData_Open,  /* xOpen */` |
|      - | 5831 | `	0,   /* xOpenDir */` |
|      - | 5832 | `	PHPStreamData_Close, /* xClose */` |
|      - | 5833 | `	0,  /* xCloseDir */` |
|      - | 5834 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 5835 | `	0,  /* xReadDir */` |
|      - | 5836 | `	DataStreamData_Write, /* xWrite */` |
|      - | 5837 | `	PHPStreamData_Seek,  /* xSeek */` |
|      - | 5838 | `	0,  /* xLock */` |
|      - | 5839 | `	0,  /* xRewindDir */` |
|      - | 5840 | `	PHPStreamData_Tell,  /* xTell */` |
|      - | 5841 | `	0,  /* xTrunc */` |
|      - | 5842 | `	0,  /* xSync */` |
|      - | 5843 | `	0   /* xStat */` |
|      - | 5844 | `};` |
|      - | 5845 | `/*` |
|      - | 5846 | ` * Pipe stream implementation for popen/pclose.` |
|      - | 5847 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|      - | 5848 | ` * PHP-compatible process I/O functionality.` |
|      - | 5849 | ` */` |
|      - | 5850 | `typedef struct pipe_private pipe_private;` |
|      - | 5851 | `struct pipe_private` |
|      - | 5852 | `{` |
|      - | 5853 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|      - | 5854 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|      - | 5855 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|      - | 5856 | `#ifdef __WINNT__` |
|      - | 5857 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|      - | 5858 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|      - | 5859 | `#endif` |
|      - | 5860 | `};` |
|      - | 5861 |  |
|      - | 5862 | `#ifdef __WINNT__` |
|      - | 5863 | `#include <Windows.h>` |
|      - | 5864 | `#include <stdio.h>` |
|      - | 5865 | `#include <io.h>` |
|      - | 5866 | `#include <fcntl.h>` |
|      - | 5867 | `/*` |
|      - | 5868 | ` * Custom Windows popen implementation using CreateProcess.` |
|      - | 5869 | ` * This allows us to properly wait for process completion.` |
|      - | 5870 | ` */` |
|      - | 5871 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|      5 | 5872 | `{` |
|      5 | 5873 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|      5 | 5874 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|      5 | 5875 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|      - | 5876 | `	SECURITY_ATTRIBUTES sa;` |
|      - | 5877 | `	STARTUPINFOW si;` |
|      - | 5878 | `	PROCESS_INFORMATION pi;` |
|      5 | 5879 | `	WCHAR *zWideCmd = NULL;` |
|      5 | 5880 | `	FILE *pFile = NULL;` |
|      - | 5881 | `	int fd;` |
|      5 | 5882 | `	BOOL bRead = (zMode[0] == 'r');` |
|      - | 5883 |  |
|      - | 5884 | `	/* Set up security attributes for pipe inheritance */` |
|      5 | 5885 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|      5 | 5886 | `	sa.bInheritHandle = TRUE;` |
|      5 | 5887 | `	sa.lpSecurityDescriptor = NULL;` |
|      - | 5888 |  |
|      - | 5889 | `	/* Create pipes for child process I/O */` |
|      5 | 5890 | `	if( bRead ){` |
|      - | 5891 | `		/* Reading from child's stdout */` |
|      5 | 5892 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|    ! 0 | 5893 | `			return NULL;` |
|      - | 5894 | `		}` |
|      - | 5895 | `		/* Ensure read handle is not inherited */` |
|      5 | 5896 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|      5 | 5897 | `		hReadPipe = hChildStdoutRd;` |
|      5 | 5898 | `		*phPipe = hChildStdoutRd;` |
|      5 | 5899 | `	}else{` |
|      - | 5900 | `		/* Writing to child's stdin */` |
|    ! 0 | 5901 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|    ! 0 | 5902 | `			return NULL;` |
|      - | 5903 | `		}` |
|      - | 5904 | `		/* Ensure write handle is not inherited */` |
|    ! 0 | 5905 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|    ! 0 | 5906 | `		hWritePipe = hChildStdinWr;` |
|    ! 0 | 5907 | `		*phPipe = hChildStdinWr;` |
|      - | 5908 | `	}` |
|      - | 5909 |  |
|      - | 5910 | `	/* Convert command to wide string */` |
|      - | 5911 | `	{` |
|      5 | 5912 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|      5 | 5913 | `		if( nLen <= 0 ){` |
|    ! 0 | 5914 | `			goto cleanup_pipes;` |
|      - | 5915 | `		}` |
|      5 | 5916 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|      5 | 5917 | `		if( !zWideCmd ){` |
|    ! 0 | 5918 | `			goto cleanup_pipes;` |
|      - | 5919 | `		}` |
|      5 | 5920 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|      - | 5921 | `	}` |
|      - | 5922 |  |
|      - | 5923 | `	/* Set up process startup info */` |
|      5 | 5924 | `	ZeroMemory(&si, sizeof(si));` |
|      5 | 5925 | `	si.cb = sizeof(si);` |
|      5 | 5926 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|      5 | 5927 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|      5 | 5928 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|      5 | 5929 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|      5 | 5930 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|      - | 5931 |  |
|      5 | 5932 | `	ZeroMemory(&pi, sizeof(pi));` |
|      - | 5933 |  |
|      - | 5934 | `	/* Create the child process */` |
|      5 | 5935 | `	if( !CreateProcessW(` |
|      - | 5936 | `		NULL,           /* Application name */` |
|      - | 5937 | `		zWideCmd,       /* Command line */` |
|      - | 5938 | `		NULL,           /* Process security attributes */` |
|      - | 5939 | `		NULL,           /* Thread security attributes */` |
|      - | 5940 | `		TRUE,           /* Inherit handles */` |
|      - | 5941 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|      - | 5942 | `		NULL,           /* Environment */` |
|      - | 5943 | `		NULL,           /* Current directory */` |
|      - | 5944 | `		&si,            /* Startup info */` |
|      - | 5945 | `		&pi             /* Process info */` |
|      - | 5946 | `	)){` |
|    ! 0 | 5947 | `		goto cleanup_all;` |
|      - | 5948 | `	}` |
|      - | 5949 |  |
|      - | 5950 | `	/* Close handles we don't need in parent */` |
|      5 | 5951 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|      5 | 5952 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|      - | 5953 |  |
|      - | 5954 | `	/* Close thread handle (we only need process handle) */` |
|      5 | 5955 | `	CloseHandle(pi.hThread);` |
|      - | 5956 |  |
|      - | 5957 | `	/* Store process handle for later waiting */` |
|      5 | 5958 | `	*phProcess = pi.hProcess;` |
|      - | 5959 |  |
|      - | 5960 | `	/* Convert OS handle to C file descriptor, then to FILE* */` |
|      5 | 5961 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|      - | 5962 | `	                     bRead ? _O_RDONLY \| _O_TEXT : _O_WRONLY \| _O_TEXT);` |
|      5 | 5963 | `	if( fd == -1 ){` |
|    ! 0 | 5964 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 5965 | `		*phProcess = NULL;` |
|    ! 0 | 5966 | `		goto cleanup_all;` |
|      - | 5967 | `	}` |
|      - | 5968 |  |
|      5 | 5969 | `	pFile = _fdopen(fd, zMode);` |
|      5 | 5970 | `	if( !pFile ){` |
|    ! 0 | 5971 | `		_close(fd); /* This will also close the underlying handle */` |
|    ! 0 | 5972 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 5973 | `		*phProcess = NULL;` |
|    ! 0 | 5974 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    ! 0 | 5975 | `		return NULL;` |
|      - | 5976 | `	}` |
|      - | 5977 |  |
|      5 | 5978 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      5 | 5979 | `	return pFile;` |
|      - | 5980 |  |
|      - | 5981 | `cleanup_all:` |
|    ! 0 | 5982 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      - | 5983 | `cleanup_pipes:` |
|    ! 0 | 5984 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|    ! 0 | 5985 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|    ! 0 | 5986 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|    ! 0 | 5987 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|    ! 0 | 5988 | `	return NULL;` |
|      5 | 5989 | `}` |
|      - | 5990 |  |
|      - | 5991 | `/*` |
|      - | 5992 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|      - | 5993 | ` */` |
|      - | 5994 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|      5 | 5995 | `{` |
|      5 | 5996 | `	DWORD dwExitCode = 0;` |
|      - | 5997 | `	int status;` |
|      - | 5998 |  |
|      - | 5999 | `	/* Close the FILE* (this closes the pipe) */` |
|      5 | 6000 | `	fclose(pFile);` |
|      - | 6001 |  |
|      5 | 6002 | `	if( hProcess ){` |
|      - | 6003 | `		/* Wait for the process to complete */` |
|      5 | 6004 | `		WaitForSingleObject(hProcess, INFINITE);` |
|      - | 6005 |  |
|      5 | 6006 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|      5 | 6007 | `			status = (int)dwExitCode;` |
|      5 | 6008 | `		}else{` |
|    ! 0 | 6009 | `			status = -1;` |
|      - | 6010 | `		}` |
|      - | 6011 |  |
|      - | 6012 | `		/* Close process handle */` |
|      5 | 6013 | `		CloseHandle(hProcess);` |
|      5 | 6014 | `	}else{` |
|    ! 0 | 6015 | `		status = -1;` |
|      - | 6016 | `	}` |
|      - | 6017 |  |
|      5 | 6018 | `	return status;` |
|      5 | 6019 | `}` |
|      - | 6020 | `#endif /* __WINNT__ */` |
|      - | 6021 | `/*` |
|      - | 6022 | ` * Open a pipe to a process.` |
|      - | 6023 | ` * This is called internally by popen(), not through the stream device interface.` |
|      - | 6024 | ` */` |
|   4010 | 6025 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|      5 | 6026 | `{` |
|      - | 6027 | `	pipe_private *pPipe;` |
|      - | 6028 | `	FILE *pFile;` |
|   4015 | 6029 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|    ! 0 | 6030 | `		return 0;` |
|      - | 6031 | `	}` |
|      - | 6032 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|   4015 | 6033 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|    ! 0 | 6034 | `		return 0;` |
|      - | 6035 | `	}` |
|      - | 6036 | `	/* Open the pipe using system popen */` |
|      - | 6037 | `#ifdef __WINNT__` |
|      - | 6038 | `	{` |
|      - | 6039 | `		/* Build cmd.exe command wrapper */` |
|      5 | 6040 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|      5 | 6041 | `		const char *zShellSuffix = "\"";` |
|      5 | 6042 | `		size_t nPrefix = strlen(zShellPrefix);` |
|      5 | 6043 | `		size_t nSuffix = strlen(zShellSuffix);` |
|      5 | 6044 | `		size_t nCmd = strlen(zCommand);` |
|      5 | 6045 | `		size_t nQuotes = 0;` |
|      5 | 6046 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 6047 | `			if (zCommand[i] == '"') nQuotes++;` |
|      5 | 6048 | `		}` |
|      5 | 6049 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|      5 | 6050 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|      5 | 6051 | `		if (zCmdEsc == NULL) {` |
|    ! 0 | 6052 | `			return 0;` |
|      - | 6053 | `		}` |
|      - | 6054 | `		/* Escape quotes in command */` |
|      5 | 6055 | `		size_t j = 0;` |
|      5 | 6056 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 6057 | `			char ch = zCommand[i];` |
|      5 | 6058 | `			if (ch == '"') {` |
|      4 | 6059 | `				zCmdEsc[j++] = '^';` |
|      4 | 6060 | `				zCmdEsc[j++] = '"';` |
|      4 | 6061 | `			} else {` |
|      5 | 6062 | `				zCmdEsc[j++] = ch;` |
|      - | 6063 | `			}` |
|      5 | 6064 | `		}` |
|      5 | 6065 | `		zCmdEsc[j] = '\0';` |
|      5 | 6066 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|      5 | 6067 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|      5 | 6068 | `		if (zWinCmd == NULL) {` |
|    ! 0 | 6069 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 6070 | `			return 0;` |
|      - | 6071 | `		}` |
|      5 | 6072 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|      5 | 6073 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|      5 | 6074 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|      5 | 6075 | `		zWinCmd[nTotal - 1] = '\0';` |
|      - | 6076 | `		/* Allocate pipe structure early so we can store handles */` |
|      5 | 6077 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|      5 | 6078 | `		if( pPipe == 0 ){` |
|    ! 0 | 6079 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 6080 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|    ! 0 | 6081 | `			return 0;` |
|      - | 6082 | `		}` |
|      - | 6083 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|      5 | 6084 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|      5 | 6085 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|      5 | 6086 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|      5 | 6087 | `		if( pFile == 0 ){` |
|    ! 0 | 6088 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|    ! 0 | 6089 | `			return 0;` |
|      - | 6090 | `		}` |
|      - | 6091 | `		/* Initialize remaining fields */` |
|      5 | 6092 | `		pPipe->pFile = pFile;` |
|      5 | 6093 | `		pPipe->pVm = pVm;` |
|      5 | 6094 | `		pPipe->iMode = zMode[0];` |
|      - | 6095 | `	}` |
|      - | 6096 | `#elif defined(__UNIXES__) /* Unix */` |
|   4010 | 6097 | `	pFile = popen(zCommand, zMode);` |
|   4010 | 6098 | `	if( pFile == 0 ){` |
|    ! 0 | 6099 | `		return 0;` |
|      - | 6100 | `	}` |
|      - | 6101 | `	/* Allocate pipe private structure */` |
|   4010 | 6102 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|   4010 | 6103 | `	if( pPipe == 0 ){` |
|      - | 6104 | `		/* Out of memory, close the pipe */` |
|    ! 0 | 6105 | `		pclose(pFile);` |
|    ! 0 | 6106 | `		return 0;` |
|      - | 6107 | `	}` |
|      - | 6108 | `	/* Initialize the structure */` |
|   4010 | 6109 | `	pPipe->pFile = pFile;` |
|   4010 | 6110 | `	pPipe->pVm = pVm;` |
|   4010 | 6111 | `	pPipe->iMode = zMode[0];` |
|      - | 6112 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 6113 | `	(void)pFile;` |
|      - | 6114 | `	return 0;` |
|      - | 6115 | `#endif` |
|   4015 | 6116 | `	return pPipe;` |
|   2010 | 6117 | `}` |
|      - | 6118 | `/*` |
|      - | 6119 | ` * Close a pipe and return the exit status of the process.` |
|      - | 6120 | ` * Returns the exit status, or -1 on error.` |
|      - | 6121 | ` */` |
|   3988 | 6122 | `static int PipeClose(pipe_private *pPipe)` |
|      5 | 6123 | `{` |
|      - | 6124 | `	int status;` |
|      - | 6125 | `	ph7_vm *pVm;` |
|   3993 | 6126 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6127 | `		return -1;` |
|      - | 6128 | `	}` |
|   3993 | 6129 | `	pVm = pPipe->pVm;` |
|      - | 6130 | `	/* Close the pipe and get exit status */` |
|      - | 6131 | `#ifdef __WINNT__` |
|      - | 6132 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|      5 | 6133 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|      - | 6134 | `#elif defined(__UNIXES__)` |
|   3988 | 6135 | `	status = pclose(pPipe->pFile);` |
|      - | 6136 | `	/* On Unix, pclose returns the status from waitpid, need to extract exit code */` |
|   3988 | 6137 | `	if( status != -1 ){` |
|   3988 | 6138 | `		if( WIFEXITED(status) ){` |
|   3988 | 6139 | `			status = WEXITSTATUS(status);` |
|   1994 | 6140 | `		}else if( WIFSIGNALED(status) ){` |
|      - | 6141 | `			/* Process was killed by a signal - use shell convention: 128 + signal number */` |
|    ! 0 | 6142 | `			status = 128 + WTERMSIG(status);` |
|    ! 0 | 6143 | `		}else{` |
|      - | 6144 | `			/* Unknown termination reason */` |
|    ! 0 | 6145 | `			status = -1;` |
|      - | 6146 | `		}` |
|   1994 | 6147 | `	}` |
|      - | 6148 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 6149 | `	status = -1;` |
|      - | 6150 | `#endif` |
|      - | 6151 | `	/* Free the structure */` |
|   3993 | 6152 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|   3993 | 6153 | `	return status;` |
|   1999 | 6154 | `}` |
|      - | 6155 | `/*` |
|      - | 6156 | ` * Pipe stream xClose implementation.` |
|      - | 6157 | ` * Note: This is called by fclose(), not pclose().` |
|      - | 6158 | ` * It closes the pipe but does not return the exit status.` |
|      - | 6159 | ` */` |
|     92 | 6160 | `static void PipeStream_Close(void *pHandle)` |
|      4 | 6161 | `{` |
|     96 | 6162 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|     96 | 6163 | `	if( pPipe ){` |
|     96 | 6164 | `		PipeClose(pPipe);` |
|     46 | 6165 | `	}` |
|     96 | 6166 | `}` |
|      - | 6167 | `/*` |
|      - | 6168 | ` * Pipe stream xRead implementation.` |
|      - | 6169 | ` */` |
|   5884 | 6170 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|      4 | 6171 | `{` |
|   5888 | 6172 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 6173 | `	size_t nRead;` |
|   5888 | 6174 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6175 | `		return -1;` |
|      - | 6176 | `	}` |
|   5888 | 6177 | `	if( pPipe->iMode != 'r' ){` |
|      - | 6178 | `		/* Cannot read from a write-only pipe */` |
|    ! 0 | 6179 | `		return -1;` |
|      - | 6180 | `	}` |
|   5888 | 6181 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
|   5888 | 6182 | `	if( nRead == 0 ){` |
|   4028 | 6183 | `		if( feof(pPipe->pFile) ){` |
|   4028 | 6184 | `			return 0; /* EOF */` |
|      - | 6185 | `		}` |
|    ! 0 | 6186 | `		return -1; /* Error */` |
|      - | 6187 | `	}` |
|   1864 | 6188 | `	return (ph7_int64)nRead;` |
|   2946 | 6189 | `}` |
|      - | 6190 | `/*` |
|      - | 6191 | ` * Pipe stream xWrite implementation.` |
|      - | 6192 | ` */` |
|      2 | 6193 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|    ! 0 | 6194 | `{` |
|      2 | 6195 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 6196 | `	size_t nWritten;` |
|      2 | 6197 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6198 | `		return -1;` |
|      - | 6199 | `	}` |
|      2 | 6200 | `	if( pPipe->iMode != 'w' ){` |
|      - | 6201 | `		/* Cannot write to a read-only pipe */` |
|    ! 0 | 6202 | `		return -1;` |
|      - | 6203 | `	}` |
|      2 | 6204 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|      2 | 6205 | `	if( nWritten == 0 && nWrite > 0 ){` |
|    ! 0 | 6206 | `		return -1; /* Error */` |
|      - | 6207 | `	}` |
|      2 | 6208 | `	return (ph7_int64)nWritten;` |
|      1 | 6209 | `}` |
|      - | 6210 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|      - | 6211 | `static const ph7_io_stream sPipe_Stream = {` |
|      - | 6212 | `	"pipe",` |
|      - | 6213 | `	PH7_IO_STREAM_VERSION,` |
|      - | 6214 | `	0,  /* xOpen - not used, pipes opened via PipeOpen() */` |
|      - | 6215 | `	0,  /* xOpenDir */` |
|      - | 6216 | `	PipeStream_Close,  /* xClose */` |
|      - | 6217 | `	0,  /* xCloseDir */` |
|      - | 6218 | `	PipeStream_Read,   /* xRead */` |
|      - | 6219 | `	0,  /* xReadDir */` |
|      - | 6220 | `	PipeStream_Write,  /* xWrite */` |
|      - | 6221 | `	0,  /* xSeek */` |
|      - | 6222 | `	0,  /* xLock */` |
|      - | 6223 | `	0,  /* xRewindDir */` |
|      - | 6224 | `	0,  /* xTell */` |
|      - | 6225 | `	0,  /* xTrunc */` |
|      - | 6226 | `	0,  /* xSync */` |
|      - | 6227 | `	0   /* xStat */` |
|      - | 6228 | `};` |
|      - | 6229 | `/*` |
|      - | 6230 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|      - | 6231 | ` * FALSE otherwise.` |
|      - | 6232 | ` */` |
|   3896 | 6233 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|      5 | 6234 | `{` |
|   3901 | 6235 | `	return pStream == &sPipe_Stream;` |
|      5 | 6236 | `}` |
|      - | 6237 | `/*` |
|      - | 6238 | ` * resource popen(string $command, string $mode)` |
|      - | 6239 | ` *  Opens process file pointer.` |
|      - | 6240 | ` * Parameters` |
|      - | 6241 | ` *  $command` |
|      - | 6242 | ` *   The command to execute. Passed to the system shell.` |
|      - | 6243 | ` *  $mode` |
|      - | 6244 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|      - | 6245 | ` *   'r' - Open for reading (read from the command's stdout).` |
|      - | 6246 | ` *   'w' - Open for writing (write to the command's stdin).` |
|      - | 6247 | ` * Return` |
|      - | 6248 | ` *  Returns a file pointer on success, or FALSE on error.` |
|      - | 6249 | ` */` |
|   4010 | 6250 | `static int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6251 | `{` |
|      - | 6252 | `	const char *zCommand, *zMode;` |
|      - | 6253 | `	pipe_private *pPipe;` |
|      - | 6254 | `	io_private *pDev;` |
|      - | 6255 | `	int nCmdLen, nModeLen;` |
|   4015 | 6256 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 6257 | `		/* Missing/Invalid arguments, return FALSE */` |
|    ! 0 | 6258 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a command string and mode");` |
|    ! 0 | 6259 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6260 | `		return PH7_OK;` |
|      - | 6261 | `	}` |
|      - | 6262 | `	/* Extract the command and mode */` |
|   4015 | 6263 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|   4015 | 6264 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|   4015 | 6265 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 6266 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty command");` |
|    ! 0 | 6267 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6268 | `		return PH7_OK;` |
|      - | 6269 | `	}` |
|   4015 | 6270 | `	if( nModeLen < 1 \|\| (zMode[0] != 'r' && zMode[0] != 'w') ){` |
|    ! 0 | 6271 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Invalid mode, expected 'r' or 'w'");` |
|    ! 0 | 6272 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6273 | `		return PH7_OK;` |
|      - | 6274 | `	}` |
|      - | 6275 | `	/* Open the pipe */` |
|   4015 | 6276 | `	pPipe = PipeOpen(pCtx->pVm, zCommand, zMode);` |
|   4015 | 6277 | `	if( pPipe == 0 ){` |
|      - | 6278 | `		/* Failed to open pipe */` |
|    ! 0 | 6279 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6280 | `		return PH7_OK;` |
|      - | 6281 | `	}` |
|      - | 6282 | `	/* Allocate an io_private instance to wrap the pipe */` |
|   4015 | 6283 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|   4015 | 6284 | `	if( pDev == 0 ){` |
|    ! 0 | 6285 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|    ! 0 | 6286 | `		PipeClose(pPipe);` |
|    ! 0 | 6287 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6288 | `		return PH7_OK;` |
|      - | 6289 | `	}` |
|      - | 6290 | `	/* Initialize the io_private structure */` |
|   4015 | 6291 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|   4015 | 6292 | `	pDev->pHandle = pPipe;` |
|      - | 6293 | `	/* Return the io_private instance as a resource */` |
|   4015 | 6294 | `	ph7_result_resource(pCtx, pDev);` |
|   4015 | 6295 | `	return PH7_OK;` |
|   2010 | 6296 | `}` |
|      - | 6297 | `/*` |
|      - | 6298 | ` * int pclose(resource $handle)` |
|      - | 6299 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|      - | 6300 | ` * Parameters` |
|      - | 6301 | ` *  $handle` |
|      - | 6302 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|      - | 6303 | ` * Return` |
|      - | 6304 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|      - | 6305 | ` */` |
|   3896 | 6306 | `static int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6307 | `{` |
|      - | 6308 | `	const ph7_io_stream *pStream;` |
|      - | 6309 | `	pipe_private *pPipe;` |
|      - | 6310 | `	io_private *pDev;` |
|      - | 6311 | `	int status;` |
|   3901 | 6312 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 6313 | `		/* Missing/Invalid arguments, return -1 */` |
|    ! 0 | 6314 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 6315 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 6316 | `		return PH7_OK;` |
|      - | 6317 | `	}` |
|      - | 6318 | `	/* Extract our private data */` |
|   3901 | 6319 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 6320 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   3901 | 6321 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 6322 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 6323 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 6324 | `		return PH7_OK;` |
|      - | 6325 | `	}` |
|      - | 6326 | `	/* Point to the target IO stream device */` |
|   3901 | 6327 | `	pStream = pDev->pStream;` |
|   3901 | 6328 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|    ! 0 | 6329 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|    ! 0 | 6330 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 6331 | `		return PH7_OK;` |
|      - | 6332 | `	}` |
|      - | 6333 | `	/* Get the pipe handle */` |
|   3901 | 6334 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|      - | 6335 | `	/* Close the pipe and get exit status */` |
|   3901 | 6336 | `	status = PipeClose(pPipe);` |
|      - | 6337 | `	/* Release the IO private structure */` |
|   3901 | 6338 | `	ReleaseIOPrivate(pCtx, pDev);` |
|      - | 6339 | `	/* Invalidate the resource handle */` |
|   3901 | 6340 | `	ph7_value_release(apArg[0]);` |
|      - | 6341 | `	/* Return the exit status */` |
|   3901 | 6342 | `	ph7_result_int(pCtx, status);` |
|   3901 | 6343 | `	return PH7_OK;` |
|   1953 | 6344 | `}` |
|      - | 6345 | `/* Export the php:// stream */` |
|      - | 6346 | `static const ph7_io_stream sPHP_Stream = {` |
|      - | 6347 | `	"php",` |
|      - | 6348 | `	PH7_IO_STREAM_VERSION,` |
|      - | 6349 | `	PHPStreamData_Open,  /* xOpen */` |
|      - | 6350 | `	0,   /* xOpenDir */` |
|      - | 6351 | `	PHPStreamData_Close, /* xClose */` |
|      - | 6352 | `	0,  /* xCloseDir */` |
|      - | 6353 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 6354 | `	0,  /* xReadDir */` |
|      - | 6355 | `	PHPStreamData_Write, /* xWrite */` |
|      - | 6356 | `	PHPStreamData_Seek,  /* xSeek (php://memory & php://temp) */` |
|      - | 6357 | `	0,  /* xLock */` |
|      - | 6358 | `	0,  /* xRewindDir */` |
|      - | 6359 | `	PHPStreamData_Tell,  /* xTell */` |
|      - | 6360 | `	PHPStreamData_Trunc, /* xTrunc */` |
|      - | 6361 | `	0,  /* xSync */` |
|      - | 6362 | `	0   /* xStat */` |
|      - | 6363 | `};` |
|      - | 6364 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 6365 | `/*` |
|      - | 6366 | ` * Return TRUE if we are dealing with the php:// stream.` |
|      - | 6367 | ` * FALSE otherwise.` |
|      - | 6368 | ` */` |
|  30370 | 6369 | `static int is_php_stream(const ph7_io_stream *pStream)` |
|      5 | 6370 | `{` |
|      - | 6371 | `#ifndef PH7_DISABLE_DISK_IO` |
|  30375 | 6372 | `	return pStream == &sPHP_Stream;` |
|      - | 6373 | `#else` |
|      - | 6374 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 6375 | `	return 0;` |
|      - | 6376 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      5 | 6377 | `}` |
|      - | 6378 | `/*` |
|      - | 6379 | ` * Return TRUE if we are dealing with the data:// stream.` |
|      - | 6380 | ` */` |
|  30360 | 6381 | `static int is_data_stream(const ph7_io_stream *pStream)` |
|      5 | 6382 | `{` |
|      - | 6383 | `#ifndef PH7_DISABLE_DISK_IO` |
|  30365 | 6384 | `	return pStream == &sDATA_Stream;` |
|      - | 6385 | `#else` |
|      - | 6386 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 6387 | `	return 0;` |
|      - | 6388 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      5 | 6389 | `}` |
|      - | 6390 |  |
|      - | 6391 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 6392 | `/*` |
|      - | 6393 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 6394 | ` * [i.e: file://,php://].` |
|      - | 6395 | ` * Note:` |
|      - | 6396 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 6397 | ` *  defined then this function is a no-op.` |
|      - | 6398 | ` */` |
|   3528 | 6399 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 6400 | `{` |
|      - | 6401 | `	/*` |
|      - | 6402 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 6403 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 6404 | `	 */` |
|      - | 6405 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 6406 | `	/* VFS: disk I/O related functions */` |
|      - | 6407 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 6408 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 6409 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 6410 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 6411 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 6412 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 6413 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 6414 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 6415 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 6416 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 6417 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 6418 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 6419 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 6420 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 6421 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 6422 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 6423 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 6424 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 6425 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 6426 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 6427 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 6428 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 6429 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 6430 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 6431 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 6432 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 6433 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 6434 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 6435 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 6436 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 6437 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 6438 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 6439 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 6440 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 6441 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 6442 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 6443 | `		{"link",        PH7_vfs_link     },` |
|      - | 6444 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 6445 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 6446 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 6447 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 6448 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 6449 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 6450 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 6451 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 6452 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 6453 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 6454 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 6455 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 6456 | `	};` |
|      - | 6457 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 6458 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 6459 | `	 */` |
|      - | 6460 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 6461 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 6462 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 6463 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 6464 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 6465 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 6466 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 6467 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 6468 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 6469 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 6470 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 6471 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 6472 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 6473 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 6474 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 6475 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 6476 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 6477 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 6478 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 6479 | `		{"file",      PH7_builtin_file   },` |
|      - | 6480 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 6481 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 6482 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 6483 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 6484 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 6485 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 6486 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 6487 | `		{"stream_get_contents",  PH7_builtin_stream_get_contents },` |
|      - | 6488 | `		{"stream_get_wrappers",  PH7_builtin_stream_get_wrappers },` |
|      - | 6489 | `		{"stream_get_meta_data", PH7_builtin_stream_get_meta_data },` |
|      - | 6490 | `		{"stream_context_create",PH7_builtin_stream_context_create },` |
|      - | 6491 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 6492 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 6493 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 6494 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 6495 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 6496 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 6497 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 6498 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 6499 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 6500 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 6501 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 6502 | `	};` |
|   3533 | 6503 | `	const ph7_io_stream *pFileStream = 0;` |
|   3533 | 6504 | `	sxu32 n = 0;` |
|      - | 6505 | `	/* Register disk-related functions */` |
| 172877 | 6506 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 169349 | 6507 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
|  84677 | 6508 | `	}` |
| 141125 | 6509 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 137597 | 6510 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  68801 | 6511 | `	}` |
|      - | 6512 | `#else` |
|      - | 6513 | `	SXUNUSED(pVm);` |
|      - | 6514 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 6515 |  |
|      - | 6516 | `	/*` |
|      - | 6517 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 6518 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 6519 | `	 */` |
|      - | 6520 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 6521 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 6522 | `		/* Path processing */` |
|      - | 6523 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 6524 | `		{"basename",    PH7_builtin_basename },` |
|      - | 6525 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 6526 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 6527 | `		{"fnmatch",     PH7_builtin_fnmatch  },` |
|      - | 6528 | `		/* ZIP processing */` |
|      - | 6529 | `		{"zip_open",    PH7_builtin_zip_open },` |
|      - | 6530 | `		{"zip_close",   PH7_builtin_zip_close},` |
|      - | 6531 | `		{"zip_read",    PH7_builtin_zip_read },` |
|      - | 6532 | `		{"zip_entry_open", PH7_builtin_zip_entry_open },` |
|      - | 6533 | `		{"zip_entry_close",PH7_builtin_zip_entry_close},` |
|      - | 6534 | `		{"zip_entry_name", PH7_builtin_zip_entry_name },` |
|      - | 6535 | `		{"zip_entry_filesize",      PH7_builtin_zip_entry_filesize       },` |
|      - | 6536 | `		{"zip_entry_compressedsize",PH7_builtin_zip_entry_compressedsize },` |
|      - | 6537 | `		{"zip_entry_read", PH7_builtin_zip_entry_read },` |
|      - | 6538 | `		{"zip_entry_reset_read_cursor",PH7_builtin_zip_entry_reset_read_cursor},` |
|      - | 6539 | `		{"zip_entry_compressionmethod",PH7_builtin_zip_entry_compressionmethod}` |
|      - | 6540 | `	};` |
|  59981 | 6541 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  56453 | 6542 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|  28229 | 6543 | `	}` |
|      - | 6544 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 6545 |  |
|      - | 6546 | `	/* Install streams if disk I/O is enabled */` |
|      - | 6547 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 6548 | `#ifdef __WINNT__` |
|      5 | 6549 | `	pFileStream = &sWinFileStream;` |
|      - | 6550 | `#elif defined(__UNIXES__)` |
|   3528 | 6551 | `	pFileStream = &sUnixFileStream;` |
|      - | 6552 | `#endif` |
|      - | 6553 | `	/* Install the php:// stream */` |
|   3533 | 6554 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   3533 | 6555 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);` |
|   3533 | 6556 | `	if( pFileStream ){` |
|      - | 6557 | `		/* Install the file:// stream */` |
|   3533 | 6558 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   1764 | 6559 | `	}` |
|      - | 6560 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 6561 |  |
|   3533 | 6562 | `	return SXRET_OK;` |
|      5 | 6563 | `}` |
|      - | 6564 | `/*` |
|      - | 6565 | ` * Export the STDIN handle.` |
|      - | 6566 | ` */` |
|      2 | 6567 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|      1 | 6568 | `{` |
|      - | 6569 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 6570 | `	if( pVm->pStdin == 0  ){` |
|      - | 6571 | `		io_private *pIn;` |
|      - | 6572 | `		/* Allocate an IO private instance */` |
|      3 | 6573 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 6574 | `		if( pIn == 0 ){` |
|    ! 0 | 6575 | `			return 0;` |
|      - | 6576 | `		}` |
|      3 | 6577 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|      - | 6578 | `		/* Initialize the handle */` |
|      3 | 6579 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|      - | 6580 | `		/* Install the STDIN stream */` |
|      3 | 6581 | `		pVm->pStdin = pIn;` |
|      3 | 6582 | `		return pIn;` |
|    ! 0 | 6583 | `	}else{` |
|      - | 6584 | `		/* NULL or STDIN */` |
|    ! 0 | 6585 | `		return pVm->pStdin;` |
|      - | 6586 | `	}` |
|      - | 6587 | `#else` |
|      - | 6588 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 6589 | `	return 0;` |
|      - | 6590 | `#endif` |
|      2 | 6591 | `}` |
|      - | 6592 | `/*` |
|      - | 6593 | ` * Export the STDOUT handle.` |
|      - | 6594 | ` */` |
|      2 | 6595 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|      1 | 6596 | `{` |
|      - | 6597 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 6598 | `	if( pVm->pStdout == 0  ){` |
|      - | 6599 | `		io_private *pOut;` |
|      - | 6600 | `		/* Allocate an IO private instance */` |
|      3 | 6601 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 6602 | `		if( pOut == 0 ){` |
|    ! 0 | 6603 | `			return 0;` |
|      - | 6604 | `		}` |
|      3 | 6605 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|      - | 6606 | `		/* Initialize the handle */` |
|      3 | 6607 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|      - | 6608 | `		/* Install the STDOUT stream */` |
|      3 | 6609 | `		pVm->pStdout = pOut;` |
|      3 | 6610 | `		return pOut;` |
|    ! 0 | 6611 | `	}else{` |
|      - | 6612 | `		/* NULL or STDOUT */` |
|    ! 0 | 6613 | `		return pVm->pStdout;` |
|      - | 6614 | `	}` |
|      - | 6615 | `#else` |
|      - | 6616 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 6617 | `	return 0;` |
|      - | 6618 | `#endif` |
|      2 | 6619 | `}` |
|      - | 6620 | `/*` |
|      - | 6621 | ` * Export the STDERR handle.` |
|      - | 6622 | ` */` |
|      2 | 6623 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|      1 | 6624 | `{` |
|      - | 6625 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 6626 | `	if( pVm->pStderr == 0  ){` |
|      - | 6627 | `		io_private *pErr;` |
|      - | 6628 | `		/* Allocate an IO private instance */` |
|      3 | 6629 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 6630 | `		if( pErr == 0 ){` |
|    ! 0 | 6631 | `			return 0;` |
|      - | 6632 | `		}` |
|      3 | 6633 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|      - | 6634 | `		/* Initialize the handle */` |
|      3 | 6635 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|      - | 6636 | `		/* Install the STDERR stream */` |
|      3 | 6637 | `		pVm->pStderr = pErr;` |
|      3 | 6638 | `		return pErr;` |
|    ! 0 | 6639 | `	}else{` |
|      - | 6640 | `		/* NULL or STDERR */` |
|    ! 0 | 6641 | `		return pVm->pStderr;` |
|      - | 6642 | `	}` |
|      - | 6643 | `#else` |
|      - | 6644 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 6645 | `	return 0;` |
|      - | 6646 | `#endif` |
|      2 | 6647 | `}` |
|      - | 6648 |  |
