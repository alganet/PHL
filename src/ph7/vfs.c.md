# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1854/2856 lines (64.92%)

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
|     50 |   19 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)` |
|      5 |   20 | `{` |
|     55 |   21 | `	const char *zEnd = &zPath[nByte - 1];` |
|      - |   22 | `	int c,d;` |
|     55 |   23 | `	c = d = '/';` |
|      - |   24 | `#ifdef __WINNT__` |
|      5 |   25 | `	d = '\\';` |
|      - |   26 | `#endif` |
|   1086 |   27 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   1011 |   28 | `		zEnd--;` |
|      5 |   29 | `	}` |
|     55 |   30 | `	*pLen = (int)(zEnd-zPath);` |
|      - |   31 | `#ifdef __WINNT__` |
|      5 |   32 | `	if( (*pLen) == (int)sizeof(char) && zPath[0] == '/' ){` |
|      - |   33 | `		/* Normalize path on windows */` |
|    ! 0 |   34 | `		return "\\";` |
|      - |   35 | `	}` |
|      - |   36 | `#endif` |
|     55 |   37 | `	if( zEnd == zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d) ){` |
|      - |   38 | `		/* No separator,return "." as the current directory */` |
|      5 |   39 | `		*pLen = sizeof(char);` |
|      5 |   40 | `		return ".";` |
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
|     30 |   51 | `}` |
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
|  13400 |   66 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |   67 | `{` |
|      - |   68 | `	const char *zPath;` |
|      - |   69 | `	ph7_vfs *pVfs;` |
|      - |   70 | `	int rc;` |
|  13405 |   71 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |   72 | `		/* Missing/Invalid argument,return FALSE */` |
|      6 |   73 | `		ph7_result_bool(pCtx,0);` |
|      6 |   74 | `		return PH7_OK;` |
|      - |   75 | `	}` |
|      - |   76 | `	/* Point to the underlying vfs */` |
|  13401 |   77 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  13401 |   78 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |   79 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |   80 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |   81 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |   82 | `			ph7_function_name(pCtx)` |
|      - |   83 | `			);` |
|    ! 0 |   84 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |   85 | `		return PH7_OK;` |
|      - |   86 | `	}` |
|      - |   87 | `	/* Point to the desired directory */` |
|  13401 |   88 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |   89 | `	/* Perform the requested operation */` |
|  13401 |   90 | `	rc = pVfs->xChdir(zPath);` |
|      - |   91 | `	/* IO return value */` |
|  13401 |   92 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  13401 |   93 | `	return PH7_OK;` |
|   6705 |   94 | `}` |
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
|   8300 |  214 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  215 | `{` |
|      - |  216 | `	const char *zPath;` |
|      - |  217 | `	ph7_vfs *pVfs;` |
|      - |  218 | `	int rc;` |
|   8305 |  219 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  220 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  221 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  222 | `		return PH7_OK;` |
|      - |  223 | `	}` |
|      - |  224 | `	/* Point to the underlying vfs */` |
|   8305 |  225 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   8305 |  226 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  227 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  228 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  229 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  230 | `			ph7_function_name(pCtx)` |
|      - |  231 | `			);` |
|    ! 0 |  232 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  233 | `		return PH7_OK;` |
|      - |  234 | `	}` |
|      - |  235 | `	/* Point to the desired directory */` |
|   8305 |  236 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  237 | `	/* Perform the requested operation */` |
|   8305 |  238 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  239 | `	/* IO return value */` |
|   8305 |  240 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   8305 |  241 | `	return PH7_OK;` |
|   4155 |  242 | `}` |
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
|     30 |  262 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  263 | `{` |
|     31 |  264 | `	int iRecursive = 0;` |
|      - |  265 | `	const char *zPath;` |
|      - |  266 | `	ph7_vfs *pVfs;` |
|      - |  267 | `	int iMode,rc;` |
|     31 |  268 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  269 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 |  270 | `		ph7_result_bool(pCtx,0);` |
|      3 |  271 | `		return PH7_OK;` |
|      - |  272 | `	}` |
|      - |  273 | `	/* Point to the underlying vfs */` |
|     29 |  274 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     29 |  275 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  276 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  277 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  278 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  279 | `			ph7_function_name(pCtx)` |
|      - |  280 | `			);` |
|    ! 0 |  281 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  282 | `		return PH7_OK;` |
|      - |  283 | `	}` |
|      - |  284 | `	/* Point to the desired directory */` |
|     29 |  285 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  286 | `#ifdef __WINNT__` |
|      1 |  287 | `	iMode = 0;` |
|      - |  288 | `#else` |
|      - |  289 | `	/* Assume UNIX */` |
|     28 |  290 | `	iMode = 0777;` |
|      - |  291 | `#endif` |
|     29 |  292 | `	if( nArg > 1 ){` |
|    ! 0 |  293 | `		iMode = ph7_value_to_int(apArg[1]);` |
|    ! 0 |  294 | `		if( nArg > 2 ){` |
|    ! 0 |  295 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|    ! 0 |  296 | `		}` |
|    ! 0 |  297 | `	}` |
|      - |  298 | `	/* Perform the requested operation */` |
|     29 |  299 | `	rc = pVfs->xMkdir(zPath,iMode,iRecursive);` |
|      - |  300 | `	/* IO return value */` |
|     29 |  301 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     29 |  302 | `	return PH7_OK;` |
|     16 |  303 | `}` |
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
|  31908 |  477 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  478 | `{` |
|      - |  479 | `	const char *zPath;` |
|      - |  480 | `	ph7_vfs *pVfs;` |
|      - |  481 | `	int rc;` |
|  31913 |  482 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  483 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  484 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  485 | `		return PH7_OK;` |
|      - |  486 | `	}` |
|      - |  487 | `	/* Point to the underlying vfs */` |
|  31913 |  488 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  31913 |  489 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  490 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  491 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  492 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  493 | `			ph7_function_name(pCtx)` |
|      - |  494 | `			);` |
|    ! 0 |  495 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  496 | `		return PH7_OK;` |
|      - |  497 | `	}` |
|      - |  498 | `	/* Point to the desired directory */` |
|  31913 |  499 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  500 | `	/* Perform the requested operation */` |
|  31913 |  501 | `	rc = pVfs->xUnlink(zPath);` |
|      - |  502 | `	/* IO return value */` |
|  31913 |  503 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  31913 |  504 | `	return PH7_OK;` |
|  15959 |  505 | `}` |
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
|     46 |  718 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  719 | `{` |
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
|     24 |  746 | `}` |
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
|   6354 |  908 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  909 | `{` |
|      - |  910 | `	const char *zPath;` |
|      - |  911 | `	ph7_vfs *pVfs;` |
|      - |  912 | `	int rc;` |
|   6359 |  913 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  914 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  915 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  916 | `		return PH7_OK;` |
|      - |  917 | `	}` |
|      - |  918 | `	/* Point to the underlying vfs */` |
|   6359 |  919 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   6359 |  920 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - |  921 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  922 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  923 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  924 | `			ph7_function_name(pCtx)` |
|      - |  925 | `			);` |
|    ! 0 |  926 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  927 | `		return PH7_OK;` |
|      - |  928 | `	}` |
|      - |  929 | `	/* Point to the desired directory */` |
|   6359 |  930 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  931 | `	/* Perform the requested operation */` |
|   6359 |  932 | `	rc = pVfs->xIsfile(zPath);` |
|      - |  933 | `	/* IO return value */` |
|   6359 |  934 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   6359 |  935 | `	return PH7_OK;` |
|   3182 |  936 | `}` |
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
|     46 | 1275 | `static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1276 | `{` |
|      - | 1277 | `	const char *zEnv;` |
|      - | 1278 | `	ph7_vfs *pVfs;` |
|      - | 1279 | `	int iLen;` |
|     51 | 1280 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1281 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1282 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1283 | `		return PH7_OK;` |
|      - | 1284 | `	}` |
|      - | 1285 | `	/* Point to the underlying vfs */` |
|     51 | 1286 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     51 | 1287 | `	if( pVfs == 0 \|\| pVfs->xGetenv == 0 ){` |
|      - | 1288 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1289 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1290 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1291 | `			ph7_function_name(pCtx)` |
|      - | 1292 | `			);` |
|    ! 0 | 1293 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1294 | `		return PH7_OK;` |
|      - | 1295 | `	}` |
|      - | 1296 | `	/* Extract the environment variable */` |
|     51 | 1297 | `	zEnv = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1298 | `	/* Set a boolean FALSE as the default return value */` |
|     51 | 1299 | `	ph7_result_bool(pCtx,0);` |
|     51 | 1300 | `	if( iLen < 1 ){` |
|      - | 1301 | `		/* Empty string */` |
|    ! 0 | 1302 | `		return PH7_OK;` |
|      - | 1303 | `	}` |
|      - | 1304 | `	/* Perform the requested operation */` |
|     51 | 1305 | `	pVfs->xGetenv(zEnv,pCtx);` |
|     51 | 1306 | `	return PH7_OK;` |
|     28 | 1307 | `}` |
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
|  12698 | 1555 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 1556 | `{` |
|  12703 | 1557 | `	const char *zPtr,*zEnd = &zPath[nByte - 1];` |
|      - | 1558 | `	SyString *pCur;` |
|      - | 1559 | `	int c,d;` |
|  12703 | 1560 | `	c = d = '/';` |
|      - | 1561 | `#ifdef __WINNT__` |
|      5 | 1562 | `	d = '\\';` |
|      - | 1563 | `#endif` |
|      - | 1564 | `	/* Zero the structure */` |
|  12703 | 1565 | `	SyZero(pOut,sizeof(path_info));` |
|      - | 1566 | `	/* Handle special case */` |
|  12703 | 1567 | `	if( nByte == sizeof(char) && ( (int)zPath[0] == c \|\| (int)zPath[0] == d ) ){` |
|      - | 1568 | `#ifdef __WINNT__` |
|    ! 0 | 1569 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1570 | `#else` |
|    ! 0 | 1571 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1572 | `#endif` |
|    ! 0 | 1573 | `		return SXRET_OK;` |
|      - | 1574 | `	}` |
|      - | 1575 | `	/* Extract the basename */` |
| 342532 | 1576 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
| 323485 | 1577 | `		zEnd--;` |
|      5 | 1578 | `	}` |
|  12703 | 1579 | `	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|  12703 | 1580 | `	zEnd = &zPath[nByte];` |
|      - | 1581 | `	/* dirname */` |
|  12703 | 1582 | `	pCur = &pOut->sDir;` |
|  12703 | 1583 | `	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);` |
|  12703 | 1584 | `	if( pCur->nByte > 1 ){` |
|  25401 | 1585 | `		SyStringTrimTrailingChar(pCur,'/');` |
|      - | 1586 | `#ifdef __WINNT__` |
|      5 | 1587 | `		SyStringTrimTrailingChar(pCur,'\\');` |
|      - | 1588 | `#endif` |
|   6354 | 1589 | `	}else if( (int)zPath[0] == c \|\| (int)zPath[0] == d ){` |
|      - | 1590 | `#ifdef __WINNT__` |
|    ! 0 | 1591 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1592 | `#else` |
|    ! 0 | 1593 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1594 | `#endif` |
|    ! 0 | 1595 | `	}` |
|      - | 1596 | `	/* basename/filename */` |
|  12703 | 1597 | `	pCur = &pOut->sBasename;` |
|  12703 | 1598 | `	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);` |
|  12703 | 1599 | `	SyStringTrimLeadingChar(pCur,'/');` |
|      - | 1600 | `#ifdef __WINNT__` |
|      5 | 1601 | `	SyStringTrimLeadingChar(pCur,'\\');` |
|      - | 1602 | `#endif` |
|  12703 | 1603 | `	SyStringDupPtr(&pOut->sFilename,pCur);` |
|  12703 | 1604 | `	if( pCur->nByte > 0 ){` |
|      - | 1605 | `		/* extension */` |
|  12703 | 1606 | `		zEnd--;` |
|  63489 | 1607 | `		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){` |
|  50791 | 1608 | `			zEnd--;` |
|      5 | 1609 | `		}` |
|  12703 | 1610 | `		if( zEnd > pCur->zString ){` |
|  12701 | 1611 | `			zEnd++; /* Jump leading dot */` |
|  12701 | 1612 | `			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);` |
|      - | 1613 | `			/* Fix filename */` |
|  12701 | 1614 | `			pCur = &pOut->sFilename;` |
|  12701 | 1615 | `			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){` |
|  12701 | 1616 | `				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);` |
|   6348 | 1617 | `			}` |
|   6348 | 1618 | `		}` |
|   6349 | 1619 | `	}` |
|  12703 | 1620 | `	return SXRET_OK;` |
|   6354 | 1621 | `}` |
|      - | 1622 | `/*` |
|      - | 1623 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1624 | ` *  See block comment above.` |
|      - | 1625 | ` */` |
|  12698 | 1626 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1627 | `{` |
|      - | 1628 | `	const char *zPath;` |
|      - | 1629 | `	path_info sInfo;` |
|      - | 1630 | `	SyString *pComp;` |
|      - | 1631 | `	int iLen;` |
|  12703 | 1632 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1633 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1634 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1635 | `		return PH7_OK;` |
|      - | 1636 | `	}` |
|      - | 1637 | `	/* Point to the target path */` |
|  12703 | 1638 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|  12703 | 1639 | `	if( iLen < 1 ){` |
|      - | 1640 | `		/* Empty string */` |
|    ! 0 | 1641 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1642 | `		return PH7_OK;` |
|      - | 1643 | `	}` |
|      - | 1644 | `	/* Extract path info */` |
|  12703 | 1645 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|  19051 | 1646 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1647 | `		/* Return path component */` |
|  12701 | 1648 | `		int nComp = ph7_value_to_int(apArg[1]);` |
|  12701 | 1649 | `		switch(nComp){` |
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
|   3175 | 1668 | `		case 3: /*PATHINFO_EXTENSION*/` |
|   6355 | 1669 | `			pComp = &sInfo.sExtension;` |
|   6355 | 1670 | `			if( pComp->nByte > 0 ){` |
|   6353 | 1671 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3179 | 1672 | `			}else{` |
|      - | 1673 | `				/* Expand the empty string */` |
|      3 | 1674 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1675 | `			}` |
|   6355 | 1676 | `			break;` |
|   3171 | 1677 | `		case 4: /*PATHINFO_FILENAME*/` |
|   6347 | 1678 | `			pComp = &sInfo.sFilename;` |
|   6347 | 1679 | `			if( pComp->nByte > 0 ){` |
|   6347 | 1680 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3176 | 1681 | `			}else{` |
|      - | 1682 | `				/* Expand the empty string */` |
|    ! 0 | 1683 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1684 | `			}` |
|   6347 | 1685 | `			break;` |
|    ! 0 | 1686 | `		default:` |
|      - | 1687 | `			/* Expand the empty string */` |
|    ! 0 | 1688 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1689 | `			break;` |
|      - | 1690 | `		}` |
|   6353 | 1691 | `	}else{` |
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
|  12703 | 1741 | `	return PH7_OK;` |
|   6354 | 1742 | `}` |
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
|    204 | 2140 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2141 | `{` |
|      - | 2142 | `	ph7_vfs *pVfs;` |
|      - | 2143 | `	/* Set the empty string as the default return value */` |
|    207 | 2144 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2145 | `	/* Point to the underlying vfs */` |
|    207 | 2146 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    207 | 2147 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
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
|    207 | 2158 | `	pVfs->xTempDir(pCtx);` |
|    207 | 2159 | `	return PH7_OK;` |
|    105 | 2160 | `}` |
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
|     64 | 2198 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2199 | `{` |
|      - | 2200 | `	ph7_int64 nProcessId;` |
|      - | 2201 | `	ph7_vfs *pVfs;` |
|      - | 2202 | `	/* Point to the underlying vfs */` |
|     65 | 2203 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     65 | 2204 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
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
|     65 | 2216 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 2217 | `	/* Set the result */` |
|     65 | 2218 | `	ph7_result_int64(pCtx,nProcessId);` |
|     65 | 2219 | `	return PH7_OK;` |
|     33 | 2220 | `}` |
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
|      - | 2435 | `/* Make sure we are dealing with a valid io_private instance */` |
|      - | 2436 | `#define IO_PRIVATE_INVALID(IO) ( IO == 0 \|\| IO->iMagic != IO_PRIVATE_MAGIC )` |
|      - | 2437 | `/* Forward declaration */` |
|      - | 2438 | `static void ResetIOPrivate(io_private *pDev);` |
|      - | 2439 | `/*` |
|      - | 2440 | ` * Return the PHP resource-type name for a raw resource handle.` |
|      - | 2441 | ` * Every IO handle this VFS hands out (fopen/tmpfile/popen/opendir and the` |
|      - | 2442 | ` * STDIN/STDOUT/STDERR constants) is an io_private, which PHP reports as` |
|      - | 2443 | ` * "stream"; anything else is "Unknown". The magic probe mirrors the` |
|      - | 2444 | ` * IO_PRIVATE_INVALID check the rest of this file uses to validate handles.` |
|      - | 2445 | ` */` |
|      4 | 2446 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      1 | 2447 | `{` |
|      5 | 2448 | `	io_private *pDev = (io_private *)pResource;` |
|      5 | 2449 | `	if( !IO_PRIVATE_INVALID(pDev) ){` |
|      5 | 2450 | `		return "stream";` |
|      - | 2451 | `	}` |
|    ! 0 | 2452 | `	return "Unknown";` |
|      3 | 2453 | `}` |
|      - | 2454 | `/*` |
|      - | 2455 | ` * bool ftruncate(resource $handle,int64 $size)` |
|      - | 2456 | ` *  Truncates a file to a given length.` |
|      - | 2457 | ` * Parameters` |
|      - | 2458 | ` *  $handle` |
|      - | 2459 | ` *   The file pointer.` |
|      - | 2460 | ` *   Note:` |
|      - | 2461 | ` *    The handle must be open for writing.` |
|      - | 2462 | ` * $size` |
|      - | 2463 | ` *   The size to truncate to.` |
|      - | 2464 | ` * Return` |
|      - | 2465 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2466 | ` */` |
|      6 | 2467 | `static int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2468 | `{` |
|      - | 2469 | `	const ph7_io_stream *pStream;` |
|      - | 2470 | `	io_private *pDev;` |
|      - | 2471 | `	int rc;` |
|      7 | 2472 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2473 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2474 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2475 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2476 | `		return PH7_OK;` |
|      - | 2477 | `	}` |
|      - | 2478 | `	/* Extract our private data */` |
|      7 | 2479 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2480 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 2481 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2482 | `		/*Expecting an IO handle */` |
|    ! 0 | 2483 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2484 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2485 | `		return PH7_OK;` |
|      - | 2486 | `	}` |
|      - | 2487 | `	/* Point to the target IO stream device */` |
|      7 | 2488 | `	pStream = pDev->pStream;` |
|      7 | 2489 | `	if( pStream == 0  \|\| pStream->xTrunc == 0){` |
|    ! 0 | 2490 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2491 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2492 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2493 | `			);` |
|    ! 0 | 2494 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2495 | `		return PH7_OK;` |
|      - | 2496 | `	}` |
|      - | 2497 | `	/* Perform the requested operation */` |
|      7 | 2498 | `	rc = pStream->xTrunc(pDev->pHandle,ph7_value_to_int64(apArg[1]));` |
|      7 | 2499 | `	if( rc == PH7_OK ){` |
|      - | 2500 | `		/* Discard buffered data */` |
|      7 | 2501 | `		ResetIOPrivate(pDev);` |
|      3 | 2502 | `	}` |
|      - | 2503 | `	/* IO result */` |
|      7 | 2504 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      7 | 2505 | `	return PH7_OK;` |
|      4 | 2506 | `}` |
|      - | 2507 | `/*` |
|      - | 2508 | ` * int fseek(resource $handle,int $offset[,int $whence = SEEK_SET ])` |
|      - | 2509 | ` *  Seeks on a file pointer.` |
|      - | 2510 | ` * Parameters` |
|      - | 2511 | ` *  $handle` |
|      - | 2512 | ` *   A file system pointer resource that is typically created using fopen().` |
|      - | 2513 | ` * $offset` |
|      - | 2514 | ` *   The offset.` |
|      - | 2515 | ` *   To move to a position before the end-of-file, you need to pass a negative` |
|      - | 2516 | ` *   value in offset and set whence to SEEK_END.` |
|      - | 2517 | ` *   whence` |
|      - | 2518 | ` *   whence values are:` |
|      - | 2519 | ` *    SEEK_SET - Set position equal to offset bytes.` |
|      - | 2520 | ` *    SEEK_CUR - Set position to current location plus offset.` |
|      - | 2521 | ` *    SEEK_END - Set position to end-of-file plus offset.` |
|      - | 2522 | ` * Return` |
|      - | 2523 | ` *  0 on success,-1 on failure` |
|      - | 2524 | ` */` |
|      2 | 2525 | `static int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2526 | `{` |
|      - | 2527 | `	const ph7_io_stream *pStream;` |
|      - | 2528 | `	io_private *pDev;` |
|      - | 2529 | `	ph7_int64 iOfft;` |
|      - | 2530 | `	int whence;` |
|      - | 2531 | `	int rc;` |
|      3 | 2532 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2533 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2534 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2535 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2536 | `		return PH7_OK;` |
|      - | 2537 | `	}` |
|      - | 2538 | `	/* Extract our private data */` |
|      3 | 2539 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2540 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 2541 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2542 | `		/*Expecting an IO handle */` |
|    ! 0 | 2543 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2544 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2545 | `		return PH7_OK;` |
|      - | 2546 | `	}` |
|      - | 2547 | `	/* Point to the target IO stream device */` |
|      3 | 2548 | `	pStream = pDev->pStream;` |
|      3 | 2549 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2550 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2551 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2552 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2553 | `			);` |
|    ! 0 | 2554 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2555 | `		return PH7_OK;` |
|      - | 2556 | `	}` |
|      - | 2557 | `	/* Extract the offset */` |
|      3 | 2558 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|      3 | 2559 | `	whence = 0;/* SEEK_SET */` |
|      3 | 2560 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|    ! 0 | 2561 | `		whence = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2562 | `	}` |
|      - | 2563 | `	/* Perform the requested operation */` |
|      3 | 2564 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|      3 | 2565 | `	if( rc == PH7_OK ){` |
|      - | 2566 | `		/* Ignore buffered data */` |
|      3 | 2567 | `		ResetIOPrivate(pDev);` |
|      1 | 2568 | `	}` |
|      - | 2569 | `	/* IO result */` |
|      3 | 2570 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|      3 | 2571 | `	return PH7_OK;` |
|      2 | 2572 | `}` |
|      - | 2573 | `/*` |
|      - | 2574 | ` * int64 ftell(resource $handle)` |
|      - | 2575 | ` *  Returns the current position of the file read/write pointer.` |
|      - | 2576 | ` * Parameters` |
|      - | 2577 | ` *  $handle` |
|      - | 2578 | ` *   The file pointer.` |
|      - | 2579 | ` * Return` |
|      - | 2580 | ` *  Returns the position of the file pointer referenced by handle` |
|      - | 2581 | ` *  as an integer; i.e., its offset into the file stream.` |
|      - | 2582 | ` *  FALSE is returned on failure.` |
|      - | 2583 | ` */` |
|      6 | 2584 | `static int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2585 | `{` |
|      - | 2586 | `	const ph7_io_stream *pStream;` |
|      - | 2587 | `	io_private *pDev;` |
|      - | 2588 | `	ph7_int64 iOfft;` |
|      7 | 2589 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2590 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2591 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2592 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2593 | `		return PH7_OK;` |
|      - | 2594 | `	}` |
|      - | 2595 | `	/* Extract our private data */` |
|      7 | 2596 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2597 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 2598 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2599 | `		/*Expecting an IO handle */` |
|    ! 0 | 2600 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2601 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2602 | `		return PH7_OK;` |
|      - | 2603 | `	}` |
|      - | 2604 | `	/* Point to the target IO stream device */` |
|      7 | 2605 | `	pStream = pDev->pStream;` |
|      7 | 2606 | `	if( pStream == 0  \|\| pStream->xTell == 0){` |
|    ! 0 | 2607 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2608 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2609 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2610 | `			);` |
|    ! 0 | 2611 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2612 | `		return PH7_OK;` |
|      - | 2613 | `	}` |
|      - | 2614 | `	/* Perform the requested operation */` |
|      7 | 2615 | `	iOfft = pStream->xTell(pDev->pHandle);` |
|      - | 2616 | `	/* IO result */` |
|      7 | 2617 | `	ph7_result_int64(pCtx,iOfft);` |
|      7 | 2618 | `	return PH7_OK;` |
|      4 | 2619 | `}` |
|      - | 2620 | `/*` |
|      - | 2621 | ` * bool rewind(resource $handle)` |
|      - | 2622 | ` *  Rewind the position of a file pointer.` |
|      - | 2623 | ` * Parameters` |
|      - | 2624 | ` *  $handle` |
|      - | 2625 | ` *   The file pointer.` |
|      - | 2626 | ` * Return` |
|      - | 2627 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2628 | ` */` |
|      4 | 2629 | `static int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2630 | `{` |
|      - | 2631 | `	const ph7_io_stream *pStream;` |
|      - | 2632 | `	io_private *pDev;` |
|      - | 2633 | `	int rc;` |
|      5 | 2634 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2635 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2636 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2637 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2638 | `		return PH7_OK;` |
|      - | 2639 | `	}` |
|      - | 2640 | `	/* Extract our private data */` |
|      5 | 2641 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2642 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 2643 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2644 | `		/*Expecting an IO handle */` |
|    ! 0 | 2645 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2646 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2647 | `		return PH7_OK;` |
|      - | 2648 | `	}` |
|      - | 2649 | `	/* Point to the target IO stream device */` |
|      5 | 2650 | `	pStream = pDev->pStream;` |
|      5 | 2651 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2652 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2653 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2654 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2655 | `			);` |
|    ! 0 | 2656 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2657 | `		return PH7_OK;` |
|      - | 2658 | `	}` |
|      - | 2659 | `	/* Perform the requested operation */` |
|      5 | 2660 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|      5 | 2661 | `	if( rc == PH7_OK ){` |
|      - | 2662 | `		/* Ignore buffered data */` |
|      5 | 2663 | `		ResetIOPrivate(pDev);` |
|      2 | 2664 | `	}` |
|      - | 2665 | `	/* IO result */` |
|      5 | 2666 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 2667 | `	return PH7_OK;` |
|      3 | 2668 | `}` |
|      - | 2669 | `/*` |
|      - | 2670 | ` * bool fflush(resource $handle)` |
|      - | 2671 | ` *  Flushes the output to a file.` |
|      - | 2672 | ` * Parameters` |
|      - | 2673 | ` *  $handle` |
|      - | 2674 | ` *   The file pointer.` |
|      - | 2675 | ` * Return` |
|      - | 2676 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2677 | ` */` |
|      2 | 2678 | `static int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2679 | `{` |
|      - | 2680 | `	const ph7_io_stream *pStream;` |
|      - | 2681 | `	io_private *pDev;` |
|      - | 2682 | `	int rc;` |
|      3 | 2683 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2684 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2685 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2686 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2687 | `		return PH7_OK;` |
|      - | 2688 | `	}` |
|      - | 2689 | `	/* Extract our private data */` |
|      3 | 2690 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2691 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 2692 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2693 | `		/*Expecting an IO handle */` |
|    ! 0 | 2694 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2695 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2696 | `		return PH7_OK;` |
|      - | 2697 | `	}` |
|      - | 2698 | `	/* Point to the target IO stream device */` |
|      3 | 2699 | `	pStream = pDev->pStream;` |
|      3 | 2700 | `	if( pStream == 0 \|\| pStream->xSync == 0){` |
|    ! 0 | 2701 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2702 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2703 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2704 | `			);` |
|    ! 0 | 2705 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2706 | `		return PH7_OK;` |
|      - | 2707 | `	}` |
|      - | 2708 | `	/* Perform the requested operation */` |
|      3 | 2709 | `	rc = pStream->xSync(pDev->pHandle);` |
|      - | 2710 | `	/* IO result */` |
|      3 | 2711 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 2712 | `	return PH7_OK;` |
|      2 | 2713 | `}` |
|      - | 2714 | `/*` |
|      - | 2715 | ` * bool feof(resource $handle)` |
|      - | 2716 | ` *  Tests for end-of-file on a file pointer.` |
|      - | 2717 | ` * Parameters` |
|      - | 2718 | ` *  $handle` |
|      - | 2719 | ` *   The file pointer.` |
|      - | 2720 | ` * Return` |
|      - | 2721 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|      - | 2722 | ` */` |
|  10344 | 2723 | `static int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2724 | `{` |
|      - | 2725 | `	const ph7_io_stream *pStream;` |
|      - | 2726 | `	io_private *pDev;` |
|      - | 2727 | `	int rc;` |
|  10349 | 2728 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2729 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 2730 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2731 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2732 | `		return PH7_OK;` |
|      - | 2733 | `	}` |
|      - | 2734 | `	/* Extract our private data */` |
|  10349 | 2735 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2736 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  10349 | 2737 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2738 | `		/*Expecting an IO handle */` |
|    ! 0 | 2739 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2740 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2741 | `		return PH7_OK;` |
|      - | 2742 | `	}` |
|      - | 2743 | `	/* Point to the target IO stream device */` |
|  10349 | 2744 | `	pStream = pDev->pStream;` |
|  10349 | 2745 | `	if( pStream == 0 ){` |
|    ! 0 | 2746 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2747 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2748 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2749 | `			);` |
|    ! 0 | 2750 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2751 | `		return PH7_OK;` |
|      - | 2752 | `	}` |
|  10349 | 2753 | `	rc = SXERR_EOF;` |
|      - | 2754 | `	/* Perform the requested operation */` |
|  10349 | 2755 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2756 | `		/* Data is available */` |
|   4711 | 2757 | `		rc = PH7_OK;` |
|   2358 | 2758 | `	}else{` |
|      - | 2759 | `		char zBuf[4096];` |
|      - | 2760 | `		ph7_int64 n;` |
|      - | 2761 | `		/* Perform a buffered read */` |
|   5643 | 2762 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|   5643 | 2763 | `		if( n > 0 ){` |
|      - | 2764 | `			/* Copy buffered data */` |
|   1777 | 2765 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|   1777 | 2766 | `			rc = PH7_OK;` |
|    886 | 2767 | `		}` |
|      - | 2768 | `	}` |
|      - | 2769 | `	/* EOF or not */` |
|  10349 | 2770 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|  10349 | 2771 | `	return PH7_OK;` |
|   5177 | 2772 | `}` |
|      - | 2773 | `/*` |
|      - | 2774 | ` * Read n bytes from the underlying IO stream device.` |
|      - | 2775 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|      - | 2776 | ` * [i.e: IO error ] or EOF.` |
|      - | 2777 | ` */` |
|     18 | 2778 | `static ph7_int64 StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      2 | 2779 | `{` |
|     20 | 2780 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|     20 | 2781 | `	char *zBuf = (char *)pBuf;` |
|      - | 2782 | `	ph7_int64 n,nRead;` |
|     20 | 2783 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     20 | 2784 | `	if( n > 0 ){` |
|    ! 0 | 2785 | `		if( n > nLen ){` |
|    ! 0 | 2786 | `			n = nLen;` |
|    ! 0 | 2787 | `		}` |
|      - | 2788 | `		/* Copy the buffered data */` |
|    ! 0 | 2789 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|      - | 2790 | `		/* Update the read offset */` |
|    ! 0 | 2791 | `		pDev->nOfft += (sxu32)n;` |
|    ! 0 | 2792 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2793 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|    ! 0 | 2794 | `			SyBlobReset(&pDev->sBuffer);` |
|    ! 0 | 2795 | `			pDev->nOfft = 0;` |
|    ! 0 | 2796 | `		}` |
|    ! 0 | 2797 | `		nLen -= n;` |
|    ! 0 | 2798 | `		if( nLen < 1 ){` |
|      - | 2799 | `			/* All done */` |
|    ! 0 | 2800 | `			return n;` |
|      - | 2801 | `		}` |
|      - | 2802 | `		/* Advance the cursor */` |
|    ! 0 | 2803 | `		zBuf += n;` |
|    ! 0 | 2804 | `	}` |
|      - | 2805 | `	/* Read without buffering */` |
|     20 | 2806 | `	nRead = pStream->xRead(pDev->pHandle,zBuf,nLen);` |
|     20 | 2807 | `	if( nRead > 0 ){` |
|     18 | 2808 | `		n += nRead;` |
|     11 | 2809 | `	}else if( n < 1 ){` |
|      - | 2810 | `		/* EOF or IO error */` |
|      3 | 2811 | `		return nRead;` |
|      - | 2812 | `	}` |
|     18 | 2813 | `	return n;` |
|     11 | 2814 | `}` |
|      - | 2815 | `/*` |
|      - | 2816 | ` * Extract a single line from the buffered input.` |
|      - | 2817 | ` */` |
|   6538 | 2818 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      5 | 2819 | `{` |
|      - | 2820 | `	const char *zIn,*zEnd,*zPtr;` |
|   6543 | 2821 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|   6543 | 2822 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|   6543 | 2823 | `	zPtr = zIn;` |
| 381746 | 2824 | `	while( zIn < zEnd ){` |
| 381664 | 2825 | `		if( zIn[0] == '\n' ){` |
|      - | 2826 | `			/* Line found */` |
|   6461 | 2827 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|   6461 | 2828 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|   6461 | 2829 | `			*pzLine = zPtr;` |
|   6461 | 2830 | `			return SXRET_OK;` |
|      - | 2831 | `		}` |
| 375208 | 2832 | `		zIn++;` |
|      5 | 2833 | `	}` |
|      - | 2834 | `	/* No line were found */` |
|     87 | 2835 | `	return SXERR_NOTFOUND;` |
|   3274 | 2836 | `}` |
|      - | 2837 | `/*` |
|      - | 2838 | ` * Read a single line from the underlying IO stream device.` |
|      - | 2839 | ` */` |
|   6542 | 2840 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      5 | 2841 | `{` |
|   6547 | 2842 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - | 2843 | `	char zBuf[8192];` |
|      - | 2844 | `	ph7_int64 n;` |
|      - | 2845 | `	sxi32 rc;` |
|   6547 | 2846 | `	n = 0;` |
|   6547 | 2847 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2848 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     65 | 2849 | `		SyBlobReset(&pDev->sBuffer);` |
|     65 | 2850 | `		pDev->nOfft = 0;` |
|     30 | 2851 | `	}` |
|   6547 | 2852 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2853 | `		/* Check if there is a line */` |
|   6487 | 2854 | `		rc = GetLine(pDev,&n,pzData);` |
|   6487 | 2855 | `		if( rc == SXRET_OK ){` |
|      - | 2856 | `			/* Got line,update the cursor  */` |
|   6407 | 2857 | `			pDev->nOfft += (sxu32)n;` |
|   6407 | 2858 | `			return n;` |
|      - | 2859 | `		}` |
|     40 | 2860 | `	}` |
|      - | 2861 | `	/* Perform the read operation until a new line is extracted or length` |
|      - | 2862 | `	 * limit is reached.` |
|      - | 2863 | `	 */` |
|     71 | 2864 | `	for(;;){` |
|    147 | 2865 | `		n = pStream->xRead(pDev->pHandle,zBuf, (nMaxLen > 0 && nMaxLen < (ph7_int64)sizeof(zBuf)) ? nMaxLen : (ph7_int64)sizeof(zBuf));` |
|    147 | 2866 | `		if( n < 1 ){` |
|      - | 2867 | `			/* EOF or IO error */` |
|     91 | 2868 | `			break;` |
|      - | 2869 | `		}` |
|      - | 2870 | `		/* Append the data just read */` |
|     59 | 2871 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - | 2872 | `		/* Try to extract a line */` |
|     59 | 2873 | `		rc = GetLine(pDev,&n,pzData);` |
|     59 | 2874 | `		if( rc == SXRET_OK ){` |
|      - | 2875 | `			/* Got one,return immediately */` |
|     57 | 2876 | `			pDev->nOfft += (sxu32)n;` |
|     57 | 2877 | `			return n;` |
|      - | 2878 | `		}` |
|      3 | 2879 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
|      - | 2880 | `			/* Read limit reached,return the available data */` |
|    ! 0 | 2881 | `			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    ! 0 | 2882 | `			n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 2883 | `			/* Reset the working buffer */` |
|    ! 0 | 2884 | `			SyBlobReset(&pDev->sBuffer);` |
|    ! 0 | 2885 | `			pDev->nOfft = 0;` |
|    ! 0 | 2886 | `			return n;` |
|      - | 2887 | `		}` |
|      1 | 2888 | `	}` |
|     91 | 2889 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2890 | `		/* Read limit reached,return the available data */` |
|     87 | 2891 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|     87 | 2892 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 2893 | `		/* Reset the working buffer */` |
|     87 | 2894 | `		SyBlobReset(&pDev->sBuffer);` |
|     87 | 2895 | `		pDev->nOfft = 0;` |
|     41 | 2896 | `	}` |
|     91 | 2897 | `	return n;` |
|   3276 | 2898 | `}` |
|      - | 2899 | `/*` |
|      - | 2900 | ` * Open an IO stream handle.` |
|      - | 2901 | ` * Notes on stream:` |
|      - | 2902 | ` * According to the PHP reference manual.` |
|      - | 2903 | ` * In its simplest definition, a stream is a resource object which exhibits streamable behavior.` |
|      - | 2904 | ` * That is, it can be read from or written to in a linear fashion, and may be able to fseek()` |
|      - | 2905 | ` * to an arbitrary locations within the stream.` |
|      - | 2906 | ` * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.` |
|      - | 2907 | ` * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file` |
|      - | 2908 | ` * on a remote server.` |
|      - | 2909 | ` * A stream is referenced as: scheme://target` |
|      - | 2910 | ` *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...` |
|      - | 2911 | ` *   If no wrapper is specified, the function default is used (typically file://).` |
|      - | 2912 | ` *   target - Depends on the wrapper used. For filesystem related streams this is typically a path` |
|      - | 2913 | ` *  and filename of the desired file. For network related streams this is typically a hostname, often` |
|      - | 2914 | ` *  with a path appended.` |
|      - | 2915 | ` *` |
|      - | 2916 | ` * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.` |
|      - | 2917 | ` * Please refer to the official documentation for a full discussion.` |
|      - | 2918 | ` * This function return a handle on success. Otherwise null.` |
|      - | 2919 | ` */` |
|  29694 | 2920 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - | 2921 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      5 | 2922 | `{` |
|  29699 | 2923 | `	void *pHandle = 0; /* cc warning */` |
|      - | 2924 | `	SyString sFile;` |
|      - | 2925 | `	int rc;` |
|  29699 | 2926 | `	if( pStream == 0 ){` |
|      - | 2927 | `		/* No such stream device */` |
|    ! 0 | 2928 | `		return 0;` |
|      - | 2929 | `	}` |
|  29699 | 2930 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  29699 | 2931 | `	if( use_include ){` |
|   9620 | 2932 | `		if(	sFile.zString[0] == '/' \|\|` |
|      - | 2933 | `#ifdef __WINNT__` |
|      - | 2934 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - | 2935 | `#endif` |
|   9606 | 2936 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   9602 | 2937 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - | 2938 | `				/*  Open the file directly */` |
|     19 | 2939 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|     10 | 2940 | `		}else{` |
|      - | 2941 | `			SyString *pPath;` |
|      - | 2942 | `			SyBlob sWorker;` |
|      - | 2943 | `#ifdef __WINNT__` |
|      - | 2944 | `			static const int c = '\\';` |
|      - | 2945 | `#else` |
|      - | 2946 | `			static const int c = '/';` |
|      - | 2947 | `#endif` |
|      - | 2948 | `			/* Init the path builder working buffer */` |
|   9606 | 2949 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - | 2950 | `			/* Build a path from the set of include path */` |
|   9606 | 2951 | `			SySetResetCursor(&pVm->aPaths);` |
|   9606 | 2952 | `			rc = SXERR_IO;` |
|   9612 | 2953 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - | 2954 | `				/* Build full path */` |
|   9606 | 2955 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - | 2956 | `				/* Append null terminator */` |
|   9606 | 2957 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 2958 | `					continue;` |
|      - | 2959 | `				}` |
|      - | 2960 | `				/* Try to open the file */` |
|   9606 | 2961 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   9606 | 2962 | `				if( rc == PH7_OK ){` |
|   9599 | 2963 | `					if( bPushInclude ){` |
|      - | 2964 | `						/* Mark as included */` |
|   9599 | 2965 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   4798 | 2966 | `					}` |
|   9599 | 2967 | `					break;` |
|      - | 2968 | `				}` |
|      - | 2969 | `				/* Reset the working buffer */` |
|      8 | 2970 | `				SyBlobReset(&sWorker);` |
|      - | 2971 | `				/* Check the next path */` |
|      2 | 2972 | `			}` |
|   9606 | 2973 | `			SyBlobRelease(&sWorker);` |
|      - | 2974 | `		}` |
|   9624 | 2975 | `		if( rc == PH7_OK ){` |
|   9617 | 2976 | `			if( bPushInclude ){` |
|      - | 2977 | `				/* Mark as included */` |
|   9617 | 2978 | `				PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|   4807 | 2979 | `			}` |
|   4807 | 2980 | `		}` |
|   4814 | 2981 | `	}else{` |
|      - | 2982 | `		/* Open the URI direcly */` |
|  20079 | 2983 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - | 2984 | `	}` |
|  29699 | 2985 | `	if( rc != PH7_OK ){` |
|      - | 2986 | `		/* IO error */` |
|     15 | 2987 | `		return 0;` |
|      - | 2988 | `	}` |
|      - | 2989 | `	/* Return the file handle */` |
|  29687 | 2990 | `	return pHandle;` |
|  14852 | 2991 | `}` |
|      - | 2992 | `/*` |
|      - | 2993 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - | 2994 | ` * Store the read data in the given BLOB (last argument).` |
|      - | 2995 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - | 2996 | ` */` |
|   9608 | 2997 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      3 | 2998 | `{` |
|      - | 2999 | `	ph7_int64 nRead;` |
|      - | 3000 | `	char zBuf[8192]; /* 8K */` |
|      - | 3001 | `	int rc;` |
|      - | 3002 | `	/* Perform the requested operation */` |
|   9608 | 3003 | `	for(;;){` |
|  19219 | 3004 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  19219 | 3005 | `		if( nRead < 1 ){` |
|      - | 3006 | `			/* EOF or IO error */` |
|   9611 | 3007 | `			break;` |
|      - | 3008 | `		}` |
|      - | 3009 | `		/* Append contents */` |
|   9611 | 3010 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   9611 | 3011 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3012 | `			break;` |
|      - | 3013 | `		}` |
|      3 | 3014 | `	}` |
|   9611 | 3015 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      3 | 3016 | `}` |
|      - | 3017 | `/*` |
|      - | 3018 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - | 3019 | ` */` |
|  29774 | 3020 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      5 | 3021 | `{` |
|  29779 | 3022 | `	if( pStream->xClose ){` |
|  29779 | 3023 | `		pStream->xClose(pHandle);` |
|  14887 | 3024 | `	}` |
|  29779 | 3025 | `}` |
|      - | 3026 | `/*` |
|      - | 3027 | ` * string fgetc(resource $handle)` |
|      - | 3028 | ` *  Gets a character from the given file pointer.` |
|      - | 3029 | ` * Parameters` |
|      - | 3030 | ` *  $handle` |
|      - | 3031 | ` *   The file pointer.` |
|      - | 3032 | ` * Return` |
|      - | 3033 | ` *  Returns a string containing a single character read from the file` |
|      - | 3034 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|      - | 3035 | ` * WARNING` |
|      - | 3036 | ` *  This operation is extremely slow.Avoid using it.` |
|      - | 3037 | ` */` |
|      4 | 3038 | `static int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3039 | `{` |
|      - | 3040 | `	const ph7_io_stream *pStream;` |
|      - | 3041 | `	io_private *pDev;` |
|      - | 3042 | `	int c,n;` |
|      5 | 3043 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3044 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3045 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3046 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3047 | `		return PH7_OK;` |
|      - | 3048 | `	}` |
|      - | 3049 | `	/* Extract our private data */` |
|      5 | 3050 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3051 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 3052 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3053 | `		/*Expecting an IO handle */` |
|    ! 0 | 3054 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3055 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3056 | `		return PH7_OK;` |
|      - | 3057 | `	}` |
|      - | 3058 | `	/* Point to the target IO stream device */` |
|      5 | 3059 | `	pStream = pDev->pStream;` |
|      5 | 3060 | `	if( pStream == 0  ){` |
|    ! 0 | 3061 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3062 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3063 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3064 | `			);` |
|    ! 0 | 3065 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3066 | `		return PH7_OK;` |
|      - | 3067 | `	}` |
|      - | 3068 | `	/* Perform the requested operation */` |
|      5 | 3069 | `	n = (int)StreamRead(pDev,(void *)&c,sizeof(char));` |
|      - | 3070 | `	/* IO result */` |
|      5 | 3071 | `	if( n < 1 ){` |
|      - | 3072 | `		/* EOF or error,return FALSE */` |
|    ! 0 | 3073 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3074 | `	}else{` |
|      - | 3075 | `		/* Return the string holding the character */` |
|      5 | 3076 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|      - | 3077 | `	}` |
|      5 | 3078 | `	return PH7_OK;` |
|      3 | 3079 | `}` |
|      - | 3080 | `/*` |
|      - | 3081 | ` * string fgets(resource $handle[,int64 $length ])` |
|      - | 3082 | ` *  Gets line from file pointer.` |
|      - | 3083 | ` * Parameters` |
|      - | 3084 | ` *  $handle` |
|      - | 3085 | ` *   The file pointer.` |
|      - | 3086 | ` * $length` |
|      - | 3087 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3088 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3089 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3090 | ` *  the end of the line.` |
|      - | 3091 | ` * Return` |
|      - | 3092 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3093 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3094 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3095 | ` */` |
|   6532 | 3096 | `static int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3097 | `{` |
|      - | 3098 | `	const ph7_io_stream *pStream;` |
|      - | 3099 | `	const char *zLine;` |
|      - | 3100 | `	io_private *pDev;` |
|      - | 3101 | `	ph7_int64 n,nLen;` |
|   6537 | 3102 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3103 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3104 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3105 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3106 | `		return PH7_OK;` |
|      - | 3107 | `	}` |
|      - | 3108 | `	/* Extract our private data */` |
|   6537 | 3109 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3110 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   6537 | 3111 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3112 | `		/*Expecting an IO handle */` |
|    ! 0 | 3113 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3114 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3115 | `		return PH7_OK;` |
|      - | 3116 | `	}` |
|      - | 3117 | `	/* Point to the target IO stream device */` |
|   6537 | 3118 | `	pStream = pDev->pStream;` |
|   6537 | 3119 | `	if( pStream == 0  ){` |
|    ! 0 | 3120 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3121 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3122 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3123 | `			);` |
|    ! 0 | 3124 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3125 | `		return PH7_OK;` |
|      - | 3126 | `	}` |
|   6537 | 3127 | `	nLen = -1;` |
|   6537 | 3128 | `	if( nArg > 1 ){` |
|      - | 3129 | `		/* Maximum data to read */` |
|    ! 0 | 3130 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3131 | `	}` |
|      - | 3132 | `	/* Perform the requested operation */` |
|   6537 | 3133 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|   6537 | 3134 | `	if( n < 1 ){` |
|      - | 3135 | `		/* EOF or IO error,return FALSE */` |
|      7 | 3136 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3137 | `	}else{` |
|      - | 3138 | `		/* Return the freshly extracted line */` |
|   6535 | 3139 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - | 3140 | `	}` |
|   6537 | 3141 | `	return PH7_OK;` |
|   3271 | 3142 | `}` |
|      - | 3143 | `/*` |
|      - | 3144 | ` * string fread(resource $handle,int64 $length)` |
|      - | 3145 | ` *  Binary-safe file read.` |
|      - | 3146 | ` * Parameters` |
|      - | 3147 | ` *  $handle` |
|      - | 3148 | ` *   The file pointer.` |
|      - | 3149 | ` * $length` |
|      - | 3150 | ` *  Up to length number of bytes read.` |
|      - | 3151 | ` * Return` |
|      - | 3152 | ` *  The data readen on success or FALSE on failure.` |
|      - | 3153 | ` */` |
|     10 | 3154 | `static int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3155 | `{` |
|      - | 3156 | `	const ph7_io_stream *pStream;` |
|      - | 3157 | `	io_private *pDev;` |
|      - | 3158 | `	ph7_int64 nRead;` |
|      - | 3159 | `	void *pBuf;` |
|      - | 3160 | `	int nLen;` |
|     12 | 3161 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3162 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3163 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3164 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3165 | `		return PH7_OK;` |
|      - | 3166 | `	}` |
|      - | 3167 | `	/* Extract our private data */` |
|     12 | 3168 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3169 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     12 | 3170 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3171 | `		/*Expecting an IO handle */` |
|    ! 0 | 3172 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3173 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3174 | `		return PH7_OK;` |
|      - | 3175 | `	}` |
|      - | 3176 | `	/* Point to the target IO stream device */` |
|     12 | 3177 | `	pStream = pDev->pStream;` |
|     12 | 3178 | `	if( pStream == 0  ){` |
|    ! 0 | 3179 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3180 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3181 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3182 | `			);` |
|    ! 0 | 3183 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3184 | `		return PH7_OK;` |
|      - | 3185 | `	}` |
|     12 | 3186 | `        nLen = 4096;` |
|     12 | 3187 | `	if( nArg > 1 ){` |
|     12 | 3188 | ` 	  nLen = ph7_value_to_int(apArg[1]);` |
|     12 | 3189 | `	  if( nLen < 1 ){` |
|      - | 3190 | `		/* Invalid length,set a default length */` |
|    ! 0 | 3191 | `		nLen = 4096;` |
|    ! 0 | 3192 | `	  }` |
|      5 | 3193 | `        }` |
|      - | 3194 | `	/* Allocate enough buffer */` |
|     12 | 3195 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     12 | 3196 | `	if( pBuf == 0 ){` |
|    ! 0 | 3197 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3198 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3199 | `		return PH7_OK;` |
|      - | 3200 | `	}` |
|      - | 3201 | `	/* Perform the requested operation */` |
|     12 | 3202 | `	nRead = StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     12 | 3203 | `	if( nRead < 1 ){` |
|      - | 3204 | `		/* Nothing read,return FALSE */` |
|    ! 0 | 3205 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3206 | `	}else{` |
|      - | 3207 | `		/* Make a copy of the data just read */` |
|     12 | 3208 | `		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);` |
|      - | 3209 | `	}` |
|      - | 3210 | `	/* Release the buffer */` |
|     12 | 3211 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     12 | 3212 | `	return PH7_OK;` |
|      7 | 3213 | `}` |
|      - | 3214 | `/*` |
|      - | 3215 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|      - | 3216 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|      - | 3217 | ` * Gets line from file pointer and parse for CSV fields.` |
|      - | 3218 | ` * Parameters` |
|      - | 3219 | ` * $handle` |
|      - | 3220 | ` *   The file pointer.` |
|      - | 3221 | ` * $length` |
|      - | 3222 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3223 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3224 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3225 | ` *  the end of the line.` |
|      - | 3226 | ` * $delimiter` |
|      - | 3227 | ` *   Set the field delimiter (one character only).` |
|      - | 3228 | ` * $enclosure` |
|      - | 3229 | ` *   Set the field enclosure character (one character only).` |
|      - | 3230 | ` * $escape` |
|      - | 3231 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 3232 | ` * Return` |
|      - | 3233 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3234 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3235 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3236 | ` */` |
|      2 | 3237 | `static int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3238 | `{` |
|      - | 3239 | `	const ph7_io_stream *pStream;` |
|      - | 3240 | `	const char *zLine;` |
|      - | 3241 | `	io_private *pDev;` |
|      - | 3242 | `	ph7_int64 n,nLen;` |
|      3 | 3243 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3244 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3245 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3246 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3247 | `		return PH7_OK;` |
|      - | 3248 | `	}` |
|      - | 3249 | `	/* Extract our private data */` |
|      3 | 3250 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3251 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3252 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3253 | `		/*Expecting an IO handle */` |
|    ! 0 | 3254 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3255 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3256 | `		return PH7_OK;` |
|      - | 3257 | `	}` |
|      - | 3258 | `	/* Point to the target IO stream device */` |
|      3 | 3259 | `	pStream = pDev->pStream;` |
|      3 | 3260 | `	if( pStream == 0  ){` |
|    ! 0 | 3261 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3262 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3263 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3264 | `			);` |
|    ! 0 | 3265 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3266 | `		return PH7_OK;` |
|      - | 3267 | `	}` |
|      3 | 3268 | `	nLen = -1;` |
|      3 | 3269 | `	if( nArg > 1 ){` |
|      - | 3270 | `		/* Maximum data to read */` |
|      3 | 3271 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      1 | 3272 | `	}` |
|      - | 3273 | `	/* Perform the requested operation */` |
|      3 | 3274 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3275 | `	if( n < 1 ){` |
|      - | 3276 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3277 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3278 | `	}else{` |
|      - | 3279 | `		ph7_value *pArray;` |
|      3 | 3280 | `		int delim  = ',';   /* Delimiter */` |
|      3 | 3281 | `		int encl   = '"' ;  /* Enclosure */` |
|      3 | 3282 | `		int escape = '\\';  /* Escape character */` |
|      3 | 3283 | `		if( nArg > 2 ){` |
|      - | 3284 | `			const char *zPtr;` |
|      - | 3285 | `			int i;` |
|      3 | 3286 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 3287 | `				/* Extract the delimiter */` |
|      3 | 3288 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 3289 | `				if( i > 0 ){` |
|      3 | 3290 | `					delim = zPtr[0];` |
|      1 | 3291 | `				}` |
|      1 | 3292 | `			}` |
|      3 | 3293 | `			if( nArg > 3 ){` |
|      3 | 3294 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 3295 | `					/* Extract the enclosure */` |
|      3 | 3296 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 3297 | `					if( i > 0 ){` |
|      3 | 3298 | `						encl = zPtr[0];` |
|      1 | 3299 | `					}` |
|      1 | 3300 | `				}` |
|      3 | 3301 | `				if( nArg > 4 ){` |
|      3 | 3302 | `					if( ph7_value_is_string(apArg[4]) ){` |
|      - | 3303 | `						/* Extract the escape character */` |
|      3 | 3304 | `						zPtr = ph7_value_to_string(apArg[4],&i);` |
|      3 | 3305 | `						if( i > 0 ){` |
|      3 | 3306 | `							escape = zPtr[0];` |
|      1 | 3307 | `						}` |
|      1 | 3308 | `					}` |
|      1 | 3309 | `				}` |
|      1 | 3310 | `			}` |
|      1 | 3311 | `		}` |
|      - | 3312 | `		/* Create our array */` |
|      3 | 3313 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 3314 | `		if( pArray == 0 ){` |
|    ! 0 | 3315 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3316 | `			ph7_result_null(pCtx);` |
|    ! 0 | 3317 | `			return PH7_OK;` |
|      - | 3318 | `		}` |
|      - | 3319 | `		/* Parse the raw input */` |
|      3 | 3320 | `		PH7_ProcessCsv(zLine,(int)n,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 3321 | `		/* Return the freshly created array  */` |
|      3 | 3322 | `		ph7_result_value(pCtx,pArray);` |
|      - | 3323 | `	}` |
|      3 | 3324 | `	return PH7_OK;` |
|      2 | 3325 | `}` |
|      - | 3326 | `/*` |
|      - | 3327 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|      - | 3328 | ` *  Gets line from file pointer and strip HTML tags.` |
|      - | 3329 | ` * Parameters` |
|      - | 3330 | ` * $handle` |
|      - | 3331 | ` *   The file pointer.` |
|      - | 3332 | ` * $length` |
|      - | 3333 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3334 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3335 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3336 | ` *  the end of the line.` |
|      - | 3337 | ` * $allowable_tags` |
|      - | 3338 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 3339 | ` * Return` |
|      - | 3340 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|      - | 3341 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|      - | 3342 | ` */` |
|      2 | 3343 | `static int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3344 | `{` |
|      - | 3345 | `	const ph7_io_stream *pStream;` |
|      - | 3346 | `	const char *zLine;` |
|      - | 3347 | `	io_private *pDev;` |
|      - | 3348 | `	ph7_int64 n,nLen;` |
|      3 | 3349 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3350 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3351 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3352 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3353 | `		return PH7_OK;` |
|      - | 3354 | `	}` |
|      - | 3355 | `	/* Extract our private data */` |
|      3 | 3356 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3357 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3358 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3359 | `		/*Expecting an IO handle */` |
|    ! 0 | 3360 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3361 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3362 | `		return PH7_OK;` |
|      - | 3363 | `	}` |
|      - | 3364 | `	/* Point to the target IO stream device */` |
|      3 | 3365 | `	pStream = pDev->pStream;` |
|      3 | 3366 | `	if( pStream == 0  ){` |
|    ! 0 | 3367 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3368 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3369 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3370 | `			);` |
|    ! 0 | 3371 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3372 | `		return PH7_OK;` |
|      - | 3373 | `	}` |
|      3 | 3374 | `	nLen = -1;` |
|      3 | 3375 | `	if( nArg > 1 ){` |
|      - | 3376 | `		/* Maximum data to read */` |
|    ! 0 | 3377 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3378 | `	}` |
|      - | 3379 | `	/* Perform the requested operation */` |
|      3 | 3380 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3381 | `	if( n < 1 ){` |
|      - | 3382 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3383 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3384 | `	}else{` |
|      3 | 3385 | `		const char *zTaglist = 0;` |
|      3 | 3386 | `		int nTaglen = 0;` |
|      3 | 3387 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|      - | 3388 | `			/* Allowed tag */` |
|    ! 0 | 3389 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|    ! 0 | 3390 | `		}` |
|      - | 3391 | `		/* Process data just read */` |
|      3 | 3392 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen);` |
|      - | 3393 | `	}` |
|      3 | 3394 | `	return PH7_OK;` |
|      2 | 3395 | `}` |
|      - | 3396 | `/*` |
|      - | 3397 | ` * string readdir(resource $dir_handle)` |
|      - | 3398 | ` *   Read entry from directory handle.` |
|      - | 3399 | ` * Parameter` |
|      - | 3400 | ` *  $dir_handle` |
|      - | 3401 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3402 | ` * Return` |
|      - | 3403 | ` *  Returns the filename on success or FALSE on failure.` |
|      - | 3404 | ` */` |
|   8298 | 3405 | `static int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3406 | `{` |
|      - | 3407 | `	const ph7_io_stream *pStream;` |
|      - | 3408 | `	io_private *pDev;` |
|      - | 3409 | `	int rc;` |
|   8303 | 3410 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3411 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3412 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3413 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3414 | `		return PH7_OK;` |
|      - | 3415 | `	}` |
|      - | 3416 | `	/* Extract our private data */` |
|   8303 | 3417 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3418 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   8303 | 3419 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3420 | `		/*Expecting an IO handle */` |
|    ! 0 | 3421 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3422 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3423 | `		return PH7_OK;` |
|      - | 3424 | `	}` |
|      - | 3425 | `	/* Point to the target IO stream device */` |
|   8303 | 3426 | `	pStream = pDev->pStream;` |
|   8303 | 3427 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 3428 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3429 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3430 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3431 | `			);` |
|    ! 0 | 3432 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3433 | `		return PH7_OK;` |
|      - | 3434 | `	}` |
|   8303 | 3435 | `	ph7_result_bool(pCtx,0);` |
|      - | 3436 | `	/* Perform the requested operation */` |
|   8303 | 3437 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|   8303 | 3438 | `	if( rc != PH7_OK ){` |
|      - | 3439 | `		/* Return FALSE */` |
|    979 | 3440 | `		ph7_result_bool(pCtx,0);` |
|    487 | 3441 | `	}` |
|   8303 | 3442 | `	return PH7_OK;` |
|   4154 | 3443 | `}` |
|      - | 3444 | `/*` |
|      - | 3445 | ` * void rewinddir(resource $dir_handle)` |
|      - | 3446 | ` *   Rewind directory handle.` |
|      - | 3447 | ` * Parameter` |
|      - | 3448 | ` *  $dir_handle` |
|      - | 3449 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3450 | ` * Return` |
|      - | 3451 | ` *  FALSE on failure.` |
|      - | 3452 | ` */` |
|      2 | 3453 | `static int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3454 | `{` |
|      - | 3455 | `	const ph7_io_stream *pStream;` |
|      - | 3456 | `	io_private *pDev;` |
|      3 | 3457 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3458 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3459 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3460 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3461 | `		return PH7_OK;` |
|      - | 3462 | `	}` |
|      - | 3463 | `	/* Extract our private data */` |
|      3 | 3464 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3465 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3466 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3467 | `		/*Expecting an IO handle */` |
|    ! 0 | 3468 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3469 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3470 | `		return PH7_OK;` |
|      - | 3471 | `	}` |
|      - | 3472 | `	/* Point to the target IO stream device */` |
|      3 | 3473 | `	pStream = pDev->pStream;` |
|      3 | 3474 | `	if( pStream == 0  \|\| pStream->xRewindDir == 0 ){` |
|    ! 0 | 3475 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3476 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3477 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3478 | `			);` |
|    ! 0 | 3479 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3480 | `		return PH7_OK;` |
|      - | 3481 | `	}` |
|      - | 3482 | `	/* Perform the requested operation */` |
|      3 | 3483 | `	pStream->xRewindDir(pDev->pHandle);` |
|      3 | 3484 | `	return PH7_OK;` |
|      2 | 3485 | ` }` |
|      - | 3486 | `/* Forward declaration */` |
|      - | 3487 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut);` |
|      - | 3488 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|      - | 3489 | `/*` |
|      - | 3490 | ` * void closedir(resource $dir_handle)` |
|      - | 3491 | ` *   Close directory handle.` |
|      - | 3492 | ` * Parameter` |
|      - | 3493 | ` *  $dir_handle` |
|      - | 3494 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3495 | ` * Return` |
|      - | 3496 | ` *  FALSE on failure.` |
|      - | 3497 | ` */` |
|    978 | 3498 | `static int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3499 | `{` |
|      - | 3500 | `	const ph7_io_stream *pStream;` |
|      - | 3501 | `	io_private *pDev;` |
|    983 | 3502 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3503 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3504 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3505 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3506 | `		return PH7_OK;` |
|      - | 3507 | `	}` |
|      - | 3508 | `	/* Extract our private data */` |
|    983 | 3509 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3510 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    983 | 3511 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3512 | `		/*Expecting an IO handle */` |
|    ! 0 | 3513 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3514 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3515 | `		return PH7_OK;` |
|      - | 3516 | `	}` |
|      - | 3517 | `	/* Point to the target IO stream device */` |
|    983 | 3518 | `	pStream = pDev->pStream;` |
|    983 | 3519 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 3520 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3521 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3522 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3523 | `			);` |
|    ! 0 | 3524 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3525 | `		return PH7_OK;` |
|      - | 3526 | `	}` |
|      - | 3527 | `	/* Perform the requested operation */` |
|    983 | 3528 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 3529 | `	/* Release the private stucture */` |
|    983 | 3530 | `	ReleaseIOPrivate(pCtx,pDev);` |
|    983 | 3531 | `	PH7_MemObjRelease(apArg[0]);` |
|    983 | 3532 | `	return PH7_OK;` |
|    494 | 3533 | ` }` |
|      - | 3534 | `/*` |
|      - | 3535 | ` * resource opendir(string $path[,resource $context])` |
|      - | 3536 | ` *  Open directory handle.` |
|      - | 3537 | ` * Parameters` |
|      - | 3538 | ` * $path` |
|      - | 3539 | ` *   The directory path that is to be opened.` |
|      - | 3540 | ` * $context` |
|      - | 3541 | ` *   A context stream resource.` |
|      - | 3542 | ` * Return` |
|      - | 3543 | ` *  A directory handle resource on success,or FALSE on failure.` |
|      - | 3544 | ` */` |
|    978 | 3545 | `static int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3546 | `{` |
|      - | 3547 | `	const ph7_io_stream *pStream;` |
|      - | 3548 | `	const char *zPath;` |
|      - | 3549 | `	io_private *pDev;` |
|      - | 3550 | `	int iLen,rc;` |
|    983 | 3551 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3552 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3553 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 3554 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3555 | `		return PH7_OK;` |
|      - | 3556 | `	}` |
|      - | 3557 | `	/* Extract the target path */` |
|    983 | 3558 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 3559 | `	/* Try to extract a stream */` |
|    983 | 3560 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|    983 | 3561 | `	if( pStream == 0 ){` |
|    ! 0 | 3562 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3563 | `			"No stream device is associated with the given path(%s)",zPath);` |
|    ! 0 | 3564 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3565 | `		return PH7_OK;` |
|      - | 3566 | `	}` |
|    983 | 3567 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 3568 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3569 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 3570 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 3571 | `			);` |
|    ! 0 | 3572 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3573 | `		return PH7_OK;` |
|      - | 3574 | `	}` |
|      - | 3575 | `	/* Allocate a new IO private instance */` |
|    983 | 3576 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    983 | 3577 | `	if( pDev == 0 ){` |
|    ! 0 | 3578 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3579 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3580 | `		return PH7_OK;` |
|      - | 3581 | `	}` |
|      - | 3582 | `	/* Initialize the structure */` |
|    983 | 3583 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 3584 | `	/* Open the target directory */` |
|    983 | 3585 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|    983 | 3586 | `	if( rc != PH7_OK ){` |
|      - | 3587 | `		/* IO error,return FALSE */` |
|    ! 0 | 3588 | `		ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 3589 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3590 | `	}else{` |
|      - | 3591 | `		/* Return the handle as a resource */` |
|    983 | 3592 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 3593 | `	}` |
|    983 | 3594 | `	return PH7_OK;` |
|    494 | 3595 | `}` |
|      - | 3596 | `/*` |
|      - | 3597 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|      - | 3598 | ` *  Reads a file and writes it to the output buffer.` |
|      - | 3599 | ` * Parameters` |
|      - | 3600 | ` *  $filename` |
|      - | 3601 | ` *   The filename being read.` |
|      - | 3602 | ` *  $use_include_path` |
|      - | 3603 | ` *   You can use the optional second parameter and set it to` |
|      - | 3604 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3605 | ` *  $context` |
|      - | 3606 | ` *   A context stream resource.` |
|      - | 3607 | ` * Return` |
|      - | 3608 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|      - | 3609 | ` */` |
|      2 | 3610 | `static int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3611 | `{` |
|      3 | 3612 | `	int use_include  = FALSE;` |
|      - | 3613 | `	const ph7_io_stream *pStream;` |
|      - | 3614 | `	ph7_int64 n,nRead;` |
|      - | 3615 | `	const char *zFile;` |
|      - | 3616 | `	char zBuf[8192];` |
|      - | 3617 | `	void *pHandle;` |
|      - | 3618 | `	int rc,nLen;` |
|      3 | 3619 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3620 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3621 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3622 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3623 | `		return PH7_OK;` |
|      - | 3624 | `	}` |
|      - | 3625 | `	/* Extract the file path */` |
|      3 | 3626 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3627 | `	/* Point to the target IO stream device */` |
|      3 | 3628 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3629 | `	if( pStream == 0 ){` |
|    ! 0 | 3630 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3631 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3632 | `		return PH7_OK;` |
|      - | 3633 | `	}` |
|      3 | 3634 | `	if( nArg > 1 ){` |
|    ! 0 | 3635 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3636 | `	}` |
|      - | 3637 | `	/* Try to open the file in read-only mode */` |
|      4 | 3638 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|      1 | 3639 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 3640 | `	if( pHandle == 0 ){` |
|    ! 0 | 3641 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3642 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3643 | `		return PH7_OK;` |
|      - | 3644 | `	}` |
|      - | 3645 | `	/* Perform the requested operation */` |
|      3 | 3646 | `	nRead = 0;` |
|      2 | 3647 | `	for(;;){` |
|      5 | 3648 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3649 | `		if( n < 1 ){` |
|      - | 3650 | `			/* EOF or IO error,break immediately */` |
|      3 | 3651 | `			break;` |
|      - | 3652 | `		}` |
|      - | 3653 | `		/* Output data */` |
|      3 | 3654 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      3 | 3655 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 3656 | `			break;` |
|      - | 3657 | `		}` |
|      - | 3658 | `		/* Increment counter */` |
|      3 | 3659 | `		nRead += n;` |
|      1 | 3660 | `	}` |
|      - | 3661 | `	/* Close the stream */` |
|      3 | 3662 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3663 | `	/* Total number of bytes readen */` |
|      3 | 3664 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 3665 | `	return PH7_OK;` |
|      2 | 3666 | `}` |
|      - | 3667 | `/*` |
|      - | 3668 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|      - | 3669 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|      - | 3670 | ` *  Reads entire file into a string.` |
|      - | 3671 | ` * Parameters` |
|      - | 3672 | ` *  $filename` |
|      - | 3673 | ` *   The filename being read.` |
|      - | 3674 | ` *  $use_include_path` |
|      - | 3675 | ` *   You can use the optional second parameter and set it to` |
|      - | 3676 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3677 | ` *  $context` |
|      - | 3678 | ` *   A context stream resource.` |
|      - | 3679 | ` *  $offset` |
|      - | 3680 | ` *   The offset where the reading starts on the original stream.` |
|      - | 3681 | ` *  $maxlen` |
|      - | 3682 | ` *    Maximum length of data read. The default is to read until end of file` |
|      - | 3683 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|      - | 3684 | ` * Return` |
|      - | 3685 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3686 | ` */` |
|   6382 | 3687 | `static int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3688 | `{` |
|      - | 3689 | `	const ph7_io_stream *pStream;` |
|      - | 3690 | `	ph7_int64 n,nRead,nMaxlen;` |
|   6387 | 3691 | `	int use_include  = FALSE;` |
|      - | 3692 | `	const char *zFile;` |
|      - | 3693 | `	char zBuf[8192];` |
|      - | 3694 | `	void *pHandle;` |
|      - | 3695 | `	int nLen;` |
|      - | 3696 |  |
|   6387 | 3697 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3698 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3699 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3700 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3701 | `		return PH7_OK;` |
|      - | 3702 | `	}` |
|      - | 3703 | `	/* Extract the file path */` |
|   6387 | 3704 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3705 | `	/* Point to the target IO stream device */` |
|   6387 | 3706 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   6387 | 3707 | `	if( pStream == 0 ){` |
|    ! 0 | 3708 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3709 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3710 | `		return PH7_OK;` |
|      - | 3711 | `	}` |
|   6387 | 3712 | `	nMaxlen = -1;` |
|   6387 | 3713 | `	if( nArg > 1 ){` |
|      5 | 3714 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|      2 | 3715 | `	}` |
|      - | 3716 | `	/* Try to open the file in read-only mode */` |
|   6387 | 3717 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|   6387 | 3718 | `	if( pHandle == 0 ){` |
|    ! 0 | 3719 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3720 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3721 | `		return PH7_OK;` |
|      - | 3722 | `	}` |
|   6387 | 3723 | `	if( nArg > 3 ){` |
|      - | 3724 | `		/* Extract the offset */` |
|      5 | 3725 | `		n = ph7_value_to_int64(apArg[3]);` |
|      5 | 3726 | `		if( n > 0 ){` |
|    ! 0 | 3727 | `			if( pStream->xSeek ){` |
|      - | 3728 | `				/* Seek to the desired offset */` |
|    ! 0 | 3729 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|    ! 0 | 3730 | `			}` |
|    ! 0 | 3731 | `		}` |
|      5 | 3732 | `		if( nArg > 4 ){` |
|      - | 3733 | `			/* Maximum data to read */` |
|      5 | 3734 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|      2 | 3735 | `		}` |
|      2 | 3736 | `	}` |
|      - | 3737 | `	/* Perform the requested operation */` |
|   6387 | 3738 | `	nRead = 0;` |
|   6380 | 3739 | `	for(;;){` |
|  19148 | 3740 | `		n = pStream->xRead(pHandle,zBuf,` |
|   6383 | 3741 | `			(nMaxlen > 0 && (nMaxlen < (ph7_int64)sizeof(zBuf))) ? nMaxlen : (ph7_int64)sizeof(zBuf));` |
|  12765 | 3742 | `		if( n < 1 ){` |
|      - | 3743 | `			/* EOF or IO error,break immediately */` |
|   6385 | 3744 | `			break;` |
|      - | 3745 | `		}` |
|      - | 3746 | `		/* Append data */` |
|   6385 | 3747 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 3748 | `		/* Increment read counter */` |
|   6385 | 3749 | `		nRead += n;` |
|   6385 | 3750 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 3751 | `			/* Read limit reached */` |
|      3 | 3752 | `			break;` |
|      - | 3753 | `		}` |
|      5 | 3754 | `	}` |
|      - | 3755 | `	/* Close the stream */` |
|   6387 | 3756 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3757 | `	/* Check if we have read something */` |
|   6387 | 3758 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 3759 | `		/* Nothing read,return FALSE */` |
|      3 | 3760 | `		ph7_result_bool(pCtx,0);` |
|      1 | 3761 | `	}` |
|   6387 | 3762 | `	return PH7_OK;` |
|   3196 | 3763 | `}` |
|      - | 3764 | `/*` |
|      - | 3765 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 3766 | ` *  Write a string to a file.` |
|      - | 3767 | ` * Parameters` |
|      - | 3768 | ` *  $filename` |
|      - | 3769 | ` *  Path to the file where to write the data.` |
|      - | 3770 | ` * $data` |
|      - | 3771 | ` *  The data to write(Must be a string).` |
|      - | 3772 | ` * $flags` |
|      - | 3773 | ` *  The value of flags can be any combination of the following` |
|      - | 3774 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 3775 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 3776 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 3777 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 3778 | ` * context` |
|      - | 3779 | ` *  A context stream resource.` |
|      - | 3780 | ` * Return` |
|      - | 3781 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 3782 | ` */` |
|  13608 | 3783 | `static int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3784 | `{` |
|  13613 | 3785 | `	int use_include  = FALSE;` |
|      - | 3786 | `	const ph7_io_stream *pStream;` |
|      - | 3787 | `	const char *zFile;` |
|      - | 3788 | `	const char *zData;` |
|      - | 3789 | `	int iOpenFlags;` |
|      - | 3790 | `	void *pHandle;` |
|      - | 3791 | `	int iFlags;` |
|      - | 3792 | `	int nLen;` |
|      - | 3793 |  |
|  13613 | 3794 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3795 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3796 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3797 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3798 | `		return PH7_OK;` |
|      - | 3799 | `	}` |
|      - | 3800 | `	/* Extract the file path */` |
|  13613 | 3801 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3802 | `	/* Point to the target IO stream device */` |
|  13613 | 3803 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  13613 | 3804 | `	if( pStream == 0 ){` |
|    ! 0 | 3805 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3806 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3807 | `		return PH7_OK;` |
|      - | 3808 | `	}` |
|      - | 3809 | `	/* Data to write */` |
|  13613 | 3810 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 3811 | `	/* Try to open the file in read-write mode */` |
|  13613 | 3812 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 3813 | `	/* Extract the flags */` |
|  13613 | 3814 | `	iFlags = 0;` |
|  13613 | 3815 | `	if( nArg > 2 ){` |
|    ! 0 | 3816 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3817 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 3818 | `			use_include = TRUE;` |
|    ! 0 | 3819 | `		}` |
|    ! 0 | 3820 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 3821 | `			/* If the file already exists, append the data to the file` |
|      - | 3822 | `			 * instead of overwriting it.` |
|      - | 3823 | `			 */` |
|    ! 0 | 3824 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 3825 | `			/* Append mode */` |
|    ! 0 | 3826 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 3827 | `		}` |
|    ! 0 | 3828 | `	}` |
|  20417 | 3829 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   6804 | 3830 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|  13613 | 3831 | `	if( pHandle == 0 ){` |
|    ! 0 | 3832 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3833 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3834 | `		return PH7_OK;` |
|      - | 3835 | `	}` |
|  13613 | 3836 | `	if( nLen < 1 ){` |
|      - | 3837 | `		/* Empty data, file is created/truncated */` |
|      7 | 3838 | `		ph7_result_int64(pCtx,0);` |
|      7 | 3839 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|      7 | 3840 | `		return PH7_OK;` |
|      - | 3841 | `	}` |
|  13607 | 3842 | `	if( pStream->xWrite ){` |
|      - | 3843 | `		ph7_int64 n;` |
|  13607 | 3844 | `		if( (iFlags & 0x01/* LOCK_EX */) && pStream->xLock ){` |
|      - | 3845 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 3846 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 3847 | `		}` |
|      - | 3848 | `		/* Perform the write operation */` |
|  13607 | 3849 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  13607 | 3850 | `		if( n < 0 ){` |
|      - | 3851 | `			/* IO error,return FALSE */` |
|    ! 0 | 3852 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3853 | `		}else{` |
|      - | 3854 | `			/* Total number of bytes written */` |
|  13607 | 3855 | `			ph7_result_int64(pCtx,n);` |
|      - | 3856 | `		}` |
|   6806 | 3857 | `	}else{` |
|      - | 3858 | `		/* Read-only stream */` |
|    ! 0 | 3859 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 3860 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 3861 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 3862 | `			);` |
|    ! 0 | 3863 | `		ph7_result_bool(pCtx,0);` |
|      - | 3864 | `	}` |
|      - | 3865 | `	/* Close the handle */` |
|  13607 | 3866 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  13607 | 3867 | `	return PH7_OK;` |
|   6809 | 3868 | `}` |
|      - | 3869 | `/*` |
|      - | 3870 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 3871 | ` *  Reads entire file into an array.` |
|      - | 3872 | ` * Parameters` |
|      - | 3873 | ` *  $filename` |
|      - | 3874 | ` *   The filename being read.` |
|      - | 3875 | ` *  $flags` |
|      - | 3876 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 3877 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 3878 | ` *       Search for the file in the include_path.` |
|      - | 3879 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 3880 | ` *       Do not add newline at the end of each array element` |
|      - | 3881 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 3882 | ` *       Skip empty lines` |
|      - | 3883 | ` *  $context` |
|      - | 3884 | ` *   A context stream resource.` |
|      - | 3885 | ` * Return` |
|      - | 3886 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3887 | ` */` |
|      6 | 3888 | `static int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3889 | `{` |
|      - | 3890 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 3891 | `	ph7_value *pArray,*pLine;` |
|      - | 3892 | `	const ph7_io_stream *pStream;` |
|      8 | 3893 | `	int use_include = 0;` |
|      - | 3894 | `	io_private *pDev;` |
|      - | 3895 | `	ph7_int64 n;` |
|      - | 3896 | `	int iFlags;` |
|      - | 3897 | `	int nLen;` |
|      - | 3898 |  |
|      8 | 3899 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3900 | `		/* Missing/Invalid arguments,return FALSE */` |
|      3 | 3901 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|      3 | 3902 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3903 | `		return PH7_OK;` |
|      - | 3904 | `	}` |
|      - | 3905 | `	/* Extract the file path */` |
|      6 | 3906 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3907 | `	/* Point to the target IO stream device */` |
|      6 | 3908 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 3909 | `	if( pStream == 0 ){` |
|    ! 0 | 3910 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3911 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3912 | `		return PH7_OK;` |
|      - | 3913 | `	}` |
|      - | 3914 | `	/* Allocate a new IO private instance */` |
|      6 | 3915 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      6 | 3916 | `	if( pDev == 0 ){` |
|    ! 0 | 3917 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3918 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3919 | `		return PH7_OK;` |
|      - | 3920 | `	}` |
|      - | 3921 | `	/* Initialize the structure */` |
|      6 | 3922 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      6 | 3923 | `	iFlags = 0;` |
|      6 | 3924 | `	if( nArg > 1 ){` |
|    ! 0 | 3925 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 3926 | `	}` |
|      6 | 3927 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|    ! 0 | 3928 | `		use_include = TRUE;` |
|    ! 0 | 3929 | `	}` |
|      - | 3930 | `	/* Create the array and the working value */` |
|      6 | 3931 | `	pArray = ph7_context_new_array(pCtx);` |
|      6 | 3932 | `	pLine = ph7_context_new_scalar(pCtx);` |
|      6 | 3933 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 3934 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3935 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3936 | `		return PH7_OK;` |
|      - | 3937 | `	}` |
|      - | 3938 | `	/* Try to open the file in read-only mode */` |
|      6 | 3939 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 3940 | `	if( pDev->pHandle == 0 ){` |
|      3 | 3941 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|      3 | 3942 | `		ph7_result_bool(pCtx,0);` |
|      - | 3943 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 3944 | `		 * as soon we return from this function.` |
|      - | 3945 | `		 */` |
|      3 | 3946 | `		return PH7_OK;` |
|      - | 3947 | `	}` |
|      - | 3948 | `	/* Perform the requested operation */` |
|      3 | 3949 | `	for(;;){` |
|      - | 3950 | `		/* Try to extract a line */` |
|      7 | 3951 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|      7 | 3952 | `		if( n < 1 ){` |
|      - | 3953 | `			/* EOF or IO error */` |
|      3 | 3954 | `			break;` |
|      - | 3955 | `		}` |
|      - | 3956 | `		/* Reset the cursor */` |
|      5 | 3957 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 3958 | `		/* Remove line ending if requested by the caller */` |
|      5 | 3959 | `		zPtr = zBuf;` |
|      5 | 3960 | `		zEnd = &zBuf[n];` |
|      5 | 3961 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 3962 | `			/* Ignore trailig lines */` |
|    ! 0 | 3963 | `			while( zPtr < zEnd && (zEnd[-1] == '\n'` |
|      - | 3964 | `#ifdef __WINNT__` |
|      - | 3965 | `				\|\| zEnd[-1] == '\r'` |
|      - | 3966 | `#endif` |
|      - | 3967 | `				)){` |
|    ! 0 | 3968 | `					n--;` |
|    ! 0 | 3969 | `					zEnd--;` |
|    ! 0 | 3970 | `			}` |
|    ! 0 | 3971 | `		}` |
|      5 | 3972 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 3973 | `			/* Ignore empty lines */` |
|    ! 0 | 3974 | `			while( zPtr < zEnd && (unsigned char)zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) ){` |
|    ! 0 | 3975 | `				zPtr++;` |
|    ! 0 | 3976 | `			}` |
|    ! 0 | 3977 | `			if( zPtr >= zEnd ){` |
|      - | 3978 | `				/* Empty line */` |
|    ! 0 | 3979 | `				continue;` |
|      - | 3980 | `			}` |
|    ! 0 | 3981 | `		}` |
|      5 | 3982 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 3983 | `		/* Insert line */` |
|      5 | 3984 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 3985 | `	}` |
|      - | 3986 | `	/* Close the stream */` |
|      3 | 3987 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 3988 | `	/* Release the io_private instance */` |
|      3 | 3989 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 3990 | `	/* Return the created array */` |
|      3 | 3991 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3992 | `	return PH7_OK;` |
|      5 | 3993 | `}` |
|      - | 3994 | `/*` |
|      - | 3995 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 3996 | ` *  Makes a copy of the file source to dest.` |
|      - | 3997 | ` * Parameters` |
|      - | 3998 | ` *  $source` |
|      - | 3999 | ` *   Path to the source file.` |
|      - | 4000 | ` *  $dest` |
|      - | 4001 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 4002 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 4003 | ` *  $context` |
|      - | 4004 | ` *   A context stream resource.` |
|      - | 4005 | ` * Return` |
|      - | 4006 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4007 | ` */` |
|     10 | 4008 | `static int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4009 | `{` |
|      - | 4010 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 4011 | `	const char *zFile;` |
|      - | 4012 | `	char zBuf[8192];` |
|      - | 4013 | `	void *pIn,*pOut;` |
|      - | 4014 | `	ph7_int64 n;` |
|      - | 4015 | `	int nLen;` |
|     12 | 4016 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 4017 | `		/* Missing/Invalid arguments,return FALSE */` |
|      7 | 4018 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|      7 | 4019 | `		ph7_result_bool(pCtx,0);` |
|      7 | 4020 | `		return PH7_OK;` |
|      - | 4021 | `	}` |
|      - | 4022 | `	/* Extract the source name */` |
|      6 | 4023 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4024 | `	/* Point to the target IO stream device */` |
|      6 | 4025 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 4026 | `	if( pSin == 0 ){` |
|    ! 0 | 4027 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4028 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4029 | `		return PH7_OK;` |
|      - | 4030 | `	}` |
|      - | 4031 | `	/* Try to open the source file in a read-only mode */` |
|      6 | 4032 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 4033 | `	if( pIn == 0 ){` |
|      3 | 4034 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening source: '%s'",zFile);` |
|      3 | 4035 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4036 | `		return PH7_OK;` |
|      - | 4037 | `	}` |
|      - | 4038 | `	/* Extract the destination name */` |
|      3 | 4039 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 4040 | `	/* Point to the target IO stream device */` |
|      3 | 4041 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4042 | `	if( pSout == 0 ){` |
|    ! 0 | 4043 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4044 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4045 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4046 | `		return PH7_OK;` |
|      - | 4047 | `	}` |
|      3 | 4048 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 4049 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4050 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4051 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 4052 | `			);` |
|    ! 0 | 4053 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4054 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4055 | `		return PH7_OK;` |
|      - | 4056 | `	}` |
|      - | 4057 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 4058 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 4059 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 4060 | `	if( pOut == 0 ){` |
|    ! 0 | 4061 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening destination: '%s'",zFile);` |
|    ! 0 | 4062 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4063 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4064 | `		return PH7_OK;` |
|      - | 4065 | `	}` |
|      - | 4066 | `	/* Perform the requested operation */` |
|      2 | 4067 | `	for(;;){` |
|      - | 4068 | `		/* Read from source */` |
|      5 | 4069 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 4070 | `		if( n < 1 ){` |
|      - | 4071 | `			/* EOF or IO error,break immediately */` |
|      3 | 4072 | `			break;` |
|      - | 4073 | `		}` |
|      - | 4074 | `		/* Write to dest */` |
|      3 | 4075 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 4076 | `		if( n < 1 ){` |
|      - | 4077 | `			/* IO error,break immediately */` |
|    ! 0 | 4078 | `			break;` |
|      - | 4079 | `		}` |
|      1 | 4080 | `	}` |
|      - | 4081 | `	/* Close the streams */` |
|      3 | 4082 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 4083 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 4084 | `	/* Return TRUE */` |
|      3 | 4085 | `	ph7_result_bool(pCtx,1);` |
|      3 | 4086 | `	return PH7_OK;` |
|      7 | 4087 | `}` |
|      - | 4088 | `/*` |
|      - | 4089 | ` * array fstat(resource $handle)` |
|      - | 4090 | ` *  Gets information about a file using an open file pointer.` |
|      - | 4091 | ` * Parameters` |
|      - | 4092 | ` *  $handle` |
|      - | 4093 | ` *   The file pointer.` |
|      - | 4094 | ` * Return` |
|      - | 4095 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 4096 | ` */` |
|      2 | 4097 | `static int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4098 | `{` |
|      - | 4099 | `	ph7_value *pArray,*pValue;` |
|      - | 4100 | `	const ph7_io_stream *pStream;` |
|      - | 4101 | `	io_private *pDev;` |
|      3 | 4102 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4103 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4104 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4105 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4106 | `		return PH7_OK;` |
|      - | 4107 | `	}` |
|      - | 4108 | `	/* Extract our private data */` |
|      3 | 4109 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4110 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4111 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4112 | `		/* Expecting an IO handle */` |
|    ! 0 | 4113 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4114 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4115 | `		return PH7_OK;` |
|      - | 4116 | `	}` |
|      - | 4117 | `	/* Point to the target IO stream device */` |
|      3 | 4118 | `	pStream = pDev->pStream;` |
|      3 | 4119 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 4120 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4121 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4122 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4123 | `			);` |
|    ! 0 | 4124 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4125 | `		return PH7_OK;` |
|      - | 4126 | `	}` |
|      - | 4127 | `	/* Create the array and the working value */` |
|      3 | 4128 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4129 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4130 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4131 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4132 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4133 | `		return PH7_OK;` |
|      - | 4134 | `	}` |
|      - | 4135 | `	/* Perform the requested operation */` |
|      3 | 4136 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 4137 | `	/* Return the freshly created array */` |
|      3 | 4138 | `	ph7_result_value(pCtx,pArray);` |
|      - | 4139 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 4140 | `	 * released automatically as soon we return from this function.` |
|      - | 4141 | `	 */` |
|      3 | 4142 | `	return PH7_OK;` |
|      2 | 4143 | `}` |
|      - | 4144 | `/*` |
|      - | 4145 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 4146 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 4147 | ` * Parameters` |
|      - | 4148 | ` *  $handle` |
|      - | 4149 | ` *   The file pointer.` |
|      - | 4150 | ` *  $string` |
|      - | 4151 | ` *   The string that is to be written.` |
|      - | 4152 | ` *  $length` |
|      - | 4153 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 4154 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 4155 | ` * Return` |
|      - | 4156 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 4157 | ` */` |
|      6 | 4158 | `static int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4159 | `{` |
|      - | 4160 | `	const ph7_io_stream *pStream;` |
|      - | 4161 | `	const char *zString;` |
|      - | 4162 | `	io_private *pDev;` |
|      - | 4163 | `	int nLen,n;` |
|      7 | 4164 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4165 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4166 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4167 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4168 | `		return PH7_OK;` |
|      - | 4169 | `	}` |
|      - | 4170 | `	/* Extract our private data */` |
|      7 | 4171 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4172 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 4173 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4174 | `		/* Expecting an IO handle */` |
|    ! 0 | 4175 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4176 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4177 | `		return PH7_OK;` |
|      - | 4178 | `	}` |
|      - | 4179 | `	/* Point to the target IO stream device */` |
|      7 | 4180 | `	pStream = pDev->pStream;` |
|      7 | 4181 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4182 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4183 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4184 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4185 | `			);` |
|    ! 0 | 4186 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4187 | `		return PH7_OK;` |
|      - | 4188 | `	}` |
|      - | 4189 | `	/* Extract the data to write */` |
|      7 | 4190 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|      7 | 4191 | `	if( nArg > 2 ){` |
|      - | 4192 | `		/* Maximum data length to write */` |
|    ! 0 | 4193 | `		n = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 4194 | `		if( n >= 0 && n < nLen ){` |
|    ! 0 | 4195 | `			nLen = n;` |
|    ! 0 | 4196 | `		}` |
|    ! 0 | 4197 | `	}` |
|      7 | 4198 | `	if( nLen < 1 ){` |
|      - | 4199 | `		/* Nothing to write */` |
|    ! 0 | 4200 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4201 | `		return PH7_OK;` |
|      - | 4202 | `	}` |
|      - | 4203 | `	/* Perform the requested operation */` |
|      7 | 4204 | `	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);` |
|      7 | 4205 | `	if( n <  0 ){` |
|      - | 4206 | `		/* IO error,return FALSE */` |
|    ! 0 | 4207 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4208 | `	}else{` |
|      - | 4209 | `		/* #Bytes written */` |
|      7 | 4210 | `		ph7_result_int(pCtx,n);` |
|      - | 4211 | `	}` |
|      7 | 4212 | `	return PH7_OK;` |
|      4 | 4213 | `}` |
|      - | 4214 | `/*` |
|      - | 4215 | ` * bool flock(resource $handle,int $operation)` |
|      - | 4216 | ` *  Portable advisory file locking.` |
|      - | 4217 | ` * Parameters` |
|      - | 4218 | ` *  $handle` |
|      - | 4219 | ` *   The file pointer.` |
|      - | 4220 | ` *  $operation` |
|      - | 4221 | ` *   operation is one of the following:` |
|      - | 4222 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 4223 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 4224 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 4225 | ` * Return` |
|      - | 4226 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 4227 | ` */` |
|      4 | 4228 | `static int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4229 | `{` |
|      - | 4230 | `	const ph7_io_stream *pStream;` |
|      - | 4231 | `	io_private *pDev;` |
|      - | 4232 | `	int nLock;` |
|      - | 4233 | `	int rc;` |
|      5 | 4234 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4235 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4236 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4237 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4238 | `		return PH7_OK;` |
|      - | 4239 | `	}` |
|      - | 4240 | `	/* Extract our private data */` |
|      5 | 4241 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4242 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 4243 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4244 | `		/*Expecting an IO handle */` |
|    ! 0 | 4245 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4246 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4247 | `		return PH7_OK;` |
|      - | 4248 | `	}` |
|      - | 4249 | `	/* Point to the target IO stream device */` |
|      5 | 4250 | `	pStream = pDev->pStream;` |
|      5 | 4251 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|    ! 0 | 4252 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4253 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4254 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4255 | `			);` |
|    ! 0 | 4256 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4257 | `		return PH7_OK;` |
|      - | 4258 | `	}` |
|      - | 4259 | `	/* Requested lock operation */` |
|      5 | 4260 | `	nLock = ph7_value_to_int(apArg[1]);` |
|      - | 4261 | `	/* Lock operation */` |
|      5 | 4262 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 4263 | `	/* IO result */` |
|      5 | 4264 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 4265 | `	return PH7_OK;` |
|      3 | 4266 | `}` |
|      - | 4267 | `/*` |
|      - | 4268 | ` * int fpassthru(resource $handle)` |
|      - | 4269 | ` *  Output all remaining data on a file pointer.` |
|      - | 4270 | ` * Parameters` |
|      - | 4271 | ` *  $handle` |
|      - | 4272 | ` *   The file pointer.` |
|      - | 4273 | ` * Return` |
|      - | 4274 | ` *  Total number of characters read from handle and passed through` |
|      - | 4275 | ` *  to the output on success or FALSE on failure.` |
|      - | 4276 | ` */` |
|      2 | 4277 | `static int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4278 | `{` |
|      - | 4279 | `	const ph7_io_stream *pStream;` |
|      - | 4280 | `	io_private *pDev;` |
|      - | 4281 | `	ph7_int64 n,nRead;` |
|      - | 4282 | `	char zBuf[8192];` |
|      - | 4283 | `	int rc;` |
|      3 | 4284 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4285 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4286 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4287 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4288 | `		return PH7_OK;` |
|      - | 4289 | `	}` |
|      - | 4290 | `	/* Extract our private data */` |
|      3 | 4291 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4292 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4293 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4294 | `		/*Expecting an IO handle */` |
|    ! 0 | 4295 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4296 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4297 | `		return PH7_OK;` |
|      - | 4298 | `	}` |
|      - | 4299 | `	/* Point to the target IO stream device */` |
|      3 | 4300 | `	pStream = pDev->pStream;` |
|      3 | 4301 | `	if( pStream == 0  ){` |
|    ! 0 | 4302 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4303 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4304 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4305 | `			);` |
|    ! 0 | 4306 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4307 | `		return PH7_OK;` |
|      - | 4308 | `	}` |
|      - | 4309 | `	/* Perform the requested operation */` |
|      3 | 4310 | `	nRead = 0;` |
|      2 | 4311 | `	for(;;){` |
|      5 | 4312 | `		n = StreamRead(pDev,zBuf,sizeof(zBuf));` |
|      5 | 4313 | `		if( n < 1 ){` |
|      - | 4314 | `			/* Error or EOF */` |
|      3 | 4315 | `			break;` |
|      - | 4316 | `		}` |
|      - | 4317 | `		/* Increment the read counter */` |
|      3 | 4318 | `		nRead += n;` |
|      - | 4319 | `		/* Output data */` |
|      3 | 4320 | `		rc = ph7_context_output(pCtx,zBuf,(int)nRead /* FIXME: 64-bit issues */);` |
|      3 | 4321 | `		if( rc == PH7_ABORT ){` |
|      - | 4322 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 4323 | `			break;` |
|      - | 4324 | `		}` |
|      1 | 4325 | `	}` |
|      - | 4326 | `	/* Total number of bytes readen */` |
|      3 | 4327 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 4328 | `	return PH7_OK;` |
|      2 | 4329 | `}` |
|      - | 4330 | `/* CSV reader/writer private data */` |
|      - | 4331 | `struct csv_data` |
|      - | 4332 | `{` |
|      - | 4333 | `	int delimiter;    /* Delimiter. Default ',' */` |
|      - | 4334 | `	int enclosure;    /* Enclosure. Default '"'*/` |
|      - | 4335 | `	io_private *pDev; /* Open stream handle */` |
|      - | 4336 | `	int iCount;       /* Counter */` |
|      - | 4337 | `};` |
|      - | 4338 | `/*` |
|      - | 4339 | ` * The following callback is used by the fputcsv() function inorder to iterate` |
|      - | 4340 | ` * throw array entries and output CSV data based on the current key and it's` |
|      - | 4341 | ` * associated data.` |
|      - | 4342 | ` */` |
|      6 | 4343 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 4344 | `{` |
|      7 | 4345 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 4346 | `	const char *zData;` |
|      - | 4347 | `	int nLen,c2;` |
|      - | 4348 | `	sxu32 n;` |
|      - | 4349 | `	/* Point to the raw data */` |
|      7 | 4350 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      7 | 4351 | `	if( nLen < 1 ){` |
|      - | 4352 | `		/* Nothing to write */` |
|    ! 0 | 4353 | `		return PH7_OK;` |
|      - | 4354 | `	}` |
|      7 | 4355 | `	if( pData->iCount > 0 ){` |
|      - | 4356 | `		/* Write the delimiter */` |
|      5 | 4357 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->delimiter,sizeof(char));` |
|      2 | 4358 | `	}` |
|      7 | 4359 | `	n = 1;` |
|      7 | 4360 | `	c2 = 0;` |
|     10 | 4361 | `	if( SyByteFind(zData,(sxu32)nLen,pData->delimiter,0) == SXRET_OK \|\|` |
|      6 | 4362 | `		SyByteFind(zData,(sxu32)nLen,pData->enclosure,&n) == SXRET_OK ){` |
|    ! 0 | 4363 | `			c2 = 1;` |
|    ! 0 | 4364 | `			if( n == 0 ){` |
|    ! 0 | 4365 | `				c2 = 2;` |
|    ! 0 | 4366 | `			}` |
|      - | 4367 | `			/* Write the enclosure */` |
|    ! 0 | 4368 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4369 | `			if( c2 > 1 ){` |
|    ! 0 | 4370 | `				pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4371 | `			}` |
|    ! 0 | 4372 | `	}` |
|      - | 4373 | `	/* Write the data */` |
|      7 | 4374 | `	if( pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)zData,(ph7_int64)nLen) < 1 ){` |
|    ! 0 | 4375 | `		SXUNUSED(pKey); /* cc warning */` |
|    ! 0 | 4376 | `		return PH7_ABORT;` |
|      - | 4377 | `	}` |
|      7 | 4378 | `	if( c2 > 0 ){` |
|      - | 4379 | `		/* Write the enclosure */` |
|    ! 0 | 4380 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4381 | `		if( c2 > 1 ){` |
|    ! 0 | 4382 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4383 | `		}` |
|    ! 0 | 4384 | `	}` |
|      7 | 4385 | `	pData->iCount++;` |
|      7 | 4386 | `	return PH7_OK;` |
|      4 | 4387 | `}` |
|      - | 4388 | `/*` |
|      - | 4389 | ` * int fputcsv(resource $handle,array $fields[,string $delimiter = ','[,string $enclosure = '"' ]])` |
|      - | 4390 | ` *  Format line as CSV and write to file pointer.` |
|      - | 4391 | ` * Parameters` |
|      - | 4392 | ` *  $handle` |
|      - | 4393 | ` *   Open file handle.` |
|      - | 4394 | ` * $fields` |
|      - | 4395 | ` *   An array of values.` |
|      - | 4396 | ` * $delimiter` |
|      - | 4397 | ` *   The optional delimiter parameter sets the field delimiter (one character only).` |
|      - | 4398 | ` * $enclosure` |
|      - | 4399 | ` *  The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 4400 | ` */` |
|      2 | 4401 | `static int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4402 | `{` |
|      - | 4403 | `	const ph7_io_stream *pStream;` |
|      - | 4404 | `	struct csv_data sCsv;` |
|      - | 4405 | `	io_private *pDev;` |
|      - | 4406 | `	char *zEol;` |
|      - | 4407 | `	int eolen;` |
|      3 | 4408 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4409 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4410 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|    ! 0 | 4411 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4412 | `		return PH7_OK;` |
|      - | 4413 | `	}` |
|      - | 4414 | `	/* Extract our private data */` |
|      3 | 4415 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4416 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4417 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4418 | `		/*Expecting an IO handle */` |
|    ! 0 | 4419 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4420 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4421 | `		return PH7_OK;` |
|      - | 4422 | `	}` |
|      - | 4423 | `	/* Point to the target IO stream device */` |
|      3 | 4424 | `	pStream = pDev->pStream;` |
|      3 | 4425 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4426 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4427 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4428 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4429 | `			);` |
|    ! 0 | 4430 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4431 | `		return PH7_OK;` |
|      - | 4432 | `	}` |
|      - | 4433 | `	/* Set default csv separator */` |
|      3 | 4434 | `	sCsv.delimiter = ',';` |
|      3 | 4435 | `	sCsv.enclosure = '"';` |
|      3 | 4436 | `	sCsv.pDev = pDev;` |
|      3 | 4437 | `	sCsv.iCount = 0;` |
|      3 | 4438 | `	if( nArg > 2 ){` |
|      - | 4439 | `		/* User delimiter */` |
|      - | 4440 | `		const char *z;` |
|      - | 4441 | `		int n;` |
|      3 | 4442 | `		z = ph7_value_to_string(apArg[2],&n);` |
|      3 | 4443 | `		if( n > 0 ){` |
|      3 | 4444 | `			sCsv.delimiter = z[0];` |
|      1 | 4445 | `		}` |
|      3 | 4446 | `		if( nArg > 3 ){` |
|      3 | 4447 | `			z = ph7_value_to_string(apArg[3],&n);` |
|      3 | 4448 | `			if( n > 0 ){` |
|      3 | 4449 | `				sCsv.enclosure = z[0];` |
|      1 | 4450 | `			}` |
|      1 | 4451 | `		}` |
|      1 | 4452 | `	}` |
|      - | 4453 | `	/* Iterate throw array entries and write csv data */` |
|      3 | 4454 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      - | 4455 | `	/* Write a line ending */` |
|      - | 4456 | `#ifdef __WINNT__` |
|      1 | 4457 | `	zEol = "\r\n";` |
|      1 | 4458 | `	eolen = (int)sizeof("\r\n")-1;` |
|      - | 4459 | `#else` |
|      - | 4460 | `	/* Assume UNIX LF */` |
|      2 | 4461 | `	zEol = "\n";` |
|      2 | 4462 | `	eolen = (int)sizeof(char);` |
|      - | 4463 | `#endif` |
|      3 | 4464 | `	pDev->pStream->xWrite(pDev->pHandle,(const void *)zEol,eolen);` |
|      3 | 4465 | `	return PH7_OK;` |
|      2 | 4466 | `}` |
|      - | 4467 | `/*` |
|      - | 4468 | ` * fprintf,vfprintf private data.` |
|      - | 4469 | ` * An instance of the following structure is passed to the formatted` |
|      - | 4470 | ` * input consumer callback defined below.` |
|      - | 4471 | ` */` |
|      - | 4472 | `typedef struct fprintf_data fprintf_data;` |
|      - | 4473 | `struct fprintf_data` |
|      - | 4474 | `{` |
|      - | 4475 | `	io_private *pIO;        /* IO stream */` |
|      - | 4476 | `	ph7_int64 nCount;       /* Total number of bytes written */` |
|      - | 4477 | `};` |
|      - | 4478 | `/*` |
|      - | 4479 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 4480 | ` */` |
|     30 | 4481 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4482 | `{` |
|     31 | 4483 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 4484 | `	ph7_int64 n;` |
|      - | 4485 | `	/* Write the formatted data */` |
|     31 | 4486 | `	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);` |
|     31 | 4487 | `	if( n < 1 ){` |
|    ! 0 | 4488 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 4489 | `		/* IO error,abort immediately */` |
|    ! 0 | 4490 | `		return SXERR_ABORT;` |
|      - | 4491 | `	}` |
|      - | 4492 | `	/* Increment counter */` |
|     31 | 4493 | `	pFdata->nCount += n;` |
|     31 | 4494 | `	return PH7_OK;` |
|     16 | 4495 | `}` |
|      - | 4496 | `/*` |
|      - | 4497 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 4498 | ` *  Write a formatted string to a stream.` |
|      - | 4499 | ` * Parameters` |
|      - | 4500 | ` *  $handle` |
|      - | 4501 | ` *   The file pointer.` |
|      - | 4502 | ` *  $format` |
|      - | 4503 | ` *   String format (see sprintf()).` |
|      - | 4504 | ` * Return` |
|      - | 4505 | ` *  The length of the written string.` |
|      - | 4506 | ` */` |
|     16 | 4507 | `static int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4508 | `{` |
|      - | 4509 | `	fprintf_data sFdata;` |
|      - | 4510 | `	const char *zFormat;` |
|      - | 4511 | `	io_private *pDev;` |
|      - | 4512 | `	int nLen;` |
|     17 | 4513 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4514 | `		/* Missing/Invalid arguments,return zero */` |
|    ! 0 | 4515 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4516 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4517 | `		return PH7_OK;` |
|      - | 4518 | `	}` |
|      - | 4519 | `	/* Extract our private data */` |
|     17 | 4520 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4521 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     17 | 4522 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4523 | `		/*Expecting an IO handle */` |
|    ! 0 | 4524 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4525 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4526 | `		return PH7_OK;` |
|      - | 4527 | `	}` |
|      - | 4528 | `	/* Point to the target IO stream device */` |
|     17 | 4529 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4530 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4531 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4532 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4533 | `			);` |
|    ! 0 | 4534 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4535 | `		return PH7_OK;` |
|      - | 4536 | `	}` |
|      - | 4537 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */` |
|      - | 4538 | `	{` |
|     17 | 4539 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|     17 | 4540 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4541 | `			return rcf;` |
|      - | 4542 | `		}` |
|      - | 4543 | `	}` |
|      - | 4544 | `	/* Extract the string format (scalars/null coerce). */` |
|     17 | 4545 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     17 | 4546 | `	if( nLen < 1 ){` |
|      - | 4547 | `		/* Empty string,return zero */` |
|    ! 0 | 4548 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4549 | `		return PH7_OK;` |
|      - | 4550 | `	}` |
|      - | 4551 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4552 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4553 | `	{` |
|     17 | 4554 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     17 | 4555 | `		if( rcv != PH7_OK ){` |
|      3 | 4556 | `			return rcv;` |
|      - | 4557 | `		}` |
|      - | 4558 | `	}` |
|      - | 4559 | `	/* Prepare our private data */` |
|     15 | 4560 | `	sFdata.nCount = 0;` |
|     15 | 4561 | `	sFdata.pIO = pDev;` |
|      - | 4562 | `	/* Format the string */` |
|     15 | 4563 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 4564 | `	/* Return total number of bytes written */` |
|     15 | 4565 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|     15 | 4566 | `	return PH7_OK;` |
|      9 | 4567 | `}` |
|      - | 4568 | `/*` |
|      - | 4569 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 4570 | ` *  Write a formatted string to a stream.` |
|      - | 4571 | ` * Parameters` |
|      - | 4572 | ` *  $handle` |
|      - | 4573 | ` *   The file pointer.` |
|      - | 4574 | ` *  $format` |
|      - | 4575 | ` *   String format (see sprintf()).` |
|      - | 4576 | ` * $args` |
|      - | 4577 | ` *   User arguments.` |
|      - | 4578 | ` * Return` |
|      - | 4579 | ` *  The length of the written string.` |
|      - | 4580 | ` */` |
|      4 | 4581 | `static int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4582 | `{` |
|      - | 4583 | `	fprintf_data sFdata;` |
|      - | 4584 | `	const char *zFormat;` |
|      - | 4585 | `	ph7_hashmap *pMap;` |
|      - | 4586 | `	io_private *pDev;` |
|      - | 4587 | `	SySet sArg;` |
|      - | 4588 | `	int n,nLen;` |
|      5 | 4589 | `	if( nArg < 3 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4590 | `		/* Missing/Invalid arguments,return zero */` |
|      3 | 4591 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|      3 | 4592 | `		ph7_result_int(pCtx,0);` |
|      3 | 4593 | `		return PH7_OK;` |
|      - | 4594 | `	}` |
|      - | 4595 | `	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */` |
|      - | 4596 | `	{` |
|      3 | 4597 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      3 | 4598 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4599 | `			return rcf;` |
|      - | 4600 | `		}` |
|      - | 4601 | `	}` |
|      3 | 4602 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|      - | 4603 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4604 | `		char zBuf[64];` |
|    ! 0 | 4605 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4606 | `			"vfprintf(): Argument #3 ($values) must be of type array, %s given",` |
|    ! 0 | 4607 | `			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|      - | 4608 | `	}` |
|      - | 4609 | `	/* Extract our private data */` |
|      3 | 4610 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4611 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4612 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4613 | `		/*Expecting an IO handle */` |
|    ! 0 | 4614 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4615 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4616 | `		return PH7_OK;` |
|      - | 4617 | `	}` |
|      - | 4618 | `	/* Point to the target IO stream device */` |
|      3 | 4619 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4620 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4621 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4622 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4623 | `			);` |
|    ! 0 | 4624 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4625 | `		return PH7_OK;` |
|      - | 4626 | `	}` |
|      - | 4627 | `	/* Extract the string format */` |
|      3 | 4628 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      3 | 4629 | `	if( nLen < 1 ){` |
|      - | 4630 | `		/* Empty string,return zero */` |
|    ! 0 | 4631 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4632 | `		return PH7_OK;` |
|      - | 4633 | `	}` |
|      - | 4634 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4635 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4636 | `	{` |
|      3 | 4637 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 4638 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4639 | `			return rcv;` |
|      - | 4640 | `		}` |
|      - | 4641 | `	}` |
|      - | 4642 | `	/* Point to hashmap */` |
|      3 | 4643 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 4644 | `	/* Extract arguments from the hashmap */` |
|      3 | 4645 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4646 | `	/* Prepare our private data */` |
|      3 | 4647 | `	sFdata.nCount = 0;` |
|      3 | 4648 | `	sFdata.pIO = pDev;` |
|      - | 4649 | `	/* Format the string */` |
|      3 | 4650 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 4651 | `	/* Return total number of bytes written*/` |
|      3 | 4652 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      3 | 4653 | `	SySetRelease(&sArg);` |
|      3 | 4654 | `	return PH7_OK;` |
|      3 | 4655 | `}` |
|      - | 4656 | `/*` |
|      - | 4657 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 4658 | ` * According to the PHP reference manual:` |
|      - | 4659 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 4660 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 4661 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 4662 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 4663 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 4664 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 4665 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 4666 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 4667 | ` *         exist, attempt to create it.` |
|      - | 4668 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 4669 | ` *          not exist, attempt to create it.` |
|      - | 4670 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 4671 | ` *         already exists,` |
|      - | 4672 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 4673 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 4674 | ` *         the underlying open(2) system call.` |
|      - | 4675 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 4676 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 4677 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 4678 | ` *          is positioned on the beginning of the file.` |
|      - | 4679 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 4680 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 4681 | ` *          be used after the lock is requested).` |
|      - | 4682 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 4683 | ` */` |
|     64 | 4684 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      2 | 4685 | `{` |
|     66 | 4686 | `	const char *zEnd = &zMode[nLen];` |
|     66 | 4687 | `	int iFlag = 0;` |
|      - | 4688 | `	int c;` |
|     66 | 4689 | `	if( nLen < 1 ){` |
|      - | 4690 | `		/* Open in a read-only mode */` |
|    ! 0 | 4691 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 4692 | `	}` |
|     66 | 4693 | `	c = zMode[0];` |
|     66 | 4694 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 4695 | `		/* Read-only access */` |
|     40 | 4696 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|     40 | 4697 | `		zMode++; /* Advance */` |
|     40 | 4698 | `		if( zMode < zEnd ){` |
|      7 | 4699 | `			c = zMode[0];` |
|      7 | 4700 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 4701 | `				/* Read+Write access */` |
|      7 | 4702 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|      3 | 4703 | `			}` |
|      5 | 4704 | `		}` |
|     46 | 4705 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 4706 | `		/* Overwrite mode.` |
|      - | 4707 | `		 * If the file does not exists,try to create it` |
|      - | 4708 | `		 */` |
|     27 | 4709 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     27 | 4710 | `		zMode++; /* Advance */` |
|     27 | 4711 | `		if( zMode < zEnd ){` |
|      3 | 4712 | `			c = zMode[0];` |
|      3 | 4713 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4714 | `				/* Read+Write access */` |
|      3 | 4715 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|      3 | 4716 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|      1 | 4717 | `			}` |
|      2 | 4718 | `		}` |
|     13 | 4719 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 4720 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 4721 | `		 * Create the file if it does not exists.` |
|      - | 4722 | `		 */` |
|    ! 0 | 4723 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4724 | `		zMode++; /* Advance */` |
|    ! 0 | 4725 | `		if( zMode < zEnd ){` |
|    ! 0 | 4726 | `			c = zMode[0];` |
|    ! 0 | 4727 | `			if( c == '+' ){` |
|      - | 4728 | `				/* Read-Write access */` |
|    ! 0 | 4729 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4730 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4731 | `			}` |
|    ! 0 | 4732 | `		}` |
|    ! 0 | 4733 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 4734 | `		/* Exclusive access.` |
|      - | 4735 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 4736 | `		 * Otherwise create a new file.` |
|      - | 4737 | `		 */` |
|    ! 0 | 4738 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|    ! 0 | 4739 | `		zMode++; /* Advance */` |
|    ! 0 | 4740 | `		if( zMode < zEnd ){` |
|    ! 0 | 4741 | `			c = zMode[0];` |
|    ! 0 | 4742 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4743 | `				/* Read-Write access */` |
|    ! 0 | 4744 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4745 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4746 | `			}` |
|    ! 0 | 4747 | `		}` |
|    ! 0 | 4748 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 4749 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 4750 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4751 | `		zMode++; /* Advance */` |
|    ! 0 | 4752 | `		if( zMode < zEnd ){` |
|    ! 0 | 4753 | `			c = zMode[0];` |
|    ! 0 | 4754 | `			if( c == '+' ){` |
|      - | 4755 | `				/* Read-Write access */` |
|    ! 0 | 4756 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4757 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4758 | `			}` |
|    ! 0 | 4759 | `		}` |
|    ! 0 | 4760 | `	}else{` |
|      - | 4761 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 4762 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 4763 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 4764 | `	}` |
|     74 | 4765 | `	while( zMode < zEnd ){` |
|      9 | 4766 | `		c = zMode[0];` |
|      9 | 4767 | `		if( c == 'b' \|\| c == 'B' ){` |
|    ! 0 | 4768 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4769 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|      9 | 4770 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 4771 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 4772 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4773 | `		}` |
|      9 | 4774 | `		zMode++;` |
|      1 | 4775 | `	}` |
|     66 | 4776 | `	return iFlag;` |
|     34 | 4777 | `}` |
|      - | 4778 | `/*` |
|      - | 4779 | ` * Initialize the IO private structure.` |
|      - | 4780 | ` */` |
|   4992 | 4781 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      5 | 4782 | `{` |
|   4997 | 4783 | `	pOut->pStream = pStream;` |
|   4997 | 4784 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   4997 | 4785 | `	pOut->nOfft = 0;` |
|      - | 4786 | `	/* Set the magic number */` |
|   4997 | 4787 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   4997 | 4788 | `}` |
|      - | 4789 | `/*` |
|      - | 4790 | ` * Release the IO private structure.` |
|      - | 4791 | ` */` |
|   4960 | 4792 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      5 | 4793 | `{` |
|   4965 | 4794 | `	SyBlobRelease(&pDev->sBuffer);` |
|   4965 | 4795 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 4796 | `	/* Release the whole structure */` |
|   4965 | 4797 | `	ph7_context_free_chunk(pCtx,pDev);` |
|   4965 | 4798 | `}` |
|      - | 4799 | `/*` |
|      - | 4800 | ` * Reset the IO private structure.` |
|      - | 4801 | ` */` |
|     12 | 4802 | `static void ResetIOPrivate(io_private *pDev)` |
|      1 | 4803 | `{` |
|     13 | 4804 | `	SyBlobReset(&pDev->sBuffer);` |
|     13 | 4805 | `	pDev->nOfft = 0;` |
|     13 | 4806 | `}` |
|      - | 4807 | `/* Forward declaration */` |
|      - | 4808 | `static int is_php_stream(const ph7_io_stream *pStream);` |
|      - | 4809 | `/*` |
|      - | 4810 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 4811 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 4812 | ` * Parameters` |
|      - | 4813 | ` *  $filename` |
|      - | 4814 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 4815 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 4816 | ` *   then a regular file is assumed.` |
|      - | 4817 | ` *  $mode` |
|      - | 4818 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 4819 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 4820 | ` *   modes.` |
|      - | 4821 | ` *  $use_include_path` |
|      - | 4822 | ` *   You can use the optional second parameter and set it to` |
|      - | 4823 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 4824 | ` *  $context` |
|      - | 4825 | ` *   A context stream resource.` |
|      - | 4826 | ` * Return` |
|      - | 4827 | ` *  File handle on success or FALSE on failure.` |
|      - | 4828 | ` */` |
|     64 | 4829 | `static int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4830 | `{` |
|      - | 4831 | `	const ph7_io_stream *pStream;` |
|      - | 4832 | `	const char *zUri,*zMode;` |
|      - | 4833 | `	ph7_value *pResource;` |
|      - | 4834 | `	io_private *pDev;` |
|      - | 4835 | `	int iLen,imLen;` |
|      - | 4836 | `	int iOpenFlags;` |
|     66 | 4837 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4838 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4839 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 4840 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4841 | `		return PH7_OK;` |
|      - | 4842 | `	}` |
|      - | 4843 | `	/* Extract the URI and the desired access mode */` |
|     66 | 4844 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|     66 | 4845 | `	if( nArg > 1 ){` |
|     66 | 4846 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|     34 | 4847 | `	}else{` |
|      - | 4848 | `		/* Set a default read-only mode */` |
|    ! 0 | 4849 | `		zMode = "r";` |
|    ! 0 | 4850 | `		imLen = (int)sizeof(char);` |
|      - | 4851 | `	}` |
|      - | 4852 | `	/* Try to extract a stream */` |
|     66 | 4853 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|     66 | 4854 | `	if( pStream == 0 ){` |
|    ! 0 | 4855 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 4856 | `			"No stream device is associated with the given URI(%s)",zUri);` |
|    ! 0 | 4857 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4858 | `		return PH7_OK;` |
|      - | 4859 | `	}` |
|      - | 4860 | `	/* Allocate a new IO private instance */` |
|     66 | 4861 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     66 | 4862 | `	if( pDev == 0 ){` |
|    ! 0 | 4863 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4864 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4865 | `		return PH7_OK;` |
|      - | 4866 | `	}` |
|     66 | 4867 | `	pResource = 0;` |
|     66 | 4868 | `	if( nArg > 3 ){` |
|    ! 0 | 4869 | `		pResource = apArg[3];` |
|     66 | 4870 | `	}else if( is_php_stream(pStream) ){` |
|      - | 4871 | `		/* TICKET 1433-80: The php:// stream need a ph7_value to access the underlying` |
|      - | 4872 | `		 * virtual machine.` |
|      - | 4873 | `		 */` |
|      3 | 4874 | `		pResource = apArg[0];` |
|      1 | 4875 | `	}` |
|      - | 4876 | `	/* Initialize the structure */` |
|     66 | 4877 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 4878 | `	/* Convert open mode to PH7 flags */` |
|     66 | 4879 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|      - | 4880 | `	/* Try to get a handle */` |
|     98 | 4881 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|     32 | 4882 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);` |
|     66 | 4883 | `	if( pDev->pHandle == 0 ){` |
|    ! 0 | 4884 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zUri);` |
|    ! 0 | 4885 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4886 | `		ph7_context_free_chunk(pCtx,pDev);` |
|    ! 0 | 4887 | `		return PH7_OK;` |
|      - | 4888 | `	}` |
|      - | 4889 | `	/* All done,return the io_private instance as a resource */` |
|     66 | 4890 | `	ph7_result_resource(pCtx,pDev);` |
|     66 | 4891 | `	return PH7_OK;` |
|     34 | 4892 | `}` |
|      - | 4893 | `/*` |
|      - | 4894 | ` * bool fclose(resource $handle)` |
|      - | 4895 | ` *  Closes an open file pointer` |
|      - | 4896 | ` * Parameters` |
|      - | 4897 | ` *  $handle` |
|      - | 4898 | ` *   The file pointer.` |
|      - | 4899 | ` * Return` |
|      - | 4900 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4901 | ` */` |
|    156 | 4902 | `static int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 4903 | `{` |
|      - | 4904 | `	const ph7_io_stream *pStream;` |
|      - | 4905 | `	io_private *pDev;` |
|      - | 4906 | `	ph7_vm *pVm;` |
|    161 | 4907 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4908 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4909 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4910 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4911 | `		return PH7_OK;` |
|      - | 4912 | `	}` |
|      - | 4913 | `	/* Extract our private data */` |
|    161 | 4914 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4915 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    161 | 4916 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4917 | `		/*Expecting an IO handle */` |
|    ! 0 | 4918 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4919 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4920 | `		return PH7_OK;` |
|      - | 4921 | `	}` |
|      - | 4922 | `	/* Point to the target IO stream device */` |
|    161 | 4923 | `	pStream = pDev->pStream;` |
|    161 | 4924 | `	if( pStream == 0 ){` |
|    ! 0 | 4925 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4926 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4927 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4928 | `			);` |
|    ! 0 | 4929 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4930 | `		return PH7_OK;` |
|      - | 4931 | `	}` |
|      - | 4932 | `	/* Point to the VM that own this context */` |
|    161 | 4933 | `	pVm = pCtx->pVm;` |
|      - | 4934 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|    161 | 4935 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 4936 | `		/* Perform the requested operation */` |
|    161 | 4937 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 4938 | `		/* Release the IO private structure */` |
|    161 | 4939 | `		ReleaseIOPrivate(pCtx,pDev);` |
|      - | 4940 | `		/* Invalidate the resource handle */` |
|    161 | 4941 | `		ph7_value_release(apArg[0]);` |
|     78 | 4942 | `	}` |
|      - | 4943 | `	/* Return TRUE */` |
|    161 | 4944 | `	ph7_result_bool(pCtx,1);` |
|    161 | 4945 | `	return PH7_OK;` |
|     83 | 4946 | `}` |
|      - | 4947 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4948 | `/*` |
|      - | 4949 | ` * MD5/SHA1 digest consumer.` |
|      - | 4950 | ` */` |
|     72 | 4951 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 4952 | `{` |
|      - | 4953 | `	/* Append hex chunk verbatim */` |
|     73 | 4954 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     73 | 4955 | `	return SXRET_OK;` |
|      1 | 4956 | `}` |
|      - | 4957 | `/*` |
|      - | 4958 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 4959 | ` *  Calculates the md5 hash of a given file.` |
|      - | 4960 | ` * Parameters` |
|      - | 4961 | ` *  $uri` |
|      - | 4962 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 4963 | ` *  $raw_output` |
|      - | 4964 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 4965 | ` * Return` |
|      - | 4966 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 4967 | ` */` |
|      2 | 4968 | `static int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4969 | `{` |
|      - | 4970 | `	const ph7_io_stream *pStream;` |
|      - | 4971 | `	unsigned char zDigest[16];` |
|      3 | 4972 | `	int raw_output  = FALSE;` |
|      - | 4973 | `	const char *zFile;` |
|      - | 4974 | `	MD5Context sCtx;` |
|      - | 4975 | `	char zBuf[8192];` |
|      - | 4976 | `	void *pHandle;` |
|      - | 4977 | `	ph7_int64 n;` |
|      - | 4978 | `	int nLen;` |
|      3 | 4979 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4980 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4981 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 4982 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4983 | `		return PH7_OK;` |
|      - | 4984 | `	}` |
|      - | 4985 | `	/* Extract the file path */` |
|      3 | 4986 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4987 | `	/* Point to the target IO stream device */` |
|      3 | 4988 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4989 | `	if( pStream == 0 ){` |
|    ! 0 | 4990 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4991 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4992 | `		return PH7_OK;` |
|      - | 4993 | `	}` |
|      3 | 4994 | `	if( nArg > 1 ){` |
|    ! 0 | 4995 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 4996 | `	}` |
|      - | 4997 | `	/* Try to open the file in read-only mode */` |
|      3 | 4998 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 4999 | `	if( pHandle == 0 ){` |
|    ! 0 | 5000 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5001 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5002 | `		return PH7_OK;` |
|      - | 5003 | `	}` |
|      - | 5004 | `	/* Init the MD5 context */` |
|      3 | 5005 | `	MD5Init(&sCtx);` |
|      - | 5006 | `	/* Perform the requested operation */` |
|      2 | 5007 | `	for(;;){` |
|      5 | 5008 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5009 | `		if( n < 1 ){` |
|      - | 5010 | `			/* EOF or IO error,break immediately */` |
|      3 | 5011 | `			break;` |
|      - | 5012 | `		}` |
|      3 | 5013 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5014 | `	}` |
|      - | 5015 | `	/* Close the stream */` |
|      3 | 5016 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5017 | `	/* Extract the digest */` |
|      3 | 5018 | `	MD5Final(zDigest,&sCtx);` |
|      3 | 5019 | `	if( raw_output ){` |
|      - | 5020 | `		/* Output raw digest */` |
|    ! 0 | 5021 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5022 | `	}else{` |
|      - | 5023 | `		/* Perform a binary to hex conversion */` |
|      3 | 5024 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5025 | `	}` |
|      3 | 5026 | `	return PH7_OK;` |
|      2 | 5027 | `}` |
|      - | 5028 | `/*` |
|      - | 5029 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 5030 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 5031 | ` * Parameters` |
|      - | 5032 | ` *  $uri` |
|      - | 5033 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 5034 | ` *  $raw_output` |
|      - | 5035 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 5036 | ` * Return` |
|      - | 5037 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 5038 | ` */` |
|      2 | 5039 | `static int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5040 | `{` |
|      - | 5041 | `	const ph7_io_stream *pStream;` |
|      - | 5042 | `	unsigned char zDigest[20];` |
|      3 | 5043 | `	int raw_output  = FALSE;` |
|      - | 5044 | `	const char *zFile;` |
|      - | 5045 | `	SHA1Context sCtx;` |
|      - | 5046 | `	char zBuf[8192];` |
|      - | 5047 | `	void *pHandle;` |
|      - | 5048 | `	ph7_int64 n;` |
|      - | 5049 | `	int nLen;` |
|      3 | 5050 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5051 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5052 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5053 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5054 | `		return PH7_OK;` |
|      - | 5055 | `	}` |
|      - | 5056 | `	/* Extract the file path */` |
|      3 | 5057 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5058 | `	/* Point to the target IO stream device */` |
|      3 | 5059 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5060 | `	if( pStream == 0 ){` |
|    ! 0 | 5061 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5062 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5063 | `		return PH7_OK;` |
|      - | 5064 | `	}` |
|      3 | 5065 | `	if( nArg > 1 ){` |
|    ! 0 | 5066 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 5067 | `	}` |
|      - | 5068 | `	/* Try to open the file in read-only mode */` |
|      3 | 5069 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5070 | `	if( pHandle == 0 ){` |
|    ! 0 | 5071 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5072 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5073 | `		return PH7_OK;` |
|      - | 5074 | `	}` |
|      - | 5075 | `	/* Init the SHA1 context */` |
|      3 | 5076 | `	SHA1Init(&sCtx);` |
|      - | 5077 | `	/* Perform the requested operation */` |
|      2 | 5078 | `	for(;;){` |
|      5 | 5079 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5080 | `		if( n < 1 ){` |
|      - | 5081 | `			/* EOF or IO error,break immediately */` |
|      3 | 5082 | `			break;` |
|      - | 5083 | `		}` |
|      3 | 5084 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5085 | `	}` |
|      - | 5086 | `	/* Close the stream */` |
|      3 | 5087 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5088 | `	/* Extract the digest */` |
|      3 | 5089 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 5090 | `	if( raw_output ){` |
|      - | 5091 | `		/* Output raw digest */` |
|    ! 0 | 5092 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5093 | `	}else{` |
|      - | 5094 | `		/* Perform a binary to hex conversion */` |
|      3 | 5095 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5096 | `	}` |
|      3 | 5097 | `	return PH7_OK;` |
|      2 | 5098 | `}` |
|      - | 5099 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5100 | `/*` |
|      - | 5101 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 5102 | ` *  Parse a configuration file.` |
|      - | 5103 | ` * Parameters` |
|      - | 5104 | ` * $filename` |
|      - | 5105 | ` *  The filename of the ini file being parsed.` |
|      - | 5106 | ` * $process_sections` |
|      - | 5107 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 5108 | ` *  with the section names and settings included.` |
|      - | 5109 | ` *  The default for process_sections is FALSE.` |
|      - | 5110 | ` * $scanner_mode` |
|      - | 5111 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 5112 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 5113 | ` * Return` |
|      - | 5114 | ` *  The settings are returned as an associative array on success.` |
|      - | 5115 | ` *  Otherwise is returned.` |
|      - | 5116 | ` */` |
|      2 | 5117 | `static int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5118 | `{` |
|      - | 5119 | `	const ph7_io_stream *pStream;` |
|      - | 5120 | `	const char *zFile;` |
|      - | 5121 | `	SyBlob sContents;` |
|      - | 5122 | `	void *pHandle;` |
|      - | 5123 | `	int nLen;` |
|      3 | 5124 | `	sxi32 rc = PH7_OK;` |
|      3 | 5125 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5126 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5127 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5128 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5129 | `		return PH7_OK;` |
|      - | 5130 | `	}` |
|      - | 5131 | `	/* Extract the file path */` |
|      3 | 5132 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5133 | `	/* Point to the target IO stream device */` |
|      3 | 5134 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5135 | `	if( pStream == 0 ){` |
|    ! 0 | 5136 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5137 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5138 | `		return PH7_OK;` |
|      - | 5139 | `	}` |
|      - | 5140 | `	/* Try to open the file in read-only mode */` |
|      3 | 5141 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5142 | `	if( pHandle == 0 ){` |
|    ! 0 | 5143 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5144 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5145 | `		return PH7_OK;` |
|      - | 5146 | `	}` |
|      3 | 5147 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 5148 | `	/* Read the whole file */` |
|      3 | 5149 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      3 | 5150 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 5151 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 5152 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5153 | `	}else{` |
|      - | 5154 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|      5 | 5155 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      2 | 5156 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0);` |
|      - | 5157 | `	}` |
|      - | 5158 | `	/* Close the stream */` |
|      3 | 5159 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5160 | `	/* Release the working buffer */` |
|      3 | 5161 | `	SyBlobRelease(&sContents);` |
|      - | 5162 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      3 | 5163 | `	return rc;` |
|      2 | 5164 | `}` |
|      - | 5165 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 5166 | `#else /* PH7_DISABLE_DISK_IO */` |
|      - | 5167 | `/*` |
|      - | 5168 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|      - | 5169 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|      - | 5170 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|      - | 5171 | ` * to any non-VFS resource).` |
|      - | 5172 | ` */` |
|      - | 5173 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      - | 5174 | `{` |
|      - | 5175 | `	SXUNUSED(pResource);` |
|      - | 5176 | `	return "Unknown";` |
|      - | 5177 | `}` |
|      - | 5178 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 5179 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 5180 | `#if defined(_MSC_VER)` |
|      - | 5181 | `static const ph7_vfs null_vfs = {` |
|      - | 5182 | `#else` |
|      - | 5183 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 5184 | `#endif` |
|      - | 5185 | `	"null_vfs",` |
|      - | 5186 | `	PH7_VFS_VERSION,` |
|      - | 5187 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 5188 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 5189 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 5190 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 5191 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 5192 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 5193 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 5194 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 5195 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 5196 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 5197 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 5198 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 5199 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 5200 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 5201 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 5202 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 5203 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 5204 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 5205 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 5206 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 5207 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 5208 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 5209 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 5210 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 5211 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 5212 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 5213 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 5214 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 5215 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 5216 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 5217 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 5218 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 5219 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 5220 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 5221 | `	0, /* int (*xUmask)(int) */` |
|      - | 5222 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 5223 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 5224 | `	0, /* int (*xUid)(void) */` |
|      - | 5225 | `	0, /* int (*xGid)(void) */` |
|      - | 5226 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 5227 | `	0  /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 5228 | `};` |
|      - | 5229 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 5230 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 5231 | `/*` |
|      - | 5232 | ` * Export the builtin vfs.` |
|      - | 5233 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 5234 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 5235 | ` * Note:` |
|      - | 5236 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 5237 | ` * Note:` |
|      - | 5238 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 5239 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 5240 | ` */` |
|   3880 | 5241 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 5242 | `{` |
|      - | 5243 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 5244 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 5245 | `	return &null_vfs;` |
|      - | 5246 | `#else` |
|      - | 5247 | `#ifdef __WINNT__` |
|      5 | 5248 | `	return &sWinVfs;` |
|      - | 5249 | `#elif defined(__UNIXES__)` |
|   3880 | 5250 | `	return &sUnixVfs;` |
|      - | 5251 | `#else` |
|      - | 5252 | `	return &null_vfs;` |
|      - | 5253 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 5254 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 5255 | `#else` |
|      - | 5256 | `	return &null_vfs;` |
|      - | 5257 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 5258 | `}` |
|      - | 5259 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 5260 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 5261 | `/*` |
|      - | 5262 | ` * The following defines are mostly used by the UNIX built and have` |
|      - | 5263 | ` * no particular meaning on windows.` |
|      - | 5264 | ` */` |
|      - | 5265 | `#ifndef STDIN_FILENO` |
|      - | 5266 | `#define STDIN_FILENO	0` |
|      - | 5267 | `#endif` |
|      - | 5268 | `#ifndef STDOUT_FILENO` |
|      - | 5269 | `#define STDOUT_FILENO	1` |
|      - | 5270 | `#endif` |
|      - | 5271 | `#ifndef STDERR_FILENO` |
|      - | 5272 | `#define STDERR_FILENO	2` |
|      - | 5273 | `#endif` |
|      - | 5274 | `/*` |
|      - | 5275 | ` * php:// Accessing various I/O streams` |
|      - | 5276 | ` * According to the PHP langage reference manual` |
|      - | 5277 | ` * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input` |
|      - | 5278 | ` * and output streams, the standard input, output and error file descriptors.` |
|      - | 5279 | ` * php://stdin, php://stdout and php://stderr:` |
|      - | 5280 | ` *  Allow direct access to the corresponding input or output stream of the PHP process.` |
|      - | 5281 | ` *  The stream references a duplicate file descriptor, so if you open php://stdin and later` |
|      - | 5282 | ` *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.` |
|      - | 5283 | ` *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.` |
|      - | 5284 | ` * php://output` |
|      - | 5285 | ` *  php://output is a write-only stream that allows you to write to the output buffer` |
|      - | 5286 | ` *  mechanism in the same way as print and echo.` |
|      - | 5287 | ` */` |
|      - | 5288 | `typedef struct ph7_stream_data ph7_stream_data;` |
|      - | 5289 | `/* Supported IO streams */` |
|      - | 5290 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|      - | 5291 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|      - | 5292 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|      - | 5293 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|      - | 5294 | ` /* The following structure is the private data associated with the php:// stream */` |
|      - | 5295 | `struct ph7_stream_data` |
|      - | 5296 | `{` |
|      - | 5297 | `	ph7_vm *pVm; /* VM that own this instance */` |
|      - | 5298 | `	int iType;   /* Stream type */` |
|      - | 5299 | `	union{` |
|      - | 5300 | `		void *pHandle; /* Stream handle */` |
|      - | 5301 | `		ph7_output_consumer sConsumer; /* VM output consumer */` |
|      - | 5302 | `	}x;` |
|      - | 5303 | `};` |
|      - | 5304 | `/*` |
|      - | 5305 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|      - | 5306 | ` */` |
|      8 | 5307 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|      1 | 5308 | `{` |
|      - | 5309 | `	ph7_stream_data *pData;` |
|      9 | 5310 | `	if( pVm == 0 ){` |
|    ! 0 | 5311 | `		return 0;` |
|      - | 5312 | `	}` |
|      - | 5313 | `	/* Allocate a new instance */` |
|      9 | 5314 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|      9 | 5315 | `	if( pData == 0 ){` |
|    ! 0 | 5316 | `		return 0;` |
|      - | 5317 | `	}` |
|      - | 5318 | `	/* Zero the structure */` |
|      9 | 5319 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|      - | 5320 | `	/* Initialize fields */` |
|      9 | 5321 | `	pData->iType = iType;` |
|      9 | 5322 | `	if( iType == PH7_IO_STREAM_OUTPUT ){` |
|      - | 5323 | `		/* Point to the default VM consumer routine. */` |
|      3 | 5324 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|      2 | 5325 | `	}else{` |
|      - | 5326 | `#ifdef __WINNT__` |
|      - | 5327 | `		DWORD nChannel;` |
|      1 | 5328 | `		switch(iType){` |
|      1 | 5329 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|      1 | 5330 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|      - | 5331 | `		default:` |
|      1 | 5332 | `			nChannel = STD_INPUT_HANDLE;` |
|      - | 5333 | `			break;` |
|      - | 5334 | `		}` |
|      1 | 5335 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|      - | 5336 | `#else` |
|      - | 5337 | `		/* Assume an UNIX system */` |
|      6 | 5338 | `		int ifd = STDIN_FILENO;` |
|      6 | 5339 | `		switch(iType){` |
|      2 | 5340 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|      2 | 5341 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|      1 | 5342 | `		default:` |
|      2 | 5343 | `			break;` |
|      - | 5344 | `		}` |
|      6 | 5345 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|      - | 5346 | `#endif` |
|      - | 5347 | `	}` |
|      9 | 5348 | `	pData->pVm = pVm;` |
|      9 | 5349 | `	return pData;` |
|      5 | 5350 | `}` |
|      - | 5351 | `/*` |
|      - | 5352 | ` * Implementation of the php:// IO streams routines` |
|      - | 5353 | ` * Status:` |
|      - | 5354 | ` *   Stable.` |
|      - | 5355 | ` */` |
|      - | 5356 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|      2 | 5357 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 5358 | `{` |
|      - | 5359 | `	ph7_stream_data *pData;` |
|      - | 5360 | `	SyString sStream;` |
|      3 | 5361 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|      - | 5362 | `	/* Trim leading and trailing white spaces */` |
|      3 | 5363 | `	SyStringFullTrim(&sStream);` |
|      - | 5364 | `	/* Stream to open */` |
|      3 | 5365 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|    ! 0 | 5366 | `		iMode = PH7_IO_STREAM_STDIN;` |
|      3 | 5367 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|      3 | 5368 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|      1 | 5369 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|    ! 0 | 5370 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|    ! 0 | 5371 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|    ! 0 | 5372 | `		iMode = PH7_IO_STREAM_STDERR;` |
|    ! 0 | 5373 | `	}else{` |
|      - | 5374 | `		/* unknown stream name */` |
|    ! 0 | 5375 | `		return -1;` |
|      - | 5376 | `	}` |
|      - | 5377 | `	/* Create our handle */` |
|      3 | 5378 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|      3 | 5379 | `	if( pData == 0 ){` |
|    ! 0 | 5380 | `		return -1;` |
|      - | 5381 | `	}` |
|      - | 5382 | `	/* Make the handle public */` |
|      3 | 5383 | `	*ppHandle = (void *)pData;` |
|      3 | 5384 | `	return PH7_OK;` |
|      2 | 5385 | `}` |
|      - | 5386 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|    ! 0 | 5387 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|    ! 0 | 5388 | `{` |
|    ! 0 | 5389 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    ! 0 | 5390 | `	if( pData == 0 ){` |
|    ! 0 | 5391 | `		return -1;` |
|      - | 5392 | `	}` |
|    ! 0 | 5393 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|      - | 5394 | `		/* Forbidden */` |
|    ! 0 | 5395 | `		return -1;` |
|      - | 5396 | `	}` |
|      - | 5397 | `#ifdef __WINNT__` |
|      - | 5398 | `	{` |
|      - | 5399 | `		DWORD nRd;` |
|      - | 5400 | `		BOOL rc;` |
|    ! 0 | 5401 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    ! 0 | 5402 | `		if( !rc ){` |
|      - | 5403 | `			/* IO error */` |
|    ! 0 | 5404 | `			return -1;` |
|      - | 5405 | `		}` |
|    ! 0 | 5406 | `		return (ph7_int64)nRd;` |
|      - | 5407 | `	}` |
|      - | 5408 | `#elif defined(__UNIXES__)` |
|      - | 5409 | `	{` |
|      - | 5410 | `		ssize_t nRd;` |
|      - | 5411 | `		int fd;` |
|    ! 0 | 5412 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 5413 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|    ! 0 | 5414 | `		if( nRd < 1 ){` |
|    ! 0 | 5415 | `			return -1;` |
|      - | 5416 | `		}` |
|    ! 0 | 5417 | `		return (ph7_int64)nRd;` |
|      - | 5418 | `	}` |
|      - | 5419 | `#else` |
|      - | 5420 | `	return -1;` |
|      - | 5421 | `#endif` |
|    ! 0 | 5422 | `}` |
|      - | 5423 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|      2 | 5424 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 5425 | `{` |
|      3 | 5426 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      3 | 5427 | `	if( pData == 0 ){` |
|    ! 0 | 5428 | `		return -1;` |
|      - | 5429 | `	}` |
|      3 | 5430 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|      - | 5431 | `		/* Forbidden */` |
|    ! 0 | 5432 | `		return -1;` |
|      3 | 5433 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|      3 | 5434 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|      - | 5435 | `		int rc;` |
|      - | 5436 | `		/* Call the vm output consumer */` |
|      3 | 5437 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|      3 | 5438 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 5439 | `			return -1;` |
|      - | 5440 | `		}` |
|      3 | 5441 | `		return nWrite;` |
|      - | 5442 | `	}` |
|      - | 5443 | `#ifdef __WINNT__` |
|      - | 5444 | `	{` |
|      - | 5445 | `		DWORD nWr;` |
|      - | 5446 | `		BOOL rc;` |
|    ! 0 | 5447 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|    ! 0 | 5448 | `		if( !rc ){` |
|      - | 5449 | `			/* IO error */` |
|    ! 0 | 5450 | `			return -1;` |
|      - | 5451 | `		}` |
|    ! 0 | 5452 | `		return (ph7_int64)nWr;` |
|      - | 5453 | `	}` |
|      - | 5454 | `#elif defined(__UNIXES__)` |
|      - | 5455 | `	{` |
|      - | 5456 | `		ssize_t nWr;` |
|      - | 5457 | `		int fd;` |
|    ! 0 | 5458 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 5459 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|    ! 0 | 5460 | `		if( nWr < 1 ){` |
|    ! 0 | 5461 | `			return -1;` |
|      - | 5462 | `		}` |
|    ! 0 | 5463 | `		return (ph7_int64)nWr;` |
|      - | 5464 | `	}` |
|      - | 5465 | `#else` |
|      - | 5466 | `	return -1;` |
|      - | 5467 | `#endif` |
|      2 | 5468 | `}` |
|      - | 5469 | `/* void (*xClose)(void *) */` |
|      2 | 5470 | `static void PHPStreamData_Close(void *pHandle)` |
|      1 | 5471 | `{` |
|      3 | 5472 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 5473 | `	ph7_vm *pVm;` |
|      3 | 5474 | `	if( pData == 0 ){` |
|    ! 0 | 5475 | `		return;` |
|      - | 5476 | `	}` |
|      3 | 5477 | `	pVm = pData->pVm;` |
|      - | 5478 | `	/* Free the instance */` |
|      3 | 5479 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|      2 | 5480 | `}` |
|      - | 5481 | `/*` |
|      - | 5482 | ` * Pipe stream implementation for popen/pclose.` |
|      - | 5483 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|      - | 5484 | ` * PHP-compatible process I/O functionality.` |
|      - | 5485 | ` */` |
|      - | 5486 | `typedef struct pipe_private pipe_private;` |
|      - | 5487 | `struct pipe_private` |
|      - | 5488 | `{` |
|      - | 5489 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|      - | 5490 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|      - | 5491 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|      - | 5492 | `#ifdef __WINNT__` |
|      - | 5493 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|      - | 5494 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|      - | 5495 | `#endif` |
|      - | 5496 | `};` |
|      - | 5497 |  |
|      - | 5498 | `#ifdef __WINNT__` |
|      - | 5499 | `#include <Windows.h>` |
|      - | 5500 | `#include <stdio.h>` |
|      - | 5501 | `#include <io.h>` |
|      - | 5502 | `#include <fcntl.h>` |
|      - | 5503 | `/*` |
|      - | 5504 | ` * Custom Windows popen implementation using CreateProcess.` |
|      - | 5505 | ` * This allows us to properly wait for process completion.` |
|      - | 5506 | ` */` |
|      - | 5507 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|      5 | 5508 | `{` |
|      5 | 5509 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|      5 | 5510 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|      5 | 5511 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|      - | 5512 | `	SECURITY_ATTRIBUTES sa;` |
|      - | 5513 | `	STARTUPINFOW si;` |
|      - | 5514 | `	PROCESS_INFORMATION pi;` |
|      5 | 5515 | `	WCHAR *zWideCmd = NULL;` |
|      5 | 5516 | `	FILE *pFile = NULL;` |
|      - | 5517 | `	int fd;` |
|      5 | 5518 | `	BOOL bRead = (zMode[0] == 'r');` |
|      - | 5519 |  |
|      - | 5520 | `	/* Set up security attributes for pipe inheritance */` |
|      5 | 5521 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|      5 | 5522 | `	sa.bInheritHandle = TRUE;` |
|      5 | 5523 | `	sa.lpSecurityDescriptor = NULL;` |
|      - | 5524 |  |
|      - | 5525 | `	/* Create pipes for child process I/O */` |
|      5 | 5526 | `	if( bRead ){` |
|      - | 5527 | `		/* Reading from child's stdout */` |
|      5 | 5528 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|    ! 0 | 5529 | `			return NULL;` |
|      - | 5530 | `		}` |
|      - | 5531 | `		/* Ensure read handle is not inherited */` |
|      5 | 5532 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|      5 | 5533 | `		hReadPipe = hChildStdoutRd;` |
|      5 | 5534 | `		*phPipe = hChildStdoutRd;` |
|      5 | 5535 | `	}else{` |
|      - | 5536 | `		/* Writing to child's stdin */` |
|    ! 0 | 5537 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|    ! 0 | 5538 | `			return NULL;` |
|      - | 5539 | `		}` |
|      - | 5540 | `		/* Ensure write handle is not inherited */` |
|    ! 0 | 5541 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|    ! 0 | 5542 | `		hWritePipe = hChildStdinWr;` |
|    ! 0 | 5543 | `		*phPipe = hChildStdinWr;` |
|      - | 5544 | `	}` |
|      - | 5545 |  |
|      - | 5546 | `	/* Convert command to wide string */` |
|      - | 5547 | `	{` |
|      5 | 5548 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|      5 | 5549 | `		if( nLen <= 0 ){` |
|    ! 0 | 5550 | `			goto cleanup_pipes;` |
|      - | 5551 | `		}` |
|      5 | 5552 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|      5 | 5553 | `		if( !zWideCmd ){` |
|    ! 0 | 5554 | `			goto cleanup_pipes;` |
|      - | 5555 | `		}` |
|      5 | 5556 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|      - | 5557 | `	}` |
|      - | 5558 |  |
|      - | 5559 | `	/* Set up process startup info */` |
|      5 | 5560 | `	ZeroMemory(&si, sizeof(si));` |
|      5 | 5561 | `	si.cb = sizeof(si);` |
|      5 | 5562 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|      5 | 5563 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|      5 | 5564 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|      5 | 5565 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|      5 | 5566 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|      - | 5567 |  |
|      5 | 5568 | `	ZeroMemory(&pi, sizeof(pi));` |
|      - | 5569 |  |
|      - | 5570 | `	/* Create the child process */` |
|      5 | 5571 | `	if( !CreateProcessW(` |
|      - | 5572 | `		NULL,           /* Application name */` |
|      - | 5573 | `		zWideCmd,       /* Command line */` |
|      - | 5574 | `		NULL,           /* Process security attributes */` |
|      - | 5575 | `		NULL,           /* Thread security attributes */` |
|      - | 5576 | `		TRUE,           /* Inherit handles */` |
|      - | 5577 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|      - | 5578 | `		NULL,           /* Environment */` |
|      - | 5579 | `		NULL,           /* Current directory */` |
|      - | 5580 | `		&si,            /* Startup info */` |
|      - | 5581 | `		&pi             /* Process info */` |
|      - | 5582 | `	)){` |
|    ! 0 | 5583 | `		goto cleanup_all;` |
|      - | 5584 | `	}` |
|      - | 5585 |  |
|      - | 5586 | `	/* Close handles we don't need in parent */` |
|      5 | 5587 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|      5 | 5588 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|      - | 5589 |  |
|      - | 5590 | `	/* Close thread handle (we only need process handle) */` |
|      5 | 5591 | `	CloseHandle(pi.hThread);` |
|      - | 5592 |  |
|      - | 5593 | `	/* Store process handle for later waiting */` |
|      5 | 5594 | `	*phProcess = pi.hProcess;` |
|      - | 5595 |  |
|      - | 5596 | `	/* Convert OS handle to C file descriptor, then to FILE* */` |
|      5 | 5597 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|      - | 5598 | `	                     bRead ? _O_RDONLY \| _O_TEXT : _O_WRONLY \| _O_TEXT);` |
|      5 | 5599 | `	if( fd == -1 ){` |
|    ! 0 | 5600 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 5601 | `		*phProcess = NULL;` |
|    ! 0 | 5602 | `		goto cleanup_all;` |
|      - | 5603 | `	}` |
|      - | 5604 |  |
|      5 | 5605 | `	pFile = _fdopen(fd, zMode);` |
|      5 | 5606 | `	if( !pFile ){` |
|    ! 0 | 5607 | `		_close(fd); /* This will also close the underlying handle */` |
|    ! 0 | 5608 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 5609 | `		*phProcess = NULL;` |
|    ! 0 | 5610 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    ! 0 | 5611 | `		return NULL;` |
|      - | 5612 | `	}` |
|      - | 5613 |  |
|      5 | 5614 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      5 | 5615 | `	return pFile;` |
|      - | 5616 |  |
|      - | 5617 | `cleanup_all:` |
|    ! 0 | 5618 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      - | 5619 | `cleanup_pipes:` |
|    ! 0 | 5620 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|    ! 0 | 5621 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|    ! 0 | 5622 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|    ! 0 | 5623 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|    ! 0 | 5624 | `	return NULL;` |
|      5 | 5625 | `}` |
|      - | 5626 |  |
|      - | 5627 | `/*` |
|      - | 5628 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|      - | 5629 | ` */` |
|      - | 5630 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|      5 | 5631 | `{` |
|      5 | 5632 | `	DWORD dwExitCode = 0;` |
|      - | 5633 | `	int status;` |
|      - | 5634 |  |
|      - | 5635 | `	/* Close the FILE* (this closes the pipe) */` |
|      5 | 5636 | `	fclose(pFile);` |
|      - | 5637 |  |
|      5 | 5638 | `	if( hProcess ){` |
|      - | 5639 | `		/* Wait for the process to complete */` |
|      5 | 5640 | `		WaitForSingleObject(hProcess, INFINITE);` |
|      - | 5641 |  |
|      5 | 5642 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|      5 | 5643 | `			status = (int)dwExitCode;` |
|      5 | 5644 | `		}else{` |
|    ! 0 | 5645 | `			status = -1;` |
|      - | 5646 | `		}` |
|      - | 5647 |  |
|      - | 5648 | `		/* Close process handle */` |
|      5 | 5649 | `		CloseHandle(hProcess);` |
|      5 | 5650 | `	}else{` |
|    ! 0 | 5651 | `		status = -1;` |
|      - | 5652 | `	}` |
|      - | 5653 |  |
|      5 | 5654 | `	return status;` |
|      5 | 5655 | `}` |
|      - | 5656 | `#endif /* __WINNT__ */` |
|      - | 5657 | `/*` |
|      - | 5658 | ` * Open a pipe to a process.` |
|      - | 5659 | ` * This is called internally by popen(), not through the stream device interface.` |
|      - | 5660 | ` */` |
|   3940 | 5661 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|      5 | 5662 | `{` |
|      - | 5663 | `	pipe_private *pPipe;` |
|      - | 5664 | `	FILE *pFile;` |
|   3945 | 5665 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|    ! 0 | 5666 | `		return 0;` |
|      - | 5667 | `	}` |
|      - | 5668 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|   3945 | 5669 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|    ! 0 | 5670 | `		return 0;` |
|      - | 5671 | `	}` |
|      - | 5672 | `	/* Open the pipe using system popen */` |
|      - | 5673 | `#ifdef __WINNT__` |
|      - | 5674 | `	{` |
|      - | 5675 | `		/* Build cmd.exe command wrapper */` |
|      5 | 5676 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|      5 | 5677 | `		const char *zShellSuffix = "\"";` |
|      5 | 5678 | `		size_t nPrefix = strlen(zShellPrefix);` |
|      5 | 5679 | `		size_t nSuffix = strlen(zShellSuffix);` |
|      5 | 5680 | `		size_t nCmd = strlen(zCommand);` |
|      5 | 5681 | `		size_t nQuotes = 0;` |
|      5 | 5682 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 5683 | `			if (zCommand[i] == '"') nQuotes++;` |
|      5 | 5684 | `		}` |
|      5 | 5685 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|      5 | 5686 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|      5 | 5687 | `		if (zCmdEsc == NULL) {` |
|    ! 0 | 5688 | `			return 0;` |
|      - | 5689 | `		}` |
|      - | 5690 | `		/* Escape quotes in command */` |
|      5 | 5691 | `		size_t j = 0;` |
|      5 | 5692 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 5693 | `			char ch = zCommand[i];` |
|      5 | 5694 | `			if (ch == '"') {` |
|      4 | 5695 | `				zCmdEsc[j++] = '^';` |
|      4 | 5696 | `				zCmdEsc[j++] = '"';` |
|      4 | 5697 | `			} else {` |
|      5 | 5698 | `				zCmdEsc[j++] = ch;` |
|      - | 5699 | `			}` |
|      5 | 5700 | `		}` |
|      5 | 5701 | `		zCmdEsc[j] = '\0';` |
|      5 | 5702 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|      5 | 5703 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|      5 | 5704 | `		if (zWinCmd == NULL) {` |
|    ! 0 | 5705 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 5706 | `			return 0;` |
|      - | 5707 | `		}` |
|      5 | 5708 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|      5 | 5709 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|      5 | 5710 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|      5 | 5711 | `		zWinCmd[nTotal - 1] = '\0';` |
|      - | 5712 | `		/* Allocate pipe structure early so we can store handles */` |
|      5 | 5713 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|      5 | 5714 | `		if( pPipe == 0 ){` |
|    ! 0 | 5715 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 5716 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|    ! 0 | 5717 | `			return 0;` |
|      - | 5718 | `		}` |
|      - | 5719 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|      5 | 5720 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|      5 | 5721 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|      5 | 5722 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|      5 | 5723 | `		if( pFile == 0 ){` |
|    ! 0 | 5724 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|    ! 0 | 5725 | `			return 0;` |
|      - | 5726 | `		}` |
|      - | 5727 | `		/* Initialize remaining fields */` |
|      5 | 5728 | `		pPipe->pFile = pFile;` |
|      5 | 5729 | `		pPipe->pVm = pVm;` |
|      5 | 5730 | `		pPipe->iMode = zMode[0];` |
|      - | 5731 | `	}` |
|      - | 5732 | `#elif defined(__UNIXES__) /* Unix */` |
|   3940 | 5733 | `	pFile = popen(zCommand, zMode);` |
|   3940 | 5734 | `	if( pFile == 0 ){` |
|    ! 0 | 5735 | `		return 0;` |
|      - | 5736 | `	}` |
|      - | 5737 | `	/* Allocate pipe private structure */` |
|   3940 | 5738 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|   3940 | 5739 | `	if( pPipe == 0 ){` |
|      - | 5740 | `		/* Out of memory, close the pipe */` |
|    ! 0 | 5741 | `		pclose(pFile);` |
|    ! 0 | 5742 | `		return 0;` |
|      - | 5743 | `	}` |
|      - | 5744 | `	/* Initialize the structure */` |
|   3940 | 5745 | `	pPipe->pFile = pFile;` |
|   3940 | 5746 | `	pPipe->pVm = pVm;` |
|   3940 | 5747 | `	pPipe->iMode = zMode[0];` |
|      - | 5748 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 5749 | `	(void)pFile;` |
|      - | 5750 | `	return 0;` |
|      - | 5751 | `#endif` |
|   3945 | 5752 | `	return pPipe;` |
|   1975 | 5753 | `}` |
|      - | 5754 | `/*` |
|      - | 5755 | ` * Close a pipe and return the exit status of the process.` |
|      - | 5756 | ` * Returns the exit status, or -1 on error.` |
|      - | 5757 | ` */` |
|   3918 | 5758 | `static int PipeClose(pipe_private *pPipe)` |
|      5 | 5759 | `{` |
|      - | 5760 | `	int status;` |
|      - | 5761 | `	ph7_vm *pVm;` |
|   3923 | 5762 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 5763 | `		return -1;` |
|      - | 5764 | `	}` |
|   3923 | 5765 | `	pVm = pPipe->pVm;` |
|      - | 5766 | `	/* Close the pipe and get exit status */` |
|      - | 5767 | `#ifdef __WINNT__` |
|      - | 5768 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|      5 | 5769 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|      - | 5770 | `#elif defined(__UNIXES__)` |
|   3918 | 5771 | `	status = pclose(pPipe->pFile);` |
|      - | 5772 | `	/* On Unix, pclose returns the status from waitpid, need to extract exit code */` |
|   3918 | 5773 | `	if( status != -1 ){` |
|   3918 | 5774 | `		if( WIFEXITED(status) ){` |
|   3918 | 5775 | `			status = WEXITSTATUS(status);` |
|   1959 | 5776 | `		}else if( WIFSIGNALED(status) ){` |
|      - | 5777 | `			/* Process was killed by a signal - use shell convention: 128 + signal number */` |
|    ! 0 | 5778 | `			status = 128 + WTERMSIG(status);` |
|    ! 0 | 5779 | `		}else{` |
|      - | 5780 | `			/* Unknown termination reason */` |
|    ! 0 | 5781 | `			status = -1;` |
|      - | 5782 | `		}` |
|   1959 | 5783 | `	}` |
|      - | 5784 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 5785 | `	status = -1;` |
|      - | 5786 | `#endif` |
|      - | 5787 | `	/* Free the structure */` |
|   3923 | 5788 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|   3923 | 5789 | `	return status;` |
|   1964 | 5790 | `}` |
|      - | 5791 | `/*` |
|      - | 5792 | ` * Pipe stream xClose implementation.` |
|      - | 5793 | ` * Note: This is called by fclose(), not pclose().` |
|      - | 5794 | ` * It closes the pipe but does not return the exit status.` |
|      - | 5795 | ` */` |
|     94 | 5796 | `static void PipeStream_Close(void *pHandle)` |
|      4 | 5797 | `{` |
|     98 | 5798 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|     98 | 5799 | `	if( pPipe ){` |
|     98 | 5800 | `		PipeClose(pPipe);` |
|     47 | 5801 | `	}` |
|     98 | 5802 | `}` |
|      - | 5803 | `/*` |
|      - | 5804 | ` * Pipe stream xRead implementation.` |
|      - | 5805 | ` */` |
|   5758 | 5806 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|      4 | 5807 | `{` |
|   5762 | 5808 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 5809 | `	size_t nRead;` |
|   5762 | 5810 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 5811 | `		return -1;` |
|      - | 5812 | `	}` |
|   5762 | 5813 | `	if( pPipe->iMode != 'r' ){` |
|      - | 5814 | `		/* Cannot read from a write-only pipe */` |
|    ! 0 | 5815 | `		return -1;` |
|      - | 5816 | `	}` |
|   5762 | 5817 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
|   5762 | 5818 | `	if( nRead == 0 ){` |
|   3942 | 5819 | `		if( feof(pPipe->pFile) ){` |
|   3942 | 5820 | `			return 0; /* EOF */` |
|      - | 5821 | `		}` |
|    ! 0 | 5822 | `		return -1; /* Error */` |
|      - | 5823 | `	}` |
|   1824 | 5824 | `	return (ph7_int64)nRead;` |
|   2883 | 5825 | `}` |
|      - | 5826 | `/*` |
|      - | 5827 | ` * Pipe stream xWrite implementation.` |
|      - | 5828 | ` */` |
|      2 | 5829 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|    ! 0 | 5830 | `{` |
|      2 | 5831 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 5832 | `	size_t nWritten;` |
|      2 | 5833 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 5834 | `		return -1;` |
|      - | 5835 | `	}` |
|      2 | 5836 | `	if( pPipe->iMode != 'w' ){` |
|      - | 5837 | `		/* Cannot write to a read-only pipe */` |
|    ! 0 | 5838 | `		return -1;` |
|      - | 5839 | `	}` |
|      2 | 5840 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|      2 | 5841 | `	if( nWritten == 0 && nWrite > 0 ){` |
|    ! 0 | 5842 | `		return -1; /* Error */` |
|      - | 5843 | `	}` |
|      2 | 5844 | `	return (ph7_int64)nWritten;` |
|      1 | 5845 | `}` |
|      - | 5846 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|      - | 5847 | `static const ph7_io_stream sPipe_Stream = {` |
|      - | 5848 | `	"pipe",` |
|      - | 5849 | `	PH7_IO_STREAM_VERSION,` |
|      - | 5850 | `	0,  /* xOpen - not used, pipes opened via PipeOpen() */` |
|      - | 5851 | `	0,  /* xOpenDir */` |
|      - | 5852 | `	PipeStream_Close,  /* xClose */` |
|      - | 5853 | `	0,  /* xCloseDir */` |
|      - | 5854 | `	PipeStream_Read,   /* xRead */` |
|      - | 5855 | `	0,  /* xReadDir */` |
|      - | 5856 | `	PipeStream_Write,  /* xWrite */` |
|      - | 5857 | `	0,  /* xSeek */` |
|      - | 5858 | `	0,  /* xLock */` |
|      - | 5859 | `	0,  /* xRewindDir */` |
|      - | 5860 | `	0,  /* xTell */` |
|      - | 5861 | `	0,  /* xTrunc */` |
|      - | 5862 | `	0,  /* xSync */` |
|      - | 5863 | `	0   /* xStat */` |
|      - | 5864 | `};` |
|      - | 5865 | `/*` |
|      - | 5866 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|      - | 5867 | ` * FALSE otherwise.` |
|      - | 5868 | ` */` |
|   3824 | 5869 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|      5 | 5870 | `{` |
|   3829 | 5871 | `	return pStream == &sPipe_Stream;` |
|      5 | 5872 | `}` |
|      - | 5873 | `/*` |
|      - | 5874 | ` * resource popen(string $command, string $mode)` |
|      - | 5875 | ` *  Opens process file pointer.` |
|      - | 5876 | ` * Parameters` |
|      - | 5877 | ` *  $command` |
|      - | 5878 | ` *   The command to execute. Passed to the system shell.` |
|      - | 5879 | ` *  $mode` |
|      - | 5880 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|      - | 5881 | ` *   'r' - Open for reading (read from the command's stdout).` |
|      - | 5882 | ` *   'w' - Open for writing (write to the command's stdin).` |
|      - | 5883 | ` * Return` |
|      - | 5884 | ` *  Returns a file pointer on success, or FALSE on error.` |
|      - | 5885 | ` */` |
|   3940 | 5886 | `static int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5887 | `{` |
|      - | 5888 | `	const char *zCommand, *zMode;` |
|      - | 5889 | `	pipe_private *pPipe;` |
|      - | 5890 | `	io_private *pDev;` |
|      - | 5891 | `	int nCmdLen, nModeLen;` |
|   3945 | 5892 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 5893 | `		/* Missing/Invalid arguments, return FALSE */` |
|    ! 0 | 5894 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a command string and mode");` |
|    ! 0 | 5895 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 5896 | `		return PH7_OK;` |
|      - | 5897 | `	}` |
|      - | 5898 | `	/* Extract the command and mode */` |
|   3945 | 5899 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|   3945 | 5900 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|   3945 | 5901 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 5902 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty command");` |
|    ! 0 | 5903 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 5904 | `		return PH7_OK;` |
|      - | 5905 | `	}` |
|   3945 | 5906 | `	if( nModeLen < 1 \|\| (zMode[0] != 'r' && zMode[0] != 'w') ){` |
|    ! 0 | 5907 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Invalid mode, expected 'r' or 'w'");` |
|    ! 0 | 5908 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 5909 | `		return PH7_OK;` |
|      - | 5910 | `	}` |
|      - | 5911 | `	/* Open the pipe */` |
|   3945 | 5912 | `	pPipe = PipeOpen(pCtx->pVm, zCommand, zMode);` |
|   3945 | 5913 | `	if( pPipe == 0 ){` |
|      - | 5914 | `		/* Failed to open pipe */` |
|    ! 0 | 5915 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 5916 | `		return PH7_OK;` |
|      - | 5917 | `	}` |
|      - | 5918 | `	/* Allocate an io_private instance to wrap the pipe */` |
|   3945 | 5919 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|   3945 | 5920 | `	if( pDev == 0 ){` |
|    ! 0 | 5921 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|    ! 0 | 5922 | `		PipeClose(pPipe);` |
|    ! 0 | 5923 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 5924 | `		return PH7_OK;` |
|      - | 5925 | `	}` |
|      - | 5926 | `	/* Initialize the io_private structure */` |
|   3945 | 5927 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|   3945 | 5928 | `	pDev->pHandle = pPipe;` |
|      - | 5929 | `	/* Return the io_private instance as a resource */` |
|   3945 | 5930 | `	ph7_result_resource(pCtx, pDev);` |
|   3945 | 5931 | `	return PH7_OK;` |
|   1975 | 5932 | `}` |
|      - | 5933 | `/*` |
|      - | 5934 | ` * int pclose(resource $handle)` |
|      - | 5935 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|      - | 5936 | ` * Parameters` |
|      - | 5937 | ` *  $handle` |
|      - | 5938 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|      - | 5939 | ` * Return` |
|      - | 5940 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|      - | 5941 | ` */` |
|   3824 | 5942 | `static int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5943 | `{` |
|      - | 5944 | `	const ph7_io_stream *pStream;` |
|      - | 5945 | `	pipe_private *pPipe;` |
|      - | 5946 | `	io_private *pDev;` |
|      - | 5947 | `	int status;` |
|   3829 | 5948 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5949 | `		/* Missing/Invalid arguments, return -1 */` |
|    ! 0 | 5950 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 5951 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 5952 | `		return PH7_OK;` |
|      - | 5953 | `	}` |
|      - | 5954 | `	/* Extract our private data */` |
|   3829 | 5955 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 5956 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   3829 | 5957 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 5958 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 5959 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 5960 | `		return PH7_OK;` |
|      - | 5961 | `	}` |
|      - | 5962 | `	/* Point to the target IO stream device */` |
|   3829 | 5963 | `	pStream = pDev->pStream;` |
|   3829 | 5964 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|    ! 0 | 5965 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|    ! 0 | 5966 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 5967 | `		return PH7_OK;` |
|      - | 5968 | `	}` |
|      - | 5969 | `	/* Get the pipe handle */` |
|   3829 | 5970 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|      - | 5971 | `	/* Close the pipe and get exit status */` |
|   3829 | 5972 | `	status = PipeClose(pPipe);` |
|      - | 5973 | `	/* Release the IO private structure */` |
|   3829 | 5974 | `	ReleaseIOPrivate(pCtx, pDev);` |
|      - | 5975 | `	/* Invalidate the resource handle */` |
|   3829 | 5976 | `	ph7_value_release(apArg[0]);` |
|      - | 5977 | `	/* Return the exit status */` |
|   3829 | 5978 | `	ph7_result_int(pCtx, status);` |
|   3829 | 5979 | `	return PH7_OK;` |
|   1917 | 5980 | `}` |
|      - | 5981 | `/* Export the php:// stream */` |
|      - | 5982 | `static const ph7_io_stream sPHP_Stream = {` |
|      - | 5983 | `	"php",` |
|      - | 5984 | `	PH7_IO_STREAM_VERSION,` |
|      - | 5985 | `	PHPStreamData_Open,  /* xOpen */` |
|      - | 5986 | `	0,   /* xOpenDir */` |
|      - | 5987 | `	PHPStreamData_Close, /* xClose */` |
|      - | 5988 | `	0,  /* xCloseDir */` |
|      - | 5989 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 5990 | `	0,  /* xReadDir */` |
|      - | 5991 | `	PHPStreamData_Write, /* xWrite */` |
|      - | 5992 | `	0,  /* xSeek */` |
|      - | 5993 | `	0,  /* xLock */` |
|      - | 5994 | `	0,  /* xRewindDir */` |
|      - | 5995 | `	0,  /* xTell */` |
|      - | 5996 | `	0,  /* xTrunc */` |
|      - | 5997 | `	0,  /* xSeek */` |
|      - | 5998 | `	0   /* xStat */` |
|      - | 5999 | `};` |
|      - | 6000 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 6001 | `/*` |
|      - | 6002 | ` * Return TRUE if we are dealing with the php:// stream.` |
|      - | 6003 | ` * FALSE otherwise.` |
|      - | 6004 | ` */` |
|     64 | 6005 | `static int is_php_stream(const ph7_io_stream *pStream)` |
|      2 | 6006 | `{` |
|      - | 6007 | `#ifndef PH7_DISABLE_DISK_IO` |
|     66 | 6008 | `	return pStream == &sPHP_Stream;` |
|      - | 6009 | `#else` |
|      - | 6010 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 6011 | `	return 0;` |
|      - | 6012 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      2 | 6013 | `}` |
|      - | 6014 |  |
|      - | 6015 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 6016 | `/*` |
|      - | 6017 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 6018 | ` * [i.e: file://,php://].` |
|      - | 6019 | ` * Note:` |
|      - | 6020 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 6021 | ` *  defined then this function is a no-op.` |
|      - | 6022 | ` */` |
|   3474 | 6023 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 6024 | `{` |
|      - | 6025 | `	/*` |
|      - | 6026 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 6027 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 6028 | `	 */` |
|      - | 6029 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 6030 | `	/* VFS: disk I/O related functions */` |
|      - | 6031 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 6032 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 6033 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 6034 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 6035 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 6036 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 6037 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 6038 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 6039 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 6040 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 6041 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 6042 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 6043 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 6044 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 6045 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 6046 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 6047 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 6048 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 6049 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 6050 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 6051 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 6052 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 6053 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 6054 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 6055 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 6056 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 6057 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 6058 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 6059 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 6060 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 6061 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 6062 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 6063 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 6064 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 6065 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 6066 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 6067 | `		{"link",        PH7_vfs_link     },` |
|      - | 6068 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 6069 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 6070 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 6071 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 6072 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 6073 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 6074 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 6075 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 6076 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 6077 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 6078 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 6079 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 6080 | `	};` |
|      - | 6081 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 6082 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 6083 | `	 */` |
|      - | 6084 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 6085 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 6086 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 6087 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 6088 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 6089 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 6090 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 6091 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 6092 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 6093 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 6094 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 6095 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 6096 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 6097 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 6098 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 6099 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 6100 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 6101 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 6102 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 6103 | `		{"file",      PH7_builtin_file   },` |
|      - | 6104 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 6105 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 6106 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 6107 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 6108 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 6109 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 6110 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 6111 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 6112 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 6113 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 6114 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 6115 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 6116 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 6117 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 6118 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 6119 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 6120 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 6121 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 6122 | `	};` |
|   3479 | 6123 | `	const ph7_io_stream *pFileStream = 0;` |
|   3479 | 6124 | `	sxu32 n = 0;` |
|      - | 6125 | `	/* Register disk-related functions */` |
| 170231 | 6126 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 166757 | 6127 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
|  83381 | 6128 | `	}` |
| 125069 | 6129 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 121595 | 6130 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  60800 | 6131 | `	}` |
|      - | 6132 | `#else` |
|      - | 6133 | `	SXUNUSED(pVm);` |
|      - | 6134 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 6135 |  |
|      - | 6136 | `	/*` |
|      - | 6137 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 6138 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 6139 | `	 */` |
|      - | 6140 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 6141 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 6142 | `		/* Path processing */` |
|      - | 6143 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 6144 | `		{"basename",    PH7_builtin_basename },` |
|      - | 6145 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 6146 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 6147 | `		{"fnmatch",     PH7_builtin_fnmatch  },` |
|      - | 6148 | `		/* ZIP processing */` |
|      - | 6149 | `		{"zip_open",    PH7_builtin_zip_open },` |
|      - | 6150 | `		{"zip_close",   PH7_builtin_zip_close},` |
|      - | 6151 | `		{"zip_read",    PH7_builtin_zip_read },` |
|      - | 6152 | `		{"zip_entry_open", PH7_builtin_zip_entry_open },` |
|      - | 6153 | `		{"zip_entry_close",PH7_builtin_zip_entry_close},` |
|      - | 6154 | `		{"zip_entry_name", PH7_builtin_zip_entry_name },` |
|      - | 6155 | `		{"zip_entry_filesize",      PH7_builtin_zip_entry_filesize       },` |
|      - | 6156 | `		{"zip_entry_compressedsize",PH7_builtin_zip_entry_compressedsize },` |
|      - | 6157 | `		{"zip_entry_read", PH7_builtin_zip_entry_read },` |
|      - | 6158 | `		{"zip_entry_reset_read_cursor",PH7_builtin_zip_entry_reset_read_cursor},` |
|      - | 6159 | `		{"zip_entry_compressionmethod",PH7_builtin_zip_entry_compressionmethod}` |
|      - | 6160 | `	};` |
|  59063 | 6161 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  55589 | 6162 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|  27797 | 6163 | `	}` |
|      - | 6164 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 6165 |  |
|      - | 6166 | `	/* Install streams if disk I/O is enabled */` |
|      - | 6167 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 6168 | `#ifdef __WINNT__` |
|      5 | 6169 | `	pFileStream = &sWinFileStream;` |
|      - | 6170 | `#elif defined(__UNIXES__)` |
|   3474 | 6171 | `	pFileStream = &sUnixFileStream;` |
|      - | 6172 | `#endif` |
|      - | 6173 | `	/* Install the php:// stream */` |
|   3479 | 6174 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   3479 | 6175 | `	if( pFileStream ){` |
|      - | 6176 | `		/* Install the file:// stream */` |
|   3479 | 6177 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   1737 | 6178 | `	}` |
|      - | 6179 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 6180 |  |
|   3479 | 6181 | `	return SXRET_OK;` |
|      5 | 6182 | `}` |
|      - | 6183 | `/*` |
|      - | 6184 | ` * Export the STDIN handle.` |
|      - | 6185 | ` */` |
|      2 | 6186 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|      1 | 6187 | `{` |
|      - | 6188 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 6189 | `	if( pVm->pStdin == 0  ){` |
|      - | 6190 | `		io_private *pIn;` |
|      - | 6191 | `		/* Allocate an IO private instance */` |
|      3 | 6192 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 6193 | `		if( pIn == 0 ){` |
|    ! 0 | 6194 | `			return 0;` |
|      - | 6195 | `		}` |
|      3 | 6196 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|      - | 6197 | `		/* Initialize the handle */` |
|      3 | 6198 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|      - | 6199 | `		/* Install the STDIN stream */` |
|      3 | 6200 | `		pVm->pStdin = pIn;` |
|      3 | 6201 | `		return pIn;` |
|    ! 0 | 6202 | `	}else{` |
|      - | 6203 | `		/* NULL or STDIN */` |
|    ! 0 | 6204 | `		return pVm->pStdin;` |
|      - | 6205 | `	}` |
|      - | 6206 | `#else` |
|      - | 6207 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 6208 | `	return 0;` |
|      - | 6209 | `#endif` |
|      2 | 6210 | `}` |
|      - | 6211 | `/*` |
|      - | 6212 | ` * Export the STDOUT handle.` |
|      - | 6213 | ` */` |
|      2 | 6214 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|      1 | 6215 | `{` |
|      - | 6216 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 6217 | `	if( pVm->pStdout == 0  ){` |
|      - | 6218 | `		io_private *pOut;` |
|      - | 6219 | `		/* Allocate an IO private instance */` |
|      3 | 6220 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 6221 | `		if( pOut == 0 ){` |
|    ! 0 | 6222 | `			return 0;` |
|      - | 6223 | `		}` |
|      3 | 6224 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|      - | 6225 | `		/* Initialize the handle */` |
|      3 | 6226 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|      - | 6227 | `		/* Install the STDOUT stream */` |
|      3 | 6228 | `		pVm->pStdout = pOut;` |
|      3 | 6229 | `		return pOut;` |
|    ! 0 | 6230 | `	}else{` |
|      - | 6231 | `		/* NULL or STDOUT */` |
|    ! 0 | 6232 | `		return pVm->pStdout;` |
|      - | 6233 | `	}` |
|      - | 6234 | `#else` |
|      - | 6235 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 6236 | `	return 0;` |
|      - | 6237 | `#endif` |
|      2 | 6238 | `}` |
|      - | 6239 | `/*` |
|      - | 6240 | ` * Export the STDERR handle.` |
|      - | 6241 | ` */` |
|      2 | 6242 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|      1 | 6243 | `{` |
|      - | 6244 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 6245 | `	if( pVm->pStderr == 0  ){` |
|      - | 6246 | `		io_private *pErr;` |
|      - | 6247 | `		/* Allocate an IO private instance */` |
|      3 | 6248 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 6249 | `		if( pErr == 0 ){` |
|    ! 0 | 6250 | `			return 0;` |
|      - | 6251 | `		}` |
|      3 | 6252 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|      - | 6253 | `		/* Initialize the handle */` |
|      3 | 6254 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|      - | 6255 | `		/* Install the STDERR stream */` |
|      3 | 6256 | `		pVm->pStderr = pErr;` |
|      3 | 6257 | `		return pErr;` |
|    ! 0 | 6258 | `	}else{` |
|      - | 6259 | `		/* NULL or STDERR */` |
|    ! 0 | 6260 | `		return pVm->pStderr;` |
|      - | 6261 | `	}` |
|      - | 6262 | `#else` |
|      - | 6263 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 6264 | `	return 0;` |
|      - | 6265 | `#endif` |
|      2 | 6266 | `}` |
|      - | 6267 |  |
