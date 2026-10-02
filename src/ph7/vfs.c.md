# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1948/2421 lines (80.46%)

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
|      - |   12 | `#include <sys/types.h>` |
|      - |   13 | `#include <sys/stat.h>  /* the shared fstat() shape of a stream's stat answer */` |
|      - |   14 | `#ifdef __UNIXES__` |
|      - |   15 | `#include <unistd.h>` |
|      - |   16 | `#include <sys/wait.h>` |
|      - |   17 | `#include <fcntl.h>` |
|      - |   18 | `#include <signal.h>` |
|      - |   19 | `#endif` |
|      - |   20 | `/*` |
|      - |   21 | ` * This file implement a virtual file systems (VFS) for the PH7 engine.` |
|      - |   22 | ` */` |
|      - |   23 | `/*` |
|      - |   24 | ` * Given a string containing the path of a file or directory, this function` |
|      - |   25 | ` * return the parent directory's path.` |
|      - |   26 | ` */` |
|  20180 |   27 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)` |
|      5 |   28 | `{` |
|      - |   29 | `	/* php_dirname: strip any trailing separators, cut at the last remaining one,` |
|      - |   30 | `	 * then strip trailing separators off the parent too. The previous version` |
|      - |   31 | `	 * scanned back to the first separator and stopped, so it never coped with a` |
|      - |   32 | `	 * trailing separator or a run of them: dirname("/a/") answered "/a" instead` |
|      - |   33 | `	 * of "/", dirname("a//b") answered "a/", and dirname("///") answered "//".` |
|      - |   34 | `	 * It also answered "." for the empty string, where php answers "". */` |
|      - |   35 | `	int c,d,iEnd,i;` |
|      - |   36 | `#ifdef __WINNT__` |
|      5 |   37 | `	const char *zRoot = "\\";` |
|      - |   38 | `#else` |
|  20180 |   39 | `	const char *zRoot = "/";` |
|      - |   40 | `#endif` |
|  20185 |   41 | `	c = d = '/';` |
|      - |   42 | `#ifdef __WINNT__` |
|      5 |   43 | `	d = '\\';` |
|      - |   44 | `#endif` |
|      - |   45 | `#define DIR_IS_SEP(x) ( (int)(x) == c \|\| (int)(x) == d )` |
|  20185 |   46 | `	if( nByte < 1 ){` |
|      - |   47 | `		/* php returns the empty string for the empty path */` |
|      5 |   48 | `		*pLen = 0;` |
|      5 |   49 | `		return "";` |
|      - |   50 | `	}` |
|  20181 |   51 | `	iEnd = nByte;` |
|  30297 |   52 | `	while( iEnd > 0 && DIR_IS_SEP(zPath[iEnd - 1]) ){` |
|     29 |   53 | `		iEnd--;` |
|      1 |   54 | `	}` |
|  20181 |   55 | `	if( iEnd == 0 ){` |
|      - |   56 | `		/* The path is nothing but separators: the root is its own parent */` |
|     17 |   57 | `		*pLen = (int)sizeof(char);` |
|     17 |   58 | `		return zRoot;` |
|      - |   59 | `	}` |
|      - |   60 | `	/* Walk back to the separator that ends the parent directory */` |
|  20165 |   61 | `	i = iEnd;` |
| 555525 |   62 | `	while( i > 0 && !DIR_IS_SEP(zPath[i - 1]) ){` |
| 535365 |   63 | `		i--;` |
|      5 |   64 | `	}` |
|  20165 |   65 | `	if( i == 0 ){` |
|      - |   66 | `		/* No separator at all,return "." as the current directory */` |
|     70 |   67 | `		*pLen = (int)sizeof(char);` |
|     70 |   68 | `		return ".";` |
|      - |   69 | `	}` |
|      - |   70 | `	/* Drop the separator, plus any that repeat before it */` |
|  50225 |   71 | `	while( i > 1 && DIR_IS_SEP(zPath[i - 1]) ){` |
|  20087 |   72 | `		i--;` |
|      5 |   73 | `	}` |
|  20097 |   74 | `	if( i == 1 && DIR_IS_SEP(zPath[0]) ){` |
|     13 |   75 | `		*pLen = (int)sizeof(char);` |
|     13 |   76 | `		return zRoot;` |
|      - |   77 | `	}` |
|  20085 |   78 | `	*pLen = i;` |
|  20085 |   79 | `	return zPath;` |
|      - |   80 | `#undef DIR_IS_SEP` |
|  10095 |   81 | `}` |
|      - |   82 | `/*` |
|      - |   83 | ` * php_basename: drop any trailing separators, then answer what follows the last` |
|      - |   84 | ` * remaining one. Shared by basename() and pathinfo() — they used to hand-roll the` |
|      - |   85 | ` * same walk separately, and pathinfo()'s copy kept the trailing separator run` |
|      - |   86 | `` * (`pathinfo("/var/www/")` answered basename "" where php answers "www").`` |
|      - |   87 | ` */` |
|  19966 |   88 | `PH7_PRIVATE const char * PH7_ExtractBaseName(const char *zPath,int nByte,int *pLen)` |
|      5 |   89 | `{` |
|      - |   90 | `	int c,d,iEnd,i;` |
|  19971 |   91 | `	c = d = '/';` |
|      - |   92 | `#ifdef __WINNT__` |
|      5 |   93 | `	d = '\\';` |
|      - |   94 | `#endif` |
|      - |   95 | `#define DIR_IS_SEP(x) ( (int)(x) == c \|\| (int)(x) == d )` |
|  19971 |   96 | `	iEnd = nByte;` |
|  29979 |   97 | `	while( iEnd > 0 && DIR_IS_SEP(zPath[iEnd - 1]) ){` |
|     27 |   98 | `		iEnd--;` |
|      1 |   99 | `	}` |
|  19971 |  100 | `	if( iEnd < 1 ){` |
|      - |  101 | `		/* Empty, or nothing but separators: php answers the empty string */` |
|     29 |  102 | `		*pLen = 0;` |
|     29 |  103 | `		return "";` |
|      - |  104 | `	}` |
|  19943 |  105 | `	i = iEnd;` |
| 548392 |  106 | `	while( i > 0 && !DIR_IS_SEP(zPath[i - 1]) ){` |
| 528454 |  107 | `		i--;` |
|      5 |  108 | `	}` |
|  19943 |  109 | `	*pLen = iEnd - i;` |
|  19943 |  110 | `	return &zPath[i];` |
|      - |  111 | `#undef DIR_IS_SEP` |
|   9988 |  112 | `}` |
|      - |  113 | `/*` |
|      - |  114 | `` * php's `ValueError: Path must not be empty`, raised by its STREAM LAYER before`` |
|      - |  115 | ` * anything is looked up -- so every door that opens one gets it, unqualified by` |
|      - |  116 | `` * a function name, and a program that hands `''` to file_get_contents() catches`` |
|      - |  117 | ` * an exception rather than reading a warning and a false.` |
|      - |  118 | ` *` |
|      - |  119 | ` * Three doors say it in their own words instead, naming the argument` |
|      - |  120 | ` * (parse_ini_file, scandir, and the DirectoryIterator/DOM constructors that` |
|      - |  121 | ` * already did), and the STAT family says nothing at all: an empty path there is` |
|      - |  122 | `` * a silent false, where a missing one is php's `stat failed for` warning.`` |
|      - |  123 | ` *` |
|      - |  124 | ` * Answers 1 once the exception is raised, which is the caller's cue to return.` |
|      - |  125 | ` */` |
|  44950 |  126 | `PH7_PRIVATE int PH7_VfsEmptyPathRefused(ph7_context *pCtx,int nPath)` |
|      5 |  127 | `{` |
|  44955 |  128 | `	if( nPath > 0 ){` |
|  44925 |  129 | `		return 0;` |
|      - |  130 | `	}` |
|     31 |  131 | `	PH7_VmThrowException(pCtx,"ValueError","Path must not be empty");` |
|     31 |  132 | `	return 1;` |
|  22443 |  133 | `}` |
|      - |  134 | `/*` |
|      - |  135 | ` * Is this path already anchored -- a leading slash, or a drive prefix on` |
|      - |  136 | ` * Windows? Everything else resolves against the working directory.` |
|      - |  137 | ` */` |
|    400 |  138 | `PH7_PRIVATE int PH7_VfsPathIsAbsolute(const char *zPath,int nPath)` |
|      4 |  139 | `{` |
|    404 |  140 | `	if( nPath < 1 ){` |
|    ! 0 |  141 | `		return 0;` |
|      - |  142 | `	}` |
|    404 |  143 | `	if( zPath[0] == '/' ){` |
|    383 |  144 | `		return 1;` |
|      - |  145 | `	}` |
|      - |  146 | `#ifdef __WINNT__` |
|      3 |  147 | `	if( zPath[0] == '\\' ){` |
|    ! 0 |  148 | `		return 1;` |
|      - |  149 | `	}` |
|      3 |  150 | `	if( nPath > 2 && zPath[1] == ':' && (zPath[2] == '/' \|\| zPath[2] == '\\') ){` |
|      3 |  151 | `		return 1;` |
|      - |  152 | `	}` |
|      - |  153 | `#endif` |
|     19 |  154 | `	return 0;` |
|    204 |  155 | `}` |
|      - |  156 | `/*` |
|      - |  157 | `` * php's `expand_filepath()`: the absolute form of a name, built without asking`` |
|      - |  158 | ` * the filesystem anything. Two extensions run every filename they are given` |
|      - |  159 | ` * through it -- ext/sqlite3 before it hands one to the library, and ext/zip for` |
|      - |  160 | `` * the `filename` property a script reads back -- and both need the same two`` |
|      - |  161 | ` * properties: a relative name resolves against the working directory, and a` |
|      - |  162 | ` * name that does not EXIST expands just as well as one that does.` |
|      - |  163 | ` *` |
|      - |  164 | `` * The `.` and `..` segments are collapsed HERE rather than by the filesystem,`` |
|      - |  165 | ``  * which is php's own behaviour: `sub/../db` names `db` even when no `sub` `` |
|      - |  166 | ` * directory exists, where handing the OS the uncollapsed path is ENOENT.` |
|      - |  167 | ` *` |
|      - |  168 | ` * The result is NUL-terminated without counting the byte, so it is both a` |
|      - |  169 | ` * length-carrying blob and a C string.` |
|      - |  170 | ` */` |
|    400 |  171 | `PH7_PRIVATE void PH7_VfsExpandPath(ph7_context *pCtx,const char *zPath,int nPath,SyBlob *pOut)` |
|      4 |  172 | `{` |
|      - |  173 | `	SyBlob sRaw;` |
|      - |  174 | `	const char *z;` |
|      - |  175 | `	sxu32 n,nRoot,nRaw;` |
|    404 |  176 | `	SyBlobInit(pOut,&pCtx->pVm->sAllocator);` |
|    404 |  177 | `	SyBlobInit(&sRaw,&pCtx->pVm->sAllocator);` |
|    404 |  178 | `	if( !PH7_VfsPathIsAbsolute(zPath,nPath) ){` |
|      - |  179 | `		/* the VFS answers through the context's RESULT slot, which the caller` |
|      - |  180 | `		 * overwrites with its own return value afterwards */` |
|     19 |  181 | `		const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     19 |  182 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     18 |  183 | `		if( pVfs && pVfs->xGetcwd && pVfs->xGetcwd(pCtx) == PH7_OK` |
|     19 |  184 | `		 && (pCtx->pRet->iFlags & MEMOBJ_STRING) != 0 ){` |
|     28 |  185 | `			SyBlobAppend(&sRaw,SyBlobData(&pCtx->pRet->sBlob),` |
|     18 |  186 | `				SyBlobLength(&pCtx->pRet->sBlob));` |
|      9 |  187 | `		}` |
|     19 |  188 | `		PH7_MemObjRelease(pCtx->pRet);` |
|     19 |  189 | `		SyBlobAppend(&sRaw,PH7_PATH_SEP_STR,sizeof(PH7_PATH_SEP_STR)-1);` |
|      9 |  190 | `	}` |
|    404 |  191 | `	if( nPath > 0 ){` |
|    404 |  192 | `		SyBlobAppend(&sRaw,zPath,(sxu32)nPath);` |
|    200 |  193 | `	}` |
|      - |  194 | `	/* Keep the root -- a leading slash, or a drive prefix -- and rebuild the` |
|      - |  195 | `	 * rest segment by segment. */` |
|    404 |  196 | `	z = (const char *)SyBlobData(&sRaw);` |
|    404 |  197 | `	nRaw = SyBlobLength(&sRaw);` |
|    404 |  198 | `	nRoot = 0;` |
|      - |  199 | `#ifdef __WINNT__` |
|      4 |  200 | `	if( nRaw > 1 && z[1] == ':' ){` |
|      3 |  201 | `		nRoot = 2;` |
|      - |  202 | `	}` |
|      - |  203 | `#endif` |
|    404 |  204 | `	if( nRoot < nRaw && (z[nRoot] == '/' \|\| z[nRoot] == '\\') ){` |
|    404 |  205 | `		++nRoot;` |
|    200 |  206 | `	}` |
|    404 |  207 | `	SyBlobAppend(pOut,z,nRoot);` |
|   2419 |  208 | `	for( n = nRoot ; n < nRaw ; ){` |
|   2019 |  209 | `		sxu32 nStart = n;` |
|      - |  210 | `		sxu32 nSeg;` |
|  18857 |  211 | `		while( n < nRaw && z[n] != '/' && z[n] != '\\' ){` |
|  16842 |  212 | `			++n;` |
|      4 |  213 | `		}` |
|   2019 |  214 | `		nSeg = n - nStart;` |
|   2019 |  215 | `		if( n < nRaw ){` |
|   1619 |  216 | `			++n;   /* step past the separator */` |
|   1210 |  217 | `		}` |
|   2019 |  218 | `		if( nSeg == 0 \|\| (nSeg == 1 && z[nStart] == '.') ){` |
|      3 |  219 | ``			continue;   /* `//` and `.` name the directory they stand in */`` |
|      - |  220 | `		}` |
|   2017 |  221 | `		if( nSeg == 2 && z[nStart] == '.' && z[nStart+1] == '.' ){` |
|      - |  222 | ``			/* pop the previous segment; `..` above the root is the root */`` |
|      5 |  223 | `			sxu32 nHave = SyBlobLength(pOut);` |
|     17 |  224 | `			while( nHave > nRoot && ((char *)SyBlobData(pOut))[nHave-1] != PH7_PATH_SEP ){` |
|     13 |  225 | `				--nHave;` |
|      1 |  226 | `			}` |
|      5 |  227 | `			if( nHave > nRoot ){` |
|      5 |  228 | `				--nHave;   /* and the separator that held it */` |
|      2 |  229 | `			}` |
|      5 |  230 | `			pOut->nByte = nHave;   /* the blob has no truncate of its own */` |
|      5 |  231 | `			continue;` |
|      - |  232 | `		}` |
|   2013 |  233 | `		if( SyBlobLength(pOut) > nRoot ){` |
|      - |  234 | `			/* php's expansion writes the PLATFORM's separator, which is what a` |
|      - |  235 | ``			 * name it hands back reads as: ext/zip's `filename` property is the`` |
|      - |  236 | `			 * one surface that shows it, and a Windows one there is spelled` |
|      - |  237 | `			 * with backslashes exactly as php spells it. */` |
|   1613 |  238 | `			SyBlobAppend(pOut,PH7_PATH_SEP_STR,sizeof(PH7_PATH_SEP_STR)-1);` |
|   1207 |  239 | `		}` |
|   2013 |  240 | `		SyBlobAppend(pOut,&z[nStart],nSeg);` |
|      4 |  241 | `	}` |
|    404 |  242 | `	SyBlobRelease(&sRaw);` |
|    404 |  243 | `	SyBlobNullAppend(pOut);` |
|    404 |  244 | `}` |
|      - |  245 | `/*` |
|      - |  246 | ` * Compile the VFS implementations when builtins are enabled OR when disk I/O` |
|      - |  247 | ` * is explicitly enabled (i.e. PH7_DISABLE_DISK_IO is NOT defined).` |
|      - |  248 | ` */` |
|      - |  249 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  250 | `/*` |
|      - |  251 | ` * strerror() trips MSVC's C4996 "may be unsafe" deprecation under /WX. It is a` |
|      - |  252 | ` * standard C function we use deliberately to mirror php's IO error text; wrap it` |
|      - |  253 | ` * once with the deprecation suppressed. The pragma is _MSC_VER-guarded so the` |
|      - |  254 | ` * GCC/-Werror Linux build never sees an unknown-pragma warning.` |
|      - |  255 | ` */` |
|  31510 |  256 | `PH7_PRIVATE const char * VfsStrerror(int iErr)` |
|      5 |  257 | `{` |
|      - |  258 | `#if defined(_MSC_VER)` |
|      - |  259 | `#pragma warning(push)` |
|      - |  260 | `#pragma warning(disable:4996)` |
|      - |  261 | `#endif` |
|  31515 |  262 | `	return strerror(iErr);` |
|      - |  263 | `#if defined(_MSC_VER)` |
|      - |  264 | `#pragma warning(pop)` |
|      - |  265 | `#endif` |
|      5 |  266 | `}` |
|      - |  267 | `/*` |
|      - |  268 | ` * php's non-open IO failures: "unlink(/nope): No such file or directory".` |
|      - |  269 | ` * PH7 returned FALSE in SILENCE for unlink/rmdir/mkdir/rename/chdir/opendir/scandir and` |
|      - |  270 | ` * filesize, so a script could not tell a failed operation from a successful one without` |
|      - |  271 | ` * checking the return value it never got told to check.` |
|      - |  272 | ` */` |
|  31234 |  273 | `static void VfsThrowSysWarning(ph7_context *pCtx,const char *zPath)` |
|      5 |  274 | `{` |
|  46854 |  275 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s",` |
|  31234 |  276 | `		ph7_function_name(pCtx),zPath ? zPath : "",VfsStrerror(errno));` |
|  31239 |  277 | `}` |
|      - |  278 | `/*` |
|      - |  279 | ` * php's "fopen(data://x): Failed to open stream: rfc2397: no comma in URL".` |
|      - |  280 | ` *` |
|      - |  281 | ` * Two things in that sentence were this engine's own. The NAME was whatever` |
|      - |  282 | ` * PH7_VmGetStreamDevice() left after the scheme, so a wrapper's failure blamed` |
|      - |  283 | `` * a path the script never wrote (`fopen(x)`, `fopen(nosuchthing)`); it is the`` |
|      - |  284 | ` * whole URI when the caller is reporting the open that lookup resolved, which` |
|      - |  285 | ` * is what the pointer it handed back identifies. And the REASON was errno --` |
|      - |  286 | ` * which only the plain-file wrapper sets, so every other one reported whatever` |
|      - |  287 | `` * errno happened to be lying around, including `Success` for a failed open.`` |
|      - |  288 | ` * php's reason is the wrapper's: its own sentence when it logged one, and a` |
|      - |  289 | ` * flat "operation failed" when it did not.` |
|      - |  290 | ` */` |
|      - |  291 | `/*` |
|      - |  292 | ` * The errno a stream OPEN reports, which is not always the one the system left.` |
|      - |  293 | ` *` |
|      - |  294 | ` * php's plain-file opener expands the whole path before it opens anything, so a` |
|      - |  295 | ` * name whose parent is a FILE rather than a directory fails there and reports` |
|      - |  296 | `` * `No such file or directory` -- where the open itself would have said `Not a`` |
|      - |  297 | `` * directory`. The substitution belongs to the OPEN and to nothing else: an`` |
|      - |  298 | ` * opendir(), an unlink(), a rename(), a touch() and a mkdir() on the same name` |
|      - |  299 | ` * all report ENOTDIR under php, and it is only what a script reads after` |
|      - |  300 | `` * `Failed to open stream:` that changes.`` |
|      - |  301 | ` */` |
|    134 |  302 | `PH7_PRIVATE const char * PH7_VfsOpenStrerror(int iErr)` |
|      5 |  303 | `{` |
|      - |  304 | `#ifdef ENOTDIR` |
|    139 |  305 | `	if( iErr == ENOTDIR ){` |
|      2 |  306 | `		iErr = ENOENT;` |
|      1 |  307 | `	}` |
|      - |  308 | `#endif` |
|    139 |  309 | `	return VfsStrerror(iErr);` |
|      5 |  310 | `}` |
|    292 |  311 | `PH7_PRIVATE void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile)` |
|      5 |  312 | `{` |
|    297 |  313 | `	ph7_vm *pVm = pCtx->pVm;` |
|    297 |  314 | `	const char *zName = zFile ? zFile : "";` |
|      - |  315 | `	char zFn[64];` |
|    297 |  316 | `	int nName = -1;` |
|    297 |  317 | `	if( zFile != 0 && zFile == pVm->zOpenUriTail && pVm->zOpenUri != 0 ){` |
|    190 |  318 | `		zName = pVm->zOpenUri;` |
|    190 |  319 | `		nName = pVm->nOpenUri;` |
|     92 |  320 | `	}` |
|    495 |  321 | `	PH7_VmThrowWarningFmt(pVm,"%s(%.*s): Failed to open stream: %s",` |
|    145 |  322 | `		PH7_CtxDiagFuncName(pCtx,zFn,(int)sizeof(zFn)),` |
|    198 |  323 | `		nName < 0 ? (int)SyStrlen(zName) : nName,zName,` |
|    292 |  324 | `		pVm->zOpenErr ? pVm->zOpenErr : PH7_VfsOpenStrerror(errno));` |
|    297 |  325 | `}` |
|      - |  326 | `/*` |
|      - |  327 | ` * php's answer when NO wrapper will take a name is a reason of its own, raised` |
|      - |  328 | ` * before the operation's own failure and naming the scheme the script wrote:` |
|      - |  329 | ` *` |
|      - |  330 | ` *   file_get_contents(): Unable to find the wrapper "zzz" - did you forget to` |
|      - |  331 | ` *   enable it when you configured PHP?` |
|      - |  332 | ` *` |
|      - |  333 | ` * PHL raised one PH7-specific sentence -- "No such stream device,PH7 is` |
|      - |  334 | ` * returning FALSE" -- which names neither the function's argument nor what was` |
|      - |  335 | ` * wrong with it, and two more call sites had a third wording of their own.` |
|      - |  336 | ` */` |
|     16 |  337 | `PH7_PRIVATE void VfsThrowNoDeviceWarning(ph7_context *pCtx,const char *zUri,int bDir)` |
|      2 |  338 | `{` |
|      - |  339 | `	char zFn[64];` |
|      - |  340 | `	/* php names the function the SCRIPT called: a prelude builtin's own name,` |
|      - |  341 | `	 * not the host builtin it delegated the open to (PH7_CtxDiagFuncName). */` |
|     18 |  342 | `	const char *zFunc = PH7_CtxDiagFuncName(pCtx,zFn,(int)sizeof(zFn));` |
|     18 |  343 | `	const char *zWhat = bDir ? "directory" : "stream";` |
|     18 |  344 | `	int nScheme = 0;` |
|     18 |  345 | `	if( zUri == 0 ){` |
|    ! 0 |  346 | `		zUri = "";` |
|    ! 0 |  347 | `	}` |
|     18 |  348 | `	if( PH7_VmStreamDeviceIsRemoteHost(zUri,-1,&nScheme) ){` |
|      - |  349 | `		/* A wrapper WAS found for the scheme and refused the name, which php` |
|      - |  350 | `		 * words differently from a scheme nothing is registered under. */` |
|     22 |  351 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Remote host file access not supported, %s",` |
|      7 |  352 | `			zFunc,zUri);` |
|     22 |  353 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      7 |  354 | `			"%s(%s): Failed to open %s: no suitable wrapper could be found",zFunc,zUri,zWhat);` |
|     16 |  355 | `		return;` |
|      - |  356 | `	}` |
|      3 |  357 | `	if( nScheme > 0 ){` |
|    ! 0 |  358 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - |  359 | `			"%s(): Unable to find the wrapper \"%.*s\" - did you forget to enable it when you configured PHP?",` |
|    ! 0 |  360 | `			zFunc,nScheme,zUri);` |
|    ! 0 |  361 | `	}` |
|      - |  362 | `	/* A name with no scheme is the plain-files wrapper's, and that is the ONE` |
|      - |  363 | `	 * php reports as switched off rather than missing: its fallback branch runs` |
|      - |  364 | ``	 * after the hash lookup, so an explicit `file://` gets both sentences and a`` |
|      - |  365 | `	 * bare path only the second. Every other unregistered wrapper is` |
|      - |  366 | `	 * indistinguishable from one that never existed, and php words it that way. */` |
|      2 |  367 | `	if( (nScheme == 0` |
|      1 |  368 | `	  \|\| (nScheme == (int)sizeof("file")-1 && SyStrnicmp(zUri,"file",sizeof("file")-1) == 0))` |
|      3 |  369 | `	 && PH7_VmStreamSchemeDisabled(pCtx->pVm,"file",(int)sizeof("file")-1) ){` |
|      4 |  370 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      1 |  371 | `			"%s(): file:// wrapper is disabled in the server configuration",zFunc);` |
|      4 |  372 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      1 |  373 | `			"%s(%s): Failed to open %s: no suitable wrapper could be found",zFunc,zUri,zWhat);` |
|      3 |  374 | `		return;` |
|      - |  375 | `	}` |
|    ! 0 |  376 | `	PH7_VmThrowWarningFmt(pCtx->pVm,` |
|    ! 0 |  377 | `		"%s(%s): Failed to open %s: No such file or directory",zFunc,zUri,zWhat);` |
|     10 |  378 | `}` |
|      - |  379 | `/*` |
|      - |  380 | ` * The FIRST half of the sentence above, on its own: php's` |
|      - |  381 | `` * `Unable to find the wrapper "zzz" - did you forget to enable it when you`` |
|      - |  382 | `` * configured PHP?`. A caller that resolves a wrapper WITHOUT opening anything`` |
|      - |  383 | ` * (get_headers()) reports only this one -- the failed-open line under it comes` |
|      - |  384 | ` * from the open, and there is none.` |
|      - |  385 | ` */` |
|    124 |  386 | `PH7_PRIVATE void VfsThrowUnknownWrapperWarning(ph7_context *pCtx,const char *zUri)` |
|      5 |  387 | `{` |
|      - |  388 | `	char zFn[64];` |
|    129 |  389 | `	int nScheme = 0;` |
|    129 |  390 | `	if( zUri == 0 ){` |
|    ! 0 |  391 | `		zUri = "";` |
|    ! 0 |  392 | `	}` |
|    129 |  393 | `	PH7_VmStreamDeviceIsRemoteHost(zUri,-1,&nScheme);` |
|    129 |  394 | `	if( nScheme > 0 ){` |
|    191 |  395 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - |  396 | `			"%s(): Unable to find the wrapper \"%.*s\" - did you forget to enable it when you configured PHP?",` |
|     62 |  397 | `			PH7_CtxDiagFuncName(pCtx,zFn,(int)sizeof(zFn)),nScheme,zUri);` |
|     62 |  398 | `	}` |
|    129 |  399 | `}` |
|      - |  400 | `/*` |
|      - |  401 | ` * A scheme NOBODY is registered under is not a refusal in php: its lookup warns,` |
|      - |  402 | ` * FORGETS the protocol, and hands the whole uri -- scheme and all -- to the` |
|      - |  403 | ` * plain-files wrapper, which resolves it against the filesystem like any other` |
|      - |  404 | `` * relative name. So `file_get_contents('zzz://hit.php')` warns once and then`` |
|      - |  405 | `` * READS ./zzz:/hit.php, and `scandir('zzz://')` lists that directory, where this`` |
|      - |  406 | ` * engine warned twice and answered FALSE. Every door that opens a url wants that` |
|      - |  407 | ` * behaviour, and reading the lookup's NULL as "no wrapper will take this" left` |
|      - |  408 | ` * the fallback stated in the include door alone.` |
|      - |  409 | ` *` |
|      - |  410 | ` * Two cases keep the refusal, because php reaches no fallback for them either:` |
|      - |  411 | ` *` |
|      - |  412 | ` *   file://host/path  -- a wrapper WAS found and declined the name, which php` |
|      - |  413 | ` *                        words as its own sentence (see VfsThrowNoDeviceWarning).` |
|      - |  414 | ` *   file:// disabled  -- php's fallback branch IS the plain-files wrapper, so a` |
|      - |  415 | ` *                        configuration that switched it off has nothing to fall` |
|      - |  416 | ` *                        back to.` |
|      - |  417 | ` *` |
|      - |  418 | ` * A scheme that is merely SUPPRESSED falls back with the rest: php cannot tell an` |
|      - |  419 | ` * unregistered wrapper from one that never existed, and neither can a script.` |
|      - |  420 | ` *` |
|      - |  421 | ` * *pzUri is in/out exactly as PH7_VmGetStreamDevice leaves it -- advanced past a` |
|      - |  422 | ` * scheme some wrapper answered for, and left at the WHOLE uri when the fallback` |
|      - |  423 | ` * takes it, which is the string the plain-files wrapper is meant to see.` |
|      - |  424 | ` */` |
|  62119 |  425 | `PH7_PRIVATE const ph7_io_stream * PH7_VfsStreamDeviceOrFile(` |
|      - |  426 | `	ph7_context *pCtx,     /* Call context, for the warning's function name */` |
|      - |  427 | `	const char **pzUri,    /* IN: full uri. OUT: what the wrapper is handed */` |
|      - |  428 | `	int nByte              /* *pzUri length */` |
|      - |  429 | `	)` |
|      5 |  430 | `{` |
|  62124 |  431 | `	const char *zUri = *pzUri;` |
|      - |  432 | `	const ph7_io_stream *pStream;` |
|  62124 |  433 | `	int nScheme = 0;` |
|  62124 |  434 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,pzUri,nByte);` |
|  62124 |  435 | `	if( pStream != 0 ){` |
|  62036 |  436 | `		return pStream;` |
|      - |  437 | `	}` |
|     91 |  438 | `	if( PH7_VmStreamDeviceIsRemoteHost(zUri,nByte,&nScheme) \|\| nScheme < 1 ){` |
|     18 |  439 | `		return 0;` |
|      - |  440 | `	}` |
|     75 |  441 | `	if( PH7_VmStreamSchemeDisabled(pCtx->pVm,"file",(int)sizeof("file")-1) ){` |
|    ! 0 |  442 | `		return 0;` |
|      - |  443 | `	}` |
|     75 |  444 | `	VfsThrowUnknownWrapperWarning(pCtx,zUri);` |
|     75 |  445 | `	*pzUri = zUri;` |
|     75 |  446 | `	return PH7_VmFindStreamDevice(pCtx->pVm,"file",(int)sizeof("file")-1);` |
|  31010 |  447 | `}` |
|      - |  448 | `/*` |
|      - |  449 | `` * php's stat-failure warning: `filemtime(): stat failed for /nope`, and`` |
|      - |  450 | `` * `filetype(): Lstat failed for /nope` for the two members that LSTAT. php raises`` |
|      - |  451 | ` * it from php_stat() for the whole family and answers FALSE; PHL answered the` |
|      - |  452 | ` * VFS's raw -1 (or the string "unknown") for most of them, in silence -- and -1 is` |
|      - |  453 | `` * TRUTHY, so `if (filemtime($f))` took the found branch for a file that is not`` |
|      - |  454 | `` * there and `filemtime($a) > filemtime($b)` compared a real time against it.`` |
|      - |  455 | ` */` |
|     62 |  456 | `static void VfsThrowStatWarning(ph7_context *pCtx,const char *zPath,int bLstat)` |
|      3 |  457 | `{` |
|     65 |  458 | `	if( zPath == 0 \|\| zPath[0] == 0 ){` |
|      - |  459 | `		/* php's stat family says NOTHING about an empty path -- it answers the` |
|      - |  460 | `		 * same FALSE a missing one gets and skips the warning, which is the one` |
|      - |  461 | `		 * place in the family where the empty path is not the opener's` |
|      - |  462 | `		 * ValueError but a silence of its own. */` |
|     12 |  463 | `		return;` |
|      - |  464 | `	}` |
|     78 |  465 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s failed for %s",` |
|     25 |  466 | `		ph7_function_name(pCtx),bLstat ? "Lstat" : "stat",zPath);` |
|     34 |  467 | `}` |
|      - |  468 | `/*` |
|      - |  469 | ` * php's stat() answer is TWENTY-SIX entries, not thirteen: the same thirteen` |
|      - |  470 | ` * fields once at numeric indices 0..12 and once under their names, in this` |
|      - |  471 | ` * order. The numeric half is what php's own documentation indexes by ($s[7] is` |
|      - |  472 | `` * the size) and it is what a `list()`/destructuring reader takes, so a script`` |
|      - |  473 | `` * written against php read `Undefined array key 7` here and answered NULL.`` |
|      - |  474 | ` *` |
|      - |  475 | ` * The VFS fills the NAMED half (both the unix and Windows implementations use` |
|      - |  476 | ` * exactly these keys), so the doubling is done once, here, rather than in every` |
|      - |  477 | ` * xStat: pOut gets the numeric run first and then the names, which is php's own` |
|      - |  478 | ` * insertion order — visible through foreach, print_r, var_dump and json_encode.` |
|      - |  479 | ` */` |
|     72 |  480 | `PH7_PRIVATE int PH7_VfsStatDoubleUp(ph7_value *pIn,ph7_value *pOut)` |
|      2 |  481 | `{` |
|      - |  482 | `	static const char * const azField[] = {` |
|      - |  483 | `		"dev","ino","mode","nlink","uid","gid","rdev","size",` |
|      - |  484 | `		"atime","mtime","ctime","blksize","blocks"` |
|      - |  485 | `	};` |
|      - |  486 | `	sxu32 i;` |
|     74 |  487 | `	if( pIn == 0 \|\| pOut == 0 ){` |
|    ! 0 |  488 | `		return -1;` |
|      - |  489 | `	}` |
|    958 |  490 | `	for( i = 0 ; i < SX_ARRAYSIZE(azField) ; ++i ){` |
|    890 |  491 | `		ph7_value *pField = ph7_array_fetch(pIn,azField[i],-1);` |
|    890 |  492 | `		if( pField == 0 ){` |
|      - |  493 | `			/* A VFS that does not report this field: php always has all thirteen,` |
|      - |  494 | `			 * so the doubling would silently shift every later index. Hand the` |
|      - |  495 | `			 * caller the named-only array it already had instead. */` |
|      5 |  496 | `			return -1;` |
|      - |  497 | `		}` |
|    886 |  498 | `		ph7_array_add_elem(pOut,0,pField);` |
|    444 |  499 | `	}` |
|    954 |  500 | `	for( i = 0 ; i < SX_ARRAYSIZE(azField) ; ++i ){` |
|    886 |  501 | `		ph7_value *pField = ph7_array_fetch(pIn,azField[i],-1);` |
|    886 |  502 | `		ph7_array_add_strkey_elem(pOut,azField[i],pField);` |
|    444 |  503 | `	}` |
|     70 |  504 | `	return PH7_OK;` |
|     38 |  505 | `}` |
|      - |  506 | `/*` |
|      - |  507 | ` * The thirteen NAMED fields of a stat answer, filled from thirteen values in` |
|      - |  508 | ` * php's own order. Two devices that never had a stat need one: php's memory` |
|      - |  509 | ` * streams (php://memory, php://temp, data://) answer a SYNTHETIC record that` |
|      - |  510 | ` * describes no file at all, and a pipe/socket/standard descriptor answers the` |
|      - |  511 | ` * real fstat() of its handle. Both go through here so the field list -- and` |
|      - |  512 | ` * therefore what PH7_VfsStatDoubleUp() can double -- exists in one place.` |
|      - |  513 | ` */` |
|     48 |  514 | `PH7_PRIVATE int PH7_VfsStatFill(ph7_value *pArray,ph7_value *pWorker,const ph7_int64 *aVal)` |
|      1 |  515 | `{` |
|      - |  516 | `	static const char * const azField[] = {` |
|      - |  517 | `		"dev","ino","mode","nlink","uid","gid","rdev","size",` |
|      - |  518 | `		"atime","mtime","ctime","blksize","blocks"` |
|      - |  519 | `	};` |
|      - |  520 | `	sxu32 i;` |
|     49 |  521 | `	if( pArray == 0 \|\| pWorker == 0 \|\| aVal == 0 ){` |
|    ! 0 |  522 | `		return -1;` |
|      - |  523 | `	}` |
|    673 |  524 | `	for( i = 0 ; i < SX_ARRAYSIZE(azField) ; ++i ){` |
|    625 |  525 | `		ph7_value_int64(pWorker,aVal[i]);` |
|    625 |  526 | `		ph7_array_add_strkey_elem(pArray,azField[i],pWorker); /* Takes its own copy */` |
|    313 |  527 | `	}` |
|     49 |  528 | `	return PH7_OK;` |
|     25 |  529 | `}` |
|      - |  530 | `/*` |
|      - |  531 | ` * The real fstat() of an open descriptor, in the shape above. php's pipe,` |
|      - |  532 | ` * socket and php://stdin\|stdout\|stderr streams all answer exactly this -- the` |
|      - |  533 | ` * same call, so the same platform answers, including the FAILURE that makes` |
|      - |  534 | ` * fstat() report false.` |
|      - |  535 | ` */` |
|    ! 0 |  536 | `PH7_PRIVATE int PH7_VfsStatFromFd(int iFd,ph7_value *pArray,ph7_value *pWorker)` |
|    ! 0 |  537 | `{` |
|      - |  538 | `#ifdef __WINNT__` |
|      - |  539 | `	struct _stat64 st;` |
|    ! 0 |  540 | `	if( iFd < 0 \|\| _fstat64(iFd,&st) != 0 ){` |
|    ! 0 |  541 | `		return -1;` |
|      - |  542 | `	}` |
|      - |  543 | `#else` |
|      - |  544 | `	struct stat st;` |
|    ! 0 |  545 | `	if( iFd < 0 \|\| fstat(iFd,&st) != 0 ){` |
|    ! 0 |  546 | `		return -1;` |
|      - |  547 | `	}` |
|      - |  548 | `#endif` |
|      - |  549 | `	{` |
|      - |  550 | `		ph7_int64 aVal[13];` |
|    ! 0 |  551 | `		aVal[0]  = (ph7_int64)st.st_dev;` |
|    ! 0 |  552 | `		aVal[1]  = (ph7_int64)st.st_ino;` |
|    ! 0 |  553 | `		aVal[2]  = (ph7_int64)st.st_mode;` |
|    ! 0 |  554 | `		aVal[3]  = (ph7_int64)st.st_nlink;` |
|    ! 0 |  555 | `		aVal[4]  = (ph7_int64)st.st_uid;` |
|    ! 0 |  556 | `		aVal[5]  = (ph7_int64)st.st_gid;` |
|    ! 0 |  557 | `		aVal[6]  = (ph7_int64)st.st_rdev;` |
|    ! 0 |  558 | `		aVal[7]  = (ph7_int64)st.st_size;` |
|    ! 0 |  559 | `		aVal[8]  = (ph7_int64)st.st_atime;` |
|    ! 0 |  560 | `		aVal[9]  = (ph7_int64)st.st_mtime;` |
|    ! 0 |  561 | `		aVal[10] = (ph7_int64)st.st_ctime;` |
|      - |  562 | `#ifdef __WINNT__` |
|      - |  563 | `		/* Windows has neither field, and php reports -1 for both there -- on` |
|      - |  564 | `		 * EVERY stream, a plain file included (read back from php 8.5.8 on the` |
|      - |  565 | `		 * gate guest). */` |
|    ! 0 |  566 | `		aVal[11] = -1;` |
|    ! 0 |  567 | `		aVal[12] = -1;` |
|      - |  568 | `#else` |
|    ! 0 |  569 | `		aVal[11] = (ph7_int64)st.st_blksize;` |
|    ! 0 |  570 | `		aVal[12] = (ph7_int64)st.st_blocks;` |
|      - |  571 | `#endif` |
|    ! 0 |  572 | `		return PH7_VfsStatFill(pArray,pWorker,aVal);` |
|      - |  573 | `	}` |
|    ! 0 |  574 | `}` |
|      - |  575 | `/* Defined with the path operations below, and used by every one of them. */` |
|      - |  576 | `static int VfsBuiltinWrapperRefuses(ph7_context *pCtx,const char *zPath,const char *zDest,int eOp);` |
|      - |  577 | `#define VFS_POP_UNLINK 0` |
|      - |  578 | `#define VFS_POP_RENAME 1` |
|      - |  579 | `#define VFS_POP_MKDIR  2   /* mkdir and rmdir: false, and nothing said -- but they` |
|      - |  580 | `                            * are two different operations to a wrapper that` |
|      - |  581 | `                            * IMPLEMENTS them, which ext/phar does */` |
|      - |  582 | `#define VFS_POP_CHMOD  3` |
|      - |  583 | `#define VFS_POP_RMDIR  4` |
|      - |  584 | `/*` |
|      - |  585 | ` * php's WRITE door for a path a userland wrapper owns: unlink(), rename(), mkdir(),` |
|      - |  586 | ` * rmdir(), and the stream_metadata() that touch(), chmod(), chown() and chgrp() all` |
|      - |  587 | ``  * become. PHL sent every one of them to the OS instead -- so `unlink('vfs://root/f')` `` |
|      - |  588 | `` * reported `No such file or directory` about a path the OS had never heard of, and`` |
|      - |  589 | ` * the wrapper was never told -- which is the second half of what a test suite that` |
|      - |  590 | `` * fakes a filesystem does with one. (The `?resource $context` these builtins already`` |
|      - |  591 | ` * screened is what php sets on the serving instance, so it is handed over too.)` |
|      - |  592 | ` *` |
|      - |  593 | ` * Answers 1 when a wrapper owned the path and the result is set; 0 when none did and` |
|      - |  594 | ` * the caller carries on to the VFS.` |
|      - |  595 | ` */` |
|  52506 |  596 | `static int VfsUserWrite(ph7_context *pCtx,const char *zPath,const char *zMethod,` |
|      - |  597 | `	void *pStreamCtx,ph7_value **apExtra,int nExtra)` |
|      5 |  598 | `{` |
|  52511 |  599 | `	int bAnswer = 0;` |
|  52511 |  600 | `	int rc = PH7_StreamUserPathOp(pCtx,zPath,zMethod,pStreamCtx,apExtra,nExtra,&bAnswer);` |
|  52511 |  601 | `	if( rc == PHL_URLSTAT_NOWRAP ){` |
|  52439 |  602 | `		return 0;` |
|      - |  603 | `	}` |
|      - |  604 | `	/* A wrapper that declined, one with no such method (php has already said so), and` |
|      - |  605 | `	 * one that threw all answer the same false; the throw is its own report. */` |
|     74 |  606 | `	ph7_result_bool(pCtx,rc == PHL_URLSTAT_OK ? bAnswer : 0);` |
|     74 |  607 | `	return 1;` |
|  26232 |  608 | `}` |
|      - |  609 | `/*` |
|      - |  610 | ` * ---------------------------------------------------------------------------` |
|      - |  611 | ` * The stat family over a path a USERLAND stream wrapper owns.` |
|      - |  612 | ` *` |
|      - |  613 | `` * php routes every member of the family through one door -- `php_stat`, over`` |
|      - |  614 | `` * `php_stream_url_stat_path` -- so a registered wrapper answers `file_exists()`,`` |
|      - |  615 | `` * `is_dir()`, `filesize()`, `stat()` and the rest through its own `url_stat()`.`` |
|      - |  616 | ` * PHL asked the OS VFS straight from each builtin, so a wrapper was reachable` |
|      - |  617 | ` * through fopen() and invisible to everything that asks ABOUT a name: vfsStream --` |
|      - |  618 | ` * what every test suite that fakes a filesystem uses -- could not report the size` |
|      - |  619 | `` * of a virtual file, and `file_exists('vfs://root/t.txt')` was false.`` |
|      - |  620 | ` *` |
|      - |  621 | ` * Everything php's door decides is decided here, once, for all of them:` |
|      - |  622 | ` *  - the FLAGS the wrapper is handed: LINK for the three lstat readers, QUIET for` |
|      - |  623 | ` *    the seven existence/access questions, and NOCACHE always (php's own one-entry` |
|      - |  624 | ` *    stat cache lives above this door, so it asks for a fresh answer every time);` |
|      - |  625 | ` *  - what a FAILURE says: nothing at all for a quiet ask -- php answers a plain` |
|      - |  626 | `` *    false there -- and `%s(): stat failed for %s` / `Lstat failed for` for the`` |
|      - |  627 | ` *    rest;` |
|      - |  628 | ` *  - how the thirteen fields answer each question: the mode's type bits, and php's` |
|      - |  629 | ` *    owner/group/other access masks.` |
|      - |  630 | ` * ---------------------------------------------------------------------------` |
|      - |  631 | ` */` |
|      - |  632 | `/* The seven questions php asks QUIETLY, and reports as a bare false. */` |
|  27145 |  633 | `static int VfsAskIsQuiet(int eAsk)` |
|      5 |  634 | `{` |
|  27150 |  635 | `	return eAsk <= PH7_STAT_ASK_IS_X;` |
|      5 |  636 | `}` |
|      - |  637 | ``/* The three php asks with LSTAT, and words a failure as `Lstat failed for`. */`` |
|  27119 |  638 | `static int VfsAskIsLink(int eAsk)` |
|      5 |  639 | `{` |
|  40596 |  640 | `	return eAsk == PH7_STAT_ASK_IS_LINK \|\| eAsk == PH7_STAT_ASK_TYPE` |
|  40655 |  641 | `	    \|\| eAsk == PH7_STAT_ASK_LSTAT;` |
|      5 |  642 | `}` |
|      - |  643 | `/*` |
|      - |  644 | ` * php's rmask/wmask/xmask for a stat record that did not come from a plain file.` |
|      - |  645 | ` * It starts at the OTHER bits and moves to the OWNER bits when the record's uid is` |
|      - |  646 | ` * the process's, or to the GROUP bits when its gid is the process's or one of its` |
|      - |  647 | ` * supplementary groups. On Windows php makes none of those comparisons: it seeds` |
|      - |  648 | `` * the masks with `S_IREAD/S_IWRITE/S_IEXEC` -- the OWNER bits -- and reads those`` |
|      - |  649 | ` * whatever the record says (read back from php 8.5 on the gate guest, where a` |
|      - |  650 | ` * record with uid 65534 and mode 0644 still answered readable AND writable).` |
|      - |  651 | ` */` |
|     48 |  652 | `PH7_PRIVATE void PH7_VfsStatAccessMasks(ph7_context *pCtx,ph7_int64 nUid,ph7_int64 nGid,` |
|      - |  653 | `	int *pR,int *pW,int *pX)` |
|      1 |  654 | `{` |
|      - |  655 | `#ifdef __WINNT__` |
|      - |  656 | `	SXUNUSED(pCtx); SXUNUSED(nUid); SXUNUSED(nGid);` |
|      1 |  657 | `	*pR = 0400; *pW = 0200; *pX = 0100;` |
|      - |  658 | `#else` |
|     48 |  659 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     48 |  660 | `	*pR = 0004; *pW = 0002; *pX = 0001;` |
|     48 |  661 | `	if( pVfs && pVfs->xUid && (ph7_int64)pVfs->xUid() == nUid ){` |
|     30 |  662 | `		*pR = 0400; *pW = 0200; *pX = 0100;` |
|     30 |  663 | `		return;` |
|      - |  664 | `	}` |
|     18 |  665 | `	if( pVfs && pVfs->xGid && (ph7_int64)pVfs->xGid() == nGid ){` |
|    ! 0 |  666 | `		*pR = 0040; *pW = 0020; *pX = 0010;` |
|    ! 0 |  667 | `		return;` |
|      - |  668 | `	}` |
|      - |  669 | `#ifdef __UNIXES__` |
|      - |  670 | `	{` |
|      - |  671 | `		/* php's last arm: the record's group may be one the process merely` |
|      - |  672 | `		 * belongs to. */` |
|     18 |  673 | `		int nGroup = getgroups(0,NULL);` |
|     18 |  674 | `		if( nGroup > 0 ){` |
|     27 |  675 | `			gid_t *aGid = (gid_t *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     18 |  676 | `				(sxu32)nGroup * (sxu32)sizeof(gid_t));` |
|     18 |  677 | `			if( aGid ){` |
|     18 |  678 | `				int n = getgroups(nGroup,aGid);` |
|      - |  679 | `				int i;` |
|    207 |  680 | `				for( i = 0 ; i < n ; ++i ){` |
|    189 |  681 | `					if( (ph7_int64)aGid[i] == nGid ){` |
|    ! 0 |  682 | `						*pR = 0040; *pW = 0020; *pX = 0010;` |
|    ! 0 |  683 | `						break;` |
|      - |  684 | `					}` |
|    144 |  685 | `				}` |
|     18 |  686 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,aGid);` |
|      9 |  687 | `			}` |
|      9 |  688 | `		}` |
|      - |  689 | `	}` |
|      - |  690 | `#endif /* __UNIXES__ */` |
|      - |  691 | `#endif /* __WINNT__ */` |
|     25 |  692 | `}` |
|      - |  693 | `/*` |
|      - |  694 | ` * Ask the wrapper that owns zPath, with the flags this member of the family uses.` |
|      - |  695 | ` * PHL_URLSTAT_NOWRAP means nothing here owns the path and the caller carries on to` |
|      - |  696 | ` * the VFS.` |
|      - |  697 | ` */` |
|  27111 |  698 | `PH7_PRIVATE int PH7_VfsUserStatFields(ph7_context *pCtx,const char *zPath,int eAsk,` |
|      - |  699 | `	ph7_int64 *aVal)` |
|      5 |  700 | `{` |
|  27116 |  701 | `	int iFlags = PH7_URL_STAT_NOCACHE;` |
|      - |  702 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  703 | `	/* An ARCHIVE answers for its own entries. php gives every wrapper a` |
|      - |  704 | `	 * url_stat handler and routes the whole stat family through it, which is` |
|      - |  705 | ``	 * what makes `file_exists('phar://x.phar/f')` and `is_dir()` true for a name`` |
|      - |  706 | `	 * that exists nowhere on disk. This engine has the door for a USERLAND` |
|      - |  707 | `	 * wrapper (below) and this one built-in wrapper that needs it. */` |
|  27116 |  708 | `	if( zPath && SyStrnicmp(zPath,"phar://",sizeof("phar://")-1) == 0 ){` |
|     16 |  709 | `		return PH7_PharUrlStat(pCtx->pVm,&zPath[sizeof("phar://")-1],aVal) == 0` |
|      8 |  710 | `			? PHL_URLSTAT_OK : PHL_URLSTAT_FAIL;` |
|      - |  711 | `	}` |
|      - |  712 | `#endif` |
|  27100 |  713 | `	if( VfsAskIsLink(eAsk) ){` |
|    126 |  714 | `		iFlags \|= PH7_URL_STAT_LINK;` |
|     62 |  715 | `	}` |
|  27100 |  716 | `	if( VfsAskIsQuiet(eAsk) ){` |
|  26644 |  717 | `		iFlags \|= PH7_URL_STAT_QUIET;` |
|  13315 |  718 | `	}` |
|  27100 |  719 | `	return PH7_StreamUserUrlStat(pCtx,zPath,iFlags,aVal);` |
|  13556 |  720 | `}` |
|      - |  721 | `/*` |
|      - |  722 | ` * Answer one question from a filled record. Shared with SplFileInfo, whose` |
|      - |  723 | ` * accessors ask exactly the same things of exactly the same thirteen fields.` |
|      - |  724 | ` */` |
|    266 |  725 | `PH7_PRIVATE void PH7_VfsUserStatResult(ph7_context *pCtx,int eAsk,const ph7_int64 *aVal)` |
|      1 |  726 | `{` |
|    267 |  727 | `	ph7_int64 nMode = aVal[2];` |
|    267 |  728 | `	switch( eAsk ){` |
|      8 |  729 | `	case PH7_STAT_ASK_EXISTS:` |
|     17 |  730 | `		ph7_result_bool(pCtx,1);` |
|     17 |  731 | `		break;` |
|     12 |  732 | `	case PH7_STAT_ASK_IS_FILE:` |
|     25 |  733 | `		ph7_result_bool(pCtx,(nMode & PH7_S_IFMT) == PH7_S_IFREG);` |
|     25 |  734 | `		break;` |
|     13 |  735 | `	case PH7_STAT_ASK_IS_DIR:` |
|     27 |  736 | `		ph7_result_bool(pCtx,(nMode & PH7_S_IFMT) == PH7_S_IFDIR);` |
|     27 |  737 | `		break;` |
|     11 |  738 | `	case PH7_STAT_ASK_IS_LINK:` |
|     23 |  739 | `		ph7_result_bool(pCtx,(nMode & PH7_S_IFMT) == PH7_S_IFLNK);` |
|     23 |  740 | `		break;` |
|     24 |  741 | `	case PH7_STAT_ASK_IS_R:` |
|      - |  742 | `	case PH7_STAT_ASK_IS_W:` |
|      - |  743 | `	case PH7_STAT_ASK_IS_X: {` |
|      - |  744 | `		int iR,iW,iX,iMask;` |
|     49 |  745 | `		PH7_VfsStatAccessMasks(pCtx,aVal[4],aVal[5],&iR,&iW,&iX);` |
|     49 |  746 | `		iMask = eAsk == PH7_STAT_ASK_IS_R ? iR : (eAsk == PH7_STAT_ASK_IS_W ? iW : iX);` |
|     49 |  747 | `		ph7_result_bool(pCtx,(nMode & iMask) != 0);` |
|     49 |  748 | `		break; }` |
|     19 |  749 | `	case PH7_STAT_ASK_SIZE:  ph7_result_int64(pCtx,aVal[7]);  break;` |
|     15 |  750 | `	case PH7_STAT_ASK_ATIME: ph7_result_int64(pCtx,aVal[8]);  break;` |
|     15 |  751 | `	case PH7_STAT_ASK_MTIME: ph7_result_int64(pCtx,aVal[9]);  break;` |
|     15 |  752 | `	case PH7_STAT_ASK_CTIME: ph7_result_int64(pCtx,aVal[10]); break;` |
|      7 |  753 | `	case PH7_STAT_ASK_OWNER: ph7_result_int64(pCtx,aVal[4]);  break;` |
|      7 |  754 | `	case PH7_STAT_ASK_GROUP: ph7_result_int64(pCtx,aVal[5]);  break;` |
|     15 |  755 | `	case PH7_STAT_ASK_INODE: ph7_result_int64(pCtx,aVal[1]);  break;` |
|     15 |  756 | `	case PH7_STAT_ASK_PERMS: ph7_result_int64(pCtx,nMode);    break;` |
|     11 |  757 | `	case PH7_STAT_ASK_TYPE: {` |
|      - |  758 | `		/* php's FS_TYPE: a symlink first (this ask lstats), then the S_IFMT` |
|      - |  759 | `		 * switch, then a NOTICE naming the type bits it did not recognise.` |
|      - |  760 | `		 * S_IFSOCK is the one arm php's Windows build does not compile, so a` |
|      - |  761 | `		 * socket record answers "unknown" there -- read back from the gate` |
|      - |  762 | `		 * guest's php 8.5. */` |
|     23 |  763 | `		const char *zType = 0;` |
|     23 |  764 | `		switch( (int)(nMode & PH7_S_IFMT) ){` |
|      5 |  765 | `		case PH7_S_IFLNK:  zType = "link";   break;` |
|    ! 0 |  766 | `		case PH7_S_IFIFO:  zType = "fifo";   break;` |
|    ! 0 |  767 | `		case PH7_S_IFCHR:  zType = "char";   break;` |
|      5 |  768 | `		case PH7_S_IFDIR:  zType = "dir";    break;` |
|    ! 0 |  769 | `		case PH7_S_IFBLK:  zType = "block";  break;` |
|     11 |  770 | `		case PH7_S_IFREG:  zType = "file";   break;` |
|      - |  771 | `#ifndef __WINNT__` |
|    ! 0 |  772 | `		case PH7_S_IFSOCK: zType = "socket"; break;` |
|      - |  773 | `#endif` |
|      4 |  774 | `		default: break;` |
|      - |  775 | `		}` |
|     23 |  776 | `		if( zType == 0 ){` |
|      7 |  777 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      2 |  778 | `				"Unknown file type (%d)",(int)(nMode & PH7_S_IFMT));` |
|      5 |  779 | `			zType = "unknown";` |
|      2 |  780 | `		}` |
|     23 |  781 | `		ph7_result_string(pCtx,zType,-1);` |
|     23 |  782 | `		break; }` |
|      4 |  783 | `	default: {` |
|      - |  784 | `		/* stat()/lstat(): php's thirteen fields numbered 0..12 and then named. */` |
|      9 |  785 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|      9 |  786 | `		ph7_value *pWorker = ph7_context_new_scalar(pCtx);` |
|      9 |  787 | `		ph7_value *pFull = ph7_context_new_array(pCtx);` |
|      9 |  788 | `		if( pArray == 0 \|\| pWorker == 0 ){` |
|    ! 0 |  789 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  790 | `			break;` |
|      - |  791 | `		}` |
|      9 |  792 | `		PH7_VfsStatFill(pArray,pWorker,aVal);` |
|      9 |  793 | `		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){` |
|      9 |  794 | `			ph7_result_value(pCtx,pFull);` |
|      5 |  795 | `		}else{` |
|    ! 0 |  796 | `			ph7_result_value(pCtx,pArray);` |
|      - |  797 | `		}` |
|      8 |  798 | `		break; }` |
|      - |  799 | `	}` |
|    267 |  800 | `}` |
|      - |  801 | `/*` |
|      - |  802 | ` * The one line every member of the family carries: if a userland wrapper owns this` |
|      - |  803 | ` * path, answer from it and stop. Returns 0 when nothing does.` |
|      - |  804 | ` */` |
|  26892 |  805 | `static int VfsUserStat(ph7_context *pCtx,const char *zPath,int eAsk)` |
|      5 |  806 | `{` |
|      - |  807 | `	ph7_int64 aVal[13];` |
|      - |  808 | `	SyBlob sPath;` |
|      - |  809 | `	int rc;` |
|  26897 |  810 | `	if( zPath == 0 ){` |
|    ! 0 |  811 | `		return 0;` |
|      - |  812 | `	}` |
|      - |  813 | `	/* The wrapper is PHP code, and running some can move the argument slot zPath` |
|      - |  814 | `	 * points into -- so the copy the failure message needs is taken first. */` |
|  26897 |  815 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|  26897 |  816 | `	SyBlobAppend(&sPath,zPath,(sxu32)SyStrlen(zPath));` |
|  26897 |  817 | `	SyBlobAppend(&sPath,"",1); /* NUL, for the %s below */` |
|  26897 |  818 | `	rc = PH7_VfsUserStatFields(pCtx,zPath,eAsk,aVal);` |
|  26897 |  819 | `	if( rc == PHL_URLSTAT_NOWRAP ){` |
|      - |  820 | `		/* php resolves the wrapper for a stat the same way it resolves one for an` |
|      - |  821 | `		 * open (php_stream_locate_url_wrapper), so a scheme nothing is registered` |
|      - |  822 | `		 * under is NAMED here too -- and then forgotten, the whole uri going on to` |
|      - |  823 | `		 * the plain-files path below. The family answered that path's verdict in` |
|      - |  824 | ``		 * silence, so `file_exists('zzz://hit.php')` was TRUE with nothing said.`` |
|      - |  825 | `		 *` |
|      - |  826 | ``		 * NOWRAP is "no wrapper of its OWN answered", which a plain `file://` name`` |
|      - |  827 | `		 * reaches too -- so the sentence is left to the same screen every open` |
|      - |  828 | `		 * door uses, which says nothing for a scheme somebody IS registered under.` |
|      - |  829 | `		 * The device it resolves is the plain-files one this path already takes. */` |
|      - |  830 | `		{` |
|  26611 |  831 | `			const char *zProbe = (const char *)SyBlobData(&sPath);` |
|  26611 |  832 | `			PH7_VfsStreamDeviceOrFile(pCtx,&zProbe,(int)SyStrlen(zProbe));` |
|      - |  833 | `		}` |
|  26611 |  834 | `		SyBlobRelease(&sPath);` |
|  26611 |  835 | `		return 0;` |
|      - |  836 | `	}` |
|    287 |  837 | `	if( rc == PHL_URLSTAT_OK ){` |
|    237 |  838 | `		PH7_VfsUserStatResult(pCtx,eAsk,aVal);` |
|    119 |  839 | `	}else{` |
|     51 |  840 | `		if( !VfsAskIsQuiet(eAsk) ){` |
|     25 |  841 | `			VfsThrowStatWarning(pCtx,(const char *)SyBlobData(&sPath),VfsAskIsLink(eAsk));` |
|     12 |  842 | `		}` |
|     51 |  843 | `		ph7_result_bool(pCtx,0);` |
|      - |  844 | `	}` |
|    287 |  845 | `	SyBlobRelease(&sPath);` |
|    287 |  846 | `	return 1;` |
|  13446 |  847 | `}` |
|      - |  848 | `/*` |
|      - |  849 | ` * Can this path be stat'ed at all? The three TIME readers report a failure as -1,` |
|      - |  850 | ` * which is also a legitimate timestamp (a file stamped in the last second before` |
|      - |  851 | ` * the epoch), so the failure verdict is asked of the VFS separately rather than` |
|      - |  852 | ` * read off the value -- one extra call, and only on the negative branch.` |
|      - |  853 | ` */` |
|    232 |  854 | `static int VfsPathStatable(ph7_vfs *pVfs,const char *zPath)` |
|      4 |  855 | `{` |
|    236 |  856 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|    ! 0 |  857 | `		return 0;` |
|      - |  858 | `	}` |
|    236 |  859 | `	return pVfs->xFileExists(zPath) == PH7_OK;` |
|    148 |  860 | `}` |
|      - |  861 | `/*` |
|      - |  862 | `` * php declares every path parameter in this file `string`, and its ZPP converts an`` |
|      - |  863 | ` * OBJECT with __toString before the C body runs. PHL's guards asked` |
|      - |  864 | ` * ph7_value_is_string, which is false for such an object, so each of these builtins` |
|      - |  865 | `` * silently answered its miss value for one -- `is_dir($it->current())` answered FALSE`` |
|      - |  866 | ` * for a directory, which is how phpcs walked three top-level directories and found` |
|      - |  867 | ` * 16 of a project's 711 files without reporting anything wrong. SplFileInfo is the` |
|      - |  868 | ` * object every real program passes here (RecursiveDirectoryIterator yields it).` |
|      - |  869 | ` *` |
|      - |  870 | ` * The conversion itself is already in ph7_value_to_string(); only the screen in front` |
|      - |  871 | ` * of it was wrong.` |
|      - |  872 | ` */` |
| 101944 |  873 | `static int VfsPathArgUsable(ph7_value *pVal)` |
|      5 |  874 | `{` |
| 101949 |  875 | `	if( pVal == 0 ){` |
|    ! 0 |  876 | `		return 0;` |
|      - |  877 | `	}` |
| 101949 |  878 | `	if( ph7_value_is_string(pVal) ){` |
| 101949 |  879 | `		return 1;` |
|      - |  880 | `	}` |
|    ! 0 |  881 | `	return ph7_value_is_object(pVal) && !PH7_MemObjIsNotStringable(pVal);` |
|  50943 |  882 | `}` |
|      - |  883 |  |
|      - |  884 | `/*` |
|      - |  885 | ` * bool chdir(string $directory)` |
|      - |  886 | ` *  Change the current directory.` |
|      - |  887 | ` * Parameters` |
|      - |  888 | ` *  $directory` |
|      - |  889 | ` *   The new current directory` |
|      - |  890 | ` * Return` |
|      - |  891 | ` *  TRUE on success or FALSE on failure.` |
|      - |  892 | ` */` |
|  17590 |  893 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  894 | `{` |
|      - |  895 | `	const char *zPath;` |
|      - |  896 | `	ph7_vfs *pVfs;` |
|      - |  897 | `	int rc;` |
|      - |  898 | `	/* Only the ARITY is checked here: php coerces a scalar $directory to string,` |
|      - |  899 | `	 * so chdir(123) attempts "123" and warns that it does not exist. Requiring a` |
|      - |  900 | `	 * string outright made that call return FALSE silently, with no diagnostic. */` |
|  17595 |  901 | `	if( nArg < 1 ){` |
|      - |  902 | `		/* Missing argument,return FALSE */` |
|    ! 0 |  903 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  904 | `		return PH7_OK;` |
|      - |  905 | `	}` |
|      - |  906 | `	/* Point to the underlying vfs */` |
|  17595 |  907 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  17595 |  908 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |  909 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  910 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  911 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  912 | `			ph7_function_name(pCtx)` |
|      - |  913 | `			);` |
|    ! 0 |  914 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  915 | `		return PH7_OK;` |
|      - |  916 | `	}` |
|      - |  917 | `	/* Point to the desired directory */` |
|  17595 |  918 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  919 | `	/* Perform the requested operation */` |
|  17595 |  920 | `	errno = 0;` |
|  17595 |  921 | `	rc = pVfs->xChdir(zPath);` |
|  17595 |  922 | `	if( rc != PH7_OK ){` |
|      - |  923 | `		/* chdir has its own php shape: no path, and the errno spelled out. */` |
|      8 |  924 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|      4 |  925 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|      2 |  926 | `	}` |
|      - |  927 | `	/* IO return value */` |
|  17595 |  928 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  17595 |  929 | `	return PH7_OK;` |
|   8795 |  930 | `}` |
|      - |  931 | `/*` |
|      - |  932 | ` * bool chroot(string $directory)` |
|      - |  933 | ` *  Change the root directory.` |
|      - |  934 | ` * Parameters` |
|      - |  935 | ` *  $directory` |
|      - |  936 | ` *   The path to change the root directory to` |
|      - |  937 | ` * Return` |
|      - |  938 | ` *  TRUE on success or FALSE on failure.` |
|      - |  939 | ` *` |
|      - |  940 | ` * POSIX only, like php's: the registration below is guarded the same way, and an` |
|      - |  941 | ` * unreferenced static is an error under the Windows build's /W4 /WX.` |
|      - |  942 | ` */` |
|      - |  943 | `#ifndef __WINNT__` |
|    ! 0 |  944 | `static int PH7_vfs_chroot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - |  945 | `{` |
|      - |  946 | `	const char *zPath;` |
|      - |  947 | `	ph7_vfs *pVfs;` |
|      - |  948 | `	int rc;` |
|    ! 0 |  949 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - |  950 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  951 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  952 | `		return PH7_OK;` |
|      - |  953 | `	}` |
|      - |  954 | `	/* Point to the underlying vfs */` |
|    ! 0 |  955 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    ! 0 |  956 | `	if( pVfs == 0 \|\| pVfs->xChroot == 0 ){` |
|      - |  957 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  958 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  959 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  960 | `			ph7_function_name(pCtx)` |
|      - |  961 | `			);` |
|    ! 0 |  962 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  963 | `		return PH7_OK;` |
|      - |  964 | `	}` |
|      - |  965 | `	/* Point to the desired directory */` |
|    ! 0 |  966 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  967 | `	/* Perform the requested operation */` |
|    ! 0 |  968 | `	errno = 0;` |
|    ! 0 |  969 | `	rc = pVfs->xChroot(zPath);` |
|    ! 0 |  970 | `	if( rc != PH7_OK ){` |
|      - |  971 | `		/* php's own wording, and the failure a script actually meets: chroot(2)` |
|      - |  972 | `		 * needs privilege, so an ordinary process gets EPERM. PHL answered the` |
|      - |  973 | `		 * bare false in SILENCE — a refused chroot() and a chroot() that did` |
|      - |  974 | `		 * nothing looked the same to the caller. */` |
|    ! 0 |  975 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|    ! 0 |  976 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|    ! 0 |  977 | `	}` |
|      - |  978 | `	/* IO return value */` |
|    ! 0 |  979 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    ! 0 |  980 | `	return PH7_OK;` |
|    ! 0 |  981 | `}` |
|      - |  982 | `#endif /* __WINNT__ */` |
|      - |  983 | `/*` |
|      - |  984 | ` * string getcwd(void)` |
|      - |  985 | ` *  Gets the current working directory.` |
|      - |  986 | ` * Parameters` |
|      - |  987 | ` *  None` |
|      - |  988 | ` * Return` |
|      - |  989 | ` *  Returns the current working directory on success, or FALSE on failure.` |
|      - |  990 | ` */` |
|     35 |  991 | `static int PH7_vfs_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  992 | `{` |
|      - |  993 | `	ph7_vfs *pVfs;` |
|      - |  994 | `	int rc;` |
|      - |  995 | `	/* Point to the underlying vfs */` |
|     40 |  996 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     40 |  997 | `	if( pVfs == 0 \|\| pVfs->xGetcwd == 0 ){` |
|    ! 0 |  998 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 |  999 | `		SXUNUSED(apArg);` |
|      - | 1000 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1001 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1002 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1003 | `			ph7_function_name(pCtx)` |
|      - | 1004 | `			);` |
|    ! 0 | 1005 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1006 | `		return PH7_OK;` |
|      - | 1007 | `	}` |
|     40 | 1008 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1009 | `	/* Perform the requested operation */` |
|     40 | 1010 | `	rc = pVfs->xGetcwd(pCtx);` |
|     40 | 1011 | `	if( rc != PH7_OK ){` |
|      - | 1012 | `		/* Error,return FALSE */` |
|    ! 0 | 1013 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1014 | `	}` |
|     40 | 1015 | `	return PH7_OK;` |
|     21 | 1016 | `}` |
|      - | 1017 | `/*` |
|      - | 1018 | ` * bool rmdir(string $directory)` |
|      - | 1019 | ` *  Removes directory.` |
|      - | 1020 | ` * Parameters` |
|      - | 1021 | ` *  $directory` |
|      - | 1022 | ` *   The path to the directory` |
|      - | 1023 | ` * Return` |
|      - | 1024 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1025 | ` */` |
|    416 | 1026 | `static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1027 | `{` |
|      - | 1028 | `	const char *zPath;` |
|      - | 1029 | `	ph7_vfs *pVfs;` |
|      - | 1030 | `	phl_stream_ctx *pStreamCtx;` |
|    421 | 1031 | `	int rc,bThrew = 0;` |
|    421 | 1032 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 1033 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1034 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1035 | `		return PH7_OK;` |
|      - | 1036 | `	}` |
|      - | 1037 | ``	/* php's `?resource $context`, refused when it is a resource of another kind,`` |
|      - | 1038 | `	 * and SET on the wrapper instance the write door below dispatches to. */` |
|    421 | 1039 | `	pStreamCtx = PH7_StreamCtxFromArg(pCtx,nArg,apArg,1,"$context",0,&bThrew);` |
|    421 | 1040 | `	if( bThrew ){` |
|      3 | 1041 | `		return PH7_OK;` |
|      - | 1042 | `	}` |
|      - | 1043 | `	/* php's rmdir() hands the wrapper the url and its own $options -- always` |
|      - | 1044 | `	 * STREAM_REPORT_ERRORS, the bit that says a diagnostic is wanted. */` |
|      - | 1045 | `	{` |
|      - | 1046 | `		ph7_value sOpt;` |
|      - | 1047 | `		ph7_value *apExtra[1];` |
|      - | 1048 | `		int bDone;` |
|    419 | 1049 | `		PH7_MemObjInit(pCtx->pVm,&sOpt);` |
|    419 | 1050 | `		ph7_value_int(&sOpt,PH7_STREAM_REPORT_ERRORS);` |
|    419 | 1051 | `		apExtra[0] = &sOpt;` |
|    618 | 1052 | `		bDone = VfsUserWrite(pCtx,ph7_value_to_string(apArg[0],0),"rmdir",` |
|    199 | 1053 | `			(void *)pStreamCtx,apExtra,1);` |
|    419 | 1054 | `		PH7_MemObjRelease(&sOpt);` |
|    419 | 1055 | `		if( bDone ){` |
|      7 | 1056 | `			return PH7_OK;` |
|      - | 1057 | `		}` |
|      - | 1058 | `	}` |
|    413 | 1059 | `	if( VfsBuiltinWrapperRefuses(pCtx,ph7_value_to_string(apArg[0],0),0,VFS_POP_RMDIR) ){` |
|     23 | 1060 | `		return PH7_OK;` |
|      - | 1061 | `	}` |
|      - | 1062 | `	/* Point to the underlying vfs */` |
|    391 | 1063 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    391 | 1064 | `	if( pVfs == 0 \|\| pVfs->xRmdir == 0 ){` |
|      - | 1065 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1066 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1067 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1068 | `			ph7_function_name(pCtx)` |
|      - | 1069 | `			);` |
|    ! 0 | 1070 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1071 | `		return PH7_OK;` |
|      - | 1072 | `	}` |
|      - | 1073 | `	/* Point to the desired directory */` |
|    391 | 1074 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1075 | `	/* Perform the requested operation */` |
|    391 | 1076 | `	errno = 0;` |
|    391 | 1077 | `	rc = pVfs->xRmdir(zPath);` |
|    391 | 1078 | `	if( rc != PH7_OK ){` |
|     33 | 1079 | `		VfsThrowSysWarning(pCtx,zPath);` |
|     14 | 1080 | `	}` |
|      - | 1081 | `	/* IO return value */` |
|    391 | 1082 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    391 | 1083 | `	return PH7_OK;` |
|    205 | 1084 | `}` |
|      - | 1085 | `/*` |
|      - | 1086 | ` * bool is_dir(string $filename)` |
|      - | 1087 | ` *  Tells whether the given filename is a directory.` |
|      - | 1088 | ` * Parameters` |
|      - | 1089 | ` *  $filename` |
|      - | 1090 | ` *   Path to the file.` |
|      - | 1091 | ` * Return` |
|      - | 1092 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1093 | ` */` |
|  14714 | 1094 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1095 | `{` |
|      - | 1096 | `	const char *zPath;` |
|      - | 1097 | `	ph7_vfs *pVfs;` |
|      - | 1098 | `	int rc;` |
|  14719 | 1099 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 1100 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1101 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1102 | `		return PH7_OK;` |
|      - | 1103 | `	}` |
|      - | 1104 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 1105 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|  14719 | 1106 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_IS_DIR) ){` |
|     27 | 1107 | `		return PH7_OK;` |
|      - | 1108 | `	}` |
|  14693 | 1109 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  14693 | 1110 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - | 1111 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1112 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1113 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1114 | `			ph7_function_name(pCtx)` |
|      - | 1115 | `			);` |
|    ! 0 | 1116 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1117 | `		return PH7_OK;` |
|      - | 1118 | `	}` |
|      - | 1119 | `	/* Point to the desired directory */` |
|  14693 | 1120 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1121 | `	/* Perform the requested operation */` |
|  14693 | 1122 | `	rc = pVfs->xIsdir(zPath);` |
|      - | 1123 | `	/* IO return value */` |
|  14693 | 1124 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  14693 | 1125 | `	return PH7_OK;` |
|   7359 | 1126 | `}` |
|      - | 1127 | `/*` |
|      - | 1128 | ` * bool mkdir(string $pathname[,int $mode = 0777 [,bool $recursive = false])` |
|      - | 1129 | ` *  Make a directory.` |
|      - | 1130 | ` * Parameters` |
|      - | 1131 | ` *  $pathname` |
|      - | 1132 | ` *   The directory path.` |
|      - | 1133 | ` * $mode` |
|      - | 1134 | ` *  The mode is 0777 by default, which means the widest possible access.` |
|      - | 1135 | ` *  Note:` |
|      - | 1136 | ` *   mode is ignored on Windows.` |
|      - | 1137 | ` *   Note that you probably want to specify the mode as an octal number, which means` |
|      - | 1138 | ` *   it should have a leading zero. The mode is also modified by the current umask` |
|      - | 1139 | ` *   which you can change using umask().` |
|      - | 1140 | ` * $recursive` |
|      - | 1141 | ` *  Allows the creation of nested directories specified in the pathname.` |
|      - | 1142 | ` *  Defaults to FALSE. (Not used)` |
|      - | 1143 | ` * Return` |
|      - | 1144 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1145 | ` */` |
|      - | 1146 | `/*` |
|      - | 1147 | ` * A prefix the recursive mkdir must not try to CREATE: it names a volume rather` |
|      - | 1148 | ` * than a directory. POSIX has none of these (the leading "/" is never a prefix` |
|      - | 1149 | ` * here, since the walk starts one byte in).` |
|      - | 1150 | ` */` |
|    224 | 1151 | `static int VfsMkdirVolumePrefix(const char *z,int n)` |
|      3 | 1152 | `{` |
|      - | 1153 | `#ifdef __WINNT__` |
|      3 | 1154 | `	int i,nSep = 0;` |
|      3 | 1155 | `	if( n < 1 ){` |
|    ! 0 | 1156 | `		return 1;` |
|      - | 1157 | `	}` |
|      3 | 1158 | `	if( n == 2 && z[1] == ':' ){` |
|      3 | 1159 | `		return 1; /* a bare drive, "C:" */` |
|      - | 1160 | `	}` |
|      3 | 1161 | `	if( n > 1 && (z[0] == '/' \|\| z[0] == '\\') && (z[1] == '/' \|\| z[1] == '\\') ){` |
|    ! 0 | 1162 | `		for( i = 2 ; i < n ; i++ ){` |
|    ! 0 | 1163 | `			if( z[i] == '/' \|\| z[i] == '\\' ){` |
|    ! 0 | 1164 | `				nSep++;` |
|      - | 1165 | `			}` |
|    ! 0 | 1166 | `		}` |
|    ! 0 | 1167 | `		return nSep < 2; /* still inside \\server\share */` |
|      - | 1168 | `	}` |
|      3 | 1169 | `	return 0;` |
|      - | 1170 | `#else` |
|    140 | 1171 | `	SXUNUSED(z);` |
|    224 | 1172 | `	return n < 1;` |
|      - | 1173 | `#endif` |
|      3 | 1174 | `}` |
|      - | 1175 | `#ifdef __WINNT__` |
|      - | 1176 | `#define VFS_MKDIR_SLASH(c) ((c) == '/' \|\| (c) == '\\')` |
|      - | 1177 | `#else` |
|      - | 1178 | `#define VFS_MKDIR_SLASH(c) ((c) == '/')` |
|      - | 1179 | `#endif` |
|      - | 1180 | `/*` |
|      - | 1181 | ` * php's $recursive: create every missing ancestor, then the directory itself.` |
|      - | 1182 | `` * The flag reached the VFS and both back ends dropped it (`SXUNUSED(recursive)`),`` |
|      - | 1183 | `` * so `mkdir("$d/a/b", 0777, true)` -- the everyday way a script prepares an`` |
|      - | 1184 | ` * output tree -- warned "No such file or directory" and answered false whenever` |
|      - | 1185 | ` * more than one level was missing.` |
|      - | 1186 | ` *` |
|      - | 1187 | ` * php does the walk in the WRAPPER too, not in the syscall, and the rules the` |
|      - | 1188 | ` * oracle shows are: the mode is applied to every level it creates; an ancestor` |
|      - | 1189 | ` * that already exists is skipped in silence; the LEAF is always attempted, so an` |
|      - | 1190 | ` * existing one is "File exists" exactly as without the flag; a trailing` |
|      - | 1191 | ` * separator names the same directory; and the empty path is refused up front` |
|      - | 1192 | ` * with a message of its own.` |
|      - | 1193 | ` */` |
|     52 | 1194 | `static int VfsMkdirRecursive(ph7_context *pCtx,ph7_vfs *pVfs,const char *zPath,int iMode)` |
|      3 | 1195 | `{` |
|      - | 1196 | `	SyBlob sWorker;` |
|      - | 1197 | `	const char *zLocal;` |
|     55 | 1198 | `	int i,nPath,nScheme,rc = PH7_OK;` |
|      - | 1199 | `	/* The strip first: the component walk must not cut a "file://" scheme up. */` |
|     55 | 1200 | `	zLocal = PH7_VmFileUrlLocalPath(zPath);` |
|     55 | 1201 | `	nScheme = PH7_VmUrlSchemeLen(zPath,-1);` |
|     52 | 1202 | `	if( zLocal == zPath && nScheme == (int)sizeof("file")-1` |
|     24 | 1203 | `	 && SyStrnicmp(zPath,"file",sizeof("file")-1) == 0 ){` |
|      - | 1204 | `		/* A file:// AUTHORITY this build will not reach: the strip handed the` |
|      - | 1205 | `		 * URL straight back. php answers false and creates NOTHING, where the` |
|      - | 1206 | `		 * walk below would cut the URL into components and make a directory` |
|      - | 1207 | `		 * literally called "file:". (A name under any OTHER scheme really does` |
|      - | 1208 | `` 		 * become a directory of that name on php too -- measured: `zzz://a/b` `` |
|      - | 1209 | ``		 * leaves a `zzz:` behind there as well -- so only this one is refused.) */`` |
|    ! 0 | 1210 | `		return -1;` |
|      - | 1211 | `	}` |
|     55 | 1212 | `	zPath = zLocal;` |
|     55 | 1213 | `	nPath = (int)SyStrlen(zPath);` |
|      - | 1214 | `	/* A trailing separator names the same directory; php's own expand_filepath` |
|      - | 1215 | `	 * drops it before it starts. */` |
|     57 | 1216 | `	while( nPath > 1 && VFS_MKDIR_SLASH(zPath[nPath-1]) ){` |
|      3 | 1217 | `		nPath--;` |
|      1 | 1218 | `	}` |
|     55 | 1219 | `	if( nPath < 1 ){` |
|      - | 1220 | `		/* php: expand_filepath() refuses it, with this wording and no path. */` |
|      3 | 1221 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Invalid path",ph7_function_name(pCtx));` |
|      3 | 1222 | `		return -1;` |
|      - | 1223 | `	}` |
|     53 | 1224 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|   2462 | 1225 | `	for( i = 1 ; i <= nPath ; i++ ){` |
|   2420 | 1226 | `		int bLeaf = (i == nPath);` |
|   2420 | 1227 | `		if( !bLeaf ){` |
|      - | 1228 | `			/* Only at a separator that ENDS a component: a run of them names` |
|      - | 1229 | `			 * the same ancestor once. */` |
|   2370 | 1230 | `			if( !VFS_MKDIR_SLASH(zPath[i]) \|\| VFS_MKDIR_SLASH(zPath[i-1]) ){` |
|   2146 | 1231 | `				continue;` |
|      - | 1232 | `			}` |
|    227 | 1233 | `			if( VfsMkdirVolumePrefix(zPath,i) ){` |
|      3 | 1234 | `				continue;` |
|      - | 1235 | `			}` |
|    140 | 1236 | `		}` |
|    277 | 1237 | `		SyBlobReset(&sWorker);` |
|    274 | 1238 | `		if( SyBlobAppend(&sWorker,zPath,(sxu32)i) != SXRET_OK` |
|    277 | 1239 | `		 \|\| SyBlobNullAppend(&sWorker) != SXRET_OK ){` |
|    ! 0 | 1240 | `			rc = -1;` |
|    ! 0 | 1241 | `			break;` |
|      - | 1242 | `		}` |
|    277 | 1243 | `		if( !bLeaf && VfsPathStatable(pVfs,(const char *)SyBlobData(&sWorker)) ){` |
|    174 | 1244 | `			continue; /* an ancestor that is already there */` |
|      - | 1245 | `		}` |
|    106 | 1246 | `		errno = 0;` |
|    106 | 1247 | `		rc = pVfs->xMkdir((const char *)SyBlobData(&sWorker),iMode,0);` |
|    106 | 1248 | `		if( rc != PH7_OK ){` |
|     12 | 1249 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      8 | 1250 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      9 | 1251 | `			break;` |
|      - | 1252 | `		}` |
|     44 | 1253 | `	}` |
|     53 | 1254 | `	SyBlobRelease(&sWorker);` |
|     53 | 1255 | `	return rc;` |
|     25 | 1256 | `}` |
|    361 | 1257 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1258 | `{` |
|    366 | 1259 | `	int iRecursive = 0;` |
|      - | 1260 | `	const char *zPath;` |
|      - | 1261 | `	ph7_vfs *pVfs;` |
|      - | 1262 | `	phl_stream_ctx *pStreamCtx;` |
|    366 | 1263 | `	int iMode,rc,bThrew = 0;` |
|    366 | 1264 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 1265 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1266 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1267 | `		return PH7_OK;` |
|      - | 1268 | `	}` |
|      - | 1269 | ``	/* php's `?resource $context`, refused when it is a resource of another kind,`` |
|      - | 1270 | `	 * and SET on the wrapper instance the write door below dispatches to. */` |
|    366 | 1271 | `	pStreamCtx = PH7_StreamCtxFromArg(pCtx,nArg,apArg,3,"$context",0,&bThrew);` |
|    366 | 1272 | `	if( bThrew ){` |
|      6 | 1273 | `		return PH7_OK;` |
|      - | 1274 | `	}` |
|      - | 1275 | `	/* php's mkdir() hands the wrapper ($url, $mode, $options) -- the mode the caller` |
|      - | 1276 | `	 * asked for (0777 by default on every platform: it is the WRAPPER's to` |
|      - | 1277 | `	 * interpret), and STREAM_REPORT_ERRORS plus STREAM_MKDIR_RECURSIVE. */` |
|      - | 1278 | `	{` |
|      - | 1279 | `		ph7_value sMode,sOpt;` |
|      - | 1280 | `		ph7_value *apExtra[2];` |
|      - | 1281 | `		int bDone;` |
|    362 | 1282 | `		PH7_MemObjInit(pCtx->pVm,&sMode);` |
|    362 | 1283 | `		PH7_MemObjInit(pCtx->pVm,&sOpt);` |
|    362 | 1284 | `		ph7_value_int(&sMode,nArg > 1 ? ph7_value_to_int(apArg[1]) : 0777);` |
|    362 | 1285 | `		ph7_value_int(&sOpt,PH7_STREAM_REPORT_ERRORS` |
|    357 | 1286 | `			\| ((nArg > 2 && ph7_value_to_bool(apArg[2])) ? PH7_STREAM_MKDIR_RECURSIVE : 0));` |
|    362 | 1287 | `		apExtra[0] = &sMode;` |
|    362 | 1288 | `		apExtra[1] = &sOpt;` |
|    535 | 1289 | `		bDone = VfsUserWrite(pCtx,ph7_value_to_string(apArg[0],0),"mkdir",` |
|    173 | 1290 | `			(void *)pStreamCtx,apExtra,2);` |
|    362 | 1291 | `		PH7_MemObjRelease(&sMode);` |
|    362 | 1292 | `		PH7_MemObjRelease(&sOpt);` |
|    362 | 1293 | `		if( bDone ){` |
|     11 | 1294 | `			return PH7_OK;` |
|      - | 1295 | `		}` |
|      - | 1296 | `	}` |
|    352 | 1297 | `	if( VfsBuiltinWrapperRefuses(pCtx,ph7_value_to_string(apArg[0],0),0,VFS_POP_MKDIR) ){` |
|     22 | 1298 | `		return PH7_OK;` |
|      - | 1299 | `	}` |
|      - | 1300 | `	/* Point to the underlying vfs */` |
|    332 | 1301 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    332 | 1302 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - | 1303 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1304 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1305 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1306 | `			ph7_function_name(pCtx)` |
|      - | 1307 | `			);` |
|    ! 0 | 1308 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1309 | `		return PH7_OK;` |
|      - | 1310 | `	}` |
|      - | 1311 | `	/* Point to the desired directory */` |
|    332 | 1312 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1313 | `#ifdef __WINNT__` |
|      5 | 1314 | `	iMode = 0;` |
|      - | 1315 | `#else` |
|      - | 1316 | `	/* Assume UNIX */` |
|    327 | 1317 | `	iMode = 0777;` |
|      - | 1318 | `#endif` |
|    332 | 1319 | `	if( nArg > 1 ){` |
|     55 | 1320 | `		iMode = ph7_value_to_int(apArg[1]);` |
|     55 | 1321 | `		if( nArg > 2 ){` |
|     55 | 1322 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|     22 | 1323 | `		}` |
|     22 | 1324 | `	}` |
|      - | 1325 | `	/* Perform the requested operation */` |
|    332 | 1326 | `	if( iRecursive ){` |
|     55 | 1327 | `		rc = VfsMkdirRecursive(pCtx,pVfs,zPath,iMode);` |
|     25 | 1328 | `	}else{` |
|    280 | 1329 | `		errno = 0;` |
|    280 | 1330 | `		rc = pVfs->xMkdir(zPath,iMode,0);` |
|    280 | 1331 | `		if( rc != PH7_OK ){` |
|      - | 1332 | `			/* php does NOT name the path for mkdir: "mkdir(): File exists" */` |
|      7 | 1333 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 | 1334 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      2 | 1335 | `		}` |
|      - | 1336 | `	}` |
|      - | 1337 | `	/* IO return value */` |
|    332 | 1338 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    332 | 1339 | `	return PH7_OK;` |
|    180 | 1340 | `}` |
|      - | 1341 | `/*` |
|      - | 1342 | ` * bool rename(string $oldname,string $newname)` |
|      - | 1343 | ` *  Attempts to rename oldname to newname.` |
|      - | 1344 | ` * Parameters` |
|      - | 1345 | ` *  $oldname` |
|      - | 1346 | ` *   Old name.` |
|      - | 1347 | ` *  $newname` |
|      - | 1348 | ` *   New name.` |
|      - | 1349 | ` * Return` |
|      - | 1350 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1351 | ` */` |
|     44 | 1352 | `static int PH7_vfs_rename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1353 | `{` |
|      - | 1354 | `	const char *zOld,*zNew;` |
|      - | 1355 | `	ph7_vfs *pVfs;` |
|      - | 1356 | `	phl_stream_ctx *pStreamCtx;` |
|     46 | 1357 | `	int rc,bThrew = 0;` |
|     46 | 1358 | `	if( nArg < 2 \|\| !VfsPathArgUsable(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 1359 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1360 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1361 | `		return PH7_OK;` |
|      - | 1362 | `	}` |
|      - | 1363 | ``	/* php's `?resource $context`, refused when it is a resource of another kind,`` |
|      - | 1364 | `	 * and SET on the wrapper instance the write door below dispatches to. */` |
|     46 | 1365 | `	pStreamCtx = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|     46 | 1366 | `	if( bThrew ){` |
|      3 | 1367 | `		return PH7_OK;` |
|      - | 1368 | `	}` |
|     44 | 1369 | `	zOld = ph7_value_to_string(apArg[0],0);` |
|     44 | 1370 | `	zNew = ph7_value_to_string(apArg[1],0);` |
|      - | 1371 | `	{` |
|      - | 1372 | `		/* php resolves BOTH ends before renaming anything. A scheme nothing is` |
|      - | 1373 | `		 * registered under is its own reason, reported and then left to the` |
|      - | 1374 | `		 * ordinary rename to fail on; two ends belonging to DIFFERENT wrappers are` |
|      - | 1375 | `		 * refused outright rather than read through one and written to the other. */` |
|     44 | 1376 | `		const char *zProbe = zOld;` |
|     44 | 1377 | `		const ph7_io_stream *pA = PH7_VmGetStreamDevice(pCtx->pVm,&zProbe,(int)SyStrlen(zOld));` |
|      - | 1378 | `		const ph7_io_stream *pB;` |
|     44 | 1379 | `		zProbe = zNew;` |
|     44 | 1380 | `		pB = PH7_VmGetStreamDevice(pCtx->pVm,&zProbe,(int)SyStrlen(zNew));` |
|     44 | 1381 | `		if( pA == 0 \|\| pB == 0 ){` |
|      - | 1382 | `			/* php reports the scheme it could not place and then goes on with the` |
|      - | 1383 | `			 * comparison, the unplaced end counting as the plain-files wrapper --` |
|      - | 1384 | `			 * so a plain path beside it renames (and fails on the path), while a` |
|      - | 1385 | `			 * WRAPPER beside it is the across-types refusal below. */` |
|      - | 1386 | `			{` |
|      - | 1387 | ``				/* `file://host` is not "nobody is registered": a wrapper answered`` |
|      - | 1388 | `				 * and declined the name, and php words that ONCE, from the` |
|      - | 1389 | `				 * path-op door (VfsBuiltinWrapperRefuses) -- so this screen stays` |
|      - | 1390 | `				 * quiet for it and speaks only for a scheme with no wrapper. */` |
|      7 | 1391 | `				const char *zBad = pA == 0 ? zOld : zNew;` |
|      7 | 1392 | `				int nScheme = 0;` |
|      7 | 1393 | `				if( !PH7_VmStreamDeviceIsRemoteHost(zBad,-1,&nScheme) ){` |
|      5 | 1394 | `					VfsThrowUnknownWrapperWarning(pCtx,zBad);` |
|      2 | 1395 | `				}` |
|      - | 1396 | `			}` |
|      7 | 1397 | `			if( pA == 0 ){` |
|      4 | 1398 | `				pA = pCtx->pVm->pDefStream;` |
|      2 | 1399 | `			}` |
|      7 | 1400 | `			if( pB == 0 ){` |
|      7 | 1401 | `				pB = pCtx->pVm->pDefStream;` |
|      3 | 1402 | `			}` |
|      3 | 1403 | `		}` |
|     44 | 1404 | `		if( pA != pB ){` |
|     10 | 1405 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Cannot rename a file across wrapper types",` |
|      3 | 1406 | `				ph7_function_name(pCtx));` |
|      7 | 1407 | `			ph7_result_bool(pCtx,0);` |
|     24 | 1408 | `			return PH7_OK;` |
|    ! 0 | 1409 | `		}else{` |
|      - | 1410 | `			/* One wrapper owns both ends, so the destination is its second` |
|      - | 1411 | `			 * argument -- the full url, as php hands it over. */` |
|      - | 1412 | `			ph7_value sDest;` |
|      - | 1413 | `			ph7_value *apExtra[1];` |
|      - | 1414 | `			int bDone;` |
|     38 | 1415 | `			PH7_MemObjInit(pCtx->pVm,&sDest);` |
|     38 | 1416 | `			ph7_value_string(&sDest,zNew,-1);` |
|     38 | 1417 | `			apExtra[0] = &sDest;` |
|     38 | 1418 | `			bDone = VfsUserWrite(pCtx,zOld,"rename",(void *)pStreamCtx,apExtra,1);` |
|     38 | 1419 | `			PH7_MemObjRelease(&sDest);` |
|     38 | 1420 | `			if( bDone ){` |
|     21 | 1421 | `				return PH7_OK;` |
|      - | 1422 | `			}` |
|     32 | 1423 | `			if( VfsBuiltinWrapperRefuses(pCtx,zOld,zNew,VFS_POP_RENAME) ){` |
|     27 | 1424 | `				return PH7_OK;` |
|      - | 1425 | `			}` |
|      - | 1426 | `		}` |
|      - | 1427 | `	}` |
|      - | 1428 | `	/* Point to the underlying vfs */` |
|      5 | 1429 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 1430 | `	if( pVfs == 0 \|\| pVfs->xRename == 0 ){` |
|      - | 1431 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1432 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1433 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1434 | `			ph7_function_name(pCtx)` |
|      - | 1435 | `			);` |
|    ! 0 | 1436 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1437 | `		return PH7_OK;` |
|      - | 1438 | `	}` |
|      - | 1439 | `	/* Perform the requested operation */` |
|      5 | 1440 | `	errno = 0;` |
|      5 | 1441 | `	rc = pVfs->xRename(zOld,zNew);` |
|      5 | 1442 | `	if( rc != PH7_OK ){` |
|      - | 1443 | `		/* php names BOTH paths here */` |
|    ! 0 | 1444 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s,%s): %s",` |
|    ! 0 | 1445 | `			ph7_function_name(pCtx),zOld,zNew,VfsStrerror(errno));` |
|    ! 0 | 1446 | `	}` |
|      - | 1447 | `	/* IO result */` |
|      5 | 1448 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      5 | 1449 | `	return PH7_OK;` |
|     23 | 1450 | `}` |
|      - | 1451 | `/*` |
|      - | 1452 | ` * string realpath(string $path)` |
|      - | 1453 | ` *  Returns canonicalized absolute pathname.` |
|      - | 1454 | ` * Parameters` |
|      - | 1455 | ` *  $path` |
|      - | 1456 | ` *   Target path.` |
|      - | 1457 | ` * Return` |
|      - | 1458 | ` *  Canonicalized absolute pathname on success. or FALSE on failure.` |
|      - | 1459 | ` */` |
|    812 | 1460 | `static int PH7_vfs_realpath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1461 | `{` |
|      - | 1462 | `	const char *zPath;` |
|      - | 1463 | `	ph7_vfs *pVfs;` |
|      - | 1464 | `        int rc;` |
|    817 | 1465 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 1466 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1467 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1468 | `		return PH7_OK;` |
|      - | 1469 | `	}` |
|      - | 1470 | `	/* Point to the underlying vfs */` |
|    817 | 1471 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    817 | 1472 | `	if( pVfs == 0 \|\| pVfs->xRealpath == 0 ){` |
|      - | 1473 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1474 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1475 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1476 | `			ph7_function_name(pCtx)` |
|      - | 1477 | `			);` |
|    ! 0 | 1478 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1479 | `		return PH7_OK;` |
|      - | 1480 | `	}` |
|      - | 1481 | `	/* Set an empty string untnil the underlying OS interface change that */` |
|    817 | 1482 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1483 | ``	/* Perform the requested operation. php resolves an EMPTY path as `.` and so`` |
|      - | 1484 | `	 * answers the working directory, where this answered false. */` |
|    817 | 1485 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|    817 | 1486 | `	if( zPath == 0 \|\| zPath[0] == 0 ){` |
|      2 | 1487 | `		zPath = ".";` |
|      1 | 1488 | `	}` |
|    817 | 1489 | `	rc = pVfs->xRealpath(zPath,pCtx);` |
|    817 | 1490 | `	if( rc != PH7_OK ){` |
|     16 | 1491 | `	 ph7_result_bool(pCtx,0);` |
|      8 | 1492 | `	}` |
|    817 | 1493 | `	return PH7_OK;` |
|    410 | 1494 | `}` |
|      - | 1495 | `/*` |
|      - | 1496 | ` * Does this candidate name something, and if so what is its canonical path?` |
|      - | 1497 | ` * The existence question is asked separately because xRealpath() writes STRAIGHT` |
|      - | 1498 | ` * into the call's result, so it may only be run on the winner.` |
|      - | 1499 | ` */` |
|     36 | 1500 | `static int VfsResolveTry(ph7_vfs *pVfs,ph7_context *pCtx,const char *zCand)` |
|      2 | 1501 | `{` |
|     38 | 1502 | `	if( pVfs->xFileExists(zCand) != PH7_OK ){` |
|     22 | 1503 | `		return 0;` |
|      - | 1504 | `	}` |
|      - | 1505 | `	/* The VFS APPENDS into the call's result, so make it an empty string first` |
|      - | 1506 | `	 * -- the realpath() builtin beside this one seeds the same way. */` |
|     18 | 1507 | `	ph7_result_string(pCtx,"",0);` |
|     18 | 1508 | `	if( pVfs->xRealpath(zCand,pCtx) == PH7_OK ){` |
|     18 | 1509 | `		return 1;` |
|      - | 1510 | `	}` |
|    ! 0 | 1511 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 | 1512 | `	return 0;` |
|     20 | 1513 | `}` |
|      - | 1514 | `/*` |
|      - | 1515 | ` * Is this an ABSOLUTE path, by php's rule for this platform? A lone leading` |
|      - | 1516 | ` * slash is NOT absolute on Windows -- php walks the include_path for it.` |
|      - | 1517 | ` */` |
|     18 | 1518 | `static int VfsPathIsAbsolute(const char *z,int n)` |
|      2 | 1519 | `{` |
|      - | 1520 | `#ifdef __WINNT__` |
|      2 | 1521 | `	if( n >= 2 && ((z[0] >= 'A' && z[0] <= 'Z') \|\| (z[0] >= 'a' && z[0] <= 'z')) && z[1] == ':' ){` |
|      1 | 1522 | `		return 1;` |
|      - | 1523 | `	}` |
|      2 | 1524 | `	return n >= 2 && (z[0] == '/' \|\| z[0] == '\\') && (z[1] == '/' \|\| z[1] == '\\');` |
|      - | 1525 | `#else` |
|     18 | 1526 | `	return n >= 1 && z[0] == '/';` |
|      - | 1527 | `#endif` |
|      2 | 1528 | `}` |
|      - | 1529 | `/* "./x" and "../x": php reads these against the CWD and never walks the path. */` |
|     22 | 1530 | `static int VfsPathIsDotRelative(const char *z,int n)` |
|      2 | 1531 | `{` |
|      - | 1532 | `#ifdef __WINNT__` |
|      - | 1533 | `#define VFS_RESOLVE_SLASH(c) ((c) == '/' \|\| (c) == '\\')` |
|      - | 1534 | `#else` |
|      - | 1535 | `#define VFS_RESOLVE_SLASH(c) ((c) == '/')` |
|      - | 1536 | `#endif` |
|     24 | 1537 | `	if( n < 2 \|\| z[0] != '.' ){` |
|     20 | 1538 | `		return 0;` |
|      - | 1539 | `	}` |
|      5 | 1540 | `	if( VFS_RESOLVE_SLASH(z[1]) ){` |
|      3 | 1541 | `		return 1;` |
|      - | 1542 | `	}` |
|      3 | 1543 | `	return n > 2 && z[1] == '.' && VFS_RESOLVE_SLASH(z[2]);` |
|     13 | 1544 | `}` |
|      - | 1545 | `/*` |
|      - | 1546 | ` * string\|false stream_resolve_include_path(string $filename)` |
|      - | 1547 | ` *  Where would include/require find this name? php's own php_resolve_path,` |
|      - | 1548 | ` *  which is the ONLY way a script can ask that question without opening` |
|      - | 1549 | ` *  anything -- and the way an autoloader decides whether a class file exists` |
|      - | 1550 | ` *  before requiring it.` |
|      - | 1551 | ` * Return` |
|      - | 1552 | ` *  The canonical path on success, FALSE when nothing answers.` |
|      - | 1553 | ` */` |
|     30 | 1554 | `static int PH7_vfs_stream_resolve_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1555 | `{` |
|     32 | 1556 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1557 | `	ph7_vfs *pVfs;` |
|      - | 1558 | `	const char *zPath;` |
|      - | 1559 | `	SyString *aEntry;` |
|      - | 1560 | `	SyString sDir;` |
|      - | 1561 | `	SyBlob sWorker;` |
|     32 | 1562 | `	int nPath = 0, nScheme, c;` |
|      - | 1563 | `	sxu32 n;` |
|      - | 1564 | `	/* FALSE until something resolves: xRealpath() overwrites it on the winner. */` |
|     32 | 1565 | `	ph7_result_bool(pCtx,0);` |
|     32 | 1566 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     32 | 1567 | `	if( nArg < 1 \|\| pVfs == 0 \|\| pVfs->xRealpath == 0 \|\| pVfs->xFileExists == 0 ){` |
|    ! 0 | 1568 | `		return PH7_OK;` |
|      - | 1569 | `	}` |
|     32 | 1570 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|     32 | 1571 | `	if( nPath < 0 ){` |
|    ! 0 | 1572 | `		nPath = 0;` |
|    ! 0 | 1573 | `	}` |
|     32 | 1574 | `	nScheme = PH7_VmUrlSchemeLen(zPath,nPath);` |
|     32 | 1575 | `	if( nScheme > 0 ){` |
|      - | 1576 | `		/* A name that carries a scheme is never walked. php resolves exactly one` |
|      - | 1577 | `		 * of them -- file://, which it realpaths where it stands -- and answers` |
|      - | 1578 | `		 * false for every other wrapper. An unreachable authority (and any other` |
|      - | 1579 | `		 * scheme) comes back unchanged from the strip, and is false. */` |
|      9 | 1580 | `		const char *zLocal = PH7_VmFileUrlLocalPath(zPath);` |
|      9 | 1581 | `		if( zLocal != zPath ){` |
|      3 | 1582 | `			VfsResolveTry(pVfs,pCtx,zLocal);` |
|      1 | 1583 | `		}` |
|      9 | 1584 | `		return PH7_OK;` |
|      - | 1585 | `	}` |
|     22 | 1586 | `	if( VfsPathIsDotRelative(zPath,nPath) \|\| VfsPathIsAbsolute(zPath,nPath)` |
|     17 | 1587 | `	 \|\| SySetUsed(&pVm->aPaths) < 1 ){` |
|     11 | 1588 | `		VfsResolveTry(pVfs,pCtx,zPath);` |
|     11 | 1589 | `		return PH7_OK;` |
|      - | 1590 | `	}` |
|     14 | 1591 | `	c = '/';` |
|      - | 1592 | `#ifdef __WINNT__` |
|      2 | 1593 | `	c = '\\';` |
|      - | 1594 | `#endif` |
|     14 | 1595 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|     14 | 1596 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|     26 | 1597 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|      - | 1598 | `		SyString sFile;` |
|     20 | 1599 | `		SyStringInitFromBuf(&sFile,zPath,(sxu32)nPath);` |
|     20 | 1600 | `		SyBlobReset(&sWorker);` |
|     20 | 1601 | `		SyBlobFormat(&sWorker,"%z%c%z",&aEntry[n],c,&sFile);` |
|     20 | 1602 | `		if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 1603 | `			continue;` |
|      - | 1604 | `		}` |
|     20 | 1605 | `		if( VfsResolveTry(pVfs,pCtx,(const char *)SyBlobData(&sWorker)) ){` |
|      7 | 1606 | `			SyBlobRelease(&sWorker);` |
|      7 | 1607 | `			return PH7_OK;` |
|      - | 1608 | `		}` |
|      8 | 1609 | `	}` |
|      - | 1610 | `	/* The same last resort the opener uses: the executing file's directory. */` |
|      8 | 1611 | `	if( PH7_VmExecutingDir(pVm,&sDir) ){` |
|      - | 1612 | `		SyString sFile;` |
|      8 | 1613 | `		SyStringInitFromBuf(&sFile,zPath,(sxu32)nPath);` |
|      8 | 1614 | `		SyBlobReset(&sWorker);` |
|      8 | 1615 | `		SyBlobFormat(&sWorker,"%z%c%z",&sDir,c,&sFile);` |
|      8 | 1616 | `		if( SXRET_OK == SyBlobNullAppend(&sWorker) ){` |
|      8 | 1617 | `			VfsResolveTry(pVfs,pCtx,(const char *)SyBlobData(&sWorker));` |
|      3 | 1618 | `		}` |
|      3 | 1619 | `	}` |
|      8 | 1620 | `	SyBlobRelease(&sWorker);` |
|      8 | 1621 | `	return PH7_OK;` |
|     17 | 1622 | `}` |
|      - | 1623 | `/*` |
|      - | 1624 | ` * int sleep(int $seconds)` |
|      - | 1625 | ` *  Delays the program execution for the given number of seconds.` |
|      - | 1626 | ` * Parameters` |
|      - | 1627 | ` *  $seconds` |
|      - | 1628 | ` *   Halt time in seconds.` |
|      - | 1629 | ` * Return` |
|      - | 1630 | ` *  Zero on success or FALSE on failure.` |
|      - | 1631 | ` */` |
|     10 | 1632 | `static int PH7_vfs_sleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1633 | `{` |
|      - | 1634 | `	ph7_vfs *pVfs;` |
|      - | 1635 | `	int rc,nSleep;` |
|     11 | 1636 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - | 1637 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1638 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1639 | `		return PH7_OK;` |
|      - | 1640 | `	}` |
|      - | 1641 | `	/* Point to the underlying vfs */` |
|     11 | 1642 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 1643 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - | 1644 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1645 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1646 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1647 | `			ph7_function_name(pCtx)` |
|      - | 1648 | `			);` |
|    ! 0 | 1649 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1650 | `		return PH7_OK;` |
|      - | 1651 | `	}` |
|      - | 1652 | `	/* Amount to sleep */` |
|     11 | 1653 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|     11 | 1654 | `	if( nSleep < 0 ){` |
|      - | 1655 | `		/* php raises rather than sleeping for an unsigned-wrapped eternity */` |
|      3 | 1656 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1657 | `			"sleep(): Argument #1 ($seconds) must be greater than or equal to 0");` |
|      - | 1658 | `	}` |
|      - | 1659 | `	/* Perform the requested operation (Microseconds) */` |
|      9 | 1660 | `	rc = pVfs->xSleep((unsigned int)(nSleep * SX_USEC_PER_SEC));` |
|      9 | 1661 | `	if( rc != PH7_OK ){` |
|      - | 1662 | `		/* Return FALSE */` |
|    ! 0 | 1663 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1664 | `	}else{` |
|      - | 1665 | `		/* Return zero */` |
|      9 | 1666 | `		ph7_result_int(pCtx,0);` |
|      - | 1667 | `	}` |
|      9 | 1668 | `	return PH7_OK;` |
|      6 | 1669 | `}` |
|      - | 1670 | `/*` |
|      - | 1671 | ` * void usleep(int $micro_seconds)` |
|      - | 1672 | ` *  Delays program execution for the given number of micro seconds.` |
|      - | 1673 | ` * Parameters` |
|      - | 1674 | ` *  $micro_seconds` |
|      - | 1675 | ` *   Halt time in micro seconds. A micro second is one millionth of a second.` |
|      - | 1676 | ` * Return` |
|      - | 1677 | ` *  None.` |
|      - | 1678 | ` */` |
|    193 | 1679 | `static int PH7_vfs_usleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1680 | `{` |
|      - | 1681 | `	ph7_vfs *pVfs;` |
|      - | 1682 | `	int nSleep;` |
|    197 | 1683 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - | 1684 | `		/* Missing/Invalid argument,return immediately */` |
|    ! 0 | 1685 | `		return PH7_OK;` |
|      - | 1686 | `	}` |
|      - | 1687 | `	/* Point to the underlying vfs */` |
|    197 | 1688 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    197 | 1689 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - | 1690 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1691 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1692 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 1693 | `			ph7_function_name(pCtx)` |
|      - | 1694 | `			);` |
|    ! 0 | 1695 | `		return PH7_OK;` |
|      - | 1696 | `	}` |
|      - | 1697 | `	/* Amount to sleep */` |
|    197 | 1698 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|    197 | 1699 | `	if( nSleep < 0 ){` |
|      - | 1700 | `		/* php raises rather than sleeping for an unsigned-wrapped eternity */` |
|      3 | 1701 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1702 | `			"usleep(): Argument #1 ($microseconds) must be greater than or equal to 0");` |
|      - | 1703 | `	}` |
|      - | 1704 | `	/* Perform the requested operation (Microseconds) */` |
|    195 | 1705 | `	pVfs->xSleep((unsigned int)nSleep);` |
|    195 | 1706 | `	return PH7_OK;` |
|    102 | 1707 | `}` |
|      - | 1708 | `/*` |
|      - | 1709 | ` * ---------------------------------------------------------------------------` |
|      - | 1710 | ` * A path operation over a BUILT-IN wrapper other than the plain-file one.` |
|      - | 1711 | ` *` |
|      - | 1712 | ` * php's unlink/rename/mkdir/rmdir/chmod belong to a WRAPPER, and every built-in` |
|      - | 1713 | ` * wrapper but the plain-file one implements none of them: php answers false and` |
|      - | 1714 | ` * words the refusal from the wrapper's LABEL. PHL sent every one of them` |
|      - | 1715 | ` * straight to the OS, which then failed on a path with a scheme in it -- so` |
|      - | 1716 | `` * `unlink('php://memory')` reported `No such file or directory` where php says`` |
|      - | 1717 | `` * `PHP does not allow unlinking`, and the same for data://, glob://, http:// and`` |
|      - | 1718 | ` * compress.zlib://.` |
|      - | 1719 | ` *` |
|      - | 1720 | ` * The two silent ones are php's too: mkdir() and rmdir() over such a wrapper are` |
|      - | 1721 | ` * a bare false with no diagnostic at all, while unlink() and rename() have a` |
|      - | 1722 | ` * sentence each and the chmod family shares a third.` |
|      - | 1723 | ` *` |
|      - | 1724 | ` * A USERLAND wrapper never reaches here -- its own door (VfsUserWrite) runs` |
|      - | 1725 | ` * first and answers for it, including php's "no such method" refusal.` |
|      - | 1726 | ` * ---------------------------------------------------------------------------` |
|      - | 1727 | ` */` |
|  52380 | 1728 | `static int VfsBuiltinWrapperRefuses(ph7_context *pCtx,const char *zPath,const char *zDest,int eOp)` |
|      5 | 1729 | `{` |
|      - | 1730 | `	const ph7_io_stream *pStream;` |
|  52385 | 1731 | `	const char *zTail = zPath;` |
|      - | 1732 | `	const char *zLabel;` |
|  52385 | 1733 | `	if( zPath == 0 ){` |
|    ! 0 | 1734 | `		return 0;` |
|      - | 1735 | `	}` |
|  52385 | 1736 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zTail,(int)SyStrlen(zPath));` |
|  52385 | 1737 | `	if( pStream == 0 ){` |
|     23 | 1738 | `		int nScheme = 0;` |
|     23 | 1739 | `		if( PH7_VmStreamDeviceIsRemoteHost(zPath,(int)SyStrlen(zPath),&nScheme) ){` |
|      - | 1740 | ``			/* `file://host/path` is the OTHER shape: a wrapper was found and`` |
|      - | 1741 | `			 * declined the name, so php refuses outright rather than falling` |
|      - | 1742 | `			 * back. Its table is the one below with two sentences swapped --` |
|      - | 1743 | `			 * unlink() and rename() name the LOOKUP rather than a wrapper label,` |
|      - | 1744 | `			 * chmod() keeps its own, and mkdir()/rmdir() stay silent. */` |
|     13 | 1745 | `			if( eOp == VFS_POP_UNLINK \|\| eOp == VFS_POP_RENAME ){` |
|      6 | 1746 | `				PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to locate stream wrapper",` |
|      2 | 1747 | `					ph7_function_name(pCtx));` |
|     11 | 1748 | `			}else if( eOp == VFS_POP_CHMOD ){` |
|      3 | 1749 | `				PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - | 1750 | `					"%s(): Cannot call %s() for a non-standard stream",` |
|      1 | 1751 | `					ph7_function_name(pCtx),ph7_function_name(pCtx));` |
|      1 | 1752 | `			}` |
|     13 | 1753 | `			ph7_result_bool(pCtx,0);` |
|     13 | 1754 | `			return 1;` |
|      - | 1755 | `		}` |
|      - | 1756 | `		/* A scheme nothing is registered under: php names it and then takes the` |
|      - | 1757 | `		 * whole uri to the plain-files path below, which is what these ops run` |
|      - | 1758 | `		 * on. Once per lookup the C body makes: rename() resolves BOTH of its` |
|      - | 1759 | `		 * names and so says it twice, but its second is the wrapper-mismatch` |
|      - | 1760 | `		 * screen's own (see the pA/pB compare below) and not this one's. */` |
|     10 | 1761 | `		VfsThrowUnknownWrapperWarning(pCtx,zPath);` |
|     10 | 1762 | `		return 0;` |
|      - | 1763 | `	}` |
|  52363 | 1764 | `	zLabel = PH7_StreamWrapperLabel(pCtx->pVm,pStream);` |
|  52363 | 1765 | `	if( zLabel == 0 ){` |
|  52268 | 1766 | `		return 0;` |
|      - | 1767 | `	}` |
|      - | 1768 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     96 | 1769 | `	if( PH7_PharStreamIs(pStream) && eOp != VFS_POP_CHMOD ){` |
|      - | 1770 | `		/* ext/phar is the one built-in wrapper here that DOES implement the` |
|      - | 1771 | `		 * path operations -- an archive can have an entry deleted, created or` |
|      - | 1772 | `` 		 * renamed -- so it answers for those itself, with its own `phar.readonly` `` |
|      - | 1773 | `		 * refusal. It implements no stream_metadata, so chmod(), chown() and` |
|      - | 1774 | `		 * chgrp() still take the sentence below. */` |
|     67 | 1775 | `		return PH7_PharPathOp(pCtx,zPath,zDest,eOp);` |
|      - | 1776 | `	}` |
|      - | 1777 | `#endif` |
|     29 | 1778 | `	switch( eOp ){` |
|      4 | 1779 | `	case VFS_POP_UNLINK:` |
|     13 | 1780 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s does not allow unlinking",` |
|      4 | 1781 | `			ph7_function_name(pCtx),zLabel);` |
|      9 | 1782 | `		break;` |
|      2 | 1783 | `	case VFS_POP_RENAME:` |
|      7 | 1784 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s wrapper does not support renaming",` |
|      2 | 1785 | `			ph7_function_name(pCtx),zLabel);` |
|      5 | 1786 | `		break;` |
|      6 | 1787 | `	case VFS_POP_CHMOD:` |
|      - | 1788 | `		/* php's sentence names the function TWICE and the wrapper not at all. */` |
|     19 | 1789 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Cannot call %s() for a non-standard stream",` |
|      6 | 1790 | `			ph7_function_name(pCtx),ph7_function_name(pCtx));` |
|     12 | 1791 | `		break;` |
|      2 | 1792 | `	default:` |
|      - | 1793 | `		/* mkdir() and rmdir(): php answers a bare false and says nothing. */` |
|      4 | 1794 | `		break;` |
|      - | 1795 | `	}` |
|     29 | 1796 | `	ph7_result_bool(pCtx,0);` |
|     29 | 1797 | `	return 1;` |
|  26170 | 1798 | `}` |
|      - | 1799 | `/*` |
|      - | 1800 | ` * bool unlink (string $filename)` |
|      - | 1801 | ` *  Delete a file.` |
|      - | 1802 | ` * Parameters` |
|      - | 1803 | ` *  $filename` |
|      - | 1804 | ` *   Path to the file.` |
|      - | 1805 | ` * Return` |
|      - | 1806 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1807 | ` */` |
|  50916 | 1808 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1809 | `{` |
|      - | 1810 | `	const char *zPath;` |
|      - | 1811 | `	ph7_vfs *pVfs;` |
|      - | 1812 | `	phl_stream_ctx *pStreamCtx;` |
|  50921 | 1813 | `	int rc,bThrew = 0;` |
|  50921 | 1814 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 1815 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1816 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1817 | `		return PH7_OK;` |
|      - | 1818 | `	}` |
|      - | 1819 | ``	/* php's `?resource $context`, refused when it is a resource of another kind,`` |
|      - | 1820 | `	 * and SET on the wrapper instance the write door below dispatches to. */` |
|  50921 | 1821 | `	pStreamCtx = PH7_StreamCtxFromArg(pCtx,nArg,apArg,1,"$context",0,&bThrew);` |
|  50921 | 1822 | `	if( bThrew ){` |
|      8 | 1823 | `		return PH7_OK;` |
|      - | 1824 | `	}` |
|      - | 1825 | `	/* php's unlink() hands the wrapper the url and nothing else. */` |
|  76360 | 1826 | `	if( VfsUserWrite(pCtx,ph7_value_to_string(apArg[0],0),"unlink",` |
|  25445 | 1827 | `		(void *)pStreamCtx,0,0) ){` |
|      7 | 1828 | `		return PH7_OK;` |
|      - | 1829 | `	}` |
|  50909 | 1830 | `	if( VfsBuiltinWrapperRefuses(pCtx,ph7_value_to_string(apArg[0],0),0,VFS_POP_UNLINK) ){` |
|     26 | 1831 | `		return PH7_OK;` |
|      - | 1832 | `	}` |
|      - | 1833 | `	/* Point to the underlying vfs */` |
|  50884 | 1834 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  50884 | 1835 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - | 1836 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1837 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1838 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1839 | `			ph7_function_name(pCtx)` |
|      - | 1840 | `			);` |
|    ! 0 | 1841 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1842 | `		return PH7_OK;` |
|      - | 1843 | `	}` |
|      - | 1844 | `	/* Point to the desired directory */` |
|  50884 | 1845 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1846 | `	/* Perform the requested operation */` |
|  50884 | 1847 | `	errno = 0;` |
|  50884 | 1848 | `	rc = pVfs->xUnlink(zPath);` |
|  50884 | 1849 | `	if( rc != PH7_OK ){` |
|  31211 | 1850 | `		VfsThrowSysWarning(pCtx,zPath);` |
|  15601 | 1851 | `	}` |
|      - | 1852 | `	/* IO return value */` |
|  50884 | 1853 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  50884 | 1854 | `	return PH7_OK;` |
|  25453 | 1855 | `}` |
|      - | 1856 | `/*` |
|      - | 1857 | ` * bool chmod(string $filename,int $mode)` |
|      - | 1858 | ` *  Attempts to change the mode of the specified file to that given in mode.` |
|      - | 1859 | ` * Parameters` |
|      - | 1860 | ` *  $filename` |
|      - | 1861 | ` *   Path to the file.` |
|      - | 1862 | ` * $mode` |
|      - | 1863 | ` *   Mode (Must be an integer)` |
|      - | 1864 | ` * Return` |
|      - | 1865 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1866 | ` */` |
|    681 | 1867 | `static int PH7_vfs_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1868 | `{` |
|      - | 1869 | `	const char *zPath;` |
|      - | 1870 | `	ph7_vfs *pVfs;` |
|      - | 1871 | `	int iMode;` |
|      - | 1872 | `	int rc;` |
|    686 | 1873 | `	if( nArg < 2 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 1874 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1875 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1876 | `		return PH7_OK;` |
|      - | 1877 | `	}` |
|      - | 1878 | `	/* php's chmod() is a stream_metadata(STREAM_META_ACCESS) on a wrapper path. */` |
|      - | 1879 | `	{` |
|      - | 1880 | `		ph7_value sOp,sVal;` |
|      - | 1881 | `		ph7_value *apExtra[2];` |
|      - | 1882 | `		int bDone;` |
|    686 | 1883 | `		PH7_MemObjInit(pCtx->pVm,&sOp);` |
|    686 | 1884 | `		PH7_MemObjInit(pCtx->pVm,&sVal);` |
|    686 | 1885 | `		ph7_value_int(&sOp,PH7_STREAM_META_ACCESS);` |
|    686 | 1886 | `		ph7_value_int(&sVal,ph7_value_to_int(apArg[1]));` |
|    686 | 1887 | `		apExtra[0] = &sOp;` |
|    686 | 1888 | `		apExtra[1] = &sVal;` |
|   1026 | 1889 | `		bDone = VfsUserWrite(pCtx,ph7_value_to_string(apArg[0],0),"stream_metadata",` |
|    340 | 1890 | `			0,apExtra,2);` |
|    686 | 1891 | `		PH7_MemObjRelease(&sOp);` |
|    686 | 1892 | `		PH7_MemObjRelease(&sVal);` |
|    686 | 1893 | `		if( bDone ){` |
|      7 | 1894 | `			return PH7_OK;` |
|      - | 1895 | `		}` |
|      - | 1896 | `	}` |
|    680 | 1897 | `	if( VfsBuiltinWrapperRefuses(pCtx,ph7_value_to_string(apArg[0],0),0,VFS_POP_CHMOD) ){` |
|     11 | 1898 | `		return PH7_OK;` |
|      - | 1899 | `	}` |
|      - | 1900 | `	/* Point to the underlying vfs */` |
|    670 | 1901 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    670 | 1902 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - | 1903 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1904 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1905 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1906 | `			ph7_function_name(pCtx)` |
|      - | 1907 | `			);` |
|    ! 0 | 1908 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1909 | `		return PH7_OK;` |
|      - | 1910 | `	}` |
|      - | 1911 | `	/* Point to the desired directory */` |
|    670 | 1912 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1913 | `	/* Extract the mode */` |
|    670 | 1914 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - | 1915 | `	/* Perform the requested operation */` |
|    670 | 1916 | `	errno = 0;` |
|    670 | 1917 | `	rc = pVfs->xChmod(zPath,iMode);` |
|    670 | 1918 | `	if( rc != PH7_OK ){` |
|      - | 1919 | `		/* php warns with the C library's own reason and names NO path -- one of` |
|      - | 1920 | `		 * the family's two shapes, the one mkdir() and the link pair share.` |
|      - | 1921 | `		 * PHL answered FALSE in silence. */` |
|      3 | 1922 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      2 | 1923 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|      1 | 1924 | `	}` |
|      - | 1925 | `	/* IO return value */` |
|    670 | 1926 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    670 | 1927 | `	return PH7_OK;` |
|    345 | 1928 | `}` |
|      - | 1929 | `/*` |
|      - | 1930 | ` * bool chown(string $filename,string $user)` |
|      - | 1931 | ` *  Attempts to change the owner of the file filename to user user.` |
|      - | 1932 | ` * Parameters` |
|      - | 1933 | ` *  $filename` |
|      - | 1934 | ` *   Path to the file.` |
|      - | 1935 | ` * $user` |
|      - | 1936 | ` *   Username.` |
|      - | 1937 | ` * Return` |
|      - | 1938 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1939 | ` */` |
|     16 | 1940 | `static int PH7_vfs_chown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1941 | `{` |
|      - | 1942 | `	const char *zPath,*zUser;` |
|      - | 1943 | `	ph7_vfs *pVfs;` |
|      - | 1944 | `	int rc;` |
|     18 | 1945 | `	if( nArg < 2 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 1946 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1947 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1948 | `		return PH7_OK;` |
|      - | 1949 | `	}` |
|      - | 1950 | `	/* php's chown() is a stream_metadata() on a wrapper path, and WHICH verb it is` |
|      - | 1951 | `	 * depends on the argument's TYPE: an integer names the id, anything else the` |
|      - | 1952 | `	 * name. */` |
|      - | 1953 | `	{` |
|      - | 1954 | `		ph7_value sOp,sVal;` |
|      - | 1955 | `		ph7_value *apExtra[2];` |
|      - | 1956 | `		int bDone;` |
|     18 | 1957 | `		int bId = ph7_value_is_int(apArg[1]);` |
|     18 | 1958 | `		PH7_MemObjInit(pCtx->pVm,&sOp);` |
|     18 | 1959 | `		PH7_MemObjInit(pCtx->pVm,&sVal);` |
|     18 | 1960 | `		ph7_value_int(&sOp,bId ? PH7_STREAM_META_OWNER : PH7_STREAM_META_OWNER_NAME);` |
|     18 | 1961 | `		if( bId ){` |
|      9 | 1962 | `			ph7_value_int64(&sVal,ph7_value_to_int64(apArg[1]));` |
|      5 | 1963 | `		}else{` |
|     10 | 1964 | `			int nUser = 0;` |
|     10 | 1965 | `			const char *zVal = ph7_value_to_string(apArg[1],&nUser);` |
|     10 | 1966 | `			ph7_value_string(&sVal,zVal,nUser);` |
|      - | 1967 | `		}` |
|     18 | 1968 | `		apExtra[0] = &sOp;` |
|     18 | 1969 | `		apExtra[1] = &sVal;` |
|     26 | 1970 | `		bDone = VfsUserWrite(pCtx,ph7_value_to_string(apArg[0],0),"stream_metadata",` |
|      8 | 1971 | `			0,apExtra,2);` |
|     18 | 1972 | `		PH7_MemObjRelease(&sOp);` |
|     18 | 1973 | `		PH7_MemObjRelease(&sVal);` |
|     18 | 1974 | `		if( bDone ){` |
|      9 | 1975 | `			return PH7_OK;` |
|      - | 1976 | `		}` |
|      - | 1977 | `	}` |
|     10 | 1978 | `	if( VfsBuiltinWrapperRefuses(pCtx,ph7_value_to_string(apArg[0],0),0,VFS_POP_CHMOD) ){` |
|      3 | 1979 | `		return PH7_OK;` |
|      - | 1980 | `	}` |
|      - | 1981 | `	/* Point to the underlying vfs */` |
|      7 | 1982 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 1983 | `	if( pVfs == 0 \|\| pVfs->xChown == 0 ){` |
|      - | 1984 | `		/* No ownership to change: Windows has neither a uid nor anything to look` |
|      - | 1985 | `		 * one up in, and php's own build there keeps the function and answers a` |
|      - | 1986 | `		 * SILENT false for it -- asked of php 8.5.8 on the gate guest. */` |
|      1 | 1987 | `		ph7_result_bool(pCtx,0);` |
|      1 | 1988 | `		return PH7_OK;` |
|      - | 1989 | `	}` |
|      - | 1990 | `	/* Point to the desired directory */` |
|      6 | 1991 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1992 | `	/* Extract the user */` |
|      6 | 1993 | `	zUser = ph7_value_to_string(apArg[1],0);` |
|      - | 1994 | `	/* Perform the requested operation */` |
|      6 | 1995 | `	errno = 0;` |
|      6 | 1996 | `	rc = pVfs->xChown(zPath,zUser);` |
|      6 | 1997 | `	if( rc != PH7_OK ){` |
|      - | 1998 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - | 1999 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - | 2000 | `		 * "chown(): Operation not permitted". */` |
|      6 | 2001 | `		if( rc == -2 ){` |
|      3 | 2002 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find uid for %s",` |
|      1 | 2003 | `				ph7_function_name(pCtx),zUser);` |
|      1 | 2004 | `		}else{` |
|      6 | 2005 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 | 2006 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - | 2007 | `		}` |
|      3 | 2008 | `	}` |
|      - | 2009 | `	/* IO return value */` |
|      6 | 2010 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 | 2011 | `	return PH7_OK;` |
|     10 | 2012 | `}` |
|      - | 2013 | `/*` |
|      - | 2014 | ` * bool chgrp(string $filename,string $group)` |
|      - | 2015 | ` *  Attempts to change the group of the file filename to group.` |
|      - | 2016 | ` * Parameters` |
|      - | 2017 | ` *  $filename` |
|      - | 2018 | ` *   Path to the file.` |
|      - | 2019 | ` * $group` |
|      - | 2020 | ` *   groupname.` |
|      - | 2021 | ` * Return` |
|      - | 2022 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2023 | ` */` |
|     16 | 2024 | `static int PH7_vfs_chgrp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2025 | `{` |
|      - | 2026 | `	const char *zPath,*zGroup;` |
|      - | 2027 | `	ph7_vfs *pVfs;` |
|      - | 2028 | `	int rc;` |
|     18 | 2029 | `	if( nArg < 2 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2030 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2031 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2032 | `		return PH7_OK;` |
|      - | 2033 | `	}` |
|      - | 2034 | `	/* php's chgrp() is a stream_metadata() on a wrapper path, and WHICH verb it is` |
|      - | 2035 | `	 * depends on the argument's TYPE: an integer names the id, anything else the` |
|      - | 2036 | `	 * name. */` |
|      - | 2037 | `	{` |
|      - | 2038 | `		ph7_value sOp,sVal;` |
|      - | 2039 | `		ph7_value *apExtra[2];` |
|      - | 2040 | `		int bDone;` |
|     18 | 2041 | `		int bId = ph7_value_is_int(apArg[1]);` |
|     18 | 2042 | `		PH7_MemObjInit(pCtx->pVm,&sOp);` |
|     18 | 2043 | `		PH7_MemObjInit(pCtx->pVm,&sVal);` |
|     18 | 2044 | `		ph7_value_int(&sOp,bId ? PH7_STREAM_META_GROUP : PH7_STREAM_META_GROUP_NAME);` |
|     18 | 2045 | `		if( bId ){` |
|      9 | 2046 | `			ph7_value_int64(&sVal,ph7_value_to_int64(apArg[1]));` |
|      5 | 2047 | `		}else{` |
|     10 | 2048 | `			int nUser = 0;` |
|     10 | 2049 | `			const char *zVal = ph7_value_to_string(apArg[1],&nUser);` |
|     10 | 2050 | `			ph7_value_string(&sVal,zVal,nUser);` |
|      - | 2051 | `		}` |
|     18 | 2052 | `		apExtra[0] = &sOp;` |
|     18 | 2053 | `		apExtra[1] = &sVal;` |
|     26 | 2054 | `		bDone = VfsUserWrite(pCtx,ph7_value_to_string(apArg[0],0),"stream_metadata",` |
|      8 | 2055 | `			0,apExtra,2);` |
|     18 | 2056 | `		PH7_MemObjRelease(&sOp);` |
|     18 | 2057 | `		PH7_MemObjRelease(&sVal);` |
|     18 | 2058 | `		if( bDone ){` |
|      9 | 2059 | `			return PH7_OK;` |
|      - | 2060 | `		}` |
|      - | 2061 | `	}` |
|     10 | 2062 | `	if( VfsBuiltinWrapperRefuses(pCtx,ph7_value_to_string(apArg[0],0),0,VFS_POP_CHMOD) ){` |
|      3 | 2063 | `		return PH7_OK;` |
|      - | 2064 | `	}` |
|      - | 2065 | `	/* Point to the underlying vfs */` |
|      7 | 2066 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 2067 | `	if( pVfs == 0 \|\| pVfs->xChgrp == 0 ){` |
|      - | 2068 | `		/* No ownership to change: Windows has neither a gid nor anything to look` |
|      - | 2069 | `		 * one up in, and php's own build there keeps the function and answers a` |
|      - | 2070 | `		 * SILENT false for it -- asked of php 8.5.8 on the gate guest. */` |
|      1 | 2071 | `		ph7_result_bool(pCtx,0);` |
|      1 | 2072 | `		return PH7_OK;` |
|      - | 2073 | `	}` |
|      - | 2074 | `	/* Point to the desired directory */` |
|      6 | 2075 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2076 | `	/* Extract the user */` |
|      6 | 2077 | `	zGroup = ph7_value_to_string(apArg[1],0);` |
|      - | 2078 | `	/* Perform the requested operation */` |
|      6 | 2079 | `	errno = 0;` |
|      6 | 2080 | `	rc = pVfs->xChgrp(zPath,zGroup);` |
|      6 | 2081 | `	if( rc != PH7_OK ){` |
|      - | 2082 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - | 2083 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - | 2084 | `		 * "chown(): Operation not permitted". */` |
|      6 | 2085 | `		if( rc == -2 ){` |
|      3 | 2086 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find gid for %s",` |
|      1 | 2087 | `				ph7_function_name(pCtx),zGroup);` |
|      1 | 2088 | `		}else{` |
|      6 | 2089 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 | 2090 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - | 2091 | `		}` |
|      3 | 2092 | `	}` |
|      - | 2093 | `	/* IO return value */` |
|      6 | 2094 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 | 2095 | `	return PH7_OK;` |
|     10 | 2096 | `}` |
|      - | 2097 | `/*` |
|      - | 2098 | ` * int64 disk_free_space(string $directory)` |
|      - | 2099 | ` *  Returns available space on filesystem or disk partition.` |
|      - | 2100 | ` * Parameters` |
|      - | 2101 | ` *  $directory` |
|      - | 2102 | ` *   A directory of the filesystem or disk partition.` |
|      - | 2103 | ` * Return` |
|      - | 2104 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - | 2105 | ` */` |
|     16 | 2106 | `static int PH7_vfs_disk_free_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2107 | `{` |
|      - | 2108 | `	const char *zPath;` |
|      - | 2109 | `	ph7_int64 iSize;` |
|      - | 2110 | `	ph7_vfs *pVfs;` |
|     17 | 2111 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2112 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2113 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2114 | `		return PH7_OK;` |
|      - | 2115 | `	}` |
|      - | 2116 | `	/* Point to the underlying vfs */` |
|     17 | 2117 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     17 | 2118 | `	if( pVfs == 0 \|\| pVfs->xFreeSpace == 0 ){` |
|      - | 2119 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2120 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2121 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2122 | `			ph7_function_name(pCtx)` |
|      - | 2123 | `			);` |
|    ! 0 | 2124 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2125 | `		return PH7_OK;` |
|      - | 2126 | `	}` |
|      - | 2127 | `	/* Point to the desired directory */` |
|     17 | 2128 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|     17 | 2129 | `	if( zPath == 0 \|\| zPath[0] == 0 ){` |
|      - | 2130 | `		/* php answers FALSE for an empty one and says nothing, as the stat` |
|      - | 2131 | `		 * family does. */` |
|      2 | 2132 | `		ph7_result_bool(pCtx,0);` |
|      2 | 2133 | `		return PH7_OK;` |
|      - | 2134 | `	}` |
|      - | 2135 | `	/* Perform the requested operation */` |
|     15 | 2136 | `	errno = 0;` |
|     15 | 2137 | `	iSize = pVfs->xFreeSpace(zPath);` |
|     15 | 2138 | `	if( iSize < 0 ){` |
|      - | 2139 | `		/* php answers FALSE and warns with the C library's own reason` |
|      - | 2140 | ``		 * (`disk_free_space(): No such file or directory`). PHL answered int(-1) --`` |
|      - | 2141 | `		 * truthy, and a plausible-looking byte count for a caller that only tests` |
|      - | 2142 | ``		 * `if ($free)` or compares it against a threshold. */`` |
|      7 | 2143 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 | 2144 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|      5 | 2145 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2146 | `		return PH7_OK;` |
|      - | 2147 | `	}` |
|      - | 2148 | `	/* php's return type is FLOAT, not int: the value can exceed what an int holds` |
|      - | 2149 | ``	 * on a large volume, and a `===`/gettype()/json_encode() reader sees the`` |
|      - | 2150 | `	 * difference on every volume. */` |
|     11 | 2151 | `	ph7_result_double(pCtx,(double)iSize);` |
|     11 | 2152 | `	return PH7_OK;` |
|      9 | 2153 | `}` |
|      - | 2154 | `/*` |
|      - | 2155 | ` * int64 disk_total_space(string $directory)` |
|      - | 2156 | ` *  Returns the total size of a filesystem or disk partition.` |
|      - | 2157 | ` * Parameters` |
|      - | 2158 | ` *  $directory` |
|      - | 2159 | ` *   A directory of the filesystem or disk partition.` |
|      - | 2160 | ` * Return` |
|      - | 2161 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - | 2162 | ` */` |
|     10 | 2163 | `static int PH7_vfs_disk_total_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2164 | `{` |
|      - | 2165 | `	const char *zPath;` |
|      - | 2166 | `	ph7_int64 iSize;` |
|      - | 2167 | `	ph7_vfs *pVfs;` |
|     11 | 2168 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2169 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2170 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2171 | `		return PH7_OK;` |
|      - | 2172 | `	}` |
|      - | 2173 | `	/* Point to the underlying vfs */` |
|     11 | 2174 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 2175 | `	if( pVfs == 0 \|\| pVfs->xTotalSpace == 0 ){` |
|      - | 2176 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2177 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2178 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2179 | `			ph7_function_name(pCtx)` |
|      - | 2180 | `			);` |
|    ! 0 | 2181 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2182 | `		return PH7_OK;` |
|      - | 2183 | `	}` |
|      - | 2184 | `	/* Point to the desired directory */` |
|     11 | 2185 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2186 | `	/* Perform the requested operation */` |
|     11 | 2187 | `	errno = 0;` |
|     11 | 2188 | `	iSize = pVfs->xTotalSpace(zPath);` |
|     11 | 2189 | `	if( iSize < 0 ){` |
|      - | 2190 | `		/* php answers FALSE and warns with the C library's own reason` |
|      - | 2191 | ``		 * (`disk_free_space(): No such file or directory`). PHL answered int(-1) --`` |
|      - | 2192 | `		 * truthy, and a plausible-looking byte count for a caller that only tests` |
|      - | 2193 | ``		 * `if ($free)` or compares it against a threshold. */`` |
|      4 | 2194 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      2 | 2195 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|      3 | 2196 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2197 | `		return PH7_OK;` |
|      - | 2198 | `	}` |
|      - | 2199 | `	/* php's return type is FLOAT, not int: the value can exceed what an int holds` |
|      - | 2200 | ``	 * on a large volume, and a `===`/gettype()/json_encode() reader sees the`` |
|      - | 2201 | `	 * difference on every volume. */` |
|      9 | 2202 | `	ph7_result_double(pCtx,(double)iSize);` |
|      9 | 2203 | `	return PH7_OK;` |
|      6 | 2204 | `}` |
|      - | 2205 | `/*` |
|      - | 2206 | ` * bool file_exists(string $filename)` |
|      - | 2207 | ` *  Checks whether a file or directory exists.` |
|      - | 2208 | ` * Parameters` |
|      - | 2209 | ` *  $filename` |
|      - | 2210 | ` *   Path to the file.` |
|      - | 2211 | ` * Return` |
|      - | 2212 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2213 | ` */` |
|   1001 | 2214 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2215 | `{` |
|      - | 2216 | `	const char *zPath;` |
|      - | 2217 | `	ph7_vfs *pVfs;` |
|      - | 2218 | `	int rc;` |
|   1006 | 2219 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2220 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2221 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2222 | `		return PH7_OK;` |
|      - | 2223 | `	}` |
|      - | 2224 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2225 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|   1006 | 2226 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_EXISTS) ){` |
|     25 | 2227 | `		return PH7_OK;` |
|      - | 2228 | `	}` |
|    982 | 2229 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    982 | 2230 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - | 2231 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2232 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2233 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2234 | `			ph7_function_name(pCtx)` |
|      - | 2235 | `			);` |
|    ! 0 | 2236 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2237 | `		return PH7_OK;` |
|      - | 2238 | `	}` |
|      - | 2239 | `	/* Point to the desired directory */` |
|    982 | 2240 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2241 | `	/* Perform the requested operation */` |
|    982 | 2242 | `	rc = pVfs->xFileExists(zPath);` |
|      - | 2243 | `	/* IO return value */` |
|    982 | 2244 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    982 | 2245 | `	return PH7_OK;` |
|    504 | 2246 | `}` |
|      - | 2247 | `/*` |
|      - | 2248 | ` * int64 file_size(string $filename)` |
|      - | 2249 | ` *  Gets the size for the given file.` |
|      - | 2250 | ` * Parameters` |
|      - | 2251 | ` *  $filename` |
|      - | 2252 | ` *   Path to the file.` |
|      - | 2253 | ` * Return` |
|      - | 2254 | ` *  File size on success or FALSE on failure.` |
|      - | 2255 | ` */` |
|     64 | 2256 | `static int PH7_vfs_file_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2257 | `{` |
|      - | 2258 | `	const char *zPath;` |
|      - | 2259 | `	ph7_int64 iSize;` |
|      - | 2260 | `	ph7_vfs *pVfs;` |
|     68 | 2261 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2262 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2263 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2264 | `		return PH7_OK;` |
|      - | 2265 | `	}` |
|      - | 2266 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2267 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     68 | 2268 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_SIZE) ){` |
|     23 | 2269 | `		return PH7_OK;` |
|      - | 2270 | `	}` |
|     46 | 2271 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     46 | 2272 | `	if( pVfs == 0 \|\| pVfs->xFileSize == 0 ){` |
|      - | 2273 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2274 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2275 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2276 | `			ph7_function_name(pCtx)` |
|      - | 2277 | `			);` |
|    ! 0 | 2278 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2279 | `		return PH7_OK;` |
|      - | 2280 | `	}` |
|      - | 2281 | `	/* Point to the desired directory */` |
|     46 | 2282 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2283 | `	/* Perform the requested operation */` |
|     46 | 2284 | `	iSize = pVfs->xFileSize(zPath);` |
|     46 | 2285 | `	if( iSize < 0 ){` |
|      - | 2286 | `		/* php: a stat failure warns and returns FALSE -- PH7 returned int(-1), which is` |
|      - | 2287 | `		 * truthy and compares equal to nothing a caller would test for. An EMPTY` |
|      - | 2288 | `		 * path is the family's silence (VfsThrowStatWarning). */` |
|      8 | 2289 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      8 | 2290 | `		ph7_result_bool(pCtx,0);` |
|      8 | 2291 | `		return PH7_OK;` |
|      - | 2292 | `	}` |
|      - | 2293 | `	/* IO return value */` |
|     39 | 2294 | `	ph7_result_int64(pCtx,iSize);` |
|     39 | 2295 | `	return PH7_OK;` |
|     36 | 2296 | `}` |
|      - | 2297 | `/*` |
|      - | 2298 | ` * int64 fileatime(string $filename)` |
|      - | 2299 | ` *  Gets the last access time of the given file.` |
|      - | 2300 | ` * Parameters` |
|      - | 2301 | ` *  $filename` |
|      - | 2302 | ` *   Path to the file.` |
|      - | 2303 | ` * Return` |
|      - | 2304 | ` *  File atime on success or FALSE on failure.` |
|      - | 2305 | ` */` |
|     26 | 2306 | `static int PH7_vfs_file_atime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2307 | `{` |
|      - | 2308 | `	const char *zPath;` |
|      - | 2309 | `	ph7_int64 iTime;` |
|      - | 2310 | `	ph7_vfs *pVfs;` |
|     27 | 2311 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2312 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2313 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2314 | `		return PH7_OK;` |
|      - | 2315 | `	}` |
|      - | 2316 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2317 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     27 | 2318 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_ATIME) ){` |
|     17 | 2319 | `		return PH7_OK;` |
|      - | 2320 | `	}` |
|     11 | 2321 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 2322 | `	if( pVfs == 0 \|\| pVfs->xFileAtime == 0 ){` |
|      - | 2323 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2324 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2325 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2326 | `			ph7_function_name(pCtx)` |
|      - | 2327 | `			);` |
|    ! 0 | 2328 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2329 | `		return PH7_OK;` |
|      - | 2330 | `	}` |
|      - | 2331 | `	/* Point to the desired directory */` |
|     11 | 2332 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2333 | `	/* Perform the requested operation */` |
|     11 | 2334 | `	iTime = pVfs->xFileAtime(zPath);` |
|     11 | 2335 | `	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){` |
|      - | 2336 | `		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --` |
|      - | 2337 | `		 * truthy, and one second before the epoch is also a real timestamp, which` |
|      - | 2338 | `		 * is why the verdict comes from the VFS rather than from the value. */` |
|      5 | 2339 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      5 | 2340 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2341 | `		return PH7_OK;` |
|      - | 2342 | `	}` |
|      - | 2343 | `	/* IO return value */` |
|      7 | 2344 | `	ph7_result_int64(pCtx,iTime);` |
|      7 | 2345 | `	return PH7_OK;` |
|     14 | 2346 | `}` |
|      - | 2347 | `/*` |
|      - | 2348 | ` * int64 filemtime(string $filename)` |
|      - | 2349 | ` *  Gets file modification time.` |
|      - | 2350 | ` * Parameters` |
|      - | 2351 | ` *  $filename` |
|      - | 2352 | ` *   Path to the file.` |
|      - | 2353 | ` * Return` |
|      - | 2354 | ` *  File mtime on success or FALSE on failure.` |
|      - | 2355 | ` */` |
|     46 | 2356 | `static int PH7_vfs_file_mtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2357 | `{` |
|      - | 2358 | `	const char *zPath;` |
|      - | 2359 | `	ph7_int64 iTime;` |
|      - | 2360 | `	ph7_vfs *pVfs;` |
|     49 | 2361 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2362 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2363 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2364 | `		return PH7_OK;` |
|      - | 2365 | `	}` |
|      - | 2366 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2367 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     49 | 2368 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_MTIME) ){` |
|     17 | 2369 | `		return PH7_OK;` |
|      - | 2370 | `	}` |
|     33 | 2371 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     33 | 2372 | `	if( pVfs == 0 \|\| pVfs->xFileMtime == 0 ){` |
|      - | 2373 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2374 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2375 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2376 | `			ph7_function_name(pCtx)` |
|      - | 2377 | `			);` |
|    ! 0 | 2378 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2379 | `		return PH7_OK;` |
|      - | 2380 | `	}` |
|      - | 2381 | `	/* Point to the desired directory */` |
|     33 | 2382 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2383 | `	/* Perform the requested operation */` |
|     33 | 2384 | `	iTime = pVfs->xFileMtime(zPath);` |
|     33 | 2385 | `	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){` |
|      - | 2386 | `		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --` |
|      - | 2387 | `		 * truthy, and one second before the epoch is also a real timestamp, which` |
|      - | 2388 | `		 * is why the verdict comes from the VFS rather than from the value. */` |
|      3 | 2389 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      3 | 2390 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2391 | `		return PH7_OK;` |
|      - | 2392 | `	}` |
|      - | 2393 | `	/* IO return value */` |
|     31 | 2394 | `	ph7_result_int64(pCtx,iTime);` |
|     31 | 2395 | `	return PH7_OK;` |
|     26 | 2396 | `}` |
|      - | 2397 | `/*` |
|      - | 2398 | ` * int64 filectime(string $filename)` |
|      - | 2399 | ` *  Gets inode change time of file.` |
|      - | 2400 | ` * Parameters` |
|      - | 2401 | ` *  $filename` |
|      - | 2402 | ` *   Path to the file.` |
|      - | 2403 | ` * Return` |
|      - | 2404 | ` *  File ctime on success or FALSE on failure.` |
|      - | 2405 | ` */` |
|     22 | 2406 | `static int PH7_vfs_file_ctime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2407 | `{` |
|      - | 2408 | `	const char *zPath;` |
|      - | 2409 | `	ph7_int64 iTime;` |
|      - | 2410 | `	ph7_vfs *pVfs;` |
|     23 | 2411 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2412 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2413 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2414 | `		return PH7_OK;` |
|      - | 2415 | `	}` |
|      - | 2416 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2417 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     23 | 2418 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_CTIME) ){` |
|     17 | 2419 | `		return PH7_OK;` |
|      - | 2420 | `	}` |
|      7 | 2421 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 2422 | `	if( pVfs == 0 \|\| pVfs->xFileCtime == 0 ){` |
|      - | 2423 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2424 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2425 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2426 | `			ph7_function_name(pCtx)` |
|      - | 2427 | `			);` |
|    ! 0 | 2428 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2429 | `		return PH7_OK;` |
|      - | 2430 | `	}` |
|      - | 2431 | `	/* Point to the desired directory */` |
|      7 | 2432 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2433 | `	/* Perform the requested operation */` |
|      7 | 2434 | `	iTime = pVfs->xFileCtime(zPath);` |
|      7 | 2435 | `	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){` |
|      - | 2436 | `		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --` |
|      - | 2437 | `		 * truthy, and one second before the epoch is also a real timestamp, which` |
|      - | 2438 | `		 * is why the verdict comes from the VFS rather than from the value. */` |
|      3 | 2439 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      3 | 2440 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2441 | `		return PH7_OK;` |
|      - | 2442 | `	}` |
|      - | 2443 | `	/* IO return value */` |
|      5 | 2444 | `	ph7_result_int64(pCtx,iTime);` |
|      5 | 2445 | `	return PH7_OK;` |
|     12 | 2446 | `}` |
|      - | 2447 | `/*` |
|      - | 2448 | ` * bool is_file(string $filename)` |
|      - | 2449 | ` *  Tells whether the filename is a regular file.` |
|      - | 2450 | ` * Parameters` |
|      - | 2451 | ` *  $filename` |
|      - | 2452 | ` *   Path to the file.` |
|      - | 2453 | ` * Return` |
|      - | 2454 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2455 | ` */` |
|  10134 | 2456 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2457 | `{` |
|      - | 2458 | `	const char *zPath;` |
|      - | 2459 | `	ph7_vfs *pVfs;` |
|      - | 2460 | `	int rc;` |
|  10139 | 2461 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2462 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2463 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2464 | `		return PH7_OK;` |
|      - | 2465 | `	}` |
|      - | 2466 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2467 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|  10139 | 2468 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_IS_FILE) ){` |
|     23 | 2469 | `		return PH7_OK;` |
|      - | 2470 | `	}` |
|  10117 | 2471 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  10117 | 2472 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - | 2473 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2474 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2475 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2476 | `			ph7_function_name(pCtx)` |
|      - | 2477 | `			);` |
|    ! 0 | 2478 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2479 | `		return PH7_OK;` |
|      - | 2480 | `	}` |
|      - | 2481 | `	/* Point to the desired directory */` |
|  10117 | 2482 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2483 | `	/* Perform the requested operation */` |
|  10117 | 2484 | `	rc = pVfs->xIsfile(zPath);` |
|      - | 2485 | `	/* IO return value */` |
|  10117 | 2486 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  10117 | 2487 | `	return PH7_OK;` |
|   5072 | 2488 | `}` |
|      - | 2489 | `/*` |
|      - | 2490 | ` * bool is_link(string $filename)` |
|      - | 2491 | ` *  Tells whether the filename is a symbolic link.` |
|      - | 2492 | ` * Parameters` |
|      - | 2493 | ` *  $filename` |
|      - | 2494 | ` *   Path to the file.` |
|      - | 2495 | ` * Return` |
|      - | 2496 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2497 | ` */` |
|     30 | 2498 | `static int PH7_vfs_is_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2499 | `{` |
|      - | 2500 | `	const char *zPath;` |
|      - | 2501 | `	ph7_vfs *pVfs;` |
|      - | 2502 | `	int rc;` |
|     31 | 2503 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2504 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2505 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2506 | `		return PH7_OK;` |
|      - | 2507 | `	}` |
|      - | 2508 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2509 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     31 | 2510 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_IS_LINK) ){` |
|     19 | 2511 | `		return PH7_OK;` |
|      - | 2512 | `	}` |
|     12 | 2513 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     12 | 2514 | `	if( pVfs == 0 \|\| pVfs->xIslink == 0 ){` |
|      - | 2515 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2516 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2517 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2518 | `			ph7_function_name(pCtx)` |
|      - | 2519 | `			);` |
|    ! 0 | 2520 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2521 | `		return PH7_OK;` |
|      - | 2522 | `	}` |
|      - | 2523 | `	/* Point to the desired directory */` |
|     12 | 2524 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2525 | `	/* Perform the requested operation */` |
|     12 | 2526 | `	rc = pVfs->xIslink(zPath);` |
|      - | 2527 | `	/* IO return value */` |
|     12 | 2528 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     12 | 2529 | `	return PH7_OK;` |
|     16 | 2530 | `}` |
|      - | 2531 | `/*` |
|      - | 2532 | ` * bool is_readable(string $filename)` |
|      - | 2533 | ` *  Tells whether a file exists and is readable.` |
|      - | 2534 | ` * Parameters` |
|      - | 2535 | ` *  $filename` |
|      - | 2536 | ` *   Path to the file.` |
|      - | 2537 | ` * Return` |
|      - | 2538 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2539 | ` */` |
|     20 | 2540 | `static int PH7_vfs_is_readable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2541 | `{` |
|      - | 2542 | `	const char *zPath;` |
|      - | 2543 | `	ph7_vfs *pVfs;` |
|      - | 2544 | `	int rc;` |
|     21 | 2545 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2546 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2547 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2548 | `		return PH7_OK;` |
|      - | 2549 | `	}` |
|      - | 2550 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2551 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     21 | 2552 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_IS_R) ){` |
|     17 | 2553 | `		return PH7_OK;` |
|      - | 2554 | `	}` |
|      5 | 2555 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 2556 | `	if( pVfs == 0 \|\| pVfs->xReadable == 0 ){` |
|      - | 2557 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2558 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2559 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2560 | `			ph7_function_name(pCtx)` |
|      - | 2561 | `			);` |
|    ! 0 | 2562 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2563 | `		return PH7_OK;` |
|      - | 2564 | `	}` |
|      - | 2565 | `	/* Point to the desired directory */` |
|      5 | 2566 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2567 | `	/* Perform the requested operation */` |
|      5 | 2568 | `	rc = pVfs->xReadable(zPath);` |
|      - | 2569 | `	/* IO return value */` |
|      5 | 2570 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 | 2571 | `	return PH7_OK;` |
|     11 | 2572 | `}` |
|      - | 2573 | `/*` |
|      - | 2574 | ` * bool is_writable(string $filename)` |
|      - | 2575 | ` *  Tells whether the filename is writable.` |
|      - | 2576 | ` * Parameters` |
|      - | 2577 | ` *  $filename` |
|      - | 2578 | ` *   Path to the file.` |
|      - | 2579 | ` * Return` |
|      - | 2580 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2581 | ` */` |
|    633 | 2582 | `static int PH7_vfs_is_writable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2583 | `{` |
|      - | 2584 | `	const char *zPath;` |
|      - | 2585 | `	ph7_vfs *pVfs;` |
|      - | 2586 | `	int rc;` |
|    638 | 2587 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2588 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2589 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2590 | `		return PH7_OK;` |
|      - | 2591 | `	}` |
|      - | 2592 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2593 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|    638 | 2594 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_IS_W) ){` |
|     17 | 2595 | `		return PH7_OK;` |
|      - | 2596 | `	}` |
|    622 | 2597 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    622 | 2598 | `	if( pVfs == 0 \|\| pVfs->xWritable == 0 ){` |
|      - | 2599 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2600 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2601 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2602 | `			ph7_function_name(pCtx)` |
|      - | 2603 | `			);` |
|    ! 0 | 2604 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2605 | `		return PH7_OK;` |
|      - | 2606 | `	}` |
|      - | 2607 | `	/* Point to the desired directory */` |
|    622 | 2608 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2609 | `	/* Perform the requested operation */` |
|    622 | 2610 | `	rc = pVfs->xWritable(zPath);` |
|      - | 2611 | `	/* IO return value */` |
|    622 | 2612 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    622 | 2613 | `	return PH7_OK;` |
|    321 | 2614 | `}` |
|      - | 2615 | `/*` |
|      - | 2616 | ` * bool is_executable(string $filename)` |
|      - | 2617 | ` *  Tells whether the filename is executable.` |
|      - | 2618 | ` * Parameters` |
|      - | 2619 | ` *  $filename` |
|      - | 2620 | ` *   Path to the file.` |
|      - | 2621 | ` * Return` |
|      - | 2622 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2623 | ` */` |
|     18 | 2624 | `static int PH7_vfs_is_executable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2625 | `{` |
|      - | 2626 | `	const char *zPath;` |
|      - | 2627 | `	ph7_vfs *pVfs;` |
|      - | 2628 | `	int rc;` |
|     20 | 2629 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2630 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2631 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2632 | `		return PH7_OK;` |
|      - | 2633 | `	}` |
|      - | 2634 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2635 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     20 | 2636 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_IS_X) ){` |
|     17 | 2637 | `		return PH7_OK;` |
|      - | 2638 | `	}` |
|      4 | 2639 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 | 2640 | `	if( pVfs == 0 \|\| pVfs->xExecutable == 0 ){` |
|      - | 2641 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2642 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2643 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2644 | `			ph7_function_name(pCtx)` |
|      - | 2645 | `			);` |
|    ! 0 | 2646 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2647 | `		return PH7_OK;` |
|      - | 2648 | `	}` |
|      - | 2649 | `	/* Point to the desired directory */` |
|      4 | 2650 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2651 | `	/* Perform the requested operation */` |
|      4 | 2652 | `	rc = pVfs->xExecutable(zPath);` |
|      - | 2653 | `	/* IO return value */` |
|      4 | 2654 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 | 2655 | `	return PH7_OK;` |
|     11 | 2656 | `}` |
|      - | 2657 | `/*` |
|      - | 2658 | ` * string filetype(string $filename)` |
|      - | 2659 | ` *  Gets file type.` |
|      - | 2660 | ` * Parameters` |
|      - | 2661 | ` *  $filename` |
|      - | 2662 | ` *   Path to the file.` |
|      - | 2663 | ` * Return` |
|      - | 2664 | ` *  The type of the file. Possible values are fifo, char, dir, block, link` |
|      - | 2665 | ` *  file, socket and unknown.` |
|      - | 2666 | ` */` |
|     40 | 2667 | `static int PH7_vfs_filetype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2668 | `{` |
|      - | 2669 | `	const char *zPath;` |
|      - | 2670 | `	ph7_vfs *pVfs;` |
|     41 | 2671 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2672 | `		/* Missing/Invalid argument,return 'unknown' */` |
|    ! 0 | 2673 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 2674 | `		return PH7_OK;` |
|      - | 2675 | `	}` |
|      - | 2676 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2677 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     41 | 2678 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_TYPE) ){` |
|     19 | 2679 | `		return PH7_OK;` |
|      - | 2680 | `	}` |
|     23 | 2681 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     23 | 2682 | `	if( pVfs == 0 \|\| pVfs->xFiletype == 0 ){` |
|      - | 2683 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2684 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2685 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2686 | `			ph7_function_name(pCtx)` |
|      - | 2687 | `			);` |
|    ! 0 | 2688 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2689 | `		return PH7_OK;` |
|      - | 2690 | `	}` |
|      - | 2691 | `	/* Point to the desired directory */` |
|     23 | 2692 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2693 | `	/* Set the empty string as the default return value */` |
|     23 | 2694 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2695 | `	/* Perform the requested operation */` |
|     23 | 2696 | `	if( pVfs->xFiletype(zPath,pCtx) != PH7_OK ){` |
|      - | 2697 | `		/* php LSTATs here (which is why a symlink answers "link") and a failure is` |
|      - | 2698 | ``		 * the `Lstat failed for` warning plus FALSE. PHL answered the string`` |
|      - | 2699 | `		 * "unknown" -- a real return value of this function, so a caller could not` |
|      - | 2700 | `		 * tell a missing path from a socket or a fifo. */` |
|      5 | 2701 | `		VfsThrowStatWarning(pCtx,zPath,1);` |
|      5 | 2702 | `		ph7_result_bool(pCtx,0);` |
|      2 | 2703 | `	}` |
|     23 | 2704 | `	return PH7_OK;` |
|     21 | 2705 | `}` |
|      - | 2706 | `/*` |
|      - | 2707 | ` * array stat(string $filename)` |
|      - | 2708 | ` *  Gives information about a file.` |
|      - | 2709 | ` * Parameters` |
|      - | 2710 | ` *  $filename` |
|      - | 2711 | ` *   Path to the file.` |
|      - | 2712 | ` * Return` |
|      - | 2713 | ` *  An associative array on success holding the following entries on success` |
|      - | 2714 | ` *  0   dev     device number` |
|      - | 2715 | ` * 1    ino     inode number (zero on windows)` |
|      - | 2716 | ` * 2    mode    inode protection mode` |
|      - | 2717 | ` * 3    nlink   number of links` |
|      - | 2718 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 2719 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 2720 | ` * 6    rdev    device type, if inode device` |
|      - | 2721 | ` * 7    size    size in bytes` |
|      - | 2722 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 2723 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 2724 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 2725 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 2726 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 2727 | ` * Note:` |
|      - | 2728 | ` *  FALSE is returned on failure.` |
|      - | 2729 | ` */` |
|     32 | 2730 | `static int PH7_vfs_stat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2731 | `{` |
|      - | 2732 | `	ph7_value *pArray,*pValue;` |
|      - | 2733 | `	const char *zPath;` |
|      - | 2734 | `	ph7_vfs *pVfs;` |
|      - | 2735 | `	int rc;` |
|     34 | 2736 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2737 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2738 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2739 | `		return PH7_OK;` |
|      - | 2740 | `	}` |
|      - | 2741 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2742 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     34 | 2743 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_STAT) ){` |
|      7 | 2744 | `		return PH7_OK;` |
|      - | 2745 | `	}` |
|     28 | 2746 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     28 | 2747 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|      - | 2748 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2749 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2750 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2751 | `			ph7_function_name(pCtx)` |
|      - | 2752 | `			);` |
|    ! 0 | 2753 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2754 | `		return PH7_OK;` |
|      - | 2755 | `	}` |
|      - | 2756 | `	/* Create the array and the working value */` |
|     28 | 2757 | `	pArray = ph7_context_new_array(pCtx);` |
|     28 | 2758 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     28 | 2759 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 2760 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 2761 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2762 | `		return PH7_OK;` |
|      - | 2763 | `	}` |
|      - | 2764 | `	/* Extract the file path */` |
|     28 | 2765 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2766 | `	/* Perform the requested operation */` |
|     28 | 2767 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|     28 | 2768 | `	if( rc != PH7_OK ){` |
|      - | 2769 | ``		/* php warns before answering FALSE -- the same `stat failed for` /`` |
|      - | 2770 | ``		 * `Lstat failed for` line the rest of the family raises. PHL returned the`` |
|      - | 2771 | `		 * FALSE in silence, so a missing path and an empty result looked alike. */` |
|      8 | 2772 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      8 | 2773 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2774 | `	}else{` |
|      - | 2775 | `		/* php's answer is the thirteen fields TWICE: numeric 0..12 then named. */` |
|     21 | 2776 | `		ph7_value *pFull = ph7_context_new_array(pCtx);` |
|     21 | 2777 | `		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){` |
|     21 | 2778 | `			ph7_result_value(pCtx,pFull);` |
|     11 | 2779 | `		}else{` |
|    ! 0 | 2780 | `			ph7_result_value(pCtx,pArray);` |
|      - | 2781 | `		}` |
|      - | 2782 | `	}` |
|      - | 2783 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 2784 | `	 * automatically as soon we return from this function. */` |
|     28 | 2785 | `	return PH7_OK;` |
|     18 | 2786 | `}` |
|      - | 2787 | `/*` |
|      - | 2788 | ` * array lstat(string $filename)` |
|      - | 2789 | ` *  Gives information about a file or symbolic link.` |
|      - | 2790 | ` * Parameters` |
|      - | 2791 | ` *  $filename` |
|      - | 2792 | ` *   Path to the file.` |
|      - | 2793 | ` * Return` |
|      - | 2794 | ` *  An associative array on success holding the following entries on success` |
|      - | 2795 | ` *  0   dev     device number` |
|      - | 2796 | ` * 1    ino     inode number (zero on windows)` |
|      - | 2797 | ` * 2    mode    inode protection mode` |
|      - | 2798 | ` * 3    nlink   number of links` |
|      - | 2799 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 2800 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 2801 | ` * 6    rdev    device type, if inode device` |
|      - | 2802 | ` * 7    size    size in bytes` |
|      - | 2803 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 2804 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 2805 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 2806 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 2807 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 2808 | ` * Note:` |
|      - | 2809 | ` *  FALSE is returned on failure.` |
|      - | 2810 | ` */` |
|     16 | 2811 | `static int PH7_vfs_lstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2812 | `{` |
|      - | 2813 | `	ph7_value *pArray,*pValue;` |
|      - | 2814 | `	const char *zPath;` |
|      - | 2815 | `	ph7_vfs *pVfs;` |
|      - | 2816 | `	int rc;` |
|     17 | 2817 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2818 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2819 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2820 | `		return PH7_OK;` |
|      - | 2821 | `	}` |
|      - | 2822 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2823 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     17 | 2824 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_LSTAT) ){` |
|      7 | 2825 | `		return PH7_OK;` |
|      - | 2826 | `	}` |
|     11 | 2827 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 2828 | `	if( pVfs == 0 \|\| pVfs->xlStat == 0 ){` |
|      - | 2829 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2830 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2831 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2832 | `			ph7_function_name(pCtx)` |
|      - | 2833 | `			);` |
|    ! 0 | 2834 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2835 | `		return PH7_OK;` |
|      - | 2836 | `	}` |
|      - | 2837 | `	/* Create the array and the working value */` |
|     11 | 2838 | `	pArray = ph7_context_new_array(pCtx);` |
|     11 | 2839 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     11 | 2840 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 2841 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 2842 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2843 | `		return PH7_OK;` |
|      - | 2844 | `	}` |
|      - | 2845 | `	/* Extract the file path */` |
|     11 | 2846 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2847 | `	/* Perform the requested operation */` |
|     11 | 2848 | `	rc = pVfs->xlStat(zPath,pArray,pValue);` |
|     11 | 2849 | `	if( rc != PH7_OK ){` |
|      - | 2850 | ``		/* php warns before answering FALSE -- the same `stat failed for` /`` |
|      - | 2851 | ``		 * `Lstat failed for` line the rest of the family raises. PHL returned the`` |
|      - | 2852 | `		 * FALSE in silence, so a missing path and an empty result looked alike. */` |
|      5 | 2853 | `		VfsThrowStatWarning(pCtx,zPath,1);` |
|      5 | 2854 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2855 | `	}else{` |
|      - | 2856 | `		/* php's answer is the thirteen fields TWICE: numeric 0..12 then named. */` |
|      7 | 2857 | `		ph7_value *pFull = ph7_context_new_array(pCtx);` |
|      7 | 2858 | `		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){` |
|      7 | 2859 | `			ph7_result_value(pCtx,pFull);` |
|      4 | 2860 | `		}else{` |
|    ! 0 | 2861 | `			ph7_result_value(pCtx,pArray);` |
|      - | 2862 | `		}` |
|      - | 2863 | `	}` |
|      - | 2864 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 2865 | `	 * automatically as soon we return from this function. */` |
|     11 | 2866 | `	return PH7_OK;` |
|      9 | 2867 | `}` |
|      - | 2868 | `/*` |
|      - | 2869 | ` * int\|false fileowner / filegroup / fileinode / fileperms (string $filename)` |
|      - | 2870 | ` *  One stat() with one of its fields taken out of it, which is exactly how php` |
|      - | 2871 | ` *  implements them (php_stat's FS_OWNER / FS_GROUP / FS_INODE / FS_PERMS arms).` |
|      - | 2872 | ` *` |
|      - | 2873 | ` * They were prelude PHP wrapping stat(), which cost them php's diagnostic twice` |
|      - | 2874 | ` * over: three of the four said NOTHING on a failed stat (the fourth raised its own` |
|      - | 2875 | `` * `trigger_error`, so its errno was E_USER_WARNING's 512 rather than E_WARNING's 2`` |
|      - | 2876 | ` * and its line was the prelude's, not the caller's). In C the family shares one` |
|      - | 2877 | ` * warning site with the rest of stat(), and the four get real signature rows.` |
|      - | 2878 | ` */` |
|     96 | 2879 | `static int VfsStatField(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zField,int eAsk)` |
|      2 | 2880 | `{` |
|      - | 2881 | `	ph7_value *pArray,*pValue,*pField;` |
|      - | 2882 | `	const char *zPath;` |
|      - | 2883 | `	ph7_vfs *pVfs;` |
|      - | 2884 | `	int rc;` |
|     98 | 2885 | `	if( nArg < 1 ){` |
|    ! 0 | 2886 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2887 | `		return PH7_OK;` |
|      - | 2888 | `	}` |
|      - | 2889 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2890 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     98 | 2891 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),eAsk) ){` |
|     49 | 2892 | `		return PH7_OK;` |
|      - | 2893 | `	}` |
|     50 | 2894 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     50 | 2895 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|    ! 0 | 2896 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2897 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2898 | `			ph7_function_name(pCtx)` |
|      - | 2899 | `			);` |
|    ! 0 | 2900 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2901 | `		return PH7_OK;` |
|      - | 2902 | `	}` |
|     50 | 2903 | `	pArray = ph7_context_new_array(pCtx);` |
|     50 | 2904 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     50 | 2905 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 2906 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 2907 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2908 | `		return PH7_OK;` |
|      - | 2909 | `	}` |
|     50 | 2910 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|     50 | 2911 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|     50 | 2912 | `	if( rc != PH7_OK ){` |
|     11 | 2913 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|     11 | 2914 | `		ph7_result_bool(pCtx,0);` |
|     11 | 2915 | `		return PH7_OK;` |
|      - | 2916 | `	}` |
|     40 | 2917 | `	pField = ph7_array_fetch(pArray,zField,-1);` |
|     40 | 2918 | `	if( pField == 0 ){` |
|      - | 2919 | `		/* The VFS answered a stat array without this field: nothing to report but` |
|      - | 2920 | `		 * the failure itself, which is what php answers when its own stat has no` |
|      - | 2921 | `		 * such member either. */` |
|    ! 0 | 2922 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2923 | `		return PH7_OK;` |
|      - | 2924 | `	}` |
|     40 | 2925 | `	ph7_result_int64(pCtx,ph7_value_to_int64(pField));` |
|     40 | 2926 | `	return PH7_OK;` |
|     50 | 2927 | `}` |
|     16 | 2928 | `static int PH7_vfs_file_owner(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2929 | `{` |
|     17 | 2930 | `	return VfsStatField(pCtx,nArg,apArg,"uid",PH7_STAT_ASK_OWNER);` |
|      1 | 2931 | `}` |
|     14 | 2932 | `static int PH7_vfs_file_group(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2933 | `{` |
|     15 | 2934 | `	return VfsStatField(pCtx,nArg,apArg,"gid",PH7_STAT_ASK_GROUP);` |
|      1 | 2935 | `}` |
|     22 | 2936 | `static int PH7_vfs_file_inode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2937 | `{` |
|     23 | 2938 | `	return VfsStatField(pCtx,nArg,apArg,"ino",PH7_STAT_ASK_INODE);` |
|      1 | 2939 | `}` |
|     44 | 2940 | `static int PH7_vfs_file_perms(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2941 | `{` |
|     46 | 2942 | `	return VfsStatField(pCtx,nArg,apArg,"mode",PH7_STAT_ASK_PERMS);` |
|      2 | 2943 | `}` |
|      - | 2944 | `/*` |
|      - | 2945 | ` * array\|string\|false getenv(?string $name = null, bool $local_only = false)` |
|      - | 2946 | ` *  Gets the value of an environment variable.` |
|      - | 2947 | ` * Parameters` |
|      - | 2948 | ` *  $name` |
|      - | 2949 | ` *   The variable name -- or NOTHING, which is the documented way to ask for the` |
|      - | 2950 | ` *   WHOLE environment as a name => value array. That form answered FALSE here,` |
|      - | 2951 | `` *   so `foreach (getenv() as $k => $v)` iterated over a bool.`` |
|      - | 2952 | ` *  $local_only` |
|      - | 2953 | ` *   Ask only the process's own environment rather than the SAPI's. On the CLI` |
|      - | 2954 | ` *   they are the same environment, so the argument selects the same answer --` |
|      - | 2955 | ` *   but it must still be ACCEPTED, and asking for the whole map with it set` |
|      - | 2956 | ` *   answered false too.` |
|      - | 2957 | ` * Return` |
|      - | 2958 | ` *  The value of the environment variable, or FALSE when it does not exist, or` |
|      - | 2959 | ` *  the whole environment when no name is given.` |
|      - | 2960 | ` */` |
|    306 | 2961 | `static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2962 | `{` |
|      - | 2963 | `	const char *zEnv;` |
|      - | 2964 | `	ph7_vfs *pVfs;` |
|      - | 2965 | `	int iLen;` |
|      - | 2966 | `	/* Point to the underlying vfs */` |
|    311 | 2967 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    311 | 2968 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|      - | 2969 | `		/* The whole environment. xEnviron was APPENDED to ph7_vfs, so an` |
|      - | 2970 | `		 * embedder VFS built against version 2 does not have the field at all --` |
|      - | 2971 | `		 * reading it would run off the end of their struct. */` |
|      8 | 2972 | `		if( pVfs == 0 \|\| pVfs->iVersion < 3 \|\| pVfs->xEnviron == 0` |
|      9 | 2973 | `		 \|\| pVfs->xEnviron(pCtx) != PH7_OK ){` |
|    ! 0 | 2974 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2975 | `		}` |
|      9 | 2976 | `		return PH7_OK;` |
|      - | 2977 | `	}` |
|    303 | 2978 | `	if( pVfs == 0 \|\| pVfs->xGetenv == 0 ){` |
|      - | 2979 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2980 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2981 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2982 | `			ph7_function_name(pCtx)` |
|      - | 2983 | `			);` |
|    ! 0 | 2984 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2985 | `		return PH7_OK;` |
|      - | 2986 | `	}` |
|      - | 2987 | `	/* Extract the environment variable */` |
|    303 | 2988 | `	zEnv = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 2989 | `	/* Set a boolean FALSE as the default return value */` |
|    303 | 2990 | `	ph7_result_bool(pCtx,0);` |
|    303 | 2991 | `	if( iLen < 1 ){` |
|      - | 2992 | `		/* Empty string */` |
|      3 | 2993 | `		return PH7_OK;` |
|      - | 2994 | `	}` |
|      - | 2995 | `	/* Perform the requested operation */` |
|    301 | 2996 | `	pVfs->xGetenv(zEnv,pCtx);` |
|    301 | 2997 | `	return PH7_OK;` |
|    158 | 2998 | `}` |
|      - | 2999 | `/*` |
|      - | 3000 | ` * bool putenv(string $settings)` |
|      - | 3001 | ` *  Set the value of an environment variable.` |
|      - | 3002 | ` * Parameters` |
|      - | 3003 | ` *  $setting` |
|      - | 3004 | ` *   The setting, like "FOO=BAR"` |
|      - | 3005 | ` * Return` |
|      - | 3006 | ` *  TRUE on success or FALSE on failure.` |
|      - | 3007 | ` */` |
|     81 | 3008 | `static int PH7_vfs_putenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3009 | `{` |
|      - | 3010 | `	const char *zName,*zValue;` |
|      - | 3011 | `	char *zSettings,*zEnd;` |
|      - | 3012 | `	ph7_vfs *pVfs;` |
|      - | 3013 | `	int iLen,rc;` |
|     85 | 3014 | `	if( nArg < 1 ){` |
|      - | 3015 | `		/* Missing argument,return FALSE */` |
|    ! 0 | 3016 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3017 | `		return PH7_OK;` |
|      - | 3018 | `	}` |
|      - | 3019 | `	/* Extract the setting variable. It is NOT required to already BE a string:` |
|      - | 3020 | ``	 * the declared parameter is `string $assignment`, so php coerces an int or a`` |
|      - | 3021 | `	 * __toString() object first, where PH7 answered false and did nothing. */` |
|     85 | 3022 | `	zSettings = (char *)ph7_value_to_string(apArg[0],&iLen);` |
|     85 | 3023 | `	if( iLen < 1 \|\| zSettings[0] == '=' ){` |
|      - | 3024 | `		/* php's whole validity rule: an empty assignment, or one with no name in` |
|      - | 3025 | `		 * front of the '='. Everything else is accepted. */` |
|      9 | 3026 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3027 | `			"putenv(): Argument #1 ($assignment) must have a valid syntax");` |
|      - | 3028 | `	}` |
|      - | 3029 | `	/* Parse the setting. php looks for the '=' with strchr(), so an embedded NUL` |
|      - | 3030 | `	 * ENDS the search: putenv("FO\0O=BAR") finds no '=' at all and removes the` |
|      - | 3031 | `	 * variable named "FO" instead of setting one. */` |
|     77 | 3032 | `	zEnd = &zSettings[iLen];` |
|     77 | 3033 | `	zValue = 0;` |
|     77 | 3034 | `	zName = zSettings;` |
|    685 | 3035 | `	while( zSettings < zEnd && zSettings[0] != 0 ){` |
|    661 | 3036 | `		if( zSettings[0] == '=' ){` |
|      - | 3037 | `			/* Null terminate the name */` |
|     53 | 3038 | `			zSettings[0] = 0;` |
|     53 | 3039 | `			zValue = &zSettings[1];` |
|     53 | 3040 | `			break;` |
|      - | 3041 | `		}` |
|    612 | 3042 | `		zSettings++;` |
|      4 | 3043 | `	}` |
|      - | 3044 | ``	/* A missing '=' is not invalid syntax: `putenv("NAME")` REMOVES the variable,`` |
|      - | 3045 | `	 * which is the documented way to unset one, and PH7 read it as a failure and` |
|      - | 3046 | `	 * left the old value in place. An empty VALUE is a value too` |
|      - | 3047 | ``	 * (`putenv("NAME=")`), which the old `zValue >= zEnd` test rejected.`` |
|      - | 3048 | `	 * php does NOT touch $_ENV here: that array is the SAPI's startup snapshot,` |
|      - | 3049 | `	 * and a putenv() after it changes the process environment alone. PH7 wrote` |
|      - | 3050 | `	 * the pair into $_ENV as well, so a script could read back through $_ENV a` |
|      - | 3051 | `	 * variable php only exposes through getenv(). */` |
|      - | 3052 | `	/* Point to the underlying vfs */` |
|     77 | 3053 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     77 | 3054 | `	if( pVfs == 0 \|\| pVfs->xSetenv == 0 ){` |
|      - | 3055 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 3056 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3057 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 3058 | `			ph7_function_name(pCtx)` |
|      - | 3059 | `			);` |
|    ! 0 | 3060 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3061 | `		if( zValue ){` |
|    ! 0 | 3062 | `			zSettings[0] = '=';` |
|    ! 0 | 3063 | `		}` |
|    ! 0 | 3064 | `		return PH7_OK;` |
|      - | 3065 | `	}` |
|      - | 3066 | `	/* Perform the requested operation. A NULL value means REMOVE, and php reports` |
|      - | 3067 | `	 * TRUE for that whether or not the variable was there (or nameable) at all --` |
|      - | 3068 | `	 * only a failed SET is false. */` |
|     77 | 3069 | `	rc = pVfs->xSetenv(zName,zValue);` |
|     77 | 3070 | `	ph7_result_bool(pCtx,zValue == 0 \|\| rc == PH7_OK );` |
|     77 | 3071 | `	if( zValue ){` |
|      - | 3072 | `		/* Put back the '=' the name was terminated on. Without one, zSettings` |
|      - | 3073 | `		 * stopped on the terminator or on an embedded NUL, neither of which this` |
|      - | 3074 | `		 * routine wrote. */` |
|     53 | 3075 | `		zSettings[0] = '=';` |
|     17 | 3076 | `	}` |
|     77 | 3077 | `	return PH7_OK;` |
|     37 | 3078 | `}` |
|      - | 3079 | `/*` |
|      - | 3080 | ` * bool touch(string $filename[,int64 $time = time()[,int64 $atime]])` |
|      - | 3081 | ` *  Sets access and modification time of file.` |
|      - | 3082 | ` * Note: On windows` |
|      - | 3083 | ` *   If the file does not exists,it will not be created.` |
|      - | 3084 | ` * Parameters` |
|      - | 3085 | ` *  $filename` |
|      - | 3086 | ` *   The name of the file being touched.` |
|      - | 3087 | ` *  $time` |
|      - | 3088 | ` *   The touch time. If time is not supplied, the current system time is used.` |
|      - | 3089 | ` * $atime` |
|      - | 3090 | ` *   If present, the access time of the given filename is set to the value of atime.` |
|      - | 3091 | ` *   Otherwise, it is set to the value passed to the time parameter. If neither are` |
|      - | 3092 | ` *   present, the current system time is used.` |
|      - | 3093 | ` * Return` |
|      - | 3094 | ` *  TRUE on success or FALSE on failure.` |
|      - | 3095 | `*/` |
|     76 | 3096 | `static int PH7_vfs_touch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3097 | `{` |
|      - | 3098 | `	ph7_int64 nTime,nAccess;` |
|      - | 3099 | `	const char *zFile;` |
|      - | 3100 | `	ph7_vfs *pVfs;` |
|      - | 3101 | `	int rc;` |
|     79 | 3102 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 3103 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 3104 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3105 | `		return PH7_OK;` |
|      - | 3106 | `	}` |
|      - | 3107 | `	/* php's touch() is a stream_metadata(STREAM_META_TOUCH) on a wrapper path, and` |
|      - | 3108 | `	 * its value is an ARRAY: empty when the caller named no time at all, else` |
|      - | 3109 | `	 * [mtime, atime] -- resolved BEFORE php's own now/echo defaults, which belong` |
|      - | 3110 | `	 * to the plain-file path below. */` |
|      - | 3111 | `	{` |
|     79 | 3112 | `		ph7_value *pTimes = ph7_context_new_array(pCtx);` |
|      - | 3113 | `		ph7_value sOp,sOne;` |
|      - | 3114 | `		ph7_value *apExtra[2];` |
|      - | 3115 | `		int bDone;` |
|     79 | 3116 | `		if( pTimes == 0 ){` |
|    ! 0 | 3117 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3118 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3119 | `			return PH7_OK;` |
|      - | 3120 | `		}` |
|     79 | 3121 | `		PH7_MemObjInit(pCtx->pVm,&sOp);` |
|     79 | 3122 | `		PH7_MemObjInit(pCtx->pVm,&sOne);` |
|     79 | 3123 | `		ph7_value_int(&sOp,PH7_STREAM_META_TOUCH);` |
|     79 | 3124 | `		if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     23 | 3125 | `			ph7_int64 nM = ph7_value_to_int64(apArg[1]);` |
|     17 | 3126 | `			ph7_int64 nA = (nArg > 2 && !ph7_value_is_null(apArg[2]))` |
|     24 | 3127 | `				? ph7_value_to_int64(apArg[2]) : nM;` |
|     23 | 3128 | `			ph7_value_int64(&sOne,nM);` |
|     23 | 3129 | `			ph7_array_add_elem(pTimes,0,&sOne);` |
|     23 | 3130 | `			ph7_value_int64(&sOne,nA);` |
|     23 | 3131 | `			ph7_array_add_elem(pTimes,0,&sOne);` |
|     10 | 3132 | `		}` |
|     79 | 3133 | `		apExtra[0] = &sOp;` |
|     79 | 3134 | `		apExtra[1] = pTimes;` |
|    116 | 3135 | `		bDone = VfsUserWrite(pCtx,ph7_value_to_string(apArg[0],0),"stream_metadata",` |
|     37 | 3136 | `			0,apExtra,2);` |
|     79 | 3137 | `		PH7_MemObjRelease(&sOp);` |
|     79 | 3138 | `		PH7_MemObjRelease(&sOne);` |
|     79 | 3139 | `		if( bDone ){` |
|     24 | 3140 | `			return PH7_OK;` |
|      - | 3141 | `		}` |
|      - | 3142 | `	}` |
|      - | 3143 | `	/*` |
|      - | 3144 | `	 * A BUILT-IN wrapper has no stream_metadata to call, and php's touch() falls` |
|      - | 3145 | `	 * back to opening the url: it answers true for a name the wrapper has -- a` |
|      - | 3146 | ``	 * phar entry, a `data:` payload, `php://memory` -- and the open's own`` |
|      - | 3147 | `	 * sentence for one it does not. A userland wrapper without the method is NOT` |
|      - | 3148 | `	 * this case: php drops it to the plain-file path below, which reports it as` |
|      - | 3149 | `	 * a file it could not create.` |
|      - | 3150 | `	 */` |
|      - | 3151 | `	{` |
|     57 | 3152 | `		const char *zPath = ph7_value_to_string(apArg[0],0);` |
|     57 | 3153 | `		const char *zTail = zPath;` |
|     57 | 3154 | `		const ph7_io_stream *pStream = zPath` |
|     54 | 3155 | `			? PH7_VmGetStreamDevice(pCtx->pVm,&zTail,(int)SyStrlen(zPath)) : 0;` |
|     57 | 3156 | `		if( zPath && PH7_StreamWrapperLabel(pCtx->pVm,pStream) != 0 ){` |
|      - | 3157 | ``			/* php's fallback opens the url the way `c` does -- so a wrapper that`` |
|      - | 3158 | `			 * has the name answers true, one that cannot take a write-mode url` |
|      - | 3159 | ``			 * says so, and a `compress.zlib://` name is CREATED before its own`` |
|      - | 3160 | `			 * refusal. ext/phar is the one wrapper here with a touch handler of` |
|      - | 3161 | `			 * its own, and php's asks the entry the READING question. */` |
|     19 | 3162 | `			int iMode = PH7_PharStreamIs(pStream)` |
|     10 | 3163 | `				? PH7_IO_OPEN_RDONLY : PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|     27 | 3164 | `			void *pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zTail,` |
|      8 | 3165 | `				iMode,FALSE,0,FALSE,0,0);` |
|     19 | 3166 | `			if( pHandle ){` |
|     13 | 3167 | `				PH7_StreamCloseHandle(pStream,pHandle);` |
|     13 | 3168 | `				ph7_result_bool(pCtx,1);` |
|      6 | 3169 | `			}else{` |
|      7 | 3170 | `				VfsThrowOpenWarning(pCtx,zTail);` |
|      7 | 3171 | `				ph7_result_bool(pCtx,0);` |
|      - | 3172 | `			}` |
|     19 | 3173 | `			return PH7_OK;` |
|      - | 3174 | `		}` |
|     39 | 3175 | `		if( zPath && pStream == 0 ){` |
|      - | 3176 | `			/* A scheme nothing is registered under: php names it and then goes` |
|      - | 3177 | `			 * on to the plain-files path, which fails on the whole url. */` |
|      5 | 3178 | `			VfsThrowUnknownWrapperWarning(pCtx,zPath);` |
|      2 | 3179 | `		}` |
|      - | 3180 | `	}` |
|      - | 3181 | `	/* Point to the underlying vfs */` |
|     39 | 3182 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     39 | 3183 | `	if( pVfs == 0 \|\| pVfs->xTouch == 0 ){` |
|      - | 3184 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 3185 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3186 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 3187 | `			ph7_function_name(pCtx)` |
|      - | 3188 | `			);` |
|    ! 0 | 3189 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3190 | `		return PH7_OK;` |
|      - | 3191 | `	}` |
|      - | 3192 | `	/* Resolve php's defaults HERE, so the driver only ever sees real timestamps: a` |
|      - | 3193 | ``	 * NEGATIVE stamp is perfectly legal to php (`touch($f, -100)` is 1969), so it`` |
|      - | 3194 | `	 * cannot double as the "not given" sentinel the drivers used to read it as. An` |
|      - | 3195 | `	 * omitted/null $mtime is NOW; an omitted/null $atime follows $mtime. $atime also` |
|      - | 3196 | `	 * used to be read from apArg[1] — the mtime — so touch($f, $m, $a) silently` |
|      - | 3197 | `	 * stamped the modification time onto both. */` |
|     39 | 3198 | `	zFile = ph7_value_to_string(apArg[0],0);` |
|     39 | 3199 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) && nArg > 1 && ph7_value_is_null(apArg[1]) ){` |
|    ! 0 | 3200 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3201 | `			"touch(): Argument #2 ($mtime) cannot be null when argument #3 ($atime) "` |
|      - | 3202 | `			"is an integer");` |
|      - | 3203 | `	}` |
|     39 | 3204 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     13 | 3205 | `		nTime = ph7_value_to_int64(apArg[1]);` |
|      8 | 3206 | `	}else{` |
|      - | 3207 | `		time_t tNow;` |
|     28 | 3208 | `		time(&tNow);` |
|     28 | 3209 | `		nTime = (ph7_int64)tNow;` |
|      - | 3210 | `	}` |
|     39 | 3211 | `	nAccess = nTime;` |
|     39 | 3212 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      3 | 3213 | `		nAccess = ph7_value_to_int64(apArg[2]);` |
|      1 | 3214 | `	}` |
|     39 | 3215 | `	errno = 0;` |
|     39 | 3216 | `	rc = pVfs->xTouch(zFile,nTime,nAccess);` |
|     39 | 3217 | `	if( rc != PH7_OK ){` |
|      - | 3218 | `		/* php's own sentence for this one, which names both the path and the` |
|      - | 3219 | `		 * reason and reads like neither of the family's other two. */` |
|      7 | 3220 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to create file %s because %s",` |
|      4 | 3221 | `			ph7_function_name(pCtx),zFile ? zFile : "",VfsStrerror(errno));` |
|      2 | 3222 | `	}` |
|      - | 3223 | `	/* IO result */` |
|     39 | 3224 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     39 | 3225 | `	return PH7_OK;` |
|     40 | 3226 | `}` |
|      - | 3227 | `/*` |
|      - | 3228 | ` * Path processing functions that do not need access to the VFS layer` |
|      - | 3229 | ` * Status:` |
|      - | 3230 | ` *    Stable.` |
|      - | 3231 | ` */` |
|      - | 3232 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 3233 | `/*` |
|      - | 3234 | ` * string dirname(string $path)` |
|      - | 3235 |  |
|      - | 3236 | ` *  Returns parent directory's path.` |
|      - | 3237 | ` * Parameters` |
|      - | 3238 | ` * $path` |
|      - | 3239 | ` *  Target path.` |
|      - | 3240 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 3241 | ` *  In other environments, it is the forward slash (/).` |
|      - | 3242 | ` * Return` |
|      - | 3243 | ` *  The path of the parent directory. If there are no slashes in path, a dot ('.')` |
|      - | 3244 | ` *  is returned, indicating the current directory.` |
|      - | 3245 | ` */` |
|    132 | 3246 | `static int PH7_builtin_dirname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3247 | `{` |
|      - | 3248 | `	const char *zPath,*zDir;` |
|      - | 3249 | `	int iLen,iDirlen;` |
|    137 | 3250 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 3251 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 3252 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3253 | `		return PH7_OK;` |
|      - | 3254 | `	}` |
|      - | 3255 | `	/* Point to the target path */` |
|    137 | 3256 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|    137 | 3257 | `	if( iLen < 1 ){` |
|      - | 3258 | `		/* php answers "" for the empty path, not "." */` |
|      3 | 3259 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3260 | `		return PH7_OK;` |
|      - | 3261 | `	}` |
|      - | 3262 | `	/* $levels (php 7.0) was ACCEPTED AND IGNORED, so dirname($p, 3) silently answered` |
|      - | 3263 | `	 * the one-level parent — the caller's own answer, one or more levels too deep. Each` |
|      - | 3264 | `	 * level re-runs php_dirname on the previous result and stops as soon as the answer` |
|      - | 3265 | `	 * stops moving (the filesystem root, or "." for a relative path), which is what php` |
|      - | 3266 | `	 * does; php also rejects a level below 1 outright. */` |
|    135 | 3267 | `	zDir = zPath;` |
|    135 | 3268 | `	iDirlen = iLen;` |
|    135 | 3269 | `	if( nArg > 1 ){` |
|     51 | 3270 | `		ph7_int64 nLevels = ph7_value_to_int64(apArg[1]);` |
|      - | 3271 | `		ph7_int64 i;` |
|     51 | 3272 | `		if( nLevels < 1 ){` |
|      7 | 3273 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3274 | `				"dirname(): Argument #2 ($levels) must be greater than or equal to 1");` |
|      - | 3275 | `		}` |
|    125 | 3276 | `		for( i = 0 ; i < nLevels ; ++i ){` |
|    105 | 3277 | `			int iPrevLen = iDirlen;` |
|    105 | 3278 | `			const char *zPrev = zDir;` |
|    105 | 3279 | `			zDir = PH7_ExtractDirName(zPrev,iPrevLen,&iDirlen);` |
|    105 | 3280 | `			if( iDirlen == iPrevLen && SyMemcmp(zDir,zPrev,(sxu32)iDirlen) == 0 ){` |
|     25 | 3281 | `				break; /* fixed point: "/" and "." are their own parents */` |
|      - | 3282 | `			}` |
|     41 | 3283 | `		}` |
|     23 | 3284 | `	}else{` |
|     85 | 3285 | `		zDir = PH7_ExtractDirName(zPath,iLen,&iDirlen);` |
|      - | 3286 | `	}` |
|      - | 3287 | `	/* Return directory name */` |
|    129 | 3288 | `	ph7_result_string(pCtx,zDir,iDirlen);` |
|    129 | 3289 | `	return PH7_OK;` |
|     71 | 3290 | `}` |
|      - | 3291 | `/*` |
|      - | 3292 | ` * string basename(string $path[, string $suffix ])` |
|      - | 3293 | ` *  Returns trailing name component of path.` |
|      - | 3294 | ` * Parameters` |
|      - | 3295 | ` * $path` |
|      - | 3296 | ` *  Target path.` |
|      - | 3297 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 3298 | ` *  In other environments, it is the forward slash (/).` |
|      - | 3299 | ` * $suffix` |
|      - | 3300 | ` *  If the name component ends in suffix this will also be cut off.` |
|      - | 3301 | ` * Return` |
|      - | 3302 | ` *  The base name of the given path.` |
|      - | 3303 | ` */` |
|    232 | 3304 | `static int PH7_builtin_basename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3305 | `{` |
|      - | 3306 | `	const char *zPath,*zBase;` |
|      - | 3307 | `	int iLen,nBase;` |
|    237 | 3308 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 3309 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 3310 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3311 | `		return PH7_OK;` |
|      - | 3312 | `	}` |
|      - | 3313 | `	/* Point to the target path */` |
|    237 | 3314 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 3315 | `	/* php_basename, shared with pathinfo(): the hand-rolled walk this used to carry` |
|      - | 3316 | `	 * kept a leading separator on a single-component path (basename("/a") answered` |
|      - | 3317 | `	 * "/a", basename("/.") answered "/.") because it stopped one byte short. */` |
|    237 | 3318 | `	zBase = PH7_ExtractBaseName(zPath,iLen,&nBase);` |
|    237 | 3319 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 3320 | `		const char *zSuffix;` |
|      - | 3321 | `		int nSuffix;` |
|      - | 3322 | `		/* Strip suffix — php leaves the basename alone when it IS the suffix */` |
|      5 | 3323 | `		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);` |
|      4 | 3324 | `		if( nSuffix > 0 && nSuffix < nBase` |
|      5 | 3325 | `		 && SyMemcmp(&zBase[nBase - nSuffix],zSuffix,nSuffix) == 0 ){` |
|      5 | 3326 | `			nBase -= nSuffix;` |
|      2 | 3327 | `		}` |
|      2 | 3328 | `	}` |
|      - | 3329 | `	/* Store the basename */` |
|    237 | 3330 | `	ph7_result_string(pCtx,zBase,nBase);` |
|    237 | 3331 | `	return PH7_OK;` |
|    121 | 3332 | `}` |
|      - | 3333 | `/*` |
|      - | 3334 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 3335 | ` *  Returns information about a file path.` |
|      - | 3336 | ` * Parameter` |
|      - | 3337 | ` *  $path` |
|      - | 3338 | ` *   The path to be parsed.` |
|      - | 3339 | ` *  $options` |
|      - | 3340 | ` *    If present, specifies a specific element to be returned; one of` |
|      - | 3341 | ` *      PATHINFO_DIRNAME, PATHINFO_BASENAME, PATHINFO_EXTENSION or PATHINFO_FILENAME.` |
|      - | 3342 | ` * Return` |
|      - | 3343 | ` *  If the options parameter is not passed, an associative array containing the following` |
|      - | 3344 | ` *  elements is returned: dirname, basename, extension (if any), and filename.` |
|      - | 3345 | ` *  If options is present, returns a string containing the requested element.` |
|      - | 3346 | ` */` |
|      - | 3347 | `typedef struct path_info path_info;` |
|      - | 3348 | `struct path_info` |
|      - | 3349 | `{` |
|      - | 3350 | `	SyString sDir; /* Directory [i.e: /var/www] */` |
|      - | 3351 | `	SyString sBasename; /* Basename [i.e httpd.conf] */` |
|      - | 3352 | `	SyString sExtension; /* File extension [i.e xml,pdf..] */` |
|      - | 3353 | `	SyString sFilename;  /* Filename */` |
|      - | 3354 | `	int iPresent;        /* Which components php would EMIT (PH7_PATHINFO_* bits) */` |
|      - | 3355 | `};` |
|      - | 3356 | `/*` |
|      - | 3357 | ` * Extract path fields exactly as php's pathinfo() assembles them.` |
|      - | 3358 | ` *` |
|      - | 3359 | ` * Two things this has to get right beyond the values themselves:` |
|      - | 3360 | ` *` |
|      - | 3361 | ` *  - php looks for the LAST dot ANYWHERE in the basename, a leading one included,` |
|      - | 3362 | `` *    so `.bashrc` has extension "bashrc" and filename "" (PH7 stopped the scan`` |
|      - | 3363 | ` *    before the first byte, so it reported no extension and filename ".bashrc").` |
|      - | 3364 | ` *  - EMPTY is not the same as ABSENT. php always emits basename and filename when` |
|      - | 3365 | `` *    they are asked for, emits extension whenever a dot exists (even for `x.`,`` |
|      - | 3366 | ` *    whose extension is ""), and emits dirname only when it is non-empty. The` |
|      - | 3367 | ` *    scalar form answers with the first EMITTED component, so conflating the two` |
|      - | 3368 | `` *    makes `pathinfo("x.", PATHINFO_EXTENSION\|PATHINFO_FILENAME)` fall through to`` |
|      - | 3369 | ` *    the filename ("x") where php answers "" — iPresent keeps them apart.` |
|      - | 3370 | ` *` |
|      - | 3371 | ` * dirname and basename come from the shared php_dirname/php_basename helpers` |
|      - | 3372 | ` * rather than a third hand-rolled walk, so the trailing-separator and` |
|      - | 3373 | ` * relative-path rules ("file.txt" -> ".", "/var/www/" -> "/var" + "www") cannot` |
|      - | 3374 | ` * drift between the two builtins and this one.` |
|      - | 3375 | ` */` |
|  19670 | 3376 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 3377 | `{` |
|      - | 3378 | `	const char *zBase,*zDir,*zDot;` |
|      - | 3379 | `	int nBase,nDir,i;` |
|      - | 3380 | `	/* Zero the structure */` |
|  19675 | 3381 | `	SyZero(pOut,sizeof(path_info));` |
|  19675 | 3382 | `	zDir = PH7_ExtractDirName(zPath,nByte,&nDir);` |
|  19675 | 3383 | `	if( nDir > 0 ){` |
|  19671 | 3384 | `		SyStringInitFromBuf(&pOut->sDir,zDir,nDir);` |
|  19671 | 3385 | `		pOut->iPresent \|= PH7_PATHINFO_DIRNAME;` |
|   9833 | 3386 | `	}` |
|  19675 | 3387 | `	zBase = PH7_ExtractBaseName(zPath,nByte,&nBase);` |
|  19675 | 3388 | `	SyStringInitFromBuf(&pOut->sBasename,zBase,nBase);` |
|  19675 | 3389 | `	pOut->iPresent \|= PH7_PATHINFO_BASENAME\|PH7_PATHINFO_FILENAME;` |
|      - | 3390 | `	/* Last dot anywhere in the basename splits filename from extension */` |
|  19675 | 3391 | `	zDot = 0;` |
|  98271 | 3392 | `	for( i = nBase ; i > 0 ; --i ){` |
|  98249 | 3393 | `		if( zBase[i - 1] == '.' ){` |
|  19653 | 3394 | `			zDot = &zBase[i - 1];` |
|  19653 | 3395 | `			break;` |
|      - | 3396 | `		}` |
|  39303 | 3397 | `	}` |
|  19675 | 3398 | `	if( zDot ){` |
|  19653 | 3399 | `		SyStringInitFromBuf(&pOut->sExtension,zDot + 1,(int)(&zBase[nBase] - (zDot + 1)));` |
|  19653 | 3400 | `		pOut->iPresent \|= PH7_PATHINFO_EXTENSION;` |
|  19653 | 3401 | `		SyStringInitFromBuf(&pOut->sFilename,zBase,(int)(zDot - zBase));` |
|   9829 | 3402 | `	}else{` |
|     23 | 3403 | `		SyStringInitFromBuf(&pOut->sFilename,zBase,nBase);` |
|      - | 3404 | `	}` |
|  19675 | 3405 | `	return SXRET_OK;` |
|      5 | 3406 | `}` |
|      - | 3407 | `/*` |
|      - | 3408 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 3409 | ` *  See block comment above.` |
|      - | 3410 | ` */` |
|  19670 | 3411 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3412 | `{` |
|      - | 3413 | `	const char *zPath;` |
|      - | 3414 | `	path_info sInfo;` |
|      - | 3415 | `	int iLen;` |
|  19675 | 3416 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 3417 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 3418 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3419 | `		return PH7_OK;` |
|      - | 3420 | `	}` |
|      - | 3421 | `	/* Point to the target path. The EMPTY path is not a special case: php still` |
|      - | 3422 | ``	 * answers with the array `["basename" => "", "filename" => ""]` (and "" for a`` |
|      - | 3423 | `	 * scalar request), where PH7 short-circuited to "" and returned the wrong TYPE. */` |
|  19675 | 3424 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 3425 | `	/* Extract path info */` |
|  19675 | 3426 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|      - | 3427 | ``	/* Read the mask at 64-bit width: ph7_value_to_int() truncates to `int`, so a`` |
|      - | 3428 | `	 * flags value congruent to PATHINFO_ALL mod 2^32 (4294967311, -4294967281 …)` |
|      - | 3429 | `	 * would take the ARRAY branch and answer with the wrong TYPE. */` |
|  19670 | 3430 | `	if( nArg > 1 && ph7_value_is_int(apArg[1])` |
|  29487 | 3431 | `	 && ph7_value_to_int64(apArg[1]) != (ph7_int64)PH7_PATHINFO_ALL ){` |
|      - | 3432 | `		/* $flags is a BITMASK, not an enum: php assembles the requested components in` |
|      - | 3433 | `		 * the fixed order below and, for anything other than PATHINFO_ALL, hands back` |
|      - | 3434 | `		 * the FIRST one it EMITTED (zend_hash_get_current_data on the fresh array).` |
|      - | 3435 | ``		 * So `PATHINFO_DIRNAME\|PATHINFO_BASENAME` answers the dirname, and an unknown`` |
|      - | 3436 | `		 * bit that happens to carry a known one along (99 = 1\|2\|32\|64) answers as if` |
|      - | 3437 | `		 * only the known ones were passed. PH7 numbered the components 1/2/3/4 and` |
|      - | 3438 | `		 * switched on the whole value, so it read a two-flag mask as a different single` |
|      - | 3439 | `		 * component and answered "" for everything else. Emission is iPresent, NOT` |
|      - | 3440 | `		 * "non-empty": an emitted-but-empty component ends the search with "". */` |
|  19657 | 3441 | `		ph7_int64 nComp = ph7_value_to_int64(apArg[1]);` |
|      - | 3442 | `		static const int aBit[4] = {` |
|      - | 3443 | `			PH7_PATHINFO_DIRNAME,PH7_PATHINFO_BASENAME,` |
|      - | 3444 | `			PH7_PATHINFO_EXTENSION,PH7_PATHINFO_FILENAME` |
|      - | 3445 | `		};` |
|      - | 3446 | `		SyString *apComp[4];` |
|      - | 3447 | `		int i;` |
|  19657 | 3448 | `		apComp[0] = &sInfo.sDir;` |
|  19657 | 3449 | `		apComp[1] = &sInfo.sBasename;` |
|  19657 | 3450 | `		apComp[2] = &sInfo.sExtension;` |
|  19657 | 3451 | `		apComp[3] = &sInfo.sFilename;` |
|      - | 3452 | `		/* Expand the empty string unless a requested component is emitted */` |
|  19657 | 3453 | `		ph7_result_string(pCtx,"",0);` |
|  68721 | 3454 | `		for( i = 0 ; i < 4 ; ++i ){` |
|  68713 | 3455 | `			if( (nComp & aBit[i]) == aBit[i] && (sInfo.iPresent & aBit[i]) ){` |
|  19649 | 3456 | `				ph7_result_string(pCtx,apComp[i]->zString,(int)apComp[i]->nByte);` |
|  19649 | 3457 | `				break;` |
|      - | 3458 | `			}` |
|  24537 | 3459 | `		}` |
|   9831 | 3460 | `	}else{` |
|      - | 3461 | `		/* Return an associative array */` |
|      - | 3462 | `		ph7_value *pArray,*pValue;` |
|     19 | 3463 | `		pArray = ph7_context_new_array(pCtx);` |
|     19 | 3464 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     19 | 3465 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 3466 | `			/* Out of mem,return NULL */` |
|    ! 0 | 3467 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3468 | `			return PH7_OK;` |
|      - | 3469 | `		}` |
|      - | 3470 | ``		/* Emitted-but-EMPTY components are in the array too (php keys `basename` and`` |
|      - | 3471 | ``		 * `filename` for "/" with ""), so this walks iPresent, not the lengths. */`` |
|      - | 3472 | `		{` |
|      - | 3473 | `		static const char *azKey[4] = {"dirname","basename","extension","filename"};` |
|      - | 3474 | `		static const int aBit[4] = {` |
|      - | 3475 | `			PH7_PATHINFO_DIRNAME,PH7_PATHINFO_BASENAME,` |
|      - | 3476 | `			PH7_PATHINFO_EXTENSION,PH7_PATHINFO_FILENAME` |
|      - | 3477 | `		};` |
|      - | 3478 | `		SyString *apComp[4];` |
|      - | 3479 | `		int i;` |
|     19 | 3480 | `		apComp[0] = &sInfo.sDir;` |
|     19 | 3481 | `		apComp[1] = &sInfo.sBasename;` |
|     19 | 3482 | `		apComp[2] = &sInfo.sExtension;` |
|     19 | 3483 | `		apComp[3] = &sInfo.sFilename;` |
|     91 | 3484 | `		for( i = 0 ; i < 4 ; ++i ){` |
|     73 | 3485 | `			if( (sInfo.iPresent & aBit[i]) == 0 ){` |
|     11 | 3486 | `				continue;` |
|      - | 3487 | `			}` |
|     63 | 3488 | `			ph7_value_reset_string_cursor(pValue);` |
|     63 | 3489 | `			ph7_value_string(pValue,apComp[i]->zString,(int)apComp[i]->nByte);` |
|     63 | 3490 | `			ph7_array_add_strkey_elem(pArray,azKey[i],pValue); /* Will make it's own copy */` |
|     32 | 3491 | `		}` |
|      - | 3492 | `		}` |
|      - | 3493 | `		/* Return the created array */` |
|     19 | 3494 | `		ph7_result_value(pCtx,pArray);` |
|      - | 3495 | `		/* Don't worry about freeing memory, everything will be released` |
|      - | 3496 | `		 * automatically as soon we return from this foreign function.` |
|      - | 3497 | `		 */` |
|      - | 3498 | `	}` |
|  19675 | 3499 | `	return PH7_OK;` |
|   9840 | 3500 | `}` |
|      - | 3501 | `/* SPDX-SnippetBegin */` |
|      - | 3502 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 3503 | `/* SPDX-License-Identifier: blessing */` |
|      - | 3504 | `/*` |
|      - | 3505 | ` * Globbing implementation extracted from the sqlite3 source tree.` |
|      - | 3506 |  |
|      - | 3507 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|      - | 3508 | ` * Status: Public Domain` |
|      - | 3509 | ` */` |
|      - | 3510 | `typedef unsigned char u8;` |
|      - | 3511 | `/* An array to map all upper-case characters into their corresponding` |
|      - | 3512 | `** lower-case character.` |
|      - | 3513 | `**` |
|      - | 3514 | `** SQLite only considers US-ASCII (or EBCDIC) characters.  We do not` |
|      - | 3515 | `** handle case conversions for the UTF character set since the tables` |
|      - | 3516 | `** involved are nearly as big or bigger than SQLite itself.` |
|      - | 3517 | `*/` |
|      - | 3518 | `static const unsigned char sqlite3UpperToLower[] = {` |
|      - | 3519 | `      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,` |
|      - | 3520 | `     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,` |
|      - | 3521 | `     36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,` |
|      - | 3522 | `     54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,` |
|      - | 3523 | `    104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,` |
|      - | 3524 | `    122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,` |
|      - | 3525 | `    108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,` |
|      - | 3526 | `    126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,` |
|      - | 3527 | `    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,` |
|      - | 3528 | `    162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,` |
|      - | 3529 | `    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,` |
|      - | 3530 | `    198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,` |
|      - | 3531 | `    216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,` |
|      - | 3532 | `    234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,` |
|      - | 3533 | `    252,253,254,255` |
|      - | 3534 | `};` |
|      - | 3535 | `#define GlogUpperToLower(A)     if( A<0x80 ){ A = sqlite3UpperToLower[A]; }` |
|      - | 3536 | `/*` |
|      - | 3537 | `** Assuming zIn points to the first byte of a UTF-8 character,` |
|      - | 3538 | `** advance zIn to point to the first byte of the next UTF-8 character.` |
|      - | 3539 | `*/` |
|      - | 3540 | `#define SQLITE_SKIP_UTF8(zIn) {                        \` |
|      - | 3541 | `  if( (*(zIn++))>=0xc0 ){                              \` |
|      - | 3542 | `    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \` |
|      - | 3543 | `  }                                                    \` |
|      - | 3544 | `}` |
|      - | 3545 | `/*` |
|      - | 3546 | `** Compare two UTF-8 strings for equality where the first string can` |
|      - | 3547 | `** potentially be a "glob" expression.  Return true (1) if they` |
|      - | 3548 | `** are the same and false (0) if they are different.` |
|      - | 3549 | `**` |
|      - | 3550 | `** Globbing rules:` |
|      - | 3551 | `**` |
|      - | 3552 | `**      '*'       Matches any sequence of zero or more characters.` |
|      - | 3553 | `**` |
|      - | 3554 | `**      '?'       Matches exactly one character.` |
|      - | 3555 | `**` |
|      - | 3556 | `**     [...]      Matches one character from the enclosed list of` |
|      - | 3557 | `**                characters.` |
|      - | 3558 | `**` |
|      - | 3559 | `**     [^...]     Matches one character not in the enclosed list.` |
|      - | 3560 | `**` |
|      - | 3561 | `** With the [...] and [^...] matching, a ']' character can be included` |
|      - | 3562 | `** in the list by making it the first character after '[' or '^'.  A` |
|      - | 3563 | `** range of characters can be specified using '-'.  Example:` |
|      - | 3564 | `** "[a-z]" matches any single lower-case letter.  To match a '-', make` |
|      - | 3565 | `** it the last character in the list.` |
|      - | 3566 | `**` |
|      - | 3567 | `** This routine is usually quick, but can be N**2 in the worst case.` |
|      - | 3568 | `**` |
|      - | 3569 | `** Hints: to match '*' or '?', put them in "[]".  Like this:` |
|      - | 3570 | `**` |
|      - | 3571 | `**         abc[*]xyz        Matches "abc*xyz" only` |
|      - | 3572 | `*/` |
|      - | 3573 | `/*` |
|      - | 3574 | `` * One POSIX character class of a `[...]` set, as glibc's matcher answers it.`` |
|      - | 3575 | ` * The classes are ASCII-only in the C locale php runs its fnmatch()/glob() in,` |
|      - | 3576 | ` * so a code point past 127 belongs to none of them.` |
|      - | 3577 | ` */` |
|     92 | 3578 | `static int PatternPosixClass(const unsigned char *zName,int nName,int c)` |
|      1 | 3579 | `{` |
|      - | 3580 | `	static const struct { const char *zName; int nName; } aClass[] = {` |
|      - | 3581 | `		{ "alnum", 5 }, { "alpha", 5 }, { "blank", 5 }, { "cntrl", 5 },` |
|      - | 3582 | `		{ "digit", 5 }, { "graph", 5 }, { "lower", 5 }, { "print", 5 },` |
|      - | 3583 | `		{ "punct", 5 }, { "space", 5 }, { "upper", 5 }, { "xdigit", 6 },` |
|      - | 3584 | `	};` |
|     93 | 3585 | `	int i,iWhich = -1;` |
|    545 | 3586 | `	for( i = 0 ; i < (int)(sizeof(aClass)/sizeof(aClass[0])) ; ++i ){` |
|    540 | 3587 | `		if( aClass[i].nName == nName` |
|    495 | 3588 | `		 && SyMemcmp(aClass[i].zName,(const char *)zName,(sxu32)nName) == 0 ){` |
|     89 | 3589 | `			iWhich = i;` |
|     89 | 3590 | `			break;` |
|      - | 3591 | `		}` |
|    227 | 3592 | `	}` |
|     93 | 3593 | `	if( iWhich < 0 \|\| c < 0 \|\| c > 127 ){` |
|      - | 3594 | `		/* An unknown class name matches nothing, which is what a matcher that` |
|      - | 3595 | `		 * cannot name the set can honestly say. */` |
|      5 | 3596 | `		return 0;` |
|      - | 3597 | `	}` |
|     89 | 3598 | `	switch( iWhich ){` |
|    ! 0 | 3599 | `		case 0: return SyisAlphaNum(c);` |
|     21 | 3600 | `		case 1: return SyisAlpha(c);` |
|    ! 0 | 3601 | `		case 2: return c == ' ' \|\| c == '\t';` |
|    ! 0 | 3602 | `		case 3: return c < 0x20 \|\| c == 0x7F;` |
|     49 | 3603 | `		case 4: return SyisDigit(c);` |
|    ! 0 | 3604 | `		case 5: return c > 0x20 && c < 0x7F;` |
|    ! 0 | 3605 | `		case 6: return SyisLower(c);` |
|    ! 0 | 3606 | `		case 7: return c >= 0x20 && c < 0x7F;` |
|      5 | 3607 | `		case 8: return c > 0x20 && c < 0x7F && !SyisAlphaNum(c);` |
|      5 | 3608 | `		case 9: return SyisSpace(c);` |
|      9 | 3609 | `		case 10: return SyisUpper(c);` |
|      5 | 3610 | `		default: return SyisHex(c);` |
|      - | 3611 | `	}` |
|     47 | 3612 | `}` |
|   2760 | 3613 | `static int patternCompare(` |
|      - | 3614 | `  const u8 *zPattern,              /* The glob pattern */` |
|      - | 3615 | `  const u8 *zString,               /* The string to compare against the glob */` |
|      - | 3616 | `  const int esc,                    /* The escape character */` |
|      - | 3617 | `  int noCase,` |
|      - | 3618 | ``  int bCaret                        /* `[^...]` inverts (fnmatch) or is a literal `^` (glob) */`` |
|      4 | 3619 | `){` |
|      - | 3620 | `  int c, c2, cLow;` |
|      - | 3621 | `  int invert;` |
|      - | 3622 | `  int seen;` |
|   2764 | 3623 | `  u8 matchOne = '?';` |
|   2764 | 3624 | `  u8 matchAll = '*';` |
|   2764 | 3625 | `  u8 matchSet = '[';` |
|   2764 | 3626 | `  int prevEscape = 0;     /* True if the previous character was 'escape' */` |
|      - | 3627 |  |
|   2764 | 3628 | `  if( !zPattern \|\| !zString ) return 0;` |
|   3867 | 3629 | `  while( (c = PH7_Utf8Read(zPattern,0,&zPattern))!=0 ){` |
|   3475 | 3630 | `    if( !prevEscape && c==matchAll ){` |
|   2936 | 3631 | `      while( (c=PH7_Utf8Read(zPattern,0,&zPattern)) == matchAll` |
|   1472 | 3632 | `               \|\| c == matchOne ){` |
|    ! 0 | 3633 | `        if( c==matchOne && PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 3634 | `          return 0;` |
|      - | 3635 | `        }` |
|    ! 0 | 3636 | `      }` |
|   1472 | 3637 | `      if( c==0 ){` |
|    932 | 3638 | `        return 1;` |
|    542 | 3639 | `      }else if( c==esc ){` |
|    ! 0 | 3640 | `        c = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 3641 | `        if( c==0 ){` |
|    ! 0 | 3642 | `          return 0;` |
|    ! 0 | 3643 | `        }` |
|    542 | 3644 | `      }else if( c==matchSet ){` |
|      - | 3645 | ``        /* A `[...]` set right after a `*`: try it at every remaining position.`` |
|      - | 3646 | `         * The two asserts SQLite has here became guards, and one of them --` |
|      - | 3647 | `         * "'[' is a single-byte character" -- is ALWAYS true, so this branch` |
|      - | 3648 | ``         * returned 0 for every pattern of the shape `*[...]`. `*[ab]`,`` |
|      - | 3649 | ``         * `a*[0-9]` and `*[[:digit:]]` matched NOTHING, in fnmatch(), in`` |
|      - | 3650 | `         * glob() and in strglob() alike. */` |
|    270 | 3651 | `        while( *zString && patternCompare(&zPattern[-1],zString,esc,noCase,bCaret)==0 ){` |
|    147 | 3652 | `          SQLITE_SKIP_UTF8(zString);` |
|      1 | 3653 | `        }` |
|     83 | 3654 | `        return *zString!=0;` |
|      - | 3655 | `      }` |
|    548 | 3656 | `      while( (c2 = PH7_Utf8Read(zString,0,&zString))!=0 ){` |
|    548 | 3657 | `        if( noCase ){` |
|      3 | 3658 | `          GlogUpperToLower(c2);` |
|      3 | 3659 | `          GlogUpperToLower(c);` |
|     11 | 3660 | `          while( c2 != 0 && c2 != c ){` |
|      9 | 3661 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      9 | 3662 | `            GlogUpperToLower(c2);` |
|      1 | 3663 | `          }` |
|      2 | 3664 | `        }else{` |
|   1564 | 3665 | `          while( c2 != 0 && c2 != c ){` |
|   1020 | 3666 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      2 | 3667 | `          }` |
|      - | 3668 | `        }` |
|    548 | 3669 | `        if( c2==0 ) return 0;` |
|    270 | 3670 | `		if( patternCompare(zPattern,zString,esc,noCase,bCaret) ) return 1;` |
|      2 | 3671 | `      }` |
|    ! 0 | 3672 | `      return 0;` |
|   2006 | 3673 | `    }else if( !prevEscape && c==matchOne ){` |
|     25 | 3674 | `      if( PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 3675 | `        return 0;` |
|      1 | 3676 | `      }` |
|   1994 | 3677 | `    }else if( c==matchSet ){` |
|    459 | 3678 | `      int prior_c = 0;` |
|      - | 3679 | `      /* SQLite asserts here that its GLOB has no escape character; the guard` |
|      - | 3680 | `       * that replaced the assert reads the condition BACKWARDS, so a set` |
|      - | 3681 | ``       * matched nothing whenever escaping was turned off -- every `[...]` in`` |
|      - | 3682 | ``       * an `fnmatch($p,$s,FNM_NOESCAPE)` call answered false. */`` |
|    459 | 3683 | `      seen = 0;` |
|    459 | 3684 | `      invert = 0;` |
|    459 | 3685 | `      c = PH7_Utf8Read(zString, 0, &zString);` |
|    459 | 3686 | `      if( c==0 ) return 0;` |
|      - | 3687 | `      /* A case-INSENSITIVE match folds inside the set too: this branch ignored` |
|      - | 3688 | ``       * noCase entirely, so `fnmatch('[a-c]','B',FNM_CASEFOLD)` was false and`` |
|      - | 3689 | ``       * its negation `[!a-c]` was true -- both the opposite of php's. The`` |
|      - | 3690 | `       * folded subject is what MEMBERS and RANGES are compared against; a` |
|      - | 3691 | ``        * character CLASS is not folded at all (glibc tests `[[:upper:]]` `` |
|      - | 3692 | `       * against the character as written, FNM_CASEFOLD or not). */` |
|    459 | 3693 | `      cLow = c;` |
|    459 | 3694 | `      if( noCase ){` |
|     49 | 3695 | `        GlogUpperToLower(cLow);` |
|     24 | 3696 | `      }` |
|    459 | 3697 | `      c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|      - | 3698 | ``      /* POSIX spells the negation `!` and glibc accepts `^` as well; php's`` |
|      - | 3699 | ``       * fnmatch()/glob() are glibc's, so BOTH invert. Only `^` did here, which`` |
|      - | 3700 | ``       * made `[!a]` a set holding `!` and `a` -- the exact INVERSE answer for`` |
|      - | 3701 | `       * the spelling a shell uses. */` |
|    459 | 3702 | `      if( c2=='!' \|\| (bCaret && c2=='^') ){` |
|    117 | 3703 | `        invert = 1;` |
|    117 | 3704 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|     58 | 3705 | `      }` |
|    459 | 3706 | `      if( c2==']' ){` |
|     19 | 3707 | `        if( c==']' ) seen = 1;` |
|     19 | 3708 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|      9 | 3709 | `      }` |
|   1171 | 3710 | `      while( c2 && c2!=']' ){` |
|    713 | 3711 | `        int cFold = c2;` |
|    713 | 3712 | `        if( noCase ){` |
|     71 | 3713 | `          GlogUpperToLower(cFold);` |
|     35 | 3714 | `        }` |
|    713 | 3715 | `        if( c2=='[' && zPattern[0]==':' ){` |
|      - | 3716 | ``          /* A POSIX character CLASS, `[:alpha:]`, which glibc's matcher knows`` |
|      - | 3717 | ``           * and this one did not -- the whole `[[:digit:]]` bracket read as the`` |
|      - | 3718 | ``           * literal set `[:digt` and matched the wrong characters in silence. */`` |
|     93 | 3719 | `          const unsigned char *zName = &zPattern[1];` |
|     93 | 3720 | `          const unsigned char *zEnd = zName;` |
|    553 | 3721 | `          while( zEnd[0] != 0 && !(zEnd[0]==':' && zEnd[1]==']') ){` |
|    461 | 3722 | `            zEnd++;` |
|      1 | 3723 | `          }` |
|     93 | 3724 | `          if( zEnd[0] != 0 ){` |
|     93 | 3725 | `            if( PatternPosixClass(zName,(int)(zEnd - zName),c) ){` |
|     47 | 3726 | `              seen = 1;` |
|     23 | 3727 | `            }` |
|     93 | 3728 | `            zPattern = zEnd + 2;` |
|     93 | 3729 | `            prior_c = 0;` |
|     93 | 3730 | `            c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|     93 | 3731 | `            continue;` |
|      - | 3732 | `          }` |
|    ! 0 | 3733 | `        }` |
|    621 | 3734 | `        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){` |
|    187 | 3735 | `          c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    187 | 3736 | `          cFold = c2;` |
|    187 | 3737 | `          if( noCase ){` |
|     15 | 3738 | `            GlogUpperToLower(cFold);` |
|      7 | 3739 | `          }` |
|    187 | 3740 | `          if( cLow>=prior_c && cLow<=cFold ) seen = 1;` |
|    187 | 3741 | `          prior_c = 0;` |
|     94 | 3742 | `        }else{` |
|    435 | 3743 | `          if( cLow==cFold ){` |
|     61 | 3744 | `            seen = 1;` |
|     30 | 3745 | `          }` |
|    435 | 3746 | `          prior_c = cFold;` |
|      - | 3747 | `        }` |
|    621 | 3748 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|      1 | 3749 | `      }` |
|    459 | 3750 | `      if( c2==0 \|\| (seen ^ invert)==0 ){` |
|    203 | 3751 | `        return 0;` |
|      1 | 3752 | `      }` |
|   1652 | 3753 | `    }else if( esc==c && !prevEscape ){` |
|      9 | 3754 | `      prevEscape = 1;` |
|      5 | 3755 | `    }else{` |
|   1516 | 3756 | `      c2 = PH7_Utf8Read(zString, 0, &zString);` |
|   1516 | 3757 | `      if( noCase ){` |
|     17 | 3758 | `        GlogUpperToLower(c);` |
|     17 | 3759 | `        GlogUpperToLower(c2);` |
|      8 | 3760 | `      }` |
|   1516 | 3761 | `      if( c!=c2 ){` |
|    700 | 3762 | `        return 0;` |
|      - | 3763 | `      }` |
|    818 | 3764 | `      prevEscape = 0;` |
|      - | 3765 | `    }` |
|      3 | 3766 | `  }` |
|    394 | 3767 | `  return *zString==0;` |
|   1437 | 3768 | `}` |
|      - | 3769 | `/* SPDX-SnippetEnd */` |
|      - | 3770 | `/*` |
|      - | 3771 | ` * Wrapper around patternCompare() defined above.` |
|      - | 3772 | ` * See block comment above for more information.` |
|      - | 3773 | ` */` |
|   2304 | 3774 | `static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,` |
|      - | 3775 | `	int CaseCompare,int bCaret)` |
|      4 | 3776 | `{` |
|      - | 3777 | `	int rc;` |
|   2308 | 3778 | `	if( iEsc < 0 ){` |
|    ! 0 | 3779 | `		iEsc = '\\';` |
|    ! 0 | 3780 | `	}` |
|   2308 | 3781 | `	rc = patternCompare(zPattern,zString,iEsc,CaseCompare,bCaret);` |
|   2308 | 3782 | `	return rc;` |
|      4 | 3783 | `}` |
|      - | 3784 | `/*` |
|      - | 3785 | ` * bool fnmatch(string $pattern,string $string[,int $flags = 0 ])` |
|      - | 3786 | ` *  Match filename against a pattern.` |
|      - | 3787 | ` * Parameters` |
|      - | 3788 | ` *  $pattern` |
|      - | 3789 | ` *   The shell wildcard pattern.` |
|      - | 3790 | ` * $string` |
|      - | 3791 | ` *  The tested string.` |
|      - | 3792 | ` * $flags` |
|      - | 3793 | ` *   A list of possible flags:` |
|      - | 3794 | ` *    FNM_NOESCAPE 	Disable backslash escaping.` |
|      - | 3795 | ` *    FNM_PATHNAME 	Slash in string only matches slash in the given pattern.` |
|      - | 3796 | ` *    FNM_PERIOD 	Leading period in string must be exactly matched by period in the given pattern.` |
|      - | 3797 | ` *    FNM_CASEFOLD 	Caseless match.` |
|      - | 3798 | ` * Return` |
|      - | 3799 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 3800 | ` */` |
|    158 | 3801 | `static int PH7_builtin_fnmatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3802 | `{` |
|      - | 3803 | `	const char *zString,*zPattern;` |
|    159 | 3804 | `	int iEsc = '\\';` |
|    159 | 3805 | `	int noCase = 0;` |
|      - | 3806 | `	int rc;` |
|    159 | 3807 | `	if( nArg < 2 \|\| !VfsPathArgUsable(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 3808 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3809 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3810 | `		return PH7_OK;` |
|      - | 3811 | `	}` |
|      - | 3812 | `	/* Extract the pattern and the string */` |
|    159 | 3813 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|    159 | 3814 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 3815 | `	/* Extract the flags if avaialble */` |
|    159 | 3816 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|     99 | 3817 | `		rc = ph7_value_to_int(apArg[2]);` |
|     99 | 3818 | `		if( rc & 2 /*FNM_NOESCAPE (php value)*/){` |
|     51 | 3819 | `			iEsc = 0;` |
|     25 | 3820 | `		}` |
|     99 | 3821 | `		if( rc & 16 /*FNM_CASEFOLD (php value)*/){` |
|     45 | 3822 | `			noCase = 1;` |
|     22 | 3823 | `		}` |
|     49 | 3824 | `	}` |
|      - | 3825 | ``	/* Go globbing. fnmatch() is glibc's, whose matcher takes `^` as a second`` |
|      - | 3826 | `	 * spelling of the negation -- glob(3)'s does NOT, and strglob() below` |
|      - | 3827 | `	 * carries glob()'s rule because that is what the prelude glob() drives. */` |
|    159 | 3828 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,noCase,TRUE);` |
|      - | 3829 | `	/* Globbing result */` |
|    159 | 3830 | `	ph7_result_bool(pCtx,rc);` |
|    159 | 3831 | `	return PH7_OK;` |
|     80 | 3832 | `}` |
|      - | 3833 | `/*` |
|      - | 3834 | ` * bool strglob(string $pattern,string $string)` |
|      - | 3835 | ` *  Match string against a pattern.` |
|      - | 3836 | ` * Parameters` |
|      - | 3837 | ` *  $pattern` |
|      - | 3838 | ` *   The shell wildcard pattern.` |
|      - | 3839 | ` * $string` |
|      - | 3840 | ` *  The tested string.` |
|      - | 3841 | ` * Return` |
|      - | 3842 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 3843 | ` * Note that this a symisc eXtension.` |
|      - | 3844 | ` */` |
|   1554 | 3845 | `static int PH7_builtin_strglob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3846 | `{` |
|      - | 3847 | `	const char *zString,*zPattern;` |
|   1558 | 3848 | `	int iEsc = '\\';` |
|      - | 3849 | `	int rc;` |
|   1558 | 3850 | `	if( nArg < 2 \|\| !VfsPathArgUsable(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 3851 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3852 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3853 | `		return PH7_OK;` |
|      - | 3854 | `	}` |
|      - | 3855 | `	/* Extract the pattern and the string */` |
|   1558 | 3856 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|   1558 | 3857 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 3858 | ``	/* Go globbing, with glob(3)'s set rules: only `!` inverts, and a `^` right`` |
|      - | 3859 | ``	 * after the `[` is an ordinary member of the set. */`` |
|   1558 | 3860 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0,FALSE);` |
|      - | 3861 | `	/* Globbing result */` |
|   1558 | 3862 | `	ph7_result_bool(pCtx,rc);` |
|   1558 | 3863 | `	return PH7_OK;` |
|    779 | 3864 | `}` |
|      - | 3865 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 3866 | `/* Every buffer below is one path, and php's own limit for one is PATH_MAX; the` |
|      - | 3867 | ` * SPL directory opener already refuses a longer one, and a pattern past it can` |
|      - | 3868 | ` * name nothing that exists. It also bounds the recursion: each level of the` |
|      - | 3869 | ` * walk consumes at least one slash of the pattern. */` |
|      - | 3870 | `#define PH7_GLOB_PATH_MAX 4096` |
|      - | 3871 | `/*` |
|      - | 3872 | ` * ---------------------------------------------------------------------------` |
|      - | 3873 | ` * The glob:// stream device.` |
|      - | 3874 | ` *` |
|      - | 3875 | ` * php's glob wrapper is a DIRECTORY whose entries are a pattern's matches:` |
|      - | 3876 | `` * `opendir('glob://src/' . '*.php')` hands out one BASENAME per match, and`` |
|      - | 3877 | ` * GlobIterator is that stream behind the whole DirectoryIterator machinery. It` |
|      - | 3878 | ` * is a dir_opener and NOTHING else -- php gives it no stream opener (so` |
|      - | 3879 | `` * `fopen('glob://…')` is "wrapper does not support stream open") and no`` |
|      - | 3880 | `` * url_stat (so `file_exists()` and `is_dir()` answer false for one).`` |
|      - | 3881 | ` *` |
|      - | 3882 | ` * The expansion is glob(3) with NO flags, which is what php's opener asks for,` |
|      - | 3883 | ` * so it has to agree name for name AND order for order with the prelude` |
|      - | 3884 | ` * glob(). Two rules are the whole of it -- a pattern is matched one SEGMENT at` |
|      - | 3885 | ` * a time, and the answer is sorted by BYTES rather than by value -- and both` |
|      - | 3886 | ` * are spelled here the way that function spells them, because the two` |
|      - | 3887 | ` * implementations must not drift: 001-smoke/glob_stream_device.phpt walks a` |
|      - | 3888 | ` * table of patterns through both and compares, which is what pins them.` |
|      - | 3889 | ` *` |
|      - | 3890 | `` * php's `pglob->path` is the directory of the match a read just handed OUT,`` |
|      - | 3891 | `` * never the pattern's: `glob://a/` + `*` + `/` + `*.txt` reports `a/sub1`, then`` |
|      - | 3892 | `` * `a/sub2`; a match with no slash in it reports the EMPTY string; and running`` |
|      - | 3893 | ` * out clears it, which is why GlobIterator::getPathname() answers "" past the` |
|      - | 3894 | ` * end.` |
|      - | 3895 | ` * ---------------------------------------------------------------------------` |
|      - | 3896 | ` */` |
|      - | 3897 | `/* One matched path. The blob it points into grows as the walk does, so an` |
|      - | 3898 | ` * OFFSET is what may be kept -- a pointer would not survive the next append. */` |
|      - | 3899 | `typedef PH7_GlobHit glob_hit;` |
|      - | 3900 | `typedef struct glob_stream glob_stream;` |
|      - | 3901 | `struct glob_stream` |
|      - | 3902 | `{` |
|      - | 3903 | `	ph7_vm *pVm;` |
|      - | 3904 | `	SyBlob sHit;   /* every matched path, back to back */` |
|      - | 3905 | `	SySet aHit;    /* one glob_hit per match, in php's order */` |
|      - | 3906 | `	sxu32 nCur;    /* php's pglob->index -- it counts PAST the end too */` |
|      - | 3907 | `	SyBlob sDir;   /* php's pglob->path: the directory of the CURRENT match */` |
|      - | 3908 | `};` |
|      - | 3909 | `/* php's glob_pattern_p: is there anything here for glob(3) to expand? */` |
|     92 | 3910 | `static int GlobHasMeta(const char *zPat,int nPat)` |
|      1 | 3911 | `{` |
|      - | 3912 | `	int i;` |
|   3727 | 3913 | `	for( i = 0 ; i < nPat ; ++i ){` |
|   3645 | 3914 | `		if( zPat[i] == '*' \|\| zPat[i] == '?' \|\| zPat[i] == '[' ){` |
|     11 | 3915 | `			return 1;` |
|      - | 3916 | `		}` |
|   2830 | 3917 | `	}` |
|     83 | 3918 | `	return 0;` |
|     47 | 3919 | `}` |
|      - | 3920 | `/* strcoll() in the C locale php runs in: unsigned bytes, then the shorter one` |
|      - | 3921 | ` * first. The same comparison the prelude glob() gets out of SORT_STRING. */` |
|    242 | 3922 | `static int GlobCmp(const char *zA,sxu32 nA,const char *zB,sxu32 nB)` |
|      1 | 3923 | `{` |
|    243 | 3924 | `	sxu32 nMin = nA < nB ? nA : nB;` |
|      - | 3925 | `	sxu32 i;` |
|   9889 | 3926 | `	for( i = 0 ; i < nMin ; ++i ){` |
|   9883 | 3927 | `		int ca = (unsigned char)zA[i];` |
|   9883 | 3928 | `		int cb = (unsigned char)zB[i];` |
|   9883 | 3929 | `		if( ca != cb ){` |
|    237 | 3930 | `			return ca < cb ? -1 : 1;` |
|      - | 3931 | `		}` |
|   7446 | 3932 | `	}` |
|      7 | 3933 | `	if( nA == nB ){` |
|    ! 0 | 3934 | `		return 0;` |
|      - | 3935 | `	}` |
|      7 | 3936 | `	return nA < nB ? -1 : 1;` |
|    121 | 3937 | `}` |
|      - | 3938 | `/*` |
|      - | 3939 | ` * Order one call's own answers. glob(3) sorts the whole list it is about to` |
|      - | 3940 | ` * return rather than each directory it walked, and so does the prelude glob(),` |
|      - | 3941 | ` * so every branch below sorts the range IT produced.` |
|      - | 3942 | ` *` |
|      - | 3943 | ` * A shell sort: filenames within one answer are unique, so nothing here needs` |
|      - | 3944 | ` * to be stable, and a directory of ten thousand entries must not cost the` |
|      - | 3945 | ` * hundred million comparisons an insertion sort would.` |
|      - | 3946 | ` */` |
|    104 | 3947 | `static void GlobSort(SyBlob *pHit,SySet *pSet,sxu32 nStart)` |
|      1 | 3948 | `{` |
|    105 | 3949 | `	glob_hit *aHit = (glob_hit *)SySetBasePtr(pSet);` |
|    105 | 3950 | `	const char *zBase = (const char *)SyBlobData(pHit);` |
|    105 | 3951 | `	sxu32 nEnd = SySetUsed(pSet);` |
|      - | 3952 | `	sxu32 nSpan,nGap;` |
|    105 | 3953 | `	if( nEnd - nStart < 2 ){` |
|     39 | 3954 | `		return;` |
|      - | 3955 | `	}` |
|     67 | 3956 | `	nSpan = nEnd - nStart;` |
|    153 | 3957 | `	for( nGap = nSpan / 2 ; nGap > 0 ; nGap /= 2 ){` |
|      - | 3958 | `		sxu32 i;` |
|    293 | 3959 | `		for( i = nStart + nGap ; i < nEnd ; ++i ){` |
|    207 | 3960 | `			glob_hit sTmp = aHit[i];` |
|    207 | 3961 | `			sxu32 j = i;` |
|    358 | 3962 | `			while( j >= nStart + nGap` |
|    416 | 3963 | `			 && GlobCmp(&zBase[aHit[j-nGap].nOfs],aHit[j-nGap].nLen,` |
|    362 | 3964 | `			            &zBase[sTmp.nOfs],sTmp.nLen) > 0 ){` |
|     87 | 3965 | `				aHit[j] = aHit[j-nGap];` |
|     87 | 3966 | `				j -= nGap;` |
|    ! 0 | 3967 | `			}` |
|    207 | 3968 | `			aHit[j] = sTmp;` |
|    104 | 3969 | `		}` |
|     44 | 3970 | `	}` |
|     53 | 3971 | `}` |
|      - | 3972 | `/* Record one match, spelled as a head and a tail so that the trailing-slash` |
|      - | 3973 | ` * branch can put its slash back without a second buffer. */` |
|    218 | 3974 | `static sxi32 GlobAdd(SyBlob *pHit,SySet *pSet,const char *zHead,sxu32 nHead,` |
|      - | 3975 | `	const char *zTail,sxu32 nTail)` |
|      1 | 3976 | `{` |
|      - | 3977 | `	glob_hit sHit;` |
|    219 | 3978 | `	sHit.nOfs = SyBlobLength(pHit);` |
|    219 | 3979 | `	sHit.nLen = nHead + nTail;` |
|    219 | 3980 | `	if( nHead > 0 && SyBlobAppend(pHit,zHead,nHead) != SXRET_OK ){` |
|    ! 0 | 3981 | `		return SXERR_MEM;` |
|      - | 3982 | `	}` |
|    219 | 3983 | `	if( nTail > 0 && SyBlobAppend(pHit,zTail,nTail) != SXRET_OK ){` |
|    ! 0 | 3984 | `		return SXERR_MEM;` |
|      - | 3985 | `	}` |
|    219 | 3986 | `	return SySetPut(pSet,(const void *)&sHit);` |
|    110 | 3987 | `}` |
|      - | 3988 | `/* Forward: the two halves of the walk call each other. */` |
|      - | 3989 | `static sxi32 GlobExpand(ph7_vm *pVm,const char *zPat,int nPat,int bOnlyDir,` |
|      - | 3990 | `	SyBlob *pHit,SySet *pSet);` |
|      - | 3991 | `/*` |
|      - | 3992 | ` * php's leaf: read the directory the pattern's last slash names and keep every` |
|      - | 3993 | ` * entry the segment after it matches, with that literal prefix back in front.` |
|      - | 3994 | ` * A directory that cannot be opened is zero matches in SILENCE, which is what` |
|      - | 3995 | ` * glob(3) answers for a path that is not there.` |
|      - | 3996 | ` */` |
|     82 | 3997 | `static sxi32 GlobLeaf(ph7_vm *pVm,const char *zPat,int nPat,int bOnlyDir,` |
|      - | 3998 | `	SyBlob *pHit,SySet *pSet)` |
|      1 | 3999 | `{` |
|     83 | 4000 | `	const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|      - | 4001 | `	const ph7_io_stream *pStream;` |
|      - | 4002 | `	const char *zDev,*zSeg;` |
|     83 | 4003 | `	void *pHandle = 0;` |
|      - | 4004 | `	ph7_context sCtx;` |
|      - | 4005 | `	ph7_value sEntry;` |
|      - | 4006 | `	char zDir[PH7_GLOB_PATH_MAX],zSegBuf[PH7_GLOB_PATH_MAX],zEnt[PH7_GLOB_PATH_MAX];` |
|     83 | 4007 | `	int nDir,nPrefix,nSeg,i,iSlash = -1;` |
|     83 | 4008 | `	sxi32 rc = SXRET_OK;` |
|    423 | 4009 | `	for( i = nPat - 1 ; i >= 0 ; --i ){` |
|    423 | 4010 | `		if( zPat[i] == '/' ){` |
|     83 | 4011 | `			iSlash = i;` |
|     83 | 4012 | `			break;` |
|      - | 4013 | `		}` |
|    171 | 4014 | `	}` |
|     83 | 4015 | `	if( iSlash < 0 ){` |
|      - | 4016 | ``		/* no directory part at all: php's own `.` */`` |
|    ! 0 | 4017 | `		zDir[0] = '.';` |
|    ! 0 | 4018 | `		nDir = 1;` |
|    ! 0 | 4019 | `		nPrefix = 0;` |
|     83 | 4020 | `	}else if( iSlash == 0 ){` |
|      - | 4021 | ``		/* the pattern is rooted: the directory is `/` itself */`` |
|    ! 0 | 4022 | `		zDir[0] = '/';` |
|    ! 0 | 4023 | `		nDir = 1;` |
|    ! 0 | 4024 | `		nPrefix = 1;` |
|    ! 0 | 4025 | `	}else{` |
|     83 | 4026 | `		nDir = iSlash;` |
|     83 | 4027 | `		SyMemcpy(zPat,zDir,(sxu32)nDir);` |
|     83 | 4028 | `		nPrefix = iSlash + 1;` |
|      - | 4029 | `	}` |
|     83 | 4030 | `	zDir[nDir] = 0;` |
|     83 | 4031 | `	zSeg = &zPat[iSlash + 1];` |
|     83 | 4032 | `	nSeg = nPat - (iSlash + 1);` |
|     83 | 4033 | `	if( nSeg >= (int)sizeof(zSegBuf) ){` |
|    ! 0 | 4034 | `		return SXRET_OK;` |
|      - | 4035 | `	}` |
|     83 | 4036 | `	SyMemcpy(zSeg,zSegBuf,(sxu32)nSeg);` |
|     83 | 4037 | `	zSegBuf[nSeg] = 0;` |
|      - | 4038 | `	/* The directory is opened through the SAME lookup opendir() uses, so the` |
|      - | 4039 | `	 * two implementations see one filesystem: the prelude glob() reaches it by` |
|      - | 4040 | `	 * calling opendir() itself. */` |
|     83 | 4041 | `	zDev = zDir;` |
|     83 | 4042 | `	pStream = PH7_VmGetStreamDevice(pVm,&zDev,nDir);` |
|     83 | 4043 | `	if( pStream == 0 \|\| pStream->xOpenDir == 0 \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 4044 | `		return SXRET_OK;` |
|      - | 4045 | `	}` |
|      - | 4046 | `	/* The VFS reports a name by writing a RESULT, so the read needs a call` |
|      - | 4047 | `	 * context of its own, and the cursor is reset between entries because a` |
|      - | 4048 | `	 * result APPENDS (rule 54). The same value carries the VM into the open:` |
|      - | 4049 | `	 * a device reaches it through that argument and nothing else. */` |
|     83 | 4050 | `	PH7_MemObjInit(pVm,&sEntry);` |
|     83 | 4051 | `	if( pStream->xOpenDir(zDev,&sEntry,&pHandle) != PH7_OK ){` |
|      3 | 4052 | `		PH7_MemObjRelease(&sEntry);` |
|      3 | 4053 | `		return SXRET_OK;` |
|      - | 4054 | `	}` |
|     81 | 4055 | `	VmInitCallContext(&sCtx,pVm,0,&sEntry,0);` |
|    479 | 4056 | `	for(;;){` |
|      - | 4057 | `		const char *zName;` |
|    873 | 4058 | `		int nName = 0;` |
|    873 | 4059 | `		ph7_value_reset_string_cursor(&sEntry);` |
|    873 | 4060 | `		if( pStream->xReadDir(pHandle,&sCtx) != PH7_OK ){` |
|     81 | 4061 | `			break;` |
|      - | 4062 | `		}` |
|    793 | 4063 | `		zName = ph7_value_to_string(&sEntry,&nName);` |
|    792 | 4064 | `		if( nName < 1 \|\| nName >= (int)sizeof(zEnt)` |
|    793 | 4065 | `		 \|\| nPrefix + nName >= (int)sizeof(zEnt) ){` |
|    ! 0 | 4066 | `			continue;` |
|      - | 4067 | `		}` |
|    793 | 4068 | `		SyMemcpy(zName,zEnt,(sxu32)nName);` |
|    793 | 4069 | `		zEnt[nName] = 0;` |
|      - | 4070 | `		/* php's FNM_PERIOD: a leading dot is matched only by a pattern that` |
|      - | 4071 | ``		 * spells one, which is what keeps `.`, `..` and every hidden name out`` |
|      - | 4072 | ``		 * of an ordinary `*`. */`` |
|    793 | 4073 | `		if( zEnt[0] == '.' && (nSeg < 1 \|\| zSegBuf[0] != '.') ){` |
|    201 | 4074 | `			continue;` |
|      - | 4075 | `		}` |
|    593 | 4076 | `		if( !Glob((const unsigned char *)zSegBuf,(const unsigned char *)zEnt,'\\',0,FALSE) ){` |
|    381 | 4077 | `			continue;` |
|      - | 4078 | `		}` |
|    213 | 4079 | `		if( bOnlyDir ){` |
|      - | 4080 | `			/* GLOB_ONLYDIR, which only the trailing-slash branch below asks` |
|      - | 4081 | `			 * for -- the device itself always globs with no flags at all. */` |
|      - | 4082 | `			char zProbe[PH7_GLOB_PATH_MAX * 2];` |
|     43 | 4083 | `			SyMemcpy(zDir,zProbe,(sxu32)nDir);` |
|     43 | 4084 | `			zProbe[nDir] = '/';` |
|     43 | 4085 | `			SyMemcpy(zEnt,&zProbe[nDir+1],(sxu32)nName);` |
|     43 | 4086 | `			zProbe[nDir + 1 + nName] = 0;` |
|     43 | 4087 | `			if( pVfs == 0 \|\| pVfs->xIsdir == 0 \|\| pVfs->xIsdir(zProbe) != PH7_OK ){` |
|     19 | 4088 | `				continue;` |
|      - | 4089 | `			}` |
|     12 | 4090 | `		}` |
|    195 | 4091 | `		rc = GlobAdd(pHit,pSet,zPat,(sxu32)nPrefix,zEnt,(sxu32)nName);` |
|    195 | 4092 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4093 | `			break;` |
|      - | 4094 | `		}` |
|      1 | 4095 | `	}` |
|     81 | 4096 | `	VmReleaseCallContext(&sCtx);` |
|     81 | 4097 | `	PH7_MemObjRelease(&sEntry);` |
|     81 | 4098 | `	if( pStream->xCloseDir ){` |
|     81 | 4099 | `		pStream->xCloseDir(pHandle);` |
|     40 | 4100 | `	}` |
|     81 | 4101 | `	return rc;` |
|     42 | 4102 | `}` |
|      - | 4103 | `/*` |
|      - | 4104 | ` * One pattern, every segment of it. php's three branches, in php's order: a` |
|      - | 4105 | ` * pattern that ENDS in a slash names directories and KEEPS the slash; a` |
|      - | 4106 | ` * wildcard in the directory part is walked level by level; anything else is` |
|      - | 4107 | ` * one directory read. Each branch sorts the range it produced.` |
|      - | 4108 | ` */` |
|    104 | 4109 | `static sxi32 GlobExpand(ph7_vm *pVm,const char *zPat,int nPat,int bOnlyDir,` |
|      - | 4110 | `	SyBlob *pHit,SySet *pSet)` |
|      1 | 4111 | `{` |
|    105 | 4112 | `	sxu32 nStart = SySetUsed(pSet);` |
|      - | 4113 | `	SyBlob sSub;` |
|      - | 4114 | `	SySet aSub;` |
|      - | 4115 | `	glob_hit *aRec;` |
|      - | 4116 | `	sxu32 n,nRec;` |
|    105 | 4117 | `	int i,iSlash = -1;` |
|      - | 4118 | `	sxi32 rc;` |
|    105 | 4119 | `	if( nPat < 1 \|\| nPat >= PH7_GLOB_PATH_MAX ){` |
|    ! 0 | 4120 | `		return SXRET_OK;` |
|      - | 4121 | `	}` |
|    105 | 4122 | `	if( zPat[nPat-1] == '/' ){` |
|      - | 4123 | ``		/* `d/` is ['d/'] and `d/` + `*` + `/` is ['d/a/','d/b/']: answer the base as`` |
|      - | 4124 | `		 * DIRECTORIES and put ONE slash back, so a pattern ending in two keeps` |
|      - | 4125 | `		 * both. php sorts the names it ANSWERS, slash included. */` |
|     13 | 4126 | `		const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|     13 | 4127 | `		if( nPat == 1 ){` |
|    ! 0 | 4128 | `			if( pVfs && pVfs->xIsdir && pVfs->xIsdir("/") == PH7_OK ){` |
|    ! 0 | 4129 | `				return GlobAdd(pHit,pSet,"/",1,0,0);` |
|      - | 4130 | `			}` |
|    ! 0 | 4131 | `			return SXRET_OK;` |
|      - | 4132 | `		}` |
|     13 | 4133 | `		SyBlobInit(&sSub,&pVm->sAllocator);` |
|     13 | 4134 | `		SySetInit(&aSub,&pVm->sAllocator,sizeof(glob_hit));` |
|     13 | 4135 | `		rc = GlobExpand(pVm,zPat,nPat-1,TRUE,&sSub,&aSub);` |
|     13 | 4136 | `		if( rc == SXRET_OK ){` |
|     13 | 4137 | `			aRec = (glob_hit *)SySetBasePtr(&aSub);` |
|     13 | 4138 | `			nRec = SySetUsed(&aSub);` |
|     37 | 4139 | `			for( n = 0 ; n < nRec ; ++n ){` |
|     37 | 4140 | `				rc = GlobAdd(pHit,pSet,` |
|     24 | 4141 | `					&((const char *)SyBlobData(&sSub))[aRec[n].nOfs],aRec[n].nLen,"/",1);` |
|     25 | 4142 | `				if( rc != SXRET_OK ){` |
|    ! 0 | 4143 | `					break;` |
|      - | 4144 | `				}` |
|     13 | 4145 | `			}` |
|      6 | 4146 | `		}` |
|     13 | 4147 | `		SyBlobRelease(&sSub);` |
|     13 | 4148 | `		SySetRelease(&aSub);` |
|     13 | 4149 | `		if( rc == SXRET_OK ){` |
|     13 | 4150 | `			GlobSort(pHit,pSet,nStart);` |
|      6 | 4151 | `		}` |
|     13 | 4152 | `		return rc;` |
|      - | 4153 | `	}` |
|    467 | 4154 | `	for( i = nPat - 1 ; i >= 0 ; --i ){` |
|    467 | 4155 | `		if( zPat[i] == '/' ){` |
|     93 | 4156 | `			iSlash = i;` |
|     93 | 4157 | `			break;` |
|      - | 4158 | `		}` |
|    188 | 4159 | `	}` |
|     93 | 4160 | `	if( iSlash > 0 && GlobHasMeta(zPat,iSlash) ){` |
|      - | 4161 | `		/* A wildcard in the DIRECTORY part is matched level by level, which is` |
|      - | 4162 | `		 * what glob(3) does: list the directories that part names, then glob` |
|      - | 4163 | `		 * the last component inside each. Reading only the last component` |
|      - | 4164 | ``		 * answers [] for `src/` + `*` + `/` + `*.php`, the everyday two-level`` |
|      - | 4165 | `		 * spelling, and for every deeper one. */` |
|     11 | 4166 | `		SyBlobInit(&sSub,&pVm->sAllocator);` |
|     11 | 4167 | `		SySetInit(&aSub,&pVm->sAllocator,sizeof(glob_hit));` |
|     11 | 4168 | `		rc = GlobExpand(pVm,zPat,iSlash+1,FALSE,&sSub,&aSub);` |
|     11 | 4169 | `		if( rc == SXRET_OK ){` |
|      - | 4170 | `			SyBlob sJoin;` |
|     11 | 4171 | `			SyBlobInit(&sJoin,&pVm->sAllocator);` |
|     11 | 4172 | `			aRec = (glob_hit *)SySetBasePtr(&aSub);` |
|     11 | 4173 | `			nRec = SySetUsed(&aSub);` |
|     31 | 4174 | `			for( n = 0 ; n < nRec ; ++n ){` |
|     21 | 4175 | `				SyBlobReset(&sJoin);` |
|     20 | 4176 | `				if( SyBlobAppend(&sJoin,` |
|     30 | 4177 | `					&((const char *)SyBlobData(&sSub))[aRec[n].nOfs],aRec[n].nLen) != SXRET_OK` |
|     21 | 4178 | `				 \|\| SyBlobAppend(&sJoin,&zPat[iSlash+1],(sxu32)(nPat - iSlash - 1)) != SXRET_OK ){` |
|    ! 0 | 4179 | `					rc = SXERR_MEM;` |
|    ! 0 | 4180 | `					break;` |
|      - | 4181 | `				}` |
|     31 | 4182 | `				rc = GlobExpand(pVm,(const char *)SyBlobData(&sJoin),` |
|     20 | 4183 | `					(int)SyBlobLength(&sJoin),bOnlyDir,pHit,pSet);` |
|     21 | 4184 | `				if( rc != SXRET_OK ){` |
|    ! 0 | 4185 | `					break;` |
|      - | 4186 | `				}` |
|     11 | 4187 | `			}` |
|     11 | 4188 | `			SyBlobRelease(&sJoin);` |
|      5 | 4189 | `		}` |
|     11 | 4190 | `		SyBlobRelease(&sSub);` |
|     11 | 4191 | `		SySetRelease(&aSub);` |
|     11 | 4192 | `		if( rc == SXRET_OK ){` |
|     11 | 4193 | `			GlobSort(pHit,pSet,nStart);` |
|      5 | 4194 | `		}` |
|     11 | 4195 | `		return rc;` |
|      - | 4196 | `	}` |
|     83 | 4197 | `	rc = GlobLeaf(pVm,zPat,nPat,bOnlyDir,pHit,pSet);` |
|     83 | 4198 | `	if( rc == SXRET_OK ){` |
|     83 | 4199 | `		GlobSort(pHit,pSet,nStart);` |
|     41 | 4200 | `	}` |
|     83 | 4201 | `	return rc;` |
|     53 | 4202 | `}` |
|      - | 4203 | `/*` |
|      - | 4204 | ` * Every entry of one directory, in the shape above. This is php's` |
|      - | 4205 | `` * `php_stream_scandir` with its alphasort comparator, which ZipArchive::`` |
|      - | 4206 | ``  * addPattern() walks: unlike a glob it keeps the DOTTED names -- `.` and `..` `` |
|      - | 4207 | ` * among them -- because php's caller is the one that decides what to do with` |
|      - | 4208 | ` * them, and it decides by STATTING each rather than by looking at the name.` |
|      - | 4209 | ` */` |
|    ! 0 | 4210 | `PH7_PRIVATE sxi32 PH7_VfsListDir(ph7_vm *pVm,const char *zDir,int nDir,SyBlob *pHit,SySet *pSet)` |
|    ! 0 | 4211 | `{` |
|      - | 4212 | `	const ph7_io_stream *pStream;` |
|      - | 4213 | `	const char *zDev;` |
|      - | 4214 | `	void *pHandle;` |
|      - | 4215 | `	ph7_value sEntry;` |
|      - | 4216 | `	ph7_context sCtx;` |
|      - | 4217 | `	char zPath[PH7_GLOB_PATH_MAX];` |
|    ! 0 | 4218 | `	sxu32 nStart = SySetUsed(pSet);` |
|    ! 0 | 4219 | `	sxi32 rc = SXRET_OK;` |
|    ! 0 | 4220 | `	if( nDir < 1 \|\| nDir >= (int)sizeof(zPath) ){` |
|    ! 0 | 4221 | `		return SXERR_INVALID;` |
|      - | 4222 | `	}` |
|    ! 0 | 4223 | `	SyMemcpy(zDir,zPath,(sxu32)nDir);` |
|    ! 0 | 4224 | `	zPath[nDir] = 0;` |
|    ! 0 | 4225 | `	zDev = zPath;` |
|    ! 0 | 4226 | `	pStream = PH7_VmGetStreamDevice(&(*pVm),&zDev,nDir);` |
|    ! 0 | 4227 | `	if( pStream == 0 \|\| pStream->xOpenDir == 0 \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 4228 | `		return SXERR_IO;` |
|      - | 4229 | `	}` |
|    ! 0 | 4230 | `	PH7_MemObjInit(pVm,&sEntry);` |
|    ! 0 | 4231 | `	if( pStream->xOpenDir(zDev,&sEntry,&pHandle) != PH7_OK ){` |
|    ! 0 | 4232 | `		PH7_MemObjRelease(&sEntry);` |
|    ! 0 | 4233 | `		return SXERR_IO;` |
|      - | 4234 | `	}` |
|    ! 0 | 4235 | `	VmInitCallContext(&sCtx,pVm,0,&sEntry,0);` |
|    ! 0 | 4236 | `	for(;;){` |
|      - | 4237 | `		const char *zName;` |
|    ! 0 | 4238 | `		int nName = 0;` |
|      - | 4239 | `		PH7_GlobHit sHit;` |
|    ! 0 | 4240 | `		ph7_value_reset_string_cursor(&sEntry);` |
|    ! 0 | 4241 | `		if( pStream->xReadDir(pHandle,&sCtx) != PH7_OK ){` |
|    ! 0 | 4242 | `			break;` |
|      - | 4243 | `		}` |
|    ! 0 | 4244 | `		zName = ph7_value_to_string(&sEntry,&nName);` |
|    ! 0 | 4245 | `		if( nName < 1 ){` |
|    ! 0 | 4246 | `			continue;` |
|      - | 4247 | `		}` |
|    ! 0 | 4248 | `		sHit.nOfs = SyBlobLength(pHit);` |
|    ! 0 | 4249 | `		sHit.nLen = (sxu32)nName;` |
|    ! 0 | 4250 | `		SyBlobAppend(pHit,zName,(sxu32)nName);` |
|    ! 0 | 4251 | `		rc = SySetPut(pSet,(const void *)&sHit);` |
|    ! 0 | 4252 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4253 | `			break;` |
|      - | 4254 | `		}` |
|    ! 0 | 4255 | `	}` |
|    ! 0 | 4256 | `	VmReleaseCallContext(&sCtx);` |
|    ! 0 | 4257 | `	PH7_MemObjRelease(&sEntry);` |
|    ! 0 | 4258 | `	if( pStream->xCloseDir ){` |
|    ! 0 | 4259 | `		pStream->xCloseDir(pHandle);` |
|    ! 0 | 4260 | `	}` |
|    ! 0 | 4261 | `	if( rc == SXRET_OK ){` |
|    ! 0 | 4262 | `		GlobSort(pHit,pSet,nStart);` |
|    ! 0 | 4263 | `	}` |
|    ! 0 | 4264 | `	return rc;` |
|    ! 0 | 4265 | `}` |
|      - | 4266 | `/* void (*xCloseDir)(void *) */` |
|     62 | 4267 | `static void GlobStream_CloseDir(void *pHandle)` |
|      1 | 4268 | `{` |
|     63 | 4269 | `	glob_stream *pGlob = (glob_stream *)pHandle;` |
|      - | 4270 | `	ph7_vm *pVm;` |
|     63 | 4271 | `	if( pGlob == 0 ){` |
|    ! 0 | 4272 | `		return;` |
|      - | 4273 | `	}` |
|     63 | 4274 | `	pVm = pGlob->pVm;` |
|     63 | 4275 | `	SyBlobRelease(&pGlob->sHit);` |
|     63 | 4276 | `	SyBlobRelease(&pGlob->sDir);` |
|     63 | 4277 | `	SySetRelease(&pGlob->aHit);` |
|     63 | 4278 | `	SyMemBackendFree(&pVm->sAllocator,pGlob);` |
|     32 | 4279 | `}` |
|      - | 4280 | `/*` |
|      - | 4281 | ` * int (*xOpenDir)(const char *,ph7_value *,void **)` |
|      - | 4282 | ` *` |
|      - | 4283 | ` * php's opener fails only on a glob(3) ERROR: no matches at all is an OPEN` |
|      - | 4284 | `` * stream with nothing in it, which is why `new GlobIterator('nope/' . '*')` is a`` |
|      - | 4285 | ` * working object whose count() is 0 rather than a constructor that throws.` |
|      - | 4286 | ` *` |
|      - | 4287 | ` * The VM comes in through the context argument, the way data:// takes it: the` |
|      - | 4288 | ` * walk allocates, and reads a directory through a call context of its own.` |
|      - | 4289 | ` */` |
|     62 | 4290 | `static int GlobStream_OpenDir(const char *zPattern,ph7_value *pResource,void **ppHandle)` |
|      1 | 4291 | `{` |
|     63 | 4292 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|      - | 4293 | `	glob_stream *pGlob;` |
|     63 | 4294 | `	if( pVm == 0 ){` |
|    ! 0 | 4295 | `		return -1;` |
|      - | 4296 | `	}` |
|     63 | 4297 | `	pGlob = (glob_stream *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(glob_stream));` |
|     63 | 4298 | `	if( pGlob == 0 ){` |
|    ! 0 | 4299 | `		return -1;` |
|      - | 4300 | `	}` |
|     63 | 4301 | `	pGlob->pVm = pVm;` |
|     63 | 4302 | `	pGlob->nCur = 0;` |
|     63 | 4303 | `	SyBlobInit(&pGlob->sHit,&pVm->sAllocator);` |
|     63 | 4304 | `	SyBlobInit(&pGlob->sDir,&pVm->sAllocator);` |
|     63 | 4305 | `	SySetInit(&pGlob->aHit,&pVm->sAllocator,sizeof(glob_hit));` |
|      - | 4306 | `	/* A pattern longer than one path is zero matches rather than a refusal (it` |
|      - | 4307 | `	 * can name nothing that exists), which is glob(3)'s GLOB_NOMATCH and an` |
|      - | 4308 | `	 * open stream either way. */` |
|     93 | 4309 | `	if( GlobExpand(pVm,zPattern,(int)SyStrlen(zPattern),FALSE,` |
|     63 | 4310 | `		&pGlob->sHit,&pGlob->aHit) != SXRET_OK ){` |
|    ! 0 | 4311 | `		GlobStream_CloseDir(pGlob);` |
|    ! 0 | 4312 | `		return -1;` |
|      - | 4313 | `	}` |
|     63 | 4314 | `	*ppHandle = (void *)pGlob;` |
|     63 | 4315 | `	return PH7_OK;` |
|     32 | 4316 | `}` |
|      - | 4317 | `/*` |
|      - | 4318 | ` * int (*xReadDir)(void *,ph7_context *)` |
|      - | 4319 | ` *` |
|      - | 4320 | ` * php's php_glob_stream_path_split, which runs on every read: the directory is` |
|      - | 4321 | ` * everything before the LAST slash and the ENTRY is what follows it. So a` |
|      - | 4322 | ``  * match with no slash in it reports an empty directory and itself, `a/` `` |
|      - | 4323 | `` * reports `a` and an EMPTY entry -- and an empty entry is what the SPL walk`` |
|      - | 4324 | ` * reads as the end, which is why GlobIterator over a trailing-slash pattern` |
|      - | 4325 | ` * counts its matches and yields none of them.` |
|      - | 4326 | ` */` |
|    208 | 4327 | `static int GlobStream_ReadDir(void *pHandle,ph7_context *pCtx)` |
|      1 | 4328 | `{` |
|    209 | 4329 | `	glob_stream *pGlob = (glob_stream *)pHandle;` |
|      - | 4330 | `	glob_hit *aHit;` |
|      - | 4331 | `	const char *zPath;` |
|      - | 4332 | `	sxu32 nPath;` |
|    209 | 4333 | `	int i,iSlash = -1;` |
|    209 | 4334 | `	if( pGlob == 0 ){` |
|    ! 0 | 4335 | `		return -1;` |
|      - | 4336 | `	}` |
|    209 | 4337 | `	if( pGlob->nCur >= SySetUsed(&pGlob->aHit) ){` |
|      - | 4338 | `		/* php drops the path when the walk runs out, and counts on past it. */` |
|     49 | 4339 | `		pGlob->nCur++;` |
|     49 | 4340 | `		SyBlobReset(&pGlob->sDir);` |
|     49 | 4341 | `		return -1;` |
|      - | 4342 | `	}` |
|    161 | 4343 | `	aHit = (glob_hit *)SySetBasePtr(&pGlob->aHit);` |
|    161 | 4344 | `	zPath = &((const char *)SyBlobData(&pGlob->sHit))[aHit[pGlob->nCur].nOfs];` |
|    161 | 4345 | `	nPath = aHit[pGlob->nCur].nLen;` |
|    161 | 4346 | `	pGlob->nCur++;` |
|    891 | 4347 | `	for( i = (int)nPath - 1 ; i >= 0 ; --i ){` |
|    891 | 4348 | `		if( zPath[i] == '/' ){` |
|    161 | 4349 | `			iSlash = i;` |
|    161 | 4350 | `			break;` |
|      - | 4351 | `		}` |
|    366 | 4352 | `	}` |
|    161 | 4353 | `	SyBlobReset(&pGlob->sDir);` |
|    161 | 4354 | `	if( iSlash >= 0 ){` |
|    161 | 4355 | `		if( iSlash > 0 && SyBlobAppend(&pGlob->sDir,zPath,(sxu32)iSlash) != SXRET_OK ){` |
|    ! 0 | 4356 | `			return -1;` |
|      - | 4357 | `		}` |
|    161 | 4358 | `		ph7_result_string(pCtx,&zPath[iSlash+1],(int)nPath - iSlash - 1);` |
|     81 | 4359 | `	}else{` |
|    ! 0 | 4360 | `		ph7_result_string(pCtx,zPath,(int)nPath);` |
|      - | 4361 | `	}` |
|    161 | 4362 | `	return PH7_OK;` |
|    105 | 4363 | `}` |
|      - | 4364 | `/* void (*xRewindDir)(void *): php's rewind moves the INDEX and leaves the path` |
|      - | 4365 | ` * where the last read put it -- every caller reads straight afterwards. */` |
|     12 | 4366 | `static void GlobStream_RewindDir(void *pHandle)` |
|      1 | 4367 | `{` |
|     13 | 4368 | `	glob_stream *pGlob = (glob_stream *)pHandle;` |
|     13 | 4369 | `	if( pGlob ){` |
|     13 | 4370 | `		pGlob->nCur = 0;` |
|      6 | 4371 | `	}` |
|     13 | 4372 | `}` |
|      - | 4373 | `PH7_PRIVATE const ph7_io_stream sGLOB_Stream = {` |
|      - | 4374 | `	"glob",` |
|      - | 4375 | `	PH7_IO_STREAM_VERSION,` |
|      - | 4376 | `	0,                    /* xOpen: php's wrapper has no stream opener at all */` |
|      - | 4377 | `	GlobStream_OpenDir,   /* xOpenDir */` |
|      - | 4378 | `	0,                    /* xClose */` |
|      - | 4379 | `	GlobStream_CloseDir,  /* xCloseDir */` |
|      - | 4380 | `	0,                    /* xRead */` |
|      - | 4381 | `	GlobStream_ReadDir,   /* xReadDir */` |
|      - | 4382 | `	0,                    /* xWrite */` |
|      - | 4383 | `	0,                    /* xSeek */` |
|      - | 4384 | `	0,                    /* xLock */` |
|      - | 4385 | `	GlobStream_RewindDir, /* xRewindDir */` |
|      - | 4386 | `	0,                    /* xTell */` |
|      - | 4387 | `	0,                    /* xTrunc */` |
|      - | 4388 | `	0,                    /* xSync */` |
|      - | 4389 | `	0                     /* xStat */` |
|      - | 4390 | `};` |
|      - | 4391 | `/* Is this the glob device? php's php_stream_is(), which is how SPL tells a` |
|      - | 4392 | ` * GlobIterator's directory handle from an ordinary one. */` |
|    195 | 4393 | `PH7_PRIVATE int PH7_GlobStreamIs(const ph7_io_stream *pStream)` |
|      2 | 4394 | `{` |
|    197 | 4395 | `	return pStream == &sGLOB_Stream;` |
|      2 | 4396 | `}` |
|      - | 4397 | `/* php's php_glob_stream_get_path: the directory of the CURRENT match, empty` |
|      - | 4398 | ` * both before the first read and after the last. */` |
|     60 | 4399 | `PH7_PRIVATE const char * PH7_GlobStreamPath(void *pHandle,int *pnLen)` |
|      1 | 4400 | `{` |
|     61 | 4401 | `	glob_stream *pGlob = (glob_stream *)pHandle;` |
|     61 | 4402 | `	if( pGlob == 0 ){` |
|    ! 0 | 4403 | `		*pnLen = 0;` |
|    ! 0 | 4404 | `		return "";` |
|      - | 4405 | `	}` |
|     61 | 4406 | `	*pnLen = (int)SyBlobLength(&pGlob->sDir);` |
|     61 | 4407 | `	return *pnLen > 0 ? (const char *)SyBlobData(&pGlob->sDir) : "";` |
|     31 | 4408 | `}` |
|      - | 4409 | `/* php's php_glob_stream_get_count: what GlobIterator::count() answers, and it` |
|      - | 4410 | ` * does not move with the walk. */` |
|     14 | 4411 | `PH7_PRIVATE sxi64 PH7_GlobStreamCount(void *pHandle)` |
|      1 | 4412 | `{` |
|     15 | 4413 | `	glob_stream *pGlob = (glob_stream *)pHandle;` |
|     15 | 4414 | `	return pGlob ? (sxi64)SySetUsed(&pGlob->aHit) : 0;` |
|      1 | 4415 | `}` |
|      - | 4416 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 4417 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 4418 | `/*` |
|      - | 4419 | ` * bool link(string $target,string $link)` |
|      - | 4420 |  |
|      - | 4421 | ` *  Create a hard link.` |
|      - | 4422 | ` * Parameters` |
|      - | 4423 | ` *  $target` |
|      - | 4424 | ` *   Target of the link.` |
|      - | 4425 | ` *  $link` |
|      - | 4426 | ` *   The link name.` |
|      - | 4427 | ` * Return` |
|      - | 4428 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4429 | ` */` |
|     10 | 4430 | `static int PH7_vfs_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4431 | `{` |
|      - | 4432 | `	const char *zTarget,*zLink;` |
|      - | 4433 | `	ph7_vfs *pVfs;` |
|      - | 4434 | `	int rc;` |
|     11 | 4435 | `	if( nArg < 2 \|\| !VfsPathArgUsable(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 4436 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4437 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4438 | `		return PH7_OK;` |
|      - | 4439 | `	}` |
|      - | 4440 | `	/* Point to the underlying vfs */` |
|     11 | 4441 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 4442 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 4443 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 4444 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4445 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 4446 | `			ph7_function_name(pCtx)` |
|      - | 4447 | `			);` |
|    ! 0 | 4448 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4449 | `		return PH7_OK;` |
|      - | 4450 | `	}` |
|      - | 4451 | `	/* Extract the given arguments */` |
|     11 | 4452 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|     11 | 4453 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 4454 | `	/* Perform the requested operation */` |
|     11 | 4455 | `	errno = 0;` |
|     11 | 4456 | `	rc = pVfs->xLink(zTarget,zLink,0/*Not a symbolic link */);` |
|     11 | 4457 | `	if( rc != PH7_OK ){` |
|      - | 4458 | `		/* php's no-path shape again, the same chmod() takes. PHL answered FALSE` |
|      - | 4459 | `		 * in silence, so a failed link was indistinguishable from a made one` |
|      - | 4460 | `		 * without testing the return value. */` |
|      6 | 4461 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 | 4462 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|      2 | 4463 | `	}` |
|      - | 4464 | `	/* IO result */` |
|     11 | 4465 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|     11 | 4466 | `	return PH7_OK;` |
|      6 | 4467 | `}` |
|      - | 4468 | `/*` |
|      - | 4469 | ` * string\|false readlink(string $path)` |
|      - | 4470 | ` *  Returns the target of a symbolic link.` |
|      - | 4471 | ` * Parameters` |
|      - | 4472 | ` *  $path` |
|      - | 4473 | ` *   The symbolic link path.` |
|      - | 4474 | ` * Return` |
|      - | 4475 | ` *  The contents of the link, or FALSE (with a warning) when $path is not a link` |
|      - | 4476 | ` *  or cannot be read -- php's own answer, error text included.` |
|      - | 4477 | ` */` |
|      8 | 4478 | `static int PH7_vfs_readlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 4479 | `{` |
|      - | 4480 | `	const char *zPath;` |
|      - | 4481 | `	ph7_vfs *pVfs;` |
|      - | 4482 | `	int rc;` |
|      8 | 4483 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|    ! 0 | 4484 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4485 | `		return PH7_OK;` |
|      - | 4486 | `	}` |
|      8 | 4487 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      8 | 4488 | `	if( pVfs == 0 \|\| pVfs->xReadlink == 0 ){` |
|    ! 0 | 4489 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4490 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 4491 | `			ph7_function_name(pCtx)` |
|      - | 4492 | `			);` |
|    ! 0 | 4493 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4494 | `		return PH7_OK;` |
|      - | 4495 | `	}` |
|      8 | 4496 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      8 | 4497 | `	rc = pVfs->xReadlink(zPath,pCtx);` |
|      8 | 4498 | `	if( rc != PH7_OK ){` |
|      - | 4499 | `		/* php's wording is the errno text alone -- the engine prefixes the` |
|      - | 4500 | `		 * function name already. */` |
|      6 | 4501 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      4 | 4502 | `			"%s",VfsStrerror(errno));` |
|      4 | 4503 | `		ph7_result_bool(pCtx,0);` |
|      2 | 4504 | `	}` |
|      8 | 4505 | `	return PH7_OK;` |
|      4 | 4506 | `}` |
|      - | 4507 | `/*` |
|      - | 4508 | ` * bool symlink(string $target,string $link)` |
|      - | 4509 | ` *  Creates a symbolic link.` |
|      - | 4510 | ` * Parameters` |
|      - | 4511 | ` *  $target` |
|      - | 4512 | ` *   Target of the link.` |
|      - | 4513 | ` *  $link` |
|      - | 4514 | ` *   The link name.` |
|      - | 4515 | ` * Return` |
|      - | 4516 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4517 | ` */` |
|     20 | 4518 | `static int PH7_vfs_symlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4519 | `{` |
|      - | 4520 | `	const char *zTarget,*zLink;` |
|      - | 4521 | `	ph7_vfs *pVfs;` |
|      - | 4522 | `	int rc;` |
|     21 | 4523 | `	if( nArg < 2 \|\| !VfsPathArgUsable(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 4524 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4525 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4526 | `		return PH7_OK;` |
|      - | 4527 | `	}` |
|      - | 4528 | `	/* Point to the underlying vfs */` |
|     21 | 4529 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     21 | 4530 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 4531 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 4532 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4533 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 4534 | `			ph7_function_name(pCtx)` |
|      - | 4535 | `			);` |
|    ! 0 | 4536 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4537 | `		return PH7_OK;` |
|      - | 4538 | `	}` |
|      - | 4539 | `	/* Extract the given arguments */` |
|     21 | 4540 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|     21 | 4541 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 4542 | `	/* Perform the requested operation */` |
|     21 | 4543 | `	errno = 0;` |
|     21 | 4544 | `	rc = pVfs->xLink(zTarget,zLink,1/*A symbolic link */);` |
|     21 | 4545 | `	if( rc != PH7_OK ){` |
|      - | 4546 | `		/* php's no-path shape again, the same chmod() takes. PHL answered FALSE` |
|      - | 4547 | `		 * in silence, so a failed link was indistinguishable from a made one` |
|      - | 4548 | `		 * without testing the return value. */` |
|      3 | 4549 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      2 | 4550 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|      1 | 4551 | `	}` |
|      - | 4552 | `	/* IO result */` |
|     21 | 4553 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|     21 | 4554 | `	return PH7_OK;` |
|     11 | 4555 | `}` |
|      - | 4556 | `/*` |
|      - | 4557 | ` * int umask([ int $mask ])` |
|      - | 4558 | ` *  Changes the current umask.` |
|      - | 4559 | ` * Parameters` |
|      - | 4560 | ` *  $mask` |
|      - | 4561 | ` *   The new umask.` |
|      - | 4562 | ` * Return` |
|      - | 4563 | ` *  umask() without arguments simply returns the current umask.` |
|      - | 4564 | ` *  Otherwise the old umask is returned.` |
|      - | 4565 | ` */` |
|      8 | 4566 | `static int PH7_vfs_umask(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4567 | `{` |
|      - | 4568 | `	int iOld,iNew;` |
|      - | 4569 | `	ph7_vfs *pVfs;` |
|      - | 4570 | `	/* Point to the underlying vfs */` |
|      9 | 4571 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 | 4572 | `	if( pVfs == 0 \|\| pVfs->xUmask == 0 ){` |
|      - | 4573 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 4574 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4575 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 4576 | `			ph7_function_name(pCtx)` |
|      - | 4577 | `			);` |
|    ! 0 | 4578 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4579 | `		return PH7_OK;` |
|      - | 4580 | `	}` |
|      9 | 4581 | `	iNew = 0;` |
|      9 | 4582 | `	if( nArg > 0 ){` |
|      5 | 4583 | `		iNew = ph7_value_to_int(apArg[0]);` |
|      2 | 4584 | `	}` |
|      - | 4585 | `	/* Perform the requested operation */` |
|      9 | 4586 | `	iOld = pVfs->xUmask(iNew);` |
|      - | 4587 | `	/* Old mask */` |
|      9 | 4588 | `	ph7_result_int(pCtx,iOld);` |
|      9 | 4589 | `	return PH7_OK;` |
|      5 | 4590 | `}` |
|      - | 4591 | `/*` |
|      - | 4592 | ` * string sys_get_temp_dir()` |
|      - | 4593 | ` *  Returns directory path used for temporary files.` |
|      - | 4594 | ` * Parameters` |
|      - | 4595 | ` *  None` |
|      - | 4596 | ` * Return` |
|      - | 4597 | ` *  Returns the path of the temporary directory.` |
|      - | 4598 | ` */` |
|   1704 | 4599 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 4600 | `{` |
|      - | 4601 | `	ph7_vfs *pVfs;` |
|      - | 4602 | `	/* Set the empty string as the default return value */` |
|   1709 | 4603 | `	ph7_result_string(pCtx,"",0);` |
|      - | 4604 | `	/* Point to the underlying vfs */` |
|   1709 | 4605 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   1709 | 4606 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
|    ! 0 | 4607 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 4608 | `		SXUNUSED(apArg);` |
|      - | 4609 | `		/* IO routine not implemented,return "" */` |
|    ! 0 | 4610 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4611 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 4612 | `			ph7_function_name(pCtx)` |
|      - | 4613 | `			);` |
|    ! 0 | 4614 | `		return PH7_OK;` |
|      - | 4615 | `	}` |
|      - | 4616 | `	/* Perform the requested operation */` |
|   1709 | 4617 | `	pVfs->xTempDir(pCtx);` |
|   1709 | 4618 | `	return PH7_OK;` |
|    844 | 4619 | `}` |
|      - | 4620 | `/*` |
|      - | 4621 | ` * string get_current_user()` |
|      - | 4622 | ` *  Returns the name of the current working user.` |
|      - | 4623 | ` * Parameters` |
|      - | 4624 | ` *  None` |
|      - | 4625 | ` * Return` |
|      - | 4626 | ` *  Returns the name of the current working user.` |
|      - | 4627 | ` */` |
|      2 | 4628 | `static int PH7_vfs_get_current_user(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4629 | `{` |
|      - | 4630 | `	ph7_vfs *pVfs;` |
|      - | 4631 | `	/* Point to the underlying vfs */` |
|      3 | 4632 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 4633 | `	if( pVfs == 0 \|\| pVfs->xUsername == 0 ){` |
|    ! 0 | 4634 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 4635 | `		SXUNUSED(apArg);` |
|      - | 4636 | `		/* IO routine not implemented */` |
|    ! 0 | 4637 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4638 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 4639 | `			ph7_function_name(pCtx)` |
|      - | 4640 | `			);` |
|      - | 4641 | `		/* Set a dummy username */` |
|    ! 0 | 4642 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 4643 | `		return PH7_OK;` |
|      - | 4644 | `	}` |
|      - | 4645 | `	/* Perform the requested operation */` |
|      3 | 4646 | `	pVfs->xUsername(pCtx);` |
|      3 | 4647 | `	return PH7_OK;` |
|      2 | 4648 | `}` |
|      - | 4649 | `/*` |
|      - | 4650 | ` * int64 getmypid()` |
|      - | 4651 | ` *  Gets process ID.` |
|      - | 4652 | ` * Parameters` |
|      - | 4653 | ` *  None` |
|      - | 4654 | ` * Return` |
|      - | 4655 | ` *  Returns the process ID.` |
|      - | 4656 | ` */` |
|    476 | 4657 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 4658 | `{` |
|      - | 4659 | `	ph7_int64 nProcessId;` |
|      - | 4660 | `	ph7_vfs *pVfs;` |
|      - | 4661 | `	/* Point to the underlying vfs */` |
|    481 | 4662 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    481 | 4663 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
|    ! 0 | 4664 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 4665 | `		SXUNUSED(apArg);` |
|      - | 4666 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 4667 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4668 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 4669 | `			ph7_function_name(pCtx)` |
|      - | 4670 | `			);` |
|    ! 0 | 4671 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 4672 | `		return PH7_OK;` |
|      - | 4673 | `	}` |
|      - | 4674 | `	/* Perform the requested operation */` |
|    481 | 4675 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 4676 | `	/* Set the result */` |
|    481 | 4677 | `	ph7_result_int64(pCtx,nProcessId);` |
|    481 | 4678 | `	return PH7_OK;` |
|    242 | 4679 | `}` |
|      - | 4680 | `/*` |
|      - | 4681 | ` * int getmyuid()` |
|      - | 4682 | ` *  Get user ID.` |
|      - | 4683 | ` * Parameters` |
|      - | 4684 | ` *  None` |
|      - | 4685 | ` * Return` |
|      - | 4686 | ` *  Returns the user ID.` |
|      - | 4687 | ` */` |
|      6 | 4688 | `static int PH7_vfs_getmyuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4689 | `{` |
|      - | 4690 | `	ph7_vfs *pVfs;` |
|      - | 4691 | `	int nUid;` |
|      - | 4692 | `	/* Point to the underlying vfs */` |
|      7 | 4693 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 4694 | `	if( pVfs == 0 \|\| pVfs->xUid == 0 ){` |
|    ! 0 | 4695 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 4696 | `		SXUNUSED(apArg);` |
|      - | 4697 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 4698 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4699 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 4700 | `			ph7_function_name(pCtx)` |
|      - | 4701 | `			);` |
|    ! 0 | 4702 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 4703 | `		return PH7_OK;` |
|      - | 4704 | `	}` |
|      - | 4705 | `	/* Perform the requested operation */` |
|      7 | 4706 | `	nUid = pVfs->xUid();` |
|      - | 4707 | `	/* Set the result */` |
|      7 | 4708 | `	ph7_result_int(pCtx,nUid);` |
|      7 | 4709 | `	return PH7_OK;` |
|      4 | 4710 | `}` |
|      - | 4711 | `/*` |
|      - | 4712 | ` * int getmygid()` |
|      - | 4713 | ` *  Get group ID.` |
|      - | 4714 | ` * Parameters` |
|      - | 4715 | ` *  None` |
|      - | 4716 | ` * Return` |
|      - | 4717 | ` *  Returns the group ID.` |
|      - | 4718 | ` */` |
|      4 | 4719 | `static int PH7_vfs_getmygid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4720 | `{` |
|      - | 4721 | `	ph7_vfs *pVfs;` |
|      - | 4722 | `	int nGid;` |
|      - | 4723 | `	/* Point to the underlying vfs */` |
|      5 | 4724 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 4725 | `	if( pVfs == 0 \|\| pVfs->xGid == 0 ){` |
|    ! 0 | 4726 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 4727 | `		SXUNUSED(apArg);` |
|      - | 4728 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 4729 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4730 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 4731 | `			ph7_function_name(pCtx)` |
|      - | 4732 | `			);` |
|    ! 0 | 4733 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 4734 | `		return PH7_OK;` |
|      - | 4735 | `	}` |
|      - | 4736 | `	/* Perform the requested operation */` |
|      5 | 4737 | `	nGid = pVfs->xGid();` |
|      - | 4738 | `	/* Set the result */` |
|      5 | 4739 | `	ph7_result_int(pCtx,nGid);` |
|      5 | 4740 | `	return PH7_OK;` |
|      3 | 4741 | `}` |
|      - | 4742 | `#ifdef __WINNT__` |
|      - | 4743 | `#include <Windows.h>` |
|      - | 4744 | `#elif defined(__UNIXES__)` |
|      - | 4745 | `#include <sys/utsname.h>` |
|      - | 4746 | `#endif` |
|      - | 4747 | `/*` |
|      - | 4748 | ` * php's five uname FIELDS, filled once per call. php answers one of them for a` |
|      - | 4749 | `` * single-letter mode and all five, space-separated, for `a` -- and the ORDER of`` |
|      - | 4750 | `` * that composite is `s n r v m`, the OS, the HOST, the release, the version and`` |
|      - | 4751 | `` * the machine. This engine used to answer `s r v n m`, so the host name stood`` |
|      - | 4752 | ` * in the version's place in every string a program logged.` |
|      - | 4753 | ` */` |
|      - | 4754 | `typedef struct vfs_uname_fields vfs_uname_fields;` |
|      - | 4755 | `struct vfs_uname_fields {` |
|      - | 4756 | `	const char *zSys;     /* 's' */` |
|      - | 4757 | `	const char *zNode;    /* 'n' */` |
|      - | 4758 | `	const char *zRel;     /* 'r' */` |
|      - | 4759 | `	const char *zVer;     /* 'v' */` |
|      - | 4760 | `	const char *zMachine; /* 'm' */` |
|      - | 4761 | `};` |
|      - | 4762 | `#if defined(__WINNT__)` |
|      - | 4763 | `/*` |
|      - | 4764 | ` * The product NAME php prints inside the version field. php reads the real` |
|      - | 4765 | ` * version through ntdll's RtlGetVersion (GetVersionEx() lies to any binary` |
|      - | 4766 | ` * without a compatibility manifest -- it answers 6.2 on Windows 10 and 11) and` |
|      - | 4767 | ` * names it from a table of its own. The rows below are the versions this port` |
|      - | 4768 | ` * targets and the only ones an oracle can be asked about; anything older` |
|      - | 4769 | ` * answers the bare "Windows" rather than a name nobody can verify.` |
|      - | 4770 | ` */` |
|      - | 4771 | `static const char * VfsWinProductName(unsigned long nMajor,unsigned long nMinor,` |
|      - | 4772 | `	unsigned long nBuild,int bWorkstation)` |
|      1 | 4773 | `{` |
|      1 | 4774 | `	if( nMajor == 10 && nMinor == 0 ){` |
|      1 | 4775 | `		if( bWorkstation ){` |
|    ! 0 | 4776 | `			return nBuild >= 22000 ? "Windows 11" : "Windows 10";` |
|      - | 4777 | `		}` |
|      1 | 4778 | `		if( nBuild >= 26100 ){` |
|      1 | 4779 | `			return "Windows Server 2025";` |
|      - | 4780 | `		}` |
|    ! 0 | 4781 | `		if( nBuild >= 20348 ){` |
|    ! 0 | 4782 | `			return "Windows Server 2022";` |
|      - | 4783 | `		}` |
|    ! 0 | 4784 | `		if( nBuild >= 17763 ){` |
|    ! 0 | 4785 | `			return "Windows Server 2019";` |
|      - | 4786 | `		}` |
|    ! 0 | 4787 | `		return "Windows Server 2016";` |
|      - | 4788 | `	}` |
|    ! 0 | 4789 | `	return "Windows";` |
|      1 | 4790 | `}` |
|      - | 4791 | `/*` |
|      - | 4792 | ` * The true OS version. RtlGetVersion is the only call that answers it for an` |
|      - | 4793 | ` * unmanifested binary, and it lives in ntdll rather than in an import library.` |
|      - | 4794 | ` */` |
|      - | 4795 | `static void VfsWinVersion(unsigned long *pnMajor,unsigned long *pnMinor,` |
|      - | 4796 | `	unsigned long *pnBuild,int *pbWorkstation)` |
|      1 | 4797 | `{` |
|      - | 4798 | `	/* RTL_OSVERSIONINFOEXW's documented layout, declared here rather than` |
|      - | 4799 | `	 * taken from a header: the name only appears in some SDK versions, and` |
|      - | 4800 | `	 * nothing else in this file needs ntdll. */` |
|      - | 4801 | `	typedef struct vfs_rtl_osversion {` |
|      - | 4802 | `		ULONG dwOSVersionInfoSize;` |
|      - | 4803 | `		ULONG dwMajorVersion;` |
|      - | 4804 | `		ULONG dwMinorVersion;` |
|      - | 4805 | `		ULONG dwBuildNumber;` |
|      - | 4806 | `		ULONG dwPlatformId;` |
|      - | 4807 | `		WCHAR szCSDVersion[128];` |
|      - | 4808 | `		USHORT wServicePackMajor;` |
|      - | 4809 | `		USHORT wServicePackMinor;` |
|      - | 4810 | `		USHORT wSuiteMask;` |
|      - | 4811 | `		UCHAR wProductType;` |
|      - | 4812 | `		UCHAR wReserved;` |
|      - | 4813 | `	} vfs_rtl_osversion;` |
|      - | 4814 | `	typedef LONG (WINAPI *rtl_get_version)(vfs_rtl_osversion *);` |
|      - | 4815 | `	vfs_rtl_osversion sInfo;` |
|      - | 4816 | `	rtl_get_version xGet;` |
|      - | 4817 | `	HMODULE hNtdll;` |
|      1 | 4818 | `	*pnMajor = 0;` |
|      1 | 4819 | `	*pnMinor = 0;` |
|      1 | 4820 | `	*pnBuild = 0;` |
|      1 | 4821 | `	*pbWorkstation = 1;` |
|      1 | 4822 | `	SyZero(&sInfo,(sxu32)sizeof(sInfo));` |
|      1 | 4823 | `	sInfo.dwOSVersionInfoSize = (ULONG)sizeof(sInfo);` |
|      1 | 4824 | `	hNtdll = GetModuleHandleA("ntdll.dll");` |
|      1 | 4825 | `	if( hNtdll == 0 ){` |
|    ! 0 | 4826 | `		return;` |
|      - | 4827 | `	}` |
|      1 | 4828 | `	xGet = (rtl_get_version)GetProcAddress(hNtdll,"RtlGetVersion");` |
|      1 | 4829 | `	if( xGet == 0 \|\| xGet(&sInfo) != 0 ){` |
|    ! 0 | 4830 | `		return;` |
|      - | 4831 | `	}` |
|      1 | 4832 | `	*pnMajor = (unsigned long)sInfo.dwMajorVersion;` |
|      1 | 4833 | `	*pnMinor = (unsigned long)sInfo.dwMinorVersion;` |
|      1 | 4834 | `	*pnBuild = (unsigned long)sInfo.dwBuildNumber;` |
|      - | 4835 | `	/* VER_NT_WORKSTATION is 1; spelled out so the struct above needs no` |
|      - | 4836 | `	 * header of its own. */` |
|      1 | 4837 | `	*pbWorkstation = (sInfo.wProductType == 1);` |
|      1 | 4838 | `}` |
|      - | 4839 | `/* php's machine field: the NATIVE architecture, named the way php names it. */` |
|      - | 4840 | `static const char * VfsWinMachine(char *zBuf,int nBuf)` |
|      1 | 4841 | `{` |
|      - | 4842 | `	SYSTEM_INFO sInfo;` |
|      1 | 4843 | `	SyZero(&sInfo,(sxu32)sizeof(sInfo));` |
|      1 | 4844 | `	GetNativeSystemInfo(&sInfo);` |
|      1 | 4845 | `	switch( sInfo.wProcessorArchitecture ){` |
|      1 | 4846 | `		case PROCESSOR_ARCHITECTURE_AMD64: return "AMD64";` |
|    ! 0 | 4847 | `		case PROCESSOR_ARCHITECTURE_ARM:   return "ARM";` |
|      - | 4848 | `#ifdef PROCESSOR_ARCHITECTURE_ARM64` |
|    ! 0 | 4849 | `		case PROCESSOR_ARCHITECTURE_ARM64: return "ARM64";` |
|      - | 4850 | `#endif` |
|    ! 0 | 4851 | `		case PROCESSOR_ARCHITECTURE_IA64:  return "IA64";` |
|      - | 4852 | `		case PROCESSOR_ARCHITECTURE_INTEL:` |
|    ! 0 | 4853 | `			SyBufferFormat(zBuf,(sxu32)nBuf,"i%u",(unsigned int)sInfo.dwProcessorType);` |
|    ! 0 | 4854 | `			return zBuf;` |
|      - | 4855 | `		default: break;` |
|      - | 4856 | `	}` |
|    ! 0 | 4857 | `	return "Unknown";` |
|      1 | 4858 | `}` |
|      - | 4859 | `#endif /* __WINNT__ */` |
|      - | 4860 | `/*` |
|      - | 4861 | ` * string php_uname([ string $mode = "a" ])` |
|      - | 4862 | ` *  Returns information about the host operating system.` |
|      - | 4863 | ` * Parameters` |
|      - | 4864 | ` *  $mode` |
|      - | 4865 | `` *   ONE character out of `a m n r s v`; php refuses every other spelling with`` |
|      - | 4866 | ` *   a ValueError, and refuses a longer or empty string with a different one.` |
|      - | 4867 | ` *    'a': the default -- all five fields in the sequence "s n r v m".` |
|      - | 4868 | ` *    's': operating system name.` |
|      - | 4869 | ` *    'n': host name.` |
|      - | 4870 | ` *    'r': release name.` |
|      - | 4871 | ` *    'v': version information.` |
|      - | 4872 | ` *    'm': machine type.` |
|      - | 4873 | ` * Return` |
|      - | 4874 | ` *  The requested field, or all five.` |
|      - | 4875 | ` */` |
|     42 | 4876 | `static int PH7_vfs_ph7_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4877 | `{` |
|      - | 4878 | `	vfs_uname_fields sF;` |
|      - | 4879 | `	const char *zMode;` |
|     43 | 4880 | `	int nMode = 1,c;` |
|      - | 4881 | `#if defined(__WINNT__)` |
|      - | 4882 | `	char zHost[256],zRel[32],zVer[128],zMach[32];` |
|      - | 4883 | `	unsigned long nMajor,nMinor,nBuild;` |
|      - | 4884 | `	int bWorkstation;` |
|      - | 4885 | `#elif defined(__UNIXES__)` |
|      - | 4886 | `	struct utsname sName;` |
|      - | 4887 | `#endif` |
|     43 | 4888 | `	zMode = "a";` |
|     43 | 4889 | `	if( nArg > 0 ){` |
|     37 | 4890 | `		zMode = ph7_value_to_string(apArg[0],&nMode);` |
|     37 | 4891 | `		if( nMode != 1 ){` |
|      9 | 4892 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4893 | `				"php_uname(): Argument #1 ($mode) must be a single character");` |
|      - | 4894 | `		}` |
|     14 | 4895 | `	}` |
|     35 | 4896 | `	c = zMode[0];` |
|     35 | 4897 | `	if( c != 'a' && c != 'm' && c != 'n' && c != 'r' && c != 's' && c != 'v' ){` |
|      7 | 4898 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4899 | `			"php_uname(): Argument #1 ($mode) must be one of \"a\", \"m\", \"n\", \"r\", \"s\", or \"v\"");` |
|      - | 4900 | `	}` |
|      - | 4901 | `#if defined(__WINNT__)` |
|      1 | 4902 | `	VfsWinVersion(&nMajor,&nMinor,&nBuild,&bWorkstation);` |
|      1 | 4903 | `	zHost[0] = 0;` |
|      - | 4904 | `	{` |
|      1 | 4905 | `		DWORD nName = (DWORD)sizeof(zHost);` |
|      1 | 4906 | `		if( !GetComputerNameA(zHost,&nName) ){` |
|    ! 0 | 4907 | `			zHost[0] = 0;` |
|      - | 4908 | `		}` |
|      - | 4909 | `	}` |
|      1 | 4910 | `	SyBufferFormat(zRel,(sxu32)sizeof(zRel),"%u.%u",` |
|      - | 4911 | `		(unsigned int)nMajor,(unsigned int)nMinor);` |
|      1 | 4912 | `	SyBufferFormat(zVer,(sxu32)sizeof(zVer),"build %u (%s)",` |
|      - | 4913 | `		(unsigned int)nBuild,VfsWinProductName(nMajor,nMinor,nBuild,bWorkstation));` |
|      - | 4914 | `	/* php's own answer on Windows is the KERNEL's name and never the product's,` |
|      - | 4915 | ``	 * which is what makes `php_uname('s')` the same three words on every`` |
|      - | 4916 | `	 * Windows there is. */` |
|      1 | 4917 | `	sF.zSys = "Windows NT";` |
|      1 | 4918 | `	sF.zNode = zHost;` |
|      1 | 4919 | `	sF.zRel = zRel;` |
|      1 | 4920 | `	sF.zVer = zVer;` |
|      1 | 4921 | `	sF.zMachine = VfsWinMachine(zMach,(int)sizeof(zMach));` |
|      - | 4922 | `#elif defined(__UNIXES__)` |
|     28 | 4923 | `	if( uname(&sName) != 0 ){` |
|    ! 0 | 4924 | `		ph7_result_string(pCtx,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 | 4925 | `		return PH7_OK;` |
|      - | 4926 | `	}` |
|     28 | 4927 | `	sF.zSys = sName.sysname;` |
|     28 | 4928 | `	sF.zNode = sName.nodename;` |
|     28 | 4929 | `	sF.zRel = sName.release;` |
|     28 | 4930 | `	sF.zVer = sName.version;` |
|     28 | 4931 | `	sF.zMachine = sName.machine;` |
|      - | 4932 | `#else` |
|      - | 4933 | `	sF.zSys = "Unknown";` |
|      - | 4934 | `	sF.zNode = "";` |
|      - | 4935 | `	sF.zRel = "";` |
|      - | 4936 | `	sF.zVer = "";` |
|      - | 4937 | `	sF.zMachine = "";` |
|      - | 4938 | `#endif` |
|     29 | 4939 | `	switch( c ){` |
|      3 | 4940 | `		case 's': ph7_result_string(pCtx,sF.zSys,-1); break;` |
|      9 | 4941 | `		case 'n': ph7_result_string(pCtx,sF.zNode,-1); break;` |
|      3 | 4942 | `		case 'r': ph7_result_string(pCtx,sF.zRel,-1); break;` |
|      3 | 4943 | `		case 'v': ph7_result_string(pCtx,sF.zVer,-1); break;` |
|      3 | 4944 | `		case 'm': ph7_result_string(pCtx,sF.zMachine,-1); break;` |
|      6 | 4945 | `		default:` |
|     19 | 4946 | `			ph7_result_string_format(pCtx,"%s %s %s %s %s",` |
|      6 | 4947 | `				sF.zSys,sF.zNode,sF.zRel,sF.zVer,sF.zMachine);` |
|     12 | 4948 | `			break;` |
|      - | 4949 | `	}` |
|     29 | 4950 | `	return PH7_OK;` |
|     22 | 4951 | `}` |
|      - | 4952 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 4953 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 4954 | `#if defined(_MSC_VER)` |
|      - | 4955 | `static const ph7_vfs null_vfs = {` |
|      - | 4956 | `#else` |
|      - | 4957 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 4958 | `#endif` |
|      - | 4959 | `	"null_vfs",` |
|      - | 4960 | `	PH7_VFS_VERSION,` |
|      - | 4961 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 4962 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 4963 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 4964 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 4965 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 4966 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 4967 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 4968 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 4969 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 4970 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 4971 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 4972 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 4973 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 4974 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 4975 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 4976 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 4977 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 4978 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 4979 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 4980 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 4981 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 4982 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 4983 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 4984 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 4985 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 4986 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 4987 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 4988 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 4989 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 4990 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 4991 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 4992 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 4993 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 4994 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 4995 | `	0, /* int (*xUmask)(int) */` |
|      - | 4996 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 4997 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 4998 | `	0, /* int (*xUid)(void) */` |
|      - | 4999 | `	0, /* int (*xGid)(void) */` |
|      - | 5000 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 5001 | `	0, /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 5002 | `	0, /* int (*xReadlink)(const char *,ph7_context *) */` |
|      - | 5003 | `	0  /* int (*xEnviron)(ph7_context *) */` |
|      - | 5004 | `};` |
|      - | 5005 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 5006 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 5007 | `/*` |
|      - | 5008 | ` * Export the builtin vfs.` |
|      - | 5009 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 5010 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 5011 | ` * Note:` |
|      - | 5012 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 5013 | ` * Note:` |
|      - | 5014 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 5015 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 5016 | ` */` |
|   7935 | 5017 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 5018 | `{` |
|      - | 5019 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 5020 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 5021 | `	return &null_vfs;` |
|      - | 5022 | `#else` |
|      - | 5023 | `#ifdef __WINNT__` |
|      5 | 5024 | `	return &sWinVfs;` |
|      - | 5025 | `#elif defined(__UNIXES__)` |
|   7935 | 5026 | `	return &sUnixVfs;` |
|      - | 5027 | `#else` |
|      - | 5028 | `	return &null_vfs;` |
|      - | 5029 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 5030 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 5031 | `#else` |
|      - | 5032 | `	return &null_vfs;` |
|      - | 5033 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 5034 | `}` |
|      - | 5035 | `/*` |
|      - | 5036 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 5037 | ` * [i.e: file://,php://].` |
|      - | 5038 | ` * Note:` |
|      - | 5039 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 5040 | ` *  defined then this function is a no-op.` |
|      - | 5041 | ` */` |
|   7925 | 5042 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 5043 | `{` |
|      - | 5044 | `	/*` |
|      - | 5045 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 5046 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 5047 | `	 */` |
|      - | 5048 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 5049 | `	/* VFS: disk I/O related functions */` |
|      - | 5050 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 5051 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 5052 | `#ifndef __WINNT__` |
|      - | 5053 | `		/* php declares chroot() on POSIX only — there is no such call on Windows,` |
|      - | 5054 | ``		 * so `function_exists('chroot')` is FALSE there and the name is free for a`` |
|      - | 5055 | `		 * script to define. PHL used to declare it on both and answer a` |
|      - | 5056 | `		 * "not implemented in the underlying VFS" warning + false on Windows,` |
|      - | 5057 | `		 * which is a different thing from php's undefined function. (chown/chgrp/` |
|      - | 5058 | `		 * link/symlink/readlink stay: php declares all five on Windows.) */` |
|      - | 5059 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 5060 | `#endif` |
|      - | 5061 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 5062 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 5063 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 5064 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 5065 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 5066 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 5067 | `		/* php's own resolver, and the question include/require answer silently:` |
|      - | 5068 | `		 * it walks the same include_path in the same order, so it belongs beside` |
|      - | 5069 | `		 * realpath() rather than with the stream builtins. */` |
|      - | 5070 | `		{"stream_resolve_include_path",PH7_vfs_stream_resolve_include_path},` |
|      - | 5071 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 5072 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 5073 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 5074 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 5075 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 5076 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 5077 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 5078 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 5079 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 5080 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 5081 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 5082 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 5083 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 5084 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 5085 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 5086 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 5087 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 5088 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 5089 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 5090 | `		/* php's own alias, spelled the other way; the diagnostics every` |
|      - | 5091 | `		 * builtin raises name the INVOKED name, so the one routine serves` |
|      - | 5092 | `		 * both. */` |
|      - | 5093 | `		{"is_writeable",PH7_vfs_is_writable   },` |
|      - | 5094 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 5095 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 5096 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 5097 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 5098 | `		{"fileowner",   PH7_vfs_file_owner},` |
|      - | 5099 | `		{"filegroup",   PH7_vfs_file_group},` |
|      - | 5100 | `		{"fileinode",   PH7_vfs_file_inode},` |
|      - | 5101 | `		{"fileperms",   PH7_vfs_file_perms},` |
|      - | 5102 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 5103 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 5104 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 5105 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 5106 | `		{"link",        PH7_vfs_link     },` |
|      - | 5107 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 5108 | `		{"readlink",    PH7_vfs_readlink },` |
|      - | 5109 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 5110 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 5111 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 5112 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 5113 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 5114 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 5115 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 5116 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 5117 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 5118 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 5119 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 5120 | `	};` |
|      - | 5121 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 5122 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 5123 | `	 */` |
|      - | 5124 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 5125 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 5126 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 5127 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 5128 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 5129 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 5130 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 5131 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 5132 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 5133 | `		{"fscanf",    PH7_builtin_fscanf },` |
|      - | 5134 | `		{"stream_get_line", PH7_builtin_stream_get_line },` |
|      - | 5135 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 5136 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 5137 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 5138 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 5139 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 5140 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 5141 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 5142 | `		/* php's dir() lives with opendir(), which is what it calls and what its` |
|      - | 5143 | `		 * failure warning is worded by. Registering it here also means the TINY` |
|      - | 5144 | `		 * build drops BOTH: the prelude copy was defined there and fataled on` |
|      - | 5145 | `		 * "Call to undefined function opendir()" the moment it was called. */` |
|      - | 5146 | `		{"dir",       PH7_builtin_dir },` |
|      - | 5147 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 5148 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 5149 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 5150 | `		{"file",      PH7_builtin_file   },` |
|      - | 5151 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 5152 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 5153 | `		{"stream_isatty", PH7_builtin_stream_isatty },` |
|      - | 5154 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 5155 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 5156 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 5157 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 5158 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 5159 | `		{"stream_get_contents",  PH7_builtin_stream_get_contents },` |
|      - | 5160 | `		{"stream_get_wrappers",  PH7_builtin_stream_get_wrappers },` |
|      - | 5161 | `		{"stream_get_meta_data", PH7_builtin_stream_get_meta_data },` |
|      - | 5162 | `		/* php's own alias, kept from the days sockets had a separate API. */` |
|      - | 5163 | `		{"socket_get_status",    PH7_builtin_stream_get_meta_data },` |
|      - | 5164 | `		{"stream_set_blocking",  PH7_builtin_stream_set_blocking },` |
|      - | 5165 | `		{"socket_set_blocking",  PH7_builtin_stream_set_blocking },` |
|      - | 5166 | `		{"stream_set_timeout",   PH7_builtin_stream_set_timeout },` |
|      - | 5167 | `		{"stream_set_chunk_size",PH7_builtin_stream_set_chunk_size },` |
|      - | 5168 | `		{"stream_set_read_buffer",  PH7_builtin_stream_set_read_buffer },` |
|      - | 5169 | `		{"stream_set_write_buffer", PH7_builtin_stream_set_write_buffer },` |
|      - | 5170 | `		{"set_file_buffer",         PH7_builtin_stream_set_write_buffer },` |
|      - | 5171 | `		{"stream_supports_lock", PH7_builtin_stream_supports_lock },` |
|      - | 5172 | `		{"stream_is_local",      PH7_builtin_stream_is_local },` |
|      - | 5173 | `		{"stream_copy_to_stream",PH7_builtin_stream_copy_to_stream },` |
|      - | 5174 | `		{"stream_get_transports",PH7_builtin_stream_get_transports },` |
|      - | 5175 | `		/* Not under PH7_ENABLE_NET: a script selects over FILES and pipes in a` |
|      - | 5176 | `		 * build with no networking at all. */` |
|      - | 5177 | `		{"stream_select",        PH7_builtin_stream_select },` |
|      - | 5178 | `		{"stream_context_create",PH7_builtin_stream_context_create },` |
|      - | 5179 | `		{"stream_context_get_options",PH7_builtin_stream_context_get_options },` |
|      - | 5180 | `		{"stream_context_set_option", PH7_builtin_stream_context_set_option },` |
|      - | 5181 | `		{"stream_context_set_options",PH7_builtin_stream_context_set_options },` |
|      - | 5182 | `		{"stream_context_get_params", PH7_builtin_stream_context_get_params },` |
|      - | 5183 | `		{"stream_context_set_params", PH7_builtin_stream_context_set_params },` |
|      - | 5184 | `		{"stream_context_get_default",PH7_builtin_stream_context_get_default },` |
|      - | 5185 | `		{"stream_context_set_default",PH7_builtin_stream_context_set_default },` |
|      - | 5186 | `		{"stream_wrapper_register",   PH7_builtin_stream_wrapper_register },` |
|      - | 5187 | `		{"stream_register_wrapper",   PH7_builtin_stream_wrapper_register },` |
|      - | 5188 | `		{"stream_wrapper_unregister", PH7_builtin_stream_wrapper_unregister },` |
|      - | 5189 | `		{"stream_wrapper_restore",    PH7_builtin_stream_wrapper_restore },` |
|      - | 5190 | `		{"stream_filter_append",  PH7_builtin_stream_filter_append },` |
|      - | 5191 | `		{"stream_filter_prepend", PH7_builtin_stream_filter_prepend },` |
|      - | 5192 | `		{"stream_filter_remove",  PH7_builtin_stream_filter_remove },` |
|      - | 5193 | `		{"stream_get_filters",    PH7_builtin_stream_get_filters },` |
|      - | 5194 | `		{"stream_filter_register",PH7_builtin_stream_filter_register },` |
|      - | 5195 | `		{"stream_bucket_make_writeable", PH7_builtin_stream_bucket_make_writeable },` |
|      - | 5196 | `		{"stream_bucket_append",  PH7_builtin_stream_bucket_append },` |
|      - | 5197 | `		{"stream_bucket_prepend", PH7_builtin_stream_bucket_prepend },` |
|      - | 5198 | `		{"stream_bucket_new",     PH7_builtin_stream_bucket_new },` |
|      - | 5199 | `#ifdef PH7_ENABLE_NET` |
|      - | 5200 | `		{"fsockopen",  PH7_builtin_fsockopen },` |
|      - | 5201 | `		{"pfsockopen", PH7_builtin_fsockopen },` |
|      - | 5202 | `		{"stream_socket_client", PH7_builtin_fsockopen },` |
|      - | 5203 | `		{"stream_socket_enable_crypto", PH7_builtin_stream_socket_enable_crypto },` |
|      - | 5204 | `		{"stream_socket_server", PH7_builtin_stream_socket_server },` |
|      - | 5205 | `		{"stream_socket_accept", PH7_builtin_stream_socket_accept },` |
|      - | 5206 | `		{"stream_socket_get_name", PH7_builtin_stream_socket_get_name },` |
|      - | 5207 | `		{"stream_socket_pair",   PH7_builtin_stream_socket_pair },` |
|      - | 5208 | `		{"stream_socket_shutdown", PH7_builtin_stream_socket_shutdown },` |
|      - | 5209 | `		{"stream_socket_recvfrom", PH7_builtin_stream_socket_recvfrom },` |
|      - | 5210 | `		{"stream_socket_sendto",   PH7_builtin_stream_socket_sendto },` |
|      - | 5211 | `		/* The address converters and the host name: php's ext/standard` |
|      - | 5212 | `		 * network trio, which sits with the socket family here because the` |
|      - | 5213 | `		 * last of the three is an OS call the others share a build flag with. */` |
|      - | 5214 | `		{"inet_pton",  PH7_builtin_inet_pton },` |
|      - | 5215 | `		{"inet_ntop",  PH7_builtin_inet_ntop },` |
|      - | 5216 | `		{"gethostname",PH7_builtin_gethostname },` |
|      - | 5217 | `#endif` |
|      - | 5218 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 5219 | `		{"proc_open",      PH7_builtin_proc_open      },` |
|      - | 5220 | `		{"proc_close",     PH7_builtin_proc_close     },` |
|      - | 5221 | `		{"proc_terminate", PH7_builtin_proc_terminate },` |
|      - | 5222 | `		{"proc_get_status",PH7_builtin_proc_get_status},` |
|      - | 5223 | `		{"proc_nice",      PH7_builtin_proc_nice      },` |
|      - | 5224 | `		{"shell_exec", PH7_builtin_shell_exec },` |
|      - | 5225 | `		{"exec",       PH7_builtin_exec     },` |
|      - | 5226 | `		{"system",     PH7_builtin_system   },` |
|      - | 5227 | `		{"passthru",   PH7_builtin_passthru },` |
|      - | 5228 | `		/* The shell-escaping pair lives with the command runners it exists to` |
|      - | 5229 | `		 * feed: a build without process execution has nothing to escape for. */` |
|      - | 5230 | `		{"escapeshellarg", PH7_builtin_escapeshellarg },` |
|      - | 5231 | `		{"escapeshellcmd", PH7_builtin_escapeshellcmd },` |
|      - | 5232 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 5233 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 5234 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 5235 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 5236 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5237 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 5238 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 5239 | `		/* The hash extension's file readers live with the disk table for the` |
|      - | 5240 | `		 * same reason md5_file does: without disk IO there is nothing to read. */` |
|      - | 5241 | `		{"hash_file",          PH7_builtin_hash_file },` |
|      - | 5242 | `		{"hash_hmac_file",     PH7_builtin_hash_hmac_file },` |
|      - | 5243 | `		{"hash_update_file",   PH7_builtin_hash_update_file },` |
|      - | 5244 | `		{"hash_update_stream", PH7_builtin_hash_update_stream },` |
|      - | 5245 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5246 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 5247 | `		{"vfprintf",  PH7_builtin_vfprintf},` |
|      - | 5248 | `#ifdef PH7_ENABLE_ZLIB` |
|      - | 5249 | `		/* ext/zlib's handle verbs, beside the stream functions they ARE: php` |
|      - | 5250 | `		 * registers each as an alias of the one above it, which is why` |
|      - | 5251 | `		 * gzread() works on a plain fopen() handle and fread() works on a` |
|      - | 5252 | `		 * gzopen() one. Their own signature rows word their diagnostics.` |
|      - | 5253 | `		 * The rest of the extension registers itself (PH7_ZlibFuncTable). */` |
|      - | 5254 | `		{"gzread",     PH7_builtin_fread  },` |
|      - | 5255 | `		{"gzwrite",    PH7_builtin_fwrite },` |
|      - | 5256 | `		{"gzputs",     PH7_builtin_fwrite },` |
|      - | 5257 | `		{"gzgets",     PH7_builtin_fgets  },` |
|      - | 5258 | `		{"gzgetc",     PH7_builtin_fgetc  },` |
|      - | 5259 | `		{"gzeof",      PH7_builtin_feof   },` |
|      - | 5260 | `		{"gzclose",    PH7_builtin_fclose },` |
|      - | 5261 | `		{"gzseek",     PH7_builtin_fseek  },` |
|      - | 5262 | `		{"gztell",     PH7_builtin_ftell  },` |
|      - | 5263 | `		{"gzrewind",   PH7_builtin_rewind },` |
|      - | 5264 | `		{"gzpassthru", PH7_builtin_fpassthru }` |
|      - | 5265 | `#endif /* PH7_ENABLE_ZLIB */` |
|      - | 5266 | `	};` |
|   7930 | 5267 | `	const ph7_io_stream *pFileStream = 0;` |
|   7930 | 5268 | `	sxu32 n = 0;` |
|      - | 5269 | `	/* Register disk-related functions */` |
| 443805 | 5270 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 435880 | 5271 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
| 217640 | 5272 | `	}` |
| 927230 | 5273 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 919305 | 5274 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
| 459017 | 5275 | `	}` |
|      - | 5276 | `#else` |
|      - | 5277 | `	SXUNUSED(pVm);` |
|      - | 5278 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 5279 |  |
|      - | 5280 | `	/*` |
|      - | 5281 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 5282 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 5283 | `	 */` |
|      - | 5284 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 5285 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 5286 | `		/* Path processing */` |
|      - | 5287 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 5288 | `		{"basename",    PH7_builtin_basename },` |
|      - | 5289 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 5290 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 5291 | `		{"fnmatch",     PH7_builtin_fnmatch  }` |
|      - | 5292 | `	};` |
|  47555 | 5293 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  39630 | 5294 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|  19790 | 5295 | `	}` |
|      - | 5296 | `	/* The three names a script asks an http:// exchange about. */` |
|   7930 | 5297 | `	PH7_HttpInstallFuncs(&(*pVm));` |
|      - | 5298 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 5299 |  |
|      - | 5300 | `	/* Install streams if disk I/O is enabled */` |
|      - | 5301 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 5302 | `#ifdef __WINNT__` |
|      5 | 5303 | `	pFileStream = &sWinFileStream;` |
|      - | 5304 | `#elif defined(__UNIXES__)` |
|   7925 | 5305 | `	pFileStream = &sUnixFileStream;` |
|      - | 5306 | `#endif` |
|      - | 5307 | `	/* Install the php:// stream */` |
|   7930 | 5308 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|      - | 5309 | `#ifdef PH7_ENABLE_ZLIB` |
|      - | 5310 | `	/* compress.zlib:// -- the same device gzopen() opens directly. */` |
|   7930 | 5311 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sZLIB_Stream);` |
|      - | 5312 | `#endif` |
|      - | 5313 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 5314 | `	/* phar:// -- what an archive's own entries are read through. */` |
|   7930 | 5315 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHAR_Stream);` |
|      - | 5316 | `#endif` |
|   7930 | 5317 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);` |
|      - | 5318 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 5319 | `	/* glob:// lives beside the pattern matcher it drives, so it is only in the` |
|      - | 5320 | `	 * build when that is. */` |
|   7930 | 5321 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sGLOB_Stream);` |
|      - | 5322 | `#endif` |
|      - | 5323 | `#ifdef PH7_ENABLE_NET` |
|   7930 | 5324 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);` |
|      - | 5325 | `	/* php's one built-in protocol wrapper. It speaks over the same sockets` |
|      - | 5326 | `	 * tcp:// hands out, so it is in the build exactly when they are. */` |
|   7930 | 5327 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sHTTP_Stream);` |
|      - | 5328 | `#ifdef PH7_ENABLE_OPENSSL` |
|      - | 5329 | `	/* ... and the same wrapper over TLS, which php registers exactly when its` |
|      - | 5330 | `	 * ssl:// transport is in the build. */` |
|   7930 | 5331 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sHTTPS_Stream);` |
|      - | 5332 | `#endif` |
|      - | 5333 | `#endif` |
|   7930 | 5334 | `	if( pFileStream ){` |
|      - | 5335 | `		/* Install the file:// stream */` |
|   7930 | 5336 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   3957 | 5337 | `	}` |
|      - | 5338 | `#if defined(PH7_ENABLE_ZLIB) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|      - | 5339 | `	/* zip:// -- an OPENER and nothing else, exactly as php's is: no url_stat,` |
|      - | 5340 | ``	 * so `file_exists('zip://…')` is false, and no directory door, so an`` |
|      - | 5341 | `	 * archive can only be listed through ZipArchive. It goes on LAST because` |
|      - | 5342 | ``	 * php registers it last and `stream_get_wrappers()` answers in that`` |
|      - | 5343 | `	 * order. */` |
|   7930 | 5344 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sZIP_Stream);` |
|      - | 5345 | `#endif` |
|      - | 5346 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 5347 |  |
|   7930 | 5348 | `	return SXRET_OK;` |
|      5 | 5349 | `}` |
|      - | 5350 |  |
