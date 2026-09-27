# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1445/1461 lines (98.90%)

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
|     194 |   14 | `static void PH7_VER_Const(ph7_value *pVal,void *pUnused)` |
|       3 |   15 | `{` |
|      97 |   16 | `	SXUNUSED(pUnused);` |
|     197 |   17 | `	ph7_value_string(pVal,ph7_lib_signature(),-1/*Compute length automatically*/);` |
|     197 |   18 | `}` |
|       - |   19 | `/*` |
|       - |   20 | ` * PHP_VERSION, PHP_MAJOR_VERSION, PHP_MINOR_VERSION, PHP_RELEASE_VERSION,` |
|       - |   21 | ` * PHP_EXTRA_VERSION, PHP_VERSION_ID` |
|       - |   22 | ` *   Expand the PHP-compatibility version PHL advertises (see PHP_COMPAT_* in ph7.h).` |
|       - |   23 | ` */` |
|      76 |   24 | `static void PH7_PHPVerConst(ph7_value *pVal,void *pUnused)` |
|       3 |   25 | `{` |
|      38 |   26 | `	SXUNUSED(pUnused);` |
|      79 |   27 | `	ph7_value_string(pVal,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION)-1);` |
|      79 |   28 | `}` |
|      66 |   29 | `static void PH7_PHPMajorConst(ph7_value *pVal,void *pUnused)` |
|       3 |   30 | `{` |
|      33 |   31 | `	SXUNUSED(pUnused);` |
|      69 |   32 | `	ph7_value_int64(pVal,PHP_COMPAT_MAJOR_VERSION);` |
|      69 |   33 | `}` |
|      66 |   34 | `static void PH7_PHPMinorConst(ph7_value *pVal,void *pUnused)` |
|       3 |   35 | `{` |
|      33 |   36 | `	SXUNUSED(pUnused);` |
|      69 |   37 | `	ph7_value_int64(pVal,PHP_COMPAT_MINOR_VERSION);` |
|      69 |   38 | `}` |
|      66 |   39 | `static void PH7_PHPReleaseConst(ph7_value *pVal,void *pUnused)` |
|       3 |   40 | `{` |
|      33 |   41 | `	SXUNUSED(pUnused);` |
|      69 |   42 | `	ph7_value_int64(pVal,PHP_COMPAT_RELEASE_VERSION);` |
|      69 |   43 | `}` |
|      64 |   44 | `static void PH7_PHPExtraConst(ph7_value *pVal,void *pUnused)` |
|       3 |   45 | `{` |
|      32 |   46 | `	SXUNUSED(pUnused);` |
|      67 |   47 | `	ph7_value_string(pVal,PHP_COMPAT_EXTRA_VERSION,(int)sizeof(PHP_COMPAT_EXTRA_VERSION)-1);` |
|      67 |   48 | `}` |
|      66 |   49 | `static void PH7_PHPVerIdConst(ph7_value *pVal,void *pUnused)` |
|       3 |   50 | `{` |
|      33 |   51 | `	SXUNUSED(pUnused);` |
|      69 |   52 | `	ph7_value_int64(pVal,PHP_COMPAT_VERSION_ID);` |
|      69 |   53 | `}` |
|       - |   54 | `#ifdef __WINNT__` |
|       - |   55 | `#include <Windows.h>` |
|       - |   56 | `#elif defined(__UNIXES__)` |
|       - |   57 | `#include <sys/utsname.h>` |
|       - |   58 | `#endif` |
|       - |   59 | `/*` |
|       - |   60 | ` * PHP_OS` |
|       - |   61 | ` *  Expand the name of the host Operating System.` |
|       - |   62 | ` */` |
|    5330 |   63 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|       5 |   64 | `{` |
|       - |   65 | `#if defined(__WINNT__)` |
|       5 |   66 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|       - |   67 | `#elif defined(__UNIXES__)` |
|       - |   68 | `	struct utsname sInfo;` |
|    5330 |   69 | `	if( uname(&sInfo) != 0 ){` |
|     ! 0 |   70 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|     ! 0 |   71 | `	}else{` |
|    5330 |   72 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|       - |   73 | `	}` |
|       - |   74 | `#else` |
|       - |   75 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|       - |   76 | `#endif` |
|    2665 |   77 | `	SXUNUSED(pUnused);` |
|    5335 |   78 | `}` |
|       - |   79 | `/*` |
|       - |   80 | ` * PHP_OS_FAMILY (php 7.2)` |
|       - |   81 | ` *  One of 'Windows', 'BSD', 'Darwin', 'Solaris', 'Linux' or 'Unknown', derived` |
|       - |   82 | ` *  from the host's uname sysname (php maps the same set at build time).` |
|       - |   83 | ` */` |
|      88 |   84 | `static void PH7_OS_FAMILY_Const(ph7_value *pVal,void *pUnused)` |
|       4 |   85 | `{` |
|      44 |   86 | `	SXUNUSED(pUnused);` |
|       - |   87 | `#if defined(__WINNT__)` |
|       4 |   88 | `	ph7_value_string(pVal,"Windows",(int)sizeof("Windows")-1);` |
|       - |   89 | `#elif defined(__UNIXES__)` |
|       - |   90 | `	struct utsname sInfo;` |
|      88 |   91 | `	const char *zFamily = "Unknown";` |
|      88 |   92 | `	if( uname(&sInfo) == 0 ){` |
|      88 |   93 | `		const char *z = sInfo.sysname;` |
|      88 |   94 | `		if( SyStrnicmp(z,"Darwin",sizeof("Darwin")-1) == 0 ){` |
|      44 |   95 | `			zFamily = "Darwin";` |
|      88 |   96 | `		}else if( SyStrnicmp(z,"Linux",sizeof("Linux")-1) == 0 ){` |
|      44 |   97 | `			zFamily = "Linux";` |
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
|      44 |  111 | `	}` |
|      88 |  112 | `	ph7_value_string(pVal,zFamily,-1);` |
|       - |  113 | `#else` |
|       - |  114 | `	ph7_value_string(pVal,"Unknown",(int)sizeof("Unknown")-1);` |
|       - |  115 | `#endif` |
|      92 |  116 | `}` |
|       - |  117 | `/*` |
|       - |  118 | ` * PHP_SAPI` |
|       - |  119 | ` *  The interface between the interpreter and the host. PHL's host binary is a` |
|       - |  120 | ` *  command-line interpreter, so this is "cli" (matching the CLI default of` |
|       - |  121 | ` *  php_sapi_name(); the built-in -S server's per-request "cli-server" flavour is` |
|       - |  122 | ` *  only surfaced by php_sapi_name(), not this compile-time constant).` |
|       - |  123 | ` */` |
|      62 |  124 | `static void PH7_SAPI_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  125 | `{` |
|      31 |  126 | `	SXUNUSED(pUnused);` |
|      65 |  127 | `	ph7_value_string(pVal,"cli",(int)sizeof("cli")-1);` |
|      65 |  128 | `}` |
|       - |  129 | `/*` |
|       - |  130 | ` * PHP_EOL` |
|       - |  131 | ` *  Expand the correct 'End Of Line' symbol for this platform.` |
|       - |  132 | ` */` |
|     898 |  133 | `static void PH7_EOL_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  134 | `{` |
|     449 |  135 | `	SXUNUSED(pUnused);` |
|       - |  136 | `#ifdef __WINNT__` |
|       4 |  137 | `	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);` |
|       - |  138 | `#else` |
|     898 |  139 | `	ph7_value_string(pVal,"\n",(int)sizeof(char));` |
|       - |  140 | `#endif` |
|     902 |  141 | `}` |
|       - |  142 | `/*` |
|       - |  143 | ` * PHP_INT_MAX` |
|       - |  144 | ` * Expand the largest integer supported.` |
|       - |  145 | ` * Note that PH7 deals with 64-bit integer for all platforms.` |
|       - |  146 | ` */` |
|     344 |  147 | `static void PH7_INTMAX_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  148 | `{` |
|     172 |  149 | `	SXUNUSED(pUnused);` |
|     349 |  150 | `	ph7_value_int64(pVal,SXI64_HIGH);` |
|     349 |  151 | `}` |
|       - |  152 | `/* ext/calendar: the only calendar cal_days_in_month() is asked for in practice. */` |
|      66 |  153 | `static void PH7_CAL_GREGORIAN_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  154 | `{` |
|      33 |  155 | `	SXUNUSED(pUnused);` |
|      70 |  156 | `	ph7_value_int(pVal,0);` |
|      70 |  157 | `}` |
|       - |  158 | `/*` |
|       - |  159 | ` * PHP_INT_MIN (php 7.0)` |
|       - |  160 | ` * Expand the smallest integer supported.` |
|       - |  161 | ` */` |
|     178 |  162 | `static void PH7_INTMIN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  163 | `{` |
|      89 |  164 | `	SXUNUSED(pUnused);` |
|     181 |  165 | `	ph7_value_int64(pVal,SMALLEST_INT64);` |
|     181 |  166 | `}` |
|       - |  167 | `/*` |
|       - |  168 | ` * PHP_INT_SIZE` |
|       - |  169 | ` * Expand the size in bytes of a 64-bit integer.` |
|       - |  170 | ` */` |
|      66 |  171 | `static void PH7_INTSIZE_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  172 | `{` |
|      33 |  173 | `	SXUNUSED(pUnused);` |
|      69 |  174 | `	ph7_value_int64(pVal,sizeof(sxi64));` |
|      69 |  175 | `}` |
|       - |  176 | `/*` |
|       - |  177 | ` * PHP_FLOAT_EPSILON / PHP_FLOAT_MAX / PHP_FLOAT_MIN / PHP_FLOAT_DIG (php 7.2)` |
|       - |  178 | ` * Double-precision characteristics, sourced from <float.h> exactly like php` |
|       - |  179 | ` * so they track the compiling platform's actual double representation.` |
|       - |  180 | ` */` |
|      66 |  181 | `static void PH7_FLOATEPSILON_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  182 | `{` |
|      33 |  183 | `	SXUNUSED(pUnused);` |
|      69 |  184 | `	ph7_value_double(pVal,DBL_EPSILON);` |
|      69 |  185 | `}` |
|      64 |  186 | `static void PH7_FLOATMAX_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  187 | `{` |
|      32 |  188 | `	SXUNUSED(pUnused);` |
|      67 |  189 | `	ph7_value_double(pVal,DBL_MAX);` |
|      67 |  190 | `}` |
|      64 |  191 | `static void PH7_FLOATMIN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  192 | `{` |
|      32 |  193 | `	SXUNUSED(pUnused);` |
|      67 |  194 | `	ph7_value_double(pVal,DBL_MIN);` |
|      67 |  195 | `}` |
|      64 |  196 | `static void PH7_FLOATDIG_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  197 | `{` |
|      32 |  198 | `	SXUNUSED(pUnused);` |
|      67 |  199 | `	ph7_value_int64(pVal,DBL_DIG);` |
|      67 |  200 | `}` |
|       - |  201 | `/*` |
|       - |  202 | ` * DIRECTORY_SEPARATOR.` |
|       - |  203 | ` * Expand the directory separator character.` |
|       - |  204 | ` */` |
|     718 |  205 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  206 | `{` |
|     359 |  207 | `	SXUNUSED(pUnused);` |
|       - |  208 | `#ifdef __WINNT__` |
|       5 |  209 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|       - |  210 | `#else` |
|     718 |  211 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|       - |  212 | `#endif` |
|     723 |  213 | `}` |
|       - |  214 | `/*` |
|       - |  215 | ` * PATH_SEPARATOR.` |
|       - |  216 | ` * Expand the path separator character.` |
|       - |  217 | ` */` |
|      70 |  218 | `static void PH7_PATHSEP_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  219 | `{` |
|      35 |  220 | `	SXUNUSED(pUnused);` |
|       - |  221 | `#ifdef __WINNT__` |
|       4 |  222 | `	ph7_value_string(pVal,";",(int)sizeof(char));` |
|       - |  223 | `#else` |
|      70 |  224 | `	ph7_value_string(pVal,":",(int)sizeof(char));` |
|       - |  225 | `#endif` |
|      74 |  226 | `}` |
|       - |  227 |  |
|       - |  228 | `#if defined(PH7_ENABLE_MATH_FUNC)` |
|       - |  229 | `/*` |
|       - |  230 | ` * NAN constant: floating-point Not-A-Number` |
|       - |  231 | ` */` |
|     190 |  232 | `static void PH7_NAN_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  233 | `{` |
|      95 |  234 | `	SXUNUSED(pUnused);` |
|     194 |  235 | `	ph7_value_double(pVal, PH7_NAN_VALUE());` |
|     194 |  236 | `}` |
|       - |  237 |  |
|       - |  238 | `/*` |
|       - |  239 | ` * INF constant: positive infinity` |
|       - |  240 | ` */` |
|     212 |  241 | `static void PH7_INF_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  242 | `{` |
|     106 |  243 | `	SXUNUSED(pUnused);` |
|       - |  244 | `	/* similarly avoid the INFINITY macro */` |
|     216 |  245 | `	ph7_value_double(pVal, PH7_INF_VALUE());` |
|     216 |  246 | `}` |
|       - |  247 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|       - |  248 |  |
|       - |  249 | `#ifndef __WINNT__` |
|       - |  250 | `#include <time.h>` |
|       - |  251 | `#endif` |
|       - |  252 | `/*` |
|       - |  253 | ` * __TIME__` |
|       - |  254 | ` *  Expand the current time (GMT).` |
|       - |  255 | ` */` |
|      64 |  256 | `static void PH7_TIME_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  257 | `{` |
|       - |  258 | `	Sytm sTm;` |
|       - |  259 | `#ifdef __WINNT__` |
|       - |  260 | `	SYSTEMTIME sOS;` |
|       3 |  261 | `	GetSystemTime(&sOS);` |
|       3 |  262 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|       - |  263 | `#else` |
|       - |  264 | `	struct tm *pTm;` |
|       - |  265 | `	time_t t;` |
|      64 |  266 | `	time(&t);` |
|      64 |  267 | `	pTm = gmtime(&t);` |
|      64 |  268 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|       - |  269 | `#endif` |
|      32 |  270 | `	SXUNUSED(pUnused); /* cc warning */` |
|       - |  271 | `	/* Expand */` |
|      67 |  272 | `	ph7_value_string_format(pVal,"%02d:%02d:%02d",sTm.tm_hour,sTm.tm_min,sTm.tm_sec);` |
|      67 |  273 | `}` |
|       - |  274 | `/*` |
|       - |  275 | ` * __DATE__` |
|       - |  276 | ` *  Expand the current date in the ISO-8601 format.` |
|       - |  277 | ` */` |
|      64 |  278 | `static void PH7_DATE_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  279 | `{` |
|       - |  280 | `	Sytm sTm;` |
|       - |  281 | `#ifdef __WINNT__` |
|       - |  282 | `	SYSTEMTIME sOS;` |
|       3 |  283 | `	GetSystemTime(&sOS);` |
|       3 |  284 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|       - |  285 | `#else` |
|       - |  286 | `	struct tm *pTm;` |
|       - |  287 | `	time_t t;` |
|      64 |  288 | `	time(&t);` |
|      64 |  289 | `	pTm = gmtime(&t);` |
|      64 |  290 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|       - |  291 | `#endif` |
|      32 |  292 | `	SXUNUSED(pUnused); /* cc warning */` |
|       - |  293 | `	/* Expand */` |
|      67 |  294 | `	ph7_value_string_format(pVal,"%04d-%02d-%02d",sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|      67 |  295 | `}` |
|       - |  296 | `/*` |
|       - |  297 | ` * __FILE__` |
|       - |  298 | ` *  Path of the processed script.` |
|       - |  299 | ` */` |
|      62 |  300 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  301 | `{` |
|      65 |  302 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - |  303 | `	SyString *pFile;` |
|       - |  304 | `	/* Peek the top entry */` |
|      65 |  305 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      65 |  306 | `	if( pFile == 0 ){` |
|       - |  307 | `		/* Expand the magic word: ":MEMORY:" */` |
|     ! 0 |  308 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|     ! 0 |  309 | `	}else{` |
|      65 |  310 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|       - |  311 | `	}` |
|      65 |  312 | `}` |
|       - |  313 | `/*` |
|       - |  314 | ` * __DIR__` |
|       - |  315 | ` *  Directory holding the processed script.` |
|       - |  316 | ` */` |
|      62 |  317 | `static void PH7_DIR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  318 | `{` |
|      65 |  319 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - |  320 | `	SyString *pFile;` |
|       - |  321 | `	/* Peek the top entry */` |
|      65 |  322 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      65 |  323 | `	if( pFile == 0 ){` |
|       - |  324 | `		/* Expand the magic word: ":MEMORY:" */` |
|     ! 0 |  325 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|     ! 0 |  326 | `	}else{` |
|      65 |  327 | `		if( pFile->nByte > 0 ){` |
|       - |  328 | `			const char *zDir;` |
|       - |  329 | `			int nLen;` |
|      65 |  330 | `			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|      65 |  331 | `			ph7_value_string(pVal,zDir,nLen);` |
|      34 |  332 | `		}else{` |
|       - |  333 | `			/* Expand '.' as the current directory*/` |
|     ! 0 |  334 | `			ph7_value_string(pVal,".",(int)sizeof(char));` |
|       - |  335 | `		}` |
|       - |  336 | `	}` |
|      65 |  337 | `}` |
|       - |  338 | `/*` |
|       - |  339 | ` * PHP_SHLIB_SUFFIX` |
|       - |  340 | ` *  Expand shared library suffix.` |
|       - |  341 | ` */` |
|      64 |  342 | `static void PH7_PHP_SHLIB_SUFFIX_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  343 | `{` |
|       - |  344 | `#ifdef __WINNT__` |
|       3 |  345 | `	ph7_value_string(pVal,"dll",(int)sizeof("dll")-1);` |
|       - |  346 | `#else` |
|      64 |  347 | `	ph7_value_string(pVal,"so",(int)sizeof("so")-1);` |
|       - |  348 | `#endif` |
|      32 |  349 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 |  350 | `}` |
|       - |  351 | `/*` |
|       - |  352 | ` * E_ERROR` |
|       - |  353 | ` *  Expands 1` |
|       - |  354 | ` */` |
|      66 |  355 | `static void PH7_E_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  356 | `{` |
|      69 |  357 | `	ph7_value_int(pVal,1);` |
|      33 |  358 | `	SXUNUSED(pUserData);` |
|      69 |  359 | `}` |
|       - |  360 | `/*` |
|       - |  361 | ` * E_WARNING` |
|       - |  362 | ` *  Expands 2` |
|       - |  363 | ` */` |
|      74 |  364 | `static void PH7_E_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  365 | `{` |
|      78 |  366 | `	ph7_value_int(pVal,2);` |
|      37 |  367 | `	SXUNUSED(pUserData);` |
|      78 |  368 | `}` |
|       - |  369 | `/*` |
|       - |  370 | ` * E_PARSE` |
|       - |  371 | ` *  Expands 4` |
|       - |  372 | ` */` |
|      64 |  373 | `static void PH7_E_PARSE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  374 | `{` |
|      67 |  375 | `	ph7_value_int(pVal,4);` |
|      32 |  376 | `	SXUNUSED(pUserData);` |
|      67 |  377 | `}` |
|       - |  378 | `/*` |
|       - |  379 | ` * E_NOTICE` |
|       - |  380 | ` * Expands 8` |
|       - |  381 | ` */` |
|      68 |  382 | `static void PH7_E_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  383 | `{` |
|      71 |  384 | `	ph7_value_int(pVal,8);` |
|      34 |  385 | `	SXUNUSED(pUserData);` |
|      71 |  386 | `}` |
|       - |  387 | `/*` |
|       - |  388 | ` * E_CORE_ERROR` |
|       - |  389 | ` * Expands 16` |
|       - |  390 | ` */` |
|      64 |  391 | `static void PH7_E_CORE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  392 | `{` |
|      67 |  393 | `	ph7_value_int(pVal,16);` |
|      32 |  394 | `	SXUNUSED(pUserData);` |
|      67 |  395 | `}` |
|       - |  396 | `/*` |
|       - |  397 | ` * E_CORE_WARNING` |
|       - |  398 | ` * Expands 32` |
|       - |  399 | ` */` |
|      64 |  400 | `static void PH7_E_CORE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  401 | `{` |
|      67 |  402 | `	ph7_value_int(pVal,32);` |
|      32 |  403 | `	SXUNUSED(pUserData);` |
|      67 |  404 | `}` |
|       - |  405 | `/*` |
|       - |  406 | ` * E_COMPILE_ERROR` |
|       - |  407 | ` * Expands 64` |
|       - |  408 | ` */` |
|      64 |  409 | `static void PH7_E_COMPILE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  410 | `{` |
|      67 |  411 | `	ph7_value_int(pVal,64);` |
|      32 |  412 | `	SXUNUSED(pUserData);` |
|      67 |  413 | `}` |
|       - |  414 | `/*` |
|       - |  415 | ` * E_COMPILE_WARNING` |
|       - |  416 | ` * Expands 128` |
|       - |  417 | ` */` |
|      64 |  418 | `static void PH7_E_COMPILE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  419 | `{` |
|      67 |  420 | `	ph7_value_int(pVal,128);` |
|      32 |  421 | `	SXUNUSED(pUserData);` |
|      67 |  422 | `}` |
|       - |  423 | `/*` |
|       - |  424 | ` * E_USER_ERROR` |
|       - |  425 | ` * Expands 256` |
|       - |  426 | ` */` |
|      68 |  427 | `static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  428 | `{` |
|      72 |  429 | `	ph7_value_int(pVal,256);` |
|      34 |  430 | `	SXUNUSED(pUserData);` |
|      72 |  431 | `}` |
|       - |  432 | `/*` |
|       - |  433 | ` * E_USER_WARNING` |
|       - |  434 | ` * Expands 512` |
|       - |  435 | ` */` |
|     110 |  436 | `static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  437 | `{` |
|     115 |  438 | `	ph7_value_int(pVal,512);` |
|      55 |  439 | `	SXUNUSED(pUserData);` |
|     115 |  440 | `}` |
|       - |  441 | `/*` |
|       - |  442 | ` * E_USER_NOTICE` |
|       - |  443 | ` * Expands 1024` |
|       - |  444 | ` */` |
|     132 |  445 | `static void PH7_E_USER_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  446 | `{` |
|     137 |  447 | `	ph7_value_int(pVal,1024);` |
|      66 |  448 | `	SXUNUSED(pUserData);` |
|     137 |  449 | `}` |
|       - |  450 | `/*` |
|       - |  451 | ` * E_RECOVERABLE_ERROR` |
|       - |  452 | ` * Expands 4096` |
|       - |  453 | ` */` |
|      64 |  454 | `static void PH7_E_RECOVERABLE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  455 | `{` |
|      67 |  456 | `	ph7_value_int(pVal,4096);` |
|      32 |  457 | `	SXUNUSED(pUserData);` |
|      67 |  458 | `}` |
|       - |  459 | `/*` |
|       - |  460 | ` * E_DEPRECATED` |
|       - |  461 | ` * Expands 8192` |
|       - |  462 | ` */` |
|      72 |  463 | `static void PH7_E_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  464 | `{` |
|      75 |  465 | `	ph7_value_int(pVal,8192);` |
|      36 |  466 | `	SXUNUSED(pUserData);` |
|      75 |  467 | `}` |
|       - |  468 | `/*` |
|       - |  469 | ` * E_USER_DEPRECATED` |
|       - |  470 | ` *   Expands 16384.` |
|       - |  471 | ` */` |
|      72 |  472 | `static void PH7_E_USER_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  473 | `{` |
|      75 |  474 | `	ph7_value_int(pVal,16384);` |
|      36 |  475 | `	SXUNUSED(pUserData);` |
|      75 |  476 | `}` |
|       - |  477 | `/*` |
|       - |  478 | ` * E_ALL` |
|       - |  479 | ` *  Expands 30719 (php 8: E_STRICT is no longer part of E_ALL)` |
|       - |  480 | ` */` |
|     108 |  481 | `static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  482 | `{` |
|     113 |  483 | `	ph7_value_int(pVal,PH7_E_ALL_MASK);` |
|      54 |  484 | `	SXUNUSED(pUserData);` |
|     113 |  485 | `}` |
|       - |  486 | `/*` |
|       - |  487 | ` * CASE_LOWER` |
|       - |  488 | ` *  Expands 0.` |
|       - |  489 | ` */` |
|      64 |  490 | `static void PH7_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  491 | `{` |
|      67 |  492 | `	ph7_value_int(pVal,0);` |
|      32 |  493 | `	SXUNUSED(pUserData);` |
|      67 |  494 | `}` |
|       - |  495 | `/*` |
|       - |  496 | ` * CASE_UPPER` |
|       - |  497 | ` *  Expands 1.` |
|       - |  498 | ` */` |
|      70 |  499 | `static void PH7_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  500 | `{` |
|      73 |  501 | `	ph7_value_int(pVal,1);` |
|      35 |  502 | `	SXUNUSED(pUserData);` |
|      73 |  503 | `}` |
|       - |  504 | `/*` |
|       - |  505 | ` * STR_PAD_LEFT` |
|       - |  506 | ` *  Expands 0.` |
|       - |  507 | ` */` |
|      74 |  508 | `static void PH7_STR_PAD_LEFT_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  509 | `{` |
|      77 |  510 | `	ph7_value_int(pVal,0);` |
|      37 |  511 | `	SXUNUSED(pUserData);` |
|      77 |  512 | `}` |
|       - |  513 | `/*` |
|       - |  514 | ` * STR_PAD_RIGHT` |
|       - |  515 | ` *  Expands 1.` |
|       - |  516 | ` */` |
|      70 |  517 | `static void PH7_STR_PAD_RIGHT_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  518 | `{` |
|      73 |  519 | `	ph7_value_int(pVal,1);` |
|      35 |  520 | `	SXUNUSED(pUserData);` |
|      73 |  521 | `}` |
|       - |  522 | `/*` |
|       - |  523 | ` * STR_PAD_BOTH` |
|       - |  524 | ` *  Expands 2.` |
|       - |  525 | ` */` |
|      68 |  526 | `static void PH7_STR_PAD_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  527 | `{` |
|      71 |  528 | `	ph7_value_int(pVal,2);` |
|      34 |  529 | `	SXUNUSED(pUserData);` |
|      71 |  530 | `}` |
|       - |  531 | `/*` |
|       - |  532 | ` * stream_wrapper_register()'s $flags. php defines exactly this one bit: the` |
|       - |  533 | ` * wrapper speaks to the network, so allow_url_fopen gates opening it and` |
|       - |  534 | ` * allow_url_include gates INCLUDING it.` |
|       - |  535 | ` */` |
|      68 |  536 | `static void PH7_STREAM_IS_URL_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  537 | `{` |
|      72 |  538 | `	ph7_value_int(pVal,PH7_STREAM_IS_URL);` |
|      34 |  539 | `	SXUNUSED(pUserData);` |
|      72 |  540 | `}` |
|       - |  541 | `/*` |
|       - |  542 | ` * A userland filter's ANSWER, and which kind of call it is answering. FEED_ME` |
|       - |  543 | ` * says "I produced nothing, ask me again with more"; ERR_FATAL ends the stream.` |
|       - |  544 | ` */` |
|      96 |  545 | `static void PH7_PSFS_PASS_ON_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  546 | `{` |
|      99 |  547 | `	ph7_value_int(pVal,PHL_PSFS_PASS_ON);` |
|      48 |  548 | `	SXUNUSED(pUserData);` |
|      99 |  549 | `}` |
|      68 |  550 | `static void PH7_PSFS_FEED_ME_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  551 | `{` |
|      71 |  552 | `	ph7_value_int(pVal,PHL_PSFS_FEED_ME);` |
|      34 |  553 | `	SXUNUSED(pUserData);` |
|      71 |  554 | `}` |
|      66 |  555 | `static void PH7_PSFS_ERR_FATAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  556 | `{` |
|      69 |  557 | `	ph7_value_int(pVal,PHL_PSFS_ERR_FATAL);` |
|      33 |  558 | `	SXUNUSED(pUserData);` |
|      69 |  559 | `}` |
|      64 |  560 | `static void PH7_PSFS_FLAG_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  561 | `{` |
|      67 |  562 | `	ph7_value_int(pVal,PHL_PSFS_FLAG_NORMAL);` |
|      32 |  563 | `	SXUNUSED(pUserData);` |
|      67 |  564 | `}` |
|      64 |  565 | `static void PH7_PSFS_FLAG_FLUSH_INC_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  566 | `{` |
|      67 |  567 | `	ph7_value_int(pVal,PHL_PSFS_FLAG_FLUSH_INC);` |
|      32 |  568 | `	SXUNUSED(pUserData);` |
|      67 |  569 | `}` |
|      64 |  570 | `static void PH7_PSFS_FLAG_FLUSH_CLOSE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  571 | `{` |
|      67 |  572 | `	ph7_value_int(pVal,PHL_PSFS_FLAG_FLUSH_CLOSE);` |
|      32 |  573 | `	SXUNUSED(pUserData);` |
|      67 |  574 | `}` |
|       - |  575 | `/*` |
|       - |  576 | ` * stream_filter_append()'s $mode — WHICH chain the filter joins. php's 0 is not` |
|       - |  577 | ` * "neither": it means "whichever chains the handle's own mode makes sense for".` |
|       - |  578 | ` */` |
|     190 |  579 | `static void PH7_STREAM_FILTER_READ_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  580 | `{` |
|     193 |  581 | `	ph7_value_int(pVal,PHL_STREAM_FILTER_READ);` |
|      95 |  582 | `	SXUNUSED(pUserData);` |
|     193 |  583 | `}` |
|      88 |  584 | `static void PH7_STREAM_FILTER_WRITE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  585 | `{` |
|      92 |  586 | `	ph7_value_int(pVal,PHL_STREAM_FILTER_WRITE);` |
|      44 |  587 | `	SXUNUSED(pUserData);` |
|      92 |  588 | `}` |
|      64 |  589 | `static void PH7_STREAM_FILTER_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  590 | `{` |
|      67 |  591 | `	ph7_value_int(pVal,PHL_STREAM_FILTER_ALL);` |
|      32 |  592 | `	SXUNUSED(pUserData);` |
|      67 |  593 | `}` |
|       - |  594 | `/*` |
|       - |  595 | ` * stream_socket_client()'s $flags. CONNECT is the default it documents;` |
|       - |  596 | ` * PERSISTENT is what pfsockopen() means and the only one that changes what a` |
|       - |  597 | ` * second call to the same address ANSWERS.` |
|       - |  598 | ` */` |
|      86 |  599 | `static void PH7_STREAM_CLIENT_CONNECT_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  600 | `{` |
|      90 |  601 | `	ph7_value_int(pVal,PH7_STREAM_CLIENT_CONNECT);` |
|      43 |  602 | `	SXUNUSED(pUserData);` |
|      90 |  603 | `}` |
|      64 |  604 | `static void PH7_STREAM_CLIENT_ASYNC_CONNECT_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  605 | `{` |
|      67 |  606 | `	ph7_value_int(pVal,PH7_STREAM_CLIENT_ASYNC_CONNECT);` |
|      32 |  607 | `	SXUNUSED(pUserData);` |
|      67 |  608 | `}` |
|      70 |  609 | `static void PH7_STREAM_CLIENT_PERSISTENT_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  610 | `{` |
|      73 |  611 | `	ph7_value_int(pVal,PH7_STREAM_CLIENT_PERSISTENT);` |
|      35 |  612 | `	SXUNUSED(pUserData);` |
|      73 |  613 | `}` |
|       - |  614 | `/*` |
|       - |  615 | ` * The socket-family constants. Their VALUES are the platform's own — AF_INET6 is` |
|       - |  616 | ` * 10 on Linux, 23 on Windows and 30 on the BSDs — so they are asked for by id` |
|       - |  617 | ` * rather than written down here, and a program handing one to` |
|       - |  618 | ` * stream_socket_pair() is handing the OS its own number.` |
|       - |  619 | ` */` |
|       - |  620 | `#ifdef PH7_ENABLE_NET` |
|      62 |  621 | `static void PH7_STREAM_PF_INET_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  622 | `{` |
|      66 |  623 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_INET));` |
|      31 |  624 | `	SXUNUSED(pUserData);` |
|      66 |  625 | `}` |
|      62 |  626 | `static void PH7_STREAM_PF_INET6_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  627 | `{` |
|      65 |  628 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_INET6));` |
|      31 |  629 | `	SXUNUSED(pUserData);` |
|      65 |  630 | `}` |
|      66 |  631 | `static void PH7_STREAM_PF_UNIX_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  632 | `{` |
|      70 |  633 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_UNIX));` |
|      33 |  634 | `	SXUNUSED(pUserData);` |
|      70 |  635 | `}` |
|      64 |  636 | `static void PH7_STREAM_SOCK_STREAM_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  637 | `{` |
|      68 |  638 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_STREAM));` |
|      32 |  639 | `	SXUNUSED(pUserData);` |
|      68 |  640 | `}` |
|      62 |  641 | `static void PH7_STREAM_SOCK_DGRAM_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  642 | `{` |
|      65 |  643 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_DGRAM));` |
|      31 |  644 | `	SXUNUSED(pUserData);` |
|      65 |  645 | `}` |
|      62 |  646 | `static void PH7_STREAM_SOCK_RAW_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  647 | `{` |
|      65 |  648 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_RAW));` |
|      31 |  649 | `	SXUNUSED(pUserData);` |
|      65 |  650 | `}` |
|      62 |  651 | `static void PH7_STREAM_SOCK_SEQPACKET_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  652 | `{` |
|      65 |  653 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_SEQPACKET));` |
|      31 |  654 | `	SXUNUSED(pUserData);` |
|      65 |  655 | `}` |
|      62 |  656 | `static void PH7_STREAM_SOCK_RDM_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  657 | `{` |
|      65 |  658 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_RDM));` |
|      31 |  659 | `	SXUNUSED(pUserData);` |
|      65 |  660 | `}` |
|      62 |  661 | `static void PH7_STREAM_IPPROTO_IP_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  662 | `{` |
|      66 |  663 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_IP));` |
|      31 |  664 | `	SXUNUSED(pUserData);` |
|      66 |  665 | `}` |
|      62 |  666 | `static void PH7_STREAM_IPPROTO_TCP_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  667 | `{` |
|      65 |  668 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_TCP));` |
|      31 |  669 | `	SXUNUSED(pUserData);` |
|      65 |  670 | `}` |
|      62 |  671 | `static void PH7_STREAM_IPPROTO_UDP_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  672 | `{` |
|      65 |  673 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_UDP));` |
|      31 |  674 | `	SXUNUSED(pUserData);` |
|      65 |  675 | `}` |
|      62 |  676 | `static void PH7_STREAM_IPPROTO_ICMP_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  677 | `{` |
|      65 |  678 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_ICMP));` |
|      31 |  679 | `	SXUNUSED(pUserData);` |
|      65 |  680 | `}` |
|      62 |  681 | `static void PH7_STREAM_IPPROTO_RAW_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  682 | `{` |
|      65 |  683 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_RAW));` |
|      31 |  684 | `	SXUNUSED(pUserData);` |
|      65 |  685 | `}` |
|       - |  686 | `#endif /* PH7_ENABLE_NET */` |
|       - |  687 | `/*` |
|       - |  688 | ` * stream_socket_shutdown()'s $mode, and the two recvfrom/sendto flags. These` |
|       - |  689 | ` * three ARE php's own numbers rather than the OS's: php maps STREAM_OOB and` |
|       - |  690 | ` * STREAM_PEEK onto MSG_OOB/MSG_PEEK itself.` |
|       - |  691 | ` */` |
|      66 |  692 | `static void PH7_STREAM_SHUT_RD_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  693 | `{` |
|      70 |  694 | `	ph7_value_int(pVal,PH7_STREAM_SHUT_RD);` |
|      33 |  695 | `	SXUNUSED(pUserData);` |
|      70 |  696 | `}` |
|      64 |  697 | `static void PH7_STREAM_SHUT_WR_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  698 | `{` |
|      68 |  699 | `	ph7_value_int(pVal,PH7_STREAM_SHUT_WR);` |
|      32 |  700 | `	SXUNUSED(pUserData);` |
|      68 |  701 | `}` |
|      62 |  702 | `static void PH7_STREAM_SHUT_RDWR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  703 | `{` |
|      65 |  704 | `	ph7_value_int(pVal,PH7_STREAM_SHUT_RDWR);` |
|      31 |  705 | `	SXUNUSED(pUserData);` |
|      65 |  706 | `}` |
|      62 |  707 | `static void PH7_STREAM_OOB_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  708 | `{` |
|      65 |  709 | `	ph7_value_int(pVal,PH7_STREAM_OOB);` |
|      31 |  710 | `	SXUNUSED(pUserData);` |
|      65 |  711 | `}` |
|      64 |  712 | `static void PH7_STREAM_PEEK_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  713 | `{` |
|      68 |  714 | `	ph7_value_int(pVal,PH7_STREAM_PEEK);` |
|      32 |  715 | `	SXUNUSED(pUserData);` |
|      68 |  716 | `}` |
|       - |  717 | `/*` |
|       - |  718 | ` * stream_socket_server()'s $flags. Its default is BIND\|LISTEN, and the two are` |
|       - |  719 | ` * separate because binding is all a datagram server does.` |
|       - |  720 | ` */` |
|      70 |  721 | `static void PH7_STREAM_SERVER_BIND_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  722 | `{` |
|      74 |  723 | `	ph7_value_int(pVal,PH7_STREAM_SERVER_BIND);` |
|      35 |  724 | `	SXUNUSED(pUserData);` |
|      74 |  725 | `}` |
|      68 |  726 | `static void PH7_STREAM_SERVER_LISTEN_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  727 | `{` |
|      72 |  728 | `	ph7_value_int(pVal,PH7_STREAM_SERVER_LISTEN);` |
|      34 |  729 | `	SXUNUSED(pUserData);` |
|      72 |  730 | `}` |
|       - |  731 | `/*` |
|       - |  732 | ` * mt_srand()'s $mode: which GENERATOR to seed. MT_RAND_PHP is php's pre-7.1` |
|       - |  733 | ` * Mersenne Twister, whose twist reads the low bit of the wrong word — a` |
|       - |  734 | ` * different sequence, which is the only reason to ask for it.` |
|       - |  735 | ` */` |
|      70 |  736 | `static void PH7_MT_RAND_MT19937_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  737 | `{` |
|      73 |  738 | `	ph7_value_int(pVal,PH7_MT_RAND_MT19937);` |
|      35 |  739 | `	SXUNUSED(pUserData);` |
|      73 |  740 | `}` |
|      76 |  741 | `static void PH7_MT_RAND_PHP_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  742 | `{` |
|      79 |  743 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - |  744 | `	/* php 8.3 deprecated the SYMBOL as well as the mode, and says why — when a` |
|       - |  745 | `	 * program NAMES it. Listing the constant table is not naming it. */` |
|      79 |  746 | `	if( pVm && !pVm->bConstEnum ){` |
|      15 |  747 | `		PH7_VmThrowError(pVm,0,8192 /* E_DEPRECATED */,` |
|       - |  748 | `			"Constant MT_RAND_PHP is deprecated since 8.3, as it uses a biased non-standard variant of Mt19937");` |
|       7 |  749 | `	}` |
|      79 |  750 | `	ph7_value_int(pVal,PH7_MT_RAND_PHP);` |
|      79 |  751 | `}` |
|       - |  752 | `/*` |
|       - |  753 | ` * Output-handler flags and phases (ob_start()'s $flags, and the $phase an output` |
|       - |  754 | ` * handler is called with). The values are php's and are a public ABI: the phase` |
|       - |  755 | ` * bits are OR'd together (a first FLUSH arrives as FLUSH\|START = 5), and the` |
|       - |  756 | ` * three capability flags are the ones ob_clean()/ob_flush()/ob_end_*() test` |
|       - |  757 | ` * before they will touch the buffer.` |
|       - |  758 | ` */` |
|     128 |  759 | `static void PH7_OB_WRITE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  760 | `{` |
|     132 |  761 | `	ph7_value_int(pVal,PH7_OB_WRITE);` |
|      64 |  762 | `	SXUNUSED(pUserData);` |
|     132 |  763 | `}` |
|      64 |  764 | `static void PH7_OB_START_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  765 | `{` |
|      68 |  766 | `	ph7_value_int(pVal,PH7_OB_START);` |
|      32 |  767 | `	SXUNUSED(pUserData);` |
|      68 |  768 | `}` |
|      64 |  769 | `static void PH7_OB_CLEAN_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  770 | `{` |
|      68 |  771 | `	ph7_value_int(pVal,PH7_OB_CLEAN);` |
|      32 |  772 | `	SXUNUSED(pUserData);` |
|      68 |  773 | `}` |
|      64 |  774 | `static void PH7_OB_FLUSH_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  775 | `{` |
|      68 |  776 | `	ph7_value_int(pVal,PH7_OB_FLUSH);` |
|      32 |  777 | `	SXUNUSED(pUserData);` |
|      68 |  778 | `}` |
|     128 |  779 | `static void PH7_OB_FINAL_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  780 | `{` |
|     132 |  781 | `	ph7_value_int(pVal,PH7_OB_FINAL);` |
|      64 |  782 | `	SXUNUSED(pUserData);` |
|     132 |  783 | `}` |
|      68 |  784 | `static void PH7_OB_CLEANABLE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  785 | `{` |
|      72 |  786 | `	ph7_value_int(pVal,PH7_OB_CLEANABLE);` |
|      34 |  787 | `	SXUNUSED(pUserData);` |
|      72 |  788 | `}` |
|      68 |  789 | `static void PH7_OB_FLUSHABLE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  790 | `{` |
|      72 |  791 | `	ph7_value_int(pVal,PH7_OB_FLUSHABLE);` |
|      34 |  792 | `	SXUNUSED(pUserData);` |
|      72 |  793 | `}` |
|      70 |  794 | `static void PH7_OB_REMOVABLE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  795 | `{` |
|      74 |  796 | `	ph7_value_int(pVal,PH7_OB_REMOVABLE);` |
|      35 |  797 | `	SXUNUSED(pUserData);` |
|      74 |  798 | `}` |
|      78 |  799 | `static void PH7_OB_STDFLAGS_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  800 | `{` |
|      82 |  801 | `	ph7_value_int(pVal,PH7_OB_STDFLAGS);` |
|      39 |  802 | `	SXUNUSED(pUserData);` |
|      82 |  803 | `}` |
|      66 |  804 | `static void PH7_OB_STARTED_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  805 | `{` |
|      70 |  806 | `	ph7_value_int(pVal,PH7_OB_STARTED);` |
|      33 |  807 | `	SXUNUSED(pUserData);` |
|      70 |  808 | `}` |
|      66 |  809 | `static void PH7_OB_DISABLED_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  810 | `{` |
|      70 |  811 | `	ph7_value_int(pVal,PH7_OB_DISABLED);` |
|      33 |  812 | `	SXUNUSED(pUserData);` |
|      70 |  813 | `}` |
|      68 |  814 | `static void PH7_OB_PROCESSED_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  815 | `{` |
|      72 |  816 | `	ph7_value_int(pVal,PH7_OB_PROCESSED);` |
|      34 |  817 | `	SXUNUSED(pUserData);` |
|      72 |  818 | `}` |
|       - |  819 | `/*` |
|       - |  820 | ` * array_filter()'s $mode selector. The VALUES are php's and are a public ABI --` |
|       - |  821 | ` * ARRAY_FILTER_USE_BOTH is 1 and ARRAY_FILTER_USE_KEY is 2, NOT the other way` |
|       - |  822 | ` * round, and they are a selector rather than a bit mask (php reads the argument` |
|       - |  823 | ` * with ==, so any other number is the default value mode).` |
|       - |  824 | ` */` |
|      80 |  825 | `static void PH7_ARRAY_FILTER_USE_KEY_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  826 | `{` |
|      83 |  827 | `	ph7_value_int(pVal,2);` |
|      40 |  828 | `	SXUNUSED(pUserData);` |
|      83 |  829 | `}` |
|      70 |  830 | `static void PH7_ARRAY_FILTER_USE_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  831 | `{` |
|      73 |  832 | `	ph7_value_int(pVal,1);` |
|      35 |  833 | `	SXUNUSED(pUserData);` |
|      73 |  834 | `}` |
|       - |  835 | `/*` |
|       - |  836 | ` * COUNT_NORMAL` |
|       - |  837 | ` *  Expands 0` |
|       - |  838 | ` */` |
|      70 |  839 | `static void PH7_COUNT_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  840 | `{` |
|      74 |  841 | `	ph7_value_int(pVal,0);` |
|      35 |  842 | `	SXUNUSED(pUserData);` |
|      74 |  843 | `}` |
|       - |  844 | `/*` |
|       - |  845 | ` * COUNT_RECURSIVE` |
|       - |  846 | ` *  Expands 1.` |
|       - |  847 | ` */` |
|      82 |  848 | `static void PH7_COUNT_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  849 | `{` |
|      86 |  850 | `	ph7_value_int(pVal,1);` |
|      41 |  851 | `	SXUNUSED(pUserData);` |
|      86 |  852 | `}` |
|       - |  853 | `/*` |
|       - |  854 | ` * php's sort-flag constants. The VALUES must match php exactly: they are a` |
|       - |  855 | ` * public ABI (code passes literal ints, dumps them, and OR-combines the base` |
|       - |  856 | ` * type with SORT_FLAG_CASE). SORT_ASC/SORT_DESC are the array_multisort` |
|       - |  857 | ` * direction flags.` |
|       - |  858 | ` * SORT_REGULAR 0 · SORT_NUMERIC 1 · SORT_STRING 2 · SORT_DESC 3 · SORT_ASC 4 ·` |
|       - |  859 | ` * SORT_LOCALE_STRING 5 · SORT_NATURAL 6 · SORT_FLAG_CASE 8` |
|       - |  860 | ` */` |
|      78 |  861 | `static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  862 | `{` |
|      82 |  863 | `	ph7_value_int(pVal,4);` |
|      39 |  864 | `	SXUNUSED(pUserData);` |
|      82 |  865 | `}` |
|      74 |  866 | `static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  867 | `{` |
|      77 |  868 | `	ph7_value_int(pVal,3);` |
|      37 |  869 | `	SXUNUSED(pUserData);` |
|      77 |  870 | `}` |
|      74 |  871 | `static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  872 | `{` |
|      77 |  873 | `	ph7_value_int(pVal,0);` |
|      37 |  874 | `	SXUNUSED(pUserData);` |
|      77 |  875 | `}` |
|     132 |  876 | `static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  877 | `{` |
|     136 |  878 | `	ph7_value_int(pVal,1);` |
|      66 |  879 | `	SXUNUSED(pUserData);` |
|     136 |  880 | `}` |
|     170 |  881 | `static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  882 | `{` |
|     175 |  883 | `	ph7_value_int(pVal,2);` |
|      85 |  884 | `	SXUNUSED(pUserData);` |
|     175 |  885 | `}` |
|      66 |  886 | `static void PH7_SORT_LOCALE_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  887 | `{` |
|      69 |  888 | `	ph7_value_int(pVal,5);` |
|      33 |  889 | `	SXUNUSED(pUserData);` |
|      69 |  890 | `}` |
|      82 |  891 | `static void PH7_SORT_NATURAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  892 | `{` |
|      85 |  893 | `	ph7_value_int(pVal,6);` |
|      41 |  894 | `	SXUNUSED(pUserData);` |
|      85 |  895 | `}` |
|      86 |  896 | `static void PH7_SORT_FLAG_CASE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  897 | `{` |
|      90 |  898 | `	ph7_value_int(pVal,8);` |
|      43 |  899 | `	SXUNUSED(pUserData);` |
|      90 |  900 | `}` |
|       - |  901 | `/*` |
|       - |  902 | ` * PHP_ROUND_HALF_UP` |
|       - |  903 | ` *  Expands 1.` |
|       - |  904 | ` */` |
|      66 |  905 | `static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  906 | `{` |
|      69 |  907 | `	ph7_value_int(pVal,1);` |
|      33 |  908 | `	SXUNUSED(pUserData);` |
|      69 |  909 | `}` |
|       - |  910 | `/*` |
|       - |  911 | ` * PHP_SESSION_DISABLED / PHP_SESSION_NONE / PHP_SESSION_ACTIVE` |
|       - |  912 | ` *  session_status() states (0 / 1 / 2).` |
|       - |  913 | ` */` |
|      64 |  914 | `static void PH7_PHP_SESSION_DISABLED_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  915 | `{` |
|      68 |  916 | `	ph7_value_int(pVal,0);` |
|      32 |  917 | `	SXUNUSED(pUserData);` |
|      68 |  918 | `}` |
|      64 |  919 | `static void PH7_PHP_SESSION_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  920 | `{` |
|      68 |  921 | `	ph7_value_int(pVal,1);` |
|      32 |  922 | `	SXUNUSED(pUserData);` |
|      68 |  923 | `}` |
|      78 |  924 | `static void PH7_PHP_SESSION_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  925 | `{` |
|      82 |  926 | `	ph7_value_int(pVal,2);` |
|      39 |  927 | `	SXUNUSED(pUserData);` |
|      82 |  928 | `}` |
|       - |  929 | `/*` |
|       - |  930 | ` * INI_USER / INI_PERDIR / INI_SYSTEM / INI_ALL` |
|       - |  931 | ` *  php.ini access levels (1 / 2 / 4 / 7).` |
|       - |  932 | ` */` |
|      64 |  933 | `static void PH7_INI_USER_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  934 | `{` |
|      68 |  935 | `	ph7_value_int(pVal,1);` |
|      32 |  936 | `	SXUNUSED(pUserData);` |
|      68 |  937 | `}` |
|      64 |  938 | `static void PH7_INI_PERDIR_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  939 | `{` |
|      68 |  940 | `	ph7_value_int(pVal,2);` |
|      32 |  941 | `	SXUNUSED(pUserData);` |
|      68 |  942 | `}` |
|      64 |  943 | `static void PH7_INI_SYSTEM_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  944 | `{` |
|      68 |  945 | `	ph7_value_int(pVal,4);` |
|      32 |  946 | `	SXUNUSED(pUserData);` |
|      68 |  947 | `}` |
|      64 |  948 | `static void PH7_INI_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  949 | `{` |
|      68 |  950 | `	ph7_value_int(pVal,7);` |
|      32 |  951 | `	SXUNUSED(pUserData);` |
|      68 |  952 | `}` |
|       - |  953 | `/*` |
|       - |  954 | ` * MB_CASE_UPPER / MB_CASE_LOWER / MB_CASE_TITLE (0 / 1 / 2)` |
|       - |  955 | ` */` |
|      66 |  956 | `static void PH7_MB_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  957 | `{` |
|      69 |  958 | `	ph7_value_int(pVal,0);` |
|      33 |  959 | `	SXUNUSED(pUserData);` |
|      69 |  960 | `}` |
|      66 |  961 | `static void PH7_MB_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  962 | `{` |
|      69 |  963 | `	ph7_value_int(pVal,1);` |
|      33 |  964 | `	SXUNUSED(pUserData);` |
|      69 |  965 | `}` |
|     102 |  966 | `static void PH7_MB_CASE_TITLE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  967 | `{` |
|     105 |  968 | `	ph7_value_int(pVal,2);` |
|      51 |  969 | `	SXUNUSED(pUserData);` |
|     105 |  970 | `}` |
|       - |  971 | `/*` |
|       - |  972 | ` * SPHP_ROUND_HALF_DOWN` |
|       - |  973 | ` *  Expands 2.` |
|       - |  974 | ` */` |
|      66 |  975 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  976 | `{` |
|      69 |  977 | `	ph7_value_int(pVal,2);` |
|      33 |  978 | `	SXUNUSED(pUserData);` |
|      69 |  979 | `}` |
|       - |  980 | `/*` |
|       - |  981 | ` * PHP_ROUND_HALF_EVEN` |
|       - |  982 | ` *  Expands 3.` |
|       - |  983 | ` */` |
|      70 |  984 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  985 | `{` |
|      73 |  986 | `	ph7_value_int(pVal,3);` |
|      35 |  987 | `	SXUNUSED(pUserData);` |
|      73 |  988 | `}` |
|       - |  989 | `/*` |
|       - |  990 | ` * PHP_ROUND_HALF_ODD` |
|       - |  991 | ` *  Expands 4.` |
|       - |  992 | ` */` |
|      66 |  993 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  994 | `{` |
|      69 |  995 | `	ph7_value_int(pVal,4);` |
|      33 |  996 | `	SXUNUSED(pUserData);` |
|      69 |  997 | `}` |
|       - |  998 | `/*` |
|       - |  999 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|       - | 1000 | ` *  Expand 0x01` |
|       - | 1001 | ` * NOTE:` |
|       - | 1002 | ` *  The expanded value must be a power of two.` |
|       - | 1003 | ` */` |
|      68 | 1004 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1005 | `{` |
|      71 | 1006 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      34 | 1007 | `	SXUNUSED(pUserData);` |
|      71 | 1008 | `}` |
|       - | 1009 | `/*` |
|       - | 1010 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|       - | 1011 | ` *  Expand 0x02` |
|       - | 1012 | ` * NOTE:` |
|       - | 1013 | ` *  The expanded value must be a power of two.` |
|       - | 1014 | ` */` |
|      72 | 1015 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1016 | `{` |
|      75 | 1017 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      36 | 1018 | `	SXUNUSED(pUserData);` |
|      75 | 1019 | `}` |
|       - | 1020 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|       - | 1021 | `/*` |
|       - | 1022 | ` * M_PI` |
|       - | 1023 | ` *  Expand the value of pi.` |
|       - | 1024 | ` */` |
|      72 | 1025 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1026 | `{` |
|      36 | 1027 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 1028 | `	ph7_value_double(pVal,PH7_PI);` |
|      75 | 1029 | `}` |
|       - | 1030 | `/*` |
|       - | 1031 | ` * M_E` |
|       - | 1032 | ` *  Expand 2.7182818284590452354` |
|       - | 1033 | ` */` |
|      64 | 1034 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1035 | `{` |
|      32 | 1036 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1037 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|      67 | 1038 | `}` |
|       - | 1039 | `/*` |
|       - | 1040 | ` * M_LOG2E` |
|       - | 1041 | ` *  Expand 2.7182818284590452354` |
|       - | 1042 | ` */` |
|      64 | 1043 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1044 | `{` |
|      32 | 1045 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1046 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      67 | 1047 | `}` |
|       - | 1048 | `/*` |
|       - | 1049 | ` * M_LOG10E` |
|       - | 1050 | ` *  Expand 0.4342944819032518276` |
|       - | 1051 | ` */` |
|      64 | 1052 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1053 | `{` |
|      32 | 1054 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1055 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      67 | 1056 | `}` |
|       - | 1057 | `/*` |
|       - | 1058 | ` * M_LN2` |
|       - | 1059 | ` *  Expand 	0.69314718055994530942` |
|       - | 1060 | ` */` |
|      64 | 1061 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1062 | `{` |
|      32 | 1063 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1064 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      67 | 1065 | `}` |
|       - | 1066 | `/*` |
|       - | 1067 | ` * M_LN10` |
|       - | 1068 | ` *  Expand 	2.30258509299404568402` |
|       - | 1069 | ` */` |
|      64 | 1070 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1071 | `{` |
|      32 | 1072 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1073 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      67 | 1074 | `}` |
|       - | 1075 | `/*` |
|       - | 1076 | ` * M_PI_2` |
|       - | 1077 | ` *  Expand 	1.57079632679489661923` |
|       - | 1078 | ` */` |
|      64 | 1079 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1080 | `{` |
|      32 | 1081 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1082 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      67 | 1083 | `}` |
|       - | 1084 | `/*` |
|       - | 1085 | ` * M_PI_4` |
|       - | 1086 | ` *  Expand 	0.78539816339744830962` |
|       - | 1087 | ` */` |
|      64 | 1088 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1089 | `{` |
|      32 | 1090 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1091 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      67 | 1092 | `}` |
|       - | 1093 | `/*` |
|       - | 1094 | ` * M_1_PI` |
|       - | 1095 | ` *  Expand 	0.31830988618379067154` |
|       - | 1096 | ` */` |
|      64 | 1097 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1098 | `{` |
|      32 | 1099 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1100 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      67 | 1101 | `}` |
|       - | 1102 | `/*` |
|       - | 1103 | ` * M_2_PI` |
|       - | 1104 | ` *  Expand 0.63661977236758134308` |
|       - | 1105 | ` */` |
|      66 | 1106 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1107 | `{` |
|      33 | 1108 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1109 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      69 | 1110 | `}` |
|       - | 1111 | `/*` |
|       - | 1112 | ` * M_SQRTPI` |
|       - | 1113 | ` *  Expand 1.77245385090551602729` |
|       - | 1114 | ` */` |
|      64 | 1115 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1116 | `{` |
|      32 | 1117 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1118 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      67 | 1119 | `}` |
|       - | 1120 | `/*` |
|       - | 1121 | ` * M_2_SQRTPI` |
|       - | 1122 | ` *  Expand 	1.12837916709551257390` |
|       - | 1123 | ` */` |
|      64 | 1124 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1125 | `{` |
|      32 | 1126 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1127 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      67 | 1128 | `}` |
|       - | 1129 | `/*` |
|       - | 1130 | ` * M_SQRT2` |
|       - | 1131 | ` *  Expand 	1.41421356237309504880` |
|       - | 1132 | ` */` |
|      64 | 1133 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1134 | `{` |
|      32 | 1135 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1136 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      67 | 1137 | `}` |
|       - | 1138 | `/*` |
|       - | 1139 | ` * M_SQRT3` |
|       - | 1140 | ` *  Expand 	1.73205080756887729352` |
|       - | 1141 | ` */` |
|      64 | 1142 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1143 | `{` |
|      32 | 1144 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1145 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      67 | 1146 | `}` |
|       - | 1147 | `/*` |
|       - | 1148 | ` * M_SQRT1_2` |
|       - | 1149 | ` *  Expand 	0.70710678118654752440` |
|       - | 1150 | ` */` |
|      64 | 1151 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1152 | `{` |
|      32 | 1153 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1154 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      67 | 1155 | `}` |
|       - | 1156 | `/*` |
|       - | 1157 | ` * M_LNPI` |
|       - | 1158 | ` *  Expand 	1.14472988584940017414` |
|       - | 1159 | ` */` |
|      64 | 1160 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1161 | `{` |
|      32 | 1162 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1163 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      67 | 1164 | `}` |
|       - | 1165 | `/*` |
|       - | 1166 | ` * M_EULER` |
|       - | 1167 | ` *  Expand  0.57721566490153286061` |
|       - | 1168 | ` */` |
|      64 | 1169 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1170 | `{` |
|      32 | 1171 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1172 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      67 | 1173 | `}` |
|       - | 1174 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|       - | 1175 | `/*` |
|       - | 1176 | ` * DATE_ATOM` |
|       - | 1177 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|       - | 1178 | ` */` |
|     128 | 1179 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1180 | `{` |
|      64 | 1181 | `	SXUNUSED(pUserData); /* cc warning */` |
|     131 | 1182 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|     131 | 1183 | `}` |
|       - | 1184 | `/*` |
|       - | 1185 | ` * DATE_COOKIE` |
|       - | 1186 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|       - | 1187 | ` */` |
|      64 | 1188 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1189 | `{` |
|      32 | 1190 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1191 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      67 | 1192 | `}` |
|       - | 1193 | `/*` |
|       - | 1194 | ` * DATE_ISO8601` |
|       - | 1195 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|       - | 1196 | ` */` |
|      64 | 1197 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1198 | `{` |
|      32 | 1199 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1200 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      67 | 1201 | `}` |
|       - | 1202 | `/*` |
|       - | 1203 | ` * DATE_RFC822` |
|       - | 1204 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|       - | 1205 | ` */` |
|      64 | 1206 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1207 | `{` |
|      32 | 1208 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1209 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      67 | 1210 | `}` |
|       - | 1211 | `/*` |
|       - | 1212 | ` * DATE_RFC850` |
|       - | 1213 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|       - | 1214 | ` */` |
|      64 | 1215 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1216 | `{` |
|      32 | 1217 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1218 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      67 | 1219 | `}` |
|       - | 1220 | `/*` |
|       - | 1221 | ` * DATE_RFC1036` |
|       - | 1222 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1223 | ` */` |
|      64 | 1224 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1225 | `{` |
|      32 | 1226 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1227 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      67 | 1228 | `}` |
|       - | 1229 | `/*` |
|       - | 1230 | ` * DATE_RFC1123` |
|       - | 1231 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1232 | ` */` |
|      64 | 1233 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1234 | `{` |
|      32 | 1235 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1236 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      67 | 1237 | `}` |
|       - | 1238 | `/*` |
|       - | 1239 | ` * DATE_RFC2822` |
|       - | 1240 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1241 | ` */` |
|      64 | 1242 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1243 | `{` |
|      32 | 1244 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1245 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      67 | 1246 | `}` |
|       - | 1247 | `/*` |
|       - | 1248 | ` * DATE_RSS` |
|       - | 1249 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1250 | ` */` |
|      64 | 1251 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1252 | `{` |
|      32 | 1253 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1254 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      67 | 1255 | `}` |
|       - | 1256 | `/*` |
|       - | 1257 | ` * DATE_W3C` |
|       - | 1258 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|       - | 1259 | ` */` |
|      64 | 1260 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1261 | `{` |
|      32 | 1262 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1263 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      67 | 1264 | `}` |
|       - | 1265 | `/*` |
|       - | 1266 | ` * The three format constants php added after the original set. Each is a plain` |
|       - | 1267 | ` * format STRING, so the whole of its behaviour is what date()/DateTime::format()` |
|       - | 1268 | ` * already do with those characters -- but each was a loud undefined-constant` |
|       - | 1269 | ` * fatal, which is a program that does not run rather than one that runs wrong.` |
|       - | 1270 | ` *` |
|       - | 1271 | ` * DATE_RFC7231 is the HTTP date (always GMT, so the zone letters are ESCAPED` |
|       - | 1272 | ` * rather than formatted -- php's own definition, and the reason it is not` |
|       - | 1273 | ` * DATE_RFC1123 with a T on the end). DATE_RFC3339_EXTENDED carries` |
|       - | 1274 | `` * milliseconds. DATE_ISO8601_EXPANDED uses `X`, the expanded-year field, where`` |
|       - | 1275 | `` * the plain DATE_ISO8601 uses `Y`.`` |
|       - | 1276 | ` */` |
|      64 | 1277 | `static void PH7_DATE_RFC7231_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1278 | `{` |
|      32 | 1279 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1280 | `	ph7_value_string(pVal,"D, d M Y H:i:s \\G\\M\\T",-1/*Compute length automatically*/);` |
|      67 | 1281 | `}` |
|      62 | 1282 | `static void PH7_DATE_RFC3339_EXTENDED_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1283 | `{` |
|      31 | 1284 | `	SXUNUSED(pUserData); /* cc warning */` |
|      65 | 1285 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:s.vP",-1/*Compute length automatically*/);` |
|      65 | 1286 | `}` |
|      62 | 1287 | `static void PH7_DATE_ISO8601_EXPANDED_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1288 | `{` |
|      31 | 1289 | `	SXUNUSED(pUserData); /* cc warning */` |
|      65 | 1290 | `	ph7_value_string(pVal,"X-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      65 | 1291 | `}` |
|       - | 1292 | `/*` |
|       - | 1293 | ` * FILE_TEXT / FILE_BINARY` |
|       - | 1294 | ` *  Both expand 0. php declares them for file()/file_put_contents()'s $flags and` |
|       - | 1295 | ` *  ignores them (the CLI has no text mode to select), but a program that names` |
|       - | 1296 | ` *  one still has to COMPILE, and an undefined constant is a fatal.` |
|       - | 1297 | ` */` |
|     126 | 1298 | `static void PH7_FILE_TEXT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1299 | `{` |
|     129 | 1300 | `	ph7_value_int(pVal,0);` |
|      63 | 1301 | `	SXUNUSED(pUserData);` |
|     129 | 1302 | `}` |
|       - | 1303 | `/*` |
|       - | 1304 | ` * The ENT_* values are PHP-exact (php 8.5.7). The low two bits are the quote` |
|       - | 1305 | ` * bits (1 = single, 2 = double), so ENT_QUOTES = ENT_COMPAT\|1 and` |
|       - | 1306 | ` * ENT_NOQUOTES = 0. Bits 16\|32 select the doctype (0 = HTML401, 16 = XML1,` |
|       - | 1307 | ` * 32 = XHTML, 48 = HTML5) — composites, not flags.` |
|       - | 1308 | ` */` |
|       - | 1309 | `/*` |
|       - | 1310 | ` * ENT_COMPAT` |
|       - | 1311 | ` *  Expand 2 (double-quote bit only)` |
|       - | 1312 | ` */` |
|      76 | 1313 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1314 | `{` |
|      38 | 1315 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 1316 | `	ph7_value_int(pVal,PH7_ENT_QUOTE_DOUBLE);` |
|      79 | 1317 | `}` |
|       - | 1318 | `/*` |
|       - | 1319 | ` * ENT_QUOTES` |
|       - | 1320 | ` *  Expand 3 (double\|single quote bits)` |
|       - | 1321 | ` */` |
|     212 | 1322 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1323 | `{` |
|     106 | 1324 | `	SXUNUSED(pUserData); /* cc warning */` |
|     215 | 1325 | `	ph7_value_int(pVal,PH7_ENT_QUOTES);` |
|     215 | 1326 | `}` |
|       - | 1327 | `/*` |
|       - | 1328 | ` * ENT_NOQUOTES` |
|       - | 1329 | ` *  Expand 0 (no quote bits)` |
|       - | 1330 | ` */` |
|      84 | 1331 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1332 | `{` |
|      42 | 1333 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1334 | `	ph7_value_int(pVal,0);` |
|      87 | 1335 | `}` |
|       - | 1336 | `/*` |
|       - | 1337 | ` * ENT_IGNORE` |
|       - | 1338 | ` *  Expand 4` |
|       - | 1339 | ` */` |
|      68 | 1340 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1341 | `{` |
|      34 | 1342 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1343 | `	ph7_value_int(pVal,PH7_ENT_IGNORE);` |
|      71 | 1344 | `}` |
|       - | 1345 | `/*` |
|       - | 1346 | ` * ENT_SUBSTITUTE` |
|       - | 1347 | ` *  Expand 8` |
|       - | 1348 | ` */` |
|      64 | 1349 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1350 | `{` |
|      32 | 1351 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1352 | `	ph7_value_int(pVal,PH7_ENT_SUBSTITUTE);` |
|      67 | 1353 | `}` |
|       - | 1354 | `/*` |
|       - | 1355 | ` * ENT_DISALLOWED` |
|       - | 1356 | ` *  Expand 128` |
|       - | 1357 | ` */` |
|     116 | 1358 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1359 | `{` |
|      58 | 1360 | `	SXUNUSED(pUserData); /* cc warning */` |
|     119 | 1361 | `	ph7_value_int(pVal,PH7_ENT_DISALLOWED);` |
|     119 | 1362 | `}` |
|       - | 1363 | `/*` |
|       - | 1364 | ` * ENT_HTML401` |
|       - | 1365 | ` *  Expand 0 (the default doctype)` |
|       - | 1366 | ` */` |
|      64 | 1367 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1368 | `{` |
|      32 | 1369 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1370 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML401);` |
|      67 | 1371 | `}` |
|       - | 1372 | `/*` |
|       - | 1373 | ` * ENT_XML1` |
|       - | 1374 | ` *  Expand 16` |
|       - | 1375 | ` */` |
|      72 | 1376 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1377 | `{` |
|      36 | 1378 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 1379 | `	ph7_value_int(pVal,PH7_ENT_DOC_XML1);` |
|      75 | 1380 | `}` |
|       - | 1381 | `/*` |
|       - | 1382 | ` * ENT_XHTML` |
|       - | 1383 | ` *  Expand 32` |
|       - | 1384 | ` */` |
|      68 | 1385 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1386 | `{` |
|      34 | 1387 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1388 | `	ph7_value_int(pVal,PH7_ENT_DOC_XHTML);` |
|      71 | 1389 | `}` |
|       - | 1390 | `/*` |
|       - | 1391 | ` * ENT_HTML5` |
|       - | 1392 | ` *  Expand 48 (16\|32 — a doctype composite, not a flag bit)` |
|       - | 1393 | ` */` |
|      70 | 1394 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1395 | `{` |
|      35 | 1396 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1397 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML5);` |
|      73 | 1398 | `}` |
|       - | 1399 | `/*` |
|       - | 1400 | ` * ISO-8859-1` |
|       - | 1401 | ` * ISO_8859_1` |
|       - | 1402 | ` *   Expand 1` |
|       - | 1403 | ` */` |
|     126 | 1404 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1405 | `{` |
|      63 | 1406 | `	SXUNUSED(pUserData); /* cc warning */` |
|     129 | 1407 | `	ph7_value_int(pVal,1);` |
|     129 | 1408 | `}` |
|       - | 1409 | `/*` |
|       - | 1410 | ` * UTF-8` |
|       - | 1411 | ` * UTF8` |
|       - | 1412 | ` *  Expand 2` |
|       - | 1413 | ` */` |
|     126 | 1414 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1415 | `{` |
|      63 | 1416 | `	SXUNUSED(pUserData); /* cc warning */` |
|     129 | 1417 | `	ph7_value_int(pVal,1);` |
|     129 | 1418 | `}` |
|       - | 1419 | `/*` |
|       - | 1420 | ` * HTML_ENTITIES` |
|       - | 1421 | ` *  Expand 1` |
|       - | 1422 | ` */` |
|      90 | 1423 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1424 | `{` |
|      45 | 1425 | `	SXUNUSED(pUserData); /* cc warning */` |
|      93 | 1426 | `	ph7_value_int(pVal,1);` |
|      93 | 1427 | `}` |
|       - | 1428 | `/*` |
|       - | 1429 | ` * HTML_SPECIALCHARS` |
|       - | 1430 | ` *  Expand 0 (PHP-exact)` |
|       - | 1431 | ` */` |
|      78 | 1432 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1433 | `{` |
|      39 | 1434 | `	SXUNUSED(pUserData); /* cc warning */` |
|      81 | 1435 | `	ph7_value_int(pVal,0);` |
|      81 | 1436 | `}` |
|       - | 1437 | `/*` |
|       - | 1438 | ` * PHP_URL_SCHEME.` |
|       - | 1439 | ` * Expand 0` |
|       - | 1440 | ` */` |
|      66 | 1441 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1442 | `{` |
|      33 | 1443 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1444 | `	ph7_value_int(pVal,0);` |
|      69 | 1445 | `}` |
|       - | 1446 | `/*` |
|       - | 1447 | ` * PHP_URL_HOST.` |
|       - | 1448 | ` * Expand 1` |
|       - | 1449 | ` */` |
|      68 | 1450 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1451 | `{` |
|      34 | 1452 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1453 | `	ph7_value_int(pVal,1);` |
|      71 | 1454 | `}` |
|       - | 1455 | `/*` |
|       - | 1456 | ` * PHP_URL_PORT.` |
|       - | 1457 | ` * Expand 2` |
|       - | 1458 | ` */` |
|      68 | 1459 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1460 | `{` |
|      34 | 1461 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1462 | `	ph7_value_int(pVal,2);` |
|      71 | 1463 | `}` |
|       - | 1464 | `/*` |
|       - | 1465 | ` * PHP_URL_USER.` |
|       - | 1466 | ` * Expand 3` |
|       - | 1467 | ` */` |
|      66 | 1468 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1469 | `{` |
|      33 | 1470 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1471 | `	ph7_value_int(pVal,3);` |
|      69 | 1472 | `}` |
|       - | 1473 | `/*` |
|       - | 1474 | ` * PHP_URL_PASS.` |
|       - | 1475 | ` * Expand 4` |
|       - | 1476 | ` */` |
|      66 | 1477 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1478 | `{` |
|      33 | 1479 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1480 | `	ph7_value_int(pVal,4);` |
|      69 | 1481 | `}` |
|       - | 1482 | `/*` |
|       - | 1483 | ` * PHP_URL_PATH.` |
|       - | 1484 | ` * Expand 5` |
|       - | 1485 | ` */` |
|      66 | 1486 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1487 | `{` |
|      33 | 1488 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1489 | `	ph7_value_int(pVal,5);` |
|      69 | 1490 | `}` |
|       - | 1491 | `/*` |
|       - | 1492 | ` * PHP_URL_QUERY.` |
|       - | 1493 | ` * Expand 6` |
|       - | 1494 | ` */` |
|      68 | 1495 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1496 | `{` |
|      34 | 1497 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1498 | `	ph7_value_int(pVal,6);` |
|      71 | 1499 | `}` |
|       - | 1500 | `/*` |
|       - | 1501 | ` * PHP_URL_FRAGMENT.` |
|       - | 1502 | ` * Expand 7` |
|       - | 1503 | ` */` |
|      68 | 1504 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1505 | `{` |
|      34 | 1506 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1507 | `	ph7_value_int(pVal,7);` |
|      71 | 1508 | `}` |
|       - | 1509 | `/*` |
|       - | 1510 | ` * PHP_QUERY_RFC1738` |
|       - | 1511 | ` * Expand 1` |
|       - | 1512 | ` */` |
|      64 | 1513 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1514 | `{` |
|      32 | 1515 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1516 | `	ph7_value_int(pVal,1);` |
|      67 | 1517 | `}` |
|       - | 1518 | `/*` |
|       - | 1519 | ` * PHP_QUERY_RFC3986` |
|       - | 1520 | ` * Expand 1` |
|       - | 1521 | ` */` |
|      66 | 1522 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1523 | `{` |
|      33 | 1524 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1525 | `	ph7_value_int(pVal,2);` |
|      69 | 1526 | `}` |
|       - | 1527 | `/* php's FNM_* values (ext/standard): PATHNAME=1, NOESCAPE=2, PERIOD=4, CASEFOLD=16.` |
|       - | 1528 | ` * PHL previously had PATHNAME/NOESCAPE swapped and CASEFOLD=8; fnmatch() reads these` |
|       - | 1529 | ` * bits, so PH7_builtin_fnmatch was updated to the same values. */` |
|       - | 1530 | `/*` |
|       - | 1531 | ` * FNM_PATHNAME` |
|       - | 1532 | ` *  Expand 1 (php value)` |
|       - | 1533 | ` */` |
|      62 | 1534 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1535 | `{` |
|      31 | 1536 | `	SXUNUSED(pUserData); /* cc warning */` |
|      65 | 1537 | `	ph7_value_int(pVal,1);` |
|      65 | 1538 | `}` |
|       - | 1539 | `/*` |
|       - | 1540 | ` * FNM_NOESCAPE` |
|       - | 1541 | ` *  Expand 2 (php value)` |
|       - | 1542 | ` */` |
|      62 | 1543 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1544 | `{` |
|      31 | 1545 | `	SXUNUSED(pUserData); /* cc warning */` |
|      65 | 1546 | `	ph7_value_int(pVal,2);` |
|      65 | 1547 | `}` |
|       - | 1548 | `/*` |
|       - | 1549 | ` * FNM_PERIOD` |
|       - | 1550 | ` *  Expand 4 (php value)` |
|       - | 1551 | ` */` |
|      68 | 1552 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1553 | `{` |
|      34 | 1554 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1555 | `	ph7_value_int(pVal,4);` |
|      71 | 1556 | `}` |
|       - | 1557 | `/*` |
|       - | 1558 | ` * FNM_CASEFOLD` |
|       - | 1559 | ` *  Expand 16 (php value)` |
|       - | 1560 | ` */` |
|      66 | 1561 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1562 | `{` |
|      33 | 1563 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1564 | `	ph7_value_int(pVal,16);` |
|      69 | 1565 | `}` |
|       - | 1566 | `/*` |
|       - | 1567 | ` * PATHINFO_DIRNAME` |
|       - | 1568 | ` *  Expand 1.` |
|       - | 1569 | ` */` |
|      84 | 1570 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1571 | `{` |
|      42 | 1572 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1573 | `	ph7_value_int(pVal,PH7_PATHINFO_DIRNAME);` |
|      87 | 1574 | `}` |
|       - | 1575 | `/*` |
|       - | 1576 | ` * PATHINFO_BASENAME` |
|       - | 1577 | ` *  Expand 2.` |
|       - | 1578 | ` */` |
|      84 | 1579 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1580 | `{` |
|      42 | 1581 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1582 | `	ph7_value_int(pVal,PH7_PATHINFO_BASENAME);` |
|      87 | 1583 | `}` |
|       - | 1584 | `/*` |
|       - | 1585 | ` * PATHINFO_EXTENSION` |
|       - | 1586 | ` *  Expand php's 4 (a POWER OF TWO: the components are a bitmask).` |
|       - | 1587 | ` */` |
|    8414 | 1588 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1589 | `{` |
|    4207 | 1590 | `	SXUNUSED(pUserData); /* cc warning */` |
|    8419 | 1591 | `	ph7_value_int(pVal,PH7_PATHINFO_EXTENSION);` |
|    8419 | 1592 | `}` |
|       - | 1593 | `/*` |
|       - | 1594 | ` * PATHINFO_FILENAME` |
|       - | 1595 | ` *  Expand php's 8 (a POWER OF TWO: the components are a bitmask).` |
|       - | 1596 | ` */` |
|    8402 | 1597 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1598 | `{` |
|    4201 | 1599 | `	SXUNUSED(pUserData); /* cc warning */` |
|    8407 | 1600 | `	ph7_value_int(pVal,PH7_PATHINFO_FILENAME);` |
|    8407 | 1601 | `}` |
|       - | 1602 | `/*` |
|       - | 1603 | ` * PATHINFO_ALL` |
|       - | 1604 | ` *  Expand php's 15 — the default, and the one value that answers with the ARRAY.` |
|       - | 1605 | ` */` |
|      72 | 1606 | `static void PH7_PATHINFO_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1607 | `{` |
|      36 | 1608 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 1609 | `	ph7_value_int(pVal,PH7_PATHINFO_ALL);` |
|      75 | 1610 | `}` |
|       - | 1611 | `/*` |
|       - | 1612 | ` * SEEK_SET.` |
|       - | 1613 | ` *  Expand 0` |
|       - | 1614 | ` */` |
|      82 | 1615 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1616 | `{` |
|      41 | 1617 | `	SXUNUSED(pUserData); /* cc warning */` |
|      85 | 1618 | `	ph7_value_int(pVal,0);` |
|      85 | 1619 | `}` |
|       - | 1620 | `/*` |
|       - | 1621 | ` * SEEK_CUR.` |
|       - | 1622 | ` *  Expand 1` |
|       - | 1623 | ` */` |
|      76 | 1624 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1625 | `{` |
|      38 | 1626 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 1627 | `	ph7_value_int(pVal,1);` |
|      79 | 1628 | `}` |
|       - | 1629 | `/*` |
|       - | 1630 | ` * SEEK_END.` |
|       - | 1631 | ` *  Expand 2` |
|       - | 1632 | ` */` |
|      70 | 1633 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1634 | `{` |
|      35 | 1635 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1636 | `	ph7_value_int(pVal,2);` |
|      73 | 1637 | `}` |
|       - | 1638 | `/*` |
|       - | 1639 | ` * LOCK_SH.` |
|       - | 1640 | ` *  Expand 2` |
|       - | 1641 | ` */` |
|      74 | 1642 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1643 | `{` |
|      37 | 1644 | `	SXUNUSED(pUserData); /* cc warning */` |
|      77 | 1645 | `	ph7_value_int(pVal,1);` |
|      77 | 1646 | `}` |
|       - | 1647 | `/*` |
|       - | 1648 | ` * LOCK_NB.` |
|       - | 1649 | ` *  Expand 4 (php)` |
|       - | 1650 | ` */` |
|      80 | 1651 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1652 | `{` |
|      40 | 1653 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 1654 | `	ph7_value_int(pVal,4);` |
|      83 | 1655 | `}` |
|       - | 1656 | `/*` |
|       - | 1657 | ` * LOCK_EX.` |
|       - | 1658 | ` *  Expand 2 (php). PH7 used 1, which collided with LOCK_SH, and LOCK_UN was 0 — so` |
|       - | 1659 | ` *  flock($h, LOCK_UN) asked the stream for a SHARED lock instead of releasing one.` |
|       - | 1660 | ` */` |
|      80 | 1661 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1662 | `{` |
|      40 | 1663 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 1664 | `	ph7_value_int(pVal,2);` |
|      83 | 1665 | `}` |
|       - | 1666 | `/*` |
|       - | 1667 | ` * LOCK_UN.` |
|       - | 1668 | ` *  Expand 3 (php)` |
|       - | 1669 | ` */` |
|      72 | 1670 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1671 | `{` |
|      36 | 1672 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 1673 | `	ph7_value_int(pVal,3);` |
|      75 | 1674 | `}` |
|       - | 1675 | `/*` |
|       - | 1676 | ` * FILE_USE_INCLUDE_PATH` |
|       - | 1677 | ` *  Expand 0x01 (Must be a power of two)` |
|       - | 1678 | ` */` |
|      66 | 1679 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1680 | `{` |
|      33 | 1681 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1682 | `	ph7_value_int(pVal,0x1);` |
|      69 | 1683 | `}` |
|       - | 1684 | `/*` |
|       - | 1685 | ` * FILE_IGNORE_NEW_LINES` |
|       - | 1686 | ` *  Expand 0x02 (Must be a power of two)` |
|       - | 1687 | ` */` |
|      74 | 1688 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1689 | `{` |
|      37 | 1690 | `	SXUNUSED(pUserData); /* cc warning */` |
|      77 | 1691 | `	ph7_value_int(pVal,0x2);` |
|      77 | 1692 | `}` |
|       - | 1693 | `/*` |
|       - | 1694 | ` * FILE_SKIP_EMPTY_LINES` |
|       - | 1695 | ` *  Expand 0x04 (Must be a power of two)` |
|       - | 1696 | ` */` |
|      66 | 1697 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1698 | `{` |
|      33 | 1699 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1700 | `	ph7_value_int(pVal,0x4);` |
|      69 | 1701 | `}` |
|       - | 1702 | `/*` |
|       - | 1703 | ` * FILE_APPEND` |
|       - | 1704 | ` *  Expand 0x08 (Must be a power of two)` |
|       - | 1705 | ` */` |
|      66 | 1706 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1707 | `{` |
|      33 | 1708 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1709 | `	ph7_value_int(pVal,0x08);` |
|      69 | 1710 | `}` |
|       - | 1711 | `/*` |
|       - | 1712 | ` * FILE_NO_DEFAULT_CONTEXT` |
|       - | 1713 | ` *  Expand php's 0x10. file()/file_put_contents() read it: it is what stops the` |
|       - | 1714 | `` *  `$context = null` argument from resolving to stream_context_get_default()'s`` |
|       - | 1715 | ` *  context. What a device then does with an open carrying no context is its own` |
|       - | 1716 | ` *  business — a userland wrapper's $this->context is a resource either way, in` |
|       - | 1717 | ` *  php as here — so the flag is only observable where an option is consumed.` |
|       - | 1718 | ` */` |
|      70 | 1719 | `static void PH7_FILE_NO_DEFAULT_CONTEXT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1720 | `{` |
|      35 | 1721 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1722 | `	ph7_value_int(pVal,0x10);` |
|      73 | 1723 | `}` |
|       - | 1724 | `/*` |
|       - | 1725 | ` * SCANDIR_SORT_ASCENDING` |
|       - | 1726 | ` *  Expand 0` |
|       - | 1727 | ` */` |
|    2530 | 1728 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1729 | `{` |
|    1263 | 1730 | `	SXUNUSED(pUserData); /* cc warning */` |
|    2535 | 1731 | `	ph7_value_int(pVal,0);` |
|    2535 | 1732 | `}` |
|       - | 1733 | `/*` |
|       - | 1734 | ` * SCANDIR_SORT_DESCENDING` |
|       - | 1735 | ` *  Expand 1` |
|       - | 1736 | ` */` |
|      68 | 1737 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1738 | `{` |
|      34 | 1739 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1740 | `	ph7_value_int(pVal,1);` |
|      71 | 1741 | `}` |
|       - | 1742 | `/*` |
|       - | 1743 | ` * SCANDIR_SORT_NONE` |
|       - | 1744 | ` *  Expand 2` |
|       - | 1745 | ` */` |
|    1304 | 1746 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1747 | `{` |
|     651 | 1748 | `	SXUNUSED(pUserData); /* cc warning */` |
|    1309 | 1749 | `	ph7_value_int(pVal,2);` |
|    1309 | 1750 | `}` |
|       - | 1751 | `/*` |
|       - | 1752 | ` * GLOB_MARK` |
|       - | 1753 | ` *  Expand php's 0x08 (php's own portable glob flag set)` |
|       - | 1754 | ` */` |
|      80 | 1755 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1756 | `{` |
|      40 | 1757 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 1758 | `	ph7_value_int(pVal,PH7_GLOB_MARK);` |
|      83 | 1759 | `}` |
|       - | 1760 | `/*` |
|       - | 1761 | ` * GLOB_NOSORT` |
|       - | 1762 | ` *  Expand php's 0x20` |
|       - | 1763 | ` */` |
|     148 | 1764 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1765 | `{` |
|      74 | 1766 | `	SXUNUSED(pUserData); /* cc warning */` |
|     153 | 1767 | `	ph7_value_int(pVal,PH7_GLOB_NOSORT);` |
|     153 | 1768 | `}` |
|       - | 1769 | `/*` |
|       - | 1770 | ` * GLOB_NOCHECK` |
|       - | 1771 | ` *  Expand php's 0x10` |
|       - | 1772 | ` */` |
|     156 | 1773 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1774 | `{` |
|      78 | 1775 | `	SXUNUSED(pUserData); /* cc warning */` |
|     161 | 1776 | `	ph7_value_int(pVal,PH7_GLOB_NOCHECK);` |
|     161 | 1777 | `}` |
|       - | 1778 | `/*` |
|       - | 1779 | ` * GLOB_NOESCAPE` |
|       - | 1780 | ` *  Expand php's 0x1000` |
|       - | 1781 | ` */` |
|      68 | 1782 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1783 | `{` |
|      34 | 1784 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1785 | `	ph7_value_int(pVal,PH7_GLOB_NOESCAPE);` |
|      71 | 1786 | `}` |
|       - | 1787 | `/*` |
|       - | 1788 | ` * GLOB_BRACE` |
|       - | 1789 | ` *  Expand php's 0x80` |
|       - | 1790 | ` */` |
|     164 | 1791 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1792 | `{` |
|      82 | 1793 | `	SXUNUSED(pUserData); /* cc warning */` |
|     169 | 1794 | `	ph7_value_int(pVal,PH7_GLOB_BRACE);` |
|     169 | 1795 | `}` |
|       - | 1796 | `/*` |
|       - | 1797 | ` * GLOB_ONLYDIR` |
|       - | 1798 | ` *  Expand php's 0x40000000` |
|       - | 1799 | ` */` |
|     130 | 1800 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1801 | `{` |
|      65 | 1802 | `	SXUNUSED(pUserData); /* cc warning */` |
|     134 | 1803 | `	ph7_value_int(pVal,PH7_GLOB_ONLYDIR);` |
|     134 | 1804 | `}` |
|       - | 1805 | `/*` |
|       - | 1806 | ` * GLOB_ERR` |
|       - | 1807 | ` *  Expand php's 0x04` |
|       - | 1808 | ` */` |
|      70 | 1809 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1810 | `{` |
|      35 | 1811 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1812 | `	ph7_value_int(pVal,PH7_GLOB_ERR);` |
|      73 | 1813 | `}` |
|       - | 1814 | `/*` |
|       - | 1815 | ` * GLOB_AVAILABLE_FLAGS` |
|       - | 1816 | ` *  Expand the OR of every glob flag php's portable glob accepts — the mask` |
|       - | 1817 | ` *  glob() itself validates against (1073746108 on every platform, since the` |
|       - | 1818 | ` *  GLOB_* values are php 8.5's own portable set).` |
|       - | 1819 | ` */` |
|     170 | 1820 | `static void PH7_GLOB_AVAILABLE_FLAGS_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1821 | `{` |
|      85 | 1822 | `	SXUNUSED(pUserData); /* cc warning */` |
|     175 | 1823 | `	ph7_value_int(pVal,PH7_GLOB_ERR\|PH7_GLOB_MARK\|PH7_GLOB_NOCHECK\|PH7_GLOB_NOSORT` |
|       - | 1824 | `		\|PH7_GLOB_BRACE\|PH7_GLOB_NOESCAPE\|PH7_GLOB_ONLYDIR);` |
|     175 | 1825 | `}` |
|       - | 1826 | `/*` |
|       - | 1827 | ` * STDIN` |
|       - | 1828 | ` *  Expand the STDIN handle as a resource.` |
|       - | 1829 | ` */` |
|     144 | 1830 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1831 | `{` |
|     148 | 1832 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 1833 | `	void *pResource;` |
|     148 | 1834 | `	pResource = PH7_ExportStdin(pVm);` |
|     148 | 1835 | `	ph7_value_resource(pVal,pResource);` |
|     148 | 1836 | `}` |
|       - | 1837 | `/*` |
|       - | 1838 | ` * STDOUT` |
|       - | 1839 | ` *   Expand the STDOUT handle as a resource.` |
|       - | 1840 | ` */` |
|     134 | 1841 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1842 | `{` |
|     137 | 1843 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 1844 | `	void *pResource;` |
|     137 | 1845 | `	pResource = PH7_ExportStdout(pVm);` |
|     137 | 1846 | `	ph7_value_resource(pVal,pResource);` |
|     137 | 1847 | `}` |
|       - | 1848 | `/*` |
|       - | 1849 | ` * STDERR` |
|       - | 1850 | ` *  Expand the STDERR handle as a resource.` |
|       - | 1851 | ` */` |
|     136 | 1852 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1853 | `{` |
|     139 | 1854 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 1855 | `	void *pResource;` |
|     139 | 1856 | `	pResource = PH7_ExportStderr(pVm);` |
|     139 | 1857 | `	ph7_value_resource(pVal,pResource);` |
|     139 | 1858 | `}` |
|       - | 1859 | `/*` |
|       - | 1860 | ` * INI_SCANNER_NORMAL` |
|       - | 1861 | ` *   Expand php's 0` |
|       - | 1862 | ` */` |
|      68 | 1863 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1864 | `{` |
|      34 | 1865 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1866 | `	ph7_value_int(pVal,PH7_INI_SCANNER_NORMAL);` |
|      71 | 1867 | `}` |
|       - | 1868 | `/*` |
|       - | 1869 | ` * INI_SCANNER_RAW` |
|       - | 1870 | ` *   Expand php's 1` |
|       - | 1871 | ` */` |
|      70 | 1872 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1873 | `{` |
|      35 | 1874 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1875 | `	ph7_value_int(pVal,PH7_INI_SCANNER_RAW);` |
|      73 | 1876 | `}` |
|       - | 1877 | `/*` |
|       - | 1878 | ` * INI_SCANNER_TYPED` |
|       - | 1879 | ` *   Expand 2 (php's value)` |
|       - | 1880 | ` */` |
|      70 | 1881 | `static void PH7_INI_SCANNER_TYPED_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1882 | `{` |
|      35 | 1883 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1884 | `	ph7_value_int(pVal,PH7_INI_SCANNER_TYPED);` |
|      73 | 1885 | `}` |
|       - | 1886 | `/*` |
|       - | 1887 | ` * EXTR_OVERWRITE` |
|       - | 1888 | ` *   Expand 0 (php's enum value; see PH7_EXTR_* in ph7int.h)` |
|       - | 1889 | ` */` |
|      76 | 1890 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1891 | `{` |
|      38 | 1892 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 1893 | `	ph7_value_int(pVal,PH7_EXTR_OVERWRITE);` |
|      80 | 1894 | `}` |
|       - | 1895 | `/*` |
|       - | 1896 | ` * EXTR_SKIP` |
|       - | 1897 | ` *   Expand 1` |
|       - | 1898 | ` */` |
|      78 | 1899 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1900 | `{` |
|      39 | 1901 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 1902 | `	ph7_value_int(pVal,PH7_EXTR_SKIP);` |
|      83 | 1903 | `}` |
|       - | 1904 | `/*` |
|       - | 1905 | ` * EXTR_PREFIX_SAME` |
|       - | 1906 | ` *   Expand 2` |
|       - | 1907 | ` */` |
|     100 | 1908 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1909 | `{` |
|      50 | 1910 | `	SXUNUSED(pUserData); /* cc warning */` |
|     105 | 1911 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_SAME);` |
|     105 | 1912 | `}` |
|       - | 1913 | `/*` |
|       - | 1914 | ` * EXTR_PREFIX_ALL` |
|       - | 1915 | ` *   Expand 3` |
|       - | 1916 | ` */` |
|      86 | 1917 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1918 | `{` |
|      43 | 1919 | `	SXUNUSED(pUserData); /* cc warning */` |
|      90 | 1920 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_ALL);` |
|      90 | 1921 | `}` |
|       - | 1922 | `/*` |
|       - | 1923 | ` * EXTR_PREFIX_INVALID` |
|       - | 1924 | ` *   Expand 4` |
|       - | 1925 | ` */` |
|      70 | 1926 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1927 | `{` |
|      35 | 1928 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1929 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_INVALID);` |
|      73 | 1930 | `}` |
|       - | 1931 | `/*` |
|       - | 1932 | ` * EXTR_IF_EXISTS` |
|       - | 1933 | ` *   Expand 6 (php orders IF_EXISTS after PREFIX_IF_EXISTS)` |
|       - | 1934 | ` */` |
|      78 | 1935 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1936 | `{` |
|      39 | 1937 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 1938 | `	ph7_value_int(pVal,PH7_EXTR_IF_EXISTS);` |
|      83 | 1939 | `}` |
|       - | 1940 | `/*` |
|       - | 1941 | ` * EXTR_REFS` |
|       - | 1942 | ` *   Expand 256 (the bit that rides above the mode: bind by REFERENCE)` |
|       - | 1943 | ` */` |
|      76 | 1944 | `static void PH7_EXTR_REFS_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1945 | `{` |
|      38 | 1946 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 1947 | `	ph7_value_int(pVal,PH7_EXTR_REFS);` |
|      80 | 1948 | `}` |
|       - | 1949 | `/*` |
|       - | 1950 | ` * EXTR_PREFIX_IF_EXISTS` |
|       - | 1951 | ` *   Expand 5` |
|       - | 1952 | ` */` |
|      82 | 1953 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1954 | `{` |
|      41 | 1955 | `	SXUNUSED(pUserData); /* cc warning */` |
|      86 | 1956 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_IF_EXISTS);` |
|      86 | 1957 | `}` |
|       - | 1958 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|       - | 1959 | `/*` |
|       - | 1960 | ` * HASH_HMAC.` |
|       - | 1961 | ` *   php's one hash_init() flag. Declared with the hash extension it belongs` |
|       - | 1962 | ` *   to, so a build without that extension has no constant either.` |
|       - | 1963 | ` */` |
|      74 | 1964 | `static void PH7_HASH_HMAC_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1965 | `{` |
|      37 | 1966 | `	SXUNUSED(pUserData); /* cc warning */` |
|      77 | 1967 | `	ph7_value_int(pVal,PH7_HASH_HMAC);` |
|      77 | 1968 | `}` |
|       - | 1969 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|       - | 1970 | `/*` |
|       - | 1971 | ` * ICONV_* — what the converter IS, and iconv_mime_decode()'s $mode bits.` |
|       - | 1972 | `` * php reports the C library behind its extension here (`glibc`, `libiconv`);`` |
|       - | 1973 | ` * PHL converts with its own code so that a Windows build answers what a POSIX` |
|       - | 1974 | ` * one does, and says so — the constants exist to be READ, and a program that` |
|       - | 1975 | ` * branches on them has to see something true.` |
|       - | 1976 | ` */` |
|      64 | 1977 | `static void PH7_ICONV_IMPL_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 1978 | `{` |
|      32 | 1979 | `	SXUNUSED(pUnused);` |
|      67 | 1980 | `	ph7_value_string(pVal,"PHL",(int)sizeof("PHL")-1);` |
|      67 | 1981 | `}` |
|      64 | 1982 | `static void PH7_ICONV_VERSION_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 1983 | `{` |
|      32 | 1984 | `	SXUNUSED(pUnused);` |
|      67 | 1985 | `	ph7_value_string(pVal,PH7_VERSION,(int)sizeof(PH7_VERSION)-1);` |
|      67 | 1986 | `}` |
|      66 | 1987 | `static void PH7_ICONV_MIME_DECODE_STRICT_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 1988 | `{` |
|      33 | 1989 | `	SXUNUSED(pUnused);` |
|      69 | 1990 | `	ph7_value_int(pVal,1);` |
|      69 | 1991 | `}` |
|      66 | 1992 | `static void PH7_ICONV_MIME_DECODE_CONTINUE_ON_ERROR_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 1993 | `{` |
|      33 | 1994 | `	SXUNUSED(pUnused);` |
|      69 | 1995 | `	ph7_value_int(pVal,2);` |
|      69 | 1996 | `}` |
|       - | 1997 | `/*` |
|       - | 1998 | ` * JSON_HEX_TAG.` |
|       - | 1999 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|       - | 2000 | ` */` |
|      68 | 2001 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2002 | `{` |
|      34 | 2003 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 2004 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      71 | 2005 | `}` |
|       - | 2006 | `/*` |
|       - | 2007 | ` * JSON_HEX_AMP.` |
|       - | 2008 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|       - | 2009 | ` */` |
|      68 | 2010 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2011 | `{` |
|      34 | 2012 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 2013 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      71 | 2014 | `}` |
|       - | 2015 | `/*` |
|       - | 2016 | ` * JSON_HEX_APOS.` |
|       - | 2017 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|       - | 2018 | ` */` |
|      68 | 2019 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2020 | `{` |
|      34 | 2021 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 2022 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      71 | 2023 | `}` |
|       - | 2024 | `/*` |
|       - | 2025 | ` * JSON_HEX_QUOT.` |
|       - | 2026 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|       - | 2027 | ` */` |
|      68 | 2028 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2029 | `{` |
|      34 | 2030 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 2031 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      71 | 2032 | `}` |
|       - | 2033 | `/*` |
|       - | 2034 | ` * JSON_FORCE_OBJECT.` |
|       - | 2035 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|       - | 2036 | ` */` |
|      68 | 2037 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2038 | `{` |
|      34 | 2039 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 2040 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      71 | 2041 | `}` |
|       - | 2042 | `/*` |
|       - | 2043 | ` * JSON_NUMERIC_CHECK.` |
|       - | 2044 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|       - | 2045 | ` */` |
|      72 | 2046 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2047 | `{` |
|      36 | 2048 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 2049 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      75 | 2050 | `}` |
|       - | 2051 | `/*` |
|       - | 2052 | ` * JSON_BIGINT_AS_STRING.` |
|       - | 2053 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|       - | 2054 | ` */` |
|      76 | 2055 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2056 | `{` |
|      38 | 2057 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 2058 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      79 | 2059 | `}` |
|       - | 2060 | `/*` |
|       - | 2061 | ` * JSON_PARTIAL_OUTPUT_ON_ERROR.` |
|       - | 2062 | ` *   Expand the value of JSON_PARTIAL_OUTPUT_ON_ERROR defined in ph7Int.h.` |
|       - | 2063 | ` */` |
|      86 | 2064 | `static void PH7_JSON_PARTIAL_OUTPUT_ON_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2065 | `{` |
|      43 | 2066 | `	SXUNUSED(pUserData); /* cc warning */` |
|      89 | 2067 | `	ph7_value_int(pVal,JSON_PARTIAL_OUTPUT_ON_ERROR);` |
|      89 | 2068 | `}` |
|       - | 2069 | `/*` |
|       - | 2070 | ` * JSON_PRESERVE_ZERO_FRACTION.` |
|       - | 2071 | ` *   Expand the value of JSON_PRESERVE_ZERO_FRACTION defined in ph7Int.h.` |
|       - | 2072 | ` */` |
|      76 | 2073 | `static void PH7_JSON_PRESERVE_ZERO_FRACTION_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2074 | `{` |
|      38 | 2075 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 2076 | `	ph7_value_int(pVal,JSON_PRESERVE_ZERO_FRACTION);` |
|      79 | 2077 | `}` |
|       - | 2078 | `/*` |
|       - | 2079 | ` * JSON_OBJECT_AS_ARRAY.` |
|       - | 2080 | ` *   Expand the value of JSON_OBJECT_AS_ARRAY defined in ph7Int.h.` |
|       - | 2081 | ` */` |
|      72 | 2082 | `static void PH7_JSON_OBJECT_AS_ARRAY_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2083 | `{` |
|      36 | 2084 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 2085 | `	ph7_value_int(pVal,JSON_OBJECT_AS_ARRAY);` |
|      75 | 2086 | `}` |
|       - | 2087 | `/*` |
|       - | 2088 | ` * JSON_PRETTY_PRINT.` |
|       - | 2089 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|       - | 2090 | ` */` |
|      76 | 2091 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2092 | `{` |
|      38 | 2093 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 2094 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      79 | 2095 | `}` |
|       - | 2096 | `/*` |
|       - | 2097 | ` * JSON_UNESCAPED_SLASHES.` |
|       - | 2098 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|       - | 2099 | ` */` |
|      72 | 2100 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2101 | `{` |
|      36 | 2102 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 2103 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      75 | 2104 | `}` |
|       - | 2105 | `/*` |
|       - | 2106 | ` * JSON_UNESCAPED_UNICODE.` |
|       - | 2107 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|       - | 2108 | ` */` |
|      84 | 2109 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2110 | `{` |
|      42 | 2111 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 2112 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|      87 | 2113 | `}` |
|       - | 2114 | `/*` |
|       - | 2115 | ` * JSON_UNESCAPED_LINE_TERMINATORS.` |
|       - | 2116 | ` *   Expand the value of JSON_UNESCAPED_LINE_TERMINATORS defined in ph7Int.h.` |
|       - | 2117 | ` */` |
|      68 | 2118 | `static void PH7_JSON_UNESCAPED_LINE_TERMINATORS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2119 | `{` |
|      34 | 2120 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 2121 | `	ph7_value_int(pVal,JSON_UNESCAPED_LINE_TERMINATORS);` |
|      71 | 2122 | `}` |
|       - | 2123 | `/*` |
|       - | 2124 | ` * JSON_INVALID_UTF8_IGNORE.` |
|       - | 2125 | ` *   Expand the value of JSON_INVALID_UTF8_IGNORE defined in ph7Int.h.` |
|       - | 2126 | ` */` |
|      88 | 2127 | `static void PH7_JSON_INVALID_UTF8_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2128 | `{` |
|      44 | 2129 | `	SXUNUSED(pUserData); /* cc warning */` |
|      92 | 2130 | `	ph7_value_int(pVal,JSON_INVALID_UTF8_IGNORE);` |
|      92 | 2131 | `}` |
|       - | 2132 | `/*` |
|       - | 2133 | ` * JSON_INVALID_UTF8_SUBSTITUTE.` |
|       - | 2134 | ` *   Expand the value of JSON_INVALID_UTF8_SUBSTITUTE defined in ph7Int.h.` |
|       - | 2135 | ` */` |
|      92 | 2136 | `static void PH7_JSON_INVALID_UTF8_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2137 | `{` |
|      46 | 2138 | `	SXUNUSED(pUserData); /* cc warning */` |
|      95 | 2139 | `	ph7_value_int(pVal,JSON_INVALID_UTF8_SUBSTITUTE);` |
|      95 | 2140 | `}` |
|       - | 2141 | `/*` |
|       - | 2142 | ` * JSON_THROW_ON_ERROR.` |
|       - | 2143 | ` *   Expand the value of JSON_THROW_ON_ERROR defined in ph7Int.h.` |
|       - | 2144 | ` */` |
|      82 | 2145 | `static void PH7_JSON_THROW_ON_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2146 | `{` |
|      41 | 2147 | `	SXUNUSED(pUserData); /* cc warning */` |
|      86 | 2148 | `	ph7_value_int(pVal,JSON_THROW_ON_ERROR);` |
|      86 | 2149 | `}` |
|       - | 2150 | `/*` |
|       - | 2151 | ` * JSON_ERROR_NONE.` |
|       - | 2152 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|       - | 2153 | ` */` |
|      66 | 2154 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2155 | `{` |
|      33 | 2156 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 2157 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      69 | 2158 | `}` |
|       - | 2159 | `/*` |
|       - | 2160 | ` * JSON_ERROR_DEPTH.` |
|       - | 2161 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|       - | 2162 | ` */` |
|      64 | 2163 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2164 | `{` |
|      32 | 2165 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 2166 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      67 | 2167 | `}` |
|       - | 2168 | `/*` |
|       - | 2169 | ` * JSON_ERROR_STATE_MISMATCH.` |
|       - | 2170 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|       - | 2171 | ` */` |
|      64 | 2172 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2173 | `{` |
|      32 | 2174 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 2175 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      67 | 2176 | `}` |
|       - | 2177 | `/*` |
|       - | 2178 | ` * JSON_ERROR_CTRL_CHAR.` |
|       - | 2179 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|       - | 2180 | ` */` |
|      64 | 2181 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2182 | `{` |
|      32 | 2183 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 2184 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      67 | 2185 | `}` |
|       - | 2186 | `/*` |
|       - | 2187 | ` * JSON_ERROR_SYNTAX.` |
|       - | 2188 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|       - | 2189 | ` */` |
|      66 | 2190 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2191 | `{` |
|      33 | 2192 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 2193 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      69 | 2194 | `}` |
|       - | 2195 | `/*` |
|       - | 2196 | ` * JSON_ERROR_UTF8.` |
|       - | 2197 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|       - | 2198 | ` */` |
|      64 | 2199 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2200 | `{` |
|      32 | 2201 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 2202 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      67 | 2203 | `}` |
|       - | 2204 | `/*` |
|       - | 2205 | ` * JSON_ERROR_RECURSION.` |
|       - | 2206 | ` *   Expand the value of JSON_ERROR_RECURSION defined in ph7Int.h.` |
|       - | 2207 | ` */` |
|      64 | 2208 | `static void PH7_JSON_ERROR_RECURSION_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2209 | `{` |
|      32 | 2210 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 2211 | `	ph7_value_int(pVal,JSON_ERROR_RECURSION);` |
|      67 | 2212 | `}` |
|       - | 2213 | `/*` |
|       - | 2214 | ` * JSON_ERROR_UNSUPPORTED_TYPE.` |
|       - | 2215 | ` *   Expand the value of JSON_ERROR_UNSUPPORTED_TYPE defined in ph7Int.h.` |
|       - | 2216 | ` */` |
|      64 | 2217 | `static void PH7_JSON_ERROR_UNSUPPORTED_TYPE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2218 | `{` |
|      32 | 2219 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 2220 | `	ph7_value_int(pVal,JSON_ERROR_UNSUPPORTED_TYPE);` |
|      67 | 2221 | `}` |
|       - | 2222 | `/*` |
|       - | 2223 | ` * JSON_ERROR_INVALID_PROPERTY_NAME.` |
|       - | 2224 | ` *   Expand the value of JSON_ERROR_INVALID_PROPERTY_NAME defined in ph7Int.h.` |
|       - | 2225 | ` */` |
|      64 | 2226 | `static void PH7_JSON_ERROR_INVALID_PROPERTY_NAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2227 | `{` |
|      32 | 2228 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 2229 | `	ph7_value_int(pVal,JSON_ERROR_INVALID_PROPERTY_NAME);` |
|      67 | 2230 | `}` |
|       - | 2231 | `/*` |
|       - | 2232 | ` * JSON_ERROR_UTF16.` |
|       - | 2233 | ` *   Expand the value of JSON_ERROR_UTF16 defined in ph7Int.h.` |
|       - | 2234 | ` */` |
|      66 | 2235 | `static void PH7_JSON_ERROR_UTF16_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2236 | `{` |
|      33 | 2237 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 2238 | `	ph7_value_int(pVal,JSON_ERROR_UTF16);` |
|      69 | 2239 | `}` |
|       - | 2240 | `/*` |
|       - | 2241 | ` * JSON_ERROR_NON_BACKED_ENUM.` |
|       - | 2242 | ` *   Expand the value of JSON_ERROR_NON_BACKED_ENUM defined in ph7Int.h (php 8.1).` |
|       - | 2243 | ` */` |
|      62 | 2244 | `static void PH7_JSON_ERROR_NON_BACKED_ENUM_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2245 | `{` |
|      31 | 2246 | `	SXUNUSED(pUserData); /* cc warning */` |
|      65 | 2247 | `	ph7_value_int(pVal,JSON_ERROR_NON_BACKED_ENUM);` |
|      65 | 2248 | `}` |
|       - | 2249 | `/*` |
|       - | 2250 | ` * JSON_ERROR_INF_OR_NAN.` |
|       - | 2251 | ` *   Expand the value of JSON_ERROR_INF_OR_NAN defined in ph7Int.h.` |
|       - | 2252 | ` */` |
|      64 | 2253 | `static void PH7_JSON_ERROR_INF_OR_NAN_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2254 | `{` |
|      32 | 2255 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 2256 | `	ph7_value_int(pVal,JSON_ERROR_INF_OR_NAN);` |
|      67 | 2257 | `}` |
|       - | 2258 | `/*` |
|       - | 2259 | ` * __CLASS__` |
|       - | 2260 | ` *  The current class name, or the EMPTY STRING outside any class — php answers "",` |
|       - | 2261 | `` *  not null (`__CLASS__ === ""` is true in global scope). `self` keeps its own`` |
|       - | 2262 | ` *  expander below because php treats IT differently outside a class scope.` |
|       - | 2263 | ` */` |
|      78 | 2264 | `static void PH7_class_magic_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2265 | `{` |
|      81 | 2266 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 2267 | `	ph7_class *pClass;` |
|       - | 2268 | `	/* php flattens a trait into the class that used it, so __CLASS__ inside a trait method` |
|       - | 2269 | `	 * is THAT class (where __TRAIT__ and __METHOD__ stay the trait's — php's own asymmetry). */` |
|      81 | 2270 | `	pClass = PH7_VmPeekSelfClass(pVm);` |
|      81 | 2271 | `	if( pClass == 0 ){` |
|      67 | 2272 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      32 | 2273 | `	}` |
|      81 | 2274 | `	if( pClass ){` |
|      15 | 2275 | `		SyString *pName = &pClass->sName;` |
|      15 | 2276 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|       8 | 2277 | `	}else{` |
|      67 | 2278 | `		ph7_value_string(pVal,"",0);` |
|       - | 2279 | `	}` |
|      81 | 2280 | `}` |
|       - | 2281 |  |
|       - | 2282 | `/*` |
|       - | 2283 | ` * PASSWORD_BCRYPT / PASSWORD_DEFAULT` |
|       - | 2284 | ` *  The bcrypt algorithm identifier (PHP 7.4+ exposes these as the string "2y").` |
|       - | 2285 | ` *  PASSWORD_DEFAULT tracks the recommended default, currently bcrypt.` |
|       - | 2286 | ` */` |
|     162 | 2287 | `static void PH7_PASSWORD_BCRYPT_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2288 | `{` |
|      81 | 2289 | `	SXUNUSED(pUnused);` |
|     165 | 2290 | `	ph7_value_string(pVal,"2y",(int)sizeof("2y")-1);` |
|     165 | 2291 | `}` |
|       - | 2292 | `/*` |
|       - | 2293 | ` * PASSWORD_BCRYPT_DEFAULT_COST` |
|       - | 2294 | ` *  The default bcrypt work factor used by password_hash() (currently 12).` |
|       - | 2295 | ` */` |
|      64 | 2296 | `static void PH7_PASSWORD_COST_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2297 | `{` |
|      32 | 2298 | `	SXUNUSED(pUnused);` |
|      67 | 2299 | `	ph7_value_int(pVal,12);` |
|      67 | 2300 | `}` |
|       - | 2301 | `/*` |
|       - | 2302 | ` * PASSWORD_ARGON2I / PASSWORD_ARGON2ID and the three argon2 option defaults` |
|       - | 2303 | ` * password_hash() reads when $options omits them.` |
|       - | 2304 | ` */` |
|      68 | 2305 | `static void PH7_PASSWORD_ARGON2I_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2306 | `{` |
|      34 | 2307 | `	SXUNUSED(pUnused);` |
|      71 | 2308 | `	ph7_value_string(pVal,"argon2i",(int)sizeof("argon2i")-1);` |
|      71 | 2309 | `}` |
|      86 | 2310 | `static void PH7_PASSWORD_ARGON2ID_Const(ph7_value *pVal,void *pUnused)` |
|       4 | 2311 | `{` |
|      43 | 2312 | `	SXUNUSED(pUnused);` |
|      90 | 2313 | `	ph7_value_string(pVal,"argon2id",(int)sizeof("argon2id")-1);` |
|      90 | 2314 | `}` |
|      64 | 2315 | `static void PH7_ARGON2_MEM_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2316 | `{` |
|      32 | 2317 | `	SXUNUSED(pUnused);` |
|      67 | 2318 | `	ph7_value_int(pVal,65536);` |
|      67 | 2319 | `}` |
|      64 | 2320 | `static void PH7_ARGON2_TIME_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2321 | `{` |
|      32 | 2322 | `	SXUNUSED(pUnused);` |
|      67 | 2323 | `	ph7_value_int(pVal,4);` |
|      67 | 2324 | `}` |
|      64 | 2325 | `static void PH7_ARGON2_THREADS_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2326 | `{` |
|      32 | 2327 | `	SXUNUSED(pUnused);` |
|      67 | 2328 | `	ph7_value_int(pVal,1);` |
|      67 | 2329 | `}` |
|       - | 2330 | `/*` |
|       - | 2331 | ` * CRYPT_* — the crypt() capability flags. Every scheme is compiled in, so all` |
|       - | 2332 | ` * six are 1, and CRYPT_SALT_LENGTH is php's 123 (the longest setting string a` |
|       - | 2333 | ` * SHA-512-crypt with an explicit rounds count can need).` |
|       - | 2334 | ` */` |
|       - | 2335 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|     384 | 2336 | `static void PH7_CRYPT_ONE_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2337 | `{` |
|     192 | 2338 | `	SXUNUSED(pUnused);` |
|     387 | 2339 | `	ph7_value_int(pVal,1);` |
|     387 | 2340 | `}` |
|      64 | 2341 | `static void PH7_CRYPT_SALT_LENGTH_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2342 | `{` |
|      32 | 2343 | `	SXUNUSED(pUnused);` |
|      67 | 2344 | `	ph7_value_int(pVal,123);` |
|      67 | 2345 | `}` |
|       - | 2346 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|       - | 2347 | `/*` |
|       - | 2348 | ` * filter_var() filter and flag identifiers (the ext/filter constants). Values` |
|       - | 2349 | ` * match PHP 8.5. One tiny int-returning callback per constant, generated by a` |
|       - | 2350 | ` * local macro to keep the ~25 near-identical definitions DRY.` |
|       - | 2351 | ` */` |
|       - | 2352 | `#define PH7_FILTER_INT_CONST(Name,Val) \` |
|       - | 2353 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|       - | 2354 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|       - | 2355 | `	}` |
|      84 | 2356 | `PH7_FILTER_INT_CONST(FILTER_DEFAULT,516)` |
|      81 | 2357 | `PH7_FILTER_INT_CONST(FILTER_UNSAFE_RAW,516)` |
|     226 | 2358 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_INT,257)` |
|     162 | 2359 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_BOOLEAN,258)` |
|     199 | 2360 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_FLOAT,259)` |
|      71 | 2361 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_REGEXP,272)` |
|     210 | 2362 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_DOMAIN,277)` |
|     385 | 2363 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_URL,273)` |
|     266 | 2364 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_EMAIL,274)` |
|     628 | 2365 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_IP,275)` |
|     100 | 2366 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_MAC,276)` |
|      91 | 2367 | `PH7_FILTER_INT_CONST(FILTER_CALLBACK,1024)` |
|      93 | 2368 | `PH7_FILTER_INT_CONST(FILTER_THROW_ON_FAILURE,268435456)` |
|      74 | 2369 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_ENCODED,514)` |
|      72 | 2370 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_ADD_SLASHES,523)` |
|      69 | 2371 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_INT,519)` |
|      69 | 2372 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_FLOAT,520)` |
|      80 | 2373 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_SPECIAL_CHARS,515)` |
|      89 | 2374 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_FULL_SPECIAL_CHARS,522)` |
|      67 | 2375 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_EMAIL,517)` |
|      67 | 2376 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_URL,518)` |
|      67 | 2377 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_OCTAL,1)` |
|      67 | 2378 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_HEX,2)` |
|      76 | 2379 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_LOW,4)` |
|      72 | 2380 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_HIGH,8)` |
|      72 | 2381 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_LOW,16)` |
|      69 | 2382 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_HIGH,32)` |
|      70 | 2383 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_AMP,64)` |
|      72 | 2384 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_ENCODE_QUOTES,128)` |
|      68 | 2385 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NONE,0)` |
|      76 | 2386 | `PH7_FILTER_INT_CONST(FILTER_FLAG_EMPTY_STRING_NULL,256)` |
|      70 | 2387 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_BACKTICK,512)` |
|      67 | 2388 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_FRACTION,4096)` |
|     101 | 2389 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_THOUSAND,8192)` |
|      67 | 2390 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_SCIENTIFIC,16384)` |
|      74 | 2391 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV4,1048576)` |
|      70 | 2392 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV6,2097152)` |
|     217 | 2393 | `PH7_FILTER_INT_CONST(FILTER_FLAG_PATH_REQUIRED,262144)` |
|     217 | 2394 | `PH7_FILTER_INT_CONST(FILTER_FLAG_QUERY_REQUIRED,524288)` |
|     130 | 2395 | `PH7_FILTER_INT_CONST(FILTER_FLAG_HOSTNAME,1048576)` |
|     149 | 2396 | `PH7_FILTER_INT_CONST(FILTER_FLAG_EMAIL_UNICODE,1048576)` |
|     278 | 2397 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_RES_RANGE,4194304)` |
|     282 | 2398 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_PRIV_RANGE,8388608)` |
|     174 | 2399 | `PH7_FILTER_INT_CONST(FILTER_FLAG_GLOBAL_RANGE,536870912)` |
|      92 | 2400 | `PH7_FILTER_INT_CONST(FILTER_REQUIRE_ARRAY,16777216)` |
|      72 | 2401 | `PH7_FILTER_INT_CONST(FILTER_REQUIRE_SCALAR,33554432)` |
|      76 | 2402 | `PH7_FILTER_INT_CONST(FILTER_FORCE_ARRAY,67108864)` |
|      88 | 2403 | `PH7_FILTER_INT_CONST(FILTER_NULL_ON_FAILURE,134217728)` |
|       - | 2404 | `/* filter_input() source selectors (php values; SESSION/REQUEST are undefined in 8.5) */` |
|      69 | 2405 | `PH7_FILTER_INT_CONST(INPUT_POST,0)` |
|      79 | 2406 | `PH7_FILTER_INT_CONST(INPUT_GET,1)` |
|      67 | 2407 | `PH7_FILTER_INT_CONST(INPUT_COOKIE,2)` |
|      67 | 2408 | `PH7_FILTER_INT_CONST(INPUT_ENV,4)` |
|      85 | 2409 | `PH7_FILTER_INT_CONST(INPUT_SERVER,5)` |
|       - | 2410 | `/*` |
|       - | 2411 | ` * Table of built-in constants.` |
|       - | 2412 | ` */` |
|       - | 2413 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|       - | 2414 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|       - | 2415 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|       - | 2416 | `	{"__PH7__",              PH7_VER_Const      },` |
|       - | 2417 | `	{"PHP_VERSION",          PH7_PHPVerConst    },` |
|       - | 2418 | `	{"PHP_MAJOR_VERSION",    PH7_PHPMajorConst  },` |
|       - | 2419 | `	{"PHP_MINOR_VERSION",    PH7_PHPMinorConst  },` |
|       - | 2420 | `	{"PHP_RELEASE_VERSION",  PH7_PHPReleaseConst},` |
|       - | 2421 | `	{"PHP_EXTRA_VERSION",    PH7_PHPExtraConst  },` |
|       - | 2422 | `	{"PHP_VERSION_ID",       PH7_PHPVerIdConst  },` |
|       - | 2423 | `	{"PHP_OS",               PH7_OS_Const       },` |
|       - | 2424 | `	{"PHP_OS_FAMILY",        PH7_OS_FAMILY_Const},` |
|       - | 2425 | `	{"PHP_SAPI",             PH7_SAPI_Const     },` |
|       - | 2426 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|       - | 2427 | `	{"PHP_SESSION_DISABLED", PH7_PHP_SESSION_DISABLED_Const },` |
|       - | 2428 | `	{"PHP_SESSION_NONE",     PH7_PHP_SESSION_NONE_Const },` |
|       - | 2429 | `	{"PHP_SESSION_ACTIVE",   PH7_PHP_SESSION_ACTIVE_Const },` |
|       - | 2430 | `	{"INI_USER",             PH7_INI_USER_Const },` |
|       - | 2431 | `	{"INI_PERDIR",           PH7_INI_PERDIR_Const },` |
|       - | 2432 | `	{"INI_SYSTEM",           PH7_INI_SYSTEM_Const },` |
|       - | 2433 | `	{"INI_ALL",              PH7_INI_ALL_Const },` |
|       - | 2434 | `	{"MB_CASE_UPPER",        PH7_MB_CASE_UPPER_Const },` |
|       - | 2435 | `	{"MB_CASE_LOWER",        PH7_MB_CASE_LOWER_Const },` |
|       - | 2436 | `	{"MB_CASE_TITLE",        PH7_MB_CASE_TITLE_Const },` |
|       - | 2437 | `	{"PASSWORD_BCRYPT",      PH7_PASSWORD_BCRYPT_Const },` |
|       - | 2438 | `	{"PASSWORD_DEFAULT",     PH7_PASSWORD_BCRYPT_Const },` |
|       - | 2439 | `	{"PASSWORD_BCRYPT_DEFAULT_COST", PH7_PASSWORD_COST_Const },` |
|       - | 2440 | `	{"PASSWORD_ARGON2I",     PH7_PASSWORD_ARGON2I_Const },` |
|       - | 2441 | `	{"PASSWORD_ARGON2ID",    PH7_PASSWORD_ARGON2ID_Const },` |
|       - | 2442 | `	{"PASSWORD_ARGON2_DEFAULT_MEMORY_COST", PH7_ARGON2_MEM_Const },` |
|       - | 2443 | `	{"PASSWORD_ARGON2_DEFAULT_TIME_COST",   PH7_ARGON2_TIME_Const },` |
|       - | 2444 | `	{"PASSWORD_ARGON2_DEFAULT_THREADS",     PH7_ARGON2_THREADS_Const },` |
|       - | 2445 | `	{"FILTER_DEFAULT",              PH7_FILTER_DEFAULT_Const },` |
|       - | 2446 | `	{"FILTER_UNSAFE_RAW",           PH7_FILTER_UNSAFE_RAW_Const },` |
|       - | 2447 | `	{"FILTER_VALIDATE_INT",         PH7_FILTER_VALIDATE_INT_Const },` |
|       - | 2448 | `	{"FILTER_VALIDATE_BOOLEAN",     PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|       - | 2449 | `	{"FILTER_VALIDATE_BOOL",        PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|       - | 2450 | `	{"FILTER_VALIDATE_FLOAT",       PH7_FILTER_VALIDATE_FLOAT_Const },` |
|       - | 2451 | `	{"FILTER_VALIDATE_REGEXP",      PH7_FILTER_VALIDATE_REGEXP_Const },` |
|       - | 2452 | `	{"FILTER_VALIDATE_DOMAIN",      PH7_FILTER_VALIDATE_DOMAIN_Const },` |
|       - | 2453 | `	{"FILTER_VALIDATE_URL",         PH7_FILTER_VALIDATE_URL_Const },` |
|       - | 2454 | `	{"FILTER_VALIDATE_EMAIL",       PH7_FILTER_VALIDATE_EMAIL_Const },` |
|       - | 2455 | `	{"FILTER_VALIDATE_IP",          PH7_FILTER_VALIDATE_IP_Const },` |
|       - | 2456 | `	{"FILTER_VALIDATE_MAC",         PH7_FILTER_VALIDATE_MAC_Const },` |
|       - | 2457 | `	{"FILTER_CALLBACK",             PH7_FILTER_CALLBACK_Const },` |
|       - | 2458 | `	{"FILTER_THROW_ON_FAILURE",     PH7_FILTER_THROW_ON_FAILURE_Const },` |
|       - | 2459 | `	{"FILTER_SANITIZE_ENCODED",     PH7_FILTER_SANITIZE_ENCODED_Const },` |
|       - | 2460 | `	{"FILTER_SANITIZE_ADD_SLASHES", PH7_FILTER_SANITIZE_ADD_SLASHES_Const },` |
|       - | 2461 | `	{"FILTER_FLAG_NONE",            PH7_FILTER_FLAG_NONE_Const },` |
|       - | 2462 | `	{"FILTER_FLAG_EMPTY_STRING_NULL", PH7_FILTER_FLAG_EMPTY_STRING_NULL_Const },` |
|       - | 2463 | `	{"FILTER_SANITIZE_NUMBER_INT",  PH7_FILTER_SANITIZE_NUMBER_INT_Const },` |
|       - | 2464 | `	{"FILTER_SANITIZE_NUMBER_FLOAT",PH7_FILTER_SANITIZE_NUMBER_FLOAT_Const },` |
|       - | 2465 | `	{"FILTER_SANITIZE_SPECIAL_CHARS",PH7_FILTER_SANITIZE_SPECIAL_CHARS_Const },` |
|       - | 2466 | `	{"FILTER_SANITIZE_FULL_SPECIAL_CHARS",PH7_FILTER_SANITIZE_FULL_SPECIAL_CHARS_Const },` |
|       - | 2467 | `	{"FILTER_SANITIZE_EMAIL",       PH7_FILTER_SANITIZE_EMAIL_Const },` |
|       - | 2468 | `	{"FILTER_SANITIZE_URL",         PH7_FILTER_SANITIZE_URL_Const },` |
|       - | 2469 | `	{"FILTER_FLAG_ALLOW_OCTAL",     PH7_FILTER_FLAG_ALLOW_OCTAL_Const },` |
|       - | 2470 | `	{"FILTER_FLAG_ALLOW_HEX",       PH7_FILTER_FLAG_ALLOW_HEX_Const },` |
|       - | 2471 | `	{"FILTER_FLAG_STRIP_LOW",       PH7_FILTER_FLAG_STRIP_LOW_Const },` |
|       - | 2472 | `	{"FILTER_FLAG_STRIP_HIGH",      PH7_FILTER_FLAG_STRIP_HIGH_Const },` |
|       - | 2473 | `	{"FILTER_FLAG_ENCODE_LOW",      PH7_FILTER_FLAG_ENCODE_LOW_Const },` |
|       - | 2474 | `	{"FILTER_FLAG_ENCODE_HIGH",     PH7_FILTER_FLAG_ENCODE_HIGH_Const },` |
|       - | 2475 | `	{"FILTER_FLAG_ENCODE_AMP",      PH7_FILTER_FLAG_ENCODE_AMP_Const },` |
|       - | 2476 | `	{"FILTER_FLAG_NO_ENCODE_QUOTES",PH7_FILTER_FLAG_NO_ENCODE_QUOTES_Const },` |
|       - | 2477 | `	{"FILTER_FLAG_STRIP_BACKTICK",  PH7_FILTER_FLAG_STRIP_BACKTICK_Const },` |
|       - | 2478 | `	{"FILTER_FLAG_ALLOW_FRACTION",  PH7_FILTER_FLAG_ALLOW_FRACTION_Const },` |
|       - | 2479 | `	{"FILTER_FLAG_ALLOW_THOUSAND",  PH7_FILTER_FLAG_ALLOW_THOUSAND_Const },` |
|       - | 2480 | `	{"FILTER_FLAG_ALLOW_SCIENTIFIC",PH7_FILTER_FLAG_ALLOW_SCIENTIFIC_Const },` |
|       - | 2481 | `	{"FILTER_FLAG_IPV4",            PH7_FILTER_FLAG_IPV4_Const },` |
|       - | 2482 | `	{"FILTER_FLAG_IPV6",            PH7_FILTER_FLAG_IPV6_Const },` |
|       - | 2483 | `	{"FILTER_FLAG_PATH_REQUIRED",   PH7_FILTER_FLAG_PATH_REQUIRED_Const },` |
|       - | 2484 | `	{"FILTER_FLAG_QUERY_REQUIRED",  PH7_FILTER_FLAG_QUERY_REQUIRED_Const },` |
|       - | 2485 | `	{"FILTER_FLAG_HOSTNAME",        PH7_FILTER_FLAG_HOSTNAME_Const },` |
|       - | 2486 | `	{"FILTER_FLAG_EMAIL_UNICODE",   PH7_FILTER_FLAG_EMAIL_UNICODE_Const },` |
|       - | 2487 | `	{"FILTER_FLAG_NO_RES_RANGE",    PH7_FILTER_FLAG_NO_RES_RANGE_Const },` |
|       - | 2488 | `	{"FILTER_FLAG_NO_PRIV_RANGE",   PH7_FILTER_FLAG_NO_PRIV_RANGE_Const },` |
|       - | 2489 | `	{"FILTER_FLAG_GLOBAL_RANGE",    PH7_FILTER_FLAG_GLOBAL_RANGE_Const },` |
|       - | 2490 | `	{"FILTER_REQUIRE_ARRAY",        PH7_FILTER_REQUIRE_ARRAY_Const },` |
|       - | 2491 | `	{"FILTER_REQUIRE_SCALAR",       PH7_FILTER_REQUIRE_SCALAR_Const },` |
|       - | 2492 | `	{"FILTER_FORCE_ARRAY",          PH7_FILTER_FORCE_ARRAY_Const },` |
|       - | 2493 | `	{"FILTER_NULL_ON_FAILURE",      PH7_FILTER_NULL_ON_FAILURE_Const },` |
|       - | 2494 | `	{"INPUT_POST",                  PH7_INPUT_POST_Const },` |
|       - | 2495 | `	{"INPUT_GET",                   PH7_INPUT_GET_Const },` |
|       - | 2496 | `	{"INPUT_COOKIE",                PH7_INPUT_COOKIE_Const },` |
|       - | 2497 | `	{"INPUT_ENV",                   PH7_INPUT_ENV_Const },` |
|       - | 2498 | `	{"INPUT_SERVER",                PH7_INPUT_SERVER_Const },` |
|       - | 2499 | `	{"CAL_GREGORIAN",        PH7_CAL_GREGORIAN_Const },` |
|       - | 2500 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|       - | 2501 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|       - | 2502 | `	{"PHP_INT_MIN",          PH7_INTMIN_Const   },` |
|       - | 2503 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|       - | 2504 | `	{"PHP_FLOAT_EPSILON",    PH7_FLOATEPSILON_Const },` |
|       - | 2505 | `	{"PHP_FLOAT_MAX",        PH7_FLOATMAX_Const },` |
|       - | 2506 | `	{"PHP_FLOAT_MIN",        PH7_FLOATMIN_Const },` |
|       - | 2507 | `	{"PHP_FLOAT_DIG",        PH7_FLOATDIG_Const },` |
|       - | 2508 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|       - | 2509 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|       - | 2510 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|       - | 2511 | `	{"__TIME__",             PH7_TIME_Const     },` |
|       - | 2512 | `	{"__DATE__",             PH7_DATE_Const     },` |
|       - | 2513 | `	{"__FILE__",             PH7_FILE_Const     },` |
|       - | 2514 | `	{"__DIR__",              PH7_DIR_Const      },` |
|       - | 2515 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|       - | 2516 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|       - | 2517 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|       - | 2518 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|       - | 2519 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|       - | 2520 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|       - | 2521 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|       - | 2522 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|       - | 2523 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|       - | 2524 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|       - | 2525 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|       - | 2526 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|       - | 2527 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|       - | 2528 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|       - | 2529 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|       - | 2530 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|       - | 2531 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|       - | 2532 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|       - | 2533 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|       - | 2534 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|       - | 2535 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|       - | 2536 | `	{"STREAM_IS_URL",                PH7_STREAM_IS_URL_Const },` |
|       - | 2537 | `	{"STREAM_SERVER_BIND",           PH7_STREAM_SERVER_BIND_Const },` |
|       - | 2538 | `	{"STREAM_SERVER_LISTEN",         PH7_STREAM_SERVER_LISTEN_Const },` |
|       - | 2539 | `	{"STREAM_CLIENT_CONNECT",        PH7_STREAM_CLIENT_CONNECT_Const },` |
|       - | 2540 | `	{"STREAM_CLIENT_ASYNC_CONNECT",  PH7_STREAM_CLIENT_ASYNC_CONNECT_Const },` |
|       - | 2541 | `	{"STREAM_CLIENT_PERSISTENT",     PH7_STREAM_CLIENT_PERSISTENT_Const },` |
|       - | 2542 | `	{"PSFS_PASS_ON",                 PH7_PSFS_PASS_ON_Const },` |
|       - | 2543 | `	{"PSFS_FEED_ME",                 PH7_PSFS_FEED_ME_Const },` |
|       - | 2544 | `	{"PSFS_ERR_FATAL",               PH7_PSFS_ERR_FATAL_Const },` |
|       - | 2545 | `	{"PSFS_FLAG_NORMAL",             PH7_PSFS_FLAG_NORMAL_Const },` |
|       - | 2546 | `	{"PSFS_FLAG_FLUSH_INC",          PH7_PSFS_FLAG_FLUSH_INC_Const },` |
|       - | 2547 | `	{"PSFS_FLAG_FLUSH_CLOSE",        PH7_PSFS_FLAG_FLUSH_CLOSE_Const },` |
|       - | 2548 | `	{"STREAM_FILTER_READ",           PH7_STREAM_FILTER_READ_Const },` |
|       - | 2549 | `	{"STREAM_FILTER_WRITE",          PH7_STREAM_FILTER_WRITE_Const },` |
|       - | 2550 | `	{"STREAM_FILTER_ALL",            PH7_STREAM_FILTER_ALL_Const },` |
|       - | 2551 | `	{"STREAM_SHUT_RD",               PH7_STREAM_SHUT_RD_Const },` |
|       - | 2552 | `	{"STREAM_SHUT_WR",               PH7_STREAM_SHUT_WR_Const },` |
|       - | 2553 | `	{"STREAM_SHUT_RDWR",             PH7_STREAM_SHUT_RDWR_Const },` |
|       - | 2554 | `	{"STREAM_OOB",                   PH7_STREAM_OOB_Const },` |
|       - | 2555 | `	{"STREAM_PEEK",                  PH7_STREAM_PEEK_Const },` |
|       - | 2556 | `#ifdef PH7_ENABLE_NET` |
|       - | 2557 | `	{"STREAM_PF_INET",               PH7_STREAM_PF_INET_Const },` |
|       - | 2558 | `	{"STREAM_PF_INET6",              PH7_STREAM_PF_INET6_Const },` |
|       - | 2559 | `	{"STREAM_PF_UNIX",               PH7_STREAM_PF_UNIX_Const },` |
|       - | 2560 | `	{"STREAM_SOCK_STREAM",           PH7_STREAM_SOCK_STREAM_Const },` |
|       - | 2561 | `	{"STREAM_SOCK_DGRAM",            PH7_STREAM_SOCK_DGRAM_Const },` |
|       - | 2562 | `	{"STREAM_SOCK_RAW",              PH7_STREAM_SOCK_RAW_Const },` |
|       - | 2563 | `	{"STREAM_SOCK_SEQPACKET",        PH7_STREAM_SOCK_SEQPACKET_Const },` |
|       - | 2564 | `	{"STREAM_SOCK_RDM",              PH7_STREAM_SOCK_RDM_Const },` |
|       - | 2565 | `	{"STREAM_IPPROTO_IP",            PH7_STREAM_IPPROTO_IP_Const },` |
|       - | 2566 | `	{"STREAM_IPPROTO_TCP",           PH7_STREAM_IPPROTO_TCP_Const },` |
|       - | 2567 | `	{"STREAM_IPPROTO_UDP",           PH7_STREAM_IPPROTO_UDP_Const },` |
|       - | 2568 | `	{"STREAM_IPPROTO_ICMP",          PH7_STREAM_IPPROTO_ICMP_Const },` |
|       - | 2569 | `	{"STREAM_IPPROTO_RAW",           PH7_STREAM_IPPROTO_RAW_Const },` |
|       - | 2570 | `#endif` |
|       - | 2571 | `	{"MT_RAND_MT19937",              PH7_MT_RAND_MT19937_Const },` |
|       - | 2572 | `	{"MT_RAND_PHP",                  PH7_MT_RAND_PHP_Const  },` |
|       - | 2573 | `	{"PHP_OUTPUT_HANDLER_WRITE",     PH7_OB_WRITE_Const     },` |
|       - | 2574 | `	{"PHP_OUTPUT_HANDLER_CONT",      PH7_OB_WRITE_Const     },` |
|       - | 2575 | `	{"PHP_OUTPUT_HANDLER_START",     PH7_OB_START_Const     },` |
|       - | 2576 | `	{"PHP_OUTPUT_HANDLER_CLEAN",     PH7_OB_CLEAN_Const     },` |
|       - | 2577 | `	{"PHP_OUTPUT_HANDLER_FLUSH",     PH7_OB_FLUSH_Const     },` |
|       - | 2578 | `	{"PHP_OUTPUT_HANDLER_FINAL",     PH7_OB_FINAL_Const     },` |
|       - | 2579 | `	{"PHP_OUTPUT_HANDLER_END",       PH7_OB_FINAL_Const     },` |
|       - | 2580 | `	{"PHP_OUTPUT_HANDLER_CLEANABLE", PH7_OB_CLEANABLE_Const },` |
|       - | 2581 | `	{"PHP_OUTPUT_HANDLER_FLUSHABLE", PH7_OB_FLUSHABLE_Const },` |
|       - | 2582 | `	{"PHP_OUTPUT_HANDLER_REMOVABLE", PH7_OB_REMOVABLE_Const },` |
|       - | 2583 | `	{"PHP_OUTPUT_HANDLER_STDFLAGS",  PH7_OB_STDFLAGS_Const  },` |
|       - | 2584 | `	{"PHP_OUTPUT_HANDLER_STARTED",   PH7_OB_STARTED_Const   },` |
|       - | 2585 | `	{"PHP_OUTPUT_HANDLER_DISABLED",  PH7_OB_DISABLED_Const  },` |
|       - | 2586 | `	{"PHP_OUTPUT_HANDLER_PROCESSED", PH7_OB_PROCESSED_Const },` |
|       - | 2587 | `	{"ARRAY_FILTER_USE_KEY", PH7_ARRAY_FILTER_USE_KEY_Const },` |
|       - | 2588 | `	{"ARRAY_FILTER_USE_BOTH",PH7_ARRAY_FILTER_USE_BOTH_Const},` |
|       - | 2589 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|       - | 2590 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|       - | 2591 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|       - | 2592 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|       - | 2593 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|       - | 2594 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|       - | 2595 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|       - | 2596 | `	{"SORT_LOCALE_STRING",   PH7_SORT_LOCALE_STRING_Const },` |
|       - | 2597 | `	{"SORT_NATURAL",         PH7_SORT_NATURAL_Const },` |
|       - | 2598 | `	{"SORT_FLAG_CASE",       PH7_SORT_FLAG_CASE_Const },` |
|       - | 2599 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|       - | 2600 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|       - | 2601 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|       - | 2602 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|       - | 2603 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|       - | 2604 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|       - | 2605 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|       - | 2606 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|       - | 2607 | `	{"M_E",                  PH7_M_E_Const          },` |
|       - | 2608 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|       - | 2609 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|       - | 2610 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|       - | 2611 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|       - | 2612 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|       - | 2613 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|       - | 2614 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|       - | 2615 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|       - | 2616 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|       - | 2617 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|       - | 2618 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|       - | 2619 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|       - | 2620 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|       - | 2621 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|       - | 2622 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|       - | 2623 | `	{"NAN",                  PH7_NAN_Const          },` |
|       - | 2624 | `	{"INF",                  PH7_INF_Const          },` |
|       - | 2625 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|       - | 2626 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|       - | 2627 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|       - | 2628 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|       - | 2629 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|       - | 2630 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|       - | 2631 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|       - | 2632 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|       - | 2633 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|       - | 2634 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|       - | 2635 | `	{"DATE_RFC3339_EXTENDED",PH7_DATE_RFC3339_EXTENDED_Const },` |
|       - | 2636 | `	{"DATE_RFC7231",         PH7_DATE_RFC7231_Const },` |
|       - | 2637 | `	{"DATE_ISO8601_EXPANDED",PH7_DATE_ISO8601_EXPANDED_Const },` |
|       - | 2638 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|       - | 2639 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|       - | 2640 | `	{"FILE_TEXT",            PH7_FILE_TEXT_Const    },` |
|       - | 2641 | `	{"FILE_BINARY",          PH7_FILE_TEXT_Const    },` |
|       - | 2642 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|       - | 2643 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|       - | 2644 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|       - | 2645 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|       - | 2646 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|       - | 2647 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|       - | 2648 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|       - | 2649 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|       - | 2650 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|       - | 2651 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|       - | 2652 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|       - | 2653 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|       - | 2654 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|       - | 2655 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|       - | 2656 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|       - | 2657 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|       - | 2658 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|       - | 2659 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|       - | 2660 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|       - | 2661 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|       - | 2662 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|       - | 2663 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|       - | 2664 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|       - | 2665 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|       - | 2666 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|       - | 2667 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|       - | 2668 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|       - | 2669 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|       - | 2670 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|       - | 2671 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|       - | 2672 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|       - | 2673 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|       - | 2674 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|       - | 2675 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|       - | 2676 | `	{"PATHINFO_ALL",         PH7_PATHINFO_ALL_Const },` |
|       - | 2677 | `	/* ASSERT_QUIET_EVAL was REMOVED in php 8.0: referencing it is an Error there */` |
|       - | 2678 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|       - | 2679 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|       - | 2680 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|       - | 2681 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|       - | 2682 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|       - | 2683 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|       - | 2684 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|       - | 2685 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|       - | 2686 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|       - | 2687 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|       - | 2688 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|       - | 2689 | `	{"FILE_NO_DEFAULT_CONTEXT", PH7_FILE_NO_DEFAULT_CONTEXT_Const },` |
|       - | 2690 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|       - | 2691 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|       - | 2692 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|       - | 2693 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|       - | 2694 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|       - | 2695 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|       - | 2696 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|       - | 2697 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|       - | 2698 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|       - | 2699 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|       - | 2700 | `	{"GLOB_AVAILABLE_FLAGS", PH7_GLOB_AVAILABLE_FLAGS_Const },` |
|       - | 2701 | `	{"STDIN",                PH7_STDIN_Const        },` |
|       - | 2702 | `	{"stdin",                PH7_STDIN_Const        },` |
|       - | 2703 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|       - | 2704 | `	{"stdout",               PH7_STDOUT_Const       },` |
|       - | 2705 | `	{"STDERR",               PH7_STDERR_Const       },` |
|       - | 2706 | `	{"stderr",               PH7_STDERR_Const       },` |
|       - | 2707 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|       - | 2708 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|       - | 2709 | `	{"INI_SCANNER_TYPED",    PH7_INI_SCANNER_TYPED_Const  },` |
|       - | 2710 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|       - | 2711 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|       - | 2712 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|       - | 2713 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|       - | 2714 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|       - | 2715 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|       - | 2716 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|       - | 2717 | `	{"EXTR_REFS",            PH7_EXTR_REFS_Const        },` |
|       - | 2718 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|       - | 2719 | `	{"HASH_HMAC",              PH7_HASH_HMAC_Const},` |
|       - | 2720 | `	{"CRYPT_SALT_LENGTH",      PH7_CRYPT_SALT_LENGTH_Const},` |
|       - | 2721 | `	{"CRYPT_STD_DES",          PH7_CRYPT_ONE_Const},` |
|       - | 2722 | `	{"CRYPT_EXT_DES",          PH7_CRYPT_ONE_Const},` |
|       - | 2723 | `	{"CRYPT_MD5",              PH7_CRYPT_ONE_Const},` |
|       - | 2724 | `	{"CRYPT_BLOWFISH",         PH7_CRYPT_ONE_Const},` |
|       - | 2725 | `	{"CRYPT_SHA256",           PH7_CRYPT_ONE_Const},` |
|       - | 2726 | `	{"CRYPT_SHA512",           PH7_CRYPT_ONE_Const},` |
|       - | 2727 | `#endif` |
|       - | 2728 | `	{"ICONV_IMPL",             PH7_ICONV_IMPL_Const},` |
|       - | 2729 | `	{"ICONV_VERSION",          PH7_ICONV_VERSION_Const},` |
|       - | 2730 | `	{"ICONV_MIME_DECODE_STRICT", PH7_ICONV_MIME_DECODE_STRICT_Const},` |
|       - | 2731 | `	{"ICONV_MIME_DECODE_CONTINUE_ON_ERROR", PH7_ICONV_MIME_DECODE_CONTINUE_ON_ERROR_Const},` |
|       - | 2732 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|       - | 2733 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|       - | 2734 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|       - | 2735 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|       - | 2736 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|       - | 2737 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|       - | 2738 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|       - | 2739 | `	{"JSON_OBJECT_AS_ARRAY",   PH7_JSON_OBJECT_AS_ARRAY_Const},` |
|       - | 2740 | `	{"JSON_PARTIAL_OUTPUT_ON_ERROR", PH7_JSON_PARTIAL_OUTPUT_ON_ERROR_Const},` |
|       - | 2741 | `	{"JSON_PRESERVE_ZERO_FRACTION",  PH7_JSON_PRESERVE_ZERO_FRACTION_Const},` |
|       - | 2742 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|       - | 2743 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|       - | 2744 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|       - | 2745 | `	{"JSON_UNESCAPED_LINE_TERMINATORS", PH7_JSON_UNESCAPED_LINE_TERMINATORS_Const},` |
|       - | 2746 | `	{"JSON_INVALID_UTF8_IGNORE", PH7_JSON_INVALID_UTF8_IGNORE_Const},` |
|       - | 2747 | `	{"JSON_INVALID_UTF8_SUBSTITUTE", PH7_JSON_INVALID_UTF8_SUBSTITUTE_Const},` |
|       - | 2748 | `	{"JSON_THROW_ON_ERROR",    PH7_JSON_THROW_ON_ERROR_Const},` |
|       - | 2749 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|       - | 2750 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|       - | 2751 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|       - | 2752 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|       - | 2753 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|       - | 2754 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|       - | 2755 | `	{"JSON_ERROR_RECURSION", PH7_JSON_ERROR_RECURSION_Const},` |
|       - | 2756 | `	{"JSON_ERROR_UNSUPPORTED_TYPE", PH7_JSON_ERROR_UNSUPPORTED_TYPE_Const},` |
|       - | 2757 | `	{"JSON_ERROR_INVALID_PROPERTY_NAME", PH7_JSON_ERROR_INVALID_PROPERTY_NAME_Const},` |
|       - | 2758 | `	{"JSON_ERROR_UTF16",     PH7_JSON_ERROR_UTF16_Const},` |
|       - | 2759 | `	{"JSON_ERROR_NON_BACKED_ENUM", PH7_JSON_ERROR_NON_BACKED_ENUM_Const},` |
|       - | 2760 | `	{"JSON_ERROR_INF_OR_NAN", PH7_JSON_ERROR_INF_OR_NAN_Const},` |
|       - | 2761 | ``	/* `self`, `parent` and `static` are KEYWORDS in php, not constants: using one as a bare`` |
|       - | 2762 | ``	 * word is an "Undefined constant" Error (or a parse error for `static`). PH7 registered`` |
|       - | 2763 | `	 * them as constants that quietly expanded to the class name / NULL, so a typo'd bare` |
|       - | 2764 | ``	 * word silently produced a value. The `self::`/`parent::`/`static::` forms are handled`` |
|       - | 2765 | ``	 * by the `::` compile path and do not go through the constant table. */`` |
|       - | 2766 | `	{"__CLASS__",            PH7_class_magic_Const  }` |
|       - | 2767 | `};` |
|       - | 2768 | `/*` |
|       - | 2769 | ` * Register the built-in constants defined above.` |
|       - | 2770 | ` */` |
|    4660 | 2771 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|       5 | 2772 | `{` |
|       - | 2773 | `	sxu32 n;` |
|       - | 2774 | `	/*` |
|       - | 2775 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|       - | 2776 | `	 * that trigger the constant invocation as their private data.` |
|       - | 2777 | `	 */` |
| 1593725 | 2778 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 1589065 | 2779 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
|  794535 | 2780 | `	}` |
|    4665 | 2781 | `}` |
