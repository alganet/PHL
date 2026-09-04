# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2361/3510 lines (67.26%)

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
|  13446 |   66 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |   67 | `{` |
|      - |   68 | `	const char *zPath;` |
|      - |   69 | `	ph7_vfs *pVfs;` |
|      - |   70 | `	int rc;` |
|  13451 |   71 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |   72 | `		/* Missing/Invalid argument,return FALSE */` |
|      6 |   73 | `		ph7_result_bool(pCtx,0);` |
|      6 |   74 | `		return PH7_OK;` |
|      - |   75 | `	}` |
|      - |   76 | `	/* Point to the underlying vfs */` |
|  13447 |   77 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  13447 |   78 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |   79 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |   80 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |   81 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |   82 | `			ph7_function_name(pCtx)` |
|      - |   83 | `			);` |
|    ! 0 |   84 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |   85 | `		return PH7_OK;` |
|      - |   86 | `	}` |
|      - |   87 | `	/* Point to the desired directory */` |
|  13447 |   88 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |   89 | `	/* Perform the requested operation */` |
|  13447 |   90 | `	rc = pVfs->xChdir(zPath);` |
|      - |   91 | `	/* IO return value */` |
|  13447 |   92 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  13447 |   93 | `	return PH7_OK;` |
|   6728 |   94 | `}` |
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
|     34 |  176 | `static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  177 | `{` |
|      - |  178 | `	const char *zPath;` |
|      - |  179 | `	ph7_vfs *pVfs;` |
|      - |  180 | `	int rc;` |
|     35 |  181 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  182 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  183 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  184 | `		return PH7_OK;` |
|      - |  185 | `	}` |
|      - |  186 | `	/* Point to the underlying vfs */` |
|     35 |  187 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     35 |  188 | `	if( pVfs == 0 \|\| pVfs->xRmdir == 0 ){` |
|      - |  189 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  190 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  191 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  192 | `			ph7_function_name(pCtx)` |
|      - |  193 | `			);` |
|    ! 0 |  194 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  195 | `		return PH7_OK;` |
|      - |  196 | `	}` |
|      - |  197 | `	/* Point to the desired directory */` |
|     35 |  198 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  199 | `	/* Perform the requested operation */` |
|     35 |  200 | `	rc = pVfs->xRmdir(zPath);` |
|      - |  201 | `	/* IO return value */` |
|     35 |  202 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     35 |  203 | `	return PH7_OK;` |
|     18 |  204 | `}` |
|      - |  205 | `/*` |
|      - |  206 | ` * bool is_dir(string $filename)` |
|      - |  207 | ` *  Tells whether the given filename is a directory.` |
|      - |  208 | ` * Parameters` |
|      - |  209 | ` *  $filename` |
|      - |  210 | ` *   Path to the file.` |
|      - |  211 | ` * Return` |
|      - |  212 | ` *  TRUE on success or FALSE on failure.` |
|      - |  213 | ` */` |
|   8502 |  214 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  215 | `{` |
|      - |  216 | `	const char *zPath;` |
|      - |  217 | `	ph7_vfs *pVfs;` |
|      - |  218 | `	int rc;` |
|   8507 |  219 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  220 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  221 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  222 | `		return PH7_OK;` |
|      - |  223 | `	}` |
|      - |  224 | `	/* Point to the underlying vfs */` |
|   8507 |  225 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   8507 |  226 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  227 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  228 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  229 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  230 | `			ph7_function_name(pCtx)` |
|      - |  231 | `			);` |
|    ! 0 |  232 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  233 | `		return PH7_OK;` |
|      - |  234 | `	}` |
|      - |  235 | `	/* Point to the desired directory */` |
|   8507 |  236 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  237 | `	/* Perform the requested operation */` |
|   8507 |  238 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  239 | `	/* IO return value */` |
|   8507 |  240 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   8507 |  241 | `	return PH7_OK;` |
|   4256 |  242 | `}` |
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
|     36 |  262 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  263 | `{` |
|     38 |  264 | `	int iRecursive = 0;` |
|      - |  265 | `	const char *zPath;` |
|      - |  266 | `	ph7_vfs *pVfs;` |
|      - |  267 | `	int iMode,rc;` |
|     38 |  268 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  269 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  270 | `		ph7_result_bool(pCtx,0);` |
|      3 |  271 | `		return PH7_OK;` |
|      - |  272 | `	}` |
|      - |  273 | `	/* Point to the underlying vfs */` |
|     36 |  274 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     36 |  275 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  276 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  277 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  278 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  279 | `			ph7_function_name(pCtx)` |
|      - |  280 | `			);` |
|    ! 0 |  281 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  282 | `		return PH7_OK;` |
|      - |  283 | `	}` |
|      - |  284 | `	/* Point to the desired directory */` |
|     36 |  285 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  286 | `#ifdef __WINNT__` |
|      2 |  287 | `	iMode = 0;` |
|      - |  288 | `#else` |
|      - |  289 | `	/* Assume UNIX */` |
|     34 |  290 | `	iMode = 0777;` |
|      - |  291 | `#endif` |
|     36 |  292 | `	if( nArg > 1 ){` |
|    ! 0 |  293 | `		iMode = ph7_value_to_int(apArg[1]);` |
|    ! 0 |  294 | `		if( nArg > 2 ){` |
|    ! 0 |  295 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|    ! 0 |  296 | `		}` |
|    ! 0 |  297 | `	}` |
|      - |  298 | `	/* Perform the requested operation */` |
|     36 |  299 | `	rc = pVfs->xMkdir(zPath,iMode,iRecursive);` |
|      - |  300 | `	/* IO return value */` |
|     36 |  301 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     36 |  302 | `	return PH7_OK;` |
|     20 |  303 | `}` |
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
|     10 |  393 | `static int PH7_vfs_sleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  394 | `{` |
|      - |  395 | `	ph7_vfs *pVfs;` |
|      - |  396 | `	int rc,nSleep;` |
|     11 |  397 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  398 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  399 | `		ph7_result_bool(pCtx,0);` |
|      3 |  400 | `		return PH7_OK;` |
|      - |  401 | `	}` |
|      - |  402 | `	/* Point to the underlying vfs */` |
|      9 |  403 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 |  404 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  405 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  406 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  407 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  408 | `			ph7_function_name(pCtx)` |
|      - |  409 | `			);` |
|    ! 0 |  410 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  411 | `		return PH7_OK;` |
|      - |  412 | `	}` |
|      - |  413 | `	/* Amount to sleep */` |
|      9 |  414 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|      9 |  415 | `	if( nSleep < 0 ){` |
|      - |  416 | `		/* Invalid value,return FALSE */` |
|      3 |  417 | `		ph7_result_bool(pCtx,0);` |
|      3 |  418 | `		return PH7_OK;` |
|      - |  419 | `	}` |
|      - |  420 | `	/* Perform the requested operation (Microseconds) */` |
|      7 |  421 | `	rc = pVfs->xSleep((unsigned int)(nSleep * SX_USEC_PER_SEC));` |
|      7 |  422 | `	if( rc != PH7_OK ){` |
|      - |  423 | `		/* Return FALSE */` |
|    ! 0 |  424 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  425 | `	}else{` |
|      - |  426 | `		/* Return zero */` |
|      7 |  427 | `		ph7_result_int(pCtx,0);` |
|      - |  428 | `	}` |
|      7 |  429 | `	return PH7_OK;` |
|      6 |  430 | `}` |
|      - |  431 | `/*` |
|      - |  432 | ` * void usleep(int $micro_seconds)` |
|      - |  433 | ` *  Delays program execution for the given number of micro seconds.` |
|      - |  434 | ` * Parameters` |
|      - |  435 | ` *  $micro_seconds` |
|      - |  436 | ` *   Halt time in micro seconds. A micro second is one millionth of a second.` |
|      - |  437 | ` * Return` |
|      - |  438 | ` *  None.` |
|      - |  439 | ` */` |
|     56 |  440 | `static int PH7_vfs_usleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  441 | `{` |
|      - |  442 | `	ph7_vfs *pVfs;` |
|      - |  443 | `	int nSleep;` |
|     57 |  444 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  445 | `		/* Missing/Invalid argument,return immediately */` |
|    ! 0 |  446 | `		return PH7_OK;` |
|      - |  447 | `	}` |
|      - |  448 | `	/* Point to the underlying vfs */` |
|     57 |  449 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     57 |  450 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  451 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  452 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  453 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 |  454 | `			ph7_function_name(pCtx)` |
|      - |  455 | `			);` |
|    ! 0 |  456 | `		return PH7_OK;` |
|      - |  457 | `	}` |
|      - |  458 | `	/* Amount to sleep */` |
|     57 |  459 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|     57 |  460 | `	if( nSleep < 0 ){` |
|      - |  461 | `		/* Invalid value,return immediately */` |
|      3 |  462 | `		return PH7_OK;` |
|      - |  463 | `	}` |
|      - |  464 | `	/* Perform the requested operation (Microseconds) */` |
|     55 |  465 | `	pVfs->xSleep((unsigned int)nSleep);` |
|     55 |  466 | `	return PH7_OK;` |
|     29 |  467 | `}` |
|      - |  468 | `/*` |
|      - |  469 | ` * bool unlink (string $filename)` |
|      - |  470 | ` *  Delete a file.` |
|      - |  471 | ` * Parameters` |
|      - |  472 | ` *  $filename` |
|      - |  473 | ` *   Path to the file.` |
|      - |  474 | ` * Return` |
|      - |  475 | ` *  TRUE on success or FALSE on failure.` |
|      - |  476 | ` */` |
|  32724 |  477 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  478 | `{` |
|      - |  479 | `	const char *zPath;` |
|      - |  480 | `	ph7_vfs *pVfs;` |
|      - |  481 | `	int rc;` |
|  32729 |  482 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  483 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  484 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  485 | `		return PH7_OK;` |
|      - |  486 | `	}` |
|      - |  487 | `	/* Point to the underlying vfs */` |
|  32729 |  488 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  32729 |  489 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  490 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  491 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  492 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  493 | `			ph7_function_name(pCtx)` |
|      - |  494 | `			);` |
|    ! 0 |  495 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  496 | `		return PH7_OK;` |
|      - |  497 | `	}` |
|      - |  498 | `	/* Point to the desired directory */` |
|  32729 |  499 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  500 | `	/* Perform the requested operation */` |
|  32729 |  501 | `	rc = pVfs->xUnlink(zPath);` |
|      - |  502 | `	/* IO return value */` |
|  32729 |  503 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  32729 |  504 | `	return PH7_OK;` |
|  16367 |  505 | `}` |
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
|      1 |  518 | `{` |
|      - |  519 | `	const char *zPath;` |
|      - |  520 | `	ph7_vfs *pVfs;` |
|      - |  521 | `	int iMode;` |
|      - |  522 | `	int rc;` |
|     11 |  523 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  524 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  525 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  526 | `		return PH7_OK;` |
|      - |  527 | `	}` |
|      - |  528 | `	/* Point to the underlying vfs */` |
|     11 |  529 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 |  530 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - |  531 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  532 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  533 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  534 | `			ph7_function_name(pCtx)` |
|      - |  535 | `			);` |
|    ! 0 |  536 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  537 | `		return PH7_OK;` |
|      - |  538 | `	}` |
|      - |  539 | `	/* Point to the desired directory */` |
|     11 |  540 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  541 | `	/* Extract the mode */` |
|     11 |  542 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - |  543 | `	/* Perform the requested operation */` |
|     11 |  544 | `	rc = pVfs->xChmod(zPath,iMode);` |
|      - |  545 | `	/* IO return value */` |
|     11 |  546 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     11 |  547 | `	return PH7_OK;` |
|      6 |  548 | `}` |
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
|      6 |  642 | `static int PH7_vfs_disk_free_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  643 | `{` |
|      - |  644 | `	const char *zPath;` |
|      - |  645 | `	ph7_int64 iSize;` |
|      - |  646 | `	ph7_vfs *pVfs;` |
|      7 |  647 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  648 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  649 | `		ph7_result_bool(pCtx,0);` |
|      3 |  650 | `		return PH7_OK;` |
|      - |  651 | `	}` |
|      - |  652 | `	/* Point to the underlying vfs */` |
|      5 |  653 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  654 | `	if( pVfs == 0 \|\| pVfs->xFreeSpace == 0 ){` |
|      - |  655 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  656 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  657 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  658 | `			ph7_function_name(pCtx)` |
|      - |  659 | `			);` |
|    ! 0 |  660 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  661 | `		return PH7_OK;` |
|      - |  662 | `	}` |
|      - |  663 | `	/* Point to the desired directory */` |
|      5 |  664 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  665 | `	/* Perform the requested operation */` |
|      5 |  666 | `	iSize = pVfs->xFreeSpace(zPath);` |
|      - |  667 | `	/* IO return value */` |
|      5 |  668 | `	ph7_result_int64(pCtx,iSize);` |
|      5 |  669 | `	return PH7_OK;` |
|      4 |  670 | `}` |
|      - |  671 | `/*` |
|      - |  672 | ` * int64 disk_total_space(string $directory)` |
|      - |  673 | ` *  Returns the total size of a filesystem or disk partition.` |
|      - |  674 | ` * Parameters` |
|      - |  675 | ` *  $directory` |
|      - |  676 | ` *   A directory of the filesystem or disk partition.` |
|      - |  677 | ` * Return` |
|      - |  678 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  679 | ` */` |
|      4 |  680 | `static int PH7_vfs_disk_total_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  681 | `{` |
|      - |  682 | `	const char *zPath;` |
|      - |  683 | `	ph7_int64 iSize;` |
|      - |  684 | `	ph7_vfs *pVfs;` |
|      5 |  685 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  686 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  687 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  688 | `		return PH7_OK;` |
|      - |  689 | `	}` |
|      - |  690 | `	/* Point to the underlying vfs */` |
|      5 |  691 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  692 | `	if( pVfs == 0 \|\| pVfs->xTotalSpace == 0 ){` |
|      - |  693 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  694 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  695 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  696 | `			ph7_function_name(pCtx)` |
|      - |  697 | `			);` |
|    ! 0 |  698 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  699 | `		return PH7_OK;` |
|      - |  700 | `	}` |
|      - |  701 | `	/* Point to the desired directory */` |
|      5 |  702 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  703 | `	/* Perform the requested operation */` |
|      5 |  704 | `	iSize = pVfs->xTotalSpace(zPath);` |
|      - |  705 | `	/* IO return value */` |
|      5 |  706 | `	ph7_result_int64(pCtx,iSize);` |
|      5 |  707 | `	return PH7_OK;` |
|      3 |  708 | `}` |
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
|   6516 |  908 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  909 | `{` |
|      - |  910 | `	const char *zPath;` |
|      - |  911 | `	ph7_vfs *pVfs;` |
|      - |  912 | `	int rc;` |
|   6521 |  913 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  914 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  915 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  916 | `		return PH7_OK;` |
|      - |  917 | `	}` |
|      - |  918 | `	/* Point to the underlying vfs */` |
|   6521 |  919 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   6521 |  920 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - |  921 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  922 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  923 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  924 | `			ph7_function_name(pCtx)` |
|      - |  925 | `			);` |
|    ! 0 |  926 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  927 | `		return PH7_OK;` |
|      - |  928 | `	}` |
|      - |  929 | `	/* Point to the desired directory */` |
|   6521 |  930 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  931 | `	/* Perform the requested operation */` |
|   6521 |  932 | `	rc = pVfs->xIsfile(zPath);` |
|      - |  933 | `	/* IO return value */` |
|   6521 |  934 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   6521 |  935 | `	return PH7_OK;` |
|   3263 |  936 | `}` |
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
|      1 |  985 | `{` |
|      - |  986 | `	const char *zPath;` |
|      - |  987 | `	ph7_vfs *pVfs;` |
|      - |  988 | `	int rc;` |
|      3 |  989 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  990 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  991 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  992 | `		return PH7_OK;` |
|      - |  993 | `	}` |
|      - |  994 | `	/* Point to the underlying vfs */` |
|      3 |  995 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  996 | `	if( pVfs == 0 \|\| pVfs->xReadable == 0 ){` |
|      - |  997 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  998 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  999 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1000 | `			ph7_function_name(pCtx)` |
|      - | 1001 | `			);` |
|    ! 0 | 1002 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1003 | `		return PH7_OK;` |
|      - | 1004 | `	}` |
|      - | 1005 | `	/* Point to the desired directory */` |
|      3 | 1006 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1007 | `	/* Perform the requested operation */` |
|      3 | 1008 | `	rc = pVfs->xReadable(zPath);` |
|      - | 1009 | `	/* IO return value */` |
|      3 | 1010 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 1011 | `	return PH7_OK;` |
|      2 | 1012 | `}` |
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
|      1 | 1023 | `{` |
|      - | 1024 | `	const char *zPath;` |
|      - | 1025 | `	ph7_vfs *pVfs;` |
|      - | 1026 | `	int rc;` |
|      5 | 1027 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1028 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1029 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1030 | `		return PH7_OK;` |
|      - | 1031 | `	}` |
|      - | 1032 | `	/* Point to the underlying vfs */` |
|      5 | 1033 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1034 | `	if( pVfs == 0 \|\| pVfs->xWritable == 0 ){` |
|      - | 1035 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1036 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1037 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1038 | `			ph7_function_name(pCtx)` |
|      - | 1039 | `			);` |
|    ! 0 | 1040 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1041 | `		return PH7_OK;` |
|      - | 1042 | `	}` |
|      - | 1043 | `	/* Point to the desired directory */` |
|      5 | 1044 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1045 | `	/* Perform the requested operation */` |
|      5 | 1046 | `	rc = pVfs->xWritable(zPath);` |
|      - | 1047 | `	/* IO return value */` |
|      5 | 1048 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 1049 | `	return PH7_OK;` |
|      3 | 1050 | `}` |
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
|      1 | 1061 | `{` |
|      - | 1062 | `	const char *zPath;` |
|      - | 1063 | `	ph7_vfs *pVfs;` |
|      - | 1064 | `	int rc;` |
|      3 | 1065 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1066 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1067 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1068 | `		return PH7_OK;` |
|      - | 1069 | `	}` |
|      - | 1070 | `	/* Point to the underlying vfs */` |
|      3 | 1071 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1072 | `	if( pVfs == 0 \|\| pVfs->xExecutable == 0 ){` |
|      - | 1073 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1074 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1075 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1076 | `			ph7_function_name(pCtx)` |
|      - | 1077 | `			);` |
|    ! 0 | 1078 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1079 | `		return PH7_OK;` |
|      - | 1080 | `	}` |
|      - | 1081 | `	/* Point to the desired directory */` |
|      3 | 1082 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1083 | `	/* Perform the requested operation */` |
|      3 | 1084 | `	rc = pVfs->xExecutable(zPath);` |
|      - | 1085 | `	/* IO return value */` |
|      3 | 1086 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 1087 | `	return PH7_OK;` |
|      2 | 1088 | `}` |
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
|      1 | 1221 | `{` |
|      - | 1222 | `	ph7_value *pArray,*pValue;` |
|      - | 1223 | `	const char *zPath;` |
|      - | 1224 | `	ph7_vfs *pVfs;` |
|      - | 1225 | `	int rc;` |
|      3 | 1226 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1227 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1228 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1229 | `		return PH7_OK;` |
|      - | 1230 | `	}` |
|      - | 1231 | `	/* Point to the underlying vfs */` |
|      3 | 1232 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1233 | `	if( pVfs == 0 \|\| pVfs->xlStat == 0 ){` |
|      - | 1234 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1235 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1236 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1237 | `			ph7_function_name(pCtx)` |
|      - | 1238 | `			);` |
|    ! 0 | 1239 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1240 | `		return PH7_OK;` |
|      - | 1241 | `	}` |
|      - | 1242 | `	/* Create the array and the working value */` |
|      3 | 1243 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 1244 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 1245 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1246 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1247 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1248 | `		return PH7_OK;` |
|      - | 1249 | `	}` |
|      - | 1250 | `	/* Extract the file path */` |
|      3 | 1251 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1252 | `	/* Perform the requested operation */` |
|      3 | 1253 | `	rc = pVfs->xlStat(zPath,pArray,pValue);` |
|      3 | 1254 | `	if( rc != PH7_OK ){` |
|      - | 1255 | `		/* IO error,return FALSE */` |
|    ! 0 | 1256 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1257 | `	}else{` |
|      - | 1258 | `		/* Return the associative array */` |
|      3 | 1259 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1260 | `	}` |
|      - | 1261 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1262 | `	 * automatically as soon we return from this function. */` |
|      3 | 1263 | `	return PH7_OK;` |
|      2 | 1264 | `}` |
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
|     52 | 1275 | `static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1276 | `{` |
|      - | 1277 | `	const char *zEnv;` |
|      - | 1278 | `	ph7_vfs *pVfs;` |
|      - | 1279 | `	int iLen;` |
|     57 | 1280 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1281 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1282 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1283 | `		return PH7_OK;` |
|      - | 1284 | `	}` |
|      - | 1285 | `	/* Point to the underlying vfs */` |
|     57 | 1286 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     57 | 1287 | `	if( pVfs == 0 \|\| pVfs->xGetenv == 0 ){` |
|      - | 1288 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1289 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1290 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1291 | `			ph7_function_name(pCtx)` |
|      - | 1292 | `			);` |
|    ! 0 | 1293 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1294 | `		return PH7_OK;` |
|      - | 1295 | `	}` |
|      - | 1296 | `	/* Extract the environment variable */` |
|     57 | 1297 | `	zEnv = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1298 | `	/* Set a boolean FALSE as the default return value */` |
|     57 | 1299 | `	ph7_result_bool(pCtx,0);` |
|     57 | 1300 | `	if( iLen < 1 ){` |
|      - | 1301 | `		/* Empty string */` |
|    ! 0 | 1302 | `		return PH7_OK;` |
|      - | 1303 | `	}` |
|      - | 1304 | `	/* Perform the requested operation */` |
|     57 | 1305 | `	pVfs->xGetenv(zEnv,pCtx);` |
|     57 | 1306 | `	return PH7_OK;` |
|     31 | 1307 | `}` |
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
|  13022 | 1555 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 1556 | `{` |
|  13027 | 1557 | `	const char *zPtr,*zEnd = &zPath[nByte - 1];` |
|      - | 1558 | `	SyString *pCur;` |
|      - | 1559 | `	int c,d;` |
|  13027 | 1560 | `	c = d = '/';` |
|      - | 1561 | `#ifdef __WINNT__` |
|      5 | 1562 | `	d = '\\';` |
|      - | 1563 | `#endif` |
|      - | 1564 | `	/* Zero the structure */` |
|  13027 | 1565 | `	SyZero(pOut,sizeof(path_info));` |
|      - | 1566 | `	/* Handle special case */` |
|  13027 | 1567 | `	if( nByte == sizeof(char) && ( (int)zPath[0] == c \|\| (int)zPath[0] == d ) ){` |
|      - | 1568 | `#ifdef __WINNT__` |
|    ! 0 | 1569 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1570 | `#else` |
|    ! 0 | 1571 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1572 | `#endif` |
|    ! 0 | 1573 | `		return SXRET_OK;` |
|      - | 1574 | `	}` |
|      - | 1575 | `	/* Extract the basename */` |
| 351346 | 1576 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
| 331813 | 1577 | `		zEnd--;` |
|      5 | 1578 | `	}` |
|  13027 | 1579 | `	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|  13027 | 1580 | `	zEnd = &zPath[nByte];` |
|      - | 1581 | `	/* dirname */` |
|  13027 | 1582 | `	pCur = &pOut->sDir;` |
|  13027 | 1583 | `	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);` |
|  13027 | 1584 | `	if( pCur->nByte > 1 ){` |
|  26049 | 1585 | `		SyStringTrimTrailingChar(pCur,'/');` |
|      - | 1586 | `#ifdef __WINNT__` |
|      5 | 1587 | `		SyStringTrimTrailingChar(pCur,'\\');` |
|      - | 1588 | `#endif` |
|   6516 | 1589 | `	}else if( (int)zPath[0] == c \|\| (int)zPath[0] == d ){` |
|      - | 1590 | `#ifdef __WINNT__` |
|    ! 0 | 1591 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1592 | `#else` |
|    ! 0 | 1593 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1594 | `#endif` |
|    ! 0 | 1595 | `	}` |
|      - | 1596 | `	/* basename/filename */` |
|  13027 | 1597 | `	pCur = &pOut->sBasename;` |
|  13027 | 1598 | `	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);` |
|  13027 | 1599 | `	SyStringTrimLeadingChar(pCur,'/');` |
|      - | 1600 | `#ifdef __WINNT__` |
|      5 | 1601 | `	SyStringTrimLeadingChar(pCur,'\\');` |
|      - | 1602 | `#endif` |
|  13027 | 1603 | `	SyStringDupPtr(&pOut->sFilename,pCur);` |
|  13027 | 1604 | `	if( pCur->nByte > 0 ){` |
|      - | 1605 | `		/* extension */` |
|  13027 | 1606 | `		zEnd--;` |
|  65109 | 1607 | `		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){` |
|  52087 | 1608 | `			zEnd--;` |
|      5 | 1609 | `		}` |
|  13027 | 1610 | `		if( zEnd > pCur->zString ){` |
|  13025 | 1611 | `			zEnd++; /* Jump leading dot */` |
|  13025 | 1612 | `			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);` |
|      - | 1613 | `			/* Fix filename */` |
|  13025 | 1614 | `			pCur = &pOut->sFilename;` |
|  13025 | 1615 | `			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){` |
|  13025 | 1616 | `				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);` |
|   6510 | 1617 | `			}` |
|   6510 | 1618 | `		}` |
|   6511 | 1619 | `	}` |
|  13027 | 1620 | `	return SXRET_OK;` |
|   6516 | 1621 | `}` |
|      - | 1622 | `/*` |
|      - | 1623 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1624 | ` *  See block comment above.` |
|      - | 1625 | ` */` |
|  13022 | 1626 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1627 | `{` |
|      - | 1628 | `	const char *zPath;` |
|      - | 1629 | `	path_info sInfo;` |
|      - | 1630 | `	SyString *pComp;` |
|      - | 1631 | `	int iLen;` |
|  13027 | 1632 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1633 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1634 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1635 | `		return PH7_OK;` |
|      - | 1636 | `	}` |
|      - | 1637 | `	/* Point to the target path */` |
|  13027 | 1638 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|  13027 | 1639 | `	if( iLen < 1 ){` |
|      - | 1640 | `		/* Empty string */` |
|    ! 0 | 1641 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1642 | `		return PH7_OK;` |
|      - | 1643 | `	}` |
|      - | 1644 | `	/* Extract path info */` |
|  13027 | 1645 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|  19537 | 1646 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1647 | `		/* Return path component */` |
|  13025 | 1648 | `		int nComp = ph7_value_to_int(apArg[1]);` |
|  13025 | 1649 | `		switch(nComp){` |
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
|   3256 | 1668 | `		case 3: /*PATHINFO_EXTENSION*/` |
|   6517 | 1669 | `			pComp = &sInfo.sExtension;` |
|   6517 | 1670 | `			if( pComp->nByte > 0 ){` |
|   6515 | 1671 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3260 | 1672 | `			}else{` |
|      - | 1673 | `				/* Expand the empty string */` |
|      3 | 1674 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1675 | `			}` |
|   6517 | 1676 | `			break;` |
|   3252 | 1677 | `		case 4: /*PATHINFO_FILENAME*/` |
|   6509 | 1678 | `			pComp = &sInfo.sFilename;` |
|   6509 | 1679 | `			if( pComp->nByte > 0 ){` |
|   6509 | 1680 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3257 | 1681 | `			}else{` |
|      - | 1682 | `				/* Expand the empty string */` |
|    ! 0 | 1683 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1684 | `			}` |
|   6509 | 1685 | `			break;` |
|    ! 0 | 1686 | `		default:` |
|      - | 1687 | `			/* Expand the empty string */` |
|    ! 0 | 1688 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1689 | `			break;` |
|      - | 1690 | `		}` |
|   6515 | 1691 | `	}else{` |
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
|  13027 | 1741 | `	return PH7_OK;` |
|   6516 | 1742 | `}` |
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
|      7 | 1969 | `		if( rc & 2 /*FNM_NOESCAPE (php value)*/){` |
|    ! 0 | 1970 | `			iEsc = 0;` |
|    ! 0 | 1971 | `		}` |
|      7 | 1972 | `		if( rc & 16 /*FNM_CASEFOLD (php value)*/){` |
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
|      1 | 2027 | `{` |
|      - | 2028 | `	const char *zTarget,*zLink;` |
|      - | 2029 | `	ph7_vfs *pVfs;` |
|      - | 2030 | `	int rc;` |
|      3 | 2031 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2032 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2033 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2034 | `		return PH7_OK;` |
|      - | 2035 | `	}` |
|      - | 2036 | `	/* Point to the underlying vfs */` |
|      3 | 2037 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2038 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2039 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2040 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2041 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2042 | `			ph7_function_name(pCtx)` |
|      - | 2043 | `			);` |
|    ! 0 | 2044 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2045 | `		return PH7_OK;` |
|      - | 2046 | `	}` |
|      - | 2047 | `	/* Extract the given arguments */` |
|      3 | 2048 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      3 | 2049 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2050 | `	/* Perform the requested operation */` |
|      3 | 2051 | `	rc = pVfs->xLink(zTarget,zLink,0/*Not a symbolic link */);` |
|      - | 2052 | `	/* IO result */` |
|      3 | 2053 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 2054 | `	return PH7_OK;` |
|      2 | 2055 | `}` |
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
|      1 | 2068 | `{` |
|      - | 2069 | `	const char *zTarget,*zLink;` |
|      - | 2070 | `	ph7_vfs *pVfs;` |
|      - | 2071 | `	int rc;` |
|      7 | 2072 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2073 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2074 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2075 | `		return PH7_OK;` |
|      - | 2076 | `	}` |
|      - | 2077 | `	/* Point to the underlying vfs */` |
|      7 | 2078 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 2079 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2080 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2081 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2082 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2083 | `			ph7_function_name(pCtx)` |
|      - | 2084 | `			);` |
|    ! 0 | 2085 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2086 | `		return PH7_OK;` |
|      - | 2087 | `	}` |
|      - | 2088 | `	/* Extract the given arguments */` |
|      7 | 2089 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      7 | 2090 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2091 | `	/* Perform the requested operation */` |
|      7 | 2092 | `	rc = pVfs->xLink(zTarget,zLink,1/*A symbolic link */);` |
|      - | 2093 | `	/* IO result */` |
|      7 | 2094 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      7 | 2095 | `	return PH7_OK;` |
|      4 | 2096 | `}` |
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
|      1 | 2108 | `{` |
|      - | 2109 | `	int iOld,iNew;` |
|      - | 2110 | `	ph7_vfs *pVfs;` |
|      - | 2111 | `	/* Point to the underlying vfs */` |
|      9 | 2112 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 | 2113 | `	if( pVfs == 0 \|\| pVfs->xUmask == 0 ){` |
|      - | 2114 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2115 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2116 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2117 | `			ph7_function_name(pCtx)` |
|      - | 2118 | `			);` |
|    ! 0 | 2119 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2120 | `		return PH7_OK;` |
|      - | 2121 | `	}` |
|      9 | 2122 | `	iNew = 0;` |
|      9 | 2123 | `	if( nArg > 0 ){` |
|      5 | 2124 | `		iNew = ph7_value_to_int(apArg[0]);` |
|      2 | 2125 | `	}` |
|      - | 2126 | `	/* Perform the requested operation */` |
|      9 | 2127 | `	iOld = pVfs->xUmask(iNew);` |
|      - | 2128 | `	/* Old mask */` |
|      9 | 2129 | `	ph7_result_int(pCtx,iOld);` |
|      9 | 2130 | `	return PH7_OK;` |
|      5 | 2131 | `}` |
|      - | 2132 | `/*` |
|      - | 2133 | ` * string sys_get_temp_dir()` |
|      - | 2134 | ` *  Returns directory path used for temporary files.` |
|      - | 2135 | ` * Parameters` |
|      - | 2136 | ` *  None` |
|      - | 2137 | ` * Return` |
|      - | 2138 | ` *  Returns the path of the temporary directory.` |
|      - | 2139 | ` */` |
|    218 | 2140 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2141 | `{` |
|      - | 2142 | `	ph7_vfs *pVfs;` |
|      - | 2143 | `	/* Set the empty string as the default return value */` |
|    221 | 2144 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2145 | `	/* Point to the underlying vfs */` |
|    221 | 2146 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    221 | 2147 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
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
|    221 | 2158 | `	pVfs->xTempDir(pCtx);` |
|    221 | 2159 | `	return PH7_OK;` |
|    112 | 2160 | `}` |
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
|     74 | 2198 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2199 | `{` |
|      - | 2200 | `	ph7_int64 nProcessId;` |
|      - | 2201 | `	ph7_vfs *pVfs;` |
|      - | 2202 | `	/* Point to the underlying vfs */` |
|     76 | 2203 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     76 | 2204 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
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
|     76 | 2216 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 2217 | `	/* Set the result */` |
|     76 | 2218 | `	ph7_result_int64(pCtx,nProcessId);` |
|     76 | 2219 | `	return PH7_OK;` |
|     39 | 2220 | `}` |
|      - | 2221 | `/*` |
|      - | 2222 | ` * int getmyuid()` |
|      - | 2223 | ` *  Get user ID.` |
|      - | 2224 | ` * Parameters` |
|      - | 2225 | ` *  None` |
|      - | 2226 | ` * Return` |
|      - | 2227 | ` *  Returns the user ID.` |
|      - | 2228 | ` */` |
|      2 | 2229 | `static int PH7_vfs_getmyuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2230 | `{` |
|      - | 2231 | `	ph7_vfs *pVfs;` |
|      - | 2232 | `	int nUid;` |
|      - | 2233 | `	/* Point to the underlying vfs */` |
|      3 | 2234 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2235 | `	if( pVfs == 0 \|\| pVfs->xUid == 0 ){` |
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
|      3 | 2247 | `	nUid = pVfs->xUid();` |
|      - | 2248 | `	/* Set the result */` |
|      3 | 2249 | `	ph7_result_int(pCtx,nUid);` |
|      3 | 2250 | `	return PH7_OK;` |
|      2 | 2251 | `}` |
|      - | 2252 | `/*` |
|      - | 2253 | ` * int getmygid()` |
|      - | 2254 | ` *  Get group ID.` |
|      - | 2255 | ` * Parameters` |
|      - | 2256 | ` *  None` |
|      - | 2257 | ` * Return` |
|      - | 2258 | ` *  Returns the group ID.` |
|      - | 2259 | ` */` |
|      2 | 2260 | `static int PH7_vfs_getmygid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2261 | `{` |
|      - | 2262 | `	ph7_vfs *pVfs;` |
|      - | 2263 | `	int nGid;` |
|      - | 2264 | `	/* Point to the underlying vfs */` |
|      3 | 2265 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2266 | `	if( pVfs == 0 \|\| pVfs->xGid == 0 ){` |
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
|      3 | 2278 | `	nGid = pVfs->xGid();` |
|      - | 2279 | `	/* Set the result */` |
|      3 | 2280 | `	ph7_result_int(pCtx,nGid);` |
|      3 | 2281 | `	return PH7_OK;` |
|      2 | 2282 | `}` |
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
|     10 | 2528 | `static int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2529 | `{` |
|      - | 2530 | `	const ph7_io_stream *pStream;` |
|      - | 2531 | `	io_private *pDev;` |
|      - | 2532 | `	ph7_int64 iOfft;` |
|      - | 2533 | `	int whence;` |
|      - | 2534 | `	int rc;` |
|     12 | 2535 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2536 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2537 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2538 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2539 | `		return PH7_OK;` |
|      - | 2540 | `	}` |
|      - | 2541 | `	/* Extract our private data */` |
|     12 | 2542 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2543 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     12 | 2544 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2545 | `		/*Expecting an IO handle */` |
|    ! 0 | 2546 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2547 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2548 | `		return PH7_OK;` |
|      - | 2549 | `	}` |
|      - | 2550 | `	/* Point to the target IO stream device */` |
|     12 | 2551 | `	pStream = pDev->pStream;` |
|     12 | 2552 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2553 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2554 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2555 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2556 | `			);` |
|    ! 0 | 2557 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2558 | `		return PH7_OK;` |
|      - | 2559 | `	}` |
|      - | 2560 | `	/* Extract the offset */` |
|     12 | 2561 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|     12 | 2562 | `	whence = 0;/* SEEK_SET */` |
|     12 | 2563 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      3 | 2564 | `		whence = ph7_value_to_int(apArg[2]);` |
|      1 | 2565 | `	}` |
|      - | 2566 | `	/* Perform the requested operation */` |
|     12 | 2567 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|     12 | 2568 | `	if( rc == PH7_OK ){` |
|      - | 2569 | `		/* Ignore buffered data */` |
|     12 | 2570 | `		ResetIOPrivate(pDev);` |
|      5 | 2571 | `	}` |
|      - | 2572 | `	/* IO result */` |
|     12 | 2573 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|     12 | 2574 | `	return PH7_OK;` |
|      7 | 2575 | `}` |
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
|     12 | 2587 | `static int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2588 | `{` |
|      - | 2589 | `	const ph7_io_stream *pStream;` |
|      - | 2590 | `	io_private *pDev;` |
|      - | 2591 | `	ph7_int64 iOfft;` |
|     14 | 2592 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2593 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2594 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2595 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2596 | `		return PH7_OK;` |
|      - | 2597 | `	}` |
|      - | 2598 | `	/* Extract our private data */` |
|     14 | 2599 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2600 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     14 | 2601 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2602 | `		/*Expecting an IO handle */` |
|    ! 0 | 2603 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2604 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2605 | `		return PH7_OK;` |
|      - | 2606 | `	}` |
|      - | 2607 | `	/* Point to the target IO stream device */` |
|     14 | 2608 | `	pStream = pDev->pStream;` |
|     14 | 2609 | `	if( pStream == 0  \|\| pStream->xTell == 0){` |
|    ! 0 | 2610 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2611 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2612 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2613 | `			);` |
|    ! 0 | 2614 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2615 | `		return PH7_OK;` |
|      - | 2616 | `	}` |
|      - | 2617 | `	/* Perform the requested operation */` |
|     14 | 2618 | `	iOfft = pStream->xTell(pDev->pHandle);` |
|      - | 2619 | `	/* IO result */` |
|     14 | 2620 | `	ph7_result_int64(pCtx,iOfft);` |
|     14 | 2621 | `	return PH7_OK;` |
|      8 | 2622 | `}` |
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
|  10722 | 2726 | `static int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2727 | `{` |
|      - | 2728 | `	const ph7_io_stream *pStream;` |
|      - | 2729 | `	io_private *pDev;` |
|      - | 2730 | `	int rc;` |
|  10727 | 2731 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2732 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 2733 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2734 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2735 | `		return PH7_OK;` |
|      - | 2736 | `	}` |
|      - | 2737 | `	/* Extract our private data */` |
|  10727 | 2738 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2739 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  10727 | 2740 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2741 | `		/*Expecting an IO handle */` |
|    ! 0 | 2742 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2743 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2744 | `		return PH7_OK;` |
|      - | 2745 | `	}` |
|      - | 2746 | `	/* Point to the target IO stream device */` |
|  10727 | 2747 | `	pStream = pDev->pStream;` |
|  10727 | 2748 | `	if( pStream == 0 ){` |
|    ! 0 | 2749 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2750 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2751 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2752 | `			);` |
|    ! 0 | 2753 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2754 | `		return PH7_OK;` |
|      - | 2755 | `	}` |
|  10727 | 2756 | `	rc = SXERR_EOF;` |
|      - | 2757 | `	/* Perform the requested operation */` |
|  10727 | 2758 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2759 | `		/* Data is available */` |
|   4937 | 2760 | `		rc = PH7_OK;` |
|   2471 | 2761 | `	}else{` |
|      - | 2762 | `		char zBuf[4096];` |
|      - | 2763 | `		ph7_int64 n;` |
|      - | 2764 | `		/* Perform a buffered read */` |
|   5795 | 2765 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|   5795 | 2766 | `		if( n > 0 ){` |
|      - | 2767 | `			/* Copy buffered data */` |
|   1831 | 2768 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|   1831 | 2769 | `			rc = PH7_OK;` |
|    913 | 2770 | `		}` |
|      - | 2771 | `	}` |
|      - | 2772 | `	/* EOF or not */` |
|  10727 | 2773 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|  10727 | 2774 | `	return PH7_OK;` |
|   5366 | 2775 | `}` |
|      - | 2776 | `/*` |
|      - | 2777 | ` * Read n bytes from the underlying IO stream device.` |
|      - | 2778 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|      - | 2779 | ` * [i.e: IO error ] or EOF.` |
|      - | 2780 | ` */` |
|     36 | 2781 | `static ph7_int64 StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      2 | 2782 | `{` |
|     38 | 2783 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|     38 | 2784 | `	char *zBuf = (char *)pBuf;` |
|      - | 2785 | `	ph7_int64 n,nRead;` |
|     38 | 2786 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     38 | 2787 | `	if( n > 0 ){` |
|      2 | 2788 | `		if( n > nLen ){` |
|    ! 0 | 2789 | `			n = nLen;` |
|    ! 0 | 2790 | `		}` |
|      - | 2791 | `		/* Copy the buffered data */` |
|      2 | 2792 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|      - | 2793 | `		/* Update the read offset */` |
|      2 | 2794 | `		pDev->nOfft += (sxu32)n;` |
|      2 | 2795 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2796 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|      2 | 2797 | `			SyBlobReset(&pDev->sBuffer);` |
|      2 | 2798 | `			pDev->nOfft = 0;` |
|      1 | 2799 | `		}` |
|      2 | 2800 | `		nLen -= n;` |
|      2 | 2801 | `		if( nLen < 1 ){` |
|      - | 2802 | `			/* All done */` |
|    ! 0 | 2803 | `			return n;` |
|      - | 2804 | `		}` |
|      - | 2805 | `		/* Advance the cursor */` |
|      2 | 2806 | `		zBuf += n;` |
|      1 | 2807 | `	}` |
|      - | 2808 | `	/* Read without buffering */` |
|     38 | 2809 | `	nRead = pStream->xRead(pDev->pHandle,zBuf,nLen);` |
|     38 | 2810 | `	if( nRead > 0 ){` |
|     34 | 2811 | `		n += nRead;` |
|     21 | 2812 | `	}else if( n < 1 ){` |
|      - | 2813 | `		/* EOF or IO error */` |
|      3 | 2814 | `		return nRead;` |
|      - | 2815 | `	}` |
|     36 | 2816 | `	return n;` |
|     20 | 2817 | `}` |
|      - | 2818 | `/*` |
|      - | 2819 | ` * Extract a single line from the buffered input.` |
|      - | 2820 | ` */` |
|   6824 | 2821 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      5 | 2822 | `{` |
|      - | 2823 | `	const char *zIn,*zEnd,*zPtr;` |
|   6829 | 2824 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|   6829 | 2825 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|   6829 | 2826 | `	zPtr = zIn;` |
| 394710 | 2827 | `	while( zIn < zEnd ){` |
| 394610 | 2828 | `		if( zIn[0] == '\n' ){` |
|      - | 2829 | `			/* Line found */` |
|   6729 | 2830 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|   6729 | 2831 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|   6729 | 2832 | `			*pzLine = zPtr;` |
|   6729 | 2833 | `			return SXRET_OK;` |
|      - | 2834 | `		}` |
| 387886 | 2835 | `		zIn++;` |
|      5 | 2836 | `	}` |
|      - | 2837 | `	/* No line were found */` |
|    105 | 2838 | `	return SXERR_NOTFOUND;` |
|   3417 | 2839 | `}` |
|      - | 2840 | `/*` |
|      - | 2841 | ` * Read a single line from the underlying IO stream device.` |
|      - | 2842 | ` */` |
|   6828 | 2843 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      5 | 2844 | `{` |
|   6833 | 2845 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - | 2846 | `	char zBuf[8192];` |
|      - | 2847 | `	ph7_int64 n;` |
|      - | 2848 | `	sxi32 rc;` |
|   6833 | 2849 | `	n = 0;` |
|   6833 | 2850 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2851 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     73 | 2852 | `		SyBlobReset(&pDev->sBuffer);` |
|     73 | 2853 | `		pDev->nOfft = 0;` |
|     34 | 2854 | `	}` |
|   6833 | 2855 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2856 | `		/* Check if there is a line */` |
|   6765 | 2857 | `		rc = GetLine(pDev,&n,pzData);` |
|   6765 | 2858 | `		if( rc == SXRET_OK ){` |
|      - | 2859 | `			/* Got line,update the cursor  */` |
|   6669 | 2860 | `			pDev->nOfft += (sxu32)n;` |
|   6669 | 2861 | `			return n;` |
|      - | 2862 | `		}` |
|     48 | 2863 | `	}` |
|      - | 2864 | `	/* Perform the read operation until a new line is extracted or length` |
|      - | 2865 | `	 * limit is reached.` |
|      - | 2866 | `	 */` |
|     84 | 2867 | `	for(;;){` |
|    173 | 2868 | `		n = pStream->xRead(pDev->pHandle,zBuf, (nMaxLen > 0 && nMaxLen < (ph7_int64)sizeof(zBuf)) ? nMaxLen : (ph7_int64)sizeof(zBuf));` |
|    173 | 2869 | `		if( n < 1 ){` |
|      - | 2870 | `			/* EOF or IO error */` |
|    109 | 2871 | `			break;` |
|      - | 2872 | `		}` |
|      - | 2873 | `		/* Append the data just read */` |
|     67 | 2874 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - | 2875 | `		/* Try to extract a line */` |
|     67 | 2876 | `		rc = GetLine(pDev,&n,pzData);` |
|     67 | 2877 | `		if( rc == SXRET_OK ){` |
|      - | 2878 | `			/* Got one,return immediately */` |
|     63 | 2879 | `			pDev->nOfft += (sxu32)n;` |
|     63 | 2880 | `			return n;` |
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
|   3419 | 2901 | `}` |
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
|  29930 | 2923 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - | 2924 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      5 | 2925 | `{` |
|  29935 | 2926 | `	void *pHandle = 0; /* cc warning */` |
|      - | 2927 | `	SyString sFile;` |
|      - | 2928 | `	ph7_value sDummy;` |
|      - | 2929 | `	int rc;` |
|  29935 | 2930 | `	if( pStream == 0 ){` |
|      - | 2931 | `		/* No such stream device */` |
|    ! 0 | 2932 | `		return 0;` |
|      - | 2933 | `	}` |
|  29935 | 2934 | `	if( pResource == 0 ){` |
|      - | 2935 | `		/* VM-dependent devices (php://, data://, tcp://, userland wrappers)` |
|      - | 2936 | `		 * reach the VM only through pResource->pVm — their xOpen has no vm` |
|      - | 2937 | `		 * parameter. Callers like file_get_contents pass no resource, so hand` |
|      - | 2938 | `		 * every device a synthesized stack value carrying the VM; xOpen only` |
|      - | 2939 | `		 * reads it during the call, and file:// ignores it. */` |
|  29917 | 2940 | `		PH7_MemObjInit(pVm,&sDummy);` |
|  29917 | 2941 | `		pResource = &sDummy;` |
|  14956 | 2942 | `	}` |
|  29935 | 2943 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  29935 | 2944 | `	if( use_include ){` |
|   9586 | 2945 | `		if(	sFile.zString[0] == '/' \|\|` |
|      - | 2946 | `#ifdef __WINNT__` |
|      - | 2947 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - | 2948 | `#endif` |
|   9572 | 2949 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   9568 | 2950 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - | 2951 | `				/*  Open the file directly */` |
|     19 | 2952 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|     10 | 2953 | `		}else{` |
|      - | 2954 | `			SyString *pPath;` |
|      - | 2955 | `			SyBlob sWorker;` |
|      - | 2956 | `#ifdef __WINNT__` |
|      - | 2957 | `			static const int c = '\\';` |
|      - | 2958 | `#else` |
|      - | 2959 | `			static const int c = '/';` |
|      - | 2960 | `#endif` |
|      - | 2961 | `			/* Init the path builder working buffer */` |
|   9572 | 2962 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - | 2963 | `			/* Build a path from the set of include path */` |
|   9572 | 2964 | `			SySetResetCursor(&pVm->aPaths);` |
|   9572 | 2965 | `			rc = SXERR_IO;` |
|   9578 | 2966 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - | 2967 | `				/* Build full path */` |
|   9572 | 2968 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - | 2969 | `				/* Append null terminator */` |
|   9572 | 2970 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 2971 | `					continue;` |
|      - | 2972 | `				}` |
|      - | 2973 | `				/* Try to open the file */` |
|   9572 | 2974 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   9572 | 2975 | `				if( rc == PH7_OK ){` |
|   9566 | 2976 | `					if( bPushInclude ){` |
|      - | 2977 | `						/* Mark as included */` |
|   9566 | 2978 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   4781 | 2979 | `					}` |
|   9566 | 2980 | `					break;` |
|      - | 2981 | `				}` |
|      - | 2982 | `				/* Reset the working buffer */` |
|      8 | 2983 | `				SyBlobReset(&sWorker);` |
|      - | 2984 | `				/* Check the next path */` |
|      2 | 2985 | `			}` |
|   9572 | 2986 | `			SyBlobRelease(&sWorker);` |
|      - | 2987 | `		}` |
|   9590 | 2988 | `		if( rc == PH7_OK ){` |
|   9584 | 2989 | `			if( bPushInclude ){` |
|      - | 2990 | `				/* Mark as included */` |
|   9584 | 2991 | `				PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|   4790 | 2992 | `			}` |
|   4790 | 2993 | `		}` |
|   4797 | 2994 | `	}else{` |
|      - | 2995 | `		/* Open the URI direcly */` |
|  20349 | 2996 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - | 2997 | `	}` |
|  29935 | 2998 | `	if( rc != PH7_OK ){` |
|      - | 2999 | `		/* IO error */` |
|     15 | 3000 | `		return 0;` |
|      - | 3001 | `	}` |
|      - | 3002 | `	/* Return the file handle */` |
|  29923 | 3003 | `	return pHandle;` |
|  14970 | 3004 | `}` |
|      - | 3005 | `/*` |
|      - | 3006 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - | 3007 | ` * Store the read data in the given BLOB (last argument).` |
|      - | 3008 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - | 3009 | ` */` |
|   9574 | 3010 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      4 | 3011 | `{` |
|      - | 3012 | `	ph7_int64 nRead;` |
|      - | 3013 | `	char zBuf[8192]; /* 8K */` |
|      - | 3014 | `	int rc;` |
|      - | 3015 | `	/* Perform the requested operation */` |
|   9574 | 3016 | `	for(;;){` |
|  19152 | 3017 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  19152 | 3018 | `		if( nRead < 1 ){` |
|      - | 3019 | `			/* EOF or IO error */` |
|   9578 | 3020 | `			break;` |
|      - | 3021 | `		}` |
|      - | 3022 | `		/* Append contents */` |
|   9578 | 3023 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   9578 | 3024 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3025 | `			break;` |
|      - | 3026 | `		}` |
|      4 | 3027 | `	}` |
|   9578 | 3028 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      4 | 3029 | `}` |
|      - | 3030 | `/*` |
|      - | 3031 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - | 3032 | ` */` |
|  30016 | 3033 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      5 | 3034 | `{` |
|  30021 | 3035 | `	if( pStream->xClose ){` |
|  30021 | 3036 | `		pStream->xClose(pHandle);` |
|  15008 | 3037 | `	}` |
|  30021 | 3038 | `}` |
|      - | 3039 | `/*` |
|      - | 3040 | ` * string fgetc(resource $handle)` |
|      - | 3041 | ` *  Gets a character from the given file pointer.` |
|      - | 3042 | ` * Parameters` |
|      - | 3043 | ` *  $handle` |
|      - | 3044 | ` *   The file pointer.` |
|      - | 3045 | ` * Return` |
|      - | 3046 | ` *  Returns a string containing a single character read from the file` |
|      - | 3047 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|      - | 3048 | ` * WARNING` |
|      - | 3049 | ` *  This operation is extremely slow.Avoid using it.` |
|      - | 3050 | ` */` |
|      4 | 3051 | `static int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3052 | `{` |
|      - | 3053 | `	const ph7_io_stream *pStream;` |
|      - | 3054 | `	io_private *pDev;` |
|      - | 3055 | `	int c,n;` |
|      5 | 3056 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3057 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3058 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3059 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3060 | `		return PH7_OK;` |
|      - | 3061 | `	}` |
|      - | 3062 | `	/* Extract our private data */` |
|      5 | 3063 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3064 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 3065 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3066 | `		/*Expecting an IO handle */` |
|    ! 0 | 3067 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3068 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3069 | `		return PH7_OK;` |
|      - | 3070 | `	}` |
|      - | 3071 | `	/* Point to the target IO stream device */` |
|      5 | 3072 | `	pStream = pDev->pStream;` |
|      5 | 3073 | `	if( pStream == 0  ){` |
|    ! 0 | 3074 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3075 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3076 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3077 | `			);` |
|    ! 0 | 3078 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3079 | `		return PH7_OK;` |
|      - | 3080 | `	}` |
|      - | 3081 | `	/* Perform the requested operation */` |
|      5 | 3082 | `	n = (int)StreamRead(pDev,(void *)&c,sizeof(char));` |
|      - | 3083 | `	/* IO result */` |
|      5 | 3084 | `	if( n < 1 ){` |
|      - | 3085 | `		/* EOF or error,return FALSE */` |
|    ! 0 | 3086 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3087 | `	}else{` |
|      - | 3088 | `		/* Return the string holding the character */` |
|      5 | 3089 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|      - | 3090 | `	}` |
|      5 | 3091 | `	return PH7_OK;` |
|      3 | 3092 | `}` |
|      - | 3093 | `/*` |
|      - | 3094 | ` * string fgets(resource $handle[,int64 $length ])` |
|      - | 3095 | ` *  Gets line from file pointer.` |
|      - | 3096 | ` * Parameters` |
|      - | 3097 | ` *  $handle` |
|      - | 3098 | ` *   The file pointer.` |
|      - | 3099 | ` * $length` |
|      - | 3100 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3101 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3102 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3103 | ` *  the end of the line.` |
|      - | 3104 | ` * Return` |
|      - | 3105 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3106 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3107 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3108 | ` */` |
|   6818 | 3109 | `static int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3110 | `{` |
|      - | 3111 | `	const ph7_io_stream *pStream;` |
|      - | 3112 | `	const char *zLine;` |
|      - | 3113 | `	io_private *pDev;` |
|      - | 3114 | `	ph7_int64 n,nLen;` |
|   6823 | 3115 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3116 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3117 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3118 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3119 | `		return PH7_OK;` |
|      - | 3120 | `	}` |
|      - | 3121 | `	/* Extract our private data */` |
|   6823 | 3122 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3123 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   6823 | 3124 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3125 | `		/*Expecting an IO handle */` |
|    ! 0 | 3126 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3127 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3128 | `		return PH7_OK;` |
|      - | 3129 | `	}` |
|      - | 3130 | `	/* Point to the target IO stream device */` |
|   6823 | 3131 | `	pStream = pDev->pStream;` |
|   6823 | 3132 | `	if( pStream == 0  ){` |
|    ! 0 | 3133 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3134 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3135 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3136 | `			);` |
|    ! 0 | 3137 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3138 | `		return PH7_OK;` |
|      - | 3139 | `	}` |
|   6823 | 3140 | `	nLen = -1;` |
|   6823 | 3141 | `	if( nArg > 1 ){` |
|      - | 3142 | `		/* Maximum data to read */` |
|    ! 0 | 3143 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3144 | `	}` |
|      - | 3145 | `	/* Perform the requested operation */` |
|   6823 | 3146 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|   6823 | 3147 | `	if( n < 1 ){` |
|      - | 3148 | `		/* EOF or IO error,return FALSE */` |
|      7 | 3149 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3150 | `	}else{` |
|      - | 3151 | `		/* Return the freshly extracted line */` |
|   6821 | 3152 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - | 3153 | `	}` |
|   6823 | 3154 | `	return PH7_OK;` |
|   3414 | 3155 | `}` |
|      - | 3156 | `/*` |
|      - | 3157 | ` * string fread(resource $handle,int64 $length)` |
|      - | 3158 | ` *  Binary-safe file read.` |
|      - | 3159 | ` * Parameters` |
|      - | 3160 | ` *  $handle` |
|      - | 3161 | ` *   The file pointer.` |
|      - | 3162 | ` * $length` |
|      - | 3163 | ` *  Up to length number of bytes read.` |
|      - | 3164 | ` * Return` |
|      - | 3165 | ` *  The data readen on success or FALSE on failure.` |
|      - | 3166 | ` */` |
|     28 | 3167 | `static int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3168 | `{` |
|      - | 3169 | `	const ph7_io_stream *pStream;` |
|      - | 3170 | `	io_private *pDev;` |
|      - | 3171 | `	ph7_int64 nRead;` |
|      - | 3172 | `	void *pBuf;` |
|      - | 3173 | `	int nLen;` |
|     30 | 3174 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3175 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3176 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3177 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3178 | `		return PH7_OK;` |
|      - | 3179 | `	}` |
|      - | 3180 | `	/* Extract our private data */` |
|     30 | 3181 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3182 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     30 | 3183 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3184 | `		/*Expecting an IO handle */` |
|    ! 0 | 3185 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3186 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3187 | `		return PH7_OK;` |
|      - | 3188 | `	}` |
|      - | 3189 | `	/* Point to the target IO stream device */` |
|     30 | 3190 | `	pStream = pDev->pStream;` |
|     30 | 3191 | `	if( pStream == 0  ){` |
|    ! 0 | 3192 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3193 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3194 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3195 | `			);` |
|    ! 0 | 3196 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3197 | `		return PH7_OK;` |
|      - | 3198 | `	}` |
|     30 | 3199 | `        nLen = 4096;` |
|     30 | 3200 | `	if( nArg > 1 ){` |
|     30 | 3201 | ` 	  nLen = ph7_value_to_int(apArg[1]);` |
|     30 | 3202 | `	  if( nLen < 1 ){` |
|      - | 3203 | `		/* Invalid length,set a default length */` |
|    ! 0 | 3204 | `		nLen = 4096;` |
|    ! 0 | 3205 | `	  }` |
|     14 | 3206 | `        }` |
|      - | 3207 | `	/* Allocate enough buffer */` |
|     30 | 3208 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     30 | 3209 | `	if( pBuf == 0 ){` |
|    ! 0 | 3210 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3211 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3212 | `		return PH7_OK;` |
|      - | 3213 | `	}` |
|      - | 3214 | `	/* Perform the requested operation */` |
|     30 | 3215 | `	nRead = StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     30 | 3216 | `	if( nRead < 1 ){` |
|      - | 3217 | `		/* Nothing read,return FALSE */` |
|    ! 0 | 3218 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3219 | `	}else{` |
|      - | 3220 | `		/* Make a copy of the data just read */` |
|     30 | 3221 | `		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);` |
|      - | 3222 | `	}` |
|      - | 3223 | `	/* Release the buffer */` |
|     30 | 3224 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     30 | 3225 | `	return PH7_OK;` |
|     16 | 3226 | `}` |
|      - | 3227 | `/*` |
|      - | 3228 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|      - | 3229 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|      - | 3230 | ` * Gets line from file pointer and parse for CSV fields.` |
|      - | 3231 | ` * Parameters` |
|      - | 3232 | ` * $handle` |
|      - | 3233 | ` *   The file pointer.` |
|      - | 3234 | ` * $length` |
|      - | 3235 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3236 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3237 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3238 | ` *  the end of the line.` |
|      - | 3239 | ` * $delimiter` |
|      - | 3240 | ` *   Set the field delimiter (one character only).` |
|      - | 3241 | ` * $enclosure` |
|      - | 3242 | ` *   Set the field enclosure character (one character only).` |
|      - | 3243 | ` * $escape` |
|      - | 3244 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 3245 | ` * Return` |
|      - | 3246 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3247 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3248 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3249 | ` */` |
|      2 | 3250 | `static int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3251 | `{` |
|      - | 3252 | `	const ph7_io_stream *pStream;` |
|      - | 3253 | `	const char *zLine;` |
|      - | 3254 | `	io_private *pDev;` |
|      - | 3255 | `	ph7_int64 n,nLen;` |
|      3 | 3256 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3257 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3258 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3259 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3260 | `		return PH7_OK;` |
|      - | 3261 | `	}` |
|      - | 3262 | `	/* Extract our private data */` |
|      3 | 3263 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3264 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3265 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3266 | `		/*Expecting an IO handle */` |
|    ! 0 | 3267 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3268 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3269 | `		return PH7_OK;` |
|      - | 3270 | `	}` |
|      - | 3271 | `	/* Point to the target IO stream device */` |
|      3 | 3272 | `	pStream = pDev->pStream;` |
|      3 | 3273 | `	if( pStream == 0  ){` |
|    ! 0 | 3274 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3275 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3276 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3277 | `			);` |
|    ! 0 | 3278 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3279 | `		return PH7_OK;` |
|      - | 3280 | `	}` |
|      3 | 3281 | `	nLen = -1;` |
|      3 | 3282 | `	if( nArg > 1 ){` |
|      - | 3283 | `		/* Maximum data to read */` |
|      3 | 3284 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      1 | 3285 | `	}` |
|      - | 3286 | `	/* Perform the requested operation */` |
|      3 | 3287 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3288 | `	if( n < 1 ){` |
|      - | 3289 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3290 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3291 | `	}else{` |
|      - | 3292 | `		ph7_value *pArray;` |
|      3 | 3293 | `		int delim  = ',';   /* Delimiter */` |
|      3 | 3294 | `		int encl   = '"' ;  /* Enclosure */` |
|      3 | 3295 | `		int escape = '\\';  /* Escape character */` |
|      3 | 3296 | `		if( nArg > 2 ){` |
|      - | 3297 | `			const char *zPtr;` |
|      - | 3298 | `			int i;` |
|      3 | 3299 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 3300 | `				/* Extract the delimiter */` |
|      3 | 3301 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 3302 | `				if( i > 0 ){` |
|      3 | 3303 | `					delim = zPtr[0];` |
|      1 | 3304 | `				}` |
|      1 | 3305 | `			}` |
|      3 | 3306 | `			if( nArg > 3 ){` |
|      3 | 3307 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 3308 | `					/* Extract the enclosure */` |
|      3 | 3309 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 3310 | `					if( i > 0 ){` |
|      3 | 3311 | `						encl = zPtr[0];` |
|      1 | 3312 | `					}` |
|      1 | 3313 | `				}` |
|      3 | 3314 | `				if( nArg > 4 ){` |
|      3 | 3315 | `					if( ph7_value_is_string(apArg[4]) ){` |
|      - | 3316 | `						/* Extract the escape character */` |
|      3 | 3317 | `						zPtr = ph7_value_to_string(apArg[4],&i);` |
|      3 | 3318 | `						if( i > 0 ){` |
|      3 | 3319 | `							escape = zPtr[0];` |
|      1 | 3320 | `						}` |
|      1 | 3321 | `					}` |
|      1 | 3322 | `				}` |
|      1 | 3323 | `			}` |
|      1 | 3324 | `		}` |
|      - | 3325 | `		/* Create our array */` |
|      3 | 3326 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 3327 | `		if( pArray == 0 ){` |
|    ! 0 | 3328 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3329 | `			ph7_result_null(pCtx);` |
|    ! 0 | 3330 | `			return PH7_OK;` |
|      - | 3331 | `		}` |
|      - | 3332 | `		/* Parse the raw input */` |
|      3 | 3333 | `		PH7_ProcessCsv(zLine,(int)n,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 3334 | `		/* Return the freshly created array  */` |
|      3 | 3335 | `		ph7_result_value(pCtx,pArray);` |
|      - | 3336 | `	}` |
|      3 | 3337 | `	return PH7_OK;` |
|      2 | 3338 | `}` |
|      - | 3339 | `/*` |
|      - | 3340 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|      - | 3341 | ` *  Gets line from file pointer and strip HTML tags.` |
|      - | 3342 | ` * Parameters` |
|      - | 3343 | ` * $handle` |
|      - | 3344 | ` *   The file pointer.` |
|      - | 3345 | ` * $length` |
|      - | 3346 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3347 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3348 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3349 | ` *  the end of the line.` |
|      - | 3350 | ` * $allowable_tags` |
|      - | 3351 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 3352 | ` * Return` |
|      - | 3353 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|      - | 3354 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|      - | 3355 | ` */` |
|      2 | 3356 | `static int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3357 | `{` |
|      - | 3358 | `	const ph7_io_stream *pStream;` |
|      - | 3359 | `	const char *zLine;` |
|      - | 3360 | `	io_private *pDev;` |
|      - | 3361 | `	ph7_int64 n,nLen;` |
|      3 | 3362 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3363 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3364 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3365 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3366 | `		return PH7_OK;` |
|      - | 3367 | `	}` |
|      - | 3368 | `	/* Extract our private data */` |
|      3 | 3369 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3370 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3371 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3372 | `		/*Expecting an IO handle */` |
|    ! 0 | 3373 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3374 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3375 | `		return PH7_OK;` |
|      - | 3376 | `	}` |
|      - | 3377 | `	/* Point to the target IO stream device */` |
|      3 | 3378 | `	pStream = pDev->pStream;` |
|      3 | 3379 | `	if( pStream == 0  ){` |
|    ! 0 | 3380 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3381 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3382 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3383 | `			);` |
|    ! 0 | 3384 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3385 | `		return PH7_OK;` |
|      - | 3386 | `	}` |
|      3 | 3387 | `	nLen = -1;` |
|      3 | 3388 | `	if( nArg > 1 ){` |
|      - | 3389 | `		/* Maximum data to read */` |
|    ! 0 | 3390 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3391 | `	}` |
|      - | 3392 | `	/* Perform the requested operation */` |
|      3 | 3393 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3394 | `	if( n < 1 ){` |
|      - | 3395 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3396 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3397 | `	}else{` |
|      3 | 3398 | `		const char *zTaglist = 0;` |
|      3 | 3399 | `		int nTaglen = 0;` |
|      3 | 3400 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|      - | 3401 | `			/* Allowed tag */` |
|    ! 0 | 3402 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|    ! 0 | 3403 | `		}` |
|      - | 3404 | `		/* Process data just read */` |
|      3 | 3405 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen);` |
|      - | 3406 | `	}` |
|      3 | 3407 | `	return PH7_OK;` |
|      2 | 3408 | `}` |
|      - | 3409 | `/*` |
|      - | 3410 | ` * string readdir(resource $dir_handle)` |
|      - | 3411 | ` *   Read entry from directory handle.` |
|      - | 3412 | ` * Parameter` |
|      - | 3413 | ` *  $dir_handle` |
|      - | 3414 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3415 | ` * Return` |
|      - | 3416 | ` *  Returns the filename on success or FALSE on failure.` |
|      - | 3417 | ` */` |
|   8500 | 3418 | `static int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3419 | `{` |
|      - | 3420 | `	const ph7_io_stream *pStream;` |
|      - | 3421 | `	io_private *pDev;` |
|      - | 3422 | `	int rc;` |
|   8505 | 3423 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3424 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3425 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3426 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3427 | `		return PH7_OK;` |
|      - | 3428 | `	}` |
|      - | 3429 | `	/* Extract our private data */` |
|   8505 | 3430 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3431 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   8505 | 3432 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3433 | `		/*Expecting an IO handle */` |
|    ! 0 | 3434 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3435 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3436 | `		return PH7_OK;` |
|      - | 3437 | `	}` |
|      - | 3438 | `	/* Point to the target IO stream device */` |
|   8505 | 3439 | `	pStream = pDev->pStream;` |
|   8505 | 3440 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 3441 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3442 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3443 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3444 | `			);` |
|    ! 0 | 3445 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3446 | `		return PH7_OK;` |
|      - | 3447 | `	}` |
|   8505 | 3448 | `	ph7_result_bool(pCtx,0);` |
|      - | 3449 | `	/* Perform the requested operation */` |
|   8505 | 3450 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|   8505 | 3451 | `	if( rc != PH7_OK ){` |
|      - | 3452 | `		/* Return FALSE */` |
|    999 | 3453 | `		ph7_result_bool(pCtx,0);` |
|    497 | 3454 | `	}` |
|   8505 | 3455 | `	return PH7_OK;` |
|   4255 | 3456 | `}` |
|      - | 3457 | `/*` |
|      - | 3458 | ` * void rewinddir(resource $dir_handle)` |
|      - | 3459 | ` *   Rewind directory handle.` |
|      - | 3460 | ` * Parameter` |
|      - | 3461 | ` *  $dir_handle` |
|      - | 3462 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3463 | ` * Return` |
|      - | 3464 | ` *  FALSE on failure.` |
|      - | 3465 | ` */` |
|      2 | 3466 | `static int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3467 | `{` |
|      - | 3468 | `	const ph7_io_stream *pStream;` |
|      - | 3469 | `	io_private *pDev;` |
|      3 | 3470 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3471 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3472 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3473 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3474 | `		return PH7_OK;` |
|      - | 3475 | `	}` |
|      - | 3476 | `	/* Extract our private data */` |
|      3 | 3477 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3478 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3479 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3480 | `		/*Expecting an IO handle */` |
|    ! 0 | 3481 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3482 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3483 | `		return PH7_OK;` |
|      - | 3484 | `	}` |
|      - | 3485 | `	/* Point to the target IO stream device */` |
|      3 | 3486 | `	pStream = pDev->pStream;` |
|      3 | 3487 | `	if( pStream == 0  \|\| pStream->xRewindDir == 0 ){` |
|    ! 0 | 3488 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3489 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3490 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3491 | `			);` |
|    ! 0 | 3492 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3493 | `		return PH7_OK;` |
|      - | 3494 | `	}` |
|      - | 3495 | `	/* Perform the requested operation */` |
|      3 | 3496 | `	pStream->xRewindDir(pDev->pHandle);` |
|      3 | 3497 | `	return PH7_OK;` |
|      2 | 3498 | ` }` |
|      - | 3499 | `/* Forward declaration */` |
|      - | 3500 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut);` |
|      - | 3501 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|      - | 3502 | `/*` |
|      - | 3503 | ` * void closedir(resource $dir_handle)` |
|      - | 3504 | ` *   Close directory handle.` |
|      - | 3505 | ` * Parameter` |
|      - | 3506 | ` *  $dir_handle` |
|      - | 3507 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3508 | ` * Return` |
|      - | 3509 | ` *  FALSE on failure.` |
|      - | 3510 | ` */` |
|    998 | 3511 | `static int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3512 | `{` |
|      - | 3513 | `	const ph7_io_stream *pStream;` |
|      - | 3514 | `	io_private *pDev;` |
|   1003 | 3515 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3516 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3517 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3518 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3519 | `		return PH7_OK;` |
|      - | 3520 | `	}` |
|      - | 3521 | `	/* Extract our private data */` |
|   1003 | 3522 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3523 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1003 | 3524 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3525 | `		/*Expecting an IO handle */` |
|    ! 0 | 3526 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3527 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3528 | `		return PH7_OK;` |
|      - | 3529 | `	}` |
|      - | 3530 | `	/* Point to the target IO stream device */` |
|   1003 | 3531 | `	pStream = pDev->pStream;` |
|   1003 | 3532 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 3533 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3534 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3535 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3536 | `			);` |
|    ! 0 | 3537 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3538 | `		return PH7_OK;` |
|      - | 3539 | `	}` |
|      - | 3540 | `	/* Perform the requested operation */` |
|   1003 | 3541 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 3542 | `	/* Release the private stucture */` |
|   1003 | 3543 | `	ReleaseIOPrivate(pCtx,pDev);` |
|   1003 | 3544 | `	PH7_MemObjRelease(apArg[0]);` |
|   1003 | 3545 | `	return PH7_OK;` |
|    504 | 3546 | ` }` |
|      - | 3547 | `/*` |
|      - | 3548 | ` * resource opendir(string $path[,resource $context])` |
|      - | 3549 | ` *  Open directory handle.` |
|      - | 3550 | ` * Parameters` |
|      - | 3551 | ` * $path` |
|      - | 3552 | ` *   The directory path that is to be opened.` |
|      - | 3553 | ` * $context` |
|      - | 3554 | ` *   A context stream resource.` |
|      - | 3555 | ` * Return` |
|      - | 3556 | ` *  A directory handle resource on success,or FALSE on failure.` |
|      - | 3557 | ` */` |
|    998 | 3558 | `static int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3559 | `{` |
|      - | 3560 | `	const ph7_io_stream *pStream;` |
|      - | 3561 | `	const char *zPath;` |
|      - | 3562 | `	io_private *pDev;` |
|      - | 3563 | `	int iLen,rc;` |
|   1003 | 3564 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3565 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3566 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 3567 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3568 | `		return PH7_OK;` |
|      - | 3569 | `	}` |
|      - | 3570 | `	/* Extract the target path */` |
|   1003 | 3571 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 3572 | `	/* Try to extract a stream */` |
|   1003 | 3573 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|   1003 | 3574 | `	if( pStream == 0 ){` |
|    ! 0 | 3575 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3576 | `			"No stream device is associated with the given path(%s)",zPath);` |
|    ! 0 | 3577 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3578 | `		return PH7_OK;` |
|      - | 3579 | `	}` |
|   1003 | 3580 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 3581 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3582 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 3583 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 3584 | `			);` |
|    ! 0 | 3585 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3586 | `		return PH7_OK;` |
|      - | 3587 | `	}` |
|      - | 3588 | `	/* Allocate a new IO private instance */` |
|   1003 | 3589 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|   1003 | 3590 | `	if( pDev == 0 ){` |
|    ! 0 | 3591 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3592 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3593 | `		return PH7_OK;` |
|      - | 3594 | `	}` |
|      - | 3595 | `	/* Initialize the structure */` |
|   1003 | 3596 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 3597 | `	/* Open the target directory */` |
|   1003 | 3598 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|   1003 | 3599 | `	if( rc != PH7_OK ){` |
|      - | 3600 | `		/* IO error,return FALSE */` |
|    ! 0 | 3601 | `		ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 3602 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3603 | `	}else{` |
|      - | 3604 | `		/* Return the handle as a resource */` |
|   1003 | 3605 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 3606 | `	}` |
|   1003 | 3607 | `	return PH7_OK;` |
|    504 | 3608 | `}` |
|      - | 3609 | `/*` |
|      - | 3610 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|      - | 3611 | ` *  Reads a file and writes it to the output buffer.` |
|      - | 3612 | ` * Parameters` |
|      - | 3613 | ` *  $filename` |
|      - | 3614 | ` *   The filename being read.` |
|      - | 3615 | ` *  $use_include_path` |
|      - | 3616 | ` *   You can use the optional second parameter and set it to` |
|      - | 3617 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3618 | ` *  $context` |
|      - | 3619 | ` *   A context stream resource.` |
|      - | 3620 | ` * Return` |
|      - | 3621 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|      - | 3622 | ` */` |
|      2 | 3623 | `static int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3624 | `{` |
|      3 | 3625 | `	int use_include  = FALSE;` |
|      - | 3626 | `	const ph7_io_stream *pStream;` |
|      - | 3627 | `	ph7_int64 n,nRead;` |
|      - | 3628 | `	const char *zFile;` |
|      - | 3629 | `	char zBuf[8192];` |
|      - | 3630 | `	void *pHandle;` |
|      - | 3631 | `	int rc,nLen;` |
|      3 | 3632 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3633 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3634 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3635 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3636 | `		return PH7_OK;` |
|      - | 3637 | `	}` |
|      - | 3638 | `	/* Extract the file path */` |
|      3 | 3639 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3640 | `	/* Point to the target IO stream device */` |
|      3 | 3641 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3642 | `	if( pStream == 0 ){` |
|    ! 0 | 3643 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3644 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3645 | `		return PH7_OK;` |
|      - | 3646 | `	}` |
|      3 | 3647 | `	if( nArg > 1 ){` |
|    ! 0 | 3648 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3649 | `	}` |
|      - | 3650 | `	/* Try to open the file in read-only mode */` |
|      4 | 3651 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|      1 | 3652 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 3653 | `	if( pHandle == 0 ){` |
|    ! 0 | 3654 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3655 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3656 | `		return PH7_OK;` |
|      - | 3657 | `	}` |
|      - | 3658 | `	/* Perform the requested operation */` |
|      3 | 3659 | `	nRead = 0;` |
|      2 | 3660 | `	for(;;){` |
|      5 | 3661 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3662 | `		if( n < 1 ){` |
|      - | 3663 | `			/* EOF or IO error,break immediately */` |
|      3 | 3664 | `			break;` |
|      - | 3665 | `		}` |
|      - | 3666 | `		/* Output data */` |
|      3 | 3667 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      3 | 3668 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 3669 | `			break;` |
|      - | 3670 | `		}` |
|      - | 3671 | `		/* Increment counter */` |
|      3 | 3672 | `		nRead += n;` |
|      1 | 3673 | `	}` |
|      - | 3674 | `	/* Close the stream */` |
|      3 | 3675 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3676 | `	/* Total number of bytes readen */` |
|      3 | 3677 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 3678 | `	return PH7_OK;` |
|      2 | 3679 | `}` |
|      - | 3680 | `/*` |
|      - | 3681 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|      - | 3682 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|      - | 3683 | ` *  Reads entire file into a string.` |
|      - | 3684 | ` * Parameters` |
|      - | 3685 | ` *  $filename` |
|      - | 3686 | ` *   The filename being read.` |
|      - | 3687 | ` *  $use_include_path` |
|      - | 3688 | ` *   You can use the optional second parameter and set it to` |
|      - | 3689 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3690 | ` *  $context` |
|      - | 3691 | ` *   A context stream resource.` |
|      - | 3692 | ` *  $offset` |
|      - | 3693 | ` *   The offset where the reading starts on the original stream.` |
|      - | 3694 | ` *  $maxlen` |
|      - | 3695 | ` *    Maximum length of data read. The default is to read until end of file` |
|      - | 3696 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|      - | 3697 | ` * Return` |
|      - | 3698 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3699 | ` */` |
|   6556 | 3700 | `static int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3701 | `{` |
|      - | 3702 | `	const ph7_io_stream *pStream;` |
|      - | 3703 | `	ph7_int64 n,nRead,nMaxlen;` |
|   6561 | 3704 | `	int use_include  = FALSE;` |
|      - | 3705 | `	const char *zFile;` |
|      - | 3706 | `	char zBuf[8192];` |
|      - | 3707 | `	void *pHandle;` |
|      - | 3708 | `	int nLen;` |
|      - | 3709 |  |
|   6561 | 3710 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3711 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3712 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3713 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3714 | `		return PH7_OK;` |
|      - | 3715 | `	}` |
|      - | 3716 | `	/* Extract the file path */` |
|   6561 | 3717 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3718 | `	/* Point to the target IO stream device */` |
|   6561 | 3719 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   6561 | 3720 | `	if( pStream == 0 ){` |
|    ! 0 | 3721 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3722 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3723 | `		return PH7_OK;` |
|      - | 3724 | `	}` |
|   6561 | 3725 | `	nMaxlen = -1;` |
|   6561 | 3726 | `	if( nArg > 1 ){` |
|      5 | 3727 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|      2 | 3728 | `	}` |
|      - | 3729 | `	/* Try to open the file in read-only mode */` |
|   6561 | 3730 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|   6561 | 3731 | `	if( pHandle == 0 ){` |
|    ! 0 | 3732 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3733 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3734 | `		return PH7_OK;` |
|      - | 3735 | `	}` |
|   6561 | 3736 | `	if( nArg > 3 ){` |
|      - | 3737 | `		/* Extract the offset */` |
|      5 | 3738 | `		n = ph7_value_to_int64(apArg[3]);` |
|      5 | 3739 | `		if( n > 0 ){` |
|    ! 0 | 3740 | `			if( pStream->xSeek ){` |
|      - | 3741 | `				/* Seek to the desired offset */` |
|    ! 0 | 3742 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|    ! 0 | 3743 | `			}` |
|    ! 0 | 3744 | `		}` |
|      5 | 3745 | `		if( nArg > 4 ){` |
|      - | 3746 | `			/* Maximum data to read */` |
|      5 | 3747 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|      2 | 3748 | `		}` |
|      2 | 3749 | `	}` |
|      - | 3750 | `	/* Perform the requested operation */` |
|   6561 | 3751 | `	nRead = 0;` |
|   6554 | 3752 | `	for(;;){` |
|  19670 | 3753 | `		n = pStream->xRead(pHandle,zBuf,` |
|   6557 | 3754 | `			(nMaxlen > 0 && (nMaxlen < (ph7_int64)sizeof(zBuf))) ? nMaxlen : (ph7_int64)sizeof(zBuf));` |
|  13113 | 3755 | `		if( n < 1 ){` |
|      - | 3756 | `			/* EOF or IO error,break immediately */` |
|   6559 | 3757 | `			break;` |
|      - | 3758 | `		}` |
|      - | 3759 | `		/* Append data */` |
|   6559 | 3760 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 3761 | `		/* Increment read counter */` |
|   6559 | 3762 | `		nRead += n;` |
|   6559 | 3763 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 3764 | `			/* Read limit reached */` |
|      3 | 3765 | `			break;` |
|      - | 3766 | `		}` |
|      5 | 3767 | `	}` |
|      - | 3768 | `	/* Close the stream */` |
|   6561 | 3769 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3770 | `	/* Check if we have read something */` |
|   6561 | 3771 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 3772 | `		/* Nothing read,return FALSE */` |
|      3 | 3773 | `		ph7_result_bool(pCtx,0);` |
|      1 | 3774 | `	}` |
|   6561 | 3775 | `	return PH7_OK;` |
|   3283 | 3776 | `}` |
|      - | 3777 | `/*` |
|      - | 3778 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 3779 | ` *  Write a string to a file.` |
|      - | 3780 | ` * Parameters` |
|      - | 3781 | ` *  $filename` |
|      - | 3782 | ` *  Path to the file where to write the data.` |
|      - | 3783 | ` * $data` |
|      - | 3784 | ` *  The data to write(Must be a string).` |
|      - | 3785 | ` * $flags` |
|      - | 3786 | ` *  The value of flags can be any combination of the following` |
|      - | 3787 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 3788 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 3789 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 3790 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 3791 | ` * context` |
|      - | 3792 | ` *  A context stream resource.` |
|      - | 3793 | ` * Return` |
|      - | 3794 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 3795 | ` */` |
|  13688 | 3796 | `static int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3797 | `{` |
|  13693 | 3798 | `	int use_include  = FALSE;` |
|      - | 3799 | `	const ph7_io_stream *pStream;` |
|      - | 3800 | `	const char *zFile;` |
|      - | 3801 | `	const char *zData;` |
|      - | 3802 | `	int iOpenFlags;` |
|      - | 3803 | `	void *pHandle;` |
|      - | 3804 | `	int iFlags;` |
|      - | 3805 | `	int nLen;` |
|      - | 3806 |  |
|  13693 | 3807 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3808 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3809 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3810 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3811 | `		return PH7_OK;` |
|      - | 3812 | `	}` |
|      - | 3813 | `	/* Extract the file path */` |
|  13693 | 3814 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3815 | `	/* Point to the target IO stream device */` |
|  13693 | 3816 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  13693 | 3817 | `	if( pStream == 0 ){` |
|    ! 0 | 3818 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3819 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3820 | `		return PH7_OK;` |
|      - | 3821 | `	}` |
|      - | 3822 | `	/* Data to write */` |
|  13693 | 3823 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 3824 | `	/* Try to open the file in read-write mode */` |
|  13693 | 3825 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 3826 | `	/* Extract the flags */` |
|  13693 | 3827 | `	iFlags = 0;` |
|  13693 | 3828 | `	if( nArg > 2 ){` |
|    ! 0 | 3829 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3830 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 3831 | `			use_include = TRUE;` |
|    ! 0 | 3832 | `		}` |
|    ! 0 | 3833 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 3834 | `			/* If the file already exists, append the data to the file` |
|      - | 3835 | `			 * instead of overwriting it.` |
|      - | 3836 | `			 */` |
|    ! 0 | 3837 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 3838 | `			/* Append mode */` |
|    ! 0 | 3839 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 3840 | `		}` |
|    ! 0 | 3841 | `	}` |
|  20537 | 3842 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   6844 | 3843 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|  13693 | 3844 | `	if( pHandle == 0 ){` |
|    ! 0 | 3845 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3846 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3847 | `		return PH7_OK;` |
|      - | 3848 | `	}` |
|  13693 | 3849 | `	if( nLen < 1 ){` |
|      - | 3850 | `		/* Empty data, file is created/truncated */` |
|      7 | 3851 | `		ph7_result_int64(pCtx,0);` |
|      7 | 3852 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|      7 | 3853 | `		return PH7_OK;` |
|      - | 3854 | `	}` |
|  13687 | 3855 | `	if( pStream->xWrite ){` |
|      - | 3856 | `		ph7_int64 n;` |
|  13687 | 3857 | `		if( (iFlags & 0x01/* LOCK_EX */) && pStream->xLock ){` |
|      - | 3858 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 3859 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 3860 | `		}` |
|      - | 3861 | `		/* Perform the write operation */` |
|  13687 | 3862 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  13687 | 3863 | `		if( n < 0 ){` |
|      - | 3864 | `			/* IO error,return FALSE */` |
|    ! 0 | 3865 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3866 | `		}else{` |
|      - | 3867 | `			/* Total number of bytes written */` |
|  13687 | 3868 | `			ph7_result_int64(pCtx,n);` |
|      - | 3869 | `		}` |
|   6846 | 3870 | `	}else{` |
|      - | 3871 | `		/* Read-only stream */` |
|    ! 0 | 3872 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 3873 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 3874 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 3875 | `			);` |
|    ! 0 | 3876 | `		ph7_result_bool(pCtx,0);` |
|      - | 3877 | `	}` |
|      - | 3878 | `	/* Close the handle */` |
|  13687 | 3879 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  13687 | 3880 | `	return PH7_OK;` |
|   6849 | 3881 | `}` |
|      - | 3882 | `/*` |
|      - | 3883 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 3884 | ` *  Reads entire file into an array.` |
|      - | 3885 | ` * Parameters` |
|      - | 3886 | ` *  $filename` |
|      - | 3887 | ` *   The filename being read.` |
|      - | 3888 | ` *  $flags` |
|      - | 3889 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 3890 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 3891 | ` *       Search for the file in the include_path.` |
|      - | 3892 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 3893 | ` *       Do not add newline at the end of each array element` |
|      - | 3894 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 3895 | ` *       Skip empty lines` |
|      - | 3896 | ` *  $context` |
|      - | 3897 | ` *   A context stream resource.` |
|      - | 3898 | ` * Return` |
|      - | 3899 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3900 | ` */` |
|      6 | 3901 | `static int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3902 | `{` |
|      - | 3903 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 3904 | `	ph7_value *pArray,*pLine;` |
|      - | 3905 | `	const ph7_io_stream *pStream;` |
|      8 | 3906 | `	int use_include = 0;` |
|      - | 3907 | `	io_private *pDev;` |
|      - | 3908 | `	ph7_int64 n;` |
|      - | 3909 | `	int iFlags;` |
|      - | 3910 | `	int nLen;` |
|      - | 3911 |  |
|      8 | 3912 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3913 | `		/* Missing/Invalid arguments,return FALSE */` |
|      3 | 3914 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|      3 | 3915 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3916 | `		return PH7_OK;` |
|      - | 3917 | `	}` |
|      - | 3918 | `	/* Extract the file path */` |
|      6 | 3919 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3920 | `	/* Point to the target IO stream device */` |
|      6 | 3921 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 3922 | `	if( pStream == 0 ){` |
|    ! 0 | 3923 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3924 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3925 | `		return PH7_OK;` |
|      - | 3926 | `	}` |
|      - | 3927 | `	/* Allocate a new IO private instance */` |
|      6 | 3928 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      6 | 3929 | `	if( pDev == 0 ){` |
|    ! 0 | 3930 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3931 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3932 | `		return PH7_OK;` |
|      - | 3933 | `	}` |
|      - | 3934 | `	/* Initialize the structure */` |
|      6 | 3935 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      6 | 3936 | `	iFlags = 0;` |
|      6 | 3937 | `	if( nArg > 1 ){` |
|    ! 0 | 3938 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 3939 | `	}` |
|      6 | 3940 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|    ! 0 | 3941 | `		use_include = TRUE;` |
|    ! 0 | 3942 | `	}` |
|      - | 3943 | `	/* Create the array and the working value */` |
|      6 | 3944 | `	pArray = ph7_context_new_array(pCtx);` |
|      6 | 3945 | `	pLine = ph7_context_new_scalar(pCtx);` |
|      6 | 3946 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 3947 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3948 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3949 | `		return PH7_OK;` |
|      - | 3950 | `	}` |
|      - | 3951 | `	/* Try to open the file in read-only mode */` |
|      6 | 3952 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 3953 | `	if( pDev->pHandle == 0 ){` |
|      3 | 3954 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|      3 | 3955 | `		ph7_result_bool(pCtx,0);` |
|      - | 3956 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 3957 | `		 * as soon we return from this function.` |
|      - | 3958 | `		 */` |
|      3 | 3959 | `		return PH7_OK;` |
|      - | 3960 | `	}` |
|      - | 3961 | `	/* Perform the requested operation */` |
|      3 | 3962 | `	for(;;){` |
|      - | 3963 | `		/* Try to extract a line */` |
|      7 | 3964 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|      7 | 3965 | `		if( n < 1 ){` |
|      - | 3966 | `			/* EOF or IO error */` |
|      3 | 3967 | `			break;` |
|      - | 3968 | `		}` |
|      - | 3969 | `		/* Reset the cursor */` |
|      5 | 3970 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 3971 | `		/* Remove line ending if requested by the caller */` |
|      5 | 3972 | `		zPtr = zBuf;` |
|      5 | 3973 | `		zEnd = &zBuf[n];` |
|      5 | 3974 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 3975 | `			/* Ignore trailig lines */` |
|    ! 0 | 3976 | `			while( zPtr < zEnd && (zEnd[-1] == '\n'` |
|      - | 3977 | `#ifdef __WINNT__` |
|      - | 3978 | `				\|\| zEnd[-1] == '\r'` |
|      - | 3979 | `#endif` |
|      - | 3980 | `				)){` |
|    ! 0 | 3981 | `					n--;` |
|    ! 0 | 3982 | `					zEnd--;` |
|    ! 0 | 3983 | `			}` |
|    ! 0 | 3984 | `		}` |
|      5 | 3985 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 3986 | `			/* Ignore empty lines */` |
|    ! 0 | 3987 | `			while( zPtr < zEnd && (unsigned char)zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) ){` |
|    ! 0 | 3988 | `				zPtr++;` |
|    ! 0 | 3989 | `			}` |
|    ! 0 | 3990 | `			if( zPtr >= zEnd ){` |
|      - | 3991 | `				/* Empty line */` |
|    ! 0 | 3992 | `				continue;` |
|      - | 3993 | `			}` |
|    ! 0 | 3994 | `		}` |
|      5 | 3995 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 3996 | `		/* Insert line */` |
|      5 | 3997 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 3998 | `	}` |
|      - | 3999 | `	/* Close the stream */` |
|      3 | 4000 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 4001 | `	/* Release the io_private instance */` |
|      3 | 4002 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 4003 | `	/* Return the created array */` |
|      3 | 4004 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4005 | `	return PH7_OK;` |
|      5 | 4006 | `}` |
|      - | 4007 | `/*` |
|      - | 4008 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 4009 | ` *  Makes a copy of the file source to dest.` |
|      - | 4010 | ` * Parameters` |
|      - | 4011 | ` *  $source` |
|      - | 4012 | ` *   Path to the source file.` |
|      - | 4013 | ` *  $dest` |
|      - | 4014 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 4015 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 4016 | ` *  $context` |
|      - | 4017 | ` *   A context stream resource.` |
|      - | 4018 | ` * Return` |
|      - | 4019 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4020 | ` */` |
|     10 | 4021 | `static int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4022 | `{` |
|      - | 4023 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 4024 | `	const char *zFile;` |
|      - | 4025 | `	char zBuf[8192];` |
|      - | 4026 | `	void *pIn,*pOut;` |
|      - | 4027 | `	ph7_int64 n;` |
|      - | 4028 | `	int nLen;` |
|     12 | 4029 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 4030 | `		/* Missing/Invalid arguments,return FALSE */` |
|      7 | 4031 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|      7 | 4032 | `		ph7_result_bool(pCtx,0);` |
|      7 | 4033 | `		return PH7_OK;` |
|      - | 4034 | `	}` |
|      - | 4035 | `	/* Extract the source name */` |
|      6 | 4036 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4037 | `	/* Point to the target IO stream device */` |
|      6 | 4038 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 4039 | `	if( pSin == 0 ){` |
|    ! 0 | 4040 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4041 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4042 | `		return PH7_OK;` |
|      - | 4043 | `	}` |
|      - | 4044 | `	/* Try to open the source file in a read-only mode */` |
|      6 | 4045 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 4046 | `	if( pIn == 0 ){` |
|      3 | 4047 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening source: '%s'",zFile);` |
|      3 | 4048 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4049 | `		return PH7_OK;` |
|      - | 4050 | `	}` |
|      - | 4051 | `	/* Extract the destination name */` |
|      3 | 4052 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 4053 | `	/* Point to the target IO stream device */` |
|      3 | 4054 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4055 | `	if( pSout == 0 ){` |
|    ! 0 | 4056 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4057 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4058 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4059 | `		return PH7_OK;` |
|      - | 4060 | `	}` |
|      3 | 4061 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 4062 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4063 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4064 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 4065 | `			);` |
|    ! 0 | 4066 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4067 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4068 | `		return PH7_OK;` |
|      - | 4069 | `	}` |
|      - | 4070 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 4071 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 4072 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 4073 | `	if( pOut == 0 ){` |
|    ! 0 | 4074 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening destination: '%s'",zFile);` |
|    ! 0 | 4075 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4076 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4077 | `		return PH7_OK;` |
|      - | 4078 | `	}` |
|      - | 4079 | `	/* Perform the requested operation */` |
|      2 | 4080 | `	for(;;){` |
|      - | 4081 | `		/* Read from source */` |
|      5 | 4082 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 4083 | `		if( n < 1 ){` |
|      - | 4084 | `			/* EOF or IO error,break immediately */` |
|      3 | 4085 | `			break;` |
|      - | 4086 | `		}` |
|      - | 4087 | `		/* Write to dest */` |
|      3 | 4088 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 4089 | `		if( n < 1 ){` |
|      - | 4090 | `			/* IO error,break immediately */` |
|    ! 0 | 4091 | `			break;` |
|      - | 4092 | `		}` |
|      1 | 4093 | `	}` |
|      - | 4094 | `	/* Close the streams */` |
|      3 | 4095 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 4096 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 4097 | `	/* Return TRUE */` |
|      3 | 4098 | `	ph7_result_bool(pCtx,1);` |
|      3 | 4099 | `	return PH7_OK;` |
|      7 | 4100 | `}` |
|      - | 4101 | `/*` |
|      - | 4102 | ` * array fstat(resource $handle)` |
|      - | 4103 | ` *  Gets information about a file using an open file pointer.` |
|      - | 4104 | ` * Parameters` |
|      - | 4105 | ` *  $handle` |
|      - | 4106 | ` *   The file pointer.` |
|      - | 4107 | ` * Return` |
|      - | 4108 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 4109 | ` */` |
|      2 | 4110 | `static int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4111 | `{` |
|      - | 4112 | `	ph7_value *pArray,*pValue;` |
|      - | 4113 | `	const ph7_io_stream *pStream;` |
|      - | 4114 | `	io_private *pDev;` |
|      3 | 4115 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4116 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4117 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4118 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4119 | `		return PH7_OK;` |
|      - | 4120 | `	}` |
|      - | 4121 | `	/* Extract our private data */` |
|      3 | 4122 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4123 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4124 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4125 | `		/* Expecting an IO handle */` |
|    ! 0 | 4126 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4127 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4128 | `		return PH7_OK;` |
|      - | 4129 | `	}` |
|      - | 4130 | `	/* Point to the target IO stream device */` |
|      3 | 4131 | `	pStream = pDev->pStream;` |
|      3 | 4132 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 4133 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4134 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4135 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4136 | `			);` |
|    ! 0 | 4137 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4138 | `		return PH7_OK;` |
|      - | 4139 | `	}` |
|      - | 4140 | `	/* Create the array and the working value */` |
|      3 | 4141 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4142 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4143 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4144 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4145 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4146 | `		return PH7_OK;` |
|      - | 4147 | `	}` |
|      - | 4148 | `	/* Perform the requested operation */` |
|      3 | 4149 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 4150 | `	/* Return the freshly created array */` |
|      3 | 4151 | `	ph7_result_value(pCtx,pArray);` |
|      - | 4152 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 4153 | `	 * released automatically as soon we return from this function.` |
|      - | 4154 | `	 */` |
|      3 | 4155 | `	return PH7_OK;` |
|      2 | 4156 | `}` |
|      - | 4157 | `/*` |
|      - | 4158 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 4159 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 4160 | ` * Parameters` |
|      - | 4161 | ` *  $handle` |
|      - | 4162 | ` *   The file pointer.` |
|      - | 4163 | ` *  $string` |
|      - | 4164 | ` *   The string that is to be written.` |
|      - | 4165 | ` *  $length` |
|      - | 4166 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 4167 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 4168 | ` * Return` |
|      - | 4169 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 4170 | ` */` |
|     22 | 4171 | `static int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4172 | `{` |
|      - | 4173 | `	const ph7_io_stream *pStream;` |
|      - | 4174 | `	const char *zString;` |
|      - | 4175 | `	io_private *pDev;` |
|      - | 4176 | `	int nLen,n;` |
|     24 | 4177 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4178 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4179 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4180 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4181 | `		return PH7_OK;` |
|      - | 4182 | `	}` |
|      - | 4183 | `	/* Extract our private data */` |
|     24 | 4184 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4185 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     24 | 4186 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4187 | `		/* Expecting an IO handle */` |
|    ! 0 | 4188 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4189 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4190 | `		return PH7_OK;` |
|      - | 4191 | `	}` |
|      - | 4192 | `	/* Point to the target IO stream device */` |
|     24 | 4193 | `	pStream = pDev->pStream;` |
|     24 | 4194 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4195 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4196 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4197 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4198 | `			);` |
|    ! 0 | 4199 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4200 | `		return PH7_OK;` |
|      - | 4201 | `	}` |
|      - | 4202 | `	/* Extract the data to write */` |
|     24 | 4203 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|     24 | 4204 | `	if( nArg > 2 ){` |
|      - | 4205 | `		/* Maximum data length to write */` |
|    ! 0 | 4206 | `		n = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 4207 | `		if( n >= 0 && n < nLen ){` |
|    ! 0 | 4208 | `			nLen = n;` |
|    ! 0 | 4209 | `		}` |
|    ! 0 | 4210 | `	}` |
|     24 | 4211 | `	if( nLen < 1 ){` |
|      - | 4212 | `		/* Nothing to write */` |
|    ! 0 | 4213 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4214 | `		return PH7_OK;` |
|      - | 4215 | `	}` |
|      - | 4216 | `	/* Perform the requested operation */` |
|     24 | 4217 | `	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);` |
|     24 | 4218 | `	if( n <  0 ){` |
|      - | 4219 | `		/* IO error,return FALSE */` |
|    ! 0 | 4220 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4221 | `	}else{` |
|      - | 4222 | `		/* #Bytes written */` |
|     24 | 4223 | `		ph7_result_int(pCtx,n);` |
|      - | 4224 | `	}` |
|     24 | 4225 | `	return PH7_OK;` |
|     13 | 4226 | `}` |
|      - | 4227 | `/*` |
|      - | 4228 | ` * bool flock(resource $handle,int $operation)` |
|      - | 4229 | ` *  Portable advisory file locking.` |
|      - | 4230 | ` * Parameters` |
|      - | 4231 | ` *  $handle` |
|      - | 4232 | ` *   The file pointer.` |
|      - | 4233 | ` *  $operation` |
|      - | 4234 | ` *   operation is one of the following:` |
|      - | 4235 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 4236 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 4237 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 4238 | ` * Return` |
|      - | 4239 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 4240 | ` */` |
|      4 | 4241 | `static int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 4242 | `{` |
|      - | 4243 | `	const ph7_io_stream *pStream;` |
|      - | 4244 | `	io_private *pDev;` |
|      - | 4245 | `	int nLock;` |
|      - | 4246 | `	int rc;` |
|      4 | 4247 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4248 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4249 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4250 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4251 | `		return PH7_OK;` |
|      - | 4252 | `	}` |
|      - | 4253 | `	/* Extract our private data */` |
|      4 | 4254 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4255 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      4 | 4256 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4257 | `		/*Expecting an IO handle */` |
|    ! 0 | 4258 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4259 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4260 | `		return PH7_OK;` |
|      - | 4261 | `	}` |
|      - | 4262 | `	/* Point to the target IO stream device */` |
|      4 | 4263 | `	pStream = pDev->pStream;` |
|      4 | 4264 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|    ! 0 | 4265 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4266 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4267 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4268 | `			);` |
|    ! 0 | 4269 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4270 | `		return PH7_OK;` |
|      - | 4271 | `	}` |
|      - | 4272 | `	/* Requested lock operation */` |
|      4 | 4273 | `	nLock = ph7_value_to_int(apArg[1]);` |
|      - | 4274 | `	/* Lock operation */` |
|      4 | 4275 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 4276 | `	/* IO result */` |
|      4 | 4277 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 | 4278 | `	return PH7_OK;` |
|      2 | 4279 | `}` |
|      - | 4280 | `/*` |
|      - | 4281 | ` * int fpassthru(resource $handle)` |
|      - | 4282 | ` *  Output all remaining data on a file pointer.` |
|      - | 4283 | ` * Parameters` |
|      - | 4284 | ` *  $handle` |
|      - | 4285 | ` *   The file pointer.` |
|      - | 4286 | ` * Return` |
|      - | 4287 | ` *  Total number of characters read from handle and passed through` |
|      - | 4288 | ` *  to the output on success or FALSE on failure.` |
|      - | 4289 | ` */` |
|      2 | 4290 | `static int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4291 | `{` |
|      - | 4292 | `	const ph7_io_stream *pStream;` |
|      - | 4293 | `	io_private *pDev;` |
|      - | 4294 | `	ph7_int64 n,nRead;` |
|      - | 4295 | `	char zBuf[8192];` |
|      - | 4296 | `	int rc;` |
|      3 | 4297 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4298 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4299 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4300 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4301 | `		return PH7_OK;` |
|      - | 4302 | `	}` |
|      - | 4303 | `	/* Extract our private data */` |
|      3 | 4304 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4305 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4306 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4307 | `		/*Expecting an IO handle */` |
|    ! 0 | 4308 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4309 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4310 | `		return PH7_OK;` |
|      - | 4311 | `	}` |
|      - | 4312 | `	/* Point to the target IO stream device */` |
|      3 | 4313 | `	pStream = pDev->pStream;` |
|      3 | 4314 | `	if( pStream == 0  ){` |
|    ! 0 | 4315 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4316 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4317 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4318 | `			);` |
|    ! 0 | 4319 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4320 | `		return PH7_OK;` |
|      - | 4321 | `	}` |
|      - | 4322 | `	/* Perform the requested operation */` |
|      3 | 4323 | `	nRead = 0;` |
|      2 | 4324 | `	for(;;){` |
|      5 | 4325 | `		n = StreamRead(pDev,zBuf,sizeof(zBuf));` |
|      5 | 4326 | `		if( n < 1 ){` |
|      - | 4327 | `			/* Error or EOF */` |
|      3 | 4328 | `			break;` |
|      - | 4329 | `		}` |
|      - | 4330 | `		/* Increment the read counter */` |
|      3 | 4331 | `		nRead += n;` |
|      - | 4332 | `		/* Output data */` |
|      3 | 4333 | `		rc = ph7_context_output(pCtx,zBuf,(int)nRead /* FIXME: 64-bit issues */);` |
|      3 | 4334 | `		if( rc == PH7_ABORT ){` |
|      - | 4335 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 4336 | `			break;` |
|      - | 4337 | `		}` |
|      1 | 4338 | `	}` |
|      - | 4339 | `	/* Total number of bytes readen */` |
|      3 | 4340 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 4341 | `	return PH7_OK;` |
|      2 | 4342 | `}` |
|      - | 4343 | `/* CSV reader/writer private data */` |
|      - | 4344 | `struct csv_data` |
|      - | 4345 | `{` |
|      - | 4346 | `	int delimiter;    /* Delimiter. Default ',' */` |
|      - | 4347 | `	int enclosure;    /* Enclosure. Default '"'*/` |
|      - | 4348 | `	io_private *pDev; /* Open stream handle */` |
|      - | 4349 | `	int iCount;       /* Counter */` |
|      - | 4350 | `};` |
|      - | 4351 | `/*` |
|      - | 4352 | ` * The following callback is used by the fputcsv() function inorder to iterate` |
|      - | 4353 | ` * throw array entries and output CSV data based on the current key and it's` |
|      - | 4354 | ` * associated data.` |
|      - | 4355 | ` */` |
|      6 | 4356 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 4357 | `{` |
|      7 | 4358 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 4359 | `	const char *zData;` |
|      - | 4360 | `	int nLen,c2;` |
|      - | 4361 | `	sxu32 n;` |
|      - | 4362 | `	/* Point to the raw data */` |
|      7 | 4363 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      7 | 4364 | `	if( nLen < 1 ){` |
|      - | 4365 | `		/* Nothing to write */` |
|    ! 0 | 4366 | `		return PH7_OK;` |
|      - | 4367 | `	}` |
|      7 | 4368 | `	if( pData->iCount > 0 ){` |
|      - | 4369 | `		/* Write the delimiter */` |
|      5 | 4370 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->delimiter,sizeof(char));` |
|      2 | 4371 | `	}` |
|      7 | 4372 | `	n = 1;` |
|      7 | 4373 | `	c2 = 0;` |
|     10 | 4374 | `	if( SyByteFind(zData,(sxu32)nLen,pData->delimiter,0) == SXRET_OK \|\|` |
|      6 | 4375 | `		SyByteFind(zData,(sxu32)nLen,pData->enclosure,&n) == SXRET_OK ){` |
|    ! 0 | 4376 | `			c2 = 1;` |
|    ! 0 | 4377 | `			if( n == 0 ){` |
|    ! 0 | 4378 | `				c2 = 2;` |
|    ! 0 | 4379 | `			}` |
|      - | 4380 | `			/* Write the enclosure */` |
|    ! 0 | 4381 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4382 | `			if( c2 > 1 ){` |
|    ! 0 | 4383 | `				pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4384 | `			}` |
|    ! 0 | 4385 | `	}` |
|      - | 4386 | `	/* Write the data */` |
|      7 | 4387 | `	if( pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)zData,(ph7_int64)nLen) < 1 ){` |
|    ! 0 | 4388 | `		SXUNUSED(pKey); /* cc warning */` |
|    ! 0 | 4389 | `		return PH7_ABORT;` |
|      - | 4390 | `	}` |
|      7 | 4391 | `	if( c2 > 0 ){` |
|      - | 4392 | `		/* Write the enclosure */` |
|    ! 0 | 4393 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4394 | `		if( c2 > 1 ){` |
|    ! 0 | 4395 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4396 | `		}` |
|    ! 0 | 4397 | `	}` |
|      7 | 4398 | `	pData->iCount++;` |
|      7 | 4399 | `	return PH7_OK;` |
|      4 | 4400 | `}` |
|      - | 4401 | `/*` |
|      - | 4402 | ` * int fputcsv(resource $handle,array $fields[,string $delimiter = ','[,string $enclosure = '"' ]])` |
|      - | 4403 | ` *  Format line as CSV and write to file pointer.` |
|      - | 4404 | ` * Parameters` |
|      - | 4405 | ` *  $handle` |
|      - | 4406 | ` *   Open file handle.` |
|      - | 4407 | ` * $fields` |
|      - | 4408 | ` *   An array of values.` |
|      - | 4409 | ` * $delimiter` |
|      - | 4410 | ` *   The optional delimiter parameter sets the field delimiter (one character only).` |
|      - | 4411 | ` * $enclosure` |
|      - | 4412 | ` *  The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 4413 | ` */` |
|      2 | 4414 | `static int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4415 | `{` |
|      - | 4416 | `	const ph7_io_stream *pStream;` |
|      - | 4417 | `	struct csv_data sCsv;` |
|      - | 4418 | `	io_private *pDev;` |
|      - | 4419 | `	char *zEol;` |
|      - | 4420 | `	int eolen;` |
|      3 | 4421 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4422 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4423 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|    ! 0 | 4424 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4425 | `		return PH7_OK;` |
|      - | 4426 | `	}` |
|      - | 4427 | `	/* Extract our private data */` |
|      3 | 4428 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4429 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4430 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4431 | `		/*Expecting an IO handle */` |
|    ! 0 | 4432 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4433 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4434 | `		return PH7_OK;` |
|      - | 4435 | `	}` |
|      - | 4436 | `	/* Point to the target IO stream device */` |
|      3 | 4437 | `	pStream = pDev->pStream;` |
|      3 | 4438 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4439 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4440 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4441 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4442 | `			);` |
|    ! 0 | 4443 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4444 | `		return PH7_OK;` |
|      - | 4445 | `	}` |
|      - | 4446 | `	/* Set default csv separator */` |
|      3 | 4447 | `	sCsv.delimiter = ',';` |
|      3 | 4448 | `	sCsv.enclosure = '"';` |
|      3 | 4449 | `	sCsv.pDev = pDev;` |
|      3 | 4450 | `	sCsv.iCount = 0;` |
|      3 | 4451 | `	if( nArg > 2 ){` |
|      - | 4452 | `		/* User delimiter */` |
|      - | 4453 | `		const char *z;` |
|      - | 4454 | `		int n;` |
|      3 | 4455 | `		z = ph7_value_to_string(apArg[2],&n);` |
|      3 | 4456 | `		if( n > 0 ){` |
|      3 | 4457 | `			sCsv.delimiter = z[0];` |
|      1 | 4458 | `		}` |
|      3 | 4459 | `		if( nArg > 3 ){` |
|      3 | 4460 | `			z = ph7_value_to_string(apArg[3],&n);` |
|      3 | 4461 | `			if( n > 0 ){` |
|      3 | 4462 | `				sCsv.enclosure = z[0];` |
|      1 | 4463 | `			}` |
|      1 | 4464 | `		}` |
|      1 | 4465 | `	}` |
|      - | 4466 | `	/* Iterate throw array entries and write csv data */` |
|      3 | 4467 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      - | 4468 | `	/* Write a line ending */` |
|      - | 4469 | `#ifdef __WINNT__` |
|      1 | 4470 | `	zEol = "\r\n";` |
|      1 | 4471 | `	eolen = (int)sizeof("\r\n")-1;` |
|      - | 4472 | `#else` |
|      - | 4473 | `	/* Assume UNIX LF */` |
|      2 | 4474 | `	zEol = "\n";` |
|      2 | 4475 | `	eolen = (int)sizeof(char);` |
|      - | 4476 | `#endif` |
|      3 | 4477 | `	pDev->pStream->xWrite(pDev->pHandle,(const void *)zEol,eolen);` |
|      3 | 4478 | `	return PH7_OK;` |
|      2 | 4479 | `}` |
|      - | 4480 | `/*` |
|      - | 4481 | ` * fprintf,vfprintf private data.` |
|      - | 4482 | ` * An instance of the following structure is passed to the formatted` |
|      - | 4483 | ` * input consumer callback defined below.` |
|      - | 4484 | ` */` |
|      - | 4485 | `typedef struct fprintf_data fprintf_data;` |
|      - | 4486 | `struct fprintf_data` |
|      - | 4487 | `{` |
|      - | 4488 | `	io_private *pIO;        /* IO stream */` |
|      - | 4489 | `	ph7_int64 nCount;       /* Total number of bytes written */` |
|      - | 4490 | `};` |
|      - | 4491 | `/*` |
|      - | 4492 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 4493 | ` */` |
|     30 | 4494 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4495 | `{` |
|     31 | 4496 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 4497 | `	ph7_int64 n;` |
|      - | 4498 | `	/* Write the formatted data */` |
|     31 | 4499 | `	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);` |
|     31 | 4500 | `	if( n < 1 ){` |
|    ! 0 | 4501 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 4502 | `		/* IO error,abort immediately */` |
|    ! 0 | 4503 | `		return SXERR_ABORT;` |
|      - | 4504 | `	}` |
|      - | 4505 | `	/* Increment counter */` |
|     31 | 4506 | `	pFdata->nCount += n;` |
|     31 | 4507 | `	return PH7_OK;` |
|     16 | 4508 | `}` |
|      - | 4509 | `/*` |
|      - | 4510 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 4511 | ` *  Write a formatted string to a stream.` |
|      - | 4512 | ` * Parameters` |
|      - | 4513 | ` *  $handle` |
|      - | 4514 | ` *   The file pointer.` |
|      - | 4515 | ` *  $format` |
|      - | 4516 | ` *   String format (see sprintf()).` |
|      - | 4517 | ` * Return` |
|      - | 4518 | ` *  The length of the written string.` |
|      - | 4519 | ` */` |
|     16 | 4520 | `static int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4521 | `{` |
|      - | 4522 | `	fprintf_data sFdata;` |
|      - | 4523 | `	const char *zFormat;` |
|      - | 4524 | `	io_private *pDev;` |
|      - | 4525 | `	int nLen;` |
|     17 | 4526 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4527 | `		/* Missing/Invalid arguments,return zero */` |
|    ! 0 | 4528 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4529 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4530 | `		return PH7_OK;` |
|      - | 4531 | `	}` |
|      - | 4532 | `	/* Extract our private data */` |
|     17 | 4533 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4534 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     17 | 4535 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4536 | `		/*Expecting an IO handle */` |
|    ! 0 | 4537 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4538 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4539 | `		return PH7_OK;` |
|      - | 4540 | `	}` |
|      - | 4541 | `	/* Point to the target IO stream device */` |
|     17 | 4542 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4543 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4544 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4545 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4546 | `			);` |
|    ! 0 | 4547 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4548 | `		return PH7_OK;` |
|      - | 4549 | `	}` |
|      - | 4550 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */` |
|      - | 4551 | `	{` |
|     17 | 4552 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|     17 | 4553 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4554 | `			return rcf;` |
|      - | 4555 | `		}` |
|      - | 4556 | `	}` |
|      - | 4557 | `	/* Extract the string format (scalars/null coerce). */` |
|     17 | 4558 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     17 | 4559 | `	if( nLen < 1 ){` |
|      - | 4560 | `		/* Empty string,return zero */` |
|    ! 0 | 4561 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4562 | `		return PH7_OK;` |
|      - | 4563 | `	}` |
|      - | 4564 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4565 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4566 | `	{` |
|     17 | 4567 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     17 | 4568 | `		if( rcv != PH7_OK ){` |
|      3 | 4569 | `			return rcv;` |
|      - | 4570 | `		}` |
|      - | 4571 | `	}` |
|      - | 4572 | `	/* Prepare our private data */` |
|     15 | 4573 | `	sFdata.nCount = 0;` |
|     15 | 4574 | `	sFdata.pIO = pDev;` |
|      - | 4575 | `	/* Format the string */` |
|     15 | 4576 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 4577 | `	/* Return total number of bytes written */` |
|     15 | 4578 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|     15 | 4579 | `	return PH7_OK;` |
|      9 | 4580 | `}` |
|      - | 4581 | `/*` |
|      - | 4582 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 4583 | ` *  Write a formatted string to a stream.` |
|      - | 4584 | ` * Parameters` |
|      - | 4585 | ` *  $handle` |
|      - | 4586 | ` *   The file pointer.` |
|      - | 4587 | ` *  $format` |
|      - | 4588 | ` *   String format (see sprintf()).` |
|      - | 4589 | ` * $args` |
|      - | 4590 | ` *   User arguments.` |
|      - | 4591 | ` * Return` |
|      - | 4592 | ` *  The length of the written string.` |
|      - | 4593 | ` */` |
|      4 | 4594 | `static int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4595 | `{` |
|      - | 4596 | `	fprintf_data sFdata;` |
|      - | 4597 | `	const char *zFormat;` |
|      - | 4598 | `	ph7_hashmap *pMap;` |
|      - | 4599 | `	io_private *pDev;` |
|      - | 4600 | `	SySet sArg;` |
|      - | 4601 | `	int n,nLen;` |
|      5 | 4602 | `	if( nArg < 3 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4603 | `		/* Missing/Invalid arguments,return zero */` |
|      3 | 4604 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|      3 | 4605 | `		ph7_result_int(pCtx,0);` |
|      3 | 4606 | `		return PH7_OK;` |
|      - | 4607 | `	}` |
|      - | 4608 | `	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */` |
|      - | 4609 | `	{` |
|      3 | 4610 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      3 | 4611 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4612 | `			return rcf;` |
|      - | 4613 | `		}` |
|      - | 4614 | `	}` |
|      3 | 4615 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|      - | 4616 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4617 | `		char zBuf[64];` |
|    ! 0 | 4618 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4619 | `			"vfprintf(): Argument #3 ($values) must be of type array, %s given",` |
|    ! 0 | 4620 | `			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|      - | 4621 | `	}` |
|      - | 4622 | `	/* Extract our private data */` |
|      3 | 4623 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4624 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4625 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4626 | `		/*Expecting an IO handle */` |
|    ! 0 | 4627 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4628 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4629 | `		return PH7_OK;` |
|      - | 4630 | `	}` |
|      - | 4631 | `	/* Point to the target IO stream device */` |
|      3 | 4632 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4633 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4634 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4635 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4636 | `			);` |
|    ! 0 | 4637 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4638 | `		return PH7_OK;` |
|      - | 4639 | `	}` |
|      - | 4640 | `	/* Extract the string format */` |
|      3 | 4641 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      3 | 4642 | `	if( nLen < 1 ){` |
|      - | 4643 | `		/* Empty string,return zero */` |
|    ! 0 | 4644 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4645 | `		return PH7_OK;` |
|      - | 4646 | `	}` |
|      - | 4647 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4648 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4649 | `	{` |
|      3 | 4650 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 4651 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4652 | `			return rcv;` |
|      - | 4653 | `		}` |
|      - | 4654 | `	}` |
|      - | 4655 | `	/* Point to hashmap */` |
|      3 | 4656 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 4657 | `	/* Extract arguments from the hashmap */` |
|      3 | 4658 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4659 | `	/* Prepare our private data */` |
|      3 | 4660 | `	sFdata.nCount = 0;` |
|      3 | 4661 | `	sFdata.pIO = pDev;` |
|      - | 4662 | `	/* Format the string */` |
|      3 | 4663 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 4664 | `	/* Return total number of bytes written*/` |
|      3 | 4665 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      3 | 4666 | `	SySetRelease(&sArg);` |
|      3 | 4667 | `	return PH7_OK;` |
|      3 | 4668 | `}` |
|      - | 4669 | `/*` |
|      - | 4670 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 4671 | ` * According to the PHP reference manual:` |
|      - | 4672 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 4673 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 4674 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 4675 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 4676 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 4677 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 4678 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 4679 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 4680 | ` *         exist, attempt to create it.` |
|      - | 4681 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 4682 | ` *          not exist, attempt to create it.` |
|      - | 4683 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 4684 | ` *         already exists,` |
|      - | 4685 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 4686 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 4687 | ` *         the underlying open(2) system call.` |
|      - | 4688 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 4689 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 4690 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 4691 | ` *          is positioned on the beginning of the file.` |
|      - | 4692 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 4693 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 4694 | ` *          be used after the lock is requested).` |
|      - | 4695 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 4696 | ` */` |
|     80 | 4697 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      3 | 4698 | `{` |
|     83 | 4699 | `	const char *zEnd = &zMode[nLen];` |
|     83 | 4700 | `	int iFlag = 0;` |
|      - | 4701 | `	int c;` |
|     83 | 4702 | `	if( nLen < 1 ){` |
|      - | 4703 | `		/* Open in a read-only mode */` |
|    ! 0 | 4704 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 4705 | `	}` |
|     83 | 4706 | `	c = zMode[0];` |
|     83 | 4707 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 4708 | `		/* Read-only access */` |
|     53 | 4709 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|     53 | 4710 | `		zMode++; /* Advance */` |
|     53 | 4711 | `		if( zMode < zEnd ){` |
|     13 | 4712 | `			c = zMode[0];` |
|     13 | 4713 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 4714 | `				/* Read+Write access */` |
|     13 | 4715 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|      6 | 4716 | `			}` |
|      9 | 4717 | `		}` |
|     57 | 4718 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 4719 | `		/* Overwrite mode.` |
|      - | 4720 | `		 * If the file does not exists,try to create it` |
|      - | 4721 | `		 */` |
|     32 | 4722 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     32 | 4723 | `		zMode++; /* Advance */` |
|     32 | 4724 | `		if( zMode < zEnd ){` |
|      5 | 4725 | `			c = zMode[0];` |
|      5 | 4726 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4727 | `				/* Read+Write access */` |
|      5 | 4728 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|      5 | 4729 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|      2 | 4730 | `			}` |
|      4 | 4731 | `		}` |
|     15 | 4732 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 4733 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 4734 | `		 * Create the file if it does not exists.` |
|      - | 4735 | `		 */` |
|    ! 0 | 4736 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4737 | `		zMode++; /* Advance */` |
|    ! 0 | 4738 | `		if( zMode < zEnd ){` |
|    ! 0 | 4739 | `			c = zMode[0];` |
|    ! 0 | 4740 | `			if( c == '+' ){` |
|      - | 4741 | `				/* Read-Write access */` |
|    ! 0 | 4742 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4743 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4744 | `			}` |
|    ! 0 | 4745 | `		}` |
|    ! 0 | 4746 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 4747 | `		/* Exclusive access.` |
|      - | 4748 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 4749 | `		 * Otherwise create a new file.` |
|      - | 4750 | `		 */` |
|    ! 0 | 4751 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|    ! 0 | 4752 | `		zMode++; /* Advance */` |
|    ! 0 | 4753 | `		if( zMode < zEnd ){` |
|    ! 0 | 4754 | `			c = zMode[0];` |
|    ! 0 | 4755 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4756 | `				/* Read-Write access */` |
|    ! 0 | 4757 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4758 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4759 | `			}` |
|    ! 0 | 4760 | `		}` |
|    ! 0 | 4761 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 4762 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 4763 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4764 | `		zMode++; /* Advance */` |
|    ! 0 | 4765 | `		if( zMode < zEnd ){` |
|    ! 0 | 4766 | `			c = zMode[0];` |
|    ! 0 | 4767 | `			if( c == '+' ){` |
|      - | 4768 | `				/* Read-Write access */` |
|    ! 0 | 4769 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4770 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4771 | `			}` |
|    ! 0 | 4772 | `		}` |
|    ! 0 | 4773 | `	}else{` |
|      - | 4774 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 4775 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 4776 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 4777 | `	}` |
|     99 | 4778 | `	while( zMode < zEnd ){` |
|     17 | 4779 | `		c = zMode[0];` |
|     17 | 4780 | `		if( c == 'b' \|\| c == 'B' ){` |
|    ! 0 | 4781 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4782 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|     17 | 4783 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 4784 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 4785 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4786 | `		}` |
|     17 | 4787 | `		zMode++;` |
|      1 | 4788 | `	}` |
|     83 | 4789 | `	return iFlag;` |
|     43 | 4790 | `}` |
|      - | 4791 | `/*` |
|      - | 4792 | ` * Initialize the IO private structure.` |
|      - | 4793 | ` */` |
|   5138 | 4794 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      5 | 4795 | `{` |
|   5143 | 4796 | `	pOut->pStream = pStream;` |
|   5143 | 4797 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   5143 | 4798 | `	pOut->nOfft = 0;` |
|      - | 4799 | `	/* Set the magic number */` |
|   5143 | 4800 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   5143 | 4801 | `}` |
|      - | 4802 | `/*` |
|      - | 4803 | ` * Release the IO private structure.` |
|      - | 4804 | ` */` |
|   5098 | 4805 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      5 | 4806 | `{` |
|   5103 | 4807 | `	SyBlobRelease(&pDev->sBuffer);` |
|   5103 | 4808 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 4809 | `	/* Release the whole structure */` |
|   5103 | 4810 | `	ph7_context_free_chunk(pCtx,pDev);` |
|   5103 | 4811 | `}` |
|      - | 4812 | `/*` |
|      - | 4813 | ` * Reset the IO private structure.` |
|      - | 4814 | ` */` |
|     30 | 4815 | `static void ResetIOPrivate(io_private *pDev)` |
|      2 | 4816 | `{` |
|     32 | 4817 | `	SyBlobReset(&pDev->sBuffer);` |
|     32 | 4818 | `	pDev->nOfft = 0;` |
|     32 | 4819 | `}` |
|      - | 4820 | `/* Forward declaration */` |
|      - | 4821 |  |
|      - | 4822 | `/*` |
|      - | 4823 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 4824 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 4825 | ` * Parameters` |
|      - | 4826 | ` *  $filename` |
|      - | 4827 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 4828 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 4829 | ` *   then a regular file is assumed.` |
|      - | 4830 | ` *  $mode` |
|      - | 4831 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 4832 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 4833 | ` *   modes.` |
|      - | 4834 | ` *  $use_include_path` |
|      - | 4835 | ` *   You can use the optional second parameter and set it to` |
|      - | 4836 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 4837 | ` *  $context` |
|      - | 4838 | ` *   A context stream resource.` |
|      - | 4839 | ` * Return` |
|      - | 4840 | ` *  File handle on success or FALSE on failure.` |
|      - | 4841 | ` */` |
|      - | 4842 | `/*` |
|      - | 4843 | ` * string\|false stream_get_contents(resource $stream, int $maxLength = -1,` |
|      - | 4844 | ` *                                  int $offset = -1)` |
|      - | 4845 | ` *  Read the remaining contents of a stream into a string.` |
|      - | 4846 | ` */` |
|     10 | 4847 | `static int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4848 | `{` |
|      - | 4849 | `	const ph7_io_stream *pStream;` |
|      - | 4850 | `	io_private *pDev;` |
|     11 | 4851 | `	ph7_int64 nMax = -1;` |
|      - | 4852 | `	char zBuf[4096];` |
|      - | 4853 | `	ph7_int64 nRead;` |
|     11 | 4854 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 4855 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4856 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4857 | `		return PH7_OK;` |
|      - | 4858 | `	}` |
|     11 | 4859 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     11 | 4860 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 4861 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4862 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4863 | `		return PH7_OK;` |
|      - | 4864 | `	}` |
|     11 | 4865 | `	pStream = pDev->pStream;` |
|     11 | 4866 | `	if( pStream == 0 \|\| pStream->xRead == 0 ){` |
|    ! 0 | 4867 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4868 | `		return PH7_OK;` |
|      - | 4869 | `	}` |
|     11 | 4870 | `	if( nArg > 1 ){` |
|      5 | 4871 | `		nMax = ph7_value_to_int64(apArg[1]);` |
|      2 | 4872 | `	}` |
|     11 | 4873 | `	if( nArg > 2 ){` |
|      5 | 4874 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|      5 | 4875 | `		if( iOfft >= 0 && pStream->xSeek ){` |
|      5 | 4876 | `			pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/);` |
|      2 | 4877 | `		}` |
|      2 | 4878 | `	}` |
|     11 | 4879 | `	ph7_result_string(pCtx,"",0); /* seed an empty string result */` |
|     22 | 4880 | `	while( nMax != 0 ){` |
|     20 | 4881 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|     20 | 4882 | `		if( nMax > 0 && nMax < nAsk ){` |
|      3 | 4883 | `			nAsk = nMax;` |
|      1 | 4884 | `		}` |
|     20 | 4885 | `		nRead = pStream->xRead(pDev->pHandle,zBuf,nAsk);` |
|     20 | 4886 | `		if( nRead < 1 ){` |
|      9 | 4887 | `			break;` |
|      - | 4888 | `		}` |
|     12 | 4889 | `		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */` |
|     12 | 4890 | `		if( nMax > 0 ){` |
|      3 | 4891 | `			nMax -= nRead;` |
|      1 | 4892 | `		}` |
|      1 | 4893 | `	}` |
|     11 | 4894 | `	return PH7_OK;` |
|      6 | 4895 | `}` |
|      - | 4896 | `/*` |
|      - | 4897 | ` * array stream_get_wrappers(void) — names of the registered stream devices.` |
|      - | 4898 | ` */` |
|      4 | 4899 | `static int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4900 | `{` |
|      - | 4901 | `	ph7_value *pArr,*pV;` |
|      - | 4902 | `	ph7_io_stream **apDev;` |
|      - | 4903 | `	sxu32 n;` |
|      2 | 4904 | `	SXUNUSED(nArg);` |
|      2 | 4905 | `	SXUNUSED(apArg);` |
|      6 | 4906 | `	pArr = ph7_context_new_array(pCtx);` |
|      6 | 4907 | `	pV = ph7_context_new_scalar(pCtx);` |
|      6 | 4908 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 4909 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4910 | `		return PH7_OK;` |
|      - | 4911 | `	}` |
|      6 | 4912 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|     24 | 4913 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|     20 | 4914 | `		ph7_value_string(pV,apDev[n]->zName,-1);` |
|     20 | 4915 | `		ph7_array_add_elem(pArr,0,pV);` |
|     20 | 4916 | `		ph7_value_reset_string_cursor(pV);` |
|     11 | 4917 | `	}` |
|      6 | 4918 | `	ph7_result_value(pCtx,pArr);` |
|      6 | 4919 | `	return PH7_OK;` |
|      4 | 4920 | `}` |
|      - | 4921 | `/*` |
|      - | 4922 | ` * array stream_get_meta_data(resource $stream) — best-effort php shape over` |
|      - | 4923 | ` * the io_private state (uri/wrapper_type/seekable/eof; recorded approximation).` |
|      - | 4924 | ` */` |
|      2 | 4925 | `static int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4926 | `{` |
|      - | 4927 | `	io_private *pDev;` |
|      - | 4928 | `	ph7_value *pArr,*pV;` |
|      3 | 4929 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 4930 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4931 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4932 | `		return PH7_OK;` |
|      - | 4933 | `	}` |
|      3 | 4934 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      3 | 4935 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 4936 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4937 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4938 | `		return PH7_OK;` |
|      - | 4939 | `	}` |
|      3 | 4940 | `	pArr = ph7_context_new_array(pCtx);` |
|      3 | 4941 | `	pV = ph7_context_new_scalar(pCtx);` |
|      3 | 4942 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 4943 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4944 | `		return PH7_OK;` |
|      - | 4945 | `	}` |
|      3 | 4946 | `	ph7_value_bool(pV,0);` |
|      3 | 4947 | `	ph7_array_add_strkey_elem(pArr,"timed_out",pV);` |
|      3 | 4948 | `	ph7_value_bool(pV,1);` |
|      3 | 4949 | `	ph7_array_add_strkey_elem(pArr,"blocked",pV);` |
|      - | 4950 | `	/* eof is best-effort: a read probe would consume state on unseekable` |
|      - | 4951 | `	 * devices, so report FALSE and let feof() answer properly */` |
|      3 | 4952 | `	ph7_value_bool(pV,0);` |
|      3 | 4953 | `	ph7_array_add_strkey_elem(pArr,"eof",pV);` |
|      3 | 4954 | `	ph7_value_int(pV,0);` |
|      3 | 4955 | `	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);` |
|      3 | 4956 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 4957 | `	ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);` |
|      3 | 4958 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 4959 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 4960 | `	ph7_array_add_strkey_elem(pArr,"stream_type",pV);` |
|      3 | 4961 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 4962 | `	ph7_value_bool(pV,pDev->pStream && pDev->pStream->xSeek != 0);` |
|      3 | 4963 | `	ph7_array_add_strkey_elem(pArr,"seekable",pV);` |
|      3 | 4964 | `	ph7_result_value(pCtx,pArr);` |
|      3 | 4965 | `	return PH7_OK;` |
|      2 | 4966 | `}` |
|      - | 4967 | `/*` |
|      - | 4968 | ` * stream_context_create([array $options[, array $params]]) — INERT: PHL has` |
|      - | 4969 | ` * no context plumbing yet; the options array itself is returned so code that` |
|      - | 4970 | ` * creates and passes contexts keeps working (recorded divergence: not a` |
|      - | 4971 | ` * resource, options unconsumed).` |
|      - | 4972 | ` */` |
|      2 | 4973 | `static int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4974 | `{` |
|      3 | 4975 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|      3 | 4976 | `		ph7_result_value(pCtx,apArg[0]);` |
|      2 | 4977 | `	}else{` |
|    ! 0 | 4978 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 4979 | `		if( pArr == 0 ){` |
|    ! 0 | 4980 | `			ph7_result_null(pCtx);` |
|    ! 0 | 4981 | `			return PH7_OK;` |
|      - | 4982 | `		}` |
|    ! 0 | 4983 | `		ph7_result_value(pCtx,pArr);` |
|      - | 4984 | `	}` |
|      3 | 4985 | `	return PH7_OK;` |
|      2 | 4986 | `}` |
|      - | 4987 | `/*` |
|      - | 4988 | ` * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a` |
|      - | 4989 | ` * small struct carrying the OS socket plus an EOF latch, so feof() works.` |
|      - | 4990 | ` */` |
|      - | 4991 | `#ifdef PH7_ENABLE_NET` |
|      - | 4992 | `typedef struct sock_private sock_private;` |
|      - | 4993 | `struct sock_private` |
|      - | 4994 | `{` |
|      - | 4995 | `	ph7_vm *pVm;` |
|      - | 4996 | `	ph7_socket sock;` |
|      - | 4997 | `	int bEof;` |
|      - | 4998 | `};` |
|     11 | 4999 | `static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|    ! 0 | 5000 | `{` |
|     11 | 5001 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 5002 | `	int n;` |
|     11 | 5003 | `	if( pSock == 0 \|\| pSock->bEof ){` |
|      2 | 5004 | `		return 0;` |
|      - | 5005 | `	}` |
|      9 | 5006 | `	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);` |
|      9 | 5007 | `	if( n <= 0 ){` |
|      4 | 5008 | `		pSock->bEof = 1;` |
|      4 | 5009 | `		return 0;` |
|      - | 5010 | `	}` |
|      5 | 5011 | `	return (ph7_int64)n;` |
|      5 | 5012 | `}` |
|      4 | 5013 | `static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 5014 | `{` |
|      4 | 5015 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 5016 | `	int n;` |
|      4 | 5017 | `	if( pSock == 0 ){` |
|    ! 0 | 5018 | `		return -1;` |
|      - | 5019 | `	}` |
|      4 | 5020 | `	n = PH7_NetSendAll(pSock->sock,pBuf,(int)nWrite);` |
|      4 | 5021 | `	return n < 0 ? -1 : (ph7_int64)n;` |
|      2 | 5022 | `}` |
|      4 | 5023 | `static void SockStreamData_Close(void *pHandle)` |
|    ! 0 | 5024 | `{` |
|      4 | 5025 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      4 | 5026 | `	if( pSock == 0 ){` |
|    ! 0 | 5027 | `		return;` |
|      - | 5028 | `	}` |
|      4 | 5029 | `	PH7_NetClose(pSock->sock);` |
|      4 | 5030 | `	SyMemBackendFree(&pSock->pVm->sAllocator,pSock);` |
|      2 | 5031 | `}` |
|      - | 5032 | `/* xOpen for "host:port" (the scheme is already stripped by the device lookup) */` |
|    ! 0 | 5033 | `static int SockStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|    ! 0 | 5034 | `{` |
|      - | 5035 | `	sock_private *pSock;` |
|      - | 5036 | `	ph7_socket sock;` |
|      - | 5037 | `	char zHost[256];` |
|      - | 5038 | `	const char *zColon;` |
|    ! 0 | 5039 | `	int iPort = 0,iErrno = 0;` |
|    ! 0 | 5040 | `	const char *zErr = "";` |
|    ! 0 | 5041 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|    ! 0 | 5042 | `	SXUNUSED(iMode);` |
|    ! 0 | 5043 | `	if( pVm == 0 ){` |
|    ! 0 | 5044 | `		return -1;` |
|      - | 5045 | `	}` |
|    ! 0 | 5046 | `	zColon = SyStrlen(zName) ? &zName[SyStrlen(zName)-1] : zName;` |
|    ! 0 | 5047 | `	while( zColon > zName && zColon[0] != ':' ){` |
|    ! 0 | 5048 | `		zColon--;` |
|    ! 0 | 5049 | `	}` |
|    ! 0 | 5050 | `	if( zColon <= zName \|\| zColon[0] != ':' ){` |
|    ! 0 | 5051 | `		return -1;` |
|      - | 5052 | `	}` |
|      - | 5053 | `	{` |
|    ! 0 | 5054 | `		sxu32 n = (sxu32)(zColon - zName);` |
|    ! 0 | 5055 | `		if( n >= sizeof(zHost) ){` |
|    ! 0 | 5056 | `			n = sizeof(zHost) - 1;` |
|    ! 0 | 5057 | `		}` |
|    ! 0 | 5058 | `		SyMemcpy(zName,zHost,n);` |
|    ! 0 | 5059 | `		zHost[n] = 0;` |
|      - | 5060 | `	}` |
|      - | 5061 | `	{` |
|    ! 0 | 5062 | `		sxi32 iTmp = 0;` |
|    ! 0 | 5063 | `		SyStrToInt32(&zColon[1],(sxu32)SyStrlen(&zColon[1]),(void *)&iTmp,0);` |
|    ! 0 | 5064 | `		iPort = (int)iTmp;` |
|      - | 5065 | `	}` |
|    ! 0 | 5066 | `	sock = PH7_NetConnect(zHost,iPort,0,&iErrno,&zErr);` |
|    ! 0 | 5067 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    ! 0 | 5068 | `		return -1;` |
|      - | 5069 | `	}` |
|    ! 0 | 5070 | `	pSock = (sock_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(sock_private));` |
|    ! 0 | 5071 | `	if( pSock == 0 ){` |
|    ! 0 | 5072 | `		PH7_NetClose(sock);` |
|    ! 0 | 5073 | `		return -1;` |
|      - | 5074 | `	}` |
|    ! 0 | 5075 | `	pSock->pVm = pVm;` |
|    ! 0 | 5076 | `	pSock->sock = sock;` |
|    ! 0 | 5077 | `	pSock->bEof = 0;` |
|    ! 0 | 5078 | `	*ppHandle = (void *)pSock;` |
|    ! 0 | 5079 | `	return PH7_OK;` |
|    ! 0 | 5080 | `}` |
|      - | 5081 | `static const ph7_io_stream sTCP_Stream = {` |
|      - | 5082 | `	"tcp",` |
|      - | 5083 | `	PH7_IO_STREAM_VERSION,` |
|      - | 5084 | `	SockStreamData_Open, /* xOpen */` |
|      - | 5085 | `	0,   /* xOpenDir */` |
|      - | 5086 | `	SockStreamData_Close,/* xClose */` |
|      - | 5087 | `	0,  /* xCloseDir */` |
|      - | 5088 | `	SockStreamData_Read, /* xRead */` |
|      - | 5089 | `	0,  /* xReadDir */` |
|      - | 5090 | `	SockStreamData_Write,/* xWrite */` |
|      - | 5091 | `	0,  /* xSeek (sockets are not seekable) */` |
|      - | 5092 | `	0,  /* xLock */` |
|      - | 5093 | `	0,  /* xRewindDir */` |
|      - | 5094 | `	0,  /* xTell */` |
|      - | 5095 | `	0,  /* xTrunc */` |
|      - | 5096 | `	0,  /* xSync */` |
|      - | 5097 | `	0   /* xStat */` |
|      - | 5098 | `};` |
|      - | 5099 | `#endif /* PH7_ENABLE_NET */` |
|      - | 5100 | `/*` |
|      - | 5101 | ` * Userland stream wrappers (stream_wrapper_register). The engine's device` |
|      - | 5102 | ` * callbacks receive no device pointer, so each registered wrapper needs its` |
|      - | 5103 | ` * OWN xOpen thunk: PHL keeps a bounded pool of PHL_UWRAP_MAX slots, each with` |
|      - | 5104 | ` * a static thunk that knows its index (recorded limit; php has no cap).` |
|      - | 5105 | ` * The handle carries the userland object, and every stream op dispatches the` |
|      - | 5106 | ` * php streamWrapper protocol method on it.` |
|      - | 5107 | ` */` |
|      - | 5108 | `#define PHL_UWRAP_MAX 8` |
|      - | 5109 | `typedef struct uwrap_slot uwrap_slot;` |
|      - | 5110 | `struct uwrap_slot` |
|      - | 5111 | `{` |
|      - | 5112 | `	ph7_vm *pVm;              /* owning VM (0 = free slot) */` |
|      - | 5113 | `	char zScheme[32];         /* protocol name */` |
|      - | 5114 | `	char zClass[128];         /* userland wrapper class */` |
|      - | 5115 | `	ph7_io_stream sStream;    /* the device handed to the VM */` |
|      - | 5116 | `};` |
|      - | 5117 | `typedef struct uwrap_handle uwrap_handle;` |
|      - | 5118 | `struct uwrap_handle` |
|      - | 5119 | `{` |
|      - | 5120 | `	ph7_vm *pVm;` |
|      - | 5121 | `	ph7_class_instance *pObj; /* the wrapper instance (one per open stream) */` |
|      - | 5122 | `	int iSlot;` |
|      - | 5123 | `	int bEof;` |
|      - | 5124 | `};` |
|      - | 5125 | `static uwrap_slot g_aUwrap[PHL_UWRAP_MAX];` |
|      - | 5126 | `/* Call $obj->$zMethod(...) and copy the result into pResult (may be 0) */` |
|     26 | 5127 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|      - | 5128 | `	ph7_value *pResult)` |
|      1 | 5129 | `{` |
|      - | 5130 | `	ph7_class_method *pMeth;` |
|     27 | 5131 | `	if( pH == 0 \|\| pH->pObj == 0 ){` |
|    ! 0 | 5132 | `		return -1;` |
|      - | 5133 | `	}` |
|     27 | 5134 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|     27 | 5135 | `	if( pMeth == 0 ){` |
|    ! 0 | 5136 | `		return -1;` |
|      - | 5137 | `	}` |
|     27 | 5138 | `	if( PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|    ! 0 | 5139 | `		return -1;` |
|      - | 5140 | `	}` |
|     27 | 5141 | `	return 0;` |
|     14 | 5142 | `}` |
|      8 | 5143 | `static ph7_int64 UwrapRead(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|      1 | 5144 | `{` |
|      9 | 5145 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5146 | `	ph7_value sArg,sRet;` |
|      - | 5147 | `	const char *zData;` |
|      9 | 5148 | `	int nData = 0;` |
|      9 | 5149 | `	ph7_int64 n = 0;` |
|      9 | 5150 | `	if( pH == 0 \|\| pH->bEof ){` |
|    ! 0 | 5151 | `		return 0;` |
|      - | 5152 | `	}` |
|      9 | 5153 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      9 | 5154 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      9 | 5155 | `	ph7_value_int64(&sArg,nRead);` |
|      - | 5156 | `	{` |
|      - | 5157 | `		ph7_value *apArg[1];` |
|      9 | 5158 | `		apArg[0] = &sArg;` |
|      9 | 5159 | `		if( UwrapCall(pH,"stream_read",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5160 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5161 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5162 | `			return -1;` |
|      - | 5163 | `		}` |
|      - | 5164 | `	}` |
|      9 | 5165 | `	zData = ph7_value_to_string(&sRet,&nData);` |
|      9 | 5166 | `	if( nData > 0 ){` |
|      7 | 5167 | `		if( (ph7_int64)nData > nRead ){` |
|    ! 0 | 5168 | `			nData = (int)nRead;` |
|    ! 0 | 5169 | `		}` |
|      7 | 5170 | `		SyMemcpy(zData,pBuffer,(sxu32)nData);` |
|      7 | 5171 | `		n = nData;` |
|      4 | 5172 | `	}else{` |
|      3 | 5173 | `		pH->bEof = 1;` |
|      - | 5174 | `	}` |
|      9 | 5175 | `	PH7_MemObjRelease(&sArg);` |
|      9 | 5176 | `	PH7_MemObjRelease(&sRet);` |
|      9 | 5177 | `	return n;` |
|      5 | 5178 | `}` |
|      2 | 5179 | `static ph7_int64 UwrapWrite(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 5180 | `{` |
|      3 | 5181 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5182 | `	ph7_value sArg,sRet;` |
|      - | 5183 | `	ph7_int64 n;` |
|      3 | 5184 | `	if( pH == 0 ){` |
|    ! 0 | 5185 | `		return -1;` |
|      - | 5186 | `	}` |
|      3 | 5187 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      3 | 5188 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5189 | `	ph7_value_string(&sArg,(const char *)pBuf,(int)nWrite);` |
|      - | 5190 | `	{` |
|      - | 5191 | `		ph7_value *apArg[1];` |
|      3 | 5192 | `		apArg[0] = &sArg;` |
|      3 | 5193 | `		if( UwrapCall(pH,"stream_write",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5194 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5195 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5196 | `			return -1;` |
|      - | 5197 | `		}` |
|      - | 5198 | `	}` |
|      3 | 5199 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 5200 | `	PH7_MemObjRelease(&sArg);` |
|      3 | 5201 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5202 | `	return n;` |
|      2 | 5203 | `}` |
|      2 | 5204 | `static int UwrapSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 5205 | `{` |
|      3 | 5206 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5207 | `	ph7_value sOfft,sWhence,sRet;` |
|      - | 5208 | `	ph7_value *apArg[2];` |
|      - | 5209 | `	int rc;` |
|      3 | 5210 | `	if( pH == 0 ){` |
|    ! 0 | 5211 | `		return -1;` |
|      - | 5212 | `	}` |
|      3 | 5213 | `	PH7_MemObjInit(pH->pVm,&sOfft);` |
|      3 | 5214 | `	PH7_MemObjInit(pH->pVm,&sWhence);` |
|      3 | 5215 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5216 | `	ph7_value_int64(&sOfft,iOfft);` |
|      3 | 5217 | `	ph7_value_int(&sWhence,whence);` |
|      3 | 5218 | `	apArg[0] = &sOfft;` |
|      3 | 5219 | `	apArg[1] = &sWhence;` |
|      3 | 5220 | `	rc = UwrapCall(pH,"stream_seek",2,apArg,&sRet);` |
|      3 | 5221 | `	if( rc == 0 ){` |
|      3 | 5222 | `		pH->bEof = 0;` |
|      3 | 5223 | `		rc = ph7_value_to_bool(&sRet) ? PH7_OK : -1;` |
|      1 | 5224 | `	}` |
|      3 | 5225 | `	PH7_MemObjRelease(&sOfft);` |
|      3 | 5226 | `	PH7_MemObjRelease(&sWhence);` |
|      3 | 5227 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5228 | `	return rc;` |
|      2 | 5229 | `}` |
|      2 | 5230 | `static ph7_int64 UwrapTell(void *pHandle)` |
|      1 | 5231 | `{` |
|      3 | 5232 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5233 | `	ph7_value sRet;` |
|      - | 5234 | `	ph7_int64 n;` |
|      3 | 5235 | `	if( pH == 0 ){` |
|    ! 0 | 5236 | `		return -1;` |
|      - | 5237 | `	}` |
|      3 | 5238 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5239 | `	if( UwrapCall(pH,"stream_tell",0,0,&sRet) != 0 ){` |
|    ! 0 | 5240 | `		PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5241 | `		return -1;` |
|      - | 5242 | `	}` |
|      3 | 5243 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 5244 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5245 | `	return n;` |
|      2 | 5246 | `}` |
|      6 | 5247 | `static void UwrapClose(void *pHandle)` |
|      1 | 5248 | `{` |
|      7 | 5249 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      7 | 5250 | `	if( pH == 0 ){` |
|    ! 0 | 5251 | `		return;` |
|      - | 5252 | `	}` |
|      7 | 5253 | `	UwrapCall(pH,"stream_close",0,0,0);` |
|      7 | 5254 | `	if( pH->pObj ){` |
|      7 | 5255 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|      3 | 5256 | `	}` |
|      7 | 5257 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|      4 | 5258 | `}` |
|      - | 5259 | `/* Shared open: instantiate the wrapper class and call stream_open() */` |
|      6 | 5260 | `static int UwrapOpenSlot(int iSlot,const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|      1 | 5261 | `{` |
|      7 | 5262 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|      7 | 5263 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|      - | 5264 | `	ph7_class *pClass;` |
|      - | 5265 | `	uwrap_handle *pH;` |
|      - | 5266 | `	ph7_value sPath,sMode,sOpts,sOpened,sRet;` |
|      - | 5267 | `	ph7_value *apArg[4];` |
|      - | 5268 | `	int rc;` |
|      7 | 5269 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|    ! 0 | 5270 | `		return -1;` |
|      - | 5271 | `	}` |
|      7 | 5272 | `	pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)SyStrlen(pSlot->zClass),TRUE,0);` |
|      7 | 5273 | `	if( pClass == 0 ){` |
|    ! 0 | 5274 | `		return -1;` |
|      - | 5275 | `	}` |
|      7 | 5276 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|      7 | 5277 | `	if( pH == 0 ){` |
|    ! 0 | 5278 | `		return -1;` |
|      - | 5279 | `	}` |
|      7 | 5280 | `	pH->pVm = pVm;` |
|      7 | 5281 | `	pH->iSlot = iSlot;` |
|      7 | 5282 | `	pH->bEof = 0;` |
|      7 | 5283 | `	pH->pObj = PH7_NewClassInstance(pVm,pClass);` |
|      7 | 5284 | `	if( pH->pObj == 0 ){` |
|    ! 0 | 5285 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5286 | `		return -1;` |
|      - | 5287 | `	}` |
|      - | 5288 | `	/* php hands stream_open the FULL url, scheme included */` |
|      7 | 5289 | `	PH7_MemObjInit(pVm,&sPath);` |
|      7 | 5290 | `	PH7_MemObjInit(pVm,&sMode);` |
|      7 | 5291 | `	PH7_MemObjInit(pVm,&sOpts);` |
|      7 | 5292 | `	PH7_MemObjInit(pVm,&sRet);` |
|      - | 5293 | `	/* $opened_path is BY REFERENCE: the callee's binding needs a real memobj` |
|      - | 5294 | `	 * slot (a stack ph7_value has nIdx == SXU32_HIGH and the engine rejects` |
|      - | 5295 | `	 * it as "could not be passed by reference"). */` |
|      - | 5296 | `	{` |
|      7 | 5297 | `		ph7_value *pRefSlot = PH7_ReserveMemObj(pVm);` |
|      7 | 5298 | `		if( pRefSlot == 0 ){` |
|    ! 0 | 5299 | `			PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5300 | `			SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5301 | `			return -1;` |
|      - | 5302 | `		}` |
|      7 | 5303 | `		PH7_MemObjInit(pVm,&sOpened);` |
|      7 | 5304 | `		sOpened.nIdx = pRefSlot->nIdx;` |
|      - | 5305 | `	}` |
|      - | 5306 | `	{` |
|      - | 5307 | `		SyBlob sUrl;` |
|      7 | 5308 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|      7 | 5309 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|      7 | 5310 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|      7 | 5311 | `		SyBlobRelease(&sUrl);` |
|      - | 5312 | `	}` |
|      9 | 5313 | `	ph7_value_string(&sMode,(iMode & PH7_IO_OPEN_WRONLY) ? "w"` |
|      4 | 5314 | `		: ((iMode & PH7_IO_OPEN_APPEND) ? "a" : "r"),-1);` |
|      7 | 5315 | `	ph7_value_int(&sOpts,0);` |
|      7 | 5316 | `	apArg[0] = &sPath;` |
|      7 | 5317 | `	apArg[1] = &sMode;` |
|      7 | 5318 | `	apArg[2] = &sOpts;` |
|      7 | 5319 | `	apArg[3] = &sOpened;` |
|      7 | 5320 | `	rc = UwrapCall(pH,"stream_open",4,apArg,&sRet);` |
|      7 | 5321 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|    ! 0 | 5322 | `		rc = -1;` |
|    ! 0 | 5323 | `	}` |
|      7 | 5324 | `	PH7_MemObjRelease(&sPath);` |
|      7 | 5325 | `	PH7_MemObjRelease(&sMode);` |
|      7 | 5326 | `	PH7_MemObjRelease(&sOpts);` |
|      7 | 5327 | `	PH7_MemObjRelease(&sOpened);` |
|      7 | 5328 | `	PH7_MemObjRelease(&sRet);` |
|      7 | 5329 | `	if( rc != 0 ){` |
|    ! 0 | 5330 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5331 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5332 | `		return -1;` |
|      - | 5333 | `	}` |
|      7 | 5334 | `	*ppHandle = (void *)pH;` |
|      7 | 5335 | `	return PH7_OK;` |
|      4 | 5336 | `}` |
|      - | 5337 | `/* One xOpen thunk per slot (the device callbacks get no device pointer) */` |
|      - | 5338 | `#define PHL_UWRAP_THUNK(N) \` |
|      - | 5339 | `	static int UwrapOpen##N(const char *zName,int iMode,ph7_value *pResource,void **ppHandle) \` |
|      - | 5340 | `	{ return UwrapOpenSlot(N,zName,iMode,pResource,ppHandle); }` |
|      7 | 5341 | `PHL_UWRAP_THUNK(0)` |
|    ! 0 | 5342 | `PHL_UWRAP_THUNK(1)` |
|    ! 0 | 5343 | `PHL_UWRAP_THUNK(2)` |
|    ! 0 | 5344 | `PHL_UWRAP_THUNK(3)` |
|    ! 0 | 5345 | `PHL_UWRAP_THUNK(4)` |
|    ! 0 | 5346 | `PHL_UWRAP_THUNK(5)` |
|    ! 0 | 5347 | `PHL_UWRAP_THUNK(6)` |
|    ! 0 | 5348 | `PHL_UWRAP_THUNK(7)` |
|      - | 5349 | `static int (* const g_aUwrapOpen[PHL_UWRAP_MAX])(const char *,int,ph7_value *,void **) = {` |
|      - | 5350 | `	UwrapOpen0,UwrapOpen1,UwrapOpen2,UwrapOpen3,` |
|      - | 5351 | `	UwrapOpen4,UwrapOpen5,UwrapOpen6,UwrapOpen7` |
|      - | 5352 | `};` |
|      - | 5353 | `/*` |
|      - | 5354 | ` * bool stream_wrapper_register(string $protocol, string $class, int $flags = 0)` |
|      - | 5355 | ` * bool stream_wrapper_unregister(string $protocol)` |
|      - | 5356 | ` */` |
|      2 | 5357 | `static int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5358 | `{` |
|      - | 5359 | `	const char *zScheme,*zClass;` |
|      3 | 5360 | `	int nScheme,nClass,i,iFree = -1;` |
|      3 | 5361 | `	if( nArg < 2 ){` |
|    ! 0 | 5362 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5363 | `		return PH7_OK;` |
|      - | 5364 | `	}` |
|      3 | 5365 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 5366 | `	zClass  = ph7_value_to_string(apArg[1],&nClass);` |
|      2 | 5367 | `	if( nScheme < 1 \|\| nScheme >= (int)sizeof(g_aUwrap[0].zScheme)` |
|      3 | 5368 | `	 \|\| nClass < 1 \|\| nClass >= (int)sizeof(g_aUwrap[0].zClass) ){` |
|    ! 0 | 5369 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5370 | `		return PH7_OK;` |
|      - | 5371 | `	}` |
|      - | 5372 | `	/* php: registering an already-taken protocol warns and returns false.` |
|      - | 5373 | `	 * (Scan the device list directly — PH7_VmGetStreamDevice falls back to` |
|      - | 5374 | `	 * the DEFAULT device for a scheme-less name, so it can't answer this.) */` |
|      - | 5375 | `	{` |
|      3 | 5376 | `		ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|      - | 5377 | `		sxu32 n;` |
|     11 | 5378 | `		for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|      8 | 5379 | `			if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|      7 | 5380 | `			 && SyStrnicmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0 ){` |
|    ! 0 | 5381 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5382 | `					"Protocol %.*s:// is already defined.",nScheme,zScheme);` |
|    ! 0 | 5383 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 5384 | `				return PH7_OK;` |
|      - | 5385 | `			}` |
|      5 | 5386 | `		}` |
|      - | 5387 | `	}` |
|      3 | 5388 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      3 | 5389 | `		if( g_aUwrap[i].pVm == 0 ){` |
|      3 | 5390 | `			iFree = i;` |
|      3 | 5391 | `			break;` |
|      - | 5392 | `		}` |
|    ! 0 | 5393 | `	}` |
|      3 | 5394 | `	if( iFree < 0 ){` |
|    ! 0 | 5395 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5396 | `			"Too many registered stream wrappers (PHL limit: %d)",PHL_UWRAP_MAX);` |
|    ! 0 | 5397 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5398 | `		return PH7_OK;` |
|      - | 5399 | `	}` |
|      - | 5400 | `	{` |
|      3 | 5401 | `		uwrap_slot *pSlot = &g_aUwrap[iFree];` |
|      3 | 5402 | `		SyMemcpy(zScheme,pSlot->zScheme,(sxu32)nScheme);` |
|      3 | 5403 | `		pSlot->zScheme[nScheme] = 0;` |
|      3 | 5404 | `		SyMemcpy(zClass,pSlot->zClass,(sxu32)nClass);` |
|      3 | 5405 | `		pSlot->zClass[nClass] = 0;` |
|      3 | 5406 | `		pSlot->pVm = pCtx->pVm;` |
|      3 | 5407 | `		SyZero(&pSlot->sStream,sizeof(ph7_io_stream));` |
|      3 | 5408 | `		pSlot->sStream.zName = pSlot->zScheme;` |
|      3 | 5409 | `		pSlot->sStream.iVersion = PH7_IO_STREAM_VERSION;` |
|      3 | 5410 | `		pSlot->sStream.xOpen = g_aUwrapOpen[iFree];` |
|      3 | 5411 | `		pSlot->sStream.xClose = UwrapClose;` |
|      3 | 5412 | `		pSlot->sStream.xRead = UwrapRead;` |
|      3 | 5413 | `		pSlot->sStream.xWrite = UwrapWrite;` |
|      3 | 5414 | `		pSlot->sStream.xSeek = UwrapSeek;` |
|      3 | 5415 | `		pSlot->sStream.xTell = UwrapTell;` |
|      3 | 5416 | `		ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_IO_STREAM,&pSlot->sStream);` |
|      - | 5417 | `	}` |
|      3 | 5418 | `	ph7_result_bool(pCtx,1);` |
|      3 | 5419 | `	return PH7_OK;` |
|      2 | 5420 | `}` |
|      2 | 5421 | `static int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5422 | `{` |
|      - | 5423 | `	const char *zScheme;` |
|      - | 5424 | `	int nScheme,i;` |
|      3 | 5425 | `	if( nArg < 1 ){` |
|    ! 0 | 5426 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5427 | `		return PH7_OK;` |
|      - | 5428 | `	}` |
|      3 | 5429 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 5430 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      2 | 5431 | `		if( g_aUwrap[i].pVm == pCtx->pVm` |
|      2 | 5432 | `		 && (int)SyStrlen(g_aUwrap[i].zScheme) == nScheme` |
|      3 | 5433 | `		 && SyMemcmp(g_aUwrap[i].zScheme,zScheme,(sxu32)nScheme) == 0 ){` |
|      - | 5434 | `			/* The device stays in the VM's list (the engine has no removal` |
|      - | 5435 | `			 * API); neutering the slot makes every later open fail, which is` |
|      - | 5436 | `			 * what unregister means to a script — recorded. */` |
|      3 | 5437 | `			g_aUwrap[i].pVm = 0;` |
|      3 | 5438 | `			ph7_result_bool(pCtx,1);` |
|      3 | 5439 | `			return PH7_OK;` |
|      - | 5440 | `		}` |
|    ! 0 | 5441 | `	}` |
|    ! 0 | 5442 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 | 5443 | `	return PH7_OK;` |
|      2 | 5444 | `}` |
|      - | 5445 | `#ifdef PH7_ENABLE_NET` |
|      - | 5446 | `/*` |
|      - | 5447 | ` * resource\|false fsockopen(string $hostname, int $port = -1, int &$error_code,` |
|      - | 5448 | ` *                          string &$error_message, ?float $timeout = null)` |
|      - | 5449 | ` * resource\|false stream_socket_client(string $address, int &$error_code,` |
|      - | 5450 | ` *                          string &$error_message, ?float $timeout = null, ...)` |
|      - | 5451 | ` * TCP only (the recorded scope: no ssl://, udp:// or unix:// yet).` |
|      - | 5452 | ` */` |
|      6 | 5453 | `static int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 5454 | `{` |
|      6 | 5455 | `	const char *zFunc = ph7_function_name(pCtx);` |
|      6 | 5456 | `	int bClientForm = (zFunc[0] == 's'); /* stream_socket_client */` |
|      6 | 5457 | `	const char *zTarget,*zErr = "";` |
|      - | 5458 | `	char zHost[256];` |
|      6 | 5459 | `	int nTarget,iPort = -1,iErrno = 0,iTimeoutMs = 0;` |
|      - | 5460 | `	ph7_socket sock;` |
|      - | 5461 | `	io_private *pDev;` |
|      - | 5462 | `	sock_private *pSock;` |
|      6 | 5463 | `	int iArgErrno = bClientForm ? 1 : 2;` |
|      6 | 5464 | `	int iArgErrstr = bClientForm ? 2 : 3;` |
|      6 | 5465 | `	int iArgTimeout = bClientForm ? 3 : 4;` |
|      6 | 5466 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    ! 0 | 5467 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5468 | `		return PH7_OK;` |
|      - | 5469 | `	}` |
|      6 | 5470 | `	zTarget = ph7_value_to_string(apArg[0],&nTarget);` |
|      - | 5471 | `	/* Strip a scheme; only tcp:// (and the bare form) are supported */` |
|      - | 5472 | `	{` |
|      6 | 5473 | `		const char *z = zTarget,*zEnd = &zTarget[nTarget];` |
|      6 | 5474 | `		const char *zSep = 0;` |
|     32 | 5475 | `		while( z < zEnd - 2 ){` |
|     30 | 5476 | `			if( z[0] == ':' && z[1] == '/' && z[2] == '/' ){` |
|      4 | 5477 | `				zSep = z;` |
|      4 | 5478 | `				break;` |
|      - | 5479 | `			}` |
|     26 | 5480 | `			z++;` |
|    ! 0 | 5481 | `		}` |
|      6 | 5482 | `		if( zSep ){` |
|      4 | 5483 | `			if( !((zSep - zTarget) == 3 && SyStrnicmp(zTarget,"tcp",3) == 0) ){` |
|    ! 0 | 5484 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5485 | `					"Unable to connect to %.*s (unsupported transport; PHL supports tcp:// only)",` |
|    ! 0 | 5486 | `					nTarget,zTarget);` |
|    ! 0 | 5487 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 5488 | `				return PH7_OK;` |
|      - | 5489 | `			}` |
|      4 | 5490 | `			nTarget -= (int)(zSep + 3 - zTarget);` |
|      4 | 5491 | `			zTarget = zSep + 3;` |
|      2 | 5492 | `		}` |
|      - | 5493 | `	}` |
|      - | 5494 | `	/* host[:port] */` |
|      - | 5495 | `	{` |
|      6 | 5496 | `		int i = nTarget - 1;` |
|      6 | 5497 | `		int nHost = nTarget;` |
|     48 | 5498 | `		while( i > 0 && zTarget[i] != ':' ){` |
|     42 | 5499 | `			i--;` |
|    ! 0 | 5500 | `		}` |
|      6 | 5501 | `		if( i > 0 && zTarget[i] == ':' ){` |
|      2 | 5502 | `			sxi32 iTmp = 0;` |
|      2 | 5503 | `			SyStrToInt32(&zTarget[i+1],(sxu32)(nTarget - i - 1),(void *)&iTmp,0);` |
|      2 | 5504 | `			iPort = (int)iTmp;` |
|      2 | 5505 | `			nHost = i;` |
|      1 | 5506 | `		}` |
|      6 | 5507 | `		if( nHost >= (int)sizeof(zHost) ){` |
|    ! 0 | 5508 | `			nHost = (int)sizeof(zHost) - 1;` |
|    ! 0 | 5509 | `		}` |
|      6 | 5510 | `		SyMemcpy(zTarget,zHost,(sxu32)nHost);` |
|      6 | 5511 | `		zHost[nHost] = 0;` |
|      - | 5512 | `	}` |
|      6 | 5513 | `	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      4 | 5514 | `		iPort = ph7_value_to_int(apArg[1]);` |
|      2 | 5515 | `	}` |
|      6 | 5516 | `	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){` |
|      6 | 5517 | `		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);` |
|      6 | 5518 | `		if( rTimeout > 0 ){` |
|      6 | 5519 | `			iTimeoutMs = (int)(rTimeout * 1000);` |
|      3 | 5520 | `		}` |
|      3 | 5521 | `	}` |
|      6 | 5522 | `	if( iPort < 0 ){` |
|    ! 0 | 5523 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5524 | `		return PH7_OK;` |
|      - | 5525 | `	}` |
|      6 | 5526 | `	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,&iErrno,&zErr);` |
|      6 | 5527 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 5528 | `		/* php reports the failure through the by-ref out-params + a warning */` |
|      - | 5529 | `		{` |
|      2 | 5530 | `			ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      2 | 5531 | `			if( pTmp ){` |
|      2 | 5532 | `				if( nArg > iArgErrno ){` |
|      2 | 5533 | `					ph7_value_int(pTmp,iErrno);` |
|      2 | 5534 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      1 | 5535 | `				}` |
|      2 | 5536 | `				if( nArg > iArgErrstr ){` |
|      2 | 5537 | `					ph7_value_string(pTmp,zErr,-1);` |
|      2 | 5538 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      1 | 5539 | `				}` |
|      1 | 5540 | `			}` |
|      - | 5541 | `		}` |
|      - | 5542 | `		/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */` |
|      3 | 5543 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      1 | 5544 | `			"Unable to connect to %s:%d (%s)",zHost,iPort,zErr);` |
|      2 | 5545 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5546 | `		return PH7_OK;` |
|      - | 5547 | `	}` |
|      - | 5548 | `	{` |
|      4 | 5549 | `		ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      4 | 5550 | `		if( pTmp ){` |
|      4 | 5551 | `			if( nArg > iArgErrno ){` |
|      4 | 5552 | `				ph7_value_int(pTmp,0);` |
|      4 | 5553 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      2 | 5554 | `			}` |
|      4 | 5555 | `			if( nArg > iArgErrstr ){` |
|      4 | 5556 | `				ph7_value_string(pTmp,"",0);` |
|      4 | 5557 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      2 | 5558 | `			}` |
|      2 | 5559 | `		}` |
|      - | 5560 | `	}` |
|      - | 5561 | `	/* Wrap the socket in an io_private so the whole f* family works on it */` |
|      4 | 5562 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      4 | 5563 | `	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));` |
|      4 | 5564 | `	if( pDev == 0 \|\| pSock == 0 ){` |
|    ! 0 | 5565 | `		PH7_NetClose(sock);` |
|    ! 0 | 5566 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5567 | `		return PH7_OK;` |
|      - | 5568 | `	}` |
|      4 | 5569 | `	pSock->pVm = pCtx->pVm;` |
|      4 | 5570 | `	pSock->sock = sock;` |
|      4 | 5571 | `	pSock->bEof = 0;` |
|      4 | 5572 | `	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);` |
|      4 | 5573 | `	pDev->pHandle = (void *)pSock;` |
|      4 | 5574 | `	ph7_result_resource(pCtx,pDev);` |
|      4 | 5575 | `	return PH7_OK;` |
|      3 | 5576 | `}` |
|      - | 5577 | `#endif /* PH7_ENABLE_NET */` |
|     80 | 5578 | `static int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5579 | `{` |
|      - | 5580 | `	const ph7_io_stream *pStream;` |
|      - | 5581 | `	const char *zUri,*zMode;` |
|      - | 5582 | `	ph7_value *pResource;` |
|      - | 5583 | `	io_private *pDev;` |
|      - | 5584 | `	int iLen,imLen;` |
|      - | 5585 | `	int iOpenFlags;` |
|     83 | 5586 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5587 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5588 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 5589 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5590 | `		return PH7_OK;` |
|      - | 5591 | `	}` |
|      - | 5592 | `	/* Extract the URI and the desired access mode */` |
|     83 | 5593 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|     83 | 5594 | `	if( nArg > 1 ){` |
|     83 | 5595 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|     43 | 5596 | `	}else{` |
|      - | 5597 | `		/* Set a default read-only mode */` |
|    ! 0 | 5598 | `		zMode = "r";` |
|    ! 0 | 5599 | `		imLen = (int)sizeof(char);` |
|      - | 5600 | `	}` |
|      - | 5601 | `	/* Try to extract a stream */` |
|     83 | 5602 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|     83 | 5603 | `	if( pStream == 0 ){` |
|    ! 0 | 5604 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5605 | `			"No stream device is associated with the given URI(%s)",zUri);` |
|    ! 0 | 5606 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5607 | `		return PH7_OK;` |
|      - | 5608 | `	}` |
|      - | 5609 | `	/* Allocate a new IO private instance */` |
|     83 | 5610 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     83 | 5611 | `	if( pDev == 0 ){` |
|    ! 0 | 5612 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5613 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5614 | `		return PH7_OK;` |
|      - | 5615 | `	}` |
|     83 | 5616 | `	pResource = 0;` |
|     83 | 5617 | `	if( nArg > 3 ){` |
|    ! 0 | 5618 | `		pResource = apArg[3];` |
|     83 | 5619 | `	}else if( is_php_stream(pStream) \|\| is_data_stream(pStream) ){` |
|      - | 5620 | `		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to` |
|      - | 5621 | `		 * access the underlying virtual machine.` |
|      - | 5622 | `		 */` |
|     15 | 5623 | `		pResource = apArg[0];` |
|      7 | 5624 | `	}` |
|      - | 5625 | `	/* Initialize the structure */` |
|     83 | 5626 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 5627 | `	/* Convert open mode to PH7 flags */` |
|     83 | 5628 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|      - | 5629 | `	/* Try to get a handle */` |
|    123 | 5630 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|     40 | 5631 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);` |
|     83 | 5632 | `	if( pDev->pHandle == 0 ){` |
|    ! 0 | 5633 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zUri);` |
|    ! 0 | 5634 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5635 | `		ph7_context_free_chunk(pCtx,pDev);` |
|    ! 0 | 5636 | `		return PH7_OK;` |
|      - | 5637 | `	}` |
|      - | 5638 | `	/* All done,return the io_private instance as a resource */` |
|     83 | 5639 | `	ph7_result_resource(pCtx,pDev);` |
|     83 | 5640 | `	return PH7_OK;` |
|     43 | 5641 | `}` |
|      - | 5642 | `/*` |
|      - | 5643 | ` * bool fclose(resource $handle)` |
|      - | 5644 | ` *  Closes an open file pointer` |
|      - | 5645 | ` * Parameters` |
|      - | 5646 | ` *  $handle` |
|      - | 5647 | ` *   The file pointer.` |
|      - | 5648 | ` * Return` |
|      - | 5649 | ` *  TRUE on success or FALSE on failure.` |
|      - | 5650 | ` */` |
|    178 | 5651 | `static int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5652 | `{` |
|      - | 5653 | `	const ph7_io_stream *pStream;` |
|      - | 5654 | `	io_private *pDev;` |
|      - | 5655 | `	ph7_vm *pVm;` |
|    183 | 5656 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5657 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5658 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5659 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5660 | `		return PH7_OK;` |
|      - | 5661 | `	}` |
|      - | 5662 | `	/* Extract our private data */` |
|    183 | 5663 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 5664 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    183 | 5665 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 5666 | `		/*Expecting an IO handle */` |
|    ! 0 | 5667 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5668 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5669 | `		return PH7_OK;` |
|      - | 5670 | `	}` |
|      - | 5671 | `	/* Point to the target IO stream device */` |
|    183 | 5672 | `	pStream = pDev->pStream;` |
|    183 | 5673 | `	if( pStream == 0 ){` |
|    ! 0 | 5674 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5675 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 5676 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 5677 | `			);` |
|    ! 0 | 5678 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5679 | `		return PH7_OK;` |
|      - | 5680 | `	}` |
|      - | 5681 | `	/* Point to the VM that own this context */` |
|    183 | 5682 | `	pVm = pCtx->pVm;` |
|      - | 5683 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|    183 | 5684 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 5685 | `		/* Perform the requested operation */` |
|    183 | 5686 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 5687 | `		/* Release the IO private structure */` |
|    183 | 5688 | `		ReleaseIOPrivate(pCtx,pDev);` |
|      - | 5689 | `		/* Invalidate the resource handle */` |
|    183 | 5690 | `		ph7_value_release(apArg[0]);` |
|     89 | 5691 | `	}` |
|      - | 5692 | `	/* Return TRUE */` |
|    183 | 5693 | `	ph7_result_bool(pCtx,1);` |
|    183 | 5694 | `	return PH7_OK;` |
|     94 | 5695 | `}` |
|      - | 5696 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5697 | `/*` |
|      - | 5698 | ` * MD5/SHA1 digest consumer.` |
|      - | 5699 | ` */` |
|     72 | 5700 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 5701 | `{` |
|      - | 5702 | `	/* Append hex chunk verbatim */` |
|     73 | 5703 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     73 | 5704 | `	return SXRET_OK;` |
|      1 | 5705 | `}` |
|      - | 5706 | `/*` |
|      - | 5707 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 5708 | ` *  Calculates the md5 hash of a given file.` |
|      - | 5709 | ` * Parameters` |
|      - | 5710 | ` *  $uri` |
|      - | 5711 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 5712 | ` *  $raw_output` |
|      - | 5713 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 5714 | ` * Return` |
|      - | 5715 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 5716 | ` */` |
|      2 | 5717 | `static int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5718 | `{` |
|      - | 5719 | `	const ph7_io_stream *pStream;` |
|      - | 5720 | `	unsigned char zDigest[16];` |
|      3 | 5721 | `	int raw_output  = FALSE;` |
|      - | 5722 | `	const char *zFile;` |
|      - | 5723 | `	MD5Context sCtx;` |
|      - | 5724 | `	char zBuf[8192];` |
|      - | 5725 | `	void *pHandle;` |
|      - | 5726 | `	ph7_int64 n;` |
|      - | 5727 | `	int nLen;` |
|      3 | 5728 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5729 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5730 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5731 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5732 | `		return PH7_OK;` |
|      - | 5733 | `	}` |
|      - | 5734 | `	/* Extract the file path */` |
|      3 | 5735 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5736 | `	/* Point to the target IO stream device */` |
|      3 | 5737 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5738 | `	if( pStream == 0 ){` |
|    ! 0 | 5739 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5740 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5741 | `		return PH7_OK;` |
|      - | 5742 | `	}` |
|      3 | 5743 | `	if( nArg > 1 ){` |
|    ! 0 | 5744 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 5745 | `	}` |
|      - | 5746 | `	/* Try to open the file in read-only mode */` |
|      3 | 5747 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5748 | `	if( pHandle == 0 ){` |
|    ! 0 | 5749 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5750 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5751 | `		return PH7_OK;` |
|      - | 5752 | `	}` |
|      - | 5753 | `	/* Init the MD5 context */` |
|      3 | 5754 | `	MD5Init(&sCtx);` |
|      - | 5755 | `	/* Perform the requested operation */` |
|      2 | 5756 | `	for(;;){` |
|      5 | 5757 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5758 | `		if( n < 1 ){` |
|      - | 5759 | `			/* EOF or IO error,break immediately */` |
|      3 | 5760 | `			break;` |
|      - | 5761 | `		}` |
|      3 | 5762 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5763 | `	}` |
|      - | 5764 | `	/* Close the stream */` |
|      3 | 5765 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5766 | `	/* Extract the digest */` |
|      3 | 5767 | `	MD5Final(zDigest,&sCtx);` |
|      3 | 5768 | `	if( raw_output ){` |
|      - | 5769 | `		/* Output raw digest */` |
|    ! 0 | 5770 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5771 | `	}else{` |
|      - | 5772 | `		/* Perform a binary to hex conversion */` |
|      3 | 5773 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5774 | `	}` |
|      3 | 5775 | `	return PH7_OK;` |
|      2 | 5776 | `}` |
|      - | 5777 | `/*` |
|      - | 5778 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 5779 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 5780 | ` * Parameters` |
|      - | 5781 | ` *  $uri` |
|      - | 5782 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 5783 | ` *  $raw_output` |
|      - | 5784 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 5785 | ` * Return` |
|      - | 5786 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 5787 | ` */` |
|      2 | 5788 | `static int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5789 | `{` |
|      - | 5790 | `	const ph7_io_stream *pStream;` |
|      - | 5791 | `	unsigned char zDigest[20];` |
|      3 | 5792 | `	int raw_output  = FALSE;` |
|      - | 5793 | `	const char *zFile;` |
|      - | 5794 | `	SHA1Context sCtx;` |
|      - | 5795 | `	char zBuf[8192];` |
|      - | 5796 | `	void *pHandle;` |
|      - | 5797 | `	ph7_int64 n;` |
|      - | 5798 | `	int nLen;` |
|      3 | 5799 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5800 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5801 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5802 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5803 | `		return PH7_OK;` |
|      - | 5804 | `	}` |
|      - | 5805 | `	/* Extract the file path */` |
|      3 | 5806 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5807 | `	/* Point to the target IO stream device */` |
|      3 | 5808 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5809 | `	if( pStream == 0 ){` |
|    ! 0 | 5810 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5811 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5812 | `		return PH7_OK;` |
|      - | 5813 | `	}` |
|      3 | 5814 | `	if( nArg > 1 ){` |
|    ! 0 | 5815 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 5816 | `	}` |
|      - | 5817 | `	/* Try to open the file in read-only mode */` |
|      3 | 5818 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5819 | `	if( pHandle == 0 ){` |
|    ! 0 | 5820 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5821 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5822 | `		return PH7_OK;` |
|      - | 5823 | `	}` |
|      - | 5824 | `	/* Init the SHA1 context */` |
|      3 | 5825 | `	SHA1Init(&sCtx);` |
|      - | 5826 | `	/* Perform the requested operation */` |
|      2 | 5827 | `	for(;;){` |
|      5 | 5828 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5829 | `		if( n < 1 ){` |
|      - | 5830 | `			/* EOF or IO error,break immediately */` |
|      3 | 5831 | `			break;` |
|      - | 5832 | `		}` |
|      3 | 5833 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5834 | `	}` |
|      - | 5835 | `	/* Close the stream */` |
|      3 | 5836 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5837 | `	/* Extract the digest */` |
|      3 | 5838 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 5839 | `	if( raw_output ){` |
|      - | 5840 | `		/* Output raw digest */` |
|    ! 0 | 5841 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5842 | `	}else{` |
|      - | 5843 | `		/* Perform a binary to hex conversion */` |
|      3 | 5844 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5845 | `	}` |
|      3 | 5846 | `	return PH7_OK;` |
|      2 | 5847 | `}` |
|      - | 5848 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5849 | `/*` |
|      - | 5850 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 5851 | ` *  Parse a configuration file.` |
|      - | 5852 | ` * Parameters` |
|      - | 5853 | ` * $filename` |
|      - | 5854 | ` *  The filename of the ini file being parsed.` |
|      - | 5855 | ` * $process_sections` |
|      - | 5856 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 5857 | ` *  with the section names and settings included.` |
|      - | 5858 | ` *  The default for process_sections is FALSE.` |
|      - | 5859 | ` * $scanner_mode` |
|      - | 5860 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 5861 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 5862 | ` * Return` |
|      - | 5863 | ` *  The settings are returned as an associative array on success.` |
|      - | 5864 | ` *  Otherwise is returned.` |
|      - | 5865 | ` */` |
|      2 | 5866 | `static int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5867 | `{` |
|      - | 5868 | `	const ph7_io_stream *pStream;` |
|      - | 5869 | `	const char *zFile;` |
|      - | 5870 | `	SyBlob sContents;` |
|      - | 5871 | `	void *pHandle;` |
|      - | 5872 | `	int nLen;` |
|      3 | 5873 | `	sxi32 rc = PH7_OK;` |
|      3 | 5874 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5875 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5876 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5877 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5878 | `		return PH7_OK;` |
|      - | 5879 | `	}` |
|      - | 5880 | `	/* Extract the file path */` |
|      3 | 5881 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5882 | `	/* Point to the target IO stream device */` |
|      3 | 5883 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5884 | `	if( pStream == 0 ){` |
|    ! 0 | 5885 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5886 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5887 | `		return PH7_OK;` |
|      - | 5888 | `	}` |
|      - | 5889 | `	/* Try to open the file in read-only mode */` |
|      3 | 5890 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5891 | `	if( pHandle == 0 ){` |
|    ! 0 | 5892 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5893 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5894 | `		return PH7_OK;` |
|      - | 5895 | `	}` |
|      3 | 5896 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 5897 | `	/* Read the whole file */` |
|      3 | 5898 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      3 | 5899 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 5900 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 5901 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5902 | `	}else{` |
|      - | 5903 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|      5 | 5904 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      2 | 5905 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0);` |
|      - | 5906 | `	}` |
|      - | 5907 | `	/* Close the stream */` |
|      3 | 5908 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5909 | `	/* Release the working buffer */` |
|      3 | 5910 | `	SyBlobRelease(&sContents);` |
|      - | 5911 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      3 | 5912 | `	return rc;` |
|      2 | 5913 | `}` |
|      - | 5914 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 5915 | `#else /* PH7_DISABLE_DISK_IO */` |
|      - | 5916 | `/*` |
|      - | 5917 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|      - | 5918 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|      - | 5919 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|      - | 5920 | ` * to any non-VFS resource).` |
|      - | 5921 | ` */` |
|      - | 5922 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      - | 5923 | `{` |
|      - | 5924 | `	SXUNUSED(pResource);` |
|      - | 5925 | `	return "Unknown";` |
|      - | 5926 | `}` |
|      - | 5927 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 5928 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 5929 | `#if defined(_MSC_VER)` |
|      - | 5930 | `static const ph7_vfs null_vfs = {` |
|      - | 5931 | `#else` |
|      - | 5932 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 5933 | `#endif` |
|      - | 5934 | `	"null_vfs",` |
|      - | 5935 | `	PH7_VFS_VERSION,` |
|      - | 5936 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 5937 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 5938 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 5939 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 5940 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 5941 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 5942 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 5943 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 5944 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 5945 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 5946 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 5947 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 5948 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 5949 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 5950 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 5951 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 5952 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 5953 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 5954 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 5955 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 5956 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 5957 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 5958 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 5959 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 5960 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 5961 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 5962 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 5963 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 5964 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 5965 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 5966 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 5967 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 5968 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 5969 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 5970 | `	0, /* int (*xUmask)(int) */` |
|      - | 5971 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 5972 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 5973 | `	0, /* int (*xUid)(void) */` |
|      - | 5974 | `	0, /* int (*xGid)(void) */` |
|      - | 5975 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 5976 | `	0  /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 5977 | `};` |
|      - | 5978 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 5979 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 5980 | `/*` |
|      - | 5981 | ` * Export the builtin vfs.` |
|      - | 5982 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 5983 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 5984 | ` * Note:` |
|      - | 5985 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 5986 | ` * Note:` |
|      - | 5987 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 5988 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 5989 | ` */` |
|   3978 | 5990 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 5991 | `{` |
|      - | 5992 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 5993 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 5994 | `	return &null_vfs;` |
|      - | 5995 | `#else` |
|      - | 5996 | `#ifdef __WINNT__` |
|      5 | 5997 | `	return &sWinVfs;` |
|      - | 5998 | `#elif defined(__UNIXES__)` |
|   3978 | 5999 | `	return &sUnixVfs;` |
|      - | 6000 | `#else` |
|      - | 6001 | `	return &null_vfs;` |
|      - | 6002 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 6003 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 6004 | `#else` |
|      - | 6005 | `	return &null_vfs;` |
|      - | 6006 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 6007 | `}` |
|      - | 6008 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 6009 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 6010 | `/*` |
|      - | 6011 | ` * The following defines are mostly used by the UNIX built and have` |
|      - | 6012 | ` * no particular meaning on windows.` |
|      - | 6013 | ` */` |
|      - | 6014 | `#ifndef STDIN_FILENO` |
|      - | 6015 | `#define STDIN_FILENO	0` |
|      - | 6016 | `#endif` |
|      - | 6017 | `#ifndef STDOUT_FILENO` |
|      - | 6018 | `#define STDOUT_FILENO	1` |
|      - | 6019 | `#endif` |
|      - | 6020 | `#ifndef STDERR_FILENO` |
|      - | 6021 | `#define STDERR_FILENO	2` |
|      - | 6022 | `#endif` |
|      - | 6023 | `/*` |
|      - | 6024 | ` * php:// Accessing various I/O streams` |
|      - | 6025 | ` * According to the PHP langage reference manual` |
|      - | 6026 | ` * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input` |
|      - | 6027 | ` * and output streams, the standard input, output and error file descriptors.` |
|      - | 6028 | ` * php://stdin, php://stdout and php://stderr:` |
|      - | 6029 | ` *  Allow direct access to the corresponding input or output stream of the PHP process.` |
|      - | 6030 | ` *  The stream references a duplicate file descriptor, so if you open php://stdin and later` |
|      - | 6031 | ` *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.` |
|      - | 6032 | ` *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.` |
|      - | 6033 | ` * php://output` |
|      - | 6034 | ` *  php://output is a write-only stream that allows you to write to the output buffer` |
|      - | 6035 | ` *  mechanism in the same way as print and echo.` |
|      - | 6036 | ` */` |
|      - | 6037 | `typedef struct ph7_stream_data ph7_stream_data;` |
|      - | 6038 | `/* Supported IO streams */` |
|      - | 6039 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|      - | 6040 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|      - | 6041 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|      - | 6042 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|      - | 6043 | `#define PH7_IO_STREAM_MEMORY 5 /* php://memory, php://temp, and data:// payloads */` |
|      - | 6044 | ` /* The following structure is the private data associated with the php:// stream */` |
|      - | 6045 | `struct ph7_stream_data` |
|      - | 6046 | `{` |
|      - | 6047 | `	ph7_vm *pVm; /* VM that own this instance */` |
|      - | 6048 | `	int iType;   /* Stream type */` |
|      - | 6049 | `	union{` |
|      - | 6050 | `		void *pHandle; /* Stream handle */` |
|      - | 6051 | `		ph7_output_consumer sConsumer; /* VM output consumer */` |
|      - | 6052 | `	}x;` |
|      - | 6053 | `	SyBlob sMem;     /* MEMORY type: backing buffer */` |
|      - | 6054 | `	sxu32 nCur;      /* MEMORY type: read/write cursor */` |
|      - | 6055 | `	int bReadOnly;   /* MEMORY type: TRUE for data:// payloads */` |
|      - | 6056 | `};` |
|      - | 6057 | `/*` |
|      - | 6058 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|      - | 6059 | ` */` |
|     26 | 6060 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|      1 | 6061 | `{` |
|      - | 6062 | `	ph7_stream_data *pData;` |
|     27 | 6063 | `	if( pVm == 0 ){` |
|    ! 0 | 6064 | `		return 0;` |
|      - | 6065 | `	}` |
|      - | 6066 | `	/* Allocate a new instance */` |
|     27 | 6067 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|     27 | 6068 | `	if( pData == 0 ){` |
|    ! 0 | 6069 | `		return 0;` |
|      - | 6070 | `	}` |
|      - | 6071 | `	/* Zero the structure */` |
|     27 | 6072 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|      - | 6073 | `	/* Initialize fields */` |
|     27 | 6074 | `	pData->iType = iType;` |
|     27 | 6075 | `	SyBlobInit(&pData->sMem,&pVm->sAllocator);` |
|     27 | 6076 | `	pData->nCur = 0;` |
|     27 | 6077 | `	pData->bReadOnly = 0;` |
|     27 | 6078 | `	if( iType == PH7_IO_STREAM_MEMORY ){` |
|      - | 6079 | `		/* Nothing else to set up: the buffer is the stream */` |
|     18 | 6080 | `	}else if( iType == PH7_IO_STREAM_OUTPUT ){` |
|      - | 6081 | `		/* Point to the default VM consumer routine. */` |
|      3 | 6082 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|      2 | 6083 | `	}else{` |
|      - | 6084 | `#ifdef __WINNT__` |
|      - | 6085 | `		DWORD nChannel;` |
|      1 | 6086 | `		switch(iType){` |
|      1 | 6087 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|      1 | 6088 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|      - | 6089 | `		default:` |
|      1 | 6090 | `			nChannel = STD_INPUT_HANDLE;` |
|      - | 6091 | `			break;` |
|      - | 6092 | `		}` |
|      1 | 6093 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|      - | 6094 | `#else` |
|      - | 6095 | `		/* Assume an UNIX system */` |
|      6 | 6096 | `		int ifd = STDIN_FILENO;` |
|      6 | 6097 | `		switch(iType){` |
|      2 | 6098 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|      2 | 6099 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|      1 | 6100 | `		default:` |
|      2 | 6101 | `			break;` |
|      - | 6102 | `		}` |
|      6 | 6103 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|      - | 6104 | `#endif` |
|      - | 6105 | `	}` |
|     27 | 6106 | `	pData->pVm = pVm;` |
|     27 | 6107 | `	return pData;` |
|     14 | 6108 | `}` |
|      - | 6109 | `/*` |
|      - | 6110 | ` * Implementation of the php:// IO streams routines` |
|      - | 6111 | ` * Status:` |
|      - | 6112 | ` *   Stable.` |
|      - | 6113 | ` */` |
|      - | 6114 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|     10 | 6115 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 6116 | `{` |
|      - | 6117 | `	ph7_stream_data *pData;` |
|      - | 6118 | `	SyString sStream;` |
|     11 | 6119 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|      - | 6120 | `	/* Trim leading and trailing white spaces */` |
|     11 | 6121 | `	SyStringFullTrim(&sStream);` |
|      - | 6122 | `	/* Stream to open */` |
|     11 | 6123 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|    ! 0 | 6124 | `		iMode = PH7_IO_STREAM_STDIN;` |
|     11 | 6125 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|      3 | 6126 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|     10 | 6127 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|    ! 0 | 6128 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|      9 | 6129 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|    ! 0 | 6130 | `		iMode = PH7_IO_STREAM_STDERR;` |
|      8 | 6131 | `	}else if( SyStrnicmp(sStream.zString,"memory",sizeof("memory")-1) == 0` |
|      6 | 6132 | `	       \|\| SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0 ){` |
|      - | 6133 | `		/* php://memory and php://temp (PHL keeps temp fully in memory —` |
|      - | 6134 | `		 * php's 2MB disk spill is a memory-pressure detail, recorded) */` |
|      9 | 6135 | `		iMode = PH7_IO_STREAM_MEMORY;` |
|      5 | 6136 | `	}else{` |
|      - | 6137 | `		/* unknown stream name */` |
|    ! 0 | 6138 | `		return -1;` |
|      - | 6139 | `	}` |
|      - | 6140 | `	/* Create our handle */` |
|     11 | 6141 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|     11 | 6142 | `	if( pData == 0 ){` |
|    ! 0 | 6143 | `		return -1;` |
|      - | 6144 | `	}` |
|      - | 6145 | `	/* Make the handle public */` |
|     11 | 6146 | `	*ppHandle = (void *)pData;` |
|     11 | 6147 | `	return PH7_OK;` |
|      6 | 6148 | `}` |
|      - | 6149 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|     42 | 6150 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|      1 | 6151 | `{` |
|     43 | 6152 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     43 | 6153 | `	if( pData == 0 ){` |
|    ! 0 | 6154 | `		return -1;` |
|      - | 6155 | `	}` |
|     43 | 6156 | `	if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|     43 | 6157 | `		sxu32 nAvail = SyBlobLength(&pData->sMem);` |
|      - | 6158 | `		sxu32 nRead;` |
|     43 | 6159 | `		if( pData->nCur >= nAvail ){` |
|     15 | 6160 | `			return 0; /* EOF */` |
|      - | 6161 | `		}` |
|     29 | 6162 | `		nRead = nAvail - pData->nCur;` |
|     29 | 6163 | `		if( (ph7_int64)nRead > nDatatoRead ){` |
|      7 | 6164 | `			nRead = (sxu32)nDatatoRead;` |
|      3 | 6165 | `		}` |
|     29 | 6166 | `		SyMemcpy((const char *)SyBlobData(&pData->sMem) + pData->nCur,pBuffer,nRead);` |
|     29 | 6167 | `		pData->nCur += nRead;` |
|     29 | 6168 | `		return (ph7_int64)nRead;` |
|      - | 6169 | `	}` |
|    ! 0 | 6170 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|      - | 6171 | `		/* Forbidden */` |
|    ! 0 | 6172 | `		return -1;` |
|      - | 6173 | `	}` |
|      - | 6174 | `#ifdef __WINNT__` |
|      - | 6175 | `	{` |
|      - | 6176 | `		DWORD nRd;` |
|      - | 6177 | `		BOOL rc;` |
|    ! 0 | 6178 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    ! 0 | 6179 | `		if( !rc ){` |
|      - | 6180 | `			/* IO error */` |
|    ! 0 | 6181 | `			return -1;` |
|      - | 6182 | `		}` |
|    ! 0 | 6183 | `		return (ph7_int64)nRd;` |
|      - | 6184 | `	}` |
|      - | 6185 | `#elif defined(__UNIXES__)` |
|      - | 6186 | `	{` |
|      - | 6187 | `		ssize_t nRd;` |
|      - | 6188 | `		int fd;` |
|    ! 0 | 6189 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 6190 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|    ! 0 | 6191 | `		if( nRd < 1 ){` |
|    ! 0 | 6192 | `			return -1;` |
|      - | 6193 | `		}` |
|    ! 0 | 6194 | `		return (ph7_int64)nRd;` |
|      - | 6195 | `	}` |
|      - | 6196 | `#else` |
|      - | 6197 | `	return -1;` |
|      - | 6198 | `#endif` |
|     22 | 6199 | `}` |
|      - | 6200 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|     12 | 6201 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 6202 | `{` |
|     13 | 6203 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     13 | 6204 | `	if( pData == 0 ){` |
|    ! 0 | 6205 | `		return -1;` |
|      - | 6206 | `	}` |
|     13 | 6207 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|      - | 6208 | `		/* Forbidden */` |
|    ! 0 | 6209 | `		return -1;` |
|     13 | 6210 | `	}else if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|      - | 6211 | `		sxu32 nLen,nEnd;` |
|     11 | 6212 | `		if( pData->bReadOnly ){` |
|    ! 0 | 6213 | `			return -1;` |
|      - | 6214 | `		}` |
|     11 | 6215 | `		nLen = SyBlobLength(&pData->sMem);` |
|     11 | 6216 | `		if( pData->nCur > nLen ){` |
|      - | 6217 | `			/* seek past end: php zero-fills the gap */` |
|      - | 6218 | `			static const char zZero[64] = {0};` |
|    ! 0 | 6219 | `			while( SyBlobLength(&pData->sMem) < pData->nCur ){` |
|    ! 0 | 6220 | `				sxu32 nPad = pData->nCur - SyBlobLength(&pData->sMem);` |
|    ! 0 | 6221 | `				if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|    ! 0 | 6222 | `				if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|    ! 0 | 6223 | `					return -1;` |
|      - | 6224 | `				}` |
|    ! 0 | 6225 | `			}` |
|    ! 0 | 6226 | `			nLen = SyBlobLength(&pData->sMem);` |
|    ! 0 | 6227 | `		}` |
|     11 | 6228 | `		nEnd = pData->nCur + (sxu32)nWrite;` |
|     11 | 6229 | `		if( pData->nCur < nLen ){` |
|      - | 6230 | `			/* overwrite in place up to the current end */` |
|      3 | 6231 | `			sxu32 nOver = nLen - pData->nCur;` |
|      3 | 6232 | `			if( nOver > (sxu32)nWrite ){ nOver = (sxu32)nWrite; }` |
|      3 | 6233 | `			SyMemcpy(pBuf,(char *)SyBlobData(&pData->sMem) + pData->nCur,nOver);` |
|      3 | 6234 | `			if( nEnd > nLen ){` |
|    ! 0 | 6235 | `				if( SyBlobAppend(&pData->sMem,(const char *)pBuf + nOver,nEnd - nLen) != SXRET_OK ){` |
|    ! 0 | 6236 | `					return -1;` |
|      - | 6237 | `				}` |
|    ! 0 | 6238 | `			}` |
|      2 | 6239 | `		}else{` |
|      9 | 6240 | `			if( SyBlobAppend(&pData->sMem,pBuf,(sxu32)nWrite) != SXRET_OK ){` |
|    ! 0 | 6241 | `				return -1;` |
|      - | 6242 | `			}` |
|      - | 6243 | `		}` |
|     11 | 6244 | `		pData->nCur = nEnd;` |
|     11 | 6245 | `		return nWrite;` |
|      3 | 6246 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|      3 | 6247 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|      - | 6248 | `		int rc;` |
|      - | 6249 | `		/* Call the vm output consumer */` |
|      3 | 6250 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|      3 | 6251 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 6252 | `			return -1;` |
|      - | 6253 | `		}` |
|      3 | 6254 | `		return nWrite;` |
|      - | 6255 | `	}` |
|      - | 6256 | `#ifdef __WINNT__` |
|      - | 6257 | `	{` |
|      - | 6258 | `		DWORD nWr;` |
|      - | 6259 | `		BOOL rc;` |
|    ! 0 | 6260 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|    ! 0 | 6261 | `		if( !rc ){` |
|      - | 6262 | `			/* IO error */` |
|    ! 0 | 6263 | `			return -1;` |
|      - | 6264 | `		}` |
|    ! 0 | 6265 | `		return (ph7_int64)nWr;` |
|      - | 6266 | `	}` |
|      - | 6267 | `#elif defined(__UNIXES__)` |
|      - | 6268 | `	{` |
|      - | 6269 | `		ssize_t nWr;` |
|      - | 6270 | `		int fd;` |
|    ! 0 | 6271 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 6272 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|    ! 0 | 6273 | `		if( nWr < 1 ){` |
|    ! 0 | 6274 | `			return -1;` |
|      - | 6275 | `		}` |
|    ! 0 | 6276 | `		return (ph7_int64)nWr;` |
|      - | 6277 | `	}` |
|      - | 6278 | `#else` |
|      - | 6279 | `	return -1;` |
|      - | 6280 | `#endif` |
|      7 | 6281 | `}` |
|      - | 6282 | `/* void (*xClose)(void *) */` |
|     16 | 6283 | `static void PHPStreamData_Close(void *pHandle)` |
|      1 | 6284 | `{` |
|     17 | 6285 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 6286 | `	ph7_vm *pVm;` |
|     17 | 6287 | `	if( pData == 0 ){` |
|    ! 0 | 6288 | `		return;` |
|      - | 6289 | `	}` |
|     17 | 6290 | `	pVm = pData->pVm;` |
|     17 | 6291 | `	SyBlobRelease(&pData->sMem);` |
|      - | 6292 | `	/* Free the instance */` |
|     17 | 6293 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|      9 | 6294 | `}` |
|      - | 6295 | `/* int (*xSeek)(void *,ph7_int64,int); MEMORY type only */` |
|     20 | 6296 | `static int PHPStreamData_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 6297 | `{` |
|     21 | 6298 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 6299 | `	ph7_int64 iNew;` |
|     21 | 6300 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|    ! 0 | 6301 | `		return -1;` |
|      - | 6302 | `	}` |
|     21 | 6303 | `	switch(whence){` |
|    ! 0 | 6304 | `	case 1/*SEEK_CUR*/: iNew = (ph7_int64)pData->nCur + iOfft; break;` |
|      3 | 6305 | `	case 2/*SEEK_END*/: iNew = (ph7_int64)SyBlobLength(&pData->sMem) + iOfft; break;` |
|     19 | 6306 | `	default:            iNew = iOfft; break;` |
|      - | 6307 | `	}` |
|     21 | 6308 | `	if( iNew < 0 ){` |
|    ! 0 | 6309 | `		return -1;` |
|      - | 6310 | `	}` |
|     21 | 6311 | `	pData->nCur = (sxu32)iNew;` |
|     21 | 6312 | `	return PH7_OK;` |
|     11 | 6313 | `}` |
|      - | 6314 | `/* ph7_int64 (*xTell)(void *); MEMORY type only */` |
|      4 | 6315 | `static ph7_int64 PHPStreamData_Tell(void *pHandle)` |
|      1 | 6316 | `{` |
|      5 | 6317 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      5 | 6318 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|    ! 0 | 6319 | `		return -1;` |
|      - | 6320 | `	}` |
|      5 | 6321 | `	return (ph7_int64)pData->nCur;` |
|      3 | 6322 | `}` |
|      - | 6323 | `/* int (*xTrunc)(void *,ph7_int64); MEMORY type only */` |
|    ! 0 | 6324 | `static int PHPStreamData_Trunc(void *pHandle,ph7_int64 nLen)` |
|    ! 0 | 6325 | `{` |
|    ! 0 | 6326 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    ! 0 | 6327 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY \|\| pData->bReadOnly ){` |
|    ! 0 | 6328 | `		return -1;` |
|      - | 6329 | `	}` |
|    ! 0 | 6330 | `	if( nLen < (ph7_int64)SyBlobLength(&pData->sMem) ){` |
|      - | 6331 | `		/* shrink in place: the blob keeps its allocation */` |
|    ! 0 | 6332 | `		pData->sMem.nByte = (sxu32)nLen;` |
|    ! 0 | 6333 | `	}else{` |
|      - | 6334 | `		static const char zZero[64] = {0};` |
|    ! 0 | 6335 | `		while( (ph7_int64)SyBlobLength(&pData->sMem) < nLen ){` |
|    ! 0 | 6336 | `			sxu32 nPad = (sxu32)(nLen - SyBlobLength(&pData->sMem));` |
|    ! 0 | 6337 | `			if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|    ! 0 | 6338 | `			if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|    ! 0 | 6339 | `				return -1;` |
|      - | 6340 | `			}` |
|    ! 0 | 6341 | `		}` |
|      - | 6342 | `	}` |
|    ! 0 | 6343 | `	return PH7_OK;` |
|    ! 0 | 6344 | `}` |
|      - | 6345 | `/*` |
|      - | 6346 | ` * data:// stream: read-only in-memory payloads parsed from RFC 2397 URIs` |
|      - | 6347 | ` * (data://[mediatype][;base64],payload — the payload percent-decodes unless` |
|      - | 6348 | ` * base64). Shares the MEMORY machinery above.` |
|      - | 6349 | ` */` |
|      8 | 6350 | `static sxi32 DataStreamB64Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 6351 | `{` |
|      9 | 6352 | `	return SyBlobAppend((SyBlob *)pUserData,pData,nLen);` |
|      1 | 6353 | `}` |
|     10 | 6354 | `static int DataStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 6355 | `{` |
|      - | 6356 | `	ph7_stream_data *pData;` |
|     11 | 6357 | `	const char *zIn = zName;` |
|     11 | 6358 | `	const char *zEnd = &zName[SyStrlen(zName)];` |
|     11 | 6359 | `	const char *zComma = 0;` |
|     11 | 6360 | `	int bBase64 = 0;` |
|      5 | 6361 | `	SXUNUSED(iMode);` |
|      - | 6362 | `	/* Find the comma separating the mediatype from the payload */` |
|    105 | 6363 | `	while( zIn < zEnd ){` |
|    105 | 6364 | `		if( zIn[0] == ',' ){` |
|     11 | 6365 | `			zComma = zIn;` |
|     11 | 6366 | `			break;` |
|      - | 6367 | `		}` |
|     95 | 6368 | `		zIn++;` |
|      1 | 6369 | `	}` |
|     11 | 6370 | `	if( zComma == 0 ){` |
|    ! 0 | 6371 | `		return -1;` |
|      - | 6372 | `	}` |
|     10 | 6373 | `	if( zComma - zName >= (int)sizeof(";base64")-1` |
|     10 | 6374 | `	 && SyStrnicmp(&zComma[-((int)sizeof(";base64")-1)],";base64",sizeof(";base64")-1) == 0 ){` |
|      3 | 6375 | `		bBase64 = 1;` |
|      1 | 6376 | `	}` |
|     11 | 6377 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,PH7_IO_STREAM_MEMORY);` |
|     11 | 6378 | `	if( pData == 0 ){` |
|    ! 0 | 6379 | `		return -1;` |
|      - | 6380 | `	}` |
|     11 | 6381 | `	pData->bReadOnly = 1;` |
|     11 | 6382 | `	zIn = &zComma[1];` |
|     11 | 6383 | `	if( bBase64 ){` |
|      3 | 6384 | `		if( SyBase64Decode(zIn,(sxu32)(zEnd - zIn),DataStreamB64Consumer,&pData->sMem) != SXRET_OK ){` |
|    ! 0 | 6385 | `			SyBlobRelease(&pData->sMem);` |
|    ! 0 | 6386 | `			SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|    ! 0 | 6387 | `			return -1;` |
|      - | 6388 | `		}` |
|      2 | 6389 | `	}else{` |
|      - | 6390 | `		/* percent-decode the payload */` |
|     71 | 6391 | `		while( zIn < zEnd ){` |
|     63 | 6392 | `			char c = zIn[0];` |
|     63 | 6393 | `			if( c == '%' && zIn + 2 < zEnd && SyisHex(zIn[1]) && SyisHex(zIn[2]) ){` |
|      3 | 6394 | `				int hi = SyHexToint(zIn[1]);` |
|      3 | 6395 | `				int lo = SyHexToint(zIn[2]);` |
|      3 | 6396 | `				c = (char)((hi << 4) \| lo);` |
|      3 | 6397 | `				zIn += 3;` |
|      2 | 6398 | `			}else{` |
|     61 | 6399 | `				zIn++;` |
|      - | 6400 | `			}` |
|     63 | 6401 | `			if( SyBlobAppend(&pData->sMem,&c,1) != SXRET_OK ){` |
|    ! 0 | 6402 | `				SyBlobRelease(&pData->sMem);` |
|    ! 0 | 6403 | `				SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|    ! 0 | 6404 | `				return -1;` |
|      - | 6405 | `			}` |
|      1 | 6406 | `		}` |
|      - | 6407 | `	}` |
|     11 | 6408 | `	*ppHandle = (void *)pData;` |
|     11 | 6409 | `	return PH7_OK;` |
|      6 | 6410 | `}` |
|      - | 6411 | `/* data:// rejects writes outright */` |
|    ! 0 | 6412 | `static ph7_int64 DataStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 6413 | `{` |
|    ! 0 | 6414 | `	SXUNUSED(pHandle);` |
|    ! 0 | 6415 | `	SXUNUSED(pBuf);` |
|    ! 0 | 6416 | `	SXUNUSED(nWrite);` |
|    ! 0 | 6417 | `	return -1;` |
|    ! 0 | 6418 | `}` |
|      - | 6419 | `static const ph7_io_stream sDATA_Stream = {` |
|      - | 6420 | `	"data",` |
|      - | 6421 | `	PH7_IO_STREAM_VERSION,` |
|      - | 6422 | `	DataStreamData_Open,  /* xOpen */` |
|      - | 6423 | `	0,   /* xOpenDir */` |
|      - | 6424 | `	PHPStreamData_Close, /* xClose */` |
|      - | 6425 | `	0,  /* xCloseDir */` |
|      - | 6426 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 6427 | `	0,  /* xReadDir */` |
|      - | 6428 | `	DataStreamData_Write, /* xWrite */` |
|      - | 6429 | `	PHPStreamData_Seek,  /* xSeek */` |
|      - | 6430 | `	0,  /* xLock */` |
|      - | 6431 | `	0,  /* xRewindDir */` |
|      - | 6432 | `	PHPStreamData_Tell,  /* xTell */` |
|      - | 6433 | `	0,  /* xTrunc */` |
|      - | 6434 | `	0,  /* xSync */` |
|      - | 6435 | `	0   /* xStat */` |
|      - | 6436 | `};` |
|      - | 6437 | `/*` |
|      - | 6438 | ` * Pipe stream implementation for popen/pclose.` |
|      - | 6439 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|      - | 6440 | ` * PHP-compatible process I/O functionality.` |
|      - | 6441 | ` */` |
|      - | 6442 | `typedef struct pipe_private pipe_private;` |
|      - | 6443 | `struct pipe_private` |
|      - | 6444 | `{` |
|      - | 6445 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|      - | 6446 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|      - | 6447 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|      - | 6448 | `#ifdef __WINNT__` |
|      - | 6449 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|      - | 6450 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|      - | 6451 | `#endif` |
|      - | 6452 | `};` |
|      - | 6453 |  |
|      - | 6454 | `#ifdef __WINNT__` |
|      - | 6455 | `#include <Windows.h>` |
|      - | 6456 | `#include <stdio.h>` |
|      - | 6457 | `#include <io.h>` |
|      - | 6458 | `#include <fcntl.h>` |
|      - | 6459 | `/*` |
|      - | 6460 | ` * Custom Windows popen implementation using CreateProcess.` |
|      - | 6461 | ` * This allows us to properly wait for process completion.` |
|      - | 6462 | ` */` |
|      - | 6463 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|      5 | 6464 | `{` |
|      5 | 6465 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|      5 | 6466 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|      5 | 6467 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|      - | 6468 | `	SECURITY_ATTRIBUTES sa;` |
|      - | 6469 | `	STARTUPINFOW si;` |
|      - | 6470 | `	PROCESS_INFORMATION pi;` |
|      5 | 6471 | `	WCHAR *zWideCmd = NULL;` |
|      5 | 6472 | `	FILE *pFile = NULL;` |
|      - | 6473 | `	int fd;` |
|      5 | 6474 | `	BOOL bRead = (zMode[0] == 'r');` |
|      - | 6475 |  |
|      - | 6476 | `	/* Set up security attributes for pipe inheritance */` |
|      5 | 6477 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|      5 | 6478 | `	sa.bInheritHandle = TRUE;` |
|      5 | 6479 | `	sa.lpSecurityDescriptor = NULL;` |
|      - | 6480 |  |
|      - | 6481 | `	/* Create pipes for child process I/O */` |
|      5 | 6482 | `	if( bRead ){` |
|      - | 6483 | `		/* Reading from child's stdout */` |
|      5 | 6484 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|    ! 0 | 6485 | `			return NULL;` |
|      - | 6486 | `		}` |
|      - | 6487 | `		/* Ensure read handle is not inherited */` |
|      5 | 6488 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|      5 | 6489 | `		hReadPipe = hChildStdoutRd;` |
|      5 | 6490 | `		*phPipe = hChildStdoutRd;` |
|      5 | 6491 | `	}else{` |
|      - | 6492 | `		/* Writing to child's stdin */` |
|    ! 0 | 6493 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|    ! 0 | 6494 | `			return NULL;` |
|      - | 6495 | `		}` |
|      - | 6496 | `		/* Ensure write handle is not inherited */` |
|    ! 0 | 6497 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|    ! 0 | 6498 | `		hWritePipe = hChildStdinWr;` |
|    ! 0 | 6499 | `		*phPipe = hChildStdinWr;` |
|      - | 6500 | `	}` |
|      - | 6501 |  |
|      - | 6502 | `	/* Convert command to wide string */` |
|      - | 6503 | `	{` |
|      5 | 6504 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|      5 | 6505 | `		if( nLen <= 0 ){` |
|    ! 0 | 6506 | `			goto cleanup_pipes;` |
|      - | 6507 | `		}` |
|      5 | 6508 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|      5 | 6509 | `		if( !zWideCmd ){` |
|    ! 0 | 6510 | `			goto cleanup_pipes;` |
|      - | 6511 | `		}` |
|      5 | 6512 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|      - | 6513 | `	}` |
|      - | 6514 |  |
|      - | 6515 | `	/* Set up process startup info */` |
|      5 | 6516 | `	ZeroMemory(&si, sizeof(si));` |
|      5 | 6517 | `	si.cb = sizeof(si);` |
|      5 | 6518 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|      5 | 6519 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|      5 | 6520 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|      5 | 6521 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|      5 | 6522 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|      - | 6523 |  |
|      5 | 6524 | `	ZeroMemory(&pi, sizeof(pi));` |
|      - | 6525 |  |
|      - | 6526 | `	/* Create the child process */` |
|      5 | 6527 | `	if( !CreateProcessW(` |
|      - | 6528 | `		NULL,           /* Application name */` |
|      - | 6529 | `		zWideCmd,       /* Command line */` |
|      - | 6530 | `		NULL,           /* Process security attributes */` |
|      - | 6531 | `		NULL,           /* Thread security attributes */` |
|      - | 6532 | `		TRUE,           /* Inherit handles */` |
|      - | 6533 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|      - | 6534 | `		NULL,           /* Environment */` |
|      - | 6535 | `		NULL,           /* Current directory */` |
|      - | 6536 | `		&si,            /* Startup info */` |
|      - | 6537 | `		&pi             /* Process info */` |
|      - | 6538 | `	)){` |
|    ! 0 | 6539 | `		goto cleanup_all;` |
|      - | 6540 | `	}` |
|      - | 6541 |  |
|      - | 6542 | `	/* Close handles we don't need in parent */` |
|      5 | 6543 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|      5 | 6544 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|      - | 6545 |  |
|      - | 6546 | `	/* Close thread handle (we only need process handle) */` |
|      5 | 6547 | `	CloseHandle(pi.hThread);` |
|      - | 6548 |  |
|      - | 6549 | `	/* Store process handle for later waiting */` |
|      5 | 6550 | `	*phProcess = pi.hProcess;` |
|      - | 6551 |  |
|      - | 6552 | `	/* Convert OS handle to C file descriptor, then to FILE* */` |
|      5 | 6553 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|      - | 6554 | `	                     bRead ? _O_RDONLY \| _O_TEXT : _O_WRONLY \| _O_TEXT);` |
|      5 | 6555 | `	if( fd == -1 ){` |
|    ! 0 | 6556 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 6557 | `		*phProcess = NULL;` |
|    ! 0 | 6558 | `		goto cleanup_all;` |
|      - | 6559 | `	}` |
|      - | 6560 |  |
|      5 | 6561 | `	pFile = _fdopen(fd, zMode);` |
|      5 | 6562 | `	if( !pFile ){` |
|    ! 0 | 6563 | `		_close(fd); /* This will also close the underlying handle */` |
|    ! 0 | 6564 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 6565 | `		*phProcess = NULL;` |
|    ! 0 | 6566 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    ! 0 | 6567 | `		return NULL;` |
|      - | 6568 | `	}` |
|      - | 6569 |  |
|      5 | 6570 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      5 | 6571 | `	return pFile;` |
|      - | 6572 |  |
|      - | 6573 | `cleanup_all:` |
|    ! 0 | 6574 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      - | 6575 | `cleanup_pipes:` |
|    ! 0 | 6576 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|    ! 0 | 6577 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|    ! 0 | 6578 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|    ! 0 | 6579 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|    ! 0 | 6580 | `	return NULL;` |
|      5 | 6581 | `}` |
|      - | 6582 |  |
|      - | 6583 | `/*` |
|      - | 6584 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|      - | 6585 | ` */` |
|      - | 6586 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|      5 | 6587 | `{` |
|      5 | 6588 | `	DWORD dwExitCode = 0;` |
|      - | 6589 | `	int status;` |
|      - | 6590 |  |
|      - | 6591 | `	/* Close the FILE* (this closes the pipe) */` |
|      5 | 6592 | `	fclose(pFile);` |
|      - | 6593 |  |
|      5 | 6594 | `	if( hProcess ){` |
|      - | 6595 | `		/* Wait for the process to complete */` |
|      5 | 6596 | `		WaitForSingleObject(hProcess, INFINITE);` |
|      - | 6597 |  |
|      5 | 6598 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|      5 | 6599 | `			status = (int)dwExitCode;` |
|      5 | 6600 | `		}else{` |
|    ! 0 | 6601 | `			status = -1;` |
|      - | 6602 | `		}` |
|      - | 6603 |  |
|      - | 6604 | `		/* Close process handle */` |
|      5 | 6605 | `		CloseHandle(hProcess);` |
|      5 | 6606 | `	}else{` |
|    ! 0 | 6607 | `		status = -1;` |
|      - | 6608 | `	}` |
|      - | 6609 |  |
|      5 | 6610 | `	return status;` |
|      5 | 6611 | `}` |
|      - | 6612 | `#endif /* __WINNT__ */` |
|      - | 6613 | `/*` |
|      - | 6614 | ` * Open a pipe to a process.` |
|      - | 6615 | ` * This is called internally by popen(), not through the stream device interface.` |
|      - | 6616 | ` */` |
|   4046 | 6617 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|      5 | 6618 | `{` |
|      - | 6619 | `	pipe_private *pPipe;` |
|      - | 6620 | `	FILE *pFile;` |
|   4051 | 6621 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|    ! 0 | 6622 | `		return 0;` |
|      - | 6623 | `	}` |
|      - | 6624 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|   4051 | 6625 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|    ! 0 | 6626 | `		return 0;` |
|      - | 6627 | `	}` |
|      - | 6628 | `	/* Open the pipe using system popen */` |
|      - | 6629 | `#ifdef __WINNT__` |
|      - | 6630 | `	{` |
|      - | 6631 | `		/* Build cmd.exe command wrapper */` |
|      5 | 6632 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|      5 | 6633 | `		const char *zShellSuffix = "\"";` |
|      5 | 6634 | `		size_t nPrefix = strlen(zShellPrefix);` |
|      5 | 6635 | `		size_t nSuffix = strlen(zShellSuffix);` |
|      5 | 6636 | `		size_t nCmd = strlen(zCommand);` |
|      5 | 6637 | `		size_t nQuotes = 0;` |
|      5 | 6638 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 6639 | `			if (zCommand[i] == '"') nQuotes++;` |
|      5 | 6640 | `		}` |
|      5 | 6641 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|      5 | 6642 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|      5 | 6643 | `		if (zCmdEsc == NULL) {` |
|    ! 0 | 6644 | `			return 0;` |
|      - | 6645 | `		}` |
|      - | 6646 | `		/* Escape quotes in command */` |
|      5 | 6647 | `		size_t j = 0;` |
|      5 | 6648 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 6649 | `			char ch = zCommand[i];` |
|      5 | 6650 | `			if (ch == '"') {` |
|      4 | 6651 | `				zCmdEsc[j++] = '^';` |
|      4 | 6652 | `				zCmdEsc[j++] = '"';` |
|      4 | 6653 | `			} else {` |
|      5 | 6654 | `				zCmdEsc[j++] = ch;` |
|      - | 6655 | `			}` |
|      5 | 6656 | `		}` |
|      5 | 6657 | `		zCmdEsc[j] = '\0';` |
|      5 | 6658 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|      5 | 6659 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|      5 | 6660 | `		if (zWinCmd == NULL) {` |
|    ! 0 | 6661 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 6662 | `			return 0;` |
|      - | 6663 | `		}` |
|      5 | 6664 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|      5 | 6665 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|      5 | 6666 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|      5 | 6667 | `		zWinCmd[nTotal - 1] = '\0';` |
|      - | 6668 | `		/* Allocate pipe structure early so we can store handles */` |
|      5 | 6669 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|      5 | 6670 | `		if( pPipe == 0 ){` |
|    ! 0 | 6671 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 6672 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|    ! 0 | 6673 | `			return 0;` |
|      - | 6674 | `		}` |
|      - | 6675 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|      5 | 6676 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|      5 | 6677 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|      5 | 6678 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|      5 | 6679 | `		if( pFile == 0 ){` |
|    ! 0 | 6680 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|    ! 0 | 6681 | `			return 0;` |
|      - | 6682 | `		}` |
|      - | 6683 | `		/* Initialize remaining fields */` |
|      5 | 6684 | `		pPipe->pFile = pFile;` |
|      5 | 6685 | `		pPipe->pVm = pVm;` |
|      5 | 6686 | `		pPipe->iMode = zMode[0];` |
|      - | 6687 | `	}` |
|      - | 6688 | `#elif defined(__UNIXES__) /* Unix */` |
|   4046 | 6689 | `	pFile = popen(zCommand, zMode);` |
|   4046 | 6690 | `	if( pFile == 0 ){` |
|    ! 0 | 6691 | `		return 0;` |
|      - | 6692 | `	}` |
|      - | 6693 | `	/* Allocate pipe private structure */` |
|   4046 | 6694 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|   4046 | 6695 | `	if( pPipe == 0 ){` |
|      - | 6696 | `		/* Out of memory, close the pipe */` |
|    ! 0 | 6697 | `		pclose(pFile);` |
|    ! 0 | 6698 | `		return 0;` |
|      - | 6699 | `	}` |
|      - | 6700 | `	/* Initialize the structure */` |
|   4046 | 6701 | `	pPipe->pFile = pFile;` |
|   4046 | 6702 | `	pPipe->pVm = pVm;` |
|   4046 | 6703 | `	pPipe->iMode = zMode[0];` |
|      - | 6704 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 6705 | `	(void)pFile;` |
|      - | 6706 | `	return 0;` |
|      - | 6707 | `#endif` |
|   4051 | 6708 | `	return pPipe;` |
|   2028 | 6709 | `}` |
|      - | 6710 | `/*` |
|      - | 6711 | ` * Close a pipe and return the exit status of the process.` |
|      - | 6712 | ` * Returns the exit status, or -1 on error.` |
|      - | 6713 | ` */` |
|   4020 | 6714 | `static int PipeClose(pipe_private *pPipe)` |
|      5 | 6715 | `{` |
|      - | 6716 | `	int status;` |
|      - | 6717 | `	ph7_vm *pVm;` |
|   4025 | 6718 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6719 | `		return -1;` |
|      - | 6720 | `	}` |
|   4025 | 6721 | `	pVm = pPipe->pVm;` |
|      - | 6722 | `	/* Close the pipe and get exit status */` |
|      - | 6723 | `#ifdef __WINNT__` |
|      - | 6724 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|      5 | 6725 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|      - | 6726 | `#elif defined(__UNIXES__)` |
|   4020 | 6727 | `	status = pclose(pPipe->pFile);` |
|      - | 6728 | `	/* On Unix, pclose returns the status from waitpid, need to extract exit code */` |
|   4020 | 6729 | `	if( status != -1 ){` |
|   4020 | 6730 | `		if( WIFEXITED(status) ){` |
|   4020 | 6731 | `			status = WEXITSTATUS(status);` |
|   2010 | 6732 | `		}else if( WIFSIGNALED(status) ){` |
|      - | 6733 | `			/* Process was killed by a signal - use shell convention: 128 + signal number */` |
|    ! 0 | 6734 | `			status = 128 + WTERMSIG(status);` |
|    ! 0 | 6735 | `		}else{` |
|      - | 6736 | `			/* Unknown termination reason */` |
|    ! 0 | 6737 | `			status = -1;` |
|      - | 6738 | `		}` |
|   2010 | 6739 | `	}` |
|      - | 6740 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 6741 | `	status = -1;` |
|      - | 6742 | `#endif` |
|      - | 6743 | `	/* Free the structure */` |
|   4025 | 6744 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|   4025 | 6745 | `	return status;` |
|   2015 | 6746 | `}` |
|      - | 6747 | `/*` |
|      - | 6748 | ` * Pipe stream xClose implementation.` |
|      - | 6749 | ` * Note: This is called by fclose(), not pclose().` |
|      - | 6750 | ` * It closes the pipe but does not return the exit status.` |
|      - | 6751 | ` */` |
|    100 | 6752 | `static void PipeStream_Close(void *pHandle)` |
|      4 | 6753 | `{` |
|    104 | 6754 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|    104 | 6755 | `	if( pPipe ){` |
|    104 | 6756 | `		PipeClose(pPipe);` |
|     50 | 6757 | `	}` |
|    104 | 6758 | `}` |
|      - | 6759 | `/*` |
|      - | 6760 | ` * Pipe stream xRead implementation.` |
|      - | 6761 | ` */` |
|   5928 | 6762 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|      4 | 6763 | `{` |
|   5932 | 6764 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 6765 | `	size_t nRead;` |
|   5932 | 6766 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6767 | `		return -1;` |
|      - | 6768 | `	}` |
|   5932 | 6769 | `	if( pPipe->iMode != 'r' ){` |
|      - | 6770 | `		/* Cannot read from a write-only pipe */` |
|    ! 0 | 6771 | `		return -1;` |
|      - | 6772 | `	}` |
|   5932 | 6773 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
|   5932 | 6774 | `	if( nRead == 0 ){` |
|   4054 | 6775 | `		if( feof(pPipe->pFile) ){` |
|   4054 | 6776 | `			return 0; /* EOF */` |
|      - | 6777 | `		}` |
|    ! 0 | 6778 | `		return -1; /* Error */` |
|      - | 6779 | `	}` |
|   1882 | 6780 | `	return (ph7_int64)nRead;` |
|   2968 | 6781 | `}` |
|      - | 6782 | `/*` |
|      - | 6783 | ` * Pipe stream xWrite implementation.` |
|      - | 6784 | ` */` |
|      2 | 6785 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|    ! 0 | 6786 | `{` |
|      2 | 6787 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 6788 | `	size_t nWritten;` |
|      2 | 6789 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6790 | `		return -1;` |
|      - | 6791 | `	}` |
|      2 | 6792 | `	if( pPipe->iMode != 'w' ){` |
|      - | 6793 | `		/* Cannot write to a read-only pipe */` |
|    ! 0 | 6794 | `		return -1;` |
|      - | 6795 | `	}` |
|      2 | 6796 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|      2 | 6797 | `	if( nWritten == 0 && nWrite > 0 ){` |
|    ! 0 | 6798 | `		return -1; /* Error */` |
|      - | 6799 | `	}` |
|      2 | 6800 | `	return (ph7_int64)nWritten;` |
|      1 | 6801 | `}` |
|      - | 6802 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|      - | 6803 | `static const ph7_io_stream sPipe_Stream = {` |
|      - | 6804 | `	"pipe",` |
|      - | 6805 | `	PH7_IO_STREAM_VERSION,` |
|      - | 6806 | `	0,  /* xOpen - not used, pipes opened via PipeOpen() */` |
|      - | 6807 | `	0,  /* xOpenDir */` |
|      - | 6808 | `	PipeStream_Close,  /* xClose */` |
|      - | 6809 | `	0,  /* xCloseDir */` |
|      - | 6810 | `	PipeStream_Read,   /* xRead */` |
|      - | 6811 | `	0,  /* xReadDir */` |
|      - | 6812 | `	PipeStream_Write,  /* xWrite */` |
|      - | 6813 | `	0,  /* xSeek */` |
|      - | 6814 | `	0,  /* xLock */` |
|      - | 6815 | `	0,  /* xRewindDir */` |
|      - | 6816 | `	0,  /* xTell */` |
|      - | 6817 | `	0,  /* xTrunc */` |
|      - | 6818 | `	0,  /* xSync */` |
|      - | 6819 | `	0   /* xStat */` |
|      - | 6820 | `};` |
|      - | 6821 | `/*` |
|      - | 6822 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|      - | 6823 | ` * FALSE otherwise.` |
|      - | 6824 | ` */` |
|   3920 | 6825 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|      5 | 6826 | `{` |
|   3925 | 6827 | `	return pStream == &sPipe_Stream;` |
|      5 | 6828 | `}` |
|      - | 6829 | `/*` |
|      - | 6830 | ` * resource popen(string $command, string $mode)` |
|      - | 6831 | ` *  Opens process file pointer.` |
|      - | 6832 | ` * Parameters` |
|      - | 6833 | ` *  $command` |
|      - | 6834 | ` *   The command to execute. Passed to the system shell.` |
|      - | 6835 | ` *  $mode` |
|      - | 6836 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|      - | 6837 | ` *   'r' - Open for reading (read from the command's stdout).` |
|      - | 6838 | ` *   'w' - Open for writing (write to the command's stdin).` |
|      - | 6839 | ` * Return` |
|      - | 6840 | ` *  Returns a file pointer on success, or FALSE on error.` |
|      - | 6841 | ` */` |
|   4046 | 6842 | `static int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6843 | `{` |
|      - | 6844 | `	const char *zCommand, *zMode;` |
|      - | 6845 | `	pipe_private *pPipe;` |
|      - | 6846 | `	io_private *pDev;` |
|      - | 6847 | `	int nCmdLen, nModeLen;` |
|   4051 | 6848 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 6849 | `		/* Missing/Invalid arguments, return FALSE */` |
|    ! 0 | 6850 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a command string and mode");` |
|    ! 0 | 6851 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6852 | `		return PH7_OK;` |
|      - | 6853 | `	}` |
|      - | 6854 | `	/* Extract the command and mode */` |
|   4051 | 6855 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|   4051 | 6856 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|   4051 | 6857 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 6858 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty command");` |
|    ! 0 | 6859 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6860 | `		return PH7_OK;` |
|      - | 6861 | `	}` |
|   4051 | 6862 | `	if( nModeLen < 1 \|\| (zMode[0] != 'r' && zMode[0] != 'w') ){` |
|    ! 0 | 6863 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Invalid mode, expected 'r' or 'w'");` |
|    ! 0 | 6864 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6865 | `		return PH7_OK;` |
|      - | 6866 | `	}` |
|      - | 6867 | `	/* Open the pipe */` |
|   4051 | 6868 | `	pPipe = PipeOpen(pCtx->pVm, zCommand, zMode);` |
|   4051 | 6869 | `	if( pPipe == 0 ){` |
|      - | 6870 | `		/* Failed to open pipe */` |
|    ! 0 | 6871 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6872 | `		return PH7_OK;` |
|      - | 6873 | `	}` |
|      - | 6874 | `	/* Allocate an io_private instance to wrap the pipe */` |
|   4051 | 6875 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|   4051 | 6876 | `	if( pDev == 0 ){` |
|    ! 0 | 6877 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|    ! 0 | 6878 | `		PipeClose(pPipe);` |
|    ! 0 | 6879 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6880 | `		return PH7_OK;` |
|      - | 6881 | `	}` |
|      - | 6882 | `	/* Initialize the io_private structure */` |
|   4051 | 6883 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|   4051 | 6884 | `	pDev->pHandle = pPipe;` |
|      - | 6885 | `	/* Return the io_private instance as a resource */` |
|   4051 | 6886 | `	ph7_result_resource(pCtx, pDev);` |
|   4051 | 6887 | `	return PH7_OK;` |
|   2028 | 6888 | `}` |
|      - | 6889 | `/*` |
|      - | 6890 | ` * int pclose(resource $handle)` |
|      - | 6891 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|      - | 6892 | ` * Parameters` |
|      - | 6893 | ` *  $handle` |
|      - | 6894 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|      - | 6895 | ` * Return` |
|      - | 6896 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|      - | 6897 | ` */` |
|   3920 | 6898 | `static int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6899 | `{` |
|      - | 6900 | `	const ph7_io_stream *pStream;` |
|      - | 6901 | `	pipe_private *pPipe;` |
|      - | 6902 | `	io_private *pDev;` |
|      - | 6903 | `	int status;` |
|   3925 | 6904 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 6905 | `		/* Missing/Invalid arguments, return -1 */` |
|    ! 0 | 6906 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 6907 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 6908 | `		return PH7_OK;` |
|      - | 6909 | `	}` |
|      - | 6910 | `	/* Extract our private data */` |
|   3925 | 6911 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 6912 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   3925 | 6913 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 6914 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 6915 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 6916 | `		return PH7_OK;` |
|      - | 6917 | `	}` |
|      - | 6918 | `	/* Point to the target IO stream device */` |
|   3925 | 6919 | `	pStream = pDev->pStream;` |
|   3925 | 6920 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|    ! 0 | 6921 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|    ! 0 | 6922 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 6923 | `		return PH7_OK;` |
|      - | 6924 | `	}` |
|      - | 6925 | `	/* Get the pipe handle */` |
|   3925 | 6926 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|      - | 6927 | `	/* Close the pipe and get exit status */` |
|   3925 | 6928 | `	status = PipeClose(pPipe);` |
|      - | 6929 | `	/* Release the IO private structure */` |
|   3925 | 6930 | `	ReleaseIOPrivate(pCtx, pDev);` |
|      - | 6931 | `	/* Invalidate the resource handle */` |
|   3925 | 6932 | `	ph7_value_release(apArg[0]);` |
|      - | 6933 | `	/* Return the exit status */` |
|   3925 | 6934 | `	ph7_result_int(pCtx, status);` |
|   3925 | 6935 | `	return PH7_OK;` |
|   1965 | 6936 | `}` |
|      - | 6937 | `/* Export the php:// stream */` |
|      - | 6938 | `static const ph7_io_stream sPHP_Stream = {` |
|      - | 6939 | `	"php",` |
|      - | 6940 | `	PH7_IO_STREAM_VERSION,` |
|      - | 6941 | `	PHPStreamData_Open,  /* xOpen */` |
|      - | 6942 | `	0,   /* xOpenDir */` |
|      - | 6943 | `	PHPStreamData_Close, /* xClose */` |
|      - | 6944 | `	0,  /* xCloseDir */` |
|      - | 6945 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 6946 | `	0,  /* xReadDir */` |
|      - | 6947 | `	PHPStreamData_Write, /* xWrite */` |
|      - | 6948 | `	PHPStreamData_Seek,  /* xSeek (php://memory & php://temp) */` |
|      - | 6949 | `	0,  /* xLock */` |
|      - | 6950 | `	0,  /* xRewindDir */` |
|      - | 6951 | `	PHPStreamData_Tell,  /* xTell */` |
|      - | 6952 | `	PHPStreamData_Trunc, /* xTrunc */` |
|      - | 6953 | `	0,  /* xSync */` |
|      - | 6954 | `	0   /* xStat */` |
|      - | 6955 | `};` |
|      - | 6956 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 6957 | `/*` |
|      - | 6958 | ` * Return TRUE if we are dealing with the php:// stream.` |
|      - | 6959 | ` * FALSE otherwise.` |
|      - | 6960 | ` */` |
|     80 | 6961 | `static int is_php_stream(const ph7_io_stream *pStream)` |
|      3 | 6962 | `{` |
|      - | 6963 | `#ifndef PH7_DISABLE_DISK_IO` |
|     83 | 6964 | `	return pStream == &sPHP_Stream;` |
|      - | 6965 | `#else` |
|      - | 6966 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 6967 | `	return 0;` |
|      - | 6968 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      3 | 6969 | `}` |
|      - | 6970 | `/*` |
|      - | 6971 | ` * Return TRUE if we are dealing with the data:// stream.` |
|      - | 6972 | ` */` |
|     70 | 6973 | `static int is_data_stream(const ph7_io_stream *pStream)` |
|      3 | 6974 | `{` |
|      - | 6975 | `#ifndef PH7_DISABLE_DISK_IO` |
|     73 | 6976 | `	return pStream == &sDATA_Stream;` |
|      - | 6977 | `#else` |
|      - | 6978 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 6979 | `	return 0;` |
|      - | 6980 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      3 | 6981 | `}` |
|      - | 6982 |  |
|      - | 6983 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 6984 | `/*` |
|      - | 6985 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 6986 | ` * [i.e: file://,php://].` |
|      - | 6987 | ` * Note:` |
|      - | 6988 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 6989 | ` *  defined then this function is a no-op.` |
|      - | 6990 | ` */` |
|   3558 | 6991 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 6992 | `{` |
|      - | 6993 | `	/*` |
|      - | 6994 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 6995 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 6996 | `	 */` |
|      - | 6997 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 6998 | `	/* VFS: disk I/O related functions */` |
|      - | 6999 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 7000 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 7001 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 7002 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 7003 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 7004 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 7005 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 7006 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 7007 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 7008 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 7009 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 7010 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 7011 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 7012 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 7013 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 7014 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 7015 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 7016 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 7017 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 7018 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 7019 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 7020 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 7021 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 7022 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 7023 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 7024 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 7025 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 7026 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 7027 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 7028 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 7029 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 7030 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 7031 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 7032 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 7033 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 7034 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 7035 | `		{"link",        PH7_vfs_link     },` |
|      - | 7036 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 7037 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 7038 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 7039 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 7040 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 7041 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 7042 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 7043 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 7044 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 7045 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 7046 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 7047 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 7048 | `	};` |
|      - | 7049 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 7050 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 7051 | `	 */` |
|      - | 7052 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 7053 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 7054 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 7055 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 7056 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 7057 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 7058 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 7059 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 7060 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 7061 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 7062 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 7063 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 7064 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 7065 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 7066 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 7067 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 7068 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 7069 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 7070 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 7071 | `		{"file",      PH7_builtin_file   },` |
|      - | 7072 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 7073 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 7074 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 7075 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 7076 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 7077 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 7078 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 7079 | `		{"stream_get_contents",  PH7_builtin_stream_get_contents },` |
|      - | 7080 | `		{"stream_get_wrappers",  PH7_builtin_stream_get_wrappers },` |
|      - | 7081 | `		{"stream_get_meta_data", PH7_builtin_stream_get_meta_data },` |
|      - | 7082 | `		{"stream_context_create",PH7_builtin_stream_context_create },` |
|      - | 7083 | `		{"stream_wrapper_register",   PH7_builtin_stream_wrapper_register },` |
|      - | 7084 | `		{"stream_register_wrapper",   PH7_builtin_stream_wrapper_register },` |
|      - | 7085 | `		{"stream_wrapper_unregister", PH7_builtin_stream_wrapper_unregister },` |
|      - | 7086 | `#ifdef PH7_ENABLE_NET` |
|      - | 7087 | `		{"fsockopen",  PH7_builtin_fsockopen },` |
|      - | 7088 | `		{"pfsockopen", PH7_builtin_fsockopen },` |
|      - | 7089 | `		{"stream_socket_client", PH7_builtin_fsockopen },` |
|      - | 7090 | `#endif` |
|      - | 7091 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 7092 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 7093 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 7094 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 7095 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 7096 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 7097 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 7098 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 7099 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7100 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 7101 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 7102 | `	};` |
|   3563 | 7103 | `	const ph7_io_stream *pFileStream = 0;` |
|   3563 | 7104 | `	sxu32 n = 0;` |
|      - | 7105 | `	/* Register disk-related functions */` |
| 174347 | 7106 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 170789 | 7107 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
|  85397 | 7108 | `	}` |
| 163673 | 7109 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 160115 | 7110 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  80060 | 7111 | `	}` |
|      - | 7112 | `#else` |
|      - | 7113 | `	SXUNUSED(pVm);` |
|      - | 7114 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7115 |  |
|      - | 7116 | `	/*` |
|      - | 7117 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7118 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 7119 | `	 */` |
|      - | 7120 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7121 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 7122 | `		/* Path processing */` |
|      - | 7123 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 7124 | `		{"basename",    PH7_builtin_basename },` |
|      - | 7125 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 7126 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 7127 | `		{"fnmatch",     PH7_builtin_fnmatch  },` |
|      - | 7128 | `		/* ZIP processing */` |
|      - | 7129 | `		{"zip_open",    PH7_builtin_zip_open },` |
|      - | 7130 | `		{"zip_close",   PH7_builtin_zip_close},` |
|      - | 7131 | `		{"zip_read",    PH7_builtin_zip_read },` |
|      - | 7132 | `		{"zip_entry_open", PH7_builtin_zip_entry_open },` |
|      - | 7133 | `		{"zip_entry_close",PH7_builtin_zip_entry_close},` |
|      - | 7134 | `		{"zip_entry_name", PH7_builtin_zip_entry_name },` |
|      - | 7135 | `		{"zip_entry_filesize",      PH7_builtin_zip_entry_filesize       },` |
|      - | 7136 | `		{"zip_entry_compressedsize",PH7_builtin_zip_entry_compressedsize },` |
|      - | 7137 | `		{"zip_entry_read", PH7_builtin_zip_entry_read },` |
|      - | 7138 | `		{"zip_entry_reset_read_cursor",PH7_builtin_zip_entry_reset_read_cursor},` |
|      - | 7139 | `		{"zip_entry_compressionmethod",PH7_builtin_zip_entry_compressionmethod}` |
|      - | 7140 | `	};` |
|  60491 | 7141 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  56933 | 7142 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|  28469 | 7143 | `	}` |
|      - | 7144 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 7145 |  |
|      - | 7146 | `	/* Install streams if disk I/O is enabled */` |
|      - | 7147 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7148 | `#ifdef __WINNT__` |
|      5 | 7149 | `	pFileStream = &sWinFileStream;` |
|      - | 7150 | `#elif defined(__UNIXES__)` |
|   3558 | 7151 | `	pFileStream = &sUnixFileStream;` |
|      - | 7152 | `#endif` |
|      - | 7153 | `	/* Install the php:// stream */` |
|   3563 | 7154 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   3563 | 7155 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);` |
|      - | 7156 | `#ifdef PH7_ENABLE_NET` |
|   3563 | 7157 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);` |
|      - | 7158 | `#endif` |
|   3563 | 7159 | `	if( pFileStream ){` |
|      - | 7160 | `		/* Install the file:// stream */` |
|   3563 | 7161 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   1779 | 7162 | `	}` |
|      - | 7163 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7164 |  |
|   3563 | 7165 | `	return SXRET_OK;` |
|      5 | 7166 | `}` |
|      - | 7167 | `/*` |
|      - | 7168 | ` * Export the STDIN handle.` |
|      - | 7169 | ` */` |
|      2 | 7170 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|      1 | 7171 | `{` |
|      - | 7172 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 7173 | `	if( pVm->pStdin == 0  ){` |
|      - | 7174 | `		io_private *pIn;` |
|      - | 7175 | `		/* Allocate an IO private instance */` |
|      3 | 7176 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 7177 | `		if( pIn == 0 ){` |
|    ! 0 | 7178 | `			return 0;` |
|      - | 7179 | `		}` |
|      3 | 7180 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|      - | 7181 | `		/* Initialize the handle */` |
|      3 | 7182 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|      - | 7183 | `		/* Install the STDIN stream */` |
|      3 | 7184 | `		pVm->pStdin = pIn;` |
|      3 | 7185 | `		return pIn;` |
|    ! 0 | 7186 | `	}else{` |
|      - | 7187 | `		/* NULL or STDIN */` |
|    ! 0 | 7188 | `		return pVm->pStdin;` |
|      - | 7189 | `	}` |
|      - | 7190 | `#else` |
|      - | 7191 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7192 | `	return 0;` |
|      - | 7193 | `#endif` |
|      2 | 7194 | `}` |
|      - | 7195 | `/*` |
|      - | 7196 | ` * Export the STDOUT handle.` |
|      - | 7197 | ` */` |
|      2 | 7198 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|      1 | 7199 | `{` |
|      - | 7200 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 7201 | `	if( pVm->pStdout == 0  ){` |
|      - | 7202 | `		io_private *pOut;` |
|      - | 7203 | `		/* Allocate an IO private instance */` |
|      3 | 7204 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 7205 | `		if( pOut == 0 ){` |
|    ! 0 | 7206 | `			return 0;` |
|      - | 7207 | `		}` |
|      3 | 7208 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|      - | 7209 | `		/* Initialize the handle */` |
|      3 | 7210 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|      - | 7211 | `		/* Install the STDOUT stream */` |
|      3 | 7212 | `		pVm->pStdout = pOut;` |
|      3 | 7213 | `		return pOut;` |
|    ! 0 | 7214 | `	}else{` |
|      - | 7215 | `		/* NULL or STDOUT */` |
|    ! 0 | 7216 | `		return pVm->pStdout;` |
|      - | 7217 | `	}` |
|      - | 7218 | `#else` |
|      - | 7219 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7220 | `	return 0;` |
|      - | 7221 | `#endif` |
|      2 | 7222 | `}` |
|      - | 7223 | `/*` |
|      - | 7224 | ` * Export the STDERR handle.` |
|      - | 7225 | ` */` |
|      2 | 7226 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|      1 | 7227 | `{` |
|      - | 7228 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 7229 | `	if( pVm->pStderr == 0  ){` |
|      - | 7230 | `		io_private *pErr;` |
|      - | 7231 | `		/* Allocate an IO private instance */` |
|      3 | 7232 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 7233 | `		if( pErr == 0 ){` |
|    ! 0 | 7234 | `			return 0;` |
|      - | 7235 | `		}` |
|      3 | 7236 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|      - | 7237 | `		/* Initialize the handle */` |
|      3 | 7238 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|      - | 7239 | `		/* Install the STDERR stream */` |
|      3 | 7240 | `		pVm->pStderr = pErr;` |
|      3 | 7241 | `		return pErr;` |
|    ! 0 | 7242 | `	}else{` |
|      - | 7243 | `		/* NULL or STDERR */` |
|    ! 0 | 7244 | `		return pVm->pStderr;` |
|      - | 7245 | `	}` |
|      - | 7246 | `#else` |
|      - | 7247 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7248 | `	return 0;` |
|      - | 7249 | `#endif` |
|      2 | 7250 | `}` |
|      - | 7251 |  |
