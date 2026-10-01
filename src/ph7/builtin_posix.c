/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
/* ctermid(), eaccess(), makedev() and `struct utsname`'s domainname are outside
 * the strict POSIX subset a default compile exposes, and php's own build asks
 * for them the same way. This has to come BEFORE any system header, which is
 * why it is above ph7int.h rather than beside the includes below. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#include "ph7int.h"
/*
 * Section:
 *    php's posix extension: the process, user, group and descriptor calls a
 *    command-line program asks the system about itself with.
 * Status:
 *    Stable.
 *
 * php's ext/posix is a thin shell over the C library's POSIX.1 surface, so
 * every answer here is the platform's and the only thing to reproduce is the
 * SHAPE php gives it. That shape is four rules and a handful of exceptions:
 *
 *   The EXTENSION IS NOT THERE ON WINDOWS. php builds no ext/posix for it, so
 *   `function_exists('posix_kill')` is false and `extension_loaded('posix')` is
 *   false on a Windows php -- which is exactly what a program that guards its
 *   use of them expects to find. This whole translation unit is empty there,
 *   and the constants are not defined either.
 *
 *   FAILURE IS `false` (or a negative number) AND AN ERRNO, never a warning.
 *   The errno is REMEMBERED: `posix_get_last_error()` (and its alias
 *   `posix_errno()`) answers what the last call that stored one saw, and
 *   nothing clears it -- a later SUCCESS leaves it standing. Which calls store
 *   one is per-function and had to be measured: `posix_isatty()` stores errno
 *   whenever it answers FALSE (so a pipe leaves ENOTTY behind, and that is what
 *   php reports); the four `getpw*`/`getgr*` lookups store errno
 *   UNCONDITIONALLY, so a name that simply is not there stores 0; and
 *   `posix_sysconf()`, `posix_times()` and `posix_uname()` do not store on the
 *   paths that answer.
 *
 *   THE ARRAY KEY ORDER IS php's, not the struct's. `posix_uname()` is
 *   sysname, nodename, release, version, machine and -- where the platform's
 *   `struct utsname` carries one -- domainname. `posix_times()` is ticks,
 *   utime, stime, cutime, cstime. `posix_getpwnam()` is name, passwd, uid,
 *   gid, gecos, dir, shell, and `posix_getgrnam()` is name, passwd, MEMBERS,
 *   gid -- the members list third, not last. `posix_getrlimit()` with no
 *   argument answers twenty keys, `soft X` and `hard X` for ten resources in
 *   php's own order, and an INFINITE limit is the STRING `unlimited` rather
 *   than a number.
 *
 *   A DESCRIPTOR ARGUMENT is an int or a php STREAM. The three doors that take
 *   one (`posix_isatty`, `posix_ttyname`, `posix_fpathconf`) accept either, and
 *   a stream with no descriptor behind it -- a memory buffer, a data:// payload
 *   -- is php's `Could not use stream of type '%s'` warning with the stream's
 *   own label, the same one `stream_get_meta_data()` reports. An int outside
 *   0..2147483647 is `must be between 0 and 2147483647`, and anything that is
 *   neither is `must be of type int|resource, %s given`. All three are
 *   E_WARNINGs answering false, not TypeErrors.
 *
 * Two of php's own quirks are reproduced rather than corrected, because a
 * program can see both:
 *
 *   `posix_mknod()` screens its major number with `(mode & S_IFCHR) || (mode &
 *   S_IFBLK)`, which is a BIT test rather than a type test -- and S_IFSOCK
 *   (0140000) shares a bit with S_IFBLK (0060000), so asking for a socket node
 *   without a major number raises the ValueError that names the two other
 *   modes. `posix_setsid()` answers the raw -1 rather than false when the
 *   caller is already a process-group leader.
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC
#ifndef __WINNT__

#include <errno.h>
#include <stdio.h>        /* ctermid, L_ctermid */
#include <string.h>       /* strerror */
#ifdef __linux__
#include <sys/sysmacros.h>/* makedev; the BSDs and macOS have it in sys/types.h */
#endif
#include <unistd.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <limits.h>

/* php's own MAXPATHLEN for getcwd()/ctermid(). */
#ifndef PH7_POSIX_PATHBUF
#ifdef PATH_MAX
#define PH7_POSIX_PATHBUF (PATH_MAX + 1)
#else
#define PH7_POSIX_PATHBUF 4096
#endif
#endif

/* --- The remembered errno ---------------------------------------------- */

/*
 * Answer false and remember WHY: php keeps `POSIX_G(last_error)` for the whole
 * module, and this keeps it per VM -- the same lifetime for a program, and two
 * embedded VMs stay apart. Which calls store one is per-function and measured;
 * the four database lookups store on the NOT-FOUND answer, where errno is 0, so
 * a name that is not there OVERWRITES an earlier failure with a zero.
 */
static int PxFail(ph7_context *pCtx)
{
	pCtx->pVm->iPosixErr = errno;
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}

/* --- Screens ------------------------------------------------------------ */

/*
 * A descriptor argument: a php stream to take one from, or anything php's WEAK
 * int parse accepts. php's own helper is that parse, which is why a numeric
 * STRING is taken in silence and a non-numeric one is a warning; the two
 * refusals below are PHL's engine-wide scope policy where php only deprecates,
 * and they are the same TypeError every other int parameter answers with.
 *
 * Answers the descriptor, or -1 after raising the diagnostic. *pbTyped says
 * whether the argument was one the parse ACCEPTED: posix_ttyname() converts a
 * refused one anyway (php's `zval_get_long` after the warning, which is where
 * an object's second diagnostic comes from) and posix_isatty() does not.
 * bThrow is posix_fpathconf(), whose refusal is a TypeError rather than a
 * warning -- php screens that one at the parameter and the other two inside
 * the body, and the difference shows.
 */
static int PxFd(ph7_context *pCtx,ph7_value *pArg,int *pbTyped,int bThrow)
{
	char zGiven[64];
	sxi64 iFd = 0;
	if( pbTyped ){
		*pbTyped = 1;
	}
	if( ph7_value_is_resource(pArg) ){
		io_private *pDev = (io_private *)ph7_value_to_resource(pArg);
		int fd;
		if( IO_PRIVATE_INVALID(pDev) ){
			PH7_VmThrowException(pCtx,"TypeError",
				"%s(): supplied resource is not a valid stream resource",
				ph7_function_name(pCtx));
			if( pbTyped ){
				*pbTyped = 0;
			}
			return -1;
		}
		fd = PH7_StreamPosixFd(pDev);
		if( fd < 0 ){
			/* php's stream layer could hand back no descriptor for it. */
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Could not use stream of type '%s'",PH7_StreamTypeLabel(pDev));
			if( pbTyped ){
				*pbTyped = 0;
			}
			return -1;
		}
		return fd;
	}
	if( ph7_value_is_null(pArg) || ph7_value_is_float(pArg) ){
		/* null and a LOSSY float are what php merely deprecates here; PHL
		 * refuses both, engine-wide, through the shared int screen. */
		if( PH7_IntArgResolve(pCtx,pArg,ph7_function_name(pCtx),1,
			"$file_descriptor","int",&iFd) != PH7_OK ){
			if( pbTyped ){
				*pbTyped = 0;
			}
			return -1;
		}
	}else if( ph7_value_is_int(pArg) || ph7_value_is_bool(pArg) ){
		iFd = ph7_value_to_int64(pArg);
	}else if( ph7_value_is_string(pArg) && PH7_MemObjStringIsNumeric(pArg) ){
		iFd = ph7_value_to_int64(pArg);
	}else{
		if( bThrow ){
			PH7_VmThrowException(pCtx,"TypeError",
				"%s(): Argument #1 ($file_descriptor) must be of type int|resource, %s given",
				ph7_function_name(pCtx),VmValueGivenName(pArg,zGiven,sizeof(zGiven)));
		}else{
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Argument #1 ($file_descriptor) must be of type int|resource, %s given",
				VmValueGivenName(pArg,zGiven,sizeof(zGiven)));
		}
		if( pbTyped ){
			*pbTyped = 0;
		}
		return -1;
	}
	if( iFd < 0 || iFd > 2147483647 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Argument #1 ($file_descriptor) must be between 0 and 2147483647");
		if( pbTyped ){
			*pbTyped = 0;
		}
		return -1;
	}
	return (int)iFd;
}

/* --- The process identity ----------------------------------------------- */

/* int posix_getpid() */
PH7_PRIVATE int PH7_builtin_posix_getpid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int64(pCtx,(sxi64)getpid());
	return PH7_OK;
}
/* int posix_getppid() */
PH7_PRIVATE int PH7_builtin_posix_getppid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int64(pCtx,(sxi64)getppid());
	return PH7_OK;
}
/* int posix_getuid() */
PH7_PRIVATE int PH7_builtin_posix_getuid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int64(pCtx,(sxi64)getuid());
	return PH7_OK;
}
/* int posix_geteuid() */
PH7_PRIVATE int PH7_builtin_posix_geteuid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int64(pCtx,(sxi64)geteuid());
	return PH7_OK;
}
/* int posix_getgid() */
PH7_PRIVATE int PH7_builtin_posix_getgid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int64(pCtx,(sxi64)getgid());
	return PH7_OK;
}
/* int posix_getegid() */
PH7_PRIVATE int PH7_builtin_posix_getegid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int64(pCtx,(sxi64)getegid());
	return PH7_OK;
}
/* bool posix_setuid(int $user_id) */
PH7_PRIVATE int PH7_builtin_posix_setuid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 || setuid((uid_t)ph7_value_to_int64(apArg[0])) != 0 ){
		return PxFail(pCtx);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* bool posix_seteuid(int $user_id) */
PH7_PRIVATE int PH7_builtin_posix_seteuid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 || seteuid((uid_t)ph7_value_to_int64(apArg[0])) != 0 ){
		return PxFail(pCtx);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* bool posix_setgid(int $group_id) */
PH7_PRIVATE int PH7_builtin_posix_setgid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 || setgid((gid_t)ph7_value_to_int64(apArg[0])) != 0 ){
		return PxFail(pCtx);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* bool posix_setegid(int $group_id) */
PH7_PRIVATE int PH7_builtin_posix_setegid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 || setegid((gid_t)ph7_value_to_int64(apArg[0])) != 0 ){
		return PxFail(pCtx);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * array|false posix_getgroups()
 *  The supplementary groups, asked for twice: once for the count and once for
 *  the list, which is how the interface is meant to be used.
 */
PH7_PRIVATE int PH7_builtin_posix_getgroups(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	gid_t *aGid;
	ph7_value *pArray,*pVal;
	int n,i;
	SXUNUSED(nArg); SXUNUSED(apArg);
	/* Asked for twice, as the interface is meant to be: once for the count and
	 * once for the list. A fixed array is not an option -- NGROUPS_MAX is 65536
	 * on Linux, which is a quarter of a megabyte of stack for a list that is
	 * usually under ten entries long. */
	n = getgroups(0,0);
	if( n < 0 ){
		return PxFail(pCtx);
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( n > 0 ){
		aGid = (gid_t *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)n * (sxu32)sizeof(gid_t));
		if( aGid == 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		n = getgroups(n,aGid);
		if( n < 0 ){
			SyMemBackendFree(&pCtx->pVm->sAllocator,aGid);
			return PxFail(pCtx);
		}
		for( i = 0 ; i < n ; ++i ){
			ph7_value_int64(pVal,(sxi64)aGid[i]);
			ph7_array_add_elem(pArray,0,pVal);
		}
		SyMemBackendFree(&pCtx->pVm->sAllocator,aGid);
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/* string|false posix_getlogin() */
PH7_PRIVATE int PH7_builtin_posix_getlogin(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	char *zName;
	SXUNUSED(nArg); SXUNUSED(apArg);
	errno = 0;
	zName = getlogin();
	if( zName == 0 ){
		return PxFail(pCtx);
	}
	ph7_result_string(pCtx,zName,-1);
	return PH7_OK;
}

/* --- Process groups and sessions ---------------------------------------- */

/* int posix_getpgrp() */
PH7_PRIVATE int PH7_builtin_posix_getpgrp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int64(pCtx,(sxi64)getpgrp());
	return PH7_OK;
}
/*
 * int posix_setsid()
 *  php hands back what setsid() answered, so a caller that is ALREADY a process
 *  group leader reads the raw -1 rather than false.
 */
PH7_PRIVATE int PH7_builtin_posix_setsid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int64(pCtx,(sxi64)setsid());
	return PH7_OK;
}
/* bool posix_setpgid(int $process_id, int $process_group_id) */
PH7_PRIVATE int PH7_builtin_posix_setpgid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 2
	 || setpgid((pid_t)ph7_value_to_int64(apArg[0]),(pid_t)ph7_value_to_int64(apArg[1])) != 0 ){
		return PxFail(pCtx);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* int|false posix_getpgid(int $process_id) */
PH7_PRIVATE int PH7_builtin_posix_getpgid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	pid_t iRes;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iRes = getpgid((pid_t)ph7_value_to_int64(apArg[0]));
	if( iRes < 0 ){
		return PxFail(pCtx);
	}
	ph7_result_int64(pCtx,(sxi64)iRes);
	return PH7_OK;
}
/* int|false posix_getsid(int $process_id) */
PH7_PRIVATE int PH7_builtin_posix_getsid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	pid_t iRes;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iRes = getsid((pid_t)ph7_value_to_int64(apArg[0]));
	if( iRes < 0 ){
		return PxFail(pCtx);
	}
	ph7_result_int64(pCtx,(sxi64)iRes);
	return PH7_OK;
}
/* bool posix_kill(int $process_id, int $signal) */
PH7_PRIVATE int PH7_builtin_posix_kill(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 2
	 || kill((pid_t)ph7_value_to_int64(apArg[0]),(int)ph7_value_to_int64(apArg[1])) != 0 ){
		return PxFail(pCtx);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}

/* --- What the system says about itself ---------------------------------- */

/* array|false posix_uname() */
PH7_PRIVATE int PH7_builtin_posix_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct utsname sName;
	ph7_value *pArray,*pVal;
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( uname(&sName) < 0 ){
		return PxFail(pCtx);
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_value_string(pVal,sName.sysname,-1);
	ph7_array_add_strkey_elem(pArray,"sysname",pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_string(pVal,sName.nodename,-1);
	ph7_array_add_strkey_elem(pArray,"nodename",pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_string(pVal,sName.release,-1);
	ph7_array_add_strkey_elem(pArray,"release",pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_string(pVal,sName.version,-1);
	ph7_array_add_strkey_elem(pArray,"version",pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_string(pVal,sName.machine,-1);
	ph7_array_add_strkey_elem(pArray,"machine",pVal);
#ifdef __linux__
	ph7_value_reset_string_cursor(pVal);
	ph7_value_string(pVal,sName.domainname,-1);
	ph7_array_add_strkey_elem(pArray,"domainname",pVal);
#endif
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/* array|false posix_times() */
PH7_PRIVATE int PH7_builtin_posix_times(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct tms sTms;
	clock_t iTicks;
	ph7_value *pArray,*pVal;
	SXUNUSED(nArg); SXUNUSED(apArg);
	iTicks = times(&sTms);
	if( iTicks == (clock_t)-1 ){
		return PxFail(pCtx);
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_value_int64(pVal,(sxi64)iTicks);
	ph7_array_add_strkey_elem(pArray,"ticks",pVal);
	ph7_value_int64(pVal,(sxi64)sTms.tms_utime);
	ph7_array_add_strkey_elem(pArray,"utime",pVal);
	ph7_value_int64(pVal,(sxi64)sTms.tms_stime);
	ph7_array_add_strkey_elem(pArray,"stime",pVal);
	ph7_value_int64(pVal,(sxi64)sTms.tms_cutime);
	ph7_array_add_strkey_elem(pArray,"cutime",pVal);
	ph7_value_int64(pVal,(sxi64)sTms.tms_cstime);
	ph7_array_add_strkey_elem(pArray,"cstime",pVal);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/* string|false posix_ctermid() */
PH7_PRIVATE int PH7_builtin_posix_ctermid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	char zBuf[L_ctermid];
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( ctermid(zBuf) == 0 ){
		return PxFail(pCtx);
	}
	ph7_result_string(pCtx,zBuf,-1);
	return PH7_OK;
}
/* string|false posix_getcwd() */
PH7_PRIVATE int PH7_builtin_posix_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	char zBuf[PH7_POSIX_PATHBUF];
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( getcwd(zBuf,sizeof(zBuf)) == 0 ){
		return PxFail(pCtx);
	}
	ph7_result_string(pCtx,zBuf,-1);
	return PH7_OK;
}

/* --- Descriptors --------------------------------------------------------- */

/*
 * bool posix_isatty(int|resource $file_descriptor)
 *  php's terminal question, and the one every CLI tool asks before it colours
 *  its output: `sebastian/environment`'s isInteractive() is exactly this call,
 *  so PHPUnit's whole interactive surface hangs off it.
 */
PH7_PRIVATE int PH7_builtin_posix_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int fd;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	fd = PxFd(pCtx,apArg[0],0,0);
	if( fd < 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( isatty(fd) ){
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	/* php stores the errno of the FALSE answer too, which is why a pipe leaves
	 * ENOTTY where a closed descriptor leaves EBADF. */
	return PxFail(pCtx);
}
/* string|false posix_ttyname(int|resource $file_descriptor) */
PH7_PRIVATE int PH7_builtin_posix_ttyname(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zName;
	int fd,bTyped = 1;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	fd = PxFd(pCtx,apArg[0],&bTyped,0);
	if( fd < 0 ){
		/* php warns and then CONVERTS the argument anyway, which is where a
		 * second diagnostic comes from for an object; a resource it could not
		 * take a descriptor from, and PHL's own refusals, stop here. */
		if( bTyped || ph7_value_is_resource(apArg[0])
		 || ph7_value_is_null(apArg[0]) || ph7_value_is_float(apArg[0]) ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		fd = (int)ph7_value_to_int64(apArg[0]);
		if( fd < 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	zName = ttyname(fd);
	if( zName == 0 ){
		return PxFail(pCtx);
	}
	ph7_result_string(pCtx,zName,-1);
	return PH7_OK;
}

/* --- Files, nodes and access -------------------------------------------- */

/* bool posix_mkfifo(string $filename, int $permissions) */
PH7_PRIVATE int PH7_builtin_posix_mkfifo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	int nPath = 0;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zPath = ph7_value_to_string(apArg[0],&nPath);
	SXUNUSED(nPath);
	if( mkfifo(zPath,(mode_t)ph7_value_to_int64(apArg[1])) != 0 ){
		return PxFail(pCtx);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool posix_mknod(string $filename, int $flags, int $major = 0, int $minor = 0)
 *  The major-number screen is php's, and so is its ODDITY: the test is
 *  `(flags & S_IFCHR) || (flags & S_IFBLK)`, a bit test rather than a type
 *  test, and S_IFSOCK shares a bit with S_IFBLK -- so asking for a socket node
 *  with no major number raises the ValueError that names the two other modes.
 */
PH7_PRIVATE int PH7_builtin_posix_mknod(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	int nPath = 0;
	sxi64 iFlags,iMajor = 0,iMinor = 0;
	dev_t iDev = 0;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zPath = ph7_value_to_string(apArg[0],&nPath);
	SXUNUSED(nPath);
	iFlags = ph7_value_to_int64(apArg[1]);
	if( nArg > 2 ){
		iMajor = ph7_value_to_int64(apArg[2]);
	}
	if( nArg > 3 ){
		iMinor = ph7_value_to_int64(apArg[3]);
	}
	if( (iFlags & S_IFCHR) || (iFlags & S_IFBLK) ){
		if( iMajor == 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #3 ($major) cannot be 0 for the POSIX_S_IFCHR and POSIX_S_IFBLK modes",
				ph7_function_name(pCtx));
		}
		iDev = makedev((unsigned int)iMajor,(unsigned int)iMinor);
	}
	if( mknod(zPath,(mode_t)iFlags,iDev) != 0 ){
		return PxFail(pCtx);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* The shared body of posix_access() and posix_eaccess(). */
static int PxAccess(ph7_context *pCtx,int nArg,ph7_value **apArg,int bEffective)
{
	const char *zPath;
	int nPath = 0,iMode = 0,rc;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zPath = ph7_value_to_string(apArg[0],&nPath);
	SXUNUSED(nPath);
	if( nArg > 1 ){
		iMode = (int)ph7_value_to_int64(apArg[1]);
	}
	if( bEffective ){
#if defined(__GLIBC__)
		rc = eaccess(zPath,iMode);
#else
		/* Without the GNU/BSD spelling there is nothing to ask the EFFECTIVE
		 * ids with, so the real ones answer -- which is the same thing for
		 * every process that is not setuid. */
		rc = access(zPath,iMode);
#endif
	}else{
		rc = access(zPath,iMode);
	}
	if( rc != 0 ){
		return PxFail(pCtx);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* bool posix_access(string $filename, int $flags = 0) */
PH7_PRIVATE int PH7_builtin_posix_access(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return PxAccess(pCtx,nArg,apArg,0);
}
/*
 * bool posix_eaccess(string $filename, int $flags = 0)
 *  The same question asked with the EFFECTIVE ids, which is what a setuid
 *  program has to ask.
 */
PH7_PRIVATE int PH7_builtin_posix_eaccess(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return PxAccess(pCtx,nArg,apArg,1);
}

/* --- The user and group databases --------------------------------------- */

/* Fill php's seven passwd keys, in php's order. */
static int PxPasswdArray(ph7_context *pCtx,struct passwd *pPw)
{
	ph7_value *pArray = ph7_context_new_array(pCtx);
	ph7_value *pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_value_string(pVal,pPw->pw_name ? pPw->pw_name : "",-1);
	ph7_array_add_strkey_elem(pArray,"name",pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_string(pVal,pPw->pw_passwd ? pPw->pw_passwd : "",-1);
	ph7_array_add_strkey_elem(pArray,"passwd",pVal);
	ph7_value_int64(pVal,(sxi64)pPw->pw_uid);
	ph7_array_add_strkey_elem(pArray,"uid",pVal);
	ph7_value_int64(pVal,(sxi64)pPw->pw_gid);
	ph7_array_add_strkey_elem(pArray,"gid",pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_string(pVal,pPw->pw_gecos ? pPw->pw_gecos : "",-1);
	ph7_array_add_strkey_elem(pArray,"gecos",pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_string(pVal,pPw->pw_dir ? pPw->pw_dir : "",-1);
	ph7_array_add_strkey_elem(pArray,"dir",pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_string(pVal,pPw->pw_shell ? pPw->pw_shell : "",-1);
	ph7_array_add_strkey_elem(pArray,"shell",pVal);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/* Fill php's four group keys -- with `members` THIRD, which is php's order. */
static int PxGroupArray(ph7_context *pCtx,struct group *pGr)
{
	ph7_value *pArray = ph7_context_new_array(pCtx);
	ph7_value *pMembers = ph7_context_new_array(pCtx);
	ph7_value *pVal = ph7_context_new_scalar(pCtx);
	int i;
	if( pArray == 0 || pMembers == 0 || pVal == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_value_string(pVal,pGr->gr_name ? pGr->gr_name : "",-1);
	ph7_array_add_strkey_elem(pArray,"name",pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_string(pVal,pGr->gr_passwd ? pGr->gr_passwd : "",-1);
	ph7_array_add_strkey_elem(pArray,"passwd",pVal);
	for( i = 0 ; pGr->gr_mem && pGr->gr_mem[i] ; ++i ){
		ph7_value_reset_string_cursor(pVal);
		ph7_value_string(pVal,pGr->gr_mem[i],-1);
		ph7_array_add_elem(pMembers,0,pVal);
	}
	ph7_array_add_strkey_elem(pArray,"members",pMembers);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_int64(pVal,(sxi64)pGr->gr_gid);
	ph7_array_add_strkey_elem(pArray,"gid",pVal);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/* array|false posix_getpwnam(string $username) */
PH7_PRIVATE int PH7_builtin_posix_getpwnam(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct passwd *pPw;
	const char *zName;
	int nName = 0;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	SXUNUSED(nName);
	errno = 0;
	pPw = getpwnam(zName);
	if( pPw == 0 ){
		/* php stores errno for the NOT-FOUND answer, and a name that simply is
		 * not there leaves errno at 0 -- so this OVERWRITES an earlier failure
		 * with a zero rather than leaving it standing. */
		return PxFail(pCtx);
	}
	return PxPasswdArray(pCtx,pPw);
}
/* array|false posix_getpwuid(int $user_id) */
PH7_PRIVATE int PH7_builtin_posix_getpwuid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct passwd *pPw;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	errno = 0;
	pPw = getpwuid((uid_t)ph7_value_to_int64(apArg[0]));
	if( pPw == 0 ){
		return PxFail(pCtx);
	}
	return PxPasswdArray(pCtx,pPw);
}
/* array|false posix_getgrnam(string $name) */
PH7_PRIVATE int PH7_builtin_posix_getgrnam(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct group *pGr;
	const char *zName;
	int nName = 0;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	SXUNUSED(nName);
	errno = 0;
	pGr = getgrnam(zName);
	if( pGr == 0 ){
		return PxFail(pCtx);
	}
	return PxGroupArray(pCtx,pGr);
}
/* array|false posix_getgrgid(int $group_id) */
PH7_PRIVATE int PH7_builtin_posix_getgrgid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct group *pGr;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	errno = 0;
	pGr = getgrgid((gid_t)ph7_value_to_int64(apArg[0]));
	if( pGr == 0 ){
		return PxFail(pCtx);
	}
	return PxGroupArray(pCtx,pGr);
}
/* bool posix_initgroups(string $username, int $group_id) */
PH7_PRIVATE int PH7_builtin_posix_initgroups(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zName;
	int nName = 0;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	SXUNUSED(nName);
	/* php stores NO errno for this one -- it is a bare RETURN_BOOL. */
	ph7_result_bool(pCtx,initgroups(zName,(gid_t)ph7_value_to_int64(apArg[1])) == 0);
	return PH7_OK;
}

/* --- Limits and configuration ------------------------------------------- */

/*
 * php's ten resources, in php's own order -- which is what decides the key
 * order of `posix_getrlimit()`'s twenty-entry answer.
 */
typedef struct px_rlimit px_rlimit;
struct px_rlimit {
	const char *zName;   /* php's own label for the pair */
	int iRes;            /* the platform's RLIMIT_* number */
};
static const px_rlimit aPxRlimit[] = {
	{ "core",      RLIMIT_CORE   },
	{ "data",      RLIMIT_DATA   },
	{ "stack",     RLIMIT_STACK  },
#ifdef RLIMIT_AS
	{ "totalmem",  RLIMIT_AS     },
#endif
#ifdef RLIMIT_RSS
	{ "rss",       RLIMIT_RSS    },
#endif
#ifdef RLIMIT_NPROC
	{ "maxproc",   RLIMIT_NPROC  },
#endif
#ifdef RLIMIT_MEMLOCK
	{ "memlock",   RLIMIT_MEMLOCK},
#endif
	{ "cpu",       RLIMIT_CPU    },
	{ "filesize",  RLIMIT_FSIZE  },
	{ "openfiles", RLIMIT_NOFILE }
};
/* One limit value: a number, or php's STRING `unlimited`. */
static void PxRlimitValue(ph7_value *pVal,rlim_t iVal)
{
	if( iVal == RLIM_INFINITY ){
		ph7_value_reset_string_cursor(pVal);
		ph7_value_string(pVal,"unlimited",(int)sizeof("unlimited")-1);
	}else{
		ph7_value_int64(pVal,(sxi64)iVal);
	}
}
/* array|false posix_getrlimit(?int $resource = null) */
PH7_PRIVATE int PH7_builtin_posix_getrlimit(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct rlimit sLimit;
	ph7_value *pArray,*pVal;
	sxu32 i;
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		/* One resource: php answers the pair as a LIST. */
		if( getrlimit((int)ph7_value_to_int64(apArg[0]),&sLimit) != 0 ){
			return PxFail(pCtx);
		}
		pArray = ph7_context_new_array(pCtx);
		pVal = ph7_context_new_scalar(pCtx);
		if( pArray == 0 || pVal == 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		PxRlimitValue(pVal,sLimit.rlim_cur);
		ph7_array_add_elem(pArray,0,pVal);
		PxRlimitValue(pVal,sLimit.rlim_max);
		ph7_array_add_elem(pArray,0,pVal);
		ph7_result_value(pCtx,pArray);
		return PH7_OK;
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	for( i = 0 ; i < SX_ARRAYSIZE(aPxRlimit) ; ++i ){
		char zKey[32];
		if( getrlimit(aPxRlimit[i].iRes,&sLimit) != 0 ){
			continue;
		}
		SyBufferFormat(zKey,(sxu32)sizeof(zKey),"soft %s",aPxRlimit[i].zName);
		PxRlimitValue(pVal,sLimit.rlim_cur);
		ph7_array_add_strkey_elem(pArray,zKey,pVal);
		SyBufferFormat(zKey,(sxu32)sizeof(zKey),"hard %s",aPxRlimit[i].zName);
		PxRlimitValue(pVal,sLimit.rlim_max);
		ph7_array_add_strkey_elem(pArray,zKey,pVal);
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/* bool posix_setrlimit(int $resource, int $soft_limit, int $hard_limit) */
PH7_PRIVATE int PH7_builtin_posix_setrlimit(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct rlimit sLimit;
	sxi64 iSoft,iHard;
	if( nArg < 3 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iSoft = ph7_value_to_int64(apArg[1]);
	iHard = ph7_value_to_int64(apArg[2]);
	sLimit.rlim_cur = iSoft < 0 ? RLIM_INFINITY : (rlim_t)iSoft;
	sLimit.rlim_max = iHard < 0 ? RLIM_INFINITY : (rlim_t)iHard;
	if( setrlimit((int)ph7_value_to_int64(apArg[0]),&sLimit) != 0 ){
		return PxFail(pCtx);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * int posix_sysconf(int $conf_id)
 *  php hands back whatever sysconf() answered, -1 included, and stores no
 *  errno for it.
 */
PH7_PRIVATE int PH7_builtin_posix_sysconf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		ph7_result_int64(pCtx,-1);
		return PH7_OK;
	}
	ph7_result_int64(pCtx,(sxi64)sysconf((int)ph7_value_to_int64(apArg[0])));
	return PH7_OK;
}
/* int|false posix_pathconf(string $path, int $name) */
PH7_PRIVATE int PH7_builtin_posix_pathconf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPath;
	int nPath = 0;
	long iRes;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zPath = ph7_value_to_string(apArg[0],&nPath);
	if( nPath < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($path) must not be empty",ph7_function_name(pCtx));
	}
	errno = 0;
	iRes = pathconf(zPath,(int)ph7_value_to_int64(apArg[1]));
	if( iRes < 0 && errno != 0 ){
		return PxFail(pCtx);
	}
	ph7_result_int64(pCtx,(sxi64)iRes);
	return PH7_OK;
}
/* int|false posix_fpathconf(int|resource $file_descriptor, int $name) */
PH7_PRIVATE int PH7_builtin_posix_fpathconf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	long iRes;
	int fd;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	fd = PxFd(pCtx,apArg[0],0,1);
	if( fd < 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	errno = 0;
	iRes = fpathconf(fd,(int)ph7_value_to_int64(apArg[1]));
	if( iRes < 0 && errno != 0 ){
		return PxFail(pCtx);
	}
	ph7_result_int64(pCtx,(sxi64)iRes);
	return PH7_OK;
}

/* --- The remembered errno, read back ------------------------------------ */

/* int posix_get_last_error() / int posix_errno() */
PH7_PRIVATE int PH7_builtin_posix_get_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int64(pCtx,(sxi64)pCtx->pVm->iPosixErr);
	return PH7_OK;
}
/*
 * string posix_strerror(int $error_code)
 *  The C library's own sentence, which is what php prints too -- including its
 *  wording for a number that names no error.
 */
PH7_PRIVATE int PH7_builtin_posix_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zMsg;
	if( nArg < 1 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	zMsg = strerror((int)ph7_value_to_int64(apArg[0]));
	ph7_result_string(pCtx,zMsg ? zMsg : "",-1);
	return PH7_OK;
}

#endif /* __WINNT__ */
#endif /* PH7_DISABLE_BUILTIN_FUNC */
