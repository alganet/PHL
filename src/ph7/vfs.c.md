# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2851/4164 lines (68.47%)

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
|   8264 |   61 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   62 |  |
|      - |   63 | `	const char *zPath;` |
|      - |   64 | `	ph7_vfs *pVfs;` |
|      - |   65 | `	int rc;` |
|   8266 |   66 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |   67 | `		/* Missing/Invalid argument,return FALSE */` |
|      8 |   68 | `		ph7_result_bool(pCtx,0);` |
|      8 |   69 | `		return PH7_OK;` |
|      - |   70 | `	}` |
|      - |   71 | `	/* Point to the underlying vfs */` |
|   8260 |   72 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   8260 |   73 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |   74 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |   75 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |   76 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |   77 | `			ph7_function_name(pCtx)` |
|      - |   78 | `			);` |
|    ! 0 |   79 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |   80 | `		return PH7_OK;` |
|      - |   81 | `	}` |
|      - |   82 | `	/* Point to the desired directory */` |
|   8260 |   83 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |   84 | `	/* Perform the requested operation */` |
|   8260 |   85 | `	rc = pVfs->xChdir(zPath);` |
|      - |   86 | `	/* IO return value */` |
|   8260 |   87 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   8260 |   88 | `	return PH7_OK;` |
|   4134 |   89 |  |
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
|   4932 |  209 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  210 |  |
|      - |  211 | `	const char *zPath;` |
|      - |  212 | `	ph7_vfs *pVfs;` |
|      - |  213 | `	int rc;` |
|   4934 |  214 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  215 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  216 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  217 | `		return PH7_OK;` |
|      - |  218 | `	}` |
|      - |  219 | `	/* Point to the underlying vfs */` |
|   4934 |  220 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   4934 |  221 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  222 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  223 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  224 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  225 | `			ph7_function_name(pCtx)` |
|      - |  226 | `			);` |
|    ! 0 |  227 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  228 | `		return PH7_OK;` |
|      - |  229 | `	}` |
|      - |  230 | `	/* Point to the desired directory */` |
|   4934 |  231 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  232 | `	/* Perform the requested operation */` |
|   4934 |  233 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  234 | `	/* IO return value */` |
|   4934 |  235 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   4934 |  236 | `	return PH7_OK;` |
|   2468 |  237 |  |
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
|  16952 |  472 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  473 |  |
|      - |  474 | `	const char *zPath;` |
|      - |  475 | `	ph7_vfs *pVfs;` |
|      - |  476 | `	int rc;` |
|  16954 |  477 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  478 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  479 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  480 | `		return PH7_OK;` |
|      - |  481 | `	}` |
|      - |  482 | `	/* Point to the underlying vfs */` |
|  16954 |  483 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  16954 |  484 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  485 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  486 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  487 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  488 | `			ph7_function_name(pCtx)` |
|      - |  489 | `			);` |
|    ! 0 |  490 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  491 | `		return PH7_OK;` |
|      - |  492 | `	}` |
|      - |  493 | `	/* Point to the desired directory */` |
|  16954 |  494 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  495 | `	/* Perform the requested operation */` |
|  16954 |  496 | `	rc = pVfs->xUnlink(zPath);` |
|      - |  497 | `	/* IO return value */` |
|  16954 |  498 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  16954 |  499 | `	return PH7_OK;` |
|   8478 |  500 |  |
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
|   3364 |  903 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  904 |  |
|      - |  905 | `	const char *zPath;` |
|      - |  906 | `	ph7_vfs *pVfs;` |
|      - |  907 | `	int rc;` |
|   3366 |  908 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  909 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  910 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  911 | `		return PH7_OK;` |
|      - |  912 | `	}` |
|      - |  913 | `	/* Point to the underlying vfs */` |
|   3366 |  914 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   3366 |  915 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - |  916 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  917 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  918 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  919 | `			ph7_function_name(pCtx)` |
|      - |  920 | `			);` |
|    ! 0 |  921 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  922 | `		return PH7_OK;` |
|      - |  923 | `	}` |
|      - |  924 | `	/* Point to the desired directory */` |
|   3366 |  925 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  926 | `	/* Perform the requested operation */` |
|   3366 |  927 | `	rc = pVfs->xIsfile(zPath);` |
|      - |  928 | `	/* IO return value */` |
|   3366 |  929 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   3366 |  930 | `	return PH7_OK;` |
|   1684 |  931 |  |
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
|   6726 | 1550 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      2 | 1551 |  |
|   6728 | 1552 | `	const char *zPtr,*zEnd = &zPath[nByte - 1];` |
|      - | 1553 | `	SyString *pCur;` |
|      - | 1554 | `	int c,d;` |
|   6728 | 1555 | `	c = d = '/';` |
|      - | 1556 | `#ifdef __WINNT__` |
|      2 | 1557 | `	d = '\\';` |
|      - | 1558 | `#endif` |
|      - | 1559 | `	/* Zero the structure */` |
|   6728 | 1560 | `	SyZero(pOut,sizeof(path_info));` |
|      - | 1561 | `	/* Handle special case */` |
|   6728 | 1562 | `	if( nByte == sizeof(char) && ( (int)zPath[0] == c \|\| (int)zPath[0] == d ) ){` |
|      - | 1563 | `#ifdef __WINNT__` |
|    ! 0 | 1564 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1565 | `#else` |
|    ! 0 | 1566 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1567 | `#endif` |
|    ! 0 | 1568 | `		return SXRET_OK;` |
|      - | 1569 | `	}` |
|      - | 1570 | `	/* Extract the basename */` |
| 161899 | 1571 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
| 151810 | 1572 | `		zEnd--;` |
|      2 | 1573 | `	}` |
|   6728 | 1574 | `	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|   6728 | 1575 | `	zEnd = &zPath[nByte];` |
|      - | 1576 | `	/* dirname */` |
|   6728 | 1577 | `	pCur = &pOut->sDir;` |
|   6728 | 1578 | `	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);` |
|   6728 | 1579 | `	if( pCur->nByte > 1 ){` |
|  13454 | 1580 | `		SyStringTrimTrailingChar(pCur,'/');` |
|      - | 1581 | `#ifdef __WINNT__` |
|      2 | 1582 | `		SyStringTrimTrailingChar(pCur,'\\');` |
|      - | 1583 | `#endif` |
|   3365 | 1584 | `	}else if( (int)zPath[0] == c \|\| (int)zPath[0] == d ){` |
|      - | 1585 | `#ifdef __WINNT__` |
|    ! 0 | 1586 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1587 | `#else` |
|    ! 0 | 1588 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1589 | `#endif` |
|    ! 0 | 1590 | `	}` |
|      - | 1591 | `	/* basename/filename */` |
|   6728 | 1592 | `	pCur = &pOut->sBasename;` |
|   6728 | 1593 | `	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);` |
|   6728 | 1594 | `	SyStringTrimLeadingChar(pCur,'/');` |
|      - | 1595 | `#ifdef __WINNT__` |
|      2 | 1596 | `	SyStringTrimLeadingChar(pCur,'\\');` |
|      - | 1597 | `#endif` |
|   6728 | 1598 | `	SyStringDupPtr(&pOut->sFilename,pCur);` |
|   6728 | 1599 | `	if( pCur->nByte > 0 ){` |
|      - | 1600 | `		/* extension */` |
|   6728 | 1601 | `		zEnd--;` |
|  33630 | 1602 | `		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){` |
|  26904 | 1603 | `			zEnd--;` |
|      2 | 1604 | `		}` |
|   6728 | 1605 | `		if( zEnd > pCur->zString ){` |
|   6726 | 1606 | `			zEnd++; /* Jump leading dot */` |
|   6726 | 1607 | `			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);` |
|      - | 1608 | `			/* Fix filename */` |
|   6726 | 1609 | `			pCur = &pOut->sFilename;` |
|   6726 | 1610 | `			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){` |
|   6726 | 1611 | `				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);` |
|   3362 | 1612 | `			}` |
|   3362 | 1613 | `		}` |
|   3363 | 1614 | `	}` |
|   6728 | 1615 | `	return SXRET_OK;` |
|   3365 | 1616 |  |
|      - | 1617 | `/*` |
|      - | 1618 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1619 | ` *  See block comment above.` |
|      - | 1620 | ` */` |
|   6726 | 1621 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1622 |  |
|      - | 1623 | `	const char *zPath;` |
|      - | 1624 | `	path_info sInfo;` |
|      - | 1625 | `	SyString *pComp;` |
|      - | 1626 | `	int iLen;` |
|   6728 | 1627 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1628 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1629 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1630 | `		return PH7_OK;` |
|      - | 1631 | `	}` |
|      - | 1632 | `	/* Point to the target path */` |
|   6728 | 1633 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|   6728 | 1634 | `	if( iLen < 1 ){` |
|      - | 1635 | `		/* Empty string */` |
|    ! 0 | 1636 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1637 | `		return PH7_OK;` |
|      - | 1638 | `	}` |
|      - | 1639 | `	/* Extract path info */` |
|   6728 | 1640 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|  10090 | 1641 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1642 | `		/* Return path component */` |
|   6726 | 1643 | `		int nComp = ph7_value_to_int(apArg[1]);` |
|   6726 | 1644 | `		switch(nComp){` |
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
|   1681 | 1663 | `		case 3: /*PATHINFO_EXTENSION*/` |
|   3364 | 1664 | `			pComp = &sInfo.sExtension;` |
|   3364 | 1665 | `			if( pComp->nByte > 0 ){` |
|   3362 | 1666 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   1682 | 1667 | `			}else{` |
|      - | 1668 | `				/* Expand the empty string */` |
|      3 | 1669 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1670 | `			}` |
|   3364 | 1671 | `			break;` |
|   1679 | 1672 | `		case 4: /*PATHINFO_FILENAME*/` |
|   3360 | 1673 | `			pComp = &sInfo.sFilename;` |
|   3360 | 1674 | `			if( pComp->nByte > 0 ){` |
|   3360 | 1675 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   1681 | 1676 | `			}else{` |
|      - | 1677 | `				/* Expand the empty string */` |
|    ! 0 | 1678 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1679 | `			}` |
|   3360 | 1680 | `			break;` |
|    ! 0 | 1681 | `		default:` |
|      - | 1682 | `			/* Expand the empty string */` |
|    ! 0 | 1683 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1684 | `			break;` |
|      - | 1685 | `		}` |
|   3364 | 1686 | `	}else{` |
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
|   6728 | 1736 | `	return PH7_OK;` |
|   3365 | 1737 |  |
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
|   3396 | 2699 | `static int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2700 |  |
|      - | 2701 | `	const ph7_io_stream *pStream;` |
|      - | 2702 | `	io_private *pDev;` |
|      - | 2703 | `	int rc;` |
|   3398 | 2704 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2705 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 2706 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2707 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2708 | `		return PH7_OK;` |
|      - | 2709 | `	}` |
|      - | 2710 | `	/* Extract our private data */` |
|   3398 | 2711 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2712 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   3398 | 2713 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2714 | `		/*Expecting an IO handle */` |
|    ! 0 | 2715 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2716 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2717 | `		return PH7_OK;` |
|      - | 2718 | `	}` |
|      - | 2719 | `	/* Point to the target IO stream device */` |
|   3398 | 2720 | `	pStream = pDev->pStream;` |
|   3398 | 2721 | `	if( pStream == 0 ){` |
|    ! 0 | 2722 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2723 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2724 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2725 | `			);` |
|    ! 0 | 2726 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2727 | `		return PH7_OK;` |
|      - | 2728 | `	}` |
|   3398 | 2729 | `	rc = SXERR_EOF;` |
|      - | 2730 | `	/* Perform the requested operation */` |
|   3398 | 2731 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2732 | `		/* Data is available */` |
|   1294 | 2733 | `		rc = PH7_OK;` |
|    648 | 2734 | `	}else{` |
|      - | 2735 | `		char zBuf[4096];` |
|      - | 2736 | `		ph7_int64 n;` |
|      - | 2737 | `		/* Perform a buffered read */` |
|   2106 | 2738 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|   2106 | 2739 | `		if( n > 0 ){` |
|      - | 2740 | `			/* Copy buffered data */` |
|    598 | 2741 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|    598 | 2742 | `			rc = PH7_OK;` |
|    298 | 2743 | `		}` |
|      - | 2744 | `	}` |
|      - | 2745 | `	/* EOF or not */` |
|   3398 | 2746 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|   3398 | 2747 | `	return PH7_OK;` |
|   1700 | 2748 |  |
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
|   1904 | 2794 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      2 | 2795 |  |
|      - | 2796 | `	const char *zIn,*zEnd,*zPtr;` |
|   1906 | 2797 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|   1906 | 2798 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|   1906 | 2799 | `	zPtr = zIn;` |
| 139455 | 2800 | `	while( zIn < zEnd ){` |
| 139425 | 2801 | `		if( zIn[0] == '\n' ){` |
|      - | 2802 | `			/* Line found */` |
|   1876 | 2803 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|   1876 | 2804 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|   1876 | 2805 | `			*pzLine = zPtr;` |
|   1876 | 2806 | `			return SXRET_OK;` |
|      - | 2807 | `		}` |
| 137551 | 2808 | `		zIn++;` |
|      2 | 2809 | `	}` |
|      - | 2810 | `	/* No line were found */` |
|     32 | 2811 | `	return SXERR_NOTFOUND;` |
|    954 | 2812 |  |
|      - | 2813 | `/*` |
|      - | 2814 | ` * Read a single line from the underlying IO stream device.` |
|      - | 2815 | ` */` |
|   1908 | 2816 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      2 | 2817 |  |
|   1910 | 2818 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - | 2819 | `	char zBuf[8192];` |
|      - | 2820 | `	ph7_int64 n;` |
|      - | 2821 | `	sxi32 rc;` |
|   1910 | 2822 | `	n = 0;` |
|   1910 | 2823 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2824 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     18 | 2825 | `		SyBlobReset(&pDev->sBuffer);` |
|     18 | 2826 | `		pDev->nOfft = 0;` |
|      8 | 2827 | `	}` |
|   1910 | 2828 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2829 | `		/* Check if there is a line */` |
|   1894 | 2830 | `		rc = GetLine(pDev,&n,pzData);` |
|   1894 | 2831 | `		if( rc == SXRET_OK ){` |
|      - | 2832 | `			/* Got line,update the cursor  */` |
|   1866 | 2833 | `			pDev->nOfft += (sxu32)n;` |
|   1866 | 2834 | `			return n;` |
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
|    956 | 2874 |  |
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
|  18712 | 2896 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - | 2897 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      2 | 2898 |  |
|  18714 | 2899 | `	void *pHandle = 0; /* cc warning */` |
|      - | 2900 | `	SyString sFile;` |
|      - | 2901 | `	int rc;` |
|  18714 | 2902 | `	if( pStream == 0 ){` |
|      - | 2903 | `		/* No such stream device */` |
|    ! 0 | 2904 | `		return 0;` |
|      - | 2905 | `	}` |
|  18714 | 2906 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  18714 | 2907 | `	if( use_include ){` |
|   6782 | 2908 | `		if(	sFile.zString[0] == '/' \|\|` |
|      - | 2909 | `#ifdef __WINNT__` |
|      - | 2910 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - | 2911 | `#endif` |
|   6772 | 2912 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   6770 | 2913 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
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
|   6772 | 2925 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - | 2926 | `			/* Build a path from the set of include path */` |
|   6772 | 2927 | `			SySetResetCursor(&pVm->aPaths);` |
|   6772 | 2928 | `			rc = SXERR_IO;` |
|   6774 | 2929 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - | 2930 | `				/* Build full path */` |
|   6772 | 2931 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - | 2932 | `				/* Append null terminator */` |
|   6772 | 2933 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 2934 | `					continue;` |
|      - | 2935 | `				}` |
|      - | 2936 | `				/* Try to open the file */` |
|   6772 | 2937 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   6772 | 2938 | `				if( rc == PH7_OK ){` |
|   6769 | 2939 | `					if( bPushInclude ){` |
|      - | 2940 | `						/* Mark as included */` |
|   6769 | 2941 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   3384 | 2942 | `					}` |
|   6769 | 2943 | `					break;` |
|      - | 2944 | `				}` |
|      - | 2945 | `				/* Reset the working buffer */` |
|      3 | 2946 | `				SyBlobReset(&sWorker);` |
|      - | 2947 | `				/* Check the next path */` |
|      1 | 2948 | `			}` |
|   6772 | 2949 | `			SyBlobRelease(&sWorker);` |
|      - | 2950 | `		}` |
|   6784 | 2951 | `		if( rc == PH7_OK ){` |
|   6781 | 2952 | `			if( bPushInclude ){` |
|      - | 2953 | `				/* Mark as included */` |
|   6781 | 2954 | `				PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|   3390 | 2955 | `			}` |
|   3390 | 2956 | `		}` |
|   3393 | 2957 | `	}else{` |
|      - | 2958 | `		/* Open the URI direcly */` |
|  11932 | 2959 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - | 2960 | `	}` |
|  18714 | 2961 | `	if( rc != PH7_OK ){` |
|      - | 2962 | `		/* IO error */` |
|      9 | 2963 | `		return 0;` |
|      - | 2964 | `	}` |
|      - | 2965 | `	/* Return the file handle */` |
|  18706 | 2966 | `	return pHandle;` |
|   9358 | 2967 |  |
|      - | 2968 | `/*` |
|      - | 2969 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - | 2970 | ` * Store the read data in the given BLOB (last argument).` |
|      - | 2971 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - | 2972 | ` */` |
|   6778 | 2973 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      1 | 2974 |  |
|      - | 2975 | `	ph7_int64 nRead;` |
|      - | 2976 | `	char zBuf[8192]; /* 8K */` |
|      - | 2977 | `	int rc;` |
|      - | 2978 | `	/* Perform the requested operation */` |
|   6778 | 2979 | `	for(;;){` |
|  13557 | 2980 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  13557 | 2981 | `		if( nRead < 1 ){` |
|      - | 2982 | `			/* EOF or IO error */` |
|   6779 | 2983 | `			break;` |
|      - | 2984 | `		}` |
|      - | 2985 | `		/* Append contents */` |
|   6779 | 2986 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   6779 | 2987 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 2988 | `			break;` |
|      - | 2989 | `		}` |
|      1 | 2990 | `	}` |
|   6779 | 2991 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      1 | 2992 |  |
|      - | 2993 | `/*` |
|      - | 2994 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - | 2995 | ` */` |
|  18718 | 2996 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      2 | 2997 |  |
|  18720 | 2998 | `	if( pStream->xClose ){` |
|  18720 | 2999 | `		pStream->xClose(pHandle);` |
|   9359 | 3000 | `	}` |
|  18720 | 3001 |  |
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
|   1898 | 3072 | `static int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3073 |  |
|      - | 3074 | `	const ph7_io_stream *pStream;` |
|      - | 3075 | `	const char *zLine;` |
|      - | 3076 | `	io_private *pDev;` |
|      - | 3077 | `	ph7_int64 n,nLen;` |
|   1900 | 3078 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3079 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3080 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3081 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3082 | `		return PH7_OK;` |
|      - | 3083 | `	}` |
|      - | 3084 | `	/* Extract our private data */` |
|   1900 | 3085 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3086 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1900 | 3087 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3088 | `		/*Expecting an IO handle */` |
|    ! 0 | 3089 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3090 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3091 | `		return PH7_OK;` |
|      - | 3092 | `	}` |
|      - | 3093 | `	/* Point to the target IO stream device */` |
|   1900 | 3094 | `	pStream = pDev->pStream;` |
|   1900 | 3095 | `	if( pStream == 0  ){` |
|    ! 0 | 3096 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3097 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3098 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3099 | `			);` |
|    ! 0 | 3100 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3101 | `		return PH7_OK;` |
|      - | 3102 | `	}` |
|   1900 | 3103 | `	nLen = -1;` |
|   1900 | 3104 | `	if( nArg > 1 ){` |
|      - | 3105 | `		/* Maximum data to read */` |
|    ! 0 | 3106 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3107 | `	}` |
|      - | 3108 | `	/* Perform the requested operation */` |
|   1900 | 3109 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|   1900 | 3110 | `	if( n < 1 ){` |
|      - | 3111 | `		/* EOF or IO error,return FALSE */` |
|      3 | 3112 | `		ph7_result_bool(pCtx,0);` |
|      2 | 3113 | `	}else{` |
|      - | 3114 | `		/* Return the freshly extracted line */` |
|   1898 | 3115 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - | 3116 | `	}` |
|   1900 | 3117 | `	return PH7_OK;` |
|    951 | 3118 |  |
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
|   4930 | 3381 | `static int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3382 |  |
|      - | 3383 | `	const ph7_io_stream *pStream;` |
|      - | 3384 | `	io_private *pDev;` |
|      - | 3385 | `	int rc;` |
|   4932 | 3386 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3387 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3388 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3389 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3390 | `		return PH7_OK;` |
|      - | 3391 | `	}` |
|      - | 3392 | `	/* Extract our private data */` |
|   4932 | 3393 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3394 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   4932 | 3395 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3396 | `		/*Expecting an IO handle */` |
|    ! 0 | 3397 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3398 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3399 | `		return PH7_OK;` |
|      - | 3400 | `	}` |
|      - | 3401 | `	/* Point to the target IO stream device */` |
|   4932 | 3402 | `	pStream = pDev->pStream;` |
|   4932 | 3403 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 3404 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3405 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3406 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3407 | `			);` |
|    ! 0 | 3408 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3409 | `		return PH7_OK;` |
|      - | 3410 | `	}` |
|   4932 | 3411 | `	ph7_result_bool(pCtx,0);` |
|      - | 3412 | `	/* Perform the requested operation */` |
|   4932 | 3413 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|   4932 | 3414 | `	if( rc != PH7_OK ){` |
|      - | 3415 | `		/* Return FALSE */` |
|    786 | 3416 | `		ph7_result_bool(pCtx,0);` |
|    392 | 3417 | `	}` |
|   4932 | 3418 | `	return PH7_OK;` |
|   2467 | 3419 |  |
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
|    786 | 3474 | `static int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3475 |  |
|      - | 3476 | `	const ph7_io_stream *pStream;` |
|      - | 3477 | `	io_private *pDev;` |
|    788 | 3478 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3479 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3480 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3481 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3482 | `		return PH7_OK;` |
|      - | 3483 | `	}` |
|      - | 3484 | `	/* Extract our private data */` |
|    788 | 3485 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3486 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    788 | 3487 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3488 | `		/*Expecting an IO handle */` |
|    ! 0 | 3489 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3490 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3491 | `		return PH7_OK;` |
|      - | 3492 | `	}` |
|      - | 3493 | `	/* Point to the target IO stream device */` |
|    788 | 3494 | `	pStream = pDev->pStream;` |
|    788 | 3495 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 3496 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3497 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3498 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3499 | `			);` |
|    ! 0 | 3500 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3501 | `		return PH7_OK;` |
|      - | 3502 | `	}` |
|      - | 3503 | `	/* Perform the requested operation */` |
|    788 | 3504 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 3505 | `	/* Release the private stucture */` |
|    788 | 3506 | `	ReleaseIOPrivate(pCtx,pDev);` |
|    788 | 3507 | `	PH7_MemObjRelease(apArg[0]);` |
|    788 | 3508 | `	return PH7_OK;` |
|    395 | 3509 | ` }` |
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
|    786 | 3521 | `static int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3522 |  |
|      - | 3523 | `	const ph7_io_stream *pStream;` |
|      - | 3524 | `	const char *zPath;` |
|      - | 3525 | `	io_private *pDev;` |
|      - | 3526 | `	int iLen,rc;` |
|    788 | 3527 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3528 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3529 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 3530 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3531 | `		return PH7_OK;` |
|      - | 3532 | `	}` |
|      - | 3533 | `	/* Extract the target path */` |
|    788 | 3534 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 3535 | `	/* Try to extract a stream */` |
|    788 | 3536 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|    788 | 3537 | `	if( pStream == 0 ){` |
|    ! 0 | 3538 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3539 | `			"No stream device is associated with the given path(%s)",zPath);` |
|    ! 0 | 3540 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3541 | `		return PH7_OK;` |
|      - | 3542 | `	}` |
|    788 | 3543 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 3544 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3545 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 3546 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 3547 | `			);` |
|    ! 0 | 3548 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3549 | `		return PH7_OK;` |
|      - | 3550 | `	}` |
|      - | 3551 | `	/* Allocate a new IO private instance */` |
|    788 | 3552 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    788 | 3553 | `	if( pDev == 0 ){` |
|    ! 0 | 3554 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3555 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3556 | `		return PH7_OK;` |
|      - | 3557 | `	}` |
|      - | 3558 | `	/* Initialize the structure */` |
|    788 | 3559 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 3560 | `	/* Open the target directory */` |
|    788 | 3561 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|    788 | 3562 | `	if( rc != PH7_OK ){` |
|      - | 3563 | `		/* IO error,return FALSE */` |
|    ! 0 | 3564 | `		ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 3565 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3566 | `	}else{` |
|      - | 3567 | `		/* Return the handle as a resource */` |
|    788 | 3568 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 3569 | `	}` |
|    788 | 3570 | `	return PH7_OK;` |
|    395 | 3571 |  |
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
|   3398 | 3663 | `static int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3664 |  |
|      - | 3665 | `	const ph7_io_stream *pStream;` |
|      - | 3666 | `	ph7_int64 n,nRead,nMaxlen;` |
|   3400 | 3667 | `	int use_include  = FALSE;` |
|      - | 3668 | `	const char *zFile;` |
|      - | 3669 | `	char zBuf[8192];` |
|      - | 3670 | `	void *pHandle;` |
|      - | 3671 | `	int nLen;` |
|      - | 3672 |  |
|   3400 | 3673 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3674 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3675 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3676 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3677 | `		return PH7_OK;` |
|      - | 3678 | `	}` |
|      - | 3679 | `	/* Extract the file path */` |
|   3400 | 3680 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3681 | `	/* Point to the target IO stream device */` |
|   3400 | 3682 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   3400 | 3683 | `	if( pStream == 0 ){` |
|    ! 0 | 3684 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3685 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3686 | `		return PH7_OK;` |
|      - | 3687 | `	}` |
|   3400 | 3688 | `	nMaxlen = -1;` |
|   3400 | 3689 | `	if( nArg > 1 ){` |
|      5 | 3690 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|      2 | 3691 | `	}` |
|      - | 3692 | `	/* Try to open the file in read-only mode */` |
|   3400 | 3693 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|   3400 | 3694 | `	if( pHandle == 0 ){` |
|    ! 0 | 3695 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3696 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3697 | `		return PH7_OK;` |
|      - | 3698 | `	}` |
|   3400 | 3699 | `	if( nArg > 3 ){` |
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
|   3400 | 3714 | `	nRead = 0;` |
|   3397 | 3715 | `	for(;;){` |
|  10196 | 3716 | `		n = pStream->xRead(pHandle,zBuf,` |
|   3400 | 3717 | `			(nMaxlen > 0 && (nMaxlen < (ph7_int64)sizeof(zBuf))) ? nMaxlen : (ph7_int64)sizeof(zBuf));` |
|   6796 | 3718 | `		if( n < 1 ){` |
|      - | 3719 | `			/* EOF or IO error,break immediately */` |
|   3398 | 3720 | `			break;` |
|      - | 3721 | `		}` |
|      - | 3722 | `		/* Append data */` |
|   3400 | 3723 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 3724 | `		/* Increment read counter */` |
|   3400 | 3725 | `		nRead += n;` |
|   3400 | 3726 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 3727 | `			/* Read limit reached */` |
|      3 | 3728 | `			break;` |
|      - | 3729 | `		}` |
|      2 | 3730 | `	}` |
|      - | 3731 | `	/* Close the stream */` |
|   3400 | 3732 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3733 | `	/* Check if we have read something */` |
|   3400 | 3734 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 3735 | `		/* Nothing read,return FALSE */` |
|    ! 0 | 3736 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3737 | `	}` |
|   3400 | 3738 | `	return PH7_OK;` |
|   1701 | 3739 |  |
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
|   8450 | 3759 | `static int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3760 |  |
|   8452 | 3761 | `	int use_include  = FALSE;` |
|      - | 3762 | `	const ph7_io_stream *pStream;` |
|      - | 3763 | `	const char *zFile;` |
|      - | 3764 | `	const char *zData;` |
|      - | 3765 | `	int iOpenFlags;` |
|      - | 3766 | `	void *pHandle;` |
|      - | 3767 | `	int iFlags;` |
|      - | 3768 | `	int nLen;` |
|      - | 3769 |  |
|   8452 | 3770 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3771 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3772 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3773 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3774 | `		return PH7_OK;` |
|      - | 3775 | `	}` |
|      - | 3776 | `	/* Extract the file path */` |
|   8452 | 3777 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3778 | `	/* Point to the target IO stream device */` |
|   8452 | 3779 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   8452 | 3780 | `	if( pStream == 0 ){` |
|    ! 0 | 3781 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3782 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3783 | `		return PH7_OK;` |
|      - | 3784 | `	}` |
|      - | 3785 | `	/* Data to write */` |
|   8452 | 3786 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 3787 | `	/* Try to open the file in read-write mode */` |
|   8452 | 3788 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 3789 | `	/* Extract the flags */` |
|   8452 | 3790 | `	iFlags = 0;` |
|   8452 | 3791 | `	if( nArg > 2 ){` |
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
|  12677 | 3805 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   4225 | 3806 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|   8452 | 3807 | `	if( pHandle == 0 ){` |
|    ! 0 | 3808 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3809 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3810 | `		return PH7_OK;` |
|      - | 3811 | `	}` |
|   8452 | 3812 | `	if( nLen < 1 ){` |
|      - | 3813 | `		/* Empty data, file is created/truncated */` |
|      7 | 3814 | `		ph7_result_int64(pCtx,0);` |
|      7 | 3815 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|      7 | 3816 | `		return PH7_OK;` |
|      - | 3817 | `	}` |
|   8446 | 3818 | `	if( pStream->xWrite ){` |
|      - | 3819 | `		ph7_int64 n;` |
|   8446 | 3820 | `		if( (iFlags & 0x01/* LOCK_EX */) && pStream->xLock ){` |
|      - | 3821 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 3822 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 3823 | `		}` |
|      - | 3824 | `		/* Perform the write operation */` |
|   8446 | 3825 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|   8446 | 3826 | `		if( n < 0 ){` |
|      - | 3827 | `			/* IO error,return FALSE */` |
|    ! 0 | 3828 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3829 | `		}else{` |
|      - | 3830 | `			/* Total number of bytes written */` |
|   8446 | 3831 | `			ph7_result_int64(pCtx,n);` |
|      - | 3832 | `		}` |
|   4224 | 3833 | `	}else{` |
|      - | 3834 | `		/* Read-only stream */` |
|    ! 0 | 3835 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 3836 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 3837 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 3838 | `			);` |
|    ! 0 | 3839 | `		ph7_result_bool(pCtx,0);` |
|      - | 3840 | `	}` |
|      - | 3841 | `	/* Close the handle */` |
|   8446 | 3842 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|   8446 | 3843 | `	return PH7_OK;` |
|   4227 | 3844 |  |
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
|   2374 | 4720 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      2 | 4721 |  |
|   2376 | 4722 | `	pOut->pStream = pStream;` |
|   2376 | 4723 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   2376 | 4724 | `	pOut->nOfft = 0;` |
|      - | 4725 | `	/* Set the magic number */` |
|   2376 | 4726 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   2376 | 4727 |  |
|      - | 4728 | `/*` |
|      - | 4729 | ` * Release the IO private structure.` |
|      - | 4730 | ` */` |
|   2366 | 4731 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      2 | 4732 |  |
|   2368 | 4733 | `	SyBlobRelease(&pDev->sBuffer);` |
|   2368 | 4734 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 4735 | `	/* Release the whole structure */` |
|   2368 | 4736 | `	ph7_context_free_chunk(pCtx,pDev);` |
|   2368 | 4737 |  |
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
|      - | 5102 | `/*` |
|      - | 5103 | ` * Section:` |
|      - | 5104 | ` *    ZIP archive processing.` |
|      - | 5105 | ` * Status:` |
|      - | 5106 | ` *    Stable.` |
|      - | 5107 | ` */` |
|      - | 5108 | `typedef struct zip_raw_data zip_raw_data;` |
|      - | 5109 | `struct zip_raw_data` |
|      - | 5110 |  |
|      - | 5111 | `	int iType;         /* Where the raw data is stored */` |
|      - | 5112 | `	union raw_data{` |
|      - | 5113 | `		struct mmap_data{` |
|      - | 5114 | `			void *pMap;          /* Memory mapped data */` |
|      - | 5115 | `			ph7_int64 nSize;     /* Map size */` |
|      - | 5116 | `			const ph7_vfs *pVfs; /* Underlying vfs */` |
|      - | 5117 | `		}mmap;` |
|      - | 5118 | `		SyBlob sBlob;  /* Memory buffer */` |
|      - | 5119 | `	}raw;` |
|      - | 5120 | `};` |
|      - | 5121 | `#define ZIP_RAW_DATA_MMAPED 1 /* Memory mapped ZIP raw data */` |
|      - | 5122 | `#define ZIP_RAW_DATA_MEMBUF 2 /* ZIP raw data stored in a dynamically` |
|      - | 5123 | `                               * allocated memory chunk.` |
|      - | 5124 | `							   */` |
|      - | 5125 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 5126 | ` /*` |
|      - | 5127 | `  * mixed zip_open(string $filename)` |
|      - | 5128 |  |
|      - | 5129 | `  *  Opens a new zip archive for reading.` |
|      - | 5130 | `  * Parameters` |
|      - | 5131 | `  *  $filename` |
|      - | 5132 | `  *   The file name of the ZIP archive to open.` |
|      - | 5133 | `  * Return` |
|      - | 5134 | `  *  A resource handle for later use with zip_read() and zip_close() or FALSE on failure.` |
|      - | 5135 | `  */` |
|     30 | 5136 | `static int PH7_builtin_zip_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5137 |  |
|      - | 5138 | `	const ph7_io_stream *pStream;` |
|      - | 5139 | `	SyArchive *pArchive;` |
|      - | 5140 | `	zip_raw_data *pRaw;` |
|      - | 5141 | `	const char *zFile;` |
|      - | 5142 | `	SyBlob *pContents;` |
|      - | 5143 | `	void *pHandle;` |
|      - | 5144 | `	int nLen;` |
|      - | 5145 | `	sxi32 rc;` |
|     32 | 5146 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5147 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5148 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5149 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5150 | `		return PH7_OK;` |
|      - | 5151 | `	}` |
|      - | 5152 | `	/* Extract the file path */` |
|     32 | 5153 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5154 | `	/* Point to the target IO stream device */` |
|     32 | 5155 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     32 | 5156 | `	if( pStream == 0 ){` |
|    ! 0 | 5157 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5158 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5159 | `		return PH7_OK;` |
|      - | 5160 | `	}` |
|      - | 5161 | `	/* Create an in-memory archive */` |
|     32 | 5162 | `	pArchive = (SyArchive *)ph7_context_alloc_chunk(pCtx,sizeof(SyArchive)+sizeof(zip_raw_data),TRUE,FALSE);` |
|     32 | 5163 | `	if( pArchive == 0 ){` |
|    ! 0 | 5164 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"PH7 is running out of memory");` |
|    ! 0 | 5165 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5166 | `		return PH7_OK;` |
|      - | 5167 | `	}` |
|     32 | 5168 | `	pRaw = (zip_raw_data *)&pArchive[1];` |
|      - | 5169 | `	/* Initialize the archive */` |
|     32 | 5170 | `	SyArchiveInit(pArchive,&pCtx->pVm->sAllocator,0,0);` |
|      - | 5171 | `	/* Extract the default stream */` |
|     32 | 5172 | `	if( pStream == pCtx->pVm->pDefStream /* file:// stream*/){` |
|      - | 5173 | `		const ph7_vfs *pVfs;` |
|      - | 5174 | `		/* Try to get a memory view of the whole file since ZIP files` |
|      - | 5175 | `		 * tends to be very big this days,this is a huge performance win.` |
|      - | 5176 | `		 */` |
|     32 | 5177 | `		pVfs = PH7_ExportBuiltinVfs();` |
|     32 | 5178 | `		if( pVfs && pVfs->xMmap ){` |
|     32 | 5179 | `			rc = pVfs->xMmap(zFile,&pRaw->raw.mmap.pMap,&pRaw->raw.mmap.nSize);` |
|     32 | 5180 | `			if( rc == PH7_OK ){` |
|      - | 5181 | `				/* Nice,Extract the whole archive */` |
|     30 | 5182 | `				rc = SyZipExtractFromBuf(pArchive,(const char *)pRaw->raw.mmap.pMap,(sxu32)pRaw->raw.mmap.nSize);` |
|     30 | 5183 | `				if( rc != SXRET_OK ){` |
|     15 | 5184 | `					if( pVfs->xUnmap ){` |
|     15 | 5185 | `						pVfs->xUnmap(pRaw->raw.mmap.pMap,pRaw->raw.mmap.nSize);` |
|      7 | 5186 | `					}` |
|      - | 5187 | `					/* Release the allocated chunk */` |
|     15 | 5188 | `					ph7_context_free_chunk(pCtx,pArchive);` |
|      - | 5189 | `					/* Something goes wrong with this ZIP archive,return FALSE */` |
|     15 | 5190 | `					ph7_result_bool(pCtx,0);` |
|     15 | 5191 | `					return PH7_OK;` |
|      - | 5192 | `				}` |
|      - | 5193 | `				/* Archive successfully opened */` |
|     16 | 5194 | `				pRaw->iType = ZIP_RAW_DATA_MMAPED;` |
|     16 | 5195 | `				pRaw->raw.mmap.pVfs = pVfs;` |
|     16 | 5196 | `				goto success;` |
|      - | 5197 | `			}` |
|      1 | 5198 | `		}` |
|      - | 5199 | `		/* FALL THROUGH */` |
|      1 | 5200 | `	}` |
|      - | 5201 | `	/* Try to open the file in read-only mode */` |
|      3 | 5202 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5203 | `	if( pHandle == 0 ){` |
|      3 | 5204 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|      3 | 5205 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5206 | `		return PH7_OK;` |
|      - | 5207 | `	}` |
|    ! 0 | 5208 | `	pContents = &pRaw->raw.sBlob;` |
|    ! 0 | 5209 | `	SyBlobInit(pContents,&pCtx->pVm->sAllocator);` |
|      - | 5210 | `	/* Read the whole file */` |
|    ! 0 | 5211 | `	PH7_StreamReadWholeFile(pHandle,pStream,pContents);` |
|      - | 5212 | `	/* Assume an invalid ZIP file */` |
|    ! 0 | 5213 | `	rc = SXERR_INVALID;` |
|    ! 0 | 5214 | `	if( SyBlobLength(pContents) > 0 ){` |
|      - | 5215 | `		/* Extract archive entries */` |
|    ! 0 | 5216 | `		rc = SyZipExtractFromBuf(pArchive,(const char *)SyBlobData(pContents),SyBlobLength(pContents));` |
|    ! 0 | 5217 | `	}` |
|    ! 0 | 5218 | `	pRaw->iType = ZIP_RAW_DATA_MEMBUF;` |
|      - | 5219 | `	/* Close the stream */` |
|    ! 0 | 5220 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|    ! 0 | 5221 | `	if( rc != SXRET_OK ){` |
|      - | 5222 | `		/* Release the working buffer */` |
|    ! 0 | 5223 | `		SyBlobRelease(pContents);` |
|      - | 5224 | `		/* Release the allocated chunk */` |
|    ! 0 | 5225 | `		ph7_context_free_chunk(pCtx,pArchive);` |
|      - | 5226 | `		/* Something goes wrong with this ZIP archive,return FALSE */` |
|    ! 0 | 5227 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5228 | `		return PH7_OK;` |
|      - | 5229 | `	}` |
|    ! 0 | 5230 | `success:` |
|      - | 5231 | `	/* Reset the loop cursor */` |
|     16 | 5232 | `	SyArchiveResetLoopCursor(pArchive);` |
|      - | 5233 | `	/* Return the in-memory archive as a resource handle */` |
|     16 | 5234 | `	ph7_result_resource(pCtx,pArchive);` |
|     16 | 5235 | `	return PH7_OK;` |
|     17 | 5236 |  |
|      - | 5237 | `/*` |
|      - | 5238 | `  * void zip_close(resource $zip)` |
|      - | 5239 | `  *  Close an in-memory ZIP archive.` |
|      - | 5240 | `  * Parameters` |
|      - | 5241 | `  *  $zip` |
|      - | 5242 | `  *   A ZIP file previously opened with zip_open().` |
|      - | 5243 | `  * Return` |
|      - | 5244 | `  *  null.` |
|      - | 5245 | `  */` |
|     14 | 5246 | `static int PH7_builtin_zip_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5247 |  |
|      - | 5248 | `	SyArchive *pArchive;` |
|      - | 5249 | `	zip_raw_data *pRaw;` |
|     16 | 5250 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5251 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5252 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|    ! 0 | 5253 | `		return PH7_OK;` |
|      - | 5254 | `	}` |
|      - | 5255 | `	/* Point to the in-memory archive */` |
|     16 | 5256 | `	pArchive = (SyArchive *)ph7_value_to_resource(apArg[0]);` |
|      - | 5257 | `	/* Make sure we are dealing with a valid ZIP archive */` |
|     16 | 5258 | `	if( SXARCH_INVALID(pArchive) ){` |
|    ! 0 | 5259 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|    ! 0 | 5260 | `		return PH7_OK;` |
|      - | 5261 | `	}` |
|      - | 5262 | `	/* Release the archive */` |
|     16 | 5263 | `	SyArchiveRelease(pArchive);` |
|     16 | 5264 | `	pRaw = (zip_raw_data *)&pArchive[1];` |
|     16 | 5265 | `	if( pRaw->iType == ZIP_RAW_DATA_MEMBUF ){` |
|    ! 0 | 5266 | `		SyBlobRelease(&pRaw->raw.sBlob);` |
|    ! 0 | 5267 | `	}else{` |
|     16 | 5268 | `		const ph7_vfs *pVfs = pRaw->raw.mmap.pVfs;` |
|     16 | 5269 | `		if( pVfs->xUnmap ){` |
|      - | 5270 | `			/* Unmap the memory view */` |
|     16 | 5271 | `			pVfs->xUnmap(pRaw->raw.mmap.pMap,pRaw->raw.mmap.nSize);` |
|      7 | 5272 | `		}` |
|      - | 5273 | `	}` |
|      - | 5274 | `	/* Release the memory chunk */` |
|     16 | 5275 | `	ph7_context_free_chunk(pCtx,pArchive);` |
|     16 | 5276 | `	return PH7_OK;` |
|      9 | 5277 |  |
|      - | 5278 | `/*` |
|      - | 5279 | `  * mixed zip_read(resource $zip)` |
|      - | 5280 | `  *  Reads the next entry from an in-memory ZIP archive.` |
|      - | 5281 | `  * Parameters` |
|      - | 5282 | `  *  $zip` |
|      - | 5283 | `  *   A ZIP file previously opened with zip_open().` |
|      - | 5284 | `  * Return` |
|      - | 5285 | `  *  A directory entry resource for later use with the zip_entry_... functions` |
|      - | 5286 | `  *  or FALSE if there are no more entries to read, or an error code if an error occurred.` |
|      - | 5287 | `  */` |
|      8 | 5288 | `static int PH7_builtin_zip_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5289 |  |
|     10 | 5290 | `	SyArchiveEntry *pNext = 0; /* cc warning */` |
|      - | 5291 | `	SyArchive *pArchive;` |
|      - | 5292 | `	sxi32 rc;` |
|     10 | 5293 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5294 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5295 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|      - | 5296 | `		/* return FALSE */` |
|    ! 0 | 5297 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5298 | `		return PH7_OK;` |
|      - | 5299 | `	}` |
|      - | 5300 | `	/* Point to the in-memory archive */` |
|     10 | 5301 | `	pArchive = (SyArchive *)ph7_value_to_resource(apArg[0]);` |
|      - | 5302 | `	/* Make sure we are dealing with a valid ZIP archive */` |
|     10 | 5303 | `	if( SXARCH_INVALID(pArchive) ){` |
|    ! 0 | 5304 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|      - | 5305 | `		/* return FALSE */` |
|    ! 0 | 5306 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5307 | `		return PH7_OK;` |
|      - | 5308 | `	}` |
|      - | 5309 | `	/* Extract the next entry */` |
|     10 | 5310 | `	rc = SyArchiveGetNextEntry(pArchive,&pNext);` |
|     10 | 5311 | `	if( rc != SXRET_OK ){` |
|      - | 5312 | `		/* No more entries in the central directory,return FALSE */` |
|    ! 0 | 5313 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5314 | `	}else{` |
|      - | 5315 | `		/* Return as a resource handle */` |
|     10 | 5316 | `		ph7_result_resource(pCtx,pNext);` |
|      - | 5317 | `		/* Point to the ZIP raw data */` |
|     10 | 5318 | `		pNext->pUserData = (void *)&pArchive[1];` |
|      - | 5319 | `	}` |
|     10 | 5320 | `	return PH7_OK;` |
|      6 | 5321 |  |
|      - | 5322 | `/*` |
|      - | 5323 | `  * bool zip_entry_open(resource $zip,resource $zip_entry[,string $mode ])` |
|      - | 5324 | `  *  Open a directory entry for reading` |
|      - | 5325 | `  * Parameters` |
|      - | 5326 | `  *  $zip` |
|      - | 5327 | `  *   A ZIP file previously opened with zip_open().` |
|      - | 5328 | `  *  $zip_entry` |
|      - | 5329 | `  *   A directory entry returned by zip_read().` |
|      - | 5330 | `  * $mode` |
|      - | 5331 | `  *   Not used` |
|      - | 5332 | `  * Return` |
|      - | 5333 | `  *  A directory entry resource for later use with the zip_entry_... functions` |
|      - | 5334 | `  *  or FALSE if there are no more entries to read, or an error code if an error occurred.` |
|      - | 5335 | `  */` |
|      2 | 5336 | `static int PH7_builtin_zip_entry_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5337 |  |
|      - | 5338 | `	SyArchiveEntry *pEntry;` |
|      - | 5339 | `	SyArchive *pArchive;` |
|      3 | 5340 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_resource(apArg[1]) ){` |
|      - | 5341 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5342 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|      - | 5343 | `		/* return FALSE */` |
|    ! 0 | 5344 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5345 | `		return PH7_OK;` |
|      - | 5346 | `	}` |
|      - | 5347 | `	/* Point to the in-memory archive */` |
|      3 | 5348 | `	pArchive = (SyArchive *)ph7_value_to_resource(apArg[0]);` |
|      - | 5349 | `	/* Make sure we are dealing with a valid ZIP archive */` |
|      3 | 5350 | `	if( SXARCH_INVALID(pArchive) ){` |
|    ! 0 | 5351 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|      - | 5352 | `		/* return FALSE */` |
|    ! 0 | 5353 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5354 | `		return PH7_OK;` |
|      - | 5355 | `	}` |
|      - | 5356 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|      3 | 5357 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[1]);` |
|      3 | 5358 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|    ! 0 | 5359 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5360 | `		/* return FALSE */` |
|    ! 0 | 5361 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5362 | `		return PH7_OK;` |
|      - | 5363 | `	}` |
|      - | 5364 | `	/* All done. Actually this function is a no-op,return TRUE */` |
|      3 | 5365 | `	ph7_result_bool(pCtx,1);` |
|      3 | 5366 | `	return PH7_OK;` |
|      2 | 5367 |  |
|      - | 5368 | `/*` |
|      - | 5369 | `  * bool zip_entry_close(resource $zip_entry)` |
|      - | 5370 | `  *  Close a directory entry.` |
|      - | 5371 | `  * Parameters` |
|      - | 5372 | `  *  $zip_entry` |
|      - | 5373 | `  *   A directory entry returned by zip_read().` |
|      - | 5374 | `  * Return` |
|      - | 5375 | `  *  Returns TRUE on success or FALSE on failure.` |
|      - | 5376 | `  */` |
|      6 | 5377 | `static int PH7_builtin_zip_entry_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5378 |  |
|      - | 5379 | `	SyArchiveEntry *pEntry;` |
|      7 | 5380 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5381 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5382 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5383 | `		/* return FALSE */` |
|    ! 0 | 5384 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5385 | `		return PH7_OK;` |
|      - | 5386 | `	}` |
|      - | 5387 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|      7 | 5388 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|      7 | 5389 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|    ! 0 | 5390 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5391 | `		/* return FALSE */` |
|    ! 0 | 5392 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5393 | `		return PH7_OK;` |
|      - | 5394 | `	}` |
|      - | 5395 | `	/* Reset the read cursor */` |
|      7 | 5396 | `	pEntry->nReadCount = 0;` |
|      - | 5397 | `	/*All done. Actually this function is a no-op,return TRUE */` |
|      7 | 5398 | `	ph7_result_bool(pCtx,1);` |
|      7 | 5399 | `	return PH7_OK;` |
|      4 | 5400 |  |
|      - | 5401 | `/*` |
|      - | 5402 | `  * string zip_entry_name(resource $zip_entry)` |
|      - | 5403 | `  *  Retrieve the name of a directory entry.` |
|      - | 5404 | `  * Parameters` |
|      - | 5405 | `  *  $zip_entry` |
|      - | 5406 | `  *   A directory entry returned by zip_read().` |
|      - | 5407 | `  * Return` |
|      - | 5408 | `  *  The name of the directory entry.` |
|      - | 5409 | `  */` |
|      2 | 5410 | `static int PH7_builtin_zip_entry_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5411 |  |
|      - | 5412 | `	SyArchiveEntry *pEntry;` |
|      - | 5413 | `	SyString *pName;` |
|      3 | 5414 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5415 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5416 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5417 | `		/* return FALSE */` |
|    ! 0 | 5418 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5419 | `		return PH7_OK;` |
|      - | 5420 | `	}` |
|      - | 5421 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|      3 | 5422 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|      3 | 5423 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|    ! 0 | 5424 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5425 | `		/* return FALSE */` |
|    ! 0 | 5426 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5427 | `		return PH7_OK;` |
|      - | 5428 | `	}` |
|      - | 5429 | `	/* Return entry name */` |
|      3 | 5430 | `	pName = &pEntry->sFileName;` |
|      3 | 5431 | `	ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      3 | 5432 | `	return PH7_OK;` |
|      2 | 5433 |  |
|      - | 5434 | `/*` |
|      - | 5435 | `  * int64 zip_entry_filesize(resource $zip_entry)` |
|      - | 5436 | `  *  Retrieve the actual file size of a directory entry.` |
|      - | 5437 | `  * Parameters` |
|      - | 5438 | `  *  $zip_entry` |
|      - | 5439 | `  *   A directory entry returned by zip_read().` |
|      - | 5440 | `  * Return` |
|      - | 5441 | `  *  The size of the directory entry.` |
|      - | 5442 | `  */` |
|      4 | 5443 | `static int PH7_builtin_zip_entry_filesize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5444 |  |
|      - | 5445 | `	SyArchiveEntry *pEntry;` |
|      5 | 5446 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5447 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5448 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5449 | `		/* return FALSE */` |
|    ! 0 | 5450 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5451 | `		return PH7_OK;` |
|      - | 5452 | `	}` |
|      - | 5453 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|      5 | 5454 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|      5 | 5455 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|    ! 0 | 5456 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5457 | `		/* return FALSE */` |
|    ! 0 | 5458 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5459 | `		return PH7_OK;` |
|      - | 5460 | `	}` |
|      - | 5461 | `	/* Return entry size */` |
|      5 | 5462 | `	ph7_result_int64(pCtx,(ph7_int64)pEntry->nByte);` |
|      5 | 5463 | `	return PH7_OK;` |
|      3 | 5464 |  |
|      - | 5465 | `/*` |
|      - | 5466 | `  * int64 zip_entry_compressedsize(resource $zip_entry)` |
|      - | 5467 | `  *  Retrieve the compressed size of a directory entry.` |
|      - | 5468 | `  * Parameters` |
|      - | 5469 | `  *  $zip_entry` |
|      - | 5470 | `  *   A directory entry returned by zip_read().` |
|      - | 5471 | `  * Return` |
|      - | 5472 | `  *  The compressed size.` |
|      - | 5473 | `  */` |
|      2 | 5474 | `static int PH7_builtin_zip_entry_compressedsize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5475 |  |
|      - | 5476 | `	SyArchiveEntry *pEntry;` |
|      3 | 5477 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5478 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5479 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5480 | `		/* return FALSE */` |
|    ! 0 | 5481 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5482 | `		return PH7_OK;` |
|      - | 5483 | `	}` |
|      - | 5484 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|      3 | 5485 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|      3 | 5486 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|    ! 0 | 5487 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5488 | `		/* return FALSE */` |
|    ! 0 | 5489 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5490 | `		return PH7_OK;` |
|      - | 5491 | `	}` |
|      - | 5492 | `	/* Return entry compressed size */` |
|      3 | 5493 | `	ph7_result_int64(pCtx,(ph7_int64)pEntry->nByteCompr);` |
|      3 | 5494 | `	return PH7_OK;` |
|      2 | 5495 |  |
|      - | 5496 | `/*` |
|      - | 5497 | `  * string zip_entry_read(resource $zip_entry[,int $length])` |
|      - | 5498 | `  *  Reads from an open directory entry.` |
|      - | 5499 | `  * Parameters` |
|      - | 5500 | `  *  $zip_entry` |
|      - | 5501 | `  *   A directory entry returned by zip_read().` |
|      - | 5502 | `  *  $length` |
|      - | 5503 | `  *   The number of bytes to return. If not specified, this function` |
|      - | 5504 | `  *   will attempt to read 1024 bytes.` |
|      - | 5505 | `  * Return` |
|      - | 5506 | `  *  Returns the data read, or FALSE if the end of the file is reached.` |
|      - | 5507 | `  */` |
|      2 | 5508 | `static int PH7_builtin_zip_entry_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5509 |  |
|      - | 5510 | `	SyArchiveEntry *pEntry;` |
|      - | 5511 | `	zip_raw_data *pRaw;` |
|      - | 5512 | `	const char *zData;` |
|      - | 5513 | `	int iLength;` |
|      3 | 5514 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5515 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5516 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5517 | `		/* return FALSE */` |
|    ! 0 | 5518 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5519 | `		return PH7_OK;` |
|      - | 5520 | `	}` |
|      - | 5521 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|      3 | 5522 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|      3 | 5523 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|    ! 0 | 5524 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5525 | `		/* return FALSE */` |
|    ! 0 | 5526 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5527 | `		return PH7_OK;` |
|      - | 5528 | `	}` |
|      3 | 5529 | `	zData = 0;` |
|      3 | 5530 | `	if( pEntry->nReadCount >= pEntry->nByteCompr ){` |
|      - | 5531 | `		/* No more data to read,return FALSE */` |
|    ! 0 | 5532 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5533 | `		return PH7_OK;` |
|      - | 5534 | `	}` |
|      - | 5535 | `	/* Set a default read length */` |
|      3 | 5536 | `	iLength = 1024;` |
|      3 | 5537 | `	if( nArg > 1 ){` |
|      3 | 5538 | `		iLength = ph7_value_to_int(apArg[1]);` |
|      3 | 5539 | `		if( iLength < 1 ){` |
|    ! 0 | 5540 | `			iLength = 1024;` |
|    ! 0 | 5541 | `		}` |
|      1 | 5542 | `	}` |
|      3 | 5543 | `	if( (sxu32)iLength > pEntry->nByteCompr - pEntry->nReadCount ){` |
|    ! 0 | 5544 | `		iLength = (int)(pEntry->nByteCompr - pEntry->nReadCount);` |
|    ! 0 | 5545 | `	}` |
|      - | 5546 | `	/* Return the entry contents */` |
|      3 | 5547 | `	pRaw = (zip_raw_data *)pEntry->pUserData;` |
|      3 | 5548 | `	if( pRaw->iType == ZIP_RAW_DATA_MEMBUF ){` |
|    ! 0 | 5549 | `		zData = (const char *)SyBlobDataAt(&pRaw->raw.sBlob,(pEntry->nOfft+pEntry->nReadCount));` |
|    ! 0 | 5550 | `	}else{` |
|      3 | 5551 | `		const char *zMap = (const char *)pRaw->raw.mmap.pMap;` |
|      - | 5552 | `		/* Memory mmaped chunk */` |
|      3 | 5553 | `		zData = &zMap[pEntry->nOfft+pEntry->nReadCount];` |
|      - | 5554 | `	}` |
|      - | 5555 | `	/* Increment the read counter */` |
|      3 | 5556 | `	pEntry->nReadCount += iLength;` |
|      - | 5557 | `	/* Return the raw data */` |
|      3 | 5558 | `	ph7_result_string(pCtx,zData,iLength);` |
|      3 | 5559 | `	return PH7_OK;` |
|      2 | 5560 |  |
|      - | 5561 | `/*` |
|      - | 5562 | `  * bool zip_entry_reset_read_cursor(resource $zip_entry)` |
|      - | 5563 | `  *  Reset the read cursor of an open directory entry.` |
|      - | 5564 | `  * Parameters` |
|      - | 5565 | `  *  $zip_entry` |
|      - | 5566 | `  *   A directory entry returned by zip_read().` |
|      - | 5567 | `  * Return` |
|      - | 5568 | `  *  TRUE on success,FALSE on failure.` |
|      - | 5569 | `  * Note that this is a symisc eXtension.` |
|      - | 5570 | `  */` |
|      6 | 5571 | `static int PH7_builtin_zip_entry_reset_read_cursor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5572 |  |
|      - | 5573 | `	SyArchiveEntry *pEntry;` |
|      7 | 5574 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5575 | `		/* Missing/Invalid arguments */` |
|      5 | 5576 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5577 | `		/* return FALSE */` |
|      5 | 5578 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5579 | `		return PH7_OK;` |
|      - | 5580 | `	}` |
|      - | 5581 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|      3 | 5582 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|      3 | 5583 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|    ! 0 | 5584 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5585 | `		/* return FALSE */` |
|    ! 0 | 5586 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5587 | `		return PH7_OK;` |
|      - | 5588 | `	}` |
|      - | 5589 | `	/* Reset the cursor */` |
|      3 | 5590 | `	pEntry->nReadCount = 0;` |
|      - | 5591 | `	/* Return TRUE */` |
|      3 | 5592 | `	ph7_result_bool(pCtx,1);` |
|      3 | 5593 | `	return PH7_OK;` |
|      4 | 5594 |  |
|      - | 5595 | `/*` |
|      - | 5596 | `  * string zip_entry_compressionmethod(resource $zip_entry)` |
|      - | 5597 | `  *  Retrieve the compression method of a directory entry.` |
|      - | 5598 | `  * Parameters` |
|      - | 5599 | `  *  $zip_entry` |
|      - | 5600 | `  *   A directory entry returned by zip_read().` |
|      - | 5601 | `  * Return` |
|      - | 5602 | `  *  The compression method on success or FALSE on failure.` |
|      - | 5603 | `  */` |
|      2 | 5604 | `static int PH7_builtin_zip_entry_compressionmethod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5605 |  |
|      - | 5606 | `	SyArchiveEntry *pEntry;` |
|      3 | 5607 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5608 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5609 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5610 | `		/* return FALSE */` |
|    ! 0 | 5611 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5612 | `		return PH7_OK;` |
|      - | 5613 | `	}` |
|      - | 5614 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|      3 | 5615 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|      3 | 5616 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|    ! 0 | 5617 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5618 | `		/* return FALSE */` |
|    ! 0 | 5619 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5620 | `		return PH7_OK;` |
|      - | 5621 | `	}` |
|      3 | 5622 | `	switch(pEntry->nComprMeth){` |
|      1 | 5623 | `	case 0:` |
|      - | 5624 | `		/* No compression;entry is stored */` |
|      3 | 5625 | `		ph7_result_string(pCtx,"stored",(int)sizeof("stored")-1);` |
|      3 | 5626 | `		break;` |
|    ! 0 | 5627 | `	case 8:` |
|      - | 5628 | `		/* Entry is deflated (Default compression algorithm)  */` |
|    ! 0 | 5629 | `		ph7_result_string(pCtx,"deflate",(int)sizeof("deflate")-1);` |
|    ! 0 | 5630 | `		break;` |
|      - | 5631 | `		/* Exotic compression algorithms */` |
|    ! 0 | 5632 | `	case 1:` |
|    ! 0 | 5633 | `		ph7_result_string(pCtx,"shrunk",(int)sizeof("shrunk")-1);` |
|    ! 0 | 5634 | `		break;` |
|    ! 0 | 5635 | `	case 2:` |
|      - | 5636 | `	case 3:` |
|      - | 5637 | `	case 4:` |
|      - | 5638 | `	case 5:` |
|      - | 5639 | `		/* Entry is reduced */` |
|    ! 0 | 5640 | `		ph7_result_string(pCtx,"reduced",(int)sizeof("reduced")-1);` |
|    ! 0 | 5641 | `		break;` |
|    ! 0 | 5642 | `	case 6:` |
|      - | 5643 | `		/* Entry is imploded */` |
|    ! 0 | 5644 | `		ph7_result_string(pCtx,"implode",(int)sizeof("implode")-1);` |
|    ! 0 | 5645 | `		break;` |
|    ! 0 | 5646 | `	default:` |
|    ! 0 | 5647 | `		ph7_result_string(pCtx,"unknown",(int)sizeof("unknown")-1);` |
|    ! 0 | 5648 | `		break;` |
|      - | 5649 | `	}` |
|      3 | 5650 | `	return PH7_OK;` |
|      2 | 5651 |  |
|      - | 5652 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 5653 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 5654 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 5655 | `#if defined(_MSC_VER)` |
|      - | 5656 | `static const ph7_vfs null_vfs = {` |
|      - | 5657 | `#else` |
|      - | 5658 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 5659 | `#endif` |
|      - | 5660 | `	"null_vfs",` |
|      - | 5661 | `	PH7_VFS_VERSION,` |
|      - | 5662 |  |
|      - | 5663 |  |
|      - | 5664 |  |
|      - | 5665 |  |
|      - | 5666 |  |
|      - | 5667 |  |
|      - | 5668 |  |
|      - | 5669 |  |
|      - | 5670 |  |
|      - | 5671 |  |
|      - | 5672 |  |
|      - | 5673 |  |
|      - | 5674 |  |
|      - | 5675 |  |
|      - | 5676 |  |
|      - | 5677 |  |
|      - | 5678 |  |
|      - | 5679 |  |
|      - | 5680 |  |
|      - | 5681 |  |
|      - | 5682 |  |
|      - | 5683 |  |
|      - | 5684 |  |
|      - | 5685 |  |
|      - | 5686 |  |
|      - | 5687 |  |
|      - | 5688 |  |
|      - | 5689 |  |
|      - | 5690 |  |
|      - | 5691 |  |
|      - | 5692 |  |
|      - | 5693 |  |
|      - | 5694 |  |
|      - | 5695 |  |
|      - | 5696 |  |
|      - | 5697 |  |
|      - | 5698 |  |
|      - | 5699 |  |
|      - | 5700 |  |
|      - | 5701 |  |
|      - | 5702 |  |
|      - | 5703 | `};` |
|      - | 5704 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 5705 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 5706 | `#ifdef __WINNT__` |
|      - | 5707 | `/*` |
|      - | 5708 | ` * Windows VFS implementation for the PH7 engine.` |
|      - | 5709 | ` * Status:` |
|      - | 5710 | ` *    Stable.` |
|      - | 5711 | ` */` |
|      - | 5712 | `/* What follows here is code that is specific to windows systems. */` |
|      - | 5713 | `#include <Windows.h>` |
|      - | 5714 | `#include <stdio.h> /* For popen/pclose pipe stream support */` |
|      - | 5715 | `#include <io.h>    /* For _open_osfhandle, _close */` |
|      - | 5716 | `#include <fcntl.h> /* For _O_RDONLY, _O_WRONLY, _O_TEXT */` |
|      - | 5717 | `/*` |
|      - | 5718 | `** Convert a UTF-8 string to microsoft unicode (UTF-16?).` |
|      - | 5719 | `**` |
|      - | 5720 | `** Space to hold the returned string is obtained from HeapAlloc().` |
|      - | 5721 | `** Taken from the sqlite3 source tree` |
|      - | 5722 | `** status: Public Domain` |
|      - | 5723 | `*/` |
|      2 | 5724 | `static WCHAR *utf8ToUnicode(const char *zFilename){` |
|      - | 5725 | `  int nChar;` |
|      - | 5726 | `  WCHAR *zWideFilename;` |
|      - | 5727 |  |
|      2 | 5728 | `  nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, 0, 0);` |
|      2 | 5729 | `  zWideFilename = (WCHAR *)HeapAlloc(GetProcessHeap(),0,nChar*sizeof(zWideFilename[0]));` |
|      2 | 5730 | `  if( zWideFilename == 0 ){` |
|    ! 0 | 5731 | ` 	return 0;` |
|      - | 5732 | `  }` |
|      2 | 5733 | `  nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, zWideFilename, nChar);` |
|      2 | 5734 | `  if( nChar==0 ){` |
|    ! 0 | 5735 | `    HeapFree(GetProcessHeap(),0,zWideFilename);` |
|    ! 0 | 5736 | `    return 0;` |
|      - | 5737 | `  }` |
|      2 | 5738 | `  return zWideFilename;` |
|      2 | 5739 |  |
|      - | 5740 | `/*` |
|      - | 5741 | `** Convert a UTF-8 filename into whatever form the underlying` |
|      - | 5742 | `** operating system wants filenames in.Space to hold the result` |
|      - | 5743 | `** is obtained from HeapAlloc() and must be freed by the calling` |
|      - | 5744 | `** function.` |
|      - | 5745 | `** Taken from the sqlite3 source tree` |
|      - | 5746 | `** status: Public Domain` |
|      - | 5747 | `*/` |
|      2 | 5748 | `static void *convertUtf8Filename(const char *zFilename){` |
|      - | 5749 | `  void *zConverted;` |
|      2 | 5750 | `  zConverted = utf8ToUnicode(zFilename);` |
|      2 | 5751 | `  return zConverted;` |
|      2 | 5752 |  |
|      - | 5753 | `/*` |
|      - | 5754 | `** Convert microsoft unicode to UTF-8.  Space to hold the returned string is` |
|      - | 5755 | `** obtained from HeapAlloc().` |
|      - | 5756 | `** Taken from the sqlite3 source tree` |
|      - | 5757 | `** status: Public Domain` |
|      - | 5758 | `*/` |
|      2 | 5759 | `static char *unicodeToUtf8(const WCHAR *zWideFilename){` |
|      - | 5760 | `  char *zFilename;` |
|      - | 5761 | `  int nByte;` |
|      - | 5762 |  |
|      2 | 5763 | `  nByte = WideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, 0, 0, 0, 0);` |
|      2 | 5764 | `  zFilename = (char *)HeapAlloc(GetProcessHeap(),0,nByte);` |
|      2 | 5765 | `  if( zFilename == 0 ){` |
|    ! 0 | 5766 | `  	return 0;` |
|      - | 5767 | `  }` |
|      2 | 5768 | `  nByte = WideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, zFilename, nByte,0, 0);` |
|      2 | 5769 | `  if( nByte == 0 ){` |
|    ! 0 | 5770 | `    HeapFree(GetProcessHeap(),0,zFilename);` |
|    ! 0 | 5771 | `    return 0;` |
|      - | 5772 | `  }` |
|      2 | 5773 | `  return zFilename;` |
|      2 | 5774 |  |
|      - | 5775 | `/* int (*xchdir)(const char *) */` |
|      - | 5776 | `static int WinVfs_chdir(const char *zPath)` |
|      2 | 5777 |  |
|      - | 5778 | `	void * pConverted;` |
|      - | 5779 | `	BOOL rc;` |
|      2 | 5780 | `	pConverted = convertUtf8Filename(zPath);` |
|      2 | 5781 | `	if( pConverted == 0 ){` |
|    ! 0 | 5782 | `		return -1;` |
|      - | 5783 | `	}` |
|      2 | 5784 | `	rc = SetCurrentDirectoryW((LPCWSTR)pConverted);` |
|      2 | 5785 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      2 | 5786 | `	return rc ? PH7_OK : -1;` |
|      2 | 5787 |  |
|      - | 5788 | `/* int (*xGetcwd)(ph7_context *) */` |
|      - | 5789 | `static int WinVfs_getcwd(ph7_context *pCtx)` |
|      2 | 5790 |  |
|      - | 5791 | `	WCHAR zDir[2048];` |
|      - | 5792 | `	char *zConverted;` |
|      - | 5793 | `	DWORD rc;` |
|      - | 5794 | `	/* Get the current directory */` |
|      2 | 5795 | `	rc = GetCurrentDirectoryW(sizeof(zDir),zDir);` |
|      2 | 5796 | `	if( rc < 1 ){` |
|    ! 0 | 5797 | `		return -1;` |
|      - | 5798 | `	}` |
|      2 | 5799 | `	zConverted = unicodeToUtf8(zDir);` |
|      2 | 5800 | `	if( zConverted == 0 ){` |
|    ! 0 | 5801 | `		return -1;` |
|      - | 5802 | `	}` |
|      2 | 5803 | `	ph7_result_string(pCtx,zConverted,-1/*Compute length automatically*/); /* Will make it's own copy */` |
|      2 | 5804 | `	HeapFree(GetProcessHeap(),0,zConverted);` |
|      2 | 5805 | `	return PH7_OK;` |
|      2 | 5806 |  |
|      - | 5807 | `/* int (*xMkdir)(const char *,int,int) */` |
|      - | 5808 | `static int WinVfs_mkdir(const char *zPath,int mode,int recursive)` |
|      1 | 5809 |  |
|      - | 5810 | `	void * pConverted;` |
|      - | 5811 | `	BOOL rc;` |
|      1 | 5812 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5813 | `	if( pConverted == 0 ){` |
|    ! 0 | 5814 | `		return -1;` |
|      - | 5815 | `	}` |
|      1 | 5816 | `	mode= 0; /* MSVC warning */` |
|      1 | 5817 | `	recursive = 0;` |
|      1 | 5818 | `	rc = CreateDirectoryW((LPCWSTR)pConverted,0);` |
|      1 | 5819 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5820 | `	return rc ? PH7_OK : -1;` |
|      1 | 5821 |  |
|      - | 5822 | `/* int (*xRmdir)(const char *) */` |
|      - | 5823 | `static int WinVfs_rmdir(const char *zPath)` |
|      1 | 5824 |  |
|      - | 5825 | `	void * pConverted;` |
|      - | 5826 | `	BOOL rc;` |
|      1 | 5827 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5828 | `	if( pConverted == 0 ){` |
|    ! 0 | 5829 | `		return -1;` |
|      - | 5830 | `	}` |
|      1 | 5831 | `	rc = RemoveDirectoryW((LPCWSTR)pConverted);` |
|      1 | 5832 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5833 | `	return rc ? PH7_OK : -1;` |
|      1 | 5834 |  |
|      - | 5835 | `/* int (*xIsdir)(const char *) */` |
|      - | 5836 | `static int WinVfs_isdir(const char *zPath)` |
|      2 | 5837 |  |
|      - | 5838 | `	void * pConverted;` |
|      - | 5839 | `	DWORD dwAttr;` |
|      2 | 5840 | `	pConverted = convertUtf8Filename(zPath);` |
|      2 | 5841 | `	if( pConverted == 0 ){` |
|    ! 0 | 5842 | `		return -1;` |
|      - | 5843 | `	}` |
|      2 | 5844 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|      2 | 5845 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      2 | 5846 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|      1 | 5847 | `		return -1;` |
|      - | 5848 | `	}` |
|      2 | 5849 | `	return (dwAttr & FILE_ATTRIBUTE_DIRECTORY) ? PH7_OK : -1;` |
|      2 | 5850 |  |
|      - | 5851 | `/* int (*xRename)(const char *,const char *) */` |
|      - | 5852 | `static int WinVfs_Rename(const char *zOld,const char *zNew)` |
|      1 | 5853 |  |
|      - | 5854 | `	void *pOld,*pNew;` |
|      1 | 5855 | `	BOOL rc = 0;` |
|      1 | 5856 | `	pOld = convertUtf8Filename(zOld);` |
|      1 | 5857 | `	if( pOld == 0 ){` |
|    ! 0 | 5858 | `		return -1;` |
|      - | 5859 | `	}` |
|      1 | 5860 | `	pNew = convertUtf8Filename(zNew);` |
|      1 | 5861 | `	if( pNew  ){` |
|      1 | 5862 | `		rc = MoveFileW((LPCWSTR)pOld,(LPCWSTR)pNew);` |
|      - | 5863 | `	}` |
|      1 | 5864 | `	HeapFree(GetProcessHeap(),0,pOld);` |
|      1 | 5865 | `	if( pNew ){` |
|      1 | 5866 | `		HeapFree(GetProcessHeap(),0,pNew);` |
|      - | 5867 | `	}` |
|      1 | 5868 | `	return rc ? PH7_OK : - 1;` |
|      1 | 5869 |  |
|      - | 5870 | `/* int (*xRealpath)(const char *,ph7_context *) */` |
|      - | 5871 | `static int WinVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|      1 | 5872 |  |
|      - | 5873 | `	WCHAR zTemp[2048];` |
|      - | 5874 | `	void *pPath;` |
|      - | 5875 | `	char *zReal;` |
|      - | 5876 | `	DWORD n;` |
|      1 | 5877 | `	pPath = convertUtf8Filename(zPath);` |
|      1 | 5878 | `	if( pPath == 0 ){` |
|    ! 0 | 5879 | `		return -1;` |
|      - | 5880 | `	}` |
|      1 | 5881 | `	n = GetFullPathNameW((LPCWSTR)pPath,0,0,0);` |
|      1 | 5882 | `	if( n > 0 ){` |
|      1 | 5883 | `		if( n >= sizeof(zTemp) ){` |
|    ! 0 | 5884 | `			n = sizeof(zTemp) - 1;` |
|      - | 5885 | `		}` |
|      1 | 5886 | `		GetFullPathNameW((LPCWSTR)pPath,n,zTemp,0);` |
|      - | 5887 | `	}` |
|      1 | 5888 | `	HeapFree(GetProcessHeap(),0,pPath);` |
|      1 | 5889 | `	if( !n ){` |
|    ! 0 | 5890 | `		return -1;` |
|      - | 5891 | `	}` |
|      1 | 5892 | `	zReal = unicodeToUtf8(zTemp);` |
|      1 | 5893 | `	if( zReal == 0 ){` |
|    ! 0 | 5894 | `		return -1;` |
|      - | 5895 | `	}` |
|      1 | 5896 | `	ph7_result_string(pCtx,zReal,-1); /* Will make it's own copy */` |
|      1 | 5897 | `	HeapFree(GetProcessHeap(),0,zReal);` |
|      1 | 5898 | `	return PH7_OK;` |
|      1 | 5899 |  |
|      - | 5900 | `/* int (*xSleep)(unsigned int) */` |
|      - | 5901 | `static int WinVfs_Sleep(unsigned int uSec)` |
|      1 | 5902 |  |
|      1 | 5903 | `	Sleep(uSec/1000/*uSec per Millisec */);` |
|      1 | 5904 | `	return PH7_OK;` |
|      1 | 5905 |  |
|      - | 5906 | `/* int (*xUnlink)(const char *) */` |
|      - | 5907 | `static int WinVfs_unlink(const char *zPath)` |
|      2 | 5908 |  |
|      - | 5909 | `	void * pConverted;` |
|      - | 5910 | `	BOOL rc;` |
|      2 | 5911 | `	pConverted = convertUtf8Filename(zPath);` |
|      2 | 5912 | `	if( pConverted == 0 ){` |
|    ! 0 | 5913 | `		return -1;` |
|      - | 5914 | `	}` |
|      2 | 5915 | `	rc = DeleteFileW((LPCWSTR)pConverted);` |
|      2 | 5916 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      2 | 5917 | `	return rc ? PH7_OK : - 1;` |
|      2 | 5918 |  |
|      - | 5919 | `/* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 5920 | `static ph7_int64 WinVfs_DiskFreeSpace(const char *zPath)` |
|    ! 0 | 5921 |  |
|      - | 5922 | `#ifdef _WIN32_WCE` |
|      - | 5923 | `	/* GetDiskFreeSpace is not supported under WINCE */` |
|      - | 5924 | `	SXUNUSED(zPath);` |
|      - | 5925 | `	return 0;` |
|      - | 5926 | `#else` |
|      - | 5927 | `	DWORD dwSectPerClust,dwBytesPerSect,dwFreeClusters,dwTotalClusters;` |
|      - | 5928 | `	void * pConverted;` |
|      - | 5929 | `	WCHAR *p;` |
|      - | 5930 | `	BOOL rc;` |
|    ! 0 | 5931 | `	pConverted = convertUtf8Filename(zPath);` |
|    ! 0 | 5932 | `	if( pConverted == 0 ){` |
|    ! 0 | 5933 | `		return 0;` |
|      - | 5934 | `	}` |
|    ! 0 | 5935 | `	p = (WCHAR *)pConverted;` |
|    ! 0 | 5936 | `	for(;*p;p++){` |
|    ! 0 | 5937 | `		if( *p == '\\' \|\| *p == '/'){` |
|    ! 0 | 5938 | `			*p = '\0';` |
|    ! 0 | 5939 | `			break;` |
|      - | 5940 | `		}` |
|    ! 0 | 5941 | `	}` |
|    ! 0 | 5942 | `	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted,&dwSectPerClust,&dwBytesPerSect,&dwFreeClusters,&dwTotalClusters);` |
|    ! 0 | 5943 | `	if( !rc ){` |
|    ! 0 | 5944 | `		return 0;` |
|      - | 5945 | `	}` |
|    ! 0 | 5946 | `	return (ph7_int64)dwFreeClusters * dwSectPerClust * dwBytesPerSect;` |
|      - | 5947 | `#endif` |
|    ! 0 | 5948 |  |
|      - | 5949 | `/* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 5950 | `static ph7_int64 WinVfs_DiskTotalSpace(const char *zPath)` |
|    ! 0 | 5951 |  |
|      - | 5952 | `#ifdef _WIN32_WCE` |
|      - | 5953 | `	/* GetDiskFreeSpace is not supported under WINCE */` |
|      - | 5954 | `	SXUNUSED(zPath);` |
|      - | 5955 | `	return 0;` |
|      - | 5956 | `#else` |
|      - | 5957 | `	DWORD dwSectPerClust,dwBytesPerSect,dwFreeClusters,dwTotalClusters;` |
|      - | 5958 | `	void * pConverted;` |
|      - | 5959 | `	WCHAR *p;` |
|      - | 5960 | `	BOOL rc;` |
|    ! 0 | 5961 | `	pConverted = convertUtf8Filename(zPath);` |
|    ! 0 | 5962 | `	if( pConverted == 0 ){` |
|    ! 0 | 5963 | `		return 0;` |
|      - | 5964 | `	}` |
|    ! 0 | 5965 | `	p = (WCHAR *)pConverted;` |
|    ! 0 | 5966 | `	for(;*p;p++){` |
|    ! 0 | 5967 | `		if( *p == '\\' \|\| *p == '/'){` |
|    ! 0 | 5968 | `			*p = '\0';` |
|    ! 0 | 5969 | `			break;` |
|      - | 5970 | `		}` |
|    ! 0 | 5971 | `	}` |
|    ! 0 | 5972 | `	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted,&dwSectPerClust,&dwBytesPerSect,&dwFreeClusters,&dwTotalClusters);` |
|    ! 0 | 5973 | `	if( !rc ){` |
|    ! 0 | 5974 | `		return 0;` |
|      - | 5975 | `	}` |
|    ! 0 | 5976 | `	return (ph7_int64)dwTotalClusters * dwSectPerClust * dwBytesPerSect;` |
|      - | 5977 | `#endif` |
|    ! 0 | 5978 |  |
|      - | 5979 | `/* int (*xFileExists)(const char *) */` |
|      - | 5980 | `static int WinVfs_FileExists(const char *zPath)` |
|      1 | 5981 |  |
|      - | 5982 | `	void * pConverted;` |
|      - | 5983 | `	DWORD dwAttr;` |
|      1 | 5984 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5985 | `	if( pConverted == 0 ){` |
|    ! 0 | 5986 | `		return -1;` |
|      - | 5987 | `	}` |
|      1 | 5988 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|      1 | 5989 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5990 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|      1 | 5991 | `		return -1;` |
|      - | 5992 | `	}` |
|      1 | 5993 | `	return PH7_OK;` |
|      1 | 5994 |  |
|      - | 5995 | `/* Open a file in a read-only mode */` |
|      - | 5996 | `static HANDLE OpenReadOnly(LPCWSTR pPath)` |
|      2 | 5997 |  |
|      2 | 5998 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|      2 | 5999 | `	DWORD dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|      2 | 6000 | `	DWORD dwAccess = GENERIC_READ;` |
|      2 | 6001 | `	DWORD dwCreate = OPEN_EXISTING;` |
|      - | 6002 | `	HANDLE pHandle;` |
|      2 | 6003 | `	pHandle = CreateFileW(pPath,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|      2 | 6004 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|      1 | 6005 | `		return 0;` |
|      - | 6006 | `	}` |
|      2 | 6007 | `	return pHandle;` |
|      2 | 6008 |  |
|      - | 6009 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 6010 | `static ph7_int64 WinVfs_FileSize(const char *zPath)` |
|      1 | 6011 |  |
|      - | 6012 | `	DWORD dwLow,dwHigh;` |
|      - | 6013 | `	void * pConverted;` |
|      - | 6014 | `	ph7_int64 nSize;` |
|      - | 6015 | `	HANDLE pHandle;` |
|      - | 6016 |  |
|      1 | 6017 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 6018 | `	if( pConverted == 0 ){` |
|    ! 0 | 6019 | `		return -1;` |
|      - | 6020 | `	}` |
|      - | 6021 | `	/* Open the file in read-only mode */` |
|      1 | 6022 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 6023 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 6024 | `	if( pHandle ){` |
|      1 | 6025 | `		dwLow = GetFileSize(pHandle,&dwHigh);` |
|      1 | 6026 | `		nSize = dwHigh;` |
|      1 | 6027 | `		nSize <<= 32;` |
|      1 | 6028 | `		nSize += dwLow;` |
|      1 | 6029 | `		CloseHandle(pHandle);` |
|      1 | 6030 | `	}else{` |
|    ! 0 | 6031 | `		nSize = -1;` |
|      - | 6032 | `	}` |
|      1 | 6033 | `	return nSize;` |
|      1 | 6034 |  |
|      - | 6035 | `#define TICKS_PER_SECOND 10000000` |
|      - | 6036 | `#define EPOCH_DIFFERENCE 11644473600LL` |
|      - | 6037 | `/* Convert Windows timestamp to UNIX timestamp */` |
|      - | 6038 | `static ph7_int64 convertWindowsTimeToUnixTime(LPFILETIME pTime)` |
|      1 | 6039 |  |
|      - | 6040 | `    ph7_int64 input,temp;` |
|      1 | 6041 | `	input = pTime->dwHighDateTime;` |
|      1 | 6042 | `	input <<= 32;` |
|      1 | 6043 | `	input += pTime->dwLowDateTime;` |
|      1 | 6044 | `    temp = input / TICKS_PER_SECOND; /*convert from 100ns intervals to seconds*/` |
|      1 | 6045 | `    temp = temp - EPOCH_DIFFERENCE;  /*subtract number of seconds between epochs*/` |
|      1 | 6046 | `    return temp;` |
|      1 | 6047 |  |
|      - | 6048 | `/* Convert UNIX timestamp to Windows timestamp */` |
|      - | 6049 | `static void convertUnixTimeToWindowsTime(ph7_int64 nUnixtime,LPFILETIME pOut)` |
|    ! 0 | 6050 |  |
|    ! 0 | 6051 | `  ph7_int64 result = EPOCH_DIFFERENCE;` |
|    ! 0 | 6052 | `  result += nUnixtime;` |
|    ! 0 | 6053 | `  result *= 10000000LL;` |
|    ! 0 | 6054 | `  pOut->dwHighDateTime = (DWORD)(nUnixtime>>32);` |
|    ! 0 | 6055 | `  pOut->dwLowDateTime = (DWORD)nUnixtime;` |
|    ! 0 | 6056 |  |
|      - | 6057 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 6058 | `static int WinVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|      1 | 6059 |  |
|      - | 6060 | `	FILETIME sTouch,sAccess;` |
|      - | 6061 | `	void *pConverted;` |
|      - | 6062 | `	void *pHandle;` |
|      1 | 6063 | `	BOOL rc = 0;` |
|      1 | 6064 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 6065 | `	if( pConverted == 0 ){` |
|    ! 0 | 6066 | `		return -1;` |
|      - | 6067 | `	}` |
|      1 | 6068 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 6069 | `	if( pHandle ){` |
|      1 | 6070 | `		if( touch_time < 0 ){` |
|      1 | 6071 | `			GetSystemTimeAsFileTime(&sTouch);` |
|      1 | 6072 | `		}else{` |
|    ! 0 | 6073 | `			convertUnixTimeToWindowsTime(touch_time,&sTouch);` |
|      - | 6074 | `		}` |
|      1 | 6075 | `		if( access_time < 0 ){` |
|      - | 6076 | `			/* Use the touch time */` |
|      1 | 6077 | `			sAccess = sTouch; /* Structure assignment */` |
|      1 | 6078 | `		}else{` |
|    ! 0 | 6079 | `			convertUnixTimeToWindowsTime(access_time,&sAccess);` |
|      - | 6080 | `		}` |
|      1 | 6081 | `		rc = SetFileTime(pHandle,&sTouch,&sAccess,0);` |
|      - | 6082 | `		/* Close the handle */` |
|      1 | 6083 | `		CloseHandle(pHandle);` |
|      - | 6084 | `	}` |
|      1 | 6085 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 6086 | `	return rc ? PH7_OK : -1;` |
|      1 | 6087 |  |
|      - | 6088 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 6089 | `static ph7_int64 WinVfs_FileAtime(const char *zPath)` |
|      1 | 6090 |  |
|      - | 6091 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|      - | 6092 | `	void * pConverted;` |
|      - | 6093 | `	ph7_int64 atime;` |
|      - | 6094 | `	HANDLE pHandle;` |
|      1 | 6095 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 6096 | `	if( pConverted == 0 ){` |
|    ! 0 | 6097 | `		return -1;` |
|      - | 6098 | `	}` |
|      - | 6099 | `	/* Open the file in read-only mode */` |
|      1 | 6100 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 6101 | `	if( pHandle ){` |
|      - | 6102 | `		BOOL rc;` |
|      1 | 6103 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|      1 | 6104 | `		if( rc ){` |
|      1 | 6105 | `			atime = convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime);` |
|      1 | 6106 | `		}else{` |
|    ! 0 | 6107 | `			atime = -1;` |
|      - | 6108 | `		}` |
|      1 | 6109 | `		CloseHandle(pHandle);` |
|      1 | 6110 | `	}else{` |
|    ! 0 | 6111 | `		atime = -1;` |
|      - | 6112 | `	}` |
|      1 | 6113 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 6114 | `	return atime;` |
|      1 | 6115 |  |
|      - | 6116 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 6117 | `static ph7_int64 WinVfs_FileMtime(const char *zPath)` |
|      1 | 6118 |  |
|      - | 6119 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|      - | 6120 | `	void * pConverted;` |
|      - | 6121 | `	ph7_int64 mtime;` |
|      - | 6122 | `	HANDLE pHandle;` |
|      1 | 6123 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 6124 | `	if( pConverted == 0 ){` |
|    ! 0 | 6125 | `		return -1;` |
|      - | 6126 | `	}` |
|      - | 6127 | `	/* Open the file in read-only mode */` |
|      1 | 6128 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 6129 | `	if( pHandle ){` |
|      - | 6130 | `		BOOL rc;` |
|      1 | 6131 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|      1 | 6132 | `		if( rc ){` |
|      1 | 6133 | `			mtime = convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime);` |
|      1 | 6134 | `		}else{` |
|    ! 0 | 6135 | `			mtime = -1;` |
|      - | 6136 | `		}` |
|      1 | 6137 | `		CloseHandle(pHandle);` |
|      1 | 6138 | `	}else{` |
|    ! 0 | 6139 | `		mtime = -1;` |
|      - | 6140 | `	}` |
|      1 | 6141 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 6142 | `	return mtime;` |
|      1 | 6143 |  |
|      - | 6144 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 6145 | `static ph7_int64 WinVfs_FileCtime(const char *zPath)` |
|      1 | 6146 |  |
|      - | 6147 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|      - | 6148 | `	void * pConverted;` |
|      - | 6149 | `	ph7_int64 ctime;` |
|      - | 6150 | `	HANDLE pHandle;` |
|      1 | 6151 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 6152 | `	if( pConverted == 0 ){` |
|    ! 0 | 6153 | `		return -1;` |
|      - | 6154 | `	}` |
|      - | 6155 | `	/* Open the file in read-only mode */` |
|      1 | 6156 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 6157 | `	if( pHandle ){` |
|      - | 6158 | `		BOOL rc;` |
|      1 | 6159 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|      1 | 6160 | `		if( rc ){` |
|      1 | 6161 | `			ctime = convertWindowsTimeToUnixTime(&sInfo.ftCreationTime);` |
|      1 | 6162 | `		}else{` |
|    ! 0 | 6163 | `			ctime = -1;` |
|      - | 6164 | `		}` |
|      1 | 6165 | `		CloseHandle(pHandle);` |
|      1 | 6166 | `	}else{` |
|    ! 0 | 6167 | `		ctime = -1;` |
|      - | 6168 | `	}` |
|      1 | 6169 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 6170 | `	return ctime;` |
|      1 | 6171 |  |
|      - | 6172 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6173 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6174 | `static int WinVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|      1 | 6175 |  |
|      - | 6176 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|      - | 6177 | `	void *pConverted;` |
|      - | 6178 | `	HANDLE pHandle;` |
|      - | 6179 | `	BOOL rc;` |
|      1 | 6180 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 6181 | `	if( pConverted == 0 ){` |
|    ! 0 | 6182 | `		return -1;` |
|      - | 6183 | `	}` |
|      - | 6184 | `	/* Open the file in read-only mode */` |
|      1 | 6185 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 6186 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 6187 | `	if( pHandle == 0 ){` |
|    ! 0 | 6188 | `		return -1;` |
|      - | 6189 | `	}` |
|      1 | 6190 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|      1 | 6191 | `	CloseHandle(pHandle);` |
|      1 | 6192 | `	if( !rc ){` |
|    ! 0 | 6193 | `		return -1;` |
|      - | 6194 | `	}` |
|      - | 6195 | `	/* dev */` |
|      1 | 6196 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|      1 | 6197 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|      - | 6198 | `	/* ino */` |
|      1 | 6199 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|      1 | 6200 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|      - | 6201 | `	/* mode */` |
|      1 | 6202 | `	ph7_value_int(pWorker,0);` |
|      1 | 6203 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|      - | 6204 | `	/* nlink */` |
|      1 | 6205 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|      1 | 6206 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|      - | 6207 | `	/* uid,gid,rdev */` |
|      1 | 6208 | `	ph7_value_int(pWorker,0);` |
|      1 | 6209 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|      1 | 6210 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|      1 | 6211 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|      - | 6212 | `	/* size */` |
|      1 | 6213 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|      1 | 6214 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|      - | 6215 | `	/* atime */` |
|      1 | 6216 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|      1 | 6217 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|      - | 6218 | `	/* mtime */` |
|      1 | 6219 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|      1 | 6220 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|      - | 6221 | `	/* ctime */` |
|      1 | 6222 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|      1 | 6223 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|      - | 6224 | `	/* blksize,blocks */` |
|      1 | 6225 | `	ph7_value_int(pWorker,0);` |
|      1 | 6226 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|      1 | 6227 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|      1 | 6228 | `	return PH7_OK;` |
|      1 | 6229 |  |
|      - | 6230 | `/* int (*xIsfile)(const char *) */` |
|      - | 6231 | `static int WinVfs_isfile(const char *zPath)` |
|      2 | 6232 |  |
|      - | 6233 | `	void * pConverted;` |
|      - | 6234 | `	DWORD dwAttr;` |
|      2 | 6235 | `	pConverted = convertUtf8Filename(zPath);` |
|      2 | 6236 | `	if( pConverted == 0 ){` |
|    ! 0 | 6237 | `		return -1;` |
|      - | 6238 | `	}` |
|      2 | 6239 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|      2 | 6240 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      2 | 6241 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|      1 | 6242 | `		return -1;` |
|      - | 6243 | `	}` |
|      2 | 6244 | `	return (dwAttr & (FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE)) ? PH7_OK : -1;` |
|      2 | 6245 |  |
|      - | 6246 | `/* int (*xIslink)(const char *) */` |
|      - | 6247 | `static int WinVfs_islink(const char *zPath)` |
|    ! 0 | 6248 |  |
|      - | 6249 | `	void * pConverted;` |
|      - | 6250 | `	DWORD dwAttr;` |
|    ! 0 | 6251 | `	pConverted = convertUtf8Filename(zPath);` |
|    ! 0 | 6252 | `	if( pConverted == 0 ){` |
|    ! 0 | 6253 | `		return -1;` |
|      - | 6254 | `	}` |
|    ! 0 | 6255 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    ! 0 | 6256 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    ! 0 | 6257 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    ! 0 | 6258 | `		return -1;` |
|      - | 6259 | `	}` |
|    ! 0 | 6260 | `	return (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT) ? PH7_OK : -1;` |
|    ! 0 | 6261 |  |
|      - | 6262 | `/* int (*xWritable)(const char *) */` |
|      - | 6263 | `static int WinVfs_iswritable(const char *zPath)` |
|    ! 0 | 6264 |  |
|      - | 6265 | `	void * pConverted;` |
|      - | 6266 | `	DWORD dwAttr;` |
|    ! 0 | 6267 | `	pConverted = convertUtf8Filename(zPath);` |
|    ! 0 | 6268 | `	if( pConverted == 0 ){` |
|    ! 0 | 6269 | `		return -1;` |
|      - | 6270 | `	}` |
|    ! 0 | 6271 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    ! 0 | 6272 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    ! 0 | 6273 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    ! 0 | 6274 | `		return -1;` |
|      - | 6275 | `	}` |
|    ! 0 | 6276 | `	if( (dwAttr & (FILE_ATTRIBUTE_ARCHIVE\|FILE_ATTRIBUTE_NORMAL)) == 0 ){` |
|      - | 6277 | `		/* Not a regular file */` |
|    ! 0 | 6278 | `		return -1;` |
|      - | 6279 | `	}` |
|    ! 0 | 6280 | `	if( dwAttr & FILE_ATTRIBUTE_READONLY ){` |
|      - | 6281 | `		/* Read-only file */` |
|    ! 0 | 6282 | `		return -1;` |
|      - | 6283 | `	}` |
|      - | 6284 | `	/* File is writable */` |
|    ! 0 | 6285 | `	return PH7_OK;` |
|    ! 0 | 6286 |  |
|      - | 6287 | `/* int (*xExecutable)(const char *) */` |
|      - | 6288 | `static int WinVfs_isexecutable(const char *zPath)` |
|    ! 0 | 6289 |  |
|      - | 6290 | `	void * pConverted;` |
|      - | 6291 | `	DWORD dwAttr;` |
|    ! 0 | 6292 | `	pConverted = convertUtf8Filename(zPath);` |
|    ! 0 | 6293 | `	if( pConverted == 0 ){` |
|    ! 0 | 6294 | `		return -1;` |
|      - | 6295 | `	}` |
|    ! 0 | 6296 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    ! 0 | 6297 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    ! 0 | 6298 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    ! 0 | 6299 | `		return -1;` |
|      - | 6300 | `	}` |
|    ! 0 | 6301 | `	if( (dwAttr & FILE_ATTRIBUTE_NORMAL) == 0 ){` |
|      - | 6302 | `		/* Not a regular file */` |
|    ! 0 | 6303 | `		return -1;` |
|      - | 6304 | `	}` |
|      - | 6305 | `	/* File is executable */` |
|    ! 0 | 6306 | `	return PH7_OK;` |
|    ! 0 | 6307 |  |
|      - | 6308 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 6309 | `static int WinVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|      1 | 6310 |  |
|      - | 6311 | `	void * pConverted;` |
|      - | 6312 | `	DWORD dwAttr;` |
|      1 | 6313 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 6314 | `	if( pConverted == 0 ){` |
|      - | 6315 | `		/* Expand 'unknown' */` |
|    ! 0 | 6316 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 6317 | `		return -1;` |
|      - | 6318 | `	}` |
|      1 | 6319 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|      1 | 6320 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 6321 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|      - | 6322 | `		/* Expand 'unknown' */` |
|    ! 0 | 6323 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 6324 | `		return -1;` |
|      - | 6325 | `	}` |
|      1 | 6326 | `	if(dwAttr & (FILE_ATTRIBUTE_HIDDEN\|FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE) ){` |
|      1 | 6327 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|      1 | 6328 | `	}else if(dwAttr & FILE_ATTRIBUTE_DIRECTORY){` |
|      1 | 6329 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|    ! 0 | 6330 | `	}else if(dwAttr & FILE_ATTRIBUTE_REPARSE_POINT){` |
|    ! 0 | 6331 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|    ! 0 | 6332 | `	}else if(dwAttr & (FILE_ATTRIBUTE_DEVICE)){` |
|    ! 0 | 6333 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|    ! 0 | 6334 | `	}else{` |
|    ! 0 | 6335 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|      - | 6336 | `	}` |
|      1 | 6337 | `	return PH7_OK;` |
|      1 | 6338 |  |
|      - | 6339 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 6340 | `static int WinVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|      2 | 6341 |  |
|      - | 6342 | `	char zValue[1024];` |
|      - | 6343 | `	DWORD n;` |
|      - | 6344 | `	/*` |
|      - | 6345 | `	 * According to MSDN` |
|      - | 6346 | `	 * If lpBuffer is not large enough to hold the data, the return` |
|      - | 6347 | `	 * value is the buffer size, in characters, required to hold the` |
|      - | 6348 | `	 * string and its terminating null character and the contents` |
|      - | 6349 | `	 * of lpBuffer are undefined.` |
|      - | 6350 | `	 */` |
|      2 | 6351 | `	n = sizeof(zValue);` |
|      2 | 6352 | `	SyMemcpy("Undefined",zValue,sizeof("Undefined")-1);` |
|      - | 6353 | `	/* Extract the environment value */` |
|      2 | 6354 | `	n = GetEnvironmentVariableA(zVar,zValue,sizeof(zValue));` |
|      2 | 6355 | `	if( !n ){` |
|      - | 6356 | `		/* No such variable*/` |
|    ! 0 | 6357 | `		return -1;` |
|      - | 6358 | `	}` |
|      2 | 6359 | `	ph7_result_string(pCtx,zValue,(int)n);` |
|      2 | 6360 | `	return PH7_OK;` |
|      2 | 6361 |  |
|      - | 6362 | `/* int (*xSetenv)(const char *,const char *) */` |
|      - | 6363 | `static int WinVfs_Setenv(const char *zName,const char *zValue)` |
|      1 | 6364 |  |
|      - | 6365 | `	BOOL rc;` |
|      1 | 6366 | `	rc = SetEnvironmentVariableA(zName,zValue);` |
|      1 | 6367 | `	return rc ? PH7_OK : -1;` |
|      1 | 6368 |  |
|      - | 6369 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 6370 | `static int WinVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|      2 | 6371 |  |
|      - | 6372 | `	DWORD dwSizeLow,dwSizeHigh;` |
|      - | 6373 | `	HANDLE pHandle,pMapHandle;` |
|      - | 6374 | `	void *pConverted,*pView;` |
|      - | 6375 |  |
|      2 | 6376 | `	pConverted = convertUtf8Filename(zPath);` |
|      2 | 6377 | `	if( pConverted == 0 ){` |
|    ! 0 | 6378 | `		return -1;` |
|      - | 6379 | `	}` |
|      2 | 6380 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      2 | 6381 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      2 | 6382 | `	if( pHandle == 0 ){` |
|      1 | 6383 | `		return -1;` |
|      - | 6384 | `	}` |
|      - | 6385 | `	/* Get the file size */` |
|      2 | 6386 | `	dwSizeLow = GetFileSize(pHandle,&dwSizeHigh);` |
|      - | 6387 | `	/* Create the mapping */` |
|      2 | 6388 | `	pMapHandle = CreateFileMappingW(pHandle,0,PAGE_READONLY,dwSizeHigh,dwSizeLow,0);` |
|      2 | 6389 | `	if( pMapHandle == 0 ){` |
|    ! 0 | 6390 | `		CloseHandle(pHandle);` |
|    ! 0 | 6391 | `		return -1;` |
|      - | 6392 | `	}` |
|      2 | 6393 | `	*pSize = ((ph7_int64)dwSizeHigh << 32) \| dwSizeLow;` |
|      - | 6394 | `	/* Obtain the view */` |
|      2 | 6395 | `	pView = MapViewOfFile(pMapHandle,FILE_MAP_READ,0,0,(SIZE_T)(*pSize));` |
|      2 | 6396 | `	if( pView ){` |
|      - | 6397 | `		/* Let the upper layer point to the view */` |
|      2 | 6398 | `		*ppMap = pView;` |
|      - | 6399 | `	}` |
|      - | 6400 | `	/* Close the handle` |
|      - | 6401 | `	 * According to MSDN it's OK the close the HANDLES.` |
|      - | 6402 | `	 */` |
|      2 | 6403 | `	CloseHandle(pMapHandle);` |
|      2 | 6404 | `	CloseHandle(pHandle);` |
|      2 | 6405 | `	return pView ? PH7_OK : -1;` |
|      2 | 6406 |  |
|      - | 6407 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|      - | 6408 | `static void WinVfs_Unmap(void *pView,ph7_int64 nSize)` |
|      2 | 6409 |  |
|      2 | 6410 | `	nSize = 0; /* Compiler warning */` |
|      2 | 6411 | `	UnmapViewOfFile(pView);` |
|      2 | 6412 |  |
|      - | 6413 | `/* void (*xTempDir)(ph7_context *) */` |
|      - | 6414 | `static void WinVfs_TempDir(ph7_context *pCtx)` |
|      2 | 6415 |  |
|      - | 6416 | `	CHAR zTemp[1024];` |
|      - | 6417 | `	DWORD n;` |
|      2 | 6418 | `	n = GetTempPathA(sizeof(zTemp),zTemp);` |
|      2 | 6419 | `	if( n < 1 ){` |
|      - | 6420 | `		/* Assume the default windows temp directory */` |
|    ! 0 | 6421 | `		ph7_result_string(pCtx,"C:\\Windows\\Temp",-1/*Compute length automatically*/);` |
|    ! 0 | 6422 | `	}else{` |
|      2 | 6423 | `		ph7_result_string(pCtx,zTemp,(int)n);` |
|      - | 6424 | `	}` |
|      2 | 6425 |  |
|      - | 6426 | `/* unsigned int (*xProcessId)(void) */` |
|      - | 6427 | `static unsigned int WinVfs_ProcessId(void)` |
|      1 | 6428 |  |
|      1 | 6429 | `	DWORD nID = 0;` |
|      - | 6430 | `#ifndef __MINGW32__` |
|      1 | 6431 | `	nID = GetProcessId(GetCurrentProcess());` |
|      - | 6432 | `#endif /* __MINGW32__ */` |
|      1 | 6433 | `	return (unsigned int)nID;` |
|      1 | 6434 |  |
|      - | 6435 | `/* void (*xUsername)(ph7_context *) */` |
|      - | 6436 | `static void WinVfs_Username(ph7_context *pCtx)` |
|      1 | 6437 |  |
|      - | 6438 | `	WCHAR zUser[1024];` |
|      - | 6439 | `	DWORD nByte;` |
|      - | 6440 | `	BOOL rc;` |
|      1 | 6441 | `	nByte = sizeof(zUser);` |
|      1 | 6442 | `	rc = GetUserNameW(zUser,&nByte);` |
|      1 | 6443 | `	if( !rc ){` |
|      - | 6444 | `		/* Set a dummy name */` |
|    ! 0 | 6445 | `		ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|    ! 0 | 6446 | `	}else{` |
|      - | 6447 | `		char *zName;` |
|      1 | 6448 | `		zName = unicodeToUtf8(zUser);` |
|      1 | 6449 | `		if( zName == 0 ){` |
|    ! 0 | 6450 | `			ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|    ! 0 | 6451 | `		}else{` |
|      1 | 6452 | `			ph7_result_string(pCtx,zName,-1/*Compute length automatically*/); /* Will make it's own copy */` |
|      1 | 6453 | `			HeapFree(GetProcessHeap(),0,zName);` |
|      - | 6454 | `		}` |
|      - | 6455 | `	}` |
|      - | 6456 |  |
|      1 | 6457 |  |
|      - | 6458 | `/* Export the windows vfs */` |
|      - | 6459 | `static const ph7_vfs sWinVfs = {` |
|      - | 6460 | `	"Windows_vfs",` |
|      - | 6461 | `	PH7_VFS_VERSION,` |
|      - | 6462 | `	WinVfs_chdir,    /* int (*xChdir)(const char *) */` |
|      - | 6463 |  |
|      - | 6464 | `	WinVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|      - | 6465 | `	WinVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|      - | 6466 | `	WinVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|      - | 6467 | `	WinVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|      - | 6468 | `	WinVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|      - | 6469 | `	WinVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 6470 | `	WinVfs_Sleep,               /* int (*xSleep)(unsigned int) */` |
|      - | 6471 | `	WinVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|      - | 6472 | `	WinVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|      - | 6473 |  |
|      - | 6474 |  |
|      - | 6475 |  |
|      - | 6476 | `	WinVfs_DiskFreeSpace,/* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 6477 | `	WinVfs_DiskTotalSpace,/* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 6478 | `	WinVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 6479 | `	WinVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 6480 | `	WinVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 6481 | `	WinVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 6482 | `	WinVfs_Stat, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6483 | `	WinVfs_Stat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6484 | `	WinVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|      - | 6485 | `	WinVfs_islink,     /* int (*xIslink)(const char *) */` |
|      - | 6486 | `	WinVfs_isfile,     /* int (*xReadable)(const char *) */` |
|      - | 6487 | `	WinVfs_iswritable, /* int (*xWritable)(const char *) */` |
|      - | 6488 | `	WinVfs_isexecutable, /* int (*xExecutable)(const char *) */` |
|      - | 6489 | `	WinVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 6490 | `	WinVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 6491 | `	WinVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|      - | 6492 | `	WinVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 6493 | `	WinVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 6494 | `	WinVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 6495 |  |
|      - | 6496 |  |
|      - | 6497 | `	WinVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|      - | 6498 | `	WinVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|      - | 6499 |  |
|      - | 6500 |  |
|      - | 6501 | `	WinVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|      - | 6502 |  |
|      - | 6503 | `};` |
|      - | 6504 | `/* Windows file IO */` |
|      - | 6505 | `#ifndef INVALID_SET_FILE_POINTER` |
|      - | 6506 | `# define INVALID_SET_FILE_POINTER ((DWORD)-1)` |
|      - | 6507 | `#endif` |
|      - | 6508 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|      - | 6509 | `static int WinFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|      2 | 6510 |  |
|      2 | 6511 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|      2 | 6512 | `	DWORD dwAccess = GENERIC_READ;` |
|      - | 6513 | `	DWORD dwShare,dwCreate;` |
|      - | 6514 | `	void *pConverted;` |
|      - | 6515 | `	HANDLE pHandle;` |
|      - | 6516 |  |
|      2 | 6517 | `	pConverted = convertUtf8Filename(zPath);` |
|      2 | 6518 | `	if( pConverted == 0 ){` |
|    ! 0 | 6519 | `		return -1;` |
|      - | 6520 | `	}` |
|      - | 6521 | `	/* Set the desired flags according to the open mode */` |
|      2 | 6522 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|      - | 6523 | `		/* Open existing file, or create if it doesn't exist */` |
|      2 | 6524 | `		dwCreate = OPEN_ALWAYS;` |
|      2 | 6525 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|      - | 6526 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
|      2 | 6527 | `			dwCreate = CREATE_ALWAYS;` |
|      2 | 6528 | `		}` |
|      2 | 6529 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|      - | 6530 | `		/* Creates a new file, only if it does not already exist.` |
|      - | 6531 | `		* If the file exists, it fails.` |
|      - | 6532 | `		*/` |
|    ! 0 | 6533 | `		dwCreate = CREATE_NEW;` |
|      2 | 6534 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|      - | 6535 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|      - | 6536 | `		 * The file must exist.` |
|      - | 6537 | `		 */` |
|    ! 0 | 6538 | `		dwCreate = TRUNCATE_EXISTING;` |
|    ! 0 | 6539 | `	}else{` |
|      - | 6540 | `		/* Opens a file, only if it exists. */` |
|      2 | 6541 | `		dwCreate = OPEN_EXISTING;` |
|      - | 6542 | `	}` |
|      2 | 6543 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|      - | 6544 | `		/* Read+Write access */` |
|      2 | 6545 | `		dwAccess \|= GENERIC_WRITE;` |
|      2 | 6546 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|      - | 6547 | `		/* Write only access */` |
|      1 | 6548 | `		dwAccess = GENERIC_WRITE;` |
|      - | 6549 | `	}` |
|      2 | 6550 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|      - | 6551 | `		/* Append mode */` |
|    ! 0 | 6552 | `		dwAccess = FILE_APPEND_DATA;` |
|      - | 6553 | `	}` |
|      2 | 6554 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|      - | 6555 | `		/* File is temporary */` |
|    ! 0 | 6556 | `		dwType = FILE_ATTRIBUTE_TEMPORARY;` |
|      - | 6557 | `	}` |
|      2 | 6558 | `	dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|      2 | 6559 | `	pHandle = CreateFileW((LPCWSTR)pConverted,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|      2 | 6560 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      2 | 6561 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|      - | 6562 | `		SXUNUSED(pResource); /* MSVC warning */` |
|      1 | 6563 | `		return -1;` |
|      - | 6564 | `	}` |
|      - | 6565 | `	/* Make the handle accessible to the upper layer */` |
|      2 | 6566 | `	*ppHandle = (void *)pHandle;` |
|      2 | 6567 | `	return PH7_OK;` |
|      2 | 6568 |  |
|      - | 6569 | `/* An instance of the following structure is used to record state information` |
|      - | 6570 | ` * while iterating throw directory entries.` |
|      - | 6571 | ` */` |
|      - | 6572 | `typedef struct WinDir_Info WinDir_Info;` |
|      - | 6573 | `struct WinDir_Info` |
|      - | 6574 |  |
|      - | 6575 | `	HANDLE pDirHandle;` |
|      - | 6576 | `	void *pPath;` |
|      - | 6577 | `	WIN32_FIND_DATAW sInfo;` |
|      - | 6578 | `	int rc;` |
|      - | 6579 | `};` |
|      - | 6580 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|      - | 6581 | `static int WinDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|      2 | 6582 |  |
|      - | 6583 | `	WinDir_Info *pDirInfo;` |
|      - | 6584 | `	void *pConverted;` |
|      - | 6585 | `	char *zPrep;` |
|      - | 6586 | `	sxu32 n;` |
|      - | 6587 | `	/* Prepare the path */` |
|      2 | 6588 | `	n = SyStrlen(zPath);` |
|      2 | 6589 | `	zPrep = (char *)HeapAlloc(GetProcessHeap(),0,n+sizeof("\\*")+4);` |
|      2 | 6590 | `	if( zPrep == 0 ){` |
|    ! 0 | 6591 | `		return -1;` |
|      - | 6592 | `	}` |
|      2 | 6593 | `	SyMemcpy((const void *)zPath,zPrep,n);` |
|      2 | 6594 | `	zPrep[n]   = '\\';` |
|      2 | 6595 | `	zPrep[n+1] =  '*';` |
|      2 | 6596 | `	zPrep[n+2] = 0;` |
|      2 | 6597 | `	pConverted = convertUtf8Filename(zPrep);` |
|      2 | 6598 | `	HeapFree(GetProcessHeap(),0,zPrep);` |
|      2 | 6599 | `	if( pConverted == 0 ){` |
|    ! 0 | 6600 | `		return -1;` |
|      - | 6601 | `	}` |
|      - | 6602 | `	/* Allocate a new instance */` |
|      2 | 6603 | `	pDirInfo = (WinDir_Info *)HeapAlloc(GetProcessHeap(),0,sizeof(WinDir_Info));` |
|      2 | 6604 | `	if( pDirInfo == 0 ){` |
|    ! 0 | 6605 | `		pResource = 0; /* Compiler warning */` |
|    ! 0 | 6606 | `		return -1;` |
|      - | 6607 | `	}` |
|      2 | 6608 | `	pDirInfo->rc = SXRET_OK;` |
|      2 | 6609 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pConverted,&pDirInfo->sInfo);` |
|      2 | 6610 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|      - | 6611 | `		/* Cannot open directory */` |
|    ! 0 | 6612 | `		HeapFree(GetProcessHeap(),0,pConverted);` |
|    ! 0 | 6613 | `		HeapFree(GetProcessHeap(),0,pDirInfo);` |
|    ! 0 | 6614 | `		return -1;` |
|      - | 6615 | `	}` |
|      - | 6616 | `	/* Save the path */` |
|      2 | 6617 | `	pDirInfo->pPath = pConverted;` |
|      - | 6618 | `	/* Save our structure */` |
|      2 | 6619 | `	*ppHandle = pDirInfo;` |
|      2 | 6620 | `	return PH7_OK;` |
|      2 | 6621 |  |
|      - | 6622 | `/* void (*xCloseDir)(void *) */` |
|      - | 6623 | `static void WinDir_Close(void *pUserData)` |
|      2 | 6624 |  |
|      2 | 6625 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|      2 | 6626 | `	if( pDirInfo->pDirHandle != INVALID_HANDLE_VALUE ){` |
|      2 | 6627 | `		FindClose(pDirInfo->pDirHandle);` |
|      - | 6628 | `	}` |
|      2 | 6629 | `	HeapFree(GetProcessHeap(),0,pDirInfo->pPath);` |
|      2 | 6630 | `	HeapFree(GetProcessHeap(),0,pDirInfo);` |
|      2 | 6631 |  |
|      - | 6632 | `/* void (*xClose)(void *); */` |
|      - | 6633 | `static void WinFile_Close(void *pUserData)` |
|      2 | 6634 |  |
|      2 | 6635 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      2 | 6636 | `	CloseHandle(pHandle);` |
|      2 | 6637 |  |
|      - | 6638 | `/* int (*xReadDir)(void *,ph7_context *) */` |
|      - | 6639 | `static int WinDir_Read(void *pUserData,ph7_context *pCtx)` |
|      2 | 6640 |  |
|      2 | 6641 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|      - | 6642 | `	LPWIN32_FIND_DATAW pData;` |
|      - | 6643 | `	char *zName;` |
|      - | 6644 | `	BOOL rc;` |
|      - | 6645 | `	sxu32 n;` |
|      2 | 6646 | `	if( pDirInfo->rc != SXRET_OK ){` |
|      - | 6647 | `		/* No more entry to process */` |
|      2 | 6648 | `		return -1;` |
|      - | 6649 | `	}` |
|      2 | 6650 | `	pData = &pDirInfo->sInfo;` |
|      - | 6651 | `	for(;;){` |
|      2 | 6652 | `		zName = unicodeToUtf8(pData->cFileName);` |
|      2 | 6653 | `		if( zName == 0 ){` |
|      - | 6654 | `			/* Out of memory */` |
|    ! 0 | 6655 | `			return -1;` |
|      - | 6656 | `		}` |
|      2 | 6657 | `		n = SyStrlen(zName);` |
|      - | 6658 | `		/* Ignore '.' && '..' */` |
|      2 | 6659 | `		if( n > sizeof("..")-1 \|\| zName[0] != '.' \|\| ( n == sizeof("..")-1 && zName[1] != '.') ){` |
|      2 | 6660 | `			break;` |
|      - | 6661 | `		}` |
|      2 | 6662 | `		HeapFree(GetProcessHeap(),0,zName);` |
|      2 | 6663 | `		rc = FindNextFileW(pDirInfo->pDirHandle,&pDirInfo->sInfo);` |
|      2 | 6664 | `		if( !rc ){` |
|    ! 0 | 6665 | `			return -1;` |
|      - | 6666 | `		}` |
|      2 | 6667 | `	}` |
|      - | 6668 | `	/* Return the current file name */` |
|      2 | 6669 | `	ph7_result_string(pCtx,zName,-1);` |
|      2 | 6670 | `	HeapFree(GetProcessHeap(),0,zName);` |
|      - | 6671 | `	/* Point to the next entry */` |
|      2 | 6672 | `	rc = FindNextFileW(pDirInfo->pDirHandle,&pDirInfo->sInfo);` |
|      2 | 6673 | `	if( !rc ){` |
|      2 | 6674 | `		pDirInfo->rc = SXERR_EOF;` |
|      - | 6675 | `	}` |
|      2 | 6676 | `	return PH7_OK;` |
|      2 | 6677 |  |
|      - | 6678 | `/* void (*xRewindDir)(void *) */` |
|      - | 6679 | `static void WinDir_RewindDir(void *pUserData)` |
|      1 | 6680 |  |
|      1 | 6681 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|      1 | 6682 | `	FindClose(pDirInfo->pDirHandle);` |
|      1 | 6683 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pDirInfo->pPath,&pDirInfo->sInfo);` |
|      1 | 6684 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|    ! 0 | 6685 | `		pDirInfo->rc = SXERR_EOF;` |
|    ! 0 | 6686 | `	}else{` |
|      1 | 6687 | `		pDirInfo->rc = SXRET_OK;` |
|      - | 6688 | `	}` |
|      1 | 6689 |  |
|      - | 6690 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
|      - | 6691 | `static ph7_int64 WinFile_Read(void *pOS,void *pBuffer,ph7_int64 nDatatoRead)` |
|      2 | 6692 |  |
|      2 | 6693 | `	HANDLE pHandle = (HANDLE)pOS;` |
|      - | 6694 | `	DWORD nRd;` |
|      - | 6695 | `	BOOL rc;` |
|      2 | 6696 | `	rc = ReadFile(pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|      2 | 6697 | `	if( !rc ){` |
|      - | 6698 | `		/* EOF or IO error */` |
|    ! 0 | 6699 | `		return -1;` |
|      - | 6700 | `	}` |
|      2 | 6701 | `	return (ph7_int64)nRd;` |
|      2 | 6702 |  |
|      - | 6703 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
|      - | 6704 | `static ph7_int64 WinFile_Write(void *pOS,const void *pBuffer,ph7_int64 nWrite)` |
|      2 | 6705 |  |
|      2 | 6706 | `	const char *zData = (const char *)pBuffer;` |
|      2 | 6707 | `	HANDLE pHandle = (HANDLE)pOS;` |
|      - | 6708 | `	ph7_int64 nCount;` |
|      - | 6709 | `	DWORD nWr;` |
|      - | 6710 | `	BOOL rc;` |
|      2 | 6711 | `	nWr = 0;` |
|      2 | 6712 | `	nCount = 0;` |
|      - | 6713 | `	for(;;){` |
|      2 | 6714 | `		if( nWrite < 1 ){` |
|      2 | 6715 | `			break;` |
|      - | 6716 | `		}` |
|      2 | 6717 | `		rc = WriteFile(pHandle,zData,(DWORD)nWrite,&nWr,0);` |
|      2 | 6718 | `		if( !rc ){` |
|      - | 6719 | `			/* IO error */` |
|    ! 0 | 6720 | `			break;` |
|      - | 6721 | `		}` |
|      2 | 6722 | `		nWrite -= nWr;` |
|      2 | 6723 | `		nCount += nWr;` |
|      2 | 6724 | `		zData += nWr;` |
|      2 | 6725 | `	}` |
|      2 | 6726 | `	if( nWrite > 0 ){` |
|    ! 0 | 6727 | `		return -1;` |
|      - | 6728 | `	}` |
|      2 | 6729 | `	return nCount;` |
|      2 | 6730 |  |
|      - | 6731 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|      - | 6732 | `static int WinFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|      1 | 6733 |  |
|      1 | 6734 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6735 | `	DWORD dwMove,dwNew;` |
|      - | 6736 | `	LONG nHighOfft;` |
|      1 | 6737 | `	switch(whence){` |
|      - | 6738 | `	case 1:/*SEEK_CUR*/` |
|    ! 0 | 6739 | `		dwMove = FILE_CURRENT;` |
|    ! 0 | 6740 | `		break;` |
|      - | 6741 | `	case 2: /* SEEK_END */` |
|    ! 0 | 6742 | `		dwMove = FILE_END;` |
|    ! 0 | 6743 | `		break;` |
|      - | 6744 | `	case 0: /* SEEK_SET */` |
|      - | 6745 | `	default:` |
|      1 | 6746 | `		dwMove = FILE_BEGIN;` |
|      - | 6747 | `		break;` |
|      - | 6748 | `	}` |
|      1 | 6749 | `	nHighOfft = (LONG)(iOfft >> 32);` |
|      1 | 6750 | `	dwNew = SetFilePointer(pHandle,(LONG)iOfft,&nHighOfft,dwMove);` |
|      1 | 6751 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|    ! 0 | 6752 | `		return -1;` |
|      - | 6753 | `	}` |
|      1 | 6754 | `	return PH7_OK;` |
|      1 | 6755 |  |
|      - | 6756 | `/* int (*xLock)(void *,int) */` |
|      - | 6757 | `static int WinFile_Lock(void *pUserData,int lock_type)` |
|      1 | 6758 |  |
|      1 | 6759 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6760 | `	static DWORD dwLo = 0,dwHi = 0; /* xx: MT-SAFE */` |
|      - | 6761 | `	OVERLAPPED sDummy;` |
|      - | 6762 | `	BOOL rc;` |
|      1 | 6763 | `	SyZero(&sDummy,sizeof(sDummy));` |
|      - | 6764 | `	/* Get the file size */` |
|      1 | 6765 | `	if( lock_type < 1 ){` |
|      - | 6766 | `		/* Unlock the file */` |
|      1 | 6767 | `		rc = UnlockFileEx(pHandle,0,dwLo,dwHi,&sDummy);` |
|      1 | 6768 | `	}else{` |
|      1 | 6769 | `		DWORD dwFlags = LOCKFILE_FAIL_IMMEDIATELY; /* Shared non-blocking lock by default*/` |
|      - | 6770 | `		/* Lock the file */` |
|      1 | 6771 | `		if( lock_type == 1 /* LOCK_EXCL */ ){` |
|      1 | 6772 | `			dwFlags \|= LOCKFILE_EXCLUSIVE_LOCK;` |
|      - | 6773 | `		}` |
|      1 | 6774 | `		dwLo = GetFileSize(pHandle,&dwHi);` |
|      1 | 6775 | `		rc = LockFileEx(pHandle,dwFlags,0,dwLo,dwHi,&sDummy);` |
|      - | 6776 | `	}` |
|      1 | 6777 | `	return rc ? PH7_OK : -1 /* Lock error */;` |
|      1 | 6778 |  |
|      - | 6779 | `/* ph7_int64 (*xTell)(void *) */` |
|      - | 6780 | `static ph7_int64 WinFile_Tell(void *pUserData)` |
|      1 | 6781 |  |
|      1 | 6782 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6783 | `	DWORD dwNew;` |
|      1 | 6784 | `	dwNew = SetFilePointer(pHandle,0,0,FILE_CURRENT/* SEEK_CUR */);` |
|      1 | 6785 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|    ! 0 | 6786 | `		return -1;` |
|      - | 6787 | `	}` |
|      1 | 6788 | `	return (ph7_int64)dwNew;` |
|      1 | 6789 |  |
|      - | 6790 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|      - | 6791 | `static int WinFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|      1 | 6792 |  |
|      1 | 6793 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6794 | `	LONG HighOfft;` |
|      - | 6795 | `	DWORD dwNew;` |
|      - | 6796 | `	BOOL rc;` |
|      1 | 6797 | `	HighOfft = (LONG)(nOfft >> 32);` |
|      1 | 6798 | `	dwNew = SetFilePointer(pHandle,(LONG)nOfft,&HighOfft,FILE_BEGIN);` |
|      1 | 6799 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|    ! 0 | 6800 | `		return -1;` |
|      - | 6801 | `	}` |
|      1 | 6802 | `	rc = SetEndOfFile(pHandle);` |
|      1 | 6803 | `	return rc ? PH7_OK : -1;` |
|      1 | 6804 |  |
|      - | 6805 | `/* int (*xSync)(void *); */` |
|      - | 6806 | `static int WinFile_Sync(void *pUserData)` |
|      1 | 6807 |  |
|      1 | 6808 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6809 | `	BOOL rc;` |
|      1 | 6810 | `	rc = FlushFileBuffers(pHandle);` |
|      1 | 6811 | `	return rc ? PH7_OK : - 1;` |
|      1 | 6812 |  |
|      - | 6813 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|      - | 6814 | `static int WinFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|      1 | 6815 |  |
|      - | 6816 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|      1 | 6817 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6818 | `	BOOL rc;` |
|      1 | 6819 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|      1 | 6820 | `	if( !rc ){` |
|    ! 0 | 6821 | `		return -1;` |
|      - | 6822 | `	}` |
|      - | 6823 | `	/* dev */` |
|      1 | 6824 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|      1 | 6825 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|      - | 6826 | `	/* ino */` |
|      1 | 6827 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|      1 | 6828 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|      - | 6829 | `	/* mode */` |
|      1 | 6830 | `	ph7_value_int(pWorker,0);` |
|      1 | 6831 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|      - | 6832 | `	/* nlink */` |
|      1 | 6833 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|      1 | 6834 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|      - | 6835 | `	/* uid,gid,rdev */` |
|      1 | 6836 | `	ph7_value_int(pWorker,0);` |
|      1 | 6837 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|      1 | 6838 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|      1 | 6839 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|      - | 6840 | `	/* size */` |
|      1 | 6841 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|      1 | 6842 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|      - | 6843 | `	/* atime */` |
|      1 | 6844 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|      1 | 6845 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|      - | 6846 | `	/* mtime */` |
|      1 | 6847 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|      1 | 6848 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|      - | 6849 | `	/* ctime */` |
|      1 | 6850 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|      1 | 6851 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|      - | 6852 | `	/* blksize,blocks */` |
|      1 | 6853 | `	ph7_value_int(pWorker,0);` |
|      1 | 6854 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|      1 | 6855 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|      1 | 6856 | `	return PH7_OK;` |
|      1 | 6857 |  |
|      - | 6858 | `/* Export the file:// stream */` |
|      - | 6859 | `static const ph7_io_stream sWinFileStream = {` |
|      - | 6860 | `	"file", /* Stream name */` |
|      - | 6861 | `	PH7_IO_STREAM_VERSION,` |
|      - | 6862 | `	WinFile_Open,  /* xOpen */` |
|      - | 6863 | `	WinDir_Open,   /* xOpenDir */` |
|      - | 6864 | `	WinFile_Close, /* xClose */` |
|      - | 6865 | `	WinDir_Close,  /* xCloseDir */` |
|      - | 6866 | `	WinFile_Read,  /* xRead */` |
|      - | 6867 | `	WinDir_Read,   /* xReadDir */` |
|      - | 6868 | `	WinFile_Write, /* xWrite */` |
|      - | 6869 | `	WinFile_Seek,  /* xSeek */` |
|      - | 6870 | `	WinFile_Lock,  /* xLock */` |
|      - | 6871 | `	WinDir_RewindDir, /* xRewindDir */` |
|      - | 6872 | `	WinFile_Tell,  /* xTell */` |
|      - | 6873 | `	WinFile_Trunc, /* xTrunc */` |
|      - | 6874 | `	WinFile_Sync,  /* xSeek */` |
|      - | 6875 | `	WinFile_Stat   /* xStat */` |
|      - | 6876 | `};` |
|      - | 6877 | `#elif defined(__UNIXES__)` |
|      - | 6878 | `/*` |
|      - | 6879 | ` * UNIX VFS implementation for the PH7 engine.` |
|      - | 6880 | ` * Status:` |
|      - | 6881 | ` *    Stable.` |
|      - | 6882 | ` */` |
|      - | 6883 | `#include <sys/types.h>` |
|      - | 6884 | `#include <limits.h>` |
|      - | 6885 | `#include <fcntl.h>` |
|      - | 6886 | `#include <unistd.h>` |
|      - | 6887 | `#include <sys/uio.h>` |
|      - | 6888 | `#include <sys/stat.h>` |
|      - | 6889 | `#include <sys/mman.h>` |
|      - | 6890 | `#include <sys/file.h>` |
|      - | 6891 | `#include <sys/wait.h>` |
|      - | 6892 | `#include <pwd.h>` |
|      - | 6893 | `#include <grp.h>` |
|      - | 6894 | `#include <dirent.h>` |
|      - | 6895 | `#include <utime.h>` |
|      - | 6896 | `#include <stdio.h>` |
|      - | 6897 | `#include <stdlib.h>` |
|      - | 6898 | `/* int (*xchdir)(const char *) */` |
|   8258 | 6899 | `static int UnixVfs_chdir(const char *zPath)` |
|      - | 6900 |  |
|      - | 6901 | `  int rc;` |
|   8258 | 6902 | `  rc = chdir(zPath);` |
|   8258 | 6903 | `  return rc == 0 ? PH7_OK : -1;` |
|      - | 6904 |  |
|      - | 6905 | `/* int (*xGetcwd)(ph7_context *) */` |
|     20 | 6906 | `static int UnixVfs_getcwd(ph7_context *pCtx)` |
|      - | 6907 |  |
|      - | 6908 | `	char zBuf[4096];` |
|      - | 6909 | `	char *zDir;` |
|      - | 6910 | `	/* Get the current directory */` |
|     20 | 6911 | `	zDir = getcwd(zBuf,sizeof(zBuf));` |
|     20 | 6912 | `	if( zDir == 0 ){` |
|    ! 0 | 6913 | `	  return -1;` |
|      - | 6914 | `    }` |
|     20 | 6915 | `	ph7_result_string(pCtx,zDir,-1/*Compute length automatically*/);` |
|     20 | 6916 | `	return PH7_OK;` |
|     10 | 6917 |  |
|      - | 6918 | `/* int (*xMkdir)(const char *,int,int) */` |
|      6 | 6919 | `static int UnixVfs_mkdir(const char *zPath,int mode,int recursive)` |
|      - | 6920 |  |
|      - | 6921 | `	int rc;` |
|      6 | 6922 | `        rc = mkdir(zPath,mode);` |
|      3 | 6923 | `	SXUNUSED(recursive); /* cc warning */` |
|      6 | 6924 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 6925 |  |
|      - | 6926 | `/* int (*xRmdir)(const char *) */` |
|      8 | 6927 | `static int UnixVfs_rmdir(const char *zPath)` |
|      - | 6928 |  |
|      - | 6929 | `	int rc;` |
|      8 | 6930 | `	rc = rmdir(zPath);` |
|      8 | 6931 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 6932 |  |
|      - | 6933 | `/* int (*xIsdir)(const char *) */` |
|   4932 | 6934 | `static int UnixVfs_isdir(const char *zPath)` |
|      - | 6935 |  |
|      - | 6936 | `	struct stat st;` |
|      - | 6937 | `	int rc;` |
|   4932 | 6938 | `	rc = stat(zPath,&st);` |
|   4932 | 6939 | `	if( rc != 0 ){` |
|      4 | 6940 | `	 return -1;` |
|      - | 6941 | `	}` |
|   4928 | 6942 | `	rc = S_ISDIR(st.st_mode);` |
|   4928 | 6943 | `	return rc ? PH7_OK : -1 ;` |
|   2466 | 6944 |  |
|      - | 6945 | `/* int (*xRename)(const char *,const char *) */` |
|      2 | 6946 | `static int UnixVfs_Rename(const char *zOld,const char *zNew)` |
|      - | 6947 |  |
|      - | 6948 | `	int rc;` |
|      2 | 6949 | `	rc = rename(zOld,zNew);` |
|      2 | 6950 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 6951 |  |
|      - | 6952 | `/* int (*xRealpath)(const char *,ph7_context *) */` |
|      4 | 6953 | `static int UnixVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|      - | 6954 |  |
|      - | 6955 | `#ifndef PH7_UNIX_OLD_LIBC` |
|      - | 6956 | `	char *zReal;` |
|      4 | 6957 | `	zReal = realpath(zPath,0);` |
|      4 | 6958 | `	if( zReal == 0 ){` |
|      2 | 6959 | `	  return -1;` |
|      - | 6960 | `	}` |
|      2 | 6961 | `	ph7_result_string(pCtx,zReal,-1/*Compute length automatically*/);` |
|      - | 6962 | `        /* Release the allocated buffer */` |
|      2 | 6963 | `	free(zReal);` |
|      2 | 6964 | `	return PH7_OK;` |
|      - | 6965 | `#else` |
|      - | 6966 | `    zPath = 0; /* cc warning */` |
|      - | 6967 | `    pCtx = 0;` |
|      - | 6968 | `    return -1;` |
|      - | 6969 | `#endif` |
|      2 | 6970 |  |
|      - | 6971 | `/* int (*xSleep)(unsigned int) */` |
|      4 | 6972 | `static int UnixVfs_Sleep(unsigned int uSec)` |
|      - | 6973 |  |
|      4 | 6974 | `	usleep(uSec);` |
|      4 | 6975 | `	return PH7_OK;` |
|      - | 6976 |  |
|      - | 6977 | `/* int (*xUnlink)(const char *) */` |
|  16952 | 6978 | `static int UnixVfs_unlink(const char *zPath)` |
|      - | 6979 |  |
|      - | 6980 | `	int rc;` |
|  16952 | 6981 | `	rc = unlink(zPath);` |
|  16952 | 6982 | `	return rc == 0 ? PH7_OK : -1 ;` |
|      - | 6983 |  |
|      - | 6984 | `/* int (*xFileExists)(const char *) */` |
|     44 | 6985 | `static int UnixVfs_FileExists(const char *zPath)` |
|      - | 6986 |  |
|      - | 6987 | `	int rc;` |
|     44 | 6988 | `	rc = access(zPath,F_OK);` |
|     44 | 6989 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 6990 |  |
|      - | 6991 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|     26 | 6992 | `static ph7_int64 UnixVfs_FileSize(const char *zPath)` |
|      - | 6993 |  |
|      - | 6994 | `	struct stat st;` |
|      - | 6995 | `	int rc;` |
|     26 | 6996 | `	rc = stat(zPath,&st);` |
|     26 | 6997 | `	if( rc != 0 ){` |
|    ! 0 | 6998 | `	 return -1;` |
|      - | 6999 | `	}` |
|     26 | 7000 | `	return (ph7_int64)st.st_size;` |
|     13 | 7001 |  |
|      - | 7002 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      4 | 7003 | `static int UnixVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|      - | 7004 |  |
|      - | 7005 | `	struct utimbuf ut;` |
|      - | 7006 | `	int rc;` |
|      4 | 7007 | `	ut.actime  = (time_t)access_time;` |
|      4 | 7008 | `	ut.modtime = (time_t)touch_time;` |
|      4 | 7009 | `	rc = utime(zPath,&ut);` |
|      4 | 7010 | `	if( rc != 0 ){` |
|    ! 0 | 7011 | `	 return -1;` |
|      - | 7012 | `	}` |
|      4 | 7013 | `	return PH7_OK;` |
|      2 | 7014 |  |
|      - | 7015 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|      2 | 7016 | `static ph7_int64 UnixVfs_FileAtime(const char *zPath)` |
|      - | 7017 |  |
|      - | 7018 | `	struct stat st;` |
|      - | 7019 | `	int rc;` |
|      2 | 7020 | `	rc = stat(zPath,&st);` |
|      2 | 7021 | `	if( rc != 0 ){` |
|    ! 0 | 7022 | `	 return -1;` |
|      - | 7023 | `	}` |
|      2 | 7024 | `	return (ph7_int64)st.st_atime;` |
|      1 | 7025 |  |
|      - | 7026 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|      4 | 7027 | `static ph7_int64 UnixVfs_FileMtime(const char *zPath)` |
|      - | 7028 |  |
|      - | 7029 | `	struct stat st;` |
|      - | 7030 | `	int rc;` |
|      4 | 7031 | `	rc = stat(zPath,&st);` |
|      4 | 7032 | `	if( rc != 0 ){` |
|    ! 0 | 7033 | `	 return -1;` |
|      - | 7034 | `	}` |
|      4 | 7035 | `	return (ph7_int64)st.st_mtime;` |
|      2 | 7036 |  |
|      - | 7037 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|      2 | 7038 | `static ph7_int64 UnixVfs_FileCtime(const char *zPath)` |
|      - | 7039 |  |
|      - | 7040 | `	struct stat st;` |
|      - | 7041 | `	int rc;` |
|      2 | 7042 | `	rc = stat(zPath,&st);` |
|      2 | 7043 | `	if( rc != 0 ){` |
|    ! 0 | 7044 | `	 return -1;` |
|      - | 7045 | `	}` |
|      2 | 7046 | `	return (ph7_int64)st.st_ctime;` |
|      1 | 7047 |  |
|      - | 7048 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      4 | 7049 | `static int UnixVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|      - | 7050 |  |
|      - | 7051 | `	struct stat st;` |
|      - | 7052 | `	int rc;` |
|      4 | 7053 | `	rc = stat(zPath,&st);` |
|      4 | 7054 | `	if( rc != 0 ){` |
|    ! 0 | 7055 | `	 return -1;` |
|      - | 7056 | `	}` |
|      - | 7057 | `	/* dev */` |
|      4 | 7058 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|      4 | 7059 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|      - | 7060 | `	/* ino */` |
|      4 | 7061 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|      4 | 7062 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|      - | 7063 | `	/* mode */` |
|      4 | 7064 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|      4 | 7065 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|      - | 7066 | `	/* nlink */` |
|      4 | 7067 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|      4 | 7068 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|      - | 7069 | `	/* uid,gid,rdev */` |
|      4 | 7070 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|      4 | 7071 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|      4 | 7072 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|      4 | 7073 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|      4 | 7074 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|      4 | 7075 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|      - | 7076 | `	/* size */` |
|      4 | 7077 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|      4 | 7078 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|      - | 7079 | `	/* atime */` |
|      4 | 7080 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|      4 | 7081 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|      - | 7082 | `	/* mtime */` |
|      4 | 7083 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|      4 | 7084 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|      - | 7085 | `	/* ctime */` |
|      4 | 7086 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|      4 | 7087 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|      - | 7088 | `	/* blksize,blocks */` |
|      4 | 7089 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|      4 | 7090 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|      4 | 7091 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|      4 | 7092 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|      4 | 7093 | `	return PH7_OK;` |
|      2 | 7094 |  |
|      - | 7095 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      2 | 7096 | `static int UnixVfs_lStat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|      - | 7097 |  |
|      - | 7098 | `	struct stat st;` |
|      - | 7099 | `	int rc;` |
|      2 | 7100 | `	rc = lstat(zPath,&st);` |
|      2 | 7101 | `	if( rc != 0 ){` |
|    ! 0 | 7102 | `	 return -1;` |
|      - | 7103 | `	}` |
|      - | 7104 | `	/* dev */` |
|      2 | 7105 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|      2 | 7106 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|      - | 7107 | `	/* ino */` |
|      2 | 7108 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|      2 | 7109 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|      - | 7110 | `	/* mode */` |
|      2 | 7111 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|      2 | 7112 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|      - | 7113 | `	/* nlink */` |
|      2 | 7114 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|      2 | 7115 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|      - | 7116 | `	/* uid,gid,rdev */` |
|      2 | 7117 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|      2 | 7118 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|      2 | 7119 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|      2 | 7120 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|      2 | 7121 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|      2 | 7122 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|      - | 7123 | `	/* size */` |
|      2 | 7124 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|      2 | 7125 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|      - | 7126 | `	/* atime */` |
|      2 | 7127 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|      2 | 7128 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|      - | 7129 | `	/* mtime */` |
|      2 | 7130 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|      2 | 7131 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|      - | 7132 | `	/* ctime */` |
|      2 | 7133 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|      2 | 7134 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|      - | 7135 | `	/* blksize,blocks */` |
|      2 | 7136 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|      2 | 7137 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|      2 | 7138 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|      2 | 7139 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|      2 | 7140 | `	return PH7_OK;` |
|      1 | 7141 |  |
|      - | 7142 | `/* int (*xChmod)(const char *,int) */` |
|     10 | 7143 | `static int UnixVfs_Chmod(const char *zPath,int mode)` |
|      - | 7144 |  |
|      - | 7145 | `    int rc;` |
|     10 | 7146 | `    rc = chmod(zPath,(mode_t)mode);` |
|     10 | 7147 | `    return rc == 0 ? PH7_OK : - 1;` |
|      - | 7148 |  |
|      - | 7149 | `/* int (*xChown)(const char *,const char *) */` |
|      4 | 7150 | `static int UnixVfs_Chown(const char *zPath,const char *zUser)` |
|      - | 7151 |  |
|      - | 7152 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|      - | 7153 | `  struct passwd *pwd;` |
|      - | 7154 | `  uid_t uid;` |
|      - | 7155 | `  int rc;` |
|      4 | 7156 | `  pwd = getpwnam(zUser);   /* Try getting UID for username */` |
|      4 | 7157 | `  if (pwd == 0) {` |
|      4 | 7158 | `    return -1;` |
|      - | 7159 | `  }` |
|    ! 0 | 7160 | `  uid = pwd->pw_uid;` |
|    ! 0 | 7161 | `  rc = chown(zPath,uid,-1);` |
|    ! 0 | 7162 | `  return rc == 0 ? PH7_OK : -1;` |
|      - | 7163 | `#else` |
|      - | 7164 | `	SXUNUSED(zPath);` |
|      - | 7165 | `	SXUNUSED(zUser);` |
|      - | 7166 | `	return -1;` |
|      - | 7167 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|      2 | 7168 |  |
|      - | 7169 | `/* int (*xChgrp)(const char *,const char *) */` |
|      4 | 7170 | `static int UnixVfs_Chgrp(const char *zPath,const char *zGroup)` |
|      - | 7171 |  |
|      - | 7172 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|      - | 7173 | `  struct group *group;` |
|      - | 7174 | `  gid_t gid;` |
|      - | 7175 | `  int rc;` |
|      4 | 7176 | `  group = getgrnam(zGroup);` |
|      4 | 7177 | `  if (group == 0) {` |
|      4 | 7178 | `    return -1;` |
|      - | 7179 | `  }` |
|    ! 0 | 7180 | `  gid = group->gr_gid;` |
|    ! 0 | 7181 | `  rc = chown(zPath,-1,gid);` |
|    ! 0 | 7182 | `  return rc == 0 ? PH7_OK : -1;` |
|      - | 7183 | `#else` |
|      - | 7184 | `	SXUNUSED(zPath);` |
|      - | 7185 | `	SXUNUSED(zGroup);` |
|      - | 7186 | `	return -1;` |
|      - | 7187 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|      2 | 7188 |  |
|      - | 7189 | `/* int (*xIsfile)(const char *) */` |
|   3364 | 7190 | `static int UnixVfs_isfile(const char *zPath)` |
|      - | 7191 |  |
|      - | 7192 | `	struct stat st;` |
|      - | 7193 | `	int rc;` |
|   3364 | 7194 | `	rc = stat(zPath,&st);` |
|   3364 | 7195 | `	if( rc != 0 ){` |
|      2 | 7196 | `	 return -1;` |
|      - | 7197 | `	}` |
|   3362 | 7198 | `	rc = S_ISREG(st.st_mode);` |
|   3362 | 7199 | `	return rc ? PH7_OK : -1 ;` |
|   1682 | 7200 |  |
|      - | 7201 | `/* int (*xIslink)(const char *) */` |
|      4 | 7202 | `static int UnixVfs_islink(const char *zPath)` |
|      - | 7203 |  |
|      - | 7204 | `	struct stat st;` |
|      - | 7205 | `	int rc;` |
|      4 | 7206 | `	rc = stat(zPath,&st);` |
|      4 | 7207 | `	if( rc != 0 ){` |
|    ! 0 | 7208 | `	 return -1;` |
|      - | 7209 | `	}` |
|      4 | 7210 | `	rc = S_ISLNK(st.st_mode);` |
|      4 | 7211 | `	return rc ? PH7_OK : -1 ;` |
|      2 | 7212 |  |
|      - | 7213 | `/* int (*xReadable)(const char *) */` |
|      2 | 7214 | `static int UnixVfs_isreadable(const char *zPath)` |
|      - | 7215 |  |
|      - | 7216 | `	int rc;` |
|      2 | 7217 | `	rc = access(zPath,R_OK);` |
|      2 | 7218 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 7219 |  |
|      - | 7220 | `/* int (*xWritable)(const char *) */` |
|      4 | 7221 | `static int UnixVfs_iswritable(const char *zPath)` |
|      - | 7222 |  |
|      - | 7223 | `	int rc;` |
|      4 | 7224 | `	rc = access(zPath,W_OK);` |
|      4 | 7225 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 7226 |  |
|      - | 7227 | `/* int (*xExecutable)(const char *) */` |
|      2 | 7228 | `static int UnixVfs_isexecutable(const char *zPath)` |
|      - | 7229 |  |
|      - | 7230 | `	int rc;` |
|      2 | 7231 | `	rc = access(zPath,X_OK);` |
|      2 | 7232 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 7233 |  |
|      - | 7234 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|      4 | 7235 | `static int UnixVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|      - | 7236 |  |
|      - | 7237 | `	struct stat st;` |
|      - | 7238 | `	int rc;` |
|      4 | 7239 | `    rc = stat(zPath,&st);` |
|      4 | 7240 | `	if( rc != 0 ){` |
|      - | 7241 | `	  /* Expand 'unknown' */` |
|    ! 0 | 7242 | `	  ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 7243 | `	  return -1;` |
|      - | 7244 | `	}` |
|      4 | 7245 | `	if(S_ISREG(st.st_mode) ){` |
|      2 | 7246 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|      3 | 7247 | `	}else if(S_ISDIR(st.st_mode)){` |
|      2 | 7248 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|      1 | 7249 | `	}else if(S_ISLNK(st.st_mode)){` |
|    ! 0 | 7250 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|    ! 0 | 7251 | `	}else if(S_ISBLK(st.st_mode)){` |
|    ! 0 | 7252 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|    ! 0 | 7253 | `    }else if(S_ISSOCK(st.st_mode)){` |
|    ! 0 | 7254 | `		ph7_result_string(pCtx,"socket",sizeof("socket")-1);` |
|    ! 0 | 7255 | `	}else if(S_ISFIFO(st.st_mode)){` |
|    ! 0 | 7256 | `       ph7_result_string(pCtx,"fifo",sizeof("fifo")-1);` |
|    ! 0 | 7257 | `	}else{` |
|    ! 0 | 7258 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|      - | 7259 | `	}` |
|      4 | 7260 | `	return PH7_OK;` |
|      2 | 7261 |  |
|      - | 7262 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|     16 | 7263 | `static int UnixVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|      - | 7264 |  |
|      - | 7265 | `	char *zEnv;` |
|     16 | 7266 | `	zEnv = getenv(zVar);` |
|     16 | 7267 | `	if( zEnv == 0 ){` |
|    ! 0 | 7268 | `	  return -1;` |
|      - | 7269 | `	}` |
|     16 | 7270 | `	ph7_result_string(pCtx,zEnv,-1/*Compute length automatically*/);` |
|     16 | 7271 | `	return PH7_OK;` |
|      8 | 7272 |  |
|      - | 7273 | `/* int (*xSetenv)(const char *,const char *) */` |
|      2 | 7274 | `static int UnixVfs_Setenv(const char *zName,const char *zValue)` |
|      - | 7275 |  |
|      - | 7276 | `   int rc;` |
|      2 | 7277 | `   rc = setenv(zName,zValue,1);` |
|      2 | 7278 | `   return rc == 0 ? PH7_OK : -1;` |
|      - | 7279 |  |
|      - | 7280 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|   1524 | 7281 | `static int UnixVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|      - | 7282 |  |
|      - | 7283 | `	struct stat st;` |
|      - | 7284 | `	void *pMap;` |
|      - | 7285 | `	int fd;` |
|      - | 7286 | `	int rc;` |
|      - | 7287 | `	/* Open the file in a read-only mode */` |
|   1524 | 7288 | `	fd = open(zPath,O_RDONLY);` |
|   1524 | 7289 | `	if( fd < 0 ){` |
|      2 | 7290 | `		return -1;` |
|      - | 7291 | `	}` |
|      - | 7292 | `	/* stat the handle */` |
|   1522 | 7293 | `	fstat(fd,&st);` |
|      - | 7294 | `	/* Obtain a memory view of the whole file */` |
|   1522 | 7295 | `	pMap = mmap(0,st.st_size,PROT_READ,MAP_PRIVATE\|MAP_FILE,fd,0);` |
|   1522 | 7296 | `	rc = PH7_OK;` |
|   1522 | 7297 | `	if( pMap == MAP_FAILED ){` |
|    ! 0 | 7298 | `		rc = -1;` |
|    ! 0 | 7299 | `	}else{` |
|      - | 7300 | `		/* Point to the memory view */` |
|   1522 | 7301 | `		*ppMap = pMap;` |
|   1522 | 7302 | `		*pSize = (ph7_int64)st.st_size;` |
|      - | 7303 | `	}` |
|   1522 | 7304 | `	close(fd);` |
|   1522 | 7305 | `	return rc;` |
|    762 | 7306 |  |
|      - | 7307 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|   1522 | 7308 | `static void UnixVfs_Unmap(void *pView,ph7_int64 nSize)` |
|      - | 7309 |  |
|   1522 | 7310 | `	munmap(pView,(size_t)nSize);` |
|   1522 | 7311 |  |
|      - | 7312 | `/* void (*xTempDir)(ph7_context *) */` |
|    166 | 7313 | `static void UnixVfs_TempDir(ph7_context *pCtx)` |
|      - | 7314 |  |
|      - | 7315 | `	static const char *azDirs[] = {` |
|      - | 7316 | `     "/var/tmp",` |
|      - | 7317 | `     "/usr/tmp",` |
|      - | 7318 | `	 "/usr/local/tmp"` |
|      - | 7319 | `  };` |
|      - | 7320 | `  unsigned int i;` |
|      - | 7321 | `  struct stat buf;` |
|      - | 7322 | `  const char *zDir;` |
|    166 | 7323 | `  zDir = getenv("TMPDIR");` |
|    166 | 7324 | `  if( zDir && zDir[0] != 0 && !access(zDir,07) ){` |
|     83 | 7325 | `	  ph7_result_string(pCtx,zDir,-1);` |
|     83 | 7326 | `	  return;` |
|      - | 7327 | `  }` |
|     83 | 7328 | `  for(i=0; i<sizeof(azDirs)/sizeof(azDirs[0]); i++){` |
|     83 | 7329 | `	zDir=azDirs[i];` |
|     83 | 7330 | `    if( zDir==0 ) continue;` |
|     83 | 7331 | `    if( stat(zDir, &buf) ) continue;` |
|     83 | 7332 | `    if( !S_ISDIR(buf.st_mode) ) continue;` |
|     83 | 7333 | `    if( access(zDir, 07) ) continue;` |
|      - | 7334 | `    /* Got one */` |
|     83 | 7335 | `	ph7_result_string(pCtx,zDir,-1);` |
|     83 | 7336 | `	return;` |
|      - | 7337 | `  }` |
|      - | 7338 | `  /* Default temp dir */` |
|    ! 0 | 7339 | `  ph7_result_string(pCtx,"/tmp",(int)sizeof("/tmp")-1);` |
|     83 | 7340 |  |
|      - | 7341 | `/* unsigned int (*xProcessId)(void) */` |
|      4 | 7342 | `static unsigned int UnixVfs_ProcessId(void)` |
|      - | 7343 |  |
|      4 | 7344 | `	return (unsigned int)getpid();` |
|      - | 7345 |  |
|      - | 7346 | `/* int (*xUid)(void) */` |
|      2 | 7347 | `static int UnixVfs_uid(void)` |
|      - | 7348 |  |
|      2 | 7349 | `	return (int)getuid();` |
|      - | 7350 |  |
|      - | 7351 | `/* int (*xGid)(void) */` |
|      2 | 7352 | `static int UnixVfs_gid(void)` |
|      - | 7353 |  |
|      2 | 7354 | `	return (int)getgid();` |
|      - | 7355 |  |
|      - | 7356 | `/* int (*xUmask)(int) */` |
|      8 | 7357 | `static int UnixVfs_Umask(int new_mask)` |
|      - | 7358 |  |
|      - | 7359 | `	int old_mask;` |
|      8 | 7360 | `	old_mask = umask(new_mask);` |
|      8 | 7361 | `	return old_mask;` |
|      - | 7362 |  |
|      - | 7363 | `/* void (*xUsername)(ph7_context *) */` |
|      2 | 7364 | `static void UnixVfs_Username(ph7_context *pCtx)` |
|      - | 7365 |  |
|      - | 7366 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|      - | 7367 | `  struct passwd *pwd;` |
|      - | 7368 | `  uid_t uid;` |
|      2 | 7369 | `  uid = getuid();` |
|      2 | 7370 | `  pwd = getpwuid(uid);   /* Try getting UID for username */` |
|      2 | 7371 | `  if (pwd == 0) {` |
|    ! 0 | 7372 | `    return;` |
|      - | 7373 | `  }` |
|      - | 7374 | `  /* Return the username */` |
|      2 | 7375 | `  ph7_result_string(pCtx,pwd->pw_name,-1);` |
|      - | 7376 | `#else` |
|      - | 7377 | `  ph7_result_string(pCtx,"Unknown",-1);` |
|      - | 7378 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|      2 | 7379 | `  return;` |
|      1 | 7380 |  |
|      - | 7381 | `/* int (*xLink)(const char *,const char *,int) */` |
|      8 | 7382 | `static int UnixVfs_link(const char *zSrc,const char *zTarget,int is_sym)` |
|      - | 7383 |  |
|      - | 7384 | `	int rc;` |
|      8 | 7385 | `	if( is_sym ){` |
|      - | 7386 | `		/* Symbolic link */` |
|      6 | 7387 | `		rc = symlink(zSrc,zTarget);` |
|      3 | 7388 | `	}else{` |
|      - | 7389 | `		/* Hard link */` |
|      2 | 7390 | `		rc = link(zSrc,zTarget);` |
|      - | 7391 | `	}` |
|      8 | 7392 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 7393 |  |
|      - | 7394 | `/* int (*xChroot)(const char *) */` |
|      2 | 7395 | `static int UnixVfs_chroot(const char *zRootDir)` |
|      - | 7396 |  |
|      - | 7397 | `	int rc;` |
|      2 | 7398 | `	rc = chroot(zRootDir);` |
|      2 | 7399 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 7400 |  |
|      - | 7401 | `/* Export the UNIX vfs */` |
|      - | 7402 | `static const ph7_vfs sUnixVfs = {` |
|      - | 7403 | `	"Unix_vfs",` |
|      - | 7404 | `	PH7_VFS_VERSION,` |
|      - | 7405 | `	UnixVfs_chdir,    /* int (*xChdir)(const char *) */` |
|      - | 7406 | `	UnixVfs_chroot,   /* int (*xChroot)(const char *); */` |
|      - | 7407 | `	UnixVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|      - | 7408 | `	UnixVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|      - | 7409 | `	UnixVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|      - | 7410 | `	UnixVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|      - | 7411 | `	UnixVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|      - | 7412 | `	UnixVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 7413 | `	UnixVfs_Sleep,    /* int (*xSleep)(unsigned int) */` |
|      - | 7414 | `	UnixVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|      - | 7415 | `	UnixVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|      - | 7416 | `	UnixVfs_Chmod, /*int (*xChmod)(const char *,int)*/` |
|      - | 7417 | `	UnixVfs_Chown, /*int (*xChown)(const char *,const char *)*/` |
|      - | 7418 | `	UnixVfs_Chgrp, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 7419 |  |
|      - | 7420 |  |
|      - | 7421 | `	UnixVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 7422 | `	UnixVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 7423 | `	UnixVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 7424 | `	UnixVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 7425 | `	UnixVfs_Stat,  /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 7426 | `	UnixVfs_lStat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 7427 | `	UnixVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|      - | 7428 | `	UnixVfs_islink,     /* int (*xIslink)(const char *) */` |
|      - | 7429 | `	UnixVfs_isreadable, /* int (*xReadable)(const char *) */` |
|      - | 7430 | `	UnixVfs_iswritable, /* int (*xWritable)(const char *) */` |
|      - | 7431 | `	UnixVfs_isexecutable,/* int (*xExecutable)(const char *) */` |
|      - | 7432 | `	UnixVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 7433 | `	UnixVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 7434 | `	UnixVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|      - | 7435 | `	UnixVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 7436 | `	UnixVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 7437 | `	UnixVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 7438 | `	UnixVfs_link,       /* int (*xLink)(const char *,const char *,int) */` |
|      - | 7439 | `	UnixVfs_Umask,      /* int (*xUmask)(int) */` |
|      - | 7440 | `	UnixVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|      - | 7441 | `	UnixVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|      - | 7442 | `	UnixVfs_uid, /* int (*xUid)(void) */` |
|      - | 7443 | `	UnixVfs_gid, /* int (*xGid)(void) */` |
|      - | 7444 | `	UnixVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|      - | 7445 |  |
|      - | 7446 | `};` |
|      - | 7447 | `/* UNIX File IO */` |
|      - | 7448 | `#define PH7_UNIX_OPEN_MODE	0640 /* Default open mode */` |
|      - | 7449 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|  18710 | 7450 | `static int UnixFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|      - | 7451 |  |
|  18710 | 7452 | `	int iOpen = O_RDONLY;` |
|      - | 7453 | `	int fd;` |
|      - | 7454 | `	/* Set the desired flags according to the open mode */` |
|  18710 | 7455 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|      - | 7456 | `		/* Open existing file, or create if it doesn't exist */` |
|   8476 | 7457 | `		iOpen = O_CREAT;` |
|   8476 | 7458 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|      - | 7459 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
|   8476 | 7460 | `			iOpen \|= O_TRUNC;` |
|   4238 | 7461 | `			SXUNUSED(pResource); /* cc warning */` |
|   4238 | 7462 | `		}` |
|  14472 | 7463 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|      - | 7464 | `		/* Creates a new file, only if it does not already exist.` |
|      - | 7465 | `		* If the file exists, it fails.` |
|      - | 7466 | `		*/` |
|    ! 0 | 7467 | `		iOpen = O_CREAT\|O_EXCL;` |
|  10234 | 7468 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|      - | 7469 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|      - | 7470 | `		 * The file must exist.` |
|      - | 7471 | `		 */` |
|    ! 0 | 7472 | `		iOpen = O_RDWR\|O_TRUNC;` |
|    ! 0 | 7473 | `	}` |
|  18710 | 7474 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|      - | 7475 | `		/* Read+Write access */` |
|   8460 | 7476 | `		iOpen &= ~O_RDONLY;` |
|   8460 | 7477 | `		iOpen \|= O_RDWR;` |
|  14480 | 7478 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|      - | 7479 | `		/* Write only access */` |
|     22 | 7480 | `		iOpen &= ~O_RDONLY;` |
|     22 | 7481 | `		iOpen \|= O_WRONLY;` |
|     11 | 7482 | `	}` |
|  18710 | 7483 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|      - | 7484 | `		/* Append mode */` |
|    ! 0 | 7485 | `		iOpen \|= O_APPEND;` |
|    ! 0 | 7486 | `	}` |
|      - | 7487 | `#ifdef O_TEMP` |
|      - | 7488 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|      - | 7489 | `		/* File is temporary */` |
|      - | 7490 | `		iOpen \|= O_TEMP;` |
|      - | 7491 | `	}` |
|      - | 7492 | `#endif` |
|      - | 7493 | `	/* Open the file now */` |
|  18710 | 7494 | `	fd = open(zPath,iOpen,PH7_UNIX_OPEN_MODE);` |
|  18710 | 7495 | `	if( fd < 0 ){` |
|      - | 7496 | `		/* IO error */` |
|      8 | 7497 | `		return -1;` |
|      - | 7498 | `	}` |
|      - | 7499 | `	/* Save the handle */` |
|  18702 | 7500 | `	*ppHandle = SX_INT_TO_PTR(fd);` |
|  18702 | 7501 | `	return PH7_OK;` |
|   9355 | 7502 |  |
|      - | 7503 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|    786 | 7504 | `static int UnixDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|      - | 7505 |  |
|      - | 7506 | `	DIR *pDir;` |
|      - | 7507 | `	/* Open the target directory */` |
|    786 | 7508 | `	pDir = opendir(zPath);` |
|    786 | 7509 | `	if( pDir == 0 ){` |
|    ! 0 | 7510 | `		SXUNUSED(pResource); /* Compiler warning */` |
|    ! 0 | 7511 | `		return -1;` |
|      - | 7512 | `	}` |
|      - | 7513 | `	/* Save our structure */` |
|    786 | 7514 | `	*ppHandle = pDir;` |
|    786 | 7515 | `	return PH7_OK;` |
|    393 | 7516 |  |
|      - | 7517 | `/* void (*xCloseDir)(void *) */` |
|    786 | 7518 | `static void UnixDir_Close(void *pUserData)` |
|      - | 7519 |  |
|    786 | 7520 | `	closedir((DIR *)pUserData);` |
|    786 | 7521 |  |
|      - | 7522 | `/* void (*xClose)(void *); */` |
|  18702 | 7523 | `static void UnixFile_Close(void *pUserData)` |
|      - | 7524 |  |
|  18702 | 7525 | `	close(SX_PTR_TO_INT(pUserData));` |
|  18702 | 7526 |  |
|      - | 7527 | `/* int (*xReadDir)(void *,ph7_context *) */` |
|   4930 | 7528 | `static int UnixDir_Read(void *pUserData,ph7_context *pCtx)` |
|      - | 7529 |  |
|   4930 | 7530 | `	DIR *pDir = (DIR *)pUserData;` |
|      - | 7531 | `	struct dirent *pEntry;` |
|   4930 | 7532 | `	char *zName = 0; /* cc warning */` |
|   4930 | 7533 | `	sxu32 n = 0;` |
|   3253 | 7534 | `	for(;;){` |
|   6506 | 7535 | `		pEntry = readdir(pDir);` |
|   6506 | 7536 | `		if( pEntry == 0 ){` |
|      - | 7537 | `			/* No more entries to process */` |
|    784 | 7538 | `			return -1;` |
|      - | 7539 | `		}` |
|   5722 | 7540 | `		zName = pEntry->d_name;` |
|   5722 | 7541 | `		n = SyStrlen(zName);` |
|      - | 7542 | `		/* Ignore '.' && '..' */` |
|   5722 | 7543 | `		if( n > sizeof("..")-1 \|\| zName[0] != '.' \|\| ( n == sizeof("..")-1 && zName[1] != '.') ){` |
|   2073 | 7544 | `			break;` |
|      - | 7545 | `		}` |
|      - | 7546 | `		/* Next entry */` |
|      - | 7547 | `	}` |
|      - | 7548 | `	/* Return the current file name */` |
|   4146 | 7549 | `	ph7_result_string(pCtx,zName,(int)n);` |
|   4146 | 7550 | `	return PH7_OK;` |
|   2465 | 7551 |  |
|      - | 7552 | `/* void (*xRewindDir)(void *) */` |
|      2 | 7553 | `static void UnixDir_Rewind(void *pUserData)` |
|      - | 7554 |  |
|      2 | 7555 | `	rewinddir((DIR *)pUserData);` |
|      2 | 7556 |  |
|      - | 7557 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
|  20406 | 7558 | `static ph7_int64 UnixFile_Read(void *pUserData,void *pBuffer,ph7_int64 nDatatoRead)` |
|      - | 7559 |  |
|      - | 7560 | `	ssize_t nRd;` |
|  20406 | 7561 | `	nRd = read(SX_PTR_TO_INT(pUserData),pBuffer,(size_t)nDatatoRead);` |
|  20406 | 7562 | `	if( nRd < 1 ){` |
|      - | 7563 | `		/* EOF or IO error */` |
|  10198 | 7564 | `		return -1;` |
|      - | 7565 | `	}` |
|  10208 | 7566 | `	return (ph7_int64)nRd;` |
|  10203 | 7567 |  |
|      - | 7568 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
|   8498 | 7569 | `static ph7_int64 UnixFile_Write(void *pUserData,const void *pBuffer,ph7_int64 nWrite)` |
|      - | 7570 |  |
|   8498 | 7571 | `	const char *zData = (const char *)pBuffer;` |
|   8498 | 7572 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|      - | 7573 | `	ph7_int64 nCount;` |
|      - | 7574 | `	ssize_t nWr;` |
|   8498 | 7575 | `	nCount = 0;` |
|   8498 | 7576 | `	for(;;){` |
|  16996 | 7577 | `		if( nWrite < 1 ){` |
|   8498 | 7578 | `			break;` |
|      - | 7579 | `		}` |
|   8498 | 7580 | `		nWr = write(fd,zData,(size_t)nWrite);` |
|   8498 | 7581 | `		if( nWr < 1 ){` |
|      - | 7582 | `			/* IO error */` |
|    ! 0 | 7583 | `			break;` |
|      - | 7584 | `		}` |
|   8498 | 7585 | `		nWrite -= nWr;` |
|   8498 | 7586 | `		nCount += nWr;` |
|   8498 | 7587 | `		zData += nWr;` |
|      - | 7588 | `	}` |
|   8498 | 7589 | `	if( nWrite > 0 ){` |
|    ! 0 | 7590 | `		return -1;` |
|      - | 7591 | `	}` |
|   8498 | 7592 | `	return nCount;` |
|   4249 | 7593 |  |
|      - | 7594 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|      6 | 7595 | `static int UnixFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|      - | 7596 |  |
|      - | 7597 | `	off_t iNew;` |
|      6 | 7598 | `	switch(whence){` |
|    ! 0 | 7599 | `	case 1:/*SEEK_CUR*/` |
|    ! 0 | 7600 | `		whence = SEEK_CUR;` |
|    ! 0 | 7601 | `		break;` |
|    ! 0 | 7602 | `	case 2: /* SEEK_END */` |
|    ! 0 | 7603 | `		whence = SEEK_END;` |
|    ! 0 | 7604 | `		break;` |
|      6 | 7605 | `	case 0: /* SEEK_SET */` |
|      - | 7606 | `	default:` |
|      6 | 7607 | `		whence = SEEK_SET;` |
|      6 | 7608 | `		break;` |
|      - | 7609 | `	}` |
|      6 | 7610 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),(off_t)iOfft,whence);` |
|      6 | 7611 | `	if( iNew < 0 ){` |
|    ! 0 | 7612 | `		return -1;` |
|      - | 7613 | `	}` |
|      6 | 7614 | `	return PH7_OK;` |
|      3 | 7615 |  |
|      - | 7616 | `/* int (*xLock)(void *,int) */` |
|      4 | 7617 | `static int UnixFile_Lock(void *pUserData,int lock_type)` |
|      - | 7618 |  |
|      4 | 7619 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|      4 | 7620 | `	int rc = PH7_OK; /* cc warning */` |
|      4 | 7621 | `	if( lock_type < 0 ){` |
|      - | 7622 | `		/* Unlock the file */` |
|    ! 0 | 7623 | `		rc = flock(fd,LOCK_UN);` |
|    ! 0 | 7624 | `	}else{` |
|      4 | 7625 | `		if( lock_type == 1 ){` |
|      - | 7626 | `			/* Exculsive lock */` |
|      2 | 7627 | `			rc = flock(fd,LOCK_EX);` |
|      1 | 7628 | `		}else{` |
|      - | 7629 | `			/* Shared lock */` |
|      2 | 7630 | `			rc = flock(fd,LOCK_SH);` |
|      - | 7631 | `		}` |
|      - | 7632 | `	}` |
|      4 | 7633 | `	return !rc ? PH7_OK : -1;` |
|      - | 7634 |  |
|      - | 7635 | `/* ph7_int64 (*xTell)(void *) */` |
|      6 | 7636 | `static ph7_int64 UnixFile_Tell(void *pUserData)` |
|      - | 7637 |  |
|      - | 7638 | `	off_t iNew;` |
|      6 | 7639 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),0,SEEK_CUR);` |
|      6 | 7640 | `	return (ph7_int64)iNew;` |
|      - | 7641 |  |
|      - | 7642 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|      6 | 7643 | `static int UnixFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|      - | 7644 |  |
|      - | 7645 | `	int rc;` |
|      6 | 7646 | `	rc = ftruncate(SX_PTR_TO_INT(pUserData),(off_t)nOfft);` |
|      6 | 7647 | `	if( rc != 0 ){` |
|    ! 0 | 7648 | `		return -1;` |
|      - | 7649 | `	}` |
|      6 | 7650 | `	return PH7_OK;` |
|      3 | 7651 |  |
|      - | 7652 | `/* int (*xSync)(void *); */` |
|      2 | 7653 | `static int UnixFile_Sync(void *pUserData)` |
|      - | 7654 |  |
|      - | 7655 | `	int rc;` |
|      2 | 7656 | `	rc = fsync(SX_PTR_TO_INT(pUserData));` |
|      2 | 7657 | `	return rc == 0 ? PH7_OK : - 1;` |
|      - | 7658 |  |
|      - | 7659 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|      2 | 7660 | `static int UnixFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|      - | 7661 |  |
|      - | 7662 | `	struct stat st;` |
|      - | 7663 | `	int rc;` |
|      2 | 7664 | `	rc = fstat(SX_PTR_TO_INT(pUserData),&st);` |
|      2 | 7665 | `	if( rc != 0 ){` |
|    ! 0 | 7666 | `	 return -1;` |
|      - | 7667 | `	}` |
|      - | 7668 | `	/* dev */` |
|      2 | 7669 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|      2 | 7670 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|      - | 7671 | `	/* ino */` |
|      2 | 7672 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|      2 | 7673 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|      - | 7674 | `	/* mode */` |
|      2 | 7675 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|      2 | 7676 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|      - | 7677 | `	/* nlink */` |
|      2 | 7678 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|      2 | 7679 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|      - | 7680 | `	/* uid,gid,rdev */` |
|      2 | 7681 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|      2 | 7682 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|      2 | 7683 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|      2 | 7684 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|      2 | 7685 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|      2 | 7686 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|      - | 7687 | `	/* size */` |
|      2 | 7688 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|      2 | 7689 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|      - | 7690 | `	/* atime */` |
|      2 | 7691 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|      2 | 7692 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|      - | 7693 | `	/* mtime */` |
|      2 | 7694 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|      2 | 7695 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|      - | 7696 | `	/* ctime */` |
|      2 | 7697 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|      2 | 7698 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|      - | 7699 | `	/* blksize,blocks */` |
|      2 | 7700 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|      2 | 7701 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|      2 | 7702 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|      2 | 7703 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|      2 | 7704 | `	return PH7_OK;` |
|      1 | 7705 |  |
|      - | 7706 | `/* Export the file:// stream */` |
|      - | 7707 | `static const ph7_io_stream sUnixFileStream = {` |
|      - | 7708 | `	"file", /* Stream name */` |
|      - | 7709 | `	PH7_IO_STREAM_VERSION,` |
|      - | 7710 | `	UnixFile_Open,  /* xOpen */` |
|      - | 7711 | `	UnixDir_Open,   /* xOpenDir */` |
|      - | 7712 | `	UnixFile_Close, /* xClose */` |
|      - | 7713 | `	UnixDir_Close,  /* xCloseDir */` |
|      - | 7714 | `	UnixFile_Read,  /* xRead */` |
|      - | 7715 | `	UnixDir_Read,   /* xReadDir */` |
|      - | 7716 | `	UnixFile_Write, /* xWrite */` |
|      - | 7717 | `	UnixFile_Seek,  /* xSeek */` |
|      - | 7718 | `	UnixFile_Lock,  /* xLock */` |
|      - | 7719 | `	UnixDir_Rewind, /* xRewindDir */` |
|      - | 7720 | `	UnixFile_Tell,  /* xTell */` |
|      - | 7721 | `	UnixFile_Trunc, /* xTrunc */` |
|      - | 7722 | `	UnixFile_Sync,  /* xSeek */` |
|      - | 7723 | `	UnixFile_Stat   /* xStat */` |
|      - | 7724 | `};` |
|      - | 7725 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 7726 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7727 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 7728 | `/*` |
|      - | 7729 | ` * Export the builtin vfs.` |
|      - | 7730 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 7731 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 7732 | ` * Note:` |
|      - | 7733 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 7734 | ` * Note:` |
|      - | 7735 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7736 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 7737 | ` */` |
|   1532 | 7738 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      2 | 7739 |  |
|      - | 7740 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 7741 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 7742 | `	return &null_vfs;` |
|      - | 7743 | `#else` |
|      - | 7744 | `#ifdef __WINNT__` |
|      2 | 7745 | `	return &sWinVfs;` |
|      - | 7746 | `#elif defined(__UNIXES__)` |
|   1532 | 7747 | `	return &sUnixVfs;` |
|      - | 7748 | `#else` |
|      - | 7749 | `	return &null_vfs;` |
|      - | 7750 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 7751 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 7752 | `#else` |
|      - | 7753 | `	return &null_vfs;` |
|      - | 7754 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      2 | 7755 |  |
|      - | 7756 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 7757 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7758 | `/*` |
|      - | 7759 | ` * The following defines are mostly used by the UNIX built and have` |
|      - | 7760 | ` * no particular meaning on windows.` |
|      - | 7761 | ` */` |
|      - | 7762 | `#ifndef STDIN_FILENO` |
|      - | 7763 | `#define STDIN_FILENO	0` |
|      - | 7764 | `#endif` |
|      - | 7765 | `#ifndef STDOUT_FILENO` |
|      - | 7766 | `#define STDOUT_FILENO	1` |
|      - | 7767 | `#endif` |
|      - | 7768 | `#ifndef STDERR_FILENO` |
|      - | 7769 | `#define STDERR_FILENO	2` |
|      - | 7770 | `#endif` |
|      - | 7771 | `/*` |
|      - | 7772 | ` * php:// Accessing various I/O streams` |
|      - | 7773 | ` * According to the PHP langage reference manual` |
|      - | 7774 | ` * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input` |
|      - | 7775 | ` * and output streams, the standard input, output and error file descriptors.` |
|      - | 7776 | ` * php://stdin, php://stdout and php://stderr:` |
|      - | 7777 | ` *  Allow direct access to the corresponding input or output stream of the PHP process.` |
|      - | 7778 | ` *  The stream references a duplicate file descriptor, so if you open php://stdin and later` |
|      - | 7779 | ` *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.` |
|      - | 7780 | ` *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.` |
|      - | 7781 | ` * php://output` |
|      - | 7782 | ` *  php://output is a write-only stream that allows you to write to the output buffer` |
|      - | 7783 | ` *  mechanism in the same way as print and echo.` |
|      - | 7784 | ` */` |
|      - | 7785 | `typedef struct ph7_stream_data ph7_stream_data;` |
|      - | 7786 | `/* Supported IO streams */` |
|      - | 7787 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|      - | 7788 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|      - | 7789 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|      - | 7790 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|      - | 7791 | ` /* The following structure is the private data associated with the php:// stream */` |
|      - | 7792 | `struct ph7_stream_data` |
|      - | 7793 |  |
|      - | 7794 | `	ph7_vm *pVm; /* VM that own this instance */` |
|      - | 7795 | `	int iType;   /* Stream type */` |
|      - | 7796 | `	union{` |
|      - | 7797 | `		void *pHandle; /* Stream handle */` |
|      - | 7798 | `		ph7_output_consumer sConsumer; /* VM output consumer */` |
|      - | 7799 | `	}x;` |
|      - | 7800 | `};` |
|      - | 7801 | `/*` |
|      - | 7802 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|      - | 7803 | ` */` |
|      8 | 7804 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|      1 | 7805 |  |
|      - | 7806 | `	ph7_stream_data *pData;` |
|      9 | 7807 | `	if( pVm == 0 ){` |
|    ! 0 | 7808 | `		return 0;` |
|      - | 7809 | `	}` |
|      - | 7810 | `	/* Allocate a new instance */` |
|      9 | 7811 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|      9 | 7812 | `	if( pData == 0 ){` |
|    ! 0 | 7813 | `		return 0;` |
|      - | 7814 | `	}` |
|      - | 7815 | `	/* Zero the structure */` |
|      9 | 7816 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|      - | 7817 | `	/* Initialize fields */` |
|      9 | 7818 | `	pData->iType = iType;` |
|      9 | 7819 | `	if( iType == PH7_IO_STREAM_OUTPUT ){` |
|      - | 7820 | `		/* Point to the default VM consumer routine. */` |
|      3 | 7821 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|      2 | 7822 | `	}else{` |
|      - | 7823 | `#ifdef __WINNT__` |
|      - | 7824 | `		DWORD nChannel;` |
|      1 | 7825 | `		switch(iType){` |
|      1 | 7826 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|      1 | 7827 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|      - | 7828 | `		default:` |
|      1 | 7829 | `			nChannel = STD_INPUT_HANDLE;` |
|      - | 7830 | `			break;` |
|      - | 7831 | `		}` |
|      1 | 7832 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|      - | 7833 | `#else` |
|      - | 7834 | `		/* Assume an UNIX system */` |
|      6 | 7835 | `		int ifd = STDIN_FILENO;` |
|      6 | 7836 | `		switch(iType){` |
|      2 | 7837 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|      2 | 7838 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|      1 | 7839 | `		default:` |
|      2 | 7840 | `			break;` |
|      - | 7841 | `		}` |
|      6 | 7842 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|      - | 7843 | `#endif` |
|      - | 7844 | `	}` |
|      9 | 7845 | `	pData->pVm = pVm;` |
|      9 | 7846 | `	return pData;` |
|      5 | 7847 |  |
|      - | 7848 | `/*` |
|      - | 7849 | ` * Implementation of the php:// IO streams routines` |
|      - | 7850 | ` * Status:` |
|      - | 7851 | ` *   Stable.` |
|      - | 7852 | ` */` |
|      - | 7853 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|      2 | 7854 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 7855 |  |
|      - | 7856 | `	ph7_stream_data *pData;` |
|      - | 7857 | `	SyString sStream;` |
|      3 | 7858 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|      - | 7859 | `	/* Trim leading and trailing white spaces */` |
|      3 | 7860 | `	SyStringFullTrim(&sStream);` |
|      - | 7861 | `	/* Stream to open */` |
|      3 | 7862 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|    ! 0 | 7863 | `		iMode = PH7_IO_STREAM_STDIN;` |
|      3 | 7864 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|      3 | 7865 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|      1 | 7866 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|    ! 0 | 7867 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|    ! 0 | 7868 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|    ! 0 | 7869 | `		iMode = PH7_IO_STREAM_STDERR;` |
|    ! 0 | 7870 | `	}else{` |
|      - | 7871 | `		/* unknown stream name */` |
|    ! 0 | 7872 | `		return -1;` |
|      - | 7873 | `	}` |
|      - | 7874 | `	/* Create our handle */` |
|      3 | 7875 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|      3 | 7876 | `	if( pData == 0 ){` |
|    ! 0 | 7877 | `		return -1;` |
|      - | 7878 | `	}` |
|      - | 7879 | `	/* Make the handle public */` |
|      3 | 7880 | `	*ppHandle = (void *)pData;` |
|      3 | 7881 | `	return PH7_OK;` |
|      2 | 7882 |  |
|      - | 7883 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|    ! 0 | 7884 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|    ! 0 | 7885 |  |
|    ! 0 | 7886 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    ! 0 | 7887 | `	if( pData == 0 ){` |
|    ! 0 | 7888 | `		return -1;` |
|      - | 7889 | `	}` |
|    ! 0 | 7890 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|      - | 7891 | `		/* Forbidden */` |
|    ! 0 | 7892 | `		return -1;` |
|      - | 7893 | `	}` |
|      - | 7894 | `#ifdef __WINNT__` |
|      - | 7895 | `	{` |
|      - | 7896 | `		DWORD nRd;` |
|      - | 7897 | `		BOOL rc;` |
|    ! 0 | 7898 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    ! 0 | 7899 | `		if( !rc ){` |
|      - | 7900 | `			/* IO error */` |
|    ! 0 | 7901 | `			return -1;` |
|      - | 7902 | `		}` |
|    ! 0 | 7903 | `		return (ph7_int64)nRd;` |
|      - | 7904 | `	}` |
|      - | 7905 | `#elif defined(__UNIXES__)` |
|      - | 7906 | `	{` |
|      - | 7907 | `		ssize_t nRd;` |
|      - | 7908 | `		int fd;` |
|    ! 0 | 7909 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 7910 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|    ! 0 | 7911 | `		if( nRd < 1 ){` |
|    ! 0 | 7912 | `			return -1;` |
|      - | 7913 | `		}` |
|    ! 0 | 7914 | `		return (ph7_int64)nRd;` |
|      - | 7915 | `	}` |
|      - | 7916 | `#else` |
|      - | 7917 | `	return -1;` |
|      - | 7918 | `#endif` |
|    ! 0 | 7919 |  |
|      - | 7920 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|      2 | 7921 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 7922 |  |
|      3 | 7923 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      3 | 7924 | `	if( pData == 0 ){` |
|    ! 0 | 7925 | `		return -1;` |
|      - | 7926 | `	}` |
|      3 | 7927 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|      - | 7928 | `		/* Forbidden */` |
|    ! 0 | 7929 | `		return -1;` |
|      3 | 7930 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|      3 | 7931 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|      - | 7932 | `		int rc;` |
|      - | 7933 | `		/* Call the vm output consumer */` |
|      3 | 7934 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|      3 | 7935 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 7936 | `			return -1;` |
|      - | 7937 | `		}` |
|      3 | 7938 | `		return nWrite;` |
|      - | 7939 | `	}` |
|      - | 7940 | `#ifdef __WINNT__` |
|      - | 7941 | `	{` |
|      - | 7942 | `		DWORD nWr;` |
|      - | 7943 | `		BOOL rc;` |
|    ! 0 | 7944 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|    ! 0 | 7945 | `		if( !rc ){` |
|      - | 7946 | `			/* IO error */` |
|    ! 0 | 7947 | `			return -1;` |
|      - | 7948 | `		}` |
|    ! 0 | 7949 | `		return (ph7_int64)nWr;` |
|      - | 7950 | `	}` |
|      - | 7951 | `#elif defined(__UNIXES__)` |
|      - | 7952 | `	{` |
|      - | 7953 | `		ssize_t nWr;` |
|      - | 7954 | `		int fd;` |
|    ! 0 | 7955 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 7956 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|    ! 0 | 7957 | `		if( nWr < 1 ){` |
|    ! 0 | 7958 | `			return -1;` |
|      - | 7959 | `		}` |
|    ! 0 | 7960 | `		return (ph7_int64)nWr;` |
|      - | 7961 | `	}` |
|      - | 7962 | `#else` |
|      - | 7963 | `	return -1;` |
|      - | 7964 | `#endif` |
|      2 | 7965 |  |
|      - | 7966 | `/* void (*xClose)(void *) */` |
|      2 | 7967 | `static void PHPStreamData_Close(void *pHandle)` |
|      1 | 7968 |  |
|      3 | 7969 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 7970 | `	ph7_vm *pVm;` |
|      3 | 7971 | `	if( pData == 0 ){` |
|    ! 0 | 7972 | `		return;` |
|      - | 7973 | `	}` |
|      3 | 7974 | `	pVm = pData->pVm;` |
|      - | 7975 | `	/* Free the instance */` |
|      3 | 7976 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|      2 | 7977 |  |
|      - | 7978 | `/*` |
|      - | 7979 | ` * Pipe stream implementation for popen/pclose.` |
|      - | 7980 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|      - | 7981 | ` * PHP-compatible process I/O functionality.` |
|      - | 7982 | ` */` |
|      - | 7983 | `typedef struct pipe_private pipe_private;` |
|      - | 7984 | `struct pipe_private` |
|      - | 7985 |  |
|      - | 7986 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|      - | 7987 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|      - | 7988 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|      - | 7989 | `#ifdef __WINNT__` |
|      - | 7990 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|      - | 7991 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|      - | 7992 | `#endif` |
|      - | 7993 | `};` |
|      - | 7994 |  |
|      - | 7995 | `#ifdef __WINNT__` |
|      - | 7996 | `/*` |
|      - | 7997 | ` * Custom Windows popen implementation using CreateProcess.` |
|      - | 7998 | ` * This allows us to properly wait for process completion.` |
|      - | 7999 | ` */` |
|      - | 8000 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|      2 | 8001 |  |
|      2 | 8002 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|      2 | 8003 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|      2 | 8004 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|      - | 8005 | `	SECURITY_ATTRIBUTES sa;` |
|      - | 8006 | `	STARTUPINFOW si;` |
|      - | 8007 | `	PROCESS_INFORMATION pi;` |
|      2 | 8008 | `	WCHAR *zWideCmd = NULL;` |
|      2 | 8009 | `	FILE *pFile = NULL;` |
|      - | 8010 | `	int fd;` |
|      2 | 8011 | `	BOOL bRead = (zMode[0] == 'r');` |
|      - | 8012 |  |
|      - | 8013 | `	/* Set up security attributes for pipe inheritance */` |
|      2 | 8014 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|      2 | 8015 | `	sa.bInheritHandle = TRUE;` |
|      2 | 8016 | `	sa.lpSecurityDescriptor = NULL;` |
|      - | 8017 |  |
|      - | 8018 | `	/* Create pipes for child process I/O */` |
|      2 | 8019 | `	if( bRead ){` |
|      - | 8020 | `		/* Reading from child's stdout */` |
|      2 | 8021 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|    ! 0 | 8022 | `			return NULL;` |
|      - | 8023 | `		}` |
|      - | 8024 | `		/* Ensure read handle is not inherited */` |
|      2 | 8025 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|      2 | 8026 | `		hReadPipe = hChildStdoutRd;` |
|      2 | 8027 | `		*phPipe = hChildStdoutRd;` |
|      2 | 8028 | `	}else{` |
|      - | 8029 | `		/* Writing to child's stdin */` |
|    ! 0 | 8030 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|    ! 0 | 8031 | `			return NULL;` |
|      - | 8032 | `		}` |
|      - | 8033 | `		/* Ensure write handle is not inherited */` |
|    ! 0 | 8034 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|    ! 0 | 8035 | `		hWritePipe = hChildStdinWr;` |
|    ! 0 | 8036 | `		*phPipe = hChildStdinWr;` |
|      - | 8037 | `	}` |
|      - | 8038 |  |
|      - | 8039 | `	/* Convert command to wide string */` |
|      - | 8040 | `	{` |
|      2 | 8041 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|      2 | 8042 | `		if( nLen <= 0 ){` |
|    ! 0 | 8043 | `			goto cleanup_pipes;` |
|      - | 8044 | `		}` |
|      2 | 8045 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|      2 | 8046 | `		if( !zWideCmd ){` |
|    ! 0 | 8047 | `			goto cleanup_pipes;` |
|      - | 8048 | `		}` |
|      2 | 8049 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|      - | 8050 | `	}` |
|      - | 8051 |  |
|      - | 8052 | `	/* Set up process startup info */` |
|      2 | 8053 | `	ZeroMemory(&si, sizeof(si));` |
|      2 | 8054 | `	si.cb = sizeof(si);` |
|      2 | 8055 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|      2 | 8056 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|      2 | 8057 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|      2 | 8058 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|      2 | 8059 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|      - | 8060 |  |
|      2 | 8061 | `	ZeroMemory(&pi, sizeof(pi));` |
|      - | 8062 |  |
|      - | 8063 | `	/* Create the child process */` |
|      2 | 8064 | `	if( !CreateProcessW(` |
|      - | 8065 | `		NULL,           /* Application name */` |
|      - | 8066 | `		zWideCmd,       /* Command line */` |
|      - | 8067 | `		NULL,           /* Process security attributes */` |
|      - | 8068 | `		NULL,           /* Thread security attributes */` |
|      - | 8069 | `		TRUE,           /* Inherit handles */` |
|      - | 8070 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|      - | 8071 | `		NULL,           /* Environment */` |
|      - | 8072 | `		NULL,           /* Current directory */` |
|      - | 8073 | `		&si,            /* Startup info */` |
|      - | 8074 | `		&pi             /* Process info */` |
|      - | 8075 | `	)){` |
|    ! 0 | 8076 | `		goto cleanup_all;` |
|      - | 8077 | `	}` |
|      - | 8078 |  |
|      - | 8079 | `	/* Close handles we don't need in parent */` |
|      2 | 8080 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|      2 | 8081 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|      - | 8082 |  |
|      - | 8083 | `	/* Close thread handle (we only need process handle) */` |
|      2 | 8084 | `	CloseHandle(pi.hThread);` |
|      - | 8085 |  |
|      - | 8086 | `	/* Store process handle for later waiting */` |
|      2 | 8087 | `	*phProcess = pi.hProcess;` |
|      - | 8088 |  |
|      - | 8089 | `	/* Convert OS handle to C file descriptor, then to FILE* */` |
|      2 | 8090 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|      - | 8091 | `	                     bRead ? _O_RDONLY \| _O_TEXT : _O_WRONLY \| _O_TEXT);` |
|      2 | 8092 | `	if( fd == -1 ){` |
|    ! 0 | 8093 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 8094 | `		*phProcess = NULL;` |
|    ! 0 | 8095 | `		goto cleanup_all;` |
|      - | 8096 | `	}` |
|      - | 8097 |  |
|      2 | 8098 | `	pFile = _fdopen(fd, zMode);` |
|      2 | 8099 | `	if( !pFile ){` |
|    ! 0 | 8100 | `		_close(fd); /* This will also close the underlying handle */` |
|    ! 0 | 8101 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 8102 | `		*phProcess = NULL;` |
|    ! 0 | 8103 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    ! 0 | 8104 | `		return NULL;` |
|      - | 8105 | `	}` |
|      - | 8106 |  |
|      2 | 8107 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      2 | 8108 | `	return pFile;` |
|      - | 8109 |  |
|      - | 8110 | `cleanup_all:` |
|    ! 0 | 8111 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      - | 8112 | `cleanup_pipes:` |
|    ! 0 | 8113 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|    ! 0 | 8114 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|    ! 0 | 8115 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|    ! 0 | 8116 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|    ! 0 | 8117 | `	return NULL;` |
|      2 | 8118 |  |
|      - | 8119 |  |
|      - | 8120 | `/*` |
|      - | 8121 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|      - | 8122 | ` */` |
|      - | 8123 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|      2 | 8124 |  |
|      2 | 8125 | `	DWORD dwExitCode = 0;` |
|      - | 8126 | `	int status;` |
|      - | 8127 |  |
|      - | 8128 | `	/* Close the FILE* (this closes the pipe) */` |
|      2 | 8129 | `	fclose(pFile);` |
|      - | 8130 |  |
|      2 | 8131 | `	if( hProcess ){` |
|      - | 8132 | `		/* Wait for the process to complete */` |
|      2 | 8133 | `		WaitForSingleObject(hProcess, INFINITE);` |
|      - | 8134 |  |
|      2 | 8135 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|      2 | 8136 | `			status = (int)dwExitCode;` |
|      2 | 8137 | `		}else{` |
|    ! 0 | 8138 | `			status = -1;` |
|      - | 8139 | `		}` |
|      - | 8140 |  |
|      - | 8141 | `		/* Close process handle */` |
|      2 | 8142 | `		CloseHandle(hProcess);` |
|      2 | 8143 | `	}else{` |
|    ! 0 | 8144 | `		status = -1;` |
|      - | 8145 | `	}` |
|      - | 8146 |  |
|      2 | 8147 | `	return status;` |
|      2 | 8148 |  |
|      - | 8149 | `#endif /* __WINNT__ */` |
|      - | 8150 | `/*` |
|      - | 8151 | ` * Open a pipe to a process.` |
|      - | 8152 | ` * This is called internally by popen(), not through the stream device interface.` |
|      - | 8153 | ` */` |
|   1516 | 8154 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|      2 | 8155 |  |
|      - | 8156 | `	pipe_private *pPipe;` |
|      - | 8157 | `	FILE *pFile;` |
|   1518 | 8158 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|    ! 0 | 8159 | `		return 0;` |
|      - | 8160 | `	}` |
|      - | 8161 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|   1518 | 8162 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|    ! 0 | 8163 | `		return 0;` |
|      - | 8164 | `	}` |
|      - | 8165 | `	/* Open the pipe using system popen */` |
|      - | 8166 | `#ifdef __WINNT__` |
|      - | 8167 | `	{` |
|      - | 8168 | `		/* Build cmd.exe command wrapper */` |
|      2 | 8169 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|      2 | 8170 | `		const char *zShellSuffix = "\"";` |
|      2 | 8171 | `		size_t nPrefix = strlen(zShellPrefix);` |
|      2 | 8172 | `		size_t nSuffix = strlen(zShellSuffix);` |
|      2 | 8173 | `		size_t nCmd = strlen(zCommand);` |
|      2 | 8174 | `		size_t nQuotes = 0;` |
|      2 | 8175 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      2 | 8176 | `			if (zCommand[i] == '"') nQuotes++;` |
|      2 | 8177 | `		}` |
|      2 | 8178 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|      2 | 8179 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|      2 | 8180 | `		if (zCmdEsc == NULL) {` |
|    ! 0 | 8181 | `			return 0;` |
|      - | 8182 | `		}` |
|      - | 8183 | `		/* Escape quotes in command */` |
|      2 | 8184 | `		size_t j = 0;` |
|      2 | 8185 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      2 | 8186 | `			char ch = zCommand[i];` |
|      2 | 8187 | `			if (ch == '"') {` |
|      1 | 8188 | `				zCmdEsc[j++] = '^';` |
|      1 | 8189 | `				zCmdEsc[j++] = '"';` |
|      1 | 8190 | `			} else {` |
|      2 | 8191 | `				zCmdEsc[j++] = ch;` |
|      - | 8192 | `			}` |
|      2 | 8193 | `		}` |
|      2 | 8194 | `		zCmdEsc[j] = '\0';` |
|      2 | 8195 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|      2 | 8196 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|      2 | 8197 | `		if (zWinCmd == NULL) {` |
|    ! 0 | 8198 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 8199 | `			return 0;` |
|      - | 8200 | `		}` |
|      2 | 8201 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|      2 | 8202 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|      2 | 8203 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|      2 | 8204 | `		zWinCmd[nTotal - 1] = '\0';` |
|      - | 8205 | `		/* Allocate pipe structure early so we can store handles */` |
|      2 | 8206 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|      2 | 8207 | `		if( pPipe == 0 ){` |
|    ! 0 | 8208 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 8209 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|    ! 0 | 8210 | `			return 0;` |
|      - | 8211 | `		}` |
|      - | 8212 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|      2 | 8213 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|      2 | 8214 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|      2 | 8215 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|      2 | 8216 | `		if( pFile == 0 ){` |
|    ! 0 | 8217 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|    ! 0 | 8218 | `			return 0;` |
|      - | 8219 | `		}` |
|      - | 8220 | `		/* Initialize remaining fields */` |
|      2 | 8221 | `		pPipe->pFile = pFile;` |
|      2 | 8222 | `		pPipe->pVm = pVm;` |
|      2 | 8223 | `		pPipe->iMode = zMode[0];` |
|      - | 8224 | `	}` |
|      - | 8225 | `#else /* Unix */` |
|   1516 | 8226 | `	pFile = popen(zCommand, zMode);` |
|   1516 | 8227 | `	if( pFile == 0 ){` |
|    ! 0 | 8228 | `		return 0;` |
|      - | 8229 | `	}` |
|      - | 8230 | `	/* Allocate pipe private structure */` |
|   1516 | 8231 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|   1516 | 8232 | `	if( pPipe == 0 ){` |
|      - | 8233 | `		/* Out of memory, close the pipe */` |
|    ! 0 | 8234 | `		pclose(pFile);` |
|    ! 0 | 8235 | `		return 0;` |
|      - | 8236 | `	}` |
|      - | 8237 | `	/* Initialize the structure */` |
|   1516 | 8238 | `	pPipe->pFile = pFile;` |
|   1516 | 8239 | `	pPipe->pVm = pVm;` |
|   1516 | 8240 | `	pPipe->iMode = zMode[0];` |
|      - | 8241 | `#endif` |
|   1518 | 8242 | `	return pPipe;` |
|    760 | 8243 |  |
|      - | 8244 | `/*` |
|      - | 8245 | ` * Close a pipe and return the exit status of the process.` |
|      - | 8246 | ` * Returns the exit status, or -1 on error.` |
|      - | 8247 | ` */` |
|   1516 | 8248 | `static int PipeClose(pipe_private *pPipe)` |
|      2 | 8249 |  |
|      - | 8250 | `	int status;` |
|      - | 8251 | `	ph7_vm *pVm;` |
|   1518 | 8252 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 8253 | `		return -1;` |
|      - | 8254 | `	}` |
|   1518 | 8255 | `	pVm = pPipe->pVm;` |
|      - | 8256 | `	/* Close the pipe and get exit status */` |
|      - | 8257 | `#ifdef __WINNT__` |
|      - | 8258 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|      2 | 8259 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|      - | 8260 | `#else` |
|   1516 | 8261 | `	status = pclose(pPipe->pFile);` |
|      - | 8262 | `	/* On Unix, pclose returns the status from waitpid, need to extract exit code */` |
|   1516 | 8263 | `	if( status != -1 ){` |
|   1516 | 8264 | `		if( WIFEXITED(status) ){` |
|   1516 | 8265 | `			status = WEXITSTATUS(status);` |
|    758 | 8266 | `		}else if( WIFSIGNALED(status) ){` |
|      - | 8267 | `			/* Process was killed by a signal - use shell convention: 128 + signal number */` |
|    ! 0 | 8268 | `			status = 128 + WTERMSIG(status);` |
|    ! 0 | 8269 | `		}else{` |
|      - | 8270 | `			/* Unknown termination reason */` |
|    ! 0 | 8271 | `			status = -1;` |
|      - | 8272 | `		}` |
|    758 | 8273 | `	}` |
|      - | 8274 | `#endif` |
|      - | 8275 | `	/* Free the structure */` |
|   1518 | 8276 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|   1518 | 8277 | `	return status;` |
|    760 | 8278 |  |
|      - | 8279 | `/*` |
|      - | 8280 | ` * Pipe stream xClose implementation.` |
|      - | 8281 | ` * Note: This is called by fclose(), not pclose().` |
|      - | 8282 | ` * It closes the pipe but does not return the exit status.` |
|      - | 8283 | ` */` |
|     14 | 8284 | `static void PipeStream_Close(void *pHandle)` |
|      1 | 8285 |  |
|     15 | 8286 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|     15 | 8287 | `	if( pPipe ){` |
|     15 | 8288 | `		PipeClose(pPipe);` |
|      7 | 8289 | `	}` |
|     15 | 8290 |  |
|      - | 8291 | `/*` |
|      - | 8292 | ` * Pipe stream xRead implementation.` |
|      - | 8293 | ` */` |
|   2128 | 8294 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|      1 | 8295 |  |
|   2129 | 8296 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 8297 | `	size_t nRead;` |
|   2129 | 8298 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 8299 | `		return -1;` |
|      - | 8300 | `	}` |
|   2129 | 8301 | `	if( pPipe->iMode != 'r' ){` |
|      - | 8302 | `		/* Cannot read from a write-only pipe */` |
|    ! 0 | 8303 | `		return -1;` |
|      - | 8304 | `	}` |
|   2129 | 8305 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
|   2129 | 8306 | `	if( nRead == 0 ){` |
|   1529 | 8307 | `		if( feof(pPipe->pFile) ){` |
|   1529 | 8308 | `			return 0; /* EOF */` |
|      - | 8309 | `		}` |
|    ! 0 | 8310 | `		return -1; /* Error */` |
|      - | 8311 | `	}` |
|    601 | 8312 | `	return (ph7_int64)nRead;` |
|   1065 | 8313 |  |
|      - | 8314 | `/*` |
|      - | 8315 | ` * Pipe stream xWrite implementation.` |
|      - | 8316 | ` */` |
|      2 | 8317 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|    ! 0 | 8318 |  |
|      2 | 8319 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 8320 | `	size_t nWritten;` |
|      2 | 8321 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 8322 | `		return -1;` |
|      - | 8323 | `	}` |
|      2 | 8324 | `	if( pPipe->iMode != 'w' ){` |
|      - | 8325 | `		/* Cannot write to a read-only pipe */` |
|    ! 0 | 8326 | `		return -1;` |
|      - | 8327 | `	}` |
|      2 | 8328 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|      2 | 8329 | `	if( nWritten == 0 && nWrite > 0 ){` |
|    ! 0 | 8330 | `		return -1; /* Error */` |
|      - | 8331 | `	}` |
|      2 | 8332 | `	return (ph7_int64)nWritten;` |
|      1 | 8333 |  |
|      - | 8334 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|      - | 8335 | `static const ph7_io_stream sPipe_Stream = {` |
|      - | 8336 | `	"pipe",` |
|      - | 8337 | `	PH7_IO_STREAM_VERSION,` |
|      - | 8338 |  |
|      - | 8339 |  |
|      - | 8340 | `	PipeStream_Close,  /* xClose */` |
|      - | 8341 |  |
|      - | 8342 | `	PipeStream_Read,   /* xRead */` |
|      - | 8343 |  |
|      - | 8344 | `	PipeStream_Write,  /* xWrite */` |
|      - | 8345 |  |
|      - | 8346 |  |
|      - | 8347 |  |
|      - | 8348 |  |
|      - | 8349 |  |
|      - | 8350 |  |
|      - | 8351 |  |
|      - | 8352 | `};` |
|      - | 8353 | `/*` |
|      - | 8354 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|      - | 8355 | ` * FALSE otherwise.` |
|      - | 8356 | ` */` |
|   1502 | 8357 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|      2 | 8358 |  |
|   1504 | 8359 | `	return pStream == &sPipe_Stream;` |
|      2 | 8360 |  |
|      - | 8361 | `/*` |
|      - | 8362 | ` * resource popen(string $command, string $mode)` |
|      - | 8363 | ` *  Opens process file pointer.` |
|      - | 8364 | ` * Parameters` |
|      - | 8365 | ` *  $command` |
|      - | 8366 | ` *   The command to execute. Passed to the system shell.` |
|      - | 8367 | ` *  $mode` |
|      - | 8368 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|      - | 8369 | ` *   'r' - Open for reading (read from the command's stdout).` |
|      - | 8370 | ` *   'w' - Open for writing (write to the command's stdin).` |
|      - | 8371 | ` * Return` |
|      - | 8372 | ` *  Returns a file pointer on success, or FALSE on error.` |
|      - | 8373 | ` */` |
|   1516 | 8374 | `static int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8375 |  |
|      - | 8376 | `	const char *zCommand, *zMode;` |
|      - | 8377 | `	pipe_private *pPipe;` |
|      - | 8378 | `	io_private *pDev;` |
|      - | 8379 | `	int nCmdLen, nModeLen;` |
|   1518 | 8380 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 8381 | `		/* Missing/Invalid arguments, return FALSE */` |
|    ! 0 | 8382 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a command string and mode");` |
|    ! 0 | 8383 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 8384 | `		return PH7_OK;` |
|      - | 8385 | `	}` |
|      - | 8386 | `	/* Extract the command and mode */` |
|   1518 | 8387 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|   1518 | 8388 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|   1518 | 8389 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 8390 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty command");` |
|    ! 0 | 8391 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 8392 | `		return PH7_OK;` |
|      - | 8393 | `	}` |
|   1518 | 8394 | `	if( nModeLen < 1 \|\| (zMode[0] != 'r' && zMode[0] != 'w') ){` |
|    ! 0 | 8395 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Invalid mode, expected 'r' or 'w'");` |
|    ! 0 | 8396 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 8397 | `		return PH7_OK;` |
|      - | 8398 | `	}` |
|      - | 8399 | `	/* Open the pipe */` |
|   1518 | 8400 | `	pPipe = PipeOpen(pCtx->pVm, zCommand, zMode);` |
|   1518 | 8401 | `	if( pPipe == 0 ){` |
|      - | 8402 | `		/* Failed to open pipe */` |
|    ! 0 | 8403 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 8404 | `		return PH7_OK;` |
|      - | 8405 | `	}` |
|      - | 8406 | `	/* Allocate an io_private instance to wrap the pipe */` |
|   1518 | 8407 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|   1518 | 8408 | `	if( pDev == 0 ){` |
|    ! 0 | 8409 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|    ! 0 | 8410 | `		PipeClose(pPipe);` |
|    ! 0 | 8411 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 8412 | `		return PH7_OK;` |
|      - | 8413 | `	}` |
|      - | 8414 | `	/* Initialize the io_private structure */` |
|   1518 | 8415 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|   1518 | 8416 | `	pDev->pHandle = pPipe;` |
|      - | 8417 | `	/* Return the io_private instance as a resource */` |
|   1518 | 8418 | `	ph7_result_resource(pCtx, pDev);` |
|   1518 | 8419 | `	return PH7_OK;` |
|    760 | 8420 |  |
|      - | 8421 | `/*` |
|      - | 8422 | ` * int pclose(resource $handle)` |
|      - | 8423 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|      - | 8424 | ` * Parameters` |
|      - | 8425 | ` *  $handle` |
|      - | 8426 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|      - | 8427 | ` * Return` |
|      - | 8428 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|      - | 8429 | ` */` |
|   1502 | 8430 | `static int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8431 |  |
|      - | 8432 | `	const ph7_io_stream *pStream;` |
|      - | 8433 | `	pipe_private *pPipe;` |
|      - | 8434 | `	io_private *pDev;` |
|      - | 8435 | `	int status;` |
|   1504 | 8436 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 8437 | `		/* Missing/Invalid arguments, return -1 */` |
|    ! 0 | 8438 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 8439 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 8440 | `		return PH7_OK;` |
|      - | 8441 | `	}` |
|      - | 8442 | `	/* Extract our private data */` |
|   1504 | 8443 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 8444 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1504 | 8445 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 8446 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 8447 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 8448 | `		return PH7_OK;` |
|      - | 8449 | `	}` |
|      - | 8450 | `	/* Point to the target IO stream device */` |
|   1504 | 8451 | `	pStream = pDev->pStream;` |
|   1504 | 8452 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|    ! 0 | 8453 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|    ! 0 | 8454 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 8455 | `		return PH7_OK;` |
|      - | 8456 | `	}` |
|      - | 8457 | `	/* Get the pipe handle */` |
|   1504 | 8458 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|      - | 8459 | `	/* Close the pipe and get exit status */` |
|   1504 | 8460 | `	status = PipeClose(pPipe);` |
|      - | 8461 | `	/* Release the IO private structure */` |
|   1504 | 8462 | `	ReleaseIOPrivate(pCtx, pDev);` |
|      - | 8463 | `	/* Invalidate the resource handle */` |
|   1504 | 8464 | `	ph7_value_release(apArg[0]);` |
|      - | 8465 | `	/* Return the exit status */` |
|   1504 | 8466 | `	ph7_result_int(pCtx, status);` |
|   1504 | 8467 | `	return PH7_OK;` |
|    753 | 8468 |  |
|      - | 8469 | `/* Export the php:// stream */` |
|      - | 8470 | `static const ph7_io_stream sPHP_Stream = {` |
|      - | 8471 | `	"php",` |
|      - | 8472 | `	PH7_IO_STREAM_VERSION,` |
|      - | 8473 | `	PHPStreamData_Open,  /* xOpen */` |
|      - | 8474 |  |
|      - | 8475 | `	PHPStreamData_Close, /* xClose */` |
|      - | 8476 |  |
|      - | 8477 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 8478 |  |
|      - | 8479 | `	PHPStreamData_Write, /* xWrite */` |
|      - | 8480 |  |
|      - | 8481 |  |
|      - | 8482 |  |
|      - | 8483 |  |
|      - | 8484 |  |
|      - | 8485 |  |
|      - | 8486 |  |
|      - | 8487 | `};` |
|      - | 8488 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 8489 | `/*` |
|      - | 8490 | ` * Return TRUE if we are dealing with the php:// stream.` |
|      - | 8491 | ` * FALSE otherwise.` |
|      - | 8492 | ` */` |
|     62 | 8493 | `static int is_php_stream(const ph7_io_stream *pStream)` |
|      1 | 8494 |  |
|      - | 8495 | `#ifndef PH7_DISABLE_DISK_IO` |
|     63 | 8496 | `	return pStream == &sPHP_Stream;` |
|      - | 8497 | `#else` |
|      - | 8498 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 8499 | `	return 0;` |
|      - | 8500 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      1 | 8501 |  |
|      - | 8502 |  |
|      - | 8503 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 8504 | `/*` |
|      - | 8505 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 8506 | ` * [i.e: file://,php://].` |
|      - | 8507 | ` * Note:` |
|      - | 8508 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 8509 | ` *  defined then this function is a no-op.` |
|      - | 8510 | ` */` |
|   1244 | 8511 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      2 | 8512 |  |
|      - | 8513 | `	/*` |
|      - | 8514 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 8515 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 8516 | `	 */` |
|      - | 8517 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 8518 | `	/* VFS: disk I/O related functions */` |
|      - | 8519 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 8520 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 8521 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 8522 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 8523 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 8524 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 8525 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 8526 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 8527 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 8528 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 8529 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 8530 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 8531 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 8532 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 8533 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 8534 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 8535 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 8536 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 8537 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 8538 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 8539 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 8540 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 8541 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 8542 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 8543 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 8544 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 8545 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 8546 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 8547 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 8548 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 8549 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 8550 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 8551 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 8552 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 8553 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 8554 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 8555 | `		{"link",        PH7_vfs_link     },` |
|      - | 8556 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 8557 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 8558 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 8559 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 8560 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 8561 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 8562 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 8563 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 8564 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 8565 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 8566 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 8567 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 8568 | `	};` |
|      - | 8569 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 8570 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 8571 | `	 */` |
|      - | 8572 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 8573 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 8574 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 8575 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 8576 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 8577 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 8578 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 8579 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 8580 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 8581 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 8582 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 8583 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 8584 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 8585 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 8586 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 8587 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 8588 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 8589 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 8590 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 8591 | `		{"file",      PH7_builtin_file   },` |
|      - | 8592 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 8593 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 8594 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 8595 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 8596 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 8597 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 8598 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 8599 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 8600 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 8601 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 8602 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 8603 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 8604 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 8605 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 8606 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 8607 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8608 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 8609 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 8610 | `	};` |
|   1246 | 8611 | `	const ph7_io_stream *pFileStream = 0;` |
|   1246 | 8612 | `	sxu32 n = 0;` |
|      - | 8613 | `	/* Register disk-related functions */` |
|  60958 | 8614 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
|  59714 | 8615 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
|  29858 | 8616 | `	}` |
|  44786 | 8617 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
|  43542 | 8618 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  21772 | 8619 | `	}` |
|      - | 8620 | `#else` |
|      - | 8621 | `	SXUNUSED(pVm);` |
|      - | 8622 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 8623 |  |
|      - | 8624 | `	/*` |
|      - | 8625 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 8626 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 8627 | `	 */` |
|      - | 8628 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 8629 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 8630 | `		/* Path processing */` |
|      - | 8631 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 8632 | `		{"basename",    PH7_builtin_basename },` |
|      - | 8633 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 8634 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 8635 | `		{"fnmatch",     PH7_builtin_fnmatch  },` |
|      - | 8636 | `		/* ZIP processing */` |
|      - | 8637 | `		{"zip_open",    PH7_builtin_zip_open },` |
|      - | 8638 | `		{"zip_close",   PH7_builtin_zip_close},` |
|      - | 8639 | `		{"zip_read",    PH7_builtin_zip_read },` |
|      - | 8640 | `		{"zip_entry_open", PH7_builtin_zip_entry_open },` |
|      - | 8641 | `		{"zip_entry_close",PH7_builtin_zip_entry_close},` |
|      - | 8642 | `		{"zip_entry_name", PH7_builtin_zip_entry_name },` |
|      - | 8643 | `		{"zip_entry_filesize",      PH7_builtin_zip_entry_filesize       },` |
|      - | 8644 | `		{"zip_entry_compressedsize",PH7_builtin_zip_entry_compressedsize },` |
|      - | 8645 | `		{"zip_entry_read", PH7_builtin_zip_entry_read },` |
|      - | 8646 | `		{"zip_entry_reset_read_cursor",PH7_builtin_zip_entry_reset_read_cursor},` |
|      - | 8647 | `		{"zip_entry_compressionmethod",PH7_builtin_zip_entry_compressionmethod}` |
|      - | 8648 | `	};` |
|  21150 | 8649 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  19906 | 8650 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|   9954 | 8651 | `	}` |
|      - | 8652 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 8653 |  |
|      - | 8654 | `	/* Install streams if disk I/O is enabled */` |
|      - | 8655 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 8656 | `#ifdef __WINNT__` |
|      2 | 8657 | `	pFileStream = &sWinFileStream;` |
|      - | 8658 | `#elif defined(__UNIXES__)` |
|   1244 | 8659 | `	pFileStream = &sUnixFileStream;` |
|      - | 8660 | `#endif` |
|      - | 8661 | `	/* Install the php:// stream */` |
|   1246 | 8662 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   1246 | 8663 | `	if( pFileStream ){` |
|      - | 8664 | `		/* Install the file:// stream */` |
|   1246 | 8665 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|    622 | 8666 | `	}` |
|      - | 8667 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 8668 |  |
|   1246 | 8669 | `	return SXRET_OK;` |
|      2 | 8670 |  |
|      - | 8671 | `/*` |
|      - | 8672 | ` * Export the STDIN handle.` |
|      - | 8673 | ` */` |
|      2 | 8674 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|      1 | 8675 |  |
|      - | 8676 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 8677 | `	if( pVm->pStdin == 0  ){` |
|      - | 8678 | `		io_private *pIn;` |
|      - | 8679 | `		/* Allocate an IO private instance */` |
|      3 | 8680 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 8681 | `		if( pIn == 0 ){` |
|    ! 0 | 8682 | `			return 0;` |
|      - | 8683 | `		}` |
|      3 | 8684 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|      - | 8685 | `		/* Initialize the handle */` |
|      3 | 8686 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|      - | 8687 | `		/* Install the STDIN stream */` |
|      3 | 8688 | `		pVm->pStdin = pIn;` |
|      3 | 8689 | `		return pIn;` |
|    ! 0 | 8690 | `	}else{` |
|      - | 8691 | `		/* NULL or STDIN */` |
|    ! 0 | 8692 | `		return pVm->pStdin;` |
|      - | 8693 | `	}` |
|      - | 8694 | `#else` |
|      - | 8695 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 8696 | `	return 0;` |
|      - | 8697 | `#endif` |
|      2 | 8698 |  |
|      - | 8699 | `/*` |
|      - | 8700 | ` * Export the STDOUT handle.` |
|      - | 8701 | ` */` |
|      2 | 8702 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|      1 | 8703 |  |
|      - | 8704 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 8705 | `	if( pVm->pStdout == 0  ){` |
|      - | 8706 | `		io_private *pOut;` |
|      - | 8707 | `		/* Allocate an IO private instance */` |
|      3 | 8708 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 8709 | `		if( pOut == 0 ){` |
|    ! 0 | 8710 | `			return 0;` |
|      - | 8711 | `		}` |
|      3 | 8712 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|      - | 8713 | `		/* Initialize the handle */` |
|      3 | 8714 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|      - | 8715 | `		/* Install the STDOUT stream */` |
|      3 | 8716 | `		pVm->pStdout = pOut;` |
|      3 | 8717 | `		return pOut;` |
|    ! 0 | 8718 | `	}else{` |
|      - | 8719 | `		/* NULL or STDOUT */` |
|    ! 0 | 8720 | `		return pVm->pStdout;` |
|      - | 8721 | `	}` |
|      - | 8722 | `#else` |
|      - | 8723 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 8724 | `	return 0;` |
|      - | 8725 | `#endif` |
|      2 | 8726 |  |
|      - | 8727 | `/*` |
|      - | 8728 | ` * Export the STDERR handle.` |
|      - | 8729 | ` */` |
|      2 | 8730 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|      1 | 8731 |  |
|      - | 8732 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 8733 | `	if( pVm->pStderr == 0  ){` |
|      - | 8734 | `		io_private *pErr;` |
|      - | 8735 | `		/* Allocate an IO private instance */` |
|      3 | 8736 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 8737 | `		if( pErr == 0 ){` |
|    ! 0 | 8738 | `			return 0;` |
|      - | 8739 | `		}` |
|      3 | 8740 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|      - | 8741 | `		/* Initialize the handle */` |
|      3 | 8742 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|      - | 8743 | `		/* Install the STDERR stream */` |
|      3 | 8744 | `		pVm->pStderr = pErr;` |
|      3 | 8745 | `		return pErr;` |
|    ! 0 | 8746 | `	}else{` |
|      - | 8747 | `		/* NULL or STDERR */` |
|    ! 0 | 8748 | `		return pVm->pStderr;` |
|      - | 8749 | `	}` |
|      - | 8750 | `#else` |
|      - | 8751 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 8752 | `	return 0;` |
|      - | 8753 | `#endif` |
|      2 | 8754 |  |
|      - | 8755 |  |
