# src/ph7/vfs_unix.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 376/427 lines (88.06%)

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
|     - |   19 | `#include <sys/mman.h>` |
|     - |   20 | `#include <sys/file.h>` |
|     - |   21 | `#include <sys/wait.h>` |
|     - |   22 | `#include <pwd.h>` |
|     - |   23 | `#include <grp.h>` |
|     - |   24 | `#include <dirent.h>` |
|     - |   25 | `#include <utime.h>` |
|     - |   26 | `#include <stdio.h>` |
|     - |   27 | `#include <stdlib.h>` |
|     - |   28 | `/* int (*xchdir)(const char *) */` |
| 12156 |   29 | `static int UnixVfs_chdir(const char *zPath)` |
|     - |   30 |  |
|     - |   31 | `  int rc;` |
| 12156 |   32 | `  rc = chdir(zPath);` |
| 12156 |   33 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |   34 |  |
|     - |   35 | `/* int (*xGetcwd)(ph7_context *) */` |
|    20 |   36 | `static int UnixVfs_getcwd(ph7_context *pCtx)` |
|     - |   37 |  |
|     - |   38 | `	char zBuf[4096];` |
|     - |   39 | `	char *zDir;` |
|     - |   40 | `	/* Get the current directory */` |
|    20 |   41 | `	zDir = getcwd(zBuf,sizeof(zBuf));` |
|    20 |   42 | `	if( zDir == 0 ){` |
|   ! 0 |   43 | `	  return -1;` |
|     - |   44 | `    }` |
|    20 |   45 | `	ph7_result_string(pCtx,zDir,-1/*Compute length automatically*/);` |
|    20 |   46 | `	return PH7_OK;` |
|    10 |   47 |  |
|     - |   48 | `/* int (*xMkdir)(const char *,int,int) */` |
|    24 |   49 | `static int UnixVfs_mkdir(const char *zPath,int mode,int recursive)` |
|     - |   50 |  |
|     - |   51 | `	int rc;` |
|    24 |   52 | `        rc = mkdir(zPath,mode);` |
|    12 |   53 | `	SXUNUSED(recursive); /* cc warning */` |
|    24 |   54 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   55 |  |
|     - |   56 | `/* int (*xRmdir)(const char *) */` |
|    26 |   57 | `static int UnixVfs_rmdir(const char *zPath)` |
|     - |   58 |  |
|     - |   59 | `	int rc;` |
|    26 |   60 | `	rc = rmdir(zPath);` |
|    26 |   61 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   62 |  |
|     - |   63 | `/* int (*xIsdir)(const char *) */` |
|  7146 |   64 | `static int UnixVfs_isdir(const char *zPath)` |
|     - |   65 |  |
|     - |   66 | `	struct stat st;` |
|     - |   67 | `	int rc;` |
|  7146 |   68 | `	rc = stat(zPath,&st);` |
|  7146 |   69 | `	if( rc != 0 ){` |
|     4 |   70 | `	 return -1;` |
|     - |   71 | `	}` |
|  7142 |   72 | `	rc = S_ISDIR(st.st_mode);` |
|  7142 |   73 | `	return rc ? PH7_OK : -1 ;` |
|  3573 |   74 |  |
|     - |   75 | `/* int (*xRename)(const char *,const char *) */` |
|     2 |   76 | `static int UnixVfs_Rename(const char *zOld,const char *zNew)` |
|     - |   77 |  |
|     - |   78 | `	int rc;` |
|     2 |   79 | `	rc = rename(zOld,zNew);` |
|     2 |   80 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   81 |  |
|     - |   82 | `/* int (*xRealpath)(const char *,ph7_context *) */` |
|     4 |   83 | `static int UnixVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|     - |   84 |  |
|     - |   85 | `#ifndef PH7_UNIX_OLD_LIBC` |
|     - |   86 | `	char *zReal;` |
|     4 |   87 | `	zReal = realpath(zPath,0);` |
|     4 |   88 | `	if( zReal == 0 ){` |
|     2 |   89 | `	  return -1;` |
|     - |   90 | `	}` |
|     2 |   91 | `	ph7_result_string(pCtx,zReal,-1/*Compute length automatically*/);` |
|     - |   92 | `        /* Release the allocated buffer */` |
|     2 |   93 | `	free(zReal);` |
|     2 |   94 | `	return PH7_OK;` |
|     - |   95 | `#else` |
|     - |   96 | `    zPath = 0; /* cc warning */` |
|     - |   97 | `    pCtx = 0;` |
|     - |   98 | `    return -1;` |
|     - |   99 | `#endif` |
|     2 |  100 |  |
|     - |  101 | `/* int (*xSleep)(unsigned int) */` |
|    40 |  102 | `static int UnixVfs_Sleep(unsigned int uSec)` |
|     - |  103 |  |
|    40 |  104 | `	usleep(uSec);` |
|    40 |  105 | `	return PH7_OK;` |
|     - |  106 |  |
|     - |  107 | `/* int (*xUnlink)(const char *) */` |
| 26758 |  108 | `static int UnixVfs_unlink(const char *zPath)` |
|     - |  109 |  |
|     - |  110 | `	int rc;` |
| 26758 |  111 | `	rc = unlink(zPath);` |
| 26758 |  112 | `	return rc == 0 ? PH7_OK : -1 ;` |
|     - |  113 |  |
|     - |  114 | `/* int (*xFileExists)(const char *) */` |
|    44 |  115 | `static int UnixVfs_FileExists(const char *zPath)` |
|     - |  116 |  |
|     - |  117 | `	int rc;` |
|    44 |  118 | `	rc = access(zPath,F_OK);` |
|    44 |  119 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  120 |  |
|     - |  121 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|    26 |  122 | `static ph7_int64 UnixVfs_FileSize(const char *zPath)` |
|     - |  123 |  |
|     - |  124 | `	struct stat st;` |
|     - |  125 | `	int rc;` |
|    26 |  126 | `	rc = stat(zPath,&st);` |
|    26 |  127 | `	if( rc != 0 ){` |
|   ! 0 |  128 | `	 return -1;` |
|     - |  129 | `	}` |
|    26 |  130 | `	return (ph7_int64)st.st_size;` |
|    13 |  131 |  |
|     - |  132 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|     4 |  133 | `static int UnixVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|     - |  134 |  |
|     - |  135 | `	struct utimbuf ut;` |
|     - |  136 | `	int rc;` |
|     4 |  137 | `	ut.actime  = (time_t)access_time;` |
|     4 |  138 | `	ut.modtime = (time_t)touch_time;` |
|     4 |  139 | `	rc = utime(zPath,&ut);` |
|     4 |  140 | `	if( rc != 0 ){` |
|   ! 0 |  141 | `	 return -1;` |
|     - |  142 | `	}` |
|     4 |  143 | `	return PH7_OK;` |
|     2 |  144 |  |
|     - |  145 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|     2 |  146 | `static ph7_int64 UnixVfs_FileAtime(const char *zPath)` |
|     - |  147 |  |
|     - |  148 | `	struct stat st;` |
|     - |  149 | `	int rc;` |
|     2 |  150 | `	rc = stat(zPath,&st);` |
|     2 |  151 | `	if( rc != 0 ){` |
|   ! 0 |  152 | `	 return -1;` |
|     - |  153 | `	}` |
|     2 |  154 | `	return (ph7_int64)st.st_atime;` |
|     1 |  155 |  |
|     - |  156 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|     4 |  157 | `static ph7_int64 UnixVfs_FileMtime(const char *zPath)` |
|     - |  158 |  |
|     - |  159 | `	struct stat st;` |
|     - |  160 | `	int rc;` |
|     4 |  161 | `	rc = stat(zPath,&st);` |
|     4 |  162 | `	if( rc != 0 ){` |
|   ! 0 |  163 | `	 return -1;` |
|     - |  164 | `	}` |
|     4 |  165 | `	return (ph7_int64)st.st_mtime;` |
|     2 |  166 |  |
|     - |  167 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|     2 |  168 | `static ph7_int64 UnixVfs_FileCtime(const char *zPath)` |
|     - |  169 |  |
|     - |  170 | `	struct stat st;` |
|     - |  171 | `	int rc;` |
|     2 |  172 | `	rc = stat(zPath,&st);` |
|     2 |  173 | `	if( rc != 0 ){` |
|   ! 0 |  174 | `	 return -1;` |
|     - |  175 | `	}` |
|     2 |  176 | `	return (ph7_int64)st.st_ctime;` |
|     1 |  177 |  |
|     - |  178 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|     4 |  179 | `static int UnixVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  180 |  |
|     - |  181 | `	struct stat st;` |
|     - |  182 | `	int rc;` |
|     4 |  183 | `	rc = stat(zPath,&st);` |
|     4 |  184 | `	if( rc != 0 ){` |
|   ! 0 |  185 | `	 return -1;` |
|     - |  186 | `	}` |
|     - |  187 | `	/* dev */` |
|     4 |  188 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|     4 |  189 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  190 | `	/* ino */` |
|     4 |  191 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|     4 |  192 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  193 | `	/* mode */` |
|     4 |  194 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|     4 |  195 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  196 | `	/* nlink */` |
|     4 |  197 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|     4 |  198 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  199 | `	/* uid,gid,rdev */` |
|     4 |  200 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|     4 |  201 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|     4 |  202 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|     4 |  203 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|     4 |  204 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|     4 |  205 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  206 | `	/* size */` |
|     4 |  207 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|     4 |  208 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  209 | `	/* atime */` |
|     4 |  210 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|     4 |  211 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  212 | `	/* mtime */` |
|     4 |  213 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|     4 |  214 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  215 | `	/* ctime */` |
|     4 |  216 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|     4 |  217 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  218 | `	/* blksize,blocks */` |
|     4 |  219 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|     4 |  220 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|     4 |  221 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|     4 |  222 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|     4 |  223 | `	return PH7_OK;` |
|     2 |  224 |  |
|     - |  225 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|     2 |  226 | `static int UnixVfs_lStat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  227 |  |
|     - |  228 | `	struct stat st;` |
|     - |  229 | `	int rc;` |
|     2 |  230 | `	rc = lstat(zPath,&st);` |
|     2 |  231 | `	if( rc != 0 ){` |
|   ! 0 |  232 | `	 return -1;` |
|     - |  233 | `	}` |
|     - |  234 | `	/* dev */` |
|     2 |  235 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|     2 |  236 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  237 | `	/* ino */` |
|     2 |  238 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|     2 |  239 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  240 | `	/* mode */` |
|     2 |  241 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|     2 |  242 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  243 | `	/* nlink */` |
|     2 |  244 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|     2 |  245 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  246 | `	/* uid,gid,rdev */` |
|     2 |  247 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|     2 |  248 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|     2 |  249 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|     2 |  250 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|     2 |  251 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|     2 |  252 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  253 | `	/* size */` |
|     2 |  254 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|     2 |  255 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  256 | `	/* atime */` |
|     2 |  257 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|     2 |  258 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  259 | `	/* mtime */` |
|     2 |  260 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|     2 |  261 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  262 | `	/* ctime */` |
|     2 |  263 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|     2 |  264 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  265 | `	/* blksize,blocks */` |
|     2 |  266 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|     2 |  267 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|     2 |  268 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|     2 |  269 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|     2 |  270 | `	return PH7_OK;` |
|     1 |  271 |  |
|     - |  272 | `/* int (*xChmod)(const char *,int) */` |
|    10 |  273 | `static int UnixVfs_Chmod(const char *zPath,int mode)` |
|     - |  274 |  |
|     - |  275 | `    int rc;` |
|    10 |  276 | `    rc = chmod(zPath,(mode_t)mode);` |
|    10 |  277 | `    return rc == 0 ? PH7_OK : - 1;` |
|     - |  278 |  |
|     - |  279 | `/* int (*xChown)(const char *,const char *) */` |
|     4 |  280 | `static int UnixVfs_Chown(const char *zPath,const char *zUser)` |
|     - |  281 |  |
|     - |  282 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  283 | `  struct passwd *pwd;` |
|     - |  284 | `  uid_t uid;` |
|     - |  285 | `  int rc;` |
|     4 |  286 | `  pwd = getpwnam(zUser);   /* Try getting UID for username */` |
|     4 |  287 | `  if (pwd == 0) {` |
|     4 |  288 | `    return -1;` |
|     - |  289 | `  }` |
|   ! 0 |  290 | `  uid = pwd->pw_uid;` |
|   ! 0 |  291 | `  rc = chown(zPath,uid,-1);` |
|   ! 0 |  292 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |  293 | `#else` |
|     - |  294 | `	SXUNUSED(zPath);` |
|     - |  295 | `	SXUNUSED(zUser);` |
|     - |  296 | `	return -1;` |
|     - |  297 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     2 |  298 |  |
|     - |  299 | `/* int (*xChgrp)(const char *,const char *) */` |
|     4 |  300 | `static int UnixVfs_Chgrp(const char *zPath,const char *zGroup)` |
|     - |  301 |  |
|     - |  302 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  303 | `  struct group *group;` |
|     - |  304 | `  gid_t gid;` |
|     - |  305 | `  int rc;` |
|     4 |  306 | `  group = getgrnam(zGroup);` |
|     4 |  307 | `  if (group == 0) {` |
|     4 |  308 | `    return -1;` |
|     - |  309 | `  }` |
|   ! 0 |  310 | `  gid = group->gr_gid;` |
|   ! 0 |  311 | `  rc = chown(zPath,-1,gid);` |
|   ! 0 |  312 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |  313 | `#else` |
|     - |  314 | `	SXUNUSED(zPath);` |
|     - |  315 | `	SXUNUSED(zGroup);` |
|     - |  316 | `	return -1;` |
|     - |  317 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     2 |  318 |  |
|     - |  319 | `/* int (*xIsfile)(const char *) */` |
|  5326 |  320 | `static int UnixVfs_isfile(const char *zPath)` |
|     - |  321 |  |
|     - |  322 | `	struct stat st;` |
|     - |  323 | `	int rc;` |
|  5326 |  324 | `	rc = stat(zPath,&st);` |
|  5326 |  325 | `	if( rc != 0 ){` |
|     2 |  326 | `	 return -1;` |
|     - |  327 | `	}` |
|  5324 |  328 | `	rc = S_ISREG(st.st_mode);` |
|  5324 |  329 | `	return rc ? PH7_OK : -1 ;` |
|  2663 |  330 |  |
|     - |  331 | `/* int (*xIslink)(const char *) */` |
|     4 |  332 | `static int UnixVfs_islink(const char *zPath)` |
|     - |  333 |  |
|     - |  334 | `	struct stat st;` |
|     - |  335 | `	int rc;` |
|     4 |  336 | `	rc = stat(zPath,&st);` |
|     4 |  337 | `	if( rc != 0 ){` |
|   ! 0 |  338 | `	 return -1;` |
|     - |  339 | `	}` |
|     4 |  340 | `	rc = S_ISLNK(st.st_mode);` |
|     4 |  341 | `	return rc ? PH7_OK : -1 ;` |
|     2 |  342 |  |
|     - |  343 | `/* int (*xReadable)(const char *) */` |
|     2 |  344 | `static int UnixVfs_isreadable(const char *zPath)` |
|     - |  345 |  |
|     - |  346 | `	int rc;` |
|     2 |  347 | `	rc = access(zPath,R_OK);` |
|     2 |  348 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  349 |  |
|     - |  350 | `/* int (*xWritable)(const char *) */` |
|     4 |  351 | `static int UnixVfs_iswritable(const char *zPath)` |
|     - |  352 |  |
|     - |  353 | `	int rc;` |
|     4 |  354 | `	rc = access(zPath,W_OK);` |
|     4 |  355 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  356 |  |
|     - |  357 | `/* int (*xExecutable)(const char *) */` |
|     2 |  358 | `static int UnixVfs_isexecutable(const char *zPath)` |
|     - |  359 |  |
|     - |  360 | `	int rc;` |
|     2 |  361 | `	rc = access(zPath,X_OK);` |
|     2 |  362 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  363 |  |
|     - |  364 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|     4 |  365 | `static int UnixVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|     - |  366 |  |
|     - |  367 | `	struct stat st;` |
|     - |  368 | `	int rc;` |
|     4 |  369 | `    rc = stat(zPath,&st);` |
|     4 |  370 | `	if( rc != 0 ){` |
|     - |  371 | `	  /* Expand 'unknown' */` |
|   ! 0 |  372 | `	  ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|   ! 0 |  373 | `	  return -1;` |
|     - |  374 | `	}` |
|     4 |  375 | `	if(S_ISREG(st.st_mode) ){` |
|     2 |  376 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|     3 |  377 | `	}else if(S_ISDIR(st.st_mode)){` |
|     2 |  378 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|     1 |  379 | `	}else if(S_ISLNK(st.st_mode)){` |
|   ! 0 |  380 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|   ! 0 |  381 | `	}else if(S_ISBLK(st.st_mode)){` |
|   ! 0 |  382 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|   ! 0 |  383 | `    }else if(S_ISSOCK(st.st_mode)){` |
|   ! 0 |  384 | `		ph7_result_string(pCtx,"socket",sizeof("socket")-1);` |
|   ! 0 |  385 | `	}else if(S_ISFIFO(st.st_mode)){` |
|   ! 0 |  386 | `       ph7_result_string(pCtx,"fifo",sizeof("fifo")-1);` |
|   ! 0 |  387 | `	}else{` |
|   ! 0 |  388 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|     - |  389 | `	}` |
|     4 |  390 | `	return PH7_OK;` |
|     2 |  391 |  |
|     - |  392 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|    34 |  393 | `static int UnixVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|     - |  394 |  |
|     - |  395 | `	char *zEnv;` |
|    34 |  396 | `	zEnv = getenv(zVar);` |
|    34 |  397 | `	if( zEnv == 0 ){` |
|   ! 0 |  398 | `	  return -1;` |
|     - |  399 | `	}` |
|    34 |  400 | `	ph7_result_string(pCtx,zEnv,-1/*Compute length automatically*/);` |
|    34 |  401 | `	return PH7_OK;` |
|    17 |  402 |  |
|     - |  403 | `/* int (*xSetenv)(const char *,const char *) */` |
|     2 |  404 | `static int UnixVfs_Setenv(const char *zName,const char *zValue)` |
|     - |  405 |  |
|     - |  406 | `   int rc;` |
|     2 |  407 | `   rc = setenv(zName,zValue,1);` |
|     2 |  408 | `   return rc == 0 ? PH7_OK : -1;` |
|     - |  409 |  |
|     - |  410 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|  2994 |  411 | `static int UnixVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|     - |  412 |  |
|     - |  413 | `	struct stat st;` |
|     - |  414 | `	void *pMap;` |
|     - |  415 | `	int fd;` |
|     - |  416 | `	int rc;` |
|     - |  417 | `	/* Open the file in a read-only mode */` |
|  2994 |  418 | `	fd = open(zPath,O_RDONLY);` |
|  2994 |  419 | `	if( fd < 0 ){` |
|     2 |  420 | `		return -1;` |
|     - |  421 | `	}` |
|     - |  422 | `	/* stat the handle */` |
|  2992 |  423 | `	fstat(fd,&st);` |
|     - |  424 | `	/* Obtain a memory view of the whole file */` |
|  2992 |  425 | `	pMap = mmap(0,st.st_size,PROT_READ,MAP_PRIVATE\|MAP_FILE,fd,0);` |
|  2992 |  426 | `	rc = PH7_OK;` |
|  2992 |  427 | `	if( pMap == MAP_FAILED ){` |
|   ! 0 |  428 | `		rc = -1;` |
|   ! 0 |  429 | `	}else{` |
|     - |  430 | `		/* Point to the memory view */` |
|  2992 |  431 | `		*ppMap = pMap;` |
|  2992 |  432 | `		*pSize = (ph7_int64)st.st_size;` |
|     - |  433 | `	}` |
|  2992 |  434 | `	close(fd);` |
|  2992 |  435 | `	return rc;` |
|  1497 |  436 |  |
|     - |  437 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|  2992 |  438 | `static void UnixVfs_Unmap(void *pView,ph7_int64 nSize)` |
|     - |  439 |  |
|  2992 |  440 | `	munmap(pView,(size_t)nSize);` |
|  2992 |  441 |  |
|     - |  442 | `/* void (*xTempDir)(ph7_context *) */` |
|   184 |  443 | `static void UnixVfs_TempDir(ph7_context *pCtx)` |
|     - |  444 |  |
|     - |  445 | `	static const char *azDirs[] = {` |
|     - |  446 | `     "/var/tmp",` |
|     - |  447 | `     "/usr/tmp",` |
|     - |  448 | `	 "/usr/local/tmp"` |
|     - |  449 | `  };` |
|     - |  450 | `  unsigned int i;` |
|     - |  451 | `  struct stat buf;` |
|     - |  452 | `  const char *zDir;` |
|   184 |  453 | `  zDir = getenv("TMPDIR");` |
|   184 |  454 | `  if( zDir && zDir[0] != 0 && !access(zDir,07) ){` |
|    92 |  455 | `	  ph7_result_string(pCtx,zDir,-1);` |
|    92 |  456 | `	  return;` |
|     - |  457 | `  }` |
|    92 |  458 | `  for(i=0; i<sizeof(azDirs)/sizeof(azDirs[0]); i++){` |
|    92 |  459 | `	zDir=azDirs[i];` |
|    92 |  460 | `    if( zDir==0 ) continue;` |
|    92 |  461 | `    if( stat(zDir, &buf) ) continue;` |
|    92 |  462 | `    if( !S_ISDIR(buf.st_mode) ) continue;` |
|    92 |  463 | `    if( access(zDir, 07) ) continue;` |
|     - |  464 | `    /* Got one */` |
|    92 |  465 | `	ph7_result_string(pCtx,zDir,-1);` |
|    92 |  466 | `	return;` |
|     - |  467 | `  }` |
|     - |  468 | `  /* Default temp dir */` |
|   ! 0 |  469 | `  ph7_result_string(pCtx,"/tmp",(int)sizeof("/tmp")-1);` |
|    92 |  470 |  |
|     - |  471 | `/* unsigned int (*xProcessId)(void) */` |
|    40 |  472 | `static unsigned int UnixVfs_ProcessId(void)` |
|     - |  473 |  |
|    40 |  474 | `	return (unsigned int)getpid();` |
|     - |  475 |  |
|     - |  476 | `/* int (*xUid)(void) */` |
|     2 |  477 | `static int UnixVfs_uid(void)` |
|     - |  478 |  |
|     2 |  479 | `	return (int)getuid();` |
|     - |  480 |  |
|     - |  481 | `/* int (*xGid)(void) */` |
|     2 |  482 | `static int UnixVfs_gid(void)` |
|     - |  483 |  |
|     2 |  484 | `	return (int)getgid();` |
|     - |  485 |  |
|     - |  486 | `/* int (*xUmask)(int) */` |
|     8 |  487 | `static int UnixVfs_Umask(int new_mask)` |
|     - |  488 |  |
|     - |  489 | `	int old_mask;` |
|     8 |  490 | `	old_mask = umask(new_mask);` |
|     8 |  491 | `	return old_mask;` |
|     - |  492 |  |
|     - |  493 | `/* void (*xUsername)(ph7_context *) */` |
|     2 |  494 | `static void UnixVfs_Username(ph7_context *pCtx)` |
|     - |  495 |  |
|     - |  496 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  497 | `  struct passwd *pwd;` |
|     - |  498 | `  uid_t uid;` |
|     2 |  499 | `  uid = getuid();` |
|     2 |  500 | `  pwd = getpwuid(uid);   /* Try getting UID for username */` |
|     2 |  501 | `  if (pwd == 0) {` |
|   ! 0 |  502 | `    return;` |
|     - |  503 | `  }` |
|     - |  504 | `  /* Return the username */` |
|     2 |  505 | `  ph7_result_string(pCtx,pwd->pw_name,-1);` |
|     - |  506 | `#else` |
|     - |  507 | `  ph7_result_string(pCtx,"Unknown",-1);` |
|     - |  508 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     2 |  509 | `  return;` |
|     1 |  510 |  |
|     - |  511 | `/* int (*xLink)(const char *,const char *,int) */` |
|     8 |  512 | `static int UnixVfs_link(const char *zSrc,const char *zTarget,int is_sym)` |
|     - |  513 |  |
|     - |  514 | `	int rc;` |
|     8 |  515 | `	if( is_sym ){` |
|     - |  516 | `		/* Symbolic link */` |
|     6 |  517 | `		rc = symlink(zSrc,zTarget);` |
|     3 |  518 | `	}else{` |
|     - |  519 | `		/* Hard link */` |
|     2 |  520 | `		rc = link(zSrc,zTarget);` |
|     - |  521 | `	}` |
|     8 |  522 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  523 |  |
|     - |  524 | `/* int (*xChroot)(const char *) */` |
|     2 |  525 | `static int UnixVfs_chroot(const char *zRootDir)` |
|     - |  526 |  |
|     - |  527 | `	int rc;` |
|     2 |  528 | `	rc = chroot(zRootDir);` |
|     2 |  529 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  530 |  |
|     - |  531 | `/* Export the UNIX vfs */` |
|     - |  532 | `PH7_PRIVATE const ph7_vfs sUnixVfs = {` |
|     - |  533 | `	"Unix_vfs",` |
|     - |  534 | `	PH7_VFS_VERSION,` |
|     - |  535 | `	UnixVfs_chdir,    /* int (*xChdir)(const char *) */` |
|     - |  536 | `	UnixVfs_chroot,   /* int (*xChroot)(const char *); */` |
|     - |  537 | `	UnixVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|     - |  538 | `	UnixVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|     - |  539 | `	UnixVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|     - |  540 | `	UnixVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|     - |  541 | `	UnixVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|     - |  542 | `	UnixVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|     - |  543 | `	UnixVfs_Sleep,    /* int (*xSleep)(unsigned int) */` |
|     - |  544 | `	UnixVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|     - |  545 | `	UnixVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|     - |  546 | `	UnixVfs_Chmod, /*int (*xChmod)(const char *,int)*/` |
|     - |  547 | `	UnixVfs_Chown, /*int (*xChown)(const char *,const char *)*/` |
|     - |  548 | `	UnixVfs_Chgrp, /*int (*xChgrp)(const char *,const char *)*/` |
|     - |  549 |  |
|     - |  550 |  |
|     - |  551 | `	UnixVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|     - |  552 | `	UnixVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|     - |  553 | `	UnixVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|     - |  554 | `	UnixVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|     - |  555 | `	UnixVfs_Stat,  /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|     - |  556 | `	UnixVfs_lStat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|     - |  557 | `	UnixVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|     - |  558 | `	UnixVfs_islink,     /* int (*xIslink)(const char *) */` |
|     - |  559 | `	UnixVfs_isreadable, /* int (*xReadable)(const char *) */` |
|     - |  560 | `	UnixVfs_iswritable, /* int (*xWritable)(const char *) */` |
|     - |  561 | `	UnixVfs_isexecutable,/* int (*xExecutable)(const char *) */` |
|     - |  562 | `	UnixVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|     - |  563 | `	UnixVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|     - |  564 | `	UnixVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|     - |  565 | `	UnixVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|     - |  566 | `	UnixVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|     - |  567 | `	UnixVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|     - |  568 | `	UnixVfs_link,       /* int (*xLink)(const char *,const char *,int) */` |
|     - |  569 | `	UnixVfs_Umask,      /* int (*xUmask)(int) */` |
|     - |  570 | `	UnixVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|     - |  571 | `	UnixVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|     - |  572 | `	UnixVfs_uid, /* int (*xUid)(void) */` |
|     - |  573 | `	UnixVfs_gid, /* int (*xGid)(void) */` |
|     - |  574 | `	UnixVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|     - |  575 |  |
|     - |  576 | `};` |
|     - |  577 | `/* UNIX File IO */` |
|     - |  578 | `#define PH7_UNIX_OPEN_MODE	0640 /* Default open mode */` |
|     - |  579 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
| 27036 |  580 | `static int UnixFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|     - |  581 |  |
| 27036 |  582 | `	int iOpen = O_RDONLY;` |
|     - |  583 | `	int fd;` |
|     - |  584 | `	/* Set the desired flags according to the open mode */` |
| 27036 |  585 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|     - |  586 | `		/* Open existing file, or create if it doesn't exist */` |
| 12390 |  587 | `		iOpen = O_CREAT;` |
| 12390 |  588 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|     - |  589 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
| 12390 |  590 | `			iOpen \|= O_TRUNC;` |
|  6195 |  591 | `			SXUNUSED(pResource); /* cc warning */` |
|  6195 |  592 | `		}` |
| 20841 |  593 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|     - |  594 | `		/* Creates a new file, only if it does not already exist.` |
|     - |  595 | `		* If the file exists, it fails.` |
|     - |  596 | `		*/` |
|   ! 0 |  597 | `		iOpen = O_CREAT\|O_EXCL;` |
| 14646 |  598 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|     - |  599 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|     - |  600 | `		 * The file must exist.` |
|     - |  601 | `		 */` |
|   ! 0 |  602 | `		iOpen = O_RDWR\|O_TRUNC;` |
|   ! 0 |  603 | `	}` |
| 27036 |  604 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|     - |  605 | `		/* Read+Write access */` |
| 12374 |  606 | `		iOpen &= ~O_RDONLY;` |
| 12374 |  607 | `		iOpen \|= O_RDWR;` |
| 20849 |  608 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|     - |  609 | `		/* Write only access */` |
|    22 |  610 | `		iOpen &= ~O_RDONLY;` |
|    22 |  611 | `		iOpen \|= O_WRONLY;` |
|    11 |  612 | `	}` |
| 27036 |  613 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|     - |  614 | `		/* Append mode */` |
|   ! 0 |  615 | `		iOpen \|= O_APPEND;` |
|   ! 0 |  616 | `	}` |
|     - |  617 | `#ifdef O_TEMP` |
|     - |  618 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|     - |  619 | `		/* File is temporary */` |
|     - |  620 | `		iOpen \|= O_TEMP;` |
|     - |  621 | `	}` |
|     - |  622 | `#endif` |
|     - |  623 | `	/* Open the file now */` |
| 27036 |  624 | `	fd = open(zPath,iOpen,PH7_UNIX_OPEN_MODE);` |
| 27036 |  625 | `	if( fd < 0 ){` |
|     - |  626 | `		/* IO error */` |
|    12 |  627 | `		return -1;` |
|     - |  628 | `	}` |
|     - |  629 | `	/* Save the handle */` |
| 27024 |  630 | `	*ppHandle = SX_INT_TO_PTR(fd);` |
| 27024 |  631 | `	return PH7_OK;` |
| 13518 |  632 |  |
|     - |  633 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|   912 |  634 | `static int UnixDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|     - |  635 |  |
|     - |  636 | `	DIR *pDir;` |
|     - |  637 | `	/* Open the target directory */` |
|   912 |  638 | `	pDir = opendir(zPath);` |
|   912 |  639 | `	if( pDir == 0 ){` |
|   ! 0 |  640 | `		SXUNUSED(pResource); /* Compiler warning */` |
|   ! 0 |  641 | `		return -1;` |
|     - |  642 | `	}` |
|     - |  643 | `	/* Save our structure */` |
|   912 |  644 | `	*ppHandle = pDir;` |
|   912 |  645 | `	return PH7_OK;` |
|   456 |  646 |  |
|     - |  647 | `/* void (*xCloseDir)(void *) */` |
|   912 |  648 | `static void UnixDir_Close(void *pUserData)` |
|     - |  649 |  |
|   912 |  650 | `	closedir((DIR *)pUserData);` |
|   912 |  651 |  |
|     - |  652 | `/* void (*xClose)(void *); */` |
| 27022 |  653 | `static void UnixFile_Close(void *pUserData)` |
|     - |  654 |  |
| 27022 |  655 | `	close(SX_PTR_TO_INT(pUserData));` |
| 27022 |  656 |  |
|     - |  657 | `/* int (*xReadDir)(void *,ph7_context *) */` |
|  7144 |  658 | `static int UnixDir_Read(void *pUserData,ph7_context *pCtx)` |
|     - |  659 |  |
|  7144 |  660 | `	DIR *pDir = (DIR *)pUserData;` |
|     - |  661 | `	struct dirent *pEntry;` |
|  7144 |  662 | `	char *zName = 0; /* cc warning */` |
|  7144 |  663 | `	sxu32 n = 0;` |
|  4486 |  664 | `	for(;;){` |
|  8970 |  665 | `		pEntry = readdir(pDir);` |
|  8970 |  666 | `		if( pEntry == 0 ){` |
|     - |  667 | `			/* No more entries to process */` |
|   910 |  668 | `			return -1;` |
|     - |  669 | `		}` |
|  8060 |  670 | `		zName = pEntry->d_name;` |
|  8060 |  671 | `		n = SyStrlen(zName);` |
|     - |  672 | `		/* Ignore '.' && '..' */` |
|  8060 |  673 | `		if( n > sizeof("..")-1 \|\| zName[0] != '.' \|\| ( n == sizeof("..")-1 && zName[1] != '.') ){` |
|  3117 |  674 | `			break;` |
|     - |  675 | `		}` |
|     - |  676 | `		/* Next entry */` |
|     - |  677 | `	}` |
|     - |  678 | `	/* Return the current file name */` |
|  6234 |  679 | `	ph7_result_string(pCtx,zName,(int)n);` |
|  6234 |  680 | `	return PH7_OK;` |
|  3572 |  681 |  |
|     - |  682 | `/* void (*xRewindDir)(void *) */` |
|     2 |  683 | `static void UnixDir_Rewind(void *pUserData)` |
|     - |  684 |  |
|     2 |  685 | `	rewinddir((DIR *)pUserData);` |
|     2 |  686 |  |
|     - |  687 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
| 29218 |  688 | `static ph7_int64 UnixFile_Read(void *pUserData,void *pBuffer,ph7_int64 nDatatoRead)` |
|     - |  689 |  |
|     - |  690 | `	ssize_t nRd;` |
| 29218 |  691 | `	nRd = read(SX_PTR_TO_INT(pUserData),pBuffer,(size_t)nDatatoRead);` |
| 29218 |  692 | `	if( nRd < 1 ){` |
|     - |  693 | `		/* EOF or IO error */` |
| 14604 |  694 | `		return -1;` |
|     - |  695 | `	}` |
| 14614 |  696 | `	return (ph7_int64)nRd;` |
| 14609 |  697 |  |
|     - |  698 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
| 12412 |  699 | `static ph7_int64 UnixFile_Write(void *pUserData,const void *pBuffer,ph7_int64 nWrite)` |
|     - |  700 |  |
| 12412 |  701 | `	const char *zData = (const char *)pBuffer;` |
| 12412 |  702 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|     - |  703 | `	ph7_int64 nCount;` |
|     - |  704 | `	ssize_t nWr;` |
| 12412 |  705 | `	nCount = 0;` |
| 12412 |  706 | `	for(;;){` |
| 24824 |  707 | `		if( nWrite < 1 ){` |
| 12412 |  708 | `			break;` |
|     - |  709 | `		}` |
| 12412 |  710 | `		nWr = write(fd,zData,(size_t)nWrite);` |
| 12412 |  711 | `		if( nWr < 1 ){` |
|     - |  712 | `			/* IO error */` |
|   ! 0 |  713 | `			break;` |
|     - |  714 | `		}` |
| 12412 |  715 | `		nWrite -= nWr;` |
| 12412 |  716 | `		nCount += nWr;` |
| 12412 |  717 | `		zData += nWr;` |
|     - |  718 | `	}` |
| 12412 |  719 | `	if( nWrite > 0 ){` |
|   ! 0 |  720 | `		return -1;` |
|     - |  721 | `	}` |
| 12412 |  722 | `	return nCount;` |
|  6206 |  723 |  |
|     - |  724 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|     6 |  725 | `static int UnixFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|     - |  726 |  |
|     - |  727 | `	off_t iNew;` |
|     6 |  728 | `	switch(whence){` |
|   ! 0 |  729 | `	case 1:/*SEEK_CUR*/` |
|   ! 0 |  730 | `		whence = SEEK_CUR;` |
|   ! 0 |  731 | `		break;` |
|   ! 0 |  732 | `	case 2: /* SEEK_END */` |
|   ! 0 |  733 | `		whence = SEEK_END;` |
|   ! 0 |  734 | `		break;` |
|     6 |  735 | `	case 0: /* SEEK_SET */` |
|     - |  736 | `	default:` |
|     6 |  737 | `		whence = SEEK_SET;` |
|     6 |  738 | `		break;` |
|     - |  739 | `	}` |
|     6 |  740 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),(off_t)iOfft,whence);` |
|     6 |  741 | `	if( iNew < 0 ){` |
|   ! 0 |  742 | `		return -1;` |
|     - |  743 | `	}` |
|     6 |  744 | `	return PH7_OK;` |
|     3 |  745 |  |
|     - |  746 | `/* int (*xLock)(void *,int) */` |
|     4 |  747 | `static int UnixFile_Lock(void *pUserData,int lock_type)` |
|     - |  748 |  |
|     4 |  749 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|     4 |  750 | `	int rc = PH7_OK; /* cc warning */` |
|     4 |  751 | `	if( lock_type < 0 ){` |
|     - |  752 | `		/* Unlock the file */` |
|   ! 0 |  753 | `		rc = flock(fd,LOCK_UN);` |
|   ! 0 |  754 | `	}else{` |
|     4 |  755 | `		if( lock_type == 1 ){` |
|     - |  756 | `			/* Exculsive lock */` |
|     2 |  757 | `			rc = flock(fd,LOCK_EX);` |
|     1 |  758 | `		}else{` |
|     - |  759 | `			/* Shared lock */` |
|     2 |  760 | `			rc = flock(fd,LOCK_SH);` |
|     - |  761 | `		}` |
|     - |  762 | `	}` |
|     4 |  763 | `	return !rc ? PH7_OK : -1;` |
|     - |  764 |  |
|     - |  765 | `/* ph7_int64 (*xTell)(void *) */` |
|     6 |  766 | `static ph7_int64 UnixFile_Tell(void *pUserData)` |
|     - |  767 |  |
|     - |  768 | `	off_t iNew;` |
|     6 |  769 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),0,SEEK_CUR);` |
|     6 |  770 | `	return (ph7_int64)iNew;` |
|     - |  771 |  |
|     - |  772 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|     6 |  773 | `static int UnixFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|     - |  774 |  |
|     - |  775 | `	int rc;` |
|     6 |  776 | `	rc = ftruncate(SX_PTR_TO_INT(pUserData),(off_t)nOfft);` |
|     6 |  777 | `	if( rc != 0 ){` |
|   ! 0 |  778 | `		return -1;` |
|     - |  779 | `	}` |
|     6 |  780 | `	return PH7_OK;` |
|     3 |  781 |  |
|     - |  782 | `/* int (*xSync)(void *); */` |
|     2 |  783 | `static int UnixFile_Sync(void *pUserData)` |
|     - |  784 |  |
|     - |  785 | `	int rc;` |
|     2 |  786 | `	rc = fsync(SX_PTR_TO_INT(pUserData));` |
|     2 |  787 | `	return rc == 0 ? PH7_OK : - 1;` |
|     - |  788 |  |
|     - |  789 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|     2 |  790 | `static int UnixFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  791 |  |
|     - |  792 | `	struct stat st;` |
|     - |  793 | `	int rc;` |
|     2 |  794 | `	rc = fstat(SX_PTR_TO_INT(pUserData),&st);` |
|     2 |  795 | `	if( rc != 0 ){` |
|   ! 0 |  796 | `	 return -1;` |
|     - |  797 | `	}` |
|     - |  798 | `	/* dev */` |
|     2 |  799 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|     2 |  800 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  801 | `	/* ino */` |
|     2 |  802 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|     2 |  803 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  804 | `	/* mode */` |
|     2 |  805 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|     2 |  806 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  807 | `	/* nlink */` |
|     2 |  808 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|     2 |  809 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  810 | `	/* uid,gid,rdev */` |
|     2 |  811 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|     2 |  812 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|     2 |  813 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|     2 |  814 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|     2 |  815 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|     2 |  816 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  817 | `	/* size */` |
|     2 |  818 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|     2 |  819 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  820 | `	/* atime */` |
|     2 |  821 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|     2 |  822 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  823 | `	/* mtime */` |
|     2 |  824 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|     2 |  825 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  826 | `	/* ctime */` |
|     2 |  827 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|     2 |  828 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  829 | `	/* blksize,blocks */` |
|     2 |  830 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|     2 |  831 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|     2 |  832 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|     2 |  833 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|     2 |  834 | `	return PH7_OK;` |
|     1 |  835 |  |
|     - |  836 | `/* Export the file:// stream */` |
|     - |  837 | `PH7_PRIVATE const ph7_io_stream sUnixFileStream = {` |
|     - |  838 | `	"file", /* Stream name */` |
|     - |  839 | `	PH7_IO_STREAM_VERSION,` |
|     - |  840 | `	UnixFile_Open,  /* xOpen */` |
|     - |  841 | `	UnixDir_Open,   /* xOpenDir */` |
|     - |  842 | `	UnixFile_Close, /* xClose */` |
|     - |  843 | `	UnixDir_Close,  /* xCloseDir */` |
|     - |  844 | `	UnixFile_Read,  /* xRead */` |
|     - |  845 | `	UnixDir_Read,   /* xReadDir */` |
|     - |  846 | `	UnixFile_Write, /* xWrite */` |
|     - |  847 | `	UnixFile_Seek,  /* xSeek */` |
|     - |  848 | `	UnixFile_Lock,  /* xLock */` |
|     - |  849 | `	UnixDir_Rewind, /* xRewindDir */` |
|     - |  850 | `	UnixFile_Tell,  /* xTell */` |
|     - |  851 | `	UnixFile_Trunc, /* xTrunc */` |
|     - |  852 | `	UnixFile_Sync,  /* xSeek */` |
|     - |  853 | `	UnixFile_Stat   /* xStat */` |
|     - |  854 | `};` |
|     - |  855 | `#endif /* __UNIXES__ */` |
|     - |  856 |  |
