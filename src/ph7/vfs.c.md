# src/ph7/vfs.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1914/2384 lines (80.29%)

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
|  19544 |   27 | `PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen)` |
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
|  19544 |   39 | `	const char *zRoot = "/";` |
|      - |   40 | `#endif` |
|  19549 |   41 | `	c = d = '/';` |
|      - |   42 | `#ifdef __WINNT__` |
|      5 |   43 | `	d = '\\';` |
|      - |   44 | `#endif` |
|      - |   45 | `#define DIR_IS_SEP(x) ( (int)(x) == c \|\| (int)(x) == d )` |
|  19549 |   46 | `	if( nByte < 1 ){` |
|      - |   47 | `		/* php returns the empty string for the empty path */` |
|      5 |   48 | `		*pLen = 0;` |
|      5 |   49 | `		return "";` |
|      - |   50 | `	}` |
|  19545 |   51 | `	iEnd = nByte;` |
|  29343 |   52 | `	while( iEnd > 0 && DIR_IS_SEP(zPath[iEnd - 1]) ){` |
|     29 |   53 | `		iEnd--;` |
|      1 |   54 | `	}` |
|  19545 |   55 | `	if( iEnd == 0 ){` |
|      - |   56 | `		/* The path is nothing but separators: the root is its own parent */` |
|     17 |   57 | `		*pLen = (int)sizeof(char);` |
|     17 |   58 | `		return zRoot;` |
|      - |   59 | `	}` |
|      - |   60 | `	/* Walk back to the separator that ends the parent directory */` |
|  19529 |   61 | `	i = iEnd;` |
| 535781 |   62 | `	while( i > 0 && !DIR_IS_SEP(zPath[i - 1]) ){` |
| 516257 |   63 | `		i--;` |
|      5 |   64 | `	}` |
|  19529 |   65 | `	if( i == 0 ){` |
|      - |   66 | `		/* No separator at all,return "." as the current directory */` |
|     70 |   67 | `		*pLen = (int)sizeof(char);` |
|     70 |   68 | `		return ".";` |
|      - |   69 | `	}` |
|      - |   70 | `	/* Drop the separator, plus any that repeat before it */` |
|  48635 |   71 | `	while( i > 1 && DIR_IS_SEP(zPath[i - 1]) ){` |
|  19451 |   72 | `		i--;` |
|      5 |   73 | `	}` |
|  19461 |   74 | `	if( i == 1 && DIR_IS_SEP(zPath[0]) ){` |
|     13 |   75 | `		*pLen = (int)sizeof(char);` |
|     13 |   76 | `		return zRoot;` |
|      - |   77 | `	}` |
|  19449 |   78 | `	*pLen = i;` |
|  19449 |   79 | `	return zPath;` |
|      - |   80 | `#undef DIR_IS_SEP` |
|   9777 |   81 | `}` |
|      - |   82 | `/*` |
|      - |   83 | ` * php_basename: drop any trailing separators, then answer what follows the last` |
|      - |   84 | ` * remaining one. Shared by basename() and pathinfo() — they used to hand-roll the` |
|      - |   85 | ` * same walk separately, and pathinfo()'s copy kept the trailing separator run` |
|      - |   86 | `` * (`pathinfo("/var/www/")` answered basename "" where php answers "www").`` |
|      - |   87 | ` */` |
|  19330 |   88 | `PH7_PRIVATE const char * PH7_ExtractBaseName(const char *zPath,int nByte,int *pLen)` |
|      5 |   89 | `{` |
|      - |   90 | `	int c,d,iEnd,i;` |
|  19335 |   91 | `	c = d = '/';` |
|      - |   92 | `#ifdef __WINNT__` |
|      5 |   93 | `	d = '\\';` |
|      - |   94 | `#endif` |
|      - |   95 | `#define DIR_IS_SEP(x) ( (int)(x) == c \|\| (int)(x) == d )` |
|  19335 |   96 | `	iEnd = nByte;` |
|  29025 |   97 | `	while( iEnd > 0 && DIR_IS_SEP(zPath[iEnd - 1]) ){` |
|     27 |   98 | `		iEnd--;` |
|      1 |   99 | `	}` |
|  19335 |  100 | `	if( iEnd < 1 ){` |
|      - |  101 | `		/* Empty, or nothing but separators: php answers the empty string */` |
|     29 |  102 | `		*pLen = 0;` |
|     29 |  103 | `		return "";` |
|      - |  104 | `	}` |
|  19307 |  105 | `	i = iEnd;` |
| 528530 |  106 | `	while( i > 0 && !DIR_IS_SEP(zPath[i - 1]) ){` |
| 509228 |  107 | `		i--;` |
|      5 |  108 | `	}` |
|  19307 |  109 | `	*pLen = iEnd - i;` |
|  19307 |  110 | `	return &zPath[i];` |
|      - |  111 | `#undef DIR_IS_SEP` |
|   9670 |  112 | `}` |
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
|  43005 |  126 | `PH7_PRIVATE int PH7_VfsEmptyPathRefused(ph7_context *pCtx,int nPath)` |
|      5 |  127 | `{` |
|  43010 |  128 | `	if( nPath > 0 ){` |
|  42980 |  129 | `		return 0;` |
|      - |  130 | `	}` |
|     31 |  131 | `	PH7_VmThrowException(pCtx,"ValueError","Path must not be empty");` |
|     31 |  132 | `	return 1;` |
|  21498 |  133 | `}` |
|      - |  134 | `/*` |
|      - |  135 | ` * Is this path already anchored -- a leading slash, or a drive prefix on` |
|      - |  136 | ` * Windows? Everything else resolves against the working directory.` |
|      - |  137 | ` */` |
|    400 |  138 | `PH7_PRIVATE int PH7_VfsPathIsAbsolute(const char *zPath,int nPath)` |
|      5 |  139 | `{` |
|    405 |  140 | `	if( nPath < 1 ){` |
|    ! 0 |  141 | `		return 0;` |
|      - |  142 | `	}` |
|    405 |  143 | `	if( zPath[0] == '/' ){` |
|    383 |  144 | `		return 1;` |
|      - |  145 | `	}` |
|      - |  146 | `#ifdef __WINNT__` |
|      4 |  147 | `	if( zPath[0] == '\\' ){` |
|    ! 0 |  148 | `		return 1;` |
|      - |  149 | `	}` |
|      4 |  150 | `	if( nPath > 2 && zPath[1] == ':' && (zPath[2] == '/' \|\| zPath[2] == '\\') ){` |
|      4 |  151 | `		return 1;` |
|      - |  152 | `	}` |
|      - |  153 | `#endif` |
|     19 |  154 | `	return 0;` |
|    205 |  155 | `}` |
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
|      5 |  172 | `{` |
|      - |  173 | `	SyBlob sRaw;` |
|      - |  174 | `	const char *z;` |
|      - |  175 | `	sxu32 n,nRoot,nRaw;` |
|    405 |  176 | `	SyBlobInit(pOut,&pCtx->pVm->sAllocator);` |
|    405 |  177 | `	SyBlobInit(&sRaw,&pCtx->pVm->sAllocator);` |
|    405 |  178 | `	if( !PH7_VfsPathIsAbsolute(zPath,nPath) ){` |
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
|    405 |  191 | `	if( nPath > 0 ){` |
|    405 |  192 | `		SyBlobAppend(&sRaw,zPath,(sxu32)nPath);` |
|    200 |  193 | `	}` |
|      - |  194 | `	/* Keep the root -- a leading slash, or a drive prefix -- and rebuild the` |
|      - |  195 | `	 * rest segment by segment. */` |
|    405 |  196 | `	z = (const char *)SyBlobData(&sRaw);` |
|    405 |  197 | `	nRaw = SyBlobLength(&sRaw);` |
|    405 |  198 | `	nRoot = 0;` |
|      - |  199 | `#ifdef __WINNT__` |
|      5 |  200 | `	if( nRaw > 1 && z[1] == ':' ){` |
|      4 |  201 | `		nRoot = 2;` |
|      - |  202 | `	}` |
|      - |  203 | `#endif` |
|    405 |  204 | `	if( nRoot < nRaw && (z[nRoot] == '/' \|\| z[nRoot] == '\\') ){` |
|    405 |  205 | `		++nRoot;` |
|    200 |  206 | `	}` |
|    405 |  207 | `	SyBlobAppend(pOut,z,nRoot);` |
|   2420 |  208 | `	for( n = nRoot ; n < nRaw ; ){` |
|   2020 |  209 | `		sxu32 nStart = n;` |
|      - |  210 | `		sxu32 nSeg;` |
|  18858 |  211 | `		while( n < nRaw && z[n] != '/' && z[n] != '\\' ){` |
|  16843 |  212 | `			++n;` |
|      5 |  213 | `		}` |
|   2020 |  214 | `		nSeg = n - nStart;` |
|   2020 |  215 | `		if( n < nRaw ){` |
|   1620 |  216 | `			++n;   /* step past the separator */` |
|   1210 |  217 | `		}` |
|   2020 |  218 | `		if( nSeg == 0 \|\| (nSeg == 1 && z[nStart] == '.') ){` |
|      3 |  219 | ``			continue;   /* `//` and `.` name the directory they stand in */`` |
|      - |  220 | `		}` |
|   2018 |  221 | `		if( nSeg == 2 && z[nStart] == '.' && z[nStart+1] == '.' ){` |
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
|   2014 |  233 | `		if( SyBlobLength(pOut) > nRoot ){` |
|      - |  234 | `			/* php's expansion writes the PLATFORM's separator, which is what a` |
|      - |  235 | ``			 * name it hands back reads as: ext/zip's `filename` property is the`` |
|      - |  236 | `			 * one surface that shows it, and a Windows one there is spelled` |
|      - |  237 | `			 * with backslashes exactly as php spells it. */` |
|   1614 |  238 | `			SyBlobAppend(pOut,PH7_PATH_SEP_STR,sizeof(PH7_PATH_SEP_STR)-1);` |
|   1207 |  239 | `		}` |
|   2014 |  240 | `		SyBlobAppend(pOut,&z[nStart],nSeg);` |
|      5 |  241 | `	}` |
|    405 |  242 | `	SyBlobRelease(&sRaw);` |
|    405 |  243 | `	SyBlobNullAppend(pOut);` |
|    405 |  244 | `}` |
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
|  30358 |  256 | `PH7_PRIVATE const char * VfsStrerror(int iErr)` |
|      5 |  257 | `{` |
|      - |  258 | `#if defined(_MSC_VER)` |
|      - |  259 | `#pragma warning(push)` |
|      - |  260 | `#pragma warning(disable:4996)` |
|      - |  261 | `#endif` |
|  30363 |  262 | `	return strerror(iErr);` |
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
|  30118 |  273 | `static void VfsThrowSysWarning(ph7_context *pCtx,const char *zPath)` |
|      5 |  274 | `{` |
|  45180 |  275 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s",` |
|  30118 |  276 | `		ph7_function_name(pCtx),zPath ? zPath : "",VfsStrerror(errno));` |
|  30123 |  277 | `}` |
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
|     90 |  302 | `PH7_PRIVATE const char * PH7_VfsOpenStrerror(int iErr)` |
|      5 |  303 | `{` |
|      - |  304 | `#ifdef ENOTDIR` |
|     95 |  305 | `	if( iErr == ENOTDIR ){` |
|    ! 0 |  306 | `		iErr = ENOENT;` |
|    ! 0 |  307 | `	}` |
|      - |  308 | `#endif` |
|     95 |  309 | `	return VfsStrerror(iErr);` |
|      5 |  310 | `}` |
|    268 |  311 | `PH7_PRIVATE void VfsThrowOpenWarning(ph7_context *pCtx,const char *zFile)` |
|      5 |  312 | `{` |
|    273 |  313 | `	ph7_vm *pVm = pCtx->pVm;` |
|    273 |  314 | `	const char *zName = zFile ? zFile : "";` |
|    273 |  315 | `	int nName = -1;` |
|    273 |  316 | `	if( zFile != 0 && zFile == pVm->zOpenUriTail && pVm->zOpenUri != 0 ){` |
|    184 |  317 | `		zName = pVm->zOpenUri;` |
|    184 |  318 | `		nName = pVm->nOpenUri;` |
|     89 |  319 | `	}` |
|    450 |  320 | `	PH7_VmThrowWarningFmt(pVm,"%s(%.*s): Failed to open stream: %s",` |
|    177 |  321 | `		ph7_function_name(pCtx),nName < 0 ? (int)SyStrlen(zName) : nName,zName,` |
|    268 |  322 | `		pVm->zOpenErr ? pVm->zOpenErr : PH7_VfsOpenStrerror(errno));` |
|    273 |  323 | `}` |
|      - |  324 | `/*` |
|      - |  325 | ` * php's answer when NO wrapper will take a name is a reason of its own, raised` |
|      - |  326 | ` * before the operation's own failure and naming the scheme the script wrote:` |
|      - |  327 | ` *` |
|      - |  328 | ` *   file_get_contents(): Unable to find the wrapper "zzz" - did you forget to` |
|      - |  329 | ` *   enable it when you configured PHP?` |
|      - |  330 | ` *` |
|      - |  331 | ` * PHL raised one PH7-specific sentence -- "No such stream device,PH7 is` |
|      - |  332 | ` * returning FALSE" -- which names neither the function's argument nor what was` |
|      - |  333 | ` * wrong with it, and two more call sites had a third wording of their own.` |
|      - |  334 | ` */` |
|     28 |  335 | `PH7_PRIVATE void VfsThrowNoDeviceWarning(ph7_context *pCtx,const char *zUri,int bDir)` |
|      2 |  336 | `{` |
|     30 |  337 | `	const char *zFunc = ph7_function_name(pCtx);` |
|     30 |  338 | `	const char *zWhat = bDir ? "directory" : "stream";` |
|     30 |  339 | `	int nScheme = 0;` |
|     30 |  340 | `	if( zUri == 0 ){` |
|    ! 0 |  341 | `		zUri = "";` |
|    ! 0 |  342 | `	}` |
|     30 |  343 | `	if( PH7_VmStreamDeviceIsRemoteHost(zUri,-1,&nScheme) ){` |
|      - |  344 | `		/* A wrapper WAS found for the scheme and refused the name, which php` |
|      - |  345 | `		 * words differently from a scheme nothing is registered under. */` |
|     19 |  346 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Remote host file access not supported, %s",` |
|      6 |  347 | `			zFunc,zUri);` |
|     19 |  348 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      6 |  349 | `			"%s(%s): Failed to open %s: no suitable wrapper could be found",zFunc,zUri,zWhat);` |
|     14 |  350 | `		return;` |
|      - |  351 | `	}` |
|     18 |  352 | `	if( nScheme > 0 ){` |
|     23 |  353 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - |  354 | `			"%s(): Unable to find the wrapper \"%.*s\" - did you forget to enable it when you configured PHP?",` |
|      7 |  355 | `			zFunc,nScheme,zUri);` |
|      7 |  356 | `	}` |
|      - |  357 | `	/* A name with no scheme is the plain-files wrapper's, and that is the ONE` |
|      - |  358 | `	 * php reports as switched off rather than missing: its fallback branch runs` |
|      - |  359 | ``	 * after the hash lookup, so an explicit `file://` gets both sentences and a`` |
|      - |  360 | `	 * bare path only the second. Every other unregistered wrapper is` |
|      - |  361 | `	 * indistinguishable from one that never existed, and php words it that way. */` |
|     16 |  362 | `	if( (nScheme == 0` |
|     15 |  363 | `	  \|\| (nScheme == (int)sizeof("file")-1 && SyStrnicmp(zUri,"file",sizeof("file")-1) == 0))` |
|      5 |  364 | `	 && PH7_VmStreamSchemeDisabled(pCtx->pVm,"file",(int)sizeof("file")-1) ){` |
|      4 |  365 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      1 |  366 | `			"%s(): file:// wrapper is disabled in the server configuration",zFunc);` |
|      4 |  367 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      1 |  368 | `			"%s(%s): Failed to open %s: no suitable wrapper could be found",zFunc,zUri,zWhat);` |
|      3 |  369 | `		return;` |
|      - |  370 | `	}` |
|     23 |  371 | `	PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      7 |  372 | `		"%s(%s): Failed to open %s: No such file or directory",zFunc,zUri,zWhat);` |
|     16 |  373 | `}` |
|      - |  374 | `/*` |
|      - |  375 | ` * The FIRST half of the sentence above, on its own: php's` |
|      - |  376 | `` * `Unable to find the wrapper "zzz" - did you forget to enable it when you`` |
|      - |  377 | `` * configured PHP?`. A caller that resolves a wrapper WITHOUT opening anything`` |
|      - |  378 | ` * (get_headers()) reports only this one -- the failed-open line under it comes` |
|      - |  379 | ` * from the open, and there is none.` |
|      - |  380 | ` */` |
|      8 |  381 | `PH7_PRIVATE void VfsThrowUnknownWrapperWarning(ph7_context *pCtx,const char *zUri)` |
|      3 |  382 | `{` |
|     11 |  383 | `	int nScheme = 0;` |
|     11 |  384 | `	if( zUri == 0 ){` |
|    ! 0 |  385 | `		zUri = "";` |
|    ! 0 |  386 | `	}` |
|     11 |  387 | `	PH7_VmStreamDeviceIsRemoteHost(zUri,-1,&nScheme);` |
|     11 |  388 | `	if( nScheme > 0 ){` |
|     15 |  389 | `		PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - |  390 | `			"%s(): Unable to find the wrapper \"%.*s\" - did you forget to enable it when you configured PHP?",` |
|      4 |  391 | `			ph7_function_name(pCtx),nScheme,zUri);` |
|      4 |  392 | `	}` |
|     11 |  393 | `}` |
|      - |  394 | `/*` |
|      - |  395 | `` * php's stat-failure warning: `filemtime(): stat failed for /nope`, and`` |
|      - |  396 | `` * `filetype(): Lstat failed for /nope` for the two members that LSTAT. php raises`` |
|      - |  397 | ` * it from php_stat() for the whole family and answers FALSE; PHL answered the` |
|      - |  398 | ` * VFS's raw -1 (or the string "unknown") for most of them, in silence -- and -1 is` |
|      - |  399 | `` * TRUTHY, so `if (filemtime($f))` took the found branch for a file that is not`` |
|      - |  400 | `` * there and `filemtime($a) > filemtime($b)` compared a real time against it.`` |
|      - |  401 | ` */` |
|     62 |  402 | `static void VfsThrowStatWarning(ph7_context *pCtx,const char *zPath,int bLstat)` |
|      3 |  403 | `{` |
|     65 |  404 | `	if( zPath == 0 \|\| zPath[0] == 0 ){` |
|      - |  405 | `		/* php's stat family says NOTHING about an empty path -- it answers the` |
|      - |  406 | `		 * same FALSE a missing one gets and skips the warning, which is the one` |
|      - |  407 | `		 * place in the family where the empty path is not the opener's` |
|      - |  408 | `		 * ValueError but a silence of its own. */` |
|     12 |  409 | `		return;` |
|      - |  410 | `	}` |
|     78 |  411 | `	PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s failed for %s",` |
|     25 |  412 | `		ph7_function_name(pCtx),bLstat ? "Lstat" : "stat",zPath);` |
|     34 |  413 | `}` |
|      - |  414 | `/*` |
|      - |  415 | ` * php's stat() answer is TWENTY-SIX entries, not thirteen: the same thirteen` |
|      - |  416 | ` * fields once at numeric indices 0..12 and once under their names, in this` |
|      - |  417 | ` * order. The numeric half is what php's own documentation indexes by ($s[7] is` |
|      - |  418 | `` * the size) and it is what a `list()`/destructuring reader takes, so a script`` |
|      - |  419 | `` * written against php read `Undefined array key 7` here and answered NULL.`` |
|      - |  420 | ` *` |
|      - |  421 | ` * The VFS fills the NAMED half (both the unix and Windows implementations use` |
|      - |  422 | ` * exactly these keys), so the doubling is done once, here, rather than in every` |
|      - |  423 | ` * xStat: pOut gets the numeric run first and then the names, which is php's own` |
|      - |  424 | ` * insertion order — visible through foreach, print_r, var_dump and json_encode.` |
|      - |  425 | ` */` |
|     72 |  426 | `PH7_PRIVATE int PH7_VfsStatDoubleUp(ph7_value *pIn,ph7_value *pOut)` |
|      2 |  427 | `{` |
|      - |  428 | `	static const char * const azField[] = {` |
|      - |  429 | `		"dev","ino","mode","nlink","uid","gid","rdev","size",` |
|      - |  430 | `		"atime","mtime","ctime","blksize","blocks"` |
|      - |  431 | `	};` |
|      - |  432 | `	sxu32 i;` |
|     74 |  433 | `	if( pIn == 0 \|\| pOut == 0 ){` |
|    ! 0 |  434 | `		return -1;` |
|      - |  435 | `	}` |
|    958 |  436 | `	for( i = 0 ; i < SX_ARRAYSIZE(azField) ; ++i ){` |
|    890 |  437 | `		ph7_value *pField = ph7_array_fetch(pIn,azField[i],-1);` |
|    890 |  438 | `		if( pField == 0 ){` |
|      - |  439 | `			/* A VFS that does not report this field: php always has all thirteen,` |
|      - |  440 | `			 * so the doubling would silently shift every later index. Hand the` |
|      - |  441 | `			 * caller the named-only array it already had instead. */` |
|      5 |  442 | `			return -1;` |
|      - |  443 | `		}` |
|    886 |  444 | `		ph7_array_add_elem(pOut,0,pField);` |
|    444 |  445 | `	}` |
|    954 |  446 | `	for( i = 0 ; i < SX_ARRAYSIZE(azField) ; ++i ){` |
|    886 |  447 | `		ph7_value *pField = ph7_array_fetch(pIn,azField[i],-1);` |
|    886 |  448 | `		ph7_array_add_strkey_elem(pOut,azField[i],pField);` |
|    444 |  449 | `	}` |
|     70 |  450 | `	return PH7_OK;` |
|     38 |  451 | `}` |
|      - |  452 | `/*` |
|      - |  453 | ` * The thirteen NAMED fields of a stat answer, filled from thirteen values in` |
|      - |  454 | ` * php's own order. Two devices that never had a stat need one: php's memory` |
|      - |  455 | ` * streams (php://memory, php://temp, data://) answer a SYNTHETIC record that` |
|      - |  456 | ` * describes no file at all, and a pipe/socket/standard descriptor answers the` |
|      - |  457 | ` * real fstat() of its handle. Both go through here so the field list -- and` |
|      - |  458 | ` * therefore what PH7_VfsStatDoubleUp() can double -- exists in one place.` |
|      - |  459 | ` */` |
|     48 |  460 | `PH7_PRIVATE int PH7_VfsStatFill(ph7_value *pArray,ph7_value *pWorker,const ph7_int64 *aVal)` |
|      1 |  461 | `{` |
|      - |  462 | `	static const char * const azField[] = {` |
|      - |  463 | `		"dev","ino","mode","nlink","uid","gid","rdev","size",` |
|      - |  464 | `		"atime","mtime","ctime","blksize","blocks"` |
|      - |  465 | `	};` |
|      - |  466 | `	sxu32 i;` |
|     49 |  467 | `	if( pArray == 0 \|\| pWorker == 0 \|\| aVal == 0 ){` |
|    ! 0 |  468 | `		return -1;` |
|      - |  469 | `	}` |
|    673 |  470 | `	for( i = 0 ; i < SX_ARRAYSIZE(azField) ; ++i ){` |
|    625 |  471 | `		ph7_value_int64(pWorker,aVal[i]);` |
|    625 |  472 | `		ph7_array_add_strkey_elem(pArray,azField[i],pWorker); /* Takes its own copy */` |
|    313 |  473 | `	}` |
|     49 |  474 | `	return PH7_OK;` |
|     25 |  475 | `}` |
|      - |  476 | `/*` |
|      - |  477 | ` * The real fstat() of an open descriptor, in the shape above. php's pipe,` |
|      - |  478 | ` * socket and php://stdin\|stdout\|stderr streams all answer exactly this -- the` |
|      - |  479 | ` * same call, so the same platform answers, including the FAILURE that makes` |
|      - |  480 | ` * fstat() report false.` |
|      - |  481 | ` */` |
|    ! 0 |  482 | `PH7_PRIVATE int PH7_VfsStatFromFd(int iFd,ph7_value *pArray,ph7_value *pWorker)` |
|    ! 0 |  483 | `{` |
|      - |  484 | `#ifdef __WINNT__` |
|      - |  485 | `	struct _stat64 st;` |
|    ! 0 |  486 | `	if( iFd < 0 \|\| _fstat64(iFd,&st) != 0 ){` |
|    ! 0 |  487 | `		return -1;` |
|      - |  488 | `	}` |
|      - |  489 | `#else` |
|      - |  490 | `	struct stat st;` |
|    ! 0 |  491 | `	if( iFd < 0 \|\| fstat(iFd,&st) != 0 ){` |
|    ! 0 |  492 | `		return -1;` |
|      - |  493 | `	}` |
|      - |  494 | `#endif` |
|      - |  495 | `	{` |
|      - |  496 | `		ph7_int64 aVal[13];` |
|    ! 0 |  497 | `		aVal[0]  = (ph7_int64)st.st_dev;` |
|    ! 0 |  498 | `		aVal[1]  = (ph7_int64)st.st_ino;` |
|    ! 0 |  499 | `		aVal[2]  = (ph7_int64)st.st_mode;` |
|    ! 0 |  500 | `		aVal[3]  = (ph7_int64)st.st_nlink;` |
|    ! 0 |  501 | `		aVal[4]  = (ph7_int64)st.st_uid;` |
|    ! 0 |  502 | `		aVal[5]  = (ph7_int64)st.st_gid;` |
|    ! 0 |  503 | `		aVal[6]  = (ph7_int64)st.st_rdev;` |
|    ! 0 |  504 | `		aVal[7]  = (ph7_int64)st.st_size;` |
|    ! 0 |  505 | `		aVal[8]  = (ph7_int64)st.st_atime;` |
|    ! 0 |  506 | `		aVal[9]  = (ph7_int64)st.st_mtime;` |
|    ! 0 |  507 | `		aVal[10] = (ph7_int64)st.st_ctime;` |
|      - |  508 | `#ifdef __WINNT__` |
|      - |  509 | `		/* Windows has neither field, and php reports -1 for both there -- on` |
|      - |  510 | `		 * EVERY stream, a plain file included (read back from php 8.5.8 on the` |
|      - |  511 | `		 * gate guest). */` |
|    ! 0 |  512 | `		aVal[11] = -1;` |
|    ! 0 |  513 | `		aVal[12] = -1;` |
|      - |  514 | `#else` |
|    ! 0 |  515 | `		aVal[11] = (ph7_int64)st.st_blksize;` |
|    ! 0 |  516 | `		aVal[12] = (ph7_int64)st.st_blocks;` |
|      - |  517 | `#endif` |
|    ! 0 |  518 | `		return PH7_VfsStatFill(pArray,pWorker,aVal);` |
|      - |  519 | `	}` |
|    ! 0 |  520 | `}` |
|      - |  521 | `/* Defined with the path operations below, and used by every one of them. */` |
|      - |  522 | `static int VfsBuiltinWrapperRefuses(ph7_context *pCtx,const char *zPath,const char *zDest,int eOp);` |
|      - |  523 | `#define VFS_POP_UNLINK 0` |
|      - |  524 | `#define VFS_POP_RENAME 1` |
|      - |  525 | `#define VFS_POP_MKDIR  2   /* mkdir and rmdir: false, and nothing said -- but they` |
|      - |  526 | `                            * are two different operations to a wrapper that` |
|      - |  527 | `                            * IMPLEMENTS them, which ext/phar does */` |
|      - |  528 | `#define VFS_POP_CHMOD  3` |
|      - |  529 | `#define VFS_POP_RMDIR  4` |
|      - |  530 | `/*` |
|      - |  531 | ` * php's WRITE door for a path a userland wrapper owns: unlink(), rename(), mkdir(),` |
|      - |  532 | ` * rmdir(), and the stream_metadata() that touch(), chmod(), chown() and chgrp() all` |
|      - |  533 | ``  * become. PHL sent every one of them to the OS instead -- so `unlink('vfs://root/f')` `` |
|      - |  534 | `` * reported `No such file or directory` about a path the OS had never heard of, and`` |
|      - |  535 | ` * the wrapper was never told -- which is the second half of what a test suite that` |
|      - |  536 | `` * fakes a filesystem does with one. (The `?resource $context` these builtins already`` |
|      - |  537 | ` * screened is what php sets on the serving instance, so it is handed over too.)` |
|      - |  538 | ` *` |
|      - |  539 | ` * Answers 1 when a wrapper owned the path and the result is set; 0 when none did and` |
|      - |  540 | ` * the caller carries on to the VFS.` |
|      - |  541 | ` */` |
|  50432 |  542 | `static int VfsUserWrite(ph7_context *pCtx,const char *zPath,const char *zMethod,` |
|      - |  543 | `	void *pStreamCtx,ph7_value **apExtra,int nExtra)` |
|      5 |  544 | `{` |
|  50437 |  545 | `	int bAnswer = 0;` |
|  50437 |  546 | `	int rc = PH7_StreamUserPathOp(pCtx,zPath,zMethod,pStreamCtx,apExtra,nExtra,&bAnswer);` |
|  50437 |  547 | `	if( rc == PHL_URLSTAT_NOWRAP ){` |
|  50365 |  548 | `		return 0;` |
|      - |  549 | `	}` |
|      - |  550 | `	/* A wrapper that declined, one with no such method (php has already said so), and` |
|      - |  551 | `	 * one that threw all answer the same false; the throw is its own report. */` |
|     74 |  552 | `	ph7_result_bool(pCtx,rc == PHL_URLSTAT_OK ? bAnswer : 0);` |
|     74 |  553 | `	return 1;` |
|  25196 |  554 | `}` |
|      - |  555 | `/*` |
|      - |  556 | ` * ---------------------------------------------------------------------------` |
|      - |  557 | ` * The stat family over a path a USERLAND stream wrapper owns.` |
|      - |  558 | ` *` |
|      - |  559 | `` * php routes every member of the family through one door -- `php_stat`, over`` |
|      - |  560 | `` * `php_stream_url_stat_path` -- so a registered wrapper answers `file_exists()`,`` |
|      - |  561 | `` * `is_dir()`, `filesize()`, `stat()` and the rest through its own `url_stat()`.`` |
|      - |  562 | ` * PHL asked the OS VFS straight from each builtin, so a wrapper was reachable` |
|      - |  563 | ` * through fopen() and invisible to everything that asks ABOUT a name: vfsStream --` |
|      - |  564 | ` * what every test suite that fakes a filesystem uses -- could not report the size` |
|      - |  565 | `` * of a virtual file, and `file_exists('vfs://root/t.txt')` was false.`` |
|      - |  566 | ` *` |
|      - |  567 | ` * Everything php's door decides is decided here, once, for all of them:` |
|      - |  568 | ` *  - the FLAGS the wrapper is handed: LINK for the three lstat readers, QUIET for` |
|      - |  569 | ` *    the seven existence/access questions, and NOCACHE always (php's own one-entry` |
|      - |  570 | ` *    stat cache lives above this door, so it asks for a fresh answer every time);` |
|      - |  571 | ` *  - what a FAILURE says: nothing at all for a quiet ask -- php answers a plain` |
|      - |  572 | `` *    false there -- and `%s(): stat failed for %s` / `Lstat failed for` for the`` |
|      - |  573 | ` *    rest;` |
|      - |  574 | ` *  - how the thirteen fields answer each question: the mode's type bits, and php's` |
|      - |  575 | ` *    owner/group/other access masks.` |
|      - |  576 | ` * ---------------------------------------------------------------------------` |
|      - |  577 | ` */` |
|      - |  578 | `/* The seven questions php asks QUIETLY, and reports as a bare false. */` |
|  25783 |  579 | `static int VfsAskIsQuiet(int eAsk)` |
|      5 |  580 | `{` |
|  25788 |  581 | `	return eAsk <= PH7_STAT_ASK_IS_X;` |
|      5 |  582 | `}` |
|      - |  583 | ``/* The three php asks with LSTAT, and words a failure as `Lstat failed for`. */`` |
|  25757 |  584 | `static int VfsAskIsLink(int eAsk)` |
|      5 |  585 | `{` |
|  38556 |  586 | `	return eAsk == PH7_STAT_ASK_IS_LINK \|\| eAsk == PH7_STAT_ASK_TYPE` |
|  38610 |  587 | `	    \|\| eAsk == PH7_STAT_ASK_LSTAT;` |
|      5 |  588 | `}` |
|      - |  589 | `/*` |
|      - |  590 | ` * php's rmask/wmask/xmask for a stat record that did not come from a plain file.` |
|      - |  591 | ` * It starts at the OTHER bits and moves to the OWNER bits when the record's uid is` |
|      - |  592 | ` * the process's, or to the GROUP bits when its gid is the process's or one of its` |
|      - |  593 | ` * supplementary groups. On Windows php makes none of those comparisons: it seeds` |
|      - |  594 | `` * the masks with `S_IREAD/S_IWRITE/S_IEXEC` -- the OWNER bits -- and reads those`` |
|      - |  595 | ` * whatever the record says (read back from php 8.5 on the gate guest, where a` |
|      - |  596 | ` * record with uid 65534 and mode 0644 still answered readable AND writable).` |
|      - |  597 | ` */` |
|     48 |  598 | `PH7_PRIVATE void PH7_VfsStatAccessMasks(ph7_context *pCtx,ph7_int64 nUid,ph7_int64 nGid,` |
|      - |  599 | `	int *pR,int *pW,int *pX)` |
|      1 |  600 | `{` |
|      - |  601 | `#ifdef __WINNT__` |
|      - |  602 | `	SXUNUSED(pCtx); SXUNUSED(nUid); SXUNUSED(nGid);` |
|      1 |  603 | `	*pR = 0400; *pW = 0200; *pX = 0100;` |
|      - |  604 | `#else` |
|     48 |  605 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     48 |  606 | `	*pR = 0004; *pW = 0002; *pX = 0001;` |
|     48 |  607 | `	if( pVfs && pVfs->xUid && (ph7_int64)pVfs->xUid() == nUid ){` |
|     30 |  608 | `		*pR = 0400; *pW = 0200; *pX = 0100;` |
|     30 |  609 | `		return;` |
|      - |  610 | `	}` |
|     18 |  611 | `	if( pVfs && pVfs->xGid && (ph7_int64)pVfs->xGid() == nGid ){` |
|    ! 0 |  612 | `		*pR = 0040; *pW = 0020; *pX = 0010;` |
|    ! 0 |  613 | `		return;` |
|      - |  614 | `	}` |
|      - |  615 | `#ifdef __UNIXES__` |
|      - |  616 | `	{` |
|      - |  617 | `		/* php's last arm: the record's group may be one the process merely` |
|      - |  618 | `		 * belongs to. */` |
|     18 |  619 | `		int nGroup = getgroups(0,NULL);` |
|     18 |  620 | `		if( nGroup > 0 ){` |
|     27 |  621 | `			gid_t *aGid = (gid_t *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     18 |  622 | `				(sxu32)nGroup * (sxu32)sizeof(gid_t));` |
|     18 |  623 | `			if( aGid ){` |
|     18 |  624 | `				int n = getgroups(nGroup,aGid);` |
|      - |  625 | `				int i;` |
|    207 |  626 | `				for( i = 0 ; i < n ; ++i ){` |
|    189 |  627 | `					if( (ph7_int64)aGid[i] == nGid ){` |
|    ! 0 |  628 | `						*pR = 0040; *pW = 0020; *pX = 0010;` |
|    ! 0 |  629 | `						break;` |
|      - |  630 | `					}` |
|    144 |  631 | `				}` |
|     18 |  632 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,aGid);` |
|      9 |  633 | `			}` |
|      9 |  634 | `		}` |
|      - |  635 | `	}` |
|      - |  636 | `#endif /* __UNIXES__ */` |
|      - |  637 | `#endif /* __WINNT__ */` |
|     25 |  638 | `}` |
|      - |  639 | `/*` |
|      - |  640 | ` * Ask the wrapper that owns zPath, with the flags this member of the family uses.` |
|      - |  641 | ` * PHL_URLSTAT_NOWRAP means nothing here owns the path and the caller carries on to` |
|      - |  642 | ` * the VFS.` |
|      - |  643 | ` */` |
|  25749 |  644 | `PH7_PRIVATE int PH7_VfsUserStatFields(ph7_context *pCtx,const char *zPath,int eAsk,` |
|      - |  645 | `	ph7_int64 *aVal)` |
|      5 |  646 | `{` |
|  25754 |  647 | `	int iFlags = PH7_URL_STAT_NOCACHE;` |
|      - |  648 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  649 | `	/* An ARCHIVE answers for its own entries. php gives every wrapper a` |
|      - |  650 | `	 * url_stat handler and routes the whole stat family through it, which is` |
|      - |  651 | ``	 * what makes `file_exists('phar://x.phar/f')` and `is_dir()` true for a name`` |
|      - |  652 | `	 * that exists nowhere on disk. This engine has the door for a USERLAND` |
|      - |  653 | `	 * wrapper (below) and this one built-in wrapper that needs it. */` |
|  25754 |  654 | `	if( zPath && SyStrnicmp(zPath,"phar://",sizeof("phar://")-1) == 0 ){` |
|     16 |  655 | `		return PH7_PharUrlStat(pCtx->pVm,&zPath[sizeof("phar://")-1],aVal) == 0` |
|      8 |  656 | `			? PHL_URLSTAT_OK : PHL_URLSTAT_FAIL;` |
|      - |  657 | `	}` |
|      - |  658 | `#endif` |
|  25738 |  659 | `	if( VfsAskIsLink(eAsk) ){` |
|    124 |  660 | `		iFlags \|= PH7_URL_STAT_LINK;` |
|     61 |  661 | `	}` |
|  25738 |  662 | `	if( VfsAskIsQuiet(eAsk) ){` |
|  25332 |  663 | `		iFlags \|= PH7_URL_STAT_QUIET;` |
|  12661 |  664 | `	}` |
|  25738 |  665 | `	return PH7_StreamUserUrlStat(pCtx,zPath,iFlags,aVal);` |
|  12877 |  666 | `}` |
|      - |  667 | `/*` |
|      - |  668 | ` * Answer one question from a filled record. Shared with SplFileInfo, whose` |
|      - |  669 | ` * accessors ask exactly the same things of exactly the same thirteen fields.` |
|      - |  670 | ` */` |
|    266 |  671 | `PH7_PRIVATE void PH7_VfsUserStatResult(ph7_context *pCtx,int eAsk,const ph7_int64 *aVal)` |
|      1 |  672 | `{` |
|    267 |  673 | `	ph7_int64 nMode = aVal[2];` |
|    267 |  674 | `	switch( eAsk ){` |
|      8 |  675 | `	case PH7_STAT_ASK_EXISTS:` |
|     17 |  676 | `		ph7_result_bool(pCtx,1);` |
|     17 |  677 | `		break;` |
|     12 |  678 | `	case PH7_STAT_ASK_IS_FILE:` |
|     25 |  679 | `		ph7_result_bool(pCtx,(nMode & PH7_S_IFMT) == PH7_S_IFREG);` |
|     25 |  680 | `		break;` |
|     13 |  681 | `	case PH7_STAT_ASK_IS_DIR:` |
|     27 |  682 | `		ph7_result_bool(pCtx,(nMode & PH7_S_IFMT) == PH7_S_IFDIR);` |
|     27 |  683 | `		break;` |
|     11 |  684 | `	case PH7_STAT_ASK_IS_LINK:` |
|     23 |  685 | `		ph7_result_bool(pCtx,(nMode & PH7_S_IFMT) == PH7_S_IFLNK);` |
|     23 |  686 | `		break;` |
|     24 |  687 | `	case PH7_STAT_ASK_IS_R:` |
|      - |  688 | `	case PH7_STAT_ASK_IS_W:` |
|      - |  689 | `	case PH7_STAT_ASK_IS_X: {` |
|      - |  690 | `		int iR,iW,iX,iMask;` |
|     49 |  691 | `		PH7_VfsStatAccessMasks(pCtx,aVal[4],aVal[5],&iR,&iW,&iX);` |
|     49 |  692 | `		iMask = eAsk == PH7_STAT_ASK_IS_R ? iR : (eAsk == PH7_STAT_ASK_IS_W ? iW : iX);` |
|     49 |  693 | `		ph7_result_bool(pCtx,(nMode & iMask) != 0);` |
|     49 |  694 | `		break; }` |
|     19 |  695 | `	case PH7_STAT_ASK_SIZE:  ph7_result_int64(pCtx,aVal[7]);  break;` |
|     15 |  696 | `	case PH7_STAT_ASK_ATIME: ph7_result_int64(pCtx,aVal[8]);  break;` |
|     15 |  697 | `	case PH7_STAT_ASK_MTIME: ph7_result_int64(pCtx,aVal[9]);  break;` |
|     15 |  698 | `	case PH7_STAT_ASK_CTIME: ph7_result_int64(pCtx,aVal[10]); break;` |
|      7 |  699 | `	case PH7_STAT_ASK_OWNER: ph7_result_int64(pCtx,aVal[4]);  break;` |
|      7 |  700 | `	case PH7_STAT_ASK_GROUP: ph7_result_int64(pCtx,aVal[5]);  break;` |
|     15 |  701 | `	case PH7_STAT_ASK_INODE: ph7_result_int64(pCtx,aVal[1]);  break;` |
|     15 |  702 | `	case PH7_STAT_ASK_PERMS: ph7_result_int64(pCtx,nMode);    break;` |
|     11 |  703 | `	case PH7_STAT_ASK_TYPE: {` |
|      - |  704 | `		/* php's FS_TYPE: a symlink first (this ask lstats), then the S_IFMT` |
|      - |  705 | `		 * switch, then a NOTICE naming the type bits it did not recognise.` |
|      - |  706 | `		 * S_IFSOCK is the one arm php's Windows build does not compile, so a` |
|      - |  707 | `		 * socket record answers "unknown" there -- read back from the gate` |
|      - |  708 | `		 * guest's php 8.5. */` |
|     23 |  709 | `		const char *zType = 0;` |
|     23 |  710 | `		switch( (int)(nMode & PH7_S_IFMT) ){` |
|      5 |  711 | `		case PH7_S_IFLNK:  zType = "link";   break;` |
|    ! 0 |  712 | `		case PH7_S_IFIFO:  zType = "fifo";   break;` |
|    ! 0 |  713 | `		case PH7_S_IFCHR:  zType = "char";   break;` |
|      5 |  714 | `		case PH7_S_IFDIR:  zType = "dir";    break;` |
|    ! 0 |  715 | `		case PH7_S_IFBLK:  zType = "block";  break;` |
|     11 |  716 | `		case PH7_S_IFREG:  zType = "file";   break;` |
|      - |  717 | `#ifndef __WINNT__` |
|    ! 0 |  718 | `		case PH7_S_IFSOCK: zType = "socket"; break;` |
|      - |  719 | `#endif` |
|      4 |  720 | `		default: break;` |
|      - |  721 | `		}` |
|     23 |  722 | `		if( zType == 0 ){` |
|      7 |  723 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      2 |  724 | `				"Unknown file type (%d)",(int)(nMode & PH7_S_IFMT));` |
|      5 |  725 | `			zType = "unknown";` |
|      2 |  726 | `		}` |
|     23 |  727 | `		ph7_result_string(pCtx,zType,-1);` |
|     23 |  728 | `		break; }` |
|      4 |  729 | `	default: {` |
|      - |  730 | `		/* stat()/lstat(): php's thirteen fields numbered 0..12 and then named. */` |
|      9 |  731 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|      9 |  732 | `		ph7_value *pWorker = ph7_context_new_scalar(pCtx);` |
|      9 |  733 | `		ph7_value *pFull = ph7_context_new_array(pCtx);` |
|      9 |  734 | `		if( pArray == 0 \|\| pWorker == 0 ){` |
|    ! 0 |  735 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  736 | `			break;` |
|      - |  737 | `		}` |
|      9 |  738 | `		PH7_VfsStatFill(pArray,pWorker,aVal);` |
|      9 |  739 | `		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){` |
|      9 |  740 | `			ph7_result_value(pCtx,pFull);` |
|      5 |  741 | `		}else{` |
|    ! 0 |  742 | `			ph7_result_value(pCtx,pArray);` |
|      - |  743 | `		}` |
|      8 |  744 | `		break; }` |
|      - |  745 | `	}` |
|    267 |  746 | `}` |
|      - |  747 | `/*` |
|      - |  748 | ` * The one line every member of the family carries: if a userland wrapper owns this` |
|      - |  749 | ` * path, answer from it and stop. Returns 0 when nothing does.` |
|      - |  750 | ` */` |
|  25606 |  751 | `static int VfsUserStat(ph7_context *pCtx,const char *zPath,int eAsk)` |
|      5 |  752 | `{` |
|      - |  753 | `	ph7_int64 aVal[13];` |
|      - |  754 | `	SyBlob sPath;` |
|      - |  755 | `	int rc;` |
|  25611 |  756 | `	if( zPath == 0 ){` |
|    ! 0 |  757 | `		return 0;` |
|      - |  758 | `	}` |
|      - |  759 | `	/* The wrapper is PHP code, and running some can move the argument slot zPath` |
|      - |  760 | `	 * points into -- so the copy the failure message needs is taken first. */` |
|  25611 |  761 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|  25611 |  762 | `	SyBlobAppend(&sPath,zPath,(sxu32)SyStrlen(zPath));` |
|  25611 |  763 | `	SyBlobAppend(&sPath,"",1); /* NUL, for the %s below */` |
|  25611 |  764 | `	rc = PH7_VfsUserStatFields(pCtx,zPath,eAsk,aVal);` |
|  25611 |  765 | `	if( rc == PHL_URLSTAT_NOWRAP ){` |
|  25325 |  766 | `		SyBlobRelease(&sPath);` |
|  25325 |  767 | `		return 0;` |
|      - |  768 | `	}` |
|    287 |  769 | `	if( rc == PHL_URLSTAT_OK ){` |
|    237 |  770 | `		PH7_VfsUserStatResult(pCtx,eAsk,aVal);` |
|    119 |  771 | `	}else{` |
|     51 |  772 | `		if( !VfsAskIsQuiet(eAsk) ){` |
|     25 |  773 | `			VfsThrowStatWarning(pCtx,(const char *)SyBlobData(&sPath),VfsAskIsLink(eAsk));` |
|     12 |  774 | `		}` |
|     51 |  775 | `		ph7_result_bool(pCtx,0);` |
|      - |  776 | `	}` |
|    287 |  777 | `	SyBlobRelease(&sPath);` |
|    287 |  778 | `	return 1;` |
|  12805 |  779 | `}` |
|      - |  780 | `/*` |
|      - |  781 | ` * Can this path be stat'ed at all? The three TIME readers report a failure as -1,` |
|      - |  782 | ` * which is also a legitimate timestamp (a file stamped in the last second before` |
|      - |  783 | ` * the epoch), so the failure verdict is asked of the VFS separately rather than` |
|      - |  784 | ` * read off the value -- one extra call, and only on the negative branch.` |
|      - |  785 | ` */` |
|    152 |  786 | `static int VfsPathStatable(ph7_vfs *pVfs,const char *zPath)` |
|      4 |  787 | `{` |
|    156 |  788 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|    ! 0 |  789 | `		return 0;` |
|      - |  790 | `	}` |
|    156 |  791 | `	return pVfs->xFileExists(zPath) == PH7_OK;` |
|     90 |  792 | `}` |
|      - |  793 | `/*` |
|      - |  794 | `` * php declares every path parameter in this file `string`, and its ZPP converts an`` |
|      - |  795 | ` * OBJECT with __toString before the C body runs. PHL's guards asked` |
|      - |  796 | ` * ph7_value_is_string, which is false for such an object, so each of these builtins` |
|      - |  797 | `` * silently answered its miss value for one -- `is_dir($it->current())` answered FALSE`` |
|      - |  798 | ` * for a directory, which is how phpcs walked three top-level directories and found` |
|      - |  799 | ` * 16 of a project's 711 files without reporting anything wrong. SplFileInfo is the` |
|      - |  800 | ` * object every real program passes here (RecursiveDirectoryIterator yields it).` |
|      - |  801 | ` *` |
|      - |  802 | ` * The conversion itself is already in ph7_value_to_string(); only the screen in front` |
|      - |  803 | ` * of it was wrong.` |
|      - |  804 | ` */` |
|  97482 |  805 | `static int VfsPathArgUsable(ph7_value *pVal)` |
|      5 |  806 | `{` |
|  97487 |  807 | `	if( pVal == 0 ){` |
|    ! 0 |  808 | `		return 0;` |
|      - |  809 | `	}` |
|  97487 |  810 | `	if( ph7_value_is_string(pVal) ){` |
|  97487 |  811 | `		return 1;` |
|      - |  812 | `	}` |
|    ! 0 |  813 | `	return ph7_value_is_object(pVal) && !PH7_MemObjIsNotStringable(pVal);` |
|  48715 |  814 | `}` |
|      - |  815 |  |
|      - |  816 | `/*` |
|      - |  817 | ` * bool chdir(string $directory)` |
|      - |  818 | ` *  Change the current directory.` |
|      - |  819 | ` * Parameters` |
|      - |  820 | ` *  $directory` |
|      - |  821 | ` *   The new current directory` |
|      - |  822 | ` * Return` |
|      - |  823 | ` *  TRUE on success or FALSE on failure.` |
|      - |  824 | ` */` |
|  17120 |  825 | `static int PH7_vfs_chdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  826 | `{` |
|      - |  827 | `	const char *zPath;` |
|      - |  828 | `	ph7_vfs *pVfs;` |
|      - |  829 | `	int rc;` |
|      - |  830 | `	/* Only the ARITY is checked here: php coerces a scalar $directory to string,` |
|      - |  831 | `	 * so chdir(123) attempts "123" and warns that it does not exist. Requiring a` |
|      - |  832 | `	 * string outright made that call return FALSE silently, with no diagnostic. */` |
|  17125 |  833 | `	if( nArg < 1 ){` |
|      - |  834 | `		/* Missing argument,return FALSE */` |
|    ! 0 |  835 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  836 | `		return PH7_OK;` |
|      - |  837 | `	}` |
|      - |  838 | `	/* Point to the underlying vfs */` |
|  17125 |  839 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  17125 |  840 | `	if( pVfs == 0 \|\| pVfs->xChdir == 0 ){` |
|      - |  841 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  842 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  843 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  844 | `			ph7_function_name(pCtx)` |
|      - |  845 | `			);` |
|    ! 0 |  846 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  847 | `		return PH7_OK;` |
|      - |  848 | `	}` |
|      - |  849 | `	/* Point to the desired directory */` |
|  17125 |  850 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  851 | `	/* Perform the requested operation */` |
|  17125 |  852 | `	errno = 0;` |
|  17125 |  853 | `	rc = pVfs->xChdir(zPath);` |
|  17125 |  854 | `	if( rc != PH7_OK ){` |
|      - |  855 | `		/* chdir has its own php shape: no path, and the errno spelled out. */` |
|      8 |  856 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|      4 |  857 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|      2 |  858 | `	}` |
|      - |  859 | `	/* IO return value */` |
|  17125 |  860 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  17125 |  861 | `	return PH7_OK;` |
|   8561 |  862 | `}` |
|      - |  863 | `/*` |
|      - |  864 | ` * bool chroot(string $directory)` |
|      - |  865 | ` *  Change the root directory.` |
|      - |  866 | ` * Parameters` |
|      - |  867 | ` *  $directory` |
|      - |  868 | ` *   The path to change the root directory to` |
|      - |  869 | ` * Return` |
|      - |  870 | ` *  TRUE on success or FALSE on failure.` |
|      - |  871 | ` *` |
|      - |  872 | ` * POSIX only, like php's: the registration below is guarded the same way, and an` |
|      - |  873 | ` * unreferenced static is an error under the Windows build's /W4 /WX.` |
|      - |  874 | ` */` |
|      - |  875 | `#ifndef __WINNT__` |
|    ! 0 |  876 | `static int PH7_vfs_chroot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - |  877 | `{` |
|      - |  878 | `	const char *zPath;` |
|      - |  879 | `	ph7_vfs *pVfs;` |
|      - |  880 | `	int rc;` |
|    ! 0 |  881 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - |  882 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  883 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  884 | `		return PH7_OK;` |
|      - |  885 | `	}` |
|      - |  886 | `	/* Point to the underlying vfs */` |
|    ! 0 |  887 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    ! 0 |  888 | `	if( pVfs == 0 \|\| pVfs->xChroot == 0 ){` |
|      - |  889 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  890 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  891 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  892 | `			ph7_function_name(pCtx)` |
|      - |  893 | `			);` |
|    ! 0 |  894 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  895 | `		return PH7_OK;` |
|      - |  896 | `	}` |
|      - |  897 | `	/* Point to the desired directory */` |
|    ! 0 |  898 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - |  899 | `	/* Perform the requested operation */` |
|    ! 0 |  900 | `	errno = 0;` |
|    ! 0 |  901 | `	rc = pVfs->xChroot(zPath);` |
|    ! 0 |  902 | `	if( rc != PH7_OK ){` |
|      - |  903 | `		/* php's own wording, and the failure a script actually meets: chroot(2)` |
|      - |  904 | `		 * needs privilege, so an ordinary process gets EPERM. PHL answered the` |
|      - |  905 | `		 * bare false in SILENCE — a refused chroot() and a chroot() that did` |
|      - |  906 | `		 * nothing looked the same to the caller. */` |
|    ! 0 |  907 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s (errno %d)",` |
|    ! 0 |  908 | `			ph7_function_name(pCtx),VfsStrerror(errno),errno);` |
|    ! 0 |  909 | `	}` |
|      - |  910 | `	/* IO return value */` |
|    ! 0 |  911 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    ! 0 |  912 | `	return PH7_OK;` |
|    ! 0 |  913 | `}` |
|      - |  914 | `#endif /* __WINNT__ */` |
|      - |  915 | `/*` |
|      - |  916 | ` * string getcwd(void)` |
|      - |  917 | ` *  Gets the current working directory.` |
|      - |  918 | ` * Parameters` |
|      - |  919 | ` *  None` |
|      - |  920 | ` * Return` |
|      - |  921 | ` *  Returns the current working directory on success, or FALSE on failure.` |
|      - |  922 | ` */` |
|     27 |  923 | `static int PH7_vfs_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  924 | `{` |
|      - |  925 | `	ph7_vfs *pVfs;` |
|      - |  926 | `	int rc;` |
|      - |  927 | `	/* Point to the underlying vfs */` |
|     32 |  928 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     32 |  929 | `	if( pVfs == 0 \|\| pVfs->xGetcwd == 0 ){` |
|    ! 0 |  930 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 |  931 | `		SXUNUSED(apArg);` |
|      - |  932 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  933 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  934 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 |  935 | `			ph7_function_name(pCtx)` |
|      - |  936 | `			);` |
|    ! 0 |  937 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  938 | `		return PH7_OK;` |
|      - |  939 | `	}` |
|     32 |  940 | `	ph7_result_string(pCtx,"",0);` |
|      - |  941 | `	/* Perform the requested operation */` |
|     32 |  942 | `	rc = pVfs->xGetcwd(pCtx);` |
|     32 |  943 | `	if( rc != PH7_OK ){` |
|      - |  944 | `		/* Error,return FALSE */` |
|    ! 0 |  945 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  946 | `	}` |
|     32 |  947 | `	return PH7_OK;` |
|     17 |  948 | `}` |
|      - |  949 | `/*` |
|      - |  950 | ` * bool rmdir(string $directory)` |
|      - |  951 | ` *  Removes directory.` |
|      - |  952 | ` * Parameters` |
|      - |  953 | ` *  $directory` |
|      - |  954 | ` *   The path to the directory` |
|      - |  955 | ` * Return` |
|      - |  956 | ` *  TRUE on success or FALSE on failure.` |
|      - |  957 | ` */` |
|    340 |  958 | `static int PH7_vfs_rmdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  959 | `{` |
|      - |  960 | `	const char *zPath;` |
|      - |  961 | `	ph7_vfs *pVfs;` |
|      - |  962 | `	phl_stream_ctx *pStreamCtx;` |
|    345 |  963 | `	int rc,bThrew = 0;` |
|    345 |  964 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - |  965 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 |  966 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  967 | `		return PH7_OK;` |
|      - |  968 | `	}` |
|      - |  969 | ``	/* php's `?resource $context`, refused when it is a resource of another kind,`` |
|      - |  970 | `	 * and SET on the wrapper instance the write door below dispatches to. */` |
|    345 |  971 | `	pStreamCtx = PH7_StreamCtxFromArg(pCtx,nArg,apArg,1,"$context",0,&bThrew);` |
|    345 |  972 | `	if( bThrew ){` |
|      3 |  973 | `		return PH7_OK;` |
|      - |  974 | `	}` |
|      - |  975 | `	/* php's rmdir() hands the wrapper the url and its own $options -- always` |
|      - |  976 | `	 * STREAM_REPORT_ERRORS, the bit that says a diagnostic is wanted. */` |
|      - |  977 | `	{` |
|      - |  978 | `		ph7_value sOpt;` |
|      - |  979 | `		ph7_value *apExtra[1];` |
|      - |  980 | `		int bDone;` |
|    343 |  981 | `		PH7_MemObjInit(pCtx->pVm,&sOpt);` |
|    343 |  982 | `		ph7_value_int(&sOpt,PH7_STREAM_REPORT_ERRORS);` |
|    343 |  983 | `		apExtra[0] = &sOpt;` |
|    504 |  984 | `		bDone = VfsUserWrite(pCtx,ph7_value_to_string(apArg[0],0),"rmdir",` |
|    161 |  985 | `			(void *)pStreamCtx,apExtra,1);` |
|    343 |  986 | `		PH7_MemObjRelease(&sOpt);` |
|    343 |  987 | `		if( bDone ){` |
|      7 |  988 | `			return PH7_OK;` |
|      - |  989 | `		}` |
|      - |  990 | `	}` |
|    337 |  991 | `	if( VfsBuiltinWrapperRefuses(pCtx,ph7_value_to_string(apArg[0],0),0,VFS_POP_RMDIR) ){` |
|     21 |  992 | `		return PH7_OK;` |
|      - |  993 | `	}` |
|      - |  994 | `	/* Point to the underlying vfs */` |
|    317 |  995 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    317 |  996 | `	if( pVfs == 0 \|\| pVfs->xRmdir == 0 ){` |
|      - |  997 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 |  998 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  999 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1000 | `			ph7_function_name(pCtx)` |
|      - | 1001 | `			);` |
|    ! 0 | 1002 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1003 | `		return PH7_OK;` |
|      - | 1004 | `	}` |
|      - | 1005 | `	/* Point to the desired directory */` |
|    317 | 1006 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1007 | `	/* Perform the requested operation */` |
|    317 | 1008 | `	errno = 0;` |
|    317 | 1009 | `	rc = pVfs->xRmdir(zPath);` |
|    317 | 1010 | `	if( rc != PH7_OK ){` |
|     33 | 1011 | `		VfsThrowSysWarning(pCtx,zPath);` |
|     14 | 1012 | `	}` |
|      - | 1013 | `	/* IO return value */` |
|    317 | 1014 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    317 | 1015 | `	return PH7_OK;` |
|    167 | 1016 | `}` |
|      - | 1017 | `/*` |
|      - | 1018 | ` * bool is_dir(string $filename)` |
|      - | 1019 | ` *  Tells whether the given filename is a directory.` |
|      - | 1020 | ` * Parameters` |
|      - | 1021 | ` *  $filename` |
|      - | 1022 | ` *   Path to the file.` |
|      - | 1023 | ` * Return` |
|      - | 1024 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1025 | ` */` |
|  13904 | 1026 | `static int PH7_vfs_is_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1027 | `{` |
|      - | 1028 | `	const char *zPath;` |
|      - | 1029 | `	ph7_vfs *pVfs;` |
|      - | 1030 | `	int rc;` |
|  13909 | 1031 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 1032 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1033 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1034 | `		return PH7_OK;` |
|      - | 1035 | `	}` |
|      - | 1036 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 1037 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|  13909 | 1038 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_IS_DIR) ){` |
|     27 | 1039 | `		return PH7_OK;` |
|      - | 1040 | `	}` |
|  13883 | 1041 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  13883 | 1042 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|      - | 1043 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1044 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1045 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1046 | `			ph7_function_name(pCtx)` |
|      - | 1047 | `			);` |
|    ! 0 | 1048 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1049 | `		return PH7_OK;` |
|      - | 1050 | `	}` |
|      - | 1051 | `	/* Point to the desired directory */` |
|  13883 | 1052 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1053 | `	/* Perform the requested operation */` |
|  13883 | 1054 | `	rc = pVfs->xIsdir(zPath);` |
|      - | 1055 | `	/* IO return value */` |
|  13883 | 1056 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  13883 | 1057 | `	return PH7_OK;` |
|   6955 | 1058 | `}` |
|      - | 1059 | `/*` |
|      - | 1060 | ` * bool mkdir(string $pathname[,int $mode = 0777 [,bool $recursive = false])` |
|      - | 1061 | ` *  Make a directory.` |
|      - | 1062 | ` * Parameters` |
|      - | 1063 | ` *  $pathname` |
|      - | 1064 | ` *   The directory path.` |
|      - | 1065 | ` * $mode` |
|      - | 1066 | ` *  The mode is 0777 by default, which means the widest possible access.` |
|      - | 1067 | ` *  Note:` |
|      - | 1068 | ` *   mode is ignored on Windows.` |
|      - | 1069 | ` *   Note that you probably want to specify the mode as an octal number, which means` |
|      - | 1070 | ` *   it should have a leading zero. The mode is also modified by the current umask` |
|      - | 1071 | ` *   which you can change using umask().` |
|      - | 1072 | ` * $recursive` |
|      - | 1073 | ` *  Allows the creation of nested directories specified in the pathname.` |
|      - | 1074 | ` *  Defaults to FALSE. (Not used)` |
|      - | 1075 | ` * Return` |
|      - | 1076 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1077 | ` */` |
|      - | 1078 | `/*` |
|      - | 1079 | ` * A prefix the recursive mkdir must not try to CREATE: it names a volume rather` |
|      - | 1080 | ` * than a directory. POSIX has none of these (the leading "/" is never a prefix` |
|      - | 1081 | ` * here, since the walk starts one byte in).` |
|      - | 1082 | ` */` |
|    144 | 1083 | `static int VfsMkdirVolumePrefix(const char *z,int n)` |
|      3 | 1084 | `{` |
|      - | 1085 | `#ifdef __WINNT__` |
|      3 | 1086 | `	int i,nSep = 0;` |
|      3 | 1087 | `	if( n < 1 ){` |
|    ! 0 | 1088 | `		return 1;` |
|      - | 1089 | `	}` |
|      3 | 1090 | `	if( n == 2 && z[1] == ':' ){` |
|      3 | 1091 | `		return 1; /* a bare drive, "C:" */` |
|      - | 1092 | `	}` |
|      3 | 1093 | `	if( n > 1 && (z[0] == '/' \|\| z[0] == '\\') && (z[1] == '/' \|\| z[1] == '\\') ){` |
|    ! 0 | 1094 | `		for( i = 2 ; i < n ; i++ ){` |
|    ! 0 | 1095 | `			if( z[i] == '/' \|\| z[i] == '\\' ){` |
|    ! 0 | 1096 | `				nSep++;` |
|      - | 1097 | `			}` |
|    ! 0 | 1098 | `		}` |
|    ! 0 | 1099 | `		return nSep < 2; /* still inside \\server\share */` |
|      - | 1100 | `	}` |
|      3 | 1101 | `	return 0;` |
|      - | 1102 | `#else` |
|     82 | 1103 | `	SXUNUSED(z);` |
|    144 | 1104 | `	return n < 1;` |
|      - | 1105 | `#endif` |
|      3 | 1106 | `}` |
|      - | 1107 | `#ifdef __WINNT__` |
|      - | 1108 | `#define VFS_MKDIR_SLASH(c) ((c) == '/' \|\| (c) == '\\')` |
|      - | 1109 | `#else` |
|      - | 1110 | `#define VFS_MKDIR_SLASH(c) ((c) == '/')` |
|      - | 1111 | `#endif` |
|      - | 1112 | `/*` |
|      - | 1113 | ` * php's $recursive: create every missing ancestor, then the directory itself.` |
|      - | 1114 | `` * The flag reached the VFS and both back ends dropped it (`SXUNUSED(recursive)`),`` |
|      - | 1115 | `` * so `mkdir("$d/a/b", 0777, true)` -- the everyday way a script prepares an`` |
|      - | 1116 | ` * output tree -- warned "No such file or directory" and answered false whenever` |
|      - | 1117 | ` * more than one level was missing.` |
|      - | 1118 | ` *` |
|      - | 1119 | ` * php does the walk in the WRAPPER too, not in the syscall, and the rules the` |
|      - | 1120 | ` * oracle shows are: the mode is applied to every level it creates; an ancestor` |
|      - | 1121 | ` * that already exists is skipped in silence; the LEAF is always attempted, so an` |
|      - | 1122 | ` * existing one is "File exists" exactly as without the flag; a trailing` |
|      - | 1123 | ` * separator names the same directory; and the empty path is refused up front` |
|      - | 1124 | ` * with a message of its own.` |
|      - | 1125 | ` */` |
|     36 | 1126 | `static int VfsMkdirRecursive(ph7_context *pCtx,ph7_vfs *pVfs,const char *zPath,int iMode)` |
|      3 | 1127 | `{` |
|      - | 1128 | `	SyBlob sWorker;` |
|      - | 1129 | `	const char *zLocal;` |
|     39 | 1130 | `	int i,nPath,nScheme,rc = PH7_OK;` |
|      - | 1131 | `	/* The strip first: the component walk must not cut a "file://" scheme up. */` |
|     39 | 1132 | `	zLocal = PH7_VmFileUrlLocalPath(zPath);` |
|     39 | 1133 | `	nScheme = PH7_VmUrlSchemeLen(zPath,-1);` |
|     36 | 1134 | `	if( zLocal == zPath && nScheme == (int)sizeof("file")-1` |
|     17 | 1135 | `	 && SyStrnicmp(zPath,"file",sizeof("file")-1) == 0 ){` |
|      - | 1136 | `		/* A file:// AUTHORITY this build will not reach: the strip handed the` |
|      - | 1137 | `		 * URL straight back. php answers false and creates NOTHING, where the` |
|      - | 1138 | `		 * walk below would cut the URL into components and make a directory` |
|      - | 1139 | `		 * literally called "file:". (A name under any OTHER scheme really does` |
|      - | 1140 | `` 		 * become a directory of that name on php too -- measured: `zzz://a/b` `` |
|      - | 1141 | ``		 * leaves a `zzz:` behind there as well -- so only this one is refused.) */`` |
|      3 | 1142 | `		return -1;` |
|      - | 1143 | `	}` |
|     37 | 1144 | `	zPath = zLocal;` |
|     37 | 1145 | `	nPath = (int)SyStrlen(zPath);` |
|      - | 1146 | `	/* A trailing separator names the same directory; php's own expand_filepath` |
|      - | 1147 | `	 * drops it before it starts. */` |
|     39 | 1148 | `	while( nPath > 1 && VFS_MKDIR_SLASH(zPath[nPath-1]) ){` |
|      3 | 1149 | `		nPath--;` |
|      1 | 1150 | `	}` |
|     37 | 1151 | `	if( nPath < 1 ){` |
|      - | 1152 | `		/* php: expand_filepath() refuses it, with this wording and no path. */` |
|      3 | 1153 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Invalid path",ph7_function_name(pCtx));` |
|      3 | 1154 | `		return -1;` |
|      - | 1155 | `	}` |
|     35 | 1156 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|   1485 | 1157 | `	for( i = 1 ; i <= nPath ; i++ ){` |
|   1461 | 1158 | `		int bLeaf = (i == nPath);` |
|   1461 | 1159 | `		if( !bLeaf ){` |
|      - | 1160 | `			/* Only at a separator that ENDS a component: a run of them names` |
|      - | 1161 | `			 * the same ancestor once. */` |
|   1429 | 1162 | `			if( !VFS_MKDIR_SLASH(zPath[i]) \|\| VFS_MKDIR_SLASH(zPath[i-1]) ){` |
|   1285 | 1163 | `				continue;` |
|      - | 1164 | `			}` |
|    147 | 1165 | `			if( VfsMkdirVolumePrefix(zPath,i) ){` |
|      3 | 1166 | `				continue;` |
|      - | 1167 | `			}` |
|     82 | 1168 | `		}` |
|    179 | 1169 | `		SyBlobReset(&sWorker);` |
|    176 | 1170 | `		if( SyBlobAppend(&sWorker,zPath,(sxu32)i) != SXRET_OK` |
|    179 | 1171 | `		 \|\| SyBlobNullAppend(&sWorker) != SXRET_OK ){` |
|    ! 0 | 1172 | `			rc = -1;` |
|    ! 0 | 1173 | `			break;` |
|      - | 1174 | `		}` |
|    179 | 1175 | `		if( !bLeaf && VfsPathStatable(pVfs,(const char *)SyBlobData(&sWorker)) ){` |
|    118 | 1176 | `			continue; /* an ancestor that is already there */` |
|      - | 1177 | `		}` |
|     64 | 1178 | `		errno = 0;` |
|     64 | 1179 | `		rc = pVfs->xMkdir((const char *)SyBlobData(&sWorker),iMode,0);` |
|     64 | 1180 | `		if( rc != PH7_OK ){` |
|     13 | 1181 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      8 | 1182 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|     10 | 1183 | `			break;` |
|      - | 1184 | `		}` |
|     23 | 1185 | `	}` |
|     35 | 1186 | `	SyBlobRelease(&sWorker);` |
|     35 | 1187 | `	return rc;` |
|     17 | 1188 | `}` |
|    309 | 1189 | `static int PH7_vfs_mkdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1190 | `{` |
|    314 | 1191 | `	int iRecursive = 0;` |
|      - | 1192 | `	const char *zPath;` |
|      - | 1193 | `	ph7_vfs *pVfs;` |
|      - | 1194 | `	phl_stream_ctx *pStreamCtx;` |
|    314 | 1195 | `	int iMode,rc,bThrew = 0;` |
|    314 | 1196 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 1197 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1198 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1199 | `		return PH7_OK;` |
|      - | 1200 | `	}` |
|      - | 1201 | ``	/* php's `?resource $context`, refused when it is a resource of another kind,`` |
|      - | 1202 | `	 * and SET on the wrapper instance the write door below dispatches to. */` |
|    314 | 1203 | `	pStreamCtx = PH7_StreamCtxFromArg(pCtx,nArg,apArg,3,"$context",0,&bThrew);` |
|    314 | 1204 | `	if( bThrew ){` |
|      6 | 1205 | `		return PH7_OK;` |
|      - | 1206 | `	}` |
|      - | 1207 | `	/* php's mkdir() hands the wrapper ($url, $mode, $options) -- the mode the caller` |
|      - | 1208 | `	 * asked for (0777 by default on every platform: it is the WRAPPER's to` |
|      - | 1209 | `	 * interpret), and STREAM_REPORT_ERRORS plus STREAM_MKDIR_RECURSIVE. */` |
|      - | 1210 | `	{` |
|      - | 1211 | `		ph7_value sMode,sOpt;` |
|      - | 1212 | `		ph7_value *apExtra[2];` |
|      - | 1213 | `		int bDone;` |
|    310 | 1214 | `		PH7_MemObjInit(pCtx->pVm,&sMode);` |
|    310 | 1215 | `		PH7_MemObjInit(pCtx->pVm,&sOpt);` |
|    310 | 1216 | `		ph7_value_int(&sMode,nArg > 1 ? ph7_value_to_int(apArg[1]) : 0777);` |
|    310 | 1217 | `		ph7_value_int(&sOpt,PH7_STREAM_REPORT_ERRORS` |
|    305 | 1218 | `			\| ((nArg > 2 && ph7_value_to_bool(apArg[2])) ? PH7_STREAM_MKDIR_RECURSIVE : 0));` |
|    310 | 1219 | `		apExtra[0] = &sMode;` |
|    310 | 1220 | `		apExtra[1] = &sOpt;` |
|    457 | 1221 | `		bDone = VfsUserWrite(pCtx,ph7_value_to_string(apArg[0],0),"mkdir",` |
|    147 | 1222 | `			(void *)pStreamCtx,apExtra,2);` |
|    310 | 1223 | `		PH7_MemObjRelease(&sMode);` |
|    310 | 1224 | `		PH7_MemObjRelease(&sOpt);` |
|    310 | 1225 | `		if( bDone ){` |
|     11 | 1226 | `			return PH7_OK;` |
|      - | 1227 | `		}` |
|      - | 1228 | `	}` |
|    300 | 1229 | `	if( VfsBuiltinWrapperRefuses(pCtx,ph7_value_to_string(apArg[0],0),0,VFS_POP_MKDIR) ){` |
|     17 | 1230 | `		return PH7_OK;` |
|      - | 1231 | `	}` |
|      - | 1232 | `	/* Point to the underlying vfs */` |
|    284 | 1233 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    284 | 1234 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 ){` |
|      - | 1235 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1236 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1237 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1238 | `			ph7_function_name(pCtx)` |
|      - | 1239 | `			);` |
|    ! 0 | 1240 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1241 | `		return PH7_OK;` |
|      - | 1242 | `	}` |
|      - | 1243 | `	/* Point to the desired directory */` |
|    284 | 1244 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1245 | `#ifdef __WINNT__` |
|      5 | 1246 | `	iMode = 0;` |
|      - | 1247 | `#else` |
|      - | 1248 | `	/* Assume UNIX */` |
|    279 | 1249 | `	iMode = 0777;` |
|      - | 1250 | `#endif` |
|    284 | 1251 | `	if( nArg > 1 ){` |
|     39 | 1252 | `		iMode = ph7_value_to_int(apArg[1]);` |
|     39 | 1253 | `		if( nArg > 2 ){` |
|     39 | 1254 | `			iRecursive = ph7_value_to_bool(apArg[2]);` |
|     14 | 1255 | `		}` |
|     14 | 1256 | `	}` |
|      - | 1257 | `	/* Perform the requested operation */` |
|    284 | 1258 | `	if( iRecursive ){` |
|     39 | 1259 | `		rc = VfsMkdirRecursive(pCtx,pVfs,zPath,iMode);` |
|     17 | 1260 | `	}else{` |
|    248 | 1261 | `		errno = 0;` |
|    248 | 1262 | `		rc = pVfs->xMkdir(zPath,iMode,0);` |
|    248 | 1263 | `		if( rc != PH7_OK ){` |
|      - | 1264 | `			/* php does NOT name the path for mkdir: "mkdir(): File exists" */` |
|      7 | 1265 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 | 1266 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      2 | 1267 | `		}` |
|      - | 1268 | `	}` |
|      - | 1269 | `	/* IO return value */` |
|    284 | 1270 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    284 | 1271 | `	return PH7_OK;` |
|    154 | 1272 | `}` |
|      - | 1273 | `/*` |
|      - | 1274 | ` * bool rename(string $oldname,string $newname)` |
|      - | 1275 | ` *  Attempts to rename oldname to newname.` |
|      - | 1276 | ` * Parameters` |
|      - | 1277 | ` *  $oldname` |
|      - | 1278 | ` *   Old name.` |
|      - | 1279 | ` *  $newname` |
|      - | 1280 | ` *   New name.` |
|      - | 1281 | ` * Return` |
|      - | 1282 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1283 | ` */` |
|     40 | 1284 | `static int PH7_vfs_rename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1285 | `{` |
|      - | 1286 | `	const char *zOld,*zNew;` |
|      - | 1287 | `	ph7_vfs *pVfs;` |
|      - | 1288 | `	phl_stream_ctx *pStreamCtx;` |
|     42 | 1289 | `	int rc,bThrew = 0;` |
|     42 | 1290 | `	if( nArg < 2 \|\| !VfsPathArgUsable(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 1291 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1292 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1293 | `		return PH7_OK;` |
|      - | 1294 | `	}` |
|      - | 1295 | ``	/* php's `?resource $context`, refused when it is a resource of another kind,`` |
|      - | 1296 | `	 * and SET on the wrapper instance the write door below dispatches to. */` |
|     42 | 1297 | `	pStreamCtx = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|     42 | 1298 | `	if( bThrew ){` |
|      3 | 1299 | `		return PH7_OK;` |
|      - | 1300 | `	}` |
|     40 | 1301 | `	zOld = ph7_value_to_string(apArg[0],0);` |
|     40 | 1302 | `	zNew = ph7_value_to_string(apArg[1],0);` |
|      - | 1303 | `	{` |
|      - | 1304 | `		/* php resolves BOTH ends before renaming anything. A scheme nothing is` |
|      - | 1305 | `		 * registered under is its own reason, reported and then left to the` |
|      - | 1306 | `		 * ordinary rename to fail on; two ends belonging to DIFFERENT wrappers are` |
|      - | 1307 | `		 * refused outright rather than read through one and written to the other. */` |
|     40 | 1308 | `		const char *zProbe = zOld;` |
|     40 | 1309 | `		const ph7_io_stream *pA = PH7_VmGetStreamDevice(pCtx->pVm,&zProbe,(int)SyStrlen(zOld));` |
|      - | 1310 | `		const ph7_io_stream *pB;` |
|     40 | 1311 | `		zProbe = zNew;` |
|     40 | 1312 | `		pB = PH7_VmGetStreamDevice(pCtx->pVm,&zProbe,(int)SyStrlen(zNew));` |
|     40 | 1313 | `		if( pA == 0 \|\| pB == 0 ){` |
|      - | 1314 | `			/* php reports the scheme it could not place and then goes on with the` |
|      - | 1315 | `			 * comparison, the unplaced end counting as the plain-files wrapper --` |
|      - | 1316 | `			 * so a plain path beside it renames (and fails on the path), while a` |
|      - | 1317 | `			 * WRAPPER beside it is the across-types refusal below. */` |
|      3 | 1318 | `			VfsThrowUnknownWrapperWarning(pCtx,pA == 0 ? zOld : zNew);` |
|      3 | 1319 | `			if( pA == 0 ){` |
|    ! 0 | 1320 | `				pA = pCtx->pVm->pDefStream;` |
|    ! 0 | 1321 | `			}` |
|      3 | 1322 | `			if( pB == 0 ){` |
|      3 | 1323 | `				pB = pCtx->pVm->pDefStream;` |
|      1 | 1324 | `			}` |
|      1 | 1325 | `		}` |
|     40 | 1326 | `		if( pA != pB ){` |
|     10 | 1327 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Cannot rename a file across wrapper types",` |
|      3 | 1328 | `				ph7_function_name(pCtx));` |
|      7 | 1329 | `			ph7_result_bool(pCtx,0);` |
|     23 | 1330 | `			return PH7_OK;` |
|    ! 0 | 1331 | `		}else{` |
|      - | 1332 | `			/* One wrapper owns both ends, so the destination is its second` |
|      - | 1333 | `			 * argument -- the full url, as php hands it over. */` |
|      - | 1334 | `			ph7_value sDest;` |
|      - | 1335 | `			ph7_value *apExtra[1];` |
|      - | 1336 | `			int bDone;` |
|     34 | 1337 | `			PH7_MemObjInit(pCtx->pVm,&sDest);` |
|     34 | 1338 | `			ph7_value_string(&sDest,zNew,-1);` |
|     34 | 1339 | `			apExtra[0] = &sDest;` |
|     34 | 1340 | `			bDone = VfsUserWrite(pCtx,zOld,"rename",(void *)pStreamCtx,apExtra,1);` |
|     34 | 1341 | `			PH7_MemObjRelease(&sDest);` |
|     34 | 1342 | `			if( bDone ){` |
|     20 | 1343 | `				return PH7_OK;` |
|      - | 1344 | `			}` |
|     28 | 1345 | `			if( VfsBuiltinWrapperRefuses(pCtx,zOld,zNew,VFS_POP_RENAME) ){` |
|     25 | 1346 | `				return PH7_OK;` |
|      - | 1347 | `			}` |
|      - | 1348 | `		}` |
|      - | 1349 | `	}` |
|      - | 1350 | `	/* Point to the underlying vfs */` |
|      3 | 1351 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 1352 | `	if( pVfs == 0 \|\| pVfs->xRename == 0 ){` |
|      - | 1353 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1354 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1355 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1356 | `			ph7_function_name(pCtx)` |
|      - | 1357 | `			);` |
|    ! 0 | 1358 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1359 | `		return PH7_OK;` |
|      - | 1360 | `	}` |
|      - | 1361 | `	/* Perform the requested operation */` |
|      3 | 1362 | `	errno = 0;` |
|      3 | 1363 | `	rc = pVfs->xRename(zOld,zNew);` |
|      3 | 1364 | `	if( rc != PH7_OK ){` |
|      - | 1365 | `		/* php names BOTH paths here */` |
|    ! 0 | 1366 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s,%s): %s",` |
|    ! 0 | 1367 | `			ph7_function_name(pCtx),zOld,zNew,VfsStrerror(errno));` |
|    ! 0 | 1368 | `	}` |
|      - | 1369 | `	/* IO result */` |
|      3 | 1370 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      3 | 1371 | `	return PH7_OK;` |
|     21 | 1372 | `}` |
|      - | 1373 | `/*` |
|      - | 1374 | ` * string realpath(string $path)` |
|      - | 1375 | ` *  Returns canonicalized absolute pathname.` |
|      - | 1376 | ` * Parameters` |
|      - | 1377 | ` *  $path` |
|      - | 1378 | ` *   Target path.` |
|      - | 1379 | ` * Return` |
|      - | 1380 | ` *  Canonicalized absolute pathname on success. or FALSE on failure.` |
|      - | 1381 | ` */` |
|    450 | 1382 | `static int PH7_vfs_realpath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1383 | `{` |
|      - | 1384 | `	const char *zPath;` |
|      - | 1385 | `	ph7_vfs *pVfs;` |
|      - | 1386 | `        int rc;` |
|    455 | 1387 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 1388 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1389 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1390 | `		return PH7_OK;` |
|      - | 1391 | `	}` |
|      - | 1392 | `	/* Point to the underlying vfs */` |
|    455 | 1393 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    455 | 1394 | `	if( pVfs == 0 \|\| pVfs->xRealpath == 0 ){` |
|      - | 1395 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1396 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1397 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1398 | `			ph7_function_name(pCtx)` |
|      - | 1399 | `			);` |
|    ! 0 | 1400 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1401 | `		return PH7_OK;` |
|      - | 1402 | `	}` |
|      - | 1403 | `	/* Set an empty string untnil the underlying OS interface change that */` |
|    455 | 1404 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1405 | ``	/* Perform the requested operation. php resolves an EMPTY path as `.` and so`` |
|      - | 1406 | `	 * answers the working directory, where this answered false. */` |
|    455 | 1407 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|    455 | 1408 | `	if( zPath == 0 \|\| zPath[0] == 0 ){` |
|      2 | 1409 | `		zPath = ".";` |
|      1 | 1410 | `	}` |
|    455 | 1411 | `	rc = pVfs->xRealpath(zPath,pCtx);` |
|    455 | 1412 | `	if( rc != PH7_OK ){` |
|      2 | 1413 | `	 ph7_result_bool(pCtx,0);` |
|      1 | 1414 | `	}` |
|    455 | 1415 | `	return PH7_OK;` |
|    229 | 1416 | `}` |
|      - | 1417 | `/*` |
|      - | 1418 | ` * Does this candidate name something, and if so what is its canonical path?` |
|      - | 1419 | ` * The existence question is asked separately because xRealpath() writes STRAIGHT` |
|      - | 1420 | ` * into the call's result, so it may only be run on the winner.` |
|      - | 1421 | ` */` |
|     32 | 1422 | `static int VfsResolveTry(ph7_vfs *pVfs,ph7_context *pCtx,const char *zCand)` |
|      1 | 1423 | `{` |
|     33 | 1424 | `	if( pVfs->xFileExists(zCand) != PH7_OK ){` |
|     19 | 1425 | `		return 0;` |
|      - | 1426 | `	}` |
|      - | 1427 | `	/* The VFS APPENDS into the call's result, so make it an empty string first` |
|      - | 1428 | `	 * -- the realpath() builtin beside this one seeds the same way. */` |
|     15 | 1429 | `	ph7_result_string(pCtx,"",0);` |
|     15 | 1430 | `	if( pVfs->xRealpath(zCand,pCtx) == PH7_OK ){` |
|     15 | 1431 | `		return 1;` |
|      - | 1432 | `	}` |
|    ! 0 | 1433 | `	ph7_result_bool(pCtx,0);` |
|    ! 0 | 1434 | `	return 0;` |
|     17 | 1435 | `}` |
|      - | 1436 | `/*` |
|      - | 1437 | ` * Is this an ABSOLUTE path, by php's rule for this platform? A lone leading` |
|      - | 1438 | ` * slash is NOT absolute on Windows -- php walks the include_path for it.` |
|      - | 1439 | ` */` |
|     16 | 1440 | `static int VfsPathIsAbsolute(const char *z,int n)` |
|      1 | 1441 | `{` |
|      - | 1442 | `#ifdef __WINNT__` |
|      1 | 1443 | `	if( n >= 2 && ((z[0] >= 'A' && z[0] <= 'Z') \|\| (z[0] >= 'a' && z[0] <= 'z')) && z[1] == ':' ){` |
|      1 | 1444 | `		return 1;` |
|      - | 1445 | `	}` |
|      1 | 1446 | `	return n >= 2 && (z[0] == '/' \|\| z[0] == '\\') && (z[1] == '/' \|\| z[1] == '\\');` |
|      - | 1447 | `#else` |
|     16 | 1448 | `	return n >= 1 && z[0] == '/';` |
|      - | 1449 | `#endif` |
|      1 | 1450 | `}` |
|      - | 1451 | `/* "./x" and "../x": php reads these against the CWD and never walks the path. */` |
|     20 | 1452 | `static int VfsPathIsDotRelative(const char *z,int n)` |
|      1 | 1453 | `{` |
|      - | 1454 | `#ifdef __WINNT__` |
|      - | 1455 | `#define VFS_RESOLVE_SLASH(c) ((c) == '/' \|\| (c) == '\\')` |
|      - | 1456 | `#else` |
|      - | 1457 | `#define VFS_RESOLVE_SLASH(c) ((c) == '/')` |
|      - | 1458 | `#endif` |
|     21 | 1459 | `	if( n < 2 \|\| z[0] != '.' ){` |
|     17 | 1460 | `		return 0;` |
|      - | 1461 | `	}` |
|      5 | 1462 | `	if( VFS_RESOLVE_SLASH(z[1]) ){` |
|      3 | 1463 | `		return 1;` |
|      - | 1464 | `	}` |
|      3 | 1465 | `	return n > 2 && z[1] == '.' && VFS_RESOLVE_SLASH(z[2]);` |
|     11 | 1466 | `}` |
|      - | 1467 | `/*` |
|      - | 1468 | ` * string\|false stream_resolve_include_path(string $filename)` |
|      - | 1469 | ` *  Where would include/require find this name? php's own php_resolve_path,` |
|      - | 1470 | ` *  which is the ONLY way a script can ask that question without opening` |
|      - | 1471 | ` *  anything -- and the way an autoloader decides whether a class file exists` |
|      - | 1472 | ` *  before requiring it.` |
|      - | 1473 | ` * Return` |
|      - | 1474 | ` *  The canonical path on success, FALSE when nothing answers.` |
|      - | 1475 | ` */` |
|     28 | 1476 | `static int PH7_vfs_stream_resolve_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1477 | `{` |
|     29 | 1478 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1479 | `	ph7_vfs *pVfs;` |
|      - | 1480 | `	const char *zPath;` |
|      - | 1481 | `	SyString *aEntry;` |
|      - | 1482 | `	SyString sDir;` |
|      - | 1483 | `	SyBlob sWorker;` |
|     29 | 1484 | `	int nPath = 0, nScheme, c;` |
|      - | 1485 | `	sxu32 n;` |
|      - | 1486 | `	/* FALSE until something resolves: xRealpath() overwrites it on the winner. */` |
|     29 | 1487 | `	ph7_result_bool(pCtx,0);` |
|     29 | 1488 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     29 | 1489 | `	if( nArg < 1 \|\| pVfs == 0 \|\| pVfs->xRealpath == 0 \|\| pVfs->xFileExists == 0 ){` |
|    ! 0 | 1490 | `		return PH7_OK;` |
|      - | 1491 | `	}` |
|     29 | 1492 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|     29 | 1493 | `	if( nPath < 0 ){` |
|    ! 0 | 1494 | `		nPath = 0;` |
|    ! 0 | 1495 | `	}` |
|     29 | 1496 | `	nScheme = PH7_VmUrlSchemeLen(zPath,nPath);` |
|     29 | 1497 | `	if( nScheme > 0 ){` |
|      - | 1498 | `		/* A name that carries a scheme is never walked. php resolves exactly one` |
|      - | 1499 | `		 * of them -- file://, which it realpaths where it stands -- and answers` |
|      - | 1500 | `		 * false for every other wrapper. An unreachable authority (and any other` |
|      - | 1501 | `		 * scheme) comes back unchanged from the strip, and is false. */` |
|      9 | 1502 | `		const char *zLocal = PH7_VmFileUrlLocalPath(zPath);` |
|      9 | 1503 | `		if( zLocal != zPath ){` |
|      3 | 1504 | `			VfsResolveTry(pVfs,pCtx,zLocal);` |
|      1 | 1505 | `		}` |
|      9 | 1506 | `		return PH7_OK;` |
|      - | 1507 | `	}` |
|     20 | 1508 | `	if( VfsPathIsDotRelative(zPath,nPath) \|\| VfsPathIsAbsolute(zPath,nPath)` |
|     14 | 1509 | `	 \|\| SySetUsed(&pVm->aPaths) < 1 ){` |
|     11 | 1510 | `		VfsResolveTry(pVfs,pCtx,zPath);` |
|     11 | 1511 | `		return PH7_OK;` |
|      - | 1512 | `	}` |
|     11 | 1513 | `	c = '/';` |
|      - | 1514 | `#ifdef __WINNT__` |
|      1 | 1515 | `	c = '\\';` |
|      - | 1516 | `#endif` |
|     11 | 1517 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|     11 | 1518 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|     21 | 1519 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|      - | 1520 | `		SyString sFile;` |
|     17 | 1521 | `		SyStringInitFromBuf(&sFile,zPath,(sxu32)nPath);` |
|     17 | 1522 | `		SyBlobReset(&sWorker);` |
|     17 | 1523 | `		SyBlobFormat(&sWorker,"%z%c%z",&aEntry[n],c,&sFile);` |
|     17 | 1524 | `		if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 1525 | `			continue;` |
|      - | 1526 | `		}` |
|     17 | 1527 | `		if( VfsResolveTry(pVfs,pCtx,(const char *)SyBlobData(&sWorker)) ){` |
|      7 | 1528 | `			SyBlobRelease(&sWorker);` |
|      7 | 1529 | `			return PH7_OK;` |
|      - | 1530 | `		}` |
|      6 | 1531 | `	}` |
|      - | 1532 | `	/* The same last resort the opener uses: the executing file's directory. */` |
|      5 | 1533 | `	if( PH7_VmExecutingDir(pVm,&sDir) ){` |
|      - | 1534 | `		SyString sFile;` |
|      5 | 1535 | `		SyStringInitFromBuf(&sFile,zPath,(sxu32)nPath);` |
|      5 | 1536 | `		SyBlobReset(&sWorker);` |
|      5 | 1537 | `		SyBlobFormat(&sWorker,"%z%c%z",&sDir,c,&sFile);` |
|      5 | 1538 | `		if( SXRET_OK == SyBlobNullAppend(&sWorker) ){` |
|      5 | 1539 | `			VfsResolveTry(pVfs,pCtx,(const char *)SyBlobData(&sWorker));` |
|      2 | 1540 | `		}` |
|      2 | 1541 | `	}` |
|      5 | 1542 | `	SyBlobRelease(&sWorker);` |
|      5 | 1543 | `	return PH7_OK;` |
|     15 | 1544 | `}` |
|      - | 1545 | `/*` |
|      - | 1546 | ` * int sleep(int $seconds)` |
|      - | 1547 | ` *  Delays the program execution for the given number of seconds.` |
|      - | 1548 | ` * Parameters` |
|      - | 1549 | ` *  $seconds` |
|      - | 1550 | ` *   Halt time in seconds.` |
|      - | 1551 | ` * Return` |
|      - | 1552 | ` *  Zero on success or FALSE on failure.` |
|      - | 1553 | ` */` |
|     10 | 1554 | `static int PH7_vfs_sleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1555 | `{` |
|      - | 1556 | `	ph7_vfs *pVfs;` |
|      - | 1557 | `	int rc,nSleep;` |
|     11 | 1558 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - | 1559 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1560 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1561 | `		return PH7_OK;` |
|      - | 1562 | `	}` |
|      - | 1563 | `	/* Point to the underlying vfs */` |
|     11 | 1564 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 1565 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - | 1566 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1567 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1568 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1569 | `			ph7_function_name(pCtx)` |
|      - | 1570 | `			);` |
|    ! 0 | 1571 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1572 | `		return PH7_OK;` |
|      - | 1573 | `	}` |
|      - | 1574 | `	/* Amount to sleep */` |
|     11 | 1575 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|     11 | 1576 | `	if( nSleep < 0 ){` |
|      - | 1577 | `		/* php raises rather than sleeping for an unsigned-wrapped eternity */` |
|      3 | 1578 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1579 | `			"sleep(): Argument #1 ($seconds) must be greater than or equal to 0");` |
|      - | 1580 | `	}` |
|      - | 1581 | `	/* Perform the requested operation (Microseconds) */` |
|      9 | 1582 | `	rc = pVfs->xSleep((unsigned int)(nSleep * SX_USEC_PER_SEC));` |
|      9 | 1583 | `	if( rc != PH7_OK ){` |
|      - | 1584 | `		/* Return FALSE */` |
|    ! 0 | 1585 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1586 | `	}else{` |
|      - | 1587 | `		/* Return zero */` |
|      9 | 1588 | `		ph7_result_int(pCtx,0);` |
|      - | 1589 | `	}` |
|      9 | 1590 | `	return PH7_OK;` |
|      6 | 1591 | `}` |
|      - | 1592 | `/*` |
|      - | 1593 | ` * void usleep(int $micro_seconds)` |
|      - | 1594 | ` *  Delays program execution for the given number of micro seconds.` |
|      - | 1595 | ` * Parameters` |
|      - | 1596 | ` *  $micro_seconds` |
|      - | 1597 | ` *   Halt time in micro seconds. A micro second is one millionth of a second.` |
|      - | 1598 | ` * Return` |
|      - | 1599 | ` *  None.` |
|      - | 1600 | ` */` |
|    128 | 1601 | `static int PH7_vfs_usleep(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1602 | `{` |
|      - | 1603 | `	ph7_vfs *pVfs;` |
|      - | 1604 | `	int nSleep;` |
|    132 | 1605 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|      - | 1606 | `		/* Missing/Invalid argument,return immediately */` |
|    ! 0 | 1607 | `		return PH7_OK;` |
|      - | 1608 | `	}` |
|      - | 1609 | `	/* Point to the underlying vfs */` |
|    132 | 1610 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    132 | 1611 | `	if( pVfs == 0 \|\| pVfs->xSleep == 0 ){` |
|      - | 1612 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1613 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1614 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 1615 | `			ph7_function_name(pCtx)` |
|      - | 1616 | `			);` |
|    ! 0 | 1617 | `		return PH7_OK;` |
|      - | 1618 | `	}` |
|      - | 1619 | `	/* Amount to sleep */` |
|    132 | 1620 | `	nSleep = ph7_value_to_int(apArg[0]);` |
|    132 | 1621 | `	if( nSleep < 0 ){` |
|      - | 1622 | `		/* php raises rather than sleeping for an unsigned-wrapped eternity */` |
|      3 | 1623 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1624 | `			"usleep(): Argument #1 ($microseconds) must be greater than or equal to 0");` |
|      - | 1625 | `	}` |
|      - | 1626 | `	/* Perform the requested operation (Microseconds) */` |
|    130 | 1627 | `	pVfs->xSleep((unsigned int)nSleep);` |
|    130 | 1628 | `	return PH7_OK;` |
|     68 | 1629 | `}` |
|      - | 1630 | `/*` |
|      - | 1631 | ` * ---------------------------------------------------------------------------` |
|      - | 1632 | ` * A path operation over a BUILT-IN wrapper other than the plain-file one.` |
|      - | 1633 | ` *` |
|      - | 1634 | ` * php's unlink/rename/mkdir/rmdir/chmod belong to a WRAPPER, and every built-in` |
|      - | 1635 | ` * wrapper but the plain-file one implements none of them: php answers false and` |
|      - | 1636 | ` * words the refusal from the wrapper's LABEL. PHL sent every one of them` |
|      - | 1637 | ` * straight to the OS, which then failed on a path with a scheme in it -- so` |
|      - | 1638 | `` * `unlink('php://memory')` reported `No such file or directory` where php says`` |
|      - | 1639 | `` * `PHP does not allow unlinking`, and the same for data://, glob://, http:// and`` |
|      - | 1640 | ` * compress.zlib://.` |
|      - | 1641 | ` *` |
|      - | 1642 | ` * The two silent ones are php's too: mkdir() and rmdir() over such a wrapper are` |
|      - | 1643 | ` * a bare false with no diagnostic at all, while unlink() and rename() have a` |
|      - | 1644 | ` * sentence each and the chmod family shares a third.` |
|      - | 1645 | ` *` |
|      - | 1646 | ` * A USERLAND wrapper never reaches here -- its own door (VfsUserWrite) runs` |
|      - | 1647 | ` * first and answers for it, including php's "no such method" refusal.` |
|      - | 1648 | ` * ---------------------------------------------------------------------------` |
|      - | 1649 | ` */` |
|  50308 | 1650 | `static int VfsBuiltinWrapperRefuses(ph7_context *pCtx,const char *zPath,const char *zDest,int eOp)` |
|      5 | 1651 | `{` |
|      - | 1652 | `	const ph7_io_stream *pStream;` |
|  50313 | 1653 | `	const char *zTail = zPath;` |
|      - | 1654 | `	const char *zLabel;` |
|  50313 | 1655 | `	if( zPath == 0 ){` |
|    ! 0 | 1656 | `		return 0;` |
|      - | 1657 | `	}` |
|  50313 | 1658 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zTail,(int)SyStrlen(zPath));` |
|  50313 | 1659 | `	zLabel = PH7_StreamWrapperLabel(pCtx->pVm,pStream);` |
|  50313 | 1660 | `	if( zLabel == 0 ){` |
|  50218 | 1661 | `		return 0;` |
|      - | 1662 | `	}` |
|      - | 1663 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     96 | 1664 | `	if( PH7_PharStreamIs(pStream) && eOp != VFS_POP_CHMOD ){` |
|      - | 1665 | `		/* ext/phar is the one built-in wrapper here that DOES implement the` |
|      - | 1666 | `		 * path operations -- an archive can have an entry deleted, created or` |
|      - | 1667 | `` 		 * renamed -- so it answers for those itself, with its own `phar.readonly` `` |
|      - | 1668 | `		 * refusal. It implements no stream_metadata, so chmod(), chown() and` |
|      - | 1669 | `		 * chgrp() still take the sentence below. */` |
|     67 | 1670 | `		return PH7_PharPathOp(pCtx,zPath,zDest,eOp);` |
|      - | 1671 | `	}` |
|      - | 1672 | `#endif` |
|     29 | 1673 | `	switch( eOp ){` |
|      4 | 1674 | `	case VFS_POP_UNLINK:` |
|     13 | 1675 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s does not allow unlinking",` |
|      4 | 1676 | `			ph7_function_name(pCtx),zLabel);` |
|      9 | 1677 | `		break;` |
|      2 | 1678 | `	case VFS_POP_RENAME:` |
|      7 | 1679 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s wrapper does not support renaming",` |
|      2 | 1680 | `			ph7_function_name(pCtx),zLabel);` |
|      5 | 1681 | `		break;` |
|      6 | 1682 | `	case VFS_POP_CHMOD:` |
|      - | 1683 | `		/* php's sentence names the function TWICE and the wrapper not at all. */` |
|     19 | 1684 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Cannot call %s() for a non-standard stream",` |
|      6 | 1685 | `			ph7_function_name(pCtx),ph7_function_name(pCtx));` |
|     12 | 1686 | `		break;` |
|      2 | 1687 | `	default:` |
|      - | 1688 | `		/* mkdir() and rmdir(): php answers a bare false and says nothing. */` |
|      4 | 1689 | `		break;` |
|      - | 1690 | `	}` |
|     29 | 1691 | `	ph7_result_bool(pCtx,0);` |
|     29 | 1692 | `	return 1;` |
|  25135 | 1693 | `}` |
|      - | 1694 | `/*` |
|      - | 1695 | ` * bool unlink (string $filename)` |
|      - | 1696 | ` *  Delete a file.` |
|      - | 1697 | ` * Parameters` |
|      - | 1698 | ` *  $filename` |
|      - | 1699 | ` *   Path to the file.` |
|      - | 1700 | ` * Return` |
|      - | 1701 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1702 | ` */` |
|  49005 | 1703 | `static int PH7_vfs_unlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1704 | `{` |
|      - | 1705 | `	const char *zPath;` |
|      - | 1706 | `	ph7_vfs *pVfs;` |
|      - | 1707 | `	phl_stream_ctx *pStreamCtx;` |
|  49010 | 1708 | `	int rc,bThrew = 0;` |
|  49010 | 1709 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 1710 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1711 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1712 | `		return PH7_OK;` |
|      - | 1713 | `	}` |
|      - | 1714 | ``	/* php's `?resource $context`, refused when it is a resource of another kind,`` |
|      - | 1715 | `	 * and SET on the wrapper instance the write door below dispatches to. */` |
|  49010 | 1716 | `	pStreamCtx = PH7_StreamCtxFromArg(pCtx,nArg,apArg,1,"$context",0,&bThrew);` |
|  49010 | 1717 | `	if( bThrew ){` |
|      8 | 1718 | `		return PH7_OK;` |
|      - | 1719 | `	}` |
|      - | 1720 | `	/* php's unlink() hands the wrapper the url and nothing else. */` |
|  73494 | 1721 | `	if( VfsUserWrite(pCtx,ph7_value_to_string(apArg[0],0),"unlink",` |
|  24490 | 1722 | `		(void *)pStreamCtx,0,0) ){` |
|      7 | 1723 | `		return PH7_OK;` |
|      - | 1724 | `	}` |
|  48998 | 1725 | `	if( VfsBuiltinWrapperRefuses(pCtx,ph7_value_to_string(apArg[0],0),0,VFS_POP_UNLINK) ){` |
|     24 | 1726 | `		return PH7_OK;` |
|      - | 1727 | `	}` |
|      - | 1728 | `	/* Point to the underlying vfs */` |
|  48975 | 1729 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|  48975 | 1730 | `	if( pVfs == 0 \|\| pVfs->xUnlink == 0 ){` |
|      - | 1731 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1732 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1733 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1734 | `			ph7_function_name(pCtx)` |
|      - | 1735 | `			);` |
|    ! 0 | 1736 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1737 | `		return PH7_OK;` |
|      - | 1738 | `	}` |
|      - | 1739 | `	/* Point to the desired directory */` |
|  48975 | 1740 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1741 | `	/* Perform the requested operation */` |
|  48975 | 1742 | `	errno = 0;` |
|  48975 | 1743 | `	rc = pVfs->xUnlink(zPath);` |
|  48975 | 1744 | `	if( rc != PH7_OK ){` |
|  30095 | 1745 | `		VfsThrowSysWarning(pCtx,zPath);` |
|  15043 | 1746 | `	}` |
|      - | 1747 | `	/* IO return value */` |
|  48975 | 1748 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|  48975 | 1749 | `	return PH7_OK;` |
|  24498 | 1750 | `}` |
|      - | 1751 | `/*` |
|      - | 1752 | ` * bool chmod(string $filename,int $mode)` |
|      - | 1753 | ` *  Attempts to change the mode of the specified file to that given in mode.` |
|      - | 1754 | ` * Parameters` |
|      - | 1755 | ` *  $filename` |
|      - | 1756 | ` *   Path to the file.` |
|      - | 1757 | ` * $mode` |
|      - | 1758 | ` *   Mode (Must be an integer)` |
|      - | 1759 | ` * Return` |
|      - | 1760 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1761 | ` */` |
|    652 | 1762 | `static int PH7_vfs_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1763 | `{` |
|      - | 1764 | `	const char *zPath;` |
|      - | 1765 | `	ph7_vfs *pVfs;` |
|      - | 1766 | `	int iMode;` |
|      - | 1767 | `	int rc;` |
|    657 | 1768 | `	if( nArg < 2 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 1769 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 1770 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1771 | `		return PH7_OK;` |
|      - | 1772 | `	}` |
|      - | 1773 | `	/* php's chmod() is a stream_metadata(STREAM_META_ACCESS) on a wrapper path. */` |
|      - | 1774 | `	{` |
|      - | 1775 | `		ph7_value sOp,sVal;` |
|      - | 1776 | `		ph7_value *apExtra[2];` |
|      - | 1777 | `		int bDone;` |
|    657 | 1778 | `		PH7_MemObjInit(pCtx->pVm,&sOp);` |
|    657 | 1779 | `		PH7_MemObjInit(pCtx->pVm,&sVal);` |
|    657 | 1780 | `		ph7_value_int(&sOp,PH7_STREAM_META_ACCESS);` |
|    657 | 1781 | `		ph7_value_int(&sVal,ph7_value_to_int(apArg[1]));` |
|    657 | 1782 | `		apExtra[0] = &sOp;` |
|    657 | 1783 | `		apExtra[1] = &sVal;` |
|    983 | 1784 | `		bDone = VfsUserWrite(pCtx,ph7_value_to_string(apArg[0],0),"stream_metadata",` |
|    326 | 1785 | `			0,apExtra,2);` |
|    657 | 1786 | `		PH7_MemObjRelease(&sOp);` |
|    657 | 1787 | `		PH7_MemObjRelease(&sVal);` |
|    657 | 1788 | `		if( bDone ){` |
|      7 | 1789 | `			return PH7_OK;` |
|      - | 1790 | `		}` |
|      - | 1791 | `	}` |
|    651 | 1792 | `	if( VfsBuiltinWrapperRefuses(pCtx,ph7_value_to_string(apArg[0],0),0,VFS_POP_CHMOD) ){` |
|      9 | 1793 | `		return PH7_OK;` |
|      - | 1794 | `	}` |
|      - | 1795 | `	/* Point to the underlying vfs */` |
|    643 | 1796 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    643 | 1797 | `	if( pVfs == 0 \|\| pVfs->xChmod == 0 ){` |
|      - | 1798 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 1799 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1800 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 1801 | `			ph7_function_name(pCtx)` |
|      - | 1802 | `			);` |
|    ! 0 | 1803 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1804 | `		return PH7_OK;` |
|      - | 1805 | `	}` |
|      - | 1806 | `	/* Point to the desired directory */` |
|    643 | 1807 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1808 | `	/* Extract the mode */` |
|    643 | 1809 | `	iMode = ph7_value_to_int(apArg[1]);` |
|      - | 1810 | `	/* Perform the requested operation */` |
|    643 | 1811 | `	errno = 0;` |
|    643 | 1812 | `	rc = pVfs->xChmod(zPath,iMode);` |
|    643 | 1813 | `	if( rc != PH7_OK ){` |
|      - | 1814 | `		/* php warns with the C library's own reason and names NO path -- one of` |
|      - | 1815 | `		 * the family's two shapes, the one mkdir() and the link pair share.` |
|      - | 1816 | `		 * PHL answered FALSE in silence. */` |
|      3 | 1817 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      2 | 1818 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|      1 | 1819 | `	}` |
|      - | 1820 | `	/* IO return value */` |
|    643 | 1821 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    643 | 1822 | `	return PH7_OK;` |
|    331 | 1823 | `}` |
|      - | 1824 | `/*` |
|      - | 1825 | ` * bool chown(string $filename,string $user)` |
|      - | 1826 | ` *  Attempts to change the owner of the file filename to user user.` |
|      - | 1827 | ` * Parameters` |
|      - | 1828 | ` *  $filename` |
|      - | 1829 | ` *   Path to the file.` |
|      - | 1830 | ` * $user` |
|      - | 1831 | ` *   Username.` |
|      - | 1832 | ` * Return` |
|      - | 1833 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1834 | ` */` |
|     16 | 1835 | `static int PH7_vfs_chown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1836 | `{` |
|      - | 1837 | `	const char *zPath,*zUser;` |
|      - | 1838 | `	ph7_vfs *pVfs;` |
|      - | 1839 | `	int rc;` |
|     18 | 1840 | `	if( nArg < 2 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 1841 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1842 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1843 | `		return PH7_OK;` |
|      - | 1844 | `	}` |
|      - | 1845 | `	/* php's chown() is a stream_metadata() on a wrapper path, and WHICH verb it is` |
|      - | 1846 | `	 * depends on the argument's TYPE: an integer names the id, anything else the` |
|      - | 1847 | `	 * name. */` |
|      - | 1848 | `	{` |
|      - | 1849 | `		ph7_value sOp,sVal;` |
|      - | 1850 | `		ph7_value *apExtra[2];` |
|      - | 1851 | `		int bDone;` |
|     18 | 1852 | `		int bId = ph7_value_is_int(apArg[1]);` |
|     18 | 1853 | `		PH7_MemObjInit(pCtx->pVm,&sOp);` |
|     18 | 1854 | `		PH7_MemObjInit(pCtx->pVm,&sVal);` |
|     18 | 1855 | `		ph7_value_int(&sOp,bId ? PH7_STREAM_META_OWNER : PH7_STREAM_META_OWNER_NAME);` |
|     18 | 1856 | `		if( bId ){` |
|      9 | 1857 | `			ph7_value_int64(&sVal,ph7_value_to_int64(apArg[1]));` |
|      5 | 1858 | `		}else{` |
|     10 | 1859 | `			int nUser = 0;` |
|     10 | 1860 | `			const char *zVal = ph7_value_to_string(apArg[1],&nUser);` |
|     10 | 1861 | `			ph7_value_string(&sVal,zVal,nUser);` |
|      - | 1862 | `		}` |
|     18 | 1863 | `		apExtra[0] = &sOp;` |
|     18 | 1864 | `		apExtra[1] = &sVal;` |
|     26 | 1865 | `		bDone = VfsUserWrite(pCtx,ph7_value_to_string(apArg[0],0),"stream_metadata",` |
|      8 | 1866 | `			0,apExtra,2);` |
|     18 | 1867 | `		PH7_MemObjRelease(&sOp);` |
|     18 | 1868 | `		PH7_MemObjRelease(&sVal);` |
|     18 | 1869 | `		if( bDone ){` |
|      9 | 1870 | `			return PH7_OK;` |
|      - | 1871 | `		}` |
|      - | 1872 | `	}` |
|     10 | 1873 | `	if( VfsBuiltinWrapperRefuses(pCtx,ph7_value_to_string(apArg[0],0),0,VFS_POP_CHMOD) ){` |
|      3 | 1874 | `		return PH7_OK;` |
|      - | 1875 | `	}` |
|      - | 1876 | `	/* Point to the underlying vfs */` |
|      7 | 1877 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 1878 | `	if( pVfs == 0 \|\| pVfs->xChown == 0 ){` |
|      - | 1879 | `		/* No ownership to change: Windows has neither a uid nor anything to look` |
|      - | 1880 | `		 * one up in, and php's own build there keeps the function and answers a` |
|      - | 1881 | `		 * SILENT false for it -- asked of php 8.5.8 on the gate guest. */` |
|      1 | 1882 | `		ph7_result_bool(pCtx,0);` |
|      1 | 1883 | `		return PH7_OK;` |
|      - | 1884 | `	}` |
|      - | 1885 | `	/* Point to the desired directory */` |
|      6 | 1886 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1887 | `	/* Extract the user */` |
|      6 | 1888 | `	zUser = ph7_value_to_string(apArg[1],0);` |
|      - | 1889 | `	/* Perform the requested operation */` |
|      6 | 1890 | `	errno = 0;` |
|      6 | 1891 | `	rc = pVfs->xChown(zPath,zUser);` |
|      6 | 1892 | `	if( rc != PH7_OK ){` |
|      - | 1893 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - | 1894 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - | 1895 | `		 * "chown(): Operation not permitted". */` |
|      6 | 1896 | `		if( rc == -2 ){` |
|      3 | 1897 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find uid for %s",` |
|      1 | 1898 | `				ph7_function_name(pCtx),zUser);` |
|      1 | 1899 | `		}else{` |
|      6 | 1900 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 | 1901 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - | 1902 | `		}` |
|      3 | 1903 | `	}` |
|      - | 1904 | `	/* IO return value */` |
|      6 | 1905 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 | 1906 | `	return PH7_OK;` |
|     10 | 1907 | `}` |
|      - | 1908 | `/*` |
|      - | 1909 | ` * bool chgrp(string $filename,string $group)` |
|      - | 1910 | ` *  Attempts to change the group of the file filename to group.` |
|      - | 1911 | ` * Parameters` |
|      - | 1912 | ` *  $filename` |
|      - | 1913 | ` *   Path to the file.` |
|      - | 1914 | ` * $group` |
|      - | 1915 | ` *   groupname.` |
|      - | 1916 | ` * Return` |
|      - | 1917 | ` *  TRUE on success or FALSE on failure.` |
|      - | 1918 | ` */` |
|     16 | 1919 | `static int PH7_vfs_chgrp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1920 | `{` |
|      - | 1921 | `	const char *zPath,*zGroup;` |
|      - | 1922 | `	ph7_vfs *pVfs;` |
|      - | 1923 | `	int rc;` |
|     18 | 1924 | `	if( nArg < 2 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 1925 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1926 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1927 | `		return PH7_OK;` |
|      - | 1928 | `	}` |
|      - | 1929 | `	/* php's chgrp() is a stream_metadata() on a wrapper path, and WHICH verb it is` |
|      - | 1930 | `	 * depends on the argument's TYPE: an integer names the id, anything else the` |
|      - | 1931 | `	 * name. */` |
|      - | 1932 | `	{` |
|      - | 1933 | `		ph7_value sOp,sVal;` |
|      - | 1934 | `		ph7_value *apExtra[2];` |
|      - | 1935 | `		int bDone;` |
|     18 | 1936 | `		int bId = ph7_value_is_int(apArg[1]);` |
|     18 | 1937 | `		PH7_MemObjInit(pCtx->pVm,&sOp);` |
|     18 | 1938 | `		PH7_MemObjInit(pCtx->pVm,&sVal);` |
|     18 | 1939 | `		ph7_value_int(&sOp,bId ? PH7_STREAM_META_GROUP : PH7_STREAM_META_GROUP_NAME);` |
|     18 | 1940 | `		if( bId ){` |
|      9 | 1941 | `			ph7_value_int64(&sVal,ph7_value_to_int64(apArg[1]));` |
|      5 | 1942 | `		}else{` |
|     10 | 1943 | `			int nUser = 0;` |
|     10 | 1944 | `			const char *zVal = ph7_value_to_string(apArg[1],&nUser);` |
|     10 | 1945 | `			ph7_value_string(&sVal,zVal,nUser);` |
|      - | 1946 | `		}` |
|     18 | 1947 | `		apExtra[0] = &sOp;` |
|     18 | 1948 | `		apExtra[1] = &sVal;` |
|     26 | 1949 | `		bDone = VfsUserWrite(pCtx,ph7_value_to_string(apArg[0],0),"stream_metadata",` |
|      8 | 1950 | `			0,apExtra,2);` |
|     18 | 1951 | `		PH7_MemObjRelease(&sOp);` |
|     18 | 1952 | `		PH7_MemObjRelease(&sVal);` |
|     18 | 1953 | `		if( bDone ){` |
|      9 | 1954 | `			return PH7_OK;` |
|      - | 1955 | `		}` |
|      - | 1956 | `	}` |
|     10 | 1957 | `	if( VfsBuiltinWrapperRefuses(pCtx,ph7_value_to_string(apArg[0],0),0,VFS_POP_CHMOD) ){` |
|      3 | 1958 | `		return PH7_OK;` |
|      - | 1959 | `	}` |
|      - | 1960 | `	/* Point to the underlying vfs */` |
|      7 | 1961 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 1962 | `	if( pVfs == 0 \|\| pVfs->xChgrp == 0 ){` |
|      - | 1963 | `		/* No ownership to change: Windows has neither a gid nor anything to look` |
|      - | 1964 | `		 * one up in, and php's own build there keeps the function and answers a` |
|      - | 1965 | `		 * SILENT false for it -- asked of php 8.5.8 on the gate guest. */` |
|      1 | 1966 | `		ph7_result_bool(pCtx,0);` |
|      1 | 1967 | `		return PH7_OK;` |
|      - | 1968 | `	}` |
|      - | 1969 | `	/* Point to the desired directory */` |
|      6 | 1970 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 1971 | `	/* Extract the user */` |
|      6 | 1972 | `	zGroup = ph7_value_to_string(apArg[1],0);` |
|      - | 1973 | `	/* Perform the requested operation */` |
|      6 | 1974 | `	errno = 0;` |
|      6 | 1975 | `	rc = pVfs->xChgrp(zPath,zGroup);` |
|      6 | 1976 | `	if( rc != PH7_OK ){` |
|      - | 1977 | `		/* php words a failed NAME lookup differently from a failed syscall, and names no` |
|      - | 1978 | `		 * path in either: "chown(): Unable to find uid for bogus" vs` |
|      - | 1979 | `		 * "chown(): Operation not permitted". */` |
|      6 | 1980 | `		if( rc == -2 ){` |
|      3 | 1981 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to find gid for %s",` |
|      1 | 1982 | `				ph7_function_name(pCtx),zGroup);` |
|      1 | 1983 | `		}else{` |
|      6 | 1984 | `			PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 | 1985 | `				ph7_function_name(pCtx),VfsStrerror(errno));` |
|      - | 1986 | `		}` |
|      3 | 1987 | `	}` |
|      - | 1988 | `	/* IO return value */` |
|      6 | 1989 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      6 | 1990 | `	return PH7_OK;` |
|     10 | 1991 | `}` |
|      - | 1992 | `/*` |
|      - | 1993 | ` * int64 disk_free_space(string $directory)` |
|      - | 1994 | ` *  Returns available space on filesystem or disk partition.` |
|      - | 1995 | ` * Parameters` |
|      - | 1996 | ` *  $directory` |
|      - | 1997 | ` *   A directory of the filesystem or disk partition.` |
|      - | 1998 | ` * Return` |
|      - | 1999 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - | 2000 | ` */` |
|     16 | 2001 | `static int PH7_vfs_disk_free_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2002 | `{` |
|      - | 2003 | `	const char *zPath;` |
|      - | 2004 | `	ph7_int64 iSize;` |
|      - | 2005 | `	ph7_vfs *pVfs;` |
|     17 | 2006 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2007 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2008 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2009 | `		return PH7_OK;` |
|      - | 2010 | `	}` |
|      - | 2011 | `	/* Point to the underlying vfs */` |
|     17 | 2012 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     17 | 2013 | `	if( pVfs == 0 \|\| pVfs->xFreeSpace == 0 ){` |
|      - | 2014 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2015 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2016 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2017 | `			ph7_function_name(pCtx)` |
|      - | 2018 | `			);` |
|    ! 0 | 2019 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2020 | `		return PH7_OK;` |
|      - | 2021 | `	}` |
|      - | 2022 | `	/* Point to the desired directory */` |
|     17 | 2023 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|     17 | 2024 | `	if( zPath == 0 \|\| zPath[0] == 0 ){` |
|      - | 2025 | `		/* php answers FALSE for an empty one and says nothing, as the stat` |
|      - | 2026 | `		 * family does. */` |
|      2 | 2027 | `		ph7_result_bool(pCtx,0);` |
|      2 | 2028 | `		return PH7_OK;` |
|      - | 2029 | `	}` |
|      - | 2030 | `	/* Perform the requested operation */` |
|     15 | 2031 | `	errno = 0;` |
|     15 | 2032 | `	iSize = pVfs->xFreeSpace(zPath);` |
|     15 | 2033 | `	if( iSize < 0 ){` |
|      - | 2034 | `		/* php answers FALSE and warns with the C library's own reason` |
|      - | 2035 | ``		 * (`disk_free_space(): No such file or directory`). PHL answered int(-1) --`` |
|      - | 2036 | `		 * truthy, and a plausible-looking byte count for a caller that only tests` |
|      - | 2037 | ``		 * `if ($free)` or compares it against a threshold. */`` |
|      7 | 2038 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 | 2039 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|      5 | 2040 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2041 | `		return PH7_OK;` |
|      - | 2042 | `	}` |
|      - | 2043 | `	/* php's return type is FLOAT, not int: the value can exceed what an int holds` |
|      - | 2044 | ``	 * on a large volume, and a `===`/gettype()/json_encode() reader sees the`` |
|      - | 2045 | `	 * difference on every volume. */` |
|     11 | 2046 | `	ph7_result_double(pCtx,(double)iSize);` |
|     11 | 2047 | `	return PH7_OK;` |
|      9 | 2048 | `}` |
|      - | 2049 | `/*` |
|      - | 2050 | ` * int64 disk_total_space(string $directory)` |
|      - | 2051 | ` *  Returns the total size of a filesystem or disk partition.` |
|      - | 2052 | ` * Parameters` |
|      - | 2053 | ` *  $directory` |
|      - | 2054 | ` *   A directory of the filesystem or disk partition.` |
|      - | 2055 | ` * Return` |
|      - | 2056 | ` *  Returns the number of available bytes as a 64-bit integer or FALSE on failure.` |
|      - | 2057 | ` */` |
|     10 | 2058 | `static int PH7_vfs_disk_total_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2059 | `{` |
|      - | 2060 | `	const char *zPath;` |
|      - | 2061 | `	ph7_int64 iSize;` |
|      - | 2062 | `	ph7_vfs *pVfs;` |
|     11 | 2063 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2064 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2065 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2066 | `		return PH7_OK;` |
|      - | 2067 | `	}` |
|      - | 2068 | `	/* Point to the underlying vfs */` |
|     11 | 2069 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 2070 | `	if( pVfs == 0 \|\| pVfs->xTotalSpace == 0 ){` |
|      - | 2071 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2072 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2073 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2074 | `			ph7_function_name(pCtx)` |
|      - | 2075 | `			);` |
|    ! 0 | 2076 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2077 | `		return PH7_OK;` |
|      - | 2078 | `	}` |
|      - | 2079 | `	/* Point to the desired directory */` |
|     11 | 2080 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2081 | `	/* Perform the requested operation */` |
|     11 | 2082 | `	errno = 0;` |
|     11 | 2083 | `	iSize = pVfs->xTotalSpace(zPath);` |
|     11 | 2084 | `	if( iSize < 0 ){` |
|      - | 2085 | `		/* php answers FALSE and warns with the C library's own reason` |
|      - | 2086 | ``		 * (`disk_free_space(): No such file or directory`). PHL answered int(-1) --`` |
|      - | 2087 | `		 * truthy, and a plausible-looking byte count for a caller that only tests` |
|      - | 2088 | ``		 * `if ($free)` or compares it against a threshold. */`` |
|      4 | 2089 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      2 | 2090 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|      3 | 2091 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2092 | `		return PH7_OK;` |
|      - | 2093 | `	}` |
|      - | 2094 | `	/* php's return type is FLOAT, not int: the value can exceed what an int holds` |
|      - | 2095 | ``	 * on a large volume, and a `===`/gettype()/json_encode() reader sees the`` |
|      - | 2096 | `	 * difference on every volume. */` |
|      9 | 2097 | `	ph7_result_double(pCtx,(double)iSize);` |
|      9 | 2098 | `	return PH7_OK;` |
|      6 | 2099 | `}` |
|      - | 2100 | `/*` |
|      - | 2101 | ` * bool file_exists(string $filename)` |
|      - | 2102 | ` *  Checks whether a file or directory exists.` |
|      - | 2103 | ` * Parameters` |
|      - | 2104 | ` *  $filename` |
|      - | 2105 | ` *   Path to the file.` |
|      - | 2106 | ` * Return` |
|      - | 2107 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2108 | ` */` |
|    952 | 2109 | `static int PH7_vfs_file_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2110 | `{` |
|      - | 2111 | `	const char *zPath;` |
|      - | 2112 | `	ph7_vfs *pVfs;` |
|      - | 2113 | `	int rc;` |
|    957 | 2114 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2115 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2116 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2117 | `		return PH7_OK;` |
|      - | 2118 | `	}` |
|      - | 2119 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2120 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|    957 | 2121 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_EXISTS) ){` |
|     25 | 2122 | `		return PH7_OK;` |
|      - | 2123 | `	}` |
|    933 | 2124 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    933 | 2125 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0 ){` |
|      - | 2126 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2127 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2128 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2129 | `			ph7_function_name(pCtx)` |
|      - | 2130 | `			);` |
|    ! 0 | 2131 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2132 | `		return PH7_OK;` |
|      - | 2133 | `	}` |
|      - | 2134 | `	/* Point to the desired directory */` |
|    933 | 2135 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2136 | `	/* Perform the requested operation */` |
|    933 | 2137 | `	rc = pVfs->xFileExists(zPath);` |
|      - | 2138 | `	/* IO return value */` |
|    933 | 2139 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    933 | 2140 | `	return PH7_OK;` |
|    480 | 2141 | `}` |
|      - | 2142 | `/*` |
|      - | 2143 | ` * int64 file_size(string $filename)` |
|      - | 2144 | ` *  Gets the size for the given file.` |
|      - | 2145 | ` * Parameters` |
|      - | 2146 | ` *  $filename` |
|      - | 2147 | ` *   Path to the file.` |
|      - | 2148 | ` * Return` |
|      - | 2149 | ` *  File size on success or FALSE on failure.` |
|      - | 2150 | ` */` |
|     62 | 2151 | `static int PH7_vfs_file_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2152 | `{` |
|      - | 2153 | `	const char *zPath;` |
|      - | 2154 | `	ph7_int64 iSize;` |
|      - | 2155 | `	ph7_vfs *pVfs;` |
|     66 | 2156 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2157 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2158 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2159 | `		return PH7_OK;` |
|      - | 2160 | `	}` |
|      - | 2161 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2162 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     66 | 2163 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_SIZE) ){` |
|     23 | 2164 | `		return PH7_OK;` |
|      - | 2165 | `	}` |
|     44 | 2166 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     44 | 2167 | `	if( pVfs == 0 \|\| pVfs->xFileSize == 0 ){` |
|      - | 2168 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2169 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2170 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2171 | `			ph7_function_name(pCtx)` |
|      - | 2172 | `			);` |
|    ! 0 | 2173 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2174 | `		return PH7_OK;` |
|      - | 2175 | `	}` |
|      - | 2176 | `	/* Point to the desired directory */` |
|     44 | 2177 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2178 | `	/* Perform the requested operation */` |
|     44 | 2179 | `	iSize = pVfs->xFileSize(zPath);` |
|     44 | 2180 | `	if( iSize < 0 ){` |
|      - | 2181 | `		/* php: a stat failure warns and returns FALSE -- PH7 returned int(-1), which is` |
|      - | 2182 | `		 * truthy and compares equal to nothing a caller would test for. An EMPTY` |
|      - | 2183 | `		 * path is the family's silence (VfsThrowStatWarning). */` |
|      8 | 2184 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      8 | 2185 | `		ph7_result_bool(pCtx,0);` |
|      8 | 2186 | `		return PH7_OK;` |
|      - | 2187 | `	}` |
|      - | 2188 | `	/* IO return value */` |
|     38 | 2189 | `	ph7_result_int64(pCtx,iSize);` |
|     38 | 2190 | `	return PH7_OK;` |
|     35 | 2191 | `}` |
|      - | 2192 | `/*` |
|      - | 2193 | ` * int64 fileatime(string $filename)` |
|      - | 2194 | ` *  Gets the last access time of the given file.` |
|      - | 2195 | ` * Parameters` |
|      - | 2196 | ` *  $filename` |
|      - | 2197 | ` *   Path to the file.` |
|      - | 2198 | ` * Return` |
|      - | 2199 | ` *  File atime on success or FALSE on failure.` |
|      - | 2200 | ` */` |
|     26 | 2201 | `static int PH7_vfs_file_atime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2202 | `{` |
|      - | 2203 | `	const char *zPath;` |
|      - | 2204 | `	ph7_int64 iTime;` |
|      - | 2205 | `	ph7_vfs *pVfs;` |
|     27 | 2206 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2207 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2208 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2209 | `		return PH7_OK;` |
|      - | 2210 | `	}` |
|      - | 2211 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2212 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     27 | 2213 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_ATIME) ){` |
|     17 | 2214 | `		return PH7_OK;` |
|      - | 2215 | `	}` |
|     11 | 2216 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 2217 | `	if( pVfs == 0 \|\| pVfs->xFileAtime == 0 ){` |
|      - | 2218 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2219 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2220 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2221 | `			ph7_function_name(pCtx)` |
|      - | 2222 | `			);` |
|    ! 0 | 2223 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2224 | `		return PH7_OK;` |
|      - | 2225 | `	}` |
|      - | 2226 | `	/* Point to the desired directory */` |
|     11 | 2227 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2228 | `	/* Perform the requested operation */` |
|     11 | 2229 | `	iTime = pVfs->xFileAtime(zPath);` |
|     11 | 2230 | `	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){` |
|      - | 2231 | `		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --` |
|      - | 2232 | `		 * truthy, and one second before the epoch is also a real timestamp, which` |
|      - | 2233 | `		 * is why the verdict comes from the VFS rather than from the value. */` |
|      5 | 2234 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      5 | 2235 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2236 | `		return PH7_OK;` |
|      - | 2237 | `	}` |
|      - | 2238 | `	/* IO return value */` |
|      7 | 2239 | `	ph7_result_int64(pCtx,iTime);` |
|      7 | 2240 | `	return PH7_OK;` |
|     14 | 2241 | `}` |
|      - | 2242 | `/*` |
|      - | 2243 | ` * int64 filemtime(string $filename)` |
|      - | 2244 | ` *  Gets file modification time.` |
|      - | 2245 | ` * Parameters` |
|      - | 2246 | ` *  $filename` |
|      - | 2247 | ` *   Path to the file.` |
|      - | 2248 | ` * Return` |
|      - | 2249 | ` *  File mtime on success or FALSE on failure.` |
|      - | 2250 | ` */` |
|     48 | 2251 | `static int PH7_vfs_file_mtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2252 | `{` |
|      - | 2253 | `	const char *zPath;` |
|      - | 2254 | `	ph7_int64 iTime;` |
|      - | 2255 | `	ph7_vfs *pVfs;` |
|     52 | 2256 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2257 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2258 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2259 | `		return PH7_OK;` |
|      - | 2260 | `	}` |
|      - | 2261 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2262 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     52 | 2263 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_MTIME) ){` |
|     17 | 2264 | `		return PH7_OK;` |
|      - | 2265 | `	}` |
|     36 | 2266 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     36 | 2267 | `	if( pVfs == 0 \|\| pVfs->xFileMtime == 0 ){` |
|      - | 2268 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2269 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2270 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2271 | `			ph7_function_name(pCtx)` |
|      - | 2272 | `			);` |
|    ! 0 | 2273 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2274 | `		return PH7_OK;` |
|      - | 2275 | `	}` |
|      - | 2276 | `	/* Point to the desired directory */` |
|     36 | 2277 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2278 | `	/* Perform the requested operation */` |
|     36 | 2279 | `	iTime = pVfs->xFileMtime(zPath);` |
|     36 | 2280 | `	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){` |
|      - | 2281 | `		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --` |
|      - | 2282 | `		 * truthy, and one second before the epoch is also a real timestamp, which` |
|      - | 2283 | `		 * is why the verdict comes from the VFS rather than from the value. */` |
|      3 | 2284 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      3 | 2285 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2286 | `		return PH7_OK;` |
|      - | 2287 | `	}` |
|      - | 2288 | `	/* IO return value */` |
|     34 | 2289 | `	ph7_result_int64(pCtx,iTime);` |
|     34 | 2290 | `	return PH7_OK;` |
|     28 | 2291 | `}` |
|      - | 2292 | `/*` |
|      - | 2293 | ` * int64 filectime(string $filename)` |
|      - | 2294 | ` *  Gets inode change time of file.` |
|      - | 2295 | ` * Parameters` |
|      - | 2296 | ` *  $filename` |
|      - | 2297 | ` *   Path to the file.` |
|      - | 2298 | ` * Return` |
|      - | 2299 | ` *  File ctime on success or FALSE on failure.` |
|      - | 2300 | ` */` |
|     22 | 2301 | `static int PH7_vfs_file_ctime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2302 | `{` |
|      - | 2303 | `	const char *zPath;` |
|      - | 2304 | `	ph7_int64 iTime;` |
|      - | 2305 | `	ph7_vfs *pVfs;` |
|     23 | 2306 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2307 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2308 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2309 | `		return PH7_OK;` |
|      - | 2310 | `	}` |
|      - | 2311 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2312 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     23 | 2313 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_CTIME) ){` |
|     17 | 2314 | `		return PH7_OK;` |
|      - | 2315 | `	}` |
|      7 | 2316 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 2317 | `	if( pVfs == 0 \|\| pVfs->xFileCtime == 0 ){` |
|      - | 2318 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2319 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2320 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2321 | `			ph7_function_name(pCtx)` |
|      - | 2322 | `			);` |
|    ! 0 | 2323 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2324 | `		return PH7_OK;` |
|      - | 2325 | `	}` |
|      - | 2326 | `	/* Point to the desired directory */` |
|      7 | 2327 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2328 | `	/* Perform the requested operation */` |
|      7 | 2329 | `	iTime = pVfs->xFileCtime(zPath);` |
|      7 | 2330 | `	if( iTime < 0 && !VfsPathStatable(pVfs,zPath) ){` |
|      - | 2331 | `		/* php: a stat failure warns and answers FALSE. PH7 answered int(-1) --` |
|      - | 2332 | `		 * truthy, and one second before the epoch is also a real timestamp, which` |
|      - | 2333 | `		 * is why the verdict comes from the VFS rather than from the value. */` |
|      3 | 2334 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      3 | 2335 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2336 | `		return PH7_OK;` |
|      - | 2337 | `	}` |
|      - | 2338 | `	/* IO return value */` |
|      5 | 2339 | `	ph7_result_int64(pCtx,iTime);` |
|      5 | 2340 | `	return PH7_OK;` |
|     12 | 2341 | `}` |
|      - | 2342 | `/*` |
|      - | 2343 | ` * bool is_file(string $filename)` |
|      - | 2344 | ` *  Tells whether the filename is a regular file.` |
|      - | 2345 | ` * Parameters` |
|      - | 2346 | ` *  $filename` |
|      - | 2347 | ` *   Path to the file.` |
|      - | 2348 | ` * Return` |
|      - | 2349 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2350 | ` */` |
|   9742 | 2351 | `static int PH7_vfs_is_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2352 | `{` |
|      - | 2353 | `	const char *zPath;` |
|      - | 2354 | `	ph7_vfs *pVfs;` |
|      - | 2355 | `	int rc;` |
|   9747 | 2356 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2357 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2358 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2359 | `		return PH7_OK;` |
|      - | 2360 | `	}` |
|      - | 2361 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2362 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|   9747 | 2363 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_IS_FILE) ){` |
|     23 | 2364 | `		return PH7_OK;` |
|      - | 2365 | `	}` |
|   9725 | 2366 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   9725 | 2367 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|      - | 2368 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2369 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2370 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2371 | `			ph7_function_name(pCtx)` |
|      - | 2372 | `			);` |
|    ! 0 | 2373 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2374 | `		return PH7_OK;` |
|      - | 2375 | `	}` |
|      - | 2376 | `	/* Point to the desired directory */` |
|   9725 | 2377 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2378 | `	/* Perform the requested operation */` |
|   9725 | 2379 | `	rc = pVfs->xIsfile(zPath);` |
|      - | 2380 | `	/* IO return value */` |
|   9725 | 2381 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|   9725 | 2382 | `	return PH7_OK;` |
|   4876 | 2383 | `}` |
|      - | 2384 | `/*` |
|      - | 2385 | ` * bool is_link(string $filename)` |
|      - | 2386 | ` *  Tells whether the filename is a symbolic link.` |
|      - | 2387 | ` * Parameters` |
|      - | 2388 | ` *  $filename` |
|      - | 2389 | ` *   Path to the file.` |
|      - | 2390 | ` * Return` |
|      - | 2391 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2392 | ` */` |
|     30 | 2393 | `static int PH7_vfs_is_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2394 | `{` |
|      - | 2395 | `	const char *zPath;` |
|      - | 2396 | `	ph7_vfs *pVfs;` |
|      - | 2397 | `	int rc;` |
|     31 | 2398 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2399 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2400 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2401 | `		return PH7_OK;` |
|      - | 2402 | `	}` |
|      - | 2403 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2404 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     31 | 2405 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_IS_LINK) ){` |
|     19 | 2406 | `		return PH7_OK;` |
|      - | 2407 | `	}` |
|     12 | 2408 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     12 | 2409 | `	if( pVfs == 0 \|\| pVfs->xIslink == 0 ){` |
|      - | 2410 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2411 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2412 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2413 | `			ph7_function_name(pCtx)` |
|      - | 2414 | `			);` |
|    ! 0 | 2415 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2416 | `		return PH7_OK;` |
|      - | 2417 | `	}` |
|      - | 2418 | `	/* Point to the desired directory */` |
|     12 | 2419 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2420 | `	/* Perform the requested operation */` |
|     12 | 2421 | `	rc = pVfs->xIslink(zPath);` |
|      - | 2422 | `	/* IO return value */` |
|     12 | 2423 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     12 | 2424 | `	return PH7_OK;` |
|     16 | 2425 | `}` |
|      - | 2426 | `/*` |
|      - | 2427 | ` * bool is_readable(string $filename)` |
|      - | 2428 | ` *  Tells whether a file exists and is readable.` |
|      - | 2429 | ` * Parameters` |
|      - | 2430 | ` *  $filename` |
|      - | 2431 | ` *   Path to the file.` |
|      - | 2432 | ` * Return` |
|      - | 2433 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2434 | ` */` |
|     18 | 2435 | `static int PH7_vfs_is_readable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2436 | `{` |
|      - | 2437 | `	const char *zPath;` |
|      - | 2438 | `	ph7_vfs *pVfs;` |
|      - | 2439 | `	int rc;` |
|     19 | 2440 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2441 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2442 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2443 | `		return PH7_OK;` |
|      - | 2444 | `	}` |
|      - | 2445 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2446 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     19 | 2447 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_IS_R) ){` |
|     17 | 2448 | `		return PH7_OK;` |
|      - | 2449 | `	}` |
|      3 | 2450 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 2451 | `	if( pVfs == 0 \|\| pVfs->xReadable == 0 ){` |
|      - | 2452 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2453 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2454 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2455 | `			ph7_function_name(pCtx)` |
|      - | 2456 | `			);` |
|    ! 0 | 2457 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2458 | `		return PH7_OK;` |
|      - | 2459 | `	}` |
|      - | 2460 | `	/* Point to the desired directory */` |
|      3 | 2461 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2462 | `	/* Perform the requested operation */` |
|      3 | 2463 | `	rc = pVfs->xReadable(zPath);` |
|      - | 2464 | `	/* IO return value */` |
|      3 | 2465 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      3 | 2466 | `	return PH7_OK;` |
|     10 | 2467 | `}` |
|      - | 2468 | `/*` |
|      - | 2469 | ` * bool is_writable(string $filename)` |
|      - | 2470 | ` *  Tells whether the filename is writable.` |
|      - | 2471 | ` * Parameters` |
|      - | 2472 | ` *  $filename` |
|      - | 2473 | ` *   Path to the file.` |
|      - | 2474 | ` * Return` |
|      - | 2475 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2476 | ` */` |
|    606 | 2477 | `static int PH7_vfs_is_writable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2478 | `{` |
|      - | 2479 | `	const char *zPath;` |
|      - | 2480 | `	ph7_vfs *pVfs;` |
|      - | 2481 | `	int rc;` |
|    611 | 2482 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2483 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2484 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2485 | `		return PH7_OK;` |
|      - | 2486 | `	}` |
|      - | 2487 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2488 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|    611 | 2489 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_IS_W) ){` |
|     17 | 2490 | `		return PH7_OK;` |
|      - | 2491 | `	}` |
|    595 | 2492 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    595 | 2493 | `	if( pVfs == 0 \|\| pVfs->xWritable == 0 ){` |
|      - | 2494 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2495 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2496 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2497 | `			ph7_function_name(pCtx)` |
|      - | 2498 | `			);` |
|    ! 0 | 2499 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2500 | `		return PH7_OK;` |
|      - | 2501 | `	}` |
|      - | 2502 | `	/* Point to the desired directory */` |
|    595 | 2503 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2504 | `	/* Perform the requested operation */` |
|    595 | 2505 | `	rc = pVfs->xWritable(zPath);` |
|      - | 2506 | `	/* IO return value */` |
|    595 | 2507 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    595 | 2508 | `	return PH7_OK;` |
|    308 | 2509 | `}` |
|      - | 2510 | `/*` |
|      - | 2511 | ` * bool is_executable(string $filename)` |
|      - | 2512 | ` *  Tells whether the filename is executable.` |
|      - | 2513 | ` * Parameters` |
|      - | 2514 | ` *  $filename` |
|      - | 2515 | ` *   Path to the file.` |
|      - | 2516 | ` * Return` |
|      - | 2517 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2518 | ` */` |
|     18 | 2519 | `static int PH7_vfs_is_executable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2520 | `{` |
|      - | 2521 | `	const char *zPath;` |
|      - | 2522 | `	ph7_vfs *pVfs;` |
|      - | 2523 | `	int rc;` |
|     20 | 2524 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2525 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2526 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2527 | `		return PH7_OK;` |
|      - | 2528 | `	}` |
|      - | 2529 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2530 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     20 | 2531 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_IS_X) ){` |
|     17 | 2532 | `		return PH7_OK;` |
|      - | 2533 | `	}` |
|      4 | 2534 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      4 | 2535 | `	if( pVfs == 0 \|\| pVfs->xExecutable == 0 ){` |
|      - | 2536 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2537 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2538 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2539 | `			ph7_function_name(pCtx)` |
|      - | 2540 | `			);` |
|    ! 0 | 2541 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2542 | `		return PH7_OK;` |
|      - | 2543 | `	}` |
|      - | 2544 | `	/* Point to the desired directory */` |
|      4 | 2545 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2546 | `	/* Perform the requested operation */` |
|      4 | 2547 | `	rc = pVfs->xExecutable(zPath);` |
|      - | 2548 | `	/* IO return value */` |
|      4 | 2549 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      4 | 2550 | `	return PH7_OK;` |
|     11 | 2551 | `}` |
|      - | 2552 | `/*` |
|      - | 2553 | ` * string filetype(string $filename)` |
|      - | 2554 | ` *  Gets file type.` |
|      - | 2555 | ` * Parameters` |
|      - | 2556 | ` *  $filename` |
|      - | 2557 | ` *   Path to the file.` |
|      - | 2558 | ` * Return` |
|      - | 2559 | ` *  The type of the file. Possible values are fifo, char, dir, block, link` |
|      - | 2560 | ` *  file, socket and unknown.` |
|      - | 2561 | ` */` |
|     38 | 2562 | `static int PH7_vfs_filetype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2563 | `{` |
|      - | 2564 | `	const char *zPath;` |
|      - | 2565 | `	ph7_vfs *pVfs;` |
|     39 | 2566 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2567 | `		/* Missing/Invalid argument,return 'unknown' */` |
|    ! 0 | 2568 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 2569 | `		return PH7_OK;` |
|      - | 2570 | `	}` |
|      - | 2571 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2572 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     39 | 2573 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_TYPE) ){` |
|     19 | 2574 | `		return PH7_OK;` |
|      - | 2575 | `	}` |
|     21 | 2576 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     21 | 2577 | `	if( pVfs == 0 \|\| pVfs->xFiletype == 0 ){` |
|      - | 2578 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2579 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2580 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2581 | `			ph7_function_name(pCtx)` |
|      - | 2582 | `			);` |
|    ! 0 | 2583 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2584 | `		return PH7_OK;` |
|      - | 2585 | `	}` |
|      - | 2586 | `	/* Point to the desired directory */` |
|     21 | 2587 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2588 | `	/* Set the empty string as the default return value */` |
|     21 | 2589 | `	ph7_result_string(pCtx,"",0);` |
|      - | 2590 | `	/* Perform the requested operation */` |
|     21 | 2591 | `	if( pVfs->xFiletype(zPath,pCtx) != PH7_OK ){` |
|      - | 2592 | `		/* php LSTATs here (which is why a symlink answers "link") and a failure is` |
|      - | 2593 | ``		 * the `Lstat failed for` warning plus FALSE. PHL answered the string`` |
|      - | 2594 | `		 * "unknown" -- a real return value of this function, so a caller could not` |
|      - | 2595 | `		 * tell a missing path from a socket or a fifo. */` |
|      5 | 2596 | `		VfsThrowStatWarning(pCtx,zPath,1);` |
|      5 | 2597 | `		ph7_result_bool(pCtx,0);` |
|      2 | 2598 | `	}` |
|     21 | 2599 | `	return PH7_OK;` |
|     20 | 2600 | `}` |
|      - | 2601 | `/*` |
|      - | 2602 | ` * array stat(string $filename)` |
|      - | 2603 | ` *  Gives information about a file.` |
|      - | 2604 | ` * Parameters` |
|      - | 2605 | ` *  $filename` |
|      - | 2606 | ` *   Path to the file.` |
|      - | 2607 | ` * Return` |
|      - | 2608 | ` *  An associative array on success holding the following entries on success` |
|      - | 2609 | ` *  0   dev     device number` |
|      - | 2610 | ` * 1    ino     inode number (zero on windows)` |
|      - | 2611 | ` * 2    mode    inode protection mode` |
|      - | 2612 | ` * 3    nlink   number of links` |
|      - | 2613 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 2614 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 2615 | ` * 6    rdev    device type, if inode device` |
|      - | 2616 | ` * 7    size    size in bytes` |
|      - | 2617 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 2618 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 2619 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 2620 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 2621 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 2622 | ` * Note:` |
|      - | 2623 | ` *  FALSE is returned on failure.` |
|      - | 2624 | ` */` |
|     32 | 2625 | `static int PH7_vfs_stat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2626 | `{` |
|      - | 2627 | `	ph7_value *pArray,*pValue;` |
|      - | 2628 | `	const char *zPath;` |
|      - | 2629 | `	ph7_vfs *pVfs;` |
|      - | 2630 | `	int rc;` |
|     34 | 2631 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2632 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2633 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2634 | `		return PH7_OK;` |
|      - | 2635 | `	}` |
|      - | 2636 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2637 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     34 | 2638 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_STAT) ){` |
|      7 | 2639 | `		return PH7_OK;` |
|      - | 2640 | `	}` |
|     28 | 2641 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     28 | 2642 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|      - | 2643 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2644 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2645 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2646 | `			ph7_function_name(pCtx)` |
|      - | 2647 | `			);` |
|    ! 0 | 2648 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2649 | `		return PH7_OK;` |
|      - | 2650 | `	}` |
|      - | 2651 | `	/* Create the array and the working value */` |
|     28 | 2652 | `	pArray = ph7_context_new_array(pCtx);` |
|     28 | 2653 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     28 | 2654 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 2655 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 2656 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2657 | `		return PH7_OK;` |
|      - | 2658 | `	}` |
|      - | 2659 | `	/* Extract the file path */` |
|     28 | 2660 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2661 | `	/* Perform the requested operation */` |
|     28 | 2662 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|     28 | 2663 | `	if( rc != PH7_OK ){` |
|      - | 2664 | ``		/* php warns before answering FALSE -- the same `stat failed for` /`` |
|      - | 2665 | ``		 * `Lstat failed for` line the rest of the family raises. PHL returned the`` |
|      - | 2666 | `		 * FALSE in silence, so a missing path and an empty result looked alike. */` |
|      8 | 2667 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|      8 | 2668 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2669 | `	}else{` |
|      - | 2670 | `		/* php's answer is the thirteen fields TWICE: numeric 0..12 then named. */` |
|     21 | 2671 | `		ph7_value *pFull = ph7_context_new_array(pCtx);` |
|     21 | 2672 | `		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){` |
|     21 | 2673 | `			ph7_result_value(pCtx,pFull);` |
|     11 | 2674 | `		}else{` |
|    ! 0 | 2675 | `			ph7_result_value(pCtx,pArray);` |
|      - | 2676 | `		}` |
|      - | 2677 | `	}` |
|      - | 2678 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 2679 | `	 * automatically as soon we return from this function. */` |
|     28 | 2680 | `	return PH7_OK;` |
|     18 | 2681 | `}` |
|      - | 2682 | `/*` |
|      - | 2683 | ` * array lstat(string $filename)` |
|      - | 2684 | ` *  Gives information about a file or symbolic link.` |
|      - | 2685 | ` * Parameters` |
|      - | 2686 | ` *  $filename` |
|      - | 2687 | ` *   Path to the file.` |
|      - | 2688 | ` * Return` |
|      - | 2689 | ` *  An associative array on success holding the following entries on success` |
|      - | 2690 | ` *  0   dev     device number` |
|      - | 2691 | ` * 1    ino     inode number (zero on windows)` |
|      - | 2692 | ` * 2    mode    inode protection mode` |
|      - | 2693 | ` * 3    nlink   number of links` |
|      - | 2694 | ` * 4    uid     userid of owner (zero on windows)` |
|      - | 2695 | ` * 5    gid     groupid of owner (zero on windows)` |
|      - | 2696 | ` * 6    rdev    device type, if inode device` |
|      - | 2697 | ` * 7    size    size in bytes` |
|      - | 2698 | ` * 8    atime   time of last access (Unix timestamp)` |
|      - | 2699 | ` * 9    mtime   time of last modification (Unix timestamp)` |
|      - | 2700 | ` * 10   ctime   time of last inode change (Unix timestamp)` |
|      - | 2701 | ` * 11   blksize blocksize of filesystem IO (zero on windows)` |
|      - | 2702 | ` * 12   blocks  number of 512-byte blocks allocated.` |
|      - | 2703 | ` * Note:` |
|      - | 2704 | ` *  FALSE is returned on failure.` |
|      - | 2705 | ` */` |
|     16 | 2706 | `static int PH7_vfs_lstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2707 | `{` |
|      - | 2708 | `	ph7_value *pArray,*pValue;` |
|      - | 2709 | `	const char *zPath;` |
|      - | 2710 | `	ph7_vfs *pVfs;` |
|      - | 2711 | `	int rc;` |
|     17 | 2712 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2713 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2714 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2715 | `		return PH7_OK;` |
|      - | 2716 | `	}` |
|      - | 2717 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2718 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     17 | 2719 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),PH7_STAT_ASK_LSTAT) ){` |
|      7 | 2720 | `		return PH7_OK;` |
|      - | 2721 | `	}` |
|     11 | 2722 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     11 | 2723 | `	if( pVfs == 0 \|\| pVfs->xlStat == 0 ){` |
|      - | 2724 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2725 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2726 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2727 | `			ph7_function_name(pCtx)` |
|      - | 2728 | `			);` |
|    ! 0 | 2729 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2730 | `		return PH7_OK;` |
|      - | 2731 | `	}` |
|      - | 2732 | `	/* Create the array and the working value */` |
|     11 | 2733 | `	pArray = ph7_context_new_array(pCtx);` |
|     11 | 2734 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     11 | 2735 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 2736 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 2737 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2738 | `		return PH7_OK;` |
|      - | 2739 | `	}` |
|      - | 2740 | `	/* Extract the file path */` |
|     11 | 2741 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      - | 2742 | `	/* Perform the requested operation */` |
|     11 | 2743 | `	rc = pVfs->xlStat(zPath,pArray,pValue);` |
|     11 | 2744 | `	if( rc != PH7_OK ){` |
|      - | 2745 | ``		/* php warns before answering FALSE -- the same `stat failed for` /`` |
|      - | 2746 | ``		 * `Lstat failed for` line the rest of the family raises. PHL returned the`` |
|      - | 2747 | `		 * FALSE in silence, so a missing path and an empty result looked alike. */` |
|      5 | 2748 | `		VfsThrowStatWarning(pCtx,zPath,1);` |
|      5 | 2749 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2750 | `	}else{` |
|      - | 2751 | `		/* php's answer is the thirteen fields TWICE: numeric 0..12 then named. */` |
|      7 | 2752 | `		ph7_value *pFull = ph7_context_new_array(pCtx);` |
|      7 | 2753 | `		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){` |
|      7 | 2754 | `			ph7_result_value(pCtx,pFull);` |
|      4 | 2755 | `		}else{` |
|    ! 0 | 2756 | `			ph7_result_value(pCtx,pArray);` |
|      - | 2757 | `		}` |
|      - | 2758 | `	}` |
|      - | 2759 | `	/* Don't worry about freeing memory here,everything will be released` |
|      - | 2760 | `	 * automatically as soon we return from this function. */` |
|     11 | 2761 | `	return PH7_OK;` |
|      9 | 2762 | `}` |
|      - | 2763 | `/*` |
|      - | 2764 | ` * int\|false fileowner / filegroup / fileinode / fileperms (string $filename)` |
|      - | 2765 | ` *  One stat() with one of its fields taken out of it, which is exactly how php` |
|      - | 2766 | ` *  implements them (php_stat's FS_OWNER / FS_GROUP / FS_INODE / FS_PERMS arms).` |
|      - | 2767 | ` *` |
|      - | 2768 | ` * They were prelude PHP wrapping stat(), which cost them php's diagnostic twice` |
|      - | 2769 | ` * over: three of the four said NOTHING on a failed stat (the fourth raised its own` |
|      - | 2770 | `` * `trigger_error`, so its errno was E_USER_WARNING's 512 rather than E_WARNING's 2`` |
|      - | 2771 | ` * and its line was the prelude's, not the caller's). In C the family shares one` |
|      - | 2772 | ` * warning site with the rest of stat(), and the four get real signature rows.` |
|      - | 2773 | ` */` |
|     92 | 2774 | `static int VfsStatField(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zField,int eAsk)` |
|      1 | 2775 | `{` |
|      - | 2776 | `	ph7_value *pArray,*pValue,*pField;` |
|      - | 2777 | `	const char *zPath;` |
|      - | 2778 | `	ph7_vfs *pVfs;` |
|      - | 2779 | `	int rc;` |
|     93 | 2780 | `	if( nArg < 1 ){` |
|    ! 0 | 2781 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2782 | `		return PH7_OK;` |
|      - | 2783 | `	}` |
|      - | 2784 | `	/* php asks the wrapper that owns the path before anything else` |
|      - | 2785 | `	 * (php_stream_url_stat_path); the VFS answers only what none owns. */` |
|     93 | 2786 | `	if( VfsUserStat(pCtx,ph7_value_to_string(apArg[0],0),eAsk) ){` |
|     49 | 2787 | `		return PH7_OK;` |
|      - | 2788 | `	}` |
|     45 | 2789 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     45 | 2790 | `	if( pVfs == 0 \|\| pVfs->xStat == 0 ){` |
|    ! 0 | 2791 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2792 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2793 | `			ph7_function_name(pCtx)` |
|      - | 2794 | `			);` |
|    ! 0 | 2795 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2796 | `		return PH7_OK;` |
|      - | 2797 | `	}` |
|     45 | 2798 | `	pArray = ph7_context_new_array(pCtx);` |
|     45 | 2799 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     45 | 2800 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 2801 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 2802 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2803 | `		return PH7_OK;` |
|      - | 2804 | `	}` |
|     45 | 2805 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|     45 | 2806 | `	rc = pVfs->xStat(zPath,pArray,pValue);` |
|     45 | 2807 | `	if( rc != PH7_OK ){` |
|     11 | 2808 | `		VfsThrowStatWarning(pCtx,zPath,0);` |
|     11 | 2809 | `		ph7_result_bool(pCtx,0);` |
|     11 | 2810 | `		return PH7_OK;` |
|      - | 2811 | `	}` |
|     35 | 2812 | `	pField = ph7_array_fetch(pArray,zField,-1);` |
|     35 | 2813 | `	if( pField == 0 ){` |
|      - | 2814 | `		/* The VFS answered a stat array without this field: nothing to report but` |
|      - | 2815 | `		 * the failure itself, which is what php answers when its own stat has no` |
|      - | 2816 | `		 * such member either. */` |
|    ! 0 | 2817 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2818 | `		return PH7_OK;` |
|      - | 2819 | `	}` |
|     35 | 2820 | `	ph7_result_int64(pCtx,ph7_value_to_int64(pField));` |
|     35 | 2821 | `	return PH7_OK;` |
|     47 | 2822 | `}` |
|     16 | 2823 | `static int PH7_vfs_file_owner(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2824 | `{` |
|     17 | 2825 | `	return VfsStatField(pCtx,nArg,apArg,"uid",PH7_STAT_ASK_OWNER);` |
|      1 | 2826 | `}` |
|     14 | 2827 | `static int PH7_vfs_file_group(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2828 | `{` |
|     15 | 2829 | `	return VfsStatField(pCtx,nArg,apArg,"gid",PH7_STAT_ASK_GROUP);` |
|      1 | 2830 | `}` |
|     22 | 2831 | `static int PH7_vfs_file_inode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2832 | `{` |
|     23 | 2833 | `	return VfsStatField(pCtx,nArg,apArg,"ino",PH7_STAT_ASK_INODE);` |
|      1 | 2834 | `}` |
|     40 | 2835 | `static int PH7_vfs_file_perms(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2836 | `{` |
|     41 | 2837 | `	return VfsStatField(pCtx,nArg,apArg,"mode",PH7_STAT_ASK_PERMS);` |
|      1 | 2838 | `}` |
|      - | 2839 | `/*` |
|      - | 2840 | ` * array\|string\|false getenv(?string $name = null, bool $local_only = false)` |
|      - | 2841 | ` *  Gets the value of an environment variable.` |
|      - | 2842 | ` * Parameters` |
|      - | 2843 | ` *  $name` |
|      - | 2844 | ` *   The variable name -- or NOTHING, which is the documented way to ask for the` |
|      - | 2845 | ` *   WHOLE environment as a name => value array. That form answered FALSE here,` |
|      - | 2846 | `` *   so `foreach (getenv() as $k => $v)` iterated over a bool.`` |
|      - | 2847 | ` *  $local_only` |
|      - | 2848 | ` *   Ask only the process's own environment rather than the SAPI's. On the CLI` |
|      - | 2849 | ` *   they are the same environment, so the argument selects the same answer --` |
|      - | 2850 | ` *   but it must still be ACCEPTED, and asking for the whole map with it set` |
|      - | 2851 | ` *   answered false too.` |
|      - | 2852 | ` * Return` |
|      - | 2853 | ` *  The value of the environment variable, or FALSE when it does not exist, or` |
|      - | 2854 | ` *  the whole environment when no name is given.` |
|      - | 2855 | ` */` |
|    146 | 2856 | `static int PH7_vfs_getenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2857 | `{` |
|      - | 2858 | `	const char *zEnv;` |
|      - | 2859 | `	ph7_vfs *pVfs;` |
|      - | 2860 | `	int iLen;` |
|      - | 2861 | `	/* Point to the underlying vfs */` |
|    151 | 2862 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    151 | 2863 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|      - | 2864 | `		/* The whole environment. xEnviron was APPENDED to ph7_vfs, so an` |
|      - | 2865 | `		 * embedder VFS built against version 2 does not have the field at all --` |
|      - | 2866 | `		 * reading it would run off the end of their struct. */` |
|      8 | 2867 | `		if( pVfs == 0 \|\| pVfs->iVersion < 3 \|\| pVfs->xEnviron == 0` |
|      9 | 2868 | `		 \|\| pVfs->xEnviron(pCtx) != PH7_OK ){` |
|    ! 0 | 2869 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2870 | `		}` |
|      9 | 2871 | `		return PH7_OK;` |
|      - | 2872 | `	}` |
|    143 | 2873 | `	if( pVfs == 0 \|\| pVfs->xGetenv == 0 ){` |
|      - | 2874 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2875 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2876 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2877 | `			ph7_function_name(pCtx)` |
|      - | 2878 | `			);` |
|    ! 0 | 2879 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2880 | `		return PH7_OK;` |
|      - | 2881 | `	}` |
|      - | 2882 | `	/* Extract the environment variable */` |
|    143 | 2883 | `	zEnv = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 2884 | `	/* Set a boolean FALSE as the default return value */` |
|    143 | 2885 | `	ph7_result_bool(pCtx,0);` |
|    143 | 2886 | `	if( iLen < 1 ){` |
|      - | 2887 | `		/* Empty string */` |
|      3 | 2888 | `		return PH7_OK;` |
|      - | 2889 | `	}` |
|      - | 2890 | `	/* Perform the requested operation */` |
|    141 | 2891 | `	pVfs->xGetenv(zEnv,pCtx);` |
|    141 | 2892 | `	return PH7_OK;` |
|     78 | 2893 | `}` |
|      - | 2894 | `/*` |
|      - | 2895 | ` * bool putenv(string $settings)` |
|      - | 2896 | ` *  Set the value of an environment variable.` |
|      - | 2897 | ` * Parameters` |
|      - | 2898 | ` *  $setting` |
|      - | 2899 | ` *   The setting, like "FOO=BAR"` |
|      - | 2900 | ` * Return` |
|      - | 2901 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2902 | ` */` |
|     77 | 2903 | `static int PH7_vfs_putenv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2904 | `{` |
|      - | 2905 | `	const char *zName,*zValue;` |
|      - | 2906 | `	char *zSettings,*zEnd;` |
|      - | 2907 | `	ph7_vfs *pVfs;` |
|      - | 2908 | `	int iLen,rc;` |
|     80 | 2909 | `	if( nArg < 1 ){` |
|      - | 2910 | `		/* Missing argument,return FALSE */` |
|    ! 0 | 2911 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2912 | `		return PH7_OK;` |
|      - | 2913 | `	}` |
|      - | 2914 | `	/* Extract the setting variable. It is NOT required to already BE a string:` |
|      - | 2915 | ``	 * the declared parameter is `string $assignment`, so php coerces an int or a`` |
|      - | 2916 | `	 * __toString() object first, where PH7 answered false and did nothing. */` |
|     80 | 2917 | `	zSettings = (char *)ph7_value_to_string(apArg[0],&iLen);` |
|     80 | 2918 | `	if( iLen < 1 \|\| zSettings[0] == '=' ){` |
|      - | 2919 | `		/* php's whole validity rule: an empty assignment, or one with no name in` |
|      - | 2920 | `		 * front of the '='. Everything else is accepted. */` |
|      9 | 2921 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2922 | `			"putenv(): Argument #1 ($assignment) must have a valid syntax");` |
|      - | 2923 | `	}` |
|      - | 2924 | `	/* Parse the setting. php looks for the '=' with strchr(), so an embedded NUL` |
|      - | 2925 | `	 * ENDS the search: putenv("FO\0O=BAR") finds no '=' at all and removes the` |
|      - | 2926 | `	 * variable named "FO" instead of setting one. */` |
|     72 | 2927 | `	zEnd = &zSettings[iLen];` |
|     72 | 2928 | `	zValue = 0;` |
|     72 | 2929 | `	zName = zSettings;` |
|    652 | 2930 | `	while( zSettings < zEnd && zSettings[0] != 0 ){` |
|    628 | 2931 | `		if( zSettings[0] == '=' ){` |
|      - | 2932 | `			/* Null terminate the name */` |
|     48 | 2933 | `			zSettings[0] = 0;` |
|     48 | 2934 | `			zValue = &zSettings[1];` |
|     48 | 2935 | `			break;` |
|      - | 2936 | `		}` |
|    583 | 2937 | `		zSettings++;` |
|      3 | 2938 | `	}` |
|      - | 2939 | ``	/* A missing '=' is not invalid syntax: `putenv("NAME")` REMOVES the variable,`` |
|      - | 2940 | `	 * which is the documented way to unset one, and PH7 read it as a failure and` |
|      - | 2941 | `	 * left the old value in place. An empty VALUE is a value too` |
|      - | 2942 | ``	 * (`putenv("NAME=")`), which the old `zValue >= zEnd` test rejected.`` |
|      - | 2943 | `	 * php does NOT touch $_ENV here: that array is the SAPI's startup snapshot,` |
|      - | 2944 | `	 * and a putenv() after it changes the process environment alone. PH7 wrote` |
|      - | 2945 | `	 * the pair into $_ENV as well, so a script could read back through $_ENV a` |
|      - | 2946 | `	 * variable php only exposes through getenv(). */` |
|      - | 2947 | `	/* Point to the underlying vfs */` |
|     72 | 2948 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     72 | 2949 | `	if( pVfs == 0 \|\| pVfs->xSetenv == 0 ){` |
|      - | 2950 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 2951 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2952 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 2953 | `			ph7_function_name(pCtx)` |
|      - | 2954 | `			);` |
|    ! 0 | 2955 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2956 | `		if( zValue ){` |
|    ! 0 | 2957 | `			zSettings[0] = '=';` |
|    ! 0 | 2958 | `		}` |
|    ! 0 | 2959 | `		return PH7_OK;` |
|      - | 2960 | `	}` |
|      - | 2961 | `	/* Perform the requested operation. A NULL value means REMOVE, and php reports` |
|      - | 2962 | `	 * TRUE for that whether or not the variable was there (or nameable) at all --` |
|      - | 2963 | `	 * only a failed SET is false. */` |
|     72 | 2964 | `	rc = pVfs->xSetenv(zName,zValue);` |
|     72 | 2965 | `	ph7_result_bool(pCtx,zValue == 0 \|\| rc == PH7_OK );` |
|     72 | 2966 | `	if( zValue ){` |
|      - | 2967 | `		/* Put back the '=' the name was terminated on. Without one, zSettings` |
|      - | 2968 | `		 * stopped on the terminator or on an embedded NUL, neither of which this` |
|      - | 2969 | `		 * routine wrote. */` |
|     48 | 2970 | `		zSettings[0] = '=';` |
|     15 | 2971 | `	}` |
|     72 | 2972 | `	return PH7_OK;` |
|     34 | 2973 | `}` |
|      - | 2974 | `/*` |
|      - | 2975 | ` * bool touch(string $filename[,int64 $time = time()[,int64 $atime]])` |
|      - | 2976 | ` *  Sets access and modification time of file.` |
|      - | 2977 | ` * Note: On windows` |
|      - | 2978 | ` *   If the file does not exists,it will not be created.` |
|      - | 2979 | ` * Parameters` |
|      - | 2980 | ` *  $filename` |
|      - | 2981 | ` *   The name of the file being touched.` |
|      - | 2982 | ` *  $time` |
|      - | 2983 | ` *   The touch time. If time is not supplied, the current system time is used.` |
|      - | 2984 | ` * $atime` |
|      - | 2985 | ` *   If present, the access time of the given filename is set to the value of atime.` |
|      - | 2986 | ` *   Otherwise, it is set to the value passed to the time parameter. If neither are` |
|      - | 2987 | ` *   present, the current system time is used.` |
|      - | 2988 | ` * Return` |
|      - | 2989 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2990 | `*/` |
|     74 | 2991 | `static int PH7_vfs_touch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2992 | `{` |
|      - | 2993 | `	ph7_int64 nTime,nAccess;` |
|      - | 2994 | `	const char *zFile;` |
|      - | 2995 | `	ph7_vfs *pVfs;` |
|      - | 2996 | `	int rc;` |
|     78 | 2997 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 2998 | `		/* Missing/Invalid argument,return FALSE */` |
|    ! 0 | 2999 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3000 | `		return PH7_OK;` |
|      - | 3001 | `	}` |
|      - | 3002 | `	/* php's touch() is a stream_metadata(STREAM_META_TOUCH) on a wrapper path, and` |
|      - | 3003 | `	 * its value is an ARRAY: empty when the caller named no time at all, else` |
|      - | 3004 | `	 * [mtime, atime] -- resolved BEFORE php's own now/echo defaults, which belong` |
|      - | 3005 | `	 * to the plain-file path below. */` |
|      - | 3006 | `	{` |
|     78 | 3007 | `		ph7_value *pTimes = ph7_context_new_array(pCtx);` |
|      - | 3008 | `		ph7_value sOp,sOne;` |
|      - | 3009 | `		ph7_value *apExtra[2];` |
|      - | 3010 | `		int bDone;` |
|     78 | 3011 | `		if( pTimes == 0 ){` |
|    ! 0 | 3012 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 3013 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3014 | `			return PH7_OK;` |
|      - | 3015 | `		}` |
|     78 | 3016 | `		PH7_MemObjInit(pCtx->pVm,&sOp);` |
|     78 | 3017 | `		PH7_MemObjInit(pCtx->pVm,&sOne);` |
|     78 | 3018 | `		ph7_value_int(&sOp,PH7_STREAM_META_TOUCH);` |
|     78 | 3019 | `		if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     24 | 3020 | `			ph7_int64 nM = ph7_value_to_int64(apArg[1]);` |
|     18 | 3021 | `			ph7_int64 nA = (nArg > 2 && !ph7_value_is_null(apArg[2]))` |
|     24 | 3022 | `				? ph7_value_to_int64(apArg[2]) : nM;` |
|     24 | 3023 | `			ph7_value_int64(&sOne,nM);` |
|     24 | 3024 | `			ph7_array_add_elem(pTimes,0,&sOne);` |
|     24 | 3025 | `			ph7_value_int64(&sOne,nA);` |
|     24 | 3026 | `			ph7_array_add_elem(pTimes,0,&sOne);` |
|     10 | 3027 | `		}` |
|     78 | 3028 | `		apExtra[0] = &sOp;` |
|     78 | 3029 | `		apExtra[1] = pTimes;` |
|    114 | 3030 | `		bDone = VfsUserWrite(pCtx,ph7_value_to_string(apArg[0],0),"stream_metadata",` |
|     36 | 3031 | `			0,apExtra,2);` |
|     78 | 3032 | `		PH7_MemObjRelease(&sOp);` |
|     78 | 3033 | `		PH7_MemObjRelease(&sOne);` |
|     78 | 3034 | `		if( bDone ){` |
|     24 | 3035 | `			return PH7_OK;` |
|      - | 3036 | `		}` |
|      - | 3037 | `	}` |
|      - | 3038 | `	/*` |
|      - | 3039 | `	 * A BUILT-IN wrapper has no stream_metadata to call, and php's touch() falls` |
|      - | 3040 | `	 * back to opening the url: it answers true for a name the wrapper has -- a` |
|      - | 3041 | ``	 * phar entry, a `data:` payload, `php://memory` -- and the open's own`` |
|      - | 3042 | `	 * sentence for one it does not. A userland wrapper without the method is NOT` |
|      - | 3043 | `	 * this case: php drops it to the plain-file path below, which reports it as` |
|      - | 3044 | `	 * a file it could not create.` |
|      - | 3045 | `	 */` |
|      - | 3046 | `	{` |
|     56 | 3047 | `		const char *zPath = ph7_value_to_string(apArg[0],0);` |
|     56 | 3048 | `		const char *zTail = zPath;` |
|     56 | 3049 | `		const ph7_io_stream *pStream = zPath` |
|     52 | 3050 | `			? PH7_VmGetStreamDevice(pCtx->pVm,&zTail,(int)SyStrlen(zPath)) : 0;` |
|     56 | 3051 | `		if( zPath && PH7_StreamWrapperLabel(pCtx->pVm,pStream) != 0 ){` |
|      - | 3052 | ``			/* php's fallback opens the url the way `c` does -- so a wrapper that`` |
|      - | 3053 | `			 * has the name answers true, one that cannot take a write-mode url` |
|      - | 3054 | ``			 * says so, and a `compress.zlib://` name is CREATED before its own`` |
|      - | 3055 | `			 * refusal. ext/phar is the one wrapper here with a touch handler of` |
|      - | 3056 | `			 * its own, and php's asks the entry the READING question. */` |
|     19 | 3057 | `			int iMode = PH7_PharStreamIs(pStream)` |
|     10 | 3058 | `				? PH7_IO_OPEN_RDONLY : PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|     27 | 3059 | `			void *pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zTail,` |
|      8 | 3060 | `				iMode,FALSE,0,FALSE,0,0);` |
|     19 | 3061 | `			if( pHandle ){` |
|     13 | 3062 | `				PH7_StreamCloseHandle(pStream,pHandle);` |
|     13 | 3063 | `				ph7_result_bool(pCtx,1);` |
|      6 | 3064 | `			}else{` |
|      7 | 3065 | `				VfsThrowOpenWarning(pCtx,zTail);` |
|      7 | 3066 | `				ph7_result_bool(pCtx,0);` |
|      - | 3067 | `			}` |
|     19 | 3068 | `			return PH7_OK;` |
|      - | 3069 | `		}` |
|     38 | 3070 | `		if( zPath && pStream == 0 ){` |
|      - | 3071 | `			/* A scheme nothing is registered under: php names it and then goes` |
|      - | 3072 | `			 * on to the plain-files path, which fails on the whole url. */` |
|      3 | 3073 | `			VfsThrowUnknownWrapperWarning(pCtx,zPath);` |
|      1 | 3074 | `		}` |
|      - | 3075 | `	}` |
|      - | 3076 | `	/* Point to the underlying vfs */` |
|     38 | 3077 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     38 | 3078 | `	if( pVfs == 0 \|\| pVfs->xTouch == 0 ){` |
|      - | 3079 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 3080 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3081 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 3082 | `			ph7_function_name(pCtx)` |
|      - | 3083 | `			);` |
|    ! 0 | 3084 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3085 | `		return PH7_OK;` |
|      - | 3086 | `	}` |
|      - | 3087 | `	/* Resolve php's defaults HERE, so the driver only ever sees real timestamps: a` |
|      - | 3088 | ``	 * NEGATIVE stamp is perfectly legal to php (`touch($f, -100)` is 1969), so it`` |
|      - | 3089 | `	 * cannot double as the "not given" sentinel the drivers used to read it as. An` |
|      - | 3090 | `	 * omitted/null $mtime is NOW; an omitted/null $atime follows $mtime. $atime also` |
|      - | 3091 | `	 * used to be read from apArg[1] — the mtime — so touch($f, $m, $a) silently` |
|      - | 3092 | `	 * stamped the modification time onto both. */` |
|     38 | 3093 | `	zFile = ph7_value_to_string(apArg[0],0);` |
|     38 | 3094 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) && nArg > 1 && ph7_value_is_null(apArg[1]) ){` |
|    ! 0 | 3095 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3096 | `			"touch(): Argument #2 ($mtime) cannot be null when argument #3 ($atime) "` |
|      - | 3097 | `			"is an integer");` |
|      - | 3098 | `	}` |
|     38 | 3099 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     13 | 3100 | `		nTime = ph7_value_to_int64(apArg[1]);` |
|      8 | 3101 | `	}else{` |
|      - | 3102 | `		time_t tNow;` |
|     26 | 3103 | `		time(&tNow);` |
|     26 | 3104 | `		nTime = (ph7_int64)tNow;` |
|      - | 3105 | `	}` |
|     38 | 3106 | `	nAccess = nTime;` |
|     38 | 3107 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      3 | 3108 | `		nAccess = ph7_value_to_int64(apArg[2]);` |
|      1 | 3109 | `	}` |
|     38 | 3110 | `	errno = 0;` |
|     38 | 3111 | `	rc = pVfs->xTouch(zFile,nTime,nAccess);` |
|     38 | 3112 | `	if( rc != PH7_OK ){` |
|      - | 3113 | `		/* php's own sentence for this one, which names both the path and the` |
|      - | 3114 | `		 * reason and reads like neither of the family's other two. */` |
|      7 | 3115 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to create file %s because %s",` |
|      4 | 3116 | `			ph7_function_name(pCtx),zFile ? zFile : "",VfsStrerror(errno));` |
|      2 | 3117 | `	}` |
|      - | 3118 | `	/* IO result */` |
|     38 | 3119 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     38 | 3120 | `	return PH7_OK;` |
|     40 | 3121 | `}` |
|      - | 3122 | `/*` |
|      - | 3123 | ` * Path processing functions that do not need access to the VFS layer` |
|      - | 3124 | ` * Status:` |
|      - | 3125 | ` *    Stable.` |
|      - | 3126 | ` */` |
|      - | 3127 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 3128 | `/*` |
|      - | 3129 | ` * string dirname(string $path)` |
|      - | 3130 |  |
|      - | 3131 | ` *  Returns parent directory's path.` |
|      - | 3132 | ` * Parameters` |
|      - | 3133 | ` * $path` |
|      - | 3134 | ` *  Target path.` |
|      - | 3135 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 3136 | ` *  In other environments, it is the forward slash (/).` |
|      - | 3137 | ` * Return` |
|      - | 3138 | ` *  The path of the parent directory. If there are no slashes in path, a dot ('.')` |
|      - | 3139 | ` *  is returned, indicating the current directory.` |
|      - | 3140 | ` */` |
|    130 | 3141 | `static int PH7_builtin_dirname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3142 | `{` |
|      - | 3143 | `	const char *zPath,*zDir;` |
|      - | 3144 | `	int iLen,iDirlen;` |
|    135 | 3145 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 3146 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 3147 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3148 | `		return PH7_OK;` |
|      - | 3149 | `	}` |
|      - | 3150 | `	/* Point to the target path */` |
|    135 | 3151 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|    135 | 3152 | `	if( iLen < 1 ){` |
|      - | 3153 | `		/* php answers "" for the empty path, not "." */` |
|      3 | 3154 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3155 | `		return PH7_OK;` |
|      - | 3156 | `	}` |
|      - | 3157 | `	/* $levels (php 7.0) was ACCEPTED AND IGNORED, so dirname($p, 3) silently answered` |
|      - | 3158 | `	 * the one-level parent — the caller's own answer, one or more levels too deep. Each` |
|      - | 3159 | `	 * level re-runs php_dirname on the previous result and stops as soon as the answer` |
|      - | 3160 | `	 * stops moving (the filesystem root, or "." for a relative path), which is what php` |
|      - | 3161 | `	 * does; php also rejects a level below 1 outright. */` |
|    133 | 3162 | `	zDir = zPath;` |
|    133 | 3163 | `	iDirlen = iLen;` |
|    133 | 3164 | `	if( nArg > 1 ){` |
|     51 | 3165 | `		ph7_int64 nLevels = ph7_value_to_int64(apArg[1]);` |
|      - | 3166 | `		ph7_int64 i;` |
|     51 | 3167 | `		if( nLevels < 1 ){` |
|      7 | 3168 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3169 | `				"dirname(): Argument #2 ($levels) must be greater than or equal to 1");` |
|      - | 3170 | `		}` |
|    125 | 3171 | `		for( i = 0 ; i < nLevels ; ++i ){` |
|    105 | 3172 | `			int iPrevLen = iDirlen;` |
|    105 | 3173 | `			const char *zPrev = zDir;` |
|    105 | 3174 | `			zDir = PH7_ExtractDirName(zPrev,iPrevLen,&iDirlen);` |
|    105 | 3175 | `			if( iDirlen == iPrevLen && SyMemcmp(zDir,zPrev,(sxu32)iDirlen) == 0 ){` |
|     25 | 3176 | `				break; /* fixed point: "/" and "." are their own parents */` |
|      - | 3177 | `			}` |
|     41 | 3178 | `		}` |
|     23 | 3179 | `	}else{` |
|     83 | 3180 | `		zDir = PH7_ExtractDirName(zPath,iLen,&iDirlen);` |
|      - | 3181 | `	}` |
|      - | 3182 | `	/* Return directory name */` |
|    127 | 3183 | `	ph7_result_string(pCtx,zDir,iDirlen);` |
|    127 | 3184 | `	return PH7_OK;` |
|     70 | 3185 | `}` |
|      - | 3186 | `/*` |
|      - | 3187 | ` * string basename(string $path[, string $suffix ])` |
|      - | 3188 | ` *  Returns trailing name component of path.` |
|      - | 3189 | ` * Parameters` |
|      - | 3190 | ` * $path` |
|      - | 3191 | ` *  Target path.` |
|      - | 3192 | ` *  On Windows, both slash (/) and backslash (\) are used as directory separator character.` |
|      - | 3193 | ` *  In other environments, it is the forward slash (/).` |
|      - | 3194 | ` * $suffix` |
|      - | 3195 | ` *  If the name component ends in suffix this will also be cut off.` |
|      - | 3196 | ` * Return` |
|      - | 3197 | ` *  The base name of the given path.` |
|      - | 3198 | ` */` |
|    218 | 3199 | `static int PH7_builtin_basename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3200 | `{` |
|      - | 3201 | `	const char *zPath,*zBase;` |
|      - | 3202 | `	int iLen,nBase;` |
|    222 | 3203 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 3204 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 3205 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3206 | `		return PH7_OK;` |
|      - | 3207 | `	}` |
|      - | 3208 | `	/* Point to the target path */` |
|    222 | 3209 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 3210 | `	/* php_basename, shared with pathinfo(): the hand-rolled walk this used to carry` |
|      - | 3211 | `	 * kept a leading separator on a single-component path (basename("/a") answered` |
|      - | 3212 | `	 * "/a", basename("/.") answered "/.") because it stopped one byte short. */` |
|    222 | 3213 | `	zBase = PH7_ExtractBaseName(zPath,iLen,&nBase);` |
|    222 | 3214 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 3215 | `		const char *zSuffix;` |
|      - | 3216 | `		int nSuffix;` |
|      - | 3217 | `		/* Strip suffix — php leaves the basename alone when it IS the suffix */` |
|      5 | 3218 | `		zSuffix = ph7_value_to_string(apArg[1],&nSuffix);` |
|      4 | 3219 | `		if( nSuffix > 0 && nSuffix < nBase` |
|      5 | 3220 | `		 && SyMemcmp(&zBase[nBase - nSuffix],zSuffix,nSuffix) == 0 ){` |
|      5 | 3221 | `			nBase -= nSuffix;` |
|      2 | 3222 | `		}` |
|      2 | 3223 | `	}` |
|      - | 3224 | `	/* Store the basename */` |
|    222 | 3225 | `	ph7_result_string(pCtx,zBase,nBase);` |
|    222 | 3226 | `	return PH7_OK;` |
|    113 | 3227 | `}` |
|      - | 3228 | `/*` |
|      - | 3229 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 3230 | ` *  Returns information about a file path.` |
|      - | 3231 | ` * Parameter` |
|      - | 3232 | ` *  $path` |
|      - | 3233 | ` *   The path to be parsed.` |
|      - | 3234 | ` *  $options` |
|      - | 3235 | ` *    If present, specifies a specific element to be returned; one of` |
|      - | 3236 | ` *      PATHINFO_DIRNAME, PATHINFO_BASENAME, PATHINFO_EXTENSION or PATHINFO_FILENAME.` |
|      - | 3237 | ` * Return` |
|      - | 3238 | ` *  If the options parameter is not passed, an associative array containing the following` |
|      - | 3239 | ` *  elements is returned: dirname, basename, extension (if any), and filename.` |
|      - | 3240 | ` *  If options is present, returns a string containing the requested element.` |
|      - | 3241 | ` */` |
|      - | 3242 | `typedef struct path_info path_info;` |
|      - | 3243 | `struct path_info` |
|      - | 3244 | `{` |
|      - | 3245 | `	SyString sDir; /* Directory [i.e: /var/www] */` |
|      - | 3246 | `	SyString sBasename; /* Basename [i.e httpd.conf] */` |
|      - | 3247 | `	SyString sExtension; /* File extension [i.e xml,pdf..] */` |
|      - | 3248 | `	SyString sFilename;  /* Filename */` |
|      - | 3249 | `	int iPresent;        /* Which components php would EMIT (PH7_PATHINFO_* bits) */` |
|      - | 3250 | `};` |
|      - | 3251 | `/*` |
|      - | 3252 | ` * Extract path fields exactly as php's pathinfo() assembles them.` |
|      - | 3253 | ` *` |
|      - | 3254 | ` * Two things this has to get right beyond the values themselves:` |
|      - | 3255 | ` *` |
|      - | 3256 | ` *  - php looks for the LAST dot ANYWHERE in the basename, a leading one included,` |
|      - | 3257 | `` *    so `.bashrc` has extension "bashrc" and filename "" (PH7 stopped the scan`` |
|      - | 3258 | ` *    before the first byte, so it reported no extension and filename ".bashrc").` |
|      - | 3259 | ` *  - EMPTY is not the same as ABSENT. php always emits basename and filename when` |
|      - | 3260 | `` *    they are asked for, emits extension whenever a dot exists (even for `x.`,`` |
|      - | 3261 | ` *    whose extension is ""), and emits dirname only when it is non-empty. The` |
|      - | 3262 | ` *    scalar form answers with the first EMITTED component, so conflating the two` |
|      - | 3263 | `` *    makes `pathinfo("x.", PATHINFO_EXTENSION\|PATHINFO_FILENAME)` fall through to`` |
|      - | 3264 | ` *    the filename ("x") where php answers "" — iPresent keeps them apart.` |
|      - | 3265 | ` *` |
|      - | 3266 | ` * dirname and basename come from the shared php_dirname/php_basename helpers` |
|      - | 3267 | ` * rather than a third hand-rolled walk, so the trailing-separator and` |
|      - | 3268 | ` * relative-path rules ("file.txt" -> ".", "/var/www/" -> "/var" + "www") cannot` |
|      - | 3269 | ` * drift between the two builtins and this one.` |
|      - | 3270 | ` */` |
|  19048 | 3271 | `static sxi32 ExtractPathInfo(const char *zPath,int nByte,path_info *pOut)` |
|      5 | 3272 | `{` |
|      - | 3273 | `	const char *zBase,*zDir,*zDot;` |
|      - | 3274 | `	int nBase,nDir,i;` |
|      - | 3275 | `	/* Zero the structure */` |
|  19053 | 3276 | `	SyZero(pOut,sizeof(path_info));` |
|  19053 | 3277 | `	zDir = PH7_ExtractDirName(zPath,nByte,&nDir);` |
|  19053 | 3278 | `	if( nDir > 0 ){` |
|  19049 | 3279 | `		SyStringInitFromBuf(&pOut->sDir,zDir,nDir);` |
|  19049 | 3280 | `		pOut->iPresent \|= PH7_PATHINFO_DIRNAME;` |
|   9522 | 3281 | `	}` |
|  19053 | 3282 | `	zBase = PH7_ExtractBaseName(zPath,nByte,&nBase);` |
|  19053 | 3283 | `	SyStringInitFromBuf(&pOut->sBasename,zBase,nBase);` |
|  19053 | 3284 | `	pOut->iPresent \|= PH7_PATHINFO_BASENAME\|PH7_PATHINFO_FILENAME;` |
|      - | 3285 | `	/* Last dot anywhere in the basename splits filename from extension */` |
|  19053 | 3286 | `	zDot = 0;` |
|  95163 | 3287 | `	for( i = nBase ; i > 0 ; --i ){` |
|  95141 | 3288 | `		if( zBase[i - 1] == '.' ){` |
|  19031 | 3289 | `			zDot = &zBase[i - 1];` |
|  19031 | 3290 | `			break;` |
|      - | 3291 | `		}` |
|  38060 | 3292 | `	}` |
|  19053 | 3293 | `	if( zDot ){` |
|  19031 | 3294 | `		SyStringInitFromBuf(&pOut->sExtension,zDot + 1,(int)(&zBase[nBase] - (zDot + 1)));` |
|  19031 | 3295 | `		pOut->iPresent \|= PH7_PATHINFO_EXTENSION;` |
|  19031 | 3296 | `		SyStringInitFromBuf(&pOut->sFilename,zBase,(int)(zDot - zBase));` |
|   9518 | 3297 | `	}else{` |
|     23 | 3298 | `		SyStringInitFromBuf(&pOut->sFilename,zBase,nBase);` |
|      - | 3299 | `	}` |
|  19053 | 3300 | `	return SXRET_OK;` |
|      5 | 3301 | `}` |
|      - | 3302 | `/*` |
|      - | 3303 | ` * value pathinfo(string $path [,int $options = PATHINFO_DIRNAME \| PATHINFO_BASENAME \| PATHINFO_EXTENSION \| PATHINFO_FILENAME ])` |
|      - | 3304 | ` *  See block comment above.` |
|      - | 3305 | ` */` |
|  19048 | 3306 | `static int PH7_builtin_pathinfo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3307 | `{` |
|      - | 3308 | `	const char *zPath;` |
|      - | 3309 | `	path_info sInfo;` |
|      - | 3310 | `	int iLen;` |
|  19053 | 3311 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|      - | 3312 | `		/* Missing/Invalid argument,return the empty string */` |
|    ! 0 | 3313 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3314 | `		return PH7_OK;` |
|      - | 3315 | `	}` |
|      - | 3316 | `	/* Point to the target path. The EMPTY path is not a special case: php still` |
|      - | 3317 | ``	 * answers with the array `["basename" => "", "filename" => ""]` (and "" for a`` |
|      - | 3318 | `	 * scalar request), where PH7 short-circuited to "" and returned the wrong TYPE. */` |
|  19053 | 3319 | `	zPath = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 3320 | `	/* Extract path info */` |
|  19053 | 3321 | `	ExtractPathInfo(zPath,iLen,&sInfo);` |
|      - | 3322 | ``	/* Read the mask at 64-bit width: ph7_value_to_int() truncates to `int`, so a`` |
|      - | 3323 | `	 * flags value congruent to PATHINFO_ALL mod 2^32 (4294967311, -4294967281 …)` |
|      - | 3324 | `	 * would take the ARRAY branch and answer with the wrong TYPE. */` |
|  19048 | 3325 | `	if( nArg > 1 && ph7_value_is_int(apArg[1])` |
|  28554 | 3326 | `	 && ph7_value_to_int64(apArg[1]) != (ph7_int64)PH7_PATHINFO_ALL ){` |
|      - | 3327 | `		/* $flags is a BITMASK, not an enum: php assembles the requested components in` |
|      - | 3328 | `		 * the fixed order below and, for anything other than PATHINFO_ALL, hands back` |
|      - | 3329 | `		 * the FIRST one it EMITTED (zend_hash_get_current_data on the fresh array).` |
|      - | 3330 | ``		 * So `PATHINFO_DIRNAME\|PATHINFO_BASENAME` answers the dirname, and an unknown`` |
|      - | 3331 | `		 * bit that happens to carry a known one along (99 = 1\|2\|32\|64) answers as if` |
|      - | 3332 | `		 * only the known ones were passed. PH7 numbered the components 1/2/3/4 and` |
|      - | 3333 | `		 * switched on the whole value, so it read a two-flag mask as a different single` |
|      - | 3334 | `		 * component and answered "" for everything else. Emission is iPresent, NOT` |
|      - | 3335 | `		 * "non-empty": an emitted-but-empty component ends the search with "". */` |
|  19035 | 3336 | `		ph7_int64 nComp = ph7_value_to_int64(apArg[1]);` |
|      - | 3337 | `		static const int aBit[4] = {` |
|      - | 3338 | `			PH7_PATHINFO_DIRNAME,PH7_PATHINFO_BASENAME,` |
|      - | 3339 | `			PH7_PATHINFO_EXTENSION,PH7_PATHINFO_FILENAME` |
|      - | 3340 | `		};` |
|      - | 3341 | `		SyString *apComp[4];` |
|      - | 3342 | `		int i;` |
|  19035 | 3343 | `		apComp[0] = &sInfo.sDir;` |
|  19035 | 3344 | `		apComp[1] = &sInfo.sBasename;` |
|  19035 | 3345 | `		apComp[2] = &sInfo.sExtension;` |
|  19035 | 3346 | `		apComp[3] = &sInfo.sFilename;` |
|      - | 3347 | `		/* Expand the empty string unless a requested component is emitted */` |
|  19035 | 3348 | `		ph7_result_string(pCtx,"",0);` |
|  66545 | 3349 | `		for( i = 0 ; i < 4 ; ++i ){` |
|  66537 | 3350 | `			if( (nComp & aBit[i]) == aBit[i] && (sInfo.iPresent & aBit[i]) ){` |
|  19027 | 3351 | `				ph7_result_string(pCtx,apComp[i]->zString,(int)apComp[i]->nByte);` |
|  19027 | 3352 | `				break;` |
|      - | 3353 | `			}` |
|  23760 | 3354 | `		}` |
|   9520 | 3355 | `	}else{` |
|      - | 3356 | `		/* Return an associative array */` |
|      - | 3357 | `		ph7_value *pArray,*pValue;` |
|     19 | 3358 | `		pArray = ph7_context_new_array(pCtx);` |
|     19 | 3359 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     19 | 3360 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 3361 | `			/* Out of mem,return NULL */` |
|    ! 0 | 3362 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3363 | `			return PH7_OK;` |
|      - | 3364 | `		}` |
|      - | 3365 | ``		/* Emitted-but-EMPTY components are in the array too (php keys `basename` and`` |
|      - | 3366 | ``		 * `filename` for "/" with ""), so this walks iPresent, not the lengths. */`` |
|      - | 3367 | `		{` |
|      - | 3368 | `		static const char *azKey[4] = {"dirname","basename","extension","filename"};` |
|      - | 3369 | `		static const int aBit[4] = {` |
|      - | 3370 | `			PH7_PATHINFO_DIRNAME,PH7_PATHINFO_BASENAME,` |
|      - | 3371 | `			PH7_PATHINFO_EXTENSION,PH7_PATHINFO_FILENAME` |
|      - | 3372 | `		};` |
|      - | 3373 | `		SyString *apComp[4];` |
|      - | 3374 | `		int i;` |
|     19 | 3375 | `		apComp[0] = &sInfo.sDir;` |
|     19 | 3376 | `		apComp[1] = &sInfo.sBasename;` |
|     19 | 3377 | `		apComp[2] = &sInfo.sExtension;` |
|     19 | 3378 | `		apComp[3] = &sInfo.sFilename;` |
|     91 | 3379 | `		for( i = 0 ; i < 4 ; ++i ){` |
|     73 | 3380 | `			if( (sInfo.iPresent & aBit[i]) == 0 ){` |
|     11 | 3381 | `				continue;` |
|      - | 3382 | `			}` |
|     63 | 3383 | `			ph7_value_reset_string_cursor(pValue);` |
|     63 | 3384 | `			ph7_value_string(pValue,apComp[i]->zString,(int)apComp[i]->nByte);` |
|     63 | 3385 | `			ph7_array_add_strkey_elem(pArray,azKey[i],pValue); /* Will make it's own copy */` |
|     32 | 3386 | `		}` |
|      - | 3387 | `		}` |
|      - | 3388 | `		/* Return the created array */` |
|     19 | 3389 | `		ph7_result_value(pCtx,pArray);` |
|      - | 3390 | `		/* Don't worry about freeing memory, everything will be released` |
|      - | 3391 | `		 * automatically as soon we return from this foreign function.` |
|      - | 3392 | `		 */` |
|      - | 3393 | `	}` |
|  19053 | 3394 | `	return PH7_OK;` |
|   9529 | 3395 | `}` |
|      - | 3396 | `/* SPDX-SnippetBegin */` |
|      - | 3397 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 3398 | `/* SPDX-License-Identifier: blessing */` |
|      - | 3399 | `/*` |
|      - | 3400 | ` * Globbing implementation extracted from the sqlite3 source tree.` |
|      - | 3401 |  |
|      - | 3402 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|      - | 3403 | ` * Status: Public Domain` |
|      - | 3404 | ` */` |
|      - | 3405 | `typedef unsigned char u8;` |
|      - | 3406 | `/* An array to map all upper-case characters into their corresponding` |
|      - | 3407 | `** lower-case character.` |
|      - | 3408 | `**` |
|      - | 3409 | `** SQLite only considers US-ASCII (or EBCDIC) characters.  We do not` |
|      - | 3410 | `** handle case conversions for the UTF character set since the tables` |
|      - | 3411 | `** involved are nearly as big or bigger than SQLite itself.` |
|      - | 3412 | `*/` |
|      - | 3413 | `static const unsigned char sqlite3UpperToLower[] = {` |
|      - | 3414 | `      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,` |
|      - | 3415 | `     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,` |
|      - | 3416 | `     36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,` |
|      - | 3417 | `     54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,` |
|      - | 3418 | `    104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,` |
|      - | 3419 | `    122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,` |
|      - | 3420 | `    108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,` |
|      - | 3421 | `    126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,` |
|      - | 3422 | `    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,` |
|      - | 3423 | `    162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,` |
|      - | 3424 | `    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,` |
|      - | 3425 | `    198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,` |
|      - | 3426 | `    216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,` |
|      - | 3427 | `    234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,` |
|      - | 3428 | `    252,253,254,255` |
|      - | 3429 | `};` |
|      - | 3430 | `#define GlogUpperToLower(A)     if( A<0x80 ){ A = sqlite3UpperToLower[A]; }` |
|      - | 3431 | `/*` |
|      - | 3432 | `** Assuming zIn points to the first byte of a UTF-8 character,` |
|      - | 3433 | `** advance zIn to point to the first byte of the next UTF-8 character.` |
|      - | 3434 | `*/` |
|      - | 3435 | `#define SQLITE_SKIP_UTF8(zIn) {                        \` |
|      - | 3436 | `  if( (*(zIn++))>=0xc0 ){                              \` |
|      - | 3437 | `    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \` |
|      - | 3438 | `  }                                                    \` |
|      - | 3439 | `}` |
|      - | 3440 | `/*` |
|      - | 3441 | `** Compare two UTF-8 strings for equality where the first string can` |
|      - | 3442 | `** potentially be a "glob" expression.  Return true (1) if they` |
|      - | 3443 | `** are the same and false (0) if they are different.` |
|      - | 3444 | `**` |
|      - | 3445 | `** Globbing rules:` |
|      - | 3446 | `**` |
|      - | 3447 | `**      '*'       Matches any sequence of zero or more characters.` |
|      - | 3448 | `**` |
|      - | 3449 | `**      '?'       Matches exactly one character.` |
|      - | 3450 | `**` |
|      - | 3451 | `**     [...]      Matches one character from the enclosed list of` |
|      - | 3452 | `**                characters.` |
|      - | 3453 | `**` |
|      - | 3454 | `**     [^...]     Matches one character not in the enclosed list.` |
|      - | 3455 | `**` |
|      - | 3456 | `** With the [...] and [^...] matching, a ']' character can be included` |
|      - | 3457 | `** in the list by making it the first character after '[' or '^'.  A` |
|      - | 3458 | `** range of characters can be specified using '-'.  Example:` |
|      - | 3459 | `** "[a-z]" matches any single lower-case letter.  To match a '-', make` |
|      - | 3460 | `** it the last character in the list.` |
|      - | 3461 | `**` |
|      - | 3462 | `** This routine is usually quick, but can be N**2 in the worst case.` |
|      - | 3463 | `**` |
|      - | 3464 | `** Hints: to match '*' or '?', put them in "[]".  Like this:` |
|      - | 3465 | `**` |
|      - | 3466 | `**         abc[*]xyz        Matches "abc*xyz" only` |
|      - | 3467 | `*/` |
|      - | 3468 | `/*` |
|      - | 3469 | `` * One POSIX character class of a `[...]` set, as glibc's matcher answers it.`` |
|      - | 3470 | ` * The classes are ASCII-only in the C locale php runs its fnmatch()/glob() in,` |
|      - | 3471 | ` * so a code point past 127 belongs to none of them.` |
|      - | 3472 | ` */` |
|     92 | 3473 | `static int PatternPosixClass(const unsigned char *zName,int nName,int c)` |
|      1 | 3474 | `{` |
|      - | 3475 | `	static const struct { const char *zName; int nName; } aClass[] = {` |
|      - | 3476 | `		{ "alnum", 5 }, { "alpha", 5 }, { "blank", 5 }, { "cntrl", 5 },` |
|      - | 3477 | `		{ "digit", 5 }, { "graph", 5 }, { "lower", 5 }, { "print", 5 },` |
|      - | 3478 | `		{ "punct", 5 }, { "space", 5 }, { "upper", 5 }, { "xdigit", 6 },` |
|      - | 3479 | `	};` |
|     93 | 3480 | `	int i,iWhich = -1;` |
|    545 | 3481 | `	for( i = 0 ; i < (int)(sizeof(aClass)/sizeof(aClass[0])) ; ++i ){` |
|    540 | 3482 | `		if( aClass[i].nName == nName` |
|    495 | 3483 | `		 && SyMemcmp(aClass[i].zName,(const char *)zName,(sxu32)nName) == 0 ){` |
|     89 | 3484 | `			iWhich = i;` |
|     89 | 3485 | `			break;` |
|      - | 3486 | `		}` |
|    227 | 3487 | `	}` |
|     93 | 3488 | `	if( iWhich < 0 \|\| c < 0 \|\| c > 127 ){` |
|      - | 3489 | `		/* An unknown class name matches nothing, which is what a matcher that` |
|      - | 3490 | `		 * cannot name the set can honestly say. */` |
|      5 | 3491 | `		return 0;` |
|      - | 3492 | `	}` |
|     89 | 3493 | `	switch( iWhich ){` |
|    ! 0 | 3494 | `		case 0: return SyisAlphaNum(c);` |
|     21 | 3495 | `		case 1: return SyisAlpha(c);` |
|    ! 0 | 3496 | `		case 2: return c == ' ' \|\| c == '\t';` |
|    ! 0 | 3497 | `		case 3: return c < 0x20 \|\| c == 0x7F;` |
|     49 | 3498 | `		case 4: return SyisDigit(c);` |
|    ! 0 | 3499 | `		case 5: return c > 0x20 && c < 0x7F;` |
|    ! 0 | 3500 | `		case 6: return SyisLower(c);` |
|    ! 0 | 3501 | `		case 7: return c >= 0x20 && c < 0x7F;` |
|      5 | 3502 | `		case 8: return c > 0x20 && c < 0x7F && !SyisAlphaNum(c);` |
|      5 | 3503 | `		case 9: return SyisSpace(c);` |
|      9 | 3504 | `		case 10: return SyisUpper(c);` |
|      5 | 3505 | `		default: return SyisHex(c);` |
|      - | 3506 | `	}` |
|     47 | 3507 | `}` |
|   2660 | 3508 | `static int patternCompare(` |
|      - | 3509 | `  const u8 *zPattern,              /* The glob pattern */` |
|      - | 3510 | `  const u8 *zString,               /* The string to compare against the glob */` |
|      - | 3511 | `  const int esc,                    /* The escape character */` |
|      - | 3512 | `  int noCase,` |
|      - | 3513 | ``  int bCaret                        /* `[^...]` inverts (fnmatch) or is a literal `^` (glob) */`` |
|      4 | 3514 | `){` |
|      - | 3515 | `  int c, c2, cLow;` |
|      - | 3516 | `  int invert;` |
|      - | 3517 | `  int seen;` |
|   2664 | 3518 | `  u8 matchOne = '?';` |
|   2664 | 3519 | `  u8 matchAll = '*';` |
|   2664 | 3520 | `  u8 matchSet = '[';` |
|   2664 | 3521 | `  int prevEscape = 0;     /* True if the previous character was 'escape' */` |
|      - | 3522 |  |
|   2664 | 3523 | `  if( !zPattern \|\| !zString ) return 0;` |
|   3729 | 3524 | `  while( (c = PH7_Utf8Read(zPattern,0,&zPattern))!=0 ){` |
|   3347 | 3525 | `    if( !prevEscape && c==matchAll ){` |
|   2800 | 3526 | `      while( (c=PH7_Utf8Read(zPattern,0,&zPattern)) == matchAll` |
|   1404 | 3527 | `               \|\| c == matchOne ){` |
|    ! 0 | 3528 | `        if( c==matchOne && PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 3529 | `          return 0;` |
|      - | 3530 | `        }` |
|    ! 0 | 3531 | `      }` |
|   1404 | 3532 | `      if( c==0 ){` |
|    868 | 3533 | `        return 1;` |
|    538 | 3534 | `      }else if( c==esc ){` |
|    ! 0 | 3535 | `        c = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    ! 0 | 3536 | `        if( c==0 ){` |
|    ! 0 | 3537 | `          return 0;` |
|    ! 0 | 3538 | `        }` |
|    538 | 3539 | `      }else if( c==matchSet ){` |
|      - | 3540 | ``        /* A `[...]` set right after a `*`: try it at every remaining position.`` |
|      - | 3541 | `         * The two asserts SQLite has here became guards, and one of them --` |
|      - | 3542 | `         * "'[' is a single-byte character" -- is ALWAYS true, so this branch` |
|      - | 3543 | ``         * returned 0 for every pattern of the shape `*[...]`. `*[ab]`,`` |
|      - | 3544 | ``         * `a*[0-9]` and `*[[:digit:]]` matched NOTHING, in fnmatch(), in`` |
|      - | 3545 | `         * glob() and in strglob() alike. */` |
|    270 | 3546 | `        while( *zString && patternCompare(&zPattern[-1],zString,esc,noCase,bCaret)==0 ){` |
|    147 | 3547 | `          SQLITE_SKIP_UTF8(zString);` |
|      1 | 3548 | `        }` |
|     83 | 3549 | `        return *zString!=0;` |
|      - | 3550 | `      }` |
|    544 | 3551 | `      while( (c2 = PH7_Utf8Read(zString,0,&zString))!=0 ){` |
|    544 | 3552 | `        if( noCase ){` |
|      3 | 3553 | `          GlogUpperToLower(c2);` |
|      3 | 3554 | `          GlogUpperToLower(c);` |
|     11 | 3555 | `          while( c2 != 0 && c2 != c ){` |
|      9 | 3556 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      9 | 3557 | `            GlogUpperToLower(c2);` |
|      1 | 3558 | `          }` |
|      2 | 3559 | `        }else{` |
|   1548 | 3560 | `          while( c2 != 0 && c2 != c ){` |
|   1008 | 3561 | `            c2 = PH7_Utf8Read(zString, 0, &zString);` |
|      2 | 3562 | `          }` |
|      - | 3563 | `        }` |
|    544 | 3564 | `        if( c2==0 ) return 0;` |
|    268 | 3565 | `		if( patternCompare(zPattern,zString,esc,noCase,bCaret) ) return 1;` |
|      2 | 3566 | `      }` |
|    ! 0 | 3567 | `      return 0;` |
|   1946 | 3568 | `    }else if( !prevEscape && c==matchOne ){` |
|     25 | 3569 | `      if( PH7_Utf8Read(zString, 0, &zString)==0 ){` |
|    ! 0 | 3570 | `        return 0;` |
|      1 | 3571 | `      }` |
|   1934 | 3572 | `    }else if( c==matchSet ){` |
|    459 | 3573 | `      int prior_c = 0;` |
|      - | 3574 | `      /* SQLite asserts here that its GLOB has no escape character; the guard` |
|      - | 3575 | `       * that replaced the assert reads the condition BACKWARDS, so a set` |
|      - | 3576 | ``       * matched nothing whenever escaping was turned off -- every `[...]` in`` |
|      - | 3577 | ``       * an `fnmatch($p,$s,FNM_NOESCAPE)` call answered false. */`` |
|    459 | 3578 | `      seen = 0;` |
|    459 | 3579 | `      invert = 0;` |
|    459 | 3580 | `      c = PH7_Utf8Read(zString, 0, &zString);` |
|    459 | 3581 | `      if( c==0 ) return 0;` |
|      - | 3582 | `      /* A case-INSENSITIVE match folds inside the set too: this branch ignored` |
|      - | 3583 | ``       * noCase entirely, so `fnmatch('[a-c]','B',FNM_CASEFOLD)` was false and`` |
|      - | 3584 | ``       * its negation `[!a-c]` was true -- both the opposite of php's. The`` |
|      - | 3585 | `       * folded subject is what MEMBERS and RANGES are compared against; a` |
|      - | 3586 | ``        * character CLASS is not folded at all (glibc tests `[[:upper:]]` `` |
|      - | 3587 | `       * against the character as written, FNM_CASEFOLD or not). */` |
|    459 | 3588 | `      cLow = c;` |
|    459 | 3589 | `      if( noCase ){` |
|     49 | 3590 | `        GlogUpperToLower(cLow);` |
|     24 | 3591 | `      }` |
|    459 | 3592 | `      c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|      - | 3593 | ``      /* POSIX spells the negation `!` and glibc accepts `^` as well; php's`` |
|      - | 3594 | ``       * fnmatch()/glob() are glibc's, so BOTH invert. Only `^` did here, which`` |
|      - | 3595 | ``       * made `[!a]` a set holding `!` and `a` -- the exact INVERSE answer for`` |
|      - | 3596 | `       * the spelling a shell uses. */` |
|    459 | 3597 | `      if( c2=='!' \|\| (bCaret && c2=='^') ){` |
|    117 | 3598 | `        invert = 1;` |
|    117 | 3599 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|     58 | 3600 | `      }` |
|    459 | 3601 | `      if( c2==']' ){` |
|     19 | 3602 | `        if( c==']' ) seen = 1;` |
|     19 | 3603 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|      9 | 3604 | `      }` |
|   1171 | 3605 | `      while( c2 && c2!=']' ){` |
|    713 | 3606 | `        int cFold = c2;` |
|    713 | 3607 | `        if( noCase ){` |
|     71 | 3608 | `          GlogUpperToLower(cFold);` |
|     35 | 3609 | `        }` |
|    713 | 3610 | `        if( c2=='[' && zPattern[0]==':' ){` |
|      - | 3611 | ``          /* A POSIX character CLASS, `[:alpha:]`, which glibc's matcher knows`` |
|      - | 3612 | ``           * and this one did not -- the whole `[[:digit:]]` bracket read as the`` |
|      - | 3613 | ``           * literal set `[:digt` and matched the wrong characters in silence. */`` |
|     93 | 3614 | `          const unsigned char *zName = &zPattern[1];` |
|     93 | 3615 | `          const unsigned char *zEnd = zName;` |
|    553 | 3616 | `          while( zEnd[0] != 0 && !(zEnd[0]==':' && zEnd[1]==']') ){` |
|    461 | 3617 | `            zEnd++;` |
|      1 | 3618 | `          }` |
|     93 | 3619 | `          if( zEnd[0] != 0 ){` |
|     93 | 3620 | `            if( PatternPosixClass(zName,(int)(zEnd - zName),c) ){` |
|     47 | 3621 | `              seen = 1;` |
|     23 | 3622 | `            }` |
|     93 | 3623 | `            zPattern = zEnd + 2;` |
|     93 | 3624 | `            prior_c = 0;` |
|     93 | 3625 | `            c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|     93 | 3626 | `            continue;` |
|      - | 3627 | `          }` |
|    ! 0 | 3628 | `        }` |
|    621 | 3629 | `        if( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 ){` |
|    187 | 3630 | `          c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|    187 | 3631 | `          cFold = c2;` |
|    187 | 3632 | `          if( noCase ){` |
|     15 | 3633 | `            GlogUpperToLower(cFold);` |
|      7 | 3634 | `          }` |
|    187 | 3635 | `          if( cLow>=prior_c && cLow<=cFold ) seen = 1;` |
|    187 | 3636 | `          prior_c = 0;` |
|     94 | 3637 | `        }else{` |
|    435 | 3638 | `          if( cLow==cFold ){` |
|     61 | 3639 | `            seen = 1;` |
|     30 | 3640 | `          }` |
|    435 | 3641 | `          prior_c = cFold;` |
|      - | 3642 | `        }` |
|    621 | 3643 | `        c2 = PH7_Utf8Read(zPattern, 0, &zPattern);` |
|      1 | 3644 | `      }` |
|    459 | 3645 | `      if( c2==0 \|\| (seen ^ invert)==0 ){` |
|    203 | 3646 | `        return 0;` |
|      1 | 3647 | `      }` |
|   1592 | 3648 | `    }else if( esc==c && !prevEscape ){` |
|      9 | 3649 | `      prevEscape = 1;` |
|      5 | 3650 | `    }else{` |
|   1456 | 3651 | `      c2 = PH7_Utf8Read(zString, 0, &zString);` |
|   1456 | 3652 | `      if( noCase ){` |
|     17 | 3653 | `        GlogUpperToLower(c);` |
|     17 | 3654 | `        GlogUpperToLower(c2);` |
|      8 | 3655 | `      }` |
|   1456 | 3656 | `      if( c!=c2 ){` |
|    678 | 3657 | `        return 0;` |
|      - | 3658 | `      }` |
|    780 | 3659 | `      prevEscape = 0;` |
|      - | 3660 | `    }` |
|      3 | 3661 | `  }` |
|    384 | 3662 | `  return *zString==0;` |
|   1387 | 3663 | `}` |
|      - | 3664 | `/* SPDX-SnippetEnd */` |
|      - | 3665 | `/*` |
|      - | 3666 | ` * Wrapper around patternCompare() defined above.` |
|      - | 3667 | ` * See block comment above for more information.` |
|      - | 3668 | ` */` |
|   2206 | 3669 | `static int Glob(const unsigned char *zPattern,const unsigned char *zString,int iEsc,` |
|      - | 3670 | `	int CaseCompare,int bCaret)` |
|      4 | 3671 | `{` |
|      - | 3672 | `	int rc;` |
|   2210 | 3673 | `	if( iEsc < 0 ){` |
|    ! 0 | 3674 | `		iEsc = '\\';` |
|    ! 0 | 3675 | `	}` |
|   2210 | 3676 | `	rc = patternCompare(zPattern,zString,iEsc,CaseCompare,bCaret);` |
|   2210 | 3677 | `	return rc;` |
|      4 | 3678 | `}` |
|      - | 3679 | `/*` |
|      - | 3680 | ` * bool fnmatch(string $pattern,string $string[,int $flags = 0 ])` |
|      - | 3681 | ` *  Match filename against a pattern.` |
|      - | 3682 | ` * Parameters` |
|      - | 3683 | ` *  $pattern` |
|      - | 3684 | ` *   The shell wildcard pattern.` |
|      - | 3685 | ` * $string` |
|      - | 3686 | ` *  The tested string.` |
|      - | 3687 | ` * $flags` |
|      - | 3688 | ` *   A list of possible flags:` |
|      - | 3689 | ` *    FNM_NOESCAPE 	Disable backslash escaping.` |
|      - | 3690 | ` *    FNM_PATHNAME 	Slash in string only matches slash in the given pattern.` |
|      - | 3691 | ` *    FNM_PERIOD 	Leading period in string must be exactly matched by period in the given pattern.` |
|      - | 3692 | ` *    FNM_CASEFOLD 	Caseless match.` |
|      - | 3693 | ` * Return` |
|      - | 3694 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 3695 | ` */` |
|    158 | 3696 | `static int PH7_builtin_fnmatch(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3697 | `{` |
|      - | 3698 | `	const char *zString,*zPattern;` |
|    159 | 3699 | `	int iEsc = '\\';` |
|    159 | 3700 | `	int noCase = 0;` |
|      - | 3701 | `	int rc;` |
|    159 | 3702 | `	if( nArg < 2 \|\| !VfsPathArgUsable(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 3703 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3704 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3705 | `		return PH7_OK;` |
|      - | 3706 | `	}` |
|      - | 3707 | `	/* Extract the pattern and the string */` |
|    159 | 3708 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|    159 | 3709 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 3710 | `	/* Extract the flags if avaialble */` |
|    159 | 3711 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|     99 | 3712 | `		rc = ph7_value_to_int(apArg[2]);` |
|     99 | 3713 | `		if( rc & 2 /*FNM_NOESCAPE (php value)*/){` |
|     51 | 3714 | `			iEsc = 0;` |
|     25 | 3715 | `		}` |
|     99 | 3716 | `		if( rc & 16 /*FNM_CASEFOLD (php value)*/){` |
|     45 | 3717 | `			noCase = 1;` |
|     22 | 3718 | `		}` |
|     49 | 3719 | `	}` |
|      - | 3720 | ``	/* Go globbing. fnmatch() is glibc's, whose matcher takes `^` as a second`` |
|      - | 3721 | `	 * spelling of the negation -- glob(3)'s does NOT, and strglob() below` |
|      - | 3722 | `	 * carries glob()'s rule because that is what the prelude glob() drives. */` |
|    159 | 3723 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,noCase,TRUE);` |
|      - | 3724 | `	/* Globbing result */` |
|    159 | 3725 | `	ph7_result_bool(pCtx,rc);` |
|    159 | 3726 | `	return PH7_OK;` |
|     80 | 3727 | `}` |
|      - | 3728 | `/*` |
|      - | 3729 | ` * bool strglob(string $pattern,string $string)` |
|      - | 3730 | ` *  Match string against a pattern.` |
|      - | 3731 | ` * Parameters` |
|      - | 3732 | ` *  $pattern` |
|      - | 3733 | ` *   The shell wildcard pattern.` |
|      - | 3734 | ` * $string` |
|      - | 3735 | ` *  The tested string.` |
|      - | 3736 | ` * Return` |
|      - | 3737 | ` *  TRUE if there is a match, FALSE otherwise.` |
|      - | 3738 | ` * Note that this a symisc eXtension.` |
|      - | 3739 | ` */` |
|   1456 | 3740 | `static int PH7_builtin_strglob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3741 | `{` |
|      - | 3742 | `	const char *zString,*zPattern;` |
|   1460 | 3743 | `	int iEsc = '\\';` |
|      - | 3744 | `	int rc;` |
|   1460 | 3745 | `	if( nArg < 2 \|\| !VfsPathArgUsable(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 3746 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3747 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3748 | `		return PH7_OK;` |
|      - | 3749 | `	}` |
|      - | 3750 | `	/* Extract the pattern and the string */` |
|   1460 | 3751 | `	zPattern  = ph7_value_to_string(apArg[0],0);` |
|   1460 | 3752 | `	zString = ph7_value_to_string(apArg[1],0);` |
|      - | 3753 | ``	/* Go globbing, with glob(3)'s set rules: only `!` inverts, and a `^` right`` |
|      - | 3754 | ``	 * after the `[` is an ordinary member of the set. */`` |
|   1460 | 3755 | `	rc = Glob((const unsigned char *)zPattern,(const unsigned char *)zString,iEsc,0,FALSE);` |
|      - | 3756 | `	/* Globbing result */` |
|   1460 | 3757 | `	ph7_result_bool(pCtx,rc);` |
|   1460 | 3758 | `	return PH7_OK;` |
|    730 | 3759 | `}` |
|      - | 3760 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 3761 | `/* Every buffer below is one path, and php's own limit for one is PATH_MAX; the` |
|      - | 3762 | ` * SPL directory opener already refuses a longer one, and a pattern past it can` |
|      - | 3763 | ` * name nothing that exists. It also bounds the recursion: each level of the` |
|      - | 3764 | ` * walk consumes at least one slash of the pattern. */` |
|      - | 3765 | `#define PH7_GLOB_PATH_MAX 4096` |
|      - | 3766 | `/*` |
|      - | 3767 | ` * ---------------------------------------------------------------------------` |
|      - | 3768 | ` * The glob:// stream device.` |
|      - | 3769 | ` *` |
|      - | 3770 | ` * php's glob wrapper is a DIRECTORY whose entries are a pattern's matches:` |
|      - | 3771 | `` * `opendir('glob://src/' . '*.php')` hands out one BASENAME per match, and`` |
|      - | 3772 | ` * GlobIterator is that stream behind the whole DirectoryIterator machinery. It` |
|      - | 3773 | ` * is a dir_opener and NOTHING else -- php gives it no stream opener (so` |
|      - | 3774 | `` * `fopen('glob://…')` is "wrapper does not support stream open") and no`` |
|      - | 3775 | `` * url_stat (so `file_exists()` and `is_dir()` answer false for one).`` |
|      - | 3776 | ` *` |
|      - | 3777 | ` * The expansion is glob(3) with NO flags, which is what php's opener asks for,` |
|      - | 3778 | ` * so it has to agree name for name AND order for order with the prelude` |
|      - | 3779 | ` * glob(). Two rules are the whole of it -- a pattern is matched one SEGMENT at` |
|      - | 3780 | ` * a time, and the answer is sorted by BYTES rather than by value -- and both` |
|      - | 3781 | ` * are spelled here the way that function spells them, because the two` |
|      - | 3782 | ` * implementations must not drift: 001-smoke/glob_stream_device.phpt walks a` |
|      - | 3783 | ` * table of patterns through both and compares, which is what pins them.` |
|      - | 3784 | ` *` |
|      - | 3785 | `` * php's `pglob->path` is the directory of the match a read just handed OUT,`` |
|      - | 3786 | `` * never the pattern's: `glob://a/` + `*` + `/` + `*.txt` reports `a/sub1`, then`` |
|      - | 3787 | `` * `a/sub2`; a match with no slash in it reports the EMPTY string; and running`` |
|      - | 3788 | ` * out clears it, which is why GlobIterator::getPathname() answers "" past the` |
|      - | 3789 | ` * end.` |
|      - | 3790 | ` * ---------------------------------------------------------------------------` |
|      - | 3791 | ` */` |
|      - | 3792 | `/* One matched path. The blob it points into grows as the walk does, so an` |
|      - | 3793 | ` * OFFSET is what may be kept -- a pointer would not survive the next append. */` |
|      - | 3794 | `typedef PH7_GlobHit glob_hit;` |
|      - | 3795 | `typedef struct glob_stream glob_stream;` |
|      - | 3796 | `struct glob_stream` |
|      - | 3797 | `{` |
|      - | 3798 | `	ph7_vm *pVm;` |
|      - | 3799 | `	SyBlob sHit;   /* every matched path, back to back */` |
|      - | 3800 | `	SySet aHit;    /* one glob_hit per match, in php's order */` |
|      - | 3801 | `	sxu32 nCur;    /* php's pglob->index -- it counts PAST the end too */` |
|      - | 3802 | `	SyBlob sDir;   /* php's pglob->path: the directory of the CURRENT match */` |
|      - | 3803 | `};` |
|      - | 3804 | `/* php's glob_pattern_p: is there anything here for glob(3) to expand? */` |
|     92 | 3805 | `static int GlobHasMeta(const char *zPat,int nPat)` |
|      1 | 3806 | `{` |
|      - | 3807 | `	int i;` |
|   3727 | 3808 | `	for( i = 0 ; i < nPat ; ++i ){` |
|   3645 | 3809 | `		if( zPat[i] == '*' \|\| zPat[i] == '?' \|\| zPat[i] == '[' ){` |
|     11 | 3810 | `			return 1;` |
|      - | 3811 | `		}` |
|   2830 | 3812 | `	}` |
|     83 | 3813 | `	return 0;` |
|     47 | 3814 | `}` |
|      - | 3815 | `/* strcoll() in the C locale php runs in: unsigned bytes, then the shorter one` |
|      - | 3816 | ` * first. The same comparison the prelude glob() gets out of SORT_STRING. */` |
|    242 | 3817 | `static int GlobCmp(const char *zA,sxu32 nA,const char *zB,sxu32 nB)` |
|      1 | 3818 | `{` |
|    243 | 3819 | `	sxu32 nMin = nA < nB ? nA : nB;` |
|      - | 3820 | `	sxu32 i;` |
|   9889 | 3821 | `	for( i = 0 ; i < nMin ; ++i ){` |
|   9883 | 3822 | `		int ca = (unsigned char)zA[i];` |
|   9883 | 3823 | `		int cb = (unsigned char)zB[i];` |
|   9883 | 3824 | `		if( ca != cb ){` |
|    237 | 3825 | `			return ca < cb ? -1 : 1;` |
|      - | 3826 | `		}` |
|   7446 | 3827 | `	}` |
|      7 | 3828 | `	if( nA == nB ){` |
|    ! 0 | 3829 | `		return 0;` |
|      - | 3830 | `	}` |
|      7 | 3831 | `	return nA < nB ? -1 : 1;` |
|    121 | 3832 | `}` |
|      - | 3833 | `/*` |
|      - | 3834 | ` * Order one call's own answers. glob(3) sorts the whole list it is about to` |
|      - | 3835 | ` * return rather than each directory it walked, and so does the prelude glob(),` |
|      - | 3836 | ` * so every branch below sorts the range IT produced.` |
|      - | 3837 | ` *` |
|      - | 3838 | ` * A shell sort: filenames within one answer are unique, so nothing here needs` |
|      - | 3839 | ` * to be stable, and a directory of ten thousand entries must not cost the` |
|      - | 3840 | ` * hundred million comparisons an insertion sort would.` |
|      - | 3841 | ` */` |
|    104 | 3842 | `static void GlobSort(SyBlob *pHit,SySet *pSet,sxu32 nStart)` |
|      1 | 3843 | `{` |
|    105 | 3844 | `	glob_hit *aHit = (glob_hit *)SySetBasePtr(pSet);` |
|    105 | 3845 | `	const char *zBase = (const char *)SyBlobData(pHit);` |
|    105 | 3846 | `	sxu32 nEnd = SySetUsed(pSet);` |
|      - | 3847 | `	sxu32 nSpan,nGap;` |
|    105 | 3848 | `	if( nEnd - nStart < 2 ){` |
|     39 | 3849 | `		return;` |
|      - | 3850 | `	}` |
|     67 | 3851 | `	nSpan = nEnd - nStart;` |
|    153 | 3852 | `	for( nGap = nSpan / 2 ; nGap > 0 ; nGap /= 2 ){` |
|      - | 3853 | `		sxu32 i;` |
|    293 | 3854 | `		for( i = nStart + nGap ; i < nEnd ; ++i ){` |
|    207 | 3855 | `			glob_hit sTmp = aHit[i];` |
|    207 | 3856 | `			sxu32 j = i;` |
|    358 | 3857 | `			while( j >= nStart + nGap` |
|    416 | 3858 | `			 && GlobCmp(&zBase[aHit[j-nGap].nOfs],aHit[j-nGap].nLen,` |
|    362 | 3859 | `			            &zBase[sTmp.nOfs],sTmp.nLen) > 0 ){` |
|     87 | 3860 | `				aHit[j] = aHit[j-nGap];` |
|     87 | 3861 | `				j -= nGap;` |
|    ! 0 | 3862 | `			}` |
|    207 | 3863 | `			aHit[j] = sTmp;` |
|    104 | 3864 | `		}` |
|     44 | 3865 | `	}` |
|     53 | 3866 | `}` |
|      - | 3867 | `/* Record one match, spelled as a head and a tail so that the trailing-slash` |
|      - | 3868 | ` * branch can put its slash back without a second buffer. */` |
|    218 | 3869 | `static sxi32 GlobAdd(SyBlob *pHit,SySet *pSet,const char *zHead,sxu32 nHead,` |
|      - | 3870 | `	const char *zTail,sxu32 nTail)` |
|      1 | 3871 | `{` |
|      - | 3872 | `	glob_hit sHit;` |
|    219 | 3873 | `	sHit.nOfs = SyBlobLength(pHit);` |
|    219 | 3874 | `	sHit.nLen = nHead + nTail;` |
|    219 | 3875 | `	if( nHead > 0 && SyBlobAppend(pHit,zHead,nHead) != SXRET_OK ){` |
|    ! 0 | 3876 | `		return SXERR_MEM;` |
|      - | 3877 | `	}` |
|    219 | 3878 | `	if( nTail > 0 && SyBlobAppend(pHit,zTail,nTail) != SXRET_OK ){` |
|    ! 0 | 3879 | `		return SXERR_MEM;` |
|      - | 3880 | `	}` |
|    219 | 3881 | `	return SySetPut(pSet,(const void *)&sHit);` |
|    110 | 3882 | `}` |
|      - | 3883 | `/* Forward: the two halves of the walk call each other. */` |
|      - | 3884 | `static sxi32 GlobExpand(ph7_vm *pVm,const char *zPat,int nPat,int bOnlyDir,` |
|      - | 3885 | `	SyBlob *pHit,SySet *pSet);` |
|      - | 3886 | `/*` |
|      - | 3887 | ` * php's leaf: read the directory the pattern's last slash names and keep every` |
|      - | 3888 | ` * entry the segment after it matches, with that literal prefix back in front.` |
|      - | 3889 | ` * A directory that cannot be opened is zero matches in SILENCE, which is what` |
|      - | 3890 | ` * glob(3) answers for a path that is not there.` |
|      - | 3891 | ` */` |
|     82 | 3892 | `static sxi32 GlobLeaf(ph7_vm *pVm,const char *zPat,int nPat,int bOnlyDir,` |
|      - | 3893 | `	SyBlob *pHit,SySet *pSet)` |
|      1 | 3894 | `{` |
|     83 | 3895 | `	const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|      - | 3896 | `	const ph7_io_stream *pStream;` |
|      - | 3897 | `	const char *zDev,*zSeg;` |
|     83 | 3898 | `	void *pHandle = 0;` |
|      - | 3899 | `	ph7_context sCtx;` |
|      - | 3900 | `	ph7_value sEntry;` |
|      - | 3901 | `	char zDir[PH7_GLOB_PATH_MAX],zSegBuf[PH7_GLOB_PATH_MAX],zEnt[PH7_GLOB_PATH_MAX];` |
|     83 | 3902 | `	int nDir,nPrefix,nSeg,i,iSlash = -1;` |
|     83 | 3903 | `	sxi32 rc = SXRET_OK;` |
|    423 | 3904 | `	for( i = nPat - 1 ; i >= 0 ; --i ){` |
|    423 | 3905 | `		if( zPat[i] == '/' ){` |
|     83 | 3906 | `			iSlash = i;` |
|     83 | 3907 | `			break;` |
|      - | 3908 | `		}` |
|    171 | 3909 | `	}` |
|     83 | 3910 | `	if( iSlash < 0 ){` |
|      - | 3911 | ``		/* no directory part at all: php's own `.` */`` |
|    ! 0 | 3912 | `		zDir[0] = '.';` |
|    ! 0 | 3913 | `		nDir = 1;` |
|    ! 0 | 3914 | `		nPrefix = 0;` |
|     83 | 3915 | `	}else if( iSlash == 0 ){` |
|      - | 3916 | ``		/* the pattern is rooted: the directory is `/` itself */`` |
|    ! 0 | 3917 | `		zDir[0] = '/';` |
|    ! 0 | 3918 | `		nDir = 1;` |
|    ! 0 | 3919 | `		nPrefix = 1;` |
|    ! 0 | 3920 | `	}else{` |
|     83 | 3921 | `		nDir = iSlash;` |
|     83 | 3922 | `		SyMemcpy(zPat,zDir,(sxu32)nDir);` |
|     83 | 3923 | `		nPrefix = iSlash + 1;` |
|      - | 3924 | `	}` |
|     83 | 3925 | `	zDir[nDir] = 0;` |
|     83 | 3926 | `	zSeg = &zPat[iSlash + 1];` |
|     83 | 3927 | `	nSeg = nPat - (iSlash + 1);` |
|     83 | 3928 | `	if( nSeg >= (int)sizeof(zSegBuf) ){` |
|    ! 0 | 3929 | `		return SXRET_OK;` |
|      - | 3930 | `	}` |
|     83 | 3931 | `	SyMemcpy(zSeg,zSegBuf,(sxu32)nSeg);` |
|     83 | 3932 | `	zSegBuf[nSeg] = 0;` |
|      - | 3933 | `	/* The directory is opened through the SAME lookup opendir() uses, so the` |
|      - | 3934 | `	 * two implementations see one filesystem: the prelude glob() reaches it by` |
|      - | 3935 | `	 * calling opendir() itself. */` |
|     83 | 3936 | `	zDev = zDir;` |
|     83 | 3937 | `	pStream = PH7_VmGetStreamDevice(pVm,&zDev,nDir);` |
|     83 | 3938 | `	if( pStream == 0 \|\| pStream->xOpenDir == 0 \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 3939 | `		return SXRET_OK;` |
|      - | 3940 | `	}` |
|      - | 3941 | `	/* The VFS reports a name by writing a RESULT, so the read needs a call` |
|      - | 3942 | `	 * context of its own, and the cursor is reset between entries because a` |
|      - | 3943 | `	 * result APPENDS (rule 54). The same value carries the VM into the open:` |
|      - | 3944 | `	 * a device reaches it through that argument and nothing else. */` |
|     83 | 3945 | `	PH7_MemObjInit(pVm,&sEntry);` |
|     83 | 3946 | `	if( pStream->xOpenDir(zDev,&sEntry,&pHandle) != PH7_OK ){` |
|      3 | 3947 | `		PH7_MemObjRelease(&sEntry);` |
|      3 | 3948 | `		return SXRET_OK;` |
|      - | 3949 | `	}` |
|     81 | 3950 | `	VmInitCallContext(&sCtx,pVm,0,&sEntry,0);` |
|    479 | 3951 | `	for(;;){` |
|      - | 3952 | `		const char *zName;` |
|    873 | 3953 | `		int nName = 0;` |
|    873 | 3954 | `		ph7_value_reset_string_cursor(&sEntry);` |
|    873 | 3955 | `		if( pStream->xReadDir(pHandle,&sCtx) != PH7_OK ){` |
|     81 | 3956 | `			break;` |
|      - | 3957 | `		}` |
|    793 | 3958 | `		zName = ph7_value_to_string(&sEntry,&nName);` |
|    792 | 3959 | `		if( nName < 1 \|\| nName >= (int)sizeof(zEnt)` |
|    793 | 3960 | `		 \|\| nPrefix + nName >= (int)sizeof(zEnt) ){` |
|    ! 0 | 3961 | `			continue;` |
|      - | 3962 | `		}` |
|    793 | 3963 | `		SyMemcpy(zName,zEnt,(sxu32)nName);` |
|    793 | 3964 | `		zEnt[nName] = 0;` |
|      - | 3965 | `		/* php's FNM_PERIOD: a leading dot is matched only by a pattern that` |
|      - | 3966 | ``		 * spells one, which is what keeps `.`, `..` and every hidden name out`` |
|      - | 3967 | ``		 * of an ordinary `*`. */`` |
|    793 | 3968 | `		if( zEnt[0] == '.' && (nSeg < 1 \|\| zSegBuf[0] != '.') ){` |
|    201 | 3969 | `			continue;` |
|      - | 3970 | `		}` |
|    593 | 3971 | `		if( !Glob((const unsigned char *)zSegBuf,(const unsigned char *)zEnt,'\\',0,FALSE) ){` |
|    381 | 3972 | `			continue;` |
|      - | 3973 | `		}` |
|    213 | 3974 | `		if( bOnlyDir ){` |
|      - | 3975 | `			/* GLOB_ONLYDIR, which only the trailing-slash branch below asks` |
|      - | 3976 | `			 * for -- the device itself always globs with no flags at all. */` |
|      - | 3977 | `			char zProbe[PH7_GLOB_PATH_MAX * 2];` |
|     43 | 3978 | `			SyMemcpy(zDir,zProbe,(sxu32)nDir);` |
|     43 | 3979 | `			zProbe[nDir] = '/';` |
|     43 | 3980 | `			SyMemcpy(zEnt,&zProbe[nDir+1],(sxu32)nName);` |
|     43 | 3981 | `			zProbe[nDir + 1 + nName] = 0;` |
|     43 | 3982 | `			if( pVfs == 0 \|\| pVfs->xIsdir == 0 \|\| pVfs->xIsdir(zProbe) != PH7_OK ){` |
|     19 | 3983 | `				continue;` |
|      - | 3984 | `			}` |
|     12 | 3985 | `		}` |
|    195 | 3986 | `		rc = GlobAdd(pHit,pSet,zPat,(sxu32)nPrefix,zEnt,(sxu32)nName);` |
|    195 | 3987 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3988 | `			break;` |
|      - | 3989 | `		}` |
|      1 | 3990 | `	}` |
|     81 | 3991 | `	VmReleaseCallContext(&sCtx);` |
|     81 | 3992 | `	PH7_MemObjRelease(&sEntry);` |
|     81 | 3993 | `	if( pStream->xCloseDir ){` |
|     81 | 3994 | `		pStream->xCloseDir(pHandle);` |
|     40 | 3995 | `	}` |
|     81 | 3996 | `	return rc;` |
|     42 | 3997 | `}` |
|      - | 3998 | `/*` |
|      - | 3999 | ` * One pattern, every segment of it. php's three branches, in php's order: a` |
|      - | 4000 | ` * pattern that ENDS in a slash names directories and KEEPS the slash; a` |
|      - | 4001 | ` * wildcard in the directory part is walked level by level; anything else is` |
|      - | 4002 | ` * one directory read. Each branch sorts the range it produced.` |
|      - | 4003 | ` */` |
|    104 | 4004 | `static sxi32 GlobExpand(ph7_vm *pVm,const char *zPat,int nPat,int bOnlyDir,` |
|      - | 4005 | `	SyBlob *pHit,SySet *pSet)` |
|      1 | 4006 | `{` |
|    105 | 4007 | `	sxu32 nStart = SySetUsed(pSet);` |
|      - | 4008 | `	SyBlob sSub;` |
|      - | 4009 | `	SySet aSub;` |
|      - | 4010 | `	glob_hit *aRec;` |
|      - | 4011 | `	sxu32 n,nRec;` |
|    105 | 4012 | `	int i,iSlash = -1;` |
|      - | 4013 | `	sxi32 rc;` |
|    105 | 4014 | `	if( nPat < 1 \|\| nPat >= PH7_GLOB_PATH_MAX ){` |
|    ! 0 | 4015 | `		return SXRET_OK;` |
|      - | 4016 | `	}` |
|    105 | 4017 | `	if( zPat[nPat-1] == '/' ){` |
|      - | 4018 | ``		/* `d/` is ['d/'] and `d/` + `*` + `/` is ['d/a/','d/b/']: answer the base as`` |
|      - | 4019 | `		 * DIRECTORIES and put ONE slash back, so a pattern ending in two keeps` |
|      - | 4020 | `		 * both. php sorts the names it ANSWERS, slash included. */` |
|     13 | 4021 | `		const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|     13 | 4022 | `		if( nPat == 1 ){` |
|    ! 0 | 4023 | `			if( pVfs && pVfs->xIsdir && pVfs->xIsdir("/") == PH7_OK ){` |
|    ! 0 | 4024 | `				return GlobAdd(pHit,pSet,"/",1,0,0);` |
|      - | 4025 | `			}` |
|    ! 0 | 4026 | `			return SXRET_OK;` |
|      - | 4027 | `		}` |
|     13 | 4028 | `		SyBlobInit(&sSub,&pVm->sAllocator);` |
|     13 | 4029 | `		SySetInit(&aSub,&pVm->sAllocator,sizeof(glob_hit));` |
|     13 | 4030 | `		rc = GlobExpand(pVm,zPat,nPat-1,TRUE,&sSub,&aSub);` |
|     13 | 4031 | `		if( rc == SXRET_OK ){` |
|     13 | 4032 | `			aRec = (glob_hit *)SySetBasePtr(&aSub);` |
|     13 | 4033 | `			nRec = SySetUsed(&aSub);` |
|     37 | 4034 | `			for( n = 0 ; n < nRec ; ++n ){` |
|     37 | 4035 | `				rc = GlobAdd(pHit,pSet,` |
|     24 | 4036 | `					&((const char *)SyBlobData(&sSub))[aRec[n].nOfs],aRec[n].nLen,"/",1);` |
|     25 | 4037 | `				if( rc != SXRET_OK ){` |
|    ! 0 | 4038 | `					break;` |
|      - | 4039 | `				}` |
|     13 | 4040 | `			}` |
|      6 | 4041 | `		}` |
|     13 | 4042 | `		SyBlobRelease(&sSub);` |
|     13 | 4043 | `		SySetRelease(&aSub);` |
|     13 | 4044 | `		if( rc == SXRET_OK ){` |
|     13 | 4045 | `			GlobSort(pHit,pSet,nStart);` |
|      6 | 4046 | `		}` |
|     13 | 4047 | `		return rc;` |
|      - | 4048 | `	}` |
|    467 | 4049 | `	for( i = nPat - 1 ; i >= 0 ; --i ){` |
|    467 | 4050 | `		if( zPat[i] == '/' ){` |
|     93 | 4051 | `			iSlash = i;` |
|     93 | 4052 | `			break;` |
|      - | 4053 | `		}` |
|    188 | 4054 | `	}` |
|     93 | 4055 | `	if( iSlash > 0 && GlobHasMeta(zPat,iSlash) ){` |
|      - | 4056 | `		/* A wildcard in the DIRECTORY part is matched level by level, which is` |
|      - | 4057 | `		 * what glob(3) does: list the directories that part names, then glob` |
|      - | 4058 | `		 * the last component inside each. Reading only the last component` |
|      - | 4059 | ``		 * answers [] for `src/` + `*` + `/` + `*.php`, the everyday two-level`` |
|      - | 4060 | `		 * spelling, and for every deeper one. */` |
|     11 | 4061 | `		SyBlobInit(&sSub,&pVm->sAllocator);` |
|     11 | 4062 | `		SySetInit(&aSub,&pVm->sAllocator,sizeof(glob_hit));` |
|     11 | 4063 | `		rc = GlobExpand(pVm,zPat,iSlash+1,FALSE,&sSub,&aSub);` |
|     11 | 4064 | `		if( rc == SXRET_OK ){` |
|      - | 4065 | `			SyBlob sJoin;` |
|     11 | 4066 | `			SyBlobInit(&sJoin,&pVm->sAllocator);` |
|     11 | 4067 | `			aRec = (glob_hit *)SySetBasePtr(&aSub);` |
|     11 | 4068 | `			nRec = SySetUsed(&aSub);` |
|     31 | 4069 | `			for( n = 0 ; n < nRec ; ++n ){` |
|     21 | 4070 | `				SyBlobReset(&sJoin);` |
|     20 | 4071 | `				if( SyBlobAppend(&sJoin,` |
|     30 | 4072 | `					&((const char *)SyBlobData(&sSub))[aRec[n].nOfs],aRec[n].nLen) != SXRET_OK` |
|     21 | 4073 | `				 \|\| SyBlobAppend(&sJoin,&zPat[iSlash+1],(sxu32)(nPat - iSlash - 1)) != SXRET_OK ){` |
|    ! 0 | 4074 | `					rc = SXERR_MEM;` |
|    ! 0 | 4075 | `					break;` |
|      - | 4076 | `				}` |
|     31 | 4077 | `				rc = GlobExpand(pVm,(const char *)SyBlobData(&sJoin),` |
|     20 | 4078 | `					(int)SyBlobLength(&sJoin),bOnlyDir,pHit,pSet);` |
|     21 | 4079 | `				if( rc != SXRET_OK ){` |
|    ! 0 | 4080 | `					break;` |
|      - | 4081 | `				}` |
|     11 | 4082 | `			}` |
|     11 | 4083 | `			SyBlobRelease(&sJoin);` |
|      5 | 4084 | `		}` |
|     11 | 4085 | `		SyBlobRelease(&sSub);` |
|     11 | 4086 | `		SySetRelease(&aSub);` |
|     11 | 4087 | `		if( rc == SXRET_OK ){` |
|     11 | 4088 | `			GlobSort(pHit,pSet,nStart);` |
|      5 | 4089 | `		}` |
|     11 | 4090 | `		return rc;` |
|      - | 4091 | `	}` |
|     83 | 4092 | `	rc = GlobLeaf(pVm,zPat,nPat,bOnlyDir,pHit,pSet);` |
|     83 | 4093 | `	if( rc == SXRET_OK ){` |
|     83 | 4094 | `		GlobSort(pHit,pSet,nStart);` |
|     41 | 4095 | `	}` |
|     83 | 4096 | `	return rc;` |
|     53 | 4097 | `}` |
|      - | 4098 | `/*` |
|      - | 4099 | ` * Every entry of one directory, in the shape above. This is php's` |
|      - | 4100 | `` * `php_stream_scandir` with its alphasort comparator, which ZipArchive::`` |
|      - | 4101 | ``  * addPattern() walks: unlike a glob it keeps the DOTTED names -- `.` and `..` `` |
|      - | 4102 | ` * among them -- because php's caller is the one that decides what to do with` |
|      - | 4103 | ` * them, and it decides by STATTING each rather than by looking at the name.` |
|      - | 4104 | ` */` |
|    ! 0 | 4105 | `PH7_PRIVATE sxi32 PH7_VfsListDir(ph7_vm *pVm,const char *zDir,int nDir,SyBlob *pHit,SySet *pSet)` |
|    ! 0 | 4106 | `{` |
|      - | 4107 | `	const ph7_io_stream *pStream;` |
|      - | 4108 | `	const char *zDev;` |
|      - | 4109 | `	void *pHandle;` |
|      - | 4110 | `	ph7_value sEntry;` |
|      - | 4111 | `	ph7_context sCtx;` |
|      - | 4112 | `	char zPath[PH7_GLOB_PATH_MAX];` |
|    ! 0 | 4113 | `	sxu32 nStart = SySetUsed(pSet);` |
|    ! 0 | 4114 | `	sxi32 rc = SXRET_OK;` |
|    ! 0 | 4115 | `	if( nDir < 1 \|\| nDir >= (int)sizeof(zPath) ){` |
|    ! 0 | 4116 | `		return SXERR_INVALID;` |
|      - | 4117 | `	}` |
|    ! 0 | 4118 | `	SyMemcpy(zDir,zPath,(sxu32)nDir);` |
|    ! 0 | 4119 | `	zPath[nDir] = 0;` |
|    ! 0 | 4120 | `	zDev = zPath;` |
|    ! 0 | 4121 | `	pStream = PH7_VmGetStreamDevice(&(*pVm),&zDev,nDir);` |
|    ! 0 | 4122 | `	if( pStream == 0 \|\| pStream->xOpenDir == 0 \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 4123 | `		return SXERR_IO;` |
|      - | 4124 | `	}` |
|    ! 0 | 4125 | `	PH7_MemObjInit(pVm,&sEntry);` |
|    ! 0 | 4126 | `	if( pStream->xOpenDir(zDev,&sEntry,&pHandle) != PH7_OK ){` |
|    ! 0 | 4127 | `		PH7_MemObjRelease(&sEntry);` |
|    ! 0 | 4128 | `		return SXERR_IO;` |
|      - | 4129 | `	}` |
|    ! 0 | 4130 | `	VmInitCallContext(&sCtx,pVm,0,&sEntry,0);` |
|    ! 0 | 4131 | `	for(;;){` |
|      - | 4132 | `		const char *zName;` |
|    ! 0 | 4133 | `		int nName = 0;` |
|      - | 4134 | `		PH7_GlobHit sHit;` |
|    ! 0 | 4135 | `		ph7_value_reset_string_cursor(&sEntry);` |
|    ! 0 | 4136 | `		if( pStream->xReadDir(pHandle,&sCtx) != PH7_OK ){` |
|    ! 0 | 4137 | `			break;` |
|      - | 4138 | `		}` |
|    ! 0 | 4139 | `		zName = ph7_value_to_string(&sEntry,&nName);` |
|    ! 0 | 4140 | `		if( nName < 1 ){` |
|    ! 0 | 4141 | `			continue;` |
|      - | 4142 | `		}` |
|    ! 0 | 4143 | `		sHit.nOfs = SyBlobLength(pHit);` |
|    ! 0 | 4144 | `		sHit.nLen = (sxu32)nName;` |
|    ! 0 | 4145 | `		SyBlobAppend(pHit,zName,(sxu32)nName);` |
|    ! 0 | 4146 | `		rc = SySetPut(pSet,(const void *)&sHit);` |
|    ! 0 | 4147 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4148 | `			break;` |
|      - | 4149 | `		}` |
|    ! 0 | 4150 | `	}` |
|    ! 0 | 4151 | `	VmReleaseCallContext(&sCtx);` |
|    ! 0 | 4152 | `	PH7_MemObjRelease(&sEntry);` |
|    ! 0 | 4153 | `	if( pStream->xCloseDir ){` |
|    ! 0 | 4154 | `		pStream->xCloseDir(pHandle);` |
|    ! 0 | 4155 | `	}` |
|    ! 0 | 4156 | `	if( rc == SXRET_OK ){` |
|    ! 0 | 4157 | `		GlobSort(pHit,pSet,nStart);` |
|    ! 0 | 4158 | `	}` |
|    ! 0 | 4159 | `	return rc;` |
|    ! 0 | 4160 | `}` |
|      - | 4161 | `/* void (*xCloseDir)(void *) */` |
|     62 | 4162 | `static void GlobStream_CloseDir(void *pHandle)` |
|      1 | 4163 | `{` |
|     63 | 4164 | `	glob_stream *pGlob = (glob_stream *)pHandle;` |
|      - | 4165 | `	ph7_vm *pVm;` |
|     63 | 4166 | `	if( pGlob == 0 ){` |
|    ! 0 | 4167 | `		return;` |
|      - | 4168 | `	}` |
|     63 | 4169 | `	pVm = pGlob->pVm;` |
|     63 | 4170 | `	SyBlobRelease(&pGlob->sHit);` |
|     63 | 4171 | `	SyBlobRelease(&pGlob->sDir);` |
|     63 | 4172 | `	SySetRelease(&pGlob->aHit);` |
|     63 | 4173 | `	SyMemBackendFree(&pVm->sAllocator,pGlob);` |
|     32 | 4174 | `}` |
|      - | 4175 | `/*` |
|      - | 4176 | ` * int (*xOpenDir)(const char *,ph7_value *,void **)` |
|      - | 4177 | ` *` |
|      - | 4178 | ` * php's opener fails only on a glob(3) ERROR: no matches at all is an OPEN` |
|      - | 4179 | `` * stream with nothing in it, which is why `new GlobIterator('nope/' . '*')` is a`` |
|      - | 4180 | ` * working object whose count() is 0 rather than a constructor that throws.` |
|      - | 4181 | ` *` |
|      - | 4182 | ` * The VM comes in through the context argument, the way data:// takes it: the` |
|      - | 4183 | ` * walk allocates, and reads a directory through a call context of its own.` |
|      - | 4184 | ` */` |
|     62 | 4185 | `static int GlobStream_OpenDir(const char *zPattern,ph7_value *pResource,void **ppHandle)` |
|      1 | 4186 | `{` |
|     63 | 4187 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|      - | 4188 | `	glob_stream *pGlob;` |
|     63 | 4189 | `	if( pVm == 0 ){` |
|    ! 0 | 4190 | `		return -1;` |
|      - | 4191 | `	}` |
|     63 | 4192 | `	pGlob = (glob_stream *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(glob_stream));` |
|     63 | 4193 | `	if( pGlob == 0 ){` |
|    ! 0 | 4194 | `		return -1;` |
|      - | 4195 | `	}` |
|     63 | 4196 | `	pGlob->pVm = pVm;` |
|     63 | 4197 | `	pGlob->nCur = 0;` |
|     63 | 4198 | `	SyBlobInit(&pGlob->sHit,&pVm->sAllocator);` |
|     63 | 4199 | `	SyBlobInit(&pGlob->sDir,&pVm->sAllocator);` |
|     63 | 4200 | `	SySetInit(&pGlob->aHit,&pVm->sAllocator,sizeof(glob_hit));` |
|      - | 4201 | `	/* A pattern longer than one path is zero matches rather than a refusal (it` |
|      - | 4202 | `	 * can name nothing that exists), which is glob(3)'s GLOB_NOMATCH and an` |
|      - | 4203 | `	 * open stream either way. */` |
|     93 | 4204 | `	if( GlobExpand(pVm,zPattern,(int)SyStrlen(zPattern),FALSE,` |
|     63 | 4205 | `		&pGlob->sHit,&pGlob->aHit) != SXRET_OK ){` |
|    ! 0 | 4206 | `		GlobStream_CloseDir(pGlob);` |
|    ! 0 | 4207 | `		return -1;` |
|      - | 4208 | `	}` |
|     63 | 4209 | `	*ppHandle = (void *)pGlob;` |
|     63 | 4210 | `	return PH7_OK;` |
|     32 | 4211 | `}` |
|      - | 4212 | `/*` |
|      - | 4213 | ` * int (*xReadDir)(void *,ph7_context *)` |
|      - | 4214 | ` *` |
|      - | 4215 | ` * php's php_glob_stream_path_split, which runs on every read: the directory is` |
|      - | 4216 | ` * everything before the LAST slash and the ENTRY is what follows it. So a` |
|      - | 4217 | ``  * match with no slash in it reports an empty directory and itself, `a/` `` |
|      - | 4218 | `` * reports `a` and an EMPTY entry -- and an empty entry is what the SPL walk`` |
|      - | 4219 | ` * reads as the end, which is why GlobIterator over a trailing-slash pattern` |
|      - | 4220 | ` * counts its matches and yields none of them.` |
|      - | 4221 | ` */` |
|    208 | 4222 | `static int GlobStream_ReadDir(void *pHandle,ph7_context *pCtx)` |
|      1 | 4223 | `{` |
|    209 | 4224 | `	glob_stream *pGlob = (glob_stream *)pHandle;` |
|      - | 4225 | `	glob_hit *aHit;` |
|      - | 4226 | `	const char *zPath;` |
|      - | 4227 | `	sxu32 nPath;` |
|    209 | 4228 | `	int i,iSlash = -1;` |
|    209 | 4229 | `	if( pGlob == 0 ){` |
|    ! 0 | 4230 | `		return -1;` |
|      - | 4231 | `	}` |
|    209 | 4232 | `	if( pGlob->nCur >= SySetUsed(&pGlob->aHit) ){` |
|      - | 4233 | `		/* php drops the path when the walk runs out, and counts on past it. */` |
|     49 | 4234 | `		pGlob->nCur++;` |
|     49 | 4235 | `		SyBlobReset(&pGlob->sDir);` |
|     49 | 4236 | `		return -1;` |
|      - | 4237 | `	}` |
|    161 | 4238 | `	aHit = (glob_hit *)SySetBasePtr(&pGlob->aHit);` |
|    161 | 4239 | `	zPath = &((const char *)SyBlobData(&pGlob->sHit))[aHit[pGlob->nCur].nOfs];` |
|    161 | 4240 | `	nPath = aHit[pGlob->nCur].nLen;` |
|    161 | 4241 | `	pGlob->nCur++;` |
|    891 | 4242 | `	for( i = (int)nPath - 1 ; i >= 0 ; --i ){` |
|    891 | 4243 | `		if( zPath[i] == '/' ){` |
|    161 | 4244 | `			iSlash = i;` |
|    161 | 4245 | `			break;` |
|      - | 4246 | `		}` |
|    366 | 4247 | `	}` |
|    161 | 4248 | `	SyBlobReset(&pGlob->sDir);` |
|    161 | 4249 | `	if( iSlash >= 0 ){` |
|    161 | 4250 | `		if( iSlash > 0 && SyBlobAppend(&pGlob->sDir,zPath,(sxu32)iSlash) != SXRET_OK ){` |
|    ! 0 | 4251 | `			return -1;` |
|      - | 4252 | `		}` |
|    161 | 4253 | `		ph7_result_string(pCtx,&zPath[iSlash+1],(int)nPath - iSlash - 1);` |
|     81 | 4254 | `	}else{` |
|    ! 0 | 4255 | `		ph7_result_string(pCtx,zPath,(int)nPath);` |
|      - | 4256 | `	}` |
|    161 | 4257 | `	return PH7_OK;` |
|    105 | 4258 | `}` |
|      - | 4259 | `/* void (*xRewindDir)(void *): php's rewind moves the INDEX and leaves the path` |
|      - | 4260 | ` * where the last read put it -- every caller reads straight afterwards. */` |
|     12 | 4261 | `static void GlobStream_RewindDir(void *pHandle)` |
|      1 | 4262 | `{` |
|     13 | 4263 | `	glob_stream *pGlob = (glob_stream *)pHandle;` |
|     13 | 4264 | `	if( pGlob ){` |
|     13 | 4265 | `		pGlob->nCur = 0;` |
|      6 | 4266 | `	}` |
|     13 | 4267 | `}` |
|      - | 4268 | `PH7_PRIVATE const ph7_io_stream sGLOB_Stream = {` |
|      - | 4269 | `	"glob",` |
|      - | 4270 | `	PH7_IO_STREAM_VERSION,` |
|      - | 4271 | `	0,                    /* xOpen: php's wrapper has no stream opener at all */` |
|      - | 4272 | `	GlobStream_OpenDir,   /* xOpenDir */` |
|      - | 4273 | `	0,                    /* xClose */` |
|      - | 4274 | `	GlobStream_CloseDir,  /* xCloseDir */` |
|      - | 4275 | `	0,                    /* xRead */` |
|      - | 4276 | `	GlobStream_ReadDir,   /* xReadDir */` |
|      - | 4277 | `	0,                    /* xWrite */` |
|      - | 4278 | `	0,                    /* xSeek */` |
|      - | 4279 | `	0,                    /* xLock */` |
|      - | 4280 | `	GlobStream_RewindDir, /* xRewindDir */` |
|      - | 4281 | `	0,                    /* xTell */` |
|      - | 4282 | `	0,                    /* xTrunc */` |
|      - | 4283 | `	0,                    /* xSync */` |
|      - | 4284 | `	0                     /* xStat */` |
|      - | 4285 | `};` |
|      - | 4286 | `/* Is this the glob device? php's php_stream_is(), which is how SPL tells a` |
|      - | 4287 | ` * GlobIterator's directory handle from an ordinary one. */` |
|    195 | 4288 | `PH7_PRIVATE int PH7_GlobStreamIs(const ph7_io_stream *pStream)` |
|      2 | 4289 | `{` |
|    197 | 4290 | `	return pStream == &sGLOB_Stream;` |
|      2 | 4291 | `}` |
|      - | 4292 | `/* php's php_glob_stream_get_path: the directory of the CURRENT match, empty` |
|      - | 4293 | ` * both before the first read and after the last. */` |
|     60 | 4294 | `PH7_PRIVATE const char * PH7_GlobStreamPath(void *pHandle,int *pnLen)` |
|      1 | 4295 | `{` |
|     61 | 4296 | `	glob_stream *pGlob = (glob_stream *)pHandle;` |
|     61 | 4297 | `	if( pGlob == 0 ){` |
|    ! 0 | 4298 | `		*pnLen = 0;` |
|    ! 0 | 4299 | `		return "";` |
|      - | 4300 | `	}` |
|     61 | 4301 | `	*pnLen = (int)SyBlobLength(&pGlob->sDir);` |
|     61 | 4302 | `	return *pnLen > 0 ? (const char *)SyBlobData(&pGlob->sDir) : "";` |
|     31 | 4303 | `}` |
|      - | 4304 | `/* php's php_glob_stream_get_count: what GlobIterator::count() answers, and it` |
|      - | 4305 | ` * does not move with the walk. */` |
|     14 | 4306 | `PH7_PRIVATE sxi64 PH7_GlobStreamCount(void *pHandle)` |
|      1 | 4307 | `{` |
|     15 | 4308 | `	glob_stream *pGlob = (glob_stream *)pHandle;` |
|     15 | 4309 | `	return pGlob ? (sxi64)SySetUsed(&pGlob->aHit) : 0;` |
|      1 | 4310 | `}` |
|      - | 4311 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 4312 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 4313 | `/*` |
|      - | 4314 | ` * bool link(string $target,string $link)` |
|      - | 4315 |  |
|      - | 4316 | ` *  Create a hard link.` |
|      - | 4317 | ` * Parameters` |
|      - | 4318 | ` *  $target` |
|      - | 4319 | ` *   Target of the link.` |
|      - | 4320 | ` *  $link` |
|      - | 4321 | ` *   The link name.` |
|      - | 4322 | ` * Return` |
|      - | 4323 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4324 | ` */` |
|      8 | 4325 | `static int PH7_vfs_link(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4326 | `{` |
|      - | 4327 | `	const char *zTarget,*zLink;` |
|      - | 4328 | `	ph7_vfs *pVfs;` |
|      - | 4329 | `	int rc;` |
|      9 | 4330 | `	if( nArg < 2 \|\| !VfsPathArgUsable(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 4331 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4332 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4333 | `		return PH7_OK;` |
|      - | 4334 | `	}` |
|      - | 4335 | `	/* Point to the underlying vfs */` |
|      9 | 4336 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 | 4337 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 4338 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 4339 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4340 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 4341 | `			ph7_function_name(pCtx)` |
|      - | 4342 | `			);` |
|    ! 0 | 4343 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4344 | `		return PH7_OK;` |
|      - | 4345 | `	}` |
|      - | 4346 | `	/* Extract the given arguments */` |
|      9 | 4347 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|      9 | 4348 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 4349 | `	/* Perform the requested operation */` |
|      9 | 4350 | `	errno = 0;` |
|      9 | 4351 | `	rc = pVfs->xLink(zTarget,zLink,0/*Not a symbolic link */);` |
|      9 | 4352 | `	if( rc != PH7_OK ){` |
|      - | 4353 | `		/* php's no-path shape again, the same chmod() takes. PHL answered FALSE` |
|      - | 4354 | `		 * in silence, so a failed link was indistinguishable from a made one` |
|      - | 4355 | `		 * without testing the return value. */` |
|      6 | 4356 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      4 | 4357 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|      2 | 4358 | `	}` |
|      - | 4359 | `	/* IO result */` |
|      9 | 4360 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|      9 | 4361 | `	return PH7_OK;` |
|      5 | 4362 | `}` |
|      - | 4363 | `/*` |
|      - | 4364 | ` * string\|false readlink(string $path)` |
|      - | 4365 | ` *  Returns the target of a symbolic link.` |
|      - | 4366 | ` * Parameters` |
|      - | 4367 | ` *  $path` |
|      - | 4368 | ` *   The symbolic link path.` |
|      - | 4369 | ` * Return` |
|      - | 4370 | ` *  The contents of the link, or FALSE (with a warning) when $path is not a link` |
|      - | 4371 | ` *  or cannot be read -- php's own answer, error text included.` |
|      - | 4372 | ` */` |
|      8 | 4373 | `static int PH7_vfs_readlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 4374 | `{` |
|      - | 4375 | `	const char *zPath;` |
|      - | 4376 | `	ph7_vfs *pVfs;` |
|      - | 4377 | `	int rc;` |
|      8 | 4378 | `	if( nArg < 1 \|\| !VfsPathArgUsable(apArg[0]) ){` |
|    ! 0 | 4379 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4380 | `		return PH7_OK;` |
|      - | 4381 | `	}` |
|      8 | 4382 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      8 | 4383 | `	if( pVfs == 0 \|\| pVfs->xReadlink == 0 ){` |
|    ! 0 | 4384 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4385 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 4386 | `			ph7_function_name(pCtx)` |
|      - | 4387 | `			);` |
|    ! 0 | 4388 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4389 | `		return PH7_OK;` |
|      - | 4390 | `	}` |
|      8 | 4391 | `	zPath = ph7_value_to_string(apArg[0],0);` |
|      8 | 4392 | `	rc = pVfs->xReadlink(zPath,pCtx);` |
|      8 | 4393 | `	if( rc != PH7_OK ){` |
|      - | 4394 | `		/* php's wording is the errno text alone -- the engine prefixes the` |
|      - | 4395 | `		 * function name already. */` |
|      6 | 4396 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      4 | 4397 | `			"%s",VfsStrerror(errno));` |
|      4 | 4398 | `		ph7_result_bool(pCtx,0);` |
|      2 | 4399 | `	}` |
|      8 | 4400 | `	return PH7_OK;` |
|      4 | 4401 | `}` |
|      - | 4402 | `/*` |
|      - | 4403 | ` * bool symlink(string $target,string $link)` |
|      - | 4404 | ` *  Creates a symbolic link.` |
|      - | 4405 | ` * Parameters` |
|      - | 4406 | ` *  $target` |
|      - | 4407 | ` *   Target of the link.` |
|      - | 4408 | ` *  $link` |
|      - | 4409 | ` *   The link name.` |
|      - | 4410 | ` * Return` |
|      - | 4411 | ` *  TRUE on success or FALSE on failure.` |
|      - | 4412 | ` */` |
|     14 | 4413 | `static int PH7_vfs_symlink(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4414 | `{` |
|      - | 4415 | `	const char *zTarget,*zLink;` |
|      - | 4416 | `	ph7_vfs *pVfs;` |
|      - | 4417 | `	int rc;` |
|     15 | 4418 | `	if( nArg < 2 \|\| !VfsPathArgUsable(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|      - | 4419 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 4420 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4421 | `		return PH7_OK;` |
|      - | 4422 | `	}` |
|      - | 4423 | `	/* Point to the underlying vfs */` |
|     15 | 4424 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|     15 | 4425 | `	if( pVfs == 0 \|\| pVfs->xLink == 0 ){` |
|      - | 4426 | `		/* IO routine not implemented,return NULL */` |
|    ! 0 | 4427 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4428 | `			"IO routine(%s) not implemented in the underlying VFS,PH7 is returning FALSE",` |
|    ! 0 | 4429 | `			ph7_function_name(pCtx)` |
|      - | 4430 | `			);` |
|    ! 0 | 4431 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4432 | `		return PH7_OK;` |
|      - | 4433 | `	}` |
|      - | 4434 | `	/* Extract the given arguments */` |
|     15 | 4435 | `	zTarget  = ph7_value_to_string(apArg[0],0);` |
|     15 | 4436 | `	zLink = ph7_value_to_string(apArg[1],0);` |
|      - | 4437 | `	/* Perform the requested operation */` |
|     15 | 4438 | `	errno = 0;` |
|     15 | 4439 | `	rc = pVfs->xLink(zTarget,zLink,1/*A symbolic link */);` |
|     15 | 4440 | `	if( rc != PH7_OK ){` |
|      - | 4441 | `		/* php's no-path shape again, the same chmod() takes. PHL answered FALSE` |
|      - | 4442 | `		 * in silence, so a failed link was indistinguishable from a made one` |
|      - | 4443 | `		 * without testing the return value. */` |
|      3 | 4444 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): %s",` |
|      2 | 4445 | `			ph7_function_name(pCtx),VfsStrerror(errno));` |
|      1 | 4446 | `	}` |
|      - | 4447 | `	/* IO result */` |
|     15 | 4448 | `	ph7_result_bool(pCtx,rc == PH7_OK );` |
|     15 | 4449 | `	return PH7_OK;` |
|      8 | 4450 | `}` |
|      - | 4451 | `/*` |
|      - | 4452 | ` * int umask([ int $mask ])` |
|      - | 4453 | ` *  Changes the current umask.` |
|      - | 4454 | ` * Parameters` |
|      - | 4455 | ` *  $mask` |
|      - | 4456 | ` *   The new umask.` |
|      - | 4457 | ` * Return` |
|      - | 4458 | ` *  umask() without arguments simply returns the current umask.` |
|      - | 4459 | ` *  Otherwise the old umask is returned.` |
|      - | 4460 | ` */` |
|      8 | 4461 | `static int PH7_vfs_umask(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4462 | `{` |
|      - | 4463 | `	int iOld,iNew;` |
|      - | 4464 | `	ph7_vfs *pVfs;` |
|      - | 4465 | `	/* Point to the underlying vfs */` |
|      9 | 4466 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      9 | 4467 | `	if( pVfs == 0 \|\| pVfs->xUmask == 0 ){` |
|      - | 4468 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 4469 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4470 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 4471 | `			ph7_function_name(pCtx)` |
|      - | 4472 | `			);` |
|    ! 0 | 4473 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4474 | `		return PH7_OK;` |
|      - | 4475 | `	}` |
|      9 | 4476 | `	iNew = 0;` |
|      9 | 4477 | `	if( nArg > 0 ){` |
|      5 | 4478 | `		iNew = ph7_value_to_int(apArg[0]);` |
|      2 | 4479 | `	}` |
|      - | 4480 | `	/* Perform the requested operation */` |
|      9 | 4481 | `	iOld = pVfs->xUmask(iNew);` |
|      - | 4482 | `	/* Old mask */` |
|      9 | 4483 | `	ph7_result_int(pCtx,iOld);` |
|      9 | 4484 | `	return PH7_OK;` |
|      5 | 4485 | `}` |
|      - | 4486 | `/*` |
|      - | 4487 | ` * string sys_get_temp_dir()` |
|      - | 4488 | ` *  Returns directory path used for temporary files.` |
|      - | 4489 | ` * Parameters` |
|      - | 4490 | ` *  None` |
|      - | 4491 | ` * Return` |
|      - | 4492 | ` *  Returns the path of the temporary directory.` |
|      - | 4493 | ` */` |
|   1538 | 4494 | `static int PH7_vfs_sys_get_temp_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 4495 | `{` |
|      - | 4496 | `	ph7_vfs *pVfs;` |
|      - | 4497 | `	/* Set the empty string as the default return value */` |
|   1543 | 4498 | `	ph7_result_string(pCtx,"",0);` |
|      - | 4499 | `	/* Point to the underlying vfs */` |
|   1543 | 4500 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|   1543 | 4501 | `	if( pVfs == 0 \|\| pVfs->xTempDir == 0 ){` |
|    ! 0 | 4502 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 4503 | `		SXUNUSED(apArg);` |
|      - | 4504 | `		/* IO routine not implemented,return "" */` |
|    ! 0 | 4505 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4506 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 4507 | `			ph7_function_name(pCtx)` |
|      - | 4508 | `			);` |
|    ! 0 | 4509 | `		return PH7_OK;` |
|      - | 4510 | `	}` |
|      - | 4511 | `	/* Perform the requested operation */` |
|   1543 | 4512 | `	pVfs->xTempDir(pCtx);` |
|   1543 | 4513 | `	return PH7_OK;` |
|    772 | 4514 | `}` |
|      - | 4515 | `/*` |
|      - | 4516 | ` * string get_current_user()` |
|      - | 4517 | ` *  Returns the name of the current working user.` |
|      - | 4518 | ` * Parameters` |
|      - | 4519 | ` *  None` |
|      - | 4520 | ` * Return` |
|      - | 4521 | ` *  Returns the name of the current working user.` |
|      - | 4522 | ` */` |
|      2 | 4523 | `static int PH7_vfs_get_current_user(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4524 | `{` |
|      - | 4525 | `	ph7_vfs *pVfs;` |
|      - | 4526 | `	/* Point to the underlying vfs */` |
|      3 | 4527 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      3 | 4528 | `	if( pVfs == 0 \|\| pVfs->xUsername == 0 ){` |
|    ! 0 | 4529 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 4530 | `		SXUNUSED(apArg);` |
|      - | 4531 | `		/* IO routine not implemented */` |
|    ! 0 | 4532 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4533 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 4534 | `			ph7_function_name(pCtx)` |
|      - | 4535 | `			);` |
|      - | 4536 | `		/* Set a dummy username */` |
|    ! 0 | 4537 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|    ! 0 | 4538 | `		return PH7_OK;` |
|      - | 4539 | `	}` |
|      - | 4540 | `	/* Perform the requested operation */` |
|      3 | 4541 | `	pVfs->xUsername(pCtx);` |
|      3 | 4542 | `	return PH7_OK;` |
|      2 | 4543 | `}` |
|      - | 4544 | `/*` |
|      - | 4545 | ` * int64 getmypid()` |
|      - | 4546 | ` *  Gets process ID.` |
|      - | 4547 | ` * Parameters` |
|      - | 4548 | ` *  None` |
|      - | 4549 | ` * Return` |
|      - | 4550 | ` *  Returns the process ID.` |
|      - | 4551 | ` */` |
|    380 | 4552 | `static int PH7_vfs_getmypid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 4553 | `{` |
|      - | 4554 | `	ph7_int64 nProcessId;` |
|      - | 4555 | `	ph7_vfs *pVfs;` |
|      - | 4556 | `	/* Point to the underlying vfs */` |
|    385 | 4557 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|    385 | 4558 | `	if( pVfs == 0 \|\| pVfs->xProcessId == 0 ){` |
|    ! 0 | 4559 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 4560 | `		SXUNUSED(apArg);` |
|      - | 4561 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 4562 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4563 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 4564 | `			ph7_function_name(pCtx)` |
|      - | 4565 | `			);` |
|    ! 0 | 4566 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 4567 | `		return PH7_OK;` |
|      - | 4568 | `	}` |
|      - | 4569 | `	/* Perform the requested operation */` |
|    385 | 4570 | `	nProcessId = (ph7_int64)pVfs->xProcessId();` |
|      - | 4571 | `	/* Set the result */` |
|    385 | 4572 | `	ph7_result_int64(pCtx,nProcessId);` |
|    385 | 4573 | `	return PH7_OK;` |
|    194 | 4574 | `}` |
|      - | 4575 | `/*` |
|      - | 4576 | ` * int getmyuid()` |
|      - | 4577 | ` *  Get user ID.` |
|      - | 4578 | ` * Parameters` |
|      - | 4579 | ` *  None` |
|      - | 4580 | ` * Return` |
|      - | 4581 | ` *  Returns the user ID.` |
|      - | 4582 | ` */` |
|      6 | 4583 | `static int PH7_vfs_getmyuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4584 | `{` |
|      - | 4585 | `	ph7_vfs *pVfs;` |
|      - | 4586 | `	int nUid;` |
|      - | 4587 | `	/* Point to the underlying vfs */` |
|      7 | 4588 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      7 | 4589 | `	if( pVfs == 0 \|\| pVfs->xUid == 0 ){` |
|    ! 0 | 4590 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 4591 | `		SXUNUSED(apArg);` |
|      - | 4592 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 4593 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4594 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 4595 | `			ph7_function_name(pCtx)` |
|      - | 4596 | `			);` |
|    ! 0 | 4597 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 4598 | `		return PH7_OK;` |
|      - | 4599 | `	}` |
|      - | 4600 | `	/* Perform the requested operation */` |
|      7 | 4601 | `	nUid = pVfs->xUid();` |
|      - | 4602 | `	/* Set the result */` |
|      7 | 4603 | `	ph7_result_int(pCtx,nUid);` |
|      7 | 4604 | `	return PH7_OK;` |
|      4 | 4605 | `}` |
|      - | 4606 | `/*` |
|      - | 4607 | ` * int getmygid()` |
|      - | 4608 | ` *  Get group ID.` |
|      - | 4609 | ` * Parameters` |
|      - | 4610 | ` *  None` |
|      - | 4611 | ` * Return` |
|      - | 4612 | ` *  Returns the group ID.` |
|      - | 4613 | ` */` |
|      4 | 4614 | `static int PH7_vfs_getmygid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4615 | `{` |
|      - | 4616 | `	ph7_vfs *pVfs;` |
|      - | 4617 | `	int nGid;` |
|      - | 4618 | `	/* Point to the underlying vfs */` |
|      5 | 4619 | `	pVfs = (ph7_vfs *)ph7_context_user_data(pCtx);` |
|      5 | 4620 | `	if( pVfs == 0 \|\| pVfs->xGid == 0 ){` |
|    ! 0 | 4621 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 4622 | `		SXUNUSED(apArg);` |
|      - | 4623 | `		/* IO routine not implemented,return -1 */` |
|    ! 0 | 4624 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 4625 | `			"IO routine(%s) not implemented in the underlying VFS",` |
|    ! 0 | 4626 | `			ph7_function_name(pCtx)` |
|      - | 4627 | `			);` |
|    ! 0 | 4628 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 4629 | `		return PH7_OK;` |
|      - | 4630 | `	}` |
|      - | 4631 | `	/* Perform the requested operation */` |
|      5 | 4632 | `	nGid = pVfs->xGid();` |
|      - | 4633 | `	/* Set the result */` |
|      5 | 4634 | `	ph7_result_int(pCtx,nGid);` |
|      5 | 4635 | `	return PH7_OK;` |
|      3 | 4636 | `}` |
|      - | 4637 | `#ifdef __WINNT__` |
|      - | 4638 | `#include <Windows.h>` |
|      - | 4639 | `#elif defined(__UNIXES__)` |
|      - | 4640 | `#include <sys/utsname.h>` |
|      - | 4641 | `#endif` |
|      - | 4642 | `/*` |
|      - | 4643 | ` * php's five uname FIELDS, filled once per call. php answers one of them for a` |
|      - | 4644 | `` * single-letter mode and all five, space-separated, for `a` -- and the ORDER of`` |
|      - | 4645 | `` * that composite is `s n r v m`, the OS, the HOST, the release, the version and`` |
|      - | 4646 | `` * the machine. This engine used to answer `s r v n m`, so the host name stood`` |
|      - | 4647 | ` * in the version's place in every string a program logged.` |
|      - | 4648 | ` */` |
|      - | 4649 | `typedef struct vfs_uname_fields vfs_uname_fields;` |
|      - | 4650 | `struct vfs_uname_fields {` |
|      - | 4651 | `	const char *zSys;     /* 's' */` |
|      - | 4652 | `	const char *zNode;    /* 'n' */` |
|      - | 4653 | `	const char *zRel;     /* 'r' */` |
|      - | 4654 | `	const char *zVer;     /* 'v' */` |
|      - | 4655 | `	const char *zMachine; /* 'm' */` |
|      - | 4656 | `};` |
|      - | 4657 | `#if defined(__WINNT__)` |
|      - | 4658 | `/*` |
|      - | 4659 | ` * The product NAME php prints inside the version field. php reads the real` |
|      - | 4660 | ` * version through ntdll's RtlGetVersion (GetVersionEx() lies to any binary` |
|      - | 4661 | ` * without a compatibility manifest -- it answers 6.2 on Windows 10 and 11) and` |
|      - | 4662 | ` * names it from a table of its own. The rows below are the versions this port` |
|      - | 4663 | ` * targets and the only ones an oracle can be asked about; anything older` |
|      - | 4664 | ` * answers the bare "Windows" rather than a name nobody can verify.` |
|      - | 4665 | ` */` |
|      - | 4666 | `static const char * VfsWinProductName(unsigned long nMajor,unsigned long nMinor,` |
|      - | 4667 | `	unsigned long nBuild,int bWorkstation)` |
|      1 | 4668 | `{` |
|      1 | 4669 | `	if( nMajor == 10 && nMinor == 0 ){` |
|      1 | 4670 | `		if( bWorkstation ){` |
|    ! 0 | 4671 | `			return nBuild >= 22000 ? "Windows 11" : "Windows 10";` |
|      - | 4672 | `		}` |
|      1 | 4673 | `		if( nBuild >= 26100 ){` |
|      1 | 4674 | `			return "Windows Server 2025";` |
|      - | 4675 | `		}` |
|    ! 0 | 4676 | `		if( nBuild >= 20348 ){` |
|    ! 0 | 4677 | `			return "Windows Server 2022";` |
|      - | 4678 | `		}` |
|    ! 0 | 4679 | `		if( nBuild >= 17763 ){` |
|    ! 0 | 4680 | `			return "Windows Server 2019";` |
|      - | 4681 | `		}` |
|    ! 0 | 4682 | `		return "Windows Server 2016";` |
|      - | 4683 | `	}` |
|    ! 0 | 4684 | `	return "Windows";` |
|      1 | 4685 | `}` |
|      - | 4686 | `/*` |
|      - | 4687 | ` * The true OS version. RtlGetVersion is the only call that answers it for an` |
|      - | 4688 | ` * unmanifested binary, and it lives in ntdll rather than in an import library.` |
|      - | 4689 | ` */` |
|      - | 4690 | `static void VfsWinVersion(unsigned long *pnMajor,unsigned long *pnMinor,` |
|      - | 4691 | `	unsigned long *pnBuild,int *pbWorkstation)` |
|      1 | 4692 | `{` |
|      - | 4693 | `	/* RTL_OSVERSIONINFOEXW's documented layout, declared here rather than` |
|      - | 4694 | `	 * taken from a header: the name only appears in some SDK versions, and` |
|      - | 4695 | `	 * nothing else in this file needs ntdll. */` |
|      - | 4696 | `	typedef struct vfs_rtl_osversion {` |
|      - | 4697 | `		ULONG dwOSVersionInfoSize;` |
|      - | 4698 | `		ULONG dwMajorVersion;` |
|      - | 4699 | `		ULONG dwMinorVersion;` |
|      - | 4700 | `		ULONG dwBuildNumber;` |
|      - | 4701 | `		ULONG dwPlatformId;` |
|      - | 4702 | `		WCHAR szCSDVersion[128];` |
|      - | 4703 | `		USHORT wServicePackMajor;` |
|      - | 4704 | `		USHORT wServicePackMinor;` |
|      - | 4705 | `		USHORT wSuiteMask;` |
|      - | 4706 | `		UCHAR wProductType;` |
|      - | 4707 | `		UCHAR wReserved;` |
|      - | 4708 | `	} vfs_rtl_osversion;` |
|      - | 4709 | `	typedef LONG (WINAPI *rtl_get_version)(vfs_rtl_osversion *);` |
|      - | 4710 | `	vfs_rtl_osversion sInfo;` |
|      - | 4711 | `	rtl_get_version xGet;` |
|      - | 4712 | `	HMODULE hNtdll;` |
|      1 | 4713 | `	*pnMajor = 0;` |
|      1 | 4714 | `	*pnMinor = 0;` |
|      1 | 4715 | `	*pnBuild = 0;` |
|      1 | 4716 | `	*pbWorkstation = 1;` |
|      1 | 4717 | `	SyZero(&sInfo,(sxu32)sizeof(sInfo));` |
|      1 | 4718 | `	sInfo.dwOSVersionInfoSize = (ULONG)sizeof(sInfo);` |
|      1 | 4719 | `	hNtdll = GetModuleHandleA("ntdll.dll");` |
|      1 | 4720 | `	if( hNtdll == 0 ){` |
|    ! 0 | 4721 | `		return;` |
|      - | 4722 | `	}` |
|      1 | 4723 | `	xGet = (rtl_get_version)GetProcAddress(hNtdll,"RtlGetVersion");` |
|      1 | 4724 | `	if( xGet == 0 \|\| xGet(&sInfo) != 0 ){` |
|    ! 0 | 4725 | `		return;` |
|      - | 4726 | `	}` |
|      1 | 4727 | `	*pnMajor = (unsigned long)sInfo.dwMajorVersion;` |
|      1 | 4728 | `	*pnMinor = (unsigned long)sInfo.dwMinorVersion;` |
|      1 | 4729 | `	*pnBuild = (unsigned long)sInfo.dwBuildNumber;` |
|      - | 4730 | `	/* VER_NT_WORKSTATION is 1; spelled out so the struct above needs no` |
|      - | 4731 | `	 * header of its own. */` |
|      1 | 4732 | `	*pbWorkstation = (sInfo.wProductType == 1);` |
|      1 | 4733 | `}` |
|      - | 4734 | `/* php's machine field: the NATIVE architecture, named the way php names it. */` |
|      - | 4735 | `static const char * VfsWinMachine(char *zBuf,int nBuf)` |
|      1 | 4736 | `{` |
|      - | 4737 | `	SYSTEM_INFO sInfo;` |
|      1 | 4738 | `	SyZero(&sInfo,(sxu32)sizeof(sInfo));` |
|      1 | 4739 | `	GetNativeSystemInfo(&sInfo);` |
|      1 | 4740 | `	switch( sInfo.wProcessorArchitecture ){` |
|      1 | 4741 | `		case PROCESSOR_ARCHITECTURE_AMD64: return "AMD64";` |
|    ! 0 | 4742 | `		case PROCESSOR_ARCHITECTURE_ARM:   return "ARM";` |
|      - | 4743 | `#ifdef PROCESSOR_ARCHITECTURE_ARM64` |
|    ! 0 | 4744 | `		case PROCESSOR_ARCHITECTURE_ARM64: return "ARM64";` |
|      - | 4745 | `#endif` |
|    ! 0 | 4746 | `		case PROCESSOR_ARCHITECTURE_IA64:  return "IA64";` |
|      - | 4747 | `		case PROCESSOR_ARCHITECTURE_INTEL:` |
|    ! 0 | 4748 | `			SyBufferFormat(zBuf,(sxu32)nBuf,"i%u",(unsigned int)sInfo.dwProcessorType);` |
|    ! 0 | 4749 | `			return zBuf;` |
|      - | 4750 | `		default: break;` |
|      - | 4751 | `	}` |
|    ! 0 | 4752 | `	return "Unknown";` |
|      1 | 4753 | `}` |
|      - | 4754 | `#endif /* __WINNT__ */` |
|      - | 4755 | `/*` |
|      - | 4756 | ` * string php_uname([ string $mode = "a" ])` |
|      - | 4757 | ` *  Returns information about the host operating system.` |
|      - | 4758 | ` * Parameters` |
|      - | 4759 | ` *  $mode` |
|      - | 4760 | `` *   ONE character out of `a m n r s v`; php refuses every other spelling with`` |
|      - | 4761 | ` *   a ValueError, and refuses a longer or empty string with a different one.` |
|      - | 4762 | ` *    'a': the default -- all five fields in the sequence "s n r v m".` |
|      - | 4763 | ` *    's': operating system name.` |
|      - | 4764 | ` *    'n': host name.` |
|      - | 4765 | ` *    'r': release name.` |
|      - | 4766 | ` *    'v': version information.` |
|      - | 4767 | ` *    'm': machine type.` |
|      - | 4768 | ` * Return` |
|      - | 4769 | ` *  The requested field, or all five.` |
|      - | 4770 | ` */` |
|     42 | 4771 | `static int PH7_vfs_ph7_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4772 | `{` |
|      - | 4773 | `	vfs_uname_fields sF;` |
|      - | 4774 | `	const char *zMode;` |
|     43 | 4775 | `	int nMode = 1,c;` |
|      - | 4776 | `#if defined(__WINNT__)` |
|      - | 4777 | `	char zHost[256],zRel[32],zVer[128],zMach[32];` |
|      - | 4778 | `	unsigned long nMajor,nMinor,nBuild;` |
|      - | 4779 | `	int bWorkstation;` |
|      - | 4780 | `#elif defined(__UNIXES__)` |
|      - | 4781 | `	struct utsname sName;` |
|      - | 4782 | `#endif` |
|     43 | 4783 | `	zMode = "a";` |
|     43 | 4784 | `	if( nArg > 0 ){` |
|     37 | 4785 | `		zMode = ph7_value_to_string(apArg[0],&nMode);` |
|     37 | 4786 | `		if( nMode != 1 ){` |
|      9 | 4787 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4788 | `				"php_uname(): Argument #1 ($mode) must be a single character");` |
|      - | 4789 | `		}` |
|     14 | 4790 | `	}` |
|     35 | 4791 | `	c = zMode[0];` |
|     35 | 4792 | `	if( c != 'a' && c != 'm' && c != 'n' && c != 'r' && c != 's' && c != 'v' ){` |
|      7 | 4793 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4794 | `			"php_uname(): Argument #1 ($mode) must be one of \"a\", \"m\", \"n\", \"r\", \"s\", or \"v\"");` |
|      - | 4795 | `	}` |
|      - | 4796 | `#if defined(__WINNT__)` |
|      1 | 4797 | `	VfsWinVersion(&nMajor,&nMinor,&nBuild,&bWorkstation);` |
|      1 | 4798 | `	zHost[0] = 0;` |
|      - | 4799 | `	{` |
|      1 | 4800 | `		DWORD nName = (DWORD)sizeof(zHost);` |
|      1 | 4801 | `		if( !GetComputerNameA(zHost,&nName) ){` |
|    ! 0 | 4802 | `			zHost[0] = 0;` |
|      - | 4803 | `		}` |
|      - | 4804 | `	}` |
|      1 | 4805 | `	SyBufferFormat(zRel,(sxu32)sizeof(zRel),"%u.%u",` |
|      - | 4806 | `		(unsigned int)nMajor,(unsigned int)nMinor);` |
|      1 | 4807 | `	SyBufferFormat(zVer,(sxu32)sizeof(zVer),"build %u (%s)",` |
|      - | 4808 | `		(unsigned int)nBuild,VfsWinProductName(nMajor,nMinor,nBuild,bWorkstation));` |
|      - | 4809 | `	/* php's own answer on Windows is the KERNEL's name and never the product's,` |
|      - | 4810 | ``	 * which is what makes `php_uname('s')` the same three words on every`` |
|      - | 4811 | `	 * Windows there is. */` |
|      1 | 4812 | `	sF.zSys = "Windows NT";` |
|      1 | 4813 | `	sF.zNode = zHost;` |
|      1 | 4814 | `	sF.zRel = zRel;` |
|      1 | 4815 | `	sF.zVer = zVer;` |
|      1 | 4816 | `	sF.zMachine = VfsWinMachine(zMach,(int)sizeof(zMach));` |
|      - | 4817 | `#elif defined(__UNIXES__)` |
|     28 | 4818 | `	if( uname(&sName) != 0 ){` |
|    ! 0 | 4819 | `		ph7_result_string(pCtx,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 | 4820 | `		return PH7_OK;` |
|      - | 4821 | `	}` |
|     28 | 4822 | `	sF.zSys = sName.sysname;` |
|     28 | 4823 | `	sF.zNode = sName.nodename;` |
|     28 | 4824 | `	sF.zRel = sName.release;` |
|     28 | 4825 | `	sF.zVer = sName.version;` |
|     28 | 4826 | `	sF.zMachine = sName.machine;` |
|      - | 4827 | `#else` |
|      - | 4828 | `	sF.zSys = "Unknown";` |
|      - | 4829 | `	sF.zNode = "";` |
|      - | 4830 | `	sF.zRel = "";` |
|      - | 4831 | `	sF.zVer = "";` |
|      - | 4832 | `	sF.zMachine = "";` |
|      - | 4833 | `#endif` |
|     29 | 4834 | `	switch( c ){` |
|      3 | 4835 | `		case 's': ph7_result_string(pCtx,sF.zSys,-1); break;` |
|      9 | 4836 | `		case 'n': ph7_result_string(pCtx,sF.zNode,-1); break;` |
|      3 | 4837 | `		case 'r': ph7_result_string(pCtx,sF.zRel,-1); break;` |
|      3 | 4838 | `		case 'v': ph7_result_string(pCtx,sF.zVer,-1); break;` |
|      3 | 4839 | `		case 'm': ph7_result_string(pCtx,sF.zMachine,-1); break;` |
|      6 | 4840 | `		default:` |
|     19 | 4841 | `			ph7_result_string_format(pCtx,"%s %s %s %s %s",` |
|      6 | 4842 | `				sF.zSys,sF.zNode,sF.zRel,sF.zVer,sF.zMachine);` |
|     12 | 4843 | `			break;` |
|      - | 4844 | `	}` |
|     29 | 4845 | `	return PH7_OK;` |
|     22 | 4846 | `}` |
|      - | 4847 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 4848 | `/* NULL VFS [i.e: a no-op VFS]*/` |
|      - | 4849 | `#if defined(_MSC_VER)` |
|      - | 4850 | `static const ph7_vfs null_vfs = {` |
|      - | 4851 | `#else` |
|      - | 4852 | `static const ph7_vfs null_vfs __attribute__((unused)) = {` |
|      - | 4853 | `#endif` |
|      - | 4854 | `	"null_vfs",` |
|      - | 4855 | `	PH7_VFS_VERSION,` |
|      - | 4856 | `	0, /* int (*xChdir)(const char *) */` |
|      - | 4857 | `	0, /* int (*xChroot)(const char *); */` |
|      - | 4858 | `	0, /* int (*xGetcwd)(ph7_context *) */` |
|      - | 4859 | `	0, /* int (*xMkdir)(const char *,int,int) */` |
|      - | 4860 | `	0, /* int (*xRmdir)(const char *) */` |
|      - | 4861 | `	0, /* int (*xIsdir)(const char *) */` |
|      - | 4862 | `	0, /* int (*xRename)(const char *,const char *) */` |
|      - | 4863 | `	0, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|      - | 4864 | `	0, /* int (*xSleep)(unsigned int) */` |
|      - | 4865 | `	0, /* int (*xUnlink)(const char *) */` |
|      - | 4866 | `	0, /* int (*xFileExists)(const char *) */` |
|      - | 4867 | `	0, /*int (*xChmod)(const char *,int)*/` |
|      - | 4868 | `	0, /*int (*xChown)(const char *,const char *)*/` |
|      - | 4869 | `	0, /*int (*xChgrp)(const char *,const char *)*/` |
|      - | 4870 | `	0, /* ph7_int64 (*xFreeSpace)(const char *) */` |
|      - | 4871 | `	0, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|      - | 4872 | `	0, /* ph7_int64 (*xFileSize)(const char *) */` |
|      - | 4873 | `	0, /* ph7_int64 (*xFileAtime)(const char *) */` |
|      - | 4874 | `	0, /* ph7_int64 (*xFileMtime)(const char *) */` |
|      - | 4875 | `	0, /* ph7_int64 (*xFileCtime)(const char *) */` |
|      - | 4876 | `	0, /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 4877 | `	0, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|      - | 4878 | `	0, /* int (*xIsfile)(const char *) */` |
|      - | 4879 | `	0, /* int (*xIslink)(const char *) */` |
|      - | 4880 | `	0, /* int (*xReadable)(const char *) */` |
|      - | 4881 | `	0, /* int (*xWritable)(const char *) */` |
|      - | 4882 | `	0, /* int (*xExecutable)(const char *) */` |
|      - | 4883 | `	0, /* int (*xFiletype)(const char *,ph7_context *) */` |
|      - | 4884 | `	0, /* int (*xGetenv)(const char *,ph7_context *) */` |
|      - | 4885 | `	0, /* int (*xSetenv)(const char *,const char *) */` |
|      - | 4886 | `	0, /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|      - | 4887 | `	0, /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|      - | 4888 | `	0, /* void (*xUnmap)(void *,ph7_int64);  */` |
|      - | 4889 | `	0, /* int (*xLink)(const char *,const char *,int) */` |
|      - | 4890 | `	0, /* int (*xUmask)(int) */` |
|      - | 4891 | `	0, /* void (*xTempDir)(ph7_context *) */` |
|      - | 4892 | `	0, /* unsigned int (*xProcessId)(void) */` |
|      - | 4893 | `	0, /* int (*xUid)(void) */` |
|      - | 4894 | `	0, /* int (*xGid)(void) */` |
|      - | 4895 | `	0, /* void (*xUsername)(ph7_context *) */` |
|      - | 4896 | `	0, /* int (*xExec)(const char *,ph7_context *) */` |
|      - | 4897 | `	0, /* int (*xReadlink)(const char *,ph7_context *) */` |
|      - | 4898 | `	0  /* int (*xEnviron)(ph7_context *) */` |
|      - | 4899 | `};` |
|      - | 4900 | `/* Windows VFS implementation moved to vfs_win.c */` |
|      - | 4901 | `/* Unix VFS implementation moved to vfs_unix.c */` |
|      - | 4902 | `/*` |
|      - | 4903 | ` * Export the builtin vfs.` |
|      - | 4904 | ` * Return a pointer to the builtin vfs if available.` |
|      - | 4905 | ` * Otherwise return the null_vfs [i.e: a no-op vfs] instead.` |
|      - | 4906 | ` * Note:` |
|      - | 4907 | ` *  The built-in vfs is always available for Windows/UNIX systems.` |
|      - | 4908 | ` * Note:` |
|      - | 4909 | ` *  If the engine is compiled with the PH7_DISABLE_DISK_IO/PH7_DISABLE_BUILTIN_FUNC` |
|      - | 4910 | ` *  directives defined then this function return the null_vfs instead.` |
|      - | 4911 | ` */` |
|   6731 | 4912 | `PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void)` |
|      5 | 4913 | `{` |
|      - | 4914 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|      - | 4915 | `#ifdef PH7_DISABLE_DISK_IO` |
|      - | 4916 | `	return &null_vfs;` |
|      - | 4917 | `#else` |
|      - | 4918 | `#ifdef __WINNT__` |
|      5 | 4919 | `	return &sWinVfs;` |
|      - | 4920 | `#elif defined(__UNIXES__)` |
|   6731 | 4921 | `	return &sUnixVfs;` |
|      - | 4922 | `#else` |
|      - | 4923 | `	return &null_vfs;` |
|      - | 4924 | `#endif /* __WINNT__/__UNIXES__ */` |
|      - | 4925 | `#endif /*PH7_DISABLE_DISK_IO*/` |
|      - | 4926 | `#else` |
|      - | 4927 | `	return &null_vfs;` |
|      - | 4928 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      5 | 4929 | `}` |
|      - | 4930 | `/*` |
|      - | 4931 | ` * Export the IO routines defined above and the built-in IO streams` |
|      - | 4932 | ` * [i.e: file://,php://].` |
|      - | 4933 | ` * Note:` |
|      - | 4934 | ` *  If the engine is compiled with the PH7_DISABLE_BUILTIN_FUNC directive` |
|      - | 4935 | ` *  defined then this function is a no-op.` |
|      - | 4936 | ` */` |
|   5619 | 4937 | `PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm)` |
|      5 | 4938 | `{` |
|      - | 4939 | `	/*` |
|      - | 4940 | `	 * Disk I/O routines are independent of PH7_DISABLE_BUILTIN_FUNC.` |
|      - | 4941 | `	 * Register them unless PH7_DISABLE_DISK_IO is explicitly defined.` |
|      - | 4942 | `	 */` |
|      - | 4943 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 4944 | `	/* VFS: disk I/O related functions */` |
|      - | 4945 | `	static const ph7_builtin_func aVfsDiskFunc[] = {` |
|      - | 4946 | `		{"chdir",   PH7_vfs_chdir   },` |
|      - | 4947 | `#ifndef __WINNT__` |
|      - | 4948 | `		/* php declares chroot() on POSIX only — there is no such call on Windows,` |
|      - | 4949 | ``		 * so `function_exists('chroot')` is FALSE there and the name is free for a`` |
|      - | 4950 | `		 * script to define. PHL used to declare it on both and answer a` |
|      - | 4951 | `		 * "not implemented in the underlying VFS" warning + false on Windows,` |
|      - | 4952 | `		 * which is a different thing from php's undefined function. (chown/chgrp/` |
|      - | 4953 | `		 * link/symlink/readlink stay: php declares all five on Windows.) */` |
|      - | 4954 | `		{"chroot",  PH7_vfs_chroot  },` |
|      - | 4955 | `#endif` |
|      - | 4956 | `		{"getcwd",  PH7_vfs_getcwd  },` |
|      - | 4957 | `		{"rmdir",   PH7_vfs_rmdir   },` |
|      - | 4958 | `		{"is_dir",  PH7_vfs_is_dir  },` |
|      - | 4959 | `		{"mkdir",   PH7_vfs_mkdir   },` |
|      - | 4960 | `		{"rename",  PH7_vfs_rename  },` |
|      - | 4961 | `		{"realpath",PH7_vfs_realpath},` |
|      - | 4962 | `		/* php's own resolver, and the question include/require answer silently:` |
|      - | 4963 | `		 * it walks the same include_path in the same order, so it belongs beside` |
|      - | 4964 | `		 * realpath() rather than with the stream builtins. */` |
|      - | 4965 | `		{"stream_resolve_include_path",PH7_vfs_stream_resolve_include_path},` |
|      - | 4966 | `		{"sleep",   PH7_vfs_sleep   },` |
|      - | 4967 | `		{"usleep",  PH7_vfs_usleep  },` |
|      - | 4968 | `		{"unlink",  PH7_vfs_unlink  },` |
|      - | 4969 | `		{"delete",  PH7_vfs_unlink  },` |
|      - | 4970 | `		{"chmod",   PH7_vfs_chmod   },` |
|      - | 4971 | `		{"chown",   PH7_vfs_chown   },` |
|      - | 4972 | `		{"chgrp",   PH7_vfs_chgrp   },` |
|      - | 4973 | `		{"disk_free_space",PH7_vfs_disk_free_space  },` |
|      - | 4974 | `		{"diskfreespace",  PH7_vfs_disk_free_space  },` |
|      - | 4975 | `		{"disk_total_space",PH7_vfs_disk_total_space},` |
|      - | 4976 | `		{"file_exists", PH7_vfs_file_exists },` |
|      - | 4977 | `		{"filesize",    PH7_vfs_file_size   },` |
|      - | 4978 | `		{"fileatime",   PH7_vfs_file_atime  },` |
|      - | 4979 | `		{"filemtime",   PH7_vfs_file_mtime  },` |
|      - | 4980 | `		{"filectime",   PH7_vfs_file_ctime  },` |
|      - | 4981 | `		{"is_file",     PH7_vfs_is_file  },` |
|      - | 4982 | `		{"is_link",     PH7_vfs_is_link  },` |
|      - | 4983 | `		{"is_readable", PH7_vfs_is_readable   },` |
|      - | 4984 | `		{"is_writable", PH7_vfs_is_writable   },` |
|      - | 4985 | `		/* php's own alias, spelled the other way; the diagnostics every` |
|      - | 4986 | `		 * builtin raises name the INVOKED name, so the one routine serves` |
|      - | 4987 | `		 * both. */` |
|      - | 4988 | `		{"is_writeable",PH7_vfs_is_writable   },` |
|      - | 4989 | `		{"is_executable",PH7_vfs_is_executable},` |
|      - | 4990 | `		{"filetype",    PH7_vfs_filetype },` |
|      - | 4991 | `		{"stat",        PH7_vfs_stat     },` |
|      - | 4992 | `		{"lstat",       PH7_vfs_lstat    },` |
|      - | 4993 | `		{"fileowner",   PH7_vfs_file_owner},` |
|      - | 4994 | `		{"filegroup",   PH7_vfs_file_group},` |
|      - | 4995 | `		{"fileinode",   PH7_vfs_file_inode},` |
|      - | 4996 | `		{"fileperms",   PH7_vfs_file_perms},` |
|      - | 4997 | `		{"getenv",      PH7_vfs_getenv   },` |
|      - | 4998 | `		{"setenv",      PH7_vfs_putenv   },` |
|      - | 4999 | `		{"putenv",      PH7_vfs_putenv   },` |
|      - | 5000 | `		{"touch",       PH7_vfs_touch    },` |
|      - | 5001 | `		{"link",        PH7_vfs_link     },` |
|      - | 5002 | `		{"symlink",     PH7_vfs_symlink  },` |
|      - | 5003 | `		{"readlink",    PH7_vfs_readlink },` |
|      - | 5004 | `		{"umask",       PH7_vfs_umask    },` |
|      - | 5005 | `		{"sys_get_temp_dir", PH7_vfs_sys_get_temp_dir },` |
|      - | 5006 | `		{"get_current_user", PH7_vfs_get_current_user },` |
|      - | 5007 | `		{"getmypid",    PH7_vfs_getmypid },` |
|      - | 5008 | `		{"getpid",      PH7_vfs_getmypid },` |
|      - | 5009 | `		{"getmyuid",    PH7_vfs_getmyuid },` |
|      - | 5010 | `		{"getuid",      PH7_vfs_getmyuid },` |
|      - | 5011 | `		{"getmygid",    PH7_vfs_getmygid },` |
|      - | 5012 | `		{"getgid",      PH7_vfs_getmygid },` |
|      - | 5013 | `		{"ph7_uname",   PH7_vfs_ph7_uname},` |
|      - | 5014 | `		{"php_uname",   PH7_vfs_ph7_uname}` |
|      - | 5015 | `	};` |
|      - | 5016 | `	/* IO stream / file operation functions (disk-related)` |
|      - | 5017 | `	 * md5_file/sha1_file are controlled only by PH7_DISABLE_HASH_FUNC.` |
|      - | 5018 | `	 */` |
|      - | 5019 | `	static const ph7_builtin_func aIOFunc[] = {` |
|      - | 5020 | `		{"ftruncate", PH7_builtin_ftruncate },` |
|      - | 5021 | `		{"fseek",     PH7_builtin_fseek  },` |
|      - | 5022 | `		{"ftell",     PH7_builtin_ftell  },` |
|      - | 5023 | `		{"rewind",    PH7_builtin_rewind },` |
|      - | 5024 | `		{"fflush",    PH7_builtin_fflush },` |
|      - | 5025 | `		{"feof",      PH7_builtin_feof   },` |
|      - | 5026 | `		{"fgetc",     PH7_builtin_fgetc  },` |
|      - | 5027 | `		{"fgets",     PH7_builtin_fgets  },` |
|      - | 5028 | `		{"fscanf",    PH7_builtin_fscanf },` |
|      - | 5029 | `		{"stream_get_line", PH7_builtin_stream_get_line },` |
|      - | 5030 | `		{"fread",     PH7_builtin_fread  },` |
|      - | 5031 | `		{"fgetcsv",   PH7_builtin_fgetcsv},` |
|      - | 5032 | `		{"fgetss",    PH7_builtin_fgetss },` |
|      - | 5033 | `		{"readdir",   PH7_builtin_readdir},` |
|      - | 5034 | `		{"rewinddir", PH7_builtin_rewinddir },` |
|      - | 5035 | `		{"closedir",  PH7_builtin_closedir},` |
|      - | 5036 | `		{"opendir",   PH7_builtin_opendir },` |
|      - | 5037 | `		/* php's dir() lives with opendir(), which is what it calls and what its` |
|      - | 5038 | `		 * failure warning is worded by. Registering it here also means the TINY` |
|      - | 5039 | `		 * build drops BOTH: the prelude copy was defined there and fataled on` |
|      - | 5040 | `		 * "Call to undefined function opendir()" the moment it was called. */` |
|      - | 5041 | `		{"dir",       PH7_builtin_dir },` |
|      - | 5042 | `		{"readfile",  PH7_builtin_readfile},` |
|      - | 5043 | `		{"file_get_contents", PH7_builtin_file_get_contents},` |
|      - | 5044 | `		{"file_put_contents", PH7_builtin_file_put_contents},` |
|      - | 5045 | `		{"file",      PH7_builtin_file   },` |
|      - | 5046 | `		{"copy",      PH7_builtin_copy   },` |
|      - | 5047 | `		{"fstat",     PH7_builtin_fstat  },` |
|      - | 5048 | `		{"stream_isatty", PH7_builtin_stream_isatty },` |
|      - | 5049 | `		{"fwrite",    PH7_builtin_fwrite },` |
|      - | 5050 | `		{"fputs",     PH7_builtin_fwrite },` |
|      - | 5051 | `		{"flock",     PH7_builtin_flock  },` |
|      - | 5052 | `		{"fclose",    PH7_builtin_fclose },` |
|      - | 5053 | `		{"fopen",     PH7_builtin_fopen  },` |
|      - | 5054 | `		{"stream_get_contents",  PH7_builtin_stream_get_contents },` |
|      - | 5055 | `		{"stream_get_wrappers",  PH7_builtin_stream_get_wrappers },` |
|      - | 5056 | `		{"stream_get_meta_data", PH7_builtin_stream_get_meta_data },` |
|      - | 5057 | `		/* php's own alias, kept from the days sockets had a separate API. */` |
|      - | 5058 | `		{"socket_get_status",    PH7_builtin_stream_get_meta_data },` |
|      - | 5059 | `		{"stream_set_blocking",  PH7_builtin_stream_set_blocking },` |
|      - | 5060 | `		{"socket_set_blocking",  PH7_builtin_stream_set_blocking },` |
|      - | 5061 | `		{"stream_set_timeout",   PH7_builtin_stream_set_timeout },` |
|      - | 5062 | `		{"stream_set_chunk_size",PH7_builtin_stream_set_chunk_size },` |
|      - | 5063 | `		{"stream_set_read_buffer",  PH7_builtin_stream_set_read_buffer },` |
|      - | 5064 | `		{"stream_set_write_buffer", PH7_builtin_stream_set_write_buffer },` |
|      - | 5065 | `		{"set_file_buffer",         PH7_builtin_stream_set_write_buffer },` |
|      - | 5066 | `		{"stream_supports_lock", PH7_builtin_stream_supports_lock },` |
|      - | 5067 | `		{"stream_is_local",      PH7_builtin_stream_is_local },` |
|      - | 5068 | `		{"stream_copy_to_stream",PH7_builtin_stream_copy_to_stream },` |
|      - | 5069 | `		{"stream_get_transports",PH7_builtin_stream_get_transports },` |
|      - | 5070 | `		/* Not under PH7_ENABLE_NET: a script selects over FILES and pipes in a` |
|      - | 5071 | `		 * build with no networking at all. */` |
|      - | 5072 | `		{"stream_select",        PH7_builtin_stream_select },` |
|      - | 5073 | `		{"stream_context_create",PH7_builtin_stream_context_create },` |
|      - | 5074 | `		{"stream_context_get_options",PH7_builtin_stream_context_get_options },` |
|      - | 5075 | `		{"stream_context_set_option", PH7_builtin_stream_context_set_option },` |
|      - | 5076 | `		{"stream_context_set_options",PH7_builtin_stream_context_set_options },` |
|      - | 5077 | `		{"stream_context_get_params", PH7_builtin_stream_context_get_params },` |
|      - | 5078 | `		{"stream_context_set_params", PH7_builtin_stream_context_set_params },` |
|      - | 5079 | `		{"stream_context_get_default",PH7_builtin_stream_context_get_default },` |
|      - | 5080 | `		{"stream_context_set_default",PH7_builtin_stream_context_set_default },` |
|      - | 5081 | `		{"stream_wrapper_register",   PH7_builtin_stream_wrapper_register },` |
|      - | 5082 | `		{"stream_register_wrapper",   PH7_builtin_stream_wrapper_register },` |
|      - | 5083 | `		{"stream_wrapper_unregister", PH7_builtin_stream_wrapper_unregister },` |
|      - | 5084 | `		{"stream_wrapper_restore",    PH7_builtin_stream_wrapper_restore },` |
|      - | 5085 | `		{"stream_filter_append",  PH7_builtin_stream_filter_append },` |
|      - | 5086 | `		{"stream_filter_prepend", PH7_builtin_stream_filter_prepend },` |
|      - | 5087 | `		{"stream_filter_remove",  PH7_builtin_stream_filter_remove },` |
|      - | 5088 | `		{"stream_get_filters",    PH7_builtin_stream_get_filters },` |
|      - | 5089 | `		{"stream_filter_register",PH7_builtin_stream_filter_register },` |
|      - | 5090 | `		{"stream_bucket_make_writeable", PH7_builtin_stream_bucket_make_writeable },` |
|      - | 5091 | `		{"stream_bucket_append",  PH7_builtin_stream_bucket_append },` |
|      - | 5092 | `		{"stream_bucket_prepend", PH7_builtin_stream_bucket_prepend },` |
|      - | 5093 | `		{"stream_bucket_new",     PH7_builtin_stream_bucket_new },` |
|      - | 5094 | `#ifdef PH7_ENABLE_NET` |
|      - | 5095 | `		{"fsockopen",  PH7_builtin_fsockopen },` |
|      - | 5096 | `		{"pfsockopen", PH7_builtin_fsockopen },` |
|      - | 5097 | `		{"stream_socket_client", PH7_builtin_fsockopen },` |
|      - | 5098 | `		{"stream_socket_server", PH7_builtin_stream_socket_server },` |
|      - | 5099 | `		{"stream_socket_accept", PH7_builtin_stream_socket_accept },` |
|      - | 5100 | `		{"stream_socket_get_name", PH7_builtin_stream_socket_get_name },` |
|      - | 5101 | `		{"stream_socket_pair",   PH7_builtin_stream_socket_pair },` |
|      - | 5102 | `		{"stream_socket_shutdown", PH7_builtin_stream_socket_shutdown },` |
|      - | 5103 | `		{"stream_socket_recvfrom", PH7_builtin_stream_socket_recvfrom },` |
|      - | 5104 | `		{"stream_socket_sendto",   PH7_builtin_stream_socket_sendto },` |
|      - | 5105 | `		/* The address converters and the host name: php's ext/standard` |
|      - | 5106 | `		 * network trio, which sits with the socket family here because the` |
|      - | 5107 | `		 * last of the three is an OS call the others share a build flag with. */` |
|      - | 5108 | `		{"inet_pton",  PH7_builtin_inet_pton },` |
|      - | 5109 | `		{"inet_ntop",  PH7_builtin_inet_ntop },` |
|      - | 5110 | `		{"gethostname",PH7_builtin_gethostname },` |
|      - | 5111 | `#endif` |
|      - | 5112 | `		{"popen",     PH7_builtin_popen  },` |
|      - | 5113 | `		{"proc_open",      PH7_builtin_proc_open      },` |
|      - | 5114 | `		{"proc_close",     PH7_builtin_proc_close     },` |
|      - | 5115 | `		{"proc_terminate", PH7_builtin_proc_terminate },` |
|      - | 5116 | `		{"proc_get_status",PH7_builtin_proc_get_status},` |
|      - | 5117 | `		{"proc_nice",      PH7_builtin_proc_nice      },` |
|      - | 5118 | `		{"shell_exec", PH7_builtin_shell_exec },` |
|      - | 5119 | `		{"exec",       PH7_builtin_exec     },` |
|      - | 5120 | `		{"system",     PH7_builtin_system   },` |
|      - | 5121 | `		{"passthru",   PH7_builtin_passthru },` |
|      - | 5122 | `		/* The shell-escaping pair lives with the command runners it exists to` |
|      - | 5123 | `		 * feed: a build without process execution has nothing to escape for. */` |
|      - | 5124 | `		{"escapeshellarg", PH7_builtin_escapeshellarg },` |
|      - | 5125 | `		{"escapeshellcmd", PH7_builtin_escapeshellcmd },` |
|      - | 5126 | `		{"pclose",    PH7_builtin_pclose },` |
|      - | 5127 | `		{"fpassthru", PH7_builtin_fpassthru },` |
|      - | 5128 | `		{"fputcsv",   PH7_builtin_fputcsv },` |
|      - | 5129 | `		{"fprintf",   PH7_builtin_fprintf },` |
|      - | 5130 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5131 | `		{"md5_file",  PH7_builtin_md5_file},` |
|      - | 5132 | `		{"sha1_file", PH7_builtin_sha1_file},` |
|      - | 5133 | `		/* The hash extension's file readers live with the disk table for the` |
|      - | 5134 | `		 * same reason md5_file does: without disk IO there is nothing to read. */` |
|      - | 5135 | `		{"hash_file",          PH7_builtin_hash_file },` |
|      - | 5136 | `		{"hash_hmac_file",     PH7_builtin_hash_hmac_file },` |
|      - | 5137 | `		{"hash_update_file",   PH7_builtin_hash_update_file },` |
|      - | 5138 | `		{"hash_update_stream", PH7_builtin_hash_update_stream },` |
|      - | 5139 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5140 | `		{"parse_ini_file", PH7_builtin_parse_ini_file},` |
|      - | 5141 | `		{"vfprintf",  PH7_builtin_vfprintf},` |
|      - | 5142 | `#ifdef PH7_ENABLE_ZLIB` |
|      - | 5143 | `		/* ext/zlib's handle verbs, beside the stream functions they ARE: php` |
|      - | 5144 | `		 * registers each as an alias of the one above it, which is why` |
|      - | 5145 | `		 * gzread() works on a plain fopen() handle and fread() works on a` |
|      - | 5146 | `		 * gzopen() one. Their own signature rows word their diagnostics.` |
|      - | 5147 | `		 * The rest of the extension registers itself (PH7_ZlibFuncTable). */` |
|      - | 5148 | `		{"gzread",     PH7_builtin_fread  },` |
|      - | 5149 | `		{"gzwrite",    PH7_builtin_fwrite },` |
|      - | 5150 | `		{"gzputs",     PH7_builtin_fwrite },` |
|      - | 5151 | `		{"gzgets",     PH7_builtin_fgets  },` |
|      - | 5152 | `		{"gzgetc",     PH7_builtin_fgetc  },` |
|      - | 5153 | `		{"gzeof",      PH7_builtin_feof   },` |
|      - | 5154 | `		{"gzclose",    PH7_builtin_fclose },` |
|      - | 5155 | `		{"gzseek",     PH7_builtin_fseek  },` |
|      - | 5156 | `		{"gztell",     PH7_builtin_ftell  },` |
|      - | 5157 | `		{"gzrewind",   PH7_builtin_rewind },` |
|      - | 5158 | `		{"gzpassthru", PH7_builtin_fpassthru }` |
|      - | 5159 | `#endif /* PH7_ENABLE_ZLIB */` |
|      - | 5160 | `	};` |
|   5624 | 5161 | `	const ph7_io_stream *pFileStream = 0;` |
|   5624 | 5162 | `	sxu32 n = 0;` |
|      - | 5163 | `	/* Register disk-related functions */` |
| 314669 | 5164 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsDiskFunc) ; ++n ){` |
| 309050 | 5165 | `		ph7_create_function(&(*pVm),aVfsDiskFunc[n].zName,aVfsDiskFunc[n].xFunc,(void *)pVm->pEngine->pVfs);` |
| 154280 | 5166 | `	}` |
| 651809 | 5167 | `	for( n = 0 ; n < SX_ARRAYSIZE(aIOFunc) ; ++n ){` |
| 646190 | 5168 | `		ph7_create_function(&(*pVm),aIOFunc[n].zName,aIOFunc[n].xFunc,pVm);` |
| 322580 | 5169 | `	}` |
|      - | 5170 | `#else` |
|      - | 5171 | `	SXUNUSED(pVm);` |
|      - | 5172 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 5173 |  |
|      - | 5174 | `	/*` |
|      - | 5175 | `	 * Register non-disk helper builtins only when PH7_DISABLE_BUILTIN_FUNC` |
|      - | 5176 | `	 * is not set (preserve previous behavior for those helpers).` |
|      - | 5177 | `	 */` |
|      - | 5178 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 5179 | `	static const ph7_builtin_func aVfsHelperFunc[] = {` |
|      - | 5180 | `		/* Path processing */` |
|      - | 5181 | `		{"dirname",     PH7_builtin_dirname  },` |
|      - | 5182 | `		{"basename",    PH7_builtin_basename },` |
|      - | 5183 | `		{"pathinfo",    PH7_builtin_pathinfo },` |
|      - | 5184 | `		{"strglob",     PH7_builtin_strglob  },` |
|      - | 5185 | `		{"fnmatch",     PH7_builtin_fnmatch  }` |
|      - | 5186 | `	};` |
|  33719 | 5187 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVfsHelperFunc) ; ++n ){` |
|  28100 | 5188 | `		ph7_create_function(&(*pVm),aVfsHelperFunc[n].zName,aVfsHelperFunc[n].xFunc,pVm);` |
|  14030 | 5189 | `	}` |
|      - | 5190 | `	/* The three names a script asks an http:// exchange about. */` |
|   5624 | 5191 | `	PH7_HttpInstallFuncs(&(*pVm));` |
|      - | 5192 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 5193 |  |
|      - | 5194 | `	/* Install streams if disk I/O is enabled */` |
|      - | 5195 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 5196 | `#ifdef __WINNT__` |
|      5 | 5197 | `	pFileStream = &sWinFileStream;` |
|      - | 5198 | `#elif defined(__UNIXES__)` |
|   5619 | 5199 | `	pFileStream = &sUnixFileStream;` |
|      - | 5200 | `#endif` |
|      - | 5201 | `	/* Install the php:// stream */` |
|   5624 | 5202 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHP_Stream);` |
|      - | 5203 | `#ifdef PH7_ENABLE_ZLIB` |
|      - | 5204 | `	/* compress.zlib:// -- the same device gzopen() opens directly. */` |
|   5624 | 5205 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sZLIB_Stream);` |
|      - | 5206 | `#endif` |
|      - | 5207 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 5208 | `	/* phar:// -- what an archive's own entries are read through. */` |
|   5624 | 5209 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sPHAR_Stream);` |
|      - | 5210 | `#endif` |
|   5624 | 5211 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sDATA_Stream);` |
|      - | 5212 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 5213 | `	/* glob:// lives beside the pattern matcher it drives, so it is only in the` |
|      - | 5214 | `	 * build when that is. */` |
|   5624 | 5215 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sGLOB_Stream);` |
|      - | 5216 | `#endif` |
|      - | 5217 | `#ifdef PH7_ENABLE_NET` |
|   5624 | 5218 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sTCP_Stream);` |
|      - | 5219 | `	/* php's one built-in protocol wrapper. It speaks over the same sockets` |
|      - | 5220 | `	 * tcp:// hands out, so it is in the build exactly when they are. */` |
|   5624 | 5221 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sHTTP_Stream);` |
|      - | 5222 | `#endif` |
|   5624 | 5223 | `	if( pFileStream ){` |
|      - | 5224 | `		/* Install the file:// stream */` |
|   5624 | 5225 | `		ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,pFileStream);` |
|   2805 | 5226 | `	}` |
|      - | 5227 | `#if defined(PH7_ENABLE_ZLIB) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|      - | 5228 | `	/* zip:// -- an OPENER and nothing else, exactly as php's is: no url_stat,` |
|      - | 5229 | ``	 * so `file_exists('zip://…')` is false, and no directory door, so an`` |
|      - | 5230 | `	 * archive can only be listed through ZipArchive. It goes on LAST because` |
|      - | 5231 | ``	 * php registers it last and `stream_get_wrappers()` answers in that`` |
|      - | 5232 | `	 * order. */` |
|   5624 | 5233 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_IO_STREAM,&sZIP_Stream);` |
|      - | 5234 | `#endif` |
|      - | 5235 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 5236 |  |
|   5624 | 5237 | `	return SXRET_OK;` |
|      5 | 5238 | `}` |
|      - | 5239 |  |
