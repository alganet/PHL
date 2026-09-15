# src/ph7/vfs_unix.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 421/471 lines (89.38%)

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
|     - |   30 | `/*` |
|     - |   31 | ` * php's file:// wrapper is the default local-file scheme: a filesystem builtin` |
|     - |   32 | ` * given "file://[authority]/path" acts on the plain "/path". The authority is` |
|     - |   33 | ` * empty (file:///abs) or "localhost"; the scheme match is case-insensitive.` |
|     - |   34 | ` * The native syscalls below do not understand the scheme, so strip it here.` |
|     - |   35 | ` * Anything else (a non-local authority, or no path component) is left intact so` |
|     - |   36 | ` * the syscall fails exactly as php does. chdir()/chroot()/realpath() are NOT` |
|     - |   37 | ` * routed through here -- php rejects file:// for those.` |
|     - |   38 | ` */` |
| 52392 |   39 | `static const char * UnixVfsLocalPath(const char *zPath)` |
|     - |   40 | `{` |
|     - |   41 | `	const char *zRest;` |
| 52392 |   42 | `	if( zPath == 0 \|\| SyStrnicmp(zPath,"file://",sizeof("file://")-1) != 0 ){` |
| 52382 |   43 | `		return zPath;` |
|     - |   44 | `	}` |
|    10 |   45 | `	zRest = &zPath[sizeof("file://")-1];` |
|    10 |   46 | `	if( zRest[0] == '/' ){` |
|     - |   47 | `		/* file:///abs -> /abs (empty authority) */` |
|    10 |   48 | `		return zRest;` |
|     - |   49 | `	}` |
|   ! 0 |   50 | `	if( SyStrnicmp(zRest,"localhost/",sizeof("localhost/")-1) == 0 ){` |
|     - |   51 | `		/* file://localhost/abs -> /abs (keep the leading slash) */` |
|   ! 0 |   52 | `		return &zRest[sizeof("localhost")-1];` |
|     - |   53 | `	}` |
|   ! 0 |   54 | `	return zPath;` |
| 26196 |   55 | `}` |
|     - |   56 | `/* int (*xchdir)(const char *) */` |
| 12938 |   57 | `static int UnixVfs_chdir(const char *zPath)` |
|     - |   58 | `{` |
|     - |   59 | `  int rc;` |
| 12938 |   60 | `  rc = chdir(zPath);` |
| 12938 |   61 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |   62 | `}` |
|     - |   63 | `/* int (*xGetcwd)(ph7_context *) */` |
|    20 |   64 | `static int UnixVfs_getcwd(ph7_context *pCtx)` |
|     - |   65 | `{` |
|     - |   66 | `	char zBuf[4096];` |
|     - |   67 | `	char *zDir;` |
|     - |   68 | `	/* Get the current directory */` |
|    20 |   69 | `	zDir = getcwd(zBuf,sizeof(zBuf));` |
|    20 |   70 | `	if( zDir == 0 ){` |
|   ! 0 |   71 | `	  return -1;` |
|     - |   72 | `    }` |
|    20 |   73 | `	ph7_result_string(pCtx,zDir,-1/*Compute length automatically*/);` |
|    20 |   74 | `	return PH7_OK;` |
|    10 |   75 | `}` |
|     - |   76 | `/* int (*xMkdir)(const char *,int,int) */` |
|    46 |   77 | `static int UnixVfs_mkdir(const char *zPath,int mode,int recursive)` |
|     - |   78 | `{` |
|    46 |   79 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |   80 | `	int rc;` |
|    46 |   81 | `        rc = mkdir(zPath,mode);` |
|    23 |   82 | `	SXUNUSED(recursive); /* cc warning */` |
|    46 |   83 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   84 | `}` |
|     - |   85 | `/* int (*xRmdir)(const char *) */` |
|    46 |   86 | `static int UnixVfs_rmdir(const char *zPath)` |
|     - |   87 | `{` |
|    46 |   88 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |   89 | `	int rc;` |
|    46 |   90 | `	rc = rmdir(zPath);` |
|    46 |   91 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   92 | `}` |
|     - |   93 | `/* int (*xIsdir)(const char *) */` |
|  8654 |   94 | `static int UnixVfs_isdir(const char *zPath)` |
|     - |   95 | `{` |
|  8654 |   96 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |   97 | `	struct stat st;` |
|     - |   98 | `	int rc;` |
|  8654 |   99 | `	rc = stat(zPath,&st);` |
|  8654 |  100 | `	if( rc != 0 ){` |
|     4 |  101 | `	 return -1;` |
|     - |  102 | `	}` |
|  8650 |  103 | `	rc = S_ISDIR(st.st_mode);` |
|  8650 |  104 | `	return rc ? PH7_OK : -1 ;` |
|  4327 |  105 | `}` |
|     - |  106 | `/* int (*xRename)(const char *,const char *) */` |
|     2 |  107 | `static int UnixVfs_Rename(const char *zOld,const char *zNew)` |
|     - |  108 | `{` |
|     2 |  109 | `	zOld = UnixVfsLocalPath(zOld);` |
|     2 |  110 | `	zNew = UnixVfsLocalPath(zNew);` |
|     - |  111 | `	int rc;` |
|     2 |  112 | `	rc = rename(zOld,zNew);` |
|     2 |  113 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  114 | `}` |
|     - |  115 | `/* int (*xRealpath)(const char *,ph7_context *) */` |
|     6 |  116 | `static int UnixVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|     - |  117 | `{` |
|     - |  118 | `#ifndef PH7_UNIX_OLD_LIBC` |
|     - |  119 | `	char *zReal;` |
|     6 |  120 | `	zReal = realpath(zPath,0);` |
|     6 |  121 | `	if( zReal == 0 ){` |
|     2 |  122 | `	  return -1;` |
|     - |  123 | `	}` |
|     4 |  124 | `	ph7_result_string(pCtx,zReal,-1/*Compute length automatically*/);` |
|     - |  125 | `        /* Release the allocated buffer */` |
|     4 |  126 | `	free(zReal);` |
|     4 |  127 | `	return PH7_OK;` |
|     - |  128 | `#else` |
|     - |  129 | `    zPath = 0; /* cc warning */` |
|     - |  130 | `    pCtx = 0;` |
|     - |  131 | `    return -1;` |
|     - |  132 | `#endif` |
|     3 |  133 | `}` |
|     - |  134 | `/* int (*xSleep)(unsigned int) */` |
|    60 |  135 | `static int UnixVfs_Sleep(unsigned int uSec)` |
|     - |  136 | `{` |
|    60 |  137 | `	usleep(uSec);` |
|    60 |  138 | `	return PH7_OK;` |
|     - |  139 | `}` |
|     - |  140 | `/* int (*xUnlink)(const char *) */` |
| 32808 |  141 | `static int UnixVfs_unlink(const char *zPath)` |
|     - |  142 | `{` |
| 32808 |  143 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  144 | `	int rc;` |
| 32808 |  145 | `	rc = unlink(zPath);` |
| 32808 |  146 | `	return rc == 0 ? PH7_OK : -1 ;` |
|     - |  147 | `}` |
|     - |  148 | `/* int (*xFileExists)(const char *) */` |
|   160 |  149 | `static int UnixVfs_FileExists(const char *zPath)` |
|     - |  150 | `{` |
|   160 |  151 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  152 | `	int rc;` |
|   160 |  153 | `	rc = access(zPath,F_OK);` |
|   160 |  154 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  155 | `}` |
|     - |  156 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|     - |  157 | `/* ph7_int64 (*xFreeSpace)(const char *) */` |
|     4 |  158 | `static ph7_int64 UnixVfs_FreeSpace(const char *zPath)` |
|     - |  159 | `{` |
|     4 |  160 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  161 | `	struct statvfs sInfo;` |
|     4 |  162 | `	if( statvfs(zPath,&sInfo) != 0 ){` |
|   ! 0 |  163 | `		return -1;` |
|     - |  164 | `	}` |
|     - |  165 | `	/* php reports the space available to an UNPRIVILEGED user (f_bavail) */` |
|     4 |  166 | `	return (ph7_int64)sInfo.f_bavail * (ph7_int64)sInfo.f_frsize;` |
|     2 |  167 | `}` |
|     - |  168 | `/* ph7_int64 (*xTotalSpace)(const char *) */` |
|     4 |  169 | `static ph7_int64 UnixVfs_TotalSpace(const char *zPath)` |
|     - |  170 | `{` |
|     4 |  171 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  172 | `	struct statvfs sInfo;` |
|     4 |  173 | `	if( statvfs(zPath,&sInfo) != 0 ){` |
|   ! 0 |  174 | `		return -1;` |
|     - |  175 | `	}` |
|     4 |  176 | `	return (ph7_int64)sInfo.f_blocks * (ph7_int64)sInfo.f_frsize;` |
|     2 |  177 | `}` |
|    10 |  178 | `static ph7_int64 UnixVfs_FileSize(const char *zPath)` |
|     - |  179 | `{` |
|    10 |  180 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  181 | `	struct stat st;` |
|     - |  182 | `	int rc;` |
|    10 |  183 | `	rc = stat(zPath,&st);` |
|    10 |  184 | `	if( rc != 0 ){` |
|   ! 0 |  185 | `	 return -1;` |
|     - |  186 | `	}` |
|    10 |  187 | `	return (ph7_int64)st.st_size;` |
|     5 |  188 | `}` |
|     - |  189 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|     4 |  190 | `static int UnixVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|     - |  191 | `{` |
|     4 |  192 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  193 | `	struct utimbuf ut;` |
|     - |  194 | `	int rc;` |
|     4 |  195 | `	ut.actime  = (time_t)access_time;` |
|     4 |  196 | `	ut.modtime = (time_t)touch_time;` |
|     4 |  197 | `	rc = utime(zPath,&ut);` |
|     4 |  198 | `	if( rc != 0 ){` |
|   ! 0 |  199 | `	 return -1;` |
|     - |  200 | `	}` |
|     4 |  201 | `	return PH7_OK;` |
|     2 |  202 | `}` |
|     - |  203 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|     2 |  204 | `static ph7_int64 UnixVfs_FileAtime(const char *zPath)` |
|     - |  205 | `{` |
|     2 |  206 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  207 | `	struct stat st;` |
|     - |  208 | `	int rc;` |
|     2 |  209 | `	rc = stat(zPath,&st);` |
|     2 |  210 | `	if( rc != 0 ){` |
|   ! 0 |  211 | `	 return -1;` |
|     - |  212 | `	}` |
|     2 |  213 | `	return (ph7_int64)st.st_atime;` |
|     1 |  214 | `}` |
|     - |  215 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|     4 |  216 | `static ph7_int64 UnixVfs_FileMtime(const char *zPath)` |
|     - |  217 | `{` |
|     4 |  218 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  219 | `	struct stat st;` |
|     - |  220 | `	int rc;` |
|     4 |  221 | `	rc = stat(zPath,&st);` |
|     4 |  222 | `	if( rc != 0 ){` |
|   ! 0 |  223 | `	 return -1;` |
|     - |  224 | `	}` |
|     4 |  225 | `	return (ph7_int64)st.st_mtime;` |
|     2 |  226 | `}` |
|     - |  227 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|     2 |  228 | `static ph7_int64 UnixVfs_FileCtime(const char *zPath)` |
|     - |  229 | `{` |
|     2 |  230 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  231 | `	struct stat st;` |
|     - |  232 | `	int rc;` |
|     2 |  233 | `	rc = stat(zPath,&st);` |
|     2 |  234 | `	if( rc != 0 ){` |
|   ! 0 |  235 | `	 return -1;` |
|     - |  236 | `	}` |
|     2 |  237 | `	return (ph7_int64)st.st_ctime;` |
|     1 |  238 | `}` |
|     - |  239 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    10 |  240 | `static int UnixVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  241 | `{` |
|    10 |  242 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  243 | `	struct stat st;` |
|     - |  244 | `	int rc;` |
|    10 |  245 | `	rc = stat(zPath,&st);` |
|    10 |  246 | `	if( rc != 0 ){` |
|   ! 0 |  247 | `	 return -1;` |
|     - |  248 | `	}` |
|     - |  249 | `	/* dev */` |
|    10 |  250 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|    10 |  251 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  252 | `	/* ino */` |
|    10 |  253 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|    10 |  254 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  255 | `	/* mode */` |
|    10 |  256 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|    10 |  257 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  258 | `	/* nlink */` |
|    10 |  259 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|    10 |  260 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  261 | `	/* uid,gid,rdev */` |
|    10 |  262 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|    10 |  263 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    10 |  264 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|    10 |  265 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    10 |  266 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|    10 |  267 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  268 | `	/* size */` |
|    10 |  269 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|    10 |  270 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  271 | `	/* atime */` |
|    10 |  272 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|    10 |  273 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  274 | `	/* mtime */` |
|    10 |  275 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|    10 |  276 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  277 | `	/* ctime */` |
|    10 |  278 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|    10 |  279 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  280 | `	/* blksize,blocks */` |
|    10 |  281 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|    10 |  282 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    10 |  283 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|    10 |  284 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    10 |  285 | `	return PH7_OK;` |
|     5 |  286 | `}` |
|     - |  287 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|     2 |  288 | `static int UnixVfs_lStat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  289 | `{` |
|     2 |  290 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  291 | `	struct stat st;` |
|     - |  292 | `	int rc;` |
|     2 |  293 | `	rc = lstat(zPath,&st);` |
|     2 |  294 | `	if( rc != 0 ){` |
|   ! 0 |  295 | `	 return -1;` |
|     - |  296 | `	}` |
|     - |  297 | `	/* dev */` |
|     2 |  298 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|     2 |  299 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  300 | `	/* ino */` |
|     2 |  301 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|     2 |  302 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  303 | `	/* mode */` |
|     2 |  304 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|     2 |  305 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  306 | `	/* nlink */` |
|     2 |  307 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|     2 |  308 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  309 | `	/* uid,gid,rdev */` |
|     2 |  310 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|     2 |  311 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|     2 |  312 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|     2 |  313 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|     2 |  314 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|     2 |  315 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  316 | `	/* size */` |
|     2 |  317 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|     2 |  318 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  319 | `	/* atime */` |
|     2 |  320 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|     2 |  321 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  322 | `	/* mtime */` |
|     2 |  323 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|     2 |  324 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  325 | `	/* ctime */` |
|     2 |  326 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|     2 |  327 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  328 | `	/* blksize,blocks */` |
|     2 |  329 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|     2 |  330 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|     2 |  331 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|     2 |  332 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|     2 |  333 | `	return PH7_OK;` |
|     1 |  334 | `}` |
|     - |  335 | `/* int (*xChmod)(const char *,int) */` |
|   140 |  336 | `static int UnixVfs_Chmod(const char *zPath,int mode)` |
|     - |  337 | `{` |
|   140 |  338 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  339 | `    int rc;` |
|   140 |  340 | `    rc = chmod(zPath,(mode_t)mode);` |
|   140 |  341 | `    return rc == 0 ? PH7_OK : - 1;` |
|     - |  342 | `}` |
|     - |  343 | `/* int (*xChown)(const char *,const char *) */` |
|     6 |  344 | `static int UnixVfs_Chown(const char *zPath,const char *zUser)` |
|     - |  345 | `{` |
|     6 |  346 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  347 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  348 | `  struct passwd *pwd;` |
|     - |  349 | `  uid_t uid;` |
|     - |  350 | `  int rc;` |
|     - |  351 | `  /* php accepts a numeric uid as well as a name; PH7 only ever did getpwnam(), so` |
|     - |  352 | `   * chown($f, 0) failed on the NAME lookup and never reached the syscall (leaving errno` |
|     - |  353 | `   * unset, hence a bogus "Undefined error: 0" in the warning). */` |
|     6 |  354 | `  if( zUser[0] >= '0' && zUser[0] <= '9' ){` |
|     4 |  355 | `    uid = (uid_t)atoi(zUser);` |
|     2 |  356 | `  }else{` |
|     2 |  357 | `    pwd = getpwnam(zUser);   /* Try getting UID for username */` |
|     2 |  358 | `    if (pwd == 0) {` |
|     - |  359 | `      /* -2 = the NAME could not be resolved (no syscall ran, so errno means nothing).` |
|     - |  360 | `       * php words that case differently: "chown(): Unable to find uid for bogus". */` |
|     2 |  361 | `      return -2;` |
|     - |  362 | `    }` |
|   ! 0 |  363 | `    uid = pwd->pw_uid;` |
|     - |  364 | `  }` |
|     4 |  365 | `  rc = chown(zPath,uid,-1);` |
|     4 |  366 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |  367 | `#else` |
|     - |  368 | `	SXUNUSED(zPath);` |
|     - |  369 | `	SXUNUSED(zUser);` |
|     - |  370 | `	return -1;` |
|     - |  371 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     3 |  372 | `}` |
|     - |  373 | `/* int (*xChgrp)(const char *,const char *) */` |
|     6 |  374 | `static int UnixVfs_Chgrp(const char *zPath,const char *zGroup)` |
|     - |  375 | `{` |
|     6 |  376 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  377 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  378 | `  struct group *group;` |
|     - |  379 | `  gid_t gid;` |
|     - |  380 | `  int rc;` |
|     - |  381 | `  /* Numeric gid accepted too (see UnixVfs_Chown) */` |
|     6 |  382 | `  if( zGroup[0] >= '0' && zGroup[0] <= '9' ){` |
|     4 |  383 | `    gid = (gid_t)atoi(zGroup);` |
|     2 |  384 | `  }else{` |
|     2 |  385 | `    group = getgrnam(zGroup);` |
|     2 |  386 | `    if (group == 0) {` |
|     2 |  387 | `      return -2;   /* name lookup failed -- see UnixVfs_Chown */` |
|     - |  388 | `    }` |
|   ! 0 |  389 | `    gid = group->gr_gid;` |
|     - |  390 | `  }` |
|     4 |  391 | `  rc = chown(zPath,-1,gid);` |
|     4 |  392 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |  393 | `#else` |
|     - |  394 | `	SXUNUSED(zPath);` |
|     - |  395 | `	SXUNUSED(zGroup);` |
|     - |  396 | `	return -1;` |
|     - |  397 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     3 |  398 | `}` |
|     - |  399 | `/* int (*xIsfile)(const char *) */` |
|  6592 |  400 | `static int UnixVfs_isfile(const char *zPath)` |
|     - |  401 | `{` |
|  6592 |  402 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  403 | `	struct stat st;` |
|     - |  404 | `	int rc;` |
|  6592 |  405 | `	rc = stat(zPath,&st);` |
|  6592 |  406 | `	if( rc != 0 ){` |
|    54 |  407 | `	 return -1;` |
|     - |  408 | `	}` |
|  6538 |  409 | `	rc = S_ISREG(st.st_mode);` |
|  6538 |  410 | `	return rc ? PH7_OK : -1 ;` |
|  3296 |  411 | `}` |
|     - |  412 | `/* int (*xIslink)(const char *) */` |
|     4 |  413 | `static int UnixVfs_islink(const char *zPath)` |
|     - |  414 | `{` |
|     4 |  415 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  416 | `	struct stat st;` |
|     - |  417 | `	int rc;` |
|     4 |  418 | `	rc = stat(zPath,&st);` |
|     4 |  419 | `	if( rc != 0 ){` |
|   ! 0 |  420 | `	 return -1;` |
|     - |  421 | `	}` |
|     4 |  422 | `	rc = S_ISLNK(st.st_mode);` |
|     4 |  423 | `	return rc ? PH7_OK : -1 ;` |
|     2 |  424 | `}` |
|     - |  425 | `/* int (*xReadable)(const char *) */` |
|     2 |  426 | `static int UnixVfs_isreadable(const char *zPath)` |
|     - |  427 | `{` |
|     2 |  428 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  429 | `	int rc;` |
|     2 |  430 | `	rc = access(zPath,R_OK);` |
|     2 |  431 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  432 | `}` |
|     - |  433 | `/* int (*xWritable)(const char *) */` |
|     4 |  434 | `static int UnixVfs_iswritable(const char *zPath)` |
|     - |  435 | `{` |
|     4 |  436 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  437 | `	int rc;` |
|     4 |  438 | `	rc = access(zPath,W_OK);` |
|     4 |  439 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  440 | `}` |
|     - |  441 | `/* int (*xExecutable)(const char *) */` |
|     2 |  442 | `static int UnixVfs_isexecutable(const char *zPath)` |
|     - |  443 | `{` |
|     2 |  444 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  445 | `	int rc;` |
|     2 |  446 | `	rc = access(zPath,X_OK);` |
|     2 |  447 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  448 | `}` |
|     - |  449 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|     4 |  450 | `static int UnixVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|     - |  451 | `{` |
|     4 |  452 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  453 | `	struct stat st;` |
|     - |  454 | `	int rc;` |
|     4 |  455 | `    rc = stat(zPath,&st);` |
|     4 |  456 | `	if( rc != 0 ){` |
|     - |  457 | `	  /* Expand 'unknown' */` |
|   ! 0 |  458 | `	  ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|   ! 0 |  459 | `	  return -1;` |
|     - |  460 | `	}` |
|     4 |  461 | `	if(S_ISREG(st.st_mode) ){` |
|     2 |  462 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|     3 |  463 | `	}else if(S_ISDIR(st.st_mode)){` |
|     2 |  464 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|     1 |  465 | `	}else if(S_ISLNK(st.st_mode)){` |
|   ! 0 |  466 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|   ! 0 |  467 | `	}else if(S_ISBLK(st.st_mode)){` |
|   ! 0 |  468 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|   ! 0 |  469 | `    }else if(S_ISSOCK(st.st_mode)){` |
|   ! 0 |  470 | `		ph7_result_string(pCtx,"socket",sizeof("socket")-1);` |
|   ! 0 |  471 | `	}else if(S_ISFIFO(st.st_mode)){` |
|   ! 0 |  472 | `       ph7_result_string(pCtx,"fifo",sizeof("fifo")-1);` |
|   ! 0 |  473 | `	}else{` |
|   ! 0 |  474 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|     - |  475 | `	}` |
|     4 |  476 | `	return PH7_OK;` |
|     2 |  477 | `}` |
|     - |  478 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|    56 |  479 | `static int UnixVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|     - |  480 | `{` |
|     - |  481 | `	char *zEnv;` |
|    56 |  482 | `	zEnv = getenv(zVar);` |
|    56 |  483 | `	if( zEnv == 0 ){` |
|   ! 0 |  484 | `	  return -1;` |
|     - |  485 | `	}` |
|    56 |  486 | `	ph7_result_string(pCtx,zEnv,-1/*Compute length automatically*/);` |
|    56 |  487 | `	return PH7_OK;` |
|    28 |  488 | `}` |
|     - |  489 | `/* int (*xSetenv)(const char *,const char *) */` |
|     2 |  490 | `static int UnixVfs_Setenv(const char *zName,const char *zValue)` |
|     - |  491 | `{` |
|     - |  492 | `   int rc;` |
|     2 |  493 | `   rc = setenv(zName,zValue,1);` |
|     2 |  494 | `   return rc == 0 ? PH7_OK : -1;` |
|     - |  495 | `}` |
|     - |  496 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|  3856 |  497 | `static int UnixVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|     - |  498 | `{` |
|  3856 |  499 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  500 | `	struct stat st;` |
|     - |  501 | `	void *pMap;` |
|     - |  502 | `	int fd;` |
|     - |  503 | `	int rc;` |
|     - |  504 | `	/* Open the file in a read-only mode */` |
|  3856 |  505 | `	fd = open(zPath,O_RDONLY);` |
|  3856 |  506 | `	if( fd < 0 ){` |
|   ! 0 |  507 | `		return -1;` |
|     - |  508 | `	}` |
|     - |  509 | `	/* stat the handle */` |
|  3856 |  510 | `	fstat(fd,&st);` |
|     - |  511 | `	/* Obtain a memory view of the whole file */` |
|  3856 |  512 | `	pMap = mmap(0,st.st_size,PROT_READ,MAP_PRIVATE\|MAP_FILE,fd,0);` |
|  3856 |  513 | `	rc = PH7_OK;` |
|  3856 |  514 | `	if( pMap == MAP_FAILED ){` |
|   ! 0 |  515 | `		rc = -1;` |
|   ! 0 |  516 | `	}else{` |
|     - |  517 | `		/* Point to the memory view */` |
|  3856 |  518 | `		*ppMap = pMap;` |
|  3856 |  519 | `		*pSize = (ph7_int64)st.st_size;` |
|     - |  520 | `	}` |
|  3856 |  521 | `	close(fd);` |
|  3856 |  522 | `	return rc;` |
|  1928 |  523 | `}` |
|     - |  524 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|  3856 |  525 | `static void UnixVfs_Unmap(void *pView,ph7_int64 nSize)` |
|     - |  526 | `{` |
|  3856 |  527 | `	munmap(pView,(size_t)nSize);` |
|  3856 |  528 | `}` |
|     - |  529 | `/* void (*xTempDir)(ph7_context *) */` |
|   214 |  530 | `static void UnixVfs_TempDir(ph7_context *pCtx)` |
|     - |  531 | `{` |
|     - |  532 | `  const char *zDir;` |
|     - |  533 | `  /* php's php_get_temporary_directory: honour TMPDIR, else fall back to P_tmpdir` |
|     - |  534 | `   * ("/tmp" on Unix). PH7 also scanned /var/tmp, /usr/tmp, /usr/local/tmp first,` |
|     - |  535 | `   * which returned /var/tmp on a typical box where php returns /tmp — a divergence` |
|     - |  536 | `   * observable through sys_get_temp_dir()/tempnam()/session paths. */` |
|   214 |  537 | `  zDir = getenv("TMPDIR");` |
|   214 |  538 | `  if( zDir && zDir[0] != 0 && !access(zDir,07) ){` |
|     - |  539 | `	  /* php reports the temp dir WITHOUT a trailing separator; macOS's TMPDIR ends with` |
|     - |  540 | `	   * one, so returning it verbatim produced paths like "/var/.../T//file". */` |
|   107 |  541 | `	  int nDir = (int)strlen(zDir);` |
|   214 |  542 | `	  while( nDir > 1 && zDir[nDir-1] == '/' ){` |
|   107 |  543 | `		  nDir--;` |
|     - |  544 | `	  }` |
|   107 |  545 | `	  ph7_result_string(pCtx,zDir,nDir);` |
|   107 |  546 | `	  return;` |
|     - |  547 | `  }` |
|   107 |  548 | `  ph7_result_string(pCtx,"/tmp",(int)sizeof("/tmp")-1);` |
|   107 |  549 | `}` |
|     - |  550 | `/* unsigned int (*xProcessId)(void) */` |
|    84 |  551 | `static unsigned int UnixVfs_ProcessId(void)` |
|     - |  552 | `{` |
|    84 |  553 | `	return (unsigned int)getpid();` |
|     - |  554 | `}` |
|     - |  555 | `/* int (*xUid)(void) */` |
|     2 |  556 | `static int UnixVfs_uid(void)` |
|     - |  557 | `{` |
|     2 |  558 | `	return (int)getuid();` |
|     - |  559 | `}` |
|     - |  560 | `/* int (*xGid)(void) */` |
|     2 |  561 | `static int UnixVfs_gid(void)` |
|     - |  562 | `{` |
|     2 |  563 | `	return (int)getgid();` |
|     - |  564 | `}` |
|     - |  565 | `/* int (*xUmask)(int) */` |
|     8 |  566 | `static int UnixVfs_Umask(int new_mask)` |
|     - |  567 | `{` |
|     - |  568 | `	int old_mask;` |
|     8 |  569 | `	old_mask = umask(new_mask);` |
|     8 |  570 | `	return old_mask;` |
|     - |  571 | `}` |
|     - |  572 | `/* void (*xUsername)(ph7_context *) */` |
|     2 |  573 | `static void UnixVfs_Username(ph7_context *pCtx)` |
|     - |  574 | `{` |
|     - |  575 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  576 | `  struct passwd *pwd;` |
|     - |  577 | `  uid_t uid;` |
|     2 |  578 | `  uid = getuid();` |
|     2 |  579 | `  pwd = getpwuid(uid);   /* Try getting UID for username */` |
|     2 |  580 | `  if (pwd == 0) {` |
|   ! 0 |  581 | `    return;` |
|     - |  582 | `  }` |
|     - |  583 | `  /* Return the username */` |
|     2 |  584 | `  ph7_result_string(pCtx,pwd->pw_name,-1);` |
|     - |  585 | `#else` |
|     - |  586 | `  ph7_result_string(pCtx,"Unknown",-1);` |
|     - |  587 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     2 |  588 | `  return;` |
|     1 |  589 | `}` |
|     - |  590 | `/* int (*xLink)(const char *,const char *,int) */` |
|     8 |  591 | `static int UnixVfs_link(const char *zSrc,const char *zTarget,int is_sym)` |
|     - |  592 | `{` |
|     8 |  593 | `	zSrc = UnixVfsLocalPath(zSrc);` |
|     8 |  594 | `	zTarget = UnixVfsLocalPath(zTarget);` |
|     - |  595 | `	int rc;` |
|     8 |  596 | `	if( is_sym ){` |
|     - |  597 | `		/* Symbolic link */` |
|     6 |  598 | `		rc = symlink(zSrc,zTarget);` |
|     3 |  599 | `	}else{` |
|     - |  600 | `		/* Hard link */` |
|     2 |  601 | `		rc = link(zSrc,zTarget);` |
|     - |  602 | `	}` |
|     8 |  603 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  604 | `}` |
|     - |  605 | `/* int (*xChroot)(const char *) */` |
|     2 |  606 | `static int UnixVfs_chroot(const char *zRootDir)` |
|     - |  607 | `{` |
|     - |  608 | `	int rc;` |
|     2 |  609 | `	rc = chroot(zRootDir);` |
|     2 |  610 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  611 | `}` |
|     - |  612 | `/* Export the UNIX vfs */` |
|     - |  613 | `PH7_PRIVATE const ph7_vfs sUnixVfs = {` |
|     - |  614 | `	"Unix_vfs",` |
|     - |  615 | `	PH7_VFS_VERSION,` |
|     - |  616 | `	UnixVfs_chdir,    /* int (*xChdir)(const char *) */` |
|     - |  617 | `	UnixVfs_chroot,   /* int (*xChroot)(const char *); */` |
|     - |  618 | `	UnixVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|     - |  619 | `	UnixVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|     - |  620 | `	UnixVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|     - |  621 | `	UnixVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|     - |  622 | `	UnixVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|     - |  623 | `	UnixVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|     - |  624 | `	UnixVfs_Sleep,    /* int (*xSleep)(unsigned int) */` |
|     - |  625 | `	UnixVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|     - |  626 | `	UnixVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|     - |  627 | `	UnixVfs_Chmod, /*int (*xChmod)(const char *,int)*/` |
|     - |  628 | `	UnixVfs_Chown, /*int (*xChown)(const char *,const char *)*/` |
|     - |  629 | `	UnixVfs_Chgrp, /*int (*xChgrp)(const char *,const char *)*/` |
|     - |  630 | `	UnixVfs_FreeSpace,  /* ph7_int64 (*xFreeSpace)(const char *) */` |
|     - |  631 | `	UnixVfs_TotalSpace, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|     - |  632 | `	UnixVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|     - |  633 | `	UnixVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|     - |  634 | `	UnixVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|     - |  635 | `	UnixVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|     - |  636 | `	UnixVfs_Stat,  /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|     - |  637 | `	UnixVfs_lStat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|     - |  638 | `	UnixVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|     - |  639 | `	UnixVfs_islink,     /* int (*xIslink)(const char *) */` |
|     - |  640 | `	UnixVfs_isreadable, /* int (*xReadable)(const char *) */` |
|     - |  641 | `	UnixVfs_iswritable, /* int (*xWritable)(const char *) */` |
|     - |  642 | `	UnixVfs_isexecutable,/* int (*xExecutable)(const char *) */` |
|     - |  643 | `	UnixVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|     - |  644 | `	UnixVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|     - |  645 | `	UnixVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|     - |  646 | `	UnixVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|     - |  647 | `	UnixVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|     - |  648 | `	UnixVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|     - |  649 | `	UnixVfs_link,       /* int (*xLink)(const char *,const char *,int) */` |
|     - |  650 | `	UnixVfs_Umask,      /* int (*xUmask)(int) */` |
|     - |  651 | `	UnixVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|     - |  652 | `	UnixVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|     - |  653 | `	UnixVfs_uid, /* int (*xUid)(void) */` |
|     - |  654 | `	UnixVfs_gid, /* int (*xGid)(void) */` |
|     - |  655 | `	UnixVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|     - |  656 | `	0 /* int (*xExec)(const char *,ph7_context *) */` |
|     - |  657 | `};` |
|     - |  658 | `/* UNIX File IO */` |
|     - |  659 | `#define PH7_UNIX_OPEN_MODE	0640 /* Default open mode */` |
|     - |  660 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
| 29104 |  661 | `static int UnixFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|     - |  662 | `{` |
| 29104 |  663 | `	int iOpen = O_RDONLY;` |
|     - |  664 | `	int fd;` |
|     - |  665 | `	/* Set the desired flags according to the open mode */` |
| 29104 |  666 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|     - |  667 | `		/* Open existing file, or create if it doesn't exist */` |
| 13196 |  668 | `		iOpen = O_CREAT;` |
| 13196 |  669 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|     - |  670 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
| 13196 |  671 | `			iOpen \|= O_TRUNC;` |
|  6598 |  672 | `			SXUNUSED(pResource); /* cc warning */` |
|  6598 |  673 | `		}` |
| 22506 |  674 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|     - |  675 | `		/* Creates a new file, only if it does not already exist.` |
|     - |  676 | `		* If the file exists, it fails.` |
|     - |  677 | `		*/` |
|   126 |  678 | `		iOpen = O_CREAT\|O_EXCL;` |
| 15845 |  679 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|     - |  680 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|     - |  681 | `		 * The file must exist.` |
|     - |  682 | `		 */` |
|   ! 0 |  683 | `		iOpen = O_RDWR\|O_TRUNC;` |
|   ! 0 |  684 | `	}` |
| 29104 |  685 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|     - |  686 | `		/* Read+Write access */` |
| 13174 |  687 | `		iOpen &= ~O_RDONLY;` |
| 13174 |  688 | `		iOpen \|= O_RDWR;` |
| 22517 |  689 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|     - |  690 | `		/* Write only access */` |
|   150 |  691 | `		iOpen &= ~O_RDONLY;` |
|   150 |  692 | `		iOpen \|= O_WRONLY;` |
|    75 |  693 | `	}` |
| 29104 |  694 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|     - |  695 | `		/* Append mode */` |
|   ! 0 |  696 | `		iOpen \|= O_APPEND;` |
|   ! 0 |  697 | `	}` |
|     - |  698 | `#ifdef O_TEMP` |
|     - |  699 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|     - |  700 | `		/* File is temporary */` |
|     - |  701 | `		iOpen \|= O_TEMP;` |
|     - |  702 | `	}` |
|     - |  703 | `#endif` |
|     - |  704 | `	/* Open the file now */` |
| 29104 |  705 | `	fd = open(zPath,iOpen,PH7_UNIX_OPEN_MODE);` |
| 29104 |  706 | `	if( fd < 0 ){` |
|     - |  707 | `		/* IO error */` |
|    18 |  708 | `		return -1;` |
|     - |  709 | `	}` |
|     - |  710 | `	/* Save the handle */` |
| 29086 |  711 | `	*ppHandle = SX_INT_TO_PTR(fd);` |
| 29086 |  712 | `	return PH7_OK;` |
| 14552 |  713 | `}` |
|     - |  714 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|  1066 |  715 | `static int UnixDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|     - |  716 | `{` |
|     - |  717 | `	DIR *pDir;` |
|     - |  718 | `	/* Open the target directory */` |
|  1066 |  719 | `	pDir = opendir(zPath);` |
|  1066 |  720 | `	if( pDir == 0 ){` |
|   ! 0 |  721 | `		SXUNUSED(pResource); /* Compiler warning */` |
|   ! 0 |  722 | `		return -1;` |
|     - |  723 | `	}` |
|     - |  724 | `	/* Save our structure */` |
|  1066 |  725 | `	*ppHandle = pDir;` |
|  1066 |  726 | `	return PH7_OK;` |
|   533 |  727 | `}` |
|     - |  728 | `/* void (*xCloseDir)(void *) */` |
|  1066 |  729 | `static void UnixDir_Close(void *pUserData)` |
|     - |  730 | `{` |
|  1066 |  731 | `	closedir((DIR *)pUserData);` |
|  1066 |  732 | `}` |
|     - |  733 | `/* void (*xClose)(void *); */` |
| 29112 |  734 | `static void UnixFile_Close(void *pUserData)` |
|     - |  735 | `{` |
| 29112 |  736 | `	close(SX_PTR_TO_INT(pUserData));` |
| 29112 |  737 | `}` |
|     - |  738 | `/* int (*xReadDir)(void *,ph7_context *) */` |
| 10814 |  739 | `static int UnixDir_Read(void *pUserData,ph7_context *pCtx)` |
|     - |  740 | `{` |
| 10814 |  741 | `	DIR *pDir = (DIR *)pUserData;` |
|     - |  742 | `	struct dirent *pEntry;` |
|     - |  743 | `	char *zName;` |
|     - |  744 | `	sxu32 n;` |
|     - |  745 | `	/* php's readdir() yields every entry including '.' and '..' */` |
| 10814 |  746 | `	pEntry = readdir(pDir);` |
| 10814 |  747 | `	if( pEntry == 0 ){` |
|     - |  748 | `		/* No more entries to process */` |
|  1062 |  749 | `		return -1;` |
|     - |  750 | `	}` |
|  9752 |  751 | `	zName = pEntry->d_name;` |
|  9752 |  752 | `	n = SyStrlen(zName);` |
|     - |  753 | `	/* Return the current file name */` |
|  9752 |  754 | `	ph7_result_string(pCtx,zName,(int)n);` |
|  9752 |  755 | `	return PH7_OK;` |
|  5407 |  756 | `}` |
|     - |  757 | `/* void (*xRewindDir)(void *) */` |
|     2 |  758 | `static void UnixDir_Rewind(void *pUserData)` |
|     - |  759 | `{` |
|     2 |  760 | `	rewinddir((DIR *)pUserData);` |
|     2 |  761 | `}` |
|     - |  762 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
| 31494 |  763 | `static ph7_int64 UnixFile_Read(void *pUserData,void *pBuffer,ph7_int64 nDatatoRead)` |
|     - |  764 | `{` |
|     - |  765 | `	ssize_t nRd;` |
| 31494 |  766 | `	nRd = read(SX_PTR_TO_INT(pUserData),pBuffer,(size_t)nDatatoRead);` |
| 31494 |  767 | `	if( nRd < 1 ){` |
|     - |  768 | `		/* EOF or IO error */` |
| 15748 |  769 | `		return -1;` |
|     - |  770 | `	}` |
| 15746 |  771 | `	return (ph7_int64)nRd;` |
| 15747 |  772 | `}` |
|     - |  773 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
| 13216 |  774 | `static ph7_int64 UnixFile_Write(void *pUserData,const void *pBuffer,ph7_int64 nWrite)` |
|     - |  775 | `{` |
| 13216 |  776 | `	const char *zData = (const char *)pBuffer;` |
| 13216 |  777 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|     - |  778 | `	ph7_int64 nCount;` |
|     - |  779 | `	ssize_t nWr;` |
| 13216 |  780 | `	nCount = 0;` |
| 13216 |  781 | `	for(;;){` |
| 26432 |  782 | `		if( nWrite < 1 ){` |
| 13216 |  783 | `			break;` |
|     - |  784 | `		}` |
| 13216 |  785 | `		nWr = write(fd,zData,(size_t)nWrite);` |
| 13216 |  786 | `		if( nWr < 1 ){` |
|     - |  787 | `			/* IO error */` |
|   ! 0 |  788 | `			break;` |
|     - |  789 | `		}` |
| 13216 |  790 | `		nWrite -= nWr;` |
| 13216 |  791 | `		nCount += nWr;` |
| 13216 |  792 | `		zData += nWr;` |
|     - |  793 | `	}` |
| 13216 |  794 | `	if( nWrite > 0 ){` |
|   ! 0 |  795 | `		return -1;` |
|     - |  796 | `	}` |
| 13216 |  797 | `	return nCount;` |
|  6608 |  798 | `}` |
|     - |  799 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|     6 |  800 | `static int UnixFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|     - |  801 | `{` |
|     - |  802 | `	off_t iNew;` |
|     6 |  803 | `	switch(whence){` |
|   ! 0 |  804 | `	case 1:/*SEEK_CUR*/` |
|   ! 0 |  805 | `		whence = SEEK_CUR;` |
|   ! 0 |  806 | `		break;` |
|   ! 0 |  807 | `	case 2: /* SEEK_END */` |
|   ! 0 |  808 | `		whence = SEEK_END;` |
|   ! 0 |  809 | `		break;` |
|     6 |  810 | `	case 0: /* SEEK_SET */` |
|     - |  811 | `	default:` |
|     6 |  812 | `		whence = SEEK_SET;` |
|     6 |  813 | `		break;` |
|     - |  814 | `	}` |
|     6 |  815 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),(off_t)iOfft,whence);` |
|     6 |  816 | `	if( iNew < 0 ){` |
|   ! 0 |  817 | `		return -1;` |
|     - |  818 | `	}` |
|     6 |  819 | `	return PH7_OK;` |
|     3 |  820 | `}` |
|     - |  821 | `/* int (*xLock)(void *,int) */` |
|     4 |  822 | `static int UnixFile_Lock(void *pUserData,int lock_type)` |
|     - |  823 | `{` |
|     4 |  824 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|     4 |  825 | `	int rc = PH7_OK; /* cc warning */` |
|     4 |  826 | `	if( lock_type < 0 ){` |
|     - |  827 | `		/* Unlock the file */` |
|     2 |  828 | `		rc = flock(fd,LOCK_UN);` |
|     1 |  829 | `	}else{` |
|     2 |  830 | `		if( lock_type == 1 ){` |
|     - |  831 | `			/* Exculsive lock */` |
|     2 |  832 | `			rc = flock(fd,LOCK_EX);` |
|     1 |  833 | `		}else{` |
|     - |  834 | `			/* Shared lock */` |
|   ! 0 |  835 | `			rc = flock(fd,LOCK_SH);` |
|     - |  836 | `		}` |
|     - |  837 | `	}` |
|     4 |  838 | `	return !rc ? PH7_OK : -1;` |
|     - |  839 | `}` |
|     - |  840 | `/* ph7_int64 (*xTell)(void *) */` |
|     6 |  841 | `static ph7_int64 UnixFile_Tell(void *pUserData)` |
|     - |  842 | `{` |
|     - |  843 | `	off_t iNew;` |
|     6 |  844 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),0,SEEK_CUR);` |
|     6 |  845 | `	return (ph7_int64)iNew;` |
|     - |  846 | `}` |
|     - |  847 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|     2 |  848 | `static int UnixFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|     - |  849 | `{` |
|     - |  850 | `	int rc;` |
|     2 |  851 | `	rc = ftruncate(SX_PTR_TO_INT(pUserData),(off_t)nOfft);` |
|     2 |  852 | `	if( rc != 0 ){` |
|   ! 0 |  853 | `		return -1;` |
|     - |  854 | `	}` |
|     2 |  855 | `	return PH7_OK;` |
|     1 |  856 | `}` |
|     - |  857 | `/* int (*xSync)(void *); */` |
|     2 |  858 | `static int UnixFile_Sync(void *pUserData)` |
|     - |  859 | `{` |
|     - |  860 | `	int rc;` |
|     2 |  861 | `	rc = fsync(SX_PTR_TO_INT(pUserData));` |
|     2 |  862 | `	return rc == 0 ? PH7_OK : - 1;` |
|     - |  863 | `}` |
|     - |  864 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|     2 |  865 | `static int UnixFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  866 | `{` |
|     - |  867 | `	struct stat st;` |
|     - |  868 | `	int rc;` |
|     2 |  869 | `	rc = fstat(SX_PTR_TO_INT(pUserData),&st);` |
|     2 |  870 | `	if( rc != 0 ){` |
|   ! 0 |  871 | `	 return -1;` |
|     - |  872 | `	}` |
|     - |  873 | `	/* dev */` |
|     2 |  874 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|     2 |  875 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  876 | `	/* ino */` |
|     2 |  877 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|     2 |  878 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  879 | `	/* mode */` |
|     2 |  880 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|     2 |  881 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  882 | `	/* nlink */` |
|     2 |  883 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|     2 |  884 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  885 | `	/* uid,gid,rdev */` |
|     2 |  886 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|     2 |  887 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|     2 |  888 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|     2 |  889 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|     2 |  890 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|     2 |  891 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  892 | `	/* size */` |
|     2 |  893 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|     2 |  894 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  895 | `	/* atime */` |
|     2 |  896 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|     2 |  897 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  898 | `	/* mtime */` |
|     2 |  899 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|     2 |  900 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  901 | `	/* ctime */` |
|     2 |  902 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|     2 |  903 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  904 | `	/* blksize,blocks */` |
|     2 |  905 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|     2 |  906 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|     2 |  907 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|     2 |  908 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|     2 |  909 | `	return PH7_OK;` |
|     1 |  910 | `}` |
|     - |  911 | `/* Export the file:// stream */` |
|     - |  912 | `PH7_PRIVATE const ph7_io_stream sUnixFileStream = {` |
|     - |  913 | `	"file", /* Stream name */` |
|     - |  914 | `	PH7_IO_STREAM_VERSION,` |
|     - |  915 | `	UnixFile_Open,  /* xOpen */` |
|     - |  916 | `	UnixDir_Open,   /* xOpenDir */` |
|     - |  917 | `	UnixFile_Close, /* xClose */` |
|     - |  918 | `	UnixDir_Close,  /* xCloseDir */` |
|     - |  919 | `	UnixFile_Read,  /* xRead */` |
|     - |  920 | `	UnixDir_Read,   /* xReadDir */` |
|     - |  921 | `	UnixFile_Write, /* xWrite */` |
|     - |  922 | `	UnixFile_Seek,  /* xSeek */` |
|     - |  923 | `	UnixFile_Lock,  /* xLock */` |
|     - |  924 | `	UnixDir_Rewind, /* xRewindDir */` |
|     - |  925 | `	UnixFile_Tell,  /* xTell */` |
|     - |  926 | `	UnixFile_Trunc, /* xTrunc */` |
|     - |  927 | `	UnixFile_Sync,  /* xSeek */` |
|     - |  928 | `	UnixFile_Stat   /* xStat */` |
|     - |  929 | `};` |
|     - |  930 | `#endif /* __UNIXES__ */` |
|     - |  931 |  |
