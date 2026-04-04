# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1838/2833 lines (64.88%)

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
|     30 |   19 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)` |
|      2 |   20 |  |
|     32 |   21 | `	const char *zEnd = &zPath[nByte - 1];` |
|      - |   22 | `	int c,d;` |
|     32 |   23 | `	c = d = '/';` |
|      - |   24 | `#ifdef __WINNT__` |
|      2 |   25 | `	d = '\\';` |
|      - |   26 | `#endif` |
|    505 |   27 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    460 |   28 | `		zEnd--;` |
|      2 |   29 | `	}` |
|     32 |   30 | `	*pLen = (int)(zEnd-zPath);` |
|      - |   31 | `#ifdef __WINNT__` |
|      2 |   32 | `	if( (*pLen) == (int)sizeof(char) && zPath[0] == '/' ){` |
|      - |   33 | `		/* Normalize path on windows */` |
|    ! 0 |   34 | `		return "\\";` |
|      - |   35 | `	}` |
|      - |   36 | `#endif` |
|     32 |   37 | `	if( zEnd == zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d) ){` |
|      - |   38 | `		/* No separator,return "." as the current directory */` |
|      5 |   39 | `		*pLen = sizeof(char);` |
|      5 |   40 | `		return ".";` |
|      - |   41 | `	}` |
|     28 |   42 | `	if( (*pLen) == 0 ){` |
|      2 |   43 | `		*pLen = sizeof(char);` |
|      - |   44 | `#ifdef __WINNT__` |
|    ! 0 |   45 | `		return "\\";` |
|      - |   46 | `#else` |
|      2 |   47 | `		return "/";` |
|      - |   48 | `#endif` |
|      - |   49 | `	}` |
|     26 |   50 | `	return zPath;` |
|     17 |   51 |  |
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
|  10382 |   66 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   67 |  |
|      - |   68 | `	const char *zPath;` |
|      - |   69 | `	ph7_vfs *pVfs;` |
|      - |   70 | `	int rc;` |
|  10384 |   71 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |   72 | `		/* Missing/Invalid argument,return FALSE */` |
|      8 |   73 | `		ph7_result_bool(pCtx,0);` |
|      8 |   74 | `		return PH7_OK;` |
|      - |   75 | `	}` |
|      - |   76 | `	/* Point to the underlying vfs */` |
|  10378 |   77 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  10378 |   78 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |   79 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |   80 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |   81 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |   82 | `			ph7_function_name(pCtx)` |
|      - |   83 | `			);` |
|    ! 0 |   84 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |   85 | `		return PH7_OK;` |
|      - |   86 | `	}` |
|      - |   87 | `	/* Point to the desired directory */` |
|  10378 |   88 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |   89 | `	/* Perform the requested operation */` |
|  10378 |   90 | `	rc = pVfs->xChdir(zPath);` |
|      - |   91 | `	/* IO return value */` |
|  10378 |   92 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  10378 |   93 | `	return PH7_OK;` |
|   5193 |   94 |  |
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
|      1 |  105 |  |
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
|      4 |  132 |  |
|      - |  133 | `/*` |
|      - |  134 | ` * string getcwd(void)` |
|      - |  135 | ` *  Gets the current working directory.` |
|      - |  136 | ` * Parameters` |
|      - |  137 | ` *  None` |
|      - |  138 | ` * Return` |
|      - |  139 | ` *  Returns the current working directory on success, or FALSE on failure.` |
|      - |  140 | ` */` |
|     20 |  141 | `static int PH7_vfs_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  142 |  |
|      - |  143 | `	ph7_vfs *pVfs;` |
|      - |  144 | `	int rc;` |
|      - |  145 | `	/* Point to the underlying vfs */` |
|     22 |  146 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     22 |  147 | `	if( pVfs == 0 \|\| pVfs->xGetcwd == 0 ){` |
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
|     22 |  158 | `	ph7_result_string(pCtx,"",0);` |
|      - |  159 | `	/* Perform the requested operation */` |
|     22 |  160 | `	rc = pVfs->xGetcwd(pCtx);` |
|     22 |  161 | `	if( rc != PH7_OK ){` |
|      - |  162 | `		/* Error,return FALSE */` |
|    ! 0 |  163 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  164 | `	}` |
|     22 |  165 | `	return PH7_OK;` |
|     12 |  166 |  |
|      - |  167 | `/*` |
|      - |  168 | ` * bool rmdir(string $directory)` |
|      - |  169 | ` *  Removes directory.` |
|      - |  170 | ` * Parameters` |
|      - |  171 | ` *  $directory` |
|      - |  172 | ` *   The path to the directory` |
|      - |  173 | ` * Return` |
|      - |  174 | ` *  TRUE on success or FALSE on failure.` |
|      - |  175 | ` */` |
|     26 |  176 | `static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  177 |  |
|      - |  178 | `	const char *zPath;` |
|      - |  179 | `	ph7_vfs *pVfs;` |
|      - |  180 | `	int rc;` |
|     27 |  181 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  182 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  183 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  184 | `		return PH7_OK;` |
|      - |  185 | `	}` |
|      - |  186 | `	/* Point to the underlying vfs */` |
|     27 |  187 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     27 |  188 | `	if( pVfs == 0 \|\| pVfs->xRmdir == 0 ){` |
|      - |  189 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  190 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  191 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  192 | `			ph7_function_name(pCtx)` |
|      - |  193 | `			);` |
|    ! 0 |  194 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  195 | `		return PH7_OK;` |
|      - |  196 | `	}` |
|      - |  197 | `	/* Point to the desired directory */` |
|     27 |  198 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  199 | `	/* Perform the requested operation */` |
|     27 |  200 | `	rc = pVfs->xRmdir(zPath);` |
|      - |  201 | `	/* IO return value */` |
|     27 |  202 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     27 |  203 | `	return PH7_OK;` |
|     14 |  204 |  |
|      - |  205 | `/*` |
|      - |  206 | ` * bool is_dir(string $filename)` |
|      - |  207 | ` *  Tells whether the given filename is a directory.` |
|      - |  208 | ` * Parameters` |
|      - |  209 | ` *  $filename` |
|      - |  210 | ` *   Path to the file.` |
|      - |  211 | ` * Return` |
|      - |  212 | ` *  TRUE on success or FALSE on failure.` |
|      - |  213 | ` */` |
|   6180 |  214 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  215 |  |
|      - |  216 | `	const char *zPath;` |
|      - |  217 | `	ph7_vfs *pVfs;` |
|      - |  218 | `	int rc;` |
|   6182 |  219 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  220 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  221 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  222 | `		return PH7_OK;` |
|      - |  223 | `	}` |
|      - |  224 | `	/* Point to the underlying vfs */` |
|   6182 |  225 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   6182 |  226 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  227 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  228 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  229 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  230 | `			ph7_function_name(pCtx)` |
|      - |  231 | `			);` |
|    ! 0 |  232 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  233 | `		return PH7_OK;` |
|      - |  234 | `	}` |
|      - |  235 | `	/* Point to the desired directory */` |
|   6182 |  236 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  237 | `	/* Perform the requested operation */` |
|   6182 |  238 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  239 | `	/* IO return value */` |
|   6182 |  240 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   6182 |  241 | `	return PH7_OK;` |
|   3092 |  242 |  |
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
|     26 |  262 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  263 |  |
|     27 |  264 | `	int iRecursive = 0;` |
|      - |  265 | `	const char *zPath;` |
|      - |  266 | `	ph7_vfs *pVfs;` |
|      - |  267 | `	int iMode,rc;` |
|     27 |  268 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  269 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  270 | `		ph7_result_bool(pCtx,0);` |
|      3 |  271 | `		return PH7_OK;` |
|      - |  272 | `	}` |
|      - |  273 | `	/* Point to the underlying vfs */` |
|     25 |  274 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     25 |  275 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  276 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  277 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  278 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  279 | `			ph7_function_name(pCtx)` |
|      - |  280 | `			);` |
|    ! 0 |  281 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  282 | `		return PH7_OK;` |
|      - |  283 | `	}` |
|      - |  284 | `	/* Point to the desired directory */` |
|     25 |  285 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  286 | `#ifdef __WINNT__` |
|      1 |  287 | `	iMode = 0;` |
|      - |  288 | `#else` |
|      - |  289 | `	/* Assume UNIX */` |
|     24 |  290 | `	iMode = 0777;` |
|      - |  291 | `#endif` |
|     25 |  292 | `	if( nArg > 1 ){` |
|    ! 0 |  293 | `		iMode = ph7_value_to_int(apArg[1]);` |
|    ! 0 |  294 | `		if( nArg > 2 ){` |
|    ! 0 |  295 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|    ! 0 |  296 | `		}` |
|    ! 0 |  297 | `	}` |
|      - |  298 | `	/* Perform the requested operation */` |
|     25 |  299 | `	rc = pVfs->xMkdir(zPath,iMode,iRecursive);` |
|      - |  300 | `	/* IO return value */` |
|     25 |  301 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     25 |  302 | `	return PH7_OK;` |
|     14 |  303 |  |
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
|      1 |  316 |  |
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
|      3 |  343 |  |
|      - |  344 | `/*` |
|      - |  345 | ` * string realpath(string $path)` |
|      - |  346 | ` *  Returns canonicalized absolute pathname.` |
|      - |  347 | ` * Parameters` |
|      - |  348 | ` *  $path` |
|      - |  349 | ` *   Target path.` |
|      - |  350 | ` * Return` |
|      - |  351 | ` *  Canonicalized absolute pathname on success. or FALSE on failure.` |
|      - |  352 | ` */` |
|     10 |  353 | `static int PH7_vfs_realpath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  354 |  |
|      - |  355 | `	const char *zPath;` |
|      - |  356 | `	ph7_vfs *pVfs;` |
|      - |  357 | `        int rc;` |
|     11 |  358 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  359 | `		/* Missing/Invalid argument,return FALSE */` |
|      7 |  360 | `		ph7_result_bool(pCtx,0);` |
|      7 |  361 | `		return PH7_OK;` |
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
|      6 |  383 |  |
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
|      1 |  394 |  |
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
|      4 |  430 |  |
|      - |  431 | `/*` |
|      - |  432 | ` * void usleep(int $micro_seconds)` |
|      - |  433 | ` *  Delays program execution for the given number of micro seconds.` |
|      - |  434 | ` * Parameters` |
|      - |  435 | ` *  $micro_seconds` |
|      - |  436 | ` *   Halt time in micro seconds. A micro second is one millionth of a second.` |
|      - |  437 | ` * Return` |
|      - |  438 | ` *  None.` |
|      - |  439 | ` */` |
|     40 |  440 | `static int PH7_vfs_usleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  441 |  |
|      - |  442 | `	ph7_vfs *pVfs;` |
|      - |  443 | `	int nSleep;` |
|     41 |  444 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  445 | `		/* Missing/Invalid argument,return immediately */` |
|    ! 0 |  446 | `		return PH7_OK;` |
|      - |  447 | `	}` |
|      - |  448 | `	/* Point to the underlying vfs */` |
|     41 |  449 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     41 |  450 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  451 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  452 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  453 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 |  454 | `			ph7_function_name(pCtx)` |
|      - |  455 | `			);` |
|    ! 0 |  456 | `		return PH7_OK;` |
|      - |  457 | `	}` |
|      - |  458 | `	/* Amount to sleep */` |
|     41 |  459 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|     41 |  460 | `	if( nSleep < 0 ){` |
|      - |  461 | `		/* Invalid value,return immediately */` |
|      3 |  462 | `		return PH7_OK;` |
|      - |  463 | `	}` |
|      - |  464 | `	/* Perform the requested operation (Microseconds) */` |
|     39 |  465 | `	pVfs->xSleep((unsigned int)nSleep);` |
|     39 |  466 | `	return PH7_OK;` |
|     21 |  467 |  |
|      - |  468 | `/*` |
|      - |  469 | ` * bool unlink (string $filename)` |
|      - |  470 | ` *  Delete a file.` |
|      - |  471 | ` * Parameters` |
|      - |  472 | ` *  $filename` |
|      - |  473 | ` *   Path to the file.` |
|      - |  474 | ` * Return` |
|      - |  475 | ` *  TRUE on success or FALSE on failure.` |
|      - |  476 | ` */` |
|  22268 |  477 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  478 |  |
|      - |  479 | `	const char *zPath;` |
|      - |  480 | `	ph7_vfs *pVfs;` |
|      - |  481 | `	int rc;` |
|  22270 |  482 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  483 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  484 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  485 | `		return PH7_OK;` |
|      - |  486 | `	}` |
|      - |  487 | `	/* Point to the underlying vfs */` |
|  22270 |  488 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  22270 |  489 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  490 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  491 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  492 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  493 | `			ph7_function_name(pCtx)` |
|      - |  494 | `			);` |
|    ! 0 |  495 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  496 | `		return PH7_OK;` |
|      - |  497 | `	}` |
|      - |  498 | `	/* Point to the desired directory */` |
|  22270 |  499 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  500 | `	/* Perform the requested operation */` |
|  22270 |  501 | `	rc = pVfs->xUnlink(zPath);` |
|      - |  502 | `	/* IO return value */` |
|  22270 |  503 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  22270 |  504 | `	return PH7_OK;` |
|  11136 |  505 |  |
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
|    ! 0 |  518 |  |
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
|      5 |  548 |  |
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
|      1 |  561 |  |
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
|      4 |  590 |  |
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
|      1 |  603 |  |
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
|      4 |  632 |  |
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
|      1 |  643 |  |
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
|      2 |  670 |  |
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
|    ! 0 |  681 |  |
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
|      1 |  708 |  |
|      - |  709 | `/*` |
|      - |  710 | ` * bool file_exists(string $filename)` |
|      - |  711 | ` *  Checks whether a file or directory exists.` |
|      - |  712 | ` * Parameters` |
|      - |  713 | ` *  $filename` |
|      - |  714 | ` *   Path to the file.` |
|      - |  715 | ` * Return` |
|      - |  716 | ` *  TRUE on success or FALSE on failure.` |
|      - |  717 | ` */` |
|     46 |  718 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  719 |  |
|      - |  720 | `	const char *zPath;` |
|      - |  721 | `	ph7_vfs *pVfs;` |
|      - |  722 | `	int rc;` |
|     47 |  723 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  724 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  725 | `		ph7_result_bool(pCtx,0);` |
|      3 |  726 | `		return PH7_OK;` |
|      - |  727 | `	}` |
|      - |  728 | `	/* Point to the underlying vfs */` |
|     45 |  729 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     45 |  730 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - |  731 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  732 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  733 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  734 | `			ph7_function_name(pCtx)` |
|      - |  735 | `			);` |
|    ! 0 |  736 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  737 | `		return PH7_OK;` |
|      - |  738 | `	}` |
|      - |  739 | `	/* Point to the desired directory */` |
|     45 |  740 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  741 | `	/* Perform the requested operation */` |
|     45 |  742 | `	rc = pVfs->xFileExists(zPath);` |
|      - |  743 | `	/* IO return value */` |
|     45 |  744 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     45 |  745 | `	return PH7_OK;` |
|     24 |  746 |  |
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
|      1 |  757 |  |
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
|     14 |  784 |  |
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
|      1 |  795 |  |
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
|      2 |  822 |  |
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
|      1 |  833 |  |
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
|      3 |  860 |  |
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
|      1 |  871 |  |
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
|      2 |  898 |  |
|      - |  899 | `/*` |
|      - |  900 | ` * bool is_file(string $filename)` |
|      - |  901 | ` *  Tells whether the filename is a regular file.` |
|      - |  902 | ` * Parameters` |
|      - |  903 | ` *  $filename` |
|      - |  904 | ` *   Path to the file.` |
|      - |  905 | ` * Return` |
|      - |  906 | ` *  TRUE on success or FALSE on failure.` |
|      - |  907 | ` */` |
|   4424 |  908 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  909 |  |
|      - |  910 | `	const char *zPath;` |
|      - |  911 | `	ph7_vfs *pVfs;` |
|      - |  912 | `	int rc;` |
|   4426 |  913 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  914 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  915 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  916 | `		return PH7_OK;` |
|      - |  917 | `	}` |
|      - |  918 | `	/* Point to the underlying vfs */` |
|   4426 |  919 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   4426 |  920 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - |  921 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  922 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  923 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  924 | `			ph7_function_name(pCtx)` |
|      - |  925 | `			);` |
|    ! 0 |  926 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  927 | `		return PH7_OK;` |
|      - |  928 | `	}` |
|      - |  929 | `	/* Point to the desired directory */` |
|   4426 |  930 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  931 | `	/* Perform the requested operation */` |
|   4426 |  932 | `	rc = pVfs->xIsfile(zPath);` |
|      - |  933 | `	/* IO return value */` |
|   4426 |  934 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   4426 |  935 | `	return PH7_OK;` |
|   2214 |  936 |  |
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
|    ! 0 |  947 |  |
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
|      2 |  974 |  |
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
|    ! 0 |  985 |  |
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
|      1 | 1012 |  |
|      - | 1013 | `/*` |
|      - | 1014 | ` * bool is_writable(string $filename)` |
|      - | 1015 | ` *  Tells whether the filename is writable.` |
|      - | 1016 | ` * Parameters` |
|      - | 1017 | ` *  $filename` |
|      - | 1018 | ` *   Path to the file.` |
|      - | 1019 | ` * Return` |
|      - | 1020 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1021 | ` */` |
|      8 | 1022 | `static int PH7_vfs_is_writable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1023 |  |
|      - | 1024 | `	const char *zPath;` |
|      - | 1025 | `	ph7_vfs *pVfs;` |
|      - | 1026 | `	int rc;` |
|      9 | 1027 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1028 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 1029 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1030 | `		return PH7_OK;` |
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
|      5 | 1050 |  |
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
|    ! 0 | 1061 |  |
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
|      1 | 1088 |  |
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
|      1 | 1100 |  |
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
|      3 | 1126 |  |
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
|      1 | 1152 |  |
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
|      3 | 1195 |  |
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
|    ! 0 | 1221 |  |
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
|      1 | 1264 |  |
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
|     34 | 1275 | `static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1276 |  |
|      - | 1277 | `	const char *zEnv;` |
|      - | 1278 | `	ph7_vfs *pVfs;` |
|      - | 1279 | `	int iLen;` |
|     36 | 1280 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1281 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1282 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1283 | `		return PH7_OK;` |
|      - | 1284 | `	}` |
|      - | 1285 | `	/* Point to the underlying vfs */` |
|     36 | 1286 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     36 | 1287 | `	if( pVfs == 0 \|\| pVfs->xGetenv == 0 ){` |
|      - | 1288 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1289 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1290 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1291 | `			ph7_function_name(pCtx)` |
|      - | 1292 | `			);` |
|    ! 0 | 1293 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1294 | `		return PH7_OK;` |
|      - | 1295 | `	}` |
|      - | 1296 | `	/* Extract the environment variable */` |
|     36 | 1297 | `	zEnv = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1298 | `	/* Set a boolean FALSE as the default return value */` |
|     36 | 1299 | `	ph7_result_bool(pCtx,0);` |
|     36 | 1300 | `	if( iLen < 1 ){` |
|      - | 1301 | `		/* Empty string */` |
|    ! 0 | 1302 | `		return PH7_OK;` |
|      - | 1303 | `	}` |
|      - | 1304 | `	/* Perform the requested operation */` |
|     36 | 1305 | `	pVfs->xGetenv(zEnv,pCtx);` |
|     36 | 1306 | `	return PH7_OK;` |
|     19 | 1307 |  |
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
|      1 | 1318 |  |
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
|      4 | 1375 |  |
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
|      1 | 1394 |  |
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
|      3 | 1430 |  |
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
|      2 | 1451 |  |
|      - | 1452 | `	const char *zPath,*zDir;` |
|      - | 1453 | `	int iLen,iDirlen;` |
|     16 | 1454 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1455 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 1456 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1457 | `		return PH7_OK;` |
|      - | 1458 | `	}` |
|      - | 1459 | `	/* Point to the target path */` |
|     16 | 1460 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     16 | 1461 | `	if( iLen < 1 ){` |
|      - | 1462 | `		/* Reuturn "." */` |
|      2 | 1463 | `		ph7_result_string(pCtx,".",sizeof(char));` |
|      2 | 1464 | `		return PH7_OK;` |
|      - | 1465 | `	}` |
|      - | 1466 | `	/* Perform the requested operation */` |
|     14 | 1467 | `	zDir = PH7_ExtractDirName(zPath,iLen,&iDirlen);` |
|      - | 1468 | `	/* Return directory name */` |
|     14 | 1469 | `	ph7_result_string(pCtx,zDir,iDirlen);` |
|     14 | 1470 | `	return PH7_OK;` |
|      9 | 1471 |  |
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
|     18 | 1485 | `static int PH7_builtin_basename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1486 |  |
|      - | 1487 | `	const char *zPath,*zBase,*zEnd;` |
|      - | 1488 | `	int c,d,iLen;` |
|     19 | 1489 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1490 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1491 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1492 | `		return PH7_OK;` |
|      - | 1493 | `	}` |
|     19 | 1494 | `	c = d = '/';` |
|      - | 1495 | `#ifdef __WINNT__` |
|      1 | 1496 | `	d = '\\';` |
|      - | 1497 | `#endif` |
|      - | 1498 | `	/* Point to the target path */` |
|     19 | 1499 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     19 | 1500 | `	if( iLen < 1 ){` |
|      - | 1501 | `		/* Empty string */` |
|      3 | 1502 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1503 | `		return PH7_OK;` |
|      - | 1504 | `	}` |
|      - | 1505 | `	/* Perform the requested operation */` |
|     17 | 1506 | `	zEnd = &zPath[iLen - 1];` |
|      - | 1507 | `	/* Ignore trailing '/' */` |
|     30 | 1508 | `	while( zEnd > zPath && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      6 | 1509 | `		zEnd--;` |
|      1 | 1510 | `	}` |
|     17 | 1511 | `	iLen = (int)(&zEnd[1]-zPath);` |
|    135 | 1512 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    111 | 1513 | `		zEnd--;` |
|      1 | 1514 | `	}` |
|     17 | 1515 | `	zBase = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|     17 | 1516 | `	zEnd = &zPath[iLen];` |
|     17 | 1517 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 1518 | `		const char *zSuffix;` |
|      - | 1519 | `		int nSuffix;` |
|      - | 1520 | `		/* Strip suffix */` |
|      5 | 1521 | `		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);` |
|      5 | 1522 | `		if( nSuffix > 0 && nSuffix < iLen && SyMemcmp(&zEnd[-nSuffix],zSuffix,nSuffix) == 0 ){` |
|      5 | 1523 | `			zEnd -= nSuffix;` |
|      2 | 1524 | `		}` |
|      2 | 1525 | `	}` |
|      - | 1526 | `	/* Store the basename */` |
|     17 | 1527 | `	ph7_result_string(pCtx,zBase,(int)(zEnd-zBase));` |
|     17 | 1528 | `	return PH7_OK;` |
|     10 | 1529 |  |
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
|      - | 1546 |  |
|      - | 1547 | `	SyString sDir; /* Directory [i.e: /var/www] */` |
|      - | 1548 | `	SyString sBasename; /* Basename [i.e httpd.conf] */` |
|      - | 1549 | `	SyString sExtension; /* File extension [i.e xml,pdf..] */` |
|      - | 1550 | `	SyString sFilename;  /* Filename */` |
|      - | 1551 | `};` |
|      - | 1552 | `/*` |
|      - | 1553 | ` * Extract path fields.` |
|      - | 1554 | ` */` |
|   8846 | 1555 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      2 | 1556 |  |
|   8848 | 1557 | `	const char *zPtr,*zEnd = &zPath[nByte - 1];` |
|      - | 1558 | `	SyString *pCur;` |
|      - | 1559 | `	int c,d;` |
|   8848 | 1560 | `	c = d = '/';` |
|      - | 1561 | `#ifdef __WINNT__` |
|      2 | 1562 | `	d = '\\';` |
|      - | 1563 | `#endif` |
|      - | 1564 | `	/* Zero the structure */` |
|   8848 | 1565 | `	SyZero(pOut,sizeof(path_info));` |
|      - | 1566 | `	/* Handle special case */` |
|   8848 | 1567 | `	if( nByte == sizeof(char) && ( (int)zPath[0] == c \|\| (int)zPath[0] == d ) ){` |
|      - | 1568 | `#ifdef __WINNT__` |
|    ! 0 | 1569 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1570 | `#else` |
|    ! 0 | 1571 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1572 | `#endif` |
|    ! 0 | 1573 | `		return SXRET_OK;` |
|      - | 1574 | `	}` |
|      - | 1575 | `	/* Extract the basename */` |
| 226579 | 1576 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
| 213310 | 1577 | `		zEnd--;` |
|      2 | 1578 | `	}` |
|   8848 | 1579 | `	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|   8848 | 1580 | `	zEnd = &zPath[nByte];` |
|      - | 1581 | `	/* dirname */` |
|   8848 | 1582 | `	pCur = &pOut->sDir;` |
|   8848 | 1583 | `	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);` |
|   8848 | 1584 | `	if( pCur->nByte > 1 ){` |
|  17694 | 1585 | `		SyStringTrimTrailingChar(pCur,'/');` |
|      - | 1586 | `#ifdef __WINNT__` |
|      2 | 1587 | `		SyStringTrimTrailingChar(pCur,'\\');` |
|      - | 1588 | `#endif` |
|   4425 | 1589 | `	}else if( (int)zPath[0] == c \|\| (int)zPath[0] == d ){` |
|      - | 1590 | `#ifdef __WINNT__` |
|    ! 0 | 1591 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1592 | `#else` |
|    ! 0 | 1593 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1594 | `#endif` |
|    ! 0 | 1595 | `	}` |
|      - | 1596 | `	/* basename/filename */` |
|   8848 | 1597 | `	pCur = &pOut->sBasename;` |
|   8848 | 1598 | `	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);` |
|   8848 | 1599 | `	SyStringTrimLeadingChar(pCur,'/');` |
|      - | 1600 | `#ifdef __WINNT__` |
|      2 | 1601 | `	SyStringTrimLeadingChar(pCur,'\\');` |
|      - | 1602 | `#endif` |
|   8848 | 1603 | `	SyStringDupPtr(&pOut->sFilename,pCur);` |
|   8848 | 1604 | `	if( pCur->nByte > 0 ){` |
|      - | 1605 | `		/* extension */` |
|   8848 | 1606 | `		zEnd--;` |
|  44230 | 1607 | `		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){` |
|  35384 | 1608 | `			zEnd--;` |
|      2 | 1609 | `		}` |
|   8848 | 1610 | `		if( zEnd > pCur->zString ){` |
|   8846 | 1611 | `			zEnd++; /* Jump leading dot */` |
|   8846 | 1612 | `			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);` |
|      - | 1613 | `			/* Fix filename */` |
|   8846 | 1614 | `			pCur = &pOut->sFilename;` |
|   8846 | 1615 | `			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){` |
|   8846 | 1616 | `				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);` |
|   4422 | 1617 | `			}` |
|   4422 | 1618 | `		}` |
|   4423 | 1619 | `	}` |
|   8848 | 1620 | `	return SXRET_OK;` |
|   4425 | 1621 |  |
|      - | 1622 | `/*` |
|      - | 1623 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1624 | ` *  See block comment above.` |
|      - | 1625 | ` */` |
|   8846 | 1626 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1627 |  |
|      - | 1628 | `	const char *zPath;` |
|      - | 1629 | `	path_info sInfo;` |
|      - | 1630 | `	SyString *pComp;` |
|      - | 1631 | `	int iLen;` |
|   8848 | 1632 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1633 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1634 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1635 | `		return PH7_OK;` |
|      - | 1636 | `	}` |
|      - | 1637 | `	/* Point to the target path */` |
|   8848 | 1638 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|   8848 | 1639 | `	if( iLen < 1 ){` |
|      - | 1640 | `		/* Empty string */` |
|    ! 0 | 1641 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1642 | `		return PH7_OK;` |
|      - | 1643 | `	}` |
|      - | 1644 | `	/* Extract path info */` |
|   8848 | 1645 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|  13270 | 1646 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1647 | `		/* Return path component */` |
|   8846 | 1648 | `		int nComp = ph7_value_to_int(apArg[1]);` |
|   8846 | 1649 | `		switch(nComp){` |
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
|   2211 | 1668 | `		case 3: /*PATHINFO_EXTENSION*/` |
|   4424 | 1669 | `			pComp = &sInfo.sExtension;` |
|   4424 | 1670 | `			if( pComp->nByte > 0 ){` |
|   4422 | 1671 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   2212 | 1672 | `			}else{` |
|      - | 1673 | `				/* Expand the empty string */` |
|      3 | 1674 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1675 | `			}` |
|   4424 | 1676 | `			break;` |
|   2209 | 1677 | `		case 4: /*PATHINFO_FILENAME*/` |
|   4420 | 1678 | `			pComp = &sInfo.sFilename;` |
|   4420 | 1679 | `			if( pComp->nByte > 0 ){` |
|   4420 | 1680 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   2211 | 1681 | `			}else{` |
|      - | 1682 | `				/* Expand the empty string */` |
|    ! 0 | 1683 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1684 | `			}` |
|   4420 | 1685 | `			break;` |
|    ! 0 | 1686 | `		default:` |
|      - | 1687 | `			/* Expand the empty string */` |
|    ! 0 | 1688 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1689 | `			break;` |
|      - | 1690 | `		}` |
|   4424 | 1691 | `	}else{` |
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
|   8848 | 1741 | `	return PH7_OK;` |
|   4425 | 1742 |  |
|      - | 1743 | `/*` |
|      - | 1744 | ` * Globbing implementation extracted from the sqlite3 source tree.` |
|      - | 1745 |  |
|      - | 1746 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|      - | 1747 | ` * Status: Public Domain` |
|      - | 1748 | ` */` |
|      - | 1749 | `typedef unsigned char u8;` |
|      - | 1750 | `/* An array to map all upper-case characters into their corresponding` |
|      - | 1751 | `** lower-case character.` |
|      - | 1752 | `**` |
|      - | 1753 | `** SQLite only considers US-ASCII (or EBCDIC) characters.  We do not` |
|      - | 1754 | `** handle case conversions for the UTF character set since the tables` |
|      - | 1755 | `** involved are nearly as big or bigger than SQLite itself.` |
|      - | 1756 | `*/` |
|      - | 1757 | `static const unsigned char sqlite3UpperToLower[] = {` |
|      - | 1758 |  |
|      - | 1759 | `     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,` |
|      - | 1760 | `     36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,` |
|      - | 1761 | `     54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,` |
|      - | 1762 | `    104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,` |
|      - | 1763 | `    122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,` |
|      - | 1764 | `    108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,` |
|      - | 1765 | `    126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,` |
|      - | 1766 | `    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,` |
|      - | 1767 | `    162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,` |
|      - | 1768 | `    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,` |
|      - | 1769 | `    198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,` |
|      - | 1770 | `    216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,` |
|      - | 1771 | `    234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,` |
|      - | 1772 | `    252,253,254,255` |
|      - | 1773 | `};` |
|      - | 1774 | `#define GlogUpperToLower(A)     if( A<0x80 ){ A = sqlite3UpperToLower[A]; }` |
|      - | 1775 | `/*` |
|      - | 1776 | `** Assuming zIn points to the first byte of a UTF-8 character,` |
|      - | 1777 | `** advance zIn to point to the first byte of the next UTF-8 character.` |
|      - | 1778 | `*/` |
|      - | 1779 | `#define SQLITE_SKIP_UTF8(zIn) {                        \` |
|      - | 1780 | `  if( (*(zIn++))>=0xc0 ){                              \` |
|      - | 1781 | `    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \` |
|      - | 1782 | `  }                                                    \` |
|      - | 1783 |  |
|      - | 1784 | `/*` |
|      - | 1785 | `** Compare two UTF-8 strings for equality where the first string can` |
|      - | 1786 | `** potentially be a "glob" expression.  Return true (1) if they` |
|      - | 1787 | `** are the same and false (0) if they are different.` |
|      - | 1788 | `**` |
|      - | 1789 | `** Globbing rules:` |
|      - | 1790 | `**` |
|      - | 1791 | `**      '*'       Matches any sequence of zero or more characters.` |
|      - | 1792 | `**` |
|      - | 1793 | `**      '?'       Matches exactly one character.` |
|      - | 1794 | `**` |
|      - | 1795 | `**     [...]      Matches one character from the enclosed list of` |
|      - | 1796 | `**                characters.` |
|      - | 1797 | `**` |
|      - | 1798 | `**     [^...]     Matches one character not in the enclosed list.` |
|      - | 1799 | `**` |
|      - | 1800 | `** With the [...] and [^...] matching, a ']' character can be included` |
|      - | 1801 | `** in the list by making it the first character after '[' or '^'.  A` |
|      - | 1802 | `** range of characters can be specified using '-'.  Example:` |
|      - | 1803 | `** "[a-z]" matches any single lower-case letter.  To match a '-', make` |
|      - | 1804 | `** it the last character in the list.` |
|      - | 1805 | `**` |
|      - | 1806 | `** This routine is usually quick, but can be N**2 in the worst case.` |
|      - | 1807 | `**` |
|      - | 1808 | `** Hints: to match '*' or '?', put them in "[]".  Like this:` |
|      - | 1809 | `**` |
|      - | 1810 | `**         abc[*]xyz        Matches "abc*xyz" only` |
|      - | 1811 | `*/` |
|     20 | 1812 | `static int patternCompare(` |
|      - | 1813 | `  const u8 *zPattern,              /* The glob pattern */` |
|      - | 1814 | `  const u8 *zString,               /* The string to compare against the glob */` |
|      - | 1815 | `  const int esc,                    /* The escape character */` |
|      - | 1816 | `  int noCase` |
|      1 | 1817 | `){` |
|      - | 1818 | `  int c, c2;` |
|      - | 1819 | `  int invert;` |
|      - | 1820 | `  int seen;` |
|     21 | 1821 | `  u8 matchOne = '?';` |
|     21 | 1822 | `  u8 matchAll = '*';` |
|     21 | 1823 | `  u8 matchSet = '[';` |
|     21 | 1824 | `  int prevEscape = 0;     /* True if the previous character was 'escape' */` |
|      - | 1825 |  |
|     21 | 1826 | `  if( !zPattern \|\| !zString ) return 0;` |
|     51 | 1827 | `  while( (c = PH7_Utf8Read(zPattern,0,&zPattern))!=0 ){` |
|     43 | 1828 | `    if( !prevEscape && c==matchAll ){` |
|     16 | 1829 | `      while( (c=PH7_Utf8Read(zPattern,0,&zPattern)) == matchAll` |
|      9 | 1830 | `               \|\| c == matchOne ){` |
|    ! 0 | 1831 | `        if( c==matchOne && PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1832 | `          return 0;` |
|      - | 1833 | `        }` |
|    ! 0 | 1834 | `      }` |
|      9 | 1835 | `      if( c==0 ){` |
|    ! 0 | 1836 | `        return 1;` |
|      9 | 1837 | `      }else if( c==esc ){` |
|    ! 0 | 1838 | `        c = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1839 | `        if( c==0 ){` |
|    ! 0 | 1840 | `          return 0;` |
|    ! 0 | 1841 | `        }` |
|      9 | 1842 | `      }else if( c==matchSet ){` |
|    ! 0 | 1843 | `	  if( (esc==0) \|\| (matchSet<0x80) ) return 0;` |
|    ! 0 | 1844 | `	  while( *zString && patternCompare(&zPattern[-1],zString,esc,noCase)==0 ){` |
|    ! 0 | 1845 | `          SQLITE_SKIP_UTF8(zString);` |
|    ! 0 | 1846 | `        }` |
|    ! 0 | 1847 | `        return *zString!=0;` |
|      - | 1848 | `      }` |
|     11 | 1849 | `      while( (c2 = PH7_Utf8Read(zString,0,&zString))!=0 ){` |
|     11 | 1850 | `        if( noCase ){` |
|      3 | 1851 | `          GlogUpperToLower(c2);` |
|      3 | 1852 | `          GlogUpperToLower(c);` |
|     11 | 1853 | `          while( c2 != 0 && c2 != c ){` |
|      9 | 1854 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      9 | 1855 | `            GlogUpperToLower(c2);` |
|      1 | 1856 | `          }` |
|      2 | 1857 | `        }else{` |
|     47 | 1858 | `          while( c2 != 0 && c2 != c ){` |
|     39 | 1859 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      1 | 1860 | `          }` |
|      - | 1861 | `        }` |
|     11 | 1862 | `        if( c2==0 ) return 0;` |
|      9 | 1863 | `		if( patternCompare(zPattern,zString,esc,noCase) ) return 1;` |
|      1 | 1864 | `      }` |
|    ! 0 | 1865 | `      return 0;` |
|     35 | 1866 | `    }else if( !prevEscape && c==matchOne ){` |
|    ! 0 | 1867 | `      if( PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1868 | `        return 0;` |
|    ! 0 | 1869 | `      }` |
|     35 | 1870 | `    }else if( c==matchSet ){` |
|    ! 0 | 1871 | `      int prior_c = 0;` |
|    ! 0 | 1872 | `      if( esc == 0 ) return 0;` |
|    ! 0 | 1873 | `      seen = 0;` |
|    ! 0 | 1874 | `      invert = 0;` |
|    ! 0 | 1875 | `      c = PH7_Utf8Read(zString, 0, &zString);` |
|    ! 0 | 1876 | `      if( c==0 ) return 0;` |
|    ! 0 | 1877 | `      c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1878 | `      if( c2=='^' ){` |
|    ! 0 | 1879 | `        invert = 1;` |
|    ! 0 | 1880 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1881 | `      }` |
|    ! 0 | 1882 | `      if( c2==']' ){` |
|    ! 0 | 1883 | `        if( c==']' ) seen = 1;` |
|    ! 0 | 1884 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1885 | `      }` |
|    ! 0 | 1886 | `      while( c2 && c2!=']' ){` |
|    ! 0 | 1887 | `        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){` |
|    ! 0 | 1888 | `          c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1889 | `          if( c>=prior_c && c<=c2 ) seen = 1;` |
|    ! 0 | 1890 | `          prior_c = 0;` |
|    ! 0 | 1891 | `        }else{` |
|    ! 0 | 1892 | `          if( c==c2 ){` |
|    ! 0 | 1893 | `            seen = 1;` |
|    ! 0 | 1894 | `          }` |
|    ! 0 | 1895 | `          prior_c = c2;` |
|      - | 1896 | `        }` |
|    ! 0 | 1897 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1898 | `      }` |
|    ! 0 | 1899 | `      if( c2==0 \|\| (seen ^ invert)==0 ){` |
|    ! 0 | 1900 | `        return 0;` |
|    ! 0 | 1901 | `      }` |
|     35 | 1902 | `    }else if( esc==c && !prevEscape ){` |
|    ! 0 | 1903 | `      prevEscape = 1;` |
|    ! 0 | 1904 | `    }else{` |
|     35 | 1905 | `      c2 = PH7_Utf8Read(zString, 0, &zString);` |
|     35 | 1906 | `      if( noCase ){` |
|      7 | 1907 | `        GlogUpperToLower(c);` |
|      7 | 1908 | `        GlogUpperToLower(c2);` |
|      3 | 1909 | `      }` |
|     35 | 1910 | `      if( c!=c2 ){` |
|      5 | 1911 | `        return 0;` |
|      - | 1912 | `      }` |
|     31 | 1913 | `      prevEscape = 0;` |
|      - | 1914 | `    }` |
|      1 | 1915 | `  }` |
|      9 | 1916 | `  return *zString==0;` |
|     11 | 1917 |  |
|      - | 1918 | `/*` |
|      - | 1919 | ` * Wrapper around patternCompare() defined above.` |
|      - | 1920 | ` * See block comment above for more information.` |
|      - | 1921 | ` */` |
|     12 | 1922 | `static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,int CaseCompare)` |
|      1 | 1923 |  |
|      - | 1924 | `	int rc;` |
|     13 | 1925 | `	if( iEsc < 0 ){` |
|    ! 0 | 1926 | `		iEsc = '\\';` |
|    ! 0 | 1927 | `	}` |
|     13 | 1928 | `	rc = patternCompare(zPattern,zString,iEsc,CaseCompare);` |
|     13 | 1929 | `	return rc;` |
|      1 | 1930 |  |
|      - | 1931 | `/*` |
|      - | 1932 | ` * bool fnmatch(string $pattern,string $string[,int $flags = 0 ])` |
|      - | 1933 | ` *  Match filename against a pattern.` |
|      - | 1934 | ` * Parameters` |
|      - | 1935 | ` *  $pattern` |
|      - | 1936 | ` *   The shell wildcard pattern.` |
|      - | 1937 | ` * $string` |
|      - | 1938 | ` *  The tested string.` |
|      - | 1939 | ` * $flags` |
|      - | 1940 | ` *   A list of possible flags:` |
|      - | 1941 | ` *    FNM_NOESCAPE 	Disable backslash escaping.` |
|      - | 1942 | ` *    FNM_PATHNAME 	Slash in string only matches slash in the given pattern.` |
|      - | 1943 | ` *    FNM_PERIOD 	Leading period in string must be exactly matched by period in the given pattern.` |
|      - | 1944 | ` *    FNM_CASEFOLD 	Caseless match.` |
|      - | 1945 | ` * Return` |
|      - | 1946 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 1947 | ` */` |
|      8 | 1948 | `static int PH7_builtin_fnmatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1949 |  |
|      - | 1950 | `	const char *zString,*zPattern;` |
|      9 | 1951 | `	int iEsc = '\\';` |
|      9 | 1952 | `	int noCase = 0;` |
|      - | 1953 | `	int rc;` |
|      9 | 1954 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 1955 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1956 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1957 | `		return PH7_OK;` |
|      - | 1958 | `	}` |
|      - | 1959 | `	/* Extract the pattern and the string */` |
|      9 | 1960 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|      9 | 1961 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 1962 | `	/* Extract the flags if avaialble */` |
|      9 | 1963 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      7 | 1964 | `		rc = ph7_value_to_int(apArg[2]);` |
|      7 | 1965 | `		if( rc & 0x01 /*FNM_NOESCAPE*/){` |
|    ! 0 | 1966 | `			iEsc = 0;` |
|    ! 0 | 1967 | `		}` |
|      7 | 1968 | `		if( rc & 0x08 /*FNM_CASEFOLD*/){` |
|      3 | 1969 | `			noCase = 1;` |
|      1 | 1970 | `		}` |
|      3 | 1971 | `	}` |
|      - | 1972 | `	/* Go globbing */` |
|      9 | 1973 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,noCase);` |
|      - | 1974 | `	/* Globbing result */` |
|      9 | 1975 | `	ph7_result_bool(pCtx,rc);` |
|      9 | 1976 | `	return PH7_OK;` |
|      5 | 1977 |  |
|      - | 1978 | `/*` |
|      - | 1979 | ` * bool strglob(string $pattern,string $string)` |
|      - | 1980 | ` *  Match string against a pattern.` |
|      - | 1981 | ` * Parameters` |
|      - | 1982 | ` *  $pattern` |
|      - | 1983 | ` *   The shell wildcard pattern.` |
|      - | 1984 | ` * $string` |
|      - | 1985 | ` *  The tested string.` |
|      - | 1986 | ` * Return` |
|      - | 1987 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 1988 | ` * Note that this a symisc eXtension.` |
|      - | 1989 | ` */` |
|      4 | 1990 | `static int PH7_builtin_strglob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1991 |  |
|      - | 1992 | `	const char *zString,*zPattern;` |
|      5 | 1993 | `	int iEsc = '\\';` |
|      - | 1994 | `	int rc;` |
|      5 | 1995 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 1996 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1997 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1998 | `		return PH7_OK;` |
|      - | 1999 | `	}` |
|      - | 2000 | `	/* Extract the pattern and the string */` |
|      5 | 2001 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|      5 | 2002 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2003 | `	/* Go globbing */` |
|      5 | 2004 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0);` |
|      - | 2005 | `	/* Globbing result */` |
|      5 | 2006 | `	ph7_result_bool(pCtx,rc);` |
|      5 | 2007 | `	return PH7_OK;` |
|      3 | 2008 |  |
|      - | 2009 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2010 | `/*` |
|      - | 2011 | ` * bool link(string $target,string $link)` |
|      - | 2012 |  |
|      - | 2013 | ` *  Create a hard link.` |
|      - | 2014 | ` * Parameters` |
|      - | 2015 | ` *  $target` |
|      - | 2016 | ` *   Target of the link.` |
|      - | 2017 | ` *  $link` |
|      - | 2018 | ` *   The link name.` |
|      - | 2019 | ` * Return` |
|      - | 2020 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2021 | ` */` |
|      2 | 2022 | `static int PH7_vfs_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2023 |  |
|      - | 2024 | `	const char *zTarget,*zLink;` |
|      - | 2025 | `	ph7_vfs *pVfs;` |
|      - | 2026 | `	int rc;` |
|      2 | 2027 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2028 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2029 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2030 | `		return PH7_OK;` |
|      - | 2031 | `	}` |
|      - | 2032 | `	/* Point to the underlying vfs */` |
|      2 | 2033 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 2034 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2035 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2036 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2037 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2038 | `			ph7_function_name(pCtx)` |
|      - | 2039 | `			);` |
|    ! 0 | 2040 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2041 | `		return PH7_OK;` |
|      - | 2042 | `	}` |
|      - | 2043 | `	/* Extract the given arguments */` |
|      2 | 2044 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      2 | 2045 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2046 | `	/* Perform the requested operation */` |
|      2 | 2047 | `	rc = pVfs->xLink(zTarget,zLink,0/*Not a symbolic link */);` |
|      - | 2048 | `	/* IO result */` |
|      2 | 2049 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      2 | 2050 | `	return PH7_OK;` |
|      1 | 2051 |  |
|      - | 2052 | `/*` |
|      - | 2053 | ` * bool symlink(string $target,string $link)` |
|      - | 2054 | ` *  Creates a symbolic link.` |
|      - | 2055 | ` * Parameters` |
|      - | 2056 | ` *  $target` |
|      - | 2057 | ` *   Target of the link.` |
|      - | 2058 | ` *  $link` |
|      - | 2059 | ` *   The link name.` |
|      - | 2060 | ` * Return` |
|      - | 2061 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2062 | ` */` |
|      6 | 2063 | `static int PH7_vfs_symlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2064 |  |
|      - | 2065 | `	const char *zTarget,*zLink;` |
|      - | 2066 | `	ph7_vfs *pVfs;` |
|      - | 2067 | `	int rc;` |
|      6 | 2068 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2069 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2070 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2071 | `		return PH7_OK;` |
|      - | 2072 | `	}` |
|      - | 2073 | `	/* Point to the underlying vfs */` |
|      6 | 2074 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      6 | 2075 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2076 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2077 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2078 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2079 | `			ph7_function_name(pCtx)` |
|      - | 2080 | `			);` |
|    ! 0 | 2081 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2082 | `		return PH7_OK;` |
|      - | 2083 | `	}` |
|      - | 2084 | `	/* Extract the given arguments */` |
|      6 | 2085 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      6 | 2086 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2087 | `	/* Perform the requested operation */` |
|      6 | 2088 | `	rc = pVfs->xLink(zTarget,zLink,1/*A symbolic link */);` |
|      - | 2089 | `	/* IO result */` |
|      6 | 2090 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      6 | 2091 | `	return PH7_OK;` |
|      3 | 2092 |  |
|      - | 2093 | `/*` |
|      - | 2094 | ` * int umask([ int $mask ])` |
|      - | 2095 | ` *  Changes the current umask.` |
|      - | 2096 | ` * Parameters` |
|      - | 2097 | ` *  $mask` |
|      - | 2098 | ` *   The new umask.` |
|      - | 2099 | ` * Return` |
|      - | 2100 | ` *  umask() without arguments simply returns the current umask.` |
|      - | 2101 | ` *  Otherwise the old umask is returned.` |
|      - | 2102 | ` */` |
|      8 | 2103 | `static int PH7_vfs_umask(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2104 |  |
|      - | 2105 | `	int iOld,iNew;` |
|      - | 2106 | `	ph7_vfs *pVfs;` |
|      - | 2107 | `	/* Point to the underlying vfs */` |
|      8 | 2108 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      8 | 2109 | `	if( pVfs == 0 \|\| pVfs->xUmask == 0 ){` |
|      - | 2110 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2111 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2112 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2113 | `			ph7_function_name(pCtx)` |
|      - | 2114 | `			);` |
|    ! 0 | 2115 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2116 | `		return PH7_OK;` |
|      - | 2117 | `	}` |
|      8 | 2118 | `	iNew = 0;` |
|      8 | 2119 | `	if( nArg > 0 ){` |
|      4 | 2120 | `		iNew = ph7_value_to_int(apArg[0]);` |
|      2 | 2121 | `	}` |
|      - | 2122 | `	/* Perform the requested operation */` |
|      8 | 2123 | `	iOld = pVfs->xUmask(iNew);` |
|      - | 2124 | `	/* Old mask */` |
|      8 | 2125 | `	ph7_result_int(pCtx,iOld);` |
|      8 | 2126 | `	return PH7_OK;` |
|      4 | 2127 |  |
|      - | 2128 | `/*` |
|      - | 2129 | ` * string sys_get_temp_dir()` |
|      - | 2130 | ` *  Returns directory path used for temporary files.` |
|      - | 2131 | ` * Parameters` |
|      - | 2132 | ` *  None` |
|      - | 2133 | ` * Return` |
|      - | 2134 | ` *  Returns the path of the temporary directory.` |
|      - | 2135 | ` */` |
|    184 | 2136 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2137 |  |
|      - | 2138 | `	ph7_vfs *pVfs;` |
|      - | 2139 | `	/* Set the empty string as the default return value */` |
|    186 | 2140 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2141 | `	/* Point to the underlying vfs */` |
|    186 | 2142 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    186 | 2143 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
|    ! 0 | 2144 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2145 | `		SXUNUSED(apArg);` |
|      - | 2146 | `		/* IO routine not implemented,return "" */` |
|    ! 0 | 2147 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2148 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2149 | `			ph7_function_name(pCtx)` |
|      - | 2150 | `			);` |
|    ! 0 | 2151 | `		return PH7_OK;` |
|      - | 2152 | `	}` |
|      - | 2153 | `	/* Perform the requested operation */` |
|    186 | 2154 | `	pVfs->xTempDir(pCtx);` |
|    186 | 2155 | `	return PH7_OK;` |
|     94 | 2156 |  |
|      - | 2157 | `/*` |
|      - | 2158 | ` * string get_current_user()` |
|      - | 2159 | ` *  Returns the name of the current working user.` |
|      - | 2160 | ` * Parameters` |
|      - | 2161 | ` *  None` |
|      - | 2162 | ` * Return` |
|      - | 2163 | ` *  Returns the name of the current working user.` |
|      - | 2164 | ` */` |
|      2 | 2165 | `static int PH7_vfs_get_current_user(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2166 |  |
|      - | 2167 | `	ph7_vfs *pVfs;` |
|      - | 2168 | `	/* Point to the underlying vfs */` |
|      3 | 2169 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2170 | `	if( pVfs == 0 \|\| pVfs->xUsername == 0 ){` |
|    ! 0 | 2171 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2172 | `		SXUNUSED(apArg);` |
|      - | 2173 | `		/* IO routine not implemented */` |
|    ! 0 | 2174 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2175 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2176 | `			ph7_function_name(pCtx)` |
|      - | 2177 | `			);` |
|      - | 2178 | `		/* Set a dummy username */` |
|    ! 0 | 2179 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 2180 | `		return PH7_OK;` |
|      - | 2181 | `	}` |
|      - | 2182 | `	/* Perform the requested operation */` |
|      3 | 2183 | `	pVfs->xUsername(pCtx);` |
|      3 | 2184 | `	return PH7_OK;` |
|      2 | 2185 |  |
|      - | 2186 | `/*` |
|      - | 2187 | ` * int64 getmypid()` |
|      - | 2188 | ` *  Gets process ID.` |
|      - | 2189 | ` * Parameters` |
|      - | 2190 | ` *  None` |
|      - | 2191 | ` * Return` |
|      - | 2192 | ` *  Returns the process ID.` |
|      - | 2193 | ` */` |
|     40 | 2194 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2195 |  |
|      - | 2196 | `	ph7_int64 nProcessId;` |
|      - | 2197 | `	ph7_vfs *pVfs;` |
|      - | 2198 | `	/* Point to the underlying vfs */` |
|     41 | 2199 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     41 | 2200 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
|    ! 0 | 2201 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2202 | `		SXUNUSED(apArg);` |
|      - | 2203 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2204 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2205 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2206 | `			ph7_function_name(pCtx)` |
|      - | 2207 | `			);` |
|    ! 0 | 2208 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2209 | `		return PH7_OK;` |
|      - | 2210 | `	}` |
|      - | 2211 | `	/* Perform the requested operation */` |
|     41 | 2212 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 2213 | `	/* Set the result */` |
|     41 | 2214 | `	ph7_result_int64(pCtx,nProcessId);` |
|     41 | 2215 | `	return PH7_OK;` |
|     21 | 2216 |  |
|      - | 2217 | `/*` |
|      - | 2218 | ` * int getmyuid()` |
|      - | 2219 | ` *  Get user ID.` |
|      - | 2220 | ` * Parameters` |
|      - | 2221 | ` *  None` |
|      - | 2222 | ` * Return` |
|      - | 2223 | ` *  Returns the user ID.` |
|      - | 2224 | ` */` |
|      2 | 2225 | `static int PH7_vfs_getmyuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2226 |  |
|      - | 2227 | `	ph7_vfs *pVfs;` |
|      - | 2228 | `	int nUid;` |
|      - | 2229 | `	/* Point to the underlying vfs */` |
|      2 | 2230 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 2231 | `	if( pVfs == 0 \|\| pVfs->xUid == 0 ){` |
|    ! 0 | 2232 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2233 | `		SXUNUSED(apArg);` |
|      - | 2234 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2235 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2236 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2237 | `			ph7_function_name(pCtx)` |
|      - | 2238 | `			);` |
|    ! 0 | 2239 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2240 | `		return PH7_OK;` |
|      - | 2241 | `	}` |
|      - | 2242 | `	/* Perform the requested operation */` |
|      2 | 2243 | `	nUid = pVfs->xUid();` |
|      - | 2244 | `	/* Set the result */` |
|      2 | 2245 | `	ph7_result_int(pCtx,nUid);` |
|      2 | 2246 | `	return PH7_OK;` |
|      1 | 2247 |  |
|      - | 2248 | `/*` |
|      - | 2249 | ` * int getmygid()` |
|      - | 2250 | ` *  Get group ID.` |
|      - | 2251 | ` * Parameters` |
|      - | 2252 | ` *  None` |
|      - | 2253 | ` * Return` |
|      - | 2254 | ` *  Returns the group ID.` |
|      - | 2255 | ` */` |
|      2 | 2256 | `static int PH7_vfs_getmygid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2257 |  |
|      - | 2258 | `	ph7_vfs *pVfs;` |
|      - | 2259 | `	int nGid;` |
|      - | 2260 | `	/* Point to the underlying vfs */` |
|      2 | 2261 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 2262 | `	if( pVfs == 0 \|\| pVfs->xGid == 0 ){` |
|    ! 0 | 2263 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2264 | `		SXUNUSED(apArg);` |
|      - | 2265 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2266 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2267 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2268 | `			ph7_function_name(pCtx)` |
|      - | 2269 | `			);` |
|    ! 0 | 2270 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2271 | `		return PH7_OK;` |
|      - | 2272 | `	}` |
|      - | 2273 | `	/* Perform the requested operation */` |
|      2 | 2274 | `	nGid = pVfs->xGid();` |
|      - | 2275 | `	/* Set the result */` |
|      2 | 2276 | `	ph7_result_int(pCtx,nGid);` |
|      2 | 2277 | `	return PH7_OK;` |
|      1 | 2278 |  |
|      - | 2279 | `#ifdef __WINNT__` |
|      - | 2280 | `#include <Windows.h>` |
|      - | 2281 | `#elif defined(__UNIXES__)` |
|      - | 2282 | `#include <sys/utsname.h>` |
|      - | 2283 | `#endif` |
|      - | 2284 | `/*` |
|      - | 2285 | ` * string php_uname([ string $mode = "a" ])` |
|      - | 2286 | ` *  Returns information about the host operating system.` |
|      - | 2287 | ` * Parameters` |
|      - | 2288 | ` *  $mode` |
|      - | 2289 | ` *   mode is a single character that defines what information is returned:` |
|      - | 2290 | ` *    'a': This is the default. Contains all modes in the sequence "s n r v m".` |
|      - | 2291 | ` *    's': Operating system name. eg. FreeBSD.` |
|      - | 2292 | ` *    'n': Host name. eg. localhost.example.com.` |
|      - | 2293 | ` *    'r': Release name. eg. 5.1.2-RELEASE.` |
|      - | 2294 | ` *    'v': Version information. Varies a lot between operating systems.` |
|      - | 2295 | ` *    'm': Machine type. eg. i386.` |
|      - | 2296 | ` * Return` |
|      - | 2297 | ` *  OS description as a string.` |
|      - | 2298 | ` */` |
|      4 | 2299 | `static int PH7_vfs_ph7_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2300 |  |
|      - | 2301 | `#if defined(__WINNT__)` |
|      1 | 2302 | `	const char *zName = "Microsoft Windows";` |
|      - | 2303 | `	OSVERSIONINFOW sVer;` |
|      - | 2304 | `#elif defined(__UNIXES__)` |
|      - | 2305 | `	struct utsname sName;` |
|      - | 2306 | `#endif` |
|      5 | 2307 | `	const char *zMode = "a";` |
|      5 | 2308 | `	if( nArg > 0 && ph7_value_is_string(apArg[0]) ){` |
|      - | 2309 | `		/* Extract the desired mode */` |
|    ! 0 | 2310 | `		zMode = ph7_value_to_string(apArg[0],0);` |
|    ! 0 | 2311 | `	}` |
|      - | 2312 | `#if defined(__WINNT__)` |
|      1 | 2313 | `	sVer.dwOSVersionInfoSize = sizeof(sVer);` |
|      - | 2314 | `	/* GetVersionExW is deprecated in modern MSVC. Suppress deprecation for this call. */` |
|      - | 2315 | `#if defined(_MSC_VER)` |
|      - | 2316 | `#pragma warning(push)` |
|      - | 2317 | `#pragma warning(disable:4996)` |
|      - | 2318 | `#endif` |
|      1 | 2319 | `	if( TRUE != GetVersionExW(&sVer)){` |
|      - | 2320 | `#if defined(_MSC_VER)` |
|      - | 2321 | `#pragma warning(pop)` |
|      - | 2322 | `#endif` |
|    ! 0 | 2323 | `		ph7_result_string(pCtx,zName,-1);` |
|    ! 0 | 2324 | `		return PH7_OK;` |
|      - | 2325 | `	}` |
|      1 | 2326 | `	if( sVer.dwPlatformId == VER_PLATFORM_WIN32_NT ){` |
|      1 | 2327 | `		if( sVer.dwMajorVersion <= 4 ){` |
|    ! 0 | 2328 | `			zName = "Microsoft Windows NT";` |
|      1 | 2329 | `		}else if( sVer.dwMajorVersion == 5 ){` |
|    ! 0 | 2330 | `			switch(sVer.dwMinorVersion){` |
|    ! 0 | 2331 | `				case 0:	zName = "Microsoft Windows 2000"; break;` |
|    ! 0 | 2332 | `				case 1: zName = "Microsoft Windows XP";   break;` |
|    ! 0 | 2333 | `				case 2: zName = "Microsoft Windows Server 2003"; break;` |
|      - | 2334 | `			}` |
|    ! 0 | 2335 | `		}else if( sVer.dwMajorVersion == 6){` |
|      1 | 2336 | `				switch(sVer.dwMinorVersion){` |
|    ! 0 | 2337 | `					case 0: zName = "Microsoft Windows Vista"; break;` |
|    ! 0 | 2338 | `					case 1: zName = "Microsoft Windows 7"; break;` |
|      1 | 2339 | `					case 2: zName = "Microsoft Windows Server 2008"; break;` |
|    ! 0 | 2340 | `					case 3: zName = "Microsoft Windows 8"; break;` |
|      - | 2341 | `					default: break;` |
|      - | 2342 | `				}` |
|      - | 2343 | `		}` |
|      - | 2344 | `	}` |
|      1 | 2345 | `	switch(zMode[0]){` |
|      - | 2346 | `	case 's':` |
|      - | 2347 | `		/* Operating system name */` |
|    ! 0 | 2348 | `		ph7_result_string(pCtx,zName,-1/* Compute length automatically*/);` |
|    ! 0 | 2349 | `		break;` |
|      - | 2350 | `	case 'n':` |
|      - | 2351 | `		/* Host name */` |
|    ! 0 | 2352 | `		ph7_result_string(pCtx,"localhost",(int)sizeof("localhost")-1);` |
|    ! 0 | 2353 | `		break;` |
|      - | 2354 | `	case 'r':` |
|      - | 2355 | `	case 'v':` |
|      - | 2356 | `		/* Version information. */` |
|    ! 0 | 2357 | `		ph7_result_string_format(pCtx,"%u.%u build %u",` |
|      - | 2358 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2359 | `			);` |
|    ! 0 | 2360 | `		break;` |
|      - | 2361 | `	case 'm':` |
|      - | 2362 | `		/* Machine name */` |
|    ! 0 | 2363 | `		ph7_result_string(pCtx,"x86",(int)sizeof("x86")-1);` |
|    ! 0 | 2364 | `		break;` |
|      - | 2365 | `	default:` |
|      1 | 2366 | `		ph7_result_string_format(pCtx,"%s localhost %u.%u build %u x86",` |
|      - | 2367 | `			zName,` |
|      - | 2368 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2369 | `			);` |
|      - | 2370 | `		break;` |
|      - | 2371 | `	}` |
|      - | 2372 | `#elif defined(__UNIXES__)` |
|      4 | 2373 | `	if( uname(&sName) != 0 ){` |
|    ! 0 | 2374 | `		ph7_result_string(pCtx,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 | 2375 | `		return PH7_OK;` |
|      - | 2376 | `	}` |
|      4 | 2377 | `	switch(zMode[0]){` |
|    ! 0 | 2378 | `	case 's':` |
|      - | 2379 | `		/* Operating system name */` |
|    ! 0 | 2380 | `		ph7_result_string(pCtx,sName.sysname,-1/* Compute length automatically*/);` |
|    ! 0 | 2381 | `		break;` |
|    ! 0 | 2382 | `	case 'n':` |
|      - | 2383 | `		/* Host name */` |
|    ! 0 | 2384 | `		ph7_result_string(pCtx,sName.nodename,-1/* Compute length automatically*/);` |
|    ! 0 | 2385 | `		break;` |
|    ! 0 | 2386 | `	case 'r':` |
|      - | 2387 | `		/* Release information */` |
|    ! 0 | 2388 | `		ph7_result_string(pCtx,sName.release,-1/* Compute length automatically*/);` |
|    ! 0 | 2389 | `		break;` |
|    ! 0 | 2390 | `	case 'v':` |
|      - | 2391 | `		/* Version information. */` |
|    ! 0 | 2392 | `		ph7_result_string(pCtx,sName.version,-1/* Compute length automatically*/);` |
|    ! 0 | 2393 | `		break;` |
|    ! 0 | 2394 | `	case 'm':` |
|      - | 2395 | `		/* Machine name */` |
|    ! 0 | 2396 | `		ph7_result_string(pCtx,sName.machine,-1/* Compute length automatically*/);` |
|    ! 0 | 2397 | `		break;` |
|      2 | 2398 | `	default:` |
|      6 | 2399 | `		ph7_result_string_format(pCtx,` |
|      - | 2400 | `			"%s %s %s %s %s",` |
|      2 | 2401 | `			sName.sysname,` |
|      2 | 2402 | `			sName.release,` |
|      2 | 2403 | `			sName.version,` |
|      2 | 2404 | `			sName.nodename,` |
|      2 | 2405 | `			sName.machine` |
|      - | 2406 | `			);` |
|      4 | 2407 | `		break;` |
|      - | 2408 | `	}` |
|      - | 2409 | `#else` |
|      - | 2410 | `	ph7_result_string(pCtx,"Unknown Operating System",(int)sizeof("Unknown Operating System")-1);` |
|      - | 2411 | `#endif` |
|      5 | 2412 | `	return PH7_OK;` |
|      3 | 2413 |  |
|      - | 2414 | `/*` |
|      - | 2415 | ` * Section:` |
|      - | 2416 | ` *    IO stream implementation.` |
|      - | 2417 | ` * Status:` |
|      - | 2418 | ` *    Stable.` |
|      - | 2419 | ` */` |
|      - | 2420 | `typedef struct io_private io_private;` |
|      - | 2421 | `struct io_private` |
|      - | 2422 |  |
|      - | 2423 | `	const ph7_io_stream *pStream; /* Underlying IO device */` |
|      - | 2424 | `	void *pHandle; /* IO handle */` |
|      - | 2425 | `	/* Unbuffered IO */` |
|      - | 2426 | `	SyBlob sBuffer; /* Working buffer */` |
|      - | 2427 | `	sxu32 nOfft;    /* Current read offset */` |
|      - | 2428 | `	sxu32 iMagic;   /* Sanity check to avoid misuse */` |
|      - | 2429 | `};` |
|      - | 2430 | `#define IO_PRIVATE_MAGIC 0xFEAC14` |
|      - | 2431 | `/* Make sure we are dealing with a valid io_private instance */` |
|      - | 2432 | `#define IO_PRIVATE_INVALID(IO) ( IO == 0 \|\| IO->iMagic != IO_PRIVATE_MAGIC )` |
|      - | 2433 | `/* Forward declaration */` |
|      - | 2434 | `static void ResetIOPrivate(io_private *pDev);` |
|      - | 2435 | `/*` |
|      - | 2436 | ` * bool ftruncate(resource $handle,int64 $size)` |
|      - | 2437 | ` *  Truncates a file to a given length.` |
|      - | 2438 | ` * Parameters` |
|      - | 2439 | ` *  $handle` |
|      - | 2440 | ` *   The file pointer.` |
|      - | 2441 | ` *   Note:` |
|      - | 2442 | ` *    The handle must be open for writing.` |
|      - | 2443 | ` * $size` |
|      - | 2444 | ` *   The size to truncate to.` |
|      - | 2445 | ` * Return` |
|      - | 2446 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2447 | ` */` |
|      6 | 2448 | `static int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2449 |  |
|      - | 2450 | `	const ph7_io_stream *pStream;` |
|      - | 2451 | `	io_private *pDev;` |
|      - | 2452 | `	int rc;` |
|      7 | 2453 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2454 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2455 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2456 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2457 | `		return PH7_OK;` |
|      - | 2458 | `	}` |
|      - | 2459 | `	/* Extract our private data */` |
|      7 | 2460 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2461 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 2462 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2463 | `		/*Expecting an IO handle */` |
|    ! 0 | 2464 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2465 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2466 | `		return PH7_OK;` |
|      - | 2467 | `	}` |
|      - | 2468 | `	/* Point to the target IO stream device */` |
|      7 | 2469 | `	pStream = pDev->pStream;` |
|      7 | 2470 | `	if( pStream == 0  \|\| pStream->xTrunc == 0){` |
|    ! 0 | 2471 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2472 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2473 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2474 | `			);` |
|    ! 0 | 2475 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2476 | `		return PH7_OK;` |
|      - | 2477 | `	}` |
|      - | 2478 | `	/* Perform the requested operation */` |
|      7 | 2479 | `	rc = pStream->xTrunc(pDev->pHandle,ph7_value_to_int64(apArg[1]));` |
|      7 | 2480 | `	if( rc == PH7_OK ){` |
|      - | 2481 | `		/* Discard buffered data */` |
|      7 | 2482 | `		ResetIOPrivate(pDev);` |
|      3 | 2483 | `	}` |
|      - | 2484 | `	/* IO result */` |
|      7 | 2485 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      7 | 2486 | `	return PH7_OK;` |
|      4 | 2487 |  |
|      - | 2488 | `/*` |
|      - | 2489 | ` * int fseek(resource $handle,int $offset[,int $whence = SEEK_SET ])` |
|      - | 2490 | ` *  Seeks on a file pointer.` |
|      - | 2491 | ` * Parameters` |
|      - | 2492 | ` *  $handle` |
|      - | 2493 | ` *   A file system pointer resource that is typically created using fopen().` |
|      - | 2494 | ` * $offset` |
|      - | 2495 | ` *   The offset.` |
|      - | 2496 | ` *   To move to a position before the end-of-file, you need to pass a negative` |
|      - | 2497 | ` *   value in offset and set whence to SEEK_END.` |
|      - | 2498 | ` *   whence` |
|      - | 2499 | ` *   whence values are:` |
|      - | 2500 | ` *    SEEK_SET - Set position equal to offset bytes.` |
|      - | 2501 | ` *    SEEK_CUR - Set position to current location plus offset.` |
|      - | 2502 | ` *    SEEK_END - Set position to end-of-file plus offset.` |
|      - | 2503 | ` * Return` |
|      - | 2504 | ` *  0 on success,-1 on failure` |
|      - | 2505 | ` */` |
|      2 | 2506 | `static int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2507 |  |
|      - | 2508 | `	const ph7_io_stream *pStream;` |
|      - | 2509 | `	io_private *pDev;` |
|      - | 2510 | `	ph7_int64 iOfft;` |
|      - | 2511 | `	int whence;` |
|      - | 2512 | `	int rc;` |
|      3 | 2513 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2514 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2515 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2516 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2517 | `		return PH7_OK;` |
|      - | 2518 | `	}` |
|      - | 2519 | `	/* Extract our private data */` |
|      3 | 2520 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2521 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 2522 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2523 | `		/*Expecting an IO handle */` |
|    ! 0 | 2524 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2525 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2526 | `		return PH7_OK;` |
|      - | 2527 | `	}` |
|      - | 2528 | `	/* Point to the target IO stream device */` |
|      3 | 2529 | `	pStream = pDev->pStream;` |
|      3 | 2530 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2531 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2532 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2533 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2534 | `			);` |
|    ! 0 | 2535 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2536 | `		return PH7_OK;` |
|      - | 2537 | `	}` |
|      - | 2538 | `	/* Extract the offset */` |
|      3 | 2539 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|      3 | 2540 | `	whence = 0;/* SEEK_SET */` |
|      3 | 2541 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|    ! 0 | 2542 | `		whence = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2543 | `	}` |
|      - | 2544 | `	/* Perform the requested operation */` |
|      3 | 2545 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|      3 | 2546 | `	if( rc == PH7_OK ){` |
|      - | 2547 | `		/* Ignore buffered data */` |
|      3 | 2548 | `		ResetIOPrivate(pDev);` |
|      1 | 2549 | `	}` |
|      - | 2550 | `	/* IO result */` |
|      3 | 2551 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|      3 | 2552 | `	return PH7_OK;` |
|      2 | 2553 |  |
|      - | 2554 | `/*` |
|      - | 2555 | ` * int64 ftell(resource $handle)` |
|      - | 2556 | ` *  Returns the current position of the file read/write pointer.` |
|      - | 2557 | ` * Parameters` |
|      - | 2558 | ` *  $handle` |
|      - | 2559 | ` *   The file pointer.` |
|      - | 2560 | ` * Return` |
|      - | 2561 | ` *  Returns the position of the file pointer referenced by handle` |
|      - | 2562 | ` *  as an integer; i.e., its offset into the file stream.` |
|      - | 2563 | ` *  FALSE is returned on failure.` |
|      - | 2564 | ` */` |
|      6 | 2565 | `static int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2566 |  |
|      - | 2567 | `	const ph7_io_stream *pStream;` |
|      - | 2568 | `	io_private *pDev;` |
|      - | 2569 | `	ph7_int64 iOfft;` |
|      7 | 2570 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2571 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2572 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2573 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2574 | `		return PH7_OK;` |
|      - | 2575 | `	}` |
|      - | 2576 | `	/* Extract our private data */` |
|      7 | 2577 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2578 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 2579 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2580 | `		/*Expecting an IO handle */` |
|    ! 0 | 2581 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2582 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2583 | `		return PH7_OK;` |
|      - | 2584 | `	}` |
|      - | 2585 | `	/* Point to the target IO stream device */` |
|      7 | 2586 | `	pStream = pDev->pStream;` |
|      7 | 2587 | `	if( pStream == 0  \|\| pStream->xTell == 0){` |
|    ! 0 | 2588 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2589 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2590 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2591 | `			);` |
|    ! 0 | 2592 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2593 | `		return PH7_OK;` |
|      - | 2594 | `	}` |
|      - | 2595 | `	/* Perform the requested operation */` |
|      7 | 2596 | `	iOfft = pStream->xTell(pDev->pHandle);` |
|      - | 2597 | `	/* IO result */` |
|      7 | 2598 | `	ph7_result_int64(pCtx,iOfft);` |
|      7 | 2599 | `	return PH7_OK;` |
|      4 | 2600 |  |
|      - | 2601 | `/*` |
|      - | 2602 | ` * bool rewind(resource $handle)` |
|      - | 2603 | ` *  Rewind the position of a file pointer.` |
|      - | 2604 | ` * Parameters` |
|      - | 2605 | ` *  $handle` |
|      - | 2606 | ` *   The file pointer.` |
|      - | 2607 | ` * Return` |
|      - | 2608 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2609 | ` */` |
|      4 | 2610 | `static int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2611 |  |
|      - | 2612 | `	const ph7_io_stream *pStream;` |
|      - | 2613 | `	io_private *pDev;` |
|      - | 2614 | `	int rc;` |
|      5 | 2615 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2616 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2617 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2618 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2619 | `		return PH7_OK;` |
|      - | 2620 | `	}` |
|      - | 2621 | `	/* Extract our private data */` |
|      5 | 2622 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2623 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 2624 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2625 | `		/*Expecting an IO handle */` |
|    ! 0 | 2626 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2627 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2628 | `		return PH7_OK;` |
|      - | 2629 | `	}` |
|      - | 2630 | `	/* Point to the target IO stream device */` |
|      5 | 2631 | `	pStream = pDev->pStream;` |
|      5 | 2632 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2633 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2634 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2635 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2636 | `			);` |
|    ! 0 | 2637 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2638 | `		return PH7_OK;` |
|      - | 2639 | `	}` |
|      - | 2640 | `	/* Perform the requested operation */` |
|      5 | 2641 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|      5 | 2642 | `	if( rc == PH7_OK ){` |
|      - | 2643 | `		/* Ignore buffered data */` |
|      5 | 2644 | `		ResetIOPrivate(pDev);` |
|      2 | 2645 | `	}` |
|      - | 2646 | `	/* IO result */` |
|      5 | 2647 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 2648 | `	return PH7_OK;` |
|      3 | 2649 |  |
|      - | 2650 | `/*` |
|      - | 2651 | ` * bool fflush(resource $handle)` |
|      - | 2652 | ` *  Flushes the output to a file.` |
|      - | 2653 | ` * Parameters` |
|      - | 2654 | ` *  $handle` |
|      - | 2655 | ` *   The file pointer.` |
|      - | 2656 | ` * Return` |
|      - | 2657 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2658 | ` */` |
|      2 | 2659 | `static int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2660 |  |
|      - | 2661 | `	const ph7_io_stream *pStream;` |
|      - | 2662 | `	io_private *pDev;` |
|      - | 2663 | `	int rc;` |
|      3 | 2664 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2665 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2666 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2667 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2668 | `		return PH7_OK;` |
|      - | 2669 | `	}` |
|      - | 2670 | `	/* Extract our private data */` |
|      3 | 2671 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2672 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 2673 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2674 | `		/*Expecting an IO handle */` |
|    ! 0 | 2675 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2676 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2677 | `		return PH7_OK;` |
|      - | 2678 | `	}` |
|      - | 2679 | `	/* Point to the target IO stream device */` |
|      3 | 2680 | `	pStream = pDev->pStream;` |
|      3 | 2681 | `	if( pStream == 0 \|\| pStream->xSync == 0){` |
|    ! 0 | 2682 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2683 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2684 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2685 | `			);` |
|    ! 0 | 2686 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2687 | `		return PH7_OK;` |
|      - | 2688 | `	}` |
|      - | 2689 | `	/* Perform the requested operation */` |
|      3 | 2690 | `	rc = pStream->xSync(pDev->pHandle);` |
|      - | 2691 | `	/* IO result */` |
|      3 | 2692 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 2693 | `	return PH7_OK;` |
|      2 | 2694 |  |
|      - | 2695 | `/*` |
|      - | 2696 | ` * bool feof(resource $handle)` |
|      - | 2697 | ` *  Tests for end-of-file on a file pointer.` |
|      - | 2698 | ` * Parameters` |
|      - | 2699 | ` *  $handle` |
|      - | 2700 | ` *   The file pointer.` |
|      - | 2701 | ` * Return` |
|      - | 2702 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|      - | 2703 | ` */` |
|   7304 | 2704 | `static int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2705 |  |
|      - | 2706 | `	const ph7_io_stream *pStream;` |
|      - | 2707 | `	io_private *pDev;` |
|      - | 2708 | `	int rc;` |
|   7306 | 2709 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2710 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 2711 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2712 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2713 | `		return PH7_OK;` |
|      - | 2714 | `	}` |
|      - | 2715 | `	/* Extract our private data */` |
|   7306 | 2716 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2717 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   7306 | 2718 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2719 | `		/*Expecting an IO handle */` |
|    ! 0 | 2720 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2721 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2722 | `		return PH7_OK;` |
|      - | 2723 | `	}` |
|      - | 2724 | `	/* Point to the target IO stream device */` |
|   7306 | 2725 | `	pStream = pDev->pStream;` |
|   7306 | 2726 | `	if( pStream == 0 ){` |
|    ! 0 | 2727 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2728 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2729 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2730 | `			);` |
|    ! 0 | 2731 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2732 | `		return PH7_OK;` |
|      - | 2733 | `	}` |
|   7306 | 2734 | `	rc = SXERR_EOF;` |
|      - | 2735 | `	/* Perform the requested operation */` |
|   7306 | 2736 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2737 | `		/* Data is available */` |
|   3308 | 2738 | `		rc = PH7_OK;` |
|   1655 | 2739 | `	}else{` |
|      - | 2740 | `		char zBuf[4096];` |
|      - | 2741 | `		ph7_int64 n;` |
|      - | 2742 | `		/* Perform a buffered read */` |
|   4000 | 2743 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|   4000 | 2744 | `		if( n > 0 ){` |
|      - | 2745 | `			/* Copy buffered data */` |
|   1212 | 2746 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|   1212 | 2747 | `			rc = PH7_OK;` |
|    605 | 2748 | `		}` |
|      - | 2749 | `	}` |
|      - | 2750 | `	/* EOF or not */` |
|   7306 | 2751 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|   7306 | 2752 | `	return PH7_OK;` |
|   3654 | 2753 |  |
|      - | 2754 | `/*` |
|      - | 2755 | ` * Read n bytes from the underlying IO stream device.` |
|      - | 2756 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|      - | 2757 | ` * [i.e: IO error ] or EOF.` |
|      - | 2758 | ` */` |
|     18 | 2759 | `static ph7_int64 StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      2 | 2760 |  |
|     20 | 2761 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|     20 | 2762 | `	char *zBuf = (char *)pBuf;` |
|      - | 2763 | `	ph7_int64 n,nRead;` |
|     20 | 2764 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     20 | 2765 | `	if( n > 0 ){` |
|    ! 0 | 2766 | `		if( n > nLen ){` |
|    ! 0 | 2767 | `			n = nLen;` |
|    ! 0 | 2768 | `		}` |
|      - | 2769 | `		/* Copy the buffered data */` |
|    ! 0 | 2770 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|      - | 2771 | `		/* Update the read offset */` |
|    ! 0 | 2772 | `		pDev->nOfft += (sxu32)n;` |
|    ! 0 | 2773 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2774 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|    ! 0 | 2775 | `			SyBlobReset(&pDev->sBuffer);` |
|    ! 0 | 2776 | `			pDev->nOfft = 0;` |
|    ! 0 | 2777 | `		}` |
|    ! 0 | 2778 | `		nLen -= n;` |
|    ! 0 | 2779 | `		if( nLen < 1 ){` |
|      - | 2780 | `			/* All done */` |
|    ! 0 | 2781 | `			return n;` |
|      - | 2782 | `		}` |
|      - | 2783 | `		/* Advance the cursor */` |
|    ! 0 | 2784 | `		zBuf += n;` |
|    ! 0 | 2785 | `	}` |
|      - | 2786 | `	/* Read without buffering */` |
|     20 | 2787 | `	nRead = pStream->xRead(pDev->pHandle,zBuf,nLen);` |
|     20 | 2788 | `	if( nRead > 0 ){` |
|     18 | 2789 | `		n += nRead;` |
|     11 | 2790 | `	}else if( n < 1 ){` |
|      - | 2791 | `		/* EOF or IO error */` |
|      3 | 2792 | `		return nRead;` |
|      - | 2793 | `	}` |
|     18 | 2794 | `	return n;` |
|     11 | 2795 |  |
|      - | 2796 | `/*` |
|      - | 2797 | ` * Extract a single line from the buffered input.` |
|      - | 2798 | ` */` |
|   4568 | 2799 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      2 | 2800 |  |
|      - | 2801 | `	const char *zIn,*zEnd,*zPtr;` |
|   4570 | 2802 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|   4570 | 2803 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|   4570 | 2804 | `	zPtr = zIn;` |
| 292047 | 2805 | `	while( zIn < zEnd ){` |
| 291951 | 2806 | `		if( zIn[0] == '\n' ){` |
|      - | 2807 | `			/* Line found */` |
|   4474 | 2808 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|   4474 | 2809 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|   4474 | 2810 | `			*pzLine = zPtr;` |
|   4474 | 2811 | `			return SXRET_OK;` |
|      - | 2812 | `		}` |
| 287479 | 2813 | `		zIn++;` |
|      2 | 2814 | `	}` |
|      - | 2815 | `	/* No line were found */` |
|     98 | 2816 | `	return SXERR_NOTFOUND;` |
|   2286 | 2817 |  |
|      - | 2818 | `/*` |
|      - | 2819 | ` * Read a single line from the underlying IO stream device.` |
|      - | 2820 | ` */` |
|   4572 | 2821 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      2 | 2822 |  |
|   4574 | 2823 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - | 2824 | `	char zBuf[8192];` |
|      - | 2825 | `	ph7_int64 n;` |
|      - | 2826 | `	sxi32 rc;` |
|   4574 | 2827 | `	n = 0;` |
|   4574 | 2828 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2829 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     54 | 2830 | `		SyBlobReset(&pDev->sBuffer);` |
|     54 | 2831 | `		pDev->nOfft = 0;` |
|     26 | 2832 | `	}` |
|   4574 | 2833 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2834 | `		/* Check if there is a line */` |
|   4522 | 2835 | `		rc = GetLine(pDev,&n,pzData);` |
|   4522 | 2836 | `		if( rc == SXRET_OK ){` |
|      - | 2837 | `			/* Got line,update the cursor  */` |
|   4428 | 2838 | `			pDev->nOfft += (sxu32)n;` |
|   4428 | 2839 | `			return n;` |
|      - | 2840 | `		}` |
|     47 | 2841 | `	}` |
|      - | 2842 | `	/* Perform the read operation until a new line is extracted or length` |
|      - | 2843 | `	 * limit is reached.` |
|      - | 2844 | `	 */` |
|     74 | 2845 | `	for(;;){` |
|    150 | 2846 | `		n = pStream->xRead(pDev->pHandle,zBuf, (nMaxLen > 0 && nMaxLen < (ph7_int64)sizeof(zBuf)) ? nMaxLen : (ph7_int64)sizeof(zBuf));` |
|    150 | 2847 | `		if( n < 1 ){` |
|      - | 2848 | `			/* EOF or IO error */` |
|    102 | 2849 | `			break;` |
|      - | 2850 | `		}` |
|      - | 2851 | `		/* Append the data just read */` |
|     50 | 2852 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - | 2853 | `		/* Try to extract a line */` |
|     50 | 2854 | `		rc = GetLine(pDev,&n,pzData);` |
|     50 | 2855 | `		if( rc == SXRET_OK ){` |
|      - | 2856 | `			/* Got one,return immediately */` |
|     48 | 2857 | `			pDev->nOfft += (sxu32)n;` |
|     48 | 2858 | `			return n;` |
|      - | 2859 | `		}` |
|      3 | 2860 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
|      - | 2861 | `			/* Read limit reached,return the available data */` |
|    ! 0 | 2862 | `			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    ! 0 | 2863 | `			n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 2864 | `			/* Reset the working buffer */` |
|    ! 0 | 2865 | `			SyBlobReset(&pDev->sBuffer);` |
|    ! 0 | 2866 | `			pDev->nOfft = 0;` |
|    ! 0 | 2867 | `			return n;` |
|      - | 2868 | `		}` |
|      1 | 2869 | `	}` |
|    102 | 2870 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2871 | `		/* Read limit reached,return the available data */` |
|     98 | 2872 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|     98 | 2873 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 2874 | `		/* Reset the working buffer */` |
|     98 | 2875 | `		SyBlobReset(&pDev->sBuffer);` |
|     98 | 2876 | `		pDev->nOfft = 0;` |
|     48 | 2877 | `	}` |
|    102 | 2878 | `	return n;` |
|   2288 | 2879 |  |
|      - | 2880 | `/*` |
|      - | 2881 | ` * Open an IO stream handle.` |
|      - | 2882 | ` * Notes on stream:` |
|      - | 2883 | ` * According to the PHP reference manual.` |
|      - | 2884 | ` * In its simplest definition, a stream is a resource object which exhibits streamable behavior.` |
|      - | 2885 | ` * That is, it can be read from or written to in a linear fashion, and may be able to fseek()` |
|      - | 2886 | ` * to an arbitrary locations within the stream.` |
|      - | 2887 | ` * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.` |
|      - | 2888 | ` * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file` |
|      - | 2889 | ` * on a remote server.` |
|      - | 2890 | ` * A stream is referenced as: scheme://target` |
|      - | 2891 | ` *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...` |
|      - | 2892 | ` *   If no wrapper is specified, the function default is used (typically file://).` |
|      - | 2893 | ` *   target - Depends on the wrapper used. For filesystem related streams this is typically a path` |
|      - | 2894 | ` *  and filename of the desired file. For network related streams this is typically a hostname, often` |
|      - | 2895 | ` *  with a path appended.` |
|      - | 2896 | ` *` |
|      - | 2897 | ` * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.` |
|      - | 2898 | ` * Please refer to the official documentation for a full discussion.` |
|      - | 2899 | ` * This function return a handle on success. Otherwise null.` |
|      - | 2900 | ` */` |
|  22768 | 2901 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - | 2902 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      2 | 2903 |  |
|  22770 | 2904 | `	void *pHandle = 0; /* cc warning */` |
|      - | 2905 | `	SyString sFile;` |
|      - | 2906 | `	int rc;` |
|  22770 | 2907 | `	if( pStream == 0 ){` |
|      - | 2908 | `		/* No such stream device */` |
|    ! 0 | 2909 | `		return 0;` |
|      - | 2910 | `	}` |
|  22770 | 2911 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  22770 | 2912 | `	if( use_include ){` |
|   7642 | 2913 | `		if(	sFile.zString[0] == '/' \|\|` |
|      - | 2914 | `#ifdef __WINNT__` |
|      - | 2915 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - | 2916 | `#endif` |
|   7632 | 2917 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   7630 | 2918 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - | 2919 | `				/*  Open the file directly */` |
|     13 | 2920 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      7 | 2921 | `		}else{` |
|      - | 2922 | `			SyString *pPath;` |
|      - | 2923 | `			SyBlob sWorker;` |
|      - | 2924 | `#ifdef __WINNT__` |
|      - | 2925 | `			static const int c = '\\';` |
|      - | 2926 | `#else` |
|      - | 2927 | `			static const int c = '/';` |
|      - | 2928 | `#endif` |
|      - | 2929 | `			/* Init the path builder working buffer */` |
|   7632 | 2930 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - | 2931 | `			/* Build a path from the set of include path */` |
|   7632 | 2932 | `			SySetResetCursor(&pVm->aPaths);` |
|   7632 | 2933 | `			rc = SXERR_IO;` |
|   7634 | 2934 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - | 2935 | `				/* Build full path */` |
|   7632 | 2936 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - | 2937 | `				/* Append null terminator */` |
|   7632 | 2938 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 2939 | `					continue;` |
|      - | 2940 | `				}` |
|      - | 2941 | `				/* Try to open the file */` |
|   7632 | 2942 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   7632 | 2943 | `				if( rc == PH7_OK ){` |
|   7629 | 2944 | `					if( bPushInclude ){` |
|      - | 2945 | `						/* Mark as included */` |
|   7629 | 2946 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   3814 | 2947 | `					}` |
|   7629 | 2948 | `					break;` |
|      - | 2949 | `				}` |
|      - | 2950 | `				/* Reset the working buffer */` |
|      3 | 2951 | `				SyBlobReset(&sWorker);` |
|      - | 2952 | `				/* Check the next path */` |
|      1 | 2953 | `			}` |
|   7632 | 2954 | `			SyBlobRelease(&sWorker);` |
|      - | 2955 | `		}` |
|   7644 | 2956 | `		if( rc == PH7_OK ){` |
|   7641 | 2957 | `			if( bPushInclude ){` |
|      - | 2958 | `				/* Mark as included */` |
|   7641 | 2959 | `				PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|   3820 | 2960 | `			}` |
|   3820 | 2961 | `		}` |
|   3823 | 2962 | `	}else{` |
|      - | 2963 | `		/* Open the URI direcly */` |
|  15128 | 2964 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - | 2965 | `	}` |
|  22770 | 2966 | `	if( rc != PH7_OK ){` |
|      - | 2967 | `		/* IO error */` |
|      9 | 2968 | `		return 0;` |
|      - | 2969 | `	}` |
|      - | 2970 | `	/* Return the file handle */` |
|  22762 | 2971 | `	return pHandle;` |
|  11386 | 2972 |  |
|      - | 2973 | `/*` |
|      - | 2974 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - | 2975 | ` * Store the read data in the given BLOB (last argument).` |
|      - | 2976 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - | 2977 | ` */` |
|   7638 | 2978 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      1 | 2979 |  |
|      - | 2980 | `	ph7_int64 nRead;` |
|      - | 2981 | `	char zBuf[8192]; /* 8K */` |
|      - | 2982 | `	int rc;` |
|      - | 2983 | `	/* Perform the requested operation */` |
|   7638 | 2984 | `	for(;;){` |
|  15277 | 2985 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  15277 | 2986 | `		if( nRead < 1 ){` |
|      - | 2987 | `			/* EOF or IO error */` |
|   7639 | 2988 | `			break;` |
|      - | 2989 | `		}` |
|      - | 2990 | `		/* Append contents */` |
|   7639 | 2991 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   7639 | 2992 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 2993 | `			break;` |
|      - | 2994 | `		}` |
|      1 | 2995 | `	}` |
|   7639 | 2996 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      1 | 2997 |  |
|      - | 2998 | `/*` |
|      - | 2999 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - | 3000 | ` */` |
|  22830 | 3001 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      2 | 3002 |  |
|  22832 | 3003 | `	if( pStream->xClose ){` |
|  22832 | 3004 | `		pStream->xClose(pHandle);` |
|  11415 | 3005 | `	}` |
|  22832 | 3006 |  |
|      - | 3007 | `/*` |
|      - | 3008 | ` * string fgetc(resource $handle)` |
|      - | 3009 | ` *  Gets a character from the given file pointer.` |
|      - | 3010 | ` * Parameters` |
|      - | 3011 | ` *  $handle` |
|      - | 3012 | ` *   The file pointer.` |
|      - | 3013 | ` * Return` |
|      - | 3014 | ` *  Returns a string containing a single character read from the file` |
|      - | 3015 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|      - | 3016 | ` * WARNING` |
|      - | 3017 | ` *  This operation is extremely slow.Avoid using it.` |
|      - | 3018 | ` */` |
|      4 | 3019 | `static int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3020 |  |
|      - | 3021 | `	const ph7_io_stream *pStream;` |
|      - | 3022 | `	io_private *pDev;` |
|      - | 3023 | `	int c,n;` |
|      5 | 3024 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3025 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3026 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3027 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3028 | `		return PH7_OK;` |
|      - | 3029 | `	}` |
|      - | 3030 | `	/* Extract our private data */` |
|      5 | 3031 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3032 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 3033 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3034 | `		/*Expecting an IO handle */` |
|    ! 0 | 3035 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3036 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3037 | `		return PH7_OK;` |
|      - | 3038 | `	}` |
|      - | 3039 | `	/* Point to the target IO stream device */` |
|      5 | 3040 | `	pStream = pDev->pStream;` |
|      5 | 3041 | `	if( pStream == 0  ){` |
|    ! 0 | 3042 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3043 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3044 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3045 | `			);` |
|    ! 0 | 3046 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3047 | `		return PH7_OK;` |
|      - | 3048 | `	}` |
|      - | 3049 | `	/* Perform the requested operation */` |
|      5 | 3050 | `	n = (int)StreamRead(pDev,(void *)&c,sizeof(char));` |
|      - | 3051 | `	/* IO result */` |
|      5 | 3052 | `	if( n < 1 ){` |
|      - | 3053 | `		/* EOF or error,return FALSE */` |
|    ! 0 | 3054 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3055 | `	}else{` |
|      - | 3056 | `		/* Return the string holding the character */` |
|      5 | 3057 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|      - | 3058 | `	}` |
|      5 | 3059 | `	return PH7_OK;` |
|      3 | 3060 |  |
|      - | 3061 | `/*` |
|      - | 3062 | ` * string fgets(resource $handle[,int64 $length ])` |
|      - | 3063 | ` *  Gets line from file pointer.` |
|      - | 3064 | ` * Parameters` |
|      - | 3065 | ` *  $handle` |
|      - | 3066 | ` *   The file pointer.` |
|      - | 3067 | ` * $length` |
|      - | 3068 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3069 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3070 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3071 | ` *  the end of the line.` |
|      - | 3072 | ` * Return` |
|      - | 3073 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3074 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3075 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3076 | ` */` |
|   4562 | 3077 | `static int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3078 |  |
|      - | 3079 | `	const ph7_io_stream *pStream;` |
|      - | 3080 | `	const char *zLine;` |
|      - | 3081 | `	io_private *pDev;` |
|      - | 3082 | `	ph7_int64 n,nLen;` |
|   4564 | 3083 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3084 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3085 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3086 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3087 | `		return PH7_OK;` |
|      - | 3088 | `	}` |
|      - | 3089 | `	/* Extract our private data */` |
|   4564 | 3090 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3091 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   4564 | 3092 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3093 | `		/*Expecting an IO handle */` |
|    ! 0 | 3094 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3095 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3096 | `		return PH7_OK;` |
|      - | 3097 | `	}` |
|      - | 3098 | `	/* Point to the target IO stream device */` |
|   4564 | 3099 | `	pStream = pDev->pStream;` |
|   4564 | 3100 | `	if( pStream == 0  ){` |
|    ! 0 | 3101 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3102 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3103 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3104 | `			);` |
|    ! 0 | 3105 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3106 | `		return PH7_OK;` |
|      - | 3107 | `	}` |
|   4564 | 3108 | `	nLen = -1;` |
|   4564 | 3109 | `	if( nArg > 1 ){` |
|      - | 3110 | `		/* Maximum data to read */` |
|    ! 0 | 3111 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3112 | `	}` |
|      - | 3113 | `	/* Perform the requested operation */` |
|   4564 | 3114 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|   4564 | 3115 | `	if( n < 1 ){` |
|      - | 3116 | `		/* EOF or IO error,return FALSE */` |
|      4 | 3117 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3118 | `	}else{` |
|      - | 3119 | `		/* Return the freshly extracted line */` |
|   4562 | 3120 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - | 3121 | `	}` |
|   4564 | 3122 | `	return PH7_OK;` |
|   2283 | 3123 |  |
|      - | 3124 | `/*` |
|      - | 3125 | ` * string fread(resource $handle,int64 $length)` |
|      - | 3126 | ` *  Binary-safe file read.` |
|      - | 3127 | ` * Parameters` |
|      - | 3128 | ` *  $handle` |
|      - | 3129 | ` *   The file pointer.` |
|      - | 3130 | ` * $length` |
|      - | 3131 | ` *  Up to length number of bytes read.` |
|      - | 3132 | ` * Return` |
|      - | 3133 | ` *  The data readen on success or FALSE on failure.` |
|      - | 3134 | ` */` |
|     10 | 3135 | `static int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3136 |  |
|      - | 3137 | `	const ph7_io_stream *pStream;` |
|      - | 3138 | `	io_private *pDev;` |
|      - | 3139 | `	ph7_int64 nRead;` |
|      - | 3140 | `	void *pBuf;` |
|      - | 3141 | `	int nLen;` |
|     12 | 3142 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3143 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3144 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3145 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3146 | `		return PH7_OK;` |
|      - | 3147 | `	}` |
|      - | 3148 | `	/* Extract our private data */` |
|     12 | 3149 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3150 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     12 | 3151 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3152 | `		/*Expecting an IO handle */` |
|    ! 0 | 3153 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3154 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3155 | `		return PH7_OK;` |
|      - | 3156 | `	}` |
|      - | 3157 | `	/* Point to the target IO stream device */` |
|     12 | 3158 | `	pStream = pDev->pStream;` |
|     12 | 3159 | `	if( pStream == 0  ){` |
|    ! 0 | 3160 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3161 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3162 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3163 | `			);` |
|    ! 0 | 3164 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3165 | `		return PH7_OK;` |
|      - | 3166 | `	}` |
|     12 | 3167 | `        nLen = 4096;` |
|     12 | 3168 | `	if( nArg > 1 ){` |
|     12 | 3169 | ` 	  nLen = ph7_value_to_int(apArg[1]);` |
|     12 | 3170 | `	  if( nLen < 1 ){` |
|      - | 3171 | `		/* Invalid length,set a default length */` |
|    ! 0 | 3172 | `		nLen = 4096;` |
|    ! 0 | 3173 | `	  }` |
|      5 | 3174 | `        }` |
|      - | 3175 | `	/* Allocate enough buffer */` |
|     12 | 3176 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     12 | 3177 | `	if( pBuf == 0 ){` |
|    ! 0 | 3178 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3179 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3180 | `		return PH7_OK;` |
|      - | 3181 | `	}` |
|      - | 3182 | `	/* Perform the requested operation */` |
|     12 | 3183 | `	nRead = StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     12 | 3184 | `	if( nRead < 1 ){` |
|      - | 3185 | `		/* Nothing read,return FALSE */` |
|    ! 0 | 3186 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3187 | `	}else{` |
|      - | 3188 | `		/* Make a copy of the data just read */` |
|     12 | 3189 | `		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);` |
|      - | 3190 | `	}` |
|      - | 3191 | `	/* Release the buffer */` |
|     12 | 3192 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     12 | 3193 | `	return PH7_OK;` |
|      7 | 3194 |  |
|      - | 3195 | `/*` |
|      - | 3196 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|      - | 3197 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|      - | 3198 | ` * Gets line from file pointer and parse for CSV fields.` |
|      - | 3199 | ` * Parameters` |
|      - | 3200 | ` * $handle` |
|      - | 3201 | ` *   The file pointer.` |
|      - | 3202 | ` * $length` |
|      - | 3203 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3204 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3205 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3206 | ` *  the end of the line.` |
|      - | 3207 | ` * $delimiter` |
|      - | 3208 | ` *   Set the field delimiter (one character only).` |
|      - | 3209 | ` * $enclosure` |
|      - | 3210 | ` *   Set the field enclosure character (one character only).` |
|      - | 3211 | ` * $escape` |
|      - | 3212 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 3213 | ` * Return` |
|      - | 3214 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3215 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3216 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3217 | ` */` |
|      2 | 3218 | `static int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3219 |  |
|      - | 3220 | `	const ph7_io_stream *pStream;` |
|      - | 3221 | `	const char *zLine;` |
|      - | 3222 | `	io_private *pDev;` |
|      - | 3223 | `	ph7_int64 n,nLen;` |
|      3 | 3224 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3225 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3226 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3227 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3228 | `		return PH7_OK;` |
|      - | 3229 | `	}` |
|      - | 3230 | `	/* Extract our private data */` |
|      3 | 3231 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3232 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3233 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3234 | `		/*Expecting an IO handle */` |
|    ! 0 | 3235 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3236 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3237 | `		return PH7_OK;` |
|      - | 3238 | `	}` |
|      - | 3239 | `	/* Point to the target IO stream device */` |
|      3 | 3240 | `	pStream = pDev->pStream;` |
|      3 | 3241 | `	if( pStream == 0  ){` |
|    ! 0 | 3242 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3243 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3244 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3245 | `			);` |
|    ! 0 | 3246 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3247 | `		return PH7_OK;` |
|      - | 3248 | `	}` |
|      3 | 3249 | `	nLen = -1;` |
|      3 | 3250 | `	if( nArg > 1 ){` |
|      - | 3251 | `		/* Maximum data to read */` |
|      3 | 3252 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      1 | 3253 | `	}` |
|      - | 3254 | `	/* Perform the requested operation */` |
|      3 | 3255 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3256 | `	if( n < 1 ){` |
|      - | 3257 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3258 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3259 | `	}else{` |
|      - | 3260 | `		ph7_value *pArray;` |
|      3 | 3261 | `		int delim  = ',';   /* Delimiter */` |
|      3 | 3262 | `		int encl   = '"' ;  /* Enclosure */` |
|      3 | 3263 | `		int escape = '\\';  /* Escape character */` |
|      3 | 3264 | `		if( nArg > 2 ){` |
|      - | 3265 | `			const char *zPtr;` |
|      - | 3266 | `			int i;` |
|      3 | 3267 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 3268 | `				/* Extract the delimiter */` |
|      3 | 3269 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 3270 | `				if( i > 0 ){` |
|      3 | 3271 | `					delim = zPtr[0];` |
|      1 | 3272 | `				}` |
|      1 | 3273 | `			}` |
|      3 | 3274 | `			if( nArg > 3 ){` |
|      3 | 3275 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 3276 | `					/* Extract the enclosure */` |
|      3 | 3277 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 3278 | `					if( i > 0 ){` |
|      3 | 3279 | `						encl = zPtr[0];` |
|      1 | 3280 | `					}` |
|      1 | 3281 | `				}` |
|      3 | 3282 | `				if( nArg > 4 ){` |
|      3 | 3283 | `					if( ph7_value_is_string(apArg[4]) ){` |
|      - | 3284 | `						/* Extract the escape character */` |
|      3 | 3285 | `						zPtr = ph7_value_to_string(apArg[4],&i);` |
|      3 | 3286 | `						if( i > 0 ){` |
|      3 | 3287 | `							escape = zPtr[0];` |
|      1 | 3288 | `						}` |
|      1 | 3289 | `					}` |
|      1 | 3290 | `				}` |
|      1 | 3291 | `			}` |
|      1 | 3292 | `		}` |
|      - | 3293 | `		/* Create our array */` |
|      3 | 3294 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 3295 | `		if( pArray == 0 ){` |
|    ! 0 | 3296 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3297 | `			ph7_result_null(pCtx);` |
|    ! 0 | 3298 | `			return PH7_OK;` |
|      - | 3299 | `		}` |
|      - | 3300 | `		/* Parse the raw input */` |
|      3 | 3301 | `		PH7_ProcessCsv(zLine,(int)n,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 3302 | `		/* Return the freshly created array  */` |
|      3 | 3303 | `		ph7_result_value(pCtx,pArray);` |
|      - | 3304 | `	}` |
|      3 | 3305 | `	return PH7_OK;` |
|      2 | 3306 |  |
|      - | 3307 | `/*` |
|      - | 3308 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|      - | 3309 | ` *  Gets line from file pointer and strip HTML tags.` |
|      - | 3310 | ` * Parameters` |
|      - | 3311 | ` * $handle` |
|      - | 3312 | ` *   The file pointer.` |
|      - | 3313 | ` * $length` |
|      - | 3314 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3315 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3316 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3317 | ` *  the end of the line.` |
|      - | 3318 | ` * $allowable_tags` |
|      - | 3319 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 3320 | ` * Return` |
|      - | 3321 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|      - | 3322 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|      - | 3323 | ` */` |
|      2 | 3324 | `static int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3325 |  |
|      - | 3326 | `	const ph7_io_stream *pStream;` |
|      - | 3327 | `	const char *zLine;` |
|      - | 3328 | `	io_private *pDev;` |
|      - | 3329 | `	ph7_int64 n,nLen;` |
|      3 | 3330 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3331 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3332 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3333 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3334 | `		return PH7_OK;` |
|      - | 3335 | `	}` |
|      - | 3336 | `	/* Extract our private data */` |
|      3 | 3337 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3338 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3339 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3340 | `		/*Expecting an IO handle */` |
|    ! 0 | 3341 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3342 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3343 | `		return PH7_OK;` |
|      - | 3344 | `	}` |
|      - | 3345 | `	/* Point to the target IO stream device */` |
|      3 | 3346 | `	pStream = pDev->pStream;` |
|      3 | 3347 | `	if( pStream == 0  ){` |
|    ! 0 | 3348 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3349 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3350 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3351 | `			);` |
|    ! 0 | 3352 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3353 | `		return PH7_OK;` |
|      - | 3354 | `	}` |
|      3 | 3355 | `	nLen = -1;` |
|      3 | 3356 | `	if( nArg > 1 ){` |
|      - | 3357 | `		/* Maximum data to read */` |
|    ! 0 | 3358 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3359 | `	}` |
|      - | 3360 | `	/* Perform the requested operation */` |
|      3 | 3361 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3362 | `	if( n < 1 ){` |
|      - | 3363 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3364 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3365 | `	}else{` |
|      3 | 3366 | `		const char *zTaglist = 0;` |
|      3 | 3367 | `		int nTaglen = 0;` |
|      3 | 3368 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|      - | 3369 | `			/* Allowed tag */` |
|    ! 0 | 3370 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|    ! 0 | 3371 | `		}` |
|      - | 3372 | `		/* Process data just read */` |
|      3 | 3373 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen);` |
|      - | 3374 | `	}` |
|      3 | 3375 | `	return PH7_OK;` |
|      2 | 3376 |  |
|      - | 3377 | `/*` |
|      - | 3378 | ` * string readdir(resource $dir_handle)` |
|      - | 3379 | ` *   Read entry from directory handle.` |
|      - | 3380 | ` * Parameter` |
|      - | 3381 | ` *  $dir_handle` |
|      - | 3382 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3383 | ` * Return` |
|      - | 3384 | ` *  Returns the filename on success or FALSE on failure.` |
|      - | 3385 | ` */` |
|   6178 | 3386 | `static int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3387 |  |
|      - | 3388 | `	const ph7_io_stream *pStream;` |
|      - | 3389 | `	io_private *pDev;` |
|      - | 3390 | `	int rc;` |
|   6180 | 3391 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3392 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3393 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3394 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3395 | `		return PH7_OK;` |
|      - | 3396 | `	}` |
|      - | 3397 | `	/* Extract our private data */` |
|   6180 | 3398 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3399 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   6180 | 3400 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3401 | `		/*Expecting an IO handle */` |
|    ! 0 | 3402 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3403 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3404 | `		return PH7_OK;` |
|      - | 3405 | `	}` |
|      - | 3406 | `	/* Point to the target IO stream device */` |
|   6180 | 3407 | `	pStream = pDev->pStream;` |
|   6180 | 3408 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 3409 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3410 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3411 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3412 | `			);` |
|    ! 0 | 3413 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3414 | `		return PH7_OK;` |
|      - | 3415 | `	}` |
|   6180 | 3416 | `	ph7_result_bool(pCtx,0);` |
|      - | 3417 | `	/* Perform the requested operation */` |
|   6180 | 3418 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|   6180 | 3419 | `	if( rc != PH7_OK ){` |
|      - | 3420 | `		/* Return FALSE */` |
|    880 | 3421 | `		ph7_result_bool(pCtx,0);` |
|    439 | 3422 | `	}` |
|   6180 | 3423 | `	return PH7_OK;` |
|   3091 | 3424 |  |
|      - | 3425 | `/*` |
|      - | 3426 | ` * void rewinddir(resource $dir_handle)` |
|      - | 3427 | ` *   Rewind directory handle.` |
|      - | 3428 | ` * Parameter` |
|      - | 3429 | ` *  $dir_handle` |
|      - | 3430 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3431 | ` * Return` |
|      - | 3432 | ` *  FALSE on failure.` |
|      - | 3433 | ` */` |
|      2 | 3434 | `static int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3435 |  |
|      - | 3436 | `	const ph7_io_stream *pStream;` |
|      - | 3437 | `	io_private *pDev;` |
|      3 | 3438 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3439 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3440 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3441 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3442 | `		return PH7_OK;` |
|      - | 3443 | `	}` |
|      - | 3444 | `	/* Extract our private data */` |
|      3 | 3445 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3446 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3447 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3448 | `		/*Expecting an IO handle */` |
|    ! 0 | 3449 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3450 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3451 | `		return PH7_OK;` |
|      - | 3452 | `	}` |
|      - | 3453 | `	/* Point to the target IO stream device */` |
|      3 | 3454 | `	pStream = pDev->pStream;` |
|      3 | 3455 | `	if( pStream == 0  \|\| pStream->xRewindDir == 0 ){` |
|    ! 0 | 3456 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3457 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3458 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3459 | `			);` |
|    ! 0 | 3460 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3461 | `		return PH7_OK;` |
|      - | 3462 | `	}` |
|      - | 3463 | `	/* Perform the requested operation */` |
|      3 | 3464 | `	pStream->xRewindDir(pDev->pHandle);` |
|      3 | 3465 | `	return PH7_OK;` |
|      2 | 3466 | ` }` |
|      - | 3467 | `/* Forward declaration */` |
|      - | 3468 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut);` |
|      - | 3469 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|      - | 3470 | `/*` |
|      - | 3471 | ` * void closedir(resource $dir_handle)` |
|      - | 3472 | ` *   Close directory handle.` |
|      - | 3473 | ` * Parameter` |
|      - | 3474 | ` *  $dir_handle` |
|      - | 3475 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3476 | ` * Return` |
|      - | 3477 | ` *  FALSE on failure.` |
|      - | 3478 | ` */` |
|    880 | 3479 | `static int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3480 |  |
|      - | 3481 | `	const ph7_io_stream *pStream;` |
|      - | 3482 | `	io_private *pDev;` |
|    882 | 3483 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3484 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3485 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3486 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3487 | `		return PH7_OK;` |
|      - | 3488 | `	}` |
|      - | 3489 | `	/* Extract our private data */` |
|    882 | 3490 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3491 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    882 | 3492 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3493 | `		/*Expecting an IO handle */` |
|    ! 0 | 3494 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3495 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3496 | `		return PH7_OK;` |
|      - | 3497 | `	}` |
|      - | 3498 | `	/* Point to the target IO stream device */` |
|    882 | 3499 | `	pStream = pDev->pStream;` |
|    882 | 3500 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 3501 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3502 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3503 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3504 | `			);` |
|    ! 0 | 3505 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3506 | `		return PH7_OK;` |
|      - | 3507 | `	}` |
|      - | 3508 | `	/* Perform the requested operation */` |
|    882 | 3509 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 3510 | `	/* Release the private stucture */` |
|    882 | 3511 | `	ReleaseIOPrivate(pCtx,pDev);` |
|    882 | 3512 | `	PH7_MemObjRelease(apArg[0]);` |
|    882 | 3513 | `	return PH7_OK;` |
|    442 | 3514 | ` }` |
|      - | 3515 | `/*` |
|      - | 3516 | ` * resource opendir(string $path[,resource $context])` |
|      - | 3517 | ` *  Open directory handle.` |
|      - | 3518 | ` * Parameters` |
|      - | 3519 | ` * $path` |
|      - | 3520 | ` *   The directory path that is to be opened.` |
|      - | 3521 | ` * $context` |
|      - | 3522 | ` *   A context stream resource.` |
|      - | 3523 | ` * Return` |
|      - | 3524 | ` *  A directory handle resource on success,or FALSE on failure.` |
|      - | 3525 | ` */` |
|    880 | 3526 | `static int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3527 |  |
|      - | 3528 | `	const ph7_io_stream *pStream;` |
|      - | 3529 | `	const char *zPath;` |
|      - | 3530 | `	io_private *pDev;` |
|      - | 3531 | `	int iLen,rc;` |
|    882 | 3532 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3533 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3534 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 3535 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3536 | `		return PH7_OK;` |
|      - | 3537 | `	}` |
|      - | 3538 | `	/* Extract the target path */` |
|    882 | 3539 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 3540 | `	/* Try to extract a stream */` |
|    882 | 3541 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|    882 | 3542 | `	if( pStream == 0 ){` |
|    ! 0 | 3543 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3544 | `			"No stream device is associated with the given path(%s)",zPath);` |
|    ! 0 | 3545 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3546 | `		return PH7_OK;` |
|      - | 3547 | `	}` |
|    882 | 3548 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 3549 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3550 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 3551 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 3552 | `			);` |
|    ! 0 | 3553 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3554 | `		return PH7_OK;` |
|      - | 3555 | `	}` |
|      - | 3556 | `	/* Allocate a new IO private instance */` |
|    882 | 3557 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    882 | 3558 | `	if( pDev == 0 ){` |
|    ! 0 | 3559 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3560 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3561 | `		return PH7_OK;` |
|      - | 3562 | `	}` |
|      - | 3563 | `	/* Initialize the structure */` |
|    882 | 3564 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 3565 | `	/* Open the target directory */` |
|    882 | 3566 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|    882 | 3567 | `	if( rc != PH7_OK ){` |
|      - | 3568 | `		/* IO error,return FALSE */` |
|    ! 0 | 3569 | `		ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 3570 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3571 | `	}else{` |
|      - | 3572 | `		/* Return the handle as a resource */` |
|    882 | 3573 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 3574 | `	}` |
|    882 | 3575 | `	return PH7_OK;` |
|    442 | 3576 |  |
|      - | 3577 | `/*` |
|      - | 3578 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|      - | 3579 | ` *  Reads a file and writes it to the output buffer.` |
|      - | 3580 | ` * Parameters` |
|      - | 3581 | ` *  $filename` |
|      - | 3582 | ` *   The filename being read.` |
|      - | 3583 | ` *  $use_include_path` |
|      - | 3584 | ` *   You can use the optional second parameter and set it to` |
|      - | 3585 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3586 | ` *  $context` |
|      - | 3587 | ` *   A context stream resource.` |
|      - | 3588 | ` * Return` |
|      - | 3589 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|      - | 3590 | ` */` |
|      2 | 3591 | `static int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3592 |  |
|      3 | 3593 | `	int use_include  = FALSE;` |
|      - | 3594 | `	const ph7_io_stream *pStream;` |
|      - | 3595 | `	ph7_int64 n,nRead;` |
|      - | 3596 | `	const char *zFile;` |
|      - | 3597 | `	char zBuf[8192];` |
|      - | 3598 | `	void *pHandle;` |
|      - | 3599 | `	int rc,nLen;` |
|      3 | 3600 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3601 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3602 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3603 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3604 | `		return PH7_OK;` |
|      - | 3605 | `	}` |
|      - | 3606 | `	/* Extract the file path */` |
|      3 | 3607 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3608 | `	/* Point to the target IO stream device */` |
|      3 | 3609 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3610 | `	if( pStream == 0 ){` |
|    ! 0 | 3611 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3612 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3613 | `		return PH7_OK;` |
|      - | 3614 | `	}` |
|      3 | 3615 | `	if( nArg > 1 ){` |
|    ! 0 | 3616 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3617 | `	}` |
|      - | 3618 | `	/* Try to open the file in read-only mode */` |
|      4 | 3619 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|      1 | 3620 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 3621 | `	if( pHandle == 0 ){` |
|    ! 0 | 3622 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3623 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3624 | `		return PH7_OK;` |
|      - | 3625 | `	}` |
|      - | 3626 | `	/* Perform the requested operation */` |
|      3 | 3627 | `	nRead = 0;` |
|      2 | 3628 | `	for(;;){` |
|      5 | 3629 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3630 | `		if( n < 1 ){` |
|      - | 3631 | `			/* EOF or IO error,break immediately */` |
|      3 | 3632 | `			break;` |
|      - | 3633 | `		}` |
|      - | 3634 | `		/* Output data */` |
|      3 | 3635 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      3 | 3636 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 3637 | `			break;` |
|      - | 3638 | `		}` |
|      - | 3639 | `		/* Increment counter */` |
|      3 | 3640 | `		nRead += n;` |
|      1 | 3641 | `	}` |
|      - | 3642 | `	/* Close the stream */` |
|      3 | 3643 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3644 | `	/* Total number of bytes readen */` |
|      3 | 3645 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 3646 | `	return PH7_OK;` |
|      2 | 3647 |  |
|      - | 3648 | `/*` |
|      - | 3649 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|      - | 3650 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|      - | 3651 | ` *  Reads entire file into a string.` |
|      - | 3652 | ` * Parameters` |
|      - | 3653 | ` *  $filename` |
|      - | 3654 | ` *   The filename being read.` |
|      - | 3655 | ` *  $use_include_path` |
|      - | 3656 | ` *   You can use the optional second parameter and set it to` |
|      - | 3657 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3658 | ` *  $context` |
|      - | 3659 | ` *   A context stream resource.` |
|      - | 3660 | ` *  $offset` |
|      - | 3661 | ` *   The offset where the reading starts on the original stream.` |
|      - | 3662 | ` *  $maxlen` |
|      - | 3663 | ` *    Maximum length of data read. The default is to read until end of file` |
|      - | 3664 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|      - | 3665 | ` * Return` |
|      - | 3666 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3667 | ` */` |
|   4458 | 3668 | `static int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3669 |  |
|      - | 3670 | `	const ph7_io_stream *pStream;` |
|      - | 3671 | `	ph7_int64 n,nRead,nMaxlen;` |
|   4460 | 3672 | `	int use_include  = FALSE;` |
|      - | 3673 | `	const char *zFile;` |
|      - | 3674 | `	char zBuf[8192];` |
|      - | 3675 | `	void *pHandle;` |
|      - | 3676 | `	int nLen;` |
|      - | 3677 |  |
|   4460 | 3678 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3679 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3680 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3681 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3682 | `		return PH7_OK;` |
|      - | 3683 | `	}` |
|      - | 3684 | `	/* Extract the file path */` |
|   4460 | 3685 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3686 | `	/* Point to the target IO stream device */` |
|   4460 | 3687 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   4460 | 3688 | `	if( pStream == 0 ){` |
|    ! 0 | 3689 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3690 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3691 | `		return PH7_OK;` |
|      - | 3692 | `	}` |
|   4460 | 3693 | `	nMaxlen = -1;` |
|   4460 | 3694 | `	if( nArg > 1 ){` |
|      5 | 3695 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|      2 | 3696 | `	}` |
|      - | 3697 | `	/* Try to open the file in read-only mode */` |
|   4460 | 3698 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|   4460 | 3699 | `	if( pHandle == 0 ){` |
|    ! 0 | 3700 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3701 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3702 | `		return PH7_OK;` |
|      - | 3703 | `	}` |
|   4460 | 3704 | `	if( nArg > 3 ){` |
|      - | 3705 | `		/* Extract the offset */` |
|      5 | 3706 | `		n = ph7_value_to_int64(apArg[3]);` |
|      5 | 3707 | `		if( n > 0 ){` |
|    ! 0 | 3708 | `			if( pStream->xSeek ){` |
|      - | 3709 | `				/* Seek to the desired offset */` |
|    ! 0 | 3710 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|    ! 0 | 3711 | `			}` |
|    ! 0 | 3712 | `		}` |
|      5 | 3713 | `		if( nArg > 4 ){` |
|      - | 3714 | `			/* Maximum data to read */` |
|      5 | 3715 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|      2 | 3716 | `		}` |
|      2 | 3717 | `	}` |
|      - | 3718 | `	/* Perform the requested operation */` |
|   4460 | 3719 | `	nRead = 0;` |
|   4457 | 3720 | `	for(;;){` |
|  13376 | 3721 | `		n = pStream->xRead(pHandle,zBuf,` |
|   4460 | 3722 | `			(nMaxlen > 0 && (nMaxlen < (ph7_int64)sizeof(zBuf))) ? nMaxlen : (ph7_int64)sizeof(zBuf));` |
|   8916 | 3723 | `		if( n < 1 ){` |
|      - | 3724 | `			/* EOF or IO error,break immediately */` |
|   4458 | 3725 | `			break;` |
|      - | 3726 | `		}` |
|      - | 3727 | `		/* Append data */` |
|   4460 | 3728 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 3729 | `		/* Increment read counter */` |
|   4460 | 3730 | `		nRead += n;` |
|   4460 | 3731 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 3732 | `			/* Read limit reached */` |
|      3 | 3733 | `			break;` |
|      - | 3734 | `		}` |
|      2 | 3735 | `	}` |
|      - | 3736 | `	/* Close the stream */` |
|   4460 | 3737 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3738 | `	/* Check if we have read something */` |
|   4460 | 3739 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 3740 | `		/* Nothing read,return FALSE */` |
|    ! 0 | 3741 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3742 | `	}` |
|   4460 | 3743 | `	return PH7_OK;` |
|   2231 | 3744 |  |
|      - | 3745 | `/*` |
|      - | 3746 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 3747 | ` *  Write a string to a file.` |
|      - | 3748 | ` * Parameters` |
|      - | 3749 | ` *  $filename` |
|      - | 3750 | ` *  Path to the file where to write the data.` |
|      - | 3751 | ` * $data` |
|      - | 3752 | ` *  The data to write(Must be a string).` |
|      - | 3753 | ` * $flags` |
|      - | 3754 | ` *  The value of flags can be any combination of the following` |
|      - | 3755 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 3756 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 3757 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 3758 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 3759 | ` * context` |
|      - | 3760 | ` *  A context stream resource.` |
|      - | 3761 | ` * Return` |
|      - | 3762 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 3763 | ` */` |
|  10584 | 3764 | `static int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3765 |  |
|  10586 | 3766 | `	int use_include  = FALSE;` |
|      - | 3767 | `	const ph7_io_stream *pStream;` |
|      - | 3768 | `	const char *zFile;` |
|      - | 3769 | `	const char *zData;` |
|      - | 3770 | `	int iOpenFlags;` |
|      - | 3771 | `	void *pHandle;` |
|      - | 3772 | `	int iFlags;` |
|      - | 3773 | `	int nLen;` |
|      - | 3774 |  |
|  10586 | 3775 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3776 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3777 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3778 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3779 | `		return PH7_OK;` |
|      - | 3780 | `	}` |
|      - | 3781 | `	/* Extract the file path */` |
|  10586 | 3782 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3783 | `	/* Point to the target IO stream device */` |
|  10586 | 3784 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  10586 | 3785 | `	if( pStream == 0 ){` |
|    ! 0 | 3786 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3787 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3788 | `		return PH7_OK;` |
|      - | 3789 | `	}` |
|      - | 3790 | `	/* Data to write */` |
|  10586 | 3791 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 3792 | `	/* Try to open the file in read-write mode */` |
|  10586 | 3793 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 3794 | `	/* Extract the flags */` |
|  10586 | 3795 | `	iFlags = 0;` |
|  10586 | 3796 | `	if( nArg > 2 ){` |
|    ! 0 | 3797 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3798 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 3799 | `			use_include = TRUE;` |
|    ! 0 | 3800 | `		}` |
|    ! 0 | 3801 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 3802 | `			/* If the file already exists, append the data to the file` |
|      - | 3803 | `			 * instead of overwriting it.` |
|      - | 3804 | `			 */` |
|    ! 0 | 3805 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 3806 | `			/* Append mode */` |
|    ! 0 | 3807 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 3808 | `		}` |
|    ! 0 | 3809 | `	}` |
|  15878 | 3810 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   5292 | 3811 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|  10586 | 3812 | `	if( pHandle == 0 ){` |
|    ! 0 | 3813 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3814 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3815 | `		return PH7_OK;` |
|      - | 3816 | `	}` |
|  10586 | 3817 | `	if( nLen < 1 ){` |
|      - | 3818 | `		/* Empty data, file is created/truncated */` |
|      7 | 3819 | `		ph7_result_int64(pCtx,0);` |
|      7 | 3820 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|      7 | 3821 | `		return PH7_OK;` |
|      - | 3822 | `	}` |
|  10580 | 3823 | `	if( pStream->xWrite ){` |
|      - | 3824 | `		ph7_int64 n;` |
|  10580 | 3825 | `		if( (iFlags & 0x01/* LOCK_EX */) && pStream->xLock ){` |
|      - | 3826 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 3827 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 3828 | `		}` |
|      - | 3829 | `		/* Perform the write operation */` |
|  10580 | 3830 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  10580 | 3831 | `		if( n < 0 ){` |
|      - | 3832 | `			/* IO error,return FALSE */` |
|    ! 0 | 3833 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3834 | `		}else{` |
|      - | 3835 | `			/* Total number of bytes written */` |
|  10580 | 3836 | `			ph7_result_int64(pCtx,n);` |
|      - | 3837 | `		}` |
|   5291 | 3838 | `	}else{` |
|      - | 3839 | `		/* Read-only stream */` |
|    ! 0 | 3840 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 3841 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 3842 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 3843 | `			);` |
|    ! 0 | 3844 | `		ph7_result_bool(pCtx,0);` |
|      - | 3845 | `	}` |
|      - | 3846 | `	/* Close the handle */` |
|  10580 | 3847 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  10580 | 3848 | `	return PH7_OK;` |
|   5294 | 3849 |  |
|      - | 3850 | `/*` |
|      - | 3851 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 3852 | ` *  Reads entire file into an array.` |
|      - | 3853 | ` * Parameters` |
|      - | 3854 | ` *  $filename` |
|      - | 3855 | ` *   The filename being read.` |
|      - | 3856 | ` *  $flags` |
|      - | 3857 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 3858 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 3859 | ` *       Search for the file in the include_path.` |
|      - | 3860 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 3861 | ` *       Do not add newline at the end of each array element` |
|      - | 3862 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 3863 | ` *       Skip empty lines` |
|      - | 3864 | ` *  $context` |
|      - | 3865 | ` *   A context stream resource.` |
|      - | 3866 | ` * Return` |
|      - | 3867 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3868 | ` */` |
|      8 | 3869 | `static int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3870 |  |
|      - | 3871 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 3872 | `	ph7_value *pArray,*pLine;` |
|      - | 3873 | `	const ph7_io_stream *pStream;` |
|     10 | 3874 | `	int use_include = 0;` |
|      - | 3875 | `	io_private *pDev;` |
|      - | 3876 | `	ph7_int64 n;` |
|      - | 3877 | `	int iFlags;` |
|      - | 3878 | `	int nLen;` |
|      - | 3879 |  |
|     10 | 3880 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3881 | `		/* Missing/Invalid arguments,return FALSE */` |
|      5 | 3882 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|      5 | 3883 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3884 | `		return PH7_OK;` |
|      - | 3885 | `	}` |
|      - | 3886 | `	/* Extract the file path */` |
|      6 | 3887 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3888 | `	/* Point to the target IO stream device */` |
|      6 | 3889 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 3890 | `	if( pStream == 0 ){` |
|    ! 0 | 3891 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3892 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3893 | `		return PH7_OK;` |
|      - | 3894 | `	}` |
|      - | 3895 | `	/* Allocate a new IO private instance */` |
|      6 | 3896 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      6 | 3897 | `	if( pDev == 0 ){` |
|    ! 0 | 3898 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3899 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3900 | `		return PH7_OK;` |
|      - | 3901 | `	}` |
|      - | 3902 | `	/* Initialize the structure */` |
|      6 | 3903 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      6 | 3904 | `	iFlags = 0;` |
|      6 | 3905 | `	if( nArg > 1 ){` |
|    ! 0 | 3906 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 3907 | `	}` |
|      6 | 3908 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|    ! 0 | 3909 | `		use_include = TRUE;` |
|    ! 0 | 3910 | `	}` |
|      - | 3911 | `	/* Create the array and the working value */` |
|      6 | 3912 | `	pArray = ph7_context_new_array(pCtx);` |
|      6 | 3913 | `	pLine = ph7_context_new_scalar(pCtx);` |
|      6 | 3914 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 3915 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3916 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3917 | `		return PH7_OK;` |
|      - | 3918 | `	}` |
|      - | 3919 | `	/* Try to open the file in read-only mode */` |
|      6 | 3920 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 3921 | `	if( pDev->pHandle == 0 ){` |
|      3 | 3922 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|      3 | 3923 | `		ph7_result_bool(pCtx,0);` |
|      - | 3924 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 3925 | `		 * as soon we return from this function.` |
|      - | 3926 | `		 */` |
|      3 | 3927 | `		return PH7_OK;` |
|      - | 3928 | `	}` |
|      - | 3929 | `	/* Perform the requested operation */` |
|      3 | 3930 | `	for(;;){` |
|      - | 3931 | `		/* Try to extract a line */` |
|      7 | 3932 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|      7 | 3933 | `		if( n < 1 ){` |
|      - | 3934 | `			/* EOF or IO error */` |
|      3 | 3935 | `			break;` |
|      - | 3936 | `		}` |
|      - | 3937 | `		/* Reset the cursor */` |
|      5 | 3938 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 3939 | `		/* Remove line ending if requested by the caller */` |
|      5 | 3940 | `		zPtr = zBuf;` |
|      5 | 3941 | `		zEnd = &zBuf[n];` |
|      5 | 3942 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 3943 | `			/* Ignore trailig lines */` |
|    ! 0 | 3944 | `			while( zPtr < zEnd && (zEnd[-1] == '\n'` |
|      - | 3945 | `#ifdef __WINNT__` |
|      - | 3946 | `				\|\| zEnd[-1] == '\r'` |
|      - | 3947 | `#endif` |
|      - | 3948 | `				)){` |
|    ! 0 | 3949 | `					n--;` |
|    ! 0 | 3950 | `					zEnd--;` |
|    ! 0 | 3951 | `			}` |
|    ! 0 | 3952 | `		}` |
|      5 | 3953 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 3954 | `			/* Ignore empty lines */` |
|    ! 0 | 3955 | `			while( zPtr < zEnd && (unsigned char)zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) ){` |
|    ! 0 | 3956 | `				zPtr++;` |
|    ! 0 | 3957 | `			}` |
|    ! 0 | 3958 | `			if( zPtr >= zEnd ){` |
|      - | 3959 | `				/* Empty line */` |
|    ! 0 | 3960 | `				continue;` |
|      - | 3961 | `			}` |
|    ! 0 | 3962 | `		}` |
|      5 | 3963 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 3964 | `		/* Insert line */` |
|      5 | 3965 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 3966 | `	}` |
|      - | 3967 | `	/* Close the stream */` |
|      3 | 3968 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 3969 | `	/* Release the io_private instance */` |
|      3 | 3970 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 3971 | `	/* Return the created array */` |
|      3 | 3972 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3973 | `	return PH7_OK;` |
|      6 | 3974 |  |
|      - | 3975 | `/*` |
|      - | 3976 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 3977 | ` *  Makes a copy of the file source to dest.` |
|      - | 3978 | ` * Parameters` |
|      - | 3979 | ` *  $source` |
|      - | 3980 | ` *   Path to the source file.` |
|      - | 3981 | ` *  $dest` |
|      - | 3982 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 3983 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 3984 | ` *  $context` |
|      - | 3985 | ` *   A context stream resource.` |
|      - | 3986 | ` * Return` |
|      - | 3987 | ` *  TRUE on success or FALSE on failure.` |
|      - | 3988 | ` */` |
|     10 | 3989 | `static int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3990 |  |
|      - | 3991 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 3992 | `	const char *zFile;` |
|      - | 3993 | `	char zBuf[8192];` |
|      - | 3994 | `	void *pIn,*pOut;` |
|      - | 3995 | `	ph7_int64 n;` |
|      - | 3996 | `	int nLen;` |
|     12 | 3997 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 3998 | `		/* Missing/Invalid arguments,return FALSE */` |
|      7 | 3999 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|      7 | 4000 | `		ph7_result_bool(pCtx,0);` |
|      7 | 4001 | `		return PH7_OK;` |
|      - | 4002 | `	}` |
|      - | 4003 | `	/* Extract the source name */` |
|      6 | 4004 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4005 | `	/* Point to the target IO stream device */` |
|      6 | 4006 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 4007 | `	if( pSin == 0 ){` |
|    ! 0 | 4008 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4009 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4010 | `		return PH7_OK;` |
|      - | 4011 | `	}` |
|      - | 4012 | `	/* Try to open the source file in a read-only mode */` |
|      6 | 4013 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 4014 | `	if( pIn == 0 ){` |
|      3 | 4015 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening source: '%s'",zFile);` |
|      3 | 4016 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4017 | `		return PH7_OK;` |
|      - | 4018 | `	}` |
|      - | 4019 | `	/* Extract the destination name */` |
|      3 | 4020 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 4021 | `	/* Point to the target IO stream device */` |
|      3 | 4022 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4023 | `	if( pSout == 0 ){` |
|    ! 0 | 4024 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4025 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4026 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4027 | `		return PH7_OK;` |
|      - | 4028 | `	}` |
|      3 | 4029 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 4030 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4031 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4032 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 4033 | `			);` |
|    ! 0 | 4034 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4035 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4036 | `		return PH7_OK;` |
|      - | 4037 | `	}` |
|      - | 4038 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 4039 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 4040 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 4041 | `	if( pOut == 0 ){` |
|    ! 0 | 4042 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening destination: '%s'",zFile);` |
|    ! 0 | 4043 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4044 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4045 | `		return PH7_OK;` |
|      - | 4046 | `	}` |
|      - | 4047 | `	/* Perform the requested operation */` |
|      2 | 4048 | `	for(;;){` |
|      - | 4049 | `		/* Read from source */` |
|      5 | 4050 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 4051 | `		if( n < 1 ){` |
|      - | 4052 | `			/* EOF or IO error,break immediately */` |
|      3 | 4053 | `			break;` |
|      - | 4054 | `		}` |
|      - | 4055 | `		/* Write to dest */` |
|      3 | 4056 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 4057 | `		if( n < 1 ){` |
|      - | 4058 | `			/* IO error,break immediately */` |
|    ! 0 | 4059 | `			break;` |
|      - | 4060 | `		}` |
|      1 | 4061 | `	}` |
|      - | 4062 | `	/* Close the streams */` |
|      3 | 4063 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 4064 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 4065 | `	/* Return TRUE */` |
|      3 | 4066 | `	ph7_result_bool(pCtx,1);` |
|      3 | 4067 | `	return PH7_OK;` |
|      7 | 4068 |  |
|      - | 4069 | `/*` |
|      - | 4070 | ` * array fstat(resource $handle)` |
|      - | 4071 | ` *  Gets information about a file using an open file pointer.` |
|      - | 4072 | ` * Parameters` |
|      - | 4073 | ` *  $handle` |
|      - | 4074 | ` *   The file pointer.` |
|      - | 4075 | ` * Return` |
|      - | 4076 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 4077 | ` */` |
|      2 | 4078 | `static int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4079 |  |
|      - | 4080 | `	ph7_value *pArray,*pValue;` |
|      - | 4081 | `	const ph7_io_stream *pStream;` |
|      - | 4082 | `	io_private *pDev;` |
|      3 | 4083 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4084 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4085 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4086 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4087 | `		return PH7_OK;` |
|      - | 4088 | `	}` |
|      - | 4089 | `	/* Extract our private data */` |
|      3 | 4090 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4091 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4092 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4093 | `		/* Expecting an IO handle */` |
|    ! 0 | 4094 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4095 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4096 | `		return PH7_OK;` |
|      - | 4097 | `	}` |
|      - | 4098 | `	/* Point to the target IO stream device */` |
|      3 | 4099 | `	pStream = pDev->pStream;` |
|      3 | 4100 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 4101 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4102 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4103 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4104 | `			);` |
|    ! 0 | 4105 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4106 | `		return PH7_OK;` |
|      - | 4107 | `	}` |
|      - | 4108 | `	/* Create the array and the working value */` |
|      3 | 4109 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4110 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4111 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4112 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4113 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4114 | `		return PH7_OK;` |
|      - | 4115 | `	}` |
|      - | 4116 | `	/* Perform the requested operation */` |
|      3 | 4117 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 4118 | `	/* Return the freshly created array */` |
|      3 | 4119 | `	ph7_result_value(pCtx,pArray);` |
|      - | 4120 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 4121 | `	 * released automatically as soon we return from this function.` |
|      - | 4122 | `	 */` |
|      3 | 4123 | `	return PH7_OK;` |
|      2 | 4124 |  |
|      - | 4125 | `/*` |
|      - | 4126 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 4127 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 4128 | ` * Parameters` |
|      - | 4129 | ` *  $handle` |
|      - | 4130 | ` *   The file pointer.` |
|      - | 4131 | ` *  $string` |
|      - | 4132 | ` *   The string that is to be written.` |
|      - | 4133 | ` *  $length` |
|      - | 4134 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 4135 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 4136 | ` * Return` |
|      - | 4137 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 4138 | ` */` |
|      6 | 4139 | `static int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4140 |  |
|      - | 4141 | `	const ph7_io_stream *pStream;` |
|      - | 4142 | `	const char *zString;` |
|      - | 4143 | `	io_private *pDev;` |
|      - | 4144 | `	int nLen,n;` |
|      7 | 4145 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4146 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4147 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4148 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4149 | `		return PH7_OK;` |
|      - | 4150 | `	}` |
|      - | 4151 | `	/* Extract our private data */` |
|      7 | 4152 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4153 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 4154 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4155 | `		/* Expecting an IO handle */` |
|    ! 0 | 4156 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4157 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4158 | `		return PH7_OK;` |
|      - | 4159 | `	}` |
|      - | 4160 | `	/* Point to the target IO stream device */` |
|      7 | 4161 | `	pStream = pDev->pStream;` |
|      7 | 4162 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4163 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4164 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4165 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4166 | `			);` |
|    ! 0 | 4167 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4168 | `		return PH7_OK;` |
|      - | 4169 | `	}` |
|      - | 4170 | `	/* Extract the data to write */` |
|      7 | 4171 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|      7 | 4172 | `	if( nArg > 2 ){` |
|      - | 4173 | `		/* Maximum data length to write */` |
|    ! 0 | 4174 | `		n = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 4175 | `		if( n >= 0 && n < nLen ){` |
|    ! 0 | 4176 | `			nLen = n;` |
|    ! 0 | 4177 | `		}` |
|    ! 0 | 4178 | `	}` |
|      7 | 4179 | `	if( nLen < 1 ){` |
|      - | 4180 | `		/* Nothing to write */` |
|    ! 0 | 4181 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4182 | `		return PH7_OK;` |
|      - | 4183 | `	}` |
|      - | 4184 | `	/* Perform the requested operation */` |
|      7 | 4185 | `	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);` |
|      7 | 4186 | `	if( n <  0 ){` |
|      - | 4187 | `		/* IO error,return FALSE */` |
|    ! 0 | 4188 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4189 | `	}else{` |
|      - | 4190 | `		/* #Bytes written */` |
|      7 | 4191 | `		ph7_result_int(pCtx,n);` |
|      - | 4192 | `	}` |
|      7 | 4193 | `	return PH7_OK;` |
|      4 | 4194 |  |
|      - | 4195 | `/*` |
|      - | 4196 | ` * bool flock(resource $handle,int $operation)` |
|      - | 4197 | ` *  Portable advisory file locking.` |
|      - | 4198 | ` * Parameters` |
|      - | 4199 | ` *  $handle` |
|      - | 4200 | ` *   The file pointer.` |
|      - | 4201 | ` *  $operation` |
|      - | 4202 | ` *   operation is one of the following:` |
|      - | 4203 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 4204 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 4205 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 4206 | ` * Return` |
|      - | 4207 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 4208 | ` */` |
|      4 | 4209 | `static int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4210 |  |
|      - | 4211 | `	const ph7_io_stream *pStream;` |
|      - | 4212 | `	io_private *pDev;` |
|      - | 4213 | `	int nLock;` |
|      - | 4214 | `	int rc;` |
|      5 | 4215 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4216 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4217 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4218 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4219 | `		return PH7_OK;` |
|      - | 4220 | `	}` |
|      - | 4221 | `	/* Extract our private data */` |
|      5 | 4222 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4223 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 4224 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4225 | `		/*Expecting an IO handle */` |
|    ! 0 | 4226 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4227 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4228 | `		return PH7_OK;` |
|      - | 4229 | `	}` |
|      - | 4230 | `	/* Point to the target IO stream device */` |
|      5 | 4231 | `	pStream = pDev->pStream;` |
|      5 | 4232 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|    ! 0 | 4233 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4234 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4235 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4236 | `			);` |
|    ! 0 | 4237 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4238 | `		return PH7_OK;` |
|      - | 4239 | `	}` |
|      - | 4240 | `	/* Requested lock operation */` |
|      5 | 4241 | `	nLock = ph7_value_to_int(apArg[1]);` |
|      - | 4242 | `	/* Lock operation */` |
|      5 | 4243 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 4244 | `	/* IO result */` |
|      5 | 4245 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 4246 | `	return PH7_OK;` |
|      3 | 4247 |  |
|      - | 4248 | `/*` |
|      - | 4249 | ` * int fpassthru(resource $handle)` |
|      - | 4250 | ` *  Output all remaining data on a file pointer.` |
|      - | 4251 | ` * Parameters` |
|      - | 4252 | ` *  $handle` |
|      - | 4253 | ` *   The file pointer.` |
|      - | 4254 | ` * Return` |
|      - | 4255 | ` *  Total number of characters read from handle and passed through` |
|      - | 4256 | ` *  to the output on success or FALSE on failure.` |
|      - | 4257 | ` */` |
|      2 | 4258 | `static int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4259 |  |
|      - | 4260 | `	const ph7_io_stream *pStream;` |
|      - | 4261 | `	io_private *pDev;` |
|      - | 4262 | `	ph7_int64 n,nRead;` |
|      - | 4263 | `	char zBuf[8192];` |
|      - | 4264 | `	int rc;` |
|      3 | 4265 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4266 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4267 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4268 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4269 | `		return PH7_OK;` |
|      - | 4270 | `	}` |
|      - | 4271 | `	/* Extract our private data */` |
|      3 | 4272 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4273 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4274 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4275 | `		/*Expecting an IO handle */` |
|    ! 0 | 4276 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4277 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4278 | `		return PH7_OK;` |
|      - | 4279 | `	}` |
|      - | 4280 | `	/* Point to the target IO stream device */` |
|      3 | 4281 | `	pStream = pDev->pStream;` |
|      3 | 4282 | `	if( pStream == 0  ){` |
|    ! 0 | 4283 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4284 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4285 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4286 | `			);` |
|    ! 0 | 4287 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4288 | `		return PH7_OK;` |
|      - | 4289 | `	}` |
|      - | 4290 | `	/* Perform the requested operation */` |
|      3 | 4291 | `	nRead = 0;` |
|      2 | 4292 | `	for(;;){` |
|      5 | 4293 | `		n = StreamRead(pDev,zBuf,sizeof(zBuf));` |
|      5 | 4294 | `		if( n < 1 ){` |
|      - | 4295 | `			/* Error or EOF */` |
|      3 | 4296 | `			break;` |
|      - | 4297 | `		}` |
|      - | 4298 | `		/* Increment the read counter */` |
|      3 | 4299 | `		nRead += n;` |
|      - | 4300 | `		/* Output data */` |
|      3 | 4301 | `		rc = ph7_context_output(pCtx,zBuf,(int)nRead /* FIXME: 64-bit issues */);` |
|      3 | 4302 | `		if( rc == PH7_ABORT ){` |
|      - | 4303 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 4304 | `			break;` |
|      - | 4305 | `		}` |
|      1 | 4306 | `	}` |
|      - | 4307 | `	/* Total number of bytes readen */` |
|      3 | 4308 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 4309 | `	return PH7_OK;` |
|      2 | 4310 |  |
|      - | 4311 | `/* CSV reader/writer private data */` |
|      - | 4312 | `struct csv_data` |
|      - | 4313 |  |
|      - | 4314 | `	int delimiter;    /* Delimiter. Default ',' */` |
|      - | 4315 | `	int enclosure;    /* Enclosure. Default '"'*/` |
|      - | 4316 | `	io_private *pDev; /* Open stream handle */` |
|      - | 4317 | `	int iCount;       /* Counter */` |
|      - | 4318 | `};` |
|      - | 4319 | `/*` |
|      - | 4320 | ` * The following callback is used by the fputcsv() function inorder to iterate` |
|      - | 4321 | ` * throw array entries and output CSV data based on the current key and it's` |
|      - | 4322 | ` * associated data.` |
|      - | 4323 | ` */` |
|      6 | 4324 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 4325 |  |
|      7 | 4326 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 4327 | `	const char *zData;` |
|      - | 4328 | `	int nLen,c2;` |
|      - | 4329 | `	sxu32 n;` |
|      - | 4330 | `	/* Point to the raw data */` |
|      7 | 4331 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      7 | 4332 | `	if( nLen < 1 ){` |
|      - | 4333 | `		/* Nothing to write */` |
|    ! 0 | 4334 | `		return PH7_OK;` |
|      - | 4335 | `	}` |
|      7 | 4336 | `	if( pData->iCount > 0 ){` |
|      - | 4337 | `		/* Write the delimiter */` |
|      5 | 4338 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->delimiter,sizeof(char));` |
|      2 | 4339 | `	}` |
|      7 | 4340 | `	n = 1;` |
|      7 | 4341 | `	c2 = 0;` |
|     10 | 4342 | `	if( SyByteFind(zData,(sxu32)nLen,pData->delimiter,0) == SXRET_OK \|\|` |
|      6 | 4343 | `		SyByteFind(zData,(sxu32)nLen,pData->enclosure,&n) == SXRET_OK ){` |
|    ! 0 | 4344 | `			c2 = 1;` |
|    ! 0 | 4345 | `			if( n == 0 ){` |
|    ! 0 | 4346 | `				c2 = 2;` |
|    ! 0 | 4347 | `			}` |
|      - | 4348 | `			/* Write the enclosure */` |
|    ! 0 | 4349 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4350 | `			if( c2 > 1 ){` |
|    ! 0 | 4351 | `				pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4352 | `			}` |
|    ! 0 | 4353 | `	}` |
|      - | 4354 | `	/* Write the data */` |
|      7 | 4355 | `	if( pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)zData,(ph7_int64)nLen) < 1 ){` |
|    ! 0 | 4356 | `		SXUNUSED(pKey); /* cc warning */` |
|    ! 0 | 4357 | `		return PH7_ABORT;` |
|      - | 4358 | `	}` |
|      7 | 4359 | `	if( c2 > 0 ){` |
|      - | 4360 | `		/* Write the enclosure */` |
|    ! 0 | 4361 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4362 | `		if( c2 > 1 ){` |
|    ! 0 | 4363 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4364 | `		}` |
|    ! 0 | 4365 | `	}` |
|      7 | 4366 | `	pData->iCount++;` |
|      7 | 4367 | `	return PH7_OK;` |
|      4 | 4368 |  |
|      - | 4369 | `/*` |
|      - | 4370 | ` * int fputcsv(resource $handle,array $fields[,string $delimiter = ','[,string $enclosure = '"' ]])` |
|      - | 4371 | ` *  Format line as CSV and write to file pointer.` |
|      - | 4372 | ` * Parameters` |
|      - | 4373 | ` *  $handle` |
|      - | 4374 | ` *   Open file handle.` |
|      - | 4375 | ` * $fields` |
|      - | 4376 | ` *   An array of values.` |
|      - | 4377 | ` * $delimiter` |
|      - | 4378 | ` *   The optional delimiter parameter sets the field delimiter (one character only).` |
|      - | 4379 | ` * $enclosure` |
|      - | 4380 | ` *  The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 4381 | ` */` |
|      2 | 4382 | `static int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4383 |  |
|      - | 4384 | `	const ph7_io_stream *pStream;` |
|      - | 4385 | `	struct csv_data sCsv;` |
|      - | 4386 | `	io_private *pDev;` |
|      - | 4387 | `	char *zEol;` |
|      - | 4388 | `	int eolen;` |
|      3 | 4389 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4390 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4391 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|    ! 0 | 4392 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4393 | `		return PH7_OK;` |
|      - | 4394 | `	}` |
|      - | 4395 | `	/* Extract our private data */` |
|      3 | 4396 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4397 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4398 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4399 | `		/*Expecting an IO handle */` |
|    ! 0 | 4400 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4401 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4402 | `		return PH7_OK;` |
|      - | 4403 | `	}` |
|      - | 4404 | `	/* Point to the target IO stream device */` |
|      3 | 4405 | `	pStream = pDev->pStream;` |
|      3 | 4406 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4407 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4408 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4409 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4410 | `			);` |
|    ! 0 | 4411 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4412 | `		return PH7_OK;` |
|      - | 4413 | `	}` |
|      - | 4414 | `	/* Set default csv separator */` |
|      3 | 4415 | `	sCsv.delimiter = ',';` |
|      3 | 4416 | `	sCsv.enclosure = '"';` |
|      3 | 4417 | `	sCsv.pDev = pDev;` |
|      3 | 4418 | `	sCsv.iCount = 0;` |
|      3 | 4419 | `	if( nArg > 2 ){` |
|      - | 4420 | `		/* User delimiter */` |
|      - | 4421 | `		const char *z;` |
|      - | 4422 | `		int n;` |
|      3 | 4423 | `		z = ph7_value_to_string(apArg[2],&n);` |
|      3 | 4424 | `		if( n > 0 ){` |
|      3 | 4425 | `			sCsv.delimiter = z[0];` |
|      1 | 4426 | `		}` |
|      3 | 4427 | `		if( nArg > 3 ){` |
|      3 | 4428 | `			z = ph7_value_to_string(apArg[3],&n);` |
|      3 | 4429 | `			if( n > 0 ){` |
|      3 | 4430 | `				sCsv.enclosure = z[0];` |
|      1 | 4431 | `			}` |
|      1 | 4432 | `		}` |
|      1 | 4433 | `	}` |
|      - | 4434 | `	/* Iterate throw array entries and write csv data */` |
|      3 | 4435 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      - | 4436 | `	/* Write a line ending */` |
|      - | 4437 | `#ifdef __WINNT__` |
|      1 | 4438 | `	zEol = "\r\n";` |
|      1 | 4439 | `	eolen = (int)sizeof("\r\n")-1;` |
|      - | 4440 | `#else` |
|      - | 4441 | `	/* Assume UNIX LF */` |
|      2 | 4442 | `	zEol = "\n";` |
|      2 | 4443 | `	eolen = (int)sizeof(char);` |
|      - | 4444 | `#endif` |
|      3 | 4445 | `	pDev->pStream->xWrite(pDev->pHandle,(const void *)zEol,eolen);` |
|      3 | 4446 | `	return PH7_OK;` |
|      2 | 4447 |  |
|      - | 4448 | `/*` |
|      - | 4449 | ` * fprintf,vfprintf private data.` |
|      - | 4450 | ` * An instance of the following structure is passed to the formatted` |
|      - | 4451 | ` * input consumer callback defined below.` |
|      - | 4452 | ` */` |
|      - | 4453 | `typedef struct fprintf_data fprintf_data;` |
|      - | 4454 | `struct fprintf_data` |
|      - | 4455 |  |
|      - | 4456 | `	io_private *pIO;        /* IO stream */` |
|      - | 4457 | `	ph7_int64 nCount;       /* Total number of bytes written */` |
|      - | 4458 | `};` |
|      - | 4459 | `/*` |
|      - | 4460 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 4461 | ` */` |
|     38 | 4462 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4463 |  |
|     39 | 4464 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 4465 | `	ph7_int64 n;` |
|      - | 4466 | `	/* Write the formatted data */` |
|     39 | 4467 | `	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);` |
|     39 | 4468 | `	if( n < 1 ){` |
|    ! 0 | 4469 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 4470 | `		/* IO error,abort immediately */` |
|    ! 0 | 4471 | `		return SXERR_ABORT;` |
|      - | 4472 | `	}` |
|      - | 4473 | `	/* Increment counter */` |
|     39 | 4474 | `	pFdata->nCount += n;` |
|     39 | 4475 | `	return PH7_OK;` |
|     20 | 4476 |  |
|      - | 4477 | `/*` |
|      - | 4478 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 4479 | ` *  Write a formatted string to a stream.` |
|      - | 4480 | ` * Parameters` |
|      - | 4481 | ` *  $handle` |
|      - | 4482 | ` *   The file pointer.` |
|      - | 4483 | ` *  $format` |
|      - | 4484 | ` *   String format (see sprintf()).` |
|      - | 4485 | ` * Return` |
|      - | 4486 | ` *  The length of the written string.` |
|      - | 4487 | ` */` |
|     16 | 4488 | `static int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4489 |  |
|      - | 4490 | `	fprintf_data sFdata;` |
|      - | 4491 | `	const char *zFormat;` |
|      - | 4492 | `	io_private *pDev;` |
|      - | 4493 | `	int nLen;` |
|     17 | 4494 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 4495 | `		/* Missing/Invalid arguments,return zero */` |
|    ! 0 | 4496 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4497 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4498 | `		return PH7_OK;` |
|      - | 4499 | `	}` |
|      - | 4500 | `	/* Extract our private data */` |
|     17 | 4501 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4502 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     17 | 4503 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4504 | `		/*Expecting an IO handle */` |
|    ! 0 | 4505 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4506 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4507 | `		return PH7_OK;` |
|      - | 4508 | `	}` |
|      - | 4509 | `	/* Point to the target IO stream device */` |
|     17 | 4510 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4511 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4512 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4513 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4514 | `			);` |
|    ! 0 | 4515 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4516 | `		return PH7_OK;` |
|      - | 4517 | `	}` |
|      - | 4518 | `	/* Extract the string format */` |
|     17 | 4519 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     17 | 4520 | `	if( nLen < 1 ){` |
|      - | 4521 | `		/* Empty string,return zero */` |
|    ! 0 | 4522 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4523 | `		return PH7_OK;` |
|      - | 4524 | `	}` |
|      - | 4525 | `	/* Prepare our private data */` |
|     17 | 4526 | `	sFdata.nCount = 0;` |
|     17 | 4527 | `	sFdata.pIO = pDev;` |
|      - | 4528 | `	/* Format the string */` |
|     17 | 4529 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 4530 | `	/* Return total number of bytes written */` |
|     17 | 4531 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|     17 | 4532 | `	return PH7_OK;` |
|      9 | 4533 |  |
|      - | 4534 | `/*` |
|      - | 4535 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 4536 | ` *  Write a formatted string to a stream.` |
|      - | 4537 | ` * Parameters` |
|      - | 4538 | ` *  $handle` |
|      - | 4539 | ` *   The file pointer.` |
|      - | 4540 | ` *  $format` |
|      - | 4541 | ` *   String format (see sprintf()).` |
|      - | 4542 | ` * $args` |
|      - | 4543 | ` *   User arguments.` |
|      - | 4544 | ` * Return` |
|      - | 4545 | ` *  The length of the written string.` |
|      - | 4546 | ` */` |
|      4 | 4547 | `static int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4548 |  |
|      - | 4549 | `	fprintf_data sFdata;` |
|      - | 4550 | `	const char *zFormat;` |
|      - | 4551 | `	ph7_hashmap *pMap;` |
|      - | 4552 | `	io_private *pDev;` |
|      - | 4553 | `	SySet sArg;` |
|      - | 4554 | `	int n,nLen;` |
|      5 | 4555 | `	if( nArg < 3 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_string(apArg[1])  \|\| !ph7_value_is_array(apArg[2]) ){` |
|      - | 4556 | `		/* Missing/Invalid arguments,return zero */` |
|      3 | 4557 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|      3 | 4558 | `		ph7_result_int(pCtx,0);` |
|      3 | 4559 | `		return PH7_OK;` |
|      - | 4560 | `	}` |
|      - | 4561 | `	/* Extract our private data */` |
|      3 | 4562 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4563 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4564 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4565 | `		/*Expecting an IO handle */` |
|    ! 0 | 4566 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4567 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4568 | `		return PH7_OK;` |
|      - | 4569 | `	}` |
|      - | 4570 | `	/* Point to the target IO stream device */` |
|      3 | 4571 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4572 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4573 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4574 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4575 | `			);` |
|    ! 0 | 4576 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4577 | `		return PH7_OK;` |
|      - | 4578 | `	}` |
|      - | 4579 | `	/* Extract the string format */` |
|      3 | 4580 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      3 | 4581 | `	if( nLen < 1 ){` |
|      - | 4582 | `		/* Empty string,return zero */` |
|    ! 0 | 4583 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4584 | `		return PH7_OK;` |
|      - | 4585 | `	}` |
|      - | 4586 | `	/* Point to hashmap */` |
|      3 | 4587 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 4588 | `	/* Extract arguments from the hashmap */` |
|      3 | 4589 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4590 | `	/* Prepare our private data */` |
|      3 | 4591 | `	sFdata.nCount = 0;` |
|      3 | 4592 | `	sFdata.pIO = pDev;` |
|      - | 4593 | `	/* Format the string */` |
|      3 | 4594 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 4595 | `	/* Return total number of bytes written*/` |
|      3 | 4596 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      3 | 4597 | `	SySetRelease(&sArg);` |
|      3 | 4598 | `	return PH7_OK;` |
|      3 | 4599 |  |
|      - | 4600 | `/*` |
|      - | 4601 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 4602 | ` * According to the PHP reference manual:` |
|      - | 4603 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 4604 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 4605 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 4606 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 4607 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 4608 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 4609 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 4610 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 4611 | ` *         exist, attempt to create it.` |
|      - | 4612 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 4613 | ` *          not exist, attempt to create it.` |
|      - | 4614 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 4615 | ` *         already exists,` |
|      - | 4616 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 4617 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 4618 | ` *         the underlying open(2) system call.` |
|      - | 4619 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 4620 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 4621 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 4622 | ` *          is positioned on the beginning of the file.` |
|      - | 4623 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 4624 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 4625 | ` *          be used after the lock is requested).` |
|      - | 4626 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 4627 | ` */` |
|     64 | 4628 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      2 | 4629 |  |
|     66 | 4630 | `	const char *zEnd = &zMode[nLen];` |
|     66 | 4631 | `	int iFlag = 0;` |
|      - | 4632 | `	int c;` |
|     66 | 4633 | `	if( nLen < 1 ){` |
|      - | 4634 | `		/* Open in a read-only mode */` |
|    ! 0 | 4635 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 4636 | `	}` |
|     66 | 4637 | `	c = zMode[0];` |
|     66 | 4638 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 4639 | `		/* Read-only access */` |
|     40 | 4640 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|     40 | 4641 | `		zMode++; /* Advance */` |
|     40 | 4642 | `		if( zMode < zEnd ){` |
|      7 | 4643 | `			c = zMode[0];` |
|      7 | 4644 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 4645 | `				/* Read+Write access */` |
|      7 | 4646 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|      3 | 4647 | `			}` |
|      5 | 4648 | `		}` |
|     46 | 4649 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 4650 | `		/* Overwrite mode.` |
|      - | 4651 | `		 * If the file does not exists,try to create it` |
|      - | 4652 | `		 */` |
|     27 | 4653 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     27 | 4654 | `		zMode++; /* Advance */` |
|     27 | 4655 | `		if( zMode < zEnd ){` |
|      3 | 4656 | `			c = zMode[0];` |
|      3 | 4657 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4658 | `				/* Read+Write access */` |
|      3 | 4659 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|      3 | 4660 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|      1 | 4661 | `			}` |
|      2 | 4662 | `		}` |
|     13 | 4663 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 4664 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 4665 | `		 * Create the file if it does not exists.` |
|      - | 4666 | `		 */` |
|    ! 0 | 4667 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4668 | `		zMode++; /* Advance */` |
|    ! 0 | 4669 | `		if( zMode < zEnd ){` |
|    ! 0 | 4670 | `			c = zMode[0];` |
|    ! 0 | 4671 | `			if( c == '+' ){` |
|      - | 4672 | `				/* Read-Write access */` |
|    ! 0 | 4673 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4674 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4675 | `			}` |
|    ! 0 | 4676 | `		}` |
|    ! 0 | 4677 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 4678 | `		/* Exclusive access.` |
|      - | 4679 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 4680 | `		 * Otherwise create a new file.` |
|      - | 4681 | `		 */` |
|    ! 0 | 4682 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|    ! 0 | 4683 | `		zMode++; /* Advance */` |
|    ! 0 | 4684 | `		if( zMode < zEnd ){` |
|    ! 0 | 4685 | `			c = zMode[0];` |
|    ! 0 | 4686 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4687 | `				/* Read-Write access */` |
|    ! 0 | 4688 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4689 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4690 | `			}` |
|    ! 0 | 4691 | `		}` |
|    ! 0 | 4692 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 4693 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 4694 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4695 | `		zMode++; /* Advance */` |
|    ! 0 | 4696 | `		if( zMode < zEnd ){` |
|    ! 0 | 4697 | `			c = zMode[0];` |
|    ! 0 | 4698 | `			if( c == '+' ){` |
|      - | 4699 | `				/* Read-Write access */` |
|    ! 0 | 4700 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4701 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4702 | `			}` |
|    ! 0 | 4703 | `		}` |
|    ! 0 | 4704 | `	}else{` |
|      - | 4705 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 4706 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 4707 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 4708 | `	}` |
|     74 | 4709 | `	while( zMode < zEnd ){` |
|      9 | 4710 | `		c = zMode[0];` |
|      9 | 4711 | `		if( c == 'b' \|\| c == 'B' ){` |
|    ! 0 | 4712 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4713 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|      9 | 4714 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 4715 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 4716 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4717 | `		}` |
|      9 | 4718 | `		zMode++;` |
|      1 | 4719 | `	}` |
|     66 | 4720 | `	return iFlag;` |
|     34 | 4721 |  |
|      - | 4722 | `/*` |
|      - | 4723 | ` * Initialize the IO private structure.` |
|      - | 4724 | ` */` |
|   3804 | 4725 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      2 | 4726 |  |
|   3806 | 4727 | `	pOut->pStream = pStream;` |
|   3806 | 4728 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   3806 | 4729 | `	pOut->nOfft = 0;` |
|      - | 4730 | `	/* Set the magic number */` |
|   3806 | 4731 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   3806 | 4732 |  |
|      - | 4733 | `/*` |
|      - | 4734 | ` * Release the IO private structure.` |
|      - | 4735 | ` */` |
|   3776 | 4736 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      2 | 4737 |  |
|   3778 | 4738 | `	SyBlobRelease(&pDev->sBuffer);` |
|   3778 | 4739 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 4740 | `	/* Release the whole structure */` |
|   3778 | 4741 | `	ph7_context_free_chunk(pCtx,pDev);` |
|   3778 | 4742 |  |
|      - | 4743 | `/*` |
|      - | 4744 | ` * Reset the IO private structure.` |
|      - | 4745 | ` */` |
|     12 | 4746 | `static void ResetIOPrivate(io_private *pDev)` |
|      1 | 4747 |  |
|     13 | 4748 | `	SyBlobReset(&pDev->sBuffer);` |
|     13 | 4749 | `	pDev->nOfft = 0;` |
|     13 | 4750 |  |
|      - | 4751 | `/* Forward declaration */` |
|      - | 4752 | `static int is_php_stream(const ph7_io_stream *pStream);` |
|      - | 4753 | `/*` |
|      - | 4754 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 4755 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 4756 | ` * Parameters` |
|      - | 4757 | ` *  $filename` |
|      - | 4758 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 4759 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 4760 | ` *   then a regular file is assumed.` |
|      - | 4761 | ` *  $mode` |
|      - | 4762 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 4763 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 4764 | ` *   modes.` |
|      - | 4765 | ` *  $use_include_path` |
|      - | 4766 | ` *   You can use the optional second parameter and set it to` |
|      - | 4767 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 4768 | ` *  $context` |
|      - | 4769 | ` *   A context stream resource.` |
|      - | 4770 | ` * Return` |
|      - | 4771 | ` *  File handle on success or FALSE on failure.` |
|      - | 4772 | ` */` |
|     64 | 4773 | `static int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4774 |  |
|      - | 4775 | `	const ph7_io_stream *pStream;` |
|      - | 4776 | `	const char *zUri,*zMode;` |
|      - | 4777 | `	ph7_value *pResource;` |
|      - | 4778 | `	io_private *pDev;` |
|      - | 4779 | `	int iLen,imLen;` |
|      - | 4780 | `	int iOpenFlags;` |
|     66 | 4781 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4782 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4783 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 4784 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4785 | `		return PH7_OK;` |
|      - | 4786 | `	}` |
|      - | 4787 | `	/* Extract the URI and the desired access mode */` |
|     66 | 4788 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|     66 | 4789 | `	if( nArg > 1 ){` |
|     66 | 4790 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|     34 | 4791 | `	}else{` |
|      - | 4792 | `		/* Set a default read-only mode */` |
|    ! 0 | 4793 | `		zMode = "r";` |
|    ! 0 | 4794 | `		imLen = (int)sizeof(char);` |
|      - | 4795 | `	}` |
|      - | 4796 | `	/* Try to extract a stream */` |
|     66 | 4797 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|     66 | 4798 | `	if( pStream == 0 ){` |
|    ! 0 | 4799 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 4800 | `			"No stream device is associated with the given URI(%s)",zUri);` |
|    ! 0 | 4801 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4802 | `		return PH7_OK;` |
|      - | 4803 | `	}` |
|      - | 4804 | `	/* Allocate a new IO private instance */` |
|     66 | 4805 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     66 | 4806 | `	if( pDev == 0 ){` |
|    ! 0 | 4807 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4808 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4809 | `		return PH7_OK;` |
|      - | 4810 | `	}` |
|     66 | 4811 | `	pResource = 0;` |
|     66 | 4812 | `	if( nArg > 3 ){` |
|    ! 0 | 4813 | `		pResource = apArg[3];` |
|     66 | 4814 | `	}else if( is_php_stream(pStream) ){` |
|      - | 4815 | `		/* TICKET 1433-80: The php:// stream need a ph7_value to access the underlying` |
|      - | 4816 | `		 * virtual machine.` |
|      - | 4817 | `		 */` |
|      3 | 4818 | `		pResource = apArg[0];` |
|      1 | 4819 | `	}` |
|      - | 4820 | `	/* Initialize the structure */` |
|     66 | 4821 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 4822 | `	/* Convert open mode to PH7 flags */` |
|     66 | 4823 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|      - | 4824 | `	/* Try to get a handle */` |
|     98 | 4825 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|     32 | 4826 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);` |
|     66 | 4827 | `	if( pDev->pHandle == 0 ){` |
|    ! 0 | 4828 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zUri);` |
|    ! 0 | 4829 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4830 | `		ph7_context_free_chunk(pCtx,pDev);` |
|    ! 0 | 4831 | `		return PH7_OK;` |
|      - | 4832 | `	}` |
|      - | 4833 | `	/* All done,return the io_private instance as a resource */` |
|     66 | 4834 | `	ph7_result_resource(pCtx,pDev);` |
|     66 | 4835 | `	return PH7_OK;` |
|     34 | 4836 |  |
|      - | 4837 | `/*` |
|      - | 4838 | ` * bool fclose(resource $handle)` |
|      - | 4839 | ` *  Closes an open file pointer` |
|      - | 4840 | ` * Parameters` |
|      - | 4841 | ` *  $handle` |
|      - | 4842 | ` *   The file pointer.` |
|      - | 4843 | ` * Return` |
|      - | 4844 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4845 | ` */` |
|    134 | 4846 | `static int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4847 |  |
|      - | 4848 | `	const ph7_io_stream *pStream;` |
|      - | 4849 | `	io_private *pDev;` |
|      - | 4850 | `	ph7_vm *pVm;` |
|    136 | 4851 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4852 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4853 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4854 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4855 | `		return PH7_OK;` |
|      - | 4856 | `	}` |
|      - | 4857 | `	/* Extract our private data */` |
|    136 | 4858 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4859 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    136 | 4860 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4861 | `		/*Expecting an IO handle */` |
|    ! 0 | 4862 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4863 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4864 | `		return PH7_OK;` |
|      - | 4865 | `	}` |
|      - | 4866 | `	/* Point to the target IO stream device */` |
|    136 | 4867 | `	pStream = pDev->pStream;` |
|    136 | 4868 | `	if( pStream == 0 ){` |
|    ! 0 | 4869 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4870 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4871 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4872 | `			);` |
|    ! 0 | 4873 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4874 | `		return PH7_OK;` |
|      - | 4875 | `	}` |
|      - | 4876 | `	/* Point to the VM that own this context */` |
|    136 | 4877 | `	pVm = pCtx->pVm;` |
|      - | 4878 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|    136 | 4879 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 4880 | `		/* Perform the requested operation */` |
|    136 | 4881 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 4882 | `		/* Release the IO private structure */` |
|    136 | 4883 | `		ReleaseIOPrivate(pCtx,pDev);` |
|      - | 4884 | `		/* Invalidate the resource handle */` |
|    136 | 4885 | `		ph7_value_release(apArg[0]);` |
|     67 | 4886 | `	}` |
|      - | 4887 | `	/* Return TRUE */` |
|    136 | 4888 | `	ph7_result_bool(pCtx,1);` |
|    136 | 4889 | `	return PH7_OK;` |
|     69 | 4890 |  |
|      - | 4891 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4892 | `/*` |
|      - | 4893 | ` * MD5/SHA1 digest consumer.` |
|      - | 4894 | ` */` |
|     72 | 4895 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 4896 |  |
|      - | 4897 | `	/* Append hex chunk verbatim */` |
|     73 | 4898 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     73 | 4899 | `	return SXRET_OK;` |
|      1 | 4900 |  |
|      - | 4901 | `/*` |
|      - | 4902 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 4903 | ` *  Calculates the md5 hash of a given file.` |
|      - | 4904 | ` * Parameters` |
|      - | 4905 | ` *  $uri` |
|      - | 4906 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 4907 | ` *  $raw_output` |
|      - | 4908 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 4909 | ` * Return` |
|      - | 4910 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 4911 | ` */` |
|      2 | 4912 | `static int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4913 |  |
|      - | 4914 | `	const ph7_io_stream *pStream;` |
|      - | 4915 | `	unsigned char zDigest[16];` |
|      3 | 4916 | `	int raw_output  = FALSE;` |
|      - | 4917 | `	const char *zFile;` |
|      - | 4918 | `	MD5Context sCtx;` |
|      - | 4919 | `	char zBuf[8192];` |
|      - | 4920 | `	void *pHandle;` |
|      - | 4921 | `	ph7_int64 n;` |
|      - | 4922 | `	int nLen;` |
|      3 | 4923 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4924 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4925 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 4926 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4927 | `		return PH7_OK;` |
|      - | 4928 | `	}` |
|      - | 4929 | `	/* Extract the file path */` |
|      3 | 4930 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4931 | `	/* Point to the target IO stream device */` |
|      3 | 4932 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4933 | `	if( pStream == 0 ){` |
|    ! 0 | 4934 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4935 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4936 | `		return PH7_OK;` |
|      - | 4937 | `	}` |
|      3 | 4938 | `	if( nArg > 1 ){` |
|    ! 0 | 4939 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 4940 | `	}` |
|      - | 4941 | `	/* Try to open the file in read-only mode */` |
|      3 | 4942 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 4943 | `	if( pHandle == 0 ){` |
|    ! 0 | 4944 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 4945 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4946 | `		return PH7_OK;` |
|      - | 4947 | `	}` |
|      - | 4948 | `	/* Init the MD5 context */` |
|      3 | 4949 | `	MD5Init(&sCtx);` |
|      - | 4950 | `	/* Perform the requested operation */` |
|      2 | 4951 | `	for(;;){` |
|      5 | 4952 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 4953 | `		if( n < 1 ){` |
|      - | 4954 | `			/* EOF or IO error,break immediately */` |
|      3 | 4955 | `			break;` |
|      - | 4956 | `		}` |
|      3 | 4957 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 4958 | `	}` |
|      - | 4959 | `	/* Close the stream */` |
|      3 | 4960 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 4961 | `	/* Extract the digest */` |
|      3 | 4962 | `	MD5Final(zDigest,&sCtx);` |
|      3 | 4963 | `	if( raw_output ){` |
|      - | 4964 | `		/* Output raw digest */` |
|    ! 0 | 4965 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 4966 | `	}else{` |
|      - | 4967 | `		/* Perform a binary to hex conversion */` |
|      3 | 4968 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 4969 | `	}` |
|      3 | 4970 | `	return PH7_OK;` |
|      2 | 4971 |  |
|      - | 4972 | `/*` |
|      - | 4973 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 4974 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 4975 | ` * Parameters` |
|      - | 4976 | ` *  $uri` |
|      - | 4977 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 4978 | ` *  $raw_output` |
|      - | 4979 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 4980 | ` * Return` |
|      - | 4981 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 4982 | ` */` |
|      2 | 4983 | `static int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4984 |  |
|      - | 4985 | `	const ph7_io_stream *pStream;` |
|      - | 4986 | `	unsigned char zDigest[20];` |
|      3 | 4987 | `	int raw_output  = FALSE;` |
|      - | 4988 | `	const char *zFile;` |
|      - | 4989 | `	SHA1Context sCtx;` |
|      - | 4990 | `	char zBuf[8192];` |
|      - | 4991 | `	void *pHandle;` |
|      - | 4992 | `	ph7_int64 n;` |
|      - | 4993 | `	int nLen;` |
|      3 | 4994 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4995 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4996 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 4997 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4998 | `		return PH7_OK;` |
|      - | 4999 | `	}` |
|      - | 5000 | `	/* Extract the file path */` |
|      3 | 5001 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5002 | `	/* Point to the target IO stream device */` |
|      3 | 5003 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5004 | `	if( pStream == 0 ){` |
|    ! 0 | 5005 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5006 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5007 | `		return PH7_OK;` |
|      - | 5008 | `	}` |
|      3 | 5009 | `	if( nArg > 1 ){` |
|    ! 0 | 5010 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 5011 | `	}` |
|      - | 5012 | `	/* Try to open the file in read-only mode */` |
|      3 | 5013 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5014 | `	if( pHandle == 0 ){` |
|    ! 0 | 5015 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5016 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5017 | `		return PH7_OK;` |
|      - | 5018 | `	}` |
|      - | 5019 | `	/* Init the SHA1 context */` |
|      3 | 5020 | `	SHA1Init(&sCtx);` |
|      - | 5021 | `	/* Perform the requested operation */` |
|      2 | 5022 | `	for(;;){` |
|      5 | 5023 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5024 | `		if( n < 1 ){` |
|      - | 5025 | `			/* EOF or IO error,break immediately */` |
|      3 | 5026 | `			break;` |
|      - | 5027 | `		}` |
|      3 | 5028 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5029 | `	}` |
|      - | 5030 | `	/* Close the stream */` |
|      3 | 5031 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5032 | `	/* Extract the digest */` |
|      3 | 5033 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 5034 | `	if( raw_output ){` |
|      - | 5035 | `		/* Output raw digest */` |
|    ! 0 | 5036 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5037 | `	}else{` |
|      - | 5038 | `		/* Perform a binary to hex conversion */` |
|      3 | 5039 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5040 | `	}` |
|      3 | 5041 | `	return PH7_OK;` |
|      2 | 5042 |  |
|      - | 5043 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5044 | `/*` |
|      - | 5045 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 5046 | ` *  Parse a configuration file.` |
|      - | 5047 | ` * Parameters` |
|      - | 5048 | ` * $filename` |
|      - | 5049 | ` *  The filename of the ini file being parsed.` |
|      - | 5050 | ` * $process_sections` |
|      - | 5051 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 5052 | ` *  with the section names and settings included.` |
|      - | 5053 | ` *  The default for process_sections is FALSE.` |
|      - | 5054 | ` * $scanner_mode` |
|      - | 5055 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 5056 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 5057 | ` * Return` |
|      - | 5058 | ` *  The settings are returned as an associative array on success.` |
|      - | 5059 | ` *  Otherwise is returned.` |
|      - | 5060 | ` */` |
|      2 | 5061 | `static int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5062 |  |
|      - | 5063 | `	const ph7_io_stream *pStream;` |
|      - | 5064 | `	const char *zFile;` |
|      - | 5065 | `	SyBlob sContents;` |
|      - | 5066 | `	void *pHandle;` |
|      - | 5067 | `	int nLen;` |
|      3 | 5068 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5069 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5070 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5071 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5072 | `		return PH7_OK;` |
|      - | 5073 | `	}` |
|      - | 5074 | `	/* Extract the file path */` |
|      3 | 5075 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5076 | `	/* Point to the target IO stream device */` |
|      3 | 5077 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5078 | `	if( pStream == 0 ){` |
|    ! 0 | 5079 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5080 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5081 | `		return PH7_OK;` |
|      - | 5082 | `	}` |
|      - | 5083 | `	/* Try to open the file in read-only mode */` |
|      3 | 5084 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5085 | `	if( pHandle == 0 ){` |
|    ! 0 | 5086 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5087 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5088 | `		return PH7_OK;` |
|      - | 5089 | `	}` |
|      3 | 5090 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 5091 | `	/* Read the whole file */` |
|      3 | 5092 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      3 | 5093 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 5094 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 5095 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5096 | `	}else{` |
|      - | 5097 | `		/* Process the raw INI buffer */` |
|      5 | 5098 | `		PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      2 | 5099 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0);` |
|      - | 5100 | `	}` |
|      - | 5101 | `	/* Close the stream */` |
|      3 | 5102 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5103 | `	/* Release the working buffer */` |
|      3 | 5104 | `	SyBlobRelease(&sContents);` |
|      3 | 5105 | `	return PH7_OK;` |
|      2 | 5106 |  |
|      - | 5107 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 5108 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 5109 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 5110 | `#if defined(_MSC_VER)` |
|      - | 5111 | `static const ph7_vfs null_vfs = {` |
|      - | 5112 | `#else` |
|      - | 5113 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 5114 | `#endif` |
|      - | 5115 | `	"null_vfs",` |
|      - | 5116 | `	PH7_VFS_VERSION,` |
|      - | 5117 |  |
|      - | 5118 |  |
|      - | 5119 |  |
|      - | 5120 |  |
|      - | 5121 |  |
|      - | 5122 |  |
|      - | 5123 |  |
|      - | 5124 |  |
|      - | 5125 |  |
|      - | 5126 |  |
|      - | 5127 |  |
|      - | 5128 |  |
|      - | 5129 |  |
|      - | 5130 |  |
|      - | 5131 |  |
|      - | 5132 |  |
|      - | 5133 |  |
|      - | 5134 |  |
|      - | 5135 |  |
|      - | 5136 |  |
|      - | 5137 |  |
|      - | 5138 |  |
|      - | 5139 |  |
|      - | 5140 |  |
|      - | 5141 |  |
|      - | 5142 |  |
|      - | 5143 |  |
|      - | 5144 |  |
|      - | 5145 |  |
|      - | 5146 |  |
|      - | 5147 |  |
|      - | 5148 |  |
|      - | 5149 |  |
|      - | 5150 |  |
|      - | 5151 |  |
|      - | 5152 |  |
|      - | 5153 |  |
|      - | 5154 |  |
|      - | 5155 |  |
|      - | 5156 |  |
|      - | 5157 |  |
|      - | 5158 | `};` |
|      - | 5159 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 5160 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 5161 | `/*` |
|      - | 5162 | ` * Export the builtin vfs.` |
|      - | 5163 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 5164 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 5165 | ` * Note:` |
|      - | 5166 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 5167 | ` * Note:` |
|      - | 5168 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 5169 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 5170 | ` */` |
|   2808 | 5171 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      2 | 5172 |  |
|      - | 5173 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 5174 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 5175 | `	return &null_vfs;` |
|      - | 5176 | `#else` |
|      - | 5177 | `#ifdef __WINNT__` |
|      2 | 5178 | `	return &sWinVfs;` |
|      - | 5179 | `#elif defined(__UNIXES__)` |
|   2808 | 5180 | `	return &sUnixVfs;` |
|      - | 5181 | `#else` |
|      - | 5182 | `	return &null_vfs;` |
|      - | 5183 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 5184 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 5185 | `#else` |
|      - | 5186 | `	return &null_vfs;` |
|      - | 5187 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      2 | 5188 |  |
|      - | 5189 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 5190 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 5191 | `/*` |
|      - | 5192 | ` * The following defines are mostly used by the UNIX built and have` |
|      - | 5193 | ` * no particular meaning on windows.` |
|      - | 5194 | ` */` |
|      - | 5195 | `#ifndef STDIN_FILENO` |
|      - | 5196 | `#define STDIN_FILENO	0` |
|      - | 5197 | `#endif` |
|      - | 5198 | `#ifndef STDOUT_FILENO` |
|      - | 5199 | `#define STDOUT_FILENO	1` |
|      - | 5200 | `#endif` |
|      - | 5201 | `#ifndef STDERR_FILENO` |
|      - | 5202 | `#define STDERR_FILENO	2` |
|      - | 5203 | `#endif` |
|      - | 5204 | `/*` |
|      - | 5205 | ` * php:// Accessing various I/O streams` |
|      - | 5206 | ` * According to the PHP langage reference manual` |
|      - | 5207 | ` * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input` |
|      - | 5208 | ` * and output streams, the standard input, output and error file descriptors.` |
|      - | 5209 | ` * php://stdin, php://stdout and php://stderr:` |
|      - | 5210 | ` *  Allow direct access to the corresponding input or output stream of the PHP process.` |
|      - | 5211 | ` *  The stream references a duplicate file descriptor, so if you open php://stdin and later` |
|      - | 5212 | ` *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.` |
|      - | 5213 | ` *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.` |
|      - | 5214 | ` * php://output` |
|      - | 5215 | ` *  php://output is a write-only stream that allows you to write to the output buffer` |
|      - | 5216 | ` *  mechanism in the same way as print and echo.` |
|      - | 5217 | ` */` |
|      - | 5218 | `typedef struct ph7_stream_data ph7_stream_data;` |
|      - | 5219 | `/* Supported IO streams */` |
|      - | 5220 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|      - | 5221 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|      - | 5222 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|      - | 5223 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|      - | 5224 | ` /* The following structure is the private data associated with the php:// stream */` |
|      - | 5225 | `struct ph7_stream_data` |
|      - | 5226 |  |
|      - | 5227 | `	ph7_vm *pVm; /* VM that own this instance */` |
|      - | 5228 | `	int iType;   /* Stream type */` |
|      - | 5229 | `	union{` |
|      - | 5230 | `		void *pHandle; /* Stream handle */` |
|      - | 5231 | `		ph7_output_consumer sConsumer; /* VM output consumer */` |
|      - | 5232 | `	}x;` |
|      - | 5233 | `};` |
|      - | 5234 | `/*` |
|      - | 5235 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|      - | 5236 | ` */` |
|      8 | 5237 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|      1 | 5238 |  |
|      - | 5239 | `	ph7_stream_data *pData;` |
|      9 | 5240 | `	if( pVm == 0 ){` |
|    ! 0 | 5241 | `		return 0;` |
|      - | 5242 | `	}` |
|      - | 5243 | `	/* Allocate a new instance */` |
|      9 | 5244 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|      9 | 5245 | `	if( pData == 0 ){` |
|    ! 0 | 5246 | `		return 0;` |
|      - | 5247 | `	}` |
|      - | 5248 | `	/* Zero the structure */` |
|      9 | 5249 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|      - | 5250 | `	/* Initialize fields */` |
|      9 | 5251 | `	pData->iType = iType;` |
|      9 | 5252 | `	if( iType == PH7_IO_STREAM_OUTPUT ){` |
|      - | 5253 | `		/* Point to the default VM consumer routine. */` |
|      3 | 5254 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|      2 | 5255 | `	}else{` |
|      - | 5256 | `#ifdef __WINNT__` |
|      - | 5257 | `		DWORD nChannel;` |
|      1 | 5258 | `		switch(iType){` |
|      1 | 5259 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|      1 | 5260 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|      - | 5261 | `		default:` |
|      1 | 5262 | `			nChannel = STD_INPUT_HANDLE;` |
|      - | 5263 | `			break;` |
|      - | 5264 | `		}` |
|      1 | 5265 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|      - | 5266 | `#else` |
|      - | 5267 | `		/* Assume an UNIX system */` |
|      6 | 5268 | `		int ifd = STDIN_FILENO;` |
|      6 | 5269 | `		switch(iType){` |
|      2 | 5270 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|      2 | 5271 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|      1 | 5272 | `		default:` |
|      2 | 5273 | `			break;` |
|      - | 5274 | `		}` |
|      6 | 5275 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|      - | 5276 | `#endif` |
|      - | 5277 | `	}` |
|      9 | 5278 | `	pData->pVm = pVm;` |
|      9 | 5279 | `	return pData;` |
|      5 | 5280 |  |
|      - | 5281 | `/*` |
|      - | 5282 | ` * Implementation of the php:// IO streams routines` |
|      - | 5283 | ` * Status:` |
|      - | 5284 | ` *   Stable.` |
|      - | 5285 | ` */` |
|      - | 5286 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|      2 | 5287 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 5288 |  |
|      - | 5289 | `	ph7_stream_data *pData;` |
|      - | 5290 | `	SyString sStream;` |
|      3 | 5291 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|      - | 5292 | `	/* Trim leading and trailing white spaces */` |
|      3 | 5293 | `	SyStringFullTrim(&sStream);` |
|      - | 5294 | `	/* Stream to open */` |
|      3 | 5295 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|    ! 0 | 5296 | `		iMode = PH7_IO_STREAM_STDIN;` |
|      3 | 5297 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|      3 | 5298 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|      1 | 5299 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|    ! 0 | 5300 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|    ! 0 | 5301 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|    ! 0 | 5302 | `		iMode = PH7_IO_STREAM_STDERR;` |
|    ! 0 | 5303 | `	}else{` |
|      - | 5304 | `		/* unknown stream name */` |
|    ! 0 | 5305 | `		return -1;` |
|      - | 5306 | `	}` |
|      - | 5307 | `	/* Create our handle */` |
|      3 | 5308 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|      3 | 5309 | `	if( pData == 0 ){` |
|    ! 0 | 5310 | `		return -1;` |
|      - | 5311 | `	}` |
|      - | 5312 | `	/* Make the handle public */` |
|      3 | 5313 | `	*ppHandle = (void *)pData;` |
|      3 | 5314 | `	return PH7_OK;` |
|      2 | 5315 |  |
|      - | 5316 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|    ! 0 | 5317 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|    ! 0 | 5318 |  |
|    ! 0 | 5319 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    ! 0 | 5320 | `	if( pData == 0 ){` |
|    ! 0 | 5321 | `		return -1;` |
|      - | 5322 | `	}` |
|    ! 0 | 5323 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|      - | 5324 | `		/* Forbidden */` |
|    ! 0 | 5325 | `		return -1;` |
|      - | 5326 | `	}` |
|      - | 5327 | `#ifdef __WINNT__` |
|      - | 5328 | `	{` |
|      - | 5329 | `		DWORD nRd;` |
|      - | 5330 | `		BOOL rc;` |
|    ! 0 | 5331 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    ! 0 | 5332 | `		if( !rc ){` |
|      - | 5333 | `			/* IO error */` |
|    ! 0 | 5334 | `			return -1;` |
|      - | 5335 | `		}` |
|    ! 0 | 5336 | `		return (ph7_int64)nRd;` |
|      - | 5337 | `	}` |
|      - | 5338 | `#elif defined(__UNIXES__)` |
|      - | 5339 | `	{` |
|      - | 5340 | `		ssize_t nRd;` |
|      - | 5341 | `		int fd;` |
|    ! 0 | 5342 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 5343 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|    ! 0 | 5344 | `		if( nRd < 1 ){` |
|    ! 0 | 5345 | `			return -1;` |
|      - | 5346 | `		}` |
|    ! 0 | 5347 | `		return (ph7_int64)nRd;` |
|      - | 5348 | `	}` |
|      - | 5349 | `#else` |
|      - | 5350 | `	return -1;` |
|      - | 5351 | `#endif` |
|    ! 0 | 5352 |  |
|      - | 5353 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|      2 | 5354 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 5355 |  |
|      3 | 5356 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      3 | 5357 | `	if( pData == 0 ){` |
|    ! 0 | 5358 | `		return -1;` |
|      - | 5359 | `	}` |
|      3 | 5360 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|      - | 5361 | `		/* Forbidden */` |
|    ! 0 | 5362 | `		return -1;` |
|      3 | 5363 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|      3 | 5364 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|      - | 5365 | `		int rc;` |
|      - | 5366 | `		/* Call the vm output consumer */` |
|      3 | 5367 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|      3 | 5368 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 5369 | `			return -1;` |
|      - | 5370 | `		}` |
|      3 | 5371 | `		return nWrite;` |
|      - | 5372 | `	}` |
|      - | 5373 | `#ifdef __WINNT__` |
|      - | 5374 | `	{` |
|      - | 5375 | `		DWORD nWr;` |
|      - | 5376 | `		BOOL rc;` |
|    ! 0 | 5377 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|    ! 0 | 5378 | `		if( !rc ){` |
|      - | 5379 | `			/* IO error */` |
|    ! 0 | 5380 | `			return -1;` |
|      - | 5381 | `		}` |
|    ! 0 | 5382 | `		return (ph7_int64)nWr;` |
|      - | 5383 | `	}` |
|      - | 5384 | `#elif defined(__UNIXES__)` |
|      - | 5385 | `	{` |
|      - | 5386 | `		ssize_t nWr;` |
|      - | 5387 | `		int fd;` |
|    ! 0 | 5388 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 5389 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|    ! 0 | 5390 | `		if( nWr < 1 ){` |
|    ! 0 | 5391 | `			return -1;` |
|      - | 5392 | `		}` |
|    ! 0 | 5393 | `		return (ph7_int64)nWr;` |
|      - | 5394 | `	}` |
|      - | 5395 | `#else` |
|      - | 5396 | `	return -1;` |
|      - | 5397 | `#endif` |
|      2 | 5398 |  |
|      - | 5399 | `/* void (*xClose)(void *) */` |
|      2 | 5400 | `static void PHPStreamData_Close(void *pHandle)` |
|      1 | 5401 |  |
|      3 | 5402 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 5403 | `	ph7_vm *pVm;` |
|      3 | 5404 | `	if( pData == 0 ){` |
|    ! 0 | 5405 | `		return;` |
|      - | 5406 | `	}` |
|      3 | 5407 | `	pVm = pData->pVm;` |
|      - | 5408 | `	/* Free the instance */` |
|      3 | 5409 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|      2 | 5410 |  |
|      - | 5411 | `/*` |
|      - | 5412 | ` * Pipe stream implementation for popen/pclose.` |
|      - | 5413 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|      - | 5414 | ` * PHP-compatible process I/O functionality.` |
|      - | 5415 | ` */` |
|      - | 5416 | `typedef struct pipe_private pipe_private;` |
|      - | 5417 | `struct pipe_private` |
|      - | 5418 |  |
|      - | 5419 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|      - | 5420 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|      - | 5421 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|      - | 5422 | `#ifdef __WINNT__` |
|      - | 5423 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|      - | 5424 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|      - | 5425 | `#endif` |
|      - | 5426 | `};` |
|      - | 5427 |  |
|      - | 5428 | `#ifdef __WINNT__` |
|      - | 5429 | `#include <Windows.h>` |
|      - | 5430 | `#include <stdio.h>` |
|      - | 5431 | `#include <io.h>` |
|      - | 5432 | `#include <fcntl.h>` |
|      - | 5433 | `/*` |
|      - | 5434 | ` * Custom Windows popen implementation using CreateProcess.` |
|      - | 5435 | ` * This allows us to properly wait for process completion.` |
|      - | 5436 | ` */` |
|      - | 5437 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|      2 | 5438 |  |
|      2 | 5439 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|      2 | 5440 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|      2 | 5441 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|      - | 5442 | `	SECURITY_ATTRIBUTES sa;` |
|      - | 5443 | `	STARTUPINFOW si;` |
|      - | 5444 | `	PROCESS_INFORMATION pi;` |
|      2 | 5445 | `	WCHAR *zWideCmd = NULL;` |
|      2 | 5446 | `	FILE *pFile = NULL;` |
|      - | 5447 | `	int fd;` |
|      2 | 5448 | `	BOOL bRead = (zMode[0] == 'r');` |
|      - | 5449 |  |
|      - | 5450 | `	/* Set up security attributes for pipe inheritance */` |
|      2 | 5451 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|      2 | 5452 | `	sa.bInheritHandle = TRUE;` |
|      2 | 5453 | `	sa.lpSecurityDescriptor = NULL;` |
|      - | 5454 |  |
|      - | 5455 | `	/* Create pipes for child process I/O */` |
|      2 | 5456 | `	if( bRead ){` |
|      - | 5457 | `		/* Reading from child's stdout */` |
|      2 | 5458 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|    ! 0 | 5459 | `			return NULL;` |
|      - | 5460 | `		}` |
|      - | 5461 | `		/* Ensure read handle is not inherited */` |
|      2 | 5462 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|      2 | 5463 | `		hReadPipe = hChildStdoutRd;` |
|      2 | 5464 | `		*phPipe = hChildStdoutRd;` |
|      2 | 5465 | `	}else{` |
|      - | 5466 | `		/* Writing to child's stdin */` |
|    ! 0 | 5467 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|    ! 0 | 5468 | `			return NULL;` |
|      - | 5469 | `		}` |
|      - | 5470 | `		/* Ensure write handle is not inherited */` |
|    ! 0 | 5471 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|    ! 0 | 5472 | `		hWritePipe = hChildStdinWr;` |
|    ! 0 | 5473 | `		*phPipe = hChildStdinWr;` |
|      - | 5474 | `	}` |
|      - | 5475 |  |
|      - | 5476 | `	/* Convert command to wide string */` |
|      - | 5477 | `	{` |
|      2 | 5478 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|      2 | 5479 | `		if( nLen <= 0 ){` |
|    ! 0 | 5480 | `			goto cleanup_pipes;` |
|      - | 5481 | `		}` |
|      2 | 5482 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|      2 | 5483 | `		if( !zWideCmd ){` |
|    ! 0 | 5484 | `			goto cleanup_pipes;` |
|      - | 5485 | `		}` |
|      2 | 5486 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|      - | 5487 | `	}` |
|      - | 5488 |  |
|      - | 5489 | `	/* Set up process startup info */` |
|      2 | 5490 | `	ZeroMemory(&si, sizeof(si));` |
|      2 | 5491 | `	si.cb = sizeof(si);` |
|      2 | 5492 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|      2 | 5493 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|      2 | 5494 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|      2 | 5495 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|      2 | 5496 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|      - | 5497 |  |
|      2 | 5498 | `	ZeroMemory(&pi, sizeof(pi));` |
|      - | 5499 |  |
|      - | 5500 | `	/* Create the child process */` |
|      2 | 5501 | `	if( !CreateProcessW(` |
|      - | 5502 | `		NULL,           /* Application name */` |
|      - | 5503 | `		zWideCmd,       /* Command line */` |
|      - | 5504 | `		NULL,           /* Process security attributes */` |
|      - | 5505 | `		NULL,           /* Thread security attributes */` |
|      - | 5506 | `		TRUE,           /* Inherit handles */` |
|      - | 5507 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|      - | 5508 | `		NULL,           /* Environment */` |
|      - | 5509 | `		NULL,           /* Current directory */` |
|      - | 5510 | `		&si,            /* Startup info */` |
|      - | 5511 | `		&pi             /* Process info */` |
|      - | 5512 | `	)){` |
|    ! 0 | 5513 | `		goto cleanup_all;` |
|      - | 5514 | `	}` |
|      - | 5515 |  |
|      - | 5516 | `	/* Close handles we don't need in parent */` |
|      2 | 5517 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|      2 | 5518 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|      - | 5519 |  |
|      - | 5520 | `	/* Close thread handle (we only need process handle) */` |
|      2 | 5521 | `	CloseHandle(pi.hThread);` |
|      - | 5522 |  |
|      - | 5523 | `	/* Store process handle for later waiting */` |
|      2 | 5524 | `	*phProcess = pi.hProcess;` |
|      - | 5525 |  |
|      - | 5526 | `	/* Convert OS handle to C file descriptor, then to FILE* */` |
|      2 | 5527 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|      - | 5528 | `	                     bRead ? _O_RDONLY \| _O_TEXT : _O_WRONLY \| _O_TEXT);` |
|      2 | 5529 | `	if( fd == -1 ){` |
|    ! 0 | 5530 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 5531 | `		*phProcess = NULL;` |
|    ! 0 | 5532 | `		goto cleanup_all;` |
|      - | 5533 | `	}` |
|      - | 5534 |  |
|      2 | 5535 | `	pFile = _fdopen(fd, zMode);` |
|      2 | 5536 | `	if( !pFile ){` |
|    ! 0 | 5537 | `		_close(fd); /* This will also close the underlying handle */` |
|    ! 0 | 5538 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 5539 | `		*phProcess = NULL;` |
|    ! 0 | 5540 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    ! 0 | 5541 | `		return NULL;` |
|      - | 5542 | `	}` |
|      - | 5543 |  |
|      2 | 5544 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      2 | 5545 | `	return pFile;` |
|      - | 5546 |  |
|      - | 5547 | `cleanup_all:` |
|    ! 0 | 5548 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      - | 5549 | `cleanup_pipes:` |
|    ! 0 | 5550 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|    ! 0 | 5551 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|    ! 0 | 5552 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|    ! 0 | 5553 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|    ! 0 | 5554 | `	return NULL;` |
|      2 | 5555 |  |
|      - | 5556 |  |
|      - | 5557 | `/*` |
|      - | 5558 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|      - | 5559 | ` */` |
|      - | 5560 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|      2 | 5561 |  |
|      2 | 5562 | `	DWORD dwExitCode = 0;` |
|      - | 5563 | `	int status;` |
|      - | 5564 |  |
|      - | 5565 | `	/* Close the FILE* (this closes the pipe) */` |
|      2 | 5566 | `	fclose(pFile);` |
|      - | 5567 |  |
|      2 | 5568 | `	if( hProcess ){` |
|      - | 5569 | `		/* Wait for the process to complete */` |
|      2 | 5570 | `		WaitForSingleObject(hProcess, INFINITE);` |
|      - | 5571 |  |
|      2 | 5572 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|      2 | 5573 | `			status = (int)dwExitCode;` |
|      2 | 5574 | `		}else{` |
|    ! 0 | 5575 | `			status = -1;` |
|      - | 5576 | `		}` |
|      - | 5577 |  |
|      - | 5578 | `		/* Close process handle */` |
|      2 | 5579 | `		CloseHandle(hProcess);` |
|      2 | 5580 | `	}else{` |
|    ! 0 | 5581 | `		status = -1;` |
|      - | 5582 | `	}` |
|      - | 5583 |  |
|      2 | 5584 | `	return status;` |
|      2 | 5585 |  |
|      - | 5586 | `#endif /* __WINNT__ */` |
|      - | 5587 | `/*` |
|      - | 5588 | ` * Open a pipe to a process.` |
|      - | 5589 | ` * This is called internally by popen(), not through the stream device interface.` |
|      - | 5590 | ` */` |
|   2850 | 5591 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|      2 | 5592 |  |
|      - | 5593 | `	pipe_private *pPipe;` |
|      - | 5594 | `	FILE *pFile;` |
|   2852 | 5595 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|    ! 0 | 5596 | `		return 0;` |
|      - | 5597 | `	}` |
|      - | 5598 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|   2852 | 5599 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|    ! 0 | 5600 | `		return 0;` |
|      - | 5601 | `	}` |
|      - | 5602 | `	/* Open the pipe using system popen */` |
|      - | 5603 | `#ifdef __WINNT__` |
|      - | 5604 | `	{` |
|      - | 5605 | `		/* Build cmd.exe command wrapper */` |
|      2 | 5606 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|      2 | 5607 | `		const char *zShellSuffix = "\"";` |
|      2 | 5608 | `		size_t nPrefix = strlen(zShellPrefix);` |
|      2 | 5609 | `		size_t nSuffix = strlen(zShellSuffix);` |
|      2 | 5610 | `		size_t nCmd = strlen(zCommand);` |
|      2 | 5611 | `		size_t nQuotes = 0;` |
|      2 | 5612 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      2 | 5613 | `			if (zCommand[i] == '"') nQuotes++;` |
|      2 | 5614 | `		}` |
|      2 | 5615 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|      2 | 5616 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|      2 | 5617 | `		if (zCmdEsc == NULL) {` |
|    ! 0 | 5618 | `			return 0;` |
|      - | 5619 | `		}` |
|      - | 5620 | `		/* Escape quotes in command */` |
|      2 | 5621 | `		size_t j = 0;` |
|      2 | 5622 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      2 | 5623 | `			char ch = zCommand[i];` |
|      2 | 5624 | `			if (ch == '"') {` |
|      1 | 5625 | `				zCmdEsc[j++] = '^';` |
|      1 | 5626 | `				zCmdEsc[j++] = '"';` |
|      1 | 5627 | `			} else {` |
|      2 | 5628 | `				zCmdEsc[j++] = ch;` |
|      - | 5629 | `			}` |
|      2 | 5630 | `		}` |
|      2 | 5631 | `		zCmdEsc[j] = '\0';` |
|      2 | 5632 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|      2 | 5633 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|      2 | 5634 | `		if (zWinCmd == NULL) {` |
|    ! 0 | 5635 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 5636 | `			return 0;` |
|      - | 5637 | `		}` |
|      2 | 5638 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|      2 | 5639 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|      2 | 5640 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|      2 | 5641 | `		zWinCmd[nTotal - 1] = '\0';` |
|      - | 5642 | `		/* Allocate pipe structure early so we can store handles */` |
|      2 | 5643 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|      2 | 5644 | `		if( pPipe == 0 ){` |
|    ! 0 | 5645 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 5646 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|    ! 0 | 5647 | `			return 0;` |
|      - | 5648 | `		}` |
|      - | 5649 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|      2 | 5650 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|      2 | 5651 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|      2 | 5652 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|      2 | 5653 | `		if( pFile == 0 ){` |
|    ! 0 | 5654 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|    ! 0 | 5655 | `			return 0;` |
|      - | 5656 | `		}` |
|      - | 5657 | `		/* Initialize remaining fields */` |
|      2 | 5658 | `		pPipe->pFile = pFile;` |
|      2 | 5659 | `		pPipe->pVm = pVm;` |
|      2 | 5660 | `		pPipe->iMode = zMode[0];` |
|      - | 5661 | `	}` |
|      - | 5662 | `#else /* Unix */` |
|   2850 | 5663 | `	pFile = popen(zCommand, zMode);` |
|   2850 | 5664 | `	if( pFile == 0 ){` |
|    ! 0 | 5665 | `		return 0;` |
|      - | 5666 | `	}` |
|      - | 5667 | `	/* Allocate pipe private structure */` |
|   2850 | 5668 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|   2850 | 5669 | `	if( pPipe == 0 ){` |
|      - | 5670 | `		/* Out of memory, close the pipe */` |
|    ! 0 | 5671 | `		pclose(pFile);` |
|    ! 0 | 5672 | `		return 0;` |
|      - | 5673 | `	}` |
|      - | 5674 | `	/* Initialize the structure */` |
|   2850 | 5675 | `	pPipe->pFile = pFile;` |
|   2850 | 5676 | `	pPipe->pVm = pVm;` |
|   2850 | 5677 | `	pPipe->iMode = zMode[0];` |
|      - | 5678 | `#endif` |
|   2852 | 5679 | `	return pPipe;` |
|   1427 | 5680 |  |
|      - | 5681 | `/*` |
|      - | 5682 | ` * Close a pipe and return the exit status of the process.` |
|      - | 5683 | ` * Returns the exit status, or -1 on error.` |
|      - | 5684 | ` */` |
|   2832 | 5685 | `static int PipeClose(pipe_private *pPipe)` |
|      2 | 5686 |  |
|      - | 5687 | `	int status;` |
|      - | 5688 | `	ph7_vm *pVm;` |
|   2834 | 5689 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 5690 | `		return -1;` |
|      - | 5691 | `	}` |
|   2834 | 5692 | `	pVm = pPipe->pVm;` |
|      - | 5693 | `	/* Close the pipe and get exit status */` |
|      - | 5694 | `#ifdef __WINNT__` |
|      - | 5695 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|      2 | 5696 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|      - | 5697 | `#else` |
|   2832 | 5698 | `	status = pclose(pPipe->pFile);` |
|      - | 5699 | `	/* On Unix, pclose returns the status from waitpid, need to extract exit code */` |
|   2832 | 5700 | `	if( status != -1 ){` |
|   2832 | 5701 | `		if( WIFEXITED(status) ){` |
|   2832 | 5702 | `			status = WEXITSTATUS(status);` |
|   1416 | 5703 | `		}else if( WIFSIGNALED(status) ){` |
|      - | 5704 | `			/* Process was killed by a signal - use shell convention: 128 + signal number */` |
|    ! 0 | 5705 | `			status = 128 + WTERMSIG(status);` |
|    ! 0 | 5706 | `		}else{` |
|      - | 5707 | `			/* Unknown termination reason */` |
|    ! 0 | 5708 | `			status = -1;` |
|      - | 5709 | `		}` |
|   1416 | 5710 | `	}` |
|      - | 5711 | `#endif` |
|      - | 5712 | `	/* Free the structure */` |
|   2834 | 5713 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|   2834 | 5714 | `	return status;` |
|   1418 | 5715 |  |
|      - | 5716 | `/*` |
|      - | 5717 | ` * Pipe stream xClose implementation.` |
|      - | 5718 | ` * Note: This is called by fclose(), not pclose().` |
|      - | 5719 | ` * It closes the pipe but does not return the exit status.` |
|      - | 5720 | ` */` |
|     72 | 5721 | `static void PipeStream_Close(void *pHandle)` |
|      1 | 5722 |  |
|     73 | 5723 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|     73 | 5724 | `	if( pPipe ){` |
|     73 | 5725 | `		PipeClose(pPipe);` |
|     36 | 5726 | `	}` |
|     73 | 5727 |  |
|      - | 5728 | `/*` |
|      - | 5729 | ` * Pipe stream xRead implementation.` |
|      - | 5730 | ` */` |
|   4124 | 5731 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|      1 | 5732 |  |
|   4125 | 5733 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 5734 | `	size_t nRead;` |
|   4125 | 5735 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 5736 | `		return -1;` |
|      - | 5737 | `	}` |
|   4125 | 5738 | `	if( pPipe->iMode != 'r' ){` |
|      - | 5739 | `		/* Cannot read from a write-only pipe */` |
|    ! 0 | 5740 | `		return -1;` |
|      - | 5741 | `	}` |
|   4125 | 5742 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
|   4125 | 5743 | `	if( nRead == 0 ){` |
|   2875 | 5744 | `		if( feof(pPipe->pFile) ){` |
|   2875 | 5745 | `			return 0; /* EOF */` |
|      - | 5746 | `		}` |
|    ! 0 | 5747 | `		return -1; /* Error */` |
|      - | 5748 | `	}` |
|   1251 | 5749 | `	return (ph7_int64)nRead;` |
|   2063 | 5750 |  |
|      - | 5751 | `/*` |
|      - | 5752 | ` * Pipe stream xWrite implementation.` |
|      - | 5753 | ` */` |
|      2 | 5754 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|    ! 0 | 5755 |  |
|      2 | 5756 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 5757 | `	size_t nWritten;` |
|      2 | 5758 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 5759 | `		return -1;` |
|      - | 5760 | `	}` |
|      2 | 5761 | `	if( pPipe->iMode != 'w' ){` |
|      - | 5762 | `		/* Cannot write to a read-only pipe */` |
|    ! 0 | 5763 | `		return -1;` |
|      - | 5764 | `	}` |
|      2 | 5765 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|      2 | 5766 | `	if( nWritten == 0 && nWrite > 0 ){` |
|    ! 0 | 5767 | `		return -1; /* Error */` |
|      - | 5768 | `	}` |
|      2 | 5769 | `	return (ph7_int64)nWritten;` |
|      1 | 5770 |  |
|      - | 5771 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|      - | 5772 | `static const ph7_io_stream sPipe_Stream = {` |
|      - | 5773 | `	"pipe",` |
|      - | 5774 | `	PH7_IO_STREAM_VERSION,` |
|      - | 5775 |  |
|      - | 5776 |  |
|      - | 5777 | `	PipeStream_Close,  /* xClose */` |
|      - | 5778 |  |
|      - | 5779 | `	PipeStream_Read,   /* xRead */` |
|      - | 5780 |  |
|      - | 5781 | `	PipeStream_Write,  /* xWrite */` |
|      - | 5782 |  |
|      - | 5783 |  |
|      - | 5784 |  |
|      - | 5785 |  |
|      - | 5786 |  |
|      - | 5787 |  |
|      - | 5788 |  |
|      - | 5789 | `};` |
|      - | 5790 | `/*` |
|      - | 5791 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|      - | 5792 | ` * FALSE otherwise.` |
|      - | 5793 | ` */` |
|   2760 | 5794 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|      2 | 5795 |  |
|   2762 | 5796 | `	return pStream == &sPipe_Stream;` |
|      2 | 5797 |  |
|      - | 5798 | `/*` |
|      - | 5799 | ` * resource popen(string $command, string $mode)` |
|      - | 5800 | ` *  Opens process file pointer.` |
|      - | 5801 | ` * Parameters` |
|      - | 5802 | ` *  $command` |
|      - | 5803 | ` *   The command to execute. Passed to the system shell.` |
|      - | 5804 | ` *  $mode` |
|      - | 5805 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|      - | 5806 | ` *   'r' - Open for reading (read from the command's stdout).` |
|      - | 5807 | ` *   'w' - Open for writing (write to the command's stdin).` |
|      - | 5808 | ` * Return` |
|      - | 5809 | ` *  Returns a file pointer on success, or FALSE on error.` |
|      - | 5810 | ` */` |
|   2850 | 5811 | `static int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5812 |  |
|      - | 5813 | `	const char *zCommand, *zMode;` |
|      - | 5814 | `	pipe_private *pPipe;` |
|      - | 5815 | `	io_private *pDev;` |
|      - | 5816 | `	int nCmdLen, nModeLen;` |
|   2852 | 5817 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 5818 | `		/* Missing/Invalid arguments, return FALSE */` |
|    ! 0 | 5819 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a command string and mode");` |
|    ! 0 | 5820 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 5821 | `		return PH7_OK;` |
|      - | 5822 | `	}` |
|      - | 5823 | `	/* Extract the command and mode */` |
|   2852 | 5824 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|   2852 | 5825 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|   2852 | 5826 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 5827 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty command");` |
|    ! 0 | 5828 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 5829 | `		return PH7_OK;` |
|      - | 5830 | `	}` |
|   2852 | 5831 | `	if( nModeLen < 1 \|\| (zMode[0] != 'r' && zMode[0] != 'w') ){` |
|    ! 0 | 5832 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Invalid mode, expected 'r' or 'w'");` |
|    ! 0 | 5833 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 5834 | `		return PH7_OK;` |
|      - | 5835 | `	}` |
|      - | 5836 | `	/* Open the pipe */` |
|   2852 | 5837 | `	pPipe = PipeOpen(pCtx->pVm, zCommand, zMode);` |
|   2852 | 5838 | `	if( pPipe == 0 ){` |
|      - | 5839 | `		/* Failed to open pipe */` |
|    ! 0 | 5840 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 5841 | `		return PH7_OK;` |
|      - | 5842 | `	}` |
|      - | 5843 | `	/* Allocate an io_private instance to wrap the pipe */` |
|   2852 | 5844 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|   2852 | 5845 | `	if( pDev == 0 ){` |
|    ! 0 | 5846 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|    ! 0 | 5847 | `		PipeClose(pPipe);` |
|    ! 0 | 5848 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 5849 | `		return PH7_OK;` |
|      - | 5850 | `	}` |
|      - | 5851 | `	/* Initialize the io_private structure */` |
|   2852 | 5852 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|   2852 | 5853 | `	pDev->pHandle = pPipe;` |
|      - | 5854 | `	/* Return the io_private instance as a resource */` |
|   2852 | 5855 | `	ph7_result_resource(pCtx, pDev);` |
|   2852 | 5856 | `	return PH7_OK;` |
|   1427 | 5857 |  |
|      - | 5858 | `/*` |
|      - | 5859 | ` * int pclose(resource $handle)` |
|      - | 5860 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|      - | 5861 | ` * Parameters` |
|      - | 5862 | ` *  $handle` |
|      - | 5863 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|      - | 5864 | ` * Return` |
|      - | 5865 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|      - | 5866 | ` */` |
|   2760 | 5867 | `static int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5868 |  |
|      - | 5869 | `	const ph7_io_stream *pStream;` |
|      - | 5870 | `	pipe_private *pPipe;` |
|      - | 5871 | `	io_private *pDev;` |
|      - | 5872 | `	int status;` |
|   2762 | 5873 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5874 | `		/* Missing/Invalid arguments, return -1 */` |
|    ! 0 | 5875 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 5876 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 5877 | `		return PH7_OK;` |
|      - | 5878 | `	}` |
|      - | 5879 | `	/* Extract our private data */` |
|   2762 | 5880 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 5881 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   2762 | 5882 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 5883 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 5884 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 5885 | `		return PH7_OK;` |
|      - | 5886 | `	}` |
|      - | 5887 | `	/* Point to the target IO stream device */` |
|   2762 | 5888 | `	pStream = pDev->pStream;` |
|   2762 | 5889 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|    ! 0 | 5890 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|    ! 0 | 5891 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 5892 | `		return PH7_OK;` |
|      - | 5893 | `	}` |
|      - | 5894 | `	/* Get the pipe handle */` |
|   2762 | 5895 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|      - | 5896 | `	/* Close the pipe and get exit status */` |
|   2762 | 5897 | `	status = PipeClose(pPipe);` |
|      - | 5898 | `	/* Release the IO private structure */` |
|   2762 | 5899 | `	ReleaseIOPrivate(pCtx, pDev);` |
|      - | 5900 | `	/* Invalidate the resource handle */` |
|   2762 | 5901 | `	ph7_value_release(apArg[0]);` |
|      - | 5902 | `	/* Return the exit status */` |
|   2762 | 5903 | `	ph7_result_int(pCtx, status);` |
|   2762 | 5904 | `	return PH7_OK;` |
|   1382 | 5905 |  |
|      - | 5906 | `/* Export the php:// stream */` |
|      - | 5907 | `static const ph7_io_stream sPHP_Stream = {` |
|      - | 5908 | `	"php",` |
|      - | 5909 | `	PH7_IO_STREAM_VERSION,` |
|      - | 5910 | `	PHPStreamData_Open,  /* xOpen */` |
|      - | 5911 |  |
|      - | 5912 | `	PHPStreamData_Close, /* xClose */` |
|      - | 5913 |  |
|      - | 5914 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 5915 |  |
|      - | 5916 | `	PHPStreamData_Write, /* xWrite */` |
|      - | 5917 |  |
|      - | 5918 |  |
|      - | 5919 |  |
|      - | 5920 |  |
|      - | 5921 |  |
|      - | 5922 |  |
|      - | 5923 |  |
|      - | 5924 | `};` |
|      - | 5925 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 5926 | `/*` |
|      - | 5927 | ` * Return TRUE if we are dealing with the php:// stream.` |
|      - | 5928 | ` * FALSE otherwise.` |
|      - | 5929 | ` */` |
|     64 | 5930 | `static int is_php_stream(const ph7_io_stream *pStream)` |
|      2 | 5931 |  |
|      - | 5932 | `#ifndef PH7_DISABLE_DISK_IO` |
|     66 | 5933 | `	return pStream == &sPHP_Stream;` |
|      - | 5934 | `#else` |
|      - | 5935 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 5936 | `	return 0;` |
|      - | 5937 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      2 | 5938 |  |
|      - | 5939 |  |
|      - | 5940 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 5941 | `/*` |
|      - | 5942 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 5943 | ` * [i.e: file://,php://].` |
|      - | 5944 | ` * Note:` |
|      - | 5945 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 5946 | ` *  defined then this function is a no-op.` |
|      - | 5947 | ` */` |
|   2516 | 5948 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      2 | 5949 |  |
|      - | 5950 | `	/*` |
|      - | 5951 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 5952 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 5953 | `	 */` |
|      - | 5954 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 5955 | `	/* VFS: disk I/O related functions */` |
|      - | 5956 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 5957 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 5958 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 5959 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 5960 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 5961 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 5962 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 5963 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 5964 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 5965 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 5966 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 5967 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 5968 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 5969 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 5970 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 5971 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 5972 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 5973 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 5974 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 5975 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 5976 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 5977 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 5978 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 5979 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 5980 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 5981 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 5982 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 5983 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 5984 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 5985 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 5986 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 5987 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 5988 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 5989 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 5990 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 5991 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 5992 | `		{"link",        PH7_vfs_link     },` |
|      - | 5993 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 5994 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 5995 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 5996 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 5997 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 5998 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 5999 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 6000 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 6001 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 6002 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 6003 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 6004 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 6005 | `	};` |
|      - | 6006 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 6007 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 6008 | `	 */` |
|      - | 6009 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 6010 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 6011 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 6012 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 6013 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 6014 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 6015 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 6016 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 6017 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 6018 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 6019 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 6020 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 6021 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 6022 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 6023 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 6024 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 6025 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 6026 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 6027 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 6028 | `		{"file",      PH7_builtin_file   },` |
|      - | 6029 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 6030 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 6031 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 6032 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 6033 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 6034 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 6035 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 6036 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 6037 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 6038 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 6039 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 6040 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 6041 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 6042 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 6043 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 6044 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 6045 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 6046 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 6047 | `	};` |
|   2518 | 6048 | `	const ph7_io_stream *pFileStream = 0;` |
|   2518 | 6049 | `	sxu32 n = 0;` |
|      - | 6050 | `	/* Register disk-related functions */` |
| 123286 | 6051 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 120770 | 6052 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
|  60386 | 6053 | `	}` |
|  90578 | 6054 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
|  88062 | 6055 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  44032 | 6056 | `	}` |
|      - | 6057 | `#else` |
|      - | 6058 | `	SXUNUSED(pVm);` |
|      - | 6059 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 6060 |  |
|      - | 6061 | `	/*` |
|      - | 6062 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 6063 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 6064 | `	 */` |
|      - | 6065 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 6066 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 6067 | `		/* Path processing */` |
|      - | 6068 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 6069 | `		{"basename",    PH7_builtin_basename },` |
|      - | 6070 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 6071 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 6072 | `		{"fnmatch",     PH7_builtin_fnmatch  },` |
|      - | 6073 | `		/* ZIP processing */` |
|      - | 6074 | `		{"zip_open",    PH7_builtin_zip_open },` |
|      - | 6075 | `		{"zip_close",   PH7_builtin_zip_close},` |
|      - | 6076 | `		{"zip_read",    PH7_builtin_zip_read },` |
|      - | 6077 | `		{"zip_entry_open", PH7_builtin_zip_entry_open },` |
|      - | 6078 | `		{"zip_entry_close",PH7_builtin_zip_entry_close},` |
|      - | 6079 | `		{"zip_entry_name", PH7_builtin_zip_entry_name },` |
|      - | 6080 | `		{"zip_entry_filesize",      PH7_builtin_zip_entry_filesize       },` |
|      - | 6081 | `		{"zip_entry_compressedsize",PH7_builtin_zip_entry_compressedsize },` |
|      - | 6082 | `		{"zip_entry_read", PH7_builtin_zip_entry_read },` |
|      - | 6083 | `		{"zip_entry_reset_read_cursor",PH7_builtin_zip_entry_reset_read_cursor},` |
|      - | 6084 | `		{"zip_entry_compressionmethod",PH7_builtin_zip_entry_compressionmethod}` |
|      - | 6085 | `	};` |
|  42774 | 6086 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  40258 | 6087 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|  20130 | 6088 | `	}` |
|      - | 6089 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 6090 |  |
|      - | 6091 | `	/* Install streams if disk I/O is enabled */` |
|      - | 6092 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 6093 | `#ifdef __WINNT__` |
|      2 | 6094 | `	pFileStream = &sWinFileStream;` |
|      - | 6095 | `#elif defined(__UNIXES__)` |
|   2516 | 6096 | `	pFileStream = &sUnixFileStream;` |
|      - | 6097 | `#endif` |
|      - | 6098 | `	/* Install the php:// stream */` |
|   2518 | 6099 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   2518 | 6100 | `	if( pFileStream ){` |
|      - | 6101 | `		/* Install the file:// stream */` |
|   2518 | 6102 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   1258 | 6103 | `	}` |
|      - | 6104 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 6105 |  |
|   2518 | 6106 | `	return SXRET_OK;` |
|      2 | 6107 |  |
|      - | 6108 | `/*` |
|      - | 6109 | ` * Export the STDIN handle.` |
|      - | 6110 | ` */` |
|      2 | 6111 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|      1 | 6112 |  |
|      - | 6113 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 6114 | `	if( pVm->pStdin == 0  ){` |
|      - | 6115 | `		io_private *pIn;` |
|      - | 6116 | `		/* Allocate an IO private instance */` |
|      3 | 6117 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 6118 | `		if( pIn == 0 ){` |
|    ! 0 | 6119 | `			return 0;` |
|      - | 6120 | `		}` |
|      3 | 6121 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|      - | 6122 | `		/* Initialize the handle */` |
|      3 | 6123 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|      - | 6124 | `		/* Install the STDIN stream */` |
|      3 | 6125 | `		pVm->pStdin = pIn;` |
|      3 | 6126 | `		return pIn;` |
|    ! 0 | 6127 | `	}else{` |
|      - | 6128 | `		/* NULL or STDIN */` |
|    ! 0 | 6129 | `		return pVm->pStdin;` |
|      - | 6130 | `	}` |
|      - | 6131 | `#else` |
|      - | 6132 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 6133 | `	return 0;` |
|      - | 6134 | `#endif` |
|      2 | 6135 |  |
|      - | 6136 | `/*` |
|      - | 6137 | ` * Export the STDOUT handle.` |
|      - | 6138 | ` */` |
|      2 | 6139 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|      1 | 6140 |  |
|      - | 6141 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 6142 | `	if( pVm->pStdout == 0  ){` |
|      - | 6143 | `		io_private *pOut;` |
|      - | 6144 | `		/* Allocate an IO private instance */` |
|      3 | 6145 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 6146 | `		if( pOut == 0 ){` |
|    ! 0 | 6147 | `			return 0;` |
|      - | 6148 | `		}` |
|      3 | 6149 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|      - | 6150 | `		/* Initialize the handle */` |
|      3 | 6151 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|      - | 6152 | `		/* Install the STDOUT stream */` |
|      3 | 6153 | `		pVm->pStdout = pOut;` |
|      3 | 6154 | `		return pOut;` |
|    ! 0 | 6155 | `	}else{` |
|      - | 6156 | `		/* NULL or STDOUT */` |
|    ! 0 | 6157 | `		return pVm->pStdout;` |
|      - | 6158 | `	}` |
|      - | 6159 | `#else` |
|      - | 6160 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 6161 | `	return 0;` |
|      - | 6162 | `#endif` |
|      2 | 6163 |  |
|      - | 6164 | `/*` |
|      - | 6165 | ` * Export the STDERR handle.` |
|      - | 6166 | ` */` |
|      2 | 6167 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|      1 | 6168 |  |
|      - | 6169 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 6170 | `	if( pVm->pStderr == 0  ){` |
|      - | 6171 | `		io_private *pErr;` |
|      - | 6172 | `		/* Allocate an IO private instance */` |
|      3 | 6173 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 6174 | `		if( pErr == 0 ){` |
|    ! 0 | 6175 | `			return 0;` |
|      - | 6176 | `		}` |
|      3 | 6177 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|      - | 6178 | `		/* Initialize the handle */` |
|      3 | 6179 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|      - | 6180 | `		/* Install the STDERR stream */` |
|      3 | 6181 | `		pVm->pStderr = pErr;` |
|      3 | 6182 | `		return pErr;` |
|    ! 0 | 6183 | `	}else{` |
|      - | 6184 | `		/* NULL or STDERR */` |
|    ! 0 | 6185 | `		return pVm->pStderr;` |
|      - | 6186 | `	}` |
|      - | 6187 | `#else` |
|      - | 6188 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 6189 | `	return 0;` |
|      - | 6190 | `#endif` |
|      2 | 6191 |  |
|      - | 6192 |  |
