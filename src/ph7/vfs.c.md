# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2368/3548 lines (66.74%)

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
|  13140 |   66 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |   67 | `{` |
|      - |   68 | `	const char *zPath;` |
|      - |   69 | `	ph7_vfs *pVfs;` |
|      - |   70 | `	int rc;` |
|  13145 |   71 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |   72 | `		/* Missing/Invalid argument,return FALSE */` |
|      6 |   73 | `		ph7_result_bool(pCtx,0);` |
|      6 |   74 | `		return PH7_OK;` |
|      - |   75 | `	}` |
|      - |   76 | `	/* Point to the underlying vfs */` |
|  13141 |   77 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  13141 |   78 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |   79 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |   80 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |   81 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |   82 | `			ph7_function_name(pCtx)` |
|      - |   83 | `			);` |
|    ! 0 |   84 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |   85 | `		return PH7_OK;` |
|      - |   86 | `	}` |
|      - |   87 | `	/* Point to the desired directory */` |
|  13141 |   88 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |   89 | `	/* Perform the requested operation */` |
|  13141 |   90 | `	rc = pVfs->xChdir(zPath);` |
|      - |   91 | `	/* IO return value */` |
|  13141 |   92 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  13141 |   93 | `	return PH7_OK;` |
|   6575 |   94 | `}` |
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
|      3 |  115 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  116 | `	if( pVfs == 0 \|\| pVfs->xChroot == 0 ){` |
|      - |  117 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  118 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  119 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  120 | `			ph7_function_name(pCtx)` |
|      - |  121 | `			);` |
|    ! 0 |  122 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  123 | `		return PH7_OK;` |
|      - |  124 | `	}` |
|      - |  125 | `	/* Point to the desired directory */` |
|      3 |  126 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  127 | `	/* Perform the requested operation */` |
|      3 |  128 | `	rc = pVfs->xChroot(zPath);` |
|      - |  129 | `	/* IO return value */` |
|      3 |  130 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 |  131 | `	return PH7_OK;` |
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
|   8506 |  214 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  215 | `{` |
|      - |  216 | `	const char *zPath;` |
|      - |  217 | `	ph7_vfs *pVfs;` |
|      - |  218 | `	int rc;` |
|   8511 |  219 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  220 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  221 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  222 | `		return PH7_OK;` |
|      - |  223 | `	}` |
|      - |  224 | `	/* Point to the underlying vfs */` |
|   8511 |  225 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   8511 |  226 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  227 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  228 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  229 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  230 | `			ph7_function_name(pCtx)` |
|      - |  231 | `			);` |
|    ! 0 |  232 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  233 | `		return PH7_OK;` |
|      - |  234 | `	}` |
|      - |  235 | `	/* Point to the desired directory */` |
|   8511 |  236 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  237 | `	/* Perform the requested operation */` |
|   8511 |  238 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  239 | `	/* IO return value */` |
|   8511 |  240 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   8511 |  241 | `	return PH7_OK;` |
|   4258 |  242 | `}` |
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
|     34 |  262 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  263 | `{` |
|     36 |  264 | `	int iRecursive = 0;` |
|      - |  265 | `	const char *zPath;` |
|      - |  266 | `	ph7_vfs *pVfs;` |
|      - |  267 | `	int iMode,rc;` |
|     36 |  268 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  269 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  270 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  271 | `		return PH7_OK;` |
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
|     19 |  303 | `}` |
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
|      2 |  315 | `static int PH7_vfs_rename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  316 | `{` |
|      - |  317 | `	const char *zOld,*zNew;` |
|      - |  318 | `	ph7_vfs *pVfs;` |
|      - |  319 | `	int rc;` |
|      3 |  320 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - |  321 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  322 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  323 | `		return PH7_OK;` |
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
|      2 |  343 | `}` |
|      - |  344 | `/*` |
|      - |  345 | ` * string realpath(string $path)` |
|      - |  346 | ` *  Returns canonicalized absolute pathname.` |
|      - |  347 | ` * Parameters` |
|      - |  348 | ` *  $path` |
|      - |  349 | ` *   Target path.` |
|      - |  350 | ` * Return` |
|      - |  351 | ` *  Canonicalized absolute pathname on success. or FALSE on failure.` |
|      - |  352 | ` */` |
|      4 |  353 | `static int PH7_vfs_realpath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  354 | `{` |
|      - |  355 | `	const char *zPath;` |
|      - |  356 | `	ph7_vfs *pVfs;` |
|      - |  357 | `        int rc;` |
|      5 |  358 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  359 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  360 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  361 | `		return PH7_OK;` |
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
|      3 |  383 | `}` |
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
|  32704 |  477 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  478 | `{` |
|      - |  479 | `	const char *zPath;` |
|      - |  480 | `	ph7_vfs *pVfs;` |
|      - |  481 | `	int rc;` |
|  32709 |  482 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  483 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  484 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  485 | `		return PH7_OK;` |
|      - |  486 | `	}` |
|      - |  487 | `	/* Point to the underlying vfs */` |
|  32709 |  488 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  32709 |  489 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  490 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  491 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  492 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  493 | `			ph7_function_name(pCtx)` |
|      - |  494 | `			);` |
|    ! 0 |  495 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  496 | `		return PH7_OK;` |
|      - |  497 | `	}` |
|      - |  498 | `	/* Point to the desired directory */` |
|  32709 |  499 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  500 | `	/* Perform the requested operation */` |
|  32709 |  501 | `	rc = pVfs->xUnlink(zPath);` |
|      - |  502 | `	/* IO return value */` |
|  32709 |  503 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  32709 |  504 | `	return PH7_OK;` |
|  16357 |  505 | `}` |
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
|      4 |  560 | `static int PH7_vfs_chown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  561 | `{` |
|      - |  562 | `	const char *zPath,*zUser;` |
|      - |  563 | `	ph7_vfs *pVfs;` |
|      - |  564 | `	int rc;` |
|      4 |  565 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  566 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  567 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  568 | `		return PH7_OK;` |
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
|      2 |  590 | `}` |
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
|      4 |  602 | `static int PH7_vfs_chgrp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  603 | `{` |
|      - |  604 | `	const char *zPath,*zGroup;` |
|      - |  605 | `	ph7_vfs *pVfs;` |
|      - |  606 | `	int rc;` |
|      4 |  607 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  608 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  609 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  610 | `		return PH7_OK;` |
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
|      2 |  632 | `}` |
|      - |  633 | `/*` |
|      - |  634 | ` * int64 disk_free_space(string $directory)` |
|      - |  635 | ` *  Returns available space on filesystem or disk partition.` |
|      - |  636 | ` * Parameters` |
|      - |  637 | ` *  $directory` |
|      - |  638 | ` *   A directory of the filesystem or disk partition.` |
|      - |  639 | ` * Return` |
|      - |  640 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  641 | ` */` |
|      4 |  642 | `static int PH7_vfs_disk_free_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  643 | `{` |
|      - |  644 | `	const char *zPath;` |
|      - |  645 | `	ph7_int64 iSize;` |
|      - |  646 | `	ph7_vfs *pVfs;` |
|      5 |  647 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  648 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  649 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  650 | `		return PH7_OK;` |
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
|      3 |  670 | `}` |
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
|     52 |  718 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  719 | `{` |
|      - |  720 | `	const char *zPath;` |
|      - |  721 | `	ph7_vfs *pVfs;` |
|      - |  722 | `	int rc;` |
|     54 |  723 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  724 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  725 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  726 | `		return PH7_OK;` |
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
|     28 |  746 | `}` |
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
|     35 | 1511 | `	if( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ){` |
|      - | 1512 | `		/* Nothing but separators ("/", "///"): php answers the EMPTY string, where the` |
|      - | 1513 | `		 * strip loop above stops one short and left PH7 returning "/". */` |
|      5 | 1514 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1515 | `		return PH7_OK;` |
|      - | 1516 | `	}` |
|     31 | 1517 | `	iLen = (int)(&zEnd[1]-zPath);` |
|    752 | 1518 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    707 | 1519 | `		zEnd--;` |
|      1 | 1520 | `	}` |
|     31 | 1521 | `	zBase = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|     31 | 1522 | `	zEnd = &zPath[iLen];` |
|     31 | 1523 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 1524 | `		const char *zSuffix;` |
|      - | 1525 | `		int nSuffix;` |
|      - | 1526 | `		/* Strip suffix */` |
|      5 | 1527 | `		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);` |
|      5 | 1528 | `		if( nSuffix > 0 && nSuffix < iLen && SyMemcmp(&zEnd[-nSuffix],zSuffix,nSuffix) == 0 ){` |
|      5 | 1529 | `			zEnd -= nSuffix;` |
|      2 | 1530 | `		}` |
|      2 | 1531 | `	}` |
|      - | 1532 | `	/* Store the basename */` |
|     31 | 1533 | `	ph7_result_string(pCtx,zBase,(int)(zEnd-zBase));` |
|     31 | 1534 | `	return PH7_OK;` |
|     19 | 1535 | `}` |
|      - | 1536 | `/*` |
|      - | 1537 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1538 | ` *  Returns information about a file path.` |
|      - | 1539 | ` * Parameter` |
|      - | 1540 | ` *  $path` |
|      - | 1541 | ` *   The path to be parsed.` |
|      - | 1542 | ` *  $options` |
|      - | 1543 | ` *    If present, specifies a specific element to be returned; one of` |
|      - | 1544 | ` *      PATHINFO_DIRNAME, PATHINFO_BASENAME, PATHINFO_EXTENSION or PATHINFO_FILENAME.` |
|      - | 1545 | ` * Return` |
|      - | 1546 | ` *  If the options parameter is not passed, an associative array containing the following` |
|      - | 1547 | ` *  elements is returned: dirname, basename, extension (if any), and filename.` |
|      - | 1548 | ` *  If options is present, returns a string containing the requested element.` |
|      - | 1549 | ` */` |
|      - | 1550 | `typedef struct path_info path_info;` |
|      - | 1551 | `struct path_info` |
|      - | 1552 | `{` |
|      - | 1553 | `	SyString sDir; /* Directory [i.e: /var/www] */` |
|      - | 1554 | `	SyString sBasename; /* Basename [i.e httpd.conf] */` |
|      - | 1555 | `	SyString sExtension; /* File extension [i.e xml,pdf..] */` |
|      - | 1556 | `	SyString sFilename;  /* Filename */` |
|      - | 1557 | `};` |
|      - | 1558 | `/*` |
|      - | 1559 | ` * Extract path fields.` |
|      - | 1560 | ` */` |
|  13014 | 1561 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 1562 | `{` |
|  13019 | 1563 | `	const char *zPtr,*zEnd = &zPath[nByte - 1];` |
|      - | 1564 | `	SyString *pCur;` |
|      - | 1565 | `	int c,d;` |
|  13019 | 1566 | `	c = d = '/';` |
|      - | 1567 | `#ifdef __WINNT__` |
|      5 | 1568 | `	d = '\\';` |
|      - | 1569 | `#endif` |
|      - | 1570 | `	/* Zero the structure */` |
|  13019 | 1571 | `	SyZero(pOut,sizeof(path_info));` |
|      - | 1572 | `	/* Handle special case */` |
|  13019 | 1573 | `	if( nByte == sizeof(char) && ( (int)zPath[0] == c \|\| (int)zPath[0] == d ) ){` |
|      - | 1574 | `#ifdef __WINNT__` |
|    ! 0 | 1575 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1576 | `#else` |
|    ! 0 | 1577 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1578 | `#endif` |
|    ! 0 | 1579 | `		return SXRET_OK;` |
|      - | 1580 | `	}` |
|      - | 1581 | `	/* Extract the basename */` |
| 351138 | 1582 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
| 331617 | 1583 | `		zEnd--;` |
|      5 | 1584 | `	}` |
|  13019 | 1585 | `	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|  13019 | 1586 | `	zEnd = &zPath[nByte];` |
|      - | 1587 | `	/* dirname */` |
|  13019 | 1588 | `	pCur = &pOut->sDir;` |
|  13019 | 1589 | `	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);` |
|  13019 | 1590 | `	if( pCur->nByte > 1 ){` |
|  26033 | 1591 | `		SyStringTrimTrailingChar(pCur,'/');` |
|      - | 1592 | `#ifdef __WINNT__` |
|      5 | 1593 | `		SyStringTrimTrailingChar(pCur,'\\');` |
|      - | 1594 | `#endif` |
|   6512 | 1595 | `	}else if( (int)zPath[0] == c \|\| (int)zPath[0] == d ){` |
|      - | 1596 | `#ifdef __WINNT__` |
|    ! 0 | 1597 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1598 | `#else` |
|    ! 0 | 1599 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1600 | `#endif` |
|    ! 0 | 1601 | `	}` |
|      - | 1602 | `	/* basename/filename */` |
|  13019 | 1603 | `	pCur = &pOut->sBasename;` |
|  13019 | 1604 | `	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);` |
|  13019 | 1605 | `	SyStringTrimLeadingChar(pCur,'/');` |
|      - | 1606 | `#ifdef __WINNT__` |
|      5 | 1607 | `	SyStringTrimLeadingChar(pCur,'\\');` |
|      - | 1608 | `#endif` |
|  13019 | 1609 | `	SyStringDupPtr(&pOut->sFilename,pCur);` |
|  13019 | 1610 | `	if( pCur->nByte > 0 ){` |
|      - | 1611 | `		/* extension */` |
|  13019 | 1612 | `		zEnd--;` |
|  65069 | 1613 | `		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){` |
|  52055 | 1614 | `			zEnd--;` |
|      5 | 1615 | `		}` |
|  13019 | 1616 | `		if( zEnd > pCur->zString ){` |
|  13017 | 1617 | `			zEnd++; /* Jump leading dot */` |
|  13017 | 1618 | `			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);` |
|      - | 1619 | `			/* Fix filename */` |
|  13017 | 1620 | `			pCur = &pOut->sFilename;` |
|  13017 | 1621 | `			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){` |
|  13017 | 1622 | `				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);` |
|   6506 | 1623 | `			}` |
|   6506 | 1624 | `		}` |
|   6507 | 1625 | `	}` |
|  13019 | 1626 | `	return SXRET_OK;` |
|   6512 | 1627 | `}` |
|      - | 1628 | `/*` |
|      - | 1629 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1630 | ` *  See block comment above.` |
|      - | 1631 | ` */` |
|  13014 | 1632 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1633 | `{` |
|      - | 1634 | `	const char *zPath;` |
|      - | 1635 | `	path_info sInfo;` |
|      - | 1636 | `	SyString *pComp;` |
|      - | 1637 | `	int iLen;` |
|  13019 | 1638 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1639 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1640 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1641 | `		return PH7_OK;` |
|      - | 1642 | `	}` |
|      - | 1643 | `	/* Point to the target path */` |
|  13019 | 1644 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|  13019 | 1645 | `	if( iLen < 1 ){` |
|      - | 1646 | `		/* Empty string */` |
|    ! 0 | 1647 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1648 | `		return PH7_OK;` |
|      - | 1649 | `	}` |
|      - | 1650 | `	/* Extract path info */` |
|  13019 | 1651 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|  19525 | 1652 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1653 | `		/* Return path component */` |
|  13017 | 1654 | `		int nComp = ph7_value_to_int(apArg[1]);` |
|  13017 | 1655 | `		switch(nComp){` |
|      1 | 1656 | `		case 1: /* PATHINFO_DIRNAME */` |
|      3 | 1657 | `			pComp = &sInfo.sDir;` |
|      3 | 1658 | `			if( pComp->nByte > 0 ){` |
|      3 | 1659 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1660 | `			}else{` |
|      - | 1661 | `				/* Expand the empty string */` |
|    ! 0 | 1662 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1663 | `			}` |
|      3 | 1664 | `			break;` |
|      1 | 1665 | `		case 2: /*PATHINFO_BASENAME*/` |
|      3 | 1666 | `			pComp = &sInfo.sBasename;` |
|      3 | 1667 | `			if( pComp->nByte > 0 ){` |
|      3 | 1668 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1669 | `			}else{` |
|      - | 1670 | `				/* Expand the empty string */` |
|    ! 0 | 1671 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1672 | `			}` |
|      3 | 1673 | `			break;` |
|   3254 | 1674 | `		case 3: /*PATHINFO_EXTENSION*/` |
|   6513 | 1675 | `			pComp = &sInfo.sExtension;` |
|   6513 | 1676 | `			if( pComp->nByte > 0 ){` |
|   6511 | 1677 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3258 | 1678 | `			}else{` |
|      - | 1679 | `				/* Expand the empty string */` |
|      3 | 1680 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1681 | `			}` |
|   6513 | 1682 | `			break;` |
|   3250 | 1683 | `		case 4: /*PATHINFO_FILENAME*/` |
|   6505 | 1684 | `			pComp = &sInfo.sFilename;` |
|   6505 | 1685 | `			if( pComp->nByte > 0 ){` |
|   6505 | 1686 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3255 | 1687 | `			}else{` |
|      - | 1688 | `				/* Expand the empty string */` |
|    ! 0 | 1689 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1690 | `			}` |
|   6505 | 1691 | `			break;` |
|    ! 0 | 1692 | `		default:` |
|      - | 1693 | `			/* Expand the empty string */` |
|    ! 0 | 1694 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1695 | `			break;` |
|      - | 1696 | `		}` |
|   6511 | 1697 | `	}else{` |
|      - | 1698 | `		/* Return an associative array */` |
|      - | 1699 | `		ph7_value *pArray,*pValue;` |
|      3 | 1700 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 1701 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 1702 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1703 | `			/* Out of mem,return NULL */` |
|    ! 0 | 1704 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1705 | `			return PH7_OK;` |
|      - | 1706 | `		}` |
|      - | 1707 | `		/* dirname */` |
|      3 | 1708 | `		pComp = &sInfo.sDir;` |
|      3 | 1709 | `		if( pComp->nByte > 0 ){` |
|      3 | 1710 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1711 | `			/* Perform the insertion */` |
|      3 | 1712 | `			ph7_array_add_strkey_elem(pArray,"dirname",pValue); /* Will make it's own copy */` |
|      1 | 1713 | `		}` |
|      - | 1714 | `		/* Reset the string cursor */` |
|      3 | 1715 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1716 | `		/* basername */` |
|      3 | 1717 | `		pComp = &sInfo.sBasename;` |
|      3 | 1718 | `		if( pComp->nByte > 0 ){` |
|      3 | 1719 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1720 | `			/* Perform the insertion */` |
|      3 | 1721 | `			ph7_array_add_strkey_elem(pArray,"basename",pValue); /* Will make it's own copy */` |
|      1 | 1722 | `		}` |
|      - | 1723 | `		/* Reset the string cursor */` |
|      3 | 1724 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1725 | `		/* extension */` |
|      3 | 1726 | `		pComp = &sInfo.sExtension;` |
|      3 | 1727 | `		if( pComp->nByte > 0 ){` |
|      3 | 1728 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1729 | `			/* Perform the insertion */` |
|      3 | 1730 | `			ph7_array_add_strkey_elem(pArray,"extension",pValue); /* Will make it's own copy */` |
|      1 | 1731 | `		}` |
|      - | 1732 | `		/* Reset the string cursor */` |
|      3 | 1733 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1734 | `		/* filename */` |
|      3 | 1735 | `		pComp = &sInfo.sFilename;` |
|      3 | 1736 | `		if( pComp->nByte > 0 ){` |
|      3 | 1737 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1738 | `			/* Perform the insertion */` |
|      3 | 1739 | `			ph7_array_add_strkey_elem(pArray,"filename",pValue); /* Will make it's own copy */` |
|      1 | 1740 | `		}` |
|      - | 1741 | `		/* Return the created array */` |
|      3 | 1742 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1743 | `		/* Don't worry about freeing memory, everything will be released` |
|      - | 1744 | `		 * automatically as soon we return from this foreign function.` |
|      - | 1745 | `		 */` |
|      - | 1746 | `	}` |
|  13019 | 1747 | `	return PH7_OK;` |
|   6512 | 1748 | `}` |
|      - | 1749 | `/* SPDX-SnippetBegin */` |
|      - | 1750 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 1751 | `/* SPDX-License-Identifier: blessing */` |
|      - | 1752 | `/*` |
|      - | 1753 | ` * Globbing implementation extracted from the sqlite3 source tree.` |
|      - | 1754 |  |
|      - | 1755 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|      - | 1756 | ` * Status: Public Domain` |
|      - | 1757 | ` */` |
|      - | 1758 | `typedef unsigned char u8;` |
|      - | 1759 | `/* An array to map all upper-case characters into their corresponding` |
|      - | 1760 | `** lower-case character.` |
|      - | 1761 | `**` |
|      - | 1762 | `** SQLite only considers US-ASCII (or EBCDIC) characters.  We do not` |
|      - | 1763 | `** handle case conversions for the UTF character set since the tables` |
|      - | 1764 | `** involved are nearly as big or bigger than SQLite itself.` |
|      - | 1765 | `*/` |
|      - | 1766 | `static const unsigned char sqlite3UpperToLower[] = {` |
|      - | 1767 | `      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,` |
|      - | 1768 | `     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,` |
|      - | 1769 | `     36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,` |
|      - | 1770 | `     54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,` |
|      - | 1771 | `    104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,` |
|      - | 1772 | `    122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,` |
|      - | 1773 | `    108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,` |
|      - | 1774 | `    126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,` |
|      - | 1775 | `    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,` |
|      - | 1776 | `    162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,` |
|      - | 1777 | `    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,` |
|      - | 1778 | `    198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,` |
|      - | 1779 | `    216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,` |
|      - | 1780 | `    234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,` |
|      - | 1781 | `    252,253,254,255` |
|      - | 1782 | `};` |
|      - | 1783 | `#define GlogUpperToLower(A)     if( A<0x80 ){ A = sqlite3UpperToLower[A]; }` |
|      - | 1784 | `/*` |
|      - | 1785 | `** Assuming zIn points to the first byte of a UTF-8 character,` |
|      - | 1786 | `** advance zIn to point to the first byte of the next UTF-8 character.` |
|      - | 1787 | `*/` |
|      - | 1788 | `#define SQLITE_SKIP_UTF8(zIn) {                        \` |
|      - | 1789 | `  if( (*(zIn++))>=0xc0 ){                              \` |
|      - | 1790 | `    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \` |
|      - | 1791 | `  }                                                    \` |
|      - | 1792 | `}` |
|      - | 1793 | `/*` |
|      - | 1794 | `** Compare two UTF-8 strings for equality where the first string can` |
|      - | 1795 | `** potentially be a "glob" expression.  Return true (1) if they` |
|      - | 1796 | `** are the same and false (0) if they are different.` |
|      - | 1797 | `**` |
|      - | 1798 | `** Globbing rules:` |
|      - | 1799 | `**` |
|      - | 1800 | `**      '*'       Matches any sequence of zero or more characters.` |
|      - | 1801 | `**` |
|      - | 1802 | `**      '?'       Matches exactly one character.` |
|      - | 1803 | `**` |
|      - | 1804 | `**     [...]      Matches one character from the enclosed list of` |
|      - | 1805 | `**                characters.` |
|      - | 1806 | `**` |
|      - | 1807 | `**     [^...]     Matches one character not in the enclosed list.` |
|      - | 1808 | `**` |
|      - | 1809 | `** With the [...] and [^...] matching, a ']' character can be included` |
|      - | 1810 | `** in the list by making it the first character after '[' or '^'.  A` |
|      - | 1811 | `** range of characters can be specified using '-'.  Example:` |
|      - | 1812 | `** "[a-z]" matches any single lower-case letter.  To match a '-', make` |
|      - | 1813 | `** it the last character in the list.` |
|      - | 1814 | `**` |
|      - | 1815 | `** This routine is usually quick, but can be N**2 in the worst case.` |
|      - | 1816 | `**` |
|      - | 1817 | `** Hints: to match '*' or '?', put them in "[]".  Like this:` |
|      - | 1818 | `**` |
|      - | 1819 | `**         abc[*]xyz        Matches "abc*xyz" only` |
|      - | 1820 | `*/` |
|     20 | 1821 | `static int patternCompare(` |
|      - | 1822 | `  const u8 *zPattern,              /* The glob pattern */` |
|      - | 1823 | `  const u8 *zString,               /* The string to compare against the glob */` |
|      - | 1824 | `  const int esc,                    /* The escape character */` |
|      - | 1825 | `  int noCase` |
|      1 | 1826 | `){` |
|      - | 1827 | `  int c, c2;` |
|      - | 1828 | `  int invert;` |
|      - | 1829 | `  int seen;` |
|     21 | 1830 | `  u8 matchOne = '?';` |
|     21 | 1831 | `  u8 matchAll = '*';` |
|     21 | 1832 | `  u8 matchSet = '[';` |
|     21 | 1833 | `  int prevEscape = 0;     /* True if the previous character was 'escape' */` |
|      - | 1834 |  |
|     21 | 1835 | `  if( !zPattern \|\| !zString ) return 0;` |
|     51 | 1836 | `  while( (c = PH7_Utf8Read(zPattern,0,&zPattern))!=0 ){` |
|     43 | 1837 | `    if( !prevEscape && c==matchAll ){` |
|     16 | 1838 | `      while( (c=PH7_Utf8Read(zPattern,0,&zPattern)) == matchAll` |
|      9 | 1839 | `               \|\| c == matchOne ){` |
|    ! 0 | 1840 | `        if( c==matchOne && PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1841 | `          return 0;` |
|      - | 1842 | `        }` |
|    ! 0 | 1843 | `      }` |
|      9 | 1844 | `      if( c==0 ){` |
|    ! 0 | 1845 | `        return 1;` |
|      9 | 1846 | `      }else if( c==esc ){` |
|    ! 0 | 1847 | `        c = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1848 | `        if( c==0 ){` |
|    ! 0 | 1849 | `          return 0;` |
|    ! 0 | 1850 | `        }` |
|      9 | 1851 | `      }else if( c==matchSet ){` |
|    ! 0 | 1852 | `	  if( (esc==0) \|\| (matchSet<0x80) ) return 0;` |
|    ! 0 | 1853 | `	  while( *zString && patternCompare(&zPattern[-1],zString,esc,noCase)==0 ){` |
|    ! 0 | 1854 | `          SQLITE_SKIP_UTF8(zString);` |
|    ! 0 | 1855 | `        }` |
|    ! 0 | 1856 | `        return *zString!=0;` |
|      - | 1857 | `      }` |
|     11 | 1858 | `      while( (c2 = PH7_Utf8Read(zString,0,&zString))!=0 ){` |
|     11 | 1859 | `        if( noCase ){` |
|      3 | 1860 | `          GlogUpperToLower(c2);` |
|      3 | 1861 | `          GlogUpperToLower(c);` |
|     11 | 1862 | `          while( c2 != 0 && c2 != c ){` |
|      9 | 1863 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      9 | 1864 | `            GlogUpperToLower(c2);` |
|      1 | 1865 | `          }` |
|      2 | 1866 | `        }else{` |
|     47 | 1867 | `          while( c2 != 0 && c2 != c ){` |
|     39 | 1868 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      1 | 1869 | `          }` |
|      - | 1870 | `        }` |
|     11 | 1871 | `        if( c2==0 ) return 0;` |
|      9 | 1872 | `		if( patternCompare(zPattern,zString,esc,noCase) ) return 1;` |
|      1 | 1873 | `      }` |
|    ! 0 | 1874 | `      return 0;` |
|     35 | 1875 | `    }else if( !prevEscape && c==matchOne ){` |
|    ! 0 | 1876 | `      if( PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1877 | `        return 0;` |
|    ! 0 | 1878 | `      }` |
|     35 | 1879 | `    }else if( c==matchSet ){` |
|    ! 0 | 1880 | `      int prior_c = 0;` |
|    ! 0 | 1881 | `      if( esc == 0 ) return 0;` |
|    ! 0 | 1882 | `      seen = 0;` |
|    ! 0 | 1883 | `      invert = 0;` |
|    ! 0 | 1884 | `      c = PH7_Utf8Read(zString, 0, &zString);` |
|    ! 0 | 1885 | `      if( c==0 ) return 0;` |
|    ! 0 | 1886 | `      c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1887 | `      if( c2=='^' ){` |
|    ! 0 | 1888 | `        invert = 1;` |
|    ! 0 | 1889 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1890 | `      }` |
|    ! 0 | 1891 | `      if( c2==']' ){` |
|    ! 0 | 1892 | `        if( c==']' ) seen = 1;` |
|    ! 0 | 1893 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1894 | `      }` |
|    ! 0 | 1895 | `      while( c2 && c2!=']' ){` |
|    ! 0 | 1896 | `        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){` |
|    ! 0 | 1897 | `          c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1898 | `          if( c>=prior_c && c<=c2 ) seen = 1;` |
|    ! 0 | 1899 | `          prior_c = 0;` |
|    ! 0 | 1900 | `        }else{` |
|    ! 0 | 1901 | `          if( c==c2 ){` |
|    ! 0 | 1902 | `            seen = 1;` |
|    ! 0 | 1903 | `          }` |
|    ! 0 | 1904 | `          prior_c = c2;` |
|      - | 1905 | `        }` |
|    ! 0 | 1906 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1907 | `      }` |
|    ! 0 | 1908 | `      if( c2==0 \|\| (seen ^ invert)==0 ){` |
|    ! 0 | 1909 | `        return 0;` |
|    ! 0 | 1910 | `      }` |
|     35 | 1911 | `    }else if( esc==c && !prevEscape ){` |
|    ! 0 | 1912 | `      prevEscape = 1;` |
|    ! 0 | 1913 | `    }else{` |
|     35 | 1914 | `      c2 = PH7_Utf8Read(zString, 0, &zString);` |
|     35 | 1915 | `      if( noCase ){` |
|      7 | 1916 | `        GlogUpperToLower(c);` |
|      7 | 1917 | `        GlogUpperToLower(c2);` |
|      3 | 1918 | `      }` |
|     35 | 1919 | `      if( c!=c2 ){` |
|      5 | 1920 | `        return 0;` |
|      - | 1921 | `      }` |
|     31 | 1922 | `      prevEscape = 0;` |
|      - | 1923 | `    }` |
|      1 | 1924 | `  }` |
|      9 | 1925 | `  return *zString==0;` |
|     11 | 1926 | `}` |
|      - | 1927 | `/* SPDX-SnippetEnd */` |
|      - | 1928 | `/*` |
|      - | 1929 | ` * Wrapper around patternCompare() defined above.` |
|      - | 1930 | ` * See block comment above for more information.` |
|      - | 1931 | ` */` |
|     12 | 1932 | `static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,int CaseCompare)` |
|      1 | 1933 | `{` |
|      - | 1934 | `	int rc;` |
|     13 | 1935 | `	if( iEsc < 0 ){` |
|    ! 0 | 1936 | `		iEsc = '\\';` |
|    ! 0 | 1937 | `	}` |
|     13 | 1938 | `	rc = patternCompare(zPattern,zString,iEsc,CaseCompare);` |
|     13 | 1939 | `	return rc;` |
|      1 | 1940 | `}` |
|      - | 1941 | `/*` |
|      - | 1942 | ` * bool fnmatch(string $pattern,string $string[,int $flags = 0 ])` |
|      - | 1943 | ` *  Match filename against a pattern.` |
|      - | 1944 | ` * Parameters` |
|      - | 1945 | ` *  $pattern` |
|      - | 1946 | ` *   The shell wildcard pattern.` |
|      - | 1947 | ` * $string` |
|      - | 1948 | ` *  The tested string.` |
|      - | 1949 | ` * $flags` |
|      - | 1950 | ` *   A list of possible flags:` |
|      - | 1951 | ` *    FNM_NOESCAPE 	Disable backslash escaping.` |
|      - | 1952 | ` *    FNM_PATHNAME 	Slash in string only matches slash in the given pattern.` |
|      - | 1953 | ` *    FNM_PERIOD 	Leading period in string must be exactly matched by period in the given pattern.` |
|      - | 1954 | ` *    FNM_CASEFOLD 	Caseless match.` |
|      - | 1955 | ` * Return` |
|      - | 1956 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 1957 | ` */` |
|      8 | 1958 | `static int PH7_builtin_fnmatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1959 | `{` |
|      - | 1960 | `	const char *zString,*zPattern;` |
|      9 | 1961 | `	int iEsc = '\\';` |
|      9 | 1962 | `	int noCase = 0;` |
|      - | 1963 | `	int rc;` |
|      9 | 1964 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 1965 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1966 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1967 | `		return PH7_OK;` |
|      - | 1968 | `	}` |
|      - | 1969 | `	/* Extract the pattern and the string */` |
|      9 | 1970 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|      9 | 1971 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 1972 | `	/* Extract the flags if avaialble */` |
|      9 | 1973 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      7 | 1974 | `		rc = ph7_value_to_int(apArg[2]);` |
|      7 | 1975 | `		if( rc & 2 /*FNM_NOESCAPE (php value)*/){` |
|    ! 0 | 1976 | `			iEsc = 0;` |
|    ! 0 | 1977 | `		}` |
|      7 | 1978 | `		if( rc & 16 /*FNM_CASEFOLD (php value)*/){` |
|      3 | 1979 | `			noCase = 1;` |
|      1 | 1980 | `		}` |
|      3 | 1981 | `	}` |
|      - | 1982 | `	/* Go globbing */` |
|      9 | 1983 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,noCase);` |
|      - | 1984 | `	/* Globbing result */` |
|      9 | 1985 | `	ph7_result_bool(pCtx,rc);` |
|      9 | 1986 | `	return PH7_OK;` |
|      5 | 1987 | `}` |
|      - | 1988 | `/*` |
|      - | 1989 | ` * bool strglob(string $pattern,string $string)` |
|      - | 1990 | ` *  Match string against a pattern.` |
|      - | 1991 | ` * Parameters` |
|      - | 1992 | ` *  $pattern` |
|      - | 1993 | ` *   The shell wildcard pattern.` |
|      - | 1994 | ` * $string` |
|      - | 1995 | ` *  The tested string.` |
|      - | 1996 | ` * Return` |
|      - | 1997 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 1998 | ` * Note that this a symisc eXtension.` |
|      - | 1999 | ` */` |
|      4 | 2000 | `static int PH7_builtin_strglob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2001 | `{` |
|      - | 2002 | `	const char *zString,*zPattern;` |
|      5 | 2003 | `	int iEsc = '\\';` |
|      - | 2004 | `	int rc;` |
|      5 | 2005 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2006 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2007 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2008 | `		return PH7_OK;` |
|      - | 2009 | `	}` |
|      - | 2010 | `	/* Extract the pattern and the string */` |
|      5 | 2011 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|      5 | 2012 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2013 | `	/* Go globbing */` |
|      5 | 2014 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0);` |
|      - | 2015 | `	/* Globbing result */` |
|      5 | 2016 | `	ph7_result_bool(pCtx,rc);` |
|      5 | 2017 | `	return PH7_OK;` |
|      3 | 2018 | `}` |
|      - | 2019 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2020 | `/*` |
|      - | 2021 | ` * bool link(string $target,string $link)` |
|      - | 2022 |  |
|      - | 2023 | ` *  Create a hard link.` |
|      - | 2024 | ` * Parameters` |
|      - | 2025 | ` *  $target` |
|      - | 2026 | ` *   Target of the link.` |
|      - | 2027 | ` *  $link` |
|      - | 2028 | ` *   The link name.` |
|      - | 2029 | ` * Return` |
|      - | 2030 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2031 | ` */` |
|      2 | 2032 | `static int PH7_vfs_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2033 | `{` |
|      - | 2034 | `	const char *zTarget,*zLink;` |
|      - | 2035 | `	ph7_vfs *pVfs;` |
|      - | 2036 | `	int rc;` |
|      3 | 2037 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2038 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2039 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2040 | `		return PH7_OK;` |
|      - | 2041 | `	}` |
|      - | 2042 | `	/* Point to the underlying vfs */` |
|      3 | 2043 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2044 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2045 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2046 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2047 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2048 | `			ph7_function_name(pCtx)` |
|      - | 2049 | `			);` |
|    ! 0 | 2050 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2051 | `		return PH7_OK;` |
|      - | 2052 | `	}` |
|      - | 2053 | `	/* Extract the given arguments */` |
|      3 | 2054 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      3 | 2055 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2056 | `	/* Perform the requested operation */` |
|      3 | 2057 | `	rc = pVfs->xLink(zTarget,zLink,0/*Not a symbolic link */);` |
|      - | 2058 | `	/* IO result */` |
|      3 | 2059 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 2060 | `	return PH7_OK;` |
|      2 | 2061 | `}` |
|      - | 2062 | `/*` |
|      - | 2063 | ` * bool symlink(string $target,string $link)` |
|      - | 2064 | ` *  Creates a symbolic link.` |
|      - | 2065 | ` * Parameters` |
|      - | 2066 | ` *  $target` |
|      - | 2067 | ` *   Target of the link.` |
|      - | 2068 | ` *  $link` |
|      - | 2069 | ` *   The link name.` |
|      - | 2070 | ` * Return` |
|      - | 2071 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2072 | ` */` |
|      6 | 2073 | `static int PH7_vfs_symlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2074 | `{` |
|      - | 2075 | `	const char *zTarget,*zLink;` |
|      - | 2076 | `	ph7_vfs *pVfs;` |
|      - | 2077 | `	int rc;` |
|      7 | 2078 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2079 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2080 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2081 | `		return PH7_OK;` |
|      - | 2082 | `	}` |
|      - | 2083 | `	/* Point to the underlying vfs */` |
|      7 | 2084 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 2085 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2086 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2087 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2088 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2089 | `			ph7_function_name(pCtx)` |
|      - | 2090 | `			);` |
|    ! 0 | 2091 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2092 | `		return PH7_OK;` |
|      - | 2093 | `	}` |
|      - | 2094 | `	/* Extract the given arguments */` |
|      7 | 2095 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      7 | 2096 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2097 | `	/* Perform the requested operation */` |
|      7 | 2098 | `	rc = pVfs->xLink(zTarget,zLink,1/*A symbolic link */);` |
|      - | 2099 | `	/* IO result */` |
|      7 | 2100 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      7 | 2101 | `	return PH7_OK;` |
|      4 | 2102 | `}` |
|      - | 2103 | `/*` |
|      - | 2104 | ` * int umask([ int $mask ])` |
|      - | 2105 | ` *  Changes the current umask.` |
|      - | 2106 | ` * Parameters` |
|      - | 2107 | ` *  $mask` |
|      - | 2108 | ` *   The new umask.` |
|      - | 2109 | ` * Return` |
|      - | 2110 | ` *  umask() without arguments simply returns the current umask.` |
|      - | 2111 | ` *  Otherwise the old umask is returned.` |
|      - | 2112 | ` */` |
|      8 | 2113 | `static int PH7_vfs_umask(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2114 | `{` |
|      - | 2115 | `	int iOld,iNew;` |
|      - | 2116 | `	ph7_vfs *pVfs;` |
|      - | 2117 | `	/* Point to the underlying vfs */` |
|      9 | 2118 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 | 2119 | `	if( pVfs == 0 \|\| pVfs->xUmask == 0 ){` |
|      - | 2120 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2121 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2122 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2123 | `			ph7_function_name(pCtx)` |
|      - | 2124 | `			);` |
|    ! 0 | 2125 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2126 | `		return PH7_OK;` |
|      - | 2127 | `	}` |
|      9 | 2128 | `	iNew = 0;` |
|      9 | 2129 | `	if( nArg > 0 ){` |
|      5 | 2130 | `		iNew = ph7_value_to_int(apArg[0]);` |
|      2 | 2131 | `	}` |
|      - | 2132 | `	/* Perform the requested operation */` |
|      9 | 2133 | `	iOld = pVfs->xUmask(iNew);` |
|      - | 2134 | `	/* Old mask */` |
|      9 | 2135 | `	ph7_result_int(pCtx,iOld);` |
|      9 | 2136 | `	return PH7_OK;` |
|      5 | 2137 | `}` |
|      - | 2138 | `/*` |
|      - | 2139 | ` * string sys_get_temp_dir()` |
|      - | 2140 | ` *  Returns directory path used for temporary files.` |
|      - | 2141 | ` * Parameters` |
|      - | 2142 | ` *  None` |
|      - | 2143 | ` * Return` |
|      - | 2144 | ` *  Returns the path of the temporary directory.` |
|      - | 2145 | ` */` |
|    218 | 2146 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2147 | `{` |
|      - | 2148 | `	ph7_vfs *pVfs;` |
|      - | 2149 | `	/* Set the empty string as the default return value */` |
|    223 | 2150 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2151 | `	/* Point to the underlying vfs */` |
|    223 | 2152 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    223 | 2153 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
|    ! 0 | 2154 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2155 | `		SXUNUSED(apArg);` |
|      - | 2156 | `		/* IO routine not implemented,return "" */` |
|    ! 0 | 2157 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2158 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2159 | `			ph7_function_name(pCtx)` |
|      - | 2160 | `			);` |
|    ! 0 | 2161 | `		return PH7_OK;` |
|      - | 2162 | `	}` |
|      - | 2163 | `	/* Perform the requested operation */` |
|    223 | 2164 | `	pVfs->xTempDir(pCtx);` |
|    223 | 2165 | `	return PH7_OK;` |
|    114 | 2166 | `}` |
|      - | 2167 | `/*` |
|      - | 2168 | ` * string get_current_user()` |
|      - | 2169 | ` *  Returns the name of the current working user.` |
|      - | 2170 | ` * Parameters` |
|      - | 2171 | ` *  None` |
|      - | 2172 | ` * Return` |
|      - | 2173 | ` *  Returns the name of the current working user.` |
|      - | 2174 | ` */` |
|      2 | 2175 | `static int PH7_vfs_get_current_user(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2176 | `{` |
|      - | 2177 | `	ph7_vfs *pVfs;` |
|      - | 2178 | `	/* Point to the underlying vfs */` |
|      3 | 2179 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2180 | `	if( pVfs == 0 \|\| pVfs->xUsername == 0 ){` |
|    ! 0 | 2181 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2182 | `		SXUNUSED(apArg);` |
|      - | 2183 | `		/* IO routine not implemented */` |
|    ! 0 | 2184 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2185 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2186 | `			ph7_function_name(pCtx)` |
|      - | 2187 | `			);` |
|      - | 2188 | `		/* Set a dummy username */` |
|    ! 0 | 2189 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 2190 | `		return PH7_OK;` |
|      - | 2191 | `	}` |
|      - | 2192 | `	/* Perform the requested operation */` |
|      3 | 2193 | `	pVfs->xUsername(pCtx);` |
|      3 | 2194 | `	return PH7_OK;` |
|      2 | 2195 | `}` |
|      - | 2196 | `/*` |
|      - | 2197 | ` * int64 getmypid()` |
|      - | 2198 | ` *  Gets process ID.` |
|      - | 2199 | ` * Parameters` |
|      - | 2200 | ` *  None` |
|      - | 2201 | ` * Return` |
|      - | 2202 | ` *  Returns the process ID.` |
|      - | 2203 | ` */` |
|     74 | 2204 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2205 | `{` |
|      - | 2206 | `	ph7_int64 nProcessId;` |
|      - | 2207 | `	ph7_vfs *pVfs;` |
|      - | 2208 | `	/* Point to the underlying vfs */` |
|     76 | 2209 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     76 | 2210 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
|    ! 0 | 2211 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2212 | `		SXUNUSED(apArg);` |
|      - | 2213 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2214 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2215 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2216 | `			ph7_function_name(pCtx)` |
|      - | 2217 | `			);` |
|    ! 0 | 2218 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2219 | `		return PH7_OK;` |
|      - | 2220 | `	}` |
|      - | 2221 | `	/* Perform the requested operation */` |
|     76 | 2222 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 2223 | `	/* Set the result */` |
|     76 | 2224 | `	ph7_result_int64(pCtx,nProcessId);` |
|     76 | 2225 | `	return PH7_OK;` |
|     39 | 2226 | `}` |
|      - | 2227 | `/*` |
|      - | 2228 | ` * int getmyuid()` |
|      - | 2229 | ` *  Get user ID.` |
|      - | 2230 | ` * Parameters` |
|      - | 2231 | ` *  None` |
|      - | 2232 | ` * Return` |
|      - | 2233 | ` *  Returns the user ID.` |
|      - | 2234 | ` */` |
|      2 | 2235 | `static int PH7_vfs_getmyuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2236 | `{` |
|      - | 2237 | `	ph7_vfs *pVfs;` |
|      - | 2238 | `	int nUid;` |
|      - | 2239 | `	/* Point to the underlying vfs */` |
|      3 | 2240 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2241 | `	if( pVfs == 0 \|\| pVfs->xUid == 0 ){` |
|    ! 0 | 2242 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2243 | `		SXUNUSED(apArg);` |
|      - | 2244 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2245 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2246 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2247 | `			ph7_function_name(pCtx)` |
|      - | 2248 | `			);` |
|    ! 0 | 2249 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2250 | `		return PH7_OK;` |
|      - | 2251 | `	}` |
|      - | 2252 | `	/* Perform the requested operation */` |
|      3 | 2253 | `	nUid = pVfs->xUid();` |
|      - | 2254 | `	/* Set the result */` |
|      3 | 2255 | `	ph7_result_int(pCtx,nUid);` |
|      3 | 2256 | `	return PH7_OK;` |
|      2 | 2257 | `}` |
|      - | 2258 | `/*` |
|      - | 2259 | ` * int getmygid()` |
|      - | 2260 | ` *  Get group ID.` |
|      - | 2261 | ` * Parameters` |
|      - | 2262 | ` *  None` |
|      - | 2263 | ` * Return` |
|      - | 2264 | ` *  Returns the group ID.` |
|      - | 2265 | ` */` |
|      2 | 2266 | `static int PH7_vfs_getmygid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2267 | `{` |
|      - | 2268 | `	ph7_vfs *pVfs;` |
|      - | 2269 | `	int nGid;` |
|      - | 2270 | `	/* Point to the underlying vfs */` |
|      3 | 2271 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2272 | `	if( pVfs == 0 \|\| pVfs->xGid == 0 ){` |
|    ! 0 | 2273 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2274 | `		SXUNUSED(apArg);` |
|      - | 2275 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2276 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2277 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2278 | `			ph7_function_name(pCtx)` |
|      - | 2279 | `			);` |
|    ! 0 | 2280 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2281 | `		return PH7_OK;` |
|      - | 2282 | `	}` |
|      - | 2283 | `	/* Perform the requested operation */` |
|      3 | 2284 | `	nGid = pVfs->xGid();` |
|      - | 2285 | `	/* Set the result */` |
|      3 | 2286 | `	ph7_result_int(pCtx,nGid);` |
|      3 | 2287 | `	return PH7_OK;` |
|      2 | 2288 | `}` |
|      - | 2289 | `#ifdef __WINNT__` |
|      - | 2290 | `#include <Windows.h>` |
|      - | 2291 | `#elif defined(__UNIXES__)` |
|      - | 2292 | `#include <sys/utsname.h>` |
|      - | 2293 | `#endif` |
|      - | 2294 | `/*` |
|      - | 2295 | ` * string php_uname([ string $mode = "a" ])` |
|      - | 2296 | ` *  Returns information about the host operating system.` |
|      - | 2297 | ` * Parameters` |
|      - | 2298 | ` *  $mode` |
|      - | 2299 | ` *   mode is a single character that defines what information is returned:` |
|      - | 2300 | ` *    'a': This is the default. Contains all modes in the sequence "s n r v m".` |
|      - | 2301 | ` *    's': Operating system name. eg. FreeBSD.` |
|      - | 2302 | ` *    'n': Host name. eg. localhost.example.com.` |
|      - | 2303 | ` *    'r': Release name. eg. 5.1.2-RELEASE.` |
|      - | 2304 | ` *    'v': Version information. Varies a lot between operating systems.` |
|      - | 2305 | ` *    'm': Machine type. eg. i386.` |
|      - | 2306 | ` * Return` |
|      - | 2307 | ` *  OS description as a string.` |
|      - | 2308 | ` */` |
|      4 | 2309 | `static int PH7_vfs_ph7_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2310 | `{` |
|      - | 2311 | `#if defined(__WINNT__)` |
|      1 | 2312 | `	const char *zName = "Microsoft Windows";` |
|      - | 2313 | `	OSVERSIONINFOW sVer;` |
|      - | 2314 | `#elif defined(__UNIXES__)` |
|      - | 2315 | `	struct utsname sName;` |
|      - | 2316 | `#endif` |
|      5 | 2317 | `	const char *zMode = "a";` |
|      5 | 2318 | `	if( nArg > 0 && ph7_value_is_string(apArg[0]) ){` |
|      - | 2319 | `		/* Extract the desired mode */` |
|    ! 0 | 2320 | `		zMode = ph7_value_to_string(apArg[0],0);` |
|    ! 0 | 2321 | `	}` |
|      - | 2322 | `#if defined(__WINNT__)` |
|      1 | 2323 | `	sVer.dwOSVersionInfoSize = sizeof(sVer);` |
|      - | 2324 | `	/* GetVersionExW is deprecated in modern MSVC. Suppress deprecation for this call. */` |
|      - | 2325 | `#if defined(_MSC_VER)` |
|      - | 2326 | `#pragma warning(push)` |
|      - | 2327 | `#pragma warning(disable:4996)` |
|      - | 2328 | `#endif` |
|      1 | 2329 | `	if( TRUE != GetVersionExW(&sVer)){` |
|      - | 2330 | `#if defined(_MSC_VER)` |
|      - | 2331 | `#pragma warning(pop)` |
|      - | 2332 | `#endif` |
|    ! 0 | 2333 | `		ph7_result_string(pCtx,zName,-1);` |
|    ! 0 | 2334 | `		return PH7_OK;` |
|      - | 2335 | `	}` |
|      1 | 2336 | `	if( sVer.dwPlatformId == VER_PLATFORM_WIN32_NT ){` |
|      1 | 2337 | `		if( sVer.dwMajorVersion <= 4 ){` |
|    ! 0 | 2338 | `			zName = "Microsoft Windows NT";` |
|      1 | 2339 | `		}else if( sVer.dwMajorVersion == 5 ){` |
|    ! 0 | 2340 | `			switch(sVer.dwMinorVersion){` |
|    ! 0 | 2341 | `				case 0:	zName = "Microsoft Windows 2000"; break;` |
|    ! 0 | 2342 | `				case 1: zName = "Microsoft Windows XP";   break;` |
|    ! 0 | 2343 | `				case 2: zName = "Microsoft Windows Server 2003"; break;` |
|      - | 2344 | `			}` |
|    ! 0 | 2345 | `		}else if( sVer.dwMajorVersion == 6){` |
|      1 | 2346 | `				switch(sVer.dwMinorVersion){` |
|    ! 0 | 2347 | `					case 0: zName = "Microsoft Windows Vista"; break;` |
|    ! 0 | 2348 | `					case 1: zName = "Microsoft Windows 7"; break;` |
|      1 | 2349 | `					case 2: zName = "Microsoft Windows Server 2008"; break;` |
|    ! 0 | 2350 | `					case 3: zName = "Microsoft Windows 8"; break;` |
|      - | 2351 | `					default: break;` |
|      - | 2352 | `				}` |
|      - | 2353 | `		}` |
|      - | 2354 | `	}` |
|      1 | 2355 | `	switch(zMode[0]){` |
|      - | 2356 | `	case 's':` |
|      - | 2357 | `		/* Operating system name */` |
|    ! 0 | 2358 | `		ph7_result_string(pCtx,zName,-1/* Compute length automatically*/);` |
|    ! 0 | 2359 | `		break;` |
|      - | 2360 | `	case 'n':` |
|      - | 2361 | `		/* Host name */` |
|    ! 0 | 2362 | `		ph7_result_string(pCtx,"localhost",(int)sizeof("localhost")-1);` |
|    ! 0 | 2363 | `		break;` |
|      - | 2364 | `	case 'r':` |
|      - | 2365 | `	case 'v':` |
|      - | 2366 | `		/* Version information. */` |
|    ! 0 | 2367 | `		ph7_result_string_format(pCtx,"%u.%u build %u",` |
|      - | 2368 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2369 | `			);` |
|    ! 0 | 2370 | `		break;` |
|      - | 2371 | `	case 'm':` |
|      - | 2372 | `		/* Machine name */` |
|    ! 0 | 2373 | `		ph7_result_string(pCtx,"x86",(int)sizeof("x86")-1);` |
|    ! 0 | 2374 | `		break;` |
|      - | 2375 | `	default:` |
|      1 | 2376 | `		ph7_result_string_format(pCtx,"%s localhost %u.%u build %u x86",` |
|      - | 2377 | `			zName,` |
|      - | 2378 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2379 | `			);` |
|      - | 2380 | `		break;` |
|      - | 2381 | `	}` |
|      - | 2382 | `#elif defined(__UNIXES__)` |
|      4 | 2383 | `	if( uname(&sName) != 0 ){` |
|    ! 0 | 2384 | `		ph7_result_string(pCtx,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 | 2385 | `		return PH7_OK;` |
|      - | 2386 | `	}` |
|      4 | 2387 | `	switch(zMode[0]){` |
|    ! 0 | 2388 | `	case 's':` |
|      - | 2389 | `		/* Operating system name */` |
|    ! 0 | 2390 | `		ph7_result_string(pCtx,sName.sysname,-1/* Compute length automatically*/);` |
|    ! 0 | 2391 | `		break;` |
|    ! 0 | 2392 | `	case 'n':` |
|      - | 2393 | `		/* Host name */` |
|    ! 0 | 2394 | `		ph7_result_string(pCtx,sName.nodename,-1/* Compute length automatically*/);` |
|    ! 0 | 2395 | `		break;` |
|    ! 0 | 2396 | `	case 'r':` |
|      - | 2397 | `		/* Release information */` |
|    ! 0 | 2398 | `		ph7_result_string(pCtx,sName.release,-1/* Compute length automatically*/);` |
|    ! 0 | 2399 | `		break;` |
|    ! 0 | 2400 | `	case 'v':` |
|      - | 2401 | `		/* Version information. */` |
|    ! 0 | 2402 | `		ph7_result_string(pCtx,sName.version,-1/* Compute length automatically*/);` |
|    ! 0 | 2403 | `		break;` |
|    ! 0 | 2404 | `	case 'm':` |
|      - | 2405 | `		/* Machine name */` |
|    ! 0 | 2406 | `		ph7_result_string(pCtx,sName.machine,-1/* Compute length automatically*/);` |
|    ! 0 | 2407 | `		break;` |
|      2 | 2408 | `	default:` |
|      6 | 2409 | `		ph7_result_string_format(pCtx,` |
|      - | 2410 | `			"%s %s %s %s %s",` |
|      2 | 2411 | `			sName.sysname,` |
|      2 | 2412 | `			sName.release,` |
|      2 | 2413 | `			sName.version,` |
|      2 | 2414 | `			sName.nodename,` |
|      2 | 2415 | `			sName.machine` |
|      - | 2416 | `			);` |
|      4 | 2417 | `		break;` |
|      - | 2418 | `	}` |
|      - | 2419 | `#else` |
|      - | 2420 | `	ph7_result_string(pCtx,"Unknown Operating System",(int)sizeof("Unknown Operating System")-1);` |
|      - | 2421 | `#endif` |
|      5 | 2422 | `	return PH7_OK;` |
|      3 | 2423 | `}` |
|      - | 2424 | `/*` |
|      - | 2425 | ` * Section:` |
|      - | 2426 | ` *    IO stream implementation.` |
|      - | 2427 | ` * Status:` |
|      - | 2428 | ` *    Stable.` |
|      - | 2429 | ` */` |
|      - | 2430 | `typedef struct io_private io_private;` |
|      - | 2431 | `struct io_private` |
|      - | 2432 | `{` |
|      - | 2433 | `	const ph7_io_stream *pStream; /* Underlying IO device */` |
|      - | 2434 | `	void *pHandle; /* IO handle */` |
|      - | 2435 | `	/* Unbuffered IO */` |
|      - | 2436 | `	SyBlob sBuffer; /* Working buffer */` |
|      - | 2437 | `	sxu32 nOfft;    /* Current read offset */` |
|      - | 2438 | `	sxu32 iMagic;   /* Sanity check to avoid misuse */` |
|      - | 2439 | `};` |
|      - | 2440 | `#define IO_PRIVATE_MAGIC 0xFEAC14` |
|      - | 2441 | `/* Stream-device predicates (devices defined later in this file) */` |
|      - | 2442 | `static int is_php_stream(const ph7_io_stream *pStream);` |
|      - | 2443 | `static int is_data_stream(const ph7_io_stream *pStream);` |
|      - | 2444 | `/* Make sure we are dealing with a valid io_private instance */` |
|      - | 2445 | `#define IO_PRIVATE_INVALID(IO) ( IO == 0 \|\| IO->iMagic != IO_PRIVATE_MAGIC )` |
|      - | 2446 | `/* Forward declaration */` |
|      - | 2447 | `static void ResetIOPrivate(io_private *pDev);` |
|      - | 2448 | `/*` |
|      - | 2449 | ` * Return the PHP resource-type name for a raw resource handle.` |
|      - | 2450 | ` * Every IO handle this VFS hands out (fopen/tmpfile/popen/opendir and the` |
|      - | 2451 | ` * STDIN/STDOUT/STDERR constants) is an io_private, which PHP reports as` |
|      - | 2452 | ` * "stream"; anything else is "Unknown". The magic probe mirrors the` |
|      - | 2453 | ` * IO_PRIVATE_INVALID check the rest of this file uses to validate handles.` |
|      - | 2454 | ` */` |
|      4 | 2455 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      1 | 2456 | `{` |
|      5 | 2457 | `	io_private *pDev = (io_private *)pResource;` |
|      5 | 2458 | `	if( !IO_PRIVATE_INVALID(pDev) ){` |
|      5 | 2459 | `		return "stream";` |
|      - | 2460 | `	}` |
|    ! 0 | 2461 | `	return "Unknown";` |
|      3 | 2462 | `}` |
|      - | 2463 | `/*` |
|      - | 2464 | ` * bool ftruncate(resource $handle,int64 $size)` |
|      - | 2465 | ` *  Truncates a file to a given length.` |
|      - | 2466 | ` * Parameters` |
|      - | 2467 | ` *  $handle` |
|      - | 2468 | ` *   The file pointer.` |
|      - | 2469 | ` *   Note:` |
|      - | 2470 | ` *    The handle must be open for writing.` |
|      - | 2471 | ` * $size` |
|      - | 2472 | ` *   The size to truncate to.` |
|      - | 2473 | ` * Return` |
|      - | 2474 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2475 | ` */` |
|      6 | 2476 | `static int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2477 | `{` |
|      - | 2478 | `	const ph7_io_stream *pStream;` |
|      - | 2479 | `	io_private *pDev;` |
|      - | 2480 | `	int rc;` |
|      7 | 2481 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2482 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2483 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2484 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2485 | `		return PH7_OK;` |
|      - | 2486 | `	}` |
|      - | 2487 | `	/* Extract our private data */` |
|      7 | 2488 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2489 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 2490 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2491 | `		/*Expecting an IO handle */` |
|    ! 0 | 2492 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2493 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2494 | `		return PH7_OK;` |
|      - | 2495 | `	}` |
|      - | 2496 | `	/* Point to the target IO stream device */` |
|      7 | 2497 | `	pStream = pDev->pStream;` |
|      7 | 2498 | `	if( pStream == 0  \|\| pStream->xTrunc == 0){` |
|    ! 0 | 2499 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2500 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2501 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2502 | `			);` |
|    ! 0 | 2503 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2504 | `		return PH7_OK;` |
|      - | 2505 | `	}` |
|      - | 2506 | `	/* Perform the requested operation */` |
|      7 | 2507 | `	rc = pStream->xTrunc(pDev->pHandle,ph7_value_to_int64(apArg[1]));` |
|      7 | 2508 | `	if( rc == PH7_OK ){` |
|      - | 2509 | `		/* Discard buffered data */` |
|      7 | 2510 | `		ResetIOPrivate(pDev);` |
|      3 | 2511 | `	}` |
|      - | 2512 | `	/* IO result */` |
|      7 | 2513 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      7 | 2514 | `	return PH7_OK;` |
|      4 | 2515 | `}` |
|      - | 2516 | `/*` |
|      - | 2517 | ` * int fseek(resource $handle,int $offset[,int $whence = SEEK_SET ])` |
|      - | 2518 | ` *  Seeks on a file pointer.` |
|      - | 2519 | ` * Parameters` |
|      - | 2520 | ` *  $handle` |
|      - | 2521 | ` *   A file system pointer resource that is typically created using fopen().` |
|      - | 2522 | ` * $offset` |
|      - | 2523 | ` *   The offset.` |
|      - | 2524 | ` *   To move to a position before the end-of-file, you need to pass a negative` |
|      - | 2525 | ` *   value in offset and set whence to SEEK_END.` |
|      - | 2526 | ` *   whence` |
|      - | 2527 | ` *   whence values are:` |
|      - | 2528 | ` *    SEEK_SET - Set position equal to offset bytes.` |
|      - | 2529 | ` *    SEEK_CUR - Set position to current location plus offset.` |
|      - | 2530 | ` *    SEEK_END - Set position to end-of-file plus offset.` |
|      - | 2531 | ` * Return` |
|      - | 2532 | ` *  0 on success,-1 on failure` |
|      - | 2533 | ` */` |
|     10 | 2534 | `static int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2535 | `{` |
|      - | 2536 | `	const ph7_io_stream *pStream;` |
|      - | 2537 | `	io_private *pDev;` |
|      - | 2538 | `	ph7_int64 iOfft;` |
|      - | 2539 | `	int whence;` |
|      - | 2540 | `	int rc;` |
|     12 | 2541 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2542 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2543 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2544 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2545 | `		return PH7_OK;` |
|      - | 2546 | `	}` |
|      - | 2547 | `	/* Extract our private data */` |
|     12 | 2548 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2549 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     12 | 2550 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2551 | `		/*Expecting an IO handle */` |
|    ! 0 | 2552 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2553 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2554 | `		return PH7_OK;` |
|      - | 2555 | `	}` |
|      - | 2556 | `	/* Point to the target IO stream device */` |
|     12 | 2557 | `	pStream = pDev->pStream;` |
|     12 | 2558 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2559 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2560 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2561 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2562 | `			);` |
|    ! 0 | 2563 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2564 | `		return PH7_OK;` |
|      - | 2565 | `	}` |
|      - | 2566 | `	/* Extract the offset */` |
|     12 | 2567 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|     12 | 2568 | `	whence = 0;/* SEEK_SET */` |
|     12 | 2569 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      3 | 2570 | `		whence = ph7_value_to_int(apArg[2]);` |
|      1 | 2571 | `	}` |
|      - | 2572 | `	/* Perform the requested operation */` |
|     12 | 2573 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|     12 | 2574 | `	if( rc == PH7_OK ){` |
|      - | 2575 | `		/* Ignore buffered data */` |
|     12 | 2576 | `		ResetIOPrivate(pDev);` |
|      5 | 2577 | `	}` |
|      - | 2578 | `	/* IO result */` |
|     12 | 2579 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|     12 | 2580 | `	return PH7_OK;` |
|      7 | 2581 | `}` |
|      - | 2582 | `/*` |
|      - | 2583 | ` * int64 ftell(resource $handle)` |
|      - | 2584 | ` *  Returns the current position of the file read/write pointer.` |
|      - | 2585 | ` * Parameters` |
|      - | 2586 | ` *  $handle` |
|      - | 2587 | ` *   The file pointer.` |
|      - | 2588 | ` * Return` |
|      - | 2589 | ` *  Returns the position of the file pointer referenced by handle` |
|      - | 2590 | ` *  as an integer; i.e., its offset into the file stream.` |
|      - | 2591 | ` *  FALSE is returned on failure.` |
|      - | 2592 | ` */` |
|     12 | 2593 | `static int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2594 | `{` |
|      - | 2595 | `	const ph7_io_stream *pStream;` |
|      - | 2596 | `	io_private *pDev;` |
|      - | 2597 | `	ph7_int64 iOfft;` |
|     14 | 2598 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2599 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2600 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2601 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2602 | `		return PH7_OK;` |
|      - | 2603 | `	}` |
|      - | 2604 | `	/* Extract our private data */` |
|     14 | 2605 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2606 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     14 | 2607 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2608 | `		/*Expecting an IO handle */` |
|    ! 0 | 2609 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2610 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2611 | `		return PH7_OK;` |
|      - | 2612 | `	}` |
|      - | 2613 | `	/* Point to the target IO stream device */` |
|     14 | 2614 | `	pStream = pDev->pStream;` |
|     14 | 2615 | `	if( pStream == 0  \|\| pStream->xTell == 0){` |
|    ! 0 | 2616 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2617 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2618 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2619 | `			);` |
|    ! 0 | 2620 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2621 | `		return PH7_OK;` |
|      - | 2622 | `	}` |
|      - | 2623 | `	/* Perform the requested operation */` |
|     14 | 2624 | `	iOfft = pStream->xTell(pDev->pHandle);` |
|      - | 2625 | `	/* IO result */` |
|     14 | 2626 | `	ph7_result_int64(pCtx,iOfft);` |
|     14 | 2627 | `	return PH7_OK;` |
|      8 | 2628 | `}` |
|      - | 2629 | `/*` |
|      - | 2630 | ` * bool rewind(resource $handle)` |
|      - | 2631 | ` *  Rewind the position of a file pointer.` |
|      - | 2632 | ` * Parameters` |
|      - | 2633 | ` *  $handle` |
|      - | 2634 | ` *   The file pointer.` |
|      - | 2635 | ` * Return` |
|      - | 2636 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2637 | ` */` |
|     14 | 2638 | `static int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2639 | `{` |
|      - | 2640 | `	const ph7_io_stream *pStream;` |
|      - | 2641 | `	io_private *pDev;` |
|      - | 2642 | `	int rc;` |
|     15 | 2643 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2644 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2645 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2646 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2647 | `		return PH7_OK;` |
|      - | 2648 | `	}` |
|      - | 2649 | `	/* Extract our private data */` |
|     15 | 2650 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2651 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     15 | 2652 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2653 | `		/*Expecting an IO handle */` |
|    ! 0 | 2654 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2655 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2656 | `		return PH7_OK;` |
|      - | 2657 | `	}` |
|      - | 2658 | `	/* Point to the target IO stream device */` |
|     15 | 2659 | `	pStream = pDev->pStream;` |
|     15 | 2660 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2661 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2662 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2663 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2664 | `			);` |
|    ! 0 | 2665 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2666 | `		return PH7_OK;` |
|      - | 2667 | `	}` |
|      - | 2668 | `	/* Perform the requested operation */` |
|     15 | 2669 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|     15 | 2670 | `	if( rc == PH7_OK ){` |
|      - | 2671 | `		/* Ignore buffered data */` |
|     15 | 2672 | `		ResetIOPrivate(pDev);` |
|      7 | 2673 | `	}` |
|      - | 2674 | `	/* IO result */` |
|     15 | 2675 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     15 | 2676 | `	return PH7_OK;` |
|      8 | 2677 | `}` |
|      - | 2678 | `/*` |
|      - | 2679 | ` * bool fflush(resource $handle)` |
|      - | 2680 | ` *  Flushes the output to a file.` |
|      - | 2681 | ` * Parameters` |
|      - | 2682 | ` *  $handle` |
|      - | 2683 | ` *   The file pointer.` |
|      - | 2684 | ` * Return` |
|      - | 2685 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2686 | ` */` |
|      2 | 2687 | `static int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2688 | `{` |
|      - | 2689 | `	const ph7_io_stream *pStream;` |
|      - | 2690 | `	io_private *pDev;` |
|      - | 2691 | `	int rc;` |
|      3 | 2692 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2693 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2694 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2695 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2696 | `		return PH7_OK;` |
|      - | 2697 | `	}` |
|      - | 2698 | `	/* Extract our private data */` |
|      3 | 2699 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2700 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 2701 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2702 | `		/*Expecting an IO handle */` |
|    ! 0 | 2703 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2704 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2705 | `		return PH7_OK;` |
|      - | 2706 | `	}` |
|      - | 2707 | `	/* Point to the target IO stream device */` |
|      3 | 2708 | `	pStream = pDev->pStream;` |
|      3 | 2709 | `	if( pStream == 0 \|\| pStream->xSync == 0){` |
|    ! 0 | 2710 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2711 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2712 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2713 | `			);` |
|    ! 0 | 2714 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2715 | `		return PH7_OK;` |
|      - | 2716 | `	}` |
|      - | 2717 | `	/* Perform the requested operation */` |
|      3 | 2718 | `	rc = pStream->xSync(pDev->pHandle);` |
|      - | 2719 | `	/* IO result */` |
|      3 | 2720 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 2721 | `	return PH7_OK;` |
|      2 | 2722 | `}` |
|      - | 2723 | `/*` |
|      - | 2724 | ` * bool feof(resource $handle)` |
|      - | 2725 | ` *  Tests for end-of-file on a file pointer.` |
|      - | 2726 | ` * Parameters` |
|      - | 2727 | ` *  $handle` |
|      - | 2728 | ` *   The file pointer.` |
|      - | 2729 | ` * Return` |
|      - | 2730 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|      - | 2731 | ` */` |
|  10198 | 2732 | `static int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2733 | `{` |
|      - | 2734 | `	const ph7_io_stream *pStream;` |
|      - | 2735 | `	io_private *pDev;` |
|      - | 2736 | `	int rc;` |
|  10203 | 2737 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2738 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 2739 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2740 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2741 | `		return PH7_OK;` |
|      - | 2742 | `	}` |
|      - | 2743 | `	/* Extract our private data */` |
|  10203 | 2744 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2745 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  10203 | 2746 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2747 | `		/*Expecting an IO handle */` |
|    ! 0 | 2748 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2749 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2750 | `		return PH7_OK;` |
|      - | 2751 | `	}` |
|      - | 2752 | `	/* Point to the target IO stream device */` |
|  10203 | 2753 | `	pStream = pDev->pStream;` |
|  10203 | 2754 | `	if( pStream == 0 ){` |
|    ! 0 | 2755 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2756 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2757 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2758 | `			);` |
|    ! 0 | 2759 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2760 | `		return PH7_OK;` |
|      - | 2761 | `	}` |
|  10203 | 2762 | `	rc = SXERR_EOF;` |
|      - | 2763 | `	/* Perform the requested operation */` |
|  10203 | 2764 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2765 | `		/* Data is available */` |
|   4557 | 2766 | `		rc = PH7_OK;` |
|   2281 | 2767 | `	}else{` |
|      - | 2768 | `		char zBuf[4096];` |
|      - | 2769 | `		ph7_int64 n;` |
|      - | 2770 | `		/* Perform a buffered read */` |
|   5651 | 2771 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|   5651 | 2772 | `		if( n > 0 ){` |
|      - | 2773 | `			/* Copy buffered data */` |
|   1837 | 2774 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|   1837 | 2775 | `			rc = PH7_OK;` |
|    916 | 2776 | `		}` |
|      - | 2777 | `	}` |
|      - | 2778 | `	/* EOF or not */` |
|  10203 | 2779 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|  10203 | 2780 | `	return PH7_OK;` |
|   5104 | 2781 | `}` |
|      - | 2782 | `/*` |
|      - | 2783 | ` * Read n bytes from the underlying IO stream device.` |
|      - | 2784 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|      - | 2785 | ` * [i.e: IO error ] or EOF.` |
|      - | 2786 | ` */` |
|     36 | 2787 | `static ph7_int64 StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      2 | 2788 | `{` |
|     38 | 2789 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|     38 | 2790 | `	char *zBuf = (char *)pBuf;` |
|      - | 2791 | `	ph7_int64 n,nRead;` |
|     38 | 2792 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     38 | 2793 | `	if( n > 0 ){` |
|      2 | 2794 | `		if( n > nLen ){` |
|    ! 0 | 2795 | `			n = nLen;` |
|    ! 0 | 2796 | `		}` |
|      - | 2797 | `		/* Copy the buffered data */` |
|      2 | 2798 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|      - | 2799 | `		/* Update the read offset */` |
|      2 | 2800 | `		pDev->nOfft += (sxu32)n;` |
|      2 | 2801 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2802 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|      2 | 2803 | `			SyBlobReset(&pDev->sBuffer);` |
|      2 | 2804 | `			pDev->nOfft = 0;` |
|      1 | 2805 | `		}` |
|      2 | 2806 | `		nLen -= n;` |
|      2 | 2807 | `		if( nLen < 1 ){` |
|      - | 2808 | `			/* All done */` |
|    ! 0 | 2809 | `			return n;` |
|      - | 2810 | `		}` |
|      - | 2811 | `		/* Advance the cursor */` |
|      2 | 2812 | `		zBuf += n;` |
|      1 | 2813 | `	}` |
|      - | 2814 | `	/* Read without buffering */` |
|     38 | 2815 | `	nRead = pStream->xRead(pDev->pHandle,zBuf,nLen);` |
|     38 | 2816 | `	if( nRead > 0 ){` |
|     35 | 2817 | `		n += nRead;` |
|     20 | 2818 | `	}else if( n < 1 ){` |
|      - | 2819 | `		/* EOF or IO error */` |
|      3 | 2820 | `		return nRead;` |
|      - | 2821 | `	}` |
|     36 | 2822 | `	return n;` |
|     20 | 2823 | `}` |
|      - | 2824 | `/*` |
|      - | 2825 | ` * Extract a single line from the buffered input.` |
|      - | 2826 | ` */` |
|   6450 | 2827 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      5 | 2828 | `{` |
|      - | 2829 | `	const char *zIn,*zEnd,*zPtr;` |
|   6455 | 2830 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|   6455 | 2831 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|   6455 | 2832 | `	zPtr = zIn;` |
| 387031 | 2833 | `	while( zIn < zEnd ){` |
| 386931 | 2834 | `		if( zIn[0] == '\n' ){` |
|      - | 2835 | `			/* Line found */` |
|   6355 | 2836 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|   6355 | 2837 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|   6355 | 2838 | `			*pzLine = zPtr;` |
|   6355 | 2839 | `			return SXRET_OK;` |
|      - | 2840 | `		}` |
| 380581 | 2841 | `		zIn++;` |
|      5 | 2842 | `	}` |
|      - | 2843 | `	/* No line were found */` |
|    105 | 2844 | `	return SXERR_NOTFOUND;` |
|   3230 | 2845 | `}` |
|      - | 2846 | `/*` |
|      - | 2847 | ` * Read a single line from the underlying IO stream device.` |
|      - | 2848 | ` */` |
|   6454 | 2849 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      5 | 2850 | `{` |
|   6459 | 2851 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - | 2852 | `	char zBuf[8192];` |
|      - | 2853 | `	ph7_int64 n;` |
|      - | 2854 | `	sxi32 rc;` |
|   6459 | 2855 | `	n = 0;` |
|   6459 | 2856 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2857 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     73 | 2858 | `		SyBlobReset(&pDev->sBuffer);` |
|     73 | 2859 | `		pDev->nOfft = 0;` |
|     34 | 2860 | `	}` |
|   6459 | 2861 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2862 | `		/* Check if there is a line */` |
|   6391 | 2863 | `		rc = GetLine(pDev,&n,pzData);` |
|   6391 | 2864 | `		if( rc == SXRET_OK ){` |
|      - | 2865 | `			/* Got line,update the cursor  */` |
|   6295 | 2866 | `			pDev->nOfft += (sxu32)n;` |
|   6295 | 2867 | `			return n;` |
|      - | 2868 | `		}` |
|     48 | 2869 | `	}` |
|      - | 2870 | `	/* Perform the read operation until a new line is extracted or length` |
|      - | 2871 | `	 * limit is reached.` |
|      - | 2872 | `	 */` |
|     84 | 2873 | `	for(;;){` |
|    173 | 2874 | `		n = pStream->xRead(pDev->pHandle,zBuf, (nMaxLen > 0 && nMaxLen < (ph7_int64)sizeof(zBuf)) ? nMaxLen : (ph7_int64)sizeof(zBuf));` |
|    173 | 2875 | `		if( n < 1 ){` |
|      - | 2876 | `			/* EOF or IO error */` |
|    109 | 2877 | `			break;` |
|      - | 2878 | `		}` |
|      - | 2879 | `		/* Append the data just read */` |
|     67 | 2880 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - | 2881 | `		/* Try to extract a line */` |
|     67 | 2882 | `		rc = GetLine(pDev,&n,pzData);` |
|     67 | 2883 | `		if( rc == SXRET_OK ){` |
|      - | 2884 | `			/* Got one,return immediately */` |
|     63 | 2885 | `			pDev->nOfft += (sxu32)n;` |
|     63 | 2886 | `			return n;` |
|      - | 2887 | `		}` |
|      5 | 2888 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
|      - | 2889 | `			/* Read limit reached,return the available data */` |
|    ! 0 | 2890 | `			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    ! 0 | 2891 | `			n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 2892 | `			/* Reset the working buffer */` |
|    ! 0 | 2893 | `			SyBlobReset(&pDev->sBuffer);` |
|    ! 0 | 2894 | `			pDev->nOfft = 0;` |
|    ! 0 | 2895 | `			return n;` |
|      - | 2896 | `		}` |
|      1 | 2897 | `	}` |
|    109 | 2898 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2899 | `		/* Read limit reached,return the available data */` |
|    105 | 2900 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    105 | 2901 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 2902 | `		/* Reset the working buffer */` |
|    105 | 2903 | `		SyBlobReset(&pDev->sBuffer);` |
|    105 | 2904 | `		pDev->nOfft = 0;` |
|     50 | 2905 | `	}` |
|    109 | 2906 | `	return n;` |
|   3232 | 2907 | `}` |
|      - | 2908 | `/*` |
|      - | 2909 | ` * Open an IO stream handle.` |
|      - | 2910 | ` * Notes on stream:` |
|      - | 2911 | ` * According to the PHP reference manual.` |
|      - | 2912 | ` * In its simplest definition, a stream is a resource object which exhibits streamable behavior.` |
|      - | 2913 | ` * That is, it can be read from or written to in a linear fashion, and may be able to fseek()` |
|      - | 2914 | ` * to an arbitrary locations within the stream.` |
|      - | 2915 | ` * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.` |
|      - | 2916 | ` * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file` |
|      - | 2917 | ` * on a remote server.` |
|      - | 2918 | ` * A stream is referenced as: scheme://target` |
|      - | 2919 | ` *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...` |
|      - | 2920 | ` *   If no wrapper is specified, the function default is used (typically file://).` |
|      - | 2921 | ` *   target - Depends on the wrapper used. For filesystem related streams this is typically a path` |
|      - | 2922 | ` *  and filename of the desired file. For network related streams this is typically a hostname, often` |
|      - | 2923 | ` *  with a path appended.` |
|      - | 2924 | ` *` |
|      - | 2925 | ` * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.` |
|      - | 2926 | ` * Please refer to the official documentation for a full discussion.` |
|      - | 2927 | ` * This function return a handle on success. Otherwise null.` |
|      - | 2928 | ` */` |
|  29452 | 2929 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - | 2930 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      5 | 2931 | `{` |
|  29457 | 2932 | `	void *pHandle = 0; /* cc warning */` |
|      - | 2933 | `	SyString sFile;` |
|      - | 2934 | `	ph7_value sDummy;` |
|      - | 2935 | `	int rc;` |
|  29457 | 2936 | `	if( pStream == 0 ){` |
|      - | 2937 | `		/* No such stream device */` |
|    ! 0 | 2938 | `		return 0;` |
|      - | 2939 | `	}` |
|  29457 | 2940 | `	if( pResource == 0 ){` |
|      - | 2941 | `		/* VM-dependent devices (php://, data://, tcp://, userland wrappers)` |
|      - | 2942 | `		 * reach the VM only through pResource->pVm — their xOpen has no vm` |
|      - | 2943 | `		 * parameter. Callers like file_get_contents pass no resource, so hand` |
|      - | 2944 | `		 * every device a synthesized stack value carrying the VM; xOpen only` |
|      - | 2945 | `		 * reads it during the call, and file:// ignores it. */` |
|  29439 | 2946 | `		PH7_MemObjInit(pVm,&sDummy);` |
|  29439 | 2947 | `		pResource = &sDummy;` |
|  14717 | 2948 | `	}` |
|  29457 | 2949 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  29457 | 2950 | `	if( use_include ){` |
|   9430 | 2951 | `		if(	sFile.zString[0] == '/' \|\|` |
|      - | 2952 | `#ifdef __WINNT__` |
|      - | 2953 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - | 2954 | `#endif` |
|   9416 | 2955 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   9412 | 2956 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - | 2957 | `				/*  Open the file directly */` |
|     19 | 2958 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|     10 | 2959 | `		}else{` |
|      - | 2960 | `			SyString *pPath;` |
|      - | 2961 | `			SyBlob sWorker;` |
|      - | 2962 | `#ifdef __WINNT__` |
|      - | 2963 | `			static const int c = '\\';` |
|      - | 2964 | `#else` |
|      - | 2965 | `			static const int c = '/';` |
|      - | 2966 | `#endif` |
|      - | 2967 | `			/* Init the path builder working buffer */` |
|   9416 | 2968 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - | 2969 | `			/* Build a path from the set of include path */` |
|   9416 | 2970 | `			SySetResetCursor(&pVm->aPaths);` |
|   9416 | 2971 | `			rc = SXERR_IO;` |
|   9422 | 2972 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - | 2973 | `				/* Build full path */` |
|   9416 | 2974 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - | 2975 | `				/* Append null terminator */` |
|   9416 | 2976 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 2977 | `					continue;` |
|      - | 2978 | `				}` |
|      - | 2979 | `				/* Try to open the file */` |
|   9416 | 2980 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   9416 | 2981 | `				if( rc == PH7_OK ){` |
|   9409 | 2982 | `					if( bPushInclude ){` |
|      - | 2983 | `						/* Mark as included */` |
|   9409 | 2984 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   4703 | 2985 | `					}` |
|   9409 | 2986 | `					break;` |
|      - | 2987 | `				}` |
|      - | 2988 | `				/* Reset the working buffer */` |
|      8 | 2989 | `				SyBlobReset(&sWorker);` |
|      - | 2990 | `				/* Check the next path */` |
|      2 | 2991 | `			}` |
|   9416 | 2992 | `			SyBlobRelease(&sWorker);` |
|      - | 2993 | `		}` |
|   9434 | 2994 | `		if( rc == PH7_OK ){` |
|   9427 | 2995 | `			if( bPushInclude ){` |
|      - | 2996 | `				/* Mark as included */` |
|   9427 | 2997 | `				PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|   4712 | 2998 | `			}` |
|   4712 | 2999 | `		}` |
|   4719 | 3000 | `	}else{` |
|      - | 3001 | `		/* Open the URI direcly */` |
|  20027 | 3002 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - | 3003 | `	}` |
|  29457 | 3004 | `	if( rc != PH7_OK ){` |
|      - | 3005 | `		/* IO error */` |
|     17 | 3006 | `		return 0;` |
|      - | 3007 | `	}` |
|      - | 3008 | `	/* Return the file handle */` |
|  29445 | 3009 | `	return pHandle;` |
|  14731 | 3010 | `}` |
|      - | 3011 | `/*` |
|      - | 3012 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - | 3013 | ` * Store the read data in the given BLOB (last argument).` |
|      - | 3014 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - | 3015 | ` */` |
|   9418 | 3016 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      3 | 3017 | `{` |
|      - | 3018 | `	ph7_int64 nRead;` |
|      - | 3019 | `	char zBuf[8192]; /* 8K */` |
|      - | 3020 | `	int rc;` |
|      - | 3021 | `	/* Perform the requested operation */` |
|   9418 | 3022 | `	for(;;){` |
|  18839 | 3023 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  18839 | 3024 | `		if( nRead < 1 ){` |
|      - | 3025 | `			/* EOF or IO error */` |
|   9421 | 3026 | `			break;` |
|      - | 3027 | `		}` |
|      - | 3028 | `		/* Append contents */` |
|   9421 | 3029 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   9421 | 3030 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3031 | `			break;` |
|      - | 3032 | `		}` |
|      3 | 3033 | `	}` |
|   9421 | 3034 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      3 | 3035 | `}` |
|      - | 3036 | `/*` |
|      - | 3037 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - | 3038 | ` */` |
|  29538 | 3039 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      5 | 3040 | `{` |
|  29543 | 3041 | `	if( pStream->xClose ){` |
|  29543 | 3042 | `		pStream->xClose(pHandle);` |
|  14769 | 3043 | `	}` |
|  29543 | 3044 | `}` |
|      - | 3045 | `/*` |
|      - | 3046 | ` * string fgetc(resource $handle)` |
|      - | 3047 | ` *  Gets a character from the given file pointer.` |
|      - | 3048 | ` * Parameters` |
|      - | 3049 | ` *  $handle` |
|      - | 3050 | ` *   The file pointer.` |
|      - | 3051 | ` * Return` |
|      - | 3052 | ` *  Returns a string containing a single character read from the file` |
|      - | 3053 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|      - | 3054 | ` * WARNING` |
|      - | 3055 | ` *  This operation is extremely slow.Avoid using it.` |
|      - | 3056 | ` */` |
|      4 | 3057 | `static int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3058 | `{` |
|      - | 3059 | `	const ph7_io_stream *pStream;` |
|      - | 3060 | `	io_private *pDev;` |
|      - | 3061 | `	int c,n;` |
|      5 | 3062 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3063 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3064 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3065 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3066 | `		return PH7_OK;` |
|      - | 3067 | `	}` |
|      - | 3068 | `	/* Extract our private data */` |
|      5 | 3069 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3070 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 3071 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3072 | `		/*Expecting an IO handle */` |
|    ! 0 | 3073 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3074 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3075 | `		return PH7_OK;` |
|      - | 3076 | `	}` |
|      - | 3077 | `	/* Point to the target IO stream device */` |
|      5 | 3078 | `	pStream = pDev->pStream;` |
|      5 | 3079 | `	if( pStream == 0  ){` |
|    ! 0 | 3080 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3081 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3082 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3083 | `			);` |
|    ! 0 | 3084 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3085 | `		return PH7_OK;` |
|      - | 3086 | `	}` |
|      - | 3087 | `	/* Perform the requested operation */` |
|      5 | 3088 | `	n = (int)StreamRead(pDev,(void *)&c,sizeof(char));` |
|      - | 3089 | `	/* IO result */` |
|      5 | 3090 | `	if( n < 1 ){` |
|      - | 3091 | `		/* EOF or error,return FALSE */` |
|    ! 0 | 3092 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3093 | `	}else{` |
|      - | 3094 | `		/* Return the string holding the character */` |
|      5 | 3095 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|      - | 3096 | `	}` |
|      5 | 3097 | `	return PH7_OK;` |
|      3 | 3098 | `}` |
|      - | 3099 | `/*` |
|      - | 3100 | ` * string fgets(resource $handle[,int64 $length ])` |
|      - | 3101 | ` *  Gets line from file pointer.` |
|      - | 3102 | ` * Parameters` |
|      - | 3103 | ` *  $handle` |
|      - | 3104 | ` *   The file pointer.` |
|      - | 3105 | ` * $length` |
|      - | 3106 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3107 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3108 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3109 | ` *  the end of the line.` |
|      - | 3110 | ` * Return` |
|      - | 3111 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3112 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3113 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3114 | ` */` |
|   6444 | 3115 | `static int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3116 | `{` |
|      - | 3117 | `	const ph7_io_stream *pStream;` |
|      - | 3118 | `	const char *zLine;` |
|      - | 3119 | `	io_private *pDev;` |
|      - | 3120 | `	ph7_int64 n,nLen;` |
|   6449 | 3121 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3122 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3123 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3124 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3125 | `		return PH7_OK;` |
|      - | 3126 | `	}` |
|      - | 3127 | `	/* Extract our private data */` |
|   6449 | 3128 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3129 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   6449 | 3130 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3131 | `		/*Expecting an IO handle */` |
|    ! 0 | 3132 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3133 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3134 | `		return PH7_OK;` |
|      - | 3135 | `	}` |
|      - | 3136 | `	/* Point to the target IO stream device */` |
|   6449 | 3137 | `	pStream = pDev->pStream;` |
|   6449 | 3138 | `	if( pStream == 0  ){` |
|    ! 0 | 3139 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3140 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3141 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3142 | `			);` |
|    ! 0 | 3143 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3144 | `		return PH7_OK;` |
|      - | 3145 | `	}` |
|   6449 | 3146 | `	nLen = -1;` |
|   6449 | 3147 | `	if( nArg > 1 ){` |
|      - | 3148 | `		/* Maximum data to read */` |
|    ! 0 | 3149 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3150 | `	}` |
|      - | 3151 | `	/* Perform the requested operation */` |
|   6449 | 3152 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|   6449 | 3153 | `	if( n < 1 ){` |
|      - | 3154 | `		/* EOF or IO error,return FALSE */` |
|      7 | 3155 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3156 | `	}else{` |
|      - | 3157 | `		/* Return the freshly extracted line */` |
|   6447 | 3158 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - | 3159 | `	}` |
|   6449 | 3160 | `	return PH7_OK;` |
|   3227 | 3161 | `}` |
|      - | 3162 | `/*` |
|      - | 3163 | ` * string fread(resource $handle,int64 $length)` |
|      - | 3164 | ` *  Binary-safe file read.` |
|      - | 3165 | ` * Parameters` |
|      - | 3166 | ` *  $handle` |
|      - | 3167 | ` *   The file pointer.` |
|      - | 3168 | ` * $length` |
|      - | 3169 | ` *  Up to length number of bytes read.` |
|      - | 3170 | ` * Return` |
|      - | 3171 | ` *  The data readen on success or FALSE on failure.` |
|      - | 3172 | ` */` |
|     28 | 3173 | `static int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3174 | `{` |
|      - | 3175 | `	const ph7_io_stream *pStream;` |
|      - | 3176 | `	io_private *pDev;` |
|      - | 3177 | `	ph7_int64 nRead;` |
|      - | 3178 | `	void *pBuf;` |
|      - | 3179 | `	int nLen;` |
|     30 | 3180 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3181 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3182 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3183 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3184 | `		return PH7_OK;` |
|      - | 3185 | `	}` |
|      - | 3186 | `	/* Extract our private data */` |
|     30 | 3187 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3188 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     30 | 3189 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3190 | `		/*Expecting an IO handle */` |
|    ! 0 | 3191 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3192 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3193 | `		return PH7_OK;` |
|      - | 3194 | `	}` |
|      - | 3195 | `	/* Point to the target IO stream device */` |
|     30 | 3196 | `	pStream = pDev->pStream;` |
|     30 | 3197 | `	if( pStream == 0  ){` |
|    ! 0 | 3198 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3199 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3200 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3201 | `			);` |
|    ! 0 | 3202 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3203 | `		return PH7_OK;` |
|      - | 3204 | `	}` |
|     30 | 3205 | `        nLen = 4096;` |
|     30 | 3206 | `	if( nArg > 1 ){` |
|     30 | 3207 | ` 	  nLen = ph7_value_to_int(apArg[1]);` |
|     30 | 3208 | `	  if( nLen < 1 ){` |
|      - | 3209 | `		/* Invalid length,set a default length */` |
|    ! 0 | 3210 | `		nLen = 4096;` |
|    ! 0 | 3211 | `	  }` |
|     14 | 3212 | `        }` |
|      - | 3213 | `	/* Allocate enough buffer */` |
|     30 | 3214 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     30 | 3215 | `	if( pBuf == 0 ){` |
|    ! 0 | 3216 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3217 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3218 | `		return PH7_OK;` |
|      - | 3219 | `	}` |
|      - | 3220 | `	/* Perform the requested operation */` |
|     30 | 3221 | `	nRead = StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     30 | 3222 | `	if( nRead < 1 ){` |
|      - | 3223 | `		/* Nothing read,return FALSE */` |
|    ! 0 | 3224 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3225 | `	}else{` |
|      - | 3226 | `		/* Make a copy of the data just read */` |
|     30 | 3227 | `		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);` |
|      - | 3228 | `	}` |
|      - | 3229 | `	/* Release the buffer */` |
|     30 | 3230 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     30 | 3231 | `	return PH7_OK;` |
|     16 | 3232 | `}` |
|      - | 3233 | `/*` |
|      - | 3234 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|      - | 3235 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|      - | 3236 | ` * Gets line from file pointer and parse for CSV fields.` |
|      - | 3237 | ` * Parameters` |
|      - | 3238 | ` * $handle` |
|      - | 3239 | ` *   The file pointer.` |
|      - | 3240 | ` * $length` |
|      - | 3241 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3242 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3243 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3244 | ` *  the end of the line.` |
|      - | 3245 | ` * $delimiter` |
|      - | 3246 | ` *   Set the field delimiter (one character only).` |
|      - | 3247 | ` * $enclosure` |
|      - | 3248 | ` *   Set the field enclosure character (one character only).` |
|      - | 3249 | ` * $escape` |
|      - | 3250 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 3251 | ` * Return` |
|      - | 3252 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3253 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3254 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3255 | ` */` |
|      2 | 3256 | `static int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3257 | `{` |
|      - | 3258 | `	const ph7_io_stream *pStream;` |
|      - | 3259 | `	const char *zLine;` |
|      - | 3260 | `	io_private *pDev;` |
|      - | 3261 | `	ph7_int64 n,nLen;` |
|      3 | 3262 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3263 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3264 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3265 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3266 | `		return PH7_OK;` |
|      - | 3267 | `	}` |
|      - | 3268 | `	/* Extract our private data */` |
|      3 | 3269 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3270 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3271 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3272 | `		/*Expecting an IO handle */` |
|    ! 0 | 3273 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3274 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3275 | `		return PH7_OK;` |
|      - | 3276 | `	}` |
|      - | 3277 | `	/* Point to the target IO stream device */` |
|      3 | 3278 | `	pStream = pDev->pStream;` |
|      3 | 3279 | `	if( pStream == 0  ){` |
|    ! 0 | 3280 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3281 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3282 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3283 | `			);` |
|    ! 0 | 3284 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3285 | `		return PH7_OK;` |
|      - | 3286 | `	}` |
|      3 | 3287 | `	nLen = -1;` |
|      3 | 3288 | `	if( nArg > 1 ){` |
|      - | 3289 | `		/* Maximum data to read */` |
|      3 | 3290 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      1 | 3291 | `	}` |
|      - | 3292 | `	/* Perform the requested operation */` |
|      3 | 3293 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3294 | `	if( n < 1 ){` |
|      - | 3295 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3296 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3297 | `	}else{` |
|      - | 3298 | `		ph7_value *pArray;` |
|      3 | 3299 | `		int delim  = ',';   /* Delimiter */` |
|      3 | 3300 | `		int encl   = '"' ;  /* Enclosure */` |
|      3 | 3301 | `		int escape = '\\';  /* Escape character */` |
|      3 | 3302 | `		if( nArg > 2 ){` |
|      - | 3303 | `			const char *zPtr;` |
|      - | 3304 | `			int i;` |
|      3 | 3305 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 3306 | `				/* Extract the delimiter */` |
|      3 | 3307 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 3308 | `				if( i > 0 ){` |
|      3 | 3309 | `					delim = zPtr[0];` |
|      1 | 3310 | `				}` |
|      1 | 3311 | `			}` |
|      3 | 3312 | `			if( nArg > 3 ){` |
|      3 | 3313 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 3314 | `					/* Extract the enclosure */` |
|      3 | 3315 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 3316 | `					if( i > 0 ){` |
|      3 | 3317 | `						encl = zPtr[0];` |
|      1 | 3318 | `					}` |
|      1 | 3319 | `				}` |
|      3 | 3320 | `				if( nArg > 4 ){` |
|      3 | 3321 | `					if( ph7_value_is_string(apArg[4]) ){` |
|      - | 3322 | `						/* Extract the escape character */` |
|      3 | 3323 | `						zPtr = ph7_value_to_string(apArg[4],&i);` |
|      3 | 3324 | `						if( i > 0 ){` |
|      3 | 3325 | `							escape = zPtr[0];` |
|      1 | 3326 | `						}` |
|      1 | 3327 | `					}` |
|      1 | 3328 | `				}` |
|      1 | 3329 | `			}` |
|      1 | 3330 | `		}` |
|      - | 3331 | `		/* Create our array */` |
|      3 | 3332 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 3333 | `		if( pArray == 0 ){` |
|    ! 0 | 3334 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3335 | `			ph7_result_null(pCtx);` |
|    ! 0 | 3336 | `			return PH7_OK;` |
|      - | 3337 | `		}` |
|      - | 3338 | `		/* Parse the raw input */` |
|      3 | 3339 | `		PH7_ProcessCsv(zLine,(int)n,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 3340 | `		/* Return the freshly created array  */` |
|      3 | 3341 | `		ph7_result_value(pCtx,pArray);` |
|      - | 3342 | `	}` |
|      3 | 3343 | `	return PH7_OK;` |
|      2 | 3344 | `}` |
|      - | 3345 | `/*` |
|      - | 3346 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|      - | 3347 | ` *  Gets line from file pointer and strip HTML tags.` |
|      - | 3348 | ` * Parameters` |
|      - | 3349 | ` * $handle` |
|      - | 3350 | ` *   The file pointer.` |
|      - | 3351 | ` * $length` |
|      - | 3352 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3353 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3354 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3355 | ` *  the end of the line.` |
|      - | 3356 | ` * $allowable_tags` |
|      - | 3357 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 3358 | ` * Return` |
|      - | 3359 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|      - | 3360 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|      - | 3361 | ` */` |
|      2 | 3362 | `static int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3363 | `{` |
|      - | 3364 | `	const ph7_io_stream *pStream;` |
|      - | 3365 | `	const char *zLine;` |
|      - | 3366 | `	io_private *pDev;` |
|      - | 3367 | `	ph7_int64 n,nLen;` |
|      3 | 3368 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3369 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3370 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3371 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3372 | `		return PH7_OK;` |
|      - | 3373 | `	}` |
|      - | 3374 | `	/* Extract our private data */` |
|      3 | 3375 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3376 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3377 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3378 | `		/*Expecting an IO handle */` |
|    ! 0 | 3379 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3380 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3381 | `		return PH7_OK;` |
|      - | 3382 | `	}` |
|      - | 3383 | `	/* Point to the target IO stream device */` |
|      3 | 3384 | `	pStream = pDev->pStream;` |
|      3 | 3385 | `	if( pStream == 0  ){` |
|    ! 0 | 3386 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3387 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3388 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3389 | `			);` |
|    ! 0 | 3390 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3391 | `		return PH7_OK;` |
|      - | 3392 | `	}` |
|      3 | 3393 | `	nLen = -1;` |
|      3 | 3394 | `	if( nArg > 1 ){` |
|      - | 3395 | `		/* Maximum data to read */` |
|    ! 0 | 3396 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3397 | `	}` |
|      - | 3398 | `	/* Perform the requested operation */` |
|      3 | 3399 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3400 | `	if( n < 1 ){` |
|      - | 3401 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3402 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3403 | `	}else{` |
|      3 | 3404 | `		const char *zTaglist = 0;` |
|      3 | 3405 | `		int nTaglen = 0;` |
|      3 | 3406 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|      - | 3407 | `			/* Allowed tag */` |
|    ! 0 | 3408 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|    ! 0 | 3409 | `		}` |
|      - | 3410 | `		/* Process data just read */` |
|      3 | 3411 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen);` |
|      - | 3412 | `	}` |
|      3 | 3413 | `	return PH7_OK;` |
|      2 | 3414 | `}` |
|      - | 3415 | `/*` |
|      - | 3416 | ` * string readdir(resource $dir_handle)` |
|      - | 3417 | ` *   Read entry from directory handle.` |
|      - | 3418 | ` * Parameter` |
|      - | 3419 | ` *  $dir_handle` |
|      - | 3420 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3421 | ` * Return` |
|      - | 3422 | ` *  Returns the filename on success or FALSE on failure.` |
|      - | 3423 | ` */` |
|   8504 | 3424 | `static int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3425 | `{` |
|      - | 3426 | `	const ph7_io_stream *pStream;` |
|      - | 3427 | `	io_private *pDev;` |
|      - | 3428 | `	int rc;` |
|   8509 | 3429 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3430 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3431 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3432 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3433 | `		return PH7_OK;` |
|      - | 3434 | `	}` |
|      - | 3435 | `	/* Extract our private data */` |
|   8509 | 3436 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3437 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   8509 | 3438 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3439 | `		/*Expecting an IO handle */` |
|    ! 0 | 3440 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3441 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3442 | `		return PH7_OK;` |
|      - | 3443 | `	}` |
|      - | 3444 | `	/* Point to the target IO stream device */` |
|   8509 | 3445 | `	pStream = pDev->pStream;` |
|   8509 | 3446 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 3447 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3448 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3449 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3450 | `			);` |
|    ! 0 | 3451 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3452 | `		return PH7_OK;` |
|      - | 3453 | `	}` |
|   8509 | 3454 | `	ph7_result_bool(pCtx,0);` |
|      - | 3455 | `	/* Perform the requested operation */` |
|   8509 | 3456 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|   8509 | 3457 | `	if( rc != PH7_OK ){` |
|      - | 3458 | `		/* Return FALSE */` |
|   1003 | 3459 | `		ph7_result_bool(pCtx,0);` |
|    499 | 3460 | `	}` |
|   8509 | 3461 | `	return PH7_OK;` |
|   4257 | 3462 | `}` |
|      - | 3463 | `/*` |
|      - | 3464 | ` * void rewinddir(resource $dir_handle)` |
|      - | 3465 | ` *   Rewind directory handle.` |
|      - | 3466 | ` * Parameter` |
|      - | 3467 | ` *  $dir_handle` |
|      - | 3468 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3469 | ` * Return` |
|      - | 3470 | ` *  FALSE on failure.` |
|      - | 3471 | ` */` |
|      2 | 3472 | `static int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3473 | `{` |
|      - | 3474 | `	const ph7_io_stream *pStream;` |
|      - | 3475 | `	io_private *pDev;` |
|      3 | 3476 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3477 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3478 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3479 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3480 | `		return PH7_OK;` |
|      - | 3481 | `	}` |
|      - | 3482 | `	/* Extract our private data */` |
|      3 | 3483 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3484 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3485 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3486 | `		/*Expecting an IO handle */` |
|    ! 0 | 3487 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3488 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3489 | `		return PH7_OK;` |
|      - | 3490 | `	}` |
|      - | 3491 | `	/* Point to the target IO stream device */` |
|      3 | 3492 | `	pStream = pDev->pStream;` |
|      3 | 3493 | `	if( pStream == 0  \|\| pStream->xRewindDir == 0 ){` |
|    ! 0 | 3494 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3495 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3496 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3497 | `			);` |
|    ! 0 | 3498 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3499 | `		return PH7_OK;` |
|      - | 3500 | `	}` |
|      - | 3501 | `	/* Perform the requested operation */` |
|      3 | 3502 | `	pStream->xRewindDir(pDev->pHandle);` |
|      3 | 3503 | `	return PH7_OK;` |
|      2 | 3504 | ` }` |
|      - | 3505 | `/* Forward declaration */` |
|      - | 3506 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut);` |
|      - | 3507 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|      - | 3508 | `/*` |
|      - | 3509 | ` * void closedir(resource $dir_handle)` |
|      - | 3510 | ` *   Close directory handle.` |
|      - | 3511 | ` * Parameter` |
|      - | 3512 | ` *  $dir_handle` |
|      - | 3513 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3514 | ` * Return` |
|      - | 3515 | ` *  FALSE on failure.` |
|      - | 3516 | ` */` |
|   1002 | 3517 | `static int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3518 | `{` |
|      - | 3519 | `	const ph7_io_stream *pStream;` |
|      - | 3520 | `	io_private *pDev;` |
|   1007 | 3521 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3522 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3523 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3524 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3525 | `		return PH7_OK;` |
|      - | 3526 | `	}` |
|      - | 3527 | `	/* Extract our private data */` |
|   1007 | 3528 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3529 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1007 | 3530 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3531 | `		/*Expecting an IO handle */` |
|    ! 0 | 3532 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3533 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3534 | `		return PH7_OK;` |
|      - | 3535 | `	}` |
|      - | 3536 | `	/* Point to the target IO stream device */` |
|   1007 | 3537 | `	pStream = pDev->pStream;` |
|   1007 | 3538 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 3539 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3540 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3541 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3542 | `			);` |
|    ! 0 | 3543 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3544 | `		return PH7_OK;` |
|      - | 3545 | `	}` |
|      - | 3546 | `	/* Perform the requested operation */` |
|   1007 | 3547 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 3548 | `	/* Release the private stucture */` |
|   1007 | 3549 | `	ReleaseIOPrivate(pCtx,pDev);` |
|   1007 | 3550 | `	PH7_MemObjRelease(apArg[0]);` |
|   1007 | 3551 | `	return PH7_OK;` |
|    506 | 3552 | ` }` |
|      - | 3553 | `/*` |
|      - | 3554 | ` * resource opendir(string $path[,resource $context])` |
|      - | 3555 | ` *  Open directory handle.` |
|      - | 3556 | ` * Parameters` |
|      - | 3557 | ` * $path` |
|      - | 3558 | ` *   The directory path that is to be opened.` |
|      - | 3559 | ` * $context` |
|      - | 3560 | ` *   A context stream resource.` |
|      - | 3561 | ` * Return` |
|      - | 3562 | ` *  A directory handle resource on success,or FALSE on failure.` |
|      - | 3563 | ` */` |
|   1002 | 3564 | `static int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3565 | `{` |
|      - | 3566 | `	const ph7_io_stream *pStream;` |
|      - | 3567 | `	const char *zPath;` |
|      - | 3568 | `	io_private *pDev;` |
|      - | 3569 | `	int iLen,rc;` |
|   1007 | 3570 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3571 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3572 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 3573 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3574 | `		return PH7_OK;` |
|      - | 3575 | `	}` |
|      - | 3576 | `	/* Extract the target path */` |
|   1007 | 3577 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 3578 | `	/* Try to extract a stream */` |
|   1007 | 3579 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|   1007 | 3580 | `	if( pStream == 0 ){` |
|    ! 0 | 3581 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3582 | `			"No stream device is associated with the given path(%s)",zPath);` |
|    ! 0 | 3583 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3584 | `		return PH7_OK;` |
|      - | 3585 | `	}` |
|   1007 | 3586 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 3587 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3588 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 3589 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 3590 | `			);` |
|    ! 0 | 3591 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3592 | `		return PH7_OK;` |
|      - | 3593 | `	}` |
|      - | 3594 | `	/* Allocate a new IO private instance */` |
|   1007 | 3595 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|   1007 | 3596 | `	if( pDev == 0 ){` |
|    ! 0 | 3597 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3598 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3599 | `		return PH7_OK;` |
|      - | 3600 | `	}` |
|      - | 3601 | `	/* Initialize the structure */` |
|   1007 | 3602 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 3603 | `	/* Open the target directory */` |
|   1007 | 3604 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|   1007 | 3605 | `	if( rc != PH7_OK ){` |
|      - | 3606 | `		/* IO error,return FALSE */` |
|    ! 0 | 3607 | `		ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 3608 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3609 | `	}else{` |
|      - | 3610 | `		/* Return the handle as a resource */` |
|   1007 | 3611 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 3612 | `	}` |
|   1007 | 3613 | `	return PH7_OK;` |
|    506 | 3614 | `}` |
|      - | 3615 | `/*` |
|      - | 3616 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|      - | 3617 | ` *  Reads a file and writes it to the output buffer.` |
|      - | 3618 | ` * Parameters` |
|      - | 3619 | ` *  $filename` |
|      - | 3620 | ` *   The filename being read.` |
|      - | 3621 | ` *  $use_include_path` |
|      - | 3622 | ` *   You can use the optional second parameter and set it to` |
|      - | 3623 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3624 | ` *  $context` |
|      - | 3625 | ` *   A context stream resource.` |
|      - | 3626 | ` * Return` |
|      - | 3627 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|      - | 3628 | ` */` |
|      2 | 3629 | `static int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3630 | `{` |
|      3 | 3631 | `	int use_include  = FALSE;` |
|      - | 3632 | `	const ph7_io_stream *pStream;` |
|      - | 3633 | `	ph7_int64 n,nRead;` |
|      - | 3634 | `	const char *zFile;` |
|      - | 3635 | `	char zBuf[8192];` |
|      - | 3636 | `	void *pHandle;` |
|      - | 3637 | `	int rc,nLen;` |
|      3 | 3638 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3639 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3640 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3641 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3642 | `		return PH7_OK;` |
|      - | 3643 | `	}` |
|      - | 3644 | `	/* Extract the file path */` |
|      3 | 3645 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3646 | `	/* Point to the target IO stream device */` |
|      3 | 3647 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3648 | `	if( pStream == 0 ){` |
|    ! 0 | 3649 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3650 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3651 | `		return PH7_OK;` |
|      - | 3652 | `	}` |
|      3 | 3653 | `	if( nArg > 1 ){` |
|    ! 0 | 3654 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3655 | `	}` |
|      - | 3656 | `	/* Try to open the file in read-only mode */` |
|      4 | 3657 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|      1 | 3658 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 3659 | `	if( pHandle == 0 ){` |
|    ! 0 | 3660 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3661 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3662 | `		return PH7_OK;` |
|      - | 3663 | `	}` |
|      - | 3664 | `	/* Perform the requested operation */` |
|      3 | 3665 | `	nRead = 0;` |
|      2 | 3666 | `	for(;;){` |
|      5 | 3667 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3668 | `		if( n < 1 ){` |
|      - | 3669 | `			/* EOF or IO error,break immediately */` |
|      3 | 3670 | `			break;` |
|      - | 3671 | `		}` |
|      - | 3672 | `		/* Output data */` |
|      3 | 3673 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      3 | 3674 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 3675 | `			break;` |
|      - | 3676 | `		}` |
|      - | 3677 | `		/* Increment counter */` |
|      3 | 3678 | `		nRead += n;` |
|      1 | 3679 | `	}` |
|      - | 3680 | `	/* Close the stream */` |
|      3 | 3681 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3682 | `	/* Total number of bytes readen */` |
|      3 | 3683 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 3684 | `	return PH7_OK;` |
|      2 | 3685 | `}` |
|      - | 3686 | `/*` |
|      - | 3687 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|      - | 3688 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|      - | 3689 | ` *  Reads entire file into a string.` |
|      - | 3690 | ` * Parameters` |
|      - | 3691 | ` *  $filename` |
|      - | 3692 | ` *   The filename being read.` |
|      - | 3693 | ` *  $use_include_path` |
|      - | 3694 | ` *   You can use the optional second parameter and set it to` |
|      - | 3695 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3696 | ` *  $context` |
|      - | 3697 | ` *   A context stream resource.` |
|      - | 3698 | ` *  $offset` |
|      - | 3699 | ` *   The offset where the reading starts on the original stream.` |
|      - | 3700 | ` *  $maxlen` |
|      - | 3701 | ` *    Maximum length of data read. The default is to read until end of file` |
|      - | 3702 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|      - | 3703 | ` * Return` |
|      - | 3704 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3705 | ` */` |
|   6552 | 3706 | `static int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3707 | `{` |
|      - | 3708 | `	const ph7_io_stream *pStream;` |
|      - | 3709 | `	ph7_int64 n,nRead,nMaxlen;` |
|   6557 | 3710 | `	int use_include  = FALSE;` |
|      - | 3711 | `	const char *zFile;` |
|      - | 3712 | `	char zBuf[8192];` |
|      - | 3713 | `	void *pHandle;` |
|      - | 3714 | `	int nLen;` |
|      - | 3715 |  |
|   6557 | 3716 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3717 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3718 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3719 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3720 | `		return PH7_OK;` |
|      - | 3721 | `	}` |
|      - | 3722 | `	/* Extract the file path */` |
|   6557 | 3723 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3724 | `	/* Point to the target IO stream device */` |
|   6557 | 3725 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   6557 | 3726 | `	if( pStream == 0 ){` |
|    ! 0 | 3727 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3728 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3729 | `		return PH7_OK;` |
|      - | 3730 | `	}` |
|   6557 | 3731 | `	nMaxlen = -1;` |
|   6557 | 3732 | `	if( nArg > 1 ){` |
|      5 | 3733 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|      2 | 3734 | `	}` |
|      - | 3735 | `	/* Try to open the file in read-only mode */` |
|   6557 | 3736 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|   6557 | 3737 | `	if( pHandle == 0 ){` |
|    ! 0 | 3738 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3739 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3740 | `		return PH7_OK;` |
|      - | 3741 | `	}` |
|   6557 | 3742 | `	if( nArg > 3 ){` |
|      - | 3743 | `		/* Extract the offset */` |
|      5 | 3744 | `		n = ph7_value_to_int64(apArg[3]);` |
|      5 | 3745 | `		if( n > 0 ){` |
|    ! 0 | 3746 | `			if( pStream->xSeek ){` |
|      - | 3747 | `				/* Seek to the desired offset */` |
|    ! 0 | 3748 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|    ! 0 | 3749 | `			}` |
|    ! 0 | 3750 | `		}` |
|      5 | 3751 | `		if( nArg > 4 ){` |
|      - | 3752 | `			/* Maximum data to read */` |
|      5 | 3753 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|      2 | 3754 | `		}` |
|      2 | 3755 | `	}` |
|      - | 3756 | `	/* Perform the requested operation */` |
|   6557 | 3757 | `	nRead = 0;` |
|   6550 | 3758 | `	for(;;){` |
|  19658 | 3759 | `		n = pStream->xRead(pHandle,zBuf,` |
|   6553 | 3760 | `			(nMaxlen > 0 && (nMaxlen < (ph7_int64)sizeof(zBuf))) ? nMaxlen : (ph7_int64)sizeof(zBuf));` |
|  13105 | 3761 | `		if( n < 1 ){` |
|      - | 3762 | `			/* EOF or IO error,break immediately */` |
|   6555 | 3763 | `			break;` |
|      - | 3764 | `		}` |
|      - | 3765 | `		/* Append data */` |
|   6555 | 3766 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 3767 | `		/* Increment read counter */` |
|   6555 | 3768 | `		nRead += n;` |
|   6555 | 3769 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 3770 | `			/* Read limit reached */` |
|      3 | 3771 | `			break;` |
|      - | 3772 | `		}` |
|      5 | 3773 | `	}` |
|      - | 3774 | `	/* Close the stream */` |
|   6557 | 3775 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3776 | `	/* Check if we have read something */` |
|   6557 | 3777 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 3778 | `		/* Nothing read,return FALSE */` |
|      3 | 3779 | `		ph7_result_bool(pCtx,0);` |
|      1 | 3780 | `	}` |
|   6557 | 3781 | `	return PH7_OK;` |
|   3281 | 3782 | `}` |
|      - | 3783 | `/*` |
|      - | 3784 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 3785 | ` *  Write a string to a file.` |
|      - | 3786 | ` * Parameters` |
|      - | 3787 | ` *  $filename` |
|      - | 3788 | ` *  Path to the file where to write the data.` |
|      - | 3789 | ` * $data` |
|      - | 3790 | ` *  The data to write(Must be a string).` |
|      - | 3791 | ` * $flags` |
|      - | 3792 | ` *  The value of flags can be any combination of the following` |
|      - | 3793 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 3794 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 3795 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 3796 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 3797 | ` * context` |
|      - | 3798 | ` *  A context stream resource.` |
|      - | 3799 | ` * Return` |
|      - | 3800 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 3801 | ` */` |
|  13370 | 3802 | `static int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3803 | `{` |
|  13375 | 3804 | `	int use_include  = FALSE;` |
|      - | 3805 | `	const ph7_io_stream *pStream;` |
|      - | 3806 | `	const char *zFile;` |
|      - | 3807 | `	const char *zData;` |
|      - | 3808 | `	int iOpenFlags;` |
|      - | 3809 | `	void *pHandle;` |
|      - | 3810 | `	int iFlags;` |
|      - | 3811 | `	int nLen;` |
|      - | 3812 |  |
|  13375 | 3813 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3814 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3815 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3816 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3817 | `		return PH7_OK;` |
|      - | 3818 | `	}` |
|      - | 3819 | `	/* Extract the file path */` |
|  13375 | 3820 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3821 | `	/* Point to the target IO stream device */` |
|  13375 | 3822 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  13375 | 3823 | `	if( pStream == 0 ){` |
|    ! 0 | 3824 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3825 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3826 | `		return PH7_OK;` |
|      - | 3827 | `	}` |
|      - | 3828 | `	/* Data to write */` |
|  13375 | 3829 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 3830 | `	/* Try to open the file in read-write mode */` |
|  13375 | 3831 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 3832 | `	/* Extract the flags */` |
|  13375 | 3833 | `	iFlags = 0;` |
|  13375 | 3834 | `	if( nArg > 2 ){` |
|    ! 0 | 3835 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3836 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 3837 | `			use_include = TRUE;` |
|    ! 0 | 3838 | `		}` |
|    ! 0 | 3839 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 3840 | `			/* If the file already exists, append the data to the file` |
|      - | 3841 | `			 * instead of overwriting it.` |
|      - | 3842 | `			 */` |
|    ! 0 | 3843 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 3844 | `			/* Append mode */` |
|    ! 0 | 3845 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 3846 | `		}` |
|    ! 0 | 3847 | `	}` |
|  20060 | 3848 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   6685 | 3849 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|  13375 | 3850 | `	if( pHandle == 0 ){` |
|    ! 0 | 3851 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3852 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3853 | `		return PH7_OK;` |
|      - | 3854 | `	}` |
|  13375 | 3855 | `	if( nLen < 1 ){` |
|      - | 3856 | `		/* Empty data, file is created/truncated */` |
|      7 | 3857 | `		ph7_result_int64(pCtx,0);` |
|      7 | 3858 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|      7 | 3859 | `		return PH7_OK;` |
|      - | 3860 | `	}` |
|  13369 | 3861 | `	if( pStream->xWrite ){` |
|      - | 3862 | `		ph7_int64 n;` |
|  13369 | 3863 | `		if( (iFlags & 2/* LOCK_EX, php's value */) && pStream->xLock ){` |
|      - | 3864 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 3865 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 3866 | `		}` |
|      - | 3867 | `		/* Perform the write operation */` |
|  13369 | 3868 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  13369 | 3869 | `		if( n < 0 ){` |
|      - | 3870 | `			/* IO error,return FALSE */` |
|    ! 0 | 3871 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3872 | `		}else{` |
|      - | 3873 | `			/* Total number of bytes written */` |
|  13369 | 3874 | `			ph7_result_int64(pCtx,n);` |
|      - | 3875 | `		}` |
|   6687 | 3876 | `	}else{` |
|      - | 3877 | `		/* Read-only stream */` |
|    ! 0 | 3878 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 3879 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 3880 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 3881 | `			);` |
|    ! 0 | 3882 | `		ph7_result_bool(pCtx,0);` |
|      - | 3883 | `	}` |
|      - | 3884 | `	/* Close the handle */` |
|  13369 | 3885 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  13369 | 3886 | `	return PH7_OK;` |
|   6690 | 3887 | `}` |
|      - | 3888 | `/*` |
|      - | 3889 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 3890 | ` *  Reads entire file into an array.` |
|      - | 3891 | ` * Parameters` |
|      - | 3892 | ` *  $filename` |
|      - | 3893 | ` *   The filename being read.` |
|      - | 3894 | ` *  $flags` |
|      - | 3895 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 3896 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 3897 | ` *       Search for the file in the include_path.` |
|      - | 3898 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 3899 | ` *       Do not add newline at the end of each array element` |
|      - | 3900 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 3901 | ` *       Skip empty lines` |
|      - | 3902 | ` *  $context` |
|      - | 3903 | ` *   A context stream resource.` |
|      - | 3904 | ` * Return` |
|      - | 3905 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3906 | ` */` |
|      4 | 3907 | `static int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3908 | `{` |
|      - | 3909 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 3910 | `	ph7_value *pArray,*pLine;` |
|      - | 3911 | `	const ph7_io_stream *pStream;` |
|      6 | 3912 | `	int use_include = 0;` |
|      - | 3913 | `	io_private *pDev;` |
|      - | 3914 | `	ph7_int64 n;` |
|      - | 3915 | `	int iFlags;` |
|      - | 3916 | `	int nLen;` |
|      - | 3917 |  |
|      6 | 3918 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3919 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3920 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3921 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3922 | `		return PH7_OK;` |
|      - | 3923 | `	}` |
|      - | 3924 | `	/* Extract the file path */` |
|      6 | 3925 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3926 | `	/* Point to the target IO stream device */` |
|      6 | 3927 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 3928 | `	if( pStream == 0 ){` |
|    ! 0 | 3929 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3930 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3931 | `		return PH7_OK;` |
|      - | 3932 | `	}` |
|      - | 3933 | `	/* Allocate a new IO private instance */` |
|      6 | 3934 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      6 | 3935 | `	if( pDev == 0 ){` |
|    ! 0 | 3936 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3937 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3938 | `		return PH7_OK;` |
|      - | 3939 | `	}` |
|      - | 3940 | `	/* Initialize the structure */` |
|      6 | 3941 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      6 | 3942 | `	iFlags = 0;` |
|      6 | 3943 | `	if( nArg > 1 ){` |
|    ! 0 | 3944 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 3945 | `	}` |
|      6 | 3946 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|    ! 0 | 3947 | `		use_include = TRUE;` |
|    ! 0 | 3948 | `	}` |
|      - | 3949 | `	/* Create the array and the working value */` |
|      6 | 3950 | `	pArray = ph7_context_new_array(pCtx);` |
|      6 | 3951 | `	pLine = ph7_context_new_scalar(pCtx);` |
|      6 | 3952 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 3953 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3954 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3955 | `		return PH7_OK;` |
|      - | 3956 | `	}` |
|      - | 3957 | `	/* Try to open the file in read-only mode */` |
|      6 | 3958 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 3959 | `	if( pDev->pHandle == 0 ){` |
|      3 | 3960 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|      3 | 3961 | `		ph7_result_bool(pCtx,0);` |
|      - | 3962 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 3963 | `		 * as soon we return from this function.` |
|      - | 3964 | `		 */` |
|      3 | 3965 | `		return PH7_OK;` |
|      - | 3966 | `	}` |
|      - | 3967 | `	/* Perform the requested operation */` |
|      3 | 3968 | `	for(;;){` |
|      - | 3969 | `		/* Try to extract a line */` |
|      7 | 3970 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|      7 | 3971 | `		if( n < 1 ){` |
|      - | 3972 | `			/* EOF or IO error */` |
|      3 | 3973 | `			break;` |
|      - | 3974 | `		}` |
|      - | 3975 | `		/* Reset the cursor */` |
|      5 | 3976 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 3977 | `		/* Remove line ending if requested by the caller */` |
|      5 | 3978 | `		zPtr = zBuf;` |
|      5 | 3979 | `		zEnd = &zBuf[n];` |
|      5 | 3980 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 3981 | `			/* Ignore trailig lines */` |
|    ! 0 | 3982 | `			while( zPtr < zEnd && (zEnd[-1] == '\n'` |
|      - | 3983 | `#ifdef __WINNT__` |
|      - | 3984 | `				\|\| zEnd[-1] == '\r'` |
|      - | 3985 | `#endif` |
|      - | 3986 | `				)){` |
|    ! 0 | 3987 | `					n--;` |
|    ! 0 | 3988 | `					zEnd--;` |
|    ! 0 | 3989 | `			}` |
|    ! 0 | 3990 | `		}` |
|      5 | 3991 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 3992 | `			/* Ignore empty lines */` |
|    ! 0 | 3993 | `			while( zPtr < zEnd && (unsigned char)zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) ){` |
|    ! 0 | 3994 | `				zPtr++;` |
|    ! 0 | 3995 | `			}` |
|    ! 0 | 3996 | `			if( zPtr >= zEnd ){` |
|      - | 3997 | `				/* Empty line */` |
|    ! 0 | 3998 | `				continue;` |
|      - | 3999 | `			}` |
|    ! 0 | 4000 | `		}` |
|      5 | 4001 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 4002 | `		/* Insert line */` |
|      5 | 4003 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 4004 | `	}` |
|      - | 4005 | `	/* Close the stream */` |
|      3 | 4006 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 4007 | `	/* Release the io_private instance */` |
|      3 | 4008 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 4009 | `	/* Return the created array */` |
|      3 | 4010 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4011 | `	return PH7_OK;` |
|      4 | 4012 | `}` |
|      - | 4013 | `/*` |
|      - | 4014 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 4015 | ` *  Makes a copy of the file source to dest.` |
|      - | 4016 | ` * Parameters` |
|      - | 4017 | ` *  $source` |
|      - | 4018 | ` *   Path to the source file.` |
|      - | 4019 | ` *  $dest` |
|      - | 4020 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 4021 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 4022 | ` *  $context` |
|      - | 4023 | ` *   A context stream resource.` |
|      - | 4024 | ` * Return` |
|      - | 4025 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4026 | ` */` |
|      4 | 4027 | `static int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4028 | `{` |
|      - | 4029 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 4030 | `	const char *zFile;` |
|      - | 4031 | `	char zBuf[8192];` |
|      - | 4032 | `	void *pIn,*pOut;` |
|      - | 4033 | `	ph7_int64 n;` |
|      - | 4034 | `	int nLen;` |
|      6 | 4035 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 4036 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4037 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|    ! 0 | 4038 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4039 | `		return PH7_OK;` |
|      - | 4040 | `	}` |
|      - | 4041 | `	/* Extract the source name */` |
|      6 | 4042 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4043 | `	/* Point to the target IO stream device */` |
|      6 | 4044 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 4045 | `	if( pSin == 0 ){` |
|    ! 0 | 4046 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4047 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4048 | `		return PH7_OK;` |
|      - | 4049 | `	}` |
|      - | 4050 | `	/* Try to open the source file in a read-only mode */` |
|      6 | 4051 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 4052 | `	if( pIn == 0 ){` |
|      3 | 4053 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening source: '%s'",zFile);` |
|      3 | 4054 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4055 | `		return PH7_OK;` |
|      - | 4056 | `	}` |
|      - | 4057 | `	/* Extract the destination name */` |
|      3 | 4058 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 4059 | `	/* Point to the target IO stream device */` |
|      3 | 4060 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4061 | `	if( pSout == 0 ){` |
|    ! 0 | 4062 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4063 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4064 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4065 | `		return PH7_OK;` |
|      - | 4066 | `	}` |
|      3 | 4067 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 4068 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4069 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4070 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 4071 | `			);` |
|    ! 0 | 4072 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4073 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4074 | `		return PH7_OK;` |
|      - | 4075 | `	}` |
|      - | 4076 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 4077 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 4078 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 4079 | `	if( pOut == 0 ){` |
|    ! 0 | 4080 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening destination: '%s'",zFile);` |
|    ! 0 | 4081 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4082 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4083 | `		return PH7_OK;` |
|      - | 4084 | `	}` |
|      - | 4085 | `	/* Perform the requested operation */` |
|      2 | 4086 | `	for(;;){` |
|      - | 4087 | `		/* Read from source */` |
|      5 | 4088 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 4089 | `		if( n < 1 ){` |
|      - | 4090 | `			/* EOF or IO error,break immediately */` |
|      3 | 4091 | `			break;` |
|      - | 4092 | `		}` |
|      - | 4093 | `		/* Write to dest */` |
|      3 | 4094 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 4095 | `		if( n < 1 ){` |
|      - | 4096 | `			/* IO error,break immediately */` |
|    ! 0 | 4097 | `			break;` |
|      - | 4098 | `		}` |
|      1 | 4099 | `	}` |
|      - | 4100 | `	/* Close the streams */` |
|      3 | 4101 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 4102 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 4103 | `	/* Return TRUE */` |
|      3 | 4104 | `	ph7_result_bool(pCtx,1);` |
|      3 | 4105 | `	return PH7_OK;` |
|      4 | 4106 | `}` |
|      - | 4107 | `/*` |
|      - | 4108 | ` * array fstat(resource $handle)` |
|      - | 4109 | ` *  Gets information about a file using an open file pointer.` |
|      - | 4110 | ` * Parameters` |
|      - | 4111 | ` *  $handle` |
|      - | 4112 | ` *   The file pointer.` |
|      - | 4113 | ` * Return` |
|      - | 4114 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 4115 | ` */` |
|      2 | 4116 | `static int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4117 | `{` |
|      - | 4118 | `	ph7_value *pArray,*pValue;` |
|      - | 4119 | `	const ph7_io_stream *pStream;` |
|      - | 4120 | `	io_private *pDev;` |
|      3 | 4121 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4122 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4123 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4124 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4125 | `		return PH7_OK;` |
|      - | 4126 | `	}` |
|      - | 4127 | `	/* Extract our private data */` |
|      3 | 4128 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4129 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4130 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4131 | `		/* Expecting an IO handle */` |
|    ! 0 | 4132 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4133 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4134 | `		return PH7_OK;` |
|      - | 4135 | `	}` |
|      - | 4136 | `	/* Point to the target IO stream device */` |
|      3 | 4137 | `	pStream = pDev->pStream;` |
|      3 | 4138 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 4139 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4140 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4141 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4142 | `			);` |
|    ! 0 | 4143 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4144 | `		return PH7_OK;` |
|      - | 4145 | `	}` |
|      - | 4146 | `	/* Create the array and the working value */` |
|      3 | 4147 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4148 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4149 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4150 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4151 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4152 | `		return PH7_OK;` |
|      - | 4153 | `	}` |
|      - | 4154 | `	/* Perform the requested operation */` |
|      3 | 4155 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 4156 | `	/* Return the freshly created array */` |
|      3 | 4157 | `	ph7_result_value(pCtx,pArray);` |
|      - | 4158 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 4159 | `	 * released automatically as soon we return from this function.` |
|      - | 4160 | `	 */` |
|      3 | 4161 | `	return PH7_OK;` |
|      2 | 4162 | `}` |
|      - | 4163 | `/*` |
|      - | 4164 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 4165 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 4166 | ` * Parameters` |
|      - | 4167 | ` *  $handle` |
|      - | 4168 | ` *   The file pointer.` |
|      - | 4169 | ` *  $string` |
|      - | 4170 | ` *   The string that is to be written.` |
|      - | 4171 | ` *  $length` |
|      - | 4172 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 4173 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 4174 | ` * Return` |
|      - | 4175 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 4176 | ` */` |
|     22 | 4177 | `static int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4178 | `{` |
|      - | 4179 | `	const ph7_io_stream *pStream;` |
|      - | 4180 | `	const char *zString;` |
|      - | 4181 | `	io_private *pDev;` |
|      - | 4182 | `	int nLen,n;` |
|     24 | 4183 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4184 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4185 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4186 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4187 | `		return PH7_OK;` |
|      - | 4188 | `	}` |
|      - | 4189 | `	/* Extract our private data */` |
|     24 | 4190 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4191 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     24 | 4192 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4193 | `		/* Expecting an IO handle */` |
|    ! 0 | 4194 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4195 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4196 | `		return PH7_OK;` |
|      - | 4197 | `	}` |
|      - | 4198 | `	/* Point to the target IO stream device */` |
|     24 | 4199 | `	pStream = pDev->pStream;` |
|     24 | 4200 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4201 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4202 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4203 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4204 | `			);` |
|    ! 0 | 4205 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4206 | `		return PH7_OK;` |
|      - | 4207 | `	}` |
|      - | 4208 | `	/* Extract the data to write */` |
|     24 | 4209 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|     24 | 4210 | `	if( nArg > 2 ){` |
|      - | 4211 | `		/* Maximum data length to write */` |
|    ! 0 | 4212 | `		n = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 4213 | `		if( n >= 0 && n < nLen ){` |
|    ! 0 | 4214 | `			nLen = n;` |
|    ! 0 | 4215 | `		}` |
|    ! 0 | 4216 | `	}` |
|     24 | 4217 | `	if( nLen < 1 ){` |
|      - | 4218 | `		/* Nothing to write */` |
|    ! 0 | 4219 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4220 | `		return PH7_OK;` |
|      - | 4221 | `	}` |
|      - | 4222 | `	/* Perform the requested operation */` |
|     24 | 4223 | `	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);` |
|     24 | 4224 | `	if( n <  0 ){` |
|      - | 4225 | `		/* IO error,return FALSE */` |
|    ! 0 | 4226 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4227 | `	}else{` |
|      - | 4228 | `		/* #Bytes written */` |
|     24 | 4229 | `		ph7_result_int(pCtx,n);` |
|      - | 4230 | `	}` |
|     24 | 4231 | `	return PH7_OK;` |
|     13 | 4232 | `}` |
|      - | 4233 | `/*` |
|      - | 4234 | ` * bool flock(resource $handle,int $operation)` |
|      - | 4235 | ` *  Portable advisory file locking.` |
|      - | 4236 | ` * Parameters` |
|      - | 4237 | ` *  $handle` |
|      - | 4238 | ` *   The file pointer.` |
|      - | 4239 | ` *  $operation` |
|      - | 4240 | ` *   operation is one of the following:` |
|      - | 4241 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 4242 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 4243 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 4244 | ` * Return` |
|      - | 4245 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 4246 | ` */` |
|      4 | 4247 | `static int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 4248 | `{` |
|      - | 4249 | `	const ph7_io_stream *pStream;` |
|      - | 4250 | `	io_private *pDev;` |
|      - | 4251 | `	int nLock;` |
|      - | 4252 | `	int rc;` |
|      4 | 4253 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4254 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4255 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4256 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4257 | `		return PH7_OK;` |
|      - | 4258 | `	}` |
|      - | 4259 | `	/* Extract our private data */` |
|      4 | 4260 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4261 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      4 | 4262 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4263 | `		/*Expecting an IO handle */` |
|    ! 0 | 4264 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4265 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4266 | `		return PH7_OK;` |
|      - | 4267 | `	}` |
|      - | 4268 | `	/* Point to the target IO stream device */` |
|      4 | 4269 | `	pStream = pDev->pStream;` |
|      4 | 4270 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|    ! 0 | 4271 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4272 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4273 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4274 | `			);` |
|    ! 0 | 4275 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4276 | `		return PH7_OK;` |
|      - | 4277 | `	}` |
|      - | 4278 | `	/* Requested lock operation */` |
|      4 | 4279 | `	nLock = ph7_value_to_int(apArg[1]);` |
|      - | 4280 | `	/*` |
|      - | 4281 | `	 * Translate php's operation (LOCK_SH=1, LOCK_EX=2, LOCK_UN=3, optionally \|LOCK_NB=4)` |
|      - | 4282 | `	 * into the xLock() vtable contract, which is a PUBLIC C API and stays as it is:` |
|      - | 4283 | `	 * negative = unlock, 1 = exclusive, anything else = shared.` |
|      - | 4284 | `	 */` |
|      - | 4285 | `	{` |
|      4 | 4286 | `		int iOp = nLock & ~4 /* strip LOCK_NB */;` |
|      4 | 4287 | `		if( iOp == 3 /* LOCK_UN */ ){` |
|      2 | 4288 | `			nLock = -1;` |
|      3 | 4289 | `		}else if( iOp == 2 /* LOCK_EX */ ){` |
|      2 | 4290 | `			nLock = 1;` |
|      1 | 4291 | `		}else{` |
|    ! 0 | 4292 | `			nLock = 0; /* LOCK_SH */` |
|      - | 4293 | `		}` |
|      - | 4294 | `	}` |
|      - | 4295 | `	/* Lock operation */` |
|      4 | 4296 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 4297 | `	/* IO result */` |
|      4 | 4298 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 | 4299 | `	return PH7_OK;` |
|      2 | 4300 | `}` |
|      - | 4301 | `/*` |
|      - | 4302 | ` * int fpassthru(resource $handle)` |
|      - | 4303 | ` *  Output all remaining data on a file pointer.` |
|      - | 4304 | ` * Parameters` |
|      - | 4305 | ` *  $handle` |
|      - | 4306 | ` *   The file pointer.` |
|      - | 4307 | ` * Return` |
|      - | 4308 | ` *  Total number of characters read from handle and passed through` |
|      - | 4309 | ` *  to the output on success or FALSE on failure.` |
|      - | 4310 | ` */` |
|      2 | 4311 | `static int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4312 | `{` |
|      - | 4313 | `	const ph7_io_stream *pStream;` |
|      - | 4314 | `	io_private *pDev;` |
|      - | 4315 | `	ph7_int64 n,nRead;` |
|      - | 4316 | `	char zBuf[8192];` |
|      - | 4317 | `	int rc;` |
|      3 | 4318 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4319 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4320 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4321 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4322 | `		return PH7_OK;` |
|      - | 4323 | `	}` |
|      - | 4324 | `	/* Extract our private data */` |
|      3 | 4325 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4326 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4327 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4328 | `		/*Expecting an IO handle */` |
|    ! 0 | 4329 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4330 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4331 | `		return PH7_OK;` |
|      - | 4332 | `	}` |
|      - | 4333 | `	/* Point to the target IO stream device */` |
|      3 | 4334 | `	pStream = pDev->pStream;` |
|      3 | 4335 | `	if( pStream == 0  ){` |
|    ! 0 | 4336 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4337 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4338 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4339 | `			);` |
|    ! 0 | 4340 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4341 | `		return PH7_OK;` |
|      - | 4342 | `	}` |
|      - | 4343 | `	/* Perform the requested operation */` |
|      3 | 4344 | `	nRead = 0;` |
|      2 | 4345 | `	for(;;){` |
|      5 | 4346 | `		n = StreamRead(pDev,zBuf,sizeof(zBuf));` |
|      5 | 4347 | `		if( n < 1 ){` |
|      - | 4348 | `			/* Error or EOF */` |
|      3 | 4349 | `			break;` |
|      - | 4350 | `		}` |
|      - | 4351 | `		/* Increment the read counter */` |
|      3 | 4352 | `		nRead += n;` |
|      - | 4353 | `		/* Output data */` |
|      3 | 4354 | `		rc = ph7_context_output(pCtx,zBuf,(int)nRead /* FIXME: 64-bit issues */);` |
|      3 | 4355 | `		if( rc == PH7_ABORT ){` |
|      - | 4356 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 4357 | `			break;` |
|      - | 4358 | `		}` |
|      1 | 4359 | `	}` |
|      - | 4360 | `	/* Total number of bytes readen */` |
|      3 | 4361 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 4362 | `	return PH7_OK;` |
|      2 | 4363 | `}` |
|      - | 4364 | `/* CSV reader/writer private data */` |
|      - | 4365 | `struct csv_data` |
|      - | 4366 | `{` |
|      - | 4367 | `	int delimiter;    /* Delimiter. Default ',' */` |
|      - | 4368 | `	int enclosure;    /* Enclosure. Default '"'*/` |
|      - | 4369 | `	io_private *pDev; /* Open stream handle */` |
|      - | 4370 | `	int iCount;       /* Counter */` |
|      - | 4371 | `};` |
|      - | 4372 | `/*` |
|      - | 4373 | ` * The following callback is used by the fputcsv() function inorder to iterate` |
|      - | 4374 | ` * throw array entries and output CSV data based on the current key and it's` |
|      - | 4375 | ` * associated data.` |
|      - | 4376 | ` */` |
|      6 | 4377 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 4378 | `{` |
|      7 | 4379 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 4380 | `	const char *zData;` |
|      - | 4381 | `	int nLen,c2;` |
|      - | 4382 | `	sxu32 n;` |
|      - | 4383 | `	/* Point to the raw data */` |
|      7 | 4384 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      7 | 4385 | `	if( nLen < 1 ){` |
|      - | 4386 | `		/* Nothing to write */` |
|    ! 0 | 4387 | `		return PH7_OK;` |
|      - | 4388 | `	}` |
|      7 | 4389 | `	if( pData->iCount > 0 ){` |
|      - | 4390 | `		/* Write the delimiter */` |
|      5 | 4391 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->delimiter,sizeof(char));` |
|      2 | 4392 | `	}` |
|      7 | 4393 | `	n = 1;` |
|      7 | 4394 | `	c2 = 0;` |
|     10 | 4395 | `	if( SyByteFind(zData,(sxu32)nLen,pData->delimiter,0) == SXRET_OK \|\|` |
|      6 | 4396 | `		SyByteFind(zData,(sxu32)nLen,pData->enclosure,&n) == SXRET_OK ){` |
|    ! 0 | 4397 | `			c2 = 1;` |
|    ! 0 | 4398 | `			if( n == 0 ){` |
|    ! 0 | 4399 | `				c2 = 2;` |
|    ! 0 | 4400 | `			}` |
|      - | 4401 | `			/* Write the enclosure */` |
|    ! 0 | 4402 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4403 | `			if( c2 > 1 ){` |
|    ! 0 | 4404 | `				pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4405 | `			}` |
|    ! 0 | 4406 | `	}` |
|      - | 4407 | `	/* Write the data */` |
|      7 | 4408 | `	if( pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)zData,(ph7_int64)nLen) < 1 ){` |
|    ! 0 | 4409 | `		SXUNUSED(pKey); /* cc warning */` |
|    ! 0 | 4410 | `		return PH7_ABORT;` |
|      - | 4411 | `	}` |
|      7 | 4412 | `	if( c2 > 0 ){` |
|      - | 4413 | `		/* Write the enclosure */` |
|    ! 0 | 4414 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4415 | `		if( c2 > 1 ){` |
|    ! 0 | 4416 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4417 | `		}` |
|    ! 0 | 4418 | `	}` |
|      7 | 4419 | `	pData->iCount++;` |
|      7 | 4420 | `	return PH7_OK;` |
|      4 | 4421 | `}` |
|      - | 4422 | `/*` |
|      - | 4423 | ` * int fputcsv(resource $handle,array $fields[,string $delimiter = ','[,string $enclosure = '"' ]])` |
|      - | 4424 | ` *  Format line as CSV and write to file pointer.` |
|      - | 4425 | ` * Parameters` |
|      - | 4426 | ` *  $handle` |
|      - | 4427 | ` *   Open file handle.` |
|      - | 4428 | ` * $fields` |
|      - | 4429 | ` *   An array of values.` |
|      - | 4430 | ` * $delimiter` |
|      - | 4431 | ` *   The optional delimiter parameter sets the field delimiter (one character only).` |
|      - | 4432 | ` * $enclosure` |
|      - | 4433 | ` *  The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 4434 | ` */` |
|      2 | 4435 | `static int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4436 | `{` |
|      - | 4437 | `	const ph7_io_stream *pStream;` |
|      - | 4438 | `	struct csv_data sCsv;` |
|      - | 4439 | `	io_private *pDev;` |
|      - | 4440 | `	char *zEol;` |
|      - | 4441 | `	int eolen;` |
|      3 | 4442 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4443 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4444 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
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
|      3 | 4459 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4460 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4461 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4462 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4463 | `			);` |
|    ! 0 | 4464 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4465 | `		return PH7_OK;` |
|      - | 4466 | `	}` |
|      - | 4467 | `	/* Set default csv separator */` |
|      3 | 4468 | `	sCsv.delimiter = ',';` |
|      3 | 4469 | `	sCsv.enclosure = '"';` |
|      3 | 4470 | `	sCsv.pDev = pDev;` |
|      3 | 4471 | `	sCsv.iCount = 0;` |
|      3 | 4472 | `	if( nArg > 2 ){` |
|      - | 4473 | `		/* User delimiter */` |
|      - | 4474 | `		const char *z;` |
|      - | 4475 | `		int n;` |
|      3 | 4476 | `		z = ph7_value_to_string(apArg[2],&n);` |
|      3 | 4477 | `		if( n > 0 ){` |
|      3 | 4478 | `			sCsv.delimiter = z[0];` |
|      1 | 4479 | `		}` |
|      3 | 4480 | `		if( nArg > 3 ){` |
|      3 | 4481 | `			z = ph7_value_to_string(apArg[3],&n);` |
|      3 | 4482 | `			if( n > 0 ){` |
|      3 | 4483 | `				sCsv.enclosure = z[0];` |
|      1 | 4484 | `			}` |
|      1 | 4485 | `		}` |
|      1 | 4486 | `	}` |
|      - | 4487 | `	/* Iterate throw array entries and write csv data */` |
|      3 | 4488 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      - | 4489 | `	/* Write a line ending */` |
|      - | 4490 | `#ifdef __WINNT__` |
|      1 | 4491 | `	zEol = "\r\n";` |
|      1 | 4492 | `	eolen = (int)sizeof("\r\n")-1;` |
|      - | 4493 | `#else` |
|      - | 4494 | `	/* Assume UNIX LF */` |
|      2 | 4495 | `	zEol = "\n";` |
|      2 | 4496 | `	eolen = (int)sizeof(char);` |
|      - | 4497 | `#endif` |
|      3 | 4498 | `	pDev->pStream->xWrite(pDev->pHandle,(const void *)zEol,eolen);` |
|      3 | 4499 | `	return PH7_OK;` |
|      2 | 4500 | `}` |
|      - | 4501 | `/*` |
|      - | 4502 | ` * fprintf,vfprintf private data.` |
|      - | 4503 | ` * An instance of the following structure is passed to the formatted` |
|      - | 4504 | ` * input consumer callback defined below.` |
|      - | 4505 | ` */` |
|      - | 4506 | `typedef struct fprintf_data fprintf_data;` |
|      - | 4507 | `struct fprintf_data` |
|      - | 4508 | `{` |
|      - | 4509 | `	io_private *pIO;        /* IO stream */` |
|      - | 4510 | `	ph7_int64 nCount;       /* Total number of bytes written */` |
|      - | 4511 | `};` |
|      - | 4512 | `/*` |
|      - | 4513 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 4514 | ` */` |
|     30 | 4515 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4516 | `{` |
|     31 | 4517 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 4518 | `	ph7_int64 n;` |
|      - | 4519 | `	/* Write the formatted data */` |
|     31 | 4520 | `	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);` |
|     31 | 4521 | `	if( n < 1 ){` |
|    ! 0 | 4522 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 4523 | `		/* IO error,abort immediately */` |
|    ! 0 | 4524 | `		return SXERR_ABORT;` |
|      - | 4525 | `	}` |
|      - | 4526 | `	/* Increment counter */` |
|     31 | 4527 | `	pFdata->nCount += n;` |
|     31 | 4528 | `	return PH7_OK;` |
|     16 | 4529 | `}` |
|      - | 4530 | `/*` |
|      - | 4531 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 4532 | ` *  Write a formatted string to a stream.` |
|      - | 4533 | ` * Parameters` |
|      - | 4534 | ` *  $handle` |
|      - | 4535 | ` *   The file pointer.` |
|      - | 4536 | ` *  $format` |
|      - | 4537 | ` *   String format (see sprintf()).` |
|      - | 4538 | ` * Return` |
|      - | 4539 | ` *  The length of the written string.` |
|      - | 4540 | ` */` |
|     16 | 4541 | `static int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4542 | `{` |
|      - | 4543 | `	fprintf_data sFdata;` |
|      - | 4544 | `	const char *zFormat;` |
|      - | 4545 | `	io_private *pDev;` |
|      - | 4546 | `	int nLen;` |
|     17 | 4547 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4548 | `		/* Missing/Invalid arguments,return zero */` |
|    ! 0 | 4549 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4550 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4551 | `		return PH7_OK;` |
|      - | 4552 | `	}` |
|      - | 4553 | `	/* Extract our private data */` |
|     17 | 4554 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4555 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     17 | 4556 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4557 | `		/*Expecting an IO handle */` |
|    ! 0 | 4558 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4559 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4560 | `		return PH7_OK;` |
|      - | 4561 | `	}` |
|      - | 4562 | `	/* Point to the target IO stream device */` |
|     17 | 4563 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4564 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4565 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4566 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4567 | `			);` |
|    ! 0 | 4568 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4569 | `		return PH7_OK;` |
|      - | 4570 | `	}` |
|      - | 4571 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */` |
|      - | 4572 | `	{` |
|     17 | 4573 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|     17 | 4574 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4575 | `			return rcf;` |
|      - | 4576 | `		}` |
|      - | 4577 | `	}` |
|      - | 4578 | `	/* Extract the string format (scalars/null coerce). */` |
|     17 | 4579 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     17 | 4580 | `	if( nLen < 1 ){` |
|      - | 4581 | `		/* Empty string,return zero */` |
|    ! 0 | 4582 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4583 | `		return PH7_OK;` |
|      - | 4584 | `	}` |
|      - | 4585 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4586 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4587 | `	{` |
|     17 | 4588 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     17 | 4589 | `		if( rcv != PH7_OK ){` |
|      3 | 4590 | `			return rcv;` |
|      - | 4591 | `		}` |
|      - | 4592 | `	}` |
|      - | 4593 | `	/* Prepare our private data */` |
|     15 | 4594 | `	sFdata.nCount = 0;` |
|     15 | 4595 | `	sFdata.pIO = pDev;` |
|      - | 4596 | `	/* Format the string */` |
|     15 | 4597 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 4598 | `	/* Return total number of bytes written */` |
|     15 | 4599 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|     15 | 4600 | `	return PH7_OK;` |
|      9 | 4601 | `}` |
|      - | 4602 | `/*` |
|      - | 4603 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 4604 | ` *  Write a formatted string to a stream.` |
|      - | 4605 | ` * Parameters` |
|      - | 4606 | ` *  $handle` |
|      - | 4607 | ` *   The file pointer.` |
|      - | 4608 | ` *  $format` |
|      - | 4609 | ` *   String format (see sprintf()).` |
|      - | 4610 | ` * $args` |
|      - | 4611 | ` *   User arguments.` |
|      - | 4612 | ` * Return` |
|      - | 4613 | ` *  The length of the written string.` |
|      - | 4614 | ` */` |
|      4 | 4615 | `static int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4616 | `{` |
|      - | 4617 | `	fprintf_data sFdata;` |
|      - | 4618 | `	const char *zFormat;` |
|      - | 4619 | `	ph7_hashmap *pMap;` |
|      - | 4620 | `	io_private *pDev;` |
|      - | 4621 | `	SySet sArg;` |
|      - | 4622 | `	int n,nLen;` |
|      5 | 4623 | `	if( nArg < 3 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4624 | `		/* Missing/Invalid arguments,return zero */` |
|      3 | 4625 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|      3 | 4626 | `		ph7_result_int(pCtx,0);` |
|      3 | 4627 | `		return PH7_OK;` |
|      - | 4628 | `	}` |
|      - | 4629 | `	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */` |
|      - | 4630 | `	{` |
|      3 | 4631 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      3 | 4632 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4633 | `			return rcf;` |
|      - | 4634 | `		}` |
|      - | 4635 | `	}` |
|      3 | 4636 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|      - | 4637 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4638 | `		char zBuf[64];` |
|    ! 0 | 4639 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4640 | `			"vfprintf(): Argument #3 ($values) must be of type array, %s given",` |
|    ! 0 | 4641 | `			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|      - | 4642 | `	}` |
|      - | 4643 | `	/* Extract our private data */` |
|      3 | 4644 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4645 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4646 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4647 | `		/*Expecting an IO handle */` |
|    ! 0 | 4648 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4649 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4650 | `		return PH7_OK;` |
|      - | 4651 | `	}` |
|      - | 4652 | `	/* Point to the target IO stream device */` |
|      3 | 4653 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4654 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4655 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4656 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4657 | `			);` |
|    ! 0 | 4658 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4659 | `		return PH7_OK;` |
|      - | 4660 | `	}` |
|      - | 4661 | `	/* Extract the string format */` |
|      3 | 4662 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      3 | 4663 | `	if( nLen < 1 ){` |
|      - | 4664 | `		/* Empty string,return zero */` |
|    ! 0 | 4665 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4666 | `		return PH7_OK;` |
|      - | 4667 | `	}` |
|      - | 4668 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4669 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4670 | `	{` |
|      3 | 4671 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 4672 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4673 | `			return rcv;` |
|      - | 4674 | `		}` |
|      - | 4675 | `	}` |
|      - | 4676 | `	/* Point to hashmap */` |
|      3 | 4677 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 4678 | `	/* Extract arguments from the hashmap */` |
|      3 | 4679 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4680 | `	/* Prepare our private data */` |
|      3 | 4681 | `	sFdata.nCount = 0;` |
|      3 | 4682 | `	sFdata.pIO = pDev;` |
|      - | 4683 | `	/* Format the string */` |
|      3 | 4684 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 4685 | `	/* Return total number of bytes written*/` |
|      3 | 4686 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      3 | 4687 | `	SySetRelease(&sArg);` |
|      3 | 4688 | `	return PH7_OK;` |
|      3 | 4689 | `}` |
|      - | 4690 | `/*` |
|      - | 4691 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 4692 | ` * According to the PHP reference manual:` |
|      - | 4693 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 4694 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 4695 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 4696 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 4697 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 4698 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 4699 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 4700 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 4701 | ` *         exist, attempt to create it.` |
|      - | 4702 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 4703 | ` *          not exist, attempt to create it.` |
|      - | 4704 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 4705 | ` *         already exists,` |
|      - | 4706 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 4707 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 4708 | ` *         the underlying open(2) system call.` |
|      - | 4709 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 4710 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 4711 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 4712 | ` *          is positioned on the beginning of the file.` |
|      - | 4713 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 4714 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 4715 | ` *          be used after the lock is requested).` |
|      - | 4716 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 4717 | ` */` |
|     80 | 4718 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      3 | 4719 | `{` |
|     83 | 4720 | `	const char *zEnd = &zMode[nLen];` |
|     83 | 4721 | `	int iFlag = 0;` |
|      - | 4722 | `	int c;` |
|     83 | 4723 | `	if( nLen < 1 ){` |
|      - | 4724 | `		/* Open in a read-only mode */` |
|    ! 0 | 4725 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 4726 | `	}` |
|     83 | 4727 | `	c = zMode[0];` |
|     83 | 4728 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 4729 | `		/* Read-only access */` |
|     53 | 4730 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|     53 | 4731 | `		zMode++; /* Advance */` |
|     53 | 4732 | `		if( zMode < zEnd ){` |
|     13 | 4733 | `			c = zMode[0];` |
|     13 | 4734 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 4735 | `				/* Read+Write access */` |
|     13 | 4736 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|      6 | 4737 | `			}` |
|      9 | 4738 | `		}` |
|     57 | 4739 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 4740 | `		/* Overwrite mode.` |
|      - | 4741 | `		 * If the file does not exists,try to create it` |
|      - | 4742 | `		 */` |
|     32 | 4743 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     32 | 4744 | `		zMode++; /* Advance */` |
|     32 | 4745 | `		if( zMode < zEnd ){` |
|      5 | 4746 | `			c = zMode[0];` |
|      5 | 4747 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4748 | `				/* Read+Write access */` |
|      5 | 4749 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|      5 | 4750 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|      2 | 4751 | `			}` |
|      4 | 4752 | `		}` |
|     15 | 4753 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 4754 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 4755 | `		 * Create the file if it does not exists.` |
|      - | 4756 | `		 */` |
|    ! 0 | 4757 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4758 | `		zMode++; /* Advance */` |
|    ! 0 | 4759 | `		if( zMode < zEnd ){` |
|    ! 0 | 4760 | `			c = zMode[0];` |
|    ! 0 | 4761 | `			if( c == '+' ){` |
|      - | 4762 | `				/* Read-Write access */` |
|    ! 0 | 4763 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4764 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4765 | `			}` |
|    ! 0 | 4766 | `		}` |
|    ! 0 | 4767 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 4768 | `		/* Exclusive access.` |
|      - | 4769 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 4770 | `		 * Otherwise create a new file.` |
|      - | 4771 | `		 */` |
|    ! 0 | 4772 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|    ! 0 | 4773 | `		zMode++; /* Advance */` |
|    ! 0 | 4774 | `		if( zMode < zEnd ){` |
|    ! 0 | 4775 | `			c = zMode[0];` |
|    ! 0 | 4776 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4777 | `				/* Read-Write access */` |
|    ! 0 | 4778 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4779 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4780 | `			}` |
|    ! 0 | 4781 | `		}` |
|    ! 0 | 4782 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 4783 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 4784 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4785 | `		zMode++; /* Advance */` |
|    ! 0 | 4786 | `		if( zMode < zEnd ){` |
|    ! 0 | 4787 | `			c = zMode[0];` |
|    ! 0 | 4788 | `			if( c == '+' ){` |
|      - | 4789 | `				/* Read-Write access */` |
|    ! 0 | 4790 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4791 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4792 | `			}` |
|    ! 0 | 4793 | `		}` |
|    ! 0 | 4794 | `	}else{` |
|      - | 4795 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 4796 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 4797 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 4798 | `	}` |
|     99 | 4799 | `	while( zMode < zEnd ){` |
|     17 | 4800 | `		c = zMode[0];` |
|     17 | 4801 | `		if( c == 'b' \|\| c == 'B' ){` |
|    ! 0 | 4802 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4803 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|     17 | 4804 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 4805 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 4806 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4807 | `		}` |
|     17 | 4808 | `		zMode++;` |
|      1 | 4809 | `	}` |
|     83 | 4810 | `	return iFlag;` |
|     43 | 4811 | `}` |
|      - | 4812 | `/*` |
|      - | 4813 | ` * Initialize the IO private structure.` |
|      - | 4814 | ` */` |
|   4992 | 4815 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      5 | 4816 | `{` |
|   4997 | 4817 | `	pOut->pStream = pStream;` |
|   4997 | 4818 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   4997 | 4819 | `	pOut->nOfft = 0;` |
|      - | 4820 | `	/* Set the magic number */` |
|   4997 | 4821 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   4997 | 4822 | `}` |
|      - | 4823 | `/*` |
|      - | 4824 | ` * Release the IO private structure.` |
|      - | 4825 | ` */` |
|   4952 | 4826 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      5 | 4827 | `{` |
|   4957 | 4828 | `	SyBlobRelease(&pDev->sBuffer);` |
|   4957 | 4829 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 4830 | `	/* Release the whole structure */` |
|   4957 | 4831 | `	ph7_context_free_chunk(pCtx,pDev);` |
|   4957 | 4832 | `}` |
|      - | 4833 | `/*` |
|      - | 4834 | ` * Reset the IO private structure.` |
|      - | 4835 | ` */` |
|     30 | 4836 | `static void ResetIOPrivate(io_private *pDev)` |
|      2 | 4837 | `{` |
|     32 | 4838 | `	SyBlobReset(&pDev->sBuffer);` |
|     32 | 4839 | `	pDev->nOfft = 0;` |
|     32 | 4840 | `}` |
|      - | 4841 | `/* Forward declaration */` |
|      - | 4842 |  |
|      - | 4843 | `/*` |
|      - | 4844 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 4845 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 4846 | ` * Parameters` |
|      - | 4847 | ` *  $filename` |
|      - | 4848 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 4849 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 4850 | ` *   then a regular file is assumed.` |
|      - | 4851 | ` *  $mode` |
|      - | 4852 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 4853 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 4854 | ` *   modes.` |
|      - | 4855 | ` *  $use_include_path` |
|      - | 4856 | ` *   You can use the optional second parameter and set it to` |
|      - | 4857 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 4858 | ` *  $context` |
|      - | 4859 | ` *   A context stream resource.` |
|      - | 4860 | ` * Return` |
|      - | 4861 | ` *  File handle on success or FALSE on failure.` |
|      - | 4862 | ` */` |
|      - | 4863 | `/*` |
|      - | 4864 | ` * string\|false stream_get_contents(resource $stream, int $maxLength = -1,` |
|      - | 4865 | ` *                                  int $offset = -1)` |
|      - | 4866 | ` *  Read the remaining contents of a stream into a string.` |
|      - | 4867 | ` */` |
|     10 | 4868 | `static int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4869 | `{` |
|      - | 4870 | `	const ph7_io_stream *pStream;` |
|      - | 4871 | `	io_private *pDev;` |
|     11 | 4872 | `	ph7_int64 nMax = -1;` |
|      - | 4873 | `	char zBuf[4096];` |
|      - | 4874 | `	ph7_int64 nRead;` |
|     11 | 4875 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 4876 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4877 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4878 | `		return PH7_OK;` |
|      - | 4879 | `	}` |
|     11 | 4880 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     11 | 4881 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 4882 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4883 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4884 | `		return PH7_OK;` |
|      - | 4885 | `	}` |
|     11 | 4886 | `	pStream = pDev->pStream;` |
|     11 | 4887 | `	if( pStream == 0 \|\| pStream->xRead == 0 ){` |
|    ! 0 | 4888 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4889 | `		return PH7_OK;` |
|      - | 4890 | `	}` |
|     11 | 4891 | `	if( nArg > 1 ){` |
|      5 | 4892 | `		nMax = ph7_value_to_int64(apArg[1]);` |
|      2 | 4893 | `	}` |
|     11 | 4894 | `	if( nArg > 2 ){` |
|      5 | 4895 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|      5 | 4896 | `		if( iOfft >= 0 && pStream->xSeek ){` |
|      5 | 4897 | `			pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/);` |
|      2 | 4898 | `		}` |
|      2 | 4899 | `	}` |
|     11 | 4900 | `	ph7_result_string(pCtx,"",0); /* seed an empty string result */` |
|     22 | 4901 | `	while( nMax != 0 ){` |
|     20 | 4902 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|     20 | 4903 | `		if( nMax > 0 && nMax < nAsk ){` |
|      3 | 4904 | `			nAsk = nMax;` |
|      1 | 4905 | `		}` |
|     20 | 4906 | `		nRead = pStream->xRead(pDev->pHandle,zBuf,nAsk);` |
|     20 | 4907 | `		if( nRead < 1 ){` |
|      9 | 4908 | `			break;` |
|      - | 4909 | `		}` |
|     12 | 4910 | `		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */` |
|     12 | 4911 | `		if( nMax > 0 ){` |
|      3 | 4912 | `			nMax -= nRead;` |
|      1 | 4913 | `		}` |
|      1 | 4914 | `	}` |
|     11 | 4915 | `	return PH7_OK;` |
|      6 | 4916 | `}` |
|      - | 4917 | `/*` |
|      - | 4918 | ` * array stream_get_wrappers(void) — names of the registered stream devices.` |
|      - | 4919 | ` */` |
|      4 | 4920 | `static int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4921 | `{` |
|      - | 4922 | `	ph7_value *pArr,*pV;` |
|      - | 4923 | `	ph7_io_stream **apDev;` |
|      - | 4924 | `	sxu32 n;` |
|      2 | 4925 | `	SXUNUSED(nArg);` |
|      2 | 4926 | `	SXUNUSED(apArg);` |
|      6 | 4927 | `	pArr = ph7_context_new_array(pCtx);` |
|      6 | 4928 | `	pV = ph7_context_new_scalar(pCtx);` |
|      6 | 4929 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 4930 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4931 | `		return PH7_OK;` |
|      - | 4932 | `	}` |
|      6 | 4933 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|     24 | 4934 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|     20 | 4935 | `		ph7_value_string(pV,apDev[n]->zName,-1);` |
|     20 | 4936 | `		ph7_array_add_elem(pArr,0,pV);` |
|     20 | 4937 | `		ph7_value_reset_string_cursor(pV);` |
|     11 | 4938 | `	}` |
|      6 | 4939 | `	ph7_result_value(pCtx,pArr);` |
|      6 | 4940 | `	return PH7_OK;` |
|      4 | 4941 | `}` |
|      - | 4942 | `/*` |
|      - | 4943 | ` * array stream_get_meta_data(resource $stream) — best-effort php shape over` |
|      - | 4944 | ` * the io_private state (uri/wrapper_type/seekable/eof; recorded approximation).` |
|      - | 4945 | ` */` |
|      2 | 4946 | `static int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4947 | `{` |
|      - | 4948 | `	io_private *pDev;` |
|      - | 4949 | `	ph7_value *pArr,*pV;` |
|      3 | 4950 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 4951 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4952 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4953 | `		return PH7_OK;` |
|      - | 4954 | `	}` |
|      3 | 4955 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      3 | 4956 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 4957 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4958 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4959 | `		return PH7_OK;` |
|      - | 4960 | `	}` |
|      3 | 4961 | `	pArr = ph7_context_new_array(pCtx);` |
|      3 | 4962 | `	pV = ph7_context_new_scalar(pCtx);` |
|      3 | 4963 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 4964 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4965 | `		return PH7_OK;` |
|      - | 4966 | `	}` |
|      3 | 4967 | `	ph7_value_bool(pV,0);` |
|      3 | 4968 | `	ph7_array_add_strkey_elem(pArr,"timed_out",pV);` |
|      3 | 4969 | `	ph7_value_bool(pV,1);` |
|      3 | 4970 | `	ph7_array_add_strkey_elem(pArr,"blocked",pV);` |
|      - | 4971 | `	/* eof is best-effort: a read probe would consume state on unseekable` |
|      - | 4972 | `	 * devices, so report FALSE and let feof() answer properly */` |
|      3 | 4973 | `	ph7_value_bool(pV,0);` |
|      3 | 4974 | `	ph7_array_add_strkey_elem(pArr,"eof",pV);` |
|      3 | 4975 | `	ph7_value_int(pV,0);` |
|      3 | 4976 | `	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);` |
|      3 | 4977 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 4978 | `	ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);` |
|      3 | 4979 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 4980 | `	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);` |
|      3 | 4981 | `	ph7_array_add_strkey_elem(pArr,"stream_type",pV);` |
|      3 | 4982 | `	ph7_value_reset_string_cursor(pV);` |
|      3 | 4983 | `	ph7_value_bool(pV,pDev->pStream && pDev->pStream->xSeek != 0);` |
|      3 | 4984 | `	ph7_array_add_strkey_elem(pArr,"seekable",pV);` |
|      3 | 4985 | `	ph7_result_value(pCtx,pArr);` |
|      3 | 4986 | `	return PH7_OK;` |
|      2 | 4987 | `}` |
|      - | 4988 | `/*` |
|      - | 4989 | ` * stream_context_create([array $options[, array $params]]) — INERT: PHL has` |
|      - | 4990 | ` * no context plumbing yet; the options array itself is returned so code that` |
|      - | 4991 | ` * creates and passes contexts keeps working (recorded divergence: not a` |
|      - | 4992 | ` * resource, options unconsumed).` |
|      - | 4993 | ` */` |
|      2 | 4994 | `static int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4995 | `{` |
|      3 | 4996 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|      3 | 4997 | `		ph7_result_value(pCtx,apArg[0]);` |
|      2 | 4998 | `	}else{` |
|    ! 0 | 4999 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 5000 | `		if( pArr == 0 ){` |
|    ! 0 | 5001 | `			ph7_result_null(pCtx);` |
|    ! 0 | 5002 | `			return PH7_OK;` |
|      - | 5003 | `		}` |
|    ! 0 | 5004 | `		ph7_result_value(pCtx,pArr);` |
|      - | 5005 | `	}` |
|      3 | 5006 | `	return PH7_OK;` |
|      2 | 5007 | `}` |
|      - | 5008 | `/*` |
|      - | 5009 | ` * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a` |
|      - | 5010 | ` * small struct carrying the OS socket plus an EOF latch, so feof() works.` |
|      - | 5011 | ` */` |
|      - | 5012 | `#ifdef PH7_ENABLE_NET` |
|      - | 5013 | `typedef struct sock_private sock_private;` |
|      - | 5014 | `struct sock_private` |
|      - | 5015 | `{` |
|      - | 5016 | `	ph7_vm *pVm;` |
|      - | 5017 | `	ph7_socket sock;` |
|      - | 5018 | `	int bEof;` |
|      - | 5019 | `};` |
|     11 | 5020 | `static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|    ! 0 | 5021 | `{` |
|     11 | 5022 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 5023 | `	int n;` |
|     11 | 5024 | `	if( pSock == 0 \|\| pSock->bEof ){` |
|      1 | 5025 | `		return 0;` |
|      - | 5026 | `	}` |
|     10 | 5027 | `	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);` |
|     10 | 5028 | `	if( n <= 0 ){` |
|      4 | 5029 | `		pSock->bEof = 1;` |
|      4 | 5030 | `		return 0;` |
|      - | 5031 | `	}` |
|      6 | 5032 | `	return (ph7_int64)n;` |
|      5 | 5033 | `}` |
|      4 | 5034 | `static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 5035 | `{` |
|      4 | 5036 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 5037 | `	int n;` |
|      4 | 5038 | `	if( pSock == 0 ){` |
|    ! 0 | 5039 | `		return -1;` |
|      - | 5040 | `	}` |
|      4 | 5041 | `	n = PH7_NetSendAll(pSock->sock,pBuf,(int)nWrite);` |
|      4 | 5042 | `	return n < 0 ? -1 : (ph7_int64)n;` |
|      2 | 5043 | `}` |
|      4 | 5044 | `static void SockStreamData_Close(void *pHandle)` |
|    ! 0 | 5045 | `{` |
|      4 | 5046 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      4 | 5047 | `	if( pSock == 0 ){` |
|    ! 0 | 5048 | `		return;` |
|      - | 5049 | `	}` |
|      4 | 5050 | `	PH7_NetClose(pSock->sock);` |
|      4 | 5051 | `	SyMemBackendFree(&pSock->pVm->sAllocator,pSock);` |
|      2 | 5052 | `}` |
|      - | 5053 | `/* xOpen for "host:port" (the scheme is already stripped by the device lookup) */` |
|    ! 0 | 5054 | `static int SockStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|    ! 0 | 5055 | `{` |
|      - | 5056 | `	sock_private *pSock;` |
|      - | 5057 | `	ph7_socket sock;` |
|      - | 5058 | `	char zHost[256];` |
|      - | 5059 | `	const char *zColon;` |
|    ! 0 | 5060 | `	int iPort = 0,iErrno = 0;` |
|    ! 0 | 5061 | `	const char *zErr = "";` |
|    ! 0 | 5062 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|    ! 0 | 5063 | `	SXUNUSED(iMode);` |
|    ! 0 | 5064 | `	if( pVm == 0 ){` |
|    ! 0 | 5065 | `		return -1;` |
|      - | 5066 | `	}` |
|    ! 0 | 5067 | `	zColon = SyStrlen(zName) ? &zName[SyStrlen(zName)-1] : zName;` |
|    ! 0 | 5068 | `	while( zColon > zName && zColon[0] != ':' ){` |
|    ! 0 | 5069 | `		zColon--;` |
|    ! 0 | 5070 | `	}` |
|    ! 0 | 5071 | `	if( zColon <= zName \|\| zColon[0] != ':' ){` |
|    ! 0 | 5072 | `		return -1;` |
|      - | 5073 | `	}` |
|      - | 5074 | `	{` |
|    ! 0 | 5075 | `		sxu32 n = (sxu32)(zColon - zName);` |
|    ! 0 | 5076 | `		if( n >= sizeof(zHost) ){` |
|    ! 0 | 5077 | `			n = sizeof(zHost) - 1;` |
|    ! 0 | 5078 | `		}` |
|    ! 0 | 5079 | `		SyMemcpy(zName,zHost,n);` |
|    ! 0 | 5080 | `		zHost[n] = 0;` |
|      - | 5081 | `	}` |
|      - | 5082 | `	{` |
|    ! 0 | 5083 | `		sxi32 iTmp = 0;` |
|    ! 0 | 5084 | `		SyStrToInt32(&zColon[1],(sxu32)SyStrlen(&zColon[1]),(void *)&iTmp,0);` |
|    ! 0 | 5085 | `		iPort = (int)iTmp;` |
|      - | 5086 | `	}` |
|    ! 0 | 5087 | `	sock = PH7_NetConnect(zHost,iPort,0,&iErrno,&zErr);` |
|    ! 0 | 5088 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    ! 0 | 5089 | `		return -1;` |
|      - | 5090 | `	}` |
|    ! 0 | 5091 | `	pSock = (sock_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(sock_private));` |
|    ! 0 | 5092 | `	if( pSock == 0 ){` |
|    ! 0 | 5093 | `		PH7_NetClose(sock);` |
|    ! 0 | 5094 | `		return -1;` |
|      - | 5095 | `	}` |
|    ! 0 | 5096 | `	pSock->pVm = pVm;` |
|    ! 0 | 5097 | `	pSock->sock = sock;` |
|    ! 0 | 5098 | `	pSock->bEof = 0;` |
|    ! 0 | 5099 | `	*ppHandle = (void *)pSock;` |
|    ! 0 | 5100 | `	return PH7_OK;` |
|    ! 0 | 5101 | `}` |
|      - | 5102 | `static const ph7_io_stream sTCP_Stream = {` |
|      - | 5103 | `	"tcp",` |
|      - | 5104 | `	PH7_IO_STREAM_VERSION,` |
|      - | 5105 | `	SockStreamData_Open, /* xOpen */` |
|      - | 5106 | `	0,   /* xOpenDir */` |
|      - | 5107 | `	SockStreamData_Close,/* xClose */` |
|      - | 5108 | `	0,  /* xCloseDir */` |
|      - | 5109 | `	SockStreamData_Read, /* xRead */` |
|      - | 5110 | `	0,  /* xReadDir */` |
|      - | 5111 | `	SockStreamData_Write,/* xWrite */` |
|      - | 5112 | `	0,  /* xSeek (sockets are not seekable) */` |
|      - | 5113 | `	0,  /* xLock */` |
|      - | 5114 | `	0,  /* xRewindDir */` |
|      - | 5115 | `	0,  /* xTell */` |
|      - | 5116 | `	0,  /* xTrunc */` |
|      - | 5117 | `	0,  /* xSync */` |
|      - | 5118 | `	0   /* xStat */` |
|      - | 5119 | `};` |
|      - | 5120 | `#endif /* PH7_ENABLE_NET */` |
|      - | 5121 | `/*` |
|      - | 5122 | ` * Userland stream wrappers (stream_wrapper_register). The engine's device` |
|      - | 5123 | ` * callbacks receive no device pointer, so each registered wrapper needs its` |
|      - | 5124 | ` * OWN xOpen thunk: PHL keeps a bounded pool of PHL_UWRAP_MAX slots, each with` |
|      - | 5125 | ` * a static thunk that knows its index (recorded limit; php has no cap).` |
|      - | 5126 | ` * The handle carries the userland object, and every stream op dispatches the` |
|      - | 5127 | ` * php streamWrapper protocol method on it.` |
|      - | 5128 | ` */` |
|      - | 5129 | `#define PHL_UWRAP_MAX 8` |
|      - | 5130 | `typedef struct uwrap_slot uwrap_slot;` |
|      - | 5131 | `struct uwrap_slot` |
|      - | 5132 | `{` |
|      - | 5133 | `	ph7_vm *pVm;              /* owning VM (0 = free slot) */` |
|      - | 5134 | `	char zScheme[32];         /* protocol name */` |
|      - | 5135 | `	char zClass[128];         /* userland wrapper class */` |
|      - | 5136 | `	ph7_io_stream sStream;    /* the device handed to the VM */` |
|      - | 5137 | `};` |
|      - | 5138 | `typedef struct uwrap_handle uwrap_handle;` |
|      - | 5139 | `struct uwrap_handle` |
|      - | 5140 | `{` |
|      - | 5141 | `	ph7_vm *pVm;` |
|      - | 5142 | `	ph7_class_instance *pObj; /* the wrapper instance (one per open stream) */` |
|      - | 5143 | `	int iSlot;` |
|      - | 5144 | `	int bEof;` |
|      - | 5145 | `};` |
|      - | 5146 | `static uwrap_slot g_aUwrap[PHL_UWRAP_MAX];` |
|      - | 5147 | `/* Call $obj->$zMethod(...) and copy the result into pResult (may be 0) */` |
|     26 | 5148 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|      - | 5149 | `	ph7_value *pResult)` |
|      1 | 5150 | `{` |
|      - | 5151 | `	ph7_class_method *pMeth;` |
|     27 | 5152 | `	if( pH == 0 \|\| pH->pObj == 0 ){` |
|    ! 0 | 5153 | `		return -1;` |
|      - | 5154 | `	}` |
|     27 | 5155 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|     27 | 5156 | `	if( pMeth == 0 ){` |
|    ! 0 | 5157 | `		return -1;` |
|      - | 5158 | `	}` |
|     27 | 5159 | `	if( PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|    ! 0 | 5160 | `		return -1;` |
|      - | 5161 | `	}` |
|     27 | 5162 | `	return 0;` |
|     14 | 5163 | `}` |
|      8 | 5164 | `static ph7_int64 UwrapRead(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|      1 | 5165 | `{` |
|      9 | 5166 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5167 | `	ph7_value sArg,sRet;` |
|      - | 5168 | `	const char *zData;` |
|      9 | 5169 | `	int nData = 0;` |
|      9 | 5170 | `	ph7_int64 n = 0;` |
|      9 | 5171 | `	if( pH == 0 \|\| pH->bEof ){` |
|    ! 0 | 5172 | `		return 0;` |
|      - | 5173 | `	}` |
|      9 | 5174 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      9 | 5175 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      9 | 5176 | `	ph7_value_int64(&sArg,nRead);` |
|      - | 5177 | `	{` |
|      - | 5178 | `		ph7_value *apArg[1];` |
|      9 | 5179 | `		apArg[0] = &sArg;` |
|      9 | 5180 | `		if( UwrapCall(pH,"stream_read",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5181 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5182 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5183 | `			return -1;` |
|      - | 5184 | `		}` |
|      - | 5185 | `	}` |
|      9 | 5186 | `	zData = ph7_value_to_string(&sRet,&nData);` |
|      9 | 5187 | `	if( nData > 0 ){` |
|      7 | 5188 | `		if( (ph7_int64)nData > nRead ){` |
|    ! 0 | 5189 | `			nData = (int)nRead;` |
|    ! 0 | 5190 | `		}` |
|      7 | 5191 | `		SyMemcpy(zData,pBuffer,(sxu32)nData);` |
|      7 | 5192 | `		n = nData;` |
|      4 | 5193 | `	}else{` |
|      3 | 5194 | `		pH->bEof = 1;` |
|      - | 5195 | `	}` |
|      9 | 5196 | `	PH7_MemObjRelease(&sArg);` |
|      9 | 5197 | `	PH7_MemObjRelease(&sRet);` |
|      9 | 5198 | `	return n;` |
|      5 | 5199 | `}` |
|      2 | 5200 | `static ph7_int64 UwrapWrite(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 5201 | `{` |
|      3 | 5202 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5203 | `	ph7_value sArg,sRet;` |
|      - | 5204 | `	ph7_int64 n;` |
|      3 | 5205 | `	if( pH == 0 ){` |
|    ! 0 | 5206 | `		return -1;` |
|      - | 5207 | `	}` |
|      3 | 5208 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      3 | 5209 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5210 | `	ph7_value_string(&sArg,(const char *)pBuf,(int)nWrite);` |
|      - | 5211 | `	{` |
|      - | 5212 | `		ph7_value *apArg[1];` |
|      3 | 5213 | `		apArg[0] = &sArg;` |
|      3 | 5214 | `		if( UwrapCall(pH,"stream_write",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5215 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5216 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5217 | `			return -1;` |
|      - | 5218 | `		}` |
|      - | 5219 | `	}` |
|      3 | 5220 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 5221 | `	PH7_MemObjRelease(&sArg);` |
|      3 | 5222 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5223 | `	return n;` |
|      2 | 5224 | `}` |
|      2 | 5225 | `static int UwrapSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 5226 | `{` |
|      3 | 5227 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5228 | `	ph7_value sOfft,sWhence,sRet;` |
|      - | 5229 | `	ph7_value *apArg[2];` |
|      - | 5230 | `	int rc;` |
|      3 | 5231 | `	if( pH == 0 ){` |
|    ! 0 | 5232 | `		return -1;` |
|      - | 5233 | `	}` |
|      3 | 5234 | `	PH7_MemObjInit(pH->pVm,&sOfft);` |
|      3 | 5235 | `	PH7_MemObjInit(pH->pVm,&sWhence);` |
|      3 | 5236 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5237 | `	ph7_value_int64(&sOfft,iOfft);` |
|      3 | 5238 | `	ph7_value_int(&sWhence,whence);` |
|      3 | 5239 | `	apArg[0] = &sOfft;` |
|      3 | 5240 | `	apArg[1] = &sWhence;` |
|      3 | 5241 | `	rc = UwrapCall(pH,"stream_seek",2,apArg,&sRet);` |
|      3 | 5242 | `	if( rc == 0 ){` |
|      3 | 5243 | `		pH->bEof = 0;` |
|      3 | 5244 | `		rc = ph7_value_to_bool(&sRet) ? PH7_OK : -1;` |
|      1 | 5245 | `	}` |
|      3 | 5246 | `	PH7_MemObjRelease(&sOfft);` |
|      3 | 5247 | `	PH7_MemObjRelease(&sWhence);` |
|      3 | 5248 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5249 | `	return rc;` |
|      2 | 5250 | `}` |
|      2 | 5251 | `static ph7_int64 UwrapTell(void *pHandle)` |
|      1 | 5252 | `{` |
|      3 | 5253 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5254 | `	ph7_value sRet;` |
|      - | 5255 | `	ph7_int64 n;` |
|      3 | 5256 | `	if( pH == 0 ){` |
|    ! 0 | 5257 | `		return -1;` |
|      - | 5258 | `	}` |
|      3 | 5259 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5260 | `	if( UwrapCall(pH,"stream_tell",0,0,&sRet) != 0 ){` |
|    ! 0 | 5261 | `		PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5262 | `		return -1;` |
|      - | 5263 | `	}` |
|      3 | 5264 | `	n = ph7_value_to_int64(&sRet);` |
|      3 | 5265 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5266 | `	return n;` |
|      2 | 5267 | `}` |
|      6 | 5268 | `static void UwrapClose(void *pHandle)` |
|      1 | 5269 | `{` |
|      7 | 5270 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      7 | 5271 | `	if( pH == 0 ){` |
|    ! 0 | 5272 | `		return;` |
|      - | 5273 | `	}` |
|      7 | 5274 | `	UwrapCall(pH,"stream_close",0,0,0);` |
|      7 | 5275 | `	if( pH->pObj ){` |
|      7 | 5276 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|      3 | 5277 | `	}` |
|      7 | 5278 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|      4 | 5279 | `}` |
|      - | 5280 | `/* Shared open: instantiate the wrapper class and call stream_open() */` |
|      6 | 5281 | `static int UwrapOpenSlot(int iSlot,const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|      1 | 5282 | `{` |
|      7 | 5283 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|      7 | 5284 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|      - | 5285 | `	ph7_class *pClass;` |
|      - | 5286 | `	uwrap_handle *pH;` |
|      - | 5287 | `	ph7_value sPath,sMode,sOpts,sOpened,sRet;` |
|      - | 5288 | `	ph7_value *apArg[4];` |
|      - | 5289 | `	int rc;` |
|      7 | 5290 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|    ! 0 | 5291 | `		return -1;` |
|      - | 5292 | `	}` |
|      7 | 5293 | `	pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)SyStrlen(pSlot->zClass),TRUE,0);` |
|      7 | 5294 | `	if( pClass == 0 ){` |
|    ! 0 | 5295 | `		return -1;` |
|      - | 5296 | `	}` |
|      7 | 5297 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|      7 | 5298 | `	if( pH == 0 ){` |
|    ! 0 | 5299 | `		return -1;` |
|      - | 5300 | `	}` |
|      7 | 5301 | `	pH->pVm = pVm;` |
|      7 | 5302 | `	pH->iSlot = iSlot;` |
|      7 | 5303 | `	pH->bEof = 0;` |
|      7 | 5304 | `	pH->pObj = PH7_NewClassInstance(pVm,pClass);` |
|      7 | 5305 | `	if( pH->pObj == 0 ){` |
|    ! 0 | 5306 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5307 | `		return -1;` |
|      - | 5308 | `	}` |
|      - | 5309 | `	/* php hands stream_open the FULL url, scheme included */` |
|      7 | 5310 | `	PH7_MemObjInit(pVm,&sPath);` |
|      7 | 5311 | `	PH7_MemObjInit(pVm,&sMode);` |
|      7 | 5312 | `	PH7_MemObjInit(pVm,&sOpts);` |
|      7 | 5313 | `	PH7_MemObjInit(pVm,&sRet);` |
|      - | 5314 | `	/* $opened_path is BY REFERENCE: the callee's binding needs a real memobj` |
|      - | 5315 | `	 * slot (a stack ph7_value has nIdx == SXU32_HIGH and the engine rejects` |
|      - | 5316 | `	 * it as "could not be passed by reference"). */` |
|      - | 5317 | `	{` |
|      7 | 5318 | `		ph7_value *pRefSlot = PH7_ReserveMemObj(pVm);` |
|      7 | 5319 | `		if( pRefSlot == 0 ){` |
|    ! 0 | 5320 | `			PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5321 | `			SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5322 | `			return -1;` |
|      - | 5323 | `		}` |
|      7 | 5324 | `		PH7_MemObjInit(pVm,&sOpened);` |
|      7 | 5325 | `		sOpened.nIdx = pRefSlot->nIdx;` |
|      - | 5326 | `	}` |
|      - | 5327 | `	{` |
|      - | 5328 | `		SyBlob sUrl;` |
|      7 | 5329 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|      7 | 5330 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|      7 | 5331 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|      7 | 5332 | `		SyBlobRelease(&sUrl);` |
|      - | 5333 | `	}` |
|      9 | 5334 | `	ph7_value_string(&sMode,(iMode & PH7_IO_OPEN_WRONLY) ? "w"` |
|      4 | 5335 | `		: ((iMode & PH7_IO_OPEN_APPEND) ? "a" : "r"),-1);` |
|      7 | 5336 | `	ph7_value_int(&sOpts,0);` |
|      7 | 5337 | `	apArg[0] = &sPath;` |
|      7 | 5338 | `	apArg[1] = &sMode;` |
|      7 | 5339 | `	apArg[2] = &sOpts;` |
|      7 | 5340 | `	apArg[3] = &sOpened;` |
|      7 | 5341 | `	rc = UwrapCall(pH,"stream_open",4,apArg,&sRet);` |
|      7 | 5342 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|    ! 0 | 5343 | `		rc = -1;` |
|    ! 0 | 5344 | `	}` |
|      7 | 5345 | `	PH7_MemObjRelease(&sPath);` |
|      7 | 5346 | `	PH7_MemObjRelease(&sMode);` |
|      7 | 5347 | `	PH7_MemObjRelease(&sOpts);` |
|      7 | 5348 | `	PH7_MemObjRelease(&sOpened);` |
|      7 | 5349 | `	PH7_MemObjRelease(&sRet);` |
|      7 | 5350 | `	if( rc != 0 ){` |
|    ! 0 | 5351 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5352 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5353 | `		return -1;` |
|      - | 5354 | `	}` |
|      7 | 5355 | `	*ppHandle = (void *)pH;` |
|      7 | 5356 | `	return PH7_OK;` |
|      4 | 5357 | `}` |
|      - | 5358 | `/* One xOpen thunk per slot (the device callbacks get no device pointer) */` |
|      - | 5359 | `#define PHL_UWRAP_THUNK(N) \` |
|      - | 5360 | `	static int UwrapOpen##N(const char *zName,int iMode,ph7_value *pResource,void **ppHandle) \` |
|      - | 5361 | `	{ return UwrapOpenSlot(N,zName,iMode,pResource,ppHandle); }` |
|      7 | 5362 | `PHL_UWRAP_THUNK(0)` |
|    ! 0 | 5363 | `PHL_UWRAP_THUNK(1)` |
|    ! 0 | 5364 | `PHL_UWRAP_THUNK(2)` |
|    ! 0 | 5365 | `PHL_UWRAP_THUNK(3)` |
|    ! 0 | 5366 | `PHL_UWRAP_THUNK(4)` |
|    ! 0 | 5367 | `PHL_UWRAP_THUNK(5)` |
|    ! 0 | 5368 | `PHL_UWRAP_THUNK(6)` |
|    ! 0 | 5369 | `PHL_UWRAP_THUNK(7)` |
|      - | 5370 | `static int (* const g_aUwrapOpen[PHL_UWRAP_MAX])(const char *,int,ph7_value *,void **) = {` |
|      - | 5371 | `	UwrapOpen0,UwrapOpen1,UwrapOpen2,UwrapOpen3,` |
|      - | 5372 | `	UwrapOpen4,UwrapOpen5,UwrapOpen6,UwrapOpen7` |
|      - | 5373 | `};` |
|      - | 5374 | `/*` |
|      - | 5375 | ` * bool stream_wrapper_register(string $protocol, string $class, int $flags = 0)` |
|      - | 5376 | ` * bool stream_wrapper_unregister(string $protocol)` |
|      - | 5377 | ` */` |
|      2 | 5378 | `static int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5379 | `{` |
|      - | 5380 | `	const char *zScheme,*zClass;` |
|      3 | 5381 | `	int nScheme,nClass,i,iFree = -1;` |
|      3 | 5382 | `	if( nArg < 2 ){` |
|    ! 0 | 5383 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5384 | `		return PH7_OK;` |
|      - | 5385 | `	}` |
|      3 | 5386 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 5387 | `	zClass  = ph7_value_to_string(apArg[1],&nClass);` |
|      2 | 5388 | `	if( nScheme < 1 \|\| nScheme >= (int)sizeof(g_aUwrap[0].zScheme)` |
|      3 | 5389 | `	 \|\| nClass < 1 \|\| nClass >= (int)sizeof(g_aUwrap[0].zClass) ){` |
|    ! 0 | 5390 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5391 | `		return PH7_OK;` |
|      - | 5392 | `	}` |
|      - | 5393 | `	/* php: registering an already-taken protocol warns and returns false.` |
|      - | 5394 | `	 * (Scan the device list directly — PH7_VmGetStreamDevice falls back to` |
|      - | 5395 | `	 * the DEFAULT device for a scheme-less name, so it can't answer this.) */` |
|      - | 5396 | `	{` |
|      3 | 5397 | `		ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|      - | 5398 | `		sxu32 n;` |
|     11 | 5399 | `		for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|      8 | 5400 | `			if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|      7 | 5401 | `			 && SyStrnicmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0 ){` |
|    ! 0 | 5402 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5403 | `					"Protocol %.*s:// is already defined.",nScheme,zScheme);` |
|    ! 0 | 5404 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 5405 | `				return PH7_OK;` |
|      - | 5406 | `			}` |
|      5 | 5407 | `		}` |
|      - | 5408 | `	}` |
|      3 | 5409 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      3 | 5410 | `		if( g_aUwrap[i].pVm == 0 ){` |
|      3 | 5411 | `			iFree = i;` |
|      3 | 5412 | `			break;` |
|      - | 5413 | `		}` |
|    ! 0 | 5414 | `	}` |
|      3 | 5415 | `	if( iFree < 0 ){` |
|    ! 0 | 5416 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5417 | `			"Too many registered stream wrappers (PHL limit: %d)",PHL_UWRAP_MAX);` |
|    ! 0 | 5418 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5419 | `		return PH7_OK;` |
|      - | 5420 | `	}` |
|      - | 5421 | `	{` |
|      3 | 5422 | `		uwrap_slot *pSlot = &g_aUwrap[iFree];` |
|      3 | 5423 | `		SyMemcpy(zScheme,pSlot->zScheme,(sxu32)nScheme);` |
|      3 | 5424 | `		pSlot->zScheme[nScheme] = 0;` |
|      3 | 5425 | `		SyMemcpy(zClass,pSlot->zClass,(sxu32)nClass);` |
|      3 | 5426 | `		pSlot->zClass[nClass] = 0;` |
|      3 | 5427 | `		pSlot->pVm = pCtx->pVm;` |
|      3 | 5428 | `		SyZero(&pSlot->sStream,sizeof(ph7_io_stream));` |
|      3 | 5429 | `		pSlot->sStream.zName = pSlot->zScheme;` |
|      3 | 5430 | `		pSlot->sStream.iVersion = PH7_IO_STREAM_VERSION;` |
|      3 | 5431 | `		pSlot->sStream.xOpen = g_aUwrapOpen[iFree];` |
|      3 | 5432 | `		pSlot->sStream.xClose = UwrapClose;` |
|      3 | 5433 | `		pSlot->sStream.xRead = UwrapRead;` |
|      3 | 5434 | `		pSlot->sStream.xWrite = UwrapWrite;` |
|      3 | 5435 | `		pSlot->sStream.xSeek = UwrapSeek;` |
|      3 | 5436 | `		pSlot->sStream.xTell = UwrapTell;` |
|      3 | 5437 | `		ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_IO_STREAM,&pSlot->sStream);` |
|      - | 5438 | `	}` |
|      3 | 5439 | `	ph7_result_bool(pCtx,1);` |
|      3 | 5440 | `	return PH7_OK;` |
|      2 | 5441 | `}` |
|      2 | 5442 | `static int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5443 | `{` |
|      - | 5444 | `	const char *zScheme;` |
|      - | 5445 | `	int nScheme,i;` |
|      3 | 5446 | `	if( nArg < 1 ){` |
|    ! 0 | 5447 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5448 | `		return PH7_OK;` |
|      - | 5449 | `	}` |
|      3 | 5450 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|      3 | 5451 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|      2 | 5452 | `		if( g_aUwrap[i].pVm == pCtx->pVm` |
|      2 | 5453 | `		 && (int)SyStrlen(g_aUwrap[i].zScheme) == nScheme` |
|      3 | 5454 | `		 && SyMemcmp(g_aUwrap[i].zScheme,zScheme,(sxu32)nScheme) == 0 ){` |
|      - | 5455 | `			/* The device stays in the VM's list (the engine has no removal` |
|      - | 5456 | `			 * API); neutering the slot makes every later open fail, which is` |
|      - | 5457 | `			 * what unregister means to a script — recorded. */` |
|      3 | 5458 | `			g_aUwrap[i].pVm = 0;` |
|      3 | 5459 | `			ph7_result_bool(pCtx,1);` |
|      3 | 5460 | `			return PH7_OK;` |
|      - | 5461 | `		}` |
|    ! 0 | 5462 | `	}` |
|    ! 0 | 5463 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 | 5464 | `	return PH7_OK;` |
|      2 | 5465 | `}` |
|      - | 5466 | `#ifdef PH7_ENABLE_NET` |
|      - | 5467 | `/*` |
|      - | 5468 | ` * resource\|false fsockopen(string $hostname, int $port = -1, int &$error_code,` |
|      - | 5469 | ` *                          string &$error_message, ?float $timeout = null)` |
|      - | 5470 | ` * resource\|false stream_socket_client(string $address, int &$error_code,` |
|      - | 5471 | ` *                          string &$error_message, ?float $timeout = null, ...)` |
|      - | 5472 | ` * TCP only (the recorded scope: no ssl://, udp:// or unix:// yet).` |
|      - | 5473 | ` */` |
|      6 | 5474 | `static int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 5475 | `{` |
|      6 | 5476 | `	const char *zFunc = ph7_function_name(pCtx);` |
|      6 | 5477 | `	int bClientForm = (zFunc[0] == 's'); /* stream_socket_client */` |
|      6 | 5478 | `	const char *zTarget,*zErr = "";` |
|      - | 5479 | `	char zHost[256];` |
|      6 | 5480 | `	int nTarget,iPort = -1,iErrno = 0,iTimeoutMs = 0;` |
|      - | 5481 | `	ph7_socket sock;` |
|      - | 5482 | `	io_private *pDev;` |
|      - | 5483 | `	sock_private *pSock;` |
|      6 | 5484 | `	int iArgErrno = bClientForm ? 1 : 2;` |
|      6 | 5485 | `	int iArgErrstr = bClientForm ? 2 : 3;` |
|      6 | 5486 | `	int iArgTimeout = bClientForm ? 3 : 4;` |
|      6 | 5487 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    ! 0 | 5488 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5489 | `		return PH7_OK;` |
|      - | 5490 | `	}` |
|      6 | 5491 | `	zTarget = ph7_value_to_string(apArg[0],&nTarget);` |
|      - | 5492 | `	/* Strip a scheme; only tcp:// (and the bare form) are supported */` |
|      - | 5493 | `	{` |
|      6 | 5494 | `		const char *z = zTarget,*zEnd = &zTarget[nTarget];` |
|      6 | 5495 | `		const char *zSep = 0;` |
|     32 | 5496 | `		while( z < zEnd - 2 ){` |
|     30 | 5497 | `			if( z[0] == ':' && z[1] == '/' && z[2] == '/' ){` |
|      4 | 5498 | `				zSep = z;` |
|      4 | 5499 | `				break;` |
|      - | 5500 | `			}` |
|     26 | 5501 | `			z++;` |
|    ! 0 | 5502 | `		}` |
|      6 | 5503 | `		if( zSep ){` |
|      4 | 5504 | `			if( !((zSep - zTarget) == 3 && SyStrnicmp(zTarget,"tcp",3) == 0) ){` |
|    ! 0 | 5505 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5506 | `					"Unable to connect to %.*s (unsupported transport; PHL supports tcp:// only)",` |
|    ! 0 | 5507 | `					nTarget,zTarget);` |
|    ! 0 | 5508 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 5509 | `				return PH7_OK;` |
|      - | 5510 | `			}` |
|      4 | 5511 | `			nTarget -= (int)(zSep + 3 - zTarget);` |
|      4 | 5512 | `			zTarget = zSep + 3;` |
|      2 | 5513 | `		}` |
|      - | 5514 | `	}` |
|      - | 5515 | `	/* host[:port] */` |
|      - | 5516 | `	{` |
|      6 | 5517 | `		int i = nTarget - 1;` |
|      6 | 5518 | `		int nHost = nTarget;` |
|     48 | 5519 | `		while( i > 0 && zTarget[i] != ':' ){` |
|     42 | 5520 | `			i--;` |
|    ! 0 | 5521 | `		}` |
|      6 | 5522 | `		if( i > 0 && zTarget[i] == ':' ){` |
|      2 | 5523 | `			sxi32 iTmp = 0;` |
|      2 | 5524 | `			SyStrToInt32(&zTarget[i+1],(sxu32)(nTarget - i - 1),(void *)&iTmp,0);` |
|      2 | 5525 | `			iPort = (int)iTmp;` |
|      2 | 5526 | `			nHost = i;` |
|      1 | 5527 | `		}` |
|      6 | 5528 | `		if( nHost >= (int)sizeof(zHost) ){` |
|    ! 0 | 5529 | `			nHost = (int)sizeof(zHost) - 1;` |
|    ! 0 | 5530 | `		}` |
|      6 | 5531 | `		SyMemcpy(zTarget,zHost,(sxu32)nHost);` |
|      6 | 5532 | `		zHost[nHost] = 0;` |
|      - | 5533 | `	}` |
|      6 | 5534 | `	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      4 | 5535 | `		iPort = ph7_value_to_int(apArg[1]);` |
|      2 | 5536 | `	}` |
|      6 | 5537 | `	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){` |
|      6 | 5538 | `		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);` |
|      6 | 5539 | `		if( rTimeout > 0 ){` |
|      6 | 5540 | `			iTimeoutMs = (int)(rTimeout * 1000);` |
|      3 | 5541 | `		}` |
|      3 | 5542 | `	}` |
|      6 | 5543 | `	if( iPort < 0 ){` |
|    ! 0 | 5544 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5545 | `		return PH7_OK;` |
|      - | 5546 | `	}` |
|      6 | 5547 | `	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,&iErrno,&zErr);` |
|      6 | 5548 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 5549 | `		/* php reports the failure through the by-ref out-params + a warning */` |
|      - | 5550 | `		{` |
|      2 | 5551 | `			ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      2 | 5552 | `			if( pTmp ){` |
|      2 | 5553 | `				if( nArg > iArgErrno ){` |
|      2 | 5554 | `					ph7_value_int(pTmp,iErrno);` |
|      2 | 5555 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      1 | 5556 | `				}` |
|      2 | 5557 | `				if( nArg > iArgErrstr ){` |
|      2 | 5558 | `					ph7_value_string(pTmp,zErr,-1);` |
|      2 | 5559 | `					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      1 | 5560 | `				}` |
|      1 | 5561 | `			}` |
|      - | 5562 | `		}` |
|      - | 5563 | `		/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */` |
|      3 | 5564 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      1 | 5565 | `			"Unable to connect to %s:%d (%s)",zHost,iPort,zErr);` |
|      2 | 5566 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5567 | `		return PH7_OK;` |
|      - | 5568 | `	}` |
|      - | 5569 | `	{` |
|      4 | 5570 | `		ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      4 | 5571 | `		if( pTmp ){` |
|      4 | 5572 | `			if( nArg > iArgErrno ){` |
|      4 | 5573 | `				ph7_value_int(pTmp,0);` |
|      4 | 5574 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|      2 | 5575 | `			}` |
|      4 | 5576 | `			if( nArg > iArgErrstr ){` |
|      4 | 5577 | `				ph7_value_string(pTmp,"",0);` |
|      4 | 5578 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|      2 | 5579 | `			}` |
|      2 | 5580 | `		}` |
|      - | 5581 | `	}` |
|      - | 5582 | `	/* Wrap the socket in an io_private so the whole f* family works on it */` |
|      4 | 5583 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      4 | 5584 | `	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));` |
|      4 | 5585 | `	if( pDev == 0 \|\| pSock == 0 ){` |
|    ! 0 | 5586 | `		PH7_NetClose(sock);` |
|    ! 0 | 5587 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5588 | `		return PH7_OK;` |
|      - | 5589 | `	}` |
|      4 | 5590 | `	pSock->pVm = pCtx->pVm;` |
|      4 | 5591 | `	pSock->sock = sock;` |
|      4 | 5592 | `	pSock->bEof = 0;` |
|      4 | 5593 | `	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);` |
|      4 | 5594 | `	pDev->pHandle = (void *)pSock;` |
|      4 | 5595 | `	ph7_result_resource(pCtx,pDev);` |
|      4 | 5596 | `	return PH7_OK;` |
|      3 | 5597 | `}` |
|      - | 5598 | `#endif /* PH7_ENABLE_NET */` |
|     80 | 5599 | `static int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5600 | `{` |
|      - | 5601 | `	const ph7_io_stream *pStream;` |
|      - | 5602 | `	const char *zUri,*zMode;` |
|      - | 5603 | `	ph7_value *pResource;` |
|      - | 5604 | `	io_private *pDev;` |
|      - | 5605 | `	int iLen,imLen;` |
|      - | 5606 | `	int iOpenFlags;` |
|     83 | 5607 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5608 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5609 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 5610 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5611 | `		return PH7_OK;` |
|      - | 5612 | `	}` |
|      - | 5613 | `	/* Extract the URI and the desired access mode */` |
|     83 | 5614 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|     83 | 5615 | `	if( nArg > 1 ){` |
|     83 | 5616 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|     43 | 5617 | `	}else{` |
|      - | 5618 | `		/* Set a default read-only mode */` |
|    ! 0 | 5619 | `		zMode = "r";` |
|    ! 0 | 5620 | `		imLen = (int)sizeof(char);` |
|      - | 5621 | `	}` |
|      - | 5622 | `	/* Try to extract a stream */` |
|     83 | 5623 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|     83 | 5624 | `	if( pStream == 0 ){` |
|    ! 0 | 5625 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5626 | `			"No stream device is associated with the given URI(%s)",zUri);` |
|    ! 0 | 5627 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5628 | `		return PH7_OK;` |
|      - | 5629 | `	}` |
|      - | 5630 | `	/* Allocate a new IO private instance */` |
|     83 | 5631 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     83 | 5632 | `	if( pDev == 0 ){` |
|    ! 0 | 5633 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5634 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5635 | `		return PH7_OK;` |
|      - | 5636 | `	}` |
|     83 | 5637 | `	pResource = 0;` |
|     83 | 5638 | `	if( nArg > 3 ){` |
|    ! 0 | 5639 | `		pResource = apArg[3];` |
|     83 | 5640 | `	}else if( is_php_stream(pStream) \|\| is_data_stream(pStream) ){` |
|      - | 5641 | `		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to` |
|      - | 5642 | `		 * access the underlying virtual machine.` |
|      - | 5643 | `		 */` |
|     15 | 5644 | `		pResource = apArg[0];` |
|      7 | 5645 | `	}` |
|      - | 5646 | `	/* Initialize the structure */` |
|     83 | 5647 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 5648 | `	/* Convert open mode to PH7 flags */` |
|     83 | 5649 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|      - | 5650 | `	/* Try to get a handle */` |
|    123 | 5651 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|     40 | 5652 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);` |
|     83 | 5653 | `	if( pDev->pHandle == 0 ){` |
|    ! 0 | 5654 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zUri);` |
|    ! 0 | 5655 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5656 | `		ph7_context_free_chunk(pCtx,pDev);` |
|    ! 0 | 5657 | `		return PH7_OK;` |
|      - | 5658 | `	}` |
|      - | 5659 | `	/* All done,return the io_private instance as a resource */` |
|     83 | 5660 | `	ph7_result_resource(pCtx,pDev);` |
|     83 | 5661 | `	return PH7_OK;` |
|     43 | 5662 | `}` |
|      - | 5663 | `/*` |
|      - | 5664 | ` * bool fclose(resource $handle)` |
|      - | 5665 | ` *  Closes an open file pointer` |
|      - | 5666 | ` * Parameters` |
|      - | 5667 | ` *  $handle` |
|      - | 5668 | ` *   The file pointer.` |
|      - | 5669 | ` * Return` |
|      - | 5670 | ` *  TRUE on success or FALSE on failure.` |
|      - | 5671 | ` */` |
|    178 | 5672 | `static int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5673 | `{` |
|      - | 5674 | `	const ph7_io_stream *pStream;` |
|      - | 5675 | `	io_private *pDev;` |
|      - | 5676 | `	ph7_vm *pVm;` |
|    183 | 5677 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5678 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5679 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5680 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5681 | `		return PH7_OK;` |
|      - | 5682 | `	}` |
|      - | 5683 | `	/* Extract our private data */` |
|    183 | 5684 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 5685 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    183 | 5686 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 5687 | `		/*Expecting an IO handle */` |
|    ! 0 | 5688 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 5689 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5690 | `		return PH7_OK;` |
|      - | 5691 | `	}` |
|      - | 5692 | `	/* Point to the target IO stream device */` |
|    183 | 5693 | `	pStream = pDev->pStream;` |
|    183 | 5694 | `	if( pStream == 0 ){` |
|    ! 0 | 5695 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5696 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 5697 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 5698 | `			);` |
|    ! 0 | 5699 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5700 | `		return PH7_OK;` |
|      - | 5701 | `	}` |
|      - | 5702 | `	/* Point to the VM that own this context */` |
|    183 | 5703 | `	pVm = pCtx->pVm;` |
|      - | 5704 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|    183 | 5705 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 5706 | `		/* Perform the requested operation */` |
|    183 | 5707 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 5708 | `		/* Release the IO private structure */` |
|    183 | 5709 | `		ReleaseIOPrivate(pCtx,pDev);` |
|      - | 5710 | `		/* Invalidate the resource handle */` |
|    183 | 5711 | `		ph7_value_release(apArg[0]);` |
|     89 | 5712 | `	}` |
|      - | 5713 | `	/* Return TRUE */` |
|    183 | 5714 | `	ph7_result_bool(pCtx,1);` |
|    183 | 5715 | `	return PH7_OK;` |
|     94 | 5716 | `}` |
|      - | 5717 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5718 | `/*` |
|      - | 5719 | ` * MD5/SHA1 digest consumer.` |
|      - | 5720 | ` */` |
|     72 | 5721 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 5722 | `{` |
|      - | 5723 | `	/* Append hex chunk verbatim */` |
|     73 | 5724 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     73 | 5725 | `	return SXRET_OK;` |
|      1 | 5726 | `}` |
|      - | 5727 | `/*` |
|      - | 5728 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 5729 | ` *  Calculates the md5 hash of a given file.` |
|      - | 5730 | ` * Parameters` |
|      - | 5731 | ` *  $uri` |
|      - | 5732 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 5733 | ` *  $raw_output` |
|      - | 5734 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 5735 | ` * Return` |
|      - | 5736 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 5737 | ` */` |
|      2 | 5738 | `static int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5739 | `{` |
|      - | 5740 | `	const ph7_io_stream *pStream;` |
|      - | 5741 | `	unsigned char zDigest[16];` |
|      3 | 5742 | `	int raw_output  = FALSE;` |
|      - | 5743 | `	const char *zFile;` |
|      - | 5744 | `	MD5Context sCtx;` |
|      - | 5745 | `	char zBuf[8192];` |
|      - | 5746 | `	void *pHandle;` |
|      - | 5747 | `	ph7_int64 n;` |
|      - | 5748 | `	int nLen;` |
|      3 | 5749 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5750 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5751 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5752 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5753 | `		return PH7_OK;` |
|      - | 5754 | `	}` |
|      - | 5755 | `	/* Extract the file path */` |
|      3 | 5756 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5757 | `	/* Point to the target IO stream device */` |
|      3 | 5758 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5759 | `	if( pStream == 0 ){` |
|    ! 0 | 5760 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5761 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5762 | `		return PH7_OK;` |
|      - | 5763 | `	}` |
|      3 | 5764 | `	if( nArg > 1 ){` |
|    ! 0 | 5765 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 5766 | `	}` |
|      - | 5767 | `	/* Try to open the file in read-only mode */` |
|      3 | 5768 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5769 | `	if( pHandle == 0 ){` |
|    ! 0 | 5770 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5771 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5772 | `		return PH7_OK;` |
|      - | 5773 | `	}` |
|      - | 5774 | `	/* Init the MD5 context */` |
|      3 | 5775 | `	MD5Init(&sCtx);` |
|      - | 5776 | `	/* Perform the requested operation */` |
|      2 | 5777 | `	for(;;){` |
|      5 | 5778 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5779 | `		if( n < 1 ){` |
|      - | 5780 | `			/* EOF or IO error,break immediately */` |
|      3 | 5781 | `			break;` |
|      - | 5782 | `		}` |
|      3 | 5783 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5784 | `	}` |
|      - | 5785 | `	/* Close the stream */` |
|      3 | 5786 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5787 | `	/* Extract the digest */` |
|      3 | 5788 | `	MD5Final(zDigest,&sCtx);` |
|      3 | 5789 | `	if( raw_output ){` |
|      - | 5790 | `		/* Output raw digest */` |
|    ! 0 | 5791 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5792 | `	}else{` |
|      - | 5793 | `		/* Perform a binary to hex conversion */` |
|      3 | 5794 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5795 | `	}` |
|      3 | 5796 | `	return PH7_OK;` |
|      2 | 5797 | `}` |
|      - | 5798 | `/*` |
|      - | 5799 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 5800 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 5801 | ` * Parameters` |
|      - | 5802 | ` *  $uri` |
|      - | 5803 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 5804 | ` *  $raw_output` |
|      - | 5805 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 5806 | ` * Return` |
|      - | 5807 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 5808 | ` */` |
|      2 | 5809 | `static int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5810 | `{` |
|      - | 5811 | `	const ph7_io_stream *pStream;` |
|      - | 5812 | `	unsigned char zDigest[20];` |
|      3 | 5813 | `	int raw_output  = FALSE;` |
|      - | 5814 | `	const char *zFile;` |
|      - | 5815 | `	SHA1Context sCtx;` |
|      - | 5816 | `	char zBuf[8192];` |
|      - | 5817 | `	void *pHandle;` |
|      - | 5818 | `	ph7_int64 n;` |
|      - | 5819 | `	int nLen;` |
|      3 | 5820 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5821 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5822 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5823 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5824 | `		return PH7_OK;` |
|      - | 5825 | `	}` |
|      - | 5826 | `	/* Extract the file path */` |
|      3 | 5827 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5828 | `	/* Point to the target IO stream device */` |
|      3 | 5829 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5830 | `	if( pStream == 0 ){` |
|    ! 0 | 5831 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5832 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5833 | `		return PH7_OK;` |
|      - | 5834 | `	}` |
|      3 | 5835 | `	if( nArg > 1 ){` |
|    ! 0 | 5836 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 5837 | `	}` |
|      - | 5838 | `	/* Try to open the file in read-only mode */` |
|      3 | 5839 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5840 | `	if( pHandle == 0 ){` |
|    ! 0 | 5841 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5842 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5843 | `		return PH7_OK;` |
|      - | 5844 | `	}` |
|      - | 5845 | `	/* Init the SHA1 context */` |
|      3 | 5846 | `	SHA1Init(&sCtx);` |
|      - | 5847 | `	/* Perform the requested operation */` |
|      2 | 5848 | `	for(;;){` |
|      5 | 5849 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5850 | `		if( n < 1 ){` |
|      - | 5851 | `			/* EOF or IO error,break immediately */` |
|      3 | 5852 | `			break;` |
|      - | 5853 | `		}` |
|      3 | 5854 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5855 | `	}` |
|      - | 5856 | `	/* Close the stream */` |
|      3 | 5857 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5858 | `	/* Extract the digest */` |
|      3 | 5859 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 5860 | `	if( raw_output ){` |
|      - | 5861 | `		/* Output raw digest */` |
|    ! 0 | 5862 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5863 | `	}else{` |
|      - | 5864 | `		/* Perform a binary to hex conversion */` |
|      3 | 5865 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5866 | `	}` |
|      3 | 5867 | `	return PH7_OK;` |
|      2 | 5868 | `}` |
|      - | 5869 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5870 | `/*` |
|      - | 5871 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 5872 | ` *  Parse a configuration file.` |
|      - | 5873 | ` * Parameters` |
|      - | 5874 | ` * $filename` |
|      - | 5875 | ` *  The filename of the ini file being parsed.` |
|      - | 5876 | ` * $process_sections` |
|      - | 5877 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 5878 | ` *  with the section names and settings included.` |
|      - | 5879 | ` *  The default for process_sections is FALSE.` |
|      - | 5880 | ` * $scanner_mode` |
|      - | 5881 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 5882 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 5883 | ` * Return` |
|      - | 5884 | ` *  The settings are returned as an associative array on success.` |
|      - | 5885 | ` *  Otherwise is returned.` |
|      - | 5886 | ` */` |
|      2 | 5887 | `static int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5888 | `{` |
|      - | 5889 | `	const ph7_io_stream *pStream;` |
|      - | 5890 | `	const char *zFile;` |
|      - | 5891 | `	SyBlob sContents;` |
|      - | 5892 | `	void *pHandle;` |
|      - | 5893 | `	int nLen;` |
|      3 | 5894 | `	sxi32 rc = PH7_OK;` |
|      3 | 5895 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5896 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5897 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5898 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5899 | `		return PH7_OK;` |
|      - | 5900 | `	}` |
|      - | 5901 | `	/* Extract the file path */` |
|      3 | 5902 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5903 | `	/* Point to the target IO stream device */` |
|      3 | 5904 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5905 | `	if( pStream == 0 ){` |
|    ! 0 | 5906 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5907 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5908 | `		return PH7_OK;` |
|      - | 5909 | `	}` |
|      - | 5910 | `	/* Try to open the file in read-only mode */` |
|      3 | 5911 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5912 | `	if( pHandle == 0 ){` |
|    ! 0 | 5913 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5914 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5915 | `		return PH7_OK;` |
|      - | 5916 | `	}` |
|      3 | 5917 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 5918 | `	/* Read the whole file */` |
|      3 | 5919 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      3 | 5920 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 5921 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 5922 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5923 | `	}else{` |
|      - | 5924 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|      5 | 5925 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      2 | 5926 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0);` |
|      - | 5927 | `	}` |
|      - | 5928 | `	/* Close the stream */` |
|      3 | 5929 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5930 | `	/* Release the working buffer */` |
|      3 | 5931 | `	SyBlobRelease(&sContents);` |
|      - | 5932 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      3 | 5933 | `	return rc;` |
|      2 | 5934 | `}` |
|      - | 5935 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 5936 | `#else /* PH7_DISABLE_DISK_IO */` |
|      - | 5937 | `/*` |
|      - | 5938 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|      - | 5939 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|      - | 5940 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|      - | 5941 | ` * to any non-VFS resource).` |
|      - | 5942 | ` */` |
|      - | 5943 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      - | 5944 | `{` |
|      - | 5945 | `	SXUNUSED(pResource);` |
|      - | 5946 | `	return "Unknown";` |
|      - | 5947 | `}` |
|      - | 5948 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 5949 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 5950 | `#if defined(_MSC_VER)` |
|      - | 5951 | `static const ph7_vfs null_vfs = {` |
|      - | 5952 | `#else` |
|      - | 5953 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 5954 | `#endif` |
|      - | 5955 | `	"null_vfs",` |
|      - | 5956 | `	PH7_VFS_VERSION,` |
|      - | 5957 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 5958 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 5959 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 5960 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 5961 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 5962 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 5963 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 5964 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 5965 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 5966 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 5967 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 5968 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 5969 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 5970 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 5971 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 5972 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 5973 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 5974 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 5975 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 5976 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 5977 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 5978 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 5979 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 5980 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 5981 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 5982 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 5983 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 5984 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 5985 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 5986 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 5987 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 5988 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 5989 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 5990 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 5991 | `	0, /* int (*xUmask)(int) */` |
|      - | 5992 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 5993 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 5994 | `	0, /* int (*xUid)(void) */` |
|      - | 5995 | `	0, /* int (*xGid)(void) */` |
|      - | 5996 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 5997 | `	0  /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 5998 | `};` |
|      - | 5999 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 6000 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 6001 | `/*` |
|      - | 6002 | ` * Export the builtin vfs.` |
|      - | 6003 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 6004 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 6005 | ` * Note:` |
|      - | 6006 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 6007 | ` * Note:` |
|      - | 6008 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 6009 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 6010 | ` */` |
|   3828 | 6011 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 6012 | `{` |
|      - | 6013 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 6014 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 6015 | `	return &null_vfs;` |
|      - | 6016 | `#else` |
|      - | 6017 | `#ifdef __WINNT__` |
|      5 | 6018 | `	return &sWinVfs;` |
|      - | 6019 | `#elif defined(__UNIXES__)` |
|   3828 | 6020 | `	return &sUnixVfs;` |
|      - | 6021 | `#else` |
|      - | 6022 | `	return &null_vfs;` |
|      - | 6023 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 6024 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 6025 | `#else` |
|      - | 6026 | `	return &null_vfs;` |
|      - | 6027 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 6028 | `}` |
|      - | 6029 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 6030 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 6031 | `/*` |
|      - | 6032 | ` * The following defines are mostly used by the UNIX built and have` |
|      - | 6033 | ` * no particular meaning on windows.` |
|      - | 6034 | ` */` |
|      - | 6035 | `#ifndef STDIN_FILENO` |
|      - | 6036 | `#define STDIN_FILENO	0` |
|      - | 6037 | `#endif` |
|      - | 6038 | `#ifndef STDOUT_FILENO` |
|      - | 6039 | `#define STDOUT_FILENO	1` |
|      - | 6040 | `#endif` |
|      - | 6041 | `#ifndef STDERR_FILENO` |
|      - | 6042 | `#define STDERR_FILENO	2` |
|      - | 6043 | `#endif` |
|      - | 6044 | `/*` |
|      - | 6045 | ` * php:// Accessing various I/O streams` |
|      - | 6046 | ` * According to the PHP langage reference manual` |
|      - | 6047 | ` * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input` |
|      - | 6048 | ` * and output streams, the standard input, output and error file descriptors.` |
|      - | 6049 | ` * php://stdin, php://stdout and php://stderr:` |
|      - | 6050 | ` *  Allow direct access to the corresponding input or output stream of the PHP process.` |
|      - | 6051 | ` *  The stream references a duplicate file descriptor, so if you open php://stdin and later` |
|      - | 6052 | ` *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.` |
|      - | 6053 | ` *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.` |
|      - | 6054 | ` * php://output` |
|      - | 6055 | ` *  php://output is a write-only stream that allows you to write to the output buffer` |
|      - | 6056 | ` *  mechanism in the same way as print and echo.` |
|      - | 6057 | ` */` |
|      - | 6058 | `typedef struct ph7_stream_data ph7_stream_data;` |
|      - | 6059 | `/* Supported IO streams */` |
|      - | 6060 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|      - | 6061 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|      - | 6062 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|      - | 6063 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|      - | 6064 | `#define PH7_IO_STREAM_MEMORY 5 /* php://memory, php://temp, and data:// payloads */` |
|      - | 6065 | ` /* The following structure is the private data associated with the php:// stream */` |
|      - | 6066 | `struct ph7_stream_data` |
|      - | 6067 | `{` |
|      - | 6068 | `	ph7_vm *pVm; /* VM that own this instance */` |
|      - | 6069 | `	int iType;   /* Stream type */` |
|      - | 6070 | `	union{` |
|      - | 6071 | `		void *pHandle; /* Stream handle */` |
|      - | 6072 | `		ph7_output_consumer sConsumer; /* VM output consumer */` |
|      - | 6073 | `	}x;` |
|      - | 6074 | `	SyBlob sMem;     /* MEMORY type: backing buffer */` |
|      - | 6075 | `	sxu32 nCur;      /* MEMORY type: read/write cursor */` |
|      - | 6076 | `	int bReadOnly;   /* MEMORY type: TRUE for data:// payloads */` |
|      - | 6077 | `};` |
|      - | 6078 | `/*` |
|      - | 6079 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|      - | 6080 | ` */` |
|     26 | 6081 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|      1 | 6082 | `{` |
|      - | 6083 | `	ph7_stream_data *pData;` |
|     27 | 6084 | `	if( pVm == 0 ){` |
|    ! 0 | 6085 | `		return 0;` |
|      - | 6086 | `	}` |
|      - | 6087 | `	/* Allocate a new instance */` |
|     27 | 6088 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|     27 | 6089 | `	if( pData == 0 ){` |
|    ! 0 | 6090 | `		return 0;` |
|      - | 6091 | `	}` |
|      - | 6092 | `	/* Zero the structure */` |
|     27 | 6093 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|      - | 6094 | `	/* Initialize fields */` |
|     27 | 6095 | `	pData->iType = iType;` |
|     27 | 6096 | `	SyBlobInit(&pData->sMem,&pVm->sAllocator);` |
|     27 | 6097 | `	pData->nCur = 0;` |
|     27 | 6098 | `	pData->bReadOnly = 0;` |
|     27 | 6099 | `	if( iType == PH7_IO_STREAM_MEMORY ){` |
|      - | 6100 | `		/* Nothing else to set up: the buffer is the stream */` |
|     18 | 6101 | `	}else if( iType == PH7_IO_STREAM_OUTPUT ){` |
|      - | 6102 | `		/* Point to the default VM consumer routine. */` |
|      3 | 6103 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|      2 | 6104 | `	}else{` |
|      - | 6105 | `#ifdef __WINNT__` |
|      - | 6106 | `		DWORD nChannel;` |
|      1 | 6107 | `		switch(iType){` |
|      1 | 6108 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|      1 | 6109 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|      - | 6110 | `		default:` |
|      1 | 6111 | `			nChannel = STD_INPUT_HANDLE;` |
|      - | 6112 | `			break;` |
|      - | 6113 | `		}` |
|      1 | 6114 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|      - | 6115 | `#else` |
|      - | 6116 | `		/* Assume an UNIX system */` |
|      6 | 6117 | `		int ifd = STDIN_FILENO;` |
|      6 | 6118 | `		switch(iType){` |
|      2 | 6119 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|      2 | 6120 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|      1 | 6121 | `		default:` |
|      2 | 6122 | `			break;` |
|      - | 6123 | `		}` |
|      6 | 6124 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|      - | 6125 | `#endif` |
|      - | 6126 | `	}` |
|     27 | 6127 | `	pData->pVm = pVm;` |
|     27 | 6128 | `	return pData;` |
|     14 | 6129 | `}` |
|      - | 6130 | `/*` |
|      - | 6131 | ` * Implementation of the php:// IO streams routines` |
|      - | 6132 | ` * Status:` |
|      - | 6133 | ` *   Stable.` |
|      - | 6134 | ` */` |
|      - | 6135 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|     10 | 6136 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 6137 | `{` |
|      - | 6138 | `	ph7_stream_data *pData;` |
|      - | 6139 | `	SyString sStream;` |
|     11 | 6140 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|      - | 6141 | `	/* Trim leading and trailing white spaces */` |
|     11 | 6142 | `	SyStringFullTrim(&sStream);` |
|      - | 6143 | `	/* Stream to open */` |
|     11 | 6144 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|    ! 0 | 6145 | `		iMode = PH7_IO_STREAM_STDIN;` |
|     11 | 6146 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|      3 | 6147 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|     10 | 6148 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|    ! 0 | 6149 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|      9 | 6150 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|    ! 0 | 6151 | `		iMode = PH7_IO_STREAM_STDERR;` |
|      8 | 6152 | `	}else if( SyStrnicmp(sStream.zString,"memory",sizeof("memory")-1) == 0` |
|      6 | 6153 | `	       \|\| SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0 ){` |
|      - | 6154 | `		/* php://memory and php://temp (PHL keeps temp fully in memory —` |
|      - | 6155 | `		 * php's 2MB disk spill is a memory-pressure detail, recorded) */` |
|      9 | 6156 | `		iMode = PH7_IO_STREAM_MEMORY;` |
|      5 | 6157 | `	}else{` |
|      - | 6158 | `		/* unknown stream name */` |
|    ! 0 | 6159 | `		return -1;` |
|      - | 6160 | `	}` |
|      - | 6161 | `	/* Create our handle */` |
|     11 | 6162 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|     11 | 6163 | `	if( pData == 0 ){` |
|    ! 0 | 6164 | `		return -1;` |
|      - | 6165 | `	}` |
|      - | 6166 | `	/* Make the handle public */` |
|     11 | 6167 | `	*ppHandle = (void *)pData;` |
|     11 | 6168 | `	return PH7_OK;` |
|      6 | 6169 | `}` |
|      - | 6170 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|     42 | 6171 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|      1 | 6172 | `{` |
|     43 | 6173 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     43 | 6174 | `	if( pData == 0 ){` |
|    ! 0 | 6175 | `		return -1;` |
|      - | 6176 | `	}` |
|     43 | 6177 | `	if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|     43 | 6178 | `		sxu32 nAvail = SyBlobLength(&pData->sMem);` |
|      - | 6179 | `		sxu32 nRead;` |
|     43 | 6180 | `		if( pData->nCur >= nAvail ){` |
|     15 | 6181 | `			return 0; /* EOF */` |
|      - | 6182 | `		}` |
|     29 | 6183 | `		nRead = nAvail - pData->nCur;` |
|     29 | 6184 | `		if( (ph7_int64)nRead > nDatatoRead ){` |
|      7 | 6185 | `			nRead = (sxu32)nDatatoRead;` |
|      3 | 6186 | `		}` |
|     29 | 6187 | `		SyMemcpy((const char *)SyBlobData(&pData->sMem) + pData->nCur,pBuffer,nRead);` |
|     29 | 6188 | `		pData->nCur += nRead;` |
|     29 | 6189 | `		return (ph7_int64)nRead;` |
|      - | 6190 | `	}` |
|    ! 0 | 6191 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|      - | 6192 | `		/* Forbidden */` |
|    ! 0 | 6193 | `		return -1;` |
|      - | 6194 | `	}` |
|      - | 6195 | `#ifdef __WINNT__` |
|      - | 6196 | `	{` |
|      - | 6197 | `		DWORD nRd;` |
|      - | 6198 | `		BOOL rc;` |
|    ! 0 | 6199 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    ! 0 | 6200 | `		if( !rc ){` |
|      - | 6201 | `			/* IO error */` |
|    ! 0 | 6202 | `			return -1;` |
|      - | 6203 | `		}` |
|    ! 0 | 6204 | `		return (ph7_int64)nRd;` |
|      - | 6205 | `	}` |
|      - | 6206 | `#elif defined(__UNIXES__)` |
|      - | 6207 | `	{` |
|      - | 6208 | `		ssize_t nRd;` |
|      - | 6209 | `		int fd;` |
|    ! 0 | 6210 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 6211 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|    ! 0 | 6212 | `		if( nRd < 1 ){` |
|    ! 0 | 6213 | `			return -1;` |
|      - | 6214 | `		}` |
|    ! 0 | 6215 | `		return (ph7_int64)nRd;` |
|      - | 6216 | `	}` |
|      - | 6217 | `#else` |
|      - | 6218 | `	return -1;` |
|      - | 6219 | `#endif` |
|     22 | 6220 | `}` |
|      - | 6221 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|     12 | 6222 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 6223 | `{` |
|     13 | 6224 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     13 | 6225 | `	if( pData == 0 ){` |
|    ! 0 | 6226 | `		return -1;` |
|      - | 6227 | `	}` |
|     13 | 6228 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|      - | 6229 | `		/* Forbidden */` |
|    ! 0 | 6230 | `		return -1;` |
|     13 | 6231 | `	}else if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|      - | 6232 | `		sxu32 nLen,nEnd;` |
|     11 | 6233 | `		if( pData->bReadOnly ){` |
|    ! 0 | 6234 | `			return -1;` |
|      - | 6235 | `		}` |
|     11 | 6236 | `		nLen = SyBlobLength(&pData->sMem);` |
|     11 | 6237 | `		if( pData->nCur > nLen ){` |
|      - | 6238 | `			/* seek past end: php zero-fills the gap */` |
|      - | 6239 | `			static const char zZero[64] = {0};` |
|    ! 0 | 6240 | `			while( SyBlobLength(&pData->sMem) < pData->nCur ){` |
|    ! 0 | 6241 | `				sxu32 nPad = pData->nCur - SyBlobLength(&pData->sMem);` |
|    ! 0 | 6242 | `				if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|    ! 0 | 6243 | `				if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|    ! 0 | 6244 | `					return -1;` |
|      - | 6245 | `				}` |
|    ! 0 | 6246 | `			}` |
|    ! 0 | 6247 | `			nLen = SyBlobLength(&pData->sMem);` |
|    ! 0 | 6248 | `		}` |
|     11 | 6249 | `		nEnd = pData->nCur + (sxu32)nWrite;` |
|     11 | 6250 | `		if( pData->nCur < nLen ){` |
|      - | 6251 | `			/* overwrite in place up to the current end */` |
|      3 | 6252 | `			sxu32 nOver = nLen - pData->nCur;` |
|      3 | 6253 | `			if( nOver > (sxu32)nWrite ){ nOver = (sxu32)nWrite; }` |
|      3 | 6254 | `			SyMemcpy(pBuf,(char *)SyBlobData(&pData->sMem) + pData->nCur,nOver);` |
|      3 | 6255 | `			if( nEnd > nLen ){` |
|    ! 0 | 6256 | `				if( SyBlobAppend(&pData->sMem,(const char *)pBuf + nOver,nEnd - nLen) != SXRET_OK ){` |
|    ! 0 | 6257 | `					return -1;` |
|      - | 6258 | `				}` |
|    ! 0 | 6259 | `			}` |
|      2 | 6260 | `		}else{` |
|      9 | 6261 | `			if( SyBlobAppend(&pData->sMem,pBuf,(sxu32)nWrite) != SXRET_OK ){` |
|    ! 0 | 6262 | `				return -1;` |
|      - | 6263 | `			}` |
|      - | 6264 | `		}` |
|     11 | 6265 | `		pData->nCur = nEnd;` |
|     11 | 6266 | `		return nWrite;` |
|      3 | 6267 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|      3 | 6268 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|      - | 6269 | `		int rc;` |
|      - | 6270 | `		/* Call the vm output consumer */` |
|      3 | 6271 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|      3 | 6272 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 6273 | `			return -1;` |
|      - | 6274 | `		}` |
|      3 | 6275 | `		return nWrite;` |
|      - | 6276 | `	}` |
|      - | 6277 | `#ifdef __WINNT__` |
|      - | 6278 | `	{` |
|      - | 6279 | `		DWORD nWr;` |
|      - | 6280 | `		BOOL rc;` |
|    ! 0 | 6281 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|    ! 0 | 6282 | `		if( !rc ){` |
|      - | 6283 | `			/* IO error */` |
|    ! 0 | 6284 | `			return -1;` |
|      - | 6285 | `		}` |
|    ! 0 | 6286 | `		return (ph7_int64)nWr;` |
|      - | 6287 | `	}` |
|      - | 6288 | `#elif defined(__UNIXES__)` |
|      - | 6289 | `	{` |
|      - | 6290 | `		ssize_t nWr;` |
|      - | 6291 | `		int fd;` |
|    ! 0 | 6292 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 6293 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|    ! 0 | 6294 | `		if( nWr < 1 ){` |
|    ! 0 | 6295 | `			return -1;` |
|      - | 6296 | `		}` |
|    ! 0 | 6297 | `		return (ph7_int64)nWr;` |
|      - | 6298 | `	}` |
|      - | 6299 | `#else` |
|      - | 6300 | `	return -1;` |
|      - | 6301 | `#endif` |
|      7 | 6302 | `}` |
|      - | 6303 | `/* void (*xClose)(void *) */` |
|     16 | 6304 | `static void PHPStreamData_Close(void *pHandle)` |
|      1 | 6305 | `{` |
|     17 | 6306 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 6307 | `	ph7_vm *pVm;` |
|     17 | 6308 | `	if( pData == 0 ){` |
|    ! 0 | 6309 | `		return;` |
|      - | 6310 | `	}` |
|     17 | 6311 | `	pVm = pData->pVm;` |
|     17 | 6312 | `	SyBlobRelease(&pData->sMem);` |
|      - | 6313 | `	/* Free the instance */` |
|     17 | 6314 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|      9 | 6315 | `}` |
|      - | 6316 | `/* int (*xSeek)(void *,ph7_int64,int); MEMORY type only */` |
|     20 | 6317 | `static int PHPStreamData_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 6318 | `{` |
|     21 | 6319 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 6320 | `	ph7_int64 iNew;` |
|     21 | 6321 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|    ! 0 | 6322 | `		return -1;` |
|      - | 6323 | `	}` |
|     21 | 6324 | `	switch(whence){` |
|    ! 0 | 6325 | `	case 1/*SEEK_CUR*/: iNew = (ph7_int64)pData->nCur + iOfft; break;` |
|      3 | 6326 | `	case 2/*SEEK_END*/: iNew = (ph7_int64)SyBlobLength(&pData->sMem) + iOfft; break;` |
|     19 | 6327 | `	default:            iNew = iOfft; break;` |
|      - | 6328 | `	}` |
|     21 | 6329 | `	if( iNew < 0 ){` |
|    ! 0 | 6330 | `		return -1;` |
|      - | 6331 | `	}` |
|     21 | 6332 | `	pData->nCur = (sxu32)iNew;` |
|     21 | 6333 | `	return PH7_OK;` |
|     11 | 6334 | `}` |
|      - | 6335 | `/* ph7_int64 (*xTell)(void *); MEMORY type only */` |
|      4 | 6336 | `static ph7_int64 PHPStreamData_Tell(void *pHandle)` |
|      1 | 6337 | `{` |
|      5 | 6338 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      5 | 6339 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|    ! 0 | 6340 | `		return -1;` |
|      - | 6341 | `	}` |
|      5 | 6342 | `	return (ph7_int64)pData->nCur;` |
|      3 | 6343 | `}` |
|      - | 6344 | `/* int (*xTrunc)(void *,ph7_int64); MEMORY type only */` |
|    ! 0 | 6345 | `static int PHPStreamData_Trunc(void *pHandle,ph7_int64 nLen)` |
|    ! 0 | 6346 | `{` |
|    ! 0 | 6347 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    ! 0 | 6348 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY \|\| pData->bReadOnly ){` |
|    ! 0 | 6349 | `		return -1;` |
|      - | 6350 | `	}` |
|    ! 0 | 6351 | `	if( nLen < (ph7_int64)SyBlobLength(&pData->sMem) ){` |
|      - | 6352 | `		/* shrink in place: the blob keeps its allocation */` |
|    ! 0 | 6353 | `		pData->sMem.nByte = (sxu32)nLen;` |
|    ! 0 | 6354 | `	}else{` |
|      - | 6355 | `		static const char zZero[64] = {0};` |
|    ! 0 | 6356 | `		while( (ph7_int64)SyBlobLength(&pData->sMem) < nLen ){` |
|    ! 0 | 6357 | `			sxu32 nPad = (sxu32)(nLen - SyBlobLength(&pData->sMem));` |
|    ! 0 | 6358 | `			if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|    ! 0 | 6359 | `			if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|    ! 0 | 6360 | `				return -1;` |
|      - | 6361 | `			}` |
|    ! 0 | 6362 | `		}` |
|      - | 6363 | `	}` |
|    ! 0 | 6364 | `	return PH7_OK;` |
|    ! 0 | 6365 | `}` |
|      - | 6366 | `/*` |
|      - | 6367 | ` * data:// stream: read-only in-memory payloads parsed from RFC 2397 URIs` |
|      - | 6368 | ` * (data://[mediatype][;base64],payload — the payload percent-decodes unless` |
|      - | 6369 | ` * base64). Shares the MEMORY machinery above.` |
|      - | 6370 | ` */` |
|      8 | 6371 | `static sxi32 DataStreamB64Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 6372 | `{` |
|      9 | 6373 | `	return SyBlobAppend((SyBlob *)pUserData,pData,nLen);` |
|      1 | 6374 | `}` |
|     10 | 6375 | `static int DataStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 6376 | `{` |
|      - | 6377 | `	ph7_stream_data *pData;` |
|     11 | 6378 | `	const char *zIn = zName;` |
|     11 | 6379 | `	const char *zEnd = &zName[SyStrlen(zName)];` |
|     11 | 6380 | `	const char *zComma = 0;` |
|     11 | 6381 | `	int bBase64 = 0;` |
|      5 | 6382 | `	SXUNUSED(iMode);` |
|      - | 6383 | `	/* Find the comma separating the mediatype from the payload */` |
|    105 | 6384 | `	while( zIn < zEnd ){` |
|    105 | 6385 | `		if( zIn[0] == ',' ){` |
|     11 | 6386 | `			zComma = zIn;` |
|     11 | 6387 | `			break;` |
|      - | 6388 | `		}` |
|     95 | 6389 | `		zIn++;` |
|      1 | 6390 | `	}` |
|     11 | 6391 | `	if( zComma == 0 ){` |
|    ! 0 | 6392 | `		return -1;` |
|      - | 6393 | `	}` |
|     10 | 6394 | `	if( zComma - zName >= (int)sizeof(";base64")-1` |
|     10 | 6395 | `	 && SyStrnicmp(&zComma[-((int)sizeof(";base64")-1)],";base64",sizeof(";base64")-1) == 0 ){` |
|      3 | 6396 | `		bBase64 = 1;` |
|      1 | 6397 | `	}` |
|     11 | 6398 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,PH7_IO_STREAM_MEMORY);` |
|     11 | 6399 | `	if( pData == 0 ){` |
|    ! 0 | 6400 | `		return -1;` |
|      - | 6401 | `	}` |
|     11 | 6402 | `	pData->bReadOnly = 1;` |
|     11 | 6403 | `	zIn = &zComma[1];` |
|     11 | 6404 | `	if( bBase64 ){` |
|      3 | 6405 | `		if( SyBase64Decode(zIn,(sxu32)(zEnd - zIn),DataStreamB64Consumer,&pData->sMem) != SXRET_OK ){` |
|    ! 0 | 6406 | `			SyBlobRelease(&pData->sMem);` |
|    ! 0 | 6407 | `			SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|    ! 0 | 6408 | `			return -1;` |
|      - | 6409 | `		}` |
|      2 | 6410 | `	}else{` |
|      - | 6411 | `		/* percent-decode the payload */` |
|     71 | 6412 | `		while( zIn < zEnd ){` |
|     63 | 6413 | `			char c = zIn[0];` |
|     63 | 6414 | `			if( c == '%' && zIn + 2 < zEnd && SyisHex(zIn[1]) && SyisHex(zIn[2]) ){` |
|      3 | 6415 | `				int hi = SyHexToint(zIn[1]);` |
|      3 | 6416 | `				int lo = SyHexToint(zIn[2]);` |
|      3 | 6417 | `				c = (char)((hi << 4) \| lo);` |
|      3 | 6418 | `				zIn += 3;` |
|      2 | 6419 | `			}else{` |
|     61 | 6420 | `				zIn++;` |
|      - | 6421 | `			}` |
|     63 | 6422 | `			if( SyBlobAppend(&pData->sMem,&c,1) != SXRET_OK ){` |
|    ! 0 | 6423 | `				SyBlobRelease(&pData->sMem);` |
|    ! 0 | 6424 | `				SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|    ! 0 | 6425 | `				return -1;` |
|      - | 6426 | `			}` |
|      1 | 6427 | `		}` |
|      - | 6428 | `	}` |
|     11 | 6429 | `	*ppHandle = (void *)pData;` |
|     11 | 6430 | `	return PH7_OK;` |
|      6 | 6431 | `}` |
|      - | 6432 | `/* data:// rejects writes outright */` |
|    ! 0 | 6433 | `static ph7_int64 DataStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    ! 0 | 6434 | `{` |
|    ! 0 | 6435 | `	SXUNUSED(pHandle);` |
|    ! 0 | 6436 | `	SXUNUSED(pBuf);` |
|    ! 0 | 6437 | `	SXUNUSED(nWrite);` |
|    ! 0 | 6438 | `	return -1;` |
|    ! 0 | 6439 | `}` |
|      - | 6440 | `static const ph7_io_stream sDATA_Stream = {` |
|      - | 6441 | `	"data",` |
|      - | 6442 | `	PH7_IO_STREAM_VERSION,` |
|      - | 6443 | `	DataStreamData_Open,  /* xOpen */` |
|      - | 6444 | `	0,   /* xOpenDir */` |
|      - | 6445 | `	PHPStreamData_Close, /* xClose */` |
|      - | 6446 | `	0,  /* xCloseDir */` |
|      - | 6447 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 6448 | `	0,  /* xReadDir */` |
|      - | 6449 | `	DataStreamData_Write, /* xWrite */` |
|      - | 6450 | `	PHPStreamData_Seek,  /* xSeek */` |
|      - | 6451 | `	0,  /* xLock */` |
|      - | 6452 | `	0,  /* xRewindDir */` |
|      - | 6453 | `	PHPStreamData_Tell,  /* xTell */` |
|      - | 6454 | `	0,  /* xTrunc */` |
|      - | 6455 | `	0,  /* xSync */` |
|      - | 6456 | `	0   /* xStat */` |
|      - | 6457 | `};` |
|      - | 6458 | `/*` |
|      - | 6459 | ` * Pipe stream implementation for popen/pclose.` |
|      - | 6460 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|      - | 6461 | ` * PHP-compatible process I/O functionality.` |
|      - | 6462 | ` */` |
|      - | 6463 | `typedef struct pipe_private pipe_private;` |
|      - | 6464 | `struct pipe_private` |
|      - | 6465 | `{` |
|      - | 6466 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|      - | 6467 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|      - | 6468 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|      - | 6469 | `#ifdef __WINNT__` |
|      - | 6470 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|      - | 6471 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|      - | 6472 | `#endif` |
|      - | 6473 | `};` |
|      - | 6474 |  |
|      - | 6475 | `#ifdef __WINNT__` |
|      - | 6476 | `#include <Windows.h>` |
|      - | 6477 | `#include <stdio.h>` |
|      - | 6478 | `#include <io.h>` |
|      - | 6479 | `#include <fcntl.h>` |
|      - | 6480 | `/*` |
|      - | 6481 | ` * Custom Windows popen implementation using CreateProcess.` |
|      - | 6482 | ` * This allows us to properly wait for process completion.` |
|      - | 6483 | ` */` |
|      - | 6484 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|      5 | 6485 | `{` |
|      5 | 6486 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|      5 | 6487 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|      5 | 6488 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|      - | 6489 | `	SECURITY_ATTRIBUTES sa;` |
|      - | 6490 | `	STARTUPINFOW si;` |
|      - | 6491 | `	PROCESS_INFORMATION pi;` |
|      5 | 6492 | `	WCHAR *zWideCmd = NULL;` |
|      5 | 6493 | `	FILE *pFile = NULL;` |
|      - | 6494 | `	int fd;` |
|      5 | 6495 | `	BOOL bRead = (zMode[0] == 'r');` |
|      - | 6496 |  |
|      - | 6497 | `	/* Set up security attributes for pipe inheritance */` |
|      5 | 6498 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|      5 | 6499 | `	sa.bInheritHandle = TRUE;` |
|      5 | 6500 | `	sa.lpSecurityDescriptor = NULL;` |
|      - | 6501 |  |
|      - | 6502 | `	/* Create pipes for child process I/O */` |
|      5 | 6503 | `	if( bRead ){` |
|      - | 6504 | `		/* Reading from child's stdout */` |
|      5 | 6505 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|    ! 0 | 6506 | `			return NULL;` |
|      - | 6507 | `		}` |
|      - | 6508 | `		/* Ensure read handle is not inherited */` |
|      5 | 6509 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|      5 | 6510 | `		hReadPipe = hChildStdoutRd;` |
|      5 | 6511 | `		*phPipe = hChildStdoutRd;` |
|      5 | 6512 | `	}else{` |
|      - | 6513 | `		/* Writing to child's stdin */` |
|    ! 0 | 6514 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|    ! 0 | 6515 | `			return NULL;` |
|      - | 6516 | `		}` |
|      - | 6517 | `		/* Ensure write handle is not inherited */` |
|    ! 0 | 6518 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|    ! 0 | 6519 | `		hWritePipe = hChildStdinWr;` |
|    ! 0 | 6520 | `		*phPipe = hChildStdinWr;` |
|      - | 6521 | `	}` |
|      - | 6522 |  |
|      - | 6523 | `	/* Convert command to wide string */` |
|      - | 6524 | `	{` |
|      5 | 6525 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|      5 | 6526 | `		if( nLen <= 0 ){` |
|    ! 0 | 6527 | `			goto cleanup_pipes;` |
|      - | 6528 | `		}` |
|      5 | 6529 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|      5 | 6530 | `		if( !zWideCmd ){` |
|    ! 0 | 6531 | `			goto cleanup_pipes;` |
|      - | 6532 | `		}` |
|      5 | 6533 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|      - | 6534 | `	}` |
|      - | 6535 |  |
|      - | 6536 | `	/* Set up process startup info */` |
|      5 | 6537 | `	ZeroMemory(&si, sizeof(si));` |
|      5 | 6538 | `	si.cb = sizeof(si);` |
|      5 | 6539 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|      5 | 6540 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|      5 | 6541 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|      5 | 6542 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|      5 | 6543 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|      - | 6544 |  |
|      5 | 6545 | `	ZeroMemory(&pi, sizeof(pi));` |
|      - | 6546 |  |
|      - | 6547 | `	/* Create the child process */` |
|      5 | 6548 | `	if( !CreateProcessW(` |
|      - | 6549 | `		NULL,           /* Application name */` |
|      - | 6550 | `		zWideCmd,       /* Command line */` |
|      - | 6551 | `		NULL,           /* Process security attributes */` |
|      - | 6552 | `		NULL,           /* Thread security attributes */` |
|      - | 6553 | `		TRUE,           /* Inherit handles */` |
|      - | 6554 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|      - | 6555 | `		NULL,           /* Environment */` |
|      - | 6556 | `		NULL,           /* Current directory */` |
|      - | 6557 | `		&si,            /* Startup info */` |
|      - | 6558 | `		&pi             /* Process info */` |
|      - | 6559 | `	)){` |
|    ! 0 | 6560 | `		goto cleanup_all;` |
|      - | 6561 | `	}` |
|      - | 6562 |  |
|      - | 6563 | `	/* Close handles we don't need in parent */` |
|      5 | 6564 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|      5 | 6565 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|      - | 6566 |  |
|      - | 6567 | `	/* Close thread handle (we only need process handle) */` |
|      5 | 6568 | `	CloseHandle(pi.hThread);` |
|      - | 6569 |  |
|      - | 6570 | `	/* Store process handle for later waiting */` |
|      5 | 6571 | `	*phProcess = pi.hProcess;` |
|      - | 6572 |  |
|      - | 6573 | `	/* Convert OS handle to C file descriptor, then to FILE* */` |
|      5 | 6574 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|      - | 6575 | `	                     bRead ? _O_RDONLY \| _O_TEXT : _O_WRONLY \| _O_TEXT);` |
|      5 | 6576 | `	if( fd == -1 ){` |
|    ! 0 | 6577 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 6578 | `		*phProcess = NULL;` |
|    ! 0 | 6579 | `		goto cleanup_all;` |
|      - | 6580 | `	}` |
|      - | 6581 |  |
|      5 | 6582 | `	pFile = _fdopen(fd, zMode);` |
|      5 | 6583 | `	if( !pFile ){` |
|    ! 0 | 6584 | `		_close(fd); /* This will also close the underlying handle */` |
|    ! 0 | 6585 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 6586 | `		*phProcess = NULL;` |
|    ! 0 | 6587 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    ! 0 | 6588 | `		return NULL;` |
|      - | 6589 | `	}` |
|      - | 6590 |  |
|      5 | 6591 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      5 | 6592 | `	return pFile;` |
|      - | 6593 |  |
|      - | 6594 | `cleanup_all:` |
|    ! 0 | 6595 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      - | 6596 | `cleanup_pipes:` |
|    ! 0 | 6597 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|    ! 0 | 6598 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|    ! 0 | 6599 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|    ! 0 | 6600 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|    ! 0 | 6601 | `	return NULL;` |
|      5 | 6602 | `}` |
|      - | 6603 |  |
|      - | 6604 | `/*` |
|      - | 6605 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|      - | 6606 | ` */` |
|      - | 6607 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|      5 | 6608 | `{` |
|      5 | 6609 | `	DWORD dwExitCode = 0;` |
|      - | 6610 | `	int status;` |
|      - | 6611 |  |
|      - | 6612 | `	/* Close the FILE* (this closes the pipe) */` |
|      5 | 6613 | `	fclose(pFile);` |
|      - | 6614 |  |
|      5 | 6615 | `	if( hProcess ){` |
|      - | 6616 | `		/* Wait for the process to complete */` |
|      5 | 6617 | `		WaitForSingleObject(hProcess, INFINITE);` |
|      - | 6618 |  |
|      5 | 6619 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|      5 | 6620 | `			status = (int)dwExitCode;` |
|      5 | 6621 | `		}else{` |
|    ! 0 | 6622 | `			status = -1;` |
|      - | 6623 | `		}` |
|      - | 6624 |  |
|      - | 6625 | `		/* Close process handle */` |
|      5 | 6626 | `		CloseHandle(hProcess);` |
|      5 | 6627 | `	}else{` |
|    ! 0 | 6628 | `		status = -1;` |
|      - | 6629 | `	}` |
|      - | 6630 |  |
|      5 | 6631 | `	return status;` |
|      5 | 6632 | `}` |
|      - | 6633 | `#endif /* __WINNT__ */` |
|      - | 6634 | `/*` |
|      - | 6635 | ` * Open a pipe to a process.` |
|      - | 6636 | ` * This is called internally by popen(), not through the stream device interface.` |
|      - | 6637 | ` */` |
|   3900 | 6638 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|      5 | 6639 | `{` |
|      - | 6640 | `	pipe_private *pPipe;` |
|      - | 6641 | `	FILE *pFile;` |
|   3905 | 6642 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|    ! 0 | 6643 | `		return 0;` |
|      - | 6644 | `	}` |
|      - | 6645 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|   3905 | 6646 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|    ! 0 | 6647 | `		return 0;` |
|      - | 6648 | `	}` |
|      - | 6649 | `	/* Open the pipe using system popen */` |
|      - | 6650 | `#ifdef __WINNT__` |
|      - | 6651 | `	{` |
|      - | 6652 | `		/* Build cmd.exe command wrapper */` |
|      5 | 6653 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|      5 | 6654 | `		const char *zShellSuffix = "\"";` |
|      5 | 6655 | `		size_t nPrefix = strlen(zShellPrefix);` |
|      5 | 6656 | `		size_t nSuffix = strlen(zShellSuffix);` |
|      5 | 6657 | `		size_t nCmd = strlen(zCommand);` |
|      5 | 6658 | `		size_t nQuotes = 0;` |
|      5 | 6659 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 6660 | `			if (zCommand[i] == '"') nQuotes++;` |
|      5 | 6661 | `		}` |
|      5 | 6662 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|      5 | 6663 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|      5 | 6664 | `		if (zCmdEsc == NULL) {` |
|    ! 0 | 6665 | `			return 0;` |
|      - | 6666 | `		}` |
|      - | 6667 | `		/* Escape quotes in command */` |
|      5 | 6668 | `		size_t j = 0;` |
|      5 | 6669 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 6670 | `			char ch = zCommand[i];` |
|      5 | 6671 | `			if (ch == '"') {` |
|      4 | 6672 | `				zCmdEsc[j++] = '^';` |
|      4 | 6673 | `				zCmdEsc[j++] = '"';` |
|      4 | 6674 | `			} else {` |
|      5 | 6675 | `				zCmdEsc[j++] = ch;` |
|      - | 6676 | `			}` |
|      5 | 6677 | `		}` |
|      5 | 6678 | `		zCmdEsc[j] = '\0';` |
|      5 | 6679 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|      5 | 6680 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|      5 | 6681 | `		if (zWinCmd == NULL) {` |
|    ! 0 | 6682 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 6683 | `			return 0;` |
|      - | 6684 | `		}` |
|      5 | 6685 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|      5 | 6686 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|      5 | 6687 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|      5 | 6688 | `		zWinCmd[nTotal - 1] = '\0';` |
|      - | 6689 | `		/* Allocate pipe structure early so we can store handles */` |
|      5 | 6690 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|      5 | 6691 | `		if( pPipe == 0 ){` |
|    ! 0 | 6692 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 6693 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|    ! 0 | 6694 | `			return 0;` |
|      - | 6695 | `		}` |
|      - | 6696 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|      5 | 6697 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|      5 | 6698 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|      5 | 6699 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|      5 | 6700 | `		if( pFile == 0 ){` |
|    ! 0 | 6701 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|    ! 0 | 6702 | `			return 0;` |
|      - | 6703 | `		}` |
|      - | 6704 | `		/* Initialize remaining fields */` |
|      5 | 6705 | `		pPipe->pFile = pFile;` |
|      5 | 6706 | `		pPipe->pVm = pVm;` |
|      5 | 6707 | `		pPipe->iMode = zMode[0];` |
|      - | 6708 | `	}` |
|      - | 6709 | `#elif defined(__UNIXES__) /* Unix */` |
|   3900 | 6710 | `	pFile = popen(zCommand, zMode);` |
|   3900 | 6711 | `	if( pFile == 0 ){` |
|    ! 0 | 6712 | `		return 0;` |
|      - | 6713 | `	}` |
|      - | 6714 | `	/* Allocate pipe private structure */` |
|   3900 | 6715 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|   3900 | 6716 | `	if( pPipe == 0 ){` |
|      - | 6717 | `		/* Out of memory, close the pipe */` |
|    ! 0 | 6718 | `		pclose(pFile);` |
|    ! 0 | 6719 | `		return 0;` |
|      - | 6720 | `	}` |
|      - | 6721 | `	/* Initialize the structure */` |
|   3900 | 6722 | `	pPipe->pFile = pFile;` |
|   3900 | 6723 | `	pPipe->pVm = pVm;` |
|   3900 | 6724 | `	pPipe->iMode = zMode[0];` |
|      - | 6725 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 6726 | `	(void)pFile;` |
|      - | 6727 | `	return 0;` |
|      - | 6728 | `#endif` |
|   3905 | 6729 | `	return pPipe;` |
|   1955 | 6730 | `}` |
|      - | 6731 | `/*` |
|      - | 6732 | ` * Close a pipe and return the exit status of the process.` |
|      - | 6733 | ` * Returns the exit status, or -1 on error.` |
|      - | 6734 | ` */` |
|   3874 | 6735 | `static int PipeClose(pipe_private *pPipe)` |
|      5 | 6736 | `{` |
|      - | 6737 | `	int status;` |
|      - | 6738 | `	ph7_vm *pVm;` |
|   3879 | 6739 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6740 | `		return -1;` |
|      - | 6741 | `	}` |
|   3879 | 6742 | `	pVm = pPipe->pVm;` |
|      - | 6743 | `	/* Close the pipe and get exit status */` |
|      - | 6744 | `#ifdef __WINNT__` |
|      - | 6745 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|      5 | 6746 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|      - | 6747 | `#elif defined(__UNIXES__)` |
|   3874 | 6748 | `	status = pclose(pPipe->pFile);` |
|      - | 6749 | `	/* On Unix, pclose returns the status from waitpid, need to extract exit code */` |
|   3874 | 6750 | `	if( status != -1 ){` |
|   3874 | 6751 | `		if( WIFEXITED(status) ){` |
|   3874 | 6752 | `			status = WEXITSTATUS(status);` |
|   1937 | 6753 | `		}else if( WIFSIGNALED(status) ){` |
|      - | 6754 | `			/* Process was killed by a signal - use shell convention: 128 + signal number */` |
|    ! 0 | 6755 | `			status = 128 + WTERMSIG(status);` |
|    ! 0 | 6756 | `		}else{` |
|      - | 6757 | `			/* Unknown termination reason */` |
|    ! 0 | 6758 | `			status = -1;` |
|      - | 6759 | `		}` |
|   1937 | 6760 | `	}` |
|      - | 6761 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 6762 | `	status = -1;` |
|      - | 6763 | `#endif` |
|      - | 6764 | `	/* Free the structure */` |
|   3879 | 6765 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|   3879 | 6766 | `	return status;` |
|   1942 | 6767 | `}` |
|      - | 6768 | `/*` |
|      - | 6769 | ` * Pipe stream xClose implementation.` |
|      - | 6770 | ` * Note: This is called by fclose(), not pclose().` |
|      - | 6771 | ` * It closes the pipe but does not return the exit status.` |
|      - | 6772 | ` */` |
|    100 | 6773 | `static void PipeStream_Close(void *pHandle)` |
|      4 | 6774 | `{` |
|    104 | 6775 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|    104 | 6776 | `	if( pPipe ){` |
|    104 | 6777 | `		PipeClose(pPipe);` |
|     50 | 6778 | `	}` |
|    104 | 6779 | `}` |
|      - | 6780 | `/*` |
|      - | 6781 | ` * Pipe stream xRead implementation.` |
|      - | 6782 | ` */` |
|   5784 | 6783 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|      4 | 6784 | `{` |
|   5788 | 6785 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 6786 | `	size_t nRead;` |
|   5788 | 6787 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6788 | `		return -1;` |
|      - | 6789 | `	}` |
|   5788 | 6790 | `	if( pPipe->iMode != 'r' ){` |
|      - | 6791 | `		/* Cannot read from a write-only pipe */` |
|    ! 0 | 6792 | `		return -1;` |
|      - | 6793 | `	}` |
|   5788 | 6794 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
|   5788 | 6795 | `	if( nRead == 0 ){` |
|   3904 | 6796 | `		if( feof(pPipe->pFile) ){` |
|   3904 | 6797 | `			return 0; /* EOF */` |
|      - | 6798 | `		}` |
|    ! 0 | 6799 | `		return -1; /* Error */` |
|      - | 6800 | `	}` |
|   1888 | 6801 | `	return (ph7_int64)nRead;` |
|   2896 | 6802 | `}` |
|      - | 6803 | `/*` |
|      - | 6804 | ` * Pipe stream xWrite implementation.` |
|      - | 6805 | ` */` |
|      2 | 6806 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|    ! 0 | 6807 | `{` |
|      2 | 6808 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 6809 | `	size_t nWritten;` |
|      2 | 6810 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6811 | `		return -1;` |
|      - | 6812 | `	}` |
|      2 | 6813 | `	if( pPipe->iMode != 'w' ){` |
|      - | 6814 | `		/* Cannot write to a read-only pipe */` |
|    ! 0 | 6815 | `		return -1;` |
|      - | 6816 | `	}` |
|      2 | 6817 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|      2 | 6818 | `	if( nWritten == 0 && nWrite > 0 ){` |
|    ! 0 | 6819 | `		return -1; /* Error */` |
|      - | 6820 | `	}` |
|      2 | 6821 | `	return (ph7_int64)nWritten;` |
|      1 | 6822 | `}` |
|      - | 6823 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|      - | 6824 | `static const ph7_io_stream sPipe_Stream = {` |
|      - | 6825 | `	"pipe",` |
|      - | 6826 | `	PH7_IO_STREAM_VERSION,` |
|      - | 6827 | `	0,  /* xOpen - not used, pipes opened via PipeOpen() */` |
|      - | 6828 | `	0,  /* xOpenDir */` |
|      - | 6829 | `	PipeStream_Close,  /* xClose */` |
|      - | 6830 | `	0,  /* xCloseDir */` |
|      - | 6831 | `	PipeStream_Read,   /* xRead */` |
|      - | 6832 | `	0,  /* xReadDir */` |
|      - | 6833 | `	PipeStream_Write,  /* xWrite */` |
|      - | 6834 | `	0,  /* xSeek */` |
|      - | 6835 | `	0,  /* xLock */` |
|      - | 6836 | `	0,  /* xRewindDir */` |
|      - | 6837 | `	0,  /* xTell */` |
|      - | 6838 | `	0,  /* xTrunc */` |
|      - | 6839 | `	0,  /* xSync */` |
|      - | 6840 | `	0   /* xStat */` |
|      - | 6841 | `};` |
|      - | 6842 | `/*` |
|      - | 6843 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|      - | 6844 | ` * FALSE otherwise.` |
|      - | 6845 | ` */` |
|   3770 | 6846 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|      5 | 6847 | `{` |
|   3775 | 6848 | `	return pStream == &sPipe_Stream;` |
|      5 | 6849 | `}` |
|      - | 6850 | `/*` |
|      - | 6851 | ` * resource popen(string $command, string $mode)` |
|      - | 6852 | ` *  Opens process file pointer.` |
|      - | 6853 | ` * Parameters` |
|      - | 6854 | ` *  $command` |
|      - | 6855 | ` *   The command to execute. Passed to the system shell.` |
|      - | 6856 | ` *  $mode` |
|      - | 6857 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|      - | 6858 | ` *   'r' - Open for reading (read from the command's stdout).` |
|      - | 6859 | ` *   'w' - Open for writing (write to the command's stdin).` |
|      - | 6860 | ` * Return` |
|      - | 6861 | ` *  Returns a file pointer on success, or FALSE on error.` |
|      - | 6862 | ` */` |
|      - | 6863 | `/*` |
|      - | 6864 | ` * string\|false\|null shell_exec(string $command)` |
|      - | 6865 | ` *  Execute a command via the shell and return the complete output as a string.` |
|      - | 6866 | ` * Returns NULL when the command produces no output, FALSE when the pipe cannot be` |
|      - | 6867 | ` * opened. This is what the backtick operator compiles to, exactly as in php.` |
|      - | 6868 | ` */` |
|      4 | 6869 | `static int PH7_builtin_shell_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6870 | `{` |
|      - | 6871 | `	const char *zCommand;` |
|      - | 6872 | `	pipe_private *pPipe;` |
|      - | 6873 | `	SyBlob sOut;` |
|      - | 6874 | `	char zBuf[4096];` |
|      - | 6875 | `	size_t nRead;` |
|      - | 6876 | `	int nCmdLen;` |
|      6 | 6877 | `	if( nArg < 1 ){` |
|    ! 0 | 6878 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6879 | `		return PH7_OK;` |
|      - | 6880 | `	}` |
|      6 | 6881 | `	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);` |
|      6 | 6882 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 6883 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6884 | `		return PH7_OK;` |
|      - | 6885 | `	}` |
|      6 | 6886 | `	pPipe = PipeOpen(pCtx->pVm,zCommand,"r");` |
|      6 | 6887 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 6888 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6889 | `		return PH7_OK;` |
|      - | 6890 | `	}` |
|      6 | 6891 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|      4 | 6892 | `	for(;;){` |
|     10 | 6893 | `		nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);` |
|     10 | 6894 | `		if( nRead < 1 ){` |
|      6 | 6895 | `			break;` |
|      - | 6896 | `		}` |
|      6 | 6897 | `		SyBlobAppend(&sOut,zBuf,(sxu32)nRead);` |
|      2 | 6898 | `	}` |
|      6 | 6899 | `	PipeClose(pPipe);` |
|      6 | 6900 | `	if( SyBlobLength(&sOut) < 1 ){` |
|      - | 6901 | `		/* php answers NULL, not "", when the command printed nothing */` |
|    ! 0 | 6902 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6903 | `	}else{` |
|      6 | 6904 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|      - | 6905 | `	}` |
|      6 | 6906 | `	SyBlobRelease(&sOut);` |
|      6 | 6907 | `	return PH7_OK;` |
|      4 | 6908 | `}` |
|   3896 | 6909 | `static int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6910 | `{` |
|      - | 6911 | `	const char *zCommand, *zMode;` |
|      - | 6912 | `	pipe_private *pPipe;` |
|      - | 6913 | `	io_private *pDev;` |
|      - | 6914 | `	int nCmdLen, nModeLen;` |
|   3901 | 6915 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 6916 | `		/* Missing/Invalid arguments, return FALSE */` |
|    ! 0 | 6917 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a command string and mode");` |
|    ! 0 | 6918 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6919 | `		return PH7_OK;` |
|      - | 6920 | `	}` |
|      - | 6921 | `	/* Extract the command and mode */` |
|   3901 | 6922 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|   3901 | 6923 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|   3901 | 6924 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 6925 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty command");` |
|    ! 0 | 6926 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6927 | `		return PH7_OK;` |
|      - | 6928 | `	}` |
|   3901 | 6929 | `	if( nModeLen < 1 \|\| (zMode[0] != 'r' && zMode[0] != 'w') ){` |
|    ! 0 | 6930 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Invalid mode, expected 'r' or 'w'");` |
|    ! 0 | 6931 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6932 | `		return PH7_OK;` |
|      - | 6933 | `	}` |
|      - | 6934 | `	/* Open the pipe */` |
|   3901 | 6935 | `	pPipe = PipeOpen(pCtx->pVm, zCommand, zMode);` |
|   3901 | 6936 | `	if( pPipe == 0 ){` |
|      - | 6937 | `		/* Failed to open pipe */` |
|    ! 0 | 6938 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6939 | `		return PH7_OK;` |
|      - | 6940 | `	}` |
|      - | 6941 | `	/* Allocate an io_private instance to wrap the pipe */` |
|   3901 | 6942 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|   3901 | 6943 | `	if( pDev == 0 ){` |
|    ! 0 | 6944 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|    ! 0 | 6945 | `		PipeClose(pPipe);` |
|    ! 0 | 6946 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6947 | `		return PH7_OK;` |
|      - | 6948 | `	}` |
|      - | 6949 | `	/* Initialize the io_private structure */` |
|   3901 | 6950 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|   3901 | 6951 | `	pDev->pHandle = pPipe;` |
|      - | 6952 | `	/* Return the io_private instance as a resource */` |
|   3901 | 6953 | `	ph7_result_resource(pCtx, pDev);` |
|   3901 | 6954 | `	return PH7_OK;` |
|   1953 | 6955 | `}` |
|      - | 6956 | `/*` |
|      - | 6957 | ` * int pclose(resource $handle)` |
|      - | 6958 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|      - | 6959 | ` * Parameters` |
|      - | 6960 | ` *  $handle` |
|      - | 6961 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|      - | 6962 | ` * Return` |
|      - | 6963 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|      - | 6964 | ` */` |
|   3770 | 6965 | `static int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6966 | `{` |
|      - | 6967 | `	const ph7_io_stream *pStream;` |
|      - | 6968 | `	pipe_private *pPipe;` |
|      - | 6969 | `	io_private *pDev;` |
|      - | 6970 | `	int status;` |
|   3775 | 6971 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 6972 | `		/* Missing/Invalid arguments, return -1 */` |
|    ! 0 | 6973 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 6974 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 6975 | `		return PH7_OK;` |
|      - | 6976 | `	}` |
|      - | 6977 | `	/* Extract our private data */` |
|   3775 | 6978 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 6979 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   3775 | 6980 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 6981 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 6982 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 6983 | `		return PH7_OK;` |
|      - | 6984 | `	}` |
|      - | 6985 | `	/* Point to the target IO stream device */` |
|   3775 | 6986 | `	pStream = pDev->pStream;` |
|   3775 | 6987 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|    ! 0 | 6988 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|    ! 0 | 6989 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 6990 | `		return PH7_OK;` |
|      - | 6991 | `	}` |
|      - | 6992 | `	/* Get the pipe handle */` |
|   3775 | 6993 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|      - | 6994 | `	/* Close the pipe and get exit status */` |
|   3775 | 6995 | `	status = PipeClose(pPipe);` |
|      - | 6996 | `	/* Release the IO private structure */` |
|   3775 | 6997 | `	ReleaseIOPrivate(pCtx, pDev);` |
|      - | 6998 | `	/* Invalidate the resource handle */` |
|   3775 | 6999 | `	ph7_value_release(apArg[0]);` |
|      - | 7000 | `	/* Return the exit status */` |
|   3775 | 7001 | `	ph7_result_int(pCtx, status);` |
|   3775 | 7002 | `	return PH7_OK;` |
|   1890 | 7003 | `}` |
|      - | 7004 | `/* Export the php:// stream */` |
|      - | 7005 | `static const ph7_io_stream sPHP_Stream = {` |
|      - | 7006 | `	"php",` |
|      - | 7007 | `	PH7_IO_STREAM_VERSION,` |
|      - | 7008 | `	PHPStreamData_Open,  /* xOpen */` |
|      - | 7009 | `	0,   /* xOpenDir */` |
|      - | 7010 | `	PHPStreamData_Close, /* xClose */` |
|      - | 7011 | `	0,  /* xCloseDir */` |
|      - | 7012 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 7013 | `	0,  /* xReadDir */` |
|      - | 7014 | `	PHPStreamData_Write, /* xWrite */` |
|      - | 7015 | `	PHPStreamData_Seek,  /* xSeek (php://memory & php://temp) */` |
|      - | 7016 | `	0,  /* xLock */` |
|      - | 7017 | `	0,  /* xRewindDir */` |
|      - | 7018 | `	PHPStreamData_Tell,  /* xTell */` |
|      - | 7019 | `	PHPStreamData_Trunc, /* xTrunc */` |
|      - | 7020 | `	0,  /* xSync */` |
|      - | 7021 | `	0   /* xStat */` |
|      - | 7022 | `};` |
|      - | 7023 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7024 | `/*` |
|      - | 7025 | ` * Return TRUE if we are dealing with the php:// stream.` |
|      - | 7026 | ` * FALSE otherwise.` |
|      - | 7027 | ` */` |
|     80 | 7028 | `static int is_php_stream(const ph7_io_stream *pStream)` |
|      3 | 7029 | `{` |
|      - | 7030 | `#ifndef PH7_DISABLE_DISK_IO` |
|     83 | 7031 | `	return pStream == &sPHP_Stream;` |
|      - | 7032 | `#else` |
|      - | 7033 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 7034 | `	return 0;` |
|      - | 7035 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      3 | 7036 | `}` |
|      - | 7037 | `/*` |
|      - | 7038 | ` * Return TRUE if we are dealing with the data:// stream.` |
|      - | 7039 | ` */` |
|     70 | 7040 | `static int is_data_stream(const ph7_io_stream *pStream)` |
|      3 | 7041 | `{` |
|      - | 7042 | `#ifndef PH7_DISABLE_DISK_IO` |
|     73 | 7043 | `	return pStream == &sDATA_Stream;` |
|      - | 7044 | `#else` |
|      - | 7045 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 7046 | `	return 0;` |
|      - | 7047 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      3 | 7048 | `}` |
|      - | 7049 |  |
|      - | 7050 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 7051 | `/*` |
|      - | 7052 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 7053 | ` * [i.e: file://,php://].` |
|      - | 7054 | ` * Note:` |
|      - | 7055 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 7056 | ` *  defined then this function is a no-op.` |
|      - | 7057 | ` */` |
|   3342 | 7058 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 7059 | `{` |
|      - | 7060 | `	/*` |
|      - | 7061 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 7062 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 7063 | `	 */` |
|      - | 7064 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7065 | `	/* VFS: disk I/O related functions */` |
|      - | 7066 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 7067 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 7068 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 7069 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 7070 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 7071 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 7072 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 7073 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 7074 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 7075 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 7076 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 7077 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 7078 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 7079 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 7080 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 7081 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 7082 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 7083 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 7084 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 7085 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 7086 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 7087 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 7088 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 7089 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 7090 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 7091 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 7092 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 7093 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 7094 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 7095 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 7096 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 7097 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 7098 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 7099 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 7100 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 7101 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 7102 | `		{"link",        PH7_vfs_link     },` |
|      - | 7103 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 7104 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 7105 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 7106 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 7107 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 7108 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 7109 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 7110 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 7111 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 7112 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 7113 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 7114 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 7115 | `	};` |
|      - | 7116 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 7117 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 7118 | `	 */` |
|      - | 7119 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 7120 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 7121 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 7122 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 7123 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 7124 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 7125 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 7126 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 7127 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 7128 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 7129 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 7130 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 7131 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 7132 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 7133 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 7134 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 7135 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 7136 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 7137 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 7138 | `		{"file",      PH7_builtin_file   },` |
|      - | 7139 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 7140 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 7141 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 7142 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 7143 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 7144 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 7145 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 7146 | `		{"stream_get_contents",  PH7_builtin_stream_get_contents },` |
|      - | 7147 | `		{"stream_get_wrappers",  PH7_builtin_stream_get_wrappers },` |
|      - | 7148 | `		{"stream_get_meta_data", PH7_builtin_stream_get_meta_data },` |
|      - | 7149 | `		{"stream_context_create",PH7_builtin_stream_context_create },` |
|      - | 7150 | `		{"stream_wrapper_register",   PH7_builtin_stream_wrapper_register },` |
|      - | 7151 | `		{"stream_register_wrapper",   PH7_builtin_stream_wrapper_register },` |
|      - | 7152 | `		{"stream_wrapper_unregister", PH7_builtin_stream_wrapper_unregister },` |
|      - | 7153 | `#ifdef PH7_ENABLE_NET` |
|      - | 7154 | `		{"fsockopen",  PH7_builtin_fsockopen },` |
|      - | 7155 | `		{"pfsockopen", PH7_builtin_fsockopen },` |
|      - | 7156 | `		{"stream_socket_client", PH7_builtin_fsockopen },` |
|      - | 7157 | `#endif` |
|      - | 7158 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 7159 | `		{"shell_exec", PH7_builtin_shell_exec },` |
|      - | 7160 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 7161 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 7162 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 7163 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 7164 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 7165 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 7166 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 7167 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7168 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 7169 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 7170 | `	};` |
|   3347 | 7171 | `	const ph7_io_stream *pFileStream = 0;` |
|   3347 | 7172 | `	sxu32 n = 0;` |
|      - | 7173 | `	/* Register disk-related functions */` |
| 163763 | 7174 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 160421 | 7175 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
|  80213 | 7176 | `	}` |
| 157079 | 7177 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 153737 | 7178 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  76871 | 7179 | `	}` |
|      - | 7180 | `#else` |
|      - | 7181 | `	SXUNUSED(pVm);` |
|      - | 7182 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7183 |  |
|      - | 7184 | `	/*` |
|      - | 7185 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7186 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 7187 | `	 */` |
|      - | 7188 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7189 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 7190 | `		/* Path processing */` |
|      - | 7191 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 7192 | `		{"basename",    PH7_builtin_basename },` |
|      - | 7193 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 7194 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 7195 | `		{"fnmatch",     PH7_builtin_fnmatch  },` |
|      - | 7196 | `		/* ZIP processing */` |
|      - | 7197 | `		{"zip_open",    PH7_builtin_zip_open },` |
|      - | 7198 | `		{"zip_close",   PH7_builtin_zip_close},` |
|      - | 7199 | `		{"zip_read",    PH7_builtin_zip_read },` |
|      - | 7200 | `		{"zip_entry_open", PH7_builtin_zip_entry_open },` |
|      - | 7201 | `		{"zip_entry_close",PH7_builtin_zip_entry_close},` |
|      - | 7202 | `		{"zip_entry_name", PH7_builtin_zip_entry_name },` |
|      - | 7203 | `		{"zip_entry_filesize",      PH7_builtin_zip_entry_filesize       },` |
|      - | 7204 | `		{"zip_entry_compressedsize",PH7_builtin_zip_entry_compressedsize },` |
|      - | 7205 | `		{"zip_entry_read", PH7_builtin_zip_entry_read },` |
|      - | 7206 | `		{"zip_entry_reset_read_cursor",PH7_builtin_zip_entry_reset_read_cursor},` |
|      - | 7207 | `		{"zip_entry_compressionmethod",PH7_builtin_zip_entry_compressionmethod}` |
|      - | 7208 | `	};` |
|  56819 | 7209 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  53477 | 7210 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|  26741 | 7211 | `	}` |
|      - | 7212 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 7213 |  |
|      - | 7214 | `	/* Install streams if disk I/O is enabled */` |
|      - | 7215 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7216 | `#ifdef __WINNT__` |
|      5 | 7217 | `	pFileStream = &sWinFileStream;` |
|      - | 7218 | `#elif defined(__UNIXES__)` |
|   3342 | 7219 | `	pFileStream = &sUnixFileStream;` |
|      - | 7220 | `#endif` |
|      - | 7221 | `	/* Install the php:// stream */` |
|   3347 | 7222 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   3347 | 7223 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);` |
|      - | 7224 | `#ifdef PH7_ENABLE_NET` |
|   3347 | 7225 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);` |
|      - | 7226 | `#endif` |
|   3347 | 7227 | `	if( pFileStream ){` |
|      - | 7228 | `		/* Install the file:// stream */` |
|   3347 | 7229 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   1671 | 7230 | `	}` |
|      - | 7231 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7232 |  |
|   3347 | 7233 | `	return SXRET_OK;` |
|      5 | 7234 | `}` |
|      - | 7235 | `/*` |
|      - | 7236 | ` * Export the STDIN handle.` |
|      - | 7237 | ` */` |
|      2 | 7238 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|      1 | 7239 | `{` |
|      - | 7240 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 7241 | `	if( pVm->pStdin == 0  ){` |
|      - | 7242 | `		io_private *pIn;` |
|      - | 7243 | `		/* Allocate an IO private instance */` |
|      3 | 7244 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 7245 | `		if( pIn == 0 ){` |
|    ! 0 | 7246 | `			return 0;` |
|      - | 7247 | `		}` |
|      3 | 7248 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|      - | 7249 | `		/* Initialize the handle */` |
|      3 | 7250 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|      - | 7251 | `		/* Install the STDIN stream */` |
|      3 | 7252 | `		pVm->pStdin = pIn;` |
|      3 | 7253 | `		return pIn;` |
|    ! 0 | 7254 | `	}else{` |
|      - | 7255 | `		/* NULL or STDIN */` |
|    ! 0 | 7256 | `		return pVm->pStdin;` |
|      - | 7257 | `	}` |
|      - | 7258 | `#else` |
|      - | 7259 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7260 | `	return 0;` |
|      - | 7261 | `#endif` |
|      2 | 7262 | `}` |
|      - | 7263 | `/*` |
|      - | 7264 | ` * Export the STDOUT handle.` |
|      - | 7265 | ` */` |
|      2 | 7266 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|      1 | 7267 | `{` |
|      - | 7268 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 7269 | `	if( pVm->pStdout == 0  ){` |
|      - | 7270 | `		io_private *pOut;` |
|      - | 7271 | `		/* Allocate an IO private instance */` |
|      3 | 7272 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 7273 | `		if( pOut == 0 ){` |
|    ! 0 | 7274 | `			return 0;` |
|      - | 7275 | `		}` |
|      3 | 7276 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|      - | 7277 | `		/* Initialize the handle */` |
|      3 | 7278 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|      - | 7279 | `		/* Install the STDOUT stream */` |
|      3 | 7280 | `		pVm->pStdout = pOut;` |
|      3 | 7281 | `		return pOut;` |
|    ! 0 | 7282 | `	}else{` |
|      - | 7283 | `		/* NULL or STDOUT */` |
|    ! 0 | 7284 | `		return pVm->pStdout;` |
|      - | 7285 | `	}` |
|      - | 7286 | `#else` |
|      - | 7287 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7288 | `	return 0;` |
|      - | 7289 | `#endif` |
|      2 | 7290 | `}` |
|      - | 7291 | `/*` |
|      - | 7292 | ` * Export the STDERR handle.` |
|      - | 7293 | ` */` |
|      2 | 7294 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|      1 | 7295 | `{` |
|      - | 7296 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 7297 | `	if( pVm->pStderr == 0  ){` |
|      - | 7298 | `		io_private *pErr;` |
|      - | 7299 | `		/* Allocate an IO private instance */` |
|      3 | 7300 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 7301 | `		if( pErr == 0 ){` |
|    ! 0 | 7302 | `			return 0;` |
|      - | 7303 | `		}` |
|      3 | 7304 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|      - | 7305 | `		/* Initialize the handle */` |
|      3 | 7306 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|      - | 7307 | `		/* Install the STDERR stream */` |
|      3 | 7308 | `		pVm->pStderr = pErr;` |
|      3 | 7309 | `		return pErr;` |
|    ! 0 | 7310 | `	}else{` |
|      - | 7311 | `		/* NULL or STDERR */` |
|    ! 0 | 7312 | `		return pVm->pStderr;` |
|      - | 7313 | `	}` |
|      - | 7314 | `#else` |
|      - | 7315 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 7316 | `	return 0;` |
|      - | 7317 | `#endif` |
|      2 | 7318 | `}` |
|      - | 7319 |  |
