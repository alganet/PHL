# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2699/3903 lines (69.15%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/*` |
|      - |    8 | ` * This file implement a virtual file systems (VFS) for the PH7 engine.` |
|      - |    9 | ` */` |
|      - |   10 | `/*` |
|      - |   11 | ` * Given a string containing the path of a file or directory, this function` |
|      - |   12 | ` * return the parent directory's path.` |
|      - |   13 | ` */` |
|     30 |   14 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)` |
|      2 |   15 |  |
|     32 |   16 | `	const char *zEnd = &zPath[nByte - 1];` |
|      - |   17 | `	int c,d;` |
|     32 |   18 | `	c = d = '/';` |
|      - |   19 | `#ifdef __WINNT__` |
|      2 |   20 | `	d = '\\';` |
|      - |   21 | `#endif` |
|    505 |   22 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    460 |   23 | `		zEnd--;` |
|      2 |   24 | `	}` |
|     32 |   25 | `	*pLen = (int)(zEnd-zPath);` |
|      - |   26 | `#ifdef __WINNT__` |
|      2 |   27 | `	if( (*pLen) == (int)sizeof(char) && zPath[0] == '/' ){` |
|      - |   28 | `		/* Normalize path on windows */` |
|    ! 0 |   29 | `		return "\\";` |
|      - |   30 | `	}` |
|      - |   31 | `#endif` |
|     32 |   32 | `	if( zEnd == zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d) ){` |
|      - |   33 | `		/* No separator,return "." as the current directory */` |
|      5 |   34 | `		*pLen = sizeof(char);` |
|      5 |   35 | `		return ".";` |
|      - |   36 | `	}` |
|     28 |   37 | `	if( (*pLen) == 0 ){` |
|      2 |   38 | `		*pLen = sizeof(char);` |
|      - |   39 | `#ifdef __WINNT__` |
|    ! 0 |   40 | `		return "\\";` |
|      - |   41 | `#else` |
|      2 |   42 | `		return "/";` |
|      - |   43 | `#endif` |
|      - |   44 | `	}` |
|     26 |   45 | `	return zPath;` |
|     17 |   46 |  |
|      - |   47 | `/*` |
|      - |   48 | ` * Compile the VFS implementations when builtins are enabled OR when disk I/O` |
|      - |   49 | ` * is explicitly enabled (i.e. PH7_DISABLE_DISK_IO is NOT defined).` |
|      - |   50 | ` */` |
|      - |   51 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |   52 | `/*` |
|      - |   53 | ` * bool chdir(string $directory)` |
|      - |   54 | ` *  Change the current directory.` |
|      - |   55 | ` * Parameters` |
|      - |   56 | ` *  $directory` |
|      - |   57 | ` *   The new current directory` |
|      - |   58 | ` * Return` |
|      - |   59 | ` *  TRUE on success or FALSE on failure.` |
|      - |   60 | ` */` |
|   9364 |   61 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   62 |  |
|      - |   63 | `	const char *zPath;` |
|      - |   64 | `	ph7_vfs *pVfs;` |
|      - |   65 | `	int rc;` |
|   9366 |   66 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |   67 | `		/* Missing/Invalid argument,return FALSE */` |
|      8 |   68 | `		ph7_result_bool(pCtx,0);` |
|      8 |   69 | `		return PH7_OK;` |
|      - |   70 | `	}` |
|      - |   71 | `	/* Point to the underlying vfs */` |
|   9360 |   72 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   9360 |   73 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |   74 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |   75 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |   76 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |   77 | `			ph7_function_name(pCtx)` |
|      - |   78 | `			);` |
|    ! 0 |   79 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |   80 | `		return PH7_OK;` |
|      - |   81 | `	}` |
|      - |   82 | `	/* Point to the desired directory */` |
|   9360 |   83 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |   84 | `	/* Perform the requested operation */` |
|   9360 |   85 | `	rc = pVfs->xChdir(zPath);` |
|      - |   86 | `	/* IO return value */` |
|   9360 |   87 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   9360 |   88 | `	return PH7_OK;` |
|   4684 |   89 |  |
|      - |   90 | `/*` |
|      - |   91 | ` * bool chroot(string $directory)` |
|      - |   92 | ` *  Change the root directory.` |
|      - |   93 | ` * Parameters` |
|      - |   94 | ` *  $directory` |
|      - |   95 | ` *   The path to change the root directory to` |
|      - |   96 | ` * Return` |
|      - |   97 | ` *  TRUE on success or FALSE on failure.` |
|      - |   98 | ` */` |
|      6 |   99 | `static int PH7_vfs_chroot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  100 |  |
|      - |  101 | `	const char *zPath;` |
|      - |  102 | `	ph7_vfs *pVfs;` |
|      - |  103 | `	int rc;` |
|      7 |  104 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  105 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 |  106 | `		ph7_result_bool(pCtx,0);` |
|      5 |  107 | `		return PH7_OK;` |
|      - |  108 | `	}` |
|      - |  109 | `	/* Point to the underlying vfs */` |
|      2 |  110 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 |  111 | `	if( pVfs == 0 \|\| pVfs->xChroot == 0 ){` |
|      - |  112 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  113 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  114 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  115 | `			ph7_function_name(pCtx)` |
|      - |  116 | `			);` |
|    ! 0 |  117 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  118 | `		return PH7_OK;` |
|      - |  119 | `	}` |
|      - |  120 | `	/* Point to the desired directory */` |
|      2 |  121 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  122 | `	/* Perform the requested operation */` |
|      2 |  123 | `	rc = pVfs->xChroot(zPath);` |
|      - |  124 | `	/* IO return value */` |
|      2 |  125 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      2 |  126 | `	return PH7_OK;` |
|      4 |  127 |  |
|      - |  128 | `/*` |
|      - |  129 | ` * string getcwd(void)` |
|      - |  130 | ` *  Gets the current working directory.` |
|      - |  131 | ` * Parameters` |
|      - |  132 | ` *  None` |
|      - |  133 | ` * Return` |
|      - |  134 | ` *  Returns the current working directory on success, or FALSE on failure.` |
|      - |  135 | ` */` |
|     20 |  136 | `static int PH7_vfs_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  137 |  |
|      - |  138 | `	ph7_vfs *pVfs;` |
|      - |  139 | `	int rc;` |
|      - |  140 | `	/* Point to the underlying vfs */` |
|     22 |  141 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     22 |  142 | `	if( pVfs == 0 \|\| pVfs->xGetcwd == 0 ){` |
|    ! 0 |  143 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 |  144 | `		SXUNUSED(apArg);` |
|      - |  145 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  146 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  147 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  148 | `			ph7_function_name(pCtx)` |
|      - |  149 | `			);` |
|    ! 0 |  150 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  151 | `		return PH7_OK;` |
|      - |  152 | `	}` |
|     22 |  153 | `	ph7_result_string(pCtx,"",0);` |
|      - |  154 | `	/* Perform the requested operation */` |
|     22 |  155 | `	rc = pVfs->xGetcwd(pCtx);` |
|     22 |  156 | `	if( rc != PH7_OK ){` |
|      - |  157 | `		/* Error,return FALSE */` |
|    ! 0 |  158 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  159 | `	}` |
|     22 |  160 | `	return PH7_OK;` |
|     12 |  161 |  |
|      - |  162 | `/*` |
|      - |  163 | ` * bool rmdir(string $directory)` |
|      - |  164 | ` *  Removes directory.` |
|      - |  165 | ` * Parameters` |
|      - |  166 | ` *  $directory` |
|      - |  167 | ` *   The path to the directory` |
|      - |  168 | ` * Return` |
|      - |  169 | ` *  TRUE on success or FALSE on failure.` |
|      - |  170 | ` */` |
|      8 |  171 | `static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  172 |  |
|      - |  173 | `	const char *zPath;` |
|      - |  174 | `	ph7_vfs *pVfs;` |
|      - |  175 | `	int rc;` |
|      9 |  176 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  177 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  178 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  179 | `		return PH7_OK;` |
|      - |  180 | `	}` |
|      - |  181 | `	/* Point to the underlying vfs */` |
|      9 |  182 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 |  183 | `	if( pVfs == 0 \|\| pVfs->xRmdir == 0 ){` |
|      - |  184 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  185 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  186 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  187 | `			ph7_function_name(pCtx)` |
|      - |  188 | `			);` |
|    ! 0 |  189 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  190 | `		return PH7_OK;` |
|      - |  191 | `	}` |
|      - |  192 | `	/* Point to the desired directory */` |
|      9 |  193 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  194 | `	/* Perform the requested operation */` |
|      9 |  195 | `	rc = pVfs->xRmdir(zPath);` |
|      - |  196 | `	/* IO return value */` |
|      9 |  197 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      9 |  198 | `	return PH7_OK;` |
|      5 |  199 |  |
|      - |  200 | `/*` |
|      - |  201 | ` * bool is_dir(string $filename)` |
|      - |  202 | ` *  Tells whether the given filename is a directory.` |
|      - |  203 | ` * Parameters` |
|      - |  204 | ` *  $filename` |
|      - |  205 | ` *   Path to the file.` |
|      - |  206 | ` * Return` |
|      - |  207 | ` *  TRUE on success or FALSE on failure.` |
|      - |  208 | ` */` |
|   5592 |  209 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  210 |  |
|      - |  211 | `	const char *zPath;` |
|      - |  212 | `	ph7_vfs *pVfs;` |
|      - |  213 | `	int rc;` |
|   5594 |  214 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  215 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  216 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  217 | `		return PH7_OK;` |
|      - |  218 | `	}` |
|      - |  219 | `	/* Point to the underlying vfs */` |
|   5594 |  220 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   5594 |  221 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  222 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  223 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  224 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  225 | `			ph7_function_name(pCtx)` |
|      - |  226 | `			);` |
|    ! 0 |  227 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  228 | `		return PH7_OK;` |
|      - |  229 | `	}` |
|      - |  230 | `	/* Point to the desired directory */` |
|   5594 |  231 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  232 | `	/* Perform the requested operation */` |
|   5594 |  233 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  234 | `	/* IO return value */` |
|   5594 |  235 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   5594 |  236 | `	return PH7_OK;` |
|   2798 |  237 |  |
|      - |  238 | `/*` |
|      - |  239 | ` * bool mkdir(string $pathname[,int $mode = 0777 [,bool $recursive = false])` |
|      - |  240 | ` *  Make a directory.` |
|      - |  241 | ` * Parameters` |
|      - |  242 | ` *  $pathname` |
|      - |  243 | ` *   The directory path.` |
|      - |  244 | ` * $mode` |
|      - |  245 | ` *  The mode is 0777 by default, which means the widest possible access.` |
|      - |  246 | ` *  Note:` |
|      - |  247 | ` *   mode is ignored on Windows.` |
|      - |  248 | ` *   Note that you probably want to specify the mode as an octal number, which means` |
|      - |  249 | ` *   it should have a leading zero. The mode is also modified by the current umask` |
|      - |  250 | ` *   which you can change using umask().` |
|      - |  251 | ` * $recursive` |
|      - |  252 | ` *  Allows the creation of nested directories specified in the pathname.` |
|      - |  253 | ` *  Defaults to FALSE. (Not used)` |
|      - |  254 | ` * Return` |
|      - |  255 | ` *  TRUE on success or FALSE on failure.` |
|      - |  256 | ` */` |
|      8 |  257 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  258 |  |
|      9 |  259 | `	int iRecursive = 0;` |
|      - |  260 | `	const char *zPath;` |
|      - |  261 | `	ph7_vfs *pVfs;` |
|      - |  262 | `	int iMode,rc;` |
|      9 |  263 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  264 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  265 | `		ph7_result_bool(pCtx,0);` |
|      3 |  266 | `		return PH7_OK;` |
|      - |  267 | `	}` |
|      - |  268 | `	/* Point to the underlying vfs */` |
|      7 |  269 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 |  270 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  271 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  272 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  273 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  274 | `			ph7_function_name(pCtx)` |
|      - |  275 | `			);` |
|    ! 0 |  276 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  277 | `		return PH7_OK;` |
|      - |  278 | `	}` |
|      - |  279 | `	/* Point to the desired directory */` |
|      7 |  280 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  281 | `#ifdef __WINNT__` |
|      1 |  282 | `	iMode = 0;` |
|      - |  283 | `#else` |
|      - |  284 | `	/* Assume UNIX */` |
|      6 |  285 | `	iMode = 0777;` |
|      - |  286 | `#endif` |
|      7 |  287 | `	if( nArg > 1 ){` |
|    ! 0 |  288 | `		iMode = ph7_value_to_int(apArg[1]);` |
|    ! 0 |  289 | `		if( nArg > 2 ){` |
|    ! 0 |  290 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|    ! 0 |  291 | `		}` |
|    ! 0 |  292 | `	}` |
|      - |  293 | `	/* Perform the requested operation */` |
|      7 |  294 | `	rc = pVfs->xMkdir(zPath,iMode,iRecursive);` |
|      - |  295 | `	/* IO return value */` |
|      7 |  296 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      7 |  297 | `	return PH7_OK;` |
|      5 |  298 |  |
|      - |  299 | `/*` |
|      - |  300 | ` * bool rename(string $oldname,string $newname)` |
|      - |  301 | ` *  Attempts to rename oldname to newname.` |
|      - |  302 | ` * Parameters` |
|      - |  303 | ` *  $oldname` |
|      - |  304 | ` *   Old name.` |
|      - |  305 | ` *  $newname` |
|      - |  306 | ` *   New name.` |
|      - |  307 | ` * Return` |
|      - |  308 | ` *  TRUE on success or FALSE on failure.` |
|      - |  309 | ` */` |
|      4 |  310 | `static int PH7_vfs_rename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  311 |  |
|      - |  312 | `	const char *zOld,*zNew;` |
|      - |  313 | `	ph7_vfs *pVfs;` |
|      - |  314 | `	int rc;` |
|      5 |  315 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - |  316 | `		/* Missing/Invalid arguments,return FALSE */` |
|      3 |  317 | `		ph7_result_bool(pCtx,0);` |
|      3 |  318 | `		return PH7_OK;` |
|      - |  319 | `	}` |
|      - |  320 | `	/* Point to the underlying vfs */` |
|      3 |  321 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  322 | `	if( pVfs == 0 \|\| pVfs->xRename == 0 ){` |
|      - |  323 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  324 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  325 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  326 | `			ph7_function_name(pCtx)` |
|      - |  327 | `			);` |
|    ! 0 |  328 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  329 | `		return PH7_OK;` |
|      - |  330 | `	}` |
|      - |  331 | `	/* Perform the requested operation */` |
|      3 |  332 | `	zOld = ph7_value_to_string(apArg[0],0);` |
|      3 |  333 | `	zNew = ph7_value_to_string(apArg[1],0);` |
|      3 |  334 | `	rc = pVfs->xRename(zOld,zNew);` |
|      - |  335 | `	/* IO result */` |
|      3 |  336 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 |  337 | `	return PH7_OK;` |
|      3 |  338 |  |
|      - |  339 | `/*` |
|      - |  340 | ` * string realpath(string $path)` |
|      - |  341 | ` *  Returns canonicalized absolute pathname.` |
|      - |  342 | ` * Parameters` |
|      - |  343 | ` *  $path` |
|      - |  344 | ` *   Target path.` |
|      - |  345 | ` * Return` |
|      - |  346 | ` *  Canonicalized absolute pathname on success. or FALSE on failure.` |
|      - |  347 | ` */` |
|     10 |  348 | `static int PH7_vfs_realpath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  349 |  |
|      - |  350 | `	const char *zPath;` |
|      - |  351 | `	ph7_vfs *pVfs;` |
|      - |  352 | `        int rc;` |
|     11 |  353 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  354 | `		/* Missing/Invalid argument,return FALSE */` |
|      7 |  355 | `		ph7_result_bool(pCtx,0);` |
|      7 |  356 | `		return PH7_OK;` |
|      - |  357 | `	}` |
|      - |  358 | `	/* Point to the underlying vfs */` |
|      5 |  359 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  360 | `	if( pVfs == 0 \|\| pVfs->xRealpath == 0 ){` |
|      - |  361 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  362 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  363 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  364 | `			ph7_function_name(pCtx)` |
|      - |  365 | `			);` |
|    ! 0 |  366 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  367 | `		return PH7_OK;` |
|      - |  368 | `	}` |
|      - |  369 | `	/* Set an empty string untnil the underlying OS interface change that */` |
|      5 |  370 | `	ph7_result_string(pCtx,"",0);` |
|      - |  371 | `	/* Perform the requested operation */` |
|      5 |  372 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      5 |  373 | `	rc = pVfs->xRealpath(zPath,pCtx);` |
|      5 |  374 | `	if( rc != PH7_OK ){` |
|      2 |  375 | `	 ph7_result_bool(pCtx,0);` |
|      1 |  376 | `	}` |
|      5 |  377 | `	return PH7_OK;` |
|      6 |  378 |  |
|      - |  379 | `/*` |
|      - |  380 | ` * int sleep(int $seconds)` |
|      - |  381 | ` *  Delays the program execution for the given number of seconds.` |
|      - |  382 | ` * Parameters` |
|      - |  383 | ` *  $seconds` |
|      - |  384 | ` *   Halt time in seconds.` |
|      - |  385 | ` * Return` |
|      - |  386 | ` *  Zero on success or FALSE on failure.` |
|      - |  387 | ` */` |
|      6 |  388 | `static int PH7_vfs_sleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  389 |  |
|      - |  390 | `	ph7_vfs *pVfs;` |
|      - |  391 | `	int rc,nSleep;` |
|      7 |  392 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  393 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  394 | `		ph7_result_bool(pCtx,0);` |
|      3 |  395 | `		return PH7_OK;` |
|      - |  396 | `	}` |
|      - |  397 | `	/* Point to the underlying vfs */` |
|      5 |  398 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  399 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  400 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  401 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  402 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  403 | `			ph7_function_name(pCtx)` |
|      - |  404 | `			);` |
|    ! 0 |  405 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  406 | `		return PH7_OK;` |
|      - |  407 | `	}` |
|      - |  408 | `	/* Amount to sleep */` |
|      5 |  409 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|      5 |  410 | `	if( nSleep < 0 ){` |
|      - |  411 | `		/* Invalid value,return FALSE */` |
|      3 |  412 | `		ph7_result_bool(pCtx,0);` |
|      3 |  413 | `		return PH7_OK;` |
|      - |  414 | `	}` |
|      - |  415 | `	/* Perform the requested operation (Microseconds) */` |
|      3 |  416 | `	rc = pVfs->xSleep((unsigned int)(nSleep * SX_USEC_PER_SEC));` |
|      3 |  417 | `	if( rc != PH7_OK ){` |
|      - |  418 | `		/* Return FALSE */` |
|    ! 0 |  419 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  420 | `	}else{` |
|      - |  421 | `		/* Return zero */` |
|      3 |  422 | `		ph7_result_int(pCtx,0);` |
|      - |  423 | `	}` |
|      3 |  424 | `	return PH7_OK;` |
|      4 |  425 |  |
|      - |  426 | `/*` |
|      - |  427 | ` * void usleep(int $micro_seconds)` |
|      - |  428 | ` *  Delays program execution for the given number of micro seconds.` |
|      - |  429 | ` * Parameters` |
|      - |  430 | ` *  $micro_seconds` |
|      - |  431 | ` *   Halt time in micro seconds. A micro second is one millionth of a second.` |
|      - |  432 | ` * Return` |
|      - |  433 | ` *  None.` |
|      - |  434 | ` */` |
|      4 |  435 | `static int PH7_vfs_usleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  436 |  |
|      - |  437 | `	ph7_vfs *pVfs;` |
|      - |  438 | `	int nSleep;` |
|      5 |  439 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  440 | `		/* Missing/Invalid argument,return immediately */` |
|    ! 0 |  441 | `		return PH7_OK;` |
|      - |  442 | `	}` |
|      - |  443 | `	/* Point to the underlying vfs */` |
|      5 |  444 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  445 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  446 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  447 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  448 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 |  449 | `			ph7_function_name(pCtx)` |
|      - |  450 | `			);` |
|    ! 0 |  451 | `		return PH7_OK;` |
|      - |  452 | `	}` |
|      - |  453 | `	/* Amount to sleep */` |
|      5 |  454 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|      5 |  455 | `	if( nSleep < 0 ){` |
|      - |  456 | `		/* Invalid value,return immediately */` |
|      3 |  457 | `		return PH7_OK;` |
|      - |  458 | `	}` |
|      - |  459 | `	/* Perform the requested operation (Microseconds) */` |
|      3 |  460 | `	pVfs->xSleep((unsigned int)nSleep);` |
|      3 |  461 | `	return PH7_OK;` |
|      3 |  462 |  |
|      - |  463 | `/*` |
|      - |  464 | ` * bool unlink (string $filename)` |
|      - |  465 | ` *  Delete a file.` |
|      - |  466 | ` * Parameters` |
|      - |  467 | ` *  $filename` |
|      - |  468 | ` *   Path to the file.` |
|      - |  469 | ` * Return` |
|      - |  470 | ` *  TRUE on success or FALSE on failure.` |
|      - |  471 | ` */` |
|  19752 |  472 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  473 |  |
|      - |  474 | `	const char *zPath;` |
|      - |  475 | `	ph7_vfs *pVfs;` |
|      - |  476 | `	int rc;` |
|  19754 |  477 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  478 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  479 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  480 | `		return PH7_OK;` |
|      - |  481 | `	}` |
|      - |  482 | `	/* Point to the underlying vfs */` |
|  19754 |  483 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  19754 |  484 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  485 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  486 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  487 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  488 | `			ph7_function_name(pCtx)` |
|      - |  489 | `			);` |
|    ! 0 |  490 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  491 | `		return PH7_OK;` |
|      - |  492 | `	}` |
|      - |  493 | `	/* Point to the desired directory */` |
|  19754 |  494 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  495 | `	/* Perform the requested operation */` |
|  19754 |  496 | `	rc = pVfs->xUnlink(zPath);` |
|      - |  497 | `	/* IO return value */` |
|  19754 |  498 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  19754 |  499 | `	return PH7_OK;` |
|   9878 |  500 |  |
|      - |  501 | `/*` |
|      - |  502 | ` * bool chmod(string $filename,int $mode)` |
|      - |  503 | ` *  Attempts to change the mode of the specified file to that given in mode.` |
|      - |  504 | ` * Parameters` |
|      - |  505 | ` *  $filename` |
|      - |  506 | ` *   Path to the file.` |
|      - |  507 | ` * $mode` |
|      - |  508 | ` *   Mode (Must be an integer)` |
|      - |  509 | ` * Return` |
|      - |  510 | ` *  TRUE on success or FALSE on failure.` |
|      - |  511 | ` */` |
|     10 |  512 | `static int PH7_vfs_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  513 |  |
|      - |  514 | `	const char *zPath;` |
|      - |  515 | `	ph7_vfs *pVfs;` |
|      - |  516 | `	int iMode;` |
|      - |  517 | `	int rc;` |
|     10 |  518 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  519 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  520 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  521 | `		return PH7_OK;` |
|      - |  522 | `	}` |
|      - |  523 | `	/* Point to the underlying vfs */` |
|     10 |  524 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     10 |  525 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - |  526 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  527 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  528 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  529 | `			ph7_function_name(pCtx)` |
|      - |  530 | `			);` |
|    ! 0 |  531 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  532 | `		return PH7_OK;` |
|      - |  533 | `	}` |
|      - |  534 | `	/* Point to the desired directory */` |
|     10 |  535 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  536 | `	/* Extract the mode */` |
|     10 |  537 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - |  538 | `	/* Perform the requested operation */` |
|     10 |  539 | `	rc = pVfs->xChmod(zPath,iMode);` |
|      - |  540 | `	/* IO return value */` |
|     10 |  541 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     10 |  542 | `	return PH7_OK;` |
|      5 |  543 |  |
|      - |  544 | `/*` |
|      - |  545 | ` * bool chown(string $filename,string $user)` |
|      - |  546 | ` *  Attempts to change the owner of the file filename to user user.` |
|      - |  547 | ` * Parameters` |
|      - |  548 | ` *  $filename` |
|      - |  549 | ` *   Path to the file.` |
|      - |  550 | ` * $user` |
|      - |  551 | ` *   Username.` |
|      - |  552 | ` * Return` |
|      - |  553 | ` *  TRUE on success or FALSE on failure.` |
|      - |  554 | ` */` |
|      6 |  555 | `static int PH7_vfs_chown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  556 |  |
|      - |  557 | `	const char *zPath,*zUser;` |
|      - |  558 | `	ph7_vfs *pVfs;` |
|      - |  559 | `	int rc;` |
|      7 |  560 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  561 | `		/* Missing/Invalid arguments,return FALSE */` |
|      3 |  562 | `		ph7_result_bool(pCtx,0);` |
|      3 |  563 | `		return PH7_OK;` |
|      - |  564 | `	}` |
|      - |  565 | `	/* Point to the underlying vfs */` |
|      4 |  566 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 |  567 | `	if( pVfs == 0 \|\| pVfs->xChown == 0 ){` |
|      - |  568 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  569 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  570 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  571 | `			ph7_function_name(pCtx)` |
|      - |  572 | `			);` |
|    ! 0 |  573 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  574 | `		return PH7_OK;` |
|      - |  575 | `	}` |
|      - |  576 | `	/* Point to the desired directory */` |
|      4 |  577 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  578 | `	/* Extract the user */` |
|      4 |  579 | `	zUser = ph7_value_to_string(apArg[1],0);` |
|      - |  580 | `	/* Perform the requested operation */` |
|      4 |  581 | `	rc = pVfs->xChown(zPath,zUser);` |
|      - |  582 | `	/* IO return value */` |
|      4 |  583 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 |  584 | `	return PH7_OK;` |
|      4 |  585 |  |
|      - |  586 | `/*` |
|      - |  587 | ` * bool chgrp(string $filename,string $group)` |
|      - |  588 | ` *  Attempts to change the group of the file filename to group.` |
|      - |  589 | ` * Parameters` |
|      - |  590 | ` *  $filename` |
|      - |  591 | ` *   Path to the file.` |
|      - |  592 | ` * $group` |
|      - |  593 | ` *   groupname.` |
|      - |  594 | ` * Return` |
|      - |  595 | ` *  TRUE on success or FALSE on failure.` |
|      - |  596 | ` */` |
|      6 |  597 | `static int PH7_vfs_chgrp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  598 |  |
|      - |  599 | `	const char *zPath,*zGroup;` |
|      - |  600 | `	ph7_vfs *pVfs;` |
|      - |  601 | `	int rc;` |
|      7 |  602 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  603 | `		/* Missing/Invalid arguments,return FALSE */` |
|      3 |  604 | `		ph7_result_bool(pCtx,0);` |
|      3 |  605 | `		return PH7_OK;` |
|      - |  606 | `	}` |
|      - |  607 | `	/* Point to the underlying vfs */` |
|      4 |  608 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 |  609 | `	if( pVfs == 0 \|\| pVfs->xChgrp == 0 ){` |
|      - |  610 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  611 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  612 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  613 | `			ph7_function_name(pCtx)` |
|      - |  614 | `			);` |
|    ! 0 |  615 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  616 | `		return PH7_OK;` |
|      - |  617 | `	}` |
|      - |  618 | `	/* Point to the desired directory */` |
|      4 |  619 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  620 | `	/* Extract the user */` |
|      4 |  621 | `	zGroup = ph7_value_to_string(apArg[1],0);` |
|      - |  622 | `	/* Perform the requested operation */` |
|      4 |  623 | `	rc = pVfs->xChgrp(zPath,zGroup);` |
|      - |  624 | `	/* IO return value */` |
|      4 |  625 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 |  626 | `	return PH7_OK;` |
|      4 |  627 |  |
|      - |  628 | `/*` |
|      - |  629 | ` * int64 disk_free_space(string $directory)` |
|      - |  630 | ` *  Returns available space on filesystem or disk partition.` |
|      - |  631 | ` * Parameters` |
|      - |  632 | ` *  $directory` |
|      - |  633 | ` *   A directory of the filesystem or disk partition.` |
|      - |  634 | ` * Return` |
|      - |  635 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  636 | ` */` |
|      2 |  637 | `static int PH7_vfs_disk_free_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  638 |  |
|      - |  639 | `	const char *zPath;` |
|      - |  640 | `	ph7_int64 iSize;` |
|      - |  641 | `	ph7_vfs *pVfs;` |
|      3 |  642 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  643 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  644 | `		ph7_result_bool(pCtx,0);` |
|      3 |  645 | `		return PH7_OK;` |
|      - |  646 | `	}` |
|      - |  647 | `	/* Point to the underlying vfs */` |
|    ! 0 |  648 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    ! 0 |  649 | `	if( pVfs == 0 \|\| pVfs->xFreeSpace == 0 ){` |
|      - |  650 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  651 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  652 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  653 | `			ph7_function_name(pCtx)` |
|      - |  654 | `			);` |
|    ! 0 |  655 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  656 | `		return PH7_OK;` |
|      - |  657 | `	}` |
|      - |  658 | `	/* Point to the desired directory */` |
|    ! 0 |  659 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  660 | `	/* Perform the requested operation */` |
|    ! 0 |  661 | `	iSize = pVfs->xFreeSpace(zPath);` |
|      - |  662 | `	/* IO return value */` |
|    ! 0 |  663 | `	ph7_result_int64(pCtx,iSize);` |
|    ! 0 |  664 | `	return PH7_OK;` |
|      2 |  665 |  |
|      - |  666 | `/*` |
|      - |  667 | ` * int64 disk_total_space(string $directory)` |
|      - |  668 | ` *  Returns the total size of a filesystem or disk partition.` |
|      - |  669 | ` * Parameters` |
|      - |  670 | ` *  $directory` |
|      - |  671 | ` *   A directory of the filesystem or disk partition.` |
|      - |  672 | ` * Return` |
|      - |  673 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  674 | ` */` |
|      2 |  675 | `static int PH7_vfs_disk_total_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  676 |  |
|      - |  677 | `	const char *zPath;` |
|      - |  678 | `	ph7_int64 iSize;` |
|      - |  679 | `	ph7_vfs *pVfs;` |
|      2 |  680 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  681 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  682 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  683 | `		return PH7_OK;` |
|      - |  684 | `	}` |
|      - |  685 | `	/* Point to the underlying vfs */` |
|      2 |  686 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 |  687 | `	if( pVfs == 0 \|\| pVfs->xTotalSpace == 0 ){` |
|      - |  688 | `		/* IO routine not implemented,return NULL */` |
|      3 |  689 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  690 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|      1 |  691 | `			ph7_function_name(pCtx)` |
|      - |  692 | `			);` |
|      2 |  693 | `		ph7_result_bool(pCtx,0);` |
|      2 |  694 | `		return PH7_OK;` |
|      - |  695 | `	}` |
|      - |  696 | `	/* Point to the desired directory */` |
|    ! 0 |  697 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  698 | `	/* Perform the requested operation */` |
|    ! 0 |  699 | `	iSize = pVfs->xTotalSpace(zPath);` |
|      - |  700 | `	/* IO return value */` |
|    ! 0 |  701 | `	ph7_result_int64(pCtx,iSize);` |
|    ! 0 |  702 | `	return PH7_OK;` |
|      1 |  703 |  |
|      - |  704 | `/*` |
|      - |  705 | ` * bool file_exists(string $filename)` |
|      - |  706 | ` *  Checks whether a file or directory exists.` |
|      - |  707 | ` * Parameters` |
|      - |  708 | ` *  $filename` |
|      - |  709 | ` *   Path to the file.` |
|      - |  710 | ` * Return` |
|      - |  711 | ` *  TRUE on success or FALSE on failure.` |
|      - |  712 | ` */` |
|     46 |  713 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  714 |  |
|      - |  715 | `	const char *zPath;` |
|      - |  716 | `	ph7_vfs *pVfs;` |
|      - |  717 | `	int rc;` |
|     47 |  718 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  719 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  720 | `		ph7_result_bool(pCtx,0);` |
|      3 |  721 | `		return PH7_OK;` |
|      - |  722 | `	}` |
|      - |  723 | `	/* Point to the underlying vfs */` |
|     45 |  724 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     45 |  725 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - |  726 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  727 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  728 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  729 | `			ph7_function_name(pCtx)` |
|      - |  730 | `			);` |
|    ! 0 |  731 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  732 | `		return PH7_OK;` |
|      - |  733 | `	}` |
|      - |  734 | `	/* Point to the desired directory */` |
|     45 |  735 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  736 | `	/* Perform the requested operation */` |
|     45 |  737 | `	rc = pVfs->xFileExists(zPath);` |
|      - |  738 | `	/* IO return value */` |
|     45 |  739 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     45 |  740 | `	return PH7_OK;` |
|     24 |  741 |  |
|      - |  742 | `/*` |
|      - |  743 | ` * int64 file_size(string $filename)` |
|      - |  744 | ` *  Gets the size for the given file.` |
|      - |  745 | ` * Parameters` |
|      - |  746 | ` *  $filename` |
|      - |  747 | ` *   Path to the file.` |
|      - |  748 | ` * Return` |
|      - |  749 | ` *  File size on success or FALSE on failure.` |
|      - |  750 | ` */` |
|     26 |  751 | `static int PH7_vfs_file_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  752 |  |
|      - |  753 | `	const char *zPath;` |
|      - |  754 | `	ph7_int64 iSize;` |
|      - |  755 | `	ph7_vfs *pVfs;` |
|     27 |  756 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  757 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  758 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  759 | `		return PH7_OK;` |
|      - |  760 | `	}` |
|      - |  761 | `	/* Point to the underlying vfs */` |
|     27 |  762 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     27 |  763 | `	if( pVfs == 0 \|\| pVfs->xFileSize == 0 ){` |
|      - |  764 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  765 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  766 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  767 | `			ph7_function_name(pCtx)` |
|      - |  768 | `			);` |
|    ! 0 |  769 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  770 | `		return PH7_OK;` |
|      - |  771 | `	}` |
|      - |  772 | `	/* Point to the desired directory */` |
|     27 |  773 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  774 | `	/* Perform the requested operation */` |
|     27 |  775 | `	iSize = pVfs->xFileSize(zPath);` |
|      - |  776 | `	/* IO return value */` |
|     27 |  777 | `	ph7_result_int64(pCtx,iSize);` |
|     27 |  778 | `	return PH7_OK;` |
|     14 |  779 |  |
|      - |  780 | `/*` |
|      - |  781 | ` * int64 fileatime(string $filename)` |
|      - |  782 | ` *  Gets the last access time of the given file.` |
|      - |  783 | ` * Parameters` |
|      - |  784 | ` *  $filename` |
|      - |  785 | ` *   Path to the file.` |
|      - |  786 | ` * Return` |
|      - |  787 | ` *  File atime on success or FALSE on failure.` |
|      - |  788 | ` */` |
|      2 |  789 | `static int PH7_vfs_file_atime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  790 |  |
|      - |  791 | `	const char *zPath;` |
|      - |  792 | `	ph7_int64 iTime;` |
|      - |  793 | `	ph7_vfs *pVfs;` |
|      3 |  794 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  795 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  796 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  797 | `		return PH7_OK;` |
|      - |  798 | `	}` |
|      - |  799 | `	/* Point to the underlying vfs */` |
|      3 |  800 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  801 | `	if( pVfs == 0 \|\| pVfs->xFileAtime == 0 ){` |
|      - |  802 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  803 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  804 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  805 | `			ph7_function_name(pCtx)` |
|      - |  806 | `			);` |
|    ! 0 |  807 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  808 | `		return PH7_OK;` |
|      - |  809 | `	}` |
|      - |  810 | `	/* Point to the desired directory */` |
|      3 |  811 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  812 | `	/* Perform the requested operation */` |
|      3 |  813 | `	iTime = pVfs->xFileAtime(zPath);` |
|      - |  814 | `	/* IO return value */` |
|      3 |  815 | `	ph7_result_int64(pCtx,iTime);` |
|      3 |  816 | `	return PH7_OK;` |
|      2 |  817 |  |
|      - |  818 | `/*` |
|      - |  819 | ` * int64 filemtime(string $filename)` |
|      - |  820 | ` *  Gets file modification time.` |
|      - |  821 | ` * Parameters` |
|      - |  822 | ` *  $filename` |
|      - |  823 | ` *   Path to the file.` |
|      - |  824 | ` * Return` |
|      - |  825 | ` *  File mtime on success or FALSE on failure.` |
|      - |  826 | ` */` |
|      4 |  827 | `static int PH7_vfs_file_mtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  828 |  |
|      - |  829 | `	const char *zPath;` |
|      - |  830 | `	ph7_int64 iTime;` |
|      - |  831 | `	ph7_vfs *pVfs;` |
|      5 |  832 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  833 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  834 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  835 | `		return PH7_OK;` |
|      - |  836 | `	}` |
|      - |  837 | `	/* Point to the underlying vfs */` |
|      5 |  838 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  839 | `	if( pVfs == 0 \|\| pVfs->xFileMtime == 0 ){` |
|      - |  840 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  841 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  842 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  843 | `			ph7_function_name(pCtx)` |
|      - |  844 | `			);` |
|    ! 0 |  845 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  846 | `		return PH7_OK;` |
|      - |  847 | `	}` |
|      - |  848 | `	/* Point to the desired directory */` |
|      5 |  849 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  850 | `	/* Perform the requested operation */` |
|      5 |  851 | `	iTime = pVfs->xFileMtime(zPath);` |
|      - |  852 | `	/* IO return value */` |
|      5 |  853 | `	ph7_result_int64(pCtx,iTime);` |
|      5 |  854 | `	return PH7_OK;` |
|      3 |  855 |  |
|      - |  856 | `/*` |
|      - |  857 | ` * int64 filectime(string $filename)` |
|      - |  858 | ` *  Gets inode change time of file.` |
|      - |  859 | ` * Parameters` |
|      - |  860 | ` *  $filename` |
|      - |  861 | ` *   Path to the file.` |
|      - |  862 | ` * Return` |
|      - |  863 | ` *  File ctime on success or FALSE on failure.` |
|      - |  864 | ` */` |
|      2 |  865 | `static int PH7_vfs_file_ctime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  866 |  |
|      - |  867 | `	const char *zPath;` |
|      - |  868 | `	ph7_int64 iTime;` |
|      - |  869 | `	ph7_vfs *pVfs;` |
|      3 |  870 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  871 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  872 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  873 | `		return PH7_OK;` |
|      - |  874 | `	}` |
|      - |  875 | `	/* Point to the underlying vfs */` |
|      3 |  876 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  877 | `	if( pVfs == 0 \|\| pVfs->xFileCtime == 0 ){` |
|      - |  878 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  879 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  880 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  881 | `			ph7_function_name(pCtx)` |
|      - |  882 | `			);` |
|    ! 0 |  883 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  884 | `		return PH7_OK;` |
|      - |  885 | `	}` |
|      - |  886 | `	/* Point to the desired directory */` |
|      3 |  887 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  888 | `	/* Perform the requested operation */` |
|      3 |  889 | `	iTime = pVfs->xFileCtime(zPath);` |
|      - |  890 | `	/* IO return value */` |
|      3 |  891 | `	ph7_result_int64(pCtx,iTime);` |
|      3 |  892 | `	return PH7_OK;` |
|      2 |  893 |  |
|      - |  894 | `/*` |
|      - |  895 | ` * bool is_file(string $filename)` |
|      - |  896 | ` *  Tells whether the filename is a regular file.` |
|      - |  897 | ` * Parameters` |
|      - |  898 | ` *  $filename` |
|      - |  899 | ` *   Path to the file.` |
|      - |  900 | ` * Return` |
|      - |  901 | ` *  TRUE on success or FALSE on failure.` |
|      - |  902 | ` */` |
|   3924 |  903 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  904 |  |
|      - |  905 | `	const char *zPath;` |
|      - |  906 | `	ph7_vfs *pVfs;` |
|      - |  907 | `	int rc;` |
|   3926 |  908 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  909 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  910 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  911 | `		return PH7_OK;` |
|      - |  912 | `	}` |
|      - |  913 | `	/* Point to the underlying vfs */` |
|   3926 |  914 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   3926 |  915 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - |  916 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  917 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  918 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  919 | `			ph7_function_name(pCtx)` |
|      - |  920 | `			);` |
|    ! 0 |  921 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  922 | `		return PH7_OK;` |
|      - |  923 | `	}` |
|      - |  924 | `	/* Point to the desired directory */` |
|   3926 |  925 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  926 | `	/* Perform the requested operation */` |
|   3926 |  927 | `	rc = pVfs->xIsfile(zPath);` |
|      - |  928 | `	/* IO return value */` |
|   3926 |  929 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   3926 |  930 | `	return PH7_OK;` |
|   1964 |  931 |  |
|      - |  932 | `/*` |
|      - |  933 | ` * bool is_link(string $filename)` |
|      - |  934 | ` *  Tells whether the filename is a symbolic link.` |
|      - |  935 | ` * Parameters` |
|      - |  936 | ` *  $filename` |
|      - |  937 | ` *   Path to the file.` |
|      - |  938 | ` * Return` |
|      - |  939 | ` *  TRUE on success or FALSE on failure.` |
|      - |  940 | ` */` |
|      4 |  941 | `static int PH7_vfs_is_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  942 |  |
|      - |  943 | `	const char *zPath;` |
|      - |  944 | `	ph7_vfs *pVfs;` |
|      - |  945 | `	int rc;` |
|      4 |  946 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  947 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  948 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  949 | `		return PH7_OK;` |
|      - |  950 | `	}` |
|      - |  951 | `	/* Point to the underlying vfs */` |
|      4 |  952 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 |  953 | `	if( pVfs == 0 \|\| pVfs->xIslink == 0 ){` |
|      - |  954 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  955 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  956 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  957 | `			ph7_function_name(pCtx)` |
|      - |  958 | `			);` |
|    ! 0 |  959 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  960 | `		return PH7_OK;` |
|      - |  961 | `	}` |
|      - |  962 | `	/* Point to the desired directory */` |
|      4 |  963 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  964 | `	/* Perform the requested operation */` |
|      4 |  965 | `	rc = pVfs->xIslink(zPath);` |
|      - |  966 | `	/* IO return value */` |
|      4 |  967 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 |  968 | `	return PH7_OK;` |
|      2 |  969 |  |
|      - |  970 | `/*` |
|      - |  971 | ` * bool is_readable(string $filename)` |
|      - |  972 | ` *  Tells whether a file exists and is readable.` |
|      - |  973 | ` * Parameters` |
|      - |  974 | ` *  $filename` |
|      - |  975 | ` *   Path to the file.` |
|      - |  976 | ` * Return` |
|      - |  977 | ` *  TRUE on success or FALSE on failure.` |
|      - |  978 | ` */` |
|      2 |  979 | `static int PH7_vfs_is_readable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  980 |  |
|      - |  981 | `	const char *zPath;` |
|      - |  982 | `	ph7_vfs *pVfs;` |
|      - |  983 | `	int rc;` |
|      2 |  984 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  985 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  986 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  987 | `		return PH7_OK;` |
|      - |  988 | `	}` |
|      - |  989 | `	/* Point to the underlying vfs */` |
|      2 |  990 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 |  991 | `	if( pVfs == 0 \|\| pVfs->xReadable == 0 ){` |
|      - |  992 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  993 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  994 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  995 | `			ph7_function_name(pCtx)` |
|      - |  996 | `			);` |
|    ! 0 |  997 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  998 | `		return PH7_OK;` |
|      - |  999 | `	}` |
|      - | 1000 | `	/* Point to the desired directory */` |
|      2 | 1001 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1002 | `	/* Perform the requested operation */` |
|      2 | 1003 | `	rc = pVfs->xReadable(zPath);` |
|      - | 1004 | `	/* IO return value */` |
|      2 | 1005 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      2 | 1006 | `	return PH7_OK;` |
|      1 | 1007 |  |
|      - | 1008 | `/*` |
|      - | 1009 | ` * bool is_writable(string $filename)` |
|      - | 1010 | ` *  Tells whether the filename is writable.` |
|      - | 1011 | ` * Parameters` |
|      - | 1012 | ` *  $filename` |
|      - | 1013 | ` *   Path to the file.` |
|      - | 1014 | ` * Return` |
|      - | 1015 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1016 | ` */` |
|      8 | 1017 | `static int PH7_vfs_is_writable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1018 |  |
|      - | 1019 | `	const char *zPath;` |
|      - | 1020 | `	ph7_vfs *pVfs;` |
|      - | 1021 | `	int rc;` |
|      9 | 1022 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1023 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 1024 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1025 | `		return PH7_OK;` |
|      - | 1026 | `	}` |
|      - | 1027 | `	/* Point to the underlying vfs */` |
|      4 | 1028 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 | 1029 | `	if( pVfs == 0 \|\| pVfs->xWritable == 0 ){` |
|      - | 1030 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1031 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1032 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1033 | `			ph7_function_name(pCtx)` |
|      - | 1034 | `			);` |
|    ! 0 | 1035 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1036 | `		return PH7_OK;` |
|      - | 1037 | `	}` |
|      - | 1038 | `	/* Point to the desired directory */` |
|      4 | 1039 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1040 | `	/* Perform the requested operation */` |
|      4 | 1041 | `	rc = pVfs->xWritable(zPath);` |
|      - | 1042 | `	/* IO return value */` |
|      4 | 1043 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 | 1044 | `	return PH7_OK;` |
|      5 | 1045 |  |
|      - | 1046 | `/*` |
|      - | 1047 | ` * bool is_executable(string $filename)` |
|      - | 1048 | ` *  Tells whether the filename is executable.` |
|      - | 1049 | ` * Parameters` |
|      - | 1050 | ` *  $filename` |
|      - | 1051 | ` *   Path to the file.` |
|      - | 1052 | ` * Return` |
|      - | 1053 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1054 | ` */` |
|      2 | 1055 | `static int PH7_vfs_is_executable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1056 |  |
|      - | 1057 | `	const char *zPath;` |
|      - | 1058 | `	ph7_vfs *pVfs;` |
|      - | 1059 | `	int rc;` |
|      2 | 1060 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1061 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1062 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1063 | `		return PH7_OK;` |
|      - | 1064 | `	}` |
|      - | 1065 | `	/* Point to the underlying vfs */` |
|      2 | 1066 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 1067 | `	if( pVfs == 0 \|\| pVfs->xExecutable == 0 ){` |
|      - | 1068 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1069 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1070 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1071 | `			ph7_function_name(pCtx)` |
|      - | 1072 | `			);` |
|    ! 0 | 1073 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1074 | `		return PH7_OK;` |
|      - | 1075 | `	}` |
|      - | 1076 | `	/* Point to the desired directory */` |
|      2 | 1077 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1078 | `	/* Perform the requested operation */` |
|      2 | 1079 | `	rc = pVfs->xExecutable(zPath);` |
|      - | 1080 | `	/* IO return value */` |
|      2 | 1081 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      2 | 1082 | `	return PH7_OK;` |
|      1 | 1083 |  |
|      - | 1084 | `/*` |
|      - | 1085 | ` * string filetype(string $filename)` |
|      - | 1086 | ` *  Gets file type.` |
|      - | 1087 | ` * Parameters` |
|      - | 1088 | ` *  $filename` |
|      - | 1089 | ` *   Path to the file.` |
|      - | 1090 | ` * Return` |
|      - | 1091 | ` *  The type of the file. Possible values are fifo, char, dir, block, link` |
|      - | 1092 | ` *  file, socket and unknown.` |
|      - | 1093 | ` */` |
|      4 | 1094 | `static int PH7_vfs_filetype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1095 |  |
|      - | 1096 | `	const char *zPath;` |
|      - | 1097 | `	ph7_vfs *pVfs;` |
|      5 | 1098 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1099 | `		/* Missing/Invalid argument,return 'unknown' */` |
|    ! 0 | 1100 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 1101 | `		return PH7_OK;` |
|      - | 1102 | `	}` |
|      - | 1103 | `	/* Point to the underlying vfs */` |
|      5 | 1104 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1105 | `	if( pVfs == 0 \|\| pVfs->xFiletype == 0 ){` |
|      - | 1106 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1107 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1108 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1109 | `			ph7_function_name(pCtx)` |
|      - | 1110 | `			);` |
|    ! 0 | 1111 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1112 | `		return PH7_OK;` |
|      - | 1113 | `	}` |
|      - | 1114 | `	/* Point to the desired directory */` |
|      5 | 1115 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1116 | `	/* Set the empty string as the default return value */` |
|      5 | 1117 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1118 | `	/* Perform the requested operation */` |
|      5 | 1119 | `	pVfs->xFiletype(zPath,pCtx);` |
|      5 | 1120 | `	return PH7_OK;` |
|      3 | 1121 |  |
|      - | 1122 | `/*` |
|      - | 1123 | ` * array stat(string $filename)` |
|      - | 1124 | ` *  Gives information about a file.` |
|      - | 1125 | ` * Parameters` |
|      - | 1126 | ` *  $filename` |
|      - | 1127 | ` *   Path to the file.` |
|      - | 1128 | ` * Return` |
|      - | 1129 | ` *  An associative array on success holding the following entries on success` |
|      - | 1130 | ` *  0   dev     device number` |
|      - | 1131 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1132 | ` * 2    mode    inode protection mode` |
|      - | 1133 | ` * 3    nlink   number of links` |
|      - | 1134 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1135 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1136 | ` * 6    rdev    device type, if inode device` |
|      - | 1137 | ` * 7    size    size in bytes` |
|      - | 1138 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1139 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1140 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1141 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1142 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1143 | ` * Note:` |
|      - | 1144 | ` *  FALSE is returned on failure.` |
|      - | 1145 | ` */` |
|      4 | 1146 | `static int PH7_vfs_stat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1147 |  |
|      - | 1148 | `	ph7_value *pArray,*pValue;` |
|      - | 1149 | `	const char *zPath;` |
|      - | 1150 | `	ph7_vfs *pVfs;` |
|      - | 1151 | `	int rc;` |
|      5 | 1152 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1153 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1154 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1155 | `		return PH7_OK;` |
|      - | 1156 | `	}` |
|      - | 1157 | `	/* Point to the underlying vfs */` |
|      5 | 1158 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1159 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|      - | 1160 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1161 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1162 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1163 | `			ph7_function_name(pCtx)` |
|      - | 1164 | `			);` |
|    ! 0 | 1165 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1166 | `		return PH7_OK;` |
|      - | 1167 | `	}` |
|      - | 1168 | `	/* Create the array and the working value */` |
|      5 | 1169 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 1170 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 1171 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1172 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1173 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1174 | `		return PH7_OK;` |
|      - | 1175 | `	}` |
|      - | 1176 | `	/* Extract the file path */` |
|      5 | 1177 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1178 | `	/* Perform the requested operation */` |
|      5 | 1179 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|      5 | 1180 | `	if( rc != PH7_OK ){` |
|      - | 1181 | `		/* IO error,return FALSE */` |
|    ! 0 | 1182 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1183 | `	}else{` |
|      - | 1184 | `		/* Return the associative array */` |
|      5 | 1185 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1186 | `	}` |
|      - | 1187 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1188 | `	 * automatically as soon we return from this function. */` |
|      5 | 1189 | `	return PH7_OK;` |
|      3 | 1190 |  |
|      - | 1191 | `/*` |
|      - | 1192 | ` * array lstat(string $filename)` |
|      - | 1193 | ` *  Gives information about a file or symbolic link.` |
|      - | 1194 | ` * Parameters` |
|      - | 1195 | ` *  $filename` |
|      - | 1196 | ` *   Path to the file.` |
|      - | 1197 | ` * Return` |
|      - | 1198 | ` *  An associative array on success holding the following entries on success` |
|      - | 1199 | ` *  0   dev     device number` |
|      - | 1200 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1201 | ` * 2    mode    inode protection mode` |
|      - | 1202 | ` * 3    nlink   number of links` |
|      - | 1203 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1204 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1205 | ` * 6    rdev    device type, if inode device` |
|      - | 1206 | ` * 7    size    size in bytes` |
|      - | 1207 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1208 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1209 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1210 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1211 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1212 | ` * Note:` |
|      - | 1213 | ` *  FALSE is returned on failure.` |
|      - | 1214 | ` */` |
|      2 | 1215 | `static int PH7_vfs_lstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1216 |  |
|      - | 1217 | `	ph7_value *pArray,*pValue;` |
|      - | 1218 | `	const char *zPath;` |
|      - | 1219 | `	ph7_vfs *pVfs;` |
|      - | 1220 | `	int rc;` |
|      2 | 1221 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1222 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1223 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1224 | `		return PH7_OK;` |
|      - | 1225 | `	}` |
|      - | 1226 | `	/* Point to the underlying vfs */` |
|      2 | 1227 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 1228 | `	if( pVfs == 0 \|\| pVfs->xlStat == 0 ){` |
|      - | 1229 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1230 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1231 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1232 | `			ph7_function_name(pCtx)` |
|      - | 1233 | `			);` |
|    ! 0 | 1234 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1235 | `		return PH7_OK;` |
|      - | 1236 | `	}` |
|      - | 1237 | `	/* Create the array and the working value */` |
|      2 | 1238 | `	pArray = ph7_context_new_array(pCtx);` |
|      2 | 1239 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      2 | 1240 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1241 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1242 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1243 | `		return PH7_OK;` |
|      - | 1244 | `	}` |
|      - | 1245 | `	/* Extract the file path */` |
|      2 | 1246 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1247 | `	/* Perform the requested operation */` |
|      2 | 1248 | `	rc = pVfs->xlStat(zPath,pArray,pValue);` |
|      2 | 1249 | `	if( rc != PH7_OK ){` |
|      - | 1250 | `		/* IO error,return FALSE */` |
|    ! 0 | 1251 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1252 | `	}else{` |
|      - | 1253 | `		/* Return the associative array */` |
|      2 | 1254 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1255 | `	}` |
|      - | 1256 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1257 | `	 * automatically as soon we return from this function. */` |
|      2 | 1258 | `	return PH7_OK;` |
|      1 | 1259 |  |
|      - | 1260 | `/*` |
|      - | 1261 | ` * string getenv(string $varname)` |
|      - | 1262 | ` *  Gets the value of an environment variable.` |
|      - | 1263 | ` * Parameters` |
|      - | 1264 | ` *  $varname` |
|      - | 1265 | ` *   The variable name.` |
|      - | 1266 | ` * Return` |
|      - | 1267 | ` *  Returns the value of the environment variable varname, or FALSE if the environment` |
|      - | 1268 | ` * variable varname does not exist.` |
|      - | 1269 | ` */` |
|     16 | 1270 | `static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1271 |  |
|      - | 1272 | `	const char *zEnv;` |
|      - | 1273 | `	ph7_vfs *pVfs;` |
|      - | 1274 | `	int iLen;` |
|     18 | 1275 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1276 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1277 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1278 | `		return PH7_OK;` |
|      - | 1279 | `	}` |
|      - | 1280 | `	/* Point to the underlying vfs */` |
|     18 | 1281 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     18 | 1282 | `	if( pVfs == 0 \|\| pVfs->xGetenv == 0 ){` |
|      - | 1283 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1284 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1285 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1286 | `			ph7_function_name(pCtx)` |
|      - | 1287 | `			);` |
|    ! 0 | 1288 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1289 | `		return PH7_OK;` |
|      - | 1290 | `	}` |
|      - | 1291 | `	/* Extract the environment variable */` |
|     18 | 1292 | `	zEnv = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1293 | `	/* Set a boolean FALSE as the default return value */` |
|     18 | 1294 | `	ph7_result_bool(pCtx,0);` |
|     18 | 1295 | `	if( iLen < 1 ){` |
|      - | 1296 | `		/* Empty string */` |
|    ! 0 | 1297 | `		return PH7_OK;` |
|      - | 1298 | `	}` |
|      - | 1299 | `	/* Perform the requested operation */` |
|     18 | 1300 | `	pVfs->xGetenv(zEnv,pCtx);` |
|     18 | 1301 | `	return PH7_OK;` |
|     10 | 1302 |  |
|      - | 1303 | `/*` |
|      - | 1304 | ` * bool putenv(string $settings)` |
|      - | 1305 | ` *  Set the value of an environment variable.` |
|      - | 1306 | ` * Parameters` |
|      - | 1307 | ` *  $setting` |
|      - | 1308 | ` *   The setting, like "FOO=BAR"` |
|      - | 1309 | ` * Return` |
|      - | 1310 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1311 | ` */` |
|      6 | 1312 | `static int PH7_vfs_putenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1313 |  |
|      - | 1314 | `	const char *zName,*zValue;` |
|      - | 1315 | `	char *zSettings,*zEnd;` |
|      - | 1316 | `	ph7_vfs *pVfs;` |
|      - | 1317 | `	int iLen,rc;` |
|      7 | 1318 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1319 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1320 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1321 | `		return PH7_OK;` |
|      - | 1322 | `	}` |
|      - | 1323 | `	/* Extract the setting variable */` |
|      7 | 1324 | `	zSettings = (char *)ph7_value_to_string(apArg[0],&iLen);` |
|      7 | 1325 | `	if( iLen < 1 ){` |
|      - | 1326 | `		/* Empty string,return FALSE */` |
|    ! 0 | 1327 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1328 | `		return PH7_OK;` |
|      - | 1329 | `	}` |
|      - | 1330 | `	/* Parse the setting */` |
|      7 | 1331 | `	zEnd = &zSettings[iLen];` |
|      7 | 1332 | `	zValue = 0;` |
|      7 | 1333 | `	zName = zSettings;` |
|    127 | 1334 | `	while( zSettings < zEnd ){` |
|    127 | 1335 | `		if( zSettings[0] == '=' ){` |
|      - | 1336 | `			/* Null terminate the name */` |
|      7 | 1337 | `			zSettings[0] = 0;` |
|      7 | 1338 | `			zValue = &zSettings[1];` |
|      7 | 1339 | `			break;` |
|      - | 1340 | `		}` |
|    121 | 1341 | `		zSettings++;` |
|      1 | 1342 | `	}` |
|      - | 1343 | `	/* Install the environment variable in the $_Env array */` |
|      7 | 1344 | `	if( zValue == 0 \|\| zName[0] == 0 \|\| zValue >= zEnd \|\| zName >= zValue ){` |
|      - | 1345 | `		/* Invalid settings,retun FALSE */` |
|      5 | 1346 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1347 | `		if( zSettings  < zEnd ){` |
|      5 | 1348 | `			zSettings[0] = '=';` |
|      2 | 1349 | `		}` |
|      5 | 1350 | `		return PH7_OK;` |
|      - | 1351 | `	}` |
|      3 | 1352 | `	ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_ENV_ATTR,zName,zValue,(int)(zEnd-zValue));` |
|      - | 1353 | `	/* Point to the underlying vfs */` |
|      3 | 1354 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1355 | `	if( pVfs == 0 \|\| pVfs->xSetenv == 0 ){` |
|      - | 1356 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1357 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1358 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1359 | `			ph7_function_name(pCtx)` |
|      - | 1360 | `			);` |
|    ! 0 | 1361 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1362 | `		zSettings[0] = '=';` |
|    ! 0 | 1363 | `		return PH7_OK;` |
|      - | 1364 | `	}` |
|      - | 1365 | `	/* Perform the requested operation */` |
|      3 | 1366 | `	rc = pVfs->xSetenv(zName,zValue);` |
|      3 | 1367 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 1368 | `	zSettings[0] = '=';` |
|      3 | 1369 | `	return PH7_OK;` |
|      4 | 1370 |  |
|      - | 1371 | `/*` |
|      - | 1372 | ` * bool touch(string $filename[,int64 $time = time()[,int64 $atime]])` |
|      - | 1373 | ` *  Sets access and modification time of file.` |
|      - | 1374 | ` * Note: On windows` |
|      - | 1375 | ` *   If the file does not exists,it will not be created.` |
|      - | 1376 | ` * Parameters` |
|      - | 1377 | ` *  $filename` |
|      - | 1378 | ` *   The name of the file being touched.` |
|      - | 1379 | ` *  $time` |
|      - | 1380 | ` *   The touch time. If time is not supplied, the current system time is used.` |
|      - | 1381 | ` * $atime` |
|      - | 1382 | ` *   If present, the access time of the given filename is set to the value of atime.` |
|      - | 1383 | ` *   Otherwise, it is set to the value passed to the time parameter. If neither are` |
|      - | 1384 | ` *   present, the current system time is used.` |
|      - | 1385 | ` * Return` |
|      - | 1386 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1387 | `*/` |
|      4 | 1388 | `static int PH7_vfs_touch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1389 |  |
|      - | 1390 | `	ph7_int64 nTime,nAccess;` |
|      - | 1391 | `	const char *zFile;` |
|      - | 1392 | `	ph7_vfs *pVfs;` |
|      - | 1393 | `	int rc;` |
|      5 | 1394 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1395 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1396 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1397 | `		return PH7_OK;` |
|      - | 1398 | `	}` |
|      - | 1399 | `	/* Point to the underlying vfs */` |
|      5 | 1400 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1401 | `	if( pVfs == 0 \|\| pVfs->xTouch == 0 ){` |
|      - | 1402 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1403 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1404 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1405 | `			ph7_function_name(pCtx)` |
|      - | 1406 | `			);` |
|    ! 0 | 1407 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1408 | `		return PH7_OK;` |
|      - | 1409 | `	}` |
|      - | 1410 | `	/* Perform the requested operation */` |
|      5 | 1411 | `	nTime = nAccess = -1;` |
|      5 | 1412 | `	zFile = ph7_value_to_string(apArg[0],0);` |
|      5 | 1413 | `	if( nArg > 1 ){` |
|      2 | 1414 | `		nTime = ph7_value_to_int64(apArg[1]);` |
|      2 | 1415 | `		if( nArg > 2 ){` |
|      2 | 1416 | `			nAccess = ph7_value_to_int64(apArg[1]);` |
|      1 | 1417 | `		}else{` |
|    ! 0 | 1418 | `			nAccess = nTime;` |
|      - | 1419 | `		}` |
|      1 | 1420 | `	}` |
|      5 | 1421 | `	rc = pVfs->xTouch(zFile,nTime,nAccess);` |
|      - | 1422 | `	/* IO result */` |
|      5 | 1423 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 1424 | `	return PH7_OK;` |
|      3 | 1425 |  |
|      - | 1426 | `/*` |
|      - | 1427 | ` * Path processing functions that do not need access to the VFS layer` |
|      - | 1428 | ` * Status:` |
|      - | 1429 | ` *    Stable.` |
|      - | 1430 | ` */` |
|      - | 1431 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1432 | `/*` |
|      - | 1433 | ` * string dirname(string $path)` |
|      - | 1434 |  |
|      - | 1435 | ` *  Returns parent directory's path.` |
|      - | 1436 | ` * Parameters` |
|      - | 1437 | ` * $path` |
|      - | 1438 | ` *  Target path.` |
|      - | 1439 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1440 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1441 | ` * Return` |
|      - | 1442 | ` *  The path of the parent directory. If there are no slashes in path, a dot ('.')` |
|      - | 1443 | ` *  is returned, indicating the current directory.` |
|      - | 1444 | ` */` |
|     14 | 1445 | `static int PH7_builtin_dirname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1446 |  |
|      - | 1447 | `	const char *zPath,*zDir;` |
|      - | 1448 | `	int iLen,iDirlen;` |
|     16 | 1449 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1450 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 1451 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1452 | `		return PH7_OK;` |
|      - | 1453 | `	}` |
|      - | 1454 | `	/* Point to the target path */` |
|     16 | 1455 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     16 | 1456 | `	if( iLen < 1 ){` |
|      - | 1457 | `		/* Reuturn "." */` |
|      2 | 1458 | `		ph7_result_string(pCtx,".",sizeof(char));` |
|      2 | 1459 | `		return PH7_OK;` |
|      - | 1460 | `	}` |
|      - | 1461 | `	/* Perform the requested operation */` |
|     14 | 1462 | `	zDir = PH7_ExtractDirName(zPath,iLen,&iDirlen);` |
|      - | 1463 | `	/* Return directory name */` |
|     14 | 1464 | `	ph7_result_string(pCtx,zDir,iDirlen);` |
|     14 | 1465 | `	return PH7_OK;` |
|      9 | 1466 |  |
|      - | 1467 | `/*` |
|      - | 1468 | ` * string basename(string $path[, string $suffix ])` |
|      - | 1469 | ` *  Returns trailing name component of path.` |
|      - | 1470 | ` * Parameters` |
|      - | 1471 | ` * $path` |
|      - | 1472 | ` *  Target path.` |
|      - | 1473 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1474 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1475 | ` * $suffix` |
|      - | 1476 | ` *  If the name component ends in suffix this will also be cut off.` |
|      - | 1477 | ` * Return` |
|      - | 1478 | ` *  The base name of the given path.` |
|      - | 1479 | ` */` |
|     18 | 1480 | `static int PH7_builtin_basename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1481 |  |
|      - | 1482 | `	const char *zPath,*zBase,*zEnd;` |
|      - | 1483 | `	int c,d,iLen;` |
|     19 | 1484 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1485 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1486 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1487 | `		return PH7_OK;` |
|      - | 1488 | `	}` |
|     19 | 1489 | `	c = d = '/';` |
|      - | 1490 | `#ifdef __WINNT__` |
|      1 | 1491 | `	d = '\\';` |
|      - | 1492 | `#endif` |
|      - | 1493 | `	/* Point to the target path */` |
|     19 | 1494 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     19 | 1495 | `	if( iLen < 1 ){` |
|      - | 1496 | `		/* Empty string */` |
|      3 | 1497 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1498 | `		return PH7_OK;` |
|      - | 1499 | `	}` |
|      - | 1500 | `	/* Perform the requested operation */` |
|     17 | 1501 | `	zEnd = &zPath[iLen - 1];` |
|      - | 1502 | `	/* Ignore trailing '/' */` |
|     30 | 1503 | `	while( zEnd > zPath && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      6 | 1504 | `		zEnd--;` |
|      1 | 1505 | `	}` |
|     17 | 1506 | `	iLen = (int)(&zEnd[1]-zPath);` |
|    135 | 1507 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    111 | 1508 | `		zEnd--;` |
|      1 | 1509 | `	}` |
|     17 | 1510 | `	zBase = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|     17 | 1511 | `	zEnd = &zPath[iLen];` |
|     17 | 1512 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 1513 | `		const char *zSuffix;` |
|      - | 1514 | `		int nSuffix;` |
|      - | 1515 | `		/* Strip suffix */` |
|      5 | 1516 | `		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);` |
|      5 | 1517 | `		if( nSuffix > 0 && nSuffix < iLen && SyMemcmp(&zEnd[-nSuffix],zSuffix,nSuffix) == 0 ){` |
|      5 | 1518 | `			zEnd -= nSuffix;` |
|      2 | 1519 | `		}` |
|      2 | 1520 | `	}` |
|      - | 1521 | `	/* Store the basename */` |
|     17 | 1522 | `	ph7_result_string(pCtx,zBase,(int)(zEnd-zBase));` |
|     17 | 1523 | `	return PH7_OK;` |
|     10 | 1524 |  |
|      - | 1525 | `/*` |
|      - | 1526 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1527 | ` *  Returns information about a file path.` |
|      - | 1528 | ` * Parameter` |
|      - | 1529 | ` *  $path` |
|      - | 1530 | ` *   The path to be parsed.` |
|      - | 1531 | ` *  $options` |
|      - | 1532 | ` *    If present, specifies a specific element to be returned; one of` |
|      - | 1533 | ` *      PATHINFO_DIRNAME, PATHINFO_BASENAME, PATHINFO_EXTENSION or PATHINFO_FILENAME.` |
|      - | 1534 | ` * Return` |
|      - | 1535 | ` *  If the options parameter is not passed, an associative array containing the following` |
|      - | 1536 | ` *  elements is returned: dirname, basename, extension (if any), and filename.` |
|      - | 1537 | ` *  If options is present, returns a string containing the requested element.` |
|      - | 1538 | ` */` |
|      - | 1539 | `typedef struct path_info path_info;` |
|      - | 1540 | `struct path_info` |
|      - | 1541 |  |
|      - | 1542 | `	SyString sDir; /* Directory [i.e: /var/www] */` |
|      - | 1543 | `	SyString sBasename; /* Basename [i.e httpd.conf] */` |
|      - | 1544 | `	SyString sExtension; /* File extension [i.e xml,pdf..] */` |
|      - | 1545 | `	SyString sFilename;  /* Filename */` |
|      - | 1546 | `};` |
|      - | 1547 | `/*` |
|      - | 1548 | ` * Extract path fields.` |
|      - | 1549 | ` */` |
|   7846 | 1550 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      2 | 1551 |  |
|   7848 | 1552 | `	const char *zPtr,*zEnd = &zPath[nByte - 1];` |
|      - | 1553 | `	SyString *pCur;` |
|      - | 1554 | `	int c,d;` |
|   7848 | 1555 | `	c = d = '/';` |
|      - | 1556 | `#ifdef __WINNT__` |
|      2 | 1557 | `	d = '\\';` |
|      - | 1558 | `#endif` |
|      - | 1559 | `	/* Zero the structure */` |
|   7848 | 1560 | `	SyZero(pOut,sizeof(path_info));` |
|      - | 1561 | `	/* Handle special case */` |
|   7848 | 1562 | `	if( nByte == sizeof(char) && ( (int)zPath[0] == c \|\| (int)zPath[0] == d ) ){` |
|      - | 1563 | `#ifdef __WINNT__` |
|    ! 0 | 1564 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1565 | `#else` |
|    ! 0 | 1566 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1567 | `#endif` |
|    ! 0 | 1568 | `		return SXRET_OK;` |
|      - | 1569 | `	}` |
|      - | 1570 | `	/* Extract the basename */` |
| 198203 | 1571 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
| 186434 | 1572 | `		zEnd--;` |
|      2 | 1573 | `	}` |
|   7848 | 1574 | `	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|   7848 | 1575 | `	zEnd = &zPath[nByte];` |
|      - | 1576 | `	/* dirname */` |
|   7848 | 1577 | `	pCur = &pOut->sDir;` |
|   7848 | 1578 | `	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);` |
|   7848 | 1579 | `	if( pCur->nByte > 1 ){` |
|  15694 | 1580 | `		SyStringTrimTrailingChar(pCur,'/');` |
|      - | 1581 | `#ifdef __WINNT__` |
|      2 | 1582 | `		SyStringTrimTrailingChar(pCur,'\\');` |
|      - | 1583 | `#endif` |
|   3925 | 1584 | `	}else if( (int)zPath[0] == c \|\| (int)zPath[0] == d ){` |
|      - | 1585 | `#ifdef __WINNT__` |
|    ! 0 | 1586 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1587 | `#else` |
|    ! 0 | 1588 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1589 | `#endif` |
|    ! 0 | 1590 | `	}` |
|      - | 1591 | `	/* basename/filename */` |
|   7848 | 1592 | `	pCur = &pOut->sBasename;` |
|   7848 | 1593 | `	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);` |
|   7848 | 1594 | `	SyStringTrimLeadingChar(pCur,'/');` |
|      - | 1595 | `#ifdef __WINNT__` |
|      2 | 1596 | `	SyStringTrimLeadingChar(pCur,'\\');` |
|      - | 1597 | `#endif` |
|   7848 | 1598 | `	SyStringDupPtr(&pOut->sFilename,pCur);` |
|   7848 | 1599 | `	if( pCur->nByte > 0 ){` |
|      - | 1600 | `		/* extension */` |
|   7848 | 1601 | `		zEnd--;` |
|  39230 | 1602 | `		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){` |
|  31384 | 1603 | `			zEnd--;` |
|      2 | 1604 | `		}` |
|   7848 | 1605 | `		if( zEnd > pCur->zString ){` |
|   7846 | 1606 | `			zEnd++; /* Jump leading dot */` |
|   7846 | 1607 | `			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);` |
|      - | 1608 | `			/* Fix filename */` |
|   7846 | 1609 | `			pCur = &pOut->sFilename;` |
|   7846 | 1610 | `			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){` |
|   7846 | 1611 | `				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);` |
|   3922 | 1612 | `			}` |
|   3922 | 1613 | `		}` |
|   3923 | 1614 | `	}` |
|   7848 | 1615 | `	return SXRET_OK;` |
|   3925 | 1616 |  |
|      - | 1617 | `/*` |
|      - | 1618 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1619 | ` *  See block comment above.` |
|      - | 1620 | ` */` |
|   7846 | 1621 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1622 |  |
|      - | 1623 | `	const char *zPath;` |
|      - | 1624 | `	path_info sInfo;` |
|      - | 1625 | `	SyString *pComp;` |
|      - | 1626 | `	int iLen;` |
|   7848 | 1627 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1628 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1629 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1630 | `		return PH7_OK;` |
|      - | 1631 | `	}` |
|      - | 1632 | `	/* Point to the target path */` |
|   7848 | 1633 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|   7848 | 1634 | `	if( iLen < 1 ){` |
|      - | 1635 | `		/* Empty string */` |
|    ! 0 | 1636 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1637 | `		return PH7_OK;` |
|      - | 1638 | `	}` |
|      - | 1639 | `	/* Extract path info */` |
|   7848 | 1640 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|  11770 | 1641 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1642 | `		/* Return path component */` |
|   7846 | 1643 | `		int nComp = ph7_value_to_int(apArg[1]);` |
|   7846 | 1644 | `		switch(nComp){` |
|      1 | 1645 | `		case 1: /* PATHINFO_DIRNAME */` |
|      3 | 1646 | `			pComp = &sInfo.sDir;` |
|      3 | 1647 | `			if( pComp->nByte > 0 ){` |
|      3 | 1648 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1649 | `			}else{` |
|      - | 1650 | `				/* Expand the empty string */` |
|    ! 0 | 1651 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1652 | `			}` |
|      3 | 1653 | `			break;` |
|      1 | 1654 | `		case 2: /*PATHINFO_BASENAME*/` |
|      3 | 1655 | `			pComp = &sInfo.sBasename;` |
|      3 | 1656 | `			if( pComp->nByte > 0 ){` |
|      3 | 1657 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1658 | `			}else{` |
|      - | 1659 | `				/* Expand the empty string */` |
|    ! 0 | 1660 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1661 | `			}` |
|      3 | 1662 | `			break;` |
|   1961 | 1663 | `		case 3: /*PATHINFO_EXTENSION*/` |
|   3924 | 1664 | `			pComp = &sInfo.sExtension;` |
|   3924 | 1665 | `			if( pComp->nByte > 0 ){` |
|   3922 | 1666 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   1962 | 1667 | `			}else{` |
|      - | 1668 | `				/* Expand the empty string */` |
|      3 | 1669 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1670 | `			}` |
|   3924 | 1671 | `			break;` |
|   1959 | 1672 | `		case 4: /*PATHINFO_FILENAME*/` |
|   3920 | 1673 | `			pComp = &sInfo.sFilename;` |
|   3920 | 1674 | `			if( pComp->nByte > 0 ){` |
|   3920 | 1675 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   1961 | 1676 | `			}else{` |
|      - | 1677 | `				/* Expand the empty string */` |
|    ! 0 | 1678 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1679 | `			}` |
|   3920 | 1680 | `			break;` |
|    ! 0 | 1681 | `		default:` |
|      - | 1682 | `			/* Expand the empty string */` |
|    ! 0 | 1683 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1684 | `			break;` |
|      - | 1685 | `		}` |
|   3924 | 1686 | `	}else{` |
|      - | 1687 | `		/* Return an associative array */` |
|      - | 1688 | `		ph7_value *pArray,*pValue;` |
|      3 | 1689 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 1690 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 1691 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1692 | `			/* Out of mem,return NULL */` |
|    ! 0 | 1693 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1694 | `			return PH7_OK;` |
|      - | 1695 | `		}` |
|      - | 1696 | `		/* dirname */` |
|      3 | 1697 | `		pComp = &sInfo.sDir;` |
|      3 | 1698 | `		if( pComp->nByte > 0 ){` |
|      3 | 1699 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1700 | `			/* Perform the insertion */` |
|      3 | 1701 | `			ph7_array_add_strkey_elem(pArray,"dirname",pValue); /* Will make it's own copy */` |
|      1 | 1702 | `		}` |
|      - | 1703 | `		/* Reset the string cursor */` |
|      3 | 1704 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1705 | `		/* basername */` |
|      3 | 1706 | `		pComp = &sInfo.sBasename;` |
|      3 | 1707 | `		if( pComp->nByte > 0 ){` |
|      3 | 1708 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1709 | `			/* Perform the insertion */` |
|      3 | 1710 | `			ph7_array_add_strkey_elem(pArray,"basename",pValue); /* Will make it's own copy */` |
|      1 | 1711 | `		}` |
|      - | 1712 | `		/* Reset the string cursor */` |
|      3 | 1713 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1714 | `		/* extension */` |
|      3 | 1715 | `		pComp = &sInfo.sExtension;` |
|      3 | 1716 | `		if( pComp->nByte > 0 ){` |
|      3 | 1717 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1718 | `			/* Perform the insertion */` |
|      3 | 1719 | `			ph7_array_add_strkey_elem(pArray,"extension",pValue); /* Will make it's own copy */` |
|      1 | 1720 | `		}` |
|      - | 1721 | `		/* Reset the string cursor */` |
|      3 | 1722 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1723 | `		/* filename */` |
|      3 | 1724 | `		pComp = &sInfo.sFilename;` |
|      3 | 1725 | `		if( pComp->nByte > 0 ){` |
|      3 | 1726 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1727 | `			/* Perform the insertion */` |
|      3 | 1728 | `			ph7_array_add_strkey_elem(pArray,"filename",pValue); /* Will make it's own copy */` |
|      1 | 1729 | `		}` |
|      - | 1730 | `		/* Return the created array */` |
|      3 | 1731 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1732 | `		/* Don't worry about freeing memory, everything will be released` |
|      - | 1733 | `		 * automatically as soon we return from this foreign function.` |
|      - | 1734 | `		 */` |
|      - | 1735 | `	}` |
|   7848 | 1736 | `	return PH7_OK;` |
|   3925 | 1737 |  |
|      - | 1738 | `/*` |
|      - | 1739 | ` * Globbing implementation extracted from the sqlite3 source tree.` |
|      - | 1740 |  |
|      - | 1741 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|      - | 1742 | ` * Status: Public Domain` |
|      - | 1743 | ` */` |
|      - | 1744 | `typedef unsigned char u8;` |
|      - | 1745 | `/* An array to map all upper-case characters into their corresponding` |
|      - | 1746 | `** lower-case character.` |
|      - | 1747 | `**` |
|      - | 1748 | `** SQLite only considers US-ASCII (or EBCDIC) characters.  We do not` |
|      - | 1749 | `** handle case conversions for the UTF character set since the tables` |
|      - | 1750 | `** involved are nearly as big or bigger than SQLite itself.` |
|      - | 1751 | `*/` |
|      - | 1752 | `static const unsigned char sqlite3UpperToLower[] = {` |
|      - | 1753 |  |
|      - | 1754 | `     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,` |
|      - | 1755 | `     36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,` |
|      - | 1756 | `     54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,` |
|      - | 1757 | `    104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,` |
|      - | 1758 | `    122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,` |
|      - | 1759 | `    108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,` |
|      - | 1760 | `    126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,` |
|      - | 1761 | `    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,` |
|      - | 1762 | `    162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,` |
|      - | 1763 | `    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,` |
|      - | 1764 | `    198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,` |
|      - | 1765 | `    216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,` |
|      - | 1766 | `    234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,` |
|      - | 1767 | `    252,253,254,255` |
|      - | 1768 | `};` |
|      - | 1769 | `#define GlogUpperToLower(A)     if( A<0x80 ){ A = sqlite3UpperToLower[A]; }` |
|      - | 1770 | `/*` |
|      - | 1771 | `** Assuming zIn points to the first byte of a UTF-8 character,` |
|      - | 1772 | `** advance zIn to point to the first byte of the next UTF-8 character.` |
|      - | 1773 | `*/` |
|      - | 1774 | `#define SQLITE_SKIP_UTF8(zIn) {                        \` |
|      - | 1775 | `  if( (*(zIn++))>=0xc0 ){                              \` |
|      - | 1776 | `    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \` |
|      - | 1777 | `  }                                                    \` |
|      - | 1778 |  |
|      - | 1779 | `/*` |
|      - | 1780 | `** Compare two UTF-8 strings for equality where the first string can` |
|      - | 1781 | `** potentially be a "glob" expression.  Return true (1) if they` |
|      - | 1782 | `** are the same and false (0) if they are different.` |
|      - | 1783 | `**` |
|      - | 1784 | `** Globbing rules:` |
|      - | 1785 | `**` |
|      - | 1786 | `**      '*'       Matches any sequence of zero or more characters.` |
|      - | 1787 | `**` |
|      - | 1788 | `**      '?'       Matches exactly one character.` |
|      - | 1789 | `**` |
|      - | 1790 | `**     [...]      Matches one character from the enclosed list of` |
|      - | 1791 | `**                characters.` |
|      - | 1792 | `**` |
|      - | 1793 | `**     [^...]     Matches one character not in the enclosed list.` |
|      - | 1794 | `**` |
|      - | 1795 | `** With the [...] and [^...] matching, a ']' character can be included` |
|      - | 1796 | `** in the list by making it the first character after '[' or '^'.  A` |
|      - | 1797 | `** range of characters can be specified using '-'.  Example:` |
|      - | 1798 | `** "[a-z]" matches any single lower-case letter.  To match a '-', make` |
|      - | 1799 | `** it the last character in the list.` |
|      - | 1800 | `**` |
|      - | 1801 | `** This routine is usually quick, but can be N**2 in the worst case.` |
|      - | 1802 | `**` |
|      - | 1803 | `** Hints: to match '*' or '?', put them in "[]".  Like this:` |
|      - | 1804 | `**` |
|      - | 1805 | `**         abc[*]xyz        Matches "abc*xyz" only` |
|      - | 1806 | `*/` |
|     20 | 1807 | `static int patternCompare(` |
|      - | 1808 | `  const u8 *zPattern,              /* The glob pattern */` |
|      - | 1809 | `  const u8 *zString,               /* The string to compare against the glob */` |
|      - | 1810 | `  const int esc,                    /* The escape character */` |
|      - | 1811 | `  int noCase` |
|      1 | 1812 | `){` |
|      - | 1813 | `  int c, c2;` |
|      - | 1814 | `  int invert;` |
|      - | 1815 | `  int seen;` |
|     21 | 1816 | `  u8 matchOne = '?';` |
|     21 | 1817 | `  u8 matchAll = '*';` |
|     21 | 1818 | `  u8 matchSet = '[';` |
|     21 | 1819 | `  int prevEscape = 0;     /* True if the previous character was 'escape' */` |
|      - | 1820 |  |
|     21 | 1821 | `  if( !zPattern \|\| !zString ) return 0;` |
|     51 | 1822 | `  while( (c = PH7_Utf8Read(zPattern,0,&zPattern))!=0 ){` |
|     43 | 1823 | `    if( !prevEscape && c==matchAll ){` |
|     16 | 1824 | `      while( (c=PH7_Utf8Read(zPattern,0,&zPattern)) == matchAll` |
|      9 | 1825 | `               \|\| c == matchOne ){` |
|    ! 0 | 1826 | `        if( c==matchOne && PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1827 | `          return 0;` |
|      - | 1828 | `        }` |
|    ! 0 | 1829 | `      }` |
|      9 | 1830 | `      if( c==0 ){` |
|    ! 0 | 1831 | `        return 1;` |
|      9 | 1832 | `      }else if( c==esc ){` |
|    ! 0 | 1833 | `        c = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1834 | `        if( c==0 ){` |
|    ! 0 | 1835 | `          return 0;` |
|    ! 0 | 1836 | `        }` |
|      9 | 1837 | `      }else if( c==matchSet ){` |
|    ! 0 | 1838 | `	  if( (esc==0) \|\| (matchSet<0x80) ) return 0;` |
|    ! 0 | 1839 | `	  while( *zString && patternCompare(&zPattern[-1],zString,esc,noCase)==0 ){` |
|    ! 0 | 1840 | `          SQLITE_SKIP_UTF8(zString);` |
|    ! 0 | 1841 | `        }` |
|    ! 0 | 1842 | `        return *zString!=0;` |
|      - | 1843 | `      }` |
|     11 | 1844 | `      while( (c2 = PH7_Utf8Read(zString,0,&zString))!=0 ){` |
|     11 | 1845 | `        if( noCase ){` |
|      3 | 1846 | `          GlogUpperToLower(c2);` |
|      3 | 1847 | `          GlogUpperToLower(c);` |
|     11 | 1848 | `          while( c2 != 0 && c2 != c ){` |
|      9 | 1849 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      9 | 1850 | `            GlogUpperToLower(c2);` |
|      1 | 1851 | `          }` |
|      2 | 1852 | `        }else{` |
|     47 | 1853 | `          while( c2 != 0 && c2 != c ){` |
|     39 | 1854 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      1 | 1855 | `          }` |
|      - | 1856 | `        }` |
|     11 | 1857 | `        if( c2==0 ) return 0;` |
|      9 | 1858 | `		if( patternCompare(zPattern,zString,esc,noCase) ) return 1;` |
|      1 | 1859 | `      }` |
|    ! 0 | 1860 | `      return 0;` |
|     35 | 1861 | `    }else if( !prevEscape && c==matchOne ){` |
|    ! 0 | 1862 | `      if( PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1863 | `        return 0;` |
|    ! 0 | 1864 | `      }` |
|     35 | 1865 | `    }else if( c==matchSet ){` |
|    ! 0 | 1866 | `      int prior_c = 0;` |
|    ! 0 | 1867 | `      if( esc == 0 ) return 0;` |
|    ! 0 | 1868 | `      seen = 0;` |
|    ! 0 | 1869 | `      invert = 0;` |
|    ! 0 | 1870 | `      c = PH7_Utf8Read(zString, 0, &zString);` |
|    ! 0 | 1871 | `      if( c==0 ) return 0;` |
|    ! 0 | 1872 | `      c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1873 | `      if( c2=='^' ){` |
|    ! 0 | 1874 | `        invert = 1;` |
|    ! 0 | 1875 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1876 | `      }` |
|    ! 0 | 1877 | `      if( c2==']' ){` |
|    ! 0 | 1878 | `        if( c==']' ) seen = 1;` |
|    ! 0 | 1879 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1880 | `      }` |
|    ! 0 | 1881 | `      while( c2 && c2!=']' ){` |
|    ! 0 | 1882 | `        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){` |
|    ! 0 | 1883 | `          c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1884 | `          if( c>=prior_c && c<=c2 ) seen = 1;` |
|    ! 0 | 1885 | `          prior_c = 0;` |
|    ! 0 | 1886 | `        }else{` |
|    ! 0 | 1887 | `          if( c==c2 ){` |
|    ! 0 | 1888 | `            seen = 1;` |
|    ! 0 | 1889 | `          }` |
|    ! 0 | 1890 | `          prior_c = c2;` |
|      - | 1891 | `        }` |
|    ! 0 | 1892 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1893 | `      }` |
|    ! 0 | 1894 | `      if( c2==0 \|\| (seen ^ invert)==0 ){` |
|    ! 0 | 1895 | `        return 0;` |
|    ! 0 | 1896 | `      }` |
|     35 | 1897 | `    }else if( esc==c && !prevEscape ){` |
|    ! 0 | 1898 | `      prevEscape = 1;` |
|    ! 0 | 1899 | `    }else{` |
|     35 | 1900 | `      c2 = PH7_Utf8Read(zString, 0, &zString);` |
|     35 | 1901 | `      if( noCase ){` |
|      7 | 1902 | `        GlogUpperToLower(c);` |
|      7 | 1903 | `        GlogUpperToLower(c2);` |
|      3 | 1904 | `      }` |
|     35 | 1905 | `      if( c!=c2 ){` |
|      5 | 1906 | `        return 0;` |
|      - | 1907 | `      }` |
|     31 | 1908 | `      prevEscape = 0;` |
|      - | 1909 | `    }` |
|      1 | 1910 | `  }` |
|      9 | 1911 | `  return *zString==0;` |
|     11 | 1912 |  |
|      - | 1913 | `/*` |
|      - | 1914 | ` * Wrapper around patternCompare() defined above.` |
|      - | 1915 | ` * See block comment above for more information.` |
|      - | 1916 | ` */` |
|     12 | 1917 | `static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,int CaseCompare)` |
|      1 | 1918 |  |
|      - | 1919 | `	int rc;` |
|     13 | 1920 | `	if( iEsc < 0 ){` |
|    ! 0 | 1921 | `		iEsc = '\\';` |
|    ! 0 | 1922 | `	}` |
|     13 | 1923 | `	rc = patternCompare(zPattern,zString,iEsc,CaseCompare);` |
|     13 | 1924 | `	return rc;` |
|      1 | 1925 |  |
|      - | 1926 | `/*` |
|      - | 1927 | ` * bool fnmatch(string $pattern,string $string[,int $flags = 0 ])` |
|      - | 1928 | ` *  Match filename against a pattern.` |
|      - | 1929 | ` * Parameters` |
|      - | 1930 | ` *  $pattern` |
|      - | 1931 | ` *   The shell wildcard pattern.` |
|      - | 1932 | ` * $string` |
|      - | 1933 | ` *  The tested string.` |
|      - | 1934 | ` * $flags` |
|      - | 1935 | ` *   A list of possible flags:` |
|      - | 1936 | ` *    FNM_NOESCAPE 	Disable backslash escaping.` |
|      - | 1937 | ` *    FNM_PATHNAME 	Slash in string only matches slash in the given pattern.` |
|      - | 1938 | ` *    FNM_PERIOD 	Leading period in string must be exactly matched by period in the given pattern.` |
|      - | 1939 | ` *    FNM_CASEFOLD 	Caseless match.` |
|      - | 1940 | ` * Return` |
|      - | 1941 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 1942 | ` */` |
|      8 | 1943 | `static int PH7_builtin_fnmatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1944 |  |
|      - | 1945 | `	const char *zString,*zPattern;` |
|      9 | 1946 | `	int iEsc = '\\';` |
|      9 | 1947 | `	int noCase = 0;` |
|      - | 1948 | `	int rc;` |
|      9 | 1949 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 1950 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1951 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1952 | `		return PH7_OK;` |
|      - | 1953 | `	}` |
|      - | 1954 | `	/* Extract the pattern and the string */` |
|      9 | 1955 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|      9 | 1956 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 1957 | `	/* Extract the flags if avaialble */` |
|      9 | 1958 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      7 | 1959 | `		rc = ph7_value_to_int(apArg[2]);` |
|      7 | 1960 | `		if( rc & 0x01 /*FNM_NOESCAPE*/){` |
|    ! 0 | 1961 | `			iEsc = 0;` |
|    ! 0 | 1962 | `		}` |
|      7 | 1963 | `		if( rc & 0x08 /*FNM_CASEFOLD*/){` |
|      3 | 1964 | `			noCase = 1;` |
|      1 | 1965 | `		}` |
|      3 | 1966 | `	}` |
|      - | 1967 | `	/* Go globbing */` |
|      9 | 1968 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,noCase);` |
|      - | 1969 | `	/* Globbing result */` |
|      9 | 1970 | `	ph7_result_bool(pCtx,rc);` |
|      9 | 1971 | `	return PH7_OK;` |
|      5 | 1972 |  |
|      - | 1973 | `/*` |
|      - | 1974 | ` * bool strglob(string $pattern,string $string)` |
|      - | 1975 | ` *  Match string against a pattern.` |
|      - | 1976 | ` * Parameters` |
|      - | 1977 | ` *  $pattern` |
|      - | 1978 | ` *   The shell wildcard pattern.` |
|      - | 1979 | ` * $string` |
|      - | 1980 | ` *  The tested string.` |
|      - | 1981 | ` * Return` |
|      - | 1982 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 1983 | ` * Note that this a symisc eXtension.` |
|      - | 1984 | ` */` |
|      4 | 1985 | `static int PH7_builtin_strglob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1986 |  |
|      - | 1987 | `	const char *zString,*zPattern;` |
|      5 | 1988 | `	int iEsc = '\\';` |
|      - | 1989 | `	int rc;` |
|      5 | 1990 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 1991 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1992 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1993 | `		return PH7_OK;` |
|      - | 1994 | `	}` |
|      - | 1995 | `	/* Extract the pattern and the string */` |
|      5 | 1996 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|      5 | 1997 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 1998 | `	/* Go globbing */` |
|      5 | 1999 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0);` |
|      - | 2000 | `	/* Globbing result */` |
|      5 | 2001 | `	ph7_result_bool(pCtx,rc);` |
|      5 | 2002 | `	return PH7_OK;` |
|      3 | 2003 |  |
|      - | 2004 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2005 | `/*` |
|      - | 2006 | ` * bool link(string $target,string $link)` |
|      - | 2007 |  |
|      - | 2008 | ` *  Create a hard link.` |
|      - | 2009 | ` * Parameters` |
|      - | 2010 | ` *  $target` |
|      - | 2011 | ` *   Target of the link.` |
|      - | 2012 | ` *  $link` |
|      - | 2013 | ` *   The link name.` |
|      - | 2014 | ` * Return` |
|      - | 2015 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2016 | ` */` |
|      2 | 2017 | `static int PH7_vfs_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2018 |  |
|      - | 2019 | `	const char *zTarget,*zLink;` |
|      - | 2020 | `	ph7_vfs *pVfs;` |
|      - | 2021 | `	int rc;` |
|      2 | 2022 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2023 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2024 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2025 | `		return PH7_OK;` |
|      - | 2026 | `	}` |
|      - | 2027 | `	/* Point to the underlying vfs */` |
|      2 | 2028 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 2029 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2030 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2031 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2032 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2033 | `			ph7_function_name(pCtx)` |
|      - | 2034 | `			);` |
|    ! 0 | 2035 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2036 | `		return PH7_OK;` |
|      - | 2037 | `	}` |
|      - | 2038 | `	/* Extract the given arguments */` |
|      2 | 2039 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      2 | 2040 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2041 | `	/* Perform the requested operation */` |
|      2 | 2042 | `	rc = pVfs->xLink(zTarget,zLink,0/*Not a symbolic link */);` |
|      - | 2043 | `	/* IO result */` |
|      2 | 2044 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      2 | 2045 | `	return PH7_OK;` |
|      1 | 2046 |  |
|      - | 2047 | `/*` |
|      - | 2048 | ` * bool symlink(string $target,string $link)` |
|      - | 2049 | ` *  Creates a symbolic link.` |
|      - | 2050 | ` * Parameters` |
|      - | 2051 | ` *  $target` |
|      - | 2052 | ` *   Target of the link.` |
|      - | 2053 | ` *  $link` |
|      - | 2054 | ` *   The link name.` |
|      - | 2055 | ` * Return` |
|      - | 2056 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2057 | ` */` |
|      6 | 2058 | `static int PH7_vfs_symlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2059 |  |
|      - | 2060 | `	const char *zTarget,*zLink;` |
|      - | 2061 | `	ph7_vfs *pVfs;` |
|      - | 2062 | `	int rc;` |
|      6 | 2063 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2064 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2065 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2066 | `		return PH7_OK;` |
|      - | 2067 | `	}` |
|      - | 2068 | `	/* Point to the underlying vfs */` |
|      6 | 2069 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      6 | 2070 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2071 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2072 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2073 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2074 | `			ph7_function_name(pCtx)` |
|      - | 2075 | `			);` |
|    ! 0 | 2076 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2077 | `		return PH7_OK;` |
|      - | 2078 | `	}` |
|      - | 2079 | `	/* Extract the given arguments */` |
|      6 | 2080 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      6 | 2081 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2082 | `	/* Perform the requested operation */` |
|      6 | 2083 | `	rc = pVfs->xLink(zTarget,zLink,1/*A symbolic link */);` |
|      - | 2084 | `	/* IO result */` |
|      6 | 2085 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      6 | 2086 | `	return PH7_OK;` |
|      3 | 2087 |  |
|      - | 2088 | `/*` |
|      - | 2089 | ` * int umask([ int $mask ])` |
|      - | 2090 | ` *  Changes the current umask.` |
|      - | 2091 | ` * Parameters` |
|      - | 2092 | ` *  $mask` |
|      - | 2093 | ` *   The new umask.` |
|      - | 2094 | ` * Return` |
|      - | 2095 | ` *  umask() without arguments simply returns the current umask.` |
|      - | 2096 | ` *  Otherwise the old umask is returned.` |
|      - | 2097 | ` */` |
|      8 | 2098 | `static int PH7_vfs_umask(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2099 |  |
|      - | 2100 | `	int iOld,iNew;` |
|      - | 2101 | `	ph7_vfs *pVfs;` |
|      - | 2102 | `	/* Point to the underlying vfs */` |
|      8 | 2103 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      8 | 2104 | `	if( pVfs == 0 \|\| pVfs->xUmask == 0 ){` |
|      - | 2105 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2106 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2107 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2108 | `			ph7_function_name(pCtx)` |
|      - | 2109 | `			);` |
|    ! 0 | 2110 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2111 | `		return PH7_OK;` |
|      - | 2112 | `	}` |
|      8 | 2113 | `	iNew = 0;` |
|      8 | 2114 | `	if( nArg > 0 ){` |
|      4 | 2115 | `		iNew = ph7_value_to_int(apArg[0]);` |
|      2 | 2116 | `	}` |
|      - | 2117 | `	/* Perform the requested operation */` |
|      8 | 2118 | `	iOld = pVfs->xUmask(iNew);` |
|      - | 2119 | `	/* Old mask */` |
|      8 | 2120 | `	ph7_result_int(pCtx,iOld);` |
|      8 | 2121 | `	return PH7_OK;` |
|      4 | 2122 |  |
|      - | 2123 | `/*` |
|      - | 2124 | ` * string sys_get_temp_dir()` |
|      - | 2125 | ` *  Returns directory path used for temporary files.` |
|      - | 2126 | ` * Parameters` |
|      - | 2127 | ` *  None` |
|      - | 2128 | ` * Return` |
|      - | 2129 | ` *  Returns the path of the temporary directory.` |
|      - | 2130 | ` */` |
|    166 | 2131 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2132 |  |
|      - | 2133 | `	ph7_vfs *pVfs;` |
|      - | 2134 | `	/* Set the empty string as the default return value */` |
|    168 | 2135 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2136 | `	/* Point to the underlying vfs */` |
|    168 | 2137 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    168 | 2138 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
|    ! 0 | 2139 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2140 | `		SXUNUSED(apArg);` |
|      - | 2141 | `		/* IO routine not implemented,return "" */` |
|    ! 0 | 2142 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2143 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2144 | `			ph7_function_name(pCtx)` |
|      - | 2145 | `			);` |
|    ! 0 | 2146 | `		return PH7_OK;` |
|      - | 2147 | `	}` |
|      - | 2148 | `	/* Perform the requested operation */` |
|    168 | 2149 | `	pVfs->xTempDir(pCtx);` |
|    168 | 2150 | `	return PH7_OK;` |
|     85 | 2151 |  |
|      - | 2152 | `/*` |
|      - | 2153 | ` * string get_current_user()` |
|      - | 2154 | ` *  Returns the name of the current working user.` |
|      - | 2155 | ` * Parameters` |
|      - | 2156 | ` *  None` |
|      - | 2157 | ` * Return` |
|      - | 2158 | ` *  Returns the name of the current working user.` |
|      - | 2159 | ` */` |
|      2 | 2160 | `static int PH7_vfs_get_current_user(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2161 |  |
|      - | 2162 | `	ph7_vfs *pVfs;` |
|      - | 2163 | `	/* Point to the underlying vfs */` |
|      3 | 2164 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2165 | `	if( pVfs == 0 \|\| pVfs->xUsername == 0 ){` |
|    ! 0 | 2166 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2167 | `		SXUNUSED(apArg);` |
|      - | 2168 | `		/* IO routine not implemented */` |
|    ! 0 | 2169 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2170 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2171 | `			ph7_function_name(pCtx)` |
|      - | 2172 | `			);` |
|      - | 2173 | `		/* Set a dummy username */` |
|    ! 0 | 2174 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 2175 | `		return PH7_OK;` |
|      - | 2176 | `	}` |
|      - | 2177 | `	/* Perform the requested operation */` |
|      3 | 2178 | `	pVfs->xUsername(pCtx);` |
|      3 | 2179 | `	return PH7_OK;` |
|      2 | 2180 |  |
|      - | 2181 | `/*` |
|      - | 2182 | ` * int64 getmypid()` |
|      - | 2183 | ` *  Gets process ID.` |
|      - | 2184 | ` * Parameters` |
|      - | 2185 | ` *  None` |
|      - | 2186 | ` * Return` |
|      - | 2187 | ` *  Returns the process ID.` |
|      - | 2188 | ` */` |
|      4 | 2189 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2190 |  |
|      - | 2191 | `	ph7_int64 nProcessId;` |
|      - | 2192 | `	ph7_vfs *pVfs;` |
|      - | 2193 | `	/* Point to the underlying vfs */` |
|      5 | 2194 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 2195 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
|    ! 0 | 2196 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2197 | `		SXUNUSED(apArg);` |
|      - | 2198 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2199 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2200 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2201 | `			ph7_function_name(pCtx)` |
|      - | 2202 | `			);` |
|    ! 0 | 2203 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2204 | `		return PH7_OK;` |
|      - | 2205 | `	}` |
|      - | 2206 | `	/* Perform the requested operation */` |
|      5 | 2207 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 2208 | `	/* Set the result */` |
|      5 | 2209 | `	ph7_result_int64(pCtx,nProcessId);` |
|      5 | 2210 | `	return PH7_OK;` |
|      3 | 2211 |  |
|      - | 2212 | `/*` |
|      - | 2213 | ` * int getmyuid()` |
|      - | 2214 | ` *  Get user ID.` |
|      - | 2215 | ` * Parameters` |
|      - | 2216 | ` *  None` |
|      - | 2217 | ` * Return` |
|      - | 2218 | ` *  Returns the user ID.` |
|      - | 2219 | ` */` |
|      2 | 2220 | `static int PH7_vfs_getmyuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2221 |  |
|      - | 2222 | `	ph7_vfs *pVfs;` |
|      - | 2223 | `	int nUid;` |
|      - | 2224 | `	/* Point to the underlying vfs */` |
|      2 | 2225 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 2226 | `	if( pVfs == 0 \|\| pVfs->xUid == 0 ){` |
|    ! 0 | 2227 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2228 | `		SXUNUSED(apArg);` |
|      - | 2229 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2230 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2231 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2232 | `			ph7_function_name(pCtx)` |
|      - | 2233 | `			);` |
|    ! 0 | 2234 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2235 | `		return PH7_OK;` |
|      - | 2236 | `	}` |
|      - | 2237 | `	/* Perform the requested operation */` |
|      2 | 2238 | `	nUid = pVfs->xUid();` |
|      - | 2239 | `	/* Set the result */` |
|      2 | 2240 | `	ph7_result_int(pCtx,nUid);` |
|      2 | 2241 | `	return PH7_OK;` |
|      1 | 2242 |  |
|      - | 2243 | `/*` |
|      - | 2244 | ` * int getmygid()` |
|      - | 2245 | ` *  Get group ID.` |
|      - | 2246 | ` * Parameters` |
|      - | 2247 | ` *  None` |
|      - | 2248 | ` * Return` |
|      - | 2249 | ` *  Returns the group ID.` |
|      - | 2250 | ` */` |
|      2 | 2251 | `static int PH7_vfs_getmygid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2252 |  |
|      - | 2253 | `	ph7_vfs *pVfs;` |
|      - | 2254 | `	int nGid;` |
|      - | 2255 | `	/* Point to the underlying vfs */` |
|      2 | 2256 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 2257 | `	if( pVfs == 0 \|\| pVfs->xGid == 0 ){` |
|    ! 0 | 2258 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2259 | `		SXUNUSED(apArg);` |
|      - | 2260 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2261 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2262 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2263 | `			ph7_function_name(pCtx)` |
|      - | 2264 | `			);` |
|    ! 0 | 2265 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2266 | `		return PH7_OK;` |
|      - | 2267 | `	}` |
|      - | 2268 | `	/* Perform the requested operation */` |
|      2 | 2269 | `	nGid = pVfs->xGid();` |
|      - | 2270 | `	/* Set the result */` |
|      2 | 2271 | `	ph7_result_int(pCtx,nGid);` |
|      2 | 2272 | `	return PH7_OK;` |
|      1 | 2273 |  |
|      - | 2274 | `#ifdef __WINNT__` |
|      - | 2275 | `#include <Windows.h>` |
|      - | 2276 | `#elif defined(__UNIXES__)` |
|      - | 2277 | `#include <sys/utsname.h>` |
|      - | 2278 | `#endif` |
|      - | 2279 | `/*` |
|      - | 2280 | ` * string php_uname([ string $mode = "a" ])` |
|      - | 2281 | ` *  Returns information about the host operating system.` |
|      - | 2282 | ` * Parameters` |
|      - | 2283 | ` *  $mode` |
|      - | 2284 | ` *   mode is a single character that defines what information is returned:` |
|      - | 2285 | ` *    'a': This is the default. Contains all modes in the sequence "s n r v m".` |
|      - | 2286 | ` *    's': Operating system name. eg. FreeBSD.` |
|      - | 2287 | ` *    'n': Host name. eg. localhost.example.com.` |
|      - | 2288 | ` *    'r': Release name. eg. 5.1.2-RELEASE.` |
|      - | 2289 | ` *    'v': Version information. Varies a lot between operating systems.` |
|      - | 2290 | ` *    'm': Machine type. eg. i386.` |
|      - | 2291 | ` * Return` |
|      - | 2292 | ` *  OS description as a string.` |
|      - | 2293 | ` */` |
|      4 | 2294 | `static int PH7_vfs_ph7_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2295 |  |
|      - | 2296 | `#if defined(__WINNT__)` |
|      1 | 2297 | `	const char *zName = "Microsoft Windows";` |
|      - | 2298 | `	OSVERSIONINFOW sVer;` |
|      - | 2299 | `#elif defined(__UNIXES__)` |
|      - | 2300 | `	struct utsname sName;` |
|      - | 2301 | `#endif` |
|      5 | 2302 | `	const char *zMode = "a";` |
|      5 | 2303 | `	if( nArg > 0 && ph7_value_is_string(apArg[0]) ){` |
|      - | 2304 | `		/* Extract the desired mode */` |
|    ! 0 | 2305 | `		zMode = ph7_value_to_string(apArg[0],0);` |
|    ! 0 | 2306 | `	}` |
|      - | 2307 | `#if defined(__WINNT__)` |
|      1 | 2308 | `	sVer.dwOSVersionInfoSize = sizeof(sVer);` |
|      - | 2309 | `	/* GetVersionExW is deprecated in modern MSVC. Suppress deprecation for this call. */` |
|      - | 2310 | `#if defined(_MSC_VER)` |
|      - | 2311 | `#pragma warning(push)` |
|      - | 2312 | `#pragma warning(disable:4996)` |
|      - | 2313 | `#endif` |
|      1 | 2314 | `	if( TRUE != GetVersionExW(&sVer)){` |
|      - | 2315 | `#if defined(_MSC_VER)` |
|      - | 2316 | `#pragma warning(pop)` |
|      - | 2317 | `#endif` |
|    ! 0 | 2318 | `		ph7_result_string(pCtx,zName,-1);` |
|    ! 0 | 2319 | `		return PH7_OK;` |
|      - | 2320 | `	}` |
|      1 | 2321 | `	if( sVer.dwPlatformId == VER_PLATFORM_WIN32_NT ){` |
|      1 | 2322 | `		if( sVer.dwMajorVersion <= 4 ){` |
|    ! 0 | 2323 | `			zName = "Microsoft Windows NT";` |
|      1 | 2324 | `		}else if( sVer.dwMajorVersion == 5 ){` |
|    ! 0 | 2325 | `			switch(sVer.dwMinorVersion){` |
|    ! 0 | 2326 | `				case 0:	zName = "Microsoft Windows 2000"; break;` |
|    ! 0 | 2327 | `				case 1: zName = "Microsoft Windows XP";   break;` |
|    ! 0 | 2328 | `				case 2: zName = "Microsoft Windows Server 2003"; break;` |
|      - | 2329 | `			}` |
|    ! 0 | 2330 | `		}else if( sVer.dwMajorVersion == 6){` |
|      1 | 2331 | `				switch(sVer.dwMinorVersion){` |
|    ! 0 | 2332 | `					case 0: zName = "Microsoft Windows Vista"; break;` |
|    ! 0 | 2333 | `					case 1: zName = "Microsoft Windows 7"; break;` |
|      1 | 2334 | `					case 2: zName = "Microsoft Windows Server 2008"; break;` |
|    ! 0 | 2335 | `					case 3: zName = "Microsoft Windows 8"; break;` |
|      - | 2336 | `					default: break;` |
|      - | 2337 | `				}` |
|      - | 2338 | `		}` |
|      - | 2339 | `	}` |
|      1 | 2340 | `	switch(zMode[0]){` |
|      - | 2341 | `	case 's':` |
|      - | 2342 | `		/* Operating system name */` |
|    ! 0 | 2343 | `		ph7_result_string(pCtx,zName,-1/* Compute length automatically*/);` |
|    ! 0 | 2344 | `		break;` |
|      - | 2345 | `	case 'n':` |
|      - | 2346 | `		/* Host name */` |
|    ! 0 | 2347 | `		ph7_result_string(pCtx,"localhost",(int)sizeof("localhost")-1);` |
|    ! 0 | 2348 | `		break;` |
|      - | 2349 | `	case 'r':` |
|      - | 2350 | `	case 'v':` |
|      - | 2351 | `		/* Version information. */` |
|    ! 0 | 2352 | `		ph7_result_string_format(pCtx,"%u.%u build %u",` |
|      - | 2353 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2354 | `			);` |
|    ! 0 | 2355 | `		break;` |
|      - | 2356 | `	case 'm':` |
|      - | 2357 | `		/* Machine name */` |
|    ! 0 | 2358 | `		ph7_result_string(pCtx,"x86",(int)sizeof("x86")-1);` |
|    ! 0 | 2359 | `		break;` |
|      - | 2360 | `	default:` |
|      1 | 2361 | `		ph7_result_string_format(pCtx,"%s localhost %u.%u build %u x86",` |
|      - | 2362 | `			zName,` |
|      - | 2363 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2364 | `			);` |
|      - | 2365 | `		break;` |
|      - | 2366 | `	}` |
|      - | 2367 | `#elif defined(__UNIXES__)` |
|      4 | 2368 | `	if( uname(&sName) != 0 ){` |
|    ! 0 | 2369 | `		ph7_result_string(pCtx,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 | 2370 | `		return PH7_OK;` |
|      - | 2371 | `	}` |
|      4 | 2372 | `	switch(zMode[0]){` |
|    ! 0 | 2373 | `	case 's':` |
|      - | 2374 | `		/* Operating system name */` |
|    ! 0 | 2375 | `		ph7_result_string(pCtx,sName.sysname,-1/* Compute length automatically*/);` |
|    ! 0 | 2376 | `		break;` |
|    ! 0 | 2377 | `	case 'n':` |
|      - | 2378 | `		/* Host name */` |
|    ! 0 | 2379 | `		ph7_result_string(pCtx,sName.nodename,-1/* Compute length automatically*/);` |
|    ! 0 | 2380 | `		break;` |
|    ! 0 | 2381 | `	case 'r':` |
|      - | 2382 | `		/* Release information */` |
|    ! 0 | 2383 | `		ph7_result_string(pCtx,sName.release,-1/* Compute length automatically*/);` |
|    ! 0 | 2384 | `		break;` |
|    ! 0 | 2385 | `	case 'v':` |
|      - | 2386 | `		/* Version information. */` |
|    ! 0 | 2387 | `		ph7_result_string(pCtx,sName.version,-1/* Compute length automatically*/);` |
|    ! 0 | 2388 | `		break;` |
|    ! 0 | 2389 | `	case 'm':` |
|      - | 2390 | `		/* Machine name */` |
|    ! 0 | 2391 | `		ph7_result_string(pCtx,sName.machine,-1/* Compute length automatically*/);` |
|    ! 0 | 2392 | `		break;` |
|      2 | 2393 | `	default:` |
|      6 | 2394 | `		ph7_result_string_format(pCtx,` |
|      - | 2395 | `			"%s %s %s %s %s",` |
|      2 | 2396 | `			sName.sysname,` |
|      2 | 2397 | `			sName.release,` |
|      2 | 2398 | `			sName.version,` |
|      2 | 2399 | `			sName.nodename,` |
|      2 | 2400 | `			sName.machine` |
|      - | 2401 | `			);` |
|      4 | 2402 | `		break;` |
|      - | 2403 | `	}` |
|      - | 2404 | `#else` |
|      - | 2405 | `	ph7_result_string(pCtx,"Unknown Operating System",(int)sizeof("Unknown Operating System")-1);` |
|      - | 2406 | `#endif` |
|      5 | 2407 | `	return PH7_OK;` |
|      3 | 2408 |  |
|      - | 2409 | `/*` |
|      - | 2410 | ` * Section:` |
|      - | 2411 | ` *    IO stream implementation.` |
|      - | 2412 | ` * Status:` |
|      - | 2413 | ` *    Stable.` |
|      - | 2414 | ` */` |
|      - | 2415 | `typedef struct io_private io_private;` |
|      - | 2416 | `struct io_private` |
|      - | 2417 |  |
|      - | 2418 | `	const ph7_io_stream *pStream; /* Underlying IO device */` |
|      - | 2419 | `	void *pHandle; /* IO handle */` |
|      - | 2420 | `	/* Unbuffered IO */` |
|      - | 2421 | `	SyBlob sBuffer; /* Working buffer */` |
|      - | 2422 | `	sxu32 nOfft;    /* Current read offset */` |
|      - | 2423 | `	sxu32 iMagic;   /* Sanity check to avoid misuse */` |
|      - | 2424 | `};` |
|      - | 2425 | `#define IO_PRIVATE_MAGIC 0xFEAC14` |
|      - | 2426 | `/* Make sure we are dealing with a valid io_private instance */` |
|      - | 2427 | `#define IO_PRIVATE_INVALID(IO) ( IO == 0 \|\| IO->iMagic != IO_PRIVATE_MAGIC )` |
|      - | 2428 | `/* Forward declaration */` |
|      - | 2429 | `static void ResetIOPrivate(io_private *pDev);` |
|      - | 2430 | `/*` |
|      - | 2431 | ` * bool ftruncate(resource $handle,int64 $size)` |
|      - | 2432 | ` *  Truncates a file to a given length.` |
|      - | 2433 | ` * Parameters` |
|      - | 2434 | ` *  $handle` |
|      - | 2435 | ` *   The file pointer.` |
|      - | 2436 | ` *   Note:` |
|      - | 2437 | ` *    The handle must be open for writing.` |
|      - | 2438 | ` * $size` |
|      - | 2439 | ` *   The size to truncate to.` |
|      - | 2440 | ` * Return` |
|      - | 2441 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2442 | ` */` |
|      6 | 2443 | `static int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2444 |  |
|      - | 2445 | `	const ph7_io_stream *pStream;` |
|      - | 2446 | `	io_private *pDev;` |
|      - | 2447 | `	int rc;` |
|      7 | 2448 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2449 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2450 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2451 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2452 | `		return PH7_OK;` |
|      - | 2453 | `	}` |
|      - | 2454 | `	/* Extract our private data */` |
|      7 | 2455 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2456 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 2457 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2458 | `		/*Expecting an IO handle */` |
|    ! 0 | 2459 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2460 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2461 | `		return PH7_OK;` |
|      - | 2462 | `	}` |
|      - | 2463 | `	/* Point to the target IO stream device */` |
|      7 | 2464 | `	pStream = pDev->pStream;` |
|      7 | 2465 | `	if( pStream == 0  \|\| pStream->xTrunc == 0){` |
|    ! 0 | 2466 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2467 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2468 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2469 | `			);` |
|    ! 0 | 2470 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2471 | `		return PH7_OK;` |
|      - | 2472 | `	}` |
|      - | 2473 | `	/* Perform the requested operation */` |
|      7 | 2474 | `	rc = pStream->xTrunc(pDev->pHandle,ph7_value_to_int64(apArg[1]));` |
|      7 | 2475 | `	if( rc == PH7_OK ){` |
|      - | 2476 | `		/* Discard buffered data */` |
|      7 | 2477 | `		ResetIOPrivate(pDev);` |
|      3 | 2478 | `	}` |
|      - | 2479 | `	/* IO result */` |
|      7 | 2480 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      7 | 2481 | `	return PH7_OK;` |
|      4 | 2482 |  |
|      - | 2483 | `/*` |
|      - | 2484 | ` * int fseek(resource $handle,int $offset[,int $whence = SEEK_SET ])` |
|      - | 2485 | ` *  Seeks on a file pointer.` |
|      - | 2486 | ` * Parameters` |
|      - | 2487 | ` *  $handle` |
|      - | 2488 | ` *   A file system pointer resource that is typically created using fopen().` |
|      - | 2489 | ` * $offset` |
|      - | 2490 | ` *   The offset.` |
|      - | 2491 | ` *   To move to a position before the end-of-file, you need to pass a negative` |
|      - | 2492 | ` *   value in offset and set whence to SEEK_END.` |
|      - | 2493 | ` *   whence` |
|      - | 2494 | ` *   whence values are:` |
|      - | 2495 | ` *    SEEK_SET - Set position equal to offset bytes.` |
|      - | 2496 | ` *    SEEK_CUR - Set position to current location plus offset.` |
|      - | 2497 | ` *    SEEK_END - Set position to end-of-file plus offset.` |
|      - | 2498 | ` * Return` |
|      - | 2499 | ` *  0 on success,-1 on failure` |
|      - | 2500 | ` */` |
|      2 | 2501 | `static int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2502 |  |
|      - | 2503 | `	const ph7_io_stream *pStream;` |
|      - | 2504 | `	io_private *pDev;` |
|      - | 2505 | `	ph7_int64 iOfft;` |
|      - | 2506 | `	int whence;` |
|      - | 2507 | `	int rc;` |
|      3 | 2508 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2509 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2510 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2511 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2512 | `		return PH7_OK;` |
|      - | 2513 | `	}` |
|      - | 2514 | `	/* Extract our private data */` |
|      3 | 2515 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2516 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 2517 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2518 | `		/*Expecting an IO handle */` |
|    ! 0 | 2519 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2520 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2521 | `		return PH7_OK;` |
|      - | 2522 | `	}` |
|      - | 2523 | `	/* Point to the target IO stream device */` |
|      3 | 2524 | `	pStream = pDev->pStream;` |
|      3 | 2525 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2526 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2527 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2528 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2529 | `			);` |
|    ! 0 | 2530 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2531 | `		return PH7_OK;` |
|      - | 2532 | `	}` |
|      - | 2533 | `	/* Extract the offset */` |
|      3 | 2534 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|      3 | 2535 | `	whence = 0;/* SEEK_SET */` |
|      3 | 2536 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|    ! 0 | 2537 | `		whence = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2538 | `	}` |
|      - | 2539 | `	/* Perform the requested operation */` |
|      3 | 2540 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|      3 | 2541 | `	if( rc == PH7_OK ){` |
|      - | 2542 | `		/* Ignore buffered data */` |
|      3 | 2543 | `		ResetIOPrivate(pDev);` |
|      1 | 2544 | `	}` |
|      - | 2545 | `	/* IO result */` |
|      3 | 2546 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|      3 | 2547 | `	return PH7_OK;` |
|      2 | 2548 |  |
|      - | 2549 | `/*` |
|      - | 2550 | ` * int64 ftell(resource $handle)` |
|      - | 2551 | ` *  Returns the current position of the file read/write pointer.` |
|      - | 2552 | ` * Parameters` |
|      - | 2553 | ` *  $handle` |
|      - | 2554 | ` *   The file pointer.` |
|      - | 2555 | ` * Return` |
|      - | 2556 | ` *  Returns the position of the file pointer referenced by handle` |
|      - | 2557 | ` *  as an integer; i.e., its offset into the file stream.` |
|      - | 2558 | ` *  FALSE is returned on failure.` |
|      - | 2559 | ` */` |
|      6 | 2560 | `static int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2561 |  |
|      - | 2562 | `	const ph7_io_stream *pStream;` |
|      - | 2563 | `	io_private *pDev;` |
|      - | 2564 | `	ph7_int64 iOfft;` |
|      7 | 2565 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2566 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2567 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2568 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2569 | `		return PH7_OK;` |
|      - | 2570 | `	}` |
|      - | 2571 | `	/* Extract our private data */` |
|      7 | 2572 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2573 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 2574 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2575 | `		/*Expecting an IO handle */` |
|    ! 0 | 2576 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2577 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2578 | `		return PH7_OK;` |
|      - | 2579 | `	}` |
|      - | 2580 | `	/* Point to the target IO stream device */` |
|      7 | 2581 | `	pStream = pDev->pStream;` |
|      7 | 2582 | `	if( pStream == 0  \|\| pStream->xTell == 0){` |
|    ! 0 | 2583 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2584 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2585 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2586 | `			);` |
|    ! 0 | 2587 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2588 | `		return PH7_OK;` |
|      - | 2589 | `	}` |
|      - | 2590 | `	/* Perform the requested operation */` |
|      7 | 2591 | `	iOfft = pStream->xTell(pDev->pHandle);` |
|      - | 2592 | `	/* IO result */` |
|      7 | 2593 | `	ph7_result_int64(pCtx,iOfft);` |
|      7 | 2594 | `	return PH7_OK;` |
|      4 | 2595 |  |
|      - | 2596 | `/*` |
|      - | 2597 | ` * bool rewind(resource $handle)` |
|      - | 2598 | ` *  Rewind the position of a file pointer.` |
|      - | 2599 | ` * Parameters` |
|      - | 2600 | ` *  $handle` |
|      - | 2601 | ` *   The file pointer.` |
|      - | 2602 | ` * Return` |
|      - | 2603 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2604 | ` */` |
|      4 | 2605 | `static int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2606 |  |
|      - | 2607 | `	const ph7_io_stream *pStream;` |
|      - | 2608 | `	io_private *pDev;` |
|      - | 2609 | `	int rc;` |
|      5 | 2610 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2611 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2612 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2613 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2614 | `		return PH7_OK;` |
|      - | 2615 | `	}` |
|      - | 2616 | `	/* Extract our private data */` |
|      5 | 2617 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2618 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 2619 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2620 | `		/*Expecting an IO handle */` |
|    ! 0 | 2621 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2622 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2623 | `		return PH7_OK;` |
|      - | 2624 | `	}` |
|      - | 2625 | `	/* Point to the target IO stream device */` |
|      5 | 2626 | `	pStream = pDev->pStream;` |
|      5 | 2627 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2628 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2629 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2630 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2631 | `			);` |
|    ! 0 | 2632 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2633 | `		return PH7_OK;` |
|      - | 2634 | `	}` |
|      - | 2635 | `	/* Perform the requested operation */` |
|      5 | 2636 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|      5 | 2637 | `	if( rc == PH7_OK ){` |
|      - | 2638 | `		/* Ignore buffered data */` |
|      5 | 2639 | `		ResetIOPrivate(pDev);` |
|      2 | 2640 | `	}` |
|      - | 2641 | `	/* IO result */` |
|      5 | 2642 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 2643 | `	return PH7_OK;` |
|      3 | 2644 |  |
|      - | 2645 | `/*` |
|      - | 2646 | ` * bool fflush(resource $handle)` |
|      - | 2647 | ` *  Flushes the output to a file.` |
|      - | 2648 | ` * Parameters` |
|      - | 2649 | ` *  $handle` |
|      - | 2650 | ` *   The file pointer.` |
|      - | 2651 | ` * Return` |
|      - | 2652 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2653 | ` */` |
|      2 | 2654 | `static int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2655 |  |
|      - | 2656 | `	const ph7_io_stream *pStream;` |
|      - | 2657 | `	io_private *pDev;` |
|      - | 2658 | `	int rc;` |
|      3 | 2659 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2660 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2661 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2662 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2663 | `		return PH7_OK;` |
|      - | 2664 | `	}` |
|      - | 2665 | `	/* Extract our private data */` |
|      3 | 2666 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2667 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 2668 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2669 | `		/*Expecting an IO handle */` |
|    ! 0 | 2670 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2671 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2672 | `		return PH7_OK;` |
|      - | 2673 | `	}` |
|      - | 2674 | `	/* Point to the target IO stream device */` |
|      3 | 2675 | `	pStream = pDev->pStream;` |
|      3 | 2676 | `	if( pStream == 0 \|\| pStream->xSync == 0){` |
|    ! 0 | 2677 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2678 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2679 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2680 | `			);` |
|    ! 0 | 2681 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2682 | `		return PH7_OK;` |
|      - | 2683 | `	}` |
|      - | 2684 | `	/* Perform the requested operation */` |
|      3 | 2685 | `	rc = pStream->xSync(pDev->pHandle);` |
|      - | 2686 | `	/* IO result */` |
|      3 | 2687 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 2688 | `	return PH7_OK;` |
|      2 | 2689 |  |
|      - | 2690 | `/*` |
|      - | 2691 | ` * bool feof(resource $handle)` |
|      - | 2692 | ` *  Tests for end-of-file on a file pointer.` |
|      - | 2693 | ` * Parameters` |
|      - | 2694 | ` *  $handle` |
|      - | 2695 | ` *   The file pointer.` |
|      - | 2696 | ` * Return` |
|      - | 2697 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|      - | 2698 | ` */` |
|   4784 | 2699 | `static int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2700 |  |
|      - | 2701 | `	const ph7_io_stream *pStream;` |
|      - | 2702 | `	io_private *pDev;` |
|      - | 2703 | `	int rc;` |
|   4786 | 2704 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2705 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 2706 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2707 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2708 | `		return PH7_OK;` |
|      - | 2709 | `	}` |
|      - | 2710 | `	/* Extract our private data */` |
|   4786 | 2711 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2712 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   4786 | 2713 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2714 | `		/*Expecting an IO handle */` |
|    ! 0 | 2715 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2716 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2717 | `		return PH7_OK;` |
|      - | 2718 | `	}` |
|      - | 2719 | `	/* Point to the target IO stream device */` |
|   4786 | 2720 | `	pStream = pDev->pStream;` |
|   4786 | 2721 | `	if( pStream == 0 ){` |
|    ! 0 | 2722 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2723 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2724 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2725 | `			);` |
|    ! 0 | 2726 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2727 | `		return PH7_OK;` |
|      - | 2728 | `	}` |
|   4786 | 2729 | `	rc = SXERR_EOF;` |
|      - | 2730 | `	/* Perform the requested operation */` |
|   4786 | 2731 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2732 | `		/* Data is available */` |
|   2090 | 2733 | `		rc = PH7_OK;` |
|   1046 | 2734 | `	}else{` |
|      - | 2735 | `		char zBuf[4096];` |
|      - | 2736 | `		ph7_int64 n;` |
|      - | 2737 | `		/* Perform a buffered read */` |
|   2698 | 2738 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|   2698 | 2739 | `		if( n > 0 ){` |
|      - | 2740 | `			/* Copy buffered data */` |
|    782 | 2741 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|    782 | 2742 | `			rc = PH7_OK;` |
|    390 | 2743 | `		}` |
|      - | 2744 | `	}` |
|      - | 2745 | `	/* EOF or not */` |
|   4786 | 2746 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|   4786 | 2747 | `	return PH7_OK;` |
|   2394 | 2748 |  |
|      - | 2749 | `/*` |
|      - | 2750 | ` * Read n bytes from the underlying IO stream device.` |
|      - | 2751 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|      - | 2752 | ` * [i.e: IO error ] or EOF.` |
|      - | 2753 | ` */` |
|     18 | 2754 | `static ph7_int64 StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      2 | 2755 |  |
|     20 | 2756 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|     20 | 2757 | `	char *zBuf = (char *)pBuf;` |
|      - | 2758 | `	ph7_int64 n,nRead;` |
|     20 | 2759 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     20 | 2760 | `	if( n > 0 ){` |
|    ! 0 | 2761 | `		if( n > nLen ){` |
|    ! 0 | 2762 | `			n = nLen;` |
|    ! 0 | 2763 | `		}` |
|      - | 2764 | `		/* Copy the buffered data */` |
|    ! 0 | 2765 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|      - | 2766 | `		/* Update the read offset */` |
|    ! 0 | 2767 | `		pDev->nOfft += (sxu32)n;` |
|    ! 0 | 2768 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2769 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|    ! 0 | 2770 | `			SyBlobReset(&pDev->sBuffer);` |
|    ! 0 | 2771 | `			pDev->nOfft = 0;` |
|    ! 0 | 2772 | `		}` |
|    ! 0 | 2773 | `		nLen -= n;` |
|    ! 0 | 2774 | `		if( nLen < 1 ){` |
|      - | 2775 | `			/* All done */` |
|    ! 0 | 2776 | `			return n;` |
|      - | 2777 | `		}` |
|      - | 2778 | `		/* Advance the cursor */` |
|    ! 0 | 2779 | `		zBuf += n;` |
|    ! 0 | 2780 | `	}` |
|      - | 2781 | `	/* Read without buffering */` |
|     20 | 2782 | `	nRead = pStream->xRead(pDev->pHandle,zBuf,nLen);` |
|     20 | 2783 | `	if( nRead > 0 ){` |
|     18 | 2784 | `		n += nRead;` |
|     11 | 2785 | `	}else if( n < 1 ){` |
|      - | 2786 | `		/* EOF or IO error */` |
|      3 | 2787 | `		return nRead;` |
|      - | 2788 | `	}` |
|     18 | 2789 | `	return n;` |
|     11 | 2790 |  |
|      - | 2791 | `/*` |
|      - | 2792 | ` * Extract a single line from the buffered input.` |
|      - | 2793 | ` */` |
|   2884 | 2794 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      2 | 2795 |  |
|      - | 2796 | `	const char *zIn,*zEnd,*zPtr;` |
|   2886 | 2797 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|   2886 | 2798 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|   2886 | 2799 | `	zPtr = zIn;` |
| 224351 | 2800 | `	while( zIn < zEnd ){` |
| 224321 | 2801 | `		if( zIn[0] == '\n' ){` |
|      - | 2802 | `			/* Line found */` |
|   2856 | 2803 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|   2856 | 2804 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|   2856 | 2805 | `			*pzLine = zPtr;` |
|   2856 | 2806 | `			return SXRET_OK;` |
|      - | 2807 | `		}` |
| 221467 | 2808 | `		zIn++;` |
|      2 | 2809 | `	}` |
|      - | 2810 | `	/* No line were found */` |
|     32 | 2811 | `	return SXERR_NOTFOUND;` |
|   1444 | 2812 |  |
|      - | 2813 | `/*` |
|      - | 2814 | ` * Read a single line from the underlying IO stream device.` |
|      - | 2815 | ` */` |
|   2888 | 2816 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      2 | 2817 |  |
|   2890 | 2818 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - | 2819 | `	char zBuf[8192];` |
|      - | 2820 | `	ph7_int64 n;` |
|      - | 2821 | `	sxi32 rc;` |
|   2890 | 2822 | `	n = 0;` |
|   2890 | 2823 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2824 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     18 | 2825 | `		SyBlobReset(&pDev->sBuffer);` |
|     18 | 2826 | `		pDev->nOfft = 0;` |
|      8 | 2827 | `	}` |
|   2890 | 2828 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2829 | `		/* Check if there is a line */` |
|   2874 | 2830 | `		rc = GetLine(pDev,&n,pzData);` |
|   2874 | 2831 | `		if( rc == SXRET_OK ){` |
|      - | 2832 | `			/* Got line,update the cursor  */` |
|   2846 | 2833 | `			pDev->nOfft += (sxu32)n;` |
|   2846 | 2834 | `			return n;` |
|      - | 2835 | `		}` |
|     14 | 2836 | `	}` |
|      - | 2837 | `	/* Perform the read operation until a new line is extracted or length` |
|      - | 2838 | `	 * limit is reached.` |
|      - | 2839 | `	 */` |
|     23 | 2840 | `	for(;;){` |
|     48 | 2841 | `		n = pStream->xRead(pDev->pHandle,zBuf, (nMaxLen > 0 && nMaxLen < (ph7_int64)sizeof(zBuf)) ? nMaxLen : (ph7_int64)sizeof(zBuf));` |
|     48 | 2842 | `		if( n < 1 ){` |
|      - | 2843 | `			/* EOF or IO error */` |
|     36 | 2844 | `			break;` |
|      - | 2845 | `		}` |
|      - | 2846 | `		/* Append the data just read */` |
|     14 | 2847 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - | 2848 | `		/* Try to extract a line */` |
|     14 | 2849 | `		rc = GetLine(pDev,&n,pzData);` |
|     14 | 2850 | `		if( rc == SXRET_OK ){` |
|      - | 2851 | `			/* Got one,return immediately */` |
|     12 | 2852 | `			pDev->nOfft += (sxu32)n;` |
|     12 | 2853 | `			return n;` |
|      - | 2854 | `		}` |
|      3 | 2855 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
|      - | 2856 | `			/* Read limit reached,return the available data */` |
|    ! 0 | 2857 | `			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    ! 0 | 2858 | `			n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 2859 | `			/* Reset the working buffer */` |
|    ! 0 | 2860 | `			SyBlobReset(&pDev->sBuffer);` |
|    ! 0 | 2861 | `			pDev->nOfft = 0;` |
|    ! 0 | 2862 | `			return n;` |
|      - | 2863 | `		}` |
|      1 | 2864 | `	}` |
|     36 | 2865 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2866 | `		/* Read limit reached,return the available data */` |
|     32 | 2867 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|     32 | 2868 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 2869 | `		/* Reset the working buffer */` |
|     32 | 2870 | `		SyBlobReset(&pDev->sBuffer);` |
|     32 | 2871 | `		pDev->nOfft = 0;` |
|     15 | 2872 | `	}` |
|     36 | 2873 | `	return n;` |
|   1446 | 2874 |  |
|      - | 2875 | `/*` |
|      - | 2876 | ` * Open an IO stream handle.` |
|      - | 2877 | ` * Notes on stream:` |
|      - | 2878 | ` * According to the PHP reference manual.` |
|      - | 2879 | ` * In its simplest definition, a stream is a resource object which exhibits streamable behavior.` |
|      - | 2880 | ` * That is, it can be read from or written to in a linear fashion, and may be able to fseek()` |
|      - | 2881 | ` * to an arbitrary locations within the stream.` |
|      - | 2882 | ` * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.` |
|      - | 2883 | ` * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file` |
|      - | 2884 | ` * on a remote server.` |
|      - | 2885 | ` * A stream is referenced as: scheme://target` |
|      - | 2886 | ` *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...` |
|      - | 2887 | ` *   If no wrapper is specified, the function default is used (typically file://).` |
|      - | 2888 | ` *   target - Depends on the wrapper used. For filesystem related streams this is typically a path` |
|      - | 2889 | ` *  and filename of the desired file. For network related streams this is typically a hostname, often` |
|      - | 2890 | ` *  with a path appended.` |
|      - | 2891 | ` *` |
|      - | 2892 | ` * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.` |
|      - | 2893 | ` * Please refer to the official documentation for a full discussion.` |
|      - | 2894 | ` * This function return a handle on success. Otherwise null.` |
|      - | 2895 | ` */` |
|  21064 | 2896 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - | 2897 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      2 | 2898 |  |
|  21066 | 2899 | `	void *pHandle = 0; /* cc warning */` |
|      - | 2900 | `	SyString sFile;` |
|      - | 2901 | `	int rc;` |
|  21066 | 2902 | `	if( pStream == 0 ){` |
|      - | 2903 | `		/* No such stream device */` |
|    ! 0 | 2904 | `		return 0;` |
|      - | 2905 | `	}` |
|  21066 | 2906 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  21066 | 2907 | `	if( use_include ){` |
|   7474 | 2908 | `		if(	sFile.zString[0] == '/' \|\|` |
|      - | 2909 | `#ifdef __WINNT__` |
|      - | 2910 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - | 2911 | `#endif` |
|   7464 | 2912 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   7462 | 2913 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - | 2914 | `				/*  Open the file directly */` |
|     13 | 2915 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      7 | 2916 | `		}else{` |
|      - | 2917 | `			SyString *pPath;` |
|      - | 2918 | `			SyBlob sWorker;` |
|      - | 2919 | `#ifdef __WINNT__` |
|      - | 2920 | `			static const int c = '\\';` |
|      - | 2921 | `#else` |
|      - | 2922 | `			static const int c = '/';` |
|      - | 2923 | `#endif` |
|      - | 2924 | `			/* Init the path builder working buffer */` |
|   7464 | 2925 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - | 2926 | `			/* Build a path from the set of include path */` |
|   7464 | 2927 | `			SySetResetCursor(&pVm->aPaths);` |
|   7464 | 2928 | `			rc = SXERR_IO;` |
|   7466 | 2929 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - | 2930 | `				/* Build full path */` |
|   7464 | 2931 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - | 2932 | `				/* Append null terminator */` |
|   7464 | 2933 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 2934 | `					continue;` |
|      - | 2935 | `				}` |
|      - | 2936 | `				/* Try to open the file */` |
|   7464 | 2937 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   7464 | 2938 | `				if( rc == PH7_OK ){` |
|   7461 | 2939 | `					if( bPushInclude ){` |
|      - | 2940 | `						/* Mark as included */` |
|   7461 | 2941 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   3730 | 2942 | `					}` |
|   7461 | 2943 | `					break;` |
|      - | 2944 | `				}` |
|      - | 2945 | `				/* Reset the working buffer */` |
|      3 | 2946 | `				SyBlobReset(&sWorker);` |
|      - | 2947 | `				/* Check the next path */` |
|      1 | 2948 | `			}` |
|   7464 | 2949 | `			SyBlobRelease(&sWorker);` |
|      - | 2950 | `		}` |
|   7476 | 2951 | `		if( rc == PH7_OK ){` |
|   7473 | 2952 | `			if( bPushInclude ){` |
|      - | 2953 | `				/* Mark as included */` |
|   7473 | 2954 | `				PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|   3736 | 2955 | `			}` |
|   3736 | 2956 | `		}` |
|   3739 | 2957 | `	}else{` |
|      - | 2958 | `		/* Open the URI direcly */` |
|  13592 | 2959 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - | 2960 | `	}` |
|  21066 | 2961 | `	if( rc != PH7_OK ){` |
|      - | 2962 | `		/* IO error */` |
|      9 | 2963 | `		return 0;` |
|      - | 2964 | `	}` |
|      - | 2965 | `	/* Return the file handle */` |
|  21058 | 2966 | `	return pHandle;` |
|  10534 | 2967 |  |
|      - | 2968 | `/*` |
|      - | 2969 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - | 2970 | ` * Store the read data in the given BLOB (last argument).` |
|      - | 2971 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - | 2972 | ` */` |
|   7470 | 2973 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      1 | 2974 |  |
|      - | 2975 | `	ph7_int64 nRead;` |
|      - | 2976 | `	char zBuf[8192]; /* 8K */` |
|      - | 2977 | `	int rc;` |
|      - | 2978 | `	/* Perform the requested operation */` |
|   7470 | 2979 | `	for(;;){` |
|  14941 | 2980 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  14941 | 2981 | `		if( nRead < 1 ){` |
|      - | 2982 | `			/* EOF or IO error */` |
|   7471 | 2983 | `			break;` |
|      - | 2984 | `		}` |
|      - | 2985 | `		/* Append contents */` |
|   7471 | 2986 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   7471 | 2987 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 2988 | `			break;` |
|      - | 2989 | `		}` |
|      1 | 2990 | `	}` |
|   7471 | 2991 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      1 | 2992 |  |
|      - | 2993 | `/*` |
|      - | 2994 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - | 2995 | ` */` |
|  21070 | 2996 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      2 | 2997 |  |
|  21072 | 2998 | `	if( pStream->xClose ){` |
|  21072 | 2999 | `		pStream->xClose(pHandle);` |
|  10535 | 3000 | `	}` |
|  21072 | 3001 |  |
|      - | 3002 | `/*` |
|      - | 3003 | ` * string fgetc(resource $handle)` |
|      - | 3004 | ` *  Gets a character from the given file pointer.` |
|      - | 3005 | ` * Parameters` |
|      - | 3006 | ` *  $handle` |
|      - | 3007 | ` *   The file pointer.` |
|      - | 3008 | ` * Return` |
|      - | 3009 | ` *  Returns a string containing a single character read from the file` |
|      - | 3010 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|      - | 3011 | ` * WARNING` |
|      - | 3012 | ` *  This operation is extremely slow.Avoid using it.` |
|      - | 3013 | ` */` |
|      4 | 3014 | `static int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3015 |  |
|      - | 3016 | `	const ph7_io_stream *pStream;` |
|      - | 3017 | `	io_private *pDev;` |
|      - | 3018 | `	int c,n;` |
|      5 | 3019 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3020 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3021 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3022 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3023 | `		return PH7_OK;` |
|      - | 3024 | `	}` |
|      - | 3025 | `	/* Extract our private data */` |
|      5 | 3026 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3027 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 3028 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3029 | `		/*Expecting an IO handle */` |
|    ! 0 | 3030 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3031 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3032 | `		return PH7_OK;` |
|      - | 3033 | `	}` |
|      - | 3034 | `	/* Point to the target IO stream device */` |
|      5 | 3035 | `	pStream = pDev->pStream;` |
|      5 | 3036 | `	if( pStream == 0  ){` |
|    ! 0 | 3037 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3038 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3039 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3040 | `			);` |
|    ! 0 | 3041 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3042 | `		return PH7_OK;` |
|      - | 3043 | `	}` |
|      - | 3044 | `	/* Perform the requested operation */` |
|      5 | 3045 | `	n = (int)StreamRead(pDev,(void *)&c,sizeof(char));` |
|      - | 3046 | `	/* IO result */` |
|      5 | 3047 | `	if( n < 1 ){` |
|      - | 3048 | `		/* EOF or error,return FALSE */` |
|    ! 0 | 3049 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3050 | `	}else{` |
|      - | 3051 | `		/* Return the string holding the character */` |
|      5 | 3052 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|      - | 3053 | `	}` |
|      5 | 3054 | `	return PH7_OK;` |
|      3 | 3055 |  |
|      - | 3056 | `/*` |
|      - | 3057 | ` * string fgets(resource $handle[,int64 $length ])` |
|      - | 3058 | ` *  Gets line from file pointer.` |
|      - | 3059 | ` * Parameters` |
|      - | 3060 | ` *  $handle` |
|      - | 3061 | ` *   The file pointer.` |
|      - | 3062 | ` * $length` |
|      - | 3063 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3064 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3065 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3066 | ` *  the end of the line.` |
|      - | 3067 | ` * Return` |
|      - | 3068 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3069 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3070 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3071 | ` */` |
|   2878 | 3072 | `static int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3073 |  |
|      - | 3074 | `	const ph7_io_stream *pStream;` |
|      - | 3075 | `	const char *zLine;` |
|      - | 3076 | `	io_private *pDev;` |
|      - | 3077 | `	ph7_int64 n,nLen;` |
|   2880 | 3078 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3079 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3080 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3081 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3082 | `		return PH7_OK;` |
|      - | 3083 | `	}` |
|      - | 3084 | `	/* Extract our private data */` |
|   2880 | 3085 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3086 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   2880 | 3087 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3088 | `		/*Expecting an IO handle */` |
|    ! 0 | 3089 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3090 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3091 | `		return PH7_OK;` |
|      - | 3092 | `	}` |
|      - | 3093 | `	/* Point to the target IO stream device */` |
|   2880 | 3094 | `	pStream = pDev->pStream;` |
|   2880 | 3095 | `	if( pStream == 0  ){` |
|    ! 0 | 3096 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3097 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3098 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3099 | `			);` |
|    ! 0 | 3100 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3101 | `		return PH7_OK;` |
|      - | 3102 | `	}` |
|   2880 | 3103 | `	nLen = -1;` |
|   2880 | 3104 | `	if( nArg > 1 ){` |
|      - | 3105 | `		/* Maximum data to read */` |
|    ! 0 | 3106 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3107 | `	}` |
|      - | 3108 | `	/* Perform the requested operation */` |
|   2880 | 3109 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|   2880 | 3110 | `	if( n < 1 ){` |
|      - | 3111 | `		/* EOF or IO error,return FALSE */` |
|      3 | 3112 | `		ph7_result_bool(pCtx,0);` |
|      2 | 3113 | `	}else{` |
|      - | 3114 | `		/* Return the freshly extracted line */` |
|   2878 | 3115 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - | 3116 | `	}` |
|   2880 | 3117 | `	return PH7_OK;` |
|   1441 | 3118 |  |
|      - | 3119 | `/*` |
|      - | 3120 | ` * string fread(resource $handle,int64 $length)` |
|      - | 3121 | ` *  Binary-safe file read.` |
|      - | 3122 | ` * Parameters` |
|      - | 3123 | ` *  $handle` |
|      - | 3124 | ` *   The file pointer.` |
|      - | 3125 | ` * $length` |
|      - | 3126 | ` *  Up to length number of bytes read.` |
|      - | 3127 | ` * Return` |
|      - | 3128 | ` *  The data readen on success or FALSE on failure.` |
|      - | 3129 | ` */` |
|     10 | 3130 | `static int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3131 |  |
|      - | 3132 | `	const ph7_io_stream *pStream;` |
|      - | 3133 | `	io_private *pDev;` |
|      - | 3134 | `	ph7_int64 nRead;` |
|      - | 3135 | `	void *pBuf;` |
|      - | 3136 | `	int nLen;` |
|     12 | 3137 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3138 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3139 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3140 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3141 | `		return PH7_OK;` |
|      - | 3142 | `	}` |
|      - | 3143 | `	/* Extract our private data */` |
|     12 | 3144 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3145 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     12 | 3146 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3147 | `		/*Expecting an IO handle */` |
|    ! 0 | 3148 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3149 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3150 | `		return PH7_OK;` |
|      - | 3151 | `	}` |
|      - | 3152 | `	/* Point to the target IO stream device */` |
|     12 | 3153 | `	pStream = pDev->pStream;` |
|     12 | 3154 | `	if( pStream == 0  ){` |
|    ! 0 | 3155 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3156 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3157 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3158 | `			);` |
|    ! 0 | 3159 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3160 | `		return PH7_OK;` |
|      - | 3161 | `	}` |
|     12 | 3162 | `        nLen = 4096;` |
|     12 | 3163 | `	if( nArg > 1 ){` |
|     12 | 3164 | ` 	  nLen = ph7_value_to_int(apArg[1]);` |
|     12 | 3165 | `	  if( nLen < 1 ){` |
|      - | 3166 | `		/* Invalid length,set a default length */` |
|    ! 0 | 3167 | `		nLen = 4096;` |
|    ! 0 | 3168 | `	  }` |
|      5 | 3169 | `        }` |
|      - | 3170 | `	/* Allocate enough buffer */` |
|     12 | 3171 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     12 | 3172 | `	if( pBuf == 0 ){` |
|    ! 0 | 3173 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3174 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3175 | `		return PH7_OK;` |
|      - | 3176 | `	}` |
|      - | 3177 | `	/* Perform the requested operation */` |
|     12 | 3178 | `	nRead = StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     12 | 3179 | `	if( nRead < 1 ){` |
|      - | 3180 | `		/* Nothing read,return FALSE */` |
|    ! 0 | 3181 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3182 | `	}else{` |
|      - | 3183 | `		/* Make a copy of the data just read */` |
|     12 | 3184 | `		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);` |
|      - | 3185 | `	}` |
|      - | 3186 | `	/* Release the buffer */` |
|     12 | 3187 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     12 | 3188 | `	return PH7_OK;` |
|      7 | 3189 |  |
|      - | 3190 | `/*` |
|      - | 3191 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|      - | 3192 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|      - | 3193 | ` * Gets line from file pointer and parse for CSV fields.` |
|      - | 3194 | ` * Parameters` |
|      - | 3195 | ` * $handle` |
|      - | 3196 | ` *   The file pointer.` |
|      - | 3197 | ` * $length` |
|      - | 3198 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3199 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3200 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3201 | ` *  the end of the line.` |
|      - | 3202 | ` * $delimiter` |
|      - | 3203 | ` *   Set the field delimiter (one character only).` |
|      - | 3204 | ` * $enclosure` |
|      - | 3205 | ` *   Set the field enclosure character (one character only).` |
|      - | 3206 | ` * $escape` |
|      - | 3207 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 3208 | ` * Return` |
|      - | 3209 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3210 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3211 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3212 | ` */` |
|      2 | 3213 | `static int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3214 |  |
|      - | 3215 | `	const ph7_io_stream *pStream;` |
|      - | 3216 | `	const char *zLine;` |
|      - | 3217 | `	io_private *pDev;` |
|      - | 3218 | `	ph7_int64 n,nLen;` |
|      3 | 3219 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3220 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3221 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3222 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3223 | `		return PH7_OK;` |
|      - | 3224 | `	}` |
|      - | 3225 | `	/* Extract our private data */` |
|      3 | 3226 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3227 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3228 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3229 | `		/*Expecting an IO handle */` |
|    ! 0 | 3230 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3231 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3232 | `		return PH7_OK;` |
|      - | 3233 | `	}` |
|      - | 3234 | `	/* Point to the target IO stream device */` |
|      3 | 3235 | `	pStream = pDev->pStream;` |
|      3 | 3236 | `	if( pStream == 0  ){` |
|    ! 0 | 3237 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3238 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3239 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3240 | `			);` |
|    ! 0 | 3241 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3242 | `		return PH7_OK;` |
|      - | 3243 | `	}` |
|      3 | 3244 | `	nLen = -1;` |
|      3 | 3245 | `	if( nArg > 1 ){` |
|      - | 3246 | `		/* Maximum data to read */` |
|      3 | 3247 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      1 | 3248 | `	}` |
|      - | 3249 | `	/* Perform the requested operation */` |
|      3 | 3250 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3251 | `	if( n < 1 ){` |
|      - | 3252 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3253 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3254 | `	}else{` |
|      - | 3255 | `		ph7_value *pArray;` |
|      3 | 3256 | `		int delim  = ',';   /* Delimiter */` |
|      3 | 3257 | `		int encl   = '"' ;  /* Enclosure */` |
|      3 | 3258 | `		int escape = '\\';  /* Escape character */` |
|      3 | 3259 | `		if( nArg > 2 ){` |
|      - | 3260 | `			const char *zPtr;` |
|      - | 3261 | `			int i;` |
|      3 | 3262 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 3263 | `				/* Extract the delimiter */` |
|      3 | 3264 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 3265 | `				if( i > 0 ){` |
|      3 | 3266 | `					delim = zPtr[0];` |
|      1 | 3267 | `				}` |
|      1 | 3268 | `			}` |
|      3 | 3269 | `			if( nArg > 3 ){` |
|      3 | 3270 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 3271 | `					/* Extract the enclosure */` |
|      3 | 3272 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 3273 | `					if( i > 0 ){` |
|      3 | 3274 | `						encl = zPtr[0];` |
|      1 | 3275 | `					}` |
|      1 | 3276 | `				}` |
|      3 | 3277 | `				if( nArg > 4 ){` |
|      3 | 3278 | `					if( ph7_value_is_string(apArg[4]) ){` |
|      - | 3279 | `						/* Extract the escape character */` |
|      3 | 3280 | `						zPtr = ph7_value_to_string(apArg[4],&i);` |
|      3 | 3281 | `						if( i > 0 ){` |
|      3 | 3282 | `							escape = zPtr[0];` |
|      1 | 3283 | `						}` |
|      1 | 3284 | `					}` |
|      1 | 3285 | `				}` |
|      1 | 3286 | `			}` |
|      1 | 3287 | `		}` |
|      - | 3288 | `		/* Create our array */` |
|      3 | 3289 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 3290 | `		if( pArray == 0 ){` |
|    ! 0 | 3291 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3292 | `			ph7_result_null(pCtx);` |
|    ! 0 | 3293 | `			return PH7_OK;` |
|      - | 3294 | `		}` |
|      - | 3295 | `		/* Parse the raw input */` |
|      3 | 3296 | `		PH7_ProcessCsv(zLine,(int)n,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 3297 | `		/* Return the freshly created array  */` |
|      3 | 3298 | `		ph7_result_value(pCtx,pArray);` |
|      - | 3299 | `	}` |
|      3 | 3300 | `	return PH7_OK;` |
|      2 | 3301 |  |
|      - | 3302 | `/*` |
|      - | 3303 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|      - | 3304 | ` *  Gets line from file pointer and strip HTML tags.` |
|      - | 3305 | ` * Parameters` |
|      - | 3306 | ` * $handle` |
|      - | 3307 | ` *   The file pointer.` |
|      - | 3308 | ` * $length` |
|      - | 3309 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3310 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3311 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3312 | ` *  the end of the line.` |
|      - | 3313 | ` * $allowable_tags` |
|      - | 3314 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 3315 | ` * Return` |
|      - | 3316 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|      - | 3317 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|      - | 3318 | ` */` |
|      2 | 3319 | `static int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3320 |  |
|      - | 3321 | `	const ph7_io_stream *pStream;` |
|      - | 3322 | `	const char *zLine;` |
|      - | 3323 | `	io_private *pDev;` |
|      - | 3324 | `	ph7_int64 n,nLen;` |
|      3 | 3325 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3326 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3327 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3328 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3329 | `		return PH7_OK;` |
|      - | 3330 | `	}` |
|      - | 3331 | `	/* Extract our private data */` |
|      3 | 3332 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3333 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3334 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3335 | `		/*Expecting an IO handle */` |
|    ! 0 | 3336 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3337 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3338 | `		return PH7_OK;` |
|      - | 3339 | `	}` |
|      - | 3340 | `	/* Point to the target IO stream device */` |
|      3 | 3341 | `	pStream = pDev->pStream;` |
|      3 | 3342 | `	if( pStream == 0  ){` |
|    ! 0 | 3343 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3344 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3345 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3346 | `			);` |
|    ! 0 | 3347 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3348 | `		return PH7_OK;` |
|      - | 3349 | `	}` |
|      3 | 3350 | `	nLen = -1;` |
|      3 | 3351 | `	if( nArg > 1 ){` |
|      - | 3352 | `		/* Maximum data to read */` |
|    ! 0 | 3353 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3354 | `	}` |
|      - | 3355 | `	/* Perform the requested operation */` |
|      3 | 3356 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3357 | `	if( n < 1 ){` |
|      - | 3358 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3359 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3360 | `	}else{` |
|      3 | 3361 | `		const char *zTaglist = 0;` |
|      3 | 3362 | `		int nTaglen = 0;` |
|      3 | 3363 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|      - | 3364 | `			/* Allowed tag */` |
|    ! 0 | 3365 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|    ! 0 | 3366 | `		}` |
|      - | 3367 | `		/* Process data just read */` |
|      3 | 3368 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen);` |
|      - | 3369 | `	}` |
|      3 | 3370 | `	return PH7_OK;` |
|      2 | 3371 |  |
|      - | 3372 | `/*` |
|      - | 3373 | ` * string readdir(resource $dir_handle)` |
|      - | 3374 | ` *   Read entry from directory handle.` |
|      - | 3375 | ` * Parameter` |
|      - | 3376 | ` *  $dir_handle` |
|      - | 3377 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3378 | ` * Return` |
|      - | 3379 | ` *  Returns the filename on success or FALSE on failure.` |
|      - | 3380 | ` */` |
|   5590 | 3381 | `static int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3382 |  |
|      - | 3383 | `	const ph7_io_stream *pStream;` |
|      - | 3384 | `	io_private *pDev;` |
|      - | 3385 | `	int rc;` |
|   5592 | 3386 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3387 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3388 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3389 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3390 | `		return PH7_OK;` |
|      - | 3391 | `	}` |
|      - | 3392 | `	/* Extract our private data */` |
|   5592 | 3393 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3394 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   5592 | 3395 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3396 | `		/*Expecting an IO handle */` |
|    ! 0 | 3397 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3398 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3399 | `		return PH7_OK;` |
|      - | 3400 | `	}` |
|      - | 3401 | `	/* Point to the target IO stream device */` |
|   5592 | 3402 | `	pStream = pDev->pStream;` |
|   5592 | 3403 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 3404 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3405 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3406 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3407 | `			);` |
|    ! 0 | 3408 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3409 | `		return PH7_OK;` |
|      - | 3410 | `	}` |
|   5592 | 3411 | `	ph7_result_bool(pCtx,0);` |
|      - | 3412 | `	/* Perform the requested operation */` |
|   5592 | 3413 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|   5592 | 3414 | `	if( rc != PH7_OK ){` |
|      - | 3415 | `		/* Return FALSE */` |
|    836 | 3416 | `		ph7_result_bool(pCtx,0);` |
|    417 | 3417 | `	}` |
|   5592 | 3418 | `	return PH7_OK;` |
|   2797 | 3419 |  |
|      - | 3420 | `/*` |
|      - | 3421 | ` * void rewinddir(resource $dir_handle)` |
|      - | 3422 | ` *   Rewind directory handle.` |
|      - | 3423 | ` * Parameter` |
|      - | 3424 | ` *  $dir_handle` |
|      - | 3425 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3426 | ` * Return` |
|      - | 3427 | ` *  FALSE on failure.` |
|      - | 3428 | ` */` |
|      2 | 3429 | `static int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3430 |  |
|      - | 3431 | `	const ph7_io_stream *pStream;` |
|      - | 3432 | `	io_private *pDev;` |
|      3 | 3433 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3434 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3435 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3436 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3437 | `		return PH7_OK;` |
|      - | 3438 | `	}` |
|      - | 3439 | `	/* Extract our private data */` |
|      3 | 3440 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3441 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3442 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3443 | `		/*Expecting an IO handle */` |
|    ! 0 | 3444 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3445 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3446 | `		return PH7_OK;` |
|      - | 3447 | `	}` |
|      - | 3448 | `	/* Point to the target IO stream device */` |
|      3 | 3449 | `	pStream = pDev->pStream;` |
|      3 | 3450 | `	if( pStream == 0  \|\| pStream->xRewindDir == 0 ){` |
|    ! 0 | 3451 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3452 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3453 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3454 | `			);` |
|    ! 0 | 3455 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3456 | `		return PH7_OK;` |
|      - | 3457 | `	}` |
|      - | 3458 | `	/* Perform the requested operation */` |
|      3 | 3459 | `	pStream->xRewindDir(pDev->pHandle);` |
|      3 | 3460 | `	return PH7_OK;` |
|      2 | 3461 | ` }` |
|      - | 3462 | `/* Forward declaration */` |
|      - | 3463 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut);` |
|      - | 3464 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|      - | 3465 | `/*` |
|      - | 3466 | ` * void closedir(resource $dir_handle)` |
|      - | 3467 | ` *   Close directory handle.` |
|      - | 3468 | ` * Parameter` |
|      - | 3469 | ` *  $dir_handle` |
|      - | 3470 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3471 | ` * Return` |
|      - | 3472 | ` *  FALSE on failure.` |
|      - | 3473 | ` */` |
|    836 | 3474 | `static int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3475 |  |
|      - | 3476 | `	const ph7_io_stream *pStream;` |
|      - | 3477 | `	io_private *pDev;` |
|    838 | 3478 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3479 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3480 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3481 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3482 | `		return PH7_OK;` |
|      - | 3483 | `	}` |
|      - | 3484 | `	/* Extract our private data */` |
|    838 | 3485 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3486 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    838 | 3487 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3488 | `		/*Expecting an IO handle */` |
|    ! 0 | 3489 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3490 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3491 | `		return PH7_OK;` |
|      - | 3492 | `	}` |
|      - | 3493 | `	/* Point to the target IO stream device */` |
|    838 | 3494 | `	pStream = pDev->pStream;` |
|    838 | 3495 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 3496 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3497 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3498 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3499 | `			);` |
|    ! 0 | 3500 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3501 | `		return PH7_OK;` |
|      - | 3502 | `	}` |
|      - | 3503 | `	/* Perform the requested operation */` |
|    838 | 3504 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 3505 | `	/* Release the private stucture */` |
|    838 | 3506 | `	ReleaseIOPrivate(pCtx,pDev);` |
|    838 | 3507 | `	PH7_MemObjRelease(apArg[0]);` |
|    838 | 3508 | `	return PH7_OK;` |
|    420 | 3509 | ` }` |
|      - | 3510 | `/*` |
|      - | 3511 | ` * resource opendir(string $path[,resource $context])` |
|      - | 3512 | ` *  Open directory handle.` |
|      - | 3513 | ` * Parameters` |
|      - | 3514 | ` * $path` |
|      - | 3515 | ` *   The directory path that is to be opened.` |
|      - | 3516 | ` * $context` |
|      - | 3517 | ` *   A context stream resource.` |
|      - | 3518 | ` * Return` |
|      - | 3519 | ` *  A directory handle resource on success,or FALSE on failure.` |
|      - | 3520 | ` */` |
|    836 | 3521 | `static int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3522 |  |
|      - | 3523 | `	const ph7_io_stream *pStream;` |
|      - | 3524 | `	const char *zPath;` |
|      - | 3525 | `	io_private *pDev;` |
|      - | 3526 | `	int iLen,rc;` |
|    838 | 3527 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3528 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3529 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 3530 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3531 | `		return PH7_OK;` |
|      - | 3532 | `	}` |
|      - | 3533 | `	/* Extract the target path */` |
|    838 | 3534 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 3535 | `	/* Try to extract a stream */` |
|    838 | 3536 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|    838 | 3537 | `	if( pStream == 0 ){` |
|    ! 0 | 3538 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3539 | `			"No stream device is associated with the given path(%s)",zPath);` |
|    ! 0 | 3540 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3541 | `		return PH7_OK;` |
|      - | 3542 | `	}` |
|    838 | 3543 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 3544 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3545 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 3546 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 3547 | `			);` |
|    ! 0 | 3548 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3549 | `		return PH7_OK;` |
|      - | 3550 | `	}` |
|      - | 3551 | `	/* Allocate a new IO private instance */` |
|    838 | 3552 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    838 | 3553 | `	if( pDev == 0 ){` |
|    ! 0 | 3554 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3555 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3556 | `		return PH7_OK;` |
|      - | 3557 | `	}` |
|      - | 3558 | `	/* Initialize the structure */` |
|    838 | 3559 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 3560 | `	/* Open the target directory */` |
|    838 | 3561 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|    838 | 3562 | `	if( rc != PH7_OK ){` |
|      - | 3563 | `		/* IO error,return FALSE */` |
|    ! 0 | 3564 | `		ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 3565 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3566 | `	}else{` |
|      - | 3567 | `		/* Return the handle as a resource */` |
|    838 | 3568 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 3569 | `	}` |
|    838 | 3570 | `	return PH7_OK;` |
|    420 | 3571 |  |
|      - | 3572 | `/*` |
|      - | 3573 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|      - | 3574 | ` *  Reads a file and writes it to the output buffer.` |
|      - | 3575 | ` * Parameters` |
|      - | 3576 | ` *  $filename` |
|      - | 3577 | ` *   The filename being read.` |
|      - | 3578 | ` *  $use_include_path` |
|      - | 3579 | ` *   You can use the optional second parameter and set it to` |
|      - | 3580 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3581 | ` *  $context` |
|      - | 3582 | ` *   A context stream resource.` |
|      - | 3583 | ` * Return` |
|      - | 3584 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|      - | 3585 | ` */` |
|      2 | 3586 | `static int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3587 |  |
|      3 | 3588 | `	int use_include  = FALSE;` |
|      - | 3589 | `	const ph7_io_stream *pStream;` |
|      - | 3590 | `	ph7_int64 n,nRead;` |
|      - | 3591 | `	const char *zFile;` |
|      - | 3592 | `	char zBuf[8192];` |
|      - | 3593 | `	void *pHandle;` |
|      - | 3594 | `	int rc,nLen;` |
|      3 | 3595 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3596 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3597 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3598 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3599 | `		return PH7_OK;` |
|      - | 3600 | `	}` |
|      - | 3601 | `	/* Extract the file path */` |
|      3 | 3602 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3603 | `	/* Point to the target IO stream device */` |
|      3 | 3604 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3605 | `	if( pStream == 0 ){` |
|    ! 0 | 3606 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3607 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3608 | `		return PH7_OK;` |
|      - | 3609 | `	}` |
|      3 | 3610 | `	if( nArg > 1 ){` |
|    ! 0 | 3611 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3612 | `	}` |
|      - | 3613 | `	/* Try to open the file in read-only mode */` |
|      4 | 3614 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|      1 | 3615 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 3616 | `	if( pHandle == 0 ){` |
|    ! 0 | 3617 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3618 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3619 | `		return PH7_OK;` |
|      - | 3620 | `	}` |
|      - | 3621 | `	/* Perform the requested operation */` |
|      3 | 3622 | `	nRead = 0;` |
|      2 | 3623 | `	for(;;){` |
|      5 | 3624 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3625 | `		if( n < 1 ){` |
|      - | 3626 | `			/* EOF or IO error,break immediately */` |
|      3 | 3627 | `			break;` |
|      - | 3628 | `		}` |
|      - | 3629 | `		/* Output data */` |
|      3 | 3630 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      3 | 3631 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 3632 | `			break;` |
|      - | 3633 | `		}` |
|      - | 3634 | `		/* Increment counter */` |
|      3 | 3635 | `		nRead += n;` |
|      1 | 3636 | `	}` |
|      - | 3637 | `	/* Close the stream */` |
|      3 | 3638 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3639 | `	/* Total number of bytes readen */` |
|      3 | 3640 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 3641 | `	return PH7_OK;` |
|      2 | 3642 |  |
|      - | 3643 | `/*` |
|      - | 3644 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|      - | 3645 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|      - | 3646 | ` *  Reads entire file into a string.` |
|      - | 3647 | ` * Parameters` |
|      - | 3648 | ` *  $filename` |
|      - | 3649 | ` *   The filename being read.` |
|      - | 3650 | ` *  $use_include_path` |
|      - | 3651 | ` *   You can use the optional second parameter and set it to` |
|      - | 3652 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3653 | ` *  $context` |
|      - | 3654 | ` *   A context stream resource.` |
|      - | 3655 | ` *  $offset` |
|      - | 3656 | ` *   The offset where the reading starts on the original stream.` |
|      - | 3657 | ` *  $maxlen` |
|      - | 3658 | ` *    Maximum length of data read. The default is to read until end of file` |
|      - | 3659 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|      - | 3660 | ` * Return` |
|      - | 3661 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3662 | ` */` |
|   3958 | 3663 | `static int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3664 |  |
|      - | 3665 | `	const ph7_io_stream *pStream;` |
|      - | 3666 | `	ph7_int64 n,nRead,nMaxlen;` |
|   3960 | 3667 | `	int use_include  = FALSE;` |
|      - | 3668 | `	const char *zFile;` |
|      - | 3669 | `	char zBuf[8192];` |
|      - | 3670 | `	void *pHandle;` |
|      - | 3671 | `	int nLen;` |
|      - | 3672 |  |
|   3960 | 3673 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3674 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3675 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3676 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3677 | `		return PH7_OK;` |
|      - | 3678 | `	}` |
|      - | 3679 | `	/* Extract the file path */` |
|   3960 | 3680 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3681 | `	/* Point to the target IO stream device */` |
|   3960 | 3682 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   3960 | 3683 | `	if( pStream == 0 ){` |
|    ! 0 | 3684 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3685 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3686 | `		return PH7_OK;` |
|      - | 3687 | `	}` |
|   3960 | 3688 | `	nMaxlen = -1;` |
|   3960 | 3689 | `	if( nArg > 1 ){` |
|      5 | 3690 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|      2 | 3691 | `	}` |
|      - | 3692 | `	/* Try to open the file in read-only mode */` |
|   3960 | 3693 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|   3960 | 3694 | `	if( pHandle == 0 ){` |
|    ! 0 | 3695 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3696 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3697 | `		return PH7_OK;` |
|      - | 3698 | `	}` |
|   3960 | 3699 | `	if( nArg > 3 ){` |
|      - | 3700 | `		/* Extract the offset */` |
|      5 | 3701 | `		n = ph7_value_to_int64(apArg[3]);` |
|      5 | 3702 | `		if( n > 0 ){` |
|    ! 0 | 3703 | `			if( pStream->xSeek ){` |
|      - | 3704 | `				/* Seek to the desired offset */` |
|    ! 0 | 3705 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|    ! 0 | 3706 | `			}` |
|    ! 0 | 3707 | `		}` |
|      5 | 3708 | `		if( nArg > 4 ){` |
|      - | 3709 | `			/* Maximum data to read */` |
|      5 | 3710 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|      2 | 3711 | `		}` |
|      2 | 3712 | `	}` |
|      - | 3713 | `	/* Perform the requested operation */` |
|   3960 | 3714 | `	nRead = 0;` |
|   3957 | 3715 | `	for(;;){` |
|  11876 | 3716 | `		n = pStream->xRead(pHandle,zBuf,` |
|   3960 | 3717 | `			(nMaxlen > 0 && (nMaxlen < (ph7_int64)sizeof(zBuf))) ? nMaxlen : (ph7_int64)sizeof(zBuf));` |
|   7916 | 3718 | `		if( n < 1 ){` |
|      - | 3719 | `			/* EOF or IO error,break immediately */` |
|   3958 | 3720 | `			break;` |
|      - | 3721 | `		}` |
|      - | 3722 | `		/* Append data */` |
|   3960 | 3723 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 3724 | `		/* Increment read counter */` |
|   3960 | 3725 | `		nRead += n;` |
|   3960 | 3726 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 3727 | `			/* Read limit reached */` |
|      3 | 3728 | `			break;` |
|      - | 3729 | `		}` |
|      2 | 3730 | `	}` |
|      - | 3731 | `	/* Close the stream */` |
|   3960 | 3732 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3733 | `	/* Check if we have read something */` |
|   3960 | 3734 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 3735 | `		/* Nothing read,return FALSE */` |
|    ! 0 | 3736 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3737 | `	}` |
|   3960 | 3738 | `	return PH7_OK;` |
|   1981 | 3739 |  |
|      - | 3740 | `/*` |
|      - | 3741 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 3742 | ` *  Write a string to a file.` |
|      - | 3743 | ` * Parameters` |
|      - | 3744 | ` *  $filename` |
|      - | 3745 | ` *  Path to the file where to write the data.` |
|      - | 3746 | ` * $data` |
|      - | 3747 | ` *  The data to write(Must be a string).` |
|      - | 3748 | ` * $flags` |
|      - | 3749 | ` *  The value of flags can be any combination of the following` |
|      - | 3750 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 3751 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 3752 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 3753 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 3754 | ` * context` |
|      - | 3755 | ` *  A context stream resource.` |
|      - | 3756 | ` * Return` |
|      - | 3757 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 3758 | ` */` |
|   9550 | 3759 | `static int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3760 |  |
|   9552 | 3761 | `	int use_include  = FALSE;` |
|      - | 3762 | `	const ph7_io_stream *pStream;` |
|      - | 3763 | `	const char *zFile;` |
|      - | 3764 | `	const char *zData;` |
|      - | 3765 | `	int iOpenFlags;` |
|      - | 3766 | `	void *pHandle;` |
|      - | 3767 | `	int iFlags;` |
|      - | 3768 | `	int nLen;` |
|      - | 3769 |  |
|   9552 | 3770 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3771 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3772 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3773 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3774 | `		return PH7_OK;` |
|      - | 3775 | `	}` |
|      - | 3776 | `	/* Extract the file path */` |
|   9552 | 3777 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3778 | `	/* Point to the target IO stream device */` |
|   9552 | 3779 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   9552 | 3780 | `	if( pStream == 0 ){` |
|    ! 0 | 3781 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3782 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3783 | `		return PH7_OK;` |
|      - | 3784 | `	}` |
|      - | 3785 | `	/* Data to write */` |
|   9552 | 3786 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 3787 | `	/* Try to open the file in read-write mode */` |
|   9552 | 3788 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 3789 | `	/* Extract the flags */` |
|   9552 | 3790 | `	iFlags = 0;` |
|   9552 | 3791 | `	if( nArg > 2 ){` |
|    ! 0 | 3792 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3793 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 3794 | `			use_include = TRUE;` |
|    ! 0 | 3795 | `		}` |
|    ! 0 | 3796 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 3797 | `			/* If the file already exists, append the data to the file` |
|      - | 3798 | `			 * instead of overwriting it.` |
|      - | 3799 | `			 */` |
|    ! 0 | 3800 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 3801 | `			/* Append mode */` |
|    ! 0 | 3802 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 3803 | `		}` |
|    ! 0 | 3804 | `	}` |
|  14327 | 3805 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   4775 | 3806 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|   9552 | 3807 | `	if( pHandle == 0 ){` |
|    ! 0 | 3808 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3809 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3810 | `		return PH7_OK;` |
|      - | 3811 | `	}` |
|   9552 | 3812 | `	if( nLen < 1 ){` |
|      - | 3813 | `		/* Empty data, file is created/truncated */` |
|      7 | 3814 | `		ph7_result_int64(pCtx,0);` |
|      7 | 3815 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|      7 | 3816 | `		return PH7_OK;` |
|      - | 3817 | `	}` |
|   9546 | 3818 | `	if( pStream->xWrite ){` |
|      - | 3819 | `		ph7_int64 n;` |
|   9546 | 3820 | `		if( (iFlags & 0x01/* LOCK_EX */) && pStream->xLock ){` |
|      - | 3821 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 3822 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 3823 | `		}` |
|      - | 3824 | `		/* Perform the write operation */` |
|   9546 | 3825 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|   9546 | 3826 | `		if( n < 0 ){` |
|      - | 3827 | `			/* IO error,return FALSE */` |
|    ! 0 | 3828 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3829 | `		}else{` |
|      - | 3830 | `			/* Total number of bytes written */` |
|   9546 | 3831 | `			ph7_result_int64(pCtx,n);` |
|      - | 3832 | `		}` |
|   4774 | 3833 | `	}else{` |
|      - | 3834 | `		/* Read-only stream */` |
|    ! 0 | 3835 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 3836 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 3837 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 3838 | `			);` |
|    ! 0 | 3839 | `		ph7_result_bool(pCtx,0);` |
|      - | 3840 | `	}` |
|      - | 3841 | `	/* Close the handle */` |
|   9546 | 3842 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|   9546 | 3843 | `	return PH7_OK;` |
|   4777 | 3844 |  |
|      - | 3845 | `/*` |
|      - | 3846 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 3847 | ` *  Reads entire file into an array.` |
|      - | 3848 | ` * Parameters` |
|      - | 3849 | ` *  $filename` |
|      - | 3850 | ` *   The filename being read.` |
|      - | 3851 | ` *  $flags` |
|      - | 3852 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 3853 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 3854 | ` *       Search for the file in the include_path.` |
|      - | 3855 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 3856 | ` *       Do not add newline at the end of each array element` |
|      - | 3857 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 3858 | ` *       Skip empty lines` |
|      - | 3859 | ` *  $context` |
|      - | 3860 | ` *   A context stream resource.` |
|      - | 3861 | ` * Return` |
|      - | 3862 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3863 | ` */` |
|      8 | 3864 | `static int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3865 |  |
|      - | 3866 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 3867 | `	ph7_value *pArray,*pLine;` |
|      - | 3868 | `	const ph7_io_stream *pStream;` |
|     10 | 3869 | `	int use_include = 0;` |
|      - | 3870 | `	io_private *pDev;` |
|      - | 3871 | `	ph7_int64 n;` |
|      - | 3872 | `	int iFlags;` |
|      - | 3873 | `	int nLen;` |
|      - | 3874 |  |
|     10 | 3875 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3876 | `		/* Missing/Invalid arguments,return FALSE */` |
|      5 | 3877 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|      5 | 3878 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3879 | `		return PH7_OK;` |
|      - | 3880 | `	}` |
|      - | 3881 | `	/* Extract the file path */` |
|      6 | 3882 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3883 | `	/* Point to the target IO stream device */` |
|      6 | 3884 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 3885 | `	if( pStream == 0 ){` |
|    ! 0 | 3886 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3887 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3888 | `		return PH7_OK;` |
|      - | 3889 | `	}` |
|      - | 3890 | `	/* Allocate a new IO private instance */` |
|      6 | 3891 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      6 | 3892 | `	if( pDev == 0 ){` |
|    ! 0 | 3893 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3894 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3895 | `		return PH7_OK;` |
|      - | 3896 | `	}` |
|      - | 3897 | `	/* Initialize the structure */` |
|      6 | 3898 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      6 | 3899 | `	iFlags = 0;` |
|      6 | 3900 | `	if( nArg > 1 ){` |
|    ! 0 | 3901 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 3902 | `	}` |
|      6 | 3903 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|    ! 0 | 3904 | `		use_include = TRUE;` |
|    ! 0 | 3905 | `	}` |
|      - | 3906 | `	/* Create the array and the working value */` |
|      6 | 3907 | `	pArray = ph7_context_new_array(pCtx);` |
|      6 | 3908 | `	pLine = ph7_context_new_scalar(pCtx);` |
|      6 | 3909 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 3910 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3911 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3912 | `		return PH7_OK;` |
|      - | 3913 | `	}` |
|      - | 3914 | `	/* Try to open the file in read-only mode */` |
|      6 | 3915 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 3916 | `	if( pDev->pHandle == 0 ){` |
|      3 | 3917 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|      3 | 3918 | `		ph7_result_bool(pCtx,0);` |
|      - | 3919 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 3920 | `		 * as soon we return from this function.` |
|      - | 3921 | `		 */` |
|      3 | 3922 | `		return PH7_OK;` |
|      - | 3923 | `	}` |
|      - | 3924 | `	/* Perform the requested operation */` |
|      3 | 3925 | `	for(;;){` |
|      - | 3926 | `		/* Try to extract a line */` |
|      7 | 3927 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|      7 | 3928 | `		if( n < 1 ){` |
|      - | 3929 | `			/* EOF or IO error */` |
|      3 | 3930 | `			break;` |
|      - | 3931 | `		}` |
|      - | 3932 | `		/* Reset the cursor */` |
|      5 | 3933 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 3934 | `		/* Remove line ending if requested by the caller */` |
|      5 | 3935 | `		zPtr = zBuf;` |
|      5 | 3936 | `		zEnd = &zBuf[n];` |
|      5 | 3937 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 3938 | `			/* Ignore trailig lines */` |
|    ! 0 | 3939 | `			while( zPtr < zEnd && (zEnd[-1] == '\n'` |
|      - | 3940 | `#ifdef __WINNT__` |
|      - | 3941 | `				\|\| zEnd[-1] == '\r'` |
|      - | 3942 | `#endif` |
|      - | 3943 | `				)){` |
|    ! 0 | 3944 | `					n--;` |
|    ! 0 | 3945 | `					zEnd--;` |
|    ! 0 | 3946 | `			}` |
|    ! 0 | 3947 | `		}` |
|      5 | 3948 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 3949 | `			/* Ignore empty lines */` |
|    ! 0 | 3950 | `			while( zPtr < zEnd && (unsigned char)zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) ){` |
|    ! 0 | 3951 | `				zPtr++;` |
|    ! 0 | 3952 | `			}` |
|    ! 0 | 3953 | `			if( zPtr >= zEnd ){` |
|      - | 3954 | `				/* Empty line */` |
|    ! 0 | 3955 | `				continue;` |
|      - | 3956 | `			}` |
|    ! 0 | 3957 | `		}` |
|      5 | 3958 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 3959 | `		/* Insert line */` |
|      5 | 3960 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 3961 | `	}` |
|      - | 3962 | `	/* Close the stream */` |
|      3 | 3963 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 3964 | `	/* Release the io_private instance */` |
|      3 | 3965 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 3966 | `	/* Return the created array */` |
|      3 | 3967 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3968 | `	return PH7_OK;` |
|      6 | 3969 |  |
|      - | 3970 | `/*` |
|      - | 3971 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 3972 | ` *  Makes a copy of the file source to dest.` |
|      - | 3973 | ` * Parameters` |
|      - | 3974 | ` *  $source` |
|      - | 3975 | ` *   Path to the source file.` |
|      - | 3976 | ` *  $dest` |
|      - | 3977 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 3978 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 3979 | ` *  $context` |
|      - | 3980 | ` *   A context stream resource.` |
|      - | 3981 | ` * Return` |
|      - | 3982 | ` *  TRUE on success or FALSE on failure.` |
|      - | 3983 | ` */` |
|     10 | 3984 | `static int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3985 |  |
|      - | 3986 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 3987 | `	const char *zFile;` |
|      - | 3988 | `	char zBuf[8192];` |
|      - | 3989 | `	void *pIn,*pOut;` |
|      - | 3990 | `	ph7_int64 n;` |
|      - | 3991 | `	int nLen;` |
|     12 | 3992 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 3993 | `		/* Missing/Invalid arguments,return FALSE */` |
|      7 | 3994 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|      7 | 3995 | `		ph7_result_bool(pCtx,0);` |
|      7 | 3996 | `		return PH7_OK;` |
|      - | 3997 | `	}` |
|      - | 3998 | `	/* Extract the source name */` |
|      6 | 3999 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4000 | `	/* Point to the target IO stream device */` |
|      6 | 4001 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 4002 | `	if( pSin == 0 ){` |
|    ! 0 | 4003 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4004 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4005 | `		return PH7_OK;` |
|      - | 4006 | `	}` |
|      - | 4007 | `	/* Try to open the source file in a read-only mode */` |
|      6 | 4008 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 4009 | `	if( pIn == 0 ){` |
|      3 | 4010 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening source: '%s'",zFile);` |
|      3 | 4011 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4012 | `		return PH7_OK;` |
|      - | 4013 | `	}` |
|      - | 4014 | `	/* Extract the destination name */` |
|      3 | 4015 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 4016 | `	/* Point to the target IO stream device */` |
|      3 | 4017 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4018 | `	if( pSout == 0 ){` |
|    ! 0 | 4019 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4020 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4021 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4022 | `		return PH7_OK;` |
|      - | 4023 | `	}` |
|      3 | 4024 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 4025 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4026 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4027 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 4028 | `			);` |
|    ! 0 | 4029 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4030 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4031 | `		return PH7_OK;` |
|      - | 4032 | `	}` |
|      - | 4033 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 4034 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 4035 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 4036 | `	if( pOut == 0 ){` |
|    ! 0 | 4037 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening destination: '%s'",zFile);` |
|    ! 0 | 4038 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4039 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4040 | `		return PH7_OK;` |
|      - | 4041 | `	}` |
|      - | 4042 | `	/* Perform the requested operation */` |
|      2 | 4043 | `	for(;;){` |
|      - | 4044 | `		/* Read from source */` |
|      5 | 4045 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 4046 | `		if( n < 1 ){` |
|      - | 4047 | `			/* EOF or IO error,break immediately */` |
|      3 | 4048 | `			break;` |
|      - | 4049 | `		}` |
|      - | 4050 | `		/* Write to dest */` |
|      3 | 4051 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 4052 | `		if( n < 1 ){` |
|      - | 4053 | `			/* IO error,break immediately */` |
|    ! 0 | 4054 | `			break;` |
|      - | 4055 | `		}` |
|      1 | 4056 | `	}` |
|      - | 4057 | `	/* Close the streams */` |
|      3 | 4058 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 4059 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 4060 | `	/* Return TRUE */` |
|      3 | 4061 | `	ph7_result_bool(pCtx,1);` |
|      3 | 4062 | `	return PH7_OK;` |
|      7 | 4063 |  |
|      - | 4064 | `/*` |
|      - | 4065 | ` * array fstat(resource $handle)` |
|      - | 4066 | ` *  Gets information about a file using an open file pointer.` |
|      - | 4067 | ` * Parameters` |
|      - | 4068 | ` *  $handle` |
|      - | 4069 | ` *   The file pointer.` |
|      - | 4070 | ` * Return` |
|      - | 4071 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 4072 | ` */` |
|      2 | 4073 | `static int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4074 |  |
|      - | 4075 | `	ph7_value *pArray,*pValue;` |
|      - | 4076 | `	const ph7_io_stream *pStream;` |
|      - | 4077 | `	io_private *pDev;` |
|      3 | 4078 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4079 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4080 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4081 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4082 | `		return PH7_OK;` |
|      - | 4083 | `	}` |
|      - | 4084 | `	/* Extract our private data */` |
|      3 | 4085 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4086 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4087 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4088 | `		/* Expecting an IO handle */` |
|    ! 0 | 4089 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4090 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4091 | `		return PH7_OK;` |
|      - | 4092 | `	}` |
|      - | 4093 | `	/* Point to the target IO stream device */` |
|      3 | 4094 | `	pStream = pDev->pStream;` |
|      3 | 4095 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 4096 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4097 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4098 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4099 | `			);` |
|    ! 0 | 4100 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4101 | `		return PH7_OK;` |
|      - | 4102 | `	}` |
|      - | 4103 | `	/* Create the array and the working value */` |
|      3 | 4104 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4105 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4106 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4107 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4108 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4109 | `		return PH7_OK;` |
|      - | 4110 | `	}` |
|      - | 4111 | `	/* Perform the requested operation */` |
|      3 | 4112 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 4113 | `	/* Return the freshly created array */` |
|      3 | 4114 | `	ph7_result_value(pCtx,pArray);` |
|      - | 4115 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 4116 | `	 * released automatically as soon we return from this function.` |
|      - | 4117 | `	 */` |
|      3 | 4118 | `	return PH7_OK;` |
|      2 | 4119 |  |
|      - | 4120 | `/*` |
|      - | 4121 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 4122 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 4123 | ` * Parameters` |
|      - | 4124 | ` *  $handle` |
|      - | 4125 | ` *   The file pointer.` |
|      - | 4126 | ` *  $string` |
|      - | 4127 | ` *   The string that is to be written.` |
|      - | 4128 | ` *  $length` |
|      - | 4129 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 4130 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 4131 | ` * Return` |
|      - | 4132 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 4133 | ` */` |
|      6 | 4134 | `static int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4135 |  |
|      - | 4136 | `	const ph7_io_stream *pStream;` |
|      - | 4137 | `	const char *zString;` |
|      - | 4138 | `	io_private *pDev;` |
|      - | 4139 | `	int nLen,n;` |
|      7 | 4140 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4141 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4142 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4143 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4144 | `		return PH7_OK;` |
|      - | 4145 | `	}` |
|      - | 4146 | `	/* Extract our private data */` |
|      7 | 4147 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4148 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 4149 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4150 | `		/* Expecting an IO handle */` |
|    ! 0 | 4151 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4152 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4153 | `		return PH7_OK;` |
|      - | 4154 | `	}` |
|      - | 4155 | `	/* Point to the target IO stream device */` |
|      7 | 4156 | `	pStream = pDev->pStream;` |
|      7 | 4157 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4158 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4159 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4160 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4161 | `			);` |
|    ! 0 | 4162 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4163 | `		return PH7_OK;` |
|      - | 4164 | `	}` |
|      - | 4165 | `	/* Extract the data to write */` |
|      7 | 4166 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|      7 | 4167 | `	if( nArg > 2 ){` |
|      - | 4168 | `		/* Maximum data length to write */` |
|    ! 0 | 4169 | `		n = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 4170 | `		if( n >= 0 && n < nLen ){` |
|    ! 0 | 4171 | `			nLen = n;` |
|    ! 0 | 4172 | `		}` |
|    ! 0 | 4173 | `	}` |
|      7 | 4174 | `	if( nLen < 1 ){` |
|      - | 4175 | `		/* Nothing to write */` |
|    ! 0 | 4176 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4177 | `		return PH7_OK;` |
|      - | 4178 | `	}` |
|      - | 4179 | `	/* Perform the requested operation */` |
|      7 | 4180 | `	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);` |
|      7 | 4181 | `	if( n <  0 ){` |
|      - | 4182 | `		/* IO error,return FALSE */` |
|    ! 0 | 4183 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4184 | `	}else{` |
|      - | 4185 | `		/* #Bytes written */` |
|      7 | 4186 | `		ph7_result_int(pCtx,n);` |
|      - | 4187 | `	}` |
|      7 | 4188 | `	return PH7_OK;` |
|      4 | 4189 |  |
|      - | 4190 | `/*` |
|      - | 4191 | ` * bool flock(resource $handle,int $operation)` |
|      - | 4192 | ` *  Portable advisory file locking.` |
|      - | 4193 | ` * Parameters` |
|      - | 4194 | ` *  $handle` |
|      - | 4195 | ` *   The file pointer.` |
|      - | 4196 | ` *  $operation` |
|      - | 4197 | ` *   operation is one of the following:` |
|      - | 4198 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 4199 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 4200 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 4201 | ` * Return` |
|      - | 4202 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 4203 | ` */` |
|      4 | 4204 | `static int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4205 |  |
|      - | 4206 | `	const ph7_io_stream *pStream;` |
|      - | 4207 | `	io_private *pDev;` |
|      - | 4208 | `	int nLock;` |
|      - | 4209 | `	int rc;` |
|      5 | 4210 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4211 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4212 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4213 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4214 | `		return PH7_OK;` |
|      - | 4215 | `	}` |
|      - | 4216 | `	/* Extract our private data */` |
|      5 | 4217 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4218 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 4219 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4220 | `		/*Expecting an IO handle */` |
|    ! 0 | 4221 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4222 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4223 | `		return PH7_OK;` |
|      - | 4224 | `	}` |
|      - | 4225 | `	/* Point to the target IO stream device */` |
|      5 | 4226 | `	pStream = pDev->pStream;` |
|      5 | 4227 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|    ! 0 | 4228 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4229 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4230 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4231 | `			);` |
|    ! 0 | 4232 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4233 | `		return PH7_OK;` |
|      - | 4234 | `	}` |
|      - | 4235 | `	/* Requested lock operation */` |
|      5 | 4236 | `	nLock = ph7_value_to_int(apArg[1]);` |
|      - | 4237 | `	/* Lock operation */` |
|      5 | 4238 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 4239 | `	/* IO result */` |
|      5 | 4240 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 4241 | `	return PH7_OK;` |
|      3 | 4242 |  |
|      - | 4243 | `/*` |
|      - | 4244 | ` * int fpassthru(resource $handle)` |
|      - | 4245 | ` *  Output all remaining data on a file pointer.` |
|      - | 4246 | ` * Parameters` |
|      - | 4247 | ` *  $handle` |
|      - | 4248 | ` *   The file pointer.` |
|      - | 4249 | ` * Return` |
|      - | 4250 | ` *  Total number of characters read from handle and passed through` |
|      - | 4251 | ` *  to the output on success or FALSE on failure.` |
|      - | 4252 | ` */` |
|      2 | 4253 | `static int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4254 |  |
|      - | 4255 | `	const ph7_io_stream *pStream;` |
|      - | 4256 | `	io_private *pDev;` |
|      - | 4257 | `	ph7_int64 n,nRead;` |
|      - | 4258 | `	char zBuf[8192];` |
|      - | 4259 | `	int rc;` |
|      3 | 4260 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4261 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4262 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4263 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4264 | `		return PH7_OK;` |
|      - | 4265 | `	}` |
|      - | 4266 | `	/* Extract our private data */` |
|      3 | 4267 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4268 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4269 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4270 | `		/*Expecting an IO handle */` |
|    ! 0 | 4271 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4272 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4273 | `		return PH7_OK;` |
|      - | 4274 | `	}` |
|      - | 4275 | `	/* Point to the target IO stream device */` |
|      3 | 4276 | `	pStream = pDev->pStream;` |
|      3 | 4277 | `	if( pStream == 0  ){` |
|    ! 0 | 4278 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4279 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4280 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4281 | `			);` |
|    ! 0 | 4282 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4283 | `		return PH7_OK;` |
|      - | 4284 | `	}` |
|      - | 4285 | `	/* Perform the requested operation */` |
|      3 | 4286 | `	nRead = 0;` |
|      2 | 4287 | `	for(;;){` |
|      5 | 4288 | `		n = StreamRead(pDev,zBuf,sizeof(zBuf));` |
|      5 | 4289 | `		if( n < 1 ){` |
|      - | 4290 | `			/* Error or EOF */` |
|      3 | 4291 | `			break;` |
|      - | 4292 | `		}` |
|      - | 4293 | `		/* Increment the read counter */` |
|      3 | 4294 | `		nRead += n;` |
|      - | 4295 | `		/* Output data */` |
|      3 | 4296 | `		rc = ph7_context_output(pCtx,zBuf,(int)nRead /* FIXME: 64-bit issues */);` |
|      3 | 4297 | `		if( rc == PH7_ABORT ){` |
|      - | 4298 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 4299 | `			break;` |
|      - | 4300 | `		}` |
|      1 | 4301 | `	}` |
|      - | 4302 | `	/* Total number of bytes readen */` |
|      3 | 4303 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 4304 | `	return PH7_OK;` |
|      2 | 4305 |  |
|      - | 4306 | `/* CSV reader/writer private data */` |
|      - | 4307 | `struct csv_data` |
|      - | 4308 |  |
|      - | 4309 | `	int delimiter;    /* Delimiter. Default ',' */` |
|      - | 4310 | `	int enclosure;    /* Enclosure. Default '"'*/` |
|      - | 4311 | `	io_private *pDev; /* Open stream handle */` |
|      - | 4312 | `	int iCount;       /* Counter */` |
|      - | 4313 | `};` |
|      - | 4314 | `/*` |
|      - | 4315 | ` * The following callback is used by the fputcsv() function inorder to iterate` |
|      - | 4316 | ` * throw array entries and output CSV data based on the current key and it's` |
|      - | 4317 | ` * associated data.` |
|      - | 4318 | ` */` |
|      6 | 4319 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 4320 |  |
|      7 | 4321 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 4322 | `	const char *zData;` |
|      - | 4323 | `	int nLen,c2;` |
|      - | 4324 | `	sxu32 n;` |
|      - | 4325 | `	/* Point to the raw data */` |
|      7 | 4326 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      7 | 4327 | `	if( nLen < 1 ){` |
|      - | 4328 | `		/* Nothing to write */` |
|    ! 0 | 4329 | `		return PH7_OK;` |
|      - | 4330 | `	}` |
|      7 | 4331 | `	if( pData->iCount > 0 ){` |
|      - | 4332 | `		/* Write the delimiter */` |
|      5 | 4333 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->delimiter,sizeof(char));` |
|      2 | 4334 | `	}` |
|      7 | 4335 | `	n = 1;` |
|      7 | 4336 | `	c2 = 0;` |
|     10 | 4337 | `	if( SyByteFind(zData,(sxu32)nLen,pData->delimiter,0) == SXRET_OK \|\|` |
|      6 | 4338 | `		SyByteFind(zData,(sxu32)nLen,pData->enclosure,&n) == SXRET_OK ){` |
|    ! 0 | 4339 | `			c2 = 1;` |
|    ! 0 | 4340 | `			if( n == 0 ){` |
|    ! 0 | 4341 | `				c2 = 2;` |
|    ! 0 | 4342 | `			}` |
|      - | 4343 | `			/* Write the enclosure */` |
|    ! 0 | 4344 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4345 | `			if( c2 > 1 ){` |
|    ! 0 | 4346 | `				pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4347 | `			}` |
|    ! 0 | 4348 | `	}` |
|      - | 4349 | `	/* Write the data */` |
|      7 | 4350 | `	if( pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)zData,(ph7_int64)nLen) < 1 ){` |
|    ! 0 | 4351 | `		SXUNUSED(pKey); /* cc warning */` |
|    ! 0 | 4352 | `		return PH7_ABORT;` |
|      - | 4353 | `	}` |
|      7 | 4354 | `	if( c2 > 0 ){` |
|      - | 4355 | `		/* Write the enclosure */` |
|    ! 0 | 4356 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4357 | `		if( c2 > 1 ){` |
|    ! 0 | 4358 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4359 | `		}` |
|    ! 0 | 4360 | `	}` |
|      7 | 4361 | `	pData->iCount++;` |
|      7 | 4362 | `	return PH7_OK;` |
|      4 | 4363 |  |
|      - | 4364 | `/*` |
|      - | 4365 | ` * int fputcsv(resource $handle,array $fields[,string $delimiter = ','[,string $enclosure = '"' ]])` |
|      - | 4366 | ` *  Format line as CSV and write to file pointer.` |
|      - | 4367 | ` * Parameters` |
|      - | 4368 | ` *  $handle` |
|      - | 4369 | ` *   Open file handle.` |
|      - | 4370 | ` * $fields` |
|      - | 4371 | ` *   An array of values.` |
|      - | 4372 | ` * $delimiter` |
|      - | 4373 | ` *   The optional delimiter parameter sets the field delimiter (one character only).` |
|      - | 4374 | ` * $enclosure` |
|      - | 4375 | ` *  The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 4376 | ` */` |
|      2 | 4377 | `static int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4378 |  |
|      - | 4379 | `	const ph7_io_stream *pStream;` |
|      - | 4380 | `	struct csv_data sCsv;` |
|      - | 4381 | `	io_private *pDev;` |
|      - | 4382 | `	char *zEol;` |
|      - | 4383 | `	int eolen;` |
|      3 | 4384 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4385 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4386 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|    ! 0 | 4387 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4388 | `		return PH7_OK;` |
|      - | 4389 | `	}` |
|      - | 4390 | `	/* Extract our private data */` |
|      3 | 4391 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4392 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4393 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4394 | `		/*Expecting an IO handle */` |
|    ! 0 | 4395 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4396 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4397 | `		return PH7_OK;` |
|      - | 4398 | `	}` |
|      - | 4399 | `	/* Point to the target IO stream device */` |
|      3 | 4400 | `	pStream = pDev->pStream;` |
|      3 | 4401 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4402 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4403 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4404 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4405 | `			);` |
|    ! 0 | 4406 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4407 | `		return PH7_OK;` |
|      - | 4408 | `	}` |
|      - | 4409 | `	/* Set default csv separator */` |
|      3 | 4410 | `	sCsv.delimiter = ',';` |
|      3 | 4411 | `	sCsv.enclosure = '"';` |
|      3 | 4412 | `	sCsv.pDev = pDev;` |
|      3 | 4413 | `	sCsv.iCount = 0;` |
|      3 | 4414 | `	if( nArg > 2 ){` |
|      - | 4415 | `		/* User delimiter */` |
|      - | 4416 | `		const char *z;` |
|      - | 4417 | `		int n;` |
|      3 | 4418 | `		z = ph7_value_to_string(apArg[2],&n);` |
|      3 | 4419 | `		if( n > 0 ){` |
|      3 | 4420 | `			sCsv.delimiter = z[0];` |
|      1 | 4421 | `		}` |
|      3 | 4422 | `		if( nArg > 3 ){` |
|      3 | 4423 | `			z = ph7_value_to_string(apArg[3],&n);` |
|      3 | 4424 | `			if( n > 0 ){` |
|      3 | 4425 | `				sCsv.enclosure = z[0];` |
|      1 | 4426 | `			}` |
|      1 | 4427 | `		}` |
|      1 | 4428 | `	}` |
|      - | 4429 | `	/* Iterate throw array entries and write csv data */` |
|      3 | 4430 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      - | 4431 | `	/* Write a line ending */` |
|      - | 4432 | `#ifdef __WINNT__` |
|      1 | 4433 | `	zEol = "\r\n";` |
|      1 | 4434 | `	eolen = (int)sizeof("\r\n")-1;` |
|      - | 4435 | `#else` |
|      - | 4436 | `	/* Assume UNIX LF */` |
|      2 | 4437 | `	zEol = "\n";` |
|      2 | 4438 | `	eolen = (int)sizeof(char);` |
|      - | 4439 | `#endif` |
|      3 | 4440 | `	pDev->pStream->xWrite(pDev->pHandle,(const void *)zEol,eolen);` |
|      3 | 4441 | `	return PH7_OK;` |
|      2 | 4442 |  |
|      - | 4443 | `/*` |
|      - | 4444 | ` * fprintf,vfprintf private data.` |
|      - | 4445 | ` * An instance of the following structure is passed to the formatted` |
|      - | 4446 | ` * input consumer callback defined below.` |
|      - | 4447 | ` */` |
|      - | 4448 | `typedef struct fprintf_data fprintf_data;` |
|      - | 4449 | `struct fprintf_data` |
|      - | 4450 |  |
|      - | 4451 | `	io_private *pIO;        /* IO stream */` |
|      - | 4452 | `	ph7_int64 nCount;       /* Total number of bytes written */` |
|      - | 4453 | `};` |
|      - | 4454 | `/*` |
|      - | 4455 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 4456 | ` */` |
|     38 | 4457 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4458 |  |
|     39 | 4459 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 4460 | `	ph7_int64 n;` |
|      - | 4461 | `	/* Write the formatted data */` |
|     39 | 4462 | `	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);` |
|     39 | 4463 | `	if( n < 1 ){` |
|    ! 0 | 4464 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 4465 | `		/* IO error,abort immediately */` |
|    ! 0 | 4466 | `		return SXERR_ABORT;` |
|      - | 4467 | `	}` |
|      - | 4468 | `	/* Increment counter */` |
|     39 | 4469 | `	pFdata->nCount += n;` |
|     39 | 4470 | `	return PH7_OK;` |
|     20 | 4471 |  |
|      - | 4472 | `/*` |
|      - | 4473 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 4474 | ` *  Write a formatted string to a stream.` |
|      - | 4475 | ` * Parameters` |
|      - | 4476 | ` *  $handle` |
|      - | 4477 | ` *   The file pointer.` |
|      - | 4478 | ` *  $format` |
|      - | 4479 | ` *   String format (see sprintf()).` |
|      - | 4480 | ` * Return` |
|      - | 4481 | ` *  The length of the written string.` |
|      - | 4482 | ` */` |
|     16 | 4483 | `static int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4484 |  |
|      - | 4485 | `	fprintf_data sFdata;` |
|      - | 4486 | `	const char *zFormat;` |
|      - | 4487 | `	io_private *pDev;` |
|      - | 4488 | `	int nLen;` |
|     17 | 4489 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 4490 | `		/* Missing/Invalid arguments,return zero */` |
|    ! 0 | 4491 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4492 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4493 | `		return PH7_OK;` |
|      - | 4494 | `	}` |
|      - | 4495 | `	/* Extract our private data */` |
|     17 | 4496 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4497 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     17 | 4498 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4499 | `		/*Expecting an IO handle */` |
|    ! 0 | 4500 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4501 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4502 | `		return PH7_OK;` |
|      - | 4503 | `	}` |
|      - | 4504 | `	/* Point to the target IO stream device */` |
|     17 | 4505 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4506 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4507 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4508 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4509 | `			);` |
|    ! 0 | 4510 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4511 | `		return PH7_OK;` |
|      - | 4512 | `	}` |
|      - | 4513 | `	/* Extract the string format */` |
|     17 | 4514 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     17 | 4515 | `	if( nLen < 1 ){` |
|      - | 4516 | `		/* Empty string,return zero */` |
|    ! 0 | 4517 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4518 | `		return PH7_OK;` |
|      - | 4519 | `	}` |
|      - | 4520 | `	/* Prepare our private data */` |
|     17 | 4521 | `	sFdata.nCount = 0;` |
|     17 | 4522 | `	sFdata.pIO = pDev;` |
|      - | 4523 | `	/* Format the string */` |
|     17 | 4524 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 4525 | `	/* Return total number of bytes written */` |
|     17 | 4526 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|     17 | 4527 | `	return PH7_OK;` |
|      9 | 4528 |  |
|      - | 4529 | `/*` |
|      - | 4530 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 4531 | ` *  Write a formatted string to a stream.` |
|      - | 4532 | ` * Parameters` |
|      - | 4533 | ` *  $handle` |
|      - | 4534 | ` *   The file pointer.` |
|      - | 4535 | ` *  $format` |
|      - | 4536 | ` *   String format (see sprintf()).` |
|      - | 4537 | ` * $args` |
|      - | 4538 | ` *   User arguments.` |
|      - | 4539 | ` * Return` |
|      - | 4540 | ` *  The length of the written string.` |
|      - | 4541 | ` */` |
|      4 | 4542 | `static int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4543 |  |
|      - | 4544 | `	fprintf_data sFdata;` |
|      - | 4545 | `	const char *zFormat;` |
|      - | 4546 | `	ph7_hashmap *pMap;` |
|      - | 4547 | `	io_private *pDev;` |
|      - | 4548 | `	SySet sArg;` |
|      - | 4549 | `	int n,nLen;` |
|      5 | 4550 | `	if( nArg < 3 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_string(apArg[1])  \|\| !ph7_value_is_array(apArg[2]) ){` |
|      - | 4551 | `		/* Missing/Invalid arguments,return zero */` |
|      3 | 4552 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|      3 | 4553 | `		ph7_result_int(pCtx,0);` |
|      3 | 4554 | `		return PH7_OK;` |
|      - | 4555 | `	}` |
|      - | 4556 | `	/* Extract our private data */` |
|      3 | 4557 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4558 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4559 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4560 | `		/*Expecting an IO handle */` |
|    ! 0 | 4561 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4562 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4563 | `		return PH7_OK;` |
|      - | 4564 | `	}` |
|      - | 4565 | `	/* Point to the target IO stream device */` |
|      3 | 4566 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4567 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4568 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4569 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4570 | `			);` |
|    ! 0 | 4571 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4572 | `		return PH7_OK;` |
|      - | 4573 | `	}` |
|      - | 4574 | `	/* Extract the string format */` |
|      3 | 4575 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      3 | 4576 | `	if( nLen < 1 ){` |
|      - | 4577 | `		/* Empty string,return zero */` |
|    ! 0 | 4578 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4579 | `		return PH7_OK;` |
|      - | 4580 | `	}` |
|      - | 4581 | `	/* Point to hashmap */` |
|      3 | 4582 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 4583 | `	/* Extract arguments from the hashmap */` |
|      3 | 4584 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4585 | `	/* Prepare our private data */` |
|      3 | 4586 | `	sFdata.nCount = 0;` |
|      3 | 4587 | `	sFdata.pIO = pDev;` |
|      - | 4588 | `	/* Format the string */` |
|      3 | 4589 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 4590 | `	/* Return total number of bytes written*/` |
|      3 | 4591 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      3 | 4592 | `	SySetRelease(&sArg);` |
|      3 | 4593 | `	return PH7_OK;` |
|      3 | 4594 |  |
|      - | 4595 | `/*` |
|      - | 4596 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 4597 | ` * According to the PHP reference manual:` |
|      - | 4598 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 4599 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 4600 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 4601 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 4602 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 4603 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 4604 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 4605 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 4606 | ` *         exist, attempt to create it.` |
|      - | 4607 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 4608 | ` *          not exist, attempt to create it.` |
|      - | 4609 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 4610 | ` *         already exists,` |
|      - | 4611 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 4612 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 4613 | ` *         the underlying open(2) system call.` |
|      - | 4614 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 4615 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 4616 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 4617 | ` *          is positioned on the beginning of the file.` |
|      - | 4618 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 4619 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 4620 | ` *          be used after the lock is requested).` |
|      - | 4621 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 4622 | ` */` |
|     62 | 4623 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      1 | 4624 |  |
|     63 | 4625 | `	const char *zEnd = &zMode[nLen];` |
|     63 | 4626 | `	int iFlag = 0;` |
|      - | 4627 | `	int c;` |
|     63 | 4628 | `	if( nLen < 1 ){` |
|      - | 4629 | `		/* Open in a read-only mode */` |
|    ! 0 | 4630 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 4631 | `	}` |
|     63 | 4632 | `	c = zMode[0];` |
|     63 | 4633 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 4634 | `		/* Read-only access */` |
|     37 | 4635 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|     37 | 4636 | `		zMode++; /* Advance */` |
|     37 | 4637 | `		if( zMode < zEnd ){` |
|      7 | 4638 | `			c = zMode[0];` |
|      7 | 4639 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 4640 | `				/* Read+Write access */` |
|      7 | 4641 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|      3 | 4642 | `			}` |
|      4 | 4643 | `		}` |
|     45 | 4644 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 4645 | `		/* Overwrite mode.` |
|      - | 4646 | `		 * If the file does not exists,try to create it` |
|      - | 4647 | `		 */` |
|     27 | 4648 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     27 | 4649 | `		zMode++; /* Advance */` |
|     27 | 4650 | `		if( zMode < zEnd ){` |
|      3 | 4651 | `			c = zMode[0];` |
|      3 | 4652 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4653 | `				/* Read+Write access */` |
|      3 | 4654 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|      3 | 4655 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|      1 | 4656 | `			}` |
|      2 | 4657 | `		}` |
|     13 | 4658 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 4659 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 4660 | `		 * Create the file if it does not exists.` |
|      - | 4661 | `		 */` |
|    ! 0 | 4662 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4663 | `		zMode++; /* Advance */` |
|    ! 0 | 4664 | `		if( zMode < zEnd ){` |
|    ! 0 | 4665 | `			c = zMode[0];` |
|    ! 0 | 4666 | `			if( c == '+' ){` |
|      - | 4667 | `				/* Read-Write access */` |
|    ! 0 | 4668 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4669 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4670 | `			}` |
|    ! 0 | 4671 | `		}` |
|    ! 0 | 4672 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 4673 | `		/* Exclusive access.` |
|      - | 4674 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 4675 | `		 * Otherwise create a new file.` |
|      - | 4676 | `		 */` |
|    ! 0 | 4677 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|    ! 0 | 4678 | `		zMode++; /* Advance */` |
|    ! 0 | 4679 | `		if( zMode < zEnd ){` |
|    ! 0 | 4680 | `			c = zMode[0];` |
|    ! 0 | 4681 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4682 | `				/* Read-Write access */` |
|    ! 0 | 4683 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4684 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4685 | `			}` |
|    ! 0 | 4686 | `		}` |
|    ! 0 | 4687 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 4688 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 4689 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4690 | `		zMode++; /* Advance */` |
|    ! 0 | 4691 | `		if( zMode < zEnd ){` |
|    ! 0 | 4692 | `			c = zMode[0];` |
|    ! 0 | 4693 | `			if( c == '+' ){` |
|      - | 4694 | `				/* Read-Write access */` |
|    ! 0 | 4695 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4696 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4697 | `			}` |
|    ! 0 | 4698 | `		}` |
|    ! 0 | 4699 | `	}else{` |
|      - | 4700 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 4701 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 4702 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 4703 | `	}` |
|     71 | 4704 | `	while( zMode < zEnd ){` |
|      9 | 4705 | `		c = zMode[0];` |
|      9 | 4706 | `		if( c == 'b' \|\| c == 'B' ){` |
|    ! 0 | 4707 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4708 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|      9 | 4709 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 4710 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 4711 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4712 | `		}` |
|      9 | 4713 | `		zMode++;` |
|      1 | 4714 | `	}` |
|     63 | 4715 | `	return iFlag;` |
|     32 | 4716 |  |
|      - | 4717 | `/*` |
|      - | 4718 | ` * Initialize the IO private structure.` |
|      - | 4719 | ` */` |
|   2832 | 4720 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      2 | 4721 |  |
|   2834 | 4722 | `	pOut->pStream = pStream;` |
|   2834 | 4723 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   2834 | 4724 | `	pOut->nOfft = 0;` |
|      - | 4725 | `	/* Set the magic number */` |
|   2834 | 4726 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   2834 | 4727 |  |
|      - | 4728 | `/*` |
|      - | 4729 | ` * Release the IO private structure.` |
|      - | 4730 | ` */` |
|   2824 | 4731 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      2 | 4732 |  |
|   2826 | 4733 | `	SyBlobRelease(&pDev->sBuffer);` |
|   2826 | 4734 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 4735 | `	/* Release the whole structure */` |
|   2826 | 4736 | `	ph7_context_free_chunk(pCtx,pDev);` |
|   2826 | 4737 |  |
|      - | 4738 | `/*` |
|      - | 4739 | ` * Reset the IO private structure.` |
|      - | 4740 | ` */` |
|     12 | 4741 | `static void ResetIOPrivate(io_private *pDev)` |
|      1 | 4742 |  |
|     13 | 4743 | `	SyBlobReset(&pDev->sBuffer);` |
|     13 | 4744 | `	pDev->nOfft = 0;` |
|     13 | 4745 |  |
|      - | 4746 | `/* Forward declaration */` |
|      - | 4747 | `static int is_php_stream(const ph7_io_stream *pStream);` |
|      - | 4748 | `/*` |
|      - | 4749 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 4750 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 4751 | ` * Parameters` |
|      - | 4752 | ` *  $filename` |
|      - | 4753 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 4754 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 4755 | ` *   then a regular file is assumed.` |
|      - | 4756 | ` *  $mode` |
|      - | 4757 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 4758 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 4759 | ` *   modes.` |
|      - | 4760 | ` *  $use_include_path` |
|      - | 4761 | ` *   You can use the optional second parameter and set it to` |
|      - | 4762 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 4763 | ` *  $context` |
|      - | 4764 | ` *   A context stream resource.` |
|      - | 4765 | ` * Return` |
|      - | 4766 | ` *  File handle on success or FALSE on failure.` |
|      - | 4767 | ` */` |
|     62 | 4768 | `static int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4769 |  |
|      - | 4770 | `	const ph7_io_stream *pStream;` |
|      - | 4771 | `	const char *zUri,*zMode;` |
|      - | 4772 | `	ph7_value *pResource;` |
|      - | 4773 | `	io_private *pDev;` |
|      - | 4774 | `	int iLen,imLen;` |
|      - | 4775 | `	int iOpenFlags;` |
|     63 | 4776 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4777 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4778 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 4779 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4780 | `		return PH7_OK;` |
|      - | 4781 | `	}` |
|      - | 4782 | `	/* Extract the URI and the desired access mode */` |
|     63 | 4783 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|     63 | 4784 | `	if( nArg > 1 ){` |
|     63 | 4785 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|     32 | 4786 | `	}else{` |
|      - | 4787 | `		/* Set a default read-only mode */` |
|    ! 0 | 4788 | `		zMode = "r";` |
|    ! 0 | 4789 | `		imLen = (int)sizeof(char);` |
|      - | 4790 | `	}` |
|      - | 4791 | `	/* Try to extract a stream */` |
|     63 | 4792 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|     63 | 4793 | `	if( pStream == 0 ){` |
|    ! 0 | 4794 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 4795 | `			"No stream device is associated with the given URI(%s)",zUri);` |
|    ! 0 | 4796 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4797 | `		return PH7_OK;` |
|      - | 4798 | `	}` |
|      - | 4799 | `	/* Allocate a new IO private instance */` |
|     63 | 4800 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     63 | 4801 | `	if( pDev == 0 ){` |
|    ! 0 | 4802 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4803 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4804 | `		return PH7_OK;` |
|      - | 4805 | `	}` |
|     63 | 4806 | `	pResource = 0;` |
|     63 | 4807 | `	if( nArg > 3 ){` |
|    ! 0 | 4808 | `		pResource = apArg[3];` |
|     63 | 4809 | `	}else if( is_php_stream(pStream) ){` |
|      - | 4810 | `		/* TICKET 1433-80: The php:// stream need a ph7_value to access the underlying` |
|      - | 4811 | `		 * virtual machine.` |
|      - | 4812 | `		 */` |
|      3 | 4813 | `		pResource = apArg[0];` |
|      1 | 4814 | `	}` |
|      - | 4815 | `	/* Initialize the structure */` |
|     63 | 4816 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 4817 | `	/* Convert open mode to PH7 flags */` |
|     63 | 4818 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|      - | 4819 | `	/* Try to get a handle */` |
|     94 | 4820 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|     31 | 4821 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);` |
|     63 | 4822 | `	if( pDev->pHandle == 0 ){` |
|    ! 0 | 4823 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zUri);` |
|    ! 0 | 4824 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4825 | `		ph7_context_free_chunk(pCtx,pDev);` |
|    ! 0 | 4826 | `		return PH7_OK;` |
|      - | 4827 | `	}` |
|      - | 4828 | `	/* All done,return the io_private instance as a resource */` |
|     63 | 4829 | `	ph7_result_resource(pCtx,pDev);` |
|     63 | 4830 | `	return PH7_OK;` |
|     32 | 4831 |  |
|      - | 4832 | `/*` |
|      - | 4833 | ` * bool fclose(resource $handle)` |
|      - | 4834 | ` *  Closes an open file pointer` |
|      - | 4835 | ` * Parameters` |
|      - | 4836 | ` *  $handle` |
|      - | 4837 | ` *   The file pointer.` |
|      - | 4838 | ` * Return` |
|      - | 4839 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4840 | ` */` |
|     76 | 4841 | `static int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4842 |  |
|      - | 4843 | `	const ph7_io_stream *pStream;` |
|      - | 4844 | `	io_private *pDev;` |
|      - | 4845 | `	ph7_vm *pVm;` |
|     78 | 4846 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4847 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4848 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4849 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4850 | `		return PH7_OK;` |
|      - | 4851 | `	}` |
|      - | 4852 | `	/* Extract our private data */` |
|     78 | 4853 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4854 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     78 | 4855 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4856 | `		/*Expecting an IO handle */` |
|    ! 0 | 4857 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4858 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4859 | `		return PH7_OK;` |
|      - | 4860 | `	}` |
|      - | 4861 | `	/* Point to the target IO stream device */` |
|     78 | 4862 | `	pStream = pDev->pStream;` |
|     78 | 4863 | `	if( pStream == 0 ){` |
|    ! 0 | 4864 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4865 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4866 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4867 | `			);` |
|    ! 0 | 4868 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4869 | `		return PH7_OK;` |
|      - | 4870 | `	}` |
|      - | 4871 | `	/* Point to the VM that own this context */` |
|     78 | 4872 | `	pVm = pCtx->pVm;` |
|      - | 4873 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|     78 | 4874 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 4875 | `		/* Perform the requested operation */` |
|     78 | 4876 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 4877 | `		/* Release the IO private structure */` |
|     78 | 4878 | `		ReleaseIOPrivate(pCtx,pDev);` |
|      - | 4879 | `		/* Invalidate the resource handle */` |
|     78 | 4880 | `		ph7_value_release(apArg[0]);` |
|     38 | 4881 | `	}` |
|      - | 4882 | `	/* Return TRUE */` |
|     78 | 4883 | `	ph7_result_bool(pCtx,1);` |
|     78 | 4884 | `	return PH7_OK;` |
|     40 | 4885 |  |
|      - | 4886 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4887 | `/*` |
|      - | 4888 | ` * MD5/SHA1 digest consumer.` |
|      - | 4889 | ` */` |
|     72 | 4890 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 4891 |  |
|      - | 4892 | `	/* Append hex chunk verbatim */` |
|     73 | 4893 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     73 | 4894 | `	return SXRET_OK;` |
|      1 | 4895 |  |
|      - | 4896 | `/*` |
|      - | 4897 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 4898 | ` *  Calculates the md5 hash of a given file.` |
|      - | 4899 | ` * Parameters` |
|      - | 4900 | ` *  $uri` |
|      - | 4901 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 4902 | ` *  $raw_output` |
|      - | 4903 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 4904 | ` * Return` |
|      - | 4905 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 4906 | ` */` |
|      2 | 4907 | `static int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4908 |  |
|      - | 4909 | `	const ph7_io_stream *pStream;` |
|      - | 4910 | `	unsigned char zDigest[16];` |
|      3 | 4911 | `	int raw_output  = FALSE;` |
|      - | 4912 | `	const char *zFile;` |
|      - | 4913 | `	MD5Context sCtx;` |
|      - | 4914 | `	char zBuf[8192];` |
|      - | 4915 | `	void *pHandle;` |
|      - | 4916 | `	ph7_int64 n;` |
|      - | 4917 | `	int nLen;` |
|      3 | 4918 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4919 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4920 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 4921 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4922 | `		return PH7_OK;` |
|      - | 4923 | `	}` |
|      - | 4924 | `	/* Extract the file path */` |
|      3 | 4925 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4926 | `	/* Point to the target IO stream device */` |
|      3 | 4927 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4928 | `	if( pStream == 0 ){` |
|    ! 0 | 4929 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4930 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4931 | `		return PH7_OK;` |
|      - | 4932 | `	}` |
|      3 | 4933 | `	if( nArg > 1 ){` |
|    ! 0 | 4934 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 4935 | `	}` |
|      - | 4936 | `	/* Try to open the file in read-only mode */` |
|      3 | 4937 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 4938 | `	if( pHandle == 0 ){` |
|    ! 0 | 4939 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 4940 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4941 | `		return PH7_OK;` |
|      - | 4942 | `	}` |
|      - | 4943 | `	/* Init the MD5 context */` |
|      3 | 4944 | `	MD5Init(&sCtx);` |
|      - | 4945 | `	/* Perform the requested operation */` |
|      2 | 4946 | `	for(;;){` |
|      5 | 4947 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 4948 | `		if( n < 1 ){` |
|      - | 4949 | `			/* EOF or IO error,break immediately */` |
|      3 | 4950 | `			break;` |
|      - | 4951 | `		}` |
|      3 | 4952 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 4953 | `	}` |
|      - | 4954 | `	/* Close the stream */` |
|      3 | 4955 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 4956 | `	/* Extract the digest */` |
|      3 | 4957 | `	MD5Final(zDigest,&sCtx);` |
|      3 | 4958 | `	if( raw_output ){` |
|      - | 4959 | `		/* Output raw digest */` |
|    ! 0 | 4960 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 4961 | `	}else{` |
|      - | 4962 | `		/* Perform a binary to hex conversion */` |
|      3 | 4963 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 4964 | `	}` |
|      3 | 4965 | `	return PH7_OK;` |
|      2 | 4966 |  |
|      - | 4967 | `/*` |
|      - | 4968 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 4969 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 4970 | ` * Parameters` |
|      - | 4971 | ` *  $uri` |
|      - | 4972 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 4973 | ` *  $raw_output` |
|      - | 4974 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 4975 | ` * Return` |
|      - | 4976 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 4977 | ` */` |
|      2 | 4978 | `static int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4979 |  |
|      - | 4980 | `	const ph7_io_stream *pStream;` |
|      - | 4981 | `	unsigned char zDigest[20];` |
|      3 | 4982 | `	int raw_output  = FALSE;` |
|      - | 4983 | `	const char *zFile;` |
|      - | 4984 | `	SHA1Context sCtx;` |
|      - | 4985 | `	char zBuf[8192];` |
|      - | 4986 | `	void *pHandle;` |
|      - | 4987 | `	ph7_int64 n;` |
|      - | 4988 | `	int nLen;` |
|      3 | 4989 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4990 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4991 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 4992 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4993 | `		return PH7_OK;` |
|      - | 4994 | `	}` |
|      - | 4995 | `	/* Extract the file path */` |
|      3 | 4996 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4997 | `	/* Point to the target IO stream device */` |
|      3 | 4998 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4999 | `	if( pStream == 0 ){` |
|    ! 0 | 5000 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5001 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5002 | `		return PH7_OK;` |
|      - | 5003 | `	}` |
|      3 | 5004 | `	if( nArg > 1 ){` |
|    ! 0 | 5005 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 5006 | `	}` |
|      - | 5007 | `	/* Try to open the file in read-only mode */` |
|      3 | 5008 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5009 | `	if( pHandle == 0 ){` |
|    ! 0 | 5010 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5011 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5012 | `		return PH7_OK;` |
|      - | 5013 | `	}` |
|      - | 5014 | `	/* Init the SHA1 context */` |
|      3 | 5015 | `	SHA1Init(&sCtx);` |
|      - | 5016 | `	/* Perform the requested operation */` |
|      2 | 5017 | `	for(;;){` |
|      5 | 5018 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5019 | `		if( n < 1 ){` |
|      - | 5020 | `			/* EOF or IO error,break immediately */` |
|      3 | 5021 | `			break;` |
|      - | 5022 | `		}` |
|      3 | 5023 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5024 | `	}` |
|      - | 5025 | `	/* Close the stream */` |
|      3 | 5026 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5027 | `	/* Extract the digest */` |
|      3 | 5028 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 5029 | `	if( raw_output ){` |
|      - | 5030 | `		/* Output raw digest */` |
|    ! 0 | 5031 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5032 | `	}else{` |
|      - | 5033 | `		/* Perform a binary to hex conversion */` |
|      3 | 5034 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5035 | `	}` |
|      3 | 5036 | `	return PH7_OK;` |
|      2 | 5037 |  |
|      - | 5038 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5039 | `/*` |
|      - | 5040 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 5041 | ` *  Parse a configuration file.` |
|      - | 5042 | ` * Parameters` |
|      - | 5043 | ` * $filename` |
|      - | 5044 | ` *  The filename of the ini file being parsed.` |
|      - | 5045 | ` * $process_sections` |
|      - | 5046 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 5047 | ` *  with the section names and settings included.` |
|      - | 5048 | ` *  The default for process_sections is FALSE.` |
|      - | 5049 | ` * $scanner_mode` |
|      - | 5050 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 5051 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 5052 | ` * Return` |
|      - | 5053 | ` *  The settings are returned as an associative array on success.` |
|      - | 5054 | ` *  Otherwise is returned.` |
|      - | 5055 | ` */` |
|      2 | 5056 | `static int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5057 |  |
|      - | 5058 | `	const ph7_io_stream *pStream;` |
|      - | 5059 | `	const char *zFile;` |
|      - | 5060 | `	SyBlob sContents;` |
|      - | 5061 | `	void *pHandle;` |
|      - | 5062 | `	int nLen;` |
|      3 | 5063 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5064 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5065 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5066 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5067 | `		return PH7_OK;` |
|      - | 5068 | `	}` |
|      - | 5069 | `	/* Extract the file path */` |
|      3 | 5070 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5071 | `	/* Point to the target IO stream device */` |
|      3 | 5072 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5073 | `	if( pStream == 0 ){` |
|    ! 0 | 5074 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5075 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5076 | `		return PH7_OK;` |
|      - | 5077 | `	}` |
|      - | 5078 | `	/* Try to open the file in read-only mode */` |
|      3 | 5079 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5080 | `	if( pHandle == 0 ){` |
|    ! 0 | 5081 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5082 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5083 | `		return PH7_OK;` |
|      - | 5084 | `	}` |
|      3 | 5085 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 5086 | `	/* Read the whole file */` |
|      3 | 5087 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      3 | 5088 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 5089 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 5090 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5091 | `	}else{` |
|      - | 5092 | `		/* Process the raw INI buffer */` |
|      5 | 5093 | `		PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      2 | 5094 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0);` |
|      - | 5095 | `	}` |
|      - | 5096 | `	/* Close the stream */` |
|      3 | 5097 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5098 | `	/* Release the working buffer */` |
|      3 | 5099 | `	SyBlobRelease(&sContents);` |
|      3 | 5100 | `	return PH7_OK;` |
|      2 | 5101 |  |
|      - | 5102 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 5103 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 5104 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 5105 | `#if defined(_MSC_VER)` |
|      - | 5106 | `static const ph7_vfs null_vfs = {` |
|      - | 5107 | `#else` |
|      - | 5108 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 5109 | `#endif` |
|      - | 5110 | `	"null_vfs",` |
|      - | 5111 | `	PH7_VFS_VERSION,` |
|      - | 5112 |  |
|      - | 5113 |  |
|      - | 5114 |  |
|      - | 5115 |  |
|      - | 5116 |  |
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
|      - | 5153 | `};` |
|      - | 5154 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 5155 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 5156 | `#ifdef __WINNT__` |
|      - | 5157 | `/*` |
|      - | 5158 | ` * Windows VFS implementation for the PH7 engine.` |
|      - | 5159 | ` * Status:` |
|      - | 5160 | ` *    Stable.` |
|      - | 5161 | ` */` |
|      - | 5162 | `/* What follows here is code that is specific to windows systems. */` |
|      - | 5163 | `#include <Windows.h>` |
|      - | 5164 | `#include <stdio.h> /* For popen/pclose pipe stream support */` |
|      - | 5165 | `#include <io.h>    /* For _open_osfhandle, _close */` |
|      - | 5166 | `#include <fcntl.h> /* For _O_RDONLY, _O_WRONLY, _O_TEXT */` |
|      - | 5167 | `/*` |
|      - | 5168 | `** Convert a UTF-8 string to microsoft unicode (UTF-16?).` |
|      - | 5169 | `**` |
|      - | 5170 | `** Space to hold the returned string is obtained from HeapAlloc().` |
|      - | 5171 | `** Taken from the sqlite3 source tree` |
|      - | 5172 | `** status: Public Domain` |
|      - | 5173 | `*/` |
|      2 | 5174 | `static WCHAR *utf8ToUnicode(const char *zFilename){` |
|      - | 5175 | `  int nChar;` |
|      - | 5176 | `  WCHAR *zWideFilename;` |
|      - | 5177 |  |
|      2 | 5178 | `  nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, 0, 0);` |
|      2 | 5179 | `  zWideFilename = (WCHAR *)HeapAlloc(GetProcessHeap(),0,nChar*sizeof(zWideFilename[0]));` |
|      2 | 5180 | `  if( zWideFilename == 0 ){` |
|    ! 0 | 5181 | ` 	return 0;` |
|      - | 5182 | `  }` |
|      2 | 5183 | `  nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, zWideFilename, nChar);` |
|      2 | 5184 | `  if( nChar==0 ){` |
|    ! 0 | 5185 | `    HeapFree(GetProcessHeap(),0,zWideFilename);` |
|    ! 0 | 5186 | `    return 0;` |
|      - | 5187 | `  }` |
|      2 | 5188 | `  return zWideFilename;` |
|      2 | 5189 |  |
|      - | 5190 | `/*` |
|      - | 5191 | `** Convert a UTF-8 filename into whatever form the underlying` |
|      - | 5192 | `** operating system wants filenames in.Space to hold the result` |
|      - | 5193 | `** is obtained from HeapAlloc() and must be freed by the calling` |
|      - | 5194 | `** function.` |
|      - | 5195 | `** Taken from the sqlite3 source tree` |
|      - | 5196 | `** status: Public Domain` |
|      - | 5197 | `*/` |
|      2 | 5198 | `static void *convertUtf8Filename(const char *zFilename){` |
|      - | 5199 | `  void *zConverted;` |
|      2 | 5200 | `  zConverted = utf8ToUnicode(zFilename);` |
|      2 | 5201 | `  return zConverted;` |
|      2 | 5202 |  |
|      - | 5203 | `/*` |
|      - | 5204 | `** Convert microsoft unicode to UTF-8.  Space to hold the returned string is` |
|      - | 5205 | `** obtained from HeapAlloc().` |
|      - | 5206 | `** Taken from the sqlite3 source tree` |
|      - | 5207 | `** status: Public Domain` |
|      - | 5208 | `*/` |
|      2 | 5209 | `static char *unicodeToUtf8(const WCHAR *zWideFilename){` |
|      - | 5210 | `  char *zFilename;` |
|      - | 5211 | `  int nByte;` |
|      - | 5212 |  |
|      2 | 5213 | `  nByte = WideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, 0, 0, 0, 0);` |
|      2 | 5214 | `  zFilename = (char *)HeapAlloc(GetProcessHeap(),0,nByte);` |
|      2 | 5215 | `  if( zFilename == 0 ){` |
|    ! 0 | 5216 | `  	return 0;` |
|      - | 5217 | `  }` |
|      2 | 5218 | `  nByte = WideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, zFilename, nByte,0, 0);` |
|      2 | 5219 | `  if( nByte == 0 ){` |
|    ! 0 | 5220 | `    HeapFree(GetProcessHeap(),0,zFilename);` |
|    ! 0 | 5221 | `    return 0;` |
|      - | 5222 | `  }` |
|      2 | 5223 | `  return zFilename;` |
|      2 | 5224 |  |
|      - | 5225 | `/* int (*xchdir)(const char *) */` |
|      - | 5226 | `static int WinVfs_chdir(const char *zPath)` |
|      2 | 5227 |  |
|      - | 5228 | `	void * pConverted;` |
|      - | 5229 | `	BOOL rc;` |
|      2 | 5230 | `	pConverted = convertUtf8Filename(zPath);` |
|      2 | 5231 | `	if( pConverted == 0 ){` |
|    ! 0 | 5232 | `		return -1;` |
|      - | 5233 | `	}` |
|      2 | 5234 | `	rc = SetCurrentDirectoryW((LPCWSTR)pConverted);` |
|      2 | 5235 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      2 | 5236 | `	return rc ? PH7_OK : -1;` |
|      2 | 5237 |  |
|      - | 5238 | `/* int (*xGetcwd)(ph7_context *) */` |
|      - | 5239 | `static int WinVfs_getcwd(ph7_context *pCtx)` |
|      2 | 5240 |  |
|      - | 5241 | `	WCHAR zDir[2048];` |
|      - | 5242 | `	char *zConverted;` |
|      - | 5243 | `	DWORD rc;` |
|      - | 5244 | `	/* Get the current directory */` |
|      2 | 5245 | `	rc = GetCurrentDirectoryW(sizeof(zDir),zDir);` |
|      2 | 5246 | `	if( rc < 1 ){` |
|    ! 0 | 5247 | `		return -1;` |
|      - | 5248 | `	}` |
|      2 | 5249 | `	zConverted = unicodeToUtf8(zDir);` |
|      2 | 5250 | `	if( zConverted == 0 ){` |
|    ! 0 | 5251 | `		return -1;` |
|      - | 5252 | `	}` |
|      2 | 5253 | `	ph7_result_string(pCtx,zConverted,-1/*Compute length automatically*/); /* Will make it's own copy */` |
|      2 | 5254 | `	HeapFree(GetProcessHeap(),0,zConverted);` |
|      2 | 5255 | `	return PH7_OK;` |
|      2 | 5256 |  |
|      - | 5257 | `/* int (*xMkdir)(const char *,int,int) */` |
|      - | 5258 | `static int WinVfs_mkdir(const char *zPath,int mode,int recursive)` |
|      1 | 5259 |  |
|      - | 5260 | `	void * pConverted;` |
|      - | 5261 | `	BOOL rc;` |
|      1 | 5262 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5263 | `	if( pConverted == 0 ){` |
|    ! 0 | 5264 | `		return -1;` |
|      - | 5265 | `	}` |
|      1 | 5266 | `	mode= 0; /* MSVC warning */` |
|      1 | 5267 | `	recursive = 0;` |
|      1 | 5268 | `	rc = CreateDirectoryW((LPCWSTR)pConverted,0);` |
|      1 | 5269 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5270 | `	return rc ? PH7_OK : -1;` |
|      1 | 5271 |  |
|      - | 5272 | `/* int (*xRmdir)(const char *) */` |
|      - | 5273 | `static int WinVfs_rmdir(const char *zPath)` |
|      1 | 5274 |  |
|      - | 5275 | `	void * pConverted;` |
|      - | 5276 | `	BOOL rc;` |
|      1 | 5277 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5278 | `	if( pConverted == 0 ){` |
|    ! 0 | 5279 | `		return -1;` |
|      - | 5280 | `	}` |
|      1 | 5281 | `	rc = RemoveDirectoryW((LPCWSTR)pConverted);` |
|      1 | 5282 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5283 | `	return rc ? PH7_OK : -1;` |
|      1 | 5284 |  |
|      - | 5285 | `/* int (*xIsdir)(const char *) */` |
|      - | 5286 | `static int WinVfs_isdir(const char *zPath)` |
|      2 | 5287 |  |
|      - | 5288 | `	void * pConverted;` |
|      - | 5289 | `	DWORD dwAttr;` |
|      2 | 5290 | `	pConverted = convertUtf8Filename(zPath);` |
|      2 | 5291 | `	if( pConverted == 0 ){` |
|    ! 0 | 5292 | `		return -1;` |
|      - | 5293 | `	}` |
|      2 | 5294 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|      2 | 5295 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      2 | 5296 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|      1 | 5297 | `		return -1;` |
|      - | 5298 | `	}` |
|      2 | 5299 | `	return (dwAttr & FILE_ATTRIBUTE_DIRECTORY) ? PH7_OK : -1;` |
|      2 | 5300 |  |
|      - | 5301 | `/* int (*xRename)(const char *,const char *) */` |
|      - | 5302 | `static int WinVfs_Rename(const char *zOld,const char *zNew)` |
|      1 | 5303 |  |
|      - | 5304 | `	void *pOld,*pNew;` |
|      1 | 5305 | `	BOOL rc = 0;` |
|      1 | 5306 | `	pOld = convertUtf8Filename(zOld);` |
|      1 | 5307 | `	if( pOld == 0 ){` |
|    ! 0 | 5308 | `		return -1;` |
|      - | 5309 | `	}` |
|      1 | 5310 | `	pNew = convertUtf8Filename(zNew);` |
|      1 | 5311 | `	if( pNew  ){` |
|      1 | 5312 | `		rc = MoveFileW((LPCWSTR)pOld,(LPCWSTR)pNew);` |
|      - | 5313 | `	}` |
|      1 | 5314 | `	HeapFree(GetProcessHeap(),0,pOld);` |
|      1 | 5315 | `	if( pNew ){` |
|      1 | 5316 | `		HeapFree(GetProcessHeap(),0,pNew);` |
|      - | 5317 | `	}` |
|      1 | 5318 | `	return rc ? PH7_OK : - 1;` |
|      1 | 5319 |  |
|      - | 5320 | `/* int (*xRealpath)(const char *,ph7_context *) */` |
|      - | 5321 | `static int WinVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|      1 | 5322 |  |
|      - | 5323 | `	WCHAR zTemp[2048];` |
|      - | 5324 | `	void *pPath;` |
|      - | 5325 | `	char *zReal;` |
|      - | 5326 | `	DWORD n;` |
|      1 | 5327 | `	pPath = convertUtf8Filename(zPath);` |
|      1 | 5328 | `	if( pPath == 0 ){` |
|    ! 0 | 5329 | `		return -1;` |
|      - | 5330 | `	}` |
|      1 | 5331 | `	n = GetFullPathNameW((LPCWSTR)pPath,0,0,0);` |
|      1 | 5332 | `	if( n > 0 ){` |
|      1 | 5333 | `		if( n >= sizeof(zTemp) ){` |
|    ! 0 | 5334 | `			n = sizeof(zTemp) - 1;` |
|      - | 5335 | `		}` |
|      1 | 5336 | `		GetFullPathNameW((LPCWSTR)pPath,n,zTemp,0);` |
|      - | 5337 | `	}` |
|      1 | 5338 | `	HeapFree(GetProcessHeap(),0,pPath);` |
|      1 | 5339 | `	if( !n ){` |
|    ! 0 | 5340 | `		return -1;` |
|      - | 5341 | `	}` |
|      1 | 5342 | `	zReal = unicodeToUtf8(zTemp);` |
|      1 | 5343 | `	if( zReal == 0 ){` |
|    ! 0 | 5344 | `		return -1;` |
|      - | 5345 | `	}` |
|      1 | 5346 | `	ph7_result_string(pCtx,zReal,-1); /* Will make it's own copy */` |
|      1 | 5347 | `	HeapFree(GetProcessHeap(),0,zReal);` |
|      1 | 5348 | `	return PH7_OK;` |
|      1 | 5349 |  |
|      - | 5350 | `/* int (*xSleep)(unsigned int) */` |
|      - | 5351 | `static int WinVfs_Sleep(unsigned int uSec)` |
|      1 | 5352 |  |
|      1 | 5353 | `	Sleep(uSec/1000/*uSec per Millisec */);` |
|      1 | 5354 | `	return PH7_OK;` |
|      1 | 5355 |  |
|      - | 5356 | `/* int (*xUnlink)(const char *) */` |
|      - | 5357 | `static int WinVfs_unlink(const char *zPath)` |
|      2 | 5358 |  |
|      - | 5359 | `	void * pConverted;` |
|      - | 5360 | `	BOOL rc;` |
|      2 | 5361 | `	pConverted = convertUtf8Filename(zPath);` |
|      2 | 5362 | `	if( pConverted == 0 ){` |
|    ! 0 | 5363 | `		return -1;` |
|      - | 5364 | `	}` |
|      2 | 5365 | `	rc = DeleteFileW((LPCWSTR)pConverted);` |
|      2 | 5366 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      2 | 5367 | `	return rc ? PH7_OK : - 1;` |
|      2 | 5368 |  |
|      - | 5369 | `/* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 5370 | `static ph7_int64 WinVfs_DiskFreeSpace(const char *zPath)` |
|    ! 0 | 5371 |  |
|      - | 5372 | `#ifdef _WIN32_WCE` |
|      - | 5373 | `	/* GetDiskFreeSpace is not supported under WINCE */` |
|      - | 5374 | `	SXUNUSED(zPath);` |
|      - | 5375 | `	return 0;` |
|      - | 5376 | `#else` |
|      - | 5377 | `	DWORD dwSectPerClust,dwBytesPerSect,dwFreeClusters,dwTotalClusters;` |
|      - | 5378 | `	void * pConverted;` |
|      - | 5379 | `	WCHAR *p;` |
|      - | 5380 | `	BOOL rc;` |
|    ! 0 | 5381 | `	pConverted = convertUtf8Filename(zPath);` |
|    ! 0 | 5382 | `	if( pConverted == 0 ){` |
|    ! 0 | 5383 | `		return 0;` |
|      - | 5384 | `	}` |
|    ! 0 | 5385 | `	p = (WCHAR *)pConverted;` |
|    ! 0 | 5386 | `	for(;*p;p++){` |
|    ! 0 | 5387 | `		if( *p == '\\' \|\| *p == '/'){` |
|    ! 0 | 5388 | `			*p = '\0';` |
|    ! 0 | 5389 | `			break;` |
|      - | 5390 | `		}` |
|    ! 0 | 5391 | `	}` |
|    ! 0 | 5392 | `	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted,&dwSectPerClust,&dwBytesPerSect,&dwFreeClusters,&dwTotalClusters);` |
|    ! 0 | 5393 | `	if( !rc ){` |
|    ! 0 | 5394 | `		return 0;` |
|      - | 5395 | `	}` |
|    ! 0 | 5396 | `	return (ph7_int64)dwFreeClusters * dwSectPerClust * dwBytesPerSect;` |
|      - | 5397 | `#endif` |
|    ! 0 | 5398 |  |
|      - | 5399 | `/* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 5400 | `static ph7_int64 WinVfs_DiskTotalSpace(const char *zPath)` |
|    ! 0 | 5401 |  |
|      - | 5402 | `#ifdef _WIN32_WCE` |
|      - | 5403 | `	/* GetDiskFreeSpace is not supported under WINCE */` |
|      - | 5404 | `	SXUNUSED(zPath);` |
|      - | 5405 | `	return 0;` |
|      - | 5406 | `#else` |
|      - | 5407 | `	DWORD dwSectPerClust,dwBytesPerSect,dwFreeClusters,dwTotalClusters;` |
|      - | 5408 | `	void * pConverted;` |
|      - | 5409 | `	WCHAR *p;` |
|      - | 5410 | `	BOOL rc;` |
|    ! 0 | 5411 | `	pConverted = convertUtf8Filename(zPath);` |
|    ! 0 | 5412 | `	if( pConverted == 0 ){` |
|    ! 0 | 5413 | `		return 0;` |
|      - | 5414 | `	}` |
|    ! 0 | 5415 | `	p = (WCHAR *)pConverted;` |
|    ! 0 | 5416 | `	for(;*p;p++){` |
|    ! 0 | 5417 | `		if( *p == '\\' \|\| *p == '/'){` |
|    ! 0 | 5418 | `			*p = '\0';` |
|    ! 0 | 5419 | `			break;` |
|      - | 5420 | `		}` |
|    ! 0 | 5421 | `	}` |
|    ! 0 | 5422 | `	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted,&dwSectPerClust,&dwBytesPerSect,&dwFreeClusters,&dwTotalClusters);` |
|    ! 0 | 5423 | `	if( !rc ){` |
|    ! 0 | 5424 | `		return 0;` |
|      - | 5425 | `	}` |
|    ! 0 | 5426 | `	return (ph7_int64)dwTotalClusters * dwSectPerClust * dwBytesPerSect;` |
|      - | 5427 | `#endif` |
|    ! 0 | 5428 |  |
|      - | 5429 | `/* int (*xFileExists)(const char *) */` |
|      - | 5430 | `static int WinVfs_FileExists(const char *zPath)` |
|      1 | 5431 |  |
|      - | 5432 | `	void * pConverted;` |
|      - | 5433 | `	DWORD dwAttr;` |
|      1 | 5434 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5435 | `	if( pConverted == 0 ){` |
|    ! 0 | 5436 | `		return -1;` |
|      - | 5437 | `	}` |
|      1 | 5438 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|      1 | 5439 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5440 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|      1 | 5441 | `		return -1;` |
|      - | 5442 | `	}` |
|      1 | 5443 | `	return PH7_OK;` |
|      1 | 5444 |  |
|      - | 5445 | `/* Open a file in a read-only mode */` |
|      - | 5446 | `static HANDLE OpenReadOnly(LPCWSTR pPath)` |
|      2 | 5447 |  |
|      2 | 5448 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|      2 | 5449 | `	DWORD dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|      2 | 5450 | `	DWORD dwAccess = GENERIC_READ;` |
|      2 | 5451 | `	DWORD dwCreate = OPEN_EXISTING;` |
|      - | 5452 | `	HANDLE pHandle;` |
|      2 | 5453 | `	pHandle = CreateFileW(pPath,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|      2 | 5454 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|      1 | 5455 | `		return 0;` |
|      - | 5456 | `	}` |
|      2 | 5457 | `	return pHandle;` |
|      2 | 5458 |  |
|      - | 5459 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 5460 | `static ph7_int64 WinVfs_FileSize(const char *zPath)` |
|      1 | 5461 |  |
|      - | 5462 | `	DWORD dwLow,dwHigh;` |
|      - | 5463 | `	void * pConverted;` |
|      - | 5464 | `	ph7_int64 nSize;` |
|      - | 5465 | `	HANDLE pHandle;` |
|      - | 5466 |  |
|      1 | 5467 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5468 | `	if( pConverted == 0 ){` |
|    ! 0 | 5469 | `		return -1;` |
|      - | 5470 | `	}` |
|      - | 5471 | `	/* Open the file in read-only mode */` |
|      1 | 5472 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 5473 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5474 | `	if( pHandle ){` |
|      1 | 5475 | `		dwLow = GetFileSize(pHandle,&dwHigh);` |
|      1 | 5476 | `		nSize = dwHigh;` |
|      1 | 5477 | `		nSize <<= 32;` |
|      1 | 5478 | `		nSize += dwLow;` |
|      1 | 5479 | `		CloseHandle(pHandle);` |
|      1 | 5480 | `	}else{` |
|    ! 0 | 5481 | `		nSize = -1;` |
|      - | 5482 | `	}` |
|      1 | 5483 | `	return nSize;` |
|      1 | 5484 |  |
|      - | 5485 | `#define TICKS_PER_SECOND 10000000` |
|      - | 5486 | `#define EPOCH_DIFFERENCE 11644473600LL` |
|      - | 5487 | `/* Convert Windows timestamp to UNIX timestamp */` |
|      - | 5488 | `static ph7_int64 convertWindowsTimeToUnixTime(LPFILETIME pTime)` |
|      1 | 5489 |  |
|      - | 5490 | `    ph7_int64 input,temp;` |
|      1 | 5491 | `	input = pTime->dwHighDateTime;` |
|      1 | 5492 | `	input <<= 32;` |
|      1 | 5493 | `	input += pTime->dwLowDateTime;` |
|      1 | 5494 | `    temp = input / TICKS_PER_SECOND; /*convert from 100ns intervals to seconds*/` |
|      1 | 5495 | `    temp = temp - EPOCH_DIFFERENCE;  /*subtract number of seconds between epochs*/` |
|      1 | 5496 | `    return temp;` |
|      1 | 5497 |  |
|      - | 5498 | `/* Convert UNIX timestamp to Windows timestamp */` |
|      - | 5499 | `static void convertUnixTimeToWindowsTime(ph7_int64 nUnixtime,LPFILETIME pOut)` |
|    ! 0 | 5500 |  |
|    ! 0 | 5501 | `  ph7_int64 result = EPOCH_DIFFERENCE;` |
|    ! 0 | 5502 | `  result += nUnixtime;` |
|    ! 0 | 5503 | `  result *= 10000000LL;` |
|    ! 0 | 5504 | `  pOut->dwHighDateTime = (DWORD)(nUnixtime>>32);` |
|    ! 0 | 5505 | `  pOut->dwLowDateTime = (DWORD)nUnixtime;` |
|    ! 0 | 5506 |  |
|      - | 5507 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 5508 | `static int WinVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|      1 | 5509 |  |
|      - | 5510 | `	FILETIME sTouch,sAccess;` |
|      - | 5511 | `	void *pConverted;` |
|      - | 5512 | `	void *pHandle;` |
|      1 | 5513 | `	BOOL rc = 0;` |
|      1 | 5514 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5515 | `	if( pConverted == 0 ){` |
|    ! 0 | 5516 | `		return -1;` |
|      - | 5517 | `	}` |
|      1 | 5518 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 5519 | `	if( pHandle ){` |
|      1 | 5520 | `		if( touch_time < 0 ){` |
|      1 | 5521 | `			GetSystemTimeAsFileTime(&sTouch);` |
|      1 | 5522 | `		}else{` |
|    ! 0 | 5523 | `			convertUnixTimeToWindowsTime(touch_time,&sTouch);` |
|      - | 5524 | `		}` |
|      1 | 5525 | `		if( access_time < 0 ){` |
|      - | 5526 | `			/* Use the touch time */` |
|      1 | 5527 | `			sAccess = sTouch; /* Structure assignment */` |
|      1 | 5528 | `		}else{` |
|    ! 0 | 5529 | `			convertUnixTimeToWindowsTime(access_time,&sAccess);` |
|      - | 5530 | `		}` |
|      1 | 5531 | `		rc = SetFileTime(pHandle,&sTouch,&sAccess,0);` |
|      - | 5532 | `		/* Close the handle */` |
|      1 | 5533 | `		CloseHandle(pHandle);` |
|      - | 5534 | `	}` |
|      1 | 5535 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5536 | `	return rc ? PH7_OK : -1;` |
|      1 | 5537 |  |
|      - | 5538 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 5539 | `static ph7_int64 WinVfs_FileAtime(const char *zPath)` |
|      1 | 5540 |  |
|      - | 5541 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|      - | 5542 | `	void * pConverted;` |
|      - | 5543 | `	ph7_int64 atime;` |
|      - | 5544 | `	HANDLE pHandle;` |
|      1 | 5545 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5546 | `	if( pConverted == 0 ){` |
|    ! 0 | 5547 | `		return -1;` |
|      - | 5548 | `	}` |
|      - | 5549 | `	/* Open the file in read-only mode */` |
|      1 | 5550 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 5551 | `	if( pHandle ){` |
|      - | 5552 | `		BOOL rc;` |
|      1 | 5553 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|      1 | 5554 | `		if( rc ){` |
|      1 | 5555 | `			atime = convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime);` |
|      1 | 5556 | `		}else{` |
|    ! 0 | 5557 | `			atime = -1;` |
|      - | 5558 | `		}` |
|      1 | 5559 | `		CloseHandle(pHandle);` |
|      1 | 5560 | `	}else{` |
|    ! 0 | 5561 | `		atime = -1;` |
|      - | 5562 | `	}` |
|      1 | 5563 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5564 | `	return atime;` |
|      1 | 5565 |  |
|      - | 5566 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 5567 | `static ph7_int64 WinVfs_FileMtime(const char *zPath)` |
|      1 | 5568 |  |
|      - | 5569 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|      - | 5570 | `	void * pConverted;` |
|      - | 5571 | `	ph7_int64 mtime;` |
|      - | 5572 | `	HANDLE pHandle;` |
|      1 | 5573 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5574 | `	if( pConverted == 0 ){` |
|    ! 0 | 5575 | `		return -1;` |
|      - | 5576 | `	}` |
|      - | 5577 | `	/* Open the file in read-only mode */` |
|      1 | 5578 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 5579 | `	if( pHandle ){` |
|      - | 5580 | `		BOOL rc;` |
|      1 | 5581 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|      1 | 5582 | `		if( rc ){` |
|      1 | 5583 | `			mtime = convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime);` |
|      1 | 5584 | `		}else{` |
|    ! 0 | 5585 | `			mtime = -1;` |
|      - | 5586 | `		}` |
|      1 | 5587 | `		CloseHandle(pHandle);` |
|      1 | 5588 | `	}else{` |
|    ! 0 | 5589 | `		mtime = -1;` |
|      - | 5590 | `	}` |
|      1 | 5591 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5592 | `	return mtime;` |
|      1 | 5593 |  |
|      - | 5594 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 5595 | `static ph7_int64 WinVfs_FileCtime(const char *zPath)` |
|      1 | 5596 |  |
|      - | 5597 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|      - | 5598 | `	void * pConverted;` |
|      - | 5599 | `	ph7_int64 ctime;` |
|      - | 5600 | `	HANDLE pHandle;` |
|      1 | 5601 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5602 | `	if( pConverted == 0 ){` |
|    ! 0 | 5603 | `		return -1;` |
|      - | 5604 | `	}` |
|      - | 5605 | `	/* Open the file in read-only mode */` |
|      1 | 5606 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 5607 | `	if( pHandle ){` |
|      - | 5608 | `		BOOL rc;` |
|      1 | 5609 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|      1 | 5610 | `		if( rc ){` |
|      1 | 5611 | `			ctime = convertWindowsTimeToUnixTime(&sInfo.ftCreationTime);` |
|      1 | 5612 | `		}else{` |
|    ! 0 | 5613 | `			ctime = -1;` |
|      - | 5614 | `		}` |
|      1 | 5615 | `		CloseHandle(pHandle);` |
|      1 | 5616 | `	}else{` |
|    ! 0 | 5617 | `		ctime = -1;` |
|      - | 5618 | `	}` |
|      1 | 5619 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5620 | `	return ctime;` |
|      1 | 5621 |  |
|      - | 5622 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 5623 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 5624 | `static int WinVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|      1 | 5625 |  |
|      - | 5626 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|      - | 5627 | `	void *pConverted;` |
|      - | 5628 | `	HANDLE pHandle;` |
|      - | 5629 | `	BOOL rc;` |
|      1 | 5630 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5631 | `	if( pConverted == 0 ){` |
|    ! 0 | 5632 | `		return -1;` |
|      - | 5633 | `	}` |
|      - | 5634 | `	/* Open the file in read-only mode */` |
|      1 | 5635 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 5636 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5637 | `	if( pHandle == 0 ){` |
|    ! 0 | 5638 | `		return -1;` |
|      - | 5639 | `	}` |
|      1 | 5640 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|      1 | 5641 | `	CloseHandle(pHandle);` |
|      1 | 5642 | `	if( !rc ){` |
|    ! 0 | 5643 | `		return -1;` |
|      - | 5644 | `	}` |
|      - | 5645 | `	/* dev */` |
|      1 | 5646 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|      1 | 5647 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|      - | 5648 | `	/* ino */` |
|      1 | 5649 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|      1 | 5650 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|      - | 5651 | `	/* mode */` |
|      1 | 5652 | `	ph7_value_int(pWorker,0);` |
|      1 | 5653 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|      - | 5654 | `	/* nlink */` |
|      1 | 5655 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|      1 | 5656 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|      - | 5657 | `	/* uid,gid,rdev */` |
|      1 | 5658 | `	ph7_value_int(pWorker,0);` |
|      1 | 5659 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|      1 | 5660 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|      1 | 5661 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|      - | 5662 | `	/* size */` |
|      1 | 5663 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|      1 | 5664 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|      - | 5665 | `	/* atime */` |
|      1 | 5666 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|      1 | 5667 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|      - | 5668 | `	/* mtime */` |
|      1 | 5669 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|      1 | 5670 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|      - | 5671 | `	/* ctime */` |
|      1 | 5672 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|      1 | 5673 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|      - | 5674 | `	/* blksize,blocks */` |
|      1 | 5675 | `	ph7_value_int(pWorker,0);` |
|      1 | 5676 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|      1 | 5677 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|      1 | 5678 | `	return PH7_OK;` |
|      1 | 5679 |  |
|      - | 5680 | `/* int (*xIsfile)(const char *) */` |
|      - | 5681 | `static int WinVfs_isfile(const char *zPath)` |
|      2 | 5682 |  |
|      - | 5683 | `	void * pConverted;` |
|      - | 5684 | `	DWORD dwAttr;` |
|      2 | 5685 | `	pConverted = convertUtf8Filename(zPath);` |
|      2 | 5686 | `	if( pConverted == 0 ){` |
|    ! 0 | 5687 | `		return -1;` |
|      - | 5688 | `	}` |
|      2 | 5689 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|      2 | 5690 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      2 | 5691 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|      1 | 5692 | `		return -1;` |
|      - | 5693 | `	}` |
|      2 | 5694 | `	return (dwAttr & (FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE)) ? PH7_OK : -1;` |
|      2 | 5695 |  |
|      - | 5696 | `/* int (*xIslink)(const char *) */` |
|      - | 5697 | `static int WinVfs_islink(const char *zPath)` |
|    ! 0 | 5698 |  |
|      - | 5699 | `	void * pConverted;` |
|      - | 5700 | `	DWORD dwAttr;` |
|    ! 0 | 5701 | `	pConverted = convertUtf8Filename(zPath);` |
|    ! 0 | 5702 | `	if( pConverted == 0 ){` |
|    ! 0 | 5703 | `		return -1;` |
|      - | 5704 | `	}` |
|    ! 0 | 5705 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    ! 0 | 5706 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    ! 0 | 5707 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    ! 0 | 5708 | `		return -1;` |
|      - | 5709 | `	}` |
|    ! 0 | 5710 | `	return (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT) ? PH7_OK : -1;` |
|    ! 0 | 5711 |  |
|      - | 5712 | `/* int (*xWritable)(const char *) */` |
|      - | 5713 | `static int WinVfs_iswritable(const char *zPath)` |
|    ! 0 | 5714 |  |
|      - | 5715 | `	void * pConverted;` |
|      - | 5716 | `	DWORD dwAttr;` |
|    ! 0 | 5717 | `	pConverted = convertUtf8Filename(zPath);` |
|    ! 0 | 5718 | `	if( pConverted == 0 ){` |
|    ! 0 | 5719 | `		return -1;` |
|      - | 5720 | `	}` |
|    ! 0 | 5721 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    ! 0 | 5722 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    ! 0 | 5723 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    ! 0 | 5724 | `		return -1;` |
|      - | 5725 | `	}` |
|    ! 0 | 5726 | `	if( (dwAttr & (FILE_ATTRIBUTE_ARCHIVE\|FILE_ATTRIBUTE_NORMAL)) == 0 ){` |
|      - | 5727 | `		/* Not a regular file */` |
|    ! 0 | 5728 | `		return -1;` |
|      - | 5729 | `	}` |
|    ! 0 | 5730 | `	if( dwAttr & FILE_ATTRIBUTE_READONLY ){` |
|      - | 5731 | `		/* Read-only file */` |
|    ! 0 | 5732 | `		return -1;` |
|      - | 5733 | `	}` |
|      - | 5734 | `	/* File is writable */` |
|    ! 0 | 5735 | `	return PH7_OK;` |
|    ! 0 | 5736 |  |
|      - | 5737 | `/* int (*xExecutable)(const char *) */` |
|      - | 5738 | `static int WinVfs_isexecutable(const char *zPath)` |
|    ! 0 | 5739 |  |
|      - | 5740 | `	void * pConverted;` |
|      - | 5741 | `	DWORD dwAttr;` |
|    ! 0 | 5742 | `	pConverted = convertUtf8Filename(zPath);` |
|    ! 0 | 5743 | `	if( pConverted == 0 ){` |
|    ! 0 | 5744 | `		return -1;` |
|      - | 5745 | `	}` |
|    ! 0 | 5746 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    ! 0 | 5747 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    ! 0 | 5748 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    ! 0 | 5749 | `		return -1;` |
|      - | 5750 | `	}` |
|    ! 0 | 5751 | `	if( (dwAttr & FILE_ATTRIBUTE_NORMAL) == 0 ){` |
|      - | 5752 | `		/* Not a regular file */` |
|    ! 0 | 5753 | `		return -1;` |
|      - | 5754 | `	}` |
|      - | 5755 | `	/* File is executable */` |
|    ! 0 | 5756 | `	return PH7_OK;` |
|    ! 0 | 5757 |  |
|      - | 5758 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 5759 | `static int WinVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|      1 | 5760 |  |
|      - | 5761 | `	void * pConverted;` |
|      - | 5762 | `	DWORD dwAttr;` |
|      1 | 5763 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5764 | `	if( pConverted == 0 ){` |
|      - | 5765 | `		/* Expand 'unknown' */` |
|    ! 0 | 5766 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 5767 | `		return -1;` |
|      - | 5768 | `	}` |
|      1 | 5769 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|      1 | 5770 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5771 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|      - | 5772 | `		/* Expand 'unknown' */` |
|    ! 0 | 5773 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 5774 | `		return -1;` |
|      - | 5775 | `	}` |
|      1 | 5776 | `	if(dwAttr & (FILE_ATTRIBUTE_HIDDEN\|FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE) ){` |
|      1 | 5777 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|      1 | 5778 | `	}else if(dwAttr & FILE_ATTRIBUTE_DIRECTORY){` |
|      1 | 5779 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|    ! 0 | 5780 | `	}else if(dwAttr & FILE_ATTRIBUTE_REPARSE_POINT){` |
|    ! 0 | 5781 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|    ! 0 | 5782 | `	}else if(dwAttr & (FILE_ATTRIBUTE_DEVICE)){` |
|    ! 0 | 5783 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|    ! 0 | 5784 | `	}else{` |
|    ! 0 | 5785 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|      - | 5786 | `	}` |
|      1 | 5787 | `	return PH7_OK;` |
|      1 | 5788 |  |
|      - | 5789 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 5790 | `static int WinVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|      2 | 5791 |  |
|      - | 5792 | `	char zValue[1024];` |
|      - | 5793 | `	DWORD n;` |
|      - | 5794 | `	/*` |
|      - | 5795 | `	 * According to MSDN` |
|      - | 5796 | `	 * If lpBuffer is not large enough to hold the data, the return` |
|      - | 5797 | `	 * value is the buffer size, in characters, required to hold the` |
|      - | 5798 | `	 * string and its terminating null character and the contents` |
|      - | 5799 | `	 * of lpBuffer are undefined.` |
|      - | 5800 | `	 */` |
|      2 | 5801 | `	n = sizeof(zValue);` |
|      2 | 5802 | `	SyMemcpy("Undefined",zValue,sizeof("Undefined")-1);` |
|      - | 5803 | `	/* Extract the environment value */` |
|      2 | 5804 | `	n = GetEnvironmentVariableA(zVar,zValue,sizeof(zValue));` |
|      2 | 5805 | `	if( !n ){` |
|      - | 5806 | `		/* No such variable*/` |
|    ! 0 | 5807 | `		return -1;` |
|      - | 5808 | `	}` |
|      2 | 5809 | `	ph7_result_string(pCtx,zValue,(int)n);` |
|      2 | 5810 | `	return PH7_OK;` |
|      2 | 5811 |  |
|      - | 5812 | `/* int (*xSetenv)(const char *,const char *) */` |
|      - | 5813 | `static int WinVfs_Setenv(const char *zName,const char *zValue)` |
|      1 | 5814 |  |
|      - | 5815 | `	BOOL rc;` |
|      1 | 5816 | `	rc = SetEnvironmentVariableA(zName,zValue);` |
|      1 | 5817 | `	return rc ? PH7_OK : -1;` |
|      1 | 5818 |  |
|      - | 5819 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 5820 | `static int WinVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|      2 | 5821 |  |
|      - | 5822 | `	DWORD dwSizeLow,dwSizeHigh;` |
|      - | 5823 | `	HANDLE pHandle,pMapHandle;` |
|      - | 5824 | `	void *pConverted,*pView;` |
|      - | 5825 |  |
|      2 | 5826 | `	pConverted = convertUtf8Filename(zPath);` |
|      2 | 5827 | `	if( pConverted == 0 ){` |
|    ! 0 | 5828 | `		return -1;` |
|      - | 5829 | `	}` |
|      2 | 5830 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      2 | 5831 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      2 | 5832 | `	if( pHandle == 0 ){` |
|      1 | 5833 | `		return -1;` |
|      - | 5834 | `	}` |
|      - | 5835 | `	/* Get the file size */` |
|      2 | 5836 | `	dwSizeLow = GetFileSize(pHandle,&dwSizeHigh);` |
|      - | 5837 | `	/* Create the mapping */` |
|      2 | 5838 | `	pMapHandle = CreateFileMappingW(pHandle,0,PAGE_READONLY,dwSizeHigh,dwSizeLow,0);` |
|      2 | 5839 | `	if( pMapHandle == 0 ){` |
|    ! 0 | 5840 | `		CloseHandle(pHandle);` |
|    ! 0 | 5841 | `		return -1;` |
|      - | 5842 | `	}` |
|      2 | 5843 | `	*pSize = ((ph7_int64)dwSizeHigh << 32) \| dwSizeLow;` |
|      - | 5844 | `	/* Obtain the view */` |
|      2 | 5845 | `	pView = MapViewOfFile(pMapHandle,FILE_MAP_READ,0,0,(SIZE_T)(*pSize));` |
|      2 | 5846 | `	if( pView ){` |
|      - | 5847 | `		/* Let the upper layer point to the view */` |
|      2 | 5848 | `		*ppMap = pView;` |
|      - | 5849 | `	}` |
|      - | 5850 | `	/* Close the handle` |
|      - | 5851 | `	 * According to MSDN it's OK the close the HANDLES.` |
|      - | 5852 | `	 */` |
|      2 | 5853 | `	CloseHandle(pMapHandle);` |
|      2 | 5854 | `	CloseHandle(pHandle);` |
|      2 | 5855 | `	return pView ? PH7_OK : -1;` |
|      2 | 5856 |  |
|      - | 5857 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|      - | 5858 | `static void WinVfs_Unmap(void *pView,ph7_int64 nSize)` |
|      2 | 5859 |  |
|      2 | 5860 | `	nSize = 0; /* Compiler warning */` |
|      2 | 5861 | `	UnmapViewOfFile(pView);` |
|      2 | 5862 |  |
|      - | 5863 | `/* void (*xTempDir)(ph7_context *) */` |
|      - | 5864 | `static void WinVfs_TempDir(ph7_context *pCtx)` |
|      2 | 5865 |  |
|      - | 5866 | `	CHAR zTemp[1024];` |
|      - | 5867 | `	DWORD n;` |
|      2 | 5868 | `	n = GetTempPathA(sizeof(zTemp),zTemp);` |
|      2 | 5869 | `	if( n < 1 ){` |
|      - | 5870 | `		/* Assume the default windows temp directory */` |
|    ! 0 | 5871 | `		ph7_result_string(pCtx,"C:\\Windows\\Temp",-1/*Compute length automatically*/);` |
|    ! 0 | 5872 | `	}else{` |
|      2 | 5873 | `		ph7_result_string(pCtx,zTemp,(int)n);` |
|      - | 5874 | `	}` |
|      2 | 5875 |  |
|      - | 5876 | `/* unsigned int (*xProcessId)(void) */` |
|      - | 5877 | `static unsigned int WinVfs_ProcessId(void)` |
|      1 | 5878 |  |
|      1 | 5879 | `	DWORD nID = 0;` |
|      - | 5880 | `#ifndef __MINGW32__` |
|      1 | 5881 | `	nID = GetProcessId(GetCurrentProcess());` |
|      - | 5882 | `#endif /* __MINGW32__ */` |
|      1 | 5883 | `	return (unsigned int)nID;` |
|      1 | 5884 |  |
|      - | 5885 | `/* void (*xUsername)(ph7_context *) */` |
|      - | 5886 | `static void WinVfs_Username(ph7_context *pCtx)` |
|      1 | 5887 |  |
|      - | 5888 | `	WCHAR zUser[1024];` |
|      - | 5889 | `	DWORD nByte;` |
|      - | 5890 | `	BOOL rc;` |
|      1 | 5891 | `	nByte = sizeof(zUser);` |
|      1 | 5892 | `	rc = GetUserNameW(zUser,&nByte);` |
|      1 | 5893 | `	if( !rc ){` |
|      - | 5894 | `		/* Set a dummy name */` |
|    ! 0 | 5895 | `		ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|    ! 0 | 5896 | `	}else{` |
|      - | 5897 | `		char *zName;` |
|      1 | 5898 | `		zName = unicodeToUtf8(zUser);` |
|      1 | 5899 | `		if( zName == 0 ){` |
|    ! 0 | 5900 | `			ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|    ! 0 | 5901 | `		}else{` |
|      1 | 5902 | `			ph7_result_string(pCtx,zName,-1/*Compute length automatically*/); /* Will make it's own copy */` |
|      1 | 5903 | `			HeapFree(GetProcessHeap(),0,zName);` |
|      - | 5904 | `		}` |
|      - | 5905 | `	}` |
|      - | 5906 |  |
|      1 | 5907 |  |
|      - | 5908 | `/* Export the windows vfs */` |
|      - | 5909 | `static const ph7_vfs sWinVfs = {` |
|      - | 5910 | `	"Windows_vfs",` |
|      - | 5911 | `	PH7_VFS_VERSION,` |
|      - | 5912 | `	WinVfs_chdir,    /* int (*xChdir)(const char *) */` |
|      - | 5913 |  |
|      - | 5914 | `	WinVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|      - | 5915 | `	WinVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|      - | 5916 | `	WinVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|      - | 5917 | `	WinVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|      - | 5918 | `	WinVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|      - | 5919 | `	WinVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 5920 | `	WinVfs_Sleep,               /* int (*xSleep)(unsigned int) */` |
|      - | 5921 | `	WinVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|      - | 5922 | `	WinVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|      - | 5923 |  |
|      - | 5924 |  |
|      - | 5925 |  |
|      - | 5926 | `	WinVfs_DiskFreeSpace,/* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 5927 | `	WinVfs_DiskTotalSpace,/* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 5928 | `	WinVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 5929 | `	WinVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 5930 | `	WinVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 5931 | `	WinVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 5932 | `	WinVfs_Stat, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 5933 | `	WinVfs_Stat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 5934 | `	WinVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|      - | 5935 | `	WinVfs_islink,     /* int (*xIslink)(const char *) */` |
|      - | 5936 | `	WinVfs_isfile,     /* int (*xReadable)(const char *) */` |
|      - | 5937 | `	WinVfs_iswritable, /* int (*xWritable)(const char *) */` |
|      - | 5938 | `	WinVfs_isexecutable, /* int (*xExecutable)(const char *) */` |
|      - | 5939 | `	WinVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 5940 | `	WinVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 5941 | `	WinVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|      - | 5942 | `	WinVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 5943 | `	WinVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 5944 | `	WinVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 5945 |  |
|      - | 5946 |  |
|      - | 5947 | `	WinVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|      - | 5948 | `	WinVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|      - | 5949 |  |
|      - | 5950 |  |
|      - | 5951 | `	WinVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|      - | 5952 |  |
|      - | 5953 | `};` |
|      - | 5954 | `/* Windows file IO */` |
|      - | 5955 | `#ifndef INVALID_SET_FILE_POINTER` |
|      - | 5956 | `# define INVALID_SET_FILE_POINTER ((DWORD)-1)` |
|      - | 5957 | `#endif` |
|      - | 5958 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|      - | 5959 | `static int WinFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|      2 | 5960 |  |
|      2 | 5961 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|      2 | 5962 | `	DWORD dwAccess = GENERIC_READ;` |
|      - | 5963 | `	DWORD dwShare,dwCreate;` |
|      - | 5964 | `	void *pConverted;` |
|      - | 5965 | `	HANDLE pHandle;` |
|      - | 5966 |  |
|      2 | 5967 | `	pConverted = convertUtf8Filename(zPath);` |
|      2 | 5968 | `	if( pConverted == 0 ){` |
|    ! 0 | 5969 | `		return -1;` |
|      - | 5970 | `	}` |
|      - | 5971 | `	/* Set the desired flags according to the open mode */` |
|      2 | 5972 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|      - | 5973 | `		/* Open existing file, or create if it doesn't exist */` |
|      2 | 5974 | `		dwCreate = OPEN_ALWAYS;` |
|      2 | 5975 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|      - | 5976 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
|      2 | 5977 | `			dwCreate = CREATE_ALWAYS;` |
|      2 | 5978 | `		}` |
|      2 | 5979 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|      - | 5980 | `		/* Creates a new file, only if it does not already exist.` |
|      - | 5981 | `		* If the file exists, it fails.` |
|      - | 5982 | `		*/` |
|    ! 0 | 5983 | `		dwCreate = CREATE_NEW;` |
|      2 | 5984 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|      - | 5985 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|      - | 5986 | `		 * The file must exist.` |
|      - | 5987 | `		 */` |
|    ! 0 | 5988 | `		dwCreate = TRUNCATE_EXISTING;` |
|    ! 0 | 5989 | `	}else{` |
|      - | 5990 | `		/* Opens a file, only if it exists. */` |
|      2 | 5991 | `		dwCreate = OPEN_EXISTING;` |
|      - | 5992 | `	}` |
|      2 | 5993 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|      - | 5994 | `		/* Read+Write access */` |
|      2 | 5995 | `		dwAccess \|= GENERIC_WRITE;` |
|      2 | 5996 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|      - | 5997 | `		/* Write only access */` |
|      1 | 5998 | `		dwAccess = GENERIC_WRITE;` |
|      - | 5999 | `	}` |
|      2 | 6000 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|      - | 6001 | `		/* Append mode */` |
|    ! 0 | 6002 | `		dwAccess = FILE_APPEND_DATA;` |
|      - | 6003 | `	}` |
|      2 | 6004 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|      - | 6005 | `		/* File is temporary */` |
|    ! 0 | 6006 | `		dwType = FILE_ATTRIBUTE_TEMPORARY;` |
|      - | 6007 | `	}` |
|      2 | 6008 | `	dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|      2 | 6009 | `	pHandle = CreateFileW((LPCWSTR)pConverted,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|      2 | 6010 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      2 | 6011 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|      - | 6012 | `		SXUNUSED(pResource); /* MSVC warning */` |
|      1 | 6013 | `		return -1;` |
|      - | 6014 | `	}` |
|      - | 6015 | `	/* Make the handle accessible to the upper layer */` |
|      2 | 6016 | `	*ppHandle = (void *)pHandle;` |
|      2 | 6017 | `	return PH7_OK;` |
|      2 | 6018 |  |
|      - | 6019 | `/* An instance of the following structure is used to record state information` |
|      - | 6020 | ` * while iterating throw directory entries.` |
|      - | 6021 | ` */` |
|      - | 6022 | `typedef struct WinDir_Info WinDir_Info;` |
|      - | 6023 | `struct WinDir_Info` |
|      - | 6024 |  |
|      - | 6025 | `	HANDLE pDirHandle;` |
|      - | 6026 | `	void *pPath;` |
|      - | 6027 | `	WIN32_FIND_DATAW sInfo;` |
|      - | 6028 | `	int rc;` |
|      - | 6029 | `};` |
|      - | 6030 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|      - | 6031 | `static int WinDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|      2 | 6032 |  |
|      - | 6033 | `	WinDir_Info *pDirInfo;` |
|      - | 6034 | `	void *pConverted;` |
|      - | 6035 | `	char *zPrep;` |
|      - | 6036 | `	sxu32 n;` |
|      - | 6037 | `	/* Prepare the path */` |
|      2 | 6038 | `	n = SyStrlen(zPath);` |
|      2 | 6039 | `	zPrep = (char *)HeapAlloc(GetProcessHeap(),0,n+sizeof("\\*")+4);` |
|      2 | 6040 | `	if( zPrep == 0 ){` |
|    ! 0 | 6041 | `		return -1;` |
|      - | 6042 | `	}` |
|      2 | 6043 | `	SyMemcpy((const void *)zPath,zPrep,n);` |
|      2 | 6044 | `	zPrep[n]   = '\\';` |
|      2 | 6045 | `	zPrep[n+1] =  '*';` |
|      2 | 6046 | `	zPrep[n+2] = 0;` |
|      2 | 6047 | `	pConverted = convertUtf8Filename(zPrep);` |
|      2 | 6048 | `	HeapFree(GetProcessHeap(),0,zPrep);` |
|      2 | 6049 | `	if( pConverted == 0 ){` |
|    ! 0 | 6050 | `		return -1;` |
|      - | 6051 | `	}` |
|      - | 6052 | `	/* Allocate a new instance */` |
|      2 | 6053 | `	pDirInfo = (WinDir_Info *)HeapAlloc(GetProcessHeap(),0,sizeof(WinDir_Info));` |
|      2 | 6054 | `	if( pDirInfo == 0 ){` |
|    ! 0 | 6055 | `		pResource = 0; /* Compiler warning */` |
|    ! 0 | 6056 | `		return -1;` |
|      - | 6057 | `	}` |
|      2 | 6058 | `	pDirInfo->rc = SXRET_OK;` |
|      2 | 6059 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pConverted,&pDirInfo->sInfo);` |
|      2 | 6060 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|      - | 6061 | `		/* Cannot open directory */` |
|    ! 0 | 6062 | `		HeapFree(GetProcessHeap(),0,pConverted);` |
|    ! 0 | 6063 | `		HeapFree(GetProcessHeap(),0,pDirInfo);` |
|    ! 0 | 6064 | `		return -1;` |
|      - | 6065 | `	}` |
|      - | 6066 | `	/* Save the path */` |
|      2 | 6067 | `	pDirInfo->pPath = pConverted;` |
|      - | 6068 | `	/* Save our structure */` |
|      2 | 6069 | `	*ppHandle = pDirInfo;` |
|      2 | 6070 | `	return PH7_OK;` |
|      2 | 6071 |  |
|      - | 6072 | `/* void (*xCloseDir)(void *) */` |
|      - | 6073 | `static void WinDir_Close(void *pUserData)` |
|      2 | 6074 |  |
|      2 | 6075 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|      2 | 6076 | `	if( pDirInfo->pDirHandle != INVALID_HANDLE_VALUE ){` |
|      2 | 6077 | `		FindClose(pDirInfo->pDirHandle);` |
|      - | 6078 | `	}` |
|      2 | 6079 | `	HeapFree(GetProcessHeap(),0,pDirInfo->pPath);` |
|      2 | 6080 | `	HeapFree(GetProcessHeap(),0,pDirInfo);` |
|      2 | 6081 |  |
|      - | 6082 | `/* void (*xClose)(void *); */` |
|      - | 6083 | `static void WinFile_Close(void *pUserData)` |
|      2 | 6084 |  |
|      2 | 6085 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      2 | 6086 | `	CloseHandle(pHandle);` |
|      2 | 6087 |  |
|      - | 6088 | `/* int (*xReadDir)(void *,ph7_context *) */` |
|      - | 6089 | `static int WinDir_Read(void *pUserData,ph7_context *pCtx)` |
|      2 | 6090 |  |
|      2 | 6091 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|      - | 6092 | `	LPWIN32_FIND_DATAW pData;` |
|      - | 6093 | `	char *zName;` |
|      - | 6094 | `	BOOL rc;` |
|      - | 6095 | `	sxu32 n;` |
|      2 | 6096 | `	if( pDirInfo->rc != SXRET_OK ){` |
|      - | 6097 | `		/* No more entry to process */` |
|      2 | 6098 | `		return -1;` |
|      - | 6099 | `	}` |
|      2 | 6100 | `	pData = &pDirInfo->sInfo;` |
|      - | 6101 | `	for(;;){` |
|      2 | 6102 | `		zName = unicodeToUtf8(pData->cFileName);` |
|      2 | 6103 | `		if( zName == 0 ){` |
|      - | 6104 | `			/* Out of memory */` |
|    ! 0 | 6105 | `			return -1;` |
|      - | 6106 | `		}` |
|      2 | 6107 | `		n = SyStrlen(zName);` |
|      - | 6108 | `		/* Ignore '.' && '..' */` |
|      2 | 6109 | `		if( n > sizeof("..")-1 \|\| zName[0] != '.' \|\| ( n == sizeof("..")-1 && zName[1] != '.') ){` |
|      2 | 6110 | `			break;` |
|      - | 6111 | `		}` |
|      2 | 6112 | `		HeapFree(GetProcessHeap(),0,zName);` |
|      2 | 6113 | `		rc = FindNextFileW(pDirInfo->pDirHandle,&pDirInfo->sInfo);` |
|      2 | 6114 | `		if( !rc ){` |
|    ! 0 | 6115 | `			return -1;` |
|      - | 6116 | `		}` |
|      2 | 6117 | `	}` |
|      - | 6118 | `	/* Return the current file name */` |
|      2 | 6119 | `	ph7_result_string(pCtx,zName,-1);` |
|      2 | 6120 | `	HeapFree(GetProcessHeap(),0,zName);` |
|      - | 6121 | `	/* Point to the next entry */` |
|      2 | 6122 | `	rc = FindNextFileW(pDirInfo->pDirHandle,&pDirInfo->sInfo);` |
|      2 | 6123 | `	if( !rc ){` |
|      2 | 6124 | `		pDirInfo->rc = SXERR_EOF;` |
|      - | 6125 | `	}` |
|      2 | 6126 | `	return PH7_OK;` |
|      2 | 6127 |  |
|      - | 6128 | `/* void (*xRewindDir)(void *) */` |
|      - | 6129 | `static void WinDir_RewindDir(void *pUserData)` |
|      1 | 6130 |  |
|      1 | 6131 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|      1 | 6132 | `	FindClose(pDirInfo->pDirHandle);` |
|      1 | 6133 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pDirInfo->pPath,&pDirInfo->sInfo);` |
|      1 | 6134 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|    ! 0 | 6135 | `		pDirInfo->rc = SXERR_EOF;` |
|    ! 0 | 6136 | `	}else{` |
|      1 | 6137 | `		pDirInfo->rc = SXRET_OK;` |
|      - | 6138 | `	}` |
|      1 | 6139 |  |
|      - | 6140 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
|      - | 6141 | `static ph7_int64 WinFile_Read(void *pOS,void *pBuffer,ph7_int64 nDatatoRead)` |
|      2 | 6142 |  |
|      2 | 6143 | `	HANDLE pHandle = (HANDLE)pOS;` |
|      - | 6144 | `	DWORD nRd;` |
|      - | 6145 | `	BOOL rc;` |
|      2 | 6146 | `	rc = ReadFile(pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|      2 | 6147 | `	if( !rc ){` |
|      - | 6148 | `		/* EOF or IO error */` |
|    ! 0 | 6149 | `		return -1;` |
|      - | 6150 | `	}` |
|      2 | 6151 | `	return (ph7_int64)nRd;` |
|      2 | 6152 |  |
|      - | 6153 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
|      - | 6154 | `static ph7_int64 WinFile_Write(void *pOS,const void *pBuffer,ph7_int64 nWrite)` |
|      2 | 6155 |  |
|      2 | 6156 | `	const char *zData = (const char *)pBuffer;` |
|      2 | 6157 | `	HANDLE pHandle = (HANDLE)pOS;` |
|      - | 6158 | `	ph7_int64 nCount;` |
|      - | 6159 | `	DWORD nWr;` |
|      - | 6160 | `	BOOL rc;` |
|      2 | 6161 | `	nWr = 0;` |
|      2 | 6162 | `	nCount = 0;` |
|      - | 6163 | `	for(;;){` |
|      2 | 6164 | `		if( nWrite < 1 ){` |
|      2 | 6165 | `			break;` |
|      - | 6166 | `		}` |
|      2 | 6167 | `		rc = WriteFile(pHandle,zData,(DWORD)nWrite,&nWr,0);` |
|      2 | 6168 | `		if( !rc ){` |
|      - | 6169 | `			/* IO error */` |
|    ! 0 | 6170 | `			break;` |
|      - | 6171 | `		}` |
|      2 | 6172 | `		nWrite -= nWr;` |
|      2 | 6173 | `		nCount += nWr;` |
|      2 | 6174 | `		zData += nWr;` |
|      2 | 6175 | `	}` |
|      2 | 6176 | `	if( nWrite > 0 ){` |
|    ! 0 | 6177 | `		return -1;` |
|      - | 6178 | `	}` |
|      2 | 6179 | `	return nCount;` |
|      2 | 6180 |  |
|      - | 6181 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|      - | 6182 | `static int WinFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|      1 | 6183 |  |
|      1 | 6184 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6185 | `	DWORD dwMove,dwNew;` |
|      - | 6186 | `	LONG nHighOfft;` |
|      1 | 6187 | `	switch(whence){` |
|      - | 6188 | `	case 1:/*SEEK_CUR*/` |
|    ! 0 | 6189 | `		dwMove = FILE_CURRENT;` |
|    ! 0 | 6190 | `		break;` |
|      - | 6191 | `	case 2: /* SEEK_END */` |
|    ! 0 | 6192 | `		dwMove = FILE_END;` |
|    ! 0 | 6193 | `		break;` |
|      - | 6194 | `	case 0: /* SEEK_SET */` |
|      - | 6195 | `	default:` |
|      1 | 6196 | `		dwMove = FILE_BEGIN;` |
|      - | 6197 | `		break;` |
|      - | 6198 | `	}` |
|      1 | 6199 | `	nHighOfft = (LONG)(iOfft >> 32);` |
|      1 | 6200 | `	dwNew = SetFilePointer(pHandle,(LONG)iOfft,&nHighOfft,dwMove);` |
|      1 | 6201 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|    ! 0 | 6202 | `		return -1;` |
|      - | 6203 | `	}` |
|      1 | 6204 | `	return PH7_OK;` |
|      1 | 6205 |  |
|      - | 6206 | `/* int (*xLock)(void *,int) */` |
|      - | 6207 | `static int WinFile_Lock(void *pUserData,int lock_type)` |
|      1 | 6208 |  |
|      1 | 6209 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6210 | `	static DWORD dwLo = 0,dwHi = 0; /* xx: MT-SAFE */` |
|      - | 6211 | `	OVERLAPPED sDummy;` |
|      - | 6212 | `	BOOL rc;` |
|      1 | 6213 | `	SyZero(&sDummy,sizeof(sDummy));` |
|      - | 6214 | `	/* Get the file size */` |
|      1 | 6215 | `	if( lock_type < 1 ){` |
|      - | 6216 | `		/* Unlock the file */` |
|      1 | 6217 | `		rc = UnlockFileEx(pHandle,0,dwLo,dwHi,&sDummy);` |
|      1 | 6218 | `	}else{` |
|      1 | 6219 | `		DWORD dwFlags = LOCKFILE_FAIL_IMMEDIATELY; /* Shared non-blocking lock by default*/` |
|      - | 6220 | `		/* Lock the file */` |
|      1 | 6221 | `		if( lock_type == 1 /* LOCK_EXCL */ ){` |
|      1 | 6222 | `			dwFlags \|= LOCKFILE_EXCLUSIVE_LOCK;` |
|      - | 6223 | `		}` |
|      1 | 6224 | `		dwLo = GetFileSize(pHandle,&dwHi);` |
|      1 | 6225 | `		rc = LockFileEx(pHandle,dwFlags,0,dwLo,dwHi,&sDummy);` |
|      - | 6226 | `	}` |
|      1 | 6227 | `	return rc ? PH7_OK : -1 /* Lock error */;` |
|      1 | 6228 |  |
|      - | 6229 | `/* ph7_int64 (*xTell)(void *) */` |
|      - | 6230 | `static ph7_int64 WinFile_Tell(void *pUserData)` |
|      1 | 6231 |  |
|      1 | 6232 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6233 | `	DWORD dwNew;` |
|      1 | 6234 | `	dwNew = SetFilePointer(pHandle,0,0,FILE_CURRENT/* SEEK_CUR */);` |
|      1 | 6235 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|    ! 0 | 6236 | `		return -1;` |
|      - | 6237 | `	}` |
|      1 | 6238 | `	return (ph7_int64)dwNew;` |
|      1 | 6239 |  |
|      - | 6240 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|      - | 6241 | `static int WinFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|      1 | 6242 |  |
|      1 | 6243 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6244 | `	LONG HighOfft;` |
|      - | 6245 | `	DWORD dwNew;` |
|      - | 6246 | `	BOOL rc;` |
|      1 | 6247 | `	HighOfft = (LONG)(nOfft >> 32);` |
|      1 | 6248 | `	dwNew = SetFilePointer(pHandle,(LONG)nOfft,&HighOfft,FILE_BEGIN);` |
|      1 | 6249 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|    ! 0 | 6250 | `		return -1;` |
|      - | 6251 | `	}` |
|      1 | 6252 | `	rc = SetEndOfFile(pHandle);` |
|      1 | 6253 | `	return rc ? PH7_OK : -1;` |
|      1 | 6254 |  |
|      - | 6255 | `/* int (*xSync)(void *); */` |
|      - | 6256 | `static int WinFile_Sync(void *pUserData)` |
|      1 | 6257 |  |
|      1 | 6258 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6259 | `	BOOL rc;` |
|      1 | 6260 | `	rc = FlushFileBuffers(pHandle);` |
|      1 | 6261 | `	return rc ? PH7_OK : - 1;` |
|      1 | 6262 |  |
|      - | 6263 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|      - | 6264 | `static int WinFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|      1 | 6265 |  |
|      - | 6266 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|      1 | 6267 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6268 | `	BOOL rc;` |
|      1 | 6269 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|      1 | 6270 | `	if( !rc ){` |
|    ! 0 | 6271 | `		return -1;` |
|      - | 6272 | `	}` |
|      - | 6273 | `	/* dev */` |
|      1 | 6274 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|      1 | 6275 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|      - | 6276 | `	/* ino */` |
|      1 | 6277 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|      1 | 6278 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|      - | 6279 | `	/* mode */` |
|      1 | 6280 | `	ph7_value_int(pWorker,0);` |
|      1 | 6281 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|      - | 6282 | `	/* nlink */` |
|      1 | 6283 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|      1 | 6284 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|      - | 6285 | `	/* uid,gid,rdev */` |
|      1 | 6286 | `	ph7_value_int(pWorker,0);` |
|      1 | 6287 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|      1 | 6288 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|      1 | 6289 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|      - | 6290 | `	/* size */` |
|      1 | 6291 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|      1 | 6292 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|      - | 6293 | `	/* atime */` |
|      1 | 6294 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|      1 | 6295 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|      - | 6296 | `	/* mtime */` |
|      1 | 6297 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|      1 | 6298 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|      - | 6299 | `	/* ctime */` |
|      1 | 6300 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|      1 | 6301 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|      - | 6302 | `	/* blksize,blocks */` |
|      1 | 6303 | `	ph7_value_int(pWorker,0);` |
|      1 | 6304 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|      1 | 6305 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|      1 | 6306 | `	return PH7_OK;` |
|      1 | 6307 |  |
|      - | 6308 | `/* Export the file:// stream */` |
|      - | 6309 | `static const ph7_io_stream sWinFileStream = {` |
|      - | 6310 | `	"file", /* Stream name */` |
|      - | 6311 | `	PH7_IO_STREAM_VERSION,` |
|      - | 6312 | `	WinFile_Open,  /* xOpen */` |
|      - | 6313 | `	WinDir_Open,   /* xOpenDir */` |
|      - | 6314 | `	WinFile_Close, /* xClose */` |
|      - | 6315 | `	WinDir_Close,  /* xCloseDir */` |
|      - | 6316 | `	WinFile_Read,  /* xRead */` |
|      - | 6317 | `	WinDir_Read,   /* xReadDir */` |
|      - | 6318 | `	WinFile_Write, /* xWrite */` |
|      - | 6319 | `	WinFile_Seek,  /* xSeek */` |
|      - | 6320 | `	WinFile_Lock,  /* xLock */` |
|      - | 6321 | `	WinDir_RewindDir, /* xRewindDir */` |
|      - | 6322 | `	WinFile_Tell,  /* xTell */` |
|      - | 6323 | `	WinFile_Trunc, /* xTrunc */` |
|      - | 6324 | `	WinFile_Sync,  /* xSeek */` |
|      - | 6325 | `	WinFile_Stat   /* xStat */` |
|      - | 6326 | `};` |
|      - | 6327 | `#elif defined(__UNIXES__)` |
|      - | 6328 | `/*` |
|      - | 6329 | ` * UNIX VFS implementation for the PH7 engine.` |
|      - | 6330 | ` * Status:` |
|      - | 6331 | ` *    Stable.` |
|      - | 6332 | ` */` |
|      - | 6333 | `#include <sys/types.h>` |
|      - | 6334 | `#include <limits.h>` |
|      - | 6335 | `#include <fcntl.h>` |
|      - | 6336 | `#include <unistd.h>` |
|      - | 6337 | `#include <sys/uio.h>` |
|      - | 6338 | `#include <sys/stat.h>` |
|      - | 6339 | `#include <sys/mman.h>` |
|      - | 6340 | `#include <sys/file.h>` |
|      - | 6341 | `#include <sys/wait.h>` |
|      - | 6342 | `#include <pwd.h>` |
|      - | 6343 | `#include <grp.h>` |
|      - | 6344 | `#include <dirent.h>` |
|      - | 6345 | `#include <utime.h>` |
|      - | 6346 | `#include <stdio.h>` |
|      - | 6347 | `#include <stdlib.h>` |
|      - | 6348 | `/* int (*xchdir)(const char *) */` |
|   9358 | 6349 | `static int UnixVfs_chdir(const char *zPath)` |
|      - | 6350 |  |
|      - | 6351 | `  int rc;` |
|   9358 | 6352 | `  rc = chdir(zPath);` |
|   9358 | 6353 | `  return rc == 0 ? PH7_OK : -1;` |
|      - | 6354 |  |
|      - | 6355 | `/* int (*xGetcwd)(ph7_context *) */` |
|     20 | 6356 | `static int UnixVfs_getcwd(ph7_context *pCtx)` |
|      - | 6357 |  |
|      - | 6358 | `	char zBuf[4096];` |
|      - | 6359 | `	char *zDir;` |
|      - | 6360 | `	/* Get the current directory */` |
|     20 | 6361 | `	zDir = getcwd(zBuf,sizeof(zBuf));` |
|     20 | 6362 | `	if( zDir == 0 ){` |
|    ! 0 | 6363 | `	  return -1;` |
|      - | 6364 | `    }` |
|     20 | 6365 | `	ph7_result_string(pCtx,zDir,-1/*Compute length automatically*/);` |
|     20 | 6366 | `	return PH7_OK;` |
|     10 | 6367 |  |
|      - | 6368 | `/* int (*xMkdir)(const char *,int,int) */` |
|      6 | 6369 | `static int UnixVfs_mkdir(const char *zPath,int mode,int recursive)` |
|      - | 6370 |  |
|      - | 6371 | `	int rc;` |
|      6 | 6372 | `        rc = mkdir(zPath,mode);` |
|      3 | 6373 | `	SXUNUSED(recursive); /* cc warning */` |
|      6 | 6374 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 6375 |  |
|      - | 6376 | `/* int (*xRmdir)(const char *) */` |
|      8 | 6377 | `static int UnixVfs_rmdir(const char *zPath)` |
|      - | 6378 |  |
|      - | 6379 | `	int rc;` |
|      8 | 6380 | `	rc = rmdir(zPath);` |
|      8 | 6381 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 6382 |  |
|      - | 6383 | `/* int (*xIsdir)(const char *) */` |
|   5592 | 6384 | `static int UnixVfs_isdir(const char *zPath)` |
|      - | 6385 |  |
|      - | 6386 | `	struct stat st;` |
|      - | 6387 | `	int rc;` |
|   5592 | 6388 | `	rc = stat(zPath,&st);` |
|   5592 | 6389 | `	if( rc != 0 ){` |
|      4 | 6390 | `	 return -1;` |
|      - | 6391 | `	}` |
|   5588 | 6392 | `	rc = S_ISDIR(st.st_mode);` |
|   5588 | 6393 | `	return rc ? PH7_OK : -1 ;` |
|   2796 | 6394 |  |
|      - | 6395 | `/* int (*xRename)(const char *,const char *) */` |
|      2 | 6396 | `static int UnixVfs_Rename(const char *zOld,const char *zNew)` |
|      - | 6397 |  |
|      - | 6398 | `	int rc;` |
|      2 | 6399 | `	rc = rename(zOld,zNew);` |
|      2 | 6400 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 6401 |  |
|      - | 6402 | `/* int (*xRealpath)(const char *,ph7_context *) */` |
|      4 | 6403 | `static int UnixVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|      - | 6404 |  |
|      - | 6405 | `#ifndef PH7_UNIX_OLD_LIBC` |
|      - | 6406 | `	char *zReal;` |
|      4 | 6407 | `	zReal = realpath(zPath,0);` |
|      4 | 6408 | `	if( zReal == 0 ){` |
|      2 | 6409 | `	  return -1;` |
|      - | 6410 | `	}` |
|      2 | 6411 | `	ph7_result_string(pCtx,zReal,-1/*Compute length automatically*/);` |
|      - | 6412 | `        /* Release the allocated buffer */` |
|      2 | 6413 | `	free(zReal);` |
|      2 | 6414 | `	return PH7_OK;` |
|      - | 6415 | `#else` |
|      - | 6416 | `    zPath = 0; /* cc warning */` |
|      - | 6417 | `    pCtx = 0;` |
|      - | 6418 | `    return -1;` |
|      - | 6419 | `#endif` |
|      2 | 6420 |  |
|      - | 6421 | `/* int (*xSleep)(unsigned int) */` |
|      4 | 6422 | `static int UnixVfs_Sleep(unsigned int uSec)` |
|      - | 6423 |  |
|      4 | 6424 | `	usleep(uSec);` |
|      4 | 6425 | `	return PH7_OK;` |
|      - | 6426 |  |
|      - | 6427 | `/* int (*xUnlink)(const char *) */` |
|  19752 | 6428 | `static int UnixVfs_unlink(const char *zPath)` |
|      - | 6429 |  |
|      - | 6430 | `	int rc;` |
|  19752 | 6431 | `	rc = unlink(zPath);` |
|  19752 | 6432 | `	return rc == 0 ? PH7_OK : -1 ;` |
|      - | 6433 |  |
|      - | 6434 | `/* int (*xFileExists)(const char *) */` |
|     44 | 6435 | `static int UnixVfs_FileExists(const char *zPath)` |
|      - | 6436 |  |
|      - | 6437 | `	int rc;` |
|     44 | 6438 | `	rc = access(zPath,F_OK);` |
|     44 | 6439 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 6440 |  |
|      - | 6441 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|     26 | 6442 | `static ph7_int64 UnixVfs_FileSize(const char *zPath)` |
|      - | 6443 |  |
|      - | 6444 | `	struct stat st;` |
|      - | 6445 | `	int rc;` |
|     26 | 6446 | `	rc = stat(zPath,&st);` |
|     26 | 6447 | `	if( rc != 0 ){` |
|    ! 0 | 6448 | `	 return -1;` |
|      - | 6449 | `	}` |
|     26 | 6450 | `	return (ph7_int64)st.st_size;` |
|     13 | 6451 |  |
|      - | 6452 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      4 | 6453 | `static int UnixVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|      - | 6454 |  |
|      - | 6455 | `	struct utimbuf ut;` |
|      - | 6456 | `	int rc;` |
|      4 | 6457 | `	ut.actime  = (time_t)access_time;` |
|      4 | 6458 | `	ut.modtime = (time_t)touch_time;` |
|      4 | 6459 | `	rc = utime(zPath,&ut);` |
|      4 | 6460 | `	if( rc != 0 ){` |
|    ! 0 | 6461 | `	 return -1;` |
|      - | 6462 | `	}` |
|      4 | 6463 | `	return PH7_OK;` |
|      2 | 6464 |  |
|      - | 6465 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|      2 | 6466 | `static ph7_int64 UnixVfs_FileAtime(const char *zPath)` |
|      - | 6467 |  |
|      - | 6468 | `	struct stat st;` |
|      - | 6469 | `	int rc;` |
|      2 | 6470 | `	rc = stat(zPath,&st);` |
|      2 | 6471 | `	if( rc != 0 ){` |
|    ! 0 | 6472 | `	 return -1;` |
|      - | 6473 | `	}` |
|      2 | 6474 | `	return (ph7_int64)st.st_atime;` |
|      1 | 6475 |  |
|      - | 6476 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|      4 | 6477 | `static ph7_int64 UnixVfs_FileMtime(const char *zPath)` |
|      - | 6478 |  |
|      - | 6479 | `	struct stat st;` |
|      - | 6480 | `	int rc;` |
|      4 | 6481 | `	rc = stat(zPath,&st);` |
|      4 | 6482 | `	if( rc != 0 ){` |
|    ! 0 | 6483 | `	 return -1;` |
|      - | 6484 | `	}` |
|      4 | 6485 | `	return (ph7_int64)st.st_mtime;` |
|      2 | 6486 |  |
|      - | 6487 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|      2 | 6488 | `static ph7_int64 UnixVfs_FileCtime(const char *zPath)` |
|      - | 6489 |  |
|      - | 6490 | `	struct stat st;` |
|      - | 6491 | `	int rc;` |
|      2 | 6492 | `	rc = stat(zPath,&st);` |
|      2 | 6493 | `	if( rc != 0 ){` |
|    ! 0 | 6494 | `	 return -1;` |
|      - | 6495 | `	}` |
|      2 | 6496 | `	return (ph7_int64)st.st_ctime;` |
|      1 | 6497 |  |
|      - | 6498 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      4 | 6499 | `static int UnixVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|      - | 6500 |  |
|      - | 6501 | `	struct stat st;` |
|      - | 6502 | `	int rc;` |
|      4 | 6503 | `	rc = stat(zPath,&st);` |
|      4 | 6504 | `	if( rc != 0 ){` |
|    ! 0 | 6505 | `	 return -1;` |
|      - | 6506 | `	}` |
|      - | 6507 | `	/* dev */` |
|      4 | 6508 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|      4 | 6509 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|      - | 6510 | `	/* ino */` |
|      4 | 6511 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|      4 | 6512 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|      - | 6513 | `	/* mode */` |
|      4 | 6514 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|      4 | 6515 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|      - | 6516 | `	/* nlink */` |
|      4 | 6517 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|      4 | 6518 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|      - | 6519 | `	/* uid,gid,rdev */` |
|      4 | 6520 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|      4 | 6521 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|      4 | 6522 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|      4 | 6523 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|      4 | 6524 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|      4 | 6525 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|      - | 6526 | `	/* size */` |
|      4 | 6527 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|      4 | 6528 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|      - | 6529 | `	/* atime */` |
|      4 | 6530 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|      4 | 6531 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|      - | 6532 | `	/* mtime */` |
|      4 | 6533 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|      4 | 6534 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|      - | 6535 | `	/* ctime */` |
|      4 | 6536 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|      4 | 6537 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|      - | 6538 | `	/* blksize,blocks */` |
|      4 | 6539 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|      4 | 6540 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|      4 | 6541 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|      4 | 6542 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|      4 | 6543 | `	return PH7_OK;` |
|      2 | 6544 |  |
|      - | 6545 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      2 | 6546 | `static int UnixVfs_lStat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|      - | 6547 |  |
|      - | 6548 | `	struct stat st;` |
|      - | 6549 | `	int rc;` |
|      2 | 6550 | `	rc = lstat(zPath,&st);` |
|      2 | 6551 | `	if( rc != 0 ){` |
|    ! 0 | 6552 | `	 return -1;` |
|      - | 6553 | `	}` |
|      - | 6554 | `	/* dev */` |
|      2 | 6555 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|      2 | 6556 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|      - | 6557 | `	/* ino */` |
|      2 | 6558 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|      2 | 6559 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|      - | 6560 | `	/* mode */` |
|      2 | 6561 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|      2 | 6562 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|      - | 6563 | `	/* nlink */` |
|      2 | 6564 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|      2 | 6565 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|      - | 6566 | `	/* uid,gid,rdev */` |
|      2 | 6567 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|      2 | 6568 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|      2 | 6569 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|      2 | 6570 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|      2 | 6571 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|      2 | 6572 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|      - | 6573 | `	/* size */` |
|      2 | 6574 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|      2 | 6575 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|      - | 6576 | `	/* atime */` |
|      2 | 6577 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|      2 | 6578 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|      - | 6579 | `	/* mtime */` |
|      2 | 6580 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|      2 | 6581 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|      - | 6582 | `	/* ctime */` |
|      2 | 6583 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|      2 | 6584 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|      - | 6585 | `	/* blksize,blocks */` |
|      2 | 6586 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|      2 | 6587 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|      2 | 6588 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|      2 | 6589 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|      2 | 6590 | `	return PH7_OK;` |
|      1 | 6591 |  |
|      - | 6592 | `/* int (*xChmod)(const char *,int) */` |
|     10 | 6593 | `static int UnixVfs_Chmod(const char *zPath,int mode)` |
|      - | 6594 |  |
|      - | 6595 | `    int rc;` |
|     10 | 6596 | `    rc = chmod(zPath,(mode_t)mode);` |
|     10 | 6597 | `    return rc == 0 ? PH7_OK : - 1;` |
|      - | 6598 |  |
|      - | 6599 | `/* int (*xChown)(const char *,const char *) */` |
|      4 | 6600 | `static int UnixVfs_Chown(const char *zPath,const char *zUser)` |
|      - | 6601 |  |
|      - | 6602 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|      - | 6603 | `  struct passwd *pwd;` |
|      - | 6604 | `  uid_t uid;` |
|      - | 6605 | `  int rc;` |
|      4 | 6606 | `  pwd = getpwnam(zUser);   /* Try getting UID for username */` |
|      4 | 6607 | `  if (pwd == 0) {` |
|      4 | 6608 | `    return -1;` |
|      - | 6609 | `  }` |
|    ! 0 | 6610 | `  uid = pwd->pw_uid;` |
|    ! 0 | 6611 | `  rc = chown(zPath,uid,-1);` |
|    ! 0 | 6612 | `  return rc == 0 ? PH7_OK : -1;` |
|      - | 6613 | `#else` |
|      - | 6614 | `	SXUNUSED(zPath);` |
|      - | 6615 | `	SXUNUSED(zUser);` |
|      - | 6616 | `	return -1;` |
|      - | 6617 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|      2 | 6618 |  |
|      - | 6619 | `/* int (*xChgrp)(const char *,const char *) */` |
|      4 | 6620 | `static int UnixVfs_Chgrp(const char *zPath,const char *zGroup)` |
|      - | 6621 |  |
|      - | 6622 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|      - | 6623 | `  struct group *group;` |
|      - | 6624 | `  gid_t gid;` |
|      - | 6625 | `  int rc;` |
|      4 | 6626 | `  group = getgrnam(zGroup);` |
|      4 | 6627 | `  if (group == 0) {` |
|      4 | 6628 | `    return -1;` |
|      - | 6629 | `  }` |
|    ! 0 | 6630 | `  gid = group->gr_gid;` |
|    ! 0 | 6631 | `  rc = chown(zPath,-1,gid);` |
|    ! 0 | 6632 | `  return rc == 0 ? PH7_OK : -1;` |
|      - | 6633 | `#else` |
|      - | 6634 | `	SXUNUSED(zPath);` |
|      - | 6635 | `	SXUNUSED(zGroup);` |
|      - | 6636 | `	return -1;` |
|      - | 6637 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|      2 | 6638 |  |
|      - | 6639 | `/* int (*xIsfile)(const char *) */` |
|   3924 | 6640 | `static int UnixVfs_isfile(const char *zPath)` |
|      - | 6641 |  |
|      - | 6642 | `	struct stat st;` |
|      - | 6643 | `	int rc;` |
|   3924 | 6644 | `	rc = stat(zPath,&st);` |
|   3924 | 6645 | `	if( rc != 0 ){` |
|      2 | 6646 | `	 return -1;` |
|      - | 6647 | `	}` |
|   3922 | 6648 | `	rc = S_ISREG(st.st_mode);` |
|   3922 | 6649 | `	return rc ? PH7_OK : -1 ;` |
|   1962 | 6650 |  |
|      - | 6651 | `/* int (*xIslink)(const char *) */` |
|      4 | 6652 | `static int UnixVfs_islink(const char *zPath)` |
|      - | 6653 |  |
|      - | 6654 | `	struct stat st;` |
|      - | 6655 | `	int rc;` |
|      4 | 6656 | `	rc = stat(zPath,&st);` |
|      4 | 6657 | `	if( rc != 0 ){` |
|    ! 0 | 6658 | `	 return -1;` |
|      - | 6659 | `	}` |
|      4 | 6660 | `	rc = S_ISLNK(st.st_mode);` |
|      4 | 6661 | `	return rc ? PH7_OK : -1 ;` |
|      2 | 6662 |  |
|      - | 6663 | `/* int (*xReadable)(const char *) */` |
|      2 | 6664 | `static int UnixVfs_isreadable(const char *zPath)` |
|      - | 6665 |  |
|      - | 6666 | `	int rc;` |
|      2 | 6667 | `	rc = access(zPath,R_OK);` |
|      2 | 6668 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 6669 |  |
|      - | 6670 | `/* int (*xWritable)(const char *) */` |
|      4 | 6671 | `static int UnixVfs_iswritable(const char *zPath)` |
|      - | 6672 |  |
|      - | 6673 | `	int rc;` |
|      4 | 6674 | `	rc = access(zPath,W_OK);` |
|      4 | 6675 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 6676 |  |
|      - | 6677 | `/* int (*xExecutable)(const char *) */` |
|      2 | 6678 | `static int UnixVfs_isexecutable(const char *zPath)` |
|      - | 6679 |  |
|      - | 6680 | `	int rc;` |
|      2 | 6681 | `	rc = access(zPath,X_OK);` |
|      2 | 6682 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 6683 |  |
|      - | 6684 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|      4 | 6685 | `static int UnixVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|      - | 6686 |  |
|      - | 6687 | `	struct stat st;` |
|      - | 6688 | `	int rc;` |
|      4 | 6689 | `    rc = stat(zPath,&st);` |
|      4 | 6690 | `	if( rc != 0 ){` |
|      - | 6691 | `	  /* Expand 'unknown' */` |
|    ! 0 | 6692 | `	  ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 6693 | `	  return -1;` |
|      - | 6694 | `	}` |
|      4 | 6695 | `	if(S_ISREG(st.st_mode) ){` |
|      2 | 6696 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|      3 | 6697 | `	}else if(S_ISDIR(st.st_mode)){` |
|      2 | 6698 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|      1 | 6699 | `	}else if(S_ISLNK(st.st_mode)){` |
|    ! 0 | 6700 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|    ! 0 | 6701 | `	}else if(S_ISBLK(st.st_mode)){` |
|    ! 0 | 6702 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|    ! 0 | 6703 | `    }else if(S_ISSOCK(st.st_mode)){` |
|    ! 0 | 6704 | `		ph7_result_string(pCtx,"socket",sizeof("socket")-1);` |
|    ! 0 | 6705 | `	}else if(S_ISFIFO(st.st_mode)){` |
|    ! 0 | 6706 | `       ph7_result_string(pCtx,"fifo",sizeof("fifo")-1);` |
|    ! 0 | 6707 | `	}else{` |
|    ! 0 | 6708 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|      - | 6709 | `	}` |
|      4 | 6710 | `	return PH7_OK;` |
|      2 | 6711 |  |
|      - | 6712 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|     16 | 6713 | `static int UnixVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|      - | 6714 |  |
|      - | 6715 | `	char *zEnv;` |
|     16 | 6716 | `	zEnv = getenv(zVar);` |
|     16 | 6717 | `	if( zEnv == 0 ){` |
|    ! 0 | 6718 | `	  return -1;` |
|      - | 6719 | `	}` |
|     16 | 6720 | `	ph7_result_string(pCtx,zEnv,-1/*Compute length automatically*/);` |
|     16 | 6721 | `	return PH7_OK;` |
|      8 | 6722 |  |
|      - | 6723 | `/* int (*xSetenv)(const char *,const char *) */` |
|      2 | 6724 | `static int UnixVfs_Setenv(const char *zName,const char *zValue)` |
|      - | 6725 |  |
|      - | 6726 | `   int rc;` |
|      2 | 6727 | `   rc = setenv(zName,zValue,1);` |
|      2 | 6728 | `   return rc == 0 ? PH7_OK : -1;` |
|      - | 6729 |  |
|      - | 6730 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|   1932 | 6731 | `static int UnixVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|      - | 6732 |  |
|      - | 6733 | `	struct stat st;` |
|      - | 6734 | `	void *pMap;` |
|      - | 6735 | `	int fd;` |
|      - | 6736 | `	int rc;` |
|      - | 6737 | `	/* Open the file in a read-only mode */` |
|   1932 | 6738 | `	fd = open(zPath,O_RDONLY);` |
|   1932 | 6739 | `	if( fd < 0 ){` |
|      2 | 6740 | `		return -1;` |
|      - | 6741 | `	}` |
|      - | 6742 | `	/* stat the handle */` |
|   1930 | 6743 | `	fstat(fd,&st);` |
|      - | 6744 | `	/* Obtain a memory view of the whole file */` |
|   1930 | 6745 | `	pMap = mmap(0,st.st_size,PROT_READ,MAP_PRIVATE\|MAP_FILE,fd,0);` |
|   1930 | 6746 | `	rc = PH7_OK;` |
|   1930 | 6747 | `	if( pMap == MAP_FAILED ){` |
|    ! 0 | 6748 | `		rc = -1;` |
|    ! 0 | 6749 | `	}else{` |
|      - | 6750 | `		/* Point to the memory view */` |
|   1930 | 6751 | `		*ppMap = pMap;` |
|   1930 | 6752 | `		*pSize = (ph7_int64)st.st_size;` |
|      - | 6753 | `	}` |
|   1930 | 6754 | `	close(fd);` |
|   1930 | 6755 | `	return rc;` |
|    966 | 6756 |  |
|      - | 6757 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|   1930 | 6758 | `static void UnixVfs_Unmap(void *pView,ph7_int64 nSize)` |
|      - | 6759 |  |
|   1930 | 6760 | `	munmap(pView,(size_t)nSize);` |
|   1930 | 6761 |  |
|      - | 6762 | `/* void (*xTempDir)(ph7_context *) */` |
|    166 | 6763 | `static void UnixVfs_TempDir(ph7_context *pCtx)` |
|      - | 6764 |  |
|      - | 6765 | `	static const char *azDirs[] = {` |
|      - | 6766 | `     "/var/tmp",` |
|      - | 6767 | `     "/usr/tmp",` |
|      - | 6768 | `	 "/usr/local/tmp"` |
|      - | 6769 | `  };` |
|      - | 6770 | `  unsigned int i;` |
|      - | 6771 | `  struct stat buf;` |
|      - | 6772 | `  const char *zDir;` |
|    166 | 6773 | `  zDir = getenv("TMPDIR");` |
|    166 | 6774 | `  if( zDir && zDir[0] != 0 && !access(zDir,07) ){` |
|     83 | 6775 | `	  ph7_result_string(pCtx,zDir,-1);` |
|     83 | 6776 | `	  return;` |
|      - | 6777 | `  }` |
|     83 | 6778 | `  for(i=0; i<sizeof(azDirs)/sizeof(azDirs[0]); i++){` |
|     83 | 6779 | `	zDir=azDirs[i];` |
|     83 | 6780 | `    if( zDir==0 ) continue;` |
|     83 | 6781 | `    if( stat(zDir, &buf) ) continue;` |
|     83 | 6782 | `    if( !S_ISDIR(buf.st_mode) ) continue;` |
|     83 | 6783 | `    if( access(zDir, 07) ) continue;` |
|      - | 6784 | `    /* Got one */` |
|     83 | 6785 | `	ph7_result_string(pCtx,zDir,-1);` |
|     83 | 6786 | `	return;` |
|      - | 6787 | `  }` |
|      - | 6788 | `  /* Default temp dir */` |
|    ! 0 | 6789 | `  ph7_result_string(pCtx,"/tmp",(int)sizeof("/tmp")-1);` |
|     83 | 6790 |  |
|      - | 6791 | `/* unsigned int (*xProcessId)(void) */` |
|      4 | 6792 | `static unsigned int UnixVfs_ProcessId(void)` |
|      - | 6793 |  |
|      4 | 6794 | `	return (unsigned int)getpid();` |
|      - | 6795 |  |
|      - | 6796 | `/* int (*xUid)(void) */` |
|      2 | 6797 | `static int UnixVfs_uid(void)` |
|      - | 6798 |  |
|      2 | 6799 | `	return (int)getuid();` |
|      - | 6800 |  |
|      - | 6801 | `/* int (*xGid)(void) */` |
|      2 | 6802 | `static int UnixVfs_gid(void)` |
|      - | 6803 |  |
|      2 | 6804 | `	return (int)getgid();` |
|      - | 6805 |  |
|      - | 6806 | `/* int (*xUmask)(int) */` |
|      8 | 6807 | `static int UnixVfs_Umask(int new_mask)` |
|      - | 6808 |  |
|      - | 6809 | `	int old_mask;` |
|      8 | 6810 | `	old_mask = umask(new_mask);` |
|      8 | 6811 | `	return old_mask;` |
|      - | 6812 |  |
|      - | 6813 | `/* void (*xUsername)(ph7_context *) */` |
|      2 | 6814 | `static void UnixVfs_Username(ph7_context *pCtx)` |
|      - | 6815 |  |
|      - | 6816 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|      - | 6817 | `  struct passwd *pwd;` |
|      - | 6818 | `  uid_t uid;` |
|      2 | 6819 | `  uid = getuid();` |
|      2 | 6820 | `  pwd = getpwuid(uid);   /* Try getting UID for username */` |
|      2 | 6821 | `  if (pwd == 0) {` |
|    ! 0 | 6822 | `    return;` |
|      - | 6823 | `  }` |
|      - | 6824 | `  /* Return the username */` |
|      2 | 6825 | `  ph7_result_string(pCtx,pwd->pw_name,-1);` |
|      - | 6826 | `#else` |
|      - | 6827 | `  ph7_result_string(pCtx,"Unknown",-1);` |
|      - | 6828 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|      2 | 6829 | `  return;` |
|      1 | 6830 |  |
|      - | 6831 | `/* int (*xLink)(const char *,const char *,int) */` |
|      8 | 6832 | `static int UnixVfs_link(const char *zSrc,const char *zTarget,int is_sym)` |
|      - | 6833 |  |
|      - | 6834 | `	int rc;` |
|      8 | 6835 | `	if( is_sym ){` |
|      - | 6836 | `		/* Symbolic link */` |
|      6 | 6837 | `		rc = symlink(zSrc,zTarget);` |
|      3 | 6838 | `	}else{` |
|      - | 6839 | `		/* Hard link */` |
|      2 | 6840 | `		rc = link(zSrc,zTarget);` |
|      - | 6841 | `	}` |
|      8 | 6842 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 6843 |  |
|      - | 6844 | `/* int (*xChroot)(const char *) */` |
|      2 | 6845 | `static int UnixVfs_chroot(const char *zRootDir)` |
|      - | 6846 |  |
|      - | 6847 | `	int rc;` |
|      2 | 6848 | `	rc = chroot(zRootDir);` |
|      2 | 6849 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 6850 |  |
|      - | 6851 | `/* Export the UNIX vfs */` |
|      - | 6852 | `static const ph7_vfs sUnixVfs = {` |
|      - | 6853 | `	"Unix_vfs",` |
|      - | 6854 | `	PH7_VFS_VERSION,` |
|      - | 6855 | `	UnixVfs_chdir,    /* int (*xChdir)(const char *) */` |
|      - | 6856 | `	UnixVfs_chroot,   /* int (*xChroot)(const char *); */` |
|      - | 6857 | `	UnixVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|      - | 6858 | `	UnixVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|      - | 6859 | `	UnixVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|      - | 6860 | `	UnixVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|      - | 6861 | `	UnixVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|      - | 6862 | `	UnixVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 6863 | `	UnixVfs_Sleep,    /* int (*xSleep)(unsigned int) */` |
|      - | 6864 | `	UnixVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|      - | 6865 | `	UnixVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|      - | 6866 | `	UnixVfs_Chmod, /*int (*xChmod)(const char *,int)*/` |
|      - | 6867 | `	UnixVfs_Chown, /*int (*xChown)(const char *,const char *)*/` |
|      - | 6868 | `	UnixVfs_Chgrp, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 6869 |  |
|      - | 6870 |  |
|      - | 6871 | `	UnixVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 6872 | `	UnixVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 6873 | `	UnixVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 6874 | `	UnixVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 6875 | `	UnixVfs_Stat,  /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6876 | `	UnixVfs_lStat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6877 | `	UnixVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|      - | 6878 | `	UnixVfs_islink,     /* int (*xIslink)(const char *) */` |
|      - | 6879 | `	UnixVfs_isreadable, /* int (*xReadable)(const char *) */` |
|      - | 6880 | `	UnixVfs_iswritable, /* int (*xWritable)(const char *) */` |
|      - | 6881 | `	UnixVfs_isexecutable,/* int (*xExecutable)(const char *) */` |
|      - | 6882 | `	UnixVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 6883 | `	UnixVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 6884 | `	UnixVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|      - | 6885 | `	UnixVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 6886 | `	UnixVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 6887 | `	UnixVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 6888 | `	UnixVfs_link,       /* int (*xLink)(const char *,const char *,int) */` |
|      - | 6889 | `	UnixVfs_Umask,      /* int (*xUmask)(int) */` |
|      - | 6890 | `	UnixVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|      - | 6891 | `	UnixVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|      - | 6892 | `	UnixVfs_uid, /* int (*xUid)(void) */` |
|      - | 6893 | `	UnixVfs_gid, /* int (*xGid)(void) */` |
|      - | 6894 | `	UnixVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|      - | 6895 |  |
|      - | 6896 | `};` |
|      - | 6897 | `/* UNIX File IO */` |
|      - | 6898 | `#define PH7_UNIX_OPEN_MODE	0640 /* Default open mode */` |
|      - | 6899 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|  21062 | 6900 | `static int UnixFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|      - | 6901 |  |
|  21062 | 6902 | `	int iOpen = O_RDONLY;` |
|      - | 6903 | `	int fd;` |
|      - | 6904 | `	/* Set the desired flags according to the open mode */` |
|  21062 | 6905 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|      - | 6906 | `		/* Open existing file, or create if it doesn't exist */` |
|   9576 | 6907 | `		iOpen = O_CREAT;` |
|   9576 | 6908 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|      - | 6909 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
|   9576 | 6910 | `			iOpen \|= O_TRUNC;` |
|   4788 | 6911 | `			SXUNUSED(pResource); /* cc warning */` |
|   4788 | 6912 | `		}` |
|  16274 | 6913 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|      - | 6914 | `		/* Creates a new file, only if it does not already exist.` |
|      - | 6915 | `		* If the file exists, it fails.` |
|      - | 6916 | `		*/` |
|    ! 0 | 6917 | `		iOpen = O_CREAT\|O_EXCL;` |
|  11486 | 6918 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|      - | 6919 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|      - | 6920 | `		 * The file must exist.` |
|      - | 6921 | `		 */` |
|    ! 0 | 6922 | `		iOpen = O_RDWR\|O_TRUNC;` |
|    ! 0 | 6923 | `	}` |
|  21062 | 6924 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|      - | 6925 | `		/* Read+Write access */` |
|   9560 | 6926 | `		iOpen &= ~O_RDONLY;` |
|   9560 | 6927 | `		iOpen \|= O_RDWR;` |
|  16282 | 6928 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|      - | 6929 | `		/* Write only access */` |
|     22 | 6930 | `		iOpen &= ~O_RDONLY;` |
|     22 | 6931 | `		iOpen \|= O_WRONLY;` |
|     11 | 6932 | `	}` |
|  21062 | 6933 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|      - | 6934 | `		/* Append mode */` |
|    ! 0 | 6935 | `		iOpen \|= O_APPEND;` |
|    ! 0 | 6936 | `	}` |
|      - | 6937 | `#ifdef O_TEMP` |
|      - | 6938 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|      - | 6939 | `		/* File is temporary */` |
|      - | 6940 | `		iOpen \|= O_TEMP;` |
|      - | 6941 | `	}` |
|      - | 6942 | `#endif` |
|      - | 6943 | `	/* Open the file now */` |
|  21062 | 6944 | `	fd = open(zPath,iOpen,PH7_UNIX_OPEN_MODE);` |
|  21062 | 6945 | `	if( fd < 0 ){` |
|      - | 6946 | `		/* IO error */` |
|      8 | 6947 | `		return -1;` |
|      - | 6948 | `	}` |
|      - | 6949 | `	/* Save the handle */` |
|  21054 | 6950 | `	*ppHandle = SX_INT_TO_PTR(fd);` |
|  21054 | 6951 | `	return PH7_OK;` |
|  10531 | 6952 |  |
|      - | 6953 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|    836 | 6954 | `static int UnixDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|      - | 6955 |  |
|      - | 6956 | `	DIR *pDir;` |
|      - | 6957 | `	/* Open the target directory */` |
|    836 | 6958 | `	pDir = opendir(zPath);` |
|    836 | 6959 | `	if( pDir == 0 ){` |
|    ! 0 | 6960 | `		SXUNUSED(pResource); /* Compiler warning */` |
|    ! 0 | 6961 | `		return -1;` |
|      - | 6962 | `	}` |
|      - | 6963 | `	/* Save our structure */` |
|    836 | 6964 | `	*ppHandle = pDir;` |
|    836 | 6965 | `	return PH7_OK;` |
|    418 | 6966 |  |
|      - | 6967 | `/* void (*xCloseDir)(void *) */` |
|    836 | 6968 | `static void UnixDir_Close(void *pUserData)` |
|      - | 6969 |  |
|    836 | 6970 | `	closedir((DIR *)pUserData);` |
|    836 | 6971 |  |
|      - | 6972 | `/* void (*xClose)(void *); */` |
|  21054 | 6973 | `static void UnixFile_Close(void *pUserData)` |
|      - | 6974 |  |
|  21054 | 6975 | `	close(SX_PTR_TO_INT(pUserData));` |
|  21054 | 6976 |  |
|      - | 6977 | `/* int (*xReadDir)(void *,ph7_context *) */` |
|   5590 | 6978 | `static int UnixDir_Read(void *pUserData,ph7_context *pCtx)` |
|      - | 6979 |  |
|   5590 | 6980 | `	DIR *pDir = (DIR *)pUserData;` |
|      - | 6981 | `	struct dirent *pEntry;` |
|   5590 | 6982 | `	char *zName = 0; /* cc warning */` |
|   5590 | 6983 | `	sxu32 n = 0;` |
|   3633 | 6984 | `	for(;;){` |
|   7266 | 6985 | `		pEntry = readdir(pDir);` |
|   7266 | 6986 | `		if( pEntry == 0 ){` |
|      - | 6987 | `			/* No more entries to process */` |
|    834 | 6988 | `			return -1;` |
|      - | 6989 | `		}` |
|   6432 | 6990 | `		zName = pEntry->d_name;` |
|   6432 | 6991 | `		n = SyStrlen(zName);` |
|      - | 6992 | `		/* Ignore '.' && '..' */` |
|   6432 | 6993 | `		if( n > sizeof("..")-1 \|\| zName[0] != '.' \|\| ( n == sizeof("..")-1 && zName[1] != '.') ){` |
|   2378 | 6994 | `			break;` |
|      - | 6995 | `		}` |
|      - | 6996 | `		/* Next entry */` |
|      - | 6997 | `	}` |
|      - | 6998 | `	/* Return the current file name */` |
|   4756 | 6999 | `	ph7_result_string(pCtx,zName,(int)n);` |
|   4756 | 7000 | `	return PH7_OK;` |
|   2795 | 7001 |  |
|      - | 7002 | `/* void (*xRewindDir)(void *) */` |
|      2 | 7003 | `static void UnixDir_Rewind(void *pUserData)` |
|      - | 7004 |  |
|      2 | 7005 | `	rewinddir((DIR *)pUserData);` |
|      2 | 7006 |  |
|      - | 7007 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
|  22910 | 7008 | `static ph7_int64 UnixFile_Read(void *pUserData,void *pBuffer,ph7_int64 nDatatoRead)` |
|      - | 7009 |  |
|      - | 7010 | `	ssize_t nRd;` |
|  22910 | 7011 | `	nRd = read(SX_PTR_TO_INT(pUserData),pBuffer,(size_t)nDatatoRead);` |
|  22910 | 7012 | `	if( nRd < 1 ){` |
|      - | 7013 | `		/* EOF or IO error */` |
|  11450 | 7014 | `		return -1;` |
|      - | 7015 | `	}` |
|  11460 | 7016 | `	return (ph7_int64)nRd;` |
|  11455 | 7017 |  |
|      - | 7018 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
|   9598 | 7019 | `static ph7_int64 UnixFile_Write(void *pUserData,const void *pBuffer,ph7_int64 nWrite)` |
|      - | 7020 |  |
|   9598 | 7021 | `	const char *zData = (const char *)pBuffer;` |
|   9598 | 7022 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|      - | 7023 | `	ph7_int64 nCount;` |
|      - | 7024 | `	ssize_t nWr;` |
|   9598 | 7025 | `	nCount = 0;` |
|   9598 | 7026 | `	for(;;){` |
|  19196 | 7027 | `		if( nWrite < 1 ){` |
|   9598 | 7028 | `			break;` |
|      - | 7029 | `		}` |
|   9598 | 7030 | `		nWr = write(fd,zData,(size_t)nWrite);` |
|   9598 | 7031 | `		if( nWr < 1 ){` |
|      - | 7032 | `			/* IO error */` |
|    ! 0 | 7033 | `			break;` |
|      - | 7034 | `		}` |
|   9598 | 7035 | `		nWrite -= nWr;` |
|   9598 | 7036 | `		nCount += nWr;` |
|   9598 | 7037 | `		zData += nWr;` |
|      - | 7038 | `	}` |
|   9598 | 7039 | `	if( nWrite > 0 ){` |
|    ! 0 | 7040 | `		return -1;` |
|      - | 7041 | `	}` |
|   9598 | 7042 | `	return nCount;` |
|   4799 | 7043 |  |
|      - | 7044 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|      6 | 7045 | `static int UnixFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|      - | 7046 |  |
|      - | 7047 | `	off_t iNew;` |
|      6 | 7048 | `	switch(whence){` |
|    ! 0 | 7049 | `	case 1:/*SEEK_CUR*/` |
|    ! 0 | 7050 | `		whence = SEEK_CUR;` |
|    ! 0 | 7051 | `		break;` |
|    ! 0 | 7052 | `	case 2: /* SEEK_END */` |
|    ! 0 | 7053 | `		whence = SEEK_END;` |
|    ! 0 | 7054 | `		break;` |
|      6 | 7055 | `	case 0: /* SEEK_SET */` |
|      - | 7056 | `	default:` |
|      6 | 7057 | `		whence = SEEK_SET;` |
|      6 | 7058 | `		break;` |
|      - | 7059 | `	}` |
|      6 | 7060 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),(off_t)iOfft,whence);` |
|      6 | 7061 | `	if( iNew < 0 ){` |
|    ! 0 | 7062 | `		return -1;` |
|      - | 7063 | `	}` |
|      6 | 7064 | `	return PH7_OK;` |
|      3 | 7065 |  |
|      - | 7066 | `/* int (*xLock)(void *,int) */` |
|      4 | 7067 | `static int UnixFile_Lock(void *pUserData,int lock_type)` |
|      - | 7068 |  |
|      4 | 7069 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|      4 | 7070 | `	int rc = PH7_OK; /* cc warning */` |
|      4 | 7071 | `	if( lock_type < 0 ){` |
|      - | 7072 | `		/* Unlock the file */` |
|    ! 0 | 7073 | `		rc = flock(fd,LOCK_UN);` |
|    ! 0 | 7074 | `	}else{` |
|      4 | 7075 | `		if( lock_type == 1 ){` |
|      - | 7076 | `			/* Exculsive lock */` |
|      2 | 7077 | `			rc = flock(fd,LOCK_EX);` |
|      1 | 7078 | `		}else{` |
|      - | 7079 | `			/* Shared lock */` |
|      2 | 7080 | `			rc = flock(fd,LOCK_SH);` |
|      - | 7081 | `		}` |
|      - | 7082 | `	}` |
|      4 | 7083 | `	return !rc ? PH7_OK : -1;` |
|      - | 7084 |  |
|      - | 7085 | `/* ph7_int64 (*xTell)(void *) */` |
|      6 | 7086 | `static ph7_int64 UnixFile_Tell(void *pUserData)` |
|      - | 7087 |  |
|      - | 7088 | `	off_t iNew;` |
|      6 | 7089 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),0,SEEK_CUR);` |
|      6 | 7090 | `	return (ph7_int64)iNew;` |
|      - | 7091 |  |
|      - | 7092 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|      6 | 7093 | `static int UnixFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|      - | 7094 |  |
|      - | 7095 | `	int rc;` |
|      6 | 7096 | `	rc = ftruncate(SX_PTR_TO_INT(pUserData),(off_t)nOfft);` |
|      6 | 7097 | `	if( rc != 0 ){` |
|    ! 0 | 7098 | `		return -1;` |
|      - | 7099 | `	}` |
|      6 | 7100 | `	return PH7_OK;` |
|      3 | 7101 |  |
|      - | 7102 | `/* int (*xSync)(void *); */` |
|      2 | 7103 | `static int UnixFile_Sync(void *pUserData)` |
|      - | 7104 |  |
|      - | 7105 | `	int rc;` |
|      2 | 7106 | `	rc = fsync(SX_PTR_TO_INT(pUserData));` |
|      2 | 7107 | `	return rc == 0 ? PH7_OK : - 1;` |
|      - | 7108 |  |
|      - | 7109 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|      2 | 7110 | `static int UnixFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|      - | 7111 |  |
|      - | 7112 | `	struct stat st;` |
|      - | 7113 | `	int rc;` |
|      2 | 7114 | `	rc = fstat(SX_PTR_TO_INT(pUserData),&st);` |
|      2 | 7115 | `	if( rc != 0 ){` |
|    ! 0 | 7116 | `	 return -1;` |
|      - | 7117 | `	}` |
|      - | 7118 | `	/* dev */` |
|      2 | 7119 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|      2 | 7120 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|      - | 7121 | `	/* ino */` |
|      2 | 7122 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|      2 | 7123 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|      - | 7124 | `	/* mode */` |
|      2 | 7125 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|      2 | 7126 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|      - | 7127 | `	/* nlink */` |
|      2 | 7128 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|      2 | 7129 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|      - | 7130 | `	/* uid,gid,rdev */` |
|      2 | 7131 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|      2 | 7132 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|      2 | 7133 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|      2 | 7134 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|      2 | 7135 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|      2 | 7136 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|      - | 7137 | `	/* size */` |
|      2 | 7138 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|      2 | 7139 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|      - | 7140 | `	/* atime */` |
|      2 | 7141 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|      2 | 7142 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|      - | 7143 | `	/* mtime */` |
|      2 | 7144 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|      2 | 7145 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|      - | 7146 | `	/* ctime */` |
|      2 | 7147 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|      2 | 7148 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|      - | 7149 | `	/* blksize,blocks */` |
|      2 | 7150 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|      2 | 7151 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|      2 | 7152 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|      2 | 7153 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|      2 | 7154 | `	return PH7_OK;` |
|      1 | 7155 |  |
|      - | 7156 | `/* Export the file:// stream */` |
|      - | 7157 | `static const ph7_io_stream sUnixFileStream = {` |
|      - | 7158 | `	"file", /* Stream name */` |
|      - | 7159 | `	PH7_IO_STREAM_VERSION,` |
|      - | 7160 | `	UnixFile_Open,  /* xOpen */` |
|      - | 7161 | `	UnixDir_Open,   /* xOpenDir */` |
|      - | 7162 | `	UnixFile_Close, /* xClose */` |
|      - | 7163 | `	UnixDir_Close,  /* xCloseDir */` |
|      - | 7164 | `	UnixFile_Read,  /* xRead */` |
|      - | 7165 | `	UnixDir_Read,   /* xReadDir */` |
|      - | 7166 | `	UnixFile_Write, /* xWrite */` |
|      - | 7167 | `	UnixFile_Seek,  /* xSeek */` |
|      - | 7168 | `	UnixFile_Lock,  /* xLock */` |
|      - | 7169 | `	UnixDir_Rewind, /* xRewindDir */` |
|      - | 7170 | `	UnixFile_Tell,  /* xTell */` |
|      - | 7171 | `	UnixFile_Trunc, /* xTrunc */` |
|      - | 7172 | `	UnixFile_Sync,  /* xSeek */` |
|      - | 7173 | `	UnixFile_Stat   /* xStat */` |
|      - | 7174 | `};` |
|      - | 7175 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 7176 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7177 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 7178 | `/*` |
|      - | 7179 | ` * Export the builtin vfs.` |
|      - | 7180 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 7181 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 7182 | ` * Note:` |
|      - | 7183 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 7184 | ` * Note:` |
|      - | 7185 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7186 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 7187 | ` */` |
|   1940 | 7188 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      2 | 7189 |  |
|      - | 7190 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 7191 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 7192 | `	return &null_vfs;` |
|      - | 7193 | `#else` |
|      - | 7194 | `#ifdef __WINNT__` |
|      2 | 7195 | `	return &sWinVfs;` |
|      - | 7196 | `#elif defined(__UNIXES__)` |
|   1940 | 7197 | `	return &sUnixVfs;` |
|      - | 7198 | `#else` |
|      - | 7199 | `	return &null_vfs;` |
|      - | 7200 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 7201 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 7202 | `#else` |
|      - | 7203 | `	return &null_vfs;` |
|      - | 7204 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      2 | 7205 |  |
|      - | 7206 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 7207 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7208 | `/*` |
|      - | 7209 | ` * The following defines are mostly used by the UNIX built and have` |
|      - | 7210 | ` * no particular meaning on windows.` |
|      - | 7211 | ` */` |
|      - | 7212 | `#ifndef STDIN_FILENO` |
|      - | 7213 | `#define STDIN_FILENO	0` |
|      - | 7214 | `#endif` |
|      - | 7215 | `#ifndef STDOUT_FILENO` |
|      - | 7216 | `#define STDOUT_FILENO	1` |
|      - | 7217 | `#endif` |
|      - | 7218 | `#ifndef STDERR_FILENO` |
|      - | 7219 | `#define STDERR_FILENO	2` |
|      - | 7220 | `#endif` |
|      - | 7221 | `/*` |
|      - | 7222 | ` * php:// Accessing various I/O streams` |
|      - | 7223 | ` * According to the PHP langage reference manual` |
|      - | 7224 | ` * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input` |
|      - | 7225 | ` * and output streams, the standard input, output and error file descriptors.` |
|      - | 7226 | ` * php://stdin, php://stdout and php://stderr:` |
|      - | 7227 | ` *  Allow direct access to the corresponding input or output stream of the PHP process.` |
|      - | 7228 | ` *  The stream references a duplicate file descriptor, so if you open php://stdin and later` |
|      - | 7229 | ` *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.` |
|      - | 7230 | ` *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.` |
|      - | 7231 | ` * php://output` |
|      - | 7232 | ` *  php://output is a write-only stream that allows you to write to the output buffer` |
|      - | 7233 | ` *  mechanism in the same way as print and echo.` |
|      - | 7234 | ` */` |
|      - | 7235 | `typedef struct ph7_stream_data ph7_stream_data;` |
|      - | 7236 | `/* Supported IO streams */` |
|      - | 7237 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|      - | 7238 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|      - | 7239 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|      - | 7240 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|      - | 7241 | ` /* The following structure is the private data associated with the php:// stream */` |
|      - | 7242 | `struct ph7_stream_data` |
|      - | 7243 |  |
|      - | 7244 | `	ph7_vm *pVm; /* VM that own this instance */` |
|      - | 7245 | `	int iType;   /* Stream type */` |
|      - | 7246 | `	union{` |
|      - | 7247 | `		void *pHandle; /* Stream handle */` |
|      - | 7248 | `		ph7_output_consumer sConsumer; /* VM output consumer */` |
|      - | 7249 | `	}x;` |
|      - | 7250 | `};` |
|      - | 7251 | `/*` |
|      - | 7252 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|      - | 7253 | ` */` |
|      8 | 7254 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|      1 | 7255 |  |
|      - | 7256 | `	ph7_stream_data *pData;` |
|      9 | 7257 | `	if( pVm == 0 ){` |
|    ! 0 | 7258 | `		return 0;` |
|      - | 7259 | `	}` |
|      - | 7260 | `	/* Allocate a new instance */` |
|      9 | 7261 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|      9 | 7262 | `	if( pData == 0 ){` |
|    ! 0 | 7263 | `		return 0;` |
|      - | 7264 | `	}` |
|      - | 7265 | `	/* Zero the structure */` |
|      9 | 7266 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|      - | 7267 | `	/* Initialize fields */` |
|      9 | 7268 | `	pData->iType = iType;` |
|      9 | 7269 | `	if( iType == PH7_IO_STREAM_OUTPUT ){` |
|      - | 7270 | `		/* Point to the default VM consumer routine. */` |
|      3 | 7271 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|      2 | 7272 | `	}else{` |
|      - | 7273 | `#ifdef __WINNT__` |
|      - | 7274 | `		DWORD nChannel;` |
|      1 | 7275 | `		switch(iType){` |
|      1 | 7276 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|      1 | 7277 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|      - | 7278 | `		default:` |
|      1 | 7279 | `			nChannel = STD_INPUT_HANDLE;` |
|      - | 7280 | `			break;` |
|      - | 7281 | `		}` |
|      1 | 7282 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|      - | 7283 | `#else` |
|      - | 7284 | `		/* Assume an UNIX system */` |
|      6 | 7285 | `		int ifd = STDIN_FILENO;` |
|      6 | 7286 | `		switch(iType){` |
|      2 | 7287 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|      2 | 7288 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|      1 | 7289 | `		default:` |
|      2 | 7290 | `			break;` |
|      - | 7291 | `		}` |
|      6 | 7292 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|      - | 7293 | `#endif` |
|      - | 7294 | `	}` |
|      9 | 7295 | `	pData->pVm = pVm;` |
|      9 | 7296 | `	return pData;` |
|      5 | 7297 |  |
|      - | 7298 | `/*` |
|      - | 7299 | ` * Implementation of the php:// IO streams routines` |
|      - | 7300 | ` * Status:` |
|      - | 7301 | ` *   Stable.` |
|      - | 7302 | ` */` |
|      - | 7303 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|      2 | 7304 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 7305 |  |
|      - | 7306 | `	ph7_stream_data *pData;` |
|      - | 7307 | `	SyString sStream;` |
|      3 | 7308 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|      - | 7309 | `	/* Trim leading and trailing white spaces */` |
|      3 | 7310 | `	SyStringFullTrim(&sStream);` |
|      - | 7311 | `	/* Stream to open */` |
|      3 | 7312 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|    ! 0 | 7313 | `		iMode = PH7_IO_STREAM_STDIN;` |
|      3 | 7314 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|      3 | 7315 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|      1 | 7316 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|    ! 0 | 7317 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|    ! 0 | 7318 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|    ! 0 | 7319 | `		iMode = PH7_IO_STREAM_STDERR;` |
|    ! 0 | 7320 | `	}else{` |
|      - | 7321 | `		/* unknown stream name */` |
|    ! 0 | 7322 | `		return -1;` |
|      - | 7323 | `	}` |
|      - | 7324 | `	/* Create our handle */` |
|      3 | 7325 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|      3 | 7326 | `	if( pData == 0 ){` |
|    ! 0 | 7327 | `		return -1;` |
|      - | 7328 | `	}` |
|      - | 7329 | `	/* Make the handle public */` |
|      3 | 7330 | `	*ppHandle = (void *)pData;` |
|      3 | 7331 | `	return PH7_OK;` |
|      2 | 7332 |  |
|      - | 7333 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|    ! 0 | 7334 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|    ! 0 | 7335 |  |
|    ! 0 | 7336 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    ! 0 | 7337 | `	if( pData == 0 ){` |
|    ! 0 | 7338 | `		return -1;` |
|      - | 7339 | `	}` |
|    ! 0 | 7340 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|      - | 7341 | `		/* Forbidden */` |
|    ! 0 | 7342 | `		return -1;` |
|      - | 7343 | `	}` |
|      - | 7344 | `#ifdef __WINNT__` |
|      - | 7345 | `	{` |
|      - | 7346 | `		DWORD nRd;` |
|      - | 7347 | `		BOOL rc;` |
|    ! 0 | 7348 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    ! 0 | 7349 | `		if( !rc ){` |
|      - | 7350 | `			/* IO error */` |
|    ! 0 | 7351 | `			return -1;` |
|      - | 7352 | `		}` |
|    ! 0 | 7353 | `		return (ph7_int64)nRd;` |
|      - | 7354 | `	}` |
|      - | 7355 | `#elif defined(__UNIXES__)` |
|      - | 7356 | `	{` |
|      - | 7357 | `		ssize_t nRd;` |
|      - | 7358 | `		int fd;` |
|    ! 0 | 7359 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 7360 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|    ! 0 | 7361 | `		if( nRd < 1 ){` |
|    ! 0 | 7362 | `			return -1;` |
|      - | 7363 | `		}` |
|    ! 0 | 7364 | `		return (ph7_int64)nRd;` |
|      - | 7365 | `	}` |
|      - | 7366 | `#else` |
|      - | 7367 | `	return -1;` |
|      - | 7368 | `#endif` |
|    ! 0 | 7369 |  |
|      - | 7370 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|      2 | 7371 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 7372 |  |
|      3 | 7373 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      3 | 7374 | `	if( pData == 0 ){` |
|    ! 0 | 7375 | `		return -1;` |
|      - | 7376 | `	}` |
|      3 | 7377 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|      - | 7378 | `		/* Forbidden */` |
|    ! 0 | 7379 | `		return -1;` |
|      3 | 7380 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|      3 | 7381 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|      - | 7382 | `		int rc;` |
|      - | 7383 | `		/* Call the vm output consumer */` |
|      3 | 7384 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|      3 | 7385 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 7386 | `			return -1;` |
|      - | 7387 | `		}` |
|      3 | 7388 | `		return nWrite;` |
|      - | 7389 | `	}` |
|      - | 7390 | `#ifdef __WINNT__` |
|      - | 7391 | `	{` |
|      - | 7392 | `		DWORD nWr;` |
|      - | 7393 | `		BOOL rc;` |
|    ! 0 | 7394 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|    ! 0 | 7395 | `		if( !rc ){` |
|      - | 7396 | `			/* IO error */` |
|    ! 0 | 7397 | `			return -1;` |
|      - | 7398 | `		}` |
|    ! 0 | 7399 | `		return (ph7_int64)nWr;` |
|      - | 7400 | `	}` |
|      - | 7401 | `#elif defined(__UNIXES__)` |
|      - | 7402 | `	{` |
|      - | 7403 | `		ssize_t nWr;` |
|      - | 7404 | `		int fd;` |
|    ! 0 | 7405 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 7406 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|    ! 0 | 7407 | `		if( nWr < 1 ){` |
|    ! 0 | 7408 | `			return -1;` |
|      - | 7409 | `		}` |
|    ! 0 | 7410 | `		return (ph7_int64)nWr;` |
|      - | 7411 | `	}` |
|      - | 7412 | `#else` |
|      - | 7413 | `	return -1;` |
|      - | 7414 | `#endif` |
|      2 | 7415 |  |
|      - | 7416 | `/* void (*xClose)(void *) */` |
|      2 | 7417 | `static void PHPStreamData_Close(void *pHandle)` |
|      1 | 7418 |  |
|      3 | 7419 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 7420 | `	ph7_vm *pVm;` |
|      3 | 7421 | `	if( pData == 0 ){` |
|    ! 0 | 7422 | `		return;` |
|      - | 7423 | `	}` |
|      3 | 7424 | `	pVm = pData->pVm;` |
|      - | 7425 | `	/* Free the instance */` |
|      3 | 7426 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|      2 | 7427 |  |
|      - | 7428 | `/*` |
|      - | 7429 | ` * Pipe stream implementation for popen/pclose.` |
|      - | 7430 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|      - | 7431 | ` * PHP-compatible process I/O functionality.` |
|      - | 7432 | ` */` |
|      - | 7433 | `typedef struct pipe_private pipe_private;` |
|      - | 7434 | `struct pipe_private` |
|      - | 7435 |  |
|      - | 7436 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|      - | 7437 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|      - | 7438 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|      - | 7439 | `#ifdef __WINNT__` |
|      - | 7440 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|      - | 7441 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|      - | 7442 | `#endif` |
|      - | 7443 | `};` |
|      - | 7444 |  |
|      - | 7445 | `#ifdef __WINNT__` |
|      - | 7446 | `/*` |
|      - | 7447 | ` * Custom Windows popen implementation using CreateProcess.` |
|      - | 7448 | ` * This allows us to properly wait for process completion.` |
|      - | 7449 | ` */` |
|      - | 7450 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|      2 | 7451 |  |
|      2 | 7452 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|      2 | 7453 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|      2 | 7454 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|      - | 7455 | `	SECURITY_ATTRIBUTES sa;` |
|      - | 7456 | `	STARTUPINFOW si;` |
|      - | 7457 | `	PROCESS_INFORMATION pi;` |
|      2 | 7458 | `	WCHAR *zWideCmd = NULL;` |
|      2 | 7459 | `	FILE *pFile = NULL;` |
|      - | 7460 | `	int fd;` |
|      2 | 7461 | `	BOOL bRead = (zMode[0] == 'r');` |
|      - | 7462 |  |
|      - | 7463 | `	/* Set up security attributes for pipe inheritance */` |
|      2 | 7464 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|      2 | 7465 | `	sa.bInheritHandle = TRUE;` |
|      2 | 7466 | `	sa.lpSecurityDescriptor = NULL;` |
|      - | 7467 |  |
|      - | 7468 | `	/* Create pipes for child process I/O */` |
|      2 | 7469 | `	if( bRead ){` |
|      - | 7470 | `		/* Reading from child's stdout */` |
|      2 | 7471 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|    ! 0 | 7472 | `			return NULL;` |
|      - | 7473 | `		}` |
|      - | 7474 | `		/* Ensure read handle is not inherited */` |
|      2 | 7475 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|      2 | 7476 | `		hReadPipe = hChildStdoutRd;` |
|      2 | 7477 | `		*phPipe = hChildStdoutRd;` |
|      2 | 7478 | `	}else{` |
|      - | 7479 | `		/* Writing to child's stdin */` |
|    ! 0 | 7480 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|    ! 0 | 7481 | `			return NULL;` |
|      - | 7482 | `		}` |
|      - | 7483 | `		/* Ensure write handle is not inherited */` |
|    ! 0 | 7484 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|    ! 0 | 7485 | `		hWritePipe = hChildStdinWr;` |
|    ! 0 | 7486 | `		*phPipe = hChildStdinWr;` |
|      - | 7487 | `	}` |
|      - | 7488 |  |
|      - | 7489 | `	/* Convert command to wide string */` |
|      - | 7490 | `	{` |
|      2 | 7491 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|      2 | 7492 | `		if( nLen <= 0 ){` |
|    ! 0 | 7493 | `			goto cleanup_pipes;` |
|      - | 7494 | `		}` |
|      2 | 7495 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|      2 | 7496 | `		if( !zWideCmd ){` |
|    ! 0 | 7497 | `			goto cleanup_pipes;` |
|      - | 7498 | `		}` |
|      2 | 7499 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|      - | 7500 | `	}` |
|      - | 7501 |  |
|      - | 7502 | `	/* Set up process startup info */` |
|      2 | 7503 | `	ZeroMemory(&si, sizeof(si));` |
|      2 | 7504 | `	si.cb = sizeof(si);` |
|      2 | 7505 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|      2 | 7506 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|      2 | 7507 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|      2 | 7508 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|      2 | 7509 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|      - | 7510 |  |
|      2 | 7511 | `	ZeroMemory(&pi, sizeof(pi));` |
|      - | 7512 |  |
|      - | 7513 | `	/* Create the child process */` |
|      2 | 7514 | `	if( !CreateProcessW(` |
|      - | 7515 | `		NULL,           /* Application name */` |
|      - | 7516 | `		zWideCmd,       /* Command line */` |
|      - | 7517 | `		NULL,           /* Process security attributes */` |
|      - | 7518 | `		NULL,           /* Thread security attributes */` |
|      - | 7519 | `		TRUE,           /* Inherit handles */` |
|      - | 7520 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|      - | 7521 | `		NULL,           /* Environment */` |
|      - | 7522 | `		NULL,           /* Current directory */` |
|      - | 7523 | `		&si,            /* Startup info */` |
|      - | 7524 | `		&pi             /* Process info */` |
|      - | 7525 | `	)){` |
|    ! 0 | 7526 | `		goto cleanup_all;` |
|      - | 7527 | `	}` |
|      - | 7528 |  |
|      - | 7529 | `	/* Close handles we don't need in parent */` |
|      2 | 7530 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|      2 | 7531 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|      - | 7532 |  |
|      - | 7533 | `	/* Close thread handle (we only need process handle) */` |
|      2 | 7534 | `	CloseHandle(pi.hThread);` |
|      - | 7535 |  |
|      - | 7536 | `	/* Store process handle for later waiting */` |
|      2 | 7537 | `	*phProcess = pi.hProcess;` |
|      - | 7538 |  |
|      - | 7539 | `	/* Convert OS handle to C file descriptor, then to FILE* */` |
|      2 | 7540 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|      - | 7541 | `	                     bRead ? _O_RDONLY \| _O_TEXT : _O_WRONLY \| _O_TEXT);` |
|      2 | 7542 | `	if( fd == -1 ){` |
|    ! 0 | 7543 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 7544 | `		*phProcess = NULL;` |
|    ! 0 | 7545 | `		goto cleanup_all;` |
|      - | 7546 | `	}` |
|      - | 7547 |  |
|      2 | 7548 | `	pFile = _fdopen(fd, zMode);` |
|      2 | 7549 | `	if( !pFile ){` |
|    ! 0 | 7550 | `		_close(fd); /* This will also close the underlying handle */` |
|    ! 0 | 7551 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 7552 | `		*phProcess = NULL;` |
|    ! 0 | 7553 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    ! 0 | 7554 | `		return NULL;` |
|      - | 7555 | `	}` |
|      - | 7556 |  |
|      2 | 7557 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      2 | 7558 | `	return pFile;` |
|      - | 7559 |  |
|      - | 7560 | `cleanup_all:` |
|    ! 0 | 7561 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      - | 7562 | `cleanup_pipes:` |
|    ! 0 | 7563 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|    ! 0 | 7564 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|    ! 0 | 7565 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|    ! 0 | 7566 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|    ! 0 | 7567 | `	return NULL;` |
|      2 | 7568 |  |
|      - | 7569 |  |
|      - | 7570 | `/*` |
|      - | 7571 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|      - | 7572 | ` */` |
|      - | 7573 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|      2 | 7574 |  |
|      2 | 7575 | `	DWORD dwExitCode = 0;` |
|      - | 7576 | `	int status;` |
|      - | 7577 |  |
|      - | 7578 | `	/* Close the FILE* (this closes the pipe) */` |
|      2 | 7579 | `	fclose(pFile);` |
|      - | 7580 |  |
|      2 | 7581 | `	if( hProcess ){` |
|      - | 7582 | `		/* Wait for the process to complete */` |
|      2 | 7583 | `		WaitForSingleObject(hProcess, INFINITE);` |
|      - | 7584 |  |
|      2 | 7585 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|      2 | 7586 | `			status = (int)dwExitCode;` |
|      2 | 7587 | `		}else{` |
|    ! 0 | 7588 | `			status = -1;` |
|      - | 7589 | `		}` |
|      - | 7590 |  |
|      - | 7591 | `		/* Close process handle */` |
|      2 | 7592 | `		CloseHandle(hProcess);` |
|      2 | 7593 | `	}else{` |
|    ! 0 | 7594 | `		status = -1;` |
|      - | 7595 | `	}` |
|      - | 7596 |  |
|      2 | 7597 | `	return status;` |
|      2 | 7598 |  |
|      - | 7599 | `#endif /* __WINNT__ */` |
|      - | 7600 | `/*` |
|      - | 7601 | ` * Open a pipe to a process.` |
|      - | 7602 | ` * This is called internally by popen(), not through the stream device interface.` |
|      - | 7603 | ` */` |
|   1924 | 7604 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|      2 | 7605 |  |
|      - | 7606 | `	pipe_private *pPipe;` |
|      - | 7607 | `	FILE *pFile;` |
|   1926 | 7608 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|    ! 0 | 7609 | `		return 0;` |
|      - | 7610 | `	}` |
|      - | 7611 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|   1926 | 7612 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|    ! 0 | 7613 | `		return 0;` |
|      - | 7614 | `	}` |
|      - | 7615 | `	/* Open the pipe using system popen */` |
|      - | 7616 | `#ifdef __WINNT__` |
|      - | 7617 | `	{` |
|      - | 7618 | `		/* Build cmd.exe command wrapper */` |
|      2 | 7619 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|      2 | 7620 | `		const char *zShellSuffix = "\"";` |
|      2 | 7621 | `		size_t nPrefix = strlen(zShellPrefix);` |
|      2 | 7622 | `		size_t nSuffix = strlen(zShellSuffix);` |
|      2 | 7623 | `		size_t nCmd = strlen(zCommand);` |
|      2 | 7624 | `		size_t nQuotes = 0;` |
|      2 | 7625 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      2 | 7626 | `			if (zCommand[i] == '"') nQuotes++;` |
|      2 | 7627 | `		}` |
|      2 | 7628 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|      2 | 7629 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|      2 | 7630 | `		if (zCmdEsc == NULL) {` |
|    ! 0 | 7631 | `			return 0;` |
|      - | 7632 | `		}` |
|      - | 7633 | `		/* Escape quotes in command */` |
|      2 | 7634 | `		size_t j = 0;` |
|      2 | 7635 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      2 | 7636 | `			char ch = zCommand[i];` |
|      2 | 7637 | `			if (ch == '"') {` |
|      1 | 7638 | `				zCmdEsc[j++] = '^';` |
|      1 | 7639 | `				zCmdEsc[j++] = '"';` |
|      1 | 7640 | `			} else {` |
|      2 | 7641 | `				zCmdEsc[j++] = ch;` |
|      - | 7642 | `			}` |
|      2 | 7643 | `		}` |
|      2 | 7644 | `		zCmdEsc[j] = '\0';` |
|      2 | 7645 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|      2 | 7646 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|      2 | 7647 | `		if (zWinCmd == NULL) {` |
|    ! 0 | 7648 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 7649 | `			return 0;` |
|      - | 7650 | `		}` |
|      2 | 7651 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|      2 | 7652 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|      2 | 7653 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|      2 | 7654 | `		zWinCmd[nTotal - 1] = '\0';` |
|      - | 7655 | `		/* Allocate pipe structure early so we can store handles */` |
|      2 | 7656 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|      2 | 7657 | `		if( pPipe == 0 ){` |
|    ! 0 | 7658 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 7659 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|    ! 0 | 7660 | `			return 0;` |
|      - | 7661 | `		}` |
|      - | 7662 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|      2 | 7663 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|      2 | 7664 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|      2 | 7665 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|      2 | 7666 | `		if( pFile == 0 ){` |
|    ! 0 | 7667 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|    ! 0 | 7668 | `			return 0;` |
|      - | 7669 | `		}` |
|      - | 7670 | `		/* Initialize remaining fields */` |
|      2 | 7671 | `		pPipe->pFile = pFile;` |
|      2 | 7672 | `		pPipe->pVm = pVm;` |
|      2 | 7673 | `		pPipe->iMode = zMode[0];` |
|      - | 7674 | `	}` |
|      - | 7675 | `#else /* Unix */` |
|   1924 | 7676 | `	pFile = popen(zCommand, zMode);` |
|   1924 | 7677 | `	if( pFile == 0 ){` |
|    ! 0 | 7678 | `		return 0;` |
|      - | 7679 | `	}` |
|      - | 7680 | `	/* Allocate pipe private structure */` |
|   1924 | 7681 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|   1924 | 7682 | `	if( pPipe == 0 ){` |
|      - | 7683 | `		/* Out of memory, close the pipe */` |
|    ! 0 | 7684 | `		pclose(pFile);` |
|    ! 0 | 7685 | `		return 0;` |
|      - | 7686 | `	}` |
|      - | 7687 | `	/* Initialize the structure */` |
|   1924 | 7688 | `	pPipe->pFile = pFile;` |
|   1924 | 7689 | `	pPipe->pVm = pVm;` |
|   1924 | 7690 | `	pPipe->iMode = zMode[0];` |
|      - | 7691 | `#endif` |
|   1926 | 7692 | `	return pPipe;` |
|    964 | 7693 |  |
|      - | 7694 | `/*` |
|      - | 7695 | ` * Close a pipe and return the exit status of the process.` |
|      - | 7696 | ` * Returns the exit status, or -1 on error.` |
|      - | 7697 | ` */` |
|   1924 | 7698 | `static int PipeClose(pipe_private *pPipe)` |
|      2 | 7699 |  |
|      - | 7700 | `	int status;` |
|      - | 7701 | `	ph7_vm *pVm;` |
|   1926 | 7702 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 7703 | `		return -1;` |
|      - | 7704 | `	}` |
|   1926 | 7705 | `	pVm = pPipe->pVm;` |
|      - | 7706 | `	/* Close the pipe and get exit status */` |
|      - | 7707 | `#ifdef __WINNT__` |
|      - | 7708 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|      2 | 7709 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|      - | 7710 | `#else` |
|   1924 | 7711 | `	status = pclose(pPipe->pFile);` |
|      - | 7712 | `	/* On Unix, pclose returns the status from waitpid, need to extract exit code */` |
|   1924 | 7713 | `	if( status != -1 ){` |
|   1924 | 7714 | `		if( WIFEXITED(status) ){` |
|   1924 | 7715 | `			status = WEXITSTATUS(status);` |
|    962 | 7716 | `		}else if( WIFSIGNALED(status) ){` |
|      - | 7717 | `			/* Process was killed by a signal - use shell convention: 128 + signal number */` |
|    ! 0 | 7718 | `			status = 128 + WTERMSIG(status);` |
|    ! 0 | 7719 | `		}else{` |
|      - | 7720 | `			/* Unknown termination reason */` |
|    ! 0 | 7721 | `			status = -1;` |
|      - | 7722 | `		}` |
|    962 | 7723 | `	}` |
|      - | 7724 | `#endif` |
|      - | 7725 | `	/* Free the structure */` |
|   1926 | 7726 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|   1926 | 7727 | `	return status;` |
|    964 | 7728 |  |
|      - | 7729 | `/*` |
|      - | 7730 | ` * Pipe stream xClose implementation.` |
|      - | 7731 | ` * Note: This is called by fclose(), not pclose().` |
|      - | 7732 | ` * It closes the pipe but does not return the exit status.` |
|      - | 7733 | ` */` |
|     14 | 7734 | `static void PipeStream_Close(void *pHandle)` |
|      1 | 7735 |  |
|     15 | 7736 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|     15 | 7737 | `	if( pPipe ){` |
|     15 | 7738 | `		PipeClose(pPipe);` |
|      7 | 7739 | `	}` |
|     15 | 7740 |  |
|      - | 7741 | `/*` |
|      - | 7742 | ` * Pipe stream xRead implementation.` |
|      - | 7743 | ` */` |
|   2720 | 7744 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|      1 | 7745 |  |
|   2721 | 7746 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 7747 | `	size_t nRead;` |
|   2721 | 7748 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 7749 | `		return -1;` |
|      - | 7750 | `	}` |
|   2721 | 7751 | `	if( pPipe->iMode != 'r' ){` |
|      - | 7752 | `		/* Cannot read from a write-only pipe */` |
|    ! 0 | 7753 | `		return -1;` |
|      - | 7754 | `	}` |
|   2721 | 7755 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
|   2721 | 7756 | `	if( nRead == 0 ){` |
|   1937 | 7757 | `		if( feof(pPipe->pFile) ){` |
|   1937 | 7758 | `			return 0; /* EOF */` |
|      - | 7759 | `		}` |
|    ! 0 | 7760 | `		return -1; /* Error */` |
|      - | 7761 | `	}` |
|    785 | 7762 | `	return (ph7_int64)nRead;` |
|   1361 | 7763 |  |
|      - | 7764 | `/*` |
|      - | 7765 | ` * Pipe stream xWrite implementation.` |
|      - | 7766 | ` */` |
|      2 | 7767 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|    ! 0 | 7768 |  |
|      2 | 7769 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 7770 | `	size_t nWritten;` |
|      2 | 7771 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 7772 | `		return -1;` |
|      - | 7773 | `	}` |
|      2 | 7774 | `	if( pPipe->iMode != 'w' ){` |
|      - | 7775 | `		/* Cannot write to a read-only pipe */` |
|    ! 0 | 7776 | `		return -1;` |
|      - | 7777 | `	}` |
|      2 | 7778 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|      2 | 7779 | `	if( nWritten == 0 && nWrite > 0 ){` |
|    ! 0 | 7780 | `		return -1; /* Error */` |
|      - | 7781 | `	}` |
|      2 | 7782 | `	return (ph7_int64)nWritten;` |
|      1 | 7783 |  |
|      - | 7784 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|      - | 7785 | `static const ph7_io_stream sPipe_Stream = {` |
|      - | 7786 | `	"pipe",` |
|      - | 7787 | `	PH7_IO_STREAM_VERSION,` |
|      - | 7788 |  |
|      - | 7789 |  |
|      - | 7790 | `	PipeStream_Close,  /* xClose */` |
|      - | 7791 |  |
|      - | 7792 | `	PipeStream_Read,   /* xRead */` |
|      - | 7793 |  |
|      - | 7794 | `	PipeStream_Write,  /* xWrite */` |
|      - | 7795 |  |
|      - | 7796 |  |
|      - | 7797 |  |
|      - | 7798 |  |
|      - | 7799 |  |
|      - | 7800 |  |
|      - | 7801 |  |
|      - | 7802 | `};` |
|      - | 7803 | `/*` |
|      - | 7804 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|      - | 7805 | ` * FALSE otherwise.` |
|      - | 7806 | ` */` |
|   1910 | 7807 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|      2 | 7808 |  |
|   1912 | 7809 | `	return pStream == &sPipe_Stream;` |
|      2 | 7810 |  |
|      - | 7811 | `/*` |
|      - | 7812 | ` * resource popen(string $command, string $mode)` |
|      - | 7813 | ` *  Opens process file pointer.` |
|      - | 7814 | ` * Parameters` |
|      - | 7815 | ` *  $command` |
|      - | 7816 | ` *   The command to execute. Passed to the system shell.` |
|      - | 7817 | ` *  $mode` |
|      - | 7818 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|      - | 7819 | ` *   'r' - Open for reading (read from the command's stdout).` |
|      - | 7820 | ` *   'w' - Open for writing (write to the command's stdin).` |
|      - | 7821 | ` * Return` |
|      - | 7822 | ` *  Returns a file pointer on success, or FALSE on error.` |
|      - | 7823 | ` */` |
|   1924 | 7824 | `static int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7825 |  |
|      - | 7826 | `	const char *zCommand, *zMode;` |
|      - | 7827 | `	pipe_private *pPipe;` |
|      - | 7828 | `	io_private *pDev;` |
|      - | 7829 | `	int nCmdLen, nModeLen;` |
|   1926 | 7830 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 7831 | `		/* Missing/Invalid arguments, return FALSE */` |
|    ! 0 | 7832 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a command string and mode");` |
|    ! 0 | 7833 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7834 | `		return PH7_OK;` |
|      - | 7835 | `	}` |
|      - | 7836 | `	/* Extract the command and mode */` |
|   1926 | 7837 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|   1926 | 7838 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|   1926 | 7839 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 7840 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty command");` |
|    ! 0 | 7841 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7842 | `		return PH7_OK;` |
|      - | 7843 | `	}` |
|   1926 | 7844 | `	if( nModeLen < 1 \|\| (zMode[0] != 'r' && zMode[0] != 'w') ){` |
|    ! 0 | 7845 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Invalid mode, expected 'r' or 'w'");` |
|    ! 0 | 7846 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7847 | `		return PH7_OK;` |
|      - | 7848 | `	}` |
|      - | 7849 | `	/* Open the pipe */` |
|   1926 | 7850 | `	pPipe = PipeOpen(pCtx->pVm, zCommand, zMode);` |
|   1926 | 7851 | `	if( pPipe == 0 ){` |
|      - | 7852 | `		/* Failed to open pipe */` |
|    ! 0 | 7853 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7854 | `		return PH7_OK;` |
|      - | 7855 | `	}` |
|      - | 7856 | `	/* Allocate an io_private instance to wrap the pipe */` |
|   1926 | 7857 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|   1926 | 7858 | `	if( pDev == 0 ){` |
|    ! 0 | 7859 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|    ! 0 | 7860 | `		PipeClose(pPipe);` |
|    ! 0 | 7861 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7862 | `		return PH7_OK;` |
|      - | 7863 | `	}` |
|      - | 7864 | `	/* Initialize the io_private structure */` |
|   1926 | 7865 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|   1926 | 7866 | `	pDev->pHandle = pPipe;` |
|      - | 7867 | `	/* Return the io_private instance as a resource */` |
|   1926 | 7868 | `	ph7_result_resource(pCtx, pDev);` |
|   1926 | 7869 | `	return PH7_OK;` |
|    964 | 7870 |  |
|      - | 7871 | `/*` |
|      - | 7872 | ` * int pclose(resource $handle)` |
|      - | 7873 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|      - | 7874 | ` * Parameters` |
|      - | 7875 | ` *  $handle` |
|      - | 7876 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|      - | 7877 | ` * Return` |
|      - | 7878 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|      - | 7879 | ` */` |
|   1910 | 7880 | `static int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7881 |  |
|      - | 7882 | `	const ph7_io_stream *pStream;` |
|      - | 7883 | `	pipe_private *pPipe;` |
|      - | 7884 | `	io_private *pDev;` |
|      - | 7885 | `	int status;` |
|   1912 | 7886 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 7887 | `		/* Missing/Invalid arguments, return -1 */` |
|    ! 0 | 7888 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 7889 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7890 | `		return PH7_OK;` |
|      - | 7891 | `	}` |
|      - | 7892 | `	/* Extract our private data */` |
|   1912 | 7893 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 7894 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1912 | 7895 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 7896 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 7897 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7898 | `		return PH7_OK;` |
|      - | 7899 | `	}` |
|      - | 7900 | `	/* Point to the target IO stream device */` |
|   1912 | 7901 | `	pStream = pDev->pStream;` |
|   1912 | 7902 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|    ! 0 | 7903 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|    ! 0 | 7904 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 7905 | `		return PH7_OK;` |
|      - | 7906 | `	}` |
|      - | 7907 | `	/* Get the pipe handle */` |
|   1912 | 7908 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|      - | 7909 | `	/* Close the pipe and get exit status */` |
|   1912 | 7910 | `	status = PipeClose(pPipe);` |
|      - | 7911 | `	/* Release the IO private structure */` |
|   1912 | 7912 | `	ReleaseIOPrivate(pCtx, pDev);` |
|      - | 7913 | `	/* Invalidate the resource handle */` |
|   1912 | 7914 | `	ph7_value_release(apArg[0]);` |
|      - | 7915 | `	/* Return the exit status */` |
|   1912 | 7916 | `	ph7_result_int(pCtx, status);` |
|   1912 | 7917 | `	return PH7_OK;` |
|    957 | 7918 |  |
|      - | 7919 | `/* Export the php:// stream */` |
|      - | 7920 | `static const ph7_io_stream sPHP_Stream = {` |
|      - | 7921 | `	"php",` |
|      - | 7922 | `	PH7_IO_STREAM_VERSION,` |
|      - | 7923 | `	PHPStreamData_Open,  /* xOpen */` |
|      - | 7924 |  |
|      - | 7925 | `	PHPStreamData_Close, /* xClose */` |
|      - | 7926 |  |
|      - | 7927 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 7928 |  |
|      - | 7929 | `	PHPStreamData_Write, /* xWrite */` |
|      - | 7930 |  |
|      - | 7931 |  |
|      - | 7932 |  |
|      - | 7933 |  |
|      - | 7934 |  |
|      - | 7935 |  |
|      - | 7936 |  |
|      - | 7937 | `};` |
|      - | 7938 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7939 | `/*` |
|      - | 7940 | ` * Return TRUE if we are dealing with the php:// stream.` |
|      - | 7941 | ` * FALSE otherwise.` |
|      - | 7942 | ` */` |
|     62 | 7943 | `static int is_php_stream(const ph7_io_stream *pStream)` |
|      1 | 7944 |  |
|      - | 7945 | `#ifndef PH7_DISABLE_DISK_IO` |
|     63 | 7946 | `	return pStream == &sPHP_Stream;` |
|      - | 7947 | `#else` |
|      - | 7948 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 7949 | `	return 0;` |
|      - | 7950 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      1 | 7951 |  |
|      - | 7952 |  |
|      - | 7953 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 7954 | `/*` |
|      - | 7955 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 7956 | ` * [i.e: file://,php://].` |
|      - | 7957 | ` * Note:` |
|      - | 7958 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 7959 | ` *  defined then this function is a no-op.` |
|      - | 7960 | ` */` |
|   1672 | 7961 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      2 | 7962 |  |
|      - | 7963 | `	/*` |
|      - | 7964 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 7965 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 7966 | `	 */` |
|      - | 7967 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7968 | `	/* VFS: disk I/O related functions */` |
|      - | 7969 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 7970 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 7971 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 7972 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 7973 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 7974 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 7975 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 7976 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 7977 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 7978 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 7979 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 7980 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 7981 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 7982 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 7983 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 7984 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 7985 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 7986 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 7987 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 7988 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 7989 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 7990 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 7991 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 7992 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 7993 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 7994 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 7995 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 7996 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 7997 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 7998 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 7999 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 8000 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 8001 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 8002 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 8003 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 8004 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 8005 | `		{"link",        PH7_vfs_link     },` |
|      - | 8006 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 8007 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 8008 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 8009 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 8010 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 8011 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 8012 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 8013 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 8014 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 8015 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 8016 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 8017 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 8018 | `	};` |
|      - | 8019 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 8020 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 8021 | `	 */` |
|      - | 8022 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 8023 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 8024 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 8025 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 8026 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 8027 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 8028 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 8029 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 8030 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 8031 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 8032 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 8033 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 8034 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 8035 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 8036 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 8037 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 8038 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 8039 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 8040 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 8041 | `		{"file",      PH7_builtin_file   },` |
|      - | 8042 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 8043 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 8044 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 8045 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 8046 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 8047 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 8048 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 8049 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 8050 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 8051 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 8052 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 8053 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 8054 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 8055 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 8056 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 8057 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8058 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 8059 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 8060 | `	};` |
|   1674 | 8061 | `	const ph7_io_stream *pFileStream = 0;` |
|   1674 | 8062 | `	sxu32 n = 0;` |
|      - | 8063 | `	/* Register disk-related functions */` |
|  81930 | 8064 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
|  80258 | 8065 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
|  40130 | 8066 | `	}` |
|  60194 | 8067 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
|  58522 | 8068 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  29262 | 8069 | `	}` |
|      - | 8070 | `#else` |
|      - | 8071 | `	SXUNUSED(pVm);` |
|      - | 8072 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 8073 |  |
|      - | 8074 | `	/*` |
|      - | 8075 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 8076 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 8077 | `	 */` |
|      - | 8078 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 8079 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 8080 | `		/* Path processing */` |
|      - | 8081 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 8082 | `		{"basename",    PH7_builtin_basename },` |
|      - | 8083 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 8084 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 8085 | `		{"fnmatch",     PH7_builtin_fnmatch  },` |
|      - | 8086 | `		/* ZIP processing */` |
|      - | 8087 | `		{"zip_open",    PH7_builtin_zip_open },` |
|      - | 8088 | `		{"zip_close",   PH7_builtin_zip_close},` |
|      - | 8089 | `		{"zip_read",    PH7_builtin_zip_read },` |
|      - | 8090 | `		{"zip_entry_open", PH7_builtin_zip_entry_open },` |
|      - | 8091 | `		{"zip_entry_close",PH7_builtin_zip_entry_close},` |
|      - | 8092 | `		{"zip_entry_name", PH7_builtin_zip_entry_name },` |
|      - | 8093 | `		{"zip_entry_filesize",      PH7_builtin_zip_entry_filesize       },` |
|      - | 8094 | `		{"zip_entry_compressedsize",PH7_builtin_zip_entry_compressedsize },` |
|      - | 8095 | `		{"zip_entry_read", PH7_builtin_zip_entry_read },` |
|      - | 8096 | `		{"zip_entry_reset_read_cursor",PH7_builtin_zip_entry_reset_read_cursor},` |
|      - | 8097 | `		{"zip_entry_compressionmethod",PH7_builtin_zip_entry_compressionmethod}` |
|      - | 8098 | `	};` |
|  28426 | 8099 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  26754 | 8100 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|  13378 | 8101 | `	}` |
|      - | 8102 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 8103 |  |
|      - | 8104 | `	/* Install streams if disk I/O is enabled */` |
|      - | 8105 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 8106 | `#ifdef __WINNT__` |
|      2 | 8107 | `	pFileStream = &sWinFileStream;` |
|      - | 8108 | `#elif defined(__UNIXES__)` |
|   1672 | 8109 | `	pFileStream = &sUnixFileStream;` |
|      - | 8110 | `#endif` |
|      - | 8111 | `	/* Install the php:// stream */` |
|   1674 | 8112 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   1674 | 8113 | `	if( pFileStream ){` |
|      - | 8114 | `		/* Install the file:// stream */` |
|   1674 | 8115 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|    836 | 8116 | `	}` |
|      - | 8117 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 8118 |  |
|   1674 | 8119 | `	return SXRET_OK;` |
|      2 | 8120 |  |
|      - | 8121 | `/*` |
|      - | 8122 | ` * Export the STDIN handle.` |
|      - | 8123 | ` */` |
|      2 | 8124 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|      1 | 8125 |  |
|      - | 8126 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 8127 | `	if( pVm->pStdin == 0  ){` |
|      - | 8128 | `		io_private *pIn;` |
|      - | 8129 | `		/* Allocate an IO private instance */` |
|      3 | 8130 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 8131 | `		if( pIn == 0 ){` |
|    ! 0 | 8132 | `			return 0;` |
|      - | 8133 | `		}` |
|      3 | 8134 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|      - | 8135 | `		/* Initialize the handle */` |
|      3 | 8136 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|      - | 8137 | `		/* Install the STDIN stream */` |
|      3 | 8138 | `		pVm->pStdin = pIn;` |
|      3 | 8139 | `		return pIn;` |
|    ! 0 | 8140 | `	}else{` |
|      - | 8141 | `		/* NULL or STDIN */` |
|    ! 0 | 8142 | `		return pVm->pStdin;` |
|      - | 8143 | `	}` |
|      - | 8144 | `#else` |
|      - | 8145 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 8146 | `	return 0;` |
|      - | 8147 | `#endif` |
|      2 | 8148 |  |
|      - | 8149 | `/*` |
|      - | 8150 | ` * Export the STDOUT handle.` |
|      - | 8151 | ` */` |
|      2 | 8152 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|      1 | 8153 |  |
|      - | 8154 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 8155 | `	if( pVm->pStdout == 0  ){` |
|      - | 8156 | `		io_private *pOut;` |
|      - | 8157 | `		/* Allocate an IO private instance */` |
|      3 | 8158 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 8159 | `		if( pOut == 0 ){` |
|    ! 0 | 8160 | `			return 0;` |
|      - | 8161 | `		}` |
|      3 | 8162 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|      - | 8163 | `		/* Initialize the handle */` |
|      3 | 8164 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|      - | 8165 | `		/* Install the STDOUT stream */` |
|      3 | 8166 | `		pVm->pStdout = pOut;` |
|      3 | 8167 | `		return pOut;` |
|    ! 0 | 8168 | `	}else{` |
|      - | 8169 | `		/* NULL or STDOUT */` |
|    ! 0 | 8170 | `		return pVm->pStdout;` |
|      - | 8171 | `	}` |
|      - | 8172 | `#else` |
|      - | 8173 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 8174 | `	return 0;` |
|      - | 8175 | `#endif` |
|      2 | 8176 |  |
|      - | 8177 | `/*` |
|      - | 8178 | ` * Export the STDERR handle.` |
|      - | 8179 | ` */` |
|      2 | 8180 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|      1 | 8181 |  |
|      - | 8182 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 8183 | `	if( pVm->pStderr == 0  ){` |
|      - | 8184 | `		io_private *pErr;` |
|      - | 8185 | `		/* Allocate an IO private instance */` |
|      3 | 8186 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 8187 | `		if( pErr == 0 ){` |
|    ! 0 | 8188 | `			return 0;` |
|      - | 8189 | `		}` |
|      3 | 8190 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|      - | 8191 | `		/* Initialize the handle */` |
|      3 | 8192 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|      - | 8193 | `		/* Install the STDERR stream */` |
|      3 | 8194 | `		pVm->pStderr = pErr;` |
|      3 | 8195 | `		return pErr;` |
|    ! 0 | 8196 | `	}else{` |
|      - | 8197 | `		/* NULL or STDERR */` |
|    ! 0 | 8198 | `		return pVm->pStderr;` |
|      - | 8199 | `	}` |
|      - | 8200 | `#else` |
|      - | 8201 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 8202 | `	return 0;` |
|      - | 8203 | `#endif` |
|      2 | 8204 |  |
|      - | 8205 |  |
