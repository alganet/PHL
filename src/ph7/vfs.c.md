# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1839/2834 lines (64.89%)

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
|     48 |   19 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)` |
|      5 |   20 | `{` |
|     53 |   21 | `	const char *zEnd = &zPath[nByte - 1];` |
|      - |   22 | `	int c,d;` |
|     53 |   23 | `	c = d = '/';` |
|      - |   24 | `#ifdef __WINNT__` |
|      5 |   25 | `	d = '\\';` |
|      - |   26 | `#endif` |
|   1029 |   27 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    957 |   28 | `		zEnd--;` |
|      5 |   29 | `	}` |
|     53 |   30 | `	*pLen = (int)(zEnd-zPath);` |
|      - |   31 | `#ifdef __WINNT__` |
|      5 |   32 | `	if( (*pLen) == (int)sizeof(char) && zPath[0] == '/' ){` |
|      - |   33 | `		/* Normalize path on windows */` |
|    ! 0 |   34 | `		return "\\";` |
|      - |   35 | `	}` |
|      - |   36 | `#endif` |
|     53 |   37 | `	if( zEnd == zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d) ){` |
|      - |   38 | `		/* No separator,return "." as the current directory */` |
|      5 |   39 | `		*pLen = sizeof(char);` |
|      5 |   40 | `		return ".";` |
|      - |   41 | `	}` |
|     49 |   42 | `	if( (*pLen) == 0 ){` |
|      2 |   43 | `		*pLen = sizeof(char);` |
|      - |   44 | `#ifdef __WINNT__` |
|    ! 0 |   45 | `		return "\\";` |
|      - |   46 | `#else` |
|      2 |   47 | `		return "/";` |
|      - |   48 | `#endif` |
|      - |   49 | `	}` |
|     47 |   50 | `	return zPath;` |
|     29 |   51 | `}` |
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
|  13710 |   66 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |   67 | `{` |
|      - |   68 | `	const char *zPath;` |
|      - |   69 | `	ph7_vfs *pVfs;` |
|      - |   70 | `	int rc;` |
|  13715 |   71 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |   72 | `		/* Missing/Invalid argument,return FALSE */` |
|      8 |   73 | `		ph7_result_bool(pCtx,0);` |
|      8 |   74 | `		return PH7_OK;` |
|      - |   75 | `	}` |
|      - |   76 | `	/* Point to the underlying vfs */` |
|  13709 |   77 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  13709 |   78 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |   79 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |   80 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |   81 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |   82 | `			ph7_function_name(pCtx)` |
|      - |   83 | `			);` |
|    ! 0 |   84 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |   85 | `		return PH7_OK;` |
|      - |   86 | `	}` |
|      - |   87 | `	/* Point to the desired directory */` |
|  13709 |   88 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |   89 | `	/* Perform the requested operation */` |
|  13709 |   90 | `	rc = pVfs->xChdir(zPath);` |
|      - |   91 | `	/* IO return value */` |
|  13709 |   92 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  13709 |   93 | `	return PH7_OK;` |
|   6860 |   94 | `}` |
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
|   8096 |  214 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  215 | `{` |
|      - |  216 | `	const char *zPath;` |
|      - |  217 | `	ph7_vfs *pVfs;` |
|      - |  218 | `	int rc;` |
|   8101 |  219 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  220 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  221 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  222 | `		return PH7_OK;` |
|      - |  223 | `	}` |
|      - |  224 | `	/* Point to the underlying vfs */` |
|   8101 |  225 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   8101 |  226 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  227 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  228 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  229 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  230 | `			ph7_function_name(pCtx)` |
|      - |  231 | `			);` |
|    ! 0 |  232 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  233 | `		return PH7_OK;` |
|      - |  234 | `	}` |
|      - |  235 | `	/* Point to the desired directory */` |
|   8101 |  236 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  237 | `	/* Perform the requested operation */` |
|   8101 |  238 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  239 | `	/* IO return value */` |
|   8101 |  240 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   8101 |  241 | `	return PH7_OK;` |
|   4053 |  242 | `}` |
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
|     10 |  353 | `static int PH7_vfs_realpath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  354 | `{` |
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
|      6 |  383 | `}` |
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
|  30928 |  477 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  478 | `{` |
|      - |  479 | `	const char *zPath;` |
|      - |  480 | `	ph7_vfs *pVfs;` |
|      - |  481 | `	int rc;` |
|  30933 |  482 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  483 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  484 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  485 | `		return PH7_OK;` |
|      - |  486 | `	}` |
|      - |  487 | `	/* Point to the underlying vfs */` |
|  30933 |  488 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  30933 |  489 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  490 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  491 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  492 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  493 | `			ph7_function_name(pCtx)` |
|      - |  494 | `			);` |
|    ! 0 |  495 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  496 | `		return PH7_OK;` |
|      - |  497 | `	}` |
|      - |  498 | `	/* Point to the desired directory */` |
|  30933 |  499 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  500 | `	/* Perform the requested operation */` |
|  30933 |  501 | `	rc = pVfs->xUnlink(zPath);` |
|      - |  502 | `	/* IO return value */` |
|  30933 |  503 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  30933 |  504 | `	return PH7_OK;` |
|  15469 |  505 | `}` |
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
|   6158 |  908 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  909 | `{` |
|      - |  910 | `	const char *zPath;` |
|      - |  911 | `	ph7_vfs *pVfs;` |
|      - |  912 | `	int rc;` |
|   6163 |  913 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  914 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  915 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  916 | `		return PH7_OK;` |
|      - |  917 | `	}` |
|      - |  918 | `	/* Point to the underlying vfs */` |
|   6163 |  919 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   6163 |  920 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - |  921 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  922 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  923 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  924 | `			ph7_function_name(pCtx)` |
|      - |  925 | `			);` |
|    ! 0 |  926 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  927 | `		return PH7_OK;` |
|      - |  928 | `	}` |
|      - |  929 | `	/* Point to the desired directory */` |
|   6163 |  930 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  931 | `	/* Perform the requested operation */` |
|   6163 |  932 | `	rc = pVfs->xIsfile(zPath);` |
|      - |  933 | `	/* IO return value */` |
|   6163 |  934 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   6163 |  935 | `	return PH7_OK;` |
|   3084 |  936 | `}` |
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
|      8 | 1022 | `static int PH7_vfs_is_writable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1023 | `{` |
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
|      5 | 1050 | `}` |
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
|     20 | 1485 | `static int PH7_builtin_basename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1486 | `{` |
|      - | 1487 | `	const char *zPath,*zBase,*zEnd;` |
|      - | 1488 | `	int c,d,iLen;` |
|     21 | 1489 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1490 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1491 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1492 | `		return PH7_OK;` |
|      - | 1493 | `	}` |
|     21 | 1494 | `	c = d = '/';` |
|      - | 1495 | `#ifdef __WINNT__` |
|      1 | 1496 | `	d = '\\';` |
|      - | 1497 | `#endif` |
|      - | 1498 | `	/* Point to the target path */` |
|     21 | 1499 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     21 | 1500 | `	if( iLen < 1 ){` |
|      - | 1501 | `		/* Empty string */` |
|      3 | 1502 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1503 | `		return PH7_OK;` |
|      - | 1504 | `	}` |
|      - | 1505 | `	/* Perform the requested operation */` |
|     19 | 1506 | `	zEnd = &zPath[iLen - 1];` |
|      - | 1507 | `	/* Ignore trailing '/' */` |
|     33 | 1508 | `	while( zEnd > zPath && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      6 | 1509 | `		zEnd--;` |
|      1 | 1510 | `	}` |
|     19 | 1511 | `	iLen = (int)(&zEnd[1]-zPath);` |
|    179 | 1512 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|    152 | 1513 | `		zEnd--;` |
|      1 | 1514 | `	}` |
|     19 | 1515 | `	zBase = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|     19 | 1516 | `	zEnd = &zPath[iLen];` |
|     19 | 1517 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 1518 | `		const char *zSuffix;` |
|      - | 1519 | `		int nSuffix;` |
|      - | 1520 | `		/* Strip suffix */` |
|      5 | 1521 | `		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);` |
|      5 | 1522 | `		if( nSuffix > 0 && nSuffix < iLen && SyMemcmp(&zEnd[-nSuffix],zSuffix,nSuffix) == 0 ){` |
|      5 | 1523 | `			zEnd -= nSuffix;` |
|      2 | 1524 | `		}` |
|      2 | 1525 | `	}` |
|      - | 1526 | `	/* Store the basename */` |
|     19 | 1527 | `	ph7_result_string(pCtx,zBase,(int)(zEnd-zBase));` |
|     19 | 1528 | `	return PH7_OK;` |
|     11 | 1529 | `}` |
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
|  12306 | 1555 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 1556 | `{` |
|  12311 | 1557 | `	const char *zPtr,*zEnd = &zPath[nByte - 1];` |
|      - | 1558 | `	SyString *pCur;` |
|      - | 1559 | `	int c,d;` |
|  12311 | 1560 | `	c = d = '/';` |
|      - | 1561 | `#ifdef __WINNT__` |
|      5 | 1562 | `	d = '\\';` |
|      - | 1563 | `#endif` |
|      - | 1564 | `	/* Zero the structure */` |
|  12311 | 1565 | `	SyZero(pOut,sizeof(path_info));` |
|      - | 1566 | `	/* Handle special case */` |
|  12311 | 1567 | `	if( nByte == sizeof(char) && ( (int)zPath[0] == c \|\| (int)zPath[0] == d ) ){` |
|      - | 1568 | `#ifdef __WINNT__` |
|    ! 0 | 1569 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1570 | `#else` |
|    ! 0 | 1571 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1572 | `#endif` |
|    ! 0 | 1573 | `		return SXRET_OK;` |
|      - | 1574 | `	}` |
|      - | 1575 | `	/* Extract the basename */` |
| 330576 | 1576 | `	while( zEnd > zPath && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
| 312117 | 1577 | `		zEnd--;` |
|      5 | 1578 | `	}` |
|  12311 | 1579 | `	zPtr = (zEnd > zPath) ? &zEnd[1] : zPath;` |
|  12311 | 1580 | `	zEnd = &zPath[nByte];` |
|      - | 1581 | `	/* dirname */` |
|  12311 | 1582 | `	pCur = &pOut->sDir;` |
|  12311 | 1583 | `	SyStringInitFromBuf(pCur,zPath,zPtr-zPath);` |
|  12311 | 1584 | `	if( pCur->nByte > 1 ){` |
|  24617 | 1585 | `		SyStringTrimTrailingChar(pCur,'/');` |
|      - | 1586 | `#ifdef __WINNT__` |
|      5 | 1587 | `		SyStringTrimTrailingChar(pCur,'\\');` |
|      - | 1588 | `#endif` |
|   6158 | 1589 | `	}else if( (int)zPath[0] == c \|\| (int)zPath[0] == d ){` |
|      - | 1590 | `#ifdef __WINNT__` |
|    ! 0 | 1591 | `		SyStringInitFromBuf(&pOut->sDir,"\\",sizeof(char));` |
|      - | 1592 | `#else` |
|    ! 0 | 1593 | `		SyStringInitFromBuf(&pOut->sDir,"/",sizeof(char));` |
|      - | 1594 | `#endif` |
|    ! 0 | 1595 | `	}` |
|      - | 1596 | `	/* basename/filename */` |
|  12311 | 1597 | `	pCur = &pOut->sBasename;` |
|  12311 | 1598 | `	SyStringInitFromBuf(pCur,zPtr,zEnd-zPtr);` |
|  12311 | 1599 | `	SyStringTrimLeadingChar(pCur,'/');` |
|      - | 1600 | `#ifdef __WINNT__` |
|      5 | 1601 | `	SyStringTrimLeadingChar(pCur,'\\');` |
|      - | 1602 | `#endif` |
|  12311 | 1603 | `	SyStringDupPtr(&pOut->sFilename,pCur);` |
|  12311 | 1604 | `	if( pCur->nByte > 0 ){` |
|      - | 1605 | `		/* extension */` |
|  12311 | 1606 | `		zEnd--;` |
|  61529 | 1607 | `		while( zEnd > pCur->zString /*basename*/ && zEnd[0] != '.' ){` |
|  49223 | 1608 | `			zEnd--;` |
|      5 | 1609 | `		}` |
|  12311 | 1610 | `		if( zEnd > pCur->zString ){` |
|  12309 | 1611 | `			zEnd++; /* Jump leading dot */` |
|  12309 | 1612 | `			SyStringInitFromBuf(&pOut->sExtension,zEnd,&zPath[nByte]-zEnd);` |
|      - | 1613 | `			/* Fix filename */` |
|  12309 | 1614 | `			pCur = &pOut->sFilename;` |
|  12309 | 1615 | `			if( pCur->nByte > SyStringLength(&pOut->sExtension) ){` |
|  12309 | 1616 | `				pCur->nByte -= 1 + SyStringLength(&pOut->sExtension);` |
|   6152 | 1617 | `			}` |
|   6152 | 1618 | `		}` |
|   6153 | 1619 | `	}` |
|  12311 | 1620 | `	return SXRET_OK;` |
|   6158 | 1621 | `}` |
|      - | 1622 | `/*` |
|      - | 1623 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1624 | ` *  See block comment above.` |
|      - | 1625 | ` */` |
|  12306 | 1626 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1627 | `{` |
|      - | 1628 | `	const char *zPath;` |
|      - | 1629 | `	path_info sInfo;` |
|      - | 1630 | `	SyString *pComp;` |
|      - | 1631 | `	int iLen;` |
|  12311 | 1632 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1633 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1634 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1635 | `		return PH7_OK;` |
|      - | 1636 | `	}` |
|      - | 1637 | `	/* Point to the target path */` |
|  12311 | 1638 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|  12311 | 1639 | `	if( iLen < 1 ){` |
|      - | 1640 | `		/* Empty string */` |
|    ! 0 | 1641 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1642 | `		return PH7_OK;` |
|      - | 1643 | `	}` |
|      - | 1644 | `	/* Extract path info */` |
|  12311 | 1645 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|  18463 | 1646 | `	if( nArg > 1 && ph7_value_is_int(apArg[1]) ){` |
|      - | 1647 | `		/* Return path component */` |
|  12309 | 1648 | `		int nComp = ph7_value_to_int(apArg[1]);` |
|  12309 | 1649 | `		switch(nComp){` |
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
|   3077 | 1668 | `		case 3: /*PATHINFO_EXTENSION*/` |
|   6159 | 1669 | `			pComp = &sInfo.sExtension;` |
|   6159 | 1670 | `			if( pComp->nByte > 0 ){` |
|   6157 | 1671 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3081 | 1672 | `			}else{` |
|      - | 1673 | `				/* Expand the empty string */` |
|      3 | 1674 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1675 | `			}` |
|   6159 | 1676 | `			break;` |
|   3073 | 1677 | `		case 4: /*PATHINFO_FILENAME*/` |
|   6151 | 1678 | `			pComp = &sInfo.sFilename;` |
|   6151 | 1679 | `			if( pComp->nByte > 0 ){` |
|   6151 | 1680 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|   3078 | 1681 | `			}else{` |
|      - | 1682 | `				/* Expand the empty string */` |
|    ! 0 | 1683 | `				ph7_result_string(pCtx,"",0);` |
|      - | 1684 | `			}` |
|   6151 | 1685 | `			break;` |
|    ! 0 | 1686 | `		default:` |
|      - | 1687 | `			/* Expand the empty string */` |
|    ! 0 | 1688 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1689 | `			break;` |
|      - | 1690 | `		}` |
|   6157 | 1691 | `	}else{` |
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
|  12311 | 1741 | `	return PH7_OK;` |
|   6158 | 1742 | `}` |
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
|      - | 1761 |  |
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
|      5 | 2141 | `{` |
|      - | 2142 | `	ph7_vfs *pVfs;` |
|      - | 2143 | `	/* Set the empty string as the default return value */` |
|    209 | 2144 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2145 | `	/* Point to the underlying vfs */` |
|    209 | 2146 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    209 | 2147 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
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
|    209 | 2158 | `	pVfs->xTempDir(pCtx);` |
|    209 | 2159 | `	return PH7_OK;` |
|    107 | 2160 | `}` |
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
|      - | 2440 | ` * bool ftruncate(resource $handle,int64 $size)` |
|      - | 2441 | ` *  Truncates a file to a given length.` |
|      - | 2442 | ` * Parameters` |
|      - | 2443 | ` *  $handle` |
|      - | 2444 | ` *   The file pointer.` |
|      - | 2445 | ` *   Note:` |
|      - | 2446 | ` *    The handle must be open for writing.` |
|      - | 2447 | ` * $size` |
|      - | 2448 | ` *   The size to truncate to.` |
|      - | 2449 | ` * Return` |
|      - | 2450 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2451 | ` */` |
|      6 | 2452 | `static int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2453 | `{` |
|      - | 2454 | `	const ph7_io_stream *pStream;` |
|      - | 2455 | `	io_private *pDev;` |
|      - | 2456 | `	int rc;` |
|      7 | 2457 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2458 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2459 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2460 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2461 | `		return PH7_OK;` |
|      - | 2462 | `	}` |
|      - | 2463 | `	/* Extract our private data */` |
|      7 | 2464 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2465 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 2466 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2467 | `		/*Expecting an IO handle */` |
|    ! 0 | 2468 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2469 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2470 | `		return PH7_OK;` |
|      - | 2471 | `	}` |
|      - | 2472 | `	/* Point to the target IO stream device */` |
|      7 | 2473 | `	pStream = pDev->pStream;` |
|      7 | 2474 | `	if( pStream == 0  \|\| pStream->xTrunc == 0){` |
|    ! 0 | 2475 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2476 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2477 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2478 | `			);` |
|    ! 0 | 2479 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2480 | `		return PH7_OK;` |
|      - | 2481 | `	}` |
|      - | 2482 | `	/* Perform the requested operation */` |
|      7 | 2483 | `	rc = pStream->xTrunc(pDev->pHandle,ph7_value_to_int64(apArg[1]));` |
|      7 | 2484 | `	if( rc == PH7_OK ){` |
|      - | 2485 | `		/* Discard buffered data */` |
|      7 | 2486 | `		ResetIOPrivate(pDev);` |
|      3 | 2487 | `	}` |
|      - | 2488 | `	/* IO result */` |
|      7 | 2489 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      7 | 2490 | `	return PH7_OK;` |
|      4 | 2491 | `}` |
|      - | 2492 | `/*` |
|      - | 2493 | ` * int fseek(resource $handle,int $offset[,int $whence = SEEK_SET ])` |
|      - | 2494 | ` *  Seeks on a file pointer.` |
|      - | 2495 | ` * Parameters` |
|      - | 2496 | ` *  $handle` |
|      - | 2497 | ` *   A file system pointer resource that is typically created using fopen().` |
|      - | 2498 | ` * $offset` |
|      - | 2499 | ` *   The offset.` |
|      - | 2500 | ` *   To move to a position before the end-of-file, you need to pass a negative` |
|      - | 2501 | ` *   value in offset and set whence to SEEK_END.` |
|      - | 2502 | ` *   whence` |
|      - | 2503 | ` *   whence values are:` |
|      - | 2504 | ` *    SEEK_SET - Set position equal to offset bytes.` |
|      - | 2505 | ` *    SEEK_CUR - Set position to current location plus offset.` |
|      - | 2506 | ` *    SEEK_END - Set position to end-of-file plus offset.` |
|      - | 2507 | ` * Return` |
|      - | 2508 | ` *  0 on success,-1 on failure` |
|      - | 2509 | ` */` |
|      2 | 2510 | `static int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2511 | `{` |
|      - | 2512 | `	const ph7_io_stream *pStream;` |
|      - | 2513 | `	io_private *pDev;` |
|      - | 2514 | `	ph7_int64 iOfft;` |
|      - | 2515 | `	int whence;` |
|      - | 2516 | `	int rc;` |
|      3 | 2517 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2518 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2519 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2520 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2521 | `		return PH7_OK;` |
|      - | 2522 | `	}` |
|      - | 2523 | `	/* Extract our private data */` |
|      3 | 2524 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2525 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 2526 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2527 | `		/*Expecting an IO handle */` |
|    ! 0 | 2528 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2529 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2530 | `		return PH7_OK;` |
|      - | 2531 | `	}` |
|      - | 2532 | `	/* Point to the target IO stream device */` |
|      3 | 2533 | `	pStream = pDev->pStream;` |
|      3 | 2534 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2535 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2536 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 2537 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2538 | `			);` |
|    ! 0 | 2539 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2540 | `		return PH7_OK;` |
|      - | 2541 | `	}` |
|      - | 2542 | `	/* Extract the offset */` |
|      3 | 2543 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|      3 | 2544 | `	whence = 0;/* SEEK_SET */` |
|      3 | 2545 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|    ! 0 | 2546 | `		whence = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2547 | `	}` |
|      - | 2548 | `	/* Perform the requested operation */` |
|      3 | 2549 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|      3 | 2550 | `	if( rc == PH7_OK ){` |
|      - | 2551 | `		/* Ignore buffered data */` |
|      3 | 2552 | `		ResetIOPrivate(pDev);` |
|      1 | 2553 | `	}` |
|      - | 2554 | `	/* IO result */` |
|      3 | 2555 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|      3 | 2556 | `	return PH7_OK;` |
|      2 | 2557 | `}` |
|      - | 2558 | `/*` |
|      - | 2559 | ` * int64 ftell(resource $handle)` |
|      - | 2560 | ` *  Returns the current position of the file read/write pointer.` |
|      - | 2561 | ` * Parameters` |
|      - | 2562 | ` *  $handle` |
|      - | 2563 | ` *   The file pointer.` |
|      - | 2564 | ` * Return` |
|      - | 2565 | ` *  Returns the position of the file pointer referenced by handle` |
|      - | 2566 | ` *  as an integer; i.e., its offset into the file stream.` |
|      - | 2567 | ` *  FALSE is returned on failure.` |
|      - | 2568 | ` */` |
|      6 | 2569 | `static int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2570 | `{` |
|      - | 2571 | `	const ph7_io_stream *pStream;` |
|      - | 2572 | `	io_private *pDev;` |
|      - | 2573 | `	ph7_int64 iOfft;` |
|      7 | 2574 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2575 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2576 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2577 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2578 | `		return PH7_OK;` |
|      - | 2579 | `	}` |
|      - | 2580 | `	/* Extract our private data */` |
|      7 | 2581 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2582 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 2583 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2584 | `		/*Expecting an IO handle */` |
|    ! 0 | 2585 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2586 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2587 | `		return PH7_OK;` |
|      - | 2588 | `	}` |
|      - | 2589 | `	/* Point to the target IO stream device */` |
|      7 | 2590 | `	pStream = pDev->pStream;` |
|      7 | 2591 | `	if( pStream == 0  \|\| pStream->xTell == 0){` |
|    ! 0 | 2592 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2593 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2594 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2595 | `			);` |
|    ! 0 | 2596 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2597 | `		return PH7_OK;` |
|      - | 2598 | `	}` |
|      - | 2599 | `	/* Perform the requested operation */` |
|      7 | 2600 | `	iOfft = pStream->xTell(pDev->pHandle);` |
|      - | 2601 | `	/* IO result */` |
|      7 | 2602 | `	ph7_result_int64(pCtx,iOfft);` |
|      7 | 2603 | `	return PH7_OK;` |
|      4 | 2604 | `}` |
|      - | 2605 | `/*` |
|      - | 2606 | ` * bool rewind(resource $handle)` |
|      - | 2607 | ` *  Rewind the position of a file pointer.` |
|      - | 2608 | ` * Parameters` |
|      - | 2609 | ` *  $handle` |
|      - | 2610 | ` *   The file pointer.` |
|      - | 2611 | ` * Return` |
|      - | 2612 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2613 | ` */` |
|      4 | 2614 | `static int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2615 | `{` |
|      - | 2616 | `	const ph7_io_stream *pStream;` |
|      - | 2617 | `	io_private *pDev;` |
|      - | 2618 | `	int rc;` |
|      5 | 2619 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2620 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2621 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2622 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2623 | `		return PH7_OK;` |
|      - | 2624 | `	}` |
|      - | 2625 | `	/* Extract our private data */` |
|      5 | 2626 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2627 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 2628 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2629 | `		/*Expecting an IO handle */` |
|    ! 0 | 2630 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2631 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2632 | `		return PH7_OK;` |
|      - | 2633 | `	}` |
|      - | 2634 | `	/* Point to the target IO stream device */` |
|      5 | 2635 | `	pStream = pDev->pStream;` |
|      5 | 2636 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 | 2637 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2638 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2639 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2640 | `			);` |
|    ! 0 | 2641 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2642 | `		return PH7_OK;` |
|      - | 2643 | `	}` |
|      - | 2644 | `	/* Perform the requested operation */` |
|      5 | 2645 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|      5 | 2646 | `	if( rc == PH7_OK ){` |
|      - | 2647 | `		/* Ignore buffered data */` |
|      5 | 2648 | `		ResetIOPrivate(pDev);` |
|      2 | 2649 | `	}` |
|      - | 2650 | `	/* IO result */` |
|      5 | 2651 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 2652 | `	return PH7_OK;` |
|      3 | 2653 | `}` |
|      - | 2654 | `/*` |
|      - | 2655 | ` * bool fflush(resource $handle)` |
|      - | 2656 | ` *  Flushes the output to a file.` |
|      - | 2657 | ` * Parameters` |
|      - | 2658 | ` *  $handle` |
|      - | 2659 | ` *   The file pointer.` |
|      - | 2660 | ` * Return` |
|      - | 2661 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2662 | ` */` |
|      2 | 2663 | `static int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2664 | `{` |
|      - | 2665 | `	const ph7_io_stream *pStream;` |
|      - | 2666 | `	io_private *pDev;` |
|      - | 2667 | `	int rc;` |
|      3 | 2668 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2669 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2670 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2671 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2672 | `		return PH7_OK;` |
|      - | 2673 | `	}` |
|      - | 2674 | `	/* Extract our private data */` |
|      3 | 2675 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2676 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 2677 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2678 | `		/*Expecting an IO handle */` |
|    ! 0 | 2679 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2680 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2681 | `		return PH7_OK;` |
|      - | 2682 | `	}` |
|      - | 2683 | `	/* Point to the target IO stream device */` |
|      3 | 2684 | `	pStream = pDev->pStream;` |
|      3 | 2685 | `	if( pStream == 0 \|\| pStream->xSync == 0){` |
|    ! 0 | 2686 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2687 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2688 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2689 | `			);` |
|    ! 0 | 2690 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2691 | `		return PH7_OK;` |
|      - | 2692 | `	}` |
|      - | 2693 | `	/* Perform the requested operation */` |
|      3 | 2694 | `	rc = pStream->xSync(pDev->pHandle);` |
|      - | 2695 | `	/* IO result */` |
|      3 | 2696 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 2697 | `	return PH7_OK;` |
|      2 | 2698 | `}` |
|      - | 2699 | `/*` |
|      - | 2700 | ` * bool feof(resource $handle)` |
|      - | 2701 | ` *  Tests for end-of-file on a file pointer.` |
|      - | 2702 | ` * Parameters` |
|      - | 2703 | ` *  $handle` |
|      - | 2704 | ` *   The file pointer.` |
|      - | 2705 | ` * Return` |
|      - | 2706 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|      - | 2707 | ` */` |
|  10128 | 2708 | `static int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2709 | `{` |
|      - | 2710 | `	const ph7_io_stream *pStream;` |
|      - | 2711 | `	io_private *pDev;` |
|      - | 2712 | `	int rc;` |
|  10133 | 2713 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2714 | `		/* Missing/Invalid arguments */` |
|    ! 0 | 2715 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2716 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2717 | `		return PH7_OK;` |
|      - | 2718 | `	}` |
|      - | 2719 | `	/* Extract our private data */` |
|  10133 | 2720 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2721 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  10133 | 2722 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2723 | `		/*Expecting an IO handle */` |
|    ! 0 | 2724 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2725 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2726 | `		return PH7_OK;` |
|      - | 2727 | `	}` |
|      - | 2728 | `	/* Point to the target IO stream device */` |
|  10133 | 2729 | `	pStream = pDev->pStream;` |
|  10133 | 2730 | `	if( pStream == 0 ){` |
|    ! 0 | 2731 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2732 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2733 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2734 | `			);` |
|    ! 0 | 2735 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 | 2736 | `		return PH7_OK;` |
|      - | 2737 | `	}` |
|  10133 | 2738 | `	rc = SXERR_EOF;` |
|      - | 2739 | `	/* Perform the requested operation */` |
|  10133 | 2740 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2741 | `		/* Data is available */` |
|   4597 | 2742 | `		rc = PH7_OK;` |
|   2301 | 2743 | `	}else{` |
|      - | 2744 | `		char zBuf[4096];` |
|      - | 2745 | `		ph7_int64 n;` |
|      - | 2746 | `		/* Perform a buffered read */` |
|   5541 | 2747 | `		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));` |
|   5541 | 2748 | `		if( n > 0 ){` |
|      - | 2749 | `			/* Copy buffered data */` |
|   1739 | 2750 | `			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|   1739 | 2751 | `			rc = PH7_OK;` |
|    867 | 2752 | `		}` |
|      - | 2753 | `	}` |
|      - | 2754 | `	/* EOF or not */` |
|  10133 | 2755 | `	ph7_result_bool(pCtx,rc == SXERR_EOF);` |
|  10133 | 2756 | `	return PH7_OK;` |
|   5069 | 2757 | `}` |
|      - | 2758 | `/*` |
|      - | 2759 | ` * Read n bytes from the underlying IO stream device.` |
|      - | 2760 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|      - | 2761 | ` * [i.e: IO error ] or EOF.` |
|      - | 2762 | ` */` |
|     18 | 2763 | `static ph7_int64 StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      2 | 2764 | `{` |
|     20 | 2765 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|     20 | 2766 | `	char *zBuf = (char *)pBuf;` |
|      - | 2767 | `	ph7_int64 n,nRead;` |
|     20 | 2768 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     20 | 2769 | `	if( n > 0 ){` |
|    ! 0 | 2770 | `		if( n > nLen ){` |
|    ! 0 | 2771 | `			n = nLen;` |
|    ! 0 | 2772 | `		}` |
|      - | 2773 | `		/* Copy the buffered data */` |
|    ! 0 | 2774 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|      - | 2775 | `		/* Update the read offset */` |
|    ! 0 | 2776 | `		pDev->nOfft += (sxu32)n;` |
|    ! 0 | 2777 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2778 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|    ! 0 | 2779 | `			SyBlobReset(&pDev->sBuffer);` |
|    ! 0 | 2780 | `			pDev->nOfft = 0;` |
|    ! 0 | 2781 | `		}` |
|    ! 0 | 2782 | `		nLen -= n;` |
|    ! 0 | 2783 | `		if( nLen < 1 ){` |
|      - | 2784 | `			/* All done */` |
|    ! 0 | 2785 | `			return n;` |
|      - | 2786 | `		}` |
|      - | 2787 | `		/* Advance the cursor */` |
|    ! 0 | 2788 | `		zBuf += n;` |
|    ! 0 | 2789 | `	}` |
|      - | 2790 | `	/* Read without buffering */` |
|     20 | 2791 | `	nRead = pStream->xRead(pDev->pHandle,zBuf,nLen);` |
|     20 | 2792 | `	if( nRead > 0 ){` |
|     18 | 2793 | `		n += nRead;` |
|     11 | 2794 | `	}else if( n < 1 ){` |
|      - | 2795 | `		/* EOF or IO error */` |
|      3 | 2796 | `		return nRead;` |
|      - | 2797 | `	}` |
|     18 | 2798 | `	return n;` |
|     11 | 2799 | `}` |
|      - | 2800 | `/*` |
|      - | 2801 | ` * Extract a single line from the buffered input.` |
|      - | 2802 | ` */` |
|   6386 | 2803 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      5 | 2804 | `{` |
|      - | 2805 | `	const char *zIn,*zEnd,*zPtr;` |
|   6391 | 2806 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|   6391 | 2807 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|   6391 | 2808 | `	zPtr = zIn;` |
| 377834 | 2809 | `	while( zIn < zEnd ){` |
| 377752 | 2810 | `		if( zIn[0] == '\n' ){` |
|      - | 2811 | `			/* Line found */` |
|   6309 | 2812 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|   6309 | 2813 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|   6309 | 2814 | `			*pzLine = zPtr;` |
|   6309 | 2815 | `			return SXRET_OK;` |
|      - | 2816 | `		}` |
| 371448 | 2817 | `		zIn++;` |
|      5 | 2818 | `	}` |
|      - | 2819 | `	/* No line were found */` |
|     87 | 2820 | `	return SXERR_NOTFOUND;` |
|   3198 | 2821 | `}` |
|      - | 2822 | `/*` |
|      - | 2823 | ` * Read a single line from the underlying IO stream device.` |
|      - | 2824 | ` */` |
|   6390 | 2825 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      5 | 2826 | `{` |
|   6395 | 2827 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|      - | 2828 | `	char zBuf[8192];` |
|      - | 2829 | `	ph7_int64 n;` |
|      - | 2830 | `	sxi32 rc;` |
|   6395 | 2831 | `	n = 0;` |
|   6395 | 2832 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 2833 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     65 | 2834 | `		SyBlobReset(&pDev->sBuffer);` |
|     65 | 2835 | `		pDev->nOfft = 0;` |
|     30 | 2836 | `	}` |
|   6395 | 2837 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2838 | `		/* Check if there is a line */` |
|   6335 | 2839 | `		rc = GetLine(pDev,&n,pzData);` |
|   6335 | 2840 | `		if( rc == SXRET_OK ){` |
|      - | 2841 | `			/* Got line,update the cursor  */` |
|   6255 | 2842 | `			pDev->nOfft += (sxu32)n;` |
|   6255 | 2843 | `			return n;` |
|      - | 2844 | `		}` |
|     40 | 2845 | `	}` |
|      - | 2846 | `	/* Perform the read operation until a new line is extracted or length` |
|      - | 2847 | `	 * limit is reached.` |
|      - | 2848 | `	 */` |
|     71 | 2849 | `	for(;;){` |
|    147 | 2850 | `		n = pStream->xRead(pDev->pHandle,zBuf, (nMaxLen > 0 && nMaxLen < (ph7_int64)sizeof(zBuf)) ? nMaxLen : (ph7_int64)sizeof(zBuf));` |
|    147 | 2851 | `		if( n < 1 ){` |
|      - | 2852 | `			/* EOF or IO error */` |
|     91 | 2853 | `			break;` |
|      - | 2854 | `		}` |
|      - | 2855 | `		/* Append the data just read */` |
|     59 | 2856 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - | 2857 | `		/* Try to extract a line */` |
|     59 | 2858 | `		rc = GetLine(pDev,&n,pzData);` |
|     59 | 2859 | `		if( rc == SXRET_OK ){` |
|      - | 2860 | `			/* Got one,return immediately */` |
|     57 | 2861 | `			pDev->nOfft += (sxu32)n;` |
|     57 | 2862 | `			return n;` |
|      - | 2863 | `		}` |
|      3 | 2864 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
|      - | 2865 | `			/* Read limit reached,return the available data */` |
|    ! 0 | 2866 | `			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    ! 0 | 2867 | `			n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 2868 | `			/* Reset the working buffer */` |
|    ! 0 | 2869 | `			SyBlobReset(&pDev->sBuffer);` |
|    ! 0 | 2870 | `			pDev->nOfft = 0;` |
|    ! 0 | 2871 | `			return n;` |
|      - | 2872 | `		}` |
|      1 | 2873 | `	}` |
|     91 | 2874 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - | 2875 | `		/* Read limit reached,return the available data */` |
|     87 | 2876 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|     87 | 2877 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - | 2878 | `		/* Reset the working buffer */` |
|     87 | 2879 | `		SyBlobReset(&pDev->sBuffer);` |
|     87 | 2880 | `		pDev->nOfft = 0;` |
|     41 | 2881 | `	}` |
|     91 | 2882 | `	return n;` |
|   3200 | 2883 | `}` |
|      - | 2884 | `/*` |
|      - | 2885 | ` * Open an IO stream handle.` |
|      - | 2886 | ` * Notes on stream:` |
|      - | 2887 | ` * According to the PHP reference manual.` |
|      - | 2888 | ` * In its simplest definition, a stream is a resource object which exhibits streamable behavior.` |
|      - | 2889 | ` * That is, it can be read from or written to in a linear fashion, and may be able to fseek()` |
|      - | 2890 | ` * to an arbitrary locations within the stream.` |
|      - | 2891 | ` * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.` |
|      - | 2892 | ` * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file` |
|      - | 2893 | ` * on a remote server.` |
|      - | 2894 | ` * A stream is referenced as: scheme://target` |
|      - | 2895 | ` *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...` |
|      - | 2896 | ` *   If no wrapper is specified, the function default is used (typically file://).` |
|      - | 2897 | ` *   target - Depends on the wrapper used. For filesystem related streams this is typically a path` |
|      - | 2898 | ` *  and filename of the desired file. For network related streams this is typically a hostname, often` |
|      - | 2899 | ` *  with a path appended.` |
|      - | 2900 | ` *` |
|      - | 2901 | ` * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.` |
|      - | 2902 | ` * Please refer to the official documentation for a full discussion.` |
|      - | 2903 | ` * This function return a handle on success. Otherwise null.` |
|      - | 2904 | ` */` |
|  30214 | 2905 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - | 2906 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)` |
|      5 | 2907 | `{` |
|  30219 | 2908 | `	void *pHandle = 0; /* cc warning */` |
|      - | 2909 | `	SyString sFile;` |
|      - | 2910 | `	int rc;` |
|  30219 | 2911 | `	if( pStream == 0 ){` |
|      - | 2912 | `		/* No such stream device */` |
|    ! 0 | 2913 | `		return 0;` |
|      - | 2914 | `	}` |
|  30219 | 2915 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  30219 | 2916 | `	if( use_include ){` |
|   9992 | 2917 | `		if(	sFile.zString[0] == '/' \|\|` |
|      - | 2918 | `#ifdef __WINNT__` |
|      - | 2919 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - | 2920 | `#endif` |
|   9978 | 2921 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|   9974 | 2922 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - | 2923 | `				/*  Open the file directly */` |
|     19 | 2924 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|     10 | 2925 | `		}else{` |
|      - | 2926 | `			SyString *pPath;` |
|      - | 2927 | `			SyBlob sWorker;` |
|      - | 2928 | `#ifdef __WINNT__` |
|      - | 2929 | `			static const int c = '\\';` |
|      - | 2930 | `#else` |
|      - | 2931 | `			static const int c = '/';` |
|      - | 2932 | `#endif` |
|      - | 2933 | `			/* Init the path builder working buffer */` |
|   9978 | 2934 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - | 2935 | `			/* Build a path from the set of include path */` |
|   9978 | 2936 | `			SySetResetCursor(&pVm->aPaths);` |
|   9978 | 2937 | `			rc = SXERR_IO;` |
|   9984 | 2938 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - | 2939 | `				/* Build full path */` |
|   9978 | 2940 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - | 2941 | `				/* Append null terminator */` |
|   9978 | 2942 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 2943 | `					continue;` |
|      - | 2944 | `				}` |
|      - | 2945 | `				/* Try to open the file */` |
|   9978 | 2946 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|   9978 | 2947 | `				if( rc == PH7_OK ){` |
|   9972 | 2948 | `					if( bPushInclude ){` |
|      - | 2949 | `						/* Mark as included */` |
|   9972 | 2950 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   4984 | 2951 | `					}` |
|   9972 | 2952 | `					break;` |
|      - | 2953 | `				}` |
|      - | 2954 | `				/* Reset the working buffer */` |
|      8 | 2955 | `				SyBlobReset(&sWorker);` |
|      - | 2956 | `				/* Check the next path */` |
|      2 | 2957 | `			}` |
|   9978 | 2958 | `			SyBlobRelease(&sWorker);` |
|      - | 2959 | `		}` |
|   9996 | 2960 | `		if( rc == PH7_OK ){` |
|   9990 | 2961 | `			if( bPushInclude ){` |
|      - | 2962 | `				/* Mark as included */` |
|   9990 | 2963 | `				PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|   4993 | 2964 | `			}` |
|   4993 | 2965 | `		}` |
|   5000 | 2966 | `	}else{` |
|      - | 2967 | `		/* Open the URI direcly */` |
|  20227 | 2968 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - | 2969 | `	}` |
|  30219 | 2970 | `	if( rc != PH7_OK ){` |
|      - | 2971 | `		/* IO error */` |
|     15 | 2972 | `		return 0;` |
|      - | 2973 | `	}` |
|      - | 2974 | `	/* Return the file handle */` |
|  30207 | 2975 | `	return pHandle;` |
|  15112 | 2976 | `}` |
|      - | 2977 | `/*` |
|      - | 2978 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - | 2979 | ` * Store the read data in the given BLOB (last argument).` |
|      - | 2980 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - | 2981 | ` */` |
|   9980 | 2982 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      4 | 2983 | `{` |
|      - | 2984 | `	ph7_int64 nRead;` |
|      - | 2985 | `	char zBuf[8192]; /* 8K */` |
|      - | 2986 | `	int rc;` |
|      - | 2987 | `	/* Perform the requested operation */` |
|   9980 | 2988 | `	for(;;){` |
|  19964 | 2989 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  19964 | 2990 | `		if( nRead < 1 ){` |
|      - | 2991 | `			/* EOF or IO error */` |
|   9984 | 2992 | `			break;` |
|      - | 2993 | `		}` |
|      - | 2994 | `		/* Append contents */` |
|   9984 | 2995 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|   9984 | 2996 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 2997 | `			break;` |
|      - | 2998 | `		}` |
|      4 | 2999 | `	}` |
|   9984 | 3000 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      4 | 3001 | `}` |
|      - | 3002 | `/*` |
|      - | 3003 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - | 3004 | ` */` |
|  30294 | 3005 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      5 | 3006 | `{` |
|  30299 | 3007 | `	if( pStream->xClose ){` |
|  30299 | 3008 | `		pStream->xClose(pHandle);` |
|  15147 | 3009 | `	}` |
|  30299 | 3010 | `}` |
|      - | 3011 | `/*` |
|      - | 3012 | ` * string fgetc(resource $handle)` |
|      - | 3013 | ` *  Gets a character from the given file pointer.` |
|      - | 3014 | ` * Parameters` |
|      - | 3015 | ` *  $handle` |
|      - | 3016 | ` *   The file pointer.` |
|      - | 3017 | ` * Return` |
|      - | 3018 | ` *  Returns a string containing a single character read from the file` |
|      - | 3019 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|      - | 3020 | ` * WARNING` |
|      - | 3021 | ` *  This operation is extremely slow.Avoid using it.` |
|      - | 3022 | ` */` |
|      4 | 3023 | `static int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3024 | `{` |
|      - | 3025 | `	const ph7_io_stream *pStream;` |
|      - | 3026 | `	io_private *pDev;` |
|      - | 3027 | `	int c,n;` |
|      5 | 3028 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3029 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3030 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3031 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3032 | `		return PH7_OK;` |
|      - | 3033 | `	}` |
|      - | 3034 | `	/* Extract our private data */` |
|      5 | 3035 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3036 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 3037 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3038 | `		/*Expecting an IO handle */` |
|    ! 0 | 3039 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3040 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3041 | `		return PH7_OK;` |
|      - | 3042 | `	}` |
|      - | 3043 | `	/* Point to the target IO stream device */` |
|      5 | 3044 | `	pStream = pDev->pStream;` |
|      5 | 3045 | `	if( pStream == 0  ){` |
|    ! 0 | 3046 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3047 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3048 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3049 | `			);` |
|    ! 0 | 3050 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3051 | `		return PH7_OK;` |
|      - | 3052 | `	}` |
|      - | 3053 | `	/* Perform the requested operation */` |
|      5 | 3054 | `	n = (int)StreamRead(pDev,(void *)&c,sizeof(char));` |
|      - | 3055 | `	/* IO result */` |
|      5 | 3056 | `	if( n < 1 ){` |
|      - | 3057 | `		/* EOF or error,return FALSE */` |
|    ! 0 | 3058 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3059 | `	}else{` |
|      - | 3060 | `		/* Return the string holding the character */` |
|      5 | 3061 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|      - | 3062 | `	}` |
|      5 | 3063 | `	return PH7_OK;` |
|      3 | 3064 | `}` |
|      - | 3065 | `/*` |
|      - | 3066 | ` * string fgets(resource $handle[,int64 $length ])` |
|      - | 3067 | ` *  Gets line from file pointer.` |
|      - | 3068 | ` * Parameters` |
|      - | 3069 | ` *  $handle` |
|      - | 3070 | ` *   The file pointer.` |
|      - | 3071 | ` * $length` |
|      - | 3072 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3073 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3074 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3075 | ` *  the end of the line.` |
|      - | 3076 | ` * Return` |
|      - | 3077 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3078 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3079 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3080 | ` */` |
|   6380 | 3081 | `static int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3082 | `{` |
|      - | 3083 | `	const ph7_io_stream *pStream;` |
|      - | 3084 | `	const char *zLine;` |
|      - | 3085 | `	io_private *pDev;` |
|      - | 3086 | `	ph7_int64 n,nLen;` |
|   6385 | 3087 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3088 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3089 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3090 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3091 | `		return PH7_OK;` |
|      - | 3092 | `	}` |
|      - | 3093 | `	/* Extract our private data */` |
|   6385 | 3094 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3095 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   6385 | 3096 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3097 | `		/*Expecting an IO handle */` |
|    ! 0 | 3098 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3099 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3100 | `		return PH7_OK;` |
|      - | 3101 | `	}` |
|      - | 3102 | `	/* Point to the target IO stream device */` |
|   6385 | 3103 | `	pStream = pDev->pStream;` |
|   6385 | 3104 | `	if( pStream == 0  ){` |
|    ! 0 | 3105 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3106 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3107 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3108 | `			);` |
|    ! 0 | 3109 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3110 | `		return PH7_OK;` |
|      - | 3111 | `	}` |
|   6385 | 3112 | `	nLen = -1;` |
|   6385 | 3113 | `	if( nArg > 1 ){` |
|      - | 3114 | `		/* Maximum data to read */` |
|    ! 0 | 3115 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3116 | `	}` |
|      - | 3117 | `	/* Perform the requested operation */` |
|   6385 | 3118 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|   6385 | 3119 | `	if( n < 1 ){` |
|      - | 3120 | `		/* EOF or IO error,return FALSE */` |
|      7 | 3121 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3122 | `	}else{` |
|      - | 3123 | `		/* Return the freshly extracted line */` |
|   6383 | 3124 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - | 3125 | `	}` |
|   6385 | 3126 | `	return PH7_OK;` |
|   3195 | 3127 | `}` |
|      - | 3128 | `/*` |
|      - | 3129 | ` * string fread(resource $handle,int64 $length)` |
|      - | 3130 | ` *  Binary-safe file read.` |
|      - | 3131 | ` * Parameters` |
|      - | 3132 | ` *  $handle` |
|      - | 3133 | ` *   The file pointer.` |
|      - | 3134 | ` * $length` |
|      - | 3135 | ` *  Up to length number of bytes read.` |
|      - | 3136 | ` * Return` |
|      - | 3137 | ` *  The data readen on success or FALSE on failure.` |
|      - | 3138 | ` */` |
|     10 | 3139 | `static int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3140 | `{` |
|      - | 3141 | `	const ph7_io_stream *pStream;` |
|      - | 3142 | `	io_private *pDev;` |
|      - | 3143 | `	ph7_int64 nRead;` |
|      - | 3144 | `	void *pBuf;` |
|      - | 3145 | `	int nLen;` |
|     12 | 3146 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3147 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3148 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3149 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3150 | `		return PH7_OK;` |
|      - | 3151 | `	}` |
|      - | 3152 | `	/* Extract our private data */` |
|     12 | 3153 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3154 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     12 | 3155 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3156 | `		/*Expecting an IO handle */` |
|    ! 0 | 3157 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3158 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3159 | `		return PH7_OK;` |
|      - | 3160 | `	}` |
|      - | 3161 | `	/* Point to the target IO stream device */` |
|     12 | 3162 | `	pStream = pDev->pStream;` |
|     12 | 3163 | `	if( pStream == 0  ){` |
|    ! 0 | 3164 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3165 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3166 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3167 | `			);` |
|    ! 0 | 3168 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3169 | `		return PH7_OK;` |
|      - | 3170 | `	}` |
|     12 | 3171 | `        nLen = 4096;` |
|     12 | 3172 | `	if( nArg > 1 ){` |
|     12 | 3173 | ` 	  nLen = ph7_value_to_int(apArg[1]);` |
|     12 | 3174 | `	  if( nLen < 1 ){` |
|      - | 3175 | `		/* Invalid length,set a default length */` |
|    ! 0 | 3176 | `		nLen = 4096;` |
|    ! 0 | 3177 | `	  }` |
|      5 | 3178 | `        }` |
|      - | 3179 | `	/* Allocate enough buffer */` |
|     12 | 3180 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|     12 | 3181 | `	if( pBuf == 0 ){` |
|    ! 0 | 3182 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3183 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3184 | `		return PH7_OK;` |
|      - | 3185 | `	}` |
|      - | 3186 | `	/* Perform the requested operation */` |
|     12 | 3187 | `	nRead = StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|     12 | 3188 | `	if( nRead < 1 ){` |
|      - | 3189 | `		/* Nothing read,return FALSE */` |
|    ! 0 | 3190 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3191 | `	}else{` |
|      - | 3192 | `		/* Make a copy of the data just read */` |
|     12 | 3193 | `		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);` |
|      - | 3194 | `	}` |
|      - | 3195 | `	/* Release the buffer */` |
|     12 | 3196 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|     12 | 3197 | `	return PH7_OK;` |
|      7 | 3198 | `}` |
|      - | 3199 | `/*` |
|      - | 3200 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|      - | 3201 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|      - | 3202 | ` * Gets line from file pointer and parse for CSV fields.` |
|      - | 3203 | ` * Parameters` |
|      - | 3204 | ` * $handle` |
|      - | 3205 | ` *   The file pointer.` |
|      - | 3206 | ` * $length` |
|      - | 3207 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3208 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3209 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3210 | ` *  the end of the line.` |
|      - | 3211 | ` * $delimiter` |
|      - | 3212 | ` *   Set the field delimiter (one character only).` |
|      - | 3213 | ` * $enclosure` |
|      - | 3214 | ` *   Set the field enclosure character (one character only).` |
|      - | 3215 | ` * $escape` |
|      - | 3216 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 3217 | ` * Return` |
|      - | 3218 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 3219 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 3220 | ` *  If an error occurs, FALSE is returned.` |
|      - | 3221 | ` */` |
|      2 | 3222 | `static int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3223 | `{` |
|      - | 3224 | `	const ph7_io_stream *pStream;` |
|      - | 3225 | `	const char *zLine;` |
|      - | 3226 | `	io_private *pDev;` |
|      - | 3227 | `	ph7_int64 n,nLen;` |
|      3 | 3228 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3229 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3230 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3231 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3232 | `		return PH7_OK;` |
|      - | 3233 | `	}` |
|      - | 3234 | `	/* Extract our private data */` |
|      3 | 3235 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3236 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3237 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3238 | `		/*Expecting an IO handle */` |
|    ! 0 | 3239 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3240 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3241 | `		return PH7_OK;` |
|      - | 3242 | `	}` |
|      - | 3243 | `	/* Point to the target IO stream device */` |
|      3 | 3244 | `	pStream = pDev->pStream;` |
|      3 | 3245 | `	if( pStream == 0  ){` |
|    ! 0 | 3246 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3247 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3248 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3249 | `			);` |
|    ! 0 | 3250 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3251 | `		return PH7_OK;` |
|      - | 3252 | `	}` |
|      3 | 3253 | `	nLen = -1;` |
|      3 | 3254 | `	if( nArg > 1 ){` |
|      - | 3255 | `		/* Maximum data to read */` |
|      3 | 3256 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|      1 | 3257 | `	}` |
|      - | 3258 | `	/* Perform the requested operation */` |
|      3 | 3259 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3260 | `	if( n < 1 ){` |
|      - | 3261 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3262 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3263 | `	}else{` |
|      - | 3264 | `		ph7_value *pArray;` |
|      3 | 3265 | `		int delim  = ',';   /* Delimiter */` |
|      3 | 3266 | `		int encl   = '"' ;  /* Enclosure */` |
|      3 | 3267 | `		int escape = '\\';  /* Escape character */` |
|      3 | 3268 | `		if( nArg > 2 ){` |
|      - | 3269 | `			const char *zPtr;` |
|      - | 3270 | `			int i;` |
|      3 | 3271 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 3272 | `				/* Extract the delimiter */` |
|      3 | 3273 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 3274 | `				if( i > 0 ){` |
|      3 | 3275 | `					delim = zPtr[0];` |
|      1 | 3276 | `				}` |
|      1 | 3277 | `			}` |
|      3 | 3278 | `			if( nArg > 3 ){` |
|      3 | 3279 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 3280 | `					/* Extract the enclosure */` |
|      3 | 3281 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 3282 | `					if( i > 0 ){` |
|      3 | 3283 | `						encl = zPtr[0];` |
|      1 | 3284 | `					}` |
|      1 | 3285 | `				}` |
|      3 | 3286 | `				if( nArg > 4 ){` |
|      3 | 3287 | `					if( ph7_value_is_string(apArg[4]) ){` |
|      - | 3288 | `						/* Extract the escape character */` |
|      3 | 3289 | `						zPtr = ph7_value_to_string(apArg[4],&i);` |
|      3 | 3290 | `						if( i > 0 ){` |
|      3 | 3291 | `							escape = zPtr[0];` |
|      1 | 3292 | `						}` |
|      1 | 3293 | `					}` |
|      1 | 3294 | `				}` |
|      1 | 3295 | `			}` |
|      1 | 3296 | `		}` |
|      - | 3297 | `		/* Create our array */` |
|      3 | 3298 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 3299 | `		if( pArray == 0 ){` |
|    ! 0 | 3300 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3301 | `			ph7_result_null(pCtx);` |
|    ! 0 | 3302 | `			return PH7_OK;` |
|      - | 3303 | `		}` |
|      - | 3304 | `		/* Parse the raw input */` |
|      3 | 3305 | `		PH7_ProcessCsv(zLine,(int)n,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 3306 | `		/* Return the freshly created array  */` |
|      3 | 3307 | `		ph7_result_value(pCtx,pArray);` |
|      - | 3308 | `	}` |
|      3 | 3309 | `	return PH7_OK;` |
|      2 | 3310 | `}` |
|      - | 3311 | `/*` |
|      - | 3312 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|      - | 3313 | ` *  Gets line from file pointer and strip HTML tags.` |
|      - | 3314 | ` * Parameters` |
|      - | 3315 | ` * $handle` |
|      - | 3316 | ` *   The file pointer.` |
|      - | 3317 | ` * $length` |
|      - | 3318 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 3319 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 3320 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 3321 | ` *  the end of the line.` |
|      - | 3322 | ` * $allowable_tags` |
|      - | 3323 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 3324 | ` * Return` |
|      - | 3325 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|      - | 3326 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|      - | 3327 | ` */` |
|      2 | 3328 | `static int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3329 | `{` |
|      - | 3330 | `	const ph7_io_stream *pStream;` |
|      - | 3331 | `	const char *zLine;` |
|      - | 3332 | `	io_private *pDev;` |
|      - | 3333 | `	ph7_int64 n,nLen;` |
|      3 | 3334 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3335 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3336 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3337 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3338 | `		return PH7_OK;` |
|      - | 3339 | `	}` |
|      - | 3340 | `	/* Extract our private data */` |
|      3 | 3341 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3342 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3343 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3344 | `		/*Expecting an IO handle */` |
|    ! 0 | 3345 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3346 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3347 | `		return PH7_OK;` |
|      - | 3348 | `	}` |
|      - | 3349 | `	/* Point to the target IO stream device */` |
|      3 | 3350 | `	pStream = pDev->pStream;` |
|      3 | 3351 | `	if( pStream == 0  ){` |
|    ! 0 | 3352 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3353 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3354 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3355 | `			);` |
|    ! 0 | 3356 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3357 | `		return PH7_OK;` |
|      - | 3358 | `	}` |
|      3 | 3359 | `	nLen = -1;` |
|      3 | 3360 | `	if( nArg > 1 ){` |
|      - | 3361 | `		/* Maximum data to read */` |
|    ! 0 | 3362 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 3363 | `	}` |
|      - | 3364 | `	/* Perform the requested operation */` |
|      3 | 3365 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 3366 | `	if( n < 1 ){` |
|      - | 3367 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 3368 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3369 | `	}else{` |
|      3 | 3370 | `		const char *zTaglist = 0;` |
|      3 | 3371 | `		int nTaglen = 0;` |
|      3 | 3372 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|      - | 3373 | `			/* Allowed tag */` |
|    ! 0 | 3374 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|    ! 0 | 3375 | `		}` |
|      - | 3376 | `		/* Process data just read */` |
|      3 | 3377 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen);` |
|      - | 3378 | `	}` |
|      3 | 3379 | `	return PH7_OK;` |
|      2 | 3380 | `}` |
|      - | 3381 | `/*` |
|      - | 3382 | ` * string readdir(resource $dir_handle)` |
|      - | 3383 | ` *   Read entry from directory handle.` |
|      - | 3384 | ` * Parameter` |
|      - | 3385 | ` *  $dir_handle` |
|      - | 3386 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3387 | ` * Return` |
|      - | 3388 | ` *  Returns the filename on success or FALSE on failure.` |
|      - | 3389 | ` */` |
|   8094 | 3390 | `static int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3391 | `{` |
|      - | 3392 | `	const ph7_io_stream *pStream;` |
|      - | 3393 | `	io_private *pDev;` |
|      - | 3394 | `	int rc;` |
|   8099 | 3395 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3396 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3397 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3398 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3399 | `		return PH7_OK;` |
|      - | 3400 | `	}` |
|      - | 3401 | `	/* Extract our private data */` |
|   8099 | 3402 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3403 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   8099 | 3404 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3405 | `		/*Expecting an IO handle */` |
|    ! 0 | 3406 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3407 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3408 | `		return PH7_OK;` |
|      - | 3409 | `	}` |
|      - | 3410 | `	/* Point to the target IO stream device */` |
|   8099 | 3411 | `	pStream = pDev->pStream;` |
|   8099 | 3412 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 3413 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3414 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3415 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3416 | `			);` |
|    ! 0 | 3417 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3418 | `		return PH7_OK;` |
|      - | 3419 | `	}` |
|   8099 | 3420 | `	ph7_result_bool(pCtx,0);` |
|      - | 3421 | `	/* Perform the requested operation */` |
|   8099 | 3422 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|   8099 | 3423 | `	if( rc != PH7_OK ){` |
|      - | 3424 | `		/* Return FALSE */` |
|    975 | 3425 | `		ph7_result_bool(pCtx,0);` |
|    485 | 3426 | `	}` |
|   8099 | 3427 | `	return PH7_OK;` |
|   4052 | 3428 | `}` |
|      - | 3429 | `/*` |
|      - | 3430 | ` * void rewinddir(resource $dir_handle)` |
|      - | 3431 | ` *   Rewind directory handle.` |
|      - | 3432 | ` * Parameter` |
|      - | 3433 | ` *  $dir_handle` |
|      - | 3434 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3435 | ` * Return` |
|      - | 3436 | ` *  FALSE on failure.` |
|      - | 3437 | ` */` |
|      2 | 3438 | `static int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3439 | `{` |
|      - | 3440 | `	const ph7_io_stream *pStream;` |
|      - | 3441 | `	io_private *pDev;` |
|      3 | 3442 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3443 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3444 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3445 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3446 | `		return PH7_OK;` |
|      - | 3447 | `	}` |
|      - | 3448 | `	/* Extract our private data */` |
|      3 | 3449 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3450 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 3451 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3452 | `		/*Expecting an IO handle */` |
|    ! 0 | 3453 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3454 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3455 | `		return PH7_OK;` |
|      - | 3456 | `	}` |
|      - | 3457 | `	/* Point to the target IO stream device */` |
|      3 | 3458 | `	pStream = pDev->pStream;` |
|      3 | 3459 | `	if( pStream == 0  \|\| pStream->xRewindDir == 0 ){` |
|    ! 0 | 3460 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3461 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3462 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3463 | `			);` |
|    ! 0 | 3464 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3465 | `		return PH7_OK;` |
|      - | 3466 | `	}` |
|      - | 3467 | `	/* Perform the requested operation */` |
|      3 | 3468 | `	pStream->xRewindDir(pDev->pHandle);` |
|      3 | 3469 | `	return PH7_OK;` |
|      2 | 3470 | ` }` |
|      - | 3471 | `/* Forward declaration */` |
|      - | 3472 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut);` |
|      - | 3473 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|      - | 3474 | `/*` |
|      - | 3475 | ` * void closedir(resource $dir_handle)` |
|      - | 3476 | ` *   Close directory handle.` |
|      - | 3477 | ` * Parameter` |
|      - | 3478 | ` *  $dir_handle` |
|      - | 3479 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 3480 | ` * Return` |
|      - | 3481 | ` *  FALSE on failure.` |
|      - | 3482 | ` */` |
|    972 | 3483 | `static int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3484 | `{` |
|      - | 3485 | `	const ph7_io_stream *pStream;` |
|      - | 3486 | `	io_private *pDev;` |
|    977 | 3487 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 3488 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3489 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3490 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3491 | `		return PH7_OK;` |
|      - | 3492 | `	}` |
|      - | 3493 | `	/* Extract our private data */` |
|    977 | 3494 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3495 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    977 | 3496 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3497 | `		/*Expecting an IO handle */` |
|    ! 0 | 3498 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3499 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3500 | `		return PH7_OK;` |
|      - | 3501 | `	}` |
|      - | 3502 | `	/* Point to the target IO stream device */` |
|    977 | 3503 | `	pStream = pDev->pStream;` |
|    977 | 3504 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 3505 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3506 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3507 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3508 | `			);` |
|    ! 0 | 3509 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3510 | `		return PH7_OK;` |
|      - | 3511 | `	}` |
|      - | 3512 | `	/* Perform the requested operation */` |
|    977 | 3513 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 3514 | `	/* Release the private stucture */` |
|    977 | 3515 | `	ReleaseIOPrivate(pCtx,pDev);` |
|    977 | 3516 | `	PH7_MemObjRelease(apArg[0]);` |
|    977 | 3517 | `	return PH7_OK;` |
|    491 | 3518 | ` }` |
|      - | 3519 | `/*` |
|      - | 3520 | ` * resource opendir(string $path[,resource $context])` |
|      - | 3521 | ` *  Open directory handle.` |
|      - | 3522 | ` * Parameters` |
|      - | 3523 | ` * $path` |
|      - | 3524 | ` *   The directory path that is to be opened.` |
|      - | 3525 | ` * $context` |
|      - | 3526 | ` *   A context stream resource.` |
|      - | 3527 | ` * Return` |
|      - | 3528 | ` *  A directory handle resource on success,or FALSE on failure.` |
|      - | 3529 | ` */` |
|    972 | 3530 | `static int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3531 | `{` |
|      - | 3532 | `	const ph7_io_stream *pStream;` |
|      - | 3533 | `	const char *zPath;` |
|      - | 3534 | `	io_private *pDev;` |
|      - | 3535 | `	int iLen,rc;` |
|    977 | 3536 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3537 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3538 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 3539 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3540 | `		return PH7_OK;` |
|      - | 3541 | `	}` |
|      - | 3542 | `	/* Extract the target path */` |
|    977 | 3543 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 3544 | `	/* Try to extract a stream */` |
|    977 | 3545 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|    977 | 3546 | `	if( pStream == 0 ){` |
|    ! 0 | 3547 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 3548 | `			"No stream device is associated with the given path(%s)",zPath);` |
|    ! 0 | 3549 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3550 | `		return PH7_OK;` |
|      - | 3551 | `	}` |
|    977 | 3552 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 3553 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3554 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 3555 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 3556 | `			);` |
|    ! 0 | 3557 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3558 | `		return PH7_OK;` |
|      - | 3559 | `	}` |
|      - | 3560 | `	/* Allocate a new IO private instance */` |
|    977 | 3561 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    977 | 3562 | `	if( pDev == 0 ){` |
|    ! 0 | 3563 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3564 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3565 | `		return PH7_OK;` |
|      - | 3566 | `	}` |
|      - | 3567 | `	/* Initialize the structure */` |
|    977 | 3568 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 3569 | `	/* Open the target directory */` |
|    977 | 3570 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|    977 | 3571 | `	if( rc != PH7_OK ){` |
|      - | 3572 | `		/* IO error,return FALSE */` |
|    ! 0 | 3573 | `		ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 3574 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3575 | `	}else{` |
|      - | 3576 | `		/* Return the handle as a resource */` |
|    977 | 3577 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 3578 | `	}` |
|    977 | 3579 | `	return PH7_OK;` |
|    491 | 3580 | `}` |
|      - | 3581 | `/*` |
|      - | 3582 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|      - | 3583 | ` *  Reads a file and writes it to the output buffer.` |
|      - | 3584 | ` * Parameters` |
|      - | 3585 | ` *  $filename` |
|      - | 3586 | ` *   The filename being read.` |
|      - | 3587 | ` *  $use_include_path` |
|      - | 3588 | ` *   You can use the optional second parameter and set it to` |
|      - | 3589 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3590 | ` *  $context` |
|      - | 3591 | ` *   A context stream resource.` |
|      - | 3592 | ` * Return` |
|      - | 3593 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|      - | 3594 | ` */` |
|      2 | 3595 | `static int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3596 | `{` |
|      3 | 3597 | `	int use_include  = FALSE;` |
|      - | 3598 | `	const ph7_io_stream *pStream;` |
|      - | 3599 | `	ph7_int64 n,nRead;` |
|      - | 3600 | `	const char *zFile;` |
|      - | 3601 | `	char zBuf[8192];` |
|      - | 3602 | `	void *pHandle;` |
|      - | 3603 | `	int rc,nLen;` |
|      3 | 3604 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3605 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3606 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3607 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3608 | `		return PH7_OK;` |
|      - | 3609 | `	}` |
|      - | 3610 | `	/* Extract the file path */` |
|      3 | 3611 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3612 | `	/* Point to the target IO stream device */` |
|      3 | 3613 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 3614 | `	if( pStream == 0 ){` |
|    ! 0 | 3615 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3616 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3617 | `		return PH7_OK;` |
|      - | 3618 | `	}` |
|      3 | 3619 | `	if( nArg > 1 ){` |
|    ! 0 | 3620 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 3621 | `	}` |
|      - | 3622 | `	/* Try to open the file in read-only mode */` |
|      4 | 3623 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|      1 | 3624 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 3625 | `	if( pHandle == 0 ){` |
|    ! 0 | 3626 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3627 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3628 | `		return PH7_OK;` |
|      - | 3629 | `	}` |
|      - | 3630 | `	/* Perform the requested operation */` |
|      3 | 3631 | `	nRead = 0;` |
|      2 | 3632 | `	for(;;){` |
|      5 | 3633 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 3634 | `		if( n < 1 ){` |
|      - | 3635 | `			/* EOF or IO error,break immediately */` |
|      3 | 3636 | `			break;` |
|      - | 3637 | `		}` |
|      - | 3638 | `		/* Output data */` |
|      3 | 3639 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      3 | 3640 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 3641 | `			break;` |
|      - | 3642 | `		}` |
|      - | 3643 | `		/* Increment counter */` |
|      3 | 3644 | `		nRead += n;` |
|      1 | 3645 | `	}` |
|      - | 3646 | `	/* Close the stream */` |
|      3 | 3647 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3648 | `	/* Total number of bytes readen */` |
|      3 | 3649 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 3650 | `	return PH7_OK;` |
|      2 | 3651 | `}` |
|      - | 3652 | `/*` |
|      - | 3653 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|      - | 3654 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|      - | 3655 | ` *  Reads entire file into a string.` |
|      - | 3656 | ` * Parameters` |
|      - | 3657 | ` *  $filename` |
|      - | 3658 | ` *   The filename being read.` |
|      - | 3659 | ` *  $use_include_path` |
|      - | 3660 | ` *   You can use the optional second parameter and set it to` |
|      - | 3661 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3662 | ` *  $context` |
|      - | 3663 | ` *   A context stream resource.` |
|      - | 3664 | ` *  $offset` |
|      - | 3665 | ` *   The offset where the reading starts on the original stream.` |
|      - | 3666 | ` *  $maxlen` |
|      - | 3667 | ` *    Maximum length of data read. The default is to read until end of file` |
|      - | 3668 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|      - | 3669 | ` * Return` |
|      - | 3670 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3671 | ` */` |
|   6186 | 3672 | `static int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3673 | `{` |
|      - | 3674 | `	const ph7_io_stream *pStream;` |
|      - | 3675 | `	ph7_int64 n,nRead,nMaxlen;` |
|   6191 | 3676 | `	int use_include  = FALSE;` |
|      - | 3677 | `	const char *zFile;` |
|      - | 3678 | `	char zBuf[8192];` |
|      - | 3679 | `	void *pHandle;` |
|      - | 3680 | `	int nLen;` |
|      - | 3681 |  |
|   6191 | 3682 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3683 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3684 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3685 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3686 | `		return PH7_OK;` |
|      - | 3687 | `	}` |
|      - | 3688 | `	/* Extract the file path */` |
|   6191 | 3689 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3690 | `	/* Point to the target IO stream device */` |
|   6191 | 3691 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   6191 | 3692 | `	if( pStream == 0 ){` |
|    ! 0 | 3693 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3694 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3695 | `		return PH7_OK;` |
|      - | 3696 | `	}` |
|   6191 | 3697 | `	nMaxlen = -1;` |
|   6191 | 3698 | `	if( nArg > 1 ){` |
|      5 | 3699 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|      2 | 3700 | `	}` |
|      - | 3701 | `	/* Try to open the file in read-only mode */` |
|   6191 | 3702 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|   6191 | 3703 | `	if( pHandle == 0 ){` |
|    ! 0 | 3704 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3705 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3706 | `		return PH7_OK;` |
|      - | 3707 | `	}` |
|   6191 | 3708 | `	if( nArg > 3 ){` |
|      - | 3709 | `		/* Extract the offset */` |
|      5 | 3710 | `		n = ph7_value_to_int64(apArg[3]);` |
|      5 | 3711 | `		if( n > 0 ){` |
|    ! 0 | 3712 | `			if( pStream->xSeek ){` |
|      - | 3713 | `				/* Seek to the desired offset */` |
|    ! 0 | 3714 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|    ! 0 | 3715 | `			}` |
|    ! 0 | 3716 | `		}` |
|      5 | 3717 | `		if( nArg > 4 ){` |
|      - | 3718 | `			/* Maximum data to read */` |
|      5 | 3719 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|      2 | 3720 | `		}` |
|      2 | 3721 | `	}` |
|      - | 3722 | `	/* Perform the requested operation */` |
|   6191 | 3723 | `	nRead = 0;` |
|   6185 | 3724 | `	for(;;){` |
|  18563 | 3725 | `		n = pStream->xRead(pHandle,zBuf,` |
|   6188 | 3726 | `			(nMaxlen > 0 && (nMaxlen < (ph7_int64)sizeof(zBuf))) ? nMaxlen : (ph7_int64)sizeof(zBuf));` |
|  12375 | 3727 | `		if( n < 1 ){` |
|      - | 3728 | `			/* EOF or IO error,break immediately */` |
|   6189 | 3729 | `			break;` |
|      - | 3730 | `		}` |
|      - | 3731 | `		/* Append data */` |
|   6191 | 3732 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 3733 | `		/* Increment read counter */` |
|   6191 | 3734 | `		nRead += n;` |
|   6191 | 3735 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 3736 | `			/* Read limit reached */` |
|      3 | 3737 | `			break;` |
|      - | 3738 | `		}` |
|      5 | 3739 | `	}` |
|      - | 3740 | `	/* Close the stream */` |
|   6191 | 3741 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 3742 | `	/* Check if we have read something */` |
|   6191 | 3743 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 3744 | `		/* Nothing read,return FALSE */` |
|    ! 0 | 3745 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3746 | `	}` |
|   6191 | 3747 | `	return PH7_OK;` |
|   3098 | 3748 | `}` |
|      - | 3749 | `/*` |
|      - | 3750 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 3751 | ` *  Write a string to a file.` |
|      - | 3752 | ` * Parameters` |
|      - | 3753 | ` *  $filename` |
|      - | 3754 | ` *  Path to the file where to write the data.` |
|      - | 3755 | ` * $data` |
|      - | 3756 | ` *  The data to write(Must be a string).` |
|      - | 3757 | ` * $flags` |
|      - | 3758 | ` *  The value of flags can be any combination of the following` |
|      - | 3759 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 3760 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 3761 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 3762 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 3763 | ` * context` |
|      - | 3764 | ` *  A context stream resource.` |
|      - | 3765 | ` * Return` |
|      - | 3766 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 3767 | ` */` |
|  13952 | 3768 | `static int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3769 | `{` |
|  13957 | 3770 | `	int use_include  = FALSE;` |
|      - | 3771 | `	const ph7_io_stream *pStream;` |
|      - | 3772 | `	const char *zFile;` |
|      - | 3773 | `	const char *zData;` |
|      - | 3774 | `	int iOpenFlags;` |
|      - | 3775 | `	void *pHandle;` |
|      - | 3776 | `	int iFlags;` |
|      - | 3777 | `	int nLen;` |
|      - | 3778 |  |
|  13957 | 3779 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3780 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3781 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 3782 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3783 | `		return PH7_OK;` |
|      - | 3784 | `	}` |
|      - | 3785 | `	/* Extract the file path */` |
|  13957 | 3786 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3787 | `	/* Point to the target IO stream device */` |
|  13957 | 3788 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  13957 | 3789 | `	if( pStream == 0 ){` |
|    ! 0 | 3790 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3791 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3792 | `		return PH7_OK;` |
|      - | 3793 | `	}` |
|      - | 3794 | `	/* Data to write */` |
|  13957 | 3795 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 3796 | `	/* Try to open the file in read-write mode */` |
|  13957 | 3797 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 3798 | `	/* Extract the flags */` |
|  13957 | 3799 | `	iFlags = 0;` |
|  13957 | 3800 | `	if( nArg > 2 ){` |
|    ! 0 | 3801 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3802 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 3803 | `			use_include = TRUE;` |
|    ! 0 | 3804 | `		}` |
|    ! 0 | 3805 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 3806 | `			/* If the file already exists, append the data to the file` |
|      - | 3807 | `			 * instead of overwriting it.` |
|      - | 3808 | `			 */` |
|    ! 0 | 3809 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 3810 | `			/* Append mode */` |
|    ! 0 | 3811 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 3812 | `		}` |
|    ! 0 | 3813 | `	}` |
|  20933 | 3814 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   6976 | 3815 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);` |
|  13957 | 3816 | `	if( pHandle == 0 ){` |
|    ! 0 | 3817 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 3818 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3819 | `		return PH7_OK;` |
|      - | 3820 | `	}` |
|  13957 | 3821 | `	if( nLen < 1 ){` |
|      - | 3822 | `		/* Empty data, file is created/truncated */` |
|      7 | 3823 | `		ph7_result_int64(pCtx,0);` |
|      7 | 3824 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|      7 | 3825 | `		return PH7_OK;` |
|      - | 3826 | `	}` |
|  13951 | 3827 | `	if( pStream->xWrite ){` |
|      - | 3828 | `		ph7_int64 n;` |
|  13951 | 3829 | `		if( (iFlags & 0x01/* LOCK_EX */) && pStream->xLock ){` |
|      - | 3830 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 3831 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 3832 | `		}` |
|      - | 3833 | `		/* Perform the write operation */` |
|  13951 | 3834 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  13951 | 3835 | `		if( n < 0 ){` |
|      - | 3836 | `			/* IO error,return FALSE */` |
|    ! 0 | 3837 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3838 | `		}else{` |
|      - | 3839 | `			/* Total number of bytes written */` |
|  13951 | 3840 | `			ph7_result_int64(pCtx,n);` |
|      - | 3841 | `		}` |
|   6978 | 3842 | `	}else{` |
|      - | 3843 | `		/* Read-only stream */` |
|    ! 0 | 3844 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 3845 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 3846 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 3847 | `			);` |
|    ! 0 | 3848 | `		ph7_result_bool(pCtx,0);` |
|      - | 3849 | `	}` |
|      - | 3850 | `	/* Close the handle */` |
|  13951 | 3851 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  13951 | 3852 | `	return PH7_OK;` |
|   6981 | 3853 | `}` |
|      - | 3854 | `/*` |
|      - | 3855 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 3856 | ` *  Reads entire file into an array.` |
|      - | 3857 | ` * Parameters` |
|      - | 3858 | ` *  $filename` |
|      - | 3859 | ` *   The filename being read.` |
|      - | 3860 | ` *  $flags` |
|      - | 3861 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 3862 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 3863 | ` *       Search for the file in the include_path.` |
|      - | 3864 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 3865 | ` *       Do not add newline at the end of each array element` |
|      - | 3866 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 3867 | ` *       Skip empty lines` |
|      - | 3868 | ` *  $context` |
|      - | 3869 | ` *   A context stream resource.` |
|      - | 3870 | ` * Return` |
|      - | 3871 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 3872 | ` */` |
|      8 | 3873 | `static int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3874 | `{` |
|      - | 3875 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 3876 | `	ph7_value *pArray,*pLine;` |
|      - | 3877 | `	const ph7_io_stream *pStream;` |
|     10 | 3878 | `	int use_include = 0;` |
|      - | 3879 | `	io_private *pDev;` |
|      - | 3880 | `	ph7_int64 n;` |
|      - | 3881 | `	int iFlags;` |
|      - | 3882 | `	int nLen;` |
|      - | 3883 |  |
|     10 | 3884 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3885 | `		/* Missing/Invalid arguments,return FALSE */` |
|      5 | 3886 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|      5 | 3887 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3888 | `		return PH7_OK;` |
|      - | 3889 | `	}` |
|      - | 3890 | `	/* Extract the file path */` |
|      6 | 3891 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3892 | `	/* Point to the target IO stream device */` |
|      6 | 3893 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 3894 | `	if( pStream == 0 ){` |
|    ! 0 | 3895 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 3896 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3897 | `		return PH7_OK;` |
|      - | 3898 | `	}` |
|      - | 3899 | `	/* Allocate a new IO private instance */` |
|      6 | 3900 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|      6 | 3901 | `	if( pDev == 0 ){` |
|    ! 0 | 3902 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3903 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3904 | `		return PH7_OK;` |
|      - | 3905 | `	}` |
|      - | 3906 | `	/* Initialize the structure */` |
|      6 | 3907 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      6 | 3908 | `	iFlags = 0;` |
|      6 | 3909 | `	if( nArg > 1 ){` |
|    ! 0 | 3910 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 3911 | `	}` |
|      6 | 3912 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|    ! 0 | 3913 | `		use_include = TRUE;` |
|    ! 0 | 3914 | `	}` |
|      - | 3915 | `	/* Create the array and the working value */` |
|      6 | 3916 | `	pArray = ph7_context_new_array(pCtx);` |
|      6 | 3917 | `	pLine = ph7_context_new_scalar(pCtx);` |
|      6 | 3918 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 3919 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3920 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3921 | `		return PH7_OK;` |
|      - | 3922 | `	}` |
|      - | 3923 | `	/* Try to open the file in read-only mode */` |
|      6 | 3924 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 3925 | `	if( pDev->pHandle == 0 ){` |
|      3 | 3926 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|      3 | 3927 | `		ph7_result_bool(pCtx,0);` |
|      - | 3928 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 3929 | `		 * as soon we return from this function.` |
|      - | 3930 | `		 */` |
|      3 | 3931 | `		return PH7_OK;` |
|      - | 3932 | `	}` |
|      - | 3933 | `	/* Perform the requested operation */` |
|      3 | 3934 | `	for(;;){` |
|      - | 3935 | `		/* Try to extract a line */` |
|      7 | 3936 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|      7 | 3937 | `		if( n < 1 ){` |
|      - | 3938 | `			/* EOF or IO error */` |
|      3 | 3939 | `			break;` |
|      - | 3940 | `		}` |
|      - | 3941 | `		/* Reset the cursor */` |
|      5 | 3942 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 3943 | `		/* Remove line ending if requested by the caller */` |
|      5 | 3944 | `		zPtr = zBuf;` |
|      5 | 3945 | `		zEnd = &zBuf[n];` |
|      5 | 3946 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 3947 | `			/* Ignore trailig lines */` |
|    ! 0 | 3948 | `			while( zPtr < zEnd && (zEnd[-1] == '\n'` |
|      - | 3949 | `#ifdef __WINNT__` |
|      - | 3950 | `				\|\| zEnd[-1] == '\r'` |
|      - | 3951 | `#endif` |
|      - | 3952 | `				)){` |
|    ! 0 | 3953 | `					n--;` |
|    ! 0 | 3954 | `					zEnd--;` |
|    ! 0 | 3955 | `			}` |
|    ! 0 | 3956 | `		}` |
|      5 | 3957 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 3958 | `			/* Ignore empty lines */` |
|    ! 0 | 3959 | `			while( zPtr < zEnd && (unsigned char)zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) ){` |
|    ! 0 | 3960 | `				zPtr++;` |
|    ! 0 | 3961 | `			}` |
|    ! 0 | 3962 | `			if( zPtr >= zEnd ){` |
|      - | 3963 | `				/* Empty line */` |
|    ! 0 | 3964 | `				continue;` |
|      - | 3965 | `			}` |
|    ! 0 | 3966 | `		}` |
|      5 | 3967 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 3968 | `		/* Insert line */` |
|      5 | 3969 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 3970 | `	}` |
|      - | 3971 | `	/* Close the stream */` |
|      3 | 3972 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 3973 | `	/* Release the io_private instance */` |
|      3 | 3974 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 3975 | `	/* Return the created array */` |
|      3 | 3976 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3977 | `	return PH7_OK;` |
|      6 | 3978 | `}` |
|      - | 3979 | `/*` |
|      - | 3980 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 3981 | ` *  Makes a copy of the file source to dest.` |
|      - | 3982 | ` * Parameters` |
|      - | 3983 | ` *  $source` |
|      - | 3984 | ` *   Path to the source file.` |
|      - | 3985 | ` *  $dest` |
|      - | 3986 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 3987 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 3988 | ` *  $context` |
|      - | 3989 | ` *   A context stream resource.` |
|      - | 3990 | ` * Return` |
|      - | 3991 | ` *  TRUE on success or FALSE on failure.` |
|      - | 3992 | ` */` |
|     10 | 3993 | `static int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3994 | `{` |
|      - | 3995 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 3996 | `	const char *zFile;` |
|      - | 3997 | `	char zBuf[8192];` |
|      - | 3998 | `	void *pIn,*pOut;` |
|      - | 3999 | `	ph7_int64 n;` |
|      - | 4000 | `	int nLen;` |
|     12 | 4001 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 4002 | `		/* Missing/Invalid arguments,return FALSE */` |
|      7 | 4003 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|      7 | 4004 | `		ph7_result_bool(pCtx,0);` |
|      7 | 4005 | `		return PH7_OK;` |
|      - | 4006 | `	}` |
|      - | 4007 | `	/* Extract the source name */` |
|      6 | 4008 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4009 | `	/* Point to the target IO stream device */` |
|      6 | 4010 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      6 | 4011 | `	if( pSin == 0 ){` |
|    ! 0 | 4012 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4013 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4014 | `		return PH7_OK;` |
|      - | 4015 | `	}` |
|      - | 4016 | `	/* Try to open the source file in a read-only mode */` |
|      6 | 4017 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      6 | 4018 | `	if( pIn == 0 ){` |
|      3 | 4019 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening source: '%s'",zFile);` |
|      3 | 4020 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4021 | `		return PH7_OK;` |
|      - | 4022 | `	}` |
|      - | 4023 | `	/* Extract the destination name */` |
|      3 | 4024 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 4025 | `	/* Point to the target IO stream device */` |
|      3 | 4026 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4027 | `	if( pSout == 0 ){` |
|    ! 0 | 4028 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4029 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4030 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4031 | `		return PH7_OK;` |
|      - | 4032 | `	}` |
|      3 | 4033 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 4034 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4035 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4036 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 4037 | `			);` |
|    ! 0 | 4038 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4039 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4040 | `		return PH7_OK;` |
|      - | 4041 | `	}` |
|      - | 4042 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 4043 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 4044 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);` |
|      3 | 4045 | `	if( pOut == 0 ){` |
|    ! 0 | 4046 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening destination: '%s'",zFile);` |
|    ! 0 | 4047 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4048 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 4049 | `		return PH7_OK;` |
|      - | 4050 | `	}` |
|      - | 4051 | `	/* Perform the requested operation */` |
|      2 | 4052 | `	for(;;){` |
|      - | 4053 | `		/* Read from source */` |
|      5 | 4054 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 4055 | `		if( n < 1 ){` |
|      - | 4056 | `			/* EOF or IO error,break immediately */` |
|      3 | 4057 | `			break;` |
|      - | 4058 | `		}` |
|      - | 4059 | `		/* Write to dest */` |
|      3 | 4060 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 4061 | `		if( n < 1 ){` |
|      - | 4062 | `			/* IO error,break immediately */` |
|    ! 0 | 4063 | `			break;` |
|      - | 4064 | `		}` |
|      1 | 4065 | `	}` |
|      - | 4066 | `	/* Close the streams */` |
|      3 | 4067 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 4068 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 4069 | `	/* Return TRUE */` |
|      3 | 4070 | `	ph7_result_bool(pCtx,1);` |
|      3 | 4071 | `	return PH7_OK;` |
|      7 | 4072 | `}` |
|      - | 4073 | `/*` |
|      - | 4074 | ` * array fstat(resource $handle)` |
|      - | 4075 | ` *  Gets information about a file using an open file pointer.` |
|      - | 4076 | ` * Parameters` |
|      - | 4077 | ` *  $handle` |
|      - | 4078 | ` *   The file pointer.` |
|      - | 4079 | ` * Return` |
|      - | 4080 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 4081 | ` */` |
|      2 | 4082 | `static int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4083 | `{` |
|      - | 4084 | `	ph7_value *pArray,*pValue;` |
|      - | 4085 | `	const ph7_io_stream *pStream;` |
|      - | 4086 | `	io_private *pDev;` |
|      3 | 4087 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4088 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4089 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4090 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4091 | `		return PH7_OK;` |
|      - | 4092 | `	}` |
|      - | 4093 | `	/* Extract our private data */` |
|      3 | 4094 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4095 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4096 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4097 | `		/* Expecting an IO handle */` |
|    ! 0 | 4098 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4099 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4100 | `		return PH7_OK;` |
|      - | 4101 | `	}` |
|      - | 4102 | `	/* Point to the target IO stream device */` |
|      3 | 4103 | `	pStream = pDev->pStream;` |
|      3 | 4104 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 4105 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4106 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4107 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4108 | `			);` |
|    ! 0 | 4109 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4110 | `		return PH7_OK;` |
|      - | 4111 | `	}` |
|      - | 4112 | `	/* Create the array and the working value */` |
|      3 | 4113 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4114 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4115 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4116 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4117 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4118 | `		return PH7_OK;` |
|      - | 4119 | `	}` |
|      - | 4120 | `	/* Perform the requested operation */` |
|      3 | 4121 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 4122 | `	/* Return the freshly created array */` |
|      3 | 4123 | `	ph7_result_value(pCtx,pArray);` |
|      - | 4124 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 4125 | `	 * released automatically as soon we return from this function.` |
|      - | 4126 | `	 */` |
|      3 | 4127 | `	return PH7_OK;` |
|      2 | 4128 | `}` |
|      - | 4129 | `/*` |
|      - | 4130 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 4131 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 4132 | ` * Parameters` |
|      - | 4133 | ` *  $handle` |
|      - | 4134 | ` *   The file pointer.` |
|      - | 4135 | ` *  $string` |
|      - | 4136 | ` *   The string that is to be written.` |
|      - | 4137 | ` *  $length` |
|      - | 4138 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 4139 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 4140 | ` * Return` |
|      - | 4141 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 4142 | ` */` |
|      6 | 4143 | `static int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4144 | `{` |
|      - | 4145 | `	const ph7_io_stream *pStream;` |
|      - | 4146 | `	const char *zString;` |
|      - | 4147 | `	io_private *pDev;` |
|      - | 4148 | `	int nLen,n;` |
|      7 | 4149 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4150 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4151 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4152 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4153 | `		return PH7_OK;` |
|      - | 4154 | `	}` |
|      - | 4155 | `	/* Extract our private data */` |
|      7 | 4156 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4157 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 4158 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4159 | `		/* Expecting an IO handle */` |
|    ! 0 | 4160 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4161 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4162 | `		return PH7_OK;` |
|      - | 4163 | `	}` |
|      - | 4164 | `	/* Point to the target IO stream device */` |
|      7 | 4165 | `	pStream = pDev->pStream;` |
|      7 | 4166 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4167 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4168 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4169 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4170 | `			);` |
|    ! 0 | 4171 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4172 | `		return PH7_OK;` |
|      - | 4173 | `	}` |
|      - | 4174 | `	/* Extract the data to write */` |
|      7 | 4175 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|      7 | 4176 | `	if( nArg > 2 ){` |
|      - | 4177 | `		/* Maximum data length to write */` |
|    ! 0 | 4178 | `		n = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 4179 | `		if( n >= 0 && n < nLen ){` |
|    ! 0 | 4180 | `			nLen = n;` |
|    ! 0 | 4181 | `		}` |
|    ! 0 | 4182 | `	}` |
|      7 | 4183 | `	if( nLen < 1 ){` |
|      - | 4184 | `		/* Nothing to write */` |
|    ! 0 | 4185 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4186 | `		return PH7_OK;` |
|      - | 4187 | `	}` |
|      - | 4188 | `	/* Perform the requested operation */` |
|      7 | 4189 | `	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);` |
|      7 | 4190 | `	if( n <  0 ){` |
|      - | 4191 | `		/* IO error,return FALSE */` |
|    ! 0 | 4192 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4193 | `	}else{` |
|      - | 4194 | `		/* #Bytes written */` |
|      7 | 4195 | `		ph7_result_int(pCtx,n);` |
|      - | 4196 | `	}` |
|      7 | 4197 | `	return PH7_OK;` |
|      4 | 4198 | `}` |
|      - | 4199 | `/*` |
|      - | 4200 | ` * bool flock(resource $handle,int $operation)` |
|      - | 4201 | ` *  Portable advisory file locking.` |
|      - | 4202 | ` * Parameters` |
|      - | 4203 | ` *  $handle` |
|      - | 4204 | ` *   The file pointer.` |
|      - | 4205 | ` *  $operation` |
|      - | 4206 | ` *   operation is one of the following:` |
|      - | 4207 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 4208 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 4209 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 4210 | ` * Return` |
|      - | 4211 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 4212 | ` */` |
|      4 | 4213 | `static int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4214 | `{` |
|      - | 4215 | `	const ph7_io_stream *pStream;` |
|      - | 4216 | `	io_private *pDev;` |
|      - | 4217 | `	int nLock;` |
|      - | 4218 | `	int rc;` |
|      5 | 4219 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4220 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4221 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4222 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4223 | `		return PH7_OK;` |
|      - | 4224 | `	}` |
|      - | 4225 | `	/* Extract our private data */` |
|      5 | 4226 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4227 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 4228 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4229 | `		/*Expecting an IO handle */` |
|    ! 0 | 4230 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4231 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4232 | `		return PH7_OK;` |
|      - | 4233 | `	}` |
|      - | 4234 | `	/* Point to the target IO stream device */` |
|      5 | 4235 | `	pStream = pDev->pStream;` |
|      5 | 4236 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|    ! 0 | 4237 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4238 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4239 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4240 | `			);` |
|    ! 0 | 4241 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4242 | `		return PH7_OK;` |
|      - | 4243 | `	}` |
|      - | 4244 | `	/* Requested lock operation */` |
|      5 | 4245 | `	nLock = ph7_value_to_int(apArg[1]);` |
|      - | 4246 | `	/* Lock operation */` |
|      5 | 4247 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 4248 | `	/* IO result */` |
|      5 | 4249 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 4250 | `	return PH7_OK;` |
|      3 | 4251 | `}` |
|      - | 4252 | `/*` |
|      - | 4253 | ` * int fpassthru(resource $handle)` |
|      - | 4254 | ` *  Output all remaining data on a file pointer.` |
|      - | 4255 | ` * Parameters` |
|      - | 4256 | ` *  $handle` |
|      - | 4257 | ` *   The file pointer.` |
|      - | 4258 | ` * Return` |
|      - | 4259 | ` *  Total number of characters read from handle and passed through` |
|      - | 4260 | ` *  to the output on success or FALSE on failure.` |
|      - | 4261 | ` */` |
|      2 | 4262 | `static int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4263 | `{` |
|      - | 4264 | `	const ph7_io_stream *pStream;` |
|      - | 4265 | `	io_private *pDev;` |
|      - | 4266 | `	ph7_int64 n,nRead;` |
|      - | 4267 | `	char zBuf[8192];` |
|      - | 4268 | `	int rc;` |
|      3 | 4269 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4270 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4271 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4272 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4273 | `		return PH7_OK;` |
|      - | 4274 | `	}` |
|      - | 4275 | `	/* Extract our private data */` |
|      3 | 4276 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4277 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4278 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4279 | `		/*Expecting an IO handle */` |
|    ! 0 | 4280 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4281 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4282 | `		return PH7_OK;` |
|      - | 4283 | `	}` |
|      - | 4284 | `	/* Point to the target IO stream device */` |
|      3 | 4285 | `	pStream = pDev->pStream;` |
|      3 | 4286 | `	if( pStream == 0  ){` |
|    ! 0 | 4287 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4288 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4289 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4290 | `			);` |
|    ! 0 | 4291 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4292 | `		return PH7_OK;` |
|      - | 4293 | `	}` |
|      - | 4294 | `	/* Perform the requested operation */` |
|      3 | 4295 | `	nRead = 0;` |
|      2 | 4296 | `	for(;;){` |
|      5 | 4297 | `		n = StreamRead(pDev,zBuf,sizeof(zBuf));` |
|      5 | 4298 | `		if( n < 1 ){` |
|      - | 4299 | `			/* Error or EOF */` |
|      3 | 4300 | `			break;` |
|      - | 4301 | `		}` |
|      - | 4302 | `		/* Increment the read counter */` |
|      3 | 4303 | `		nRead += n;` |
|      - | 4304 | `		/* Output data */` |
|      3 | 4305 | `		rc = ph7_context_output(pCtx,zBuf,(int)nRead /* FIXME: 64-bit issues */);` |
|      3 | 4306 | `		if( rc == PH7_ABORT ){` |
|      - | 4307 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 4308 | `			break;` |
|      - | 4309 | `		}` |
|      1 | 4310 | `	}` |
|      - | 4311 | `	/* Total number of bytes readen */` |
|      3 | 4312 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 4313 | `	return PH7_OK;` |
|      2 | 4314 | `}` |
|      - | 4315 | `/* CSV reader/writer private data */` |
|      - | 4316 | `struct csv_data` |
|      - | 4317 | `{` |
|      - | 4318 | `	int delimiter;    /* Delimiter. Default ',' */` |
|      - | 4319 | `	int enclosure;    /* Enclosure. Default '"'*/` |
|      - | 4320 | `	io_private *pDev; /* Open stream handle */` |
|      - | 4321 | `	int iCount;       /* Counter */` |
|      - | 4322 | `};` |
|      - | 4323 | `/*` |
|      - | 4324 | ` * The following callback is used by the fputcsv() function inorder to iterate` |
|      - | 4325 | ` * throw array entries and output CSV data based on the current key and it's` |
|      - | 4326 | ` * associated data.` |
|      - | 4327 | ` */` |
|      6 | 4328 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 4329 | `{` |
|      7 | 4330 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 4331 | `	const char *zData;` |
|      - | 4332 | `	int nLen,c2;` |
|      - | 4333 | `	sxu32 n;` |
|      - | 4334 | `	/* Point to the raw data */` |
|      7 | 4335 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      7 | 4336 | `	if( nLen < 1 ){` |
|      - | 4337 | `		/* Nothing to write */` |
|    ! 0 | 4338 | `		return PH7_OK;` |
|      - | 4339 | `	}` |
|      7 | 4340 | `	if( pData->iCount > 0 ){` |
|      - | 4341 | `		/* Write the delimiter */` |
|      5 | 4342 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->delimiter,sizeof(char));` |
|      2 | 4343 | `	}` |
|      7 | 4344 | `	n = 1;` |
|      7 | 4345 | `	c2 = 0;` |
|     10 | 4346 | `	if( SyByteFind(zData,(sxu32)nLen,pData->delimiter,0) == SXRET_OK \|\|` |
|      6 | 4347 | `		SyByteFind(zData,(sxu32)nLen,pData->enclosure,&n) == SXRET_OK ){` |
|    ! 0 | 4348 | `			c2 = 1;` |
|    ! 0 | 4349 | `			if( n == 0 ){` |
|    ! 0 | 4350 | `				c2 = 2;` |
|    ! 0 | 4351 | `			}` |
|      - | 4352 | `			/* Write the enclosure */` |
|    ! 0 | 4353 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4354 | `			if( c2 > 1 ){` |
|    ! 0 | 4355 | `				pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4356 | `			}` |
|    ! 0 | 4357 | `	}` |
|      - | 4358 | `	/* Write the data */` |
|      7 | 4359 | `	if( pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)zData,(ph7_int64)nLen) < 1 ){` |
|    ! 0 | 4360 | `		SXUNUSED(pKey); /* cc warning */` |
|    ! 0 | 4361 | `		return PH7_ABORT;` |
|      - | 4362 | `	}` |
|      7 | 4363 | `	if( c2 > 0 ){` |
|      - | 4364 | `		/* Write the enclosure */` |
|    ! 0 | 4365 | `		pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4366 | `		if( c2 > 1 ){` |
|    ! 0 | 4367 | `			pData->pDev->pStream->xWrite(pData->pDev->pHandle,(const void *)&pData->enclosure,sizeof(char));` |
|    ! 0 | 4368 | `		}` |
|    ! 0 | 4369 | `	}` |
|      7 | 4370 | `	pData->iCount++;` |
|      7 | 4371 | `	return PH7_OK;` |
|      4 | 4372 | `}` |
|      - | 4373 | `/*` |
|      - | 4374 | ` * int fputcsv(resource $handle,array $fields[,string $delimiter = ','[,string $enclosure = '"' ]])` |
|      - | 4375 | ` *  Format line as CSV and write to file pointer.` |
|      - | 4376 | ` * Parameters` |
|      - | 4377 | ` *  $handle` |
|      - | 4378 | ` *   Open file handle.` |
|      - | 4379 | ` * $fields` |
|      - | 4380 | ` *   An array of values.` |
|      - | 4381 | ` * $delimiter` |
|      - | 4382 | ` *   The optional delimiter parameter sets the field delimiter (one character only).` |
|      - | 4383 | ` * $enclosure` |
|      - | 4384 | ` *  The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 4385 | ` */` |
|      2 | 4386 | `static int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4387 | `{` |
|      - | 4388 | `	const ph7_io_stream *pStream;` |
|      - | 4389 | `	struct csv_data sCsv;` |
|      - | 4390 | `	io_private *pDev;` |
|      - | 4391 | `	char *zEol;` |
|      - | 4392 | `	int eolen;` |
|      3 | 4393 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4394 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4395 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|    ! 0 | 4396 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4397 | `		return PH7_OK;` |
|      - | 4398 | `	}` |
|      - | 4399 | `	/* Extract our private data */` |
|      3 | 4400 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4401 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4402 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4403 | `		/*Expecting an IO handle */` |
|    ! 0 | 4404 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4405 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4406 | `		return PH7_OK;` |
|      - | 4407 | `	}` |
|      - | 4408 | `	/* Point to the target IO stream device */` |
|      3 | 4409 | `	pStream = pDev->pStream;` |
|      3 | 4410 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 4411 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4412 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4413 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4414 | `			);` |
|    ! 0 | 4415 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4416 | `		return PH7_OK;` |
|      - | 4417 | `	}` |
|      - | 4418 | `	/* Set default csv separator */` |
|      3 | 4419 | `	sCsv.delimiter = ',';` |
|      3 | 4420 | `	sCsv.enclosure = '"';` |
|      3 | 4421 | `	sCsv.pDev = pDev;` |
|      3 | 4422 | `	sCsv.iCount = 0;` |
|      3 | 4423 | `	if( nArg > 2 ){` |
|      - | 4424 | `		/* User delimiter */` |
|      - | 4425 | `		const char *z;` |
|      - | 4426 | `		int n;` |
|      3 | 4427 | `		z = ph7_value_to_string(apArg[2],&n);` |
|      3 | 4428 | `		if( n > 0 ){` |
|      3 | 4429 | `			sCsv.delimiter = z[0];` |
|      1 | 4430 | `		}` |
|      3 | 4431 | `		if( nArg > 3 ){` |
|      3 | 4432 | `			z = ph7_value_to_string(apArg[3],&n);` |
|      3 | 4433 | `			if( n > 0 ){` |
|      3 | 4434 | `				sCsv.enclosure = z[0];` |
|      1 | 4435 | `			}` |
|      1 | 4436 | `		}` |
|      1 | 4437 | `	}` |
|      - | 4438 | `	/* Iterate throw array entries and write csv data */` |
|      3 | 4439 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|      - | 4440 | `	/* Write a line ending */` |
|      - | 4441 | `#ifdef __WINNT__` |
|      1 | 4442 | `	zEol = "\r\n";` |
|      1 | 4443 | `	eolen = (int)sizeof("\r\n")-1;` |
|      - | 4444 | `#else` |
|      - | 4445 | `	/* Assume UNIX LF */` |
|      2 | 4446 | `	zEol = "\n";` |
|      2 | 4447 | `	eolen = (int)sizeof(char);` |
|      - | 4448 | `#endif` |
|      3 | 4449 | `	pDev->pStream->xWrite(pDev->pHandle,(const void *)zEol,eolen);` |
|      3 | 4450 | `	return PH7_OK;` |
|      2 | 4451 | `}` |
|      - | 4452 | `/*` |
|      - | 4453 | ` * fprintf,vfprintf private data.` |
|      - | 4454 | ` * An instance of the following structure is passed to the formatted` |
|      - | 4455 | ` * input consumer callback defined below.` |
|      - | 4456 | ` */` |
|      - | 4457 | `typedef struct fprintf_data fprintf_data;` |
|      - | 4458 | `struct fprintf_data` |
|      - | 4459 | `{` |
|      - | 4460 | `	io_private *pIO;        /* IO stream */` |
|      - | 4461 | `	ph7_int64 nCount;       /* Total number of bytes written */` |
|      - | 4462 | `};` |
|      - | 4463 | `/*` |
|      - | 4464 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 4465 | ` */` |
|     38 | 4466 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4467 | `{` |
|     39 | 4468 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 4469 | `	ph7_int64 n;` |
|      - | 4470 | `	/* Write the formatted data */` |
|     39 | 4471 | `	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);` |
|     39 | 4472 | `	if( n < 1 ){` |
|    ! 0 | 4473 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 4474 | `		/* IO error,abort immediately */` |
|    ! 0 | 4475 | `		return SXERR_ABORT;` |
|      - | 4476 | `	}` |
|      - | 4477 | `	/* Increment counter */` |
|     39 | 4478 | `	pFdata->nCount += n;` |
|     39 | 4479 | `	return PH7_OK;` |
|     20 | 4480 | `}` |
|      - | 4481 | `/*` |
|      - | 4482 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 4483 | ` *  Write a formatted string to a stream.` |
|      - | 4484 | ` * Parameters` |
|      - | 4485 | ` *  $handle` |
|      - | 4486 | ` *   The file pointer.` |
|      - | 4487 | ` *  $format` |
|      - | 4488 | ` *   String format (see sprintf()).` |
|      - | 4489 | ` * Return` |
|      - | 4490 | ` *  The length of the written string.` |
|      - | 4491 | ` */` |
|     16 | 4492 | `static int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4493 | `{` |
|      - | 4494 | `	fprintf_data sFdata;` |
|      - | 4495 | `	const char *zFormat;` |
|      - | 4496 | `	io_private *pDev;` |
|      - | 4497 | `	int nLen;` |
|     17 | 4498 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 4499 | `		/* Missing/Invalid arguments,return zero */` |
|    ! 0 | 4500 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 4501 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4502 | `		return PH7_OK;` |
|      - | 4503 | `	}` |
|      - | 4504 | `	/* Extract our private data */` |
|     17 | 4505 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4506 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     17 | 4507 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4508 | `		/*Expecting an IO handle */` |
|    ! 0 | 4509 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4510 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4511 | `		return PH7_OK;` |
|      - | 4512 | `	}` |
|      - | 4513 | `	/* Point to the target IO stream device */` |
|     17 | 4514 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4515 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4516 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4517 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4518 | `			);` |
|    ! 0 | 4519 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4520 | `		return PH7_OK;` |
|      - | 4521 | `	}` |
|      - | 4522 | `	/* Extract the string format */` |
|     17 | 4523 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     17 | 4524 | `	if( nLen < 1 ){` |
|      - | 4525 | `		/* Empty string,return zero */` |
|    ! 0 | 4526 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4527 | `		return PH7_OK;` |
|      - | 4528 | `	}` |
|      - | 4529 | `	/* Prepare our private data */` |
|     17 | 4530 | `	sFdata.nCount = 0;` |
|     17 | 4531 | `	sFdata.pIO = pDev;` |
|      - | 4532 | `	/* Format the string */` |
|     17 | 4533 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 4534 | `	/* Return total number of bytes written */` |
|     17 | 4535 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|     17 | 4536 | `	return PH7_OK;` |
|      9 | 4537 | `}` |
|      - | 4538 | `/*` |
|      - | 4539 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 4540 | ` *  Write a formatted string to a stream.` |
|      - | 4541 | ` * Parameters` |
|      - | 4542 | ` *  $handle` |
|      - | 4543 | ` *   The file pointer.` |
|      - | 4544 | ` *  $format` |
|      - | 4545 | ` *   String format (see sprintf()).` |
|      - | 4546 | ` * $args` |
|      - | 4547 | ` *   User arguments.` |
|      - | 4548 | ` * Return` |
|      - | 4549 | ` *  The length of the written string.` |
|      - | 4550 | ` */` |
|      4 | 4551 | `static int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4552 | `{` |
|      - | 4553 | `	fprintf_data sFdata;` |
|      - | 4554 | `	const char *zFormat;` |
|      - | 4555 | `	ph7_hashmap *pMap;` |
|      - | 4556 | `	io_private *pDev;` |
|      - | 4557 | `	SySet sArg;` |
|      - | 4558 | `	int n,nLen;` |
|      5 | 4559 | `	if( nArg < 3 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_string(apArg[1])  \|\| !ph7_value_is_array(apArg[2]) ){` |
|      - | 4560 | `		/* Missing/Invalid arguments,return zero */` |
|      3 | 4561 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|      3 | 4562 | `		ph7_result_int(pCtx,0);` |
|      3 | 4563 | `		return PH7_OK;` |
|      - | 4564 | `	}` |
|      - | 4565 | `	/* Extract our private data */` |
|      3 | 4566 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4567 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 4568 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4569 | `		/*Expecting an IO handle */` |
|    ! 0 | 4570 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4571 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4572 | `		return PH7_OK;` |
|      - | 4573 | `	}` |
|      - | 4574 | `	/* Point to the target IO stream device */` |
|      3 | 4575 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 4576 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4577 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 4578 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 4579 | `			);` |
|    ! 0 | 4580 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4581 | `		return PH7_OK;` |
|      - | 4582 | `	}` |
|      - | 4583 | `	/* Extract the string format */` |
|      3 | 4584 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      3 | 4585 | `	if( nLen < 1 ){` |
|      - | 4586 | `		/* Empty string,return zero */` |
|    ! 0 | 4587 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4588 | `		return PH7_OK;` |
|      - | 4589 | `	}` |
|      - | 4590 | `	/* Point to hashmap */` |
|      3 | 4591 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 4592 | `	/* Extract arguments from the hashmap */` |
|      3 | 4593 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4594 | `	/* Prepare our private data */` |
|      3 | 4595 | `	sFdata.nCount = 0;` |
|      3 | 4596 | `	sFdata.pIO = pDev;` |
|      - | 4597 | `	/* Format the string */` |
|      3 | 4598 | `	PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 4599 | `	/* Return total number of bytes written*/` |
|      3 | 4600 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      3 | 4601 | `	SySetRelease(&sArg);` |
|      3 | 4602 | `	return PH7_OK;` |
|      3 | 4603 | `}` |
|      - | 4604 | `/*` |
|      - | 4605 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 4606 | ` * According to the PHP reference manual:` |
|      - | 4607 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 4608 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 4609 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 4610 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 4611 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 4612 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 4613 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 4614 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 4615 | ` *         exist, attempt to create it.` |
|      - | 4616 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 4617 | ` *          not exist, attempt to create it.` |
|      - | 4618 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 4619 | ` *         already exists,` |
|      - | 4620 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 4621 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 4622 | ` *         the underlying open(2) system call.` |
|      - | 4623 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 4624 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 4625 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 4626 | ` *          is positioned on the beginning of the file.` |
|      - | 4627 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 4628 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 4629 | ` *          be used after the lock is requested).` |
|      - | 4630 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 4631 | ` */` |
|     64 | 4632 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      2 | 4633 | `{` |
|     66 | 4634 | `	const char *zEnd = &zMode[nLen];` |
|     66 | 4635 | `	int iFlag = 0;` |
|      - | 4636 | `	int c;` |
|     66 | 4637 | `	if( nLen < 1 ){` |
|      - | 4638 | `		/* Open in a read-only mode */` |
|    ! 0 | 4639 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 4640 | `	}` |
|     66 | 4641 | `	c = zMode[0];` |
|     66 | 4642 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 4643 | `		/* Read-only access */` |
|     40 | 4644 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|     40 | 4645 | `		zMode++; /* Advance */` |
|     40 | 4646 | `		if( zMode < zEnd ){` |
|      7 | 4647 | `			c = zMode[0];` |
|      7 | 4648 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 4649 | `				/* Read+Write access */` |
|      7 | 4650 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|      3 | 4651 | `			}` |
|      5 | 4652 | `		}` |
|     46 | 4653 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 4654 | `		/* Overwrite mode.` |
|      - | 4655 | `		 * If the file does not exists,try to create it` |
|      - | 4656 | `		 */` |
|     27 | 4657 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|     27 | 4658 | `		zMode++; /* Advance */` |
|     27 | 4659 | `		if( zMode < zEnd ){` |
|      3 | 4660 | `			c = zMode[0];` |
|      3 | 4661 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4662 | `				/* Read+Write access */` |
|      3 | 4663 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|      3 | 4664 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|      1 | 4665 | `			}` |
|      2 | 4666 | `		}` |
|     13 | 4667 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 4668 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 4669 | `		 * Create the file if it does not exists.` |
|      - | 4670 | `		 */` |
|    ! 0 | 4671 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4672 | `		zMode++; /* Advance */` |
|    ! 0 | 4673 | `		if( zMode < zEnd ){` |
|    ! 0 | 4674 | `			c = zMode[0];` |
|    ! 0 | 4675 | `			if( c == '+' ){` |
|      - | 4676 | `				/* Read-Write access */` |
|    ! 0 | 4677 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4678 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4679 | `			}` |
|    ! 0 | 4680 | `		}` |
|    ! 0 | 4681 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 4682 | `		/* Exclusive access.` |
|      - | 4683 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 4684 | `		 * Otherwise create a new file.` |
|      - | 4685 | `		 */` |
|    ! 0 | 4686 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|    ! 0 | 4687 | `		zMode++; /* Advance */` |
|    ! 0 | 4688 | `		if( zMode < zEnd ){` |
|    ! 0 | 4689 | `			c = zMode[0];` |
|    ! 0 | 4690 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 4691 | `				/* Read-Write access */` |
|    ! 0 | 4692 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4693 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4694 | `			}` |
|    ! 0 | 4695 | `		}` |
|    ! 0 | 4696 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 4697 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 4698 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 4699 | `		zMode++; /* Advance */` |
|    ! 0 | 4700 | `		if( zMode < zEnd ){` |
|    ! 0 | 4701 | `			c = zMode[0];` |
|    ! 0 | 4702 | `			if( c == '+' ){` |
|      - | 4703 | `				/* Read-Write access */` |
|    ! 0 | 4704 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 4705 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 4706 | `			}` |
|    ! 0 | 4707 | `		}` |
|    ! 0 | 4708 | `	}else{` |
|      - | 4709 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 4710 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 4711 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 4712 | `	}` |
|     74 | 4713 | `	while( zMode < zEnd ){` |
|      9 | 4714 | `		c = zMode[0];` |
|      9 | 4715 | `		if( c == 'b' \|\| c == 'B' ){` |
|    ! 0 | 4716 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4717 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|      9 | 4718 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 4719 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 4720 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 4721 | `		}` |
|      9 | 4722 | `		zMode++;` |
|      1 | 4723 | `	}` |
|     66 | 4724 | `	return iFlag;` |
|     34 | 4725 | `}` |
|      - | 4726 | `/*` |
|      - | 4727 | ` * Initialize the IO private structure.` |
|      - | 4728 | ` */` |
|   4922 | 4729 | `static void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      5 | 4730 | `{` |
|   4927 | 4731 | `	pOut->pStream = pStream;` |
|   4927 | 4732 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   4927 | 4733 | `	pOut->nOfft = 0;` |
|      - | 4734 | `	/* Set the magic number */` |
|   4927 | 4735 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   4927 | 4736 | `}` |
|      - | 4737 | `/*` |
|      - | 4738 | ` * Release the IO private structure.` |
|      - | 4739 | ` */` |
|   4890 | 4740 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      5 | 4741 | `{` |
|   4895 | 4742 | `	SyBlobRelease(&pDev->sBuffer);` |
|   4895 | 4743 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 4744 | `	/* Release the whole structure */` |
|   4895 | 4745 | `	ph7_context_free_chunk(pCtx,pDev);` |
|   4895 | 4746 | `}` |
|      - | 4747 | `/*` |
|      - | 4748 | ` * Reset the IO private structure.` |
|      - | 4749 | ` */` |
|     12 | 4750 | `static void ResetIOPrivate(io_private *pDev)` |
|      1 | 4751 | `{` |
|     13 | 4752 | `	SyBlobReset(&pDev->sBuffer);` |
|     13 | 4753 | `	pDev->nOfft = 0;` |
|     13 | 4754 | `}` |
|      - | 4755 | `/* Forward declaration */` |
|      - | 4756 | `static int is_php_stream(const ph7_io_stream *pStream);` |
|      - | 4757 | `/*` |
|      - | 4758 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 4759 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 4760 | ` * Parameters` |
|      - | 4761 | ` *  $filename` |
|      - | 4762 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 4763 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 4764 | ` *   then a regular file is assumed.` |
|      - | 4765 | ` *  $mode` |
|      - | 4766 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 4767 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 4768 | ` *   modes.` |
|      - | 4769 | ` *  $use_include_path` |
|      - | 4770 | ` *   You can use the optional second parameter and set it to` |
|      - | 4771 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 4772 | ` *  $context` |
|      - | 4773 | ` *   A context stream resource.` |
|      - | 4774 | ` * Return` |
|      - | 4775 | ` *  File handle on success or FALSE on failure.` |
|      - | 4776 | ` */` |
|     64 | 4777 | `static int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4778 | `{` |
|      - | 4779 | `	const ph7_io_stream *pStream;` |
|      - | 4780 | `	const char *zUri,*zMode;` |
|      - | 4781 | `	ph7_value *pResource;` |
|      - | 4782 | `	io_private *pDev;` |
|      - | 4783 | `	int iLen,imLen;` |
|      - | 4784 | `	int iOpenFlags;` |
|     66 | 4785 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4786 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4787 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 4788 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4789 | `		return PH7_OK;` |
|      - | 4790 | `	}` |
|      - | 4791 | `	/* Extract the URI and the desired access mode */` |
|     66 | 4792 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|     66 | 4793 | `	if( nArg > 1 ){` |
|     66 | 4794 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|     34 | 4795 | `	}else{` |
|      - | 4796 | `		/* Set a default read-only mode */` |
|    ! 0 | 4797 | `		zMode = "r";` |
|    ! 0 | 4798 | `		imLen = (int)sizeof(char);` |
|      - | 4799 | `	}` |
|      - | 4800 | `	/* Try to extract a stream */` |
|     66 | 4801 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|     66 | 4802 | `	if( pStream == 0 ){` |
|    ! 0 | 4803 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 4804 | `			"No stream device is associated with the given URI(%s)",zUri);` |
|    ! 0 | 4805 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4806 | `		return PH7_OK;` |
|      - | 4807 | `	}` |
|      - | 4808 | `	/* Allocate a new IO private instance */` |
|     66 | 4809 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     66 | 4810 | `	if( pDev == 0 ){` |
|    ! 0 | 4811 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4812 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4813 | `		return PH7_OK;` |
|      - | 4814 | `	}` |
|     66 | 4815 | `	pResource = 0;` |
|     66 | 4816 | `	if( nArg > 3 ){` |
|    ! 0 | 4817 | `		pResource = apArg[3];` |
|     66 | 4818 | `	}else if( is_php_stream(pStream) ){` |
|      - | 4819 | `		/* TICKET 1433-80: The php:// stream need a ph7_value to access the underlying` |
|      - | 4820 | `		 * virtual machine.` |
|      - | 4821 | `		 */` |
|      3 | 4822 | `		pResource = apArg[0];` |
|      1 | 4823 | `	}` |
|      - | 4824 | `	/* Initialize the structure */` |
|     66 | 4825 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 4826 | `	/* Convert open mode to PH7 flags */` |
|     66 | 4827 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|      - | 4828 | `	/* Try to get a handle */` |
|     98 | 4829 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|     32 | 4830 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);` |
|     66 | 4831 | `	if( pDev->pHandle == 0 ){` |
|    ! 0 | 4832 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zUri);` |
|    ! 0 | 4833 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4834 | `		ph7_context_free_chunk(pCtx,pDev);` |
|    ! 0 | 4835 | `		return PH7_OK;` |
|      - | 4836 | `	}` |
|      - | 4837 | `	/* All done,return the io_private instance as a resource */` |
|     66 | 4838 | `	ph7_result_resource(pCtx,pDev);` |
|     66 | 4839 | `	return PH7_OK;` |
|     34 | 4840 | `}` |
|      - | 4841 | `/*` |
|      - | 4842 | ` * bool fclose(resource $handle)` |
|      - | 4843 | ` *  Closes an open file pointer` |
|      - | 4844 | ` * Parameters` |
|      - | 4845 | ` *  $handle` |
|      - | 4846 | ` *   The file pointer.` |
|      - | 4847 | ` * Return` |
|      - | 4848 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4849 | ` */` |
|    156 | 4850 | `static int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 4851 | `{` |
|      - | 4852 | `	const ph7_io_stream *pStream;` |
|      - | 4853 | `	io_private *pDev;` |
|      - | 4854 | `	ph7_vm *pVm;` |
|    161 | 4855 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 4856 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4857 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4858 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4859 | `		return PH7_OK;` |
|      - | 4860 | `	}` |
|      - | 4861 | `	/* Extract our private data */` |
|    161 | 4862 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 4863 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    161 | 4864 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4865 | `		/*Expecting an IO handle */` |
|    ! 0 | 4866 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 4867 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4868 | `		return PH7_OK;` |
|      - | 4869 | `	}` |
|      - | 4870 | `	/* Point to the target IO stream device */` |
|    161 | 4871 | `	pStream = pDev->pStream;` |
|    161 | 4872 | `	if( pStream == 0 ){` |
|    ! 0 | 4873 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4874 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 4875 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 4876 | `			);` |
|    ! 0 | 4877 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4878 | `		return PH7_OK;` |
|      - | 4879 | `	}` |
|      - | 4880 | `	/* Point to the VM that own this context */` |
|    161 | 4881 | `	pVm = pCtx->pVm;` |
|      - | 4882 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|    161 | 4883 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 4884 | `		/* Perform the requested operation */` |
|    161 | 4885 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 4886 | `		/* Release the IO private structure */` |
|    161 | 4887 | `		ReleaseIOPrivate(pCtx,pDev);` |
|      - | 4888 | `		/* Invalidate the resource handle */` |
|    161 | 4889 | `		ph7_value_release(apArg[0]);` |
|     78 | 4890 | `	}` |
|      - | 4891 | `	/* Return TRUE */` |
|    161 | 4892 | `	ph7_result_bool(pCtx,1);` |
|    161 | 4893 | `	return PH7_OK;` |
|     83 | 4894 | `}` |
|      - | 4895 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4896 | `/*` |
|      - | 4897 | ` * MD5/SHA1 digest consumer.` |
|      - | 4898 | ` */` |
|     72 | 4899 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 4900 | `{` |
|      - | 4901 | `	/* Append hex chunk verbatim */` |
|     73 | 4902 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|     73 | 4903 | `	return SXRET_OK;` |
|      1 | 4904 | `}` |
|      - | 4905 | `/*` |
|      - | 4906 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 4907 | ` *  Calculates the md5 hash of a given file.` |
|      - | 4908 | ` * Parameters` |
|      - | 4909 | ` *  $uri` |
|      - | 4910 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 4911 | ` *  $raw_output` |
|      - | 4912 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 4913 | ` * Return` |
|      - | 4914 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 4915 | ` */` |
|      2 | 4916 | `static int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4917 | `{` |
|      - | 4918 | `	const ph7_io_stream *pStream;` |
|      - | 4919 | `	unsigned char zDigest[16];` |
|      3 | 4920 | `	int raw_output  = FALSE;` |
|      - | 4921 | `	const char *zFile;` |
|      - | 4922 | `	MD5Context sCtx;` |
|      - | 4923 | `	char zBuf[8192];` |
|      - | 4924 | `	void *pHandle;` |
|      - | 4925 | `	ph7_int64 n;` |
|      - | 4926 | `	int nLen;` |
|      3 | 4927 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4928 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4929 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 4930 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4931 | `		return PH7_OK;` |
|      - | 4932 | `	}` |
|      - | 4933 | `	/* Extract the file path */` |
|      3 | 4934 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4935 | `	/* Point to the target IO stream device */` |
|      3 | 4936 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 4937 | `	if( pStream == 0 ){` |
|    ! 0 | 4938 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 4939 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4940 | `		return PH7_OK;` |
|      - | 4941 | `	}` |
|      3 | 4942 | `	if( nArg > 1 ){` |
|    ! 0 | 4943 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 4944 | `	}` |
|      - | 4945 | `	/* Try to open the file in read-only mode */` |
|      3 | 4946 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 4947 | `	if( pHandle == 0 ){` |
|    ! 0 | 4948 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 4949 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4950 | `		return PH7_OK;` |
|      - | 4951 | `	}` |
|      - | 4952 | `	/* Init the MD5 context */` |
|      3 | 4953 | `	MD5Init(&sCtx);` |
|      - | 4954 | `	/* Perform the requested operation */` |
|      2 | 4955 | `	for(;;){` |
|      5 | 4956 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 4957 | `		if( n < 1 ){` |
|      - | 4958 | `			/* EOF or IO error,break immediately */` |
|      3 | 4959 | `			break;` |
|      - | 4960 | `		}` |
|      3 | 4961 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 4962 | `	}` |
|      - | 4963 | `	/* Close the stream */` |
|      3 | 4964 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 4965 | `	/* Extract the digest */` |
|      3 | 4966 | `	MD5Final(zDigest,&sCtx);` |
|      3 | 4967 | `	if( raw_output ){` |
|      - | 4968 | `		/* Output raw digest */` |
|    ! 0 | 4969 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 4970 | `	}else{` |
|      - | 4971 | `		/* Perform a binary to hex conversion */` |
|      3 | 4972 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 4973 | `	}` |
|      3 | 4974 | `	return PH7_OK;` |
|      2 | 4975 | `}` |
|      - | 4976 | `/*` |
|      - | 4977 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 4978 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 4979 | ` * Parameters` |
|      - | 4980 | ` *  $uri` |
|      - | 4981 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 4982 | ` *  $raw_output` |
|      - | 4983 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 4984 | ` * Return` |
|      - | 4985 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 4986 | ` */` |
|      2 | 4987 | `static int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4988 | `{` |
|      - | 4989 | `	const ph7_io_stream *pStream;` |
|      - | 4990 | `	unsigned char zDigest[20];` |
|      3 | 4991 | `	int raw_output  = FALSE;` |
|      - | 4992 | `	const char *zFile;` |
|      - | 4993 | `	SHA1Context sCtx;` |
|      - | 4994 | `	char zBuf[8192];` |
|      - | 4995 | `	void *pHandle;` |
|      - | 4996 | `	ph7_int64 n;` |
|      - | 4997 | `	int nLen;` |
|      3 | 4998 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4999 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5000 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5001 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5002 | `		return PH7_OK;` |
|      - | 5003 | `	}` |
|      - | 5004 | `	/* Extract the file path */` |
|      3 | 5005 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5006 | `	/* Point to the target IO stream device */` |
|      3 | 5007 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5008 | `	if( pStream == 0 ){` |
|    ! 0 | 5009 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5010 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5011 | `		return PH7_OK;` |
|      - | 5012 | `	}` |
|      3 | 5013 | `	if( nArg > 1 ){` |
|    ! 0 | 5014 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 5015 | `	}` |
|      - | 5016 | `	/* Try to open the file in read-only mode */` |
|      3 | 5017 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5018 | `	if( pHandle == 0 ){` |
|    ! 0 | 5019 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5020 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5021 | `		return PH7_OK;` |
|      - | 5022 | `	}` |
|      - | 5023 | `	/* Init the SHA1 context */` |
|      3 | 5024 | `	SHA1Init(&sCtx);` |
|      - | 5025 | `	/* Perform the requested operation */` |
|      2 | 5026 | `	for(;;){` |
|      5 | 5027 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 5028 | `		if( n < 1 ){` |
|      - | 5029 | `			/* EOF or IO error,break immediately */` |
|      3 | 5030 | `			break;` |
|      - | 5031 | `		}` |
|      3 | 5032 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 5033 | `	}` |
|      - | 5034 | `	/* Close the stream */` |
|      3 | 5035 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5036 | `	/* Extract the digest */` |
|      3 | 5037 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 5038 | `	if( raw_output ){` |
|      - | 5039 | `		/* Output raw digest */` |
|    ! 0 | 5040 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 5041 | `	}else{` |
|      - | 5042 | `		/* Perform a binary to hex conversion */` |
|      3 | 5043 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 5044 | `	}` |
|      3 | 5045 | `	return PH7_OK;` |
|      2 | 5046 | `}` |
|      - | 5047 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5048 | `/*` |
|      - | 5049 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 5050 | ` *  Parse a configuration file.` |
|      - | 5051 | ` * Parameters` |
|      - | 5052 | ` * $filename` |
|      - | 5053 | ` *  The filename of the ini file being parsed.` |
|      - | 5054 | ` * $process_sections` |
|      - | 5055 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 5056 | ` *  with the section names and settings included.` |
|      - | 5057 | ` *  The default for process_sections is FALSE.` |
|      - | 5058 | ` * $scanner_mode` |
|      - | 5059 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 5060 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 5061 | ` * Return` |
|      - | 5062 | ` *  The settings are returned as an associative array on success.` |
|      - | 5063 | ` *  Otherwise is returned.` |
|      - | 5064 | ` */` |
|      2 | 5065 | `static int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5066 | `{` |
|      - | 5067 | `	const ph7_io_stream *pStream;` |
|      - | 5068 | `	const char *zFile;` |
|      - | 5069 | `	SyBlob sContents;` |
|      - | 5070 | `	void *pHandle;` |
|      - | 5071 | `	int nLen;` |
|      3 | 5072 | `	sxi32 rc = PH7_OK;` |
|      3 | 5073 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5074 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 5075 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 5076 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5077 | `		return PH7_OK;` |
|      - | 5078 | `	}` |
|      - | 5079 | `	/* Extract the file path */` |
|      3 | 5080 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 5081 | `	/* Point to the target IO stream device */` |
|      3 | 5082 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 5083 | `	if( pStream == 0 ){` |
|    ! 0 | 5084 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|    ! 0 | 5085 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5086 | `		return PH7_OK;` |
|      - | 5087 | `	}` |
|      - | 5088 | `	/* Try to open the file in read-only mode */` |
|      3 | 5089 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|      3 | 5090 | `	if( pHandle == 0 ){` |
|    ! 0 | 5091 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    ! 0 | 5092 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5093 | `		return PH7_OK;` |
|      - | 5094 | `	}` |
|      3 | 5095 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 5096 | `	/* Read the whole file */` |
|      3 | 5097 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      3 | 5098 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 5099 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 5100 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5101 | `	}else{` |
|      - | 5102 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|      5 | 5103 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      2 | 5104 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0);` |
|      - | 5105 | `	}` |
|      - | 5106 | `	/* Close the stream */` |
|      3 | 5107 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 5108 | `	/* Release the working buffer */` |
|      3 | 5109 | `	SyBlobRelease(&sContents);` |
|      - | 5110 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      3 | 5111 | `	return rc;` |
|      2 | 5112 | `}` |
|      - | 5113 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 5114 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 5115 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 5116 | `#if defined(_MSC_VER)` |
|      - | 5117 | `static const ph7_vfs null_vfs = {` |
|      - | 5118 | `#else` |
|      - | 5119 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 5120 | `#endif` |
|      - | 5121 | `	"null_vfs",` |
|      - | 5122 | `	PH7_VFS_VERSION,` |
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
|      - | 5158 |  |
|      - | 5159 |  |
|      - | 5160 |  |
|      - | 5161 |  |
|      - | 5162 |  |
|      - | 5163 |  |
|      - | 5164 | `};` |
|      - | 5165 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 5166 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 5167 | `/*` |
|      - | 5168 | ` * Export the builtin vfs.` |
|      - | 5169 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 5170 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 5171 | ` * Note:` |
|      - | 5172 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 5173 | ` * Note:` |
|      - | 5174 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 5175 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 5176 | ` */` |
|   3816 | 5177 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 5178 | `{` |
|      - | 5179 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 5180 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 5181 | `	return &null_vfs;` |
|      - | 5182 | `#else` |
|      - | 5183 | `#ifdef __WINNT__` |
|      5 | 5184 | `	return &sWinVfs;` |
|      - | 5185 | `#elif defined(__UNIXES__)` |
|   3816 | 5186 | `	return &sUnixVfs;` |
|      - | 5187 | `#else` |
|      - | 5188 | `	return &null_vfs;` |
|      - | 5189 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 5190 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 5191 | `#else` |
|      - | 5192 | `	return &null_vfs;` |
|      - | 5193 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 5194 | `}` |
|      - | 5195 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 5196 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 5197 | `/*` |
|      - | 5198 | ` * The following defines are mostly used by the UNIX built and have` |
|      - | 5199 | ` * no particular meaning on windows.` |
|      - | 5200 | ` */` |
|      - | 5201 | `#ifndef STDIN_FILENO` |
|      - | 5202 | `#define STDIN_FILENO	0` |
|      - | 5203 | `#endif` |
|      - | 5204 | `#ifndef STDOUT_FILENO` |
|      - | 5205 | `#define STDOUT_FILENO	1` |
|      - | 5206 | `#endif` |
|      - | 5207 | `#ifndef STDERR_FILENO` |
|      - | 5208 | `#define STDERR_FILENO	2` |
|      - | 5209 | `#endif` |
|      - | 5210 | `/*` |
|      - | 5211 | ` * php:// Accessing various I/O streams` |
|      - | 5212 | ` * According to the PHP langage reference manual` |
|      - | 5213 | ` * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input` |
|      - | 5214 | ` * and output streams, the standard input, output and error file descriptors.` |
|      - | 5215 | ` * php://stdin, php://stdout and php://stderr:` |
|      - | 5216 | ` *  Allow direct access to the corresponding input or output stream of the PHP process.` |
|      - | 5217 | ` *  The stream references a duplicate file descriptor, so if you open php://stdin and later` |
|      - | 5218 | ` *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.` |
|      - | 5219 | ` *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.` |
|      - | 5220 | ` * php://output` |
|      - | 5221 | ` *  php://output is a write-only stream that allows you to write to the output buffer` |
|      - | 5222 | ` *  mechanism in the same way as print and echo.` |
|      - | 5223 | ` */` |
|      - | 5224 | `typedef struct ph7_stream_data ph7_stream_data;` |
|      - | 5225 | `/* Supported IO streams */` |
|      - | 5226 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|      - | 5227 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|      - | 5228 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|      - | 5229 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|      - | 5230 | ` /* The following structure is the private data associated with the php:// stream */` |
|      - | 5231 | `struct ph7_stream_data` |
|      - | 5232 | `{` |
|      - | 5233 | `	ph7_vm *pVm; /* VM that own this instance */` |
|      - | 5234 | `	int iType;   /* Stream type */` |
|      - | 5235 | `	union{` |
|      - | 5236 | `		void *pHandle; /* Stream handle */` |
|      - | 5237 | `		ph7_output_consumer sConsumer; /* VM output consumer */` |
|      - | 5238 | `	}x;` |
|      - | 5239 | `};` |
|      - | 5240 | `/*` |
|      - | 5241 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|      - | 5242 | ` */` |
|      8 | 5243 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|      1 | 5244 | `{` |
|      - | 5245 | `	ph7_stream_data *pData;` |
|      9 | 5246 | `	if( pVm == 0 ){` |
|    ! 0 | 5247 | `		return 0;` |
|      - | 5248 | `	}` |
|      - | 5249 | `	/* Allocate a new instance */` |
|      9 | 5250 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|      9 | 5251 | `	if( pData == 0 ){` |
|    ! 0 | 5252 | `		return 0;` |
|      - | 5253 | `	}` |
|      - | 5254 | `	/* Zero the structure */` |
|      9 | 5255 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|      - | 5256 | `	/* Initialize fields */` |
|      9 | 5257 | `	pData->iType = iType;` |
|      9 | 5258 | `	if( iType == PH7_IO_STREAM_OUTPUT ){` |
|      - | 5259 | `		/* Point to the default VM consumer routine. */` |
|      3 | 5260 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|      2 | 5261 | `	}else{` |
|      - | 5262 | `#ifdef __WINNT__` |
|      - | 5263 | `		DWORD nChannel;` |
|      1 | 5264 | `		switch(iType){` |
|      1 | 5265 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|      1 | 5266 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|      - | 5267 | `		default:` |
|      1 | 5268 | `			nChannel = STD_INPUT_HANDLE;` |
|      - | 5269 | `			break;` |
|      - | 5270 | `		}` |
|      1 | 5271 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|      - | 5272 | `#else` |
|      - | 5273 | `		/* Assume an UNIX system */` |
|      6 | 5274 | `		int ifd = STDIN_FILENO;` |
|      6 | 5275 | `		switch(iType){` |
|      2 | 5276 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|      2 | 5277 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|      1 | 5278 | `		default:` |
|      2 | 5279 | `			break;` |
|      - | 5280 | `		}` |
|      6 | 5281 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|      - | 5282 | `#endif` |
|      - | 5283 | `	}` |
|      9 | 5284 | `	pData->pVm = pVm;` |
|      9 | 5285 | `	return pData;` |
|      5 | 5286 | `}` |
|      - | 5287 | `/*` |
|      - | 5288 | ` * Implementation of the php:// IO streams routines` |
|      - | 5289 | ` * Status:` |
|      - | 5290 | ` *   Stable.` |
|      - | 5291 | ` */` |
|      - | 5292 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|      2 | 5293 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|      1 | 5294 | `{` |
|      - | 5295 | `	ph7_stream_data *pData;` |
|      - | 5296 | `	SyString sStream;` |
|      3 | 5297 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|      - | 5298 | `	/* Trim leading and trailing white spaces */` |
|      3 | 5299 | `	SyStringFullTrim(&sStream);` |
|      - | 5300 | `	/* Stream to open */` |
|      3 | 5301 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|    ! 0 | 5302 | `		iMode = PH7_IO_STREAM_STDIN;` |
|      3 | 5303 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|      3 | 5304 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|      1 | 5305 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|    ! 0 | 5306 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|    ! 0 | 5307 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|    ! 0 | 5308 | `		iMode = PH7_IO_STREAM_STDERR;` |
|    ! 0 | 5309 | `	}else{` |
|      - | 5310 | `		/* unknown stream name */` |
|    ! 0 | 5311 | `		return -1;` |
|      - | 5312 | `	}` |
|      - | 5313 | `	/* Create our handle */` |
|      3 | 5314 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|      3 | 5315 | `	if( pData == 0 ){` |
|    ! 0 | 5316 | `		return -1;` |
|      - | 5317 | `	}` |
|      - | 5318 | `	/* Make the handle public */` |
|      3 | 5319 | `	*ppHandle = (void *)pData;` |
|      3 | 5320 | `	return PH7_OK;` |
|      2 | 5321 | `}` |
|      - | 5322 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|    ! 0 | 5323 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|    ! 0 | 5324 | `{` |
|    ! 0 | 5325 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    ! 0 | 5326 | `	if( pData == 0 ){` |
|    ! 0 | 5327 | `		return -1;` |
|      - | 5328 | `	}` |
|    ! 0 | 5329 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|      - | 5330 | `		/* Forbidden */` |
|    ! 0 | 5331 | `		return -1;` |
|      - | 5332 | `	}` |
|      - | 5333 | `#ifdef __WINNT__` |
|      - | 5334 | `	{` |
|      - | 5335 | `		DWORD nRd;` |
|      - | 5336 | `		BOOL rc;` |
|    ! 0 | 5337 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|    ! 0 | 5338 | `		if( !rc ){` |
|      - | 5339 | `			/* IO error */` |
|    ! 0 | 5340 | `			return -1;` |
|      - | 5341 | `		}` |
|    ! 0 | 5342 | `		return (ph7_int64)nRd;` |
|      - | 5343 | `	}` |
|      - | 5344 | `#elif defined(__UNIXES__)` |
|      - | 5345 | `	{` |
|      - | 5346 | `		ssize_t nRd;` |
|      - | 5347 | `		int fd;` |
|    ! 0 | 5348 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 5349 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|    ! 0 | 5350 | `		if( nRd < 1 ){` |
|    ! 0 | 5351 | `			return -1;` |
|      - | 5352 | `		}` |
|    ! 0 | 5353 | `		return (ph7_int64)nRd;` |
|      - | 5354 | `	}` |
|      - | 5355 | `#else` |
|      - | 5356 | `	return -1;` |
|      - | 5357 | `#endif` |
|    ! 0 | 5358 | `}` |
|      - | 5359 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|      2 | 5360 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 5361 | `{` |
|      3 | 5362 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      3 | 5363 | `	if( pData == 0 ){` |
|    ! 0 | 5364 | `		return -1;` |
|      - | 5365 | `	}` |
|      3 | 5366 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|      - | 5367 | `		/* Forbidden */` |
|    ! 0 | 5368 | `		return -1;` |
|      3 | 5369 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|      3 | 5370 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|      - | 5371 | `		int rc;` |
|      - | 5372 | `		/* Call the vm output consumer */` |
|      3 | 5373 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|      3 | 5374 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 5375 | `			return -1;` |
|      - | 5376 | `		}` |
|      3 | 5377 | `		return nWrite;` |
|      - | 5378 | `	}` |
|      - | 5379 | `#ifdef __WINNT__` |
|      - | 5380 | `	{` |
|      - | 5381 | `		DWORD nWr;` |
|      - | 5382 | `		BOOL rc;` |
|    ! 0 | 5383 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|    ! 0 | 5384 | `		if( !rc ){` |
|      - | 5385 | `			/* IO error */` |
|    ! 0 | 5386 | `			return -1;` |
|      - | 5387 | `		}` |
|    ! 0 | 5388 | `		return (ph7_int64)nWr;` |
|      - | 5389 | `	}` |
|      - | 5390 | `#elif defined(__UNIXES__)` |
|      - | 5391 | `	{` |
|      - | 5392 | `		ssize_t nWr;` |
|      - | 5393 | `		int fd;` |
|    ! 0 | 5394 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    ! 0 | 5395 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|    ! 0 | 5396 | `		if( nWr < 1 ){` |
|    ! 0 | 5397 | `			return -1;` |
|      - | 5398 | `		}` |
|    ! 0 | 5399 | `		return (ph7_int64)nWr;` |
|      - | 5400 | `	}` |
|      - | 5401 | `#else` |
|      - | 5402 | `	return -1;` |
|      - | 5403 | `#endif` |
|      2 | 5404 | `}` |
|      - | 5405 | `/* void (*xClose)(void *) */` |
|      2 | 5406 | `static void PHPStreamData_Close(void *pHandle)` |
|      1 | 5407 | `{` |
|      3 | 5408 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|      - | 5409 | `	ph7_vm *pVm;` |
|      3 | 5410 | `	if( pData == 0 ){` |
|    ! 0 | 5411 | `		return;` |
|      - | 5412 | `	}` |
|      3 | 5413 | `	pVm = pData->pVm;` |
|      - | 5414 | `	/* Free the instance */` |
|      3 | 5415 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|      2 | 5416 | `}` |
|      - | 5417 | `/*` |
|      - | 5418 | ` * Pipe stream implementation for popen/pclose.` |
|      - | 5419 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|      - | 5420 | ` * PHP-compatible process I/O functionality.` |
|      - | 5421 | ` */` |
|      - | 5422 | `typedef struct pipe_private pipe_private;` |
|      - | 5423 | `struct pipe_private` |
|      - | 5424 | `{` |
|      - | 5425 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|      - | 5426 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|      - | 5427 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|      - | 5428 | `#ifdef __WINNT__` |
|      - | 5429 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|      - | 5430 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|      - | 5431 | `#endif` |
|      - | 5432 | `};` |
|      - | 5433 |  |
|      - | 5434 | `#ifdef __WINNT__` |
|      - | 5435 | `#include <Windows.h>` |
|      - | 5436 | `#include <stdio.h>` |
|      - | 5437 | `#include <io.h>` |
|      - | 5438 | `#include <fcntl.h>` |
|      - | 5439 | `/*` |
|      - | 5440 | ` * Custom Windows popen implementation using CreateProcess.` |
|      - | 5441 | ` * This allows us to properly wait for process completion.` |
|      - | 5442 | ` */` |
|      - | 5443 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|      5 | 5444 | `{` |
|      5 | 5445 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|      5 | 5446 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|      5 | 5447 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|      - | 5448 | `	SECURITY_ATTRIBUTES sa;` |
|      - | 5449 | `	STARTUPINFOW si;` |
|      - | 5450 | `	PROCESS_INFORMATION pi;` |
|      5 | 5451 | `	WCHAR *zWideCmd = NULL;` |
|      5 | 5452 | `	FILE *pFile = NULL;` |
|      - | 5453 | `	int fd;` |
|      5 | 5454 | `	BOOL bRead = (zMode[0] == 'r');` |
|      - | 5455 |  |
|      - | 5456 | `	/* Set up security attributes for pipe inheritance */` |
|      5 | 5457 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|      5 | 5458 | `	sa.bInheritHandle = TRUE;` |
|      5 | 5459 | `	sa.lpSecurityDescriptor = NULL;` |
|      - | 5460 |  |
|      - | 5461 | `	/* Create pipes for child process I/O */` |
|      5 | 5462 | `	if( bRead ){` |
|      - | 5463 | `		/* Reading from child's stdout */` |
|      5 | 5464 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|    ! 0 | 5465 | `			return NULL;` |
|      - | 5466 | `		}` |
|      - | 5467 | `		/* Ensure read handle is not inherited */` |
|      5 | 5468 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|      5 | 5469 | `		hReadPipe = hChildStdoutRd;` |
|      5 | 5470 | `		*phPipe = hChildStdoutRd;` |
|      5 | 5471 | `	}else{` |
|      - | 5472 | `		/* Writing to child's stdin */` |
|    ! 0 | 5473 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|    ! 0 | 5474 | `			return NULL;` |
|      - | 5475 | `		}` |
|      - | 5476 | `		/* Ensure write handle is not inherited */` |
|    ! 0 | 5477 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|    ! 0 | 5478 | `		hWritePipe = hChildStdinWr;` |
|    ! 0 | 5479 | `		*phPipe = hChildStdinWr;` |
|      - | 5480 | `	}` |
|      - | 5481 |  |
|      - | 5482 | `	/* Convert command to wide string */` |
|      - | 5483 | `	{` |
|      5 | 5484 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|      5 | 5485 | `		if( nLen <= 0 ){` |
|    ! 0 | 5486 | `			goto cleanup_pipes;` |
|      - | 5487 | `		}` |
|      5 | 5488 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|      5 | 5489 | `		if( !zWideCmd ){` |
|    ! 0 | 5490 | `			goto cleanup_pipes;` |
|      - | 5491 | `		}` |
|      5 | 5492 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|      - | 5493 | `	}` |
|      - | 5494 |  |
|      - | 5495 | `	/* Set up process startup info */` |
|      5 | 5496 | `	ZeroMemory(&si, sizeof(si));` |
|      5 | 5497 | `	si.cb = sizeof(si);` |
|      5 | 5498 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|      5 | 5499 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|      5 | 5500 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|      5 | 5501 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|      5 | 5502 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|      - | 5503 |  |
|      5 | 5504 | `	ZeroMemory(&pi, sizeof(pi));` |
|      - | 5505 |  |
|      - | 5506 | `	/* Create the child process */` |
|      5 | 5507 | `	if( !CreateProcessW(` |
|      - | 5508 | `		NULL,           /* Application name */` |
|      - | 5509 | `		zWideCmd,       /* Command line */` |
|      - | 5510 | `		NULL,           /* Process security attributes */` |
|      - | 5511 | `		NULL,           /* Thread security attributes */` |
|      - | 5512 | `		TRUE,           /* Inherit handles */` |
|      - | 5513 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|      - | 5514 | `		NULL,           /* Environment */` |
|      - | 5515 | `		NULL,           /* Current directory */` |
|      - | 5516 | `		&si,            /* Startup info */` |
|      - | 5517 | `		&pi             /* Process info */` |
|      - | 5518 | `	)){` |
|    ! 0 | 5519 | `		goto cleanup_all;` |
|      - | 5520 | `	}` |
|      - | 5521 |  |
|      - | 5522 | `	/* Close handles we don't need in parent */` |
|      5 | 5523 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|      5 | 5524 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|      - | 5525 |  |
|      - | 5526 | `	/* Close thread handle (we only need process handle) */` |
|      5 | 5527 | `	CloseHandle(pi.hThread);` |
|      - | 5528 |  |
|      - | 5529 | `	/* Store process handle for later waiting */` |
|      5 | 5530 | `	*phProcess = pi.hProcess;` |
|      - | 5531 |  |
|      - | 5532 | `	/* Convert OS handle to C file descriptor, then to FILE* */` |
|      5 | 5533 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|      - | 5534 | `	                     bRead ? _O_RDONLY \| _O_TEXT : _O_WRONLY \| _O_TEXT);` |
|      5 | 5535 | `	if( fd == -1 ){` |
|    ! 0 | 5536 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 5537 | `		*phProcess = NULL;` |
|    ! 0 | 5538 | `		goto cleanup_all;` |
|      - | 5539 | `	}` |
|      - | 5540 |  |
|      5 | 5541 | `	pFile = _fdopen(fd, zMode);` |
|      5 | 5542 | `	if( !pFile ){` |
|    ! 0 | 5543 | `		_close(fd); /* This will also close the underlying handle */` |
|    ! 0 | 5544 | `		CloseHandle(pi.hProcess);` |
|    ! 0 | 5545 | `		*phProcess = NULL;` |
|    ! 0 | 5546 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    ! 0 | 5547 | `		return NULL;` |
|      - | 5548 | `	}` |
|      - | 5549 |  |
|      5 | 5550 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      5 | 5551 | `	return pFile;` |
|      - | 5552 |  |
|      - | 5553 | `cleanup_all:` |
|    ! 0 | 5554 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|      - | 5555 | `cleanup_pipes:` |
|    ! 0 | 5556 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|    ! 0 | 5557 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|    ! 0 | 5558 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|    ! 0 | 5559 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|    ! 0 | 5560 | `	return NULL;` |
|      5 | 5561 | `}` |
|      - | 5562 |  |
|      - | 5563 | `/*` |
|      - | 5564 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|      - | 5565 | ` */` |
|      - | 5566 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|      5 | 5567 | `{` |
|      5 | 5568 | `	DWORD dwExitCode = 0;` |
|      - | 5569 | `	int status;` |
|      - | 5570 |  |
|      - | 5571 | `	/* Close the FILE* (this closes the pipe) */` |
|      5 | 5572 | `	fclose(pFile);` |
|      - | 5573 |  |
|      5 | 5574 | `	if( hProcess ){` |
|      - | 5575 | `		/* Wait for the process to complete */` |
|      5 | 5576 | `		WaitForSingleObject(hProcess, INFINITE);` |
|      - | 5577 |  |
|      5 | 5578 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|      5 | 5579 | `			status = (int)dwExitCode;` |
|      5 | 5580 | `		}else{` |
|    ! 0 | 5581 | `			status = -1;` |
|      - | 5582 | `		}` |
|      - | 5583 |  |
|      - | 5584 | `		/* Close process handle */` |
|      5 | 5585 | `		CloseHandle(hProcess);` |
|      5 | 5586 | `	}else{` |
|    ! 0 | 5587 | `		status = -1;` |
|      - | 5588 | `	}` |
|      - | 5589 |  |
|      5 | 5590 | `	return status;` |
|      5 | 5591 | `}` |
|      - | 5592 | `#endif /* __WINNT__ */` |
|      - | 5593 | `/*` |
|      - | 5594 | ` * Open a pipe to a process.` |
|      - | 5595 | ` * This is called internally by popen(), not through the stream device interface.` |
|      - | 5596 | ` */` |
|   3876 | 5597 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|      5 | 5598 | `{` |
|      - | 5599 | `	pipe_private *pPipe;` |
|      - | 5600 | `	FILE *pFile;` |
|   3881 | 5601 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|    ! 0 | 5602 | `		return 0;` |
|      - | 5603 | `	}` |
|      - | 5604 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|   3881 | 5605 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|    ! 0 | 5606 | `		return 0;` |
|      - | 5607 | `	}` |
|      - | 5608 | `	/* Open the pipe using system popen */` |
|      - | 5609 | `#ifdef __WINNT__` |
|      - | 5610 | `	{` |
|      - | 5611 | `		/* Build cmd.exe command wrapper */` |
|      5 | 5612 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|      5 | 5613 | `		const char *zShellSuffix = "\"";` |
|      5 | 5614 | `		size_t nPrefix = strlen(zShellPrefix);` |
|      5 | 5615 | `		size_t nSuffix = strlen(zShellSuffix);` |
|      5 | 5616 | `		size_t nCmd = strlen(zCommand);` |
|      5 | 5617 | `		size_t nQuotes = 0;` |
|      5 | 5618 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 5619 | `			if (zCommand[i] == '"') nQuotes++;` |
|      5 | 5620 | `		}` |
|      5 | 5621 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|      5 | 5622 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|      5 | 5623 | `		if (zCmdEsc == NULL) {` |
|    ! 0 | 5624 | `			return 0;` |
|      - | 5625 | `		}` |
|      - | 5626 | `		/* Escape quotes in command */` |
|      5 | 5627 | `		size_t j = 0;` |
|      5 | 5628 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|      5 | 5629 | `			char ch = zCommand[i];` |
|      5 | 5630 | `			if (ch == '"') {` |
|      4 | 5631 | `				zCmdEsc[j++] = '^';` |
|      4 | 5632 | `				zCmdEsc[j++] = '"';` |
|      4 | 5633 | `			} else {` |
|      5 | 5634 | `				zCmdEsc[j++] = ch;` |
|      - | 5635 | `			}` |
|      5 | 5636 | `		}` |
|      5 | 5637 | `		zCmdEsc[j] = '\0';` |
|      5 | 5638 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|      5 | 5639 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|      5 | 5640 | `		if (zWinCmd == NULL) {` |
|    ! 0 | 5641 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 5642 | `			return 0;` |
|      - | 5643 | `		}` |
|      5 | 5644 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|      5 | 5645 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|      5 | 5646 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|      5 | 5647 | `		zWinCmd[nTotal - 1] = '\0';` |
|      - | 5648 | `		/* Allocate pipe structure early so we can store handles */` |
|      5 | 5649 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|      5 | 5650 | `		if( pPipe == 0 ){` |
|    ! 0 | 5651 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    ! 0 | 5652 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|    ! 0 | 5653 | `			return 0;` |
|      - | 5654 | `		}` |
|      - | 5655 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|      5 | 5656 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|      5 | 5657 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|      5 | 5658 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|      5 | 5659 | `		if( pFile == 0 ){` |
|    ! 0 | 5660 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|    ! 0 | 5661 | `			return 0;` |
|      - | 5662 | `		}` |
|      - | 5663 | `		/* Initialize remaining fields */` |
|      5 | 5664 | `		pPipe->pFile = pFile;` |
|      5 | 5665 | `		pPipe->pVm = pVm;` |
|      5 | 5666 | `		pPipe->iMode = zMode[0];` |
|      - | 5667 | `	}` |
|      - | 5668 | `#elif defined(__UNIXES__) /* Unix */` |
|   3876 | 5669 | `	pFile = popen(zCommand, zMode);` |
|   3876 | 5670 | `	if( pFile == 0 ){` |
|    ! 0 | 5671 | `		return 0;` |
|      - | 5672 | `	}` |
|      - | 5673 | `	/* Allocate pipe private structure */` |
|   3876 | 5674 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|   3876 | 5675 | `	if( pPipe == 0 ){` |
|      - | 5676 | `		/* Out of memory, close the pipe */` |
|    ! 0 | 5677 | `		pclose(pFile);` |
|    ! 0 | 5678 | `		return 0;` |
|      - | 5679 | `	}` |
|      - | 5680 | `	/* Initialize the structure */` |
|   3876 | 5681 | `	pPipe->pFile = pFile;` |
|   3876 | 5682 | `	pPipe->pVm = pVm;` |
|   3876 | 5683 | `	pPipe->iMode = zMode[0];` |
|      - | 5684 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 5685 | `	(void)pFile;` |
|      - | 5686 | `	return 0;` |
|      - | 5687 | `#endif` |
|   3881 | 5688 | `	return pPipe;` |
|   1943 | 5689 | `}` |
|      - | 5690 | `/*` |
|      - | 5691 | ` * Close a pipe and return the exit status of the process.` |
|      - | 5692 | ` * Returns the exit status, or -1 on error.` |
|      - | 5693 | ` */` |
|   3854 | 5694 | `static int PipeClose(pipe_private *pPipe)` |
|      5 | 5695 | `{` |
|      - | 5696 | `	int status;` |
|      - | 5697 | `	ph7_vm *pVm;` |
|   3859 | 5698 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 5699 | `		return -1;` |
|      - | 5700 | `	}` |
|   3859 | 5701 | `	pVm = pPipe->pVm;` |
|      - | 5702 | `	/* Close the pipe and get exit status */` |
|      - | 5703 | `#ifdef __WINNT__` |
|      - | 5704 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|      5 | 5705 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|      - | 5706 | `#elif defined(__UNIXES__)` |
|   3854 | 5707 | `	status = pclose(pPipe->pFile);` |
|      - | 5708 | `	/* On Unix, pclose returns the status from waitpid, need to extract exit code */` |
|   3854 | 5709 | `	if( status != -1 ){` |
|   3854 | 5710 | `		if( WIFEXITED(status) ){` |
|   3854 | 5711 | `			status = WEXITSTATUS(status);` |
|   1927 | 5712 | `		}else if( WIFSIGNALED(status) ){` |
|      - | 5713 | `			/* Process was killed by a signal - use shell convention: 128 + signal number */` |
|    ! 0 | 5714 | `			status = 128 + WTERMSIG(status);` |
|    ! 0 | 5715 | `		}else{` |
|      - | 5716 | `			/* Unknown termination reason */` |
|    ! 0 | 5717 | `			status = -1;` |
|      - | 5718 | `		}` |
|   1927 | 5719 | `	}` |
|      - | 5720 | `#else /* OS_OTHER: no process pipes on this platform */` |
|      - | 5721 | `	status = -1;` |
|      - | 5722 | `#endif` |
|      - | 5723 | `	/* Free the structure */` |
|   3859 | 5724 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|   3859 | 5725 | `	return status;` |
|   1932 | 5726 | `}` |
|      - | 5727 | `/*` |
|      - | 5728 | ` * Pipe stream xClose implementation.` |
|      - | 5729 | ` * Note: This is called by fclose(), not pclose().` |
|      - | 5730 | ` * It closes the pipe but does not return the exit status.` |
|      - | 5731 | ` */` |
|     94 | 5732 | `static void PipeStream_Close(void *pHandle)` |
|      4 | 5733 | `{` |
|     98 | 5734 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|     98 | 5735 | `	if( pPipe ){` |
|     98 | 5736 | `		PipeClose(pPipe);` |
|     47 | 5737 | `	}` |
|     98 | 5738 | `}` |
|      - | 5739 | `/*` |
|      - | 5740 | ` * Pipe stream xRead implementation.` |
|      - | 5741 | ` */` |
|   5656 | 5742 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|      4 | 5743 | `{` |
|   5660 | 5744 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 5745 | `	size_t nRead;` |
|   5660 | 5746 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 5747 | `		return -1;` |
|      - | 5748 | `	}` |
|   5660 | 5749 | `	if( pPipe->iMode != 'r' ){` |
|      - | 5750 | `		/* Cannot read from a write-only pipe */` |
|    ! 0 | 5751 | `		return -1;` |
|      - | 5752 | `	}` |
|   5660 | 5753 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
|   5660 | 5754 | `	if( nRead == 0 ){` |
|   3878 | 5755 | `		if( feof(pPipe->pFile) ){` |
|   3878 | 5756 | `			return 0; /* EOF */` |
|      - | 5757 | `		}` |
|    ! 0 | 5758 | `		return -1; /* Error */` |
|      - | 5759 | `	}` |
|   1786 | 5760 | `	return (ph7_int64)nRead;` |
|   2832 | 5761 | `}` |
|      - | 5762 | `/*` |
|      - | 5763 | ` * Pipe stream xWrite implementation.` |
|      - | 5764 | ` */` |
|      2 | 5765 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|    ! 0 | 5766 | `{` |
|      2 | 5767 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|      - | 5768 | `	size_t nWritten;` |
|      2 | 5769 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    ! 0 | 5770 | `		return -1;` |
|      - | 5771 | `	}` |
|      2 | 5772 | `	if( pPipe->iMode != 'w' ){` |
|      - | 5773 | `		/* Cannot write to a read-only pipe */` |
|    ! 0 | 5774 | `		return -1;` |
|      - | 5775 | `	}` |
|      2 | 5776 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|      2 | 5777 | `	if( nWritten == 0 && nWrite > 0 ){` |
|    ! 0 | 5778 | `		return -1; /* Error */` |
|      - | 5779 | `	}` |
|      2 | 5780 | `	return (ph7_int64)nWritten;` |
|      1 | 5781 | `}` |
|      - | 5782 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|      - | 5783 | `static const ph7_io_stream sPipe_Stream = {` |
|      - | 5784 | `	"pipe",` |
|      - | 5785 | `	PH7_IO_STREAM_VERSION,` |
|      - | 5786 |  |
|      - | 5787 |  |
|      - | 5788 | `	PipeStream_Close,  /* xClose */` |
|      - | 5789 |  |
|      - | 5790 | `	PipeStream_Read,   /* xRead */` |
|      - | 5791 |  |
|      - | 5792 | `	PipeStream_Write,  /* xWrite */` |
|      - | 5793 |  |
|      - | 5794 |  |
|      - | 5795 |  |
|      - | 5796 |  |
|      - | 5797 |  |
|      - | 5798 |  |
|      - | 5799 |  |
|      - | 5800 | `};` |
|      - | 5801 | `/*` |
|      - | 5802 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|      - | 5803 | ` * FALSE otherwise.` |
|      - | 5804 | ` */` |
|   3760 | 5805 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|      5 | 5806 | `{` |
|   3765 | 5807 | `	return pStream == &sPipe_Stream;` |
|      5 | 5808 | `}` |
|      - | 5809 | `/*` |
|      - | 5810 | ` * resource popen(string $command, string $mode)` |
|      - | 5811 | ` *  Opens process file pointer.` |
|      - | 5812 | ` * Parameters` |
|      - | 5813 | ` *  $command` |
|      - | 5814 | ` *   The command to execute. Passed to the system shell.` |
|      - | 5815 | ` *  $mode` |
|      - | 5816 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|      - | 5817 | ` *   'r' - Open for reading (read from the command's stdout).` |
|      - | 5818 | ` *   'w' - Open for writing (write to the command's stdin).` |
|      - | 5819 | ` * Return` |
|      - | 5820 | ` *  Returns a file pointer on success, or FALSE on error.` |
|      - | 5821 | ` */` |
|   3876 | 5822 | `static int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5823 | `{` |
|      - | 5824 | `	const char *zCommand, *zMode;` |
|      - | 5825 | `	pipe_private *pPipe;` |
|      - | 5826 | `	io_private *pDev;` |
|      - | 5827 | `	int nCmdLen, nModeLen;` |
|   3881 | 5828 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 5829 | `		/* Missing/Invalid arguments, return FALSE */` |
|    ! 0 | 5830 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a command string and mode");` |
|    ! 0 | 5831 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 5832 | `		return PH7_OK;` |
|      - | 5833 | `	}` |
|      - | 5834 | `	/* Extract the command and mode */` |
|   3881 | 5835 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|   3881 | 5836 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|   3881 | 5837 | `	if( nCmdLen < 1 ){` |
|    ! 0 | 5838 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty command");` |
|    ! 0 | 5839 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 5840 | `		return PH7_OK;` |
|      - | 5841 | `	}` |
|   3881 | 5842 | `	if( nModeLen < 1 \|\| (zMode[0] != 'r' && zMode[0] != 'w') ){` |
|    ! 0 | 5843 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Invalid mode, expected 'r' or 'w'");` |
|    ! 0 | 5844 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 5845 | `		return PH7_OK;` |
|      - | 5846 | `	}` |
|      - | 5847 | `	/* Open the pipe */` |
|   3881 | 5848 | `	pPipe = PipeOpen(pCtx->pVm, zCommand, zMode);` |
|   3881 | 5849 | `	if( pPipe == 0 ){` |
|      - | 5850 | `		/* Failed to open pipe */` |
|    ! 0 | 5851 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 5852 | `		return PH7_OK;` |
|      - | 5853 | `	}` |
|      - | 5854 | `	/* Allocate an io_private instance to wrap the pipe */` |
|   3881 | 5855 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|   3881 | 5856 | `	if( pDev == 0 ){` |
|    ! 0 | 5857 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|    ! 0 | 5858 | `		PipeClose(pPipe);` |
|    ! 0 | 5859 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 5860 | `		return PH7_OK;` |
|      - | 5861 | `	}` |
|      - | 5862 | `	/* Initialize the io_private structure */` |
|   3881 | 5863 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|   3881 | 5864 | `	pDev->pHandle = pPipe;` |
|      - | 5865 | `	/* Return the io_private instance as a resource */` |
|   3881 | 5866 | `	ph7_result_resource(pCtx, pDev);` |
|   3881 | 5867 | `	return PH7_OK;` |
|   1943 | 5868 | `}` |
|      - | 5869 | `/*` |
|      - | 5870 | ` * int pclose(resource $handle)` |
|      - | 5871 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|      - | 5872 | ` * Parameters` |
|      - | 5873 | ` *  $handle` |
|      - | 5874 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|      - | 5875 | ` * Return` |
|      - | 5876 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|      - | 5877 | ` */` |
|   3760 | 5878 | `static int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5879 | `{` |
|      - | 5880 | `	const ph7_io_stream *pStream;` |
|      - | 5881 | `	pipe_private *pPipe;` |
|      - | 5882 | `	io_private *pDev;` |
|      - | 5883 | `	int status;` |
|   3765 | 5884 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 5885 | `		/* Missing/Invalid arguments, return -1 */` |
|    ! 0 | 5886 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 5887 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 5888 | `		return PH7_OK;` |
|      - | 5889 | `	}` |
|      - | 5890 | `	/* Extract our private data */` |
|   3765 | 5891 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 5892 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   3765 | 5893 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 5894 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|    ! 0 | 5895 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 5896 | `		return PH7_OK;` |
|      - | 5897 | `	}` |
|      - | 5898 | `	/* Point to the target IO stream device */` |
|   3765 | 5899 | `	pStream = pDev->pStream;` |
|   3765 | 5900 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|    ! 0 | 5901 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|    ! 0 | 5902 | `		ph7_result_int(pCtx, -1);` |
|    ! 0 | 5903 | `		return PH7_OK;` |
|      - | 5904 | `	}` |
|      - | 5905 | `	/* Get the pipe handle */` |
|   3765 | 5906 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|      - | 5907 | `	/* Close the pipe and get exit status */` |
|   3765 | 5908 | `	status = PipeClose(pPipe);` |
|      - | 5909 | `	/* Release the IO private structure */` |
|   3765 | 5910 | `	ReleaseIOPrivate(pCtx, pDev);` |
|      - | 5911 | `	/* Invalidate the resource handle */` |
|   3765 | 5912 | `	ph7_value_release(apArg[0]);` |
|      - | 5913 | `	/* Return the exit status */` |
|   3765 | 5914 | `	ph7_result_int(pCtx, status);` |
|   3765 | 5915 | `	return PH7_OK;` |
|   1885 | 5916 | `}` |
|      - | 5917 | `/* Export the php:// stream */` |
|      - | 5918 | `static const ph7_io_stream sPHP_Stream = {` |
|      - | 5919 | `	"php",` |
|      - | 5920 | `	PH7_IO_STREAM_VERSION,` |
|      - | 5921 | `	PHPStreamData_Open,  /* xOpen */` |
|      - | 5922 |  |
|      - | 5923 | `	PHPStreamData_Close, /* xClose */` |
|      - | 5924 |  |
|      - | 5925 | `	PHPStreamData_Read,  /* xRead */` |
|      - | 5926 |  |
|      - | 5927 | `	PHPStreamData_Write, /* xWrite */` |
|      - | 5928 |  |
|      - | 5929 |  |
|      - | 5930 |  |
|      - | 5931 |  |
|      - | 5932 |  |
|      - | 5933 |  |
|      - | 5934 |  |
|      - | 5935 | `};` |
|      - | 5936 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 5937 | `/*` |
|      - | 5938 | ` * Return TRUE if we are dealing with the php:// stream.` |
|      - | 5939 | ` * FALSE otherwise.` |
|      - | 5940 | ` */` |
|     64 | 5941 | `static int is_php_stream(const ph7_io_stream *pStream)` |
|      2 | 5942 | `{` |
|      - | 5943 | `#ifndef PH7_DISABLE_DISK_IO` |
|     66 | 5944 | `	return pStream == &sPHP_Stream;` |
|      - | 5945 | `#else` |
|      - | 5946 | `	SXUNUSED(pStream); /* cc warning */` |
|      - | 5947 | `	return 0;` |
|      - | 5948 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      2 | 5949 | `}` |
|      - | 5950 |  |
|      - | 5951 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 5952 | `/*` |
|      - | 5953 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 5954 | ` * [i.e: file://,php://].` |
|      - | 5955 | ` * Note:` |
|      - | 5956 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 5957 | ` *  defined then this function is a no-op.` |
|      - | 5958 | ` */` |
|   3410 | 5959 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 5960 | `{` |
|      - | 5961 | `	/*` |
|      - | 5962 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 5963 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 5964 | `	 */` |
|      - | 5965 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 5966 | `	/* VFS: disk I/O related functions */` |
|      - | 5967 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 5968 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 5969 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 5970 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 5971 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 5972 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 5973 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 5974 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 5975 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 5976 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 5977 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 5978 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 5979 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 5980 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 5981 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 5982 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 5983 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 5984 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 5985 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 5986 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 5987 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 5988 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 5989 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 5990 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 5991 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 5992 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 5993 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 5994 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 5995 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 5996 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 5997 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 5998 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 5999 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 6000 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 6001 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 6002 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 6003 | `		{"link",        PH7_vfs_link     },` |
|      - | 6004 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 6005 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 6006 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 6007 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 6008 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 6009 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 6010 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 6011 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 6012 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 6013 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 6014 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 6015 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 6016 | `	};` |
|      - | 6017 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 6018 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 6019 | `	 */` |
|      - | 6020 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 6021 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 6022 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 6023 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 6024 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 6025 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 6026 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 6027 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 6028 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 6029 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 6030 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 6031 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 6032 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 6033 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 6034 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 6035 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 6036 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 6037 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 6038 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 6039 | `		{"file",      PH7_builtin_file   },` |
|      - | 6040 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 6041 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 6042 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 6043 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 6044 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 6045 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 6046 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 6047 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 6048 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 6049 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 6050 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 6051 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 6052 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 6053 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 6054 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 6055 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 6056 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 6057 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 6058 | `	};` |
|   3415 | 6059 | `	const ph7_io_stream *pFileStream = 0;` |
|   3415 | 6060 | `	sxu32 n = 0;` |
|      - | 6061 | `	/* Register disk-related functions */` |
| 167095 | 6062 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 163685 | 6063 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
|  81845 | 6064 | `	}` |
| 122765 | 6065 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 119355 | 6066 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
|  59680 | 6067 | `	}` |
|      - | 6068 | `#else` |
|      - | 6069 | `	SXUNUSED(pVm);` |
|      - | 6070 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 6071 |  |
|      - | 6072 | `	/*` |
|      - | 6073 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 6074 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 6075 | `	 */` |
|      - | 6076 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 6077 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 6078 | `		/* Path processing */` |
|      - | 6079 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 6080 | `		{"basename",    PH7_builtin_basename },` |
|      - | 6081 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 6082 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 6083 | `		{"fnmatch",     PH7_builtin_fnmatch  },` |
|      - | 6084 | `		/* ZIP processing */` |
|      - | 6085 | `		{"zip_open",    PH7_builtin_zip_open },` |
|      - | 6086 | `		{"zip_close",   PH7_builtin_zip_close},` |
|      - | 6087 | `		{"zip_read",    PH7_builtin_zip_read },` |
|      - | 6088 | `		{"zip_entry_open", PH7_builtin_zip_entry_open },` |
|      - | 6089 | `		{"zip_entry_close",PH7_builtin_zip_entry_close},` |
|      - | 6090 | `		{"zip_entry_name", PH7_builtin_zip_entry_name },` |
|      - | 6091 | `		{"zip_entry_filesize",      PH7_builtin_zip_entry_filesize       },` |
|      - | 6092 | `		{"zip_entry_compressedsize",PH7_builtin_zip_entry_compressedsize },` |
|      - | 6093 | `		{"zip_entry_read", PH7_builtin_zip_entry_read },` |
|      - | 6094 | `		{"zip_entry_reset_read_cursor",PH7_builtin_zip_entry_reset_read_cursor},` |
|      - | 6095 | `		{"zip_entry_compressionmethod",PH7_builtin_zip_entry_compressionmethod}` |
|      - | 6096 | `	};` |
|  57975 | 6097 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  54565 | 6098 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|  27285 | 6099 | `	}` |
|      - | 6100 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 6101 |  |
|      - | 6102 | `	/* Install streams if disk I/O is enabled */` |
|      - | 6103 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 6104 | `#ifdef __WINNT__` |
|      5 | 6105 | `	pFileStream = &sWinFileStream;` |
|      - | 6106 | `#elif defined(__UNIXES__)` |
|   3410 | 6107 | `	pFileStream = &sUnixFileStream;` |
|      - | 6108 | `#endif` |
|      - | 6109 | `	/* Install the php:// stream */` |
|   3415 | 6110 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   3415 | 6111 | `	if( pFileStream ){` |
|      - | 6112 | `		/* Install the file:// stream */` |
|   3415 | 6113 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   1705 | 6114 | `	}` |
|      - | 6115 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 6116 |  |
|   3415 | 6117 | `	return SXRET_OK;` |
|      5 | 6118 | `}` |
|      - | 6119 | `/*` |
|      - | 6120 | ` * Export the STDIN handle.` |
|      - | 6121 | ` */` |
|      2 | 6122 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|      1 | 6123 | `{` |
|      - | 6124 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 6125 | `	if( pVm->pStdin == 0  ){` |
|      - | 6126 | `		io_private *pIn;` |
|      - | 6127 | `		/* Allocate an IO private instance */` |
|      3 | 6128 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 6129 | `		if( pIn == 0 ){` |
|    ! 0 | 6130 | `			return 0;` |
|      - | 6131 | `		}` |
|      3 | 6132 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|      - | 6133 | `		/* Initialize the handle */` |
|      3 | 6134 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|      - | 6135 | `		/* Install the STDIN stream */` |
|      3 | 6136 | `		pVm->pStdin = pIn;` |
|      3 | 6137 | `		return pIn;` |
|    ! 0 | 6138 | `	}else{` |
|      - | 6139 | `		/* NULL or STDIN */` |
|    ! 0 | 6140 | `		return pVm->pStdin;` |
|      - | 6141 | `	}` |
|      - | 6142 | `#else` |
|      - | 6143 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 6144 | `	return 0;` |
|      - | 6145 | `#endif` |
|      2 | 6146 | `}` |
|      - | 6147 | `/*` |
|      - | 6148 | ` * Export the STDOUT handle.` |
|      - | 6149 | ` */` |
|      2 | 6150 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|      1 | 6151 | `{` |
|      - | 6152 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 6153 | `	if( pVm->pStdout == 0  ){` |
|      - | 6154 | `		io_private *pOut;` |
|      - | 6155 | `		/* Allocate an IO private instance */` |
|      3 | 6156 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 6157 | `		if( pOut == 0 ){` |
|    ! 0 | 6158 | `			return 0;` |
|      - | 6159 | `		}` |
|      3 | 6160 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|      - | 6161 | `		/* Initialize the handle */` |
|      3 | 6162 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|      - | 6163 | `		/* Install the STDOUT stream */` |
|      3 | 6164 | `		pVm->pStdout = pOut;` |
|      3 | 6165 | `		return pOut;` |
|    ! 0 | 6166 | `	}else{` |
|      - | 6167 | `		/* NULL or STDOUT */` |
|    ! 0 | 6168 | `		return pVm->pStdout;` |
|      - | 6169 | `	}` |
|      - | 6170 | `#else` |
|      - | 6171 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 6172 | `	return 0;` |
|      - | 6173 | `#endif` |
|      2 | 6174 | `}` |
|      - | 6175 | `/*` |
|      - | 6176 | ` * Export the STDERR handle.` |
|      - | 6177 | ` */` |
|      2 | 6178 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|      1 | 6179 | `{` |
|      - | 6180 | `#ifndef PH7_DISABLE_DISK_IO` |
|      3 | 6181 | `	if( pVm->pStderr == 0  ){` |
|      - | 6182 | `		io_private *pErr;` |
|      - | 6183 | `		/* Allocate an IO private instance */` |
|      3 | 6184 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|      3 | 6185 | `		if( pErr == 0 ){` |
|    ! 0 | 6186 | `			return 0;` |
|      - | 6187 | `		}` |
|      3 | 6188 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|      - | 6189 | `		/* Initialize the handle */` |
|      3 | 6190 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|      - | 6191 | `		/* Install the STDERR stream */` |
|      3 | 6192 | `		pVm->pStderr = pErr;` |
|      3 | 6193 | `		return pErr;` |
|    ! 0 | 6194 | `	}else{` |
|      - | 6195 | `		/* NULL or STDERR */` |
|    ! 0 | 6196 | `		return pVm->pStderr;` |
|      - | 6197 | `	}` |
|      - | 6198 | `#else` |
|      - | 6199 | `	SXUNUSED(pVm); /* cc warning */` |
|      - | 6200 | `	return 0;` |
|      - | 6201 | `#endif` |
|      2 | 6202 | `}` |
|      - | 6203 |  |
