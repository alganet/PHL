# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1062/1117 lines (95.08%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `#include <float.h> /* DBL_EPSILON/DBL_MAX/DBL_MIN/DBL_DIG for the PHP_FLOAT_* constants */` |
|       - |    8 | `/* This file implement built-in constants for the PH7 engine. */` |
|       - |    9 | `/*` |
|       - |   10 | ` * PH7_VERSION` |
|       - |   11 | ` * __PH7__` |
|       - |   12 | ` *   Expand the current version of the PH7 engine.` |
|       - |   13 | ` */` |
|       8 |   14 | `static void PH7_VER_Const(ph7_value *pVal,void *pUnused)` |
|       1 |   15 | `{` |
|       4 |   16 | `	SXUNUSED(pUnused);` |
|       9 |   17 | `	ph7_value_string(pVal,ph7_lib_signature(),-1/*Compute length automatically*/);` |
|       9 |   18 | `}` |
|       - |   19 | `/*` |
|       - |   20 | ` * PHP_VERSION, PHP_MAJOR_VERSION, PHP_MINOR_VERSION, PHP_RELEASE_VERSION,` |
|       - |   21 | ` * PHP_EXTRA_VERSION, PHP_VERSION_ID` |
|       - |   22 | ` *   Expand the PHP-compatibility version PHL advertises (see PHP_COMPAT_* in ph7.h).` |
|       - |   23 | ` */` |
|       4 |   24 | `static void PH7_PHPVerConst(ph7_value *pVal,void *pUnused)` |
|       1 |   25 | `{` |
|       2 |   26 | `	SXUNUSED(pUnused);` |
|       5 |   27 | `	ph7_value_string(pVal,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION)-1);` |
|       5 |   28 | `}` |
|       4 |   29 | `static void PH7_PHPMajorConst(ph7_value *pVal,void *pUnused)` |
|       1 |   30 | `{` |
|       2 |   31 | `	SXUNUSED(pUnused);` |
|       5 |   32 | `	ph7_value_int64(pVal,PHP_COMPAT_MAJOR_VERSION);` |
|       5 |   33 | `}` |
|       4 |   34 | `static void PH7_PHPMinorConst(ph7_value *pVal,void *pUnused)` |
|       1 |   35 | `{` |
|       2 |   36 | `	SXUNUSED(pUnused);` |
|       5 |   37 | `	ph7_value_int64(pVal,PHP_COMPAT_MINOR_VERSION);` |
|       5 |   38 | `}` |
|       4 |   39 | `static void PH7_PHPReleaseConst(ph7_value *pVal,void *pUnused)` |
|       1 |   40 | `{` |
|       2 |   41 | `	SXUNUSED(pUnused);` |
|       5 |   42 | `	ph7_value_int64(pVal,PHP_COMPAT_RELEASE_VERSION);` |
|       5 |   43 | `}` |
|       2 |   44 | `static void PH7_PHPExtraConst(ph7_value *pVal,void *pUnused)` |
|       1 |   45 | `{` |
|       1 |   46 | `	SXUNUSED(pUnused);` |
|       3 |   47 | `	ph7_value_string(pVal,PHP_COMPAT_EXTRA_VERSION,(int)sizeof(PHP_COMPAT_EXTRA_VERSION)-1);` |
|       3 |   48 | `}` |
|       4 |   49 | `static void PH7_PHPVerIdConst(ph7_value *pVal,void *pUnused)` |
|       1 |   50 | `{` |
|       2 |   51 | `	SXUNUSED(pUnused);` |
|       5 |   52 | `	ph7_value_int64(pVal,PHP_COMPAT_VERSION_ID);` |
|       5 |   53 | `}` |
|       - |   54 | `#ifdef __WINNT__` |
|       - |   55 | `#include <Windows.h>` |
|       - |   56 | `#elif defined(__UNIXES__)` |
|       - |   57 | `#include <sys/utsname.h>` |
|       - |   58 | `#endif` |
|       - |   59 | `/*` |
|       - |   60 | ` * PHP_OS` |
|       - |   61 | ` *  Expand the name of the host Operating System.` |
|       - |   62 | ` */` |
|    4752 |   63 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|       5 |   64 | `{` |
|       - |   65 | `#if defined(__WINNT__)` |
|       5 |   66 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|       - |   67 | `#elif defined(__UNIXES__)` |
|       - |   68 | `	struct utsname sInfo;` |
|    4752 |   69 | `	if( uname(&sInfo) != 0 ){` |
|     ! 0 |   70 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|     ! 0 |   71 | `	}else{` |
|    4752 |   72 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|       - |   73 | `	}` |
|       - |   74 | `#else` |
|       - |   75 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|       - |   76 | `#endif` |
|    2376 |   77 | `	SXUNUSED(pUnused);` |
|    4757 |   78 | `}` |
|       - |   79 | `/*` |
|       - |   80 | ` * PHP_OS_FAMILY (php 7.2)` |
|       - |   81 | ` *  One of 'Windows', 'BSD', 'Darwin', 'Solaris', 'Linux' or 'Unknown', derived` |
|       - |   82 | ` *  from the host's uname sysname (php maps the same set at build time).` |
|       - |   83 | ` */` |
|      22 |   84 | `static void PH7_OS_FAMILY_Const(ph7_value *pVal,void *pUnused)` |
|       4 |   85 | `{` |
|      11 |   86 | `	SXUNUSED(pUnused);` |
|       - |   87 | `#if defined(__WINNT__)` |
|       4 |   88 | `	ph7_value_string(pVal,"Windows",(int)sizeof("Windows")-1);` |
|       - |   89 | `#elif defined(__UNIXES__)` |
|       - |   90 | `	struct utsname sInfo;` |
|      22 |   91 | `	const char *zFamily = "Unknown";` |
|      22 |   92 | `	if( uname(&sInfo) == 0 ){` |
|      22 |   93 | `		const char *z = sInfo.sysname;` |
|      22 |   94 | `		if( SyStrnicmp(z,"Darwin",sizeof("Darwin")-1) == 0 ){` |
|      11 |   95 | `			zFamily = "Darwin";` |
|      22 |   96 | `		}else if( SyStrnicmp(z,"Linux",sizeof("Linux")-1) == 0 ){` |
|      11 |   97 | `			zFamily = "Linux";` |
|     ! 0 |   98 | `		}else if( SyStrnicmp(z,"SunOS",sizeof("SunOS")-1) == 0 ){` |
|     ! 0 |   99 | `			zFamily = "Solaris";` |
|     ! 0 |  100 | `		}else{` |
|       - |  101 | `			/* FreeBSD/OpenBSD/NetBSD/DragonFly -> 'BSD' (scan for "BSD"). */` |
|     ! 0 |  102 | `			const char *p = z;` |
|     ! 0 |  103 | `			while( p[0] && p[1] && p[2] ){` |
|     ! 0 |  104 | `				if( (p[0]=='B'\|\|p[0]=='b') && (p[1]=='S'\|\|p[1]=='s') && (p[2]=='D'\|\|p[2]=='d') ){` |
|     ! 0 |  105 | `					zFamily = "BSD";` |
|     ! 0 |  106 | `					break;` |
|       - |  107 | `				}` |
|     ! 0 |  108 | `				p++;` |
|       - |  109 | `			}` |
|       - |  110 | `		}` |
|      11 |  111 | `	}` |
|      22 |  112 | `	ph7_value_string(pVal,zFamily,-1);` |
|       - |  113 | `#else` |
|       - |  114 | `	ph7_value_string(pVal,"Unknown",(int)sizeof("Unknown")-1);` |
|       - |  115 | `#endif` |
|      26 |  116 | `}` |
|       - |  117 | `/*` |
|       - |  118 | ` * PHP_SAPI` |
|       - |  119 | ` *  The interface between the interpreter and the host. PHL's host binary is a` |
|       - |  120 | ` *  command-line interpreter, so this is "cli" (matching the CLI default of` |
|       - |  121 | ` *  php_sapi_name(); the built-in -S server's per-request "cli-server" flavour is` |
|       - |  122 | ` *  only surfaced by php_sapi_name(), not this compile-time constant).` |
|       - |  123 | ` */` |
|     ! 0 |  124 | `static void PH7_SAPI_Const(ph7_value *pVal,void *pUnused)` |
|     ! 0 |  125 | `{` |
|     ! 0 |  126 | `	SXUNUSED(pUnused);` |
|     ! 0 |  127 | `	ph7_value_string(pVal,"cli",(int)sizeof("cli")-1);` |
|     ! 0 |  128 | `}` |
|       - |  129 | `/*` |
|       - |  130 | ` * PHP_EOL` |
|       - |  131 | ` *  Expand the correct 'End Of Line' symbol for this platform.` |
|       - |  132 | ` */` |
|     834 |  133 | `static void PH7_EOL_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  134 | `{` |
|     417 |  135 | `	SXUNUSED(pUnused);` |
|       - |  136 | `#ifdef __WINNT__` |
|       4 |  137 | `	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);` |
|       - |  138 | `#else` |
|     834 |  139 | `	ph7_value_string(pVal,"\n",(int)sizeof(char));` |
|       - |  140 | `#endif` |
|     838 |  141 | `}` |
|       - |  142 | `/*` |
|       - |  143 | ` * PHP_INT_MAX` |
|       - |  144 | ` * Expand the largest integer supported.` |
|       - |  145 | ` * Note that PH7 deals with 64-bit integer for all platforms.` |
|       - |  146 | ` */` |
|     166 |  147 | `static void PH7_INTMAX_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  148 | `{` |
|      83 |  149 | `	SXUNUSED(pUnused);` |
|     170 |  150 | `	ph7_value_int64(pVal,SXI64_HIGH);` |
|     170 |  151 | `}` |
|       - |  152 | `/* ext/calendar: the only calendar cal_days_in_month() is asked for in practice. */` |
|       4 |  153 | `static void PH7_CAL_GREGORIAN_Const(ph7_value *pVal,void *pUnused)` |
|       1 |  154 | `{` |
|       2 |  155 | `	SXUNUSED(pUnused);` |
|       5 |  156 | `	ph7_value_int(pVal,0);` |
|       5 |  157 | `}` |
|       - |  158 | `/*` |
|       - |  159 | ` * PHP_INT_MIN (php 7.0)` |
|       - |  160 | ` * Expand the smallest integer supported.` |
|       - |  161 | ` */` |
|      84 |  162 | `static void PH7_INTMIN_Const(ph7_value *pVal,void *pUnused)` |
|       2 |  163 | `{` |
|      42 |  164 | `	SXUNUSED(pUnused);` |
|      86 |  165 | `	ph7_value_int64(pVal,SMALLEST_INT64);` |
|      86 |  166 | `}` |
|       - |  167 | `/*` |
|       - |  168 | ` * PHP_INT_SIZE` |
|       - |  169 | ` * Expand the size in bytes of a 64-bit integer.` |
|       - |  170 | ` */` |
|       4 |  171 | `static void PH7_INTSIZE_Const(ph7_value *pVal,void *pUnused)` |
|       1 |  172 | `{` |
|       2 |  173 | `	SXUNUSED(pUnused);` |
|       5 |  174 | `	ph7_value_int64(pVal,sizeof(sxi64));` |
|       5 |  175 | `}` |
|       - |  176 | `/*` |
|       - |  177 | ` * PHP_FLOAT_EPSILON / PHP_FLOAT_MAX / PHP_FLOAT_MIN / PHP_FLOAT_DIG (php 7.2)` |
|       - |  178 | ` * Double-precision characteristics, sourced from <float.h> exactly like php` |
|       - |  179 | ` * so they track the compiling platform's actual double representation.` |
|       - |  180 | ` */` |
|       4 |  181 | `static void PH7_FLOATEPSILON_Const(ph7_value *pVal,void *pUnused)` |
|       1 |  182 | `{` |
|       2 |  183 | `	SXUNUSED(pUnused);` |
|       5 |  184 | `	ph7_value_double(pVal,DBL_EPSILON);` |
|       5 |  185 | `}` |
|       2 |  186 | `static void PH7_FLOATMAX_Const(ph7_value *pVal,void *pUnused)` |
|       1 |  187 | `{` |
|       1 |  188 | `	SXUNUSED(pUnused);` |
|       3 |  189 | `	ph7_value_double(pVal,DBL_MAX);` |
|       3 |  190 | `}` |
|       2 |  191 | `static void PH7_FLOATMIN_Const(ph7_value *pVal,void *pUnused)` |
|       1 |  192 | `{` |
|       1 |  193 | `	SXUNUSED(pUnused);` |
|       3 |  194 | `	ph7_value_double(pVal,DBL_MIN);` |
|       3 |  195 | `}` |
|       2 |  196 | `static void PH7_FLOATDIG_Const(ph7_value *pVal,void *pUnused)` |
|       1 |  197 | `{` |
|       1 |  198 | `	SXUNUSED(pUnused);` |
|       3 |  199 | `	ph7_value_int64(pVal,DBL_DIG);` |
|       3 |  200 | `}` |
|       - |  201 | `/*` |
|       - |  202 | ` * DIRECTORY_SEPARATOR.` |
|       - |  203 | ` * Expand the directory separator character.` |
|       - |  204 | ` */` |
|     386 |  205 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  206 | `{` |
|     193 |  207 | `	SXUNUSED(pUnused);` |
|       - |  208 | `#ifdef __WINNT__` |
|       3 |  209 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|       - |  210 | `#else` |
|     386 |  211 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|       - |  212 | `#endif` |
|     389 |  213 | `}` |
|       - |  214 | `/*` |
|       - |  215 | ` * PATH_SEPARATOR.` |
|       - |  216 | ` * Expand the path separator character.` |
|       - |  217 | ` */` |
|       2 |  218 | `static void PH7_PATHSEP_Const(ph7_value *pVal,void *pUnused)` |
|       1 |  219 | `{` |
|       1 |  220 | `	SXUNUSED(pUnused);` |
|       - |  221 | `#ifdef __WINNT__` |
|       1 |  222 | `	ph7_value_string(pVal,";",(int)sizeof(char));` |
|       - |  223 | `#else` |
|       2 |  224 | `	ph7_value_string(pVal,":",(int)sizeof(char));` |
|       - |  225 | `#endif` |
|       3 |  226 | `}` |
|       - |  227 |  |
|       - |  228 | `#if defined(PH7_ENABLE_MATH_FUNC)` |
|       - |  229 | `/*` |
|       - |  230 | ` * NAN constant: floating-point Not-A-Number` |
|       - |  231 | ` */` |
|     106 |  232 | `static void PH7_NAN_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  233 | `{` |
|      53 |  234 | `	SXUNUSED(pUnused);` |
|     110 |  235 | `	ph7_value_double(pVal, PH7_NAN_VALUE());` |
|     110 |  236 | `}` |
|       - |  237 |  |
|       - |  238 | `/*` |
|       - |  239 | ` * INF constant: positive infinity` |
|       - |  240 | ` */` |
|     120 |  241 | `static void PH7_INF_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  242 | `{` |
|      60 |  243 | `	SXUNUSED(pUnused);` |
|       - |  244 | `	/* similarly avoid the INFINITY macro */` |
|     124 |  245 | `	ph7_value_double(pVal, PH7_INF_VALUE());` |
|     124 |  246 | `}` |
|       - |  247 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|       - |  248 |  |
|       - |  249 | `#ifndef __WINNT__` |
|       - |  250 | `#include <time.h>` |
|       - |  251 | `#endif` |
|       - |  252 | `/*` |
|       - |  253 | ` * __TIME__` |
|       - |  254 | ` *  Expand the current time (GMT).` |
|       - |  255 | ` */` |
|       2 |  256 | `static void PH7_TIME_Const(ph7_value *pVal,void *pUnused)` |
|       1 |  257 | `{` |
|       - |  258 | `	Sytm sTm;` |
|       - |  259 | `#ifdef __WINNT__` |
|       - |  260 | `	SYSTEMTIME sOS;` |
|       1 |  261 | `	GetSystemTime(&sOS);` |
|       1 |  262 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|       - |  263 | `#else` |
|       - |  264 | `	struct tm *pTm;` |
|       - |  265 | `	time_t t;` |
|       2 |  266 | `	time(&t);` |
|       2 |  267 | `	pTm = gmtime(&t);` |
|       2 |  268 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|       - |  269 | `#endif` |
|       1 |  270 | `	SXUNUSED(pUnused); /* cc warning */` |
|       - |  271 | `	/* Expand */` |
|       3 |  272 | `	ph7_value_string_format(pVal,"%02d:%02d:%02d",sTm.tm_hour,sTm.tm_min,sTm.tm_sec);` |
|       3 |  273 | `}` |
|       - |  274 | `/*` |
|       - |  275 | ` * __DATE__` |
|       - |  276 | ` *  Expand the current date in the ISO-8601 format.` |
|       - |  277 | ` */` |
|       2 |  278 | `static void PH7_DATE_Const(ph7_value *pVal,void *pUnused)` |
|       1 |  279 | `{` |
|       - |  280 | `	Sytm sTm;` |
|       - |  281 | `#ifdef __WINNT__` |
|       - |  282 | `	SYSTEMTIME sOS;` |
|       1 |  283 | `	GetSystemTime(&sOS);` |
|       1 |  284 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|       - |  285 | `#else` |
|       - |  286 | `	struct tm *pTm;` |
|       - |  287 | `	time_t t;` |
|       2 |  288 | `	time(&t);` |
|       2 |  289 | `	pTm = gmtime(&t);` |
|       2 |  290 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|       - |  291 | `#endif` |
|       1 |  292 | `	SXUNUSED(pUnused); /* cc warning */` |
|       - |  293 | `	/* Expand */` |
|       3 |  294 | `	ph7_value_string_format(pVal,"%04d-%02d-%02d",sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|       3 |  295 | `}` |
|       - |  296 | `/*` |
|       - |  297 | ` * __FILE__` |
|       - |  298 | ` *  Path of the processed script.` |
|       - |  299 | ` */` |
|     ! 0 |  300 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|     ! 0 |  301 | `{` |
|     ! 0 |  302 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - |  303 | `	SyString *pFile;` |
|       - |  304 | `	/* Peek the top entry */` |
|     ! 0 |  305 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     ! 0 |  306 | `	if( pFile == 0 ){` |
|       - |  307 | `		/* Expand the magic word: ":MEMORY:" */` |
|     ! 0 |  308 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|     ! 0 |  309 | `	}else{` |
|     ! 0 |  310 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|       - |  311 | `	}` |
|     ! 0 |  312 | `}` |
|       - |  313 | `/*` |
|       - |  314 | ` * __DIR__` |
|       - |  315 | ` *  Directory holding the processed script.` |
|       - |  316 | ` */` |
|     ! 0 |  317 | `static void PH7_DIR_Const(ph7_value *pVal,void *pUserData)` |
|     ! 0 |  318 | `{` |
|     ! 0 |  319 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - |  320 | `	SyString *pFile;` |
|       - |  321 | `	/* Peek the top entry */` |
|     ! 0 |  322 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     ! 0 |  323 | `	if( pFile == 0 ){` |
|       - |  324 | `		/* Expand the magic word: ":MEMORY:" */` |
|     ! 0 |  325 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|     ! 0 |  326 | `	}else{` |
|     ! 0 |  327 | `		if( pFile->nByte > 0 ){` |
|       - |  328 | `			const char *zDir;` |
|       - |  329 | `			int nLen;` |
|     ! 0 |  330 | `			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|     ! 0 |  331 | `			ph7_value_string(pVal,zDir,nLen);` |
|     ! 0 |  332 | `		}else{` |
|       - |  333 | `			/* Expand '.' as the current directory*/` |
|     ! 0 |  334 | `			ph7_value_string(pVal,".",(int)sizeof(char));` |
|       - |  335 | `		}` |
|       - |  336 | `	}` |
|     ! 0 |  337 | `}` |
|       - |  338 | `/*` |
|       - |  339 | ` * PHP_SHLIB_SUFFIX` |
|       - |  340 | ` *  Expand shared library suffix.` |
|       - |  341 | ` */` |
|       2 |  342 | `static void PH7_PHP_SHLIB_SUFFIX_Const(ph7_value *pVal,void *pUserData)` |
|     ! 0 |  343 | `{` |
|       - |  344 | `#ifdef __WINNT__` |
|     ! 0 |  345 | `	ph7_value_string(pVal,"dll",(int)sizeof("dll")-1);` |
|       - |  346 | `#else` |
|       2 |  347 | `	ph7_value_string(pVal,"so",(int)sizeof("so")-1);` |
|       - |  348 | `#endif` |
|       1 |  349 | `	SXUNUSED(pUserData); /* cc warning */` |
|       2 |  350 | `}` |
|       - |  351 | `/*` |
|       - |  352 | ` * E_ERROR` |
|       - |  353 | ` *  Expands 1` |
|       - |  354 | ` */` |
|       4 |  355 | `static void PH7_E_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  356 | `{` |
|       5 |  357 | `	ph7_value_int(pVal,1);` |
|       2 |  358 | `	SXUNUSED(pUserData);` |
|       5 |  359 | `}` |
|       - |  360 | `/*` |
|       - |  361 | ` * E_WARNING` |
|       - |  362 | ` *  Expands 2` |
|       - |  363 | ` */` |
|       6 |  364 | `static void PH7_E_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       2 |  365 | `{` |
|       8 |  366 | `	ph7_value_int(pVal,2);` |
|       3 |  367 | `	SXUNUSED(pUserData);` |
|       8 |  368 | `}` |
|       - |  369 | `/*` |
|       - |  370 | ` * E_PARSE` |
|       - |  371 | ` *  Expands 4` |
|       - |  372 | ` */` |
|       2 |  373 | `static void PH7_E_PARSE_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  374 | `{` |
|       3 |  375 | `	ph7_value_int(pVal,4);` |
|       1 |  376 | `	SXUNUSED(pUserData);` |
|       3 |  377 | `}` |
|       - |  378 | `/*` |
|       - |  379 | ` * E_NOTICE` |
|       - |  380 | ` * Expands 8` |
|       - |  381 | ` */` |
|       4 |  382 | `static void PH7_E_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  383 | `{` |
|       5 |  384 | `	ph7_value_int(pVal,8);` |
|       2 |  385 | `	SXUNUSED(pUserData);` |
|       5 |  386 | `}` |
|       - |  387 | `/*` |
|       - |  388 | ` * E_CORE_ERROR` |
|       - |  389 | ` * Expands 16` |
|       - |  390 | ` */` |
|       2 |  391 | `static void PH7_E_CORE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  392 | `{` |
|       3 |  393 | `	ph7_value_int(pVal,16);` |
|       1 |  394 | `	SXUNUSED(pUserData);` |
|       3 |  395 | `}` |
|       - |  396 | `/*` |
|       - |  397 | ` * E_CORE_WARNING` |
|       - |  398 | ` * Expands 32` |
|       - |  399 | ` */` |
|       2 |  400 | `static void PH7_E_CORE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  401 | `{` |
|       3 |  402 | `	ph7_value_int(pVal,32);` |
|       1 |  403 | `	SXUNUSED(pUserData);` |
|       3 |  404 | `}` |
|       - |  405 | `/*` |
|       - |  406 | ` * E_COMPILE_ERROR` |
|       - |  407 | ` * Expands 64` |
|       - |  408 | ` */` |
|       2 |  409 | `static void PH7_E_COMPILE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  410 | `{` |
|       3 |  411 | `	ph7_value_int(pVal,64);` |
|       1 |  412 | `	SXUNUSED(pUserData);` |
|       3 |  413 | `}` |
|       - |  414 | `/*` |
|       - |  415 | ` * E_COMPILE_WARNING` |
|       - |  416 | ` * Expands 128` |
|       - |  417 | ` */` |
|       2 |  418 | `static void PH7_E_COMPILE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  419 | `{` |
|       3 |  420 | `	ph7_value_int(pVal,128);` |
|       1 |  421 | `	SXUNUSED(pUserData);` |
|       3 |  422 | `}` |
|       - |  423 | `/*` |
|       - |  424 | ` * E_USER_ERROR` |
|       - |  425 | ` * Expands 256` |
|       - |  426 | ` */` |
|       4 |  427 | `static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       2 |  428 | `{` |
|       6 |  429 | `	ph7_value_int(pVal,256);` |
|       2 |  430 | `	SXUNUSED(pUserData);` |
|       6 |  431 | `}` |
|       - |  432 | `/*` |
|       - |  433 | ` * E_USER_WARNING` |
|       - |  434 | ` * Expands 512` |
|       - |  435 | ` */` |
|      24 |  436 | `static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  437 | `{` |
|      27 |  438 | `	ph7_value_int(pVal,512);` |
|      12 |  439 | `	SXUNUSED(pUserData);` |
|      27 |  440 | `}` |
|       - |  441 | `/*` |
|       - |  442 | ` * E_USER_NOTICE` |
|       - |  443 | ` * Expands 1024` |
|       - |  444 | ` */` |
|      10 |  445 | `static void PH7_E_USER_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  446 | `{` |
|      14 |  447 | `	ph7_value_int(pVal,1024);` |
|       5 |  448 | `	SXUNUSED(pUserData);` |
|      14 |  449 | `}` |
|       - |  450 | `/*` |
|       - |  451 | ` * E_RECOVERABLE_ERROR` |
|       - |  452 | ` * Expands 4096` |
|       - |  453 | ` */` |
|       2 |  454 | `static void PH7_E_RECOVERABLE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  455 | `{` |
|       3 |  456 | `	ph7_value_int(pVal,4096);` |
|       1 |  457 | `	SXUNUSED(pUserData);` |
|       3 |  458 | `}` |
|       - |  459 | `/*` |
|       - |  460 | ` * E_DEPRECATED` |
|       - |  461 | ` * Expands 8192` |
|       - |  462 | ` */` |
|      10 |  463 | `static void PH7_E_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  464 | `{` |
|      11 |  465 | `	ph7_value_int(pVal,8192);` |
|       5 |  466 | `	SXUNUSED(pUserData);` |
|      11 |  467 | `}` |
|       - |  468 | `/*` |
|       - |  469 | ` * E_USER_DEPRECATED` |
|       - |  470 | ` *   Expands 16384.` |
|       - |  471 | ` */` |
|       2 |  472 | `static void PH7_E_USER_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  473 | `{` |
|       3 |  474 | `	ph7_value_int(pVal,16384);` |
|       1 |  475 | `	SXUNUSED(pUserData);` |
|       3 |  476 | `}` |
|       - |  477 | `/*` |
|       - |  478 | ` * E_ALL` |
|       - |  479 | ` *  Expands 30719 (php 8: E_STRICT is no longer part of E_ALL)` |
|       - |  480 | ` */` |
|      42 |  481 | `static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  482 | `{` |
|      47 |  483 | `	ph7_value_int(pVal,30719);` |
|      21 |  484 | `	SXUNUSED(pUserData);` |
|      47 |  485 | `}` |
|       - |  486 | `/*` |
|       - |  487 | ` * CASE_LOWER` |
|       - |  488 | ` *  Expands 0.` |
|       - |  489 | ` */` |
|       2 |  490 | `static void PH7_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  491 | `{` |
|       3 |  492 | `	ph7_value_int(pVal,0);` |
|       1 |  493 | `	SXUNUSED(pUserData);` |
|       3 |  494 | `}` |
|       - |  495 | `/*` |
|       - |  496 | ` * CASE_UPPER` |
|       - |  497 | ` *  Expands 1.` |
|       - |  498 | ` */` |
|       8 |  499 | `static void PH7_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|       2 |  500 | `{` |
|      10 |  501 | `	ph7_value_int(pVal,1);` |
|       4 |  502 | `	SXUNUSED(pUserData);` |
|      10 |  503 | `}` |
|       - |  504 | `/*` |
|       - |  505 | ` * STR_PAD_LEFT` |
|       - |  506 | ` *  Expands 0.` |
|       - |  507 | ` */` |
|      10 |  508 | `static void PH7_STR_PAD_LEFT_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  509 | `{` |
|      11 |  510 | `	ph7_value_int(pVal,0);` |
|       5 |  511 | `	SXUNUSED(pUserData);` |
|      11 |  512 | `}` |
|       - |  513 | `/*` |
|       - |  514 | ` * STR_PAD_RIGHT` |
|       - |  515 | ` *  Expands 1.` |
|       - |  516 | ` */` |
|       6 |  517 | `static void PH7_STR_PAD_RIGHT_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  518 | `{` |
|       7 |  519 | `	ph7_value_int(pVal,1);` |
|       3 |  520 | `	SXUNUSED(pUserData);` |
|       7 |  521 | `}` |
|       - |  522 | `/*` |
|       - |  523 | ` * STR_PAD_BOTH` |
|       - |  524 | ` *  Expands 2.` |
|       - |  525 | ` */` |
|       4 |  526 | `static void PH7_STR_PAD_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  527 | `{` |
|       5 |  528 | `	ph7_value_int(pVal,2);` |
|       2 |  529 | `	SXUNUSED(pUserData);` |
|       5 |  530 | `}` |
|       - |  531 | `/*` |
|       - |  532 | ` * COUNT_NORMAL` |
|       - |  533 | ` *  Expands 0` |
|       - |  534 | ` */` |
|       8 |  535 | `static void PH7_COUNT_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       2 |  536 | `{` |
|      10 |  537 | `	ph7_value_int(pVal,0);` |
|       4 |  538 | `	SXUNUSED(pUserData);` |
|      10 |  539 | `}` |
|       - |  540 | `/*` |
|       - |  541 | ` * COUNT_RECURSIVE` |
|       - |  542 | ` *  Expands 1.` |
|       - |  543 | ` */` |
|      20 |  544 | `static void PH7_COUNT_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|       2 |  545 | `{` |
|      22 |  546 | `	ph7_value_int(pVal,1);` |
|      10 |  547 | `	SXUNUSED(pUserData);` |
|      22 |  548 | `}` |
|       - |  549 | `/*` |
|       - |  550 | ` * php's sort-flag constants. The VALUES must match php exactly: they are a` |
|       - |  551 | ` * public ABI (code passes literal ints, dumps them, and OR-combines the base` |
|       - |  552 | ` * type with SORT_FLAG_CASE). SORT_ASC/SORT_DESC are the array_multisort` |
|       - |  553 | ` * direction flags.` |
|       - |  554 | ` * SORT_REGULAR 0 · SORT_NUMERIC 1 · SORT_STRING 2 · SORT_DESC 3 · SORT_ASC 4 ·` |
|       - |  555 | ` * SORT_LOCALE_STRING 5 · SORT_NATURAL 6 · SORT_FLAG_CASE 8` |
|       - |  556 | ` */` |
|      16 |  557 | `static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  558 | `{` |
|      19 |  559 | `	ph7_value_int(pVal,4);` |
|       8 |  560 | `	SXUNUSED(pUserData);` |
|      19 |  561 | `}` |
|      12 |  562 | `static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)` |
|       2 |  563 | `{` |
|      14 |  564 | `	ph7_value_int(pVal,3);` |
|       6 |  565 | `	SXUNUSED(pUserData);` |
|      14 |  566 | `}` |
|      12 |  567 | `static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)` |
|       2 |  568 | `{` |
|      14 |  569 | `	ph7_value_int(pVal,0);` |
|       6 |  570 | `	SXUNUSED(pUserData);` |
|      14 |  571 | `}` |
|      70 |  572 | `static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  573 | `{` |
|      73 |  574 | `	ph7_value_int(pVal,1);` |
|      35 |  575 | `	SXUNUSED(pUserData);` |
|      73 |  576 | `}` |
|      90 |  577 | `static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  578 | `{` |
|      93 |  579 | `	ph7_value_int(pVal,2);` |
|      45 |  580 | `	SXUNUSED(pUserData);` |
|      93 |  581 | `}` |
|       4 |  582 | `static void PH7_SORT_LOCALE_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       2 |  583 | `{` |
|       6 |  584 | `	ph7_value_int(pVal,5);` |
|       2 |  585 | `	SXUNUSED(pUserData);` |
|       6 |  586 | `}` |
|      20 |  587 | `static void PH7_SORT_NATURAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  588 | `{` |
|      23 |  589 | `	ph7_value_int(pVal,6);` |
|      10 |  590 | `	SXUNUSED(pUserData);` |
|      23 |  591 | `}` |
|      24 |  592 | `static void PH7_SORT_FLAG_CASE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  593 | `{` |
|      28 |  594 | `	ph7_value_int(pVal,8);` |
|      12 |  595 | `	SXUNUSED(pUserData);` |
|      28 |  596 | `}` |
|       - |  597 | `/*` |
|       - |  598 | ` * PHP_ROUND_HALF_UP` |
|       - |  599 | ` *  Expands 1.` |
|       - |  600 | ` */` |
|       4 |  601 | `static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  602 | `{` |
|       5 |  603 | `	ph7_value_int(pVal,1);` |
|       2 |  604 | `	SXUNUSED(pUserData);` |
|       5 |  605 | `}` |
|       - |  606 | `/*` |
|       - |  607 | ` * PHP_SESSION_DISABLED / PHP_SESSION_NONE / PHP_SESSION_ACTIVE` |
|       - |  608 | ` *  session_status() states (0 / 1 / 2).` |
|       - |  609 | ` */` |
|       2 |  610 | `static void PH7_PHP_SESSION_DISABLED_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  611 | `{` |
|       3 |  612 | `	ph7_value_int(pVal,0);` |
|       1 |  613 | `	SXUNUSED(pUserData);` |
|       3 |  614 | `}` |
|       2 |  615 | `static void PH7_PHP_SESSION_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  616 | `{` |
|       3 |  617 | `	ph7_value_int(pVal,1);` |
|       1 |  618 | `	SXUNUSED(pUserData);` |
|       3 |  619 | `}` |
|       2 |  620 | `static void PH7_PHP_SESSION_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  621 | `{` |
|       3 |  622 | `	ph7_value_int(pVal,2);` |
|       1 |  623 | `	SXUNUSED(pUserData);` |
|       3 |  624 | `}` |
|       - |  625 | `/*` |
|       - |  626 | ` * INI_USER / INI_PERDIR / INI_SYSTEM / INI_ALL` |
|       - |  627 | ` *  php.ini access levels (1 / 2 / 4 / 7).` |
|       - |  628 | ` */` |
|       2 |  629 | `static void PH7_INI_USER_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  630 | `{` |
|       3 |  631 | `	ph7_value_int(pVal,1);` |
|       1 |  632 | `	SXUNUSED(pUserData);` |
|       3 |  633 | `}` |
|       2 |  634 | `static void PH7_INI_PERDIR_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  635 | `{` |
|       3 |  636 | `	ph7_value_int(pVal,2);` |
|       1 |  637 | `	SXUNUSED(pUserData);` |
|       3 |  638 | `}` |
|       2 |  639 | `static void PH7_INI_SYSTEM_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  640 | `{` |
|       3 |  641 | `	ph7_value_int(pVal,4);` |
|       1 |  642 | `	SXUNUSED(pUserData);` |
|       3 |  643 | `}` |
|       2 |  644 | `static void PH7_INI_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  645 | `{` |
|       3 |  646 | `	ph7_value_int(pVal,7);` |
|       1 |  647 | `	SXUNUSED(pUserData);` |
|       3 |  648 | `}` |
|       - |  649 | `/*` |
|       - |  650 | ` * MB_CASE_UPPER / MB_CASE_LOWER / MB_CASE_TITLE (0 / 1 / 2)` |
|       - |  651 | ` */` |
|       4 |  652 | `static void PH7_MB_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  653 | `{` |
|       5 |  654 | `	ph7_value_int(pVal,0);` |
|       2 |  655 | `	SXUNUSED(pUserData);` |
|       5 |  656 | `}` |
|       4 |  657 | `static void PH7_MB_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  658 | `{` |
|       5 |  659 | `	ph7_value_int(pVal,1);` |
|       2 |  660 | `	SXUNUSED(pUserData);` |
|       5 |  661 | `}` |
|      32 |  662 | `static void PH7_MB_CASE_TITLE_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  663 | `{` |
|      33 |  664 | `	ph7_value_int(pVal,2);` |
|      16 |  665 | `	SXUNUSED(pUserData);` |
|      33 |  666 | `}` |
|       - |  667 | `/*` |
|       - |  668 | ` * SPHP_ROUND_HALF_DOWN` |
|       - |  669 | ` *  Expands 2.` |
|       - |  670 | ` */` |
|       4 |  671 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  672 | `{` |
|       5 |  673 | `	ph7_value_int(pVal,2);` |
|       2 |  674 | `	SXUNUSED(pUserData);` |
|       5 |  675 | `}` |
|       - |  676 | `/*` |
|       - |  677 | ` * PHP_ROUND_HALF_EVEN` |
|       - |  678 | ` *  Expands 3.` |
|       - |  679 | ` */` |
|       8 |  680 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  681 | `{` |
|       9 |  682 | `	ph7_value_int(pVal,3);` |
|       4 |  683 | `	SXUNUSED(pUserData);` |
|       9 |  684 | `}` |
|       - |  685 | `/*` |
|       - |  686 | ` * PHP_ROUND_HALF_ODD` |
|       - |  687 | ` *  Expands 4.` |
|       - |  688 | ` */` |
|       4 |  689 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  690 | `{` |
|       5 |  691 | `	ph7_value_int(pVal,4);` |
|       2 |  692 | `	SXUNUSED(pUserData);` |
|       5 |  693 | `}` |
|       - |  694 | `/*` |
|       - |  695 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|       - |  696 | ` *  Expand 0x01` |
|       - |  697 | ` * NOTE:` |
|       - |  698 | ` *  The expanded value must be a power of two.` |
|       - |  699 | ` */` |
|       4 |  700 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  701 | `{` |
|       5 |  702 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|       2 |  703 | `	SXUNUSED(pUserData);` |
|       5 |  704 | `}` |
|       - |  705 | `/*` |
|       - |  706 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|       - |  707 | ` *  Expand 0x02` |
|       - |  708 | ` * NOTE:` |
|       - |  709 | ` *  The expanded value must be a power of two.` |
|       - |  710 | ` */` |
|       8 |  711 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  712 | `{` |
|       9 |  713 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|       4 |  714 | `	SXUNUSED(pUserData);` |
|       9 |  715 | `}` |
|       - |  716 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|       - |  717 | `/*` |
|       - |  718 | ` * M_PI` |
|       - |  719 | ` *  Expand the value of pi.` |
|       - |  720 | ` */` |
|      10 |  721 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|       2 |  722 | `{` |
|       5 |  723 | `	SXUNUSED(pUserData); /* cc warning */` |
|      12 |  724 | `	ph7_value_double(pVal,PH7_PI);` |
|      12 |  725 | `}` |
|       - |  726 | `/*` |
|       - |  727 | ` * M_E` |
|       - |  728 | ` *  Expand 2.7182818284590452354` |
|       - |  729 | ` */` |
|       2 |  730 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  731 | `{` |
|       1 |  732 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  733 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|       3 |  734 | `}` |
|       - |  735 | `/*` |
|       - |  736 | ` * M_LOG2E` |
|       - |  737 | ` *  Expand 2.7182818284590452354` |
|       - |  738 | ` */` |
|       2 |  739 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  740 | `{` |
|       1 |  741 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  742 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|       3 |  743 | `}` |
|       - |  744 | `/*` |
|       - |  745 | ` * M_LOG10E` |
|       - |  746 | ` *  Expand 0.4342944819032518276` |
|       - |  747 | ` */` |
|       2 |  748 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  749 | `{` |
|       1 |  750 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  751 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|       3 |  752 | `}` |
|       - |  753 | `/*` |
|       - |  754 | ` * M_LN2` |
|       - |  755 | ` *  Expand 	0.69314718055994530942` |
|       - |  756 | ` */` |
|       2 |  757 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  758 | `{` |
|       1 |  759 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  760 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|       3 |  761 | `}` |
|       - |  762 | `/*` |
|       - |  763 | ` * M_LN10` |
|       - |  764 | ` *  Expand 	2.30258509299404568402` |
|       - |  765 | ` */` |
|       2 |  766 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  767 | `{` |
|       1 |  768 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  769 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|       3 |  770 | `}` |
|       - |  771 | `/*` |
|       - |  772 | ` * M_PI_2` |
|       - |  773 | ` *  Expand 	1.57079632679489661923` |
|       - |  774 | ` */` |
|       2 |  775 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  776 | `{` |
|       1 |  777 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  778 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|       3 |  779 | `}` |
|       - |  780 | `/*` |
|       - |  781 | ` * M_PI_4` |
|       - |  782 | ` *  Expand 	0.78539816339744830962` |
|       - |  783 | ` */` |
|       2 |  784 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  785 | `{` |
|       1 |  786 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  787 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|       3 |  788 | `}` |
|       - |  789 | `/*` |
|       - |  790 | ` * M_1_PI` |
|       - |  791 | ` *  Expand 	0.31830988618379067154` |
|       - |  792 | ` */` |
|       2 |  793 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  794 | `{` |
|       1 |  795 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  796 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|       3 |  797 | `}` |
|       - |  798 | `/*` |
|       - |  799 | ` * M_2_PI` |
|       - |  800 | ` *  Expand 0.63661977236758134308` |
|       - |  801 | ` */` |
|       4 |  802 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  803 | `{` |
|       2 |  804 | `	SXUNUSED(pUserData); /* cc warning */` |
|       5 |  805 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|       5 |  806 | `}` |
|       - |  807 | `/*` |
|       - |  808 | ` * M_SQRTPI` |
|       - |  809 | ` *  Expand 1.77245385090551602729` |
|       - |  810 | ` */` |
|       2 |  811 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  812 | `{` |
|       1 |  813 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  814 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|       3 |  815 | `}` |
|       - |  816 | `/*` |
|       - |  817 | ` * M_2_SQRTPI` |
|       - |  818 | ` *  Expand 	1.12837916709551257390` |
|       - |  819 | ` */` |
|       2 |  820 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  821 | `{` |
|       1 |  822 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  823 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|       3 |  824 | `}` |
|       - |  825 | `/*` |
|       - |  826 | ` * M_SQRT2` |
|       - |  827 | ` *  Expand 	1.41421356237309504880` |
|       - |  828 | ` */` |
|       2 |  829 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  830 | `{` |
|       1 |  831 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  832 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|       3 |  833 | `}` |
|       - |  834 | `/*` |
|       - |  835 | ` * M_SQRT3` |
|       - |  836 | ` *  Expand 	1.73205080756887729352` |
|       - |  837 | ` */` |
|       2 |  838 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  839 | `{` |
|       1 |  840 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  841 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|       3 |  842 | `}` |
|       - |  843 | `/*` |
|       - |  844 | ` * M_SQRT1_2` |
|       - |  845 | ` *  Expand 	0.70710678118654752440` |
|       - |  846 | ` */` |
|       2 |  847 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  848 | `{` |
|       1 |  849 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  850 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|       3 |  851 | `}` |
|       - |  852 | `/*` |
|       - |  853 | ` * M_LNPI` |
|       - |  854 | ` *  Expand 	1.14472988584940017414` |
|       - |  855 | ` */` |
|       2 |  856 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  857 | `{` |
|       1 |  858 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  859 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|       3 |  860 | `}` |
|       - |  861 | `/*` |
|       - |  862 | ` * M_EULER` |
|       - |  863 | ` *  Expand  0.57721566490153286061` |
|       - |  864 | ` */` |
|       2 |  865 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  866 | `{` |
|       1 |  867 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  868 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|       3 |  869 | `}` |
|       - |  870 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|       - |  871 | `/*` |
|       - |  872 | ` * DATE_ATOM` |
|       - |  873 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|       - |  874 | ` */` |
|       4 |  875 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  876 | `{` |
|       2 |  877 | `	SXUNUSED(pUserData); /* cc warning */` |
|       5 |  878 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|       5 |  879 | `}` |
|       - |  880 | `/*` |
|       - |  881 | ` * DATE_COOKIE` |
|       - |  882 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|       - |  883 | ` */` |
|       2 |  884 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  885 | `{` |
|       1 |  886 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  887 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|       3 |  888 | `}` |
|       - |  889 | `/*` |
|       - |  890 | ` * DATE_ISO8601` |
|       - |  891 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|       - |  892 | ` */` |
|       2 |  893 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  894 | `{` |
|       1 |  895 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  896 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|       3 |  897 | `}` |
|       - |  898 | `/*` |
|       - |  899 | ` * DATE_RFC822` |
|       - |  900 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|       - |  901 | ` */` |
|       2 |  902 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  903 | `{` |
|       1 |  904 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  905 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|       3 |  906 | `}` |
|       - |  907 | `/*` |
|       - |  908 | ` * DATE_RFC850` |
|       - |  909 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|       - |  910 | ` */` |
|       2 |  911 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  912 | `{` |
|       1 |  913 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  914 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|       3 |  915 | `}` |
|       - |  916 | `/*` |
|       - |  917 | ` * DATE_RFC1036` |
|       - |  918 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - |  919 | ` */` |
|       2 |  920 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  921 | `{` |
|       1 |  922 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  923 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|       3 |  924 | `}` |
|       - |  925 | `/*` |
|       - |  926 | ` * DATE_RFC1123` |
|       - |  927 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - |  928 | ` */` |
|       2 |  929 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  930 | `{` |
|       1 |  931 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  932 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|       3 |  933 | `}` |
|       - |  934 | `/*` |
|       - |  935 | ` * DATE_RFC2822` |
|       - |  936 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - |  937 | ` */` |
|       2 |  938 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  939 | `{` |
|       1 |  940 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  941 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|       3 |  942 | `}` |
|       - |  943 | `/*` |
|       - |  944 | ` * DATE_RSS` |
|       - |  945 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - |  946 | ` */` |
|       2 |  947 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  948 | `{` |
|       1 |  949 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  950 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|       3 |  951 | `}` |
|       - |  952 | `/*` |
|       - |  953 | ` * DATE_W3C` |
|       - |  954 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|       - |  955 | ` */` |
|       2 |  956 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  957 | `{` |
|       1 |  958 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 |  959 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|       3 |  960 | `}` |
|       - |  961 | `/*` |
|       - |  962 | ` * The ENT_* values are PHP-exact (php 8.5.7). The low two bits are the quote` |
|       - |  963 | ` * bits (1 = single, 2 = double), so ENT_QUOTES = ENT_COMPAT\|1 and` |
|       - |  964 | ` * ENT_NOQUOTES = 0. Bits 16\|32 select the doctype (0 = HTML401, 16 = XML1,` |
|       - |  965 | ` * 32 = XHTML, 48 = HTML5) — composites, not flags.` |
|       - |  966 | ` */` |
|       - |  967 | `/*` |
|       - |  968 | ` * ENT_COMPAT` |
|       - |  969 | ` *  Expand 2 (double-quote bit only)` |
|       - |  970 | ` */` |
|      12 |  971 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  972 | `{` |
|       6 |  973 | `	SXUNUSED(pUserData); /* cc warning */` |
|      13 |  974 | `	ph7_value_int(pVal,PH7_ENT_QUOTE_DOUBLE);` |
|      13 |  975 | `}` |
|       - |  976 | `/*` |
|       - |  977 | ` * ENT_QUOTES` |
|       - |  978 | ` *  Expand 3 (double\|single quote bits)` |
|       - |  979 | ` */` |
|      60 |  980 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  981 | `{` |
|      30 |  982 | `	SXUNUSED(pUserData); /* cc warning */` |
|      61 |  983 | `	ph7_value_int(pVal,PH7_ENT_QUOTES);` |
|      61 |  984 | `}` |
|       - |  985 | `/*` |
|       - |  986 | ` * ENT_NOQUOTES` |
|       - |  987 | ` *  Expand 0 (no quote bits)` |
|       - |  988 | ` */` |
|      20 |  989 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  990 | `{` |
|      10 |  991 | `	SXUNUSED(pUserData); /* cc warning */` |
|      21 |  992 | `	ph7_value_int(pVal,0);` |
|      21 |  993 | `}` |
|       - |  994 | `/*` |
|       - |  995 | ` * ENT_IGNORE` |
|       - |  996 | ` *  Expand 4` |
|       - |  997 | ` */` |
|       6 |  998 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|       1 |  999 | `{` |
|       3 | 1000 | `	SXUNUSED(pUserData); /* cc warning */` |
|       7 | 1001 | `	ph7_value_int(pVal,PH7_ENT_IGNORE);` |
|       7 | 1002 | `}` |
|       - | 1003 | `/*` |
|       - | 1004 | ` * ENT_SUBSTITUTE` |
|       - | 1005 | ` *  Expand 8` |
|       - | 1006 | ` */` |
|       2 | 1007 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1008 | `{` |
|       1 | 1009 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 | 1010 | `	ph7_value_int(pVal,PH7_ENT_SUBSTITUTE);` |
|       3 | 1011 | `}` |
|       - | 1012 | `/*` |
|       - | 1013 | ` * ENT_DISALLOWED` |
|       - | 1014 | ` *  Expand 128` |
|       - | 1015 | ` */` |
|       2 | 1016 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1017 | `{` |
|       1 | 1018 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 | 1019 | `	ph7_value_int(pVal,PH7_ENT_DISALLOWED);` |
|       3 | 1020 | `}` |
|       - | 1021 | `/*` |
|       - | 1022 | ` * ENT_HTML401` |
|       - | 1023 | ` *  Expand 0 (the default doctype)` |
|       - | 1024 | ` */` |
|       2 | 1025 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1026 | `{` |
|       1 | 1027 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 | 1028 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML401);` |
|       3 | 1029 | `}` |
|       - | 1030 | `/*` |
|       - | 1031 | ` * ENT_XML1` |
|       - | 1032 | ` *  Expand 16` |
|       - | 1033 | ` */` |
|       8 | 1034 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1035 | `{` |
|       4 | 1036 | `	SXUNUSED(pUserData); /* cc warning */` |
|       9 | 1037 | `	ph7_value_int(pVal,PH7_ENT_DOC_XML1);` |
|       9 | 1038 | `}` |
|       - | 1039 | `/*` |
|       - | 1040 | ` * ENT_XHTML` |
|       - | 1041 | ` *  Expand 32` |
|       - | 1042 | ` */` |
|       6 | 1043 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1044 | `{` |
|       3 | 1045 | `	SXUNUSED(pUserData); /* cc warning */` |
|       7 | 1046 | `	ph7_value_int(pVal,PH7_ENT_DOC_XHTML);` |
|       7 | 1047 | `}` |
|       - | 1048 | `/*` |
|       - | 1049 | ` * ENT_HTML5` |
|       - | 1050 | ` *  Expand 48 (16\|32 — a doctype composite, not a flag bit)` |
|       - | 1051 | ` */` |
|       8 | 1052 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1053 | `{` |
|       4 | 1054 | `	SXUNUSED(pUserData); /* cc warning */` |
|       9 | 1055 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML5);` |
|       9 | 1056 | `}` |
|       - | 1057 | `/*` |
|       - | 1058 | ` * ISO-8859-1` |
|       - | 1059 | ` * ISO_8859_1` |
|       - | 1060 | ` *   Expand 1` |
|       - | 1061 | ` */` |
|       2 | 1062 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1063 | `{` |
|       1 | 1064 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 | 1065 | `	ph7_value_int(pVal,1);` |
|       3 | 1066 | `}` |
|       - | 1067 | `/*` |
|       - | 1068 | ` * UTF-8` |
|       - | 1069 | ` * UTF8` |
|       - | 1070 | ` *  Expand 2` |
|       - | 1071 | ` */` |
|       2 | 1072 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1073 | `{` |
|       1 | 1074 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 | 1075 | `	ph7_value_int(pVal,1);` |
|       3 | 1076 | `}` |
|       - | 1077 | `/*` |
|       - | 1078 | ` * HTML_ENTITIES` |
|       - | 1079 | ` *  Expand 1` |
|       - | 1080 | ` */` |
|       4 | 1081 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1082 | `{` |
|       2 | 1083 | `	SXUNUSED(pUserData); /* cc warning */` |
|       5 | 1084 | `	ph7_value_int(pVal,1);` |
|       5 | 1085 | `}` |
|       - | 1086 | `/*` |
|       - | 1087 | ` * HTML_SPECIALCHARS` |
|       - | 1088 | ` *  Expand 0 (PHP-exact)` |
|       - | 1089 | ` */` |
|      10 | 1090 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1091 | `{` |
|       5 | 1092 | `	SXUNUSED(pUserData); /* cc warning */` |
|      11 | 1093 | `	ph7_value_int(pVal,0);` |
|      11 | 1094 | `}` |
|       - | 1095 | `/*` |
|       - | 1096 | ` * PHP_URL_SCHEME.` |
|       - | 1097 | ` * Expand 0` |
|       - | 1098 | ` */` |
|       4 | 1099 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1100 | `{` |
|       2 | 1101 | `	SXUNUSED(pUserData); /* cc warning */` |
|       5 | 1102 | `	ph7_value_int(pVal,0);` |
|       5 | 1103 | `}` |
|       - | 1104 | `/*` |
|       - | 1105 | ` * PHP_URL_HOST.` |
|       - | 1106 | ` * Expand 1` |
|       - | 1107 | ` */` |
|       6 | 1108 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1109 | `{` |
|       3 | 1110 | `	SXUNUSED(pUserData); /* cc warning */` |
|       7 | 1111 | `	ph7_value_int(pVal,1);` |
|       7 | 1112 | `}` |
|       - | 1113 | `/*` |
|       - | 1114 | ` * PHP_URL_PORT.` |
|       - | 1115 | ` * Expand 2` |
|       - | 1116 | ` */` |
|       6 | 1117 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1118 | `{` |
|       3 | 1119 | `	SXUNUSED(pUserData); /* cc warning */` |
|       7 | 1120 | `	ph7_value_int(pVal,2);` |
|       7 | 1121 | `}` |
|       - | 1122 | `/*` |
|       - | 1123 | ` * PHP_URL_USER.` |
|       - | 1124 | ` * Expand 3` |
|       - | 1125 | ` */` |
|       4 | 1126 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1127 | `{` |
|       2 | 1128 | `	SXUNUSED(pUserData); /* cc warning */` |
|       5 | 1129 | `	ph7_value_int(pVal,3);` |
|       5 | 1130 | `}` |
|       - | 1131 | `/*` |
|       - | 1132 | ` * PHP_URL_PASS.` |
|       - | 1133 | ` * Expand 4` |
|       - | 1134 | ` */` |
|       4 | 1135 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1136 | `{` |
|       2 | 1137 | `	SXUNUSED(pUserData); /* cc warning */` |
|       5 | 1138 | `	ph7_value_int(pVal,4);` |
|       5 | 1139 | `}` |
|       - | 1140 | `/*` |
|       - | 1141 | ` * PHP_URL_PATH.` |
|       - | 1142 | ` * Expand 5` |
|       - | 1143 | ` */` |
|       4 | 1144 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1145 | `{` |
|       2 | 1146 | `	SXUNUSED(pUserData); /* cc warning */` |
|       5 | 1147 | `	ph7_value_int(pVal,5);` |
|       5 | 1148 | `}` |
|       - | 1149 | `/*` |
|       - | 1150 | ` * PHP_URL_QUERY.` |
|       - | 1151 | ` * Expand 6` |
|       - | 1152 | ` */` |
|       6 | 1153 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1154 | `{` |
|       3 | 1155 | `	SXUNUSED(pUserData); /* cc warning */` |
|       7 | 1156 | `	ph7_value_int(pVal,6);` |
|       7 | 1157 | `}` |
|       - | 1158 | `/*` |
|       - | 1159 | ` * PHP_URL_FRAGMENT.` |
|       - | 1160 | ` * Expand 7` |
|       - | 1161 | ` */` |
|       6 | 1162 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1163 | `{` |
|       3 | 1164 | `	SXUNUSED(pUserData); /* cc warning */` |
|       7 | 1165 | `	ph7_value_int(pVal,7);` |
|       7 | 1166 | `}` |
|       - | 1167 | `/*` |
|       - | 1168 | ` * PHP_QUERY_RFC1738` |
|       - | 1169 | ` * Expand 1` |
|       - | 1170 | ` */` |
|       2 | 1171 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1172 | `{` |
|       1 | 1173 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 | 1174 | `	ph7_value_int(pVal,1);` |
|       3 | 1175 | `}` |
|       - | 1176 | `/*` |
|       - | 1177 | ` * PHP_QUERY_RFC3986` |
|       - | 1178 | ` * Expand 1` |
|       - | 1179 | ` */` |
|       4 | 1180 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1181 | `{` |
|       2 | 1182 | `	SXUNUSED(pUserData); /* cc warning */` |
|       5 | 1183 | `	ph7_value_int(pVal,2);` |
|       5 | 1184 | `}` |
|       - | 1185 | `/* php's FNM_* values (ext/standard): PATHNAME=1, NOESCAPE=2, PERIOD=4, CASEFOLD=16.` |
|       - | 1186 | ` * PHL previously had PATHNAME/NOESCAPE swapped and CASEFOLD=8; fnmatch() reads these` |
|       - | 1187 | ` * bits, so PH7_builtin_fnmatch was updated to the same values. */` |
|       - | 1188 | `/*` |
|       - | 1189 | ` * FNM_PATHNAME` |
|       - | 1190 | ` *  Expand 1 (php value)` |
|       - | 1191 | ` */` |
|     ! 0 | 1192 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|     ! 0 | 1193 | `{` |
|     ! 0 | 1194 | `	SXUNUSED(pUserData); /* cc warning */` |
|     ! 0 | 1195 | `	ph7_value_int(pVal,1);` |
|     ! 0 | 1196 | `}` |
|       - | 1197 | `/*` |
|       - | 1198 | ` * FNM_NOESCAPE` |
|       - | 1199 | ` *  Expand 2 (php value)` |
|       - | 1200 | ` */` |
|     ! 0 | 1201 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|     ! 0 | 1202 | `{` |
|     ! 0 | 1203 | `	SXUNUSED(pUserData); /* cc warning */` |
|     ! 0 | 1204 | `	ph7_value_int(pVal,2);` |
|     ! 0 | 1205 | `}` |
|       - | 1206 | `/*` |
|       - | 1207 | ` * FNM_PERIOD` |
|       - | 1208 | ` *  Expand 4 (php value)` |
|       - | 1209 | ` */` |
|       6 | 1210 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1211 | `{` |
|       3 | 1212 | `	SXUNUSED(pUserData); /* cc warning */` |
|       7 | 1213 | `	ph7_value_int(pVal,4);` |
|       7 | 1214 | `}` |
|       - | 1215 | `/*` |
|       - | 1216 | ` * FNM_CASEFOLD` |
|       - | 1217 | ` *  Expand 16 (php value)` |
|       - | 1218 | ` */` |
|       4 | 1219 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1220 | `{` |
|       2 | 1221 | `	SXUNUSED(pUserData); /* cc warning */` |
|       5 | 1222 | `	ph7_value_int(pVal,16);` |
|       5 | 1223 | `}` |
|       - | 1224 | `/*` |
|       - | 1225 | ` * PATHINFO_DIRNAME` |
|       - | 1226 | ` *  Expand 1.` |
|       - | 1227 | ` */` |
|      22 | 1228 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1229 | `{` |
|      11 | 1230 | `	SXUNUSED(pUserData); /* cc warning */` |
|      23 | 1231 | `	ph7_value_int(pVal,PH7_PATHINFO_DIRNAME);` |
|      23 | 1232 | `}` |
|       - | 1233 | `/*` |
|       - | 1234 | ` * PATHINFO_BASENAME` |
|       - | 1235 | ` *  Expand 2.` |
|       - | 1236 | ` */` |
|      22 | 1237 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1238 | `{` |
|      11 | 1239 | `	SXUNUSED(pUserData); /* cc warning */` |
|      23 | 1240 | `	ph7_value_int(pVal,PH7_PATHINFO_BASENAME);` |
|      23 | 1241 | `}` |
|       - | 1242 | `/*` |
|       - | 1243 | ` * PATHINFO_EXTENSION` |
|       - | 1244 | ` *  Expand php's 4 (a POWER OF TWO: the components are a bitmask).` |
|       - | 1245 | ` */` |
|    7710 | 1246 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1247 | `{` |
|    3855 | 1248 | `	SXUNUSED(pUserData); /* cc warning */` |
|    7715 | 1249 | `	ph7_value_int(pVal,PH7_PATHINFO_EXTENSION);` |
|    7715 | 1250 | `}` |
|       - | 1251 | `/*` |
|       - | 1252 | ` * PATHINFO_FILENAME` |
|       - | 1253 | ` *  Expand php's 8 (a POWER OF TWO: the components are a bitmask).` |
|       - | 1254 | ` */` |
|    7698 | 1255 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1256 | `{` |
|    3849 | 1257 | `	SXUNUSED(pUserData); /* cc warning */` |
|    7703 | 1258 | `	ph7_value_int(pVal,PH7_PATHINFO_FILENAME);` |
|    7703 | 1259 | `}` |
|       - | 1260 | `/*` |
|       - | 1261 | ` * PATHINFO_ALL` |
|       - | 1262 | ` *  Expand php's 15 — the default, and the one value that answers with the ARRAY.` |
|       - | 1263 | ` */` |
|      10 | 1264 | `static void PH7_PATHINFO_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1265 | `{` |
|       5 | 1266 | `	SXUNUSED(pUserData); /* cc warning */` |
|      11 | 1267 | `	ph7_value_int(pVal,PH7_PATHINFO_ALL);` |
|      11 | 1268 | `}` |
|       - | 1269 | `/*` |
|       - | 1270 | ` * SEEK_SET.` |
|       - | 1271 | ` *  Expand 0` |
|       - | 1272 | ` */` |
|      20 | 1273 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1274 | `{` |
|      10 | 1275 | `	SXUNUSED(pUserData); /* cc warning */` |
|      21 | 1276 | `	ph7_value_int(pVal,0);` |
|      21 | 1277 | `}` |
|       - | 1278 | `/*` |
|       - | 1279 | ` * SEEK_CUR.` |
|       - | 1280 | ` *  Expand 1` |
|       - | 1281 | ` */` |
|       8 | 1282 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|       2 | 1283 | `{` |
|       4 | 1284 | `	SXUNUSED(pUserData); /* cc warning */` |
|      10 | 1285 | `	ph7_value_int(pVal,1);` |
|      10 | 1286 | `}` |
|       - | 1287 | `/*` |
|       - | 1288 | ` * SEEK_END.` |
|       - | 1289 | ` *  Expand 2` |
|       - | 1290 | ` */` |
|       8 | 1291 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1292 | `{` |
|       4 | 1293 | `	SXUNUSED(pUserData); /* cc warning */` |
|       9 | 1294 | `	ph7_value_int(pVal,2);` |
|       9 | 1295 | `}` |
|       - | 1296 | `/*` |
|       - | 1297 | ` * LOCK_SH.` |
|       - | 1298 | ` *  Expand 2` |
|       - | 1299 | ` */` |
|      10 | 1300 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1301 | `{` |
|       5 | 1302 | `	SXUNUSED(pUserData); /* cc warning */` |
|      11 | 1303 | `	ph7_value_int(pVal,1);` |
|      11 | 1304 | `}` |
|       - | 1305 | `/*` |
|       - | 1306 | ` * LOCK_NB.` |
|       - | 1307 | ` *  Expand 4 (php)` |
|       - | 1308 | ` */` |
|      18 | 1309 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1310 | `{` |
|       9 | 1311 | `	SXUNUSED(pUserData); /* cc warning */` |
|      19 | 1312 | `	ph7_value_int(pVal,4);` |
|      19 | 1313 | `}` |
|       - | 1314 | `/*` |
|       - | 1315 | ` * LOCK_EX.` |
|       - | 1316 | ` *  Expand 2 (php). PH7 used 1, which collided with LOCK_SH, and LOCK_UN was 0 — so` |
|       - | 1317 | ` *  flock($h, LOCK_UN) asked the stream for a SHARED lock instead of releasing one.` |
|       - | 1318 | ` */` |
|      18 | 1319 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1320 | `{` |
|       9 | 1321 | `	SXUNUSED(pUserData); /* cc warning */` |
|      19 | 1322 | `	ph7_value_int(pVal,2);` |
|      19 | 1323 | `}` |
|       - | 1324 | `/*` |
|       - | 1325 | ` * LOCK_UN.` |
|       - | 1326 | ` *  Expand 3 (php)` |
|       - | 1327 | ` */` |
|       8 | 1328 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1329 | `{` |
|       4 | 1330 | `	SXUNUSED(pUserData); /* cc warning */` |
|       9 | 1331 | `	ph7_value_int(pVal,3);` |
|       9 | 1332 | `}` |
|       - | 1333 | `/*` |
|       - | 1334 | ` * FILE_USE_INCLUDE_PATH` |
|       - | 1335 | ` *  Expand 0x01 (Must be a power of two)` |
|       - | 1336 | ` */` |
|       4 | 1337 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1338 | `{` |
|       2 | 1339 | `	SXUNUSED(pUserData); /* cc warning */` |
|       5 | 1340 | `	ph7_value_int(pVal,0x1);` |
|       5 | 1341 | `}` |
|       - | 1342 | `/*` |
|       - | 1343 | ` * FILE_IGNORE_NEW_LINES` |
|       - | 1344 | ` *  Expand 0x02 (Must be a power of two)` |
|       - | 1345 | ` */` |
|      12 | 1346 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1347 | `{` |
|       6 | 1348 | `	SXUNUSED(pUserData); /* cc warning */` |
|      13 | 1349 | `	ph7_value_int(pVal,0x2);` |
|      13 | 1350 | `}` |
|       - | 1351 | `/*` |
|       - | 1352 | ` * FILE_SKIP_EMPTY_LINES` |
|       - | 1353 | ` *  Expand 0x04 (Must be a power of two)` |
|       - | 1354 | ` */` |
|       4 | 1355 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1356 | `{` |
|       2 | 1357 | `	SXUNUSED(pUserData); /* cc warning */` |
|       5 | 1358 | `	ph7_value_int(pVal,0x4);` |
|       5 | 1359 | `}` |
|       - | 1360 | `/*` |
|       - | 1361 | ` * FILE_APPEND` |
|       - | 1362 | ` *  Expand 0x08 (Must be a power of two)` |
|       - | 1363 | ` */` |
|       4 | 1364 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1365 | `{` |
|       2 | 1366 | `	SXUNUSED(pUserData); /* cc warning */` |
|       5 | 1367 | `	ph7_value_int(pVal,0x08);` |
|       5 | 1368 | `}` |
|       - | 1369 | `/*` |
|       - | 1370 | ` * FILE_NO_DEFAULT_CONTEXT` |
|       - | 1371 | ` *  Expand php's 0x10. PHL has no stream_context_set_default(), so "do not use` |
|       - | 1372 | ` *  the default context" is already how every call behaves — the flag is` |
|       - | 1373 | ` *  accepted (file()'s validator counts it a valid bit) and changes nothing,` |
|       - | 1374 | ` *  which is php's own behaviour when no default context was ever set.` |
|       - | 1375 | ` */` |
|       4 | 1376 | `static void PH7_FILE_NO_DEFAULT_CONTEXT_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1377 | `{` |
|       2 | 1378 | `	SXUNUSED(pUserData); /* cc warning */` |
|       5 | 1379 | `	ph7_value_int(pVal,0x10);` |
|       5 | 1380 | `}` |
|       - | 1381 | `/*` |
|       - | 1382 | ` * SCANDIR_SORT_ASCENDING` |
|       - | 1383 | ` *  Expand 0` |
|       - | 1384 | ` */` |
|    2324 | 1385 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1386 | `{` |
|    1162 | 1387 | `	SXUNUSED(pUserData); /* cc warning */` |
|    2329 | 1388 | `	ph7_value_int(pVal,0);` |
|    2329 | 1389 | `}` |
|       - | 1390 | `/*` |
|       - | 1391 | ` * SCANDIR_SORT_DESCENDING` |
|       - | 1392 | ` *  Expand 1` |
|       - | 1393 | ` */` |
|       6 | 1394 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1395 | `{` |
|       3 | 1396 | `	SXUNUSED(pUserData); /* cc warning */` |
|       7 | 1397 | `	ph7_value_int(pVal,1);` |
|       7 | 1398 | `}` |
|       - | 1399 | `/*` |
|       - | 1400 | ` * SCANDIR_SORT_NONE` |
|       - | 1401 | ` *  Expand 2` |
|       - | 1402 | ` */` |
|    1172 | 1403 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1404 | `{` |
|     586 | 1405 | `	SXUNUSED(pUserData); /* cc warning */` |
|    1177 | 1406 | `	ph7_value_int(pVal,2);` |
|    1177 | 1407 | `}` |
|       - | 1408 | `/*` |
|       - | 1409 | ` * GLOB_MARK` |
|       - | 1410 | ` *  Expand php's 0x08 (php's own portable glob flag set)` |
|       - | 1411 | ` */` |
|      18 | 1412 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1413 | `{` |
|       9 | 1414 | `	SXUNUSED(pUserData); /* cc warning */` |
|      19 | 1415 | `	ph7_value_int(pVal,PH7_GLOB_MARK);` |
|      19 | 1416 | `}` |
|       - | 1417 | `/*` |
|       - | 1418 | ` * GLOB_NOSORT` |
|       - | 1419 | ` *  Expand php's 0x20` |
|       - | 1420 | ` */` |
|      50 | 1421 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1422 | `{` |
|      25 | 1423 | `	SXUNUSED(pUserData); /* cc warning */` |
|      51 | 1424 | `	ph7_value_int(pVal,PH7_GLOB_NOSORT);` |
|      51 | 1425 | `}` |
|       - | 1426 | `/*` |
|       - | 1427 | ` * GLOB_NOCHECK` |
|       - | 1428 | ` *  Expand php's 0x10` |
|       - | 1429 | ` */` |
|      58 | 1430 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1431 | `{` |
|      29 | 1432 | `	SXUNUSED(pUserData); /* cc warning */` |
|      59 | 1433 | `	ph7_value_int(pVal,PH7_GLOB_NOCHECK);` |
|      59 | 1434 | `}` |
|       - | 1435 | `/*` |
|       - | 1436 | ` * GLOB_NOESCAPE` |
|       - | 1437 | ` *  Expand php's 0x1000` |
|       - | 1438 | ` */` |
|       6 | 1439 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1440 | `{` |
|       3 | 1441 | `	SXUNUSED(pUserData); /* cc warning */` |
|       7 | 1442 | `	ph7_value_int(pVal,PH7_GLOB_NOESCAPE);` |
|       7 | 1443 | `}` |
|       - | 1444 | `/*` |
|       - | 1445 | ` * GLOB_BRACE` |
|       - | 1446 | ` *  Expand php's 0x80` |
|       - | 1447 | ` */` |
|      66 | 1448 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1449 | `{` |
|      33 | 1450 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1451 | `	ph7_value_int(pVal,PH7_GLOB_BRACE);` |
|      67 | 1452 | `}` |
|       - | 1453 | `/*` |
|       - | 1454 | ` * GLOB_ONLYDIR` |
|       - | 1455 | ` *  Expand php's 0x40000000` |
|       - | 1456 | ` */` |
|      34 | 1457 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1458 | `{` |
|      17 | 1459 | `	SXUNUSED(pUserData); /* cc warning */` |
|      35 | 1460 | `	ph7_value_int(pVal,PH7_GLOB_ONLYDIR);` |
|      35 | 1461 | `}` |
|       - | 1462 | `/*` |
|       - | 1463 | ` * GLOB_ERR` |
|       - | 1464 | ` *  Expand php's 0x04` |
|       - | 1465 | ` */` |
|       8 | 1466 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1467 | `{` |
|       4 | 1468 | `	SXUNUSED(pUserData); /* cc warning */` |
|       9 | 1469 | `	ph7_value_int(pVal,PH7_GLOB_ERR);` |
|       9 | 1470 | `}` |
|       - | 1471 | `/*` |
|       - | 1472 | ` * GLOB_AVAILABLE_FLAGS` |
|       - | 1473 | ` *  Expand the OR of every glob flag php's portable glob accepts — the mask` |
|       - | 1474 | ` *  glob() itself validates against (1073746108 on every platform, since the` |
|       - | 1475 | ` *  GLOB_* values are php 8.5's own portable set).` |
|       - | 1476 | ` */` |
|      72 | 1477 | `static void PH7_GLOB_AVAILABLE_FLAGS_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1478 | `{` |
|      36 | 1479 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1480 | `	ph7_value_int(pVal,PH7_GLOB_ERR\|PH7_GLOB_MARK\|PH7_GLOB_NOCHECK\|PH7_GLOB_NOSORT` |
|       - | 1481 | `		\|PH7_GLOB_BRACE\|PH7_GLOB_NOESCAPE\|PH7_GLOB_ONLYDIR);` |
|      73 | 1482 | `}` |
|       - | 1483 | `/*` |
|       - | 1484 | ` * STDIN` |
|       - | 1485 | ` *  Expand the STDIN handle as a resource.` |
|       - | 1486 | ` */` |
|       2 | 1487 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1488 | `{` |
|       3 | 1489 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 1490 | `	void *pResource;` |
|       3 | 1491 | `	pResource = PH7_ExportStdin(pVm);` |
|       3 | 1492 | `	ph7_value_resource(pVal,pResource);` |
|       3 | 1493 | `}` |
|       - | 1494 | `/*` |
|       - | 1495 | ` * STDOUT` |
|       - | 1496 | ` *   Expand the STDOUT handle as a resource.` |
|       - | 1497 | ` */` |
|       8 | 1498 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1499 | `{` |
|       9 | 1500 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 1501 | `	void *pResource;` |
|       9 | 1502 | `	pResource = PH7_ExportStdout(pVm);` |
|       9 | 1503 | `	ph7_value_resource(pVal,pResource);` |
|       9 | 1504 | `}` |
|       - | 1505 | `/*` |
|       - | 1506 | ` * STDERR` |
|       - | 1507 | ` *  Expand the STDERR handle as a resource.` |
|       - | 1508 | ` */` |
|      10 | 1509 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1510 | `{` |
|      11 | 1511 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 1512 | `	void *pResource;` |
|      11 | 1513 | `	pResource = PH7_ExportStderr(pVm);` |
|      11 | 1514 | `	ph7_value_resource(pVal,pResource);` |
|      11 | 1515 | `}` |
|       - | 1516 | `/*` |
|       - | 1517 | ` * INI_SCANNER_NORMAL` |
|       - | 1518 | ` *   Expand php's 0` |
|       - | 1519 | ` */` |
|       6 | 1520 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1521 | `{` |
|       3 | 1522 | `	SXUNUSED(pUserData); /* cc warning */` |
|       7 | 1523 | `	ph7_value_int(pVal,PH7_INI_SCANNER_NORMAL);` |
|       7 | 1524 | `}` |
|       - | 1525 | `/*` |
|       - | 1526 | ` * INI_SCANNER_RAW` |
|       - | 1527 | ` *   Expand php's 1` |
|       - | 1528 | ` */` |
|       8 | 1529 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1530 | `{` |
|       4 | 1531 | `	SXUNUSED(pUserData); /* cc warning */` |
|       9 | 1532 | `	ph7_value_int(pVal,PH7_INI_SCANNER_RAW);` |
|       9 | 1533 | `}` |
|       - | 1534 | `/*` |
|       - | 1535 | ` * INI_SCANNER_TYPED` |
|       - | 1536 | ` *   Expand 2 (php's value)` |
|       - | 1537 | ` */` |
|       8 | 1538 | `static void PH7_INI_SCANNER_TYPED_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1539 | `{` |
|       4 | 1540 | `	SXUNUSED(pUserData); /* cc warning */` |
|       9 | 1541 | `	ph7_value_int(pVal,PH7_INI_SCANNER_TYPED);` |
|       9 | 1542 | `}` |
|       - | 1543 | `/*` |
|       - | 1544 | ` * EXTR_OVERWRITE` |
|       - | 1545 | ` *   Expand 0 (php's enum value; see PH7_EXTR_* in ph7int.h)` |
|       - | 1546 | ` */` |
|      14 | 1547 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|       2 | 1548 | `{` |
|       7 | 1549 | `	SXUNUSED(pUserData); /* cc warning */` |
|      16 | 1550 | `	ph7_value_int(pVal,PH7_EXTR_OVERWRITE);` |
|      16 | 1551 | `}` |
|       - | 1552 | `/*` |
|       - | 1553 | ` * EXTR_SKIP` |
|       - | 1554 | ` *   Expand 1` |
|       - | 1555 | ` */` |
|      16 | 1556 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1557 | `{` |
|       8 | 1558 | `	SXUNUSED(pUserData); /* cc warning */` |
|      19 | 1559 | `	ph7_value_int(pVal,PH7_EXTR_SKIP);` |
|      19 | 1560 | `}` |
|       - | 1561 | `/*` |
|       - | 1562 | ` * EXTR_PREFIX_SAME` |
|       - | 1563 | ` *   Expand 2` |
|       - | 1564 | ` */` |
|      38 | 1565 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1566 | `{` |
|      19 | 1567 | `	SXUNUSED(pUserData); /* cc warning */` |
|      41 | 1568 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_SAME);` |
|      41 | 1569 | `}` |
|       - | 1570 | `/*` |
|       - | 1571 | ` * EXTR_PREFIX_ALL` |
|       - | 1572 | ` *   Expand 3` |
|       - | 1573 | ` */` |
|      24 | 1574 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       2 | 1575 | `{` |
|      12 | 1576 | `	SXUNUSED(pUserData); /* cc warning */` |
|      26 | 1577 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_ALL);` |
|      26 | 1578 | `}` |
|       - | 1579 | `/*` |
|       - | 1580 | ` * EXTR_PREFIX_INVALID` |
|       - | 1581 | ` *   Expand 4` |
|       - | 1582 | ` */` |
|       8 | 1583 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1584 | `{` |
|       4 | 1585 | `	SXUNUSED(pUserData); /* cc warning */` |
|       9 | 1586 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_INVALID);` |
|       9 | 1587 | `}` |
|       - | 1588 | `/*` |
|       - | 1589 | ` * EXTR_IF_EXISTS` |
|       - | 1590 | ` *   Expand 6 (php orders IF_EXISTS after PREFIX_IF_EXISTS)` |
|       - | 1591 | ` */` |
|      16 | 1592 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1593 | `{` |
|       8 | 1594 | `	SXUNUSED(pUserData); /* cc warning */` |
|      20 | 1595 | `	ph7_value_int(pVal,PH7_EXTR_IF_EXISTS);` |
|      20 | 1596 | `}` |
|       - | 1597 | `/*` |
|       - | 1598 | ` * EXTR_REFS` |
|       - | 1599 | ` *   Expand 256 (the bit that rides above the mode: bind by REFERENCE)` |
|       - | 1600 | ` */` |
|      14 | 1601 | `static void PH7_EXTR_REFS_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1602 | `{` |
|       7 | 1603 | `	SXUNUSED(pUserData); /* cc warning */` |
|      15 | 1604 | `	ph7_value_int(pVal,PH7_EXTR_REFS);` |
|      15 | 1605 | `}` |
|       - | 1606 | `/*` |
|       - | 1607 | ` * EXTR_PREFIX_IF_EXISTS` |
|       - | 1608 | ` *   Expand 5` |
|       - | 1609 | ` */` |
|      20 | 1610 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|       2 | 1611 | `{` |
|      10 | 1612 | `	SXUNUSED(pUserData); /* cc warning */` |
|      22 | 1613 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_IF_EXISTS);` |
|      22 | 1614 | `}` |
|       - | 1615 | `/*` |
|       - | 1616 | ` * JSON_HEX_TAG.` |
|       - | 1617 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|       - | 1618 | ` */` |
|       6 | 1619 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1620 | `{` |
|       3 | 1621 | `	SXUNUSED(pUserData); /* cc warning */` |
|       7 | 1622 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|       7 | 1623 | `}` |
|       - | 1624 | `/*` |
|       - | 1625 | ` * JSON_HEX_AMP.` |
|       - | 1626 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|       - | 1627 | ` */` |
|       6 | 1628 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1629 | `{` |
|       3 | 1630 | `	SXUNUSED(pUserData); /* cc warning */` |
|       7 | 1631 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|       7 | 1632 | `}` |
|       - | 1633 | `/*` |
|       - | 1634 | ` * JSON_HEX_APOS.` |
|       - | 1635 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|       - | 1636 | ` */` |
|       6 | 1637 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1638 | `{` |
|       3 | 1639 | `	SXUNUSED(pUserData); /* cc warning */` |
|       7 | 1640 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|       7 | 1641 | `}` |
|       - | 1642 | `/*` |
|       - | 1643 | ` * JSON_HEX_QUOT.` |
|       - | 1644 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|       - | 1645 | ` */` |
|       6 | 1646 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1647 | `{` |
|       3 | 1648 | `	SXUNUSED(pUserData); /* cc warning */` |
|       7 | 1649 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|       7 | 1650 | `}` |
|       - | 1651 | `/*` |
|       - | 1652 | ` * JSON_FORCE_OBJECT.` |
|       - | 1653 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|       - | 1654 | ` */` |
|       6 | 1655 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1656 | `{` |
|       3 | 1657 | `	SXUNUSED(pUserData); /* cc warning */` |
|       7 | 1658 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|       7 | 1659 | `}` |
|       - | 1660 | `/*` |
|       - | 1661 | ` * JSON_NUMERIC_CHECK.` |
|       - | 1662 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|       - | 1663 | ` */` |
|      10 | 1664 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1665 | `{` |
|       5 | 1666 | `	SXUNUSED(pUserData); /* cc warning */` |
|      11 | 1667 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      11 | 1668 | `}` |
|       - | 1669 | `/*` |
|       - | 1670 | ` * JSON_BIGINT_AS_STRING.` |
|       - | 1671 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|       - | 1672 | ` */` |
|      14 | 1673 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1674 | `{` |
|       7 | 1675 | `	SXUNUSED(pUserData); /* cc warning */` |
|      15 | 1676 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      15 | 1677 | `}` |
|       - | 1678 | `/*` |
|       - | 1679 | ` * JSON_PARTIAL_OUTPUT_ON_ERROR.` |
|       - | 1680 | ` *   Expand the value of JSON_PARTIAL_OUTPUT_ON_ERROR defined in ph7Int.h.` |
|       - | 1681 | ` */` |
|      24 | 1682 | `static void PH7_JSON_PARTIAL_OUTPUT_ON_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1683 | `{` |
|      12 | 1684 | `	SXUNUSED(pUserData); /* cc warning */` |
|      25 | 1685 | `	ph7_value_int(pVal,JSON_PARTIAL_OUTPUT_ON_ERROR);` |
|      25 | 1686 | `}` |
|       - | 1687 | `/*` |
|       - | 1688 | ` * JSON_PRESERVE_ZERO_FRACTION.` |
|       - | 1689 | ` *   Expand the value of JSON_PRESERVE_ZERO_FRACTION defined in ph7Int.h.` |
|       - | 1690 | ` */` |
|      14 | 1691 | `static void PH7_JSON_PRESERVE_ZERO_FRACTION_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1692 | `{` |
|       7 | 1693 | `	SXUNUSED(pUserData); /* cc warning */` |
|      15 | 1694 | `	ph7_value_int(pVal,JSON_PRESERVE_ZERO_FRACTION);` |
|      15 | 1695 | `}` |
|       - | 1696 | `/*` |
|       - | 1697 | ` * JSON_OBJECT_AS_ARRAY.` |
|       - | 1698 | ` *   Expand the value of JSON_OBJECT_AS_ARRAY defined in ph7Int.h.` |
|       - | 1699 | ` */` |
|      10 | 1700 | `static void PH7_JSON_OBJECT_AS_ARRAY_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1701 | `{` |
|       5 | 1702 | `	SXUNUSED(pUserData); /* cc warning */` |
|      11 | 1703 | `	ph7_value_int(pVal,JSON_OBJECT_AS_ARRAY);` |
|      11 | 1704 | `}` |
|       - | 1705 | `/*` |
|       - | 1706 | ` * JSON_PRETTY_PRINT.` |
|       - | 1707 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|       - | 1708 | ` */` |
|      12 | 1709 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|       2 | 1710 | `{` |
|       6 | 1711 | `	SXUNUSED(pUserData); /* cc warning */` |
|      14 | 1712 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      14 | 1713 | `}` |
|       - | 1714 | `/*` |
|       - | 1715 | ` * JSON_UNESCAPED_SLASHES.` |
|       - | 1716 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|       - | 1717 | ` */` |
|      10 | 1718 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|       2 | 1719 | `{` |
|       5 | 1720 | `	SXUNUSED(pUserData); /* cc warning */` |
|      12 | 1721 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      12 | 1722 | `}` |
|       - | 1723 | `/*` |
|       - | 1724 | ` * JSON_UNESCAPED_UNICODE.` |
|       - | 1725 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|       - | 1726 | ` */` |
|      22 | 1727 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1728 | `{` |
|      11 | 1729 | `	SXUNUSED(pUserData); /* cc warning */` |
|      23 | 1730 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|      23 | 1731 | `}` |
|       - | 1732 | `/*` |
|       - | 1733 | ` * JSON_UNESCAPED_LINE_TERMINATORS.` |
|       - | 1734 | ` *   Expand the value of JSON_UNESCAPED_LINE_TERMINATORS defined in ph7Int.h.` |
|       - | 1735 | ` */` |
|       6 | 1736 | `static void PH7_JSON_UNESCAPED_LINE_TERMINATORS_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1737 | `{` |
|       3 | 1738 | `	SXUNUSED(pUserData); /* cc warning */` |
|       7 | 1739 | `	ph7_value_int(pVal,JSON_UNESCAPED_LINE_TERMINATORS);` |
|       7 | 1740 | `}` |
|       - | 1741 | `/*` |
|       - | 1742 | ` * JSON_INVALID_UTF8_IGNORE.` |
|       - | 1743 | ` *   Expand the value of JSON_INVALID_UTF8_IGNORE defined in ph7Int.h.` |
|       - | 1744 | ` */` |
|      26 | 1745 | `static void PH7_JSON_INVALID_UTF8_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|       2 | 1746 | `{` |
|      13 | 1747 | `	SXUNUSED(pUserData); /* cc warning */` |
|      28 | 1748 | `	ph7_value_int(pVal,JSON_INVALID_UTF8_IGNORE);` |
|      28 | 1749 | `}` |
|       - | 1750 | `/*` |
|       - | 1751 | ` * JSON_INVALID_UTF8_SUBSTITUTE.` |
|       - | 1752 | ` *   Expand the value of JSON_INVALID_UTF8_SUBSTITUTE defined in ph7Int.h.` |
|       - | 1753 | ` */` |
|      30 | 1754 | `static void PH7_JSON_INVALID_UTF8_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1755 | `{` |
|      15 | 1756 | `	SXUNUSED(pUserData); /* cc warning */` |
|      31 | 1757 | `	ph7_value_int(pVal,JSON_INVALID_UTF8_SUBSTITUTE);` |
|      31 | 1758 | `}` |
|       - | 1759 | `/*` |
|       - | 1760 | ` * JSON_THROW_ON_ERROR.` |
|       - | 1761 | ` *   Expand the value of JSON_THROW_ON_ERROR defined in ph7Int.h.` |
|       - | 1762 | ` */` |
|      20 | 1763 | `static void PH7_JSON_THROW_ON_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       2 | 1764 | `{` |
|      10 | 1765 | `	SXUNUSED(pUserData); /* cc warning */` |
|      22 | 1766 | `	ph7_value_int(pVal,JSON_THROW_ON_ERROR);` |
|      22 | 1767 | `}` |
|       - | 1768 | `/*` |
|       - | 1769 | ` * JSON_ERROR_NONE.` |
|       - | 1770 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|       - | 1771 | ` */` |
|       4 | 1772 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1773 | `{` |
|       2 | 1774 | `	SXUNUSED(pUserData); /* cc warning */` |
|       5 | 1775 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|       5 | 1776 | `}` |
|       - | 1777 | `/*` |
|       - | 1778 | ` * JSON_ERROR_DEPTH.` |
|       - | 1779 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|       - | 1780 | ` */` |
|       2 | 1781 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1782 | `{` |
|       1 | 1783 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 | 1784 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|       3 | 1785 | `}` |
|       - | 1786 | `/*` |
|       - | 1787 | ` * JSON_ERROR_STATE_MISMATCH.` |
|       - | 1788 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|       - | 1789 | ` */` |
|       2 | 1790 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1791 | `{` |
|       1 | 1792 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 | 1793 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|       3 | 1794 | `}` |
|       - | 1795 | `/*` |
|       - | 1796 | ` * JSON_ERROR_CTRL_CHAR.` |
|       - | 1797 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|       - | 1798 | ` */` |
|       2 | 1799 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1800 | `{` |
|       1 | 1801 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 | 1802 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|       3 | 1803 | `}` |
|       - | 1804 | `/*` |
|       - | 1805 | ` * JSON_ERROR_SYNTAX.` |
|       - | 1806 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|       - | 1807 | ` */` |
|       4 | 1808 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1809 | `{` |
|       2 | 1810 | `	SXUNUSED(pUserData); /* cc warning */` |
|       5 | 1811 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|       5 | 1812 | `}` |
|       - | 1813 | `/*` |
|       - | 1814 | ` * JSON_ERROR_UTF8.` |
|       - | 1815 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|       - | 1816 | ` */` |
|       2 | 1817 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1818 | `{` |
|       1 | 1819 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 | 1820 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|       3 | 1821 | `}` |
|       - | 1822 | `/*` |
|       - | 1823 | ` * JSON_ERROR_RECURSION.` |
|       - | 1824 | ` *   Expand the value of JSON_ERROR_RECURSION defined in ph7Int.h.` |
|       - | 1825 | ` */` |
|       2 | 1826 | `static void PH7_JSON_ERROR_RECURSION_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1827 | `{` |
|       1 | 1828 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 | 1829 | `	ph7_value_int(pVal,JSON_ERROR_RECURSION);` |
|       3 | 1830 | `}` |
|       - | 1831 | `/*` |
|       - | 1832 | ` * JSON_ERROR_UNSUPPORTED_TYPE.` |
|       - | 1833 | ` *   Expand the value of JSON_ERROR_UNSUPPORTED_TYPE defined in ph7Int.h.` |
|       - | 1834 | ` */` |
|       2 | 1835 | `static void PH7_JSON_ERROR_UNSUPPORTED_TYPE_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1836 | `{` |
|       1 | 1837 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 | 1838 | `	ph7_value_int(pVal,JSON_ERROR_UNSUPPORTED_TYPE);` |
|       3 | 1839 | `}` |
|       - | 1840 | `/*` |
|       - | 1841 | ` * JSON_ERROR_INVALID_PROPERTY_NAME.` |
|       - | 1842 | ` *   Expand the value of JSON_ERROR_INVALID_PROPERTY_NAME defined in ph7Int.h.` |
|       - | 1843 | ` */` |
|       2 | 1844 | `static void PH7_JSON_ERROR_INVALID_PROPERTY_NAME_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1845 | `{` |
|       1 | 1846 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 | 1847 | `	ph7_value_int(pVal,JSON_ERROR_INVALID_PROPERTY_NAME);` |
|       3 | 1848 | `}` |
|       - | 1849 | `/*` |
|       - | 1850 | ` * JSON_ERROR_UTF16.` |
|       - | 1851 | ` *   Expand the value of JSON_ERROR_UTF16 defined in ph7Int.h.` |
|       - | 1852 | ` */` |
|       4 | 1853 | `static void PH7_JSON_ERROR_UTF16_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1854 | `{` |
|       2 | 1855 | `	SXUNUSED(pUserData); /* cc warning */` |
|       5 | 1856 | `	ph7_value_int(pVal,JSON_ERROR_UTF16);` |
|       5 | 1857 | `}` |
|       - | 1858 | `/*` |
|       - | 1859 | ` * JSON_ERROR_NON_BACKED_ENUM.` |
|       - | 1860 | ` *   Expand the value of JSON_ERROR_NON_BACKED_ENUM defined in ph7Int.h (php 8.1).` |
|       - | 1861 | ` */` |
|     ! 0 | 1862 | `static void PH7_JSON_ERROR_NON_BACKED_ENUM_Const(ph7_value *pVal,void *pUserData)` |
|     ! 0 | 1863 | `{` |
|     ! 0 | 1864 | `	SXUNUSED(pUserData); /* cc warning */` |
|     ! 0 | 1865 | `	ph7_value_int(pVal,JSON_ERROR_NON_BACKED_ENUM);` |
|     ! 0 | 1866 | `}` |
|       - | 1867 | `/*` |
|       - | 1868 | ` * JSON_ERROR_INF_OR_NAN.` |
|       - | 1869 | ` *   Expand the value of JSON_ERROR_INF_OR_NAN defined in ph7Int.h.` |
|       - | 1870 | ` */` |
|       2 | 1871 | `static void PH7_JSON_ERROR_INF_OR_NAN_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1872 | `{` |
|       1 | 1873 | `	SXUNUSED(pUserData); /* cc warning */` |
|       3 | 1874 | `	ph7_value_int(pVal,JSON_ERROR_INF_OR_NAN);` |
|       3 | 1875 | `}` |
|       - | 1876 | `/*` |
|       - | 1877 | ` * __CLASS__` |
|       - | 1878 | ` *  The current class name, or the EMPTY STRING outside any class — php answers "",` |
|       - | 1879 | `` *  not null (`__CLASS__ === ""` is true in global scope). `self` keeps its own`` |
|       - | 1880 | ` *  expander below because php treats IT differently outside a class scope.` |
|       - | 1881 | ` */` |
|      16 | 1882 | `static void PH7_class_magic_Const(ph7_value *pVal,void *pUserData)` |
|       1 | 1883 | `{` |
|      17 | 1884 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 1885 | `	ph7_class *pClass;` |
|       - | 1886 | `	/* php flattens a trait into the class that used it, so __CLASS__ inside a trait method` |
|       - | 1887 | `	 * is THAT class (where __TRAIT__ and __METHOD__ stay the trait's — php's own asymmetry). */` |
|      17 | 1888 | `	pClass = PH7_VmPeekSelfClass(pVm);` |
|      17 | 1889 | `	if( pClass == 0 ){` |
|       3 | 1890 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|       1 | 1891 | `	}` |
|      17 | 1892 | `	if( pClass ){` |
|      15 | 1893 | `		SyString *pName = &pClass->sName;` |
|      15 | 1894 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|       8 | 1895 | `	}else{` |
|       3 | 1896 | `		ph7_value_string(pVal,"",0);` |
|       - | 1897 | `	}` |
|      17 | 1898 | `}` |
|       - | 1899 |  |
|       - | 1900 | `/*` |
|       - | 1901 | ` * PASSWORD_BCRYPT / PASSWORD_DEFAULT` |
|       - | 1902 | ` *  The bcrypt algorithm identifier (PHP 7.4+ exposes these as the string "2y").` |
|       - | 1903 | ` *  PASSWORD_DEFAULT tracks the recommended default, currently bcrypt.` |
|       - | 1904 | ` */` |
|      22 | 1905 | `static void PH7_PASSWORD_BCRYPT_Const(ph7_value *pVal,void *pUnused)` |
|       2 | 1906 | `{` |
|      11 | 1907 | `	SXUNUSED(pUnused);` |
|      24 | 1908 | `	ph7_value_string(pVal,"2y",(int)sizeof("2y")-1);` |
|      24 | 1909 | `}` |
|       - | 1910 | `/*` |
|       - | 1911 | ` * PASSWORD_BCRYPT_DEFAULT_COST` |
|       - | 1912 | ` *  The default bcrypt work factor used by password_hash() (currently 12).` |
|       - | 1913 | ` */` |
|       2 | 1914 | `static void PH7_PASSWORD_COST_Const(ph7_value *pVal,void *pUnused)` |
|       1 | 1915 | `{` |
|       1 | 1916 | `	SXUNUSED(pUnused);` |
|       3 | 1917 | `	ph7_value_int(pVal,12);` |
|       3 | 1918 | `}` |
|       - | 1919 | `/*` |
|       - | 1920 | ` * filter_var() filter and flag identifiers (the ext/filter constants). Values` |
|       - | 1921 | ` * match PHP 8.5. One tiny int-returning callback per constant, generated by a` |
|       - | 1922 | ` * local macro to keep the ~25 near-identical definitions DRY.` |
|       - | 1923 | ` */` |
|       - | 1924 | `#define PH7_FILTER_INT_CONST(Name,Val) \` |
|       - | 1925 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|       - | 1926 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|       - | 1927 | `	}` |
|      10 | 1928 | `PH7_FILTER_INT_CONST(FILTER_DEFAULT,516)` |
|      17 | 1929 | `PH7_FILTER_INT_CONST(FILTER_UNSAFE_RAW,516)` |
|      66 | 1930 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_INT,257)` |
|      29 | 1931 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_BOOLEAN,258)` |
|      69 | 1932 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_FLOAT,259)` |
|       8 | 1933 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_REGEXP,272)` |
|      11 | 1934 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_DOMAIN,277)` |
|      15 | 1935 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_URL,273)` |
|      28 | 1936 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_EMAIL,274)` |
|      25 | 1937 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_IP,275)` |
|      11 | 1938 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_MAC,276)` |
|       3 | 1939 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_INT,519)` |
|       5 | 1940 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_FLOAT,520)` |
|      13 | 1941 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_SPECIAL_CHARS,515)` |
|      25 | 1942 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_FULL_SPECIAL_CHARS,522)` |
|       3 | 1943 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_EMAIL,517)` |
|       3 | 1944 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_URL,518)` |
|       3 | 1945 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_OCTAL,1)` |
|       3 | 1946 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_HEX,2)` |
|       7 | 1947 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_LOW,4)` |
|       5 | 1948 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_HIGH,8)` |
|       5 | 1949 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_LOW,16)` |
|       5 | 1950 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_HIGH,32)` |
|       3 | 1951 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_AMP,64)` |
|       3 | 1952 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_ENCODE_QUOTES,128)` |
|       3 | 1953 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_BACKTICK,512)` |
|       3 | 1954 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_FRACTION,4096)` |
|      25 | 1955 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_THOUSAND,8192)` |
|       3 | 1956 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_SCIENTIFIC,16384)` |
|       5 | 1957 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV4,1048576)` |
|       3 | 1958 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV6,2097152)` |
|      14 | 1959 | `PH7_FILTER_INT_CONST(FILTER_NULL_ON_FAILURE,134217728)` |
|       - | 1960 | `/* filter_input() source selectors (php values; SESSION/REQUEST are undefined in 8.5) */` |
|       5 | 1961 | `PH7_FILTER_INT_CONST(INPUT_POST,0)` |
|       8 | 1962 | `PH7_FILTER_INT_CONST(INPUT_GET,1)` |
|       3 | 1963 | `PH7_FILTER_INT_CONST(INPUT_COOKIE,2)` |
|       3 | 1964 | `PH7_FILTER_INT_CONST(INPUT_ENV,4)` |
|      21 | 1965 | `PH7_FILTER_INT_CONST(INPUT_SERVER,5)` |
|       - | 1966 | `/*` |
|       - | 1967 | ` * Table of built-in constants.` |
|       - | 1968 | ` */` |
|       - | 1969 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|       - | 1970 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|       - | 1971 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|       - | 1972 | `	{"__PH7__",              PH7_VER_Const      },` |
|       - | 1973 | `	{"PHP_VERSION",          PH7_PHPVerConst    },` |
|       - | 1974 | `	{"PHP_MAJOR_VERSION",    PH7_PHPMajorConst  },` |
|       - | 1975 | `	{"PHP_MINOR_VERSION",    PH7_PHPMinorConst  },` |
|       - | 1976 | `	{"PHP_RELEASE_VERSION",  PH7_PHPReleaseConst},` |
|       - | 1977 | `	{"PHP_EXTRA_VERSION",    PH7_PHPExtraConst  },` |
|       - | 1978 | `	{"PHP_VERSION_ID",       PH7_PHPVerIdConst  },` |
|       - | 1979 | `	{"PHP_OS",               PH7_OS_Const       },` |
|       - | 1980 | `	{"PHP_OS_FAMILY",        PH7_OS_FAMILY_Const},` |
|       - | 1981 | `	{"PHP_SAPI",             PH7_SAPI_Const     },` |
|       - | 1982 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|       - | 1983 | `	{"PHP_SESSION_DISABLED", PH7_PHP_SESSION_DISABLED_Const },` |
|       - | 1984 | `	{"PHP_SESSION_NONE",     PH7_PHP_SESSION_NONE_Const },` |
|       - | 1985 | `	{"PHP_SESSION_ACTIVE",   PH7_PHP_SESSION_ACTIVE_Const },` |
|       - | 1986 | `	{"INI_USER",             PH7_INI_USER_Const },` |
|       - | 1987 | `	{"INI_PERDIR",           PH7_INI_PERDIR_Const },` |
|       - | 1988 | `	{"INI_SYSTEM",           PH7_INI_SYSTEM_Const },` |
|       - | 1989 | `	{"INI_ALL",              PH7_INI_ALL_Const },` |
|       - | 1990 | `	{"MB_CASE_UPPER",        PH7_MB_CASE_UPPER_Const },` |
|       - | 1991 | `	{"MB_CASE_LOWER",        PH7_MB_CASE_LOWER_Const },` |
|       - | 1992 | `	{"MB_CASE_TITLE",        PH7_MB_CASE_TITLE_Const },` |
|       - | 1993 | `	{"PASSWORD_BCRYPT",      PH7_PASSWORD_BCRYPT_Const },` |
|       - | 1994 | `	{"PASSWORD_DEFAULT",     PH7_PASSWORD_BCRYPT_Const },` |
|       - | 1995 | `	{"PASSWORD_BCRYPT_DEFAULT_COST", PH7_PASSWORD_COST_Const },` |
|       - | 1996 | `	{"FILTER_DEFAULT",              PH7_FILTER_DEFAULT_Const },` |
|       - | 1997 | `	{"FILTER_UNSAFE_RAW",           PH7_FILTER_UNSAFE_RAW_Const },` |
|       - | 1998 | `	{"FILTER_VALIDATE_INT",         PH7_FILTER_VALIDATE_INT_Const },` |
|       - | 1999 | `	{"FILTER_VALIDATE_BOOLEAN",     PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|       - | 2000 | `	{"FILTER_VALIDATE_BOOL",        PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|       - | 2001 | `	{"FILTER_VALIDATE_FLOAT",       PH7_FILTER_VALIDATE_FLOAT_Const },` |
|       - | 2002 | `	{"FILTER_VALIDATE_REGEXP",      PH7_FILTER_VALIDATE_REGEXP_Const },` |
|       - | 2003 | `	{"FILTER_VALIDATE_DOMAIN",      PH7_FILTER_VALIDATE_DOMAIN_Const },` |
|       - | 2004 | `	{"FILTER_VALIDATE_URL",         PH7_FILTER_VALIDATE_URL_Const },` |
|       - | 2005 | `	{"FILTER_VALIDATE_EMAIL",       PH7_FILTER_VALIDATE_EMAIL_Const },` |
|       - | 2006 | `	{"FILTER_VALIDATE_IP",          PH7_FILTER_VALIDATE_IP_Const },` |
|       - | 2007 | `	{"FILTER_VALIDATE_MAC",         PH7_FILTER_VALIDATE_MAC_Const },` |
|       - | 2008 | `	{"FILTER_SANITIZE_NUMBER_INT",  PH7_FILTER_SANITIZE_NUMBER_INT_Const },` |
|       - | 2009 | `	{"FILTER_SANITIZE_NUMBER_FLOAT",PH7_FILTER_SANITIZE_NUMBER_FLOAT_Const },` |
|       - | 2010 | `	{"FILTER_SANITIZE_SPECIAL_CHARS",PH7_FILTER_SANITIZE_SPECIAL_CHARS_Const },` |
|       - | 2011 | `	{"FILTER_SANITIZE_FULL_SPECIAL_CHARS",PH7_FILTER_SANITIZE_FULL_SPECIAL_CHARS_Const },` |
|       - | 2012 | `	{"FILTER_SANITIZE_EMAIL",       PH7_FILTER_SANITIZE_EMAIL_Const },` |
|       - | 2013 | `	{"FILTER_SANITIZE_URL",         PH7_FILTER_SANITIZE_URL_Const },` |
|       - | 2014 | `	{"FILTER_FLAG_ALLOW_OCTAL",     PH7_FILTER_FLAG_ALLOW_OCTAL_Const },` |
|       - | 2015 | `	{"FILTER_FLAG_ALLOW_HEX",       PH7_FILTER_FLAG_ALLOW_HEX_Const },` |
|       - | 2016 | `	{"FILTER_FLAG_STRIP_LOW",       PH7_FILTER_FLAG_STRIP_LOW_Const },` |
|       - | 2017 | `	{"FILTER_FLAG_STRIP_HIGH",      PH7_FILTER_FLAG_STRIP_HIGH_Const },` |
|       - | 2018 | `	{"FILTER_FLAG_ENCODE_LOW",      PH7_FILTER_FLAG_ENCODE_LOW_Const },` |
|       - | 2019 | `	{"FILTER_FLAG_ENCODE_HIGH",     PH7_FILTER_FLAG_ENCODE_HIGH_Const },` |
|       - | 2020 | `	{"FILTER_FLAG_ENCODE_AMP",      PH7_FILTER_FLAG_ENCODE_AMP_Const },` |
|       - | 2021 | `	{"FILTER_FLAG_NO_ENCODE_QUOTES",PH7_FILTER_FLAG_NO_ENCODE_QUOTES_Const },` |
|       - | 2022 | `	{"FILTER_FLAG_STRIP_BACKTICK",  PH7_FILTER_FLAG_STRIP_BACKTICK_Const },` |
|       - | 2023 | `	{"FILTER_FLAG_ALLOW_FRACTION",  PH7_FILTER_FLAG_ALLOW_FRACTION_Const },` |
|       - | 2024 | `	{"FILTER_FLAG_ALLOW_THOUSAND",  PH7_FILTER_FLAG_ALLOW_THOUSAND_Const },` |
|       - | 2025 | `	{"FILTER_FLAG_ALLOW_SCIENTIFIC",PH7_FILTER_FLAG_ALLOW_SCIENTIFIC_Const },` |
|       - | 2026 | `	{"FILTER_FLAG_IPV4",            PH7_FILTER_FLAG_IPV4_Const },` |
|       - | 2027 | `	{"FILTER_FLAG_IPV6",            PH7_FILTER_FLAG_IPV6_Const },` |
|       - | 2028 | `	{"FILTER_NULL_ON_FAILURE",      PH7_FILTER_NULL_ON_FAILURE_Const },` |
|       - | 2029 | `	{"INPUT_POST",                  PH7_INPUT_POST_Const },` |
|       - | 2030 | `	{"INPUT_GET",                   PH7_INPUT_GET_Const },` |
|       - | 2031 | `	{"INPUT_COOKIE",                PH7_INPUT_COOKIE_Const },` |
|       - | 2032 | `	{"INPUT_ENV",                   PH7_INPUT_ENV_Const },` |
|       - | 2033 | `	{"INPUT_SERVER",                PH7_INPUT_SERVER_Const },` |
|       - | 2034 | `	{"CAL_GREGORIAN",        PH7_CAL_GREGORIAN_Const },` |
|       - | 2035 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|       - | 2036 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|       - | 2037 | `	{"PHP_INT_MIN",          PH7_INTMIN_Const   },` |
|       - | 2038 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|       - | 2039 | `	{"PHP_FLOAT_EPSILON",    PH7_FLOATEPSILON_Const },` |
|       - | 2040 | `	{"PHP_FLOAT_MAX",        PH7_FLOATMAX_Const },` |
|       - | 2041 | `	{"PHP_FLOAT_MIN",        PH7_FLOATMIN_Const },` |
|       - | 2042 | `	{"PHP_FLOAT_DIG",        PH7_FLOATDIG_Const },` |
|       - | 2043 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|       - | 2044 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|       - | 2045 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|       - | 2046 | `	{"__TIME__",             PH7_TIME_Const     },` |
|       - | 2047 | `	{"__DATE__",             PH7_DATE_Const     },` |
|       - | 2048 | `	{"__FILE__",             PH7_FILE_Const     },` |
|       - | 2049 | `	{"__DIR__",              PH7_DIR_Const      },` |
|       - | 2050 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|       - | 2051 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|       - | 2052 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|       - | 2053 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|       - | 2054 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|       - | 2055 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|       - | 2056 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|       - | 2057 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|       - | 2058 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|       - | 2059 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|       - | 2060 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|       - | 2061 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|       - | 2062 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|       - | 2063 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|       - | 2064 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|       - | 2065 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|       - | 2066 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|       - | 2067 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|       - | 2068 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|       - | 2069 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|       - | 2070 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|       - | 2071 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|       - | 2072 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|       - | 2073 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|       - | 2074 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|       - | 2075 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|       - | 2076 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|       - | 2077 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|       - | 2078 | `	{"SORT_LOCALE_STRING",   PH7_SORT_LOCALE_STRING_Const },` |
|       - | 2079 | `	{"SORT_NATURAL",         PH7_SORT_NATURAL_Const },` |
|       - | 2080 | `	{"SORT_FLAG_CASE",       PH7_SORT_FLAG_CASE_Const },` |
|       - | 2081 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|       - | 2082 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|       - | 2083 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|       - | 2084 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|       - | 2085 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|       - | 2086 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|       - | 2087 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|       - | 2088 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|       - | 2089 | `	{"M_E",                  PH7_M_E_Const          },` |
|       - | 2090 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|       - | 2091 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|       - | 2092 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|       - | 2093 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|       - | 2094 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|       - | 2095 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|       - | 2096 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|       - | 2097 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|       - | 2098 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|       - | 2099 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|       - | 2100 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|       - | 2101 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|       - | 2102 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|       - | 2103 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|       - | 2104 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|       - | 2105 | `	{"NAN",                  PH7_NAN_Const          },` |
|       - | 2106 | `	{"INF",                  PH7_INF_Const          },` |
|       - | 2107 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|       - | 2108 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|       - | 2109 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|       - | 2110 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|       - | 2111 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|       - | 2112 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|       - | 2113 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|       - | 2114 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|       - | 2115 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|       - | 2116 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|       - | 2117 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|       - | 2118 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|       - | 2119 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|       - | 2120 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|       - | 2121 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|       - | 2122 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|       - | 2123 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|       - | 2124 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|       - | 2125 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|       - | 2126 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|       - | 2127 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|       - | 2128 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|       - | 2129 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|       - | 2130 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|       - | 2131 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|       - | 2132 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|       - | 2133 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|       - | 2134 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|       - | 2135 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|       - | 2136 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|       - | 2137 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|       - | 2138 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|       - | 2139 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|       - | 2140 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|       - | 2141 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|       - | 2142 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|       - | 2143 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|       - | 2144 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|       - | 2145 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|       - | 2146 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|       - | 2147 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|       - | 2148 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|       - | 2149 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|       - | 2150 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|       - | 2151 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|       - | 2152 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|       - | 2153 | `	{"PATHINFO_ALL",         PH7_PATHINFO_ALL_Const },` |
|       - | 2154 | `	/* ASSERT_QUIET_EVAL was REMOVED in php 8.0: referencing it is an Error there */` |
|       - | 2155 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|       - | 2156 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|       - | 2157 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|       - | 2158 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|       - | 2159 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|       - | 2160 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|       - | 2161 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|       - | 2162 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|       - | 2163 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|       - | 2164 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|       - | 2165 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|       - | 2166 | `	{"FILE_NO_DEFAULT_CONTEXT", PH7_FILE_NO_DEFAULT_CONTEXT_Const },` |
|       - | 2167 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|       - | 2168 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|       - | 2169 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|       - | 2170 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|       - | 2171 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|       - | 2172 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|       - | 2173 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|       - | 2174 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|       - | 2175 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|       - | 2176 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|       - | 2177 | `	{"GLOB_AVAILABLE_FLAGS", PH7_GLOB_AVAILABLE_FLAGS_Const },` |
|       - | 2178 | `	{"STDIN",                PH7_STDIN_Const        },` |
|       - | 2179 | `	{"stdin",                PH7_STDIN_Const        },` |
|       - | 2180 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|       - | 2181 | `	{"stdout",               PH7_STDOUT_Const       },` |
|       - | 2182 | `	{"STDERR",               PH7_STDERR_Const       },` |
|       - | 2183 | `	{"stderr",               PH7_STDERR_Const       },` |
|       - | 2184 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|       - | 2185 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|       - | 2186 | `	{"INI_SCANNER_TYPED",    PH7_INI_SCANNER_TYPED_Const  },` |
|       - | 2187 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|       - | 2188 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|       - | 2189 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|       - | 2190 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|       - | 2191 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|       - | 2192 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|       - | 2193 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|       - | 2194 | `	{"EXTR_REFS",            PH7_EXTR_REFS_Const        },` |
|       - | 2195 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|       - | 2196 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|       - | 2197 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|       - | 2198 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|       - | 2199 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|       - | 2200 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|       - | 2201 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|       - | 2202 | `	{"JSON_OBJECT_AS_ARRAY",   PH7_JSON_OBJECT_AS_ARRAY_Const},` |
|       - | 2203 | `	{"JSON_PARTIAL_OUTPUT_ON_ERROR", PH7_JSON_PARTIAL_OUTPUT_ON_ERROR_Const},` |
|       - | 2204 | `	{"JSON_PRESERVE_ZERO_FRACTION",  PH7_JSON_PRESERVE_ZERO_FRACTION_Const},` |
|       - | 2205 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|       - | 2206 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|       - | 2207 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|       - | 2208 | `	{"JSON_UNESCAPED_LINE_TERMINATORS", PH7_JSON_UNESCAPED_LINE_TERMINATORS_Const},` |
|       - | 2209 | `	{"JSON_INVALID_UTF8_IGNORE", PH7_JSON_INVALID_UTF8_IGNORE_Const},` |
|       - | 2210 | `	{"JSON_INVALID_UTF8_SUBSTITUTE", PH7_JSON_INVALID_UTF8_SUBSTITUTE_Const},` |
|       - | 2211 | `	{"JSON_THROW_ON_ERROR",    PH7_JSON_THROW_ON_ERROR_Const},` |
|       - | 2212 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|       - | 2213 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|       - | 2214 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|       - | 2215 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|       - | 2216 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|       - | 2217 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|       - | 2218 | `	{"JSON_ERROR_RECURSION", PH7_JSON_ERROR_RECURSION_Const},` |
|       - | 2219 | `	{"JSON_ERROR_UNSUPPORTED_TYPE", PH7_JSON_ERROR_UNSUPPORTED_TYPE_Const},` |
|       - | 2220 | `	{"JSON_ERROR_INVALID_PROPERTY_NAME", PH7_JSON_ERROR_INVALID_PROPERTY_NAME_Const},` |
|       - | 2221 | `	{"JSON_ERROR_UTF16",     PH7_JSON_ERROR_UTF16_Const},` |
|       - | 2222 | `	{"JSON_ERROR_NON_BACKED_ENUM", PH7_JSON_ERROR_NON_BACKED_ENUM_Const},` |
|       - | 2223 | `	{"JSON_ERROR_INF_OR_NAN", PH7_JSON_ERROR_INF_OR_NAN_Const},` |
|       - | 2224 | ``	/* `self`, `parent` and `static` are KEYWORDS in php, not constants: using one as a bare`` |
|       - | 2225 | ``	 * word is an "Undefined constant" Error (or a parse error for `static`). PH7 registered`` |
|       - | 2226 | `	 * them as constants that quietly expanded to the class name / NULL, so a typo'd bare` |
|       - | 2227 | ``	 * word silently produced a value. The `self::`/`parent::`/`static::` forms are handled`` |
|       - | 2228 | ``	 * by the `::` compile path and do not go through the constant table. */`` |
|       - | 2229 | `	{"__CLASS__",            PH7_class_magic_Const  }` |
|       - | 2230 | `};` |
|       - | 2231 | `/*` |
|       - | 2232 | ` * Register the built-in constants defined above.` |
|       - | 2233 | ` */` |
|    4076 | 2234 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|       5 | 2235 | `{` |
|       - | 2236 | `	sxu32 n;` |
|       - | 2237 | `	/*` |
|       - | 2238 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|       - | 2239 | `	 * that trigger the constant invocation as their private data.` |
|       - | 2240 | `	 */` |
| 1031233 | 2241 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 1027157 | 2242 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
|  513581 | 2243 | `	}` |
|    4081 | 2244 | `}` |
