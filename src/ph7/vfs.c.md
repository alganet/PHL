# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1060/1462 lines (72.50%)

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
|  16452 |   25 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)` |
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
|  16452 |   37 | `	const char *zRoot = "/";` |
|      - |   38 | `#endif` |
|  16457 |   39 | `	c = d = '/';` |
|      - |   40 | `#ifdef __WINNT__` |
|      5 |   41 | `	d = '\\';` |
|      - |   42 | `#endif` |
|      - |   43 | `#define DIR_IS_SEP(x) ( (int)(x) == c \|\| (int)(x) == d )` |
|  16457 |   44 | `	if( nByte < 1 ){` |
|      - |   45 | `		/* php returns the empty string for the empty path */` |
|      5 |   46 | `		*pLen = 0;` |
|      5 |   47 | `		return "";` |
|      - |   48 | `	}` |
|  16453 |   49 | `	iEnd = nByte;` |
|  24705 |   50 | `	while( iEnd > 0 && DIR_IS_SEP(zPath[iEnd - 1]) ){` |
|     29 |   51 | `		iEnd--;` |
|      1 |   52 | `	}` |
|  16453 |   53 | `	if( iEnd == 0 ){` |
|      - |   54 | `		/* The path is nothing but separators: the root is its own parent */` |
|     17 |   55 | `		*pLen = (int)sizeof(char);` |
|     17 |   56 | `		return zRoot;` |
|      - |   57 | `	}` |
|      - |   58 | `	/* Walk back to the separator that ends the parent directory */` |
|  16437 |   59 | `	i = iEnd;` |
| 444791 |   60 | `	while( i > 0 && !DIR_IS_SEP(zPath[i - 1]) ){` |
| 428359 |   61 | `		i--;` |
|      5 |   62 | `	}` |
|  16437 |   63 | `	if( i == 0 ){` |
|      - |   64 | `		/* No separator at all,return "." as the current directory */` |
|     70 |   65 | `		*pLen = (int)sizeof(char);` |
|     70 |   66 | `		return ".";` |
|      - |   67 | `	}` |
|      - |   68 | `	/* Drop the separator, plus any that repeat before it */` |
|  40905 |   69 | `	while( i > 1 && DIR_IS_SEP(zPath[i - 1]) ){` |
|  16359 |   70 | `		i--;` |
|      5 |   71 | `	}` |
|  16369 |   72 | `	if( i == 1 && DIR_IS_SEP(zPath[0]) ){` |
|     13 |   73 | `		*pLen = (int)sizeof(char);` |
|     13 |   74 | `		return zRoot;` |
|      - |   75 | `	}` |
|  16357 |   76 | `	*pLen = i;` |
|  16357 |   77 | `	return zPath;` |
|      - |   78 | `#undef DIR_IS_SEP` |
|   8231 |   79 | `}` |
|      - |   80 | `/*` |
|      - |   81 | ` * php_basename: drop any trailing separators, then answer what follows the last` |
|      - |   82 | ` * remaining one. Shared by basename() and pathinfo() — they used to hand-roll the` |
|      - |   83 | ` * same walk separately, and pathinfo()'s copy kept the trailing separator run` |
|      - |   84 | `` * (`pathinfo("/var/www/")` answered basename "" where php answers "www").`` |
|      - |   85 | ` */` |
|  16260 |   86 | `PH7_PRIVATE const char * PH7_ExtractBaseName(const char *zPath,int nByte,int *pLen)` |
|      5 |   87 | `{` |
|      - |   88 | `	int c,d,iEnd,i;` |
|  16265 |   89 | `	c = d = '/';` |
|      - |   90 | `#ifdef __WINNT__` |
|      5 |   91 | `	d = '\\';` |
|      - |   92 | `#endif` |
|      - |   93 | `#define DIR_IS_SEP(x) ( (int)(x) == c \|\| (int)(x) == d )` |
|  16265 |   94 | `	iEnd = nByte;` |
|  24420 |   95 | `	while( iEnd > 0 && DIR_IS_SEP(zPath[iEnd - 1]) ){` |
|     27 |   96 | `		iEnd--;` |
|      1 |   97 | `	}` |
|  16265 |   98 | `	if( iEnd < 1 ){` |
|      - |   99 | `		/* Empty, or nothing but separators: php answers the empty string */` |
|     29 |  100 | `		*pLen = 0;` |
|     29 |  101 | `		return "";` |
|      - |  102 | `	}` |
|  16237 |  103 | `	i = iEnd;` |
| 439957 |  104 | `	while( i > 0 && !DIR_IS_SEP(zPath[i - 1]) ){` |
| 423725 |  105 | `		i--;` |
|      5 |  106 | `	}` |
|  16237 |  107 | `	*pLen = iEnd - i;` |
|  16237 |  108 | `	return &zPath[i];` |
|      - |  109 | `#undef DIR_IS_SEP` |
|   8135 |  110 | `}` |
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
|  25018 |  122 | `PH7_PRIVATE const char * VfsStrerror(int iErr)` |
|      5 |  123 | `{` |
|      - |  124 | `#if defined(_MSC_VER)` |
|      - |  125 | `#pragma warning(push)` |
|      - |  126 | `#pragma warning(disable:4996)` |
|      - |  127 | `#endif` |
|  25023 |  128 | `	return strerror(iErr);` |
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
|  24892 |  139 | `static void VfsThrowSysWarning(ph7_context *pCtx,const char *zPath)` |
|      5 |  140 | `{` |
|  37343 |  141 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s",` |
|  24892 |  142 | `		ph7_function_name(pCtx),zPath ? zPath : "",VfsStrerror(errno));` |
|  24897 |  143 | `}` |
|     54 |  144 | `PH7_PRIVATE void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile)` |
|      5 |  145 | `{` |
|     86 |  146 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): Failed to open stream: %s",` |
|     54 |  147 | `		ph7_function_name(pCtx),zFile ? zFile : "",VfsStrerror(errno));` |
|     59 |  148 | `}` |
|      - |  149 | `/*` |
|      - |  150 | ` * php's answer when NO wrapper will take a name is a reason of its own, raised` |
|      - |  151 | ` * before the operation's own failure and naming the scheme the script wrote:` |
|      - |  152 | ` *` |
|      - |  153 | ` *   file_get_contents(): Unable to find the wrapper "zzz" - did you forget to` |
|      - |  154 | ` *   enable it when you configured PHP?` |
|      - |  155 | ` *` |
|      - |  156 | ` * PHL raised one PH7-specific sentence -- "No such stream device,PH7 is` |
|      - |  157 | ` * returning FALSE" -- which names neither the function's argument nor what was` |
|      - |  158 | ` * wrong with it, and two more call sites had a third wording of their own.` |
|      - |  159 | ` */` |
|     24 |  160 | `PH7_PRIVATE void VfsThrowNoDeviceWarning(ph7_context *pCtx,const char *zUri,int bDir)` |
|      2 |  161 | `{` |
|     26 |  162 | `	const char *zFunc = ph7_function_name(pCtx);` |
|     26 |  163 | `	const char *zWhat = bDir ? "directory" : "stream";` |
|     26 |  164 | `	int nScheme = 0;` |
|     26 |  165 | `	if( zUri == 0 ){` |
|    ! 0 |  166 | `		zUri = "";` |
|    ! 0 |  167 | `	}` |
|     26 |  168 | `	if( PH7_VmStreamDeviceIsRemoteHost(zUri,-1,&nScheme) ){` |
|      - |  169 | `		/* A wrapper WAS found for the scheme and refused the name, which php` |
|      - |  170 | `		 * words differently from a scheme nothing is registered under. */` |
|     19 |  171 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Remote host file access not supported, %s",` |
|      6 |  172 | `			zFunc,zUri);` |
|     19 |  173 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      6 |  174 | `			"%s(%s): Failed to open %s: no suitable wrapper could be found",zFunc,zUri,zWhat);` |
|     14 |  175 | `		return;` |
|      - |  176 | `	}` |
|     14 |  177 | `	if( nScheme > 0 ){` |
|     17 |  178 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - |  179 | `			"%s(): Unable to find the wrapper \"%.*s\" - did you forget to enable it when you configured PHP?",` |
|      5 |  180 | `			zFunc,nScheme,zUri);` |
|      5 |  181 | `	}` |
|      - |  182 | `	/* A name with no scheme is the plain-files wrapper's, and that is the ONE` |
|      - |  183 | `	 * php reports as switched off rather than missing: its fallback branch runs` |
|      - |  184 | ``	 * after the hash lookup, so an explicit `file://` gets both sentences and a`` |
|      - |  185 | `	 * bare path only the second. Every other unregistered wrapper is` |
|      - |  186 | `	 * indistinguishable from one that never existed, and php words it that way. */` |
|     12 |  187 | `	if( (nScheme == 0` |
|     11 |  188 | `	  \|\| (nScheme == (int)sizeof("file")-1 && SyStrnicmp(zUri,"file",sizeof("file")-1) == 0))` |
|      5 |  189 | `	 && PH7_VmStreamSchemeDisabled(pCtx->pVm,"file",(int)sizeof("file")-1) ){` |
|      4 |  190 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      1 |  191 | `			"%s(): file:// wrapper is disabled in the server configuration",zFunc);` |
|      4 |  192 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      1 |  193 | `			"%s(%s): Failed to open %s: no suitable wrapper could be found",zFunc,zUri,zWhat);` |
|      3 |  194 | `		return;` |
|      - |  195 | `	}` |
|     17 |  196 | `	PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      5 |  197 | `		"%s(%s): Failed to open %s: No such file or directory",zFunc,zUri,zWhat);` |
|     14 |  198 | `}` |
|      - |  199 | `/*` |
|      - |  200 | `` * php's stat-failure warning: `filemtime(): stat failed for /nope`, and`` |
|      - |  201 | `` * `filetype(): Lstat failed for /nope` for the two members that LSTAT. php raises`` |
|      - |  202 | ` * it from php_stat() for the whole family and answers FALSE; PHL answered the` |
|      - |  203 | ` * VFS's raw -1 (or the string "unknown") for most of them, in silence -- and -1 is` |
|      - |  204 | `` * TRUTHY, so `if (filemtime($f))` took the found branch for a file that is not`` |
|      - |  205 | `` * there and `filemtime($a) > filemtime($b)` compared a real time against it.`` |
|      - |  206 | ` */` |
|     20 |  207 | `static void VfsThrowStatWarning(ph7_context *pCtx,const char *zPath,int bLstat)` |
|      1 |  208 | `{` |
|     31 |  209 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s failed for %s",` |
|     10 |  210 | `		ph7_function_name(pCtx),bLstat ? "Lstat" : "stat",zPath ? zPath : "");` |
|     21 |  211 | `}` |
|      - |  212 | `/*` |
|      - |  213 | ` * php's stat() answer is TWENTY-SIX entries, not thirteen: the same thirteen` |
|      - |  214 | ` * fields once at numeric indices 0..12 and once under their names, in this` |
|      - |  215 | ` * order. The numeric half is what php's own documentation indexes by ($s[7] is` |
|      - |  216 | `` * the size) and it is what a `list()`/destructuring reader takes, so a script`` |
|      - |  217 | `` * written against php read `Undefined array key 7` here and answered NULL.`` |
|      - |  218 | ` *` |
|      - |  219 | ` * The VFS fills the NAMED half (both the unix and Windows implementations use` |
|      - |  220 | ` * exactly these keys), so the doubling is done once, here, rather than in every` |
|      - |  221 | ` * xStat: pOut gets the numeric run first and then the names, which is php's own` |
|      - |  222 | ` * insertion order — visible through foreach, print_r, var_dump and json_encode.` |
|      - |  223 | ` */` |
|     24 |  224 | `PH7_PRIVATE int PH7_VfsStatDoubleUp(ph7_value *pIn,ph7_value *pOut)` |
|      1 |  225 | `{` |
|      - |  226 | `	static const char * const azField[] = {` |
|      - |  227 | `		"dev","ino","mode","nlink","uid","gid","rdev","size",` |
|      - |  228 | `		"atime","mtime","ctime","blksize","blocks"` |
|      - |  229 | `	};` |
|      - |  230 | `	sxu32 i;` |
|     25 |  231 | `	if( pIn == 0 \|\| pOut == 0 ){` |
|    ! 0 |  232 | `		return -1;` |
|      - |  233 | `	}` |
|    337 |  234 | `	for( i = 0 ; i < SX_ARRAYSIZE(azField) ; ++i ){` |
|    313 |  235 | `		ph7_value *pField = ph7_array_fetch(pIn,azField[i],-1);` |
|    313 |  236 | `		if( pField == 0 ){` |
|      - |  237 | `			/* A VFS that does not report this field: php always has all thirteen,` |
|      - |  238 | `			 * so the doubling would silently shift every later index. Hand the` |
|      - |  239 | `			 * caller the named-only array it already had instead. */` |
|    ! 0 |  240 | `			return -1;` |
|      - |  241 | `		}` |
|    313 |  242 | `		ph7_array_add_elem(pOut,0,pField);` |
|    157 |  243 | `	}` |
|    337 |  244 | `	for( i = 0 ; i < SX_ARRAYSIZE(azField) ; ++i ){` |
|    313 |  245 | `		ph7_value *pField = ph7_array_fetch(pIn,azField[i],-1);` |
|    313 |  246 | `		ph7_array_add_strkey_elem(pOut,azField[i],pField);` |
|    157 |  247 | `	}` |
|     25 |  248 | `	return PH7_OK;` |
|     13 |  249 | `}` |
|      - |  250 | `/*` |
|      - |  251 | ` * Can this path be stat'ed at all? The three TIME readers report a failure as -1,` |
|      - |  252 | ` * which is also a legitimate timestamp (a file stamped in the last second before` |
|      - |  253 | ` * the epoch), so the failure verdict is asked of the VFS separately rather than` |
|      - |  254 | ` * read off the value -- one extra call, and only on the negative branch.` |
|      - |  255 | ` */` |
|    106 |  256 | `static int VfsPathStatable(ph7_vfs *pVfs,const char *zPath)` |
|      2 |  257 | `{` |
|    108 |  258 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|    ! 0 |  259 | `		return 0;` |
|      - |  260 | `	}` |
|    108 |  261 | `	return pVfs->xFileExists(zPath) == PH7_OK;` |
|     73 |  262 | `}` |
|      - |  263 | `/*` |
|      - |  264 | ` * bool chdir(string $directory)` |
|      - |  265 | ` *  Change the current directory.` |
|      - |  266 | ` * Parameters` |
|      - |  267 | ` *  $directory` |
|      - |  268 | ` *   The new current directory` |
|      - |  269 | ` * Return` |
|      - |  270 | ` *  TRUE on success or FALSE on failure.` |
|      - |  271 | ` */` |
|  15046 |  272 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  273 | `{` |
|      - |  274 | `	const char *zPath;` |
|      - |  275 | `	ph7_vfs *pVfs;` |
|      - |  276 | `	int rc;` |
|      - |  277 | `	/* Only the ARITY is checked here: php coerces a scalar $directory to string,` |
|      - |  278 | `	 * so chdir(123) attempts "123" and warns that it does not exist. Requiring a` |
|      - |  279 | `	 * string outright made that call return FALSE silently, with no diagnostic. */` |
|  15051 |  280 | `	if( nArg < 1 ){` |
|      - |  281 | `		/* Missing argument,return FALSE */` |
|    ! 0 |  282 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  283 | `		return PH7_OK;` |
|      - |  284 | `	}` |
|      - |  285 | `	/* Point to the underlying vfs */` |
|  15051 |  286 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  15051 |  287 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |  288 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  289 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  290 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  291 | `			ph7_function_name(pCtx)` |
|      - |  292 | `			);` |
|    ! 0 |  293 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  294 | `		return PH7_OK;` |
|      - |  295 | `	}` |
|      - |  296 | `	/* Point to the desired directory */` |
|  15051 |  297 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  298 | `	/* Perform the requested operation */` |
|  15051 |  299 | `	errno = 0;` |
|  15051 |  300 | `	rc = pVfs->xChdir(zPath);` |
|  15051 |  301 | `	if( rc != PH7_OK ){` |
|      - |  302 | `		/* chdir has its own php shape: no path, and the errno spelled out. */` |
|      8 |  303 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|      4 |  304 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|      2 |  305 | `	}` |
|      - |  306 | `	/* IO return value */` |
|  15051 |  307 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  15051 |  308 | `	return PH7_OK;` |
|   7528 |  309 | `}` |
|      - |  310 | `/*` |
|      - |  311 | ` * bool chroot(string $directory)` |
|      - |  312 | ` *  Change the root directory.` |
|      - |  313 | ` * Parameters` |
|      - |  314 | ` *  $directory` |
|      - |  315 | ` *   The path to change the root directory to` |
|      - |  316 | ` * Return` |
|      - |  317 | ` *  TRUE on success or FALSE on failure.` |
|      - |  318 | ` *` |
|      - |  319 | ` * POSIX only, like php's: the registration below is guarded the same way, and an` |
|      - |  320 | ` * unreferenced static is an error under the Windows build's /W4 /WX.` |
|      - |  321 | ` */` |
|      - |  322 | `#ifndef __WINNT__` |
|    ! 0 |  323 | `static int PH7_vfs_chroot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - |  324 | `{` |
|      - |  325 | `	const char *zPath;` |
|      - |  326 | `	ph7_vfs *pVfs;` |
|      - |  327 | `	int rc;` |
|    ! 0 |  328 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  329 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  330 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  331 | `		return PH7_OK;` |
|      - |  332 | `	}` |
|      - |  333 | `	/* Point to the underlying vfs */` |
|    ! 0 |  334 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    ! 0 |  335 | `	if( pVfs == 0 \|\| pVfs->xChroot == 0 ){` |
|      - |  336 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  337 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  338 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  339 | `			ph7_function_name(pCtx)` |
|      - |  340 | `			);` |
|    ! 0 |  341 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  342 | `		return PH7_OK;` |
|      - |  343 | `	}` |
|      - |  344 | `	/* Point to the desired directory */` |
|    ! 0 |  345 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  346 | `	/* Perform the requested operation */` |
|    ! 0 |  347 | `	errno = 0;` |
|    ! 0 |  348 | `	rc = pVfs->xChroot(zPath);` |
|    ! 0 |  349 | `	if( rc != PH7_OK ){` |
|      - |  350 | `		/* php's own wording, and the failure a script actually meets: chroot(2)` |
|      - |  351 | `		 * needs privilege, so an ordinary process gets EPERM. PHL answered the` |
|      - |  352 | `		 * bare false in SILENCE — a refused chroot() and a chroot() that did` |
|      - |  353 | `		 * nothing looked the same to the caller. */` |
|    ! 0 |  354 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|    ! 0 |  355 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|    ! 0 |  356 | `	}` |
|      - |  357 | `	/* IO return value */` |
|    ! 0 |  358 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    ! 0 |  359 | `	return PH7_OK;` |
|    ! 0 |  360 | `}` |
|      - |  361 | `#endif /* __WINNT__ */` |
|      - |  362 | `/*` |
|      - |  363 | ` * string getcwd(void)` |
|      - |  364 | ` *  Gets the current working directory.` |
|      - |  365 | ` * Parameters` |
|      - |  366 | ` *  None` |
|      - |  367 | ` * Return` |
|      - |  368 | ` *  Returns the current working directory on success, or FALSE on failure.` |
|      - |  369 | ` */` |
|     18 |  370 | `static int PH7_vfs_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  371 | `{` |
|      - |  372 | `	ph7_vfs *pVfs;` |
|      - |  373 | `	int rc;` |
|      - |  374 | `	/* Point to the underlying vfs */` |
|     23 |  375 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     23 |  376 | `	if( pVfs == 0 \|\| pVfs->xGetcwd == 0 ){` |
|    ! 0 |  377 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 |  378 | `		SXUNUSED(apArg);` |
|      - |  379 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  380 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  381 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  382 | `			ph7_function_name(pCtx)` |
|      - |  383 | `			);` |
|    ! 0 |  384 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  385 | `		return PH7_OK;` |
|      - |  386 | `	}` |
|     23 |  387 | `	ph7_result_string(pCtx,"",0);` |
|      - |  388 | `	/* Perform the requested operation */` |
|     23 |  389 | `	rc = pVfs->xGetcwd(pCtx);` |
|     23 |  390 | `	if( rc != PH7_OK ){` |
|      - |  391 | `		/* Error,return FALSE */` |
|    ! 0 |  392 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  393 | `	}` |
|     23 |  394 | `	return PH7_OK;` |
|     14 |  395 | `}` |
|      - |  396 | `/*` |
|      - |  397 | ` * bool rmdir(string $directory)` |
|      - |  398 | ` *  Removes directory.` |
|      - |  399 | ` * Parameters` |
|      - |  400 | ` *  $directory` |
|      - |  401 | ` *   The path to the directory` |
|      - |  402 | ` * Return` |
|      - |  403 | ` *  TRUE on success or FALSE on failure.` |
|      - |  404 | ` */` |
|    140 |  405 | `static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  406 | `{` |
|      - |  407 | `	const char *zPath;` |
|      - |  408 | `	ph7_vfs *pVfs;` |
|    145 |  409 | `	int rc,bThrew = 0;` |
|    145 |  410 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  411 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  412 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  413 | `		return PH7_OK;` |
|      - |  414 | `	}` |
|      - |  415 | ``	/* php's `?resource $context`, refused when it is a resource of another kind.`` |
|      - |  416 | `	 * Nothing here CONSUMES it — this operation never opens a stream, and the` |
|      - |  417 | `	 * userland wrapper's unlink/rename/mkdir/rmdir methods are not dispatched` |
|      - |  418 | `	 * (§7.4 slice-2 (e)) — but the refusal is the argument's contract, and it` |
|      - |  419 | `	 * was accepted in silence. */` |
|    145 |  420 | `	PH7_StreamCtxFromArg(pCtx,nArg,apArg,1,"$context",0,&bThrew);` |
|    145 |  421 | `	if( bThrew ){` |
|      3 |  422 | `		return PH7_OK;` |
|      - |  423 | `	}` |
|      - |  424 | `	/* Point to the underlying vfs */` |
|    143 |  425 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    143 |  426 | `	if( pVfs == 0 \|\| pVfs->xRmdir == 0 ){` |
|      - |  427 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  428 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  429 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  430 | `			ph7_function_name(pCtx)` |
|      - |  431 | `			);` |
|    ! 0 |  432 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  433 | `		return PH7_OK;` |
|      - |  434 | `	}` |
|      - |  435 | `	/* Point to the desired directory */` |
|    143 |  436 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  437 | `	/* Perform the requested operation */` |
|    143 |  438 | `	errno = 0;` |
|    143 |  439 | `	rc = pVfs->xRmdir(zPath);` |
|    143 |  440 | `	if( rc != PH7_OK ){` |
|     29 |  441 | `		VfsThrowSysWarning(pCtx,zPath);` |
|     12 |  442 | `	}` |
|      - |  443 | `	/* IO return value */` |
|    143 |  444 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    143 |  445 | `	return PH7_OK;` |
|     75 |  446 | `}` |
|      - |  447 | `/*` |
|      - |  448 | ` * bool is_dir(string $filename)` |
|      - |  449 | ` *  Tells whether the given filename is a directory.` |
|      - |  450 | ` * Parameters` |
|      - |  451 | ` *  $filename` |
|      - |  452 | ` *   Path to the file.` |
|      - |  453 | ` * Return` |
|      - |  454 | ` *  TRUE on success or FALSE on failure.` |
|      - |  455 | ` */` |
|  10506 |  456 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  457 | `{` |
|      - |  458 | `	const char *zPath;` |
|      - |  459 | `	ph7_vfs *pVfs;` |
|      - |  460 | `	int rc;` |
|  10511 |  461 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  462 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  463 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  464 | `		return PH7_OK;` |
|      - |  465 | `	}` |
|      - |  466 | `	/* Point to the underlying vfs */` |
|  10511 |  467 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  10511 |  468 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  469 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  470 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  471 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  472 | `			ph7_function_name(pCtx)` |
|      - |  473 | `			);` |
|    ! 0 |  474 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  475 | `		return PH7_OK;` |
|      - |  476 | `	}` |
|      - |  477 | `	/* Point to the desired directory */` |
|  10511 |  478 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  479 | `	/* Perform the requested operation */` |
|  10511 |  480 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  481 | `	/* IO return value */` |
|  10511 |  482 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  10511 |  483 | `	return PH7_OK;` |
|   5258 |  484 | `}` |
|      - |  485 | `/*` |
|      - |  486 | ` * bool mkdir(string $pathname[,int $mode = 0777 [,bool $recursive = false])` |
|      - |  487 | ` *  Make a directory.` |
|      - |  488 | ` * Parameters` |
|      - |  489 | ` *  $pathname` |
|      - |  490 | ` *   The directory path.` |
|      - |  491 | ` * $mode` |
|      - |  492 | ` *  The mode is 0777 by default, which means the widest possible access.` |
|      - |  493 | ` *  Note:` |
|      - |  494 | ` *   mode is ignored on Windows.` |
|      - |  495 | ` *   Note that you probably want to specify the mode as an octal number, which means` |
|      - |  496 | ` *   it should have a leading zero. The mode is also modified by the current umask` |
|      - |  497 | ` *   which you can change using umask().` |
|      - |  498 | ` * $recursive` |
|      - |  499 | ` *  Allows the creation of nested directories specified in the pathname.` |
|      - |  500 | ` *  Defaults to FALSE. (Not used)` |
|      - |  501 | ` * Return` |
|      - |  502 | ` *  TRUE on success or FALSE on failure.` |
|      - |  503 | ` */` |
|      - |  504 | `/*` |
|      - |  505 | ` * A prefix the recursive mkdir must not try to CREATE: it names a volume rather` |
|      - |  506 | ` * than a directory. POSIX has none of these (the leading "/" is never a prefix` |
|      - |  507 | ` * here, since the walk starts one byte in).` |
|      - |  508 | ` */` |
|    100 |  509 | `static int VfsMkdirVolumePrefix(const char *z,int n)` |
|      1 |  510 | `{` |
|      - |  511 | `#ifdef __WINNT__` |
|      1 |  512 | `	int i,nSep = 0;` |
|      1 |  513 | `	if( n < 1 ){` |
|    ! 0 |  514 | `		return 1;` |
|      - |  515 | `	}` |
|      1 |  516 | `	if( n == 2 && z[1] == ':' ){` |
|      1 |  517 | `		return 1; /* a bare drive, "C:" */` |
|      - |  518 | `	}` |
|      1 |  519 | `	if( n > 1 && (z[0] == '/' \|\| z[0] == '\\') && (z[1] == '/' \|\| z[1] == '\\') ){` |
|    ! 0 |  520 | `		for( i = 2 ; i < n ; i++ ){` |
|    ! 0 |  521 | `			if( z[i] == '/' \|\| z[i] == '\\' ){` |
|    ! 0 |  522 | `				nSep++;` |
|      - |  523 | `			}` |
|    ! 0 |  524 | `		}` |
|    ! 0 |  525 | `		return nSep < 2; /* still inside \\server\share */` |
|      - |  526 | `	}` |
|      1 |  527 | `	return 0;` |
|      - |  528 | `#else` |
|     68 |  529 | `	SXUNUSED(z);` |
|    100 |  530 | `	return n < 1;` |
|      - |  531 | `#endif` |
|      1 |  532 | `}` |
|      - |  533 | `#ifdef __WINNT__` |
|      - |  534 | `#define VFS_MKDIR_SLASH(c) ((c) == '/' \|\| (c) == '\\')` |
|      - |  535 | `#else` |
|      - |  536 | `#define VFS_MKDIR_SLASH(c) ((c) == '/')` |
|      - |  537 | `#endif` |
|      - |  538 | `/*` |
|      - |  539 | ` * php's $recursive: create every missing ancestor, then the directory itself.` |
|      - |  540 | `` * The flag reached the VFS and both back ends dropped it (`SXUNUSED(recursive)`),`` |
|      - |  541 | `` * so `mkdir("$d/a/b", 0777, true)` -- the everyday way a script prepares an`` |
|      - |  542 | ` * output tree -- warned "No such file or directory" and answered false whenever` |
|      - |  543 | ` * more than one level was missing.` |
|      - |  544 | ` *` |
|      - |  545 | ` * php does the walk in the WRAPPER too, not in the syscall, and the rules the` |
|      - |  546 | ` * oracle shows are: the mode is applied to every level it creates; an ancestor` |
|      - |  547 | ` * that already exists is skipped in silence; the LEAF is always attempted, so an` |
|      - |  548 | ` * existing one is "File exists" exactly as without the flag; a trailing` |
|      - |  549 | ` * separator names the same directory; and the empty path is refused up front` |
|      - |  550 | ` * with a message of its own.` |
|      - |  551 | ` */` |
|     24 |  552 | `static int VfsMkdirRecursive(ph7_context *pCtx,ph7_vfs *pVfs,const char *zPath,int iMode)` |
|      1 |  553 | `{` |
|      - |  554 | `	SyBlob sWorker;` |
|      - |  555 | `	const char *zLocal;` |
|     25 |  556 | `	int i,nPath,nScheme,rc = PH7_OK;` |
|      - |  557 | `	/* The strip first: the component walk must not cut a "file://" scheme up. */` |
|     25 |  558 | `	zLocal = PH7_VmFileUrlLocalPath(zPath);` |
|     25 |  559 | `	nScheme = PH7_VmUrlSchemeLen(zPath,-1);` |
|     24 |  560 | `	if( zLocal == zPath && nScheme == (int)sizeof("file")-1` |
|     13 |  561 | `	 && SyStrnicmp(zPath,"file",sizeof("file")-1) == 0 ){` |
|      - |  562 | `		/* A file:// AUTHORITY this build will not reach: the strip handed the` |
|      - |  563 | `		 * URL straight back. php answers false and creates NOTHING, where the` |
|      - |  564 | `		 * walk below would cut the URL into components and make a directory` |
|      - |  565 | `		 * literally called "file:". (A name under any OTHER scheme really does` |
|      - |  566 | `` 		 * become a directory of that name on php too -- measured: `zzz://a/b` `` |
|      - |  567 | ``		 * leaves a `zzz:` behind there as well -- so only this one is refused.) */`` |
|      3 |  568 | `		return -1;` |
|      - |  569 | `	}` |
|     23 |  570 | `	zPath = zLocal;` |
|     23 |  571 | `	nPath = (int)SyStrlen(zPath);` |
|      - |  572 | `	/* A trailing separator names the same directory; php's own expand_filepath` |
|      - |  573 | `	 * drops it before it starts. */` |
|     25 |  574 | `	while( nPath > 1 && VFS_MKDIR_SLASH(zPath[nPath-1]) ){` |
|      3 |  575 | `		nPath--;` |
|      1 |  576 | `	}` |
|     23 |  577 | `	if( nPath < 1 ){` |
|      - |  578 | `		/* php: expand_filepath() refuses it, with this wording and no path. */` |
|      3 |  579 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Invalid path",ph7_function_name(pCtx));` |
|      3 |  580 | `		return -1;` |
|      - |  581 | `	}` |
|     21 |  582 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|    858 |  583 | `	for( i = 1 ; i <= nPath ; i++ ){` |
|    844 |  584 | `		int bLeaf = (i == nPath);` |
|    844 |  585 | `		if( !bLeaf ){` |
|      - |  586 | `			/* Only at a separator that ENDS a component: a run of them names` |
|      - |  587 | `			 * the same ancestor once. */` |
|    824 |  588 | `			if( !VFS_MKDIR_SLASH(zPath[i]) \|\| VFS_MKDIR_SLASH(zPath[i-1]) ){` |
|    724 |  589 | `				continue;` |
|      - |  590 | `			}` |
|    101 |  591 | `			if( VfsMkdirVolumePrefix(zPath,i) ){` |
|      1 |  592 | `				continue;` |
|      - |  593 | `			}` |
|     68 |  594 | `		}` |
|    121 |  595 | `		SyBlobReset(&sWorker);` |
|    120 |  596 | `		if( SyBlobAppend(&sWorker,zPath,(sxu32)i) != SXRET_OK` |
|    121 |  597 | `		 \|\| SyBlobNullAppend(&sWorker) != SXRET_OK ){` |
|    ! 0 |  598 | `			rc = -1;` |
|    ! 0 |  599 | `			break;` |
|      - |  600 | `		}` |
|    121 |  601 | `		if( !bLeaf && VfsPathStatable(pVfs,(const char *)SyBlobData(&sWorker)) ){` |
|     85 |  602 | `			continue; /* an ancestor that is already there */` |
|      - |  603 | `		}` |
|     37 |  604 | `		errno = 0;` |
|     37 |  605 | `		rc = pVfs->xMkdir((const char *)SyBlobData(&sWorker),iMode,0);` |
|     37 |  606 | `		if( rc != PH7_OK ){` |
|     10 |  607 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      6 |  608 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      7 |  609 | `			break;` |
|      - |  610 | `		}` |
|     16 |  611 | `	}` |
|     21 |  612 | `	SyBlobRelease(&sWorker);` |
|     21 |  613 | `	return rc;` |
|     13 |  614 | `}` |
|    140 |  615 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  616 | `{` |
|    145 |  617 | `	int iRecursive = 0;` |
|      - |  618 | `	const char *zPath;` |
|      - |  619 | `	ph7_vfs *pVfs;` |
|    145 |  620 | `	int iMode,rc,bThrew = 0;` |
|    145 |  621 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  622 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  623 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  624 | `		return PH7_OK;` |
|      - |  625 | `	}` |
|      - |  626 | ``	/* php's `?resource $context`, refused when it is a resource of another kind.`` |
|      - |  627 | `	 * Nothing here CONSUMES it — this operation never opens a stream, and the` |
|      - |  628 | `	 * userland wrapper's unlink/rename/mkdir/rmdir methods are not dispatched` |
|      - |  629 | `	 * (§7.4 slice-2 (e)) — but the refusal is the argument's contract, and it` |
|      - |  630 | `	 * was accepted in silence. */` |
|    145 |  631 | `	PH7_StreamCtxFromArg(pCtx,nArg,apArg,3,"$context",0,&bThrew);` |
|    145 |  632 | `	if( bThrew ){` |
|      6 |  633 | `		return PH7_OK;` |
|      - |  634 | `	}` |
|      - |  635 | `	/* Point to the underlying vfs */` |
|    141 |  636 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    141 |  637 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  638 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  639 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  640 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  641 | `			ph7_function_name(pCtx)` |
|      - |  642 | `			);` |
|    ! 0 |  643 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  644 | `		return PH7_OK;` |
|      - |  645 | `	}` |
|      - |  646 | `	/* Point to the desired directory */` |
|    141 |  647 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  648 | `#ifdef __WINNT__` |
|      5 |  649 | `	iMode = 0;` |
|      - |  650 | `#else` |
|      - |  651 | `	/* Assume UNIX */` |
|    136 |  652 | `	iMode = 0777;` |
|      - |  653 | `#endif` |
|    141 |  654 | `	if( nArg > 1 ){` |
|     25 |  655 | `		iMode = ph7_value_to_int(apArg[1]);` |
|     25 |  656 | `		if( nArg > 2 ){` |
|     25 |  657 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|     12 |  658 | `		}` |
|     12 |  659 | `	}` |
|      - |  660 | `	/* Perform the requested operation */` |
|    141 |  661 | `	if( iRecursive ){` |
|     25 |  662 | `		rc = VfsMkdirRecursive(pCtx,pVfs,zPath,iMode);` |
|     13 |  663 | `	}else{` |
|    117 |  664 | `		errno = 0;` |
|    117 |  665 | `		rc = pVfs->xMkdir(zPath,iMode,0);` |
|    117 |  666 | `		if( rc != PH7_OK ){` |
|      - |  667 | `			/* php does NOT name the path for mkdir: "mkdir(): File exists" */` |
|      7 |  668 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 |  669 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      2 |  670 | `		}` |
|      - |  671 | `	}` |
|      - |  672 | `	/* IO return value */` |
|    141 |  673 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    141 |  674 | `	return PH7_OK;` |
|     75 |  675 | `}` |
|      - |  676 | `/*` |
|      - |  677 | ` * bool rename(string $oldname,string $newname)` |
|      - |  678 | ` *  Attempts to rename oldname to newname.` |
|      - |  679 | ` * Parameters` |
|      - |  680 | ` *  $oldname` |
|      - |  681 | ` *   Old name.` |
|      - |  682 | ` *  $newname` |
|      - |  683 | ` *   New name.` |
|      - |  684 | ` * Return` |
|      - |  685 | ` *  TRUE on success or FALSE on failure.` |
|      - |  686 | ` */` |
|      4 |  687 | `static int PH7_vfs_rename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  688 | `{` |
|      - |  689 | `	const char *zOld,*zNew;` |
|      - |  690 | `	ph7_vfs *pVfs;` |
|      5 |  691 | `	int rc,bThrew = 0;` |
|      5 |  692 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - |  693 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  694 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  695 | `		return PH7_OK;` |
|      - |  696 | `	}` |
|      - |  697 | ``	/* php's `?resource $context`, refused when it is a resource of another kind.`` |
|      - |  698 | `	 * Nothing here CONSUMES it — this operation never opens a stream, and the` |
|      - |  699 | `	 * userland wrapper's unlink/rename/mkdir/rmdir methods are not dispatched` |
|      - |  700 | `	 * (§7.4 slice-2 (e)) — but the refusal is the argument's contract, and it` |
|      - |  701 | `	 * was accepted in silence. */` |
|      5 |  702 | `	PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|      5 |  703 | `	if( bThrew ){` |
|      3 |  704 | `		return PH7_OK;` |
|      - |  705 | `	}` |
|      - |  706 | `	/* Point to the underlying vfs */` |
|      3 |  707 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  708 | `	if( pVfs == 0 \|\| pVfs->xRename == 0 ){` |
|      - |  709 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  710 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  711 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  712 | `			ph7_function_name(pCtx)` |
|      - |  713 | `			);` |
|    ! 0 |  714 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  715 | `		return PH7_OK;` |
|      - |  716 | `	}` |
|      - |  717 | `	/* Perform the requested operation */` |
|      3 |  718 | `	zOld = ph7_value_to_string(apArg[0],0);` |
|      3 |  719 | `	zNew = ph7_value_to_string(apArg[1],0);` |
|      3 |  720 | `	errno = 0;` |
|      3 |  721 | `	rc = pVfs->xRename(zOld,zNew);` |
|      3 |  722 | `	if( rc != PH7_OK ){` |
|      - |  723 | `		/* php names BOTH paths here */` |
|    ! 0 |  724 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s,%s): %s",` |
|    ! 0 |  725 | `			ph7_function_name(pCtx),zOld,zNew,VfsStrerror(errno));` |
|    ! 0 |  726 | `	}` |
|      - |  727 | `	/* IO result */` |
|      3 |  728 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 |  729 | `	return PH7_OK;` |
|      3 |  730 | `}` |
|      - |  731 | `/*` |
|      - |  732 | ` * string realpath(string $path)` |
|      - |  733 | ` *  Returns canonicalized absolute pathname.` |
|      - |  734 | ` * Parameters` |
|      - |  735 | ` *  $path` |
|      - |  736 | ` *   Target path.` |
|      - |  737 | ` * Return` |
|      - |  738 | ` *  Canonicalized absolute pathname on success. or FALSE on failure.` |
|      - |  739 | ` */` |
|      8 |  740 | `static int PH7_vfs_realpath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  741 | `{` |
|      - |  742 | `	const char *zPath;` |
|      - |  743 | `	ph7_vfs *pVfs;` |
|      - |  744 | `        int rc;` |
|     11 |  745 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  746 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  747 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  748 | `		return PH7_OK;` |
|      - |  749 | `	}` |
|      - |  750 | `	/* Point to the underlying vfs */` |
|     11 |  751 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 |  752 | `	if( pVfs == 0 \|\| pVfs->xRealpath == 0 ){` |
|      - |  753 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  754 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  755 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  756 | `			ph7_function_name(pCtx)` |
|      - |  757 | `			);` |
|    ! 0 |  758 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  759 | `		return PH7_OK;` |
|      - |  760 | `	}` |
|      - |  761 | `	/* Set an empty string untnil the underlying OS interface change that */` |
|     11 |  762 | `	ph7_result_string(pCtx,"",0);` |
|      - |  763 | `	/* Perform the requested operation */` |
|     11 |  764 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|     11 |  765 | `	rc = pVfs->xRealpath(zPath,pCtx);` |
|     11 |  766 | `	if( rc != PH7_OK ){` |
|      2 |  767 | `	 ph7_result_bool(pCtx,0);` |
|      1 |  768 | `	}` |
|     11 |  769 | `	return PH7_OK;` |
|      7 |  770 | `}` |
|      - |  771 | `/*` |
|      - |  772 | ` * Does this candidate name something, and if so what is its canonical path?` |
|      - |  773 | ` * The existence question is asked separately because xRealpath() writes STRAIGHT` |
|      - |  774 | ` * into the call's result, so it may only be run on the winner.` |
|      - |  775 | ` */` |
|     32 |  776 | `static int VfsResolveTry(ph7_vfs *pVfs,ph7_context *pCtx,const char *zCand)` |
|      1 |  777 | `{` |
|     33 |  778 | `	if( pVfs->xFileExists(zCand) != PH7_OK ){` |
|     19 |  779 | `		return 0;` |
|      - |  780 | `	}` |
|      - |  781 | `	/* The VFS APPENDS into the call's result, so make it an empty string first` |
|      - |  782 | `	 * -- the realpath() builtin beside this one seeds the same way. */` |
|     15 |  783 | `	ph7_result_string(pCtx,"",0);` |
|     15 |  784 | `	if( pVfs->xRealpath(zCand,pCtx) == PH7_OK ){` |
|     15 |  785 | `		return 1;` |
|      - |  786 | `	}` |
|    ! 0 |  787 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 |  788 | `	return 0;` |
|     17 |  789 | `}` |
|      - |  790 | `/*` |
|      - |  791 | ` * Is this an ABSOLUTE path, by php's rule for this platform? A lone leading` |
|      - |  792 | ` * slash is NOT absolute on Windows -- php walks the include_path for it.` |
|      - |  793 | ` */` |
|     16 |  794 | `static int VfsPathIsAbsolute(const char *z,int n)` |
|      1 |  795 | `{` |
|      - |  796 | `#ifdef __WINNT__` |
|      1 |  797 | `	if( n >= 2 && ((z[0] >= 'A' && z[0] <= 'Z') \|\| (z[0] >= 'a' && z[0] <= 'z')) && z[1] == ':' ){` |
|      1 |  798 | `		return 1;` |
|      - |  799 | `	}` |
|      1 |  800 | `	return n >= 2 && (z[0] == '/' \|\| z[0] == '\\') && (z[1] == '/' \|\| z[1] == '\\');` |
|      - |  801 | `#else` |
|     16 |  802 | `	return n >= 1 && z[0] == '/';` |
|      - |  803 | `#endif` |
|      1 |  804 | `}` |
|      - |  805 | `/* "./x" and "../x": php reads these against the CWD and never walks the path. */` |
|     20 |  806 | `static int VfsPathIsDotRelative(const char *z,int n)` |
|      1 |  807 | `{` |
|      - |  808 | `#ifdef __WINNT__` |
|      - |  809 | `#define VFS_RESOLVE_SLASH(c) ((c) == '/' \|\| (c) == '\\')` |
|      - |  810 | `#else` |
|      - |  811 | `#define VFS_RESOLVE_SLASH(c) ((c) == '/')` |
|      - |  812 | `#endif` |
|     21 |  813 | `	if( n < 2 \|\| z[0] != '.' ){` |
|     17 |  814 | `		return 0;` |
|      - |  815 | `	}` |
|      5 |  816 | `	if( VFS_RESOLVE_SLASH(z[1]) ){` |
|      3 |  817 | `		return 1;` |
|      - |  818 | `	}` |
|      3 |  819 | `	return n > 2 && z[1] == '.' && VFS_RESOLVE_SLASH(z[2]);` |
|     11 |  820 | `}` |
|      - |  821 | `/*` |
|      - |  822 | ` * string\|false stream_resolve_include_path(string $filename)` |
|      - |  823 | ` *  Where would include/require find this name? php's own php_resolve_path,` |
|      - |  824 | ` *  which is the ONLY way a script can ask that question without opening` |
|      - |  825 | ` *  anything -- and the way an autoloader decides whether a class file exists` |
|      - |  826 | ` *  before requiring it.` |
|      - |  827 | ` * Return` |
|      - |  828 | ` *  The canonical path on success, FALSE when nothing answers.` |
|      - |  829 | ` */` |
|     28 |  830 | `static int PH7_vfs_stream_resolve_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  831 | `{` |
|     29 |  832 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  833 | `	ph7_vfs *pVfs;` |
|      - |  834 | `	const char *zPath;` |
|      - |  835 | `	SyString *aEntry;` |
|      - |  836 | `	SyString sDir;` |
|      - |  837 | `	SyBlob sWorker;` |
|     29 |  838 | `	int nPath = 0, nScheme, c;` |
|      - |  839 | `	sxu32 n;` |
|      - |  840 | `	/* FALSE until something resolves: xRealpath() overwrites it on the winner. */` |
|     29 |  841 | `	ph7_result_bool(pCtx,0);` |
|     29 |  842 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     29 |  843 | `	if( nArg < 1 \|\| pVfs == 0 \|\| pVfs->xRealpath == 0 \|\| pVfs->xFileExists == 0 ){` |
|    ! 0 |  844 | `		return PH7_OK;` |
|      - |  845 | `	}` |
|     29 |  846 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|     29 |  847 | `	if( nPath < 0 ){` |
|    ! 0 |  848 | `		nPath = 0;` |
|    ! 0 |  849 | `	}` |
|     29 |  850 | `	nScheme = PH7_VmUrlSchemeLen(zPath,nPath);` |
|     29 |  851 | `	if( nScheme > 0 ){` |
|      - |  852 | `		/* A name that carries a scheme is never walked. php resolves exactly one` |
|      - |  853 | `		 * of them -- file://, which it realpaths where it stands -- and answers` |
|      - |  854 | `		 * false for every other wrapper. An unreachable authority (and any other` |
|      - |  855 | `		 * scheme) comes back unchanged from the strip, and is false. */` |
|      9 |  856 | `		const char *zLocal = PH7_VmFileUrlLocalPath(zPath);` |
|      9 |  857 | `		if( zLocal != zPath ){` |
|      3 |  858 | `			VfsResolveTry(pVfs,pCtx,zLocal);` |
|      1 |  859 | `		}` |
|      9 |  860 | `		return PH7_OK;` |
|      - |  861 | `	}` |
|     20 |  862 | `	if( VfsPathIsDotRelative(zPath,nPath) \|\| VfsPathIsAbsolute(zPath,nPath)` |
|     14 |  863 | `	 \|\| SySetUsed(&pVm->aPaths) < 1 ){` |
|     11 |  864 | `		VfsResolveTry(pVfs,pCtx,zPath);` |
|     11 |  865 | `		return PH7_OK;` |
|      - |  866 | `	}` |
|     11 |  867 | `	c = '/';` |
|      - |  868 | `#ifdef __WINNT__` |
|      1 |  869 | `	c = '\\';` |
|      - |  870 | `#endif` |
|     11 |  871 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|     11 |  872 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|     21 |  873 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|      - |  874 | `		SyString sFile;` |
|     17 |  875 | `		SyStringInitFromBuf(&sFile,zPath,(sxu32)nPath);` |
|     17 |  876 | `		SyBlobReset(&sWorker);` |
|     17 |  877 | `		SyBlobFormat(&sWorker,"%z%c%z",&aEntry[n],c,&sFile);` |
|     17 |  878 | `		if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 |  879 | `			continue;` |
|      - |  880 | `		}` |
|     17 |  881 | `		if( VfsResolveTry(pVfs,pCtx,(const char *)SyBlobData(&sWorker)) ){` |
|      7 |  882 | `			SyBlobRelease(&sWorker);` |
|      7 |  883 | `			return PH7_OK;` |
|      - |  884 | `		}` |
|      6 |  885 | `	}` |
|      - |  886 | `	/* The same last resort the opener uses: the executing file's directory. */` |
|      5 |  887 | `	if( PH7_VmExecutingDir(pVm,&sDir) ){` |
|      - |  888 | `		SyString sFile;` |
|      5 |  889 | `		SyStringInitFromBuf(&sFile,zPath,(sxu32)nPath);` |
|      5 |  890 | `		SyBlobReset(&sWorker);` |
|      5 |  891 | `		SyBlobFormat(&sWorker,"%z%c%z",&sDir,c,&sFile);` |
|      5 |  892 | `		if( SXRET_OK == SyBlobNullAppend(&sWorker) ){` |
|      5 |  893 | `			VfsResolveTry(pVfs,pCtx,(const char *)SyBlobData(&sWorker));` |
|      2 |  894 | `		}` |
|      2 |  895 | `	}` |
|      5 |  896 | `	SyBlobRelease(&sWorker);` |
|      5 |  897 | `	return PH7_OK;` |
|     15 |  898 | `}` |
|      - |  899 | `/*` |
|      - |  900 | ` * int sleep(int $seconds)` |
|      - |  901 | ` *  Delays the program execution for the given number of seconds.` |
|      - |  902 | ` * Parameters` |
|      - |  903 | ` *  $seconds` |
|      - |  904 | ` *   Halt time in seconds.` |
|      - |  905 | ` * Return` |
|      - |  906 | ` *  Zero on success or FALSE on failure.` |
|      - |  907 | ` */` |
|     10 |  908 | `static int PH7_vfs_sleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  909 | `{` |
|      - |  910 | `	ph7_vfs *pVfs;` |
|      - |  911 | `	int rc,nSleep;` |
|     11 |  912 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  913 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  914 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  915 | `		return PH7_OK;` |
|      - |  916 | `	}` |
|      - |  917 | `	/* Point to the underlying vfs */` |
|     11 |  918 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 |  919 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  920 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  921 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  922 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  923 | `			ph7_function_name(pCtx)` |
|      - |  924 | `			);` |
|    ! 0 |  925 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  926 | `		return PH7_OK;` |
|      - |  927 | `	}` |
|      - |  928 | `	/* Amount to sleep */` |
|     11 |  929 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|     11 |  930 | `	if( nSleep < 0 ){` |
|      - |  931 | `		/* php raises rather than sleeping for an unsigned-wrapped eternity */` |
|      3 |  932 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  933 | `			"sleep(): Argument #1 ($seconds) must be greater than or equal to 0");` |
|      - |  934 | `	}` |
|      - |  935 | `	/* Perform the requested operation (Microseconds) */` |
|      9 |  936 | `	rc = pVfs->xSleep((unsigned int)(nSleep * SX_USEC_PER_SEC));` |
|      9 |  937 | `	if( rc != PH7_OK ){` |
|      - |  938 | `		/* Return FALSE */` |
|    ! 0 |  939 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  940 | `	}else{` |
|      - |  941 | `		/* Return zero */` |
|      9 |  942 | `		ph7_result_int(pCtx,0);` |
|      - |  943 | `	}` |
|      9 |  944 | `	return PH7_OK;` |
|      6 |  945 | `}` |
|      - |  946 | `/*` |
|      - |  947 | ` * void usleep(int $micro_seconds)` |
|      - |  948 | ` *  Delays program execution for the given number of micro seconds.` |
|      - |  949 | ` * Parameters` |
|      - |  950 | ` *  $micro_seconds` |
|      - |  951 | ` *   Halt time in micro seconds. A micro second is one millionth of a second.` |
|      - |  952 | ` * Return` |
|      - |  953 | ` *  None.` |
|      - |  954 | ` */` |
|     78 |  955 | `static int PH7_vfs_usleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  956 | `{` |
|      - |  957 | `	ph7_vfs *pVfs;` |
|      - |  958 | `	int nSleep;` |
|     82 |  959 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  960 | `		/* Missing/Invalid argument,return immediately */` |
|    ! 0 |  961 | `		return PH7_OK;` |
|      - |  962 | `	}` |
|      - |  963 | `	/* Point to the underlying vfs */` |
|     82 |  964 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     82 |  965 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  966 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  967 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  968 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 |  969 | `			ph7_function_name(pCtx)` |
|      - |  970 | `			);` |
|    ! 0 |  971 | `		return PH7_OK;` |
|      - |  972 | `	}` |
|      - |  973 | `	/* Amount to sleep */` |
|     82 |  974 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|     82 |  975 | `	if( nSleep < 0 ){` |
|      - |  976 | `		/* php raises rather than sleeping for an unsigned-wrapped eternity */` |
|      3 |  977 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  978 | `			"usleep(): Argument #1 ($microseconds) must be greater than or equal to 0");` |
|      - |  979 | `	}` |
|      - |  980 | `	/* Perform the requested operation (Microseconds) */` |
|     80 |  981 | `	pVfs->xSleep((unsigned int)nSleep);` |
|     80 |  982 | `	return PH7_OK;` |
|     43 |  983 | `}` |
|      - |  984 | `/*` |
|      - |  985 | ` * bool unlink (string $filename)` |
|      - |  986 | ` *  Delete a file.` |
|      - |  987 | ` * Parameters` |
|      - |  988 | ` *  $filename` |
|      - |  989 | ` *   Path to the file.` |
|      - |  990 | ` * Return` |
|      - |  991 | ` *  TRUE on success or FALSE on failure.` |
|      - |  992 | ` */` |
|  40566 |  993 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  994 | `{` |
|      - |  995 | `	const char *zPath;` |
|      - |  996 | `	ph7_vfs *pVfs;` |
|  40571 |  997 | `	int rc,bThrew = 0;` |
|  40571 |  998 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  999 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1000 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1001 | `		return PH7_OK;` |
|      - | 1002 | `	}` |
|      - | 1003 | ``	/* php's `?resource $context`, refused when it is a resource of another kind.`` |
|      - | 1004 | `	 * Nothing here CONSUMES it — this operation never opens a stream, and the` |
|      - | 1005 | `	 * userland wrapper's unlink/rename/mkdir/rmdir methods are not dispatched` |
|      - | 1006 | `	 * (§7.4 slice-2 (e)) — but the refusal is the argument's contract, and it` |
|      - | 1007 | `	 * was accepted in silence. */` |
|  40571 | 1008 | `	PH7_StreamCtxFromArg(pCtx,nArg,apArg,1,"$context",0,&bThrew);` |
|  40571 | 1009 | `	if( bThrew ){` |
|      8 | 1010 | `		return PH7_OK;` |
|      - | 1011 | `	}` |
|      - | 1012 | `	/* Point to the underlying vfs */` |
|  40565 | 1013 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  40565 | 1014 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - | 1015 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1016 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1017 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1018 | `			ph7_function_name(pCtx)` |
|      - | 1019 | `			);` |
|    ! 0 | 1020 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1021 | `		return PH7_OK;` |
|      - | 1022 | `	}` |
|      - | 1023 | `	/* Point to the desired directory */` |
|  40565 | 1024 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1025 | `	/* Perform the requested operation */` |
|  40565 | 1026 | `	errno = 0;` |
|  40565 | 1027 | `	rc = pVfs->xUnlink(zPath);` |
|  40565 | 1028 | `	if( rc != PH7_OK ){` |
|  24873 | 1029 | `		VfsThrowSysWarning(pCtx,zPath);` |
|  12434 | 1030 | `	}` |
|      - | 1031 | `	/* IO return value */` |
|  40565 | 1032 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  40565 | 1033 | `	return PH7_OK;` |
|  20288 | 1034 | `}` |
|      - | 1035 | `/*` |
|      - | 1036 | ` * bool chmod(string $filename,int $mode)` |
|      - | 1037 | ` *  Attempts to change the mode of the specified file to that given in mode.` |
|      - | 1038 | ` * Parameters` |
|      - | 1039 | ` *  $filename` |
|      - | 1040 | ` *   Path to the file.` |
|      - | 1041 | ` * $mode` |
|      - | 1042 | ` *   Mode (Must be an integer)` |
|      - | 1043 | ` * Return` |
|      - | 1044 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1045 | ` */` |
|    272 | 1046 | `static int PH7_vfs_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1047 | `{` |
|      - | 1048 | `	const char *zPath;` |
|      - | 1049 | `	ph7_vfs *pVfs;` |
|      - | 1050 | `	int iMode;` |
|      - | 1051 | `	int rc;` |
|    277 | 1052 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1053 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1054 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1055 | `		return PH7_OK;` |
|      - | 1056 | `	}` |
|      - | 1057 | `	/* Point to the underlying vfs */` |
|    277 | 1058 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    277 | 1059 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - | 1060 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1061 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1062 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1063 | `			ph7_function_name(pCtx)` |
|      - | 1064 | `			);` |
|    ! 0 | 1065 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1066 | `		return PH7_OK;` |
|      - | 1067 | `	}` |
|      - | 1068 | `	/* Point to the desired directory */` |
|    277 | 1069 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1070 | `	/* Extract the mode */` |
|    277 | 1071 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - | 1072 | `	/* Perform the requested operation */` |
|    277 | 1073 | `	rc = pVfs->xChmod(zPath,iMode);` |
|      - | 1074 | `	/* IO return value */` |
|    277 | 1075 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    277 | 1076 | `	return PH7_OK;` |
|    141 | 1077 | `}` |
|      - | 1078 | `/*` |
|      - | 1079 | ` * bool chown(string $filename,string $user)` |
|      - | 1080 | ` *  Attempts to change the owner of the file filename to user user.` |
|      - | 1081 | ` * Parameters` |
|      - | 1082 | ` *  $filename` |
|      - | 1083 | ` *   Path to the file.` |
|      - | 1084 | ` * $user` |
|      - | 1085 | ` *   Username.` |
|      - | 1086 | ` * Return` |
|      - | 1087 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1088 | ` */` |
|      6 | 1089 | `static int PH7_vfs_chown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1090 | `{` |
|      - | 1091 | `	const char *zPath,*zUser;` |
|      - | 1092 | `	ph7_vfs *pVfs;` |
|      - | 1093 | `	int rc;` |
|      7 | 1094 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1095 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1096 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1097 | `		return PH7_OK;` |
|      - | 1098 | `	}` |
|      - | 1099 | `	/* Point to the underlying vfs */` |
|      7 | 1100 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 1101 | `	if( pVfs == 0 \|\| pVfs->xChown == 0 ){` |
|      - | 1102 | `		/* IO routine not implemented,return NULL */` |
|      1 | 1103 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1104 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1105 | `			ph7_function_name(pCtx)` |
|      - | 1106 | `			);` |
|      1 | 1107 | `		ph7_result_bool(pCtx,0);` |
|      1 | 1108 | `		return PH7_OK;` |
|      - | 1109 | `	}` |
|      - | 1110 | `	/* Point to the desired directory */` |
|      6 | 1111 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1112 | `	/* Extract the user */` |
|      6 | 1113 | `	zUser = ph7_value_to_string(apArg[1],0);` |
|      - | 1114 | `	/* Perform the requested operation */` |
|      6 | 1115 | `	errno = 0;` |
|      6 | 1116 | `	rc = pVfs->xChown(zPath,zUser);` |
|      6 | 1117 | `	if( rc != PH7_OK ){` |
|      - | 1118 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - | 1119 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - | 1120 | `		 * "chown(): Operation not permitted". */` |
|      6 | 1121 | `		if( rc == -2 ){` |
|      3 | 1122 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find uid for %s",` |
|      1 | 1123 | `				ph7_function_name(pCtx),zUser);` |
|      1 | 1124 | `		}else{` |
|      6 | 1125 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 | 1126 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - | 1127 | `		}` |
|      3 | 1128 | `	}` |
|      - | 1129 | `	/* IO return value */` |
|      6 | 1130 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 | 1131 | `	return PH7_OK;` |
|      4 | 1132 | `}` |
|      - | 1133 | `/*` |
|      - | 1134 | ` * bool chgrp(string $filename,string $group)` |
|      - | 1135 | ` *  Attempts to change the group of the file filename to group.` |
|      - | 1136 | ` * Parameters` |
|      - | 1137 | ` *  $filename` |
|      - | 1138 | ` *   Path to the file.` |
|      - | 1139 | ` * $group` |
|      - | 1140 | ` *   groupname.` |
|      - | 1141 | ` * Return` |
|      - | 1142 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1143 | ` */` |
|      6 | 1144 | `static int PH7_vfs_chgrp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1145 | `{` |
|      - | 1146 | `	const char *zPath,*zGroup;` |
|      - | 1147 | `	ph7_vfs *pVfs;` |
|      - | 1148 | `	int rc;` |
|      7 | 1149 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1150 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1151 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1152 | `		return PH7_OK;` |
|      - | 1153 | `	}` |
|      - | 1154 | `	/* Point to the underlying vfs */` |
|      7 | 1155 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 1156 | `	if( pVfs == 0 \|\| pVfs->xChgrp == 0 ){` |
|      - | 1157 | `		/* IO routine not implemented,return NULL */` |
|      1 | 1158 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1159 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1160 | `			ph7_function_name(pCtx)` |
|      - | 1161 | `			);` |
|      1 | 1162 | `		ph7_result_bool(pCtx,0);` |
|      1 | 1163 | `		return PH7_OK;` |
|      - | 1164 | `	}` |
|      - | 1165 | `	/* Point to the desired directory */` |
|      6 | 1166 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1167 | `	/* Extract the user */` |
|      6 | 1168 | `	zGroup = ph7_value_to_string(apArg[1],0);` |
|      - | 1169 | `	/* Perform the requested operation */` |
|      6 | 1170 | `	errno = 0;` |
|      6 | 1171 | `	rc = pVfs->xChgrp(zPath,zGroup);` |
|      6 | 1172 | `	if( rc != PH7_OK ){` |
|      - | 1173 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - | 1174 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - | 1175 | `		 * "chown(): Operation not permitted". */` |
|      6 | 1176 | `		if( rc == -2 ){` |
|      3 | 1177 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find gid for %s",` |
|      1 | 1178 | `				ph7_function_name(pCtx),zGroup);` |
|      1 | 1179 | `		}else{` |
|      6 | 1180 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 | 1181 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - | 1182 | `		}` |
|      3 | 1183 | `	}` |
|      - | 1184 | `	/* IO return value */` |
|      6 | 1185 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 | 1186 | `	return PH7_OK;` |
|      4 | 1187 | `}` |
|      - | 1188 | `/*` |
|      - | 1189 | ` * int64 disk_free_space(string $directory)` |
|      - | 1190 | ` *  Returns available space on filesystem or disk partition.` |
|      - | 1191 | ` * Parameters` |
|      - | 1192 | ` *  $directory` |
|      - | 1193 | ` *   A directory of the filesystem or disk partition.` |
|      - | 1194 | ` * Return` |
|      - | 1195 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - | 1196 | ` */` |
|     14 | 1197 | `static int PH7_vfs_disk_free_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1198 | `{` |
|      - | 1199 | `	const char *zPath;` |
|      - | 1200 | `	ph7_int64 iSize;` |
|      - | 1201 | `	ph7_vfs *pVfs;` |
|     15 | 1202 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1203 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1204 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1205 | `		return PH7_OK;` |
|      - | 1206 | `	}` |
|      - | 1207 | `	/* Point to the underlying vfs */` |
|     15 | 1208 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     15 | 1209 | `	if( pVfs == 0 \|\| pVfs->xFreeSpace == 0 ){` |
|      - | 1210 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1211 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1212 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1213 | `			ph7_function_name(pCtx)` |
|      - | 1214 | `			);` |
|    ! 0 | 1215 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1216 | `		return PH7_OK;` |
|      - | 1217 | `	}` |
|      - | 1218 | `	/* Point to the desired directory */` |
|     15 | 1219 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1220 | `	/* Perform the requested operation */` |
|     15 | 1221 | `	errno = 0;` |
|     15 | 1222 | `	iSize = pVfs->xFreeSpace(zPath);` |
|     15 | 1223 | `	if( iSize < 0 ){` |
|      - | 1224 | `		/* php answers FALSE and warns with the C library's own reason` |
|      - | 1225 | ``		 * (`disk_free_space(): No such file or directory`). PHL answered int(-1) --`` |
|      - | 1226 | `		 * truthy, and a plausible-looking byte count for a caller that only tests` |
|      - | 1227 | ``		 * `if ($free)` or compares it against a threshold. */`` |
|      7 | 1228 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 | 1229 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|      5 | 1230 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1231 | `		return PH7_OK;` |
|      - | 1232 | `	}` |
|      - | 1233 | `	/* php's return type is FLOAT, not int: the value can exceed what an int holds` |
|      - | 1234 | ``	 * on a large volume, and a `===`/gettype()/json_encode() reader sees the`` |
|      - | 1235 | `	 * difference on every volume. */` |
|     11 | 1236 | `	ph7_result_double(pCtx,(double)iSize);` |
|     11 | 1237 | `	return PH7_OK;` |
|      8 | 1238 | `}` |
|      - | 1239 | `/*` |
|      - | 1240 | ` * int64 disk_total_space(string $directory)` |
|      - | 1241 | ` *  Returns the total size of a filesystem or disk partition.` |
|      - | 1242 | ` * Parameters` |
|      - | 1243 | ` *  $directory` |
|      - | 1244 | ` *   A directory of the filesystem or disk partition.` |
|      - | 1245 | ` * Return` |
|      - | 1246 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - | 1247 | ` */` |
|     10 | 1248 | `static int PH7_vfs_disk_total_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1249 | `{` |
|      - | 1250 | `	const char *zPath;` |
|      - | 1251 | `	ph7_int64 iSize;` |
|      - | 1252 | `	ph7_vfs *pVfs;` |
|     11 | 1253 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1254 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1255 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1256 | `		return PH7_OK;` |
|      - | 1257 | `	}` |
|      - | 1258 | `	/* Point to the underlying vfs */` |
|     11 | 1259 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 1260 | `	if( pVfs == 0 \|\| pVfs->xTotalSpace == 0 ){` |
|      - | 1261 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1262 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1263 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1264 | `			ph7_function_name(pCtx)` |
|      - | 1265 | `			);` |
|    ! 0 | 1266 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1267 | `		return PH7_OK;` |
|      - | 1268 | `	}` |
|      - | 1269 | `	/* Point to the desired directory */` |
|     11 | 1270 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1271 | `	/* Perform the requested operation */` |
|     11 | 1272 | `	errno = 0;` |
|     11 | 1273 | `	iSize = pVfs->xTotalSpace(zPath);` |
|     11 | 1274 | `	if( iSize < 0 ){` |
|      - | 1275 | `		/* php answers FALSE and warns with the C library's own reason` |
|      - | 1276 | ``		 * (`disk_free_space(): No such file or directory`). PHL answered int(-1) --`` |
|      - | 1277 | `		 * truthy, and a plausible-looking byte count for a caller that only tests` |
|      - | 1278 | ``		 * `if ($free)` or compares it against a threshold. */`` |
|      4 | 1279 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      2 | 1280 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|      3 | 1281 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1282 | `		return PH7_OK;` |
|      - | 1283 | `	}` |
|      - | 1284 | `	/* php's return type is FLOAT, not int: the value can exceed what an int holds` |
|      - | 1285 | ``	 * on a large volume, and a `===`/gettype()/json_encode() reader sees the`` |
|      - | 1286 | `	 * difference on every volume. */` |
|      9 | 1287 | `	ph7_result_double(pCtx,(double)iSize);` |
|      9 | 1288 | `	return PH7_OK;` |
|      6 | 1289 | `}` |
|      - | 1290 | `/*` |
|      - | 1291 | ` * bool file_exists(string $filename)` |
|      - | 1292 | ` *  Checks whether a file or directory exists.` |
|      - | 1293 | ` * Parameters` |
|      - | 1294 | ` *  $filename` |
|      - | 1295 | ` *   Path to the file.` |
|      - | 1296 | ` * Return` |
|      - | 1297 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1298 | ` */` |
|    484 | 1299 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1300 | `{` |
|      - | 1301 | `	const char *zPath;` |
|      - | 1302 | `	ph7_vfs *pVfs;` |
|      - | 1303 | `	int rc;` |
|    489 | 1304 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1305 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1306 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1307 | `		return PH7_OK;` |
|      - | 1308 | `	}` |
|      - | 1309 | `	/* Point to the underlying vfs */` |
|    489 | 1310 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    489 | 1311 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - | 1312 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1313 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1314 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1315 | `			ph7_function_name(pCtx)` |
|      - | 1316 | `			);` |
|    ! 0 | 1317 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1318 | `		return PH7_OK;` |
|      - | 1319 | `	}` |
|      - | 1320 | `	/* Point to the desired directory */` |
|    489 | 1321 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1322 | `	/* Perform the requested operation */` |
|    489 | 1323 | `	rc = pVfs->xFileExists(zPath);` |
|      - | 1324 | `	/* IO return value */` |
|    489 | 1325 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    489 | 1326 | `	return PH7_OK;` |
|    247 | 1327 | `}` |
|      - | 1328 | `/*` |
|      - | 1329 | ` * int64 file_size(string $filename)` |
|      - | 1330 | ` *  Gets the size for the given file.` |
|      - | 1331 | ` * Parameters` |
|      - | 1332 | ` *  $filename` |
|      - | 1333 | ` *   Path to the file.` |
|      - | 1334 | ` * Return` |
|      - | 1335 | ` *  File size on success or FALSE on failure.` |
|      - | 1336 | ` */` |
|     20 | 1337 | `static int PH7_vfs_file_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1338 | `{` |
|      - | 1339 | `	const char *zPath;` |
|      - | 1340 | `	ph7_int64 iSize;` |
|      - | 1341 | `	ph7_vfs *pVfs;` |
|     23 | 1342 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1343 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1344 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1345 | `		return PH7_OK;` |
|      - | 1346 | `	}` |
|      - | 1347 | `	/* Point to the underlying vfs */` |
|     23 | 1348 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     23 | 1349 | `	if( pVfs == 0 \|\| pVfs->xFileSize == 0 ){` |
|      - | 1350 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1351 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1352 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1353 | `			ph7_function_name(pCtx)` |
|      - | 1354 | `			);` |
|    ! 0 | 1355 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1356 | `		return PH7_OK;` |
|      - | 1357 | `	}` |
|      - | 1358 | `	/* Point to the desired directory */` |
|     23 | 1359 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1360 | `	/* Perform the requested operation */` |
|     23 | 1361 | `	iSize = pVfs->xFileSize(zPath);` |
|     23 | 1362 | `	if( iSize < 0 ){` |
|      - | 1363 | `		/* php: a stat failure warns and returns FALSE -- PH7 returned int(-1), which is` |
|      - | 1364 | `		 * truthy and compares equal to nothing a caller would test for. */` |
|      4 | 1365 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): stat failed for %s",` |
|      1 | 1366 | `			ph7_function_name(pCtx),zPath);` |
|      3 | 1367 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1368 | `		return PH7_OK;` |
|      - | 1369 | `	}` |
|      - | 1370 | `	/* IO return value */` |
|     21 | 1371 | `	ph7_result_int64(pCtx,iSize);` |
|     21 | 1372 | `	return PH7_OK;` |
|     13 | 1373 | `}` |
|      - | 1374 | `/*` |
|      - | 1375 | ` * int64 fileatime(string $filename)` |
|      - | 1376 | ` *  Gets the last access time of the given file.` |
|      - | 1377 | ` * Parameters` |
|      - | 1378 | ` *  $filename` |
|      - | 1379 | ` *   Path to the file.` |
|      - | 1380 | ` * Return` |
|      - | 1381 | ` *  File atime on success or FALSE on failure.` |
|      - | 1382 | ` */` |
|      8 | 1383 | `static int PH7_vfs_file_atime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1384 | `{` |
|      - | 1385 | `	const char *zPath;` |
|      - | 1386 | `	ph7_int64 iTime;` |
|      - | 1387 | `	ph7_vfs *pVfs;` |
|      9 | 1388 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1389 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1390 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1391 | `		return PH7_OK;` |
|      - | 1392 | `	}` |
|      - | 1393 | `	/* Point to the underlying vfs */` |
|      9 | 1394 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 | 1395 | `	if( pVfs == 0 \|\| pVfs->xFileAtime == 0 ){` |
|      - | 1396 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1397 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1398 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1399 | `			ph7_function_name(pCtx)` |
|      - | 1400 | `			);` |
|    ! 0 | 1401 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1402 | `		return PH7_OK;` |
|      - | 1403 | `	}` |
|      - | 1404 | `	/* Point to the desired directory */` |
|      9 | 1405 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1406 | `	/* Perform the requested operation */` |
|      9 | 1407 | `	iTime = pVfs->xFileAtime(zPath);` |
|      9 | 1408 | `	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){` |
|      - | 1409 | `		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --` |
|      - | 1410 | `		 * truthy, and one second before the epoch is also a real timestamp, which` |
|      - | 1411 | `		 * is why the verdict comes from the VFS rather than from the value. */` |
|      3 | 1412 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      3 | 1413 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1414 | `		return PH7_OK;` |
|      - | 1415 | `	}` |
|      - | 1416 | `	/* IO return value */` |
|      7 | 1417 | `	ph7_result_int64(pCtx,iTime);` |
|      7 | 1418 | `	return PH7_OK;` |
|      5 | 1419 | `}` |
|      - | 1420 | `/*` |
|      - | 1421 | ` * int64 filemtime(string $filename)` |
|      - | 1422 | ` *  Gets file modification time.` |
|      - | 1423 | ` * Parameters` |
|      - | 1424 | ` *  $filename` |
|      - | 1425 | ` *   Path to the file.` |
|      - | 1426 | ` * Return` |
|      - | 1427 | ` *  File mtime on success or FALSE on failure.` |
|      - | 1428 | ` */` |
|     26 | 1429 | `static int PH7_vfs_file_mtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1430 | `{` |
|      - | 1431 | `	const char *zPath;` |
|      - | 1432 | `	ph7_int64 iTime;` |
|      - | 1433 | `	ph7_vfs *pVfs;` |
|     29 | 1434 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1435 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1436 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1437 | `		return PH7_OK;` |
|      - | 1438 | `	}` |
|      - | 1439 | `	/* Point to the underlying vfs */` |
|     29 | 1440 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     29 | 1441 | `	if( pVfs == 0 \|\| pVfs->xFileMtime == 0 ){` |
|      - | 1442 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1443 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1444 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1445 | `			ph7_function_name(pCtx)` |
|      - | 1446 | `			);` |
|    ! 0 | 1447 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1448 | `		return PH7_OK;` |
|      - | 1449 | `	}` |
|      - | 1450 | `	/* Point to the desired directory */` |
|     29 | 1451 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1452 | `	/* Perform the requested operation */` |
|     29 | 1453 | `	iTime = pVfs->xFileMtime(zPath);` |
|     29 | 1454 | `	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){` |
|      - | 1455 | `		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --` |
|      - | 1456 | `		 * truthy, and one second before the epoch is also a real timestamp, which` |
|      - | 1457 | `		 * is why the verdict comes from the VFS rather than from the value. */` |
|      3 | 1458 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      3 | 1459 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1460 | `		return PH7_OK;` |
|      - | 1461 | `	}` |
|      - | 1462 | `	/* IO return value */` |
|     27 | 1463 | `	ph7_result_int64(pCtx,iTime);` |
|     27 | 1464 | `	return PH7_OK;` |
|     15 | 1465 | `}` |
|      - | 1466 | `/*` |
|      - | 1467 | ` * int64 filectime(string $filename)` |
|      - | 1468 | ` *  Gets inode change time of file.` |
|      - | 1469 | ` * Parameters` |
|      - | 1470 | ` *  $filename` |
|      - | 1471 | ` *   Path to the file.` |
|      - | 1472 | ` * Return` |
|      - | 1473 | ` *  File ctime on success or FALSE on failure.` |
|      - | 1474 | ` */` |
|      6 | 1475 | `static int PH7_vfs_file_ctime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1476 | `{` |
|      - | 1477 | `	const char *zPath;` |
|      - | 1478 | `	ph7_int64 iTime;` |
|      - | 1479 | `	ph7_vfs *pVfs;` |
|      7 | 1480 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1481 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1482 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1483 | `		return PH7_OK;` |
|      - | 1484 | `	}` |
|      - | 1485 | `	/* Point to the underlying vfs */` |
|      7 | 1486 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 1487 | `	if( pVfs == 0 \|\| pVfs->xFileCtime == 0 ){` |
|      - | 1488 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1489 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1490 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1491 | `			ph7_function_name(pCtx)` |
|      - | 1492 | `			);` |
|    ! 0 | 1493 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1494 | `		return PH7_OK;` |
|      - | 1495 | `	}` |
|      - | 1496 | `	/* Point to the desired directory */` |
|      7 | 1497 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1498 | `	/* Perform the requested operation */` |
|      7 | 1499 | `	iTime = pVfs->xFileCtime(zPath);` |
|      7 | 1500 | `	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){` |
|      - | 1501 | `		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --` |
|      - | 1502 | `		 * truthy, and one second before the epoch is also a real timestamp, which` |
|      - | 1503 | `		 * is why the verdict comes from the VFS rather than from the value. */` |
|      3 | 1504 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      3 | 1505 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1506 | `		return PH7_OK;` |
|      - | 1507 | `	}` |
|      - | 1508 | `	/* IO return value */` |
|      5 | 1509 | `	ph7_result_int64(pCtx,iTime);` |
|      5 | 1510 | `	return PH7_OK;` |
|      4 | 1511 | `}` |
|      - | 1512 | `/*` |
|      - | 1513 | ` * bool is_file(string $filename)` |
|      - | 1514 | ` *  Tells whether the filename is a regular file.` |
|      - | 1515 | ` * Parameters` |
|      - | 1516 | ` *  $filename` |
|      - | 1517 | ` *   Path to the file.` |
|      - | 1518 | ` * Return` |
|      - | 1519 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1520 | ` */` |
|   8130 | 1521 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1522 | `{` |
|      - | 1523 | `	const char *zPath;` |
|      - | 1524 | `	ph7_vfs *pVfs;` |
|      - | 1525 | `	int rc;` |
|   8135 | 1526 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1527 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1528 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1529 | `		return PH7_OK;` |
|      - | 1530 | `	}` |
|      - | 1531 | `	/* Point to the underlying vfs */` |
|   8135 | 1532 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   8135 | 1533 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - | 1534 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1535 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1536 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1537 | `			ph7_function_name(pCtx)` |
|      - | 1538 | `			);` |
|    ! 0 | 1539 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1540 | `		return PH7_OK;` |
|      - | 1541 | `	}` |
|      - | 1542 | `	/* Point to the desired directory */` |
|   8135 | 1543 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1544 | `	/* Perform the requested operation */` |
|   8135 | 1545 | `	rc = pVfs->xIsfile(zPath);` |
|      - | 1546 | `	/* IO return value */` |
|   8135 | 1547 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   8135 | 1548 | `	return PH7_OK;` |
|   4070 | 1549 | `}` |
|      - | 1550 | `/*` |
|      - | 1551 | ` * bool is_link(string $filename)` |
|      - | 1552 | ` *  Tells whether the filename is a symbolic link.` |
|      - | 1553 | ` * Parameters` |
|      - | 1554 | ` *  $filename` |
|      - | 1555 | ` *   Path to the file.` |
|      - | 1556 | ` * Return` |
|      - | 1557 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1558 | ` */` |
|     12 | 1559 | `static int PH7_vfs_is_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1560 | `{` |
|      - | 1561 | `	const char *zPath;` |
|      - | 1562 | `	ph7_vfs *pVfs;` |
|      - | 1563 | `	int rc;` |
|     12 | 1564 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1565 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1566 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1567 | `		return PH7_OK;` |
|      - | 1568 | `	}` |
|      - | 1569 | `	/* Point to the underlying vfs */` |
|     12 | 1570 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     12 | 1571 | `	if( pVfs == 0 \|\| pVfs->xIslink == 0 ){` |
|      - | 1572 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1573 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1574 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1575 | `			ph7_function_name(pCtx)` |
|      - | 1576 | `			);` |
|    ! 0 | 1577 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1578 | `		return PH7_OK;` |
|      - | 1579 | `	}` |
|      - | 1580 | `	/* Point to the desired directory */` |
|     12 | 1581 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1582 | `	/* Perform the requested operation */` |
|     12 | 1583 | `	rc = pVfs->xIslink(zPath);` |
|      - | 1584 | `	/* IO return value */` |
|     12 | 1585 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     12 | 1586 | `	return PH7_OK;` |
|      6 | 1587 | `}` |
|      - | 1588 | `/*` |
|      - | 1589 | ` * bool is_readable(string $filename)` |
|      - | 1590 | ` *  Tells whether a file exists and is readable.` |
|      - | 1591 | ` * Parameters` |
|      - | 1592 | ` *  $filename` |
|      - | 1593 | ` *   Path to the file.` |
|      - | 1594 | ` * Return` |
|      - | 1595 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1596 | ` */` |
|      2 | 1597 | `static int PH7_vfs_is_readable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1598 | `{` |
|      - | 1599 | `	const char *zPath;` |
|      - | 1600 | `	ph7_vfs *pVfs;` |
|      - | 1601 | `	int rc;` |
|      3 | 1602 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1603 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1604 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1605 | `		return PH7_OK;` |
|      - | 1606 | `	}` |
|      - | 1607 | `	/* Point to the underlying vfs */` |
|      3 | 1608 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1609 | `	if( pVfs == 0 \|\| pVfs->xReadable == 0 ){` |
|      - | 1610 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1611 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1612 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1613 | `			ph7_function_name(pCtx)` |
|      - | 1614 | `			);` |
|    ! 0 | 1615 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1616 | `		return PH7_OK;` |
|      - | 1617 | `	}` |
|      - | 1618 | `	/* Point to the desired directory */` |
|      3 | 1619 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1620 | `	/* Perform the requested operation */` |
|      3 | 1621 | `	rc = pVfs->xReadable(zPath);` |
|      - | 1622 | `	/* IO return value */` |
|      3 | 1623 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 1624 | `	return PH7_OK;` |
|      2 | 1625 | `}` |
|      - | 1626 | `/*` |
|      - | 1627 | ` * bool is_writable(string $filename)` |
|      - | 1628 | ` *  Tells whether the filename is writable.` |
|      - | 1629 | ` * Parameters` |
|      - | 1630 | ` *  $filename` |
|      - | 1631 | ` *   Path to the file.` |
|      - | 1632 | ` * Return` |
|      - | 1633 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1634 | ` */` |
|      4 | 1635 | `static int PH7_vfs_is_writable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1636 | `{` |
|      - | 1637 | `	const char *zPath;` |
|      - | 1638 | `	ph7_vfs *pVfs;` |
|      - | 1639 | `	int rc;` |
|      5 | 1640 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1641 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1642 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1643 | `		return PH7_OK;` |
|      - | 1644 | `	}` |
|      - | 1645 | `	/* Point to the underlying vfs */` |
|      5 | 1646 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1647 | `	if( pVfs == 0 \|\| pVfs->xWritable == 0 ){` |
|      - | 1648 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1649 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1650 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1651 | `			ph7_function_name(pCtx)` |
|      - | 1652 | `			);` |
|    ! 0 | 1653 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1654 | `		return PH7_OK;` |
|      - | 1655 | `	}` |
|      - | 1656 | `	/* Point to the desired directory */` |
|      5 | 1657 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1658 | `	/* Perform the requested operation */` |
|      5 | 1659 | `	rc = pVfs->xWritable(zPath);` |
|      - | 1660 | `	/* IO return value */` |
|      5 | 1661 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 1662 | `	return PH7_OK;` |
|      3 | 1663 | `}` |
|      - | 1664 | `/*` |
|      - | 1665 | ` * bool is_executable(string $filename)` |
|      - | 1666 | ` *  Tells whether the filename is executable.` |
|      - | 1667 | ` * Parameters` |
|      - | 1668 | ` *  $filename` |
|      - | 1669 | ` *   Path to the file.` |
|      - | 1670 | ` * Return` |
|      - | 1671 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1672 | ` */` |
|      4 | 1673 | `static int PH7_vfs_is_executable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1674 | `{` |
|      - | 1675 | `	const char *zPath;` |
|      - | 1676 | `	ph7_vfs *pVfs;` |
|      - | 1677 | `	int rc;` |
|      6 | 1678 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1679 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1680 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1681 | `		return PH7_OK;` |
|      - | 1682 | `	}` |
|      - | 1683 | `	/* Point to the underlying vfs */` |
|      6 | 1684 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      6 | 1685 | `	if( pVfs == 0 \|\| pVfs->xExecutable == 0 ){` |
|      - | 1686 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1687 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1688 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1689 | `			ph7_function_name(pCtx)` |
|      - | 1690 | `			);` |
|    ! 0 | 1691 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1692 | `		return PH7_OK;` |
|      - | 1693 | `	}` |
|      - | 1694 | `	/* Point to the desired directory */` |
|      6 | 1695 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1696 | `	/* Perform the requested operation */` |
|      6 | 1697 | `	rc = pVfs->xExecutable(zPath);` |
|      - | 1698 | `	/* IO return value */` |
|      6 | 1699 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 | 1700 | `	return PH7_OK;` |
|      4 | 1701 | `}` |
|      - | 1702 | `/*` |
|      - | 1703 | ` * string filetype(string $filename)` |
|      - | 1704 | ` *  Gets file type.` |
|      - | 1705 | ` * Parameters` |
|      - | 1706 | ` *  $filename` |
|      - | 1707 | ` *   Path to the file.` |
|      - | 1708 | ` * Return` |
|      - | 1709 | ` *  The type of the file. Possible values are fifo, char, dir, block, link` |
|      - | 1710 | ` *  file, socket and unknown.` |
|      - | 1711 | ` */` |
|     18 | 1712 | `static int PH7_vfs_filetype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1713 | `{` |
|      - | 1714 | `	const char *zPath;` |
|      - | 1715 | `	ph7_vfs *pVfs;` |
|     19 | 1716 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1717 | `		/* Missing/Invalid argument,return 'unknown' */` |
|    ! 0 | 1718 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 1719 | `		return PH7_OK;` |
|      - | 1720 | `	}` |
|      - | 1721 | `	/* Point to the underlying vfs */` |
|     19 | 1722 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     19 | 1723 | `	if( pVfs == 0 \|\| pVfs->xFiletype == 0 ){` |
|      - | 1724 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1725 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1726 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1727 | `			ph7_function_name(pCtx)` |
|      - | 1728 | `			);` |
|    ! 0 | 1729 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1730 | `		return PH7_OK;` |
|      - | 1731 | `	}` |
|      - | 1732 | `	/* Point to the desired directory */` |
|     19 | 1733 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1734 | `	/* Set the empty string as the default return value */` |
|     19 | 1735 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1736 | `	/* Perform the requested operation */` |
|     19 | 1737 | `	if( pVfs->xFiletype(zPath,pCtx) != PH7_OK ){` |
|      - | 1738 | `		/* php LSTATs here (which is why a symlink answers "link") and a failure is` |
|      - | 1739 | ``		 * the `Lstat failed for` warning plus FALSE. PHL answered the string`` |
|      - | 1740 | `		 * "unknown" -- a real return value of this function, so a caller could not` |
|      - | 1741 | `		 * tell a missing path from a socket or a fifo. */` |
|      3 | 1742 | `		VfsThrowStatWarning(pCtx,zPath,1);` |
|      3 | 1743 | `		ph7_result_bool(pCtx,0);` |
|      1 | 1744 | `	}` |
|     19 | 1745 | `	return PH7_OK;` |
|     10 | 1746 | `}` |
|      - | 1747 | `/*` |
|      - | 1748 | ` * array stat(string $filename)` |
|      - | 1749 | ` *  Gives information about a file.` |
|      - | 1750 | ` * Parameters` |
|      - | 1751 | ` *  $filename` |
|      - | 1752 | ` *   Path to the file.` |
|      - | 1753 | ` * Return` |
|      - | 1754 | ` *  An associative array on success holding the following entries on success` |
|      - | 1755 | ` *  0   dev     device number` |
|      - | 1756 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1757 | ` * 2    mode    inode protection mode` |
|      - | 1758 | ` * 3    nlink   number of links` |
|      - | 1759 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1760 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1761 | ` * 6    rdev    device type, if inode device` |
|      - | 1762 | ` * 7    size    size in bytes` |
|      - | 1763 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1764 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1765 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1766 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1767 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1768 | ` * Note:` |
|      - | 1769 | ` *  FALSE is returned on failure.` |
|      - | 1770 | ` */` |
|     16 | 1771 | `static int PH7_vfs_stat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1772 | `{` |
|      - | 1773 | `	ph7_value *pArray,*pValue;` |
|      - | 1774 | `	const char *zPath;` |
|      - | 1775 | `	ph7_vfs *pVfs;` |
|      - | 1776 | `	int rc;` |
|     17 | 1777 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1778 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1779 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1780 | `		return PH7_OK;` |
|      - | 1781 | `	}` |
|      - | 1782 | `	/* Point to the underlying vfs */` |
|     17 | 1783 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     17 | 1784 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|      - | 1785 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1786 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1787 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1788 | `			ph7_function_name(pCtx)` |
|      - | 1789 | `			);` |
|    ! 0 | 1790 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1791 | `		return PH7_OK;` |
|      - | 1792 | `	}` |
|      - | 1793 | `	/* Create the array and the working value */` |
|     17 | 1794 | `	pArray = ph7_context_new_array(pCtx);` |
|     17 | 1795 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     17 | 1796 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1797 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1798 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1799 | `		return PH7_OK;` |
|      - | 1800 | `	}` |
|      - | 1801 | `	/* Extract the file path */` |
|     17 | 1802 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1803 | `	/* Perform the requested operation */` |
|     17 | 1804 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|     17 | 1805 | `	if( rc != PH7_OK ){` |
|      - | 1806 | ``		/* php warns before answering FALSE -- the same `stat failed for` /`` |
|      - | 1807 | ``		 * `Lstat failed for` line the rest of the family raises. PHL returned the`` |
|      - | 1808 | `		 * FALSE in silence, so a missing path and an empty result looked alike. */` |
|      3 | 1809 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      3 | 1810 | `		ph7_result_bool(pCtx,0);` |
|      2 | 1811 | `	}else{` |
|      - | 1812 | `		/* php's answer is the thirteen fields TWICE: numeric 0..12 then named. */` |
|     15 | 1813 | `		ph7_value *pFull = ph7_context_new_array(pCtx);` |
|     15 | 1814 | `		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){` |
|     15 | 1815 | `			ph7_result_value(pCtx,pFull);` |
|      8 | 1816 | `		}else{` |
|    ! 0 | 1817 | `			ph7_result_value(pCtx,pArray);` |
|      - | 1818 | `		}` |
|      - | 1819 | `	}` |
|      - | 1820 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1821 | `	 * automatically as soon we return from this function. */` |
|     17 | 1822 | `	return PH7_OK;` |
|      9 | 1823 | `}` |
|      - | 1824 | `/*` |
|      - | 1825 | ` * array lstat(string $filename)` |
|      - | 1826 | ` *  Gives information about a file or symbolic link.` |
|      - | 1827 | ` * Parameters` |
|      - | 1828 | ` *  $filename` |
|      - | 1829 | ` *   Path to the file.` |
|      - | 1830 | ` * Return` |
|      - | 1831 | ` *  An associative array on success holding the following entries on success` |
|      - | 1832 | ` *  0   dev     device number` |
|      - | 1833 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1834 | ` * 2    mode    inode protection mode` |
|      - | 1835 | ` * 3    nlink   number of links` |
|      - | 1836 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1837 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1838 | ` * 6    rdev    device type, if inode device` |
|      - | 1839 | ` * 7    size    size in bytes` |
|      - | 1840 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1841 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1842 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1843 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1844 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1845 | ` * Note:` |
|      - | 1846 | ` *  FALSE is returned on failure.` |
|      - | 1847 | ` */` |
|      6 | 1848 | `static int PH7_vfs_lstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1849 | `{` |
|      - | 1850 | `	ph7_value *pArray,*pValue;` |
|      - | 1851 | `	const char *zPath;` |
|      - | 1852 | `	ph7_vfs *pVfs;` |
|      - | 1853 | `	int rc;` |
|      7 | 1854 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1855 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1856 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1857 | `		return PH7_OK;` |
|      - | 1858 | `	}` |
|      - | 1859 | `	/* Point to the underlying vfs */` |
|      7 | 1860 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 1861 | `	if( pVfs == 0 \|\| pVfs->xlStat == 0 ){` |
|      - | 1862 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1863 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1864 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1865 | `			ph7_function_name(pCtx)` |
|      - | 1866 | `			);` |
|    ! 0 | 1867 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1868 | `		return PH7_OK;` |
|      - | 1869 | `	}` |
|      - | 1870 | `	/* Create the array and the working value */` |
|      7 | 1871 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 1872 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      7 | 1873 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1874 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1875 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1876 | `		return PH7_OK;` |
|      - | 1877 | `	}` |
|      - | 1878 | `	/* Extract the file path */` |
|      7 | 1879 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1880 | `	/* Perform the requested operation */` |
|      7 | 1881 | `	rc = pVfs->xlStat(zPath,pArray,pValue);` |
|      7 | 1882 | `	if( rc != PH7_OK ){` |
|      - | 1883 | ``		/* php warns before answering FALSE -- the same `stat failed for` /`` |
|      - | 1884 | ``		 * `Lstat failed for` line the rest of the family raises. PHL returned the`` |
|      - | 1885 | `		 * FALSE in silence, so a missing path and an empty result looked alike. */` |
|      3 | 1886 | `		VfsThrowStatWarning(pCtx,zPath,1);` |
|      3 | 1887 | `		ph7_result_bool(pCtx,0);` |
|      2 | 1888 | `	}else{` |
|      - | 1889 | `		/* php's answer is the thirteen fields TWICE: numeric 0..12 then named. */` |
|      5 | 1890 | `		ph7_value *pFull = ph7_context_new_array(pCtx);` |
|      5 | 1891 | `		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){` |
|      5 | 1892 | `			ph7_result_value(pCtx,pFull);` |
|      3 | 1893 | `		}else{` |
|    ! 0 | 1894 | `			ph7_result_value(pCtx,pArray);` |
|      - | 1895 | `		}` |
|      - | 1896 | `	}` |
|      - | 1897 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1898 | `	 * automatically as soon we return from this function. */` |
|      7 | 1899 | `	return PH7_OK;` |
|      4 | 1900 | `}` |
|      - | 1901 | `/*` |
|      - | 1902 | ` * int\|false fileowner / filegroup / fileinode / fileperms (string $filename)` |
|      - | 1903 | ` *  One stat() with one of its fields taken out of it, which is exactly how php` |
|      - | 1904 | ` *  implements them (php_stat's FS_OWNER / FS_GROUP / FS_INODE / FS_PERMS arms).` |
|      - | 1905 | ` *` |
|      - | 1906 | ` * They were prelude PHP wrapping stat(), which cost them php's diagnostic twice` |
|      - | 1907 | ` * over: three of the four said NOTHING on a failed stat (the fourth raised its own` |
|      - | 1908 | `` * `trigger_error`, so its errno was E_USER_WARNING's 512 rather than E_WARNING's 2`` |
|      - | 1909 | ` * and its line was the prelude's, not the caller's). In C the family shares one` |
|      - | 1910 | ` * warning site with the rest of stat(), and the four get real signature rows.` |
|      - | 1911 | ` */` |
|     32 | 1912 | `static int VfsStatField(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zField)` |
|      1 | 1913 | `{` |
|      - | 1914 | `	ph7_value *pArray,*pValue,*pField;` |
|      - | 1915 | `	const char *zPath;` |
|      - | 1916 | `	ph7_vfs *pVfs;` |
|      - | 1917 | `	int rc;` |
|     33 | 1918 | `	if( nArg < 1 ){` |
|    ! 0 | 1919 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1920 | `		return PH7_OK;` |
|      - | 1921 | `	}` |
|     33 | 1922 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     33 | 1923 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|    ! 0 | 1924 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1925 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1926 | `			ph7_function_name(pCtx)` |
|      - | 1927 | `			);` |
|    ! 0 | 1928 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1929 | `		return PH7_OK;` |
|      - | 1930 | `	}` |
|     33 | 1931 | `	pArray = ph7_context_new_array(pCtx);` |
|     33 | 1932 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     33 | 1933 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1934 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1935 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1936 | `		return PH7_OK;` |
|      - | 1937 | `	}` |
|     33 | 1938 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|     33 | 1939 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|     33 | 1940 | `	if( rc != PH7_OK ){` |
|      9 | 1941 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      9 | 1942 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1943 | `		return PH7_OK;` |
|      - | 1944 | `	}` |
|     25 | 1945 | `	pField = ph7_array_fetch(pArray,zField,-1);` |
|     25 | 1946 | `	if( pField == 0 ){` |
|      - | 1947 | `		/* The VFS answered a stat array without this field: nothing to report but` |
|      - | 1948 | `		 * the failure itself, which is what php answers when its own stat has no` |
|      - | 1949 | `		 * such member either. */` |
|    ! 0 | 1950 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1951 | `		return PH7_OK;` |
|      - | 1952 | `	}` |
|     25 | 1953 | `	ph7_result_int64(pCtx,ph7_value_to_int64(pField));` |
|     25 | 1954 | `	return PH7_OK;` |
|     17 | 1955 | `}` |
|      6 | 1956 | `static int PH7_vfs_file_owner(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1957 | `{` |
|      7 | 1958 | `	return VfsStatField(pCtx,nArg,apArg,"uid");` |
|      1 | 1959 | `}` |
|      4 | 1960 | `static int PH7_vfs_file_group(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1961 | `{` |
|      5 | 1962 | `	return VfsStatField(pCtx,nArg,apArg,"gid");` |
|      1 | 1963 | `}` |
|      4 | 1964 | `static int PH7_vfs_file_inode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1965 | `{` |
|      5 | 1966 | `	return VfsStatField(pCtx,nArg,apArg,"ino");` |
|      1 | 1967 | `}` |
|     18 | 1968 | `static int PH7_vfs_file_perms(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1969 | `{` |
|     19 | 1970 | `	return VfsStatField(pCtx,nArg,apArg,"mode");` |
|      1 | 1971 | `}` |
|      - | 1972 | `/*` |
|      - | 1973 | ` * array\|string\|false getenv(?string $name = null, bool $local_only = false)` |
|      - | 1974 | ` *  Gets the value of an environment variable.` |
|      - | 1975 | ` * Parameters` |
|      - | 1976 | ` *  $name` |
|      - | 1977 | ` *   The variable name -- or NOTHING, which is the documented way to ask for the` |
|      - | 1978 | ` *   WHOLE environment as a name => value array. That form answered FALSE here,` |
|      - | 1979 | `` *   so `foreach (getenv() as $k => $v)` iterated over a bool.`` |
|      - | 1980 | ` *  $local_only` |
|      - | 1981 | ` *   Ask only the process's own environment rather than the SAPI's. On the CLI` |
|      - | 1982 | ` *   they are the same environment, so the argument selects the same answer --` |
|      - | 1983 | ` *   but it must still be ACCEPTED, and asking for the whole map with it set` |
|      - | 1984 | ` *   answered false too.` |
|      - | 1985 | ` * Return` |
|      - | 1986 | ` *  The value of the environment variable, or FALSE when it does not exist, or` |
|      - | 1987 | ` *  the whole environment when no name is given.` |
|      - | 1988 | ` */` |
|     98 | 1989 | `static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1990 | `{` |
|      - | 1991 | `	const char *zEnv;` |
|      - | 1992 | `	ph7_vfs *pVfs;` |
|      - | 1993 | `	int iLen;` |
|      - | 1994 | `	/* Point to the underlying vfs */` |
|    102 | 1995 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    102 | 1996 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|      - | 1997 | `		/* The whole environment. xEnviron was APPENDED to ph7_vfs, so an` |
|      - | 1998 | `		 * embedder VFS built against version 2 does not have the field at all --` |
|      - | 1999 | `		 * reading it would run off the end of their struct. */` |
|      8 | 2000 | `		if( pVfs == 0 \|\| pVfs->iVersion < 3 \|\| pVfs->xEnviron == 0` |
|      9 | 2001 | `		 \|\| pVfs->xEnviron(pCtx) != PH7_OK ){` |
|    ! 0 | 2002 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2003 | `		}` |
|      9 | 2004 | `		return PH7_OK;` |
|      - | 2005 | `	}` |
|     94 | 2006 | `	if( pVfs == 0 \|\| pVfs->xGetenv == 0 ){` |
|      - | 2007 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2008 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2009 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2010 | `			ph7_function_name(pCtx)` |
|      - | 2011 | `			);` |
|    ! 0 | 2012 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2013 | `		return PH7_OK;` |
|      - | 2014 | `	}` |
|      - | 2015 | `	/* Extract the environment variable */` |
|     94 | 2016 | `	zEnv = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 2017 | `	/* Set a boolean FALSE as the default return value */` |
|     94 | 2018 | `	ph7_result_bool(pCtx,0);` |
|     94 | 2019 | `	if( iLen < 1 ){` |
|      - | 2020 | `		/* Empty string */` |
|      3 | 2021 | `		return PH7_OK;` |
|      - | 2022 | `	}` |
|      - | 2023 | `	/* Perform the requested operation */` |
|     92 | 2024 | `	pVfs->xGetenv(zEnv,pCtx);` |
|     92 | 2025 | `	return PH7_OK;` |
|     53 | 2026 | `}` |
|      - | 2027 | `/*` |
|      - | 2028 | ` * bool putenv(string $settings)` |
|      - | 2029 | ` *  Set the value of an environment variable.` |
|      - | 2030 | ` * Parameters` |
|      - | 2031 | ` *  $setting` |
|      - | 2032 | ` *   The setting, like "FOO=BAR"` |
|      - | 2033 | ` * Return` |
|      - | 2034 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2035 | ` */` |
|     54 | 2036 | `static int PH7_vfs_putenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2037 | `{` |
|      - | 2038 | `	const char *zName,*zValue;` |
|      - | 2039 | `	char *zSettings,*zEnd;` |
|      - | 2040 | `	ph7_vfs *pVfs;` |
|      - | 2041 | `	int iLen,rc;` |
|     55 | 2042 | `	if( nArg < 1 ){` |
|      - | 2043 | `		/* Missing argument,return FALSE */` |
|    ! 0 | 2044 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2045 | `		return PH7_OK;` |
|      - | 2046 | `	}` |
|      - | 2047 | `	/* Extract the setting variable. It is NOT required to already BE a string:` |
|      - | 2048 | ``	 * the declared parameter is `string $assignment`, so php coerces an int or a`` |
|      - | 2049 | `	 * __toString() object first, where PH7 answered false and did nothing. */` |
|     55 | 2050 | `	zSettings = (char *)ph7_value_to_string(apArg[0],&iLen);` |
|     55 | 2051 | `	if( iLen < 1 \|\| zSettings[0] == '=' ){` |
|      - | 2052 | `		/* php's whole validity rule: an empty assignment, or one with no name in` |
|      - | 2053 | `		 * front of the '='. Everything else is accepted. */` |
|      9 | 2054 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2055 | `			"putenv(): Argument #1 ($assignment) must have a valid syntax");` |
|      - | 2056 | `	}` |
|      - | 2057 | `	/* Parse the setting. php looks for the '=' with strchr(), so an embedded NUL` |
|      - | 2058 | `	 * ENDS the search: putenv("FO\0O=BAR") finds no '=' at all and removes the` |
|      - | 2059 | `	 * variable named "FO" instead of setting one. */` |
|     47 | 2060 | `	zEnd = &zSettings[iLen];` |
|     47 | 2061 | `	zValue = 0;` |
|     47 | 2062 | `	zName = zSettings;` |
|    431 | 2063 | `	while( zSettings < zEnd && zSettings[0] != 0 ){` |
|    407 | 2064 | `		if( zSettings[0] == '=' ){` |
|      - | 2065 | `			/* Null terminate the name */` |
|     23 | 2066 | `			zSettings[0] = 0;` |
|     23 | 2067 | `			zValue = &zSettings[1];` |
|     23 | 2068 | `			break;` |
|      - | 2069 | `		}` |
|    385 | 2070 | `		zSettings++;` |
|      1 | 2071 | `	}` |
|      - | 2072 | ``	/* A missing '=' is not invalid syntax: `putenv("NAME")` REMOVES the variable,`` |
|      - | 2073 | `	 * which is the documented way to unset one, and PH7 read it as a failure and` |
|      - | 2074 | `	 * left the old value in place. An empty VALUE is a value too` |
|      - | 2075 | ``	 * (`putenv("NAME=")`), which the old `zValue >= zEnd` test rejected.`` |
|      - | 2076 | `	 * php does NOT touch $_ENV here: that array is the SAPI's startup snapshot,` |
|      - | 2077 | `	 * and a putenv() after it changes the process environment alone. PH7 wrote` |
|      - | 2078 | `	 * the pair into $_ENV as well, so a script could read back through $_ENV a` |
|      - | 2079 | `	 * variable php only exposes through getenv(). */` |
|      - | 2080 | `	/* Point to the underlying vfs */` |
|     47 | 2081 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     47 | 2082 | `	if( pVfs == 0 \|\| pVfs->xSetenv == 0 ){` |
|      - | 2083 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2084 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2085 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2086 | `			ph7_function_name(pCtx)` |
|      - | 2087 | `			);` |
|    ! 0 | 2088 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2089 | `		if( zValue ){` |
|    ! 0 | 2090 | `			zSettings[0] = '=';` |
|    ! 0 | 2091 | `		}` |
|    ! 0 | 2092 | `		return PH7_OK;` |
|      - | 2093 | `	}` |
|      - | 2094 | `	/* Perform the requested operation. A NULL value means REMOVE, and php reports` |
|      - | 2095 | `	 * TRUE for that whether or not the variable was there (or nameable) at all --` |
|      - | 2096 | `	 * only a failed SET is false. */` |
|     47 | 2097 | `	rc = pVfs->xSetenv(zName,zValue);` |
|     47 | 2098 | `	ph7_result_bool(pCtx,zValue == 0 \|\| rc == PH7_OK );` |
|     47 | 2099 | `	if( zValue ){` |
|      - | 2100 | `		/* Put back the '=' the name was terminated on. Without one, zSettings` |
|      - | 2101 | `		 * stopped on the terminator or on an embedded NUL, neither of which this` |
|      - | 2102 | `		 * routine wrote. */` |
|     23 | 2103 | `		zSettings[0] = '=';` |
|     11 | 2104 | `	}` |
|     47 | 2105 | `	return PH7_OK;` |
|     28 | 2106 | `}` |
|      - | 2107 | `/*` |
|      - | 2108 | ` * bool touch(string $filename[,int64 $time = time()[,int64 $atime]])` |
|      - | 2109 | ` *  Sets access and modification time of file.` |
|      - | 2110 | ` * Note: On windows` |
|      - | 2111 | ` *   If the file does not exists,it will not be created.` |
|      - | 2112 | ` * Parameters` |
|      - | 2113 | ` *  $filename` |
|      - | 2114 | ` *   The name of the file being touched.` |
|      - | 2115 | ` *  $time` |
|      - | 2116 | ` *   The touch time. If time is not supplied, the current system time is used.` |
|      - | 2117 | ` * $atime` |
|      - | 2118 | ` *   If present, the access time of the given filename is set to the value of atime.` |
|      - | 2119 | ` *   Otherwise, it is set to the value passed to the time parameter. If neither are` |
|      - | 2120 | ` *   present, the current system time is used.` |
|      - | 2121 | ` * Return` |
|      - | 2122 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2123 | `*/` |
|     26 | 2124 | `static int PH7_vfs_touch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2125 | `{` |
|      - | 2126 | `	ph7_int64 nTime,nAccess;` |
|      - | 2127 | `	const char *zFile;` |
|      - | 2128 | `	ph7_vfs *pVfs;` |
|      - | 2129 | `	int rc;` |
|     29 | 2130 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2131 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2132 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2133 | `		return PH7_OK;` |
|      - | 2134 | `	}` |
|      - | 2135 | `	/* Point to the underlying vfs */` |
|     29 | 2136 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     29 | 2137 | `	if( pVfs == 0 \|\| pVfs->xTouch == 0 ){` |
|      - | 2138 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2139 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2140 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2141 | `			ph7_function_name(pCtx)` |
|      - | 2142 | `			);` |
|    ! 0 | 2143 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2144 | `		return PH7_OK;` |
|      - | 2145 | `	}` |
|      - | 2146 | `	/* Resolve php's defaults HERE, so the driver only ever sees real timestamps: a` |
|      - | 2147 | ``	 * NEGATIVE stamp is perfectly legal to php (`touch($f, -100)` is 1969), so it`` |
|      - | 2148 | `	 * cannot double as the "not given" sentinel the drivers used to read it as. An` |
|      - | 2149 | `	 * omitted/null $mtime is NOW; an omitted/null $atime follows $mtime. $atime also` |
|      - | 2150 | `	 * used to be read from apArg[1] — the mtime — so touch($f, $m, $a) silently` |
|      - | 2151 | `	 * stamped the modification time onto both. */` |
|     29 | 2152 | `	zFile = ph7_value_to_string(apArg[0],0);` |
|     29 | 2153 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) && nArg > 1 && ph7_value_is_null(apArg[1]) ){` |
|    ! 0 | 2154 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2155 | `			"touch(): Argument #2 ($mtime) cannot be null when argument #3 ($atime) "` |
|      - | 2156 | `			"is an integer");` |
|      - | 2157 | `	}` |
|     29 | 2158 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     13 | 2159 | `		nTime = ph7_value_to_int64(apArg[1]);` |
|      8 | 2160 | `	}else{` |
|      - | 2161 | `		time_t tNow;` |
|     17 | 2162 | `		time(&tNow);` |
|     17 | 2163 | `		nTime = (ph7_int64)tNow;` |
|      - | 2164 | `	}` |
|     29 | 2165 | `	nAccess = nTime;` |
|     29 | 2166 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      3 | 2167 | `		nAccess = ph7_value_to_int64(apArg[2]);` |
|      1 | 2168 | `	}` |
|     29 | 2169 | `	rc = pVfs->xTouch(zFile,nTime,nAccess);` |
|      - | 2170 | `	/* IO result */` |
|     29 | 2171 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     29 | 2172 | `	return PH7_OK;` |
|     16 | 2173 | `}` |
|      - | 2174 | `/*` |
|      - | 2175 | ` * Path processing functions that do not need access to the VFS layer` |
|      - | 2176 | ` * Status:` |
|      - | 2177 | ` *    Stable.` |
|      - | 2178 | ` */` |
|      - | 2179 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2180 | `/*` |
|      - | 2181 | ` * string dirname(string $path)` |
|      - | 2182 |  |
|      - | 2183 | ` *  Returns parent directory's path.` |
|      - | 2184 | ` * Parameters` |
|      - | 2185 | ` * $path` |
|      - | 2186 | ` *  Target path.` |
|      - | 2187 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 2188 | ` *  In other environments, it is the forward slash (/).` |
|      - | 2189 | ` * Return` |
|      - | 2190 | ` *  The path of the parent directory. If there are no slashes in path, a dot ('.')` |
|      - | 2191 | ` *  is returned, indicating the current directory.` |
|      - | 2192 | ` */` |
|     94 | 2193 | `static int PH7_builtin_dirname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2194 | `{` |
|      - | 2195 | `	const char *zPath,*zDir;` |
|      - | 2196 | `	int iLen,iDirlen;` |
|     99 | 2197 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2198 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 2199 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2200 | `		return PH7_OK;` |
|      - | 2201 | `	}` |
|      - | 2202 | `	/* Point to the target path */` |
|     99 | 2203 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|     99 | 2204 | `	if( iLen < 1 ){` |
|      - | 2205 | `		/* php answers "" for the empty path, not "." */` |
|      3 | 2206 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2207 | `		return PH7_OK;` |
|      - | 2208 | `	}` |
|      - | 2209 | `	/* $levels (php 7.0) was ACCEPTED AND IGNORED, so dirname($p, 3) silently answered` |
|      - | 2210 | `	 * the one-level parent — the caller's own answer, one or more levels too deep. Each` |
|      - | 2211 | `	 * level re-runs php_dirname on the previous result and stops as soon as the answer` |
|      - | 2212 | `	 * stops moving (the filesystem root, or "." for a relative path), which is what php` |
|      - | 2213 | `	 * does; php also rejects a level below 1 outright. */` |
|     97 | 2214 | `	zDir = zPath;` |
|     97 | 2215 | `	iDirlen = iLen;` |
|     97 | 2216 | `	if( nArg > 1 ){` |
|     51 | 2217 | `		ph7_int64 nLevels = ph7_value_to_int64(apArg[1]);` |
|      - | 2218 | `		ph7_int64 i;` |
|     51 | 2219 | `		if( nLevels < 1 ){` |
|      7 | 2220 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2221 | `				"dirname(): Argument #2 ($levels) must be greater than or equal to 1");` |
|      - | 2222 | `		}` |
|    125 | 2223 | `		for( i = 0 ; i < nLevels ; ++i ){` |
|    105 | 2224 | `			int iPrevLen = iDirlen;` |
|    105 | 2225 | `			const char *zPrev = zDir;` |
|    105 | 2226 | `			zDir = PH7_ExtractDirName(zPrev,iPrevLen,&iDirlen);` |
|    105 | 2227 | `			if( iDirlen == iPrevLen && SyMemcmp(zDir,zPrev,(sxu32)iDirlen) == 0 ){` |
|     25 | 2228 | `				break; /* fixed point: "/" and "." are their own parents */` |
|      - | 2229 | `			}` |
|     41 | 2230 | `		}` |
|     23 | 2231 | `	}else{` |
|     47 | 2232 | `		zDir = PH7_ExtractDirName(zPath,iLen,&iDirlen);` |
|      - | 2233 | `	}` |
|      - | 2234 | `	/* Return directory name */` |
|     91 | 2235 | `	ph7_result_string(pCtx,zDir,iDirlen);` |
|     91 | 2236 | `	return PH7_OK;` |
|     52 | 2237 | `}` |
|      - | 2238 | `/*` |
|      - | 2239 | ` * string basename(string $path[, string $suffix ])` |
|      - | 2240 | ` *  Returns trailing name component of path.` |
|      - | 2241 | ` * Parameters` |
|      - | 2242 | ` * $path` |
|      - | 2243 | ` *  Target path.` |
|      - | 2244 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 2245 | ` *  In other environments, it is the forward slash (/).` |
|      - | 2246 | ` * $suffix` |
|      - | 2247 | ` *  If the name component ends in suffix this will also be cut off.` |
|      - | 2248 | ` * Return` |
|      - | 2249 | ` *  The base name of the given path.` |
|      - | 2250 | ` */` |
|     80 | 2251 | `static int PH7_builtin_basename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2252 | `{` |
|      - | 2253 | `	const char *zPath,*zBase;` |
|      - | 2254 | `	int iLen,nBase;` |
|     84 | 2255 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2256 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 2257 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2258 | `		return PH7_OK;` |
|      - | 2259 | `	}` |
|      - | 2260 | `	/* Point to the target path */` |
|     84 | 2261 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 2262 | `	/* php_basename, shared with pathinfo(): the hand-rolled walk this used to carry` |
|      - | 2263 | `	 * kept a leading separator on a single-component path (basename("/a") answered` |
|      - | 2264 | `	 * "/a", basename("/.") answered "/.") because it stopped one byte short. */` |
|     84 | 2265 | `	zBase = PH7_ExtractBaseName(zPath,iLen,&nBase);` |
|     84 | 2266 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 2267 | `		const char *zSuffix;` |
|      - | 2268 | `		int nSuffix;` |
|      - | 2269 | `		/* Strip suffix — php leaves the basename alone when it IS the suffix */` |
|      5 | 2270 | `		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);` |
|      4 | 2271 | `		if( nSuffix > 0 && nSuffix < nBase` |
|      5 | 2272 | `		 && SyMemcmp(&zBase[nBase - nSuffix],zSuffix,nSuffix) == 0 ){` |
|      5 | 2273 | `			nBase -= nSuffix;` |
|      2 | 2274 | `		}` |
|      2 | 2275 | `	}` |
|      - | 2276 | `	/* Store the basename */` |
|     84 | 2277 | `	ph7_result_string(pCtx,zBase,nBase);` |
|     84 | 2278 | `	return PH7_OK;` |
|     44 | 2279 | `}` |
|      - | 2280 | `/*` |
|      - | 2281 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 2282 | ` *  Returns information about a file path.` |
|      - | 2283 | ` * Parameter` |
|      - | 2284 | ` *  $path` |
|      - | 2285 | ` *   The path to be parsed.` |
|      - | 2286 | ` *  $options` |
|      - | 2287 | ` *    If present, specifies a specific element to be returned; one of` |
|      - | 2288 | ` *      PATHINFO_DIRNAME, PATHINFO_BASENAME, PATHINFO_EXTENSION or PATHINFO_FILENAME.` |
|      - | 2289 | ` * Return` |
|      - | 2290 | ` *  If the options parameter is not passed, an associative array containing the following` |
|      - | 2291 | ` *  elements is returned: dirname, basename, extension (if any), and filename.` |
|      - | 2292 | ` *  If options is present, returns a string containing the requested element.` |
|      - | 2293 | ` */` |
|      - | 2294 | `typedef struct path_info path_info;` |
|      - | 2295 | `struct path_info` |
|      - | 2296 | `{` |
|      - | 2297 | `	SyString sDir; /* Directory [i.e: /var/www] */` |
|      - | 2298 | `	SyString sBasename; /* Basename [i.e httpd.conf] */` |
|      - | 2299 | `	SyString sExtension; /* File extension [i.e xml,pdf..] */` |
|      - | 2300 | `	SyString sFilename;  /* Filename */` |
|      - | 2301 | `	int iPresent;        /* Which components php would EMIT (PH7_PATHINFO_* bits) */` |
|      - | 2302 | `};` |
|      - | 2303 | `/*` |
|      - | 2304 | ` * Extract path fields exactly as php's pathinfo() assembles them.` |
|      - | 2305 | ` *` |
|      - | 2306 | ` * Two things this has to get right beyond the values themselves:` |
|      - | 2307 | ` *` |
|      - | 2308 | ` *  - php looks for the LAST dot ANYWHERE in the basename, a leading one included,` |
|      - | 2309 | `` *    so `.bashrc` has extension "bashrc" and filename "" (PH7 stopped the scan`` |
|      - | 2310 | ` *    before the first byte, so it reported no extension and filename ".bashrc").` |
|      - | 2311 | ` *  - EMPTY is not the same as ABSENT. php always emits basename and filename when` |
|      - | 2312 | `` *    they are asked for, emits extension whenever a dot exists (even for `x.`,`` |
|      - | 2313 | ` *    whose extension is ""), and emits dirname only when it is non-empty. The` |
|      - | 2314 | ` *    scalar form answers with the first EMITTED component, so conflating the two` |
|      - | 2315 | `` *    makes `pathinfo("x.", PATHINFO_EXTENSION\|PATHINFO_FILENAME)` fall through to`` |
|      - | 2316 | ` *    the filename ("x") where php answers "" — iPresent keeps them apart.` |
|      - | 2317 | ` *` |
|      - | 2318 | ` * dirname and basename come from the shared php_dirname/php_basename helpers` |
|      - | 2319 | ` * rather than a third hand-rolled walk, so the trailing-separator and` |
|      - | 2320 | ` * relative-path rules ("file.txt" -> ".", "/var/www/" -> "/var" + "www") cannot` |
|      - | 2321 | ` * drift between the two builtins and this one.` |
|      - | 2322 | ` */` |
|  16120 | 2323 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 2324 | `{` |
|      - | 2325 | `	const char *zBase,*zDir,*zDot;` |
|      - | 2326 | `	int nBase,nDir,i;` |
|      - | 2327 | `	/* Zero the structure */` |
|  16125 | 2328 | `	SyZero(pOut,sizeof(path_info));` |
|  16125 | 2329 | `	zDir = PH7_ExtractDirName(zPath,nByte,&nDir);` |
|  16125 | 2330 | `	if( nDir > 0 ){` |
|  16121 | 2331 | `		SyStringInitFromBuf(&pOut->sDir,zDir,nDir);` |
|  16121 | 2332 | `		pOut->iPresent \|= PH7_PATHINFO_DIRNAME;` |
|   8058 | 2333 | `	}` |
|  16125 | 2334 | `	zBase = PH7_ExtractBaseName(zPath,nByte,&nBase);` |
|  16125 | 2335 | `	SyStringInitFromBuf(&pOut->sBasename,zBase,nBase);` |
|  16125 | 2336 | `	pOut->iPresent \|= PH7_PATHINFO_BASENAME\|PH7_PATHINFO_FILENAME;` |
|      - | 2337 | `	/* Last dot anywhere in the basename splits filename from extension */` |
|  16125 | 2338 | `	zDot = 0;` |
|  80543 | 2339 | `	for( i = nBase ; i > 0 ; --i ){` |
|  80521 | 2340 | `		if( zBase[i - 1] == '.' ){` |
|  16103 | 2341 | `			zDot = &zBase[i - 1];` |
|  16103 | 2342 | `			break;` |
|      - | 2343 | `		}` |
|  32214 | 2344 | `	}` |
|  16125 | 2345 | `	if( zDot ){` |
|  16103 | 2346 | `		SyStringInitFromBuf(&pOut->sExtension,zDot + 1,(int)(&zBase[nBase] - (zDot + 1)));` |
|  16103 | 2347 | `		pOut->iPresent \|= PH7_PATHINFO_EXTENSION;` |
|  16103 | 2348 | `		SyStringInitFromBuf(&pOut->sFilename,zBase,(int)(zDot - zBase));` |
|   8054 | 2349 | `	}else{` |
|     23 | 2350 | `		SyStringInitFromBuf(&pOut->sFilename,zBase,nBase);` |
|      - | 2351 | `	}` |
|  16125 | 2352 | `	return SXRET_OK;` |
|      5 | 2353 | `}` |
|      - | 2354 | `/*` |
|      - | 2355 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 2356 | ` *  See block comment above.` |
|      - | 2357 | ` */` |
|  16120 | 2358 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2359 | `{` |
|      - | 2360 | `	const char *zPath;` |
|      - | 2361 | `	path_info sInfo;` |
|      - | 2362 | `	int iLen;` |
|  16125 | 2363 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2364 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 2365 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2366 | `		return PH7_OK;` |
|      - | 2367 | `	}` |
|      - | 2368 | `	/* Point to the target path. The EMPTY path is not a special case: php still` |
|      - | 2369 | ``	 * answers with the array `["basename" => "", "filename" => ""]` (and "" for a`` |
|      - | 2370 | `	 * scalar request), where PH7 short-circuited to "" and returned the wrong TYPE. */` |
|  16125 | 2371 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 2372 | `	/* Extract path info */` |
|  16125 | 2373 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|      - | 2374 | ``	/* Read the mask at 64-bit width: ph7_value_to_int() truncates to `int`, so a`` |
|      - | 2375 | `	 * flags value congruent to PATHINFO_ALL mod 2^32 (4294967311, -4294967281 …)` |
|      - | 2376 | `	 * would take the ARRAY branch and answer with the wrong TYPE. */` |
|  16120 | 2377 | `	if( nArg > 1 && ph7_value_is_int(apArg[1])` |
|  24162 | 2378 | `	 && ph7_value_to_int64(apArg[1]) != (ph7_int64)PH7_PATHINFO_ALL ){` |
|      - | 2379 | `		/* $flags is a BITMASK, not an enum: php assembles the requested components in` |
|      - | 2380 | `		 * the fixed order below and, for anything other than PATHINFO_ALL, hands back` |
|      - | 2381 | `		 * the FIRST one it EMITTED (zend_hash_get_current_data on the fresh array).` |
|      - | 2382 | ``		 * So `PATHINFO_DIRNAME\|PATHINFO_BASENAME` answers the dirname, and an unknown`` |
|      - | 2383 | `		 * bit that happens to carry a known one along (99 = 1\|2\|32\|64) answers as if` |
|      - | 2384 | `		 * only the known ones were passed. PH7 numbered the components 1/2/3/4 and` |
|      - | 2385 | `		 * switched on the whole value, so it read a two-flag mask as a different single` |
|      - | 2386 | `		 * component and answered "" for everything else. Emission is iPresent, NOT` |
|      - | 2387 | `		 * "non-empty": an emitted-but-empty component ends the search with "". */` |
|  16107 | 2388 | `		ph7_int64 nComp = ph7_value_to_int64(apArg[1]);` |
|      - | 2389 | `		static const int aBit[4] = {` |
|      - | 2390 | `			PH7_PATHINFO_DIRNAME,PH7_PATHINFO_BASENAME,` |
|      - | 2391 | `			PH7_PATHINFO_EXTENSION,PH7_PATHINFO_FILENAME` |
|      - | 2392 | `		};` |
|      - | 2393 | `		SyString *apComp[4];` |
|      - | 2394 | `		int i;` |
|  16107 | 2395 | `		apComp[0] = &sInfo.sDir;` |
|  16107 | 2396 | `		apComp[1] = &sInfo.sBasename;` |
|  16107 | 2397 | `		apComp[2] = &sInfo.sExtension;` |
|  16107 | 2398 | `		apComp[3] = &sInfo.sFilename;` |
|      - | 2399 | `		/* Expand the empty string unless a requested component is emitted */` |
|  16107 | 2400 | `		ph7_result_string(pCtx,"",0);` |
|  56307 | 2401 | `		for( i = 0 ; i < 4 ; ++i ){` |
|  56299 | 2402 | `			if( (nComp & aBit[i]) == aBit[i] && (sInfo.iPresent & aBit[i]) ){` |
|  16099 | 2403 | `				ph7_result_string(pCtx,apComp[i]->zString,(int)apComp[i]->nByte);` |
|  16099 | 2404 | `				break;` |
|      - | 2405 | `			}` |
|  20105 | 2406 | `		}` |
|   8056 | 2407 | `	}else{` |
|      - | 2408 | `		/* Return an associative array */` |
|      - | 2409 | `		ph7_value *pArray,*pValue;` |
|     19 | 2410 | `		pArray = ph7_context_new_array(pCtx);` |
|     19 | 2411 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     19 | 2412 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2413 | `			/* Out of mem,return NULL */` |
|    ! 0 | 2414 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2415 | `			return PH7_OK;` |
|      - | 2416 | `		}` |
|      - | 2417 | ``		/* Emitted-but-EMPTY components are in the array too (php keys `basename` and`` |
|      - | 2418 | ``		 * `filename` for "/" with ""), so this walks iPresent, not the lengths. */`` |
|      - | 2419 | `		{` |
|      - | 2420 | `		static const char *azKey[4] = {"dirname","basename","extension","filename"};` |
|      - | 2421 | `		static const int aBit[4] = {` |
|      - | 2422 | `			PH7_PATHINFO_DIRNAME,PH7_PATHINFO_BASENAME,` |
|      - | 2423 | `			PH7_PATHINFO_EXTENSION,PH7_PATHINFO_FILENAME` |
|      - | 2424 | `		};` |
|      - | 2425 | `		SyString *apComp[4];` |
|      - | 2426 | `		int i;` |
|     19 | 2427 | `		apComp[0] = &sInfo.sDir;` |
|     19 | 2428 | `		apComp[1] = &sInfo.sBasename;` |
|     19 | 2429 | `		apComp[2] = &sInfo.sExtension;` |
|     19 | 2430 | `		apComp[3] = &sInfo.sFilename;` |
|     91 | 2431 | `		for( i = 0 ; i < 4 ; ++i ){` |
|     73 | 2432 | `			if( (sInfo.iPresent & aBit[i]) == 0 ){` |
|     11 | 2433 | `				continue;` |
|      - | 2434 | `			}` |
|     63 | 2435 | `			ph7_value_reset_string_cursor(pValue);` |
|     63 | 2436 | `			ph7_value_string(pValue,apComp[i]->zString,(int)apComp[i]->nByte);` |
|     63 | 2437 | `			ph7_array_add_strkey_elem(pArray,azKey[i],pValue); /* Will make it's own copy */` |
|     32 | 2438 | `		}` |
|      - | 2439 | `		}` |
|      - | 2440 | `		/* Return the created array */` |
|     19 | 2441 | `		ph7_result_value(pCtx,pArray);` |
|      - | 2442 | `		/* Don't worry about freeing memory, everything will be released` |
|      - | 2443 | `		 * automatically as soon we return from this foreign function.` |
|      - | 2444 | `		 */` |
|      - | 2445 | `	}` |
|  16125 | 2446 | `	return PH7_OK;` |
|   8065 | 2447 | `}` |
|      - | 2448 | `/* SPDX-SnippetBegin */` |
|      - | 2449 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 2450 | `/* SPDX-License-Identifier: blessing */` |
|      - | 2451 | `/*` |
|      - | 2452 | ` * Globbing implementation extracted from the sqlite3 source tree.` |
|      - | 2453 |  |
|      - | 2454 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|      - | 2455 | ` * Status: Public Domain` |
|      - | 2456 | ` */` |
|      - | 2457 | `typedef unsigned char u8;` |
|      - | 2458 | `/* An array to map all upper-case characters into their corresponding` |
|      - | 2459 | `** lower-case character.` |
|      - | 2460 | `**` |
|      - | 2461 | `** SQLite only considers US-ASCII (or EBCDIC) characters.  We do not` |
|      - | 2462 | `** handle case conversions for the UTF character set since the tables` |
|      - | 2463 | `** involved are nearly as big or bigger than SQLite itself.` |
|      - | 2464 | `*/` |
|      - | 2465 | `static const unsigned char sqlite3UpperToLower[] = {` |
|      - | 2466 | `      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,` |
|      - | 2467 | `     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,` |
|      - | 2468 | `     36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,` |
|      - | 2469 | `     54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,` |
|      - | 2470 | `    104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,` |
|      - | 2471 | `    122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,` |
|      - | 2472 | `    108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,` |
|      - | 2473 | `    126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,` |
|      - | 2474 | `    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,` |
|      - | 2475 | `    162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,` |
|      - | 2476 | `    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,` |
|      - | 2477 | `    198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,` |
|      - | 2478 | `    216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,` |
|      - | 2479 | `    234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,` |
|      - | 2480 | `    252,253,254,255` |
|      - | 2481 | `};` |
|      - | 2482 | `#define GlogUpperToLower(A)     if( A<0x80 ){ A = sqlite3UpperToLower[A]; }` |
|      - | 2483 | `/*` |
|      - | 2484 | `** Assuming zIn points to the first byte of a UTF-8 character,` |
|      - | 2485 | `** advance zIn to point to the first byte of the next UTF-8 character.` |
|      - | 2486 | `*/` |
|      - | 2487 | `#define SQLITE_SKIP_UTF8(zIn) {                        \` |
|      - | 2488 | `  if( (*(zIn++))>=0xc0 ){                              \` |
|      - | 2489 | `    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \` |
|      - | 2490 | `  }                                                    \` |
|      - | 2491 | `}` |
|      - | 2492 | `/*` |
|      - | 2493 | `** Compare two UTF-8 strings for equality where the first string can` |
|      - | 2494 | `** potentially be a "glob" expression.  Return true (1) if they` |
|      - | 2495 | `** are the same and false (0) if they are different.` |
|      - | 2496 | `**` |
|      - | 2497 | `** Globbing rules:` |
|      - | 2498 | `**` |
|      - | 2499 | `**      '*'       Matches any sequence of zero or more characters.` |
|      - | 2500 | `**` |
|      - | 2501 | `**      '?'       Matches exactly one character.` |
|      - | 2502 | `**` |
|      - | 2503 | `**     [...]      Matches one character from the enclosed list of` |
|      - | 2504 | `**                characters.` |
|      - | 2505 | `**` |
|      - | 2506 | `**     [^...]     Matches one character not in the enclosed list.` |
|      - | 2507 | `**` |
|      - | 2508 | `** With the [...] and [^...] matching, a ']' character can be included` |
|      - | 2509 | `** in the list by making it the first character after '[' or '^'.  A` |
|      - | 2510 | `** range of characters can be specified using '-'.  Example:` |
|      - | 2511 | `** "[a-z]" matches any single lower-case letter.  To match a '-', make` |
|      - | 2512 | `** it the last character in the list.` |
|      - | 2513 | `**` |
|      - | 2514 | `** This routine is usually quick, but can be N**2 in the worst case.` |
|      - | 2515 | `**` |
|      - | 2516 | `** Hints: to match '*' or '?', put them in "[]".  Like this:` |
|      - | 2517 | `**` |
|      - | 2518 | `**         abc[*]xyz        Matches "abc*xyz" only` |
|      - | 2519 | `*/` |
|    204 | 2520 | `static int patternCompare(` |
|      - | 2521 | `  const u8 *zPattern,              /* The glob pattern */` |
|      - | 2522 | `  const u8 *zString,               /* The string to compare against the glob */` |
|      - | 2523 | `  const int esc,                    /* The escape character */` |
|      - | 2524 | `  int noCase` |
|      3 | 2525 | `){` |
|      - | 2526 | `  int c, c2;` |
|      - | 2527 | `  int invert;` |
|      - | 2528 | `  int seen;` |
|    207 | 2529 | `  u8 matchOne = '?';` |
|    207 | 2530 | `  u8 matchAll = '*';` |
|    207 | 2531 | `  u8 matchSet = '[';` |
|    207 | 2532 | `  int prevEscape = 0;     /* True if the previous character was 'escape' */` |
|      - | 2533 |  |
|    207 | 2534 | `  if( !zPattern \|\| !zString ) return 0;` |
|    279 | 2535 | `  while( (c = PH7_Utf8Read(zPattern,0,&zPattern))!=0 ){` |
|    265 | 2536 | `    if( !prevEscape && c==matchAll ){` |
|    144 | 2537 | `      while( (c=PH7_Utf8Read(zPattern,0,&zPattern)) == matchAll` |
|     75 | 2538 | `               \|\| c == matchOne ){` |
|    ! 0 | 2539 | `        if( c==matchOne && PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 2540 | `          return 0;` |
|      - | 2541 | `        }` |
|    ! 0 | 2542 | `      }` |
|     75 | 2543 | `      if( c==0 ){` |
|     49 | 2544 | `        return 1;` |
|     27 | 2545 | `      }else if( c==esc ){` |
|    ! 0 | 2546 | `        c = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2547 | `        if( c==0 ){` |
|    ! 0 | 2548 | `          return 0;` |
|    ! 0 | 2549 | `        }` |
|     27 | 2550 | `      }else if( c==matchSet ){` |
|    ! 0 | 2551 | `	  if( (esc==0) \|\| (matchSet<0x80) ) return 0;` |
|    ! 0 | 2552 | `	  while( *zString && patternCompare(&zPattern[-1],zString,esc,noCase)==0 ){` |
|    ! 0 | 2553 | `          SQLITE_SKIP_UTF8(zString);` |
|    ! 0 | 2554 | `        }` |
|    ! 0 | 2555 | `        return *zString!=0;` |
|      - | 2556 | `      }` |
|     41 | 2557 | `      while( (c2 = PH7_Utf8Read(zString,0,&zString))!=0 ){` |
|     41 | 2558 | `        if( noCase ){` |
|      3 | 2559 | `          GlogUpperToLower(c2);` |
|      3 | 2560 | `          GlogUpperToLower(c);` |
|     11 | 2561 | `          while( c2 != 0 && c2 != c ){` |
|      9 | 2562 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      9 | 2563 | `            GlogUpperToLower(c2);` |
|      1 | 2564 | `          }` |
|      2 | 2565 | `        }else{` |
|    131 | 2566 | `          while( c2 != 0 && c2 != c ){` |
|     93 | 2567 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      1 | 2568 | `          }` |
|      - | 2569 | `        }` |
|     41 | 2570 | `        if( c2==0 ) return 0;` |
|     27 | 2571 | `		if( patternCompare(zPattern,zString,esc,noCase) ) return 1;` |
|      1 | 2572 | `      }` |
|    ! 0 | 2573 | `      return 0;` |
|    192 | 2574 | `    }else if( !prevEscape && c==matchOne ){` |
|    ! 0 | 2575 | `      if( PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 2576 | `        return 0;` |
|    ! 0 | 2577 | `      }` |
|    192 | 2578 | `    }else if( c==matchSet ){` |
|    ! 0 | 2579 | `      int prior_c = 0;` |
|    ! 0 | 2580 | `      if( esc == 0 ) return 0;` |
|    ! 0 | 2581 | `      seen = 0;` |
|    ! 0 | 2582 | `      invert = 0;` |
|    ! 0 | 2583 | `      c = PH7_Utf8Read(zString, 0, &zString);` |
|    ! 0 | 2584 | `      if( c==0 ) return 0;` |
|    ! 0 | 2585 | `      c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2586 | `      if( c2=='^' ){` |
|    ! 0 | 2587 | `        invert = 1;` |
|    ! 0 | 2588 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2589 | `      }` |
|    ! 0 | 2590 | `      if( c2==']' ){` |
|    ! 0 | 2591 | `        if( c==']' ) seen = 1;` |
|    ! 0 | 2592 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2593 | `      }` |
|    ! 0 | 2594 | `      while( c2 && c2!=']' ){` |
|    ! 0 | 2595 | `        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){` |
|    ! 0 | 2596 | `          c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2597 | `          if( c>=prior_c && c<=c2 ) seen = 1;` |
|    ! 0 | 2598 | `          prior_c = 0;` |
|    ! 0 | 2599 | `        }else{` |
|    ! 0 | 2600 | `          if( c==c2 ){` |
|    ! 0 | 2601 | `            seen = 1;` |
|    ! 0 | 2602 | `          }` |
|    ! 0 | 2603 | `          prior_c = c2;` |
|      - | 2604 | `        }` |
|    ! 0 | 2605 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2606 | `      }` |
|    ! 0 | 2607 | `      if( c2==0 \|\| (seen ^ invert)==0 ){` |
|    ! 0 | 2608 | `        return 0;` |
|    ! 0 | 2609 | `      }` |
|    192 | 2610 | `    }else if( esc==c && !prevEscape ){` |
|    ! 0 | 2611 | `      prevEscape = 1;` |
|    ! 0 | 2612 | `    }else{` |
|    192 | 2613 | `      c2 = PH7_Utf8Read(zString, 0, &zString);` |
|    192 | 2614 | `      if( noCase ){` |
|      7 | 2615 | `        GlogUpperToLower(c);` |
|      7 | 2616 | `        GlogUpperToLower(c2);` |
|      3 | 2617 | `      }` |
|    192 | 2618 | `      if( c!=c2 ){` |
|    119 | 2619 | `        return 0;` |
|      - | 2620 | `      }` |
|     74 | 2621 | `      prevEscape = 0;` |
|      - | 2622 | `    }` |
|      2 | 2623 | `  }` |
|     15 | 2624 | `  return *zString==0;` |
|    105 | 2625 | `}` |
|      - | 2626 | `/* SPDX-SnippetEnd */` |
|      - | 2627 | `/*` |
|      - | 2628 | ` * Wrapper around patternCompare() defined above.` |
|      - | 2629 | ` * See block comment above for more information.` |
|      - | 2630 | ` */` |
|    178 | 2631 | `static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,int CaseCompare)` |
|      3 | 2632 | `{` |
|      - | 2633 | `	int rc;` |
|    181 | 2634 | `	if( iEsc < 0 ){` |
|    ! 0 | 2635 | `		iEsc = '\\';` |
|    ! 0 | 2636 | `	}` |
|    181 | 2637 | `	rc = patternCompare(zPattern,zString,iEsc,CaseCompare);` |
|    181 | 2638 | `	return rc;` |
|      3 | 2639 | `}` |
|      - | 2640 | `/*` |
|      - | 2641 | ` * bool fnmatch(string $pattern,string $string[,int $flags = 0 ])` |
|      - | 2642 | ` *  Match filename against a pattern.` |
|      - | 2643 | ` * Parameters` |
|      - | 2644 | ` *  $pattern` |
|      - | 2645 | ` *   The shell wildcard pattern.` |
|      - | 2646 | ` * $string` |
|      - | 2647 | ` *  The tested string.` |
|      - | 2648 | ` * $flags` |
|      - | 2649 | ` *   A list of possible flags:` |
|      - | 2650 | ` *    FNM_NOESCAPE 	Disable backslash escaping.` |
|      - | 2651 | ` *    FNM_PATHNAME 	Slash in string only matches slash in the given pattern.` |
|      - | 2652 | ` *    FNM_PERIOD 	Leading period in string must be exactly matched by period in the given pattern.` |
|      - | 2653 | ` *    FNM_CASEFOLD 	Caseless match.` |
|      - | 2654 | ` * Return` |
|      - | 2655 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 2656 | ` */` |
|      8 | 2657 | `static int PH7_builtin_fnmatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2658 | `{` |
|      - | 2659 | `	const char *zString,*zPattern;` |
|      9 | 2660 | `	int iEsc = '\\';` |
|      9 | 2661 | `	int noCase = 0;` |
|      - | 2662 | `	int rc;` |
|      9 | 2663 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2664 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2665 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2666 | `		return PH7_OK;` |
|      - | 2667 | `	}` |
|      - | 2668 | `	/* Extract the pattern and the string */` |
|      9 | 2669 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|      9 | 2670 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2671 | `	/* Extract the flags if avaialble */` |
|      9 | 2672 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      7 | 2673 | `		rc = ph7_value_to_int(apArg[2]);` |
|      7 | 2674 | `		if( rc & 2 /*FNM_NOESCAPE (php value)*/){` |
|    ! 0 | 2675 | `			iEsc = 0;` |
|    ! 0 | 2676 | `		}` |
|      7 | 2677 | `		if( rc & 16 /*FNM_CASEFOLD (php value)*/){` |
|      3 | 2678 | `			noCase = 1;` |
|      1 | 2679 | `		}` |
|      3 | 2680 | `	}` |
|      - | 2681 | `	/* Go globbing */` |
|      9 | 2682 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,noCase);` |
|      - | 2683 | `	/* Globbing result */` |
|      9 | 2684 | `	ph7_result_bool(pCtx,rc);` |
|      9 | 2685 | `	return PH7_OK;` |
|      5 | 2686 | `}` |
|      - | 2687 | `/*` |
|      - | 2688 | ` * bool strglob(string $pattern,string $string)` |
|      - | 2689 | ` *  Match string against a pattern.` |
|      - | 2690 | ` * Parameters` |
|      - | 2691 | ` *  $pattern` |
|      - | 2692 | ` *   The shell wildcard pattern.` |
|      - | 2693 | ` * $string` |
|      - | 2694 | ` *  The tested string.` |
|      - | 2695 | ` * Return` |
|      - | 2696 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 2697 | ` * Note that this a symisc eXtension.` |
|      - | 2698 | ` */` |
|    170 | 2699 | `static int PH7_builtin_strglob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2700 | `{` |
|      - | 2701 | `	const char *zString,*zPattern;` |
|    173 | 2702 | `	int iEsc = '\\';` |
|      - | 2703 | `	int rc;` |
|    173 | 2704 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2705 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2706 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2707 | `		return PH7_OK;` |
|      - | 2708 | `	}` |
|      - | 2709 | `	/* Extract the pattern and the string */` |
|    173 | 2710 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|    173 | 2711 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2712 | `	/* Go globbing */` |
|    173 | 2713 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0);` |
|      - | 2714 | `	/* Globbing result */` |
|    173 | 2715 | `	ph7_result_bool(pCtx,rc);` |
|    173 | 2716 | `	return PH7_OK;` |
|     88 | 2717 | `}` |
|      - | 2718 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2719 | `/*` |
|      - | 2720 | ` * bool link(string $target,string $link)` |
|      - | 2721 |  |
|      - | 2722 | ` *  Create a hard link.` |
|      - | 2723 | ` * Parameters` |
|      - | 2724 | ` *  $target` |
|      - | 2725 | ` *   Target of the link.` |
|      - | 2726 | ` *  $link` |
|      - | 2727 | ` *   The link name.` |
|      - | 2728 | ` * Return` |
|      - | 2729 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2730 | ` */` |
|      2 | 2731 | `static int PH7_vfs_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2732 | `{` |
|      - | 2733 | `	const char *zTarget,*zLink;` |
|      - | 2734 | `	ph7_vfs *pVfs;` |
|      - | 2735 | `	int rc;` |
|      3 | 2736 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2737 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2738 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2739 | `		return PH7_OK;` |
|      - | 2740 | `	}` |
|      - | 2741 | `	/* Point to the underlying vfs */` |
|      3 | 2742 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2743 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2744 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2745 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2746 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2747 | `			ph7_function_name(pCtx)` |
|      - | 2748 | `			);` |
|    ! 0 | 2749 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2750 | `		return PH7_OK;` |
|      - | 2751 | `	}` |
|      - | 2752 | `	/* Extract the given arguments */` |
|      3 | 2753 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      3 | 2754 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2755 | `	/* Perform the requested operation */` |
|      3 | 2756 | `	rc = pVfs->xLink(zTarget,zLink,0/*Not a symbolic link */);` |
|      - | 2757 | `	/* IO result */` |
|      3 | 2758 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 2759 | `	return PH7_OK;` |
|      2 | 2760 | `}` |
|      - | 2761 | `/*` |
|      - | 2762 | ` * string\|false readlink(string $path)` |
|      - | 2763 | ` *  Returns the target of a symbolic link.` |
|      - | 2764 | ` * Parameters` |
|      - | 2765 | ` *  $path` |
|      - | 2766 | ` *   The symbolic link path.` |
|      - | 2767 | ` * Return` |
|      - | 2768 | ` *  The contents of the link, or FALSE (with a warning) when $path is not a link` |
|      - | 2769 | ` *  or cannot be read -- php's own answer, error text included.` |
|      - | 2770 | ` */` |
|      8 | 2771 | `static int PH7_vfs_readlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 2772 | `{` |
|      - | 2773 | `	const char *zPath;` |
|      - | 2774 | `	ph7_vfs *pVfs;` |
|      - | 2775 | `	int rc;` |
|      8 | 2776 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    ! 0 | 2777 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2778 | `		return PH7_OK;` |
|      - | 2779 | `	}` |
|      8 | 2780 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      8 | 2781 | `	if( pVfs == 0 \|\| pVfs->xReadlink == 0 ){` |
|    ! 0 | 2782 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2783 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2784 | `			ph7_function_name(pCtx)` |
|      - | 2785 | `			);` |
|    ! 0 | 2786 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2787 | `		return PH7_OK;` |
|      - | 2788 | `	}` |
|      8 | 2789 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      8 | 2790 | `	rc = pVfs->xReadlink(zPath,pCtx);` |
|      8 | 2791 | `	if( rc != PH7_OK ){` |
|      - | 2792 | `		/* php's wording is the errno text alone -- the engine prefixes the` |
|      - | 2793 | `		 * function name already. */` |
|      6 | 2794 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      4 | 2795 | `			"%s",VfsStrerror(errno));` |
|      4 | 2796 | `		ph7_result_bool(pCtx,0);` |
|      2 | 2797 | `	}` |
|      8 | 2798 | `	return PH7_OK;` |
|      4 | 2799 | `}` |
|      - | 2800 | `/*` |
|      - | 2801 | ` * bool symlink(string $target,string $link)` |
|      - | 2802 | ` *  Creates a symbolic link.` |
|      - | 2803 | ` * Parameters` |
|      - | 2804 | ` *  $target` |
|      - | 2805 | ` *   Target of the link.` |
|      - | 2806 | ` *  $link` |
|      - | 2807 | ` *   The link name.` |
|      - | 2808 | ` * Return` |
|      - | 2809 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2810 | ` */` |
|     10 | 2811 | `static int PH7_vfs_symlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2812 | `{` |
|      - | 2813 | `	const char *zTarget,*zLink;` |
|      - | 2814 | `	ph7_vfs *pVfs;` |
|      - | 2815 | `	int rc;` |
|     11 | 2816 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2817 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2818 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2819 | `		return PH7_OK;` |
|      - | 2820 | `	}` |
|      - | 2821 | `	/* Point to the underlying vfs */` |
|     11 | 2822 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 2823 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 2824 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2825 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2826 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2827 | `			ph7_function_name(pCtx)` |
|      - | 2828 | `			);` |
|    ! 0 | 2829 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2830 | `		return PH7_OK;` |
|      - | 2831 | `	}` |
|      - | 2832 | `	/* Extract the given arguments */` |
|     11 | 2833 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|     11 | 2834 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 2835 | `	/* Perform the requested operation */` |
|     11 | 2836 | `	rc = pVfs->xLink(zTarget,zLink,1/*A symbolic link */);` |
|      - | 2837 | `	/* IO result */` |
|     11 | 2838 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|     11 | 2839 | `	return PH7_OK;` |
|      6 | 2840 | `}` |
|      - | 2841 | `/*` |
|      - | 2842 | ` * int umask([ int $mask ])` |
|      - | 2843 | ` *  Changes the current umask.` |
|      - | 2844 | ` * Parameters` |
|      - | 2845 | ` *  $mask` |
|      - | 2846 | ` *   The new umask.` |
|      - | 2847 | ` * Return` |
|      - | 2848 | ` *  umask() without arguments simply returns the current umask.` |
|      - | 2849 | ` *  Otherwise the old umask is returned.` |
|      - | 2850 | ` */` |
|      8 | 2851 | `static int PH7_vfs_umask(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2852 | `{` |
|      - | 2853 | `	int iOld,iNew;` |
|      - | 2854 | `	ph7_vfs *pVfs;` |
|      - | 2855 | `	/* Point to the underlying vfs */` |
|      9 | 2856 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 | 2857 | `	if( pVfs == 0 \|\| pVfs->xUmask == 0 ){` |
|      - | 2858 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2859 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2860 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2861 | `			ph7_function_name(pCtx)` |
|      - | 2862 | `			);` |
|    ! 0 | 2863 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2864 | `		return PH7_OK;` |
|      - | 2865 | `	}` |
|      9 | 2866 | `	iNew = 0;` |
|      9 | 2867 | `	if( nArg > 0 ){` |
|      5 | 2868 | `		iNew = ph7_value_to_int(apArg[0]);` |
|      2 | 2869 | `	}` |
|      - | 2870 | `	/* Perform the requested operation */` |
|      9 | 2871 | `	iOld = pVfs->xUmask(iNew);` |
|      - | 2872 | `	/* Old mask */` |
|      9 | 2873 | `	ph7_result_int(pCtx,iOld);` |
|      9 | 2874 | `	return PH7_OK;` |
|      5 | 2875 | `}` |
|      - | 2876 | `/*` |
|      - | 2877 | ` * string sys_get_temp_dir()` |
|      - | 2878 | ` *  Returns directory path used for temporary files.` |
|      - | 2879 | ` * Parameters` |
|      - | 2880 | ` *  None` |
|      - | 2881 | ` * Return` |
|      - | 2882 | ` *  Returns the path of the temporary directory.` |
|      - | 2883 | ` */` |
|    440 | 2884 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2885 | `{` |
|      - | 2886 | `	ph7_vfs *pVfs;` |
|      - | 2887 | `	/* Set the empty string as the default return value */` |
|    445 | 2888 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2889 | `	/* Point to the underlying vfs */` |
|    445 | 2890 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    445 | 2891 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
|    ! 0 | 2892 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2893 | `		SXUNUSED(apArg);` |
|      - | 2894 | `		/* IO routine not implemented,return "" */` |
|    ! 0 | 2895 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2896 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2897 | `			ph7_function_name(pCtx)` |
|      - | 2898 | `			);` |
|    ! 0 | 2899 | `		return PH7_OK;` |
|      - | 2900 | `	}` |
|      - | 2901 | `	/* Perform the requested operation */` |
|    445 | 2902 | `	pVfs->xTempDir(pCtx);` |
|    445 | 2903 | `	return PH7_OK;` |
|    225 | 2904 | `}` |
|      - | 2905 | `/*` |
|      - | 2906 | ` * string get_current_user()` |
|      - | 2907 | ` *  Returns the name of the current working user.` |
|      - | 2908 | ` * Parameters` |
|      - | 2909 | ` *  None` |
|      - | 2910 | ` * Return` |
|      - | 2911 | ` *  Returns the name of the current working user.` |
|      - | 2912 | ` */` |
|      2 | 2913 | `static int PH7_vfs_get_current_user(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2914 | `{` |
|      - | 2915 | `	ph7_vfs *pVfs;` |
|      - | 2916 | `	/* Point to the underlying vfs */` |
|      3 | 2917 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2918 | `	if( pVfs == 0 \|\| pVfs->xUsername == 0 ){` |
|    ! 0 | 2919 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2920 | `		SXUNUSED(apArg);` |
|      - | 2921 | `		/* IO routine not implemented */` |
|    ! 0 | 2922 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2923 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2924 | `			ph7_function_name(pCtx)` |
|      - | 2925 | `			);` |
|      - | 2926 | `		/* Set a dummy username */` |
|    ! 0 | 2927 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 2928 | `		return PH7_OK;` |
|      - | 2929 | `	}` |
|      - | 2930 | `	/* Perform the requested operation */` |
|      3 | 2931 | `	pVfs->xUsername(pCtx);` |
|      3 | 2932 | `	return PH7_OK;` |
|      2 | 2933 | `}` |
|      - | 2934 | `/*` |
|      - | 2935 | ` * int64 getmypid()` |
|      - | 2936 | ` *  Gets process ID.` |
|      - | 2937 | ` * Parameters` |
|      - | 2938 | ` *  None` |
|      - | 2939 | ` * Return` |
|      - | 2940 | ` *  Returns the process ID.` |
|      - | 2941 | ` */` |
|    230 | 2942 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2943 | `{` |
|      - | 2944 | `	ph7_int64 nProcessId;` |
|      - | 2945 | `	ph7_vfs *pVfs;` |
|      - | 2946 | `	/* Point to the underlying vfs */` |
|    235 | 2947 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    235 | 2948 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
|    ! 0 | 2949 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2950 | `		SXUNUSED(apArg);` |
|      - | 2951 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2952 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2953 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2954 | `			ph7_function_name(pCtx)` |
|      - | 2955 | `			);` |
|    ! 0 | 2956 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2957 | `		return PH7_OK;` |
|      - | 2958 | `	}` |
|      - | 2959 | `	/* Perform the requested operation */` |
|    235 | 2960 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 2961 | `	/* Set the result */` |
|    235 | 2962 | `	ph7_result_int64(pCtx,nProcessId);` |
|    235 | 2963 | `	return PH7_OK;` |
|    120 | 2964 | `}` |
|      - | 2965 | `/*` |
|      - | 2966 | ` * int getmyuid()` |
|      - | 2967 | ` *  Get user ID.` |
|      - | 2968 | ` * Parameters` |
|      - | 2969 | ` *  None` |
|      - | 2970 | ` * Return` |
|      - | 2971 | ` *  Returns the user ID.` |
|      - | 2972 | ` */` |
|      4 | 2973 | `static int PH7_vfs_getmyuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2974 | `{` |
|      - | 2975 | `	ph7_vfs *pVfs;` |
|      - | 2976 | `	int nUid;` |
|      - | 2977 | `	/* Point to the underlying vfs */` |
|      5 | 2978 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 2979 | `	if( pVfs == 0 \|\| pVfs->xUid == 0 ){` |
|    ! 0 | 2980 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 2981 | `		SXUNUSED(apArg);` |
|      - | 2982 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 2983 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2984 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 2985 | `			ph7_function_name(pCtx)` |
|      - | 2986 | `			);` |
|    ! 0 | 2987 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2988 | `		return PH7_OK;` |
|      - | 2989 | `	}` |
|      - | 2990 | `	/* Perform the requested operation */` |
|      5 | 2991 | `	nUid = pVfs->xUid();` |
|      - | 2992 | `	/* Set the result */` |
|      5 | 2993 | `	ph7_result_int(pCtx,nUid);` |
|      5 | 2994 | `	return PH7_OK;` |
|      3 | 2995 | `}` |
|      - | 2996 | `/*` |
|      - | 2997 | ` * int getmygid()` |
|      - | 2998 | ` *  Get group ID.` |
|      - | 2999 | ` * Parameters` |
|      - | 3000 | ` *  None` |
|      - | 3001 | ` * Return` |
|      - | 3002 | ` *  Returns the group ID.` |
|      - | 3003 | ` */` |
|      2 | 3004 | `static int PH7_vfs_getmygid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3005 | `{` |
|      - | 3006 | `	ph7_vfs *pVfs;` |
|      - | 3007 | `	int nGid;` |
|      - | 3008 | `	/* Point to the underlying vfs */` |
|      3 | 3009 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 3010 | `	if( pVfs == 0 \|\| pVfs->xGid == 0 ){` |
|    ! 0 | 3011 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 3012 | `		SXUNUSED(apArg);` |
|      - | 3013 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 3014 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3015 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 3016 | `			ph7_function_name(pCtx)` |
|      - | 3017 | `			);` |
|    ! 0 | 3018 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 3019 | `		return PH7_OK;` |
|      - | 3020 | `	}` |
|      - | 3021 | `	/* Perform the requested operation */` |
|      3 | 3022 | `	nGid = pVfs->xGid();` |
|      - | 3023 | `	/* Set the result */` |
|      3 | 3024 | `	ph7_result_int(pCtx,nGid);` |
|      3 | 3025 | `	return PH7_OK;` |
|      2 | 3026 | `}` |
|      - | 3027 | `#ifdef __WINNT__` |
|      - | 3028 | `#include <Windows.h>` |
|      - | 3029 | `#elif defined(__UNIXES__)` |
|      - | 3030 | `#include <sys/utsname.h>` |
|      - | 3031 | `#endif` |
|      - | 3032 | `/*` |
|      - | 3033 | ` * string php_uname([ string $mode = "a" ])` |
|      - | 3034 | ` *  Returns information about the host operating system.` |
|      - | 3035 | ` * Parameters` |
|      - | 3036 | ` *  $mode` |
|      - | 3037 | ` *   mode is a single character that defines what information is returned:` |
|      - | 3038 | ` *    'a': This is the default. Contains all modes in the sequence "s n r v m".` |
|      - | 3039 | ` *    's': Operating system name. eg. FreeBSD.` |
|      - | 3040 | ` *    'n': Host name. eg. localhost.example.com.` |
|      - | 3041 | ` *    'r': Release name. eg. 5.1.2-RELEASE.` |
|      - | 3042 | ` *    'v': Version information. Varies a lot between operating systems.` |
|      - | 3043 | ` *    'm': Machine type. eg. i386.` |
|      - | 3044 | ` * Return` |
|      - | 3045 | ` *  OS description as a string.` |
|      - | 3046 | ` */` |
|      4 | 3047 | `static int PH7_vfs_ph7_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3048 | `{` |
|      - | 3049 | `#if defined(__WINNT__)` |
|      1 | 3050 | `	const char *zName = "Microsoft Windows";` |
|      - | 3051 | `	OSVERSIONINFOW sVer;` |
|      - | 3052 | `#elif defined(__UNIXES__)` |
|      - | 3053 | `	struct utsname sName;` |
|      - | 3054 | `#endif` |
|      5 | 3055 | `	const char *zMode = "a";` |
|      5 | 3056 | `	if( nArg > 0 && ph7_value_is_string(apArg[0]) ){` |
|      - | 3057 | `		/* Extract the desired mode */` |
|    ! 0 | 3058 | `		zMode = ph7_value_to_string(apArg[0],0);` |
|    ! 0 | 3059 | `	}` |
|      - | 3060 | `#if defined(__WINNT__)` |
|      1 | 3061 | `	sVer.dwOSVersionInfoSize = sizeof(sVer);` |
|      - | 3062 | `	/* GetVersionExW is deprecated in modern MSVC. Suppress deprecation for this call. */` |
|      - | 3063 | `#if defined(_MSC_VER)` |
|      - | 3064 | `#pragma warning(push)` |
|      - | 3065 | `#pragma warning(disable:4996)` |
|      - | 3066 | `#endif` |
|      1 | 3067 | `	if( TRUE != GetVersionExW(&sVer)){` |
|      - | 3068 | `#if defined(_MSC_VER)` |
|      - | 3069 | `#pragma warning(pop)` |
|      - | 3070 | `#endif` |
|    ! 0 | 3071 | `		ph7_result_string(pCtx,zName,-1);` |
|    ! 0 | 3072 | `		return PH7_OK;` |
|      - | 3073 | `	}` |
|      1 | 3074 | `	if( sVer.dwPlatformId == VER_PLATFORM_WIN32_NT ){` |
|      1 | 3075 | `		if( sVer.dwMajorVersion <= 4 ){` |
|    ! 0 | 3076 | `			zName = "Microsoft Windows NT";` |
|      1 | 3077 | `		}else if( sVer.dwMajorVersion == 5 ){` |
|    ! 0 | 3078 | `			switch(sVer.dwMinorVersion){` |
|    ! 0 | 3079 | `				case 0:	zName = "Microsoft Windows 2000"; break;` |
|    ! 0 | 3080 | `				case 1: zName = "Microsoft Windows XP";   break;` |
|    ! 0 | 3081 | `				case 2: zName = "Microsoft Windows Server 2003"; break;` |
|      - | 3082 | `			}` |
|    ! 0 | 3083 | `		}else if( sVer.dwMajorVersion == 6){` |
|      1 | 3084 | `				switch(sVer.dwMinorVersion){` |
|    ! 0 | 3085 | `					case 0: zName = "Microsoft Windows Vista"; break;` |
|    ! 0 | 3086 | `					case 1: zName = "Microsoft Windows 7"; break;` |
|      1 | 3087 | `					case 2: zName = "Microsoft Windows Server 2008"; break;` |
|    ! 0 | 3088 | `					case 3: zName = "Microsoft Windows 8"; break;` |
|      - | 3089 | `					default: break;` |
|      - | 3090 | `				}` |
|      - | 3091 | `		}` |
|      - | 3092 | `	}` |
|      1 | 3093 | `	switch(zMode[0]){` |
|      - | 3094 | `	case 's':` |
|      - | 3095 | `		/* Operating system name */` |
|    ! 0 | 3096 | `		ph7_result_string(pCtx,zName,-1/* Compute length automatically*/);` |
|    ! 0 | 3097 | `		break;` |
|      - | 3098 | `	case 'n':` |
|      - | 3099 | `		/* Host name */` |
|    ! 0 | 3100 | `		ph7_result_string(pCtx,"localhost",(int)sizeof("localhost")-1);` |
|    ! 0 | 3101 | `		break;` |
|      - | 3102 | `	case 'r':` |
|      - | 3103 | `	case 'v':` |
|      - | 3104 | `		/* Version information. */` |
|    ! 0 | 3105 | `		ph7_result_string_format(pCtx,"%u.%u build %u",` |
|      - | 3106 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 3107 | `			);` |
|    ! 0 | 3108 | `		break;` |
|      - | 3109 | `	case 'm':` |
|      - | 3110 | `		/* Machine name */` |
|    ! 0 | 3111 | `		ph7_result_string(pCtx,"x86",(int)sizeof("x86")-1);` |
|    ! 0 | 3112 | `		break;` |
|      - | 3113 | `	default:` |
|      1 | 3114 | `		ph7_result_string_format(pCtx,"%s localhost %u.%u build %u x86",` |
|      - | 3115 | `			zName,` |
|      - | 3116 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 3117 | `			);` |
|      - | 3118 | `		break;` |
|      - | 3119 | `	}` |
|      - | 3120 | `#elif defined(__UNIXES__)` |
|      4 | 3121 | `	if( uname(&sName) != 0 ){` |
|    ! 0 | 3122 | `		ph7_result_string(pCtx,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 | 3123 | `		return PH7_OK;` |
|      - | 3124 | `	}` |
|      4 | 3125 | `	switch(zMode[0]){` |
|    ! 0 | 3126 | `	case 's':` |
|      - | 3127 | `		/* Operating system name */` |
|    ! 0 | 3128 | `		ph7_result_string(pCtx,sName.sysname,-1/* Compute length automatically*/);` |
|    ! 0 | 3129 | `		break;` |
|    ! 0 | 3130 | `	case 'n':` |
|      - | 3131 | `		/* Host name */` |
|    ! 0 | 3132 | `		ph7_result_string(pCtx,sName.nodename,-1/* Compute length automatically*/);` |
|    ! 0 | 3133 | `		break;` |
|    ! 0 | 3134 | `	case 'r':` |
|      - | 3135 | `		/* Release information */` |
|    ! 0 | 3136 | `		ph7_result_string(pCtx,sName.release,-1/* Compute length automatically*/);` |
|    ! 0 | 3137 | `		break;` |
|    ! 0 | 3138 | `	case 'v':` |
|      - | 3139 | `		/* Version information. */` |
|    ! 0 | 3140 | `		ph7_result_string(pCtx,sName.version,-1/* Compute length automatically*/);` |
|    ! 0 | 3141 | `		break;` |
|    ! 0 | 3142 | `	case 'm':` |
|      - | 3143 | `		/* Machine name */` |
|    ! 0 | 3144 | `		ph7_result_string(pCtx,sName.machine,-1/* Compute length automatically*/);` |
|    ! 0 | 3145 | `		break;` |
|      2 | 3146 | `	default:` |
|      6 | 3147 | `		ph7_result_string_format(pCtx,` |
|      - | 3148 | `			"%s %s %s %s %s",` |
|      2 | 3149 | `			sName.sysname,` |
|      2 | 3150 | `			sName.release,` |
|      2 | 3151 | `			sName.version,` |
|      2 | 3152 | `			sName.nodename,` |
|      2 | 3153 | `			sName.machine` |
|      - | 3154 | `			);` |
|      4 | 3155 | `		break;` |
|      - | 3156 | `	}` |
|      - | 3157 | `#else` |
|      - | 3158 | `	ph7_result_string(pCtx,"Unknown Operating System",(int)sizeof("Unknown Operating System")-1);` |
|      - | 3159 | `#endif` |
|      5 | 3160 | `	return PH7_OK;` |
|      3 | 3161 | `}` |
|      - | 3162 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 3163 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 3164 | `#if defined(_MSC_VER)` |
|      - | 3165 | `static const ph7_vfs null_vfs = {` |
|      - | 3166 | `#else` |
|      - | 3167 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 3168 | `#endif` |
|      - | 3169 | `	"null_vfs",` |
|      - | 3170 | `	PH7_VFS_VERSION,` |
|      - | 3171 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 3172 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 3173 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 3174 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 3175 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 3176 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 3177 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 3178 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 3179 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 3180 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 3181 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 3182 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 3183 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 3184 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 3185 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 3186 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 3187 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 3188 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 3189 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 3190 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 3191 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 3192 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 3193 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 3194 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 3195 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 3196 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 3197 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 3198 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 3199 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 3200 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 3201 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 3202 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 3203 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 3204 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 3205 | `	0, /* int (*xUmask)(int) */` |
|      - | 3206 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 3207 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 3208 | `	0, /* int (*xUid)(void) */` |
|      - | 3209 | `	0, /* int (*xGid)(void) */` |
|      - | 3210 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 3211 | `	0, /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 3212 | `	0, /* int (*xReadlink)(const char *,ph7_context *) */` |
|      - | 3213 | `	0  /* int (*xEnviron)(ph7_context *) */` |
|      - | 3214 | `};` |
|      - | 3215 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 3216 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 3217 | `/*` |
|      - | 3218 | ` * Export the builtin vfs.` |
|      - | 3219 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 3220 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 3221 | ` * Note:` |
|      - | 3222 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 3223 | ` * Note:` |
|      - | 3224 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 3225 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 3226 | ` */` |
|   5148 | 3227 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 3228 | `{` |
|      - | 3229 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 3230 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 3231 | `	return &null_vfs;` |
|      - | 3232 | `#else` |
|      - | 3233 | `#ifdef __WINNT__` |
|      5 | 3234 | `	return &sWinVfs;` |
|      - | 3235 | `#elif defined(__UNIXES__)` |
|   5148 | 3236 | `	return &sUnixVfs;` |
|      - | 3237 | `#else` |
|      - | 3238 | `	return &null_vfs;` |
|      - | 3239 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 3240 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 3241 | `#else` |
|      - | 3242 | `	return &null_vfs;` |
|      - | 3243 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 3244 | `}` |
|      - | 3245 | `/*` |
|      - | 3246 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 3247 | ` * [i.e: file://,php://].` |
|      - | 3248 | ` * Note:` |
|      - | 3249 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 3250 | ` *  defined then this function is a no-op.` |
|      - | 3251 | ` */` |
|   4552 | 3252 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 3253 | `{` |
|      - | 3254 | `	/*` |
|      - | 3255 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 3256 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 3257 | `	 */` |
|      - | 3258 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 3259 | `	/* VFS: disk I/O related functions */` |
|      - | 3260 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 3261 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 3262 | `#ifndef __WINNT__` |
|      - | 3263 | `		/* php declares chroot() on POSIX only — there is no such call on Windows,` |
|      - | 3264 | ``		 * so `function_exists('chroot')` is FALSE there and the name is free for a`` |
|      - | 3265 | `		 * script to define. PHL used to declare it on both and answer a` |
|      - | 3266 | `		 * "not implemented in the underlying VFS" warning + false on Windows,` |
|      - | 3267 | `		 * which is a different thing from php's undefined function. (chown/chgrp/` |
|      - | 3268 | `		 * link/symlink/readlink stay: php declares all five on Windows.) */` |
|      - | 3269 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 3270 | `#endif` |
|      - | 3271 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 3272 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 3273 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 3274 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 3275 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 3276 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 3277 | `		/* php's own resolver, and the question include/require answer silently:` |
|      - | 3278 | `		 * it walks the same include_path in the same order, so it belongs beside` |
|      - | 3279 | `		 * realpath() rather than with the stream builtins. */` |
|      - | 3280 | `		{"stream_resolve_include_path",PH7_vfs_stream_resolve_include_path},` |
|      - | 3281 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 3282 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 3283 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 3284 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 3285 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 3286 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 3287 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 3288 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 3289 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 3290 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 3291 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 3292 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 3293 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 3294 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 3295 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 3296 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 3297 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 3298 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 3299 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 3300 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 3301 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 3302 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 3303 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 3304 | `		{"fileowner",   PH7_vfs_file_owner},` |
|      - | 3305 | `		{"filegroup",   PH7_vfs_file_group},` |
|      - | 3306 | `		{"fileinode",   PH7_vfs_file_inode},` |
|      - | 3307 | `		{"fileperms",   PH7_vfs_file_perms},` |
|      - | 3308 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 3309 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 3310 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 3311 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 3312 | `		{"link",        PH7_vfs_link     },` |
|      - | 3313 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 3314 | `		{"readlink",    PH7_vfs_readlink },` |
|      - | 3315 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 3316 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 3317 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 3318 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 3319 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 3320 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 3321 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 3322 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 3323 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 3324 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 3325 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 3326 | `	};` |
|      - | 3327 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 3328 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 3329 | `	 */` |
|      - | 3330 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 3331 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 3332 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 3333 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 3334 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 3335 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 3336 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 3337 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 3338 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 3339 | `		{"stream_get_line", PH7_builtin_stream_get_line },` |
|      - | 3340 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 3341 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 3342 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 3343 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 3344 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 3345 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 3346 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 3347 | `		/* php's dir() lives with opendir(), which is what it calls and what its` |
|      - | 3348 | `		 * failure warning is worded by. Registering it here also means the TINY` |
|      - | 3349 | `		 * build drops BOTH: the prelude copy was defined there and fataled on` |
|      - | 3350 | `		 * "Call to undefined function opendir()" the moment it was called. */` |
|      - | 3351 | `		{"dir",       PH7_builtin_dir },` |
|      - | 3352 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 3353 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 3354 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 3355 | `		{"file",      PH7_builtin_file   },` |
|      - | 3356 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 3357 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 3358 | `		{"stream_isatty", PH7_builtin_stream_isatty },` |
|      - | 3359 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 3360 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 3361 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 3362 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 3363 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 3364 | `		{"stream_get_contents",  PH7_builtin_stream_get_contents },` |
|      - | 3365 | `		{"stream_get_wrappers",  PH7_builtin_stream_get_wrappers },` |
|      - | 3366 | `		{"stream_get_meta_data", PH7_builtin_stream_get_meta_data },` |
|      - | 3367 | `		/* php's own alias, kept from the days sockets had a separate API. */` |
|      - | 3368 | `		{"socket_get_status",    PH7_builtin_stream_get_meta_data },` |
|      - | 3369 | `		{"stream_set_blocking",  PH7_builtin_stream_set_blocking },` |
|      - | 3370 | `		{"socket_set_blocking",  PH7_builtin_stream_set_blocking },` |
|      - | 3371 | `		{"stream_set_timeout",   PH7_builtin_stream_set_timeout },` |
|      - | 3372 | `		{"stream_set_chunk_size",PH7_builtin_stream_set_chunk_size },` |
|      - | 3373 | `		{"stream_set_read_buffer",  PH7_builtin_stream_set_read_buffer },` |
|      - | 3374 | `		{"stream_set_write_buffer", PH7_builtin_stream_set_write_buffer },` |
|      - | 3375 | `		{"set_file_buffer",         PH7_builtin_stream_set_write_buffer },` |
|      - | 3376 | `		{"stream_supports_lock", PH7_builtin_stream_supports_lock },` |
|      - | 3377 | `		{"stream_is_local",      PH7_builtin_stream_is_local },` |
|      - | 3378 | `		{"stream_copy_to_stream",PH7_builtin_stream_copy_to_stream },` |
|      - | 3379 | `		{"stream_get_transports",PH7_builtin_stream_get_transports },` |
|      - | 3380 | `		/* Not under PH7_ENABLE_NET: a script selects over FILES and pipes in a` |
|      - | 3381 | `		 * build with no networking at all. */` |
|      - | 3382 | `		{"stream_select",        PH7_builtin_stream_select },` |
|      - | 3383 | `		{"stream_context_create",PH7_builtin_stream_context_create },` |
|      - | 3384 | `		{"stream_context_get_options",PH7_builtin_stream_context_get_options },` |
|      - | 3385 | `		{"stream_context_set_option", PH7_builtin_stream_context_set_option },` |
|      - | 3386 | `		{"stream_context_set_options",PH7_builtin_stream_context_set_options },` |
|      - | 3387 | `		{"stream_context_get_params", PH7_builtin_stream_context_get_params },` |
|      - | 3388 | `		{"stream_context_set_params", PH7_builtin_stream_context_set_params },` |
|      - | 3389 | `		{"stream_context_get_default",PH7_builtin_stream_context_get_default },` |
|      - | 3390 | `		{"stream_context_set_default",PH7_builtin_stream_context_set_default },` |
|      - | 3391 | `		{"stream_wrapper_register",   PH7_builtin_stream_wrapper_register },` |
|      - | 3392 | `		{"stream_register_wrapper",   PH7_builtin_stream_wrapper_register },` |
|      - | 3393 | `		{"stream_wrapper_unregister", PH7_builtin_stream_wrapper_unregister },` |
|      - | 3394 | `		{"stream_wrapper_restore",    PH7_builtin_stream_wrapper_restore },` |
|      - | 3395 | `		{"stream_filter_append",  PH7_builtin_stream_filter_append },` |
|      - | 3396 | `		{"stream_filter_prepend", PH7_builtin_stream_filter_prepend },` |
|      - | 3397 | `		{"stream_filter_remove",  PH7_builtin_stream_filter_remove },` |
|      - | 3398 | `		{"stream_get_filters",    PH7_builtin_stream_get_filters },` |
|      - | 3399 | `		{"stream_filter_register",PH7_builtin_stream_filter_register },` |
|      - | 3400 | `		{"stream_bucket_make_writeable", PH7_builtin_stream_bucket_make_writeable },` |
|      - | 3401 | `		{"stream_bucket_append",  PH7_builtin_stream_bucket_append },` |
|      - | 3402 | `		{"stream_bucket_prepend", PH7_builtin_stream_bucket_prepend },` |
|      - | 3403 | `		{"stream_bucket_new",     PH7_builtin_stream_bucket_new },` |
|      - | 3404 | `#ifdef PH7_ENABLE_NET` |
|      - | 3405 | `		{"fsockopen",  PH7_builtin_fsockopen },` |
|      - | 3406 | `		{"pfsockopen", PH7_builtin_fsockopen },` |
|      - | 3407 | `		{"stream_socket_client", PH7_builtin_fsockopen },` |
|      - | 3408 | `		{"stream_socket_server", PH7_builtin_stream_socket_server },` |
|      - | 3409 | `		{"stream_socket_accept", PH7_builtin_stream_socket_accept },` |
|      - | 3410 | `		{"stream_socket_get_name", PH7_builtin_stream_socket_get_name },` |
|      - | 3411 | `		{"stream_socket_pair",   PH7_builtin_stream_socket_pair },` |
|      - | 3412 | `		{"stream_socket_shutdown", PH7_builtin_stream_socket_shutdown },` |
|      - | 3413 | `		{"stream_socket_recvfrom", PH7_builtin_stream_socket_recvfrom },` |
|      - | 3414 | `		{"stream_socket_sendto",   PH7_builtin_stream_socket_sendto },` |
|      - | 3415 | `#endif` |
|      - | 3416 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 3417 | `		{"proc_open",      PH7_builtin_proc_open      },` |
|      - | 3418 | `		{"proc_close",     PH7_builtin_proc_close     },` |
|      - | 3419 | `		{"proc_terminate", PH7_builtin_proc_terminate },` |
|      - | 3420 | `		{"proc_get_status",PH7_builtin_proc_get_status},` |
|      - | 3421 | `		{"proc_nice",      PH7_builtin_proc_nice      },` |
|      - | 3422 | `		{"shell_exec", PH7_builtin_shell_exec },` |
|      - | 3423 | `		{"exec",       PH7_builtin_exec     },` |
|      - | 3424 | `		{"system",     PH7_builtin_system   },` |
|      - | 3425 | `		{"passthru",   PH7_builtin_passthru },` |
|      - | 3426 | `		/* The shell-escaping pair lives with the command runners it exists to` |
|      - | 3427 | `		 * feed: a build without process execution has nothing to escape for. */` |
|      - | 3428 | `		{"escapeshellarg", PH7_builtin_escapeshellarg },` |
|      - | 3429 | `		{"escapeshellcmd", PH7_builtin_escapeshellcmd },` |
|      - | 3430 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 3431 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 3432 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 3433 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 3434 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 3435 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 3436 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 3437 | `		/* The hash extension's file readers live with the disk table for the` |
|      - | 3438 | `		 * same reason md5_file does: without disk IO there is nothing to read. */` |
|      - | 3439 | `		{"hash_file",          PH7_builtin_hash_file },` |
|      - | 3440 | `		{"hash_hmac_file",     PH7_builtin_hash_hmac_file },` |
|      - | 3441 | `		{"hash_update_file",   PH7_builtin_hash_update_file },` |
|      - | 3442 | `		{"hash_update_stream", PH7_builtin_hash_update_stream },` |
|      - | 3443 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 3444 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 3445 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 3446 | `	};` |
|   4557 | 3447 | `	const ph7_io_stream *pFileStream = 0;` |
|   4557 | 3448 | `	sxu32 n = 0;` |
|      - | 3449 | `	/* Register disk-related functions */` |
| 250365 | 3450 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 245813 | 3451 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
| 122909 | 3452 | `	}` |
| 459757 | 3453 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 455205 | 3454 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
| 227605 | 3455 | `	}` |
|      - | 3456 | `#else` |
|      - | 3457 | `	SXUNUSED(pVm);` |
|      - | 3458 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 3459 |  |
|      - | 3460 | `	/*` |
|      - | 3461 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 3462 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 3463 | `	 */` |
|      - | 3464 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 3465 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 3466 | `		/* Path processing */` |
|      - | 3467 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 3468 | `		{"basename",    PH7_builtin_basename },` |
|      - | 3469 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 3470 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 3471 | `		{"fnmatch",     PH7_builtin_fnmatch  }` |
|      - | 3472 | `	};` |
|  27317 | 3473 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  22765 | 3474 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|  11385 | 3475 | `	}` |
|      - | 3476 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 3477 |  |
|      - | 3478 | `	/* Install streams if disk I/O is enabled */` |
|      - | 3479 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 3480 | `#ifdef __WINNT__` |
|      5 | 3481 | `	pFileStream = &sWinFileStream;` |
|      - | 3482 | `#elif defined(__UNIXES__)` |
|   4552 | 3483 | `	pFileStream = &sUnixFileStream;` |
|      - | 3484 | `#endif` |
|      - | 3485 | `	/* Install the php:// stream */` |
|   4557 | 3486 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   4557 | 3487 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);` |
|      - | 3488 | `#ifdef PH7_ENABLE_NET` |
|   4557 | 3489 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);` |
|      - | 3490 | `#endif` |
|   4557 | 3491 | `	if( pFileStream ){` |
|      - | 3492 | `		/* Install the file:// stream */` |
|   4557 | 3493 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   2276 | 3494 | `	}` |
|      - | 3495 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 3496 |  |
|   4557 | 3497 | `	return SXRET_OK;` |
|      5 | 3498 | `}` |
|      - | 3499 |  |
