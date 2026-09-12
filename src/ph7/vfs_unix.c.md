# src/ph7/vfs_unix.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 387/433 lines (89.38%)

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
|     - |   30 | `/* int (*xchdir)(const char *) */` |
| 13438 |   31 | `static int UnixVfs_chdir(const char *zPath)` |
|     - |   32 | `{` |
|     - |   33 | `  int rc;` |
| 13438 |   34 | `  rc = chdir(zPath);` |
| 13438 |   35 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |   36 | `}` |
|     - |   37 | `/* int (*xGetcwd)(ph7_context *) */` |
|    20 |   38 | `static int UnixVfs_getcwd(ph7_context *pCtx)` |
|     - |   39 | `{` |
|     - |   40 | `	char zBuf[4096];` |
|     - |   41 | `	char *zDir;` |
|     - |   42 | `	/* Get the current directory */` |
|    20 |   43 | `	zDir = getcwd(zBuf,sizeof(zBuf));` |
|    20 |   44 | `	if( zDir == 0 ){` |
|   ! 0 |   45 | `	  return -1;` |
|     - |   46 | `    }` |
|    20 |   47 | `	ph7_result_string(pCtx,zDir,-1/*Compute length automatically*/);` |
|    20 |   48 | `	return PH7_OK;` |
|    10 |   49 | `}` |
|     - |   50 | `/* int (*xMkdir)(const char *,int,int) */` |
|    44 |   51 | `static int UnixVfs_mkdir(const char *zPath,int mode,int recursive)` |
|     - |   52 | `{` |
|     - |   53 | `	int rc;` |
|    44 |   54 | `        rc = mkdir(zPath,mode);` |
|    22 |   55 | `	SXUNUSED(recursive); /* cc warning */` |
|    44 |   56 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   57 | `}` |
|     - |   58 | `/* int (*xRmdir)(const char *) */` |
|    44 |   59 | `static int UnixVfs_rmdir(const char *zPath)` |
|     - |   60 | `{` |
|     - |   61 | `	int rc;` |
|    44 |   62 | `	rc = rmdir(zPath);` |
|    44 |   63 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   64 | `}` |
|     - |   65 | `/* int (*xIsdir)(const char *) */` |
|  8814 |   66 | `static int UnixVfs_isdir(const char *zPath)` |
|     - |   67 | `{` |
|     - |   68 | `	struct stat st;` |
|     - |   69 | `	int rc;` |
|  8814 |   70 | `	rc = stat(zPath,&st);` |
|  8814 |   71 | `	if( rc != 0 ){` |
|     4 |   72 | `	 return -1;` |
|     - |   73 | `	}` |
|  8810 |   74 | `	rc = S_ISDIR(st.st_mode);` |
|  8810 |   75 | `	return rc ? PH7_OK : -1 ;` |
|  4407 |   76 | `}` |
|     - |   77 | `/* int (*xRename)(const char *,const char *) */` |
|     2 |   78 | `static int UnixVfs_Rename(const char *zOld,const char *zNew)` |
|     - |   79 | `{` |
|     - |   80 | `	int rc;` |
|     2 |   81 | `	rc = rename(zOld,zNew);` |
|     2 |   82 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   83 | `}` |
|     - |   84 | `/* int (*xRealpath)(const char *,ph7_context *) */` |
|     6 |   85 | `static int UnixVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|     - |   86 | `{` |
|     - |   87 | `#ifndef PH7_UNIX_OLD_LIBC` |
|     - |   88 | `	char *zReal;` |
|     6 |   89 | `	zReal = realpath(zPath,0);` |
|     6 |   90 | `	if( zReal == 0 ){` |
|     2 |   91 | `	  return -1;` |
|     - |   92 | `	}` |
|     4 |   93 | `	ph7_result_string(pCtx,zReal,-1/*Compute length automatically*/);` |
|     - |   94 | `        /* Release the allocated buffer */` |
|     4 |   95 | `	free(zReal);` |
|     4 |   96 | `	return PH7_OK;` |
|     - |   97 | `#else` |
|     - |   98 | `    zPath = 0; /* cc warning */` |
|     - |   99 | `    pCtx = 0;` |
|     - |  100 | `    return -1;` |
|     - |  101 | `#endif` |
|     3 |  102 | `}` |
|     - |  103 | `/* int (*xSleep)(unsigned int) */` |
|    60 |  104 | `static int UnixVfs_Sleep(unsigned int uSec)` |
|     - |  105 | `{` |
|    60 |  106 | `	usleep(uSec);` |
|    60 |  107 | `	return PH7_OK;` |
|     - |  108 | `}` |
|     - |  109 | `/* int (*xUnlink)(const char *) */` |
| 33658 |  110 | `static int UnixVfs_unlink(const char *zPath)` |
|     - |  111 | `{` |
|     - |  112 | `	int rc;` |
| 33658 |  113 | `	rc = unlink(zPath);` |
| 33658 |  114 | `	return rc == 0 ? PH7_OK : -1 ;` |
|     - |  115 | `}` |
|     - |  116 | `/* int (*xFileExists)(const char *) */` |
|   180 |  117 | `static int UnixVfs_FileExists(const char *zPath)` |
|     - |  118 | `{` |
|     - |  119 | `	int rc;` |
|   180 |  120 | `	rc = access(zPath,F_OK);` |
|   180 |  121 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  122 | `}` |
|     - |  123 | `/* ph7_int64 (*xFileSize)(const char *) */` |
|     - |  124 | `/* ph7_int64 (*xFreeSpace)(const char *) */` |
|     4 |  125 | `static ph7_int64 UnixVfs_FreeSpace(const char *zPath)` |
|     - |  126 | `{` |
|     - |  127 | `	struct statvfs sInfo;` |
|     4 |  128 | `	if( statvfs(zPath,&sInfo) != 0 ){` |
|   ! 0 |  129 | `		return -1;` |
|     - |  130 | `	}` |
|     - |  131 | `	/* php reports the space available to an UNPRIVILEGED user (f_bavail) */` |
|     4 |  132 | `	return (ph7_int64)sInfo.f_bavail * (ph7_int64)sInfo.f_frsize;` |
|     2 |  133 | `}` |
|     - |  134 | `/* ph7_int64 (*xTotalSpace)(const char *) */` |
|     4 |  135 | `static ph7_int64 UnixVfs_TotalSpace(const char *zPath)` |
|     - |  136 | `{` |
|     - |  137 | `	struct statvfs sInfo;` |
|     4 |  138 | `	if( statvfs(zPath,&sInfo) != 0 ){` |
|   ! 0 |  139 | `		return -1;` |
|     - |  140 | `	}` |
|     4 |  141 | `	return (ph7_int64)sInfo.f_blocks * (ph7_int64)sInfo.f_frsize;` |
|     2 |  142 | `}` |
|    26 |  143 | `static ph7_int64 UnixVfs_FileSize(const char *zPath)` |
|     - |  144 | `{` |
|     - |  145 | `	struct stat st;` |
|     - |  146 | `	int rc;` |
|    26 |  147 | `	rc = stat(zPath,&st);` |
|    26 |  148 | `	if( rc != 0 ){` |
|   ! 0 |  149 | `	 return -1;` |
|     - |  150 | `	}` |
|    26 |  151 | `	return (ph7_int64)st.st_size;` |
|    13 |  152 | `}` |
|     - |  153 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|     4 |  154 | `static int UnixVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|     - |  155 | `{` |
|     - |  156 | `	struct utimbuf ut;` |
|     - |  157 | `	int rc;` |
|     4 |  158 | `	ut.actime  = (time_t)access_time;` |
|     4 |  159 | `	ut.modtime = (time_t)touch_time;` |
|     4 |  160 | `	rc = utime(zPath,&ut);` |
|     4 |  161 | `	if( rc != 0 ){` |
|   ! 0 |  162 | `	 return -1;` |
|     - |  163 | `	}` |
|     4 |  164 | `	return PH7_OK;` |
|     2 |  165 | `}` |
|     - |  166 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|     2 |  167 | `static ph7_int64 UnixVfs_FileAtime(const char *zPath)` |
|     - |  168 | `{` |
|     - |  169 | `	struct stat st;` |
|     - |  170 | `	int rc;` |
|     2 |  171 | `	rc = stat(zPath,&st);` |
|     2 |  172 | `	if( rc != 0 ){` |
|   ! 0 |  173 | `	 return -1;` |
|     - |  174 | `	}` |
|     2 |  175 | `	return (ph7_int64)st.st_atime;` |
|     1 |  176 | `}` |
|     - |  177 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|     4 |  178 | `static ph7_int64 UnixVfs_FileMtime(const char *zPath)` |
|     - |  179 | `{` |
|     - |  180 | `	struct stat st;` |
|     - |  181 | `	int rc;` |
|     4 |  182 | `	rc = stat(zPath,&st);` |
|     4 |  183 | `	if( rc != 0 ){` |
|   ! 0 |  184 | `	 return -1;` |
|     - |  185 | `	}` |
|     4 |  186 | `	return (ph7_int64)st.st_mtime;` |
|     2 |  187 | `}` |
|     - |  188 | `/* ph7_int64 (*xFileCtime)(const char *) */` |
|     2 |  189 | `static ph7_int64 UnixVfs_FileCtime(const char *zPath)` |
|     - |  190 | `{` |
|     - |  191 | `	struct stat st;` |
|     - |  192 | `	int rc;` |
|     2 |  193 | `	rc = stat(zPath,&st);` |
|     2 |  194 | `	if( rc != 0 ){` |
|   ! 0 |  195 | `	 return -1;` |
|     - |  196 | `	}` |
|     2 |  197 | `	return (ph7_int64)st.st_ctime;` |
|     1 |  198 | `}` |
|     - |  199 | `/* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|    10 |  200 | `static int UnixVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  201 | `{` |
|     - |  202 | `	struct stat st;` |
|     - |  203 | `	int rc;` |
|    10 |  204 | `	rc = stat(zPath,&st);` |
|    10 |  205 | `	if( rc != 0 ){` |
|   ! 0 |  206 | `	 return -1;` |
|     - |  207 | `	}` |
|     - |  208 | `	/* dev */` |
|    10 |  209 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|    10 |  210 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  211 | `	/* ino */` |
|    10 |  212 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|    10 |  213 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  214 | `	/* mode */` |
|    10 |  215 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|    10 |  216 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  217 | `	/* nlink */` |
|    10 |  218 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|    10 |  219 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  220 | `	/* uid,gid,rdev */` |
|    10 |  221 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|    10 |  222 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    10 |  223 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|    10 |  224 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    10 |  225 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|    10 |  226 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  227 | `	/* size */` |
|    10 |  228 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|    10 |  229 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  230 | `	/* atime */` |
|    10 |  231 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|    10 |  232 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  233 | `	/* mtime */` |
|    10 |  234 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|    10 |  235 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  236 | `	/* ctime */` |
|    10 |  237 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|    10 |  238 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  239 | `	/* blksize,blocks */` |
|    10 |  240 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|    10 |  241 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    10 |  242 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|    10 |  243 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    10 |  244 | `	return PH7_OK;` |
|     5 |  245 | `}` |
|     - |  246 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|     2 |  247 | `static int UnixVfs_lStat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  248 | `{` |
|     - |  249 | `	struct stat st;` |
|     - |  250 | `	int rc;` |
|     2 |  251 | `	rc = lstat(zPath,&st);` |
|     2 |  252 | `	if( rc != 0 ){` |
|   ! 0 |  253 | `	 return -1;` |
|     - |  254 | `	}` |
|     - |  255 | `	/* dev */` |
|     2 |  256 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|     2 |  257 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  258 | `	/* ino */` |
|     2 |  259 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|     2 |  260 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  261 | `	/* mode */` |
|     2 |  262 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|     2 |  263 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  264 | `	/* nlink */` |
|     2 |  265 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|     2 |  266 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  267 | `	/* uid,gid,rdev */` |
|     2 |  268 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|     2 |  269 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|     2 |  270 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|     2 |  271 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|     2 |  272 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|     2 |  273 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  274 | `	/* size */` |
|     2 |  275 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|     2 |  276 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  277 | `	/* atime */` |
|     2 |  278 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|     2 |  279 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  280 | `	/* mtime */` |
|     2 |  281 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|     2 |  282 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  283 | `	/* ctime */` |
|     2 |  284 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|     2 |  285 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  286 | `	/* blksize,blocks */` |
|     2 |  287 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|     2 |  288 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|     2 |  289 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|     2 |  290 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|     2 |  291 | `	return PH7_OK;` |
|     1 |  292 | `}` |
|     - |  293 | `/* int (*xChmod)(const char *,int) */` |
|   146 |  294 | `static int UnixVfs_Chmod(const char *zPath,int mode)` |
|     - |  295 | `{` |
|     - |  296 | `    int rc;` |
|   146 |  297 | `    rc = chmod(zPath,(mode_t)mode);` |
|   146 |  298 | `    return rc == 0 ? PH7_OK : - 1;` |
|     - |  299 | `}` |
|     - |  300 | `/* int (*xChown)(const char *,const char *) */` |
|     6 |  301 | `static int UnixVfs_Chown(const char *zPath,const char *zUser)` |
|     - |  302 | `{` |
|     - |  303 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  304 | `  struct passwd *pwd;` |
|     - |  305 | `  uid_t uid;` |
|     - |  306 | `  int rc;` |
|     - |  307 | `  /* php accepts a numeric uid as well as a name; PH7 only ever did getpwnam(), so` |
|     - |  308 | `   * chown($f, 0) failed on the NAME lookup and never reached the syscall (leaving errno` |
|     - |  309 | `   * unset, hence a bogus "Undefined error: 0" in the warning). */` |
|     6 |  310 | `  if( zUser[0] >= '0' && zUser[0] <= '9' ){` |
|     4 |  311 | `    uid = (uid_t)atoi(zUser);` |
|     2 |  312 | `  }else{` |
|     2 |  313 | `    pwd = getpwnam(zUser);   /* Try getting UID for username */` |
|     2 |  314 | `    if (pwd == 0) {` |
|     - |  315 | `      /* -2 = the NAME could not be resolved (no syscall ran, so errno means nothing).` |
|     - |  316 | `       * php words that case differently: "chown(): Unable to find uid for bogus". */` |
|     2 |  317 | `      return -2;` |
|     - |  318 | `    }` |
|   ! 0 |  319 | `    uid = pwd->pw_uid;` |
|     - |  320 | `  }` |
|     4 |  321 | `  rc = chown(zPath,uid,-1);` |
|     4 |  322 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |  323 | `#else` |
|     - |  324 | `	SXUNUSED(zPath);` |
|     - |  325 | `	SXUNUSED(zUser);` |
|     - |  326 | `	return -1;` |
|     - |  327 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     3 |  328 | `}` |
|     - |  329 | `/* int (*xChgrp)(const char *,const char *) */` |
|     6 |  330 | `static int UnixVfs_Chgrp(const char *zPath,const char *zGroup)` |
|     - |  331 | `{` |
|     - |  332 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  333 | `  struct group *group;` |
|     - |  334 | `  gid_t gid;` |
|     - |  335 | `  int rc;` |
|     - |  336 | `  /* Numeric gid accepted too (see UnixVfs_Chown) */` |
|     6 |  337 | `  if( zGroup[0] >= '0' && zGroup[0] <= '9' ){` |
|     4 |  338 | `    gid = (gid_t)atoi(zGroup);` |
|     2 |  339 | `  }else{` |
|     2 |  340 | `    group = getgrnam(zGroup);` |
|     2 |  341 | `    if (group == 0) {` |
|     2 |  342 | `      return -2;   /* name lookup failed -- see UnixVfs_Chown */` |
|     - |  343 | `    }` |
|   ! 0 |  344 | `    gid = group->gr_gid;` |
|     - |  345 | `  }` |
|     4 |  346 | `  rc = chown(zPath,-1,gid);` |
|     4 |  347 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |  348 | `#else` |
|     - |  349 | `	SXUNUSED(zPath);` |
|     - |  350 | `	SXUNUSED(zGroup);` |
|     - |  351 | `	return -1;` |
|     - |  352 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     3 |  353 | `}` |
|     - |  354 | `/* int (*xIsfile)(const char *) */` |
|  6754 |  355 | `static int UnixVfs_isfile(const char *zPath)` |
|     - |  356 | `{` |
|     - |  357 | `	struct stat st;` |
|     - |  358 | `	int rc;` |
|  6754 |  359 | `	rc = stat(zPath,&st);` |
|  6754 |  360 | `	if( rc != 0 ){` |
|    52 |  361 | `	 return -1;` |
|     - |  362 | `	}` |
|  6702 |  363 | `	rc = S_ISREG(st.st_mode);` |
|  6702 |  364 | `	return rc ? PH7_OK : -1 ;` |
|  3377 |  365 | `}` |
|     - |  366 | `/* int (*xIslink)(const char *) */` |
|     4 |  367 | `static int UnixVfs_islink(const char *zPath)` |
|     - |  368 | `{` |
|     - |  369 | `	struct stat st;` |
|     - |  370 | `	int rc;` |
|     4 |  371 | `	rc = stat(zPath,&st);` |
|     4 |  372 | `	if( rc != 0 ){` |
|   ! 0 |  373 | `	 return -1;` |
|     - |  374 | `	}` |
|     4 |  375 | `	rc = S_ISLNK(st.st_mode);` |
|     4 |  376 | `	return rc ? PH7_OK : -1 ;` |
|     2 |  377 | `}` |
|     - |  378 | `/* int (*xReadable)(const char *) */` |
|     2 |  379 | `static int UnixVfs_isreadable(const char *zPath)` |
|     - |  380 | `{` |
|     - |  381 | `	int rc;` |
|     2 |  382 | `	rc = access(zPath,R_OK);` |
|     2 |  383 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  384 | `}` |
|     - |  385 | `/* int (*xWritable)(const char *) */` |
|     4 |  386 | `static int UnixVfs_iswritable(const char *zPath)` |
|     - |  387 | `{` |
|     - |  388 | `	int rc;` |
|     4 |  389 | `	rc = access(zPath,W_OK);` |
|     4 |  390 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  391 | `}` |
|     - |  392 | `/* int (*xExecutable)(const char *) */` |
|     2 |  393 | `static int UnixVfs_isexecutable(const char *zPath)` |
|     - |  394 | `{` |
|     - |  395 | `	int rc;` |
|     2 |  396 | `	rc = access(zPath,X_OK);` |
|     2 |  397 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  398 | `}` |
|     - |  399 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|     4 |  400 | `static int UnixVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|     - |  401 | `{` |
|     - |  402 | `	struct stat st;` |
|     - |  403 | `	int rc;` |
|     4 |  404 | `    rc = stat(zPath,&st);` |
|     4 |  405 | `	if( rc != 0 ){` |
|     - |  406 | `	  /* Expand 'unknown' */` |
|   ! 0 |  407 | `	  ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|   ! 0 |  408 | `	  return -1;` |
|     - |  409 | `	}` |
|     4 |  410 | `	if(S_ISREG(st.st_mode) ){` |
|     2 |  411 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|     3 |  412 | `	}else if(S_ISDIR(st.st_mode)){` |
|     2 |  413 | `		ph7_result_string(pCtx,"dir",sizeof("dir")-1);` |
|     1 |  414 | `	}else if(S_ISLNK(st.st_mode)){` |
|   ! 0 |  415 | `		ph7_result_string(pCtx,"link",sizeof("link")-1);` |
|   ! 0 |  416 | `	}else if(S_ISBLK(st.st_mode)){` |
|   ! 0 |  417 | `		ph7_result_string(pCtx,"block",sizeof("block")-1);` |
|   ! 0 |  418 | `    }else if(S_ISSOCK(st.st_mode)){` |
|   ! 0 |  419 | `		ph7_result_string(pCtx,"socket",sizeof("socket")-1);` |
|   ! 0 |  420 | `	}else if(S_ISFIFO(st.st_mode)){` |
|   ! 0 |  421 | `       ph7_result_string(pCtx,"fifo",sizeof("fifo")-1);` |
|   ! 0 |  422 | `	}else{` |
|   ! 0 |  423 | `		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|     - |  424 | `	}` |
|     4 |  425 | `	return PH7_OK;` |
|     2 |  426 | `}` |
|     - |  427 | `/* int (*xGetenv)(const char *,ph7_context *) */` |
|    56 |  428 | `static int UnixVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|     - |  429 | `{` |
|     - |  430 | `	char *zEnv;` |
|    56 |  431 | `	zEnv = getenv(zVar);` |
|    56 |  432 | `	if( zEnv == 0 ){` |
|   ! 0 |  433 | `	  return -1;` |
|     - |  434 | `	}` |
|    56 |  435 | `	ph7_result_string(pCtx,zEnv,-1/*Compute length automatically*/);` |
|    56 |  436 | `	return PH7_OK;` |
|    28 |  437 | `}` |
|     - |  438 | `/* int (*xSetenv)(const char *,const char *) */` |
|     2 |  439 | `static int UnixVfs_Setenv(const char *zName,const char *zValue)` |
|     - |  440 | `{` |
|     - |  441 | `   int rc;` |
|     2 |  442 | `   rc = setenv(zName,zValue,1);` |
|     2 |  443 | `   return rc == 0 ? PH7_OK : -1;` |
|     - |  444 | `}` |
|     - |  445 | `/* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|  3864 |  446 | `static int UnixVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|     - |  447 | `{` |
|     - |  448 | `	struct stat st;` |
|     - |  449 | `	void *pMap;` |
|     - |  450 | `	int fd;` |
|     - |  451 | `	int rc;` |
|     - |  452 | `	/* Open the file in a read-only mode */` |
|  3864 |  453 | `	fd = open(zPath,O_RDONLY);` |
|  3864 |  454 | `	if( fd < 0 ){` |
|     2 |  455 | `		return -1;` |
|     - |  456 | `	}` |
|     - |  457 | `	/* stat the handle */` |
|  3862 |  458 | `	fstat(fd,&st);` |
|     - |  459 | `	/* Obtain a memory view of the whole file */` |
|  3862 |  460 | `	pMap = mmap(0,st.st_size,PROT_READ,MAP_PRIVATE\|MAP_FILE,fd,0);` |
|  3862 |  461 | `	rc = PH7_OK;` |
|  3862 |  462 | `	if( pMap == MAP_FAILED ){` |
|   ! 0 |  463 | `		rc = -1;` |
|   ! 0 |  464 | `	}else{` |
|     - |  465 | `		/* Point to the memory view */` |
|  3862 |  466 | `		*ppMap = pMap;` |
|  3862 |  467 | `		*pSize = (ph7_int64)st.st_size;` |
|     - |  468 | `	}` |
|  3862 |  469 | `	close(fd);` |
|  3862 |  470 | `	return rc;` |
|  1932 |  471 | `}` |
|     - |  472 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|  3862 |  473 | `static void UnixVfs_Unmap(void *pView,ph7_int64 nSize)` |
|     - |  474 | `{` |
|  3862 |  475 | `	munmap(pView,(size_t)nSize);` |
|  3862 |  476 | `}` |
|     - |  477 | `/* void (*xTempDir)(ph7_context *) */` |
|   240 |  478 | `static void UnixVfs_TempDir(ph7_context *pCtx)` |
|     - |  479 | `{` |
|     - |  480 | `  const char *zDir;` |
|     - |  481 | `  /* php's php_get_temporary_directory: honour TMPDIR, else fall back to P_tmpdir` |
|     - |  482 | `   * ("/tmp" on Unix). PH7 also scanned /var/tmp, /usr/tmp, /usr/local/tmp first,` |
|     - |  483 | `   * which returned /var/tmp on a typical box where php returns /tmp — a divergence` |
|     - |  484 | `   * observable through sys_get_temp_dir()/tempnam()/session paths. */` |
|   240 |  485 | `  zDir = getenv("TMPDIR");` |
|   240 |  486 | `  if( zDir && zDir[0] != 0 && !access(zDir,07) ){` |
|     - |  487 | `	  /* php reports the temp dir WITHOUT a trailing separator; macOS's TMPDIR ends with` |
|     - |  488 | `	   * one, so returning it verbatim produced paths like "/var/.../T//file". */` |
|   120 |  489 | `	  int nDir = (int)strlen(zDir);` |
|   240 |  490 | `	  while( nDir > 1 && zDir[nDir-1] == '/' ){` |
|   120 |  491 | `		  nDir--;` |
|     - |  492 | `	  }` |
|   120 |  493 | `	  ph7_result_string(pCtx,zDir,nDir);` |
|   120 |  494 | `	  return;` |
|     - |  495 | `  }` |
|   120 |  496 | `  ph7_result_string(pCtx,"/tmp",(int)sizeof("/tmp")-1);` |
|   120 |  497 | `}` |
|     - |  498 | `/* unsigned int (*xProcessId)(void) */` |
|    84 |  499 | `static unsigned int UnixVfs_ProcessId(void)` |
|     - |  500 | `{` |
|    84 |  501 | `	return (unsigned int)getpid();` |
|     - |  502 | `}` |
|     - |  503 | `/* int (*xUid)(void) */` |
|     2 |  504 | `static int UnixVfs_uid(void)` |
|     - |  505 | `{` |
|     2 |  506 | `	return (int)getuid();` |
|     - |  507 | `}` |
|     - |  508 | `/* int (*xGid)(void) */` |
|     2 |  509 | `static int UnixVfs_gid(void)` |
|     - |  510 | `{` |
|     2 |  511 | `	return (int)getgid();` |
|     - |  512 | `}` |
|     - |  513 | `/* int (*xUmask)(int) */` |
|     8 |  514 | `static int UnixVfs_Umask(int new_mask)` |
|     - |  515 | `{` |
|     - |  516 | `	int old_mask;` |
|     8 |  517 | `	old_mask = umask(new_mask);` |
|     8 |  518 | `	return old_mask;` |
|     - |  519 | `}` |
|     - |  520 | `/* void (*xUsername)(ph7_context *) */` |
|     2 |  521 | `static void UnixVfs_Username(ph7_context *pCtx)` |
|     - |  522 | `{` |
|     - |  523 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  524 | `  struct passwd *pwd;` |
|     - |  525 | `  uid_t uid;` |
|     2 |  526 | `  uid = getuid();` |
|     2 |  527 | `  pwd = getpwuid(uid);   /* Try getting UID for username */` |
|     2 |  528 | `  if (pwd == 0) {` |
|   ! 0 |  529 | `    return;` |
|     - |  530 | `  }` |
|     - |  531 | `  /* Return the username */` |
|     2 |  532 | `  ph7_result_string(pCtx,pwd->pw_name,-1);` |
|     - |  533 | `#else` |
|     - |  534 | `  ph7_result_string(pCtx,"Unknown",-1);` |
|     - |  535 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     2 |  536 | `  return;` |
|     1 |  537 | `}` |
|     - |  538 | `/* int (*xLink)(const char *,const char *,int) */` |
|     8 |  539 | `static int UnixVfs_link(const char *zSrc,const char *zTarget,int is_sym)` |
|     - |  540 | `{` |
|     - |  541 | `	int rc;` |
|     8 |  542 | `	if( is_sym ){` |
|     - |  543 | `		/* Symbolic link */` |
|     6 |  544 | `		rc = symlink(zSrc,zTarget);` |
|     3 |  545 | `	}else{` |
|     - |  546 | `		/* Hard link */` |
|     2 |  547 | `		rc = link(zSrc,zTarget);` |
|     - |  548 | `	}` |
|     8 |  549 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  550 | `}` |
|     - |  551 | `/* int (*xChroot)(const char *) */` |
|     2 |  552 | `static int UnixVfs_chroot(const char *zRootDir)` |
|     - |  553 | `{` |
|     - |  554 | `	int rc;` |
|     2 |  555 | `	rc = chroot(zRootDir);` |
|     2 |  556 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  557 | `}` |
|     - |  558 | `/* Export the UNIX vfs */` |
|     - |  559 | `PH7_PRIVATE const ph7_vfs sUnixVfs = {` |
|     - |  560 | `	"Unix_vfs",` |
|     - |  561 | `	PH7_VFS_VERSION,` |
|     - |  562 | `	UnixVfs_chdir,    /* int (*xChdir)(const char *) */` |
|     - |  563 | `	UnixVfs_chroot,   /* int (*xChroot)(const char *); */` |
|     - |  564 | `	UnixVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|     - |  565 | `	UnixVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|     - |  566 | `	UnixVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|     - |  567 | `	UnixVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|     - |  568 | `	UnixVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|     - |  569 | `	UnixVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|     - |  570 | `	UnixVfs_Sleep,    /* int (*xSleep)(unsigned int) */` |
|     - |  571 | `	UnixVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|     - |  572 | `	UnixVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|     - |  573 | `	UnixVfs_Chmod, /*int (*xChmod)(const char *,int)*/` |
|     - |  574 | `	UnixVfs_Chown, /*int (*xChown)(const char *,const char *)*/` |
|     - |  575 | `	UnixVfs_Chgrp, /*int (*xChgrp)(const char *,const char *)*/` |
|     - |  576 | `	UnixVfs_FreeSpace,  /* ph7_int64 (*xFreeSpace)(const char *) */` |
|     - |  577 | `	UnixVfs_TotalSpace, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|     - |  578 | `	UnixVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|     - |  579 | `	UnixVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|     - |  580 | `	UnixVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|     - |  581 | `	UnixVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|     - |  582 | `	UnixVfs_Stat,  /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|     - |  583 | `	UnixVfs_lStat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|     - |  584 | `	UnixVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|     - |  585 | `	UnixVfs_islink,     /* int (*xIslink)(const char *) */` |
|     - |  586 | `	UnixVfs_isreadable, /* int (*xReadable)(const char *) */` |
|     - |  587 | `	UnixVfs_iswritable, /* int (*xWritable)(const char *) */` |
|     - |  588 | `	UnixVfs_isexecutable,/* int (*xExecutable)(const char *) */` |
|     - |  589 | `	UnixVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|     - |  590 | `	UnixVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|     - |  591 | `	UnixVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|     - |  592 | `	UnixVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|     - |  593 | `	UnixVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|     - |  594 | `	UnixVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|     - |  595 | `	UnixVfs_link,       /* int (*xLink)(const char *,const char *,int) */` |
|     - |  596 | `	UnixVfs_Umask,      /* int (*xUmask)(int) */` |
|     - |  597 | `	UnixVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|     - |  598 | `	UnixVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|     - |  599 | `	UnixVfs_uid, /* int (*xUid)(void) */` |
|     - |  600 | `	UnixVfs_gid, /* int (*xGid)(void) */` |
|     - |  601 | `	UnixVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|     - |  602 | `	0 /* int (*xExec)(const char *,ph7_context *) */` |
|     - |  603 | `};` |
|     - |  604 | `/* UNIX File IO */` |
|     - |  605 | `#define PH7_UNIX_OPEN_MODE	0640 /* Default open mode */` |
|     - |  606 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
| 30332 |  607 | `static int UnixFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|     - |  608 | `{` |
| 30332 |  609 | `	int iOpen = O_RDONLY;` |
|     - |  610 | `	int fd;` |
|     - |  611 | `	/* Set the desired flags according to the open mode */` |
| 30332 |  612 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|     - |  613 | `		/* Open existing file, or create if it doesn't exist */` |
| 13724 |  614 | `		iOpen = O_CREAT;` |
| 13724 |  615 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|     - |  616 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
| 13724 |  617 | `			iOpen \|= O_TRUNC;` |
|  6862 |  618 | `			SXUNUSED(pResource); /* cc warning */` |
|  6862 |  619 | `		}` |
| 23470 |  620 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|     - |  621 | `		/* Creates a new file, only if it does not already exist.` |
|     - |  622 | `		* If the file exists, it fails.` |
|     - |  623 | `		*/` |
|   132 |  624 | `		iOpen = O_CREAT\|O_EXCL;` |
| 16542 |  625 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|     - |  626 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|     - |  627 | `		 * The file must exist.` |
|     - |  628 | `		 */` |
|   ! 0 |  629 | `		iOpen = O_RDWR\|O_TRUNC;` |
|   ! 0 |  630 | `	}` |
| 30332 |  631 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|     - |  632 | `		/* Read+Write access */` |
| 13706 |  633 | `		iOpen &= ~O_RDONLY;` |
| 13706 |  634 | `		iOpen \|= O_RDWR;` |
| 23479 |  635 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|     - |  636 | `		/* Write only access */` |
|   156 |  637 | `		iOpen &= ~O_RDONLY;` |
|   156 |  638 | `		iOpen \|= O_WRONLY;` |
|    78 |  639 | `	}` |
| 30332 |  640 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|     - |  641 | `		/* Append mode */` |
|   ! 0 |  642 | `		iOpen \|= O_APPEND;` |
|   ! 0 |  643 | `	}` |
|     - |  644 | `#ifdef O_TEMP` |
|     - |  645 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|     - |  646 | `		/* File is temporary */` |
|     - |  647 | `		iOpen \|= O_TEMP;` |
|     - |  648 | `	}` |
|     - |  649 | `#endif` |
|     - |  650 | `	/* Open the file now */` |
| 30332 |  651 | `	fd = open(zPath,iOpen,PH7_UNIX_OPEN_MODE);` |
| 30332 |  652 | `	if( fd < 0 ){` |
|     - |  653 | `		/* IO error */` |
|    18 |  654 | `		return -1;` |
|     - |  655 | `	}` |
|     - |  656 | `	/* Save the handle */` |
| 30314 |  657 | `	*ppHandle = SX_INT_TO_PTR(fd);` |
| 30314 |  658 | `	return PH7_OK;` |
| 15166 |  659 | `}` |
|     - |  660 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|  1064 |  661 | `static int UnixDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|     - |  662 | `{` |
|     - |  663 | `	DIR *pDir;` |
|     - |  664 | `	/* Open the target directory */` |
|  1064 |  665 | `	pDir = opendir(zPath);` |
|  1064 |  666 | `	if( pDir == 0 ){` |
|   ! 0 |  667 | `		SXUNUSED(pResource); /* Compiler warning */` |
|   ! 0 |  668 | `		return -1;` |
|     - |  669 | `	}` |
|     - |  670 | `	/* Save our structure */` |
|  1064 |  671 | `	*ppHandle = pDir;` |
|  1064 |  672 | `	return PH7_OK;` |
|   532 |  673 | `}` |
|     - |  674 | `/* void (*xCloseDir)(void *) */` |
|  1064 |  675 | `static void UnixDir_Close(void *pUserData)` |
|     - |  676 | `{` |
|  1064 |  677 | `	closedir((DIR *)pUserData);` |
|  1064 |  678 | `}` |
|     - |  679 | `/* void (*xClose)(void *); */` |
| 30340 |  680 | `static void UnixFile_Close(void *pUserData)` |
|     - |  681 | `{` |
| 30340 |  682 | `	close(SX_PTR_TO_INT(pUserData));` |
| 30340 |  683 | `}` |
|     - |  684 | `/* int (*xReadDir)(void *,ph7_context *) */` |
| 10972 |  685 | `static int UnixDir_Read(void *pUserData,ph7_context *pCtx)` |
|     - |  686 | `{` |
| 10972 |  687 | `	DIR *pDir = (DIR *)pUserData;` |
|     - |  688 | `	struct dirent *pEntry;` |
|     - |  689 | `	char *zName;` |
|     - |  690 | `	sxu32 n;` |
|     - |  691 | `	/* php's readdir() yields every entry including '.' and '..' */` |
| 10972 |  692 | `	pEntry = readdir(pDir);` |
| 10972 |  693 | `	if( pEntry == 0 ){` |
|     - |  694 | `		/* No more entries to process */` |
|  1060 |  695 | `		return -1;` |
|     - |  696 | `	}` |
|  9912 |  697 | `	zName = pEntry->d_name;` |
|  9912 |  698 | `	n = SyStrlen(zName);` |
|     - |  699 | `	/* Return the current file name */` |
|  9912 |  700 | `	ph7_result_string(pCtx,zName,(int)n);` |
|  9912 |  701 | `	return PH7_OK;` |
|  5486 |  702 | `}` |
|     - |  703 | `/* void (*xRewindDir)(void *) */` |
|     2 |  704 | `static void UnixDir_Rewind(void *pUserData)` |
|     - |  705 | `{` |
|     2 |  706 | `	rewinddir((DIR *)pUserData);` |
|     2 |  707 | `}` |
|     - |  708 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
| 32884 |  709 | `static ph7_int64 UnixFile_Read(void *pUserData,void *pBuffer,ph7_int64 nDatatoRead)` |
|     - |  710 | `{` |
|     - |  711 | `	ssize_t nRd;` |
| 32884 |  712 | `	nRd = read(SX_PTR_TO_INT(pUserData),pBuffer,(size_t)nDatatoRead);` |
| 32884 |  713 | `	if( nRd < 1 ){` |
|     - |  714 | `		/* EOF or IO error */` |
| 16442 |  715 | `		return -1;` |
|     - |  716 | `	}` |
| 16442 |  717 | `	return (ph7_int64)nRd;` |
| 16442 |  718 | `}` |
|     - |  719 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
| 13746 |  720 | `static ph7_int64 UnixFile_Write(void *pUserData,const void *pBuffer,ph7_int64 nWrite)` |
|     - |  721 | `{` |
| 13746 |  722 | `	const char *zData = (const char *)pBuffer;` |
| 13746 |  723 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|     - |  724 | `	ph7_int64 nCount;` |
|     - |  725 | `	ssize_t nWr;` |
| 13746 |  726 | `	nCount = 0;` |
| 13746 |  727 | `	for(;;){` |
| 27492 |  728 | `		if( nWrite < 1 ){` |
| 13746 |  729 | `			break;` |
|     - |  730 | `		}` |
| 13746 |  731 | `		nWr = write(fd,zData,(size_t)nWrite);` |
| 13746 |  732 | `		if( nWr < 1 ){` |
|     - |  733 | `			/* IO error */` |
|   ! 0 |  734 | `			break;` |
|     - |  735 | `		}` |
| 13746 |  736 | `		nWrite -= nWr;` |
| 13746 |  737 | `		nCount += nWr;` |
| 13746 |  738 | `		zData += nWr;` |
|     - |  739 | `	}` |
| 13746 |  740 | `	if( nWrite > 0 ){` |
|   ! 0 |  741 | `		return -1;` |
|     - |  742 | `	}` |
| 13746 |  743 | `	return nCount;` |
|  6873 |  744 | `}` |
|     - |  745 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|     6 |  746 | `static int UnixFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|     - |  747 | `{` |
|     - |  748 | `	off_t iNew;` |
|     6 |  749 | `	switch(whence){` |
|   ! 0 |  750 | `	case 1:/*SEEK_CUR*/` |
|   ! 0 |  751 | `		whence = SEEK_CUR;` |
|   ! 0 |  752 | `		break;` |
|   ! 0 |  753 | `	case 2: /* SEEK_END */` |
|   ! 0 |  754 | `		whence = SEEK_END;` |
|   ! 0 |  755 | `		break;` |
|     6 |  756 | `	case 0: /* SEEK_SET */` |
|     - |  757 | `	default:` |
|     6 |  758 | `		whence = SEEK_SET;` |
|     6 |  759 | `		break;` |
|     - |  760 | `	}` |
|     6 |  761 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),(off_t)iOfft,whence);` |
|     6 |  762 | `	if( iNew < 0 ){` |
|   ! 0 |  763 | `		return -1;` |
|     - |  764 | `	}` |
|     6 |  765 | `	return PH7_OK;` |
|     3 |  766 | `}` |
|     - |  767 | `/* int (*xLock)(void *,int) */` |
|     4 |  768 | `static int UnixFile_Lock(void *pUserData,int lock_type)` |
|     - |  769 | `{` |
|     4 |  770 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|     4 |  771 | `	int rc = PH7_OK; /* cc warning */` |
|     4 |  772 | `	if( lock_type < 0 ){` |
|     - |  773 | `		/* Unlock the file */` |
|     2 |  774 | `		rc = flock(fd,LOCK_UN);` |
|     1 |  775 | `	}else{` |
|     2 |  776 | `		if( lock_type == 1 ){` |
|     - |  777 | `			/* Exculsive lock */` |
|     2 |  778 | `			rc = flock(fd,LOCK_EX);` |
|     1 |  779 | `		}else{` |
|     - |  780 | `			/* Shared lock */` |
|   ! 0 |  781 | `			rc = flock(fd,LOCK_SH);` |
|     - |  782 | `		}` |
|     - |  783 | `	}` |
|     4 |  784 | `	return !rc ? PH7_OK : -1;` |
|     - |  785 | `}` |
|     - |  786 | `/* ph7_int64 (*xTell)(void *) */` |
|     6 |  787 | `static ph7_int64 UnixFile_Tell(void *pUserData)` |
|     - |  788 | `{` |
|     - |  789 | `	off_t iNew;` |
|     6 |  790 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),0,SEEK_CUR);` |
|     6 |  791 | `	return (ph7_int64)iNew;` |
|     - |  792 | `}` |
|     - |  793 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|     6 |  794 | `static int UnixFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|     - |  795 | `{` |
|     - |  796 | `	int rc;` |
|     6 |  797 | `	rc = ftruncate(SX_PTR_TO_INT(pUserData),(off_t)nOfft);` |
|     6 |  798 | `	if( rc != 0 ){` |
|   ! 0 |  799 | `		return -1;` |
|     - |  800 | `	}` |
|     6 |  801 | `	return PH7_OK;` |
|     3 |  802 | `}` |
|     - |  803 | `/* int (*xSync)(void *); */` |
|     2 |  804 | `static int UnixFile_Sync(void *pUserData)` |
|     - |  805 | `{` |
|     - |  806 | `	int rc;` |
|     2 |  807 | `	rc = fsync(SX_PTR_TO_INT(pUserData));` |
|     2 |  808 | `	return rc == 0 ? PH7_OK : - 1;` |
|     - |  809 | `}` |
|     - |  810 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|     2 |  811 | `static int UnixFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  812 | `{` |
|     - |  813 | `	struct stat st;` |
|     - |  814 | `	int rc;` |
|     2 |  815 | `	rc = fstat(SX_PTR_TO_INT(pUserData),&st);` |
|     2 |  816 | `	if( rc != 0 ){` |
|   ! 0 |  817 | `	 return -1;` |
|     - |  818 | `	}` |
|     - |  819 | `	/* dev */` |
|     2 |  820 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|     2 |  821 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  822 | `	/* ino */` |
|     2 |  823 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|     2 |  824 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  825 | `	/* mode */` |
|     2 |  826 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|     2 |  827 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  828 | `	/* nlink */` |
|     2 |  829 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|     2 |  830 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  831 | `	/* uid,gid,rdev */` |
|     2 |  832 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|     2 |  833 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|     2 |  834 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|     2 |  835 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|     2 |  836 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|     2 |  837 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  838 | `	/* size */` |
|     2 |  839 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|     2 |  840 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  841 | `	/* atime */` |
|     2 |  842 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|     2 |  843 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  844 | `	/* mtime */` |
|     2 |  845 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|     2 |  846 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  847 | `	/* ctime */` |
|     2 |  848 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|     2 |  849 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  850 | `	/* blksize,blocks */` |
|     2 |  851 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|     2 |  852 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|     2 |  853 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|     2 |  854 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|     2 |  855 | `	return PH7_OK;` |
|     1 |  856 | `}` |
|     - |  857 | `/* Export the file:// stream */` |
|     - |  858 | `PH7_PRIVATE const ph7_io_stream sUnixFileStream = {` |
|     - |  859 | `	"file", /* Stream name */` |
|     - |  860 | `	PH7_IO_STREAM_VERSION,` |
|     - |  861 | `	UnixFile_Open,  /* xOpen */` |
|     - |  862 | `	UnixDir_Open,   /* xOpenDir */` |
|     - |  863 | `	UnixFile_Close, /* xClose */` |
|     - |  864 | `	UnixDir_Close,  /* xCloseDir */` |
|     - |  865 | `	UnixFile_Read,  /* xRead */` |
|     - |  866 | `	UnixDir_Read,   /* xReadDir */` |
|     - |  867 | `	UnixFile_Write, /* xWrite */` |
|     - |  868 | `	UnixFile_Seek,  /* xSeek */` |
|     - |  869 | `	UnixFile_Lock,  /* xLock */` |
|     - |  870 | `	UnixDir_Rewind, /* xRewindDir */` |
|     - |  871 | `	UnixFile_Tell,  /* xTell */` |
|     - |  872 | `	UnixFile_Trunc, /* xTrunc */` |
|     - |  873 | `	UnixFile_Sync,  /* xSeek */` |
|     - |  874 | `	UnixFile_Stat   /* xStat */` |
|     - |  875 | `};` |
|     - |  876 | `#endif /* __UNIXES__ */` |
|     - |  877 |  |
