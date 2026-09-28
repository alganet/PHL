# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1401/1801 lines (77.79%)

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
|  17890 |   25 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)` |
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
|  17890 |   37 | `	const char *zRoot = "/";` |
|      - |   38 | `#endif` |
|  17895 |   39 | `	c = d = '/';` |
|      - |   40 | `#ifdef __WINNT__` |
|      5 |   41 | `	d = '\\';` |
|      - |   42 | `#endif` |
|      - |   43 | `#define DIR_IS_SEP(x) ( (int)(x) == c \|\| (int)(x) == d )` |
|  17895 |   44 | `	if( nByte < 1 ){` |
|      - |   45 | `		/* php returns the empty string for the empty path */` |
|      5 |   46 | `		*pLen = 0;` |
|      5 |   47 | `		return "";` |
|      - |   48 | `	}` |
|  17891 |   49 | `	iEnd = nByte;` |
|  26862 |   50 | `	while( iEnd > 0 && DIR_IS_SEP(zPath[iEnd - 1]) ){` |
|     29 |   51 | `		iEnd--;` |
|      1 |   52 | `	}` |
|  17891 |   53 | `	if( iEnd == 0 ){` |
|      - |   54 | `		/* The path is nothing but separators: the root is its own parent */` |
|     17 |   55 | `		*pLen = (int)sizeof(char);` |
|     17 |   56 | `		return zRoot;` |
|      - |   57 | `	}` |
|      - |   58 | `	/* Walk back to the separator that ends the parent directory */` |
|  17875 |   59 | `	i = iEnd;` |
| 485585 |   60 | `	while( i > 0 && !DIR_IS_SEP(zPath[i - 1]) ){` |
| 467715 |   61 | `		i--;` |
|      5 |   62 | `	}` |
|  17875 |   63 | `	if( i == 0 ){` |
|      - |   64 | `		/* No separator at all,return "." as the current directory */` |
|     70 |   65 | `		*pLen = (int)sizeof(char);` |
|     70 |   66 | `		return ".";` |
|      - |   67 | `	}` |
|      - |   68 | `	/* Drop the separator, plus any that repeat before it */` |
|  44500 |   69 | `	while( i > 1 && DIR_IS_SEP(zPath[i - 1]) ){` |
|  17797 |   70 | `		i--;` |
|      5 |   71 | `	}` |
|  17807 |   72 | `	if( i == 1 && DIR_IS_SEP(zPath[0]) ){` |
|     13 |   73 | `		*pLen = (int)sizeof(char);` |
|     13 |   74 | `		return zRoot;` |
|      - |   75 | `	}` |
|  17795 |   76 | `	*pLen = i;` |
|  17795 |   77 | `	return zPath;` |
|      - |   78 | `#undef DIR_IS_SEP` |
|   8950 |   79 | `}` |
|      - |   80 | `/*` |
|      - |   81 | ` * php_basename: drop any trailing separators, then answer what follows the last` |
|      - |   82 | ` * remaining one. Shared by basename() and pathinfo() — they used to hand-roll the` |
|      - |   83 | ` * same walk separately, and pathinfo()'s copy kept the trailing separator run` |
|      - |   84 | `` * (`pathinfo("/var/www/")` answered basename "" where php answers "www").`` |
|      - |   85 | ` */` |
|  17760 |   86 | `PH7_PRIVATE const char * PH7_ExtractBaseName(const char *zPath,int nByte,int *pLen)` |
|      5 |   87 | `{` |
|      - |   88 | `	int c,d,iEnd,i;` |
|  17765 |   89 | `	c = d = '/';` |
|      - |   90 | `#ifdef __WINNT__` |
|      5 |   91 | `	d = '\\';` |
|      - |   92 | `#endif` |
|      - |   93 | `#define DIR_IS_SEP(x) ( (int)(x) == c \|\| (int)(x) == d )` |
|  17765 |   94 | `	iEnd = nByte;` |
|  26670 |   95 | `	while( iEnd > 0 && DIR_IS_SEP(zPath[iEnd - 1]) ){` |
|     27 |   96 | `		iEnd--;` |
|      1 |   97 | `	}` |
|  17765 |   98 | `	if( iEnd < 1 ){` |
|      - |   99 | `		/* Empty, or nothing but separators: php answers the empty string */` |
|     29 |  100 | `		*pLen = 0;` |
|     29 |  101 | `		return "";` |
|      - |  102 | `	}` |
|  17737 |  103 | `	i = iEnd;` |
| 480422 |  104 | `	while( i > 0 && !DIR_IS_SEP(zPath[i - 1]) ){` |
| 462690 |  105 | `		i--;` |
|      5 |  106 | `	}` |
|  17737 |  107 | `	*pLen = iEnd - i;` |
|  17737 |  108 | `	return &zPath[i];` |
|      - |  109 | `#undef DIR_IS_SEP` |
|   8885 |  110 | `}` |
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
|  27652 |  122 | `PH7_PRIVATE const char * VfsStrerror(int iErr)` |
|      5 |  123 | `{` |
|      - |  124 | `#if defined(_MSC_VER)` |
|      - |  125 | `#pragma warning(push)` |
|      - |  126 | `#pragma warning(disable:4996)` |
|      - |  127 | `#endif` |
|  27657 |  128 | `	return strerror(iErr);` |
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
|  27456 |  139 | `static void VfsThrowSysWarning(ph7_context *pCtx,const char *zPath)` |
|      5 |  140 | `{` |
|  41189 |  141 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s",` |
|  27456 |  142 | `		ph7_function_name(pCtx),zPath ? zPath : "",VfsStrerror(errno));` |
|  27461 |  143 | `}` |
|      - |  144 | `/*` |
|      - |  145 | ` * php's "fopen(data://x): Failed to open stream: rfc2397: no comma in URL".` |
|      - |  146 | ` *` |
|      - |  147 | ` * Two things in that sentence were this engine's own. The NAME was whatever` |
|      - |  148 | ` * PH7_VmGetStreamDevice() left after the scheme, so a wrapper's failure blamed` |
|      - |  149 | `` * a path the script never wrote (`fopen(x)`, `fopen(nosuchthing)`); it is the`` |
|      - |  150 | ` * whole URI when the caller is reporting the open that lookup resolved, which` |
|      - |  151 | ` * is what the pointer it handed back identifies. And the REASON was errno --` |
|      - |  152 | ` * which only the plain-file wrapper sets, so every other one reported whatever` |
|      - |  153 | `` * errno happened to be lying around, including `Success` for a failed open.`` |
|      - |  154 | ` * php's reason is the wrapper's: its own sentence when it logged one, and a` |
|      - |  155 | ` * flat "operation failed" when it did not.` |
|      - |  156 | ` */` |
|     94 |  157 | `PH7_PRIVATE void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile)` |
|      4 |  158 | `{` |
|     98 |  159 | `	ph7_vm *pVm = pCtx->pVm;` |
|     98 |  160 | `	const char *zName = zFile ? zFile : "";` |
|     98 |  161 | `	int nName = -1;` |
|     98 |  162 | `	if( zFile != 0 && zFile == pVm->zOpenUriTail && pVm->zOpenUri != 0 ){` |
|     32 |  163 | `		zName = pVm->zOpenUri;` |
|     32 |  164 | `		nName = pVm->nOpenUri;` |
|     15 |  165 | `	}` |
|    177 |  166 | `	PH7_VmThrowWarningFmt(pVm,"%s(%.*s): Failed to open stream: %s",` |
|     79 |  167 | `		ph7_function_name(pCtx),nName < 0 ? (int)SyStrlen(zName) : nName,zName,` |
|     94 |  168 | `		pVm->zOpenErr ? pVm->zOpenErr : VfsStrerror(errno));` |
|     98 |  169 | `}` |
|      - |  170 | `/*` |
|      - |  171 | ` * php's answer when NO wrapper will take a name is a reason of its own, raised` |
|      - |  172 | ` * before the operation's own failure and naming the scheme the script wrote:` |
|      - |  173 | ` *` |
|      - |  174 | ` *   file_get_contents(): Unable to find the wrapper "zzz" - did you forget to` |
|      - |  175 | ` *   enable it when you configured PHP?` |
|      - |  176 | ` *` |
|      - |  177 | ` * PHL raised one PH7-specific sentence -- "No such stream device,PH7 is` |
|      - |  178 | ` * returning FALSE" -- which names neither the function's argument nor what was` |
|      - |  179 | ` * wrong with it, and two more call sites had a third wording of their own.` |
|      - |  180 | ` */` |
|     24 |  181 | `PH7_PRIVATE void VfsThrowNoDeviceWarning(ph7_context *pCtx,const char *zUri,int bDir)` |
|      2 |  182 | `{` |
|     26 |  183 | `	const char *zFunc = ph7_function_name(pCtx);` |
|     26 |  184 | `	const char *zWhat = bDir ? "directory" : "stream";` |
|     26 |  185 | `	int nScheme = 0;` |
|     26 |  186 | `	if( zUri == 0 ){` |
|    ! 0 |  187 | `		zUri = "";` |
|    ! 0 |  188 | `	}` |
|     26 |  189 | `	if( PH7_VmStreamDeviceIsRemoteHost(zUri,-1,&nScheme) ){` |
|      - |  190 | `		/* A wrapper WAS found for the scheme and refused the name, which php` |
|      - |  191 | `		 * words differently from a scheme nothing is registered under. */` |
|     19 |  192 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Remote host file access not supported, %s",` |
|      6 |  193 | `			zFunc,zUri);` |
|     19 |  194 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      6 |  195 | `			"%s(%s): Failed to open %s: no suitable wrapper could be found",zFunc,zUri,zWhat);` |
|     14 |  196 | `		return;` |
|      - |  197 | `	}` |
|     14 |  198 | `	if( nScheme > 0 ){` |
|     17 |  199 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - |  200 | `			"%s(): Unable to find the wrapper \"%.*s\" - did you forget to enable it when you configured PHP?",` |
|      5 |  201 | `			zFunc,nScheme,zUri);` |
|      5 |  202 | `	}` |
|      - |  203 | `	/* A name with no scheme is the plain-files wrapper's, and that is the ONE` |
|      - |  204 | `	 * php reports as switched off rather than missing: its fallback branch runs` |
|      - |  205 | ``	 * after the hash lookup, so an explicit `file://` gets both sentences and a`` |
|      - |  206 | `	 * bare path only the second. Every other unregistered wrapper is` |
|      - |  207 | `	 * indistinguishable from one that never existed, and php words it that way. */` |
|     12 |  208 | `	if( (nScheme == 0` |
|     11 |  209 | `	  \|\| (nScheme == (int)sizeof("file")-1 && SyStrnicmp(zUri,"file",sizeof("file")-1) == 0))` |
|      5 |  210 | `	 && PH7_VmStreamSchemeDisabled(pCtx->pVm,"file",(int)sizeof("file")-1) ){` |
|      4 |  211 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      1 |  212 | `			"%s(): file:// wrapper is disabled in the server configuration",zFunc);` |
|      4 |  213 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      1 |  214 | `			"%s(%s): Failed to open %s: no suitable wrapper could be found",zFunc,zUri,zWhat);` |
|      3 |  215 | `		return;` |
|      - |  216 | `	}` |
|     17 |  217 | `	PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      5 |  218 | `		"%s(%s): Failed to open %s: No such file or directory",zFunc,zUri,zWhat);` |
|     14 |  219 | `}` |
|      - |  220 | `/*` |
|      - |  221 | `` * php's stat-failure warning: `filemtime(): stat failed for /nope`, and`` |
|      - |  222 | `` * `filetype(): Lstat failed for /nope` for the two members that LSTAT. php raises`` |
|      - |  223 | ` * it from php_stat() for the whole family and answers FALSE; PHL answered the` |
|      - |  224 | ` * VFS's raw -1 (or the string "unknown") for most of them, in silence -- and -1 is` |
|      - |  225 | `` * TRUTHY, so `if (filemtime($f))` took the found branch for a file that is not`` |
|      - |  226 | `` * there and `filemtime($a) > filemtime($b)` compared a real time against it.`` |
|      - |  227 | ` */` |
|     20 |  228 | `static void VfsThrowStatWarning(ph7_context *pCtx,const char *zPath,int bLstat)` |
|      1 |  229 | `{` |
|     31 |  230 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s failed for %s",` |
|     10 |  231 | `		ph7_function_name(pCtx),bLstat ? "Lstat" : "stat",zPath ? zPath : "");` |
|     21 |  232 | `}` |
|      - |  233 | `/*` |
|      - |  234 | ` * php's stat() answer is TWENTY-SIX entries, not thirteen: the same thirteen` |
|      - |  235 | ` * fields once at numeric indices 0..12 and once under their names, in this` |
|      - |  236 | ` * order. The numeric half is what php's own documentation indexes by ($s[7] is` |
|      - |  237 | `` * the size) and it is what a `list()`/destructuring reader takes, so a script`` |
|      - |  238 | `` * written against php read `Undefined array key 7` here and answered NULL.`` |
|      - |  239 | ` *` |
|      - |  240 | ` * The VFS fills the NAMED half (both the unix and Windows implementations use` |
|      - |  241 | ` * exactly these keys), so the doubling is done once, here, rather than in every` |
|      - |  242 | ` * xStat: pOut gets the numeric run first and then the names, which is php's own` |
|      - |  243 | ` * insertion order — visible through foreach, print_r, var_dump and json_encode.` |
|      - |  244 | ` */` |
|     30 |  245 | `PH7_PRIVATE int PH7_VfsStatDoubleUp(ph7_value *pIn,ph7_value *pOut)` |
|      1 |  246 | `{` |
|      - |  247 | `	static const char * const azField[] = {` |
|      - |  248 | `		"dev","ino","mode","nlink","uid","gid","rdev","size",` |
|      - |  249 | `		"atime","mtime","ctime","blksize","blocks"` |
|      - |  250 | `	};` |
|      - |  251 | `	sxu32 i;` |
|     31 |  252 | `	if( pIn == 0 \|\| pOut == 0 ){` |
|    ! 0 |  253 | `		return -1;` |
|      - |  254 | `	}` |
|    369 |  255 | `	for( i = 0 ; i < SX_ARRAYSIZE(azField) ; ++i ){` |
|    343 |  256 | `		ph7_value *pField = ph7_array_fetch(pIn,azField[i],-1);` |
|    343 |  257 | `		if( pField == 0 ){` |
|      - |  258 | `			/* A VFS that does not report this field: php always has all thirteen,` |
|      - |  259 | `			 * so the doubling would silently shift every later index. Hand the` |
|      - |  260 | `			 * caller the named-only array it already had instead. */` |
|      5 |  261 | `			return -1;` |
|      - |  262 | `		}` |
|    339 |  263 | `		ph7_array_add_elem(pOut,0,pField);` |
|    170 |  264 | `	}` |
|    365 |  265 | `	for( i = 0 ; i < SX_ARRAYSIZE(azField) ; ++i ){` |
|    339 |  266 | `		ph7_value *pField = ph7_array_fetch(pIn,azField[i],-1);` |
|    339 |  267 | `		ph7_array_add_strkey_elem(pOut,azField[i],pField);` |
|    170 |  268 | `	}` |
|     27 |  269 | `	return PH7_OK;` |
|     16 |  270 | `}` |
|      - |  271 | `/*` |
|      - |  272 | ` * Can this path be stat'ed at all? The three TIME readers report a failure as -1,` |
|      - |  273 | ` * which is also a legitimate timestamp (a file stamped in the last second before` |
|      - |  274 | ` * the epoch), so the failure verdict is asked of the VFS separately rather than` |
|      - |  275 | ` * read off the value -- one extra call, and only on the negative branch.` |
|      - |  276 | ` */` |
|    106 |  277 | `static int VfsPathStatable(ph7_vfs *pVfs,const char *zPath)` |
|      2 |  278 | `{` |
|    108 |  279 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|    ! 0 |  280 | `		return 0;` |
|      - |  281 | `	}` |
|    108 |  282 | `	return pVfs->xFileExists(zPath) == PH7_OK;` |
|     73 |  283 | `}` |
|      - |  284 | `/*` |
|      - |  285 | ` * bool chdir(string $directory)` |
|      - |  286 | ` *  Change the current directory.` |
|      - |  287 | ` * Parameters` |
|      - |  288 | ` *  $directory` |
|      - |  289 | ` *   The new current directory` |
|      - |  290 | ` * Return` |
|      - |  291 | ` *  TRUE on success or FALSE on failure.` |
|      - |  292 | ` */` |
|  15962 |  293 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  294 | `{` |
|      - |  295 | `	const char *zPath;` |
|      - |  296 | `	ph7_vfs *pVfs;` |
|      - |  297 | `	int rc;` |
|      - |  298 | `	/* Only the ARITY is checked here: php coerces a scalar $directory to string,` |
|      - |  299 | `	 * so chdir(123) attempts "123" and warns that it does not exist. Requiring a` |
|      - |  300 | `	 * string outright made that call return FALSE silently, with no diagnostic. */` |
|  15967 |  301 | `	if( nArg < 1 ){` |
|      - |  302 | `		/* Missing argument,return FALSE */` |
|    ! 0 |  303 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  304 | `		return PH7_OK;` |
|      - |  305 | `	}` |
|      - |  306 | `	/* Point to the underlying vfs */` |
|  15967 |  307 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  15967 |  308 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |  309 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  310 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  311 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  312 | `			ph7_function_name(pCtx)` |
|      - |  313 | `			);` |
|    ! 0 |  314 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  315 | `		return PH7_OK;` |
|      - |  316 | `	}` |
|      - |  317 | `	/* Point to the desired directory */` |
|  15967 |  318 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  319 | `	/* Perform the requested operation */` |
|  15967 |  320 | `	errno = 0;` |
|  15967 |  321 | `	rc = pVfs->xChdir(zPath);` |
|  15967 |  322 | `	if( rc != PH7_OK ){` |
|      - |  323 | `		/* chdir has its own php shape: no path, and the errno spelled out. */` |
|      8 |  324 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|      4 |  325 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|      2 |  326 | `	}` |
|      - |  327 | `	/* IO return value */` |
|  15967 |  328 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  15967 |  329 | `	return PH7_OK;` |
|   7986 |  330 | `}` |
|      - |  331 | `/*` |
|      - |  332 | ` * bool chroot(string $directory)` |
|      - |  333 | ` *  Change the root directory.` |
|      - |  334 | ` * Parameters` |
|      - |  335 | ` *  $directory` |
|      - |  336 | ` *   The path to change the root directory to` |
|      - |  337 | ` * Return` |
|      - |  338 | ` *  TRUE on success or FALSE on failure.` |
|      - |  339 | ` *` |
|      - |  340 | ` * POSIX only, like php's: the registration below is guarded the same way, and an` |
|      - |  341 | ` * unreferenced static is an error under the Windows build's /W4 /WX.` |
|      - |  342 | ` */` |
|      - |  343 | `#ifndef __WINNT__` |
|    ! 0 |  344 | `static int PH7_vfs_chroot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - |  345 | `{` |
|      - |  346 | `	const char *zPath;` |
|      - |  347 | `	ph7_vfs *pVfs;` |
|      - |  348 | `	int rc;` |
|    ! 0 |  349 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  350 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  351 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  352 | `		return PH7_OK;` |
|      - |  353 | `	}` |
|      - |  354 | `	/* Point to the underlying vfs */` |
|    ! 0 |  355 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    ! 0 |  356 | `	if( pVfs == 0 \|\| pVfs->xChroot == 0 ){` |
|      - |  357 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  358 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  359 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  360 | `			ph7_function_name(pCtx)` |
|      - |  361 | `			);` |
|    ! 0 |  362 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  363 | `		return PH7_OK;` |
|      - |  364 | `	}` |
|      - |  365 | `	/* Point to the desired directory */` |
|    ! 0 |  366 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  367 | `	/* Perform the requested operation */` |
|    ! 0 |  368 | `	errno = 0;` |
|    ! 0 |  369 | `	rc = pVfs->xChroot(zPath);` |
|    ! 0 |  370 | `	if( rc != PH7_OK ){` |
|      - |  371 | `		/* php's own wording, and the failure a script actually meets: chroot(2)` |
|      - |  372 | `		 * needs privilege, so an ordinary process gets EPERM. PHL answered the` |
|      - |  373 | `		 * bare false in SILENCE — a refused chroot() and a chroot() that did` |
|      - |  374 | `		 * nothing looked the same to the caller. */` |
|    ! 0 |  375 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|    ! 0 |  376 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|    ! 0 |  377 | `	}` |
|      - |  378 | `	/* IO return value */` |
|    ! 0 |  379 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    ! 0 |  380 | `	return PH7_OK;` |
|    ! 0 |  381 | `}` |
|      - |  382 | `#endif /* __WINNT__ */` |
|      - |  383 | `/*` |
|      - |  384 | ` * string getcwd(void)` |
|      - |  385 | ` *  Gets the current working directory.` |
|      - |  386 | ` * Parameters` |
|      - |  387 | ` *  None` |
|      - |  388 | ` * Return` |
|      - |  389 | ` *  Returns the current working directory on success, or FALSE on failure.` |
|      - |  390 | ` */` |
|     20 |  391 | `static int PH7_vfs_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  392 | `{` |
|      - |  393 | `	ph7_vfs *pVfs;` |
|      - |  394 | `	int rc;` |
|      - |  395 | `	/* Point to the underlying vfs */` |
|     25 |  396 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     25 |  397 | `	if( pVfs == 0 \|\| pVfs->xGetcwd == 0 ){` |
|    ! 0 |  398 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 |  399 | `		SXUNUSED(apArg);` |
|      - |  400 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  401 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  402 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  403 | `			ph7_function_name(pCtx)` |
|      - |  404 | `			);` |
|    ! 0 |  405 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  406 | `		return PH7_OK;` |
|      - |  407 | `	}` |
|     25 |  408 | `	ph7_result_string(pCtx,"",0);` |
|      - |  409 | `	/* Perform the requested operation */` |
|     25 |  410 | `	rc = pVfs->xGetcwd(pCtx);` |
|     25 |  411 | `	if( rc != PH7_OK ){` |
|      - |  412 | `		/* Error,return FALSE */` |
|    ! 0 |  413 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  414 | `	}` |
|     25 |  415 | `	return PH7_OK;` |
|     15 |  416 | `}` |
|      - |  417 | `/*` |
|      - |  418 | ` * bool rmdir(string $directory)` |
|      - |  419 | ` *  Removes directory.` |
|      - |  420 | ` * Parameters` |
|      - |  421 | ` *  $directory` |
|      - |  422 | ` *   The path to the directory` |
|      - |  423 | ` * Return` |
|      - |  424 | ` *  TRUE on success or FALSE on failure.` |
|      - |  425 | ` */` |
|    204 |  426 | `static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  427 | `{` |
|      - |  428 | `	const char *zPath;` |
|      - |  429 | `	ph7_vfs *pVfs;` |
|    209 |  430 | `	int rc,bThrew = 0;` |
|    209 |  431 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  432 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  433 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  434 | `		return PH7_OK;` |
|      - |  435 | `	}` |
|      - |  436 | ``	/* php's `?resource $context`, refused when it is a resource of another kind.`` |
|      - |  437 | `	 * Nothing here CONSUMES it — this operation never opens a stream, and the` |
|      - |  438 | `	 * userland wrapper's unlink/rename/mkdir/rmdir methods are not dispatched` |
|      - |  439 | `	 * (§7.4 slice-2 (e)) — but the refusal is the argument's contract, and it` |
|      - |  440 | `	 * was accepted in silence. */` |
|    209 |  441 | `	PH7_StreamCtxFromArg(pCtx,nArg,apArg,1,"$context",0,&bThrew);` |
|    209 |  442 | `	if( bThrew ){` |
|      3 |  443 | `		return PH7_OK;` |
|      - |  444 | `	}` |
|      - |  445 | `	/* Point to the underlying vfs */` |
|    207 |  446 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    207 |  447 | `	if( pVfs == 0 \|\| pVfs->xRmdir == 0 ){` |
|      - |  448 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  449 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  450 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  451 | `			ph7_function_name(pCtx)` |
|      - |  452 | `			);` |
|    ! 0 |  453 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  454 | `		return PH7_OK;` |
|      - |  455 | `	}` |
|      - |  456 | `	/* Point to the desired directory */` |
|    207 |  457 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  458 | `	/* Perform the requested operation */` |
|    207 |  459 | `	errno = 0;` |
|    207 |  460 | `	rc = pVfs->xRmdir(zPath);` |
|    207 |  461 | `	if( rc != PH7_OK ){` |
|     29 |  462 | `		VfsThrowSysWarning(pCtx,zPath);` |
|     12 |  463 | `	}` |
|      - |  464 | `	/* IO return value */` |
|    207 |  465 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    207 |  466 | `	return PH7_OK;` |
|    107 |  467 | `}` |
|      - |  468 | `/*` |
|      - |  469 | ` * bool is_dir(string $filename)` |
|      - |  470 | ` *  Tells whether the given filename is a directory.` |
|      - |  471 | ` * Parameters` |
|      - |  472 | ` *  $filename` |
|      - |  473 | ` *   Path to the file.` |
|      - |  474 | ` * Return` |
|      - |  475 | ` *  TRUE on success or FALSE on failure.` |
|      - |  476 | ` */` |
|  12016 |  477 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  478 | `{` |
|      - |  479 | `	const char *zPath;` |
|      - |  480 | `	ph7_vfs *pVfs;` |
|      - |  481 | `	int rc;` |
|  12021 |  482 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  483 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  484 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  485 | `		return PH7_OK;` |
|      - |  486 | `	}` |
|      - |  487 | `	/* Point to the underlying vfs */` |
|  12021 |  488 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  12021 |  489 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - |  490 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  491 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  492 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  493 | `			ph7_function_name(pCtx)` |
|      - |  494 | `			);` |
|    ! 0 |  495 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  496 | `		return PH7_OK;` |
|      - |  497 | `	}` |
|      - |  498 | `	/* Point to the desired directory */` |
|  12021 |  499 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  500 | `	/* Perform the requested operation */` |
|  12021 |  501 | `	rc = pVfs->xIsdir(zPath);` |
|      - |  502 | `	/* IO return value */` |
|  12021 |  503 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  12021 |  504 | `	return PH7_OK;` |
|   6013 |  505 | `}` |
|      - |  506 | `/*` |
|      - |  507 | ` * bool mkdir(string $pathname[,int $mode = 0777 [,bool $recursive = false])` |
|      - |  508 | ` *  Make a directory.` |
|      - |  509 | ` * Parameters` |
|      - |  510 | ` *  $pathname` |
|      - |  511 | ` *   The directory path.` |
|      - |  512 | ` * $mode` |
|      - |  513 | ` *  The mode is 0777 by default, which means the widest possible access.` |
|      - |  514 | ` *  Note:` |
|      - |  515 | ` *   mode is ignored on Windows.` |
|      - |  516 | ` *   Note that you probably want to specify the mode as an octal number, which means` |
|      - |  517 | ` *   it should have a leading zero. The mode is also modified by the current umask` |
|      - |  518 | ` *   which you can change using umask().` |
|      - |  519 | ` * $recursive` |
|      - |  520 | ` *  Allows the creation of nested directories specified in the pathname.` |
|      - |  521 | ` *  Defaults to FALSE. (Not used)` |
|      - |  522 | ` * Return` |
|      - |  523 | ` *  TRUE on success or FALSE on failure.` |
|      - |  524 | ` */` |
|      - |  525 | `/*` |
|      - |  526 | ` * A prefix the recursive mkdir must not try to CREATE: it names a volume rather` |
|      - |  527 | ` * than a directory. POSIX has none of these (the leading "/" is never a prefix` |
|      - |  528 | ` * here, since the walk starts one byte in).` |
|      - |  529 | ` */` |
|    100 |  530 | `static int VfsMkdirVolumePrefix(const char *z,int n)` |
|      1 |  531 | `{` |
|      - |  532 | `#ifdef __WINNT__` |
|      1 |  533 | `	int i,nSep = 0;` |
|      1 |  534 | `	if( n < 1 ){` |
|    ! 0 |  535 | `		return 1;` |
|      - |  536 | `	}` |
|      1 |  537 | `	if( n == 2 && z[1] == ':' ){` |
|      1 |  538 | `		return 1; /* a bare drive, "C:" */` |
|      - |  539 | `	}` |
|      1 |  540 | `	if( n > 1 && (z[0] == '/' \|\| z[0] == '\\') && (z[1] == '/' \|\| z[1] == '\\') ){` |
|    ! 0 |  541 | `		for( i = 2 ; i < n ; i++ ){` |
|    ! 0 |  542 | `			if( z[i] == '/' \|\| z[i] == '\\' ){` |
|    ! 0 |  543 | `				nSep++;` |
|      - |  544 | `			}` |
|    ! 0 |  545 | `		}` |
|    ! 0 |  546 | `		return nSep < 2; /* still inside \\server\share */` |
|      - |  547 | `	}` |
|      1 |  548 | `	return 0;` |
|      - |  549 | `#else` |
|     68 |  550 | `	SXUNUSED(z);` |
|    100 |  551 | `	return n < 1;` |
|      - |  552 | `#endif` |
|      1 |  553 | `}` |
|      - |  554 | `#ifdef __WINNT__` |
|      - |  555 | `#define VFS_MKDIR_SLASH(c) ((c) == '/' \|\| (c) == '\\')` |
|      - |  556 | `#else` |
|      - |  557 | `#define VFS_MKDIR_SLASH(c) ((c) == '/')` |
|      - |  558 | `#endif` |
|      - |  559 | `/*` |
|      - |  560 | ` * php's $recursive: create every missing ancestor, then the directory itself.` |
|      - |  561 | `` * The flag reached the VFS and both back ends dropped it (`SXUNUSED(recursive)`),`` |
|      - |  562 | `` * so `mkdir("$d/a/b", 0777, true)` -- the everyday way a script prepares an`` |
|      - |  563 | ` * output tree -- warned "No such file or directory" and answered false whenever` |
|      - |  564 | ` * more than one level was missing.` |
|      - |  565 | ` *` |
|      - |  566 | ` * php does the walk in the WRAPPER too, not in the syscall, and the rules the` |
|      - |  567 | ` * oracle shows are: the mode is applied to every level it creates; an ancestor` |
|      - |  568 | ` * that already exists is skipped in silence; the LEAF is always attempted, so an` |
|      - |  569 | ` * existing one is "File exists" exactly as without the flag; a trailing` |
|      - |  570 | ` * separator names the same directory; and the empty path is refused up front` |
|      - |  571 | ` * with a message of its own.` |
|      - |  572 | ` */` |
|     24 |  573 | `static int VfsMkdirRecursive(ph7_context *pCtx,ph7_vfs *pVfs,const char *zPath,int iMode)` |
|      1 |  574 | `{` |
|      - |  575 | `	SyBlob sWorker;` |
|      - |  576 | `	const char *zLocal;` |
|     25 |  577 | `	int i,nPath,nScheme,rc = PH7_OK;` |
|      - |  578 | `	/* The strip first: the component walk must not cut a "file://" scheme up. */` |
|     25 |  579 | `	zLocal = PH7_VmFileUrlLocalPath(zPath);` |
|     25 |  580 | `	nScheme = PH7_VmUrlSchemeLen(zPath,-1);` |
|     24 |  581 | `	if( zLocal == zPath && nScheme == (int)sizeof("file")-1` |
|     13 |  582 | `	 && SyStrnicmp(zPath,"file",sizeof("file")-1) == 0 ){` |
|      - |  583 | `		/* A file:// AUTHORITY this build will not reach: the strip handed the` |
|      - |  584 | `		 * URL straight back. php answers false and creates NOTHING, where the` |
|      - |  585 | `		 * walk below would cut the URL into components and make a directory` |
|      - |  586 | `		 * literally called "file:". (A name under any OTHER scheme really does` |
|      - |  587 | `` 		 * become a directory of that name on php too -- measured: `zzz://a/b` `` |
|      - |  588 | ``		 * leaves a `zzz:` behind there as well -- so only this one is refused.) */`` |
|      3 |  589 | `		return -1;` |
|      - |  590 | `	}` |
|     23 |  591 | `	zPath = zLocal;` |
|     23 |  592 | `	nPath = (int)SyStrlen(zPath);` |
|      - |  593 | `	/* A trailing separator names the same directory; php's own expand_filepath` |
|      - |  594 | `	 * drops it before it starts. */` |
|     25 |  595 | `	while( nPath > 1 && VFS_MKDIR_SLASH(zPath[nPath-1]) ){` |
|      3 |  596 | `		nPath--;` |
|      1 |  597 | `	}` |
|     23 |  598 | `	if( nPath < 1 ){` |
|      - |  599 | `		/* php: expand_filepath() refuses it, with this wording and no path. */` |
|      3 |  600 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Invalid path",ph7_function_name(pCtx));` |
|      3 |  601 | `		return -1;` |
|      - |  602 | `	}` |
|     21 |  603 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|    858 |  604 | `	for( i = 1 ; i <= nPath ; i++ ){` |
|    844 |  605 | `		int bLeaf = (i == nPath);` |
|    844 |  606 | `		if( !bLeaf ){` |
|      - |  607 | `			/* Only at a separator that ENDS a component: a run of them names` |
|      - |  608 | `			 * the same ancestor once. */` |
|    824 |  609 | `			if( !VFS_MKDIR_SLASH(zPath[i]) \|\| VFS_MKDIR_SLASH(zPath[i-1]) ){` |
|    724 |  610 | `				continue;` |
|      - |  611 | `			}` |
|    101 |  612 | `			if( VfsMkdirVolumePrefix(zPath,i) ){` |
|      1 |  613 | `				continue;` |
|      - |  614 | `			}` |
|     68 |  615 | `		}` |
|    121 |  616 | `		SyBlobReset(&sWorker);` |
|    120 |  617 | `		if( SyBlobAppend(&sWorker,zPath,(sxu32)i) != SXRET_OK` |
|    121 |  618 | `		 \|\| SyBlobNullAppend(&sWorker) != SXRET_OK ){` |
|    ! 0 |  619 | `			rc = -1;` |
|    ! 0 |  620 | `			break;` |
|      - |  621 | `		}` |
|    121 |  622 | `		if( !bLeaf && VfsPathStatable(pVfs,(const char *)SyBlobData(&sWorker)) ){` |
|     85 |  623 | `			continue; /* an ancestor that is already there */` |
|      - |  624 | `		}` |
|     37 |  625 | `		errno = 0;` |
|     37 |  626 | `		rc = pVfs->xMkdir((const char *)SyBlobData(&sWorker),iMode,0);` |
|     37 |  627 | `		if( rc != PH7_OK ){` |
|     10 |  628 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      6 |  629 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      7 |  630 | `			break;` |
|      - |  631 | `		}` |
|     16 |  632 | `	}` |
|     21 |  633 | `	SyBlobRelease(&sWorker);` |
|     21 |  634 | `	return rc;` |
|     13 |  635 | `}` |
|    204 |  636 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  637 | `{` |
|    209 |  638 | `	int iRecursive = 0;` |
|      - |  639 | `	const char *zPath;` |
|      - |  640 | `	ph7_vfs *pVfs;` |
|    209 |  641 | `	int iMode,rc,bThrew = 0;` |
|    209 |  642 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  643 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  644 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  645 | `		return PH7_OK;` |
|      - |  646 | `	}` |
|      - |  647 | ``	/* php's `?resource $context`, refused when it is a resource of another kind.`` |
|      - |  648 | `	 * Nothing here CONSUMES it — this operation never opens a stream, and the` |
|      - |  649 | `	 * userland wrapper's unlink/rename/mkdir/rmdir methods are not dispatched` |
|      - |  650 | `	 * (§7.4 slice-2 (e)) — but the refusal is the argument's contract, and it` |
|      - |  651 | `	 * was accepted in silence. */` |
|    209 |  652 | `	PH7_StreamCtxFromArg(pCtx,nArg,apArg,3,"$context",0,&bThrew);` |
|    209 |  653 | `	if( bThrew ){` |
|      6 |  654 | `		return PH7_OK;` |
|      - |  655 | `	}` |
|      - |  656 | `	/* Point to the underlying vfs */` |
|    205 |  657 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    205 |  658 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - |  659 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  660 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  661 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  662 | `			ph7_function_name(pCtx)` |
|      - |  663 | `			);` |
|    ! 0 |  664 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  665 | `		return PH7_OK;` |
|      - |  666 | `	}` |
|      - |  667 | `	/* Point to the desired directory */` |
|    205 |  668 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  669 | `#ifdef __WINNT__` |
|      5 |  670 | `	iMode = 0;` |
|      - |  671 | `#else` |
|      - |  672 | `	/* Assume UNIX */` |
|    200 |  673 | `	iMode = 0777;` |
|      - |  674 | `#endif` |
|    205 |  675 | `	if( nArg > 1 ){` |
|     25 |  676 | `		iMode = ph7_value_to_int(apArg[1]);` |
|     25 |  677 | `		if( nArg > 2 ){` |
|     25 |  678 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|     12 |  679 | `		}` |
|     12 |  680 | `	}` |
|      - |  681 | `	/* Perform the requested operation */` |
|    205 |  682 | `	if( iRecursive ){` |
|     25 |  683 | `		rc = VfsMkdirRecursive(pCtx,pVfs,zPath,iMode);` |
|     13 |  684 | `	}else{` |
|    181 |  685 | `		errno = 0;` |
|    181 |  686 | `		rc = pVfs->xMkdir(zPath,iMode,0);` |
|    181 |  687 | `		if( rc != PH7_OK ){` |
|      - |  688 | `			/* php does NOT name the path for mkdir: "mkdir(): File exists" */` |
|      7 |  689 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 |  690 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      2 |  691 | `		}` |
|      - |  692 | `	}` |
|      - |  693 | `	/* IO return value */` |
|    205 |  694 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    205 |  695 | `	return PH7_OK;` |
|    107 |  696 | `}` |
|      - |  697 | `/*` |
|      - |  698 | ` * bool rename(string $oldname,string $newname)` |
|      - |  699 | ` *  Attempts to rename oldname to newname.` |
|      - |  700 | ` * Parameters` |
|      - |  701 | ` *  $oldname` |
|      - |  702 | ` *   Old name.` |
|      - |  703 | ` *  $newname` |
|      - |  704 | ` *   New name.` |
|      - |  705 | ` * Return` |
|      - |  706 | ` *  TRUE on success or FALSE on failure.` |
|      - |  707 | ` */` |
|      4 |  708 | `static int PH7_vfs_rename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  709 | `{` |
|      - |  710 | `	const char *zOld,*zNew;` |
|      - |  711 | `	ph7_vfs *pVfs;` |
|      5 |  712 | `	int rc,bThrew = 0;` |
|      5 |  713 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - |  714 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  715 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  716 | `		return PH7_OK;` |
|      - |  717 | `	}` |
|      - |  718 | ``	/* php's `?resource $context`, refused when it is a resource of another kind.`` |
|      - |  719 | `	 * Nothing here CONSUMES it — this operation never opens a stream, and the` |
|      - |  720 | `	 * userland wrapper's unlink/rename/mkdir/rmdir methods are not dispatched` |
|      - |  721 | `	 * (§7.4 slice-2 (e)) — but the refusal is the argument's contract, and it` |
|      - |  722 | `	 * was accepted in silence. */` |
|      5 |  723 | `	PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|      5 |  724 | `	if( bThrew ){` |
|      3 |  725 | `		return PH7_OK;` |
|      - |  726 | `	}` |
|      - |  727 | `	/* Point to the underlying vfs */` |
|      3 |  728 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 |  729 | `	if( pVfs == 0 \|\| pVfs->xRename == 0 ){` |
|      - |  730 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  731 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  732 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  733 | `			ph7_function_name(pCtx)` |
|      - |  734 | `			);` |
|    ! 0 |  735 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  736 | `		return PH7_OK;` |
|      - |  737 | `	}` |
|      - |  738 | `	/* Perform the requested operation */` |
|      3 |  739 | `	zOld = ph7_value_to_string(apArg[0],0);` |
|      3 |  740 | `	zNew = ph7_value_to_string(apArg[1],0);` |
|      3 |  741 | `	errno = 0;` |
|      3 |  742 | `	rc = pVfs->xRename(zOld,zNew);` |
|      3 |  743 | `	if( rc != PH7_OK ){` |
|      - |  744 | `		/* php names BOTH paths here */` |
|    ! 0 |  745 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s,%s): %s",` |
|    ! 0 |  746 | `			ph7_function_name(pCtx),zOld,zNew,VfsStrerror(errno));` |
|    ! 0 |  747 | `	}` |
|      - |  748 | `	/* IO result */` |
|      3 |  749 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 |  750 | `	return PH7_OK;` |
|      3 |  751 | `}` |
|      - |  752 | `/*` |
|      - |  753 | ` * string realpath(string $path)` |
|      - |  754 | ` *  Returns canonicalized absolute pathname.` |
|      - |  755 | ` * Parameters` |
|      - |  756 | ` *  $path` |
|      - |  757 | ` *   Target path.` |
|      - |  758 | ` * Return` |
|      - |  759 | ` *  Canonicalized absolute pathname on success. or FALSE on failure.` |
|      - |  760 | ` */` |
|    330 |  761 | `static int PH7_vfs_realpath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  762 | `{` |
|      - |  763 | `	const char *zPath;` |
|      - |  764 | `	ph7_vfs *pVfs;` |
|      - |  765 | `        int rc;` |
|    333 |  766 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  767 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  768 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  769 | `		return PH7_OK;` |
|      - |  770 | `	}` |
|      - |  771 | `	/* Point to the underlying vfs */` |
|    333 |  772 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    333 |  773 | `	if( pVfs == 0 \|\| pVfs->xRealpath == 0 ){` |
|      - |  774 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  775 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  776 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  777 | `			ph7_function_name(pCtx)` |
|      - |  778 | `			);` |
|    ! 0 |  779 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  780 | `		return PH7_OK;` |
|      - |  781 | `	}` |
|      - |  782 | `	/* Set an empty string untnil the underlying OS interface change that */` |
|    333 |  783 | `	ph7_result_string(pCtx,"",0);` |
|      - |  784 | `	/* Perform the requested operation */` |
|    333 |  785 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|    333 |  786 | `	rc = pVfs->xRealpath(zPath,pCtx);` |
|    333 |  787 | `	if( rc != PH7_OK ){` |
|      2 |  788 | `	 ph7_result_bool(pCtx,0);` |
|      1 |  789 | `	}` |
|    333 |  790 | `	return PH7_OK;` |
|    168 |  791 | `}` |
|      - |  792 | `/*` |
|      - |  793 | ` * Does this candidate name something, and if so what is its canonical path?` |
|      - |  794 | ` * The existence question is asked separately because xRealpath() writes STRAIGHT` |
|      - |  795 | ` * into the call's result, so it may only be run on the winner.` |
|      - |  796 | ` */` |
|     32 |  797 | `static int VfsResolveTry(ph7_vfs *pVfs,ph7_context *pCtx,const char *zCand)` |
|      1 |  798 | `{` |
|     33 |  799 | `	if( pVfs->xFileExists(zCand) != PH7_OK ){` |
|     19 |  800 | `		return 0;` |
|      - |  801 | `	}` |
|      - |  802 | `	/* The VFS APPENDS into the call's result, so make it an empty string first` |
|      - |  803 | `	 * -- the realpath() builtin beside this one seeds the same way. */` |
|     15 |  804 | `	ph7_result_string(pCtx,"",0);` |
|     15 |  805 | `	if( pVfs->xRealpath(zCand,pCtx) == PH7_OK ){` |
|     15 |  806 | `		return 1;` |
|      - |  807 | `	}` |
|    ! 0 |  808 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 |  809 | `	return 0;` |
|     17 |  810 | `}` |
|      - |  811 | `/*` |
|      - |  812 | ` * Is this an ABSOLUTE path, by php's rule for this platform? A lone leading` |
|      - |  813 | ` * slash is NOT absolute on Windows -- php walks the include_path for it.` |
|      - |  814 | ` */` |
|     16 |  815 | `static int VfsPathIsAbsolute(const char *z,int n)` |
|      1 |  816 | `{` |
|      - |  817 | `#ifdef __WINNT__` |
|      1 |  818 | `	if( n >= 2 && ((z[0] >= 'A' && z[0] <= 'Z') \|\| (z[0] >= 'a' && z[0] <= 'z')) && z[1] == ':' ){` |
|      1 |  819 | `		return 1;` |
|      - |  820 | `	}` |
|      1 |  821 | `	return n >= 2 && (z[0] == '/' \|\| z[0] == '\\') && (z[1] == '/' \|\| z[1] == '\\');` |
|      - |  822 | `#else` |
|     16 |  823 | `	return n >= 1 && z[0] == '/';` |
|      - |  824 | `#endif` |
|      1 |  825 | `}` |
|      - |  826 | `/* "./x" and "../x": php reads these against the CWD and never walks the path. */` |
|     20 |  827 | `static int VfsPathIsDotRelative(const char *z,int n)` |
|      1 |  828 | `{` |
|      - |  829 | `#ifdef __WINNT__` |
|      - |  830 | `#define VFS_RESOLVE_SLASH(c) ((c) == '/' \|\| (c) == '\\')` |
|      - |  831 | `#else` |
|      - |  832 | `#define VFS_RESOLVE_SLASH(c) ((c) == '/')` |
|      - |  833 | `#endif` |
|     21 |  834 | `	if( n < 2 \|\| z[0] != '.' ){` |
|     17 |  835 | `		return 0;` |
|      - |  836 | `	}` |
|      5 |  837 | `	if( VFS_RESOLVE_SLASH(z[1]) ){` |
|      3 |  838 | `		return 1;` |
|      - |  839 | `	}` |
|      3 |  840 | `	return n > 2 && z[1] == '.' && VFS_RESOLVE_SLASH(z[2]);` |
|     11 |  841 | `}` |
|      - |  842 | `/*` |
|      - |  843 | ` * string\|false stream_resolve_include_path(string $filename)` |
|      - |  844 | ` *  Where would include/require find this name? php's own php_resolve_path,` |
|      - |  845 | ` *  which is the ONLY way a script can ask that question without opening` |
|      - |  846 | ` *  anything -- and the way an autoloader decides whether a class file exists` |
|      - |  847 | ` *  before requiring it.` |
|      - |  848 | ` * Return` |
|      - |  849 | ` *  The canonical path on success, FALSE when nothing answers.` |
|      - |  850 | ` */` |
|     28 |  851 | `static int PH7_vfs_stream_resolve_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  852 | `{` |
|     29 |  853 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  854 | `	ph7_vfs *pVfs;` |
|      - |  855 | `	const char *zPath;` |
|      - |  856 | `	SyString *aEntry;` |
|      - |  857 | `	SyString sDir;` |
|      - |  858 | `	SyBlob sWorker;` |
|     29 |  859 | `	int nPath = 0, nScheme, c;` |
|      - |  860 | `	sxu32 n;` |
|      - |  861 | `	/* FALSE until something resolves: xRealpath() overwrites it on the winner. */` |
|     29 |  862 | `	ph7_result_bool(pCtx,0);` |
|     29 |  863 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     29 |  864 | `	if( nArg < 1 \|\| pVfs == 0 \|\| pVfs->xRealpath == 0 \|\| pVfs->xFileExists == 0 ){` |
|    ! 0 |  865 | `		return PH7_OK;` |
|      - |  866 | `	}` |
|     29 |  867 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|     29 |  868 | `	if( nPath < 0 ){` |
|    ! 0 |  869 | `		nPath = 0;` |
|    ! 0 |  870 | `	}` |
|     29 |  871 | `	nScheme = PH7_VmUrlSchemeLen(zPath,nPath);` |
|     29 |  872 | `	if( nScheme > 0 ){` |
|      - |  873 | `		/* A name that carries a scheme is never walked. php resolves exactly one` |
|      - |  874 | `		 * of them -- file://, which it realpaths where it stands -- and answers` |
|      - |  875 | `		 * false for every other wrapper. An unreachable authority (and any other` |
|      - |  876 | `		 * scheme) comes back unchanged from the strip, and is false. */` |
|      9 |  877 | `		const char *zLocal = PH7_VmFileUrlLocalPath(zPath);` |
|      9 |  878 | `		if( zLocal != zPath ){` |
|      3 |  879 | `			VfsResolveTry(pVfs,pCtx,zLocal);` |
|      1 |  880 | `		}` |
|      9 |  881 | `		return PH7_OK;` |
|      - |  882 | `	}` |
|     20 |  883 | `	if( VfsPathIsDotRelative(zPath,nPath) \|\| VfsPathIsAbsolute(zPath,nPath)` |
|     14 |  884 | `	 \|\| SySetUsed(&pVm->aPaths) < 1 ){` |
|     11 |  885 | `		VfsResolveTry(pVfs,pCtx,zPath);` |
|     11 |  886 | `		return PH7_OK;` |
|      - |  887 | `	}` |
|     11 |  888 | `	c = '/';` |
|      - |  889 | `#ifdef __WINNT__` |
|      1 |  890 | `	c = '\\';` |
|      - |  891 | `#endif` |
|     11 |  892 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|     11 |  893 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|     21 |  894 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|      - |  895 | `		SyString sFile;` |
|     17 |  896 | `		SyStringInitFromBuf(&sFile,zPath,(sxu32)nPath);` |
|     17 |  897 | `		SyBlobReset(&sWorker);` |
|     17 |  898 | `		SyBlobFormat(&sWorker,"%z%c%z",&aEntry[n],c,&sFile);` |
|     17 |  899 | `		if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 |  900 | `			continue;` |
|      - |  901 | `		}` |
|     17 |  902 | `		if( VfsResolveTry(pVfs,pCtx,(const char *)SyBlobData(&sWorker)) ){` |
|      7 |  903 | `			SyBlobRelease(&sWorker);` |
|      7 |  904 | `			return PH7_OK;` |
|      - |  905 | `		}` |
|      6 |  906 | `	}` |
|      - |  907 | `	/* The same last resort the opener uses: the executing file's directory. */` |
|      5 |  908 | `	if( PH7_VmExecutingDir(pVm,&sDir) ){` |
|      - |  909 | `		SyString sFile;` |
|      5 |  910 | `		SyStringInitFromBuf(&sFile,zPath,(sxu32)nPath);` |
|      5 |  911 | `		SyBlobReset(&sWorker);` |
|      5 |  912 | `		SyBlobFormat(&sWorker,"%z%c%z",&sDir,c,&sFile);` |
|      5 |  913 | `		if( SXRET_OK == SyBlobNullAppend(&sWorker) ){` |
|      5 |  914 | `			VfsResolveTry(pVfs,pCtx,(const char *)SyBlobData(&sWorker));` |
|      2 |  915 | `		}` |
|      2 |  916 | `	}` |
|      5 |  917 | `	SyBlobRelease(&sWorker);` |
|      5 |  918 | `	return PH7_OK;` |
|     15 |  919 | `}` |
|      - |  920 | `/*` |
|      - |  921 | ` * int sleep(int $seconds)` |
|      - |  922 | ` *  Delays the program execution for the given number of seconds.` |
|      - |  923 | ` * Parameters` |
|      - |  924 | ` *  $seconds` |
|      - |  925 | ` *   Halt time in seconds.` |
|      - |  926 | ` * Return` |
|      - |  927 | ` *  Zero on success or FALSE on failure.` |
|      - |  928 | ` */` |
|     10 |  929 | `static int PH7_vfs_sleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  930 | `{` |
|      - |  931 | `	ph7_vfs *pVfs;` |
|      - |  932 | `	int rc,nSleep;` |
|     11 |  933 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  934 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  935 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  936 | `		return PH7_OK;` |
|      - |  937 | `	}` |
|      - |  938 | `	/* Point to the underlying vfs */` |
|     11 |  939 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 |  940 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  941 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  942 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  943 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  944 | `			ph7_function_name(pCtx)` |
|      - |  945 | `			);` |
|    ! 0 |  946 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  947 | `		return PH7_OK;` |
|      - |  948 | `	}` |
|      - |  949 | `	/* Amount to sleep */` |
|     11 |  950 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|     11 |  951 | `	if( nSleep < 0 ){` |
|      - |  952 | `		/* php raises rather than sleeping for an unsigned-wrapped eternity */` |
|      3 |  953 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  954 | `			"sleep(): Argument #1 ($seconds) must be greater than or equal to 0");` |
|      - |  955 | `	}` |
|      - |  956 | `	/* Perform the requested operation (Microseconds) */` |
|      9 |  957 | `	rc = pVfs->xSleep((unsigned int)(nSleep * SX_USEC_PER_SEC));` |
|      9 |  958 | `	if( rc != PH7_OK ){` |
|      - |  959 | `		/* Return FALSE */` |
|    ! 0 |  960 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  961 | `	}else{` |
|      - |  962 | `		/* Return zero */` |
|      9 |  963 | `		ph7_result_int(pCtx,0);` |
|      - |  964 | `	}` |
|      9 |  965 | `	return PH7_OK;` |
|      6 |  966 | `}` |
|      - |  967 | `/*` |
|      - |  968 | ` * void usleep(int $micro_seconds)` |
|      - |  969 | ` *  Delays program execution for the given number of micro seconds.` |
|      - |  970 | ` * Parameters` |
|      - |  971 | ` *  $micro_seconds` |
|      - |  972 | ` *   Halt time in micro seconds. A micro second is one millionth of a second.` |
|      - |  973 | ` * Return` |
|      - |  974 | ` *  None.` |
|      - |  975 | ` */` |
|     98 |  976 | `static int PH7_vfs_usleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  977 | `{` |
|      - |  978 | `	ph7_vfs *pVfs;` |
|      - |  979 | `	int nSleep;` |
|    102 |  980 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - |  981 | `		/* Missing/Invalid argument,return immediately */` |
|    ! 0 |  982 | `		return PH7_OK;` |
|      - |  983 | `	}` |
|      - |  984 | `	/* Point to the underlying vfs */` |
|    102 |  985 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    102 |  986 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - |  987 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  988 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  989 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 |  990 | `			ph7_function_name(pCtx)` |
|      - |  991 | `			);` |
|    ! 0 |  992 | `		return PH7_OK;` |
|      - |  993 | `	}` |
|      - |  994 | `	/* Amount to sleep */` |
|    102 |  995 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|    102 |  996 | `	if( nSleep < 0 ){` |
|      - |  997 | `		/* php raises rather than sleeping for an unsigned-wrapped eternity */` |
|      3 |  998 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  999 | `			"usleep(): Argument #1 ($microseconds) must be greater than or equal to 0");` |
|      - | 1000 | `	}` |
|      - | 1001 | `	/* Perform the requested operation (Microseconds) */` |
|    100 | 1002 | `	pVfs->xSleep((unsigned int)nSleep);` |
|    100 | 1003 | `	return PH7_OK;` |
|     53 | 1004 | `}` |
|      - | 1005 | `/*` |
|      - | 1006 | ` * bool unlink (string $filename)` |
|      - | 1007 | ` *  Delete a file.` |
|      - | 1008 | ` * Parameters` |
|      - | 1009 | ` *  $filename` |
|      - | 1010 | ` *   Path to the file.` |
|      - | 1011 | ` * Return` |
|      - | 1012 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1013 | ` */` |
|  44610 | 1014 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1015 | `{` |
|      - | 1016 | `	const char *zPath;` |
|      - | 1017 | `	ph7_vfs *pVfs;` |
|  44615 | 1018 | `	int rc,bThrew = 0;` |
|  44615 | 1019 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1020 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1021 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1022 | `		return PH7_OK;` |
|      - | 1023 | `	}` |
|      - | 1024 | ``	/* php's `?resource $context`, refused when it is a resource of another kind.`` |
|      - | 1025 | `	 * Nothing here CONSUMES it — this operation never opens a stream, and the` |
|      - | 1026 | `	 * userland wrapper's unlink/rename/mkdir/rmdir methods are not dispatched` |
|      - | 1027 | `	 * (§7.4 slice-2 (e)) — but the refusal is the argument's contract, and it` |
|      - | 1028 | `	 * was accepted in silence. */` |
|  44615 | 1029 | `	PH7_StreamCtxFromArg(pCtx,nArg,apArg,1,"$context",0,&bThrew);` |
|  44615 | 1030 | `	if( bThrew ){` |
|      8 | 1031 | `		return PH7_OK;` |
|      - | 1032 | `	}` |
|      - | 1033 | `	/* Point to the underlying vfs */` |
|  44609 | 1034 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  44609 | 1035 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - | 1036 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1037 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1038 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1039 | `			ph7_function_name(pCtx)` |
|      - | 1040 | `			);` |
|    ! 0 | 1041 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1042 | `		return PH7_OK;` |
|      - | 1043 | `	}` |
|      - | 1044 | `	/* Point to the desired directory */` |
|  44609 | 1045 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1046 | `	/* Perform the requested operation */` |
|  44609 | 1047 | `	errno = 0;` |
|  44609 | 1048 | `	rc = pVfs->xUnlink(zPath);` |
|  44609 | 1049 | `	if( rc != PH7_OK ){` |
|  27437 | 1050 | `		VfsThrowSysWarning(pCtx,zPath);` |
|  13716 | 1051 | `	}` |
|      - | 1052 | `	/* IO return value */` |
|  44609 | 1053 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  44609 | 1054 | `	return PH7_OK;` |
|  22310 | 1055 | `}` |
|      - | 1056 | `/*` |
|      - | 1057 | ` * bool chmod(string $filename,int $mode)` |
|      - | 1058 | ` *  Attempts to change the mode of the specified file to that given in mode.` |
|      - | 1059 | ` * Parameters` |
|      - | 1060 | ` *  $filename` |
|      - | 1061 | ` *   Path to the file.` |
|      - | 1062 | ` * $mode` |
|      - | 1063 | ` *   Mode (Must be an integer)` |
|      - | 1064 | ` * Return` |
|      - | 1065 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1066 | ` */` |
|    602 | 1067 | `static int PH7_vfs_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1068 | `{` |
|      - | 1069 | `	const char *zPath;` |
|      - | 1070 | `	ph7_vfs *pVfs;` |
|      - | 1071 | `	int iMode;` |
|      - | 1072 | `	int rc;` |
|    607 | 1073 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1074 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1075 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1076 | `		return PH7_OK;` |
|      - | 1077 | `	}` |
|      - | 1078 | `	/* Point to the underlying vfs */` |
|    607 | 1079 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    607 | 1080 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - | 1081 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1082 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1083 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1084 | `			ph7_function_name(pCtx)` |
|      - | 1085 | `			);` |
|    ! 0 | 1086 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1087 | `		return PH7_OK;` |
|      - | 1088 | `	}` |
|      - | 1089 | `	/* Point to the desired directory */` |
|    607 | 1090 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1091 | `	/* Extract the mode */` |
|    607 | 1092 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - | 1093 | `	/* Perform the requested operation */` |
|    607 | 1094 | `	rc = pVfs->xChmod(zPath,iMode);` |
|      - | 1095 | `	/* IO return value */` |
|    607 | 1096 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    607 | 1097 | `	return PH7_OK;` |
|    306 | 1098 | `}` |
|      - | 1099 | `/*` |
|      - | 1100 | ` * bool chown(string $filename,string $user)` |
|      - | 1101 | ` *  Attempts to change the owner of the file filename to user user.` |
|      - | 1102 | ` * Parameters` |
|      - | 1103 | ` *  $filename` |
|      - | 1104 | ` *   Path to the file.` |
|      - | 1105 | ` * $user` |
|      - | 1106 | ` *   Username.` |
|      - | 1107 | ` * Return` |
|      - | 1108 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1109 | ` */` |
|      6 | 1110 | `static int PH7_vfs_chown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1111 | `{` |
|      - | 1112 | `	const char *zPath,*zUser;` |
|      - | 1113 | `	ph7_vfs *pVfs;` |
|      - | 1114 | `	int rc;` |
|      7 | 1115 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1116 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1117 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1118 | `		return PH7_OK;` |
|      - | 1119 | `	}` |
|      - | 1120 | `	/* Point to the underlying vfs */` |
|      7 | 1121 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 1122 | `	if( pVfs == 0 \|\| pVfs->xChown == 0 ){` |
|      - | 1123 | `		/* IO routine not implemented,return NULL */` |
|      1 | 1124 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1125 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1126 | `			ph7_function_name(pCtx)` |
|      - | 1127 | `			);` |
|      1 | 1128 | `		ph7_result_bool(pCtx,0);` |
|      1 | 1129 | `		return PH7_OK;` |
|      - | 1130 | `	}` |
|      - | 1131 | `	/* Point to the desired directory */` |
|      6 | 1132 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1133 | `	/* Extract the user */` |
|      6 | 1134 | `	zUser = ph7_value_to_string(apArg[1],0);` |
|      - | 1135 | `	/* Perform the requested operation */` |
|      6 | 1136 | `	errno = 0;` |
|      6 | 1137 | `	rc = pVfs->xChown(zPath,zUser);` |
|      6 | 1138 | `	if( rc != PH7_OK ){` |
|      - | 1139 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - | 1140 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - | 1141 | `		 * "chown(): Operation not permitted". */` |
|      6 | 1142 | `		if( rc == -2 ){` |
|      3 | 1143 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find uid for %s",` |
|      1 | 1144 | `				ph7_function_name(pCtx),zUser);` |
|      1 | 1145 | `		}else{` |
|      6 | 1146 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 | 1147 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - | 1148 | `		}` |
|      3 | 1149 | `	}` |
|      - | 1150 | `	/* IO return value */` |
|      6 | 1151 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 | 1152 | `	return PH7_OK;` |
|      4 | 1153 | `}` |
|      - | 1154 | `/*` |
|      - | 1155 | ` * bool chgrp(string $filename,string $group)` |
|      - | 1156 | ` *  Attempts to change the group of the file filename to group.` |
|      - | 1157 | ` * Parameters` |
|      - | 1158 | ` *  $filename` |
|      - | 1159 | ` *   Path to the file.` |
|      - | 1160 | ` * $group` |
|      - | 1161 | ` *   groupname.` |
|      - | 1162 | ` * Return` |
|      - | 1163 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1164 | ` */` |
|      6 | 1165 | `static int PH7_vfs_chgrp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1166 | `{` |
|      - | 1167 | `	const char *zPath,*zGroup;` |
|      - | 1168 | `	ph7_vfs *pVfs;` |
|      - | 1169 | `	int rc;` |
|      7 | 1170 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1171 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1172 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1173 | `		return PH7_OK;` |
|      - | 1174 | `	}` |
|      - | 1175 | `	/* Point to the underlying vfs */` |
|      7 | 1176 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 1177 | `	if( pVfs == 0 \|\| pVfs->xChgrp == 0 ){` |
|      - | 1178 | `		/* IO routine not implemented,return NULL */` |
|      1 | 1179 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1180 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1181 | `			ph7_function_name(pCtx)` |
|      - | 1182 | `			);` |
|      1 | 1183 | `		ph7_result_bool(pCtx,0);` |
|      1 | 1184 | `		return PH7_OK;` |
|      - | 1185 | `	}` |
|      - | 1186 | `	/* Point to the desired directory */` |
|      6 | 1187 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1188 | `	/* Extract the user */` |
|      6 | 1189 | `	zGroup = ph7_value_to_string(apArg[1],0);` |
|      - | 1190 | `	/* Perform the requested operation */` |
|      6 | 1191 | `	errno = 0;` |
|      6 | 1192 | `	rc = pVfs->xChgrp(zPath,zGroup);` |
|      6 | 1193 | `	if( rc != PH7_OK ){` |
|      - | 1194 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - | 1195 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - | 1196 | `		 * "chown(): Operation not permitted". */` |
|      6 | 1197 | `		if( rc == -2 ){` |
|      3 | 1198 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find gid for %s",` |
|      1 | 1199 | `				ph7_function_name(pCtx),zGroup);` |
|      1 | 1200 | `		}else{` |
|      6 | 1201 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 | 1202 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - | 1203 | `		}` |
|      3 | 1204 | `	}` |
|      - | 1205 | `	/* IO return value */` |
|      6 | 1206 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 | 1207 | `	return PH7_OK;` |
|      4 | 1208 | `}` |
|      - | 1209 | `/*` |
|      - | 1210 | ` * int64 disk_free_space(string $directory)` |
|      - | 1211 | ` *  Returns available space on filesystem or disk partition.` |
|      - | 1212 | ` * Parameters` |
|      - | 1213 | ` *  $directory` |
|      - | 1214 | ` *   A directory of the filesystem or disk partition.` |
|      - | 1215 | ` * Return` |
|      - | 1216 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - | 1217 | ` */` |
|     14 | 1218 | `static int PH7_vfs_disk_free_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1219 | `{` |
|      - | 1220 | `	const char *zPath;` |
|      - | 1221 | `	ph7_int64 iSize;` |
|      - | 1222 | `	ph7_vfs *pVfs;` |
|     15 | 1223 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1224 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1225 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1226 | `		return PH7_OK;` |
|      - | 1227 | `	}` |
|      - | 1228 | `	/* Point to the underlying vfs */` |
|     15 | 1229 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     15 | 1230 | `	if( pVfs == 0 \|\| pVfs->xFreeSpace == 0 ){` |
|      - | 1231 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1232 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1233 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1234 | `			ph7_function_name(pCtx)` |
|      - | 1235 | `			);` |
|    ! 0 | 1236 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1237 | `		return PH7_OK;` |
|      - | 1238 | `	}` |
|      - | 1239 | `	/* Point to the desired directory */` |
|     15 | 1240 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1241 | `	/* Perform the requested operation */` |
|     15 | 1242 | `	errno = 0;` |
|     15 | 1243 | `	iSize = pVfs->xFreeSpace(zPath);` |
|     15 | 1244 | `	if( iSize < 0 ){` |
|      - | 1245 | `		/* php answers FALSE and warns with the C library's own reason` |
|      - | 1246 | ``		 * (`disk_free_space(): No such file or directory`). PHL answered int(-1) --`` |
|      - | 1247 | `		 * truthy, and a plausible-looking byte count for a caller that only tests` |
|      - | 1248 | ``		 * `if ($free)` or compares it against a threshold. */`` |
|      7 | 1249 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 | 1250 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|      5 | 1251 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1252 | `		return PH7_OK;` |
|      - | 1253 | `	}` |
|      - | 1254 | `	/* php's return type is FLOAT, not int: the value can exceed what an int holds` |
|      - | 1255 | ``	 * on a large volume, and a `===`/gettype()/json_encode() reader sees the`` |
|      - | 1256 | `	 * difference on every volume. */` |
|     11 | 1257 | `	ph7_result_double(pCtx,(double)iSize);` |
|     11 | 1258 | `	return PH7_OK;` |
|      8 | 1259 | `}` |
|      - | 1260 | `/*` |
|      - | 1261 | ` * int64 disk_total_space(string $directory)` |
|      - | 1262 | ` *  Returns the total size of a filesystem or disk partition.` |
|      - | 1263 | ` * Parameters` |
|      - | 1264 | ` *  $directory` |
|      - | 1265 | ` *   A directory of the filesystem or disk partition.` |
|      - | 1266 | ` * Return` |
|      - | 1267 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - | 1268 | ` */` |
|     10 | 1269 | `static int PH7_vfs_disk_total_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1270 | `{` |
|      - | 1271 | `	const char *zPath;` |
|      - | 1272 | `	ph7_int64 iSize;` |
|      - | 1273 | `	ph7_vfs *pVfs;` |
|     11 | 1274 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1275 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1276 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1277 | `		return PH7_OK;` |
|      - | 1278 | `	}` |
|      - | 1279 | `	/* Point to the underlying vfs */` |
|     11 | 1280 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 1281 | `	if( pVfs == 0 \|\| pVfs->xTotalSpace == 0 ){` |
|      - | 1282 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1283 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1284 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1285 | `			ph7_function_name(pCtx)` |
|      - | 1286 | `			);` |
|    ! 0 | 1287 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1288 | `		return PH7_OK;` |
|      - | 1289 | `	}` |
|      - | 1290 | `	/* Point to the desired directory */` |
|     11 | 1291 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1292 | `	/* Perform the requested operation */` |
|     11 | 1293 | `	errno = 0;` |
|     11 | 1294 | `	iSize = pVfs->xTotalSpace(zPath);` |
|     11 | 1295 | `	if( iSize < 0 ){` |
|      - | 1296 | `		/* php answers FALSE and warns with the C library's own reason` |
|      - | 1297 | ``		 * (`disk_free_space(): No such file or directory`). PHL answered int(-1) --`` |
|      - | 1298 | `		 * truthy, and a plausible-looking byte count for a caller that only tests` |
|      - | 1299 | ``		 * `if ($free)` or compares it against a threshold. */`` |
|      4 | 1300 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      2 | 1301 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|      3 | 1302 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1303 | `		return PH7_OK;` |
|      - | 1304 | `	}` |
|      - | 1305 | `	/* php's return type is FLOAT, not int: the value can exceed what an int holds` |
|      - | 1306 | ``	 * on a large volume, and a `===`/gettype()/json_encode() reader sees the`` |
|      - | 1307 | `	 * difference on every volume. */` |
|      9 | 1308 | `	ph7_result_double(pCtx,(double)iSize);` |
|      9 | 1309 | `	return PH7_OK;` |
|      6 | 1310 | `}` |
|      - | 1311 | `/*` |
|      - | 1312 | ` * bool file_exists(string $filename)` |
|      - | 1313 | ` *  Checks whether a file or directory exists.` |
|      - | 1314 | ` * Parameters` |
|      - | 1315 | ` *  $filename` |
|      - | 1316 | ` *   Path to the file.` |
|      - | 1317 | ` * Return` |
|      - | 1318 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1319 | ` */` |
|    830 | 1320 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1321 | `{` |
|      - | 1322 | `	const char *zPath;` |
|      - | 1323 | `	ph7_vfs *pVfs;` |
|      - | 1324 | `	int rc;` |
|    835 | 1325 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1326 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1327 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1328 | `		return PH7_OK;` |
|      - | 1329 | `	}` |
|      - | 1330 | `	/* Point to the underlying vfs */` |
|    835 | 1331 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    835 | 1332 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - | 1333 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1334 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1335 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1336 | `			ph7_function_name(pCtx)` |
|      - | 1337 | `			);` |
|    ! 0 | 1338 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1339 | `		return PH7_OK;` |
|      - | 1340 | `	}` |
|      - | 1341 | `	/* Point to the desired directory */` |
|    835 | 1342 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1343 | `	/* Perform the requested operation */` |
|    835 | 1344 | `	rc = pVfs->xFileExists(zPath);` |
|      - | 1345 | `	/* IO return value */` |
|    835 | 1346 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    835 | 1347 | `	return PH7_OK;` |
|    420 | 1348 | `}` |
|      - | 1349 | `/*` |
|      - | 1350 | ` * int64 file_size(string $filename)` |
|      - | 1351 | ` *  Gets the size for the given file.` |
|      - | 1352 | ` * Parameters` |
|      - | 1353 | ` *  $filename` |
|      - | 1354 | ` *   Path to the file.` |
|      - | 1355 | ` * Return` |
|      - | 1356 | ` *  File size on success or FALSE on failure.` |
|      - | 1357 | ` */` |
|     22 | 1358 | `static int PH7_vfs_file_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1359 | `{` |
|      - | 1360 | `	const char *zPath;` |
|      - | 1361 | `	ph7_int64 iSize;` |
|      - | 1362 | `	ph7_vfs *pVfs;` |
|     25 | 1363 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1364 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1365 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1366 | `		return PH7_OK;` |
|      - | 1367 | `	}` |
|      - | 1368 | `	/* Point to the underlying vfs */` |
|     25 | 1369 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     25 | 1370 | `	if( pVfs == 0 \|\| pVfs->xFileSize == 0 ){` |
|      - | 1371 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1372 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1373 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1374 | `			ph7_function_name(pCtx)` |
|      - | 1375 | `			);` |
|    ! 0 | 1376 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1377 | `		return PH7_OK;` |
|      - | 1378 | `	}` |
|      - | 1379 | `	/* Point to the desired directory */` |
|     25 | 1380 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1381 | `	/* Perform the requested operation */` |
|     25 | 1382 | `	iSize = pVfs->xFileSize(zPath);` |
|     25 | 1383 | `	if( iSize < 0 ){` |
|      - | 1384 | `		/* php: a stat failure warns and returns FALSE -- PH7 returned int(-1), which is` |
|      - | 1385 | `		 * truthy and compares equal to nothing a caller would test for. */` |
|      4 | 1386 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): stat failed for %s",` |
|      1 | 1387 | `			ph7_function_name(pCtx),zPath);` |
|      3 | 1388 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1389 | `		return PH7_OK;` |
|      - | 1390 | `	}` |
|      - | 1391 | `	/* IO return value */` |
|     23 | 1392 | `	ph7_result_int64(pCtx,iSize);` |
|     23 | 1393 | `	return PH7_OK;` |
|     14 | 1394 | `}` |
|      - | 1395 | `/*` |
|      - | 1396 | ` * int64 fileatime(string $filename)` |
|      - | 1397 | ` *  Gets the last access time of the given file.` |
|      - | 1398 | ` * Parameters` |
|      - | 1399 | ` *  $filename` |
|      - | 1400 | ` *   Path to the file.` |
|      - | 1401 | ` * Return` |
|      - | 1402 | ` *  File atime on success or FALSE on failure.` |
|      - | 1403 | ` */` |
|      8 | 1404 | `static int PH7_vfs_file_atime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1405 | `{` |
|      - | 1406 | `	const char *zPath;` |
|      - | 1407 | `	ph7_int64 iTime;` |
|      - | 1408 | `	ph7_vfs *pVfs;` |
|      9 | 1409 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1410 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1411 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1412 | `		return PH7_OK;` |
|      - | 1413 | `	}` |
|      - | 1414 | `	/* Point to the underlying vfs */` |
|      9 | 1415 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 | 1416 | `	if( pVfs == 0 \|\| pVfs->xFileAtime == 0 ){` |
|      - | 1417 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1418 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1419 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1420 | `			ph7_function_name(pCtx)` |
|      - | 1421 | `			);` |
|    ! 0 | 1422 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1423 | `		return PH7_OK;` |
|      - | 1424 | `	}` |
|      - | 1425 | `	/* Point to the desired directory */` |
|      9 | 1426 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1427 | `	/* Perform the requested operation */` |
|      9 | 1428 | `	iTime = pVfs->xFileAtime(zPath);` |
|      9 | 1429 | `	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){` |
|      - | 1430 | `		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --` |
|      - | 1431 | `		 * truthy, and one second before the epoch is also a real timestamp, which` |
|      - | 1432 | `		 * is why the verdict comes from the VFS rather than from the value. */` |
|      3 | 1433 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      3 | 1434 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1435 | `		return PH7_OK;` |
|      - | 1436 | `	}` |
|      - | 1437 | `	/* IO return value */` |
|      7 | 1438 | `	ph7_result_int64(pCtx,iTime);` |
|      7 | 1439 | `	return PH7_OK;` |
|      5 | 1440 | `}` |
|      - | 1441 | `/*` |
|      - | 1442 | ` * int64 filemtime(string $filename)` |
|      - | 1443 | ` *  Gets file modification time.` |
|      - | 1444 | ` * Parameters` |
|      - | 1445 | ` *  $filename` |
|      - | 1446 | ` *   Path to the file.` |
|      - | 1447 | ` * Return` |
|      - | 1448 | ` *  File mtime on success or FALSE on failure.` |
|      - | 1449 | ` */` |
|     24 | 1450 | `static int PH7_vfs_file_mtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1451 | `{` |
|      - | 1452 | `	const char *zPath;` |
|      - | 1453 | `	ph7_int64 iTime;` |
|      - | 1454 | `	ph7_vfs *pVfs;` |
|     28 | 1455 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1456 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1457 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1458 | `		return PH7_OK;` |
|      - | 1459 | `	}` |
|      - | 1460 | `	/* Point to the underlying vfs */` |
|     28 | 1461 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     28 | 1462 | `	if( pVfs == 0 \|\| pVfs->xFileMtime == 0 ){` |
|      - | 1463 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1464 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1465 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1466 | `			ph7_function_name(pCtx)` |
|      - | 1467 | `			);` |
|    ! 0 | 1468 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1469 | `		return PH7_OK;` |
|      - | 1470 | `	}` |
|      - | 1471 | `	/* Point to the desired directory */` |
|     28 | 1472 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1473 | `	/* Perform the requested operation */` |
|     28 | 1474 | `	iTime = pVfs->xFileMtime(zPath);` |
|     28 | 1475 | `	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){` |
|      - | 1476 | `		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --` |
|      - | 1477 | `		 * truthy, and one second before the epoch is also a real timestamp, which` |
|      - | 1478 | `		 * is why the verdict comes from the VFS rather than from the value. */` |
|      3 | 1479 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      3 | 1480 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1481 | `		return PH7_OK;` |
|      - | 1482 | `	}` |
|      - | 1483 | `	/* IO return value */` |
|     26 | 1484 | `	ph7_result_int64(pCtx,iTime);` |
|     26 | 1485 | `	return PH7_OK;` |
|     16 | 1486 | `}` |
|      - | 1487 | `/*` |
|      - | 1488 | ` * int64 filectime(string $filename)` |
|      - | 1489 | ` *  Gets inode change time of file.` |
|      - | 1490 | ` * Parameters` |
|      - | 1491 | ` *  $filename` |
|      - | 1492 | ` *   Path to the file.` |
|      - | 1493 | ` * Return` |
|      - | 1494 | ` *  File ctime on success or FALSE on failure.` |
|      - | 1495 | ` */` |
|      6 | 1496 | `static int PH7_vfs_file_ctime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1497 | `{` |
|      - | 1498 | `	const char *zPath;` |
|      - | 1499 | `	ph7_int64 iTime;` |
|      - | 1500 | `	ph7_vfs *pVfs;` |
|      7 | 1501 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1502 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1503 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1504 | `		return PH7_OK;` |
|      - | 1505 | `	}` |
|      - | 1506 | `	/* Point to the underlying vfs */` |
|      7 | 1507 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 1508 | `	if( pVfs == 0 \|\| pVfs->xFileCtime == 0 ){` |
|      - | 1509 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1510 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1511 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1512 | `			ph7_function_name(pCtx)` |
|      - | 1513 | `			);` |
|    ! 0 | 1514 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1515 | `		return PH7_OK;` |
|      - | 1516 | `	}` |
|      - | 1517 | `	/* Point to the desired directory */` |
|      7 | 1518 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1519 | `	/* Perform the requested operation */` |
|      7 | 1520 | `	iTime = pVfs->xFileCtime(zPath);` |
|      7 | 1521 | `	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){` |
|      - | 1522 | `		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --` |
|      - | 1523 | `		 * truthy, and one second before the epoch is also a real timestamp, which` |
|      - | 1524 | `		 * is why the verdict comes from the VFS rather than from the value. */` |
|      3 | 1525 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      3 | 1526 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1527 | `		return PH7_OK;` |
|      - | 1528 | `	}` |
|      - | 1529 | `	/* IO return value */` |
|      5 | 1530 | `	ph7_result_int64(pCtx,iTime);` |
|      5 | 1531 | `	return PH7_OK;` |
|      4 | 1532 | `}` |
|      - | 1533 | `/*` |
|      - | 1534 | ` * bool is_file(string $filename)` |
|      - | 1535 | ` *  Tells whether the filename is a regular file.` |
|      - | 1536 | ` * Parameters` |
|      - | 1537 | ` *  $filename` |
|      - | 1538 | ` *   Path to the file.` |
|      - | 1539 | ` * Return` |
|      - | 1540 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1541 | ` */` |
|   8854 | 1542 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1543 | `{` |
|      - | 1544 | `	const char *zPath;` |
|      - | 1545 | `	ph7_vfs *pVfs;` |
|      - | 1546 | `	int rc;` |
|   8859 | 1547 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1548 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1549 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1550 | `		return PH7_OK;` |
|      - | 1551 | `	}` |
|      - | 1552 | `	/* Point to the underlying vfs */` |
|   8859 | 1553 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   8859 | 1554 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - | 1555 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1556 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1557 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1558 | `			ph7_function_name(pCtx)` |
|      - | 1559 | `			);` |
|    ! 0 | 1560 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1561 | `		return PH7_OK;` |
|      - | 1562 | `	}` |
|      - | 1563 | `	/* Point to the desired directory */` |
|   8859 | 1564 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1565 | `	/* Perform the requested operation */` |
|   8859 | 1566 | `	rc = pVfs->xIsfile(zPath);` |
|      - | 1567 | `	/* IO return value */` |
|   8859 | 1568 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   8859 | 1569 | `	return PH7_OK;` |
|   4432 | 1570 | `}` |
|      - | 1571 | `/*` |
|      - | 1572 | ` * bool is_link(string $filename)` |
|      - | 1573 | ` *  Tells whether the filename is a symbolic link.` |
|      - | 1574 | ` * Parameters` |
|      - | 1575 | ` *  $filename` |
|      - | 1576 | ` *   Path to the file.` |
|      - | 1577 | ` * Return` |
|      - | 1578 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1579 | ` */` |
|     12 | 1580 | `static int PH7_vfs_is_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 1581 | `{` |
|      - | 1582 | `	const char *zPath;` |
|      - | 1583 | `	ph7_vfs *pVfs;` |
|      - | 1584 | `	int rc;` |
|     12 | 1585 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1586 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1587 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1588 | `		return PH7_OK;` |
|      - | 1589 | `	}` |
|      - | 1590 | `	/* Point to the underlying vfs */` |
|     12 | 1591 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     12 | 1592 | `	if( pVfs == 0 \|\| pVfs->xIslink == 0 ){` |
|      - | 1593 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1594 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1595 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1596 | `			ph7_function_name(pCtx)` |
|      - | 1597 | `			);` |
|    ! 0 | 1598 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1599 | `		return PH7_OK;` |
|      - | 1600 | `	}` |
|      - | 1601 | `	/* Point to the desired directory */` |
|     12 | 1602 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1603 | `	/* Perform the requested operation */` |
|     12 | 1604 | `	rc = pVfs->xIslink(zPath);` |
|      - | 1605 | `	/* IO return value */` |
|     12 | 1606 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     12 | 1607 | `	return PH7_OK;` |
|      6 | 1608 | `}` |
|      - | 1609 | `/*` |
|      - | 1610 | ` * bool is_readable(string $filename)` |
|      - | 1611 | ` *  Tells whether a file exists and is readable.` |
|      - | 1612 | ` * Parameters` |
|      - | 1613 | ` *  $filename` |
|      - | 1614 | ` *   Path to the file.` |
|      - | 1615 | ` * Return` |
|      - | 1616 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1617 | ` */` |
|      2 | 1618 | `static int PH7_vfs_is_readable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1619 | `{` |
|      - | 1620 | `	const char *zPath;` |
|      - | 1621 | `	ph7_vfs *pVfs;` |
|      - | 1622 | `	int rc;` |
|      3 | 1623 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1624 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1625 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1626 | `		return PH7_OK;` |
|      - | 1627 | `	}` |
|      - | 1628 | `	/* Point to the underlying vfs */` |
|      3 | 1629 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1630 | `	if( pVfs == 0 \|\| pVfs->xReadable == 0 ){` |
|      - | 1631 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1632 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1633 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1634 | `			ph7_function_name(pCtx)` |
|      - | 1635 | `			);` |
|    ! 0 | 1636 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1637 | `		return PH7_OK;` |
|      - | 1638 | `	}` |
|      - | 1639 | `	/* Point to the desired directory */` |
|      3 | 1640 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1641 | `	/* Perform the requested operation */` |
|      3 | 1642 | `	rc = pVfs->xReadable(zPath);` |
|      - | 1643 | `	/* IO return value */` |
|      3 | 1644 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 1645 | `	return PH7_OK;` |
|      2 | 1646 | `}` |
|      - | 1647 | `/*` |
|      - | 1648 | ` * bool is_writable(string $filename)` |
|      - | 1649 | ` *  Tells whether the filename is writable.` |
|      - | 1650 | ` * Parameters` |
|      - | 1651 | ` *  $filename` |
|      - | 1652 | ` *   Path to the file.` |
|      - | 1653 | ` * Return` |
|      - | 1654 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1655 | ` */` |
|      4 | 1656 | `static int PH7_vfs_is_writable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1657 | `{` |
|      - | 1658 | `	const char *zPath;` |
|      - | 1659 | `	ph7_vfs *pVfs;` |
|      - | 1660 | `	int rc;` |
|      5 | 1661 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1662 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1663 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1664 | `		return PH7_OK;` |
|      - | 1665 | `	}` |
|      - | 1666 | `	/* Point to the underlying vfs */` |
|      5 | 1667 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1668 | `	if( pVfs == 0 \|\| pVfs->xWritable == 0 ){` |
|      - | 1669 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1670 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1671 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1672 | `			ph7_function_name(pCtx)` |
|      - | 1673 | `			);` |
|    ! 0 | 1674 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1675 | `		return PH7_OK;` |
|      - | 1676 | `	}` |
|      - | 1677 | `	/* Point to the desired directory */` |
|      5 | 1678 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1679 | `	/* Perform the requested operation */` |
|      5 | 1680 | `	rc = pVfs->xWritable(zPath);` |
|      - | 1681 | `	/* IO return value */` |
|      5 | 1682 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 1683 | `	return PH7_OK;` |
|      3 | 1684 | `}` |
|      - | 1685 | `/*` |
|      - | 1686 | ` * bool is_executable(string $filename)` |
|      - | 1687 | ` *  Tells whether the filename is executable.` |
|      - | 1688 | ` * Parameters` |
|      - | 1689 | ` *  $filename` |
|      - | 1690 | ` *   Path to the file.` |
|      - | 1691 | ` * Return` |
|      - | 1692 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1693 | ` */` |
|      4 | 1694 | `static int PH7_vfs_is_executable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1695 | `{` |
|      - | 1696 | `	const char *zPath;` |
|      - | 1697 | `	ph7_vfs *pVfs;` |
|      - | 1698 | `	int rc;` |
|      6 | 1699 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1700 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1701 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1702 | `		return PH7_OK;` |
|      - | 1703 | `	}` |
|      - | 1704 | `	/* Point to the underlying vfs */` |
|      6 | 1705 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      6 | 1706 | `	if( pVfs == 0 \|\| pVfs->xExecutable == 0 ){` |
|      - | 1707 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1708 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1709 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1710 | `			ph7_function_name(pCtx)` |
|      - | 1711 | `			);` |
|    ! 0 | 1712 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1713 | `		return PH7_OK;` |
|      - | 1714 | `	}` |
|      - | 1715 | `	/* Point to the desired directory */` |
|      6 | 1716 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1717 | `	/* Perform the requested operation */` |
|      6 | 1718 | `	rc = pVfs->xExecutable(zPath);` |
|      - | 1719 | `	/* IO return value */` |
|      6 | 1720 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 | 1721 | `	return PH7_OK;` |
|      4 | 1722 | `}` |
|      - | 1723 | `/*` |
|      - | 1724 | ` * string filetype(string $filename)` |
|      - | 1725 | ` *  Gets file type.` |
|      - | 1726 | ` * Parameters` |
|      - | 1727 | ` *  $filename` |
|      - | 1728 | ` *   Path to the file.` |
|      - | 1729 | ` * Return` |
|      - | 1730 | ` *  The type of the file. Possible values are fifo, char, dir, block, link` |
|      - | 1731 | ` *  file, socket and unknown.` |
|      - | 1732 | ` */` |
|     18 | 1733 | `static int PH7_vfs_filetype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1734 | `{` |
|      - | 1735 | `	const char *zPath;` |
|      - | 1736 | `	ph7_vfs *pVfs;` |
|     19 | 1737 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1738 | `		/* Missing/Invalid argument,return 'unknown' */` |
|    ! 0 | 1739 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 1740 | `		return PH7_OK;` |
|      - | 1741 | `	}` |
|      - | 1742 | `	/* Point to the underlying vfs */` |
|     19 | 1743 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     19 | 1744 | `	if( pVfs == 0 \|\| pVfs->xFiletype == 0 ){` |
|      - | 1745 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1746 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1747 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1748 | `			ph7_function_name(pCtx)` |
|      - | 1749 | `			);` |
|    ! 0 | 1750 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1751 | `		return PH7_OK;` |
|      - | 1752 | `	}` |
|      - | 1753 | `	/* Point to the desired directory */` |
|     19 | 1754 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1755 | `	/* Set the empty string as the default return value */` |
|     19 | 1756 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1757 | `	/* Perform the requested operation */` |
|     19 | 1758 | `	if( pVfs->xFiletype(zPath,pCtx) != PH7_OK ){` |
|      - | 1759 | `		/* php LSTATs here (which is why a symlink answers "link") and a failure is` |
|      - | 1760 | ``		 * the `Lstat failed for` warning plus FALSE. PHL answered the string`` |
|      - | 1761 | `		 * "unknown" -- a real return value of this function, so a caller could not` |
|      - | 1762 | `		 * tell a missing path from a socket or a fifo. */` |
|      3 | 1763 | `		VfsThrowStatWarning(pCtx,zPath,1);` |
|      3 | 1764 | `		ph7_result_bool(pCtx,0);` |
|      1 | 1765 | `	}` |
|     19 | 1766 | `	return PH7_OK;` |
|     10 | 1767 | `}` |
|      - | 1768 | `/*` |
|      - | 1769 | ` * array stat(string $filename)` |
|      - | 1770 | ` *  Gives information about a file.` |
|      - | 1771 | ` * Parameters` |
|      - | 1772 | ` *  $filename` |
|      - | 1773 | ` *   Path to the file.` |
|      - | 1774 | ` * Return` |
|      - | 1775 | ` *  An associative array on success holding the following entries on success` |
|      - | 1776 | ` *  0   dev     device number` |
|      - | 1777 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1778 | ` * 2    mode    inode protection mode` |
|      - | 1779 | ` * 3    nlink   number of links` |
|      - | 1780 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1781 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1782 | ` * 6    rdev    device type, if inode device` |
|      - | 1783 | ` * 7    size    size in bytes` |
|      - | 1784 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1785 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1786 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1787 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1788 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1789 | ` * Note:` |
|      - | 1790 | ` *  FALSE is returned on failure.` |
|      - | 1791 | ` */` |
|     16 | 1792 | `static int PH7_vfs_stat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1793 | `{` |
|      - | 1794 | `	ph7_value *pArray,*pValue;` |
|      - | 1795 | `	const char *zPath;` |
|      - | 1796 | `	ph7_vfs *pVfs;` |
|      - | 1797 | `	int rc;` |
|     17 | 1798 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1799 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1800 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1801 | `		return PH7_OK;` |
|      - | 1802 | `	}` |
|      - | 1803 | `	/* Point to the underlying vfs */` |
|     17 | 1804 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     17 | 1805 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|      - | 1806 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1807 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1808 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1809 | `			ph7_function_name(pCtx)` |
|      - | 1810 | `			);` |
|    ! 0 | 1811 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1812 | `		return PH7_OK;` |
|      - | 1813 | `	}` |
|      - | 1814 | `	/* Create the array and the working value */` |
|     17 | 1815 | `	pArray = ph7_context_new_array(pCtx);` |
|     17 | 1816 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     17 | 1817 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1818 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1819 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1820 | `		return PH7_OK;` |
|      - | 1821 | `	}` |
|      - | 1822 | `	/* Extract the file path */` |
|     17 | 1823 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1824 | `	/* Perform the requested operation */` |
|     17 | 1825 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|     17 | 1826 | `	if( rc != PH7_OK ){` |
|      - | 1827 | ``		/* php warns before answering FALSE -- the same `stat failed for` /`` |
|      - | 1828 | ``		 * `Lstat failed for` line the rest of the family raises. PHL returned the`` |
|      - | 1829 | `		 * FALSE in silence, so a missing path and an empty result looked alike. */` |
|      3 | 1830 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      3 | 1831 | `		ph7_result_bool(pCtx,0);` |
|      2 | 1832 | `	}else{` |
|      - | 1833 | `		/* php's answer is the thirteen fields TWICE: numeric 0..12 then named. */` |
|     15 | 1834 | `		ph7_value *pFull = ph7_context_new_array(pCtx);` |
|     15 | 1835 | `		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){` |
|     15 | 1836 | `			ph7_result_value(pCtx,pFull);` |
|      8 | 1837 | `		}else{` |
|    ! 0 | 1838 | `			ph7_result_value(pCtx,pArray);` |
|      - | 1839 | `		}` |
|      - | 1840 | `	}` |
|      - | 1841 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1842 | `	 * automatically as soon we return from this function. */` |
|     17 | 1843 | `	return PH7_OK;` |
|      9 | 1844 | `}` |
|      - | 1845 | `/*` |
|      - | 1846 | ` * array lstat(string $filename)` |
|      - | 1847 | ` *  Gives information about a file or symbolic link.` |
|      - | 1848 | ` * Parameters` |
|      - | 1849 | ` *  $filename` |
|      - | 1850 | ` *   Path to the file.` |
|      - | 1851 | ` * Return` |
|      - | 1852 | ` *  An associative array on success holding the following entries on success` |
|      - | 1853 | ` *  0   dev     device number` |
|      - | 1854 | ` * 1    ino     inode number (zero on windows)` |
|      - | 1855 | ` * 2    mode    inode protection mode` |
|      - | 1856 | ` * 3    nlink   number of links` |
|      - | 1857 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 1858 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 1859 | ` * 6    rdev    device type, if inode device` |
|      - | 1860 | ` * 7    size    size in bytes` |
|      - | 1861 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 1862 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 1863 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 1864 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 1865 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 1866 | ` * Note:` |
|      - | 1867 | ` *  FALSE is returned on failure.` |
|      - | 1868 | ` */` |
|      6 | 1869 | `static int PH7_vfs_lstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1870 | `{` |
|      - | 1871 | `	ph7_value *pArray,*pValue;` |
|      - | 1872 | `	const char *zPath;` |
|      - | 1873 | `	ph7_vfs *pVfs;` |
|      - | 1874 | `	int rc;` |
|      7 | 1875 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1876 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1877 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1878 | `		return PH7_OK;` |
|      - | 1879 | `	}` |
|      - | 1880 | `	/* Point to the underlying vfs */` |
|      7 | 1881 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 1882 | `	if( pVfs == 0 \|\| pVfs->xlStat == 0 ){` |
|      - | 1883 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1884 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1885 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1886 | `			ph7_function_name(pCtx)` |
|      - | 1887 | `			);` |
|    ! 0 | 1888 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1889 | `		return PH7_OK;` |
|      - | 1890 | `	}` |
|      - | 1891 | `	/* Create the array and the working value */` |
|      7 | 1892 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 1893 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      7 | 1894 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1895 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1896 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1897 | `		return PH7_OK;` |
|      - | 1898 | `	}` |
|      - | 1899 | `	/* Extract the file path */` |
|      7 | 1900 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1901 | `	/* Perform the requested operation */` |
|      7 | 1902 | `	rc = pVfs->xlStat(zPath,pArray,pValue);` |
|      7 | 1903 | `	if( rc != PH7_OK ){` |
|      - | 1904 | ``		/* php warns before answering FALSE -- the same `stat failed for` /`` |
|      - | 1905 | ``		 * `Lstat failed for` line the rest of the family raises. PHL returned the`` |
|      - | 1906 | `		 * FALSE in silence, so a missing path and an empty result looked alike. */` |
|      3 | 1907 | `		VfsThrowStatWarning(pCtx,zPath,1);` |
|      3 | 1908 | `		ph7_result_bool(pCtx,0);` |
|      2 | 1909 | `	}else{` |
|      - | 1910 | `		/* php's answer is the thirteen fields TWICE: numeric 0..12 then named. */` |
|      5 | 1911 | `		ph7_value *pFull = ph7_context_new_array(pCtx);` |
|      5 | 1912 | `		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){` |
|      5 | 1913 | `			ph7_result_value(pCtx,pFull);` |
|      3 | 1914 | `		}else{` |
|    ! 0 | 1915 | `			ph7_result_value(pCtx,pArray);` |
|      - | 1916 | `		}` |
|      - | 1917 | `	}` |
|      - | 1918 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 1919 | `	 * automatically as soon we return from this function. */` |
|      7 | 1920 | `	return PH7_OK;` |
|      4 | 1921 | `}` |
|      - | 1922 | `/*` |
|      - | 1923 | ` * int\|false fileowner / filegroup / fileinode / fileperms (string $filename)` |
|      - | 1924 | ` *  One stat() with one of its fields taken out of it, which is exactly how php` |
|      - | 1925 | ` *  implements them (php_stat's FS_OWNER / FS_GROUP / FS_INODE / FS_PERMS arms).` |
|      - | 1926 | ` *` |
|      - | 1927 | ` * They were prelude PHP wrapping stat(), which cost them php's diagnostic twice` |
|      - | 1928 | ` * over: three of the four said NOTHING on a failed stat (the fourth raised its own` |
|      - | 1929 | `` * `trigger_error`, so its errno was E_USER_WARNING's 512 rather than E_WARNING's 2`` |
|      - | 1930 | ` * and its line was the prelude's, not the caller's). In C the family shares one` |
|      - | 1931 | ` * warning site with the rest of stat(), and the four get real signature rows.` |
|      - | 1932 | ` */` |
|     32 | 1933 | `static int VfsStatField(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zField)` |
|      1 | 1934 | `{` |
|      - | 1935 | `	ph7_value *pArray,*pValue,*pField;` |
|      - | 1936 | `	const char *zPath;` |
|      - | 1937 | `	ph7_vfs *pVfs;` |
|      - | 1938 | `	int rc;` |
|     33 | 1939 | `	if( nArg < 1 ){` |
|    ! 0 | 1940 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1941 | `		return PH7_OK;` |
|      - | 1942 | `	}` |
|     33 | 1943 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     33 | 1944 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|    ! 0 | 1945 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1946 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1947 | `			ph7_function_name(pCtx)` |
|      - | 1948 | `			);` |
|    ! 0 | 1949 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1950 | `		return PH7_OK;` |
|      - | 1951 | `	}` |
|     33 | 1952 | `	pArray = ph7_context_new_array(pCtx);` |
|     33 | 1953 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     33 | 1954 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1955 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1956 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1957 | `		return PH7_OK;` |
|      - | 1958 | `	}` |
|     33 | 1959 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|     33 | 1960 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|     33 | 1961 | `	if( rc != PH7_OK ){` |
|      9 | 1962 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      9 | 1963 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1964 | `		return PH7_OK;` |
|      - | 1965 | `	}` |
|     25 | 1966 | `	pField = ph7_array_fetch(pArray,zField,-1);` |
|     25 | 1967 | `	if( pField == 0 ){` |
|      - | 1968 | `		/* The VFS answered a stat array without this field: nothing to report but` |
|      - | 1969 | `		 * the failure itself, which is what php answers when its own stat has no` |
|      - | 1970 | `		 * such member either. */` |
|    ! 0 | 1971 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1972 | `		return PH7_OK;` |
|      - | 1973 | `	}` |
|     25 | 1974 | `	ph7_result_int64(pCtx,ph7_value_to_int64(pField));` |
|     25 | 1975 | `	return PH7_OK;` |
|     17 | 1976 | `}` |
|      6 | 1977 | `static int PH7_vfs_file_owner(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1978 | `{` |
|      7 | 1979 | `	return VfsStatField(pCtx,nArg,apArg,"uid");` |
|      1 | 1980 | `}` |
|      4 | 1981 | `static int PH7_vfs_file_group(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1982 | `{` |
|      5 | 1983 | `	return VfsStatField(pCtx,nArg,apArg,"gid");` |
|      1 | 1984 | `}` |
|      4 | 1985 | `static int PH7_vfs_file_inode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1986 | `{` |
|      5 | 1987 | `	return VfsStatField(pCtx,nArg,apArg,"ino");` |
|      1 | 1988 | `}` |
|     18 | 1989 | `static int PH7_vfs_file_perms(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1990 | `{` |
|     19 | 1991 | `	return VfsStatField(pCtx,nArg,apArg,"mode");` |
|      1 | 1992 | `}` |
|      - | 1993 | `/*` |
|      - | 1994 | ` * array\|string\|false getenv(?string $name = null, bool $local_only = false)` |
|      - | 1995 | ` *  Gets the value of an environment variable.` |
|      - | 1996 | ` * Parameters` |
|      - | 1997 | ` *  $name` |
|      - | 1998 | ` *   The variable name -- or NOTHING, which is the documented way to ask for the` |
|      - | 1999 | ` *   WHOLE environment as a name => value array. That form answered FALSE here,` |
|      - | 2000 | `` *   so `foreach (getenv() as $k => $v)` iterated over a bool.`` |
|      - | 2001 | ` *  $local_only` |
|      - | 2002 | ` *   Ask only the process's own environment rather than the SAPI's. On the CLI` |
|      - | 2003 | ` *   they are the same environment, so the argument selects the same answer --` |
|      - | 2004 | ` *   but it must still be ACCEPTED, and asking for the whole map with it set` |
|      - | 2005 | ` *   answered false too.` |
|      - | 2006 | ` * Return` |
|      - | 2007 | ` *  The value of the environment variable, or FALSE when it does not exist, or` |
|      - | 2008 | ` *  the whole environment when no name is given.` |
|      - | 2009 | ` */` |
|    116 | 2010 | `static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2011 | `{` |
|      - | 2012 | `	const char *zEnv;` |
|      - | 2013 | `	ph7_vfs *pVfs;` |
|      - | 2014 | `	int iLen;` |
|      - | 2015 | `	/* Point to the underlying vfs */` |
|    120 | 2016 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    120 | 2017 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|      - | 2018 | `		/* The whole environment. xEnviron was APPENDED to ph7_vfs, so an` |
|      - | 2019 | `		 * embedder VFS built against version 2 does not have the field at all --` |
|      - | 2020 | `		 * reading it would run off the end of their struct. */` |
|      8 | 2021 | `		if( pVfs == 0 \|\| pVfs->iVersion < 3 \|\| pVfs->xEnviron == 0` |
|      9 | 2022 | `		 \|\| pVfs->xEnviron(pCtx) != PH7_OK ){` |
|    ! 0 | 2023 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2024 | `		}` |
|      9 | 2025 | `		return PH7_OK;` |
|      - | 2026 | `	}` |
|    112 | 2027 | `	if( pVfs == 0 \|\| pVfs->xGetenv == 0 ){` |
|      - | 2028 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2029 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2030 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2031 | `			ph7_function_name(pCtx)` |
|      - | 2032 | `			);` |
|    ! 0 | 2033 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2034 | `		return PH7_OK;` |
|      - | 2035 | `	}` |
|      - | 2036 | `	/* Extract the environment variable */` |
|    112 | 2037 | `	zEnv = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 2038 | `	/* Set a boolean FALSE as the default return value */` |
|    112 | 2039 | `	ph7_result_bool(pCtx,0);` |
|    112 | 2040 | `	if( iLen < 1 ){` |
|      - | 2041 | `		/* Empty string */` |
|      3 | 2042 | `		return PH7_OK;` |
|      - | 2043 | `	}` |
|      - | 2044 | `	/* Perform the requested operation */` |
|    110 | 2045 | `	pVfs->xGetenv(zEnv,pCtx);` |
|    110 | 2046 | `	return PH7_OK;` |
|     62 | 2047 | `}` |
|      - | 2048 | `/*` |
|      - | 2049 | ` * bool putenv(string $settings)` |
|      - | 2050 | ` *  Set the value of an environment variable.` |
|      - | 2051 | ` * Parameters` |
|      - | 2052 | ` *  $setting` |
|      - | 2053 | ` *   The setting, like "FOO=BAR"` |
|      - | 2054 | ` * Return` |
|      - | 2055 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2056 | ` */` |
|     54 | 2057 | `static int PH7_vfs_putenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2058 | `{` |
|      - | 2059 | `	const char *zName,*zValue;` |
|      - | 2060 | `	char *zSettings,*zEnd;` |
|      - | 2061 | `	ph7_vfs *pVfs;` |
|      - | 2062 | `	int iLen,rc;` |
|     55 | 2063 | `	if( nArg < 1 ){` |
|      - | 2064 | `		/* Missing argument,return FALSE */` |
|    ! 0 | 2065 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2066 | `		return PH7_OK;` |
|      - | 2067 | `	}` |
|      - | 2068 | `	/* Extract the setting variable. It is NOT required to already BE a string:` |
|      - | 2069 | ``	 * the declared parameter is `string $assignment`, so php coerces an int or a`` |
|      - | 2070 | `	 * __toString() object first, where PH7 answered false and did nothing. */` |
|     55 | 2071 | `	zSettings = (char *)ph7_value_to_string(apArg[0],&iLen);` |
|     55 | 2072 | `	if( iLen < 1 \|\| zSettings[0] == '=' ){` |
|      - | 2073 | `		/* php's whole validity rule: an empty assignment, or one with no name in` |
|      - | 2074 | `		 * front of the '='. Everything else is accepted. */` |
|      9 | 2075 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2076 | `			"putenv(): Argument #1 ($assignment) must have a valid syntax");` |
|      - | 2077 | `	}` |
|      - | 2078 | `	/* Parse the setting. php looks for the '=' with strchr(), so an embedded NUL` |
|      - | 2079 | `	 * ENDS the search: putenv("FO\0O=BAR") finds no '=' at all and removes the` |
|      - | 2080 | `	 * variable named "FO" instead of setting one. */` |
|     47 | 2081 | `	zEnd = &zSettings[iLen];` |
|     47 | 2082 | `	zValue = 0;` |
|     47 | 2083 | `	zName = zSettings;` |
|    431 | 2084 | `	while( zSettings < zEnd && zSettings[0] != 0 ){` |
|    407 | 2085 | `		if( zSettings[0] == '=' ){` |
|      - | 2086 | `			/* Null terminate the name */` |
|     23 | 2087 | `			zSettings[0] = 0;` |
|     23 | 2088 | `			zValue = &zSettings[1];` |
|     23 | 2089 | `			break;` |
|      - | 2090 | `		}` |
|    385 | 2091 | `		zSettings++;` |
|      1 | 2092 | `	}` |
|      - | 2093 | ``	/* A missing '=' is not invalid syntax: `putenv("NAME")` REMOVES the variable,`` |
|      - | 2094 | `	 * which is the documented way to unset one, and PH7 read it as a failure and` |
|      - | 2095 | `	 * left the old value in place. An empty VALUE is a value too` |
|      - | 2096 | ``	 * (`putenv("NAME=")`), which the old `zValue >= zEnd` test rejected.`` |
|      - | 2097 | `	 * php does NOT touch $_ENV here: that array is the SAPI's startup snapshot,` |
|      - | 2098 | `	 * and a putenv() after it changes the process environment alone. PH7 wrote` |
|      - | 2099 | `	 * the pair into $_ENV as well, so a script could read back through $_ENV a` |
|      - | 2100 | `	 * variable php only exposes through getenv(). */` |
|      - | 2101 | `	/* Point to the underlying vfs */` |
|     47 | 2102 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     47 | 2103 | `	if( pVfs == 0 \|\| pVfs->xSetenv == 0 ){` |
|      - | 2104 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2105 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2106 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2107 | `			ph7_function_name(pCtx)` |
|      - | 2108 | `			);` |
|    ! 0 | 2109 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2110 | `		if( zValue ){` |
|    ! 0 | 2111 | `			zSettings[0] = '=';` |
|    ! 0 | 2112 | `		}` |
|    ! 0 | 2113 | `		return PH7_OK;` |
|      - | 2114 | `	}` |
|      - | 2115 | `	/* Perform the requested operation. A NULL value means REMOVE, and php reports` |
|      - | 2116 | `	 * TRUE for that whether or not the variable was there (or nameable) at all --` |
|      - | 2117 | `	 * only a failed SET is false. */` |
|     47 | 2118 | `	rc = pVfs->xSetenv(zName,zValue);` |
|     47 | 2119 | `	ph7_result_bool(pCtx,zValue == 0 \|\| rc == PH7_OK );` |
|     47 | 2120 | `	if( zValue ){` |
|      - | 2121 | `		/* Put back the '=' the name was terminated on. Without one, zSettings` |
|      - | 2122 | `		 * stopped on the terminator or on an embedded NUL, neither of which this` |
|      - | 2123 | `		 * routine wrote. */` |
|     23 | 2124 | `		zSettings[0] = '=';` |
|     11 | 2125 | `	}` |
|     47 | 2126 | `	return PH7_OK;` |
|     28 | 2127 | `}` |
|      - | 2128 | `/*` |
|      - | 2129 | ` * bool touch(string $filename[,int64 $time = time()[,int64 $atime]])` |
|      - | 2130 | ` *  Sets access and modification time of file.` |
|      - | 2131 | ` * Note: On windows` |
|      - | 2132 | ` *   If the file does not exists,it will not be created.` |
|      - | 2133 | ` * Parameters` |
|      - | 2134 | ` *  $filename` |
|      - | 2135 | ` *   The name of the file being touched.` |
|      - | 2136 | ` *  $time` |
|      - | 2137 | ` *   The touch time. If time is not supplied, the current system time is used.` |
|      - | 2138 | ` * $atime` |
|      - | 2139 | ` *   If present, the access time of the given filename is set to the value of atime.` |
|      - | 2140 | ` *   Otherwise, it is set to the value passed to the time parameter. If neither are` |
|      - | 2141 | ` *   present, the current system time is used.` |
|      - | 2142 | ` * Return` |
|      - | 2143 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2144 | `*/` |
|     26 | 2145 | `static int PH7_vfs_touch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2146 | `{` |
|      - | 2147 | `	ph7_int64 nTime,nAccess;` |
|      - | 2148 | `	const char *zFile;` |
|      - | 2149 | `	ph7_vfs *pVfs;` |
|      - | 2150 | `	int rc;` |
|     29 | 2151 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2152 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2153 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2154 | `		return PH7_OK;` |
|      - | 2155 | `	}` |
|      - | 2156 | `	/* Point to the underlying vfs */` |
|     29 | 2157 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     29 | 2158 | `	if( pVfs == 0 \|\| pVfs->xTouch == 0 ){` |
|      - | 2159 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2160 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2161 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2162 | `			ph7_function_name(pCtx)` |
|      - | 2163 | `			);` |
|    ! 0 | 2164 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2165 | `		return PH7_OK;` |
|      - | 2166 | `	}` |
|      - | 2167 | `	/* Resolve php's defaults HERE, so the driver only ever sees real timestamps: a` |
|      - | 2168 | ``	 * NEGATIVE stamp is perfectly legal to php (`touch($f, -100)` is 1969), so it`` |
|      - | 2169 | `	 * cannot double as the "not given" sentinel the drivers used to read it as. An` |
|      - | 2170 | `	 * omitted/null $mtime is NOW; an omitted/null $atime follows $mtime. $atime also` |
|      - | 2171 | `	 * used to be read from apArg[1] — the mtime — so touch($f, $m, $a) silently` |
|      - | 2172 | `	 * stamped the modification time onto both. */` |
|     29 | 2173 | `	zFile = ph7_value_to_string(apArg[0],0);` |
|     29 | 2174 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) && nArg > 1 && ph7_value_is_null(apArg[1]) ){` |
|    ! 0 | 2175 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2176 | `			"touch(): Argument #2 ($mtime) cannot be null when argument #3 ($atime) "` |
|      - | 2177 | `			"is an integer");` |
|      - | 2178 | `	}` |
|     29 | 2179 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     13 | 2180 | `		nTime = ph7_value_to_int64(apArg[1]);` |
|      8 | 2181 | `	}else{` |
|      - | 2182 | `		time_t tNow;` |
|     17 | 2183 | `		time(&tNow);` |
|     17 | 2184 | `		nTime = (ph7_int64)tNow;` |
|      - | 2185 | `	}` |
|     29 | 2186 | `	nAccess = nTime;` |
|     29 | 2187 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      3 | 2188 | `		nAccess = ph7_value_to_int64(apArg[2]);` |
|      1 | 2189 | `	}` |
|     29 | 2190 | `	rc = pVfs->xTouch(zFile,nTime,nAccess);` |
|      - | 2191 | `	/* IO result */` |
|     29 | 2192 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     29 | 2193 | `	return PH7_OK;` |
|     16 | 2194 | `}` |
|      - | 2195 | `/*` |
|      - | 2196 | ` * Path processing functions that do not need access to the VFS layer` |
|      - | 2197 | ` * Status:` |
|      - | 2198 | ` *    Stable.` |
|      - | 2199 | ` */` |
|      - | 2200 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2201 | `/*` |
|      - | 2202 | ` * string dirname(string $path)` |
|      - | 2203 |  |
|      - | 2204 | ` *  Returns parent directory's path.` |
|      - | 2205 | ` * Parameters` |
|      - | 2206 | ` * $path` |
|      - | 2207 | ` *  Target path.` |
|      - | 2208 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 2209 | ` *  In other environments, it is the forward slash (/).` |
|      - | 2210 | ` * Return` |
|      - | 2211 | ` *  The path of the parent directory. If there are no slashes in path, a dot ('.')` |
|      - | 2212 | ` *  is returned, indicating the current directory.` |
|      - | 2213 | ` */` |
|     96 | 2214 | `static int PH7_builtin_dirname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2215 | `{` |
|      - | 2216 | `	const char *zPath,*zDir;` |
|      - | 2217 | `	int iLen,iDirlen;` |
|    101 | 2218 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2219 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 2220 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2221 | `		return PH7_OK;` |
|      - | 2222 | `	}` |
|      - | 2223 | `	/* Point to the target path */` |
|    101 | 2224 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|    101 | 2225 | `	if( iLen < 1 ){` |
|      - | 2226 | `		/* php answers "" for the empty path, not "." */` |
|      3 | 2227 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2228 | `		return PH7_OK;` |
|      - | 2229 | `	}` |
|      - | 2230 | `	/* $levels (php 7.0) was ACCEPTED AND IGNORED, so dirname($p, 3) silently answered` |
|      - | 2231 | `	 * the one-level parent — the caller's own answer, one or more levels too deep. Each` |
|      - | 2232 | `	 * level re-runs php_dirname on the previous result and stops as soon as the answer` |
|      - | 2233 | `	 * stops moving (the filesystem root, or "." for a relative path), which is what php` |
|      - | 2234 | `	 * does; php also rejects a level below 1 outright. */` |
|     99 | 2235 | `	zDir = zPath;` |
|     99 | 2236 | `	iDirlen = iLen;` |
|     99 | 2237 | `	if( nArg > 1 ){` |
|     51 | 2238 | `		ph7_int64 nLevels = ph7_value_to_int64(apArg[1]);` |
|      - | 2239 | `		ph7_int64 i;` |
|     51 | 2240 | `		if( nLevels < 1 ){` |
|      7 | 2241 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2242 | `				"dirname(): Argument #2 ($levels) must be greater than or equal to 1");` |
|      - | 2243 | `		}` |
|    125 | 2244 | `		for( i = 0 ; i < nLevels ; ++i ){` |
|    105 | 2245 | `			int iPrevLen = iDirlen;` |
|    105 | 2246 | `			const char *zPrev = zDir;` |
|    105 | 2247 | `			zDir = PH7_ExtractDirName(zPrev,iPrevLen,&iDirlen);` |
|    105 | 2248 | `			if( iDirlen == iPrevLen && SyMemcmp(zDir,zPrev,(sxu32)iDirlen) == 0 ){` |
|     25 | 2249 | `				break; /* fixed point: "/" and "." are their own parents */` |
|      - | 2250 | `			}` |
|     41 | 2251 | `		}` |
|     23 | 2252 | `	}else{` |
|     49 | 2253 | `		zDir = PH7_ExtractDirName(zPath,iLen,&iDirlen);` |
|      - | 2254 | `	}` |
|      - | 2255 | `	/* Return directory name */` |
|     93 | 2256 | `	ph7_result_string(pCtx,zDir,iDirlen);` |
|     93 | 2257 | `	return PH7_OK;` |
|     53 | 2258 | `}` |
|      - | 2259 | `/*` |
|      - | 2260 | ` * string basename(string $path[, string $suffix ])` |
|      - | 2261 | ` *  Returns trailing name component of path.` |
|      - | 2262 | ` * Parameters` |
|      - | 2263 | ` * $path` |
|      - | 2264 | ` *  Target path.` |
|      - | 2265 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 2266 | ` *  In other environments, it is the forward slash (/).` |
|      - | 2267 | ` * $suffix` |
|      - | 2268 | ` *  If the name component ends in suffix this will also be cut off.` |
|      - | 2269 | ` * Return` |
|      - | 2270 | ` *  The base name of the given path.` |
|      - | 2271 | ` */` |
|    166 | 2272 | `static int PH7_builtin_basename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2273 | `{` |
|      - | 2274 | `	const char *zPath,*zBase;` |
|      - | 2275 | `	int iLen,nBase;` |
|    169 | 2276 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2277 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 2278 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2279 | `		return PH7_OK;` |
|      - | 2280 | `	}` |
|      - | 2281 | `	/* Point to the target path */` |
|    169 | 2282 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 2283 | `	/* php_basename, shared with pathinfo(): the hand-rolled walk this used to carry` |
|      - | 2284 | `	 * kept a leading separator on a single-component path (basename("/a") answered` |
|      - | 2285 | `	 * "/a", basename("/.") answered "/.") because it stopped one byte short. */` |
|    169 | 2286 | `	zBase = PH7_ExtractBaseName(zPath,iLen,&nBase);` |
|    169 | 2287 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 2288 | `		const char *zSuffix;` |
|      - | 2289 | `		int nSuffix;` |
|      - | 2290 | `		/* Strip suffix — php leaves the basename alone when it IS the suffix */` |
|      5 | 2291 | `		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);` |
|      4 | 2292 | `		if( nSuffix > 0 && nSuffix < nBase` |
|      5 | 2293 | `		 && SyMemcmp(&zBase[nBase - nSuffix],zSuffix,nSuffix) == 0 ){` |
|      5 | 2294 | `			nBase -= nSuffix;` |
|      2 | 2295 | `		}` |
|      2 | 2296 | `	}` |
|      - | 2297 | `	/* Store the basename */` |
|    169 | 2298 | `	ph7_result_string(pCtx,zBase,nBase);` |
|    169 | 2299 | `	return PH7_OK;` |
|     86 | 2300 | `}` |
|      - | 2301 | `/*` |
|      - | 2302 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 2303 | ` *  Returns information about a file path.` |
|      - | 2304 | ` * Parameter` |
|      - | 2305 | ` *  $path` |
|      - | 2306 | ` *   The path to be parsed.` |
|      - | 2307 | ` *  $options` |
|      - | 2308 | ` *    If present, specifies a specific element to be returned; one of` |
|      - | 2309 | ` *      PATHINFO_DIRNAME, PATHINFO_BASENAME, PATHINFO_EXTENSION or PATHINFO_FILENAME.` |
|      - | 2310 | ` * Return` |
|      - | 2311 | ` *  If the options parameter is not passed, an associative array containing the following` |
|      - | 2312 | ` *  elements is returned: dirname, basename, extension (if any), and filename.` |
|      - | 2313 | ` *  If options is present, returns a string containing the requested element.` |
|      - | 2314 | ` */` |
|      - | 2315 | `typedef struct path_info path_info;` |
|      - | 2316 | `struct path_info` |
|      - | 2317 | `{` |
|      - | 2318 | `	SyString sDir; /* Directory [i.e: /var/www] */` |
|      - | 2319 | `	SyString sBasename; /* Basename [i.e httpd.conf] */` |
|      - | 2320 | `	SyString sExtension; /* File extension [i.e xml,pdf..] */` |
|      - | 2321 | `	SyString sFilename;  /* Filename */` |
|      - | 2322 | `	int iPresent;        /* Which components php would EMIT (PH7_PATHINFO_* bits) */` |
|      - | 2323 | `};` |
|      - | 2324 | `/*` |
|      - | 2325 | ` * Extract path fields exactly as php's pathinfo() assembles them.` |
|      - | 2326 | ` *` |
|      - | 2327 | ` * Two things this has to get right beyond the values themselves:` |
|      - | 2328 | ` *` |
|      - | 2329 | ` *  - php looks for the LAST dot ANYWHERE in the basename, a leading one included,` |
|      - | 2330 | `` *    so `.bashrc` has extension "bashrc" and filename "" (PH7 stopped the scan`` |
|      - | 2331 | ` *    before the first byte, so it reported no extension and filename ".bashrc").` |
|      - | 2332 | ` *  - EMPTY is not the same as ABSENT. php always emits basename and filename when` |
|      - | 2333 | `` *    they are asked for, emits extension whenever a dot exists (even for `x.`,`` |
|      - | 2334 | ` *    whose extension is ""), and emits dirname only when it is non-empty. The` |
|      - | 2335 | ` *    scalar form answers with the first EMITTED component, so conflating the two` |
|      - | 2336 | `` *    makes `pathinfo("x.", PATHINFO_EXTENSION\|PATHINFO_FILENAME)` fall through to`` |
|      - | 2337 | ` *    the filename ("x") where php answers "" — iPresent keeps them apart.` |
|      - | 2338 | ` *` |
|      - | 2339 | ` * dirname and basename come from the shared php_dirname/php_basename helpers` |
|      - | 2340 | ` * rather than a third hand-rolled walk, so the trailing-separator and` |
|      - | 2341 | ` * relative-path rules ("file.txt" -> ".", "/var/www/" -> "/var" + "www") cannot` |
|      - | 2342 | ` * drift between the two builtins and this one.` |
|      - | 2343 | ` */` |
|  17530 | 2344 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 2345 | `{` |
|      - | 2346 | `	const char *zBase,*zDir,*zDot;` |
|      - | 2347 | `	int nBase,nDir,i;` |
|      - | 2348 | `	/* Zero the structure */` |
|  17535 | 2349 | `	SyZero(pOut,sizeof(path_info));` |
|  17535 | 2350 | `	zDir = PH7_ExtractDirName(zPath,nByte,&nDir);` |
|  17535 | 2351 | `	if( nDir > 0 ){` |
|  17531 | 2352 | `		SyStringInitFromBuf(&pOut->sDir,zDir,nDir);` |
|  17531 | 2353 | `		pOut->iPresent \|= PH7_PATHINFO_DIRNAME;` |
|   8763 | 2354 | `	}` |
|  17535 | 2355 | `	zBase = PH7_ExtractBaseName(zPath,nByte,&nBase);` |
|  17535 | 2356 | `	SyStringInitFromBuf(&pOut->sBasename,zBase,nBase);` |
|  17535 | 2357 | `	pOut->iPresent \|= PH7_PATHINFO_BASENAME\|PH7_PATHINFO_FILENAME;` |
|      - | 2358 | `	/* Last dot anywhere in the basename splits filename from extension */` |
|  17535 | 2359 | `	zDot = 0;` |
|  87591 | 2360 | `	for( i = nBase ; i > 0 ; --i ){` |
|  87569 | 2361 | `		if( zBase[i - 1] == '.' ){` |
|  17513 | 2362 | `			zDot = &zBase[i - 1];` |
|  17513 | 2363 | `			break;` |
|      - | 2364 | `		}` |
|  35033 | 2365 | `	}` |
|  17535 | 2366 | `	if( zDot ){` |
|  17513 | 2367 | `		SyStringInitFromBuf(&pOut->sExtension,zDot + 1,(int)(&zBase[nBase] - (zDot + 1)));` |
|  17513 | 2368 | `		pOut->iPresent \|= PH7_PATHINFO_EXTENSION;` |
|  17513 | 2369 | `		SyStringInitFromBuf(&pOut->sFilename,zBase,(int)(zDot - zBase));` |
|   8759 | 2370 | `	}else{` |
|     23 | 2371 | `		SyStringInitFromBuf(&pOut->sFilename,zBase,nBase);` |
|      - | 2372 | `	}` |
|  17535 | 2373 | `	return SXRET_OK;` |
|      5 | 2374 | `}` |
|      - | 2375 | `/*` |
|      - | 2376 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 2377 | ` *  See block comment above.` |
|      - | 2378 | ` */` |
|  17530 | 2379 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2380 | `{` |
|      - | 2381 | `	const char *zPath;` |
|      - | 2382 | `	path_info sInfo;` |
|      - | 2383 | `	int iLen;` |
|  17535 | 2384 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2385 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 2386 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2387 | `		return PH7_OK;` |
|      - | 2388 | `	}` |
|      - | 2389 | `	/* Point to the target path. The EMPTY path is not a special case: php still` |
|      - | 2390 | ``	 * answers with the array `["basename" => "", "filename" => ""]` (and "" for a`` |
|      - | 2391 | `	 * scalar request), where PH7 short-circuited to "" and returned the wrong TYPE. */` |
|  17535 | 2392 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 2393 | `	/* Extract path info */` |
|  17535 | 2394 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|      - | 2395 | ``	/* Read the mask at 64-bit width: ph7_value_to_int() truncates to `int`, so a`` |
|      - | 2396 | `	 * flags value congruent to PATHINFO_ALL mod 2^32 (4294967311, -4294967281 …)` |
|      - | 2397 | `	 * would take the ARRAY branch and answer with the wrong TYPE. */` |
|  17530 | 2398 | `	if( nArg > 1 && ph7_value_is_int(apArg[1])` |
|  26277 | 2399 | `	 && ph7_value_to_int64(apArg[1]) != (ph7_int64)PH7_PATHINFO_ALL ){` |
|      - | 2400 | `		/* $flags is a BITMASK, not an enum: php assembles the requested components in` |
|      - | 2401 | `		 * the fixed order below and, for anything other than PATHINFO_ALL, hands back` |
|      - | 2402 | `		 * the FIRST one it EMITTED (zend_hash_get_current_data on the fresh array).` |
|      - | 2403 | ``		 * So `PATHINFO_DIRNAME\|PATHINFO_BASENAME` answers the dirname, and an unknown`` |
|      - | 2404 | `		 * bit that happens to carry a known one along (99 = 1\|2\|32\|64) answers as if` |
|      - | 2405 | `		 * only the known ones were passed. PH7 numbered the components 1/2/3/4 and` |
|      - | 2406 | `		 * switched on the whole value, so it read a two-flag mask as a different single` |
|      - | 2407 | `		 * component and answered "" for everything else. Emission is iPresent, NOT` |
|      - | 2408 | `		 * "non-empty": an emitted-but-empty component ends the search with "". */` |
|  17517 | 2409 | `		ph7_int64 nComp = ph7_value_to_int64(apArg[1]);` |
|      - | 2410 | `		static const int aBit[4] = {` |
|      - | 2411 | `			PH7_PATHINFO_DIRNAME,PH7_PATHINFO_BASENAME,` |
|      - | 2412 | `			PH7_PATHINFO_EXTENSION,PH7_PATHINFO_FILENAME` |
|      - | 2413 | `		};` |
|      - | 2414 | `		SyString *apComp[4];` |
|      - | 2415 | `		int i;` |
|  17517 | 2416 | `		apComp[0] = &sInfo.sDir;` |
|  17517 | 2417 | `		apComp[1] = &sInfo.sBasename;` |
|  17517 | 2418 | `		apComp[2] = &sInfo.sExtension;` |
|  17517 | 2419 | `		apComp[3] = &sInfo.sFilename;` |
|      - | 2420 | `		/* Expand the empty string unless a requested component is emitted */` |
|  17517 | 2421 | `		ph7_result_string(pCtx,"",0);` |
|  61241 | 2422 | `		for( i = 0 ; i < 4 ; ++i ){` |
|  61233 | 2423 | `			if( (nComp & aBit[i]) == aBit[i] && (sInfo.iPresent & aBit[i]) ){` |
|  17509 | 2424 | `				ph7_result_string(pCtx,apComp[i]->zString,(int)apComp[i]->nByte);` |
|  17509 | 2425 | `				break;` |
|      - | 2426 | `			}` |
|  21867 | 2427 | `		}` |
|   8761 | 2428 | `	}else{` |
|      - | 2429 | `		/* Return an associative array */` |
|      - | 2430 | `		ph7_value *pArray,*pValue;` |
|     19 | 2431 | `		pArray = ph7_context_new_array(pCtx);` |
|     19 | 2432 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     19 | 2433 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2434 | `			/* Out of mem,return NULL */` |
|    ! 0 | 2435 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2436 | `			return PH7_OK;` |
|      - | 2437 | `		}` |
|      - | 2438 | ``		/* Emitted-but-EMPTY components are in the array too (php keys `basename` and`` |
|      - | 2439 | ``		 * `filename` for "/" with ""), so this walks iPresent, not the lengths. */`` |
|      - | 2440 | `		{` |
|      - | 2441 | `		static const char *azKey[4] = {"dirname","basename","extension","filename"};` |
|      - | 2442 | `		static const int aBit[4] = {` |
|      - | 2443 | `			PH7_PATHINFO_DIRNAME,PH7_PATHINFO_BASENAME,` |
|      - | 2444 | `			PH7_PATHINFO_EXTENSION,PH7_PATHINFO_FILENAME` |
|      - | 2445 | `		};` |
|      - | 2446 | `		SyString *apComp[4];` |
|      - | 2447 | `		int i;` |
|     19 | 2448 | `		apComp[0] = &sInfo.sDir;` |
|     19 | 2449 | `		apComp[1] = &sInfo.sBasename;` |
|     19 | 2450 | `		apComp[2] = &sInfo.sExtension;` |
|     19 | 2451 | `		apComp[3] = &sInfo.sFilename;` |
|     91 | 2452 | `		for( i = 0 ; i < 4 ; ++i ){` |
|     73 | 2453 | `			if( (sInfo.iPresent & aBit[i]) == 0 ){` |
|     11 | 2454 | `				continue;` |
|      - | 2455 | `			}` |
|     63 | 2456 | `			ph7_value_reset_string_cursor(pValue);` |
|     63 | 2457 | `			ph7_value_string(pValue,apComp[i]->zString,(int)apComp[i]->nByte);` |
|     63 | 2458 | `			ph7_array_add_strkey_elem(pArray,azKey[i],pValue); /* Will make it's own copy */` |
|     32 | 2459 | `		}` |
|      - | 2460 | `		}` |
|      - | 2461 | `		/* Return the created array */` |
|     19 | 2462 | `		ph7_result_value(pCtx,pArray);` |
|      - | 2463 | `		/* Don't worry about freeing memory, everything will be released` |
|      - | 2464 | `		 * automatically as soon we return from this foreign function.` |
|      - | 2465 | `		 */` |
|      - | 2466 | `	}` |
|  17535 | 2467 | `	return PH7_OK;` |
|   8770 | 2468 | `}` |
|      - | 2469 | `/* SPDX-SnippetBegin */` |
|      - | 2470 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 2471 | `/* SPDX-License-Identifier: blessing */` |
|      - | 2472 | `/*` |
|      - | 2473 | ` * Globbing implementation extracted from the sqlite3 source tree.` |
|      - | 2474 |  |
|      - | 2475 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|      - | 2476 | ` * Status: Public Domain` |
|      - | 2477 | ` */` |
|      - | 2478 | `typedef unsigned char u8;` |
|      - | 2479 | `/* An array to map all upper-case characters into their corresponding` |
|      - | 2480 | `** lower-case character.` |
|      - | 2481 | `**` |
|      - | 2482 | `** SQLite only considers US-ASCII (or EBCDIC) characters.  We do not` |
|      - | 2483 | `** handle case conversions for the UTF character set since the tables` |
|      - | 2484 | `** involved are nearly as big or bigger than SQLite itself.` |
|      - | 2485 | `*/` |
|      - | 2486 | `static const unsigned char sqlite3UpperToLower[] = {` |
|      - | 2487 | `      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,` |
|      - | 2488 | `     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,` |
|      - | 2489 | `     36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,` |
|      - | 2490 | `     54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,` |
|      - | 2491 | `    104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,` |
|      - | 2492 | `    122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,` |
|      - | 2493 | `    108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,` |
|      - | 2494 | `    126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,` |
|      - | 2495 | `    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,` |
|      - | 2496 | `    162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,` |
|      - | 2497 | `    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,` |
|      - | 2498 | `    198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,` |
|      - | 2499 | `    216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,` |
|      - | 2500 | `    234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,` |
|      - | 2501 | `    252,253,254,255` |
|      - | 2502 | `};` |
|      - | 2503 | `#define GlogUpperToLower(A)     if( A<0x80 ){ A = sqlite3UpperToLower[A]; }` |
|      - | 2504 | `/*` |
|      - | 2505 | `** Assuming zIn points to the first byte of a UTF-8 character,` |
|      - | 2506 | `** advance zIn to point to the first byte of the next UTF-8 character.` |
|      - | 2507 | `*/` |
|      - | 2508 | `#define SQLITE_SKIP_UTF8(zIn) {                        \` |
|      - | 2509 | `  if( (*(zIn++))>=0xc0 ){                              \` |
|      - | 2510 | `    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \` |
|      - | 2511 | `  }                                                    \` |
|      - | 2512 | `}` |
|      - | 2513 | `/*` |
|      - | 2514 | `** Compare two UTF-8 strings for equality where the first string can` |
|      - | 2515 | `** potentially be a "glob" expression.  Return true (1) if they` |
|      - | 2516 | `** are the same and false (0) if they are different.` |
|      - | 2517 | `**` |
|      - | 2518 | `** Globbing rules:` |
|      - | 2519 | `**` |
|      - | 2520 | `**      '*'       Matches any sequence of zero or more characters.` |
|      - | 2521 | `**` |
|      - | 2522 | `**      '?'       Matches exactly one character.` |
|      - | 2523 | `**` |
|      - | 2524 | `**     [...]      Matches one character from the enclosed list of` |
|      - | 2525 | `**                characters.` |
|      - | 2526 | `**` |
|      - | 2527 | `**     [^...]     Matches one character not in the enclosed list.` |
|      - | 2528 | `**` |
|      - | 2529 | `** With the [...] and [^...] matching, a ']' character can be included` |
|      - | 2530 | `** in the list by making it the first character after '[' or '^'.  A` |
|      - | 2531 | `** range of characters can be specified using '-'.  Example:` |
|      - | 2532 | `** "[a-z]" matches any single lower-case letter.  To match a '-', make` |
|      - | 2533 | `** it the last character in the list.` |
|      - | 2534 | `**` |
|      - | 2535 | `** This routine is usually quick, but can be N**2 in the worst case.` |
|      - | 2536 | `**` |
|      - | 2537 | `** Hints: to match '*' or '?', put them in "[]".  Like this:` |
|      - | 2538 | `**` |
|      - | 2539 | `**         abc[*]xyz        Matches "abc*xyz" only` |
|      - | 2540 | `*/` |
|      - | 2541 | `/*` |
|      - | 2542 | `` * One POSIX character class of a `[...]` set, as glibc's matcher answers it.`` |
|      - | 2543 | ` * The classes are ASCII-only in the C locale php runs its fnmatch()/glob() in,` |
|      - | 2544 | ` * so a code point past 127 belongs to none of them.` |
|      - | 2545 | ` */` |
|     92 | 2546 | `static int PatternPosixClass(const unsigned char *zName,int nName,int c)` |
|      1 | 2547 | `{` |
|      - | 2548 | `	static const struct { const char *zName; int nName; } aClass[] = {` |
|      - | 2549 | `		{ "alnum", 5 }, { "alpha", 5 }, { "blank", 5 }, { "cntrl", 5 },` |
|      - | 2550 | `		{ "digit", 5 }, { "graph", 5 }, { "lower", 5 }, { "print", 5 },` |
|      - | 2551 | `		{ "punct", 5 }, { "space", 5 }, { "upper", 5 }, { "xdigit", 6 },` |
|      - | 2552 | `	};` |
|     93 | 2553 | `	int i,iWhich = -1;` |
|    545 | 2554 | `	for( i = 0 ; i < (int)(sizeof(aClass)/sizeof(aClass[0])) ; ++i ){` |
|    540 | 2555 | `		if( aClass[i].nName == nName` |
|    495 | 2556 | `		 && SyMemcmp(aClass[i].zName,(const char *)zName,(sxu32)nName) == 0 ){` |
|     89 | 2557 | `			iWhich = i;` |
|     89 | 2558 | `			break;` |
|      - | 2559 | `		}` |
|    227 | 2560 | `	}` |
|     93 | 2561 | `	if( iWhich < 0 \|\| c < 0 \|\| c > 127 ){` |
|      - | 2562 | `		/* An unknown class name matches nothing, which is what a matcher that` |
|      - | 2563 | `		 * cannot name the set can honestly say. */` |
|      5 | 2564 | `		return 0;` |
|      - | 2565 | `	}` |
|     89 | 2566 | `	switch( iWhich ){` |
|    ! 0 | 2567 | `		case 0: return SyisAlphaNum(c);` |
|     21 | 2568 | `		case 1: return SyisAlpha(c);` |
|    ! 0 | 2569 | `		case 2: return c == ' ' \|\| c == '\t';` |
|    ! 0 | 2570 | `		case 3: return c < 0x20 \|\| c == 0x7F;` |
|     49 | 2571 | `		case 4: return SyisDigit(c);` |
|    ! 0 | 2572 | `		case 5: return c > 0x20 && c < 0x7F;` |
|    ! 0 | 2573 | `		case 6: return SyisLower(c);` |
|    ! 0 | 2574 | `		case 7: return c >= 0x20 && c < 0x7F;` |
|      5 | 2575 | `		case 8: return c > 0x20 && c < 0x7F && !SyisAlphaNum(c);` |
|      5 | 2576 | `		case 9: return SyisSpace(c);` |
|      9 | 2577 | `		case 10: return SyisUpper(c);` |
|      5 | 2578 | `		default: return SyisHex(c);` |
|      - | 2579 | `	}` |
|     47 | 2580 | `}` |
|   2410 | 2581 | `static int patternCompare(` |
|      - | 2582 | `  const u8 *zPattern,              /* The glob pattern */` |
|      - | 2583 | `  const u8 *zString,               /* The string to compare against the glob */` |
|      - | 2584 | `  const int esc,                    /* The escape character */` |
|      - | 2585 | `  int noCase,` |
|      - | 2586 | ``  int bCaret                        /* `[^...]` inverts (fnmatch) or is a literal `^` (glob) */`` |
|      3 | 2587 | `){` |
|      - | 2588 | `  int c, c2, cLow;` |
|      - | 2589 | `  int invert;` |
|      - | 2590 | `  int seen;` |
|   2413 | 2591 | `  u8 matchOne = '?';` |
|   2413 | 2592 | `  u8 matchAll = '*';` |
|   2413 | 2593 | `  u8 matchSet = '[';` |
|   2413 | 2594 | `  int prevEscape = 0;     /* True if the previous character was 'escape' */` |
|      - | 2595 |  |
|   2413 | 2596 | `  if( !zPattern \|\| !zString ) return 0;` |
|   3442 | 2597 | `  while( (c = PH7_Utf8Read(zPattern,0,&zPattern))!=0 ){` |
|   3072 | 2598 | `    if( !prevEscape && c==matchAll ){` |
|   2344 | 2599 | `      while( (c=PH7_Utf8Read(zPattern,0,&zPattern)) == matchAll` |
|   1175 | 2600 | `               \|\| c == matchOne ){` |
|    ! 0 | 2601 | `        if( c==matchOne && PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 2602 | `          return 0;` |
|      - | 2603 | `        }` |
|    ! 0 | 2604 | `      }` |
|   1175 | 2605 | `      if( c==0 ){` |
|    663 | 2606 | `        return 1;` |
|    513 | 2607 | `      }else if( c==esc ){` |
|    ! 0 | 2608 | `        c = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 2609 | `        if( c==0 ){` |
|    ! 0 | 2610 | `          return 0;` |
|    ! 0 | 2611 | `        }` |
|    513 | 2612 | `      }else if( c==matchSet ){` |
|      - | 2613 | ``        /* A `[...]` set right after a `*`: try it at every remaining position.`` |
|      - | 2614 | `         * The two asserts SQLite has here became guards, and one of them --` |
|      - | 2615 | `         * "'[' is a single-byte character" -- is ALWAYS true, so this branch` |
|      - | 2616 | ``         * returned 0 for every pattern of the shape `*[...]`. `*[ab]`,`` |
|      - | 2617 | ``         * `a*[0-9]` and `*[[:digit:]]` matched NOTHING, in fnmatch(), in`` |
|      - | 2618 | `         * glob() and in strglob() alike. */` |
|    270 | 2619 | `        while( *zString && patternCompare(&zPattern[-1],zString,esc,noCase,bCaret)==0 ){` |
|    147 | 2620 | `          SQLITE_SKIP_UTF8(zString);` |
|      1 | 2621 | `        }` |
|     83 | 2622 | `        return *zString!=0;` |
|      - | 2623 | `      }` |
|    507 | 2624 | `      while( (c2 = PH7_Utf8Read(zString,0,&zString))!=0 ){` |
|    507 | 2625 | `        if( noCase ){` |
|      3 | 2626 | `          GlogUpperToLower(c2);` |
|      3 | 2627 | `          GlogUpperToLower(c);` |
|     11 | 2628 | `          while( c2 != 0 && c2 != c ){` |
|      9 | 2629 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      9 | 2630 | `            GlogUpperToLower(c2);` |
|      1 | 2631 | `          }` |
|      2 | 2632 | `        }else{` |
|   1403 | 2633 | `          while( c2 != 0 && c2 != c ){` |
|    899 | 2634 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      1 | 2635 | `          }` |
|      - | 2636 | `        }` |
|    507 | 2637 | `        if( c2==0 ) return 0;` |
|    243 | 2638 | `		if( patternCompare(zPattern,zString,esc,noCase,bCaret) ) return 1;` |
|      1 | 2639 | `      }` |
|    ! 0 | 2640 | `      return 0;` |
|   1899 | 2641 | `    }else if( !prevEscape && c==matchOne ){` |
|     25 | 2642 | `      if( PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 2643 | `        return 0;` |
|      1 | 2644 | `      }` |
|   1887 | 2645 | `    }else if( c==matchSet ){` |
|    459 | 2646 | `      int prior_c = 0;` |
|      - | 2647 | `      /* SQLite asserts here that its GLOB has no escape character; the guard` |
|      - | 2648 | `       * that replaced the assert reads the condition BACKWARDS, so a set` |
|      - | 2649 | ``       * matched nothing whenever escaping was turned off -- every `[...]` in`` |
|      - | 2650 | ``       * an `fnmatch($p,$s,FNM_NOESCAPE)` call answered false. */`` |
|    459 | 2651 | `      seen = 0;` |
|    459 | 2652 | `      invert = 0;` |
|    459 | 2653 | `      c = PH7_Utf8Read(zString, 0, &zString);` |
|    459 | 2654 | `      if( c==0 ) return 0;` |
|      - | 2655 | `      /* A case-INSENSITIVE match folds inside the set too: this branch ignored` |
|      - | 2656 | ``       * noCase entirely, so `fnmatch('[a-c]','B',FNM_CASEFOLD)` was false and`` |
|      - | 2657 | ``       * its negation `[!a-c]` was true -- both the opposite of php's. The`` |
|      - | 2658 | `       * folded subject is what MEMBERS and RANGES are compared against; a` |
|      - | 2659 | ``        * character CLASS is not folded at all (glibc tests `[[:upper:]]` `` |
|      - | 2660 | `       * against the character as written, FNM_CASEFOLD or not). */` |
|    459 | 2661 | `      cLow = c;` |
|    459 | 2662 | `      if( noCase ){` |
|     49 | 2663 | `        GlogUpperToLower(cLow);` |
|     24 | 2664 | `      }` |
|    459 | 2665 | `      c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|      - | 2666 | ``      /* POSIX spells the negation `!` and glibc accepts `^` as well; php's`` |
|      - | 2667 | ``       * fnmatch()/glob() are glibc's, so BOTH invert. Only `^` did here, which`` |
|      - | 2668 | ``       * made `[!a]` a set holding `!` and `a` -- the exact INVERSE answer for`` |
|      - | 2669 | `       * the spelling a shell uses. */` |
|    459 | 2670 | `      if( c2=='!' \|\| (bCaret && c2=='^') ){` |
|    117 | 2671 | `        invert = 1;` |
|    117 | 2672 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|     58 | 2673 | `      }` |
|    459 | 2674 | `      if( c2==']' ){` |
|     19 | 2675 | `        if( c==']' ) seen = 1;` |
|     19 | 2676 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|      9 | 2677 | `      }` |
|   1171 | 2678 | `      while( c2 && c2!=']' ){` |
|    713 | 2679 | `        int cFold = c2;` |
|    713 | 2680 | `        if( noCase ){` |
|     71 | 2681 | `          GlogUpperToLower(cFold);` |
|     35 | 2682 | `        }` |
|    713 | 2683 | `        if( c2=='[' && zPattern[0]==':' ){` |
|      - | 2684 | ``          /* A POSIX character CLASS, `[:alpha:]`, which glibc's matcher knows`` |
|      - | 2685 | ``           * and this one did not -- the whole `[[:digit:]]` bracket read as the`` |
|      - | 2686 | ``           * literal set `[:digt` and matched the wrong characters in silence. */`` |
|     93 | 2687 | `          const unsigned char *zName = &zPattern[1];` |
|     93 | 2688 | `          const unsigned char *zEnd = zName;` |
|    553 | 2689 | `          while( zEnd[0] != 0 && !(zEnd[0]==':' && zEnd[1]==']') ){` |
|    461 | 2690 | `            zEnd++;` |
|      1 | 2691 | `          }` |
|     93 | 2692 | `          if( zEnd[0] != 0 ){` |
|     93 | 2693 | `            if( PatternPosixClass(zName,(int)(zEnd - zName),c) ){` |
|     47 | 2694 | `              seen = 1;` |
|     23 | 2695 | `            }` |
|     93 | 2696 | `            zPattern = zEnd + 2;` |
|     93 | 2697 | `            prior_c = 0;` |
|     93 | 2698 | `            c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|     93 | 2699 | `            continue;` |
|      - | 2700 | `          }` |
|    ! 0 | 2701 | `        }` |
|    621 | 2702 | `        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){` |
|    187 | 2703 | `          c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    187 | 2704 | `          cFold = c2;` |
|    187 | 2705 | `          if( noCase ){` |
|     15 | 2706 | `            GlogUpperToLower(cFold);` |
|      7 | 2707 | `          }` |
|    187 | 2708 | `          if( cLow>=prior_c && cLow<=cFold ) seen = 1;` |
|    187 | 2709 | `          prior_c = 0;` |
|     94 | 2710 | `        }else{` |
|    435 | 2711 | `          if( cLow==cFold ){` |
|     61 | 2712 | `            seen = 1;` |
|     30 | 2713 | `          }` |
|    435 | 2714 | `          prior_c = cFold;` |
|      - | 2715 | `        }` |
|    621 | 2716 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|      1 | 2717 | `      }` |
|    459 | 2718 | `      if( c2==0 \|\| (seen ^ invert)==0 ){` |
|    203 | 2719 | `        return 0;` |
|      1 | 2720 | `      }` |
|   1545 | 2721 | `    }else if( esc==c && !prevEscape ){` |
|      9 | 2722 | `      prevEscape = 1;` |
|      5 | 2723 | `    }else{` |
|   1409 | 2724 | `      c2 = PH7_Utf8Read(zString, 0, &zString);` |
|   1409 | 2725 | `      if( noCase ){` |
|     17 | 2726 | `        GlogUpperToLower(c);` |
|     17 | 2727 | `        GlogUpperToLower(c2);` |
|      8 | 2728 | `      }` |
|   1409 | 2729 | `      if( c!=c2 ){` |
|    667 | 2730 | `        return 0;` |
|      - | 2731 | `      }` |
|    743 | 2732 | `      prevEscape = 0;` |
|      - | 2733 | `    }` |
|      2 | 2734 | `  }` |
|    371 | 2735 | `  return *zString==0;` |
|   1264 | 2736 | `}` |
|      - | 2737 | `/* SPDX-SnippetEnd */` |
|      - | 2738 | `/*` |
|      - | 2739 | ` * Wrapper around patternCompare() defined above.` |
|      - | 2740 | ` * See block comment above for more information.` |
|      - | 2741 | ` */` |
|   1980 | 2742 | `static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,` |
|      - | 2743 | `	int CaseCompare,int bCaret)` |
|      3 | 2744 | `{` |
|      - | 2745 | `	int rc;` |
|   1983 | 2746 | `	if( iEsc < 0 ){` |
|    ! 0 | 2747 | `		iEsc = '\\';` |
|    ! 0 | 2748 | `	}` |
|   1983 | 2749 | `	rc = patternCompare(zPattern,zString,iEsc,CaseCompare,bCaret);` |
|   1983 | 2750 | `	return rc;` |
|      3 | 2751 | `}` |
|      - | 2752 | `/*` |
|      - | 2753 | ` * bool fnmatch(string $pattern,string $string[,int $flags = 0 ])` |
|      - | 2754 | ` *  Match filename against a pattern.` |
|      - | 2755 | ` * Parameters` |
|      - | 2756 | ` *  $pattern` |
|      - | 2757 | ` *   The shell wildcard pattern.` |
|      - | 2758 | ` * $string` |
|      - | 2759 | ` *  The tested string.` |
|      - | 2760 | ` * $flags` |
|      - | 2761 | ` *   A list of possible flags:` |
|      - | 2762 | ` *    FNM_NOESCAPE 	Disable backslash escaping.` |
|      - | 2763 | ` *    FNM_PATHNAME 	Slash in string only matches slash in the given pattern.` |
|      - | 2764 | ` *    FNM_PERIOD 	Leading period in string must be exactly matched by period in the given pattern.` |
|      - | 2765 | ` *    FNM_CASEFOLD 	Caseless match.` |
|      - | 2766 | ` * Return` |
|      - | 2767 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 2768 | ` */` |
|    158 | 2769 | `static int PH7_builtin_fnmatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2770 | `{` |
|      - | 2771 | `	const char *zString,*zPattern;` |
|    159 | 2772 | `	int iEsc = '\\';` |
|    159 | 2773 | `	int noCase = 0;` |
|      - | 2774 | `	int rc;` |
|    159 | 2775 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2776 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2777 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2778 | `		return PH7_OK;` |
|      - | 2779 | `	}` |
|      - | 2780 | `	/* Extract the pattern and the string */` |
|    159 | 2781 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|    159 | 2782 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2783 | `	/* Extract the flags if avaialble */` |
|    159 | 2784 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|     99 | 2785 | `		rc = ph7_value_to_int(apArg[2]);` |
|     99 | 2786 | `		if( rc & 2 /*FNM_NOESCAPE (php value)*/){` |
|     51 | 2787 | `			iEsc = 0;` |
|     25 | 2788 | `		}` |
|     99 | 2789 | `		if( rc & 16 /*FNM_CASEFOLD (php value)*/){` |
|     45 | 2790 | `			noCase = 1;` |
|     22 | 2791 | `		}` |
|     49 | 2792 | `	}` |
|      - | 2793 | ``	/* Go globbing. fnmatch() is glibc's, whose matcher takes `^` as a second`` |
|      - | 2794 | `	 * spelling of the negation -- glob(3)'s does NOT, and strglob() below` |
|      - | 2795 | `	 * carries glob()'s rule because that is what the prelude glob() drives. */` |
|    159 | 2796 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,noCase,TRUE);` |
|      - | 2797 | `	/* Globbing result */` |
|    159 | 2798 | `	ph7_result_bool(pCtx,rc);` |
|    159 | 2799 | `	return PH7_OK;` |
|     80 | 2800 | `}` |
|      - | 2801 | `/*` |
|      - | 2802 | ` * bool strglob(string $pattern,string $string)` |
|      - | 2803 | ` *  Match string against a pattern.` |
|      - | 2804 | ` * Parameters` |
|      - | 2805 | ` *  $pattern` |
|      - | 2806 | ` *   The shell wildcard pattern.` |
|      - | 2807 | ` * $string` |
|      - | 2808 | ` *  The tested string.` |
|      - | 2809 | ` * Return` |
|      - | 2810 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 2811 | ` * Note that this a symisc eXtension.` |
|      - | 2812 | ` */` |
|   1228 | 2813 | `static int PH7_builtin_strglob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2814 | `{` |
|      - | 2815 | `	const char *zString,*zPattern;` |
|   1231 | 2816 | `	int iEsc = '\\';` |
|      - | 2817 | `	int rc;` |
|   1231 | 2818 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 2819 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2820 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2821 | `		return PH7_OK;` |
|      - | 2822 | `	}` |
|      - | 2823 | `	/* Extract the pattern and the string */` |
|   1231 | 2824 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|   1231 | 2825 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 2826 | ``	/* Go globbing, with glob(3)'s set rules: only `!` inverts, and a `^` right`` |
|      - | 2827 | ``	 * after the `[` is an ordinary member of the set. */`` |
|   1231 | 2828 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0,FALSE);` |
|      - | 2829 | `	/* Globbing result */` |
|   1231 | 2830 | `	ph7_result_bool(pCtx,rc);` |
|   1231 | 2831 | `	return PH7_OK;` |
|    617 | 2832 | `}` |
|      - | 2833 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 2834 | `/* Every buffer below is one path, and php's own limit for one is PATH_MAX; the` |
|      - | 2835 | ` * SPL directory opener already refuses a longer one, and a pattern past it can` |
|      - | 2836 | ` * name nothing that exists. It also bounds the recursion: each level of the` |
|      - | 2837 | ` * walk consumes at least one slash of the pattern. */` |
|      - | 2838 | `#define PH7_GLOB_PATH_MAX 4096` |
|      - | 2839 | `/*` |
|      - | 2840 | ` * ---------------------------------------------------------------------------` |
|      - | 2841 | ` * The glob:// stream device.` |
|      - | 2842 | ` *` |
|      - | 2843 | ` * php's glob wrapper is a DIRECTORY whose entries are a pattern's matches:` |
|      - | 2844 | `` * `opendir('glob://src/' . '*.php')` hands out one BASENAME per match, and`` |
|      - | 2845 | ` * GlobIterator is that stream behind the whole DirectoryIterator machinery. It` |
|      - | 2846 | ` * is a dir_opener and NOTHING else -- php gives it no stream opener (so` |
|      - | 2847 | `` * `fopen('glob://…')` is "wrapper does not support stream open") and no`` |
|      - | 2848 | `` * url_stat (so `file_exists()` and `is_dir()` answer false for one).`` |
|      - | 2849 | ` *` |
|      - | 2850 | ` * The expansion is glob(3) with NO flags, which is what php's opener asks for,` |
|      - | 2851 | ` * so it has to agree name for name AND order for order with the prelude` |
|      - | 2852 | ` * glob(). Two rules are the whole of it -- a pattern is matched one SEGMENT at` |
|      - | 2853 | ` * a time, and the answer is sorted by BYTES rather than by value -- and both` |
|      - | 2854 | ` * are spelled here the way that function spells them, because the two` |
|      - | 2855 | ` * implementations must not drift: 001-smoke/glob_stream_device.phpt walks a` |
|      - | 2856 | ` * table of patterns through both and compares, which is what pins them.` |
|      - | 2857 | ` *` |
|      - | 2858 | `` * php's `pglob->path` is the directory of the match a read just handed OUT,`` |
|      - | 2859 | `` * never the pattern's: `glob://a/` + `*` + `/` + `*.txt` reports `a/sub1`, then`` |
|      - | 2860 | `` * `a/sub2`; a match with no slash in it reports the EMPTY string; and running`` |
|      - | 2861 | ` * out clears it, which is why GlobIterator::getPathname() answers "" past the` |
|      - | 2862 | ` * end.` |
|      - | 2863 | ` * ---------------------------------------------------------------------------` |
|      - | 2864 | ` */` |
|      - | 2865 | `/* One matched path. The blob it points into grows as the walk does, so an` |
|      - | 2866 | ` * OFFSET is what may be kept -- a pointer would not survive the next append. */` |
|      - | 2867 | `typedef struct glob_hit glob_hit;` |
|      - | 2868 | `struct glob_hit` |
|      - | 2869 | `{` |
|      - | 2870 | `	sxu32 nOfs;   /* where this path starts in glob_stream.sHit */` |
|      - | 2871 | `	sxu32 nLen;` |
|      - | 2872 | `};` |
|      - | 2873 | `typedef struct glob_stream glob_stream;` |
|      - | 2874 | `struct glob_stream` |
|      - | 2875 | `{` |
|      - | 2876 | `	ph7_vm *pVm;` |
|      - | 2877 | `	SyBlob sHit;   /* every matched path, back to back */` |
|      - | 2878 | `	SySet aHit;    /* one glob_hit per match, in php's order */` |
|      - | 2879 | `	sxu32 nCur;    /* php's pglob->index -- it counts PAST the end too */` |
|      - | 2880 | `	SyBlob sDir;   /* php's pglob->path: the directory of the CURRENT match */` |
|      - | 2881 | `};` |
|      - | 2882 | `/* php's glob_pattern_p: is there anything here for glob(3) to expand? */` |
|     92 | 2883 | `static int GlobHasMeta(const char *zPat,int nPat)` |
|      1 | 2884 | `{` |
|      - | 2885 | `	int i;` |
|   3727 | 2886 | `	for( i = 0 ; i < nPat ; ++i ){` |
|   3645 | 2887 | `		if( zPat[i] == '*' \|\| zPat[i] == '?' \|\| zPat[i] == '[' ){` |
|     11 | 2888 | `			return 1;` |
|      - | 2889 | `		}` |
|   2830 | 2890 | `	}` |
|     83 | 2891 | `	return 0;` |
|     47 | 2892 | `}` |
|      - | 2893 | `/* strcoll() in the C locale php runs in: unsigned bytes, then the shorter one` |
|      - | 2894 | ` * first. The same comparison the prelude glob() gets out of SORT_STRING. */` |
|    242 | 2895 | `static int GlobCmp(const char *zA,sxu32 nA,const char *zB,sxu32 nB)` |
|      1 | 2896 | `{` |
|    243 | 2897 | `	sxu32 nMin = nA < nB ? nA : nB;` |
|      - | 2898 | `	sxu32 i;` |
|   9889 | 2899 | `	for( i = 0 ; i < nMin ; ++i ){` |
|   9883 | 2900 | `		int ca = (unsigned char)zA[i];` |
|   9883 | 2901 | `		int cb = (unsigned char)zB[i];` |
|   9883 | 2902 | `		if( ca != cb ){` |
|    237 | 2903 | `			return ca < cb ? -1 : 1;` |
|      - | 2904 | `		}` |
|   7446 | 2905 | `	}` |
|      7 | 2906 | `	if( nA == nB ){` |
|    ! 0 | 2907 | `		return 0;` |
|      - | 2908 | `	}` |
|      7 | 2909 | `	return nA < nB ? -1 : 1;` |
|    121 | 2910 | `}` |
|      - | 2911 | `/*` |
|      - | 2912 | ` * Order one call's own answers. glob(3) sorts the whole list it is about to` |
|      - | 2913 | ` * return rather than each directory it walked, and so does the prelude glob(),` |
|      - | 2914 | ` * so every branch below sorts the range IT produced.` |
|      - | 2915 | ` *` |
|      - | 2916 | ` * A shell sort: filenames within one answer are unique, so nothing here needs` |
|      - | 2917 | ` * to be stable, and a directory of ten thousand entries must not cost the` |
|      - | 2918 | ` * hundred million comparisons an insertion sort would.` |
|      - | 2919 | ` */` |
|    104 | 2920 | `static void GlobSort(SyBlob *pHit,SySet *pSet,sxu32 nStart)` |
|      1 | 2921 | `{` |
|    105 | 2922 | `	glob_hit *aHit = (glob_hit *)SySetBasePtr(pSet);` |
|    105 | 2923 | `	const char *zBase = (const char *)SyBlobData(pHit);` |
|    105 | 2924 | `	sxu32 nEnd = SySetUsed(pSet);` |
|      - | 2925 | `	sxu32 nSpan,nGap;` |
|    105 | 2926 | `	if( nEnd - nStart < 2 ){` |
|     39 | 2927 | `		return;` |
|      - | 2928 | `	}` |
|     67 | 2929 | `	nSpan = nEnd - nStart;` |
|    153 | 2930 | `	for( nGap = nSpan / 2 ; nGap > 0 ; nGap /= 2 ){` |
|      - | 2931 | `		sxu32 i;` |
|    293 | 2932 | `		for( i = nStart + nGap ; i < nEnd ; ++i ){` |
|    207 | 2933 | `			glob_hit sTmp = aHit[i];` |
|    207 | 2934 | `			sxu32 j = i;` |
|    358 | 2935 | `			while( j >= nStart + nGap` |
|    416 | 2936 | `			 && GlobCmp(&zBase[aHit[j-nGap].nOfs],aHit[j-nGap].nLen,` |
|    362 | 2937 | `			            &zBase[sTmp.nOfs],sTmp.nLen) > 0 ){` |
|     87 | 2938 | `				aHit[j] = aHit[j-nGap];` |
|     87 | 2939 | `				j -= nGap;` |
|    ! 0 | 2940 | `			}` |
|    207 | 2941 | `			aHit[j] = sTmp;` |
|    104 | 2942 | `		}` |
|     44 | 2943 | `	}` |
|     53 | 2944 | `}` |
|      - | 2945 | `/* Record one match, spelled as a head and a tail so that the trailing-slash` |
|      - | 2946 | ` * branch can put its slash back without a second buffer. */` |
|    218 | 2947 | `static sxi32 GlobAdd(SyBlob *pHit,SySet *pSet,const char *zHead,sxu32 nHead,` |
|      - | 2948 | `	const char *zTail,sxu32 nTail)` |
|      1 | 2949 | `{` |
|      - | 2950 | `	glob_hit sHit;` |
|    219 | 2951 | `	sHit.nOfs = SyBlobLength(pHit);` |
|    219 | 2952 | `	sHit.nLen = nHead + nTail;` |
|    219 | 2953 | `	if( nHead > 0 && SyBlobAppend(pHit,zHead,nHead) != SXRET_OK ){` |
|    ! 0 | 2954 | `		return SXERR_MEM;` |
|      - | 2955 | `	}` |
|    219 | 2956 | `	if( nTail > 0 && SyBlobAppend(pHit,zTail,nTail) != SXRET_OK ){` |
|    ! 0 | 2957 | `		return SXERR_MEM;` |
|      - | 2958 | `	}` |
|    219 | 2959 | `	return SySetPut(pSet,(const void *)&sHit);` |
|    110 | 2960 | `}` |
|      - | 2961 | `/* Forward: the two halves of the walk call each other. */` |
|      - | 2962 | `static sxi32 GlobExpand(ph7_vm *pVm,const char *zPat,int nPat,int bOnlyDir,` |
|      - | 2963 | `	SyBlob *pHit,SySet *pSet);` |
|      - | 2964 | `/*` |
|      - | 2965 | ` * php's leaf: read the directory the pattern's last slash names and keep every` |
|      - | 2966 | ` * entry the segment after it matches, with that literal prefix back in front.` |
|      - | 2967 | ` * A directory that cannot be opened is zero matches in SILENCE, which is what` |
|      - | 2968 | ` * glob(3) answers for a path that is not there.` |
|      - | 2969 | ` */` |
|     82 | 2970 | `static sxi32 GlobLeaf(ph7_vm *pVm,const char *zPat,int nPat,int bOnlyDir,` |
|      - | 2971 | `	SyBlob *pHit,SySet *pSet)` |
|      1 | 2972 | `{` |
|     83 | 2973 | `	const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|      - | 2974 | `	const ph7_io_stream *pStream;` |
|      - | 2975 | `	const char *zDev,*zSeg;` |
|     83 | 2976 | `	void *pHandle = 0;` |
|      - | 2977 | `	ph7_context sCtx;` |
|      - | 2978 | `	ph7_value sEntry;` |
|      - | 2979 | `	char zDir[PH7_GLOB_PATH_MAX],zSegBuf[PH7_GLOB_PATH_MAX],zEnt[PH7_GLOB_PATH_MAX];` |
|     83 | 2980 | `	int nDir,nPrefix,nSeg,i,iSlash = -1;` |
|     83 | 2981 | `	sxi32 rc = SXRET_OK;` |
|    423 | 2982 | `	for( i = nPat - 1 ; i >= 0 ; --i ){` |
|    423 | 2983 | `		if( zPat[i] == '/' ){` |
|     83 | 2984 | `			iSlash = i;` |
|     83 | 2985 | `			break;` |
|      - | 2986 | `		}` |
|    171 | 2987 | `	}` |
|     83 | 2988 | `	if( iSlash < 0 ){` |
|      - | 2989 | ``		/* no directory part at all: php's own `.` */`` |
|    ! 0 | 2990 | `		zDir[0] = '.';` |
|    ! 0 | 2991 | `		nDir = 1;` |
|    ! 0 | 2992 | `		nPrefix = 0;` |
|     83 | 2993 | `	}else if( iSlash == 0 ){` |
|      - | 2994 | ``		/* the pattern is rooted: the directory is `/` itself */`` |
|    ! 0 | 2995 | `		zDir[0] = '/';` |
|    ! 0 | 2996 | `		nDir = 1;` |
|    ! 0 | 2997 | `		nPrefix = 1;` |
|    ! 0 | 2998 | `	}else{` |
|     83 | 2999 | `		nDir = iSlash;` |
|     83 | 3000 | `		SyMemcpy(zPat,zDir,(sxu32)nDir);` |
|     83 | 3001 | `		nPrefix = iSlash + 1;` |
|      - | 3002 | `	}` |
|     83 | 3003 | `	zDir[nDir] = 0;` |
|     83 | 3004 | `	zSeg = &zPat[iSlash + 1];` |
|     83 | 3005 | `	nSeg = nPat - (iSlash + 1);` |
|     83 | 3006 | `	if( nSeg >= (int)sizeof(zSegBuf) ){` |
|    ! 0 | 3007 | `		return SXRET_OK;` |
|      - | 3008 | `	}` |
|     83 | 3009 | `	SyMemcpy(zSeg,zSegBuf,(sxu32)nSeg);` |
|     83 | 3010 | `	zSegBuf[nSeg] = 0;` |
|      - | 3011 | `	/* The directory is opened through the SAME lookup opendir() uses, so the` |
|      - | 3012 | `	 * two implementations see one filesystem: the prelude glob() reaches it by` |
|      - | 3013 | `	 * calling opendir() itself. */` |
|     83 | 3014 | `	zDev = zDir;` |
|     83 | 3015 | `	pStream = PH7_VmGetStreamDevice(pVm,&zDev,nDir);` |
|     83 | 3016 | `	if( pStream == 0 \|\| pStream->xOpenDir == 0 \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 3017 | `		return SXRET_OK;` |
|      - | 3018 | `	}` |
|      - | 3019 | `	/* The VFS reports a name by writing a RESULT, so the read needs a call` |
|      - | 3020 | `	 * context of its own, and the cursor is reset between entries because a` |
|      - | 3021 | `	 * result APPENDS (rule 54). The same value carries the VM into the open:` |
|      - | 3022 | `	 * a device reaches it through that argument and nothing else. */` |
|     83 | 3023 | `	PH7_MemObjInit(pVm,&sEntry);` |
|     83 | 3024 | `	if( pStream->xOpenDir(zDev,&sEntry,&pHandle) != PH7_OK ){` |
|      3 | 3025 | `		PH7_MemObjRelease(&sEntry);` |
|      3 | 3026 | `		return SXRET_OK;` |
|      - | 3027 | `	}` |
|     81 | 3028 | `	VmInitCallContext(&sCtx,pVm,0,&sEntry,0);` |
|    479 | 3029 | `	for(;;){` |
|      - | 3030 | `		const char *zName;` |
|    875 | 3031 | `		int nName = 0;` |
|    875 | 3032 | `		ph7_value_reset_string_cursor(&sEntry);` |
|    875 | 3033 | `		if( pStream->xReadDir(pHandle,&sCtx) != PH7_OK ){` |
|     81 | 3034 | `			break;` |
|      - | 3035 | `		}` |
|    795 | 3036 | `		zName = ph7_value_to_string(&sEntry,&nName);` |
|    794 | 3037 | `		if( nName < 1 \|\| nName >= (int)sizeof(zEnt)` |
|    795 | 3038 | `		 \|\| nPrefix + nName >= (int)sizeof(zEnt) ){` |
|    ! 0 | 3039 | `			continue;` |
|      - | 3040 | `		}` |
|    795 | 3041 | `		SyMemcpy(zName,zEnt,(sxu32)nName);` |
|    795 | 3042 | `		zEnt[nName] = 0;` |
|      - | 3043 | `		/* php's FNM_PERIOD: a leading dot is matched only by a pattern that` |
|      - | 3044 | ``		 * spells one, which is what keeps `.`, `..` and every hidden name out`` |
|      - | 3045 | ``		 * of an ordinary `*`. */`` |
|    795 | 3046 | `		if( zEnt[0] == '.' && (nSeg < 1 \|\| zSegBuf[0] != '.') ){` |
|    201 | 3047 | `			continue;` |
|      - | 3048 | `		}` |
|    595 | 3049 | `		if( !Glob((const unsigned char *)zSegBuf,(const unsigned char *)zEnt,'\\',0,FALSE) ){` |
|    383 | 3050 | `			continue;` |
|      - | 3051 | `		}` |
|    213 | 3052 | `		if( bOnlyDir ){` |
|      - | 3053 | `			/* GLOB_ONLYDIR, which only the trailing-slash branch below asks` |
|      - | 3054 | `			 * for -- the device itself always globs with no flags at all. */` |
|      - | 3055 | `			char zProbe[PH7_GLOB_PATH_MAX * 2];` |
|     43 | 3056 | `			SyMemcpy(zDir,zProbe,(sxu32)nDir);` |
|     43 | 3057 | `			zProbe[nDir] = '/';` |
|     43 | 3058 | `			SyMemcpy(zEnt,&zProbe[nDir+1],(sxu32)nName);` |
|     43 | 3059 | `			zProbe[nDir + 1 + nName] = 0;` |
|     43 | 3060 | `			if( pVfs == 0 \|\| pVfs->xIsdir == 0 \|\| pVfs->xIsdir(zProbe) != PH7_OK ){` |
|     19 | 3061 | `				continue;` |
|      - | 3062 | `			}` |
|     12 | 3063 | `		}` |
|    195 | 3064 | `		rc = GlobAdd(pHit,pSet,zPat,(sxu32)nPrefix,zEnt,(sxu32)nName);` |
|    195 | 3065 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3066 | `			break;` |
|      - | 3067 | `		}` |
|      1 | 3068 | `	}` |
|     81 | 3069 | `	VmReleaseCallContext(&sCtx);` |
|     81 | 3070 | `	PH7_MemObjRelease(&sEntry);` |
|     81 | 3071 | `	if( pStream->xCloseDir ){` |
|     81 | 3072 | `		pStream->xCloseDir(pHandle);` |
|     40 | 3073 | `	}` |
|     81 | 3074 | `	return rc;` |
|     42 | 3075 | `}` |
|      - | 3076 | `/*` |
|      - | 3077 | ` * One pattern, every segment of it. php's three branches, in php's order: a` |
|      - | 3078 | ` * pattern that ENDS in a slash names directories and KEEPS the slash; a` |
|      - | 3079 | ` * wildcard in the directory part is walked level by level; anything else is` |
|      - | 3080 | ` * one directory read. Each branch sorts the range it produced.` |
|      - | 3081 | ` */` |
|    104 | 3082 | `static sxi32 GlobExpand(ph7_vm *pVm,const char *zPat,int nPat,int bOnlyDir,` |
|      - | 3083 | `	SyBlob *pHit,SySet *pSet)` |
|      1 | 3084 | `{` |
|    105 | 3085 | `	sxu32 nStart = SySetUsed(pSet);` |
|      - | 3086 | `	SyBlob sSub;` |
|      - | 3087 | `	SySet aSub;` |
|      - | 3088 | `	glob_hit *aRec;` |
|      - | 3089 | `	sxu32 n,nRec;` |
|    105 | 3090 | `	int i,iSlash = -1;` |
|      - | 3091 | `	sxi32 rc;` |
|    105 | 3092 | `	if( nPat < 1 \|\| nPat >= PH7_GLOB_PATH_MAX ){` |
|    ! 0 | 3093 | `		return SXRET_OK;` |
|      - | 3094 | `	}` |
|    105 | 3095 | `	if( zPat[nPat-1] == '/' ){` |
|      - | 3096 | ``		/* `d/` is ['d/'] and `d/` + `*` + `/` is ['d/a/','d/b/']: answer the base as`` |
|      - | 3097 | `		 * DIRECTORIES and put ONE slash back, so a pattern ending in two keeps` |
|      - | 3098 | `		 * both. php sorts the names it ANSWERS, slash included. */` |
|     13 | 3099 | `		const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|     13 | 3100 | `		if( nPat == 1 ){` |
|    ! 0 | 3101 | `			if( pVfs && pVfs->xIsdir && pVfs->xIsdir("/") == PH7_OK ){` |
|    ! 0 | 3102 | `				return GlobAdd(pHit,pSet,"/",1,0,0);` |
|      - | 3103 | `			}` |
|    ! 0 | 3104 | `			return SXRET_OK;` |
|      - | 3105 | `		}` |
|     13 | 3106 | `		SyBlobInit(&sSub,&pVm->sAllocator);` |
|     13 | 3107 | `		SySetInit(&aSub,&pVm->sAllocator,sizeof(glob_hit));` |
|     13 | 3108 | `		rc = GlobExpand(pVm,zPat,nPat-1,TRUE,&sSub,&aSub);` |
|     13 | 3109 | `		if( rc == SXRET_OK ){` |
|     13 | 3110 | `			aRec = (glob_hit *)SySetBasePtr(&aSub);` |
|     13 | 3111 | `			nRec = SySetUsed(&aSub);` |
|     37 | 3112 | `			for( n = 0 ; n < nRec ; ++n ){` |
|     37 | 3113 | `				rc = GlobAdd(pHit,pSet,` |
|     24 | 3114 | `					&((const char *)SyBlobData(&sSub))[aRec[n].nOfs],aRec[n].nLen,"/",1);` |
|     25 | 3115 | `				if( rc != SXRET_OK ){` |
|    ! 0 | 3116 | `					break;` |
|      - | 3117 | `				}` |
|     13 | 3118 | `			}` |
|      6 | 3119 | `		}` |
|     13 | 3120 | `		SyBlobRelease(&sSub);` |
|     13 | 3121 | `		SySetRelease(&aSub);` |
|     13 | 3122 | `		if( rc == SXRET_OK ){` |
|     13 | 3123 | `			GlobSort(pHit,pSet,nStart);` |
|      6 | 3124 | `		}` |
|     13 | 3125 | `		return rc;` |
|      - | 3126 | `	}` |
|    467 | 3127 | `	for( i = nPat - 1 ; i >= 0 ; --i ){` |
|    467 | 3128 | `		if( zPat[i] == '/' ){` |
|     93 | 3129 | `			iSlash = i;` |
|     93 | 3130 | `			break;` |
|      - | 3131 | `		}` |
|    188 | 3132 | `	}` |
|     93 | 3133 | `	if( iSlash > 0 && GlobHasMeta(zPat,iSlash) ){` |
|      - | 3134 | `		/* A wildcard in the DIRECTORY part is matched level by level, which is` |
|      - | 3135 | `		 * what glob(3) does: list the directories that part names, then glob` |
|      - | 3136 | `		 * the last component inside each. Reading only the last component` |
|      - | 3137 | ``		 * answers [] for `src/` + `*` + `/` + `*.php`, the everyday two-level`` |
|      - | 3138 | `		 * spelling, and for every deeper one. */` |
|     11 | 3139 | `		SyBlobInit(&sSub,&pVm->sAllocator);` |
|     11 | 3140 | `		SySetInit(&aSub,&pVm->sAllocator,sizeof(glob_hit));` |
|     11 | 3141 | `		rc = GlobExpand(pVm,zPat,iSlash+1,FALSE,&sSub,&aSub);` |
|     11 | 3142 | `		if( rc == SXRET_OK ){` |
|      - | 3143 | `			SyBlob sJoin;` |
|     11 | 3144 | `			SyBlobInit(&sJoin,&pVm->sAllocator);` |
|     11 | 3145 | `			aRec = (glob_hit *)SySetBasePtr(&aSub);` |
|     11 | 3146 | `			nRec = SySetUsed(&aSub);` |
|     31 | 3147 | `			for( n = 0 ; n < nRec ; ++n ){` |
|     21 | 3148 | `				SyBlobReset(&sJoin);` |
|     20 | 3149 | `				if( SyBlobAppend(&sJoin,` |
|     30 | 3150 | `					&((const char *)SyBlobData(&sSub))[aRec[n].nOfs],aRec[n].nLen) != SXRET_OK` |
|     21 | 3151 | `				 \|\| SyBlobAppend(&sJoin,&zPat[iSlash+1],(sxu32)(nPat - iSlash - 1)) != SXRET_OK ){` |
|    ! 0 | 3152 | `					rc = SXERR_MEM;` |
|    ! 0 | 3153 | `					break;` |
|      - | 3154 | `				}` |
|     31 | 3155 | `				rc = GlobExpand(pVm,(const char *)SyBlobData(&sJoin),` |
|     20 | 3156 | `					(int)SyBlobLength(&sJoin),bOnlyDir,pHit,pSet);` |
|     21 | 3157 | `				if( rc != SXRET_OK ){` |
|    ! 0 | 3158 | `					break;` |
|      - | 3159 | `				}` |
|     11 | 3160 | `			}` |
|     11 | 3161 | `			SyBlobRelease(&sJoin);` |
|      5 | 3162 | `		}` |
|     11 | 3163 | `		SyBlobRelease(&sSub);` |
|     11 | 3164 | `		SySetRelease(&aSub);` |
|     11 | 3165 | `		if( rc == SXRET_OK ){` |
|     11 | 3166 | `			GlobSort(pHit,pSet,nStart);` |
|      5 | 3167 | `		}` |
|     11 | 3168 | `		return rc;` |
|      - | 3169 | `	}` |
|     83 | 3170 | `	rc = GlobLeaf(pVm,zPat,nPat,bOnlyDir,pHit,pSet);` |
|     83 | 3171 | `	if( rc == SXRET_OK ){` |
|     83 | 3172 | `		GlobSort(pHit,pSet,nStart);` |
|     41 | 3173 | `	}` |
|     83 | 3174 | `	return rc;` |
|     53 | 3175 | `}` |
|      - | 3176 | `/* void (*xCloseDir)(void *) */` |
|     62 | 3177 | `static void GlobStream_CloseDir(void *pHandle)` |
|      1 | 3178 | `{` |
|     63 | 3179 | `	glob_stream *pGlob = (glob_stream *)pHandle;` |
|      - | 3180 | `	ph7_vm *pVm;` |
|     63 | 3181 | `	if( pGlob == 0 ){` |
|    ! 0 | 3182 | `		return;` |
|      - | 3183 | `	}` |
|     63 | 3184 | `	pVm = pGlob->pVm;` |
|     63 | 3185 | `	SyBlobRelease(&pGlob->sHit);` |
|     63 | 3186 | `	SyBlobRelease(&pGlob->sDir);` |
|     63 | 3187 | `	SySetRelease(&pGlob->aHit);` |
|     63 | 3188 | `	SyMemBackendFree(&pVm->sAllocator,pGlob);` |
|     32 | 3189 | `}` |
|      - | 3190 | `/*` |
|      - | 3191 | ` * int (*xOpenDir)(const char *,ph7_value *,void **)` |
|      - | 3192 | ` *` |
|      - | 3193 | ` * php's opener fails only on a glob(3) ERROR: no matches at all is an OPEN` |
|      - | 3194 | `` * stream with nothing in it, which is why `new GlobIterator('nope/' . '*')` is a`` |
|      - | 3195 | ` * working object whose count() is 0 rather than a constructor that throws.` |
|      - | 3196 | ` *` |
|      - | 3197 | ` * The VM comes in through the context argument, the way data:// takes it: the` |
|      - | 3198 | ` * walk allocates, and reads a directory through a call context of its own.` |
|      - | 3199 | ` */` |
|     62 | 3200 | `static int GlobStream_OpenDir(const char *zPattern,ph7_value *pResource,void **ppHandle)` |
|      1 | 3201 | `{` |
|     63 | 3202 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|      - | 3203 | `	glob_stream *pGlob;` |
|     63 | 3204 | `	if( pVm == 0 ){` |
|    ! 0 | 3205 | `		return -1;` |
|      - | 3206 | `	}` |
|     63 | 3207 | `	pGlob = (glob_stream *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(glob_stream));` |
|     63 | 3208 | `	if( pGlob == 0 ){` |
|    ! 0 | 3209 | `		return -1;` |
|      - | 3210 | `	}` |
|     63 | 3211 | `	pGlob->pVm = pVm;` |
|     63 | 3212 | `	pGlob->nCur = 0;` |
|     63 | 3213 | `	SyBlobInit(&pGlob->sHit,&pVm->sAllocator);` |
|     63 | 3214 | `	SyBlobInit(&pGlob->sDir,&pVm->sAllocator);` |
|     63 | 3215 | `	SySetInit(&pGlob->aHit,&pVm->sAllocator,sizeof(glob_hit));` |
|      - | 3216 | `	/* A pattern longer than one path is zero matches rather than a refusal (it` |
|      - | 3217 | `	 * can name nothing that exists), which is glob(3)'s GLOB_NOMATCH and an` |
|      - | 3218 | `	 * open stream either way. */` |
|     93 | 3219 | `	if( GlobExpand(pVm,zPattern,(int)SyStrlen(zPattern),FALSE,` |
|     63 | 3220 | `		&pGlob->sHit,&pGlob->aHit) != SXRET_OK ){` |
|    ! 0 | 3221 | `		GlobStream_CloseDir(pGlob);` |
|    ! 0 | 3222 | `		return -1;` |
|      - | 3223 | `	}` |
|     63 | 3224 | `	*ppHandle = (void *)pGlob;` |
|     63 | 3225 | `	return PH7_OK;` |
|     32 | 3226 | `}` |
|      - | 3227 | `/*` |
|      - | 3228 | ` * int (*xReadDir)(void *,ph7_context *)` |
|      - | 3229 | ` *` |
|      - | 3230 | ` * php's php_glob_stream_path_split, which runs on every read: the directory is` |
|      - | 3231 | ` * everything before the LAST slash and the ENTRY is what follows it. So a` |
|      - | 3232 | ``  * match with no slash in it reports an empty directory and itself, `a/` `` |
|      - | 3233 | `` * reports `a` and an EMPTY entry -- and an empty entry is what the SPL walk`` |
|      - | 3234 | ` * reads as the end, which is why GlobIterator over a trailing-slash pattern` |
|      - | 3235 | ` * counts its matches and yields none of them.` |
|      - | 3236 | ` */` |
|    208 | 3237 | `static int GlobStream_ReadDir(void *pHandle,ph7_context *pCtx)` |
|      1 | 3238 | `{` |
|    209 | 3239 | `	glob_stream *pGlob = (glob_stream *)pHandle;` |
|      - | 3240 | `	glob_hit *aHit;` |
|      - | 3241 | `	const char *zPath;` |
|      - | 3242 | `	sxu32 nPath;` |
|    209 | 3243 | `	int i,iSlash = -1;` |
|    209 | 3244 | `	if( pGlob == 0 ){` |
|    ! 0 | 3245 | `		return -1;` |
|      - | 3246 | `	}` |
|    209 | 3247 | `	if( pGlob->nCur >= SySetUsed(&pGlob->aHit) ){` |
|      - | 3248 | `		/* php drops the path when the walk runs out, and counts on past it. */` |
|     49 | 3249 | `		pGlob->nCur++;` |
|     49 | 3250 | `		SyBlobReset(&pGlob->sDir);` |
|     49 | 3251 | `		return -1;` |
|      - | 3252 | `	}` |
|    161 | 3253 | `	aHit = (glob_hit *)SySetBasePtr(&pGlob->aHit);` |
|    161 | 3254 | `	zPath = &((const char *)SyBlobData(&pGlob->sHit))[aHit[pGlob->nCur].nOfs];` |
|    161 | 3255 | `	nPath = aHit[pGlob->nCur].nLen;` |
|    161 | 3256 | `	pGlob->nCur++;` |
|    891 | 3257 | `	for( i = (int)nPath - 1 ; i >= 0 ; --i ){` |
|    891 | 3258 | `		if( zPath[i] == '/' ){` |
|    161 | 3259 | `			iSlash = i;` |
|    161 | 3260 | `			break;` |
|      - | 3261 | `		}` |
|    366 | 3262 | `	}` |
|    161 | 3263 | `	SyBlobReset(&pGlob->sDir);` |
|    161 | 3264 | `	if( iSlash >= 0 ){` |
|    161 | 3265 | `		if( iSlash > 0 && SyBlobAppend(&pGlob->sDir,zPath,(sxu32)iSlash) != SXRET_OK ){` |
|    ! 0 | 3266 | `			return -1;` |
|      - | 3267 | `		}` |
|    161 | 3268 | `		ph7_result_string(pCtx,&zPath[iSlash+1],(int)nPath - iSlash - 1);` |
|     81 | 3269 | `	}else{` |
|    ! 0 | 3270 | `		ph7_result_string(pCtx,zPath,(int)nPath);` |
|      - | 3271 | `	}` |
|    161 | 3272 | `	return PH7_OK;` |
|    105 | 3273 | `}` |
|      - | 3274 | `/* void (*xRewindDir)(void *): php's rewind moves the INDEX and leaves the path` |
|      - | 3275 | ` * where the last read put it -- every caller reads straight afterwards. */` |
|     12 | 3276 | `static void GlobStream_RewindDir(void *pHandle)` |
|      1 | 3277 | `{` |
|     13 | 3278 | `	glob_stream *pGlob = (glob_stream *)pHandle;` |
|     13 | 3279 | `	if( pGlob ){` |
|     13 | 3280 | `		pGlob->nCur = 0;` |
|      6 | 3281 | `	}` |
|     13 | 3282 | `}` |
|      - | 3283 | `PH7_PRIVATE const ph7_io_stream sGLOB_Stream = {` |
|      - | 3284 | `	"glob",` |
|      - | 3285 | `	PH7_IO_STREAM_VERSION,` |
|      - | 3286 | `	0,                    /* xOpen: php's wrapper has no stream opener at all */` |
|      - | 3287 | `	GlobStream_OpenDir,   /* xOpenDir */` |
|      - | 3288 | `	0,                    /* xClose */` |
|      - | 3289 | `	GlobStream_CloseDir,  /* xCloseDir */` |
|      - | 3290 | `	0,                    /* xRead */` |
|      - | 3291 | `	GlobStream_ReadDir,   /* xReadDir */` |
|      - | 3292 | `	0,                    /* xWrite */` |
|      - | 3293 | `	0,                    /* xSeek */` |
|      - | 3294 | `	0,                    /* xLock */` |
|      - | 3295 | `	GlobStream_RewindDir, /* xRewindDir */` |
|      - | 3296 | `	0,                    /* xTell */` |
|      - | 3297 | `	0,                    /* xTrunc */` |
|      - | 3298 | `	0,                    /* xSync */` |
|      - | 3299 | `	0                     /* xStat */` |
|      - | 3300 | `};` |
|      - | 3301 | `/* Is this the glob device? php's php_stream_is(), which is how SPL tells a` |
|      - | 3302 | ` * GlobIterator's directory handle from an ordinary one. */` |
|    173 | 3303 | `PH7_PRIVATE int PH7_GlobStreamIs(const ph7_io_stream *pStream)` |
|      1 | 3304 | `{` |
|    174 | 3305 | `	return pStream == &sGLOB_Stream;` |
|      1 | 3306 | `}` |
|      - | 3307 | `/* php's php_glob_stream_get_path: the directory of the CURRENT match, empty` |
|      - | 3308 | ` * both before the first read and after the last. */` |
|     60 | 3309 | `PH7_PRIVATE const char * PH7_GlobStreamPath(void *pHandle,int *pnLen)` |
|      1 | 3310 | `{` |
|     61 | 3311 | `	glob_stream *pGlob = (glob_stream *)pHandle;` |
|     61 | 3312 | `	if( pGlob == 0 ){` |
|    ! 0 | 3313 | `		*pnLen = 0;` |
|    ! 0 | 3314 | `		return "";` |
|      - | 3315 | `	}` |
|     61 | 3316 | `	*pnLen = (int)SyBlobLength(&pGlob->sDir);` |
|     61 | 3317 | `	return *pnLen > 0 ? (const char *)SyBlobData(&pGlob->sDir) : "";` |
|     31 | 3318 | `}` |
|      - | 3319 | `/* php's php_glob_stream_get_count: what GlobIterator::count() answers, and it` |
|      - | 3320 | ` * does not move with the walk. */` |
|     14 | 3321 | `PH7_PRIVATE sxi64 PH7_GlobStreamCount(void *pHandle)` |
|      1 | 3322 | `{` |
|     15 | 3323 | `	glob_stream *pGlob = (glob_stream *)pHandle;` |
|     15 | 3324 | `	return pGlob ? (sxi64)SySetUsed(&pGlob->aHit) : 0;` |
|      1 | 3325 | `}` |
|      - | 3326 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 3327 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 3328 | `/*` |
|      - | 3329 | ` * bool link(string $target,string $link)` |
|      - | 3330 |  |
|      - | 3331 | ` *  Create a hard link.` |
|      - | 3332 | ` * Parameters` |
|      - | 3333 | ` *  $target` |
|      - | 3334 | ` *   Target of the link.` |
|      - | 3335 | ` *  $link` |
|      - | 3336 | ` *   The link name.` |
|      - | 3337 | ` * Return` |
|      - | 3338 | ` *  TRUE on success or FALSE on failure.` |
|      - | 3339 | ` */` |
|      2 | 3340 | `static int PH7_vfs_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3341 | `{` |
|      - | 3342 | `	const char *zTarget,*zLink;` |
|      - | 3343 | `	ph7_vfs *pVfs;` |
|      - | 3344 | `	int rc;` |
|      3 | 3345 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 3346 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3347 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3348 | `		return PH7_OK;` |
|      - | 3349 | `	}` |
|      - | 3350 | `	/* Point to the underlying vfs */` |
|      3 | 3351 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 3352 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 3353 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 3354 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3355 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 3356 | `			ph7_function_name(pCtx)` |
|      - | 3357 | `			);` |
|    ! 0 | 3358 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3359 | `		return PH7_OK;` |
|      - | 3360 | `	}` |
|      - | 3361 | `	/* Extract the given arguments */` |
|      3 | 3362 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      3 | 3363 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 3364 | `	/* Perform the requested operation */` |
|      3 | 3365 | `	rc = pVfs->xLink(zTarget,zLink,0/*Not a symbolic link */);` |
|      - | 3366 | `	/* IO result */` |
|      3 | 3367 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 3368 | `	return PH7_OK;` |
|      2 | 3369 | `}` |
|      - | 3370 | `/*` |
|      - | 3371 | ` * string\|false readlink(string $path)` |
|      - | 3372 | ` *  Returns the target of a symbolic link.` |
|      - | 3373 | ` * Parameters` |
|      - | 3374 | ` *  $path` |
|      - | 3375 | ` *   The symbolic link path.` |
|      - | 3376 | ` * Return` |
|      - | 3377 | ` *  The contents of the link, or FALSE (with a warning) when $path is not a link` |
|      - | 3378 | ` *  or cannot be read -- php's own answer, error text included.` |
|      - | 3379 | ` */` |
|      8 | 3380 | `static int PH7_vfs_readlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 3381 | `{` |
|      - | 3382 | `	const char *zPath;` |
|      - | 3383 | `	ph7_vfs *pVfs;` |
|      - | 3384 | `	int rc;` |
|      8 | 3385 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    ! 0 | 3386 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3387 | `		return PH7_OK;` |
|      - | 3388 | `	}` |
|      8 | 3389 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      8 | 3390 | `	if( pVfs == 0 \|\| pVfs->xReadlink == 0 ){` |
|    ! 0 | 3391 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3392 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 3393 | `			ph7_function_name(pCtx)` |
|      - | 3394 | `			);` |
|    ! 0 | 3395 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3396 | `		return PH7_OK;` |
|      - | 3397 | `	}` |
|      8 | 3398 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      8 | 3399 | `	rc = pVfs->xReadlink(zPath,pCtx);` |
|      8 | 3400 | `	if( rc != PH7_OK ){` |
|      - | 3401 | `		/* php's wording is the errno text alone -- the engine prefixes the` |
|      - | 3402 | `		 * function name already. */` |
|      6 | 3403 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      4 | 3404 | `			"%s",VfsStrerror(errno));` |
|      4 | 3405 | `		ph7_result_bool(pCtx,0);` |
|      2 | 3406 | `	}` |
|      8 | 3407 | `	return PH7_OK;` |
|      4 | 3408 | `}` |
|      - | 3409 | `/*` |
|      - | 3410 | ` * bool symlink(string $target,string $link)` |
|      - | 3411 | ` *  Creates a symbolic link.` |
|      - | 3412 | ` * Parameters` |
|      - | 3413 | ` *  $target` |
|      - | 3414 | ` *   Target of the link.` |
|      - | 3415 | ` *  $link` |
|      - | 3416 | ` *   The link name.` |
|      - | 3417 | ` * Return` |
|      - | 3418 | ` *  TRUE on success or FALSE on failure.` |
|      - | 3419 | ` */` |
|     10 | 3420 | `static int PH7_vfs_symlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3421 | `{` |
|      - | 3422 | `	const char *zTarget,*zLink;` |
|      - | 3423 | `	ph7_vfs *pVfs;` |
|      - | 3424 | `	int rc;` |
|     11 | 3425 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 3426 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3427 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3428 | `		return PH7_OK;` |
|      - | 3429 | `	}` |
|      - | 3430 | `	/* Point to the underlying vfs */` |
|     11 | 3431 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 3432 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 3433 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 3434 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3435 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 3436 | `			ph7_function_name(pCtx)` |
|      - | 3437 | `			);` |
|    ! 0 | 3438 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3439 | `		return PH7_OK;` |
|      - | 3440 | `	}` |
|      - | 3441 | `	/* Extract the given arguments */` |
|     11 | 3442 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|     11 | 3443 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 3444 | `	/* Perform the requested operation */` |
|     11 | 3445 | `	rc = pVfs->xLink(zTarget,zLink,1/*A symbolic link */);` |
|      - | 3446 | `	/* IO result */` |
|     11 | 3447 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|     11 | 3448 | `	return PH7_OK;` |
|      6 | 3449 | `}` |
|      - | 3450 | `/*` |
|      - | 3451 | ` * int umask([ int $mask ])` |
|      - | 3452 | ` *  Changes the current umask.` |
|      - | 3453 | ` * Parameters` |
|      - | 3454 | ` *  $mask` |
|      - | 3455 | ` *   The new umask.` |
|      - | 3456 | ` * Return` |
|      - | 3457 | ` *  umask() without arguments simply returns the current umask.` |
|      - | 3458 | ` *  Otherwise the old umask is returned.` |
|      - | 3459 | ` */` |
|      8 | 3460 | `static int PH7_vfs_umask(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3461 | `{` |
|      - | 3462 | `	int iOld,iNew;` |
|      - | 3463 | `	ph7_vfs *pVfs;` |
|      - | 3464 | `	/* Point to the underlying vfs */` |
|      9 | 3465 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 | 3466 | `	if( pVfs == 0 \|\| pVfs->xUmask == 0 ){` |
|      - | 3467 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 3468 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3469 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 3470 | `			ph7_function_name(pCtx)` |
|      - | 3471 | `			);` |
|    ! 0 | 3472 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3473 | `		return PH7_OK;` |
|      - | 3474 | `	}` |
|      9 | 3475 | `	iNew = 0;` |
|      9 | 3476 | `	if( nArg > 0 ){` |
|      5 | 3477 | `		iNew = ph7_value_to_int(apArg[0]);` |
|      2 | 3478 | `	}` |
|      - | 3479 | `	/* Perform the requested operation */` |
|      9 | 3480 | `	iOld = pVfs->xUmask(iNew);` |
|      - | 3481 | `	/* Old mask */` |
|      9 | 3482 | `	ph7_result_int(pCtx,iOld);` |
|      9 | 3483 | `	return PH7_OK;` |
|      5 | 3484 | `}` |
|      - | 3485 | `/*` |
|      - | 3486 | ` * string sys_get_temp_dir()` |
|      - | 3487 | ` *  Returns directory path used for temporary files.` |
|      - | 3488 | ` * Parameters` |
|      - | 3489 | ` *  None` |
|      - | 3490 | ` * Return` |
|      - | 3491 | ` *  Returns the path of the temporary directory.` |
|      - | 3492 | ` */` |
|    808 | 3493 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3494 | `{` |
|      - | 3495 | `	ph7_vfs *pVfs;` |
|      - | 3496 | `	/* Set the empty string as the default return value */` |
|    813 | 3497 | `	ph7_result_string(pCtx,"",0);` |
|      - | 3498 | `	/* Point to the underlying vfs */` |
|    813 | 3499 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    813 | 3500 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
|    ! 0 | 3501 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 3502 | `		SXUNUSED(apArg);` |
|      - | 3503 | `		/* IO routine not implemented,return "" */` |
|    ! 0 | 3504 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3505 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 3506 | `			ph7_function_name(pCtx)` |
|      - | 3507 | `			);` |
|    ! 0 | 3508 | `		return PH7_OK;` |
|      - | 3509 | `	}` |
|      - | 3510 | `	/* Perform the requested operation */` |
|    813 | 3511 | `	pVfs->xTempDir(pCtx);` |
|    813 | 3512 | `	return PH7_OK;` |
|    409 | 3513 | `}` |
|      - | 3514 | `/*` |
|      - | 3515 | ` * string get_current_user()` |
|      - | 3516 | ` *  Returns the name of the current working user.` |
|      - | 3517 | ` * Parameters` |
|      - | 3518 | ` *  None` |
|      - | 3519 | ` * Return` |
|      - | 3520 | ` *  Returns the name of the current working user.` |
|      - | 3521 | ` */` |
|      2 | 3522 | `static int PH7_vfs_get_current_user(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3523 | `{` |
|      - | 3524 | `	ph7_vfs *pVfs;` |
|      - | 3525 | `	/* Point to the underlying vfs */` |
|      3 | 3526 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 3527 | `	if( pVfs == 0 \|\| pVfs->xUsername == 0 ){` |
|    ! 0 | 3528 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 3529 | `		SXUNUSED(apArg);` |
|      - | 3530 | `		/* IO routine not implemented */` |
|    ! 0 | 3531 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3532 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 3533 | `			ph7_function_name(pCtx)` |
|      - | 3534 | `			);` |
|      - | 3535 | `		/* Set a dummy username */` |
|    ! 0 | 3536 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 3537 | `		return PH7_OK;` |
|      - | 3538 | `	}` |
|      - | 3539 | `	/* Perform the requested operation */` |
|      3 | 3540 | `	pVfs->xUsername(pCtx);` |
|      3 | 3541 | `	return PH7_OK;` |
|      2 | 3542 | `}` |
|      - | 3543 | `/*` |
|      - | 3544 | ` * int64 getmypid()` |
|      - | 3545 | ` *  Gets process ID.` |
|      - | 3546 | ` * Parameters` |
|      - | 3547 | ` *  None` |
|      - | 3548 | ` * Return` |
|      - | 3549 | ` *  Returns the process ID.` |
|      - | 3550 | ` */` |
|    278 | 3551 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3552 | `{` |
|      - | 3553 | `	ph7_int64 nProcessId;` |
|      - | 3554 | `	ph7_vfs *pVfs;` |
|      - | 3555 | `	/* Point to the underlying vfs */` |
|    283 | 3556 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    283 | 3557 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
|    ! 0 | 3558 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 3559 | `		SXUNUSED(apArg);` |
|      - | 3560 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 3561 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3562 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 3563 | `			ph7_function_name(pCtx)` |
|      - | 3564 | `			);` |
|    ! 0 | 3565 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 3566 | `		return PH7_OK;` |
|      - | 3567 | `	}` |
|      - | 3568 | `	/* Perform the requested operation */` |
|    283 | 3569 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 3570 | `	/* Set the result */` |
|    283 | 3571 | `	ph7_result_int64(pCtx,nProcessId);` |
|    283 | 3572 | `	return PH7_OK;` |
|    144 | 3573 | `}` |
|      - | 3574 | `/*` |
|      - | 3575 | ` * int getmyuid()` |
|      - | 3576 | ` *  Get user ID.` |
|      - | 3577 | ` * Parameters` |
|      - | 3578 | ` *  None` |
|      - | 3579 | ` * Return` |
|      - | 3580 | ` *  Returns the user ID.` |
|      - | 3581 | ` */` |
|      4 | 3582 | `static int PH7_vfs_getmyuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3583 | `{` |
|      - | 3584 | `	ph7_vfs *pVfs;` |
|      - | 3585 | `	int nUid;` |
|      - | 3586 | `	/* Point to the underlying vfs */` |
|      5 | 3587 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 3588 | `	if( pVfs == 0 \|\| pVfs->xUid == 0 ){` |
|    ! 0 | 3589 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 3590 | `		SXUNUSED(apArg);` |
|      - | 3591 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 3592 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3593 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 3594 | `			ph7_function_name(pCtx)` |
|      - | 3595 | `			);` |
|    ! 0 | 3596 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 3597 | `		return PH7_OK;` |
|      - | 3598 | `	}` |
|      - | 3599 | `	/* Perform the requested operation */` |
|      5 | 3600 | `	nUid = pVfs->xUid();` |
|      - | 3601 | `	/* Set the result */` |
|      5 | 3602 | `	ph7_result_int(pCtx,nUid);` |
|      5 | 3603 | `	return PH7_OK;` |
|      3 | 3604 | `}` |
|      - | 3605 | `/*` |
|      - | 3606 | ` * int getmygid()` |
|      - | 3607 | ` *  Get group ID.` |
|      - | 3608 | ` * Parameters` |
|      - | 3609 | ` *  None` |
|      - | 3610 | ` * Return` |
|      - | 3611 | ` *  Returns the group ID.` |
|      - | 3612 | ` */` |
|      2 | 3613 | `static int PH7_vfs_getmygid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3614 | `{` |
|      - | 3615 | `	ph7_vfs *pVfs;` |
|      - | 3616 | `	int nGid;` |
|      - | 3617 | `	/* Point to the underlying vfs */` |
|      3 | 3618 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 3619 | `	if( pVfs == 0 \|\| pVfs->xGid == 0 ){` |
|    ! 0 | 3620 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 3621 | `		SXUNUSED(apArg);` |
|      - | 3622 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 3623 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3624 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 3625 | `			ph7_function_name(pCtx)` |
|      - | 3626 | `			);` |
|    ! 0 | 3627 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 3628 | `		return PH7_OK;` |
|      - | 3629 | `	}` |
|      - | 3630 | `	/* Perform the requested operation */` |
|      3 | 3631 | `	nGid = pVfs->xGid();` |
|      - | 3632 | `	/* Set the result */` |
|      3 | 3633 | `	ph7_result_int(pCtx,nGid);` |
|      3 | 3634 | `	return PH7_OK;` |
|      2 | 3635 | `}` |
|      - | 3636 | `#ifdef __WINNT__` |
|      - | 3637 | `#include <Windows.h>` |
|      - | 3638 | `#elif defined(__UNIXES__)` |
|      - | 3639 | `#include <sys/utsname.h>` |
|      - | 3640 | `#endif` |
|      - | 3641 | `/*` |
|      - | 3642 | ` * string php_uname([ string $mode = "a" ])` |
|      - | 3643 | ` *  Returns information about the host operating system.` |
|      - | 3644 | ` * Parameters` |
|      - | 3645 | ` *  $mode` |
|      - | 3646 | ` *   mode is a single character that defines what information is returned:` |
|      - | 3647 | ` *    'a': This is the default. Contains all modes in the sequence "s n r v m".` |
|      - | 3648 | ` *    's': Operating system name. eg. FreeBSD.` |
|      - | 3649 | ` *    'n': Host name. eg. localhost.example.com.` |
|      - | 3650 | ` *    'r': Release name. eg. 5.1.2-RELEASE.` |
|      - | 3651 | ` *    'v': Version information. Varies a lot between operating systems.` |
|      - | 3652 | ` *    'm': Machine type. eg. i386.` |
|      - | 3653 | ` * Return` |
|      - | 3654 | ` *  OS description as a string.` |
|      - | 3655 | ` */` |
|      4 | 3656 | `static int PH7_vfs_ph7_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3657 | `{` |
|      - | 3658 | `#if defined(__WINNT__)` |
|      1 | 3659 | `	const char *zName = "Microsoft Windows";` |
|      - | 3660 | `	OSVERSIONINFOW sVer;` |
|      - | 3661 | `#elif defined(__UNIXES__)` |
|      - | 3662 | `	struct utsname sName;` |
|      - | 3663 | `#endif` |
|      5 | 3664 | `	const char *zMode = "a";` |
|      5 | 3665 | `	if( nArg > 0 && ph7_value_is_string(apArg[0]) ){` |
|      - | 3666 | `		/* Extract the desired mode */` |
|    ! 0 | 3667 | `		zMode = ph7_value_to_string(apArg[0],0);` |
|    ! 0 | 3668 | `	}` |
|      - | 3669 | `#if defined(__WINNT__)` |
|      1 | 3670 | `	sVer.dwOSVersionInfoSize = sizeof(sVer);` |
|      - | 3671 | `	/* GetVersionExW is deprecated in modern MSVC. Suppress deprecation for this call. */` |
|      - | 3672 | `#if defined(_MSC_VER)` |
|      - | 3673 | `#pragma warning(push)` |
|      - | 3674 | `#pragma warning(disable:4996)` |
|      - | 3675 | `#endif` |
|      1 | 3676 | `	if( TRUE != GetVersionExW(&sVer)){` |
|      - | 3677 | `#if defined(_MSC_VER)` |
|      - | 3678 | `#pragma warning(pop)` |
|      - | 3679 | `#endif` |
|    ! 0 | 3680 | `		ph7_result_string(pCtx,zName,-1);` |
|    ! 0 | 3681 | `		return PH7_OK;` |
|      - | 3682 | `	}` |
|      1 | 3683 | `	if( sVer.dwPlatformId == VER_PLATFORM_WIN32_NT ){` |
|      1 | 3684 | `		if( sVer.dwMajorVersion <= 4 ){` |
|    ! 0 | 3685 | `			zName = "Microsoft Windows NT";` |
|      1 | 3686 | `		}else if( sVer.dwMajorVersion == 5 ){` |
|    ! 0 | 3687 | `			switch(sVer.dwMinorVersion){` |
|    ! 0 | 3688 | `				case 0:	zName = "Microsoft Windows 2000"; break;` |
|    ! 0 | 3689 | `				case 1: zName = "Microsoft Windows XP";   break;` |
|    ! 0 | 3690 | `				case 2: zName = "Microsoft Windows Server 2003"; break;` |
|      - | 3691 | `			}` |
|    ! 0 | 3692 | `		}else if( sVer.dwMajorVersion == 6){` |
|      1 | 3693 | `				switch(sVer.dwMinorVersion){` |
|    ! 0 | 3694 | `					case 0: zName = "Microsoft Windows Vista"; break;` |
|    ! 0 | 3695 | `					case 1: zName = "Microsoft Windows 7"; break;` |
|      1 | 3696 | `					case 2: zName = "Microsoft Windows Server 2008"; break;` |
|    ! 0 | 3697 | `					case 3: zName = "Microsoft Windows 8"; break;` |
|      - | 3698 | `					default: break;` |
|      - | 3699 | `				}` |
|      - | 3700 | `		}` |
|      - | 3701 | `	}` |
|      1 | 3702 | `	switch(zMode[0]){` |
|      - | 3703 | `	case 's':` |
|      - | 3704 | `		/* Operating system name */` |
|    ! 0 | 3705 | `		ph7_result_string(pCtx,zName,-1/* Compute length automatically*/);` |
|    ! 0 | 3706 | `		break;` |
|      - | 3707 | `	case 'n':` |
|      - | 3708 | `		/* Host name */` |
|    ! 0 | 3709 | `		ph7_result_string(pCtx,"localhost",(int)sizeof("localhost")-1);` |
|    ! 0 | 3710 | `		break;` |
|      - | 3711 | `	case 'r':` |
|      - | 3712 | `	case 'v':` |
|      - | 3713 | `		/* Version information. */` |
|    ! 0 | 3714 | `		ph7_result_string_format(pCtx,"%u.%u build %u",` |
|      - | 3715 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 3716 | `			);` |
|    ! 0 | 3717 | `		break;` |
|      - | 3718 | `	case 'm':` |
|      - | 3719 | `		/* Machine name */` |
|    ! 0 | 3720 | `		ph7_result_string(pCtx,"x86",(int)sizeof("x86")-1);` |
|    ! 0 | 3721 | `		break;` |
|      - | 3722 | `	default:` |
|      1 | 3723 | `		ph7_result_string_format(pCtx,"%s localhost %u.%u build %u x86",` |
|      - | 3724 | `			zName,` |
|      - | 3725 | `			sVer.dwMajorVersion,sVer.dwMinorVersion,sVer.dwBuildNumber` |
|      - | 3726 | `			);` |
|      - | 3727 | `		break;` |
|      - | 3728 | `	}` |
|      - | 3729 | `#elif defined(__UNIXES__)` |
|      4 | 3730 | `	if( uname(&sName) != 0 ){` |
|    ! 0 | 3731 | `		ph7_result_string(pCtx,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 | 3732 | `		return PH7_OK;` |
|      - | 3733 | `	}` |
|      4 | 3734 | `	switch(zMode[0]){` |
|    ! 0 | 3735 | `	case 's':` |
|      - | 3736 | `		/* Operating system name */` |
|    ! 0 | 3737 | `		ph7_result_string(pCtx,sName.sysname,-1/* Compute length automatically*/);` |
|    ! 0 | 3738 | `		break;` |
|    ! 0 | 3739 | `	case 'n':` |
|      - | 3740 | `		/* Host name */` |
|    ! 0 | 3741 | `		ph7_result_string(pCtx,sName.nodename,-1/* Compute length automatically*/);` |
|    ! 0 | 3742 | `		break;` |
|    ! 0 | 3743 | `	case 'r':` |
|      - | 3744 | `		/* Release information */` |
|    ! 0 | 3745 | `		ph7_result_string(pCtx,sName.release,-1/* Compute length automatically*/);` |
|    ! 0 | 3746 | `		break;` |
|    ! 0 | 3747 | `	case 'v':` |
|      - | 3748 | `		/* Version information. */` |
|    ! 0 | 3749 | `		ph7_result_string(pCtx,sName.version,-1/* Compute length automatically*/);` |
|    ! 0 | 3750 | `		break;` |
|    ! 0 | 3751 | `	case 'm':` |
|      - | 3752 | `		/* Machine name */` |
|    ! 0 | 3753 | `		ph7_result_string(pCtx,sName.machine,-1/* Compute length automatically*/);` |
|    ! 0 | 3754 | `		break;` |
|      2 | 3755 | `	default:` |
|      6 | 3756 | `		ph7_result_string_format(pCtx,` |
|      - | 3757 | `			"%s %s %s %s %s",` |
|      2 | 3758 | `			sName.sysname,` |
|      2 | 3759 | `			sName.release,` |
|      2 | 3760 | `			sName.version,` |
|      2 | 3761 | `			sName.nodename,` |
|      2 | 3762 | `			sName.machine` |
|      - | 3763 | `			);` |
|      4 | 3764 | `		break;` |
|      - | 3765 | `	}` |
|      - | 3766 | `#else` |
|      - | 3767 | `	ph7_result_string(pCtx,"Unknown Operating System",(int)sizeof("Unknown Operating System")-1);` |
|      - | 3768 | `#endif` |
|      5 | 3769 | `	return PH7_OK;` |
|      3 | 3770 | `}` |
|      - | 3771 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 3772 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 3773 | `#if defined(_MSC_VER)` |
|      - | 3774 | `static const ph7_vfs null_vfs = {` |
|      - | 3775 | `#else` |
|      - | 3776 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 3777 | `#endif` |
|      - | 3778 | `	"null_vfs",` |
|      - | 3779 | `	PH7_VFS_VERSION,` |
|      - | 3780 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 3781 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 3782 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 3783 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 3784 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 3785 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 3786 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 3787 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 3788 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 3789 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 3790 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 3791 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 3792 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 3793 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 3794 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 3795 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 3796 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 3797 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 3798 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 3799 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 3800 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 3801 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 3802 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 3803 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 3804 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 3805 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 3806 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 3807 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 3808 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 3809 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 3810 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 3811 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 3812 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 3813 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 3814 | `	0, /* int (*xUmask)(int) */` |
|      - | 3815 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 3816 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 3817 | `	0, /* int (*xUid)(void) */` |
|      - | 3818 | `	0, /* int (*xGid)(void) */` |
|      - | 3819 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 3820 | `	0, /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 3821 | `	0, /* int (*xReadlink)(const char *,ph7_context *) */` |
|      - | 3822 | `	0  /* int (*xEnviron)(ph7_context *) */` |
|      - | 3823 | `};` |
|      - | 3824 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 3825 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 3826 | `/*` |
|      - | 3827 | ` * Export the builtin vfs.` |
|      - | 3828 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 3829 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 3830 | ` * Note:` |
|      - | 3831 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 3832 | ` * Note:` |
|      - | 3833 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 3834 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 3835 | ` */` |
|   5742 | 3836 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 3837 | `{` |
|      - | 3838 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 3839 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 3840 | `	return &null_vfs;` |
|      - | 3841 | `#else` |
|      - | 3842 | `#ifdef __WINNT__` |
|      5 | 3843 | `	return &sWinVfs;` |
|      - | 3844 | `#elif defined(__UNIXES__)` |
|   5742 | 3845 | `	return &sUnixVfs;` |
|      - | 3846 | `#else` |
|      - | 3847 | `	return &null_vfs;` |
|      - | 3848 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 3849 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 3850 | `#else` |
|      - | 3851 | `	return &null_vfs;` |
|      - | 3852 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 3853 | `}` |
|      - | 3854 | `/*` |
|      - | 3855 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 3856 | ` * [i.e: file://,php://].` |
|      - | 3857 | ` * Note:` |
|      - | 3858 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 3859 | ` *  defined then this function is a no-op.` |
|      - | 3860 | ` */` |
|   4962 | 3861 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 3862 | `{` |
|      - | 3863 | `	/*` |
|      - | 3864 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 3865 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 3866 | `	 */` |
|      - | 3867 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 3868 | `	/* VFS: disk I/O related functions */` |
|      - | 3869 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 3870 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 3871 | `#ifndef __WINNT__` |
|      - | 3872 | `		/* php declares chroot() on POSIX only — there is no such call on Windows,` |
|      - | 3873 | ``		 * so `function_exists('chroot')` is FALSE there and the name is free for a`` |
|      - | 3874 | `		 * script to define. PHL used to declare it on both and answer a` |
|      - | 3875 | `		 * "not implemented in the underlying VFS" warning + false on Windows,` |
|      - | 3876 | `		 * which is a different thing from php's undefined function. (chown/chgrp/` |
|      - | 3877 | `		 * link/symlink/readlink stay: php declares all five on Windows.) */` |
|      - | 3878 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 3879 | `#endif` |
|      - | 3880 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 3881 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 3882 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 3883 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 3884 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 3885 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 3886 | `		/* php's own resolver, and the question include/require answer silently:` |
|      - | 3887 | `		 * it walks the same include_path in the same order, so it belongs beside` |
|      - | 3888 | `		 * realpath() rather than with the stream builtins. */` |
|      - | 3889 | `		{"stream_resolve_include_path",PH7_vfs_stream_resolve_include_path},` |
|      - | 3890 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 3891 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 3892 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 3893 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 3894 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 3895 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 3896 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 3897 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 3898 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 3899 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 3900 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 3901 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 3902 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 3903 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 3904 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 3905 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 3906 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 3907 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 3908 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 3909 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 3910 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 3911 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 3912 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 3913 | `		{"fileowner",   PH7_vfs_file_owner},` |
|      - | 3914 | `		{"filegroup",   PH7_vfs_file_group},` |
|      - | 3915 | `		{"fileinode",   PH7_vfs_file_inode},` |
|      - | 3916 | `		{"fileperms",   PH7_vfs_file_perms},` |
|      - | 3917 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 3918 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 3919 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 3920 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 3921 | `		{"link",        PH7_vfs_link     },` |
|      - | 3922 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 3923 | `		{"readlink",    PH7_vfs_readlink },` |
|      - | 3924 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 3925 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 3926 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 3927 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 3928 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 3929 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 3930 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 3931 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 3932 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 3933 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 3934 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 3935 | `	};` |
|      - | 3936 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 3937 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 3938 | `	 */` |
|      - | 3939 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 3940 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 3941 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 3942 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 3943 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 3944 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 3945 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 3946 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 3947 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 3948 | `		{"fscanf",    PH7_builtin_fscanf },` |
|      - | 3949 | `		{"stream_get_line", PH7_builtin_stream_get_line },` |
|      - | 3950 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 3951 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 3952 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 3953 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 3954 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 3955 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 3956 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 3957 | `		/* php's dir() lives with opendir(), which is what it calls and what its` |
|      - | 3958 | `		 * failure warning is worded by. Registering it here also means the TINY` |
|      - | 3959 | `		 * build drops BOTH: the prelude copy was defined there and fataled on` |
|      - | 3960 | `		 * "Call to undefined function opendir()" the moment it was called. */` |
|      - | 3961 | `		{"dir",       PH7_builtin_dir },` |
|      - | 3962 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 3963 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 3964 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 3965 | `		{"file",      PH7_builtin_file   },` |
|      - | 3966 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 3967 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 3968 | `		{"stream_isatty", PH7_builtin_stream_isatty },` |
|      - | 3969 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 3970 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 3971 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 3972 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 3973 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 3974 | `		{"stream_get_contents",  PH7_builtin_stream_get_contents },` |
|      - | 3975 | `		{"stream_get_wrappers",  PH7_builtin_stream_get_wrappers },` |
|      - | 3976 | `		{"stream_get_meta_data", PH7_builtin_stream_get_meta_data },` |
|      - | 3977 | `		/* php's own alias, kept from the days sockets had a separate API. */` |
|      - | 3978 | `		{"socket_get_status",    PH7_builtin_stream_get_meta_data },` |
|      - | 3979 | `		{"stream_set_blocking",  PH7_builtin_stream_set_blocking },` |
|      - | 3980 | `		{"socket_set_blocking",  PH7_builtin_stream_set_blocking },` |
|      - | 3981 | `		{"stream_set_timeout",   PH7_builtin_stream_set_timeout },` |
|      - | 3982 | `		{"stream_set_chunk_size",PH7_builtin_stream_set_chunk_size },` |
|      - | 3983 | `		{"stream_set_read_buffer",  PH7_builtin_stream_set_read_buffer },` |
|      - | 3984 | `		{"stream_set_write_buffer", PH7_builtin_stream_set_write_buffer },` |
|      - | 3985 | `		{"set_file_buffer",         PH7_builtin_stream_set_write_buffer },` |
|      - | 3986 | `		{"stream_supports_lock", PH7_builtin_stream_supports_lock },` |
|      - | 3987 | `		{"stream_is_local",      PH7_builtin_stream_is_local },` |
|      - | 3988 | `		{"stream_copy_to_stream",PH7_builtin_stream_copy_to_stream },` |
|      - | 3989 | `		{"stream_get_transports",PH7_builtin_stream_get_transports },` |
|      - | 3990 | `		/* Not under PH7_ENABLE_NET: a script selects over FILES and pipes in a` |
|      - | 3991 | `		 * build with no networking at all. */` |
|      - | 3992 | `		{"stream_select",        PH7_builtin_stream_select },` |
|      - | 3993 | `		{"stream_context_create",PH7_builtin_stream_context_create },` |
|      - | 3994 | `		{"stream_context_get_options",PH7_builtin_stream_context_get_options },` |
|      - | 3995 | `		{"stream_context_set_option", PH7_builtin_stream_context_set_option },` |
|      - | 3996 | `		{"stream_context_set_options",PH7_builtin_stream_context_set_options },` |
|      - | 3997 | `		{"stream_context_get_params", PH7_builtin_stream_context_get_params },` |
|      - | 3998 | `		{"stream_context_set_params", PH7_builtin_stream_context_set_params },` |
|      - | 3999 | `		{"stream_context_get_default",PH7_builtin_stream_context_get_default },` |
|      - | 4000 | `		{"stream_context_set_default",PH7_builtin_stream_context_set_default },` |
|      - | 4001 | `		{"stream_wrapper_register",   PH7_builtin_stream_wrapper_register },` |
|      - | 4002 | `		{"stream_register_wrapper",   PH7_builtin_stream_wrapper_register },` |
|      - | 4003 | `		{"stream_wrapper_unregister", PH7_builtin_stream_wrapper_unregister },` |
|      - | 4004 | `		{"stream_wrapper_restore",    PH7_builtin_stream_wrapper_restore },` |
|      - | 4005 | `		{"stream_filter_append",  PH7_builtin_stream_filter_append },` |
|      - | 4006 | `		{"stream_filter_prepend", PH7_builtin_stream_filter_prepend },` |
|      - | 4007 | `		{"stream_filter_remove",  PH7_builtin_stream_filter_remove },` |
|      - | 4008 | `		{"stream_get_filters",    PH7_builtin_stream_get_filters },` |
|      - | 4009 | `		{"stream_filter_register",PH7_builtin_stream_filter_register },` |
|      - | 4010 | `		{"stream_bucket_make_writeable", PH7_builtin_stream_bucket_make_writeable },` |
|      - | 4011 | `		{"stream_bucket_append",  PH7_builtin_stream_bucket_append },` |
|      - | 4012 | `		{"stream_bucket_prepend", PH7_builtin_stream_bucket_prepend },` |
|      - | 4013 | `		{"stream_bucket_new",     PH7_builtin_stream_bucket_new },` |
|      - | 4014 | `#ifdef PH7_ENABLE_NET` |
|      - | 4015 | `		{"fsockopen",  PH7_builtin_fsockopen },` |
|      - | 4016 | `		{"pfsockopen", PH7_builtin_fsockopen },` |
|      - | 4017 | `		{"stream_socket_client", PH7_builtin_fsockopen },` |
|      - | 4018 | `		{"stream_socket_server", PH7_builtin_stream_socket_server },` |
|      - | 4019 | `		{"stream_socket_accept", PH7_builtin_stream_socket_accept },` |
|      - | 4020 | `		{"stream_socket_get_name", PH7_builtin_stream_socket_get_name },` |
|      - | 4021 | `		{"stream_socket_pair",   PH7_builtin_stream_socket_pair },` |
|      - | 4022 | `		{"stream_socket_shutdown", PH7_builtin_stream_socket_shutdown },` |
|      - | 4023 | `		{"stream_socket_recvfrom", PH7_builtin_stream_socket_recvfrom },` |
|      - | 4024 | `		{"stream_socket_sendto",   PH7_builtin_stream_socket_sendto },` |
|      - | 4025 | `#endif` |
|      - | 4026 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 4027 | `		{"proc_open",      PH7_builtin_proc_open      },` |
|      - | 4028 | `		{"proc_close",     PH7_builtin_proc_close     },` |
|      - | 4029 | `		{"proc_terminate", PH7_builtin_proc_terminate },` |
|      - | 4030 | `		{"proc_get_status",PH7_builtin_proc_get_status},` |
|      - | 4031 | `		{"proc_nice",      PH7_builtin_proc_nice      },` |
|      - | 4032 | `		{"shell_exec", PH7_builtin_shell_exec },` |
|      - | 4033 | `		{"exec",       PH7_builtin_exec     },` |
|      - | 4034 | `		{"system",     PH7_builtin_system   },` |
|      - | 4035 | `		{"passthru",   PH7_builtin_passthru },` |
|      - | 4036 | `		/* The shell-escaping pair lives with the command runners it exists to` |
|      - | 4037 | `		 * feed: a build without process execution has nothing to escape for. */` |
|      - | 4038 | `		{"escapeshellarg", PH7_builtin_escapeshellarg },` |
|      - | 4039 | `		{"escapeshellcmd", PH7_builtin_escapeshellcmd },` |
|      - | 4040 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 4041 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 4042 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 4043 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 4044 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4045 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 4046 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 4047 | `		/* The hash extension's file readers live with the disk table for the` |
|      - | 4048 | `		 * same reason md5_file does: without disk IO there is nothing to read. */` |
|      - | 4049 | `		{"hash_file",          PH7_builtin_hash_file },` |
|      - | 4050 | `		{"hash_hmac_file",     PH7_builtin_hash_hmac_file },` |
|      - | 4051 | `		{"hash_update_file",   PH7_builtin_hash_update_file },` |
|      - | 4052 | `		{"hash_update_stream", PH7_builtin_hash_update_stream },` |
|      - | 4053 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4054 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 4055 | `		{"vfprintf",  PH7_builtin_vfprintf}` |
|      - | 4056 | `	};` |
|   4967 | 4057 | `	const ph7_io_stream *pFileStream = 0;` |
|   4967 | 4058 | `	sxu32 n = 0;` |
|      - | 4059 | `	/* Register disk-related functions */` |
| 272915 | 4060 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 267953 | 4061 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
| 133979 | 4062 | `	}` |
| 506129 | 4063 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 501167 | 4064 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
| 250586 | 4065 | `	}` |
|      - | 4066 | `#else` |
|      - | 4067 | `	SXUNUSED(pVm);` |
|      - | 4068 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 4069 |  |
|      - | 4070 | `	/*` |
|      - | 4071 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 4072 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 4073 | `	 */` |
|      - | 4074 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 4075 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 4076 | `		/* Path processing */` |
|      - | 4077 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 4078 | `		{"basename",    PH7_builtin_basename },` |
|      - | 4079 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 4080 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 4081 | `		{"fnmatch",     PH7_builtin_fnmatch  }` |
|      - | 4082 | `	};` |
|  29777 | 4083 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  24815 | 4084 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|  12410 | 4085 | `	}` |
|      - | 4086 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 4087 |  |
|      - | 4088 | `	/* Install streams if disk I/O is enabled */` |
|      - | 4089 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 4090 | `#ifdef __WINNT__` |
|      5 | 4091 | `	pFileStream = &sWinFileStream;` |
|      - | 4092 | `#elif defined(__UNIXES__)` |
|   4962 | 4093 | `	pFileStream = &sUnixFileStream;` |
|      - | 4094 | `#endif` |
|      - | 4095 | `	/* Install the php:// stream */` |
|   4967 | 4096 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|   4967 | 4097 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);` |
|      - | 4098 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 4099 | `	/* glob:// lives beside the pattern matcher it drives, so it is only in the` |
|      - | 4100 | `	 * build when that is. */` |
|   4967 | 4101 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sGLOB_Stream);` |
|      - | 4102 | `#endif` |
|      - | 4103 | `#ifdef PH7_ENABLE_NET` |
|   4967 | 4104 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);` |
|      - | 4105 | `#endif` |
|   4967 | 4106 | `	if( pFileStream ){` |
|      - | 4107 | `		/* Install the file:// stream */` |
|   4967 | 4108 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   2481 | 4109 | `	}` |
|      - | 4110 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 4111 |  |
|   4967 | 4112 | `	return SXRET_OK;` |
|      5 | 4113 | `}` |
|      - | 4114 |  |
