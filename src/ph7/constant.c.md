# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 998/1067 lines (93.53%)

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
|      4 |   49 | `static void PH7_PHPVerIdConst(ph7_value *pVal,void *pUnused)` |
|      1 |   50 | `{` |
|      2 |   51 | `	SXUNUSED(pUnused);` |
|      5 |   52 | `	ph7_value_int64(pVal,PHP_COMPAT_VERSION_ID);` |
|      5 |   53 | `}` |
|      - |   54 | `#ifdef __WINNT__` |
|      - |   55 | `#include <Windows.h>` |
|      - |   56 | `#elif defined(__UNIXES__)` |
|      - |   57 | `#include <sys/utsname.h>` |
|      - |   58 | `#endif` |
|      - |   59 | `/*` |
|      - |   60 | ` * PHP_OS` |
|      - |   61 | ` *  Expand the name of the host Operating System.` |
|      - |   62 | ` */` |
|   4606 |   63 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|      5 |   64 | `{` |
|      - |   65 | `#if defined(__WINNT__)` |
|      5 |   66 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|      - |   67 | `#elif defined(__UNIXES__)` |
|      - |   68 | `	struct utsname sInfo;` |
|   4606 |   69 | `	if( uname(&sInfo) != 0 ){` |
|    ! 0 |   70 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 |   71 | `	}else{` |
|   4606 |   72 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|      - |   73 | `	}` |
|      - |   74 | `#else` |
|      - |   75 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|      - |   76 | `#endif` |
|   2303 |   77 | `	SXUNUSED(pUnused);` |
|   4611 |   78 | `}` |
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
|    834 |  133 | `static void PH7_EOL_Const(ph7_value *pVal,void *pUnused)` |
|      4 |  134 | `{` |
|    417 |  135 | `	SXUNUSED(pUnused);` |
|      - |  136 | `#ifdef __WINNT__` |
|      4 |  137 | `	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);` |
|      - |  138 | `#else` |
|    834 |  139 | `	ph7_value_string(pVal,"\n",(int)sizeof(char));` |
|      - |  140 | `#endif` |
|    838 |  141 | `}` |
|      - |  142 | `/*` |
|      - |  143 | ` * PHP_INT_MAX` |
|      - |  144 | ` * Expand the largest integer supported.` |
|      - |  145 | ` * Note that PH7 deals with 64-bit integer for all platforms.` |
|      - |  146 | ` */` |
|    176 |  147 | `static void PH7_INTMAX_Const(ph7_value *pVal,void *pUnused)` |
|      3 |  148 | `{` |
|     88 |  149 | `	SXUNUSED(pUnused);` |
|    179 |  150 | `	ph7_value_int64(pVal,SXI64_HIGH);` |
|    179 |  151 | `}` |
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
|     72 |  162 | `static void PH7_INTMIN_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  163 | `{` |
|     36 |  164 | `	SXUNUSED(pUnused);` |
|     73 |  165 | `	ph7_value_int64(pVal,SMALLEST_INT64);` |
|     73 |  166 | `}` |
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
|    334 |  205 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|      4 |  206 | `{` |
|    167 |  207 | `	SXUNUSED(pUnused);` |
|      - |  208 | `#ifdef __WINNT__` |
|      4 |  209 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|      - |  210 | `#else` |
|    334 |  211 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|      - |  212 | `#endif` |
|    338 |  213 | `}` |
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
|     92 |  232 | `static void PH7_NAN_Const(ph7_value *pVal,void *pUnused)` |
|      3 |  233 | `{` |
|     46 |  234 | `	SXUNUSED(pUnused);` |
|     95 |  235 | `	ph7_value_double(pVal, PH7_NAN_VALUE());` |
|     95 |  236 | `}` |
|      - |  237 |  |
|      - |  238 | `/*` |
|      - |  239 | ` * INF constant: positive infinity` |
|      - |  240 | ` */` |
|    106 |  241 | `static void PH7_INF_Const(ph7_value *pVal,void *pUnused)` |
|      3 |  242 | `{` |
|     53 |  243 | `	SXUNUSED(pUnused);` |
|      - |  244 | `	/* similarly avoid the INFINITY macro */` |
|    109 |  245 | `	ph7_value_double(pVal, PH7_INF_VALUE());` |
|    109 |  246 | `}` |
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
|      6 |  364 | `static void PH7_E_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  365 | `{` |
|      8 |  366 | `	ph7_value_int(pVal,2);` |
|      3 |  367 | `	SXUNUSED(pUserData);` |
|      8 |  368 | `}` |
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
|      4 |  427 | `static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  428 | `{` |
|      6 |  429 | `	ph7_value_int(pVal,256);` |
|      2 |  430 | `	SXUNUSED(pUserData);` |
|      6 |  431 | `}` |
|      - |  432 | `/*` |
|      - |  433 | ` * E_USER_WARNING` |
|      - |  434 | ` * Expands 512` |
|      - |  435 | ` */` |
|     28 |  436 | `static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      4 |  437 | `{` |
|     32 |  438 | `	ph7_value_int(pVal,512);` |
|     14 |  439 | `	SXUNUSED(pUserData);` |
|     32 |  440 | `}` |
|      - |  441 | `/*` |
|      - |  442 | ` * E_USER_NOTICE` |
|      - |  443 | ` * Expands 1024` |
|      - |  444 | ` */` |
|     10 |  445 | `static void PH7_E_USER_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|      4 |  446 | `{` |
|     14 |  447 | `	ph7_value_int(pVal,1024);` |
|      5 |  448 | `	SXUNUSED(pUserData);` |
|     14 |  449 | `}` |
|      - |  450 | `/*` |
|      - |  451 | ` * E_RECOVERABLE_ERROR` |
|      - |  452 | ` * Expands 4096` |
|      - |  453 | ` */` |
|      2 |  454 | `static void PH7_E_RECOVERABLE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  455 | `{` |
|      3 |  456 | `	ph7_value_int(pVal,4096);` |
|      1 |  457 | `	SXUNUSED(pUserData);` |
|      3 |  458 | `}` |
|      - |  459 | `/*` |
|      - |  460 | ` * E_DEPRECATED` |
|      - |  461 | ` * Expands 8192` |
|      - |  462 | ` */` |
|      2 |  463 | `static void PH7_E_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  464 | `{` |
|      3 |  465 | `	ph7_value_int(pVal,8192);` |
|      1 |  466 | `	SXUNUSED(pUserData);` |
|      3 |  467 | `}` |
|      - |  468 | `/*` |
|      - |  469 | ` * E_USER_DEPRECATED` |
|      - |  470 | ` *   Expands 16384.` |
|      - |  471 | ` */` |
|      2 |  472 | `static void PH7_E_USER_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  473 | `{` |
|      3 |  474 | `	ph7_value_int(pVal,16384);` |
|      1 |  475 | `	SXUNUSED(pUserData);` |
|      3 |  476 | `}` |
|      - |  477 | `/*` |
|      - |  478 | ` * E_ALL` |
|      - |  479 | ` *  Expands 30719 (php 8: E_STRICT is no longer part of E_ALL)` |
|      - |  480 | ` */` |
|     20 |  481 | `static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      3 |  482 | `{` |
|     23 |  483 | `	ph7_value_int(pVal,30719);` |
|     10 |  484 | `	SXUNUSED(pUserData);` |
|     23 |  485 | `}` |
|      - |  486 | `/*` |
|      - |  487 | ` * CASE_LOWER` |
|      - |  488 | ` *  Expands 0.` |
|      - |  489 | ` */` |
|      2 |  490 | `static void PH7_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  491 | `{` |
|      3 |  492 | `	ph7_value_int(pVal,0);` |
|      1 |  493 | `	SXUNUSED(pUserData);` |
|      3 |  494 | `}` |
|      - |  495 | `/*` |
|      - |  496 | ` * CASE_UPPER` |
|      - |  497 | ` *  Expands 1.` |
|      - |  498 | ` */` |
|      8 |  499 | `static void PH7_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  500 | `{` |
|     10 |  501 | `	ph7_value_int(pVal,1);` |
|      4 |  502 | `	SXUNUSED(pUserData);` |
|     10 |  503 | `}` |
|      - |  504 | `/*` |
|      - |  505 | ` * STR_PAD_LEFT` |
|      - |  506 | ` *  Expands 0.` |
|      - |  507 | ` */` |
|      6 |  508 | `static void PH7_STR_PAD_LEFT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  509 | `{` |
|      7 |  510 | `	ph7_value_int(pVal,0);` |
|      3 |  511 | `	SXUNUSED(pUserData);` |
|      7 |  512 | `}` |
|      - |  513 | `/*` |
|      - |  514 | ` * STR_PAD_RIGHT` |
|      - |  515 | ` *  Expands 1.` |
|      - |  516 | ` */` |
|      6 |  517 | `static void PH7_STR_PAD_RIGHT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  518 | `{` |
|      7 |  519 | `	ph7_value_int(pVal,1);` |
|      3 |  520 | `	SXUNUSED(pUserData);` |
|      7 |  521 | `}` |
|      - |  522 | `/*` |
|      - |  523 | ` * STR_PAD_BOTH` |
|      - |  524 | ` *  Expands 2.` |
|      - |  525 | ` */` |
|      4 |  526 | `static void PH7_STR_PAD_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  527 | `{` |
|      5 |  528 | `	ph7_value_int(pVal,2);` |
|      2 |  529 | `	SXUNUSED(pUserData);` |
|      5 |  530 | `}` |
|      - |  531 | `/*` |
|      - |  532 | ` * COUNT_NORMAL` |
|      - |  533 | ` *  Expands 0` |
|      - |  534 | ` */` |
|      8 |  535 | `static void PH7_COUNT_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  536 | `{` |
|     10 |  537 | `	ph7_value_int(pVal,0);` |
|      4 |  538 | `	SXUNUSED(pUserData);` |
|     10 |  539 | `}` |
|      - |  540 | `/*` |
|      - |  541 | ` * COUNT_RECURSIVE` |
|      - |  542 | ` *  Expands 1.` |
|      - |  543 | ` */` |
|     20 |  544 | `static void PH7_COUNT_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  545 | `{` |
|     22 |  546 | `	ph7_value_int(pVal,1);` |
|     10 |  547 | `	SXUNUSED(pUserData);` |
|     22 |  548 | `}` |
|      - |  549 | `/*` |
|      - |  550 | ` * php's sort-flag constants. The VALUES must match php exactly: they are a` |
|      - |  551 | ` * public ABI (code passes literal ints, dumps them, and OR-combines the base` |
|      - |  552 | ` * type with SORT_FLAG_CASE). SORT_ASC/SORT_DESC are the array_multisort` |
|      - |  553 | ` * direction flags.` |
|      - |  554 | ` * SORT_REGULAR 0 · SORT_NUMERIC 1 · SORT_STRING 2 · SORT_DESC 3 · SORT_ASC 4 ·` |
|      - |  555 | ` * SORT_LOCALE_STRING 5 · SORT_NATURAL 6 · SORT_FLAG_CASE 8` |
|      - |  556 | ` */` |
|      4 |  557 | `static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  558 | `{` |
|      5 |  559 | `	ph7_value_int(pVal,4);` |
|      2 |  560 | `	SXUNUSED(pUserData);` |
|      5 |  561 | `}` |
|      4 |  562 | `static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  563 | `{` |
|      5 |  564 | `	ph7_value_int(pVal,3);` |
|      2 |  565 | `	SXUNUSED(pUserData);` |
|      5 |  566 | `}` |
|     12 |  567 | `static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  568 | `{` |
|     14 |  569 | `	ph7_value_int(pVal,0);` |
|      6 |  570 | `	SXUNUSED(pUserData);` |
|     14 |  571 | `}` |
|     68 |  572 | `static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  573 | `{` |
|     70 |  574 | `	ph7_value_int(pVal,1);` |
|     34 |  575 | `	SXUNUSED(pUserData);` |
|     70 |  576 | `}` |
|     82 |  577 | `static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  578 | `{` |
|     84 |  579 | `	ph7_value_int(pVal,2);` |
|     41 |  580 | `	SXUNUSED(pUserData);` |
|     84 |  581 | `}` |
|      4 |  582 | `static void PH7_SORT_LOCALE_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  583 | `{` |
|      6 |  584 | `	ph7_value_int(pVal,5);` |
|      2 |  585 | `	SXUNUSED(pUserData);` |
|      6 |  586 | `}` |
|     18 |  587 | `static void PH7_SORT_NATURAL_Const(ph7_value *pVal,void *pUserData)` |
|      3 |  588 | `{` |
|     21 |  589 | `	ph7_value_int(pVal,6);` |
|      9 |  590 | `	SXUNUSED(pUserData);` |
|     21 |  591 | `}` |
|     14 |  592 | `static void PH7_SORT_FLAG_CASE_Const(ph7_value *pVal,void *pUserData)` |
|      3 |  593 | `{` |
|     17 |  594 | `	ph7_value_int(pVal,8);` |
|      7 |  595 | `	SXUNUSED(pUserData);` |
|     17 |  596 | `}` |
|      - |  597 | `/*` |
|      - |  598 | ` * PHP_ROUND_HALF_UP` |
|      - |  599 | ` *  Expands 1.` |
|      - |  600 | ` */` |
|      4 |  601 | `static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  602 | `{` |
|      5 |  603 | `	ph7_value_int(pVal,1);` |
|      2 |  604 | `	SXUNUSED(pUserData);` |
|      5 |  605 | `}` |
|      - |  606 | `/*` |
|      - |  607 | ` * PHP_SESSION_DISABLED / PHP_SESSION_NONE / PHP_SESSION_ACTIVE` |
|      - |  608 | ` *  session_status() states (0 / 1 / 2).` |
|      - |  609 | ` */` |
|      2 |  610 | `static void PH7_PHP_SESSION_DISABLED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  611 | `{` |
|      3 |  612 | `	ph7_value_int(pVal,0);` |
|      1 |  613 | `	SXUNUSED(pUserData);` |
|      3 |  614 | `}` |
|      6 |  615 | `static void PH7_PHP_SESSION_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  616 | `{` |
|      7 |  617 | `	ph7_value_int(pVal,1);` |
|      3 |  618 | `	SXUNUSED(pUserData);` |
|      7 |  619 | `}` |
|     30 |  620 | `static void PH7_PHP_SESSION_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  621 | `{` |
|     32 |  622 | `	ph7_value_int(pVal,2);` |
|     15 |  623 | `	SXUNUSED(pUserData);` |
|     32 |  624 | `}` |
|      - |  625 | `/*` |
|      - |  626 | ` * INI_USER / INI_PERDIR / INI_SYSTEM / INI_ALL` |
|      - |  627 | ` *  php.ini access levels (1 / 2 / 4 / 7).` |
|      - |  628 | ` */` |
|     16 |  629 | `static void PH7_INI_USER_Const(ph7_value *pVal,void *pUserData)` |
|      3 |  630 | `{` |
|     19 |  631 | `	ph7_value_int(pVal,1);` |
|      8 |  632 | `	SXUNUSED(pUserData);` |
|     19 |  633 | `}` |
|      2 |  634 | `static void PH7_INI_PERDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  635 | `{` |
|      3 |  636 | `	ph7_value_int(pVal,2);` |
|      1 |  637 | `	SXUNUSED(pUserData);` |
|      3 |  638 | `}` |
|      2 |  639 | `static void PH7_INI_SYSTEM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  640 | `{` |
|      3 |  641 | `	ph7_value_int(pVal,4);` |
|      1 |  642 | `	SXUNUSED(pUserData);` |
|      3 |  643 | `}` |
|      2 |  644 | `static void PH7_INI_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  645 | `{` |
|      3 |  646 | `	ph7_value_int(pVal,7);` |
|      1 |  647 | `	SXUNUSED(pUserData);` |
|      3 |  648 | `}` |
|      - |  649 | `/*` |
|      - |  650 | ` * MB_CASE_UPPER / MB_CASE_LOWER / MB_CASE_TITLE (0 / 1 / 2)` |
|      - |  651 | ` */` |
|      4 |  652 | `static void PH7_MB_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  653 | `{` |
|      5 |  654 | `	ph7_value_int(pVal,0);` |
|      2 |  655 | `	SXUNUSED(pUserData);` |
|      5 |  656 | `}` |
|      4 |  657 | `static void PH7_MB_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  658 | `{` |
|      5 |  659 | `	ph7_value_int(pVal,1);` |
|      2 |  660 | `	SXUNUSED(pUserData);` |
|      5 |  661 | `}` |
|     32 |  662 | `static void PH7_MB_CASE_TITLE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  663 | `{` |
|     33 |  664 | `	ph7_value_int(pVal,2);` |
|     16 |  665 | `	SXUNUSED(pUserData);` |
|     33 |  666 | `}` |
|      - |  667 | `/*` |
|      - |  668 | ` * SPHP_ROUND_HALF_DOWN` |
|      - |  669 | ` *  Expands 2.` |
|      - |  670 | ` */` |
|      4 |  671 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  672 | `{` |
|      5 |  673 | `	ph7_value_int(pVal,2);` |
|      2 |  674 | `	SXUNUSED(pUserData);` |
|      5 |  675 | `}` |
|      - |  676 | `/*` |
|      - |  677 | ` * PHP_ROUND_HALF_EVEN` |
|      - |  678 | ` *  Expands 3.` |
|      - |  679 | ` */` |
|      8 |  680 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  681 | `{` |
|      9 |  682 | `	ph7_value_int(pVal,3);` |
|      4 |  683 | `	SXUNUSED(pUserData);` |
|      9 |  684 | `}` |
|      - |  685 | `/*` |
|      - |  686 | ` * PHP_ROUND_HALF_ODD` |
|      - |  687 | ` *  Expands 4.` |
|      - |  688 | ` */` |
|      4 |  689 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  690 | `{` |
|      5 |  691 | `	ph7_value_int(pVal,4);` |
|      2 |  692 | `	SXUNUSED(pUserData);` |
|      5 |  693 | `}` |
|      - |  694 | `/*` |
|      - |  695 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|      - |  696 | ` *  Expand 0x01` |
|      - |  697 | ` * NOTE:` |
|      - |  698 | ` *  The expanded value must be a power of two.` |
|      - |  699 | ` */` |
|      4 |  700 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  701 | `{` |
|      5 |  702 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      2 |  703 | `	SXUNUSED(pUserData);` |
|      5 |  704 | `}` |
|      - |  705 | `/*` |
|      - |  706 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|      - |  707 | ` *  Expand 0x02` |
|      - |  708 | ` * NOTE:` |
|      - |  709 | ` *  The expanded value must be a power of two.` |
|      - |  710 | ` */` |
|      8 |  711 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  712 | `{` |
|      9 |  713 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      4 |  714 | `	SXUNUSED(pUserData);` |
|      9 |  715 | `}` |
|      - |  716 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  717 | `/*` |
|      - |  718 | ` * M_PI` |
|      - |  719 | ` *  Expand the value of pi.` |
|      - |  720 | ` */` |
|      8 |  721 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  722 | `{` |
|      4 |  723 | `	SXUNUSED(pUserData); /* cc warning */` |
|     10 |  724 | `	ph7_value_double(pVal,PH7_PI);` |
|     10 |  725 | `}` |
|      - |  726 | `/*` |
|      - |  727 | ` * M_E` |
|      - |  728 | ` *  Expand 2.7182818284590452354` |
|      - |  729 | ` */` |
|      2 |  730 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  731 | `{` |
|      1 |  732 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  733 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|      3 |  734 | `}` |
|      - |  735 | `/*` |
|      - |  736 | ` * M_LOG2E` |
|      - |  737 | ` *  Expand 2.7182818284590452354` |
|      - |  738 | ` */` |
|      2 |  739 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  740 | `{` |
|      1 |  741 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  742 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      3 |  743 | `}` |
|      - |  744 | `/*` |
|      - |  745 | ` * M_LOG10E` |
|      - |  746 | ` *  Expand 0.4342944819032518276` |
|      - |  747 | ` */` |
|      2 |  748 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  749 | `{` |
|      1 |  750 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  751 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      3 |  752 | `}` |
|      - |  753 | `/*` |
|      - |  754 | ` * M_LN2` |
|      - |  755 | ` *  Expand 	0.69314718055994530942` |
|      - |  756 | ` */` |
|      2 |  757 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  758 | `{` |
|      1 |  759 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  760 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      3 |  761 | `}` |
|      - |  762 | `/*` |
|      - |  763 | ` * M_LN10` |
|      - |  764 | ` *  Expand 	2.30258509299404568402` |
|      - |  765 | ` */` |
|      2 |  766 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  767 | `{` |
|      1 |  768 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  769 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      3 |  770 | `}` |
|      - |  771 | `/*` |
|      - |  772 | ` * M_PI_2` |
|      - |  773 | ` *  Expand 	1.57079632679489661923` |
|      - |  774 | ` */` |
|      2 |  775 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  776 | `{` |
|      1 |  777 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  778 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      3 |  779 | `}` |
|      - |  780 | `/*` |
|      - |  781 | ` * M_PI_4` |
|      - |  782 | ` *  Expand 	0.78539816339744830962` |
|      - |  783 | ` */` |
|      2 |  784 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  785 | `{` |
|      1 |  786 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  787 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      3 |  788 | `}` |
|      - |  789 | `/*` |
|      - |  790 | ` * M_1_PI` |
|      - |  791 | ` *  Expand 	0.31830988618379067154` |
|      - |  792 | ` */` |
|      2 |  793 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  794 | `{` |
|      1 |  795 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  796 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      3 |  797 | `}` |
|      - |  798 | `/*` |
|      - |  799 | ` * M_2_PI` |
|      - |  800 | ` *  Expand 0.63661977236758134308` |
|      - |  801 | ` */` |
|      4 |  802 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  803 | `{` |
|      2 |  804 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  805 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      5 |  806 | `}` |
|      - |  807 | `/*` |
|      - |  808 | ` * M_SQRTPI` |
|      - |  809 | ` *  Expand 1.77245385090551602729` |
|      - |  810 | ` */` |
|      2 |  811 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  812 | `{` |
|      1 |  813 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  814 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      3 |  815 | `}` |
|      - |  816 | `/*` |
|      - |  817 | ` * M_2_SQRTPI` |
|      - |  818 | ` *  Expand 	1.12837916709551257390` |
|      - |  819 | ` */` |
|      2 |  820 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  821 | `{` |
|      1 |  822 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  823 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      3 |  824 | `}` |
|      - |  825 | `/*` |
|      - |  826 | ` * M_SQRT2` |
|      - |  827 | ` *  Expand 	1.41421356237309504880` |
|      - |  828 | ` */` |
|      2 |  829 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  830 | `{` |
|      1 |  831 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  832 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      3 |  833 | `}` |
|      - |  834 | `/*` |
|      - |  835 | ` * M_SQRT3` |
|      - |  836 | ` *  Expand 	1.73205080756887729352` |
|      - |  837 | ` */` |
|      2 |  838 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  839 | `{` |
|      1 |  840 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  841 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      3 |  842 | `}` |
|      - |  843 | `/*` |
|      - |  844 | ` * M_SQRT1_2` |
|      - |  845 | ` *  Expand 	0.70710678118654752440` |
|      - |  846 | ` */` |
|      2 |  847 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  848 | `{` |
|      1 |  849 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  850 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      3 |  851 | `}` |
|      - |  852 | `/*` |
|      - |  853 | ` * M_LNPI` |
|      - |  854 | ` *  Expand 	1.14472988584940017414` |
|      - |  855 | ` */` |
|      2 |  856 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  857 | `{` |
|      1 |  858 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  859 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      3 |  860 | `}` |
|      - |  861 | `/*` |
|      - |  862 | ` * M_EULER` |
|      - |  863 | ` *  Expand  0.57721566490153286061` |
|      - |  864 | ` */` |
|      2 |  865 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  866 | `{` |
|      1 |  867 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  868 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      3 |  869 | `}` |
|      - |  870 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|      - |  871 | `/*` |
|      - |  872 | ` * DATE_ATOM` |
|      - |  873 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|      - |  874 | ` */` |
|      4 |  875 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  876 | `{` |
|      2 |  877 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  878 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      5 |  879 | `}` |
|      - |  880 | `/*` |
|      - |  881 | ` * DATE_COOKIE` |
|      - |  882 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  883 | ` */` |
|      2 |  884 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  885 | `{` |
|      1 |  886 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  887 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  888 | `}` |
|      - |  889 | `/*` |
|      - |  890 | ` * DATE_ISO8601` |
|      - |  891 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|      - |  892 | ` */` |
|      2 |  893 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  894 | `{` |
|      1 |  895 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  896 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      3 |  897 | `}` |
|      - |  898 | `/*` |
|      - |  899 | ` * DATE_RFC822` |
|      - |  900 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|      - |  901 | ` */` |
|      2 |  902 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  903 | `{` |
|      1 |  904 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  905 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  906 | `}` |
|      - |  907 | `/*` |
|      - |  908 | ` * DATE_RFC850` |
|      - |  909 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  910 | ` */` |
|      2 |  911 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  912 | `{` |
|      1 |  913 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  914 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  915 | `}` |
|      - |  916 | `/*` |
|      - |  917 | ` * DATE_RFC1036` |
|      - |  918 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  919 | ` */` |
|      2 |  920 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  921 | `{` |
|      1 |  922 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  923 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  924 | `}` |
|      - |  925 | `/*` |
|      - |  926 | ` * DATE_RFC1123` |
|      - |  927 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  928 | ` */` |
|      2 |  929 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  930 | `{` |
|      1 |  931 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  932 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  933 | `}` |
|      - |  934 | `/*` |
|      - |  935 | ` * DATE_RFC2822` |
|      - |  936 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  937 | ` */` |
|      2 |  938 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  939 | `{` |
|      1 |  940 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  941 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  942 | `}` |
|      - |  943 | `/*` |
|      - |  944 | ` * DATE_RSS` |
|      - |  945 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  946 | ` */` |
|      2 |  947 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  948 | `{` |
|      1 |  949 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  950 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  951 | `}` |
|      - |  952 | `/*` |
|      - |  953 | ` * DATE_W3C` |
|      - |  954 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|      - |  955 | ` */` |
|      2 |  956 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  957 | `{` |
|      1 |  958 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  959 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  960 | `}` |
|      - |  961 | `/*` |
|      - |  962 | ` * The ENT_* values are PHP-exact (php 8.5.7). The low two bits are the quote` |
|      - |  963 | ` * bits (1 = single, 2 = double), so ENT_QUOTES = ENT_COMPAT\|1 and` |
|      - |  964 | ` * ENT_NOQUOTES = 0. Bits 16\|32 select the doctype (0 = HTML401, 16 = XML1,` |
|      - |  965 | ` * 32 = XHTML, 48 = HTML5) — composites, not flags.` |
|      - |  966 | ` */` |
|      - |  967 | `/*` |
|      - |  968 | ` * ENT_COMPAT` |
|      - |  969 | ` *  Expand 2 (double-quote bit only)` |
|      - |  970 | ` */` |
|     12 |  971 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  972 | `{` |
|      6 |  973 | `	SXUNUSED(pUserData); /* cc warning */` |
|     13 |  974 | `	ph7_value_int(pVal,PH7_ENT_QUOTE_DOUBLE);` |
|     13 |  975 | `}` |
|      - |  976 | `/*` |
|      - |  977 | ` * ENT_QUOTES` |
|      - |  978 | ` *  Expand 3 (double\|single quote bits)` |
|      - |  979 | ` */` |
|     60 |  980 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  981 | `{` |
|     30 |  982 | `	SXUNUSED(pUserData); /* cc warning */` |
|     61 |  983 | `	ph7_value_int(pVal,PH7_ENT_QUOTES);` |
|     61 |  984 | `}` |
|      - |  985 | `/*` |
|      - |  986 | ` * ENT_NOQUOTES` |
|      - |  987 | ` *  Expand 0 (no quote bits)` |
|      - |  988 | ` */` |
|     20 |  989 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  990 | `{` |
|     10 |  991 | `	SXUNUSED(pUserData); /* cc warning */` |
|     21 |  992 | `	ph7_value_int(pVal,0);` |
|     21 |  993 | `}` |
|      - |  994 | `/*` |
|      - |  995 | ` * ENT_IGNORE` |
|      - |  996 | ` *  Expand 4` |
|      - |  997 | ` */` |
|      6 |  998 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  999 | `{` |
|      3 | 1000 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1001 | `	ph7_value_int(pVal,PH7_ENT_IGNORE);` |
|      7 | 1002 | `}` |
|      - | 1003 | `/*` |
|      - | 1004 | ` * ENT_SUBSTITUTE` |
|      - | 1005 | ` *  Expand 8` |
|      - | 1006 | ` */` |
|      2 | 1007 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1008 | `{` |
|      1 | 1009 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1010 | `	ph7_value_int(pVal,PH7_ENT_SUBSTITUTE);` |
|      3 | 1011 | `}` |
|      - | 1012 | `/*` |
|      - | 1013 | ` * ENT_DISALLOWED` |
|      - | 1014 | ` *  Expand 128` |
|      - | 1015 | ` */` |
|      2 | 1016 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1017 | `{` |
|      1 | 1018 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1019 | `	ph7_value_int(pVal,PH7_ENT_DISALLOWED);` |
|      3 | 1020 | `}` |
|      - | 1021 | `/*` |
|      - | 1022 | ` * ENT_HTML401` |
|      - | 1023 | ` *  Expand 0 (the default doctype)` |
|      - | 1024 | ` */` |
|      2 | 1025 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1026 | `{` |
|      1 | 1027 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1028 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML401);` |
|      3 | 1029 | `}` |
|      - | 1030 | `/*` |
|      - | 1031 | ` * ENT_XML1` |
|      - | 1032 | ` *  Expand 16` |
|      - | 1033 | ` */` |
|      8 | 1034 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1035 | `{` |
|      4 | 1036 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 | 1037 | `	ph7_value_int(pVal,PH7_ENT_DOC_XML1);` |
|      9 | 1038 | `}` |
|      - | 1039 | `/*` |
|      - | 1040 | ` * ENT_XHTML` |
|      - | 1041 | ` *  Expand 32` |
|      - | 1042 | ` */` |
|      6 | 1043 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1044 | `{` |
|      3 | 1045 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1046 | `	ph7_value_int(pVal,PH7_ENT_DOC_XHTML);` |
|      7 | 1047 | `}` |
|      - | 1048 | `/*` |
|      - | 1049 | ` * ENT_HTML5` |
|      - | 1050 | ` *  Expand 48 (16\|32 — a doctype composite, not a flag bit)` |
|      - | 1051 | ` */` |
|      8 | 1052 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1053 | `{` |
|      4 | 1054 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 | 1055 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML5);` |
|      9 | 1056 | `}` |
|      - | 1057 | `/*` |
|      - | 1058 | ` * ISO-8859-1` |
|      - | 1059 | ` * ISO_8859_1` |
|      - | 1060 | ` *   Expand 1` |
|      - | 1061 | ` */` |
|      2 | 1062 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1063 | `{` |
|      1 | 1064 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1065 | `	ph7_value_int(pVal,1);` |
|      3 | 1066 | `}` |
|      - | 1067 | `/*` |
|      - | 1068 | ` * UTF-8` |
|      - | 1069 | ` * UTF8` |
|      - | 1070 | ` *  Expand 2` |
|      - | 1071 | ` */` |
|      2 | 1072 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1073 | `{` |
|      1 | 1074 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1075 | `	ph7_value_int(pVal,1);` |
|      3 | 1076 | `}` |
|      - | 1077 | `/*` |
|      - | 1078 | ` * HTML_ENTITIES` |
|      - | 1079 | ` *  Expand 1` |
|      - | 1080 | ` */` |
|      4 | 1081 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1082 | `{` |
|      2 | 1083 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1084 | `	ph7_value_int(pVal,1);` |
|      5 | 1085 | `}` |
|      - | 1086 | `/*` |
|      - | 1087 | ` * HTML_SPECIALCHARS` |
|      - | 1088 | ` *  Expand 0 (PHP-exact)` |
|      - | 1089 | ` */` |
|     10 | 1090 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1091 | `{` |
|      5 | 1092 | `	SXUNUSED(pUserData); /* cc warning */` |
|     11 | 1093 | `	ph7_value_int(pVal,0);` |
|     11 | 1094 | `}` |
|      - | 1095 | `/*` |
|      - | 1096 | ` * PHP_URL_SCHEME.` |
|      - | 1097 | ` * Expand 0` |
|      - | 1098 | ` */` |
|      4 | 1099 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1100 | `{` |
|      2 | 1101 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1102 | `	ph7_value_int(pVal,0);` |
|      5 | 1103 | `}` |
|      - | 1104 | `/*` |
|      - | 1105 | ` * PHP_URL_HOST.` |
|      - | 1106 | ` * Expand 1` |
|      - | 1107 | ` */` |
|      6 | 1108 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1109 | `{` |
|      3 | 1110 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1111 | `	ph7_value_int(pVal,1);` |
|      7 | 1112 | `}` |
|      - | 1113 | `/*` |
|      - | 1114 | ` * PHP_URL_PORT.` |
|      - | 1115 | ` * Expand 2` |
|      - | 1116 | ` */` |
|      6 | 1117 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1118 | `{` |
|      3 | 1119 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1120 | `	ph7_value_int(pVal,2);` |
|      7 | 1121 | `}` |
|      - | 1122 | `/*` |
|      - | 1123 | ` * PHP_URL_USER.` |
|      - | 1124 | ` * Expand 3` |
|      - | 1125 | ` */` |
|      4 | 1126 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1127 | `{` |
|      2 | 1128 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1129 | `	ph7_value_int(pVal,3);` |
|      5 | 1130 | `}` |
|      - | 1131 | `/*` |
|      - | 1132 | ` * PHP_URL_PASS.` |
|      - | 1133 | ` * Expand 4` |
|      - | 1134 | ` */` |
|      4 | 1135 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1136 | `{` |
|      2 | 1137 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1138 | `	ph7_value_int(pVal,4);` |
|      5 | 1139 | `}` |
|      - | 1140 | `/*` |
|      - | 1141 | ` * PHP_URL_PATH.` |
|      - | 1142 | ` * Expand 5` |
|      - | 1143 | ` */` |
|      4 | 1144 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1145 | `{` |
|      2 | 1146 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1147 | `	ph7_value_int(pVal,5);` |
|      5 | 1148 | `}` |
|      - | 1149 | `/*` |
|      - | 1150 | ` * PHP_URL_QUERY.` |
|      - | 1151 | ` * Expand 6` |
|      - | 1152 | ` */` |
|      6 | 1153 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1154 | `{` |
|      3 | 1155 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1156 | `	ph7_value_int(pVal,6);` |
|      7 | 1157 | `}` |
|      - | 1158 | `/*` |
|      - | 1159 | ` * PHP_URL_FRAGMENT.` |
|      - | 1160 | ` * Expand 7` |
|      - | 1161 | ` */` |
|      6 | 1162 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1163 | `{` |
|      3 | 1164 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1165 | `	ph7_value_int(pVal,7);` |
|      7 | 1166 | `}` |
|      - | 1167 | `/*` |
|      - | 1168 | ` * PHP_QUERY_RFC1738` |
|      - | 1169 | ` * Expand 1` |
|      - | 1170 | ` */` |
|     34 | 1171 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1172 | `{` |
|     17 | 1173 | `	SXUNUSED(pUserData); /* cc warning */` |
|     36 | 1174 | `	ph7_value_int(pVal,1);` |
|     36 | 1175 | `}` |
|      - | 1176 | `/*` |
|      - | 1177 | ` * PHP_QUERY_RFC3986` |
|      - | 1178 | ` * Expand 1` |
|      - | 1179 | ` */` |
|     96 | 1180 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1181 | `{` |
|     48 | 1182 | `	SXUNUSED(pUserData); /* cc warning */` |
|     97 | 1183 | `	ph7_value_int(pVal,2);` |
|     97 | 1184 | `}` |
|      - | 1185 | `/* php's FNM_* values (ext/standard): PATHNAME=1, NOESCAPE=2, PERIOD=4, CASEFOLD=16.` |
|      - | 1186 | ` * PHL previously had PATHNAME/NOESCAPE swapped and CASEFOLD=8; fnmatch() reads these` |
|      - | 1187 | ` * bits, so PH7_builtin_fnmatch was updated to the same values. */` |
|      - | 1188 | `/*` |
|      - | 1189 | ` * FNM_PATHNAME` |
|      - | 1190 | ` *  Expand 1 (php value)` |
|      - | 1191 | ` */` |
|    ! 0 | 1192 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 1193 | `{` |
|    ! 0 | 1194 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 1195 | `	ph7_value_int(pVal,1);` |
|    ! 0 | 1196 | `}` |
|      - | 1197 | `/*` |
|      - | 1198 | ` * FNM_NOESCAPE` |
|      - | 1199 | ` *  Expand 2 (php value)` |
|      - | 1200 | ` */` |
|    ! 0 | 1201 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 1202 | `{` |
|    ! 0 | 1203 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 1204 | `	ph7_value_int(pVal,2);` |
|    ! 0 | 1205 | `}` |
|      - | 1206 | `/*` |
|      - | 1207 | ` * FNM_PERIOD` |
|      - | 1208 | ` *  Expand 4 (php value)` |
|      - | 1209 | ` */` |
|      6 | 1210 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1211 | `{` |
|      3 | 1212 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1213 | `	ph7_value_int(pVal,4);` |
|      7 | 1214 | `}` |
|      - | 1215 | `/*` |
|      - | 1216 | ` * FNM_CASEFOLD` |
|      - | 1217 | ` *  Expand 16 (php value)` |
|      - | 1218 | ` */` |
|      4 | 1219 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1220 | `{` |
|      2 | 1221 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1222 | `	ph7_value_int(pVal,16);` |
|      5 | 1223 | `}` |
|      - | 1224 | `/*` |
|      - | 1225 | ` * PATHINFO_DIRNAME` |
|      - | 1226 | ` *  Expand 1.` |
|      - | 1227 | ` */` |
|     22 | 1228 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1229 | `{` |
|     11 | 1230 | `	SXUNUSED(pUserData); /* cc warning */` |
|     23 | 1231 | `	ph7_value_int(pVal,PH7_PATHINFO_DIRNAME);` |
|     23 | 1232 | `}` |
|      - | 1233 | `/*` |
|      - | 1234 | ` * PATHINFO_BASENAME` |
|      - | 1235 | ` *  Expand 2.` |
|      - | 1236 | ` */` |
|     22 | 1237 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1238 | `{` |
|     11 | 1239 | `	SXUNUSED(pUserData); /* cc warning */` |
|     23 | 1240 | `	ph7_value_int(pVal,PH7_PATHINFO_BASENAME);` |
|     23 | 1241 | `}` |
|      - | 1242 | `/*` |
|      - | 1243 | ` * PATHINFO_EXTENSION` |
|      - | 1244 | ` *  Expand php's 4 (a POWER OF TWO: the components are a bitmask).` |
|      - | 1245 | ` */` |
|   7206 | 1246 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1247 | `{` |
|   3603 | 1248 | `	SXUNUSED(pUserData); /* cc warning */` |
|   7211 | 1249 | `	ph7_value_int(pVal,PH7_PATHINFO_EXTENSION);` |
|   7211 | 1250 | `}` |
|      - | 1251 | `/*` |
|      - | 1252 | ` * PATHINFO_FILENAME` |
|      - | 1253 | ` *  Expand php's 8 (a POWER OF TWO: the components are a bitmask).` |
|      - | 1254 | ` */` |
|   7196 | 1255 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1256 | `{` |
|   3598 | 1257 | `	SXUNUSED(pUserData); /* cc warning */` |
|   7201 | 1258 | `	ph7_value_int(pVal,PH7_PATHINFO_FILENAME);` |
|   7201 | 1259 | `}` |
|      - | 1260 | `/*` |
|      - | 1261 | ` * PATHINFO_ALL` |
|      - | 1262 | ` *  Expand php's 15 — the default, and the one value that answers with the ARRAY.` |
|      - | 1263 | ` */` |
|     10 | 1264 | `static void PH7_PATHINFO_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1265 | `{` |
|      5 | 1266 | `	SXUNUSED(pUserData); /* cc warning */` |
|     11 | 1267 | `	ph7_value_int(pVal,PH7_PATHINFO_ALL);` |
|     11 | 1268 | `}` |
|      - | 1269 | `/*` |
|      - | 1270 | ` * SEEK_SET.` |
|      - | 1271 | ` *  Expand 0` |
|      - | 1272 | ` */` |
|     20 | 1273 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1274 | `{` |
|     10 | 1275 | `	SXUNUSED(pUserData); /* cc warning */` |
|     21 | 1276 | `	ph7_value_int(pVal,0);` |
|     21 | 1277 | `}` |
|      - | 1278 | `/*` |
|      - | 1279 | ` * SEEK_CUR.` |
|      - | 1280 | ` *  Expand 1` |
|      - | 1281 | ` */` |
|      6 | 1282 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1283 | `{` |
|      3 | 1284 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1285 | `	ph7_value_int(pVal,1);` |
|      7 | 1286 | `}` |
|      - | 1287 | `/*` |
|      - | 1288 | ` * SEEK_END.` |
|      - | 1289 | ` *  Expand 2` |
|      - | 1290 | ` */` |
|      8 | 1291 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1292 | `{` |
|      4 | 1293 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 | 1294 | `	ph7_value_int(pVal,2);` |
|      9 | 1295 | `}` |
|      - | 1296 | `/*` |
|      - | 1297 | ` * LOCK_SH.` |
|      - | 1298 | ` *  Expand 2` |
|      - | 1299 | ` */` |
|     10 | 1300 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1301 | `{` |
|      5 | 1302 | `	SXUNUSED(pUserData); /* cc warning */` |
|     11 | 1303 | `	ph7_value_int(pVal,1);` |
|     11 | 1304 | `}` |
|      - | 1305 | `/*` |
|      - | 1306 | ` * LOCK_NB.` |
|      - | 1307 | ` *  Expand 4 (php)` |
|      - | 1308 | ` */` |
|     18 | 1309 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1310 | `{` |
|      9 | 1311 | `	SXUNUSED(pUserData); /* cc warning */` |
|     19 | 1312 | `	ph7_value_int(pVal,4);` |
|     19 | 1313 | `}` |
|      - | 1314 | `/*` |
|      - | 1315 | ` * LOCK_EX.` |
|      - | 1316 | ` *  Expand 2 (php). PH7 used 1, which collided with LOCK_SH, and LOCK_UN was 0 — so` |
|      - | 1317 | ` *  flock($h, LOCK_UN) asked the stream for a SHARED lock instead of releasing one.` |
|      - | 1318 | ` */` |
|     18 | 1319 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1320 | `{` |
|      9 | 1321 | `	SXUNUSED(pUserData); /* cc warning */` |
|     19 | 1322 | `	ph7_value_int(pVal,2);` |
|     19 | 1323 | `}` |
|      - | 1324 | `/*` |
|      - | 1325 | ` * LOCK_UN.` |
|      - | 1326 | ` *  Expand 3 (php)` |
|      - | 1327 | ` */` |
|      8 | 1328 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1329 | `{` |
|      4 | 1330 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 | 1331 | `	ph7_value_int(pVal,3);` |
|      9 | 1332 | `}` |
|      - | 1333 | `/*` |
|      - | 1334 | ` * FILE_USE_INCLUDE_PATH` |
|      - | 1335 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1336 | ` */` |
|      4 | 1337 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1338 | `{` |
|      2 | 1339 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1340 | `	ph7_value_int(pVal,0x1);` |
|      5 | 1341 | `}` |
|      - | 1342 | `/*` |
|      - | 1343 | ` * FILE_IGNORE_NEW_LINES` |
|      - | 1344 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1345 | ` */` |
|     10 | 1346 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1347 | `{` |
|      5 | 1348 | `	SXUNUSED(pUserData); /* cc warning */` |
|     11 | 1349 | `	ph7_value_int(pVal,0x2);` |
|     11 | 1350 | `}` |
|      - | 1351 | `/*` |
|      - | 1352 | ` * FILE_SKIP_EMPTY_LINES` |
|      - | 1353 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1354 | ` */` |
|      4 | 1355 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1356 | `{` |
|      2 | 1357 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1358 | `	ph7_value_int(pVal,0x4);` |
|      5 | 1359 | `}` |
|      - | 1360 | `/*` |
|      - | 1361 | ` * FILE_APPEND` |
|      - | 1362 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1363 | ` */` |
|      4 | 1364 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1365 | `{` |
|      2 | 1366 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1367 | `	ph7_value_int(pVal,0x08);` |
|      5 | 1368 | `}` |
|      - | 1369 | `/*` |
|      - | 1370 | ` * SCANDIR_SORT_ASCENDING` |
|      - | 1371 | ` *  Expand 0` |
|      - | 1372 | ` */` |
|   2222 | 1373 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1374 | `{` |
|   1111 | 1375 | `	SXUNUSED(pUserData); /* cc warning */` |
|   2227 | 1376 | `	ph7_value_int(pVal,0);` |
|   2227 | 1377 | `}` |
|      - | 1378 | `/*` |
|      - | 1379 | ` * SCANDIR_SORT_DESCENDING` |
|      - | 1380 | ` *  Expand 1` |
|      - | 1381 | ` */` |
|      6 | 1382 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1383 | `{` |
|      3 | 1384 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1385 | `	ph7_value_int(pVal,1);` |
|      7 | 1386 | `}` |
|      - | 1387 | `/*` |
|      - | 1388 | ` * SCANDIR_SORT_NONE` |
|      - | 1389 | ` *  Expand 2` |
|      - | 1390 | ` */` |
|   1122 | 1391 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1392 | `{` |
|    561 | 1393 | `	SXUNUSED(pUserData); /* cc warning */` |
|   1127 | 1394 | `	ph7_value_int(pVal,2);` |
|   1127 | 1395 | `}` |
|      - | 1396 | `/*` |
|      - | 1397 | ` * GLOB_MARK` |
|      - | 1398 | ` *  Expand php's 0x08 (php's own portable glob flag set)` |
|      - | 1399 | ` */` |
|     40 | 1400 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1401 | `{` |
|     20 | 1402 | `	SXUNUSED(pUserData); /* cc warning */` |
|     41 | 1403 | `	ph7_value_int(pVal,PH7_GLOB_MARK);` |
|     41 | 1404 | `}` |
|      - | 1405 | `/*` |
|      - | 1406 | ` * GLOB_NOSORT` |
|      - | 1407 | ` *  Expand php's 0x20` |
|      - | 1408 | ` */` |
|     44 | 1409 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1410 | `{` |
|     22 | 1411 | `	SXUNUSED(pUserData); /* cc warning */` |
|     45 | 1412 | `	ph7_value_int(pVal,PH7_GLOB_NOSORT);` |
|     45 | 1413 | `}` |
|      - | 1414 | `/*` |
|      - | 1415 | ` * GLOB_NOCHECK` |
|      - | 1416 | ` *  Expand php's 0x10` |
|      - | 1417 | ` */` |
|     44 | 1418 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1419 | `{` |
|     22 | 1420 | `	SXUNUSED(pUserData); /* cc warning */` |
|     45 | 1421 | `	ph7_value_int(pVal,PH7_GLOB_NOCHECK);` |
|     45 | 1422 | `}` |
|      - | 1423 | `/*` |
|      - | 1424 | ` * GLOB_NOESCAPE` |
|      - | 1425 | ` *  Expand php's 0x1000` |
|      - | 1426 | ` */` |
|     28 | 1427 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1428 | `{` |
|     14 | 1429 | `	SXUNUSED(pUserData); /* cc warning */` |
|     29 | 1430 | `	ph7_value_int(pVal,PH7_GLOB_NOESCAPE);` |
|     29 | 1431 | `}` |
|      - | 1432 | `/*` |
|      - | 1433 | ` * GLOB_BRACE` |
|      - | 1434 | ` *  Expand php's 0x80` |
|      - | 1435 | ` */` |
|     28 | 1436 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1437 | `{` |
|     14 | 1438 | `	SXUNUSED(pUserData); /* cc warning */` |
|     29 | 1439 | `	ph7_value_int(pVal,PH7_GLOB_BRACE);` |
|     29 | 1440 | `}` |
|      - | 1441 | `/*` |
|      - | 1442 | ` * GLOB_ONLYDIR` |
|      - | 1443 | ` *  Expand php's 0x40000000` |
|      - | 1444 | ` */` |
|     46 | 1445 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1446 | `{` |
|     23 | 1447 | `	SXUNUSED(pUserData); /* cc warning */` |
|     47 | 1448 | `	ph7_value_int(pVal,PH7_GLOB_ONLYDIR);` |
|     47 | 1449 | `}` |
|      - | 1450 | `/*` |
|      - | 1451 | ` * GLOB_ERR` |
|      - | 1452 | ` *  Expand php's 0x04` |
|      - | 1453 | ` */` |
|     28 | 1454 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1455 | `{` |
|     14 | 1456 | `	SXUNUSED(pUserData); /* cc warning */` |
|     29 | 1457 | `	ph7_value_int(pVal,PH7_GLOB_ERR);` |
|     29 | 1458 | `}` |
|      - | 1459 | `/*` |
|      - | 1460 | ` * STDIN` |
|      - | 1461 | ` *  Expand the STDIN handle as a resource.` |
|      - | 1462 | ` */` |
|      2 | 1463 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1464 | `{` |
|      3 | 1465 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1466 | `	void *pResource;` |
|      3 | 1467 | `	pResource = PH7_ExportStdin(pVm);` |
|      3 | 1468 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1469 | `}` |
|      - | 1470 | `/*` |
|      - | 1471 | ` * STDOUT` |
|      - | 1472 | ` *   Expand the STDOUT handle as a resource.` |
|      - | 1473 | ` */` |
|      8 | 1474 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1475 | `{` |
|      9 | 1476 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1477 | `	void *pResource;` |
|      9 | 1478 | `	pResource = PH7_ExportStdout(pVm);` |
|      9 | 1479 | `	ph7_value_resource(pVal,pResource);` |
|      9 | 1480 | `}` |
|      - | 1481 | `/*` |
|      - | 1482 | ` * STDERR` |
|      - | 1483 | ` *  Expand the STDERR handle as a resource.` |
|      - | 1484 | ` */` |
|     10 | 1485 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1486 | `{` |
|     11 | 1487 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1488 | `	void *pResource;` |
|     11 | 1489 | `	pResource = PH7_ExportStderr(pVm);` |
|     11 | 1490 | `	ph7_value_resource(pVal,pResource);` |
|     11 | 1491 | `}` |
|      - | 1492 | `/*` |
|      - | 1493 | ` * INI_SCANNER_NORMAL` |
|      - | 1494 | ` *   Expand php's 0` |
|      - | 1495 | ` */` |
|      4 | 1496 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1497 | `{` |
|      2 | 1498 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1499 | `	ph7_value_int(pVal,PH7_INI_SCANNER_NORMAL);` |
|      5 | 1500 | `}` |
|      - | 1501 | `/*` |
|      - | 1502 | ` * INI_SCANNER_RAW` |
|      - | 1503 | ` *   Expand php's 1` |
|      - | 1504 | ` */` |
|      4 | 1505 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1506 | `{` |
|      2 | 1507 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1508 | `	ph7_value_int(pVal,PH7_INI_SCANNER_RAW);` |
|      5 | 1509 | `}` |
|      - | 1510 | `/*` |
|      - | 1511 | ` * EXTR_OVERWRITE` |
|      - | 1512 | ` *   Expand 0 (php's enum value; see PH7_EXTR_* in ph7int.h)` |
|      - | 1513 | ` */` |
|     12 | 1514 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1515 | `{` |
|      6 | 1516 | `	SXUNUSED(pUserData); /* cc warning */` |
|     13 | 1517 | `	ph7_value_int(pVal,PH7_EXTR_OVERWRITE);` |
|     13 | 1518 | `}` |
|      - | 1519 | `/*` |
|      - | 1520 | ` * EXTR_SKIP` |
|      - | 1521 | ` *   Expand 1` |
|      - | 1522 | ` */` |
|     14 | 1523 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1524 | `{` |
|      7 | 1525 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1526 | `	ph7_value_int(pVal,PH7_EXTR_SKIP);` |
|     16 | 1527 | `}` |
|      - | 1528 | `/*` |
|      - | 1529 | ` * EXTR_PREFIX_SAME` |
|      - | 1530 | ` *   Expand 2` |
|      - | 1531 | ` */` |
|     34 | 1532 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1533 | `{` |
|     17 | 1534 | `	SXUNUSED(pUserData); /* cc warning */` |
|     36 | 1535 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_SAME);` |
|     36 | 1536 | `}` |
|      - | 1537 | `/*` |
|      - | 1538 | ` * EXTR_PREFIX_ALL` |
|      - | 1539 | ` *   Expand 3` |
|      - | 1540 | ` */` |
|     24 | 1541 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1542 | `{` |
|     12 | 1543 | `	SXUNUSED(pUserData); /* cc warning */` |
|     26 | 1544 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_ALL);` |
|     26 | 1545 | `}` |
|      - | 1546 | `/*` |
|      - | 1547 | ` * EXTR_PREFIX_INVALID` |
|      - | 1548 | ` *   Expand 4` |
|      - | 1549 | ` */` |
|      8 | 1550 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1551 | `{` |
|      4 | 1552 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 | 1553 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_INVALID);` |
|      9 | 1554 | `}` |
|      - | 1555 | `/*` |
|      - | 1556 | ` * EXTR_IF_EXISTS` |
|      - | 1557 | ` *   Expand 6 (php orders IF_EXISTS after PREFIX_IF_EXISTS)` |
|      - | 1558 | ` */` |
|     14 | 1559 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1560 | `{` |
|      7 | 1561 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1562 | `	ph7_value_int(pVal,PH7_EXTR_IF_EXISTS);` |
|     16 | 1563 | `}` |
|      - | 1564 | `/*` |
|      - | 1565 | ` * EXTR_PREFIX_IF_EXISTS` |
|      - | 1566 | ` *   Expand 5` |
|      - | 1567 | ` */` |
|     20 | 1568 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1569 | `{` |
|     10 | 1570 | `	SXUNUSED(pUserData); /* cc warning */` |
|     22 | 1571 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_IF_EXISTS);` |
|     22 | 1572 | `}` |
|      - | 1573 | `/*` |
|      - | 1574 | ` * JSON_HEX_TAG.` |
|      - | 1575 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|      - | 1576 | ` */` |
|      6 | 1577 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1578 | `{` |
|      3 | 1579 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1580 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      7 | 1581 | `}` |
|      - | 1582 | `/*` |
|      - | 1583 | ` * JSON_HEX_AMP.` |
|      - | 1584 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|      - | 1585 | ` */` |
|      6 | 1586 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1587 | `{` |
|      3 | 1588 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1589 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      7 | 1590 | `}` |
|      - | 1591 | `/*` |
|      - | 1592 | ` * JSON_HEX_APOS.` |
|      - | 1593 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|      - | 1594 | ` */` |
|      6 | 1595 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1596 | `{` |
|      3 | 1597 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1598 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      7 | 1599 | `}` |
|      - | 1600 | `/*` |
|      - | 1601 | ` * JSON_HEX_QUOT.` |
|      - | 1602 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|      - | 1603 | ` */` |
|      6 | 1604 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1605 | `{` |
|      3 | 1606 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1607 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      7 | 1608 | `}` |
|      - | 1609 | `/*` |
|      - | 1610 | ` * JSON_FORCE_OBJECT.` |
|      - | 1611 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|      - | 1612 | ` */` |
|      6 | 1613 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1614 | `{` |
|      3 | 1615 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1616 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      7 | 1617 | `}` |
|      - | 1618 | `/*` |
|      - | 1619 | ` * JSON_NUMERIC_CHECK.` |
|      - | 1620 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|      - | 1621 | ` */` |
|      6 | 1622 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1623 | `{` |
|      3 | 1624 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1625 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      7 | 1626 | `}` |
|      - | 1627 | `/*` |
|      - | 1628 | ` * JSON_BIGINT_AS_STRING.` |
|      - | 1629 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|      - | 1630 | ` */` |
|      2 | 1631 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1632 | `{` |
|      1 | 1633 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1634 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      3 | 1635 | `}` |
|      - | 1636 | `/*` |
|      - | 1637 | ` * JSON_PRETTY_PRINT.` |
|      - | 1638 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|      - | 1639 | ` */` |
|      8 | 1640 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1641 | `{` |
|      4 | 1642 | `	SXUNUSED(pUserData); /* cc warning */` |
|     10 | 1643 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|     10 | 1644 | `}` |
|      - | 1645 | `/*` |
|      - | 1646 | ` * JSON_UNESCAPED_SLASHES.` |
|      - | 1647 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|      - | 1648 | ` */` |
|     10 | 1649 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1650 | `{` |
|      5 | 1651 | `	SXUNUSED(pUserData); /* cc warning */` |
|     12 | 1652 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|     12 | 1653 | `}` |
|      - | 1654 | `/*` |
|      - | 1655 | ` * JSON_UNESCAPED_UNICODE.` |
|      - | 1656 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|      - | 1657 | ` */` |
|     22 | 1658 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1659 | `{` |
|     11 | 1660 | `	SXUNUSED(pUserData); /* cc warning */` |
|     23 | 1661 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|     23 | 1662 | `}` |
|      - | 1663 | `/*` |
|      - | 1664 | ` * JSON_UNESCAPED_LINE_TERMINATORS.` |
|      - | 1665 | ` *   Expand the value of JSON_UNESCAPED_LINE_TERMINATORS defined in ph7Int.h.` |
|      - | 1666 | ` */` |
|      6 | 1667 | `static void PH7_JSON_UNESCAPED_LINE_TERMINATORS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1668 | `{` |
|      3 | 1669 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1670 | `	ph7_value_int(pVal,JSON_UNESCAPED_LINE_TERMINATORS);` |
|      7 | 1671 | `}` |
|      - | 1672 | `/*` |
|      - | 1673 | ` * JSON_INVALID_UTF8_IGNORE.` |
|      - | 1674 | ` *   Expand the value of JSON_INVALID_UTF8_IGNORE defined in ph7Int.h.` |
|      - | 1675 | ` */` |
|     24 | 1676 | `static void PH7_JSON_INVALID_UTF8_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1677 | `{` |
|     12 | 1678 | `	SXUNUSED(pUserData); /* cc warning */` |
|     25 | 1679 | `	ph7_value_int(pVal,JSON_INVALID_UTF8_IGNORE);` |
|     25 | 1680 | `}` |
|      - | 1681 | `/*` |
|      - | 1682 | ` * JSON_INVALID_UTF8_SUBSTITUTE.` |
|      - | 1683 | ` *   Expand the value of JSON_INVALID_UTF8_SUBSTITUTE defined in ph7Int.h.` |
|      - | 1684 | ` */` |
|     30 | 1685 | `static void PH7_JSON_INVALID_UTF8_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1686 | `{` |
|     15 | 1687 | `	SXUNUSED(pUserData); /* cc warning */` |
|     31 | 1688 | `	ph7_value_int(pVal,JSON_INVALID_UTF8_SUBSTITUTE);` |
|     31 | 1689 | `}` |
|      - | 1690 | `/*` |
|      - | 1691 | ` * JSON_THROW_ON_ERROR.` |
|      - | 1692 | ` *   Expand the value of JSON_THROW_ON_ERROR defined in ph7Int.h.` |
|      - | 1693 | ` */` |
|     16 | 1694 | `static void PH7_JSON_THROW_ON_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1695 | `{` |
|      8 | 1696 | `	SXUNUSED(pUserData); /* cc warning */` |
|     18 | 1697 | `	ph7_value_int(pVal,JSON_THROW_ON_ERROR);` |
|     18 | 1698 | `}` |
|      - | 1699 | `/*` |
|      - | 1700 | ` * JSON_ERROR_NONE.` |
|      - | 1701 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|      - | 1702 | ` */` |
|      4 | 1703 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1704 | `{` |
|      2 | 1705 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1706 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      5 | 1707 | `}` |
|      - | 1708 | `/*` |
|      - | 1709 | ` * JSON_ERROR_DEPTH.` |
|      - | 1710 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|      - | 1711 | ` */` |
|      2 | 1712 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1713 | `{` |
|      1 | 1714 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1715 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      3 | 1716 | `}` |
|      - | 1717 | `/*` |
|      - | 1718 | ` * JSON_ERROR_STATE_MISMATCH.` |
|      - | 1719 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|      - | 1720 | ` */` |
|      2 | 1721 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1722 | `{` |
|      1 | 1723 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1724 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      3 | 1725 | `}` |
|      - | 1726 | `/*` |
|      - | 1727 | ` * JSON_ERROR_CTRL_CHAR.` |
|      - | 1728 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|      - | 1729 | ` */` |
|      2 | 1730 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1731 | `{` |
|      1 | 1732 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1733 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      3 | 1734 | `}` |
|      - | 1735 | `/*` |
|      - | 1736 | ` * JSON_ERROR_SYNTAX.` |
|      - | 1737 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|      - | 1738 | ` */` |
|      4 | 1739 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1740 | `{` |
|      2 | 1741 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1742 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      5 | 1743 | `}` |
|      - | 1744 | `/*` |
|      - | 1745 | ` * JSON_ERROR_UTF8.` |
|      - | 1746 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|      - | 1747 | ` */` |
|      2 | 1748 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1749 | `{` |
|      1 | 1750 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1751 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      3 | 1752 | `}` |
|      - | 1753 | `/*` |
|      - | 1754 | ` * JSON_ERROR_UTF16.` |
|      - | 1755 | ` *   Expand the value of JSON_ERROR_UTF16 defined in ph7Int.h.` |
|      - | 1756 | ` */` |
|      4 | 1757 | `static void PH7_JSON_ERROR_UTF16_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1758 | `{` |
|      2 | 1759 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1760 | `	ph7_value_int(pVal,JSON_ERROR_UTF16);` |
|      5 | 1761 | `}` |
|      - | 1762 | `/*` |
|      - | 1763 | ` * JSON_ERROR_NON_BACKED_ENUM.` |
|      - | 1764 | ` *   Expand the value of JSON_ERROR_NON_BACKED_ENUM defined in ph7Int.h (php 8.1).` |
|      - | 1765 | ` */` |
|    ! 0 | 1766 | `static void PH7_JSON_ERROR_NON_BACKED_ENUM_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 1767 | `{` |
|    ! 0 | 1768 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 1769 | `	ph7_value_int(pVal,JSON_ERROR_NON_BACKED_ENUM);` |
|    ! 0 | 1770 | `}` |
|      - | 1771 | `/*` |
|      - | 1772 | ` * JSON_ERROR_INF_OR_NAN.` |
|      - | 1773 | ` *   Expand the value of JSON_ERROR_INF_OR_NAN defined in ph7Int.h.` |
|      - | 1774 | ` */` |
|      2 | 1775 | `static void PH7_JSON_ERROR_INF_OR_NAN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1776 | `{` |
|      1 | 1777 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1778 | `	ph7_value_int(pVal,JSON_ERROR_INF_OR_NAN);` |
|      3 | 1779 | `}` |
|      - | 1780 | `/*` |
|      - | 1781 | ` * __CLASS__` |
|      - | 1782 | ` *  The current class name, or the EMPTY STRING outside any class — php answers "",` |
|      - | 1783 | `` *  not null (`__CLASS__ === ""` is true in global scope). `self` keeps its own`` |
|      - | 1784 | ` *  expander below because php treats IT differently outside a class scope.` |
|      - | 1785 | ` */` |
|      8 | 1786 | `static void PH7_class_magic_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1787 | `{` |
|      9 | 1788 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1789 | `	ph7_class *pClass;` |
|      9 | 1790 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      9 | 1791 | `	if( pClass == 0 ){` |
|      3 | 1792 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      1 | 1793 | `	}` |
|      9 | 1794 | `	if( pClass ){` |
|      7 | 1795 | `		SyString *pName = &pClass->sName;` |
|      7 | 1796 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      4 | 1797 | `	}else{` |
|      3 | 1798 | `		ph7_value_string(pVal,"",0);` |
|      - | 1799 | `	}` |
|      9 | 1800 | `}` |
|      - | 1801 |  |
|      - | 1802 | `/*` |
|      - | 1803 | ` * PASSWORD_BCRYPT / PASSWORD_DEFAULT` |
|      - | 1804 | ` *  The bcrypt algorithm identifier (PHP 7.4+ exposes these as the string "2y").` |
|      - | 1805 | ` *  PASSWORD_DEFAULT tracks the recommended default, currently bcrypt.` |
|      - | 1806 | ` */` |
|     20 | 1807 | `static void PH7_PASSWORD_BCRYPT_Const(ph7_value *pVal,void *pUnused)` |
|      2 | 1808 | `{` |
|     10 | 1809 | `	SXUNUSED(pUnused);` |
|     22 | 1810 | `	ph7_value_string(pVal,"2y",(int)sizeof("2y")-1);` |
|     22 | 1811 | `}` |
|      - | 1812 | `/*` |
|      - | 1813 | ` * PASSWORD_BCRYPT_DEFAULT_COST` |
|      - | 1814 | ` *  The default bcrypt work factor used by password_hash() (currently 12).` |
|      - | 1815 | ` */` |
|      2 | 1816 | `static void PH7_PASSWORD_COST_Const(ph7_value *pVal,void *pUnused)` |
|      1 | 1817 | `{` |
|      1 | 1818 | `	SXUNUSED(pUnused);` |
|      3 | 1819 | `	ph7_value_int(pVal,12);` |
|      3 | 1820 | `}` |
|      - | 1821 | `/*` |
|      - | 1822 | ` * filter_var() filter and flag identifiers (the ext/filter constants). Values` |
|      - | 1823 | ` * match PHP 8.5. One tiny int-returning callback per constant, generated by a` |
|      - | 1824 | ` * local macro to keep the ~25 near-identical definitions DRY.` |
|      - | 1825 | ` */` |
|      - | 1826 | `#define PH7_FILTER_INT_CONST(Name,Val) \` |
|      - | 1827 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|      - | 1828 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|      - | 1829 | `	}` |
|     10 | 1830 | `PH7_FILTER_INT_CONST(FILTER_DEFAULT,516)` |
|     17 | 1831 | `PH7_FILTER_INT_CONST(FILTER_UNSAFE_RAW,516)` |
|     64 | 1832 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_INT,257)` |
|     29 | 1833 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_BOOLEAN,258)` |
|     69 | 1834 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_FLOAT,259)` |
|      8 | 1835 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_REGEXP,272)` |
|     11 | 1836 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_DOMAIN,277)` |
|     15 | 1837 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_URL,273)` |
|     28 | 1838 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_EMAIL,274)` |
|     25 | 1839 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_IP,275)` |
|     11 | 1840 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_MAC,276)` |
|      3 | 1841 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_INT,519)` |
|      5 | 1842 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_FLOAT,520)` |
|     13 | 1843 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_SPECIAL_CHARS,515)` |
|     25 | 1844 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_FULL_SPECIAL_CHARS,522)` |
|      3 | 1845 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_EMAIL,517)` |
|      3 | 1846 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_URL,518)` |
|      3 | 1847 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_OCTAL,1)` |
|      3 | 1848 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_HEX,2)` |
|      7 | 1849 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_LOW,4)` |
|      5 | 1850 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_HIGH,8)` |
|      5 | 1851 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_LOW,16)` |
|      5 | 1852 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_HIGH,32)` |
|      3 | 1853 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_AMP,64)` |
|      3 | 1854 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_ENCODE_QUOTES,128)` |
|      3 | 1855 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_BACKTICK,512)` |
|      3 | 1856 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_FRACTION,4096)` |
|     25 | 1857 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_THOUSAND,8192)` |
|      3 | 1858 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_SCIENTIFIC,16384)` |
|      5 | 1859 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV4,1048576)` |
|      3 | 1860 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV6,2097152)` |
|     14 | 1861 | `PH7_FILTER_INT_CONST(FILTER_NULL_ON_FAILURE,134217728)` |
|      - | 1862 | `/* filter_input() source selectors (php values; SESSION/REQUEST are undefined in 8.5) */` |
|      5 | 1863 | `PH7_FILTER_INT_CONST(INPUT_POST,0)` |
|      8 | 1864 | `PH7_FILTER_INT_CONST(INPUT_GET,1)` |
|      3 | 1865 | `PH7_FILTER_INT_CONST(INPUT_COOKIE,2)` |
|      3 | 1866 | `PH7_FILTER_INT_CONST(INPUT_ENV,4)` |
|     21 | 1867 | `PH7_FILTER_INT_CONST(INPUT_SERVER,5)` |
|      - | 1868 | `/*` |
|      - | 1869 | ` * Table of built-in constants.` |
|      - | 1870 | ` */` |
|      - | 1871 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|      - | 1872 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|      - | 1873 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|      - | 1874 | `	{"__PH7__",              PH7_VER_Const      },` |
|      - | 1875 | `	{"PHP_VERSION",          PH7_PHPVerConst    },` |
|      - | 1876 | `	{"PHP_MAJOR_VERSION",    PH7_PHPMajorConst  },` |
|      - | 1877 | `	{"PHP_MINOR_VERSION",    PH7_PHPMinorConst  },` |
|      - | 1878 | `	{"PHP_RELEASE_VERSION",  PH7_PHPReleaseConst},` |
|      - | 1879 | `	{"PHP_EXTRA_VERSION",    PH7_PHPExtraConst  },` |
|      - | 1880 | `	{"PHP_VERSION_ID",       PH7_PHPVerIdConst  },` |
|      - | 1881 | `	{"PHP_OS",               PH7_OS_Const       },` |
|      - | 1882 | `	{"PHP_OS_FAMILY",        PH7_OS_FAMILY_Const},` |
|      - | 1883 | `	{"PHP_SAPI",             PH7_SAPI_Const     },` |
|      - | 1884 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|      - | 1885 | `	{"PHP_SESSION_DISABLED", PH7_PHP_SESSION_DISABLED_Const },` |
|      - | 1886 | `	{"PHP_SESSION_NONE",     PH7_PHP_SESSION_NONE_Const },` |
|      - | 1887 | `	{"PHP_SESSION_ACTIVE",   PH7_PHP_SESSION_ACTIVE_Const },` |
|      - | 1888 | `	{"INI_USER",             PH7_INI_USER_Const },` |
|      - | 1889 | `	{"INI_PERDIR",           PH7_INI_PERDIR_Const },` |
|      - | 1890 | `	{"INI_SYSTEM",           PH7_INI_SYSTEM_Const },` |
|      - | 1891 | `	{"INI_ALL",              PH7_INI_ALL_Const },` |
|      - | 1892 | `	{"MB_CASE_UPPER",        PH7_MB_CASE_UPPER_Const },` |
|      - | 1893 | `	{"MB_CASE_LOWER",        PH7_MB_CASE_LOWER_Const },` |
|      - | 1894 | `	{"MB_CASE_TITLE",        PH7_MB_CASE_TITLE_Const },` |
|      - | 1895 | `	{"PASSWORD_BCRYPT",      PH7_PASSWORD_BCRYPT_Const },` |
|      - | 1896 | `	{"PASSWORD_DEFAULT",     PH7_PASSWORD_BCRYPT_Const },` |
|      - | 1897 | `	{"PASSWORD_BCRYPT_DEFAULT_COST", PH7_PASSWORD_COST_Const },` |
|      - | 1898 | `	{"FILTER_DEFAULT",              PH7_FILTER_DEFAULT_Const },` |
|      - | 1899 | `	{"FILTER_UNSAFE_RAW",           PH7_FILTER_UNSAFE_RAW_Const },` |
|      - | 1900 | `	{"FILTER_VALIDATE_INT",         PH7_FILTER_VALIDATE_INT_Const },` |
|      - | 1901 | `	{"FILTER_VALIDATE_BOOLEAN",     PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 1902 | `	{"FILTER_VALIDATE_BOOL",        PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 1903 | `	{"FILTER_VALIDATE_FLOAT",       PH7_FILTER_VALIDATE_FLOAT_Const },` |
|      - | 1904 | `	{"FILTER_VALIDATE_REGEXP",      PH7_FILTER_VALIDATE_REGEXP_Const },` |
|      - | 1905 | `	{"FILTER_VALIDATE_DOMAIN",      PH7_FILTER_VALIDATE_DOMAIN_Const },` |
|      - | 1906 | `	{"FILTER_VALIDATE_URL",         PH7_FILTER_VALIDATE_URL_Const },` |
|      - | 1907 | `	{"FILTER_VALIDATE_EMAIL",       PH7_FILTER_VALIDATE_EMAIL_Const },` |
|      - | 1908 | `	{"FILTER_VALIDATE_IP",          PH7_FILTER_VALIDATE_IP_Const },` |
|      - | 1909 | `	{"FILTER_VALIDATE_MAC",         PH7_FILTER_VALIDATE_MAC_Const },` |
|      - | 1910 | `	{"FILTER_SANITIZE_NUMBER_INT",  PH7_FILTER_SANITIZE_NUMBER_INT_Const },` |
|      - | 1911 | `	{"FILTER_SANITIZE_NUMBER_FLOAT",PH7_FILTER_SANITIZE_NUMBER_FLOAT_Const },` |
|      - | 1912 | `	{"FILTER_SANITIZE_SPECIAL_CHARS",PH7_FILTER_SANITIZE_SPECIAL_CHARS_Const },` |
|      - | 1913 | `	{"FILTER_SANITIZE_FULL_SPECIAL_CHARS",PH7_FILTER_SANITIZE_FULL_SPECIAL_CHARS_Const },` |
|      - | 1914 | `	{"FILTER_SANITIZE_EMAIL",       PH7_FILTER_SANITIZE_EMAIL_Const },` |
|      - | 1915 | `	{"FILTER_SANITIZE_URL",         PH7_FILTER_SANITIZE_URL_Const },` |
|      - | 1916 | `	{"FILTER_FLAG_ALLOW_OCTAL",     PH7_FILTER_FLAG_ALLOW_OCTAL_Const },` |
|      - | 1917 | `	{"FILTER_FLAG_ALLOW_HEX",       PH7_FILTER_FLAG_ALLOW_HEX_Const },` |
|      - | 1918 | `	{"FILTER_FLAG_STRIP_LOW",       PH7_FILTER_FLAG_STRIP_LOW_Const },` |
|      - | 1919 | `	{"FILTER_FLAG_STRIP_HIGH",      PH7_FILTER_FLAG_STRIP_HIGH_Const },` |
|      - | 1920 | `	{"FILTER_FLAG_ENCODE_LOW",      PH7_FILTER_FLAG_ENCODE_LOW_Const },` |
|      - | 1921 | `	{"FILTER_FLAG_ENCODE_HIGH",     PH7_FILTER_FLAG_ENCODE_HIGH_Const },` |
|      - | 1922 | `	{"FILTER_FLAG_ENCODE_AMP",      PH7_FILTER_FLAG_ENCODE_AMP_Const },` |
|      - | 1923 | `	{"FILTER_FLAG_NO_ENCODE_QUOTES",PH7_FILTER_FLAG_NO_ENCODE_QUOTES_Const },` |
|      - | 1924 | `	{"FILTER_FLAG_STRIP_BACKTICK",  PH7_FILTER_FLAG_STRIP_BACKTICK_Const },` |
|      - | 1925 | `	{"FILTER_FLAG_ALLOW_FRACTION",  PH7_FILTER_FLAG_ALLOW_FRACTION_Const },` |
|      - | 1926 | `	{"FILTER_FLAG_ALLOW_THOUSAND",  PH7_FILTER_FLAG_ALLOW_THOUSAND_Const },` |
|      - | 1927 | `	{"FILTER_FLAG_ALLOW_SCIENTIFIC",PH7_FILTER_FLAG_ALLOW_SCIENTIFIC_Const },` |
|      - | 1928 | `	{"FILTER_FLAG_IPV4",            PH7_FILTER_FLAG_IPV4_Const },` |
|      - | 1929 | `	{"FILTER_FLAG_IPV6",            PH7_FILTER_FLAG_IPV6_Const },` |
|      - | 1930 | `	{"FILTER_NULL_ON_FAILURE",      PH7_FILTER_NULL_ON_FAILURE_Const },` |
|      - | 1931 | `	{"INPUT_POST",                  PH7_INPUT_POST_Const },` |
|      - | 1932 | `	{"INPUT_GET",                   PH7_INPUT_GET_Const },` |
|      - | 1933 | `	{"INPUT_COOKIE",                PH7_INPUT_COOKIE_Const },` |
|      - | 1934 | `	{"INPUT_ENV",                   PH7_INPUT_ENV_Const },` |
|      - | 1935 | `	{"INPUT_SERVER",                PH7_INPUT_SERVER_Const },` |
|      - | 1936 | `	{"CAL_GREGORIAN",        PH7_CAL_GREGORIAN_Const },` |
|      - | 1937 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|      - | 1938 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|      - | 1939 | `	{"PHP_INT_MIN",          PH7_INTMIN_Const   },` |
|      - | 1940 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|      - | 1941 | `	{"PHP_FLOAT_EPSILON",    PH7_FLOATEPSILON_Const },` |
|      - | 1942 | `	{"PHP_FLOAT_MAX",        PH7_FLOATMAX_Const },` |
|      - | 1943 | `	{"PHP_FLOAT_MIN",        PH7_FLOATMIN_Const },` |
|      - | 1944 | `	{"PHP_FLOAT_DIG",        PH7_FLOATDIG_Const },` |
|      - | 1945 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|      - | 1946 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|      - | 1947 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|      - | 1948 | `	{"__TIME__",             PH7_TIME_Const     },` |
|      - | 1949 | `	{"__DATE__",             PH7_DATE_Const     },` |
|      - | 1950 | `	{"__FILE__",             PH7_FILE_Const     },` |
|      - | 1951 | `	{"__DIR__",              PH7_DIR_Const      },` |
|      - | 1952 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|      - | 1953 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|      - | 1954 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|      - | 1955 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|      - | 1956 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|      - | 1957 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|      - | 1958 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|      - | 1959 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|      - | 1960 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|      - | 1961 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|      - | 1962 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|      - | 1963 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|      - | 1964 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|      - | 1965 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|      - | 1966 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|      - | 1967 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|      - | 1968 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|      - | 1969 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|      - | 1970 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|      - | 1971 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|      - | 1972 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|      - | 1973 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|      - | 1974 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|      - | 1975 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|      - | 1976 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|      - | 1977 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|      - | 1978 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|      - | 1979 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|      - | 1980 | `	{"SORT_LOCALE_STRING",   PH7_SORT_LOCALE_STRING_Const },` |
|      - | 1981 | `	{"SORT_NATURAL",         PH7_SORT_NATURAL_Const },` |
|      - | 1982 | `	{"SORT_FLAG_CASE",       PH7_SORT_FLAG_CASE_Const },` |
|      - | 1983 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|      - | 1984 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|      - | 1985 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|      - | 1986 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|      - | 1987 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|      - | 1988 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|      - | 1989 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 1990 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|      - | 1991 | `	{"M_E",                  PH7_M_E_Const          },` |
|      - | 1992 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|      - | 1993 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|      - | 1994 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|      - | 1995 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|      - | 1996 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|      - | 1997 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|      - | 1998 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|      - | 1999 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|      - | 2000 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|      - | 2001 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|      - | 2002 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|      - | 2003 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|      - | 2004 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|      - | 2005 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|      - | 2006 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|      - | 2007 | `	{"NAN",                  PH7_NAN_Const          },` |
|      - | 2008 | `	{"INF",                  PH7_INF_Const          },` |
|      - | 2009 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 2010 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|      - | 2011 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|      - | 2012 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|      - | 2013 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|      - | 2014 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|      - | 2015 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|      - | 2016 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|      - | 2017 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|      - | 2018 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|      - | 2019 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|      - | 2020 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|      - | 2021 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|      - | 2022 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|      - | 2023 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|      - | 2024 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|      - | 2025 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|      - | 2026 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|      - | 2027 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|      - | 2028 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|      - | 2029 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|      - | 2030 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|      - | 2031 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|      - | 2032 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|      - | 2033 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|      - | 2034 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|      - | 2035 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|      - | 2036 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|      - | 2037 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|      - | 2038 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|      - | 2039 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|      - | 2040 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|      - | 2041 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|      - | 2042 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|      - | 2043 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|      - | 2044 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|      - | 2045 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|      - | 2046 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|      - | 2047 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|      - | 2048 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|      - | 2049 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|      - | 2050 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|      - | 2051 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|      - | 2052 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|      - | 2053 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|      - | 2054 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|      - | 2055 | `	{"PATHINFO_ALL",         PH7_PATHINFO_ALL_Const },` |
|      - | 2056 | `	/* ASSERT_QUIET_EVAL was REMOVED in php 8.0: referencing it is an Error there */` |
|      - | 2057 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|      - | 2058 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|      - | 2059 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|      - | 2060 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|      - | 2061 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|      - | 2062 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|      - | 2063 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|      - | 2064 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|      - | 2065 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|      - | 2066 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|      - | 2067 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|      - | 2068 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|      - | 2069 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|      - | 2070 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|      - | 2071 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|      - | 2072 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|      - | 2073 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|      - | 2074 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|      - | 2075 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|      - | 2076 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|      - | 2077 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|      - | 2078 | `	{"STDIN",                PH7_STDIN_Const        },` |
|      - | 2079 | `	{"stdin",                PH7_STDIN_Const        },` |
|      - | 2080 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|      - | 2081 | `	{"stdout",               PH7_STDOUT_Const       },` |
|      - | 2082 | `	{"STDERR",               PH7_STDERR_Const       },` |
|      - | 2083 | `	{"stderr",               PH7_STDERR_Const       },` |
|      - | 2084 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|      - | 2085 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|      - | 2086 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|      - | 2087 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|      - | 2088 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|      - | 2089 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|      - | 2090 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|      - | 2091 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|      - | 2092 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|      - | 2093 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|      - | 2094 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|      - | 2095 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|      - | 2096 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|      - | 2097 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|      - | 2098 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|      - | 2099 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|      - | 2100 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|      - | 2101 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|      - | 2102 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|      - | 2103 | `	{"JSON_UNESCAPED_LINE_TERMINATORS", PH7_JSON_UNESCAPED_LINE_TERMINATORS_Const},` |
|      - | 2104 | `	{"JSON_INVALID_UTF8_IGNORE", PH7_JSON_INVALID_UTF8_IGNORE_Const},` |
|      - | 2105 | `	{"JSON_INVALID_UTF8_SUBSTITUTE", PH7_JSON_INVALID_UTF8_SUBSTITUTE_Const},` |
|      - | 2106 | `	{"JSON_THROW_ON_ERROR",    PH7_JSON_THROW_ON_ERROR_Const},` |
|      - | 2107 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|      - | 2108 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|      - | 2109 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|      - | 2110 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|      - | 2111 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|      - | 2112 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|      - | 2113 | `	{"JSON_ERROR_UTF16",     PH7_JSON_ERROR_UTF16_Const},` |
|      - | 2114 | `	{"JSON_ERROR_NON_BACKED_ENUM", PH7_JSON_ERROR_NON_BACKED_ENUM_Const},` |
|      - | 2115 | `	{"JSON_ERROR_INF_OR_NAN", PH7_JSON_ERROR_INF_OR_NAN_Const},` |
|      - | 2116 | ``	/* `self`, `parent` and `static` are KEYWORDS in php, not constants: using one as a bare`` |
|      - | 2117 | ``	 * word is an "Undefined constant" Error (or a parse error for `static`). PH7 registered`` |
|      - | 2118 | `	 * them as constants that quietly expanded to the class name / NULL, so a typo'd bare` |
|      - | 2119 | ``	 * word silently produced a value. The `self::`/`parent::`/`static::` forms are handled`` |
|      - | 2120 | ``	 * by the `::` compile path and do not go through the constant table. */`` |
|      - | 2121 | `	{"__CLASS__",            PH7_class_magic_Const  }` |
|      - | 2122 | `};` |
|      - | 2123 | `/*` |
|      - | 2124 | ` * Register the built-in constants defined above.` |
|      - | 2125 | ` */` |
|   3956 | 2126 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|      5 | 2127 | `{` |
|      - | 2128 | `	sxu32 n;` |
|      - | 2129 | `	/*` |
|      - | 2130 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|      - | 2131 | `	 * that trigger the constant invocation as their private data.` |
|      - | 2132 | `	 */` |
| 961313 | 2133 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 957357 | 2134 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
| 478681 | 2135 | `	}` |
|   3961 | 2136 | `}` |
