# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2860/4161 lines (68.73%)

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
|     28 |   14 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)` |
|      1 |   15 |  |
|     29 |   16 | `	const char *zEnd = &zPath[nByte - 1];` |
|      - |   17 | `	int c,d;` |
|     29 |   18 | `	c = d = '/';` |
|      - |   19 | `#ifdef __WINNT__` |
|      1 |   20 | `	d = '\\';` |
|      - |   21 | `#endif` |
|    521 |   22 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    479 |   23 | `		zEnd--;` |
|      1 |   24 | `	}` |
|     29 |   25 | `	*pLen = (int)(zEnd-zPath);` |
|      - |   26 | `#ifdef __WINNT__` |
|      1 |   27 | `	if( (*pLen) == (int)sizeof(char) && zPath[0] == '/' ){` |
|      - |   28 | `		/* Normalize path on windows */` |
|    ! 0 |   29 | `		return "\\";` |
|      - |   30 | `	}` |
|      - |   31 | `#endif` |
|     29 |   32 | `	if( zEnd == zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d) ){` |
|      - |   33 | `		/* No separator,return "." as the current directory */` |
|      5 |   34 | `		*pLen = sizeof(char);` |
|      5 |   35 | `		return ".";` |
|      - |   36 | `	}` |
|     25 |   37 | `	if( (*pLen) == 0 ){` |
|      2 |   38 | `		*pLen = sizeof(char);` |
|      - |   39 | `#ifdef __WINNT__` |
|    ! 0 |   40 | `		return "\\";` |
|      - |   41 | `#else` |
|      2 |   42 | `		return "/";` |
|      - |   43 | `#endif` |
|      - |   44 | `	}` |
|     23 |   45 | `	return zPath;` |
|     15 |   46 |  |
|      - |   47 | `/*` |
|      - |   48 | ` * Omit the vfs layer implementation from the built if the PH7_DISABLE_BUILTIN_FUNC directive is defined.` |
|      - |   49 | ` */` |
|      - |   50 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |   51 | `/*` |
|      - |   52 | ` * bool chdir(string $directory)` |
|      - |   53 | ` *  Change the current directory.` |
|      - |   54 | ` * Parameters` |
|      - |   55 | ` *  $directory` |
|      - |   56 | ` *   The new current directory` |
|      - |   57 | ` * Return` |
|      - |   58 | ` *  TRUE on success or FALSE on failure.` |
|      - |   59 | ` */` |
|     12 |   60 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   61 |  |
|      - |   62 | `	const char *zPath;` |
|      - |   63 | `	ph7_vfs *pVfs;` |
|      - |   64 | `	int rc;` |
|     13 |   65 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |   66 | `		/* Missing/Invalid argument,return FALSE */` |
|      9 |   67 | `		ph7_result_bool(pCtx,0);` |
|      9 |   68 | `		return PH7_OK;` |
|      - |   69 | `	}` |
|      - |   70 | `	/* Point to the underlying vfs */` |
|      5 |   71 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |   72 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |   73 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |   74 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |   75 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |   76 | `			ph7_function_name(pCtx)` |
|      - |   77 | `			);` |
|    ! 0 |   78 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |   79 | `		return PH7_OK;` |
|      - |   80 | `	}` |
|      - |   81 | `	/* Point to the desired directory */` |
|      5 |   82 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |   83 | `	/* Perform the requested operation */` |
|      5 |   84 | `	rc = pVfs->xChdir(zPath);` |
|      - |   85 | `	/* IO return value */` |
|      5 |   86 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 |   87 | `	return PH7_OK;` |
|      7 |   88 |  |
|      - |   89 | `/*` |
|      - |   90 | ` * bool chroot(string $directory)` |
|      - |   91 | ` *  Change the root directory.` |
|      - |   92 | ` * Parameters` |
|      - |   93 | ` *  $directory` |
|      - |   94 | ` *   The path to change the root directory to` |
|      - |   95 | ` * Return` |
|      - |   96 | ` *  TRUE on success or FALSE on failure.` |
|      - |   97 | ` */` |
|      6 |   98 | `static int PH7_vfs_chroot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   99 |  |
|      - |  100 | `	const char *zPath;` |
|      - |  101 | `	ph7_vfs *pVfs;` |
|      - |  102 | `	int rc;` |
|      7 |  103 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  104 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 |  105 | `		ph7_result_bool(pCtx,0);` |
|      5 |  106 | `		return PH7_OK;` |
|      - |  107 | `	}` |
|      - |  108 | `	/* Point to the underlying vfs */` |
|      2 |  109 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 |  110 | `	if( pVfs == 0 \|\| pVfs->xChroot == 0 ){` |
|      - |  111 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  112 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  113 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  114 | `			ph7_function_name(pCtx)` |
|      - |  115 | `			);` |
|    ! 0 |  116 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  117 | `		return PH7_OK;` |
|      - |  118 | `	}` |
|      - |  119 | `	/* Point to the desired directory */` |
|      2 |  120 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  121 | `	/* Perform the requested operation */` |
|      2 |  122 | `	rc = pVfs->xChroot(zPath);` |
|      - |  123 | `	/* IO return value */` |
|      2 |  124 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      2 |  125 | `	return PH7_OK;` |
|      4 |  126 |  |
|      - |  127 | `/*` |
|      - |  128 | ` * string getcwd(void)` |
|      - |  129 | ` *  Gets the current working directory.` |
|      - |  130 | ` * Parameters` |
|      - |  131 | ` *  None` |
|      - |  132 | ` * Return` |
|      - |  133 | ` *  Returns the current working directory on success, or FALSE on failure.` |
|      - |  134 | ` */` |
|     16 |  135 | `static int PH7_vfs_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  136 |  |
|      - |  137 | `	ph7_vfs *pVfs;` |
|      - |  138 | `	int rc;` |
|      - |  139 | `	/* Point to the underlying vfs */` |
|     17 |  140 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     17 |  141 | `	if( pVfs == 0 \|\| pVfs->xGetcwd == 0 ){` |
|    ! 0 |  142 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 |  143 | `		SXUNUSED(apArg);` |
|      - |  144 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  145 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  146 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  147 | `			ph7_function_name(pCtx)` |
|      - |  148 | `			);` |
|    ! 0 |  149 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  150 | `		return PH7_OK;` |
|      - |  151 | `	}` |
|     17 |  152 | `	ph7_result_string(pCtx,"",0);` |
|      - |  153 | `	/* Perform the requested operation */` |
|     17 |  154 | `	rc = pVfs->xGetcwd(pCtx);` |
|     17 |  155 | `	if( rc != PH7_OK ){` |
|      - |  156 | `		/* Error,return FALSE */` |
|    ! 0 |  157 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  158 | `	}` |
|     17 |  159 | `	return PH7_OK;` |
|      9 |  160 |  |
|      - |  161 | `/*` |
|      - |  162 | ` * bool rmdir(string $directory)` |
|      - |  163 | ` *  Removes directory.` |
|      - |  164 | ` * Parameters` |
|      - |  165 | ` *  $directory` |
|      - |  166 | ` *   The path to the directory` |
|      - |  167 | ` * Return` |
|      - |  168 | ` *  TRUE on success or FALSE on failure.` |
|      - |  169 | ` */` |
|     10 |  170 | `static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  171 |  |
|      - |  172 | `	const char *zPath;` |
|      - |  173 | `	ph7_vfs *pVfs;` |
|      - |  174 | `	int rc;` |
|     11 |  175 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  176 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  177 | `		ph7_result_bool(pCtx,0);` |
|      3 |  178 | `		return PH7_OK;` |
|      - |  179 | `	}` |
|      - |  180 | `	/* Point to the underlying vfs */` |
|      9 |  181 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 |  182 | `	if( pVfs == 0 \|\| pVfs->xRmdir == 0 ){` |
|      - |  183 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  184 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  185 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  186 | `			ph7_function_name(pCtx)` |
|      - |  187 | `			);` |
|    ! 0 |  188 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  189 | `		return PH7_OK;` |
|      - |  190 | `	}` |
|      - |  191 | `	/* Point to the desired directory */` |
|      9 |  192 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  193 | `	/* Perform the requested operation */` |
|      9 |  194 | `	rc = pVfs->xRmdir(zPath);` |
|      - |  195 | `	/* IO return value */` |
|      9 |  196 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      9 |  197 | `	return PH7_OK;` |
|      6 |  198 |  |
|      - |  199 | `/*` |
|      - |  200 | ` * bool is_dir(string $filename)` |
|      - |  201 | ` *  Tells whether the given filename is a directory.` |
|      - |  202 | ` * Parameters` |
|      - |  203 | ` *  $filename` |
|      - |  204 | ` *   Path to the file.` |
|      - |  205 | ` * Return` |
|      - |  206 | ` *  TRUE on success or FALSE on failure.` |
|      - |  207 | ` */` |
|      6 |  208 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  209 |  |
|      - |  210 | `	const char *zPath;` |
|      - |  211 | `	ph7_vfs *pVfs;` |
|      - |  212 | `	int rc;` |
|      7 |  213 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  214 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  215 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  216 | `		return PH7_OK;` |
|      - |  217 | `	}` |
|      - |  218 | `	/* Point to the underlying vfs */` |
|      7 |  219 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 |  220 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  221 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  222 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  223 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  224 | `			ph7_function_name(pCtx)` |
|      - |  225 | `			);` |
|    ! 0 |  226 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  227 | `		return PH7_OK;` |
|      - |  228 | `	}` |
|      - |  229 | `	/* Point to the desired directory */` |
|      7 |  230 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  231 | `	/* Perform the requested operation */` |
|      7 |  232 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  233 | `	/* IO return value */` |
|      7 |  234 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      7 |  235 | `	return PH7_OK;` |
|      4 |  236 |  |
|      - |  237 | `/*` |
|      - |  238 | ` * bool mkdir(string $pathname[,int $mode = 0777 [,bool $recursive = false])` |
|      - |  239 | ` *  Make a directory.` |
|      - |  240 | ` * Parameters` |
|      - |  241 | ` *  $pathname` |
|      - |  242 | ` *   The directory path.` |
|      - |  243 | ` * $mode` |
|      - |  244 | ` *  The mode is 0777 by default, which means the widest possible access.` |
|      - |  245 | ` *  Note:` |
|      - |  246 | ` *   mode is ignored on Windows.` |
|      - |  247 | ` *   Note that you probably want to specify the mode as an octal number, which means` |
|      - |  248 | ` *   it should have a leading zero. The mode is also modified by the current umask` |
|      - |  249 | ` *   which you can change using umask().` |
|      - |  250 | ` * $recursive` |
|      - |  251 | ` *  Allows the creation of nested directories specified in the pathname.` |
|      - |  252 | ` *  Defaults to FALSE. (Not used)` |
|      - |  253 | ` * Return` |
|      - |  254 | ` *  TRUE on success or FALSE on failure.` |
|      - |  255 | ` */` |
|      8 |  256 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  257 |  |
|      9 |  258 | `	int iRecursive = 0;` |
|      - |  259 | `	const char *zPath;` |
|      - |  260 | `	ph7_vfs *pVfs;` |
|      - |  261 | `	int iMode,rc;` |
|      9 |  262 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  263 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  264 | `		ph7_result_bool(pCtx,0);` |
|      3 |  265 | `		return PH7_OK;` |
|      - |  266 | `	}` |
|      - |  267 | `	/* Point to the underlying vfs */` |
|      7 |  268 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 |  269 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  270 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  271 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  272 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  273 | `			ph7_function_name(pCtx)` |
|      - |  274 | `			);` |
|    ! 0 |  275 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  276 | `		return PH7_OK;` |
|      - |  277 | `	}` |
|      - |  278 | `	/* Point to the desired directory */` |
|      7 |  279 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  280 | `#ifdef __WINNT__` |
|      1 |  281 | `	iMode = 0;` |
|      - |  282 | `#else` |
|      - |  283 | `	/* Assume UNIX */` |
|      6 |  284 | `	iMode = 0777;` |
|      - |  285 | `#endif` |
|      7 |  286 | `	if( nArg > 1 ){` |
|    ! 0 |  287 | `		iMode = ph7_value_to_int(apArg[1]);` |
|    ! 0 |  288 | `		if( nArg > 2 ){` |
|    ! 0 |  289 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|    ! 0 |  290 | `		}` |
|    ! 0 |  291 | `	}` |
|      - |  292 | `	/* Perform the requested operation */` |
|      7 |  293 | `	rc = pVfs->xMkdir(zPath,iMode,iRecursive);` |
|      - |  294 | `	/* IO return value */` |
|      7 |  295 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      7 |  296 | `	return PH7_OK;` |
|      5 |  297 |  |
|      - |  298 | `/*` |
|      - |  299 | ` * bool rename(string $oldname,string $newname)` |
|      - |  300 | ` *  Attempts to rename oldname to newname.` |
|      - |  301 | ` * Parameters` |
|      - |  302 | ` *  $oldname` |
|      - |  303 | ` *   Old name.` |
|      - |  304 | ` *  $newname` |
|      - |  305 | ` *   New name.` |
|      - |  306 | ` * Return` |
|      - |  307 | ` *  TRUE on success or FALSE on failure.` |
|      - |  308 | ` */` |
|      4 |  309 | `static int PH7_vfs_rename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  310 |  |
|      - |  311 | `	const char *zOld,*zNew;` |
|      - |  312 | `	ph7_vfs *pVfs;` |
|      - |  313 | `	int rc;` |
|      5 |  314 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - |  315 | `		/* Missing/Invalid arguments,return FALSE */` |
|      3 |  316 | `		ph7_result_bool(pCtx,0);` |
|      3 |  317 | `		return PH7_OK;` |
|      - |  318 | `	}` |
|      - |  319 | `	/* Point to the underlying vfs */` |
|      3 |  320 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  321 | `	if( pVfs == 0 \|\| pVfs->xRename == 0 ){` |
|      - |  322 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  323 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  324 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  325 | `			ph7_function_name(pCtx)` |
|      - |  326 | `			);` |
|    ! 0 |  327 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  328 | `		return PH7_OK;` |
|      - |  329 | `	}` |
|      - |  330 | `	/* Perform the requested operation */` |
|      3 |  331 | `	zOld = ph7_value_to_string(apArg[0],0);` |
|      3 |  332 | `	zNew = ph7_value_to_string(apArg[1],0);` |
|      3 |  333 | `	rc = pVfs->xRename(zOld,zNew);` |
|      - |  334 | `	/* IO result */` |
|      3 |  335 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 |  336 | `	return PH7_OK;` |
|      3 |  337 |  |
|      - |  338 | `/*` |
|      - |  339 | ` * string realpath(string $path)` |
|      - |  340 | ` *  Returns canonicalized absolute pathname.` |
|      - |  341 | ` * Parameters` |
|      - |  342 | ` *  $path` |
|      - |  343 | ` *   Target path.` |
|      - |  344 | ` * Return` |
|      - |  345 | ` *  Canonicalized absolute pathname on success. or FALSE on failure.` |
|      - |  346 | ` */` |
|     10 |  347 | `static int PH7_vfs_realpath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  348 |  |
|      - |  349 | `	const char *zPath;` |
|      - |  350 | `	ph7_vfs *pVfs;` |
|      - |  351 | `        int rc;` |
|     11 |  352 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  353 | `		/* Missing/Invalid argument,return FALSE */` |
|      7 |  354 | `		ph7_result_bool(pCtx,0);` |
|      7 |  355 | `		return PH7_OK;` |
|      - |  356 | `	}` |
|      - |  357 | `	/* Point to the underlying vfs */` |
|      5 |  358 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  359 | `	if( pVfs == 0 \|\| pVfs->xRealpath == 0 ){` |
|      - |  360 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  361 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  362 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  363 | `			ph7_function_name(pCtx)` |
|      - |  364 | `			);` |
|    ! 0 |  365 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  366 | `		return PH7_OK;` |
|      - |  367 | `	}` |
|      - |  368 | `	/* Set an empty string untnil the underlying OS interface change that */` |
|      5 |  369 | `	ph7_result_string(pCtx,"",0);` |
|      - |  370 | `	/* Perform the requested operation */` |
|      5 |  371 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      5 |  372 | `	rc = pVfs->xRealpath(zPath,pCtx);` |
|      5 |  373 | `	if( rc != PH7_OK ){` |
|      2 |  374 | `	 ph7_result_bool(pCtx,0);` |
|      1 |  375 | `	}` |
|      5 |  376 | `	return PH7_OK;` |
|      6 |  377 |  |
|      - |  378 | `/*` |
|      - |  379 | ` * int sleep(int $seconds)` |
|      - |  380 | ` *  Delays the program execution for the given number of seconds.` |
|      - |  381 | ` * Parameters` |
|      - |  382 | ` *  $seconds` |
|      - |  383 | ` *   Halt time in seconds.` |
|      - |  384 | ` * Return` |
|      - |  385 | ` *  Zero on success or FALSE on failure.` |
|      - |  386 | ` */` |
|      6 |  387 | `static int PH7_vfs_sleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  388 |  |
|      - |  389 | `	ph7_vfs *pVfs;` |
|      - |  390 | `	int rc,nSleep;` |
|      7 |  391 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  392 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  393 | `		ph7_result_bool(pCtx,0);` |
|      3 |  394 | `		return PH7_OK;` |
|      - |  395 | `	}` |
|      - |  396 | `	/* Point to the underlying vfs */` |
|      5 |  397 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  398 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  399 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  400 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  401 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  402 | `			ph7_function_name(pCtx)` |
|      - |  403 | `			);` |
|    ! 0 |  404 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  405 | `		return PH7_OK;` |
|      - |  406 | `	}` |
|      - |  407 | `	/* Amount to sleep */` |
|      5 |  408 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|      5 |  409 | `	if( nSleep < 0 ){` |
|      - |  410 | `		/* Invalid value,return FALSE */` |
|      3 |  411 | `		ph7_result_bool(pCtx,0);` |
|      3 |  412 | `		return PH7_OK;` |
|      - |  413 | `	}` |
|      - |  414 | `	/* Perform the requested operation (Microseconds) */` |
|      3 |  415 | `	rc = pVfs->xSleep((unsigned int)(nSleep * SX_USEC_PER_SEC));` |
|      3 |  416 | `	if( rc != PH7_OK ){` |
|      - |  417 | `		/* Return FALSE */` |
|    ! 0 |  418 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  419 | `	}else{` |
|      - |  420 | `		/* Return zero */` |
|      3 |  421 | `		ph7_result_int(pCtx,0);` |
|      - |  422 | `	}` |
|      3 |  423 | `	return PH7_OK;` |
|      4 |  424 |  |
|      - |  425 | `/*` |
|      - |  426 | ` * void usleep(int $micro_seconds)` |
|      - |  427 | ` *  Delays program execution for the given number of micro seconds.` |
|      - |  428 | ` * Parameters` |
|      - |  429 | ` *  $micro_seconds` |
|      - |  430 | ` *   Halt time in micro seconds. A micro second is one millionth of a second.` |
|      - |  431 | ` * Return` |
|      - |  432 | ` *  None.` |
|      - |  433 | ` */` |
|      4 |  434 | `static int PH7_vfs_usleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  435 |  |
|      - |  436 | `	ph7_vfs *pVfs;` |
|      - |  437 | `	int nSleep;` |
|      5 |  438 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  439 | `		/* Missing/Invalid argument,return immediately */` |
|    ! 0 |  440 | `		return PH7_OK;` |
|      - |  441 | `	}` |
|      - |  442 | `	/* Point to the underlying vfs */` |
|      5 |  443 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  444 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  445 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  446 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  447 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 |  448 | `			ph7_function_name(pCtx)` |
|      - |  449 | `			);` |
|    ! 0 |  450 | `		return PH7_OK;` |
|      - |  451 | `	}` |
|      - |  452 | `	/* Amount to sleep */` |
|      5 |  453 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|      5 |  454 | `	if( nSleep < 0 ){` |
|      - |  455 | `		/* Invalid value,return immediately */` |
|      3 |  456 | `		return PH7_OK;` |
|      - |  457 | `	}` |
|      - |  458 | `	/* Perform the requested operation (Microseconds) */` |
|      3 |  459 | `	pVfs->xSleep((unsigned int)nSleep);` |
|      3 |  460 | `	return PH7_OK;` |
|      3 |  461 |  |
|      - |  462 | `/*` |
|      - |  463 | ` * bool unlink (string $filename)` |
|      - |  464 | ` *  Delete a file.` |
|      - |  465 | ` * Parameters` |
|      - |  466 | ` *  $filename` |
|      - |  467 | ` *   Path to the file.` |
|      - |  468 | ` * Return` |
|      - |  469 | ` *  TRUE on success or FALSE on failure.` |
|      - |  470 | ` */` |
|    178 |  471 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  472 |  |
|      - |  473 | `	const char *zPath;` |
|      - |  474 | `	ph7_vfs *pVfs;` |
|      - |  475 | `	int rc;` |
|    179 |  476 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  477 | `		/* Missing/Invalid argument,return FALSE */` |
|     93 |  478 | `		ph7_result_bool(pCtx,0);` |
|     93 |  479 | `		return PH7_OK;` |
|      - |  480 | `	}` |
|      - |  481 | `	/* Point to the underlying vfs */` |
|     87 |  482 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     87 |  483 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  484 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  485 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  486 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  487 | `			ph7_function_name(pCtx)` |
|      - |  488 | `			);` |
|    ! 0 |  489 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  490 | `		return PH7_OK;` |
|      - |  491 | `	}` |
|      - |  492 | `	/* Point to the desired directory */` |
|     87 |  493 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  494 | `	/* Perform the requested operation */` |
|     87 |  495 | `	rc = pVfs->xUnlink(zPath);` |
|      - |  496 | `	/* IO return value */` |
|     87 |  497 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     87 |  498 | `	return PH7_OK;` |
|     90 |  499 |  |
|      - |  500 | `/*` |
|      - |  501 | ` * bool chmod(string $filename,int $mode)` |
|      - |  502 | ` *  Attempts to change the mode of the specified file to that given in mode.` |
|      - |  503 | ` * Parameters` |
|      - |  504 | ` *  $filename` |
|      - |  505 | ` *   Path to the file.` |
|      - |  506 | ` * $mode` |
|      - |  507 | ` *   Mode (Must be an integer)` |
|      - |  508 | ` * Return` |
|      - |  509 | ` *  TRUE on success or FALSE on failure.` |
|      - |  510 | ` */` |
|     10 |  511 | `static int PH7_vfs_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  512 |  |
|      - |  513 | `	const char *zPath;` |
|      - |  514 | `	ph7_vfs *pVfs;` |
|      - |  515 | `	int iMode;` |
|      - |  516 | `	int rc;` |
|     10 |  517 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  518 | `		/* Missing/Invalid argument,return FALSE */` |
|      2 |  519 | `		ph7_result_bool(pCtx,0);` |
|      2 |  520 | `		return PH7_OK;` |
|      - |  521 | `	}` |
|      - |  522 | `	/* Point to the underlying vfs */` |
|      8 |  523 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      8 |  524 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - |  525 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  526 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  527 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  528 | `			ph7_function_name(pCtx)` |
|      - |  529 | `			);` |
|    ! 0 |  530 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  531 | `		return PH7_OK;` |
|      - |  532 | `	}` |
|      - |  533 | `	/* Point to the desired directory */` |
|      8 |  534 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  535 | `	/* Extract the mode */` |
|      8 |  536 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - |  537 | `	/* Perform the requested operation */` |
|      8 |  538 | `	rc = pVfs->xChmod(zPath,iMode);` |
|      - |  539 | `	/* IO return value */` |
|      8 |  540 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      8 |  541 | `	return PH7_OK;` |
|      5 |  542 |  |
|      - |  543 | `/*` |
|      - |  544 | ` * bool chown(string $filename,string $user)` |
|      - |  545 | ` *  Attempts to change the owner of the file filename to user user.` |
|      - |  546 | ` * Parameters` |
|      - |  547 | ` *  $filename` |
|      - |  548 | ` *   Path to the file.` |
|      - |  549 | ` * $user` |
|      - |  550 | ` *   Username.` |
|      - |  551 | ` * Return` |
|      - |  552 | ` *  TRUE on success or FALSE on failure.` |
|      - |  553 | ` */` |
|      6 |  554 | `static int PH7_vfs_chown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  555 |  |
|      - |  556 | `	const char *zPath,*zUser;` |
|      - |  557 | `	ph7_vfs *pVfs;` |
|      - |  558 | `	int rc;` |
|      7 |  559 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  560 | `		/* Missing/Invalid arguments,return FALSE */` |
|      3 |  561 | `		ph7_result_bool(pCtx,0);` |
|      3 |  562 | `		return PH7_OK;` |
|      - |  563 | `	}` |
|      - |  564 | `	/* Point to the underlying vfs */` |
|      4 |  565 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 |  566 | `	if( pVfs == 0 \|\| pVfs->xChown == 0 ){` |
|      - |  567 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  568 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  569 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  570 | `			ph7_function_name(pCtx)` |
|      - |  571 | `			);` |
|    ! 0 |  572 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  573 | `		return PH7_OK;` |
|      - |  574 | `	}` |
|      - |  575 | `	/* Point to the desired directory */` |
|      4 |  576 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  577 | `	/* Extract the user */` |
|      4 |  578 | `	zUser = ph7_value_to_string(apArg[1],0);` |
|      - |  579 | `	/* Perform the requested operation */` |
|      4 |  580 | `	rc = pVfs->xChown(zPath,zUser);` |
|      - |  581 | `	/* IO return value */` |
|      4 |  582 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 |  583 | `	return PH7_OK;` |
|      4 |  584 |  |
|      - |  585 | `/*` |
|      - |  586 | ` * bool chgrp(string $filename,string $group)` |
|      - |  587 | ` *  Attempts to change the group of the file filename to group.` |
|      - |  588 | ` * Parameters` |
|      - |  589 | ` *  $filename` |
|      - |  590 | ` *   Path to the file.` |
|      - |  591 | ` * $group` |
|      - |  592 | ` *   groupname.` |
|      - |  593 | ` * Return` |
|      - |  594 | ` *  TRUE on success or FALSE on failure.` |
|      - |  595 | ` */` |
|      6 |  596 | `static int PH7_vfs_chgrp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  597 |  |
|      - |  598 | `	const char *zPath,*zGroup;` |
|      - |  599 | `	ph7_vfs *pVfs;` |
|      - |  600 | `	int rc;` |
|      7 |  601 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  602 | `		/* Missing/Invalid arguments,return FALSE */` |
|      3 |  603 | `		ph7_result_bool(pCtx,0);` |
|      3 |  604 | `		return PH7_OK;` |
|      - |  605 | `	}` |
|      - |  606 | `	/* Point to the underlying vfs */` |
|      4 |  607 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 |  608 | `	if( pVfs == 0 \|\| pVfs->xChgrp == 0 ){` |
|      - |  609 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  610 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  611 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  612 | `			ph7_function_name(pCtx)` |
|      - |  613 | `			);` |
|    ! 0 |  614 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  615 | `		return PH7_OK;` |
|      - |  616 | `	}` |
|      - |  617 | `	/* Point to the desired directory */` |
|      4 |  618 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  619 | `	/* Extract the user */` |
|      4 |  620 | `	zGroup = ph7_value_to_string(apArg[1],0);` |
|      - |  621 | `	/* Perform the requested operation */` |
|      4 |  622 | `	rc = pVfs->xChgrp(zPath,zGroup);` |
|      - |  623 | `	/* IO return value */` |
|      4 |  624 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 |  625 | `	return PH7_OK;` |
|      4 |  626 |  |
|      - |  627 | `/*` |
|      - |  628 | ` * int64 disk_free_space(string $directory)` |
|      - |  629 | ` *  Returns available space on filesystem or disk partition.` |
|      - |  630 | ` * Parameters` |
|      - |  631 | ` *  $directory` |
|      - |  632 | ` *   A directory of the filesystem or disk partition.` |
|      - |  633 | ` * Return` |
|      - |  634 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  635 | ` */` |
|      2 |  636 | `static int PH7_vfs_disk_free_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  637 |  |
|      - |  638 | `	const char *zPath;` |
|      - |  639 | `	ph7_int64 iSize;` |
|      - |  640 | `	ph7_vfs *pVfs;` |
|      3 |  641 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  642 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  643 | `		ph7_result_bool(pCtx,0);` |
|      3 |  644 | `		return PH7_OK;` |
|      - |  645 | `	}` |
|      - |  646 | `	/* Point to the underlying vfs */` |
|    ! 0 |  647 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    ! 0 |  648 | `	if( pVfs == 0 \|\| pVfs->xFreeSpace == 0 ){` |
|      - |  649 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  650 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  651 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  652 | `			ph7_function_name(pCtx)` |
|      - |  653 | `			);` |
|    ! 0 |  654 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  655 | `		return PH7_OK;` |
|      - |  656 | `	}` |
|      - |  657 | `	/* Point to the desired directory */` |
|    ! 0 |  658 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  659 | `	/* Perform the requested operation */` |
|    ! 0 |  660 | `	iSize = pVfs->xFreeSpace(zPath);` |
|      - |  661 | `	/* IO return value */` |
|    ! 0 |  662 | `	ph7_result_int64(pCtx,iSize);` |
|    ! 0 |  663 | `	return PH7_OK;` |
|      2 |  664 |  |
|      - |  665 | `/*` |
|      - |  666 | ` * int64 disk_total_space(string $directory)` |
|      - |  667 | ` *  Returns the total size of a filesystem or disk partition.` |
|      - |  668 | ` * Parameters` |
|      - |  669 | ` *  $directory` |
|      - |  670 | ` *   A directory of the filesystem or disk partition.` |
|      - |  671 | ` * Return` |
|      - |  672 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  673 | ` */` |
|      2 |  674 | `static int PH7_vfs_disk_total_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  675 |  |
|      - |  676 | `	const char *zPath;` |
|      - |  677 | `	ph7_int64 iSize;` |
|      - |  678 | `	ph7_vfs *pVfs;` |
|      2 |  679 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  680 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  681 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  682 | `		return PH7_OK;` |
|      - |  683 | `	}` |
|      - |  684 | `	/* Point to the underlying vfs */` |
|      2 |  685 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 |  686 | `	if( pVfs == 0 \|\| pVfs->xTotalSpace == 0 ){` |
|      - |  687 | `		/* IO routine not implemented,return NULL */` |
|      3 |  688 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  689 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|      1 |  690 | `			ph7_function_name(pCtx)` |
|      - |  691 | `			);` |
|      2 |  692 | `		ph7_result_bool(pCtx,0);` |
|      2 |  693 | `		return PH7_OK;` |
|      - |  694 | `	}` |
|      - |  695 | `	/* Point to the desired directory */` |
|    ! 0 |  696 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  697 | `	/* Perform the requested operation */` |
|    ! 0 |  698 | `	iSize = pVfs->xTotalSpace(zPath);` |
|      - |  699 | `	/* IO return value */` |
|    ! 0 |  700 | `	ph7_result_int64(pCtx,iSize);` |
|    ! 0 |  701 | `	return PH7_OK;` |
|      1 |  702 |  |
|      - |  703 | `/*` |
|      - |  704 | ` * bool file_exists(string $filename)` |
|      - |  705 | ` *  Checks whether a file or directory exists.` |
|      - |  706 | ` * Parameters` |
|      - |  707 | ` *  $filename` |
|      - |  708 | ` *   Path to the file.` |
|      - |  709 | ` * Return` |
|      - |  710 | ` *  TRUE on success or FALSE on failure.` |
|      - |  711 | ` */` |
|     42 |  712 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  713 |  |
|      - |  714 | `	const char *zPath;` |
|      - |  715 | `	ph7_vfs *pVfs;` |
|      - |  716 | `	int rc;` |
|     43 |  717 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  718 | `		/* Missing/Invalid argument,return FALSE */` |
|      7 |  719 | `		ph7_result_bool(pCtx,0);` |
|      7 |  720 | `		return PH7_OK;` |
|      - |  721 | `	}` |
|      - |  722 | `	/* Point to the underlying vfs */` |
|     37 |  723 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     37 |  724 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - |  725 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  726 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  727 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  728 | `			ph7_function_name(pCtx)` |
|      - |  729 | `			);` |
|    ! 0 |  730 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  731 | `		return PH7_OK;` |
|      - |  732 | `	}` |
|      - |  733 | `	/* Point to the desired directory */` |
|     37 |  734 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  735 | `	/* Perform the requested operation */` |
|     37 |  736 | `	rc = pVfs->xFileExists(zPath);` |
|      - |  737 | `	/* IO return value */` |
|     37 |  738 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     37 |  739 | `	return PH7_OK;` |
|     22 |  740 |  |
|      - |  741 | `/*` |
|      - |  742 | ` * int64 file_size(string $filename)` |
|      - |  743 | ` *  Gets the size for the given file.` |
|      - |  744 | ` * Parameters` |
|      - |  745 | ` *  $filename` |
|      - |  746 | ` *   Path to the file.` |
|      - |  747 | ` * Return` |
|      - |  748 | ` *  File size on success or FALSE on failure.` |
|      - |  749 | ` */` |
|     26 |  750 | `static int PH7_vfs_file_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  751 |  |
|      - |  752 | `	const char *zPath;` |
|      - |  753 | `	ph7_int64 iSize;` |
|      - |  754 | `	ph7_vfs *pVfs;` |
|     27 |  755 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  756 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  757 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  758 | `		return PH7_OK;` |
|      - |  759 | `	}` |
|      - |  760 | `	/* Point to the underlying vfs */` |
|     27 |  761 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     27 |  762 | `	if( pVfs == 0 \|\| pVfs->xFileSize == 0 ){` |
|      - |  763 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  764 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  765 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  766 | `			ph7_function_name(pCtx)` |
|      - |  767 | `			);` |
|    ! 0 |  768 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  769 | `		return PH7_OK;` |
|      - |  770 | `	}` |
|      - |  771 | `	/* Point to the desired directory */` |
|     27 |  772 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  773 | `	/* Perform the requested operation */` |
|     27 |  774 | `	iSize = pVfs->xFileSize(zPath);` |
|      - |  775 | `	/* IO return value */` |
|     27 |  776 | `	ph7_result_int64(pCtx,iSize);` |
|     27 |  777 | `	return PH7_OK;` |
|     14 |  778 |  |
|      - |  779 | `/*` |
|      - |  780 | ` * int64 fileatime(string $filename)` |
|      - |  781 | ` *  Gets the last access time of the given file.` |
|      - |  782 | ` * Parameters` |
|      - |  783 | ` *  $filename` |
|      - |  784 | ` *   Path to the file.` |
|      - |  785 | ` * Return` |
|      - |  786 | ` *  File atime on success or FALSE on failure.` |
|      - |  787 | ` */` |
|      2 |  788 | `static int PH7_vfs_file_atime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  789 |  |
|      - |  790 | `	const char *zPath;` |
|      - |  791 | `	ph7_int64 iTime;` |
|      - |  792 | `	ph7_vfs *pVfs;` |
|      3 |  793 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  794 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  795 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  796 | `		return PH7_OK;` |
|      - |  797 | `	}` |
|      - |  798 | `	/* Point to the underlying vfs */` |
|      3 |  799 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  800 | `	if( pVfs == 0 \|\| pVfs->xFileAtime == 0 ){` |
|      - |  801 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  802 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  803 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  804 | `			ph7_function_name(pCtx)` |
|      - |  805 | `			);` |
|    ! 0 |  806 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  807 | `		return PH7_OK;` |
|      - |  808 | `	}` |
|      - |  809 | `	/* Point to the desired directory */` |
|      3 |  810 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  811 | `	/* Perform the requested operation */` |
|      3 |  812 | `	iTime = pVfs->xFileAtime(zPath);` |
|      - |  813 | `	/* IO return value */` |
|      3 |  814 | `	ph7_result_int64(pCtx,iTime);` |
|      3 |  815 | `	return PH7_OK;` |
|      2 |  816 |  |
|      - |  817 | `/*` |
|      - |  818 | ` * int64 filemtime(string $filename)` |
|      - |  819 | ` *  Gets file modification time.` |
|      - |  820 | ` * Parameters` |
|      - |  821 | ` *  $filename` |
|      - |  822 | ` *   Path to the file.` |
|      - |  823 | ` * Return` |
|      - |  824 | ` *  File mtime on success or FALSE on failure.` |
|      - |  825 | ` */` |
|      4 |  826 | `static int PH7_vfs_file_mtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  827 |  |
|      - |  828 | `	const char *zPath;` |
|      - |  829 | `	ph7_int64 iTime;` |
|      - |  830 | `	ph7_vfs *pVfs;` |
|      5 |  831 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  832 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  833 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  834 | `		return PH7_OK;` |
|      - |  835 | `	}` |
|      - |  836 | `	/* Point to the underlying vfs */` |
|      5 |  837 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  838 | `	if( pVfs == 0 \|\| pVfs->xFileMtime == 0 ){` |
|      - |  839 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  840 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  841 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  842 | `			ph7_function_name(pCtx)` |
|      - |  843 | `			);` |
|    ! 0 |  844 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  845 | `		return PH7_OK;` |
|      - |  846 | `	}` |
|      - |  847 | `	/* Point to the desired directory */` |
|      5 |  848 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  849 | `	/* Perform the requested operation */` |
|      5 |  850 | `	iTime = pVfs->xFileMtime(zPath);` |
|      - |  851 | `	/* IO return value */` |
|      5 |  852 | `	ph7_result_int64(pCtx,iTime);` |
|      5 |  853 | `	return PH7_OK;` |
|      3 |  854 |  |
|      - |  855 | `/*` |
|      - |  856 | ` * int64 filectime(string $filename)` |
|      - |  857 | ` *  Gets inode change time of file.` |
|      - |  858 | ` * Parameters` |
|      - |  859 | ` *  $filename` |
|      - |  860 | ` *   Path to the file.` |
|      - |  861 | ` * Return` |
|      - |  862 | ` *  File ctime on success or FALSE on failure.` |
|      - |  863 | ` */` |
|      2 |  864 | `static int PH7_vfs_file_ctime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  865 |  |
|      - |  866 | `	const char *zPath;` |
|      - |  867 | `	ph7_int64 iTime;` |
|      - |  868 | `	ph7_vfs *pVfs;` |
|      3 |  869 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  870 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  871 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  872 | `		return PH7_OK;` |
|      - |  873 | `	}` |
|      - |  874 | `	/* Point to the underlying vfs */` |
|      3 |  875 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  876 | `	if( pVfs == 0 \|\| pVfs->xFileCtime == 0 ){` |
|      - |  877 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  878 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  879 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  880 | `			ph7_function_name(pCtx)` |
|      - |  881 | `			);` |
|    ! 0 |  882 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  883 | `		return PH7_OK;` |
|      - |  884 | `	}` |
|      - |  885 | `	/* Point to the desired directory */` |
|      3 |  886 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  887 | `	/* Perform the requested operation */` |
|      3 |  888 | `	iTime = pVfs->xFileCtime(zPath);` |
|      - |  889 | `	/* IO return value */` |
|      3 |  890 | `	ph7_result_int64(pCtx,iTime);` |
|      3 |  891 | `	return PH7_OK;` |
|      2 |  892 |  |
|      - |  893 | `/*` |
|      - |  894 | ` * bool is_file(string $filename)` |
|      - |  895 | ` *  Tells whether the filename is a regular file.` |
|      - |  896 | ` * Parameters` |
|      - |  897 | ` *  $filename` |
|      - |  898 | ` *   Path to the file.` |
|      - |  899 | ` * Return` |
|      - |  900 | ` *  TRUE on success or FALSE on failure.` |
|      - |  901 | ` */` |
|      4 |  902 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  903 |  |
|      - |  904 | `	const char *zPath;` |
|      - |  905 | `	ph7_vfs *pVfs;` |
|      - |  906 | `	int rc;` |
|      5 |  907 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  908 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  909 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  910 | `		return PH7_OK;` |
|      - |  911 | `	}` |
|      - |  912 | `	/* Point to the underlying vfs */` |
|      5 |  913 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 |  914 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - |  915 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  916 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  917 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  918 | `			ph7_function_name(pCtx)` |
|      - |  919 | `			);` |
|    ! 0 |  920 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  921 | `		return PH7_OK;` |
|      - |  922 | `	}` |
|      - |  923 | `	/* Point to the desired directory */` |
|      5 |  924 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  925 | `	/* Perform the requested operation */` |
|      5 |  926 | `	rc = pVfs->xIsfile(zPath);` |
|      - |  927 | `	/* IO return value */` |
|      5 |  928 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 |  929 | `	return PH7_OK;` |
|      3 |  930 |  |
|      - |  931 | `/*` |
|      - |  932 | ` * bool is_link(string $filename)` |
|      - |  933 | ` *  Tells whether the filename is a symbolic link.` |
|      - |  934 | ` * Parameters` |
|      - |  935 | ` *  $filename` |
|      - |  936 | ` *   Path to the file.` |
|      - |  937 | ` * Return` |
|      - |  938 | ` *  TRUE on success or FALSE on failure.` |
|      - |  939 | ` */` |
|      4 |  940 | `static int PH7_vfs_is_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  941 |  |
|      - |  942 | `	const char *zPath;` |
|      - |  943 | `	ph7_vfs *pVfs;` |
|      - |  944 | `	int rc;` |
|      4 |  945 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  946 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  947 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  948 | `		return PH7_OK;` |
|      - |  949 | `	}` |
|      - |  950 | `	/* Point to the underlying vfs */` |
|      4 |  951 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 |  952 | `	if( pVfs == 0 \|\| pVfs->xIslink == 0 ){` |
|      - |  953 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  954 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  955 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  956 | `			ph7_function_name(pCtx)` |
|      - |  957 | `			);` |
|    ! 0 |  958 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  959 | `		return PH7_OK;` |
|      - |  960 | `	}` |
|      - |  961 | `	/* Point to the desired directory */` |
|      4 |  962 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  963 | `	/* Perform the requested operation */` |
|      4 |  964 | `	rc = pVfs->xIslink(zPath);` |
|      - |  965 | `	/* IO return value */` |
|      4 |  966 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 |  967 | `	return PH7_OK;` |
|      2 |  968 |  |
|      - |  969 | `/*` |
|      - |  970 | ` * bool is_readable(string $filename)` |
|      - |  971 | ` *  Tells whether a file exists and is readable.` |
|      - |  972 | ` * Parameters` |
|      - |  973 | ` *  $filename` |
|      - |  974 | ` *   Path to the file.` |
|      - |  975 | ` * Return` |
|      - |  976 | ` *  TRUE on success or FALSE on failure.` |
|      - |  977 | ` */` |
|      2 |  978 | `static int PH7_vfs_is_readable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  979 |  |
|      - |  980 | `	const char *zPath;` |
|      - |  981 | `	ph7_vfs *pVfs;` |
|      - |  982 | `	int rc;` |
|      2 |  983 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  984 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  985 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  986 | `		return PH7_OK;` |
|      - |  987 | `	}` |
|      - |  988 | `	/* Point to the underlying vfs */` |
|      2 |  989 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 |  990 | `	if( pVfs == 0 \|\| pVfs->xReadable == 0 ){` |
|      - |  991 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  992 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  993 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  994 | `			ph7_function_name(pCtx)` |
|      - |  995 | `			);` |
|    ! 0 |  996 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  997 | `		return PH7_OK;` |
|      - |  998 | `	}` |
|      - |  999 | `	/* Point to the desired directory */` |
|      2 | 1000 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1001 | `	/* Perform the requested operation */` |
|      2 | 1002 | `	rc = pVfs->xReadable(zPath);` |
|      - | 1003 | `	/* IO return value */` |
|      2 | 1004 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      2 | 1005 | `	return PH7_OK;` |
|      1 | 1006 |  |
|      - | 1007 | `/*` |
|      - | 1008 | ` * bool is_writable(string $filename)` |
|      - | 1009 | ` *  Tells whether the filename is writable.` |
|      - | 1010 | ` * Parameters` |
|      - | 1011 | ` *  $filename` |
|      - | 1012 | ` *   Path to the file.` |
|      - | 1013 | ` * Return` |
|      - | 1014 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1015 | ` */` |
|      8 | 1016 | `static int PH7_vfs_is_writable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1017 |  |
|      - | 1018 | `	const char *zPath;` |
|      - | 1019 | `	ph7_vfs *pVfs;` |
|      - | 1020 | `	int rc;` |
|      9 | 1021 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1022 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 1023 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1024 | `		return PH7_OK;` |
|      - | 1025 | `	}` |
|      - | 1026 | `	/* Point to the underlying vfs */` |
|      4 | 1027 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 | 1028 | `	if( pVfs == 0 \|\| pVfs->xWritable == 0 ){` |
|      - | 1029 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1030 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1031 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1032 | `			ph7_function_name(pCtx)` |
|      - | 1033 | `			);` |
|    ! 0 | 1034 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1035 | `		return PH7_OK;` |
|      - | 1036 | `	}` |
|      - | 1037 | `	/* Point to the desired directory */` |
|      4 | 1038 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1039 | `	/* Perform the requested operation */` |
|      4 | 1040 | `	rc = pVfs->xWritable(zPath);` |
|      - | 1041 | `	/* IO return value */` |
|      4 | 1042 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 | 1043 | `	return PH7_OK;` |
|      5 | 1044 |  |
|      - | 1045 | `/*` |
|      - | 1046 | ` * bool is_executable(string $filename)` |
|      - | 1047 | ` *  Tells whether the filename is executable.` |
|      - | 1048 | ` * Parameters` |
|      - | 1049 | ` *  $filename` |
|      - | 1050 | ` *   Path to the file.` |
|      - | 1051 | ` * Return` |
|      - | 1052 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1053 | ` */` |
|      2 | 1054 | `static int PH7_vfs_is_executable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1055 |  |
|      - | 1056 | `	const char *zPath;` |
|      - | 1057 | `	ph7_vfs *pVfs;` |
|      - | 1058 | `	int rc;` |
|      2 | 1059 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1060 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1061 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1062 | `		return PH7_OK;` |
|      - | 1063 | `	}` |
|      - | 1064 | `	/* Point to the underlying vfs */` |
|      2 | 1065 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 1066 | `	if( pVfs == 0 \|\| pVfs->xExecutable == 0 ){` |
|      - | 1067 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1068 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1069 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1070 | `			ph7_function_name(pCtx)` |
|      - | 1071 | `			);` |
|    ! 0 | 1072 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1073 | `		return PH7_OK;` |
|      - | 1074 | `	}` |
|      - | 1075 | `	/* Point to the desired directory */` |
|      2 | 1076 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1077 | `	/* Perform the requested operation */` |
|      2 | 1078 | `	rc = pVfs->xExecutable(zPath);` |
|      - | 1079 | `	/* IO return value */` |
|      2 | 1080 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      2 | 1081 | `	return PH7_OK;` |
|      1 | 1082 |  |
|      - | 1083 | `/*` |
|      - | 1084 | ` * string filetype(string $filename)` |
|      - | 1085 | ` *  Gets file type.` |
|      - | 1086 | ` * Parameters` |
|      - | 1087 | ` *  $filename` |
|      - | 1088 | ` *   Path to the file.` |
|      - | 1089 | ` * Return` |
|      - | 1090 | ` *  The type of the file. Possible values are fifo, char, dir, block, link` |
|      - | 1091 | ` *  file, socket and unknown.` |
|      - | 1092 | ` */` |
|      4 | 1093 | `static int PH7_vfs_filetype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1094 |  |
|      - | 1095 | `	const char *zPath;` |
|      - | 1096 | `	ph7_vfs *pVfs;` |
|      5 | 1097 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1098 | `		/* Missing/Invalid argument,return 'unknown' */` |
|    ! 0 | 1099 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 1100 | `		return PH7_OK;` |
|      - | 1101 | `	}` |
|      - | 1102 | `	/* Point to the underlying vfs */` |
|      5 | 1103 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1104 | `	if( pVfs == 0 \|\| pVfs->xFiletype == 0 ){` |
|      - | 1105 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1106 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1107 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1108 | `			ph7_function_name(pCtx)` |
|      - | 1109 | `			);` |
|    ! 0 | 1110 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1111 | `		return PH7_OK;` |
|      - | 1112 | `	}` |
|      - | 1113 | `	/* Point to the desired directory */` |
|      5 | 1114 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1115 | `	/* Set the empty string as the default return value */` |
|      5 | 1116 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1117 | `	/* Perform the requested operation */` |
|      5 | 1118 | `	pVfs->xFiletype(zPath,pCtx);` |
|      5 | 1119 | `	return PH7_OK;` |
|      3 | 1120 |  |
|      - | 1121 | `/*` |
|      - | 1122 | ` * array stat(string $filename)` |
|      - | 1123 | ` *  Gives information about a file.` |
|      - | 1124 | ` * Parameters` |
|      - | 1125 | ` *  $filename` |
|      - | 1126 | ` *   Path to the file.` |
|      - | 1127 | ` * Return` |
|      - | 1128 | ` *  An associative array on success holding the following entries on success` |
|      - | 1129 | ` *  0   dev     device number` |
|      - | 1130 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1131 | ` * 2    mode    inode protection mode` |
|      - | 1132 | ` * 3    nlink   number of links` |
|      - | 1133 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1134 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1135 | ` * 6    rdev    device type, if inode device` |
|      - | 1136 | ` * 7    size    size in bytes` |
|      - | 1137 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1138 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1139 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1140 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1141 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1142 | ` * Note:` |
|      - | 1143 | ` *  FALSE is returned on failure.` |
|      - | 1144 | ` */` |
|      4 | 1145 | `static int PH7_vfs_stat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1146 |  |
|      - | 1147 | `	ph7_value *pArray,*pValue;` |
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
|      5 | 1158 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|      - | 1159 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1160 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1161 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1162 | `			ph7_function_name(pCtx)` |
|      - | 1163 | `			);` |
|    ! 0 | 1164 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1165 | `		return PH7_OK;` |
|      - | 1166 | `	}` |
|      - | 1167 | `	/* Create the array and the working value */` |
|      5 | 1168 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 1169 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 1170 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1171 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1172 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1173 | `		return PH7_OK;` |
|      - | 1174 | `	}` |
|      - | 1175 | `	/* Extract the file path */` |
|      5 | 1176 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1177 | `	/* Perform the requested operation */` |
|      5 | 1178 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|      5 | 1179 | `	if( rc != PH7_OK ){` |
|      - | 1180 | `		/* IO error,return FALSE */` |
|    ! 0 | 1181 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1182 | `	}else{` |
|      - | 1183 | `		/* Return the associative array */` |
|      5 | 1184 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1185 | `	}` |
|      - | 1186 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1187 | `	 * automatically as soon we return from this function. */` |
|      5 | 1188 | `	return PH7_OK;` |
|      3 | 1189 |  |
|      - | 1190 | `/*` |
|      - | 1191 | ` * array lstat(string $filename)` |
|      - | 1192 | ` *  Gives information about a file or symbolic link.` |
|      - | 1193 | ` * Parameters` |
|      - | 1194 | ` *  $filename` |
|      - | 1195 | ` *   Path to the file.` |
|      - | 1196 | ` * Return` |
|      - | 1197 | ` *  An associative array on success holding the following entries on success` |
|      - | 1198 | ` *  0   dev     device number` |
|      - | 1199 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1200 | ` * 2    mode    inode protection mode` |
|      - | 1201 | ` * 3    nlink   number of links` |
|      - | 1202 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1203 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1204 | ` * 6    rdev    device type, if inode device` |
|      - | 1205 | ` * 7    size    size in bytes` |
|      - | 1206 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1207 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1208 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1209 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1210 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1211 | ` * Note:` |
|      - | 1212 | ` *  FALSE is returned on failure.` |
|      - | 1213 | ` */` |
|      2 | 1214 | `static int PH7_vfs_lstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1215 |  |
|      - | 1216 | `	ph7_value *pArray,*pValue;` |
|      - | 1217 | `	const char *zPath;` |
|      - | 1218 | `	ph7_vfs *pVfs;` |
|      - | 1219 | `	int rc;` |
|      2 | 1220 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1221 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1222 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1223 | `		return PH7_OK;` |
|      - | 1224 | `	}` |
|      - | 1225 | `	/* Point to the underlying vfs */` |
|      2 | 1226 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 1227 | `	if( pVfs == 0 \|\| pVfs->xlStat == 0 ){` |
|      - | 1228 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1229 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1230 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1231 | `			ph7_function_name(pCtx)` |
|      - | 1232 | `			);` |
|    ! 0 | 1233 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1234 | `		return PH7_OK;` |
|      - | 1235 | `	}` |
|      - | 1236 | `	/* Create the array and the working value */` |
|      2 | 1237 | `	pArray = ph7_context_new_array(pCtx);` |
|      2 | 1238 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      2 | 1239 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1240 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1241 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1242 | `		return PH7_OK;` |
|      - | 1243 | `	}` |
|      - | 1244 | `	/* Extract the file path */` |
|      2 | 1245 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1246 | `	/* Perform the requested operation */` |
|      2 | 1247 | `	rc = pVfs->xlStat(zPath,pArray,pValue);` |
|      2 | 1248 | `	if( rc != PH7_OK ){` |
|      - | 1249 | `		/* IO error,return FALSE */` |
|    ! 0 | 1250 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1251 | `	}else{` |
|      - | 1252 | `		/* Return the associative array */` |
|      2 | 1253 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1254 | `	}` |
|      - | 1255 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1256 | `	 * automatically as soon we return from this function. */` |
|      2 | 1257 | `	return PH7_OK;` |
|      1 | 1258 |  |
|      - | 1259 | `/*` |
|      - | 1260 | ` * string getenv(string $varname)` |
|      - | 1261 | ` *  Gets the value of an environment variable.` |
|      - | 1262 | ` * Parameters` |
|      - | 1263 | ` *  $varname` |
|      - | 1264 | ` *   The variable name.` |
|      - | 1265 | ` * Return` |
|      - | 1266 | ` *  Returns the value of the environment variable varname, or FALSE if the environment` |
|      - | 1267 | ` * variable varname does not exist.` |
|      - | 1268 | ` */` |
|     16 | 1269 | `static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1270 |  |
|      - | 1271 | `	const char *zEnv;` |
|      - | 1272 | `	ph7_vfs *pVfs;` |
|      - | 1273 | `	int iLen;` |
|     17 | 1274 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1275 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1276 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1277 | `		return PH7_OK;` |
|      - | 1278 | `	}` |
|      - | 1279 | `	/* Point to the underlying vfs */` |
|     17 | 1280 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     17 | 1281 | `	if( pVfs == 0 \|\| pVfs->xGetenv == 0 ){` |
|      - | 1282 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1283 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1284 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1285 | `			ph7_function_name(pCtx)` |
|      - | 1286 | `			);` |
|    ! 0 | 1287 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1288 | `		return PH7_OK;` |
|      - | 1289 | `	}` |
|      - | 1290 | `	/* Extract the environment variable */` |
|     17 | 1291 | `	zEnv = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1292 | `	/* Set a boolean FALSE as the default return value */` |
|     17 | 1293 | `	ph7_result_bool(pCtx,0);` |
|     17 | 1294 | `	if( iLen < 1 ){` |
|      - | 1295 | `		/* Empty string */` |
|    ! 0 | 1296 | `		return PH7_OK;` |
|      - | 1297 | `	}` |
|      - | 1298 | `	/* Perform the requested operation */` |
|     17 | 1299 | `	pVfs->xGetenv(zEnv,pCtx);` |
|     17 | 1300 | `	return PH7_OK;` |
|      9 | 1301 |  |
|      - | 1302 | `/*` |
|      - | 1303 | ` * bool putenv(string $settings)` |
|      - | 1304 | ` *  Set the value of an environment variable.` |
|      - | 1305 | ` * Parameters` |
|      - | 1306 | ` *  $setting` |
|      - | 1307 | ` *   The setting, like "FOO=BAR"` |
|      - | 1308 | ` * Return` |
|      - | 1309 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1310 | ` */` |
|      6 | 1311 | `static int PH7_vfs_putenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1312 |  |
|      - | 1313 | `	const char *zName,*zValue;` |
|      - | 1314 | `	char *zSettings,*zEnd;` |
|      - | 1315 | `	ph7_vfs *pVfs;` |
|      - | 1316 | `	int iLen,rc;` |
|      7 | 1317 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1318 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1319 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1320 | `		return PH7_OK;` |
|      - | 1321 | `	}` |
|      - | 1322 | `	/* Extract the setting variable */` |
|      7 | 1323 | `	zSettings = (char *)ph7_value_to_string(apArg[0],&iLen);` |
|      7 | 1324 | `	if( iLen < 1 ){` |
|      - | 1325 | `		/* Empty string,return FALSE */` |
|    ! 0 | 1326 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1327 | `		return PH7_OK;` |
|      - | 1328 | `	}` |
|      - | 1329 | `	/* Parse the setting */` |
|      7 | 1330 | `	zEnd = &zSettings[iLen];` |
|      7 | 1331 | `	zValue = 0;` |
|      7 | 1332 | `	zName = zSettings;` |
|    127 | 1333 | `	while( zSettings < zEnd ){` |
|    127 | 1334 | `		if( zSettings[0] == '=' ){` |
|      - | 1335 | `			/* Null terminate the name */` |
|      7 | 1336 | `			zSettings[0] = 0;` |
|      7 | 1337 | `			zValue = &zSettings[1];` |
|      7 | 1338 | `			break;` |
|      - | 1339 | `		}` |
|    121 | 1340 | `		zSettings++;` |
|      1 | 1341 | `	}` |
|      - | 1342 | `	/* Install the environment variable in the $_Env array */` |
|      7 | 1343 | `	if( zValue == 0 \|\| zName[0] == 0 \|\| zValue >= zEnd \|\| zName >= zValue ){` |
|      - | 1344 | `		/* Invalid settings,retun FALSE */` |
|      5 | 1345 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1346 | `		if( zSettings  < zEnd ){` |
|      5 | 1347 | `			zSettings[0] = '=';` |
|      2 | 1348 | `		}` |
|      5 | 1349 | `		return PH7_OK;` |
|      - | 1350 | `	}` |
|      3 | 1351 | `	ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_ENV_ATTR,zName,zValue,(int)(zEnd-zValue));` |
|      - | 1352 | `	/* Point to the underlying vfs */` |
|      3 | 1353 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1354 | `	if( pVfs == 0 \|\| pVfs->xSetenv == 0 ){` |
|      - | 1355 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1356 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1357 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1358 | `			ph7_function_name(pCtx)` |
|      - | 1359 | `			);` |
|    ! 0 | 1360 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1361 | `		zSettings[0] = '=';` |
|    ! 0 | 1362 | `		return PH7_OK;` |
|      - | 1363 | `	}` |
|      - | 1364 | `	/* Perform the requested operation */` |
|      3 | 1365 | `	rc = pVfs->xSetenv(zName,zValue);` |
|      3 | 1366 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 1367 | `	zSettings[0] = '=';` |
|      3 | 1368 | `	return PH7_OK;` |
|      4 | 1369 |  |
|      - | 1370 | `/*` |
|      - | 1371 | ` * bool touch(string $filename[,int64 $time = time()[,int64 $atime]])` |
|      - | 1372 | ` *  Sets access and modification time of file.` |
|      - | 1373 | ` * Note: On windows` |
|      - | 1374 | ` *   If the file does not exists,it will not be created.` |
|      - | 1375 | ` * Parameters` |
|      - | 1376 | ` *  $filename` |
|      - | 1377 | ` *   The name of the file being touched.` |
|      - | 1378 | ` *  $time` |
|      - | 1379 | ` *   The touch time. If time is not supplied, the current system time is used.` |
|      - | 1380 | ` * $atime` |
|      - | 1381 | ` *   If present, the access time of the given filename is set to the value of atime.` |
|      - | 1382 | ` *   Otherwise, it is set to the value passed to the time parameter. If neither are` |
|      - | 1383 | ` *   present, the current system time is used.` |
|      - | 1384 | ` * Return` |
|      - | 1385 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1386 | `*/` |
|      4 | 1387 | `static int PH7_vfs_touch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1388 |  |
|      - | 1389 | `	ph7_int64 nTime,nAccess;` |
|      - | 1390 | `	const char *zFile;` |
|      - | 1391 | `	ph7_vfs *pVfs;` |
|      - | 1392 | `	int rc;` |
|      5 | 1393 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1394 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1395 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1396 | `		return PH7_OK;` |
|      - | 1397 | `	}` |
|      - | 1398 | `	/* Point to the underlying vfs */` |
|      5 | 1399 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1400 | `	if( pVfs == 0 \|\| pVfs->xTouch == 0 ){` |
|      - | 1401 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1402 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1403 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1404 | `			ph7_function_name(pCtx)` |
|      - | 1405 | `			);` |
|    ! 0 | 1406 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1407 | `		return PH7_OK;` |
|      - | 1408 | `	}` |
|      - | 1409 | `	/* Perform the requested operation */` |
|      5 | 1410 | `	nTime = nAccess = -1;` |
|      5 | 1411 | `	zFile = ph7_value_to_string(apArg[0],0);` |
|      5 | 1412 | `	if( nArg > 1 ){` |
|      2 | 1413 | `		nTime = ph7_value_to_int64(apArg[1]);` |
|      2 | 1414 | `		if( nArg > 2 ){` |
|      2 | 1415 | `			nAccess = ph7_value_to_int64(apArg[1]);` |
|      1 | 1416 | `		}else{` |
|    ! 0 | 1417 | `			nAccess = nTime;` |
|      - | 1418 | `		}` |
|      1 | 1419 | `	}` |
|      5 | 1420 | `	rc = pVfs->xTouch(zFile,nTime,nAccess);` |
|      - | 1421 | `	/* IO result */` |
|      5 | 1422 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 1423 | `	return PH7_OK;` |
|      3 | 1424 |  |
|      - | 1425 | `/*` |
|      - | 1426 | ` * Path processing functions that do not need access to the VFS layer` |
|      - | 1427 | ` * Status:` |
|      - | 1428 | ` *    Stable.` |
|      - | 1429 | ` */` |
|      - | 1430 | `/*` |
|      - | 1431 | ` * string dirname(string $path)` |
|      - | 1432 | ` *  Returns parent directory's path.` |
|      - | 1433 | ` * Parameters` |
|      - | 1434 | ` * $path` |
|      - | 1435 | ` *  Target path.` |
|      - | 1436 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1437 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1438 | ` * Return` |
|      - | 1439 | ` *  The path of the parent directory. If there are no slashes in path, a dot ('.')` |
|      - | 1440 | ` *  is returned, indicating the current directory.` |
|      - | 1441 | ` */` |
|     10 | 1442 | `static int PH7_builtin_dirname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1443 |  |
|      - | 1444 | `	const char *zPath,*zDir;` |
|      - | 1445 | `	int iLen,iDirlen;` |
|     11 | 1446 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1447 | `		/* Missing/Invalid arguments,return the empty string */` |
|      2 | 1448 | `		ph7_result_string(pCtx,"",0);` |
|      2 | 1449 | `		return PH7_OK;` |
|      - | 1450 | `	}` |
|      - | 1451 | `	/* Point to the target path */` |
|      9 | 1452 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|      9 | 1453 | `	if( iLen < 1 ){` |
|      - | 1454 | `		/* Reuturn "." */` |
|    ! 0 | 1455 | `		ph7_result_string(pCtx,".",sizeof(char));` |
|    ! 0 | 1456 | `		return PH7_OK;` |
|      - | 1457 | `	}` |
|      - | 1458 | `	/* Perform the requested operation */` |
|      9 | 1459 | `	zDir = PH7_ExtractDirName(zPath,iLen,&iDirlen);` |
|      - | 1460 | `	/* Return directory name */` |
|      9 | 1461 | `	ph7_result_string(pCtx,zDir,iDirlen);` |
|      9 | 1462 | `	return PH7_OK;` |
|      6 | 1463 |  |
|      - | 1464 | `/*` |
|      - | 1465 | ` * string basename(string $path[, string $suffix ])` |
|      - | 1466 | ` *  Returns trailing name component of path.` |
|      - | 1467 | ` * Parameters` |
|      - | 1468 | ` * $path` |
|      - | 1469 | ` *  Target path.` |
|      - | 1470 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1471 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1472 | ` * $suffix` |
|      - | 1473 | ` *  If the name component ends in suffix this will also be cut off.` |
|      - | 1474 | ` * Return` |
|      - | 1475 | ` *  The base name of the given path.` |
|      - | 1476 | ` */` |
|     18 | 1477 | `static int PH7_builtin_basename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1478 |  |
|      - | 1479 | `	const char *zPath,*zBase,*zEnd;` |
|      - | 1480 | `	int c,d,iLen;` |
|     19 | 1481 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1482 | `		/* Missing/Invalid argument,return the empty string */` |
|      3 | 1483 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1484 | `		return PH7_OK;` |
|      - | 1485 | `	}` |
|     17 | 1486 | `	c = d = '/';` |
|      - | 1487 | `#ifdef __WINNT__` |
|      1 | 1488 | `	d = '\\';` |
|      - | 1489 | `#endif` |
|      - | 1490 | `	/* Point to the target path */` |
|     17 | 1491 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     17 | 1492 | `	if( iLen < 1 ){` |
|      - | 1493 | `		/* Empty string */` |
|    ! 0 | 1494 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1495 | `		return PH7_OK;` |
|      - | 1496 | `	}` |
|      - | 1497 | `	/* Perform the requested operation */` |
|     17 | 1498 | `	zEnd = &zPath[iLen - 1];` |
|      - | 1499 | `	/* Ignore trailing '/' */` |
|     30 | 1500 | `	while( zEnd > zPath && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      6 | 1501 | `		zEnd--;` |
|      1 | 1502 | `	}` |
|     17 | 1503 | `	iLen = (int)(&zEnd[1]-zPath);` |
|    135 | 1504 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    111 | 1505 | `		zEnd--;` |
|      1 | 1506 | `	}` |
|     17 | 1507 | `	zBase = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|     17 | 1508 | `	zEnd = &zPath[iLen];` |
|     17 | 1509 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 1510 | `		const char *zSuffix;` |
|      - | 1511 | `		int nSuffix;` |
|      - | 1512 | `		/* Strip suffix */` |
|      5 | 1513 | `		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);` |
|      5 | 1514 | `		if( nSuffix > 0 && nSuffix < iLen && SyMemcmp(&zEnd[-nSuffix],zSuffix,nSuffix) == 0 ){` |
|      5 | 1515 | `			zEnd -= nSuffix;` |
|      2 | 1516 | `		}` |
|      2 | 1517 | `	}` |
|      - | 1518 | `	/* Store the basename */` |
|     17 | 1519 | `	ph7_result_string(pCtx,zBase,(int)(zEnd-zBase));` |
|     17 | 1520 | `	return PH7_OK;` |
|     10 | 1521 |  |
|      - | 1522 | `/*` |
|      - | 1523 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1524 | ` *  Returns information about a file path.` |
|      - | 1525 | ` * Parameter` |
|      - | 1526 | ` *  $path` |
|      - | 1527 | ` *   The path to be parsed.` |
|      - | 1528 | ` *  $options` |
|      - | 1529 | ` *    If present, specifies a specific element to be returned; one of` |
|      - | 1530 | ` *      PATHINFO_DIRNAME, PATHINFO_BASENAME, PATHINFO_EXTENSION or PATHINFO_FILENAME.` |
|      - | 1531 | ` * Return` |
|      - | 1532 | ` *  If the options parameter is not passed, an associative array containing the following` |
|      - | 1533 | ` *  elements is returned: dirname, basename, extension (if any), and filename.` |
|      - | 1534 | ` *  If options is present, returns a string containing the requested element.` |
|      - | 1535 | ` */` |
|      - | 1536 | `typedef struct path_info path_info;` |
|      - | 1537 | `struct path_info` |
|      - | 1538 |  |
|      - | 1539 | `	SyString sDir; /* Directory [i.e: /var/www] */` |
|      - | 1540 | `	SyString sBasename; /* Basename [i.e httpd.conf] */` |
|      - | 1541 | `	SyString sExtension; /* File extension [i.e xml,pdf..] */` |
|      - | 1542 | `	SyString sFilename;  /* Filename */` |
|      - | 1543 | `};` |
|      - | 1544 | `/*` |
|      - | 1545 | ` * Extract path fields.` |
|      - | 1546 | ` */` |
|     10 | 1547 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      1 | 1548 |  |
|     11 | 1549 | `	const char *zPtr,*zEnd = &zPath[nByte - 1];` |
|      - | 1550 | `	SyString *pCur;` |
|      - | 1551 | `	int c,d;` |
|     11 | 1552 | `	c = d = '/';` |
|      - | 1553 | `#ifdef __WINNT__` |
|      1 | 1554 | `	d = '\\';` |
|      - | 1555 | `#endif` |
|      - | 1556 | `	/* Zero the structure */` |
|     11 | 1557 | `	SyZero(pOut,sizeof(path_info));` |
|      - | 1558 | `	/* Handle special case */` |
|     11 | 1559 | `	if( nByte == sizeof(char) && ( (int)zPath[0] == c \|\| (int)zPath[0] == d ) ){` |
|      - | 1560 | `#ifdef __WINNT__` |
|    ! 0 | 1561 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1562 | `#else` |
|    ! 0 | 1563 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1564 | `#endif` |
|    ! 0 | 1565 | `		return SXRET_OK;` |
|      - | 1566 | `	}` |
|      - | 1567 | `	/* Extract the basename */` |
|     66 | 1568 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|     51 | 1569 | `		zEnd--;` |
|      1 | 1570 | `	}` |
|     11 | 1571 | `	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|     11 | 1572 | `	zEnd = &zPath[nByte];` |
|      - | 1573 | `	/* dirname */` |
|     11 | 1574 | `	pCur = &pOut->sDir;` |
|     11 | 1575 | `	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);` |
|     11 | 1576 | `	if( pCur->nByte > 1 ){` |
|     21 | 1577 | `		SyStringTrimTrailingChar(pCur,'/');` |
|      - | 1578 | `#ifdef __WINNT__` |
|      1 | 1579 | `		SyStringTrimTrailingChar(pCur,'\\');` |
|      - | 1580 | `#endif` |
|      6 | 1581 | `	}else if( (int)zPath[0] == c \|\| (int)zPath[0] == d ){` |
|      - | 1582 | `#ifdef __WINNT__` |
|    ! 0 | 1583 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1584 | `#else` |
|    ! 0 | 1585 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1586 | `#endif` |
|    ! 0 | 1587 | `	}` |
|      - | 1588 | `	/* basename/filename */` |
|     11 | 1589 | `	pCur = &pOut->sBasename;` |
|     11 | 1590 | `	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);` |
|     11 | 1591 | `	SyStringTrimLeadingChar(pCur,'/');` |
|      - | 1592 | `#ifdef __WINNT__` |
|      1 | 1593 | `	SyStringTrimLeadingChar(pCur,'\\');` |
|      - | 1594 | `#endif` |
|     11 | 1595 | `	SyStringDupPtr(&pOut->sFilename,pCur);` |
|     11 | 1596 | `	if( pCur->nByte > 0 ){` |
|      - | 1597 | `		/* extension */` |
|     11 | 1598 | `		zEnd--;` |
|     41 | 1599 | `		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){` |
|     31 | 1600 | `			zEnd--;` |
|      1 | 1601 | `		}` |
|     11 | 1602 | `		if( zEnd > pCur->zString ){` |
|     11 | 1603 | `			zEnd++; /* Jump leading dot */` |
|     11 | 1604 | `			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);` |
|      - | 1605 | `			/* Fix filename */` |
|     11 | 1606 | `			pCur = &pOut->sFilename;` |
|     11 | 1607 | `			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){` |
|     11 | 1608 | `				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);` |
|      5 | 1609 | `			}` |
|      5 | 1610 | `		}` |
|      5 | 1611 | `	}` |
|     11 | 1612 | `	return SXRET_OK;` |
|      6 | 1613 |  |
|      - | 1614 | `/*` |
|      - | 1615 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1616 | ` *  See block comment above.` |
|      - | 1617 | ` */` |
|     10 | 1618 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1619 |  |
|      - | 1620 | `	const char *zPath;` |
|      - | 1621 | `	path_info sInfo;` |
|      - | 1622 | `	SyString *pComp;` |
|      - | 1623 | `	int iLen;` |
|     11 | 1624 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1625 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1626 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1627 | `		return PH7_OK;` |
|      - | 1628 | `	}` |
|      - | 1629 | `	/* Point to the target path */` |
|     11 | 1630 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     11 | 1631 | `	if( iLen < 1 ){` |
|      - | 1632 | `		/* Empty string */` |
|    ! 0 | 1633 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1634 | `		return PH7_OK;` |
|      - | 1635 | `	}` |
|      - | 1636 | `	/* Extract path info */` |
|     11 | 1637 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|     15 | 1638 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1639 | `		/* Return path component */` |
|      9 | 1640 | `		int nComp = ph7_value_to_int(apArg[1]);` |
|      9 | 1641 | `		switch(nComp){` |
|      1 | 1642 | `		case 1: /* PATHINFO_DIRNAME */` |
|      3 | 1643 | `			pComp = &sInfo.sDir;` |
|      3 | 1644 | `			if( pComp->nByte > 0 ){` |
|      3 | 1645 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1646 | `			}else{` |
|      - | 1647 | `				/* Expand the empty string */` |
|    ! 0 | 1648 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1649 | `			}` |
|      3 | 1650 | `			break;` |
|      1 | 1651 | `		case 2: /*PATHINFO_BASENAME*/` |
|      3 | 1652 | `			pComp = &sInfo.sBasename;` |
|      3 | 1653 | `			if( pComp->nByte > 0 ){` |
|      3 | 1654 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1655 | `			}else{` |
|      - | 1656 | `				/* Expand the empty string */` |
|    ! 0 | 1657 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1658 | `			}` |
|      3 | 1659 | `			break;` |
|      1 | 1660 | `		case 3: /*PATHINFO_EXTENSION*/` |
|      3 | 1661 | `			pComp = &sInfo.sExtension;` |
|      3 | 1662 | `			if( pComp->nByte > 0 ){` |
|      3 | 1663 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1664 | `			}else{` |
|      - | 1665 | `				/* Expand the empty string */` |
|    ! 0 | 1666 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1667 | `			}` |
|      3 | 1668 | `			break;` |
|      1 | 1669 | `		case 4: /*PATHINFO_FILENAME*/` |
|      3 | 1670 | `			pComp = &sInfo.sFilename;` |
|      3 | 1671 | `			if( pComp->nByte > 0 ){` |
|      3 | 1672 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|      2 | 1673 | `			}else{` |
|      - | 1674 | `				/* Expand the empty string */` |
|    ! 0 | 1675 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1676 | `			}` |
|      3 | 1677 | `			break;` |
|    ! 0 | 1678 | `		default:` |
|      - | 1679 | `			/* Expand the empty string */` |
|    ! 0 | 1680 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1681 | `			break;` |
|      - | 1682 | `		}` |
|      5 | 1683 | `	}else{` |
|      - | 1684 | `		/* Return an associative array */` |
|      - | 1685 | `		ph7_value *pArray,*pValue;` |
|      3 | 1686 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 1687 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 1688 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1689 | `			/* Out of mem,return NULL */` |
|    ! 0 | 1690 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1691 | `			return PH7_OK;` |
|      - | 1692 | `		}` |
|      - | 1693 | `		/* dirname */` |
|      3 | 1694 | `		pComp = &sInfo.sDir;` |
|      3 | 1695 | `		if( pComp->nByte > 0 ){` |
|      3 | 1696 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1697 | `			/* Perform the insertion */` |
|      3 | 1698 | `			ph7_array_add_strkey_elem(pArray,"dirname",pValue); /* Will make it's own copy */` |
|      1 | 1699 | `		}` |
|      - | 1700 | `		/* Reset the string cursor */` |
|      3 | 1701 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1702 | `		/* basername */` |
|      3 | 1703 | `		pComp = &sInfo.sBasename;` |
|      3 | 1704 | `		if( pComp->nByte > 0 ){` |
|      3 | 1705 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1706 | `			/* Perform the insertion */` |
|      3 | 1707 | `			ph7_array_add_strkey_elem(pArray,"basename",pValue); /* Will make it's own copy */` |
|      1 | 1708 | `		}` |
|      - | 1709 | `		/* Reset the string cursor */` |
|      3 | 1710 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1711 | `		/* extension */` |
|      3 | 1712 | `		pComp = &sInfo.sExtension;` |
|      3 | 1713 | `		if( pComp->nByte > 0 ){` |
|      3 | 1714 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1715 | `			/* Perform the insertion */` |
|      3 | 1716 | `			ph7_array_add_strkey_elem(pArray,"extension",pValue); /* Will make it's own copy */` |
|      1 | 1717 | `		}` |
|      - | 1718 | `		/* Reset the string cursor */` |
|      3 | 1719 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 1720 | `		/* filename */` |
|      3 | 1721 | `		pComp = &sInfo.sFilename;` |
|      3 | 1722 | `		if( pComp->nByte > 0 ){` |
|      3 | 1723 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|      - | 1724 | `			/* Perform the insertion */` |
|      3 | 1725 | `			ph7_array_add_strkey_elem(pArray,"filename",pValue); /* Will make it's own copy */` |
|      1 | 1726 | `		}` |
|      - | 1727 | `		/* Return the created array */` |
|      3 | 1728 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1729 | `		/* Don't worry about freeing memory, everything will be released` |
|      - | 1730 | `		 * automatically as soon we return from this foreign function.` |
|      - | 1731 | `		 */` |
|      - | 1732 | `	}` |
|     11 | 1733 | `	return PH7_OK;` |
|      6 | 1734 |  |
|      - | 1735 | `/*` |
|      - | 1736 | ` * Globbing implementation extracted from the sqlite3 source tree.` |
|      - | 1737 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|      - | 1738 | ` * Status: Public Domain` |
|      - | 1739 | ` */` |
|      - | 1740 | `typedef unsigned char u8;` |
|      - | 1741 | `/* An array to map all upper-case characters into their corresponding` |
|      - | 1742 | `** lower-case character.` |
|      - | 1743 | `**` |
|      - | 1744 | `** SQLite only considers US-ASCII (or EBCDIC) characters.  We do not` |
|      - | 1745 | `** handle case conversions for the UTF character set since the tables` |
|      - | 1746 | `** involved are nearly as big or bigger than SQLite itself.` |
|      - | 1747 | `*/` |
|      - | 1748 | `static const unsigned char sqlite3UpperToLower[] = {` |
|      - | 1749 | `      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,` |
|      - | 1750 | `     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,` |
|      - | 1751 | `     36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,` |
|      - | 1752 | `     54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,` |
|      - | 1753 | `    104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,` |
|      - | 1754 | `    122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,` |
|      - | 1755 | `    108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,` |
|      - | 1756 | `    126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,` |
|      - | 1757 | `    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,` |
|      - | 1758 | `    162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,` |
|      - | 1759 | `    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,` |
|      - | 1760 | `    198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,` |
|      - | 1761 | `    216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,` |
|      - | 1762 | `    234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,` |
|      - | 1763 | `    252,253,254,255` |
|      - | 1764 | `};` |
|      - | 1765 | `#define GlogUpperToLower(A)     if( A<0x80 ){ A = sqlite3UpperToLower[A]; }` |
|      - | 1766 | `/*` |
|      - | 1767 | `** Assuming zIn points to the first byte of a UTF-8 character,` |
|      - | 1768 | `** advance zIn to point to the first byte of the next UTF-8 character.` |
|      - | 1769 | `*/` |
|      - | 1770 | `#define SQLITE_SKIP_UTF8(zIn) {                        \` |
|      - | 1771 | `  if( (*(zIn++))>=0xc0 ){                              \` |
|      - | 1772 | `    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \` |
|      - | 1773 | `  }                                                    \` |
|      - | 1774 |  |
|      - | 1775 | `/*` |
|      - | 1776 | `** Compare two UTF-8 strings for equality where the first string can` |
|      - | 1777 | `** potentially be a "glob" expression.  Return true (1) if they` |
|      - | 1778 | `** are the same and false (0) if they are different.` |
|      - | 1779 | `**` |
|      - | 1780 | `** Globbing rules:` |
|      - | 1781 | `**` |
|      - | 1782 | `**      '*'       Matches any sequence of zero or more characters.` |
|      - | 1783 | `**` |
|      - | 1784 | `**      '?'       Matches exactly one character.` |
|      - | 1785 | `**` |
|      - | 1786 | `**     [...]      Matches one character from the enclosed list of` |
|      - | 1787 | `**                characters.` |
|      - | 1788 | `**` |
|      - | 1789 | `**     [^...]     Matches one character not in the enclosed list.` |
|      - | 1790 | `**` |
|      - | 1791 | `** With the [...] and [^...] matching, a ']' character can be included` |
|      - | 1792 | `** in the list by making it the first character after '[' or '^'.  A` |
|      - | 1793 | `** range of characters can be specified using '-'.  Example:` |
|      - | 1794 | `** "[a-z]" matches any single lower-case letter.  To match a '-', make` |
|      - | 1795 | `** it the last character in the list.` |
|      - | 1796 | `**` |
|      - | 1797 | `** This routine is usually quick, but can be N**2 in the worst case.` |
|      - | 1798 | `**` |
|      - | 1799 | `** Hints: to match '*' or '?', put them in "[]".  Like this:` |
|      - | 1800 | `**` |
|      - | 1801 | `**         abc[*]xyz        Matches "abc*xyz" only` |
|      - | 1802 | `*/` |
|     20 | 1803 | `static int patternCompare(` |
|      - | 1804 | `  const u8 *zPattern,              /* The glob pattern */` |
|      - | 1805 | `  const u8 *zString,               /* The string to compare against the glob */` |
|      - | 1806 | `  const int esc,                    /* The escape character */` |
|      - | 1807 | `  int noCase` |
|      1 | 1808 | `){` |
|      - | 1809 | `  int c, c2;` |
|      - | 1810 | `  int invert;` |
|      - | 1811 | `  int seen;` |
|     21 | 1812 | `  u8 matchOne = '?';` |
|     21 | 1813 | `  u8 matchAll = '*';` |
|     21 | 1814 | `  u8 matchSet = '[';` |
|     21 | 1815 | `  int prevEscape = 0;     /* True if the previous character was 'escape' */` |
|      - | 1816 |  |
|     21 | 1817 | `  if( !zPattern \|\| !zString ) return 0;` |
|     51 | 1818 | `  while( (c = PH7_Utf8Read(zPattern,0,&zPattern))!=0 ){` |
|     43 | 1819 | `    if( !prevEscape && c==matchAll ){` |
|     16 | 1820 | `      while( (c=PH7_Utf8Read(zPattern,0,&zPattern)) == matchAll` |
|      9 | 1821 | `               \|\| c == matchOne ){` |
|    ! 0 | 1822 | `        if( c==matchOne && PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1823 | `          return 0;` |
|      - | 1824 | `        }` |
|    ! 0 | 1825 | `      }` |
|      9 | 1826 | `      if( c==0 ){` |
|    ! 0 | 1827 | `        return 1;` |
|      9 | 1828 | `      }else if( c==esc ){` |
|    ! 0 | 1829 | `        c = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1830 | `        if( c==0 ){` |
|    ! 0 | 1831 | `          return 0;` |
|    ! 0 | 1832 | `        }` |
|      9 | 1833 | `      }else if( c==matchSet ){` |
|    ! 0 | 1834 | `	  if( (esc==0) \|\| (matchSet<0x80) ) return 0;` |
|    ! 0 | 1835 | `	  while( *zString && patternCompare(&zPattern[-1],zString,esc,noCase)==0 ){` |
|    ! 0 | 1836 | `          SQLITE_SKIP_UTF8(zString);` |
|    ! 0 | 1837 | `        }` |
|    ! 0 | 1838 | `        return *zString!=0;` |
|      - | 1839 | `      }` |
|     11 | 1840 | `      while( (c2 = PH7_Utf8Read(zString,0,&zString))!=0 ){` |
|     11 | 1841 | `        if( noCase ){` |
|      3 | 1842 | `          GlogUpperToLower(c2);` |
|      3 | 1843 | `          GlogUpperToLower(c);` |
|     11 | 1844 | `          while( c2 != 0 && c2 != c ){` |
|      9 | 1845 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      9 | 1846 | `            GlogUpperToLower(c2);` |
|      1 | 1847 | `          }` |
|      2 | 1848 | `        }else{` |
|     47 | 1849 | `          while( c2 != 0 && c2 != c ){` |
|     39 | 1850 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      1 | 1851 | `          }` |
|      - | 1852 | `        }` |
|     11 | 1853 | `        if( c2==0 ) return 0;` |
|      9 | 1854 | `		if( patternCompare(zPattern,zString,esc,noCase) ) return 1;` |
|      1 | 1855 | `      }` |
|    ! 0 | 1856 | `      return 0;` |
|     35 | 1857 | `    }else if( !prevEscape && c==matchOne ){` |
|    ! 0 | 1858 | `      if( PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 1859 | `        return 0;` |
|    ! 0 | 1860 | `      }` |
|     35 | 1861 | `    }else if( c==matchSet ){` |
|    ! 0 | 1862 | `      int prior_c = 0;` |
|    ! 0 | 1863 | `      if( esc == 0 ) return 0;` |
|    ! 0 | 1864 | `      seen = 0;` |
|    ! 0 | 1865 | `      invert = 0;` |
|    ! 0 | 1866 | `      c = PH7_Utf8Read(zString, 0, &zString);` |
|    ! 0 | 1867 | `      if( c==0 ) return 0;` |
|    ! 0 | 1868 | `      c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1869 | `      if( c2=='^' ){` |
|    ! 0 | 1870 | `        invert = 1;` |
|    ! 0 | 1871 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1872 | `      }` |
|    ! 0 | 1873 | `      if( c2==']' ){` |
|    ! 0 | 1874 | `        if( c==']' ) seen = 1;` |
|    ! 0 | 1875 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1876 | `      }` |
|    ! 0 | 1877 | `      while( c2 && c2!=']' ){` |
|    ! 0 | 1878 | `        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){` |
|    ! 0 | 1879 | `          c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1880 | `          if( c>=prior_c && c<=c2 ) seen = 1;` |
|    ! 0 | 1881 | `          prior_c = 0;` |
|    ! 0 | 1882 | `        }else{` |
|    ! 0 | 1883 | `          if( c==c2 ){` |
|    ! 0 | 1884 | `            seen = 1;` |
|    ! 0 | 1885 | `          }` |
|    ! 0 | 1886 | `          prior_c = c2;` |
|      - | 1887 | `        }` |
|    ! 0 | 1888 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 1889 | `      }` |
|    ! 0 | 1890 | `      if( c2==0 \|\| (seen ^ invert)==0 ){` |
|    ! 0 | 1891 | `        return 0;` |
|    ! 0 | 1892 | `      }` |
|     35 | 1893 | `    }else if( esc==c && !prevEscape ){` |
|    ! 0 | 1894 | `      prevEscape = 1;` |
|    ! 0 | 1895 | `    }else{` |
|     35 | 1896 | `      c2 = PH7_Utf8Read(zString, 0, &zString);` |
|     35 | 1897 | `      if( noCase ){` |
|      7 | 1898 | `        GlogUpperToLower(c);` |
|      7 | 1899 | `        GlogUpperToLower(c2);` |
|      3 | 1900 | `      }` |
|     35 | 1901 | `      if( c!=c2 ){` |
|      5 | 1902 | `        return 0;` |
|      - | 1903 | `      }` |
|     31 | 1904 | `      prevEscape = 0;` |
|      - | 1905 | `    }` |
|      1 | 1906 | `  }` |
|      9 | 1907 | `  return *zString==0;` |
|     11 | 1908 |  |
|      - | 1909 | `/*` |
|      - | 1910 | ` * Wrapper around patternCompare() defined above.` |
|      - | 1911 | ` * See block comment above for more information.` |
|      - | 1912 | ` */` |
|     12 | 1913 | `static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,int CaseCompare)` |
|      1 | 1914 |  |
|      - | 1915 | `	int rc;` |
|     13 | 1916 | `	if( iEsc < 0 ){` |
|    ! 0 | 1917 | `		iEsc = '\\';` |
|    ! 0 | 1918 | `	}` |
|     13 | 1919 | `	rc = patternCompare(zPattern,zString,iEsc,CaseCompare);` |
|     13 | 1920 | `	return rc;` |
|      1 | 1921 |  |
|      - | 1922 | `/*` |
|      - | 1923 | ` * bool fnmatch(string $pattern,string $string[,int $flags = 0 ])` |
|      - | 1924 | ` *  Match filename against a pattern.` |
|      - | 1925 | ` * Parameters` |
|      - | 1926 | ` *  $pattern` |
|      - | 1927 | ` *   The shell wildcard pattern.` |
|      - | 1928 | ` * $string` |
|      - | 1929 | ` *  The tested string.` |
|      - | 1930 | ` * $flags` |
|      - | 1931 | ` *   A list of possible flags:` |
|      - | 1932 | ` *    FNM_NOESCAPE 	Disable backslash escaping.` |
|      - | 1933 | ` *    FNM_PATHNAME 	Slash in string only matches slash in the given pattern.` |
|      - | 1934 | ` *    FNM_PERIOD 	Leading period in string must be exactly matched by period in the given pattern.` |
|      - | 1935 | ` *    FNM_CASEFOLD 	Caseless match.` |
|      - | 1936 | ` * Return` |
|      - | 1937 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 1938 | ` */` |
|      8 | 1939 | `static int PH7_builtin_fnmatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1940 |  |
|      - | 1941 | `	const char *zString,*zPattern;` |
|      9 | 1942 | `	int iEsc = '\\';` |
|      9 | 1943 | `	int noCase = 0;` |
|      - | 1944 | `	int rc;` |
|      9 | 1945 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 1946 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1947 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1948 | `		return PH7_OK;` |
|      - | 1949 | `	}` |
|      - | 1950 | `	/* Extract the pattern and the string */` |
|      9 | 1951 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|      9 | 1952 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 1953 | `	/* Extract the flags if avaialble */` |
|      9 | 1954 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      7 | 1955 | `		rc = ph7_value_to_int(apArg[2]);` |
|      7 | 1956 | `		if( rc & 0x01 /*FNM_NOESCAPE*/){` |
|    ! 0 | 1957 | `			iEsc = 0;` |
|    ! 0 | 1958 | `		}` |
|      7 | 1959 | `		if( rc & 0x08 /*FNM_CASEFOLD*/){` |
|      3 | 1960 | `			noCase = 1;` |
|      1 | 1961 | `		}` |
|      3 | 1962 | `	}` |
|      - | 1963 | `	/* Go globbing */` |
|      9 | 1964 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,noCase);` |
|      - | 1965 | `	/* Globbing result */` |
|      9 | 1966 | `	ph7_result_bool(pCtx,rc);` |
|      9 | 1967 | `	return PH7_OK;` |
|      5 | 1968 |  |
|      - | 1969 | `/*` |
|      - | 1970 | ` * bool strglob(string $pattern,string $string)` |
|      - | 1971 | ` *  Match string against a pattern.` |
|      - | 1972 | ` * Parameters` |
|      - | 1973 | ` *  $pattern` |
|      - | 1974 | ` *   The shell wildcard pattern.` |
|      - | 1975 | ` * $string` |
|      - | 1976 | ` *  The tested string.` |
|      - | 1977 | ` * Return` |
|      - | 1978 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 1979 | ` * Note that this a symisc eXtension.` |
|      - | 1980 | ` */` |
|      4 | 1981 | `static int PH7_builtin_strglob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1982 |  |
|      - | 1983 | `	const char *zString,*zPattern;` |
|      5 | 1984 | `	int iEsc = '\\';` |
|      - | 1985 | `	int rc;` |
|      5 | 1986 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 1987 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1988 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1989 | `		return PH7_OK;` |
|      - | 1990 | `	}` |
|      - | 1991 | `	/* Extract the pattern and the string */` |
|      5 | 1992 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|      5 | 1993 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 1994 | `	/* Go globbing */` |
|      5 | 1995 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0);` |
|      - | 1996 | `	/* Globbing result */` |
|      5 | 1997 | `	ph7_result_bool(pCtx,rc);` |
|      5 | 1998 | `	return PH7_OK;` |
|      3 | 1999 |  |
|      - | 2000 | `/*` |
|      - | 2001 | ` * bool link(string $target,string $link)` |
|      - | 2002 | ` *  Create a hard link.` |
|      - | 2003 | ` * Parameters` |
|      - | 2004 | ` *  $target` |
|      - | 2005 | ` *   Target of the link.` |
|      - | 2006 | ` *  $link` |
|      - | 2007 | ` *   The link name.` |
|      - | 2008 | ` * Return` |
|      - | 2009 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2010 | ` */` |
|      2 | 2011 | `static int PH7_vfs_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2012 |  |
|      - | 2013 | `	const char *zTarget,*zLink;` |
|      - | 2014 | `	ph7_vfs *pVfs;` |
|      - | 2015 | `	int rc;` |
|      2 | 2016 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2017 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2018 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2019 | `		return PH7_OK;` |
|      - | 2020 | `	}` |
|      - | 2021 | `	/* Point to the underlying vfs */` |
|      2 | 2022 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 2023 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2024 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2025 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2026 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2027 | `			ph7_function_name(pCtx)` |
|      - | 2028 | `			);` |
|    ! 0 | 2029 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2030 | `		return PH7_OK;` |
|      - | 2031 | `	}` |
|      - | 2032 | `	/* Extract the given arguments */` |
|      2 | 2033 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      2 | 2034 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2035 | `	/* Perform the requested operation */` |
|      2 | 2036 | `	rc = pVfs->xLink(zTarget,zLink,0/*Not a symbolic link */);` |
|      - | 2037 | `	/* IO result */` |
|      2 | 2038 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      2 | 2039 | `	return PH7_OK;` |
|      1 | 2040 |  |
|      - | 2041 | `/*` |
|      - | 2042 | ` * bool symlink(string $target,string $link)` |
|      - | 2043 | ` *  Creates a symbolic link.` |
|      - | 2044 | ` * Parameters` |
|      - | 2045 | ` *  $target` |
|      - | 2046 | ` *   Target of the link.` |
|      - | 2047 | ` *  $link` |
|      - | 2048 | ` *   The link name.` |
|      - | 2049 | ` * Return` |
|      - | 2050 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2051 | ` */` |
|      8 | 2052 | `static int PH7_vfs_symlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2053 |  |
|      - | 2054 | `	const char *zTarget,*zLink;` |
|      - | 2055 | `	ph7_vfs *pVfs;` |
|      - | 2056 | `	int rc;` |
|      9 | 2057 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2058 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2059 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2060 | `		return PH7_OK;` |
|      - | 2061 | `	}` |
|      - | 2062 | `	/* Point to the underlying vfs */` |
|      9 | 2063 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 | 2064 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2065 | `		/* IO routine not implemented,return NULL */` |
|      1 | 2066 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2067 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2068 | `			ph7_function_name(pCtx)` |
|      - | 2069 | `			);` |
|      1 | 2070 | `		ph7_result_bool(pCtx,0);` |
|      1 | 2071 | `		return PH7_OK;` |
|      - | 2072 | `	}` |
|      - | 2073 | `	/* Extract the given arguments */` |
|      8 | 2074 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      8 | 2075 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2076 | `	/* Perform the requested operation */` |
|      8 | 2077 | `	rc = pVfs->xLink(zTarget,zLink,1/*A symbolic link */);` |
|      - | 2078 | `	/* IO result */` |
|      8 | 2079 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      8 | 2080 | `	return PH7_OK;` |
|      5 | 2081 |  |
|      - | 2082 | `/*` |
|      - | 2083 | ` * int umask([ int $mask ])` |
|      - | 2084 | ` *  Changes the current umask.` |
|      - | 2085 | ` * Parameters` |
|      - | 2086 | ` *  $mask` |
|      - | 2087 | ` *   The new umask.` |
|      - | 2088 | ` * Return` |
|      - | 2089 | ` *  umask() without arguments simply returns the current umask.` |
|      - | 2090 | ` *  Otherwise the old umask is returned.` |
|      - | 2091 | ` */` |
|      8 | 2092 | `static int PH7_vfs_umask(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2093 |  |
|      - | 2094 | `	int iOld,iNew;` |
|      - | 2095 | `	ph7_vfs *pVfs;` |
|      - | 2096 | `	/* Point to the underlying vfs */` |
|      8 | 2097 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      8 | 2098 | `	if( pVfs == 0 \|\| pVfs->xUmask == 0 ){` |
|      - | 2099 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2100 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2101 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2102 | `			ph7_function_name(pCtx)` |
|      - | 2103 | `			);` |
|    ! 0 | 2104 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2105 | `		return PH7_OK;` |
|      - | 2106 | `	}` |
|      8 | 2107 | `	iNew = 0;` |
|      8 | 2108 | `	if( nArg > 0 ){` |
|      4 | 2109 | `		iNew = ph7_value_to_int(apArg[0]);` |
|      2 | 2110 | `	}` |
|      - | 2111 | `	/* Perform the requested operation */` |
|      8 | 2112 | `	iOld = pVfs->xUmask(iNew);` |
|      - | 2113 | `	/* Old mask */` |
|      8 | 2114 | `	ph7_result_int(pCtx,iOld);` |
|      8 | 2115 | `	return PH7_OK;` |
|      4 | 2116 |  |
|      - | 2117 | `/*` |
|      - | 2118 | ` * string sys_get_temp_dir()` |
|      - | 2119 | ` *  Returns directory path used for temporary files.` |
|      - | 2120 | ` * Parameters` |
|      - | 2121 | ` *  None` |
|      - | 2122 | ` * Return` |
|      - | 2123 | ` *  Returns the path of the temporary directory.` |
|      - | 2124 | ` */` |
|    168 | 2125 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2126 |  |
|      - | 2127 | `	ph7_vfs *pVfs;` |
|      - | 2128 | `	/* Set the empty string as the default return value */` |
|    169 | 2129 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2130 | `	/* Point to the underlying vfs */` |
|    169 | 2131 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    169 | 2132 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
|    ! 0 | 2133 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2134 | `		SXUNUSED(apArg);` |
|      - | 2135 | `		/* IO routine not implemented,return "" */` |
|    ! 0 | 2136 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2137 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2138 | `			ph7_function_name(pCtx)` |
|      - | 2139 | `			);` |
|    ! 0 | 2140 | `		return PH7_OK;` |
|      - | 2141 | `	}` |
|      - | 2142 | `	/* Perform the requested operation */` |
|    169 | 2143 | `	pVfs->xTempDir(pCtx);` |
|    169 | 2144 | `	return PH7_OK;` |
|     85 | 2145 |  |
|      - | 2146 | `/*` |
|      - | 2147 | ` * string get_current_user()` |
|      - | 2148 | ` *  Returns the name of the current working user.` |
|      - | 2149 | ` * Parameters` |
|      - | 2150 | ` *  None` |
|      - | 2151 | ` * Return` |
|      - | 2152 | ` *  Returns the name of the current working user.` |
|      - | 2153 | ` */` |
|      2 | 2154 | `static int PH7_vfs_get_current_user(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2155 |  |
|      - | 2156 | `	ph7_vfs *pVfs;` |
|      - | 2157 | `	/* Point to the underlying vfs */` |
|      3 | 2158 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2159 | `	if( pVfs == 0 \|\| pVfs->xUsername == 0 ){` |
|    ! 0 | 2160 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2161 | `		SXUNUSED(apArg);` |
|      - | 2162 | `		/* IO routine not implemented */` |
|    ! 0 | 2163 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2164 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2165 | `			ph7_function_name(pCtx)` |
|      - | 2166 | `			);` |
|      - | 2167 | `		/* Set a dummy username */` |
|    ! 0 | 2168 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 2169 | `		return PH7_OK;` |
|      - | 2170 | `	}` |
|      - | 2171 | `	/* Perform the requested operation */` |
|      3 | 2172 | `	pVfs->xUsername(pCtx);` |
|      3 | 2173 | `	return PH7_OK;` |
|      2 | 2174 |  |
|      - | 2175 | `/*` |
|      - | 2176 | ` * int64 getmypid()` |
|      - | 2177 | ` *  Gets process ID.` |
|      - | 2178 | ` * Parameters` |
|      - | 2179 | ` *  None` |
|      - | 2180 | ` * Return` |
|      - | 2181 | ` *  Returns the process ID.` |
|      - | 2182 | ` */` |
|      4 | 2183 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2184 |  |
|      - | 2185 | `	ph7_int64 nProcessId;` |
|      - | 2186 | `	ph7_vfs *pVfs;` |
|      - | 2187 | `	/* Point to the underlying vfs */` |
|      5 | 2188 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 2189 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
|    ! 0 | 2190 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2191 | `		SXUNUSED(apArg);` |
|      - | 2192 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2193 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2194 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2195 | `			ph7_function_name(pCtx)` |
|      - | 2196 | `			);` |
|    ! 0 | 2197 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2198 | `		return PH7_OK;` |
|      - | 2199 | `	}` |
|      - | 2200 | `	/* Perform the requested operation */` |
|      5 | 2201 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 2202 | `	/* Set the result */` |
|      5 | 2203 | `	ph7_result_int64(pCtx,nProcessId);` |
|      5 | 2204 | `	return PH7_OK;` |
|      3 | 2205 |  |
|      - | 2206 | `/*` |
|      - | 2207 | ` * int getmyuid()` |
|      - | 2208 | ` *  Get user ID.` |
|      - | 2209 | ` * Parameters` |
|      - | 2210 | ` *  None` |
|      - | 2211 | ` * Return` |
|      - | 2212 | ` *  Returns the user ID.` |
|      - | 2213 | ` */` |
|      2 | 2214 | `static int PH7_vfs_getmyuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2215 |  |
|      - | 2216 | `	ph7_vfs *pVfs;` |
|      - | 2217 | `	int nUid;` |
|      - | 2218 | `	/* Point to the underlying vfs */` |
|      2 | 2219 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 2220 | `	if( pVfs == 0 \|\| pVfs->xUid == 0 ){` |
|    ! 0 | 2221 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2222 | `		SXUNUSED(apArg);` |
|      - | 2223 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2224 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2225 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2226 | `			ph7_function_name(pCtx)` |
|      - | 2227 | `			);` |
|    ! 0 | 2228 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2229 | `		return PH7_OK;` |
|      - | 2230 | `	}` |
|      - | 2231 | `	/* Perform the requested operation */` |
|      2 | 2232 | `	nUid = pVfs->xUid();` |
|      - | 2233 | `	/* Set the result */` |
|      2 | 2234 | `	ph7_result_int(pCtx,nUid);` |
|      2 | 2235 | `	return PH7_OK;` |
|      1 | 2236 |  |
|      - | 2237 | `/*` |
|      - | 2238 | ` * int getmygid()` |
|      - | 2239 | ` *  Get group ID.` |
|      - | 2240 | ` * Parameters` |
|      - | 2241 | ` *  None` |
|      - | 2242 | ` * Return` |
|      - | 2243 | ` *  Returns the group ID.` |
|      - | 2244 | ` */` |
|      2 | 2245 | `static int PH7_vfs_getmygid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2246 |  |
|      - | 2247 | `	ph7_vfs *pVfs;` |
|      - | 2248 | `	int nGid;` |
|      - | 2249 | `	/* Point to the underlying vfs */` |
|      2 | 2250 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      2 | 2251 | `	if( pVfs == 0 \|\| pVfs->xGid == 0 ){` |
|    ! 0 | 2252 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2253 | `		SXUNUSED(apArg);` |
|      - | 2254 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2255 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2256 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2257 | `			ph7_function_name(pCtx)` |
|      - | 2258 | `			);` |
|    ! 0 | 2259 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2260 | `		return PH7_OK;` |
|      - | 2261 | `	}` |
|      - | 2262 | `	/* Perform the requested operation */` |
|      2 | 2263 | `	nGid = pVfs->xGid();` |
|      - | 2264 | `	/* Set the result */` |
|      2 | 2265 | `	ph7_result_int(pCtx,nGid);` |
|      2 | 2266 | `	return PH7_OK;` |
|      1 | 2267 |  |
|      - | 2268 | `#ifdef __WINNT__` |
|      - | 2269 | `#include <Windows.h>` |
|      - | 2270 | `#elif defined(__UNIXES__)` |
|      - | 2271 | `#include <sys/utsname.h>` |
|      - | 2272 | `#endif` |
|      - | 2273 | `/*` |
|      - | 2274 | ` * string php_uname([ string $mode = "a" ])` |
|      - | 2275 | ` *  Returns information about the host operating system.` |
|      - | 2276 | ` * Parameters` |
|      - | 2277 | ` *  $mode` |
|      - | 2278 | ` *   mode is a single character that defines what information is returned:` |
|      - | 2279 | ` *    'a': This is the default. Contains all modes in the sequence "s n r v m".` |
|      - | 2280 | ` *    's': Operating system name. eg. FreeBSD.` |
|      - | 2281 | ` *    'n': Host name. eg. localhost.example.com.` |
|      - | 2282 | ` *    'r': Release name. eg. 5.1.2-RELEASE.` |
|      - | 2283 | ` *    'v': Version information. Varies a lot between operating systems.` |
|      - | 2284 | ` *    'm': Machine type. eg. i386.` |
|      - | 2285 | ` * Return` |
|      - | 2286 | ` *  OS description as a string.` |
|      - | 2287 | ` */` |
|      4 | 2288 | `static int PH7_vfs_ph7_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2289 |  |
|      - | 2290 | `#if defined(__WINNT__)` |
|      1 | 2291 | `	const char *zName = "Microsoft Windows";` |
|      - | 2292 | `	OSVERSIONINFOW sVer;` |
|      - | 2293 | `#elif defined(__UNIXES__)` |
|      - | 2294 | `	struct utsname sName;` |
|      - | 2295 | `#endif` |
|      5 | 2296 | `	const char *zMode = "a";` |
|      5 | 2297 | `	if( nArg > 0 && ph7_value_is_string(apArg[0]) ){` |
|      - | 2298 | `		/* Extract the desired mode */` |
|    ! 0 | 2299 | `		zMode = ph7_value_to_string(apArg[0],0);` |
|    ! 0 | 2300 | `	}` |
|      - | 2301 | `#if defined(__WINNT__)` |
|      1 | 2302 | `	sVer.dwOSVersionInfoSize = sizeof(sVer);` |
|      - | 2303 | `	/* GetVersionExW is deprecated in modern MSVC. Suppress deprecation for this call. */` |
|      - | 2304 | `#if defined(_MSC_VER)` |
|      - | 2305 | `#pragma warning(push)` |
|      - | 2306 | `#pragma warning(disable:4996)` |
|      - | 2307 | `#endif` |
|      1 | 2308 | `	if( TRUE != GetVersionExW(&sVer)){` |
|      - | 2309 | `#if defined(_MSC_VER)` |
|      - | 2310 | `#pragma warning(pop)` |
|      - | 2311 | `#endif` |
|    ! 0 | 2312 | `		ph7_result_string(pCtx,zName,-1);` |
|    ! 0 | 2313 | `		return PH7_OK;` |
|      - | 2314 | `	}` |
|      1 | 2315 | `	if( sVer.dwPlatformId == VER_PLATFORM_WIN32_NT ){` |
|      1 | 2316 | `		if( sVer.dwMajorVersion <= 4 ){` |
|    ! 0 | 2317 | `			zName = "Microsoft Windows NT";` |
|      1 | 2318 | `		}else if( sVer.dwMajorVersion == 5 ){` |
|    ! 0 | 2319 | `			switch(sVer.dwMinorVersion){` |
|    ! 0 | 2320 | `				case 0:	zName = "Microsoft Windows 2000"; break;` |
|    ! 0 | 2321 | `				case 1: zName = "Microsoft Windows XP";   break;` |
|    ! 0 | 2322 | `				case 2: zName = "Microsoft Windows Server 2003"; break;` |
|      - | 2323 | `			}` |
|    ! 0 | 2324 | `		}else if( sVer.dwMajorVersion == 6){` |
|      1 | 2325 | `				switch(sVer.dwMinorVersion){` |
|    ! 0 | 2326 | `					case 0: zName = "Microsoft Windows Vista"; break;` |
|    ! 0 | 2327 | `					case 1: zName = "Microsoft Windows 7"; break;` |
|      1 | 2328 | `					case 2: zName = "Microsoft Windows Server 2008"; break;` |
|    ! 0 | 2329 | `					case 3: zName = "Microsoft Windows 8"; break;` |
|      - | 2330 | `					default: break;` |
|      - | 2331 | `				}` |
|      - | 2332 | `		}` |
|      - | 2333 | `	}` |
|      1 | 2334 | `	switch(zMode[0]){` |
|      - | 2335 | `	case 's':` |
|      - | 2336 | `		/* Operating system name */` |
|    ! 0 | 2337 | `		ph7_result_string(pCtx,zName,-1/* Compute length automatically*/);` |
|    ! 0 | 2338 | `		break;` |
|      - | 2339 | `	case 'n':` |
|      - | 2340 | `		/* Host name */` |
|    ! 0 | 2341 | `		ph7_result_string(pCtx,"localhost",(int)sizeof("localhost")-1);` |
|    ! 0 | 2342 | `		break;` |
|      - | 2343 | `	case 'r':` |
|      - | 2344 | `	case 'v':` |
|      - | 2345 | `		/* Version information. */` |
|    ! 0 | 2346 | `		ph7_result_string_format(pCtx,"%u.%u build %u",` |
|      - | 2347 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2348 | `			);` |
|    ! 0 | 2349 | `		break;` |
|      - | 2350 | `	case 'm':` |
|      - | 2351 | `		/* Machine name */` |
|    ! 0 | 2352 | `		ph7_result_string(pCtx,"x86",(int)sizeof("x86")-1);` |
|    ! 0 | 2353 | `		break;` |
|      - | 2354 | `	default:` |
|      1 | 2355 | `		ph7_result_string_format(pCtx,"%s localhost %u.%u build %u x86",` |
|      - | 2356 | `			zName,` |
|      - | 2357 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2358 | `			);` |
|      - | 2359 | `		break;` |
|      - | 2360 | `	}` |
|      - | 2361 | `#elif defined(__UNIXES__)` |
|      4 | 2362 | `	if( uname(&sName) != 0 ){` |
|    ! 0 | 2363 | `		ph7_result_string(pCtx,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 | 2364 | `		return PH7_OK;` |
|      - | 2365 | `	}` |
|      4 | 2366 | `	switch(zMode[0]){` |
|    ! 0 | 2367 | `	case 's':` |
|      - | 2368 | `		/* Operating system name */` |
|    ! 0 | 2369 | `		ph7_result_string(pCtx,sName.sysname,-1/* Compute length automatically*/);` |
|    ! 0 | 2370 | `		break;` |
|    ! 0 | 2371 | `	case 'n':` |
|      - | 2372 | `		/* Host name */` |
|    ! 0 | 2373 | `		ph7_result_string(pCtx,sName.nodename,-1/* Compute length automatically*/);` |
|    ! 0 | 2374 | `		break;` |
|    ! 0 | 2375 | `	case 'r':` |
|      - | 2376 | `		/* Release information */` |
|    ! 0 | 2377 | `		ph7_result_string(pCtx,sName.release,-1/* Compute length automatically*/);` |
|    ! 0 | 2378 | `		break;` |
|    ! 0 | 2379 | `	case 'v':` |
|      - | 2380 | `		/* Version information. */` |
|    ! 0 | 2381 | `		ph7_result_string(pCtx,sName.version,-1/* Compute length automatically*/);` |
|    ! 0 | 2382 | `		break;` |
|    ! 0 | 2383 | `	case 'm':` |
|      - | 2384 | `		/* Machine name */` |
|    ! 0 | 2385 | `		ph7_result_string(pCtx,sName.machine,-1/* Compute length automatically*/);` |
|    ! 0 | 2386 | `		break;` |
|      2 | 2387 | `	default:` |
|      6 | 2388 | `		ph7_result_string_format(pCtx,` |
|      - | 2389 | `			"%s %s %s %s %s",` |
|      2 | 2390 | `			sName.sysname,` |
|      2 | 2391 | `			sName.release,` |
|      2 | 2392 | `			sName.version,` |
|      2 | 2393 | `			sName.nodename,` |
|      2 | 2394 | `			sName.machine` |
|      - | 2395 | `			);` |
|      4 | 2396 | `		break;` |
|      - | 2397 | `	}` |
|      - | 2398 | `#else` |
|      - | 2399 | `	ph7_result_string(pCtx,"Unknown Operating System",(int)sizeof("Unknown Operating System")-1);` |
|      - | 2400 | `#endif` |
|      5 | 2401 | `	return PH7_OK;` |
|      3 | 2402 |  |
|      - | 2403 | `/*` |
|      - | 2404 | ` * Section:` |
|      - | 2405 | ` *    IO stream implementation.` |
|      - | 2406 | ` * Status:` |
|      - | 2407 | ` *    Stable.` |
|      - | 2408 | ` */` |
|      - | 2409 | `typedef struct io_private io_private;` |
|      - | 2410 | `struct io_private` |
|      - | 2411 |  |
|      - | 2412 | `	const ph7_io_stream *pStream; /* Underlying IO device */` |
|      - | 2413 | `	void *pHandle; /* IO handle */` |
|      - | 2414 | `	/* Unbuffered IO */` |
|      - | 2415 | `	SyBlob sBuffer; /* Working buffer */` |
|      - | 2416 | `	sxu32 nOfft;    /* Current read offset */` |
|      - | 2417 | `	sxu32 iMagic;   /* Sanity check to avoid misuse */` |
|      - | 2418 | `};` |
|      - | 2419 | `#define IO_PRIVATE_MAGIC 0xFEAC14` |
|      - | 2420 | `/* Make sure we are dealing with a valid io_private instance */` |
|      - | 2421 | `#define IO_PRIVATE_INVALID(IO) ( IO == 0 \|\| IO->iMagic != IO_PRIVATE_MAGIC )` |
|      - | 2422 | `/* Forward declaration */` |
|      - | 2423 | `static void ResetIOPrivate(io_private *pDev);` |
|      - | 2424 | `/*` |
|      - | 2425 | ` * bool ftruncate(resource $handle,int64 $size)` |
|      - | 2426 | ` *  Truncates a file to a given length.` |
|      - | 2427 | ` * Parameters` |
|      - | 2428 | ` *  $handle` |
|      - | 2429 | ` *   The file pointer.` |
|      - | 2430 | ` *   Note:` |
|      - | 2431 | ` *    The handle must be open for writing.` |
|      - | 2432 | ` * $size` |
|      - | 2433 | ` *   The size to truncate to.` |
|      - | 2434 | ` * Return` |
|      - | 2435 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2436 | ` */` |
|      6 | 2437 | `static int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2438 |  |
|      - | 2439 | `	const ph7_io_stream *pStream;` |
|      - | 2440 | `	io_private *pDev;` |
|      - | 2441 | `	int rc;` |
|      7 | 2442 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2443 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2444 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2445 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2446 | `		return PH7_OK;` |
|      - | 2447 | `	}` |
|      - | 2448 | `	/* Extract our private data */` |
|      7 | 2449 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2450 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 2451 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2452 | `		/*Expecting an IO handle */` |
|    ! 0 | 2453 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2454 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2455 | `		return PH7_OK;` |
|      - | 2456 | `	}` |
|      - | 2457 | `	/* Point to the target IO stream device */` |
|      7 | 2458 | `	pStream = pDev->pStream;` |
|      7 | 2459 | `	if( pStream == 0  \|\| pStream->xTrunc == 0){` |
|    ! 0 | 2460 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2461 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2462 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2463 | `			);` |
|    ! 0 | 2464 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2465 | `		return PH7_OK;` |
|      - | 2466 | `	}` |
|      - | 2467 | `	/* Perform the requested operation */` |
|      7 | 2468 | `	rc = pStream->xTrunc(pDev->pHandle,ph7_value_to_int64(apArg[1]));` |
|      7 | 2469 | `	if( rc == PH7_OK ){` |
|      - | 2470 | `		/* Discard buffered data */` |
|      7 | 2471 | `		ResetIOPrivate(pDev);` |
|      3 | 2472 | `	}` |
|      - | 2473 | `	/* IO result */` |
|      7 | 2474 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      7 | 2475 | `	return PH7_OK;` |
|      4 | 2476 |  |
|      - | 2477 | `/*` |
|      - | 2478 | ` * int fseek(resource $handle,int $offset[,int $whence = SEEK_SET ])` |
|      - | 2479 | ` *  Seeks on a file pointer.` |
|      - | 2480 | ` * Parameters` |
|      - | 2481 | ` *  $handle` |
|      - | 2482 | ` *   A file system pointer resource that is typically created using fopen().` |
|      - | 2483 | ` * $offset` |
|      - | 2484 | ` *   The offset.` |
|      - | 2485 | ` *   To move to a position before the end-of-file, you need to pass a negative` |
|      - | 2486 | ` *   value in offset and set whence to SEEK_END.` |
|      - | 2487 | ` *   whence` |
|      - | 2488 | ` *   whence values are:` |
|      - | 2489 | ` *    SEEK_SET - Set position equal to offset bytes.` |
|      - | 2490 | ` *    SEEK_CUR - Set position to current location plus offset.` |
|      - | 2491 | ` *    SEEK_END - Set position to end-of-file plus offset.` |
|      - | 2492 | ` * Return` |
|      - | 2493 | ` *  0 on success,-1 on failure` |
|      - | 2494 | ` */` |
|      2 | 2495 | `static int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2496 |  |
|      - | 2497 | `	const ph7_io_stream *pStream;` |
|      - | 2498 | `	io_private *pDev;` |
|      - | 2499 | `	ph7_int64 iOfft;` |
|      - | 2500 | `	int whence;` |
|      - | 2501 | `	int rc;` |
|      3 | 2502 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2503 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2504 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2505 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2506 | `		return PH7_OK;` |
|      - | 2507 | `	}` |
|      - | 2508 | `	/* Extract our private data */` |
|      3 | 2509 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2510 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 2511 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2512 | `		/*Expecting an IO handle */` |
|    ! 0 | 2513 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2514 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2515 | `		return PH7_OK;` |
|      - | 2516 | `	}` |
|      - | 2517 | `	/* Point to the target IO stream device */` |
|      3 | 2518 | `	pStream = pDev->pStream;` |
|      3 | 2519 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2520 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2521 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2522 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2523 | `			);` |
|    ! 0 | 2524 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2525 | `		return PH7_OK;` |
|      - | 2526 | `	}` |
|      - | 2527 | `	/* Extract the offset */` |
|      3 | 2528 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|      3 | 2529 | `	whence = 0;/* SEEK_SET */` |
|      3 | 2530 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|    ! 0 | 2531 | `		whence = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2532 | `	}` |
|      - | 2533 | `	/* Perform the requested operation */` |
|      3 | 2534 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|      3 | 2535 | `	if( rc == PH7_OK ){` |
|      - | 2536 | `		/* Ignore buffered data */` |
|      3 | 2537 | `		ResetIOPrivate(pDev);` |
|      1 | 2538 | `	}` |
|      - | 2539 | `	/* IO result */` |
|      3 | 2540 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|      3 | 2541 | `	return PH7_OK;` |
|      2 | 2542 |  |
|      - | 2543 | `/*` |
|      - | 2544 | ` * int64 ftell(resource $handle)` |
|      - | 2545 | ` *  Returns the current position of the file read/write pointer.` |
|      - | 2546 | ` * Parameters` |
|      - | 2547 | ` *  $handle` |
|      - | 2548 | ` *   The file pointer.` |
|      - | 2549 | ` * Return` |
|      - | 2550 | ` *  Returns the position of the file pointer referenced by handle` |
|      - | 2551 | ` *  as an integer; i.e., its offset into the file stream.` |
|      - | 2552 | ` *  FALSE is returned on failure.` |
|      - | 2553 | ` */` |
|      6 | 2554 | `static int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2555 |  |
|      - | 2556 | `	const ph7_io_stream *pStream;` |
|      - | 2557 | `	io_private *pDev;` |
|      - | 2558 | `	ph7_int64 iOfft;` |
|      7 | 2559 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2560 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2561 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2562 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2563 | `		return PH7_OK;` |
|      - | 2564 | `	}` |
|      - | 2565 | `	/* Extract our private data */` |
|      7 | 2566 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2567 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 2568 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2569 | `		/*Expecting an IO handle */` |
|    ! 0 | 2570 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2571 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2572 | `		return PH7_OK;` |
|      - | 2573 | `	}` |
|      - | 2574 | `	/* Point to the target IO stream device */` |
|      7 | 2575 | `	pStream = pDev->pStream;` |
|      7 | 2576 | `	if( pStream == 0  \|\| pStream->xTell == 0){` |
|    ! 0 | 2577 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2578 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2579 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2580 | `			);` |
|    ! 0 | 2581 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2582 | `		return PH7_OK;` |
|      - | 2583 | `	}` |
|      - | 2584 | `	/* Perform the requested operation */` |
|      7 | 2585 | `	iOfft = pStream->xTell(pDev->pHandle);` |
|      - | 2586 | `	/* IO result */` |
|      7 | 2587 | `	ph7_result_int64(pCtx,iOfft);` |
|      7 | 2588 | `	return PH7_OK;` |
|      4 | 2589 |  |
|      - | 2590 | `/*` |
|      - | 2591 | ` * bool rewind(resource $handle)` |
|      - | 2592 | ` *  Rewind the position of a file pointer.` |
|      - | 2593 | ` * Parameters` |
|      - | 2594 | ` *  $handle` |
|      - | 2595 | ` *   The file pointer.` |
|      - | 2596 | ` * Return` |
|      - | 2597 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2598 | ` */` |
|      4 | 2599 | `static int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2600 |  |
|      - | 2601 | `	const ph7_io_stream *pStream;` |
|      - | 2602 | `	io_private *pDev;` |
|      - | 2603 | `	int rc;` |
|      5 | 2604 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2605 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2606 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2607 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2608 | `		return PH7_OK;` |
|      - | 2609 | `	}` |
|      - | 2610 | `	/* Extract our private data */` |
|      5 | 2611 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2612 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 2613 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2614 | `		/*Expecting an IO handle */` |
|    ! 0 | 2615 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2616 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2617 | `		return PH7_OK;` |
|      - | 2618 | `	}` |
|      - | 2619 | `	/* Point to the target IO stream device */` |
|      5 | 2620 | `	pStream = pDev->pStream;` |
|      5 | 2621 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2622 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2623 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2624 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2625 | `			);` |
|    ! 0 | 2626 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2627 | `		return PH7_OK;` |
|      - | 2628 | `	}` |
|      - | 2629 | `	/* Perform the requested operation */` |
|      5 | 2630 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|      5 | 2631 | `	if( rc == PH7_OK ){` |
|      - | 2632 | `		/* Ignore buffered data */` |
|      5 | 2633 | `		ResetIOPrivate(pDev);` |
|      2 | 2634 | `	}` |
|      - | 2635 | `	/* IO result */` |
|      5 | 2636 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 2637 | `	return PH7_OK;` |
|      3 | 2638 |  |
|      - | 2639 | `/*` |
|      - | 2640 | ` * bool fflush(resource $handle)` |
|      - | 2641 | ` *  Flushes the output to a file.` |
|      - | 2642 | ` * Parameters` |
|      - | 2643 | ` *  $handle` |
|      - | 2644 | ` *   The file pointer.` |
|      - | 2645 | ` * Return` |
|      - | 2646 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2647 | ` */` |
|      2 | 2648 | `static int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2649 |  |
|      - | 2650 | `	const ph7_io_stream *pStream;` |
|      - | 2651 | `	io_private *pDev;` |
|      - | 2652 | `	int rc;` |
|      3 | 2653 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2654 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2655 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2656 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2657 | `		return PH7_OK;` |
|      - | 2658 | `	}` |
|      - | 2659 | `	/* Extract our private data */` |
|      3 | 2660 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2661 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 2662 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2663 | `		/*Expecting an IO handle */` |
|    ! 0 | 2664 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2665 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2666 | `		return PH7_OK;` |
|      - | 2667 | `	}` |
|      - | 2668 | `	/* Point to the target IO stream device */` |
|      3 | 2669 | `	pStream = pDev->pStream;` |
|      3 | 2670 | `	if( pStream == 0 \|\| pStream->xSync == 0){` |
|    ! 0 | 2671 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2672 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2673 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2674 | `			);` |
|    ! 0 | 2675 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2676 | `		return PH7_OK;` |
|      - | 2677 | `	}` |
|      - | 2678 | `	/* Perform the requested operation */` |
|      3 | 2679 | `	rc = pStream->xSync(pDev->pHandle);` |
|      - | 2680 | `	/* IO result */` |
|      3 | 2681 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 2682 | `	return PH7_OK;` |
|      2 | 2683 |  |
|      - | 2684 | `/*` |
|      - | 2685 | ` * bool feof(resource $handle)` |
|      - | 2686 | ` *  Tests for end-of-file on a file pointer.` |
|      - | 2687 | ` * Parameters` |
|      - | 2688 | ` *  $handle` |
|      - | 2689 | ` *   The file pointer.` |
|      - | 2690 | ` * Return` |
|      - | 2691 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|      - | 2692 | ` */` |
|     68 | 2693 | `static int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2694 |  |
|      - | 2695 | `	const ph7_io_stream *pStream;` |
|      - | 2696 | `	io_private *pDev;` |
|      - | 2697 | `	int rc;` |
|     69 | 2698 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2699 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 2700 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2701 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2702 | `		return PH7_OK;` |
|      - | 2703 | `	}` |
|      - | 2704 | `	/* Extract our private data */` |
|     69 | 2705 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2706 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     69 | 2707 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2708 | `		/*Expecting an IO handle */` |
|    ! 0 | 2709 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2710 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2711 | `		return PH7_OK;` |
|      - | 2712 | `	}` |
|      - | 2713 | `	/* Point to the target IO stream device */` |
|     69 | 2714 | `	pStream = pDev->pStream;` |
|     69 | 2715 | `	if( pStream == 0 ){` |
|    ! 0 | 2716 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2717 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2718 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2719 | `			);` |
|    ! 0 | 2720 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2721 | `		return PH7_OK;` |
|      - | 2722 | `	}` |
|     69 | 2723 | `	rc = SXERR_EOF;` |
|      - | 2724 | `	/* Perform the requested operation */` |
|     69 | 2725 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2726 | `		/* Data is available */` |
|     33 | 2727 | `		rc = PH7_OK;` |
|     17 | 2728 | `	}else{` |
|      - | 2729 | `		char zBuf[4096];` |
|      - | 2730 | `		ph7_int64 n;` |
|      - | 2731 | `		/* Perform a buffered read */` |
|     37 | 2732 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|     37 | 2733 | `		if( n > 0 ){` |
|      - | 2734 | `			/* Copy buffered data */` |
|     17 | 2735 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|     17 | 2736 | `			rc = PH7_OK;` |
|      8 | 2737 | `		}` |
|      - | 2738 | `	}` |
|      - | 2739 | `	/* EOF or not */` |
|     69 | 2740 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|     69 | 2741 | `	return PH7_OK;` |
|     35 | 2742 |  |
|      - | 2743 | `/*` |
|      - | 2744 | ` * Read n bytes from the underlying IO stream device.` |
|      - | 2745 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|      - | 2746 | ` * [i.e: IO error ] or EOF.` |
|      - | 2747 | ` */` |
|     18 | 2748 | `static ph7_int64 StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      1 | 2749 |  |
|     19 | 2750 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|     19 | 2751 | `	char *zBuf = (char *)pBuf;` |
|      - | 2752 | `	ph7_int64 n,nRead;` |
|     19 | 2753 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     19 | 2754 | `	if( n > 0 ){` |
|    ! 0 | 2755 | `		if( n > nLen ){` |
|    ! 0 | 2756 | `			n = nLen;` |
|    ! 0 | 2757 | `		}` |
|      - | 2758 | `		/* Copy the buffered data */` |
|    ! 0 | 2759 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|      - | 2760 | `		/* Update the read offset */` |
|    ! 0 | 2761 | `		pDev->nOfft += (sxu32)n;` |
|    ! 0 | 2762 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2763 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|    ! 0 | 2764 | `			SyBlobReset(&pDev->sBuffer);` |
|    ! 0 | 2765 | `			pDev->nOfft = 0;` |
|    ! 0 | 2766 | `		}` |
|    ! 0 | 2767 | `		nLen -= n;` |
|    ! 0 | 2768 | `		if( nLen < 1 ){` |
|      - | 2769 | `			/* All done */` |
|    ! 0 | 2770 | `			return n;` |
|      - | 2771 | `		}` |
|      - | 2772 | `		/* Advance the cursor */` |
|    ! 0 | 2773 | `		zBuf += n;` |
|    ! 0 | 2774 | `	}` |
|      - | 2775 | `	/* Read without buffering */` |
|     19 | 2776 | `	nRead = pStream->xRead(pDev->pHandle,zBuf,nLen);` |
|     19 | 2777 | `	if( nRead > 0 ){` |
|     17 | 2778 | `		n += nRead;` |
|     11 | 2779 | `	}else if( n < 1 ){` |
|      - | 2780 | `		/* EOF or IO error */` |
|      3 | 2781 | `		return nRead;` |
|      - | 2782 | `	}` |
|     17 | 2783 | `	return n;` |
|     10 | 2784 |  |
|      - | 2785 | `/*` |
|      - | 2786 | ` * Extract a single line from the buffered input.` |
|      - | 2787 | ` */` |
|     64 | 2788 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      1 | 2789 |  |
|      - | 2790 | `	const char *zIn,*zEnd,*zPtr;` |
|     65 | 2791 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|     65 | 2792 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|     65 | 2793 | `	zPtr = zIn;` |
|   1601 | 2794 | `	while( zIn < zEnd ){` |
|   1597 | 2795 | `		if( zIn[0] == '\n' ){` |
|      - | 2796 | `			/* Line found */` |
|     61 | 2797 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|     61 | 2798 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|     61 | 2799 | `			*pzLine = zPtr;` |
|     61 | 2800 | `			return SXRET_OK;` |
|      - | 2801 | `		}` |
|   1537 | 2802 | `		zIn++;` |
|      1 | 2803 | `	}` |
|      - | 2804 | `	/* No line were found */` |
|      5 | 2805 | `	return SXERR_NOTFOUND;` |
|     33 | 2806 |  |
|      - | 2807 | `/*` |
|      - | 2808 | ` * Read a single line from the underlying IO stream device.` |
|      - | 2809 | ` */` |
|     68 | 2810 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      1 | 2811 |  |
|     69 | 2812 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - | 2813 | `	char zBuf[8192];` |
|      - | 2814 | `	ph7_int64 n;` |
|      - | 2815 | `	sxi32 rc;` |
|     69 | 2816 | `	n = 0;` |
|     69 | 2817 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2818 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     17 | 2819 | `		SyBlobReset(&pDev->sBuffer);` |
|     17 | 2820 | `		pDev->nOfft = 0;` |
|      8 | 2821 | `	}` |
|     69 | 2822 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2823 | `		/* Check if there is a line */` |
|     53 | 2824 | `		rc = GetLine(pDev,&n,pzData);` |
|     53 | 2825 | `		if( rc == SXRET_OK ){` |
|      - | 2826 | `			/* Got line,update the cursor  */` |
|     51 | 2827 | `			pDev->nOfft += (sxu32)n;` |
|     51 | 2828 | `			return n;` |
|      - | 2829 | `		}` |
|      1 | 2830 | `	}` |
|      - | 2831 | `	/* Perform the read operation until a new line is extracted or length` |
|      - | 2832 | `	 * limit is reached.` |
|      - | 2833 | `	 */` |
|     10 | 2834 | `	for(;;){` |
|     21 | 2835 | `		n = pStream->xRead(pDev->pHandle,zBuf, (nMaxLen > 0 && nMaxLen < (ph7_int64)sizeof(zBuf)) ? nMaxLen : (ph7_int64)sizeof(zBuf));` |
|     21 | 2836 | `		if( n < 1 ){` |
|      - | 2837 | `			/* EOF or IO error */` |
|      9 | 2838 | `			break;` |
|      - | 2839 | `		}` |
|      - | 2840 | `		/* Append the data just read */` |
|     13 | 2841 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - | 2842 | `		/* Try to extract a line */` |
|     13 | 2843 | `		rc = GetLine(pDev,&n,pzData);` |
|     13 | 2844 | `		if( rc == SXRET_OK ){` |
|      - | 2845 | `			/* Got one,return immediately */` |
|     11 | 2846 | `			pDev->nOfft += (sxu32)n;` |
|     11 | 2847 | `			return n;` |
|      - | 2848 | `		}` |
|      3 | 2849 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
|      - | 2850 | `			/* Read limit reached,return the available data */` |
|    ! 0 | 2851 | `			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    ! 0 | 2852 | `			n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 2853 | `			/* Reset the working buffer */` |
|    ! 0 | 2854 | `			SyBlobReset(&pDev->sBuffer);` |
|    ! 0 | 2855 | `			pDev->nOfft = 0;` |
|    ! 0 | 2856 | `			return n;` |
|      - | 2857 | `		}` |
|      1 | 2858 | `	}` |
|      9 | 2859 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2860 | `		/* Read limit reached,return the available data */` |
|      5 | 2861 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|      5 | 2862 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 2863 | `		/* Reset the working buffer */` |
|      5 | 2864 | `		SyBlobReset(&pDev->sBuffer);` |
|      5 | 2865 | `		pDev->nOfft = 0;` |
|      2 | 2866 | `	}` |
|      9 | 2867 | `	return n;` |
|     35 | 2868 |  |
|      - | 2869 | `/*` |
|      - | 2870 | ` * Open an IO stream handle.` |
|      - | 2871 | ` * Notes on stream:` |
|      - | 2872 | ` * According to the PHP reference manual.` |
|      - | 2873 | ` * In its simplest definition, a stream is a resource object which exhibits streamable behavior.` |
|      - | 2874 | ` * That is, it can be read from or written to in a linear fashion, and may be able to fseek()` |
|      - | 2875 | ` * to an arbitrary locations within the stream.` |
|      - | 2876 | ` * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.` |
|      - | 2877 | ` * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file` |
|      - | 2878 | ` * on a remote server.` |
|      - | 2879 | ` * A stream is referenced as: scheme://target` |
|      - | 2880 | ` *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...` |
|      - | 2881 | ` *   If no wrapper is specified, the function default is used (typically file://).` |
|      - | 2882 | ` *   target - Depends on the wrapper used. For filesystem related streams this is typically a path` |
|      - | 2883 | ` *  and filename of the desired file. For network related streams this is typically a hostname, often` |
|      - | 2884 | ` *  with a path appended.` |
|      - | 2885 | ` *` |
|      - | 2886 | ` * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.` |
|      - | 2887 | ` * Please refer to the official documentation for a full discussion.` |
|      - | 2888 | ` * This function return a handle on success. Otherwise null.` |
|      - | 2889 | ` */` |
|    284 | 2890 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - | 2891 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      1 | 2892 |  |
|    285 | 2893 | `	void *pHandle = 0; /* cc warning */` |
|      - | 2894 | `	SyString sFile;` |
|      - | 2895 | `	int rc;` |
|    285 | 2896 | `	if( pStream == 0 ){` |
|      - | 2897 | `		/* No such stream device */` |
|    ! 0 | 2898 | `		return 0;` |
|      - | 2899 | `	}` |
|    285 | 2900 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|    285 | 2901 | `	if( use_include ){` |
|     20 | 2902 | `		if(	sFile.zString[0] == '/' \|\|` |
|      - | 2903 | `#ifdef __WINNT__` |
|      - | 2904 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - | 2905 | `#endif` |
|      9 | 2906 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|      8 | 2907 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - | 2908 | `				/*  Open the file directly */` |
|     13 | 2909 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      7 | 2910 | `		}else{` |
|      - | 2911 | `			SyString *pPath;` |
|      - | 2912 | `			SyBlob sWorker;` |
|      - | 2913 | `#ifdef __WINNT__` |
|      - | 2914 | `			static const int c = '\\';` |
|      - | 2915 | `#else` |
|      - | 2916 | `			static const int c = '/';` |
|      - | 2917 | `#endif` |
|      - | 2918 | `			/* Init the path builder working buffer */` |
|      9 | 2919 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - | 2920 | `			/* Build a path from the set of include path */` |
|      9 | 2921 | `			SySetResetCursor(&pVm->aPaths);` |
|      9 | 2922 | `			rc = SXERR_IO;` |
|     11 | 2923 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - | 2924 | `				/* Build full path */` |
|      9 | 2925 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - | 2926 | `				/* Append null terminator */` |
|      9 | 2927 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 2928 | `					continue;` |
|      - | 2929 | `				}` |
|      - | 2930 | `				/* Try to open the file */` |
|      9 | 2931 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|      9 | 2932 | `				if( rc == PH7_OK ){` |
|      7 | 2933 | `					if( bPushInclude ){` |
|      - | 2934 | `						/* Mark as included */` |
|      7 | 2935 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|      3 | 2936 | `					}` |
|      7 | 2937 | `					break;` |
|      - | 2938 | `				}` |
|      - | 2939 | `				/* Reset the working buffer */` |
|      3 | 2940 | `				SyBlobReset(&sWorker);` |
|      - | 2941 | `				/* Check the next path */` |
|      1 | 2942 | `			}` |
|      9 | 2943 | `			SyBlobRelease(&sWorker);` |
|      - | 2944 | `		}` |
|     21 | 2945 | `		if( rc == PH7_OK ){` |
|     19 | 2946 | `			if( bPushInclude ){` |
|      - | 2947 | `				/* Mark as included */` |
|     19 | 2948 | `				PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|      9 | 2949 | `			}` |
|      9 | 2950 | `		}` |
|     11 | 2951 | `	}else{` |
|      - | 2952 | `		/* Open the URI direcly */` |
|    265 | 2953 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - | 2954 | `	}` |
|    285 | 2955 | `	if( rc != PH7_OK ){` |
|      - | 2956 | `		/* IO error */` |
|      9 | 2957 | `		return 0;` |
|      - | 2958 | `	}` |
|      - | 2959 | `	/* Return the file handle */` |
|    277 | 2960 | `	return pHandle;` |
|    143 | 2961 |  |
|      - | 2962 | `/*` |
|      - | 2963 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - | 2964 | ` * Store the read data in the given BLOB (last argument).` |
|      - | 2965 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - | 2966 | ` */` |
|     16 | 2967 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      1 | 2968 |  |
|      - | 2969 | `	ph7_int64 nRead;` |
|      - | 2970 | `	char zBuf[8192]; /* 8K */` |
|      - | 2971 | `	int rc;` |
|      - | 2972 | `	/* Perform the requested operation */` |
|     16 | 2973 | `	for(;;){` |
|     33 | 2974 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|     33 | 2975 | `		if( nRead < 1 ){` |
|      - | 2976 | `			/* EOF or IO error */` |
|     17 | 2977 | `			break;` |
|      - | 2978 | `		}` |
|      - | 2979 | `		/* Append contents */` |
|     17 | 2980 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|     17 | 2981 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 2982 | `			break;` |
|      - | 2983 | `		}` |
|      1 | 2984 | `	}` |
|     17 | 2985 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      1 | 2986 |  |
|      - | 2987 | `/*` |
|      - | 2988 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - | 2989 | ` */` |
|    288 | 2990 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      1 | 2991 |  |
|    289 | 2992 | `	if( pStream->xClose ){` |
|    289 | 2993 | `		pStream->xClose(pHandle);` |
|    144 | 2994 | `	}` |
|    289 | 2995 |  |
|      - | 2996 | `/*` |
|      - | 2997 | ` * string fgetc(resource $handle)` |
|      - | 2998 | ` *  Gets a character from the given file pointer.` |
|      - | 2999 | ` * Parameters` |
|      - | 3000 | ` *  $handle` |
|      - | 3001 | ` *   The file pointer.` |
|      - | 3002 | ` * Return` |
|      - | 3003 | ` *  Returns a string containing a single character read from the file` |
|      - | 3004 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|      - | 3005 | ` * WARNING` |
|      - | 3006 | ` *  This operation is extremely slow.Avoid using it.` |
|      - | 3007 | ` */` |
|      4 | 3008 | `static int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3009 |  |
|      - | 3010 | `	const ph7_io_stream *pStream;` |
|      - | 3011 | `	io_private *pDev;` |
|      - | 3012 | `	int c,n;` |
|      5 | 3013 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3014 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3015 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3016 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3017 | `		return PH7_OK;` |
|      - | 3018 | `	}` |
|      - | 3019 | `	/* Extract our private data */` |
|      5 | 3020 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3021 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 3022 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3023 | `		/*Expecting an IO handle */` |
|    ! 0 | 3024 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3025 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3026 | `		return PH7_OK;` |
|      - | 3027 | `	}` |
|      - | 3028 | `	/* Point to the target IO stream device */` |
|      5 | 3029 | `	pStream = pDev->pStream;` |
|      5 | 3030 | `	if( pStream == 0  ){` |
|    ! 0 | 3031 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3032 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3033 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3034 | `			);` |
|    ! 0 | 3035 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3036 | `		return PH7_OK;` |
|      - | 3037 | `	}` |
|      - | 3038 | `	/* Perform the requested operation */` |
|      5 | 3039 | `	n = (int)StreamRead(pDev,(void *)&c,sizeof(char));` |
|      - | 3040 | `	/* IO result */` |
|      5 | 3041 | `	if( n < 1 ){` |
|      - | 3042 | `		/* EOF or error,return FALSE */` |
|    ! 0 | 3043 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3044 | `	}else{` |
|      - | 3045 | `		/* Return the string holding the character */` |
|      5 | 3046 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|      - | 3047 | `	}` |
|      5 | 3048 | `	return PH7_OK;` |
|      3 | 3049 |  |
|      - | 3050 | `/*` |
|      - | 3051 | ` * string fgets(resource $handle[,int64 $length ])` |
|      - | 3052 | ` *  Gets line from file pointer.` |
|      - | 3053 | ` * Parameters` |
|      - | 3054 | ` *  $handle` |
|      - | 3055 | ` *   The file pointer.` |
|      - | 3056 | ` * $length` |
|      - | 3057 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3058 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3059 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3060 | ` *  the end of the line.` |
|      - | 3061 | ` * Return` |
|      - | 3062 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3063 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3064 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3065 | ` */` |
|     58 | 3066 | `static int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3067 |  |
|      - | 3068 | `	const ph7_io_stream *pStream;` |
|      - | 3069 | `	const char *zLine;` |
|      - | 3070 | `	io_private *pDev;` |
|      - | 3071 | `	ph7_int64 n,nLen;` |
|     59 | 3072 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3073 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3074 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3075 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3076 | `		return PH7_OK;` |
|      - | 3077 | `	}` |
|      - | 3078 | `	/* Extract our private data */` |
|     59 | 3079 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3080 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     59 | 3081 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3082 | `		/*Expecting an IO handle */` |
|    ! 0 | 3083 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3084 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3085 | `		return PH7_OK;` |
|      - | 3086 | `	}` |
|      - | 3087 | `	/* Point to the target IO stream device */` |
|     59 | 3088 | `	pStream = pDev->pStream;` |
|     59 | 3089 | `	if( pStream == 0  ){` |
|    ! 0 | 3090 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3091 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3092 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3093 | `			);` |
|    ! 0 | 3094 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3095 | `		return PH7_OK;` |
|      - | 3096 | `	}` |
|     59 | 3097 | `	nLen = -1;` |
|     59 | 3098 | `	if( nArg > 1 ){` |
|      - | 3099 | `		/* Maximum data to read */` |
|    ! 0 | 3100 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3101 | `	}` |
|      - | 3102 | `	/* Perform the requested operation */` |
|     59 | 3103 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|     59 | 3104 | `	if( n < 1 ){` |
|      - | 3105 | `		/* EOF or IO error,return FALSE */` |
|      3 | 3106 | `		ph7_result_bool(pCtx,0);` |
|      2 | 3107 | `	}else{` |
|      - | 3108 | `		/* Return the freshly extracted line */` |
|     57 | 3109 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - | 3110 | `	}` |
|     59 | 3111 | `	return PH7_OK;` |
|     30 | 3112 |  |
|      - | 3113 | `/*` |
|      - | 3114 | ` * string fread(resource $handle,int64 $length)` |
|      - | 3115 | ` *  Binary-safe file read.` |
|      - | 3116 | ` * Parameters` |
|      - | 3117 | ` *  $handle` |
|      - | 3118 | ` *   The file pointer.` |
|      - | 3119 | ` * $length` |
|      - | 3120 | ` *  Up to length number of bytes read.` |
|      - | 3121 | ` * Return` |
|      - | 3122 | ` *  The data readen on success or FALSE on failure.` |
|      - | 3123 | ` */` |
|     10 | 3124 | `static int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3125 |  |
|      - | 3126 | `	const ph7_io_stream *pStream;` |
|      - | 3127 | `	io_private *pDev;` |
|      - | 3128 | `	ph7_int64 nRead;` |
|      - | 3129 | `	void *pBuf;` |
|      - | 3130 | `	int nLen;` |
|     11 | 3131 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3132 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3133 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3134 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3135 | `		return PH7_OK;` |
|      - | 3136 | `	}` |
|      - | 3137 | `	/* Extract our private data */` |
|     11 | 3138 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3139 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     11 | 3140 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3141 | `		/*Expecting an IO handle */` |
|    ! 0 | 3142 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3143 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3144 | `		return PH7_OK;` |
|      - | 3145 | `	}` |
|      - | 3146 | `	/* Point to the target IO stream device */` |
|     11 | 3147 | `	pStream = pDev->pStream;` |
|     11 | 3148 | `	if( pStream == 0  ){` |
|    ! 0 | 3149 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3150 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3151 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3152 | `			);` |
|    ! 0 | 3153 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3154 | `		return PH7_OK;` |
|      - | 3155 | `	}` |
|     11 | 3156 | `        nLen = 4096;` |
|     11 | 3157 | `	if( nArg > 1 ){` |
|     11 | 3158 | ` 	  nLen = ph7_value_to_int(apArg[1]);` |
|     11 | 3159 | `	  if( nLen < 1 ){` |
|      - | 3160 | `		/* Invalid length,set a default length */` |
|    ! 0 | 3161 | `		nLen = 4096;` |
|    ! 0 | 3162 | `	  }` |
|      5 | 3163 | `        }` |
|      - | 3164 | `	/* Allocate enough buffer */` |
|     11 | 3165 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     11 | 3166 | `	if( pBuf == 0 ){` |
|    ! 0 | 3167 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3168 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3169 | `		return PH7_OK;` |
|      - | 3170 | `	}` |
|      - | 3171 | `	/* Perform the requested operation */` |
|     11 | 3172 | `	nRead = StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     11 | 3173 | `	if( nRead < 1 ){` |
|      - | 3174 | `		/* Nothing read,return FALSE */` |
|    ! 0 | 3175 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3176 | `	}else{` |
|      - | 3177 | `		/* Make a copy of the data just read */` |
|     11 | 3178 | `		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);` |
|      - | 3179 | `	}` |
|      - | 3180 | `	/* Release the buffer */` |
|     11 | 3181 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     11 | 3182 | `	return PH7_OK;` |
|      6 | 3183 |  |
|      - | 3184 | `/*` |
|      - | 3185 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|      - | 3186 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|      - | 3187 | ` * Gets line from file pointer and parse for CSV fields.` |
|      - | 3188 | ` * Parameters` |
|      - | 3189 | ` * $handle` |
|      - | 3190 | ` *   The file pointer.` |
|      - | 3191 | ` * $length` |
|      - | 3192 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3193 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3194 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3195 | ` *  the end of the line.` |
|      - | 3196 | ` * $delimiter` |
|      - | 3197 | ` *   Set the field delimiter (one character only).` |
|      - | 3198 | ` * $enclosure` |
|      - | 3199 | ` *   Set the field enclosure character (one character only).` |
|      - | 3200 | ` * $escape` |
|      - | 3201 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 3202 | ` * Return` |
|      - | 3203 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3204 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3205 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3206 | ` */` |
|      2 | 3207 | `static int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3208 |  |
|      - | 3209 | `	const ph7_io_stream *pStream;` |
|      - | 3210 | `	const char *zLine;` |
|      - | 3211 | `	io_private *pDev;` |
|      - | 3212 | `	ph7_int64 n,nLen;` |
|      3 | 3213 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3214 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3215 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3216 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3217 | `		return PH7_OK;` |
|      - | 3218 | `	}` |
|      - | 3219 | `	/* Extract our private data */` |
|      3 | 3220 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3221 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3222 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3223 | `		/*Expecting an IO handle */` |
|    ! 0 | 3224 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3225 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3226 | `		return PH7_OK;` |
|      - | 3227 | `	}` |
|      - | 3228 | `	/* Point to the target IO stream device */` |
|      3 | 3229 | `	pStream = pDev->pStream;` |
|      3 | 3230 | `	if( pStream == 0  ){` |
|    ! 0 | 3231 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3232 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3233 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3234 | `			);` |
|    ! 0 | 3235 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3236 | `		return PH7_OK;` |
|      - | 3237 | `	}` |
|      3 | 3238 | `	nLen = -1;` |
|      3 | 3239 | `	if( nArg > 1 ){` |
|      - | 3240 | `		/* Maximum data to read */` |
|      3 | 3241 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      1 | 3242 | `	}` |
|      - | 3243 | `	/* Perform the requested operation */` |
|      3 | 3244 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3245 | `	if( n < 1 ){` |
|      - | 3246 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3247 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3248 | `	}else{` |
|      - | 3249 | `		ph7_value *pArray;` |
|      3 | 3250 | `		int delim  = ',';   /* Delimiter */` |
|      3 | 3251 | `		int encl   = '"' ;  /* Enclosure */` |
|      3 | 3252 | `		int escape = '\\';  /* Escape character */` |
|      3 | 3253 | `		if( nArg > 2 ){` |
|      - | 3254 | `			const char *zPtr;` |
|      - | 3255 | `			int i;` |
|      3 | 3256 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 3257 | `				/* Extract the delimiter */` |
|      3 | 3258 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 3259 | `				if( i > 0 ){` |
|      3 | 3260 | `					delim = zPtr[0];` |
|      1 | 3261 | `				}` |
|      1 | 3262 | `			}` |
|      3 | 3263 | `			if( nArg > 3 ){` |
|      3 | 3264 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 3265 | `					/* Extract the enclosure */` |
|      3 | 3266 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 3267 | `					if( i > 0 ){` |
|      3 | 3268 | `						encl = zPtr[0];` |
|      1 | 3269 | `					}` |
|      1 | 3270 | `				}` |
|      3 | 3271 | `				if( nArg > 4 ){` |
|      3 | 3272 | `					if( ph7_value_is_string(apArg[4]) ){` |
|      - | 3273 | `						/* Extract the escape character */` |
|      3 | 3274 | `						zPtr = ph7_value_to_string(apArg[4],&i);` |
|      3 | 3275 | `						if( i > 0 ){` |
|      3 | 3276 | `							escape = zPtr[0];` |
|      1 | 3277 | `						}` |
|      1 | 3278 | `					}` |
|      1 | 3279 | `				}` |
|      1 | 3280 | `			}` |
|      1 | 3281 | `		}` |
|      - | 3282 | `		/* Create our array */` |
|      3 | 3283 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 3284 | `		if( pArray == 0 ){` |
|    ! 0 | 3285 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3286 | `			ph7_result_null(pCtx);` |
|    ! 0 | 3287 | `			return PH7_OK;` |
|      - | 3288 | `		}` |
|      - | 3289 | `		/* Parse the raw input */` |
|      3 | 3290 | `		PH7_ProcessCsv(zLine,(int)n,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 3291 | `		/* Return the freshly created array  */` |
|      3 | 3292 | `		ph7_result_value(pCtx,pArray);` |
|      - | 3293 | `	}` |
|      3 | 3294 | `	return PH7_OK;` |
|      2 | 3295 |  |
|      - | 3296 | `/*` |
|      - | 3297 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|      - | 3298 | ` *  Gets line from file pointer and strip HTML tags.` |
|      - | 3299 | ` * Parameters` |
|      - | 3300 | ` * $handle` |
|      - | 3301 | ` *   The file pointer.` |
|      - | 3302 | ` * $length` |
|      - | 3303 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3304 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3305 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3306 | ` *  the end of the line.` |
|      - | 3307 | ` * $allowable_tags` |
|      - | 3308 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 3309 | ` * Return` |
|      - | 3310 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|      - | 3311 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|      - | 3312 | ` */` |
|      2 | 3313 | `static int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3314 |  |
|      - | 3315 | `	const ph7_io_stream *pStream;` |
|      - | 3316 | `	const char *zLine;` |
|      - | 3317 | `	io_private *pDev;` |
|      - | 3318 | `	ph7_int64 n,nLen;` |
|      3 | 3319 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3320 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3321 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3322 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3323 | `		return PH7_OK;` |
|      - | 3324 | `	}` |
|      - | 3325 | `	/* Extract our private data */` |
|      3 | 3326 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3327 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3328 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3329 | `		/*Expecting an IO handle */` |
|    ! 0 | 3330 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3331 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3332 | `		return PH7_OK;` |
|      - | 3333 | `	}` |
|      - | 3334 | `	/* Point to the target IO stream device */` |
|      3 | 3335 | `	pStream = pDev->pStream;` |
|      3 | 3336 | `	if( pStream == 0  ){` |
|    ! 0 | 3337 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3338 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3339 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3340 | `			);` |
|    ! 0 | 3341 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3342 | `		return PH7_OK;` |
|      - | 3343 | `	}` |
|      3 | 3344 | `	nLen = -1;` |
|      3 | 3345 | `	if( nArg > 1 ){` |
|      - | 3346 | `		/* Maximum data to read */` |
|    ! 0 | 3347 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3348 | `	}` |
|      - | 3349 | `	/* Perform the requested operation */` |
|      3 | 3350 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3351 | `	if( n < 1 ){` |
|      - | 3352 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3353 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3354 | `	}else{` |
|      3 | 3355 | `		const char *zTaglist = 0;` |
|      3 | 3356 | `		int nTaglen = 0;` |
|      3 | 3357 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|      - | 3358 | `			/* Allowed tag */` |
|    ! 0 | 3359 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|    ! 0 | 3360 | `		}` |
|      - | 3361 | `		/* Process data just read */` |
|      3 | 3362 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen);` |
|      - | 3363 | `	}` |
|      3 | 3364 | `	return PH7_OK;` |
|      2 | 3365 |  |
|      - | 3366 | `/*` |
|      - | 3367 | ` * string readdir(resource $dir_handle)` |
|      - | 3368 | ` *   Read entry from directory handle.` |
|      - | 3369 | ` * Parameter` |
|      - | 3370 | ` *  $dir_handle` |
|      - | 3371 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3372 | ` * Return` |
|      - | 3373 | ` *  Returns the filename on success or FALSE on failure.` |
|      - | 3374 | ` */` |
|     10 | 3375 | `static int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3376 |  |
|      - | 3377 | `	const ph7_io_stream *pStream;` |
|      - | 3378 | `	io_private *pDev;` |
|      - | 3379 | `	int rc;` |
|     11 | 3380 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3381 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3382 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3383 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3384 | `		return PH7_OK;` |
|      - | 3385 | `	}` |
|      - | 3386 | `	/* Extract our private data */` |
|     11 | 3387 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3388 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     11 | 3389 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3390 | `		/*Expecting an IO handle */` |
|    ! 0 | 3391 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3392 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3393 | `		return PH7_OK;` |
|      - | 3394 | `	}` |
|      - | 3395 | `	/* Point to the target IO stream device */` |
|     11 | 3396 | `	pStream = pDev->pStream;` |
|     11 | 3397 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 3398 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3399 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3400 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3401 | `			);` |
|    ! 0 | 3402 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3403 | `		return PH7_OK;` |
|      - | 3404 | `	}` |
|     11 | 3405 | `	ph7_result_bool(pCtx,0);` |
|      - | 3406 | `	/* Perform the requested operation */` |
|     11 | 3407 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|     11 | 3408 | `	if( rc != PH7_OK ){` |
|      - | 3409 | `		/* Return FALSE */` |
|      3 | 3410 | `		ph7_result_bool(pCtx,0);` |
|      1 | 3411 | `	}` |
|     11 | 3412 | `	return PH7_OK;` |
|      6 | 3413 |  |
|      - | 3414 | `/*` |
|      - | 3415 | ` * void rewinddir(resource $dir_handle)` |
|      - | 3416 | ` *   Rewind directory handle.` |
|      - | 3417 | ` * Parameter` |
|      - | 3418 | ` *  $dir_handle` |
|      - | 3419 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3420 | ` * Return` |
|      - | 3421 | ` *  FALSE on failure.` |
|      - | 3422 | ` */` |
|      2 | 3423 | `static int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3424 |  |
|      - | 3425 | `	const ph7_io_stream *pStream;` |
|      - | 3426 | `	io_private *pDev;` |
|      3 | 3427 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3428 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3429 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3430 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3431 | `		return PH7_OK;` |
|      - | 3432 | `	}` |
|      - | 3433 | `	/* Extract our private data */` |
|      3 | 3434 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3435 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3436 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3437 | `		/*Expecting an IO handle */` |
|    ! 0 | 3438 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3439 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3440 | `		return PH7_OK;` |
|      - | 3441 | `	}` |
|      - | 3442 | `	/* Point to the target IO stream device */` |
|      3 | 3443 | `	pStream = pDev->pStream;` |
|      3 | 3444 | `	if( pStream == 0  \|\| pStream->xRewindDir == 0 ){` |
|    ! 0 | 3445 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3446 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3447 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3448 | `			);` |
|    ! 0 | 3449 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3450 | `		return PH7_OK;` |
|      - | 3451 | `	}` |
|      - | 3452 | `	/* Perform the requested operation */` |
|      3 | 3453 | `	pStream->xRewindDir(pDev->pHandle);` |
|      3 | 3454 | `	return PH7_OK;` |
|      2 | 3455 | ` }` |
|      - | 3456 | `/* Forward declaration */` |
|      - | 3457 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut);` |
|      - | 3458 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|      - | 3459 | `/*` |
|      - | 3460 | ` * void closedir(resource $dir_handle)` |
|      - | 3461 | ` *   Close directory handle.` |
|      - | 3462 | ` * Parameter` |
|      - | 3463 | ` *  $dir_handle` |
|      - | 3464 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3465 | ` * Return` |
|      - | 3466 | ` *  FALSE on failure.` |
|      - | 3467 | ` */` |
|      4 | 3468 | `static int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3469 |  |
|      - | 3470 | `	const ph7_io_stream *pStream;` |
|      - | 3471 | `	io_private *pDev;` |
|      5 | 3472 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3473 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3474 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3475 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3476 | `		return PH7_OK;` |
|      - | 3477 | `	}` |
|      - | 3478 | `	/* Extract our private data */` |
|      5 | 3479 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3480 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 3481 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3482 | `		/*Expecting an IO handle */` |
|    ! 0 | 3483 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3484 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3485 | `		return PH7_OK;` |
|      - | 3486 | `	}` |
|      - | 3487 | `	/* Point to the target IO stream device */` |
|      5 | 3488 | `	pStream = pDev->pStream;` |
|      5 | 3489 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 3490 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3491 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3492 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3493 | `			);` |
|    ! 0 | 3494 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3495 | `		return PH7_OK;` |
|      - | 3496 | `	}` |
|      - | 3497 | `	/* Perform the requested operation */` |
|      5 | 3498 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 3499 | `	/* Release the private stucture */` |
|      5 | 3500 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      5 | 3501 | `	PH7_MemObjRelease(apArg[0]);` |
|      5 | 3502 | `	return PH7_OK;` |
|      3 | 3503 | ` }` |
|      - | 3504 | `/*` |
|      - | 3505 | ` * resource opendir(string $path[,resource $context])` |
|      - | 3506 | ` *  Open directory handle.` |
|      - | 3507 | ` * Parameters` |
|      - | 3508 | ` * $path` |
|      - | 3509 | ` *   The directory path that is to be opened.` |
|      - | 3510 | ` * $context` |
|      - | 3511 | ` *   A context stream resource.` |
|      - | 3512 | ` * Return` |
|      - | 3513 | ` *  A directory handle resource on success,or FALSE on failure.` |
|      - | 3514 | ` */` |
|      4 | 3515 | `static int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3516 |  |
|      - | 3517 | `	const ph7_io_stream *pStream;` |
|      - | 3518 | `	const char *zPath;` |
|      - | 3519 | `	io_private *pDev;` |
|      - | 3520 | `	int iLen,rc;` |
|      5 | 3521 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3522 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3523 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 3524 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3525 | `		return PH7_OK;` |
|      - | 3526 | `	}` |
|      - | 3527 | `	/* Extract the target path */` |
|      5 | 3528 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 3529 | `	/* Try to extract a stream */` |
|      5 | 3530 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|      5 | 3531 | `	if( pStream == 0 ){` |
|    ! 0 | 3532 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3533 | `			"No stream device is associated with the given path(%s)",zPath);` |
|    ! 0 | 3534 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3535 | `		return PH7_OK;` |
|      - | 3536 | `	}` |
|      5 | 3537 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 3538 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3539 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 3540 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 3541 | `			);` |
|    ! 0 | 3542 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3543 | `		return PH7_OK;` |
|      - | 3544 | `	}` |
|      - | 3545 | `	/* Allocate a new IO private instance */` |
|      5 | 3546 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      5 | 3547 | `	if( pDev == 0 ){` |
|    ! 0 | 3548 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3549 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3550 | `		return PH7_OK;` |
|      - | 3551 | `	}` |
|      - | 3552 | `	/* Initialize the structure */` |
|      5 | 3553 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 3554 | `	/* Open the target directory */` |
|      5 | 3555 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|      5 | 3556 | `	if( rc != PH7_OK ){` |
|      - | 3557 | `		/* IO error,return FALSE */` |
|    ! 0 | 3558 | `		ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 3559 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3560 | `	}else{` |
|      - | 3561 | `		/* Return the handle as a resource */` |
|      5 | 3562 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 3563 | `	}` |
|      5 | 3564 | `	return PH7_OK;` |
|      3 | 3565 |  |
|      - | 3566 | `/*` |
|      - | 3567 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|      - | 3568 | ` *  Reads a file and writes it to the output buffer.` |
|      - | 3569 | ` * Parameters` |
|      - | 3570 | ` *  $filename` |
|      - | 3571 | ` *   The filename being read.` |
|      - | 3572 | ` *  $use_include_path` |
|      - | 3573 | ` *   You can use the optional second parameter and set it to` |
|      - | 3574 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3575 | ` *  $context` |
|      - | 3576 | ` *   A context stream resource.` |
|      - | 3577 | ` * Return` |
|      - | 3578 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|      - | 3579 | ` */` |
|      2 | 3580 | `static int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3581 |  |
|      3 | 3582 | `	int use_include  = FALSE;` |
|      - | 3583 | `	const ph7_io_stream *pStream;` |
|      - | 3584 | `	ph7_int64 n,nRead;` |
|      - | 3585 | `	const char *zFile;` |
|      - | 3586 | `	char zBuf[8192];` |
|      - | 3587 | `	void *pHandle;` |
|      - | 3588 | `	int rc,nLen;` |
|      3 | 3589 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3590 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3591 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3592 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3593 | `		return PH7_OK;` |
|      - | 3594 | `	}` |
|      - | 3595 | `	/* Extract the file path */` |
|      3 | 3596 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3597 | `	/* Point to the target IO stream device */` |
|      3 | 3598 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3599 | `	if( pStream == 0 ){` |
|    ! 0 | 3600 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3601 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3602 | `		return PH7_OK;` |
|      - | 3603 | `	}` |
|      3 | 3604 | `	if( nArg > 1 ){` |
|    ! 0 | 3605 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3606 | `	}` |
|      - | 3607 | `	/* Try to open the file in read-only mode */` |
|      4 | 3608 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|      1 | 3609 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 3610 | `	if( pHandle == 0 ){` |
|    ! 0 | 3611 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3612 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3613 | `		return PH7_OK;` |
|      - | 3614 | `	}` |
|      - | 3615 | `	/* Perform the requested operation */` |
|      3 | 3616 | `	nRead = 0;` |
|      2 | 3617 | `	for(;;){` |
|      5 | 3618 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3619 | `		if( n < 1 ){` |
|      - | 3620 | `			/* EOF or IO error,break immediately */` |
|      3 | 3621 | `			break;` |
|      - | 3622 | `		}` |
|      - | 3623 | `		/* Output data */` |
|      3 | 3624 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      3 | 3625 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 3626 | `			break;` |
|      - | 3627 | `		}` |
|      - | 3628 | `		/* Increment counter */` |
|      3 | 3629 | `		nRead += n;` |
|      1 | 3630 | `	}` |
|      - | 3631 | `	/* Close the stream */` |
|      3 | 3632 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3633 | `	/* Total number of bytes readen */` |
|      3 | 3634 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 3635 | `	return PH7_OK;` |
|      2 | 3636 |  |
|      - | 3637 | `/*` |
|      - | 3638 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|      - | 3639 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|      - | 3640 | ` *  Reads entire file into a string.` |
|      - | 3641 | ` * Parameters` |
|      - | 3642 | ` *  $filename` |
|      - | 3643 | ` *   The filename being read.` |
|      - | 3644 | ` *  $use_include_path` |
|      - | 3645 | ` *   You can use the optional second parameter and set it to` |
|      - | 3646 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3647 | ` *  $context` |
|      - | 3648 | ` *   A context stream resource.` |
|      - | 3649 | ` *  $offset` |
|      - | 3650 | ` *   The offset where the reading starts on the original stream.` |
|      - | 3651 | ` *  $maxlen` |
|      - | 3652 | ` *    Maximum length of data read. The default is to read until end of file` |
|      - | 3653 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|      - | 3654 | ` * Return` |
|      - | 3655 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3656 | ` */` |
|     42 | 3657 | `static int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3658 |  |
|      - | 3659 | `	const ph7_io_stream *pStream;` |
|      - | 3660 | `	ph7_int64 n,nRead,nMaxlen;` |
|     43 | 3661 | `	int use_include  = FALSE;` |
|      - | 3662 | `	const char *zFile;` |
|      - | 3663 | `	char zBuf[8192];` |
|      - | 3664 | `	void *pHandle;` |
|      - | 3665 | `	int nLen;` |
|      - | 3666 |  |
|     43 | 3667 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3668 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3669 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3670 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3671 | `		return PH7_OK;` |
|      - | 3672 | `	}` |
|      - | 3673 | `	/* Extract the file path */` |
|     43 | 3674 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3675 | `	/* Point to the target IO stream device */` |
|     43 | 3676 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     43 | 3677 | `	if( pStream == 0 ){` |
|    ! 0 | 3678 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3679 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3680 | `		return PH7_OK;` |
|      - | 3681 | `	}` |
|     43 | 3682 | `	nMaxlen = -1;` |
|     43 | 3683 | `	if( nArg > 1 ){` |
|      5 | 3684 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|      2 | 3685 | `	}` |
|      - | 3686 | `	/* Try to open the file in read-only mode */` |
|     43 | 3687 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|     43 | 3688 | `	if( pHandle == 0 ){` |
|    ! 0 | 3689 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3690 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3691 | `		return PH7_OK;` |
|      - | 3692 | `	}` |
|     43 | 3693 | `	if( nArg > 3 ){` |
|      - | 3694 | `		/* Extract the offset */` |
|      5 | 3695 | `		n = ph7_value_to_int64(apArg[3]);` |
|      5 | 3696 | `		if( n > 0 ){` |
|    ! 0 | 3697 | `			if( pStream->xSeek ){` |
|      - | 3698 | `				/* Seek to the desired offset */` |
|    ! 0 | 3699 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|    ! 0 | 3700 | `			}` |
|    ! 0 | 3701 | `		}` |
|      5 | 3702 | `		if( nArg > 4 ){` |
|      - | 3703 | `			/* Maximum data to read */` |
|      5 | 3704 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|      2 | 3705 | `		}` |
|      2 | 3706 | `	}` |
|      - | 3707 | `	/* Perform the requested operation */` |
|     43 | 3708 | `	nRead = 0;` |
|     41 | 3709 | `	for(;;){` |
|    127 | 3710 | `		n = pStream->xRead(pHandle,zBuf,` |
|     44 | 3711 | `			(nMaxlen > 0 && (nMaxlen < (ph7_int64)sizeof(zBuf))) ? nMaxlen : (ph7_int64)sizeof(zBuf));` |
|     83 | 3712 | `		if( n < 1 ){` |
|      - | 3713 | `			/* EOF or IO error,break immediately */` |
|     41 | 3714 | `			break;` |
|      - | 3715 | `		}` |
|      - | 3716 | `		/* Append data */` |
|     43 | 3717 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 3718 | `		/* Increment read counter */` |
|     43 | 3719 | `		nRead += n;` |
|     43 | 3720 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 3721 | `			/* Read limit reached */` |
|      3 | 3722 | `			break;` |
|      - | 3723 | `		}` |
|      1 | 3724 | `	}` |
|      - | 3725 | `	/* Close the stream */` |
|     43 | 3726 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3727 | `	/* Check if we have read something */` |
|     43 | 3728 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 3729 | `		/* Nothing read,return FALSE */` |
|    ! 0 | 3730 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3731 | `	}` |
|     43 | 3732 | `	return PH7_OK;` |
|     22 | 3733 |  |
|      - | 3734 | `/*` |
|      - | 3735 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 3736 | ` *  Write a string to a file.` |
|      - | 3737 | ` * Parameters` |
|      - | 3738 | ` *  $filename` |
|      - | 3739 | ` *  Path to the file where to write the data.` |
|      - | 3740 | ` * $data` |
|      - | 3741 | ` *  The data to write(Must be a string).` |
|      - | 3742 | ` * $flags` |
|      - | 3743 | ` *  The value of flags can be any combination of the following` |
|      - | 3744 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 3745 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 3746 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 3747 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 3748 | ` * context` |
|      - | 3749 | ` *  A context stream resource.` |
|      - | 3750 | ` * Return` |
|      - | 3751 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 3752 | ` */` |
|    140 | 3753 | `static int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3754 |  |
|    141 | 3755 | `	int use_include  = FALSE;` |
|      - | 3756 | `	const ph7_io_stream *pStream;` |
|      - | 3757 | `	const char *zFile;` |
|      - | 3758 | `	const char *zData;` |
|      - | 3759 | `	int iOpenFlags;` |
|      - | 3760 | `	void *pHandle;` |
|      - | 3761 | `	int iFlags;` |
|      - | 3762 | `	int nLen;` |
|      - | 3763 |  |
|    141 | 3764 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3765 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3766 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3767 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3768 | `		return PH7_OK;` |
|      - | 3769 | `	}` |
|      - | 3770 | `	/* Extract the file path */` |
|    141 | 3771 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3772 | `	/* Point to the target IO stream device */` |
|    141 | 3773 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|    141 | 3774 | `	if( pStream == 0 ){` |
|    ! 0 | 3775 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3776 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3777 | `		return PH7_OK;` |
|      - | 3778 | `	}` |
|      - | 3779 | `	/* Data to write */` |
|    141 | 3780 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 3781 | `	/* Try to open the file in read-write mode */` |
|    141 | 3782 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 3783 | `	/* Extract the flags */` |
|    141 | 3784 | `	iFlags = 0;` |
|    141 | 3785 | `	if( nArg > 2 ){` |
|    ! 0 | 3786 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3787 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 3788 | `			use_include = TRUE;` |
|    ! 0 | 3789 | `		}` |
|    ! 0 | 3790 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 3791 | `			/* If the file already exists, append the data to the file` |
|      - | 3792 | `			 * instead of overwriting it.` |
|      - | 3793 | `			 */` |
|    ! 0 | 3794 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 3795 | `			/* Append mode */` |
|    ! 0 | 3796 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 3797 | `		}` |
|    ! 0 | 3798 | `	}` |
|    211 | 3799 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|     70 | 3800 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|    141 | 3801 | `	if( pHandle == 0 ){` |
|    ! 0 | 3802 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3803 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3804 | `		return PH7_OK;` |
|      - | 3805 | `	}` |
|    141 | 3806 | `	if( nLen < 1 ){` |
|      - | 3807 | `		/* Empty data, file is created/truncated */` |
|      7 | 3808 | `		ph7_result_int64(pCtx,0);` |
|      7 | 3809 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|      7 | 3810 | `		return PH7_OK;` |
|      - | 3811 | `	}` |
|    135 | 3812 | `	if( pStream->xWrite ){` |
|      - | 3813 | `		ph7_int64 n;` |
|    135 | 3814 | `		if( (iFlags & 0x01/* LOCK_EX */) && pStream->xLock ){` |
|      - | 3815 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 3816 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 3817 | `		}` |
|      - | 3818 | `		/* Perform the write operation */` |
|    135 | 3819 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|    135 | 3820 | `		if( n < 0 ){` |
|      - | 3821 | `			/* IO error,return FALSE */` |
|    ! 0 | 3822 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3823 | `		}else{` |
|      - | 3824 | `			/* Total number of bytes written */` |
|    135 | 3825 | `			ph7_result_int64(pCtx,n);` |
|      - | 3826 | `		}` |
|     68 | 3827 | `	}else{` |
|      - | 3828 | `		/* Read-only stream */` |
|    ! 0 | 3829 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 3830 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 3831 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 3832 | `			);` |
|    ! 0 | 3833 | `		ph7_result_bool(pCtx,0);` |
|      - | 3834 | `	}` |
|      - | 3835 | `	/* Close the handle */` |
|    135 | 3836 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|    135 | 3837 | `	return PH7_OK;` |
|     71 | 3838 |  |
|      - | 3839 | `/*` |
|      - | 3840 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 3841 | ` *  Reads entire file into an array.` |
|      - | 3842 | ` * Parameters` |
|      - | 3843 | ` *  $filename` |
|      - | 3844 | ` *   The filename being read.` |
|      - | 3845 | ` *  $flags` |
|      - | 3846 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 3847 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 3848 | ` *       Search for the file in the include_path.` |
|      - | 3849 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 3850 | ` *       Do not add newline at the end of each array element` |
|      - | 3851 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 3852 | ` *       Skip empty lines` |
|      - | 3853 | ` *  $context` |
|      - | 3854 | ` *   A context stream resource.` |
|      - | 3855 | ` * Return` |
|      - | 3856 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3857 | ` */` |
|      8 | 3858 | `static int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3859 |  |
|      - | 3860 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 3861 | `	ph7_value *pArray,*pLine;` |
|      - | 3862 | `	const ph7_io_stream *pStream;` |
|      9 | 3863 | `	int use_include = 0;` |
|      - | 3864 | `	io_private *pDev;` |
|      - | 3865 | `	ph7_int64 n;` |
|      - | 3866 | `	int iFlags;` |
|      - | 3867 | `	int nLen;` |
|      - | 3868 |  |
|      9 | 3869 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3870 | `		/* Missing/Invalid arguments,return FALSE */` |
|      5 | 3871 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|      5 | 3872 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3873 | `		return PH7_OK;` |
|      - | 3874 | `	}` |
|      - | 3875 | `	/* Extract the file path */` |
|      5 | 3876 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3877 | `	/* Point to the target IO stream device */` |
|      5 | 3878 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      5 | 3879 | `	if( pStream == 0 ){` |
|    ! 0 | 3880 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3881 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3882 | `		return PH7_OK;` |
|      - | 3883 | `	}` |
|      - | 3884 | `	/* Allocate a new IO private instance */` |
|      5 | 3885 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      5 | 3886 | `	if( pDev == 0 ){` |
|    ! 0 | 3887 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3888 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3889 | `		return PH7_OK;` |
|      - | 3890 | `	}` |
|      - | 3891 | `	/* Initialize the structure */` |
|      5 | 3892 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      5 | 3893 | `	iFlags = 0;` |
|      5 | 3894 | `	if( nArg > 1 ){` |
|    ! 0 | 3895 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 3896 | `	}` |
|      5 | 3897 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|    ! 0 | 3898 | `		use_include = TRUE;` |
|    ! 0 | 3899 | `	}` |
|      - | 3900 | `	/* Create the array and the working value */` |
|      5 | 3901 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 3902 | `	pLine = ph7_context_new_scalar(pCtx);` |
|      5 | 3903 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 3904 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3905 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3906 | `		return PH7_OK;` |
|      - | 3907 | `	}` |
|      - | 3908 | `	/* Try to open the file in read-only mode */` |
|      5 | 3909 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      5 | 3910 | `	if( pDev->pHandle == 0 ){` |
|      3 | 3911 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|      3 | 3912 | `		ph7_result_bool(pCtx,0);` |
|      - | 3913 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 3914 | `		 * as soon we return from this function.` |
|      - | 3915 | `		 */` |
|      3 | 3916 | `		return PH7_OK;` |
|      - | 3917 | `	}` |
|      - | 3918 | `	/* Perform the requested operation */` |
|      3 | 3919 | `	for(;;){` |
|      - | 3920 | `		/* Try to extract a line */` |
|      7 | 3921 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|      7 | 3922 | `		if( n < 1 ){` |
|      - | 3923 | `			/* EOF or IO error */` |
|      3 | 3924 | `			break;` |
|      - | 3925 | `		}` |
|      - | 3926 | `		/* Reset the cursor */` |
|      5 | 3927 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 3928 | `		/* Remove line ending if requested by the caller */` |
|      5 | 3929 | `		zPtr = zBuf;` |
|      5 | 3930 | `		zEnd = &zBuf[n];` |
|      5 | 3931 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 3932 | `			/* Ignore trailig lines */` |
|    ! 0 | 3933 | `			while( zPtr < zEnd && (zEnd[-1] == '\n'` |
|      - | 3934 | `#ifdef __WINNT__` |
|      - | 3935 | `				\|\| zEnd[-1] == '\r'` |
|      - | 3936 | `#endif` |
|      - | 3937 | `				)){` |
|    ! 0 | 3938 | `					n--;` |
|    ! 0 | 3939 | `					zEnd--;` |
|    ! 0 | 3940 | `			}` |
|    ! 0 | 3941 | `		}` |
|      5 | 3942 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 3943 | `			/* Ignore empty lines */` |
|    ! 0 | 3944 | `			while( zPtr < zEnd && (unsigned char)zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) ){` |
|    ! 0 | 3945 | `				zPtr++;` |
|    ! 0 | 3946 | `			}` |
|    ! 0 | 3947 | `			if( zPtr >= zEnd ){` |
|      - | 3948 | `				/* Empty line */` |
|    ! 0 | 3949 | `				continue;` |
|      - | 3950 | `			}` |
|    ! 0 | 3951 | `		}` |
|      5 | 3952 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 3953 | `		/* Insert line */` |
|      5 | 3954 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 3955 | `	}` |
|      - | 3956 | `	/* Close the stream */` |
|      3 | 3957 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 3958 | `	/* Release the io_private instance */` |
|      3 | 3959 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 3960 | `	/* Return the created array */` |
|      3 | 3961 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3962 | `	return PH7_OK;` |
|      5 | 3963 |  |
|      - | 3964 | `/*` |
|      - | 3965 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 3966 | ` *  Makes a copy of the file source to dest.` |
|      - | 3967 | ` * Parameters` |
|      - | 3968 | ` *  $source` |
|      - | 3969 | ` *   Path to the source file.` |
|      - | 3970 | ` *  $dest` |
|      - | 3971 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 3972 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 3973 | ` *  $context` |
|      - | 3974 | ` *   A context stream resource.` |
|      - | 3975 | ` * Return` |
|      - | 3976 | ` *  TRUE on success or FALSE on failure.` |
|      - | 3977 | ` */` |
|     10 | 3978 | `static int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3979 |  |
|      - | 3980 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 3981 | `	const char *zFile;` |
|      - | 3982 | `	char zBuf[8192];` |
|      - | 3983 | `	void *pIn,*pOut;` |
|      - | 3984 | `	ph7_int64 n;` |
|      - | 3985 | `	int nLen;` |
|     11 | 3986 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 3987 | `		/* Missing/Invalid arguments,return FALSE */` |
|      7 | 3988 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|      7 | 3989 | `		ph7_result_bool(pCtx,0);` |
|      7 | 3990 | `		return PH7_OK;` |
|      - | 3991 | `	}` |
|      - | 3992 | `	/* Extract the source name */` |
|      5 | 3993 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3994 | `	/* Point to the target IO stream device */` |
|      5 | 3995 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      5 | 3996 | `	if( pSin == 0 ){` |
|    ! 0 | 3997 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3998 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3999 | `		return PH7_OK;` |
|      - | 4000 | `	}` |
|      - | 4001 | `	/* Try to open the source file in a read-only mode */` |
|      5 | 4002 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      5 | 4003 | `	if( pIn == 0 ){` |
|      3 | 4004 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening source: '%s'",zFile);` |
|      3 | 4005 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4006 | `		return PH7_OK;` |
|      - | 4007 | `	}` |
|      - | 4008 | `	/* Extract the destination name */` |
|      3 | 4009 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 4010 | `	/* Point to the target IO stream device */` |
|      3 | 4011 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4012 | `	if( pSout == 0 ){` |
|    ! 0 | 4013 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4014 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4015 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4016 | `		return PH7_OK;` |
|      - | 4017 | `	}` |
|      3 | 4018 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 4019 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4020 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4021 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 4022 | `			);` |
|    ! 0 | 4023 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4024 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4025 | `		return PH7_OK;` |
|      - | 4026 | `	}` |
|      - | 4027 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 4028 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 4029 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 4030 | `	if( pOut == 0 ){` |
|    ! 0 | 4031 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening destination: '%s'",zFile);` |
|    ! 0 | 4032 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4033 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4034 | `		return PH7_OK;` |
|      - | 4035 | `	}` |
|      - | 4036 | `	/* Perform the requested operation */` |
|      2 | 4037 | `	for(;;){` |
|      - | 4038 | `		/* Read from source */` |
|      5 | 4039 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 4040 | `		if( n < 1 ){` |
|      - | 4041 | `			/* EOF or IO error,break immediately */` |
|      3 | 4042 | `			break;` |
|      - | 4043 | `		}` |
|      - | 4044 | `		/* Write to dest */` |
|      3 | 4045 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 4046 | `		if( n < 1 ){` |
|      - | 4047 | `			/* IO error,break immediately */` |
|    ! 0 | 4048 | `			break;` |
|      - | 4049 | `		}` |
|      1 | 4050 | `	}` |
|      - | 4051 | `	/* Close the streams */` |
|      3 | 4052 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 4053 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 4054 | `	/* Return TRUE */` |
|      3 | 4055 | `	ph7_result_bool(pCtx,1);` |
|      3 | 4056 | `	return PH7_OK;` |
|      6 | 4057 |  |
|      - | 4058 | `/*` |
|      - | 4059 | ` * array fstat(resource $handle)` |
|      - | 4060 | ` *  Gets information about a file using an open file pointer.` |
|      - | 4061 | ` * Parameters` |
|      - | 4062 | ` *  $handle` |
|      - | 4063 | ` *   The file pointer.` |
|      - | 4064 | ` * Return` |
|      - | 4065 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 4066 | ` */` |
|      2 | 4067 | `static int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4068 |  |
|      - | 4069 | `	ph7_value *pArray,*pValue;` |
|      - | 4070 | `	const ph7_io_stream *pStream;` |
|      - | 4071 | `	io_private *pDev;` |
|      3 | 4072 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4073 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4074 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4075 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4076 | `		return PH7_OK;` |
|      - | 4077 | `	}` |
|      - | 4078 | `	/* Extract our private data */` |
|      3 | 4079 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4080 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4081 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4082 | `		/* Expecting an IO handle */` |
|    ! 0 | 4083 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4084 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4085 | `		return PH7_OK;` |
|      - | 4086 | `	}` |
|      - | 4087 | `	/* Point to the target IO stream device */` |
|      3 | 4088 | `	pStream = pDev->pStream;` |
|      3 | 4089 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 4090 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4091 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4092 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4093 | `			);` |
|    ! 0 | 4094 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4095 | `		return PH7_OK;` |
|      - | 4096 | `	}` |
|      - | 4097 | `	/* Create the array and the working value */` |
|      3 | 4098 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4099 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4100 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4101 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4102 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4103 | `		return PH7_OK;` |
|      - | 4104 | `	}` |
|      - | 4105 | `	/* Perform the requested operation */` |
|      3 | 4106 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 4107 | `	/* Return the freshly created array */` |
|      3 | 4108 | `	ph7_result_value(pCtx,pArray);` |
|      - | 4109 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 4110 | `	 * released automatically as soon we return from this function.` |
|      - | 4111 | `	 */` |
|      3 | 4112 | `	return PH7_OK;` |
|      2 | 4113 |  |
|      - | 4114 | `/*` |
|      - | 4115 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 4116 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 4117 | ` * Parameters` |
|      - | 4118 | ` *  $handle` |
|      - | 4119 | ` *   The file pointer.` |
|      - | 4120 | ` *  $string` |
|      - | 4121 | ` *   The string that is to be written.` |
|      - | 4122 | ` *  $length` |
|      - | 4123 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 4124 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 4125 | ` * Return` |
|      - | 4126 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 4127 | ` */` |
|      6 | 4128 | `static int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4129 |  |
|      - | 4130 | `	const ph7_io_stream *pStream;` |
|      - | 4131 | `	const char *zString;` |
|      - | 4132 | `	io_private *pDev;` |
|      - | 4133 | `	int nLen,n;` |
|      7 | 4134 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4135 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4136 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4137 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4138 | `		return PH7_OK;` |
|      - | 4139 | `	}` |
|      - | 4140 | `	/* Extract our private data */` |
|      7 | 4141 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4142 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 4143 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4144 | `		/* Expecting an IO handle */` |
|    ! 0 | 4145 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4146 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4147 | `		return PH7_OK;` |
|      - | 4148 | `	}` |
|      - | 4149 | `	/* Point to the target IO stream device */` |
|      7 | 4150 | `	pStream = pDev->pStream;` |
|      7 | 4151 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4152 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4153 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4154 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4155 | `			);` |
|    ! 0 | 4156 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4157 | `		return PH7_OK;` |
|      - | 4158 | `	}` |
|      - | 4159 | `	/* Extract the data to write */` |
|      7 | 4160 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|      7 | 4161 | `	if( nArg > 2 ){` |
|      - | 4162 | `		/* Maximum data length to write */` |
|    ! 0 | 4163 | `		n = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 4164 | `		if( n >= 0 && n < nLen ){` |
|    ! 0 | 4165 | `			nLen = n;` |
|    ! 0 | 4166 | `		}` |
|    ! 0 | 4167 | `	}` |
|      7 | 4168 | `	if( nLen < 1 ){` |
|      - | 4169 | `		/* Nothing to write */` |
|    ! 0 | 4170 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4171 | `		return PH7_OK;` |
|      - | 4172 | `	}` |
|      - | 4173 | `	/* Perform the requested operation */` |
|      7 | 4174 | `	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);` |
|      7 | 4175 | `	if( n <  0 ){` |
|      - | 4176 | `		/* IO error,return FALSE */` |
|    ! 0 | 4177 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4178 | `	}else{` |
|      - | 4179 | `		/* #Bytes written */` |
|      7 | 4180 | `		ph7_result_int(pCtx,n);` |
|      - | 4181 | `	}` |
|      7 | 4182 | `	return PH7_OK;` |
|      4 | 4183 |  |
|      - | 4184 | `/*` |
|      - | 4185 | ` * bool flock(resource $handle,int $operation)` |
|      - | 4186 | ` *  Portable advisory file locking.` |
|      - | 4187 | ` * Parameters` |
|      - | 4188 | ` *  $handle` |
|      - | 4189 | ` *   The file pointer.` |
|      - | 4190 | ` *  $operation` |
|      - | 4191 | ` *   operation is one of the following:` |
|      - | 4192 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 4193 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 4194 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 4195 | ` * Return` |
|      - | 4196 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 4197 | ` */` |
|      4 | 4198 | `static int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4199 |  |
|      - | 4200 | `	const ph7_io_stream *pStream;` |
|      - | 4201 | `	io_private *pDev;` |
|      - | 4202 | `	int nLock;` |
|      - | 4203 | `	int rc;` |
|      5 | 4204 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4205 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4206 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4207 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4208 | `		return PH7_OK;` |
|      - | 4209 | `	}` |
|      - | 4210 | `	/* Extract our private data */` |
|      5 | 4211 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4212 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 4213 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4214 | `		/*Expecting an IO handle */` |
|    ! 0 | 4215 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4216 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4217 | `		return PH7_OK;` |
|      - | 4218 | `	}` |
|      - | 4219 | `	/* Point to the target IO stream device */` |
|      5 | 4220 | `	pStream = pDev->pStream;` |
|      5 | 4221 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|    ! 0 | 4222 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4223 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4224 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4225 | `			);` |
|    ! 0 | 4226 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4227 | `		return PH7_OK;` |
|      - | 4228 | `	}` |
|      - | 4229 | `	/* Requested lock operation */` |
|      5 | 4230 | `	nLock = ph7_value_to_int(apArg[1]);` |
|      - | 4231 | `	/* Lock operation */` |
|      5 | 4232 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 4233 | `	/* IO result */` |
|      5 | 4234 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 4235 | `	return PH7_OK;` |
|      3 | 4236 |  |
|      - | 4237 | `/*` |
|      - | 4238 | ` * int fpassthru(resource $handle)` |
|      - | 4239 | ` *  Output all remaining data on a file pointer.` |
|      - | 4240 | ` * Parameters` |
|      - | 4241 | ` *  $handle` |
|      - | 4242 | ` *   The file pointer.` |
|      - | 4243 | ` * Return` |
|      - | 4244 | ` *  Total number of characters read from handle and passed through` |
|      - | 4245 | ` *  to the output on success or FALSE on failure.` |
|      - | 4246 | ` */` |
|      2 | 4247 | `static int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4248 |  |
|      - | 4249 | `	const ph7_io_stream *pStream;` |
|      - | 4250 | `	io_private *pDev;` |
|      - | 4251 | `	ph7_int64 n,nRead;` |
|      - | 4252 | `	char zBuf[8192];` |
|      - | 4253 | `	int rc;` |
|      3 | 4254 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4255 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4256 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4257 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4258 | `		return PH7_OK;` |
|      - | 4259 | `	}` |
|      - | 4260 | `	/* Extract our private data */` |
|      3 | 4261 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4262 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4263 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4264 | `		/*Expecting an IO handle */` |
|    ! 0 | 4265 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4266 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4267 | `		return PH7_OK;` |
|      - | 4268 | `	}` |
|      - | 4269 | `	/* Point to the target IO stream device */` |
|      3 | 4270 | `	pStream = pDev->pStream;` |
|      3 | 4271 | `	if( pStream == 0  ){` |
|    ! 0 | 4272 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4273 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4274 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4275 | `			);` |
|    ! 0 | 4276 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4277 | `		return PH7_OK;` |
|      - | 4278 | `	}` |
|      - | 4279 | `	/* Perform the requested operation */` |
|      3 | 4280 | `	nRead = 0;` |
|      2 | 4281 | `	for(;;){` |
|      5 | 4282 | `		n = StreamRead(pDev,zBuf,sizeof(zBuf));` |
|      5 | 4283 | `		if( n < 1 ){` |
|      - | 4284 | `			/* Error or EOF */` |
|      3 | 4285 | `			break;` |
|      - | 4286 | `		}` |
|      - | 4287 | `		/* Increment the read counter */` |
|      3 | 4288 | `		nRead += n;` |
|      - | 4289 | `		/* Output data */` |
|      3 | 4290 | `		rc = ph7_context_output(pCtx,zBuf,(int)nRead /* FIXME: 64-bit issues */);` |
|      3 | 4291 | `		if( rc == PH7_ABORT ){` |
|      - | 4292 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 4293 | `			break;` |
|      - | 4294 | `		}` |
|      1 | 4295 | `	}` |
|      - | 4296 | `	/* Total number of bytes readen */` |
|      3 | 4297 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 4298 | `	return PH7_OK;` |
|      2 | 4299 |  |
|      - | 4300 | `/* CSV reader/writer private data */` |
|      - | 4301 | `struct csv_data` |
|      - | 4302 |  |
|      - | 4303 | `	int delimiter;    /* Delimiter. Default ',' */` |
|      - | 4304 | `	int enclosure;    /* Enclosure. Default '"'*/` |
|      - | 4305 | `	io_private *pDev; /* Open stream handle */` |
|      - | 4306 | `	int iCount;       /* Counter */` |
|      - | 4307 | `};` |
|      - | 4308 | `/*` |
|      - | 4309 | ` * The following callback is used by the fputcsv() function inorder to iterate` |
|      - | 4310 | ` * throw array entries and output CSV data based on the current key and it's` |
|      - | 4311 | ` * associated data.` |
|      - | 4312 | ` */` |
|      6 | 4313 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 4314 |  |
|      7 | 4315 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 4316 | `	const char *zData;` |
|      - | 4317 | `	int nLen,c2;` |
|      - | 4318 | `	sxu32 n;` |
|      - | 4319 | `	/* Point to the raw data */` |
|      7 | 4320 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      7 | 4321 | `	if( nLen < 1 ){` |
|      - | 4322 | `		/* Nothing to write */` |
|    ! 0 | 4323 | `		return PH7_OK;` |
|      - | 4324 | `	}` |
|      7 | 4325 | `	if( pData->iCount > 0 ){` |
|      - | 4326 | `		/* Write the delimiter */` |
|      5 | 4327 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->delimiter,sizeof(char));` |
|      2 | 4328 | `	}` |
|      7 | 4329 | `	n = 1;` |
|      7 | 4330 | `	c2 = 0;` |
|     10 | 4331 | `	if( SyByteFind(zData,(sxu32)nLen,pData->delimiter,0) == SXRET_OK \|\|` |
|      6 | 4332 | `		SyByteFind(zData,(sxu32)nLen,pData->enclosure,&n) == SXRET_OK ){` |
|    ! 0 | 4333 | `			c2 = 1;` |
|    ! 0 | 4334 | `			if( n == 0 ){` |
|    ! 0 | 4335 | `				c2 = 2;` |
|    ! 0 | 4336 | `			}` |
|      - | 4337 | `			/* Write the enclosure */` |
|    ! 0 | 4338 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4339 | `			if( c2 > 1 ){` |
|    ! 0 | 4340 | `				pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4341 | `			}` |
|    ! 0 | 4342 | `	}` |
|      - | 4343 | `	/* Write the data */` |
|      7 | 4344 | `	if( pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)zData,(ph7_int64)nLen) < 1 ){` |
|    ! 0 | 4345 | `		SXUNUSED(pKey); /* cc warning */` |
|    ! 0 | 4346 | `		return PH7_ABORT;` |
|      - | 4347 | `	}` |
|      7 | 4348 | `	if( c2 > 0 ){` |
|      - | 4349 | `		/* Write the enclosure */` |
|    ! 0 | 4350 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4351 | `		if( c2 > 1 ){` |
|    ! 0 | 4352 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4353 | `		}` |
|    ! 0 | 4354 | `	}` |
|      7 | 4355 | `	pData->iCount++;` |
|      7 | 4356 | `	return PH7_OK;` |
|      4 | 4357 |  |
|      - | 4358 | `/*` |
|      - | 4359 | ` * int fputcsv(resource $handle,array $fields[,string $delimiter = ','[,string $enclosure = '"' ]])` |
|      - | 4360 | ` *  Format line as CSV and write to file pointer.` |
|      - | 4361 | ` * Parameters` |
|      - | 4362 | ` *  $handle` |
|      - | 4363 | ` *   Open file handle.` |
|      - | 4364 | ` * $fields` |
|      - | 4365 | ` *   An array of values.` |
|      - | 4366 | ` * $delimiter` |
|      - | 4367 | ` *   The optional delimiter parameter sets the field delimiter (one character only).` |
|      - | 4368 | ` * $enclosure` |
|      - | 4369 | ` *  The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 4370 | ` */` |
|      2 | 4371 | `static int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4372 |  |
|      - | 4373 | `	const ph7_io_stream *pStream;` |
|      - | 4374 | `	struct csv_data sCsv;` |
|      - | 4375 | `	io_private *pDev;` |
|      - | 4376 | `	char *zEol;` |
|      - | 4377 | `	int eolen;` |
|      3 | 4378 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4379 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4380 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|    ! 0 | 4381 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4382 | `		return PH7_OK;` |
|      - | 4383 | `	}` |
|      - | 4384 | `	/* Extract our private data */` |
|      3 | 4385 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4386 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4387 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4388 | `		/*Expecting an IO handle */` |
|    ! 0 | 4389 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4390 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4391 | `		return PH7_OK;` |
|      - | 4392 | `	}` |
|      - | 4393 | `	/* Point to the target IO stream device */` |
|      3 | 4394 | `	pStream = pDev->pStream;` |
|      3 | 4395 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4396 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4397 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4398 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4399 | `			);` |
|    ! 0 | 4400 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4401 | `		return PH7_OK;` |
|      - | 4402 | `	}` |
|      - | 4403 | `	/* Set default csv separator */` |
|      3 | 4404 | `	sCsv.delimiter = ',';` |
|      3 | 4405 | `	sCsv.enclosure = '"';` |
|      3 | 4406 | `	sCsv.pDev = pDev;` |
|      3 | 4407 | `	sCsv.iCount = 0;` |
|      3 | 4408 | `	if( nArg > 2 ){` |
|      - | 4409 | `		/* User delimiter */` |
|      - | 4410 | `		const char *z;` |
|      - | 4411 | `		int n;` |
|      3 | 4412 | `		z = ph7_value_to_string(apArg[2],&n);` |
|      3 | 4413 | `		if( n > 0 ){` |
|      3 | 4414 | `			sCsv.delimiter = z[0];` |
|      1 | 4415 | `		}` |
|      3 | 4416 | `		if( nArg > 3 ){` |
|      3 | 4417 | `			z = ph7_value_to_string(apArg[3],&n);` |
|      3 | 4418 | `			if( n > 0 ){` |
|      3 | 4419 | `				sCsv.enclosure = z[0];` |
|      1 | 4420 | `			}` |
|      1 | 4421 | `		}` |
|      1 | 4422 | `	}` |
|      - | 4423 | `	/* Iterate throw array entries and write csv data */` |
|      3 | 4424 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      - | 4425 | `	/* Write a line ending */` |
|      - | 4426 | `#ifdef __WINNT__` |
|      1 | 4427 | `	zEol = "\r\n";` |
|      1 | 4428 | `	eolen = (int)sizeof("\r\n")-1;` |
|      - | 4429 | `#else` |
|      - | 4430 | `	/* Assume UNIX LF */` |
|      2 | 4431 | `	zEol = "\n";` |
|      2 | 4432 | `	eolen = (int)sizeof(char);` |
|      - | 4433 | `#endif` |
|      3 | 4434 | `	pDev->pStream->xWrite(pDev->pHandle,(const void *)zEol,eolen);` |
|      3 | 4435 | `	return PH7_OK;` |
|      2 | 4436 |  |
|      - | 4437 | `/*` |
|      - | 4438 | ` * fprintf,vfprintf private data.` |
|      - | 4439 | ` * An instance of the following structure is passed to the formatted` |
|      - | 4440 | ` * input consumer callback defined below.` |
|      - | 4441 | ` */` |
|      - | 4442 | `typedef struct fprintf_data fprintf_data;` |
|      - | 4443 | `struct fprintf_data` |
|      - | 4444 |  |
|      - | 4445 | `	io_private *pIO;        /* IO stream */` |
|      - | 4446 | `	ph7_int64 nCount;       /* Total number of bytes written */` |
|      - | 4447 | `};` |
|      - | 4448 | `/*` |
|      - | 4449 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 4450 | ` */` |
|     38 | 4451 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4452 |  |
|     39 | 4453 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 4454 | `	ph7_int64 n;` |
|      - | 4455 | `	/* Write the formatted data */` |
|     39 | 4456 | `	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);` |
|     39 | 4457 | `	if( n < 1 ){` |
|    ! 0 | 4458 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 4459 | `		/* IO error,abort immediately */` |
|    ! 0 | 4460 | `		return SXERR_ABORT;` |
|      - | 4461 | `	}` |
|      - | 4462 | `	/* Increment counter */` |
|     39 | 4463 | `	pFdata->nCount += n;` |
|     39 | 4464 | `	return PH7_OK;` |
|     20 | 4465 |  |
|      - | 4466 | `/*` |
|      - | 4467 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 4468 | ` *  Write a formatted string to a stream.` |
|      - | 4469 | ` * Parameters` |
|      - | 4470 | ` *  $handle` |
|      - | 4471 | ` *   The file pointer.` |
|      - | 4472 | ` *  $format` |
|      - | 4473 | ` *   String format (see sprintf()).` |
|      - | 4474 | ` * Return` |
|      - | 4475 | ` *  The length of the written string.` |
|      - | 4476 | ` */` |
|     16 | 4477 | `static int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4478 |  |
|      - | 4479 | `	fprintf_data sFdata;` |
|      - | 4480 | `	const char *zFormat;` |
|      - | 4481 | `	io_private *pDev;` |
|      - | 4482 | `	int nLen;` |
|     17 | 4483 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 4484 | `		/* Missing/Invalid arguments,return zero */` |
|    ! 0 | 4485 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4486 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4487 | `		return PH7_OK;` |
|      - | 4488 | `	}` |
|      - | 4489 | `	/* Extract our private data */` |
|     17 | 4490 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4491 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     17 | 4492 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4493 | `		/*Expecting an IO handle */` |
|    ! 0 | 4494 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4495 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4496 | `		return PH7_OK;` |
|      - | 4497 | `	}` |
|      - | 4498 | `	/* Point to the target IO stream device */` |
|     17 | 4499 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4500 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4501 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4502 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4503 | `			);` |
|    ! 0 | 4504 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4505 | `		return PH7_OK;` |
|      - | 4506 | `	}` |
|      - | 4507 | `	/* Extract the string format */` |
|     17 | 4508 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     17 | 4509 | `	if( nLen < 1 ){` |
|      - | 4510 | `		/* Empty string,return zero */` |
|    ! 0 | 4511 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4512 | `		return PH7_OK;` |
|      - | 4513 | `	}` |
|      - | 4514 | `	/* Prepare our private data */` |
|     17 | 4515 | `	sFdata.nCount = 0;` |
|     17 | 4516 | `	sFdata.pIO = pDev;` |
|      - | 4517 | `	/* Format the string */` |
|     17 | 4518 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 4519 | `	/* Return total number of bytes written */` |
|     17 | 4520 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|     17 | 4521 | `	return PH7_OK;` |
|      9 | 4522 |  |
|      - | 4523 | `/*` |
|      - | 4524 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 4525 | ` *  Write a formatted string to a stream.` |
|      - | 4526 | ` * Parameters` |
|      - | 4527 | ` *  $handle` |
|      - | 4528 | ` *   The file pointer.` |
|      - | 4529 | ` *  $format` |
|      - | 4530 | ` *   String format (see sprintf()).` |
|      - | 4531 | ` * $args` |
|      - | 4532 | ` *   User arguments.` |
|      - | 4533 | ` * Return` |
|      - | 4534 | ` *  The length of the written string.` |
|      - | 4535 | ` */` |
|      4 | 4536 | `static int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4537 |  |
|      - | 4538 | `	fprintf_data sFdata;` |
|      - | 4539 | `	const char *zFormat;` |
|      - | 4540 | `	ph7_hashmap *pMap;` |
|      - | 4541 | `	io_private *pDev;` |
|      - | 4542 | `	SySet sArg;` |
|      - | 4543 | `	int n,nLen;` |
|      5 | 4544 | `	if( nArg < 3 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_string(apArg[1])  \|\| !ph7_value_is_array(apArg[2]) ){` |
|      - | 4545 | `		/* Missing/Invalid arguments,return zero */` |
|      3 | 4546 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|      3 | 4547 | `		ph7_result_int(pCtx,0);` |
|      3 | 4548 | `		return PH7_OK;` |
|      - | 4549 | `	}` |
|      - | 4550 | `	/* Extract our private data */` |
|      3 | 4551 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4552 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4553 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4554 | `		/*Expecting an IO handle */` |
|    ! 0 | 4555 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4556 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4557 | `		return PH7_OK;` |
|      - | 4558 | `	}` |
|      - | 4559 | `	/* Point to the target IO stream device */` |
|      3 | 4560 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4561 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4562 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4563 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4564 | `			);` |
|    ! 0 | 4565 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4566 | `		return PH7_OK;` |
|      - | 4567 | `	}` |
|      - | 4568 | `	/* Extract the string format */` |
|      3 | 4569 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      3 | 4570 | `	if( nLen < 1 ){` |
|      - | 4571 | `		/* Empty string,return zero */` |
|    ! 0 | 4572 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4573 | `		return PH7_OK;` |
|      - | 4574 | `	}` |
|      - | 4575 | `	/* Point to hashmap */` |
|      3 | 4576 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 4577 | `	/* Extract arguments from the hashmap */` |
|      3 | 4578 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4579 | `	/* Prepare our private data */` |
|      3 | 4580 | `	sFdata.nCount = 0;` |
|      3 | 4581 | `	sFdata.pIO = pDev;` |
|      - | 4582 | `	/* Format the string */` |
|      3 | 4583 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 4584 | `	/* Return total number of bytes written*/` |
|      3 | 4585 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      3 | 4586 | `	SySetRelease(&sArg);` |
|      3 | 4587 | `	return PH7_OK;` |
|      3 | 4588 |  |
|      - | 4589 | `/*` |
|      - | 4590 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 4591 | ` * According to the PHP reference manual:` |
|      - | 4592 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 4593 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 4594 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 4595 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 4596 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 4597 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 4598 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 4599 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 4600 | ` *         exist, attempt to create it.` |
|      - | 4601 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 4602 | ` *          not exist, attempt to create it.` |
|      - | 4603 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 4604 | ` *         already exists,` |
|      - | 4605 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 4606 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 4607 | ` *         the underlying open(2) system call.` |
|      - | 4608 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 4609 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 4610 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 4611 | ` *          is positioned on the beginning of the file.` |
|      - | 4612 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 4613 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 4614 | ` *          be used after the lock is requested).` |
|      - | 4615 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 4616 | ` */` |
|     62 | 4617 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      1 | 4618 |  |
|     63 | 4619 | `	const char *zEnd = &zMode[nLen];` |
|     63 | 4620 | `	int iFlag = 0;` |
|      - | 4621 | `	int c;` |
|     63 | 4622 | `	if( nLen < 1 ){` |
|      - | 4623 | `		/* Open in a read-only mode */` |
|    ! 0 | 4624 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 4625 | `	}` |
|     63 | 4626 | `	c = zMode[0];` |
|     63 | 4627 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 4628 | `		/* Read-only access */` |
|     37 | 4629 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|     37 | 4630 | `		zMode++; /* Advance */` |
|     37 | 4631 | `		if( zMode < zEnd ){` |
|      7 | 4632 | `			c = zMode[0];` |
|      7 | 4633 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 4634 | `				/* Read+Write access */` |
|      7 | 4635 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|      3 | 4636 | `			}` |
|      4 | 4637 | `		}` |
|     45 | 4638 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 4639 | `		/* Overwrite mode.` |
|      - | 4640 | `		 * If the file does not exists,try to create it` |
|      - | 4641 | `		 */` |
|     27 | 4642 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     27 | 4643 | `		zMode++; /* Advance */` |
|     27 | 4644 | `		if( zMode < zEnd ){` |
|      3 | 4645 | `			c = zMode[0];` |
|      3 | 4646 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4647 | `				/* Read+Write access */` |
|      3 | 4648 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|      3 | 4649 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|      1 | 4650 | `			}` |
|      2 | 4651 | `		}` |
|     13 | 4652 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 4653 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 4654 | `		 * Create the file if it does not exists.` |
|      - | 4655 | `		 */` |
|    ! 0 | 4656 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4657 | `		zMode++; /* Advance */` |
|    ! 0 | 4658 | `		if( zMode < zEnd ){` |
|    ! 0 | 4659 | `			c = zMode[0];` |
|    ! 0 | 4660 | `			if( c == '+' ){` |
|      - | 4661 | `				/* Read-Write access */` |
|    ! 0 | 4662 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4663 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4664 | `			}` |
|    ! 0 | 4665 | `		}` |
|    ! 0 | 4666 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 4667 | `		/* Exclusive access.` |
|      - | 4668 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 4669 | `		 * Otherwise create a new file.` |
|      - | 4670 | `		 */` |
|    ! 0 | 4671 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|    ! 0 | 4672 | `		zMode++; /* Advance */` |
|    ! 0 | 4673 | `		if( zMode < zEnd ){` |
|    ! 0 | 4674 | `			c = zMode[0];` |
|    ! 0 | 4675 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4676 | `				/* Read-Write access */` |
|    ! 0 | 4677 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4678 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4679 | `			}` |
|    ! 0 | 4680 | `		}` |
|    ! 0 | 4681 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 4682 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 4683 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4684 | `		zMode++; /* Advance */` |
|    ! 0 | 4685 | `		if( zMode < zEnd ){` |
|    ! 0 | 4686 | `			c = zMode[0];` |
|    ! 0 | 4687 | `			if( c == '+' ){` |
|      - | 4688 | `				/* Read-Write access */` |
|    ! 0 | 4689 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4690 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4691 | `			}` |
|    ! 0 | 4692 | `		}` |
|    ! 0 | 4693 | `	}else{` |
|      - | 4694 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 4695 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 4696 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 4697 | `	}` |
|     71 | 4698 | `	while( zMode < zEnd ){` |
|      9 | 4699 | `		c = zMode[0];` |
|      9 | 4700 | `		if( c == 'b' \|\| c == 'B' ){` |
|    ! 0 | 4701 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4702 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|      9 | 4703 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 4704 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 4705 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4706 | `		}` |
|      9 | 4707 | `		zMode++;` |
|      1 | 4708 | `	}` |
|     63 | 4709 | `	return iFlag;` |
|     32 | 4710 |  |
|      - | 4711 | `/*` |
|      - | 4712 | ` * Initialize the IO private structure.` |
|      - | 4713 | ` */` |
|    104 | 4714 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      1 | 4715 |  |
|    105 | 4716 | `	pOut->pStream = pStream;` |
|    105 | 4717 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|    105 | 4718 | `	pOut->nOfft = 0;` |
|      - | 4719 | `	/* Set the magic number */` |
|    105 | 4720 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|    105 | 4721 |  |
|      - | 4722 | `/*` |
|      - | 4723 | ` * Release the IO private structure.` |
|      - | 4724 | ` */` |
|     94 | 4725 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      1 | 4726 |  |
|     95 | 4727 | `	SyBlobRelease(&pDev->sBuffer);` |
|     95 | 4728 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 4729 | `	/* Release the whole structure */` |
|     95 | 4730 | `	ph7_context_free_chunk(pCtx,pDev);` |
|     95 | 4731 |  |
|      - | 4732 | `/*` |
|      - | 4733 | ` * Reset the IO private structure.` |
|      - | 4734 | ` */` |
|     12 | 4735 | `static void ResetIOPrivate(io_private *pDev)` |
|      1 | 4736 |  |
|     13 | 4737 | `	SyBlobReset(&pDev->sBuffer);` |
|     13 | 4738 | `	pDev->nOfft = 0;` |
|     13 | 4739 |  |
|      - | 4740 | `/* Forward declaration */` |
|      - | 4741 | `static int is_php_stream(const ph7_io_stream *pStream);` |
|      - | 4742 | `/*` |
|      - | 4743 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 4744 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 4745 | ` * Parameters` |
|      - | 4746 | ` *  $filename` |
|      - | 4747 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 4748 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 4749 | ` *   then a regular file is assumed.` |
|      - | 4750 | ` *  $mode` |
|      - | 4751 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 4752 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 4753 | ` *   modes.` |
|      - | 4754 | ` *  $use_include_path` |
|      - | 4755 | ` *   You can use the optional second parameter and set it to` |
|      - | 4756 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 4757 | ` *  $context` |
|      - | 4758 | ` *   A context stream resource.` |
|      - | 4759 | ` * Return` |
|      - | 4760 | ` *  File handle on success or FALSE on failure.` |
|      - | 4761 | ` */` |
|     62 | 4762 | `static int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4763 |  |
|      - | 4764 | `	const ph7_io_stream *pStream;` |
|      - | 4765 | `	const char *zUri,*zMode;` |
|      - | 4766 | `	ph7_value *pResource;` |
|      - | 4767 | `	io_private *pDev;` |
|      - | 4768 | `	int iLen,imLen;` |
|      - | 4769 | `	int iOpenFlags;` |
|     63 | 4770 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4771 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4772 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 4773 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4774 | `		return PH7_OK;` |
|      - | 4775 | `	}` |
|      - | 4776 | `	/* Extract the URI and the desired access mode */` |
|     63 | 4777 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|     63 | 4778 | `	if( nArg > 1 ){` |
|     63 | 4779 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|     32 | 4780 | `	}else{` |
|      - | 4781 | `		/* Set a default read-only mode */` |
|    ! 0 | 4782 | `		zMode = "r";` |
|    ! 0 | 4783 | `		imLen = (int)sizeof(char);` |
|      - | 4784 | `	}` |
|      - | 4785 | `	/* Try to extract a stream */` |
|     63 | 4786 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|     63 | 4787 | `	if( pStream == 0 ){` |
|    ! 0 | 4788 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 4789 | `			"No stream device is associated with the given URI(%s)",zUri);` |
|    ! 0 | 4790 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4791 | `		return PH7_OK;` |
|      - | 4792 | `	}` |
|      - | 4793 | `	/* Allocate a new IO private instance */` |
|     63 | 4794 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     63 | 4795 | `	if( pDev == 0 ){` |
|    ! 0 | 4796 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4797 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4798 | `		return PH7_OK;` |
|      - | 4799 | `	}` |
|     63 | 4800 | `	pResource = 0;` |
|     63 | 4801 | `	if( nArg > 3 ){` |
|    ! 0 | 4802 | `		pResource = apArg[3];` |
|     63 | 4803 | `	}else if( is_php_stream(pStream) ){` |
|      - | 4804 | `		/* TICKET 1433-80: The php:// stream need a ph7_value to access the underlying` |
|      - | 4805 | `		 * virtual machine.` |
|      - | 4806 | `		 */` |
|      3 | 4807 | `		pResource = apArg[0];` |
|      1 | 4808 | `	}` |
|      - | 4809 | `	/* Initialize the structure */` |
|     63 | 4810 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 4811 | `	/* Convert open mode to PH7 flags */` |
|     63 | 4812 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|      - | 4813 | `	/* Try to get a handle */` |
|     94 | 4814 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|     31 | 4815 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);` |
|     63 | 4816 | `	if( pDev->pHandle == 0 ){` |
|    ! 0 | 4817 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zUri);` |
|    ! 0 | 4818 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4819 | `		ph7_context_free_chunk(pCtx,pDev);` |
|    ! 0 | 4820 | `		return PH7_OK;` |
|      - | 4821 | `	}` |
|      - | 4822 | `	/* All done,return the io_private instance as a resource */` |
|     63 | 4823 | `	ph7_result_resource(pCtx,pDev);` |
|     63 | 4824 | `	return PH7_OK;` |
|     32 | 4825 |  |
|      - | 4826 | `/*` |
|      - | 4827 | ` * bool fclose(resource $handle)` |
|      - | 4828 | ` *  Closes an open file pointer` |
|      - | 4829 | ` * Parameters` |
|      - | 4830 | ` *  $handle` |
|      - | 4831 | ` *   The file pointer.` |
|      - | 4832 | ` * Return` |
|      - | 4833 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4834 | ` */` |
|     76 | 4835 | `static int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4836 |  |
|      - | 4837 | `	const ph7_io_stream *pStream;` |
|      - | 4838 | `	io_private *pDev;` |
|      - | 4839 | `	ph7_vm *pVm;` |
|     77 | 4840 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4841 | `		/* Missing/Invalid arguments,return FALSE */` |
|      3 | 4842 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|      3 | 4843 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4844 | `		return PH7_OK;` |
|      - | 4845 | `	}` |
|      - | 4846 | `	/* Extract our private data */` |
|     75 | 4847 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4848 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     75 | 4849 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4850 | `		/*Expecting an IO handle */` |
|    ! 0 | 4851 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4852 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4853 | `		return PH7_OK;` |
|      - | 4854 | `	}` |
|      - | 4855 | `	/* Point to the target IO stream device */` |
|     75 | 4856 | `	pStream = pDev->pStream;` |
|     75 | 4857 | `	if( pStream == 0 ){` |
|    ! 0 | 4858 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4859 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4860 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4861 | `			);` |
|    ! 0 | 4862 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4863 | `		return PH7_OK;` |
|      - | 4864 | `	}` |
|      - | 4865 | `	/* Point to the VM that own this context */` |
|     75 | 4866 | `	pVm = pCtx->pVm;` |
|      - | 4867 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|     75 | 4868 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 4869 | `		/* Perform the requested operation */` |
|     75 | 4870 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 4871 | `		/* Release the IO private structure */` |
|     75 | 4872 | `		ReleaseIOPrivate(pCtx,pDev);` |
|      - | 4873 | `		/* Invalidate the resource handle */` |
|     75 | 4874 | `		ph7_value_release(apArg[0]);` |
|     37 | 4875 | `	}` |
|      - | 4876 | `	/* Return TRUE */` |
|     75 | 4877 | `	ph7_result_bool(pCtx,1);` |
|     75 | 4878 | `	return PH7_OK;` |
|     39 | 4879 |  |
|      - | 4880 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4881 | `/*` |
|      - | 4882 | ` * MD5/SHA1 digest consumer.` |
|      - | 4883 | ` */` |
|     72 | 4884 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 4885 |  |
|      - | 4886 | `	/* Append hex chunk verbatim */` |
|     73 | 4887 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     73 | 4888 | `	return SXRET_OK;` |
|      1 | 4889 |  |
|      - | 4890 | `/*` |
|      - | 4891 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 4892 | ` *  Calculates the md5 hash of a given file.` |
|      - | 4893 | ` * Parameters` |
|      - | 4894 | ` *  $uri` |
|      - | 4895 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 4896 | ` *  $raw_output` |
|      - | 4897 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 4898 | ` * Return` |
|      - | 4899 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 4900 | ` */` |
|      2 | 4901 | `static int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4902 |  |
|      - | 4903 | `	const ph7_io_stream *pStream;` |
|      - | 4904 | `	unsigned char zDigest[16];` |
|      3 | 4905 | `	int raw_output  = FALSE;` |
|      - | 4906 | `	const char *zFile;` |
|      - | 4907 | `	MD5Context sCtx;` |
|      - | 4908 | `	char zBuf[8192];` |
|      - | 4909 | `	void *pHandle;` |
|      - | 4910 | `	ph7_int64 n;` |
|      - | 4911 | `	int nLen;` |
|      3 | 4912 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4913 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4914 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 4915 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4916 | `		return PH7_OK;` |
|      - | 4917 | `	}` |
|      - | 4918 | `	/* Extract the file path */` |
|      3 | 4919 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4920 | `	/* Point to the target IO stream device */` |
|      3 | 4921 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4922 | `	if( pStream == 0 ){` |
|    ! 0 | 4923 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4924 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4925 | `		return PH7_OK;` |
|      - | 4926 | `	}` |
|      3 | 4927 | `	if( nArg > 1 ){` |
|    ! 0 | 4928 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 4929 | `	}` |
|      - | 4930 | `	/* Try to open the file in read-only mode */` |
|      3 | 4931 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 4932 | `	if( pHandle == 0 ){` |
|    ! 0 | 4933 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 4934 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4935 | `		return PH7_OK;` |
|      - | 4936 | `	}` |
|      - | 4937 | `	/* Init the MD5 context */` |
|      3 | 4938 | `	MD5Init(&sCtx);` |
|      - | 4939 | `	/* Perform the requested operation */` |
|      2 | 4940 | `	for(;;){` |
|      5 | 4941 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 4942 | `		if( n < 1 ){` |
|      - | 4943 | `			/* EOF or IO error,break immediately */` |
|      3 | 4944 | `			break;` |
|      - | 4945 | `		}` |
|      3 | 4946 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 4947 | `	}` |
|      - | 4948 | `	/* Close the stream */` |
|      3 | 4949 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 4950 | `	/* Extract the digest */` |
|      3 | 4951 | `	MD5Final(zDigest,&sCtx);` |
|      3 | 4952 | `	if( raw_output ){` |
|      - | 4953 | `		/* Output raw digest */` |
|    ! 0 | 4954 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 4955 | `	}else{` |
|      - | 4956 | `		/* Perform a binary to hex conversion */` |
|      3 | 4957 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 4958 | `	}` |
|      3 | 4959 | `	return PH7_OK;` |
|      2 | 4960 |  |
|      - | 4961 | `/*` |
|      - | 4962 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 4963 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 4964 | ` * Parameters` |
|      - | 4965 | ` *  $uri` |
|      - | 4966 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 4967 | ` *  $raw_output` |
|      - | 4968 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 4969 | ` * Return` |
|      - | 4970 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 4971 | ` */` |
|      2 | 4972 | `static int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4973 |  |
|      - | 4974 | `	const ph7_io_stream *pStream;` |
|      - | 4975 | `	unsigned char zDigest[20];` |
|      3 | 4976 | `	int raw_output  = FALSE;` |
|      - | 4977 | `	const char *zFile;` |
|      - | 4978 | `	SHA1Context sCtx;` |
|      - | 4979 | `	char zBuf[8192];` |
|      - | 4980 | `	void *pHandle;` |
|      - | 4981 | `	ph7_int64 n;` |
|      - | 4982 | `	int nLen;` |
|      3 | 4983 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4984 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4985 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 4986 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4987 | `		return PH7_OK;` |
|      - | 4988 | `	}` |
|      - | 4989 | `	/* Extract the file path */` |
|      3 | 4990 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4991 | `	/* Point to the target IO stream device */` |
|      3 | 4992 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4993 | `	if( pStream == 0 ){` |
|    ! 0 | 4994 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4995 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4996 | `		return PH7_OK;` |
|      - | 4997 | `	}` |
|      3 | 4998 | `	if( nArg > 1 ){` |
|    ! 0 | 4999 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 5000 | `	}` |
|      - | 5001 | `	/* Try to open the file in read-only mode */` |
|      3 | 5002 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5003 | `	if( pHandle == 0 ){` |
|    ! 0 | 5004 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5005 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5006 | `		return PH7_OK;` |
|      - | 5007 | `	}` |
|      - | 5008 | `	/* Init the SHA1 context */` |
|      3 | 5009 | `	SHA1Init(&sCtx);` |
|      - | 5010 | `	/* Perform the requested operation */` |
|      2 | 5011 | `	for(;;){` |
|      5 | 5012 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5013 | `		if( n < 1 ){` |
|      - | 5014 | `			/* EOF or IO error,break immediately */` |
|      3 | 5015 | `			break;` |
|      - | 5016 | `		}` |
|      3 | 5017 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5018 | `	}` |
|      - | 5019 | `	/* Close the stream */` |
|      3 | 5020 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5021 | `	/* Extract the digest */` |
|      3 | 5022 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 5023 | `	if( raw_output ){` |
|      - | 5024 | `		/* Output raw digest */` |
|    ! 0 | 5025 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5026 | `	}else{` |
|      - | 5027 | `		/* Perform a binary to hex conversion */` |
|      3 | 5028 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5029 | `	}` |
|      3 | 5030 | `	return PH7_OK;` |
|      2 | 5031 |  |
|      - | 5032 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5033 | `/*` |
|      - | 5034 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 5035 | ` *  Parse a configuration file.` |
|      - | 5036 | ` * Parameters` |
|      - | 5037 | ` * $filename` |
|      - | 5038 | ` *  The filename of the ini file being parsed.` |
|      - | 5039 | ` * $process_sections` |
|      - | 5040 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 5041 | ` *  with the section names and settings included.` |
|      - | 5042 | ` *  The default for process_sections is FALSE.` |
|      - | 5043 | ` * $scanner_mode` |
|      - | 5044 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 5045 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 5046 | ` * Return` |
|      - | 5047 | ` *  The settings are returned as an associative array on success.` |
|      - | 5048 | ` *  Otherwise is returned.` |
|      - | 5049 | ` */` |
|      2 | 5050 | `static int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5051 |  |
|      - | 5052 | `	const ph7_io_stream *pStream;` |
|      - | 5053 | `	const char *zFile;` |
|      - | 5054 | `	SyBlob sContents;` |
|      - | 5055 | `	void *pHandle;` |
|      - | 5056 | `	int nLen;` |
|      3 | 5057 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5058 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5059 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5060 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5061 | `		return PH7_OK;` |
|      - | 5062 | `	}` |
|      - | 5063 | `	/* Extract the file path */` |
|      3 | 5064 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5065 | `	/* Point to the target IO stream device */` |
|      3 | 5066 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5067 | `	if( pStream == 0 ){` |
|    ! 0 | 5068 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5069 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5070 | `		return PH7_OK;` |
|      - | 5071 | `	}` |
|      - | 5072 | `	/* Try to open the file in read-only mode */` |
|      3 | 5073 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5074 | `	if( pHandle == 0 ){` |
|    ! 0 | 5075 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5076 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5077 | `		return PH7_OK;` |
|      - | 5078 | `	}` |
|      3 | 5079 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 5080 | `	/* Read the whole file */` |
|      3 | 5081 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      3 | 5082 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 5083 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 5084 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5085 | `	}else{` |
|      - | 5086 | `		/* Process the raw INI buffer */` |
|      5 | 5087 | `		PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      2 | 5088 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0);` |
|      - | 5089 | `	}` |
|      - | 5090 | `	/* Close the stream */` |
|      3 | 5091 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5092 | `	/* Release the working buffer */` |
|      3 | 5093 | `	SyBlobRelease(&sContents);` |
|      3 | 5094 | `	return PH7_OK;` |
|      2 | 5095 |  |
|      - | 5096 | `/*` |
|      - | 5097 | ` * Section:` |
|      - | 5098 | ` *    ZIP archive processing.` |
|      - | 5099 | ` * Status:` |
|      - | 5100 | ` *    Stable.` |
|      - | 5101 | ` */` |
|      - | 5102 | `typedef struct zip_raw_data zip_raw_data;` |
|      - | 5103 | `struct zip_raw_data` |
|      - | 5104 |  |
|      - | 5105 | `	int iType;         /* Where the raw data is stored */` |
|      - | 5106 | `	union raw_data{` |
|      - | 5107 | `		struct mmap_data{` |
|      - | 5108 | `			void *pMap;          /* Memory mapped data */` |
|      - | 5109 | `			ph7_int64 nSize;     /* Map size */` |
|      - | 5110 | `			const ph7_vfs *pVfs; /* Underlying vfs */` |
|      - | 5111 | `		}mmap;` |
|      - | 5112 | `		SyBlob sBlob;  /* Memory buffer */` |
|      - | 5113 | `	}raw;` |
|      - | 5114 | `};` |
|      - | 5115 | `#define ZIP_RAW_DATA_MMAPED 1 /* Memory mapped ZIP raw data */` |
|      - | 5116 | `#define ZIP_RAW_DATA_MEMBUF 2 /* ZIP raw data stored in a dynamically` |
|      - | 5117 | `                               * allocated memory chunk.` |
|      - | 5118 | `							   */` |
|      - | 5119 | ` /*` |
|      - | 5120 | `  * mixed zip_open(string $filename)` |
|      - | 5121 | `  *  Opens a new zip archive for reading.` |
|      - | 5122 | `  * Parameters` |
|      - | 5123 | `  *  $filename` |
|      - | 5124 | `  *   The file name of the ZIP archive to open.` |
|      - | 5125 | `  * Return` |
|      - | 5126 | `  *  A resource handle for later use with zip_read() and zip_close() or FALSE on failure.` |
|      - | 5127 | `  */` |
|     30 | 5128 | `static int PH7_builtin_zip_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5129 |  |
|      - | 5130 | `	const ph7_io_stream *pStream;` |
|      - | 5131 | `	SyArchive *pArchive;` |
|      - | 5132 | `	zip_raw_data *pRaw;` |
|      - | 5133 | `	const char *zFile;` |
|      - | 5134 | `	SyBlob *pContents;` |
|      - | 5135 | `	void *pHandle;` |
|      - | 5136 | `	int nLen;` |
|      - | 5137 | `	sxi32 rc;` |
|     31 | 5138 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5139 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5140 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5141 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5142 | `		return PH7_OK;` |
|      - | 5143 | `	}` |
|      - | 5144 | `	/* Extract the file path */` |
|     31 | 5145 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5146 | `	/* Point to the target IO stream device */` |
|     31 | 5147 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     31 | 5148 | `	if( pStream == 0 ){` |
|    ! 0 | 5149 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5150 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5151 | `		return PH7_OK;` |
|      - | 5152 | `	}` |
|      - | 5153 | `	/* Create an in-memory archive */` |
|     31 | 5154 | `	pArchive = (SyArchive *)ph7_context_alloc_chunk(pCtx,sizeof(SyArchive)+sizeof(zip_raw_data),TRUE,FALSE);` |
|     31 | 5155 | `	if( pArchive == 0 ){` |
|    ! 0 | 5156 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"PH7 is running out of memory");` |
|    ! 0 | 5157 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5158 | `		return PH7_OK;` |
|      - | 5159 | `	}` |
|     31 | 5160 | `	pRaw = (zip_raw_data *)&pArchive[1];` |
|      - | 5161 | `	/* Initialize the archive */` |
|     31 | 5162 | `	SyArchiveInit(pArchive,&pCtx->pVm->sAllocator,0,0);` |
|      - | 5163 | `	/* Extract the default stream */` |
|     31 | 5164 | `	if( pStream == pCtx->pVm->pDefStream /* file:// stream*/){` |
|      - | 5165 | `		const ph7_vfs *pVfs;` |
|      - | 5166 | `		/* Try to get a memory view of the whole file since ZIP files` |
|      - | 5167 | `		 * tends to be very big this days,this is a huge performance win.` |
|      - | 5168 | `		 */` |
|     31 | 5169 | `		pVfs = PH7_ExportBuiltinVfs();` |
|     31 | 5170 | `		if( pVfs && pVfs->xMmap ){` |
|     31 | 5171 | `			rc = pVfs->xMmap(zFile,&pRaw->raw.mmap.pMap,&pRaw->raw.mmap.nSize);` |
|     31 | 5172 | `			if( rc == PH7_OK ){` |
|      - | 5173 | `				/* Nice,Extract the whole archive */` |
|     29 | 5174 | `				rc = SyZipExtractFromBuf(pArchive,(const char *)pRaw->raw.mmap.pMap,(sxu32)pRaw->raw.mmap.nSize);` |
|     29 | 5175 | `				if( rc != SXRET_OK ){` |
|     15 | 5176 | `					if( pVfs->xUnmap ){` |
|     15 | 5177 | `						pVfs->xUnmap(pRaw->raw.mmap.pMap,pRaw->raw.mmap.nSize);` |
|      7 | 5178 | `					}` |
|      - | 5179 | `					/* Release the allocated chunk */` |
|     15 | 5180 | `					ph7_context_free_chunk(pCtx,pArchive);` |
|      - | 5181 | `					/* Something goes wrong with this ZIP archive,return FALSE */` |
|     15 | 5182 | `					ph7_result_bool(pCtx,0);` |
|     15 | 5183 | `					return PH7_OK;` |
|      - | 5184 | `				}` |
|      - | 5185 | `				/* Archive successfully opened */` |
|     15 | 5186 | `				pRaw->iType = ZIP_RAW_DATA_MMAPED;` |
|     15 | 5187 | `				pRaw->raw.mmap.pVfs = pVfs;` |
|     15 | 5188 | `				goto success;` |
|      - | 5189 | `			}` |
|      1 | 5190 | `		}` |
|      - | 5191 | `		/* FALL THROUGH */` |
|      1 | 5192 | `	}` |
|      - | 5193 | `	/* Try to open the file in read-only mode */` |
|      3 | 5194 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5195 | `	if( pHandle == 0 ){` |
|      3 | 5196 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|      3 | 5197 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5198 | `		return PH7_OK;` |
|      - | 5199 | `	}` |
|    ! 0 | 5200 | `	pContents = &pRaw->raw.sBlob;` |
|    ! 0 | 5201 | `	SyBlobInit(pContents,&pCtx->pVm->sAllocator);` |
|      - | 5202 | `	/* Read the whole file */` |
|    ! 0 | 5203 | `	PH7_StreamReadWholeFile(pHandle,pStream,pContents);` |
|      - | 5204 | `	/* Assume an invalid ZIP file */` |
|    ! 0 | 5205 | `	rc = SXERR_INVALID;` |
|    ! 0 | 5206 | `	if( SyBlobLength(pContents) > 0 ){` |
|      - | 5207 | `		/* Extract archive entries */` |
|    ! 0 | 5208 | `		rc = SyZipExtractFromBuf(pArchive,(const char *)SyBlobData(pContents),SyBlobLength(pContents));` |
|    ! 0 | 5209 | `	}` |
|    ! 0 | 5210 | `	pRaw->iType = ZIP_RAW_DATA_MEMBUF;` |
|      - | 5211 | `	/* Close the stream */` |
|    ! 0 | 5212 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|    ! 0 | 5213 | `	if( rc != SXRET_OK ){` |
|      - | 5214 | `		/* Release the working buffer */` |
|    ! 0 | 5215 | `		SyBlobRelease(pContents);` |
|      - | 5216 | `		/* Release the allocated chunk */` |
|    ! 0 | 5217 | `		ph7_context_free_chunk(pCtx,pArchive);` |
|      - | 5218 | `		/* Something goes wrong with this ZIP archive,return FALSE */` |
|    ! 0 | 5219 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5220 | `		return PH7_OK;` |
|      - | 5221 | `	}` |
|    ! 0 | 5222 | `success:` |
|      - | 5223 | `	/* Reset the loop cursor */` |
|     15 | 5224 | `	SyArchiveResetLoopCursor(pArchive);` |
|      - | 5225 | `	/* Return the in-memory archive as a resource handle */` |
|     15 | 5226 | `	ph7_result_resource(pCtx,pArchive);` |
|     15 | 5227 | `	return PH7_OK;` |
|     16 | 5228 |  |
|      - | 5229 | `/*` |
|      - | 5230 | `  * void zip_close(resource $zip)` |
|      - | 5231 | `  *  Close an in-memory ZIP archive.` |
|      - | 5232 | `  * Parameters` |
|      - | 5233 | `  *  $zip` |
|      - | 5234 | `  *   A ZIP file previously opened with zip_open().` |
|      - | 5235 | `  * Return` |
|      - | 5236 | `  *  null.` |
|      - | 5237 | `  */` |
|     14 | 5238 | `static int PH7_builtin_zip_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5239 |  |
|      - | 5240 | `	SyArchive *pArchive;` |
|      - | 5241 | `	zip_raw_data *pRaw;` |
|     15 | 5242 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5243 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5244 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|    ! 0 | 5245 | `		return PH7_OK;` |
|      - | 5246 | `	}` |
|      - | 5247 | `	/* Point to the in-memory archive */` |
|     15 | 5248 | `	pArchive = (SyArchive *)ph7_value_to_resource(apArg[0]);` |
|      - | 5249 | `	/* Make sure we are dealing with a valid ZIP archive */` |
|     15 | 5250 | `	if( SXARCH_INVALID(pArchive) ){` |
|    ! 0 | 5251 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|    ! 0 | 5252 | `		return PH7_OK;` |
|      - | 5253 | `	}` |
|      - | 5254 | `	/* Release the archive */` |
|     15 | 5255 | `	SyArchiveRelease(pArchive);` |
|     15 | 5256 | `	pRaw = (zip_raw_data *)&pArchive[1];` |
|     15 | 5257 | `	if( pRaw->iType == ZIP_RAW_DATA_MEMBUF ){` |
|    ! 0 | 5258 | `		SyBlobRelease(&pRaw->raw.sBlob);` |
|    ! 0 | 5259 | `	}else{` |
|     15 | 5260 | `		const ph7_vfs *pVfs = pRaw->raw.mmap.pVfs;` |
|     15 | 5261 | `		if( pVfs->xUnmap ){` |
|      - | 5262 | `			/* Unmap the memory view */` |
|     15 | 5263 | `			pVfs->xUnmap(pRaw->raw.mmap.pMap,pRaw->raw.mmap.nSize);` |
|      7 | 5264 | `		}` |
|      - | 5265 | `	}` |
|      - | 5266 | `	/* Release the memory chunk */` |
|     15 | 5267 | `	ph7_context_free_chunk(pCtx,pArchive);` |
|     15 | 5268 | `	return PH7_OK;` |
|      8 | 5269 |  |
|      - | 5270 | `/*` |
|      - | 5271 | `  * mixed zip_read(resource $zip)` |
|      - | 5272 | `  *  Reads the next entry from an in-memory ZIP archive.` |
|      - | 5273 | `  * Parameters` |
|      - | 5274 | `  *  $zip` |
|      - | 5275 | `  *   A ZIP file previously opened with zip_open().` |
|      - | 5276 | `  * Return` |
|      - | 5277 | `  *  A directory entry resource for later use with the zip_entry_... functions` |
|      - | 5278 | `  *  or FALSE if there are no more entries to read, or an error code if an error occurred.` |
|      - | 5279 | `  */` |
|      8 | 5280 | `static int PH7_builtin_zip_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5281 |  |
|      9 | 5282 | `	SyArchiveEntry *pNext = 0; /* cc warning */` |
|      - | 5283 | `	SyArchive *pArchive;` |
|      - | 5284 | `	sxi32 rc;` |
|      9 | 5285 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5286 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5287 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|      - | 5288 | `		/* return FALSE */` |
|    ! 0 | 5289 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5290 | `		return PH7_OK;` |
|      - | 5291 | `	}` |
|      - | 5292 | `	/* Point to the in-memory archive */` |
|      9 | 5293 | `	pArchive = (SyArchive *)ph7_value_to_resource(apArg[0]);` |
|      - | 5294 | `	/* Make sure we are dealing with a valid ZIP archive */` |
|      9 | 5295 | `	if( SXARCH_INVALID(pArchive) ){` |
|    ! 0 | 5296 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|      - | 5297 | `		/* return FALSE */` |
|    ! 0 | 5298 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5299 | `		return PH7_OK;` |
|      - | 5300 | `	}` |
|      - | 5301 | `	/* Extract the next entry */` |
|      9 | 5302 | `	rc = SyArchiveGetNextEntry(pArchive,&pNext);` |
|      9 | 5303 | `	if( rc != SXRET_OK ){` |
|      - | 5304 | `		/* No more entries in the central directory,return FALSE */` |
|    ! 0 | 5305 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5306 | `	}else{` |
|      - | 5307 | `		/* Return as a resource handle */` |
|      9 | 5308 | `		ph7_result_resource(pCtx,pNext);` |
|      - | 5309 | `		/* Point to the ZIP raw data */` |
|      9 | 5310 | `		pNext->pUserData = (void *)&pArchive[1];` |
|      - | 5311 | `	}` |
|      9 | 5312 | `	return PH7_OK;` |
|      5 | 5313 |  |
|      - | 5314 | `/*` |
|      - | 5315 | `  * bool zip_entry_open(resource $zip,resource $zip_entry[,string $mode ])` |
|      - | 5316 | `  *  Open a directory entry for reading` |
|      - | 5317 | `  * Parameters` |
|      - | 5318 | `  *  $zip` |
|      - | 5319 | `  *   A ZIP file previously opened with zip_open().` |
|      - | 5320 | `  *  $zip_entry` |
|      - | 5321 | `  *   A directory entry returned by zip_read().` |
|      - | 5322 | `  * $mode` |
|      - | 5323 | `  *   Not used` |
|      - | 5324 | `  * Return` |
|      - | 5325 | `  *  A directory entry resource for later use with the zip_entry_... functions` |
|      - | 5326 | `  *  or FALSE if there are no more entries to read, or an error code if an error occurred.` |
|      - | 5327 | `  */` |
|      2 | 5328 | `static int PH7_builtin_zip_entry_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5329 |  |
|      - | 5330 | `	SyArchiveEntry *pEntry;` |
|      - | 5331 | `	SyArchive *pArchive;` |
|      3 | 5332 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_resource(apArg[1]) ){` |
|      - | 5333 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5334 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|      - | 5335 | `		/* return FALSE */` |
|    ! 0 | 5336 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5337 | `		return PH7_OK;` |
|      - | 5338 | `	}` |
|      - | 5339 | `	/* Point to the in-memory archive */` |
|      3 | 5340 | `	pArchive = (SyArchive *)ph7_value_to_resource(apArg[0]);` |
|      - | 5341 | `	/* Make sure we are dealing with a valid ZIP archive */` |
|      3 | 5342 | `	if( SXARCH_INVALID(pArchive) ){` |
|    ! 0 | 5343 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|      - | 5344 | `		/* return FALSE */` |
|    ! 0 | 5345 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5346 | `		return PH7_OK;` |
|      - | 5347 | `	}` |
|      - | 5348 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|      3 | 5349 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[1]);` |
|      3 | 5350 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|    ! 0 | 5351 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5352 | `		/* return FALSE */` |
|    ! 0 | 5353 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5354 | `		return PH7_OK;` |
|      - | 5355 | `	}` |
|      - | 5356 | `	/* All done. Actually this function is a no-op,return TRUE */` |
|      3 | 5357 | `	ph7_result_bool(pCtx,1);` |
|      3 | 5358 | `	return PH7_OK;` |
|      2 | 5359 |  |
|      - | 5360 | `/*` |
|      - | 5361 | `  * bool zip_entry_close(resource $zip_entry)` |
|      - | 5362 | `  *  Close a directory entry.` |
|      - | 5363 | `  * Parameters` |
|      - | 5364 | `  *  $zip_entry` |
|      - | 5365 | `  *   A directory entry returned by zip_read().` |
|      - | 5366 | `  * Return` |
|      - | 5367 | `  *  Returns TRUE on success or FALSE on failure.` |
|      - | 5368 | `  */` |
|      6 | 5369 | `static int PH7_builtin_zip_entry_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5370 |  |
|      - | 5371 | `	SyArchiveEntry *pEntry;` |
|      7 | 5372 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5373 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5374 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5375 | `		/* return FALSE */` |
|    ! 0 | 5376 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5377 | `		return PH7_OK;` |
|      - | 5378 | `	}` |
|      - | 5379 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|      7 | 5380 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|      7 | 5381 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|    ! 0 | 5382 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5383 | `		/* return FALSE */` |
|    ! 0 | 5384 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5385 | `		return PH7_OK;` |
|      - | 5386 | `	}` |
|      - | 5387 | `	/* Reset the read cursor */` |
|      7 | 5388 | `	pEntry->nReadCount = 0;` |
|      - | 5389 | `	/*All done. Actually this function is a no-op,return TRUE */` |
|      7 | 5390 | `	ph7_result_bool(pCtx,1);` |
|      7 | 5391 | `	return PH7_OK;` |
|      4 | 5392 |  |
|      - | 5393 | `/*` |
|      - | 5394 | `  * string zip_entry_name(resource $zip_entry)` |
|      - | 5395 | `  *  Retrieve the name of a directory entry.` |
|      - | 5396 | `  * Parameters` |
|      - | 5397 | `  *  $zip_entry` |
|      - | 5398 | `  *   A directory entry returned by zip_read().` |
|      - | 5399 | `  * Return` |
|      - | 5400 | `  *  The name of the directory entry.` |
|      - | 5401 | `  */` |
|      2 | 5402 | `static int PH7_builtin_zip_entry_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5403 |  |
|      - | 5404 | `	SyArchiveEntry *pEntry;` |
|      - | 5405 | `	SyString *pName;` |
|      3 | 5406 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5407 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5408 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5409 | `		/* return FALSE */` |
|    ! 0 | 5410 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5411 | `		return PH7_OK;` |
|      - | 5412 | `	}` |
|      - | 5413 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|      3 | 5414 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|      3 | 5415 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|    ! 0 | 5416 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5417 | `		/* return FALSE */` |
|    ! 0 | 5418 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5419 | `		return PH7_OK;` |
|      - | 5420 | `	}` |
|      - | 5421 | `	/* Return entry name */` |
|      3 | 5422 | `	pName = &pEntry->sFileName;` |
|      3 | 5423 | `	ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|      3 | 5424 | `	return PH7_OK;` |
|      2 | 5425 |  |
|      - | 5426 | `/*` |
|      - | 5427 | `  * int64 zip_entry_filesize(resource $zip_entry)` |
|      - | 5428 | `  *  Retrieve the actual file size of a directory entry.` |
|      - | 5429 | `  * Parameters` |
|      - | 5430 | `  *  $zip_entry` |
|      - | 5431 | `  *   A directory entry returned by zip_read().` |
|      - | 5432 | `  * Return` |
|      - | 5433 | `  *  The size of the directory entry.` |
|      - | 5434 | `  */` |
|      4 | 5435 | `static int PH7_builtin_zip_entry_filesize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5436 |  |
|      - | 5437 | `	SyArchiveEntry *pEntry;` |
|      5 | 5438 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5439 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5440 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5441 | `		/* return FALSE */` |
|    ! 0 | 5442 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5443 | `		return PH7_OK;` |
|      - | 5444 | `	}` |
|      - | 5445 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|      5 | 5446 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|      5 | 5447 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|    ! 0 | 5448 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5449 | `		/* return FALSE */` |
|    ! 0 | 5450 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5451 | `		return PH7_OK;` |
|      - | 5452 | `	}` |
|      - | 5453 | `	/* Return entry size */` |
|      5 | 5454 | `	ph7_result_int64(pCtx,(ph7_int64)pEntry->nByte);` |
|      5 | 5455 | `	return PH7_OK;` |
|      3 | 5456 |  |
|      - | 5457 | `/*` |
|      - | 5458 | `  * int64 zip_entry_compressedsize(resource $zip_entry)` |
|      - | 5459 | `  *  Retrieve the compressed size of a directory entry.` |
|      - | 5460 | `  * Parameters` |
|      - | 5461 | `  *  $zip_entry` |
|      - | 5462 | `  *   A directory entry returned by zip_read().` |
|      - | 5463 | `  * Return` |
|      - | 5464 | `  *  The compressed size.` |
|      - | 5465 | `  */` |
|      2 | 5466 | `static int PH7_builtin_zip_entry_compressedsize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5467 |  |
|      - | 5468 | `	SyArchiveEntry *pEntry;` |
|      3 | 5469 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5470 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5471 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5472 | `		/* return FALSE */` |
|    ! 0 | 5473 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5474 | `		return PH7_OK;` |
|      - | 5475 | `	}` |
|      - | 5476 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|      3 | 5477 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|      3 | 5478 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|    ! 0 | 5479 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5480 | `		/* return FALSE */` |
|    ! 0 | 5481 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5482 | `		return PH7_OK;` |
|      - | 5483 | `	}` |
|      - | 5484 | `	/* Return entry compressed size */` |
|      3 | 5485 | `	ph7_result_int64(pCtx,(ph7_int64)pEntry->nByteCompr);` |
|      3 | 5486 | `	return PH7_OK;` |
|      2 | 5487 |  |
|      - | 5488 | `/*` |
|      - | 5489 | `  * string zip_entry_read(resource $zip_entry[,int $length])` |
|      - | 5490 | `  *  Reads from an open directory entry.` |
|      - | 5491 | `  * Parameters` |
|      - | 5492 | `  *  $zip_entry` |
|      - | 5493 | `  *   A directory entry returned by zip_read().` |
|      - | 5494 | `  *  $length` |
|      - | 5495 | `  *   The number of bytes to return. If not specified, this function` |
|      - | 5496 | `  *   will attempt to read 1024 bytes.` |
|      - | 5497 | `  * Return` |
|      - | 5498 | `  *  Returns the data read, or FALSE if the end of the file is reached.` |
|      - | 5499 | `  */` |
|      2 | 5500 | `static int PH7_builtin_zip_entry_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5501 |  |
|      - | 5502 | `	SyArchiveEntry *pEntry;` |
|      - | 5503 | `	zip_raw_data *pRaw;` |
|      - | 5504 | `	const char *zData;` |
|      - | 5505 | `	int iLength;` |
|      3 | 5506 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5507 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5508 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5509 | `		/* return FALSE */` |
|    ! 0 | 5510 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5511 | `		return PH7_OK;` |
|      - | 5512 | `	}` |
|      - | 5513 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|      3 | 5514 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|      3 | 5515 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|    ! 0 | 5516 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5517 | `		/* return FALSE */` |
|    ! 0 | 5518 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5519 | `		return PH7_OK;` |
|      - | 5520 | `	}` |
|      3 | 5521 | `	zData = 0;` |
|      3 | 5522 | `	if( pEntry->nReadCount >= pEntry->nByteCompr ){` |
|      - | 5523 | `		/* No more data to read,return FALSE */` |
|    ! 0 | 5524 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5525 | `		return PH7_OK;` |
|      - | 5526 | `	}` |
|      - | 5527 | `	/* Set a default read length */` |
|      3 | 5528 | `	iLength = 1024;` |
|      3 | 5529 | `	if( nArg > 1 ){` |
|      3 | 5530 | `		iLength = ph7_value_to_int(apArg[1]);` |
|      3 | 5531 | `		if( iLength < 1 ){` |
|    ! 0 | 5532 | `			iLength = 1024;` |
|    ! 0 | 5533 | `		}` |
|      1 | 5534 | `	}` |
|      3 | 5535 | `	if( (sxu32)iLength > pEntry->nByteCompr - pEntry->nReadCount ){` |
|    ! 0 | 5536 | `		iLength = (int)(pEntry->nByteCompr - pEntry->nReadCount);` |
|    ! 0 | 5537 | `	}` |
|      - | 5538 | `	/* Return the entry contents */` |
|      3 | 5539 | `	pRaw = (zip_raw_data *)pEntry->pUserData;` |
|      3 | 5540 | `	if( pRaw->iType == ZIP_RAW_DATA_MEMBUF ){` |
|    ! 0 | 5541 | `		zData = (const char *)SyBlobDataAt(&pRaw->raw.sBlob,(pEntry->nOfft+pEntry->nReadCount));` |
|    ! 0 | 5542 | `	}else{` |
|      3 | 5543 | `		const char *zMap = (const char *)pRaw->raw.mmap.pMap;` |
|      - | 5544 | `		/* Memory mmaped chunk */` |
|      3 | 5545 | `		zData = &zMap[pEntry->nOfft+pEntry->nReadCount];` |
|      - | 5546 | `	}` |
|      - | 5547 | `	/* Increment the read counter */` |
|      3 | 5548 | `	pEntry->nReadCount += iLength;` |
|      - | 5549 | `	/* Return the raw data */` |
|      3 | 5550 | `	ph7_result_string(pCtx,zData,iLength);` |
|      3 | 5551 | `	return PH7_OK;` |
|      2 | 5552 |  |
|      - | 5553 | `/*` |
|      - | 5554 | `  * bool zip_entry_reset_read_cursor(resource $zip_entry)` |
|      - | 5555 | `  *  Reset the read cursor of an open directory entry.` |
|      - | 5556 | `  * Parameters` |
|      - | 5557 | `  *  $zip_entry` |
|      - | 5558 | `  *   A directory entry returned by zip_read().` |
|      - | 5559 | `  * Return` |
|      - | 5560 | `  *  TRUE on success,FALSE on failure.` |
|      - | 5561 | `  * Note that this is a symisc eXtension.` |
|      - | 5562 | `  */` |
|      6 | 5563 | `static int PH7_builtin_zip_entry_reset_read_cursor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5564 |  |
|      - | 5565 | `	SyArchiveEntry *pEntry;` |
|      7 | 5566 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5567 | `		/* Missing/Invalid arguments */` |
|      5 | 5568 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5569 | `		/* return FALSE */` |
|      5 | 5570 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5571 | `		return PH7_OK;` |
|      - | 5572 | `	}` |
|      - | 5573 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|      3 | 5574 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|      3 | 5575 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|    ! 0 | 5576 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5577 | `		/* return FALSE */` |
|    ! 0 | 5578 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5579 | `		return PH7_OK;` |
|      - | 5580 | `	}` |
|      - | 5581 | `	/* Reset the cursor */` |
|      3 | 5582 | `	pEntry->nReadCount = 0;` |
|      - | 5583 | `	/* Return TRUE */` |
|      3 | 5584 | `	ph7_result_bool(pCtx,1);` |
|      3 | 5585 | `	return PH7_OK;` |
|      4 | 5586 |  |
|      - | 5587 | `/*` |
|      - | 5588 | `  * string zip_entry_compressionmethod(resource $zip_entry)` |
|      - | 5589 | `  *  Retrieve the compression method of a directory entry.` |
|      - | 5590 | `  * Parameters` |
|      - | 5591 | `  *  $zip_entry` |
|      - | 5592 | `  *   A directory entry returned by zip_read().` |
|      - | 5593 | `  * Return` |
|      - | 5594 | `  *  The compression method on success or FALSE on failure.` |
|      - | 5595 | `  */` |
|      2 | 5596 | `static int PH7_builtin_zip_entry_compressionmethod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5597 |  |
|      - | 5598 | `	SyArchiveEntry *pEntry;` |
|      3 | 5599 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5600 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 5601 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5602 | `		/* return FALSE */` |
|    ! 0 | 5603 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5604 | `		return PH7_OK;` |
|      - | 5605 | `	}` |
|      - | 5606 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|      3 | 5607 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|      3 | 5608 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|    ! 0 | 5609 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|      - | 5610 | `		/* return FALSE */` |
|    ! 0 | 5611 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5612 | `		return PH7_OK;` |
|      - | 5613 | `	}` |
|      3 | 5614 | `	switch(pEntry->nComprMeth){` |
|      1 | 5615 | `	case 0:` |
|      - | 5616 | `		/* No compression;entry is stored */` |
|      3 | 5617 | `		ph7_result_string(pCtx,"stored",(int)sizeof("stored")-1);` |
|      3 | 5618 | `		break;` |
|    ! 0 | 5619 | `	case 8:` |
|      - | 5620 | `		/* Entry is deflated (Default compression algorithm)  */` |
|    ! 0 | 5621 | `		ph7_result_string(pCtx,"deflate",(int)sizeof("deflate")-1);` |
|    ! 0 | 5622 | `		break;` |
|      - | 5623 | `		/* Exotic compression algorithms */` |
|    ! 0 | 5624 | `	case 1:` |
|    ! 0 | 5625 | `		ph7_result_string(pCtx,"shrunk",(int)sizeof("shrunk")-1);` |
|    ! 0 | 5626 | `		break;` |
|    ! 0 | 5627 | `	case 2:` |
|      - | 5628 | `	case 3:` |
|      - | 5629 | `	case 4:` |
|      - | 5630 | `	case 5:` |
|      - | 5631 | `		/* Entry is reduced */` |
|    ! 0 | 5632 | `		ph7_result_string(pCtx,"reduced",(int)sizeof("reduced")-1);` |
|    ! 0 | 5633 | `		break;` |
|    ! 0 | 5634 | `	case 6:` |
|      - | 5635 | `		/* Entry is imploded */` |
|    ! 0 | 5636 | `		ph7_result_string(pCtx,"implode",(int)sizeof("implode")-1);` |
|    ! 0 | 5637 | `		break;` |
|    ! 0 | 5638 | `	default:` |
|    ! 0 | 5639 | `		ph7_result_string(pCtx,"unknown",(int)sizeof("unknown")-1);` |
|    ! 0 | 5640 | `		break;` |
|      - | 5641 | `	}` |
|      3 | 5642 | `	return PH7_OK;` |
|      2 | 5643 |  |
|      - | 5644 | `#endif /* #ifndef PH7_DISABLE_BUILTIN_FUNC*/` |
|      - | 5645 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 5646 | `#if defined(_MSC_VER)` |
|      - | 5647 | `static const ph7_vfs null_vfs = {` |
|      - | 5648 | `#else` |
|      - | 5649 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 5650 | `#endif` |
|      - | 5651 | `	"null_vfs",` |
|      - | 5652 | `	PH7_VFS_VERSION,` |
|      - | 5653 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 5654 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 5655 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 5656 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 5657 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 5658 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 5659 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 5660 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 5661 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 5662 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 5663 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 5664 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 5665 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 5666 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 5667 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 5668 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 5669 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 5670 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 5671 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 5672 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 5673 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 5674 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 5675 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 5676 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 5677 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 5678 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 5679 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 5680 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 5681 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 5682 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 5683 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 5684 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 5685 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 5686 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 5687 | `	0, /* int (*xUmask)(int) */` |
|      - | 5688 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 5689 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 5690 | `	0, /* int (*xUid)(void) */` |
|      - | 5691 | `	0, /* int (*xGid)(void) */` |
|      - | 5692 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 5693 | `	0  /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 5694 | `};` |
|      - | 5695 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 5696 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 5697 | `#ifdef __WINNT__` |
|      - | 5698 | `/*` |
|      - | 5699 | ` * Windows VFS implementation for the PH7 engine.` |
|      - | 5700 | ` * Status:` |
|      - | 5701 | ` *    Stable.` |
|      - | 5702 | ` */` |
|      - | 5703 | `/* What follows here is code that is specific to windows systems. */` |
|      - | 5704 | `#include <Windows.h>` |
|      - | 5705 | `#include <stdio.h> /* For popen/pclose pipe stream support */` |
|      - | 5706 | `#include <io.h>    /* For _open_osfhandle, _close */` |
|      - | 5707 | `#include <fcntl.h> /* For _O_RDONLY, _O_WRONLY, _O_TEXT */` |
|      - | 5708 | `/*` |
|      - | 5709 | `** Convert a UTF-8 string to microsoft unicode (UTF-16?).` |
|      - | 5710 | `**` |
|      - | 5711 | `** Space to hold the returned string is obtained from HeapAlloc().` |
|      - | 5712 | `** Taken from the sqlite3 source tree` |
|      - | 5713 | `** status: Public Domain` |
|      - | 5714 | `*/` |
|      1 | 5715 | `static WCHAR *utf8ToUnicode(const char *zFilename){` |
|      - | 5716 | `  int nChar;` |
|      - | 5717 | `  WCHAR *zWideFilename;` |
|      - | 5718 |  |
|      1 | 5719 | `  nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, 0, 0);` |
|      1 | 5720 | `  zWideFilename = (WCHAR *)HeapAlloc(GetProcessHeap(),0,nChar*sizeof(zWideFilename[0]));` |
|      1 | 5721 | `  if( zWideFilename == 0 ){` |
|    ! 0 | 5722 | ` 	return 0;` |
|      - | 5723 | `  }` |
|      1 | 5724 | `  nChar = MultiByteToWideChar(CP_UTF8, 0, zFilename, -1, zWideFilename, nChar);` |
|      1 | 5725 | `  if( nChar==0 ){` |
|    ! 0 | 5726 | `    HeapFree(GetProcessHeap(),0,zWideFilename);` |
|    ! 0 | 5727 | `    return 0;` |
|      - | 5728 | `  }` |
|      1 | 5729 | `  return zWideFilename;` |
|      1 | 5730 |  |
|      - | 5731 | `/*` |
|      - | 5732 | `** Convert a UTF-8 filename into whatever form the underlying` |
|      - | 5733 | `** operating system wants filenames in.Space to hold the result` |
|      - | 5734 | `** is obtained from HeapAlloc() and must be freed by the calling` |
|      - | 5735 | `** function.` |
|      - | 5736 | `** Taken from the sqlite3 source tree` |
|      - | 5737 | `** status: Public Domain` |
|      - | 5738 | `*/` |
|      1 | 5739 | `static void *convertUtf8Filename(const char *zFilename){` |
|      - | 5740 | `  void *zConverted;` |
|      1 | 5741 | `  zConverted = utf8ToUnicode(zFilename);` |
|      1 | 5742 | `  return zConverted;` |
|      1 | 5743 |  |
|      - | 5744 | `/*` |
|      - | 5745 | `** Convert microsoft unicode to UTF-8.  Space to hold the returned string is` |
|      - | 5746 | `** obtained from HeapAlloc().` |
|      - | 5747 | `** Taken from the sqlite3 source tree` |
|      - | 5748 | `** status: Public Domain` |
|      - | 5749 | `*/` |
|      1 | 5750 | `static char *unicodeToUtf8(const WCHAR *zWideFilename){` |
|      - | 5751 | `  char *zFilename;` |
|      - | 5752 | `  int nByte;` |
|      - | 5753 |  |
|      1 | 5754 | `  nByte = WideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, 0, 0, 0, 0);` |
|      1 | 5755 | `  zFilename = (char *)HeapAlloc(GetProcessHeap(),0,nByte);` |
|      1 | 5756 | `  if( zFilename == 0 ){` |
|    ! 0 | 5757 | `  	return 0;` |
|      - | 5758 | `  }` |
|      1 | 5759 | `  nByte = WideCharToMultiByte(CP_UTF8, 0, zWideFilename, -1, zFilename, nByte,0, 0);` |
|      1 | 5760 | `  if( nByte == 0 ){` |
|    ! 0 | 5761 | `    HeapFree(GetProcessHeap(),0,zFilename);` |
|    ! 0 | 5762 | `    return 0;` |
|      - | 5763 | `  }` |
|      1 | 5764 | `  return zFilename;` |
|      1 | 5765 |  |
|      - | 5766 | `/* int (*xchdir)(const char *) */` |
|      - | 5767 | `static int WinVfs_chdir(const char *zPath)` |
|      1 | 5768 |  |
|      - | 5769 | `	void * pConverted;` |
|      - | 5770 | `	BOOL rc;` |
|      1 | 5771 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5772 | `	if( pConverted == 0 ){` |
|    ! 0 | 5773 | `		return -1;` |
|      - | 5774 | `	}` |
|      1 | 5775 | `	rc = SetCurrentDirectoryW((LPCWSTR)pConverted);` |
|      1 | 5776 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5777 | `	return rc ? PH7_OK : -1;` |
|      1 | 5778 |  |
|      - | 5779 | `/* int (*xGetcwd)(ph7_context *) */` |
|      - | 5780 | `static int WinVfs_getcwd(ph7_context *pCtx)` |
|      1 | 5781 |  |
|      - | 5782 | `	WCHAR zDir[2048];` |
|      - | 5783 | `	char *zConverted;` |
|      - | 5784 | `	DWORD rc;` |
|      - | 5785 | `	/* Get the current directory */` |
|      1 | 5786 | `	rc = GetCurrentDirectoryW(sizeof(zDir),zDir);` |
|      1 | 5787 | `	if( rc < 1 ){` |
|    ! 0 | 5788 | `		return -1;` |
|      - | 5789 | `	}` |
|      1 | 5790 | `	zConverted = unicodeToUtf8(zDir);` |
|      1 | 5791 | `	if( zConverted == 0 ){` |
|    ! 0 | 5792 | `		return -1;` |
|      - | 5793 | `	}` |
|      1 | 5794 | `	ph7_result_string(pCtx,zConverted,-1/*Compute length automatically*/); /* Will make it's own copy */` |
|      1 | 5795 | `	HeapFree(GetProcessHeap(),0,zConverted);` |
|      1 | 5796 | `	return PH7_OK;` |
|      1 | 5797 |  |
|      - | 5798 | `/* int (*xMkdir)(const char *,int,int) */` |
|      - | 5799 | `static int WinVfs_mkdir(const char *zPath,int mode,int recursive)` |
|      1 | 5800 |  |
|      - | 5801 | `	void * pConverted;` |
|      - | 5802 | `	BOOL rc;` |
|      1 | 5803 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5804 | `	if( pConverted == 0 ){` |
|    ! 0 | 5805 | `		return -1;` |
|      - | 5806 | `	}` |
|      1 | 5807 | `	mode= 0; /* MSVC warning */` |
|      1 | 5808 | `	recursive = 0;` |
|      1 | 5809 | `	rc = CreateDirectoryW((LPCWSTR)pConverted,0);` |
|      1 | 5810 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5811 | `	return rc ? PH7_OK : -1;` |
|      1 | 5812 |  |
|      - | 5813 | `/* int (*xRmdir)(const char *) */` |
|      - | 5814 | `static int WinVfs_rmdir(const char *zPath)` |
|      1 | 5815 |  |
|      - | 5816 | `	void * pConverted;` |
|      - | 5817 | `	BOOL rc;` |
|      1 | 5818 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5819 | `	if( pConverted == 0 ){` |
|    ! 0 | 5820 | `		return -1;` |
|      - | 5821 | `	}` |
|      1 | 5822 | `	rc = RemoveDirectoryW((LPCWSTR)pConverted);` |
|      1 | 5823 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5824 | `	return rc ? PH7_OK : -1;` |
|      1 | 5825 |  |
|      - | 5826 | `/* int (*xIsdir)(const char *) */` |
|      - | 5827 | `static int WinVfs_isdir(const char *zPath)` |
|      1 | 5828 |  |
|      - | 5829 | `	void * pConverted;` |
|      - | 5830 | `	DWORD dwAttr;` |
|      1 | 5831 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5832 | `	if( pConverted == 0 ){` |
|    ! 0 | 5833 | `		return -1;` |
|      - | 5834 | `	}` |
|      1 | 5835 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|      1 | 5836 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5837 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|      1 | 5838 | `		return -1;` |
|      - | 5839 | `	}` |
|      1 | 5840 | `	return (dwAttr & FILE_ATTRIBUTE_DIRECTORY) ? PH7_OK : -1;` |
|      1 | 5841 |  |
|      - | 5842 | `/* int (*xRename)(const char *,const char *) */` |
|      - | 5843 | `static int WinVfs_Rename(const char *zOld,const char *zNew)` |
|      1 | 5844 |  |
|      - | 5845 | `	void *pOld,*pNew;` |
|      1 | 5846 | `	BOOL rc = 0;` |
|      1 | 5847 | `	pOld = convertUtf8Filename(zOld);` |
|      1 | 5848 | `	if( pOld == 0 ){` |
|    ! 0 | 5849 | `		return -1;` |
|      - | 5850 | `	}` |
|      1 | 5851 | `	pNew = convertUtf8Filename(zNew);` |
|      1 | 5852 | `	if( pNew  ){` |
|      1 | 5853 | `		rc = MoveFileW((LPCWSTR)pOld,(LPCWSTR)pNew);` |
|      - | 5854 | `	}` |
|      1 | 5855 | `	HeapFree(GetProcessHeap(),0,pOld);` |
|      1 | 5856 | `	if( pNew ){` |
|      1 | 5857 | `		HeapFree(GetProcessHeap(),0,pNew);` |
|      - | 5858 | `	}` |
|      1 | 5859 | `	return rc ? PH7_OK : - 1;` |
|      1 | 5860 |  |
|      - | 5861 | `/* int (*xRealpath)(const char *,ph7_context *) */` |
|      - | 5862 | `static int WinVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|      1 | 5863 |  |
|      - | 5864 | `	WCHAR zTemp[2048];` |
|      - | 5865 | `	void *pPath;` |
|      - | 5866 | `	char *zReal;` |
|      - | 5867 | `	DWORD n;` |
|      1 | 5868 | `	pPath = convertUtf8Filename(zPath);` |
|      1 | 5869 | `	if( pPath == 0 ){` |
|    ! 0 | 5870 | `		return -1;` |
|      - | 5871 | `	}` |
|      1 | 5872 | `	n = GetFullPathNameW((LPCWSTR)pPath,0,0,0);` |
|      1 | 5873 | `	if( n > 0 ){` |
|      1 | 5874 | `		if( n >= sizeof(zTemp) ){` |
|    ! 0 | 5875 | `			n = sizeof(zTemp) - 1;` |
|      - | 5876 | `		}` |
|      1 | 5877 | `		GetFullPathNameW((LPCWSTR)pPath,n,zTemp,0);` |
|      - | 5878 | `	}` |
|      1 | 5879 | `	HeapFree(GetProcessHeap(),0,pPath);` |
|      1 | 5880 | `	if( !n ){` |
|    ! 0 | 5881 | `		return -1;` |
|      - | 5882 | `	}` |
|      1 | 5883 | `	zReal = unicodeToUtf8(zTemp);` |
|      1 | 5884 | `	if( zReal == 0 ){` |
|    ! 0 | 5885 | `		return -1;` |
|      - | 5886 | `	}` |
|      1 | 5887 | `	ph7_result_string(pCtx,zReal,-1); /* Will make it's own copy */` |
|      1 | 5888 | `	HeapFree(GetProcessHeap(),0,zReal);` |
|      1 | 5889 | `	return PH7_OK;` |
|      1 | 5890 |  |
|      - | 5891 | `/* int (*xSleep)(unsigned int) */` |
|      - | 5892 | `static int WinVfs_Sleep(unsigned int uSec)` |
|      1 | 5893 |  |
|      1 | 5894 | `	Sleep(uSec/1000/*uSec per Millisec */);` |
|      1 | 5895 | `	return PH7_OK;` |
|      1 | 5896 |  |
|      - | 5897 | `/* int (*xUnlink)(const char *) */` |
|      - | 5898 | `static int WinVfs_unlink(const char *zPath)` |
|      1 | 5899 |  |
|      - | 5900 | `	void * pConverted;` |
|      - | 5901 | `	BOOL rc;` |
|      1 | 5902 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5903 | `	if( pConverted == 0 ){` |
|    ! 0 | 5904 | `		return -1;` |
|      - | 5905 | `	}` |
|      1 | 5906 | `	rc = DeleteFileW((LPCWSTR)pConverted);` |
|      1 | 5907 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5908 | `	return rc ? PH7_OK : - 1;` |
|      1 | 5909 |  |
|      - | 5910 | `/* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 5911 | `static ph7_int64 WinVfs_DiskFreeSpace(const char *zPath)` |
|    ! 0 | 5912 |  |
|      - | 5913 | `#ifdef _WIN32_WCE` |
|      - | 5914 | `	/* GetDiskFreeSpace is not supported under WINCE */` |
|      - | 5915 | `	SXUNUSED(zPath);` |
|      - | 5916 | `	return 0;` |
|      - | 5917 | `#else` |
|      - | 5918 | `	DWORD dwSectPerClust,dwBytesPerSect,dwFreeClusters,dwTotalClusters;` |
|      - | 5919 | `	void * pConverted;` |
|      - | 5920 | `	WCHAR *p;` |
|      - | 5921 | `	BOOL rc;` |
|    ! 0 | 5922 | `	pConverted = convertUtf8Filename(zPath);` |
|    ! 0 | 5923 | `	if( pConverted == 0 ){` |
|    ! 0 | 5924 | `		return 0;` |
|      - | 5925 | `	}` |
|    ! 0 | 5926 | `	p = (WCHAR *)pConverted;` |
|    ! 0 | 5927 | `	for(;*p;p++){` |
|    ! 0 | 5928 | `		if( *p == '\\' \|\| *p == '/'){` |
|    ! 0 | 5929 | `			*p = '\0';` |
|    ! 0 | 5930 | `			break;` |
|      - | 5931 | `		}` |
|    ! 0 | 5932 | `	}` |
|    ! 0 | 5933 | `	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted,&dwSectPerClust,&dwBytesPerSect,&dwFreeClusters,&dwTotalClusters);` |
|    ! 0 | 5934 | `	if( !rc ){` |
|    ! 0 | 5935 | `		return 0;` |
|      - | 5936 | `	}` |
|    ! 0 | 5937 | `	return (ph7_int64)dwFreeClusters * dwSectPerClust * dwBytesPerSect;` |
|      - | 5938 | `#endif` |
|    ! 0 | 5939 |  |
|      - | 5940 | `/* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 5941 | `static ph7_int64 WinVfs_DiskTotalSpace(const char *zPath)` |
|    ! 0 | 5942 |  |
|      - | 5943 | `#ifdef _WIN32_WCE` |
|      - | 5944 | `	/* GetDiskFreeSpace is not supported under WINCE */` |
|      - | 5945 | `	SXUNUSED(zPath);` |
|      - | 5946 | `	return 0;` |
|      - | 5947 | `#else` |
|      - | 5948 | `	DWORD dwSectPerClust,dwBytesPerSect,dwFreeClusters,dwTotalClusters;` |
|      - | 5949 | `	void * pConverted;` |
|      - | 5950 | `	WCHAR *p;` |
|      - | 5951 | `	BOOL rc;` |
|    ! 0 | 5952 | `	pConverted = convertUtf8Filename(zPath);` |
|    ! 0 | 5953 | `	if( pConverted == 0 ){` |
|    ! 0 | 5954 | `		return 0;` |
|      - | 5955 | `	}` |
|    ! 0 | 5956 | `	p = (WCHAR *)pConverted;` |
|    ! 0 | 5957 | `	for(;*p;p++){` |
|    ! 0 | 5958 | `		if( *p == '\\' \|\| *p == '/'){` |
|    ! 0 | 5959 | `			*p = '\0';` |
|    ! 0 | 5960 | `			break;` |
|      - | 5961 | `		}` |
|    ! 0 | 5962 | `	}` |
|    ! 0 | 5963 | `	rc = GetDiskFreeSpaceW((LPCWSTR)pConverted,&dwSectPerClust,&dwBytesPerSect,&dwFreeClusters,&dwTotalClusters);` |
|    ! 0 | 5964 | `	if( !rc ){` |
|    ! 0 | 5965 | `		return 0;` |
|      - | 5966 | `	}` |
|    ! 0 | 5967 | `	return (ph7_int64)dwTotalClusters * dwSectPerClust * dwBytesPerSect;` |
|      - | 5968 | `#endif` |
|    ! 0 | 5969 |  |
|      - | 5970 | `/* int (*xFileExists)(const char *) */` |
|      - | 5971 | `static int WinVfs_FileExists(const char *zPath)` |
|      1 | 5972 |  |
|      - | 5973 | `	void * pConverted;` |
|      - | 5974 | `	DWORD dwAttr;` |
|      1 | 5975 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 5976 | `	if( pConverted == 0 ){` |
|    ! 0 | 5977 | `		return -1;` |
|      - | 5978 | `	}` |
|      1 | 5979 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|      1 | 5980 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 5981 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|      1 | 5982 | `		return -1;` |
|      - | 5983 | `	}` |
|      1 | 5984 | `	return PH7_OK;` |
|      1 | 5985 |  |
|      - | 5986 | `/* Open a file in a read-only mode */` |
|      - | 5987 | `static HANDLE OpenReadOnly(LPCWSTR pPath)` |
|      1 | 5988 |  |
|      1 | 5989 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|      1 | 5990 | `	DWORD dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|      1 | 5991 | `	DWORD dwAccess = GENERIC_READ;` |
|      1 | 5992 | `	DWORD dwCreate = OPEN_EXISTING;` |
|      - | 5993 | `	HANDLE pHandle;` |
|      1 | 5994 | `	pHandle = CreateFileW(pPath,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|      1 | 5995 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|      1 | 5996 | `		return 0;` |
|      - | 5997 | `	}` |
|      1 | 5998 | `	return pHandle;` |
|      1 | 5999 |  |
|      - | 6000 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 6001 | `static ph7_int64 WinVfs_FileSize(const char *zPath)` |
|      1 | 6002 |  |
|      - | 6003 | `	DWORD dwLow,dwHigh;` |
|      - | 6004 | `	void * pConverted;` |
|      - | 6005 | `	ph7_int64 nSize;` |
|      - | 6006 | `	HANDLE pHandle;` |
|      - | 6007 |  |
|      1 | 6008 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 6009 | `	if( pConverted == 0 ){` |
|    ! 0 | 6010 | `		return -1;` |
|      - | 6011 | `	}` |
|      - | 6012 | `	/* Open the file in read-only mode */` |
|      1 | 6013 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 6014 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 6015 | `	if( pHandle ){` |
|      1 | 6016 | `		dwLow = GetFileSize(pHandle,&dwHigh);` |
|      1 | 6017 | `		nSize = dwHigh;` |
|      1 | 6018 | `		nSize <<= 32;` |
|      1 | 6019 | `		nSize += dwLow;` |
|      1 | 6020 | `		CloseHandle(pHandle);` |
|      1 | 6021 | `	}else{` |
|    ! 0 | 6022 | `		nSize = -1;` |
|      - | 6023 | `	}` |
|      1 | 6024 | `	return nSize;` |
|      1 | 6025 |  |
|      - | 6026 | `#define TICKS_PER_SECOND 10000000` |
|      - | 6027 | `#define EPOCH_DIFFERENCE 11644473600LL` |
|      - | 6028 | `/* Convert Windows timestamp to UNIX timestamp */` |
|      - | 6029 | `static ph7_int64 convertWindowsTimeToUnixTime(LPFILETIME pTime)` |
|      1 | 6030 |  |
|      - | 6031 | `    ph7_int64 input,temp;` |
|      1 | 6032 | `	input = pTime->dwHighDateTime;` |
|      1 | 6033 | `	input <<= 32;` |
|      1 | 6034 | `	input += pTime->dwLowDateTime;` |
|      1 | 6035 | `    temp = input / TICKS_PER_SECOND; /*convert from 100ns intervals to seconds*/` |
|      1 | 6036 | `    temp = temp - EPOCH_DIFFERENCE;  /*subtract number of seconds between epochs*/` |
|      1 | 6037 | `    return temp;` |
|      1 | 6038 |  |
|      - | 6039 | `/* Convert UNIX timestamp to Windows timestamp */` |
|      - | 6040 | `static void convertUnixTimeToWindowsTime(ph7_int64 nUnixtime,LPFILETIME pOut)` |
|    ! 0 | 6041 |  |
|    ! 0 | 6042 | `  ph7_int64 result = EPOCH_DIFFERENCE;` |
|    ! 0 | 6043 | `  result += nUnixtime;` |
|    ! 0 | 6044 | `  result *= 10000000LL;` |
|    ! 0 | 6045 | `  pOut->dwHighDateTime = (DWORD)(nUnixtime>>32);` |
|    ! 0 | 6046 | `  pOut->dwLowDateTime = (DWORD)nUnixtime;` |
|    ! 0 | 6047 |  |
|      - | 6048 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 6049 | `static int WinVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|      1 | 6050 |  |
|      - | 6051 | `	FILETIME sTouch,sAccess;` |
|      - | 6052 | `	void *pConverted;` |
|      - | 6053 | `	void *pHandle;` |
|      1 | 6054 | `	BOOL rc = 0;` |
|      1 | 6055 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 6056 | `	if( pConverted == 0 ){` |
|    ! 0 | 6057 | `		return -1;` |
|      - | 6058 | `	}` |
|      1 | 6059 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 6060 | `	if( pHandle ){` |
|      1 | 6061 | `		if( touch_time < 0 ){` |
|      1 | 6062 | `			GetSystemTimeAsFileTime(&sTouch);` |
|      1 | 6063 | `		}else{` |
|    ! 0 | 6064 | `			convertUnixTimeToWindowsTime(touch_time,&sTouch);` |
|      - | 6065 | `		}` |
|      1 | 6066 | `		if( access_time < 0 ){` |
|      - | 6067 | `			/* Use the touch time */` |
|      1 | 6068 | `			sAccess = sTouch; /* Structure assignment */` |
|      1 | 6069 | `		}else{` |
|    ! 0 | 6070 | `			convertUnixTimeToWindowsTime(access_time,&sAccess);` |
|      - | 6071 | `		}` |
|      1 | 6072 | `		rc = SetFileTime(pHandle,&sTouch,&sAccess,0);` |
|      - | 6073 | `		/* Close the handle */` |
|      1 | 6074 | `		CloseHandle(pHandle);` |
|      - | 6075 | `	}` |
|      1 | 6076 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 6077 | `	return rc ? PH7_OK : -1;` |
|      1 | 6078 |  |
|      - | 6079 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 6080 | `static ph7_int64 WinVfs_FileAtime(const char *zPath)` |
|      1 | 6081 |  |
|      - | 6082 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|      - | 6083 | `	void * pConverted;` |
|      - | 6084 | `	ph7_int64 atime;` |
|      - | 6085 | `	HANDLE pHandle;` |
|      1 | 6086 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 6087 | `	if( pConverted == 0 ){` |
|    ! 0 | 6088 | `		return -1;` |
|      - | 6089 | `	}` |
|      - | 6090 | `	/* Open the file in read-only mode */` |
|      1 | 6091 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 6092 | `	if( pHandle ){` |
|      - | 6093 | `		BOOL rc;` |
|      1 | 6094 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|      1 | 6095 | `		if( rc ){` |
|      1 | 6096 | `			atime = convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime);` |
|      1 | 6097 | `		}else{` |
|    ! 0 | 6098 | `			atime = -1;` |
|      - | 6099 | `		}` |
|      1 | 6100 | `		CloseHandle(pHandle);` |
|      1 | 6101 | `	}else{` |
|    ! 0 | 6102 | `		atime = -1;` |
|      - | 6103 | `	}` |
|      1 | 6104 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 6105 | `	return atime;` |
|      1 | 6106 |  |
|      - | 6107 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 6108 | `static ph7_int64 WinVfs_FileMtime(const char *zPath)` |
|      1 | 6109 |  |
|      - | 6110 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|      - | 6111 | `	void * pConverted;` |
|      - | 6112 | `	ph7_int64 mtime;` |
|      - | 6113 | `	HANDLE pHandle;` |
|      1 | 6114 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 6115 | `	if( pConverted == 0 ){` |
|    ! 0 | 6116 | `		return -1;` |
|      - | 6117 | `	}` |
|      - | 6118 | `	/* Open the file in read-only mode */` |
|      1 | 6119 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 6120 | `	if( pHandle ){` |
|      - | 6121 | `		BOOL rc;` |
|      1 | 6122 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|      1 | 6123 | `		if( rc ){` |
|      1 | 6124 | `			mtime = convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime);` |
|      1 | 6125 | `		}else{` |
|    ! 0 | 6126 | `			mtime = -1;` |
|      - | 6127 | `		}` |
|      1 | 6128 | `		CloseHandle(pHandle);` |
|      1 | 6129 | `	}else{` |
|    ! 0 | 6130 | `		mtime = -1;` |
|      - | 6131 | `	}` |
|      1 | 6132 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 6133 | `	return mtime;` |
|      1 | 6134 |  |
|      - | 6135 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 6136 | `static ph7_int64 WinVfs_FileCtime(const char *zPath)` |
|      1 | 6137 |  |
|      - | 6138 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|      - | 6139 | `	void * pConverted;` |
|      - | 6140 | `	ph7_int64 ctime;` |
|      - | 6141 | `	HANDLE pHandle;` |
|      1 | 6142 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 6143 | `	if( pConverted == 0 ){` |
|    ! 0 | 6144 | `		return -1;` |
|      - | 6145 | `	}` |
|      - | 6146 | `	/* Open the file in read-only mode */` |
|      1 | 6147 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 6148 | `	if( pHandle ){` |
|      - | 6149 | `		BOOL rc;` |
|      1 | 6150 | `		rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|      1 | 6151 | `		if( rc ){` |
|      1 | 6152 | `			ctime = convertWindowsTimeToUnixTime(&sInfo.ftCreationTime);` |
|      1 | 6153 | `		}else{` |
|    ! 0 | 6154 | `			ctime = -1;` |
|      - | 6155 | `		}` |
|      1 | 6156 | `		CloseHandle(pHandle);` |
|      1 | 6157 | `	}else{` |
|    ! 0 | 6158 | `		ctime = -1;` |
|      - | 6159 | `	}` |
|      1 | 6160 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 6161 | `	return ctime;` |
|      1 | 6162 |  |
|      - | 6163 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6164 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6165 | `static int WinVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|      1 | 6166 |  |
|      - | 6167 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|      - | 6168 | `	void *pConverted;` |
|      - | 6169 | `	HANDLE pHandle;` |
|      - | 6170 | `	BOOL rc;` |
|      1 | 6171 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 6172 | `	if( pConverted == 0 ){` |
|    ! 0 | 6173 | `		return -1;` |
|      - | 6174 | `	}` |
|      - | 6175 | `	/* Open the file in read-only mode */` |
|      1 | 6176 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 6177 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 6178 | `	if( pHandle == 0 ){` |
|    ! 0 | 6179 | `		return -1;` |
|      - | 6180 | `	}` |
|      1 | 6181 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|      1 | 6182 | `	CloseHandle(pHandle);` |
|      1 | 6183 | `	if( !rc ){` |
|    ! 0 | 6184 | `		return -1;` |
|      - | 6185 | `	}` |
|      - | 6186 | `	/* dev */` |
|      1 | 6187 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|      1 | 6188 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|      - | 6189 | `	/* ino */` |
|      1 | 6190 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|      1 | 6191 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|      - | 6192 | `	/* mode */` |
|      1 | 6193 | `	ph7_value_int(pWorker,0);` |
|      1 | 6194 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|      - | 6195 | `	/* nlink */` |
|      1 | 6196 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|      1 | 6197 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|      - | 6198 | `	/* uid,gid,rdev */` |
|      1 | 6199 | `	ph7_value_int(pWorker,0);` |
|      1 | 6200 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|      1 | 6201 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|      1 | 6202 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|      - | 6203 | `	/* size */` |
|      1 | 6204 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|      1 | 6205 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|      - | 6206 | `	/* atime */` |
|      1 | 6207 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|      1 | 6208 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|      - | 6209 | `	/* mtime */` |
|      1 | 6210 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|      1 | 6211 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|      - | 6212 | `	/* ctime */` |
|      1 | 6213 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|      1 | 6214 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|      - | 6215 | `	/* blksize,blocks */` |
|      1 | 6216 | `	ph7_value_int(pWorker,0);` |
|      1 | 6217 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|      1 | 6218 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|      1 | 6219 | `	return PH7_OK;` |
|      1 | 6220 |  |
|      - | 6221 | `/* int (*xIsfile)(const char *) */` |
|      - | 6222 | `static int WinVfs_isfile(const char *zPath)` |
|      1 | 6223 |  |
|      - | 6224 | `	void * pConverted;` |
|      - | 6225 | `	DWORD dwAttr;` |
|      1 | 6226 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 6227 | `	if( pConverted == 0 ){` |
|    ! 0 | 6228 | `		return -1;` |
|      - | 6229 | `	}` |
|      1 | 6230 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|      1 | 6231 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 6232 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|      1 | 6233 | `		return -1;` |
|      - | 6234 | `	}` |
|      1 | 6235 | `	return (dwAttr & (FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE)) ? PH7_OK : -1;` |
|      1 | 6236 |  |
|      - | 6237 | `/* int (*xIslink)(const char *) */` |
|      - | 6238 | `static int WinVfs_islink(const char *zPath)` |
|    ! 0 | 6239 |  |
|      - | 6240 | `	void * pConverted;` |
|      - | 6241 | `	DWORD dwAttr;` |
|    ! 0 | 6242 | `	pConverted = convertUtf8Filename(zPath);` |
|    ! 0 | 6243 | `	if( pConverted == 0 ){` |
|    ! 0 | 6244 | `		return -1;` |
|      - | 6245 | `	}` |
|    ! 0 | 6246 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    ! 0 | 6247 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    ! 0 | 6248 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    ! 0 | 6249 | `		return -1;` |
|      - | 6250 | `	}` |
|    ! 0 | 6251 | `	return (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT) ? PH7_OK : -1;` |
|    ! 0 | 6252 |  |
|      - | 6253 | `/* int (*xWritable)(const char *) */` |
|      - | 6254 | `static int WinVfs_iswritable(const char *zPath)` |
|    ! 0 | 6255 |  |
|      - | 6256 | `	void * pConverted;` |
|      - | 6257 | `	DWORD dwAttr;` |
|    ! 0 | 6258 | `	pConverted = convertUtf8Filename(zPath);` |
|    ! 0 | 6259 | `	if( pConverted == 0 ){` |
|    ! 0 | 6260 | `		return -1;` |
|      - | 6261 | `	}` |
|    ! 0 | 6262 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    ! 0 | 6263 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    ! 0 | 6264 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    ! 0 | 6265 | `		return -1;` |
|      - | 6266 | `	}` |
|    ! 0 | 6267 | `	if( (dwAttr & (FILE_ATTRIBUTE_ARCHIVE\|FILE_ATTRIBUTE_NORMAL)) == 0 ){` |
|      - | 6268 | `		/* Not a regular file */` |
|    ! 0 | 6269 | `		return -1;` |
|      - | 6270 | `	}` |
|    ! 0 | 6271 | `	if( dwAttr & FILE_ATTRIBUTE_READONLY ){` |
|      - | 6272 | `		/* Read-only file */` |
|    ! 0 | 6273 | `		return -1;` |
|      - | 6274 | `	}` |
|      - | 6275 | `	/* File is writable */` |
|    ! 0 | 6276 | `	return PH7_OK;` |
|    ! 0 | 6277 |  |
|      - | 6278 | `/* int (*xExecutable)(const char *) */` |
|      - | 6279 | `static int WinVfs_isexecutable(const char *zPath)` |
|    ! 0 | 6280 |  |
|      - | 6281 | `	void * pConverted;` |
|      - | 6282 | `	DWORD dwAttr;` |
|    ! 0 | 6283 | `	pConverted = convertUtf8Filename(zPath);` |
|    ! 0 | 6284 | `	if( pConverted == 0 ){` |
|    ! 0 | 6285 | `		return -1;` |
|      - | 6286 | `	}` |
|    ! 0 | 6287 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|    ! 0 | 6288 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|    ! 0 | 6289 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|    ! 0 | 6290 | `		return -1;` |
|      - | 6291 | `	}` |
|    ! 0 | 6292 | `	if( (dwAttr & FILE_ATTRIBUTE_NORMAL) == 0 ){` |
|      - | 6293 | `		/* Not a regular file */` |
|    ! 0 | 6294 | `		return -1;` |
|      - | 6295 | `	}` |
|      - | 6296 | `	/* File is executable */` |
|    ! 0 | 6297 | `	return PH7_OK;` |
|    ! 0 | 6298 |  |
|      - | 6299 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 6300 | `static int WinVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|      1 | 6301 |  |
|      - | 6302 | `	void * pConverted;` |
|      - | 6303 | `	DWORD dwAttr;` |
|      1 | 6304 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 6305 | `	if( pConverted == 0 ){` |
|      - | 6306 | `		/* Expand 'unknown' */` |
|    ! 0 | 6307 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 6308 | `		return -1;` |
|      - | 6309 | `	}` |
|      1 | 6310 | `	dwAttr = GetFileAttributesW((LPCWSTR)pConverted);` |
|      1 | 6311 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 6312 | `	if( dwAttr == INVALID_FILE_ATTRIBUTES ){` |
|      - | 6313 | `		/* Expand 'unknown' */` |
|    ! 0 | 6314 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 6315 | `		return -1;` |
|      - | 6316 | `	}` |
|      1 | 6317 | `	if(dwAttr & (FILE_ATTRIBUTE_HIDDEN\|FILE_ATTRIBUTE_NORMAL\|FILE_ATTRIBUTE_ARCHIVE) ){` |
|      1 | 6318 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|      1 | 6319 | `	}else if(dwAttr & FILE_ATTRIBUTE_DIRECTORY){` |
|      1 | 6320 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|    ! 0 | 6321 | `	}else if(dwAttr & FILE_ATTRIBUTE_REPARSE_POINT){` |
|    ! 0 | 6322 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|    ! 0 | 6323 | `	}else if(dwAttr & (FILE_ATTRIBUTE_DEVICE)){` |
|    ! 0 | 6324 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|    ! 0 | 6325 | `	}else{` |
|    ! 0 | 6326 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|      - | 6327 | `	}` |
|      1 | 6328 | `	return PH7_OK;` |
|      1 | 6329 |  |
|      - | 6330 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 6331 | `static int WinVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|      1 | 6332 |  |
|      - | 6333 | `	char zValue[1024];` |
|      - | 6334 | `	DWORD n;` |
|      - | 6335 | `	/*` |
|      - | 6336 | `	 * According to MSDN` |
|      - | 6337 | `	 * If lpBuffer is not large enough to hold the data, the return` |
|      - | 6338 | `	 * value is the buffer size, in characters, required to hold the` |
|      - | 6339 | `	 * string and its terminating null character and the contents` |
|      - | 6340 | `	 * of lpBuffer are undefined.` |
|      - | 6341 | `	 */` |
|      1 | 6342 | `	n = sizeof(zValue);` |
|      1 | 6343 | `	SyMemcpy("Undefined",zValue,sizeof("Undefined")-1);` |
|      - | 6344 | `	/* Extract the environment value */` |
|      1 | 6345 | `	n = GetEnvironmentVariableA(zVar,zValue,sizeof(zValue));` |
|      1 | 6346 | `	if( !n ){` |
|      - | 6347 | `		/* No such variable*/` |
|    ! 0 | 6348 | `		return -1;` |
|      - | 6349 | `	}` |
|      1 | 6350 | `	ph7_result_string(pCtx,zValue,(int)n);` |
|      1 | 6351 | `	return PH7_OK;` |
|      1 | 6352 |  |
|      - | 6353 | `/* int (*xSetenv)(const char *,const char *) */` |
|      - | 6354 | `static int WinVfs_Setenv(const char *zName,const char *zValue)` |
|      1 | 6355 |  |
|      - | 6356 | `	BOOL rc;` |
|      1 | 6357 | `	rc = SetEnvironmentVariableA(zName,zValue);` |
|      1 | 6358 | `	return rc ? PH7_OK : -1;` |
|      1 | 6359 |  |
|      - | 6360 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 6361 | `static int WinVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|      1 | 6362 |  |
|      - | 6363 | `	DWORD dwSizeLow,dwSizeHigh;` |
|      - | 6364 | `	HANDLE pHandle,pMapHandle;` |
|      - | 6365 | `	void *pConverted,*pView;` |
|      - | 6366 |  |
|      1 | 6367 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 6368 | `	if( pConverted == 0 ){` |
|    ! 0 | 6369 | `		return -1;` |
|      - | 6370 | `	}` |
|      1 | 6371 | `	pHandle = OpenReadOnly((LPCWSTR)pConverted);` |
|      1 | 6372 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 6373 | `	if( pHandle == 0 ){` |
|      1 | 6374 | `		return -1;` |
|      - | 6375 | `	}` |
|      - | 6376 | `	/* Get the file size */` |
|      1 | 6377 | `	dwSizeLow = GetFileSize(pHandle,&dwSizeHigh);` |
|      - | 6378 | `	/* Create the mapping */` |
|      1 | 6379 | `	pMapHandle = CreateFileMappingW(pHandle,0,PAGE_READONLY,dwSizeHigh,dwSizeLow,0);` |
|      1 | 6380 | `	if( pMapHandle == 0 ){` |
|    ! 0 | 6381 | `		CloseHandle(pHandle);` |
|    ! 0 | 6382 | `		return -1;` |
|      - | 6383 | `	}` |
|      1 | 6384 | `	*pSize = ((ph7_int64)dwSizeHigh << 32) \| dwSizeLow;` |
|      - | 6385 | `	/* Obtain the view */` |
|      1 | 6386 | `	pView = MapViewOfFile(pMapHandle,FILE_MAP_READ,0,0,(SIZE_T)(*pSize));` |
|      1 | 6387 | `	if( pView ){` |
|      - | 6388 | `		/* Let the upper layer point to the view */` |
|      1 | 6389 | `		*ppMap = pView;` |
|      - | 6390 | `	}` |
|      - | 6391 | `	/* Close the handle` |
|      - | 6392 | `	 * According to MSDN it's OK the close the HANDLES.` |
|      - | 6393 | `	 */` |
|      1 | 6394 | `	CloseHandle(pMapHandle);` |
|      1 | 6395 | `	CloseHandle(pHandle);` |
|      1 | 6396 | `	return pView ? PH7_OK : -1;` |
|      1 | 6397 |  |
|      - | 6398 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|      - | 6399 | `static void WinVfs_Unmap(void *pView,ph7_int64 nSize)` |
|      1 | 6400 |  |
|      1 | 6401 | `	nSize = 0; /* Compiler warning */` |
|      1 | 6402 | `	UnmapViewOfFile(pView);` |
|      1 | 6403 |  |
|      - | 6404 | `/* void (*xTempDir)(ph7_context *) */` |
|      - | 6405 | `static void WinVfs_TempDir(ph7_context *pCtx)` |
|      1 | 6406 |  |
|      - | 6407 | `	CHAR zTemp[1024];` |
|      - | 6408 | `	DWORD n;` |
|      1 | 6409 | `	n = GetTempPathA(sizeof(zTemp),zTemp);` |
|      1 | 6410 | `	if( n < 1 ){` |
|      - | 6411 | `		/* Assume the default windows temp directory */` |
|    ! 0 | 6412 | `		ph7_result_string(pCtx,"C:\\Windows\\Temp",-1/*Compute length automatically*/);` |
|    ! 0 | 6413 | `	}else{` |
|      1 | 6414 | `		ph7_result_string(pCtx,zTemp,(int)n);` |
|      - | 6415 | `	}` |
|      1 | 6416 |  |
|      - | 6417 | `/* unsigned int (*xProcessId)(void) */` |
|      - | 6418 | `static unsigned int WinVfs_ProcessId(void)` |
|      1 | 6419 |  |
|      1 | 6420 | `	DWORD nID = 0;` |
|      - | 6421 | `#ifndef __MINGW32__` |
|      1 | 6422 | `	nID = GetProcessId(GetCurrentProcess());` |
|      - | 6423 | `#endif /* __MINGW32__ */` |
|      1 | 6424 | `	return (unsigned int)nID;` |
|      1 | 6425 |  |
|      - | 6426 | `/* void (*xUsername)(ph7_context *) */` |
|      - | 6427 | `static void WinVfs_Username(ph7_context *pCtx)` |
|      1 | 6428 |  |
|      - | 6429 | `	WCHAR zUser[1024];` |
|      - | 6430 | `	DWORD nByte;` |
|      - | 6431 | `	BOOL rc;` |
|      1 | 6432 | `	nByte = sizeof(zUser);` |
|      1 | 6433 | `	rc = GetUserNameW(zUser,&nByte);` |
|      1 | 6434 | `	if( !rc ){` |
|      - | 6435 | `		/* Set a dummy name */` |
|    ! 0 | 6436 | `		ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|    ! 0 | 6437 | `	}else{` |
|      - | 6438 | `		char *zName;` |
|      1 | 6439 | `		zName = unicodeToUtf8(zUser);` |
|      1 | 6440 | `		if( zName == 0 ){` |
|    ! 0 | 6441 | `			ph7_result_string(pCtx,"Unknown",sizeof("Unknown")-1);` |
|    ! 0 | 6442 | `		}else{` |
|      1 | 6443 | `			ph7_result_string(pCtx,zName,-1/*Compute length automatically*/); /* Will make it's own copy */` |
|      1 | 6444 | `			HeapFree(GetProcessHeap(),0,zName);` |
|      - | 6445 | `		}` |
|      - | 6446 | `	}` |
|      - | 6447 |  |
|      1 | 6448 |  |
|      - | 6449 | `/* Export the windows vfs */` |
|      - | 6450 | `static const ph7_vfs sWinVfs = {` |
|      - | 6451 | `	"Windows_vfs",` |
|      - | 6452 | `	PH7_VFS_VERSION,` |
|      - | 6453 | `	WinVfs_chdir,    /* int (*xChdir)(const char *) */` |
|      - | 6454 | `	0,               /* int (*xChroot)(const char *); */` |
|      - | 6455 | `	WinVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|      - | 6456 | `	WinVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|      - | 6457 | `	WinVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|      - | 6458 | `	WinVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|      - | 6459 | `	WinVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|      - | 6460 | `	WinVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 6461 | `	WinVfs_Sleep,               /* int (*xSleep)(unsigned int) */` |
|      - | 6462 | `	WinVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|      - | 6463 | `	WinVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|      - | 6464 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 6465 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 6466 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 6467 | `	WinVfs_DiskFreeSpace,/* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 6468 | `	WinVfs_DiskTotalSpace,/* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 6469 | `	WinVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 6470 | `	WinVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 6471 | `	WinVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 6472 | `	WinVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 6473 | `	WinVfs_Stat, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6474 | `	WinVfs_Stat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 6475 | `	WinVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|      - | 6476 | `	WinVfs_islink,     /* int (*xIslink)(const char *) */` |
|      - | 6477 | `	WinVfs_isfile,     /* int (*xReadable)(const char *) */` |
|      - | 6478 | `	WinVfs_iswritable, /* int (*xWritable)(const char *) */` |
|      - | 6479 | `	WinVfs_isexecutable, /* int (*xExecutable)(const char *) */` |
|      - | 6480 | `	WinVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 6481 | `	WinVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 6482 | `	WinVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|      - | 6483 | `	WinVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 6484 | `	WinVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 6485 | `	WinVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 6486 | `	0,                 /* int (*xLink)(const char *,const char *,int) */` |
|      - | 6487 | `	0,                 /* int (*xUmask)(int) */` |
|      - | 6488 | `	WinVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|      - | 6489 | `	WinVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|      - | 6490 | `	0, /* int (*xUid)(void) */` |
|      - | 6491 | `	0, /* int (*xGid)(void) */` |
|      - | 6492 | `	WinVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|      - | 6493 | `	0 /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 6494 | `};` |
|      - | 6495 | `/* Windows file IO */` |
|      - | 6496 | `#ifndef INVALID_SET_FILE_POINTER` |
|      - | 6497 | `# define INVALID_SET_FILE_POINTER ((DWORD)-1)` |
|      - | 6498 | `#endif` |
|      - | 6499 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|      - | 6500 | `static int WinFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|      1 | 6501 |  |
|      1 | 6502 | `	DWORD dwType = FILE_ATTRIBUTE_NORMAL \| FILE_FLAG_RANDOM_ACCESS;` |
|      1 | 6503 | `	DWORD dwAccess = GENERIC_READ;` |
|      - | 6504 | `	DWORD dwShare,dwCreate;` |
|      - | 6505 | `	void *pConverted;` |
|      - | 6506 | `	HANDLE pHandle;` |
|      - | 6507 |  |
|      1 | 6508 | `	pConverted = convertUtf8Filename(zPath);` |
|      1 | 6509 | `	if( pConverted == 0 ){` |
|    ! 0 | 6510 | `		return -1;` |
|      - | 6511 | `	}` |
|      - | 6512 | `	/* Set the desired flags according to the open mode */` |
|      1 | 6513 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|      - | 6514 | `		/* Open existing file, or create if it doesn't exist */` |
|      1 | 6515 | `		dwCreate = OPEN_ALWAYS;` |
|      1 | 6516 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|      - | 6517 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
|      1 | 6518 | `			dwCreate = CREATE_ALWAYS;` |
|      1 | 6519 | `		}` |
|      1 | 6520 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|      - | 6521 | `		/* Creates a new file, only if it does not already exist.` |
|      - | 6522 | `		* If the file exists, it fails.` |
|      - | 6523 | `		*/` |
|    ! 0 | 6524 | `		dwCreate = CREATE_NEW;` |
|      1 | 6525 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|      - | 6526 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|      - | 6527 | `		 * The file must exist.` |
|      - | 6528 | `		 */` |
|    ! 0 | 6529 | `		dwCreate = TRUNCATE_EXISTING;` |
|    ! 0 | 6530 | `	}else{` |
|      - | 6531 | `		/* Opens a file, only if it exists. */` |
|      1 | 6532 | `		dwCreate = OPEN_EXISTING;` |
|      - | 6533 | `	}` |
|      1 | 6534 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|      - | 6535 | `		/* Read+Write access */` |
|      1 | 6536 | `		dwAccess \|= GENERIC_WRITE;` |
|      1 | 6537 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|      - | 6538 | `		/* Write only access */` |
|      1 | 6539 | `		dwAccess = GENERIC_WRITE;` |
|      - | 6540 | `	}` |
|      1 | 6541 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|      - | 6542 | `		/* Append mode */` |
|    ! 0 | 6543 | `		dwAccess = FILE_APPEND_DATA;` |
|      - | 6544 | `	}` |
|      1 | 6545 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|      - | 6546 | `		/* File is temporary */` |
|    ! 0 | 6547 | `		dwType = FILE_ATTRIBUTE_TEMPORARY;` |
|      - | 6548 | `	}` |
|      1 | 6549 | `	dwShare = FILE_SHARE_READ \| FILE_SHARE_WRITE;` |
|      1 | 6550 | `	pHandle = CreateFileW((LPCWSTR)pConverted,dwAccess,dwShare,0,dwCreate,dwType,0);` |
|      1 | 6551 | `	HeapFree(GetProcessHeap(),0,pConverted);` |
|      1 | 6552 | `	if( pHandle == INVALID_HANDLE_VALUE){` |
|      - | 6553 | `		SXUNUSED(pResource); /* MSVC warning */` |
|      1 | 6554 | `		return -1;` |
|      - | 6555 | `	}` |
|      - | 6556 | `	/* Make the handle accessible to the upper layer */` |
|      1 | 6557 | `	*ppHandle = (void *)pHandle;` |
|      1 | 6558 | `	return PH7_OK;` |
|      1 | 6559 |  |
|      - | 6560 | `/* An instance of the following structure is used to record state information` |
|      - | 6561 | ` * while iterating throw directory entries.` |
|      - | 6562 | ` */` |
|      - | 6563 | `typedef struct WinDir_Info WinDir_Info;` |
|      - | 6564 | `struct WinDir_Info` |
|      - | 6565 |  |
|      - | 6566 | `	HANDLE pDirHandle;` |
|      - | 6567 | `	void *pPath;` |
|      - | 6568 | `	WIN32_FIND_DATAW sInfo;` |
|      - | 6569 | `	int rc;` |
|      - | 6570 | `};` |
|      - | 6571 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|      - | 6572 | `static int WinDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|      1 | 6573 |  |
|      - | 6574 | `	WinDir_Info *pDirInfo;` |
|      - | 6575 | `	void *pConverted;` |
|      - | 6576 | `	char *zPrep;` |
|      - | 6577 | `	sxu32 n;` |
|      - | 6578 | `	/* Prepare the path */` |
|      1 | 6579 | `	n = SyStrlen(zPath);` |
|      1 | 6580 | `	zPrep = (char *)HeapAlloc(GetProcessHeap(),0,n+sizeof("\\*")+4);` |
|      1 | 6581 | `	if( zPrep == 0 ){` |
|    ! 0 | 6582 | `		return -1;` |
|      - | 6583 | `	}` |
|      1 | 6584 | `	SyMemcpy((const void *)zPath,zPrep,n);` |
|      1 | 6585 | `	zPrep[n]   = '\\';` |
|      1 | 6586 | `	zPrep[n+1] =  '*';` |
|      1 | 6587 | `	zPrep[n+2] = 0;` |
|      1 | 6588 | `	pConverted = convertUtf8Filename(zPrep);` |
|      1 | 6589 | `	HeapFree(GetProcessHeap(),0,zPrep);` |
|      1 | 6590 | `	if( pConverted == 0 ){` |
|    ! 0 | 6591 | `		return -1;` |
|      - | 6592 | `	}` |
|      - | 6593 | `	/* Allocate a new instance */` |
|      1 | 6594 | `	pDirInfo = (WinDir_Info *)HeapAlloc(GetProcessHeap(),0,sizeof(WinDir_Info));` |
|      1 | 6595 | `	if( pDirInfo == 0 ){` |
|    ! 0 | 6596 | `		pResource = 0; /* Compiler warning */` |
|    ! 0 | 6597 | `		return -1;` |
|      - | 6598 | `	}` |
|      1 | 6599 | `	pDirInfo->rc = SXRET_OK;` |
|      1 | 6600 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pConverted,&pDirInfo->sInfo);` |
|      1 | 6601 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|      - | 6602 | `		/* Cannot open directory */` |
|    ! 0 | 6603 | `		HeapFree(GetProcessHeap(),0,pConverted);` |
|    ! 0 | 6604 | `		HeapFree(GetProcessHeap(),0,pDirInfo);` |
|    ! 0 | 6605 | `		return -1;` |
|      - | 6606 | `	}` |
|      - | 6607 | `	/* Save the path */` |
|      1 | 6608 | `	pDirInfo->pPath = pConverted;` |
|      - | 6609 | `	/* Save our structure */` |
|      1 | 6610 | `	*ppHandle = pDirInfo;` |
|      1 | 6611 | `	return PH7_OK;` |
|      1 | 6612 |  |
|      - | 6613 | `/* void (*xCloseDir)(void *) */` |
|      - | 6614 | `static void WinDir_Close(void *pUserData)` |
|      1 | 6615 |  |
|      1 | 6616 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|      1 | 6617 | `	if( pDirInfo->pDirHandle != INVALID_HANDLE_VALUE ){` |
|      1 | 6618 | `		FindClose(pDirInfo->pDirHandle);` |
|      - | 6619 | `	}` |
|      1 | 6620 | `	HeapFree(GetProcessHeap(),0,pDirInfo->pPath);` |
|      1 | 6621 | `	HeapFree(GetProcessHeap(),0,pDirInfo);` |
|      1 | 6622 |  |
|      - | 6623 | `/* void (*xClose)(void *); */` |
|      - | 6624 | `static void WinFile_Close(void *pUserData)` |
|      1 | 6625 |  |
|      1 | 6626 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      1 | 6627 | `	CloseHandle(pHandle);` |
|      1 | 6628 |  |
|      - | 6629 | `/* int (*xReadDir)(void *,ph7_context *) */` |
|      - | 6630 | `static int WinDir_Read(void *pUserData,ph7_context *pCtx)` |
|      1 | 6631 |  |
|      1 | 6632 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|      - | 6633 | `	LPWIN32_FIND_DATAW pData;` |
|      - | 6634 | `	char *zName;` |
|      - | 6635 | `	BOOL rc;` |
|      - | 6636 | `	sxu32 n;` |
|      1 | 6637 | `	if( pDirInfo->rc != SXRET_OK ){` |
|      - | 6638 | `		/* No more entry to process */` |
|      1 | 6639 | `		return -1;` |
|      - | 6640 | `	}` |
|      1 | 6641 | `	pData = &pDirInfo->sInfo;` |
|      - | 6642 | `	for(;;){` |
|      1 | 6643 | `		zName = unicodeToUtf8(pData->cFileName);` |
|      1 | 6644 | `		if( zName == 0 ){` |
|      - | 6645 | `			/* Out of memory */` |
|    ! 0 | 6646 | `			return -1;` |
|      - | 6647 | `		}` |
|      1 | 6648 | `		n = SyStrlen(zName);` |
|      - | 6649 | `		/* Ignore '.' && '..' */` |
|      1 | 6650 | `		if( n > sizeof("..")-1 \|\| zName[0] != '.' \|\| ( n == sizeof("..")-1 && zName[1] != '.') ){` |
|      1 | 6651 | `			break;` |
|      - | 6652 | `		}` |
|      1 | 6653 | `		HeapFree(GetProcessHeap(),0,zName);` |
|      1 | 6654 | `		rc = FindNextFileW(pDirInfo->pDirHandle,&pDirInfo->sInfo);` |
|      1 | 6655 | `		if( !rc ){` |
|    ! 0 | 6656 | `			return -1;` |
|      - | 6657 | `		}` |
|      1 | 6658 | `	}` |
|      - | 6659 | `	/* Return the current file name */` |
|      1 | 6660 | `	ph7_result_string(pCtx,zName,-1);` |
|      1 | 6661 | `	HeapFree(GetProcessHeap(),0,zName);` |
|      - | 6662 | `	/* Point to the next entry */` |
|      1 | 6663 | `	rc = FindNextFileW(pDirInfo->pDirHandle,&pDirInfo->sInfo);` |
|      1 | 6664 | `	if( !rc ){` |
|      1 | 6665 | `		pDirInfo->rc = SXERR_EOF;` |
|      - | 6666 | `	}` |
|      1 | 6667 | `	return PH7_OK;` |
|      1 | 6668 |  |
|      - | 6669 | `/* void (*xRewindDir)(void *) */` |
|      - | 6670 | `static void WinDir_RewindDir(void *pUserData)` |
|      1 | 6671 |  |
|      1 | 6672 | `	WinDir_Info *pDirInfo = (WinDir_Info *)pUserData;` |
|      1 | 6673 | `	FindClose(pDirInfo->pDirHandle);` |
|      1 | 6674 | `	pDirInfo->pDirHandle = FindFirstFileW((LPCWSTR)pDirInfo->pPath,&pDirInfo->sInfo);` |
|      1 | 6675 | `	if( pDirInfo->pDirHandle == INVALID_HANDLE_VALUE ){` |
|    ! 0 | 6676 | `		pDirInfo->rc = SXERR_EOF;` |
|    ! 0 | 6677 | `	}else{` |
|      1 | 6678 | `		pDirInfo->rc = SXRET_OK;` |
|      - | 6679 | `	}` |
|      1 | 6680 |  |
|      - | 6681 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
|      - | 6682 | `static ph7_int64 WinFile_Read(void *pOS,void *pBuffer,ph7_int64 nDatatoRead)` |
|      1 | 6683 |  |
|      1 | 6684 | `	HANDLE pHandle = (HANDLE)pOS;` |
|      - | 6685 | `	DWORD nRd;` |
|      - | 6686 | `	BOOL rc;` |
|      1 | 6687 | `	rc = ReadFile(pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|      1 | 6688 | `	if( !rc ){` |
|      - | 6689 | `		/* EOF or IO error */` |
|    ! 0 | 6690 | `		return -1;` |
|      - | 6691 | `	}` |
|      1 | 6692 | `	return (ph7_int64)nRd;` |
|      1 | 6693 |  |
|      - | 6694 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
|      - | 6695 | `static ph7_int64 WinFile_Write(void *pOS,const void *pBuffer,ph7_int64 nWrite)` |
|      1 | 6696 |  |
|      1 | 6697 | `	const char *zData = (const char *)pBuffer;` |
|      1 | 6698 | `	HANDLE pHandle = (HANDLE)pOS;` |
|      - | 6699 | `	ph7_int64 nCount;` |
|      - | 6700 | `	DWORD nWr;` |
|      - | 6701 | `	BOOL rc;` |
|      1 | 6702 | `	nWr = 0;` |
|      1 | 6703 | `	nCount = 0;` |
|      - | 6704 | `	for(;;){` |
|      1 | 6705 | `		if( nWrite < 1 ){` |
|      1 | 6706 | `			break;` |
|      - | 6707 | `		}` |
|      1 | 6708 | `		rc = WriteFile(pHandle,zData,(DWORD)nWrite,&nWr,0);` |
|      1 | 6709 | `		if( !rc ){` |
|      - | 6710 | `			/* IO error */` |
|    ! 0 | 6711 | `			break;` |
|      - | 6712 | `		}` |
|      1 | 6713 | `		nWrite -= nWr;` |
|      1 | 6714 | `		nCount += nWr;` |
|      1 | 6715 | `		zData += nWr;` |
|      1 | 6716 | `	}` |
|      1 | 6717 | `	if( nWrite > 0 ){` |
|    ! 0 | 6718 | `		return -1;` |
|      - | 6719 | `	}` |
|      1 | 6720 | `	return nCount;` |
|      1 | 6721 |  |
|      - | 6722 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|      - | 6723 | `static int WinFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|      1 | 6724 |  |
|      1 | 6725 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6726 | `	DWORD dwMove,dwNew;` |
|      - | 6727 | `	LONG nHighOfft;` |
|      1 | 6728 | `	switch(whence){` |
|      - | 6729 | `	case 1:/*SEEK_CUR*/` |
|    ! 0 | 6730 | `		dwMove = FILE_CURRENT;` |
|    ! 0 | 6731 | `		break;` |
|      - | 6732 | `	case 2: /* SEEK_END */` |
|    ! 0 | 6733 | `		dwMove = FILE_END;` |
|    ! 0 | 6734 | `		break;` |
|      - | 6735 | `	case 0: /* SEEK_SET */` |
|      - | 6736 | `	default:` |
|      1 | 6737 | `		dwMove = FILE_BEGIN;` |
|      - | 6738 | `		break;` |
|      - | 6739 | `	}` |
|      1 | 6740 | `	nHighOfft = (LONG)(iOfft >> 32);` |
|      1 | 6741 | `	dwNew = SetFilePointer(pHandle,(LONG)iOfft,&nHighOfft,dwMove);` |
|      1 | 6742 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|    ! 0 | 6743 | `		return -1;` |
|      - | 6744 | `	}` |
|      1 | 6745 | `	return PH7_OK;` |
|      1 | 6746 |  |
|      - | 6747 | `/* int (*xLock)(void *,int) */` |
|      - | 6748 | `static int WinFile_Lock(void *pUserData,int lock_type)` |
|      1 | 6749 |  |
|      1 | 6750 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6751 | `	static DWORD dwLo = 0,dwHi = 0; /* xx: MT-SAFE */` |
|      - | 6752 | `	OVERLAPPED sDummy;` |
|      - | 6753 | `	BOOL rc;` |
|      1 | 6754 | `	SyZero(&sDummy,sizeof(sDummy));` |
|      - | 6755 | `	/* Get the file size */` |
|      1 | 6756 | `	if( lock_type < 1 ){` |
|      - | 6757 | `		/* Unlock the file */` |
|      1 | 6758 | `		rc = UnlockFileEx(pHandle,0,dwLo,dwHi,&sDummy);` |
|      1 | 6759 | `	}else{` |
|      1 | 6760 | `		DWORD dwFlags = LOCKFILE_FAIL_IMMEDIATELY; /* Shared non-blocking lock by default*/` |
|      - | 6761 | `		/* Lock the file */` |
|      1 | 6762 | `		if( lock_type == 1 /* LOCK_EXCL */ ){` |
|      1 | 6763 | `			dwFlags \|= LOCKFILE_EXCLUSIVE_LOCK;` |
|      - | 6764 | `		}` |
|      1 | 6765 | `		dwLo = GetFileSize(pHandle,&dwHi);` |
|      1 | 6766 | `		rc = LockFileEx(pHandle,dwFlags,0,dwLo,dwHi,&sDummy);` |
|      - | 6767 | `	}` |
|      1 | 6768 | `	return rc ? PH7_OK : -1 /* Lock error */;` |
|      1 | 6769 |  |
|      - | 6770 | `/* ph7_int64 (*xTell)(void *) */` |
|      - | 6771 | `static ph7_int64 WinFile_Tell(void *pUserData)` |
|      1 | 6772 |  |
|      1 | 6773 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6774 | `	DWORD dwNew;` |
|      1 | 6775 | `	dwNew = SetFilePointer(pHandle,0,0,FILE_CURRENT/* SEEK_CUR */);` |
|      1 | 6776 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|    ! 0 | 6777 | `		return -1;` |
|      - | 6778 | `	}` |
|      1 | 6779 | `	return (ph7_int64)dwNew;` |
|      1 | 6780 |  |
|      - | 6781 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|      - | 6782 | `static int WinFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|      1 | 6783 |  |
|      1 | 6784 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6785 | `	LONG HighOfft;` |
|      - | 6786 | `	DWORD dwNew;` |
|      - | 6787 | `	BOOL rc;` |
|      1 | 6788 | `	HighOfft = (LONG)(nOfft >> 32);` |
|      1 | 6789 | `	dwNew = SetFilePointer(pHandle,(LONG)nOfft,&HighOfft,FILE_BEGIN);` |
|      1 | 6790 | `	if( dwNew == INVALID_SET_FILE_POINTER ){` |
|    ! 0 | 6791 | `		return -1;` |
|      - | 6792 | `	}` |
|      1 | 6793 | `	rc = SetEndOfFile(pHandle);` |
|      1 | 6794 | `	return rc ? PH7_OK : -1;` |
|      1 | 6795 |  |
|      - | 6796 | `/* int (*xSync)(void *); */` |
|      - | 6797 | `static int WinFile_Sync(void *pUserData)` |
|      1 | 6798 |  |
|      1 | 6799 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6800 | `	BOOL rc;` |
|      1 | 6801 | `	rc = FlushFileBuffers(pHandle);` |
|      1 | 6802 | `	return rc ? PH7_OK : - 1;` |
|      1 | 6803 |  |
|      - | 6804 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|      - | 6805 | `static int WinFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|      1 | 6806 |  |
|      - | 6807 | `	BY_HANDLE_FILE_INFORMATION sInfo;` |
|      1 | 6808 | `	HANDLE pHandle = (HANDLE)pUserData;` |
|      - | 6809 | `	BOOL rc;` |
|      1 | 6810 | `	rc = GetFileInformationByHandle(pHandle,&sInfo);` |
|      1 | 6811 | `	if( !rc ){` |
|    ! 0 | 6812 | `		return -1;` |
|      - | 6813 | `	}` |
|      - | 6814 | `	/* dev */` |
|      1 | 6815 | `	ph7_value_int64(pWorker,(ph7_int64)sInfo.dwVolumeSerialNumber);` |
|      1 | 6816 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|      - | 6817 | `	/* ino */` |
|      1 | 6818 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileIndexHigh << 32) \| sInfo.nFileIndexLow));` |
|      1 | 6819 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|      - | 6820 | `	/* mode */` |
|      1 | 6821 | `	ph7_value_int(pWorker,0);` |
|      1 | 6822 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|      - | 6823 | `	/* nlink */` |
|      1 | 6824 | `	ph7_value_int(pWorker,(int)sInfo.nNumberOfLinks);` |
|      1 | 6825 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|      - | 6826 | `	/* uid,gid,rdev */` |
|      1 | 6827 | `	ph7_value_int(pWorker,0);` |
|      1 | 6828 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|      1 | 6829 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|      1 | 6830 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|      - | 6831 | `	/* size */` |
|      1 | 6832 | `	ph7_value_int64(pWorker,(ph7_int64)(((ph7_int64)sInfo.nFileSizeHigh << 32) \| sInfo.nFileSizeLow));` |
|      1 | 6833 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|      - | 6834 | `	/* atime */` |
|      1 | 6835 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastAccessTime));` |
|      1 | 6836 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|      - | 6837 | `	/* mtime */` |
|      1 | 6838 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftLastWriteTime));` |
|      1 | 6839 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|      - | 6840 | `	/* ctime */` |
|      1 | 6841 | `	ph7_value_int64(pWorker,convertWindowsTimeToUnixTime(&sInfo.ftCreationTime));` |
|      1 | 6842 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|      - | 6843 | `	/* blksize,blocks */` |
|      1 | 6844 | `	ph7_value_int(pWorker,0);` |
|      1 | 6845 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|      1 | 6846 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|      1 | 6847 | `	return PH7_OK;` |
|      1 | 6848 |  |
|      - | 6849 | `/* Export the file:// stream */` |
|      - | 6850 | `static const ph7_io_stream sWinFileStream = {` |
|      - | 6851 | `	"file", /* Stream name */` |
|      - | 6852 | `	PH7_IO_STREAM_VERSION,` |
|      - | 6853 | `	WinFile_Open,  /* xOpen */` |
|      - | 6854 | `	WinDir_Open,   /* xOpenDir */` |
|      - | 6855 | `	WinFile_Close, /* xClose */` |
|      - | 6856 | `	WinDir_Close,  /* xCloseDir */` |
|      - | 6857 | `	WinFile_Read,  /* xRead */` |
|      - | 6858 | `	WinDir_Read,   /* xReadDir */` |
|      - | 6859 | `	WinFile_Write, /* xWrite */` |
|      - | 6860 | `	WinFile_Seek,  /* xSeek */` |
|      - | 6861 | `	WinFile_Lock,  /* xLock */` |
|      - | 6862 | `	WinDir_RewindDir, /* xRewindDir */` |
|      - | 6863 | `	WinFile_Tell,  /* xTell */` |
|      - | 6864 | `	WinFile_Trunc, /* xTrunc */` |
|      - | 6865 | `	WinFile_Sync,  /* xSeek */` |
|      - | 6866 | `	WinFile_Stat   /* xStat */` |
|      - | 6867 | `};` |
|      - | 6868 | `#elif defined(__UNIXES__)` |
|      - | 6869 | `/*` |
|      - | 6870 | ` * UNIX VFS implementation for the PH7 engine.` |
|      - | 6871 | ` * Status:` |
|      - | 6872 | ` *    Stable.` |
|      - | 6873 | ` */` |
|      - | 6874 | `#include <sys/types.h>` |
|      - | 6875 | `#include <limits.h>` |
|      - | 6876 | `#include <fcntl.h>` |
|      - | 6877 | `#include <unistd.h>` |
|      - | 6878 | `#include <sys/uio.h>` |
|      - | 6879 | `#include <sys/stat.h>` |
|      - | 6880 | `#include <sys/mman.h>` |
|      - | 6881 | `#include <sys/file.h>` |
|      - | 6882 | `#include <sys/wait.h>` |
|      - | 6883 | `#include <pwd.h>` |
|      - | 6884 | `#include <grp.h>` |
|      - | 6885 | `#include <dirent.h>` |
|      - | 6886 | `#include <utime.h>` |
|      - | 6887 | `#include <stdio.h>` |
|      - | 6888 | `#include <stdlib.h>` |
|      - | 6889 | `/* int (*xchdir)(const char *) */` |
|      4 | 6890 | `static int UnixVfs_chdir(const char *zPath)` |
|      - | 6891 |  |
|      - | 6892 | `  int rc;` |
|      4 | 6893 | `  rc = chdir(zPath);` |
|      4 | 6894 | `  return rc == 0 ? PH7_OK : -1;` |
|      - | 6895 |  |
|      - | 6896 | `/* int (*xGetcwd)(ph7_context *) */` |
|     16 | 6897 | `static int UnixVfs_getcwd(ph7_context *pCtx)` |
|      - | 6898 |  |
|      - | 6899 | `	char zBuf[4096];` |
|      - | 6900 | `	char *zDir;` |
|      - | 6901 | `	/* Get the current directory */` |
|     16 | 6902 | `	zDir = getcwd(zBuf,sizeof(zBuf));` |
|     16 | 6903 | `	if( zDir == 0 ){` |
|    ! 0 | 6904 | `	  return -1;` |
|      - | 6905 | `    }` |
|     16 | 6906 | `	ph7_result_string(pCtx,zDir,-1/*Compute length automatically*/);` |
|     16 | 6907 | `	return PH7_OK;` |
|      8 | 6908 |  |
|      - | 6909 | `/* int (*xMkdir)(const char *,int,int) */` |
|      6 | 6910 | `static int UnixVfs_mkdir(const char *zPath,int mode,int recursive)` |
|      - | 6911 |  |
|      - | 6912 | `	int rc;` |
|      6 | 6913 | `        rc = mkdir(zPath,mode);` |
|      3 | 6914 | `	SXUNUSED(recursive); /* cc warning */` |
|      6 | 6915 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 6916 |  |
|      - | 6917 | `/* int (*xRmdir)(const char *) */` |
|      8 | 6918 | `static int UnixVfs_rmdir(const char *zPath)` |
|      - | 6919 |  |
|      - | 6920 | `	int rc;` |
|      8 | 6921 | `	rc = rmdir(zPath);` |
|      8 | 6922 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 6923 |  |
|      - | 6924 | `/* int (*xIsdir)(const char *) */` |
|      6 | 6925 | `static int UnixVfs_isdir(const char *zPath)` |
|      - | 6926 |  |
|      - | 6927 | `	struct stat st;` |
|      - | 6928 | `	int rc;` |
|      6 | 6929 | `	rc = stat(zPath,&st);` |
|      6 | 6930 | `	if( rc != 0 ){` |
|      2 | 6931 | `	 return -1;` |
|      - | 6932 | `	}` |
|      4 | 6933 | `	rc = S_ISDIR(st.st_mode);` |
|      4 | 6934 | `	return rc ? PH7_OK : -1 ;` |
|      3 | 6935 |  |
|      - | 6936 | `/* int (*xRename)(const char *,const char *) */` |
|      2 | 6937 | `static int UnixVfs_Rename(const char *zOld,const char *zNew)` |
|      - | 6938 |  |
|      - | 6939 | `	int rc;` |
|      2 | 6940 | `	rc = rename(zOld,zNew);` |
|      2 | 6941 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 6942 |  |
|      - | 6943 | `/* int (*xRealpath)(const char *,ph7_context *) */` |
|      4 | 6944 | `static int UnixVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|      - | 6945 |  |
|      - | 6946 | `#ifndef PH7_UNIX_OLD_LIBC` |
|      - | 6947 | `	char *zReal;` |
|      4 | 6948 | `	zReal = realpath(zPath,0);` |
|      4 | 6949 | `	if( zReal == 0 ){` |
|      2 | 6950 | `	  return -1;` |
|      - | 6951 | `	}` |
|      2 | 6952 | `	ph7_result_string(pCtx,zReal,-1/*Compute length automatically*/);` |
|      - | 6953 | `        /* Release the allocated buffer */` |
|      2 | 6954 | `	free(zReal);` |
|      2 | 6955 | `	return PH7_OK;` |
|      - | 6956 | `#else` |
|      - | 6957 | `    zPath = 0; /* cc warning */` |
|      - | 6958 | `    pCtx = 0;` |
|      - | 6959 | `    return -1;` |
|      - | 6960 | `#endif` |
|      2 | 6961 |  |
|      - | 6962 | `/* int (*xSleep)(unsigned int) */` |
|      4 | 6963 | `static int UnixVfs_Sleep(unsigned int uSec)` |
|      - | 6964 |  |
|      4 | 6965 | `	usleep(uSec);` |
|      4 | 6966 | `	return PH7_OK;` |
|      - | 6967 |  |
|      - | 6968 | `/* int (*xUnlink)(const char *) */` |
|     86 | 6969 | `static int UnixVfs_unlink(const char *zPath)` |
|      - | 6970 |  |
|      - | 6971 | `	int rc;` |
|     86 | 6972 | `	rc = unlink(zPath);` |
|     86 | 6973 | `	return rc == 0 ? PH7_OK : -1 ;` |
|      - | 6974 |  |
|      - | 6975 | `/* int (*xFileExists)(const char *) */` |
|     36 | 6976 | `static int UnixVfs_FileExists(const char *zPath)` |
|      - | 6977 |  |
|      - | 6978 | `	int rc;` |
|     36 | 6979 | `	rc = access(zPath,F_OK);` |
|     36 | 6980 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 6981 |  |
|      - | 6982 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|     26 | 6983 | `static ph7_int64 UnixVfs_FileSize(const char *zPath)` |
|      - | 6984 |  |
|      - | 6985 | `	struct stat st;` |
|      - | 6986 | `	int rc;` |
|     26 | 6987 | `	rc = stat(zPath,&st);` |
|     26 | 6988 | `	if( rc != 0 ){` |
|    ! 0 | 6989 | `	 return -1;` |
|      - | 6990 | `	}` |
|     26 | 6991 | `	return (ph7_int64)st.st_size;` |
|     13 | 6992 |  |
|      - | 6993 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      4 | 6994 | `static int UnixVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|      - | 6995 |  |
|      - | 6996 | `	struct utimbuf ut;` |
|      - | 6997 | `	int rc;` |
|      4 | 6998 | `	ut.actime  = (time_t)access_time;` |
|      4 | 6999 | `	ut.modtime = (time_t)touch_time;` |
|      4 | 7000 | `	rc = utime(zPath,&ut);` |
|      4 | 7001 | `	if( rc != 0 ){` |
|    ! 0 | 7002 | `	 return -1;` |
|      - | 7003 | `	}` |
|      4 | 7004 | `	return PH7_OK;` |
|      2 | 7005 |  |
|      - | 7006 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|      2 | 7007 | `static ph7_int64 UnixVfs_FileAtime(const char *zPath)` |
|      - | 7008 |  |
|      - | 7009 | `	struct stat st;` |
|      - | 7010 | `	int rc;` |
|      2 | 7011 | `	rc = stat(zPath,&st);` |
|      2 | 7012 | `	if( rc != 0 ){` |
|    ! 0 | 7013 | `	 return -1;` |
|      - | 7014 | `	}` |
|      2 | 7015 | `	return (ph7_int64)st.st_atime;` |
|      1 | 7016 |  |
|      - | 7017 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|      4 | 7018 | `static ph7_int64 UnixVfs_FileMtime(const char *zPath)` |
|      - | 7019 |  |
|      - | 7020 | `	struct stat st;` |
|      - | 7021 | `	int rc;` |
|      4 | 7022 | `	rc = stat(zPath,&st);` |
|      4 | 7023 | `	if( rc != 0 ){` |
|    ! 0 | 7024 | `	 return -1;` |
|      - | 7025 | `	}` |
|      4 | 7026 | `	return (ph7_int64)st.st_mtime;` |
|      2 | 7027 |  |
|      - | 7028 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|      2 | 7029 | `static ph7_int64 UnixVfs_FileCtime(const char *zPath)` |
|      - | 7030 |  |
|      - | 7031 | `	struct stat st;` |
|      - | 7032 | `	int rc;` |
|      2 | 7033 | `	rc = stat(zPath,&st);` |
|      2 | 7034 | `	if( rc != 0 ){` |
|    ! 0 | 7035 | `	 return -1;` |
|      - | 7036 | `	}` |
|      2 | 7037 | `	return (ph7_int64)st.st_ctime;` |
|      1 | 7038 |  |
|      - | 7039 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      4 | 7040 | `static int UnixVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|      - | 7041 |  |
|      - | 7042 | `	struct stat st;` |
|      - | 7043 | `	int rc;` |
|      4 | 7044 | `	rc = stat(zPath,&st);` |
|      4 | 7045 | `	if( rc != 0 ){` |
|    ! 0 | 7046 | `	 return -1;` |
|      - | 7047 | `	}` |
|      - | 7048 | `	/* dev */` |
|      4 | 7049 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|      4 | 7050 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|      - | 7051 | `	/* ino */` |
|      4 | 7052 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|      4 | 7053 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|      - | 7054 | `	/* mode */` |
|      4 | 7055 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|      4 | 7056 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|      - | 7057 | `	/* nlink */` |
|      4 | 7058 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|      4 | 7059 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|      - | 7060 | `	/* uid,gid,rdev */` |
|      4 | 7061 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|      4 | 7062 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|      4 | 7063 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|      4 | 7064 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|      4 | 7065 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|      4 | 7066 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|      - | 7067 | `	/* size */` |
|      4 | 7068 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|      4 | 7069 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|      - | 7070 | `	/* atime */` |
|      4 | 7071 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|      4 | 7072 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|      - | 7073 | `	/* mtime */` |
|      4 | 7074 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|      4 | 7075 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|      - | 7076 | `	/* ctime */` |
|      4 | 7077 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|      4 | 7078 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|      - | 7079 | `	/* blksize,blocks */` |
|      4 | 7080 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|      4 | 7081 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|      4 | 7082 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|      4 | 7083 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|      4 | 7084 | `	return PH7_OK;` |
|      2 | 7085 |  |
|      - | 7086 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      2 | 7087 | `static int UnixVfs_lStat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|      - | 7088 |  |
|      - | 7089 | `	struct stat st;` |
|      - | 7090 | `	int rc;` |
|      2 | 7091 | `	rc = lstat(zPath,&st);` |
|      2 | 7092 | `	if( rc != 0 ){` |
|    ! 0 | 7093 | `	 return -1;` |
|      - | 7094 | `	}` |
|      - | 7095 | `	/* dev */` |
|      2 | 7096 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|      2 | 7097 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|      - | 7098 | `	/* ino */` |
|      2 | 7099 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|      2 | 7100 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|      - | 7101 | `	/* mode */` |
|      2 | 7102 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|      2 | 7103 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|      - | 7104 | `	/* nlink */` |
|      2 | 7105 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|      2 | 7106 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|      - | 7107 | `	/* uid,gid,rdev */` |
|      2 | 7108 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|      2 | 7109 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|      2 | 7110 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|      2 | 7111 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|      2 | 7112 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|      2 | 7113 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|      - | 7114 | `	/* size */` |
|      2 | 7115 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|      2 | 7116 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|      - | 7117 | `	/* atime */` |
|      2 | 7118 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|      2 | 7119 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|      - | 7120 | `	/* mtime */` |
|      2 | 7121 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|      2 | 7122 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|      - | 7123 | `	/* ctime */` |
|      2 | 7124 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|      2 | 7125 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|      - | 7126 | `	/* blksize,blocks */` |
|      2 | 7127 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|      2 | 7128 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|      2 | 7129 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|      2 | 7130 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|      2 | 7131 | `	return PH7_OK;` |
|      1 | 7132 |  |
|      - | 7133 | `/* int (*xChmod)(const char *,int) */` |
|      8 | 7134 | `static int UnixVfs_Chmod(const char *zPath,int mode)` |
|      - | 7135 |  |
|      - | 7136 | `    int rc;` |
|      8 | 7137 | `    rc = chmod(zPath,(mode_t)mode);` |
|      8 | 7138 | `    return rc == 0 ? PH7_OK : - 1;` |
|      - | 7139 |  |
|      - | 7140 | `/* int (*xChown)(const char *,const char *) */` |
|      4 | 7141 | `static int UnixVfs_Chown(const char *zPath,const char *zUser)` |
|      - | 7142 |  |
|      - | 7143 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|      - | 7144 | `  struct passwd *pwd;` |
|      - | 7145 | `  uid_t uid;` |
|      - | 7146 | `  int rc;` |
|      4 | 7147 | `  pwd = getpwnam(zUser);   /* Try getting UID for username */` |
|      4 | 7148 | `  if (pwd == 0) {` |
|      4 | 7149 | `    return -1;` |
|      - | 7150 | `  }` |
|    ! 0 | 7151 | `  uid = pwd->pw_uid;` |
|    ! 0 | 7152 | `  rc = chown(zPath,uid,-1);` |
|    ! 0 | 7153 | `  return rc == 0 ? PH7_OK : -1;` |
|      - | 7154 | `#else` |
|      - | 7155 | `	SXUNUSED(zPath);` |
|      - | 7156 | `	SXUNUSED(zUser);` |
|      - | 7157 | `	return -1;` |
|      - | 7158 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|      2 | 7159 |  |
|      - | 7160 | `/* int (*xChgrp)(const char *,const char *) */` |
|      4 | 7161 | `static int UnixVfs_Chgrp(const char *zPath,const char *zGroup)` |
|      - | 7162 |  |
|      - | 7163 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|      - | 7164 | `  struct group *group;` |
|      - | 7165 | `  gid_t gid;` |
|      - | 7166 | `  int rc;` |
|      4 | 7167 | `  group = getgrnam(zGroup);` |
|      4 | 7168 | `  if (group == 0) {` |
|      4 | 7169 | `    return -1;` |
|      - | 7170 | `  }` |
|    ! 0 | 7171 | `  gid = group->gr_gid;` |
|    ! 0 | 7172 | `  rc = chown(zPath,-1,gid);` |
|    ! 0 | 7173 | `  return rc == 0 ? PH7_OK : -1;` |
|      - | 7174 | `#else` |
|      - | 7175 | `	SXUNUSED(zPath);` |
|      - | 7176 | `	SXUNUSED(zGroup);` |
|      - | 7177 | `	return -1;` |
|      - | 7178 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|      2 | 7179 |  |
|      - | 7180 | `/* int (*xIsfile)(const char *) */` |
|      4 | 7181 | `static int UnixVfs_isfile(const char *zPath)` |
|      - | 7182 |  |
|      - | 7183 | `	struct stat st;` |
|      - | 7184 | `	int rc;` |
|      4 | 7185 | `	rc = stat(zPath,&st);` |
|      4 | 7186 | `	if( rc != 0 ){` |
|      2 | 7187 | `	 return -1;` |
|      - | 7188 | `	}` |
|      2 | 7189 | `	rc = S_ISREG(st.st_mode);` |
|      2 | 7190 | `	return rc ? PH7_OK : -1 ;` |
|      2 | 7191 |  |
|      - | 7192 | `/* int (*xIslink)(const char *) */` |
|      4 | 7193 | `static int UnixVfs_islink(const char *zPath)` |
|      - | 7194 |  |
|      - | 7195 | `	struct stat st;` |
|      - | 7196 | `	int rc;` |
|      4 | 7197 | `	rc = stat(zPath,&st);` |
|      4 | 7198 | `	if( rc != 0 ){` |
|    ! 0 | 7199 | `	 return -1;` |
|      - | 7200 | `	}` |
|      4 | 7201 | `	rc = S_ISLNK(st.st_mode);` |
|      4 | 7202 | `	return rc ? PH7_OK : -1 ;` |
|      2 | 7203 |  |
|      - | 7204 | `/* int (*xReadable)(const char *) */` |
|      2 | 7205 | `static int UnixVfs_isreadable(const char *zPath)` |
|      - | 7206 |  |
|      - | 7207 | `	int rc;` |
|      2 | 7208 | `	rc = access(zPath,R_OK);` |
|      2 | 7209 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 7210 |  |
|      - | 7211 | `/* int (*xWritable)(const char *) */` |
|      4 | 7212 | `static int UnixVfs_iswritable(const char *zPath)` |
|      - | 7213 |  |
|      - | 7214 | `	int rc;` |
|      4 | 7215 | `	rc = access(zPath,W_OK);` |
|      4 | 7216 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 7217 |  |
|      - | 7218 | `/* int (*xExecutable)(const char *) */` |
|      2 | 7219 | `static int UnixVfs_isexecutable(const char *zPath)` |
|      - | 7220 |  |
|      - | 7221 | `	int rc;` |
|      2 | 7222 | `	rc = access(zPath,X_OK);` |
|      2 | 7223 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 7224 |  |
|      - | 7225 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|      4 | 7226 | `static int UnixVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|      - | 7227 |  |
|      - | 7228 | `	struct stat st;` |
|      - | 7229 | `	int rc;` |
|      4 | 7230 | `    rc = stat(zPath,&st);` |
|      4 | 7231 | `	if( rc != 0 ){` |
|      - | 7232 | `	  /* Expand 'unknown' */` |
|    ! 0 | 7233 | `	  ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 7234 | `	  return -1;` |
|      - | 7235 | `	}` |
|      4 | 7236 | `	if(S_ISREG(st.st_mode) ){` |
|      2 | 7237 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|      3 | 7238 | `	}else if(S_ISDIR(st.st_mode)){` |
|      2 | 7239 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|      1 | 7240 | `	}else if(S_ISLNK(st.st_mode)){` |
|    ! 0 | 7241 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|    ! 0 | 7242 | `	}else if(S_ISBLK(st.st_mode)){` |
|    ! 0 | 7243 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|    ! 0 | 7244 | `    }else if(S_ISSOCK(st.st_mode)){` |
|    ! 0 | 7245 | `		ph7_result_string(pCtx,"socket",sizeof("socket")-1);` |
|    ! 0 | 7246 | `	}else if(S_ISFIFO(st.st_mode)){` |
|    ! 0 | 7247 | `       ph7_result_string(pCtx,"fifo",sizeof("fifo")-1);` |
|    ! 0 | 7248 | `	}else{` |
|    ! 0 | 7249 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|      - | 7250 | `	}` |
|      4 | 7251 | `	return PH7_OK;` |
|      2 | 7252 |  |
|      - | 7253 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|     16 | 7254 | `static int UnixVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|      - | 7255 |  |
|      - | 7256 | `	char *zEnv;` |
|     16 | 7257 | `	zEnv = getenv(zVar);` |
|     16 | 7258 | `	if( zEnv == 0 ){` |
|    ! 0 | 7259 | `	  return -1;` |
|      - | 7260 | `	}` |
|     16 | 7261 | `	ph7_result_string(pCtx,zEnv,-1/*Compute length automatically*/);` |
|     16 | 7262 | `	return PH7_OK;` |
|      8 | 7263 |  |
|      - | 7264 | `/* int (*xSetenv)(const char *,const char *) */` |
|      2 | 7265 | `static int UnixVfs_Setenv(const char *zName,const char *zValue)` |
|      - | 7266 |  |
|      - | 7267 | `   int rc;` |
|      2 | 7268 | `   rc = setenv(zName,zValue,1);` |
|      2 | 7269 | `   return rc == 0 ? PH7_OK : -1;` |
|      - | 7270 |  |
|      - | 7271 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|   5356 | 7272 | `static int UnixVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|      - | 7273 |  |
|      - | 7274 | `	struct stat st;` |
|      - | 7275 | `	void *pMap;` |
|      - | 7276 | `	int fd;` |
|      - | 7277 | `	int rc;` |
|      - | 7278 | `	/* Open the file in a read-only mode */` |
|   5356 | 7279 | `	fd = open(zPath,O_RDONLY);` |
|   5356 | 7280 | `	if( fd < 0 ){` |
|      2 | 7281 | `		return -1;` |
|      - | 7282 | `	}` |
|      - | 7283 | `	/* stat the handle */` |
|   5354 | 7284 | `	fstat(fd,&st);` |
|      - | 7285 | `	/* Obtain a memory view of the whole file */` |
|   5354 | 7286 | `	pMap = mmap(0,st.st_size,PROT_READ,MAP_PRIVATE\|MAP_FILE,fd,0);` |
|   5354 | 7287 | `	rc = PH7_OK;` |
|   5354 | 7288 | `	if( pMap == MAP_FAILED ){` |
|    ! 0 | 7289 | `		rc = -1;` |
|    ! 0 | 7290 | `	}else{` |
|      - | 7291 | `		/* Point to the memory view */` |
|   5354 | 7292 | `		*ppMap = pMap;` |
|   5354 | 7293 | `		*pSize = (ph7_int64)st.st_size;` |
|      - | 7294 | `	}` |
|   5354 | 7295 | `	close(fd);` |
|   5354 | 7296 | `	return rc;` |
|   2678 | 7297 |  |
|      - | 7298 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|   5354 | 7299 | `static void UnixVfs_Unmap(void *pView,ph7_int64 nSize)` |
|      - | 7300 |  |
|   5354 | 7301 | `	munmap(pView,(size_t)nSize);` |
|   5354 | 7302 |  |
|      - | 7303 | `/* void (*xTempDir)(ph7_context *) */` |
|    168 | 7304 | `static void UnixVfs_TempDir(ph7_context *pCtx)` |
|      - | 7305 |  |
|      - | 7306 | `	static const char *azDirs[] = {` |
|      - | 7307 | `     "/var/tmp",` |
|      - | 7308 | `     "/usr/tmp",` |
|      - | 7309 | `	 "/usr/local/tmp"` |
|      - | 7310 | `  };` |
|      - | 7311 | `  unsigned int i;` |
|      - | 7312 | `  struct stat buf;` |
|      - | 7313 | `  const char *zDir;` |
|    168 | 7314 | `  zDir = getenv("TMPDIR");` |
|    168 | 7315 | `  if( zDir && zDir[0] != 0 && !access(zDir,07) ){` |
|     84 | 7316 | `	  ph7_result_string(pCtx,zDir,-1);` |
|     84 | 7317 | `	  return;` |
|      - | 7318 | `  }` |
|     84 | 7319 | `  for(i=0; i<sizeof(azDirs)/sizeof(azDirs[0]); i++){` |
|     84 | 7320 | `	zDir=azDirs[i];` |
|     84 | 7321 | `    if( zDir==0 ) continue;` |
|     84 | 7322 | `    if( stat(zDir, &buf) ) continue;` |
|     84 | 7323 | `    if( !S_ISDIR(buf.st_mode) ) continue;` |
|     84 | 7324 | `    if( access(zDir, 07) ) continue;` |
|      - | 7325 | `    /* Got one */` |
|     84 | 7326 | `	ph7_result_string(pCtx,zDir,-1);` |
|     84 | 7327 | `	return;` |
|      - | 7328 | `  }` |
|      - | 7329 | `  /* Default temp dir */` |
|    ! 0 | 7330 | `  ph7_result_string(pCtx,"/tmp",(int)sizeof("/tmp")-1);` |
|     84 | 7331 |  |
|      - | 7332 | `/* unsigned int (*xProcessId)(void) */` |
|      4 | 7333 | `static unsigned int UnixVfs_ProcessId(void)` |
|      - | 7334 |  |
|      4 | 7335 | `	return (unsigned int)getpid();` |
|      - | 7336 |  |
|      - | 7337 | `/* int (*xUid)(void) */` |
|      2 | 7338 | `static int UnixVfs_uid(void)` |
|      - | 7339 |  |
|      2 | 7340 | `	return (int)getuid();` |
|      - | 7341 |  |
|      - | 7342 | `/* int (*xGid)(void) */` |
|      2 | 7343 | `static int UnixVfs_gid(void)` |
|      - | 7344 |  |
|      2 | 7345 | `	return (int)getgid();` |
|      - | 7346 |  |
|      - | 7347 | `/* int (*xUmask)(int) */` |
|      8 | 7348 | `static int UnixVfs_Umask(int new_mask)` |
|      - | 7349 |  |
|      - | 7350 | `	int old_mask;` |
|      8 | 7351 | `	old_mask = umask(new_mask);` |
|      8 | 7352 | `	return old_mask;` |
|      - | 7353 |  |
|      - | 7354 | `/* void (*xUsername)(ph7_context *) */` |
|      2 | 7355 | `static void UnixVfs_Username(ph7_context *pCtx)` |
|      - | 7356 |  |
|      - | 7357 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|      - | 7358 | `  struct passwd *pwd;` |
|      - | 7359 | `  uid_t uid;` |
|      2 | 7360 | `  uid = getuid();` |
|      2 | 7361 | `  pwd = getpwuid(uid);   /* Try getting UID for username */` |
|      2 | 7362 | `  if (pwd == 0) {` |
|    ! 0 | 7363 | `    return;` |
|      - | 7364 | `  }` |
|      - | 7365 | `  /* Return the username */` |
|      2 | 7366 | `  ph7_result_string(pCtx,pwd->pw_name,-1);` |
|      - | 7367 | `#else` |
|      - | 7368 | `  ph7_result_string(pCtx,"Unknown",-1);` |
|      - | 7369 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|      2 | 7370 | `  return;` |
|      1 | 7371 |  |
|      - | 7372 | `/* int (*xLink)(const char *,const char *,int) */` |
|     10 | 7373 | `static int UnixVfs_link(const char *zSrc,const char *zTarget,int is_sym)` |
|      - | 7374 |  |
|      - | 7375 | `	int rc;` |
|     10 | 7376 | `	if( is_sym ){` |
|      - | 7377 | `		/* Symbolic link */` |
|      8 | 7378 | `		rc = symlink(zSrc,zTarget);` |
|      4 | 7379 | `	}else{` |
|      - | 7380 | `		/* Hard link */` |
|      2 | 7381 | `		rc = link(zSrc,zTarget);` |
|      - | 7382 | `	}` |
|     10 | 7383 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 7384 |  |
|      - | 7385 | `/* int (*xChroot)(const char *) */` |
|      2 | 7386 | `static int UnixVfs_chroot(const char *zRootDir)` |
|      - | 7387 |  |
|      - | 7388 | `	int rc;` |
|      2 | 7389 | `	rc = chroot(zRootDir);` |
|      2 | 7390 | `	return rc == 0 ? PH7_OK : -1;` |
|      - | 7391 |  |
|      - | 7392 | `/* Export the UNIX vfs */` |
|      - | 7393 | `static const ph7_vfs sUnixVfs = {` |
|      - | 7394 | `	"Unix_vfs",` |
|      - | 7395 | `	PH7_VFS_VERSION,` |
|      - | 7396 | `	UnixVfs_chdir,    /* int (*xChdir)(const char *) */` |
|      - | 7397 | `	UnixVfs_chroot,   /* int (*xChroot)(const char *); */` |
|      - | 7398 | `	UnixVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|      - | 7399 | `	UnixVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|      - | 7400 | `	UnixVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|      - | 7401 | `	UnixVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|      - | 7402 | `	UnixVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|      - | 7403 | `	UnixVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 7404 | `	UnixVfs_Sleep,    /* int (*xSleep)(unsigned int) */` |
|      - | 7405 | `	UnixVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|      - | 7406 | `	UnixVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|      - | 7407 | `	UnixVfs_Chmod, /*int (*xChmod)(const char *,int)*/` |
|      - | 7408 | `	UnixVfs_Chown, /*int (*xChown)(const char *,const char *)*/` |
|      - | 7409 | `	UnixVfs_Chgrp, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 7410 | `	0,             /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 7411 | `	0,             /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 7412 | `	UnixVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 7413 | `	UnixVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 7414 | `	UnixVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 7415 | `	UnixVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 7416 | `	UnixVfs_Stat,  /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 7417 | `	UnixVfs_lStat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 7418 | `	UnixVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|      - | 7419 | `	UnixVfs_islink,     /* int (*xIslink)(const char *) */` |
|      - | 7420 | `	UnixVfs_isreadable, /* int (*xReadable)(const char *) */` |
|      - | 7421 | `	UnixVfs_iswritable, /* int (*xWritable)(const char *) */` |
|      - | 7422 | `	UnixVfs_isexecutable,/* int (*xExecutable)(const char *) */` |
|      - | 7423 | `	UnixVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 7424 | `	UnixVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 7425 | `	UnixVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|      - | 7426 | `	UnixVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 7427 | `	UnixVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 7428 | `	UnixVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 7429 | `	UnixVfs_link,       /* int (*xLink)(const char *,const char *,int) */` |
|      - | 7430 | `	UnixVfs_Umask,      /* int (*xUmask)(int) */` |
|      - | 7431 | `	UnixVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|      - | 7432 | `	UnixVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|      - | 7433 | `	UnixVfs_uid, /* int (*xUid)(void) */` |
|      - | 7434 | `	UnixVfs_gid, /* int (*xGid)(void) */` |
|      - | 7435 | `	UnixVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|      - | 7436 | `	0 /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 7437 | `};` |
|      - | 7438 | `/* UNIX File IO */` |
|      - | 7439 | `#define PH7_UNIX_OPEN_MODE	0640 /* Default open mode */` |
|      - | 7440 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|    282 | 7441 | `static int UnixFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|      - | 7442 |  |
|    282 | 7443 | `	int iOpen = O_RDONLY;` |
|      - | 7444 | `	int fd;` |
|      - | 7445 | `	/* Set the desired flags according to the open mode */` |
|    282 | 7446 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|      - | 7447 | `		/* Open existing file, or create if it doesn't exist */` |
|    166 | 7448 | `		iOpen = O_CREAT;` |
|    166 | 7449 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|      - | 7450 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
|    166 | 7451 | `			iOpen \|= O_TRUNC;` |
|     83 | 7452 | `			SXUNUSED(pResource); /* cc warning */` |
|     83 | 7453 | `		}` |
|    199 | 7454 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|      - | 7455 | `		/* Creates a new file, only if it does not already exist.` |
|      - | 7456 | `		* If the file exists, it fails.` |
|      - | 7457 | `		*/` |
|    ! 0 | 7458 | `		iOpen = O_CREAT\|O_EXCL;` |
|    116 | 7459 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|      - | 7460 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|      - | 7461 | `		 * The file must exist.` |
|      - | 7462 | `		 */` |
|    ! 0 | 7463 | `		iOpen = O_RDWR\|O_TRUNC;` |
|    ! 0 | 7464 | `	}` |
|    282 | 7465 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|      - | 7466 | `		/* Read+Write access */` |
|    150 | 7467 | `		iOpen &= ~O_RDONLY;` |
|    150 | 7468 | `		iOpen \|= O_RDWR;` |
|    207 | 7469 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|      - | 7470 | `		/* Write only access */` |
|     22 | 7471 | `		iOpen &= ~O_RDONLY;` |
|     22 | 7472 | `		iOpen \|= O_WRONLY;` |
|     11 | 7473 | `	}` |
|    282 | 7474 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|      - | 7475 | `		/* Append mode */` |
|    ! 0 | 7476 | `		iOpen \|= O_APPEND;` |
|    ! 0 | 7477 | `	}` |
|      - | 7478 | `#ifdef O_TEMP` |
|      - | 7479 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|      - | 7480 | `		/* File is temporary */` |
|      - | 7481 | `		iOpen \|= O_TEMP;` |
|      - | 7482 | `	}` |
|      - | 7483 | `#endif` |
|      - | 7484 | `	/* Open the file now */` |
|    282 | 7485 | `	fd = open(zPath,iOpen,PH7_UNIX_OPEN_MODE);` |
|    282 | 7486 | `	if( fd < 0 ){` |
|      - | 7487 | `		/* IO error */` |
|      8 | 7488 | `		return -1;` |
|      - | 7489 | `	}` |
|      - | 7490 | `	/* Save the handle */` |
|    274 | 7491 | `	*ppHandle = SX_INT_TO_PTR(fd);` |
|    274 | 7492 | `	return PH7_OK;` |
|    141 | 7493 |  |
|      - | 7494 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|      4 | 7495 | `static int UnixDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|      - | 7496 |  |
|      - | 7497 | `	DIR *pDir;` |
|      - | 7498 | `	/* Open the target directory */` |
|      4 | 7499 | `	pDir = opendir(zPath);` |
|      4 | 7500 | `	if( pDir == 0 ){` |
|    ! 0 | 7501 | `		SXUNUSED(pResource); /* Compiler warning */` |
|    ! 0 | 7502 | `		return -1;` |
|      - | 7503 | `	}` |
|      - | 7504 | `	/* Save our structure */` |
|      4 | 7505 | `	*ppHandle = pDir;` |
|      4 | 7506 | `	return PH7_OK;` |
|      2 | 7507 |  |
|      - | 7508 | `/* void (*xCloseDir)(void *) */` |
|      4 | 7509 | `static void UnixDir_Close(void *pUserData)` |
|      - | 7510 |  |
|      4 | 7511 | `	closedir((DIR *)pUserData);` |
|      4 | 7512 |  |
|      - | 7513 | `/* void (*xClose)(void *); */` |
|    272 | 7514 | `static void UnixFile_Close(void *pUserData)` |
|      - | 7515 |  |
|    272 | 7516 | `	close(SX_PTR_TO_INT(pUserData));` |
|    272 | 7517 |  |
|      - | 7518 | `/* int (*xReadDir)(void *,ph7_context *) */` |
|     10 | 7519 | `static int UnixDir_Read(void *pUserData,ph7_context *pCtx)` |
|      - | 7520 |  |
|     10 | 7521 | `	DIR *pDir = (DIR *)pUserData;` |
|      - | 7522 | `	struct dirent *pEntry;` |
|     10 | 7523 | `	char *zName = 0; /* cc warning */` |
|     10 | 7524 | `	sxu32 n = 0;` |
|     11 | 7525 | `	for(;;){` |
|     18 | 7526 | `		pEntry = readdir(pDir);` |
|     18 | 7527 | `		if( pEntry == 0 ){` |
|      - | 7528 | `			/* No more entries to process */` |
|      2 | 7529 | `			return -1;` |
|      - | 7530 | `		}` |
|     16 | 7531 | `		zName = pEntry->d_name;` |
|     16 | 7532 | `		n = SyStrlen(zName);` |
|      - | 7533 | `		/* Ignore '.' && '..' */` |
|     16 | 7534 | `		if( n > sizeof("..")-1 \|\| zName[0] != '.' \|\| ( n == sizeof("..")-1 && zName[1] != '.') ){` |
|      4 | 7535 | `			break;` |
|      - | 7536 | `		}` |
|      - | 7537 | `		/* Next entry */` |
|      - | 7538 | `	}` |
|      - | 7539 | `	/* Return the current file name */` |
|      8 | 7540 | `	ph7_result_string(pCtx,zName,(int)n);` |
|      8 | 7541 | `	return PH7_OK;` |
|      5 | 7542 |  |
|      - | 7543 | `/* void (*xRewindDir)(void *) */` |
|      2 | 7544 | `static void UnixDir_Rewind(void *pUserData)` |
|      - | 7545 |  |
|      2 | 7546 | `	rewinddir((DIR *)pUserData);` |
|      2 | 7547 |  |
|      - | 7548 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
|    170 | 7549 | `static ph7_int64 UnixFile_Read(void *pUserData,void *pBuffer,ph7_int64 nDatatoRead)` |
|      - | 7550 |  |
|      - | 7551 | `	ssize_t nRd;` |
|    170 | 7552 | `	nRd = read(SX_PTR_TO_INT(pUserData),pBuffer,(size_t)nDatatoRead);` |
|    170 | 7553 | `	if( nRd < 1 ){` |
|      - | 7554 | `		/* EOF or IO error */` |
|     80 | 7555 | `		return -1;` |
|      - | 7556 | `	}` |
|     90 | 7557 | `	return (ph7_int64)nRd;` |
|     85 | 7558 |  |
|      - | 7559 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
|    188 | 7560 | `static ph7_int64 UnixFile_Write(void *pUserData,const void *pBuffer,ph7_int64 nWrite)` |
|      - | 7561 |  |
|    188 | 7562 | `	const char *zData = (const char *)pBuffer;` |
|    188 | 7563 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|      - | 7564 | `	ph7_int64 nCount;` |
|      - | 7565 | `	ssize_t nWr;` |
|    188 | 7566 | `	nCount = 0;` |
|    188 | 7567 | `	for(;;){` |
|    376 | 7568 | `		if( nWrite < 1 ){` |
|    188 | 7569 | `			break;` |
|      - | 7570 | `		}` |
|    188 | 7571 | `		nWr = write(fd,zData,(size_t)nWrite);` |
|    188 | 7572 | `		if( nWr < 1 ){` |
|      - | 7573 | `			/* IO error */` |
|    ! 0 | 7574 | `			break;` |
|      - | 7575 | `		}` |
|    188 | 7576 | `		nWrite -= nWr;` |
|    188 | 7577 | `		nCount += nWr;` |
|    188 | 7578 | `		zData += nWr;` |
|      - | 7579 | `	}` |
|    188 | 7580 | `	if( nWrite > 0 ){` |
|    ! 0 | 7581 | `		return -1;` |
|      - | 7582 | `	}` |
|    188 | 7583 | `	return nCount;` |
|     94 | 7584 |  |
|      - | 7585 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|      6 | 7586 | `static int UnixFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|      - | 7587 |  |
|      - | 7588 | `	off_t iNew;` |
|      6 | 7589 | `	switch(whence){` |
|    ! 0 | 7590 | `	case 1:/*SEEK_CUR*/` |
|    ! 0 | 7591 | `		whence = SEEK_CUR;` |
|    ! 0 | 7592 | `		break;` |
|    ! 0 | 7593 | `	case 2: /* SEEK_END */` |
|    ! 0 | 7594 | `		whence = SEEK_END;` |
|    ! 0 | 7595 | `		break;` |
|      6 | 7596 | `	case 0: /* SEEK_SET */` |
|      - | 7597 | `	default:` |
|      6 | 7598 | `		whence = SEEK_SET;` |
|      6 | 7599 | `		break;` |
|      - | 7600 | `	}` |
|      6 | 7601 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),(off_t)iOfft,whence);` |
|      6 | 7602 | `	if( iNew < 0 ){` |
|    ! 0 | 7603 | `		return -1;` |
|      - | 7604 | `	}` |
|      6 | 7605 | `	return PH7_OK;` |
|      3 | 7606 |  |
|      - | 7607 | `/* int (*xLock)(void *,int) */` |
|      4 | 7608 | `static int UnixFile_Lock(void *pUserData,int lock_type)` |
|      - | 7609 |  |
|      4 | 7610 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|      4 | 7611 | `	int rc = PH7_OK; /* cc warning */` |
|      4 | 7612 | `	if( lock_type < 0 ){` |
|      - | 7613 | `		/* Unlock the file */` |
|    ! 0 | 7614 | `		rc = flock(fd,LOCK_UN);` |
|    ! 0 | 7615 | `	}else{` |
|      4 | 7616 | `		if( lock_type == 1 ){` |
|      - | 7617 | `			/* Exculsive lock */` |
|      2 | 7618 | `			rc = flock(fd,LOCK_EX);` |
|      1 | 7619 | `		}else{` |
|      - | 7620 | `			/* Shared lock */` |
|      2 | 7621 | `			rc = flock(fd,LOCK_SH);` |
|      - | 7622 | `		}` |
|      - | 7623 | `	}` |
|      4 | 7624 | `	return !rc ? PH7_OK : -1;` |
|      - | 7625 |  |
|      - | 7626 | `/* ph7_int64 (*xTell)(void *) */` |
|      6 | 7627 | `static ph7_int64 UnixFile_Tell(void *pUserData)` |
|      - | 7628 |  |
|      - | 7629 | `	off_t iNew;` |
|      6 | 7630 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),0,SEEK_CUR);` |
|      6 | 7631 | `	return (ph7_int64)iNew;` |
|      - | 7632 |  |
|      - | 7633 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|      6 | 7634 | `static int UnixFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|      - | 7635 |  |
|      - | 7636 | `	int rc;` |
|      6 | 7637 | `	rc = ftruncate(SX_PTR_TO_INT(pUserData),(off_t)nOfft);` |
|      6 | 7638 | `	if( rc != 0 ){` |
|    ! 0 | 7639 | `		return -1;` |
|      - | 7640 | `	}` |
|      6 | 7641 | `	return PH7_OK;` |
|      3 | 7642 |  |
|      - | 7643 | `/* int (*xSync)(void *); */` |
|      2 | 7644 | `static int UnixFile_Sync(void *pUserData)` |
|      - | 7645 |  |
|      - | 7646 | `	int rc;` |
|      2 | 7647 | `	rc = fsync(SX_PTR_TO_INT(pUserData));` |
|      2 | 7648 | `	return rc == 0 ? PH7_OK : - 1;` |
|      - | 7649 |  |
|      - | 7650 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|      2 | 7651 | `static int UnixFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|      - | 7652 |  |
|      - | 7653 | `	struct stat st;` |
|      - | 7654 | `	int rc;` |
|      2 | 7655 | `	rc = fstat(SX_PTR_TO_INT(pUserData),&st);` |
|      2 | 7656 | `	if( rc != 0 ){` |
|    ! 0 | 7657 | `	 return -1;` |
|      - | 7658 | `	}` |
|      - | 7659 | `	/* dev */` |
|      2 | 7660 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|      2 | 7661 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|      - | 7662 | `	/* ino */` |
|      2 | 7663 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|      2 | 7664 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|      - | 7665 | `	/* mode */` |
|      2 | 7666 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|      2 | 7667 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|      - | 7668 | `	/* nlink */` |
|      2 | 7669 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|      2 | 7670 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|      - | 7671 | `	/* uid,gid,rdev */` |
|      2 | 7672 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|      2 | 7673 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|      2 | 7674 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|      2 | 7675 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|      2 | 7676 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|      2 | 7677 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|      - | 7678 | `	/* size */` |
|      2 | 7679 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|      2 | 7680 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|      - | 7681 | `	/* atime */` |
|      2 | 7682 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|      2 | 7683 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|      - | 7684 | `	/* mtime */` |
|      2 | 7685 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|      2 | 7686 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|      - | 7687 | `	/* ctime */` |
|      2 | 7688 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|      2 | 7689 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|      - | 7690 | `	/* blksize,blocks */` |
|      2 | 7691 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|      2 | 7692 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|      2 | 7693 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|      2 | 7694 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|      2 | 7695 | `	return PH7_OK;` |
|      1 | 7696 |  |
|      - | 7697 | `/* Export the file:// stream */` |
|      - | 7698 | `static const ph7_io_stream sUnixFileStream = {` |
|      - | 7699 | `	"file", /* Stream name */` |
|      - | 7700 | `	PH7_IO_STREAM_VERSION,` |
|      - | 7701 | `	UnixFile_Open,  /* xOpen */` |
|      - | 7702 | `	UnixDir_Open,   /* xOpenDir */` |
|      - | 7703 | `	UnixFile_Close, /* xClose */` |
|      - | 7704 | `	UnixDir_Close,  /* xCloseDir */` |
|      - | 7705 | `	UnixFile_Read,  /* xRead */` |
|      - | 7706 | `	UnixDir_Read,   /* xReadDir */` |
|      - | 7707 | `	UnixFile_Write, /* xWrite */` |
|      - | 7708 | `	UnixFile_Seek,  /* xSeek */` |
|      - | 7709 | `	UnixFile_Lock,  /* xLock */` |
|      - | 7710 | `	UnixDir_Rewind, /* xRewindDir */` |
|      - | 7711 | `	UnixFile_Tell,  /* xTell */` |
|      - | 7712 | `	UnixFile_Trunc, /* xTrunc */` |
|      - | 7713 | `	UnixFile_Sync,  /* xSeek */` |
|      - | 7714 | `	UnixFile_Stat   /* xStat */` |
|      - | 7715 | `};` |
|      - | 7716 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 7717 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7718 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 7719 | `/*` |
|      - | 7720 | ` * Export the builtin vfs.` |
|      - | 7721 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 7722 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 7723 | ` * Note:` |
|      - | 7724 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 7725 | ` * Note:` |
|      - | 7726 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7727 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 7728 | ` */` |
|   5364 | 7729 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      1 | 7730 |  |
|      - | 7731 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7732 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 7733 | `	return &null_vfs;` |
|      - | 7734 | `#else` |
|      - | 7735 | `#ifdef __WINNT__` |
|      1 | 7736 | `	return &sWinVfs;` |
|      - | 7737 | `#elif defined(__UNIXES__)` |
|   5364 | 7738 | `	return &sUnixVfs;` |
|      - | 7739 | `#else` |
|      - | 7740 | `	return &null_vfs;` |
|      - | 7741 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 7742 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 7743 | `#else` |
|      - | 7744 | `	return &null_vfs;` |
|      - | 7745 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      1 | 7746 |  |
|      - | 7747 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 7748 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 7749 | `/*` |
|      - | 7750 | ` * The following defines are mostly used by the UNIX built and have` |
|      - | 7751 | ` * no particular meaning on windows.` |
|      - | 7752 | ` */` |
|      - | 7753 | `#ifndef STDIN_FILENO` |
|      - | 7754 | `#define STDIN_FILENO	0` |
|      - | 7755 | `#endif` |
|      - | 7756 | `#ifndef STDOUT_FILENO` |
|      - | 7757 | `#define STDOUT_FILENO	1` |
|      - | 7758 | `#endif` |
|      - | 7759 | `#ifndef STDERR_FILENO` |
|      - | 7760 | `#define STDERR_FILENO	2` |
|      - | 7761 | `#endif` |
|      - | 7762 | `/*` |
|      - | 7763 | ` * php:// Accessing various I/O streams` |
|      - | 7764 | ` * According to the PHP langage reference manual` |
|      - | 7765 | ` * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input` |
|      - | 7766 | ` * and output streams, the standard input, output and error file descriptors.` |
|      - | 7767 | ` * php://stdin, php://stdout and php://stderr:` |
|      - | 7768 | ` *  Allow direct access to the corresponding input or output stream of the PHP process.` |
|      - | 7769 | ` *  The stream references a duplicate file descriptor, so if you open php://stdin and later` |
|      - | 7770 | ` *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.` |
|      - | 7771 | ` *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.` |
|      - | 7772 | ` * php://output` |
|      - | 7773 | ` *  php://output is a write-only stream that allows you to write to the output buffer` |
|      - | 7774 | ` *  mechanism in the same way as print and echo.` |
|      - | 7775 | ` */` |
|      - | 7776 | `typedef struct ph7_stream_data ph7_stream_data;` |
|      - | 7777 | `/* Supported IO streams */` |
|      - | 7778 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|      - | 7779 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|      - | 7780 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|      - | 7781 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|      - | 7782 | ` /* The following structure is the private data associated with the php:// stream */` |
|      - | 7783 | `struct ph7_stream_data` |
|      - | 7784 |  |
|      - | 7785 | `	ph7_vm *pVm; /* VM that own this instance */` |
|      - | 7786 | `	int iType;   /* Stream type */` |
|      - | 7787 | `	union{` |
|      - | 7788 | `		void *pHandle; /* Stream handle */` |
|      - | 7789 | `		ph7_output_consumer sConsumer; /* VM output consumer */` |
|      - | 7790 | `	}x;` |
|      - | 7791 | `};` |
|      - | 7792 | `/*` |
|      - | 7793 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|      - | 7794 | ` */` |
|      8 | 7795 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|      1 | 7796 |  |
|      - | 7797 | `	ph7_stream_data *pData;` |
|      9 | 7798 | `	if( pVm == 0 ){` |
|    ! 0 | 7799 | `		return 0;` |
|      - | 7800 | `	}` |
|      - | 7801 | `	/* Allocate a new instance */` |
|      9 | 7802 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|      9 | 7803 | `	if( pData == 0 ){` |
|    ! 0 | 7804 | `		return 0;` |
|      - | 7805 | `	}` |
|      - | 7806 | `	/* Zero the structure */` |
|      9 | 7807 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|      - | 7808 | `	/* Initialize fields */` |
|      9 | 7809 | `	pData->iType = iType;` |
|      9 | 7810 | `	if( iType == PH7_IO_STREAM_OUTPUT ){` |
|      - | 7811 | `		/* Point to the default VM consumer routine. */` |
|      3 | 7812 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|      2 | 7813 | `	}else{` |
|      - | 7814 | `#ifdef __WINNT__` |
|      - | 7815 | `		DWORD nChannel;` |
|      1 | 7816 | `		switch(iType){` |
|      1 | 7817 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|      1 | 7818 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|      - | 7819 | `		default:` |
|      1 | 7820 | `			nChannel = STD_INPUT_HANDLE;` |
|      - | 7821 | `			break;` |
|      - | 7822 | `		}` |
|      1 | 7823 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|      - | 7824 | `#else` |
|      - | 7825 | `		/* Assume an UNIX system */` |
|      6 | 7826 | `		int ifd = STDIN_FILENO;` |
|      6 | 7827 | `		switch(iType){` |
|      2 | 7828 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|      2 | 7829 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|      1 | 7830 | `		default:` |
|      2 | 7831 | `			break;` |
|      - | 7832 | `		}` |
|      6 | 7833 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|      - | 7834 | `#endif` |
|      - | 7835 | `	}` |
|      9 | 7836 | `	pData->pVm = pVm;` |
|      9 | 7837 | `	return pData;` |
|      5 | 7838 |  |
|      - | 7839 | `/*` |
|      - | 7840 | ` * Implementation of the php:// IO streams routines` |
|      - | 7841 | ` * Status:` |
|      - | 7842 | ` *   Stable.` |
|      - | 7843 | ` */` |
|      - | 7844 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|      2 | 7845 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 7846 |  |
|      - | 7847 | `	ph7_stream_data *pData;` |
|      - | 7848 | `	SyString sStream;` |
|      3 | 7849 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|      - | 7850 | `	/* Trim leading and trailing white spaces */` |
|      3 | 7851 | `	SyStringFullTrim(&sStream);` |
|      - | 7852 | `	/* Stream to open */` |
|      3 | 7853 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|    ! 0 | 7854 | `		iMode = PH7_IO_STREAM_STDIN;` |
|      3 | 7855 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|      3 | 7856 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|      1 | 7857 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|    ! 0 | 7858 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|    ! 0 | 7859 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|    ! 0 | 7860 | `		iMode = PH7_IO_STREAM_STDERR;` |
|    ! 0 | 7861 | `	}else{` |
|      - | 7862 | `		/* unknown stream name */` |
|    ! 0 | 7863 | `		return -1;` |
|      - | 7864 | `	}` |
|      - | 7865 | `	/* Create our handle */` |
|      3 | 7866 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|      3 | 7867 | `	if( pData == 0 ){` |
|    ! 0 | 7868 | `		return -1;` |
|      - | 7869 | `	}` |
|      - | 7870 | `	/* Make the handle public */` |
|      3 | 7871 | `	*ppHandle = (void *)pData;` |
|      3 | 7872 | `	return PH7_OK;` |
|      2 | 7873 |  |
|      - | 7874 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|    ! 0 | 7875 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|    ! 0 | 7876 |  |
|    ! 0 | 7877 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    ! 0 | 7878 | `	if( pData == 0 ){` |
|    ! 0 | 7879 | `		return -1;` |
|      - | 7880 | `	}` |
|    ! 0 | 7881 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|      - | 7882 | `		/* Forbidden */` |
|    ! 0 | 7883 | `		return -1;` |
|      - | 7884 | `	}` |
|      - | 7885 | `#ifdef __WINNT__` |
|      - | 7886 | `	{` |
|      - | 7887 | `		DWORD nRd;` |
|      - | 7888 | `		BOOL rc;` |
|    ! 0 | 7889 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    ! 0 | 7890 | `		if( !rc ){` |
|      - | 7891 | `			/* IO error */` |
|    ! 0 | 7892 | `			return -1;` |
|      - | 7893 | `		}` |
|    ! 0 | 7894 | `		return (ph7_int64)nRd;` |
|      - | 7895 | `	}` |
|      - | 7896 | `#elif defined(__UNIXES__)` |
|      - | 7897 | `	{` |
|      - | 7898 | `		ssize_t nRd;` |
|      - | 7899 | `		int fd;` |
|    ! 0 | 7900 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 7901 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|    ! 0 | 7902 | `		if( nRd < 1 ){` |
|    ! 0 | 7903 | `			return -1;` |
|      - | 7904 | `		}` |
|    ! 0 | 7905 | `		return (ph7_int64)nRd;` |
|      - | 7906 | `	}` |
|      - | 7907 | `#else` |
|      - | 7908 | `	return -1;` |
|      - | 7909 | `#endif` |
|    ! 0 | 7910 |  |
|      - | 7911 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|      2 | 7912 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 7913 |  |
|      3 | 7914 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      3 | 7915 | `	if( pData == 0 ){` |
|    ! 0 | 7916 | `		return -1;` |
|      - | 7917 | `	}` |
|      3 | 7918 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|      - | 7919 | `		/* Forbidden */` |
|    ! 0 | 7920 | `		return -1;` |
|      3 | 7921 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|      3 | 7922 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|      - | 7923 | `		int rc;` |
|      - | 7924 | `		/* Call the vm output consumer */` |
|      3 | 7925 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|      3 | 7926 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 7927 | `			return -1;` |
|      - | 7928 | `		}` |
|      3 | 7929 | `		return nWrite;` |
|      - | 7930 | `	}` |
|      - | 7931 | `#ifdef __WINNT__` |
|      - | 7932 | `	{` |
|      - | 7933 | `		DWORD nWr;` |
|      - | 7934 | `		BOOL rc;` |
|    ! 0 | 7935 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|    ! 0 | 7936 | `		if( !rc ){` |
|      - | 7937 | `			/* IO error */` |
|    ! 0 | 7938 | `			return -1;` |
|      - | 7939 | `		}` |
|    ! 0 | 7940 | `		return (ph7_int64)nWr;` |
|      - | 7941 | `	}` |
|      - | 7942 | `#elif defined(__UNIXES__)` |
|      - | 7943 | `	{` |
|      - | 7944 | `		ssize_t nWr;` |
|      - | 7945 | `		int fd;` |
|    ! 0 | 7946 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 7947 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|    ! 0 | 7948 | `		if( nWr < 1 ){` |
|    ! 0 | 7949 | `			return -1;` |
|      - | 7950 | `		}` |
|    ! 0 | 7951 | `		return (ph7_int64)nWr;` |
|      - | 7952 | `	}` |
|      - | 7953 | `#else` |
|      - | 7954 | `	return -1;` |
|      - | 7955 | `#endif` |
|      2 | 7956 |  |
|      - | 7957 | `/* void (*xClose)(void *) */` |
|      2 | 7958 | `static void PHPStreamData_Close(void *pHandle)` |
|      1 | 7959 |  |
|      3 | 7960 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 7961 | `	ph7_vm *pVm;` |
|      3 | 7962 | `	if( pData == 0 ){` |
|    ! 0 | 7963 | `		return;` |
|      - | 7964 | `	}` |
|      3 | 7965 | `	pVm = pData->pVm;` |
|      - | 7966 | `	/* Free the instance */` |
|      3 | 7967 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|      2 | 7968 |  |
|      - | 7969 | `/*` |
|      - | 7970 | ` * Pipe stream implementation for popen/pclose.` |
|      - | 7971 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|      - | 7972 | ` * PHP-compatible process I/O functionality.` |
|      - | 7973 | ` */` |
|      - | 7974 | `typedef struct pipe_private pipe_private;` |
|      - | 7975 | `struct pipe_private` |
|      - | 7976 |  |
|      - | 7977 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|      - | 7978 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|      - | 7979 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|      - | 7980 | `#ifdef __WINNT__` |
|      - | 7981 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|      - | 7982 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|      - | 7983 | `#endif` |
|      - | 7984 | `};` |
|      - | 7985 |  |
|      - | 7986 | `#ifdef __WINNT__` |
|      - | 7987 | `/*` |
|      - | 7988 | ` * Custom Windows popen implementation using CreateProcess.` |
|      - | 7989 | ` * This allows us to properly wait for process completion.` |
|      - | 7990 | ` */` |
|      - | 7991 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|      1 | 7992 |  |
|      1 | 7993 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|      1 | 7994 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|      1 | 7995 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|      - | 7996 | `	SECURITY_ATTRIBUTES sa;` |
|      - | 7997 | `	STARTUPINFOW si;` |
|      - | 7998 | `	PROCESS_INFORMATION pi;` |
|      1 | 7999 | `	WCHAR *zWideCmd = NULL;` |
|      1 | 8000 | `	FILE *pFile = NULL;` |
|      - | 8001 | `	int fd;` |
|      1 | 8002 | `	BOOL bRead = (zMode[0] == 'r');` |
|      - | 8003 |  |
|      - | 8004 | `	/* Set up security attributes for pipe inheritance */` |
|      1 | 8005 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|      1 | 8006 | `	sa.bInheritHandle = TRUE;` |
|      1 | 8007 | `	sa.lpSecurityDescriptor = NULL;` |
|      - | 8008 |  |
|      - | 8009 | `	/* Create pipes for child process I/O */` |
|      1 | 8010 | `	if( bRead ){` |
|      - | 8011 | `		/* Reading from child's stdout */` |
|      1 | 8012 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|    ! 0 | 8013 | `			return NULL;` |
|      - | 8014 | `		}` |
|      - | 8015 | `		/* Ensure read handle is not inherited */` |
|      1 | 8016 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|      1 | 8017 | `		hReadPipe = hChildStdoutRd;` |
|      1 | 8018 | `		*phPipe = hChildStdoutRd;` |
|      1 | 8019 | `	}else{` |
|      - | 8020 | `		/* Writing to child's stdin */` |
|    ! 0 | 8021 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|    ! 0 | 8022 | `			return NULL;` |
|      - | 8023 | `		}` |
|      - | 8024 | `		/* Ensure write handle is not inherited */` |
|    ! 0 | 8025 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|    ! 0 | 8026 | `		hWritePipe = hChildStdinWr;` |
|    ! 0 | 8027 | `		*phPipe = hChildStdinWr;` |
|      - | 8028 | `	}` |
|      - | 8029 |  |
|      - | 8030 | `	/* Convert command to wide string */` |
|      - | 8031 | `	{` |
|      1 | 8032 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|      1 | 8033 | `		if( nLen <= 0 ){` |
|    ! 0 | 8034 | `			goto cleanup_pipes;` |
|      - | 8035 | `		}` |
|      1 | 8036 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|      1 | 8037 | `		if( !zWideCmd ){` |
|    ! 0 | 8038 | `			goto cleanup_pipes;` |
|      - | 8039 | `		}` |
|      1 | 8040 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|      - | 8041 | `	}` |
|      - | 8042 |  |
|      - | 8043 | `	/* Set up process startup info */` |
|      1 | 8044 | `	ZeroMemory(&si, sizeof(si));` |
|      1 | 8045 | `	si.cb = sizeof(si);` |
|      1 | 8046 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|      1 | 8047 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|      1 | 8048 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|      1 | 8049 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|      1 | 8050 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|      - | 8051 |  |
|      1 | 8052 | `	ZeroMemory(&pi, sizeof(pi));` |
|      - | 8053 |  |
|      - | 8054 | `	/* Create the child process */` |
|      1 | 8055 | `	if( !CreateProcessW(` |
|      - | 8056 | `		NULL,           /* Application name */` |
|      - | 8057 | `		zWideCmd,       /* Command line */` |
|      - | 8058 | `		NULL,           /* Process security attributes */` |
|      - | 8059 | `		NULL,           /* Thread security attributes */` |
|      - | 8060 | `		TRUE,           /* Inherit handles */` |
|      - | 8061 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|      - | 8062 | `		NULL,           /* Environment */` |
|      - | 8063 | `		NULL,           /* Current directory */` |
|      - | 8064 | `		&si,            /* Startup info */` |
|      - | 8065 | `		&pi             /* Process info */` |
|      - | 8066 | `	)){` |
|    ! 0 | 8067 | `		goto cleanup_all;` |
|      - | 8068 | `	}` |
|      - | 8069 |  |
|      - | 8070 | `	/* Close handles we don't need in parent */` |
|      1 | 8071 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|      1 | 8072 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|      - | 8073 |  |
|      - | 8074 | `	/* Close thread handle (we only need process handle) */` |
|      1 | 8075 | `	CloseHandle(pi.hThread);` |
|      - | 8076 |  |
|      - | 8077 | `	/* Store process handle for later waiting */` |
|      1 | 8078 | `	*phProcess = pi.hProcess;` |
|      - | 8079 |  |
|      - | 8080 | `	/* Convert OS handle to C file descriptor, then to FILE* */` |
|      1 | 8081 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|      - | 8082 | `	                     bRead ? _O_RDONLY \| _O_TEXT : _O_WRONLY \| _O_TEXT);` |
|      1 | 8083 | `	if( fd == -1 ){` |
|    ! 0 | 8084 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 8085 | `		*phProcess = NULL;` |
|    ! 0 | 8086 | `		goto cleanup_all;` |
|      - | 8087 | `	}` |
|      - | 8088 |  |
|      1 | 8089 | `	pFile = _fdopen(fd, zMode);` |
|      1 | 8090 | `	if( !pFile ){` |
|    ! 0 | 8091 | `		_close(fd); /* This will also close the underlying handle */` |
|    ! 0 | 8092 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 8093 | `		*phProcess = NULL;` |
|    ! 0 | 8094 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    ! 0 | 8095 | `		return NULL;` |
|      - | 8096 | `	}` |
|      - | 8097 |  |
|      1 | 8098 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      1 | 8099 | `	return pFile;` |
|      - | 8100 |  |
|      - | 8101 | `cleanup_all:` |
|    ! 0 | 8102 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      - | 8103 | `cleanup_pipes:` |
|    ! 0 | 8104 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|    ! 0 | 8105 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|    ! 0 | 8106 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|    ! 0 | 8107 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|    ! 0 | 8108 | `	return NULL;` |
|      1 | 8109 |  |
|      - | 8110 |  |
|      - | 8111 | `/*` |
|      - | 8112 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|      - | 8113 | ` */` |
|      - | 8114 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|      1 | 8115 |  |
|      1 | 8116 | `	DWORD dwExitCode = 0;` |
|      - | 8117 | `	int status;` |
|      - | 8118 |  |
|      - | 8119 | `	/* Close the FILE* (this closes the pipe) */` |
|      1 | 8120 | `	fclose(pFile);` |
|      - | 8121 |  |
|      1 | 8122 | `	if( hProcess ){` |
|      - | 8123 | `		/* Wait for the process to complete */` |
|      1 | 8124 | `		WaitForSingleObject(hProcess, INFINITE);` |
|      - | 8125 |  |
|      1 | 8126 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|      1 | 8127 | `			status = (int)dwExitCode;` |
|      1 | 8128 | `		}else{` |
|    ! 0 | 8129 | `			status = -1;` |
|      - | 8130 | `		}` |
|      - | 8131 |  |
|      - | 8132 | `		/* Close process handle */` |
|      1 | 8133 | `		CloseHandle(hProcess);` |
|      1 | 8134 | `	}else{` |
|    ! 0 | 8135 | `		status = -1;` |
|      - | 8136 | `	}` |
|      - | 8137 |  |
|      1 | 8138 | `	return status;` |
|      1 | 8139 |  |
|      - | 8140 | `#endif /* __WINNT__ */` |
|      - | 8141 | `/*` |
|      - | 8142 | ` * Open a pipe to a process.` |
|      - | 8143 | ` * This is called internally by popen(), not through the stream device interface.` |
|      - | 8144 | ` */` |
|     28 | 8145 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|      1 | 8146 |  |
|      - | 8147 | `	pipe_private *pPipe;` |
|      - | 8148 | `	FILE *pFile;` |
|     29 | 8149 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|    ! 0 | 8150 | `		return 0;` |
|      - | 8151 | `	}` |
|      - | 8152 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|     29 | 8153 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|    ! 0 | 8154 | `		return 0;` |
|      - | 8155 | `	}` |
|      - | 8156 | `	/* Open the pipe using system popen */` |
|      - | 8157 | `#ifdef __WINNT__` |
|      - | 8158 | `	{` |
|      - | 8159 | `		/* Build cmd.exe command wrapper */` |
|      1 | 8160 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|      1 | 8161 | `		const char *zShellSuffix = "\"";` |
|      1 | 8162 | `		size_t nPrefix = strlen(zShellPrefix);` |
|      1 | 8163 | `		size_t nSuffix = strlen(zShellSuffix);` |
|      1 | 8164 | `		size_t nCmd = strlen(zCommand);` |
|      1 | 8165 | `		size_t nQuotes = 0;` |
|      1 | 8166 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      1 | 8167 | `			if (zCommand[i] == '"') nQuotes++;` |
|      1 | 8168 | `		}` |
|      1 | 8169 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|      1 | 8170 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|      1 | 8171 | `		if (zCmdEsc == NULL) {` |
|    ! 0 | 8172 | `			return 0;` |
|      - | 8173 | `		}` |
|      - | 8174 | `		/* Escape quotes in command */` |
|      1 | 8175 | `		size_t j = 0;` |
|      1 | 8176 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      1 | 8177 | `			char ch = zCommand[i];` |
|      1 | 8178 | `			if (ch == '"') {` |
|      1 | 8179 | `				zCmdEsc[j++] = '^';` |
|      1 | 8180 | `				zCmdEsc[j++] = '"';` |
|      1 | 8181 | `			} else {` |
|      1 | 8182 | `				zCmdEsc[j++] = ch;` |
|      - | 8183 | `			}` |
|      1 | 8184 | `		}` |
|      1 | 8185 | `		zCmdEsc[j] = '\0';` |
|      1 | 8186 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|      1 | 8187 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|      1 | 8188 | `		if (zWinCmd == NULL) {` |
|    ! 0 | 8189 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 8190 | `			return 0;` |
|      - | 8191 | `		}` |
|      1 | 8192 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|      1 | 8193 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|      1 | 8194 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|      1 | 8195 | `		zWinCmd[nTotal - 1] = '\0';` |
|      - | 8196 | `		/* Allocate pipe structure early so we can store handles */` |
|      1 | 8197 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|      1 | 8198 | `		if( pPipe == 0 ){` |
|    ! 0 | 8199 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 8200 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|    ! 0 | 8201 | `			return 0;` |
|      - | 8202 | `		}` |
|      - | 8203 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|      1 | 8204 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|      1 | 8205 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|      1 | 8206 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|      1 | 8207 | `		if( pFile == 0 ){` |
|    ! 0 | 8208 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|    ! 0 | 8209 | `			return 0;` |
|      - | 8210 | `		}` |
|      - | 8211 | `		/* Initialize remaining fields */` |
|      1 | 8212 | `		pPipe->pFile = pFile;` |
|      1 | 8213 | `		pPipe->pVm = pVm;` |
|      1 | 8214 | `		pPipe->iMode = zMode[0];` |
|      - | 8215 | `	}` |
|      - | 8216 | `#else /* Unix */` |
|     28 | 8217 | `	pFile = popen(zCommand, zMode);` |
|     28 | 8218 | `	if( pFile == 0 ){` |
|    ! 0 | 8219 | `		return 0;` |
|      - | 8220 | `	}` |
|      - | 8221 | `	/* Allocate pipe private structure */` |
|     28 | 8222 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|     28 | 8223 | `	if( pPipe == 0 ){` |
|      - | 8224 | `		/* Out of memory, close the pipe */` |
|    ! 0 | 8225 | `		pclose(pFile);` |
|    ! 0 | 8226 | `		return 0;` |
|      - | 8227 | `	}` |
|      - | 8228 | `	/* Initialize the structure */` |
|     28 | 8229 | `	pPipe->pFile = pFile;` |
|     28 | 8230 | `	pPipe->pVm = pVm;` |
|     28 | 8231 | `	pPipe->iMode = zMode[0];` |
|      - | 8232 | `#endif` |
|     29 | 8233 | `	return pPipe;` |
|     15 | 8234 |  |
|      - | 8235 | `/*` |
|      - | 8236 | ` * Close a pipe and return the exit status of the process.` |
|      - | 8237 | ` * Returns the exit status, or -1 on error.` |
|      - | 8238 | ` */` |
|     28 | 8239 | `static int PipeClose(pipe_private *pPipe)` |
|      1 | 8240 |  |
|      - | 8241 | `	int status;` |
|      - | 8242 | `	ph7_vm *pVm;` |
|     29 | 8243 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 8244 | `		return -1;` |
|      - | 8245 | `	}` |
|     29 | 8246 | `	pVm = pPipe->pVm;` |
|      - | 8247 | `	/* Close the pipe and get exit status */` |
|      - | 8248 | `#ifdef __WINNT__` |
|      - | 8249 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|      1 | 8250 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|      - | 8251 | `#else` |
|     28 | 8252 | `	status = pclose(pPipe->pFile);` |
|      - | 8253 | `	/* On Unix, pclose returns the status from waitpid, need to extract exit code */` |
|     28 | 8254 | `	if( status != -1 ){` |
|     28 | 8255 | `		if( WIFEXITED(status) ){` |
|     28 | 8256 | `			status = WEXITSTATUS(status);` |
|     14 | 8257 | `		}else if( WIFSIGNALED(status) ){` |
|      - | 8258 | `			/* Process was killed by a signal - use shell convention: 128 + signal number */` |
|    ! 0 | 8259 | `			status = 128 + WTERMSIG(status);` |
|    ! 0 | 8260 | `		}else{` |
|      - | 8261 | `			/* Unknown termination reason */` |
|    ! 0 | 8262 | `			status = -1;` |
|      - | 8263 | `		}` |
|     14 | 8264 | `	}` |
|      - | 8265 | `#endif` |
|      - | 8266 | `	/* Free the structure */` |
|     29 | 8267 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|     29 | 8268 | `	return status;` |
|     15 | 8269 |  |
|      - | 8270 | `/*` |
|      - | 8271 | ` * Pipe stream xClose implementation.` |
|      - | 8272 | ` * Note: This is called by fclose(), not pclose().` |
|      - | 8273 | ` * It closes the pipe but does not return the exit status.` |
|      - | 8274 | ` */` |
|     14 | 8275 | `static void PipeStream_Close(void *pHandle)` |
|      1 | 8276 |  |
|     15 | 8277 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|     15 | 8278 | `	if( pPipe ){` |
|     15 | 8279 | `		PipeClose(pPipe);` |
|      7 | 8280 | `	}` |
|     15 | 8281 |  |
|      - | 8282 | `/*` |
|      - | 8283 | ` * Pipe stream xRead implementation.` |
|      - | 8284 | ` */` |
|     34 | 8285 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|      1 | 8286 |  |
|     35 | 8287 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 8288 | `	size_t nRead;` |
|     35 | 8289 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 8290 | `		return -1;` |
|      - | 8291 | `	}` |
|     35 | 8292 | `	if( pPipe->iMode != 'r' ){` |
|      - | 8293 | `		/* Cannot read from a write-only pipe */` |
|    ! 0 | 8294 | `		return -1;` |
|      - | 8295 | `	}` |
|     35 | 8296 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
|     35 | 8297 | `	if( nRead == 0 ){` |
|     15 | 8298 | `		if( feof(pPipe->pFile) ){` |
|     15 | 8299 | `			return 0; /* EOF */` |
|      - | 8300 | `		}` |
|    ! 0 | 8301 | `		return -1; /* Error */` |
|      - | 8302 | `	}` |
|     21 | 8303 | `	return (ph7_int64)nRead;` |
|     18 | 8304 |  |
|      - | 8305 | `/*` |
|      - | 8306 | ` * Pipe stream xWrite implementation.` |
|      - | 8307 | ` */` |
|      2 | 8308 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|    ! 0 | 8309 |  |
|      2 | 8310 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 8311 | `	size_t nWritten;` |
|      2 | 8312 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 8313 | `		return -1;` |
|      - | 8314 | `	}` |
|      2 | 8315 | `	if( pPipe->iMode != 'w' ){` |
|      - | 8316 | `		/* Cannot write to a read-only pipe */` |
|    ! 0 | 8317 | `		return -1;` |
|      - | 8318 | `	}` |
|      2 | 8319 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|      2 | 8320 | `	if( nWritten == 0 && nWrite > 0 ){` |
|    ! 0 | 8321 | `		return -1; /* Error */` |
|      - | 8322 | `	}` |
|      2 | 8323 | `	return (ph7_int64)nWritten;` |
|      1 | 8324 |  |
|      - | 8325 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|      - | 8326 | `static const ph7_io_stream sPipe_Stream = {` |
|      - | 8327 | `	"pipe",` |
|      - | 8328 | `	PH7_IO_STREAM_VERSION,` |
|      - | 8329 | `	0,  /* xOpen - not used, pipes opened via PipeOpen() */` |
|      - | 8330 | `	0,  /* xOpenDir */` |
|      - | 8331 | `	PipeStream_Close,  /* xClose */` |
|      - | 8332 | `	0,  /* xCloseDir */` |
|      - | 8333 | `	PipeStream_Read,   /* xRead */` |
|      - | 8334 | `	0,  /* xReadDir */` |
|      - | 8335 | `	PipeStream_Write,  /* xWrite */` |
|      - | 8336 | `	0,  /* xSeek */` |
|      - | 8337 | `	0,  /* xLock */` |
|      - | 8338 | `	0,  /* xRewindDir */` |
|      - | 8339 | `	0,  /* xTell */` |
|      - | 8340 | `	0,  /* xTrunc */` |
|      - | 8341 | `	0,  /* xSync */` |
|      - | 8342 | `	0   /* xStat */` |
|      - | 8343 | `};` |
|      - | 8344 | `/*` |
|      - | 8345 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|      - | 8346 | ` * FALSE otherwise.` |
|      - | 8347 | ` */` |
|     14 | 8348 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|      1 | 8349 |  |
|     15 | 8350 | `	return pStream == &sPipe_Stream;` |
|      1 | 8351 |  |
|      - | 8352 | `/*` |
|      - | 8353 | ` * resource popen(string $command, string $mode)` |
|      - | 8354 | ` *  Opens process file pointer.` |
|      - | 8355 | ` * Parameters` |
|      - | 8356 | ` *  $command` |
|      - | 8357 | ` *   The command to execute. Passed to the system shell.` |
|      - | 8358 | ` *  $mode` |
|      - | 8359 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|      - | 8360 | ` *   'r' - Open for reading (read from the command's stdout).` |
|      - | 8361 | ` *   'w' - Open for writing (write to the command's stdin).` |
|      - | 8362 | ` * Return` |
|      - | 8363 | ` *  Returns a file pointer on success, or FALSE on error.` |
|      - | 8364 | ` */` |
|     28 | 8365 | `static int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8366 |  |
|      - | 8367 | `	const char *zCommand, *zMode;` |
|      - | 8368 | `	pipe_private *pPipe;` |
|      - | 8369 | `	io_private *pDev;` |
|      - | 8370 | `	int nCmdLen, nModeLen;` |
|     29 | 8371 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 8372 | `		/* Missing/Invalid arguments, return FALSE */` |
|    ! 0 | 8373 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a command string and mode");` |
|    ! 0 | 8374 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 8375 | `		return PH7_OK;` |
|      - | 8376 | `	}` |
|      - | 8377 | `	/* Extract the command and mode */` |
|     29 | 8378 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|     29 | 8379 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|     29 | 8380 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 8381 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty command");` |
|    ! 0 | 8382 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 8383 | `		return PH7_OK;` |
|      - | 8384 | `	}` |
|     29 | 8385 | `	if( nModeLen < 1 \|\| (zMode[0] != 'r' && zMode[0] != 'w') ){` |
|    ! 0 | 8386 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Invalid mode, expected 'r' or 'w'");` |
|    ! 0 | 8387 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 8388 | `		return PH7_OK;` |
|      - | 8389 | `	}` |
|      - | 8390 | `	/* Open the pipe */` |
|     29 | 8391 | `	pPipe = PipeOpen(pCtx->pVm, zCommand, zMode);` |
|     29 | 8392 | `	if( pPipe == 0 ){` |
|      - | 8393 | `		/* Failed to open pipe */` |
|    ! 0 | 8394 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 8395 | `		return PH7_OK;` |
|      - | 8396 | `	}` |
|      - | 8397 | `	/* Allocate an io_private instance to wrap the pipe */` |
|     29 | 8398 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|     29 | 8399 | `	if( pDev == 0 ){` |
|    ! 0 | 8400 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|    ! 0 | 8401 | `		PipeClose(pPipe);` |
|    ! 0 | 8402 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 8403 | `		return PH7_OK;` |
|      - | 8404 | `	}` |
|      - | 8405 | `	/* Initialize the io_private structure */` |
|     29 | 8406 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|     29 | 8407 | `	pDev->pHandle = pPipe;` |
|      - | 8408 | `	/* Return the io_private instance as a resource */` |
|     29 | 8409 | `	ph7_result_resource(pCtx, pDev);` |
|     29 | 8410 | `	return PH7_OK;` |
|     15 | 8411 |  |
|      - | 8412 | `/*` |
|      - | 8413 | ` * int pclose(resource $handle)` |
|      - | 8414 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|      - | 8415 | ` * Parameters` |
|      - | 8416 | ` *  $handle` |
|      - | 8417 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|      - | 8418 | ` * Return` |
|      - | 8419 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|      - | 8420 | ` */` |
|     14 | 8421 | `static int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8422 |  |
|      - | 8423 | `	const ph7_io_stream *pStream;` |
|      - | 8424 | `	pipe_private *pPipe;` |
|      - | 8425 | `	io_private *pDev;` |
|      - | 8426 | `	int status;` |
|     15 | 8427 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 8428 | `		/* Missing/Invalid arguments, return -1 */` |
|    ! 0 | 8429 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 8430 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 8431 | `		return PH7_OK;` |
|      - | 8432 | `	}` |
|      - | 8433 | `	/* Extract our private data */` |
|     15 | 8434 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 8435 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     15 | 8436 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 8437 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 8438 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 8439 | `		return PH7_OK;` |
|      - | 8440 | `	}` |
|      - | 8441 | `	/* Point to the target IO stream device */` |
|     15 | 8442 | `	pStream = pDev->pStream;` |
|     15 | 8443 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|    ! 0 | 8444 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|    ! 0 | 8445 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 8446 | `		return PH7_OK;` |
|      - | 8447 | `	}` |
|      - | 8448 | `	/* Get the pipe handle */` |
|     15 | 8449 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|      - | 8450 | `	/* Close the pipe and get exit status */` |
|     15 | 8451 | `	status = PipeClose(pPipe);` |
|      - | 8452 | `	/* Release the IO private structure */` |
|     15 | 8453 | `	ReleaseIOPrivate(pCtx, pDev);` |
|      - | 8454 | `	/* Invalidate the resource handle */` |
|     15 | 8455 | `	ph7_value_release(apArg[0]);` |
|      - | 8456 | `	/* Return the exit status */` |
|     15 | 8457 | `	ph7_result_int(pCtx, status);` |
|     15 | 8458 | `	return PH7_OK;` |
|      8 | 8459 |  |
|      - | 8460 | `/* Export the php:// stream */` |
|      - | 8461 | `static const ph7_io_stream sPHP_Stream = {` |
|      - | 8462 | `	"php",` |
|      - | 8463 | `	PH7_IO_STREAM_VERSION,` |
|      - | 8464 | `	PHPStreamData_Open,  /* xOpen */` |
|      - | 8465 | `	0,   /* xOpenDir */` |
|      - | 8466 | `	PHPStreamData_Close, /* xClose */` |
|      - | 8467 | `	0,  /* xCloseDir */` |
|      - | 8468 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 8469 | `	0,  /* xReadDir */` |
|      - | 8470 | `	PHPStreamData_Write, /* xWrite */` |
|      - | 8471 | `	0,  /* xSeek */` |
|      - | 8472 | `	0,  /* xLock */` |
|      - | 8473 | `	0,  /* xRewindDir */` |
|      - | 8474 | `	0,  /* xTell */` |
|      - | 8475 | `	0,  /* xTrunc */` |
|      - | 8476 | `	0,  /* xSeek */` |
|      - | 8477 | `	0   /* xStat */` |
|      - | 8478 | `};` |
|      - | 8479 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 8480 | `/*` |
|      - | 8481 | ` * Return TRUE if we are dealing with the php:// stream.` |
|      - | 8482 | ` * FALSE otherwise.` |
|      - | 8483 | ` */` |
|     62 | 8484 | `static int is_php_stream(const ph7_io_stream *pStream)` |
|      1 | 8485 |  |
|      - | 8486 | `#ifndef PH7_DISABLE_DISK_IO` |
|     63 | 8487 | `	return pStream == &sPHP_Stream;` |
|      - | 8488 | `#else` |
|      - | 8489 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 8490 | `	return 0;` |
|      - | 8491 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      1 | 8492 |  |
|      - | 8493 |  |
|      - | 8494 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 8495 | `/*` |
|      - | 8496 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 8497 | ` * [i.e: file://,php://].` |
|      - | 8498 | ` * Note:` |
|      - | 8499 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 8500 | ` *  defined then this function is a no-op.` |
|      - | 8501 | ` */` |
|   5092 | 8502 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      1 | 8503 |  |
|      - | 8504 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 8505 | `	      /* VFS functions */` |
|      - | 8506 | `	static const ph7_builtin_func aVfsFunc[] = {` |
|      - | 8507 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 8508 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 8509 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 8510 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 8511 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 8512 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 8513 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 8514 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 8515 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 8516 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 8517 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 8518 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 8519 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 8520 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 8521 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 8522 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 8523 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 8524 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 8525 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 8526 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 8527 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 8528 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 8529 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 8530 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 8531 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 8532 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 8533 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 8534 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 8535 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 8536 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 8537 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 8538 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 8539 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 8540 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 8541 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 8542 | `		{"link",        PH7_vfs_link     },` |
|      - | 8543 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 8544 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 8545 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 8546 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 8547 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 8548 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 8549 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 8550 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 8551 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 8552 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 8553 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 8554 | `		{"php_uname",   PH7_vfs_ph7_uname},` |
|      - | 8555 | `		     /* Path processing */` |
|      - | 8556 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 8557 | `		{"basename",    PH7_builtin_basename },` |
|      - | 8558 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 8559 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 8560 | `		{"fnmatch",     PH7_builtin_fnmatch  },` |
|      - | 8561 | `		     /* ZIP processing */` |
|      - | 8562 | `		{"zip_open",    PH7_builtin_zip_open },` |
|      - | 8563 | `		{"zip_close",   PH7_builtin_zip_close},` |
|      - | 8564 | `		{"zip_read",    PH7_builtin_zip_read },` |
|      - | 8565 | `		{"zip_entry_open", PH7_builtin_zip_entry_open },` |
|      - | 8566 | `		{"zip_entry_close",PH7_builtin_zip_entry_close},` |
|      - | 8567 | `		{"zip_entry_name", PH7_builtin_zip_entry_name },` |
|      - | 8568 | `		{"zip_entry_filesize",      PH7_builtin_zip_entry_filesize       },` |
|      - | 8569 | `		{"zip_entry_compressedsize",PH7_builtin_zip_entry_compressedsize },` |
|      - | 8570 | `		{"zip_entry_read", PH7_builtin_zip_entry_read },` |
|      - | 8571 | `		{"zip_entry_reset_read_cursor",PH7_builtin_zip_entry_reset_read_cursor},` |
|      - | 8572 | `		{"zip_entry_compressionmethod",PH7_builtin_zip_entry_compressionmethod}` |
|      - | 8573 | `	};` |
|      - | 8574 | `	    /* IO stream functions */` |
|      - | 8575 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 8576 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 8577 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 8578 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 8579 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 8580 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 8581 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 8582 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 8583 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 8584 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 8585 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 8586 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 8587 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 8588 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 8589 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 8590 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 8591 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 8592 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 8593 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 8594 | `		{"file",      PH7_builtin_file   },` |
|      - | 8595 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 8596 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 8597 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 8598 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 8599 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 8600 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 8601 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 8602 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 8603 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 8604 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 8605 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 8606 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 8607 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 8608 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 8609 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 8610 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8611 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 8612 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 8613 | `	};` |
|   5093 | 8614 | `	const ph7_io_stream *pFileStream = 0;` |
|   5093 | 8615 | `	sxu32 n = 0;` |
|      - | 8616 | `	/* Register the functions defined above */` |
| 330981 | 8617 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsFunc) ; ++n ){` |
| 325889 | 8618 | `		ph7_create_function(&(*pVm),aVfsFunc[n].zName,aVfsFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
| 162945 | 8619 | `	}` |
| 183313 | 8620 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 178221 | 8621 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  89111 | 8622 | `	}` |
|      - | 8623 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 8624 | `	/* Register the file stream if available */` |
|      - | 8625 | `#ifdef __WINNT__` |
|      1 | 8626 | `	pFileStream = &sWinFileStream;` |
|      - | 8627 | `#elif defined(__UNIXES__)` |
|   5092 | 8628 | `	pFileStream = &sUnixFileStream;` |
|      - | 8629 | `#endif` |
|      - | 8630 | `	/* Install the php:// stream */` |
|   5093 | 8631 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|      - | 8632 | `#endif /* PH7_DISABLE_DISK_IO */` |
|   5093 | 8633 | `	if( pFileStream ){` |
|      - | 8634 | `		/* Install the file:// stream */` |
|   5093 | 8635 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   2546 | 8636 | `	}` |
|      - | 8637 | `#else` |
|      - | 8638 | `   SXUNUSED(pVm); /* cc warning */` |
|      - | 8639 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|   5093 | 8640 | `	return SXRET_OK;` |
|      1 | 8641 |  |
|      - | 8642 | `/*` |
|      - | 8643 | ` * Export the STDIN handle.` |
|      - | 8644 | ` */` |
|      2 | 8645 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|      1 | 8646 |  |
|      - | 8647 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 8648 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 8649 | `	if( pVm->pStdin == 0  ){` |
|      - | 8650 | `		io_private *pIn;` |
|      - | 8651 | `		/* Allocate an IO private instance */` |
|      3 | 8652 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 8653 | `		if( pIn == 0 ){` |
|    ! 0 | 8654 | `			return 0;` |
|      - | 8655 | `		}` |
|      3 | 8656 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|      - | 8657 | `		/* Initialize the handle */` |
|      3 | 8658 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|      - | 8659 | `		/* Install the STDIN stream */` |
|      3 | 8660 | `		pVm->pStdin = pIn;` |
|      3 | 8661 | `		return pIn;` |
|    ! 0 | 8662 | `	}else{` |
|      - | 8663 | `		/* NULL or STDIN */` |
|    ! 0 | 8664 | `		return pVm->pStdin;` |
|      - | 8665 | `	}` |
|      - | 8666 | `#else` |
|      - | 8667 | `	return 0;` |
|      - | 8668 | `#endif` |
|      - | 8669 | `#else` |
|      - | 8670 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 8671 | `	return 0;` |
|      - | 8672 | `#endif` |
|      2 | 8673 |  |
|      - | 8674 | `/*` |
|      - | 8675 | ` * Export the STDOUT handle.` |
|      - | 8676 | ` */` |
|      2 | 8677 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|      1 | 8678 |  |
|      - | 8679 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 8680 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 8681 | `	if( pVm->pStdout == 0  ){` |
|      - | 8682 | `		io_private *pOut;` |
|      - | 8683 | `		/* Allocate an IO private instance */` |
|      3 | 8684 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 8685 | `		if( pOut == 0 ){` |
|    ! 0 | 8686 | `			return 0;` |
|      - | 8687 | `		}` |
|      3 | 8688 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|      - | 8689 | `		/* Initialize the handle */` |
|      3 | 8690 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|      - | 8691 | `		/* Install the STDOUT stream */` |
|      3 | 8692 | `		pVm->pStdout = pOut;` |
|      3 | 8693 | `		return pOut;` |
|    ! 0 | 8694 | `	}else{` |
|      - | 8695 | `		/* NULL or STDOUT */` |
|    ! 0 | 8696 | `		return pVm->pStdout;` |
|      - | 8697 | `	}` |
|      - | 8698 | `#else` |
|      - | 8699 | `	return 0;` |
|      - | 8700 | `#endif` |
|      - | 8701 | `#else` |
|      - | 8702 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 8703 | `	return 0;` |
|      - | 8704 | `#endif` |
|      2 | 8705 |  |
|      - | 8706 | `/*` |
|      - | 8707 | ` * Export the STDERR handle.` |
|      - | 8708 | ` */` |
|      2 | 8709 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|      1 | 8710 |  |
|      - | 8711 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 8712 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 8713 | `	if( pVm->pStderr == 0  ){` |
|      - | 8714 | `		io_private *pErr;` |
|      - | 8715 | `		/* Allocate an IO private instance */` |
|      3 | 8716 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 8717 | `		if( pErr == 0 ){` |
|    ! 0 | 8718 | `			return 0;` |
|      - | 8719 | `		}` |
|      3 | 8720 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|      - | 8721 | `		/* Initialize the handle */` |
|      3 | 8722 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|      - | 8723 | `		/* Install the STDERR stream */` |
|      3 | 8724 | `		pVm->pStderr = pErr;` |
|      3 | 8725 | `		return pErr;` |
|    ! 0 | 8726 | `	}else{` |
|      - | 8727 | `		/* NULL or STDERR */` |
|    ! 0 | 8728 | `		return pVm->pStderr;` |
|      - | 8729 | `	}` |
|      - | 8730 | `#else` |
|      - | 8731 | `	return 0;` |
|      - | 8732 | `#endif` |
|      - | 8733 | `#else` |
|      - | 8734 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 8735 | `	return 0;` |
|      - | 8736 | `#endif` |
|      2 | 8737 |  |
|      - | 8738 |  |
