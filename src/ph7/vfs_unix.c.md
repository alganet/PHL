# src/ph7/vfs_unix.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 489/516 lines (94.77%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|     - |    7 | `#ifdef __UNIXES__` |
|     - |    8 | `/*` |
|     - |    9 | ` * UNIX VFS implementation for the PH7 engine.` |
|     - |   10 | ` * Status:` |
|     - |   11 | ` *    Stable.` |
|     - |   12 | ` */` |
|     - |   13 | `#include <sys/types.h>` |
|     - |   14 | `#include <string.h>` |
|     - |   15 | `#include <limits.h>` |
|     - |   16 | `#include <fcntl.h>` |
|     - |   17 | `#include <unistd.h>` |
|     - |   18 | `#include <sys/uio.h>` |
|     - |   19 | `#include <sys/stat.h>` |
|     - |   20 | `#include <sys/statvfs.h>` |
|     - |   21 | `#include <sys/mman.h>` |
|     - |   22 | `#include <sys/file.h>` |
|     - |   23 | `#include <sys/wait.h>` |
|     - |   24 | `#include <pwd.h>` |
|     - |   25 | `#include <grp.h>` |
|     - |   26 | `#include <dirent.h>` |
|     - |   27 | `#include <utime.h>` |
|     - |   28 | `#include <stdio.h>` |
|     - |   29 | `#include <stdlib.h>` |
|     - |   30 | `#include <errno.h>` |
|     - |   31 | `/*` |
|     - |   32 | ` * php's file:// wrapper is the default local-file scheme: a filesystem builtin` |
|     - |   33 | ` * given "file://[authority]/path" acts on the plain "/path". The authority is` |
|     - |   34 | ` * empty (file:///abs) or "localhost"; the scheme match is case-insensitive.` |
|     - |   35 | ` * The native syscalls below do not understand the scheme, so strip it here.` |
|     - |   36 | ` * Anything else (a non-local authority, or no path component) is left intact so` |
|     - |   37 | ` * the syscall fails exactly as php does. chdir()/chroot()/realpath() are NOT` |
|     - |   38 | ` * routed through here -- php rejects file:// for those.` |
|     - |   39 | ` */` |
| 88810 |   40 | `static const char * UnixVfsLocalPath(const char *zPath)` |
|     - |   41 | `{` |
|     - |   42 | `	/* One rule for the whole engine: PH7_VmFileUrlLocalPath() is the same strip` |
|     - |   43 | ``	 * the stream lookup applies, so a `file://` URL means the same file whether`` |
|     - |   44 | `	 * it is opened or stat'ed. This used to be a second, shorter copy. */` |
| 88810 |   45 | `	return PH7_VmFileUrlLocalPath(zPath);` |
|     - |   46 | `}` |
|     - |   47 | `/* int (*xchdir)(const char *) */` |
| 17590 |   48 | `static int UnixVfs_chdir(const char *zPath)` |
|     - |   49 | `{` |
|     - |   50 | `  int rc;` |
| 17590 |   51 | `  rc = chdir(zPath);` |
| 17590 |   52 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |   53 | `}` |
|     - |   54 | `/* int (*xGetcwd)(ph7_context *) */` |
|  1617 |   55 | `static int UnixVfs_getcwd(ph7_context *pCtx)` |
|     - |   56 | `{` |
|     - |   57 | `	char zBuf[4096];` |
|     - |   58 | `	char *zDir;` |
|     - |   59 | `	/* Get the current directory */` |
|  1617 |   60 | `	zDir = getcwd(zBuf,sizeof(zBuf));` |
|  1617 |   61 | `	if( zDir == 0 ){` |
|   ! 0 |   62 | `	  return -1;` |
|     - |   63 | `    }` |
|  1617 |   64 | `	ph7_result_string(pCtx,zDir,-1/*Compute length automatically*/);` |
|  1617 |   65 | `	return PH7_OK;` |
|   806 |   66 | `}` |
|     - |   67 | `/* int (*xMkdir)(const char *,int,int)` |
|     - |   68 | ` * ONE level. php builds a tree in the WRAPPER rather than in the syscall, and so` |
|     - |   69 | `` * does PHL now (VfsMkdirRecursive in vfs.c), so `recursive` never arrives set. */`` |
|   400 |   70 | `static int UnixVfs_mkdir(const char *zPath,int mode,int recursive)` |
|     - |   71 | `{` |
|   400 |   72 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |   73 | `	int rc;` |
|   400 |   74 | `        rc = mkdir(zPath,mode);` |
|   192 |   75 | `	SXUNUSED(recursive); /* cc warning */` |
|   400 |   76 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   77 | `}` |
|     - |   78 | `/* int (*xRmdir)(const char *) */` |
|   386 |   79 | `static int UnixVfs_rmdir(const char *zPath)` |
|     - |   80 | `{` |
|   386 |   81 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |   82 | `	int rc;` |
|   386 |   83 | `	rc = rmdir(zPath);` |
|   386 |   84 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   85 | `}` |
|     - |   86 | `/* int (*xIsdir)(const char *) */` |
| 15366 |   87 | `static int UnixVfs_isdir(const char *zPath)` |
|     - |   88 | `{` |
| 15366 |   89 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |   90 | `	struct stat st;` |
|     - |   91 | `	int rc;` |
| 15366 |   92 | `	rc = stat(zPath,&st);` |
| 15366 |   93 | `	if( rc != 0 ){` |
|   187 |   94 | `	 return -1;` |
|     - |   95 | `	}` |
| 15179 |   96 | `	rc = S_ISDIR(st.st_mode);` |
| 15179 |   97 | `	return rc ? PH7_OK : -1 ;` |
|  7680 |   98 | `}` |
|     - |   99 | `/* int (*xRename)(const char *,const char *) */` |
|     4 |  100 | `static int UnixVfs_Rename(const char *zOld,const char *zNew)` |
|     - |  101 | `{` |
|     4 |  102 | `	zOld = UnixVfsLocalPath(zOld);` |
|     4 |  103 | `	zNew = UnixVfsLocalPath(zNew);` |
|     - |  104 | `	int rc;` |
|     4 |  105 | `	rc = rename(zOld,zNew);` |
|     4 |  106 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  107 | `}` |
|     - |  108 | `/* int (*xRealpath)(const char *,ph7_context *) */` |
|   880 |  109 | `static int UnixVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|     - |  110 | `{` |
|     - |  111 | `#ifndef PH7_UNIX_OLD_LIBC` |
|     - |  112 | `	char *zReal;` |
|   880 |  113 | `	zReal = realpath(zPath,0);` |
|   880 |  114 | `	if( zReal == 0 ){` |
|    41 |  115 | `	  return -1;` |
|     - |  116 | `	}` |
|   839 |  117 | `	ph7_result_string(pCtx,zReal,-1/*Compute length automatically*/);` |
|     - |  118 | `        /* Release the allocated buffer */` |
|   839 |  119 | `	free(zReal);` |
|   839 |  120 | `	return PH7_OK;` |
|     - |  121 | `#else` |
|     - |  122 | `    zPath = 0; /* cc warning */` |
|     - |  123 | `    pCtx = 0;` |
|     - |  124 | `    return -1;` |
|     - |  125 | `#endif` |
|   437 |  126 | `}` |
|     - |  127 | `/* int (*xSleep)(unsigned int) */` |
|   199 |  128 | `static int UnixVfs_Sleep(unsigned int uSec)` |
|     - |  129 | `{` |
|   199 |  130 | `	usleep(uSec);` |
|   199 |  131 | `	return PH7_OK;` |
|     - |  132 | `}` |
|     - |  133 | `/* int (*xUnlink)(const char *) */` |
| 50885 |  134 | `static int UnixVfs_unlink(const char *zPath)` |
|     - |  135 | `{` |
| 50885 |  136 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  137 | `	int rc;` |
| 50885 |  138 | `	rc = unlink(zPath);` |
| 50885 |  139 | `	return rc == 0 ? PH7_OK : -1 ;` |
|     - |  140 | `}` |
|     - |  141 | `/* int (*xFileExists)(const char *) */` |
|  1717 |  142 | `static int UnixVfs_FileExists(const char *zPath)` |
|     - |  143 | `{` |
|  1717 |  144 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  145 | `	int rc;` |
|  1717 |  146 | `	rc = access(zPath,F_OK);` |
|  1717 |  147 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  148 | `}` |
|     - |  149 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|     - |  150 | `/* ph7_int64 (*xFreeSpace)(const char *) */` |
|    14 |  151 | `static ph7_int64 UnixVfs_FreeSpace(const char *zPath)` |
|     - |  152 | `{` |
|    14 |  153 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  154 | `	struct statvfs sInfo;` |
|    14 |  155 | `	if( statvfs(zPath,&sInfo) != 0 ){` |
|     4 |  156 | `		return -1;` |
|     - |  157 | `	}` |
|     - |  158 | `	/* php reports the space available to an UNPRIVILEGED user (f_bavail) */` |
|    10 |  159 | `	return (ph7_int64)sInfo.f_bavail * (ph7_int64)sInfo.f_frsize;` |
|     7 |  160 | `}` |
|     - |  161 | `/* ph7_int64 (*xTotalSpace)(const char *) */` |
|    10 |  162 | `static ph7_int64 UnixVfs_TotalSpace(const char *zPath)` |
|     - |  163 | `{` |
|    10 |  164 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  165 | `	struct statvfs sInfo;` |
|    10 |  166 | `	if( statvfs(zPath,&sInfo) != 0 ){` |
|     2 |  167 | `		return -1;` |
|     - |  168 | `	}` |
|     8 |  169 | `	return (ph7_int64)sInfo.f_blocks * (ph7_int64)sInfo.f_frsize;` |
|     5 |  170 | `}` |
|    54 |  171 | `static ph7_int64 UnixVfs_FileSize(const char *zPath)` |
|     - |  172 | `{` |
|    54 |  173 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  174 | `	struct stat st;` |
|     - |  175 | `	int rc;` |
|    54 |  176 | `	rc = stat(zPath,&st);` |
|    54 |  177 | `	if( rc != 0 ){` |
|     6 |  178 | `	 return -1;` |
|     - |  179 | `	}` |
|    48 |  180 | `	return (ph7_int64)st.st_size;` |
|    27 |  181 | `}` |
|     - |  182 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    50 |  183 | `static int UnixVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|     - |  184 | `{` |
|    50 |  185 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  186 | `	struct utimbuf ut;` |
|     - |  187 | `	int rc;` |
|     - |  188 | `	/* Both stamps are always real values here — the builtin resolves php's "now"` |
|     - |  189 | `	 * default, because a NEGATIVE timestamp is legal and so cannot be a sentinel. */` |
|    50 |  190 | `	ut.actime  = (time_t)access_time;` |
|    50 |  191 | `	ut.modtime = (time_t)touch_time;` |
|    50 |  192 | `	rc = utime(zPath,&ut);` |
|    50 |  193 | `	if( rc != 0 ){` |
|     - |  194 | `		/* php's touch() CREATES the file when it is missing — that is the idiom's` |
|     - |  195 | ``		 * main use, and utime() alone fails ENOENT there, so `touch($new)` answered`` |
|     - |  196 | `		 * false and created nothing. Try the create only after utime failed, like` |
|     - |  197 | `		 * php: no extra stat on the common path, and no access(2) real-uid check. */` |
|    32 |  198 | `		int fd = open(zPath,O_WRONLY\|O_CREAT,0666);` |
|    32 |  199 | `		if( fd < 0 ){` |
|     4 |  200 | `			return -1;` |
|     - |  201 | `		}` |
|    28 |  202 | `		close(fd);` |
|    28 |  203 | `		rc = utime(zPath,&ut);` |
|    28 |  204 | `		if( rc != 0 ){` |
|   ! 0 |  205 | `			return -1;` |
|     - |  206 | `		}` |
|    14 |  207 | `	}` |
|    46 |  208 | `	return PH7_OK;` |
|    25 |  209 | `}` |
|     - |  210 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|    10 |  211 | `static ph7_int64 UnixVfs_FileAtime(const char *zPath)` |
|     - |  212 | `{` |
|    10 |  213 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  214 | `	struct stat st;` |
|     - |  215 | `	int rc;` |
|    10 |  216 | `	rc = stat(zPath,&st);` |
|    10 |  217 | `	if( rc != 0 ){` |
|     4 |  218 | `	 return -1;` |
|     - |  219 | `	}` |
|     6 |  220 | `	return (ph7_int64)st.st_atime;` |
|     5 |  221 | `}` |
|     - |  222 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|    30 |  223 | `static ph7_int64 UnixVfs_FileMtime(const char *zPath)` |
|     - |  224 | `{` |
|    30 |  225 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  226 | `	struct stat st;` |
|     - |  227 | `	int rc;` |
|    30 |  228 | `	rc = stat(zPath,&st);` |
|    30 |  229 | `	if( rc != 0 ){` |
|     2 |  230 | `	 return -1;` |
|     - |  231 | `	}` |
|    28 |  232 | `	return (ph7_int64)st.st_mtime;` |
|    15 |  233 | `}` |
|     - |  234 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|     6 |  235 | `static ph7_int64 UnixVfs_FileCtime(const char *zPath)` |
|     - |  236 | `{` |
|     6 |  237 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  238 | `	struct stat st;` |
|     - |  239 | `	int rc;` |
|     6 |  240 | `	rc = stat(zPath,&st);` |
|     6 |  241 | `	if( rc != 0 ){` |
|     2 |  242 | `	 return -1;` |
|     - |  243 | `	}` |
|     4 |  244 | `	return (ph7_int64)st.st_ctime;` |
|     3 |  245 | `}` |
|     - |  246 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|   182 |  247 | `static int UnixVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  248 | `{` |
|   182 |  249 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  250 | `	struct stat st;` |
|     - |  251 | `	int rc;` |
|   182 |  252 | `	rc = stat(zPath,&st);` |
|   182 |  253 | `	if( rc != 0 ){` |
|    50 |  254 | `	 return -1;` |
|     - |  255 | `	}` |
|     - |  256 | `	/* dev */` |
|   132 |  257 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|   132 |  258 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  259 | `	/* ino */` |
|   132 |  260 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|   132 |  261 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  262 | `	/* mode */` |
|   132 |  263 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|   132 |  264 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  265 | `	/* nlink */` |
|   132 |  266 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|   132 |  267 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  268 | `	/* uid,gid,rdev */` |
|   132 |  269 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|   132 |  270 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|   132 |  271 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|   132 |  272 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|   132 |  273 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|   132 |  274 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  275 | `	/* size */` |
|   132 |  276 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|   132 |  277 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  278 | `	/* atime */` |
|   132 |  279 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|   132 |  280 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  281 | `	/* mtime */` |
|   132 |  282 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|   132 |  283 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  284 | `	/* ctime */` |
|   132 |  285 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|   132 |  286 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  287 | `	/* blksize,blocks */` |
|   132 |  288 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|   132 |  289 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|   132 |  290 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|   132 |  291 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|   132 |  292 | `	return PH7_OK;` |
|    91 |  293 | `}` |
|     - |  294 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|    10 |  295 | `static int UnixVfs_lStat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  296 | `{` |
|    10 |  297 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  298 | `	struct stat st;` |
|     - |  299 | `	int rc;` |
|    10 |  300 | `	rc = lstat(zPath,&st);` |
|    10 |  301 | `	if( rc != 0 ){` |
|     4 |  302 | `	 return -1;` |
|     - |  303 | `	}` |
|     - |  304 | `	/* dev */` |
|     6 |  305 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|     6 |  306 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  307 | `	/* ino */` |
|     6 |  308 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|     6 |  309 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  310 | `	/* mode */` |
|     6 |  311 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|     6 |  312 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  313 | `	/* nlink */` |
|     6 |  314 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|     6 |  315 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  316 | `	/* uid,gid,rdev */` |
|     6 |  317 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|     6 |  318 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|     6 |  319 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|     6 |  320 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|     6 |  321 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|     6 |  322 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  323 | `	/* size */` |
|     6 |  324 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|     6 |  325 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  326 | `	/* atime */` |
|     6 |  327 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|     6 |  328 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  329 | `	/* mtime */` |
|     6 |  330 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|     6 |  331 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  332 | `	/* ctime */` |
|     6 |  333 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|     6 |  334 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  335 | `	/* blksize,blocks */` |
|     6 |  336 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|     6 |  337 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|     6 |  338 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|     6 |  339 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|     6 |  340 | `	return PH7_OK;` |
|     5 |  341 | `}` |
|     - |  342 | `/* int (*xChmod)(const char *,int) */` |
|   665 |  343 | `static int UnixVfs_Chmod(const char *zPath,int mode)` |
|     - |  344 | `{` |
|   665 |  345 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  346 | `    int rc;` |
|   665 |  347 | `    rc = chmod(zPath,(mode_t)mode);` |
|   665 |  348 | `    return rc == 0 ? PH7_OK : - 1;` |
|     - |  349 | `}` |
|     - |  350 | `/* int (*xChown)(const char *,const char *) */` |
|     6 |  351 | `static int UnixVfs_Chown(const char *zPath,const char *zUser)` |
|     - |  352 | `{` |
|     6 |  353 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  354 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  355 | `  struct passwd *pwd;` |
|     - |  356 | `  uid_t uid;` |
|     - |  357 | `  int rc;` |
|     - |  358 | `  /* php accepts a numeric uid as well as a name; PH7 only ever did getpwnam(), so` |
|     - |  359 | `   * chown($f, 0) failed on the NAME lookup and never reached the syscall (leaving errno` |
|     - |  360 | `   * unset, hence a bogus "Undefined error: 0" in the warning). */` |
|     6 |  361 | `  if( zUser[0] >= '0' && zUser[0] <= '9' ){` |
|     4 |  362 | `    uid = (uid_t)atoi(zUser);` |
|     2 |  363 | `  }else{` |
|     2 |  364 | `    pwd = getpwnam(zUser);   /* Try getting UID for username */` |
|     2 |  365 | `    if (pwd == 0) {` |
|     - |  366 | `      /* -2 = the NAME could not be resolved (no syscall ran, so errno means nothing).` |
|     - |  367 | `       * php words that case differently: "chown(): Unable to find uid for bogus". */` |
|     2 |  368 | `      return -2;` |
|     - |  369 | `    }` |
|   ! 0 |  370 | `    uid = pwd->pw_uid;` |
|     - |  371 | `  }` |
|     4 |  372 | `  rc = chown(zPath,uid,-1);` |
|     4 |  373 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |  374 | `#else` |
|     - |  375 | `	SXUNUSED(zPath);` |
|     - |  376 | `	SXUNUSED(zUser);` |
|     - |  377 | `	return -1;` |
|     - |  378 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     3 |  379 | `}` |
|     - |  380 | `/* int (*xChgrp)(const char *,const char *) */` |
|     6 |  381 | `static int UnixVfs_Chgrp(const char *zPath,const char *zGroup)` |
|     - |  382 | `{` |
|     6 |  383 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  384 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  385 | `  struct group *group;` |
|     - |  386 | `  gid_t gid;` |
|     - |  387 | `  int rc;` |
|     - |  388 | `  /* Numeric gid accepted too (see UnixVfs_Chown) */` |
|     6 |  389 | `  if( zGroup[0] >= '0' && zGroup[0] <= '9' ){` |
|     4 |  390 | `    gid = (gid_t)atoi(zGroup);` |
|     2 |  391 | `  }else{` |
|     2 |  392 | `    group = getgrnam(zGroup);` |
|     2 |  393 | `    if (group == 0) {` |
|     2 |  394 | `      return -2;   /* name lookup failed -- see UnixVfs_Chown */` |
|     - |  395 | `    }` |
|   ! 0 |  396 | `    gid = group->gr_gid;` |
|     - |  397 | `  }` |
|     4 |  398 | `  rc = chown(zPath,-1,gid);` |
|     4 |  399 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |  400 | `#else` |
|     - |  401 | `	SXUNUSED(zPath);` |
|     - |  402 | `	SXUNUSED(zGroup);` |
|     - |  403 | `	return -1;` |
|     - |  404 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     3 |  405 | `}` |
|     - |  406 | `/* int (*xIsfile)(const char *) */` |
| 10903 |  407 | `static int UnixVfs_isfile(const char *zPath)` |
|     - |  408 | `{` |
| 10903 |  409 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  410 | `	struct stat st;` |
|     - |  411 | `	int rc;` |
| 10903 |  412 | `	rc = stat(zPath,&st);` |
| 10903 |  413 | `	if( rc != 0 ){` |
|   202 |  414 | `	 return -1;` |
|     - |  415 | `	}` |
| 10701 |  416 | `	rc = S_ISREG(st.st_mode);` |
| 10701 |  417 | `	return rc ? PH7_OK : -1 ;` |
|  5600 |  418 | `}` |
|     - |  419 | `/* int (*xIslink)(const char *) */` |
|    32 |  420 | `static int UnixVfs_islink(const char *zPath)` |
|     - |  421 | `{` |
|    32 |  422 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  423 | `	struct stat st;` |
|     - |  424 | `	int rc;` |
|     - |  425 | `	/* LSTAT, not stat: stat() FOLLOWS the link and answers about its target, so` |
|     - |  426 | `	 * S_ISLNK could never be true here and is_link() was false for every symlink` |
|     - |  427 | `	 * in existence. php's is_link() is php_stat(FS_IS_LINK), which lstats. */` |
|    32 |  428 | `	rc = lstat(zPath,&st);` |
|    32 |  429 | `	if( rc != 0 ){` |
|     4 |  430 | `	 return -1;` |
|     - |  431 | `	}` |
|    28 |  432 | `	rc = S_ISLNK(st.st_mode);` |
|    28 |  433 | `	return rc ? PH7_OK : -1 ;` |
|    16 |  434 | `}` |
|     - |  435 | `/* int (*xReadable)(const char *) */` |
|     8 |  436 | `static int UnixVfs_isreadable(const char *zPath)` |
|     - |  437 | `{` |
|     8 |  438 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  439 | `	int rc;` |
|     8 |  440 | `	rc = access(zPath,R_OK);` |
|     8 |  441 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  442 | `}` |
|     - |  443 | `/* int (*xWritable)(const char *) */` |
|   621 |  444 | `static int UnixVfs_iswritable(const char *zPath)` |
|     - |  445 | `{` |
|   621 |  446 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  447 | `	int rc;` |
|   621 |  448 | `	rc = access(zPath,W_OK);` |
|   621 |  449 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  450 | `}` |
|     - |  451 | `/* int (*xExecutable)(const char *) */` |
|     4 |  452 | `static int UnixVfs_isexecutable(const char *zPath)` |
|     - |  453 | `{` |
|     4 |  454 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  455 | `	int rc;` |
|     4 |  456 | `	rc = access(zPath,X_OK);` |
|     4 |  457 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  458 | `}` |
|     - |  459 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|    28 |  460 | `static int UnixVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|     - |  461 | `{` |
|    28 |  462 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  463 | `	struct stat st;` |
|     - |  464 | `	int rc;` |
|     - |  465 | `	/* php's FS_TYPE lstats, which is why filetype() answers "link" for a symlink` |
|     - |  466 | `	 * and "file" for what it points at. stat() here made the "link" arm below` |
|     - |  467 | `	 * unreachable. */` |
|    28 |  468 | `    rc = lstat(zPath,&st);` |
|    28 |  469 | `	if( rc != 0 ){` |
|     - |  470 | `	  /* Expand 'unknown' */` |
|     6 |  471 | `	  ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|     6 |  472 | `	  return -1;` |
|     - |  473 | `	}` |
|    22 |  474 | `	if(S_ISREG(st.st_mode) ){` |
|    10 |  475 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|    17 |  476 | `	}else if(S_ISDIR(st.st_mode)){` |
|     8 |  477 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|     8 |  478 | `	}else if(S_ISLNK(st.st_mode)){` |
|     4 |  479 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|     2 |  480 | `	}else if(S_ISBLK(st.st_mode)){` |
|   ! 0 |  481 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|   ! 0 |  482 | `    }else if(S_ISSOCK(st.st_mode)){` |
|   ! 0 |  483 | `		ph7_result_string(pCtx,"socket",sizeof("socket")-1);` |
|   ! 0 |  484 | `	}else if(S_ISFIFO(st.st_mode)){` |
|   ! 0 |  485 | `       ph7_result_string(pCtx,"fifo",sizeof("fifo")-1);` |
|   ! 0 |  486 | `	}else{` |
|   ! 0 |  487 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|     - |  488 | `	}` |
|    22 |  489 | `	return PH7_OK;` |
|    14 |  490 | `}` |
|     - |  491 | `/* int (*xReadlink)(const char *,ph7_context *) */` |
|    10 |  492 | `static int UnixVfs_Readlink(const char *zPath,ph7_context *pCtx)` |
|     - |  493 | `{` |
|     - |  494 | `	char zBuf[4096];` |
|     - |  495 | `	ssize_t n;` |
|    10 |  496 | `	zPath = UnixVfsLocalPath(zPath);` |
|    10 |  497 | `	n = readlink(zPath,zBuf,sizeof(zBuf));` |
|    10 |  498 | `	if( n < 0 ){` |
|     6 |  499 | `		return -1;` |
|     - |  500 | `	}` |
|     - |  501 | `	/* readlink() does NOT null-terminate, and truncates silently when the target` |
|     - |  502 | `	 * does not fit -- the length is the only thing that says what was read. */` |
|     4 |  503 | `	if( (size_t)n >= sizeof(zBuf) ){` |
|   ! 0 |  504 | `		n = (ssize_t)sizeof(zBuf) - 1;` |
|   ! 0 |  505 | `	}` |
|     4 |  506 | `	ph7_result_string(pCtx,zBuf,(int)n);` |
|     4 |  507 | `	return PH7_OK;` |
|     5 |  508 | `}` |
|     - |  509 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|   330 |  510 | `static int UnixVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|     - |  511 | `{` |
|     - |  512 | `	char *zEnv;` |
|   330 |  513 | `	zEnv = getenv(zVar);` |
|   330 |  514 | `	if( zEnv == 0 ){` |
|    92 |  515 | `	  return -1;` |
|     - |  516 | `	}` |
|   238 |  517 | `	ph7_result_string(pCtx,zEnv,-1/*Compute length automatically*/);` |
|   238 |  518 | `	return PH7_OK;` |
|   165 |  519 | `}` |
|     - |  520 | `/* int (*xSetenv)(const char *,const char *) */` |
|    73 |  521 | `static int UnixVfs_Setenv(const char *zName,const char *zValue)` |
|     - |  522 | `{` |
|     - |  523 | `   int rc;` |
|    73 |  524 | `   if( zValue == 0 ){` |
|     - |  525 | `     /* php's putenv("NAME") with no '=' REMOVES the variable. */` |
|    24 |  526 | `     rc = unsetenv(zName);` |
|    24 |  527 | `     return rc == 0 ? PH7_OK : -1;` |
|     - |  528 | `   }` |
|    49 |  529 | `   rc = setenv(zName,zValue,1);` |
|    49 |  530 | `   return rc == 0 ? PH7_OK : -1;` |
|    29 |  531 | `}` |
|     - |  532 | `/* int (*xEnviron)(ph7_context *) */` |
|     8 |  533 | `static int UnixVfs_Environ(ph7_context *pCtx)` |
|     - |  534 | `{` |
|     - |  535 | `	extern char **environ;` |
|     - |  536 | `	ph7_value *pArray,*pKey,*pVal;` |
|     - |  537 | `	char **pp;` |
|     8 |  538 | `	pArray = ph7_context_new_array(pCtx);` |
|     8 |  539 | `	pKey = ph7_context_new_scalar(pCtx);` |
|     8 |  540 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     8 |  541 | `	if( pArray == 0 \|\| pKey == 0 \|\| pVal == 0 ){` |
|   ! 0 |  542 | `		return -1;` |
|     - |  543 | `	}` |
|  1002 |  544 | `	for( pp = environ ; pp && *pp ; ++pp ){` |
|   994 |  545 | `		const char *zEq = *pp;` |
| 16114 |  546 | `		while( *zEq && *zEq != '=' ){` |
| 15120 |  547 | `			zEq++;` |
|     - |  548 | `		}` |
|   994 |  549 | `		if( *zEq != '=' ){` |
|     - |  550 | `			/* No '=' at all: not an assignment, skip it like php's own loop. */` |
|   ! 0 |  551 | `			continue;` |
|     - |  552 | `		}` |
|   994 |  553 | `		ph7_value_string(pKey,*pp,(int)(zEq - *pp));` |
|   994 |  554 | `		ph7_value_string(pVal,zEq+1,-1);` |
|   994 |  555 | `		ph7_array_add_elem(pArray,pKey,pVal);` |
|   994 |  556 | `		ph7_value_reset_string_cursor(pKey);` |
|   994 |  557 | `		ph7_value_reset_string_cursor(pVal);` |
|   481 |  558 | `	}` |
|     8 |  559 | `	ph7_result_value(pCtx,pArray);` |
|     8 |  560 | `	return PH7_OK;` |
|     4 |  561 | `}` |
|     - |  562 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|  7339 |  563 | `static int UnixVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|     - |  564 | `{` |
|  7339 |  565 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  566 | `	struct stat st;` |
|     - |  567 | `	void *pMap;` |
|     - |  568 | `	int fd;` |
|     - |  569 | `	int rc;` |
|     - |  570 | `	/* Open the file in a read-only mode */` |
|  7339 |  571 | `	fd = open(zPath,O_RDONLY);` |
|  7339 |  572 | `	if( fd < 0 ){` |
|     8 |  573 | `		return -1;` |
|     - |  574 | `	}` |
|     - |  575 | `	/* stat the handle */` |
|  7331 |  576 | `	fstat(fd,&st);` |
|  7331 |  577 | `	if( S_ISREG(st.st_mode) && st.st_size == 0 ){` |
|     - |  578 | `		/* A 0-byte file is a legal PHP program: php runs it and prints nothing.` |
|     - |  579 | `		 * mmap() refuses a zero length with EINVAL, so this used to come back as` |
|     - |  580 | ``		 * an IO error and `phl empty.php` answered "Could not open input file"`` |
|     - |  581 | ``		 * where `php -l` says no syntax errors. Hand back an EMPTY view instead;`` |
|     - |  582 | `		 * the caller does not unmap one (api.c), so its pointer is never freed.` |
|     - |  583 | `		 * REGULAR files only: a pipe or a character device also stats as 0 and is` |
|     - |  584 | `		 * not empty -- neither was ever mappable, and both stay an IO error. */` |
|     4 |  585 | `		*ppMap = (void *)"";` |
|     4 |  586 | `		*pSize = 0;` |
|     4 |  587 | `		close(fd);` |
|     4 |  588 | `		return PH7_OK;` |
|     - |  589 | `	}` |
|     - |  590 | `	/* Obtain a memory view of the whole file */` |
|  7327 |  591 | `	pMap = mmap(0,st.st_size,PROT_READ,MAP_PRIVATE\|MAP_FILE,fd,0);` |
|  7327 |  592 | `	rc = PH7_OK;` |
|  7327 |  593 | `	if( pMap == MAP_FAILED ){` |
|   ! 0 |  594 | `		rc = -1;` |
|   ! 0 |  595 | `	}else{` |
|     - |  596 | `		/* Point to the memory view */` |
|  7327 |  597 | `		*ppMap = pMap;` |
|  7327 |  598 | `		*pSize = (ph7_int64)st.st_size;` |
|     - |  599 | `	}` |
|  7327 |  600 | `	close(fd);` |
|  7327 |  601 | `	return rc;` |
|  3664 |  602 | `}` |
|     - |  603 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|  7327 |  604 | `static void UnixVfs_Unmap(void *pView,ph7_int64 nSize)` |
|     - |  605 | `{` |
|  7327 |  606 | `	munmap(pView,(size_t)nSize);` |
|  7327 |  607 | `}` |
|     - |  608 | `/* void (*xTempDir)(ph7_context *) */` |
|  1704 |  609 | `static void UnixVfs_TempDir(ph7_context *pCtx)` |
|     - |  610 | `{` |
|     - |  611 | `  const char *zDir;` |
|     - |  612 | `  /* php's php_get_temporary_directory: honour TMPDIR, else fall back to P_tmpdir` |
|     - |  613 | `   * ("/tmp" on Unix). PH7 also scanned /var/tmp, /usr/tmp, /usr/local/tmp first,` |
|     - |  614 | `   * which returned /var/tmp on a typical box where php returns /tmp — a divergence` |
|     - |  615 | `   * observable through sys_get_temp_dir()/tempnam()/session paths. */` |
|  1704 |  616 | `  zDir = getenv("TMPDIR");` |
|  1704 |  617 | `  if( zDir && zDir[0] != 0 && !access(zDir,07) ){` |
|     - |  618 | `	  /* php reports the temp dir WITHOUT a trailing separator; macOS's TMPDIR ends with` |
|     - |  619 | `	   * one, so returning it verbatim produced paths like "/var/.../T//file". */` |
|   839 |  620 | `	  int nDir = (int)strlen(zDir);` |
|  1678 |  621 | `	  while( nDir > 1 && zDir[nDir-1] == '/' ){` |
|   839 |  622 | `		  nDir--;` |
|     - |  623 | `	  }` |
|   839 |  624 | `	  ph7_result_string(pCtx,zDir,nDir);` |
|   839 |  625 | `	  return;` |
|     - |  626 | `  }` |
|   865 |  627 | `  ph7_result_string(pCtx,"/tmp",(int)sizeof("/tmp")-1);` |
|   839 |  628 | `}` |
|     - |  629 | `/* unsigned int (*xProcessId)(void) */` |
|   476 |  630 | `static unsigned int UnixVfs_ProcessId(void)` |
|     - |  631 | `{` |
|   476 |  632 | `	return (unsigned int)getpid();` |
|     - |  633 | `}` |
|     - |  634 | `/* int (*xUid)(void) */` |
|    54 |  635 | `static int UnixVfs_uid(void)` |
|     - |  636 | `{` |
|    54 |  637 | `	return (int)getuid();` |
|     - |  638 | `}` |
|     - |  639 | `/* int (*xGid)(void) */` |
|    22 |  640 | `static int UnixVfs_gid(void)` |
|     - |  641 | `{` |
|    22 |  642 | `	return (int)getgid();` |
|     - |  643 | `}` |
|     - |  644 | `/* int (*xUmask)(int) */` |
|     8 |  645 | `static int UnixVfs_Umask(int new_mask)` |
|     - |  646 | `{` |
|     - |  647 | `	int old_mask;` |
|     8 |  648 | `	old_mask = umask(new_mask);` |
|     8 |  649 | `	return old_mask;` |
|     - |  650 | `}` |
|     - |  651 | `/* void (*xUsername)(ph7_context *) */` |
|     2 |  652 | `static void UnixVfs_Username(ph7_context *pCtx)` |
|     - |  653 | `{` |
|     - |  654 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  655 | `  struct passwd *pwd;` |
|     - |  656 | `  uid_t uid;` |
|     2 |  657 | `  uid = getuid();` |
|     2 |  658 | `  pwd = getpwuid(uid);   /* Try getting UID for username */` |
|     2 |  659 | `  if (pwd == 0) {` |
|   ! 0 |  660 | `    return;` |
|     - |  661 | `  }` |
|     - |  662 | `  /* Return the username */` |
|     2 |  663 | `  ph7_result_string(pCtx,pwd->pw_name,-1);` |
|     - |  664 | `#else` |
|     - |  665 | `  ph7_result_string(pCtx,"Unknown",-1);` |
|     - |  666 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     2 |  667 | `  return;` |
|     1 |  668 | `}` |
|     - |  669 | `/* int (*xLink)(const char *,const char *,int) */` |
|    30 |  670 | `static int UnixVfs_link(const char *zSrc,const char *zTarget,int is_sym)` |
|     - |  671 | `{` |
|    30 |  672 | `	zSrc = UnixVfsLocalPath(zSrc);` |
|    30 |  673 | `	zTarget = UnixVfsLocalPath(zTarget);` |
|     - |  674 | `	int rc;` |
|    30 |  675 | `	if( is_sym ){` |
|     - |  676 | `		/* Symbolic link */` |
|    20 |  677 | `		rc = symlink(zSrc,zTarget);` |
|    10 |  678 | `	}else{` |
|     - |  679 | `		/* Hard link */` |
|    10 |  680 | `		rc = link(zSrc,zTarget);` |
|     - |  681 | `	}` |
|    30 |  682 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  683 | `}` |
|     - |  684 | `/* int (*xChroot)(const char *) */` |
|   ! 0 |  685 | `static int UnixVfs_chroot(const char *zRootDir)` |
|     - |  686 | `{` |
|     - |  687 | `	int rc;` |
|   ! 0 |  688 | `	rc = chroot(zRootDir);` |
|   ! 0 |  689 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  690 | `}` |
|     - |  691 | `/* Export the UNIX vfs */` |
|     - |  692 | `PH7_PRIVATE const ph7_vfs sUnixVfs = {` |
|     - |  693 | `	"Unix_vfs",` |
|     - |  694 | `	PH7_VFS_VERSION,` |
|     - |  695 | `	UnixVfs_chdir,    /* int (*xChdir)(const char *) */` |
|     - |  696 | `	UnixVfs_chroot,   /* int (*xChroot)(const char *); */` |
|     - |  697 | `	UnixVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|     - |  698 | `	UnixVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|     - |  699 | `	UnixVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|     - |  700 | `	UnixVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|     - |  701 | `	UnixVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|     - |  702 | `	UnixVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|     - |  703 | `	UnixVfs_Sleep,    /* int (*xSleep)(unsigned int) */` |
|     - |  704 | `	UnixVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|     - |  705 | `	UnixVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|     - |  706 | `	UnixVfs_Chmod, /*int (*xChmod)(const char *,int)*/` |
|     - |  707 | `	UnixVfs_Chown, /*int (*xChown)(const char *,const char *)*/` |
|     - |  708 | `	UnixVfs_Chgrp, /*int (*xChgrp)(const char *,const char *)*/` |
|     - |  709 | `	UnixVfs_FreeSpace,  /* ph7_int64 (*xFreeSpace)(const char *) */` |
|     - |  710 | `	UnixVfs_TotalSpace, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|     - |  711 | `	UnixVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|     - |  712 | `	UnixVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|     - |  713 | `	UnixVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|     - |  714 | `	UnixVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|     - |  715 | `	UnixVfs_Stat,  /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|     - |  716 | `	UnixVfs_lStat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|     - |  717 | `	UnixVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|     - |  718 | `	UnixVfs_islink,     /* int (*xIslink)(const char *) */` |
|     - |  719 | `	UnixVfs_isreadable, /* int (*xReadable)(const char *) */` |
|     - |  720 | `	UnixVfs_iswritable, /* int (*xWritable)(const char *) */` |
|     - |  721 | `	UnixVfs_isexecutable,/* int (*xExecutable)(const char *) */` |
|     - |  722 | `	UnixVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|     - |  723 | `	UnixVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|     - |  724 | `	UnixVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|     - |  725 | `	UnixVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|     - |  726 | `	UnixVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|     - |  727 | `	UnixVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|     - |  728 | `	UnixVfs_link,       /* int (*xLink)(const char *,const char *,int) */` |
|     - |  729 | `	UnixVfs_Umask,      /* int (*xUmask)(int) */` |
|     - |  730 | `	UnixVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|     - |  731 | `	UnixVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|     - |  732 | `	UnixVfs_uid, /* int (*xUid)(void) */` |
|     - |  733 | `	UnixVfs_gid, /* int (*xGid)(void) */` |
|     - |  734 | `	UnixVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|     - |  735 | `	0, /* int (*xExec)(const char *,ph7_context *) */` |
|     - |  736 | `	UnixVfs_Readlink, /* int (*xReadlink)(const char *,ph7_context *) */` |
|     - |  737 | `	UnixVfs_Environ  /* int (*xEnviron)(ph7_context *) */` |
|     - |  738 | `};` |
|     - |  739 | `/* UNIX File IO */` |
|     - |  740 | `#define PH7_UNIX_OPEN_MODE	0640 /* Default open mode */` |
|     - |  741 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
| 44721 |  742 | `static int UnixFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|     - |  743 | `{` |
| 44721 |  744 | `	int iOpen = O_RDONLY;` |
|     - |  745 | `	int fd;` |
|     - |  746 | `	/* Set the desired flags according to the open mode */` |
| 44721 |  747 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|     - |  748 | `		/* Open existing file, or create if it doesn't exist */` |
| 20947 |  749 | `		iOpen = O_CREAT;` |
| 20947 |  750 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|     - |  751 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
| 20877 |  752 | `			iOpen \|= O_TRUNC;` |
| 10430 |  753 | `			SXUNUSED(pResource); /* cc warning */` |
| 10430 |  754 | `		}` |
| 34239 |  755 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|     - |  756 | `		/* Creates a new file, only if it does not already exist.` |
|     - |  757 | `		* If the file exists, it fails.` |
|     - |  758 | `		*/` |
|   611 |  759 | `		iOpen = O_CREAT\|O_EXCL;` |
| 23468 |  760 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|     - |  761 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|     - |  762 | `		 * The file must exist.` |
|     - |  763 | `		 */` |
|   ! 0 |  764 | `		iOpen = O_RDWR\|O_TRUNC;` |
|   ! 0 |  765 | `	}` |
| 44721 |  766 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|     - |  767 | `		/* Read+Write access */` |
|    54 |  768 | `		iOpen &= ~O_RDONLY;` |
|    54 |  769 | `		iOpen \|= O_RDWR;` |
| 44694 |  770 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|     - |  771 | `		/* Write only access */` |
| 21536 |  772 | `		iOpen &= ~O_RDONLY;` |
| 21536 |  773 | `		iOpen \|= O_WRONLY;` |
| 10759 |  774 | `	}` |
| 44721 |  775 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|     - |  776 | `		/* Append mode */` |
|    58 |  777 | `		iOpen \|= O_APPEND;` |
|    29 |  778 | `	}` |
|     - |  779 | `#ifdef O_TEMP` |
|     - |  780 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|     - |  781 | `		/* File is temporary */` |
|     - |  782 | `		iOpen \|= O_TEMP;` |
|     - |  783 | `	}` |
|     - |  784 | `#endif` |
|     - |  785 | `	/* Open the file now */` |
| 44721 |  786 | `	fd = open(zPath,iOpen,PH7_UNIX_OPEN_MODE);` |
| 44721 |  787 | `	if( fd < 0 ){` |
|     - |  788 | `		/* IO error */` |
|   288 |  789 | `		return -1;` |
|     - |  790 | `	}` |
|     - |  791 | `	/* Save the handle */` |
| 44433 |  792 | `	*ppHandle = SX_INT_TO_PTR(fd);` |
| 44433 |  793 | `	return PH7_OK;` |
| 22299 |  794 | `}` |
|     - |  795 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|  1985 |  796 | `static int UnixDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|     - |  797 | `{` |
|     - |  798 | `	DIR *pDir;` |
|     - |  799 | `	/* Open the target directory */` |
|  1985 |  800 | `	pDir = opendir(zPath);` |
|  1985 |  801 | `	if( pDir == 0 ){` |
|    12 |  802 | `		SXUNUSED(pResource); /* Compiler warning */` |
|    24 |  803 | `		return -1;` |
|     - |  804 | `	}` |
|     - |  805 | `	/* Save our structure */` |
|  1961 |  806 | `	*ppHandle = pDir;` |
|  1961 |  807 | `	return PH7_OK;` |
|   980 |  808 | `}` |
|     - |  809 | `/* void (*xCloseDir)(void *) */` |
|  1961 |  810 | `static void UnixDir_Close(void *pUserData)` |
|     - |  811 | `{` |
|  1961 |  812 | `	closedir((DIR *)pUserData);` |
|  1961 |  813 | `}` |
|     - |  814 | `/* void (*xClose)(void *); */` |
| 45767 |  815 | `static void UnixFile_Close(void *pUserData)` |
|     - |  816 | `{` |
| 45767 |  817 | `	close(SX_PTR_TO_INT(pUserData));` |
| 45767 |  818 | `}` |
|     - |  819 | `/* int (*xReadDir)(void *,ph7_context *) */` |
| 19823 |  820 | `static int UnixDir_Read(void *pUserData,ph7_context *pCtx)` |
|     - |  821 | `{` |
| 19823 |  822 | `	DIR *pDir = (DIR *)pUserData;` |
|     - |  823 | `	struct dirent *pEntry;` |
|     - |  824 | `	char *zName;` |
|     - |  825 | `	sxu32 n;` |
|     - |  826 | `	/* php's readdir() yields every entry including '.' and '..' */` |
| 19823 |  827 | `	pEntry = readdir(pDir);` |
| 19823 |  828 | `	if( pEntry == 0 ){` |
|     - |  829 | `		/* No more entries to process */` |
|  1891 |  830 | `		return -1;` |
|     - |  831 | `	}` |
| 17932 |  832 | `	zName = pEntry->d_name;` |
| 17932 |  833 | `	n = SyStrlen(zName);` |
|     - |  834 | `	/* Return the current file name */` |
| 17932 |  835 | `	ph7_result_string(pCtx,zName,(int)n);` |
| 17932 |  836 | `	return PH7_OK;` |
|  9949 |  837 | `}` |
|     - |  838 | `/* void (*xRewindDir)(void *) */` |
|    50 |  839 | `static void UnixDir_Rewind(void *pUserData)` |
|     - |  840 | `{` |
|    50 |  841 | `	rewinddir((DIR *)pUserData);` |
|    50 |  842 | `}` |
|     - |  843 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
| 47107 |  844 | `static ph7_int64 UnixFile_Read(void *pUserData,void *pBuffer,ph7_int64 nDatatoRead)` |
|     - |  845 | `{` |
|     - |  846 | `	ssize_t nRd;` |
| 47107 |  847 | `	nRd = read(SX_PTR_TO_INT(pUserData),pBuffer,(size_t)nDatatoRead);` |
| 47107 |  848 | `	if( nRd < 0 ){` |
|     - |  849 | `		/* An IO error. EOF is read()'s ZERO and rides through as one: it is not a` |
|     - |  850 | `		 * failure, and the readers above need to tell the two apart — fread() at` |
|     - |  851 | `		 * EOF is php's "" and only a real error is its false. Every consumer of` |
|     - |  852 | ``		 * this driver stops on `< 1`, so both still end a read loop. The Windows`` |
|     - |  853 | `		 * driver already answers this way (ReadFile succeeds with 0 bytes). */` |
|    98 |  854 | `		return -1;` |
|     - |  855 | `	}` |
| 47009 |  856 | `	return (ph7_int64)nRd;` |
| 23503 |  857 | `}` |
|     - |  858 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
| 20811 |  859 | `static ph7_int64 UnixFile_Write(void *pUserData,const void *pBuffer,ph7_int64 nWrite)` |
|     - |  860 | `{` |
| 20811 |  861 | `	const char *zData = (const char *)pBuffer;` |
| 20811 |  862 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|     - |  863 | `	ph7_int64 nCount;` |
|     - |  864 | `	ssize_t nWr;` |
| 20811 |  865 | `	nCount = 0;` |
| 20783 |  866 | `	for(;;){` |
| 41592 |  867 | `		if( nWrite < 1 ){` |
| 20789 |  868 | `			break;` |
|     - |  869 | `		}` |
| 20803 |  870 | `		nWr = write(fd,zData,(size_t)nWrite);` |
| 20803 |  871 | `		if( nWr < 1 ){` |
|     - |  872 | `			/* IO error */` |
|    22 |  873 | `			break;` |
|     - |  874 | `		}` |
| 20781 |  875 | `		nWrite -= nWr;` |
| 20781 |  876 | `		nCount += nWr;` |
| 20781 |  877 | `		zData += nWr;` |
|     - |  878 | `	}` |
| 20811 |  879 | `	if( nWrite > 0 ){` |
|    22 |  880 | `		return -1;` |
|     - |  881 | `	}` |
| 20789 |  882 | `	return nCount;` |
| 10399 |  883 | `}` |
|     - |  884 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|   150 |  885 | `static int UnixFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|     - |  886 | `{` |
|     - |  887 | `	off_t iNew;` |
|   150 |  888 | `	switch(whence){` |
|     6 |  889 | `	case 1:/*SEEK_CUR*/` |
|    12 |  890 | `		whence = SEEK_CUR;` |
|    12 |  891 | `		break;` |
|     2 |  892 | `	case 2: /* SEEK_END */` |
|     4 |  893 | `		whence = SEEK_END;` |
|     4 |  894 | `		break;` |
|   134 |  895 | `	case 0: /* SEEK_SET */` |
|     - |  896 | `	default:` |
|   134 |  897 | `		whence = SEEK_SET;` |
|   134 |  898 | `		break;` |
|     - |  899 | `	}` |
|   150 |  900 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),(off_t)iOfft,whence);` |
|   150 |  901 | `	if( iNew < 0 ){` |
|   ! 0 |  902 | `		return -1;` |
|     - |  903 | `	}` |
|   150 |  904 | `	return PH7_OK;` |
|    75 |  905 | `}` |
|     - |  906 | `/* int (*xLock)(void *,int) — see the lock_type value space in ph7.h */` |
|    32 |  907 | `static int UnixFile_Lock(void *pUserData,int lock_type)` |
|     - |  908 | `{` |
|    32 |  909 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|     - |  910 | `	int op;` |
|     - |  911 | `	int rc;` |
|    32 |  912 | `	if( lock_type < 0 ){` |
|     - |  913 | `		/* Unlock the file */` |
|    12 |  914 | `		op = LOCK_UN;` |
|     6 |  915 | `	}else{` |
|    20 |  916 | `		op = (lock_type == PH7_IO_LOCK_EX \|\| lock_type == PH7_IO_LOCK_EX_NB) ? LOCK_EX : LOCK_SH;` |
|    20 |  917 | `		if( lock_type == PH7_IO_LOCK_SH_NB \|\| lock_type == PH7_IO_LOCK_EX_NB ){` |
|    14 |  918 | `			op \|= LOCK_NB;` |
|     7 |  919 | `		}` |
|     - |  920 | `	}` |
|    32 |  921 | `	rc = flock(fd,op);` |
|    32 |  922 | `	if( rc == 0 ){` |
|    28 |  923 | `		return PH7_OK;` |
|     - |  924 | `	}` |
|     - |  925 | `	/* A non-blocking request that another holder refused is php's $would_block,` |
|     - |  926 | `	 * not an IO failure. (EWOULDBLOCK and EAGAIN are the same value on every` |
|     - |  927 | `	 * platform this driver builds for.) */` |
|     4 |  928 | `	if( errno == EWOULDBLOCK ){` |
|     4 |  929 | `		return SXERR_BUSY;` |
|     - |  930 | `	}` |
|   ! 0 |  931 | `	return -1;` |
|    16 |  932 | `}` |
|     - |  933 | `/* ph7_int64 (*xTell)(void *) */` |
|  2136 |  934 | `static ph7_int64 UnixFile_Tell(void *pUserData)` |
|     - |  935 | `{` |
|     - |  936 | `	off_t iNew;` |
|  2136 |  937 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),0,SEEK_CUR);` |
|  2136 |  938 | `	return (ph7_int64)iNew;` |
|     - |  939 | `}` |
|     - |  940 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|     6 |  941 | `static int UnixFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|     - |  942 | `{` |
|     - |  943 | `	int rc;` |
|     6 |  944 | `	rc = ftruncate(SX_PTR_TO_INT(pUserData),(off_t)nOfft);` |
|     6 |  945 | `	if( rc != 0 ){` |
|   ! 0 |  946 | `		return -1;` |
|     - |  947 | `	}` |
|     6 |  948 | `	return PH7_OK;` |
|     3 |  949 | `}` |
|     - |  950 | `/* int (*xSync)(void *); */` |
|     6 |  951 | `static int UnixFile_Sync(void *pUserData)` |
|     - |  952 | `{` |
|     - |  953 | `	int rc;` |
|     6 |  954 | `	rc = fsync(SX_PTR_TO_INT(pUserData));` |
|     6 |  955 | `	return rc == 0 ? PH7_OK : - 1;` |
|     - |  956 | `}` |
|     - |  957 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|    32 |  958 | `static int UnixFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  959 | `{` |
|     - |  960 | `	struct stat st;` |
|     - |  961 | `	int rc;` |
|    32 |  962 | `	rc = fstat(SX_PTR_TO_INT(pUserData),&st);` |
|    32 |  963 | `	if( rc != 0 ){` |
|   ! 0 |  964 | `	 return -1;` |
|     - |  965 | `	}` |
|     - |  966 | `	/* dev */` |
|    32 |  967 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|    32 |  968 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  969 | `	/* ino */` |
|    32 |  970 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|    32 |  971 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  972 | `	/* mode */` |
|    32 |  973 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|    32 |  974 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  975 | `	/* nlink */` |
|    32 |  976 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|    32 |  977 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  978 | `	/* uid,gid,rdev */` |
|    32 |  979 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|    32 |  980 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    32 |  981 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|    32 |  982 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    32 |  983 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|    32 |  984 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  985 | `	/* size */` |
|    32 |  986 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|    32 |  987 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  988 | `	/* atime */` |
|    32 |  989 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|    32 |  990 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  991 | `	/* mtime */` |
|    32 |  992 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|    32 |  993 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  994 | `	/* ctime */` |
|    32 |  995 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|    32 |  996 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  997 | `	/* blksize,blocks */` |
|    32 |  998 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|    32 |  999 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    32 | 1000 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|    32 | 1001 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    32 | 1002 | `	return PH7_OK;` |
|    16 | 1003 | `}` |
|     - | 1004 | `/* Export the file:// stream */` |
|     - | 1005 | `PH7_PRIVATE const ph7_io_stream sUnixFileStream = {` |
|     - | 1006 | `	"file", /* Stream name */` |
|     - | 1007 | `	PH7_IO_STREAM_VERSION,` |
|     - | 1008 | `	UnixFile_Open,  /* xOpen */` |
|     - | 1009 | `	UnixDir_Open,   /* xOpenDir */` |
|     - | 1010 | `	UnixFile_Close, /* xClose */` |
|     - | 1011 | `	UnixDir_Close,  /* xCloseDir */` |
|     - | 1012 | `	UnixFile_Read,  /* xRead */` |
|     - | 1013 | `	UnixDir_Read,   /* xReadDir */` |
|     - | 1014 | `	UnixFile_Write, /* xWrite */` |
|     - | 1015 | `	UnixFile_Seek,  /* xSeek */` |
|     - | 1016 | `	UnixFile_Lock,  /* xLock */` |
|     - | 1017 | `	UnixDir_Rewind, /* xRewindDir */` |
|     - | 1018 | `	UnixFile_Tell,  /* xTell */` |
|     - | 1019 | `	UnixFile_Trunc, /* xTrunc */` |
|     - | 1020 | `	UnixFile_Sync,  /* xSeek */` |
|     - | 1021 | `	UnixFile_Stat   /* xStat */` |
|     - | 1022 | `};` |
|     - | 1023 | `#endif /* __UNIXES__ */` |
|     - | 1024 |  |
