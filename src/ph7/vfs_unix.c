/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#ifdef __UNIXES__
/*
 * UNIX VFS implementation for the PH7 engine.
 * Status:
 *    Stable.
 */
#include <sys/types.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>
#include <utime.h>
#include <stdio.h>
#include <stdlib.h>
/* int (*xchdir)(const char *) */
static int UnixVfs_chdir(const char *zPath)
{
  int rc;
  rc = chdir(zPath);
  return rc == 0 ? PH7_OK : -1;
}
/* int (*xGetcwd)(ph7_context *) */
static int UnixVfs_getcwd(ph7_context *pCtx)
{
	char zBuf[4096];
	char *zDir;
	/* Get the current directory */
	zDir = getcwd(zBuf,sizeof(zBuf));
	if( zDir == 0 ){
	  return -1;
    }
	ph7_result_string(pCtx,zDir,-1/*Compute length automatically*/);
	return PH7_OK;
}
/* int (*xMkdir)(const char *,int,int) */
static int UnixVfs_mkdir(const char *zPath,int mode,int recursive)
{
	int rc;
        rc = mkdir(zPath,mode);
	SXUNUSED(recursive); /* cc warning */
	return rc == 0 ? PH7_OK : -1;
}
/* int (*xRmdir)(const char *) */
static int UnixVfs_rmdir(const char *zPath)
{
	int rc;
	rc = rmdir(zPath);
	return rc == 0 ? PH7_OK : -1;
}
/* int (*xIsdir)(const char *) */
static int UnixVfs_isdir(const char *zPath)
{
	struct stat st;
	int rc;
	rc = stat(zPath,&st);
	if( rc != 0 ){
	 return -1;
	}
	rc = S_ISDIR(st.st_mode);
	return rc ? PH7_OK : -1 ;
}
/* int (*xRename)(const char *,const char *) */
static int UnixVfs_Rename(const char *zOld,const char *zNew)
{
	int rc;
	rc = rename(zOld,zNew);
	return rc == 0 ? PH7_OK : -1;
}
/* int (*xRealpath)(const char *,ph7_context *) */
static int UnixVfs_Realpath(const char *zPath,ph7_context *pCtx)
{
#ifndef PH7_UNIX_OLD_LIBC
	char *zReal;
	zReal = realpath(zPath,0);
	if( zReal == 0 ){
	  return -1;
	}
	ph7_result_string(pCtx,zReal,-1/*Compute length automatically*/);
        /* Release the allocated buffer */
	free(zReal);
	return PH7_OK;
#else
    zPath = 0; /* cc warning */
    pCtx = 0;
    return -1;
#endif
}
/* int (*xSleep)(unsigned int) */
static int UnixVfs_Sleep(unsigned int uSec)
{
	usleep(uSec);
	return PH7_OK;
}
/* int (*xUnlink)(const char *) */
static int UnixVfs_unlink(const char *zPath)
{
	int rc;
	rc = unlink(zPath);
	return rc == 0 ? PH7_OK : -1 ;
}
/* int (*xFileExists)(const char *) */
static int UnixVfs_FileExists(const char *zPath)
{
	int rc;
	rc = access(zPath,F_OK);
	return rc == 0 ? PH7_OK : -1;
}
/* ph7_int64 (*xFileSize)(const char *) */
/* ph7_int64 (*xFreeSpace)(const char *) */
static ph7_int64 UnixVfs_FreeSpace(const char *zPath)
{
	struct statvfs sInfo;
	if( statvfs(zPath,&sInfo) != 0 ){
		return -1;
	}
	/* php reports the space available to an UNPRIVILEGED user (f_bavail) */
	return (ph7_int64)sInfo.f_bavail * (ph7_int64)sInfo.f_frsize;
}
/* ph7_int64 (*xTotalSpace)(const char *) */
static ph7_int64 UnixVfs_TotalSpace(const char *zPath)
{
	struct statvfs sInfo;
	if( statvfs(zPath,&sInfo) != 0 ){
		return -1;
	}
	return (ph7_int64)sInfo.f_blocks * (ph7_int64)sInfo.f_frsize;
}
static ph7_int64 UnixVfs_FileSize(const char *zPath)
{
	struct stat st;
	int rc;
	rc = stat(zPath,&st);
	if( rc != 0 ){
	 return -1;
	}
	return (ph7_int64)st.st_size;
}
/* int (*xTouch)(const char *,ph7_int64,ph7_int64) */
static int UnixVfs_Touch(const char *zPath,ph7_int64 touch_time,ph7_int64 access_time)
{
	struct utimbuf ut;
	int rc;
	ut.actime  = (time_t)access_time;
	ut.modtime = (time_t)touch_time;
	rc = utime(zPath,&ut);
	if( rc != 0 ){
	 return -1;
	}
	return PH7_OK;
}
/* ph7_int64 (*xFileAtime)(const char *) */
static ph7_int64 UnixVfs_FileAtime(const char *zPath)
{
	struct stat st;
	int rc;
	rc = stat(zPath,&st);
	if( rc != 0 ){
	 return -1;
	}
	return (ph7_int64)st.st_atime;
}
/* ph7_int64 (*xFileMtime)(const char *) */
static ph7_int64 UnixVfs_FileMtime(const char *zPath)
{
	struct stat st;
	int rc;
	rc = stat(zPath,&st);
	if( rc != 0 ){
	 return -1;
	}
	return (ph7_int64)st.st_mtime;
}
/* ph7_int64 (*xFileCtime)(const char *) */
static ph7_int64 UnixVfs_FileCtime(const char *zPath)
{
	struct stat st;
	int rc;
	rc = stat(zPath,&st);
	if( rc != 0 ){
	 return -1;
	}
	return (ph7_int64)st.st_ctime;
}
/* int (*xStat)(const char *,ph7_value *,ph7_value *) */
static int UnixVfs_Stat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)
{
	struct stat st;
	int rc;
	rc = stat(zPath,&st);
	if( rc != 0 ){
	 return -1;
	}
	/* dev */
	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);
	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */
	/* ino */
	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);
	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */
	/* mode */
	ph7_value_int(pWorker,(int)st.st_mode);
	ph7_array_add_strkey_elem(pArray,"mode",pWorker);
	/* nlink */
	ph7_value_int(pWorker,(int)st.st_nlink);
	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */
	/* uid,gid,rdev */
	ph7_value_int(pWorker,(int)st.st_uid);
	ph7_array_add_strkey_elem(pArray,"uid",pWorker);
	ph7_value_int(pWorker,(int)st.st_gid);
	ph7_array_add_strkey_elem(pArray,"gid",pWorker);
	ph7_value_int(pWorker,(int)st.st_rdev);
	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);
	/* size */
	ph7_value_int64(pWorker,(ph7_int64)st.st_size);
	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */
	/* atime */
	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);
	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */
	/* mtime */
	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);
	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */
	/* ctime */
	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);
	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */
	/* blksize,blocks */
	ph7_value_int(pWorker,(int)st.st_blksize);
	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);
	ph7_value_int(pWorker,(int)st.st_blocks);
	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);
	return PH7_OK;
}
/* int (*xlStat)(const char *,ph7_value *,ph7_value *) */
static int UnixVfs_lStat(const char *zPath,ph7_value *pArray,ph7_value *pWorker)
{
	struct stat st;
	int rc;
	rc = lstat(zPath,&st);
	if( rc != 0 ){
	 return -1;
	}
	/* dev */
	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);
	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */
	/* ino */
	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);
	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */
	/* mode */
	ph7_value_int(pWorker,(int)st.st_mode);
	ph7_array_add_strkey_elem(pArray,"mode",pWorker);
	/* nlink */
	ph7_value_int(pWorker,(int)st.st_nlink);
	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */
	/* uid,gid,rdev */
	ph7_value_int(pWorker,(int)st.st_uid);
	ph7_array_add_strkey_elem(pArray,"uid",pWorker);
	ph7_value_int(pWorker,(int)st.st_gid);
	ph7_array_add_strkey_elem(pArray,"gid",pWorker);
	ph7_value_int(pWorker,(int)st.st_rdev);
	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);
	/* size */
	ph7_value_int64(pWorker,(ph7_int64)st.st_size);
	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */
	/* atime */
	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);
	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */
	/* mtime */
	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);
	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */
	/* ctime */
	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);
	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */
	/* blksize,blocks */
	ph7_value_int(pWorker,(int)st.st_blksize);
	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);
	ph7_value_int(pWorker,(int)st.st_blocks);
	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);
	return PH7_OK;
}
/* int (*xChmod)(const char *,int) */
static int UnixVfs_Chmod(const char *zPath,int mode)
{
    int rc;
    rc = chmod(zPath,(mode_t)mode);
    return rc == 0 ? PH7_OK : - 1;
}
/* int (*xChown)(const char *,const char *) */
static int UnixVfs_Chown(const char *zPath,const char *zUser)
{
#ifndef PH7_UNIX_STATIC_BUILD
  struct passwd *pwd;
  uid_t uid;
  int rc;
  pwd = getpwnam(zUser);   /* Try getting UID for username */
  if (pwd == 0) {
    return -1;
  }
  uid = pwd->pw_uid;
  rc = chown(zPath,uid,-1);
  return rc == 0 ? PH7_OK : -1;
#else
	SXUNUSED(zPath);
	SXUNUSED(zUser);
	return -1;
#endif /* PH7_UNIX_STATIC_BUILD */
}
/* int (*xChgrp)(const char *,const char *) */
static int UnixVfs_Chgrp(const char *zPath,const char *zGroup)
{
#ifndef PH7_UNIX_STATIC_BUILD
  struct group *group;
  gid_t gid;
  int rc;
  group = getgrnam(zGroup);
  if (group == 0) {
    return -1;
  }
  gid = group->gr_gid;
  rc = chown(zPath,-1,gid);
  return rc == 0 ? PH7_OK : -1;
#else
	SXUNUSED(zPath);
	SXUNUSED(zGroup);
	return -1;
#endif /* PH7_UNIX_STATIC_BUILD */
}
/* int (*xIsfile)(const char *) */
static int UnixVfs_isfile(const char *zPath)
{
	struct stat st;
	int rc;
	rc = stat(zPath,&st);
	if( rc != 0 ){
	 return -1;
	}
	rc = S_ISREG(st.st_mode);
	return rc ? PH7_OK : -1 ;
}
/* int (*xIslink)(const char *) */
static int UnixVfs_islink(const char *zPath)
{
	struct stat st;
	int rc;
	rc = stat(zPath,&st);
	if( rc != 0 ){
	 return -1;
	}
	rc = S_ISLNK(st.st_mode);
	return rc ? PH7_OK : -1 ;
}
/* int (*xReadable)(const char *) */
static int UnixVfs_isreadable(const char *zPath)
{
	int rc;
	rc = access(zPath,R_OK);
	return rc == 0 ? PH7_OK : -1;
}
/* int (*xWritable)(const char *) */
static int UnixVfs_iswritable(const char *zPath)
{
	int rc;
	rc = access(zPath,W_OK);
	return rc == 0 ? PH7_OK : -1;
}
/* int (*xExecutable)(const char *) */
static int UnixVfs_isexecutable(const char *zPath)
{
	int rc;
	rc = access(zPath,X_OK);
	return rc == 0 ? PH7_OK : -1;
}
/* int (*xFiletype)(const char *,ph7_context *) */
static int UnixVfs_Filetype(const char *zPath,ph7_context *pCtx)
{
	struct stat st;
	int rc;
    rc = stat(zPath,&st);
	if( rc != 0 ){
	  /* Expand 'unknown' */
	  ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);
	  return -1;
	}
	if(S_ISREG(st.st_mode) ){
		ph7_result_string(pCtx,"file",sizeof("file")-1);
	}else if(S_ISDIR(st.st_mode)){
		ph7_result_string(pCtx,"dir",sizeof("dir")-1);
	}else if(S_ISLNK(st.st_mode)){
		ph7_result_string(pCtx,"link",sizeof("link")-1);
	}else if(S_ISBLK(st.st_mode)){
		ph7_result_string(pCtx,"block",sizeof("block")-1);
    }else if(S_ISSOCK(st.st_mode)){
		ph7_result_string(pCtx,"socket",sizeof("socket")-1);
	}else if(S_ISFIFO(st.st_mode)){
       ph7_result_string(pCtx,"fifo",sizeof("fifo")-1);
	}else{
		ph7_result_string(pCtx,"unknown",sizeof("unknown")-1);
	}
	return PH7_OK;
}
/* int (*xGetenv)(const char *,ph7_context *) */
static int UnixVfs_Getenv(const char *zVar,ph7_context *pCtx)
{
	char *zEnv;
	zEnv = getenv(zVar);
	if( zEnv == 0 ){
	  return -1;
	}
	ph7_result_string(pCtx,zEnv,-1/*Compute length automatically*/);
	return PH7_OK;
}
/* int (*xSetenv)(const char *,const char *) */
static int UnixVfs_Setenv(const char *zName,const char *zValue)
{
   int rc;
   rc = setenv(zName,zValue,1);
   return rc == 0 ? PH7_OK : -1;
}
/* int (*xMmap)(const char *,void **,ph7_int64 *) */
static int UnixVfs_Mmap(const char *zPath,void **ppMap,ph7_int64 *pSize)
{
	struct stat st;
	void *pMap;
	int fd;
	int rc;
	/* Open the file in a read-only mode */
	fd = open(zPath,O_RDONLY);
	if( fd < 0 ){
		return -1;
	}
	/* stat the handle */
	fstat(fd,&st);
	/* Obtain a memory view of the whole file */
	pMap = mmap(0,st.st_size,PROT_READ,MAP_PRIVATE|MAP_FILE,fd,0);
	rc = PH7_OK;
	if( pMap == MAP_FAILED ){
		rc = -1;
	}else{
		/* Point to the memory view */
		*ppMap = pMap;
		*pSize = (ph7_int64)st.st_size;
	}
	close(fd);
	return rc;
}
/* void (*xUnmap)(void *,ph7_int64)  */
static void UnixVfs_Unmap(void *pView,ph7_int64 nSize)
{
	munmap(pView,(size_t)nSize);
}
/* void (*xTempDir)(ph7_context *) */
static void UnixVfs_TempDir(ph7_context *pCtx)
{
	static const char *azDirs[] = {
     "/var/tmp",
     "/usr/tmp",
	 "/usr/local/tmp"
  };
  unsigned int i;
  struct stat buf;
  const char *zDir;
  zDir = getenv("TMPDIR");
  if( zDir && zDir[0] != 0 && !access(zDir,07) ){
	  ph7_result_string(pCtx,zDir,-1);
	  return;
  }
  for(i=0; i<sizeof(azDirs)/sizeof(azDirs[0]); i++){
	zDir=azDirs[i];
    if( zDir==0 ) continue;
    if( stat(zDir, &buf) ) continue;
    if( !S_ISDIR(buf.st_mode) ) continue;
    if( access(zDir, 07) ) continue;
    /* Got one */
	ph7_result_string(pCtx,zDir,-1);
	return;
  }
  /* Default temp dir */
  ph7_result_string(pCtx,"/tmp",(int)sizeof("/tmp")-1);
}
/* unsigned int (*xProcessId)(void) */
static unsigned int UnixVfs_ProcessId(void)
{
	return (unsigned int)getpid();
}
/* int (*xUid)(void) */
static int UnixVfs_uid(void)
{
	return (int)getuid();
}
/* int (*xGid)(void) */
static int UnixVfs_gid(void)
{
	return (int)getgid();
}
/* int (*xUmask)(int) */
static int UnixVfs_Umask(int new_mask)
{
	int old_mask;
	old_mask = umask(new_mask);
	return old_mask;
}
/* void (*xUsername)(ph7_context *) */
static void UnixVfs_Username(ph7_context *pCtx)
{
#ifndef PH7_UNIX_STATIC_BUILD
  struct passwd *pwd;
  uid_t uid;
  uid = getuid();
  pwd = getpwuid(uid);   /* Try getting UID for username */
  if (pwd == 0) {
    return;
  }
  /* Return the username */
  ph7_result_string(pCtx,pwd->pw_name,-1);
#else
  ph7_result_string(pCtx,"Unknown",-1);
#endif /* PH7_UNIX_STATIC_BUILD */
  return;
}
/* int (*xLink)(const char *,const char *,int) */
static int UnixVfs_link(const char *zSrc,const char *zTarget,int is_sym)
{
	int rc;
	if( is_sym ){
		/* Symbolic link */
		rc = symlink(zSrc,zTarget);
	}else{
		/* Hard link */
		rc = link(zSrc,zTarget);
	}
	return rc == 0 ? PH7_OK : -1;
}
/* int (*xChroot)(const char *) */
static int UnixVfs_chroot(const char *zRootDir)
{
	int rc;
	rc = chroot(zRootDir);
	return rc == 0 ? PH7_OK : -1;
}
/* Export the UNIX vfs */
PH7_PRIVATE const ph7_vfs sUnixVfs = {
	"Unix_vfs",
	PH7_VFS_VERSION,
	UnixVfs_chdir,    /* int (*xChdir)(const char *) */
	UnixVfs_chroot,   /* int (*xChroot)(const char *); */
	UnixVfs_getcwd,   /* int (*xGetcwd)(ph7_context *) */
	UnixVfs_mkdir,    /* int (*xMkdir)(const char *,int,int) */
	UnixVfs_rmdir,    /* int (*xRmdir)(const char *) */
	UnixVfs_isdir,    /* int (*xIsdir)(const char *) */
	UnixVfs_Rename,   /* int (*xRename)(const char *,const char *) */
	UnixVfs_Realpath, /*int (*xRealpath)(const char *,ph7_context *)*/
	UnixVfs_Sleep,    /* int (*xSleep)(unsigned int) */
	UnixVfs_unlink,   /* int (*xUnlink)(const char *) */
	UnixVfs_FileExists, /* int (*xFileExists)(const char *) */
	UnixVfs_Chmod, /*int (*xChmod)(const char *,int)*/
	UnixVfs_Chown, /*int (*xChown)(const char *,const char *)*/
	UnixVfs_Chgrp, /*int (*xChgrp)(const char *,const char *)*/
	UnixVfs_FreeSpace,  /* ph7_int64 (*xFreeSpace)(const char *) */
	UnixVfs_TotalSpace, /* ph7_int64 (*xTotalSpace)(const char *) */
	UnixVfs_FileSize, /* ph7_int64 (*xFileSize)(const char *) */
	UnixVfs_FileAtime,/* ph7_int64 (*xFileAtime)(const char *) */
	UnixVfs_FileMtime,/* ph7_int64 (*xFileMtime)(const char *) */
	UnixVfs_FileCtime,/* ph7_int64 (*xFileCtime)(const char *) */
	UnixVfs_Stat,  /* int (*xStat)(const char *,ph7_value *,ph7_value *) */
	UnixVfs_lStat, /* int (*xlStat)(const char *,ph7_value *,ph7_value *) */
	UnixVfs_isfile,     /* int (*xIsfile)(const char *) */
	UnixVfs_islink,     /* int (*xIslink)(const char *) */
	UnixVfs_isreadable, /* int (*xReadable)(const char *) */
	UnixVfs_iswritable, /* int (*xWritable)(const char *) */
	UnixVfs_isexecutable,/* int (*xExecutable)(const char *) */
	UnixVfs_Filetype,   /* int (*xFiletype)(const char *,ph7_context *) */
	UnixVfs_Getenv,     /* int (*xGetenv)(const char *,ph7_context *) */
	UnixVfs_Setenv,     /* int (*xSetenv)(const char *,const char *) */
	UnixVfs_Touch,      /* int (*xTouch)(const char *,ph7_int64,ph7_int64) */
	UnixVfs_Mmap,       /* int (*xMmap)(const char *,void **,ph7_int64 *) */
	UnixVfs_Unmap,      /* void (*xUnmap)(void *,ph7_int64);  */
	UnixVfs_link,       /* int (*xLink)(const char *,const char *,int) */
	UnixVfs_Umask,      /* int (*xUmask)(int) */
	UnixVfs_TempDir,    /* void (*xTempDir)(ph7_context *) */
	UnixVfs_ProcessId,  /* unsigned int (*xProcessId)(void) */
	UnixVfs_uid, /* int (*xUid)(void) */
	UnixVfs_gid, /* int (*xGid)(void) */
	UnixVfs_Username,    /* void (*xUsername)(ph7_context *) */
	0 /* int (*xExec)(const char *,ph7_context *) */
};
/* UNIX File IO */
#define PH7_UNIX_OPEN_MODE	0640 /* Default open mode */
/* int (*xOpen)(const char *,int,ph7_value *,void **) */
static int UnixFile_Open(const char *zPath,int iOpenMode,ph7_value *pResource,void **ppHandle)
{
	int iOpen = O_RDONLY;
	int fd;
	/* Set the desired flags according to the open mode */
	if( iOpenMode & PH7_IO_OPEN_CREATE ){
		/* Open existing file, or create if it doesn't exist */
		iOpen = O_CREAT;
		if( iOpenMode & PH7_IO_OPEN_TRUNC ){
			/* If the specified file exists and is writable, the function overwrites the file */
			iOpen |= O_TRUNC;
			SXUNUSED(pResource); /* cc warning */
		}
	}else if( iOpenMode & PH7_IO_OPEN_EXCL ){
		/* Creates a new file, only if it does not already exist.
		* If the file exists, it fails.
		*/
		iOpen = O_CREAT|O_EXCL;
	}else if( iOpenMode & PH7_IO_OPEN_TRUNC ){
		/* Opens a file and truncates it so that its size is zero bytes
		 * The file must exist.
		 */
		iOpen = O_RDWR|O_TRUNC;
	}
	if( iOpenMode & PH7_IO_OPEN_RDWR ){
		/* Read+Write access */
		iOpen &= ~O_RDONLY;
		iOpen |= O_RDWR;
	}else if( iOpenMode & PH7_IO_OPEN_WRONLY ){
		/* Write only access */
		iOpen &= ~O_RDONLY;
		iOpen |= O_WRONLY;
	}
	if( iOpenMode & PH7_IO_OPEN_APPEND ){
		/* Append mode */
		iOpen |= O_APPEND;
	}
#ifdef O_TEMP
	if( iOpenMode & PH7_IO_OPEN_TEMP ){
		/* File is temporary */
		iOpen |= O_TEMP;
	}
#endif
	/* Open the file now */
	fd = open(zPath,iOpen,PH7_UNIX_OPEN_MODE);
	if( fd < 0 ){
		/* IO error */
		return -1;
	}
	/* Save the handle */
	*ppHandle = SX_INT_TO_PTR(fd);
	return PH7_OK;
}
/* int (*xOpenDir)(const char *,ph7_value *,void **) */
static int UnixDir_Open(const char *zPath,ph7_value *pResource,void **ppHandle)
{
	DIR *pDir;
	/* Open the target directory */
	pDir = opendir(zPath);
	if( pDir == 0 ){
		SXUNUSED(pResource); /* Compiler warning */
		return -1;
	}
	/* Save our structure */
	*ppHandle = pDir;
	return PH7_OK;
}
/* void (*xCloseDir)(void *) */
static void UnixDir_Close(void *pUserData)
{
	closedir((DIR *)pUserData);
}
/* void (*xClose)(void *); */
static void UnixFile_Close(void *pUserData)
{
	close(SX_PTR_TO_INT(pUserData));
}
/* int (*xReadDir)(void *,ph7_context *) */
static int UnixDir_Read(void *pUserData,ph7_context *pCtx)
{
	DIR *pDir = (DIR *)pUserData;
	struct dirent *pEntry;
	char *zName = 0; /* cc warning */
	sxu32 n = 0;
	for(;;){
		pEntry = readdir(pDir);
		if( pEntry == 0 ){
			/* No more entries to process */
			return -1;
		}
		zName = pEntry->d_name;
		n = SyStrlen(zName);
		/* Ignore '.' && '..' */
		if( n > sizeof("..")-1 || zName[0] != '.' || ( n == sizeof("..")-1 && zName[1] != '.') ){
			break;
		}
		/* Next entry */
	}
	/* Return the current file name */
	ph7_result_string(pCtx,zName,(int)n);
	return PH7_OK;
}
/* void (*xRewindDir)(void *) */
static void UnixDir_Rewind(void *pUserData)
{
	rewinddir((DIR *)pUserData);
}
/* ph7_int64 (*xRead)(void *,void *,ph7_int64); */
static ph7_int64 UnixFile_Read(void *pUserData,void *pBuffer,ph7_int64 nDatatoRead)
{
	ssize_t nRd;
	nRd = read(SX_PTR_TO_INT(pUserData),pBuffer,(size_t)nDatatoRead);
	if( nRd < 1 ){
		/* EOF or IO error */
		return -1;
	}
	return (ph7_int64)nRd;
}
/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64); */
static ph7_int64 UnixFile_Write(void *pUserData,const void *pBuffer,ph7_int64 nWrite)
{
	const char *zData = (const char *)pBuffer;
	int fd = SX_PTR_TO_INT(pUserData);
	ph7_int64 nCount;
	ssize_t nWr;
	nCount = 0;
	for(;;){
		if( nWrite < 1 ){
			break;
		}
		nWr = write(fd,zData,(size_t)nWrite);
		if( nWr < 1 ){
			/* IO error */
			break;
		}
		nWrite -= nWr;
		nCount += nWr;
		zData += nWr;
	}
	if( nWrite > 0 ){
		return -1;
	}
	return nCount;
}
/* int (*xSeek)(void *,ph7_int64,int) */
static int UnixFile_Seek(void *pUserData,ph7_int64 iOfft,int whence)
{
	off_t iNew;
	switch(whence){
	case 1:/*SEEK_CUR*/
		whence = SEEK_CUR;
		break;
	case 2: /* SEEK_END */
		whence = SEEK_END;
		break;
	case 0: /* SEEK_SET */
	default:
		whence = SEEK_SET;
		break;
	}
	iNew = lseek(SX_PTR_TO_INT(pUserData),(off_t)iOfft,whence);
	if( iNew < 0 ){
		return -1;
	}
	return PH7_OK;
}
/* int (*xLock)(void *,int) */
static int UnixFile_Lock(void *pUserData,int lock_type)
{
	int fd = SX_PTR_TO_INT(pUserData);
	int rc = PH7_OK; /* cc warning */
	if( lock_type < 0 ){
		/* Unlock the file */
		rc = flock(fd,LOCK_UN);
	}else{
		if( lock_type == 1 ){
			/* Exculsive lock */
			rc = flock(fd,LOCK_EX);
		}else{
			/* Shared lock */
			rc = flock(fd,LOCK_SH);
		}
	}
	return !rc ? PH7_OK : -1;
}
/* ph7_int64 (*xTell)(void *) */
static ph7_int64 UnixFile_Tell(void *pUserData)
{
	off_t iNew;
	iNew = lseek(SX_PTR_TO_INT(pUserData),0,SEEK_CUR);
	return (ph7_int64)iNew;
}
/* int (*xTrunc)(void *,ph7_int64) */
static int UnixFile_Trunc(void *pUserData,ph7_int64 nOfft)
{
	int rc;
	rc = ftruncate(SX_PTR_TO_INT(pUserData),(off_t)nOfft);
	if( rc != 0 ){
		return -1;
	}
	return PH7_OK;
}
/* int (*xSync)(void *); */
static int UnixFile_Sync(void *pUserData)
{
	int rc;
	rc = fsync(SX_PTR_TO_INT(pUserData));
	return rc == 0 ? PH7_OK : - 1;
}
/* int (*xStat)(void *,ph7_value *,ph7_value *) */
static int UnixFile_Stat(void *pUserData,ph7_value *pArray,ph7_value *pWorker)
{
	struct stat st;
	int rc;
	rc = fstat(SX_PTR_TO_INT(pUserData),&st);
	if( rc != 0 ){
	 return -1;
	}
	/* dev */
	ph7_value_int64(pWorker,(ph7_int64)st.st_dev);
	ph7_array_add_strkey_elem(pArray,"dev",pWorker); /* Will make it's own copy */
	/* ino */
	ph7_value_int64(pWorker,(ph7_int64)st.st_ino);
	ph7_array_add_strkey_elem(pArray,"ino",pWorker); /* Will make it's own copy */
	/* mode */
	ph7_value_int(pWorker,(int)st.st_mode);
	ph7_array_add_strkey_elem(pArray,"mode",pWorker);
	/* nlink */
	ph7_value_int(pWorker,(int)st.st_nlink);
	ph7_array_add_strkey_elem(pArray,"nlink",pWorker); /* Will make it's own copy */
	/* uid,gid,rdev */
	ph7_value_int(pWorker,(int)st.st_uid);
	ph7_array_add_strkey_elem(pArray,"uid",pWorker);
	ph7_value_int(pWorker,(int)st.st_gid);
	ph7_array_add_strkey_elem(pArray,"gid",pWorker);
	ph7_value_int(pWorker,(int)st.st_rdev);
	ph7_array_add_strkey_elem(pArray,"rdev",pWorker);
	/* size */
	ph7_value_int64(pWorker,(ph7_int64)st.st_size);
	ph7_array_add_strkey_elem(pArray,"size",pWorker); /* Will make it's own copy */
	/* atime */
	ph7_value_int64(pWorker,(ph7_int64)st.st_atime);
	ph7_array_add_strkey_elem(pArray,"atime",pWorker); /* Will make it's own copy */
	/* mtime */
	ph7_value_int64(pWorker,(ph7_int64)st.st_mtime);
	ph7_array_add_strkey_elem(pArray,"mtime",pWorker); /* Will make it's own copy */
	/* ctime */
	ph7_value_int64(pWorker,(ph7_int64)st.st_ctime);
	ph7_array_add_strkey_elem(pArray,"ctime",pWorker); /* Will make it's own copy */
	/* blksize,blocks */
	ph7_value_int(pWorker,(int)st.st_blksize);
	ph7_array_add_strkey_elem(pArray,"blksize",pWorker);
	ph7_value_int(pWorker,(int)st.st_blocks);
	ph7_array_add_strkey_elem(pArray,"blocks",pWorker);
	return PH7_OK;
}
/* Export the file:// stream */
PH7_PRIVATE const ph7_io_stream sUnixFileStream = {
	"file", /* Stream name */
	PH7_IO_STREAM_VERSION,
	UnixFile_Open,  /* xOpen */
	UnixDir_Open,   /* xOpenDir */
	UnixFile_Close, /* xClose */
	UnixDir_Close,  /* xCloseDir */
	UnixFile_Read,  /* xRead */
	UnixDir_Read,   /* xReadDir */
	UnixFile_Write, /* xWrite */
	UnixFile_Seek,  /* xSeek */
	UnixFile_Lock,  /* xLock */
	UnixDir_Rewind, /* xRewindDir */
	UnixFile_Tell,  /* xTell */
	UnixFile_Trunc, /* xTrunc */
	UnixFile_Sync,  /* xSeek */
	UnixFile_Stat   /* xStat */
};
#endif /* __UNIXES__ */
