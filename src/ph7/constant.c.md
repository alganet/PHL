# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1135/1207 lines (94.03%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `#include <float.h> /* DBL_EPSILON/DBL_MAX/DBL_MIN/DBL_DIG for the PHP_FLOAT_* constants */` |
|      - |    8 | `/* This file implement built-in constants for the PH7 engine. */` |
|      - |    9 | `/*` |
|      - |   10 | ` * PH7_VERSION` |
|      - |   11 | ` * __PH7__` |
|      - |   12 | ` *   Expand the current version of the PH7 engine.` |
|      - |   13 | ` */` |
|      8 |   14 | `static void PH7_VER_Const(ph7_value *pVal,void *pUnused)` |
|      1 |   15 | `{` |
|      4 |   16 | `	SXUNUSED(pUnused);` |
|      9 |   17 | `	ph7_value_string(pVal,ph7_lib_signature(),-1/*Compute length automatically*/);` |
|      9 |   18 | `}` |
|      - |   19 | `/*` |
|      - |   20 | ` * PHP_VERSION, PHP_MAJOR_VERSION, PHP_MINOR_VERSION, PHP_RELEASE_VERSION,` |
|      - |   21 | ` * PHP_EXTRA_VERSION, PHP_VERSION_ID` |
|      - |   22 | ` *   Expand the PHP-compatibility version PHL advertises (see PHP_COMPAT_* in ph7.h).` |
|      - |   23 | ` */` |
|      4 |   24 | `static void PH7_PHPVerConst(ph7_value *pVal,void *pUnused)` |
|      1 |   25 | `{` |
|      2 |   26 | `	SXUNUSED(pUnused);` |
|      5 |   27 | `	ph7_value_string(pVal,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION)-1);` |
|      5 |   28 | `}` |
|      4 |   29 | `static void PH7_PHPMajorConst(ph7_value *pVal,void *pUnused)` |
|      1 |   30 | `{` |
|      2 |   31 | `	SXUNUSED(pUnused);` |
|      5 |   32 | `	ph7_value_int64(pVal,PHP_COMPAT_MAJOR_VERSION);` |
|      5 |   33 | `}` |
|      4 |   34 | `static void PH7_PHPMinorConst(ph7_value *pVal,void *pUnused)` |
|      1 |   35 | `{` |
|      2 |   36 | `	SXUNUSED(pUnused);` |
|      5 |   37 | `	ph7_value_int64(pVal,PHP_COMPAT_MINOR_VERSION);` |
|      5 |   38 | `}` |
|      4 |   39 | `static void PH7_PHPReleaseConst(ph7_value *pVal,void *pUnused)` |
|      1 |   40 | `{` |
|      2 |   41 | `	SXUNUSED(pUnused);` |
|      5 |   42 | `	ph7_value_int64(pVal,PHP_COMPAT_RELEASE_VERSION);` |
|      5 |   43 | `}` |
|      2 |   44 | `static void PH7_PHPExtraConst(ph7_value *pVal,void *pUnused)` |
|      1 |   45 | `{` |
|      1 |   46 | `	SXUNUSED(pUnused);` |
|      3 |   47 | `	ph7_value_string(pVal,PHP_COMPAT_EXTRA_VERSION,(int)sizeof(PHP_COMPAT_EXTRA_VERSION)-1);` |
|      3 |   48 | `}` |
|      8 |   49 | `static void PH7_PHPVerIdConst(ph7_value *pVal,void *pUnused)` |
|      1 |   50 | `{` |
|      4 |   51 | `	SXUNUSED(pUnused);` |
|      9 |   52 | `	ph7_value_int64(pVal,PHP_COMPAT_VERSION_ID);` |
|      9 |   53 | `}` |
|      - |   54 | `#ifdef __WINNT__` |
|      - |   55 | `#include <Windows.h>` |
|      - |   56 | `#elif defined(__UNIXES__)` |
|      - |   57 | `#include <sys/utsname.h>` |
|      - |   58 | `#endif` |
|      - |   59 | `/*` |
|      - |   60 | ` * PHP_OS` |
|      - |   61 | ` *  Expand the name of the host Operating System.` |
|      - |   62 | ` */` |
|   3884 |   63 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|      5 |   64 | `{` |
|      - |   65 | `#if defined(__WINNT__)` |
|      5 |   66 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|      - |   67 | `#elif defined(__UNIXES__)` |
|      - |   68 | `	struct utsname sInfo;` |
|   3884 |   69 | `	if( uname(&sInfo) != 0 ){` |
|    ! 0 |   70 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 |   71 | `	}else{` |
|   3884 |   72 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|      - |   73 | `	}` |
|      - |   74 | `#else` |
|      - |   75 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|      - |   76 | `#endif` |
|   1942 |   77 | `	SXUNUSED(pUnused);` |
|   3889 |   78 | `}` |
|      - |   79 | `/*` |
|      - |   80 | ` * PHP_OS_FAMILY (php 7.2)` |
|      - |   81 | ` *  One of 'Windows', 'BSD', 'Darwin', 'Solaris', 'Linux' or 'Unknown', derived` |
|      - |   82 | ` *  from the host's uname sysname (php maps the same set at build time).` |
|      - |   83 | ` */` |
|    ! 0 |   84 | `static void PH7_OS_FAMILY_Const(ph7_value *pVal,void *pUnused)` |
|    ! 0 |   85 | `{` |
|    ! 0 |   86 | `	SXUNUSED(pUnused);` |
|      - |   87 | `#if defined(__WINNT__)` |
|    ! 0 |   88 | `	ph7_value_string(pVal,"Windows",(int)sizeof("Windows")-1);` |
|      - |   89 | `#elif defined(__UNIXES__)` |
|      - |   90 | `	struct utsname sInfo;` |
|    ! 0 |   91 | `	const char *zFamily = "Unknown";` |
|    ! 0 |   92 | `	if( uname(&sInfo) == 0 ){` |
|    ! 0 |   93 | `		const char *z = sInfo.sysname;` |
|    ! 0 |   94 | `		if( SyStrnicmp(z,"Darwin",sizeof("Darwin")-1) == 0 ){` |
|    ! 0 |   95 | `			zFamily = "Darwin";` |
|    ! 0 |   96 | `		}else if( SyStrnicmp(z,"Linux",sizeof("Linux")-1) == 0 ){` |
|    ! 0 |   97 | `			zFamily = "Linux";` |
|    ! 0 |   98 | `		}else if( SyStrnicmp(z,"SunOS",sizeof("SunOS")-1) == 0 ){` |
|    ! 0 |   99 | `			zFamily = "Solaris";` |
|    ! 0 |  100 | `		}else{` |
|      - |  101 | `			/* FreeBSD/OpenBSD/NetBSD/DragonFly -> 'BSD' (scan for "BSD"). */` |
|    ! 0 |  102 | `			const char *p = z;` |
|    ! 0 |  103 | `			while( p[0] && p[1] && p[2] ){` |
|    ! 0 |  104 | `				if( (p[0]=='B'\|\|p[0]=='b') && (p[1]=='S'\|\|p[1]=='s') && (p[2]=='D'\|\|p[2]=='d') ){` |
|    ! 0 |  105 | `					zFamily = "BSD";` |
|    ! 0 |  106 | `					break;` |
|      - |  107 | `				}` |
|    ! 0 |  108 | `				p++;` |
|      - |  109 | `			}` |
|      - |  110 | `		}` |
|    ! 0 |  111 | `	}` |
|    ! 0 |  112 | `	ph7_value_string(pVal,zFamily,-1);` |
|      - |  113 | `#else` |
|      - |  114 | `	ph7_value_string(pVal,"Unknown",(int)sizeof("Unknown")-1);` |
|      - |  115 | `#endif` |
|    ! 0 |  116 | `}` |
|      - |  117 | `/*` |
|      - |  118 | ` * PHP_SAPI` |
|      - |  119 | ` *  The interface between the interpreter and the host. PHL's host binary is a` |
|      - |  120 | ` *  command-line interpreter, so this is "cli" (matching the CLI default of` |
|      - |  121 | ` *  php_sapi_name(); the built-in -S server's per-request "cli-server" flavour is` |
|      - |  122 | ` *  only surfaced by php_sapi_name(), not this compile-time constant).` |
|      - |  123 | ` */` |
|    ! 0 |  124 | `static void PH7_SAPI_Const(ph7_value *pVal,void *pUnused)` |
|    ! 0 |  125 | `{` |
|    ! 0 |  126 | `	SXUNUSED(pUnused);` |
|    ! 0 |  127 | `	ph7_value_string(pVal,"cli",(int)sizeof("cli")-1);` |
|    ! 0 |  128 | `}` |
|      - |  129 | `/*` |
|      - |  130 | ` * PHP_EOL` |
|      - |  131 | ` *  Expand the correct 'End Of Line' symbol for this platform.` |
|      - |  132 | ` */` |
|    840 |  133 | `static void PH7_EOL_Const(ph7_value *pVal,void *pUnused)` |
|      3 |  134 | `{` |
|    420 |  135 | `	SXUNUSED(pUnused);` |
|      - |  136 | `#ifdef __WINNT__` |
|      3 |  137 | `	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);` |
|      - |  138 | `#else` |
|    840 |  139 | `	ph7_value_string(pVal,"\n",(int)sizeof(char));` |
|      - |  140 | `#endif` |
|    843 |  141 | `}` |
|      - |  142 | `/*` |
|      - |  143 | ` * PHP_INT_MAX` |
|      - |  144 | ` * Expand the largest integer supported.` |
|      - |  145 | ` * Note that PH7 deals with 64-bit integer for all platforms.` |
|      - |  146 | ` */` |
|    106 |  147 | `static void PH7_INTMAX_Const(ph7_value *pVal,void *pUnused)` |
|      3 |  148 | `{` |
|     53 |  149 | `	SXUNUSED(pUnused);` |
|    109 |  150 | `	ph7_value_int64(pVal,SXI64_HIGH);` |
|    109 |  151 | `}` |
|      - |  152 | `/* ext/calendar: the only calendar cal_days_in_month() is asked for in practice. */` |
|      4 |  153 | `static void PH7_CAL_GREGORIAN_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  154 | `{` |
|      2 |  155 | `	SXUNUSED(pUnused);` |
|      5 |  156 | `	ph7_value_int(pVal,0);` |
|      5 |  157 | `}` |
|      - |  158 | `/*` |
|      - |  159 | ` * PHP_INT_MIN (php 7.0)` |
|      - |  160 | ` * Expand the smallest integer supported.` |
|      - |  161 | ` */` |
|     46 |  162 | `static void PH7_INTMIN_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  163 | `{` |
|     23 |  164 | `	SXUNUSED(pUnused);` |
|     47 |  165 | `	ph7_value_int64(pVal,SMALLEST_INT64);` |
|     47 |  166 | `}` |
|      - |  167 | `/*` |
|      - |  168 | ` * PHP_INT_SIZE` |
|      - |  169 | ` * Expand the size in bytes of a 64-bit integer.` |
|      - |  170 | ` */` |
|      4 |  171 | `static void PH7_INTSIZE_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  172 | `{` |
|      2 |  173 | `	SXUNUSED(pUnused);` |
|      5 |  174 | `	ph7_value_int64(pVal,sizeof(sxi64));` |
|      5 |  175 | `}` |
|      - |  176 | `/*` |
|      - |  177 | ` * PHP_FLOAT_EPSILON / PHP_FLOAT_MAX / PHP_FLOAT_MIN / PHP_FLOAT_DIG (php 7.2)` |
|      - |  178 | ` * Double-precision characteristics, sourced from <float.h> exactly like php` |
|      - |  179 | ` * so they track the compiling platform's actual double representation.` |
|      - |  180 | ` */` |
|      4 |  181 | `static void PH7_FLOATEPSILON_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  182 | `{` |
|      2 |  183 | `	SXUNUSED(pUnused);` |
|      5 |  184 | `	ph7_value_double(pVal,DBL_EPSILON);` |
|      5 |  185 | `}` |
|      2 |  186 | `static void PH7_FLOATMAX_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  187 | `{` |
|      1 |  188 | `	SXUNUSED(pUnused);` |
|      3 |  189 | `	ph7_value_double(pVal,DBL_MAX);` |
|      3 |  190 | `}` |
|      2 |  191 | `static void PH7_FLOATMIN_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  192 | `{` |
|      1 |  193 | `	SXUNUSED(pUnused);` |
|      3 |  194 | `	ph7_value_double(pVal,DBL_MIN);` |
|      3 |  195 | `}` |
|      2 |  196 | `static void PH7_FLOATDIG_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  197 | `{` |
|      1 |  198 | `	SXUNUSED(pUnused);` |
|      3 |  199 | `	ph7_value_int64(pVal,DBL_DIG);` |
|      3 |  200 | `}` |
|      - |  201 | `/*` |
|      - |  202 | ` * DIRECTORY_SEPARATOR.` |
|      - |  203 | ` * Expand the directory separator character.` |
|      - |  204 | ` */` |
|    312 |  205 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|      4 |  206 | `{` |
|    156 |  207 | `	SXUNUSED(pUnused);` |
|      - |  208 | `#ifdef __WINNT__` |
|      4 |  209 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|      - |  210 | `#else` |
|    312 |  211 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|      - |  212 | `#endif` |
|    316 |  213 | `}` |
|      - |  214 | `/*` |
|      - |  215 | ` * PATH_SEPARATOR.` |
|      - |  216 | ` * Expand the path separator character.` |
|      - |  217 | ` */` |
|      2 |  218 | `static void PH7_PATHSEP_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  219 | `{` |
|      1 |  220 | `	SXUNUSED(pUnused);` |
|      - |  221 | `#ifdef __WINNT__` |
|      1 |  222 | `	ph7_value_string(pVal,";",(int)sizeof(char));` |
|      - |  223 | `#else` |
|      2 |  224 | `	ph7_value_string(pVal,":",(int)sizeof(char));` |
|      - |  225 | `#endif` |
|      3 |  226 | `}` |
|      - |  227 |  |
|      - |  228 | `#if defined(PH7_ENABLE_MATH_FUNC)` |
|      - |  229 | `/*` |
|      - |  230 | ` * NAN constant: floating-point Not-A-Number` |
|      - |  231 | ` */` |
|     88 |  232 | `static void PH7_NAN_Const(ph7_value *pVal,void *pUnused)` |
|      2 |  233 | `{` |
|     44 |  234 | `	SXUNUSED(pUnused);` |
|     90 |  235 | `	ph7_value_double(pVal, PH7_NAN_VALUE());` |
|     90 |  236 | `}` |
|      - |  237 |  |
|      - |  238 | `/*` |
|      - |  239 | ` * INF constant: positive infinity` |
|      - |  240 | ` */` |
|     92 |  241 | `static void PH7_INF_Const(ph7_value *pVal,void *pUnused)` |
|      2 |  242 | `{` |
|     46 |  243 | `	SXUNUSED(pUnused);` |
|      - |  244 | `	/* similarly avoid the INFINITY macro */` |
|     94 |  245 | `	ph7_value_double(pVal, PH7_INF_VALUE());` |
|     94 |  246 | `}` |
|      - |  247 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - |  248 |  |
|      - |  249 | `#ifndef __WINNT__` |
|      - |  250 | `#include <time.h>` |
|      - |  251 | `#endif` |
|      - |  252 | `/*` |
|      - |  253 | ` * __TIME__` |
|      - |  254 | ` *  Expand the current time (GMT).` |
|      - |  255 | ` */` |
|      2 |  256 | `static void PH7_TIME_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  257 | `{` |
|      - |  258 | `	Sytm sTm;` |
|      - |  259 | `#ifdef __WINNT__` |
|      - |  260 | `	SYSTEMTIME sOS;` |
|      1 |  261 | `	GetSystemTime(&sOS);` |
|      1 |  262 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - |  263 | `#else` |
|      - |  264 | `	struct tm *pTm;` |
|      - |  265 | `	time_t t;` |
|      2 |  266 | `	time(&t);` |
|      2 |  267 | `	pTm = gmtime(&t);` |
|      2 |  268 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - |  269 | `#endif` |
|      1 |  270 | `	SXUNUSED(pUnused); /* cc warning */` |
|      - |  271 | `	/* Expand */` |
|      3 |  272 | `	ph7_value_string_format(pVal,"%02d:%02d:%02d",sTm.tm_hour,sTm.tm_min,sTm.tm_sec);` |
|      3 |  273 | `}` |
|      - |  274 | `/*` |
|      - |  275 | ` * __DATE__` |
|      - |  276 | ` *  Expand the current date in the ISO-8601 format.` |
|      - |  277 | ` */` |
|      2 |  278 | `static void PH7_DATE_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  279 | `{` |
|      - |  280 | `	Sytm sTm;` |
|      - |  281 | `#ifdef __WINNT__` |
|      - |  282 | `	SYSTEMTIME sOS;` |
|      1 |  283 | `	GetSystemTime(&sOS);` |
|      1 |  284 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - |  285 | `#else` |
|      - |  286 | `	struct tm *pTm;` |
|      - |  287 | `	time_t t;` |
|      2 |  288 | `	time(&t);` |
|      2 |  289 | `	pTm = gmtime(&t);` |
|      2 |  290 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - |  291 | `#endif` |
|      1 |  292 | `	SXUNUSED(pUnused); /* cc warning */` |
|      - |  293 | `	/* Expand */` |
|      3 |  294 | `	ph7_value_string_format(pVal,"%04d-%02d-%02d",sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|      3 |  295 | `}` |
|      - |  296 | `/*` |
|      - |  297 | ` * __FILE__` |
|      - |  298 | ` *  Path of the processed script.` |
|      - |  299 | ` */` |
|    ! 0 |  300 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 |  301 | `{` |
|    ! 0 |  302 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  303 | `	SyString *pFile;` |
|      - |  304 | `	/* Peek the top entry */` |
|    ! 0 |  305 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    ! 0 |  306 | `	if( pFile == 0 ){` |
|      - |  307 | `		/* Expand the magic word: ":MEMORY:" */` |
|    ! 0 |  308 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|    ! 0 |  309 | `	}else{` |
|    ! 0 |  310 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|      - |  311 | `	}` |
|    ! 0 |  312 | `}` |
|      - |  313 | `/*` |
|      - |  314 | ` * __DIR__` |
|      - |  315 | ` *  Directory holding the processed script.` |
|      - |  316 | ` */` |
|    ! 0 |  317 | `static void PH7_DIR_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 |  318 | `{` |
|    ! 0 |  319 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  320 | `	SyString *pFile;` |
|      - |  321 | `	/* Peek the top entry */` |
|    ! 0 |  322 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|    ! 0 |  323 | `	if( pFile == 0 ){` |
|      - |  324 | `		/* Expand the magic word: ":MEMORY:" */` |
|    ! 0 |  325 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|    ! 0 |  326 | `	}else{` |
|    ! 0 |  327 | `		if( pFile->nByte > 0 ){` |
|      - |  328 | `			const char *zDir;` |
|      - |  329 | `			int nLen;` |
|    ! 0 |  330 | `			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|    ! 0 |  331 | `			ph7_value_string(pVal,zDir,nLen);` |
|    ! 0 |  332 | `		}else{` |
|      - |  333 | `			/* Expand '.' as the current directory*/` |
|    ! 0 |  334 | `			ph7_value_string(pVal,".",(int)sizeof(char));` |
|      - |  335 | `		}` |
|      - |  336 | `	}` |
|    ! 0 |  337 | `}` |
|      - |  338 | `/*` |
|      - |  339 | ` * PHP_SHLIB_SUFFIX` |
|      - |  340 | ` *  Expand shared library suffix.` |
|      - |  341 | ` */` |
|      2 |  342 | `static void PH7_PHP_SHLIB_SUFFIX_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 |  343 | `{` |
|      - |  344 | `#ifdef __WINNT__` |
|    ! 0 |  345 | `	ph7_value_string(pVal,"dll",(int)sizeof("dll")-1);` |
|      - |  346 | `#else` |
|      2 |  347 | `	ph7_value_string(pVal,"so",(int)sizeof("so")-1);` |
|      - |  348 | `#endif` |
|      1 |  349 | `	SXUNUSED(pUserData); /* cc warning */` |
|      2 |  350 | `}` |
|      - |  351 | `/*` |
|      - |  352 | ` * E_ERROR` |
|      - |  353 | ` *  Expands 1` |
|      - |  354 | ` */` |
|      2 |  355 | `static void PH7_E_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  356 | `{` |
|      3 |  357 | `	ph7_value_int(pVal,1);` |
|      1 |  358 | `	SXUNUSED(pUserData);` |
|      3 |  359 | `}` |
|      - |  360 | `/*` |
|      - |  361 | ` * E_WARNING` |
|      - |  362 | ` *  Expands 2` |
|      - |  363 | ` */` |
|      2 |  364 | `static void PH7_E_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  365 | `{` |
|      3 |  366 | `	ph7_value_int(pVal,2);` |
|      1 |  367 | `	SXUNUSED(pUserData);` |
|      3 |  368 | `}` |
|      - |  369 | `/*` |
|      - |  370 | ` * E_PARSE` |
|      - |  371 | ` *  Expands 4` |
|      - |  372 | ` */` |
|      2 |  373 | `static void PH7_E_PARSE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  374 | `{` |
|      3 |  375 | `	ph7_value_int(pVal,4);` |
|      1 |  376 | `	SXUNUSED(pUserData);` |
|      3 |  377 | `}` |
|      - |  378 | `/*` |
|      - |  379 | ` * E_NOTICE` |
|      - |  380 | ` * Expands 8` |
|      - |  381 | ` */` |
|      2 |  382 | `static void PH7_E_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  383 | `{` |
|      3 |  384 | `	ph7_value_int(pVal,8);` |
|      1 |  385 | `	SXUNUSED(pUserData);` |
|      3 |  386 | `}` |
|      - |  387 | `/*` |
|      - |  388 | ` * E_CORE_ERROR` |
|      - |  389 | ` * Expands 16` |
|      - |  390 | ` */` |
|      2 |  391 | `static void PH7_E_CORE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  392 | `{` |
|      3 |  393 | `	ph7_value_int(pVal,16);` |
|      1 |  394 | `	SXUNUSED(pUserData);` |
|      3 |  395 | `}` |
|      - |  396 | `/*` |
|      - |  397 | ` * E_CORE_WARNING` |
|      - |  398 | ` * Expands 32` |
|      - |  399 | ` */` |
|      2 |  400 | `static void PH7_E_CORE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  401 | `{` |
|      3 |  402 | `	ph7_value_int(pVal,32);` |
|      1 |  403 | `	SXUNUSED(pUserData);` |
|      3 |  404 | `}` |
|      - |  405 | `/*` |
|      - |  406 | ` * E_COMPILE_ERROR` |
|      - |  407 | ` * Expands 64` |
|      - |  408 | ` */` |
|      2 |  409 | `static void PH7_E_COMPILE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  410 | `{` |
|      3 |  411 | `	ph7_value_int(pVal,64);` |
|      1 |  412 | `	SXUNUSED(pUserData);` |
|      3 |  413 | `}` |
|      - |  414 | `/*` |
|      - |  415 | ` * E_COMPILE_WARNING` |
|      - |  416 | ` * Expands 128` |
|      - |  417 | ` */` |
|      2 |  418 | `static void PH7_E_COMPILE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  419 | `{` |
|      3 |  420 | `	ph7_value_int(pVal,128);` |
|      1 |  421 | `	SXUNUSED(pUserData);` |
|      3 |  422 | `}` |
|      - |  423 | `/*` |
|      - |  424 | ` * E_USER_ERROR` |
|      - |  425 | ` * Expands 256` |
|      - |  426 | ` */` |
|      2 |  427 | `static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  428 | `{` |
|      3 |  429 | `	ph7_value_int(pVal,256);` |
|      1 |  430 | `	SXUNUSED(pUserData);` |
|      3 |  431 | `}` |
|      - |  432 | `/*` |
|      - |  433 | ` * E_USER_WARNING` |
|      - |  434 | ` * Expands 512` |
|      - |  435 | ` */` |
|     16 |  436 | `static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      3 |  437 | `{` |
|     19 |  438 | `	ph7_value_int(pVal,512);` |
|      8 |  439 | `	SXUNUSED(pUserData);` |
|     19 |  440 | `}` |
|      - |  441 | `/*` |
|      - |  442 | ` * E_USER_NOTICE` |
|      - |  443 | ` * Expands 1024` |
|      - |  444 | ` */` |
|      6 |  445 | `static void PH7_E_USER_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|      3 |  446 | `{` |
|      9 |  447 | `	ph7_value_int(pVal,1024);` |
|      3 |  448 | `	SXUNUSED(pUserData);` |
|      9 |  449 | `}` |
|      - |  450 | `/*` |
|      - |  451 | ` * E_STRICT` |
|      - |  452 | ` * Expands 2048` |
|      - |  453 | ` */` |
|      2 |  454 | `static void PH7_E_STRICT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  455 | `{` |
|      3 |  456 | `	ph7_value_int(pVal,2048);` |
|      1 |  457 | `	SXUNUSED(pUserData);` |
|      3 |  458 | `}` |
|      - |  459 | `/*` |
|      - |  460 | ` * E_RECOVERABLE_ERROR` |
|      - |  461 | ` * Expands 4096` |
|      - |  462 | ` */` |
|      2 |  463 | `static void PH7_E_RECOVERABLE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  464 | `{` |
|      3 |  465 | `	ph7_value_int(pVal,4096);` |
|      1 |  466 | `	SXUNUSED(pUserData);` |
|      3 |  467 | `}` |
|      - |  468 | `/*` |
|      - |  469 | ` * E_DEPRECATED` |
|      - |  470 | ` * Expands 8192` |
|      - |  471 | ` */` |
|     22 |  472 | `static void PH7_E_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  473 | `{` |
|     27 |  474 | `	ph7_value_int(pVal,8192);` |
|     11 |  475 | `	SXUNUSED(pUserData);` |
|     27 |  476 | `}` |
|      - |  477 | `/*` |
|      - |  478 | ` * E_USER_DEPRECATED` |
|      - |  479 | ` *   Expands 16384.` |
|      - |  480 | ` */` |
|      2 |  481 | `static void PH7_E_USER_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  482 | `{` |
|      3 |  483 | `	ph7_value_int(pVal,16384);` |
|      1 |  484 | `	SXUNUSED(pUserData);` |
|      3 |  485 | `}` |
|      - |  486 | `/*` |
|      - |  487 | ` * E_ALL` |
|      - |  488 | ` *  Expands 30719 (php 8: E_STRICT is no longer part of E_ALL)` |
|      - |  489 | ` */` |
|     36 |  490 | `static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  491 | `{` |
|     41 |  492 | `	ph7_value_int(pVal,30719);` |
|     18 |  493 | `	SXUNUSED(pUserData);` |
|     41 |  494 | `}` |
|      - |  495 | `/*` |
|      - |  496 | ` * CASE_LOWER` |
|      - |  497 | ` *  Expands 0.` |
|      - |  498 | ` */` |
|      2 |  499 | `static void PH7_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  500 | `{` |
|      3 |  501 | `	ph7_value_int(pVal,0);` |
|      1 |  502 | `	SXUNUSED(pUserData);` |
|      3 |  503 | `}` |
|      - |  504 | `/*` |
|      - |  505 | ` * CASE_UPPER` |
|      - |  506 | ` *  Expands 1.` |
|      - |  507 | ` */` |
|      8 |  508 | `static void PH7_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  509 | `{` |
|     10 |  510 | `	ph7_value_int(pVal,1);` |
|      4 |  511 | `	SXUNUSED(pUserData);` |
|     10 |  512 | `}` |
|      - |  513 | `/*` |
|      - |  514 | ` * STR_PAD_LEFT` |
|      - |  515 | ` *  Expands 0.` |
|      - |  516 | ` */` |
|      4 |  517 | `static void PH7_STR_PAD_LEFT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  518 | `{` |
|      5 |  519 | `	ph7_value_int(pVal,0);` |
|      2 |  520 | `	SXUNUSED(pUserData);` |
|      5 |  521 | `}` |
|      - |  522 | `/*` |
|      - |  523 | ` * STR_PAD_RIGHT` |
|      - |  524 | ` *  Expands 1.` |
|      - |  525 | ` */` |
|      4 |  526 | `static void PH7_STR_PAD_RIGHT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  527 | `{` |
|      5 |  528 | `	ph7_value_int(pVal,1);` |
|      2 |  529 | `	SXUNUSED(pUserData);` |
|      5 |  530 | `}` |
|      - |  531 | `/*` |
|      - |  532 | ` * STR_PAD_BOTH` |
|      - |  533 | ` *  Expands 2.` |
|      - |  534 | ` */` |
|      2 |  535 | `static void PH7_STR_PAD_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  536 | `{` |
|      3 |  537 | `	ph7_value_int(pVal,2);` |
|      1 |  538 | `	SXUNUSED(pUserData);` |
|      3 |  539 | `}` |
|      - |  540 | `/*` |
|      - |  541 | ` * COUNT_NORMAL` |
|      - |  542 | ` *  Expands 0` |
|      - |  543 | ` */` |
|      6 |  544 | `static void PH7_COUNT_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  545 | `{` |
|      8 |  546 | `	ph7_value_int(pVal,0);` |
|      3 |  547 | `	SXUNUSED(pUserData);` |
|      8 |  548 | `}` |
|      - |  549 | `/*` |
|      - |  550 | ` * COUNT_RECURSIVE` |
|      - |  551 | ` *  Expands 1.` |
|      - |  552 | ` */` |
|     20 |  553 | `static void PH7_COUNT_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  554 | `{` |
|     22 |  555 | `	ph7_value_int(pVal,1);` |
|     10 |  556 | `	SXUNUSED(pUserData);` |
|     22 |  557 | `}` |
|      - |  558 | `/*` |
|      - |  559 | ` * php's sort-flag constants. The VALUES must match php exactly: they are a` |
|      - |  560 | ` * public ABI (code passes literal ints, dumps them, and OR-combines the base` |
|      - |  561 | ` * type with SORT_FLAG_CASE). SORT_ASC/SORT_DESC are the array_multisort` |
|      - |  562 | ` * direction flags.` |
|      - |  563 | ` * SORT_REGULAR 0 · SORT_NUMERIC 1 · SORT_STRING 2 · SORT_DESC 3 · SORT_ASC 4 ·` |
|      - |  564 | ` * SORT_LOCALE_STRING 5 · SORT_NATURAL 6 · SORT_FLAG_CASE 8` |
|      - |  565 | ` */` |
|      4 |  566 | `static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  567 | `{` |
|      5 |  568 | `	ph7_value_int(pVal,4);` |
|      2 |  569 | `	SXUNUSED(pUserData);` |
|      5 |  570 | `}` |
|      4 |  571 | `static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  572 | `{` |
|      5 |  573 | `	ph7_value_int(pVal,3);` |
|      2 |  574 | `	SXUNUSED(pUserData);` |
|      5 |  575 | `}` |
|      8 |  576 | `static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  577 | `{` |
|      9 |  578 | `	ph7_value_int(pVal,0);` |
|      4 |  579 | `	SXUNUSED(pUserData);` |
|      9 |  580 | `}` |
|     18 |  581 | `static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  582 | `{` |
|     19 |  583 | `	ph7_value_int(pVal,1);` |
|      9 |  584 | `	SXUNUSED(pUserData);` |
|     19 |  585 | `}` |
|     28 |  586 | `static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  587 | `{` |
|     29 |  588 | `	ph7_value_int(pVal,2);` |
|     14 |  589 | `	SXUNUSED(pUserData);` |
|     29 |  590 | `}` |
|      2 |  591 | `static void PH7_SORT_LOCALE_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  592 | `{` |
|      3 |  593 | `	ph7_value_int(pVal,5);` |
|      1 |  594 | `	SXUNUSED(pUserData);` |
|      3 |  595 | `}` |
|     12 |  596 | `static void PH7_SORT_NATURAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  597 | `{` |
|     13 |  598 | `	ph7_value_int(pVal,6);` |
|      6 |  599 | `	SXUNUSED(pUserData);` |
|     13 |  600 | `}` |
|     10 |  601 | `static void PH7_SORT_FLAG_CASE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  602 | `{` |
|     11 |  603 | `	ph7_value_int(pVal,8);` |
|      5 |  604 | `	SXUNUSED(pUserData);` |
|     11 |  605 | `}` |
|      - |  606 | `/*` |
|      - |  607 | ` * PHP_ROUND_HALF_UP` |
|      - |  608 | ` *  Expands 1.` |
|      - |  609 | ` */` |
|      4 |  610 | `static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  611 | `{` |
|      5 |  612 | `	ph7_value_int(pVal,1);` |
|      2 |  613 | `	SXUNUSED(pUserData);` |
|      5 |  614 | `}` |
|      - |  615 | `/*` |
|      - |  616 | ` * PHP_SESSION_DISABLED / PHP_SESSION_NONE / PHP_SESSION_ACTIVE` |
|      - |  617 | ` *  session_status() states (0 / 1 / 2).` |
|      - |  618 | ` */` |
|      2 |  619 | `static void PH7_PHP_SESSION_DISABLED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  620 | `{` |
|      3 |  621 | `	ph7_value_int(pVal,0);` |
|      1 |  622 | `	SXUNUSED(pUserData);` |
|      3 |  623 | `}` |
|      6 |  624 | `static void PH7_PHP_SESSION_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  625 | `{` |
|      7 |  626 | `	ph7_value_int(pVal,1);` |
|      3 |  627 | `	SXUNUSED(pUserData);` |
|      7 |  628 | `}` |
|     30 |  629 | `static void PH7_PHP_SESSION_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  630 | `{` |
|     32 |  631 | `	ph7_value_int(pVal,2);` |
|     15 |  632 | `	SXUNUSED(pUserData);` |
|     32 |  633 | `}` |
|      - |  634 | `/*` |
|      - |  635 | ` * INI_USER / INI_PERDIR / INI_SYSTEM / INI_ALL` |
|      - |  636 | ` *  php.ini access levels (1 / 2 / 4 / 7).` |
|      - |  637 | ` */` |
|     12 |  638 | `static void PH7_INI_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  639 | `{` |
|     13 |  640 | `	ph7_value_int(pVal,1);` |
|      6 |  641 | `	SXUNUSED(pUserData);` |
|     13 |  642 | `}` |
|      2 |  643 | `static void PH7_INI_PERDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  644 | `{` |
|      3 |  645 | `	ph7_value_int(pVal,2);` |
|      1 |  646 | `	SXUNUSED(pUserData);` |
|      3 |  647 | `}` |
|      2 |  648 | `static void PH7_INI_SYSTEM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  649 | `{` |
|      3 |  650 | `	ph7_value_int(pVal,4);` |
|      1 |  651 | `	SXUNUSED(pUserData);` |
|      3 |  652 | `}` |
|      2 |  653 | `static void PH7_INI_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  654 | `{` |
|      3 |  655 | `	ph7_value_int(pVal,7);` |
|      1 |  656 | `	SXUNUSED(pUserData);` |
|      3 |  657 | `}` |
|      - |  658 | `/*` |
|      - |  659 | ` * MB_CASE_UPPER / MB_CASE_LOWER / MB_CASE_TITLE (0 / 1 / 2)` |
|      - |  660 | ` */` |
|      4 |  661 | `static void PH7_MB_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  662 | `{` |
|      5 |  663 | `	ph7_value_int(pVal,0);` |
|      2 |  664 | `	SXUNUSED(pUserData);` |
|      5 |  665 | `}` |
|      4 |  666 | `static void PH7_MB_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  667 | `{` |
|      5 |  668 | `	ph7_value_int(pVal,1);` |
|      2 |  669 | `	SXUNUSED(pUserData);` |
|      5 |  670 | `}` |
|      4 |  671 | `static void PH7_MB_CASE_TITLE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  672 | `{` |
|      5 |  673 | `	ph7_value_int(pVal,2);` |
|      2 |  674 | `	SXUNUSED(pUserData);` |
|      5 |  675 | `}` |
|      - |  676 | `/*` |
|      - |  677 | ` * SPHP_ROUND_HALF_DOWN` |
|      - |  678 | ` *  Expands 2.` |
|      - |  679 | ` */` |
|      4 |  680 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  681 | `{` |
|      5 |  682 | `	ph7_value_int(pVal,2);` |
|      2 |  683 | `	SXUNUSED(pUserData);` |
|      5 |  684 | `}` |
|      - |  685 | `/*` |
|      - |  686 | ` * PHP_ROUND_HALF_EVEN` |
|      - |  687 | ` *  Expands 3.` |
|      - |  688 | ` */` |
|      8 |  689 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  690 | `{` |
|      9 |  691 | `	ph7_value_int(pVal,3);` |
|      4 |  692 | `	SXUNUSED(pUserData);` |
|      9 |  693 | `}` |
|      - |  694 | `/*` |
|      - |  695 | ` * PHP_ROUND_HALF_ODD` |
|      - |  696 | ` *  Expands 4.` |
|      - |  697 | ` */` |
|      4 |  698 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  699 | `{` |
|      5 |  700 | `	ph7_value_int(pVal,4);` |
|      2 |  701 | `	SXUNUSED(pUserData);` |
|      5 |  702 | `}` |
|      - |  703 | `/*` |
|      - |  704 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|      - |  705 | ` *  Expand 0x01` |
|      - |  706 | ` * NOTE:` |
|      - |  707 | ` *  The expanded value must be a power of two.` |
|      - |  708 | ` */` |
|      4 |  709 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  710 | `{` |
|      5 |  711 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      2 |  712 | `	SXUNUSED(pUserData);` |
|      5 |  713 | `}` |
|      - |  714 | `/*` |
|      - |  715 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|      - |  716 | ` *  Expand 0x02` |
|      - |  717 | ` * NOTE:` |
|      - |  718 | ` *  The expanded value must be a power of two.` |
|      - |  719 | ` */` |
|      4 |  720 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  721 | `{` |
|      5 |  722 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      2 |  723 | `	SXUNUSED(pUserData);` |
|      5 |  724 | `}` |
|      - |  725 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  726 | `/*` |
|      - |  727 | ` * M_PI` |
|      - |  728 | ` *  Expand the value of pi.` |
|      - |  729 | ` */` |
|      8 |  730 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  731 | `{` |
|      4 |  732 | `	SXUNUSED(pUserData); /* cc warning */` |
|     10 |  733 | `	ph7_value_double(pVal,PH7_PI);` |
|     10 |  734 | `}` |
|      - |  735 | `/*` |
|      - |  736 | ` * M_E` |
|      - |  737 | ` *  Expand 2.7182818284590452354` |
|      - |  738 | ` */` |
|      2 |  739 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  740 | `{` |
|      1 |  741 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  742 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|      3 |  743 | `}` |
|      - |  744 | `/*` |
|      - |  745 | ` * M_LOG2E` |
|      - |  746 | ` *  Expand 2.7182818284590452354` |
|      - |  747 | ` */` |
|      2 |  748 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  749 | `{` |
|      1 |  750 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  751 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      3 |  752 | `}` |
|      - |  753 | `/*` |
|      - |  754 | ` * M_LOG10E` |
|      - |  755 | ` *  Expand 0.4342944819032518276` |
|      - |  756 | ` */` |
|      2 |  757 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  758 | `{` |
|      1 |  759 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  760 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      3 |  761 | `}` |
|      - |  762 | `/*` |
|      - |  763 | ` * M_LN2` |
|      - |  764 | ` *  Expand 	0.69314718055994530942` |
|      - |  765 | ` */` |
|      2 |  766 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  767 | `{` |
|      1 |  768 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  769 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      3 |  770 | `}` |
|      - |  771 | `/*` |
|      - |  772 | ` * M_LN10` |
|      - |  773 | ` *  Expand 	2.30258509299404568402` |
|      - |  774 | ` */` |
|      2 |  775 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  776 | `{` |
|      1 |  777 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  778 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      3 |  779 | `}` |
|      - |  780 | `/*` |
|      - |  781 | ` * M_PI_2` |
|      - |  782 | ` *  Expand 	1.57079632679489661923` |
|      - |  783 | ` */` |
|      2 |  784 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  785 | `{` |
|      1 |  786 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  787 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      3 |  788 | `}` |
|      - |  789 | `/*` |
|      - |  790 | ` * M_PI_4` |
|      - |  791 | ` *  Expand 	0.78539816339744830962` |
|      - |  792 | ` */` |
|      2 |  793 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  794 | `{` |
|      1 |  795 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  796 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      3 |  797 | `}` |
|      - |  798 | `/*` |
|      - |  799 | ` * M_1_PI` |
|      - |  800 | ` *  Expand 	0.31830988618379067154` |
|      - |  801 | ` */` |
|      2 |  802 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  803 | `{` |
|      1 |  804 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  805 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      3 |  806 | `}` |
|      - |  807 | `/*` |
|      - |  808 | ` * M_2_PI` |
|      - |  809 | ` *  Expand 0.63661977236758134308` |
|      - |  810 | ` */` |
|      4 |  811 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  812 | `{` |
|      2 |  813 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  814 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      5 |  815 | `}` |
|      - |  816 | `/*` |
|      - |  817 | ` * M_SQRTPI` |
|      - |  818 | ` *  Expand 1.77245385090551602729` |
|      - |  819 | ` */` |
|      2 |  820 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  821 | `{` |
|      1 |  822 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  823 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      3 |  824 | `}` |
|      - |  825 | `/*` |
|      - |  826 | ` * M_2_SQRTPI` |
|      - |  827 | ` *  Expand 	1.12837916709551257390` |
|      - |  828 | ` */` |
|      2 |  829 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  830 | `{` |
|      1 |  831 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  832 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      3 |  833 | `}` |
|      - |  834 | `/*` |
|      - |  835 | ` * M_SQRT2` |
|      - |  836 | ` *  Expand 	1.41421356237309504880` |
|      - |  837 | ` */` |
|      2 |  838 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  839 | `{` |
|      1 |  840 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  841 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      3 |  842 | `}` |
|      - |  843 | `/*` |
|      - |  844 | ` * M_SQRT3` |
|      - |  845 | ` *  Expand 	1.73205080756887729352` |
|      - |  846 | ` */` |
|      2 |  847 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  848 | `{` |
|      1 |  849 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  850 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      3 |  851 | `}` |
|      - |  852 | `/*` |
|      - |  853 | ` * M_SQRT1_2` |
|      - |  854 | ` *  Expand 	0.70710678118654752440` |
|      - |  855 | ` */` |
|      2 |  856 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  857 | `{` |
|      1 |  858 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  859 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      3 |  860 | `}` |
|      - |  861 | `/*` |
|      - |  862 | ` * M_LNPI` |
|      - |  863 | ` *  Expand 	1.14472988584940017414` |
|      - |  864 | ` */` |
|      2 |  865 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  866 | `{` |
|      1 |  867 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  868 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      3 |  869 | `}` |
|      - |  870 | `/*` |
|      - |  871 | ` * M_EULER` |
|      - |  872 | ` *  Expand  0.57721566490153286061` |
|      - |  873 | ` */` |
|      2 |  874 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  875 | `{` |
|      1 |  876 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  877 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      3 |  878 | `}` |
|      - |  879 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|      - |  880 | `/*` |
|      - |  881 | ` * DATE_ATOM` |
|      - |  882 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|      - |  883 | ` */` |
|      4 |  884 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  885 | `{` |
|      2 |  886 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  887 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      5 |  888 | `}` |
|      - |  889 | `/*` |
|      - |  890 | ` * DATE_COOKIE` |
|      - |  891 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  892 | ` */` |
|      2 |  893 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  894 | `{` |
|      1 |  895 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  896 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  897 | `}` |
|      - |  898 | `/*` |
|      - |  899 | ` * DATE_ISO8601` |
|      - |  900 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|      - |  901 | ` */` |
|      2 |  902 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  903 | `{` |
|      1 |  904 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  905 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      3 |  906 | `}` |
|      - |  907 | `/*` |
|      - |  908 | ` * DATE_RFC822` |
|      - |  909 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|      - |  910 | ` */` |
|      2 |  911 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  912 | `{` |
|      1 |  913 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  914 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  915 | `}` |
|      - |  916 | `/*` |
|      - |  917 | ` * DATE_RFC850` |
|      - |  918 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  919 | ` */` |
|      2 |  920 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  921 | `{` |
|      1 |  922 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  923 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  924 | `}` |
|      - |  925 | `/*` |
|      - |  926 | ` * DATE_RFC1036` |
|      - |  927 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  928 | ` */` |
|      2 |  929 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  930 | `{` |
|      1 |  931 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  932 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  933 | `}` |
|      - |  934 | `/*` |
|      - |  935 | ` * DATE_RFC1123` |
|      - |  936 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  937 | ` */` |
|      2 |  938 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  939 | `{` |
|      1 |  940 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  941 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  942 | `}` |
|      - |  943 | `/*` |
|      - |  944 | ` * DATE_RFC2822` |
|      - |  945 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  946 | ` */` |
|      2 |  947 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  948 | `{` |
|      1 |  949 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  950 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  951 | `}` |
|      - |  952 | `/*` |
|      - |  953 | ` * DATE_RSS` |
|      - |  954 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  955 | ` */` |
|      2 |  956 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  957 | `{` |
|      1 |  958 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  959 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  960 | `}` |
|      - |  961 | `/*` |
|      - |  962 | ` * DATE_W3C` |
|      - |  963 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|      - |  964 | ` */` |
|      2 |  965 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  966 | `{` |
|      1 |  967 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  968 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  969 | `}` |
|      - |  970 | `/*` |
|      - |  971 | ` * The ENT_* values are PHP-exact (php 8.5.7). The low two bits are the quote` |
|      - |  972 | ` * bits (1 = single, 2 = double), so ENT_QUOTES = ENT_COMPAT\|1 and` |
|      - |  973 | ` * ENT_NOQUOTES = 0. Bits 16\|32 select the doctype (0 = HTML401, 16 = XML1,` |
|      - |  974 | ` * 32 = XHTML, 48 = HTML5) — composites, not flags.` |
|      - |  975 | ` */` |
|      - |  976 | `/*` |
|      - |  977 | ` * ENT_COMPAT` |
|      - |  978 | ` *  Expand 2 (double-quote bit only)` |
|      - |  979 | ` */` |
|     12 |  980 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  981 | `{` |
|      6 |  982 | `	SXUNUSED(pUserData); /* cc warning */` |
|     13 |  983 | `	ph7_value_int(pVal,PH7_ENT_QUOTE_DOUBLE);` |
|     13 |  984 | `}` |
|      - |  985 | `/*` |
|      - |  986 | ` * ENT_QUOTES` |
|      - |  987 | ` *  Expand 3 (double\|single quote bits)` |
|      - |  988 | ` */` |
|     60 |  989 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  990 | `{` |
|     30 |  991 | `	SXUNUSED(pUserData); /* cc warning */` |
|     61 |  992 | `	ph7_value_int(pVal,PH7_ENT_QUOTES);` |
|     61 |  993 | `}` |
|      - |  994 | `/*` |
|      - |  995 | ` * ENT_NOQUOTES` |
|      - |  996 | ` *  Expand 0 (no quote bits)` |
|      - |  997 | ` */` |
|     20 |  998 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  999 | `{` |
|     10 | 1000 | `	SXUNUSED(pUserData); /* cc warning */` |
|     21 | 1001 | `	ph7_value_int(pVal,0);` |
|     21 | 1002 | `}` |
|      - | 1003 | `/*` |
|      - | 1004 | ` * ENT_IGNORE` |
|      - | 1005 | ` *  Expand 4` |
|      - | 1006 | ` */` |
|      6 | 1007 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1008 | `{` |
|      3 | 1009 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1010 | `	ph7_value_int(pVal,PH7_ENT_IGNORE);` |
|      7 | 1011 | `}` |
|      - | 1012 | `/*` |
|      - | 1013 | ` * ENT_SUBSTITUTE` |
|      - | 1014 | ` *  Expand 8` |
|      - | 1015 | ` */` |
|      2 | 1016 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1017 | `{` |
|      1 | 1018 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1019 | `	ph7_value_int(pVal,PH7_ENT_SUBSTITUTE);` |
|      3 | 1020 | `}` |
|      - | 1021 | `/*` |
|      - | 1022 | ` * ENT_DISALLOWED` |
|      - | 1023 | ` *  Expand 128` |
|      - | 1024 | ` */` |
|      2 | 1025 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1026 | `{` |
|      1 | 1027 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1028 | `	ph7_value_int(pVal,PH7_ENT_DISALLOWED);` |
|      3 | 1029 | `}` |
|      - | 1030 | `/*` |
|      - | 1031 | ` * ENT_HTML401` |
|      - | 1032 | ` *  Expand 0 (the default doctype)` |
|      - | 1033 | ` */` |
|      2 | 1034 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1035 | `{` |
|      1 | 1036 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1037 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML401);` |
|      3 | 1038 | `}` |
|      - | 1039 | `/*` |
|      - | 1040 | ` * ENT_XML1` |
|      - | 1041 | ` *  Expand 16` |
|      - | 1042 | ` */` |
|      8 | 1043 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1044 | `{` |
|      4 | 1045 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 | 1046 | `	ph7_value_int(pVal,PH7_ENT_DOC_XML1);` |
|      9 | 1047 | `}` |
|      - | 1048 | `/*` |
|      - | 1049 | ` * ENT_XHTML` |
|      - | 1050 | ` *  Expand 32` |
|      - | 1051 | ` */` |
|      6 | 1052 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1053 | `{` |
|      3 | 1054 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1055 | `	ph7_value_int(pVal,PH7_ENT_DOC_XHTML);` |
|      7 | 1056 | `}` |
|      - | 1057 | `/*` |
|      - | 1058 | ` * ENT_HTML5` |
|      - | 1059 | ` *  Expand 48 (16\|32 — a doctype composite, not a flag bit)` |
|      - | 1060 | ` */` |
|      8 | 1061 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1062 | `{` |
|      4 | 1063 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 | 1064 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML5);` |
|      9 | 1065 | `}` |
|      - | 1066 | `/*` |
|      - | 1067 | ` * ISO-8859-1` |
|      - | 1068 | ` * ISO_8859_1` |
|      - | 1069 | ` *   Expand 1` |
|      - | 1070 | ` */` |
|      2 | 1071 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1072 | `{` |
|      1 | 1073 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1074 | `	ph7_value_int(pVal,1);` |
|      3 | 1075 | `}` |
|      - | 1076 | `/*` |
|      - | 1077 | ` * UTF-8` |
|      - | 1078 | ` * UTF8` |
|      - | 1079 | ` *  Expand 2` |
|      - | 1080 | ` */` |
|      2 | 1081 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1082 | `{` |
|      1 | 1083 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1084 | `	ph7_value_int(pVal,1);` |
|      3 | 1085 | `}` |
|      - | 1086 | `/*` |
|      - | 1087 | ` * HTML_ENTITIES` |
|      - | 1088 | ` *  Expand 1` |
|      - | 1089 | ` */` |
|      4 | 1090 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1091 | `{` |
|      2 | 1092 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1093 | `	ph7_value_int(pVal,1);` |
|      5 | 1094 | `}` |
|      - | 1095 | `/*` |
|      - | 1096 | ` * HTML_SPECIALCHARS` |
|      - | 1097 | ` *  Expand 0 (PHP-exact)` |
|      - | 1098 | ` */` |
|     10 | 1099 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1100 | `{` |
|      5 | 1101 | `	SXUNUSED(pUserData); /* cc warning */` |
|     11 | 1102 | `	ph7_value_int(pVal,0);` |
|     11 | 1103 | `}` |
|      - | 1104 | `/*` |
|      - | 1105 | ` * PHP_URL_SCHEME.` |
|      - | 1106 | ` * Expand 1` |
|      - | 1107 | ` */` |
|      2 | 1108 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1109 | `{` |
|      1 | 1110 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1111 | `	ph7_value_int(pVal,1);` |
|      3 | 1112 | `}` |
|      - | 1113 | `/*` |
|      - | 1114 | ` * PHP_URL_HOST.` |
|      - | 1115 | ` * Expand 2` |
|      - | 1116 | ` */` |
|      2 | 1117 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1118 | `{` |
|      1 | 1119 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1120 | `	ph7_value_int(pVal,2);` |
|      3 | 1121 | `}` |
|      - | 1122 | `/*` |
|      - | 1123 | ` * PHP_URL_PORT.` |
|      - | 1124 | ` * Expand 3` |
|      - | 1125 | ` */` |
|      2 | 1126 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1127 | `{` |
|      1 | 1128 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1129 | `	ph7_value_int(pVal,3);` |
|      3 | 1130 | `}` |
|      - | 1131 | `/*` |
|      - | 1132 | ` * PHP_URL_USER.` |
|      - | 1133 | ` * Expand 4` |
|      - | 1134 | ` */` |
|      2 | 1135 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1136 | `{` |
|      1 | 1137 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1138 | `	ph7_value_int(pVal,4);` |
|      3 | 1139 | `}` |
|      - | 1140 | `/*` |
|      - | 1141 | ` * PHP_URL_PASS.` |
|      - | 1142 | ` * Expand 5` |
|      - | 1143 | ` */` |
|      2 | 1144 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1145 | `{` |
|      1 | 1146 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1147 | `	ph7_value_int(pVal,5);` |
|      3 | 1148 | `}` |
|      - | 1149 | `/*` |
|      - | 1150 | ` * PHP_URL_PATH.` |
|      - | 1151 | ` * Expand 6` |
|      - | 1152 | ` */` |
|      2 | 1153 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1154 | `{` |
|      1 | 1155 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1156 | `	ph7_value_int(pVal,6);` |
|      3 | 1157 | `}` |
|      - | 1158 | `/*` |
|      - | 1159 | ` * PHP_URL_QUERY.` |
|      - | 1160 | ` * Expand 7` |
|      - | 1161 | ` */` |
|      2 | 1162 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1163 | `{` |
|      1 | 1164 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1165 | `	ph7_value_int(pVal,7);` |
|      3 | 1166 | `}` |
|      - | 1167 | `/*` |
|      - | 1168 | ` * PHP_URL_FRAGMENT.` |
|      - | 1169 | ` * Expand 8` |
|      - | 1170 | ` */` |
|      2 | 1171 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1172 | `{` |
|      1 | 1173 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1174 | `	ph7_value_int(pVal,8);` |
|      3 | 1175 | `}` |
|      - | 1176 | `/*` |
|      - | 1177 | ` * PHP_QUERY_RFC1738` |
|      - | 1178 | ` * Expand 1` |
|      - | 1179 | ` */` |
|     22 | 1180 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1181 | `{` |
|     11 | 1182 | `	SXUNUSED(pUserData); /* cc warning */` |
|     23 | 1183 | `	ph7_value_int(pVal,1);` |
|     23 | 1184 | `}` |
|      - | 1185 | `/*` |
|      - | 1186 | ` * PHP_QUERY_RFC3986` |
|      - | 1187 | ` * Expand 1` |
|      - | 1188 | ` */` |
|     96 | 1189 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1190 | `{` |
|     48 | 1191 | `	SXUNUSED(pUserData); /* cc warning */` |
|     97 | 1192 | `	ph7_value_int(pVal,2);` |
|     97 | 1193 | `}` |
|      - | 1194 | `/* php's FNM_* values (ext/standard): PATHNAME=1, NOESCAPE=2, PERIOD=4, CASEFOLD=16.` |
|      - | 1195 | ` * PHL previously had PATHNAME/NOESCAPE swapped and CASEFOLD=8; fnmatch() reads these` |
|      - | 1196 | ` * bits, so PH7_builtin_fnmatch was updated to the same values. */` |
|      - | 1197 | `/*` |
|      - | 1198 | ` * FNM_PATHNAME` |
|      - | 1199 | ` *  Expand 1 (php value)` |
|      - | 1200 | ` */` |
|    ! 0 | 1201 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 1202 | `{` |
|    ! 0 | 1203 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 1204 | `	ph7_value_int(pVal,1);` |
|    ! 0 | 1205 | `}` |
|      - | 1206 | `/*` |
|      - | 1207 | ` * FNM_NOESCAPE` |
|      - | 1208 | ` *  Expand 2 (php value)` |
|      - | 1209 | ` */` |
|    ! 0 | 1210 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 1211 | `{` |
|    ! 0 | 1212 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 1213 | `	ph7_value_int(pVal,2);` |
|    ! 0 | 1214 | `}` |
|      - | 1215 | `/*` |
|      - | 1216 | ` * FNM_PERIOD` |
|      - | 1217 | ` *  Expand 4 (php value)` |
|      - | 1218 | ` */` |
|      6 | 1219 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1220 | `{` |
|      3 | 1221 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1222 | `	ph7_value_int(pVal,4);` |
|      7 | 1223 | `}` |
|      - | 1224 | `/*` |
|      - | 1225 | ` * FNM_CASEFOLD` |
|      - | 1226 | ` *  Expand 16 (php value)` |
|      - | 1227 | ` */` |
|      4 | 1228 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1229 | `{` |
|      2 | 1230 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1231 | `	ph7_value_int(pVal,16);` |
|      5 | 1232 | `}` |
|      - | 1233 | `/*` |
|      - | 1234 | ` * PATHINFO_DIRNAME` |
|      - | 1235 | ` *  Expand 1.` |
|      - | 1236 | ` */` |
|      4 | 1237 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1238 | `{` |
|      2 | 1239 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1240 | `	ph7_value_int(pVal,1);` |
|      5 | 1241 | `}` |
|      - | 1242 | `/*` |
|      - | 1243 | ` * PATHINFO_BASENAME` |
|      - | 1244 | ` *  Expand 2.` |
|      - | 1245 | ` */` |
|      4 | 1246 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1247 | `{` |
|      2 | 1248 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1249 | `	ph7_value_int(pVal,2);` |
|      5 | 1250 | `}` |
|      - | 1251 | `/*` |
|      - | 1252 | ` * PATHINFO_EXTENSION` |
|      - | 1253 | ` *  Expand 3.` |
|      - | 1254 | ` */` |
|   6702 | 1255 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1256 | `{` |
|   3351 | 1257 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6707 | 1258 | `	ph7_value_int(pVal,3);` |
|   6707 | 1259 | `}` |
|      - | 1260 | `/*` |
|      - | 1261 | ` * PATHINFO_FILENAME` |
|      - | 1262 | ` *  Expand 4.` |
|      - | 1263 | ` */` |
|   6694 | 1264 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1265 | `{` |
|   3347 | 1266 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6699 | 1267 | `	ph7_value_int(pVal,4);` |
|   6699 | 1268 | `}` |
|      - | 1269 | `/*` |
|      - | 1270 | ` * ASSERT_ACTIVE.` |
|      - | 1271 | ` *  PHP ASSERT_ACTIVE = 1` |
|      - | 1272 | ` */` |
|     14 | 1273 | `static void PH7_ASSERT_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1274 | `{` |
|      7 | 1275 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1276 | `	ph7_value_int(pVal,1); /* PHP ASSERT_ACTIVE = 1 */` |
|     16 | 1277 | `}` |
|      - | 1278 | `/*` |
|      - | 1279 | ` * ASSERT_CALLBACK.` |
|      - | 1280 | ` *  PHP ASSERT_CALLBACK = 2` |
|      - | 1281 | ` */` |
|      6 | 1282 | `static void PH7_ASSERT_CALLBACK_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1283 | `{` |
|      3 | 1284 | `	SXUNUSED(pUserData); /* cc warning */` |
|      8 | 1285 | `	ph7_value_int(pVal,2); /* PHP ASSERT_CALLBACK = 2 */` |
|      8 | 1286 | `}` |
|      - | 1287 | `/*` |
|      - | 1288 | ` * ASSERT_BAIL.` |
|      - | 1289 | ` *  PHP ASSERT_BAIL = 3` |
|      - | 1290 | ` */` |
|     14 | 1291 | `static void PH7_ASSERT_BAIL_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1292 | `{` |
|      7 | 1293 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1294 | `	ph7_value_int(pVal,3); /* PHP ASSERT_BAIL = 3 */` |
|     16 | 1295 | `}` |
|      - | 1296 | `/*` |
|      - | 1297 | ` * ASSERT_WARNING.` |
|      - | 1298 | ` *  PHP ASSERT_WARNING = 4 (deprecated in PHP 8.3)` |
|      - | 1299 | ` */` |
|      4 | 1300 | `static void PH7_ASSERT_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1301 | `{` |
|      2 | 1302 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1303 | `	ph7_value_int(pVal,4); /* PHP ASSERT_WARNING = 4 */` |
|      5 | 1304 | `}` |
|      - | 1305 | `/*` |
|      - | 1306 | ` * ASSERT_EXCEPTION.` |
|      - | 1307 | ` *  PHP ASSERT_EXCEPTION = 5 (deprecated in PHP 8.3)` |
|      - | 1308 | ` */` |
|      4 | 1309 | `static void PH7_ASSERT_EXCEPTION_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1310 | `{` |
|      2 | 1311 | `	SXUNUSED(pUserData); /* cc warning */` |
|      6 | 1312 | `	ph7_value_int(pVal,5); /* PHP ASSERT_EXCEPTION = 5 */` |
|      6 | 1313 | `}` |
|      - | 1314 | `/*` |
|      - | 1315 | ` * SEEK_SET.` |
|      - | 1316 | ` *  Expand 0` |
|      - | 1317 | ` */` |
|      2 | 1318 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1319 | `{` |
|      1 | 1320 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1321 | `	ph7_value_int(pVal,0);` |
|      3 | 1322 | `}` |
|      - | 1323 | `/*` |
|      - | 1324 | ` * SEEK_CUR.` |
|      - | 1325 | ` *  Expand 1` |
|      - | 1326 | ` */` |
|      2 | 1327 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1328 | `{` |
|      1 | 1329 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1330 | `	ph7_value_int(pVal,1);` |
|      3 | 1331 | `}` |
|      - | 1332 | `/*` |
|      - | 1333 | ` * SEEK_END.` |
|      - | 1334 | ` *  Expand 2` |
|      - | 1335 | ` */` |
|      4 | 1336 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1337 | `{` |
|      2 | 1338 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1339 | `	ph7_value_int(pVal,2);` |
|      5 | 1340 | `}` |
|      - | 1341 | `/*` |
|      - | 1342 | ` * LOCK_SH.` |
|      - | 1343 | ` *  Expand 2` |
|      - | 1344 | ` */` |
|      2 | 1345 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1346 | `{` |
|      1 | 1347 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1348 | `	ph7_value_int(pVal,1);` |
|      3 | 1349 | `}` |
|      - | 1350 | `/*` |
|      - | 1351 | ` * LOCK_NB.` |
|      - | 1352 | ` *  Expand 4 (php)` |
|      - | 1353 | ` */` |
|      2 | 1354 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1355 | `{` |
|      1 | 1356 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1357 | `	ph7_value_int(pVal,4);` |
|      3 | 1358 | `}` |
|      - | 1359 | `/*` |
|      - | 1360 | ` * LOCK_EX.` |
|      - | 1361 | ` *  Expand 2 (php). PH7 used 1, which collided with LOCK_SH, and LOCK_UN was 0 — so` |
|      - | 1362 | ` *  flock($h, LOCK_UN) asked the stream for a SHARED lock instead of releasing one.` |
|      - | 1363 | ` */` |
|      4 | 1364 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1365 | `{` |
|      2 | 1366 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1367 | `	ph7_value_int(pVal,2);` |
|      5 | 1368 | `}` |
|      - | 1369 | `/*` |
|      - | 1370 | ` * LOCK_UN.` |
|      - | 1371 | ` *  Expand 3 (php)` |
|      - | 1372 | ` */` |
|      4 | 1373 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1374 | `{` |
|      2 | 1375 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1376 | `	ph7_value_int(pVal,3);` |
|      5 | 1377 | `}` |
|      - | 1378 | `/*` |
|      - | 1379 | ` * FILE_USE_INCLUDE_PATH` |
|      - | 1380 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1381 | ` */` |
|      2 | 1382 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1383 | `{` |
|      1 | 1384 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1385 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1386 | `}` |
|      - | 1387 | `/*` |
|      - | 1388 | ` * FILE_IGNORE_NEW_LINES` |
|      - | 1389 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1390 | ` */` |
|      2 | 1391 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1392 | `{` |
|      1 | 1393 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1394 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1395 | `}` |
|      - | 1396 | `/*` |
|      - | 1397 | ` * FILE_SKIP_EMPTY_LINES` |
|      - | 1398 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1399 | ` */` |
|      2 | 1400 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1401 | `{` |
|      1 | 1402 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1403 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1404 | `}` |
|      - | 1405 | `/*` |
|      - | 1406 | ` * FILE_APPEND` |
|      - | 1407 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1408 | ` */` |
|      2 | 1409 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1410 | `{` |
|      1 | 1411 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1412 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1413 | `}` |
|      - | 1414 | `/*` |
|      - | 1415 | ` * SCANDIR_SORT_ASCENDING` |
|      - | 1416 | ` *  Expand 0` |
|      - | 1417 | ` */` |
|   2102 | 1418 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1419 | `{` |
|   1051 | 1420 | `	SXUNUSED(pUserData); /* cc warning */` |
|   2107 | 1421 | `	ph7_value_int(pVal,0);` |
|   2107 | 1422 | `}` |
|      - | 1423 | `/*` |
|      - | 1424 | ` * SCANDIR_SORT_DESCENDING` |
|      - | 1425 | ` *  Expand 1` |
|      - | 1426 | ` */` |
|   1052 | 1427 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1428 | `{` |
|    526 | 1429 | `	SXUNUSED(pUserData); /* cc warning */` |
|   1057 | 1430 | `	ph7_value_int(pVal,1);` |
|   1057 | 1431 | `}` |
|      - | 1432 | `/*` |
|      - | 1433 | ` * SCANDIR_SORT_NONE` |
|      - | 1434 | ` *  Expand 2` |
|      - | 1435 | ` */` |
|      2 | 1436 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1437 | `{` |
|      1 | 1438 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1439 | `	ph7_value_int(pVal,2);` |
|      3 | 1440 | `}` |
|      - | 1441 | `/*` |
|      - | 1442 | ` * GLOB_MARK` |
|      - | 1443 | ` *  Expand 0x01 (must be a power of two)` |
|      - | 1444 | ` */` |
|     10 | 1445 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1446 | `{` |
|      5 | 1447 | `	SXUNUSED(pUserData); /* cc warning */` |
|     11 | 1448 | `	ph7_value_int(pVal,0x01);` |
|     11 | 1449 | `}` |
|      - | 1450 | `/*` |
|      - | 1451 | ` * GLOB_NOSORT` |
|      - | 1452 | ` *  Expand 0x02 (must be a power of two)` |
|      - | 1453 | ` */` |
|      8 | 1454 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1455 | `{` |
|      4 | 1456 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 | 1457 | `	ph7_value_int(pVal,0x02);` |
|      9 | 1458 | `}` |
|      - | 1459 | `/*` |
|      - | 1460 | ` * GLOB_NOCHECK` |
|      - | 1461 | ` *  Expand 0x04 (must be a power of two)` |
|      - | 1462 | ` */` |
|      8 | 1463 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1464 | `{` |
|      4 | 1465 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 | 1466 | `	ph7_value_int(pVal,0x04);` |
|      9 | 1467 | `}` |
|      - | 1468 | `/*` |
|      - | 1469 | ` * GLOB_NOESCAPE` |
|      - | 1470 | ` *  Expand 0x08 (must be a power of two)` |
|      - | 1471 | ` */` |
|      2 | 1472 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1473 | `{` |
|      1 | 1474 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1475 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1476 | `}` |
|      - | 1477 | `/*` |
|      - | 1478 | ` * GLOB_BRACE` |
|      - | 1479 | ` *  Expand 0x10 (must be a power of two)` |
|      - | 1480 | ` */` |
|      2 | 1481 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1482 | `{` |
|      1 | 1483 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1484 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1485 | `}` |
|      - | 1486 | `/*` |
|      - | 1487 | ` * GLOB_ONLYDIR` |
|      - | 1488 | ` *  Expand 0x20 (must be a power of two)` |
|      - | 1489 | ` */` |
|     14 | 1490 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1491 | `{` |
|      7 | 1492 | `	SXUNUSED(pUserData); /* cc warning */` |
|     15 | 1493 | `	ph7_value_int(pVal,0x20);` |
|     15 | 1494 | `}` |
|      - | 1495 | `/*` |
|      - | 1496 | ` * GLOB_ERR` |
|      - | 1497 | ` *  Expand 0x40 (must be a power of two)` |
|      - | 1498 | ` */` |
|      2 | 1499 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1500 | `{` |
|      1 | 1501 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1502 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1503 | `}` |
|      - | 1504 | `/*` |
|      - | 1505 | ` * STDIN` |
|      - | 1506 | ` *  Expand the STDIN handle as a resource.` |
|      - | 1507 | ` */` |
|      2 | 1508 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1509 | `{` |
|      3 | 1510 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1511 | `	void *pResource;` |
|      3 | 1512 | `	pResource = PH7_ExportStdin(pVm);` |
|      3 | 1513 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1514 | `}` |
|      - | 1515 | `/*` |
|      - | 1516 | ` * STDOUT` |
|      - | 1517 | ` *   Expand the STDOUT handle as a resource.` |
|      - | 1518 | ` */` |
|      8 | 1519 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1520 | `{` |
|      9 | 1521 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1522 | `	void *pResource;` |
|      9 | 1523 | `	pResource = PH7_ExportStdout(pVm);` |
|      9 | 1524 | `	ph7_value_resource(pVal,pResource);` |
|      9 | 1525 | `}` |
|      - | 1526 | `/*` |
|      - | 1527 | ` * STDERR` |
|      - | 1528 | ` *  Expand the STDERR handle as a resource.` |
|      - | 1529 | ` */` |
|     10 | 1530 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1531 | `{` |
|     11 | 1532 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1533 | `	void *pResource;` |
|     11 | 1534 | `	pResource = PH7_ExportStderr(pVm);` |
|     11 | 1535 | `	ph7_value_resource(pVal,pResource);` |
|     11 | 1536 | `}` |
|      - | 1537 | `/*` |
|      - | 1538 | ` * INI_SCANNER_NORMAL` |
|      - | 1539 | ` *   Expand 1` |
|      - | 1540 | ` */` |
|      2 | 1541 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1542 | `{` |
|      1 | 1543 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1544 | `	ph7_value_int(pVal,1);` |
|      3 | 1545 | `}` |
|      - | 1546 | `/*` |
|      - | 1547 | ` * INI_SCANNER_RAW` |
|      - | 1548 | ` *   Expand 2` |
|      - | 1549 | ` */` |
|      2 | 1550 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1551 | `{` |
|      1 | 1552 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1553 | `	ph7_value_int(pVal,2);` |
|      3 | 1554 | `}` |
|      - | 1555 | `/*` |
|      - | 1556 | ` * EXTR_OVERWRITE` |
|      - | 1557 | ` *   Expand 0x01 (Must be a power of two)` |
|      - | 1558 | ` */` |
|      2 | 1559 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1560 | `{` |
|      1 | 1561 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1562 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1563 | `}` |
|      - | 1564 | `/*` |
|      - | 1565 | ` * EXTR_SKIP` |
|      - | 1566 | ` *   Expand 0x02 (Must be a power of two)` |
|      - | 1567 | ` */` |
|      2 | 1568 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1569 | `{` |
|      1 | 1570 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1571 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1572 | `}` |
|      - | 1573 | `/*` |
|      - | 1574 | ` * EXTR_PREFIX_SAME` |
|      - | 1575 | ` *   Expand 0x04 (Must be a power of two)` |
|      - | 1576 | ` */` |
|      2 | 1577 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1578 | `{` |
|      1 | 1579 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1580 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1581 | `}` |
|      - | 1582 | `/*` |
|      - | 1583 | ` * EXTR_PREFIX_ALL` |
|      - | 1584 | ` *   Expand 0x08 (Must be a power of two)` |
|      - | 1585 | ` */` |
|      2 | 1586 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1587 | `{` |
|      1 | 1588 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1589 | `	ph7_value_int(pVal,0x8);` |
|      3 | 1590 | `}` |
|      - | 1591 | `/*` |
|      - | 1592 | ` * EXTR_PREFIX_INVALID` |
|      - | 1593 | ` *   Expand 0x10 (Must be a power of two)` |
|      - | 1594 | ` */` |
|      2 | 1595 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1596 | `{` |
|      1 | 1597 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1598 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1599 | `}` |
|      - | 1600 | `/*` |
|      - | 1601 | ` * EXTR_IF_EXISTS` |
|      - | 1602 | ` *   Expand 0x20 (Must be a power of two)` |
|      - | 1603 | ` */` |
|      2 | 1604 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1605 | `{` |
|      1 | 1606 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1607 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1608 | `}` |
|      - | 1609 | `/*` |
|      - | 1610 | ` * EXTR_PREFIX_IF_EXISTS` |
|      - | 1611 | ` *   Expand 0x40 (Must be a power of two)` |
|      - | 1612 | ` */` |
|      2 | 1613 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1614 | `{` |
|      1 | 1615 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1616 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1617 | `}` |
|      - | 1618 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1619 | `/*` |
|      - | 1620 | ` * XML_ERROR_NONE` |
|      - | 1621 | ` *   Expand the value of SXML_ERROR_NO_MEMORY defined in ph7Int.h` |
|      - | 1622 | ` */` |
|      2 | 1623 | `static void PH7_XML_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1624 | `{` |
|      1 | 1625 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1626 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1627 | `}` |
|      - | 1628 | `/*` |
|      - | 1629 | ` * XML_ERROR_NO_MEMORY` |
|      - | 1630 | ` *   Expand the value of SXML_ERROR_NONE defined in ph7Int.h` |
|      - | 1631 | ` */` |
|      2 | 1632 | `static void PH7_XML_ERROR_NO_MEMORY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1633 | `{` |
|      1 | 1634 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1635 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1636 | `}` |
|      - | 1637 | `/*` |
|      - | 1638 | ` * XML_ERROR_SYNTAX` |
|      - | 1639 | ` *   Expand the value of SXML_ERROR_SYNTAX defined in ph7Int.h` |
|      - | 1640 | ` */` |
|      2 | 1641 | `static void PH7_XML_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1642 | `{` |
|      1 | 1643 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1644 | `	ph7_value_int(pVal,SXML_ERROR_SYNTAX);` |
|      3 | 1645 | `}` |
|      - | 1646 | `/*` |
|      - | 1647 | ` * XML_ERROR_NO_ELEMENTS` |
|      - | 1648 | ` *   Expand the value of SXML_ERROR_NO_ELEMENTS defined in ph7Int.h` |
|      - | 1649 | ` */` |
|      2 | 1650 | `static void PH7_XML_ERROR_NO_ELEMENTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1651 | `{` |
|      1 | 1652 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1653 | `	ph7_value_int(pVal,SXML_ERROR_NO_ELEMENTS);` |
|      3 | 1654 | `}` |
|      - | 1655 | `/*` |
|      - | 1656 | ` * XML_ERROR_INVALID_TOKEN` |
|      - | 1657 | ` *   Expand the value of SXML_ERROR_INVALID_TOKEN defined in ph7Int.h` |
|      - | 1658 | ` */` |
|      2 | 1659 | `static void PH7_XML_ERROR_INVALID_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1660 | `{` |
|      1 | 1661 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1662 | `	ph7_value_int(pVal,SXML_ERROR_INVALID_TOKEN);` |
|      3 | 1663 | `}` |
|      - | 1664 | `/*` |
|      - | 1665 | ` * XML_ERROR_UNCLOSED_TOKEN` |
|      - | 1666 | ` *   Expand the value of SXML_ERROR_UNCLOSED_TOKEN defined in ph7Int.h` |
|      - | 1667 | ` */` |
|      2 | 1668 | `static void PH7_XML_ERROR_UNCLOSED_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1669 | `{` |
|      1 | 1670 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1671 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_TOKEN);` |
|      3 | 1672 | `}` |
|      - | 1673 | `/*` |
|      - | 1674 | ` * XML_ERROR_PARTIAL_CHAR` |
|      - | 1675 | ` *   Expand the value of SXML_ERROR_PARTIAL_CHAR defined in ph7Int.h` |
|      - | 1676 | ` */` |
|      2 | 1677 | `static void PH7_XML_ERROR_PARTIAL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1678 | `{` |
|      1 | 1679 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1680 | `	ph7_value_int(pVal,SXML_ERROR_PARTIAL_CHAR);` |
|      3 | 1681 | `}` |
|      - | 1682 | `/*` |
|      - | 1683 | ` * XML_ERROR_TAG_MISMATCH` |
|      - | 1684 | ` *   Expand the value of SXML_ERROR_TAG_MISMATCH defined in ph7Int.h` |
|      - | 1685 | ` */` |
|      2 | 1686 | `static void PH7_XML_ERROR_TAG_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1687 | `{` |
|      1 | 1688 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1689 | `	ph7_value_int(pVal,SXML_ERROR_TAG_MISMATCH);` |
|      3 | 1690 | `}` |
|      - | 1691 | `/*` |
|      - | 1692 | ` * XML_ERROR_DUPLICATE_ATTRIBUTE` |
|      - | 1693 | ` *   Expand the value of SXML_ERROR_DUPLICATE_ATTRIBUTE defined in ph7Int.h` |
|      - | 1694 | ` */` |
|      2 | 1695 | `static void PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1696 | `{` |
|      1 | 1697 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1698 | `	ph7_value_int(pVal,SXML_ERROR_DUPLICATE_ATTRIBUTE);` |
|      3 | 1699 | `}` |
|      - | 1700 | `/*` |
|      - | 1701 | ` * XML_ERROR_JUNK_AFTER_DOC_ELEMENT` |
|      - | 1702 | ` *   Expand the value of SXML_ERROR_JUNK_AFTER_DOC_ELEMENT defined in ph7Int.h` |
|      - | 1703 | ` */` |
|      2 | 1704 | `static void PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1705 | `{` |
|      1 | 1706 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1707 | `	ph7_value_int(pVal,SXML_ERROR_JUNK_AFTER_DOC_ELEMENT);` |
|      3 | 1708 | `}` |
|      - | 1709 | `/*` |
|      - | 1710 | ` * XML_ERROR_PARAM_ENTITY_REF` |
|      - | 1711 | ` *   Expand the value of SXML_ERROR_PARAM_ENTITY_REF defined in ph7Int.h` |
|      - | 1712 | ` */` |
|      2 | 1713 | `static void PH7_XML_ERROR_PARAM_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1714 | `{` |
|      1 | 1715 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1716 | `	ph7_value_int(pVal,SXML_ERROR_PARAM_ENTITY_REF);` |
|      3 | 1717 | `}` |
|      - | 1718 | `/*` |
|      - | 1719 | ` * XML_ERROR_UNDEFINED_ENTITY` |
|      - | 1720 | ` *   Expand the value of SXML_ERROR_UNDEFINED_ENTITY defined in ph7Int.h` |
|      - | 1721 | ` */` |
|      2 | 1722 | `static void PH7_XML_ERROR_UNDEFINED_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1723 | `{` |
|      1 | 1724 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1725 | `	ph7_value_int(pVal,SXML_ERROR_UNDEFINED_ENTITY);` |
|      3 | 1726 | `}` |
|      - | 1727 | `/*` |
|      - | 1728 | ` * XML_ERROR_RECURSIVE_ENTITY_REF` |
|      - | 1729 | ` *   Expand the value of SXML_ERROR_RECURSIVE_ENTITY_REF defined in ph7Int.h` |
|      - | 1730 | ` */` |
|      2 | 1731 | `static void PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1732 | `{` |
|      1 | 1733 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1734 | `	ph7_value_int(pVal,SXML_ERROR_RECURSIVE_ENTITY_REF);` |
|      3 | 1735 | `}` |
|      - | 1736 | `/*` |
|      - | 1737 | ` * XML_ERROR_ASYNC_ENTITY` |
|      - | 1738 | ` *   Expand the value of SXML_ERROR_ASYNC_ENTITY defined in ph7Int.h` |
|      - | 1739 | ` */` |
|      2 | 1740 | `static void PH7_XML_ERROR_ASYNC_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1741 | `{` |
|      1 | 1742 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1743 | `	ph7_value_int(pVal,SXML_ERROR_ASYNC_ENTITY);` |
|      3 | 1744 | `}` |
|      - | 1745 | `/*` |
|      - | 1746 | ` * XML_ERROR_BAD_CHAR_REF` |
|      - | 1747 | ` *   Expand the value of SXML_ERROR_BAD_CHAR_REF defined in ph7Int.h` |
|      - | 1748 | ` */` |
|      2 | 1749 | `static void PH7_XML_ERROR_BAD_CHAR_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1750 | `{` |
|      1 | 1751 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1752 | `	ph7_value_int(pVal,SXML_ERROR_BAD_CHAR_REF);` |
|      3 | 1753 | `}` |
|      - | 1754 | `/*` |
|      - | 1755 | ` * XML_ERROR_BINARY_ENTITY_REF` |
|      - | 1756 | ` *   Expand the value of SXML_ERROR_BINARY_ENTITY_REF defined in ph7Int.h` |
|      - | 1757 | ` */` |
|      2 | 1758 | `static void PH7_XML_ERROR_BINARY_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1759 | `{` |
|      1 | 1760 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1761 | `	ph7_value_int(pVal,SXML_ERROR_BINARY_ENTITY_REF);` |
|      3 | 1762 | `}` |
|      - | 1763 | `/*` |
|      - | 1764 | ` * XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF` |
|      - | 1765 | ` *   Expand the value of SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF defined in ph7Int.h` |
|      - | 1766 | ` */` |
|      2 | 1767 | `static void PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1768 | `{` |
|      1 | 1769 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1770 | `	ph7_value_int(pVal,SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF);` |
|      3 | 1771 | `}` |
|      - | 1772 | `/*` |
|      - | 1773 | ` * XML_ERROR_MISPLACED_XML_PI` |
|      - | 1774 | ` *   Expand the value of SXML_ERROR_MISPLACED_XML_PI defined in ph7Int.h` |
|      - | 1775 | ` */` |
|      2 | 1776 | `static void PH7_XML_ERROR_MISPLACED_XML_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1777 | `{` |
|      1 | 1778 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1779 | `	ph7_value_int(pVal,SXML_ERROR_MISPLACED_XML_PI);` |
|      3 | 1780 | `}` |
|      - | 1781 | `/*` |
|      - | 1782 | ` * XML_ERROR_UNKNOWN_ENCODING` |
|      - | 1783 | ` *   Expand the value of SXML_ERROR_UNKNOWN_ENCODING defined in ph7Int.h` |
|      - | 1784 | ` */` |
|      2 | 1785 | `static void PH7_XML_ERROR_UNKNOWN_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1786 | `{` |
|      1 | 1787 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1788 | `	ph7_value_int(pVal,SXML_ERROR_UNKNOWN_ENCODING);` |
|      3 | 1789 | `}` |
|      - | 1790 | `/*` |
|      - | 1791 | ` * XML_ERROR_INCORRECT_ENCODING` |
|      - | 1792 | ` *   Expand the value of SXML_ERROR_INCORRECT_ENCODING defined in ph7Int.h` |
|      - | 1793 | ` */` |
|      2 | 1794 | `static void PH7_XML_ERROR_INCORRECT_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1795 | `{` |
|      1 | 1796 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1797 | `	ph7_value_int(pVal,SXML_ERROR_INCORRECT_ENCODING);` |
|      3 | 1798 | `}` |
|      - | 1799 | `/*` |
|      - | 1800 | ` * XML_ERROR_UNCLOSED_CDATA_SECTION` |
|      - | 1801 | ` *   Expand the value of SXML_ERROR_UNCLOSED_CDATA_SECTION defined in ph7Int.h` |
|      - | 1802 | ` */` |
|      2 | 1803 | `static void PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1804 | `{` |
|      1 | 1805 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1806 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_CDATA_SECTION);` |
|      3 | 1807 | `}` |
|      - | 1808 | `/*` |
|      - | 1809 | ` * XML_ERROR_EXTERNAL_ENTITY_HANDLING` |
|      - | 1810 | ` *   Expand the value of SXML_ERROR_EXTERNAL_ENTITY_HANDLING defined in ph7Int.h` |
|      - | 1811 | ` */` |
|      2 | 1812 | `static void PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1813 | `{` |
|      1 | 1814 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1815 | `	ph7_value_int(pVal,SXML_ERROR_EXTERNAL_ENTITY_HANDLING);` |
|      3 | 1816 | `}` |
|      - | 1817 | `/*` |
|      - | 1818 | ` * XML_OPTION_CASE_FOLDING` |
|      - | 1819 | ` *   Expand the value of SXML_OPTION_CASE_FOLDING defined in ph7Int.h.` |
|      - | 1820 | ` */` |
|      2 | 1821 | `static void PH7_XML_OPTION_CASE_FOLDING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1822 | `{` |
|      1 | 1823 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1824 | `	ph7_value_int(pVal,SXML_OPTION_CASE_FOLDING);` |
|      3 | 1825 | `}` |
|      - | 1826 | `/*` |
|      - | 1827 | ` * XML_OPTION_TARGET_ENCODING` |
|      - | 1828 | ` *   Expand the value of SXML_OPTION_TARGET_ENCODING defined in ph7Int.h.` |
|      - | 1829 | ` */` |
|      4 | 1830 | `static void PH7_XML_OPTION_TARGET_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1831 | `{` |
|      2 | 1832 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1833 | `	ph7_value_int(pVal,SXML_OPTION_TARGET_ENCODING);` |
|      5 | 1834 | `}` |
|      - | 1835 | `/*` |
|      - | 1836 | ` * XML_OPTION_SKIP_TAGSTART` |
|      - | 1837 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1838 | ` */` |
|      2 | 1839 | `static void PH7_XML_OPTION_SKIP_TAGSTART_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1840 | `{` |
|      1 | 1841 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1842 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_TAGSTART);` |
|      3 | 1843 | `}` |
|      - | 1844 | `/*` |
|      - | 1845 | ` * XML_OPTION_SKIP_WHITE` |
|      - | 1846 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1847 | ` */` |
|      4 | 1848 | `static void PH7_XML_OPTION_SKIP_WHITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1849 | `{` |
|      2 | 1850 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1851 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_WHITE);` |
|      5 | 1852 | `}` |
|      - | 1853 | `/*` |
|      - | 1854 | ` * XML_SAX_IMPL.` |
|      - | 1855 | ` *   Expand the name of the underlying XML engine.` |
|      - | 1856 | ` */` |
|      2 | 1857 | `static void PH7_XML_SAX_IMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1858 | `{` |
|      1 | 1859 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1860 | `	ph7_value_string(pVal,"Symisc XML engine",(int)sizeof("Symisc XML engine")-1);` |
|      3 | 1861 | `}` |
|      - | 1862 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1863 | `/*` |
|      - | 1864 | ` * JSON_HEX_TAG.` |
|      - | 1865 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|      - | 1866 | ` */` |
|      2 | 1867 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1868 | `{` |
|      1 | 1869 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1870 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      3 | 1871 | `}` |
|      - | 1872 | `/*` |
|      - | 1873 | ` * JSON_HEX_AMP.` |
|      - | 1874 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|      - | 1875 | ` */` |
|      2 | 1876 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1877 | `{` |
|      1 | 1878 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1879 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      3 | 1880 | `}` |
|      - | 1881 | `/*` |
|      - | 1882 | ` * JSON_HEX_APOS.` |
|      - | 1883 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|      - | 1884 | ` */` |
|      2 | 1885 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1886 | `{` |
|      1 | 1887 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1888 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      3 | 1889 | `}` |
|      - | 1890 | `/*` |
|      - | 1891 | ` * JSON_HEX_QUOT.` |
|      - | 1892 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|      - | 1893 | ` */` |
|      2 | 1894 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1895 | `{` |
|      1 | 1896 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1897 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      3 | 1898 | `}` |
|      - | 1899 | `/*` |
|      - | 1900 | ` * JSON_FORCE_OBJECT.` |
|      - | 1901 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|      - | 1902 | ` */` |
|      4 | 1903 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1904 | `{` |
|      2 | 1905 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1906 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      5 | 1907 | `}` |
|      - | 1908 | `/*` |
|      - | 1909 | ` * JSON_NUMERIC_CHECK.` |
|      - | 1910 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|      - | 1911 | ` */` |
|      4 | 1912 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1913 | `{` |
|      2 | 1914 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1915 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      5 | 1916 | `}` |
|      - | 1917 | `/*` |
|      - | 1918 | ` * JSON_BIGINT_AS_STRING.` |
|      - | 1919 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|      - | 1920 | ` */` |
|      2 | 1921 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1922 | `{` |
|      1 | 1923 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1924 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      3 | 1925 | `}` |
|      - | 1926 | `/*` |
|      - | 1927 | ` * JSON_PRETTY_PRINT.` |
|      - | 1928 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|      - | 1929 | ` */` |
|      2 | 1930 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1931 | `{` |
|      1 | 1932 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1933 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      3 | 1934 | `}` |
|      - | 1935 | `/*` |
|      - | 1936 | ` * JSON_UNESCAPED_SLASHES.` |
|      - | 1937 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|      - | 1938 | ` */` |
|      4 | 1939 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1940 | `{` |
|      2 | 1941 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1942 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      5 | 1943 | `}` |
|      - | 1944 | `/*` |
|      - | 1945 | ` * JSON_UNESCAPED_UNICODE.` |
|      - | 1946 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|      - | 1947 | ` */` |
|      2 | 1948 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1949 | `{` |
|      1 | 1950 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1951 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|      3 | 1952 | `}` |
|      - | 1953 | `/*` |
|      - | 1954 | ` * JSON_THROW_ON_ERROR.` |
|      - | 1955 | ` *   Expand the value of JSON_THROW_ON_ERROR defined in ph7Int.h.` |
|      - | 1956 | ` */` |
|      8 | 1957 | `static void PH7_JSON_THROW_ON_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1958 | `{` |
|      4 | 1959 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 | 1960 | `	ph7_value_int(pVal,JSON_THROW_ON_ERROR);` |
|      9 | 1961 | `}` |
|      - | 1962 | `/*` |
|      - | 1963 | ` * JSON_ERROR_NONE.` |
|      - | 1964 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|      - | 1965 | ` */` |
|      4 | 1966 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1967 | `{` |
|      2 | 1968 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1969 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      5 | 1970 | `}` |
|      - | 1971 | `/*` |
|      - | 1972 | ` * JSON_ERROR_DEPTH.` |
|      - | 1973 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|      - | 1974 | ` */` |
|      2 | 1975 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1976 | `{` |
|      1 | 1977 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1978 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      3 | 1979 | `}` |
|      - | 1980 | `/*` |
|      - | 1981 | ` * JSON_ERROR_STATE_MISMATCH.` |
|      - | 1982 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|      - | 1983 | ` */` |
|      2 | 1984 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1985 | `{` |
|      1 | 1986 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1987 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      3 | 1988 | `}` |
|      - | 1989 | `/*` |
|      - | 1990 | ` * JSON_ERROR_CTRL_CHAR.` |
|      - | 1991 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|      - | 1992 | ` */` |
|      2 | 1993 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1994 | `{` |
|      1 | 1995 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1996 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      3 | 1997 | `}` |
|      - | 1998 | `/*` |
|      - | 1999 | ` * JSON_ERROR_SYNTAX.` |
|      - | 2000 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|      - | 2001 | ` */` |
|      4 | 2002 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 2003 | `{` |
|      2 | 2004 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 2005 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      5 | 2006 | `}` |
|      - | 2007 | `/*` |
|      - | 2008 | ` * JSON_ERROR_UTF8.` |
|      - | 2009 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|      - | 2010 | ` */` |
|      2 | 2011 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 2012 | `{` |
|      1 | 2013 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 2014 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      3 | 2015 | `}` |
|      - | 2016 | `/*` |
|      - | 2017 | ` * JSON_ERROR_NON_BACKED_ENUM.` |
|      - | 2018 | ` *   Expand the value of JSON_ERROR_NON_BACKED_ENUM defined in ph7Int.h (php 8.1).` |
|      - | 2019 | ` */` |
|    ! 0 | 2020 | `static void PH7_JSON_ERROR_NON_BACKED_ENUM_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 2021 | `{` |
|    ! 0 | 2022 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 2023 | `	ph7_value_int(pVal,JSON_ERROR_NON_BACKED_ENUM);` |
|    ! 0 | 2024 | `}` |
|      - | 2025 | `/*` |
|      - | 2026 | ` * JSON_ERROR_INF_OR_NAN.` |
|      - | 2027 | ` *   Expand the value of JSON_ERROR_INF_OR_NAN defined in ph7Int.h.` |
|      - | 2028 | ` */` |
|      2 | 2029 | `static void PH7_JSON_ERROR_INF_OR_NAN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 2030 | `{` |
|      1 | 2031 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 2032 | `	ph7_value_int(pVal,JSON_ERROR_INF_OR_NAN);` |
|      3 | 2033 | `}` |
|      - | 2034 | `/*` |
|      - | 2035 | ` * __CLASS__` |
|      - | 2036 | ` *  The current class name, or the EMPTY STRING outside any class — php answers "",` |
|      - | 2037 | `` *  not null (`__CLASS__ === ""` is true in global scope). `self` keeps its own`` |
|      - | 2038 | ` *  expander below because php treats IT differently outside a class scope.` |
|      - | 2039 | ` */` |
|      2 | 2040 | `static void PH7_class_magic_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 2041 | `{` |
|      3 | 2042 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 2043 | `	ph7_class *pClass;` |
|      3 | 2044 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      3 | 2045 | `	if( pClass == 0 ){` |
|      3 | 2046 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      1 | 2047 | `	}` |
|      3 | 2048 | `	if( pClass ){` |
|    ! 0 | 2049 | `		SyString *pName = &pClass->sName;` |
|    ! 0 | 2050 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|    ! 0 | 2051 | `	}else{` |
|      3 | 2052 | `		ph7_value_string(pVal,"",0);` |
|      - | 2053 | `	}` |
|      3 | 2054 | `}` |
|      - | 2055 |  |
|      - | 2056 | `/*` |
|      - | 2057 | ` * PASSWORD_BCRYPT / PASSWORD_DEFAULT` |
|      - | 2058 | ` *  The bcrypt algorithm identifier (PHP 7.4+ exposes these as the string "2y").` |
|      - | 2059 | ` *  PASSWORD_DEFAULT tracks the recommended default, currently bcrypt.` |
|      - | 2060 | ` */` |
|     20 | 2061 | `static void PH7_PASSWORD_BCRYPT_Const(ph7_value *pVal,void *pUnused)` |
|      2 | 2062 | `{` |
|     10 | 2063 | `	SXUNUSED(pUnused);` |
|     22 | 2064 | `	ph7_value_string(pVal,"2y",(int)sizeof("2y")-1);` |
|     22 | 2065 | `}` |
|      - | 2066 | `/*` |
|      - | 2067 | ` * PASSWORD_BCRYPT_DEFAULT_COST` |
|      - | 2068 | ` *  The default bcrypt work factor used by password_hash() (currently 12).` |
|      - | 2069 | ` */` |
|      2 | 2070 | `static void PH7_PASSWORD_COST_Const(ph7_value *pVal,void *pUnused)` |
|      1 | 2071 | `{` |
|      1 | 2072 | `	SXUNUSED(pUnused);` |
|      3 | 2073 | `	ph7_value_int(pVal,12);` |
|      3 | 2074 | `}` |
|      - | 2075 | `/*` |
|      - | 2076 | ` * filter_var() filter and flag identifiers (the ext/filter constants). Values` |
|      - | 2077 | ` * match PHP 8.5. One tiny int-returning callback per constant, generated by a` |
|      - | 2078 | ` * local macro to keep the ~25 near-identical definitions DRY.` |
|      - | 2079 | ` */` |
|      - | 2080 | `#define PH7_FILTER_INT_CONST(Name,Val) \` |
|      - | 2081 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|      - | 2082 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|      - | 2083 | `	}` |
|     10 | 2084 | `PH7_FILTER_INT_CONST(FILTER_DEFAULT,516)` |
|     17 | 2085 | `PH7_FILTER_INT_CONST(FILTER_UNSAFE_RAW,516)` |
|     64 | 2086 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_INT,257)` |
|     29 | 2087 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_BOOLEAN,258)` |
|     69 | 2088 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_FLOAT,259)` |
|      8 | 2089 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_REGEXP,272)` |
|     11 | 2090 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_DOMAIN,277)` |
|     15 | 2091 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_URL,273)` |
|     28 | 2092 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_EMAIL,274)` |
|     25 | 2093 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_IP,275)` |
|     11 | 2094 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_MAC,276)` |
|      3 | 2095 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_INT,519)` |
|      5 | 2096 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_FLOAT,520)` |
|     13 | 2097 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_SPECIAL_CHARS,515)` |
|     25 | 2098 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_FULL_SPECIAL_CHARS,522)` |
|      3 | 2099 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_EMAIL,517)` |
|      3 | 2100 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_URL,518)` |
|      3 | 2101 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_OCTAL,1)` |
|      3 | 2102 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_HEX,2)` |
|      7 | 2103 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_LOW,4)` |
|      5 | 2104 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_HIGH,8)` |
|      5 | 2105 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_LOW,16)` |
|      5 | 2106 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_HIGH,32)` |
|      3 | 2107 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_AMP,64)` |
|      3 | 2108 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_ENCODE_QUOTES,128)` |
|      3 | 2109 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_BACKTICK,512)` |
|      3 | 2110 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_FRACTION,4096)` |
|     25 | 2111 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_THOUSAND,8192)` |
|      3 | 2112 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_SCIENTIFIC,16384)` |
|      5 | 2113 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV4,1048576)` |
|      3 | 2114 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV6,2097152)` |
|     14 | 2115 | `PH7_FILTER_INT_CONST(FILTER_NULL_ON_FAILURE,134217728)` |
|      - | 2116 | `/* filter_input() source selectors (php values; SESSION/REQUEST are undefined in 8.5) */` |
|      5 | 2117 | `PH7_FILTER_INT_CONST(INPUT_POST,0)` |
|      8 | 2118 | `PH7_FILTER_INT_CONST(INPUT_GET,1)` |
|      3 | 2119 | `PH7_FILTER_INT_CONST(INPUT_COOKIE,2)` |
|      3 | 2120 | `PH7_FILTER_INT_CONST(INPUT_ENV,4)` |
|     21 | 2121 | `PH7_FILTER_INT_CONST(INPUT_SERVER,5)` |
|      - | 2122 | `/*` |
|      - | 2123 | ` * Table of built-in constants.` |
|      - | 2124 | ` */` |
|      - | 2125 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|      - | 2126 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|      - | 2127 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|      - | 2128 | `	{"__PH7__",              PH7_VER_Const      },` |
|      - | 2129 | `	{"PHP_VERSION",          PH7_PHPVerConst    },` |
|      - | 2130 | `	{"PHP_MAJOR_VERSION",    PH7_PHPMajorConst  },` |
|      - | 2131 | `	{"PHP_MINOR_VERSION",    PH7_PHPMinorConst  },` |
|      - | 2132 | `	{"PHP_RELEASE_VERSION",  PH7_PHPReleaseConst},` |
|      - | 2133 | `	{"PHP_EXTRA_VERSION",    PH7_PHPExtraConst  },` |
|      - | 2134 | `	{"PHP_VERSION_ID",       PH7_PHPVerIdConst  },` |
|      - | 2135 | `	{"PHP_OS",               PH7_OS_Const       },` |
|      - | 2136 | `	{"PHP_OS_FAMILY",        PH7_OS_FAMILY_Const},` |
|      - | 2137 | `	{"PHP_SAPI",             PH7_SAPI_Const     },` |
|      - | 2138 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|      - | 2139 | `	{"PHP_SESSION_DISABLED", PH7_PHP_SESSION_DISABLED_Const },` |
|      - | 2140 | `	{"PHP_SESSION_NONE",     PH7_PHP_SESSION_NONE_Const },` |
|      - | 2141 | `	{"PHP_SESSION_ACTIVE",   PH7_PHP_SESSION_ACTIVE_Const },` |
|      - | 2142 | `	{"INI_USER",             PH7_INI_USER_Const },` |
|      - | 2143 | `	{"INI_PERDIR",           PH7_INI_PERDIR_Const },` |
|      - | 2144 | `	{"INI_SYSTEM",           PH7_INI_SYSTEM_Const },` |
|      - | 2145 | `	{"INI_ALL",              PH7_INI_ALL_Const },` |
|      - | 2146 | `	{"MB_CASE_UPPER",        PH7_MB_CASE_UPPER_Const },` |
|      - | 2147 | `	{"MB_CASE_LOWER",        PH7_MB_CASE_LOWER_Const },` |
|      - | 2148 | `	{"MB_CASE_TITLE",        PH7_MB_CASE_TITLE_Const },` |
|      - | 2149 | `	{"PASSWORD_BCRYPT",      PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2150 | `	{"PASSWORD_DEFAULT",     PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2151 | `	{"PASSWORD_BCRYPT_DEFAULT_COST", PH7_PASSWORD_COST_Const },` |
|      - | 2152 | `	{"FILTER_DEFAULT",              PH7_FILTER_DEFAULT_Const },` |
|      - | 2153 | `	{"FILTER_UNSAFE_RAW",           PH7_FILTER_UNSAFE_RAW_Const },` |
|      - | 2154 | `	{"FILTER_VALIDATE_INT",         PH7_FILTER_VALIDATE_INT_Const },` |
|      - | 2155 | `	{"FILTER_VALIDATE_BOOLEAN",     PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2156 | `	{"FILTER_VALIDATE_BOOL",        PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2157 | `	{"FILTER_VALIDATE_FLOAT",       PH7_FILTER_VALIDATE_FLOAT_Const },` |
|      - | 2158 | `	{"FILTER_VALIDATE_REGEXP",      PH7_FILTER_VALIDATE_REGEXP_Const },` |
|      - | 2159 | `	{"FILTER_VALIDATE_DOMAIN",      PH7_FILTER_VALIDATE_DOMAIN_Const },` |
|      - | 2160 | `	{"FILTER_VALIDATE_URL",         PH7_FILTER_VALIDATE_URL_Const },` |
|      - | 2161 | `	{"FILTER_VALIDATE_EMAIL",       PH7_FILTER_VALIDATE_EMAIL_Const },` |
|      - | 2162 | `	{"FILTER_VALIDATE_IP",          PH7_FILTER_VALIDATE_IP_Const },` |
|      - | 2163 | `	{"FILTER_VALIDATE_MAC",         PH7_FILTER_VALIDATE_MAC_Const },` |
|      - | 2164 | `	{"FILTER_SANITIZE_NUMBER_INT",  PH7_FILTER_SANITIZE_NUMBER_INT_Const },` |
|      - | 2165 | `	{"FILTER_SANITIZE_NUMBER_FLOAT",PH7_FILTER_SANITIZE_NUMBER_FLOAT_Const },` |
|      - | 2166 | `	{"FILTER_SANITIZE_SPECIAL_CHARS",PH7_FILTER_SANITIZE_SPECIAL_CHARS_Const },` |
|      - | 2167 | `	{"FILTER_SANITIZE_FULL_SPECIAL_CHARS",PH7_FILTER_SANITIZE_FULL_SPECIAL_CHARS_Const },` |
|      - | 2168 | `	{"FILTER_SANITIZE_EMAIL",       PH7_FILTER_SANITIZE_EMAIL_Const },` |
|      - | 2169 | `	{"FILTER_SANITIZE_URL",         PH7_FILTER_SANITIZE_URL_Const },` |
|      - | 2170 | `	{"FILTER_FLAG_ALLOW_OCTAL",     PH7_FILTER_FLAG_ALLOW_OCTAL_Const },` |
|      - | 2171 | `	{"FILTER_FLAG_ALLOW_HEX",       PH7_FILTER_FLAG_ALLOW_HEX_Const },` |
|      - | 2172 | `	{"FILTER_FLAG_STRIP_LOW",       PH7_FILTER_FLAG_STRIP_LOW_Const },` |
|      - | 2173 | `	{"FILTER_FLAG_STRIP_HIGH",      PH7_FILTER_FLAG_STRIP_HIGH_Const },` |
|      - | 2174 | `	{"FILTER_FLAG_ENCODE_LOW",      PH7_FILTER_FLAG_ENCODE_LOW_Const },` |
|      - | 2175 | `	{"FILTER_FLAG_ENCODE_HIGH",     PH7_FILTER_FLAG_ENCODE_HIGH_Const },` |
|      - | 2176 | `	{"FILTER_FLAG_ENCODE_AMP",      PH7_FILTER_FLAG_ENCODE_AMP_Const },` |
|      - | 2177 | `	{"FILTER_FLAG_NO_ENCODE_QUOTES",PH7_FILTER_FLAG_NO_ENCODE_QUOTES_Const },` |
|      - | 2178 | `	{"FILTER_FLAG_STRIP_BACKTICK",  PH7_FILTER_FLAG_STRIP_BACKTICK_Const },` |
|      - | 2179 | `	{"FILTER_FLAG_ALLOW_FRACTION",  PH7_FILTER_FLAG_ALLOW_FRACTION_Const },` |
|      - | 2180 | `	{"FILTER_FLAG_ALLOW_THOUSAND",  PH7_FILTER_FLAG_ALLOW_THOUSAND_Const },` |
|      - | 2181 | `	{"FILTER_FLAG_ALLOW_SCIENTIFIC",PH7_FILTER_FLAG_ALLOW_SCIENTIFIC_Const },` |
|      - | 2182 | `	{"FILTER_FLAG_IPV4",            PH7_FILTER_FLAG_IPV4_Const },` |
|      - | 2183 | `	{"FILTER_FLAG_IPV6",            PH7_FILTER_FLAG_IPV6_Const },` |
|      - | 2184 | `	{"FILTER_NULL_ON_FAILURE",      PH7_FILTER_NULL_ON_FAILURE_Const },` |
|      - | 2185 | `	{"INPUT_POST",                  PH7_INPUT_POST_Const },` |
|      - | 2186 | `	{"INPUT_GET",                   PH7_INPUT_GET_Const },` |
|      - | 2187 | `	{"INPUT_COOKIE",                PH7_INPUT_COOKIE_Const },` |
|      - | 2188 | `	{"INPUT_ENV",                   PH7_INPUT_ENV_Const },` |
|      - | 2189 | `	{"INPUT_SERVER",                PH7_INPUT_SERVER_Const },` |
|      - | 2190 | `	{"CAL_GREGORIAN",        PH7_CAL_GREGORIAN_Const },` |
|      - | 2191 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|      - | 2192 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|      - | 2193 | `	{"PHP_INT_MIN",          PH7_INTMIN_Const   },` |
|      - | 2194 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|      - | 2195 | `	{"PHP_FLOAT_EPSILON",    PH7_FLOATEPSILON_Const },` |
|      - | 2196 | `	{"PHP_FLOAT_MAX",        PH7_FLOATMAX_Const },` |
|      - | 2197 | `	{"PHP_FLOAT_MIN",        PH7_FLOATMIN_Const },` |
|      - | 2198 | `	{"PHP_FLOAT_DIG",        PH7_FLOATDIG_Const },` |
|      - | 2199 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|      - | 2200 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|      - | 2201 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|      - | 2202 | `	{"__TIME__",             PH7_TIME_Const     },` |
|      - | 2203 | `	{"__DATE__",             PH7_DATE_Const     },` |
|      - | 2204 | `	{"__FILE__",             PH7_FILE_Const     },` |
|      - | 2205 | `	{"__DIR__",              PH7_DIR_Const      },` |
|      - | 2206 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|      - | 2207 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|      - | 2208 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|      - | 2209 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|      - | 2210 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|      - | 2211 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|      - | 2212 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|      - | 2213 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|      - | 2214 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|      - | 2215 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|      - | 2216 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|      - | 2217 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|      - | 2218 | `	{"E_STRICT",             PH7_E_STRICT_Const        },` |
|      - | 2219 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|      - | 2220 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|      - | 2221 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|      - | 2222 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|      - | 2223 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|      - | 2224 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|      - | 2225 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|      - | 2226 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|      - | 2227 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|      - | 2228 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|      - | 2229 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|      - | 2230 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|      - | 2231 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|      - | 2232 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|      - | 2233 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|      - | 2234 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|      - | 2235 | `	{"SORT_LOCALE_STRING",   PH7_SORT_LOCALE_STRING_Const },` |
|      - | 2236 | `	{"SORT_NATURAL",         PH7_SORT_NATURAL_Const },` |
|      - | 2237 | `	{"SORT_FLAG_CASE",       PH7_SORT_FLAG_CASE_Const },` |
|      - | 2238 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|      - | 2239 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|      - | 2240 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|      - | 2241 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|      - | 2242 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|      - | 2243 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|      - | 2244 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 2245 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|      - | 2246 | `	{"M_E",                  PH7_M_E_Const          },` |
|      - | 2247 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|      - | 2248 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|      - | 2249 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|      - | 2250 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|      - | 2251 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|      - | 2252 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|      - | 2253 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|      - | 2254 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|      - | 2255 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|      - | 2256 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|      - | 2257 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|      - | 2258 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|      - | 2259 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|      - | 2260 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|      - | 2261 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|      - | 2262 | `	{"NAN",                  PH7_NAN_Const          },` |
|      - | 2263 | `	{"INF",                  PH7_INF_Const          },` |
|      - | 2264 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 2265 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|      - | 2266 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|      - | 2267 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|      - | 2268 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|      - | 2269 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|      - | 2270 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|      - | 2271 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|      - | 2272 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|      - | 2273 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|      - | 2274 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|      - | 2275 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|      - | 2276 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|      - | 2277 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|      - | 2278 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|      - | 2279 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|      - | 2280 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|      - | 2281 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|      - | 2282 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|      - | 2283 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|      - | 2284 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|      - | 2285 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|      - | 2286 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|      - | 2287 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|      - | 2288 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|      - | 2289 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|      - | 2290 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|      - | 2291 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|      - | 2292 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|      - | 2293 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|      - | 2294 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|      - | 2295 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|      - | 2296 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|      - | 2297 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|      - | 2298 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|      - | 2299 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|      - | 2300 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|      - | 2301 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|      - | 2302 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|      - | 2303 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|      - | 2304 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|      - | 2305 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|      - | 2306 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|      - | 2307 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|      - | 2308 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|      - | 2309 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|      - | 2310 | `	{"ASSERT_ACTIVE",        PH7_ASSERT_ACTIVE_Const     },` |
|      - | 2311 | `	{"ASSERT_CALLBACK",      PH7_ASSERT_CALLBACK_Const   },` |
|      - | 2312 | `	{"ASSERT_BAIL",          PH7_ASSERT_BAIL_Const       },` |
|      - | 2313 | `	{"ASSERT_WARNING",       PH7_ASSERT_WARNING_Const    },` |
|      - | 2314 | `	{"ASSERT_EXCEPTION",     PH7_ASSERT_EXCEPTION_Const  },` |
|      - | 2315 | `	/* ASSERT_QUIET_EVAL was REMOVED in php 8.0: referencing it is an Error there */` |
|      - | 2316 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|      - | 2317 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|      - | 2318 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|      - | 2319 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|      - | 2320 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|      - | 2321 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|      - | 2322 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|      - | 2323 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|      - | 2324 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|      - | 2325 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|      - | 2326 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|      - | 2327 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|      - | 2328 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|      - | 2329 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|      - | 2330 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|      - | 2331 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|      - | 2332 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|      - | 2333 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|      - | 2334 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|      - | 2335 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|      - | 2336 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|      - | 2337 | `	{"STDIN",                PH7_STDIN_Const        },` |
|      - | 2338 | `	{"stdin",                PH7_STDIN_Const        },` |
|      - | 2339 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|      - | 2340 | `	{"stdout",               PH7_STDOUT_Const       },` |
|      - | 2341 | `	{"STDERR",               PH7_STDERR_Const       },` |
|      - | 2342 | `	{"stderr",               PH7_STDERR_Const       },` |
|      - | 2343 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|      - | 2344 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|      - | 2345 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|      - | 2346 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|      - | 2347 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|      - | 2348 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|      - | 2349 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|      - | 2350 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|      - | 2351 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|      - | 2352 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2353 | `	{"XML_ERROR_NONE",       PH7_XML_ERROR_NONE_Const},` |
|      - | 2354 | `	{"XML_ERROR_NO_MEMORY",  PH7_XML_ERROR_NO_MEMORY_Const},` |
|      - | 2355 | `	{"XML_ERROR_SYNTAX",     PH7_XML_ERROR_SYNTAX_Const},` |
|      - | 2356 | `	{"XML_ERROR_NO_ELEMENTS",PH7_XML_ERROR_NO_ELEMENTS_Const},` |
|      - | 2357 | `	{"XML_ERROR_INVALID_TOKEN", PH7_XML_ERROR_INVALID_TOKEN_Const},` |
|      - | 2358 | `	{"XML_ERROR_UNCLOSED_TOKEN",PH7_XML_ERROR_UNCLOSED_TOKEN_Const},` |
|      - | 2359 | `	{"XML_ERROR_PARTIAL_CHAR",  PH7_XML_ERROR_PARTIAL_CHAR_Const},` |
|      - | 2360 | `	{"XML_ERROR_TAG_MISMATCH",  PH7_XML_ERROR_TAG_MISMATCH_Const},` |
|      - | 2361 | `	{"XML_ERROR_DUPLICATE_ATTRIBUTE",   PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const},` |
|      - | 2362 | `	{"XML_ERROR_JUNK_AFTER_DOC_ELEMENT",PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const},` |
|      - | 2363 | `	{"XML_ERROR_PARAM_ENTITY_REF",      PH7_XML_ERROR_PARAM_ENTITY_REF_Const},` |
|      - | 2364 | `	{"XML_ERROR_UNDEFINED_ENTITY",      PH7_XML_ERROR_UNDEFINED_ENTITY_Const},` |
|      - | 2365 | `	{"XML_ERROR_RECURSIVE_ENTITY_REF",  PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const},` |
|      - | 2366 | `	{"XML_ERROR_ASYNC_ENTITY",          PH7_XML_ERROR_ASYNC_ENTITY_Const},` |
|      - | 2367 | `	{"XML_ERROR_BAD_CHAR_REF",          PH7_XML_ERROR_BAD_CHAR_REF_Const},` |
|      - | 2368 | `	{"XML_ERROR_BINARY_ENTITY_REF",     PH7_XML_ERROR_BINARY_ENTITY_REF_Const},` |
|      - | 2369 | `	{"XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF", PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const},` |
|      - | 2370 | `	{"XML_ERROR_MISPLACED_XML_PI",     PH7_XML_ERROR_MISPLACED_XML_PI_Const},` |
|      - | 2371 | `	{"XML_ERROR_UNKNOWN_ENCODING",     PH7_XML_ERROR_UNKNOWN_ENCODING_Const},` |
|      - | 2372 | `	{"XML_ERROR_INCORRECT_ENCODING",   PH7_XML_ERROR_INCORRECT_ENCODING_Const},` |
|      - | 2373 | `	{"XML_ERROR_UNCLOSED_CDATA_SECTION",  PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const},` |
|      - | 2374 | `	{"XML_ERROR_EXTERNAL_ENTITY_HANDLING",PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const},` |
|      - | 2375 | `	{"XML_OPTION_CASE_FOLDING",           PH7_XML_OPTION_CASE_FOLDING_Const},` |
|      - | 2376 | `	{"XML_OPTION_TARGET_ENCODING",        PH7_XML_OPTION_TARGET_ENCODING_Const},` |
|      - | 2377 | `	{"XML_OPTION_SKIP_TAGSTART",          PH7_XML_OPTION_SKIP_TAGSTART_Const},` |
|      - | 2378 | `	{"XML_OPTION_SKIP_WHITE",             PH7_XML_OPTION_SKIP_WHITE_Const},` |
|      - | 2379 | `	{"XML_SAX_IMPL",           PH7_XML_SAX_IMP_Const},` |
|      - | 2380 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2381 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|      - | 2382 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|      - | 2383 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|      - | 2384 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|      - | 2385 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|      - | 2386 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|      - | 2387 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|      - | 2388 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|      - | 2389 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|      - | 2390 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|      - | 2391 | `	{"JSON_THROW_ON_ERROR",    PH7_JSON_THROW_ON_ERROR_Const},` |
|      - | 2392 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|      - | 2393 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|      - | 2394 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|      - | 2395 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|      - | 2396 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|      - | 2397 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|      - | 2398 | `	{"JSON_ERROR_NON_BACKED_ENUM", PH7_JSON_ERROR_NON_BACKED_ENUM_Const},` |
|      - | 2399 | `	{"JSON_ERROR_INF_OR_NAN", PH7_JSON_ERROR_INF_OR_NAN_Const},` |
|      - | 2400 | ``	/* `self`, `parent` and `static` are KEYWORDS in php, not constants: using one as a bare`` |
|      - | 2401 | ``	 * word is an "Undefined constant" Error (or a parse error for `static`). PH7 registered`` |
|      - | 2402 | `	 * them as constants that quietly expanded to the class name / NULL, so a typo'd bare` |
|      - | 2403 | ``	 * word silently produced a value. The `self::`/`parent::`/`static::` forms are handled`` |
|      - | 2404 | ``	 * by the `::` compile path and do not go through the constant table. */`` |
|      - | 2405 | `	{"__CLASS__",            PH7_class_magic_Const  }` |
|      - | 2406 | `};` |
|      - | 2407 | `/*` |
|      - | 2408 | ` * Register the built-in constants defined above.` |
|      - | 2409 | ` */` |
|   3414 | 2410 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|      5 | 2411 | `{` |
|      - | 2412 | `	sxu32 n;` |
|      - | 2413 | `	/*` |
|      - | 2414 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|      - | 2415 | `	 * that trigger the constant invocation as their private data.` |
|      - | 2416 | `	 */` |
| 925199 | 2417 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 921785 | 2418 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
| 460895 | 2419 | `	}` |
|   3419 | 2420 | `}` |
