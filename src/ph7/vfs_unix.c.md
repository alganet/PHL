# src/ph7/vfs_unix.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 432/483 lines (89.44%)

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
| 57838 |   40 | `static const char * UnixVfsLocalPath(const char *zPath)` |
|     - |   41 | `{` |
|     - |   42 | `	const char *zRest;` |
| 57838 |   43 | `	if( zPath == 0 \|\| SyStrnicmp(zPath,"file://",sizeof("file://")-1) != 0 ){` |
| 57828 |   44 | `		return zPath;` |
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
| 28919 |   56 | `}` |
|     - |   57 | `/* int (*xchdir)(const char *) */` |
| 13790 |   58 | `static int UnixVfs_chdir(const char *zPath)` |
|     - |   59 | `{` |
|     - |   60 | `  int rc;` |
| 13790 |   61 | `  rc = chdir(zPath);` |
| 13790 |   62 | `  return rc == 0 ? PH7_OK : -1;` |
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
|    52 |   78 | `static int UnixVfs_mkdir(const char *zPath,int mode,int recursive)` |
|     - |   79 | `{` |
|    52 |   80 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |   81 | `	int rc;` |
|    52 |   82 | `        rc = mkdir(zPath,mode);` |
|    26 |   83 | `	SXUNUSED(recursive); /* cc warning */` |
|    52 |   84 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   85 | `}` |
|     - |   86 | `/* int (*xRmdir)(const char *) */` |
|    52 |   87 | `static int UnixVfs_rmdir(const char *zPath)` |
|     - |   88 | `{` |
|    52 |   89 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |   90 | `	int rc;` |
|    52 |   91 | `	rc = rmdir(zPath);` |
|    52 |   92 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   93 | `}` |
|     - |   94 | `/* int (*xIsdir)(const char *) */` |
|  9420 |   95 | `static int UnixVfs_isdir(const char *zPath)` |
|     - |   96 | `{` |
|  9420 |   97 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |   98 | `	struct stat st;` |
|     - |   99 | `	int rc;` |
|  9420 |  100 | `	rc = stat(zPath,&st);` |
|  9420 |  101 | `	if( rc != 0 ){` |
|     4 |  102 | `	 return -1;` |
|     - |  103 | `	}` |
|  9416 |  104 | `	rc = S_ISDIR(st.st_mode);` |
|  9416 |  105 | `	return rc ? PH7_OK : -1 ;` |
|  4710 |  106 | `}` |
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
|     6 |  117 | `static int UnixVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|     - |  118 | `{` |
|     - |  119 | `#ifndef PH7_UNIX_OLD_LIBC` |
|     - |  120 | `	char *zReal;` |
|     6 |  121 | `	zReal = realpath(zPath,0);` |
|     6 |  122 | `	if( zReal == 0 ){` |
|     2 |  123 | `	  return -1;` |
|     - |  124 | `	}` |
|     4 |  125 | `	ph7_result_string(pCtx,zReal,-1/*Compute length automatically*/);` |
|     - |  126 | `        /* Release the allocated buffer */` |
|     4 |  127 | `	free(zReal);` |
|     4 |  128 | `	return PH7_OK;` |
|     - |  129 | `#else` |
|     - |  130 | `    zPath = 0; /* cc warning */` |
|     - |  131 | `    pCtx = 0;` |
|     - |  132 | `    return -1;` |
|     - |  133 | `#endif` |
|     3 |  134 | `}` |
|     - |  135 | `/* int (*xSleep)(unsigned int) */` |
|    64 |  136 | `static int UnixVfs_Sleep(unsigned int uSec)` |
|     - |  137 | `{` |
|    64 |  138 | `	usleep(uSec);` |
|    64 |  139 | `	return PH7_OK;` |
|     - |  140 | `}` |
|     - |  141 | `/* int (*xUnlink)(const char *) */` |
| 36130 |  142 | `static int UnixVfs_unlink(const char *zPath)` |
|     - |  143 | `{` |
| 36130 |  144 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  145 | `	int rc;` |
| 36130 |  146 | `	rc = unlink(zPath);` |
| 36130 |  147 | `	return rc == 0 ? PH7_OK : -1 ;` |
|     - |  148 | `}` |
|     - |  149 | `/* int (*xFileExists)(const char *) */` |
|   206 |  150 | `static int UnixVfs_FileExists(const char *zPath)` |
|     - |  151 | `{` |
|   206 |  152 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  153 | `	int rc;` |
|   206 |  154 | `	rc = access(zPath,F_OK);` |
|   206 |  155 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  156 | `}` |
|     - |  157 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|     - |  158 | `/* ph7_int64 (*xFreeSpace)(const char *) */` |
|     4 |  159 | `static ph7_int64 UnixVfs_FreeSpace(const char *zPath)` |
|     - |  160 | `{` |
|     4 |  161 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  162 | `	struct statvfs sInfo;` |
|     4 |  163 | `	if( statvfs(zPath,&sInfo) != 0 ){` |
|   ! 0 |  164 | `		return -1;` |
|     - |  165 | `	}` |
|     - |  166 | `	/* php reports the space available to an UNPRIVILEGED user (f_bavail) */` |
|     4 |  167 | `	return (ph7_int64)sInfo.f_bavail * (ph7_int64)sInfo.f_frsize;` |
|     2 |  168 | `}` |
|     - |  169 | `/* ph7_int64 (*xTotalSpace)(const char *) */` |
|     4 |  170 | `static ph7_int64 UnixVfs_TotalSpace(const char *zPath)` |
|     - |  171 | `{` |
|     4 |  172 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  173 | `	struct statvfs sInfo;` |
|     4 |  174 | `	if( statvfs(zPath,&sInfo) != 0 ){` |
|   ! 0 |  175 | `		return -1;` |
|     - |  176 | `	}` |
|     4 |  177 | `	return (ph7_int64)sInfo.f_blocks * (ph7_int64)sInfo.f_frsize;` |
|     2 |  178 | `}` |
|    10 |  179 | `static ph7_int64 UnixVfs_FileSize(const char *zPath)` |
|     - |  180 | `{` |
|    10 |  181 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  182 | `	struct stat st;` |
|     - |  183 | `	int rc;` |
|    10 |  184 | `	rc = stat(zPath,&st);` |
|    10 |  185 | `	if( rc != 0 ){` |
|   ! 0 |  186 | `	 return -1;` |
|     - |  187 | `	}` |
|    10 |  188 | `	return (ph7_int64)st.st_size;` |
|     5 |  189 | `}` |
|     - |  190 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    12 |  191 | `static int UnixVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|     - |  192 | `{` |
|    12 |  193 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  194 | `	struct utimbuf ut;` |
|     - |  195 | `	int rc;` |
|     - |  196 | `	/* Both stamps are always real values here — the builtin resolves php's "now"` |
|     - |  197 | `	 * default, because a NEGATIVE timestamp is legal and so cannot be a sentinel. */` |
|    12 |  198 | `	ut.actime  = (time_t)access_time;` |
|    12 |  199 | `	ut.modtime = (time_t)touch_time;` |
|    12 |  200 | `	rc = utime(zPath,&ut);` |
|    12 |  201 | `	if( rc != 0 ){` |
|     - |  202 | `		/* php's touch() CREATES the file when it is missing — that is the idiom's` |
|     - |  203 | ``		 * main use, and utime() alone fails ENOENT there, so `touch($new)` answered`` |
|     - |  204 | `		 * false and created nothing. Try the create only after utime failed, like` |
|     - |  205 | `		 * php: no extra stat on the common path, and no access(2) real-uid check. */` |
|     8 |  206 | `		int fd = open(zPath,O_WRONLY\|O_CREAT,0666);` |
|     8 |  207 | `		if( fd < 0 ){` |
|   ! 0 |  208 | `			return -1;` |
|     - |  209 | `		}` |
|     8 |  210 | `		close(fd);` |
|     8 |  211 | `		rc = utime(zPath,&ut);` |
|     8 |  212 | `		if( rc != 0 ){` |
|   ! 0 |  213 | `			return -1;` |
|     - |  214 | `		}` |
|     4 |  215 | `	}` |
|    12 |  216 | `	return PH7_OK;` |
|     6 |  217 | `}` |
|     - |  218 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|     4 |  219 | `static ph7_int64 UnixVfs_FileAtime(const char *zPath)` |
|     - |  220 | `{` |
|     4 |  221 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  222 | `	struct stat st;` |
|     - |  223 | `	int rc;` |
|     4 |  224 | `	rc = stat(zPath,&st);` |
|     4 |  225 | `	if( rc != 0 ){` |
|   ! 0 |  226 | `	 return -1;` |
|     - |  227 | `	}` |
|     4 |  228 | `	return (ph7_int64)st.st_atime;` |
|     2 |  229 | `}` |
|     - |  230 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|     6 |  231 | `static ph7_int64 UnixVfs_FileMtime(const char *zPath)` |
|     - |  232 | `{` |
|     6 |  233 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  234 | `	struct stat st;` |
|     - |  235 | `	int rc;` |
|     6 |  236 | `	rc = stat(zPath,&st);` |
|     6 |  237 | `	if( rc != 0 ){` |
|   ! 0 |  238 | `	 return -1;` |
|     - |  239 | `	}` |
|     6 |  240 | `	return (ph7_int64)st.st_mtime;` |
|     3 |  241 | `}` |
|     - |  242 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|     2 |  243 | `static ph7_int64 UnixVfs_FileCtime(const char *zPath)` |
|     - |  244 | `{` |
|     2 |  245 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  246 | `	struct stat st;` |
|     - |  247 | `	int rc;` |
|     2 |  248 | `	rc = stat(zPath,&st);` |
|     2 |  249 | `	if( rc != 0 ){` |
|   ! 0 |  250 | `	 return -1;` |
|     - |  251 | `	}` |
|     2 |  252 | `	return (ph7_int64)st.st_ctime;` |
|     1 |  253 | `}` |
|     - |  254 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    10 |  255 | `static int UnixVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  256 | `{` |
|    10 |  257 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  258 | `	struct stat st;` |
|     - |  259 | `	int rc;` |
|    10 |  260 | `	rc = stat(zPath,&st);` |
|    10 |  261 | `	if( rc != 0 ){` |
|   ! 0 |  262 | `	 return -1;` |
|     - |  263 | `	}` |
|     - |  264 | `	/* dev */` |
|    10 |  265 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|    10 |  266 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  267 | `	/* ino */` |
|    10 |  268 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|    10 |  269 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  270 | `	/* mode */` |
|    10 |  271 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|    10 |  272 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  273 | `	/* nlink */` |
|    10 |  274 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|    10 |  275 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  276 | `	/* uid,gid,rdev */` |
|    10 |  277 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|    10 |  278 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    10 |  279 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|    10 |  280 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    10 |  281 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|    10 |  282 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  283 | `	/* size */` |
|    10 |  284 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|    10 |  285 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  286 | `	/* atime */` |
|    10 |  287 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|    10 |  288 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  289 | `	/* mtime */` |
|    10 |  290 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|    10 |  291 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  292 | `	/* ctime */` |
|    10 |  293 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|    10 |  294 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  295 | `	/* blksize,blocks */` |
|    10 |  296 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|    10 |  297 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    10 |  298 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|    10 |  299 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    10 |  300 | `	return PH7_OK;` |
|     5 |  301 | `}` |
|     - |  302 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|     2 |  303 | `static int UnixVfs_lStat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  304 | `{` |
|     2 |  305 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  306 | `	struct stat st;` |
|     - |  307 | `	int rc;` |
|     2 |  308 | `	rc = lstat(zPath,&st);` |
|     2 |  309 | `	if( rc != 0 ){` |
|   ! 0 |  310 | `	 return -1;` |
|     - |  311 | `	}` |
|     - |  312 | `	/* dev */` |
|     2 |  313 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|     2 |  314 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  315 | `	/* ino */` |
|     2 |  316 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|     2 |  317 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  318 | `	/* mode */` |
|     2 |  319 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|     2 |  320 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  321 | `	/* nlink */` |
|     2 |  322 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|     2 |  323 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  324 | `	/* uid,gid,rdev */` |
|     2 |  325 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|     2 |  326 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|     2 |  327 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|     2 |  328 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|     2 |  329 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|     2 |  330 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  331 | `	/* size */` |
|     2 |  332 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|     2 |  333 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  334 | `	/* atime */` |
|     2 |  335 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|     2 |  336 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  337 | `	/* mtime */` |
|     2 |  338 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|     2 |  339 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  340 | `	/* ctime */` |
|     2 |  341 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|     2 |  342 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  343 | `	/* blksize,blocks */` |
|     2 |  344 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|     2 |  345 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|     2 |  346 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|     2 |  347 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|     2 |  348 | `	return PH7_OK;` |
|     1 |  349 | `}` |
|     - |  350 | `/* int (*xChmod)(const char *,int) */` |
|   154 |  351 | `static int UnixVfs_Chmod(const char *zPath,int mode)` |
|     - |  352 | `{` |
|   154 |  353 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  354 | `    int rc;` |
|   154 |  355 | `    rc = chmod(zPath,(mode_t)mode);` |
|   154 |  356 | `    return rc == 0 ? PH7_OK : - 1;` |
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
|  7230 |  415 | `static int UnixVfs_isfile(const char *zPath)` |
|     - |  416 | `{` |
|  7230 |  417 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  418 | `	struct stat st;` |
|     - |  419 | `	int rc;` |
|  7230 |  420 | `	rc = stat(zPath,&st);` |
|  7230 |  421 | `	if( rc != 0 ){` |
|    34 |  422 | `	 return -1;` |
|     - |  423 | `	}` |
|  7196 |  424 | `	rc = S_ISREG(st.st_mode);` |
|  7196 |  425 | `	return rc ? PH7_OK : -1 ;` |
|  3615 |  426 | `}` |
|     - |  427 | `/* int (*xIslink)(const char *) */` |
|     4 |  428 | `static int UnixVfs_islink(const char *zPath)` |
|     - |  429 | `{` |
|     4 |  430 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  431 | `	struct stat st;` |
|     - |  432 | `	int rc;` |
|     4 |  433 | `	rc = stat(zPath,&st);` |
|     4 |  434 | `	if( rc != 0 ){` |
|   ! 0 |  435 | `	 return -1;` |
|     - |  436 | `	}` |
|     4 |  437 | `	rc = S_ISLNK(st.st_mode);` |
|     4 |  438 | `	return rc ? PH7_OK : -1 ;` |
|     2 |  439 | `}` |
|     - |  440 | `/* int (*xReadable)(const char *) */` |
|     2 |  441 | `static int UnixVfs_isreadable(const char *zPath)` |
|     - |  442 | `{` |
|     2 |  443 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  444 | `	int rc;` |
|     2 |  445 | `	rc = access(zPath,R_OK);` |
|     2 |  446 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  447 | `}` |
|     - |  448 | `/* int (*xWritable)(const char *) */` |
|     4 |  449 | `static int UnixVfs_iswritable(const char *zPath)` |
|     - |  450 | `{` |
|     4 |  451 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  452 | `	int rc;` |
|     4 |  453 | `	rc = access(zPath,W_OK);` |
|     4 |  454 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  455 | `}` |
|     - |  456 | `/* int (*xExecutable)(const char *) */` |
|     2 |  457 | `static int UnixVfs_isexecutable(const char *zPath)` |
|     - |  458 | `{` |
|     2 |  459 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  460 | `	int rc;` |
|     2 |  461 | `	rc = access(zPath,X_OK);` |
|     2 |  462 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  463 | `}` |
|     - |  464 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|     4 |  465 | `static int UnixVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|     - |  466 | `{` |
|     4 |  467 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  468 | `	struct stat st;` |
|     - |  469 | `	int rc;` |
|     4 |  470 | `    rc = stat(zPath,&st);` |
|     4 |  471 | `	if( rc != 0 ){` |
|     - |  472 | `	  /* Expand 'unknown' */` |
|   ! 0 |  473 | `	  ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|   ! 0 |  474 | `	  return -1;` |
|     - |  475 | `	}` |
|     4 |  476 | `	if(S_ISREG(st.st_mode) ){` |
|     2 |  477 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|     3 |  478 | `	}else if(S_ISDIR(st.st_mode)){` |
|     2 |  479 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|     1 |  480 | `	}else if(S_ISLNK(st.st_mode)){` |
|   ! 0 |  481 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|   ! 0 |  482 | `	}else if(S_ISBLK(st.st_mode)){` |
|   ! 0 |  483 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|   ! 0 |  484 | `    }else if(S_ISSOCK(st.st_mode)){` |
|   ! 0 |  485 | `		ph7_result_string(pCtx,"socket",sizeof("socket")-1);` |
|   ! 0 |  486 | `	}else if(S_ISFIFO(st.st_mode)){` |
|   ! 0 |  487 | `       ph7_result_string(pCtx,"fifo",sizeof("fifo")-1);` |
|   ! 0 |  488 | `	}else{` |
|   ! 0 |  489 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|     - |  490 | `	}` |
|     4 |  491 | `	return PH7_OK;` |
|     2 |  492 | `}` |
|     - |  493 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|    56 |  494 | `static int UnixVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|     - |  495 | `{` |
|     - |  496 | `	char *zEnv;` |
|    56 |  497 | `	zEnv = getenv(zVar);` |
|    56 |  498 | `	if( zEnv == 0 ){` |
|   ! 0 |  499 | `	  return -1;` |
|     - |  500 | `	}` |
|    56 |  501 | `	ph7_result_string(pCtx,zEnv,-1/*Compute length automatically*/);` |
|    56 |  502 | `	return PH7_OK;` |
|    28 |  503 | `}` |
|     - |  504 | `/* int (*xSetenv)(const char *,const char *) */` |
|     2 |  505 | `static int UnixVfs_Setenv(const char *zName,const char *zValue)` |
|     - |  506 | `{` |
|     - |  507 | `   int rc;` |
|     2 |  508 | `   rc = setenv(zName,zValue,1);` |
|     2 |  509 | `   return rc == 0 ? PH7_OK : -1;` |
|     - |  510 | `}` |
|     - |  511 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|  4492 |  512 | `static int UnixVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|     - |  513 | `{` |
|  4492 |  514 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  515 | `	struct stat st;` |
|     - |  516 | `	void *pMap;` |
|     - |  517 | `	int fd;` |
|     - |  518 | `	int rc;` |
|     - |  519 | `	/* Open the file in a read-only mode */` |
|  4492 |  520 | `	fd = open(zPath,O_RDONLY);` |
|  4492 |  521 | `	if( fd < 0 ){` |
|   ! 0 |  522 | `		return -1;` |
|     - |  523 | `	}` |
|     - |  524 | `	/* stat the handle */` |
|  4492 |  525 | `	fstat(fd,&st);` |
|     - |  526 | `	/* Obtain a memory view of the whole file */` |
|  4492 |  527 | `	pMap = mmap(0,st.st_size,PROT_READ,MAP_PRIVATE\|MAP_FILE,fd,0);` |
|  4492 |  528 | `	rc = PH7_OK;` |
|  4492 |  529 | `	if( pMap == MAP_FAILED ){` |
|   ! 0 |  530 | `		rc = -1;` |
|   ! 0 |  531 | `	}else{` |
|     - |  532 | `		/* Point to the memory view */` |
|  4492 |  533 | `		*ppMap = pMap;` |
|  4492 |  534 | `		*pSize = (ph7_int64)st.st_size;` |
|     - |  535 | `	}` |
|  4492 |  536 | `	close(fd);` |
|  4492 |  537 | `	return rc;` |
|  2246 |  538 | `}` |
|     - |  539 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|  4492 |  540 | `static void UnixVfs_Unmap(void *pView,ph7_int64 nSize)` |
|     - |  541 | `{` |
|  4492 |  542 | `	munmap(pView,(size_t)nSize);` |
|  4492 |  543 | `}` |
|     - |  544 | `/* void (*xTempDir)(ph7_context *) */` |
|   236 |  545 | `static void UnixVfs_TempDir(ph7_context *pCtx)` |
|     - |  546 | `{` |
|     - |  547 | `  const char *zDir;` |
|     - |  548 | `  /* php's php_get_temporary_directory: honour TMPDIR, else fall back to P_tmpdir` |
|     - |  549 | `   * ("/tmp" on Unix). PH7 also scanned /var/tmp, /usr/tmp, /usr/local/tmp first,` |
|     - |  550 | `   * which returned /var/tmp on a typical box where php returns /tmp — a divergence` |
|     - |  551 | `   * observable through sys_get_temp_dir()/tempnam()/session paths. */` |
|   236 |  552 | `  zDir = getenv("TMPDIR");` |
|   236 |  553 | `  if( zDir && zDir[0] != 0 && !access(zDir,07) ){` |
|     - |  554 | `	  /* php reports the temp dir WITHOUT a trailing separator; macOS's TMPDIR ends with` |
|     - |  555 | `	   * one, so returning it verbatim produced paths like "/var/.../T//file". */` |
|   118 |  556 | `	  int nDir = (int)strlen(zDir);` |
|   236 |  557 | `	  while( nDir > 1 && zDir[nDir-1] == '/' ){` |
|   118 |  558 | `		  nDir--;` |
|     - |  559 | `	  }` |
|   118 |  560 | `	  ph7_result_string(pCtx,zDir,nDir);` |
|   118 |  561 | `	  return;` |
|     - |  562 | `  }` |
|   118 |  563 | `  ph7_result_string(pCtx,"/tmp",(int)sizeof("/tmp")-1);` |
|   118 |  564 | `}` |
|     - |  565 | `/* unsigned int (*xProcessId)(void) */` |
|    92 |  566 | `static unsigned int UnixVfs_ProcessId(void)` |
|     - |  567 | `{` |
|    92 |  568 | `	return (unsigned int)getpid();` |
|     - |  569 | `}` |
|     - |  570 | `/* int (*xUid)(void) */` |
|     2 |  571 | `static int UnixVfs_uid(void)` |
|     - |  572 | `{` |
|     2 |  573 | `	return (int)getuid();` |
|     - |  574 | `}` |
|     - |  575 | `/* int (*xGid)(void) */` |
|     2 |  576 | `static int UnixVfs_gid(void)` |
|     - |  577 | `{` |
|     2 |  578 | `	return (int)getgid();` |
|     - |  579 | `}` |
|     - |  580 | `/* int (*xUmask)(int) */` |
|     8 |  581 | `static int UnixVfs_Umask(int new_mask)` |
|     - |  582 | `{` |
|     - |  583 | `	int old_mask;` |
|     8 |  584 | `	old_mask = umask(new_mask);` |
|     8 |  585 | `	return old_mask;` |
|     - |  586 | `}` |
|     - |  587 | `/* void (*xUsername)(ph7_context *) */` |
|     2 |  588 | `static void UnixVfs_Username(ph7_context *pCtx)` |
|     - |  589 | `{` |
|     - |  590 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  591 | `  struct passwd *pwd;` |
|     - |  592 | `  uid_t uid;` |
|     2 |  593 | `  uid = getuid();` |
|     2 |  594 | `  pwd = getpwuid(uid);   /* Try getting UID for username */` |
|     2 |  595 | `  if (pwd == 0) {` |
|   ! 0 |  596 | `    return;` |
|     - |  597 | `  }` |
|     - |  598 | `  /* Return the username */` |
|     2 |  599 | `  ph7_result_string(pCtx,pwd->pw_name,-1);` |
|     - |  600 | `#else` |
|     - |  601 | `  ph7_result_string(pCtx,"Unknown",-1);` |
|     - |  602 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     2 |  603 | `  return;` |
|     1 |  604 | `}` |
|     - |  605 | `/* int (*xLink)(const char *,const char *,int) */` |
|     8 |  606 | `static int UnixVfs_link(const char *zSrc,const char *zTarget,int is_sym)` |
|     - |  607 | `{` |
|     8 |  608 | `	zSrc = UnixVfsLocalPath(zSrc);` |
|     8 |  609 | `	zTarget = UnixVfsLocalPath(zTarget);` |
|     - |  610 | `	int rc;` |
|     8 |  611 | `	if( is_sym ){` |
|     - |  612 | `		/* Symbolic link */` |
|     6 |  613 | `		rc = symlink(zSrc,zTarget);` |
|     3 |  614 | `	}else{` |
|     - |  615 | `		/* Hard link */` |
|     2 |  616 | `		rc = link(zSrc,zTarget);` |
|     - |  617 | `	}` |
|     8 |  618 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  619 | `}` |
|     - |  620 | `/* int (*xChroot)(const char *) */` |
|     2 |  621 | `static int UnixVfs_chroot(const char *zRootDir)` |
|     - |  622 | `{` |
|     - |  623 | `	int rc;` |
|     2 |  624 | `	rc = chroot(zRootDir);` |
|     2 |  625 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  626 | `}` |
|     - |  627 | `/* Export the UNIX vfs */` |
|     - |  628 | `PH7_PRIVATE const ph7_vfs sUnixVfs = {` |
|     - |  629 | `	"Unix_vfs",` |
|     - |  630 | `	PH7_VFS_VERSION,` |
|     - |  631 | `	UnixVfs_chdir,    /* int (*xChdir)(const char *) */` |
|     - |  632 | `	UnixVfs_chroot,   /* int (*xChroot)(const char *); */` |
|     - |  633 | `	UnixVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|     - |  634 | `	UnixVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|     - |  635 | `	UnixVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|     - |  636 | `	UnixVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|     - |  637 | `	UnixVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|     - |  638 | `	UnixVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|     - |  639 | `	UnixVfs_Sleep,    /* int (*xSleep)(unsigned int) */` |
|     - |  640 | `	UnixVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|     - |  641 | `	UnixVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|     - |  642 | `	UnixVfs_Chmod, /*int (*xChmod)(const char *,int)*/` |
|     - |  643 | `	UnixVfs_Chown, /*int (*xChown)(const char *,const char *)*/` |
|     - |  644 | `	UnixVfs_Chgrp, /*int (*xChgrp)(const char *,const char *)*/` |
|     - |  645 | `	UnixVfs_FreeSpace,  /* ph7_int64 (*xFreeSpace)(const char *) */` |
|     - |  646 | `	UnixVfs_TotalSpace, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|     - |  647 | `	UnixVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|     - |  648 | `	UnixVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|     - |  649 | `	UnixVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|     - |  650 | `	UnixVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|     - |  651 | `	UnixVfs_Stat,  /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|     - |  652 | `	UnixVfs_lStat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|     - |  653 | `	UnixVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|     - |  654 | `	UnixVfs_islink,     /* int (*xIslink)(const char *) */` |
|     - |  655 | `	UnixVfs_isreadable, /* int (*xReadable)(const char *) */` |
|     - |  656 | `	UnixVfs_iswritable, /* int (*xWritable)(const char *) */` |
|     - |  657 | `	UnixVfs_isexecutable,/* int (*xExecutable)(const char *) */` |
|     - |  658 | `	UnixVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|     - |  659 | `	UnixVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|     - |  660 | `	UnixVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|     - |  661 | `	UnixVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|     - |  662 | `	UnixVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|     - |  663 | `	UnixVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|     - |  664 | `	UnixVfs_link,       /* int (*xLink)(const char *,const char *,int) */` |
|     - |  665 | `	UnixVfs_Umask,      /* int (*xUmask)(int) */` |
|     - |  666 | `	UnixVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|     - |  667 | `	UnixVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|     - |  668 | `	UnixVfs_uid, /* int (*xUid)(void) */` |
|     - |  669 | `	UnixVfs_gid, /* int (*xGid)(void) */` |
|     - |  670 | `	UnixVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|     - |  671 | `	0 /* int (*xExec)(const char *,ph7_context *) */` |
|     - |  672 | `};` |
|     - |  673 | `/* UNIX File IO */` |
|     - |  674 | `#define PH7_UNIX_OPEN_MODE	0640 /* Default open mode */` |
|     - |  675 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
| 30994 |  676 | `static int UnixFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|     - |  677 | `{` |
| 30994 |  678 | `	int iOpen = O_RDONLY;` |
|     - |  679 | `	int fd;` |
|     - |  680 | `	/* Set the desired flags according to the open mode */` |
| 30994 |  681 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|     - |  682 | `		/* Open existing file, or create if it doesn't exist */` |
| 14120 |  683 | `		iOpen = O_CREAT;` |
| 14120 |  684 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|     - |  685 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
| 14120 |  686 | `			iOpen \|= O_TRUNC;` |
|  7060 |  687 | `			SXUNUSED(pResource); /* cc warning */` |
|  7060 |  688 | `		}` |
| 23934 |  689 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|     - |  690 | `		/* Creates a new file, only if it does not already exist.` |
|     - |  691 | `		* If the file exists, it fails.` |
|     - |  692 | `		*/` |
|   140 |  693 | `		iOpen = O_CREAT\|O_EXCL;` |
| 16804 |  694 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|     - |  695 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|     - |  696 | `		 * The file must exist.` |
|     - |  697 | `		 */` |
|   ! 0 |  698 | `		iOpen = O_RDWR\|O_TRUNC;` |
|   ! 0 |  699 | `	}` |
| 30994 |  700 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|     - |  701 | `		/* Read+Write access */` |
| 14092 |  702 | `		iOpen &= ~O_RDONLY;` |
| 14092 |  703 | `		iOpen \|= O_RDWR;` |
| 23948 |  704 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|     - |  705 | `		/* Write only access */` |
|   170 |  706 | `		iOpen &= ~O_RDONLY;` |
|   170 |  707 | `		iOpen \|= O_WRONLY;` |
|    85 |  708 | `	}` |
| 30994 |  709 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|     - |  710 | `		/* Append mode */` |
|   ! 0 |  711 | `		iOpen \|= O_APPEND;` |
|   ! 0 |  712 | `	}` |
|     - |  713 | `#ifdef O_TEMP` |
|     - |  714 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|     - |  715 | `		/* File is temporary */` |
|     - |  716 | `		iOpen \|= O_TEMP;` |
|     - |  717 | `	}` |
|     - |  718 | `#endif` |
|     - |  719 | `	/* Open the file now */` |
| 30994 |  720 | `	fd = open(zPath,iOpen,PH7_UNIX_OPEN_MODE);` |
| 30994 |  721 | `	if( fd < 0 ){` |
|     - |  722 | `		/* IO error */` |
|    20 |  723 | `		return -1;` |
|     - |  724 | `	}` |
|     - |  725 | `	/* Save the handle */` |
| 30974 |  726 | `	*ppHandle = SX_INT_TO_PTR(fd);` |
| 30974 |  727 | `	return PH7_OK;` |
| 15497 |  728 | `}` |
|     - |  729 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|  1138 |  730 | `static int UnixDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|     - |  731 | `{` |
|     - |  732 | `	DIR *pDir;` |
|     - |  733 | `	/* Open the target directory */` |
|  1138 |  734 | `	pDir = opendir(zPath);` |
|  1138 |  735 | `	if( pDir == 0 ){` |
|   ! 0 |  736 | `		SXUNUSED(pResource); /* Compiler warning */` |
|   ! 0 |  737 | `		return -1;` |
|     - |  738 | `	}` |
|     - |  739 | `	/* Save our structure */` |
|  1138 |  740 | `	*ppHandle = pDir;` |
|  1138 |  741 | `	return PH7_OK;` |
|   569 |  742 | `}` |
|     - |  743 | `/* void (*xCloseDir)(void *) */` |
|  1138 |  744 | `static void UnixDir_Close(void *pUserData)` |
|     - |  745 | `{` |
|  1138 |  746 | `	closedir((DIR *)pUserData);` |
|  1138 |  747 | `}` |
|     - |  748 | `/* void (*xClose)(void *); */` |
| 31000 |  749 | `static void UnixFile_Close(void *pUserData)` |
|     - |  750 | `{` |
| 31000 |  751 | `	close(SX_PTR_TO_INT(pUserData));` |
| 31000 |  752 | `}` |
|     - |  753 | `/* int (*xReadDir)(void *,ph7_context *) */` |
| 11820 |  754 | `static int UnixDir_Read(void *pUserData,ph7_context *pCtx)` |
|     - |  755 | `{` |
| 11820 |  756 | `	DIR *pDir = (DIR *)pUserData;` |
|     - |  757 | `	struct dirent *pEntry;` |
|     - |  758 | `	char *zName;` |
|     - |  759 | `	sxu32 n;` |
|     - |  760 | `	/* php's readdir() yields every entry including '.' and '..' */` |
| 11820 |  761 | `	pEntry = readdir(pDir);` |
| 11820 |  762 | `	if( pEntry == 0 ){` |
|     - |  763 | `		/* No more entries to process */` |
|  1134 |  764 | `		return -1;` |
|     - |  765 | `	}` |
| 10686 |  766 | `	zName = pEntry->d_name;` |
| 10686 |  767 | `	n = SyStrlen(zName);` |
|     - |  768 | `	/* Return the current file name */` |
| 10686 |  769 | `	ph7_result_string(pCtx,zName,(int)n);` |
| 10686 |  770 | `	return PH7_OK;` |
|  5910 |  771 | `}` |
|     - |  772 | `/* void (*xRewindDir)(void *) */` |
|     2 |  773 | `static void UnixDir_Rewind(void *pUserData)` |
|     - |  774 | `{` |
|     2 |  775 | `	rewinddir((DIR *)pUserData);` |
|     2 |  776 | `}` |
|     - |  777 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
| 33436 |  778 | `static ph7_int64 UnixFile_Read(void *pUserData,void *pBuffer,ph7_int64 nDatatoRead)` |
|     - |  779 | `{` |
|     - |  780 | `	ssize_t nRd;` |
| 33436 |  781 | `	nRd = read(SX_PTR_TO_INT(pUserData),pBuffer,(size_t)nDatatoRead);` |
| 33436 |  782 | `	if( nRd < 1 ){` |
|     - |  783 | `		/* EOF or IO error */` |
| 16700 |  784 | `		return -1;` |
|     - |  785 | `	}` |
| 16736 |  786 | `	return (ph7_int64)nRd;` |
| 16718 |  787 | `}` |
|     - |  788 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
| 14132 |  789 | `static ph7_int64 UnixFile_Write(void *pUserData,const void *pBuffer,ph7_int64 nWrite)` |
|     - |  790 | `{` |
| 14132 |  791 | `	const char *zData = (const char *)pBuffer;` |
| 14132 |  792 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|     - |  793 | `	ph7_int64 nCount;` |
|     - |  794 | `	ssize_t nWr;` |
| 14132 |  795 | `	nCount = 0;` |
| 14132 |  796 | `	for(;;){` |
| 28264 |  797 | `		if( nWrite < 1 ){` |
| 14132 |  798 | `			break;` |
|     - |  799 | `		}` |
| 14132 |  800 | `		nWr = write(fd,zData,(size_t)nWrite);` |
| 14132 |  801 | `		if( nWr < 1 ){` |
|     - |  802 | `			/* IO error */` |
|   ! 0 |  803 | `			break;` |
|     - |  804 | `		}` |
| 14132 |  805 | `		nWrite -= nWr;` |
| 14132 |  806 | `		nCount += nWr;` |
| 14132 |  807 | `		zData += nWr;` |
|     - |  808 | `	}` |
| 14132 |  809 | `	if( nWrite > 0 ){` |
|   ! 0 |  810 | `		return -1;` |
|     - |  811 | `	}` |
| 14132 |  812 | `	return nCount;` |
|  7066 |  813 | `}` |
|     - |  814 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|    12 |  815 | `static int UnixFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|     - |  816 | `{` |
|     - |  817 | `	off_t iNew;` |
|    12 |  818 | `	switch(whence){` |
|   ! 0 |  819 | `	case 1:/*SEEK_CUR*/` |
|   ! 0 |  820 | `		whence = SEEK_CUR;` |
|   ! 0 |  821 | `		break;` |
|   ! 0 |  822 | `	case 2: /* SEEK_END */` |
|   ! 0 |  823 | `		whence = SEEK_END;` |
|   ! 0 |  824 | `		break;` |
|    12 |  825 | `	case 0: /* SEEK_SET */` |
|     - |  826 | `	default:` |
|    12 |  827 | `		whence = SEEK_SET;` |
|    12 |  828 | `		break;` |
|     - |  829 | `	}` |
|    12 |  830 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),(off_t)iOfft,whence);` |
|    12 |  831 | `	if( iNew < 0 ){` |
|   ! 0 |  832 | `		return -1;` |
|     - |  833 | `	}` |
|    12 |  834 | `	return PH7_OK;` |
|     6 |  835 | `}` |
|     - |  836 | `/* int (*xLock)(void *,int) — see the lock_type value space in ph7.h */` |
|    24 |  837 | `static int UnixFile_Lock(void *pUserData,int lock_type)` |
|     - |  838 | `{` |
|    24 |  839 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|     - |  840 | `	int op;` |
|     - |  841 | `	int rc;` |
|    24 |  842 | `	if( lock_type < 0 ){` |
|     - |  843 | `		/* Unlock the file */` |
|     8 |  844 | `		op = LOCK_UN;` |
|     4 |  845 | `	}else{` |
|    16 |  846 | `		op = (lock_type == PH7_IO_LOCK_EX \|\| lock_type == PH7_IO_LOCK_EX_NB) ? LOCK_EX : LOCK_SH;` |
|    16 |  847 | `		if( lock_type == PH7_IO_LOCK_SH_NB \|\| lock_type == PH7_IO_LOCK_EX_NB ){` |
|    12 |  848 | `			op \|= LOCK_NB;` |
|     6 |  849 | `		}` |
|     - |  850 | `	}` |
|    24 |  851 | `	rc = flock(fd,op);` |
|    24 |  852 | `	if( rc == 0 ){` |
|    20 |  853 | `		return PH7_OK;` |
|     - |  854 | `	}` |
|     - |  855 | `	/* A non-blocking request that another holder refused is php's $would_block,` |
|     - |  856 | `	 * not an IO failure. (EWOULDBLOCK and EAGAIN are the same value on every` |
|     - |  857 | `	 * platform this driver builds for.) */` |
|     4 |  858 | `	if( errno == EWOULDBLOCK ){` |
|     4 |  859 | `		return SXERR_BUSY;` |
|     - |  860 | `	}` |
|   ! 0 |  861 | `	return -1;` |
|    12 |  862 | `}` |
|     - |  863 | `/* ph7_int64 (*xTell)(void *) */` |
|     6 |  864 | `static ph7_int64 UnixFile_Tell(void *pUserData)` |
|     - |  865 | `{` |
|     - |  866 | `	off_t iNew;` |
|     6 |  867 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),0,SEEK_CUR);` |
|     6 |  868 | `	return (ph7_int64)iNew;` |
|     - |  869 | `}` |
|     - |  870 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|     2 |  871 | `static int UnixFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|     - |  872 | `{` |
|     - |  873 | `	int rc;` |
|     2 |  874 | `	rc = ftruncate(SX_PTR_TO_INT(pUserData),(off_t)nOfft);` |
|     2 |  875 | `	if( rc != 0 ){` |
|   ! 0 |  876 | `		return -1;` |
|     - |  877 | `	}` |
|     2 |  878 | `	return PH7_OK;` |
|     1 |  879 | `}` |
|     - |  880 | `/* int (*xSync)(void *); */` |
|     2 |  881 | `static int UnixFile_Sync(void *pUserData)` |
|     - |  882 | `{` |
|     - |  883 | `	int rc;` |
|     2 |  884 | `	rc = fsync(SX_PTR_TO_INT(pUserData));` |
|     2 |  885 | `	return rc == 0 ? PH7_OK : - 1;` |
|     - |  886 | `}` |
|     - |  887 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|     2 |  888 | `static int UnixFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  889 | `{` |
|     - |  890 | `	struct stat st;` |
|     - |  891 | `	int rc;` |
|     2 |  892 | `	rc = fstat(SX_PTR_TO_INT(pUserData),&st);` |
|     2 |  893 | `	if( rc != 0 ){` |
|   ! 0 |  894 | `	 return -1;` |
|     - |  895 | `	}` |
|     - |  896 | `	/* dev */` |
|     2 |  897 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|     2 |  898 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  899 | `	/* ino */` |
|     2 |  900 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|     2 |  901 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  902 | `	/* mode */` |
|     2 |  903 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|     2 |  904 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  905 | `	/* nlink */` |
|     2 |  906 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|     2 |  907 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  908 | `	/* uid,gid,rdev */` |
|     2 |  909 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|     2 |  910 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|     2 |  911 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|     2 |  912 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|     2 |  913 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|     2 |  914 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  915 | `	/* size */` |
|     2 |  916 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|     2 |  917 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  918 | `	/* atime */` |
|     2 |  919 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|     2 |  920 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  921 | `	/* mtime */` |
|     2 |  922 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|     2 |  923 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  924 | `	/* ctime */` |
|     2 |  925 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|     2 |  926 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  927 | `	/* blksize,blocks */` |
|     2 |  928 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|     2 |  929 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|     2 |  930 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|     2 |  931 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|     2 |  932 | `	return PH7_OK;` |
|     1 |  933 | `}` |
|     - |  934 | `/* Export the file:// stream */` |
|     - |  935 | `PH7_PRIVATE const ph7_io_stream sUnixFileStream = {` |
|     - |  936 | `	"file", /* Stream name */` |
|     - |  937 | `	PH7_IO_STREAM_VERSION,` |
|     - |  938 | `	UnixFile_Open,  /* xOpen */` |
|     - |  939 | `	UnixDir_Open,   /* xOpenDir */` |
|     - |  940 | `	UnixFile_Close, /* xClose */` |
|     - |  941 | `	UnixDir_Close,  /* xCloseDir */` |
|     - |  942 | `	UnixFile_Read,  /* xRead */` |
|     - |  943 | `	UnixDir_Read,   /* xReadDir */` |
|     - |  944 | `	UnixFile_Write, /* xWrite */` |
|     - |  945 | `	UnixFile_Seek,  /* xSeek */` |
|     - |  946 | `	UnixFile_Lock,  /* xLock */` |
|     - |  947 | `	UnixDir_Rewind, /* xRewindDir */` |
|     - |  948 | `	UnixFile_Tell,  /* xTell */` |
|     - |  949 | `	UnixFile_Trunc, /* xTrunc */` |
|     - |  950 | `	UnixFile_Sync,  /* xSeek */` |
|     - |  951 | `	UnixFile_Stat   /* xStat */` |
|     - |  952 | `};` |
|     - |  953 | `#endif /* __UNIXES__ */` |
|     - |  954 |  |
