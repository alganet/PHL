# src/ph7/builtin_posix.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 351/558 lines (62.90%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | ``/* ctermid(), eaccess(), makedev() and `struct utsname`'s domainname are outside`` |
|    - |    6 | ` * the strict POSIX subset a default compile exposes, and php's own build asks` |
|    - |    7 | ` * for them the same way. This has to come BEFORE any system header, which is` |
|    - |    8 | ` * why it is above ph7int.h rather than beside the includes below. */` |
|    - |    9 | `#ifndef _GNU_SOURCE` |
|    - |   10 | `#define _GNU_SOURCE 1` |
|    - |   11 | `#endif` |
|    - |   12 | `#ifndef _DEFAULT_SOURCE` |
|    - |   13 | `#define _DEFAULT_SOURCE 1` |
|    - |   14 | `#endif` |
|    - |   15 | `#include "ph7int.h"` |
|    - |   16 | `/*` |
|    - |   17 | ` * Section:` |
|    - |   18 | ` *    php's posix extension: the process, user, group and descriptor calls a` |
|    - |   19 | ` *    command-line program asks the system about itself with.` |
|    - |   20 | ` * Status:` |
|    - |   21 | ` *    Stable.` |
|    - |   22 | ` *` |
|    - |   23 | ` * php's ext/posix is a thin shell over the C library's POSIX.1 surface, so` |
|    - |   24 | ` * every answer here is the platform's and the only thing to reproduce is the` |
|    - |   25 | ` * SHAPE php gives it. That shape is four rules and a handful of exceptions:` |
|    - |   26 | ` *` |
|    - |   27 | ` *   The EXTENSION IS NOT THERE ON WINDOWS. php builds no ext/posix for it, so` |
|    - |   28 | `` *   `function_exists('posix_kill')` is false and `extension_loaded('posix')` is`` |
|    - |   29 | ` *   false on a Windows php -- which is exactly what a program that guards its` |
|    - |   30 | ` *   use of them expects to find. This whole translation unit is empty there,` |
|    - |   31 | ` *   and the constants are not defined either.` |
|    - |   32 | ` *` |
|    - |   33 | `` *   FAILURE IS `false` (or a negative number) AND AN ERRNO, never a warning.`` |
|    - |   34 | `` *   The errno is REMEMBERED: `posix_get_last_error()` (and its alias`` |
|    - |   35 | `` *   `posix_errno()`) answers what the last call that stored one saw, and`` |
|    - |   36 | ` *   nothing clears it -- a later SUCCESS leaves it standing. Which calls store` |
|    - |   37 | `` *   one is per-function and had to be measured: `posix_isatty()` stores errno`` |
|    - |   38 | ` *   whenever it answers FALSE (so a pipe leaves ENOTTY behind, and that is what` |
|    - |   39 | `` *   php reports); the four `getpw*`/`getgr*` lookups store errno`` |
|    - |   40 | ` *   UNCONDITIONALLY, so a name that simply is not there stores 0; and` |
|    - |   41 | `` *   `posix_sysconf()`, `posix_times()` and `posix_uname()` do not store on the`` |
|    - |   42 | ` *   paths that answer.` |
|    - |   43 | ` *` |
|    - |   44 | `` *   THE ARRAY KEY ORDER IS php's, not the struct's. `posix_uname()` is`` |
|    - |   45 | ` *   sysname, nodename, release, version, machine and -- where the platform's` |
|    - |   46 | `` *   `struct utsname` carries one -- domainname. `posix_times()` is ticks,`` |
|    - |   47 | `` *   utime, stime, cutime, cstime. `posix_getpwnam()` is name, passwd, uid,`` |
|    - |   48 | `` *   gid, gecos, dir, shell, and `posix_getgrnam()` is name, passwd, MEMBERS,`` |
|    - |   49 | `` *   gid -- the members list third, not last. `posix_getrlimit()` with no`` |
|    - |   50 | `` *   argument answers twenty keys, `soft X` and `hard X` for ten resources in`` |
|    - |   51 | `` *   php's own order, and an INFINITE limit is the STRING `unlimited` rather`` |
|    - |   52 | ` *   than a number.` |
|    - |   53 | ` *` |
|    - |   54 | ` *   A DESCRIPTOR ARGUMENT is an int or a php STREAM. The three doors that take` |
|    - |   55 | `` *   one (`posix_isatty`, `posix_ttyname`, `posix_fpathconf`) accept either, and`` |
|    - |   56 | ` *   a stream with no descriptor behind it -- a memory buffer, a data:// payload` |
|    - |   57 | `` *   -- is php's `Could not use stream of type '%s'` warning with the stream's`` |
|    - |   58 | `` *   own label, the same one `stream_get_meta_data()` reports. An int outside`` |
|    - |   59 | `` *   0..2147483647 is `must be between 0 and 2147483647`, and anything that is`` |
|    - |   60 | `` *   neither is `must be of type int\|resource, %s given`. All three are`` |
|    - |   61 | ` *   E_WARNINGs answering false, not TypeErrors.` |
|    - |   62 | ` *` |
|    - |   63 | ` * Two of php's own quirks are reproduced rather than corrected, because a` |
|    - |   64 | ` * program can see both:` |
|    - |   65 | ` *` |
|    - |   66 | `` *   `posix_mknod()` screens its major number with `(mode & S_IFCHR) \|\| (mode &`` |
|    - |   67 | `` *   S_IFBLK)`, which is a BIT test rather than a type test -- and S_IFSOCK`` |
|    - |   68 | ` *   (0140000) shares a bit with S_IFBLK (0060000), so asking for a socket node` |
|    - |   69 | ` *   without a major number raises the ValueError that names the two other` |
|    - |   70 | `` *   modes. `posix_setsid()` answers the raw -1 rather than false when the`` |
|    - |   71 | ` *   caller is already a process-group leader.` |
|    - |   72 | ` */` |
|    - |   73 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |   74 | `#ifndef __WINNT__` |
|    - |   75 |  |
|    - |   76 | `#include <errno.h>` |
|    - |   77 | `#include <stdio.h>        /* ctermid, L_ctermid */` |
|    - |   78 | `#include <string.h>       /* strerror */` |
|    - |   79 | `#ifdef __linux__` |
|    - |   80 | `#include <sys/sysmacros.h>/* makedev; the BSDs and macOS have it in sys/types.h */` |
|    - |   81 | `#endif` |
|    - |   82 | `#include <unistd.h>` |
|    - |   83 | `#include <signal.h>` |
|    - |   84 | `#include <pwd.h>` |
|    - |   85 | `#include <grp.h>` |
|    - |   86 | `#include <sys/types.h>` |
|    - |   87 | `#include <sys/stat.h>` |
|    - |   88 | `#include <sys/times.h>` |
|    - |   89 | `#include <sys/resource.h>` |
|    - |   90 | `#include <sys/utsname.h>` |
|    - |   91 | `#include <limits.h>` |
|    - |   92 |  |
|    - |   93 | `/* php's own MAXPATHLEN for getcwd()/ctermid(). */` |
|    - |   94 | `#ifndef PH7_POSIX_PATHBUF` |
|    - |   95 | `#ifdef PATH_MAX` |
|    - |   96 | `#define PH7_POSIX_PATHBUF (PATH_MAX + 1)` |
|    - |   97 | `#else` |
|    - |   98 | `#define PH7_POSIX_PATHBUF 4096` |
|    - |   99 | `#endif` |
|    - |  100 | `#endif` |
|    - |  101 |  |
|    - |  102 | `/* --- The remembered errno ---------------------------------------------- */` |
|    - |  103 |  |
|    - |  104 | `/*` |
|    - |  105 | `` * Answer false and remember WHY: php keeps `POSIX_G(last_error)` for the whole`` |
|    - |  106 | ` * module, and this keeps it per VM -- the same lifetime for a program, and two` |
|    - |  107 | ` * embedded VMs stay apart. Which calls store one is per-function and measured;` |
|    - |  108 | ` * the four database lookups store on the NOT-FOUND answer, where errno is 0, so` |
|    - |  109 | ` * a name that is not there OVERWRITES an earlier failure with a zero.` |
|    - |  110 | ` */` |
|   31 |  111 | `static int PxFail(ph7_context *pCtx)` |
|    - |  112 | `{` |
|   31 |  113 | `	pCtx->pVm->iPosixErr = errno;` |
|   31 |  114 | `	ph7_result_bool(pCtx,0);` |
|   31 |  115 | `	return PH7_OK;` |
|    - |  116 | `}` |
|    - |  117 |  |
|    - |  118 | `/* --- Screens ------------------------------------------------------------ */` |
|    - |  119 |  |
|    - |  120 | `/*` |
|    - |  121 | ` * A descriptor argument: a php stream to take one from, or anything php's WEAK` |
|    - |  122 | ` * int parse accepts. php's own helper is that parse, which is why a numeric` |
|    - |  123 | ` * STRING is taken in silence and a non-numeric one is a warning; the two` |
|    - |  124 | ` * refusals below are PHL's engine-wide scope policy where php only deprecates,` |
|    - |  125 | ` * and they are the same TypeError every other int parameter answers with.` |
|    - |  126 | ` *` |
|    - |  127 | ` * Answers the descriptor, or -1 after raising the diagnostic. *pbTyped says` |
|    - |  128 | ` * whether the argument was one the parse ACCEPTED: posix_ttyname() converts a` |
|    - |  129 | `` * refused one anyway (php's `zval_get_long` after the warning, which is where`` |
|    - |  130 | ` * an object's second diagnostic comes from) and posix_isatty() does not.` |
|    - |  131 | ` * bThrow is posix_fpathconf(), whose refusal is a TypeError rather than a` |
|    - |  132 | ` * warning -- php screens that one at the parameter and the other two inside` |
|    - |  133 | ` * the body, and the difference shows.` |
|    - |  134 | ` */` |
|   16 |  135 | `static int PxFd(ph7_context *pCtx,ph7_value *pArg,int *pbTyped,int bThrow)` |
|    - |  136 | `{` |
|    - |  137 | `	char zGiven[64];` |
|   16 |  138 | `	sxi64 iFd = 0;` |
|   16 |  139 | `	if( pbTyped ){` |
|    2 |  140 | `		*pbTyped = 1;` |
|  ! 0 |  141 | `	}` |
|   16 |  142 | `	if( ph7_value_is_resource(pArg) ){` |
|    7 |  143 | `		io_private *pDev = (io_private *)ph7_value_to_resource(pArg);` |
|    - |  144 | `		int fd;` |
|    7 |  145 | `		if( IO_PRIVATE_INVALID(pDev) ){` |
|    1 |  146 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  147 | `				"%s(): supplied resource is not a valid stream resource",` |
|  ! 0 |  148 | `				ph7_function_name(pCtx));` |
|    1 |  149 | `			if( pbTyped ){` |
|  ! 0 |  150 | `				*pbTyped = 0;` |
|  ! 0 |  151 | `			}` |
|    1 |  152 | `			return -1;` |
|    - |  153 | `		}` |
|    6 |  154 | `		fd = PH7_StreamPosixFd(pDev);` |
|    6 |  155 | `		if( fd < 0 ){` |
|    - |  156 | `			/* php's stream layer could hand back no descriptor for it. */` |
|    2 |  157 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|  ! 0 |  158 | `				"Could not use stream of type '%s'",PH7_StreamTypeLabel(pDev));` |
|    2 |  159 | `			if( pbTyped ){` |
|    1 |  160 | `				*pbTyped = 0;` |
|  ! 0 |  161 | `			}` |
|    2 |  162 | `			return -1;` |
|    - |  163 | `		}` |
|    4 |  164 | `		return fd;` |
|    - |  165 | `	}` |
|    9 |  166 | `	if( ph7_value_is_null(pArg) \|\| ph7_value_is_float(pArg) ){` |
|    - |  167 | `		/* null and a LOSSY float are what php merely deprecates here; PHL` |
|    - |  168 | `		 * refuses both, engine-wide, through the shared int screen. */` |
|  ! 0 |  169 | `		if( PH7_IntArgResolve(pCtx,pArg,ph7_function_name(pCtx),1,` |
|  ! 0 |  170 | `			"$file_descriptor","int",&iFd) != PH7_OK ){` |
|  ! 0 |  171 | `			if( pbTyped ){` |
|  ! 0 |  172 | `				*pbTyped = 0;` |
|  ! 0 |  173 | `			}` |
|  ! 0 |  174 | `			return -1;` |
|    - |  175 | `		}` |
|    9 |  176 | `	}else if( ph7_value_is_int(pArg) \|\| ph7_value_is_bool(pArg) ){` |
|    4 |  177 | `		iFd = ph7_value_to_int64(pArg);` |
|    5 |  178 | `	}else if( ph7_value_is_string(pArg) && PH7_MemObjStringIsNumeric(pArg) ){` |
|    1 |  179 | `		iFd = ph7_value_to_int64(pArg);` |
|  ! 0 |  180 | `	}else{` |
|    4 |  181 | `		if( bThrow ){` |
|    1 |  182 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  183 | `				"%s(): Argument #1 ($file_descriptor) must be of type int\|resource, %s given",` |
|  ! 0 |  184 | `				ph7_function_name(pCtx),VmValueGivenName(pArg,zGiven,sizeof(zGiven)));` |
|  ! 0 |  185 | `		}else{` |
|    3 |  186 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - |  187 | `				"Argument #1 ($file_descriptor) must be of type int\|resource, %s given",` |
|  ! 0 |  188 | `				VmValueGivenName(pArg,zGiven,sizeof(zGiven)));` |
|    - |  189 | `		}` |
|    4 |  190 | `		if( pbTyped ){` |
|  ! 0 |  191 | `			*pbTyped = 0;` |
|  ! 0 |  192 | `		}` |
|    4 |  193 | `		return -1;` |
|    - |  194 | `	}` |
|    5 |  195 | `	if( iFd < 0 \|\| iFd > 2147483647 ){` |
|    1 |  196 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - |  197 | `			"Argument #1 ($file_descriptor) must be between 0 and 2147483647");` |
|    1 |  198 | `		if( pbTyped ){` |
|  ! 0 |  199 | `			*pbTyped = 0;` |
|  ! 0 |  200 | `		}` |
|    1 |  201 | `		return -1;` |
|    - |  202 | `	}` |
|    4 |  203 | `	return (int)iFd;` |
|  ! 0 |  204 | `}` |
|    - |  205 |  |
|    - |  206 | `/* --- The process identity ----------------------------------------------- */` |
|    - |  207 |  |
|    - |  208 | `/* int posix_getpid() */` |
|    6 |  209 | `PH7_PRIVATE int PH7_builtin_posix_getpid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  210 | `{` |
|    1 |  211 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    6 |  212 | `	ph7_result_int64(pCtx,(sxi64)getpid());` |
|    6 |  213 | `	return PH7_OK;` |
|    - |  214 | `}` |
|    - |  215 | `/* int posix_getppid() */` |
|    5 |  216 | `PH7_PRIVATE int PH7_builtin_posix_getppid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  217 | `{` |
|    2 |  218 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    5 |  219 | `	ph7_result_int64(pCtx,(sxi64)getppid());` |
|    5 |  220 | `	return PH7_OK;` |
|    - |  221 | `}` |
|    - |  222 | `/* int posix_getuid() */` |
|    6 |  223 | `PH7_PRIVATE int PH7_builtin_posix_getuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  224 | `{` |
|  ! 0 |  225 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    6 |  226 | `	ph7_result_int64(pCtx,(sxi64)getuid());` |
|    6 |  227 | `	return PH7_OK;` |
|    - |  228 | `}` |
|    - |  229 | `/* int posix_geteuid() */` |
|    1 |  230 | `PH7_PRIVATE int PH7_builtin_posix_geteuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  231 | `{` |
|  ! 0 |  232 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    1 |  233 | `	ph7_result_int64(pCtx,(sxi64)geteuid());` |
|    1 |  234 | `	return PH7_OK;` |
|    - |  235 | `}` |
|    - |  236 | `/* int posix_getgid() */` |
|    5 |  237 | `PH7_PRIVATE int PH7_builtin_posix_getgid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  238 | `{` |
|  ! 0 |  239 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    5 |  240 | `	ph7_result_int64(pCtx,(sxi64)getgid());` |
|    5 |  241 | `	return PH7_OK;` |
|    - |  242 | `}` |
|    - |  243 | `/* int posix_getegid() */` |
|    1 |  244 | `PH7_PRIVATE int PH7_builtin_posix_getegid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  245 | `{` |
|  ! 0 |  246 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    1 |  247 | `	ph7_result_int64(pCtx,(sxi64)getegid());` |
|    1 |  248 | `	return PH7_OK;` |
|    - |  249 | `}` |
|    - |  250 | `/* bool posix_setuid(int $user_id) */` |
|  ! 0 |  251 | `PH7_PRIVATE int PH7_builtin_posix_setuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  252 | `{` |
|  ! 0 |  253 | `	if( nArg < 1 \|\| setuid((uid_t)ph7_value_to_int64(apArg[0])) != 0 ){` |
|  ! 0 |  254 | `		return PxFail(pCtx);` |
|    - |  255 | `	}` |
|  ! 0 |  256 | `	ph7_result_bool(pCtx,1);` |
|  ! 0 |  257 | `	return PH7_OK;` |
|  ! 0 |  258 | `}` |
|    - |  259 | `/* bool posix_seteuid(int $user_id) */` |
|  ! 0 |  260 | `PH7_PRIVATE int PH7_builtin_posix_seteuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  261 | `{` |
|  ! 0 |  262 | `	if( nArg < 1 \|\| seteuid((uid_t)ph7_value_to_int64(apArg[0])) != 0 ){` |
|  ! 0 |  263 | `		return PxFail(pCtx);` |
|    - |  264 | `	}` |
|  ! 0 |  265 | `	ph7_result_bool(pCtx,1);` |
|  ! 0 |  266 | `	return PH7_OK;` |
|  ! 0 |  267 | `}` |
|    - |  268 | `/* bool posix_setgid(int $group_id) */` |
|  ! 0 |  269 | `PH7_PRIVATE int PH7_builtin_posix_setgid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  270 | `{` |
|  ! 0 |  271 | `	if( nArg < 1 \|\| setgid((gid_t)ph7_value_to_int64(apArg[0])) != 0 ){` |
|  ! 0 |  272 | `		return PxFail(pCtx);` |
|    - |  273 | `	}` |
|  ! 0 |  274 | `	ph7_result_bool(pCtx,1);` |
|  ! 0 |  275 | `	return PH7_OK;` |
|  ! 0 |  276 | `}` |
|    - |  277 | `/* bool posix_setegid(int $group_id) */` |
|  ! 0 |  278 | `PH7_PRIVATE int PH7_builtin_posix_setegid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  279 | `{` |
|  ! 0 |  280 | `	if( nArg < 1 \|\| setegid((gid_t)ph7_value_to_int64(apArg[0])) != 0 ){` |
|  ! 0 |  281 | `		return PxFail(pCtx);` |
|    - |  282 | `	}` |
|  ! 0 |  283 | `	ph7_result_bool(pCtx,1);` |
|  ! 0 |  284 | `	return PH7_OK;` |
|  ! 0 |  285 | `}` |
|    - |  286 | `/*` |
|    - |  287 | ` * array\|false posix_getgroups()` |
|    - |  288 | ` *  The supplementary groups, asked for twice: once for the count and once for` |
|    - |  289 | ` *  the list, which is how the interface is meant to be used.` |
|    - |  290 | ` */` |
|    1 |  291 | `PH7_PRIVATE int PH7_builtin_posix_getgroups(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  292 | `{` |
|    - |  293 | `	gid_t *aGid;` |
|    - |  294 | `	ph7_value *pArray,*pVal;` |
|    - |  295 | `	int n,i;` |
|  ! 0 |  296 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    - |  297 | `	/* Asked for twice, as the interface is meant to be: once for the count and` |
|    - |  298 | `	 * once for the list. A fixed array is not an option -- NGROUPS_MAX is 65536` |
|    - |  299 | `	 * on Linux, which is a quarter of a megabyte of stack for a list that is` |
|    - |  300 | `	 * usually under ten entries long. */` |
|    1 |  301 | `	n = getgroups(0,0);` |
|    1 |  302 | `	if( n < 0 ){` |
|  ! 0 |  303 | `		return PxFail(pCtx);` |
|    - |  304 | `	}` |
|    1 |  305 | `	pArray = ph7_context_new_array(pCtx);` |
|    1 |  306 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    1 |  307 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|  ! 0 |  308 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  309 | `		return PH7_OK;` |
|    - |  310 | `	}` |
|    1 |  311 | `	if( n > 0 ){` |
|    1 |  312 | `		aGid = (gid_t *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)n * (sxu32)sizeof(gid_t));` |
|    1 |  313 | `		if( aGid == 0 ){` |
|  ! 0 |  314 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 |  315 | `			return PH7_OK;` |
|    - |  316 | `		}` |
|    1 |  317 | `		n = getgroups(n,aGid);` |
|    1 |  318 | `		if( n < 0 ){` |
|  ! 0 |  319 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,aGid);` |
|  ! 0 |  320 | `			return PxFail(pCtx);` |
|    - |  321 | `		}` |
|    6 |  322 | `		for( i = 0 ; i < n ; ++i ){` |
|    5 |  323 | `			ph7_value_int64(pVal,(sxi64)aGid[i]);` |
|    5 |  324 | `			ph7_array_add_elem(pArray,0,pVal);` |
|  ! 0 |  325 | `		}` |
|    1 |  326 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,aGid);` |
|  ! 0 |  327 | `	}` |
|    1 |  328 | `	ph7_result_value(pCtx,pArray);` |
|    1 |  329 | `	return PH7_OK;` |
|  ! 0 |  330 | `}` |
|    - |  331 | `/* string\|false posix_getlogin() */` |
|    2 |  332 | `PH7_PRIVATE int PH7_builtin_posix_getlogin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  333 | `{` |
|    - |  334 | `	char *zName;` |
|  ! 0 |  335 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    2 |  336 | `	errno = 0;` |
|    2 |  337 | `	zName = getlogin();` |
|    2 |  338 | `	if( zName == 0 ){` |
|    2 |  339 | `		return PxFail(pCtx);` |
|    - |  340 | `	}` |
|  ! 0 |  341 | `	ph7_result_string(pCtx,zName,-1);` |
|  ! 0 |  342 | `	return PH7_OK;` |
|  ! 0 |  343 | `}` |
|    - |  344 |  |
|    - |  345 | `/* --- Process groups and sessions ---------------------------------------- */` |
|    - |  346 |  |
|    - |  347 | `/* int posix_getpgrp() */` |
|    1 |  348 | `PH7_PRIVATE int PH7_builtin_posix_getpgrp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  349 | `{` |
|  ! 0 |  350 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    1 |  351 | `	ph7_result_int64(pCtx,(sxi64)getpgrp());` |
|    1 |  352 | `	return PH7_OK;` |
|    - |  353 | `}` |
|    - |  354 | `/*` |
|    - |  355 | ` * int posix_setsid()` |
|    - |  356 | ` *  php hands back what setsid() answered, so a caller that is ALREADY a process` |
|    - |  357 | ` *  group leader reads the raw -1 rather than false.` |
|    - |  358 | ` */` |
|  ! 0 |  359 | `PH7_PRIVATE int PH7_builtin_posix_setsid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  360 | `{` |
|  ! 0 |  361 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|  ! 0 |  362 | `	ph7_result_int64(pCtx,(sxi64)setsid());` |
|  ! 0 |  363 | `	return PH7_OK;` |
|    - |  364 | `}` |
|    - |  365 | `/* bool posix_setpgid(int $process_id, int $process_group_id) */` |
|  ! 0 |  366 | `PH7_PRIVATE int PH7_builtin_posix_setpgid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  367 | `{` |
|  ! 0 |  368 | `	if( nArg < 2` |
|  ! 0 |  369 | `	 \|\| setpgid((pid_t)ph7_value_to_int64(apArg[0]),(pid_t)ph7_value_to_int64(apArg[1])) != 0 ){` |
|  ! 0 |  370 | `		return PxFail(pCtx);` |
|    - |  371 | `	}` |
|  ! 0 |  372 | `	ph7_result_bool(pCtx,1);` |
|  ! 0 |  373 | `	return PH7_OK;` |
|  ! 0 |  374 | `}` |
|    - |  375 | `/* int\|false posix_getpgid(int $process_id) */` |
|    2 |  376 | `PH7_PRIVATE int PH7_builtin_posix_getpgid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  377 | `{` |
|    - |  378 | `	pid_t iRes;` |
|    2 |  379 | `	if( nArg < 1 ){` |
|  ! 0 |  380 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  381 | `		return PH7_OK;` |
|    - |  382 | `	}` |
|    2 |  383 | `	iRes = getpgid((pid_t)ph7_value_to_int64(apArg[0]));` |
|    2 |  384 | `	if( iRes < 0 ){` |
|    1 |  385 | `		return PxFail(pCtx);` |
|    - |  386 | `	}` |
|    1 |  387 | `	ph7_result_int64(pCtx,(sxi64)iRes);` |
|    1 |  388 | `	return PH7_OK;` |
|  ! 0 |  389 | `}` |
|    - |  390 | `/* int\|false posix_getsid(int $process_id) */` |
|    2 |  391 | `PH7_PRIVATE int PH7_builtin_posix_getsid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  392 | `{` |
|    - |  393 | `	pid_t iRes;` |
|    2 |  394 | `	if( nArg < 1 ){` |
|  ! 0 |  395 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  396 | `		return PH7_OK;` |
|    - |  397 | `	}` |
|    2 |  398 | `	iRes = getsid((pid_t)ph7_value_to_int64(apArg[0]));` |
|    2 |  399 | `	if( iRes < 0 ){` |
|    1 |  400 | `		return PxFail(pCtx);` |
|    - |  401 | `	}` |
|    1 |  402 | `	ph7_result_int64(pCtx,(sxi64)iRes);` |
|    1 |  403 | `	return PH7_OK;` |
|  ! 0 |  404 | `}` |
|    - |  405 | `/* bool posix_kill(int $process_id, int $signal) */` |
|   34 |  406 | `PH7_PRIVATE int PH7_builtin_posix_kill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  407 | `{` |
|   34 |  408 | `	if( nArg < 2` |
|   34 |  409 | `	 \|\| kill((pid_t)ph7_value_to_int64(apArg[0]),(int)ph7_value_to_int64(apArg[1])) != 0 ){` |
|    9 |  410 | `		return PxFail(pCtx);` |
|    - |  411 | `	}` |
|   25 |  412 | `	ph7_result_bool(pCtx,1);` |
|   25 |  413 | `	return PH7_OK;` |
|    4 |  414 | `}` |
|    - |  415 |  |
|    - |  416 | `/* --- What the system says about itself ---------------------------------- */` |
|    - |  417 |  |
|    - |  418 | `/* array\|false posix_uname() */` |
|    2 |  419 | `PH7_PRIVATE int PH7_builtin_posix_uname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  420 | `{` |
|    - |  421 | `	struct utsname sName;` |
|    - |  422 | `	ph7_value *pArray,*pVal;` |
|  ! 0 |  423 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    2 |  424 | `	if( uname(&sName) < 0 ){` |
|  ! 0 |  425 | `		return PxFail(pCtx);` |
|    - |  426 | `	}` |
|    2 |  427 | `	pArray = ph7_context_new_array(pCtx);` |
|    2 |  428 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    2 |  429 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|  ! 0 |  430 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  431 | `		return PH7_OK;` |
|    - |  432 | `	}` |
|    2 |  433 | `	ph7_value_string(pVal,sName.sysname,-1);` |
|    2 |  434 | `	ph7_array_add_strkey_elem(pArray,"sysname",pVal);` |
|    2 |  435 | `	ph7_value_reset_string_cursor(pVal);` |
|    2 |  436 | `	ph7_value_string(pVal,sName.nodename,-1);` |
|    2 |  437 | `	ph7_array_add_strkey_elem(pArray,"nodename",pVal);` |
|    2 |  438 | `	ph7_value_reset_string_cursor(pVal);` |
|    2 |  439 | `	ph7_value_string(pVal,sName.release,-1);` |
|    2 |  440 | `	ph7_array_add_strkey_elem(pArray,"release",pVal);` |
|    2 |  441 | `	ph7_value_reset_string_cursor(pVal);` |
|    2 |  442 | `	ph7_value_string(pVal,sName.version,-1);` |
|    2 |  443 | `	ph7_array_add_strkey_elem(pArray,"version",pVal);` |
|    2 |  444 | `	ph7_value_reset_string_cursor(pVal);` |
|    2 |  445 | `	ph7_value_string(pVal,sName.machine,-1);` |
|    2 |  446 | `	ph7_array_add_strkey_elem(pArray,"machine",pVal);` |
|    - |  447 | `#ifdef __linux__` |
|    2 |  448 | `	ph7_value_reset_string_cursor(pVal);` |
|    2 |  449 | `	ph7_value_string(pVal,sName.domainname,-1);` |
|    2 |  450 | `	ph7_array_add_strkey_elem(pArray,"domainname",pVal);` |
|    - |  451 | `#endif` |
|    2 |  452 | `	ph7_result_value(pCtx,pArray);` |
|    2 |  453 | `	return PH7_OK;` |
|  ! 0 |  454 | `}` |
|    - |  455 | `/* array\|false posix_times() */` |
|    3 |  456 | `PH7_PRIVATE int PH7_builtin_posix_times(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  457 | `{` |
|    - |  458 | `	struct tms sTms;` |
|    - |  459 | `	clock_t iTicks;` |
|    - |  460 | `	ph7_value *pArray,*pVal;` |
|  ! 0 |  461 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    3 |  462 | `	iTicks = times(&sTms);` |
|    3 |  463 | `	if( iTicks == (clock_t)-1 ){` |
|  ! 0 |  464 | `		return PxFail(pCtx);` |
|    - |  465 | `	}` |
|    3 |  466 | `	pArray = ph7_context_new_array(pCtx);` |
|    3 |  467 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    3 |  468 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|  ! 0 |  469 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  470 | `		return PH7_OK;` |
|    - |  471 | `	}` |
|    3 |  472 | `	ph7_value_int64(pVal,(sxi64)iTicks);` |
|    3 |  473 | `	ph7_array_add_strkey_elem(pArray,"ticks",pVal);` |
|    3 |  474 | `	ph7_value_int64(pVal,(sxi64)sTms.tms_utime);` |
|    3 |  475 | `	ph7_array_add_strkey_elem(pArray,"utime",pVal);` |
|    3 |  476 | `	ph7_value_int64(pVal,(sxi64)sTms.tms_stime);` |
|    3 |  477 | `	ph7_array_add_strkey_elem(pArray,"stime",pVal);` |
|    3 |  478 | `	ph7_value_int64(pVal,(sxi64)sTms.tms_cutime);` |
|    3 |  479 | `	ph7_array_add_strkey_elem(pArray,"cutime",pVal);` |
|    3 |  480 | `	ph7_value_int64(pVal,(sxi64)sTms.tms_cstime);` |
|    3 |  481 | `	ph7_array_add_strkey_elem(pArray,"cstime",pVal);` |
|    3 |  482 | `	ph7_result_value(pCtx,pArray);` |
|    3 |  483 | `	return PH7_OK;` |
|  ! 0 |  484 | `}` |
|    - |  485 | `/* string\|false posix_ctermid() */` |
|    1 |  486 | `PH7_PRIVATE int PH7_builtin_posix_ctermid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  487 | `{` |
|    - |  488 | `	char zBuf[L_ctermid];` |
|  ! 0 |  489 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    1 |  490 | `	if( ctermid(zBuf) == 0 ){` |
|  ! 0 |  491 | `		return PxFail(pCtx);` |
|    - |  492 | `	}` |
|    1 |  493 | `	ph7_result_string(pCtx,zBuf,-1);` |
|    1 |  494 | `	return PH7_OK;` |
|  ! 0 |  495 | `}` |
|    - |  496 | `/* string\|false posix_getcwd() */` |
|    2 |  497 | `PH7_PRIVATE int PH7_builtin_posix_getcwd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  498 | `{` |
|    - |  499 | `	char zBuf[PH7_POSIX_PATHBUF];` |
|  ! 0 |  500 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    2 |  501 | `	if( getcwd(zBuf,sizeof(zBuf)) == 0 ){` |
|  ! 0 |  502 | `		return PxFail(pCtx);` |
|    - |  503 | `	}` |
|    2 |  504 | `	ph7_result_string(pCtx,zBuf,-1);` |
|    2 |  505 | `	return PH7_OK;` |
|  ! 0 |  506 | `}` |
|    - |  507 |  |
|    - |  508 | `/* --- Descriptors --------------------------------------------------------- */` |
|    - |  509 |  |
|    - |  510 | `/*` |
|    - |  511 | ` * bool posix_isatty(int\|resource $file_descriptor)` |
|    - |  512 | ` *  php's terminal question, and the one every CLI tool asks before it colours` |
|    - |  513 | `` *  its output: `sebastian/environment`'s isInteractive() is exactly this call,`` |
|    - |  514 | ` *  so PHPUnit's whole interactive surface hangs off it.` |
|    - |  515 | ` */` |
|   11 |  516 | `PH7_PRIVATE int PH7_builtin_posix_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  517 | `{` |
|    - |  518 | `	int fd;` |
|   11 |  519 | `	if( nArg < 1 ){` |
|  ! 0 |  520 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  521 | `		return PH7_OK;` |
|    - |  522 | `	}` |
|   11 |  523 | `	fd = PxFd(pCtx,apArg[0],0,0);` |
|   11 |  524 | `	if( fd < 0 ){` |
|    6 |  525 | `		ph7_result_bool(pCtx,0);` |
|    6 |  526 | `		return PH7_OK;` |
|    - |  527 | `	}` |
|    5 |  528 | `	if( isatty(fd) ){` |
|  ! 0 |  529 | `		ph7_result_bool(pCtx,1);` |
|  ! 0 |  530 | `		return PH7_OK;` |
|    - |  531 | `	}` |
|    - |  532 | `	/* php stores the errno of the FALSE answer too, which is why a pipe leaves` |
|    - |  533 | `	 * ENOTTY where a closed descriptor leaves EBADF. */` |
|    5 |  534 | `	return PxFail(pCtx);` |
|  ! 0 |  535 | `}` |
|    - |  536 | `/* string\|false posix_ttyname(int\|resource $file_descriptor) */` |
|    2 |  537 | `PH7_PRIVATE int PH7_builtin_posix_ttyname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  538 | `{` |
|    - |  539 | `	const char *zName;` |
|    2 |  540 | `	int fd,bTyped = 1;` |
|    2 |  541 | `	if( nArg < 1 ){` |
|  ! 0 |  542 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  543 | `		return PH7_OK;` |
|    - |  544 | `	}` |
|    2 |  545 | `	fd = PxFd(pCtx,apArg[0],&bTyped,0);` |
|    2 |  546 | `	if( fd < 0 ){` |
|    - |  547 | `		/* php warns and then CONVERTS the argument anyway, which is where a` |
|    - |  548 | `		 * second diagnostic comes from for an object; a resource it could not` |
|    - |  549 | `		 * take a descriptor from, and PHL's own refusals, stop here. */` |
|    1 |  550 | `		if( bTyped \|\| ph7_value_is_resource(apArg[0])` |
|  ! 0 |  551 | `		 \|\| ph7_value_is_null(apArg[0]) \|\| ph7_value_is_float(apArg[0]) ){` |
|    1 |  552 | `			ph7_result_bool(pCtx,0);` |
|    1 |  553 | `			return PH7_OK;` |
|    - |  554 | `		}` |
|  ! 0 |  555 | `		fd = (int)ph7_value_to_int64(apArg[0]);` |
|  ! 0 |  556 | `		if( fd < 0 ){` |
|  ! 0 |  557 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 |  558 | `			return PH7_OK;` |
|    - |  559 | `		}` |
|  ! 0 |  560 | `	}` |
|    1 |  561 | `	zName = ttyname(fd);` |
|    1 |  562 | `	if( zName == 0 ){` |
|    1 |  563 | `		return PxFail(pCtx);` |
|    - |  564 | `	}` |
|  ! 0 |  565 | `	ph7_result_string(pCtx,zName,-1);` |
|  ! 0 |  566 | `	return PH7_OK;` |
|  ! 0 |  567 | `}` |
|    - |  568 |  |
|    - |  569 | `/* --- Files, nodes and access -------------------------------------------- */` |
|    - |  570 |  |
|    - |  571 | `/* bool posix_mkfifo(string $filename, int $permissions) */` |
|    1 |  572 | `PH7_PRIVATE int PH7_builtin_posix_mkfifo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  573 | `{` |
|    - |  574 | `	const char *zPath;` |
|    1 |  575 | `	int nPath = 0;` |
|    1 |  576 | `	if( nArg < 2 ){` |
|  ! 0 |  577 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  578 | `		return PH7_OK;` |
|    - |  579 | `	}` |
|    1 |  580 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|  ! 0 |  581 | `	SXUNUSED(nPath);` |
|    1 |  582 | `	if( mkfifo(zPath,(mode_t)ph7_value_to_int64(apArg[1])) != 0 ){` |
|    1 |  583 | `		return PxFail(pCtx);` |
|    - |  584 | `	}` |
|  ! 0 |  585 | `	ph7_result_bool(pCtx,1);` |
|  ! 0 |  586 | `	return PH7_OK;` |
|  ! 0 |  587 | `}` |
|    - |  588 | `/*` |
|    - |  589 | ` * bool posix_mknod(string $filename, int $flags, int $major = 0, int $minor = 0)` |
|    - |  590 | ` *  The major-number screen is php's, and so is its ODDITY: the test is` |
|    - |  591 | `` *  `(flags & S_IFCHR) \|\| (flags & S_IFBLK)`, a bit test rather than a type`` |
|    - |  592 | ` *  test, and S_IFSOCK shares a bit with S_IFBLK -- so asking for a socket node` |
|    - |  593 | ` *  with no major number raises the ValueError that names the two other modes.` |
|    - |  594 | ` */` |
|    4 |  595 | `PH7_PRIVATE int PH7_builtin_posix_mknod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  596 | `{` |
|    - |  597 | `	const char *zPath;` |
|    4 |  598 | `	int nPath = 0;` |
|    4 |  599 | `	sxi64 iFlags,iMajor = 0,iMinor = 0;` |
|    4 |  600 | `	dev_t iDev = 0;` |
|    4 |  601 | `	if( nArg < 2 ){` |
|  ! 0 |  602 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  603 | `		return PH7_OK;` |
|    - |  604 | `	}` |
|    4 |  605 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|  ! 0 |  606 | `	SXUNUSED(nPath);` |
|    4 |  607 | `	iFlags = ph7_value_to_int64(apArg[1]);` |
|    4 |  608 | `	if( nArg > 2 ){` |
|    1 |  609 | `		iMajor = ph7_value_to_int64(apArg[2]);` |
|  ! 0 |  610 | `	}` |
|    4 |  611 | `	if( nArg > 3 ){` |
|    1 |  612 | `		iMinor = ph7_value_to_int64(apArg[3]);` |
|  ! 0 |  613 | `	}` |
|    4 |  614 | `	if( (iFlags & S_IFCHR) \|\| (iFlags & S_IFBLK) ){` |
|    3 |  615 | `		if( iMajor == 0 ){` |
|    3 |  616 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  617 | `				"%s(): Argument #3 ($major) cannot be 0 for the POSIX_S_IFCHR and POSIX_S_IFBLK modes",` |
|  ! 0 |  618 | `				ph7_function_name(pCtx));` |
|    - |  619 | `		}` |
|  ! 0 |  620 | `		iDev = makedev((unsigned int)iMajor,(unsigned int)iMinor);` |
|  ! 0 |  621 | `	}` |
|    1 |  622 | `	if( mknod(zPath,(mode_t)iFlags,iDev) != 0 ){` |
|    1 |  623 | `		return PxFail(pCtx);` |
|    - |  624 | `	}` |
|  ! 0 |  625 | `	ph7_result_bool(pCtx,1);` |
|  ! 0 |  626 | `	return PH7_OK;` |
|  ! 0 |  627 | `}` |
|    - |  628 | `/* The shared body of posix_access() and posix_eaccess(). */` |
|    3 |  629 | `static int PxAccess(ph7_context *pCtx,int nArg,ph7_value **apArg,int bEffective)` |
|    - |  630 | `{` |
|    - |  631 | `	const char *zPath;` |
|    3 |  632 | `	int nPath = 0,iMode = 0,rc;` |
|    3 |  633 | `	if( nArg < 1 ){` |
|  ! 0 |  634 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  635 | `		return PH7_OK;` |
|    - |  636 | `	}` |
|    3 |  637 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|  ! 0 |  638 | `	SXUNUSED(nPath);` |
|    3 |  639 | `	if( nArg > 1 ){` |
|  ! 0 |  640 | `		iMode = (int)ph7_value_to_int64(apArg[1]);` |
|  ! 0 |  641 | `	}` |
|    3 |  642 | `	if( bEffective ){` |
|    - |  643 | `#if defined(__GLIBC__)` |
|  ! 0 |  644 | `		rc = eaccess(zPath,iMode);` |
|    - |  645 | `#else` |
|    - |  646 | `		/* Without the GNU/BSD spelling there is nothing to ask the EFFECTIVE` |
|    - |  647 | `		 * ids with, so the real ones answer -- which is the same thing for` |
|    - |  648 | `		 * every process that is not setuid. */` |
|  ! 0 |  649 | `		rc = access(zPath,iMode);` |
|    - |  650 | `#endif` |
|  ! 0 |  651 | `	}else{` |
|    3 |  652 | `		rc = access(zPath,iMode);` |
|    - |  653 | `	}` |
|    3 |  654 | `	if( rc != 0 ){` |
|    2 |  655 | `		return PxFail(pCtx);` |
|    - |  656 | `	}` |
|    1 |  657 | `	ph7_result_bool(pCtx,1);` |
|    1 |  658 | `	return PH7_OK;` |
|  ! 0 |  659 | `}` |
|    - |  660 | `/* bool posix_access(string $filename, int $flags = 0) */` |
|    3 |  661 | `PH7_PRIVATE int PH7_builtin_posix_access(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  662 | `{` |
|    3 |  663 | `	return PxAccess(pCtx,nArg,apArg,0);` |
|    - |  664 | `}` |
|    - |  665 | `/*` |
|    - |  666 | ` * bool posix_eaccess(string $filename, int $flags = 0)` |
|    - |  667 | ` *  The same question asked with the EFFECTIVE ids, which is what a setuid` |
|    - |  668 | ` *  program has to ask.` |
|    - |  669 | ` */` |
|  ! 0 |  670 | `PH7_PRIVATE int PH7_builtin_posix_eaccess(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  671 | `{` |
|  ! 0 |  672 | `	return PxAccess(pCtx,nArg,apArg,1);` |
|    - |  673 | `}` |
|    - |  674 |  |
|    - |  675 | `/* --- The user and group databases --------------------------------------- */` |
|    - |  676 |  |
|    - |  677 | `/* Fill php's seven passwd keys, in php's order. */` |
|    4 |  678 | `static int PxPasswdArray(ph7_context *pCtx,struct passwd *pPw)` |
|    - |  679 | `{` |
|    4 |  680 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|    4 |  681 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    4 |  682 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|  ! 0 |  683 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  684 | `		return PH7_OK;` |
|    - |  685 | `	}` |
|    4 |  686 | `	ph7_value_string(pVal,pPw->pw_name ? pPw->pw_name : "",-1);` |
|    4 |  687 | `	ph7_array_add_strkey_elem(pArray,"name",pVal);` |
|    4 |  688 | `	ph7_value_reset_string_cursor(pVal);` |
|    4 |  689 | `	ph7_value_string(pVal,pPw->pw_passwd ? pPw->pw_passwd : "",-1);` |
|    4 |  690 | `	ph7_array_add_strkey_elem(pArray,"passwd",pVal);` |
|    4 |  691 | `	ph7_value_int64(pVal,(sxi64)pPw->pw_uid);` |
|    4 |  692 | `	ph7_array_add_strkey_elem(pArray,"uid",pVal);` |
|    4 |  693 | `	ph7_value_int64(pVal,(sxi64)pPw->pw_gid);` |
|    4 |  694 | `	ph7_array_add_strkey_elem(pArray,"gid",pVal);` |
|    4 |  695 | `	ph7_value_reset_string_cursor(pVal);` |
|    4 |  696 | `	ph7_value_string(pVal,pPw->pw_gecos ? pPw->pw_gecos : "",-1);` |
|    4 |  697 | `	ph7_array_add_strkey_elem(pArray,"gecos",pVal);` |
|    4 |  698 | `	ph7_value_reset_string_cursor(pVal);` |
|    4 |  699 | `	ph7_value_string(pVal,pPw->pw_dir ? pPw->pw_dir : "",-1);` |
|    4 |  700 | `	ph7_array_add_strkey_elem(pArray,"dir",pVal);` |
|    4 |  701 | `	ph7_value_reset_string_cursor(pVal);` |
|    4 |  702 | `	ph7_value_string(pVal,pPw->pw_shell ? pPw->pw_shell : "",-1);` |
|    4 |  703 | `	ph7_array_add_strkey_elem(pArray,"shell",pVal);` |
|    4 |  704 | `	ph7_result_value(pCtx,pArray);` |
|    4 |  705 | `	return PH7_OK;` |
|  ! 0 |  706 | `}` |
|    - |  707 | ``/* Fill php's four group keys -- with `members` THIRD, which is php's order. */`` |
|    3 |  708 | `static int PxGroupArray(ph7_context *pCtx,struct group *pGr)` |
|    - |  709 | `{` |
|    3 |  710 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|    3 |  711 | `	ph7_value *pMembers = ph7_context_new_array(pCtx);` |
|    3 |  712 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    - |  713 | `	int i;` |
|    3 |  714 | `	if( pArray == 0 \|\| pMembers == 0 \|\| pVal == 0 ){` |
|  ! 0 |  715 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  716 | `		return PH7_OK;` |
|    - |  717 | `	}` |
|    3 |  718 | `	ph7_value_string(pVal,pGr->gr_name ? pGr->gr_name : "",-1);` |
|    3 |  719 | `	ph7_array_add_strkey_elem(pArray,"name",pVal);` |
|    3 |  720 | `	ph7_value_reset_string_cursor(pVal);` |
|    3 |  721 | `	ph7_value_string(pVal,pGr->gr_passwd ? pGr->gr_passwd : "",-1);` |
|    3 |  722 | `	ph7_array_add_strkey_elem(pArray,"passwd",pVal);` |
|    3 |  723 | `	for( i = 0 ; pGr->gr_mem && pGr->gr_mem[i] ; ++i ){` |
|  ! 0 |  724 | `		ph7_value_reset_string_cursor(pVal);` |
|  ! 0 |  725 | `		ph7_value_string(pVal,pGr->gr_mem[i],-1);` |
|  ! 0 |  726 | `		ph7_array_add_elem(pMembers,0,pVal);` |
|  ! 0 |  727 | `	}` |
|    3 |  728 | `	ph7_array_add_strkey_elem(pArray,"members",pMembers);` |
|    3 |  729 | `	ph7_value_reset_string_cursor(pVal);` |
|    3 |  730 | `	ph7_value_int64(pVal,(sxi64)pGr->gr_gid);` |
|    3 |  731 | `	ph7_array_add_strkey_elem(pArray,"gid",pVal);` |
|    3 |  732 | `	ph7_result_value(pCtx,pArray);` |
|    3 |  733 | `	return PH7_OK;` |
|  ! 0 |  734 | `}` |
|    - |  735 | `/* array\|false posix_getpwnam(string $username) */` |
|    3 |  736 | `PH7_PRIVATE int PH7_builtin_posix_getpwnam(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  737 | `{` |
|    - |  738 | `	struct passwd *pPw;` |
|    - |  739 | `	const char *zName;` |
|    3 |  740 | `	int nName = 0;` |
|    3 |  741 | `	if( nArg < 1 ){` |
|  ! 0 |  742 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  743 | `		return PH7_OK;` |
|    - |  744 | `	}` |
|    3 |  745 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|  ! 0 |  746 | `	SXUNUSED(nName);` |
|    3 |  747 | `	errno = 0;` |
|    3 |  748 | `	pPw = getpwnam(zName);` |
|    3 |  749 | `	if( pPw == 0 ){` |
|    - |  750 | `		/* php stores errno for the NOT-FOUND answer, and a name that simply is` |
|    - |  751 | `		 * not there leaves errno at 0 -- so this OVERWRITES an earlier failure` |
|    - |  752 | `		 * with a zero rather than leaving it standing. */` |
|    2 |  753 | `		return PxFail(pCtx);` |
|    - |  754 | `	}` |
|    1 |  755 | `	return PxPasswdArray(pCtx,pPw);` |
|  ! 0 |  756 | `}` |
|    - |  757 | `/* array\|false posix_getpwuid(int $user_id) */` |
|    4 |  758 | `PH7_PRIVATE int PH7_builtin_posix_getpwuid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  759 | `{` |
|    - |  760 | `	struct passwd *pPw;` |
|    4 |  761 | `	if( nArg < 1 ){` |
|  ! 0 |  762 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  763 | `		return PH7_OK;` |
|    - |  764 | `	}` |
|    4 |  765 | `	errno = 0;` |
|    4 |  766 | `	pPw = getpwuid((uid_t)ph7_value_to_int64(apArg[0]));` |
|    4 |  767 | `	if( pPw == 0 ){` |
|    1 |  768 | `		return PxFail(pCtx);` |
|    - |  769 | `	}` |
|    3 |  770 | `	return PxPasswdArray(pCtx,pPw);` |
|  ! 0 |  771 | `}` |
|    - |  772 | `/* array\|false posix_getgrnam(string $name) */` |
|    1 |  773 | `PH7_PRIVATE int PH7_builtin_posix_getgrnam(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  774 | `{` |
|    - |  775 | `	struct group *pGr;` |
|    - |  776 | `	const char *zName;` |
|    1 |  777 | `	int nName = 0;` |
|    1 |  778 | `	if( nArg < 1 ){` |
|  ! 0 |  779 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  780 | `		return PH7_OK;` |
|    - |  781 | `	}` |
|    1 |  782 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|  ! 0 |  783 | `	SXUNUSED(nName);` |
|    1 |  784 | `	errno = 0;` |
|    1 |  785 | `	pGr = getgrnam(zName);` |
|    1 |  786 | `	if( pGr == 0 ){` |
|    1 |  787 | `		return PxFail(pCtx);` |
|    - |  788 | `	}` |
|  ! 0 |  789 | `	return PxGroupArray(pCtx,pGr);` |
|  ! 0 |  790 | `}` |
|    - |  791 | `/* array\|false posix_getgrgid(int $group_id) */` |
|    4 |  792 | `PH7_PRIVATE int PH7_builtin_posix_getgrgid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  793 | `{` |
|    - |  794 | `	struct group *pGr;` |
|    4 |  795 | `	if( nArg < 1 ){` |
|  ! 0 |  796 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  797 | `		return PH7_OK;` |
|    - |  798 | `	}` |
|    4 |  799 | `	errno = 0;` |
|    4 |  800 | `	pGr = getgrgid((gid_t)ph7_value_to_int64(apArg[0]));` |
|    4 |  801 | `	if( pGr == 0 ){` |
|    1 |  802 | `		return PxFail(pCtx);` |
|    - |  803 | `	}` |
|    3 |  804 | `	return PxGroupArray(pCtx,pGr);` |
|  ! 0 |  805 | `}` |
|    - |  806 | `/* bool posix_initgroups(string $username, int $group_id) */` |
|    1 |  807 | `PH7_PRIVATE int PH7_builtin_posix_initgroups(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  808 | `{` |
|    - |  809 | `	const char *zName;` |
|    1 |  810 | `	int nName = 0;` |
|    1 |  811 | `	if( nArg < 2 ){` |
|  ! 0 |  812 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  813 | `		return PH7_OK;` |
|    - |  814 | `	}` |
|    1 |  815 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|  ! 0 |  816 | `	SXUNUSED(nName);` |
|    - |  817 | `	/* php stores NO errno for this one -- it is a bare RETURN_BOOL. */` |
|    1 |  818 | `	ph7_result_bool(pCtx,initgroups(zName,(gid_t)ph7_value_to_int64(apArg[1])) == 0);` |
|    1 |  819 | `	return PH7_OK;` |
|  ! 0 |  820 | `}` |
|    - |  821 |  |
|    - |  822 | `/* --- Limits and configuration ------------------------------------------- */` |
|    - |  823 |  |
|    - |  824 | `/*` |
|    - |  825 | ` * php's ten resources, in php's own order -- which is what decides the key` |
|    - |  826 | `` * order of `posix_getrlimit()`'s twenty-entry answer.`` |
|    - |  827 | ` */` |
|    - |  828 | `typedef struct px_rlimit px_rlimit;` |
|    - |  829 | `struct px_rlimit {` |
|    - |  830 | `	const char *zName;   /* php's own label for the pair */` |
|    - |  831 | `	int iRes;            /* the platform's RLIMIT_* number */` |
|    - |  832 | `};` |
|    - |  833 | `static const px_rlimit aPxRlimit[] = {` |
|    - |  834 | `	{ "core",      RLIMIT_CORE   },` |
|    - |  835 | `	{ "data",      RLIMIT_DATA   },` |
|    - |  836 | `	{ "stack",     RLIMIT_STACK  },` |
|    - |  837 | `#ifdef RLIMIT_AS` |
|    - |  838 | `	{ "totalmem",  RLIMIT_AS     },` |
|    - |  839 | `#endif` |
|    - |  840 | `#ifdef RLIMIT_RSS` |
|    - |  841 | `	{ "rss",       RLIMIT_RSS    },` |
|    - |  842 | `#endif` |
|    - |  843 | `#ifdef RLIMIT_NPROC` |
|    - |  844 | `	{ "maxproc",   RLIMIT_NPROC  },` |
|    - |  845 | `#endif` |
|    - |  846 | `#ifdef RLIMIT_MEMLOCK` |
|    - |  847 | `	{ "memlock",   RLIMIT_MEMLOCK},` |
|    - |  848 | `#endif` |
|    - |  849 | `	{ "cpu",       RLIMIT_CPU    },` |
|    - |  850 | `	{ "filesize",  RLIMIT_FSIZE  },` |
|    - |  851 | `	{ "openfiles", RLIMIT_NOFILE }` |
|    - |  852 | `};` |
|    - |  853 | ``/* One limit value: a number, or php's STRING `unlimited`. */`` |
|   42 |  854 | `static void PxRlimitValue(ph7_value *pVal,rlim_t iVal)` |
|    - |  855 | `{` |
|   42 |  856 | `	if( iVal == RLIM_INFINITY ){` |
|   25 |  857 | `		ph7_value_reset_string_cursor(pVal);` |
|   25 |  858 | `		ph7_value_string(pVal,"unlimited",(int)sizeof("unlimited")-1);` |
|  ! 0 |  859 | `	}else{` |
|   17 |  860 | `		ph7_value_int64(pVal,(sxi64)iVal);` |
|    - |  861 | `	}` |
|   42 |  862 | `}` |
|    - |  863 | `/* array\|false posix_getrlimit(?int $resource = null) */` |
|    4 |  864 | `PH7_PRIVATE int PH7_builtin_posix_getrlimit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  865 | `{` |
|    - |  866 | `	struct rlimit sLimit;` |
|    - |  867 | `	ph7_value *pArray,*pVal;` |
|    - |  868 | `	sxu32 i;` |
|    4 |  869 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|    - |  870 | `		/* One resource: php answers the pair as a LIST. */` |
|    2 |  871 | `		if( getrlimit((int)ph7_value_to_int64(apArg[0]),&sLimit) != 0 ){` |
|    1 |  872 | `			return PxFail(pCtx);` |
|    - |  873 | `		}` |
|    1 |  874 | `		pArray = ph7_context_new_array(pCtx);` |
|    1 |  875 | `		pVal = ph7_context_new_scalar(pCtx);` |
|    1 |  876 | `		if( pArray == 0 \|\| pVal == 0 ){` |
|  ! 0 |  877 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 |  878 | `			return PH7_OK;` |
|    - |  879 | `		}` |
|    1 |  880 | `		PxRlimitValue(pVal,sLimit.rlim_cur);` |
|    1 |  881 | `		ph7_array_add_elem(pArray,0,pVal);` |
|    1 |  882 | `		PxRlimitValue(pVal,sLimit.rlim_max);` |
|    1 |  883 | `		ph7_array_add_elem(pArray,0,pVal);` |
|    1 |  884 | `		ph7_result_value(pCtx,pArray);` |
|    1 |  885 | `		return PH7_OK;` |
|    - |  886 | `	}` |
|    2 |  887 | `	pArray = ph7_context_new_array(pCtx);` |
|    2 |  888 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    2 |  889 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|  ! 0 |  890 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  891 | `		return PH7_OK;` |
|    - |  892 | `	}` |
|   22 |  893 | `	for( i = 0 ; i < SX_ARRAYSIZE(aPxRlimit) ; ++i ){` |
|    - |  894 | `		char zKey[32];` |
|   20 |  895 | `		if( getrlimit(aPxRlimit[i].iRes,&sLimit) != 0 ){` |
|  ! 0 |  896 | `			continue;` |
|    - |  897 | `		}` |
|   20 |  898 | `		SyBufferFormat(zKey,(sxu32)sizeof(zKey),"soft %s",aPxRlimit[i].zName);` |
|   20 |  899 | `		PxRlimitValue(pVal,sLimit.rlim_cur);` |
|   20 |  900 | `		ph7_array_add_strkey_elem(pArray,zKey,pVal);` |
|   20 |  901 | `		SyBufferFormat(zKey,(sxu32)sizeof(zKey),"hard %s",aPxRlimit[i].zName);` |
|   20 |  902 | `		PxRlimitValue(pVal,sLimit.rlim_max);` |
|   20 |  903 | `		ph7_array_add_strkey_elem(pArray,zKey,pVal);` |
|  ! 0 |  904 | `	}` |
|    2 |  905 | `	ph7_result_value(pCtx,pArray);` |
|    2 |  906 | `	return PH7_OK;` |
|  ! 0 |  907 | `}` |
|    - |  908 | `/* bool posix_setrlimit(int $resource, int $soft_limit, int $hard_limit) */` |
|  ! 0 |  909 | `PH7_PRIVATE int PH7_builtin_posix_setrlimit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  910 | `{` |
|    - |  911 | `	struct rlimit sLimit;` |
|    - |  912 | `	sxi64 iSoft,iHard;` |
|  ! 0 |  913 | `	if( nArg < 3 ){` |
|  ! 0 |  914 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  915 | `		return PH7_OK;` |
|    - |  916 | `	}` |
|  ! 0 |  917 | `	iSoft = ph7_value_to_int64(apArg[1]);` |
|  ! 0 |  918 | `	iHard = ph7_value_to_int64(apArg[2]);` |
|  ! 0 |  919 | `	sLimit.rlim_cur = iSoft < 0 ? RLIM_INFINITY : (rlim_t)iSoft;` |
|  ! 0 |  920 | `	sLimit.rlim_max = iHard < 0 ? RLIM_INFINITY : (rlim_t)iHard;` |
|  ! 0 |  921 | `	if( setrlimit((int)ph7_value_to_int64(apArg[0]),&sLimit) != 0 ){` |
|  ! 0 |  922 | `		return PxFail(pCtx);` |
|    - |  923 | `	}` |
|  ! 0 |  924 | `	ph7_result_bool(pCtx,1);` |
|  ! 0 |  925 | `	return PH7_OK;` |
|  ! 0 |  926 | `}` |
|    - |  927 | `/*` |
|    - |  928 | ` * int posix_sysconf(int $conf_id)` |
|    - |  929 | ` *  php hands back whatever sysconf() answered, -1 included, and stores no` |
|    - |  930 | ` *  errno for it.` |
|    - |  931 | ` */` |
|    4 |  932 | `PH7_PRIVATE int PH7_builtin_posix_sysconf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  933 | `{` |
|    4 |  934 | `	if( nArg < 1 ){` |
|  ! 0 |  935 | `		ph7_result_int64(pCtx,-1);` |
|  ! 0 |  936 | `		return PH7_OK;` |
|    - |  937 | `	}` |
|    4 |  938 | `	ph7_result_int64(pCtx,(sxi64)sysconf((int)ph7_value_to_int64(apArg[0])));` |
|    4 |  939 | `	return PH7_OK;` |
|  ! 0 |  940 | `}` |
|    - |  941 | `/* int\|false posix_pathconf(string $path, int $name) */` |
|    5 |  942 | `PH7_PRIVATE int PH7_builtin_posix_pathconf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  943 | `{` |
|    - |  944 | `	const char *zPath;` |
|    5 |  945 | `	int nPath = 0;` |
|    - |  946 | `	long iRes;` |
|    5 |  947 | `	if( nArg < 2 ){` |
|  ! 0 |  948 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  949 | `		return PH7_OK;` |
|    - |  950 | `	}` |
|    5 |  951 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|    5 |  952 | `	if( nPath < 1 ){` |
|    1 |  953 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|  ! 0 |  954 | `			"%s(): Argument #1 ($path) must not be empty",ph7_function_name(pCtx));` |
|    - |  955 | `	}` |
|    4 |  956 | `	errno = 0;` |
|    4 |  957 | `	iRes = pathconf(zPath,(int)ph7_value_to_int64(apArg[1]));` |
|    4 |  958 | `	if( iRes < 0 && errno != 0 ){` |
|    2 |  959 | `		return PxFail(pCtx);` |
|    - |  960 | `	}` |
|    2 |  961 | `	ph7_result_int64(pCtx,(sxi64)iRes);` |
|    2 |  962 | `	return PH7_OK;` |
|  ! 0 |  963 | `}` |
|    - |  964 | `/* int\|false posix_fpathconf(int\|resource $file_descriptor, int $name) */` |
|    3 |  965 | `PH7_PRIVATE int PH7_builtin_posix_fpathconf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  966 | `{` |
|    - |  967 | `	long iRes;` |
|    - |  968 | `	int fd;` |
|    3 |  969 | `	if( nArg < 2 ){` |
|  ! 0 |  970 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  971 | `		return PH7_OK;` |
|    - |  972 | `	}` |
|    3 |  973 | `	fd = PxFd(pCtx,apArg[0],0,1);` |
|    3 |  974 | `	if( fd < 0 ){` |
|    1 |  975 | `		ph7_result_bool(pCtx,0);` |
|    1 |  976 | `		return PH7_OK;` |
|    - |  977 | `	}` |
|    2 |  978 | `	errno = 0;` |
|    2 |  979 | `	iRes = fpathconf(fd,(int)ph7_value_to_int64(apArg[1]));` |
|    2 |  980 | `	if( iRes < 0 && errno != 0 ){` |
|  ! 0 |  981 | `		return PxFail(pCtx);` |
|    - |  982 | `	}` |
|    2 |  983 | `	ph7_result_int64(pCtx,(sxi64)iRes);` |
|    2 |  984 | `	return PH7_OK;` |
|  ! 0 |  985 | `}` |
|    - |  986 |  |
|    - |  987 | `/* --- The remembered errno, read back ------------------------------------ */` |
|    - |  988 |  |
|    - |  989 | `/* int posix_get_last_error() / int posix_errno() */` |
|   14 |  990 | `PH7_PRIVATE int PH7_builtin_posix_get_last_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |  991 | `{` |
|  ! 0 |  992 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   14 |  993 | `	ph7_result_int64(pCtx,(sxi64)pCtx->pVm->iPosixErr);` |
|   14 |  994 | `	return PH7_OK;` |
|    - |  995 | `}` |
|    - |  996 | `/*` |
|    - |  997 | ` * string posix_strerror(int $error_code)` |
|    - |  998 | ` *  The C library's own sentence, which is what php prints too -- including its` |
|    - |  999 | ` *  wording for a number that names no error.` |
|    - | 1000 | ` */` |
|    5 | 1001 | `PH7_PRIVATE int PH7_builtin_posix_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - | 1002 | `{` |
|    - | 1003 | `	const char *zMsg;` |
|    5 | 1004 | `	if( nArg < 1 ){` |
|  ! 0 | 1005 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 | 1006 | `		return PH7_OK;` |
|    - | 1007 | `	}` |
|    5 | 1008 | `	zMsg = strerror((int)ph7_value_to_int64(apArg[0]));` |
|    5 | 1009 | `	ph7_result_string(pCtx,zMsg ? zMsg : "",-1);` |
|    5 | 1010 | `	return PH7_OK;` |
|  ! 0 | 1011 | `}` |
|    - | 1012 |  |
|    - | 1013 | `#endif /* __WINNT__ */` |
|    - | 1014 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1015 |  |
