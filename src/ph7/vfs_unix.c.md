# src/ph7/vfs_unix.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 455/494 lines (92.11%)

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
| 61956 |   40 | `static const char * UnixVfsLocalPath(const char *zPath)` |
|     - |   41 | `{` |
|     - |   42 | `	const char *zRest;` |
| 61956 |   43 | `	if( zPath == 0 \|\| SyStrnicmp(zPath,"file://",sizeof("file://")-1) != 0 ){` |
| 61946 |   44 | `		return zPath;` |
|     - |   45 | `	}` |
|    10 |   46 | `	zRest = &zPath[sizeof("file://")-1];` |
|    10 |   47 | `	if( zRest[0] == '/' ){` |
|     - |   48 | `		/* file:///abs -> /abs (empty authority) */` |
|    10 |   49 | `		return zRest;` |
|     - |   50 | `	}` |
|   ! 0 |   51 | `	if( SyStrnicmp(zRest,"localhost/",sizeof("localhost/")-1) == 0 ){` |
|     - |   52 | `		/* file://localhost/abs -> /abs (keep the leading slash) */` |
|   ! 0 |   53 | `		return &zRest[sizeof("localhost")-1];` |
|     - |   54 | `	}` |
|   ! 0 |   55 | `	return zPath;` |
| 30978 |   56 | `}` |
|     - |   57 | `/* int (*xchdir)(const char *) */` |
| 14486 |   58 | `static int UnixVfs_chdir(const char *zPath)` |
|     - |   59 | `{` |
|     - |   60 | `  int rc;` |
| 14486 |   61 | `  rc = chdir(zPath);` |
| 14486 |   62 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |   63 | `}` |
|     - |   64 | `/* int (*xGetcwd)(ph7_context *) */` |
|    18 |   65 | `static int UnixVfs_getcwd(ph7_context *pCtx)` |
|     - |   66 | `{` |
|     - |   67 | `	char zBuf[4096];` |
|     - |   68 | `	char *zDir;` |
|     - |   69 | `	/* Get the current directory */` |
|    18 |   70 | `	zDir = getcwd(zBuf,sizeof(zBuf));` |
|    18 |   71 | `	if( zDir == 0 ){` |
|   ! 0 |   72 | `	  return -1;` |
|     - |   73 | `    }` |
|    18 |   74 | `	ph7_result_string(pCtx,zDir,-1/*Compute length automatically*/);` |
|    18 |   75 | `	return PH7_OK;` |
|     9 |   76 | `}` |
|     - |   77 | `/* int (*xMkdir)(const char *,int,int) */` |
|    66 |   78 | `static int UnixVfs_mkdir(const char *zPath,int mode,int recursive)` |
|     - |   79 | `{` |
|    66 |   80 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |   81 | `	int rc;` |
|    66 |   82 | `        rc = mkdir(zPath,mode);` |
|    33 |   83 | `	SXUNUSED(recursive); /* cc warning */` |
|    66 |   84 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   85 | `}` |
|     - |   86 | `/* int (*xRmdir)(const char *) */` |
|    66 |   87 | `static int UnixVfs_rmdir(const char *zPath)` |
|     - |   88 | `{` |
|    66 |   89 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |   90 | `	int rc;` |
|    66 |   91 | `	rc = rmdir(zPath);` |
|    66 |   92 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   93 | `}` |
|     - |   94 | `/* int (*xIsdir)(const char *) */` |
| 10042 |   95 | `static int UnixVfs_isdir(const char *zPath)` |
|     - |   96 | `{` |
| 10042 |   97 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |   98 | `	struct stat st;` |
|     - |   99 | `	int rc;` |
| 10042 |  100 | `	rc = stat(zPath,&st);` |
| 10042 |  101 | `	if( rc != 0 ){` |
|     6 |  102 | `	 return -1;` |
|     - |  103 | `	}` |
| 10036 |  104 | `	rc = S_ISDIR(st.st_mode);` |
| 10036 |  105 | `	return rc ? PH7_OK : -1 ;` |
|  5021 |  106 | `}` |
|     - |  107 | `/* int (*xRename)(const char *,const char *) */` |
|     2 |  108 | `static int UnixVfs_Rename(const char *zOld,const char *zNew)` |
|     - |  109 | `{` |
|     2 |  110 | `	zOld = UnixVfsLocalPath(zOld);` |
|     2 |  111 | `	zNew = UnixVfsLocalPath(zNew);` |
|     - |  112 | `	int rc;` |
|     2 |  113 | `	rc = rename(zOld,zNew);` |
|     2 |  114 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  115 | `}` |
|     - |  116 | `/* int (*xRealpath)(const char *,ph7_context *) */` |
|    10 |  117 | `static int UnixVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|     - |  118 | `{` |
|     - |  119 | `#ifndef PH7_UNIX_OLD_LIBC` |
|     - |  120 | `	char *zReal;` |
|    10 |  121 | `	zReal = realpath(zPath,0);` |
|    10 |  122 | `	if( zReal == 0 ){` |
|     4 |  123 | `	  return -1;` |
|     - |  124 | `	}` |
|     6 |  125 | `	ph7_result_string(pCtx,zReal,-1/*Compute length automatically*/);` |
|     - |  126 | `        /* Release the allocated buffer */` |
|     6 |  127 | `	free(zReal);` |
|     6 |  128 | `	return PH7_OK;` |
|     - |  129 | `#else` |
|     - |  130 | `    zPath = 0; /* cc warning */` |
|     - |  131 | `    pCtx = 0;` |
|     - |  132 | `    return -1;` |
|     - |  133 | `#endif` |
|     5 |  134 | `}` |
|     - |  135 | `/* int (*xSleep)(unsigned int) */` |
|    64 |  136 | `static int UnixVfs_Sleep(unsigned int uSec)` |
|     - |  137 | `{` |
|    64 |  138 | `	usleep(uSec);` |
|    64 |  139 | `	return PH7_OK;` |
|     - |  140 | `}` |
|     - |  141 | `/* int (*xUnlink)(const char *) */` |
| 38696 |  142 | `static int UnixVfs_unlink(const char *zPath)` |
|     - |  143 | `{` |
| 38696 |  144 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  145 | `	int rc;` |
| 38696 |  146 | `	rc = unlink(zPath);` |
| 38696 |  147 | `	return rc == 0 ? PH7_OK : -1 ;` |
|     - |  148 | `}` |
|     - |  149 | `/* int (*xFileExists)(const char *) */` |
|   230 |  150 | `static int UnixVfs_FileExists(const char *zPath)` |
|     - |  151 | `{` |
|   230 |  152 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  153 | `	int rc;` |
|   230 |  154 | `	rc = access(zPath,F_OK);` |
|   230 |  155 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  156 | `}` |
|     - |  157 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|     - |  158 | `/* ph7_int64 (*xFreeSpace)(const char *) */` |
|    14 |  159 | `static ph7_int64 UnixVfs_FreeSpace(const char *zPath)` |
|     - |  160 | `{` |
|    14 |  161 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  162 | `	struct statvfs sInfo;` |
|    14 |  163 | `	if( statvfs(zPath,&sInfo) != 0 ){` |
|     4 |  164 | `		return -1;` |
|     - |  165 | `	}` |
|     - |  166 | `	/* php reports the space available to an UNPRIVILEGED user (f_bavail) */` |
|    10 |  167 | `	return (ph7_int64)sInfo.f_bavail * (ph7_int64)sInfo.f_frsize;` |
|     7 |  168 | `}` |
|     - |  169 | `/* ph7_int64 (*xTotalSpace)(const char *) */` |
|    10 |  170 | `static ph7_int64 UnixVfs_TotalSpace(const char *zPath)` |
|     - |  171 | `{` |
|    10 |  172 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  173 | `	struct statvfs sInfo;` |
|    10 |  174 | `	if( statvfs(zPath,&sInfo) != 0 ){` |
|     2 |  175 | `		return -1;` |
|     - |  176 | `	}` |
|     8 |  177 | `	return (ph7_int64)sInfo.f_blocks * (ph7_int64)sInfo.f_frsize;` |
|     5 |  178 | `}` |
|    14 |  179 | `static ph7_int64 UnixVfs_FileSize(const char *zPath)` |
|     - |  180 | `{` |
|    14 |  181 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  182 | `	struct stat st;` |
|     - |  183 | `	int rc;` |
|    14 |  184 | `	rc = stat(zPath,&st);` |
|    14 |  185 | `	if( rc != 0 ){` |
|     2 |  186 | `	 return -1;` |
|     - |  187 | `	}` |
|    12 |  188 | `	return (ph7_int64)st.st_size;` |
|     7 |  189 | `}` |
|     - |  190 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    18 |  191 | `static int UnixVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|     - |  192 | `{` |
|    18 |  193 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  194 | `	struct utimbuf ut;` |
|     - |  195 | `	int rc;` |
|     - |  196 | `	/* Both stamps are always real values here — the builtin resolves php's "now"` |
|     - |  197 | `	 * default, because a NEGATIVE timestamp is legal and so cannot be a sentinel. */` |
|    18 |  198 | `	ut.actime  = (time_t)access_time;` |
|    18 |  199 | `	ut.modtime = (time_t)touch_time;` |
|    18 |  200 | `	rc = utime(zPath,&ut);` |
|    18 |  201 | `	if( rc != 0 ){` |
|     - |  202 | `		/* php's touch() CREATES the file when it is missing — that is the idiom's` |
|     - |  203 | ``		 * main use, and utime() alone fails ENOENT there, so `touch($new)` answered`` |
|     - |  204 | `		 * false and created nothing. Try the create only after utime failed, like` |
|     - |  205 | `		 * php: no extra stat on the common path, and no access(2) real-uid check. */` |
|    14 |  206 | `		int fd = open(zPath,O_WRONLY\|O_CREAT,0666);` |
|    14 |  207 | `		if( fd < 0 ){` |
|   ! 0 |  208 | `			return -1;` |
|     - |  209 | `		}` |
|    14 |  210 | `		close(fd);` |
|    14 |  211 | `		rc = utime(zPath,&ut);` |
|    14 |  212 | `		if( rc != 0 ){` |
|   ! 0 |  213 | `			return -1;` |
|     - |  214 | `		}` |
|     7 |  215 | `	}` |
|    18 |  216 | `	return PH7_OK;` |
|     9 |  217 | `}` |
|     - |  218 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|     8 |  219 | `static ph7_int64 UnixVfs_FileAtime(const char *zPath)` |
|     - |  220 | `{` |
|     8 |  221 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  222 | `	struct stat st;` |
|     - |  223 | `	int rc;` |
|     8 |  224 | `	rc = stat(zPath,&st);` |
|     8 |  225 | `	if( rc != 0 ){` |
|     2 |  226 | `	 return -1;` |
|     - |  227 | `	}` |
|     6 |  228 | `	return (ph7_int64)st.st_atime;` |
|     4 |  229 | `}` |
|     - |  230 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|    10 |  231 | `static ph7_int64 UnixVfs_FileMtime(const char *zPath)` |
|     - |  232 | `{` |
|    10 |  233 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  234 | `	struct stat st;` |
|     - |  235 | `	int rc;` |
|    10 |  236 | `	rc = stat(zPath,&st);` |
|    10 |  237 | `	if( rc != 0 ){` |
|     2 |  238 | `	 return -1;` |
|     - |  239 | `	}` |
|     8 |  240 | `	return (ph7_int64)st.st_mtime;` |
|     5 |  241 | `}` |
|     - |  242 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|     6 |  243 | `static ph7_int64 UnixVfs_FileCtime(const char *zPath)` |
|     - |  244 | `{` |
|     6 |  245 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  246 | `	struct stat st;` |
|     - |  247 | `	int rc;` |
|     6 |  248 | `	rc = stat(zPath,&st);` |
|     6 |  249 | `	if( rc != 0 ){` |
|     2 |  250 | `	 return -1;` |
|     - |  251 | `	}` |
|     4 |  252 | `	return (ph7_int64)st.st_ctime;` |
|     3 |  253 | `}` |
|     - |  254 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    78 |  255 | `static int UnixVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  256 | `{` |
|    78 |  257 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  258 | `	struct stat st;` |
|     - |  259 | `	int rc;` |
|    78 |  260 | `	rc = stat(zPath,&st);` |
|    78 |  261 | `	if( rc != 0 ){` |
|    26 |  262 | `	 return -1;` |
|     - |  263 | `	}` |
|     - |  264 | `	/* dev */` |
|    52 |  265 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|    52 |  266 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  267 | `	/* ino */` |
|    52 |  268 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|    52 |  269 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  270 | `	/* mode */` |
|    52 |  271 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|    52 |  272 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  273 | `	/* nlink */` |
|    52 |  274 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|    52 |  275 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  276 | `	/* uid,gid,rdev */` |
|    52 |  277 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|    52 |  278 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    52 |  279 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|    52 |  280 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    52 |  281 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|    52 |  282 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  283 | `	/* size */` |
|    52 |  284 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|    52 |  285 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  286 | `	/* atime */` |
|    52 |  287 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|    52 |  288 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  289 | `	/* mtime */` |
|    52 |  290 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|    52 |  291 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  292 | `	/* ctime */` |
|    52 |  293 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|    52 |  294 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  295 | `	/* blksize,blocks */` |
|    52 |  296 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|    52 |  297 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    52 |  298 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|    52 |  299 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    52 |  300 | `	return PH7_OK;` |
|    39 |  301 | `}` |
|     - |  302 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|     6 |  303 | `static int UnixVfs_lStat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  304 | `{` |
|     6 |  305 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  306 | `	struct stat st;` |
|     - |  307 | `	int rc;` |
|     6 |  308 | `	rc = lstat(zPath,&st);` |
|     6 |  309 | `	if( rc != 0 ){` |
|     2 |  310 | `	 return -1;` |
|     - |  311 | `	}` |
|     - |  312 | `	/* dev */` |
|     4 |  313 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|     4 |  314 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  315 | `	/* ino */` |
|     4 |  316 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|     4 |  317 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  318 | `	/* mode */` |
|     4 |  319 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|     4 |  320 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  321 | `	/* nlink */` |
|     4 |  322 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|     4 |  323 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  324 | `	/* uid,gid,rdev */` |
|     4 |  325 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|     4 |  326 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|     4 |  327 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|     4 |  328 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|     4 |  329 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|     4 |  330 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  331 | `	/* size */` |
|     4 |  332 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|     4 |  333 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  334 | `	/* atime */` |
|     4 |  335 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|     4 |  336 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  337 | `	/* mtime */` |
|     4 |  338 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|     4 |  339 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  340 | `	/* ctime */` |
|     4 |  341 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|     4 |  342 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  343 | `	/* blksize,blocks */` |
|     4 |  344 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|     4 |  345 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|     4 |  346 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|     4 |  347 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|     4 |  348 | `	return PH7_OK;` |
|     3 |  349 | `}` |
|     - |  350 | `/* int (*xChmod)(const char *,int) */` |
|   168 |  351 | `static int UnixVfs_Chmod(const char *zPath,int mode)` |
|     - |  352 | `{` |
|   168 |  353 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  354 | `    int rc;` |
|   168 |  355 | `    rc = chmod(zPath,(mode_t)mode);` |
|   168 |  356 | `    return rc == 0 ? PH7_OK : - 1;` |
|     - |  357 | `}` |
|     - |  358 | `/* int (*xChown)(const char *,const char *) */` |
|     6 |  359 | `static int UnixVfs_Chown(const char *zPath,const char *zUser)` |
|     - |  360 | `{` |
|     6 |  361 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  362 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  363 | `  struct passwd *pwd;` |
|     - |  364 | `  uid_t uid;` |
|     - |  365 | `  int rc;` |
|     - |  366 | `  /* php accepts a numeric uid as well as a name; PH7 only ever did getpwnam(), so` |
|     - |  367 | `   * chown($f, 0) failed on the NAME lookup and never reached the syscall (leaving errno` |
|     - |  368 | `   * unset, hence a bogus "Undefined error: 0" in the warning). */` |
|     6 |  369 | `  if( zUser[0] >= '0' && zUser[0] <= '9' ){` |
|     4 |  370 | `    uid = (uid_t)atoi(zUser);` |
|     2 |  371 | `  }else{` |
|     2 |  372 | `    pwd = getpwnam(zUser);   /* Try getting UID for username */` |
|     2 |  373 | `    if (pwd == 0) {` |
|     - |  374 | `      /* -2 = the NAME could not be resolved (no syscall ran, so errno means nothing).` |
|     - |  375 | `       * php words that case differently: "chown(): Unable to find uid for bogus". */` |
|     2 |  376 | `      return -2;` |
|     - |  377 | `    }` |
|   ! 0 |  378 | `    uid = pwd->pw_uid;` |
|     - |  379 | `  }` |
|     4 |  380 | `  rc = chown(zPath,uid,-1);` |
|     4 |  381 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |  382 | `#else` |
|     - |  383 | `	SXUNUSED(zPath);` |
|     - |  384 | `	SXUNUSED(zUser);` |
|     - |  385 | `	return -1;` |
|     - |  386 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     3 |  387 | `}` |
|     - |  388 | `/* int (*xChgrp)(const char *,const char *) */` |
|     6 |  389 | `static int UnixVfs_Chgrp(const char *zPath,const char *zGroup)` |
|     - |  390 | `{` |
|     6 |  391 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  392 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  393 | `  struct group *group;` |
|     - |  394 | `  gid_t gid;` |
|     - |  395 | `  int rc;` |
|     - |  396 | `  /* Numeric gid accepted too (see UnixVfs_Chown) */` |
|     6 |  397 | `  if( zGroup[0] >= '0' && zGroup[0] <= '9' ){` |
|     4 |  398 | `    gid = (gid_t)atoi(zGroup);` |
|     2 |  399 | `  }else{` |
|     2 |  400 | `    group = getgrnam(zGroup);` |
|     2 |  401 | `    if (group == 0) {` |
|     2 |  402 | `      return -2;   /* name lookup failed -- see UnixVfs_Chown */` |
|     - |  403 | `    }` |
|   ! 0 |  404 | `    gid = group->gr_gid;` |
|     - |  405 | `  }` |
|     4 |  406 | `  rc = chown(zPath,-1,gid);` |
|     4 |  407 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |  408 | `#else` |
|     - |  409 | `	SXUNUSED(zPath);` |
|     - |  410 | `	SXUNUSED(zGroup);` |
|     - |  411 | `	return -1;` |
|     - |  412 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     3 |  413 | `}` |
|     - |  414 | `/* int (*xIsfile)(const char *) */` |
|  7780 |  415 | `static int UnixVfs_isfile(const char *zPath)` |
|     - |  416 | `{` |
|  7780 |  417 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  418 | `	struct stat st;` |
|     - |  419 | `	int rc;` |
|  7780 |  420 | `	rc = stat(zPath,&st);` |
|  7780 |  421 | `	if( rc != 0 ){` |
|    76 |  422 | `	 return -1;` |
|     - |  423 | `	}` |
|  7704 |  424 | `	rc = S_ISREG(st.st_mode);` |
|  7704 |  425 | `	return rc ? PH7_OK : -1 ;` |
|  3890 |  426 | `}` |
|     - |  427 | `/* int (*xIslink)(const char *) */` |
|    18 |  428 | `static int UnixVfs_islink(const char *zPath)` |
|     - |  429 | `{` |
|    18 |  430 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  431 | `	struct stat st;` |
|     - |  432 | `	int rc;` |
|     - |  433 | `	/* LSTAT, not stat: stat() FOLLOWS the link and answers about its target, so` |
|     - |  434 | `	 * S_ISLNK could never be true here and is_link() was false for every symlink` |
|     - |  435 | `	 * in existence. php's is_link() is php_stat(FS_IS_LINK), which lstats. */` |
|    18 |  436 | `	rc = lstat(zPath,&st);` |
|    18 |  437 | `	if( rc != 0 ){` |
|     4 |  438 | `	 return -1;` |
|     - |  439 | `	}` |
|    14 |  440 | `	rc = S_ISLNK(st.st_mode);` |
|    14 |  441 | `	return rc ? PH7_OK : -1 ;` |
|     9 |  442 | `}` |
|     - |  443 | `/* int (*xReadable)(const char *) */` |
|     6 |  444 | `static int UnixVfs_isreadable(const char *zPath)` |
|     - |  445 | `{` |
|     6 |  446 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  447 | `	int rc;` |
|     6 |  448 | `	rc = access(zPath,R_OK);` |
|     6 |  449 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  450 | `}` |
|     - |  451 | `/* int (*xWritable)(const char *) */` |
|     8 |  452 | `static int UnixVfs_iswritable(const char *zPath)` |
|     - |  453 | `{` |
|     8 |  454 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  455 | `	int rc;` |
|     8 |  456 | `	rc = access(zPath,W_OK);` |
|     8 |  457 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  458 | `}` |
|     - |  459 | `/* int (*xExecutable)(const char *) */` |
|     4 |  460 | `static int UnixVfs_isexecutable(const char *zPath)` |
|     - |  461 | `{` |
|     4 |  462 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  463 | `	int rc;` |
|     4 |  464 | `	rc = access(zPath,X_OK);` |
|     4 |  465 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  466 | `}` |
|     - |  467 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|    24 |  468 | `static int UnixVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|     - |  469 | `{` |
|    24 |  470 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  471 | `	struct stat st;` |
|     - |  472 | `	int rc;` |
|     - |  473 | `	/* php's FS_TYPE lstats, which is why filetype() answers "link" for a symlink` |
|     - |  474 | `	 * and "file" for what it points at. stat() here made the "link" arm below` |
|     - |  475 | `	 * unreachable. */` |
|    24 |  476 | `    rc = lstat(zPath,&st);` |
|    24 |  477 | `	if( rc != 0 ){` |
|     - |  478 | `	  /* Expand 'unknown' */` |
|     4 |  479 | `	  ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|     4 |  480 | `	  return -1;` |
|     - |  481 | `	}` |
|    20 |  482 | `	if(S_ISREG(st.st_mode) ){` |
|     8 |  483 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|    16 |  484 | `	}else if(S_ISDIR(st.st_mode)){` |
|     8 |  485 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|     8 |  486 | `	}else if(S_ISLNK(st.st_mode)){` |
|     4 |  487 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|     2 |  488 | `	}else if(S_ISBLK(st.st_mode)){` |
|   ! 0 |  489 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|   ! 0 |  490 | `    }else if(S_ISSOCK(st.st_mode)){` |
|   ! 0 |  491 | `		ph7_result_string(pCtx,"socket",sizeof("socket")-1);` |
|   ! 0 |  492 | `	}else if(S_ISFIFO(st.st_mode)){` |
|   ! 0 |  493 | `       ph7_result_string(pCtx,"fifo",sizeof("fifo")-1);` |
|   ! 0 |  494 | `	}else{` |
|   ! 0 |  495 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|     - |  496 | `	}` |
|    20 |  497 | `	return PH7_OK;` |
|    12 |  498 | `}` |
|     - |  499 | `/* int (*xReadlink)(const char *,ph7_context *) */` |
|    10 |  500 | `static int UnixVfs_Readlink(const char *zPath,ph7_context *pCtx)` |
|     - |  501 | `{` |
|     - |  502 | `	char zBuf[4096];` |
|     - |  503 | `	ssize_t n;` |
|    10 |  504 | `	zPath = UnixVfsLocalPath(zPath);` |
|    10 |  505 | `	n = readlink(zPath,zBuf,sizeof(zBuf));` |
|    10 |  506 | `	if( n < 0 ){` |
|     6 |  507 | `		return -1;` |
|     - |  508 | `	}` |
|     - |  509 | `	/* readlink() does NOT null-terminate, and truncates silently when the target` |
|     - |  510 | `	 * does not fit -- the length is the only thing that says what was read. */` |
|     4 |  511 | `	if( (size_t)n >= sizeof(zBuf) ){` |
|   ! 0 |  512 | `		n = (ssize_t)sizeof(zBuf) - 1;` |
|   ! 0 |  513 | `	}` |
|     4 |  514 | `	ph7_result_string(pCtx,zBuf,(int)n);` |
|     4 |  515 | `	return PH7_OK;` |
|     5 |  516 | `}` |
|     - |  517 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|    62 |  518 | `static int UnixVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|     - |  519 | `{` |
|     - |  520 | `	char *zEnv;` |
|    62 |  521 | `	zEnv = getenv(zVar);` |
|    62 |  522 | `	if( zEnv == 0 ){` |
|     2 |  523 | `	  return -1;` |
|     - |  524 | `	}` |
|    60 |  525 | `	ph7_result_string(pCtx,zEnv,-1/*Compute length automatically*/);` |
|    60 |  526 | `	return PH7_OK;` |
|    31 |  527 | `}` |
|     - |  528 | `/* int (*xSetenv)(const char *,const char *) */` |
|     4 |  529 | `static int UnixVfs_Setenv(const char *zName,const char *zValue)` |
|     - |  530 | `{` |
|     - |  531 | `   int rc;` |
|     4 |  532 | `   rc = setenv(zName,zValue,1);` |
|     4 |  533 | `   return rc == 0 ? PH7_OK : -1;` |
|     - |  534 | `}` |
|     - |  535 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|  4634 |  536 | `static int UnixVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|     - |  537 | `{` |
|  4634 |  538 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  539 | `	struct stat st;` |
|     - |  540 | `	void *pMap;` |
|     - |  541 | `	int fd;` |
|     - |  542 | `	int rc;` |
|     - |  543 | `	/* Open the file in a read-only mode */` |
|  4634 |  544 | `	fd = open(zPath,O_RDONLY);` |
|  4634 |  545 | `	if( fd < 0 ){` |
|   ! 0 |  546 | `		return -1;` |
|     - |  547 | `	}` |
|     - |  548 | `	/* stat the handle */` |
|  4634 |  549 | `	fstat(fd,&st);` |
|     - |  550 | `	/* Obtain a memory view of the whole file */` |
|  4634 |  551 | `	pMap = mmap(0,st.st_size,PROT_READ,MAP_PRIVATE\|MAP_FILE,fd,0);` |
|  4634 |  552 | `	rc = PH7_OK;` |
|  4634 |  553 | `	if( pMap == MAP_FAILED ){` |
|   ! 0 |  554 | `		rc = -1;` |
|   ! 0 |  555 | `	}else{` |
|     - |  556 | `		/* Point to the memory view */` |
|  4634 |  557 | `		*ppMap = pMap;` |
|  4634 |  558 | `		*pSize = (ph7_int64)st.st_size;` |
|     - |  559 | `	}` |
|  4634 |  560 | `	close(fd);` |
|  4634 |  561 | `	return rc;` |
|  2317 |  562 | `}` |
|     - |  563 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|  4634 |  564 | `static void UnixVfs_Unmap(void *pView,ph7_int64 nSize)` |
|     - |  565 | `{` |
|  4634 |  566 | `	munmap(pView,(size_t)nSize);` |
|  4634 |  567 | `}` |
|     - |  568 | `/* void (*xTempDir)(ph7_context *) */` |
|   266 |  569 | `static void UnixVfs_TempDir(ph7_context *pCtx)` |
|     - |  570 | `{` |
|     - |  571 | `  const char *zDir;` |
|     - |  572 | `  /* php's php_get_temporary_directory: honour TMPDIR, else fall back to P_tmpdir` |
|     - |  573 | `   * ("/tmp" on Unix). PH7 also scanned /var/tmp, /usr/tmp, /usr/local/tmp first,` |
|     - |  574 | `   * which returned /var/tmp on a typical box where php returns /tmp — a divergence` |
|     - |  575 | `   * observable through sys_get_temp_dir()/tempnam()/session paths. */` |
|   266 |  576 | `  zDir = getenv("TMPDIR");` |
|   266 |  577 | `  if( zDir && zDir[0] != 0 && !access(zDir,07) ){` |
|     - |  578 | `	  /* php reports the temp dir WITHOUT a trailing separator; macOS's TMPDIR ends with` |
|     - |  579 | `	   * one, so returning it verbatim produced paths like "/var/.../T//file". */` |
|   133 |  580 | `	  int nDir = (int)strlen(zDir);` |
|   266 |  581 | `	  while( nDir > 1 && zDir[nDir-1] == '/' ){` |
|   133 |  582 | `		  nDir--;` |
|     - |  583 | `	  }` |
|   133 |  584 | `	  ph7_result_string(pCtx,zDir,nDir);` |
|   133 |  585 | `	  return;` |
|     - |  586 | `  }` |
|   133 |  587 | `  ph7_result_string(pCtx,"/tmp",(int)sizeof("/tmp")-1);` |
|   133 |  588 | `}` |
|     - |  589 | `/* unsigned int (*xProcessId)(void) */` |
|    98 |  590 | `static unsigned int UnixVfs_ProcessId(void)` |
|     - |  591 | `{` |
|    98 |  592 | `	return (unsigned int)getpid();` |
|     - |  593 | `}` |
|     - |  594 | `/* int (*xUid)(void) */` |
|     4 |  595 | `static int UnixVfs_uid(void)` |
|     - |  596 | `{` |
|     4 |  597 | `	return (int)getuid();` |
|     - |  598 | `}` |
|     - |  599 | `/* int (*xGid)(void) */` |
|     2 |  600 | `static int UnixVfs_gid(void)` |
|     - |  601 | `{` |
|     2 |  602 | `	return (int)getgid();` |
|     - |  603 | `}` |
|     - |  604 | `/* int (*xUmask)(int) */` |
|     8 |  605 | `static int UnixVfs_Umask(int new_mask)` |
|     - |  606 | `{` |
|     - |  607 | `	int old_mask;` |
|     8 |  608 | `	old_mask = umask(new_mask);` |
|     8 |  609 | `	return old_mask;` |
|     - |  610 | `}` |
|     - |  611 | `/* void (*xUsername)(ph7_context *) */` |
|     2 |  612 | `static void UnixVfs_Username(ph7_context *pCtx)` |
|     - |  613 | `{` |
|     - |  614 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  615 | `  struct passwd *pwd;` |
|     - |  616 | `  uid_t uid;` |
|     2 |  617 | `  uid = getuid();` |
|     2 |  618 | `  pwd = getpwuid(uid);   /* Try getting UID for username */` |
|     2 |  619 | `  if (pwd == 0) {` |
|   ! 0 |  620 | `    return;` |
|     - |  621 | `  }` |
|     - |  622 | `  /* Return the username */` |
|     2 |  623 | `  ph7_result_string(pCtx,pwd->pw_name,-1);` |
|     - |  624 | `#else` |
|     - |  625 | `  ph7_result_string(pCtx,"Unknown",-1);` |
|     - |  626 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     2 |  627 | `  return;` |
|     1 |  628 | `}` |
|     - |  629 | `/* int (*xLink)(const char *,const char *,int) */` |
|    12 |  630 | `static int UnixVfs_link(const char *zSrc,const char *zTarget,int is_sym)` |
|     - |  631 | `{` |
|    12 |  632 | `	zSrc = UnixVfsLocalPath(zSrc);` |
|    12 |  633 | `	zTarget = UnixVfsLocalPath(zTarget);` |
|     - |  634 | `	int rc;` |
|    12 |  635 | `	if( is_sym ){` |
|     - |  636 | `		/* Symbolic link */` |
|    10 |  637 | `		rc = symlink(zSrc,zTarget);` |
|     5 |  638 | `	}else{` |
|     - |  639 | `		/* Hard link */` |
|     2 |  640 | `		rc = link(zSrc,zTarget);` |
|     - |  641 | `	}` |
|    12 |  642 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  643 | `}` |
|     - |  644 | `/* int (*xChroot)(const char *) */` |
|   ! 0 |  645 | `static int UnixVfs_chroot(const char *zRootDir)` |
|     - |  646 | `{` |
|     - |  647 | `	int rc;` |
|   ! 0 |  648 | `	rc = chroot(zRootDir);` |
|   ! 0 |  649 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  650 | `}` |
|     - |  651 | `/* Export the UNIX vfs */` |
|     - |  652 | `PH7_PRIVATE const ph7_vfs sUnixVfs = {` |
|     - |  653 | `	"Unix_vfs",` |
|     - |  654 | `	PH7_VFS_VERSION,` |
|     - |  655 | `	UnixVfs_chdir,    /* int (*xChdir)(const char *) */` |
|     - |  656 | `	UnixVfs_chroot,   /* int (*xChroot)(const char *); */` |
|     - |  657 | `	UnixVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|     - |  658 | `	UnixVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|     - |  659 | `	UnixVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|     - |  660 | `	UnixVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|     - |  661 | `	UnixVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|     - |  662 | `	UnixVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|     - |  663 | `	UnixVfs_Sleep,    /* int (*xSleep)(unsigned int) */` |
|     - |  664 | `	UnixVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|     - |  665 | `	UnixVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|     - |  666 | `	UnixVfs_Chmod, /*int (*xChmod)(const char *,int)*/` |
|     - |  667 | `	UnixVfs_Chown, /*int (*xChown)(const char *,const char *)*/` |
|     - |  668 | `	UnixVfs_Chgrp, /*int (*xChgrp)(const char *,const char *)*/` |
|     - |  669 | `	UnixVfs_FreeSpace,  /* ph7_int64 (*xFreeSpace)(const char *) */` |
|     - |  670 | `	UnixVfs_TotalSpace, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|     - |  671 | `	UnixVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|     - |  672 | `	UnixVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|     - |  673 | `	UnixVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|     - |  674 | `	UnixVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|     - |  675 | `	UnixVfs_Stat,  /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|     - |  676 | `	UnixVfs_lStat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|     - |  677 | `	UnixVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|     - |  678 | `	UnixVfs_islink,     /* int (*xIslink)(const char *) */` |
|     - |  679 | `	UnixVfs_isreadable, /* int (*xReadable)(const char *) */` |
|     - |  680 | `	UnixVfs_iswritable, /* int (*xWritable)(const char *) */` |
|     - |  681 | `	UnixVfs_isexecutable,/* int (*xExecutable)(const char *) */` |
|     - |  682 | `	UnixVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|     - |  683 | `	UnixVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|     - |  684 | `	UnixVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|     - |  685 | `	UnixVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|     - |  686 | `	UnixVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|     - |  687 | `	UnixVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|     - |  688 | `	UnixVfs_link,       /* int (*xLink)(const char *,const char *,int) */` |
|     - |  689 | `	UnixVfs_Umask,      /* int (*xUmask)(int) */` |
|     - |  690 | `	UnixVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|     - |  691 | `	UnixVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|     - |  692 | `	UnixVfs_uid, /* int (*xUid)(void) */` |
|     - |  693 | `	UnixVfs_gid, /* int (*xGid)(void) */` |
|     - |  694 | `	UnixVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|     - |  695 | `	0, /* int (*xExec)(const char *,ph7_context *) */` |
|     - |  696 | `	UnixVfs_Readlink /* int (*xReadlink)(const char *,ph7_context *) */` |
|     - |  697 | `};` |
|     - |  698 | `/* UNIX File IO */` |
|     - |  699 | `#define PH7_UNIX_OPEN_MODE	0640 /* Default open mode */` |
|     - |  700 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
| 32906 |  701 | `static int UnixFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|     - |  702 | `{` |
| 32906 |  703 | `	int iOpen = O_RDONLY;` |
|     - |  704 | `	int fd;` |
|     - |  705 | `	/* Set the desired flags according to the open mode */` |
| 32906 |  706 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|     - |  707 | `		/* Open existing file, or create if it doesn't exist */` |
| 14906 |  708 | `		iOpen = O_CREAT;` |
| 14906 |  709 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|     - |  710 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
| 14900 |  711 | `			iOpen \|= O_TRUNC;` |
|  7450 |  712 | `			SXUNUSED(pResource); /* cc warning */` |
|  7450 |  713 | `		}` |
| 25453 |  714 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|     - |  715 | `		/* Creates a new file, only if it does not already exist.` |
|     - |  716 | `		* If the file exists, it fails.` |
|     - |  717 | `		*/` |
|   154 |  718 | `		iOpen = O_CREAT\|O_EXCL;` |
| 17923 |  719 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|     - |  720 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|     - |  721 | `		 * The file must exist.` |
|     - |  722 | `		 */` |
|   ! 0 |  723 | `		iOpen = O_RDWR\|O_TRUNC;` |
|   ! 0 |  724 | `	}` |
| 32906 |  725 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|     - |  726 | `		/* Read+Write access */` |
| 14878 |  727 | `		iOpen &= ~O_RDONLY;` |
| 14878 |  728 | `		iOpen \|= O_RDWR;` |
| 25467 |  729 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|     - |  730 | `		/* Write only access */` |
|   184 |  731 | `		iOpen &= ~O_RDONLY;` |
|   184 |  732 | `		iOpen \|= O_WRONLY;` |
|    92 |  733 | `	}` |
| 32906 |  734 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|     - |  735 | `		/* Append mode */` |
|     6 |  736 | `		iOpen \|= O_APPEND;` |
|     3 |  737 | `	}` |
|     - |  738 | `#ifdef O_TEMP` |
|     - |  739 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|     - |  740 | `		/* File is temporary */` |
|     - |  741 | `		iOpen \|= O_TEMP;` |
|     - |  742 | `	}` |
|     - |  743 | `#endif` |
|     - |  744 | `	/* Open the file now */` |
| 32906 |  745 | `	fd = open(zPath,iOpen,PH7_UNIX_OPEN_MODE);` |
| 32906 |  746 | `	if( fd < 0 ){` |
|     - |  747 | `		/* IO error */` |
|    22 |  748 | `		return -1;` |
|     - |  749 | `	}` |
|     - |  750 | `	/* Save the handle */` |
| 32884 |  751 | `	*ppHandle = SX_INT_TO_PTR(fd);` |
| 32884 |  752 | `	return PH7_OK;` |
| 16453 |  753 | `}` |
|     - |  754 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|  1274 |  755 | `static int UnixDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|     - |  756 | `{` |
|     - |  757 | `	DIR *pDir;` |
|     - |  758 | `	/* Open the target directory */` |
|  1274 |  759 | `	pDir = opendir(zPath);` |
|  1274 |  760 | `	if( pDir == 0 ){` |
|     6 |  761 | `		SXUNUSED(pResource); /* Compiler warning */` |
|    12 |  762 | `		return -1;` |
|     - |  763 | `	}` |
|     - |  764 | `	/* Save our structure */` |
|  1262 |  765 | `	*ppHandle = pDir;` |
|  1262 |  766 | `	return PH7_OK;` |
|   637 |  767 | `}` |
|     - |  768 | `/* void (*xCloseDir)(void *) */` |
|  1242 |  769 | `static void UnixDir_Close(void *pUserData)` |
|     - |  770 | `{` |
|  1242 |  771 | `	closedir((DIR *)pUserData);` |
|  1242 |  772 | `}` |
|     - |  773 | `/* void (*xClose)(void *); */` |
| 32910 |  774 | `static void UnixFile_Close(void *pUserData)` |
|     - |  775 | `{` |
| 32910 |  776 | `	close(SX_PTR_TO_INT(pUserData));` |
| 32910 |  777 | `}` |
|     - |  778 | `/* int (*xReadDir)(void *,ph7_context *) */` |
| 12954 |  779 | `static int UnixDir_Read(void *pUserData,ph7_context *pCtx)` |
|     - |  780 | `{` |
| 12954 |  781 | `	DIR *pDir = (DIR *)pUserData;` |
|     - |  782 | `	struct dirent *pEntry;` |
|     - |  783 | `	char *zName;` |
|     - |  784 | `	sxu32 n;` |
|     - |  785 | `	/* php's readdir() yields every entry including '.' and '..' */` |
| 12954 |  786 | `	pEntry = readdir(pDir);` |
| 12954 |  787 | `	if( pEntry == 0 ){` |
|     - |  788 | `		/* No more entries to process */` |
|  1230 |  789 | `		return -1;` |
|     - |  790 | `	}` |
| 11724 |  791 | `	zName = pEntry->d_name;` |
| 11724 |  792 | `	n = SyStrlen(zName);` |
|     - |  793 | `	/* Return the current file name */` |
| 11724 |  794 | `	ph7_result_string(pCtx,zName,(int)n);` |
| 11724 |  795 | `	return PH7_OK;` |
|  6478 |  796 | `}` |
|     - |  797 | `/* void (*xRewindDir)(void *) */` |
|    30 |  798 | `static void UnixDir_Rewind(void *pUserData)` |
|     - |  799 | `{` |
|    30 |  800 | `	rewinddir((DIR *)pUserData);` |
|    30 |  801 | `}` |
|     - |  802 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
| 35686 |  803 | `static ph7_int64 UnixFile_Read(void *pUserData,void *pBuffer,ph7_int64 nDatatoRead)` |
|     - |  804 | `{` |
|     - |  805 | `	ssize_t nRd;` |
| 35686 |  806 | `	nRd = read(SX_PTR_TO_INT(pUserData),pBuffer,(size_t)nDatatoRead);` |
| 35686 |  807 | `	if( nRd < 0 ){` |
|     - |  808 | `		/* An IO error. EOF is read()'s ZERO and rides through as one: it is not a` |
|     - |  809 | `		 * failure, and the readers above need to tell the two apart — fread() at` |
|     - |  810 | `		 * EOF is php's "" and only a real error is its false. Every consumer of` |
|     - |  811 | ``		 * this driver stops on `< 1`, so both still end a read loop. The Windows`` |
|     - |  812 | `		 * driver already answers this way (ReadFile succeeds with 0 bytes). */` |
|   ! 0 |  813 | `		return -1;` |
|     - |  814 | `	}` |
| 35686 |  815 | `	return (ph7_int64)nRd;` |
| 17843 |  816 | `}` |
|     - |  817 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
| 14916 |  818 | `static ph7_int64 UnixFile_Write(void *pUserData,const void *pBuffer,ph7_int64 nWrite)` |
|     - |  819 | `{` |
| 14916 |  820 | `	const char *zData = (const char *)pBuffer;` |
| 14916 |  821 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|     - |  822 | `	ph7_int64 nCount;` |
|     - |  823 | `	ssize_t nWr;` |
| 14916 |  824 | `	nCount = 0;` |
| 14916 |  825 | `	for(;;){` |
| 29832 |  826 | `		if( nWrite < 1 ){` |
| 14916 |  827 | `			break;` |
|     - |  828 | `		}` |
| 14916 |  829 | `		nWr = write(fd,zData,(size_t)nWrite);` |
| 14916 |  830 | `		if( nWr < 1 ){` |
|     - |  831 | `			/* IO error */` |
|   ! 0 |  832 | `			break;` |
|     - |  833 | `		}` |
| 14916 |  834 | `		nWrite -= nWr;` |
| 14916 |  835 | `		nCount += nWr;` |
| 14916 |  836 | `		zData += nWr;` |
|     - |  837 | `	}` |
| 14916 |  838 | `	if( nWrite > 0 ){` |
|   ! 0 |  839 | `		return -1;` |
|     - |  840 | `	}` |
| 14916 |  841 | `	return nCount;` |
|  7458 |  842 | `}` |
|     - |  843 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|    12 |  844 | `static int UnixFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|     - |  845 | `{` |
|     - |  846 | `	off_t iNew;` |
|    12 |  847 | `	switch(whence){` |
|   ! 0 |  848 | `	case 1:/*SEEK_CUR*/` |
|   ! 0 |  849 | `		whence = SEEK_CUR;` |
|   ! 0 |  850 | `		break;` |
|   ! 0 |  851 | `	case 2: /* SEEK_END */` |
|   ! 0 |  852 | `		whence = SEEK_END;` |
|   ! 0 |  853 | `		break;` |
|    12 |  854 | `	case 0: /* SEEK_SET */` |
|     - |  855 | `	default:` |
|    12 |  856 | `		whence = SEEK_SET;` |
|    12 |  857 | `		break;` |
|     - |  858 | `	}` |
|    12 |  859 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),(off_t)iOfft,whence);` |
|    12 |  860 | `	if( iNew < 0 ){` |
|   ! 0 |  861 | `		return -1;` |
|     - |  862 | `	}` |
|    12 |  863 | `	return PH7_OK;` |
|     6 |  864 | `}` |
|     - |  865 | `/* int (*xLock)(void *,int) — see the lock_type value space in ph7.h */` |
|    24 |  866 | `static int UnixFile_Lock(void *pUserData,int lock_type)` |
|     - |  867 | `{` |
|    24 |  868 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|     - |  869 | `	int op;` |
|     - |  870 | `	int rc;` |
|    24 |  871 | `	if( lock_type < 0 ){` |
|     - |  872 | `		/* Unlock the file */` |
|     8 |  873 | `		op = LOCK_UN;` |
|     4 |  874 | `	}else{` |
|    16 |  875 | `		op = (lock_type == PH7_IO_LOCK_EX \|\| lock_type == PH7_IO_LOCK_EX_NB) ? LOCK_EX : LOCK_SH;` |
|    16 |  876 | `		if( lock_type == PH7_IO_LOCK_SH_NB \|\| lock_type == PH7_IO_LOCK_EX_NB ){` |
|    12 |  877 | `			op \|= LOCK_NB;` |
|     6 |  878 | `		}` |
|     - |  879 | `	}` |
|    24 |  880 | `	rc = flock(fd,op);` |
|    24 |  881 | `	if( rc == 0 ){` |
|    20 |  882 | `		return PH7_OK;` |
|     - |  883 | `	}` |
|     - |  884 | `	/* A non-blocking request that another holder refused is php's $would_block,` |
|     - |  885 | `	 * not an IO failure. (EWOULDBLOCK and EAGAIN are the same value on every` |
|     - |  886 | `	 * platform this driver builds for.) */` |
|     4 |  887 | `	if( errno == EWOULDBLOCK ){` |
|     4 |  888 | `		return SXERR_BUSY;` |
|     - |  889 | `	}` |
|   ! 0 |  890 | `	return -1;` |
|    12 |  891 | `}` |
|     - |  892 | `/* ph7_int64 (*xTell)(void *) */` |
|     6 |  893 | `static ph7_int64 UnixFile_Tell(void *pUserData)` |
|     - |  894 | `{` |
|     - |  895 | `	off_t iNew;` |
|     6 |  896 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),0,SEEK_CUR);` |
|     6 |  897 | `	return (ph7_int64)iNew;` |
|     - |  898 | `}` |
|     - |  899 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|     2 |  900 | `static int UnixFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|     - |  901 | `{` |
|     - |  902 | `	int rc;` |
|     2 |  903 | `	rc = ftruncate(SX_PTR_TO_INT(pUserData),(off_t)nOfft);` |
|     2 |  904 | `	if( rc != 0 ){` |
|   ! 0 |  905 | `		return -1;` |
|     - |  906 | `	}` |
|     2 |  907 | `	return PH7_OK;` |
|     1 |  908 | `}` |
|     - |  909 | `/* int (*xSync)(void *); */` |
|     2 |  910 | `static int UnixFile_Sync(void *pUserData)` |
|     - |  911 | `{` |
|     - |  912 | `	int rc;` |
|     2 |  913 | `	rc = fsync(SX_PTR_TO_INT(pUserData));` |
|     2 |  914 | `	return rc == 0 ? PH7_OK : - 1;` |
|     - |  915 | `}` |
|     - |  916 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|     4 |  917 | `static int UnixFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  918 | `{` |
|     - |  919 | `	struct stat st;` |
|     - |  920 | `	int rc;` |
|     4 |  921 | `	rc = fstat(SX_PTR_TO_INT(pUserData),&st);` |
|     4 |  922 | `	if( rc != 0 ){` |
|   ! 0 |  923 | `	 return -1;` |
|     - |  924 | `	}` |
|     - |  925 | `	/* dev */` |
|     4 |  926 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|     4 |  927 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  928 | `	/* ino */` |
|     4 |  929 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|     4 |  930 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  931 | `	/* mode */` |
|     4 |  932 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|     4 |  933 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  934 | `	/* nlink */` |
|     4 |  935 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|     4 |  936 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  937 | `	/* uid,gid,rdev */` |
|     4 |  938 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|     4 |  939 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|     4 |  940 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|     4 |  941 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|     4 |  942 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|     4 |  943 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  944 | `	/* size */` |
|     4 |  945 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|     4 |  946 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  947 | `	/* atime */` |
|     4 |  948 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|     4 |  949 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  950 | `	/* mtime */` |
|     4 |  951 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|     4 |  952 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  953 | `	/* ctime */` |
|     4 |  954 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|     4 |  955 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  956 | `	/* blksize,blocks */` |
|     4 |  957 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|     4 |  958 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|     4 |  959 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|     4 |  960 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|     4 |  961 | `	return PH7_OK;` |
|     2 |  962 | `}` |
|     - |  963 | `/* Export the file:// stream */` |
|     - |  964 | `PH7_PRIVATE const ph7_io_stream sUnixFileStream = {` |
|     - |  965 | `	"file", /* Stream name */` |
|     - |  966 | `	PH7_IO_STREAM_VERSION,` |
|     - |  967 | `	UnixFile_Open,  /* xOpen */` |
|     - |  968 | `	UnixDir_Open,   /* xOpenDir */` |
|     - |  969 | `	UnixFile_Close, /* xClose */` |
|     - |  970 | `	UnixDir_Close,  /* xCloseDir */` |
|     - |  971 | `	UnixFile_Read,  /* xRead */` |
|     - |  972 | `	UnixDir_Read,   /* xReadDir */` |
|     - |  973 | `	UnixFile_Write, /* xWrite */` |
|     - |  974 | `	UnixFile_Seek,  /* xSeek */` |
|     - |  975 | `	UnixFile_Lock,  /* xLock */` |
|     - |  976 | `	UnixDir_Rewind, /* xRewindDir */` |
|     - |  977 | `	UnixFile_Tell,  /* xTell */` |
|     - |  978 | `	UnixFile_Trunc, /* xTrunc */` |
|     - |  979 | `	UnixFile_Sync,  /* xSeek */` |
|     - |  980 | `	UnixFile_Stat   /* xStat */` |
|     - |  981 | `};` |
|     - |  982 | `#endif /* __UNIXES__ */` |
|     - |  983 |  |
