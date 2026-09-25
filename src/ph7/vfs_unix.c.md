# src/ph7/vfs_unix.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 477/511 lines (93.35%)

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
| 65804 |   40 | `static const char * UnixVfsLocalPath(const char *zPath)` |
|     - |   41 | `{` |
|     - |   42 | `	/* One rule for the whole engine: PH7_VmFileUrlLocalPath() is the same strip` |
|     - |   43 | ``	 * the stream lookup applies, so a `file://` URL means the same file whether`` |
|     - |   44 | `	 * it is opened or stat'ed. This used to be a second, shorter copy. */` |
| 65804 |   45 | `	return PH7_VmFileUrlLocalPath(zPath);` |
|     - |   46 | `}` |
|     - |   47 | `/* int (*xchdir)(const char *) */` |
| 15046 |   48 | `static int UnixVfs_chdir(const char *zPath)` |
|     - |   49 | `{` |
|     - |   50 | `  int rc;` |
| 15046 |   51 | `  rc = chdir(zPath);` |
| 15046 |   52 | `  return rc == 0 ? PH7_OK : -1;` |
|     - |   53 | `}` |
|     - |   54 | `/* int (*xGetcwd)(ph7_context *) */` |
|    18 |   55 | `static int UnixVfs_getcwd(ph7_context *pCtx)` |
|     - |   56 | `{` |
|     - |   57 | `	char zBuf[4096];` |
|     - |   58 | `	char *zDir;` |
|     - |   59 | `	/* Get the current directory */` |
|    18 |   60 | `	zDir = getcwd(zBuf,sizeof(zBuf));` |
|    18 |   61 | `	if( zDir == 0 ){` |
|   ! 0 |   62 | `	  return -1;` |
|     - |   63 | `    }` |
|    18 |   64 | `	ph7_result_string(pCtx,zDir,-1/*Compute length automatically*/);` |
|    18 |   65 | `	return PH7_OK;` |
|     9 |   66 | `}` |
|     - |   67 | `/* int (*xMkdir)(const char *,int,int)` |
|     - |   68 | ` * ONE level. php builds a tree in the WRAPPER rather than in the syscall, and so` |
|     - |   69 | `` * does PHL now (VfsMkdirRecursive in vfs.c), so `recursive` never arrives set. */`` |
|   148 |   70 | `static int UnixVfs_mkdir(const char *zPath,int mode,int recursive)` |
|     - |   71 | `{` |
|   148 |   72 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |   73 | `	int rc;` |
|   148 |   74 | `        rc = mkdir(zPath,mode);` |
|    74 |   75 | `	SXUNUSED(recursive); /* cc warning */` |
|   148 |   76 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   77 | `}` |
|     - |   78 | `/* int (*xRmdir)(const char *) */` |
|   138 |   79 | `static int UnixVfs_rmdir(const char *zPath)` |
|     - |   80 | `{` |
|   138 |   81 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |   82 | `	int rc;` |
|   138 |   83 | `	rc = rmdir(zPath);` |
|   138 |   84 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |   85 | `}` |
|     - |   86 | `/* int (*xIsdir)(const char *) */` |
| 10512 |   87 | `static int UnixVfs_isdir(const char *zPath)` |
|     - |   88 | `{` |
| 10512 |   89 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |   90 | `	struct stat st;` |
|     - |   91 | `	int rc;` |
| 10512 |   92 | `	rc = stat(zPath,&st);` |
| 10512 |   93 | `	if( rc != 0 ){` |
|    16 |   94 | `	 return -1;` |
|     - |   95 | `	}` |
| 10496 |   96 | `	rc = S_ISDIR(st.st_mode);` |
| 10496 |   97 | `	return rc ? PH7_OK : -1 ;` |
|  5256 |   98 | `}` |
|     - |   99 | `/* int (*xRename)(const char *,const char *) */` |
|     2 |  100 | `static int UnixVfs_Rename(const char *zOld,const char *zNew)` |
|     - |  101 | `{` |
|     2 |  102 | `	zOld = UnixVfsLocalPath(zOld);` |
|     2 |  103 | `	zNew = UnixVfsLocalPath(zNew);` |
|     - |  104 | `	int rc;` |
|     2 |  105 | `	rc = rename(zOld,zNew);` |
|     2 |  106 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  107 | `}` |
|     - |  108 | `/* int (*xRealpath)(const char *,ph7_context *) */` |
|    26 |  109 | `static int UnixVfs_Realpath(const char *zPath,ph7_context *pCtx)` |
|     - |  110 | `{` |
|     - |  111 | `#ifndef PH7_UNIX_OLD_LIBC` |
|     - |  112 | `	char *zReal;` |
|    26 |  113 | `	zReal = realpath(zPath,0);` |
|    26 |  114 | `	if( zReal == 0 ){` |
|     4 |  115 | `	  return -1;` |
|     - |  116 | `	}` |
|    22 |  117 | `	ph7_result_string(pCtx,zReal,-1/*Compute length automatically*/);` |
|     - |  118 | `        /* Release the allocated buffer */` |
|    22 |  119 | `	free(zReal);` |
|    22 |  120 | `	return PH7_OK;` |
|     - |  121 | `#else` |
|     - |  122 | `    zPath = 0; /* cc warning */` |
|     - |  123 | `    pCtx = 0;` |
|     - |  124 | `    return -1;` |
|     - |  125 | `#endif` |
|    13 |  126 | `}` |
|     - |  127 | `/* int (*xSleep)(unsigned int) */` |
|    84 |  128 | `static int UnixVfs_Sleep(unsigned int uSec)` |
|     - |  129 | `{` |
|    84 |  130 | `	usleep(uSec);` |
|    84 |  131 | `	return PH7_OK;` |
|     - |  132 | `}` |
|     - |  133 | `/* int (*xUnlink)(const char *) */` |
| 40560 |  134 | `static int UnixVfs_unlink(const char *zPath)` |
|     - |  135 | `{` |
| 40560 |  136 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  137 | `	int rc;` |
| 40560 |  138 | `	rc = unlink(zPath);` |
| 40560 |  139 | `	return rc == 0 ? PH7_OK : -1 ;` |
|     - |  140 | `}` |
|     - |  141 | `/* int (*xFileExists)(const char *) */` |
|   622 |  142 | `static int UnixVfs_FileExists(const char *zPath)` |
|     - |  143 | `{` |
|   622 |  144 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  145 | `	int rc;` |
|   622 |  146 | `	rc = access(zPath,F_OK);` |
|   622 |  147 | `	return rc == 0 ? PH7_OK : -1;` |
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
|    20 |  171 | `static ph7_int64 UnixVfs_FileSize(const char *zPath)` |
|     - |  172 | `{` |
|    20 |  173 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  174 | `	struct stat st;` |
|     - |  175 | `	int rc;` |
|    20 |  176 | `	rc = stat(zPath,&st);` |
|    20 |  177 | `	if( rc != 0 ){` |
|     2 |  178 | `	 return -1;` |
|     - |  179 | `	}` |
|    18 |  180 | `	return (ph7_int64)st.st_size;` |
|    10 |  181 | `}` |
|     - |  182 | `/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|    26 |  183 | `static int UnixVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)` |
|     - |  184 | `{` |
|    26 |  185 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  186 | `	struct utimbuf ut;` |
|     - |  187 | `	int rc;` |
|     - |  188 | `	/* Both stamps are always real values here — the builtin resolves php's "now"` |
|     - |  189 | `	 * default, because a NEGATIVE timestamp is legal and so cannot be a sentinel. */` |
|    26 |  190 | `	ut.actime  = (time_t)access_time;` |
|    26 |  191 | `	ut.modtime = (time_t)touch_time;` |
|    26 |  192 | `	rc = utime(zPath,&ut);` |
|    26 |  193 | `	if( rc != 0 ){` |
|     - |  194 | `		/* php's touch() CREATES the file when it is missing — that is the idiom's` |
|     - |  195 | ``		 * main use, and utime() alone fails ENOENT there, so `touch($new)` answered`` |
|     - |  196 | `		 * false and created nothing. Try the create only after utime failed, like` |
|     - |  197 | `		 * php: no extra stat on the common path, and no access(2) real-uid check. */` |
|    22 |  198 | `		int fd = open(zPath,O_WRONLY\|O_CREAT,0666);` |
|    22 |  199 | `		if( fd < 0 ){` |
|   ! 0 |  200 | `			return -1;` |
|     - |  201 | `		}` |
|    22 |  202 | `		close(fd);` |
|    22 |  203 | `		rc = utime(zPath,&ut);` |
|    22 |  204 | `		if( rc != 0 ){` |
|   ! 0 |  205 | `			return -1;` |
|     - |  206 | `		}` |
|    11 |  207 | `	}` |
|    26 |  208 | `	return PH7_OK;` |
|    13 |  209 | `}` |
|     - |  210 | `/* ph7_int64 (*xFileAtime)(const char *) */` |
|     8 |  211 | `static ph7_int64 UnixVfs_FileAtime(const char *zPath)` |
|     - |  212 | `{` |
|     8 |  213 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  214 | `	struct stat st;` |
|     - |  215 | `	int rc;` |
|     8 |  216 | `	rc = stat(zPath,&st);` |
|     8 |  217 | `	if( rc != 0 ){` |
|     2 |  218 | `	 return -1;` |
|     - |  219 | `	}` |
|     6 |  220 | `	return (ph7_int64)st.st_atime;` |
|     4 |  221 | `}` |
|     - |  222 | `/* ph7_int64 (*xFileMtime)(const char *) */` |
|    26 |  223 | `static ph7_int64 UnixVfs_FileMtime(const char *zPath)` |
|     - |  224 | `{` |
|    26 |  225 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  226 | `	struct stat st;` |
|     - |  227 | `	int rc;` |
|    26 |  228 | `	rc = stat(zPath,&st);` |
|    26 |  229 | `	if( rc != 0 ){` |
|     2 |  230 | `	 return -1;` |
|     - |  231 | `	}` |
|    24 |  232 | `	return (ph7_int64)st.st_mtime;` |
|    12 |  233 | `}` |
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
|    84 |  247 | `static int UnixVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  248 | `{` |
|    84 |  249 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  250 | `	struct stat st;` |
|     - |  251 | `	int rc;` |
|    84 |  252 | `	rc = stat(zPath,&st);` |
|    84 |  253 | `	if( rc != 0 ){` |
|    26 |  254 | `	 return -1;` |
|     - |  255 | `	}` |
|     - |  256 | `	/* dev */` |
|    58 |  257 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|    58 |  258 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  259 | `	/* ino */` |
|    58 |  260 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|    58 |  261 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  262 | `	/* mode */` |
|    58 |  263 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|    58 |  264 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  265 | `	/* nlink */` |
|    58 |  266 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|    58 |  267 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  268 | `	/* uid,gid,rdev */` |
|    58 |  269 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|    58 |  270 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|    58 |  271 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|    58 |  272 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|    58 |  273 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|    58 |  274 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  275 | `	/* size */` |
|    58 |  276 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|    58 |  277 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  278 | `	/* atime */` |
|    58 |  279 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|    58 |  280 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  281 | `	/* mtime */` |
|    58 |  282 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|    58 |  283 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  284 | `	/* ctime */` |
|    58 |  285 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|    58 |  286 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  287 | `	/* blksize,blocks */` |
|    58 |  288 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|    58 |  289 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|    58 |  290 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|    58 |  291 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|    58 |  292 | `	return PH7_OK;` |
|    42 |  293 | `}` |
|     - |  294 | `/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|     6 |  295 | `static int UnixVfs_lStat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  296 | `{` |
|     6 |  297 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  298 | `	struct stat st;` |
|     - |  299 | `	int rc;` |
|     6 |  300 | `	rc = lstat(zPath,&st);` |
|     6 |  301 | `	if( rc != 0 ){` |
|     2 |  302 | `	 return -1;` |
|     - |  303 | `	}` |
|     - |  304 | `	/* dev */` |
|     4 |  305 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|     4 |  306 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  307 | `	/* ino */` |
|     4 |  308 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|     4 |  309 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  310 | `	/* mode */` |
|     4 |  311 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|     4 |  312 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  313 | `	/* nlink */` |
|     4 |  314 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|     4 |  315 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  316 | `	/* uid,gid,rdev */` |
|     4 |  317 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|     4 |  318 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|     4 |  319 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|     4 |  320 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|     4 |  321 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|     4 |  322 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  323 | `	/* size */` |
|     4 |  324 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|     4 |  325 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  326 | `	/* atime */` |
|     4 |  327 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|     4 |  328 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  329 | `	/* mtime */` |
|     4 |  330 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|     4 |  331 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  332 | `	/* ctime */` |
|     4 |  333 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|     4 |  334 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  335 | `	/* blksize,blocks */` |
|     4 |  336 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|     4 |  337 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|     4 |  338 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|     4 |  339 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|     4 |  340 | `	return PH7_OK;` |
|     3 |  341 | `}` |
|     - |  342 | `/* int (*xChmod)(const char *,int) */` |
|   272 |  343 | `static int UnixVfs_Chmod(const char *zPath,int mode)` |
|     - |  344 | `{` |
|   272 |  345 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  346 | `    int rc;` |
|   272 |  347 | `    rc = chmod(zPath,(mode_t)mode);` |
|   272 |  348 | `    return rc == 0 ? PH7_OK : - 1;` |
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
|  8134 |  407 | `static int UnixVfs_isfile(const char *zPath)` |
|     - |  408 | `{` |
|  8134 |  409 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  410 | `	struct stat st;` |
|     - |  411 | `	int rc;` |
|  8134 |  412 | `	rc = stat(zPath,&st);` |
|  8134 |  413 | `	if( rc != 0 ){` |
|    84 |  414 | `	 return -1;` |
|     - |  415 | `	}` |
|  8050 |  416 | `	rc = S_ISREG(st.st_mode);` |
|  8050 |  417 | `	return rc ? PH7_OK : -1 ;` |
|  4067 |  418 | `}` |
|     - |  419 | `/* int (*xIslink)(const char *) */` |
|    18 |  420 | `static int UnixVfs_islink(const char *zPath)` |
|     - |  421 | `{` |
|    18 |  422 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  423 | `	struct stat st;` |
|     - |  424 | `	int rc;` |
|     - |  425 | `	/* LSTAT, not stat: stat() FOLLOWS the link and answers about its target, so` |
|     - |  426 | `	 * S_ISLNK could never be true here and is_link() was false for every symlink` |
|     - |  427 | `	 * in existence. php's is_link() is php_stat(FS_IS_LINK), which lstats. */` |
|    18 |  428 | `	rc = lstat(zPath,&st);` |
|    18 |  429 | `	if( rc != 0 ){` |
|     4 |  430 | `	 return -1;` |
|     - |  431 | `	}` |
|    14 |  432 | `	rc = S_ISLNK(st.st_mode);` |
|    14 |  433 | `	return rc ? PH7_OK : -1 ;` |
|     9 |  434 | `}` |
|     - |  435 | `/* int (*xReadable)(const char *) */` |
|     6 |  436 | `static int UnixVfs_isreadable(const char *zPath)` |
|     - |  437 | `{` |
|     6 |  438 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  439 | `	int rc;` |
|     6 |  440 | `	rc = access(zPath,R_OK);` |
|     6 |  441 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  442 | `}` |
|     - |  443 | `/* int (*xWritable)(const char *) */` |
|     8 |  444 | `static int UnixVfs_iswritable(const char *zPath)` |
|     - |  445 | `{` |
|     8 |  446 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  447 | `	int rc;` |
|     8 |  448 | `	rc = access(zPath,W_OK);` |
|     8 |  449 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  450 | `}` |
|     - |  451 | `/* int (*xExecutable)(const char *) */` |
|     6 |  452 | `static int UnixVfs_isexecutable(const char *zPath)` |
|     - |  453 | `{` |
|     6 |  454 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  455 | `	int rc;` |
|     6 |  456 | `	rc = access(zPath,X_OK);` |
|     6 |  457 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  458 | `}` |
|     - |  459 | `/* int (*xFiletype)(const char *,ph7_context *) */` |
|    24 |  460 | `static int UnixVfs_Filetype(const char *zPath,ph7_context *pCtx)` |
|     - |  461 | `{` |
|    24 |  462 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  463 | `	struct stat st;` |
|     - |  464 | `	int rc;` |
|     - |  465 | `	/* php's FS_TYPE lstats, which is why filetype() answers "link" for a symlink` |
|     - |  466 | `	 * and "file" for what it points at. stat() here made the "link" arm below` |
|     - |  467 | `	 * unreachable. */` |
|    24 |  468 | `    rc = lstat(zPath,&st);` |
|    24 |  469 | `	if( rc != 0 ){` |
|     - |  470 | `	  /* Expand 'unknown' */` |
|     4 |  471 | `	  ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);` |
|     4 |  472 | `	  return -1;` |
|     - |  473 | `	}` |
|    20 |  474 | `	if(S_ISREG(st.st_mode) ){` |
|     8 |  475 | `		ph7_result_string(pCtx,"file",sizeof("file")-1);` |
|    16 |  476 | `	}else if(S_ISDIR(st.st_mode)){` |
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
|    20 |  489 | `	return PH7_OK;` |
|    12 |  490 | `}` |
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
|    94 |  510 | `static int UnixVfs_Getenv(const char *zVar,ph7_context *pCtx)` |
|     - |  511 | `{` |
|     - |  512 | `	char *zEnv;` |
|    94 |  513 | `	zEnv = getenv(zVar);` |
|    94 |  514 | `	if( zEnv == 0 ){` |
|    14 |  515 | `	  return -1;` |
|     - |  516 | `	}` |
|    80 |  517 | `	ph7_result_string(pCtx,zEnv,-1/*Compute length automatically*/);` |
|    80 |  518 | `	return PH7_OK;` |
|    47 |  519 | `}` |
|     - |  520 | `/* int (*xSetenv)(const char *,const char *) */` |
|    46 |  521 | `static int UnixVfs_Setenv(const char *zName,const char *zValue)` |
|     - |  522 | `{` |
|     - |  523 | `   int rc;` |
|    46 |  524 | `   if( zValue == 0 ){` |
|     - |  525 | `     /* php's putenv("NAME") with no '=' REMOVES the variable. */` |
|    24 |  526 | `     rc = unsetenv(zName);` |
|    24 |  527 | `     return rc == 0 ? PH7_OK : -1;` |
|     - |  528 | `   }` |
|    22 |  529 | `   rc = setenv(zName,zValue,1);` |
|    22 |  530 | `   return rc == 0 ? PH7_OK : -1;` |
|    23 |  531 | `}` |
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
|  5106 |  563 | `static int UnixVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)` |
|     - |  564 | `{` |
|  5106 |  565 | `	zPath = UnixVfsLocalPath(zPath);` |
|     - |  566 | `	struct stat st;` |
|     - |  567 | `	void *pMap;` |
|     - |  568 | `	int fd;` |
|     - |  569 | `	int rc;` |
|     - |  570 | `	/* Open the file in a read-only mode */` |
|  5106 |  571 | `	fd = open(zPath,O_RDONLY);` |
|  5106 |  572 | `	if( fd < 0 ){` |
|   ! 0 |  573 | `		return -1;` |
|     - |  574 | `	}` |
|     - |  575 | `	/* stat the handle */` |
|  5106 |  576 | `	fstat(fd,&st);` |
|     - |  577 | `	/* Obtain a memory view of the whole file */` |
|  5106 |  578 | `	pMap = mmap(0,st.st_size,PROT_READ,MAP_PRIVATE\|MAP_FILE,fd,0);` |
|  5106 |  579 | `	rc = PH7_OK;` |
|  5106 |  580 | `	if( pMap == MAP_FAILED ){` |
|   ! 0 |  581 | `		rc = -1;` |
|   ! 0 |  582 | `	}else{` |
|     - |  583 | `		/* Point to the memory view */` |
|  5106 |  584 | `		*ppMap = pMap;` |
|  5106 |  585 | `		*pSize = (ph7_int64)st.st_size;` |
|     - |  586 | `	}` |
|  5106 |  587 | `	close(fd);` |
|  5106 |  588 | `	return rc;` |
|  2553 |  589 | `}` |
|     - |  590 | `/* void (*xUnmap)(void *,ph7_int64)  */` |
|  5106 |  591 | `static void UnixVfs_Unmap(void *pView,ph7_int64 nSize)` |
|     - |  592 | `{` |
|  5106 |  593 | `	munmap(pView,(size_t)nSize);` |
|  5106 |  594 | `}` |
|     - |  595 | `/* void (*xTempDir)(ph7_context *) */` |
|   440 |  596 | `static void UnixVfs_TempDir(ph7_context *pCtx)` |
|     - |  597 | `{` |
|     - |  598 | `  const char *zDir;` |
|     - |  599 | `  /* php's php_get_temporary_directory: honour TMPDIR, else fall back to P_tmpdir` |
|     - |  600 | `   * ("/tmp" on Unix). PH7 also scanned /var/tmp, /usr/tmp, /usr/local/tmp first,` |
|     - |  601 | `   * which returned /var/tmp on a typical box where php returns /tmp — a divergence` |
|     - |  602 | `   * observable through sys_get_temp_dir()/tempnam()/session paths. */` |
|   440 |  603 | `  zDir = getenv("TMPDIR");` |
|   440 |  604 | `  if( zDir && zDir[0] != 0 && !access(zDir,07) ){` |
|     - |  605 | `	  /* php reports the temp dir WITHOUT a trailing separator; macOS's TMPDIR ends with` |
|     - |  606 | `	   * one, so returning it verbatim produced paths like "/var/.../T//file". */` |
|   220 |  607 | `	  int nDir = (int)strlen(zDir);` |
|   440 |  608 | `	  while( nDir > 1 && zDir[nDir-1] == '/' ){` |
|   220 |  609 | `		  nDir--;` |
|     - |  610 | `	  }` |
|   220 |  611 | `	  ph7_result_string(pCtx,zDir,nDir);` |
|   220 |  612 | `	  return;` |
|     - |  613 | `  }` |
|   220 |  614 | `  ph7_result_string(pCtx,"/tmp",(int)sizeof("/tmp")-1);` |
|   220 |  615 | `}` |
|     - |  616 | `/* unsigned int (*xProcessId)(void) */` |
|   230 |  617 | `static unsigned int UnixVfs_ProcessId(void)` |
|     - |  618 | `{` |
|   230 |  619 | `	return (unsigned int)getpid();` |
|     - |  620 | `}` |
|     - |  621 | `/* int (*xUid)(void) */` |
|     4 |  622 | `static int UnixVfs_uid(void)` |
|     - |  623 | `{` |
|     4 |  624 | `	return (int)getuid();` |
|     - |  625 | `}` |
|     - |  626 | `/* int (*xGid)(void) */` |
|     2 |  627 | `static int UnixVfs_gid(void)` |
|     - |  628 | `{` |
|     2 |  629 | `	return (int)getgid();` |
|     - |  630 | `}` |
|     - |  631 | `/* int (*xUmask)(int) */` |
|     8 |  632 | `static int UnixVfs_Umask(int new_mask)` |
|     - |  633 | `{` |
|     - |  634 | `	int old_mask;` |
|     8 |  635 | `	old_mask = umask(new_mask);` |
|     8 |  636 | `	return old_mask;` |
|     - |  637 | `}` |
|     - |  638 | `/* void (*xUsername)(ph7_context *) */` |
|     2 |  639 | `static void UnixVfs_Username(ph7_context *pCtx)` |
|     - |  640 | `{` |
|     - |  641 | `#ifndef PH7_UNIX_STATIC_BUILD` |
|     - |  642 | `  struct passwd *pwd;` |
|     - |  643 | `  uid_t uid;` |
|     2 |  644 | `  uid = getuid();` |
|     2 |  645 | `  pwd = getpwuid(uid);   /* Try getting UID for username */` |
|     2 |  646 | `  if (pwd == 0) {` |
|   ! 0 |  647 | `    return;` |
|     - |  648 | `  }` |
|     - |  649 | `  /* Return the username */` |
|     2 |  650 | `  ph7_result_string(pCtx,pwd->pw_name,-1);` |
|     - |  651 | `#else` |
|     - |  652 | `  ph7_result_string(pCtx,"Unknown",-1);` |
|     - |  653 | `#endif /* PH7_UNIX_STATIC_BUILD */` |
|     2 |  654 | `  return;` |
|     1 |  655 | `}` |
|     - |  656 | `/* int (*xLink)(const char *,const char *,int) */` |
|    12 |  657 | `static int UnixVfs_link(const char *zSrc,const char *zTarget,int is_sym)` |
|     - |  658 | `{` |
|    12 |  659 | `	zSrc = UnixVfsLocalPath(zSrc);` |
|    12 |  660 | `	zTarget = UnixVfsLocalPath(zTarget);` |
|     - |  661 | `	int rc;` |
|    12 |  662 | `	if( is_sym ){` |
|     - |  663 | `		/* Symbolic link */` |
|    10 |  664 | `		rc = symlink(zSrc,zTarget);` |
|     5 |  665 | `	}else{` |
|     - |  666 | `		/* Hard link */` |
|     2 |  667 | `		rc = link(zSrc,zTarget);` |
|     - |  668 | `	}` |
|    12 |  669 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  670 | `}` |
|     - |  671 | `/* int (*xChroot)(const char *) */` |
|   ! 0 |  672 | `static int UnixVfs_chroot(const char *zRootDir)` |
|     - |  673 | `{` |
|     - |  674 | `	int rc;` |
|   ! 0 |  675 | `	rc = chroot(zRootDir);` |
|   ! 0 |  676 | `	return rc == 0 ? PH7_OK : -1;` |
|     - |  677 | `}` |
|     - |  678 | `/* Export the UNIX vfs */` |
|     - |  679 | `PH7_PRIVATE const ph7_vfs sUnixVfs = {` |
|     - |  680 | `	"Unix_vfs",` |
|     - |  681 | `	PH7_VFS_VERSION,` |
|     - |  682 | `	UnixVfs_chdir,    /* int (*xChdir)(const char *) */` |
|     - |  683 | `	UnixVfs_chroot,   /* int (*xChroot)(const char *); */` |
|     - |  684 | `	UnixVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */` |
|     - |  685 | `	UnixVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */` |
|     - |  686 | `	UnixVfs_rmdir,    /* int (*xRmdir)(const char *) */` |
|     - |  687 | `	UnixVfs_isdir,    /* int (*xIsdir)(const char *) */` |
|     - |  688 | `	UnixVfs_Rename,   /* int (*xRename)(const char *,const char *) */` |
|     - |  689 | `	UnixVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/` |
|     - |  690 | `	UnixVfs_Sleep,    /* int (*xSleep)(unsigned int) */` |
|     - |  691 | `	UnixVfs_unlink,   /* int (*xUnlink)(const char *) */` |
|     - |  692 | `	UnixVfs_FileExists, /* int (*xFileExists)(const char *) */` |
|     - |  693 | `	UnixVfs_Chmod, /*int (*xChmod)(const char *,int)*/` |
|     - |  694 | `	UnixVfs_Chown, /*int (*xChown)(const char *,const char *)*/` |
|     - |  695 | `	UnixVfs_Chgrp, /*int (*xChgrp)(const char *,const char *)*/` |
|     - |  696 | `	UnixVfs_FreeSpace,  /* ph7_int64 (*xFreeSpace)(const char *) */` |
|     - |  697 | `	UnixVfs_TotalSpace, /* ph7_int64 (*xTotalSpace)(const char *) */` |
|     - |  698 | `	UnixVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */` |
|     - |  699 | `	UnixVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */` |
|     - |  700 | `	UnixVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */` |
|     - |  701 | `	UnixVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */` |
|     - |  702 | `	UnixVfs_Stat,  /* int (*xStat)(const char *,ph7_value *,ph7_value *) */` |
|     - |  703 | `	UnixVfs_lStat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */` |
|     - |  704 | `	UnixVfs_isfile,     /* int (*xIsfile)(const char *) */` |
|     - |  705 | `	UnixVfs_islink,     /* int (*xIslink)(const char *) */` |
|     - |  706 | `	UnixVfs_isreadable, /* int (*xReadable)(const char *) */` |
|     - |  707 | `	UnixVfs_iswritable, /* int (*xWritable)(const char *) */` |
|     - |  708 | `	UnixVfs_isexecutable,/* int (*xExecutable)(const char *) */` |
|     - |  709 | `	UnixVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */` |
|     - |  710 | `	UnixVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */` |
|     - |  711 | `	UnixVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */` |
|     - |  712 | `	UnixVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */` |
|     - |  713 | `	UnixVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */` |
|     - |  714 | `	UnixVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */` |
|     - |  715 | `	UnixVfs_link,       /* int (*xLink)(const char *,const char *,int) */` |
|     - |  716 | `	UnixVfs_Umask,      /* int (*xUmask)(int) */` |
|     - |  717 | `	UnixVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */` |
|     - |  718 | `	UnixVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */` |
|     - |  719 | `	UnixVfs_uid, /* int (*xUid)(void) */` |
|     - |  720 | `	UnixVfs_gid, /* int (*xGid)(void) */` |
|     - |  721 | `	UnixVfs_Username,    /* void (*xUsername)(ph7_context *) */` |
|     - |  722 | `	0, /* int (*xExec)(const char *,ph7_context *) */` |
|     - |  723 | `	UnixVfs_Readlink, /* int (*xReadlink)(const char *,ph7_context *) */` |
|     - |  724 | `	UnixVfs_Environ  /* int (*xEnviron)(ph7_context *) */` |
|     - |  725 | `};` |
|     - |  726 | `/* UNIX File IO */` |
|     - |  727 | `#define PH7_UNIX_OPEN_MODE	0640 /* Default open mode */` |
|     - |  728 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
| 34828 |  729 | `static int UnixFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)` |
|     - |  730 | `{` |
| 34828 |  731 | `	int iOpen = O_RDONLY;` |
|     - |  732 | `	int fd;` |
|     - |  733 | `	/* Set the desired flags according to the open mode */` |
| 34828 |  734 | `	if( iOpenMode & PH7_IO_OPEN_CREATE ){` |
|     - |  735 | `		/* Open existing file, or create if it doesn't exist */` |
| 15832 |  736 | `		iOpen = O_CREAT;` |
| 15832 |  737 | `		if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|     - |  738 | `			/* If the specified file exists and is writable, the function overwrites the file */` |
| 15826 |  739 | `			iOpen \|= O_TRUNC;` |
|  7913 |  740 | `			SXUNUSED(pResource); /* cc warning */` |
|  7913 |  741 | `		}` |
| 26912 |  742 | `	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){` |
|     - |  743 | `		/* Creates a new file, only if it does not already exist.` |
|     - |  744 | `		* If the file exists, it fails.` |
|     - |  745 | `		*/` |
|   218 |  746 | `		iOpen = O_CREAT\|O_EXCL;` |
| 18887 |  747 | `	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){` |
|     - |  748 | `		/* Opens a file and truncates it so that its size is zero bytes` |
|     - |  749 | `		 * The file must exist.` |
|     - |  750 | `		 */` |
|   ! 0 |  751 | `		iOpen = O_RDWR\|O_TRUNC;` |
|   ! 0 |  752 | `	}` |
| 34828 |  753 | `	if( iOpenMode & PH7_IO_OPEN_RDWR ){` |
|     - |  754 | `		/* Read+Write access */` |
| 15802 |  755 | `		iOpen &= ~O_RDONLY;` |
| 15802 |  756 | `		iOpen \|= O_RDWR;` |
| 26927 |  757 | `	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){` |
|     - |  758 | `		/* Write only access */` |
|   268 |  759 | `		iOpen &= ~O_RDONLY;` |
|   268 |  760 | `		iOpen \|= O_WRONLY;` |
|   134 |  761 | `	}` |
| 34828 |  762 | `	if( iOpenMode & PH7_IO_OPEN_APPEND ){` |
|     - |  763 | `		/* Append mode */` |
|     6 |  764 | `		iOpen \|= O_APPEND;` |
|     3 |  765 | `	}` |
|     - |  766 | `#ifdef O_TEMP` |
|     - |  767 | `	if( iOpenMode & PH7_IO_OPEN_TEMP ){` |
|     - |  768 | `		/* File is temporary */` |
|     - |  769 | `		iOpen \|= O_TEMP;` |
|     - |  770 | `	}` |
|     - |  771 | `#endif` |
|     - |  772 | `	/* Open the file now */` |
| 34828 |  773 | `	fd = open(zPath,iOpen,PH7_UNIX_OPEN_MODE);` |
| 34828 |  774 | `	if( fd < 0 ){` |
|     - |  775 | `		/* IO error */` |
|    82 |  776 | `		return -1;` |
|     - |  777 | `	}` |
|     - |  778 | `	/* Save the handle */` |
| 34746 |  779 | `	*ppHandle = SX_INT_TO_PTR(fd);` |
| 34746 |  780 | `	return PH7_OK;` |
| 17414 |  781 | `}` |
|     - |  782 | `/* int (*xOpenDir)(const char *,ph7_value *,void **) */` |
|  1361 |  783 | `static int UnixDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)` |
|     - |  784 | `{` |
|     - |  785 | `	DIR *pDir;` |
|     - |  786 | `	/* Open the target directory */` |
|  1361 |  787 | `	pDir = opendir(zPath);` |
|  1361 |  788 | `	if( pDir == 0 ){` |
|    15 |  789 | `		SXUNUSED(pResource); /* Compiler warning */` |
|    30 |  790 | `		return -1;` |
|     - |  791 | `	}` |
|     - |  792 | `	/* Save our structure */` |
|  1331 |  793 | `	*ppHandle = pDir;` |
|  1331 |  794 | `	return PH7_OK;` |
|   680 |  795 | `}` |
|     - |  796 | `/* void (*xCloseDir)(void *) */` |
|  1311 |  797 | `static void UnixDir_Close(void *pUserData)` |
|     - |  798 | `{` |
|  1311 |  799 | `	closedir((DIR *)pUserData);` |
|  1311 |  800 | `}` |
|     - |  801 | `/* void (*xClose)(void *); */` |
| 34780 |  802 | `static void UnixFile_Close(void *pUserData)` |
|     - |  803 | `{` |
| 34780 |  804 | `	close(SX_PTR_TO_INT(pUserData));` |
| 34780 |  805 | `}` |
|     - |  806 | `/* int (*xReadDir)(void *,ph7_context *) */` |
| 13559 |  807 | `static int UnixDir_Read(void *pUserData,ph7_context *pCtx)` |
|     - |  808 | `{` |
| 13559 |  809 | `	DIR *pDir = (DIR *)pUserData;` |
|     - |  810 | `	struct dirent *pEntry;` |
|     - |  811 | `	char *zName;` |
|     - |  812 | `	sxu32 n;` |
|     - |  813 | `	/* php's readdir() yields every entry including '.' and '..' */` |
| 13559 |  814 | `	pEntry = readdir(pDir);` |
| 13559 |  815 | `	if( pEntry == 0 ){` |
|     - |  816 | `		/* No more entries to process */` |
|  1293 |  817 | `		return -1;` |
|     - |  818 | `	}` |
| 12266 |  819 | `	zName = pEntry->d_name;` |
| 12266 |  820 | `	n = SyStrlen(zName);` |
|     - |  821 | `	/* Return the current file name */` |
| 12266 |  822 | `	ph7_result_string(pCtx,zName,(int)n);` |
| 12266 |  823 | `	return PH7_OK;` |
|  6778 |  824 | `}` |
|     - |  825 | `/* void (*xRewindDir)(void *) */` |
|    30 |  826 | `static void UnixDir_Rewind(void *pUserData)` |
|     - |  827 | `{` |
|    30 |  828 | `	rewinddir((DIR *)pUserData);` |
|    30 |  829 | `}` |
|     - |  830 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */` |
| 37288 |  831 | `static ph7_int64 UnixFile_Read(void *pUserData,void *pBuffer,ph7_int64 nDatatoRead)` |
|     - |  832 | `{` |
|     - |  833 | `	ssize_t nRd;` |
| 37288 |  834 | `	nRd = read(SX_PTR_TO_INT(pUserData),pBuffer,(size_t)nDatatoRead);` |
| 37288 |  835 | `	if( nRd < 0 ){` |
|     - |  836 | `		/* An IO error. EOF is read()'s ZERO and rides through as one: it is not a` |
|     - |  837 | `		 * failure, and the readers above need to tell the two apart — fread() at` |
|     - |  838 | `		 * EOF is php's "" and only a real error is its false. Every consumer of` |
|     - |  839 | ``		 * this driver stops on `< 1`, so both still end a read loop. The Windows`` |
|     - |  840 | `		 * driver already answers this way (ReadFile succeeds with 0 bytes). */` |
|     6 |  841 | `		return -1;` |
|     - |  842 | `	}` |
| 37282 |  843 | `	return (ph7_int64)nRd;` |
| 18644 |  844 | `}` |
|     - |  845 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */` |
| 15758 |  846 | `static ph7_int64 UnixFile_Write(void *pUserData,const void *pBuffer,ph7_int64 nWrite)` |
|     - |  847 | `{` |
| 15758 |  848 | `	const char *zData = (const char *)pBuffer;` |
| 15758 |  849 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|     - |  850 | `	ph7_int64 nCount;` |
|     - |  851 | `	ssize_t nWr;` |
| 15758 |  852 | `	nCount = 0;` |
| 15758 |  853 | `	for(;;){` |
| 31516 |  854 | `		if( nWrite < 1 ){` |
| 15758 |  855 | `			break;` |
|     - |  856 | `		}` |
| 15758 |  857 | `		nWr = write(fd,zData,(size_t)nWrite);` |
| 15758 |  858 | `		if( nWr < 1 ){` |
|     - |  859 | `			/* IO error */` |
|   ! 0 |  860 | `			break;` |
|     - |  861 | `		}` |
| 15758 |  862 | `		nWrite -= nWr;` |
| 15758 |  863 | `		nCount += nWr;` |
| 15758 |  864 | `		zData += nWr;` |
|     - |  865 | `	}` |
| 15758 |  866 | `	if( nWrite > 0 ){` |
|   ! 0 |  867 | `		return -1;` |
|     - |  868 | `	}` |
| 15758 |  869 | `	return nCount;` |
|  7879 |  870 | `}` |
|     - |  871 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|    48 |  872 | `static int UnixFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)` |
|     - |  873 | `{` |
|     - |  874 | `	off_t iNew;` |
|    48 |  875 | `	switch(whence){` |
|     5 |  876 | `	case 1:/*SEEK_CUR*/` |
|    10 |  877 | `		whence = SEEK_CUR;` |
|    10 |  878 | `		break;` |
|   ! 0 |  879 | `	case 2: /* SEEK_END */` |
|   ! 0 |  880 | `		whence = SEEK_END;` |
|   ! 0 |  881 | `		break;` |
|    38 |  882 | `	case 0: /* SEEK_SET */` |
|     - |  883 | `	default:` |
|    38 |  884 | `		whence = SEEK_SET;` |
|    38 |  885 | `		break;` |
|     - |  886 | `	}` |
|    48 |  887 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),(off_t)iOfft,whence);` |
|    48 |  888 | `	if( iNew < 0 ){` |
|   ! 0 |  889 | `		return -1;` |
|     - |  890 | `	}` |
|    48 |  891 | `	return PH7_OK;` |
|    24 |  892 | `}` |
|     - |  893 | `/* int (*xLock)(void *,int) — see the lock_type value space in ph7.h */` |
|    28 |  894 | `static int UnixFile_Lock(void *pUserData,int lock_type)` |
|     - |  895 | `{` |
|    28 |  896 | `	int fd = SX_PTR_TO_INT(pUserData);` |
|     - |  897 | `	int op;` |
|     - |  898 | `	int rc;` |
|    28 |  899 | `	if( lock_type < 0 ){` |
|     - |  900 | `		/* Unlock the file */` |
|    10 |  901 | `		op = LOCK_UN;` |
|     5 |  902 | `	}else{` |
|    18 |  903 | `		op = (lock_type == PH7_IO_LOCK_EX \|\| lock_type == PH7_IO_LOCK_EX_NB) ? LOCK_EX : LOCK_SH;` |
|    18 |  904 | `		if( lock_type == PH7_IO_LOCK_SH_NB \|\| lock_type == PH7_IO_LOCK_EX_NB ){` |
|    12 |  905 | `			op \|= LOCK_NB;` |
|     6 |  906 | `		}` |
|     - |  907 | `	}` |
|    28 |  908 | `	rc = flock(fd,op);` |
|    28 |  909 | `	if( rc == 0 ){` |
|    24 |  910 | `		return PH7_OK;` |
|     - |  911 | `	}` |
|     - |  912 | `	/* A non-blocking request that another holder refused is php's $would_block,` |
|     - |  913 | `	 * not an IO failure. (EWOULDBLOCK and EAGAIN are the same value on every` |
|     - |  914 | `	 * platform this driver builds for.) */` |
|     4 |  915 | `	if( errno == EWOULDBLOCK ){` |
|     4 |  916 | `		return SXERR_BUSY;` |
|     - |  917 | `	}` |
|   ! 0 |  918 | `	return -1;` |
|    14 |  919 | `}` |
|     - |  920 | `/* ph7_int64 (*xTell)(void *) */` |
|    94 |  921 | `static ph7_int64 UnixFile_Tell(void *pUserData)` |
|     - |  922 | `{` |
|     - |  923 | `	off_t iNew;` |
|    94 |  924 | `	iNew = lseek(SX_PTR_TO_INT(pUserData),0,SEEK_CUR);` |
|    94 |  925 | `	return (ph7_int64)iNew;` |
|     - |  926 | `}` |
|     - |  927 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|     6 |  928 | `static int UnixFile_Trunc(void *pUserData,ph7_int64 nOfft)` |
|     - |  929 | `{` |
|     - |  930 | `	int rc;` |
|     6 |  931 | `	rc = ftruncate(SX_PTR_TO_INT(pUserData),(off_t)nOfft);` |
|     6 |  932 | `	if( rc != 0 ){` |
|   ! 0 |  933 | `		return -1;` |
|     - |  934 | `	}` |
|     6 |  935 | `	return PH7_OK;` |
|     3 |  936 | `}` |
|     - |  937 | `/* int (*xSync)(void *); */` |
|     4 |  938 | `static int UnixFile_Sync(void *pUserData)` |
|     - |  939 | `{` |
|     - |  940 | `	int rc;` |
|     4 |  941 | `	rc = fsync(SX_PTR_TO_INT(pUserData));` |
|     4 |  942 | `	return rc == 0 ? PH7_OK : - 1;` |
|     - |  943 | `}` |
|     - |  944 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|     6 |  945 | `static int UnixFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)` |
|     - |  946 | `{` |
|     - |  947 | `	struct stat st;` |
|     - |  948 | `	int rc;` |
|     6 |  949 | `	rc = fstat(SX_PTR_TO_INT(pUserData),&st);` |
|     6 |  950 | `	if( rc != 0 ){` |
|   ! 0 |  951 | `	 return -1;` |
|     - |  952 | `	}` |
|     - |  953 | `	/* dev */` |
|     6 |  954 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);` |
|     6 |  955 | `	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */` |
|     - |  956 | `	/* ino */` |
|     6 |  957 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);` |
|     6 |  958 | `	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */` |
|     - |  959 | `	/* mode */` |
|     6 |  960 | `	ph7_value_int(pWorker,(int)st.st_mode);` |
|     6 |  961 | `	ph7_array_add_strkey_elem(pArray,"mode",pWorker);` |
|     - |  962 | `	/* nlink */` |
|     6 |  963 | `	ph7_value_int(pWorker,(int)st.st_nlink);` |
|     6 |  964 | `	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */` |
|     - |  965 | `	/* uid,gid,rdev */` |
|     6 |  966 | `	ph7_value_int(pWorker,(int)st.st_uid);` |
|     6 |  967 | `	ph7_array_add_strkey_elem(pArray,"uid",pWorker);` |
|     6 |  968 | `	ph7_value_int(pWorker,(int)st.st_gid);` |
|     6 |  969 | `	ph7_array_add_strkey_elem(pArray,"gid",pWorker);` |
|     6 |  970 | `	ph7_value_int(pWorker,(int)st.st_rdev);` |
|     6 |  971 | `	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);` |
|     - |  972 | `	/* size */` |
|     6 |  973 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_size);` |
|     6 |  974 | `	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */` |
|     - |  975 | `	/* atime */` |
|     6 |  976 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);` |
|     6 |  977 | `	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */` |
|     - |  978 | `	/* mtime */` |
|     6 |  979 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);` |
|     6 |  980 | `	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */` |
|     - |  981 | `	/* ctime */` |
|     6 |  982 | `	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);` |
|     6 |  983 | `	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */` |
|     - |  984 | `	/* blksize,blocks */` |
|     6 |  985 | `	ph7_value_int(pWorker,(int)st.st_blksize);` |
|     6 |  986 | `	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);` |
|     6 |  987 | `	ph7_value_int(pWorker,(int)st.st_blocks);` |
|     6 |  988 | `	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);` |
|     6 |  989 | `	return PH7_OK;` |
|     3 |  990 | `}` |
|     - |  991 | `/* Export the file:// stream */` |
|     - |  992 | `PH7_PRIVATE const ph7_io_stream sUnixFileStream = {` |
|     - |  993 | `	"file", /* Stream name */` |
|     - |  994 | `	PH7_IO_STREAM_VERSION,` |
|     - |  995 | `	UnixFile_Open,  /* xOpen */` |
|     - |  996 | `	UnixDir_Open,   /* xOpenDir */` |
|     - |  997 | `	UnixFile_Close, /* xClose */` |
|     - |  998 | `	UnixDir_Close,  /* xCloseDir */` |
|     - |  999 | `	UnixFile_Read,  /* xRead */` |
|     - | 1000 | `	UnixDir_Read,   /* xReadDir */` |
|     - | 1001 | `	UnixFile_Write, /* xWrite */` |
|     - | 1002 | `	UnixFile_Seek,  /* xSeek */` |
|     - | 1003 | `	UnixFile_Lock,  /* xLock */` |
|     - | 1004 | `	UnixDir_Rewind, /* xRewindDir */` |
|     - | 1005 | `	UnixFile_Tell,  /* xTell */` |
|     - | 1006 | `	UnixFile_Trunc, /* xTrunc */` |
|     - | 1007 | `	UnixFile_Sync,  /* xSeek */` |
|     - | 1008 | `	UnixFile_Stat   /* xStat */` |
|     - | 1009 | `};` |
|     - | 1010 | `#endif /* __UNIXES__ */` |
|     - | 1011 |  |
