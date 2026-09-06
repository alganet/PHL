# src/ph7/vfs_unix.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 384/437 lines (87.87%)

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
|     - |   14 | `#include <limits.h>` |
|     - |   15 | `#include <fcntl.h>` |
|     - |   16 | `#include <unistd.h>` |
|     - |   17 | `#include <sys/uio.h>` |
|     - |   18 | `#include <sys/stat.h>` |
|     - |   19 | `#include <sys/statvfs.h>` |
|     - |   20 | `#include <sys/mman.h>` |
|     - |   21 | `#include <sys/file.h>` |
|     - |   22 | `#include <sys/wait.h>` |
|     - |   23 | `#include <pwd.h>` |
|     - |   24 | `#include <grp.h>` |
|     - |   25 | `#include <dirent.h>` |
|     - |   26 | `#include <utime.h>` |
|     - |   27 | `#include <stdio.h>` |
|     - |   28 | `#include <stdlib.h>` |
|     - |   29 | `/* int (*xchdir)(const char *) */` |
| 13442 |   30 | `static int UnixVfs_chdir(const char *zPath)` |
|     - |   31 | `{` |
|     - |   32 | `  int rc;` |
| 13442 |   33 | `  rc = chdir(zPath);` |
| 13442 |   34 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |   35 | `}` |
|     - |   36 | `/* int (*xGetcwd)(ph7_context *) */` |
|    20 |   37 | `static int UnixVfs_getcwd(ph7_context *pCtx)` |
|     - |   38 | `{` |
|     - |   39 | `	char zBuf[4096];` |
|     - |   40 | `	char *zDir;` |
|     - |   41 | `	/* Get the current directory */` |
|    20 |   42 | `	zDir = getcwd(zBuf,sizeof(zBuf));` |
|    20 |   43 | `	if( zDir == 0 ){` |
|   ! 0 |   44 | `	  return -1;` |
|     - |   45 | `    }` |
|    20 |   46 | `	ph7_result_string(pCtx,zDir,-1/*Compute length automatically*/);` |
|    20 |   47 | `	return PH7_OK;` |
|    10 |   48 | `}` |
|     - |   49 | `/* int (*xMkdir)(const char *,int,int) */` |
|    34 |   50 | `static int UnixVfs_mkdir(const char *zPath,int mode,int recursive)` |
|     - |   51 | `{` |
|     - |   52 | `	int rc;` |
|    34 |   53 | `        rc = mkdir(zPath,mode);` |
|    17 |   54 | `	SXUNUSED(recursive); /* cc warning */` |
|    34 |   55 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   56 | `}` |
|     - |   57 | `/* int (*xRmdir)(const char *) */` |
|    34 |   58 | `static int UnixVfs_rmdir(const char *zPath)` |
|     - |   59 | `{` |
|     - |   60 | `	int rc;` |
|    34 |   61 | `	rc = rmdir(zPath);` |
|    34 |   62 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   63 | `}` |
|     - |   64 | `/* int (*xIsdir)(const char *) */` |
|  8508 |   65 | `static int UnixVfs_isdir(const char *zPath)` |
|     - |   66 | `{` |
|     - |   67 | `	struct stat st;` |
|     - |   68 | `	int rc;` |
|  8508 |   69 | `	rc = stat(zPath,&st);` |
|  8508 |   70 | `	if( rc != 0 ){` |
|     4 |   71 | `	 return -1;` |
|     - |   72 | `	}` |
|  8504 |   73 | `	rc = S_ISDIR(st.st_mode);` |
|  8504 |   74 | `	return rc ? PH7_OK : -1 ;` |
|  4254 |   75 | `}` |
|     - |   76 | `/* int (*xRename)(const char *,const char *) */` |
|     2 |   77 | `static int UnixVfs_Rename(const char *zOld,const char *zNew)` |
|     - |   78 | `{` |
|     - |   79 | `	int rc;` |
|     2 |   80 | `	rc = rename(zOld,zNew);` |
|     2 |   81 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   82 | `}` |
|     - |   83 | `/* int (*xRealpath)(const char *,ph7_context *) */` |
|     4 |   84 | `static int UnixVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|     - |   85 | `{` |
|     - |   86 | `#ifndef PH7_UNIX_OLD_LIBC` |
|     - |   87 | `	char *zReal;` |
|     4 |   88 | `	zReal = realpath(zPath,0);` |
|     4 |   89 | `	if( zReal == 0 ){` |
|     2 |   90 | `	  return -1;` |
|     - |   91 | `	}` |
|     2 |   92 | `	ph7_result_string(pCtx,zReal,-1/*Compute length automatically*/);` |
|     - |   93 | `        /* Release the allocated buffer */` |
|     2 |   94 | `	free(zReal);` |
|     2 |   95 | `	return PH7_OK;` |
|     - |   96 | `#else` |
|     - |   97 | `    zPath = 0; /* cc warning */` |
|     - |   98 | `    pCtx = 0;` |
|     - |   99 | `    return -1;` |
|     - |  100 | `#endif` |
|     2 |  101 | `}` |
|     - |  102 | `/* int (*xSleep)(unsigned int) */` |
|    60 |  103 | `static int UnixVfs_Sleep(unsigned int uSec)` |
|     - |  104 | `{` |
|    60 |  105 | `	usleep(uSec);` |
|    60 |  106 | `	return PH7_OK;` |
|     - |  107 | `}` |
|     - |  108 | `/* int (*xUnlink)(const char *) */` |
| 32734 |  109 | `static int UnixVfs_unlink(const char *zPath)` |
|     - |  110 | `{` |
|     - |  111 | `	int rc;` |
| 32734 |  112 | `	rc = unlink(zPath);` |
| 32734 |  113 | `	return rc == 0 ? PH7_OK : -1 ;` |
|     - |  114 | `}` |
|     - |  115 | `/* int (*xFileExists)(const char *) */` |
|    52 |  116 | `static int UnixVfs_FileExists(const char *zPath)` |
|     - |  117 | `{` |
|     - |  118 | `	int rc;` |
|    52 |  119 | `	rc = access(zPath,F_OK);` |
|    52 |  120 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  121 | `}` |
|     - |  122 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|     - |  123 | `/* ph7_int64 (*xFreeSpace)(const char *) */` |
|     4 |  124 | `static ph7_int64 UnixVfs_FreeSpace(const char *zPath)` |
|     - |  125 | `{` |
|     - |  126 | `	struct statvfs sInfo;` |
|     4 |  127 | `	if( statvfs(zPath,&sInfo) != 0 ){` |
|   ! 0 |  128 | `		return -1;` |
|     - |  129 | `	}` |
|     - |  130 | `	/* php reports the space available to an UNPRIVILEGED user (f_bavail) */` |
|     4 |  131 | `	return (ph7_int64)sInfo.f_bavail * (ph7_int64)sInfo.f_frsize;` |
|     2 |  132 | `}` |
|     - |  133 | `/* ph7_int64 (*xTotalSpace)(const char *) */` |
|     4 |  134 | `static ph7_int64 UnixVfs_TotalSpace(const char *zPath)` |
|     - |  135 | `{` |
|     - |  136 | `	struct statvfs sInfo;` |
|     4 |  137 | `	if( statvfs(zPath,&sInfo) != 0 ){` |
|   ! 0 |  138 | `		return -1;` |
|     - |  139 | `	}` |
|     4 |  140 | `	return (ph7_int64)sInfo.f_blocks * (ph7_int64)sInfo.f_frsize;` |
|     2 |  141 | `}` |
|    26 |  142 | `static ph7_int64 UnixVfs_FileSize(const char *zPath)` |
|     - |  143 | `{` |
|     - |  144 | `	struct stat st;` |
|     - |  145 | `	int rc;` |
|    26 |  146 | `	rc = stat(zPath,&st);` |
|    26 |  147 | `	if( rc != 0 ){` |
|   ! 0 |  148 | `	 return -1;` |
|     - |  149 | `	}` |
|    26 |  150 | `	return (ph7_int64)st.st_size;` |
|    13 |  151 | `}` |
|     - |  152 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|     4 |  153 | `static int UnixVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|     - |  154 | `{` |
|     - |  155 | `	struct utimbuf ut;` |
|     - |  156 | `	int rc;` |
|     4 |  157 | `	ut.actime  = (time_t)access_time;` |
|     4 |  158 | `	ut.modtime = (time_t)touch_time;` |
|     4 |  159 | `	rc = utime(zPath,&ut);` |
|     4 |  160 | `	if( rc != 0 ){` |
|   ! 0 |  161 | `	 return -1;` |
|     - |  162 | `	}` |
|     4 |  163 | `	return PH7_OK;` |
|     2 |  164 | `}` |
|     - |  165 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|     2 |  166 | `static ph7_int64 UnixVfs_FileAtime(const char *zPath)` |
|     - |  167 | `{` |
|     - |  168 | `	struct stat st;` |
|     - |  169 | `	int rc;` |
|     2 |  170 | `	rc = stat(zPath,&st);` |
|     2 |  171 | `	if( rc != 0 ){` |
|   ! 0 |  172 | `	 return -1;` |
|     - |  173 | `	}` |
|     2 |  174 | `	return (ph7_int64)st.st_atime;` |
|     1 |  175 | `}` |
|     - |  176 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|     4 |  177 | `static ph7_int64 UnixVfs_FileMtime(const char *zPath)` |
|     - |  178 | `{` |
|     - |  179 | `	struct stat st;` |
|     - |  180 | `	int rc;` |
|     4 |  181 | `	rc = stat(zPath,&st);` |
|     4 |  182 | `	if( rc != 0 ){` |
|   ! 0 |  183 | `	 return -1;` |
|     - |  184 | `	}` |
|     4 |  185 | `	return (ph7_int64)st.st_mtime;` |
|     2 |  186 | `}` |
|     - |  187 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|     2 |  188 | `static ph7_int64 UnixVfs_FileCtime(const char *zPath)` |
|     - |  189 | `{` |
|     - |  190 | `	struct stat st;` |
|     - |  191 | `	int rc;` |
|     2 |  192 | `	rc = stat(zPath,&st);` |
|     2 |  193 | `	if( rc != 0 ){` |
|   ! 0 |  194 | `	 return -1;` |
|     - |  195 | `	}` |
|     2 |  196 | `	return (ph7_int64)st.st_ctime;` |
|     1 |  197 | `}` |
|     - |  198 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|     4 |  199 | `static int UnixVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  200 | `{` |
|     - |  201 | `	struct stat st;` |
|     - |  202 | `	int rc;` |
|     4 |  203 | `	rc = stat(zPath,&st);` |
|     4 |  204 | `	if( rc != 0 ){` |
|   ! 0 |  205 | `	 return -1;` |
|     - |  206 | `	}` |
|     - |  207 | `	/* dev */` |
|     4 |  208 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|     4 |  209 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  210 | `	/* ino */` |
|     4 |  211 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|     4 |  212 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  213 | `	/* mode */` |
|     4 |  214 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|     4 |  215 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  216 | `	/* nlink */` |
|     4 |  217 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|     4 |  218 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  219 | `	/* uid,gid,rdev */` |
|     4 |  220 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|     4 |  221 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|     4 |  222 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|     4 |  223 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|     4 |  224 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|     4 |  225 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  226 | `	/* size */` |
|     4 |  227 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|     4 |  228 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  229 | `	/* atime */` |
|     4 |  230 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|     4 |  231 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  232 | `	/* mtime */` |
|     4 |  233 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|     4 |  234 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  235 | `	/* ctime */` |
|     4 |  236 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|     4 |  237 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  238 | `	/* blksize,blocks */` |
|     4 |  239 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|     4 |  240 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|     4 |  241 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|     4 |  242 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|     4 |  243 | `	return PH7_OK;` |
|     2 |  244 | `}` |
|     - |  245 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|     2 |  246 | `static int UnixVfs_lStat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  247 | `{` |
|     - |  248 | `	struct stat st;` |
|     - |  249 | `	int rc;` |
|     2 |  250 | `	rc = lstat(zPath,&st);` |
|     2 |  251 | `	if( rc != 0 ){` |
|   ! 0 |  252 | `	 return -1;` |
|     - |  253 | `	}` |
|     - |  254 | `	/* dev */` |
|     2 |  255 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|     2 |  256 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  257 | `	/* ino */` |
|     2 |  258 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|     2 |  259 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  260 | `	/* mode */` |
|     2 |  261 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|     2 |  262 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  263 | `	/* nlink */` |
|     2 |  264 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|     2 |  265 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  266 | `	/* uid,gid,rdev */` |
|     2 |  267 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|     2 |  268 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|     2 |  269 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|     2 |  270 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|     2 |  271 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|     2 |  272 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  273 | `	/* size */` |
|     2 |  274 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|     2 |  275 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  276 | `	/* atime */` |
|     2 |  277 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|     2 |  278 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  279 | `	/* mtime */` |
|     2 |  280 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|     2 |  281 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  282 | `	/* ctime */` |
|     2 |  283 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|     2 |  284 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  285 | `	/* blksize,blocks */` |
|     2 |  286 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|     2 |  287 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|     2 |  288 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|     2 |  289 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|     2 |  290 | `	return PH7_OK;` |
|     1 |  291 | `}` |
|     - |  292 | `/* int (*xChmod)(const char *,int) */` |
|    10 |  293 | `static int UnixVfs_Chmod(const char *zPath,int mode)` |
|     - |  294 | `{` |
|     - |  295 | `    int rc;` |
|    10 |  296 | `    rc = chmod(zPath,(mode_t)mode);` |
|    10 |  297 | `    return rc == 0 ? PH7_OK : - 1;` |
|     - |  298 | `}` |
|     - |  299 | `/* int (*xChown)(const char *,const char *) */` |
|     4 |  300 | `static int UnixVfs_Chown(const char *zPath,const char *zUser)` |
|     - |  301 | `{` |
|     - |  302 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  303 | `  struct passwd *pwd;` |
|     - |  304 | `  uid_t uid;` |
|     - |  305 | `  int rc;` |
|     4 |  306 | `  pwd = getpwnam(zUser);   /* Try getting UID for username */` |
|     4 |  307 | `  if (pwd == 0) {` |
|     4 |  308 | `    return -1;` |
|     - |  309 | `  }` |
|   ! 0 |  310 | `  uid = pwd->pw_uid;` |
|   ! 0 |  311 | `  rc = chown(zPath,uid,-1);` |
|   ! 0 |  312 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |  313 | `#else` |
|     - |  314 | `	SXUNUSED(zPath);` |
|     - |  315 | `	SXUNUSED(zUser);` |
|     - |  316 | `	return -1;` |
|     - |  317 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     2 |  318 | `}` |
|     - |  319 | `/* int (*xChgrp)(const char *,const char *) */` |
|     4 |  320 | `static int UnixVfs_Chgrp(const char *zPath,const char *zGroup)` |
|     - |  321 | `{` |
|     - |  322 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  323 | `  struct group *group;` |
|     - |  324 | `  gid_t gid;` |
|     - |  325 | `  int rc;` |
|     4 |  326 | `  group = getgrnam(zGroup);` |
|     4 |  327 | `  if (group == 0) {` |
|     4 |  328 | `    return -1;` |
|     - |  329 | `  }` |
|   ! 0 |  330 | `  gid = group->gr_gid;` |
|   ! 0 |  331 | `  rc = chown(zPath,-1,gid);` |
|   ! 0 |  332 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |  333 | `#else` |
|     - |  334 | `	SXUNUSED(zPath);` |
|     - |  335 | `	SXUNUSED(zGroup);` |
|     - |  336 | `	return -1;` |
|     - |  337 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     2 |  338 | `}` |
|     - |  339 | `/* int (*xIsfile)(const char *) */` |
|  6518 |  340 | `static int UnixVfs_isfile(const char *zPath)` |
|     - |  341 | `{` |
|     - |  342 | `	struct stat st;` |
|     - |  343 | `	int rc;` |
|  6518 |  344 | `	rc = stat(zPath,&st);` |
|  6518 |  345 | `	if( rc != 0 ){` |
|     2 |  346 | `	 return -1;` |
|     - |  347 | `	}` |
|  6516 |  348 | `	rc = S_ISREG(st.st_mode);` |
|  6516 |  349 | `	return rc ? PH7_OK : -1 ;` |
|  3259 |  350 | `}` |
|     - |  351 | `/* int (*xIslink)(const char *) */` |
|     4 |  352 | `static int UnixVfs_islink(const char *zPath)` |
|     - |  353 | `{` |
|     - |  354 | `	struct stat st;` |
|     - |  355 | `	int rc;` |
|     4 |  356 | `	rc = stat(zPath,&st);` |
|     4 |  357 | `	if( rc != 0 ){` |
|   ! 0 |  358 | `	 return -1;` |
|     - |  359 | `	}` |
|     4 |  360 | `	rc = S_ISLNK(st.st_mode);` |
|     4 |  361 | `	return rc ? PH7_OK : -1 ;` |
|     2 |  362 | `}` |
|     - |  363 | `/* int (*xReadable)(const char *) */` |
|     2 |  364 | `static int UnixVfs_isreadable(const char *zPath)` |
|     - |  365 | `{` |
|     - |  366 | `	int rc;` |
|     2 |  367 | `	rc = access(zPath,R_OK);` |
|     2 |  368 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  369 | `}` |
|     - |  370 | `/* int (*xWritable)(const char *) */` |
|     4 |  371 | `static int UnixVfs_iswritable(const char *zPath)` |
|     - |  372 | `{` |
|     - |  373 | `	int rc;` |
|     4 |  374 | `	rc = access(zPath,W_OK);` |
|     4 |  375 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  376 | `}` |
|     - |  377 | `/* int (*xExecutable)(const char *) */` |
|     2 |  378 | `static int UnixVfs_isexecutable(const char *zPath)` |
|     - |  379 | `{` |
|     - |  380 | `	int rc;` |
|     2 |  381 | `	rc = access(zPath,X_OK);` |
|     2 |  382 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  383 | `}` |
|     - |  384 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|     4 |  385 | `static int UnixVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|     - |  386 | `{` |
|     - |  387 | `	struct stat st;` |
|     - |  388 | `	int rc;` |
|     4 |  389 | `    rc = stat(zPath,&st);` |
|     4 |  390 | `	if( rc != 0 ){` |
|     - |  391 | `	  /* Expand 'unknown' */` |
|   ! 0 |  392 | `	  ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|   ! 0 |  393 | `	  return -1;` |
|     - |  394 | `	}` |
|     4 |  395 | `	if(S_ISREG(st.st_mode) ){` |
|     2 |  396 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|     3 |  397 | `	}else if(S_ISDIR(st.st_mode)){` |
|     2 |  398 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|     1 |  399 | `	}else if(S_ISLNK(st.st_mode)){` |
|   ! 0 |  400 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|   ! 0 |  401 | `	}else if(S_ISBLK(st.st_mode)){` |
|   ! 0 |  402 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|   ! 0 |  403 | `    }else if(S_ISSOCK(st.st_mode)){` |
|   ! 0 |  404 | `		ph7_result_string(pCtx,"socket",sizeof("socket")-1);` |
|   ! 0 |  405 | `	}else if(S_ISFIFO(st.st_mode)){` |
|   ! 0 |  406 | `       ph7_result_string(pCtx,"fifo",sizeof("fifo")-1);` |
|   ! 0 |  407 | `	}else{` |
|   ! 0 |  408 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|     - |  409 | `	}` |
|     4 |  410 | `	return PH7_OK;` |
|     2 |  411 | `}` |
|     - |  412 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|    52 |  413 | `static int UnixVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|     - |  414 | `{` |
|     - |  415 | `	char *zEnv;` |
|    52 |  416 | `	zEnv = getenv(zVar);` |
|    52 |  417 | `	if( zEnv == 0 ){` |
|   ! 0 |  418 | `	  return -1;` |
|     - |  419 | `	}` |
|    52 |  420 | `	ph7_result_string(pCtx,zEnv,-1/*Compute length automatically*/);` |
|    52 |  421 | `	return PH7_OK;` |
|    26 |  422 | `}` |
|     - |  423 | `/* int (*xSetenv)(const char *,const char *) */` |
|     2 |  424 | `static int UnixVfs_Setenv(const char *zName,const char *zValue)` |
|     - |  425 | `{` |
|     - |  426 | `   int rc;` |
|     2 |  427 | `   rc = setenv(zName,zValue,1);` |
|     2 |  428 | `   return rc == 0 ? PH7_OK : -1;` |
|     - |  429 | `}` |
|     - |  430 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|  3956 |  431 | `static int UnixVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|     - |  432 | `{` |
|     - |  433 | `	struct stat st;` |
|     - |  434 | `	void *pMap;` |
|     - |  435 | `	int fd;` |
|     - |  436 | `	int rc;` |
|     - |  437 | `	/* Open the file in a read-only mode */` |
|  3956 |  438 | `	fd = open(zPath,O_RDONLY);` |
|  3956 |  439 | `	if( fd < 0 ){` |
|     2 |  440 | `		return -1;` |
|     - |  441 | `	}` |
|     - |  442 | `	/* stat the handle */` |
|  3954 |  443 | `	fstat(fd,&st);` |
|     - |  444 | `	/* Obtain a memory view of the whole file */` |
|  3954 |  445 | `	pMap = mmap(0,st.st_size,PROT_READ,MAP_PRIVATE\|MAP_FILE,fd,0);` |
|  3954 |  446 | `	rc = PH7_OK;` |
|  3954 |  447 | `	if( pMap == MAP_FAILED ){` |
|   ! 0 |  448 | `		rc = -1;` |
|   ! 0 |  449 | `	}else{` |
|     - |  450 | `		/* Point to the memory view */` |
|  3954 |  451 | `		*ppMap = pMap;` |
|  3954 |  452 | `		*pSize = (ph7_int64)st.st_size;` |
|     - |  453 | `	}` |
|  3954 |  454 | `	close(fd);` |
|  3954 |  455 | `	return rc;` |
|  1978 |  456 | `}` |
|     - |  457 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|  3954 |  458 | `static void UnixVfs_Unmap(void *pView,ph7_int64 nSize)` |
|     - |  459 | `{` |
|  3954 |  460 | `	munmap(pView,(size_t)nSize);` |
|  3954 |  461 | `}` |
|     - |  462 | `/* void (*xTempDir)(ph7_context *) */` |
|   218 |  463 | `static void UnixVfs_TempDir(ph7_context *pCtx)` |
|     - |  464 | `{` |
|     - |  465 | `	static const char *azDirs[] = {` |
|     - |  466 | `     "/var/tmp",` |
|     - |  467 | `     "/usr/tmp",` |
|     - |  468 | `	 "/usr/local/tmp"` |
|     - |  469 | `  };` |
|     - |  470 | `  unsigned int i;` |
|     - |  471 | `  struct stat buf;` |
|     - |  472 | `  const char *zDir;` |
|   218 |  473 | `  zDir = getenv("TMPDIR");` |
|   218 |  474 | `  if( zDir && zDir[0] != 0 && !access(zDir,07) ){` |
|   109 |  475 | `	  ph7_result_string(pCtx,zDir,-1);` |
|   109 |  476 | `	  return;` |
|     - |  477 | `  }` |
|   109 |  478 | `  for(i=0; i<sizeof(azDirs)/sizeof(azDirs[0]); i++){` |
|   109 |  479 | `	zDir=azDirs[i];` |
|   109 |  480 | `    if( zDir==0 ) continue;` |
|   109 |  481 | `    if( stat(zDir, &buf) ) continue;` |
|   109 |  482 | `    if( !S_ISDIR(buf.st_mode) ) continue;` |
|   109 |  483 | `    if( access(zDir, 07) ) continue;` |
|     - |  484 | `    /* Got one */` |
|   109 |  485 | `	ph7_result_string(pCtx,zDir,-1);` |
|   109 |  486 | `	return;` |
|     - |  487 | `  }` |
|     - |  488 | `  /* Default temp dir */` |
|   ! 0 |  489 | `  ph7_result_string(pCtx,"/tmp",(int)sizeof("/tmp")-1);` |
|   109 |  490 | `}` |
|     - |  491 | `/* unsigned int (*xProcessId)(void) */` |
|    74 |  492 | `static unsigned int UnixVfs_ProcessId(void)` |
|     - |  493 | `{` |
|    74 |  494 | `	return (unsigned int)getpid();` |
|     - |  495 | `}` |
|     - |  496 | `/* int (*xUid)(void) */` |
|     2 |  497 | `static int UnixVfs_uid(void)` |
|     - |  498 | `{` |
|     2 |  499 | `	return (int)getuid();` |
|     - |  500 | `}` |
|     - |  501 | `/* int (*xGid)(void) */` |
|     2 |  502 | `static int UnixVfs_gid(void)` |
|     - |  503 | `{` |
|     2 |  504 | `	return (int)getgid();` |
|     - |  505 | `}` |
|     - |  506 | `/* int (*xUmask)(int) */` |
|     8 |  507 | `static int UnixVfs_Umask(int new_mask)` |
|     - |  508 | `{` |
|     - |  509 | `	int old_mask;` |
|     8 |  510 | `	old_mask = umask(new_mask);` |
|     8 |  511 | `	return old_mask;` |
|     - |  512 | `}` |
|     - |  513 | `/* void (*xUsername)(ph7_context *) */` |
|     2 |  514 | `static void UnixVfs_Username(ph7_context *pCtx)` |
|     - |  515 | `{` |
|     - |  516 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  517 | `  struct passwd *pwd;` |
|     - |  518 | `  uid_t uid;` |
|     2 |  519 | `  uid = getuid();` |
|     2 |  520 | `  pwd = getpwuid(uid);   /* Try getting UID for username */` |
|     2 |  521 | `  if (pwd == 0) {` |
|   ! 0 |  522 | `    return;` |
|     - |  523 | `  }` |
|     - |  524 | `  /* Return the username */` |
|     2 |  525 | `  ph7_result_string(pCtx,pwd->pw_name,-1);` |
|     - |  526 | `#else` |
|     - |  527 | `  ph7_result_string(pCtx,"Unknown",-1);` |
|     - |  528 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     2 |  529 | `  return;` |
|     1 |  530 | `}` |
|     - |  531 | `/* int (*xLink)(const char *,const char *,int) */` |
|     8 |  532 | `static int UnixVfs_link(const char *zSrc,const char *zTarget,int is_sym)` |
|     - |  533 | `{` |
|     - |  534 | `	int rc;` |
|     8 |  535 | `	if( is_sym ){` |
|     - |  536 | `		/* Symbolic link */` |
|     6 |  537 | `		rc = symlink(zSrc,zTarget);` |
|     3 |  538 | `	}else{` |
|     - |  539 | `		/* Hard link */` |
|     2 |  540 | `		rc = link(zSrc,zTarget);` |
|     - |  541 | `	}` |
|     8 |  542 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  543 | `}` |
|     - |  544 | `/* int (*xChroot)(const char *) */` |
|     2 |  545 | `static int UnixVfs_chroot(const char *zRootDir)` |
|     - |  546 | `{` |
|     - |  547 | `	int rc;` |
|     2 |  548 | `	rc = chroot(zRootDir);` |
|     2 |  549 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  550 | `}` |
|     - |  551 | `/* Export the UNIX vfs */` |
|     - |  552 | `PH7_PRIVATE const ph7_vfs sUnixVfs = {` |
|     - |  553 | `	"Unix_vfs",` |
|     - |  554 | `	PH7_VFS_VERSION,` |
|     - |  555 | `	UnixVfs_chdir,    /* int (*xChdir)(const char *) */` |
|     - |  556 | `	UnixVfs_chroot,   /* int (*xChroot)(const char *); */` |
|     - |  557 | `	UnixVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|     - |  558 | `	UnixVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|     - |  559 | `	UnixVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|     - |  560 | `	UnixVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|     - |  561 | `	UnixVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|     - |  562 | `	UnixVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|     - |  563 | `	UnixVfs_Sleep,    /* int (*xSleep)(unsigned int) */` |
|     - |  564 | `	UnixVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|     - |  565 | `	UnixVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|     - |  566 | `	UnixVfs_Chmod, /*int (*xChmod)(const char *,int)*/` |
|     - |  567 | `	UnixVfs_Chown, /*int (*xChown)(const char *,const char *)*/` |
|     - |  568 | `	UnixVfs_Chgrp, /*int (*xChgrp)(const char *,const char *)*/` |
|     - |  569 | `	UnixVfs_FreeSpace,  /* ph7_int64 (*xFreeSpace)(const char *) */` |
|     - |  570 | `	UnixVfs_TotalSpace, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|     - |  571 | `	UnixVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|     - |  572 | `	UnixVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|     - |  573 | `	UnixVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|     - |  574 | `	UnixVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|     - |  575 | `	UnixVfs_Stat,  /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|     - |  576 | `	UnixVfs_lStat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|     - |  577 | `	UnixVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|     - |  578 | `	UnixVfs_islink,     /* int (*xIslink)(const char *) */` |
|     - |  579 | `	UnixVfs_isreadable, /* int (*xReadable)(const char *) */` |
|     - |  580 | `	UnixVfs_iswritable, /* int (*xWritable)(const char *) */` |
|     - |  581 | `	UnixVfs_isexecutable,/* int (*xExecutable)(const char *) */` |
|     - |  582 | `	UnixVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|     - |  583 | `	UnixVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|     - |  584 | `	UnixVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|     - |  585 | `	UnixVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|     - |  586 | `	UnixVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|     - |  587 | `	UnixVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|     - |  588 | `	UnixVfs_link,       /* int (*xLink)(const char *,const char *,int) */` |
|     - |  589 | `	UnixVfs_Umask,      /* int (*xUmask)(int) */` |
|     - |  590 | `	UnixVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|     - |  591 | `	UnixVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|     - |  592 | `	UnixVfs_uid, /* int (*xUid)(void) */` |
|     - |  593 | `	UnixVfs_gid, /* int (*xGid)(void) */` |
|     - |  594 | `	UnixVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|     - |  595 | `	0 /* int (*xExec)(const char *,ph7_context *) */` |
|     - |  596 | `};` |
|     - |  597 | `/* UNIX File IO */` |
|     - |  598 | `#define PH7_UNIX_OPEN_MODE	0640 /* Default open mode */` |
|     - |  599 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
| 29908 |  600 | `static int UnixFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|     - |  601 | `{` |
| 29908 |  602 | `	int iOpen = O_RDONLY;` |
|     - |  603 | `	int fd;` |
|     - |  604 | `	/* Set the desired flags according to the open mode */` |
| 29908 |  605 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|     - |  606 | `		/* Open existing file, or create if it doesn't exist */` |
| 13718 |  607 | `		iOpen = O_CREAT;` |
| 13718 |  608 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|     - |  609 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
| 13718 |  610 | `			iOpen \|= O_TRUNC;` |
|  6859 |  611 | `			SXUNUSED(pResource); /* cc warning */` |
|  6859 |  612 | `		}` |
| 23049 |  613 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|     - |  614 | `		/* Creates a new file, only if it does not already exist.` |
|     - |  615 | `		* If the file exists, it fails.` |
|     - |  616 | `		*/` |
|   ! 0 |  617 | `		iOpen = O_CREAT\|O_EXCL;` |
| 16190 |  618 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|     - |  619 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|     - |  620 | `		 * The file must exist.` |
|     - |  621 | `		 */` |
|   ! 0 |  622 | `		iOpen = O_RDWR\|O_TRUNC;` |
|   ! 0 |  623 | `	}` |
| 29908 |  624 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|     - |  625 | `		/* Read+Write access */` |
| 13702 |  626 | `		iOpen &= ~O_RDONLY;` |
| 13702 |  627 | `		iOpen \|= O_RDWR;` |
| 23057 |  628 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|     - |  629 | `		/* Write only access */` |
|    22 |  630 | `		iOpen &= ~O_RDONLY;` |
|    22 |  631 | `		iOpen \|= O_WRONLY;` |
|    11 |  632 | `	}` |
| 29908 |  633 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|     - |  634 | `		/* Append mode */` |
|   ! 0 |  635 | `		iOpen \|= O_APPEND;` |
|   ! 0 |  636 | `	}` |
|     - |  637 | `#ifdef O_TEMP` |
|     - |  638 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|     - |  639 | `		/* File is temporary */` |
|     - |  640 | `		iOpen \|= O_TEMP;` |
|     - |  641 | `	}` |
|     - |  642 | `#endif` |
|     - |  643 | `	/* Open the file now */` |
| 29908 |  644 | `	fd = open(zPath,iOpen,PH7_UNIX_OPEN_MODE);` |
| 29908 |  645 | `	if( fd < 0 ){` |
|     - |  646 | `		/* IO error */` |
|    12 |  647 | `		return -1;` |
|     - |  648 | `	}` |
|     - |  649 | `	/* Save the handle */` |
| 29896 |  650 | `	*ppHandle = SX_INT_TO_PTR(fd);` |
| 29896 |  651 | `	return PH7_OK;` |
| 14954 |  652 | `}` |
|     - |  653 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|  1000 |  654 | `static int UnixDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|     - |  655 | `{` |
|     - |  656 | `	DIR *pDir;` |
|     - |  657 | `	/* Open the target directory */` |
|  1000 |  658 | `	pDir = opendir(zPath);` |
|  1000 |  659 | `	if( pDir == 0 ){` |
|   ! 0 |  660 | `		SXUNUSED(pResource); /* Compiler warning */` |
|   ! 0 |  661 | `		return -1;` |
|     - |  662 | `	}` |
|     - |  663 | `	/* Save our structure */` |
|  1000 |  664 | `	*ppHandle = pDir;` |
|  1000 |  665 | `	return PH7_OK;` |
|   500 |  666 | `}` |
|     - |  667 | `/* void (*xCloseDir)(void *) */` |
|  1000 |  668 | `static void UnixDir_Close(void *pUserData)` |
|     - |  669 | `{` |
|  1000 |  670 | `	closedir((DIR *)pUserData);` |
|  1000 |  671 | `}` |
|     - |  672 | `/* void (*xClose)(void *); */` |
| 29894 |  673 | `static void UnixFile_Close(void *pUserData)` |
|     - |  674 | `{` |
| 29894 |  675 | `	close(SX_PTR_TO_INT(pUserData));` |
| 29894 |  676 | `}` |
|     - |  677 | `/* int (*xReadDir)(void *,ph7_context *) */` |
|  8506 |  678 | `static int UnixDir_Read(void *pUserData,ph7_context *pCtx)` |
|     - |  679 | `{` |
|  8506 |  680 | `	DIR *pDir = (DIR *)pUserData;` |
|     - |  681 | `	struct dirent *pEntry;` |
|  8506 |  682 | `	char *zName = 0; /* cc warning */` |
|  8506 |  683 | `	sxu32 n = 0;` |
|  5253 |  684 | `	for(;;){` |
| 10506 |  685 | `		pEntry = readdir(pDir);` |
| 10506 |  686 | `		if( pEntry == 0 ){` |
|     - |  687 | `			/* No more entries to process */` |
|   996 |  688 | `			return -1;` |
|     - |  689 | `		}` |
|  9510 |  690 | `		zName = pEntry->d_name;` |
|  9510 |  691 | `		n = SyStrlen(zName);` |
|     - |  692 | `		/* Ignore '.' && '..' */` |
|  9510 |  693 | `		if( n > sizeof("..")-1 \|\| zName[0] != '.' \|\| ( n == sizeof("..")-1 && zName[1] != '.') ){` |
|  3755 |  694 | `			break;` |
|     - |  695 | `		}` |
|     - |  696 | `		/* Next entry */` |
|     - |  697 | `	}` |
|     - |  698 | `	/* Return the current file name */` |
|  7510 |  699 | `	ph7_result_string(pCtx,zName,(int)n);` |
|  7510 |  700 | `	return PH7_OK;` |
|  4253 |  701 | `}` |
|     - |  702 | `/* void (*xRewindDir)(void *) */` |
|     2 |  703 | `static void UnixDir_Rewind(void *pUserData)` |
|     - |  704 | `{` |
|     2 |  705 | `	rewinddir((DIR *)pUserData);` |
|     2 |  706 | `}` |
|     - |  707 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
| 32296 |  708 | `static ph7_int64 UnixFile_Read(void *pUserData,void *pBuffer,ph7_int64 nDatatoRead)` |
|     - |  709 | `{` |
|     - |  710 | `	ssize_t nRd;` |
| 32296 |  711 | `	nRd = read(SX_PTR_TO_INT(pUserData),pBuffer,(size_t)nDatatoRead);` |
| 32296 |  712 | `	if( nRd < 1 ){` |
|     - |  713 | `		/* EOF or IO error */` |
| 16144 |  714 | `		return -1;` |
|     - |  715 | `	}` |
| 16152 |  716 | `	return (ph7_int64)nRd;` |
| 16148 |  717 | `}` |
|     - |  718 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
| 13732 |  719 | `static ph7_int64 UnixFile_Write(void *pUserData,const void *pBuffer,ph7_int64 nWrite)` |
|     - |  720 | `{` |
| 13732 |  721 | `	const char *zData = (const char *)pBuffer;` |
| 13732 |  722 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|     - |  723 | `	ph7_int64 nCount;` |
|     - |  724 | `	ssize_t nWr;` |
| 13732 |  725 | `	nCount = 0;` |
| 13732 |  726 | `	for(;;){` |
| 27464 |  727 | `		if( nWrite < 1 ){` |
| 13732 |  728 | `			break;` |
|     - |  729 | `		}` |
| 13732 |  730 | `		nWr = write(fd,zData,(size_t)nWrite);` |
| 13732 |  731 | `		if( nWr < 1 ){` |
|     - |  732 | `			/* IO error */` |
|   ! 0 |  733 | `			break;` |
|     - |  734 | `		}` |
| 13732 |  735 | `		nWrite -= nWr;` |
| 13732 |  736 | `		nCount += nWr;` |
| 13732 |  737 | `		zData += nWr;` |
|     - |  738 | `	}` |
| 13732 |  739 | `	if( nWrite > 0 ){` |
|   ! 0 |  740 | `		return -1;` |
|     - |  741 | `	}` |
| 13732 |  742 | `	return nCount;` |
|  6866 |  743 | `}` |
|     - |  744 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|     6 |  745 | `static int UnixFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|     - |  746 | `{` |
|     - |  747 | `	off_t iNew;` |
|     6 |  748 | `	switch(whence){` |
|   ! 0 |  749 | `	case 1:/*SEEK_CUR*/` |
|   ! 0 |  750 | `		whence = SEEK_CUR;` |
|   ! 0 |  751 | `		break;` |
|   ! 0 |  752 | `	case 2: /* SEEK_END */` |
|   ! 0 |  753 | `		whence = SEEK_END;` |
|   ! 0 |  754 | `		break;` |
|     6 |  755 | `	case 0: /* SEEK_SET */` |
|     - |  756 | `	default:` |
|     6 |  757 | `		whence = SEEK_SET;` |
|     6 |  758 | `		break;` |
|     - |  759 | `	}` |
|     6 |  760 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),(off_t)iOfft,whence);` |
|     6 |  761 | `	if( iNew < 0 ){` |
|   ! 0 |  762 | `		return -1;` |
|     - |  763 | `	}` |
|     6 |  764 | `	return PH7_OK;` |
|     3 |  765 | `}` |
|     - |  766 | `/* int (*xLock)(void *,int) */` |
|     4 |  767 | `static int UnixFile_Lock(void *pUserData,int lock_type)` |
|     - |  768 | `{` |
|     4 |  769 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|     4 |  770 | `	int rc = PH7_OK; /* cc warning */` |
|     4 |  771 | `	if( lock_type < 0 ){` |
|     - |  772 | `		/* Unlock the file */` |
|   ! 0 |  773 | `		rc = flock(fd,LOCK_UN);` |
|   ! 0 |  774 | `	}else{` |
|     4 |  775 | `		if( lock_type == 1 ){` |
|     - |  776 | `			/* Exculsive lock */` |
|     2 |  777 | `			rc = flock(fd,LOCK_EX);` |
|     1 |  778 | `		}else{` |
|     - |  779 | `			/* Shared lock */` |
|     2 |  780 | `			rc = flock(fd,LOCK_SH);` |
|     - |  781 | `		}` |
|     - |  782 | `	}` |
|     4 |  783 | `	return !rc ? PH7_OK : -1;` |
|     - |  784 | `}` |
|     - |  785 | `/* ph7_int64 (*xTell)(void *) */` |
|     6 |  786 | `static ph7_int64 UnixFile_Tell(void *pUserData)` |
|     - |  787 | `{` |
|     - |  788 | `	off_t iNew;` |
|     6 |  789 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),0,SEEK_CUR);` |
|     6 |  790 | `	return (ph7_int64)iNew;` |
|     - |  791 | `}` |
|     - |  792 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|     6 |  793 | `static int UnixFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|     - |  794 | `{` |
|     - |  795 | `	int rc;` |
|     6 |  796 | `	rc = ftruncate(SX_PTR_TO_INT(pUserData),(off_t)nOfft);` |
|     6 |  797 | `	if( rc != 0 ){` |
|   ! 0 |  798 | `		return -1;` |
|     - |  799 | `	}` |
|     6 |  800 | `	return PH7_OK;` |
|     3 |  801 | `}` |
|     - |  802 | `/* int (*xSync)(void *); */` |
|     2 |  803 | `static int UnixFile_Sync(void *pUserData)` |
|     - |  804 | `{` |
|     - |  805 | `	int rc;` |
|     2 |  806 | `	rc = fsync(SX_PTR_TO_INT(pUserData));` |
|     2 |  807 | `	return rc == 0 ? PH7_OK : - 1;` |
|     - |  808 | `}` |
|     - |  809 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|     2 |  810 | `static int UnixFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  811 | `{` |
|     - |  812 | `	struct stat st;` |
|     - |  813 | `	int rc;` |
|     2 |  814 | `	rc = fstat(SX_PTR_TO_INT(pUserData),&st);` |
|     2 |  815 | `	if( rc != 0 ){` |
|   ! 0 |  816 | `	 return -1;` |
|     - |  817 | `	}` |
|     - |  818 | `	/* dev */` |
|     2 |  819 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|     2 |  820 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  821 | `	/* ino */` |
|     2 |  822 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|     2 |  823 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  824 | `	/* mode */` |
|     2 |  825 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|     2 |  826 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  827 | `	/* nlink */` |
|     2 |  828 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|     2 |  829 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  830 | `	/* uid,gid,rdev */` |
|     2 |  831 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|     2 |  832 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|     2 |  833 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|     2 |  834 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|     2 |  835 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|     2 |  836 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  837 | `	/* size */` |
|     2 |  838 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|     2 |  839 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  840 | `	/* atime */` |
|     2 |  841 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|     2 |  842 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  843 | `	/* mtime */` |
|     2 |  844 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|     2 |  845 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  846 | `	/* ctime */` |
|     2 |  847 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|     2 |  848 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  849 | `	/* blksize,blocks */` |
|     2 |  850 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|     2 |  851 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|     2 |  852 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|     2 |  853 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|     2 |  854 | `	return PH7_OK;` |
|     1 |  855 | `}` |
|     - |  856 | `/* Export the file:// stream */` |
|     - |  857 | `PH7_PRIVATE const ph7_io_stream sUnixFileStream = {` |
|     - |  858 | `	"file", /* Stream name */` |
|     - |  859 | `	PH7_IO_STREAM_VERSION,` |
|     - |  860 | `	UnixFile_Open,  /* xOpen */` |
|     - |  861 | `	UnixDir_Open,   /* xOpenDir */` |
|     - |  862 | `	UnixFile_Close, /* xClose */` |
|     - |  863 | `	UnixDir_Close,  /* xCloseDir */` |
|     - |  864 | `	UnixFile_Read,  /* xRead */` |
|     - |  865 | `	UnixDir_Read,   /* xReadDir */` |
|     - |  866 | `	UnixFile_Write, /* xWrite */` |
|     - |  867 | `	UnixFile_Seek,  /* xSeek */` |
|     - |  868 | `	UnixFile_Lock,  /* xLock */` |
|     - |  869 | `	UnixDir_Rewind, /* xRewindDir */` |
|     - |  870 | `	UnixFile_Tell,  /* xTell */` |
|     - |  871 | `	UnixFile_Trunc, /* xTrunc */` |
|     - |  872 | `	UnixFile_Sync,  /* xSeek */` |
|     - |  873 | `	UnixFile_Stat   /* xStat */` |
|     - |  874 | `};` |
|     - |  875 | `#endif /* __UNIXES__ */` |
|     - |  876 |  |
