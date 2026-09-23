# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 888/1283 lines (69.21%)

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
|  15696 |   25 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)` |
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
|  15696 |   37 | `	const char *zRoot = "/";` |
|      - |   38 | `#endif` |
|  15701 |   39 | `	c = d = '/';` |
|      - |   40 | `#ifdef __WINNT__` |
|      5 |   41 | `	d = '\\';` |
|      - |   42 | `#endif` |
|      - |   43 | `#define DIR_IS_SEP(x) ( (int)(x) == c \|\| (int)(x) == d )` |
|  15701 |   44 | `	if( nByte < 1 ){` |
|      - |   45 | `		/* php returns the empty string for the empty path */` |
|      5 |   46 | `		*pLen = 0;` |
|      5 |   47 | `		return "";` |
|      - |   48 | `	}` |
|  15697 |   49 | `	iEnd = nByte;` |
|  23571 |   50 | `	while( iEnd > 0 && DIR_IS_SEP(zPath[iEnd - 1]) ){` |
|     29 |   51 | `		iEnd--;` |
|      1 |   52 | `	}` |
|  15697 |   53 | `	if( iEnd == 0 ){` |
|      - |   54 | `		/* The path is nothing but separators: the root is its own parent */` |
|     17 |   55 | `		*pLen = (int)sizeof(char);` |
|     17 |   56 | `		return zRoot;` |
|      - |   57 | `	}` |
|      - |   58 | `	/* Walk back to the separator that ends the parent directory */` |
|  15681 |   59 | `	i = iEnd;` |
| 422679 |   60 | `	while( i > 0 && !DIR_IS_SEP(zPath[i - 1]) ){` |
| 407003 |   61 | `		i--;` |
|      5 |   62 | `	}` |
|  15681 |   63 | `	if( i == 0 ){` |
|      - |   64 | `		/* No separator at all,return "." as the current directory */` |
|     70 |   65 | `		*pLen = (int)sizeof(char);` |
|     70 |   66 | `		return ".";` |
|      - |   67 | `	}` |
|      - |   68 | `	/* Drop the separator, plus any that repeat before it */` |
|  39015 |   69 | `	while( i > 1 && DIR_IS_SEP(zPath[i - 1]) ){` |
|  15603 |   70 | `		i--;` |
|      5 |   71 | `	}` |
|  15613 |   72 | `	if( i == 1 && DIR_IS_SEP(zPath[0]) ){` |
|     13 |   73 | `		*pLen = (int)sizeof(char);` |
|     13 |   74 | `		return zRoot;` |
|      - |   75 | `	}` |
|  15601 |   76 | `	*pLen = i;` |
|  15601 |   77 | `	return zPath;` |
|      - |   78 | `#undef DIR_IS_SEP` |
|   7853 |   79 | `}` |
|      - |   80 | `/*` |
|      - |   81 | ` * php_basename: drop any trailing separators, then answer what follows the last` |
|      - |   82 | ` * remaining one. Shared by basename() and pathinfo() — they used to hand-roll the` |
|      - |   83 | ` * same walk separately, and pathinfo()'s copy kept the trailing separator run` |
|      - |   84 | `` * (`pathinfo("/var/www/")` answered basename "" where php answers "www").`` |
|      - |   85 | ` */` |
|  15572 |   86 | `PH7_PRIVATE const char * PH7_ExtractBaseName(const char *zPath,int nByte,int *pLen)` |
|      5 |   87 | `{` |
|      - |   88 | `	int c,d,iEnd,i;` |
|  15577 |   89 | `	c = d = '/';` |
|      - |   90 | `#ifdef __WINNT__` |
|      5 |   91 | `	d = '\\';` |
|      - |   92 | `#endif` |
|      - |   93 | `#define DIR_IS_SEP(x) ( (int)(x) == c \|\| (int)(x) == d )` |
|  15577 |   94 | `	iEnd = nByte;` |
|  23388 |   95 | `	while( iEnd > 0 && DIR_IS_SEP(zPath[iEnd - 1]) ){` |
|     27 |   96 | `		iEnd--;` |
|      1 |   97 | `	}` |
|  15577 |   98 | `	if( iEnd < 1 ){` |
|      - |   99 | `		/* Empty, or nothing but separators: php answers the empty string */` |
|     29 |  100 | `		*pLen = 0;` |
|     29 |  101 | `		return "";` |
|      - |  102 | `	}` |
|  15549 |  103 | `	i = iEnd;` |
| 420532 |  104 | `	while( i > 0 && !DIR_IS_SEP(zPath[i - 1]) ){` |
| 404988 |  105 | `		i--;` |
|      5 |  106 | `	}` |
|  15549 |  107 | `	*pLen = iEnd - i;` |
|  15549 |  108 | `	return &zPath[i];` |
|      - |  109 | `#undef DIR_IS_SEP` |
|   7791 |  110 | `}` |
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
|  23820 |  122 | `PH7_PRIVATE const char * VfsStrerror(int iErr)` |
|      5 |  123 | `{` |
|      - |  124 | `#if defined(_MSC_VER)` |
|      - |  125 | `#pragma warning(push)` |
|      - |  126 | `#pragma warning(disable:4996)` |
|      - |  127 | `#endif` |
|  23825 |  128 | `	return strerror(iErr);` |
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
|  23764 |  139 | `static void VfsThrowSysWarning(ph7_context *pCtx,const char *zPath)` |
|      5 |  140 | `{` |
|  35651 |  141 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s",` |
|  23764 |  142 | `		ph7_function_name(pCtx),zPath ? zPath : "",VfsStrerror(errno));` |
|  23769 |  143 | `}` |
|     16 |  144 | `PH7_PRIVATE void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile)` |
|      3 |  145 | `{` |
|     27 |  146 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): Failed to open stream: %s",` |
|     16 |  147 | `		ph7_function_name(pCtx),zFile ? zFile : "",VfsStrerror(errno));` |
|     19 |  148 | `}` |
|      - |  149 | `/*` |
|      - |  150 | `` * php's stat-failure warning: `filemtime(): stat failed for /nope`, and`` |
|      - |  151 | `` * `filetype(): Lstat failed for /nope` for the two members that LSTAT. php raises`` |
|      - |  152 | ` * it from php_stat() for the whole family and answers FALSE; PHL answered the` |
|      - |  153 | ` * VFS's raw -1 (or the string "unknown") for most of them, in silence -- and -1 is` |
|      - |  154 | `` * TRUTHY, so `if (filemtime($f))` took the found branch for a file that is not`` |
|      - |  155 | `` * there and `filemtime($a) > filemtime($b)` compared a real time against it.`` |
|      - |  156 | ` */` |
|     20 |  157 | `static void VfsThrowStatWarning(ph7_context *pCtx,const char *zPath,int bLstat)` |
|      1 |  158 | `{` |
|     31 |  159 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s failed for %s",` |
|     10 |  160 | `		ph7_function_name(pCtx),bLstat ? "Lstat" : "stat",zPath ? zPath : "");` |
|     21 |  161 | `}` |
|      - |  162 | `/*` |
|      - |  163 | ` * php's stat() answer is TWENTY-SIX entries, not thirteen: the same thirteen` |
|      - |  164 | ` * fields once at numeric indices 0..12 and once under their names, in this` |
|      - |  165 | ` * order. The numeric half is what php's own documentation indexes by ($s[7] is` |
|      - |  166 | `` * the size) and it is what a `list()`/destructuring reader takes, so a script`` |
|      - |  167 | `` * written against php read `Undefined array key 7` here and answered NULL.`` |
|      - |  168 | ` *` |
|      - |  169 | ` * The VFS fills the NAMED half (both the unix and Windows implementations use` |
|      - |  170 | ` * exactly these keys), so the doubling is done once, here, rather than in every` |
|      - |  171 | ` * xStat: pOut gets the numeric run first and then the names, which is php's own` |
|      - |  172 | ` * insertion order — visible through foreach, print_r, var_dump and json_encode.` |
|      - |  173 | ` */` |
|     22 |  174 | `PH7_PRIVATE int PH7_VfsStatDoubleUp(ph7_value *pIn,ph7_value *pOut)` |
|      1 |  175 | `{` |
|      - |  176 | `	static const char * const azField[] = {` |
|      - |  177 | `		"dev","ino","mode","nlink","uid","gid","rdev","size",` |
|      - |  178 | `		"atime","mtime","ctime","blksize","blocks"` |
|      - |  179 | `	};` |
|      - |  180 | `	sxu32 i;` |
|     23 |  181 | `	if( pIn == 0 \|\| pOut == 0 ){` |
|    ! 0 |  182 | `		return -1;` |
|      - |  183 | `	}` |
|    309 |  184 | `	for( i = 0 ; i < SX_ARRAYSIZE(azField) ; ++i ){` |
|    287 |  185 | `		ph7_value *pField = ph7_array_fetch(pIn,azField[i],-1);` |
|    287 |  186 | `		if( pField == 0 ){` |
|      - |  187 | `			/* A VFS that does not report this field: php always has all thirteen,` |
|      - |  188 | `			 * so the doubling would silently shift every later index. Hand the` |
|      - |  189 | `			 * caller the named-only array it already had instead. */` |
|    ! 0 |  190 | `			return -1;` |
|      - |  191 | `		}` |
|    287 |  192 | `		ph7_array_add_elem(pOut,0,pField);` |
|    144 |  193 | `	}` |
|    309 |  194 | `	for( i = 0 ; i < SX_ARRAYSIZE(azField) ; ++i ){` |
|    287 |  195 | `		ph7_value *pField = ph7_array_fetch(pIn,azField[i],-1);` |
|    287 |  196 | `		ph7_array_add_strkey_elem(pOut,azField[i],pField);` |
|    144 |  197 | `	}` |
|     23 |  198 | `	return PH7_OK;` |
|     12 |  199 | `}` |
|      - |  200 | `/*` |
|      - |  201 | ` * Can this path be stat'ed at all? The three TIME readers report a failure as -1,` |
|      - |  202 | ` * which is also a legitimate timestamp (a file stamped in the last second before` |
|      - |  203 | ` * the epoch), so the failure verdict is asked of the VFS separately rather than` |
|      - |  204 | ` * read off the value -- one extra call, and only on the negative branch.` |
|      - |  205 | ` */` |
|      6 |  206 | `static int VfsPathStatable(ph7_vfs *pVfs,const char *zPath)` |
|      1 |  207 | `{` |
|      7 |  208 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|    ! 0 |  209 | `		return 0;` |
|      - |  210 | `	}` |
|      7 |  211 | `	return pVfs->xFileExists(zPath) == PH7_OK;` |
|      4 |  212 | `}` |
|      - |  213 | `/*` |
|      - |  214 | ` * bool chdir(string $directory)` |
|      - |  215 | ` *  Change the current directory.` |
|      - |  216 | ` * Parameters` |
|      - |  217 | ` *  $directory` |
|      - |  218 | ` *   The new current directory` |
|      - |  219 | ` * Return` |
|      - |  220 | ` *  TRUE on success or FALSE on failure.` |
|      - |  221 | ` */` |
|  14486 |  222 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  223 | `{` |
|      - |  224 | `	const char *zPath;` |
|      - |  225 | `	ph7_vfs *pVfs;` |
|      - |  226 | `	int rc;` |
|      - |  227 | `	/* Only the ARITY is checked here: php coerces a scalar $directory to string,` |
|      - |  228 | `	 * so chdir(123) attempts "123" and warns that it does not exist. Requiring a` |
|      - |  229 | `	 * string outright made that call return FALSE silently, with no diagnostic. */` |
|  14491 |  230 | `	if( nArg < 1 ){` |
|      - |  231 | `		/* Missing argument,return FALSE */` |
|    ! 0 |  232 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  233 | `		return PH7_OK;` |
|      - |  234 | `	}` |
|      - |  235 | `	/* Point to the underlying vfs */` |
|  14491 |  236 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  14491 |  237 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |  238 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  239 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  240 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  241 | `			ph7_function_name(pCtx)` |
|      - |  242 | `			);` |
|    ! 0 |  243 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  244 | `		return PH7_OK;` |
|      - |  245 | `	}` |
|      - |  246 | `	/* Point to the desired directory */` |
|  14491 |  247 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  248 | `	/* Perform the requested operation */` |
|  14491 |  249 | `	errno = 0;` |
|  14491 |  250 | `	rc = pVfs->xChdir(zPath);` |
|  14491 |  251 | `	if( rc != PH7_OK ){` |
|      - |  252 | `		/* chdir has its own php shape: no path, and the errno spelled out. */` |
|      8 |  253 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|      4 |  254 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|      2 |  255 | `	}` |
|      - |  256 | `	/* IO return value */` |
|  14491 |  257 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  14491 |  258 | `	return PH7_OK;` |
|   7248 |  259 | `}` |
|      - |  260 | `/*` |
|      - |  261 | ` * bool chroot(string $directory)` |
|      - |  262 | ` *  Change the root directory.` |
|      - |  263 | ` * Parameters` |
|      - |  264 | ` *  $directory` |
|      - |  265 | ` *   The path to change the root directory to` |
|      - |  266 | ` * Return` |
|      - |  267 | ` *  TRUE on success or FALSE on failure.` |
|      - |  268 | ` *` |
|      - |  269 | ` * POSIX only, like php's: the registration below is guarded the same way, and an` |
|      - |  270 | ` * unreferenced static is an error under the Windows build's /W4 /WX.` |
|      - |  271 | ` */` |
|      - |  272 | `#ifndef __WINNT__` |
|    ! 0 |  273 | `static int PH7_vfs_chroot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - |  274 | `{` |
|      - |  275 | `	const char *zPath;` |
|      - |  276 | `	ph7_vfs *pVfs;` |
|      - |  277 | `	int rc;` |
|    ! 0 |  278 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  279 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  280 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  281 | `		return PH7_OK;` |
|      - |  282 | `	}` |
|      - |  283 | `	/* Point to the underlying vfs */` |
|    ! 0 |  284 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    ! 0 |  285 | `	if( pVfs == 0 \|\| pVfs->xChroot == 0 ){` |
|      - |  286 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  287 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  288 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  289 | `			ph7_function_name(pCtx)` |
|      - |  290 | `			);` |
|    ! 0 |  291 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  292 | `		return PH7_OK;` |
|      - |  293 | `	}` |
|      - |  294 | `	/* Point to the desired directory */` |
|    ! 0 |  295 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  296 | `	/* Perform the requested operation */` |
|    ! 0 |  297 | `	errno = 0;` |
|    ! 0 |  298 | `	rc = pVfs->xChroot(zPath);` |
|    ! 0 |  299 | `	if( rc != PH7_OK ){` |
|      - |  300 | `		/* php's own wording, and the failure a script actually meets: chroot(2)` |
|      - |  301 | `		 * needs privilege, so an ordinary process gets EPERM. PHL answered the` |
|      - |  302 | `		 * bare false in SILENCE — a refused chroot() and a chroot() that did` |
|      - |  303 | `		 * nothing looked the same to the caller. */` |
|    ! 0 |  304 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|    ! 0 |  305 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|    ! 0 |  306 | `	}` |
|      - |  307 | `	/* IO return value */` |
|    ! 0 |  308 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    ! 0 |  309 | `	return PH7_OK;` |
|    ! 0 |  310 | `}` |
|      - |  311 | `#endif /* __WINNT__ */` |
|      - |  312 | `/*` |
|      - |  313 | ` * string getcwd(void)` |
|      - |  314 | ` *  Gets the current working directory.` |
|      - |  315 | ` * Parameters` |
|      - |  316 | ` *  None` |
|      - |  317 | ` * Return` |
|      - |  318 | ` *  Returns the current working directory on success, or FALSE on failure.` |
|      - |  319 | ` */` |
|     18 |  320 | `static int PH7_vfs_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  321 | `{` |
|      - |  322 | `	ph7_vfs *pVfs;` |
|      - |  323 | `	int rc;` |
|      - |  324 | `	/* Point to the underlying vfs */` |
|     23 |  325 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     23 |  326 | `	if( pVfs == 0 \|\| pVfs->xGetcwd == 0 ){` |
|    ! 0 |  327 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 |  328 | `		SXUNUSED(apArg);` |
|      - |  329 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  330 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  331 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  332 | `			ph7_function_name(pCtx)` |
|      - |  333 | `			);` |
|    ! 0 |  334 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  335 | `		return PH7_OK;` |
|      - |  336 | `	}` |
|     23 |  337 | `	ph7_result_string(pCtx,"",0);` |
|      - |  338 | `	/* Perform the requested operation */` |
|     23 |  339 | `	rc = pVfs->xGetcwd(pCtx);` |
|     23 |  340 | `	if( rc != PH7_OK ){` |
|      - |  341 | `		/* Error,return FALSE */` |
|    ! 0 |  342 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  343 | `	}` |
|     23 |  344 | `	return PH7_OK;` |
|     14 |  345 | `}` |
|      - |  346 | `/*` |
|      - |  347 | ` * bool rmdir(string $directory)` |
|      - |  348 | ` *  Removes directory.` |
|      - |  349 | ` * Parameters` |
|      - |  350 | ` *  $directory` |
|      - |  351 | ` *   The path to the directory` |
|      - |  352 | ` * Return` |
|      - |  353 | ` *  TRUE on success or FALSE on failure.` |
|      - |  354 | ` */` |
|     66 |  355 | `static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  356 | `{` |
|      - |  357 | `	const char *zPath;` |
|      - |  358 | `	ph7_vfs *pVfs;` |
|      - |  359 | `	int rc;` |
|     69 |  360 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  361 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  362 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  363 | `		return PH7_OK;` |
|      - |  364 | `	}` |
|      - |  365 | `	/* Point to the underlying vfs */` |
|     69 |  366 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     69 |  367 | `	if( pVfs == 0 \|\| pVfs->xRmdir == 0 ){` |
|      - |  368 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  369 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  370 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  371 | `			ph7_function_name(pCtx)` |
|      - |  372 | `			);` |
|    ! 0 |  373 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  374 | `		return PH7_OK;` |
|      - |  375 | `	}` |
|      - |  376 | `	/* Point to the desired directory */` |
|     69 |  377 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  378 | `	/* Perform the requested operation */` |
|     69 |  379 | `	errno = 0;` |
|     69 |  380 | `	rc = pVfs->xRmdir(zPath);` |
|     69 |  381 | `	if( rc != PH7_OK ){` |
|      8 |  382 | `		VfsThrowSysWarning(pCtx,zPath);` |
|      3 |  383 | `	}` |
|      - |  384 | `	/* IO return value */` |
|     69 |  385 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     69 |  386 | `	return PH7_OK;` |
|     36 |  387 | `}` |
|      - |  388 | `/*` |
|      - |  389 | ` * bool is_dir(string $filename)` |
|      - |  390 | ` *  Tells whether the given filename is a directory.` |
|      - |  391 | ` * Parameters` |
|      - |  392 | ` *  $filename` |
|      - |  393 | ` *   Path to the file.` |
|      - |  394 | ` * Return` |
|      - |  395 | ` *  TRUE on success or FALSE on failure.` |
|      - |  396 | ` */` |
|  10036 |  397 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  398 | `{` |
|      - |  399 | `	const char *zPath;` |
|      - |  400 | `	ph7_vfs *pVfs;` |
|      - |  401 | `	int rc;` |
|  10041 |  402 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  403 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  404 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  405 | `		return PH7_OK;` |
|      - |  406 | `	}` |
|      - |  407 | `	/* Point to the underlying vfs */` |
|  10041 |  408 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  10041 |  409 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  410 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  411 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  412 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  413 | `			ph7_function_name(pCtx)` |
|      - |  414 | `			);` |
|    ! 0 |  415 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  416 | `		return PH7_OK;` |
|      - |  417 | `	}` |
|      - |  418 | `	/* Point to the desired directory */` |
|  10041 |  419 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  420 | `	/* Perform the requested operation */` |
|  10041 |  421 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  422 | `	/* IO return value */` |
|  10041 |  423 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  10041 |  424 | `	return PH7_OK;` |
|   5023 |  425 | `}` |
|      - |  426 | `/*` |
|      - |  427 | ` * bool mkdir(string $pathname[,int $mode = 0777 [,bool $recursive = false])` |
|      - |  428 | ` *  Make a directory.` |
|      - |  429 | ` * Parameters` |
|      - |  430 | ` *  $pathname` |
|      - |  431 | ` *   The directory path.` |
|      - |  432 | ` * $mode` |
|      - |  433 | ` *  The mode is 0777 by default, which means the widest possible access.` |
|      - |  434 | ` *  Note:` |
|      - |  435 | ` *   mode is ignored on Windows.` |
|      - |  436 | ` *   Note that you probably want to specify the mode as an octal number, which means` |
|      - |  437 | ` *   it should have a leading zero. The mode is also modified by the current umask` |
|      - |  438 | ` *   which you can change using umask().` |
|      - |  439 | ` * $recursive` |
|      - |  440 | ` *  Allows the creation of nested directories specified in the pathname.` |
|      - |  441 | ` *  Defaults to FALSE. (Not used)` |
|      - |  442 | ` * Return` |
|      - |  443 | ` *  TRUE on success or FALSE on failure.` |
|      - |  444 | ` */` |
|     66 |  445 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  446 | `{` |
|     70 |  447 | `	int iRecursive = 0;` |
|      - |  448 | `	const char *zPath;` |
|      - |  449 | `	ph7_vfs *pVfs;` |
|      - |  450 | `	int iMode,rc;` |
|     70 |  451 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  452 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  453 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  454 | `		return PH7_OK;` |
|      - |  455 | `	}` |
|      - |  456 | `	/* Point to the underlying vfs */` |
|     70 |  457 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     70 |  458 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  459 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  460 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  461 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  462 | `			ph7_function_name(pCtx)` |
|      - |  463 | `			);` |
|    ! 0 |  464 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  465 | `		return PH7_OK;` |
|      - |  466 | `	}` |
|      - |  467 | `	/* Point to the desired directory */` |
|     70 |  468 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  469 | `#ifdef __WINNT__` |
|      4 |  470 | `	iMode = 0;` |
|      - |  471 | `#else` |
|      - |  472 | `	/* Assume UNIX */` |
|     66 |  473 | `	iMode = 0777;` |
|      - |  474 | `#endif` |
|     70 |  475 | `	if( nArg > 1 ){` |
|    ! 0 |  476 | `		iMode = ph7_value_to_int(apArg[1]);` |
|    ! 0 |  477 | `		if( nArg > 2 ){` |
|    ! 0 |  478 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|    ! 0 |  479 | `		}` |
|    ! 0 |  480 | `	}` |
|      - |  481 | `	/* Perform the requested operation */` |
|     70 |  482 | `	errno = 0;` |
|     70 |  483 | `	rc = pVfs->xMkdir(zPath,iMode,iRecursive);` |
|     70 |  484 | `	if( rc != PH7_OK ){` |
|      - |  485 | `		/* php does NOT name the path for mkdir: "mkdir(): File exists" */` |
|    ! 0 |  486 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|    ! 0 |  487 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|    ! 0 |  488 | `	}` |
|      - |  489 | `	/* IO return value */` |
|     70 |  490 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     70 |  491 | `	return PH7_OK;` |
|     37 |  492 | `}` |
|      - |  493 | `/*` |
|      - |  494 | ` * bool rename(string $oldname,string $newname)` |
|      - |  495 | ` *  Attempts to rename oldname to newname.` |
|      - |  496 | ` * Parameters` |
|      - |  497 | ` *  $oldname` |
|      - |  498 | ` *   Old name.` |
|      - |  499 | ` *  $newname` |
|      - |  500 | ` *   New name.` |
|      - |  501 | ` * Return` |
|      - |  502 | ` *  TRUE on success or FALSE on failure.` |
|      - |  503 | ` */` |
|      2 |  504 | `static int PH7_vfs_rename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  505 | `{` |
|      - |  506 | `	const char *zOld,*zNew;` |
|      - |  507 | `	ph7_vfs *pVfs;` |
|      - |  508 | `	int rc;` |
|      3 |  509 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - |  510 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  511 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  512 | `		return PH7_OK;` |
|      - |  513 | `	}` |
|      - |  514 | `	/* Point to the underlying vfs */` |
|      3 |  515 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  516 | `	if( pVfs == 0 \|\| pVfs->xRename == 0 ){` |
|      - |  517 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  518 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  519 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  520 | `			ph7_function_name(pCtx)` |
|      - |  521 | `			);` |
|    ! 0 |  522 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  523 | `		return PH7_OK;` |
|      - |  524 | `	}` |
|      - |  525 | `	/* Perform the requested operation */` |
|      3 |  526 | `	zOld = ph7_value_to_string(apArg[0],0);` |
|      3 |  527 | `	zNew = ph7_value_to_string(apArg[1],0);` |
|      3 |  528 | `	errno = 0;` |
|      3 |  529 | `	rc = pVfs->xRename(zOld,zNew);` |
|      3 |  530 | `	if( rc != PH7_OK ){` |
|      - |  531 | `		/* php names BOTH paths here */` |
|    ! 0 |  532 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s,%s): %s",` |
|    ! 0 |  533 | `			ph7_function_name(pCtx),zOld,zNew,VfsStrerror(errno));` |
|    ! 0 |  534 | `	}` |
|      - |  535 | `	/* IO result */` |
|      3 |  536 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 |  537 | `	return PH7_OK;` |
|      2 |  538 | `}` |
|      - |  539 | `/*` |
|      - |  540 | ` * string realpath(string $path)` |
|      - |  541 | ` *  Returns canonicalized absolute pathname.` |
|      - |  542 | ` * Parameters` |
|      - |  543 | ` *  $path` |
|      - |  544 | ` *   Target path.` |
|      - |  545 | ` * Return` |
|      - |  546 | ` *  Canonicalized absolute pathname on success. or FALSE on failure.` |
|      - |  547 | ` */` |
|      6 |  548 | `static int PH7_vfs_realpath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  549 | `{` |
|      - |  550 | `	const char *zPath;` |
|      - |  551 | `	ph7_vfs *pVfs;` |
|      - |  552 | `        int rc;` |
|      8 |  553 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  554 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  555 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  556 | `		return PH7_OK;` |
|      - |  557 | `	}` |
|      - |  558 | `	/* Point to the underlying vfs */` |
|      8 |  559 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      8 |  560 | `	if( pVfs == 0 \|\| pVfs->xRealpath == 0 ){` |
|      - |  561 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  562 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  563 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  564 | `			ph7_function_name(pCtx)` |
|      - |  565 | `			);` |
|    ! 0 |  566 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  567 | `		return PH7_OK;` |
|      - |  568 | `	}` |
|      - |  569 | `	/* Set an empty string untnil the underlying OS interface change that */` |
|      8 |  570 | `	ph7_result_string(pCtx,"",0);` |
|      - |  571 | `	/* Perform the requested operation */` |
|      8 |  572 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      8 |  573 | `	rc = pVfs->xRealpath(zPath,pCtx);` |
|      8 |  574 | `	if( rc != PH7_OK ){` |
|      2 |  575 | `	 ph7_result_bool(pCtx,0);` |
|      1 |  576 | `	}` |
|      8 |  577 | `	return PH7_OK;` |
|      5 |  578 | `}` |
|      - |  579 | `/*` |
|      - |  580 | ` * int sleep(int $seconds)` |
|      - |  581 | ` *  Delays the program execution for the given number of seconds.` |
|      - |  582 | ` * Parameters` |
|      - |  583 | ` *  $seconds` |
|      - |  584 | ` *   Halt time in seconds.` |
|      - |  585 | ` * Return` |
|      - |  586 | ` *  Zero on success or FALSE on failure.` |
|      - |  587 | ` */` |
|     10 |  588 | `static int PH7_vfs_sleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  589 | `{` |
|      - |  590 | `	ph7_vfs *pVfs;` |
|      - |  591 | `	int rc,nSleep;` |
|     11 |  592 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  593 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  594 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  595 | `		return PH7_OK;` |
|      - |  596 | `	}` |
|      - |  597 | `	/* Point to the underlying vfs */` |
|     11 |  598 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 |  599 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  600 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  601 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  602 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  603 | `			ph7_function_name(pCtx)` |
|      - |  604 | `			);` |
|    ! 0 |  605 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  606 | `		return PH7_OK;` |
|      - |  607 | `	}` |
|      - |  608 | `	/* Amount to sleep */` |
|     11 |  609 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|     11 |  610 | `	if( nSleep < 0 ){` |
|      - |  611 | `		/* php raises rather than sleeping for an unsigned-wrapped eternity */` |
|      3 |  612 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  613 | `			"sleep(): Argument #1 ($seconds) must be greater than or equal to 0");` |
|      - |  614 | `	}` |
|      - |  615 | `	/* Perform the requested operation (Microseconds) */` |
|      9 |  616 | `	rc = pVfs->xSleep((unsigned int)(nSleep * SX_USEC_PER_SEC));` |
|      9 |  617 | `	if( rc != PH7_OK ){` |
|      - |  618 | `		/* Return FALSE */` |
|    ! 0 |  619 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  620 | `	}else{` |
|      - |  621 | `		/* Return zero */` |
|      9 |  622 | `		ph7_result_int(pCtx,0);` |
|      - |  623 | `	}` |
|      9 |  624 | `	return PH7_OK;` |
|      6 |  625 | `}` |
|      - |  626 | `/*` |
|      - |  627 | ` * void usleep(int $micro_seconds)` |
|      - |  628 | ` *  Delays program execution for the given number of micro seconds.` |
|      - |  629 | ` * Parameters` |
|      - |  630 | ` *  $micro_seconds` |
|      - |  631 | ` *   Halt time in micro seconds. A micro second is one millionth of a second.` |
|      - |  632 | ` * Return` |
|      - |  633 | ` *  None.` |
|      - |  634 | ` */` |
|     58 |  635 | `static int PH7_vfs_usleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  636 | `{` |
|      - |  637 | `	ph7_vfs *pVfs;` |
|      - |  638 | `	int nSleep;` |
|     59 |  639 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  640 | `		/* Missing/Invalid argument,return immediately */` |
|    ! 0 |  641 | `		return PH7_OK;` |
|      - |  642 | `	}` |
|      - |  643 | `	/* Point to the underlying vfs */` |
|     59 |  644 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     59 |  645 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  646 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  647 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  648 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 |  649 | `			ph7_function_name(pCtx)` |
|      - |  650 | `			);` |
|    ! 0 |  651 | `		return PH7_OK;` |
|      - |  652 | `	}` |
|      - |  653 | `	/* Amount to sleep */` |
|     59 |  654 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|     59 |  655 | `	if( nSleep < 0 ){` |
|      - |  656 | `		/* php raises rather than sleeping for an unsigned-wrapped eternity */` |
|      3 |  657 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  658 | `			"usleep(): Argument #1 ($microseconds) must be greater than or equal to 0");` |
|      - |  659 | `	}` |
|      - |  660 | `	/* Perform the requested operation (Microseconds) */` |
|     57 |  661 | `	pVfs->xSleep((unsigned int)nSleep);` |
|     57 |  662 | `	return PH7_OK;` |
|     30 |  663 | `}` |
|      - |  664 | `/*` |
|      - |  665 | ` * bool unlink (string $filename)` |
|      - |  666 | ` *  Delete a file.` |
|      - |  667 | ` * Parameters` |
|      - |  668 | ` *  $filename` |
|      - |  669 | ` *   Path to the file.` |
|      - |  670 | ` * Return` |
|      - |  671 | ` *  TRUE on success or FALSE on failure.` |
|      - |  672 | ` */` |
|  38696 |  673 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  674 | `{` |
|      - |  675 | `	const char *zPath;` |
|      - |  676 | `	ph7_vfs *pVfs;` |
|      - |  677 | `	int rc;` |
|  38701 |  678 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  679 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  680 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  681 | `		return PH7_OK;` |
|      - |  682 | `	}` |
|      - |  683 | `	/* Point to the underlying vfs */` |
|  38701 |  684 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  38701 |  685 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - |  686 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  687 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  688 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  689 | `			ph7_function_name(pCtx)` |
|      - |  690 | `			);` |
|    ! 0 |  691 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  692 | `		return PH7_OK;` |
|      - |  693 | `	}` |
|      - |  694 | `	/* Point to the desired directory */` |
|  38701 |  695 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  696 | `	/* Perform the requested operation */` |
|  38701 |  697 | `	errno = 0;` |
|  38701 |  698 | `	rc = pVfs->xUnlink(zPath);` |
|  38701 |  699 | `	if( rc != PH7_OK ){` |
|  23763 |  700 | `		VfsThrowSysWarning(pCtx,zPath);` |
|  11879 |  701 | `	}` |
|      - |  702 | `	/* IO return value */` |
|  38701 |  703 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  38701 |  704 | `	return PH7_OK;` |
|  19353 |  705 | `}` |
|      - |  706 | `/*` |
|      - |  707 | ` * bool chmod(string $filename,int $mode)` |
|      - |  708 | ` *  Attempts to change the mode of the specified file to that given in mode.` |
|      - |  709 | ` * Parameters` |
|      - |  710 | ` *  $filename` |
|      - |  711 | ` *   Path to the file.` |
|      - |  712 | ` * $mode` |
|      - |  713 | ` *   Mode (Must be an integer)` |
|      - |  714 | ` * Return` |
|      - |  715 | ` *  TRUE on success or FALSE on failure.` |
|      - |  716 | ` */` |
|    168 |  717 | `static int PH7_vfs_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  718 | `{` |
|      - |  719 | `	const char *zPath;` |
|      - |  720 | `	ph7_vfs *pVfs;` |
|      - |  721 | `	int iMode;` |
|      - |  722 | `	int rc;` |
|    170 |  723 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  724 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  725 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  726 | `		return PH7_OK;` |
|      - |  727 | `	}` |
|      - |  728 | `	/* Point to the underlying vfs */` |
|    170 |  729 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    170 |  730 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - |  731 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  732 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  733 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  734 | `			ph7_function_name(pCtx)` |
|      - |  735 | `			);` |
|    ! 0 |  736 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  737 | `		return PH7_OK;` |
|      - |  738 | `	}` |
|      - |  739 | `	/* Point to the desired directory */` |
|    170 |  740 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  741 | `	/* Extract the mode */` |
|    170 |  742 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - |  743 | `	/* Perform the requested operation */` |
|    170 |  744 | `	rc = pVfs->xChmod(zPath,iMode);` |
|      - |  745 | `	/* IO return value */` |
|    170 |  746 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    170 |  747 | `	return PH7_OK;` |
|     86 |  748 | `}` |
|      - |  749 | `/*` |
|      - |  750 | ` * bool chown(string $filename,string $user)` |
|      - |  751 | ` *  Attempts to change the owner of the file filename to user user.` |
|      - |  752 | ` * Parameters` |
|      - |  753 | ` *  $filename` |
|      - |  754 | ` *   Path to the file.` |
|      - |  755 | ` * $user` |
|      - |  756 | ` *   Username.` |
|      - |  757 | ` * Return` |
|      - |  758 | ` *  TRUE on success or FALSE on failure.` |
|      - |  759 | ` */` |
|      6 |  760 | `static int PH7_vfs_chown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  761 | `{` |
|      - |  762 | `	const char *zPath,*zUser;` |
|      - |  763 | `	ph7_vfs *pVfs;` |
|      - |  764 | `	int rc;` |
|      7 |  765 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  766 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  767 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  768 | `		return PH7_OK;` |
|      - |  769 | `	}` |
|      - |  770 | `	/* Point to the underlying vfs */` |
|      7 |  771 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 |  772 | `	if( pVfs == 0 \|\| pVfs->xChown == 0 ){` |
|      - |  773 | `		/* IO routine not implemented,return NULL */` |
|      1 |  774 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  775 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  776 | `			ph7_function_name(pCtx)` |
|      - |  777 | `			);` |
|      1 |  778 | `		ph7_result_bool(pCtx,0);` |
|      1 |  779 | `		return PH7_OK;` |
|      - |  780 | `	}` |
|      - |  781 | `	/* Point to the desired directory */` |
|      6 |  782 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  783 | `	/* Extract the user */` |
|      6 |  784 | `	zUser = ph7_value_to_string(apArg[1],0);` |
|      - |  785 | `	/* Perform the requested operation */` |
|      6 |  786 | `	errno = 0;` |
|      6 |  787 | `	rc = pVfs->xChown(zPath,zUser);` |
|      6 |  788 | `	if( rc != PH7_OK ){` |
|      - |  789 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - |  790 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - |  791 | `		 * "chown(): Operation not permitted". */` |
|      6 |  792 | `		if( rc == -2 ){` |
|      3 |  793 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find uid for %s",` |
|      1 |  794 | `				ph7_function_name(pCtx),zUser);` |
|      1 |  795 | `		}else{` |
|      6 |  796 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 |  797 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - |  798 | `		}` |
|      3 |  799 | `	}` |
|      - |  800 | `	/* IO return value */` |
|      6 |  801 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 |  802 | `	return PH7_OK;` |
|      4 |  803 | `}` |
|      - |  804 | `/*` |
|      - |  805 | ` * bool chgrp(string $filename,string $group)` |
|      - |  806 | ` *  Attempts to change the group of the file filename to group.` |
|      - |  807 | ` * Parameters` |
|      - |  808 | ` *  $filename` |
|      - |  809 | ` *   Path to the file.` |
|      - |  810 | ` * $group` |
|      - |  811 | ` *   groupname.` |
|      - |  812 | ` * Return` |
|      - |  813 | ` *  TRUE on success or FALSE on failure.` |
|      - |  814 | ` */` |
|      6 |  815 | `static int PH7_vfs_chgrp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  816 | `{` |
|      - |  817 | `	const char *zPath,*zGroup;` |
|      - |  818 | `	ph7_vfs *pVfs;` |
|      - |  819 | `	int rc;` |
|      7 |  820 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  821 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  822 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  823 | `		return PH7_OK;` |
|      - |  824 | `	}` |
|      - |  825 | `	/* Point to the underlying vfs */` |
|      7 |  826 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 |  827 | `	if( pVfs == 0 \|\| pVfs->xChgrp == 0 ){` |
|      - |  828 | `		/* IO routine not implemented,return NULL */` |
|      1 |  829 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  830 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  831 | `			ph7_function_name(pCtx)` |
|      - |  832 | `			);` |
|      1 |  833 | `		ph7_result_bool(pCtx,0);` |
|      1 |  834 | `		return PH7_OK;` |
|      - |  835 | `	}` |
|      - |  836 | `	/* Point to the desired directory */` |
|      6 |  837 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  838 | `	/* Extract the user */` |
|      6 |  839 | `	zGroup = ph7_value_to_string(apArg[1],0);` |
|      - |  840 | `	/* Perform the requested operation */` |
|      6 |  841 | `	errno = 0;` |
|      6 |  842 | `	rc = pVfs->xChgrp(zPath,zGroup);` |
|      6 |  843 | `	if( rc != PH7_OK ){` |
|      - |  844 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - |  845 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - |  846 | `		 * "chown(): Operation not permitted". */` |
|      6 |  847 | `		if( rc == -2 ){` |
|      3 |  848 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find gid for %s",` |
|      1 |  849 | `				ph7_function_name(pCtx),zGroup);` |
|      1 |  850 | `		}else{` |
|      6 |  851 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 |  852 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - |  853 | `		}` |
|      3 |  854 | `	}` |
|      - |  855 | `	/* IO return value */` |
|      6 |  856 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 |  857 | `	return PH7_OK;` |
|      4 |  858 | `}` |
|      - |  859 | `/*` |
|      - |  860 | ` * int64 disk_free_space(string $directory)` |
|      - |  861 | ` *  Returns available space on filesystem or disk partition.` |
|      - |  862 | ` * Parameters` |
|      - |  863 | ` *  $directory` |
|      - |  864 | ` *   A directory of the filesystem or disk partition.` |
|      - |  865 | ` * Return` |
|      - |  866 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  867 | ` */` |
|     14 |  868 | `static int PH7_vfs_disk_free_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  869 | `{` |
|      - |  870 | `	const char *zPath;` |
|      - |  871 | `	ph7_int64 iSize;` |
|      - |  872 | `	ph7_vfs *pVfs;` |
|     15 |  873 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  874 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  875 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  876 | `		return PH7_OK;` |
|      - |  877 | `	}` |
|      - |  878 | `	/* Point to the underlying vfs */` |
|     15 |  879 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     15 |  880 | `	if( pVfs == 0 \|\| pVfs->xFreeSpace == 0 ){` |
|      - |  881 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  882 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  883 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  884 | `			ph7_function_name(pCtx)` |
|      - |  885 | `			);` |
|    ! 0 |  886 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  887 | `		return PH7_OK;` |
|      - |  888 | `	}` |
|      - |  889 | `	/* Point to the desired directory */` |
|     15 |  890 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  891 | `	/* Perform the requested operation */` |
|     15 |  892 | `	errno = 0;` |
|     15 |  893 | `	iSize = pVfs->xFreeSpace(zPath);` |
|     15 |  894 | `	if( iSize < 0 ){` |
|      - |  895 | `		/* php answers FALSE and warns with the C library's own reason` |
|      - |  896 | ``		 * (`disk_free_space(): No such file or directory`). PHL answered int(-1) --`` |
|      - |  897 | `		 * truthy, and a plausible-looking byte count for a caller that only tests` |
|      - |  898 | ``		 * `if ($free)` or compares it against a threshold. */`` |
|      7 |  899 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 |  900 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|      5 |  901 | `		ph7_result_bool(pCtx,0);` |
|      5 |  902 | `		return PH7_OK;` |
|      - |  903 | `	}` |
|      - |  904 | `	/* php's return type is FLOAT, not int: the value can exceed what an int holds` |
|      - |  905 | ``	 * on a large volume, and a `===`/gettype()/json_encode() reader sees the`` |
|      - |  906 | `	 * difference on every volume. */` |
|     11 |  907 | `	ph7_result_double(pCtx,(double)iSize);` |
|     11 |  908 | `	return PH7_OK;` |
|      8 |  909 | `}` |
|      - |  910 | `/*` |
|      - |  911 | ` * int64 disk_total_space(string $directory)` |
|      - |  912 | ` *  Returns the total size of a filesystem or disk partition.` |
|      - |  913 | ` * Parameters` |
|      - |  914 | ` *  $directory` |
|      - |  915 | ` *   A directory of the filesystem or disk partition.` |
|      - |  916 | ` * Return` |
|      - |  917 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - |  918 | ` */` |
|     10 |  919 | `static int PH7_vfs_disk_total_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  920 | `{` |
|      - |  921 | `	const char *zPath;` |
|      - |  922 | `	ph7_int64 iSize;` |
|      - |  923 | `	ph7_vfs *pVfs;` |
|     11 |  924 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  925 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  926 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  927 | `		return PH7_OK;` |
|      - |  928 | `	}` |
|      - |  929 | `	/* Point to the underlying vfs */` |
|     11 |  930 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 |  931 | `	if( pVfs == 0 \|\| pVfs->xTotalSpace == 0 ){` |
|      - |  932 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  933 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  934 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  935 | `			ph7_function_name(pCtx)` |
|      - |  936 | `			);` |
|    ! 0 |  937 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  938 | `		return PH7_OK;` |
|      - |  939 | `	}` |
|      - |  940 | `	/* Point to the desired directory */` |
|     11 |  941 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  942 | `	/* Perform the requested operation */` |
|     11 |  943 | `	errno = 0;` |
|     11 |  944 | `	iSize = pVfs->xTotalSpace(zPath);` |
|     11 |  945 | `	if( iSize < 0 ){` |
|      - |  946 | `		/* php answers FALSE and warns with the C library's own reason` |
|      - |  947 | ``		 * (`disk_free_space(): No such file or directory`). PHL answered int(-1) --`` |
|      - |  948 | `		 * truthy, and a plausible-looking byte count for a caller that only tests` |
|      - |  949 | ``		 * `if ($free)` or compares it against a threshold. */`` |
|      4 |  950 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      2 |  951 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|      3 |  952 | `		ph7_result_bool(pCtx,0);` |
|      3 |  953 | `		return PH7_OK;` |
|      - |  954 | `	}` |
|      - |  955 | `	/* php's return type is FLOAT, not int: the value can exceed what an int holds` |
|      - |  956 | ``	 * on a large volume, and a `===`/gettype()/json_encode() reader sees the`` |
|      - |  957 | `	 * difference on every volume. */` |
|      9 |  958 | `	ph7_result_double(pCtx,(double)iSize);` |
|      9 |  959 | `	return PH7_OK;` |
|      6 |  960 | `}` |
|      - |  961 | `/*` |
|      - |  962 | ` * bool file_exists(string $filename)` |
|      - |  963 | ` *  Checks whether a file or directory exists.` |
|      - |  964 | ` * Parameters` |
|      - |  965 | ` *  $filename` |
|      - |  966 | ` *   Path to the file.` |
|      - |  967 | ` * Return` |
|      - |  968 | ` *  TRUE on success or FALSE on failure.` |
|      - |  969 | ` */` |
|    224 |  970 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  971 | `{` |
|      - |  972 | `	const char *zPath;` |
|      - |  973 | `	ph7_vfs *pVfs;` |
|      - |  974 | `	int rc;` |
|    229 |  975 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  976 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  977 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  978 | `		return PH7_OK;` |
|      - |  979 | `	}` |
|      - |  980 | `	/* Point to the underlying vfs */` |
|    229 |  981 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    229 |  982 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - |  983 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  984 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  985 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  986 | `			ph7_function_name(pCtx)` |
|      - |  987 | `			);` |
|    ! 0 |  988 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  989 | `		return PH7_OK;` |
|      - |  990 | `	}` |
|      - |  991 | `	/* Point to the desired directory */` |
|    229 |  992 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  993 | `	/* Perform the requested operation */` |
|    229 |  994 | `	rc = pVfs->xFileExists(zPath);` |
|      - |  995 | `	/* IO return value */` |
|    229 |  996 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    229 |  997 | `	return PH7_OK;` |
|    117 |  998 | `}` |
|      - |  999 | `/*` |
|      - | 1000 | ` * int64 file_size(string $filename)` |
|      - | 1001 | ` *  Gets the size for the given file.` |
|      - | 1002 | ` * Parameters` |
|      - | 1003 | ` *  $filename` |
|      - | 1004 | ` *   Path to the file.` |
|      - | 1005 | ` * Return` |
|      - | 1006 | ` *  File size on success or FALSE on failure.` |
|      - | 1007 | ` */` |
|     14 | 1008 | `static int PH7_vfs_file_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1009 | `{` |
|      - | 1010 | `	const char *zPath;` |
|      - | 1011 | `	ph7_int64 iSize;` |
|      - | 1012 | `	ph7_vfs *pVfs;` |
|     15 | 1013 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1014 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1015 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1016 | `		return PH7_OK;` |
|      - | 1017 | `	}` |
|      - | 1018 | `	/* Point to the underlying vfs */` |
|     15 | 1019 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     15 | 1020 | `	if( pVfs == 0 \|\| pVfs->xFileSize == 0 ){` |
|      - | 1021 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1022 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1023 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1024 | `			ph7_function_name(pCtx)` |
|      - | 1025 | `			);` |
|    ! 0 | 1026 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1027 | `		return PH7_OK;` |
|      - | 1028 | `	}` |
|      - | 1029 | `	/* Point to the desired directory */` |
|     15 | 1030 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1031 | `	/* Perform the requested operation */` |
|     15 | 1032 | `	iSize = pVfs->xFileSize(zPath);` |
|     15 | 1033 | `	if( iSize < 0 ){` |
|      - | 1034 | `		/* php: a stat failure warns and returns FALSE -- PH7 returned int(-1), which is` |
|      - | 1035 | `		 * truthy and compares equal to nothing a caller would test for. */` |
|      4 | 1036 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): stat failed for %s",` |
|      1 | 1037 | `			ph7_function_name(pCtx),zPath);` |
|      3 | 1038 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1039 | `		return PH7_OK;` |
|      - | 1040 | `	}` |
|      - | 1041 | `	/* IO return value */` |
|     13 | 1042 | `	ph7_result_int64(pCtx,iSize);` |
|     13 | 1043 | `	return PH7_OK;` |
|      8 | 1044 | `}` |
|      - | 1045 | `/*` |
|      - | 1046 | ` * int64 fileatime(string $filename)` |
|      - | 1047 | ` *  Gets the last access time of the given file.` |
|      - | 1048 | ` * Parameters` |
|      - | 1049 | ` *  $filename` |
|      - | 1050 | ` *   Path to the file.` |
|      - | 1051 | ` * Return` |
|      - | 1052 | ` *  File atime on success or FALSE on failure.` |
|      - | 1053 | ` */` |
|      8 | 1054 | `static int PH7_vfs_file_atime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1055 | `{` |
|      - | 1056 | `	const char *zPath;` |
|      - | 1057 | `	ph7_int64 iTime;` |
|      - | 1058 | `	ph7_vfs *pVfs;` |
|      9 | 1059 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1060 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1061 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1062 | `		return PH7_OK;` |
|      - | 1063 | `	}` |
|      - | 1064 | `	/* Point to the underlying vfs */` |
|      9 | 1065 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 | 1066 | `	if( pVfs == 0 \|\| pVfs->xFileAtime == 0 ){` |
|      - | 1067 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1068 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1069 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1070 | `			ph7_function_name(pCtx)` |
|      - | 1071 | `			);` |
|    ! 0 | 1072 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1073 | `		return PH7_OK;` |
|      - | 1074 | `	}` |
|      - | 1075 | `	/* Point to the desired directory */` |
|      9 | 1076 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1077 | `	/* Perform the requested operation */` |
|      9 | 1078 | `	iTime = pVfs->xFileAtime(zPath);` |
|      9 | 1079 | `	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){` |
|      - | 1080 | `		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --` |
|      - | 1081 | `		 * truthy, and one second before the epoch is also a real timestamp, which` |
|      - | 1082 | `		 * is why the verdict comes from the VFS rather than from the value. */` |
|      3 | 1083 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      3 | 1084 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1085 | `		return PH7_OK;` |
|      - | 1086 | `	}` |
|      - | 1087 | `	/* IO return value */` |
|      7 | 1088 | `	ph7_result_int64(pCtx,iTime);` |
|      7 | 1089 | `	return PH7_OK;` |
|      5 | 1090 | `}` |
|      - | 1091 | `/*` |
|      - | 1092 | ` * int64 filemtime(string $filename)` |
|      - | 1093 | ` *  Gets file modification time.` |
|      - | 1094 | ` * Parameters` |
|      - | 1095 | ` *  $filename` |
|      - | 1096 | ` *   Path to the file.` |
|      - | 1097 | ` * Return` |
|      - | 1098 | ` *  File mtime on success or FALSE on failure.` |
|      - | 1099 | ` */` |
|     10 | 1100 | `static int PH7_vfs_file_mtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1101 | `{` |
|      - | 1102 | `	const char *zPath;` |
|      - | 1103 | `	ph7_int64 iTime;` |
|      - | 1104 | `	ph7_vfs *pVfs;` |
|     11 | 1105 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1106 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1107 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1108 | `		return PH7_OK;` |
|      - | 1109 | `	}` |
|      - | 1110 | `	/* Point to the underlying vfs */` |
|     11 | 1111 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 1112 | `	if( pVfs == 0 \|\| pVfs->xFileMtime == 0 ){` |
|      - | 1113 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1114 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1115 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1116 | `			ph7_function_name(pCtx)` |
|      - | 1117 | `			);` |
|    ! 0 | 1118 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1119 | `		return PH7_OK;` |
|      - | 1120 | `	}` |
|      - | 1121 | `	/* Point to the desired directory */` |
|     11 | 1122 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1123 | `	/* Perform the requested operation */` |
|     11 | 1124 | `	iTime = pVfs->xFileMtime(zPath);` |
|     11 | 1125 | `	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){` |
|      - | 1126 | `		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --` |
|      - | 1127 | `		 * truthy, and one second before the epoch is also a real timestamp, which` |
|      - | 1128 | `		 * is why the verdict comes from the VFS rather than from the value. */` |
|      3 | 1129 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      3 | 1130 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1131 | `		return PH7_OK;` |
|      - | 1132 | `	}` |
|      - | 1133 | `	/* IO return value */` |
|      9 | 1134 | `	ph7_result_int64(pCtx,iTime);` |
|      9 | 1135 | `	return PH7_OK;` |
|      6 | 1136 | `}` |
|      - | 1137 | `/*` |
|      - | 1138 | ` * int64 filectime(string $filename)` |
|      - | 1139 | ` *  Gets inode change time of file.` |
|      - | 1140 | ` * Parameters` |
|      - | 1141 | ` *  $filename` |
|      - | 1142 | ` *   Path to the file.` |
|      - | 1143 | ` * Return` |
|      - | 1144 | ` *  File ctime on success or FALSE on failure.` |
|      - | 1145 | ` */` |
|      6 | 1146 | `static int PH7_vfs_file_ctime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1147 | `{` |
|      - | 1148 | `	const char *zPath;` |
|      - | 1149 | `	ph7_int64 iTime;` |
|      - | 1150 | `	ph7_vfs *pVfs;` |
|      7 | 1151 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1152 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1153 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1154 | `		return PH7_OK;` |
|      - | 1155 | `	}` |
|      - | 1156 | `	/* Point to the underlying vfs */` |
|      7 | 1157 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 1158 | `	if( pVfs == 0 \|\| pVfs->xFileCtime == 0 ){` |
|      - | 1159 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1160 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1161 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1162 | `			ph7_function_name(pCtx)` |
|      - | 1163 | `			);` |
|    ! 0 | 1164 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1165 | `		return PH7_OK;` |
|      - | 1166 | `	}` |
|      - | 1167 | `	/* Point to the desired directory */` |
|      7 | 1168 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1169 | `	/* Perform the requested operation */` |
|      7 | 1170 | `	iTime = pVfs->xFileCtime(zPath);` |
|      7 | 1171 | `	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){` |
|      - | 1172 | `		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --` |
|      - | 1173 | `		 * truthy, and one second before the epoch is also a real timestamp, which` |
|      - | 1174 | `		 * is why the verdict comes from the VFS rather than from the value. */` |
|      3 | 1175 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      3 | 1176 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1177 | `		return PH7_OK;` |
|      - | 1178 | `	}` |
|      - | 1179 | `	/* IO return value */` |
|      5 | 1180 | `	ph7_result_int64(pCtx,iTime);` |
|      5 | 1181 | `	return PH7_OK;` |
|      4 | 1182 | `}` |
|      - | 1183 | `/*` |
|      - | 1184 | ` * bool is_file(string $filename)` |
|      - | 1185 | ` *  Tells whether the filename is a regular file.` |
|      - | 1186 | ` * Parameters` |
|      - | 1187 | ` *  $filename` |
|      - | 1188 | ` *   Path to the file.` |
|      - | 1189 | ` * Return` |
|      - | 1190 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1191 | ` */` |
|   7776 | 1192 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1193 | `{` |
|      - | 1194 | `	const char *zPath;` |
|      - | 1195 | `	ph7_vfs *pVfs;` |
|      - | 1196 | `	int rc;` |
|   7781 | 1197 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1198 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1199 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1200 | `		return PH7_OK;` |
|      - | 1201 | `	}` |
|      - | 1202 | `	/* Point to the underlying vfs */` |
|   7781 | 1203 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   7781 | 1204 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - | 1205 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1206 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1207 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1208 | `			ph7_function_name(pCtx)` |
|      - | 1209 | `			);` |
|    ! 0 | 1210 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1211 | `		return PH7_OK;` |
|      - | 1212 | `	}` |
|      - | 1213 | `	/* Point to the desired directory */` |
|   7781 | 1214 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1215 | `	/* Perform the requested operation */` |
|   7781 | 1216 | `	rc = pVfs->xIsfile(zPath);` |
|      - | 1217 | `	/* IO return value */` |
|   7781 | 1218 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   7781 | 1219 | `	return PH7_OK;` |
|   3893 | 1220 | `}` |
|      - | 1221 | `/*` |
|      - | 1222 | ` * bool is_link(string $filename)` |
|      - | 1223 | ` *  Tells whether the filename is a symbolic link.` |
|      - | 1224 | ` * Parameters` |
|      - | 1225 | ` *  $filename` |
|      - | 1226 | ` *   Path to the file.` |
|      - | 1227 | ` * Return` |
|      - | 1228 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1229 | ` */` |
|     12 | 1230 | `static int PH7_vfs_is_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1231 | `{` |
|      - | 1232 | `	const char *zPath;` |
|      - | 1233 | `	ph7_vfs *pVfs;` |
|      - | 1234 | `	int rc;` |
|     12 | 1235 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1236 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1237 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1238 | `		return PH7_OK;` |
|      - | 1239 | `	}` |
|      - | 1240 | `	/* Point to the underlying vfs */` |
|     12 | 1241 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     12 | 1242 | `	if( pVfs == 0 \|\| pVfs->xIslink == 0 ){` |
|      - | 1243 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1244 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1245 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1246 | `			ph7_function_name(pCtx)` |
|      - | 1247 | `			);` |
|    ! 0 | 1248 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1249 | `		return PH7_OK;` |
|      - | 1250 | `	}` |
|      - | 1251 | `	/* Point to the desired directory */` |
|     12 | 1252 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1253 | `	/* Perform the requested operation */` |
|     12 | 1254 | `	rc = pVfs->xIslink(zPath);` |
|      - | 1255 | `	/* IO return value */` |
|     12 | 1256 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     12 | 1257 | `	return PH7_OK;` |
|      6 | 1258 | `}` |
|      - | 1259 | `/*` |
|      - | 1260 | ` * bool is_readable(string $filename)` |
|      - | 1261 | ` *  Tells whether a file exists and is readable.` |
|      - | 1262 | ` * Parameters` |
|      - | 1263 | ` *  $filename` |
|      - | 1264 | ` *   Path to the file.` |
|      - | 1265 | ` * Return` |
|      - | 1266 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1267 | ` */` |
|      2 | 1268 | `static int PH7_vfs_is_readable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1269 | `{` |
|      - | 1270 | `	const char *zPath;` |
|      - | 1271 | `	ph7_vfs *pVfs;` |
|      - | 1272 | `	int rc;` |
|      3 | 1273 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1274 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1275 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1276 | `		return PH7_OK;` |
|      - | 1277 | `	}` |
|      - | 1278 | `	/* Point to the underlying vfs */` |
|      3 | 1279 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1280 | `	if( pVfs == 0 \|\| pVfs->xReadable == 0 ){` |
|      - | 1281 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1282 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1283 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1284 | `			ph7_function_name(pCtx)` |
|      - | 1285 | `			);` |
|    ! 0 | 1286 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1287 | `		return PH7_OK;` |
|      - | 1288 | `	}` |
|      - | 1289 | `	/* Point to the desired directory */` |
|      3 | 1290 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1291 | `	/* Perform the requested operation */` |
|      3 | 1292 | `	rc = pVfs->xReadable(zPath);` |
|      - | 1293 | `	/* IO return value */` |
|      3 | 1294 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 1295 | `	return PH7_OK;` |
|      2 | 1296 | `}` |
|      - | 1297 | `/*` |
|      - | 1298 | ` * bool is_writable(string $filename)` |
|      - | 1299 | ` *  Tells whether the filename is writable.` |
|      - | 1300 | ` * Parameters` |
|      - | 1301 | ` *  $filename` |
|      - | 1302 | ` *   Path to the file.` |
|      - | 1303 | ` * Return` |
|      - | 1304 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1305 | ` */` |
|      4 | 1306 | `static int PH7_vfs_is_writable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1307 | `{` |
|      - | 1308 | `	const char *zPath;` |
|      - | 1309 | `	ph7_vfs *pVfs;` |
|      - | 1310 | `	int rc;` |
|      5 | 1311 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1312 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1313 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1314 | `		return PH7_OK;` |
|      - | 1315 | `	}` |
|      - | 1316 | `	/* Point to the underlying vfs */` |
|      5 | 1317 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1318 | `	if( pVfs == 0 \|\| pVfs->xWritable == 0 ){` |
|      - | 1319 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1320 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1321 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1322 | `			ph7_function_name(pCtx)` |
|      - | 1323 | `			);` |
|    ! 0 | 1324 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1325 | `		return PH7_OK;` |
|      - | 1326 | `	}` |
|      - | 1327 | `	/* Point to the desired directory */` |
|      5 | 1328 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1329 | `	/* Perform the requested operation */` |
|      5 | 1330 | `	rc = pVfs->xWritable(zPath);` |
|      - | 1331 | `	/* IO return value */` |
|      5 | 1332 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 1333 | `	return PH7_OK;` |
|      3 | 1334 | `}` |
|      - | 1335 | `/*` |
|      - | 1336 | ` * bool is_executable(string $filename)` |
|      - | 1337 | ` *  Tells whether the filename is executable.` |
|      - | 1338 | ` * Parameters` |
|      - | 1339 | ` *  $filename` |
|      - | 1340 | ` *   Path to the file.` |
|      - | 1341 | ` * Return` |
|      - | 1342 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1343 | ` */` |
|      2 | 1344 | `static int PH7_vfs_is_executable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1345 | `{` |
|      - | 1346 | `	const char *zPath;` |
|      - | 1347 | `	ph7_vfs *pVfs;` |
|      - | 1348 | `	int rc;` |
|      3 | 1349 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1350 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1351 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1352 | `		return PH7_OK;` |
|      - | 1353 | `	}` |
|      - | 1354 | `	/* Point to the underlying vfs */` |
|      3 | 1355 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1356 | `	if( pVfs == 0 \|\| pVfs->xExecutable == 0 ){` |
|      - | 1357 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1358 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1359 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1360 | `			ph7_function_name(pCtx)` |
|      - | 1361 | `			);` |
|    ! 0 | 1362 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1363 | `		return PH7_OK;` |
|      - | 1364 | `	}` |
|      - | 1365 | `	/* Point to the desired directory */` |
|      3 | 1366 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1367 | `	/* Perform the requested operation */` |
|      3 | 1368 | `	rc = pVfs->xExecutable(zPath);` |
|      - | 1369 | `	/* IO return value */` |
|      3 | 1370 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 1371 | `	return PH7_OK;` |
|      2 | 1372 | `}` |
|      - | 1373 | `/*` |
|      - | 1374 | ` * string filetype(string $filename)` |
|      - | 1375 | ` *  Gets file type.` |
|      - | 1376 | ` * Parameters` |
|      - | 1377 | ` *  $filename` |
|      - | 1378 | ` *   Path to the file.` |
|      - | 1379 | ` * Return` |
|      - | 1380 | ` *  The type of the file. Possible values are fifo, char, dir, block, link` |
|      - | 1381 | ` *  file, socket and unknown.` |
|      - | 1382 | ` */` |
|     18 | 1383 | `static int PH7_vfs_filetype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1384 | `{` |
|      - | 1385 | `	const char *zPath;` |
|      - | 1386 | `	ph7_vfs *pVfs;` |
|     19 | 1387 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1388 | `		/* Missing/Invalid argument,return 'unknown' */` |
|    ! 0 | 1389 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 1390 | `		return PH7_OK;` |
|      - | 1391 | `	}` |
|      - | 1392 | `	/* Point to the underlying vfs */` |
|     19 | 1393 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     19 | 1394 | `	if( pVfs == 0 \|\| pVfs->xFiletype == 0 ){` |
|      - | 1395 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1396 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1397 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1398 | `			ph7_function_name(pCtx)` |
|      - | 1399 | `			);` |
|    ! 0 | 1400 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1401 | `		return PH7_OK;` |
|      - | 1402 | `	}` |
|      - | 1403 | `	/* Point to the desired directory */` |
|     19 | 1404 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1405 | `	/* Set the empty string as the default return value */` |
|     19 | 1406 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1407 | `	/* Perform the requested operation */` |
|     19 | 1408 | `	if( pVfs->xFiletype(zPath,pCtx) != PH7_OK ){` |
|      - | 1409 | `		/* php LSTATs here (which is why a symlink answers "link") and a failure is` |
|      - | 1410 | ``		 * the `Lstat failed for` warning plus FALSE. PHL answered the string`` |
|      - | 1411 | `		 * "unknown" -- a real return value of this function, so a caller could not` |
|      - | 1412 | `		 * tell a missing path from a socket or a fifo. */` |
|      3 | 1413 | `		VfsThrowStatWarning(pCtx,zPath,1);` |
|      3 | 1414 | `		ph7_result_bool(pCtx,0);` |
|      1 | 1415 | `	}` |
|     19 | 1416 | `	return PH7_OK;` |
|     10 | 1417 | `}` |
|      - | 1418 | `/*` |
|      - | 1419 | ` * array stat(string $filename)` |
|      - | 1420 | ` *  Gives information about a file.` |
|      - | 1421 | ` * Parameters` |
|      - | 1422 | ` *  $filename` |
|      - | 1423 | ` *   Path to the file.` |
|      - | 1424 | ` * Return` |
|      - | 1425 | ` *  An associative array on success holding the following entries on success` |
|      - | 1426 | ` *  0   dev     device number` |
|      - | 1427 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1428 | ` * 2    mode    inode protection mode` |
|      - | 1429 | ` * 3    nlink   number of links` |
|      - | 1430 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1431 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1432 | ` * 6    rdev    device type, if inode device` |
|      - | 1433 | ` * 7    size    size in bytes` |
|      - | 1434 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1435 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1436 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1437 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1438 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1439 | ` * Note:` |
|      - | 1440 | ` *  FALSE is returned on failure.` |
|      - | 1441 | ` */` |
|     16 | 1442 | `static int PH7_vfs_stat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1443 | `{` |
|      - | 1444 | `	ph7_value *pArray,*pValue;` |
|      - | 1445 | `	const char *zPath;` |
|      - | 1446 | `	ph7_vfs *pVfs;` |
|      - | 1447 | `	int rc;` |
|     17 | 1448 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1449 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1450 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1451 | `		return PH7_OK;` |
|      - | 1452 | `	}` |
|      - | 1453 | `	/* Point to the underlying vfs */` |
|     17 | 1454 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     17 | 1455 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|      - | 1456 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1457 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1458 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1459 | `			ph7_function_name(pCtx)` |
|      - | 1460 | `			);` |
|    ! 0 | 1461 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1462 | `		return PH7_OK;` |
|      - | 1463 | `	}` |
|      - | 1464 | `	/* Create the array and the working value */` |
|     17 | 1465 | `	pArray = ph7_context_new_array(pCtx);` |
|     17 | 1466 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     17 | 1467 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1468 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1469 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1470 | `		return PH7_OK;` |
|      - | 1471 | `	}` |
|      - | 1472 | `	/* Extract the file path */` |
|     17 | 1473 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1474 | `	/* Perform the requested operation */` |
|     17 | 1475 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|     17 | 1476 | `	if( rc != PH7_OK ){` |
|      - | 1477 | ``		/* php warns before answering FALSE -- the same `stat failed for` /`` |
|      - | 1478 | ``		 * `Lstat failed for` line the rest of the family raises. PHL returned the`` |
|      - | 1479 | `		 * FALSE in silence, so a missing path and an empty result looked alike. */` |
|      3 | 1480 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      3 | 1481 | `		ph7_result_bool(pCtx,0);` |
|      2 | 1482 | `	}else{` |
|      - | 1483 | `		/* php's answer is the thirteen fields TWICE: numeric 0..12 then named. */` |
|     15 | 1484 | `		ph7_value *pFull = ph7_context_new_array(pCtx);` |
|     15 | 1485 | `		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){` |
|     15 | 1486 | `			ph7_result_value(pCtx,pFull);` |
|      8 | 1487 | `		}else{` |
|    ! 0 | 1488 | `			ph7_result_value(pCtx,pArray);` |
|      - | 1489 | `		}` |
|      - | 1490 | `	}` |
|      - | 1491 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1492 | `	 * automatically as soon we return from this function. */` |
|     17 | 1493 | `	return PH7_OK;` |
|      9 | 1494 | `}` |
|      - | 1495 | `/*` |
|      - | 1496 | ` * array lstat(string $filename)` |
|      - | 1497 | ` *  Gives information about a file or symbolic link.` |
|      - | 1498 | ` * Parameters` |
|      - | 1499 | ` *  $filename` |
|      - | 1500 | ` *   Path to the file.` |
|      - | 1501 | ` * Return` |
|      - | 1502 | ` *  An associative array on success holding the following entries on success` |
|      - | 1503 | ` *  0   dev     device number` |
|      - | 1504 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1505 | ` * 2    mode    inode protection mode` |
|      - | 1506 | ` * 3    nlink   number of links` |
|      - | 1507 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1508 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1509 | ` * 6    rdev    device type, if inode device` |
|      - | 1510 | ` * 7    size    size in bytes` |
|      - | 1511 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1512 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1513 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1514 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1515 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1516 | ` * Note:` |
|      - | 1517 | ` *  FALSE is returned on failure.` |
|      - | 1518 | ` */` |
|      6 | 1519 | `static int PH7_vfs_lstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1520 | `{` |
|      - | 1521 | `	ph7_value *pArray,*pValue;` |
|      - | 1522 | `	const char *zPath;` |
|      - | 1523 | `	ph7_vfs *pVfs;` |
|      - | 1524 | `	int rc;` |
|      7 | 1525 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1526 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1527 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1528 | `		return PH7_OK;` |
|      - | 1529 | `	}` |
|      - | 1530 | `	/* Point to the underlying vfs */` |
|      7 | 1531 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 1532 | `	if( pVfs == 0 \|\| pVfs->xlStat == 0 ){` |
|      - | 1533 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1534 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1535 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1536 | `			ph7_function_name(pCtx)` |
|      - | 1537 | `			);` |
|    ! 0 | 1538 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1539 | `		return PH7_OK;` |
|      - | 1540 | `	}` |
|      - | 1541 | `	/* Create the array and the working value */` |
|      7 | 1542 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 1543 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      7 | 1544 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1545 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1546 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1547 | `		return PH7_OK;` |
|      - | 1548 | `	}` |
|      - | 1549 | `	/* Extract the file path */` |
|      7 | 1550 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1551 | `	/* Perform the requested operation */` |
|      7 | 1552 | `	rc = pVfs->xlStat(zPath,pArray,pValue);` |
|      7 | 1553 | `	if( rc != PH7_OK ){` |
|      - | 1554 | ``		/* php warns before answering FALSE -- the same `stat failed for` /`` |
|      - | 1555 | ``		 * `Lstat failed for` line the rest of the family raises. PHL returned the`` |
|      - | 1556 | `		 * FALSE in silence, so a missing path and an empty result looked alike. */` |
|      3 | 1557 | `		VfsThrowStatWarning(pCtx,zPath,1);` |
|      3 | 1558 | `		ph7_result_bool(pCtx,0);` |
|      2 | 1559 | `	}else{` |
|      - | 1560 | `		/* php's answer is the thirteen fields TWICE: numeric 0..12 then named. */` |
|      5 | 1561 | `		ph7_value *pFull = ph7_context_new_array(pCtx);` |
|      5 | 1562 | `		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){` |
|      5 | 1563 | `			ph7_result_value(pCtx,pFull);` |
|      3 | 1564 | `		}else{` |
|    ! 0 | 1565 | `			ph7_result_value(pCtx,pArray);` |
|      - | 1566 | `		}` |
|      - | 1567 | `	}` |
|      - | 1568 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1569 | `	 * automatically as soon we return from this function. */` |
|      7 | 1570 | `	return PH7_OK;` |
|      4 | 1571 | `}` |
|      - | 1572 | `/*` |
|      - | 1573 | ` * int\|false fileowner / filegroup / fileinode / fileperms (string $filename)` |
|      - | 1574 | ` *  One stat() with one of its fields taken out of it, which is exactly how php` |
|      - | 1575 | ` *  implements them (php_stat's FS_OWNER / FS_GROUP / FS_INODE / FS_PERMS arms).` |
|      - | 1576 | ` *` |
|      - | 1577 | ` * They were prelude PHP wrapping stat(), which cost them php's diagnostic twice` |
|      - | 1578 | ` * over: three of the four said NOTHING on a failed stat (the fourth raised its own` |
|      - | 1579 | `` * `trigger_error`, so its errno was E_USER_WARNING's 512 rather than E_WARNING's 2`` |
|      - | 1580 | ` * and its line was the prelude's, not the caller's). In C the family shares one` |
|      - | 1581 | ` * warning site with the rest of stat(), and the four get real signature rows.` |
|      - | 1582 | ` */` |
|     26 | 1583 | `static int VfsStatField(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zField)` |
|      1 | 1584 | `{` |
|      - | 1585 | `	ph7_value *pArray,*pValue,*pField;` |
|      - | 1586 | `	const char *zPath;` |
|      - | 1587 | `	ph7_vfs *pVfs;` |
|      - | 1588 | `	int rc;` |
|     27 | 1589 | `	if( nArg < 1 ){` |
|    ! 0 | 1590 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1591 | `		return PH7_OK;` |
|      - | 1592 | `	}` |
|     27 | 1593 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     27 | 1594 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|    ! 0 | 1595 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1596 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1597 | `			ph7_function_name(pCtx)` |
|      - | 1598 | `			);` |
|    ! 0 | 1599 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1600 | `		return PH7_OK;` |
|      - | 1601 | `	}` |
|     27 | 1602 | `	pArray = ph7_context_new_array(pCtx);` |
|     27 | 1603 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     27 | 1604 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1605 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1606 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1607 | `		return PH7_OK;` |
|      - | 1608 | `	}` |
|     27 | 1609 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|     27 | 1610 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|     27 | 1611 | `	if( rc != PH7_OK ){` |
|      9 | 1612 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      9 | 1613 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1614 | `		return PH7_OK;` |
|      - | 1615 | `	}` |
|     19 | 1616 | `	pField = ph7_array_fetch(pArray,zField,-1);` |
|     19 | 1617 | `	if( pField == 0 ){` |
|      - | 1618 | `		/* The VFS answered a stat array without this field: nothing to report but` |
|      - | 1619 | `		 * the failure itself, which is what php answers when its own stat has no` |
|      - | 1620 | `		 * such member either. */` |
|    ! 0 | 1621 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1622 | `		return PH7_OK;` |
|      - | 1623 | `	}` |
|     19 | 1624 | `	ph7_result_int64(pCtx,ph7_value_to_int64(pField));` |
|     19 | 1625 | `	return PH7_OK;` |
|     14 | 1626 | `}` |
|      6 | 1627 | `static int PH7_vfs_file_owner(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1628 | `{` |
|      7 | 1629 | `	return VfsStatField(pCtx,nArg,apArg,"uid");` |
|      1 | 1630 | `}` |
|      4 | 1631 | `static int PH7_vfs_file_group(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1632 | `{` |
|      5 | 1633 | `	return VfsStatField(pCtx,nArg,apArg,"gid");` |
|      1 | 1634 | `}` |
|      4 | 1635 | `static int PH7_vfs_file_inode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1636 | `{` |
|      5 | 1637 | `	return VfsStatField(pCtx,nArg,apArg,"ino");` |
|      1 | 1638 | `}` |
|     12 | 1639 | `static int PH7_vfs_file_perms(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1640 | `{` |
|     13 | 1641 | `	return VfsStatField(pCtx,nArg,apArg,"mode");` |
|      1 | 1642 | `}` |
|      - | 1643 | `/*` |
|      - | 1644 | ` * string getenv(string $varname)` |
|      - | 1645 | ` *  Gets the value of an environment variable.` |
|      - | 1646 | ` * Parameters` |
|      - | 1647 | ` *  $varname` |
|      - | 1648 | ` *   The variable name.` |
|      - | 1649 | ` * Return` |
|      - | 1650 | ` *  Returns the value of the environment variable varname, or FALSE if the environment` |
|      - | 1651 | ` * variable varname does not exist.` |
|      - | 1652 | ` */` |
|     56 | 1653 | `static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1654 | `{` |
|      - | 1655 | `	const char *zEnv;` |
|      - | 1656 | `	ph7_vfs *pVfs;` |
|      - | 1657 | `	int iLen;` |
|     60 | 1658 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1659 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1660 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1661 | `		return PH7_OK;` |
|      - | 1662 | `	}` |
|      - | 1663 | `	/* Point to the underlying vfs */` |
|     60 | 1664 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     60 | 1665 | `	if( pVfs == 0 \|\| pVfs->xGetenv == 0 ){` |
|      - | 1666 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1667 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1668 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1669 | `			ph7_function_name(pCtx)` |
|      - | 1670 | `			);` |
|    ! 0 | 1671 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1672 | `		return PH7_OK;` |
|      - | 1673 | `	}` |
|      - | 1674 | `	/* Extract the environment variable */` |
|     60 | 1675 | `	zEnv = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1676 | `	/* Set a boolean FALSE as the default return value */` |
|     60 | 1677 | `	ph7_result_bool(pCtx,0);` |
|     60 | 1678 | `	if( iLen < 1 ){` |
|      - | 1679 | `		/* Empty string */` |
|    ! 0 | 1680 | `		return PH7_OK;` |
|      - | 1681 | `	}` |
|      - | 1682 | `	/* Perform the requested operation */` |
|     60 | 1683 | `	pVfs->xGetenv(zEnv,pCtx);` |
|     60 | 1684 | `	return PH7_OK;` |
|     32 | 1685 | `}` |
|      - | 1686 | `/*` |
|      - | 1687 | ` * bool putenv(string $settings)` |
|      - | 1688 | ` *  Set the value of an environment variable.` |
|      - | 1689 | ` * Parameters` |
|      - | 1690 | ` *  $setting` |
|      - | 1691 | ` *   The setting, like "FOO=BAR"` |
|      - | 1692 | ` * Return` |
|      - | 1693 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1694 | ` */` |
|      8 | 1695 | `static int PH7_vfs_putenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1696 | `{` |
|      - | 1697 | `	const char *zName,*zValue;` |
|      - | 1698 | `	char *zSettings,*zEnd;` |
|      - | 1699 | `	ph7_vfs *pVfs;` |
|      - | 1700 | `	int iLen,rc;` |
|      9 | 1701 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1702 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1703 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1704 | `		return PH7_OK;` |
|      - | 1705 | `	}` |
|      - | 1706 | `	/* Extract the setting variable */` |
|      9 | 1707 | `	zSettings = (char *)ph7_value_to_string(apArg[0],&iLen);` |
|      9 | 1708 | `	if( iLen < 1 ){` |
|      - | 1709 | `		/* Empty string,return FALSE */` |
|    ! 0 | 1710 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1711 | `		return PH7_OK;` |
|      - | 1712 | `	}` |
|      - | 1713 | `	/* Parse the setting */` |
|      9 | 1714 | `	zEnd = &zSettings[iLen];` |
|      9 | 1715 | `	zValue = 0;` |
|      9 | 1716 | `	zName = zSettings;` |
|    155 | 1717 | `	while( zSettings < zEnd ){` |
|    155 | 1718 | `		if( zSettings[0] == '=' ){` |
|      - | 1719 | `			/* Null terminate the name */` |
|      9 | 1720 | `			zSettings[0] = 0;` |
|      9 | 1721 | `			zValue = &zSettings[1];` |
|      9 | 1722 | `			break;` |
|      - | 1723 | `		}` |
|    147 | 1724 | `		zSettings++;` |
|      1 | 1725 | `	}` |
|      - | 1726 | `	/* Install the environment variable in the $_Env array */` |
|      9 | 1727 | `	if( zValue == 0 \|\| zName[0] == 0 \|\| zValue >= zEnd \|\| zName >= zValue ){` |
|      - | 1728 | `		/* Invalid settings,retun FALSE */` |
|      5 | 1729 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1730 | `		if( zSettings  < zEnd ){` |
|      5 | 1731 | `			zSettings[0] = '=';` |
|      2 | 1732 | `		}` |
|      5 | 1733 | `		return PH7_OK;` |
|      - | 1734 | `	}` |
|      5 | 1735 | `	ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_ENV_ATTR,zName,zValue,(int)(zEnd-zValue));` |
|      - | 1736 | `	/* Point to the underlying vfs */` |
|      5 | 1737 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1738 | `	if( pVfs == 0 \|\| pVfs->xSetenv == 0 ){` |
|      - | 1739 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1740 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1741 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1742 | `			ph7_function_name(pCtx)` |
|      - | 1743 | `			);` |
|    ! 0 | 1744 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1745 | `		zSettings[0] = '=';` |
|    ! 0 | 1746 | `		return PH7_OK;` |
|      - | 1747 | `	}` |
|      - | 1748 | `	/* Perform the requested operation */` |
|      5 | 1749 | `	rc = pVfs->xSetenv(zName,zValue);` |
|      5 | 1750 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      5 | 1751 | `	zSettings[0] = '=';` |
|      5 | 1752 | `	return PH7_OK;` |
|      5 | 1753 | `}` |
|      - | 1754 | `/*` |
|      - | 1755 | ` * bool touch(string $filename[,int64 $time = time()[,int64 $atime]])` |
|      - | 1756 | ` *  Sets access and modification time of file.` |
|      - | 1757 | ` * Note: On windows` |
|      - | 1758 | ` *   If the file does not exists,it will not be created.` |
|      - | 1759 | ` * Parameters` |
|      - | 1760 | ` *  $filename` |
|      - | 1761 | ` *   The name of the file being touched.` |
|      - | 1762 | ` *  $time` |
|      - | 1763 | ` *   The touch time. If time is not supplied, the current system time is used.` |
|      - | 1764 | ` * $atime` |
|      - | 1765 | ` *   If present, the access time of the given filename is set to the value of atime.` |
|      - | 1766 | ` *   Otherwise, it is set to the value passed to the time parameter. If neither are` |
|      - | 1767 | ` *   present, the current system time is used.` |
|      - | 1768 | ` * Return` |
|      - | 1769 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1770 | `*/` |
|     18 | 1771 | `static int PH7_vfs_touch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1772 | `{` |
|      - | 1773 | `	ph7_int64 nTime,nAccess;` |
|      - | 1774 | `	const char *zFile;` |
|      - | 1775 | `	ph7_vfs *pVfs;` |
|      - | 1776 | `	int rc;` |
|     19 | 1777 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1778 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1779 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1780 | `		return PH7_OK;` |
|      - | 1781 | `	}` |
|      - | 1782 | `	/* Point to the underlying vfs */` |
|     19 | 1783 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     19 | 1784 | `	if( pVfs == 0 \|\| pVfs->xTouch == 0 ){` |
|      - | 1785 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1786 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1787 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1788 | `			ph7_function_name(pCtx)` |
|      - | 1789 | `			);` |
|    ! 0 | 1790 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1791 | `		return PH7_OK;` |
|      - | 1792 | `	}` |
|      - | 1793 | `	/* Resolve php's defaults HERE, so the driver only ever sees real timestamps: a` |
|      - | 1794 | ``	 * NEGATIVE stamp is perfectly legal to php (`touch($f, -100)` is 1969), so it`` |
|      - | 1795 | `	 * cannot double as the "not given" sentinel the drivers used to read it as. An` |
|      - | 1796 | `	 * omitted/null $mtime is NOW; an omitted/null $atime follows $mtime. $atime also` |
|      - | 1797 | `	 * used to be read from apArg[1] — the mtime — so touch($f, $m, $a) silently` |
|      - | 1798 | `	 * stamped the modification time onto both. */` |
|     19 | 1799 | `	zFile = ph7_value_to_string(apArg[0],0);` |
|     19 | 1800 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) && nArg > 1 && ph7_value_is_null(apArg[1]) ){` |
|    ! 0 | 1801 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1802 | `			"touch(): Argument #2 ($mtime) cannot be null when argument #3 ($atime) "` |
|      - | 1803 | `			"is an integer");` |
|      - | 1804 | `	}` |
|     19 | 1805 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      3 | 1806 | `		nTime = ph7_value_to_int64(apArg[1]);` |
|      2 | 1807 | `	}else{` |
|      - | 1808 | `		time_t tNow;` |
|     17 | 1809 | `		time(&tNow);` |
|     17 | 1810 | `		nTime = (ph7_int64)tNow;` |
|      - | 1811 | `	}` |
|     19 | 1812 | `	nAccess = nTime;` |
|     19 | 1813 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      3 | 1814 | `		nAccess = ph7_value_to_int64(apArg[2]);` |
|      1 | 1815 | `	}` |
|     19 | 1816 | `	rc = pVfs->xTouch(zFile,nTime,nAccess);` |
|      - | 1817 | `	/* IO result */` |
|     19 | 1818 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     19 | 1819 | `	return PH7_OK;` |
|     10 | 1820 | `}` |
|      - | 1821 | `/*` |
|      - | 1822 | ` * Path processing functions that do not need access to the VFS layer` |
|      - | 1823 | ` * Status:` |
|      - | 1824 | ` *    Stable.` |
|      - | 1825 | ` */` |
|      - | 1826 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1827 | `/*` |
|      - | 1828 | ` * string dirname(string $path)` |
|      - | 1829 |  |
|      - | 1830 | ` *  Returns parent directory's path.` |
|      - | 1831 | ` * Parameters` |
|      - | 1832 | ` * $path` |
|      - | 1833 | ` *  Target path.` |
|      - | 1834 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1835 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1836 | ` * Return` |
|      - | 1837 | ` *  The path of the parent directory. If there are no slashes in path, a dot ('.')` |
|      - | 1838 | ` *  is returned, indicating the current directory.` |
|      - | 1839 | ` */` |
|     94 | 1840 | `static int PH7_builtin_dirname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1841 | `{` |
|      - | 1842 | `	const char *zPath,*zDir;` |
|      - | 1843 | `	int iLen,iDirlen;` |
|     99 | 1844 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1845 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 1846 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1847 | `		return PH7_OK;` |
|      - | 1848 | `	}` |
|      - | 1849 | `	/* Point to the target path */` |
|     99 | 1850 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     99 | 1851 | `	if( iLen < 1 ){` |
|      - | 1852 | `		/* php answers "" for the empty path, not "." */` |
|      3 | 1853 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1854 | `		return PH7_OK;` |
|      - | 1855 | `	}` |
|      - | 1856 | `	/* $levels (php 7.0) was ACCEPTED AND IGNORED, so dirname($p, 3) silently answered` |
|      - | 1857 | `	 * the one-level parent — the caller's own answer, one or more levels too deep. Each` |
|      - | 1858 | `	 * level re-runs php_dirname on the previous result and stops as soon as the answer` |
|      - | 1859 | `	 * stops moving (the filesystem root, or "." for a relative path), which is what php` |
|      - | 1860 | `	 * does; php also rejects a level below 1 outright. */` |
|     97 | 1861 | `	zDir = zPath;` |
|     97 | 1862 | `	iDirlen = iLen;` |
|     97 | 1863 | `	if( nArg > 1 ){` |
|     51 | 1864 | `		ph7_int64 nLevels = ph7_value_to_int64(apArg[1]);` |
|      - | 1865 | `		ph7_int64 i;` |
|     51 | 1866 | `		if( nLevels < 1 ){` |
|      7 | 1867 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1868 | `				"dirname(): Argument #2 ($levels) must be greater than or equal to 1");` |
|      - | 1869 | `		}` |
|    125 | 1870 | `		for( i = 0 ; i < nLevels ; ++i ){` |
|    105 | 1871 | `			int iPrevLen = iDirlen;` |
|    105 | 1872 | `			const char *zPrev = zDir;` |
|    105 | 1873 | `			zDir = PH7_ExtractDirName(zPrev,iPrevLen,&iDirlen);` |
|    105 | 1874 | `			if( iDirlen == iPrevLen && SyMemcmp(zDir,zPrev,(sxu32)iDirlen) == 0 ){` |
|     25 | 1875 | `				break; /* fixed point: "/" and "." are their own parents */` |
|      - | 1876 | `			}` |
|     41 | 1877 | `		}` |
|     23 | 1878 | `	}else{` |
|     47 | 1879 | `		zDir = PH7_ExtractDirName(zPath,iLen,&iDirlen);` |
|      - | 1880 | `	}` |
|      - | 1881 | `	/* Return directory name */` |
|     91 | 1882 | `	ph7_result_string(pCtx,zDir,iDirlen);` |
|     91 | 1883 | `	return PH7_OK;` |
|     52 | 1884 | `}` |
|      - | 1885 | `/*` |
|      - | 1886 | ` * string basename(string $path[, string $suffix ])` |
|      - | 1887 | ` *  Returns trailing name component of path.` |
|      - | 1888 | ` * Parameters` |
|      - | 1889 | ` * $path` |
|      - | 1890 | ` *  Target path.` |
|      - | 1891 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 1892 | ` *  In other environments, it is the forward slash (/).` |
|      - | 1893 | ` * $suffix` |
|      - | 1894 | ` *  If the name component ends in suffix this will also be cut off.` |
|      - | 1895 | ` * Return` |
|      - | 1896 | ` *  The base name of the given path.` |
|      - | 1897 | ` */` |
|     76 | 1898 | `static int PH7_builtin_basename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1899 | `{` |
|      - | 1900 | `	const char *zPath,*zBase;` |
|      - | 1901 | `	int iLen,nBase;` |
|     79 | 1902 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1903 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 1904 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1905 | `		return PH7_OK;` |
|      - | 1906 | `	}` |
|      - | 1907 | `	/* Point to the target path */` |
|     79 | 1908 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1909 | `	/* php_basename, shared with pathinfo(): the hand-rolled walk this used to carry` |
|      - | 1910 | `	 * kept a leading separator on a single-component path (basename("/a") answered` |
|      - | 1911 | `	 * "/a", basename("/.") answered "/.") because it stopped one byte short. */` |
|     79 | 1912 | `	zBase = PH7_ExtractBaseName(zPath,iLen,&nBase);` |
|     79 | 1913 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 1914 | `		const char *zSuffix;` |
|      - | 1915 | `		int nSuffix;` |
|      - | 1916 | `		/* Strip suffix — php leaves the basename alone when it IS the suffix */` |
|      5 | 1917 | `		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);` |
|      4 | 1918 | `		if( nSuffix > 0 && nSuffix < nBase` |
|      5 | 1919 | `		 && SyMemcmp(&zBase[nBase - nSuffix],zSuffix,nSuffix) == 0 ){` |
|      5 | 1920 | `			nBase -= nSuffix;` |
|      2 | 1921 | `		}` |
|      2 | 1922 | `	}` |
|      - | 1923 | `	/* Store the basename */` |
|     79 | 1924 | `	ph7_result_string(pCtx,zBase,nBase);` |
|     79 | 1925 | `	return PH7_OK;` |
|     41 | 1926 | `}` |
|      - | 1927 | `/*` |
|      - | 1928 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 1929 | ` *  Returns information about a file path.` |
|      - | 1930 | ` * Parameter` |
|      - | 1931 | ` *  $path` |
|      - | 1932 | ` *   The path to be parsed.` |
|      - | 1933 | ` *  $options` |
|      - | 1934 | ` *    If present, specifies a specific element to be returned; one of` |
|      - | 1935 | ` *      PATHINFO_DIRNAME, PATHINFO_BASENAME, PATHINFO_EXTENSION or PATHINFO_FILENAME.` |
|      - | 1936 | ` * Return` |
|      - | 1937 | ` *  If the options parameter is not passed, an associative array containing the following` |
|      - | 1938 | ` *  elements is returned: dirname, basename, extension (if any), and filename.` |
|      - | 1939 | ` *  If options is present, returns a string containing the requested element.` |
|      - | 1940 | ` */` |
|      - | 1941 | `typedef struct path_info path_info;` |
|      - | 1942 | `struct path_info` |
|      - | 1943 | `{` |
|      - | 1944 | `	SyString sDir; /* Directory [i.e: /var/www] */` |
|      - | 1945 | `	SyString sBasename; /* Basename [i.e httpd.conf] */` |
|      - | 1946 | `	SyString sExtension; /* File extension [i.e xml,pdf..] */` |
|      - | 1947 | `	SyString sFilename;  /* Filename */` |
|      - | 1948 | `	int iPresent;        /* Which components php would EMIT (PH7_PATHINFO_* bits) */` |
|      - | 1949 | `};` |
|      - | 1950 | `/*` |
|      - | 1951 | ` * Extract path fields exactly as php's pathinfo() assembles them.` |
|      - | 1952 | ` *` |
|      - | 1953 | ` * Two things this has to get right beyond the values themselves:` |
|      - | 1954 | ` *` |
|      - | 1955 | ` *  - php looks for the LAST dot ANYWHERE in the basename, a leading one included,` |
|      - | 1956 | `` *    so `.bashrc` has extension "bashrc" and filename "" (PH7 stopped the scan`` |
|      - | 1957 | ` *    before the first byte, so it reported no extension and filename ".bashrc").` |
|      - | 1958 | ` *  - EMPTY is not the same as ABSENT. php always emits basename and filename when` |
|      - | 1959 | `` *    they are asked for, emits extension whenever a dot exists (even for `x.`,`` |
|      - | 1960 | ` *    whose extension is ""), and emits dirname only when it is non-empty. The` |
|      - | 1961 | ` *    scalar form answers with the first EMITTED component, so conflating the two` |
|      - | 1962 | `` *    makes `pathinfo("x.", PATHINFO_EXTENSION\|PATHINFO_FILENAME)` fall through to`` |
|      - | 1963 | ` *    the filename ("x") where php answers "" — iPresent keeps them apart.` |
|      - | 1964 | ` *` |
|      - | 1965 | ` * dirname and basename come from the shared php_dirname/php_basename helpers` |
|      - | 1966 | ` * rather than a third hand-rolled walk, so the trailing-separator and` |
|      - | 1967 | ` * relative-path rules ("file.txt" -> ".", "/var/www/" -> "/var" + "www") cannot` |
|      - | 1968 | ` * drift between the two builtins and this one.` |
|      - | 1969 | ` */` |
|  15436 | 1970 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 1971 | `{` |
|      - | 1972 | `	const char *zBase,*zDir,*zDot;` |
|      - | 1973 | `	int nBase,nDir,i;` |
|      - | 1974 | `	/* Zero the structure */` |
|  15441 | 1975 | `	SyZero(pOut,sizeof(path_info));` |
|  15441 | 1976 | `	zDir = PH7_ExtractDirName(zPath,nByte,&nDir);` |
|  15441 | 1977 | `	if( nDir > 0 ){` |
|  15437 | 1978 | `		SyStringInitFromBuf(&pOut->sDir,zDir,nDir);` |
|  15437 | 1979 | `		pOut->iPresent \|= PH7_PATHINFO_DIRNAME;` |
|   7716 | 1980 | `	}` |
|  15441 | 1981 | `	zBase = PH7_ExtractBaseName(zPath,nByte,&nBase);` |
|  15441 | 1982 | `	SyStringInitFromBuf(&pOut->sBasename,zBase,nBase);` |
|  15441 | 1983 | `	pOut->iPresent \|= PH7_PATHINFO_BASENAME\|PH7_PATHINFO_FILENAME;` |
|      - | 1984 | `	/* Last dot anywhere in the basename splits filename from extension */` |
|  15441 | 1985 | `	zDot = 0;` |
|  77123 | 1986 | `	for( i = nBase ; i > 0 ; --i ){` |
|  77101 | 1987 | `		if( zBase[i - 1] == '.' ){` |
|  15419 | 1988 | `			zDot = &zBase[i - 1];` |
|  15419 | 1989 | `			break;` |
|      - | 1990 | `		}` |
|  30846 | 1991 | `	}` |
|  15441 | 1992 | `	if( zDot ){` |
|  15419 | 1993 | `		SyStringInitFromBuf(&pOut->sExtension,zDot + 1,(int)(&zBase[nBase] - (zDot + 1)));` |
|  15419 | 1994 | `		pOut->iPresent \|= PH7_PATHINFO_EXTENSION;` |
|  15419 | 1995 | `		SyStringInitFromBuf(&pOut->sFilename,zBase,(int)(zDot - zBase));` |
|   7712 | 1996 | `	}else{` |
|     23 | 1997 | `		SyStringInitFromBuf(&pOut->sFilename,zBase,nBase);` |
|      - | 1998 | `	}` |
|  15441 | 1999 | `	return SXRET_OK;` |
|      5 | 2000 | `}` |
|      - | 2001 | `/*` |
|      - | 2002 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 2003 | ` *  See block comment above.` |
|      - | 2004 | ` */` |
|  15436 | 2005 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2006 | `{` |
|      - | 2007 | `	const char *zPath;` |
|      - | 2008 | `	path_info sInfo;` |
|      - | 2009 | `	int iLen;` |
|  15441 | 2010 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2011 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 2012 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2013 | `		return PH7_OK;` |
|      - | 2014 | `	}` |
|      - | 2015 | `	/* Point to the target path. The EMPTY path is not a special case: php still` |
|      - | 2016 | ``	 * answers with the array `["basename" => "", "filename" => ""]` (and "" for a`` |
|      - | 2017 | `	 * scalar request), where PH7 short-circuited to "" and returned the wrong TYPE. */` |
|  15441 | 2018 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 2019 | `	/* Extract path info */` |
|  15441 | 2020 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|      - | 2021 | ``	/* Read the mask at 64-bit width: ph7_value_to_int() truncates to `int`, so a`` |
|      - | 2022 | `	 * flags value congruent to PATHINFO_ALL mod 2^32 (4294967311, -4294967281 …)` |
|      - | 2023 | `	 * would take the ARRAY branch and answer with the wrong TYPE. */` |
|  15436 | 2024 | `	if( nArg > 1 && ph7_value_is_int(apArg[1])` |
|  23136 | 2025 | `	 && ph7_value_to_int64(apArg[1]) != (ph7_int64)PH7_PATHINFO_ALL ){` |
|      - | 2026 | `		/* $flags is a BITMASK, not an enum: php assembles the requested components in` |
|      - | 2027 | `		 * the fixed order below and, for anything other than PATHINFO_ALL, hands back` |
|      - | 2028 | `		 * the FIRST one it EMITTED (zend_hash_get_current_data on the fresh array).` |
|      - | 2029 | ``		 * So `PATHINFO_DIRNAME\|PATHINFO_BASENAME` answers the dirname, and an unknown`` |
|      - | 2030 | `		 * bit that happens to carry a known one along (99 = 1\|2\|32\|64) answers as if` |
|      - | 2031 | `		 * only the known ones were passed. PH7 numbered the components 1/2/3/4 and` |
|      - | 2032 | `		 * switched on the whole value, so it read a two-flag mask as a different single` |
|      - | 2033 | `		 * component and answered "" for everything else. Emission is iPresent, NOT` |
|      - | 2034 | `		 * "non-empty": an emitted-but-empty component ends the search with "". */` |
|  15423 | 2035 | `		ph7_int64 nComp = ph7_value_to_int64(apArg[1]);` |
|      - | 2036 | `		static const int aBit[4] = {` |
|      - | 2037 | `			PH7_PATHINFO_DIRNAME,PH7_PATHINFO_BASENAME,` |
|      - | 2038 | `			PH7_PATHINFO_EXTENSION,PH7_PATHINFO_FILENAME` |
|      - | 2039 | `		};` |
|      - | 2040 | `		SyString *apComp[4];` |
|      - | 2041 | `		int i;` |
|  15423 | 2042 | `		apComp[0] = &sInfo.sDir;` |
|  15423 | 2043 | `		apComp[1] = &sInfo.sBasename;` |
|  15423 | 2044 | `		apComp[2] = &sInfo.sExtension;` |
|  15423 | 2045 | `		apComp[3] = &sInfo.sFilename;` |
|      - | 2046 | `		/* Expand the empty string unless a requested component is emitted */` |
|  15423 | 2047 | `		ph7_result_string(pCtx,"",0);` |
|  53913 | 2048 | `		for( i = 0 ; i < 4 ; ++i ){` |
|  53905 | 2049 | `			if( (nComp & aBit[i]) == aBit[i] && (sInfo.iPresent & aBit[i]) ){` |
|  15415 | 2050 | `				ph7_result_string(pCtx,apComp[i]->zString,(int)apComp[i]->nByte);` |
|  15415 | 2051 | `				break;` |
|      - | 2052 | `			}` |
|  19250 | 2053 | `		}` |
|   7714 | 2054 | `	}else{` |
|      - | 2055 | `		/* Return an associative array */` |
|      - | 2056 | `		ph7_value *pArray,*pValue;` |
|     19 | 2057 | `		pArray = ph7_context_new_array(pCtx);` |
|     19 | 2058 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     19 | 2059 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2060 | `			/* Out of mem,return NULL */` |
|    ! 0 | 2061 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2062 | `			return PH7_OK;` |
|      - | 2063 | `		}` |
|      - | 2064 | ``		/* Emitted-but-EMPTY components are in the array too (php keys `basename` and`` |
|      - | 2065 | ``		 * `filename` for "/" with ""), so this walks iPresent, not the lengths. */`` |
|      - | 2066 | `		{` |
|      - | 2067 | `		static const char *azKey[4] = {"dirname","basename","extension","filename"};` |
|      - | 2068 | `		static const int aBit[4] = {` |
|      - | 2069 | `			PH7_PATHINFO_DIRNAME,PH7_PATHINFO_BASENAME,` |
|      - | 2070 | `			PH7_PATHINFO_EXTENSION,PH7_PATHINFO_FILENAME` |
|      - | 2071 | `		};` |
|      - | 2072 | `		SyString *apComp[4];` |
|      - | 2073 | `		int i;` |
|     19 | 2074 | `		apComp[0] = &sInfo.sDir;` |
|     19 | 2075 | `		apComp[1] = &sInfo.sBasename;` |
|     19 | 2076 | `		apComp[2] = &sInfo.sExtension;` |
|     19 | 2077 | `		apComp[3] = &sInfo.sFilename;` |
|     91 | 2078 | `		for( i = 0 ; i < 4 ; ++i ){` |
|     73 | 2079 | `			if( (sInfo.iPresent & aBit[i]) == 0 ){` |
|     11 | 2080 | `				continue;` |
|      - | 2081 | `			}` |
|     63 | 2082 | `			ph7_value_reset_string_cursor(pValue);` |
|     63 | 2083 | `			ph7_value_string(pValue,apComp[i]->zString,(int)apComp[i]->nByte);` |
|     63 | 2084 | `			ph7_array_add_strkey_elem(pArray,azKey[i],pValue); /* Will make it's own copy */` |
|     32 | 2085 | `		}` |
|      - | 2086 | `		}` |
|      - | 2087 | `		/* Return the created array */` |
|     19 | 2088 | `		ph7_result_value(pCtx,pArray);` |
|      - | 2089 | `		/* Don't worry about freeing memory, everything will be released` |
|      - | 2090 | `		 * automatically as soon we return from this foreign function.` |
|      - | 2091 | `		 */` |
|      - | 2092 | `	}` |
|  15441 | 2093 | `	return PH7_OK;` |
|   7723 | 2094 | `}` |
|      - | 2095 | `/* SPDX-SnippetBegin */` |
|      - | 2096 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 2097 | `/* SPDX-License-Identifier: blessing */` |
|      - | 2098 | `/*` |
|      - | 2099 | ` * Globbing implementation extracted from the sqlite3 source tree.` |
|      - | 2100 |  |
|      - | 2101 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|      - | 2102 | ` * Status: Public Domain` |
|      - | 2103 | ` */` |
|      - | 2104 | `typedef unsigned char u8;` |
|      - | 2105 | `/* An array to map all upper-case characters into their corresponding` |
|      - | 2106 | `** lower-case character.` |
|      - | 2107 | `**` |
|      - | 2108 | `** SQLite only considers US-ASCII (or EBCDIC) characters.  We do not` |
|      - | 2109 | `** handle case conversions for the UTF character set since the tables` |
|      - | 2110 | `** involved are nearly as big or bigger than SQLite itself.` |
|      - | 2111 | `*/` |
|      - | 2112 | `static const unsigned char sqlite3UpperToLower[] = {` |
|      - | 2113 | `      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,` |
|      - | 2114 | `     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,` |
|      - | 2115 | `     36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,` |
|      - | 2116 | `     54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,` |
|      - | 2117 | `    104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,` |
|      - | 2118 | `    122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,` |
|      - | 2119 | `    108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,` |
|      - | 2120 | `    126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,` |
|      - | 2121 | `    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,` |
|      - | 2122 | `    162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,` |
|      - | 2123 | `    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,` |
|      - | 2124 | `    198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,` |
|      - | 2125 | `    216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,` |
|      - | 2126 | `    234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,` |
|      - | 2127 | `    252,253,254,255` |
|      - | 2128 | `};` |
|      - | 2129 | `#define GlogUpperToLower(A)     if( A<0x80 ){ A = sqlite3UpperToLower[A]; }` |
|      - | 2130 | `/*` |
|      - | 2131 | `** Assuming zIn points to the first byte of a UTF-8 character,` |
|      - | 2132 | `** advance zIn to point to the first byte of the next UTF-8 character.` |
|      - | 2133 | `*/` |
|      - | 2134 | `#define SQLITE_SKIP_UTF8(zIn) {                        \` |
|      - | 2135 | `  if( (*(zIn++))>=0xc0 ){                              \` |
|      - | 2136 | `    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \` |
|      - | 2137 | `  }                                                    \` |
|      - | 2138 | `}` |
|      - | 2139 | `/*` |
|      - | 2140 | `** Compare two UTF-8 strings for equality where the first string can` |
|      - | 2141 | `** potentially be a "glob" expression.  Return true (1) if they` |
|      - | 2142 | `** are the same and false (0) if they are different.` |
|      - | 2143 | `**` |
|      - | 2144 | `** Globbing rules:` |
|      - | 2145 | `**` |
|      - | 2146 | `**      '*'       Matches any sequence of zero or more characters.` |
|      - | 2147 | `**` |
|      - | 2148 | `**      '?'       Matches exactly one character.` |
|      - | 2149 | `**` |
|      - | 2150 | `**     [...]      Matches one character from the enclosed list of` |
|      - | 2151 | `**                characters.` |
|      - | 2152 | `**` |
|      - | 2153 | `**     [^...]     Matches one character not in the enclosed list.` |
|      - | 2154 | `**` |
|      - | 2155 | `** With the [...] and [^...] matching, a ']' character can be included` |
|      - | 2156 | `** in the list by making it the first character after '[' or '^'.  A` |
|      - | 2157 | `** range of characters can be specified using '-'.  Example:` |
|      - | 2158 | `** "[a-z]" matches any single lower-case letter.  To match a '-', make` |
|      - | 2159 | `** it the last character in the list.` |
|      - | 2160 | `**` |
|      - | 2161 | `** This routine is usually quick, but can be N**2 in the worst case.` |
|      - | 2162 | `**` |
|      - | 2163 | `** Hints: to match '*' or '?', put them in "[]".  Like this:` |
|      - | 2164 | `**` |
|      - | 2165 | `**         abc[*]xyz        Matches "abc*xyz" only` |
|      - | 2166 | `*/` |
|    188 | 2167 | `static int patternCompare(` |
|      - | 2168 | `  const u8 *zPattern,              /* The glob pattern */` |
|      - | 2169 | `  const u8 *zString,               /* The string to compare against the glob */` |
|      - | 2170 | `  const int esc,                    /* The escape character */` |
|      - | 2171 | `  int noCase` |
|      1 | 2172 | `){` |
|      - | 2173 | `  int c, c2;` |
|      - | 2174 | `  int invert;` |
|      - | 2175 | `  int seen;` |
|    189 | 2176 | `  u8 matchOne = '?';` |
|    189 | 2177 | `  u8 matchAll = '*';` |
|    189 | 2178 | `  u8 matchSet = '[';` |
|    189 | 2179 | `  int prevEscape = 0;     /* True if the previous character was 'escape' */` |
|      - | 2180 |  |
|    189 | 2181 | `  if( !zPattern \|\| !zString ) return 0;` |
|    251 | 2182 | `  while( (c = PH7_Utf8Read(zPattern,0,&zPattern))!=0 ){` |
|    237 | 2183 | `    if( !prevEscape && c==matchAll ){` |
|    112 | 2184 | `      while( (c=PH7_Utf8Read(zPattern,0,&zPattern)) == matchAll` |
|     57 | 2185 | `               \|\| c == matchOne ){` |
|    ! 0 | 2186 | `        if( c==matchOne && PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 2187 | `          return 0;` |
|      - | 2188 | `        }` |
|    ! 0 | 2189 | `      }` |
|     57 | 2190 | `      if( c==0 ){` |
|     31 | 2191 | `        return 1;` |
|     27 | 2192 | `      }else if( c==esc ){` |
|    ! 0 | 2193 | `        c = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2194 | `        if( c==0 ){` |
|    ! 0 | 2195 | `          return 0;` |
|    ! 0 | 2196 | `        }` |
|     27 | 2197 | `      }else if( c==matchSet ){` |
|    ! 0 | 2198 | `	  if( (esc==0) \|\| (matchSet<0x80) ) return 0;` |
|    ! 0 | 2199 | `	  while( *zString && patternCompare(&zPattern[-1],zString,esc,noCase)==0 ){` |
|    ! 0 | 2200 | `          SQLITE_SKIP_UTF8(zString);` |
|    ! 0 | 2201 | `        }` |
|    ! 0 | 2202 | `        return *zString!=0;` |
|      - | 2203 | `      }` |
|     41 | 2204 | `      while( (c2 = PH7_Utf8Read(zString,0,&zString))!=0 ){` |
|     41 | 2205 | `        if( noCase ){` |
|      3 | 2206 | `          GlogUpperToLower(c2);` |
|      3 | 2207 | `          GlogUpperToLower(c);` |
|     11 | 2208 | `          while( c2 != 0 && c2 != c ){` |
|      9 | 2209 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      9 | 2210 | `            GlogUpperToLower(c2);` |
|      1 | 2211 | `          }` |
|      2 | 2212 | `        }else{` |
|    131 | 2213 | `          while( c2 != 0 && c2 != c ){` |
|     93 | 2214 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      1 | 2215 | `          }` |
|      - | 2216 | `        }` |
|     41 | 2217 | `        if( c2==0 ) return 0;` |
|     27 | 2218 | `		if( patternCompare(zPattern,zString,esc,noCase) ) return 1;` |
|      1 | 2219 | `      }` |
|    ! 0 | 2220 | `      return 0;` |
|    181 | 2221 | `    }else if( !prevEscape && c==matchOne ){` |
|    ! 0 | 2222 | `      if( PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 2223 | `        return 0;` |
|    ! 0 | 2224 | `      }` |
|    181 | 2225 | `    }else if( c==matchSet ){` |
|    ! 0 | 2226 | `      int prior_c = 0;` |
|    ! 0 | 2227 | `      if( esc == 0 ) return 0;` |
|    ! 0 | 2228 | `      seen = 0;` |
|    ! 0 | 2229 | `      invert = 0;` |
|    ! 0 | 2230 | `      c = PH7_Utf8Read(zString, 0, &zString);` |
|    ! 0 | 2231 | `      if( c==0 ) return 0;` |
|    ! 0 | 2232 | `      c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2233 | `      if( c2=='^' ){` |
|    ! 0 | 2234 | `        invert = 1;` |
|    ! 0 | 2235 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2236 | `      }` |
|    ! 0 | 2237 | `      if( c2==']' ){` |
|    ! 0 | 2238 | `        if( c==']' ) seen = 1;` |
|    ! 0 | 2239 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2240 | `      }` |
|    ! 0 | 2241 | `      while( c2 && c2!=']' ){` |
|    ! 0 | 2242 | `        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){` |
|    ! 0 | 2243 | `          c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2244 | `          if( c>=prior_c && c<=c2 ) seen = 1;` |
|    ! 0 | 2245 | `          prior_c = 0;` |
|    ! 0 | 2246 | `        }else{` |
|    ! 0 | 2247 | `          if( c==c2 ){` |
|    ! 0 | 2248 | `            seen = 1;` |
|    ! 0 | 2249 | `          }` |
|    ! 0 | 2250 | `          prior_c = c2;` |
|      - | 2251 | `        }` |
|    ! 0 | 2252 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2253 | `      }` |
|    ! 0 | 2254 | `      if( c2==0 \|\| (seen ^ invert)==0 ){` |
|    ! 0 | 2255 | `        return 0;` |
|    ! 0 | 2256 | `      }` |
|    181 | 2257 | `    }else if( esc==c && !prevEscape ){` |
|    ! 0 | 2258 | `      prevEscape = 1;` |
|    ! 0 | 2259 | `    }else{` |
|    181 | 2260 | `      c2 = PH7_Utf8Read(zString, 0, &zString);` |
|    181 | 2261 | `      if( noCase ){` |
|      7 | 2262 | `        GlogUpperToLower(c);` |
|      7 | 2263 | `        GlogUpperToLower(c2);` |
|      3 | 2264 | `      }` |
|    181 | 2265 | `      if( c!=c2 ){` |
|    119 | 2266 | `        return 0;` |
|      - | 2267 | `      }` |
|     63 | 2268 | `      prevEscape = 0;` |
|      - | 2269 | `    }` |
|      1 | 2270 | `  }` |
|     15 | 2271 | `  return *zString==0;` |
|     95 | 2272 | `}` |
|      - | 2273 | `/* SPDX-SnippetEnd */` |
|      - | 2274 | `/*` |
|      - | 2275 | ` * Wrapper around patternCompare() defined above.` |
|      - | 2276 | ` * See block comment above for more information.` |
|      - | 2277 | ` */` |
|    162 | 2278 | `static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,int CaseCompare)` |
|      1 | 2279 | `{` |
|      - | 2280 | `	int rc;` |
|    163 | 2281 | `	if( iEsc < 0 ){` |
|    ! 0 | 2282 | `		iEsc = '\\';` |
|    ! 0 | 2283 | `	}` |
|    163 | 2284 | `	rc = patternCompare(zPattern,zString,iEsc,CaseCompare);` |
|    163 | 2285 | `	return rc;` |
|      1 | 2286 | `}` |
|      - | 2287 | `/*` |
|      - | 2288 | ` * bool fnmatch(string $pattern,string $string[,int $flags = 0 ])` |
|      - | 2289 | ` *  Match filename against a pattern.` |
|      - | 2290 | ` * Parameters` |
|      - | 2291 | ` *  $pattern` |
|      - | 2292 | ` *   The shell wildcard pattern.` |
|      - | 2293 | ` * $string` |
|      - | 2294 | ` *  The tested string.` |
|      - | 2295 | ` * $flags` |
|      - | 2296 | ` *   A list of possible flags:` |
|      - | 2297 | ` *    FNM_NOESCAPE 	Disable backslash escaping.` |
|      - | 2298 | ` *    FNM_PATHNAME 	Slash in string only matches slash in the given pattern.` |
|      - | 2299 | ` *    FNM_PERIOD 	Leading period in string must be exactly matched by period in the given pattern.` |
|      - | 2300 | ` *    FNM_CASEFOLD 	Caseless match.` |
|      - | 2301 | ` * Return` |
|      - | 2302 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 2303 | ` */` |
|      8 | 2304 | `static int PH7_builtin_fnmatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2305 | `{` |
|      - | 2306 | `	const char *zString,*zPattern;` |
|      9 | 2307 | `	int iEsc = '\\';` |
|      9 | 2308 | `	int noCase = 0;` |
|      - | 2309 | `	int rc;` |
|      9 | 2310 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2311 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2312 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2313 | `		return PH7_OK;` |
|      - | 2314 | `	}` |
|      - | 2315 | `	/* Extract the pattern and the string */` |
|      9 | 2316 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|      9 | 2317 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2318 | `	/* Extract the flags if avaialble */` |
|      9 | 2319 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      7 | 2320 | `		rc = ph7_value_to_int(apArg[2]);` |
|      7 | 2321 | `		if( rc & 2 /*FNM_NOESCAPE (php value)*/){` |
|    ! 0 | 2322 | `			iEsc = 0;` |
|    ! 0 | 2323 | `		}` |
|      7 | 2324 | `		if( rc & 16 /*FNM_CASEFOLD (php value)*/){` |
|      3 | 2325 | `			noCase = 1;` |
|      1 | 2326 | `		}` |
|      3 | 2327 | `	}` |
|      - | 2328 | `	/* Go globbing */` |
|      9 | 2329 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,noCase);` |
|      - | 2330 | `	/* Globbing result */` |
|      9 | 2331 | `	ph7_result_bool(pCtx,rc);` |
|      9 | 2332 | `	return PH7_OK;` |
|      5 | 2333 | `}` |
|      - | 2334 | `/*` |
|      - | 2335 | ` * bool strglob(string $pattern,string $string)` |
|      - | 2336 | ` *  Match string against a pattern.` |
|      - | 2337 | ` * Parameters` |
|      - | 2338 | ` *  $pattern` |
|      - | 2339 | ` *   The shell wildcard pattern.` |
|      - | 2340 | ` * $string` |
|      - | 2341 | ` *  The tested string.` |
|      - | 2342 | ` * Return` |
|      - | 2343 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 2344 | ` * Note that this a symisc eXtension.` |
|      - | 2345 | ` */` |
|    154 | 2346 | `static int PH7_builtin_strglob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2347 | `{` |
|      - | 2348 | `	const char *zString,*zPattern;` |
|    155 | 2349 | `	int iEsc = '\\';` |
|      - | 2350 | `	int rc;` |
|    155 | 2351 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2352 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2353 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2354 | `		return PH7_OK;` |
|      - | 2355 | `	}` |
|      - | 2356 | `	/* Extract the pattern and the string */` |
|    155 | 2357 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|    155 | 2358 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2359 | `	/* Go globbing */` |
|    155 | 2360 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0);` |
|      - | 2361 | `	/* Globbing result */` |
|    155 | 2362 | `	ph7_result_bool(pCtx,rc);` |
|    155 | 2363 | `	return PH7_OK;` |
|     78 | 2364 | `}` |
|      - | 2365 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2366 | `/*` |
|      - | 2367 | ` * bool link(string $target,string $link)` |
|      - | 2368 |  |
|      - | 2369 | ` *  Create a hard link.` |
|      - | 2370 | ` * Parameters` |
|      - | 2371 | ` *  $target` |
|      - | 2372 | ` *   Target of the link.` |
|      - | 2373 | ` *  $link` |
|      - | 2374 | ` *   The link name.` |
|      - | 2375 | ` * Return` |
|      - | 2376 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2377 | ` */` |
|      2 | 2378 | `static int PH7_vfs_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2379 | `{` |
|      - | 2380 | `	const char *zTarget,*zLink;` |
|      - | 2381 | `	ph7_vfs *pVfs;` |
|      - | 2382 | `	int rc;` |
|      3 | 2383 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2384 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2385 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2386 | `		return PH7_OK;` |
|      - | 2387 | `	}` |
|      - | 2388 | `	/* Point to the underlying vfs */` |
|      3 | 2389 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2390 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2391 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2392 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2393 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2394 | `			ph7_function_name(pCtx)` |
|      - | 2395 | `			);` |
|    ! 0 | 2396 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2397 | `		return PH7_OK;` |
|      - | 2398 | `	}` |
|      - | 2399 | `	/* Extract the given arguments */` |
|      3 | 2400 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      3 | 2401 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2402 | `	/* Perform the requested operation */` |
|      3 | 2403 | `	rc = pVfs->xLink(zTarget,zLink,0/*Not a symbolic link */);` |
|      - | 2404 | `	/* IO result */` |
|      3 | 2405 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 2406 | `	return PH7_OK;` |
|      2 | 2407 | `}` |
|      - | 2408 | `/*` |
|      - | 2409 | ` * string\|false readlink(string $path)` |
|      - | 2410 | ` *  Returns the target of a symbolic link.` |
|      - | 2411 | ` * Parameters` |
|      - | 2412 | ` *  $path` |
|      - | 2413 | ` *   The symbolic link path.` |
|      - | 2414 | ` * Return` |
|      - | 2415 | ` *  The contents of the link, or FALSE (with a warning) when $path is not a link` |
|      - | 2416 | ` *  or cannot be read -- php's own answer, error text included.` |
|      - | 2417 | ` */` |
|      8 | 2418 | `static int PH7_vfs_readlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2419 | `{` |
|      - | 2420 | `	const char *zPath;` |
|      - | 2421 | `	ph7_vfs *pVfs;` |
|      - | 2422 | `	int rc;` |
|      8 | 2423 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    ! 0 | 2424 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2425 | `		return PH7_OK;` |
|      - | 2426 | `	}` |
|      8 | 2427 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      8 | 2428 | `	if( pVfs == 0 \|\| pVfs->xReadlink == 0 ){` |
|    ! 0 | 2429 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2430 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2431 | `			ph7_function_name(pCtx)` |
|      - | 2432 | `			);` |
|    ! 0 | 2433 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2434 | `		return PH7_OK;` |
|      - | 2435 | `	}` |
|      8 | 2436 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      8 | 2437 | `	rc = pVfs->xReadlink(zPath,pCtx);` |
|      8 | 2438 | `	if( rc != PH7_OK ){` |
|      - | 2439 | `		/* php's wording is the errno text alone -- the engine prefixes the` |
|      - | 2440 | `		 * function name already. */` |
|      6 | 2441 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      4 | 2442 | `			"%s",VfsStrerror(errno));` |
|      4 | 2443 | `		ph7_result_bool(pCtx,0);` |
|      2 | 2444 | `	}` |
|      8 | 2445 | `	return PH7_OK;` |
|      4 | 2446 | `}` |
|      - | 2447 | `/*` |
|      - | 2448 | ` * bool symlink(string $target,string $link)` |
|      - | 2449 | ` *  Creates a symbolic link.` |
|      - | 2450 | ` * Parameters` |
|      - | 2451 | ` *  $target` |
|      - | 2452 | ` *   Target of the link.` |
|      - | 2453 | ` *  $link` |
|      - | 2454 | ` *   The link name.` |
|      - | 2455 | ` * Return` |
|      - | 2456 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2457 | ` */` |
|     10 | 2458 | `static int PH7_vfs_symlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2459 | `{` |
|      - | 2460 | `	const char *zTarget,*zLink;` |
|      - | 2461 | `	ph7_vfs *pVfs;` |
|      - | 2462 | `	int rc;` |
|     11 | 2463 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2464 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2465 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2466 | `		return PH7_OK;` |
|      - | 2467 | `	}` |
|      - | 2468 | `	/* Point to the underlying vfs */` |
|     11 | 2469 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 2470 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2471 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2472 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2473 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2474 | `			ph7_function_name(pCtx)` |
|      - | 2475 | `			);` |
|    ! 0 | 2476 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2477 | `		return PH7_OK;` |
|      - | 2478 | `	}` |
|      - | 2479 | `	/* Extract the given arguments */` |
|     11 | 2480 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|     11 | 2481 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2482 | `	/* Perform the requested operation */` |
|     11 | 2483 | `	rc = pVfs->xLink(zTarget,zLink,1/*A symbolic link */);` |
|      - | 2484 | `	/* IO result */` |
|     11 | 2485 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|     11 | 2486 | `	return PH7_OK;` |
|      6 | 2487 | `}` |
|      - | 2488 | `/*` |
|      - | 2489 | ` * int umask([ int $mask ])` |
|      - | 2490 | ` *  Changes the current umask.` |
|      - | 2491 | ` * Parameters` |
|      - | 2492 | ` *  $mask` |
|      - | 2493 | ` *   The new umask.` |
|      - | 2494 | ` * Return` |
|      - | 2495 | ` *  umask() without arguments simply returns the current umask.` |
|      - | 2496 | ` *  Otherwise the old umask is returned.` |
|      - | 2497 | ` */` |
|      8 | 2498 | `static int PH7_vfs_umask(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2499 | `{` |
|      - | 2500 | `	int iOld,iNew;` |
|      - | 2501 | `	ph7_vfs *pVfs;` |
|      - | 2502 | `	/* Point to the underlying vfs */` |
|      9 | 2503 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 | 2504 | `	if( pVfs == 0 \|\| pVfs->xUmask == 0 ){` |
|      - | 2505 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2506 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2507 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2508 | `			ph7_function_name(pCtx)` |
|      - | 2509 | `			);` |
|    ! 0 | 2510 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2511 | `		return PH7_OK;` |
|      - | 2512 | `	}` |
|      9 | 2513 | `	iNew = 0;` |
|      9 | 2514 | `	if( nArg > 0 ){` |
|      5 | 2515 | `		iNew = ph7_value_to_int(apArg[0]);` |
|      2 | 2516 | `	}` |
|      - | 2517 | `	/* Perform the requested operation */` |
|      9 | 2518 | `	iOld = pVfs->xUmask(iNew);` |
|      - | 2519 | `	/* Old mask */` |
|      9 | 2520 | `	ph7_result_int(pCtx,iOld);` |
|      9 | 2521 | `	return PH7_OK;` |
|      5 | 2522 | `}` |
|      - | 2523 | `/*` |
|      - | 2524 | ` * string sys_get_temp_dir()` |
|      - | 2525 | ` *  Returns directory path used for temporary files.` |
|      - | 2526 | ` * Parameters` |
|      - | 2527 | ` *  None` |
|      - | 2528 | ` * Return` |
|      - | 2529 | ` *  Returns the path of the temporary directory.` |
|      - | 2530 | ` */` |
|    266 | 2531 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2532 | `{` |
|      - | 2533 | `	ph7_vfs *pVfs;` |
|      - | 2534 | `	/* Set the empty string as the default return value */` |
|    270 | 2535 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2536 | `	/* Point to the underlying vfs */` |
|    270 | 2537 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    270 | 2538 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
|    ! 0 | 2539 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2540 | `		SXUNUSED(apArg);` |
|      - | 2541 | `		/* IO routine not implemented,return "" */` |
|    ! 0 | 2542 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2543 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2544 | `			ph7_function_name(pCtx)` |
|      - | 2545 | `			);` |
|    ! 0 | 2546 | `		return PH7_OK;` |
|      - | 2547 | `	}` |
|      - | 2548 | `	/* Perform the requested operation */` |
|    270 | 2549 | `	pVfs->xTempDir(pCtx);` |
|    270 | 2550 | `	return PH7_OK;` |
|    137 | 2551 | `}` |
|      - | 2552 | `/*` |
|      - | 2553 | ` * string get_current_user()` |
|      - | 2554 | ` *  Returns the name of the current working user.` |
|      - | 2555 | ` * Parameters` |
|      - | 2556 | ` *  None` |
|      - | 2557 | ` * Return` |
|      - | 2558 | ` *  Returns the name of the current working user.` |
|      - | 2559 | ` */` |
|      2 | 2560 | `static int PH7_vfs_get_current_user(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2561 | `{` |
|      - | 2562 | `	ph7_vfs *pVfs;` |
|      - | 2563 | `	/* Point to the underlying vfs */` |
|      3 | 2564 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2565 | `	if( pVfs == 0 \|\| pVfs->xUsername == 0 ){` |
|    ! 0 | 2566 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2567 | `		SXUNUSED(apArg);` |
|      - | 2568 | `		/* IO routine not implemented */` |
|    ! 0 | 2569 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2570 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2571 | `			ph7_function_name(pCtx)` |
|      - | 2572 | `			);` |
|      - | 2573 | `		/* Set a dummy username */` |
|    ! 0 | 2574 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 2575 | `		return PH7_OK;` |
|      - | 2576 | `	}` |
|      - | 2577 | `	/* Perform the requested operation */` |
|      3 | 2578 | `	pVfs->xUsername(pCtx);` |
|      3 | 2579 | `	return PH7_OK;` |
|      2 | 2580 | `}` |
|      - | 2581 | `/*` |
|      - | 2582 | ` * int64 getmypid()` |
|      - | 2583 | ` *  Gets process ID.` |
|      - | 2584 | ` * Parameters` |
|      - | 2585 | ` *  None` |
|      - | 2586 | ` * Return` |
|      - | 2587 | ` *  Returns the process ID.` |
|      - | 2588 | ` */` |
|     98 | 2589 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2590 | `{` |
|      - | 2591 | `	ph7_int64 nProcessId;` |
|      - | 2592 | `	ph7_vfs *pVfs;` |
|      - | 2593 | `	/* Point to the underlying vfs */` |
|    101 | 2594 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    101 | 2595 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
|    ! 0 | 2596 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2597 | `		SXUNUSED(apArg);` |
|      - | 2598 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2599 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2600 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2601 | `			ph7_function_name(pCtx)` |
|      - | 2602 | `			);` |
|    ! 0 | 2603 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2604 | `		return PH7_OK;` |
|      - | 2605 | `	}` |
|      - | 2606 | `	/* Perform the requested operation */` |
|    101 | 2607 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 2608 | `	/* Set the result */` |
|    101 | 2609 | `	ph7_result_int64(pCtx,nProcessId);` |
|    101 | 2610 | `	return PH7_OK;` |
|     52 | 2611 | `}` |
|      - | 2612 | `/*` |
|      - | 2613 | ` * int getmyuid()` |
|      - | 2614 | ` *  Get user ID.` |
|      - | 2615 | ` * Parameters` |
|      - | 2616 | ` *  None` |
|      - | 2617 | ` * Return` |
|      - | 2618 | ` *  Returns the user ID.` |
|      - | 2619 | ` */` |
|      4 | 2620 | `static int PH7_vfs_getmyuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2621 | `{` |
|      - | 2622 | `	ph7_vfs *pVfs;` |
|      - | 2623 | `	int nUid;` |
|      - | 2624 | `	/* Point to the underlying vfs */` |
|      5 | 2625 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 2626 | `	if( pVfs == 0 \|\| pVfs->xUid == 0 ){` |
|    ! 0 | 2627 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2628 | `		SXUNUSED(apArg);` |
|      - | 2629 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2630 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2631 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2632 | `			ph7_function_name(pCtx)` |
|      - | 2633 | `			);` |
|    ! 0 | 2634 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2635 | `		return PH7_OK;` |
|      - | 2636 | `	}` |
|      - | 2637 | `	/* Perform the requested operation */` |
|      5 | 2638 | `	nUid = pVfs->xUid();` |
|      - | 2639 | `	/* Set the result */` |
|      5 | 2640 | `	ph7_result_int(pCtx,nUid);` |
|      5 | 2641 | `	return PH7_OK;` |
|      3 | 2642 | `}` |
|      - | 2643 | `/*` |
|      - | 2644 | ` * int getmygid()` |
|      - | 2645 | ` *  Get group ID.` |
|      - | 2646 | ` * Parameters` |
|      - | 2647 | ` *  None` |
|      - | 2648 | ` * Return` |
|      - | 2649 | ` *  Returns the group ID.` |
|      - | 2650 | ` */` |
|      2 | 2651 | `static int PH7_vfs_getmygid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2652 | `{` |
|      - | 2653 | `	ph7_vfs *pVfs;` |
|      - | 2654 | `	int nGid;` |
|      - | 2655 | `	/* Point to the underlying vfs */` |
|      3 | 2656 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2657 | `	if( pVfs == 0 \|\| pVfs->xGid == 0 ){` |
|    ! 0 | 2658 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2659 | `		SXUNUSED(apArg);` |
|      - | 2660 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2661 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2662 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2663 | `			ph7_function_name(pCtx)` |
|      - | 2664 | `			);` |
|    ! 0 | 2665 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2666 | `		return PH7_OK;` |
|      - | 2667 | `	}` |
|      - | 2668 | `	/* Perform the requested operation */` |
|      3 | 2669 | `	nGid = pVfs->xGid();` |
|      - | 2670 | `	/* Set the result */` |
|      3 | 2671 | `	ph7_result_int(pCtx,nGid);` |
|      3 | 2672 | `	return PH7_OK;` |
|      2 | 2673 | `}` |
|      - | 2674 | `#ifdef __WINNT__` |
|      - | 2675 | `#include <Windows.h>` |
|      - | 2676 | `#elif defined(__UNIXES__)` |
|      - | 2677 | `#include <sys/utsname.h>` |
|      - | 2678 | `#endif` |
|      - | 2679 | `/*` |
|      - | 2680 | ` * string php_uname([ string $mode = "a" ])` |
|      - | 2681 | ` *  Returns information about the host operating system.` |
|      - | 2682 | ` * Parameters` |
|      - | 2683 | ` *  $mode` |
|      - | 2684 | ` *   mode is a single character that defines what information is returned:` |
|      - | 2685 | ` *    'a': This is the default. Contains all modes in the sequence "s n r v m".` |
|      - | 2686 | ` *    's': Operating system name. eg. FreeBSD.` |
|      - | 2687 | ` *    'n': Host name. eg. localhost.example.com.` |
|      - | 2688 | ` *    'r': Release name. eg. 5.1.2-RELEASE.` |
|      - | 2689 | ` *    'v': Version information. Varies a lot between operating systems.` |
|      - | 2690 | ` *    'm': Machine type. eg. i386.` |
|      - | 2691 | ` * Return` |
|      - | 2692 | ` *  OS description as a string.` |
|      - | 2693 | ` */` |
|      4 | 2694 | `static int PH7_vfs_ph7_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2695 | `{` |
|      - | 2696 | `#if defined(__WINNT__)` |
|      1 | 2697 | `	const char *zName = "Microsoft Windows";` |
|      - | 2698 | `	OSVERSIONINFOW sVer;` |
|      - | 2699 | `#elif defined(__UNIXES__)` |
|      - | 2700 | `	struct utsname sName;` |
|      - | 2701 | `#endif` |
|      5 | 2702 | `	const char *zMode = "a";` |
|      5 | 2703 | `	if( nArg > 0 && ph7_value_is_string(apArg[0]) ){` |
|      - | 2704 | `		/* Extract the desired mode */` |
|    ! 0 | 2705 | `		zMode = ph7_value_to_string(apArg[0],0);` |
|    ! 0 | 2706 | `	}` |
|      - | 2707 | `#if defined(__WINNT__)` |
|      1 | 2708 | `	sVer.dwOSVersionInfoSize = sizeof(sVer);` |
|      - | 2709 | `	/* GetVersionExW is deprecated in modern MSVC. Suppress deprecation for this call. */` |
|      - | 2710 | `#if defined(_MSC_VER)` |
|      - | 2711 | `#pragma warning(push)` |
|      - | 2712 | `#pragma warning(disable:4996)` |
|      - | 2713 | `#endif` |
|      1 | 2714 | `	if( TRUE != GetVersionExW(&sVer)){` |
|      - | 2715 | `#if defined(_MSC_VER)` |
|      - | 2716 | `#pragma warning(pop)` |
|      - | 2717 | `#endif` |
|    ! 0 | 2718 | `		ph7_result_string(pCtx,zName,-1);` |
|    ! 0 | 2719 | `		return PH7_OK;` |
|      - | 2720 | `	}` |
|      1 | 2721 | `	if( sVer.dwPlatformId == VER_PLATFORM_WIN32_NT ){` |
|      1 | 2722 | `		if( sVer.dwMajorVersion <= 4 ){` |
|    ! 0 | 2723 | `			zName = "Microsoft Windows NT";` |
|      1 | 2724 | `		}else if( sVer.dwMajorVersion == 5 ){` |
|    ! 0 | 2725 | `			switch(sVer.dwMinorVersion){` |
|    ! 0 | 2726 | `				case 0:	zName = "Microsoft Windows 2000"; break;` |
|    ! 0 | 2727 | `				case 1: zName = "Microsoft Windows XP";   break;` |
|    ! 0 | 2728 | `				case 2: zName = "Microsoft Windows Server 2003"; break;` |
|      - | 2729 | `			}` |
|    ! 0 | 2730 | `		}else if( sVer.dwMajorVersion == 6){` |
|      1 | 2731 | `				switch(sVer.dwMinorVersion){` |
|    ! 0 | 2732 | `					case 0: zName = "Microsoft Windows Vista"; break;` |
|    ! 0 | 2733 | `					case 1: zName = "Microsoft Windows 7"; break;` |
|      1 | 2734 | `					case 2: zName = "Microsoft Windows Server 2008"; break;` |
|    ! 0 | 2735 | `					case 3: zName = "Microsoft Windows 8"; break;` |
|      - | 2736 | `					default: break;` |
|      - | 2737 | `				}` |
|      - | 2738 | `		}` |
|      - | 2739 | `	}` |
|      1 | 2740 | `	switch(zMode[0]){` |
|      - | 2741 | `	case 's':` |
|      - | 2742 | `		/* Operating system name */` |
|    ! 0 | 2743 | `		ph7_result_string(pCtx,zName,-1/* Compute length automatically*/);` |
|    ! 0 | 2744 | `		break;` |
|      - | 2745 | `	case 'n':` |
|      - | 2746 | `		/* Host name */` |
|    ! 0 | 2747 | `		ph7_result_string(pCtx,"localhost",(int)sizeof("localhost")-1);` |
|    ! 0 | 2748 | `		break;` |
|      - | 2749 | `	case 'r':` |
|      - | 2750 | `	case 'v':` |
|      - | 2751 | `		/* Version information. */` |
|    ! 0 | 2752 | `		ph7_result_string_format(pCtx,"%u.%u build %u",` |
|      - | 2753 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2754 | `			);` |
|    ! 0 | 2755 | `		break;` |
|      - | 2756 | `	case 'm':` |
|      - | 2757 | `		/* Machine name */` |
|    ! 0 | 2758 | `		ph7_result_string(pCtx,"x86",(int)sizeof("x86")-1);` |
|    ! 0 | 2759 | `		break;` |
|      - | 2760 | `	default:` |
|      1 | 2761 | `		ph7_result_string_format(pCtx,"%s localhost %u.%u build %u x86",` |
|      - | 2762 | `			zName,` |
|      - | 2763 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 2764 | `			);` |
|      - | 2765 | `		break;` |
|      - | 2766 | `	}` |
|      - | 2767 | `#elif defined(__UNIXES__)` |
|      4 | 2768 | `	if( uname(&sName) != 0 ){` |
|    ! 0 | 2769 | `		ph7_result_string(pCtx,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 | 2770 | `		return PH7_OK;` |
|      - | 2771 | `	}` |
|      4 | 2772 | `	switch(zMode[0]){` |
|    ! 0 | 2773 | `	case 's':` |
|      - | 2774 | `		/* Operating system name */` |
|    ! 0 | 2775 | `		ph7_result_string(pCtx,sName.sysname,-1/* Compute length automatically*/);` |
|    ! 0 | 2776 | `		break;` |
|    ! 0 | 2777 | `	case 'n':` |
|      - | 2778 | `		/* Host name */` |
|    ! 0 | 2779 | `		ph7_result_string(pCtx,sName.nodename,-1/* Compute length automatically*/);` |
|    ! 0 | 2780 | `		break;` |
|    ! 0 | 2781 | `	case 'r':` |
|      - | 2782 | `		/* Release information */` |
|    ! 0 | 2783 | `		ph7_result_string(pCtx,sName.release,-1/* Compute length automatically*/);` |
|    ! 0 | 2784 | `		break;` |
|    ! 0 | 2785 | `	case 'v':` |
|      - | 2786 | `		/* Version information. */` |
|    ! 0 | 2787 | `		ph7_result_string(pCtx,sName.version,-1/* Compute length automatically*/);` |
|    ! 0 | 2788 | `		break;` |
|    ! 0 | 2789 | `	case 'm':` |
|      - | 2790 | `		/* Machine name */` |
|    ! 0 | 2791 | `		ph7_result_string(pCtx,sName.machine,-1/* Compute length automatically*/);` |
|    ! 0 | 2792 | `		break;` |
|      2 | 2793 | `	default:` |
|      6 | 2794 | `		ph7_result_string_format(pCtx,` |
|      - | 2795 | `			"%s %s %s %s %s",` |
|      2 | 2796 | `			sName.sysname,` |
|      2 | 2797 | `			sName.release,` |
|      2 | 2798 | `			sName.version,` |
|      2 | 2799 | `			sName.nodename,` |
|      2 | 2800 | `			sName.machine` |
|      - | 2801 | `			);` |
|      4 | 2802 | `		break;` |
|      - | 2803 | `	}` |
|      - | 2804 | `#else` |
|      - | 2805 | `	ph7_result_string(pCtx,"Unknown Operating System",(int)sizeof("Unknown Operating System")-1);` |
|      - | 2806 | `#endif` |
|      5 | 2807 | `	return PH7_OK;` |
|      3 | 2808 | `}` |
|      - | 2809 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 2810 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 2811 | `#if defined(_MSC_VER)` |
|      - | 2812 | `static const ph7_vfs null_vfs = {` |
|      - | 2813 | `#else` |
|      - | 2814 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 2815 | `#endif` |
|      - | 2816 | `	"null_vfs",` |
|      - | 2817 | `	PH7_VFS_VERSION,` |
|      - | 2818 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 2819 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 2820 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 2821 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 2822 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 2823 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 2824 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 2825 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 2826 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 2827 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 2828 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 2829 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 2830 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 2831 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 2832 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 2833 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 2834 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 2835 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 2836 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 2837 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 2838 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 2839 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 2840 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 2841 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 2842 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 2843 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 2844 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 2845 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 2846 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 2847 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 2848 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 2849 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 2850 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 2851 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 2852 | `	0, /* int (*xUmask)(int) */` |
|      - | 2853 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 2854 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 2855 | `	0, /* int (*xUid)(void) */` |
|      - | 2856 | `	0, /* int (*xGid)(void) */` |
|      - | 2857 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 2858 | `	0, /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 2859 | `	0  /* int (*xReadlink)(const char *,ph7_context *) */` |
|      - | 2860 | `};` |
|      - | 2861 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 2862 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 2863 | `/*` |
|      - | 2864 | ` * Export the builtin vfs.` |
|      - | 2865 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 2866 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 2867 | ` * Note:` |
|      - | 2868 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 2869 | ` * Note:` |
|      - | 2870 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2871 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 2872 | ` */` |
|   4672 | 2873 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 2874 | `{` |
|      - | 2875 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 2876 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 2877 | `	return &null_vfs;` |
|      - | 2878 | `#else` |
|      - | 2879 | `#ifdef __WINNT__` |
|      5 | 2880 | `	return &sWinVfs;` |
|      - | 2881 | `#elif defined(__UNIXES__)` |
|   4672 | 2882 | `	return &sUnixVfs;` |
|      - | 2883 | `#else` |
|      - | 2884 | `	return &null_vfs;` |
|      - | 2885 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 2886 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 2887 | `#else` |
|      - | 2888 | `	return &null_vfs;` |
|      - | 2889 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 2890 | `}` |
|      - | 2891 | `/*` |
|      - | 2892 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 2893 | ` * [i.e: file://,php://].` |
|      - | 2894 | ` * Note:` |
|      - | 2895 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 2896 | ` *  defined then this function is a no-op.` |
|      - | 2897 | ` */` |
|   4076 | 2898 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 2899 | `{` |
|      - | 2900 | `	/*` |
|      - | 2901 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 2902 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 2903 | `	 */` |
|      - | 2904 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 2905 | `	/* VFS: disk I/O related functions */` |
|      - | 2906 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 2907 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 2908 | `#ifndef __WINNT__` |
|      - | 2909 | `		/* php declares chroot() on POSIX only — there is no such call on Windows,` |
|      - | 2910 | ``		 * so `function_exists('chroot')` is FALSE there and the name is free for a`` |
|      - | 2911 | `		 * script to define. PHL used to declare it on both and answer a` |
|      - | 2912 | `		 * "not implemented in the underlying VFS" warning + false on Windows,` |
|      - | 2913 | `		 * which is a different thing from php's undefined function. (chown/chgrp/` |
|      - | 2914 | `		 * link/symlink/readlink stay: php declares all five on Windows.) */` |
|      - | 2915 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 2916 | `#endif` |
|      - | 2917 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 2918 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 2919 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 2920 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 2921 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 2922 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 2923 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 2924 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 2925 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 2926 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 2927 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 2928 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 2929 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 2930 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 2931 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 2932 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 2933 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 2934 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 2935 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 2936 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 2937 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 2938 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 2939 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 2940 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 2941 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 2942 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 2943 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 2944 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 2945 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 2946 | `		{"fileowner",   PH7_vfs_file_owner},` |
|      - | 2947 | `		{"filegroup",   PH7_vfs_file_group},` |
|      - | 2948 | `		{"fileinode",   PH7_vfs_file_inode},` |
|      - | 2949 | `		{"fileperms",   PH7_vfs_file_perms},` |
|      - | 2950 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 2951 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 2952 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 2953 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 2954 | `		{"link",        PH7_vfs_link     },` |
|      - | 2955 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 2956 | `		{"readlink",    PH7_vfs_readlink },` |
|      - | 2957 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 2958 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 2959 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 2960 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 2961 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 2962 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 2963 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 2964 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 2965 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 2966 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 2967 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 2968 | `	};` |
|      - | 2969 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 2970 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 2971 | `	 */` |
|      - | 2972 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 2973 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 2974 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 2975 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 2976 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 2977 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 2978 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 2979 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 2980 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 2981 | `		{"stream_get_line", PH7_builtin_stream_get_line },` |
|      - | 2982 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 2983 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 2984 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 2985 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 2986 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 2987 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 2988 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 2989 | `		/* php's dir() lives with opendir(), which is what it calls and what its` |
|      - | 2990 | `		 * failure warning is worded by. Registering it here also means the TINY` |
|      - | 2991 | `		 * build drops BOTH: the prelude copy was defined there and fataled on` |
|      - | 2992 | `		 * "Call to undefined function opendir()" the moment it was called. */` |
|      - | 2993 | `		{"dir",       PH7_builtin_dir },` |
|      - | 2994 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 2995 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 2996 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 2997 | `		{"file",      PH7_builtin_file   },` |
|      - | 2998 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 2999 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 3000 | `		{"stream_isatty", PH7_builtin_stream_isatty },` |
|      - | 3001 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 3002 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 3003 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 3004 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 3005 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 3006 | `		{"stream_get_contents",  PH7_builtin_stream_get_contents },` |
|      - | 3007 | `		{"stream_get_wrappers",  PH7_builtin_stream_get_wrappers },` |
|      - | 3008 | `		{"stream_get_meta_data", PH7_builtin_stream_get_meta_data },` |
|      - | 3009 | `		{"stream_context_create",PH7_builtin_stream_context_create },` |
|      - | 3010 | `		{"stream_wrapper_register",   PH7_builtin_stream_wrapper_register },` |
|      - | 3011 | `		{"stream_register_wrapper",   PH7_builtin_stream_wrapper_register },` |
|      - | 3012 | `		{"stream_wrapper_unregister", PH7_builtin_stream_wrapper_unregister },` |
|      - | 3013 | `#ifdef PH7_ENABLE_NET` |
|      - | 3014 | `		{"fsockopen",  PH7_builtin_fsockopen },` |
|      - | 3015 | `		{"pfsockopen", PH7_builtin_fsockopen },` |
|      - | 3016 | `		{"stream_socket_client", PH7_builtin_fsockopen },` |
|      - | 3017 | `#endif` |
|      - | 3018 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 3019 | `		{"proc_open",      PH7_builtin_proc_open      },` |
|      - | 3020 | `		{"proc_close",     PH7_builtin_proc_close     },` |
|      - | 3021 | `		{"proc_terminate", PH7_builtin_proc_terminate },` |
|      - | 3022 | `		{"proc_get_status",PH7_builtin_proc_get_status},` |
|      - | 3023 | `		{"proc_nice",      PH7_builtin_proc_nice      },` |
|      - | 3024 | `		{"shell_exec", PH7_builtin_shell_exec },` |
|      - | 3025 | `		{"exec",       PH7_builtin_exec     },` |
|      - | 3026 | `		{"system",     PH7_builtin_system   },` |
|      - | 3027 | `		{"passthru",   PH7_builtin_passthru },` |
|      - | 3028 | `		/* The shell-escaping pair lives with the command runners it exists to` |
|      - | 3029 | `		 * feed: a build without process execution has nothing to escape for. */` |
|      - | 3030 | `		{"escapeshellarg", PH7_builtin_escapeshellarg },` |
|      - | 3031 | `		{"escapeshellcmd", PH7_builtin_escapeshellcmd },` |
|      - | 3032 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 3033 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 3034 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 3035 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 3036 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 3037 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 3038 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 3039 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 3040 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 3041 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 3042 | `	};` |
|   4081 | 3043 | `	const ph7_io_stream *pFileStream = 0;` |
|   4081 | 3044 | `	sxu32 n = 0;` |
|      - | 3045 | `	/* Register disk-related functions */` |
| 220109 | 3046 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 216033 | 3047 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
| 108019 | 3048 | `	}` |
| 244565 | 3049 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 240489 | 3050 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
| 120247 | 3051 | `	}` |
|      - | 3052 | `#else` |
|      - | 3053 | `	SXUNUSED(pVm);` |
|      - | 3054 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 3055 |  |
|      - | 3056 | `	/*` |
|      - | 3057 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 3058 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 3059 | `	 */` |
|      - | 3060 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 3061 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 3062 | `		/* Path processing */` |
|      - | 3063 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 3064 | `		{"basename",    PH7_builtin_basename },` |
|      - | 3065 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 3066 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 3067 | `		{"fnmatch",     PH7_builtin_fnmatch  }` |
|      - | 3068 | `	};` |
|  24461 | 3069 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  20385 | 3070 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|  10195 | 3071 | `	}` |
|      - | 3072 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 3073 |  |
|      - | 3074 | `	/* Install streams if disk I/O is enabled */` |
|      - | 3075 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 3076 | `#ifdef __WINNT__` |
|      5 | 3077 | `	pFileStream = &sWinFileStream;` |
|      - | 3078 | `#elif defined(__UNIXES__)` |
|   4076 | 3079 | `	pFileStream = &sUnixFileStream;` |
|      - | 3080 | `#endif` |
|      - | 3081 | `	/* Install the php:// stream */` |
|   4081 | 3082 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   4081 | 3083 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);` |
|      - | 3084 | `#ifdef PH7_ENABLE_NET` |
|   4081 | 3085 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);` |
|      - | 3086 | `#endif` |
|   4081 | 3087 | `	if( pFileStream ){` |
|      - | 3088 | `		/* Install the file:// stream */` |
|   4081 | 3089 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   2038 | 3090 | `	}` |
|      - | 3091 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 3092 |  |
|   4081 | 3093 | `	return SXRET_OK;` |
|      5 | 3094 | `}` |
|      - | 3095 |  |
