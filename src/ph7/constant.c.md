# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1545/1561 lines (98.98%)

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
|    5526 |   63 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|       5 |   64 | `{` |
|       - |   65 | `#if defined(__WINNT__)` |
|       5 |   66 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|       - |   67 | `#elif defined(__UNIXES__)` |
|       - |   68 | `	struct utsname sInfo;` |
|    5526 |   69 | `	if( uname(&sInfo) != 0 ){` |
|     ! 0 |   70 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|     ! 0 |   71 | `	}else{` |
|    5526 |   72 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|       - |   73 | `	}` |
|       - |   74 | `#else` |
|       - |   75 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|       - |   76 | `#endif` |
|    2763 |   77 | `	SXUNUSED(pUnused);` |
|    5531 |   78 | `}` |
|       - |   79 | `/*` |
|       - |   80 | ` * PHP_OS_FAMILY (php 7.2)` |
|       - |   81 | ` *  One of 'Windows', 'BSD', 'Darwin', 'Solaris', 'Linux' or 'Unknown', derived` |
|       - |   82 | ` *  from the host's uname sysname (php maps the same set at build time).` |
|       - |   83 | ` */` |
|      88 |   84 | `static void PH7_OS_FAMILY_Const(ph7_value *pVal,void *pUnused)` |
|       5 |   85 | `{` |
|      44 |   86 | `	SXUNUSED(pUnused);` |
|       - |   87 | `#if defined(__WINNT__)` |
|       5 |   88 | `	ph7_value_string(pVal,"Windows",(int)sizeof("Windows")-1);` |
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
|      93 |  116 | `}` |
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
|       5 |  134 | `{` |
|     449 |  135 | `	SXUNUSED(pUnused);` |
|       - |  136 | `#ifdef __WINNT__` |
|       5 |  137 | `	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);` |
|       - |  138 | `#else` |
|     898 |  139 | `	ph7_value_string(pVal,"\n",(int)sizeof(char));` |
|       - |  140 | `#endif` |
|     903 |  141 | `}` |
|       - |  142 | `/*` |
|       - |  143 | ` * PHP_INT_MAX` |
|       - |  144 | ` * Expand the largest integer supported.` |
|       - |  145 | ` * Note that PH7 deals with 64-bit integer for all platforms.` |
|       - |  146 | ` */` |
|    1776 |  147 | `static void PH7_INTMAX_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  148 | `{` |
|     888 |  149 | `	SXUNUSED(pUnused);` |
|    1781 |  150 | `	ph7_value_int64(pVal,SXI64_HIGH);` |
|    1781 |  151 | `}` |
|       - |  152 | `/*` |
|       - |  153 | ` * ext/calendar: the four calendars, numbered in the order the conversion table` |
|       - |  154 | ` * holds them, and CAL_NUM_CALS as their count -- which is what makes the` |
|       - |  155 | ` * "valid calendar ID" screen a plain 0 <= id < CAL_NUM_CALS test.` |
|       - |  156 | ` */` |
|     110 |  157 | `static void PH7_CAL_GREGORIAN_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  158 | `{` |
|      55 |  159 | `	SXUNUSED(pUnused);` |
|     114 |  160 | `	ph7_value_int(pVal,0);` |
|     114 |  161 | `}` |
|      80 |  162 | `static void PH7_CAL_JULIAN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  163 | `{` |
|      40 |  164 | `	SXUNUSED(pUnused);` |
|      83 |  165 | `	ph7_value_int(pVal,1);` |
|      83 |  166 | `}` |
|     198 |  167 | `static void PH7_CAL_JEWISH_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  168 | `{` |
|      99 |  169 | `	SXUNUSED(pUnused);` |
|     201 |  170 | `	ph7_value_int(pVal,2);` |
|     201 |  171 | `}` |
|      90 |  172 | `static void PH7_CAL_FRENCH_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  173 | `{` |
|      45 |  174 | `	SXUNUSED(pUnused);` |
|      93 |  175 | `	ph7_value_int(pVal,3);` |
|      93 |  176 | `}` |
|      64 |  177 | `static void PH7_CAL_NUM_CALS_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  178 | `{` |
|      32 |  179 | `	SXUNUSED(pUnused);` |
|      67 |  180 | `	ph7_value_int(pVal,4);` |
|      67 |  181 | `}` |
|       - |  182 | `/*` |
|       - |  183 | ` * easter_days()/easter_date()'s $mode. DEFAULT is not a rule but a` |
|       - |  184 | ` * date-dependent choice between the two below it; ROMAN moves the 1583-1752` |
|       - |  185 | ` * window to the Gregorian rule, and the two ALWAYS_ modes pin one rule for` |
|       - |  186 | ` * every year.` |
|       - |  187 | ` */` |
|      84 |  188 | `static void PH7_CAL_EASTER_DEFAULT_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  189 | `{` |
|      42 |  190 | `	SXUNUSED(pUnused);` |
|      87 |  191 | `	ph7_value_int(pVal,0);` |
|      87 |  192 | `}` |
|      84 |  193 | `static void PH7_CAL_EASTER_ROMAN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  194 | `{` |
|      42 |  195 | `	SXUNUSED(pUnused);` |
|      87 |  196 | `	ph7_value_int(pVal,1);` |
|      87 |  197 | `}` |
|      84 |  198 | `static void PH7_CAL_EASTER_ALWAYS_GREGORIAN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  199 | `{` |
|      42 |  200 | `	SXUNUSED(pUnused);` |
|      87 |  201 | `	ph7_value_int(pVal,2);` |
|      87 |  202 | `}` |
|      88 |  203 | `static void PH7_CAL_EASTER_ALWAYS_JULIAN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  204 | `{` |
|      44 |  205 | `	SXUNUSED(pUnused);` |
|      91 |  206 | `	ph7_value_int(pVal,3);` |
|      91 |  207 | `}` |
|       - |  208 | `/* jddayofweek()'s three modes. Note that SHORT is 2 and LONG is 1: the numbers` |
|       - |  209 | ` * are not in the order the names suggest. */` |
|      64 |  210 | `static void PH7_CAL_DOW_DAYNO_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  211 | `{` |
|      32 |  212 | `	SXUNUSED(pUnused);` |
|      67 |  213 | `	ph7_value_int(pVal,0);` |
|      67 |  214 | `}` |
|     100 |  215 | `static void PH7_CAL_DOW_LONG_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  216 | `{` |
|      50 |  217 | `	SXUNUSED(pUnused);` |
|     103 |  218 | `	ph7_value_int(pVal,1);` |
|     103 |  219 | `}` |
|      90 |  220 | `static void PH7_CAL_DOW_SHORT_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  221 | `{` |
|      45 |  222 | `	SXUNUSED(pUnused);` |
|      93 |  223 | `	ph7_value_int(pVal,2);` |
|      93 |  224 | `}` |
|       - |  225 | `/* jdmonthname()'s six modes, which pick the CALENDAR as well as the spelling` |
|       - |  226 | ` * and are numbered independently of the CAL_* calendar ids above. */` |
|      66 |  227 | `static void PH7_CAL_MONTH_GREGORIAN_SHORT_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  228 | `{` |
|      33 |  229 | `	SXUNUSED(pUnused);` |
|      69 |  230 | `	ph7_value_int(pVal,0);` |
|      69 |  231 | `}` |
|      88 |  232 | `static void PH7_CAL_MONTH_GREGORIAN_LONG_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  233 | `{` |
|      44 |  234 | `	SXUNUSED(pUnused);` |
|      91 |  235 | `	ph7_value_int(pVal,1);` |
|      91 |  236 | `}` |
|      66 |  237 | `static void PH7_CAL_MONTH_JULIAN_SHORT_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  238 | `{` |
|      33 |  239 | `	SXUNUSED(pUnused);` |
|      69 |  240 | `	ph7_value_int(pVal,2);` |
|      69 |  241 | `}` |
|      66 |  242 | `static void PH7_CAL_MONTH_JULIAN_LONG_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  243 | `{` |
|      33 |  244 | `	SXUNUSED(pUnused);` |
|      69 |  245 | `	ph7_value_int(pVal,3);` |
|      69 |  246 | `}` |
|      72 |  247 | `static void PH7_CAL_MONTH_JEWISH_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  248 | `{` |
|      36 |  249 | `	SXUNUSED(pUnused);` |
|      75 |  250 | `	ph7_value_int(pVal,4);` |
|      75 |  251 | `}` |
|      66 |  252 | `static void PH7_CAL_MONTH_FRENCH_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  253 | `{` |
|      33 |  254 | `	SXUNUSED(pUnused);` |
|      69 |  255 | `	ph7_value_int(pVal,5);` |
|      69 |  256 | `}` |
|       - |  257 | `/*` |
|       - |  258 | ` * ext/calendar: the three flags jdtojewish()'s Hebrew spelling reads. They are` |
|       - |  259 | ` * a bit set, so a caller may ask for any combination of them.` |
|       - |  260 | ` */` |
|      68 |  261 | `static void PH7_CAL_JEWISH_ADD_ALAFIM_GERESH_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  262 | `{` |
|      34 |  263 | `	SXUNUSED(pUnused);` |
|      71 |  264 | `	ph7_value_int(pVal,2);` |
|      71 |  265 | `}` |
|      68 |  266 | `static void PH7_CAL_JEWISH_ADD_ALAFIM_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  267 | `{` |
|      34 |  268 | `	SXUNUSED(pUnused);` |
|      71 |  269 | `	ph7_value_int(pVal,4);` |
|      71 |  270 | `}` |
|      70 |  271 | `static void PH7_CAL_JEWISH_ADD_GERESHAYIM_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  272 | `{` |
|      35 |  273 | `	SXUNUSED(pUnused);` |
|      73 |  274 | `	ph7_value_int(pVal,8);` |
|      73 |  275 | `}` |
|       - |  276 | `/*` |
|       - |  277 | ` * PHP_INT_MIN (php 7.0)` |
|       - |  278 | ` * Expand the smallest integer supported.` |
|       - |  279 | ` */` |
|     222 |  280 | `static void PH7_INTMIN_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  281 | `{` |
|     111 |  282 | `	SXUNUSED(pUnused);` |
|     226 |  283 | `	ph7_value_int64(pVal,SMALLEST_INT64);` |
|     226 |  284 | `}` |
|       - |  285 | `/*` |
|       - |  286 | ` * PHP_INT_SIZE` |
|       - |  287 | ` * Expand the size in bytes of a 64-bit integer.` |
|       - |  288 | ` */` |
|      66 |  289 | `static void PH7_INTSIZE_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  290 | `{` |
|      33 |  291 | `	SXUNUSED(pUnused);` |
|      69 |  292 | `	ph7_value_int64(pVal,sizeof(sxi64));` |
|      69 |  293 | `}` |
|       - |  294 | `/*` |
|       - |  295 | ` * PHP_FLOAT_EPSILON / PHP_FLOAT_MAX / PHP_FLOAT_MIN / PHP_FLOAT_DIG (php 7.2)` |
|       - |  296 | ` * Double-precision characteristics, sourced from <float.h> exactly like php` |
|       - |  297 | ` * so they track the compiling platform's actual double representation.` |
|       - |  298 | ` */` |
|      68 |  299 | `static void PH7_FLOATEPSILON_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  300 | `{` |
|      34 |  301 | `	SXUNUSED(pUnused);` |
|      71 |  302 | `	ph7_value_double(pVal,DBL_EPSILON);` |
|      71 |  303 | `}` |
|      68 |  304 | `static void PH7_FLOATMAX_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  305 | `{` |
|      34 |  306 | `	SXUNUSED(pUnused);` |
|      71 |  307 | `	ph7_value_double(pVal,DBL_MAX);` |
|      71 |  308 | `}` |
|      64 |  309 | `static void PH7_FLOATMIN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  310 | `{` |
|      32 |  311 | `	SXUNUSED(pUnused);` |
|      67 |  312 | `	ph7_value_double(pVal,DBL_MIN);` |
|      67 |  313 | `}` |
|      64 |  314 | `static void PH7_FLOATDIG_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  315 | `{` |
|      32 |  316 | `	SXUNUSED(pUnused);` |
|      67 |  317 | `	ph7_value_int64(pVal,DBL_DIG);` |
|      67 |  318 | `}` |
|       - |  319 | `/*` |
|       - |  320 | ` * DIRECTORY_SEPARATOR.` |
|       - |  321 | ` * Expand the directory separator character.` |
|       - |  322 | ` */` |
|    1378 |  323 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  324 | `{` |
|     689 |  325 | `	SXUNUSED(pUnused);` |
|       - |  326 | `#ifdef __WINNT__` |
|       5 |  327 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|       - |  328 | `#else` |
|    1378 |  329 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|       - |  330 | `#endif` |
|    1383 |  331 | `}` |
|       - |  332 | `/*` |
|       - |  333 | ` * PATH_SEPARATOR.` |
|       - |  334 | ` * Expand the path separator character.` |
|       - |  335 | ` */` |
|      70 |  336 | `static void PH7_PATHSEP_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  337 | `{` |
|      35 |  338 | `	SXUNUSED(pUnused);` |
|       - |  339 | `#ifdef __WINNT__` |
|       4 |  340 | `	ph7_value_string(pVal,";",(int)sizeof(char));` |
|       - |  341 | `#else` |
|      70 |  342 | `	ph7_value_string(pVal,":",(int)sizeof(char));` |
|       - |  343 | `#endif` |
|      74 |  344 | `}` |
|       - |  345 |  |
|       - |  346 | `#if defined(PH7_ENABLE_MATH_FUNC)` |
|       - |  347 | `/*` |
|       - |  348 | ` * NAN constant: floating-point Not-A-Number` |
|       - |  349 | ` */` |
|     198 |  350 | `static void PH7_NAN_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  351 | `{` |
|      99 |  352 | `	SXUNUSED(pUnused);` |
|     203 |  353 | `	ph7_value_double(pVal, PH7_NAN_VALUE());` |
|     203 |  354 | `}` |
|       - |  355 |  |
|       - |  356 | `/*` |
|       - |  357 | ` * INF constant: positive infinity` |
|       - |  358 | ` */` |
|     224 |  359 | `static void PH7_INF_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  360 | `{` |
|     112 |  361 | `	SXUNUSED(pUnused);` |
|       - |  362 | `	/* similarly avoid the INFINITY macro */` |
|     229 |  363 | `	ph7_value_double(pVal, PH7_INF_VALUE());` |
|     229 |  364 | `}` |
|       - |  365 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|       - |  366 |  |
|       - |  367 | `#ifndef __WINNT__` |
|       - |  368 | `#include <time.h>` |
|       - |  369 | `#endif` |
|       - |  370 | `/*` |
|       - |  371 | ` * __TIME__` |
|       - |  372 | ` *  Expand the current time (GMT).` |
|       - |  373 | ` */` |
|      64 |  374 | `static void PH7_TIME_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  375 | `{` |
|       - |  376 | `	Sytm sTm;` |
|       - |  377 | `#ifdef __WINNT__` |
|       - |  378 | `	SYSTEMTIME sOS;` |
|       3 |  379 | `	GetSystemTime(&sOS);` |
|       3 |  380 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|       - |  381 | `#else` |
|       - |  382 | `	struct tm *pTm;` |
|       - |  383 | `	time_t t;` |
|      64 |  384 | `	time(&t);` |
|      64 |  385 | `	pTm = gmtime(&t);` |
|      64 |  386 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|       - |  387 | `#endif` |
|      32 |  388 | `	SXUNUSED(pUnused); /* cc warning */` |
|       - |  389 | `	/* Expand */` |
|      67 |  390 | `	ph7_value_string_format(pVal,"%02d:%02d:%02d",sTm.tm_hour,sTm.tm_min,sTm.tm_sec);` |
|      67 |  391 | `}` |
|       - |  392 | `/*` |
|       - |  393 | ` * __DATE__` |
|       - |  394 | ` *  Expand the current date in the ISO-8601 format.` |
|       - |  395 | ` */` |
|      64 |  396 | `static void PH7_DATE_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  397 | `{` |
|       - |  398 | `	Sytm sTm;` |
|       - |  399 | `#ifdef __WINNT__` |
|       - |  400 | `	SYSTEMTIME sOS;` |
|       3 |  401 | `	GetSystemTime(&sOS);` |
|       3 |  402 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|       - |  403 | `#else` |
|       - |  404 | `	struct tm *pTm;` |
|       - |  405 | `	time_t t;` |
|      64 |  406 | `	time(&t);` |
|      64 |  407 | `	pTm = gmtime(&t);` |
|      64 |  408 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|       - |  409 | `#endif` |
|      32 |  410 | `	SXUNUSED(pUnused); /* cc warning */` |
|       - |  411 | `	/* Expand */` |
|      67 |  412 | `	ph7_value_string_format(pVal,"%04qd-%02d-%02d",sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|      67 |  413 | `}` |
|       - |  414 | `/*` |
|       - |  415 | ` * __FILE__` |
|       - |  416 | ` *  Path of the processed script.` |
|       - |  417 | ` */` |
|      62 |  418 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  419 | `{` |
|      65 |  420 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - |  421 | `	SyString *pFile;` |
|       - |  422 | `	/* Peek the top entry */` |
|      65 |  423 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      65 |  424 | `	if( pFile == 0 ){` |
|       - |  425 | `		/* Expand the magic word: ":MEMORY:" */` |
|     ! 0 |  426 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|     ! 0 |  427 | `	}else{` |
|      65 |  428 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|       - |  429 | `	}` |
|      65 |  430 | `}` |
|       - |  431 | `/*` |
|       - |  432 | ` * __DIR__` |
|       - |  433 | ` *  Directory holding the processed script.` |
|       - |  434 | ` */` |
|      62 |  435 | `static void PH7_DIR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  436 | `{` |
|      65 |  437 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - |  438 | `	SyString *pFile;` |
|       - |  439 | `	/* Peek the top entry */` |
|      65 |  440 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      65 |  441 | `	if( pFile == 0 ){` |
|       - |  442 | `		/* Expand the magic word: ":MEMORY:" */` |
|     ! 0 |  443 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|     ! 0 |  444 | `	}else{` |
|      65 |  445 | `		if( pFile->nByte > 0 ){` |
|       - |  446 | `			const char *zDir;` |
|       - |  447 | `			int nLen;` |
|      65 |  448 | `			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|      65 |  449 | `			ph7_value_string(pVal,zDir,nLen);` |
|      34 |  450 | `		}else{` |
|       - |  451 | `			/* Expand '.' as the current directory*/` |
|     ! 0 |  452 | `			ph7_value_string(pVal,".",(int)sizeof(char));` |
|       - |  453 | `		}` |
|       - |  454 | `	}` |
|      65 |  455 | `}` |
|       - |  456 | `/*` |
|       - |  457 | ` * PHP_SHLIB_SUFFIX` |
|       - |  458 | ` *  Expand shared library suffix.` |
|       - |  459 | ` */` |
|      64 |  460 | `static void PH7_PHP_SHLIB_SUFFIX_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  461 | `{` |
|       - |  462 | `#ifdef __WINNT__` |
|       3 |  463 | `	ph7_value_string(pVal,"dll",(int)sizeof("dll")-1);` |
|       - |  464 | `#else` |
|      64 |  465 | `	ph7_value_string(pVal,"so",(int)sizeof("so")-1);` |
|       - |  466 | `#endif` |
|      32 |  467 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 |  468 | `}` |
|       - |  469 | `/*` |
|       - |  470 | ` * E_ERROR` |
|       - |  471 | ` *  Expands 1` |
|       - |  472 | ` */` |
|      66 |  473 | `static void PH7_E_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  474 | `{` |
|      69 |  475 | `	ph7_value_int(pVal,1);` |
|      33 |  476 | `	SXUNUSED(pUserData);` |
|      69 |  477 | `}` |
|       - |  478 | `/*` |
|       - |  479 | ` * E_WARNING` |
|       - |  480 | ` *  Expands 2` |
|       - |  481 | ` */` |
|      74 |  482 | `static void PH7_E_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  483 | `{` |
|      79 |  484 | `	ph7_value_int(pVal,2);` |
|      37 |  485 | `	SXUNUSED(pUserData);` |
|      79 |  486 | `}` |
|       - |  487 | `/*` |
|       - |  488 | ` * E_PARSE` |
|       - |  489 | ` *  Expands 4` |
|       - |  490 | ` */` |
|      64 |  491 | `static void PH7_E_PARSE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  492 | `{` |
|      67 |  493 | `	ph7_value_int(pVal,4);` |
|      32 |  494 | `	SXUNUSED(pUserData);` |
|      67 |  495 | `}` |
|       - |  496 | `/*` |
|       - |  497 | ` * E_NOTICE` |
|       - |  498 | ` * Expands 8` |
|       - |  499 | ` */` |
|      68 |  500 | `static void PH7_E_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  501 | `{` |
|      71 |  502 | `	ph7_value_int(pVal,8);` |
|      34 |  503 | `	SXUNUSED(pUserData);` |
|      71 |  504 | `}` |
|       - |  505 | `/*` |
|       - |  506 | ` * E_CORE_ERROR` |
|       - |  507 | ` * Expands 16` |
|       - |  508 | ` */` |
|      64 |  509 | `static void PH7_E_CORE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  510 | `{` |
|      67 |  511 | `	ph7_value_int(pVal,16);` |
|      32 |  512 | `	SXUNUSED(pUserData);` |
|      67 |  513 | `}` |
|       - |  514 | `/*` |
|       - |  515 | ` * E_CORE_WARNING` |
|       - |  516 | ` * Expands 32` |
|       - |  517 | ` */` |
|      64 |  518 | `static void PH7_E_CORE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  519 | `{` |
|      67 |  520 | `	ph7_value_int(pVal,32);` |
|      32 |  521 | `	SXUNUSED(pUserData);` |
|      67 |  522 | `}` |
|       - |  523 | `/*` |
|       - |  524 | ` * E_COMPILE_ERROR` |
|       - |  525 | ` * Expands 64` |
|       - |  526 | ` */` |
|      64 |  527 | `static void PH7_E_COMPILE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  528 | `{` |
|      67 |  529 | `	ph7_value_int(pVal,64);` |
|      32 |  530 | `	SXUNUSED(pUserData);` |
|      67 |  531 | `}` |
|       - |  532 | `/*` |
|       - |  533 | ` * E_COMPILE_WARNING` |
|       - |  534 | ` * Expands 128` |
|       - |  535 | ` */` |
|      64 |  536 | `static void PH7_E_COMPILE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  537 | `{` |
|      67 |  538 | `	ph7_value_int(pVal,128);` |
|      32 |  539 | `	SXUNUSED(pUserData);` |
|      67 |  540 | `}` |
|       - |  541 | `/*` |
|       - |  542 | ` * E_USER_ERROR` |
|       - |  543 | ` * Expands 256` |
|       - |  544 | ` */` |
|      68 |  545 | `static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  546 | `{` |
|      71 |  547 | `	ph7_value_int(pVal,256);` |
|      34 |  548 | `	SXUNUSED(pUserData);` |
|      71 |  549 | `}` |
|       - |  550 | `/*` |
|       - |  551 | ` * E_USER_WARNING` |
|       - |  552 | ` * Expands 512` |
|       - |  553 | ` */` |
|     110 |  554 | `static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  555 | `{` |
|     114 |  556 | `	ph7_value_int(pVal,512);` |
|      55 |  557 | `	SXUNUSED(pUserData);` |
|     114 |  558 | `}` |
|       - |  559 | `/*` |
|       - |  560 | ` * E_USER_NOTICE` |
|       - |  561 | ` * Expands 1024` |
|       - |  562 | ` */` |
|     132 |  563 | `static void PH7_E_USER_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  564 | `{` |
|     136 |  565 | `	ph7_value_int(pVal,1024);` |
|      66 |  566 | `	SXUNUSED(pUserData);` |
|     136 |  567 | `}` |
|       - |  568 | `/*` |
|       - |  569 | ` * E_RECOVERABLE_ERROR` |
|       - |  570 | ` * Expands 4096` |
|       - |  571 | ` */` |
|      64 |  572 | `static void PH7_E_RECOVERABLE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  573 | `{` |
|      67 |  574 | `	ph7_value_int(pVal,4096);` |
|      32 |  575 | `	SXUNUSED(pUserData);` |
|      67 |  576 | `}` |
|       - |  577 | `/*` |
|       - |  578 | ` * E_DEPRECATED` |
|       - |  579 | ` * Expands 8192` |
|       - |  580 | ` */` |
|      72 |  581 | `static void PH7_E_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  582 | `{` |
|      75 |  583 | `	ph7_value_int(pVal,8192);` |
|      36 |  584 | `	SXUNUSED(pUserData);` |
|      75 |  585 | `}` |
|       - |  586 | `/*` |
|       - |  587 | ` * E_USER_DEPRECATED` |
|       - |  588 | ` *   Expands 16384.` |
|       - |  589 | ` */` |
|      72 |  590 | `static void PH7_E_USER_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  591 | `{` |
|      75 |  592 | `	ph7_value_int(pVal,16384);` |
|      36 |  593 | `	SXUNUSED(pUserData);` |
|      75 |  594 | `}` |
|       - |  595 | `/*` |
|       - |  596 | ` * E_ALL` |
|       - |  597 | ` *  Expands 30719 (php 8: E_STRICT is no longer part of E_ALL)` |
|       - |  598 | ` */` |
|     110 |  599 | `static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  600 | `{` |
|     115 |  601 | `	ph7_value_int(pVal,PH7_E_ALL_MASK);` |
|      55 |  602 | `	SXUNUSED(pUserData);` |
|     115 |  603 | `}` |
|       - |  604 | `/*` |
|       - |  605 | ` * CASE_LOWER` |
|       - |  606 | ` *  Expands 0.` |
|       - |  607 | ` */` |
|      64 |  608 | `static void PH7_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  609 | `{` |
|      67 |  610 | `	ph7_value_int(pVal,0);` |
|      32 |  611 | `	SXUNUSED(pUserData);` |
|      67 |  612 | `}` |
|       - |  613 | `/*` |
|       - |  614 | ` * CASE_UPPER` |
|       - |  615 | ` *  Expands 1.` |
|       - |  616 | ` */` |
|      70 |  617 | `static void PH7_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  618 | `{` |
|      73 |  619 | `	ph7_value_int(pVal,1);` |
|      35 |  620 | `	SXUNUSED(pUserData);` |
|      73 |  621 | `}` |
|       - |  622 | `/*` |
|       - |  623 | ` * STR_PAD_LEFT` |
|       - |  624 | ` *  Expands 0.` |
|       - |  625 | ` */` |
|      76 |  626 | `static void PH7_STR_PAD_LEFT_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  627 | `{` |
|      79 |  628 | `	ph7_value_int(pVal,0);` |
|      38 |  629 | `	SXUNUSED(pUserData);` |
|      79 |  630 | `}` |
|       - |  631 | `/*` |
|       - |  632 | ` * STR_PAD_RIGHT` |
|       - |  633 | ` *  Expands 1.` |
|       - |  634 | ` */` |
|      70 |  635 | `static void PH7_STR_PAD_RIGHT_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  636 | `{` |
|      73 |  637 | `	ph7_value_int(pVal,1);` |
|      35 |  638 | `	SXUNUSED(pUserData);` |
|      73 |  639 | `}` |
|       - |  640 | `/*` |
|       - |  641 | ` * STR_PAD_BOTH` |
|       - |  642 | ` *  Expands 2.` |
|       - |  643 | ` */` |
|      68 |  644 | `static void PH7_STR_PAD_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  645 | `{` |
|      71 |  646 | `	ph7_value_int(pVal,2);` |
|      34 |  647 | `	SXUNUSED(pUserData);` |
|      71 |  648 | `}` |
|       - |  649 | `/*` |
|       - |  650 | ` * stream_wrapper_register()'s $flags. php defines exactly this one bit: the` |
|       - |  651 | ` * wrapper speaks to the network, so allow_url_fopen gates opening it and` |
|       - |  652 | ` * allow_url_include gates INCLUDING it.` |
|       - |  653 | ` */` |
|      68 |  654 | `static void PH7_STREAM_IS_URL_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  655 | `{` |
|      72 |  656 | `	ph7_value_int(pVal,PH7_STREAM_IS_URL);` |
|      34 |  657 | `	SXUNUSED(pUserData);` |
|      72 |  658 | `}` |
|       - |  659 | `/*` |
|       - |  660 | ` * A userland filter's ANSWER, and which kind of call it is answering. FEED_ME` |
|       - |  661 | ` * says "I produced nothing, ask me again with more"; ERR_FATAL ends the stream.` |
|       - |  662 | ` */` |
|      96 |  663 | `static void PH7_PSFS_PASS_ON_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  664 | `{` |
|      99 |  665 | `	ph7_value_int(pVal,PHL_PSFS_PASS_ON);` |
|      48 |  666 | `	SXUNUSED(pUserData);` |
|      99 |  667 | `}` |
|      68 |  668 | `static void PH7_PSFS_FEED_ME_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  669 | `{` |
|      71 |  670 | `	ph7_value_int(pVal,PHL_PSFS_FEED_ME);` |
|      34 |  671 | `	SXUNUSED(pUserData);` |
|      71 |  672 | `}` |
|      66 |  673 | `static void PH7_PSFS_ERR_FATAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  674 | `{` |
|      69 |  675 | `	ph7_value_int(pVal,PHL_PSFS_ERR_FATAL);` |
|      33 |  676 | `	SXUNUSED(pUserData);` |
|      69 |  677 | `}` |
|      64 |  678 | `static void PH7_PSFS_FLAG_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  679 | `{` |
|      67 |  680 | `	ph7_value_int(pVal,PHL_PSFS_FLAG_NORMAL);` |
|      32 |  681 | `	SXUNUSED(pUserData);` |
|      67 |  682 | `}` |
|      64 |  683 | `static void PH7_PSFS_FLAG_FLUSH_INC_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  684 | `{` |
|      67 |  685 | `	ph7_value_int(pVal,PHL_PSFS_FLAG_FLUSH_INC);` |
|      32 |  686 | `	SXUNUSED(pUserData);` |
|      67 |  687 | `}` |
|      64 |  688 | `static void PH7_PSFS_FLAG_FLUSH_CLOSE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  689 | `{` |
|      67 |  690 | `	ph7_value_int(pVal,PHL_PSFS_FLAG_FLUSH_CLOSE);` |
|      32 |  691 | `	SXUNUSED(pUserData);` |
|      67 |  692 | `}` |
|       - |  693 | `/*` |
|       - |  694 | ` * stream_filter_append()'s $mode — WHICH chain the filter joins. php's 0 is not` |
|       - |  695 | ` * "neither": it means "whichever chains the handle's own mode makes sense for".` |
|       - |  696 | ` */` |
|     190 |  697 | `static void PH7_STREAM_FILTER_READ_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  698 | `{` |
|     193 |  699 | `	ph7_value_int(pVal,PHL_STREAM_FILTER_READ);` |
|      95 |  700 | `	SXUNUSED(pUserData);` |
|     193 |  701 | `}` |
|      88 |  702 | `static void PH7_STREAM_FILTER_WRITE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  703 | `{` |
|      91 |  704 | `	ph7_value_int(pVal,PHL_STREAM_FILTER_WRITE);` |
|      44 |  705 | `	SXUNUSED(pUserData);` |
|      91 |  706 | `}` |
|      64 |  707 | `static void PH7_STREAM_FILTER_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  708 | `{` |
|      67 |  709 | `	ph7_value_int(pVal,PHL_STREAM_FILTER_ALL);` |
|      32 |  710 | `	SXUNUSED(pUserData);` |
|      67 |  711 | `}` |
|       - |  712 | `/*` |
|       - |  713 | ` * stream_socket_client()'s $flags. CONNECT is the default it documents;` |
|       - |  714 | ` * PERSISTENT is what pfsockopen() means and the only one that changes what a` |
|       - |  715 | ` * second call to the same address ANSWERS.` |
|       - |  716 | ` */` |
|      86 |  717 | `static void PH7_STREAM_CLIENT_CONNECT_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  718 | `{` |
|      90 |  719 | `	ph7_value_int(pVal,PH7_STREAM_CLIENT_CONNECT);` |
|      43 |  720 | `	SXUNUSED(pUserData);` |
|      90 |  721 | `}` |
|      64 |  722 | `static void PH7_STREAM_CLIENT_ASYNC_CONNECT_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  723 | `{` |
|      68 |  724 | `	ph7_value_int(pVal,PH7_STREAM_CLIENT_ASYNC_CONNECT);` |
|      32 |  725 | `	SXUNUSED(pUserData);` |
|      68 |  726 | `}` |
|      70 |  727 | `static void PH7_STREAM_CLIENT_PERSISTENT_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  728 | `{` |
|      74 |  729 | `	ph7_value_int(pVal,PH7_STREAM_CLIENT_PERSISTENT);` |
|      35 |  730 | `	SXUNUSED(pUserData);` |
|      74 |  731 | `}` |
|       - |  732 | `/*` |
|       - |  733 | ` * The socket-family constants. Their VALUES are the platform's own — AF_INET6 is` |
|       - |  734 | ` * 10 on Linux, 23 on Windows and 30 on the BSDs — so they are asked for by id` |
|       - |  735 | ` * rather than written down here, and a program handing one to` |
|       - |  736 | ` * stream_socket_pair() is handing the OS its own number.` |
|       - |  737 | ` */` |
|       - |  738 | `#ifdef PH7_ENABLE_NET` |
|      62 |  739 | `static void PH7_STREAM_PF_INET_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  740 | `{` |
|      65 |  741 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_INET));` |
|      31 |  742 | `	SXUNUSED(pUserData);` |
|      65 |  743 | `}` |
|      62 |  744 | `static void PH7_STREAM_PF_INET6_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  745 | `{` |
|      65 |  746 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_INET6));` |
|      31 |  747 | `	SXUNUSED(pUserData);` |
|      65 |  748 | `}` |
|      66 |  749 | `static void PH7_STREAM_PF_UNIX_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  750 | `{` |
|      69 |  751 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_UNIX));` |
|      33 |  752 | `	SXUNUSED(pUserData);` |
|      69 |  753 | `}` |
|      64 |  754 | `static void PH7_STREAM_SOCK_STREAM_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  755 | `{` |
|      67 |  756 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_STREAM));` |
|      32 |  757 | `	SXUNUSED(pUserData);` |
|      67 |  758 | `}` |
|      62 |  759 | `static void PH7_STREAM_SOCK_DGRAM_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  760 | `{` |
|      65 |  761 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_DGRAM));` |
|      31 |  762 | `	SXUNUSED(pUserData);` |
|      65 |  763 | `}` |
|      62 |  764 | `static void PH7_STREAM_SOCK_RAW_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  765 | `{` |
|      65 |  766 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_RAW));` |
|      31 |  767 | `	SXUNUSED(pUserData);` |
|      65 |  768 | `}` |
|      62 |  769 | `static void PH7_STREAM_SOCK_SEQPACKET_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  770 | `{` |
|      65 |  771 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_SEQPACKET));` |
|      31 |  772 | `	SXUNUSED(pUserData);` |
|      65 |  773 | `}` |
|      62 |  774 | `static void PH7_STREAM_SOCK_RDM_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  775 | `{` |
|      65 |  776 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_RDM));` |
|      31 |  777 | `	SXUNUSED(pUserData);` |
|      65 |  778 | `}` |
|      62 |  779 | `static void PH7_STREAM_IPPROTO_IP_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  780 | `{` |
|      65 |  781 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_IP));` |
|      31 |  782 | `	SXUNUSED(pUserData);` |
|      65 |  783 | `}` |
|      62 |  784 | `static void PH7_STREAM_IPPROTO_TCP_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  785 | `{` |
|      65 |  786 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_TCP));` |
|      31 |  787 | `	SXUNUSED(pUserData);` |
|      65 |  788 | `}` |
|      62 |  789 | `static void PH7_STREAM_IPPROTO_UDP_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  790 | `{` |
|      65 |  791 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_UDP));` |
|      31 |  792 | `	SXUNUSED(pUserData);` |
|      65 |  793 | `}` |
|      62 |  794 | `static void PH7_STREAM_IPPROTO_ICMP_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  795 | `{` |
|      65 |  796 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_ICMP));` |
|      31 |  797 | `	SXUNUSED(pUserData);` |
|      65 |  798 | `}` |
|      62 |  799 | `static void PH7_STREAM_IPPROTO_RAW_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  800 | `{` |
|      65 |  801 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_RAW));` |
|      31 |  802 | `	SXUNUSED(pUserData);` |
|      65 |  803 | `}` |
|       - |  804 | `#endif /* PH7_ENABLE_NET */` |
|       - |  805 | `/*` |
|       - |  806 | ` * stream_socket_shutdown()'s $mode, and the two recvfrom/sendto flags. These` |
|       - |  807 | ` * three ARE php's own numbers rather than the OS's: php maps STREAM_OOB and` |
|       - |  808 | ` * STREAM_PEEK onto MSG_OOB/MSG_PEEK itself.` |
|       - |  809 | ` */` |
|      66 |  810 | `static void PH7_STREAM_SHUT_RD_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  811 | `{` |
|      69 |  812 | `	ph7_value_int(pVal,PH7_STREAM_SHUT_RD);` |
|      33 |  813 | `	SXUNUSED(pUserData);` |
|      69 |  814 | `}` |
|      64 |  815 | `static void PH7_STREAM_SHUT_WR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  816 | `{` |
|      67 |  817 | `	ph7_value_int(pVal,PH7_STREAM_SHUT_WR);` |
|      32 |  818 | `	SXUNUSED(pUserData);` |
|      67 |  819 | `}` |
|      62 |  820 | `static void PH7_STREAM_SHUT_RDWR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  821 | `{` |
|      65 |  822 | `	ph7_value_int(pVal,PH7_STREAM_SHUT_RDWR);` |
|      31 |  823 | `	SXUNUSED(pUserData);` |
|      65 |  824 | `}` |
|      62 |  825 | `static void PH7_STREAM_OOB_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  826 | `{` |
|      65 |  827 | `	ph7_value_int(pVal,PH7_STREAM_OOB);` |
|      31 |  828 | `	SXUNUSED(pUserData);` |
|      65 |  829 | `}` |
|      64 |  830 | `static void PH7_STREAM_PEEK_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  831 | `{` |
|      67 |  832 | `	ph7_value_int(pVal,PH7_STREAM_PEEK);` |
|      32 |  833 | `	SXUNUSED(pUserData);` |
|      67 |  834 | `}` |
|       - |  835 | `/*` |
|       - |  836 | ` * stream_socket_server()'s $flags. Its default is BIND\|LISTEN, and the two are` |
|       - |  837 | ` * separate because binding is all a datagram server does.` |
|       - |  838 | ` */` |
|      70 |  839 | `static void PH7_STREAM_SERVER_BIND_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  840 | `{` |
|      74 |  841 | `	ph7_value_int(pVal,PH7_STREAM_SERVER_BIND);` |
|      35 |  842 | `	SXUNUSED(pUserData);` |
|      74 |  843 | `}` |
|      68 |  844 | `static void PH7_STREAM_SERVER_LISTEN_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  845 | `{` |
|      72 |  846 | `	ph7_value_int(pVal,PH7_STREAM_SERVER_LISTEN);` |
|      34 |  847 | `	SXUNUSED(pUserData);` |
|      72 |  848 | `}` |
|       - |  849 | `/*` |
|       - |  850 | ` * mt_srand()'s $mode: which GENERATOR to seed. MT_RAND_PHP is php's pre-7.1` |
|       - |  851 | ` * Mersenne Twister, whose twist reads the low bit of the wrong word — a` |
|       - |  852 | ` * different sequence, which is the only reason to ask for it.` |
|       - |  853 | ` */` |
|      72 |  854 | `static void PH7_MT_RAND_MT19937_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  855 | `{` |
|      75 |  856 | `	ph7_value_int(pVal,PH7_MT_RAND_MT19937);` |
|      36 |  857 | `	SXUNUSED(pUserData);` |
|      75 |  858 | `}` |
|      78 |  859 | `static void PH7_MT_RAND_PHP_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  860 | `{` |
|      81 |  861 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - |  862 | `	/* php 8.3 deprecated the SYMBOL as well as the mode, and says why — when a` |
|       - |  863 | `	 * program NAMES it. Listing the constant table is not naming it. */` |
|      81 |  864 | `	if( pVm && !pVm->bConstEnum ){` |
|      18 |  865 | `		PH7_VmThrowError(pVm,0,8192 /* E_DEPRECATED */,` |
|       - |  866 | `			"Constant MT_RAND_PHP is deprecated since 8.3, as it uses a biased non-standard variant of Mt19937");` |
|       8 |  867 | `	}` |
|      81 |  868 | `	ph7_value_int(pVal,PH7_MT_RAND_PHP);` |
|      81 |  869 | `}` |
|       - |  870 | `/*` |
|       - |  871 | ` * Output-handler flags and phases (ob_start()'s $flags, and the $phase an output` |
|       - |  872 | ` * handler is called with). The values are php's and are a public ABI: the phase` |
|       - |  873 | ` * bits are OR'd together (a first FLUSH arrives as FLUSH\|START = 5), and the` |
|       - |  874 | ` * three capability flags are the ones ob_clean()/ob_flush()/ob_end_*() test` |
|       - |  875 | ` * before they will touch the buffer.` |
|       - |  876 | ` */` |
|     128 |  877 | `static void PH7_OB_WRITE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  878 | `{` |
|     132 |  879 | `	ph7_value_int(pVal,PH7_OB_WRITE);` |
|      64 |  880 | `	SXUNUSED(pUserData);` |
|     132 |  881 | `}` |
|      64 |  882 | `static void PH7_OB_START_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  883 | `{` |
|      68 |  884 | `	ph7_value_int(pVal,PH7_OB_START);` |
|      32 |  885 | `	SXUNUSED(pUserData);` |
|      68 |  886 | `}` |
|      64 |  887 | `static void PH7_OB_CLEAN_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  888 | `{` |
|      68 |  889 | `	ph7_value_int(pVal,PH7_OB_CLEAN);` |
|      32 |  890 | `	SXUNUSED(pUserData);` |
|      68 |  891 | `}` |
|      64 |  892 | `static void PH7_OB_FLUSH_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  893 | `{` |
|      68 |  894 | `	ph7_value_int(pVal,PH7_OB_FLUSH);` |
|      32 |  895 | `	SXUNUSED(pUserData);` |
|      68 |  896 | `}` |
|     128 |  897 | `static void PH7_OB_FINAL_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  898 | `{` |
|     132 |  899 | `	ph7_value_int(pVal,PH7_OB_FINAL);` |
|      64 |  900 | `	SXUNUSED(pUserData);` |
|     132 |  901 | `}` |
|      68 |  902 | `static void PH7_OB_CLEANABLE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  903 | `{` |
|      72 |  904 | `	ph7_value_int(pVal,PH7_OB_CLEANABLE);` |
|      34 |  905 | `	SXUNUSED(pUserData);` |
|      72 |  906 | `}` |
|      68 |  907 | `static void PH7_OB_FLUSHABLE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  908 | `{` |
|      72 |  909 | `	ph7_value_int(pVal,PH7_OB_FLUSHABLE);` |
|      34 |  910 | `	SXUNUSED(pUserData);` |
|      72 |  911 | `}` |
|      70 |  912 | `static void PH7_OB_REMOVABLE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  913 | `{` |
|      74 |  914 | `	ph7_value_int(pVal,PH7_OB_REMOVABLE);` |
|      35 |  915 | `	SXUNUSED(pUserData);` |
|      74 |  916 | `}` |
|      78 |  917 | `static void PH7_OB_STDFLAGS_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  918 | `{` |
|      82 |  919 | `	ph7_value_int(pVal,PH7_OB_STDFLAGS);` |
|      39 |  920 | `	SXUNUSED(pUserData);` |
|      82 |  921 | `}` |
|      66 |  922 | `static void PH7_OB_STARTED_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  923 | `{` |
|      70 |  924 | `	ph7_value_int(pVal,PH7_OB_STARTED);` |
|      33 |  925 | `	SXUNUSED(pUserData);` |
|      70 |  926 | `}` |
|      66 |  927 | `static void PH7_OB_DISABLED_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  928 | `{` |
|      70 |  929 | `	ph7_value_int(pVal,PH7_OB_DISABLED);` |
|      33 |  930 | `	SXUNUSED(pUserData);` |
|      70 |  931 | `}` |
|      68 |  932 | `static void PH7_OB_PROCESSED_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  933 | `{` |
|      72 |  934 | `	ph7_value_int(pVal,PH7_OB_PROCESSED);` |
|      34 |  935 | `	SXUNUSED(pUserData);` |
|      72 |  936 | `}` |
|       - |  937 | `/*` |
|       - |  938 | ` * array_filter()'s $mode selector. The VALUES are php's and are a public ABI --` |
|       - |  939 | ` * ARRAY_FILTER_USE_BOTH is 1 and ARRAY_FILTER_USE_KEY is 2, NOT the other way` |
|       - |  940 | ` * round, and they are a selector rather than a bit mask (php reads the argument` |
|       - |  941 | ` * with ==, so any other number is the default value mode).` |
|       - |  942 | ` */` |
|      80 |  943 | `static void PH7_ARRAY_FILTER_USE_KEY_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  944 | `{` |
|      83 |  945 | `	ph7_value_int(pVal,2);` |
|      40 |  946 | `	SXUNUSED(pUserData);` |
|      83 |  947 | `}` |
|      70 |  948 | `static void PH7_ARRAY_FILTER_USE_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  949 | `{` |
|      73 |  950 | `	ph7_value_int(pVal,1);` |
|      35 |  951 | `	SXUNUSED(pUserData);` |
|      73 |  952 | `}` |
|       - |  953 | `/*` |
|       - |  954 | ` * COUNT_NORMAL` |
|       - |  955 | ` *  Expands 0` |
|       - |  956 | ` */` |
|      70 |  957 | `static void PH7_COUNT_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  958 | `{` |
|      73 |  959 | `	ph7_value_int(pVal,0);` |
|      35 |  960 | `	SXUNUSED(pUserData);` |
|      73 |  961 | `}` |
|       - |  962 | `/*` |
|       - |  963 | ` * COUNT_RECURSIVE` |
|       - |  964 | ` *  Expands 1.` |
|       - |  965 | ` */` |
|      82 |  966 | `static void PH7_COUNT_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  967 | `{` |
|      85 |  968 | `	ph7_value_int(pVal,1);` |
|      41 |  969 | `	SXUNUSED(pUserData);` |
|      85 |  970 | `}` |
|       - |  971 | `/*` |
|       - |  972 | ` * php's sort-flag constants. The VALUES must match php exactly: they are a` |
|       - |  973 | ` * public ABI (code passes literal ints, dumps them, and OR-combines the base` |
|       - |  974 | ` * type with SORT_FLAG_CASE). SORT_ASC/SORT_DESC are the array_multisort` |
|       - |  975 | ` * direction flags.` |
|       - |  976 | ` * SORT_REGULAR 0 · SORT_NUMERIC 1 · SORT_STRING 2 · SORT_DESC 3 · SORT_ASC 4 ·` |
|       - |  977 | ` * SORT_LOCALE_STRING 5 · SORT_NATURAL 6 · SORT_FLAG_CASE 8` |
|       - |  978 | ` */` |
|      78 |  979 | `static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  980 | `{` |
|      82 |  981 | `	ph7_value_int(pVal,4);` |
|      39 |  982 | `	SXUNUSED(pUserData);` |
|      82 |  983 | `}` |
|      74 |  984 | `static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  985 | `{` |
|      77 |  986 | `	ph7_value_int(pVal,3);` |
|      37 |  987 | `	SXUNUSED(pUserData);` |
|      77 |  988 | `}` |
|      76 |  989 | `static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  990 | `{` |
|      79 |  991 | `	ph7_value_int(pVal,0);` |
|      38 |  992 | `	SXUNUSED(pUserData);` |
|      79 |  993 | `}` |
|     132 |  994 | `static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  995 | `{` |
|     137 |  996 | `	ph7_value_int(pVal,1);` |
|      66 |  997 | `	SXUNUSED(pUserData);` |
|     137 |  998 | `}` |
|    1832 |  999 | `static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1000 | `{` |
|    1837 | 1001 | `	ph7_value_int(pVal,2);` |
|     916 | 1002 | `	SXUNUSED(pUserData);` |
|    1837 | 1003 | `}` |
|      66 | 1004 | `static void PH7_SORT_LOCALE_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1005 | `{` |
|      69 | 1006 | `	ph7_value_int(pVal,5);` |
|      33 | 1007 | `	SXUNUSED(pUserData);` |
|      69 | 1008 | `}` |
|      82 | 1009 | `static void PH7_SORT_NATURAL_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1010 | `{` |
|      86 | 1011 | `	ph7_value_int(pVal,6);` |
|      41 | 1012 | `	SXUNUSED(pUserData);` |
|      86 | 1013 | `}` |
|      86 | 1014 | `static void PH7_SORT_FLAG_CASE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1015 | `{` |
|      90 | 1016 | `	ph7_value_int(pVal,8);` |
|      43 | 1017 | `	SXUNUSED(pUserData);` |
|      90 | 1018 | `}` |
|       - | 1019 | `/*` |
|       - | 1020 | ` * PHP_ROUND_HALF_UP` |
|       - | 1021 | ` *  Expands 1.` |
|       - | 1022 | ` */` |
|      66 | 1023 | `static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1024 | `{` |
|      69 | 1025 | `	ph7_value_int(pVal,1);` |
|      33 | 1026 | `	SXUNUSED(pUserData);` |
|      69 | 1027 | `}` |
|       - | 1028 | `/*` |
|       - | 1029 | ` * PHP_SESSION_DISABLED / PHP_SESSION_NONE / PHP_SESSION_ACTIVE` |
|       - | 1030 | ` *  session_status() states (0 / 1 / 2).` |
|       - | 1031 | ` */` |
|      64 | 1032 | `static void PH7_PHP_SESSION_DISABLED_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1033 | `{` |
|      68 | 1034 | `	ph7_value_int(pVal,0);` |
|      32 | 1035 | `	SXUNUSED(pUserData);` |
|      68 | 1036 | `}` |
|      64 | 1037 | `static void PH7_PHP_SESSION_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1038 | `{` |
|      68 | 1039 | `	ph7_value_int(pVal,1);` |
|      32 | 1040 | `	SXUNUSED(pUserData);` |
|      68 | 1041 | `}` |
|      78 | 1042 | `static void PH7_PHP_SESSION_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1043 | `{` |
|      82 | 1044 | `	ph7_value_int(pVal,2);` |
|      39 | 1045 | `	SXUNUSED(pUserData);` |
|      82 | 1046 | `}` |
|       - | 1047 | `/*` |
|       - | 1048 | ` * INI_USER / INI_PERDIR / INI_SYSTEM / INI_ALL` |
|       - | 1049 | ` *  php.ini access levels (1 / 2 / 4 / 7).` |
|       - | 1050 | ` */` |
|      64 | 1051 | `static void PH7_INI_USER_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1052 | `{` |
|      68 | 1053 | `	ph7_value_int(pVal,1);` |
|      32 | 1054 | `	SXUNUSED(pUserData);` |
|      68 | 1055 | `}` |
|      64 | 1056 | `static void PH7_INI_PERDIR_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1057 | `{` |
|      68 | 1058 | `	ph7_value_int(pVal,2);` |
|      32 | 1059 | `	SXUNUSED(pUserData);` |
|      68 | 1060 | `}` |
|      64 | 1061 | `static void PH7_INI_SYSTEM_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1062 | `{` |
|      68 | 1063 | `	ph7_value_int(pVal,4);` |
|      32 | 1064 | `	SXUNUSED(pUserData);` |
|      68 | 1065 | `}` |
|      64 | 1066 | `static void PH7_INI_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1067 | `{` |
|      68 | 1068 | `	ph7_value_int(pVal,7);` |
|      32 | 1069 | `	SXUNUSED(pUserData);` |
|      68 | 1070 | `}` |
|       - | 1071 | `/*` |
|       - | 1072 | ` * MB_CASE_UPPER / MB_CASE_LOWER / MB_CASE_TITLE (0 / 1 / 2)` |
|       - | 1073 | ` */` |
|      66 | 1074 | `static void PH7_MB_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1075 | `{` |
|      69 | 1076 | `	ph7_value_int(pVal,0);` |
|      33 | 1077 | `	SXUNUSED(pUserData);` |
|      69 | 1078 | `}` |
|      66 | 1079 | `static void PH7_MB_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1080 | `{` |
|      69 | 1081 | `	ph7_value_int(pVal,1);` |
|      33 | 1082 | `	SXUNUSED(pUserData);` |
|      69 | 1083 | `}` |
|     102 | 1084 | `static void PH7_MB_CASE_TITLE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1085 | `{` |
|     105 | 1086 | `	ph7_value_int(pVal,2);` |
|      51 | 1087 | `	SXUNUSED(pUserData);` |
|     105 | 1088 | `}` |
|       - | 1089 | `/*` |
|       - | 1090 | ` * SPHP_ROUND_HALF_DOWN` |
|       - | 1091 | ` *  Expands 2.` |
|       - | 1092 | ` */` |
|      66 | 1093 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1094 | `{` |
|      69 | 1095 | `	ph7_value_int(pVal,2);` |
|      33 | 1096 | `	SXUNUSED(pUserData);` |
|      69 | 1097 | `}` |
|       - | 1098 | `/*` |
|       - | 1099 | ` * PHP_ROUND_HALF_EVEN` |
|       - | 1100 | ` *  Expands 3.` |
|       - | 1101 | ` */` |
|      70 | 1102 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1103 | `{` |
|      73 | 1104 | `	ph7_value_int(pVal,3);` |
|      35 | 1105 | `	SXUNUSED(pUserData);` |
|      73 | 1106 | `}` |
|       - | 1107 | `/*` |
|       - | 1108 | ` * PHP_ROUND_HALF_ODD` |
|       - | 1109 | ` *  Expands 4.` |
|       - | 1110 | ` */` |
|      66 | 1111 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1112 | `{` |
|      69 | 1113 | `	ph7_value_int(pVal,4);` |
|      33 | 1114 | `	SXUNUSED(pUserData);` |
|      69 | 1115 | `}` |
|       - | 1116 | `/*` |
|       - | 1117 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|       - | 1118 | ` *  Expand 0x01` |
|       - | 1119 | ` * NOTE:` |
|       - | 1120 | ` *  The expanded value must be a power of two.` |
|       - | 1121 | ` */` |
|      68 | 1122 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1123 | `{` |
|      71 | 1124 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      34 | 1125 | `	SXUNUSED(pUserData);` |
|      71 | 1126 | `}` |
|       - | 1127 | `/*` |
|       - | 1128 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|       - | 1129 | ` *  Expand 0x02` |
|       - | 1130 | ` * NOTE:` |
|       - | 1131 | ` *  The expanded value must be a power of two.` |
|       - | 1132 | ` */` |
|      72 | 1133 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1134 | `{` |
|      75 | 1135 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      36 | 1136 | `	SXUNUSED(pUserData);` |
|      75 | 1137 | `}` |
|       - | 1138 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|       - | 1139 | `/*` |
|       - | 1140 | ` * M_PI` |
|       - | 1141 | ` *  Expand the value of pi.` |
|       - | 1142 | ` */` |
|      72 | 1143 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1144 | `{` |
|      36 | 1145 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 1146 | `	ph7_value_double(pVal,PH7_PI);` |
|      75 | 1147 | `}` |
|       - | 1148 | `/*` |
|       - | 1149 | ` * M_E` |
|       - | 1150 | ` *  Expand 2.7182818284590452354` |
|       - | 1151 | ` */` |
|      64 | 1152 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1153 | `{` |
|      32 | 1154 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1155 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|      67 | 1156 | `}` |
|       - | 1157 | `/*` |
|       - | 1158 | ` * M_LOG2E` |
|       - | 1159 | ` *  Expand 2.7182818284590452354` |
|       - | 1160 | ` */` |
|      64 | 1161 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1162 | `{` |
|      32 | 1163 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1164 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      67 | 1165 | `}` |
|       - | 1166 | `/*` |
|       - | 1167 | ` * M_LOG10E` |
|       - | 1168 | ` *  Expand 0.4342944819032518276` |
|       - | 1169 | ` */` |
|      64 | 1170 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1171 | `{` |
|      32 | 1172 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1173 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      67 | 1174 | `}` |
|       - | 1175 | `/*` |
|       - | 1176 | ` * M_LN2` |
|       - | 1177 | ` *  Expand 	0.69314718055994530942` |
|       - | 1178 | ` */` |
|      64 | 1179 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1180 | `{` |
|      32 | 1181 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1182 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      67 | 1183 | `}` |
|       - | 1184 | `/*` |
|       - | 1185 | ` * M_LN10` |
|       - | 1186 | ` *  Expand 	2.30258509299404568402` |
|       - | 1187 | ` */` |
|      64 | 1188 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1189 | `{` |
|      32 | 1190 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1191 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      67 | 1192 | `}` |
|       - | 1193 | `/*` |
|       - | 1194 | ` * M_PI_2` |
|       - | 1195 | ` *  Expand 	1.57079632679489661923` |
|       - | 1196 | ` */` |
|      64 | 1197 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1198 | `{` |
|      32 | 1199 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1200 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      67 | 1201 | `}` |
|       - | 1202 | `/*` |
|       - | 1203 | ` * M_PI_4` |
|       - | 1204 | ` *  Expand 	0.78539816339744830962` |
|       - | 1205 | ` */` |
|      64 | 1206 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1207 | `{` |
|      32 | 1208 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1209 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      67 | 1210 | `}` |
|       - | 1211 | `/*` |
|       - | 1212 | ` * M_1_PI` |
|       - | 1213 | ` *  Expand 	0.31830988618379067154` |
|       - | 1214 | ` */` |
|      64 | 1215 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1216 | `{` |
|      32 | 1217 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1218 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      67 | 1219 | `}` |
|       - | 1220 | `/*` |
|       - | 1221 | ` * M_2_PI` |
|       - | 1222 | ` *  Expand 0.63661977236758134308` |
|       - | 1223 | ` */` |
|      66 | 1224 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1225 | `{` |
|      33 | 1226 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1227 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      69 | 1228 | `}` |
|       - | 1229 | `/*` |
|       - | 1230 | ` * M_SQRTPI` |
|       - | 1231 | ` *  Expand 1.77245385090551602729` |
|       - | 1232 | ` */` |
|      64 | 1233 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1234 | `{` |
|      32 | 1235 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1236 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      67 | 1237 | `}` |
|       - | 1238 | `/*` |
|       - | 1239 | ` * M_2_SQRTPI` |
|       - | 1240 | ` *  Expand 	1.12837916709551257390` |
|       - | 1241 | ` */` |
|      64 | 1242 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1243 | `{` |
|      32 | 1244 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1245 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      67 | 1246 | `}` |
|       - | 1247 | `/*` |
|       - | 1248 | ` * M_SQRT2` |
|       - | 1249 | ` *  Expand 	1.41421356237309504880` |
|       - | 1250 | ` */` |
|      64 | 1251 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1252 | `{` |
|      32 | 1253 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1254 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      67 | 1255 | `}` |
|       - | 1256 | `/*` |
|       - | 1257 | ` * M_SQRT3` |
|       - | 1258 | ` *  Expand 	1.73205080756887729352` |
|       - | 1259 | ` */` |
|      64 | 1260 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1261 | `{` |
|      32 | 1262 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1263 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      67 | 1264 | `}` |
|       - | 1265 | `/*` |
|       - | 1266 | ` * M_SQRT1_2` |
|       - | 1267 | ` *  Expand 	0.70710678118654752440` |
|       - | 1268 | ` */` |
|      64 | 1269 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1270 | `{` |
|      32 | 1271 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1272 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      67 | 1273 | `}` |
|       - | 1274 | `/*` |
|       - | 1275 | ` * M_LNPI` |
|       - | 1276 | ` *  Expand 	1.14472988584940017414` |
|       - | 1277 | ` */` |
|      64 | 1278 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1279 | `{` |
|      32 | 1280 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1281 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      67 | 1282 | `}` |
|       - | 1283 | `/*` |
|       - | 1284 | ` * M_EULER` |
|       - | 1285 | ` *  Expand  0.57721566490153286061` |
|       - | 1286 | ` */` |
|      64 | 1287 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1288 | `{` |
|      32 | 1289 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1290 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      67 | 1291 | `}` |
|       - | 1292 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|       - | 1293 | `/*` |
|       - | 1294 | ` * DATE_ATOM` |
|       - | 1295 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|       - | 1296 | ` */` |
|     128 | 1297 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1298 | `{` |
|      64 | 1299 | `	SXUNUSED(pUserData); /* cc warning */` |
|     131 | 1300 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|     131 | 1301 | `}` |
|       - | 1302 | `/*` |
|       - | 1303 | ` * DATE_COOKIE` |
|       - | 1304 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|       - | 1305 | ` */` |
|      64 | 1306 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1307 | `{` |
|      32 | 1308 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1309 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      67 | 1310 | `}` |
|       - | 1311 | `/*` |
|       - | 1312 | ` * DATE_ISO8601` |
|       - | 1313 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|       - | 1314 | ` */` |
|      64 | 1315 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1316 | `{` |
|      32 | 1317 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1318 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      67 | 1319 | `}` |
|       - | 1320 | `/*` |
|       - | 1321 | ` * DATE_RFC822` |
|       - | 1322 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|       - | 1323 | ` */` |
|      64 | 1324 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1325 | `{` |
|      32 | 1326 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1327 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      67 | 1328 | `}` |
|       - | 1329 | `/*` |
|       - | 1330 | ` * DATE_RFC850` |
|       - | 1331 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|       - | 1332 | ` */` |
|      64 | 1333 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1334 | `{` |
|      32 | 1335 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1336 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      67 | 1337 | `}` |
|       - | 1338 | `/*` |
|       - | 1339 | ` * DATE_RFC1036` |
|       - | 1340 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1341 | ` */` |
|      64 | 1342 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1343 | `{` |
|      32 | 1344 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1345 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      67 | 1346 | `}` |
|       - | 1347 | `/*` |
|       - | 1348 | ` * DATE_RFC1123` |
|       - | 1349 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1350 | ` */` |
|      64 | 1351 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1352 | `{` |
|      32 | 1353 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1354 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      67 | 1355 | `}` |
|       - | 1356 | `/*` |
|       - | 1357 | ` * DATE_RFC2822` |
|       - | 1358 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1359 | ` */` |
|      64 | 1360 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1361 | `{` |
|      32 | 1362 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1363 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      67 | 1364 | `}` |
|       - | 1365 | `/*` |
|       - | 1366 | ` * DATE_RSS` |
|       - | 1367 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1368 | ` */` |
|      64 | 1369 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1370 | `{` |
|      32 | 1371 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1372 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      67 | 1373 | `}` |
|       - | 1374 | `/*` |
|       - | 1375 | ` * DATE_W3C` |
|       - | 1376 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|       - | 1377 | ` */` |
|      64 | 1378 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1379 | `{` |
|      32 | 1380 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1381 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      67 | 1382 | `}` |
|       - | 1383 | `/*` |
|       - | 1384 | ` * The three format constants php added after the original set. Each is a plain` |
|       - | 1385 | ` * format STRING, so the whole of its behaviour is what date()/DateTime::format()` |
|       - | 1386 | ` * already do with those characters -- but each was a loud undefined-constant` |
|       - | 1387 | ` * fatal, which is a program that does not run rather than one that runs wrong.` |
|       - | 1388 | ` *` |
|       - | 1389 | ` * DATE_RFC7231 is the HTTP date (always GMT, so the zone letters are ESCAPED` |
|       - | 1390 | ` * rather than formatted -- php's own definition, and the reason it is not` |
|       - | 1391 | ` * DATE_RFC1123 with a T on the end). DATE_RFC3339_EXTENDED carries` |
|       - | 1392 | `` * milliseconds. DATE_ISO8601_EXPANDED uses `X`, the expanded-year field, where`` |
|       - | 1393 | `` * the plain DATE_ISO8601 uses `Y`.`` |
|       - | 1394 | ` */` |
|      64 | 1395 | `static void PH7_DATE_RFC7231_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1396 | `{` |
|      32 | 1397 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1398 | `	ph7_value_string(pVal,"D, d M Y H:i:s \\G\\M\\T",-1/*Compute length automatically*/);` |
|      67 | 1399 | `}` |
|      62 | 1400 | `static void PH7_DATE_RFC3339_EXTENDED_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1401 | `{` |
|      31 | 1402 | `	SXUNUSED(pUserData); /* cc warning */` |
|      65 | 1403 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:s.vP",-1/*Compute length automatically*/);` |
|      65 | 1404 | `}` |
|      62 | 1405 | `static void PH7_DATE_ISO8601_EXPANDED_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1406 | `{` |
|      31 | 1407 | `	SXUNUSED(pUserData); /* cc warning */` |
|      65 | 1408 | `	ph7_value_string(pVal,"X-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      65 | 1409 | `}` |
|       - | 1410 | `/*` |
|       - | 1411 | ` * FILE_TEXT / FILE_BINARY` |
|       - | 1412 | ` *  Both expand 0. php declares them for file()/file_put_contents()'s $flags and` |
|       - | 1413 | ` *  ignores them (the CLI has no text mode to select), but a program that names` |
|       - | 1414 | ` *  one still has to COMPILE, and an undefined constant is a fatal.` |
|       - | 1415 | ` */` |
|     126 | 1416 | `static void PH7_FILE_TEXT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1417 | `{` |
|     129 | 1418 | `	ph7_value_int(pVal,0);` |
|      63 | 1419 | `	SXUNUSED(pUserData);` |
|     129 | 1420 | `}` |
|       - | 1421 | `/*` |
|       - | 1422 | ` * The ENT_* values are PHP-exact (php 8.5.7). The low two bits are the quote` |
|       - | 1423 | ` * bits (1 = single, 2 = double), so ENT_QUOTES = ENT_COMPAT\|1 and` |
|       - | 1424 | ` * ENT_NOQUOTES = 0. Bits 16\|32 select the doctype (0 = HTML401, 16 = XML1,` |
|       - | 1425 | ` * 32 = XHTML, 48 = HTML5) — composites, not flags.` |
|       - | 1426 | ` */` |
|       - | 1427 | `/*` |
|       - | 1428 | ` * ENT_COMPAT` |
|       - | 1429 | ` *  Expand 2 (double-quote bit only)` |
|       - | 1430 | ` */` |
|      76 | 1431 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1432 | `{` |
|      38 | 1433 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 1434 | `	ph7_value_int(pVal,PH7_ENT_QUOTE_DOUBLE);` |
|      79 | 1435 | `}` |
|       - | 1436 | `/*` |
|       - | 1437 | ` * ENT_QUOTES` |
|       - | 1438 | ` *  Expand 3 (double\|single quote bits)` |
|       - | 1439 | ` */` |
|     212 | 1440 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1441 | `{` |
|     106 | 1442 | `	SXUNUSED(pUserData); /* cc warning */` |
|     215 | 1443 | `	ph7_value_int(pVal,PH7_ENT_QUOTES);` |
|     215 | 1444 | `}` |
|       - | 1445 | `/*` |
|       - | 1446 | ` * ENT_NOQUOTES` |
|       - | 1447 | ` *  Expand 0 (no quote bits)` |
|       - | 1448 | ` */` |
|      84 | 1449 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1450 | `{` |
|      42 | 1451 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1452 | `	ph7_value_int(pVal,0);` |
|      87 | 1453 | `}` |
|       - | 1454 | `/*` |
|       - | 1455 | ` * ENT_IGNORE` |
|       - | 1456 | ` *  Expand 4` |
|       - | 1457 | ` */` |
|      68 | 1458 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1459 | `{` |
|      34 | 1460 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1461 | `	ph7_value_int(pVal,PH7_ENT_IGNORE);` |
|      71 | 1462 | `}` |
|       - | 1463 | `/*` |
|       - | 1464 | ` * ENT_SUBSTITUTE` |
|       - | 1465 | ` *  Expand 8` |
|       - | 1466 | ` */` |
|      64 | 1467 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1468 | `{` |
|      32 | 1469 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1470 | `	ph7_value_int(pVal,PH7_ENT_SUBSTITUTE);` |
|      67 | 1471 | `}` |
|       - | 1472 | `/*` |
|       - | 1473 | ` * ENT_DISALLOWED` |
|       - | 1474 | ` *  Expand 128` |
|       - | 1475 | ` */` |
|     116 | 1476 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1477 | `{` |
|      58 | 1478 | `	SXUNUSED(pUserData); /* cc warning */` |
|     119 | 1479 | `	ph7_value_int(pVal,PH7_ENT_DISALLOWED);` |
|     119 | 1480 | `}` |
|       - | 1481 | `/*` |
|       - | 1482 | ` * ENT_HTML401` |
|       - | 1483 | ` *  Expand 0 (the default doctype)` |
|       - | 1484 | ` */` |
|      64 | 1485 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1486 | `{` |
|      32 | 1487 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1488 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML401);` |
|      67 | 1489 | `}` |
|       - | 1490 | `/*` |
|       - | 1491 | ` * ENT_XML1` |
|       - | 1492 | ` *  Expand 16` |
|       - | 1493 | ` */` |
|      72 | 1494 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1495 | `{` |
|      36 | 1496 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 1497 | `	ph7_value_int(pVal,PH7_ENT_DOC_XML1);` |
|      75 | 1498 | `}` |
|       - | 1499 | `/*` |
|       - | 1500 | ` * ENT_XHTML` |
|       - | 1501 | ` *  Expand 32` |
|       - | 1502 | ` */` |
|      68 | 1503 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1504 | `{` |
|      34 | 1505 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1506 | `	ph7_value_int(pVal,PH7_ENT_DOC_XHTML);` |
|      71 | 1507 | `}` |
|       - | 1508 | `/*` |
|       - | 1509 | ` * ENT_HTML5` |
|       - | 1510 | ` *  Expand 48 (16\|32 — a doctype composite, not a flag bit)` |
|       - | 1511 | ` */` |
|      70 | 1512 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1513 | `{` |
|      35 | 1514 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1515 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML5);` |
|      73 | 1516 | `}` |
|       - | 1517 | `/*` |
|       - | 1518 | ` * ISO-8859-1` |
|       - | 1519 | ` * ISO_8859_1` |
|       - | 1520 | ` *   Expand 1` |
|       - | 1521 | ` */` |
|     126 | 1522 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1523 | `{` |
|      63 | 1524 | `	SXUNUSED(pUserData); /* cc warning */` |
|     129 | 1525 | `	ph7_value_int(pVal,1);` |
|     129 | 1526 | `}` |
|       - | 1527 | `/*` |
|       - | 1528 | ` * UTF-8` |
|       - | 1529 | ` * UTF8` |
|       - | 1530 | ` *  Expand 2` |
|       - | 1531 | ` */` |
|     126 | 1532 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1533 | `{` |
|      63 | 1534 | `	SXUNUSED(pUserData); /* cc warning */` |
|     129 | 1535 | `	ph7_value_int(pVal,1);` |
|     129 | 1536 | `}` |
|       - | 1537 | `/*` |
|       - | 1538 | ` * HTML_ENTITIES` |
|       - | 1539 | ` *  Expand 1` |
|       - | 1540 | ` */` |
|      90 | 1541 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1542 | `{` |
|      45 | 1543 | `	SXUNUSED(pUserData); /* cc warning */` |
|      93 | 1544 | `	ph7_value_int(pVal,1);` |
|      93 | 1545 | `}` |
|       - | 1546 | `/*` |
|       - | 1547 | ` * HTML_SPECIALCHARS` |
|       - | 1548 | ` *  Expand 0 (PHP-exact)` |
|       - | 1549 | ` */` |
|      78 | 1550 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1551 | `{` |
|      39 | 1552 | `	SXUNUSED(pUserData); /* cc warning */` |
|      81 | 1553 | `	ph7_value_int(pVal,0);` |
|      81 | 1554 | `}` |
|       - | 1555 | `/*` |
|       - | 1556 | ` * PHP_URL_SCHEME.` |
|       - | 1557 | ` * Expand 0` |
|       - | 1558 | ` */` |
|      66 | 1559 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1560 | `{` |
|      33 | 1561 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1562 | `	ph7_value_int(pVal,0);` |
|      69 | 1563 | `}` |
|       - | 1564 | `/*` |
|       - | 1565 | ` * PHP_URL_HOST.` |
|       - | 1566 | ` * Expand 1` |
|       - | 1567 | ` */` |
|      68 | 1568 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1569 | `{` |
|      34 | 1570 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1571 | `	ph7_value_int(pVal,1);` |
|      71 | 1572 | `}` |
|       - | 1573 | `/*` |
|       - | 1574 | ` * PHP_URL_PORT.` |
|       - | 1575 | ` * Expand 2` |
|       - | 1576 | ` */` |
|      68 | 1577 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1578 | `{` |
|      34 | 1579 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1580 | `	ph7_value_int(pVal,2);` |
|      71 | 1581 | `}` |
|       - | 1582 | `/*` |
|       - | 1583 | ` * PHP_URL_USER.` |
|       - | 1584 | ` * Expand 3` |
|       - | 1585 | ` */` |
|      66 | 1586 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1587 | `{` |
|      33 | 1588 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1589 | `	ph7_value_int(pVal,3);` |
|      69 | 1590 | `}` |
|       - | 1591 | `/*` |
|       - | 1592 | ` * PHP_URL_PASS.` |
|       - | 1593 | ` * Expand 4` |
|       - | 1594 | ` */` |
|      66 | 1595 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1596 | `{` |
|      33 | 1597 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1598 | `	ph7_value_int(pVal,4);` |
|      69 | 1599 | `}` |
|       - | 1600 | `/*` |
|       - | 1601 | ` * PHP_URL_PATH.` |
|       - | 1602 | ` * Expand 5` |
|       - | 1603 | ` */` |
|      66 | 1604 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1605 | `{` |
|      33 | 1606 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1607 | `	ph7_value_int(pVal,5);` |
|      69 | 1608 | `}` |
|       - | 1609 | `/*` |
|       - | 1610 | ` * PHP_URL_QUERY.` |
|       - | 1611 | ` * Expand 6` |
|       - | 1612 | ` */` |
|      68 | 1613 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1614 | `{` |
|      34 | 1615 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1616 | `	ph7_value_int(pVal,6);` |
|      71 | 1617 | `}` |
|       - | 1618 | `/*` |
|       - | 1619 | ` * PHP_URL_FRAGMENT.` |
|       - | 1620 | ` * Expand 7` |
|       - | 1621 | ` */` |
|      68 | 1622 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1623 | `{` |
|      34 | 1624 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1625 | `	ph7_value_int(pVal,7);` |
|      71 | 1626 | `}` |
|       - | 1627 | `/*` |
|       - | 1628 | ` * PHP_QUERY_RFC1738` |
|       - | 1629 | ` * Expand 1` |
|       - | 1630 | ` */` |
|      64 | 1631 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1632 | `{` |
|      32 | 1633 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 1634 | `	ph7_value_int(pVal,1);` |
|      67 | 1635 | `}` |
|       - | 1636 | `/*` |
|       - | 1637 | ` * PHP_QUERY_RFC3986` |
|       - | 1638 | ` * Expand 1` |
|       - | 1639 | ` */` |
|      66 | 1640 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1641 | `{` |
|      33 | 1642 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1643 | `	ph7_value_int(pVal,2);` |
|      69 | 1644 | `}` |
|       - | 1645 | `/* php's FNM_* values (ext/standard): PATHNAME=1, NOESCAPE=2, PERIOD=4, CASEFOLD=16.` |
|       - | 1646 | ` * PHL previously had PATHNAME/NOESCAPE swapped and CASEFOLD=8; fnmatch() reads these` |
|       - | 1647 | ` * bits, so PH7_builtin_fnmatch was updated to the same values. */` |
|       - | 1648 | `/*` |
|       - | 1649 | ` * FNM_PATHNAME` |
|       - | 1650 | ` *  Expand 1 (php value)` |
|       - | 1651 | ` */` |
|      62 | 1652 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1653 | `{` |
|      31 | 1654 | `	SXUNUSED(pUserData); /* cc warning */` |
|      65 | 1655 | `	ph7_value_int(pVal,1);` |
|      65 | 1656 | `}` |
|       - | 1657 | `/*` |
|       - | 1658 | ` * FNM_NOESCAPE` |
|       - | 1659 | ` *  Expand 2 (php value)` |
|       - | 1660 | ` */` |
|     112 | 1661 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1662 | `{` |
|      56 | 1663 | `	SXUNUSED(pUserData); /* cc warning */` |
|     115 | 1664 | `	ph7_value_int(pVal,2);` |
|     115 | 1665 | `}` |
|       - | 1666 | `/*` |
|       - | 1667 | ` * FNM_PERIOD` |
|       - | 1668 | ` *  Expand 4 (php value)` |
|       - | 1669 | ` */` |
|      68 | 1670 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1671 | `{` |
|      34 | 1672 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1673 | `	ph7_value_int(pVal,4);` |
|      71 | 1674 | `}` |
|       - | 1675 | `/*` |
|       - | 1676 | ` * FNM_CASEFOLD` |
|       - | 1677 | ` *  Expand 16 (php value)` |
|       - | 1678 | ` */` |
|     108 | 1679 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1680 | `{` |
|      54 | 1681 | `	SXUNUSED(pUserData); /* cc warning */` |
|     111 | 1682 | `	ph7_value_int(pVal,16);` |
|     111 | 1683 | `}` |
|       - | 1684 | `/*` |
|       - | 1685 | ` * PATHINFO_DIRNAME` |
|       - | 1686 | ` *  Expand 1.` |
|       - | 1687 | ` */` |
|      84 | 1688 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1689 | `{` |
|      42 | 1690 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1691 | `	ph7_value_int(pVal,PH7_PATHINFO_DIRNAME);` |
|      87 | 1692 | `}` |
|       - | 1693 | `/*` |
|       - | 1694 | ` * PATHINFO_BASENAME` |
|       - | 1695 | ` *  Expand 2.` |
|       - | 1696 | ` */` |
|      84 | 1697 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1698 | `{` |
|      42 | 1699 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1700 | `	ph7_value_int(pVal,PH7_PATHINFO_BASENAME);` |
|      87 | 1701 | `}` |
|       - | 1702 | `/*` |
|       - | 1703 | ` * PATHINFO_EXTENSION` |
|       - | 1704 | ` *  Expand php's 4 (a POWER OF TWO: the components are a bitmask).` |
|       - | 1705 | ` */` |
|    8820 | 1706 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1707 | `{` |
|    4410 | 1708 | `	SXUNUSED(pUserData); /* cc warning */` |
|    8825 | 1709 | `	ph7_value_int(pVal,PH7_PATHINFO_EXTENSION);` |
|    8825 | 1710 | `}` |
|       - | 1711 | `/*` |
|       - | 1712 | ` * PATHINFO_FILENAME` |
|       - | 1713 | ` *  Expand php's 8 (a POWER OF TWO: the components are a bitmask).` |
|       - | 1714 | ` */` |
|    8806 | 1715 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1716 | `{` |
|    4403 | 1717 | `	SXUNUSED(pUserData); /* cc warning */` |
|    8811 | 1718 | `	ph7_value_int(pVal,PH7_PATHINFO_FILENAME);` |
|    8811 | 1719 | `}` |
|       - | 1720 | `/*` |
|       - | 1721 | ` * PATHINFO_ALL` |
|       - | 1722 | ` *  Expand php's 15 — the default, and the one value that answers with the ARRAY.` |
|       - | 1723 | ` */` |
|      72 | 1724 | `static void PH7_PATHINFO_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1725 | `{` |
|      36 | 1726 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 1727 | `	ph7_value_int(pVal,PH7_PATHINFO_ALL);` |
|      75 | 1728 | `}` |
|       - | 1729 | `/*` |
|       - | 1730 | ` * SEEK_SET.` |
|       - | 1731 | ` *  Expand 0` |
|       - | 1732 | ` */` |
|      82 | 1733 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1734 | `{` |
|      41 | 1735 | `	SXUNUSED(pUserData); /* cc warning */` |
|      85 | 1736 | `	ph7_value_int(pVal,0);` |
|      85 | 1737 | `}` |
|       - | 1738 | `/*` |
|       - | 1739 | ` * SEEK_CUR.` |
|       - | 1740 | ` *  Expand 1` |
|       - | 1741 | ` */` |
|      76 | 1742 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1743 | `{` |
|      38 | 1744 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 1745 | `	ph7_value_int(pVal,1);` |
|      79 | 1746 | `}` |
|       - | 1747 | `/*` |
|       - | 1748 | ` * SEEK_END.` |
|       - | 1749 | ` *  Expand 2` |
|       - | 1750 | ` */` |
|      72 | 1751 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1752 | `{` |
|      36 | 1753 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 1754 | `	ph7_value_int(pVal,2);` |
|      75 | 1755 | `}` |
|       - | 1756 | `/*` |
|       - | 1757 | ` * LOCK_SH.` |
|       - | 1758 | ` *  Expand 2` |
|       - | 1759 | ` */` |
|      76 | 1760 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1761 | `{` |
|      38 | 1762 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 1763 | `	ph7_value_int(pVal,1);` |
|      79 | 1764 | `}` |
|       - | 1765 | `/*` |
|       - | 1766 | ` * LOCK_NB.` |
|       - | 1767 | ` *  Expand 4 (php)` |
|       - | 1768 | ` */` |
|      82 | 1769 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1770 | `{` |
|      41 | 1771 | `	SXUNUSED(pUserData); /* cc warning */` |
|      85 | 1772 | `	ph7_value_int(pVal,4);` |
|      85 | 1773 | `}` |
|       - | 1774 | `/*` |
|       - | 1775 | ` * LOCK_EX.` |
|       - | 1776 | ` *  Expand 2 (php). PH7 used 1, which collided with LOCK_SH, and LOCK_UN was 0 — so` |
|       - | 1777 | ` *  flock($h, LOCK_UN) asked the stream for a SHARED lock instead of releasing one.` |
|       - | 1778 | ` */` |
|      80 | 1779 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1780 | `{` |
|      40 | 1781 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 1782 | `	ph7_value_int(pVal,2);` |
|      83 | 1783 | `}` |
|       - | 1784 | `/*` |
|       - | 1785 | ` * LOCK_UN.` |
|       - | 1786 | ` *  Expand 3 (php)` |
|       - | 1787 | ` */` |
|      74 | 1788 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1789 | `{` |
|      37 | 1790 | `	SXUNUSED(pUserData); /* cc warning */` |
|      77 | 1791 | `	ph7_value_int(pVal,3);` |
|      77 | 1792 | `}` |
|       - | 1793 | `/*` |
|       - | 1794 | ` * FILE_USE_INCLUDE_PATH` |
|       - | 1795 | ` *  Expand 0x01 (Must be a power of two)` |
|       - | 1796 | ` */` |
|      66 | 1797 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1798 | `{` |
|      33 | 1799 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1800 | `	ph7_value_int(pVal,0x1);` |
|      69 | 1801 | `}` |
|       - | 1802 | `/*` |
|       - | 1803 | ` * FILE_IGNORE_NEW_LINES` |
|       - | 1804 | ` *  Expand 0x02 (Must be a power of two)` |
|       - | 1805 | ` */` |
|      74 | 1806 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1807 | `{` |
|      37 | 1808 | `	SXUNUSED(pUserData); /* cc warning */` |
|      77 | 1809 | `	ph7_value_int(pVal,0x2);` |
|      77 | 1810 | `}` |
|       - | 1811 | `/*` |
|       - | 1812 | ` * FILE_SKIP_EMPTY_LINES` |
|       - | 1813 | ` *  Expand 0x04 (Must be a power of two)` |
|       - | 1814 | ` */` |
|      66 | 1815 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1816 | `{` |
|      33 | 1817 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1818 | `	ph7_value_int(pVal,0x4);` |
|      69 | 1819 | `}` |
|       - | 1820 | `/*` |
|       - | 1821 | ` * FILE_APPEND` |
|       - | 1822 | ` *  Expand 0x08 (Must be a power of two)` |
|       - | 1823 | ` */` |
|      66 | 1824 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1825 | `{` |
|      33 | 1826 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 1827 | `	ph7_value_int(pVal,0x08);` |
|      69 | 1828 | `}` |
|       - | 1829 | `/*` |
|       - | 1830 | ` * FILE_NO_DEFAULT_CONTEXT` |
|       - | 1831 | ` *  Expand php's 0x10. file()/file_put_contents() read it: it is what stops the` |
|       - | 1832 | `` *  `$context = null` argument from resolving to stream_context_get_default()'s`` |
|       - | 1833 | ` *  context. What a device then does with an open carrying no context is its own` |
|       - | 1834 | ` *  business — a userland wrapper's $this->context is a resource either way, in` |
|       - | 1835 | ` *  php as here — so the flag is only observable where an option is consumed.` |
|       - | 1836 | ` */` |
|      70 | 1837 | `static void PH7_FILE_NO_DEFAULT_CONTEXT_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1838 | `{` |
|      35 | 1839 | `	SXUNUSED(pUserData); /* cc warning */` |
|      74 | 1840 | `	ph7_value_int(pVal,0x10);` |
|      74 | 1841 | `}` |
|       - | 1842 | `/*` |
|       - | 1843 | ` * SCANDIR_SORT_ASCENDING` |
|       - | 1844 | ` *  Expand 0` |
|       - | 1845 | ` */` |
|    2672 | 1846 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1847 | `{` |
|    1336 | 1848 | `	SXUNUSED(pUserData); /* cc warning */` |
|    2677 | 1849 | `	ph7_value_int(pVal,0);` |
|    2677 | 1850 | `}` |
|       - | 1851 | `/*` |
|       - | 1852 | ` * SCANDIR_SORT_DESCENDING` |
|       - | 1853 | ` *  Expand 1` |
|       - | 1854 | ` */` |
|      70 | 1855 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1856 | `{` |
|      35 | 1857 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1858 | `	ph7_value_int(pVal,1);` |
|      73 | 1859 | `}` |
|       - | 1860 | `/*` |
|       - | 1861 | ` * SCANDIR_SORT_NONE` |
|       - | 1862 | ` *  Expand 2` |
|       - | 1863 | ` */` |
|    1380 | 1864 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1865 | `{` |
|     690 | 1866 | `	SXUNUSED(pUserData); /* cc warning */` |
|    1385 | 1867 | `	ph7_value_int(pVal,2);` |
|    1385 | 1868 | `}` |
|       - | 1869 | `/*` |
|       - | 1870 | ` * GLOB_MARK` |
|       - | 1871 | ` *  Expand php's 0x08 (php's own portable glob flag set)` |
|       - | 1872 | ` */` |
|     342 | 1873 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1874 | `{` |
|     171 | 1875 | `	SXUNUSED(pUserData); /* cc warning */` |
|     345 | 1876 | `	ph7_value_int(pVal,PH7_GLOB_MARK);` |
|     345 | 1877 | `}` |
|       - | 1878 | `/*` |
|       - | 1879 | ` * GLOB_NOSORT` |
|       - | 1880 | ` *  Expand php's 0x20` |
|       - | 1881 | ` */` |
|     430 | 1882 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1883 | `{` |
|     215 | 1884 | `	SXUNUSED(pUserData); /* cc warning */` |
|     435 | 1885 | `	ph7_value_int(pVal,PH7_GLOB_NOSORT);` |
|     435 | 1886 | `}` |
|       - | 1887 | `/*` |
|       - | 1888 | ` * GLOB_NOCHECK` |
|       - | 1889 | ` *  Expand php's 0x10` |
|       - | 1890 | ` */` |
|     582 | 1891 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1892 | `{` |
|     291 | 1893 | `	SXUNUSED(pUserData); /* cc warning */` |
|     587 | 1894 | `	ph7_value_int(pVal,PH7_GLOB_NOCHECK);` |
|     587 | 1895 | `}` |
|       - | 1896 | `/*` |
|       - | 1897 | ` * GLOB_NOESCAPE` |
|       - | 1898 | ` *  Expand php's 0x1000` |
|       - | 1899 | ` */` |
|      68 | 1900 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1901 | `{` |
|      34 | 1902 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1903 | `	ph7_value_int(pVal,PH7_GLOB_NOESCAPE);` |
|      71 | 1904 | `}` |
|       - | 1905 | `/*` |
|       - | 1906 | ` * GLOB_BRACE` |
|       - | 1907 | ` *  Expand php's 0x80` |
|       - | 1908 | ` */` |
|     454 | 1909 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1910 | `{` |
|     227 | 1911 | `	SXUNUSED(pUserData); /* cc warning */` |
|     459 | 1912 | `	ph7_value_int(pVal,PH7_GLOB_BRACE);` |
|     459 | 1913 | `}` |
|       - | 1914 | `/*` |
|       - | 1915 | ` * GLOB_ONLYDIR` |
|       - | 1916 | ` *  Expand php's 0x40000000` |
|       - | 1917 | ` */` |
|     610 | 1918 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1919 | `{` |
|     305 | 1920 | `	SXUNUSED(pUserData); /* cc warning */` |
|     614 | 1921 | `	ph7_value_int(pVal,PH7_GLOB_ONLYDIR);` |
|     614 | 1922 | `}` |
|       - | 1923 | `/*` |
|       - | 1924 | ` * GLOB_ERR` |
|       - | 1925 | ` *  Expand php's 0x04` |
|       - | 1926 | ` */` |
|      70 | 1927 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1928 | `{` |
|      35 | 1929 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1930 | `	ph7_value_int(pVal,PH7_GLOB_ERR);` |
|      73 | 1931 | `}` |
|       - | 1932 | `/*` |
|       - | 1933 | ` * GLOB_AVAILABLE_FLAGS` |
|       - | 1934 | ` *  Expand the OR of every glob flag php's portable glob accepts — the mask` |
|       - | 1935 | ` *  glob() itself validates against (1073746108 on every platform, since the` |
|       - | 1936 | ` *  GLOB_* values are php 8.5's own portable set).` |
|       - | 1937 | ` */` |
|     456 | 1938 | `static void PH7_GLOB_AVAILABLE_FLAGS_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1939 | `{` |
|     228 | 1940 | `	SXUNUSED(pUserData); /* cc warning */` |
|     461 | 1941 | `	ph7_value_int(pVal,PH7_GLOB_ERR\|PH7_GLOB_MARK\|PH7_GLOB_NOCHECK\|PH7_GLOB_NOSORT` |
|       - | 1942 | `		\|PH7_GLOB_BRACE\|PH7_GLOB_NOESCAPE\|PH7_GLOB_ONLYDIR);` |
|     461 | 1943 | `}` |
|       - | 1944 | `/*` |
|       - | 1945 | ` * STDIN` |
|       - | 1946 | ` *  Expand the STDIN handle as a resource.` |
|       - | 1947 | ` */` |
|     144 | 1948 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1949 | `{` |
|     147 | 1950 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 1951 | `	void *pResource;` |
|     147 | 1952 | `	pResource = PH7_ExportStdin(pVm);` |
|     147 | 1953 | `	ph7_value_resource(pVal,pResource);` |
|     147 | 1954 | `}` |
|       - | 1955 | `/*` |
|       - | 1956 | ` * STDOUT` |
|       - | 1957 | ` *   Expand the STDOUT handle as a resource.` |
|       - | 1958 | ` */` |
|     134 | 1959 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1960 | `{` |
|     138 | 1961 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 1962 | `	void *pResource;` |
|     138 | 1963 | `	pResource = PH7_ExportStdout(pVm);` |
|     138 | 1964 | `	ph7_value_resource(pVal,pResource);` |
|     138 | 1965 | `}` |
|       - | 1966 | `/*` |
|       - | 1967 | ` * STDERR` |
|       - | 1968 | ` *  Expand the STDERR handle as a resource.` |
|       - | 1969 | ` */` |
|     136 | 1970 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1971 | `{` |
|     140 | 1972 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 1973 | `	void *pResource;` |
|     140 | 1974 | `	pResource = PH7_ExportStderr(pVm);` |
|     140 | 1975 | `	ph7_value_resource(pVal,pResource);` |
|     140 | 1976 | `}` |
|       - | 1977 | `/*` |
|       - | 1978 | ` * INI_SCANNER_NORMAL` |
|       - | 1979 | ` *   Expand php's 0` |
|       - | 1980 | ` */` |
|      68 | 1981 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1982 | `{` |
|      34 | 1983 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1984 | `	ph7_value_int(pVal,PH7_INI_SCANNER_NORMAL);` |
|      71 | 1985 | `}` |
|       - | 1986 | `/*` |
|       - | 1987 | ` * INI_SCANNER_RAW` |
|       - | 1988 | ` *   Expand php's 1` |
|       - | 1989 | ` */` |
|      70 | 1990 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1991 | `{` |
|      35 | 1992 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1993 | `	ph7_value_int(pVal,PH7_INI_SCANNER_RAW);` |
|      73 | 1994 | `}` |
|       - | 1995 | `/*` |
|       - | 1996 | ` * INI_SCANNER_TYPED` |
|       - | 1997 | ` *   Expand 2 (php's value)` |
|       - | 1998 | ` */` |
|      70 | 1999 | `static void PH7_INI_SCANNER_TYPED_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2000 | `{` |
|      35 | 2001 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 2002 | `	ph7_value_int(pVal,PH7_INI_SCANNER_TYPED);` |
|      73 | 2003 | `}` |
|       - | 2004 | `/*` |
|       - | 2005 | ` * EXTR_OVERWRITE` |
|       - | 2006 | ` *   Expand 0 (php's enum value; see PH7_EXTR_* in ph7int.h)` |
|       - | 2007 | ` */` |
|      76 | 2008 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2009 | `{` |
|      38 | 2010 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 2011 | `	ph7_value_int(pVal,PH7_EXTR_OVERWRITE);` |
|      79 | 2012 | `}` |
|       - | 2013 | `/*` |
|       - | 2014 | ` * EXTR_SKIP` |
|       - | 2015 | ` *   Expand 1` |
|       - | 2016 | ` */` |
|      78 | 2017 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2018 | `{` |
|      39 | 2019 | `	SXUNUSED(pUserData); /* cc warning */` |
|      81 | 2020 | `	ph7_value_int(pVal,PH7_EXTR_SKIP);` |
|      81 | 2021 | `}` |
|       - | 2022 | `/*` |
|       - | 2023 | ` * EXTR_PREFIX_SAME` |
|       - | 2024 | ` *   Expand 2` |
|       - | 2025 | ` */` |
|     100 | 2026 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2027 | `{` |
|      50 | 2028 | `	SXUNUSED(pUserData); /* cc warning */` |
|     103 | 2029 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_SAME);` |
|     103 | 2030 | `}` |
|       - | 2031 | `/*` |
|       - | 2032 | ` * EXTR_PREFIX_ALL` |
|       - | 2033 | ` *   Expand 3` |
|       - | 2034 | ` */` |
|      86 | 2035 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2036 | `{` |
|      43 | 2037 | `	SXUNUSED(pUserData); /* cc warning */` |
|      89 | 2038 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_ALL);` |
|      89 | 2039 | `}` |
|       - | 2040 | `/*` |
|       - | 2041 | ` * EXTR_PREFIX_INVALID` |
|       - | 2042 | ` *   Expand 4` |
|       - | 2043 | ` */` |
|      70 | 2044 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2045 | `{` |
|      35 | 2046 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 2047 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_INVALID);` |
|      73 | 2048 | `}` |
|       - | 2049 | `/*` |
|       - | 2050 | ` * EXTR_IF_EXISTS` |
|       - | 2051 | ` *   Expand 6 (php orders IF_EXISTS after PREFIX_IF_EXISTS)` |
|       - | 2052 | ` */` |
|      78 | 2053 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2054 | `{` |
|      39 | 2055 | `	SXUNUSED(pUserData); /* cc warning */` |
|      82 | 2056 | `	ph7_value_int(pVal,PH7_EXTR_IF_EXISTS);` |
|      82 | 2057 | `}` |
|       - | 2058 | `/*` |
|       - | 2059 | ` * EXTR_REFS` |
|       - | 2060 | ` *   Expand 256 (the bit that rides above the mode: bind by REFERENCE)` |
|       - | 2061 | ` */` |
|      76 | 2062 | `static void PH7_EXTR_REFS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2063 | `{` |
|      38 | 2064 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 2065 | `	ph7_value_int(pVal,PH7_EXTR_REFS);` |
|      79 | 2066 | `}` |
|       - | 2067 | `/*` |
|       - | 2068 | ` * EXTR_PREFIX_IF_EXISTS` |
|       - | 2069 | ` *   Expand 5` |
|       - | 2070 | ` */` |
|      82 | 2071 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2072 | `{` |
|      41 | 2073 | `	SXUNUSED(pUserData); /* cc warning */` |
|      85 | 2074 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_IF_EXISTS);` |
|      85 | 2075 | `}` |
|       - | 2076 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|       - | 2077 | `/*` |
|       - | 2078 | ` * HASH_HMAC.` |
|       - | 2079 | ` *   php's one hash_init() flag. Declared with the hash extension it belongs` |
|       - | 2080 | ` *   to, so a build without that extension has no constant either.` |
|       - | 2081 | ` */` |
|      74 | 2082 | `static void PH7_HASH_HMAC_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2083 | `{` |
|      37 | 2084 | `	SXUNUSED(pUserData); /* cc warning */` |
|      77 | 2085 | `	ph7_value_int(pVal,PH7_HASH_HMAC);` |
|      77 | 2086 | `}` |
|       - | 2087 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|       - | 2088 | `/*` |
|       - | 2089 | ` * ICONV_* — what the converter IS, and iconv_mime_decode()'s $mode bits.` |
|       - | 2090 | `` * php reports the C library behind its extension here (`glibc`, `libiconv`);`` |
|       - | 2091 | ` * PHL converts with its own code so that a Windows build answers what a POSIX` |
|       - | 2092 | ` * one does, and says so — the constants exist to be READ, and a program that` |
|       - | 2093 | ` * branches on them has to see something true.` |
|       - | 2094 | ` */` |
|      64 | 2095 | `static void PH7_ICONV_IMPL_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2096 | `{` |
|      32 | 2097 | `	SXUNUSED(pUnused);` |
|      67 | 2098 | `	ph7_value_string(pVal,"PHL",(int)sizeof("PHL")-1);` |
|      67 | 2099 | `}` |
|      64 | 2100 | `static void PH7_ICONV_VERSION_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2101 | `{` |
|      32 | 2102 | `	SXUNUSED(pUnused);` |
|      67 | 2103 | `	ph7_value_string(pVal,PH7_VERSION,(int)sizeof(PH7_VERSION)-1);` |
|      67 | 2104 | `}` |
|      66 | 2105 | `static void PH7_ICONV_MIME_DECODE_STRICT_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2106 | `{` |
|      33 | 2107 | `	SXUNUSED(pUnused);` |
|      69 | 2108 | `	ph7_value_int(pVal,1);` |
|      69 | 2109 | `}` |
|      66 | 2110 | `static void PH7_ICONV_MIME_DECODE_CONTINUE_ON_ERROR_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2111 | `{` |
|      33 | 2112 | `	SXUNUSED(pUnused);` |
|      69 | 2113 | `	ph7_value_int(pVal,2);` |
|      69 | 2114 | `}` |
|       - | 2115 | `/*` |
|       - | 2116 | ` * JSON_HEX_TAG.` |
|       - | 2117 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|       - | 2118 | ` */` |
|      68 | 2119 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2120 | `{` |
|      34 | 2121 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 2122 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      71 | 2123 | `}` |
|       - | 2124 | `/*` |
|       - | 2125 | ` * JSON_HEX_AMP.` |
|       - | 2126 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|       - | 2127 | ` */` |
|      68 | 2128 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2129 | `{` |
|      34 | 2130 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 2131 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      71 | 2132 | `}` |
|       - | 2133 | `/*` |
|       - | 2134 | ` * JSON_HEX_APOS.` |
|       - | 2135 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|       - | 2136 | ` */` |
|      68 | 2137 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2138 | `{` |
|      34 | 2139 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 2140 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      71 | 2141 | `}` |
|       - | 2142 | `/*` |
|       - | 2143 | ` * JSON_HEX_QUOT.` |
|       - | 2144 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|       - | 2145 | ` */` |
|      68 | 2146 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2147 | `{` |
|      34 | 2148 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 2149 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      71 | 2150 | `}` |
|       - | 2151 | `/*` |
|       - | 2152 | ` * JSON_FORCE_OBJECT.` |
|       - | 2153 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|       - | 2154 | ` */` |
|      68 | 2155 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2156 | `{` |
|      34 | 2157 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 2158 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      71 | 2159 | `}` |
|       - | 2160 | `/*` |
|       - | 2161 | ` * JSON_NUMERIC_CHECK.` |
|       - | 2162 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|       - | 2163 | ` */` |
|      72 | 2164 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2165 | `{` |
|      36 | 2166 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 2167 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      75 | 2168 | `}` |
|       - | 2169 | `/*` |
|       - | 2170 | ` * JSON_BIGINT_AS_STRING.` |
|       - | 2171 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|       - | 2172 | ` */` |
|      76 | 2173 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2174 | `{` |
|      38 | 2175 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 2176 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      79 | 2177 | `}` |
|       - | 2178 | `/*` |
|       - | 2179 | ` * JSON_PARTIAL_OUTPUT_ON_ERROR.` |
|       - | 2180 | ` *   Expand the value of JSON_PARTIAL_OUTPUT_ON_ERROR defined in ph7Int.h.` |
|       - | 2181 | ` */` |
|      86 | 2182 | `static void PH7_JSON_PARTIAL_OUTPUT_ON_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2183 | `{` |
|      43 | 2184 | `	SXUNUSED(pUserData); /* cc warning */` |
|      89 | 2185 | `	ph7_value_int(pVal,JSON_PARTIAL_OUTPUT_ON_ERROR);` |
|      89 | 2186 | `}` |
|       - | 2187 | `/*` |
|       - | 2188 | ` * JSON_PRESERVE_ZERO_FRACTION.` |
|       - | 2189 | ` *   Expand the value of JSON_PRESERVE_ZERO_FRACTION defined in ph7Int.h.` |
|       - | 2190 | ` */` |
|      76 | 2191 | `static void PH7_JSON_PRESERVE_ZERO_FRACTION_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2192 | `{` |
|      38 | 2193 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 2194 | `	ph7_value_int(pVal,JSON_PRESERVE_ZERO_FRACTION);` |
|      79 | 2195 | `}` |
|       - | 2196 | `/*` |
|       - | 2197 | ` * JSON_OBJECT_AS_ARRAY.` |
|       - | 2198 | ` *   Expand the value of JSON_OBJECT_AS_ARRAY defined in ph7Int.h.` |
|       - | 2199 | ` */` |
|      72 | 2200 | `static void PH7_JSON_OBJECT_AS_ARRAY_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2201 | `{` |
|      36 | 2202 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 2203 | `	ph7_value_int(pVal,JSON_OBJECT_AS_ARRAY);` |
|      75 | 2204 | `}` |
|       - | 2205 | `/*` |
|       - | 2206 | ` * JSON_PRETTY_PRINT.` |
|       - | 2207 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|       - | 2208 | ` */` |
|      76 | 2209 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2210 | `{` |
|      38 | 2211 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 2212 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      79 | 2213 | `}` |
|       - | 2214 | `/*` |
|       - | 2215 | ` * JSON_UNESCAPED_SLASHES.` |
|       - | 2216 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|       - | 2217 | ` */` |
|      72 | 2218 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2219 | `{` |
|      36 | 2220 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 2221 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      75 | 2222 | `}` |
|       - | 2223 | `/*` |
|       - | 2224 | ` * JSON_UNESCAPED_UNICODE.` |
|       - | 2225 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|       - | 2226 | ` */` |
|      84 | 2227 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2228 | `{` |
|      42 | 2229 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 2230 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|      87 | 2231 | `}` |
|       - | 2232 | `/*` |
|       - | 2233 | ` * JSON_UNESCAPED_LINE_TERMINATORS.` |
|       - | 2234 | ` *   Expand the value of JSON_UNESCAPED_LINE_TERMINATORS defined in ph7Int.h.` |
|       - | 2235 | ` */` |
|      68 | 2236 | `static void PH7_JSON_UNESCAPED_LINE_TERMINATORS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2237 | `{` |
|      34 | 2238 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 2239 | `	ph7_value_int(pVal,JSON_UNESCAPED_LINE_TERMINATORS);` |
|      71 | 2240 | `}` |
|       - | 2241 | `/*` |
|       - | 2242 | ` * JSON_INVALID_UTF8_IGNORE.` |
|       - | 2243 | ` *   Expand the value of JSON_INVALID_UTF8_IGNORE defined in ph7Int.h.` |
|       - | 2244 | ` */` |
|      88 | 2245 | `static void PH7_JSON_INVALID_UTF8_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2246 | `{` |
|      44 | 2247 | `	SXUNUSED(pUserData); /* cc warning */` |
|      91 | 2248 | `	ph7_value_int(pVal,JSON_INVALID_UTF8_IGNORE);` |
|      91 | 2249 | `}` |
|       - | 2250 | `/*` |
|       - | 2251 | ` * JSON_INVALID_UTF8_SUBSTITUTE.` |
|       - | 2252 | ` *   Expand the value of JSON_INVALID_UTF8_SUBSTITUTE defined in ph7Int.h.` |
|       - | 2253 | ` */` |
|      92 | 2254 | `static void PH7_JSON_INVALID_UTF8_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2255 | `{` |
|      46 | 2256 | `	SXUNUSED(pUserData); /* cc warning */` |
|      95 | 2257 | `	ph7_value_int(pVal,JSON_INVALID_UTF8_SUBSTITUTE);` |
|      95 | 2258 | `}` |
|       - | 2259 | `/*` |
|       - | 2260 | ` * JSON_THROW_ON_ERROR.` |
|       - | 2261 | ` *   Expand the value of JSON_THROW_ON_ERROR defined in ph7Int.h.` |
|       - | 2262 | ` */` |
|      82 | 2263 | `static void PH7_JSON_THROW_ON_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2264 | `{` |
|      41 | 2265 | `	SXUNUSED(pUserData); /* cc warning */` |
|      86 | 2266 | `	ph7_value_int(pVal,JSON_THROW_ON_ERROR);` |
|      86 | 2267 | `}` |
|       - | 2268 | `/*` |
|       - | 2269 | ` * JSON_ERROR_NONE.` |
|       - | 2270 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|       - | 2271 | ` */` |
|      66 | 2272 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2273 | `{` |
|      33 | 2274 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 2275 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      69 | 2276 | `}` |
|       - | 2277 | `/*` |
|       - | 2278 | ` * JSON_ERROR_DEPTH.` |
|       - | 2279 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|       - | 2280 | ` */` |
|      64 | 2281 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2282 | `{` |
|      32 | 2283 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 2284 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      67 | 2285 | `}` |
|       - | 2286 | `/*` |
|       - | 2287 | ` * JSON_ERROR_STATE_MISMATCH.` |
|       - | 2288 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|       - | 2289 | ` */` |
|      64 | 2290 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2291 | `{` |
|      32 | 2292 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 2293 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      67 | 2294 | `}` |
|       - | 2295 | `/*` |
|       - | 2296 | ` * JSON_ERROR_CTRL_CHAR.` |
|       - | 2297 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|       - | 2298 | ` */` |
|      64 | 2299 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2300 | `{` |
|      32 | 2301 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 2302 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      67 | 2303 | `}` |
|       - | 2304 | `/*` |
|       - | 2305 | ` * JSON_ERROR_SYNTAX.` |
|       - | 2306 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|       - | 2307 | ` */` |
|      66 | 2308 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2309 | `{` |
|      33 | 2310 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 2311 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      69 | 2312 | `}` |
|       - | 2313 | `/*` |
|       - | 2314 | ` * JSON_ERROR_UTF8.` |
|       - | 2315 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|       - | 2316 | ` */` |
|      64 | 2317 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2318 | `{` |
|      32 | 2319 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 2320 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      67 | 2321 | `}` |
|       - | 2322 | `/*` |
|       - | 2323 | ` * JSON_ERROR_RECURSION.` |
|       - | 2324 | ` *   Expand the value of JSON_ERROR_RECURSION defined in ph7Int.h.` |
|       - | 2325 | ` */` |
|      64 | 2326 | `static void PH7_JSON_ERROR_RECURSION_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2327 | `{` |
|      32 | 2328 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 2329 | `	ph7_value_int(pVal,JSON_ERROR_RECURSION);` |
|      67 | 2330 | `}` |
|       - | 2331 | `/*` |
|       - | 2332 | ` * JSON_ERROR_UNSUPPORTED_TYPE.` |
|       - | 2333 | ` *   Expand the value of JSON_ERROR_UNSUPPORTED_TYPE defined in ph7Int.h.` |
|       - | 2334 | ` */` |
|      64 | 2335 | `static void PH7_JSON_ERROR_UNSUPPORTED_TYPE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2336 | `{` |
|      32 | 2337 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 2338 | `	ph7_value_int(pVal,JSON_ERROR_UNSUPPORTED_TYPE);` |
|      67 | 2339 | `}` |
|       - | 2340 | `/*` |
|       - | 2341 | ` * JSON_ERROR_INVALID_PROPERTY_NAME.` |
|       - | 2342 | ` *   Expand the value of JSON_ERROR_INVALID_PROPERTY_NAME defined in ph7Int.h.` |
|       - | 2343 | ` */` |
|      64 | 2344 | `static void PH7_JSON_ERROR_INVALID_PROPERTY_NAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2345 | `{` |
|      32 | 2346 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 2347 | `	ph7_value_int(pVal,JSON_ERROR_INVALID_PROPERTY_NAME);` |
|      67 | 2348 | `}` |
|       - | 2349 | `/*` |
|       - | 2350 | ` * JSON_ERROR_UTF16.` |
|       - | 2351 | ` *   Expand the value of JSON_ERROR_UTF16 defined in ph7Int.h.` |
|       - | 2352 | ` */` |
|      66 | 2353 | `static void PH7_JSON_ERROR_UTF16_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2354 | `{` |
|      33 | 2355 | `	SXUNUSED(pUserData); /* cc warning */` |
|      69 | 2356 | `	ph7_value_int(pVal,JSON_ERROR_UTF16);` |
|      69 | 2357 | `}` |
|       - | 2358 | `/*` |
|       - | 2359 | ` * JSON_ERROR_NON_BACKED_ENUM.` |
|       - | 2360 | ` *   Expand the value of JSON_ERROR_NON_BACKED_ENUM defined in ph7Int.h (php 8.1).` |
|       - | 2361 | ` */` |
|      62 | 2362 | `static void PH7_JSON_ERROR_NON_BACKED_ENUM_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2363 | `{` |
|      31 | 2364 | `	SXUNUSED(pUserData); /* cc warning */` |
|      65 | 2365 | `	ph7_value_int(pVal,JSON_ERROR_NON_BACKED_ENUM);` |
|      65 | 2366 | `}` |
|       - | 2367 | `/*` |
|       - | 2368 | ` * JSON_ERROR_INF_OR_NAN.` |
|       - | 2369 | ` *   Expand the value of JSON_ERROR_INF_OR_NAN defined in ph7Int.h.` |
|       - | 2370 | ` */` |
|      64 | 2371 | `static void PH7_JSON_ERROR_INF_OR_NAN_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2372 | `{` |
|      32 | 2373 | `	SXUNUSED(pUserData); /* cc warning */` |
|      67 | 2374 | `	ph7_value_int(pVal,JSON_ERROR_INF_OR_NAN);` |
|      67 | 2375 | `}` |
|       - | 2376 | `/*` |
|       - | 2377 | ` * __CLASS__` |
|       - | 2378 | ` *  The current class name, or the EMPTY STRING outside any class — php answers "",` |
|       - | 2379 | `` *  not null (`__CLASS__ === ""` is true in global scope). `self` keeps its own`` |
|       - | 2380 | ` *  expander below because php treats IT differently outside a class scope.` |
|       - | 2381 | ` */` |
|      78 | 2382 | `static void PH7_class_magic_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2383 | `{` |
|      81 | 2384 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 2385 | `	ph7_class *pClass;` |
|       - | 2386 | `	/* php flattens a trait into the class that used it, so __CLASS__ inside a trait method` |
|       - | 2387 | `	 * is THAT class (where __TRAIT__ and __METHOD__ stay the trait's — php's own asymmetry). */` |
|      81 | 2388 | `	pClass = PH7_VmPeekSelfClass(pVm);` |
|      81 | 2389 | `	if( pClass == 0 ){` |
|      67 | 2390 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      32 | 2391 | `	}` |
|      81 | 2392 | `	if( pClass ){` |
|      15 | 2393 | `		SyString *pName = &pClass->sName;` |
|      15 | 2394 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|       8 | 2395 | `	}else{` |
|      67 | 2396 | `		ph7_value_string(pVal,"",0);` |
|       - | 2397 | `	}` |
|      81 | 2398 | `}` |
|       - | 2399 |  |
|       - | 2400 | `/*` |
|       - | 2401 | ` * PASSWORD_BCRYPT / PASSWORD_DEFAULT` |
|       - | 2402 | ` *  The bcrypt algorithm identifier (PHP 7.4+ exposes these as the string "2y").` |
|       - | 2403 | ` *  PASSWORD_DEFAULT tracks the recommended default, currently bcrypt.` |
|       - | 2404 | ` */` |
|     162 | 2405 | `static void PH7_PASSWORD_BCRYPT_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2406 | `{` |
|      81 | 2407 | `	SXUNUSED(pUnused);` |
|     165 | 2408 | `	ph7_value_string(pVal,"2y",(int)sizeof("2y")-1);` |
|     165 | 2409 | `}` |
|       - | 2410 | `/*` |
|       - | 2411 | ` * PASSWORD_BCRYPT_DEFAULT_COST` |
|       - | 2412 | ` *  The default bcrypt work factor used by password_hash() (currently 12).` |
|       - | 2413 | ` */` |
|      64 | 2414 | `static void PH7_PASSWORD_COST_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2415 | `{` |
|      32 | 2416 | `	SXUNUSED(pUnused);` |
|      67 | 2417 | `	ph7_value_int(pVal,12);` |
|      67 | 2418 | `}` |
|       - | 2419 | `/*` |
|       - | 2420 | ` * PASSWORD_ARGON2I / PASSWORD_ARGON2ID and the three argon2 option defaults` |
|       - | 2421 | ` * password_hash() reads when $options omits them.` |
|       - | 2422 | ` */` |
|      68 | 2423 | `static void PH7_PASSWORD_ARGON2I_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2424 | `{` |
|      34 | 2425 | `	SXUNUSED(pUnused);` |
|      71 | 2426 | `	ph7_value_string(pVal,"argon2i",(int)sizeof("argon2i")-1);` |
|      71 | 2427 | `}` |
|      86 | 2428 | `static void PH7_PASSWORD_ARGON2ID_Const(ph7_value *pVal,void *pUnused)` |
|       4 | 2429 | `{` |
|      43 | 2430 | `	SXUNUSED(pUnused);` |
|      90 | 2431 | `	ph7_value_string(pVal,"argon2id",(int)sizeof("argon2id")-1);` |
|      90 | 2432 | `}` |
|      64 | 2433 | `static void PH7_ARGON2_MEM_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2434 | `{` |
|      32 | 2435 | `	SXUNUSED(pUnused);` |
|      67 | 2436 | `	ph7_value_int(pVal,65536);` |
|      67 | 2437 | `}` |
|      64 | 2438 | `static void PH7_ARGON2_TIME_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2439 | `{` |
|      32 | 2440 | `	SXUNUSED(pUnused);` |
|      67 | 2441 | `	ph7_value_int(pVal,4);` |
|      67 | 2442 | `}` |
|      64 | 2443 | `static void PH7_ARGON2_THREADS_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2444 | `{` |
|      32 | 2445 | `	SXUNUSED(pUnused);` |
|      67 | 2446 | `	ph7_value_int(pVal,1);` |
|      67 | 2447 | `}` |
|       - | 2448 | `/*` |
|       - | 2449 | ` * CRYPT_* — the crypt() capability flags. Every scheme is compiled in, so all` |
|       - | 2450 | ` * six are 1, and CRYPT_SALT_LENGTH is php's 123 (the longest setting string a` |
|       - | 2451 | ` * SHA-512-crypt with an explicit rounds count can need).` |
|       - | 2452 | ` */` |
|       - | 2453 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|     384 | 2454 | `static void PH7_CRYPT_ONE_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2455 | `{` |
|     192 | 2456 | `	SXUNUSED(pUnused);` |
|     387 | 2457 | `	ph7_value_int(pVal,1);` |
|     387 | 2458 | `}` |
|      64 | 2459 | `static void PH7_CRYPT_SALT_LENGTH_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2460 | `{` |
|      32 | 2461 | `	SXUNUSED(pUnused);` |
|      67 | 2462 | `	ph7_value_int(pVal,123);` |
|      67 | 2463 | `}` |
|       - | 2464 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|       - | 2465 | `/*` |
|       - | 2466 | ` * filter_var() filter and flag identifiers (the ext/filter constants). Values` |
|       - | 2467 | ` * match PHP 8.5. One tiny int-returning callback per constant, generated by a` |
|       - | 2468 | ` * local macro to keep the ~25 near-identical definitions DRY.` |
|       - | 2469 | ` */` |
|       - | 2470 | `#define PH7_FILTER_INT_CONST(Name,Val) \` |
|       - | 2471 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|       - | 2472 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|       - | 2473 | `	}` |
|      84 | 2474 | `PH7_FILTER_INT_CONST(FILTER_DEFAULT,516)` |
|      81 | 2475 | `PH7_FILTER_INT_CONST(FILTER_UNSAFE_RAW,516)` |
|     227 | 2476 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_INT,257)` |
|     162 | 2477 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_BOOLEAN,258)` |
|     200 | 2478 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_FLOAT,259)` |
|      72 | 2479 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_REGEXP,272)` |
|     209 | 2480 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_DOMAIN,277)` |
|     387 | 2481 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_URL,273)` |
|     267 | 2482 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_EMAIL,274)` |
|     628 | 2483 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_IP,275)` |
|     100 | 2484 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_MAC,276)` |
|      92 | 2485 | `PH7_FILTER_INT_CONST(FILTER_CALLBACK,1024)` |
|      94 | 2486 | `PH7_FILTER_INT_CONST(FILTER_THROW_ON_FAILURE,268435456)` |
|      73 | 2487 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_ENCODED,514)` |
|      71 | 2488 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_ADD_SLASHES,523)` |
|      70 | 2489 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_INT,519)` |
|      69 | 2490 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_FLOAT,520)` |
|      79 | 2491 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_SPECIAL_CHARS,515)` |
|      89 | 2492 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_FULL_SPECIAL_CHARS,522)` |
|      67 | 2493 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_EMAIL,517)` |
|      67 | 2494 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_URL,518)` |
|      67 | 2495 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_OCTAL,1)` |
|      67 | 2496 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_HEX,2)` |
|      75 | 2497 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_LOW,4)` |
|      71 | 2498 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_HIGH,8)` |
|      71 | 2499 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_LOW,16)` |
|      69 | 2500 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_HIGH,32)` |
|      69 | 2501 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_AMP,64)` |
|      72 | 2502 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_ENCODE_QUOTES,128)` |
|      67 | 2503 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NONE,0)` |
|      75 | 2504 | `PH7_FILTER_INT_CONST(FILTER_FLAG_EMPTY_STRING_NULL,256)` |
|      69 | 2505 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_BACKTICK,512)` |
|      67 | 2506 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_FRACTION,4096)` |
|     102 | 2507 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_THOUSAND,8192)` |
|      67 | 2508 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_SCIENTIFIC,16384)` |
|      73 | 2509 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV4,1048576)` |
|      69 | 2510 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV6,2097152)` |
|     218 | 2511 | `PH7_FILTER_INT_CONST(FILTER_FLAG_PATH_REQUIRED,262144)` |
|     218 | 2512 | `PH7_FILTER_INT_CONST(FILTER_FLAG_QUERY_REQUIRED,524288)` |
|     129 | 2513 | `PH7_FILTER_INT_CONST(FILTER_FLAG_HOSTNAME,1048576)` |
|     150 | 2514 | `PH7_FILTER_INT_CONST(FILTER_FLAG_EMAIL_UNICODE,1048576)` |
|     277 | 2515 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_RES_RANGE,4194304)` |
|     281 | 2516 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_PRIV_RANGE,8388608)` |
|     173 | 2517 | `PH7_FILTER_INT_CONST(FILTER_FLAG_GLOBAL_RANGE,536870912)` |
|      92 | 2518 | `PH7_FILTER_INT_CONST(FILTER_REQUIRE_ARRAY,16777216)` |
|      71 | 2519 | `PH7_FILTER_INT_CONST(FILTER_REQUIRE_SCALAR,33554432)` |
|      75 | 2520 | `PH7_FILTER_INT_CONST(FILTER_FORCE_ARRAY,67108864)` |
|      89 | 2521 | `PH7_FILTER_INT_CONST(FILTER_NULL_ON_FAILURE,134217728)` |
|       - | 2522 | `/* filter_input() source selectors (php values; SESSION/REQUEST are undefined in 8.5) */` |
|      70 | 2523 | `PH7_FILTER_INT_CONST(INPUT_POST,0)` |
|      79 | 2524 | `PH7_FILTER_INT_CONST(INPUT_GET,1)` |
|      68 | 2525 | `PH7_FILTER_INT_CONST(INPUT_COOKIE,2)` |
|      68 | 2526 | `PH7_FILTER_INT_CONST(INPUT_ENV,4)` |
|      86 | 2527 | `PH7_FILTER_INT_CONST(INPUT_SERVER,5)` |
|       - | 2528 | `/*` |
|       - | 2529 | ` * Table of built-in constants.` |
|       - | 2530 | ` */` |
|       - | 2531 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|       - | 2532 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|       - | 2533 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|       - | 2534 | `	{"__PH7__",              PH7_VER_Const      },` |
|       - | 2535 | `	{"PHP_VERSION",          PH7_PHPVerConst    },` |
|       - | 2536 | `	{"PHP_MAJOR_VERSION",    PH7_PHPMajorConst  },` |
|       - | 2537 | `	{"PHP_MINOR_VERSION",    PH7_PHPMinorConst  },` |
|       - | 2538 | `	{"PHP_RELEASE_VERSION",  PH7_PHPReleaseConst},` |
|       - | 2539 | `	{"PHP_EXTRA_VERSION",    PH7_PHPExtraConst  },` |
|       - | 2540 | `	{"PHP_VERSION_ID",       PH7_PHPVerIdConst  },` |
|       - | 2541 | `	{"PHP_OS",               PH7_OS_Const       },` |
|       - | 2542 | `	{"PHP_OS_FAMILY",        PH7_OS_FAMILY_Const},` |
|       - | 2543 | `	{"PHP_SAPI",             PH7_SAPI_Const     },` |
|       - | 2544 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|       - | 2545 | `	{"PHP_SESSION_DISABLED", PH7_PHP_SESSION_DISABLED_Const },` |
|       - | 2546 | `	{"PHP_SESSION_NONE",     PH7_PHP_SESSION_NONE_Const },` |
|       - | 2547 | `	{"PHP_SESSION_ACTIVE",   PH7_PHP_SESSION_ACTIVE_Const },` |
|       - | 2548 | `	{"INI_USER",             PH7_INI_USER_Const },` |
|       - | 2549 | `	{"INI_PERDIR",           PH7_INI_PERDIR_Const },` |
|       - | 2550 | `	{"INI_SYSTEM",           PH7_INI_SYSTEM_Const },` |
|       - | 2551 | `	{"INI_ALL",              PH7_INI_ALL_Const },` |
|       - | 2552 | `	{"MB_CASE_UPPER",        PH7_MB_CASE_UPPER_Const },` |
|       - | 2553 | `	{"MB_CASE_LOWER",        PH7_MB_CASE_LOWER_Const },` |
|       - | 2554 | `	{"MB_CASE_TITLE",        PH7_MB_CASE_TITLE_Const },` |
|       - | 2555 | `	{"PASSWORD_BCRYPT",      PH7_PASSWORD_BCRYPT_Const },` |
|       - | 2556 | `	{"PASSWORD_DEFAULT",     PH7_PASSWORD_BCRYPT_Const },` |
|       - | 2557 | `	{"PASSWORD_BCRYPT_DEFAULT_COST", PH7_PASSWORD_COST_Const },` |
|       - | 2558 | `	{"PASSWORD_ARGON2I",     PH7_PASSWORD_ARGON2I_Const },` |
|       - | 2559 | `	{"PASSWORD_ARGON2ID",    PH7_PASSWORD_ARGON2ID_Const },` |
|       - | 2560 | `	{"PASSWORD_ARGON2_DEFAULT_MEMORY_COST", PH7_ARGON2_MEM_Const },` |
|       - | 2561 | `	{"PASSWORD_ARGON2_DEFAULT_TIME_COST",   PH7_ARGON2_TIME_Const },` |
|       - | 2562 | `	{"PASSWORD_ARGON2_DEFAULT_THREADS",     PH7_ARGON2_THREADS_Const },` |
|       - | 2563 | `	{"FILTER_DEFAULT",              PH7_FILTER_DEFAULT_Const },` |
|       - | 2564 | `	{"FILTER_UNSAFE_RAW",           PH7_FILTER_UNSAFE_RAW_Const },` |
|       - | 2565 | `	{"FILTER_VALIDATE_INT",         PH7_FILTER_VALIDATE_INT_Const },` |
|       - | 2566 | `	{"FILTER_VALIDATE_BOOLEAN",     PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|       - | 2567 | `	{"FILTER_VALIDATE_BOOL",        PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|       - | 2568 | `	{"FILTER_VALIDATE_FLOAT",       PH7_FILTER_VALIDATE_FLOAT_Const },` |
|       - | 2569 | `	{"FILTER_VALIDATE_REGEXP",      PH7_FILTER_VALIDATE_REGEXP_Const },` |
|       - | 2570 | `	{"FILTER_VALIDATE_DOMAIN",      PH7_FILTER_VALIDATE_DOMAIN_Const },` |
|       - | 2571 | `	{"FILTER_VALIDATE_URL",         PH7_FILTER_VALIDATE_URL_Const },` |
|       - | 2572 | `	{"FILTER_VALIDATE_EMAIL",       PH7_FILTER_VALIDATE_EMAIL_Const },` |
|       - | 2573 | `	{"FILTER_VALIDATE_IP",          PH7_FILTER_VALIDATE_IP_Const },` |
|       - | 2574 | `	{"FILTER_VALIDATE_MAC",         PH7_FILTER_VALIDATE_MAC_Const },` |
|       - | 2575 | `	{"FILTER_CALLBACK",             PH7_FILTER_CALLBACK_Const },` |
|       - | 2576 | `	{"FILTER_THROW_ON_FAILURE",     PH7_FILTER_THROW_ON_FAILURE_Const },` |
|       - | 2577 | `	{"FILTER_SANITIZE_ENCODED",     PH7_FILTER_SANITIZE_ENCODED_Const },` |
|       - | 2578 | `	{"FILTER_SANITIZE_ADD_SLASHES", PH7_FILTER_SANITIZE_ADD_SLASHES_Const },` |
|       - | 2579 | `	{"FILTER_FLAG_NONE",            PH7_FILTER_FLAG_NONE_Const },` |
|       - | 2580 | `	{"FILTER_FLAG_EMPTY_STRING_NULL", PH7_FILTER_FLAG_EMPTY_STRING_NULL_Const },` |
|       - | 2581 | `	{"FILTER_SANITIZE_NUMBER_INT",  PH7_FILTER_SANITIZE_NUMBER_INT_Const },` |
|       - | 2582 | `	{"FILTER_SANITIZE_NUMBER_FLOAT",PH7_FILTER_SANITIZE_NUMBER_FLOAT_Const },` |
|       - | 2583 | `	{"FILTER_SANITIZE_SPECIAL_CHARS",PH7_FILTER_SANITIZE_SPECIAL_CHARS_Const },` |
|       - | 2584 | `	{"FILTER_SANITIZE_FULL_SPECIAL_CHARS",PH7_FILTER_SANITIZE_FULL_SPECIAL_CHARS_Const },` |
|       - | 2585 | `	{"FILTER_SANITIZE_EMAIL",       PH7_FILTER_SANITIZE_EMAIL_Const },` |
|       - | 2586 | `	{"FILTER_SANITIZE_URL",         PH7_FILTER_SANITIZE_URL_Const },` |
|       - | 2587 | `	{"FILTER_FLAG_ALLOW_OCTAL",     PH7_FILTER_FLAG_ALLOW_OCTAL_Const },` |
|       - | 2588 | `	{"FILTER_FLAG_ALLOW_HEX",       PH7_FILTER_FLAG_ALLOW_HEX_Const },` |
|       - | 2589 | `	{"FILTER_FLAG_STRIP_LOW",       PH7_FILTER_FLAG_STRIP_LOW_Const },` |
|       - | 2590 | `	{"FILTER_FLAG_STRIP_HIGH",      PH7_FILTER_FLAG_STRIP_HIGH_Const },` |
|       - | 2591 | `	{"FILTER_FLAG_ENCODE_LOW",      PH7_FILTER_FLAG_ENCODE_LOW_Const },` |
|       - | 2592 | `	{"FILTER_FLAG_ENCODE_HIGH",     PH7_FILTER_FLAG_ENCODE_HIGH_Const },` |
|       - | 2593 | `	{"FILTER_FLAG_ENCODE_AMP",      PH7_FILTER_FLAG_ENCODE_AMP_Const },` |
|       - | 2594 | `	{"FILTER_FLAG_NO_ENCODE_QUOTES",PH7_FILTER_FLAG_NO_ENCODE_QUOTES_Const },` |
|       - | 2595 | `	{"FILTER_FLAG_STRIP_BACKTICK",  PH7_FILTER_FLAG_STRIP_BACKTICK_Const },` |
|       - | 2596 | `	{"FILTER_FLAG_ALLOW_FRACTION",  PH7_FILTER_FLAG_ALLOW_FRACTION_Const },` |
|       - | 2597 | `	{"FILTER_FLAG_ALLOW_THOUSAND",  PH7_FILTER_FLAG_ALLOW_THOUSAND_Const },` |
|       - | 2598 | `	{"FILTER_FLAG_ALLOW_SCIENTIFIC",PH7_FILTER_FLAG_ALLOW_SCIENTIFIC_Const },` |
|       - | 2599 | `	{"FILTER_FLAG_IPV4",            PH7_FILTER_FLAG_IPV4_Const },` |
|       - | 2600 | `	{"FILTER_FLAG_IPV6",            PH7_FILTER_FLAG_IPV6_Const },` |
|       - | 2601 | `	{"FILTER_FLAG_PATH_REQUIRED",   PH7_FILTER_FLAG_PATH_REQUIRED_Const },` |
|       - | 2602 | `	{"FILTER_FLAG_QUERY_REQUIRED",  PH7_FILTER_FLAG_QUERY_REQUIRED_Const },` |
|       - | 2603 | `	{"FILTER_FLAG_HOSTNAME",        PH7_FILTER_FLAG_HOSTNAME_Const },` |
|       - | 2604 | `	{"FILTER_FLAG_EMAIL_UNICODE",   PH7_FILTER_FLAG_EMAIL_UNICODE_Const },` |
|       - | 2605 | `	{"FILTER_FLAG_NO_RES_RANGE",    PH7_FILTER_FLAG_NO_RES_RANGE_Const },` |
|       - | 2606 | `	{"FILTER_FLAG_NO_PRIV_RANGE",   PH7_FILTER_FLAG_NO_PRIV_RANGE_Const },` |
|       - | 2607 | `	{"FILTER_FLAG_GLOBAL_RANGE",    PH7_FILTER_FLAG_GLOBAL_RANGE_Const },` |
|       - | 2608 | `	{"FILTER_REQUIRE_ARRAY",        PH7_FILTER_REQUIRE_ARRAY_Const },` |
|       - | 2609 | `	{"FILTER_REQUIRE_SCALAR",       PH7_FILTER_REQUIRE_SCALAR_Const },` |
|       - | 2610 | `	{"FILTER_FORCE_ARRAY",          PH7_FILTER_FORCE_ARRAY_Const },` |
|       - | 2611 | `	{"FILTER_NULL_ON_FAILURE",      PH7_FILTER_NULL_ON_FAILURE_Const },` |
|       - | 2612 | `	{"INPUT_POST",                  PH7_INPUT_POST_Const },` |
|       - | 2613 | `	{"INPUT_GET",                   PH7_INPUT_GET_Const },` |
|       - | 2614 | `	{"INPUT_COOKIE",                PH7_INPUT_COOKIE_Const },` |
|       - | 2615 | `	{"INPUT_ENV",                   PH7_INPUT_ENV_Const },` |
|       - | 2616 | `	{"INPUT_SERVER",                PH7_INPUT_SERVER_Const },` |
|       - | 2617 | `	{"CAL_GREGORIAN",        PH7_CAL_GREGORIAN_Const },` |
|       - | 2618 | `	{"CAL_JULIAN",           PH7_CAL_JULIAN_Const    },` |
|       - | 2619 | `	{"CAL_JEWISH",           PH7_CAL_JEWISH_Const    },` |
|       - | 2620 | `	{"CAL_FRENCH",           PH7_CAL_FRENCH_Const    },` |
|       - | 2621 | `	{"CAL_NUM_CALS",         PH7_CAL_NUM_CALS_Const  },` |
|       - | 2622 | `	{"CAL_DOW_DAYNO",        PH7_CAL_DOW_DAYNO_Const },` |
|       - | 2623 | `	{"CAL_DOW_LONG",         PH7_CAL_DOW_LONG_Const  },` |
|       - | 2624 | `	{"CAL_DOW_SHORT",        PH7_CAL_DOW_SHORT_Const },` |
|       - | 2625 | `	{"CAL_MONTH_GREGORIAN_SHORT", PH7_CAL_MONTH_GREGORIAN_SHORT_Const },` |
|       - | 2626 | `	{"CAL_MONTH_GREGORIAN_LONG",  PH7_CAL_MONTH_GREGORIAN_LONG_Const },` |
|       - | 2627 | `	{"CAL_MONTH_JULIAN_SHORT",    PH7_CAL_MONTH_JULIAN_SHORT_Const },` |
|       - | 2628 | `	{"CAL_MONTH_JULIAN_LONG",     PH7_CAL_MONTH_JULIAN_LONG_Const },` |
|       - | 2629 | `	{"CAL_MONTH_JEWISH",          PH7_CAL_MONTH_JEWISH_Const },` |
|       - | 2630 | `	{"CAL_MONTH_FRENCH",          PH7_CAL_MONTH_FRENCH_Const },` |
|       - | 2631 | `	{"CAL_EASTER_DEFAULT",   PH7_CAL_EASTER_DEFAULT_Const },` |
|       - | 2632 | `	{"CAL_EASTER_ROMAN",     PH7_CAL_EASTER_ROMAN_Const   },` |
|       - | 2633 | `	{"CAL_EASTER_ALWAYS_GREGORIAN", PH7_CAL_EASTER_ALWAYS_GREGORIAN_Const },` |
|       - | 2634 | `	{"CAL_EASTER_ALWAYS_JULIAN",    PH7_CAL_EASTER_ALWAYS_JULIAN_Const },` |
|       - | 2635 | `	{"CAL_JEWISH_ADD_ALAFIM_GERESH", PH7_CAL_JEWISH_ADD_ALAFIM_GERESH_Const },` |
|       - | 2636 | `	{"CAL_JEWISH_ADD_ALAFIM",        PH7_CAL_JEWISH_ADD_ALAFIM_Const },` |
|       - | 2637 | `	{"CAL_JEWISH_ADD_GERESHAYIM",    PH7_CAL_JEWISH_ADD_GERESHAYIM_Const },` |
|       - | 2638 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|       - | 2639 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|       - | 2640 | `	{"PHP_INT_MIN",          PH7_INTMIN_Const   },` |
|       - | 2641 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|       - | 2642 | `	{"PHP_FLOAT_EPSILON",    PH7_FLOATEPSILON_Const },` |
|       - | 2643 | `	{"PHP_FLOAT_MAX",        PH7_FLOATMAX_Const },` |
|       - | 2644 | `	{"PHP_FLOAT_MIN",        PH7_FLOATMIN_Const },` |
|       - | 2645 | `	{"PHP_FLOAT_DIG",        PH7_FLOATDIG_Const },` |
|       - | 2646 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|       - | 2647 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|       - | 2648 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|       - | 2649 | `	{"__TIME__",             PH7_TIME_Const     },` |
|       - | 2650 | `	{"__DATE__",             PH7_DATE_Const     },` |
|       - | 2651 | `	{"__FILE__",             PH7_FILE_Const     },` |
|       - | 2652 | `	{"__DIR__",              PH7_DIR_Const      },` |
|       - | 2653 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|       - | 2654 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|       - | 2655 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|       - | 2656 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|       - | 2657 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|       - | 2658 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|       - | 2659 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|       - | 2660 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|       - | 2661 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|       - | 2662 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|       - | 2663 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|       - | 2664 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|       - | 2665 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|       - | 2666 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|       - | 2667 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|       - | 2668 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|       - | 2669 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|       - | 2670 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|       - | 2671 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|       - | 2672 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|       - | 2673 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|       - | 2674 | `	{"STREAM_IS_URL",                PH7_STREAM_IS_URL_Const },` |
|       - | 2675 | `	{"STREAM_SERVER_BIND",           PH7_STREAM_SERVER_BIND_Const },` |
|       - | 2676 | `	{"STREAM_SERVER_LISTEN",         PH7_STREAM_SERVER_LISTEN_Const },` |
|       - | 2677 | `	{"STREAM_CLIENT_CONNECT",        PH7_STREAM_CLIENT_CONNECT_Const },` |
|       - | 2678 | `	{"STREAM_CLIENT_ASYNC_CONNECT",  PH7_STREAM_CLIENT_ASYNC_CONNECT_Const },` |
|       - | 2679 | `	{"STREAM_CLIENT_PERSISTENT",     PH7_STREAM_CLIENT_PERSISTENT_Const },` |
|       - | 2680 | `	{"PSFS_PASS_ON",                 PH7_PSFS_PASS_ON_Const },` |
|       - | 2681 | `	{"PSFS_FEED_ME",                 PH7_PSFS_FEED_ME_Const },` |
|       - | 2682 | `	{"PSFS_ERR_FATAL",               PH7_PSFS_ERR_FATAL_Const },` |
|       - | 2683 | `	{"PSFS_FLAG_NORMAL",             PH7_PSFS_FLAG_NORMAL_Const },` |
|       - | 2684 | `	{"PSFS_FLAG_FLUSH_INC",          PH7_PSFS_FLAG_FLUSH_INC_Const },` |
|       - | 2685 | `	{"PSFS_FLAG_FLUSH_CLOSE",        PH7_PSFS_FLAG_FLUSH_CLOSE_Const },` |
|       - | 2686 | `	{"STREAM_FILTER_READ",           PH7_STREAM_FILTER_READ_Const },` |
|       - | 2687 | `	{"STREAM_FILTER_WRITE",          PH7_STREAM_FILTER_WRITE_Const },` |
|       - | 2688 | `	{"STREAM_FILTER_ALL",            PH7_STREAM_FILTER_ALL_Const },` |
|       - | 2689 | `	{"STREAM_SHUT_RD",               PH7_STREAM_SHUT_RD_Const },` |
|       - | 2690 | `	{"STREAM_SHUT_WR",               PH7_STREAM_SHUT_WR_Const },` |
|       - | 2691 | `	{"STREAM_SHUT_RDWR",             PH7_STREAM_SHUT_RDWR_Const },` |
|       - | 2692 | `	{"STREAM_OOB",                   PH7_STREAM_OOB_Const },` |
|       - | 2693 | `	{"STREAM_PEEK",                  PH7_STREAM_PEEK_Const },` |
|       - | 2694 | `#ifdef PH7_ENABLE_NET` |
|       - | 2695 | `	{"STREAM_PF_INET",               PH7_STREAM_PF_INET_Const },` |
|       - | 2696 | `	{"STREAM_PF_INET6",              PH7_STREAM_PF_INET6_Const },` |
|       - | 2697 | `	{"STREAM_PF_UNIX",               PH7_STREAM_PF_UNIX_Const },` |
|       - | 2698 | `	{"STREAM_SOCK_STREAM",           PH7_STREAM_SOCK_STREAM_Const },` |
|       - | 2699 | `	{"STREAM_SOCK_DGRAM",            PH7_STREAM_SOCK_DGRAM_Const },` |
|       - | 2700 | `	{"STREAM_SOCK_RAW",              PH7_STREAM_SOCK_RAW_Const },` |
|       - | 2701 | `	{"STREAM_SOCK_SEQPACKET",        PH7_STREAM_SOCK_SEQPACKET_Const },` |
|       - | 2702 | `	{"STREAM_SOCK_RDM",              PH7_STREAM_SOCK_RDM_Const },` |
|       - | 2703 | `	{"STREAM_IPPROTO_IP",            PH7_STREAM_IPPROTO_IP_Const },` |
|       - | 2704 | `	{"STREAM_IPPROTO_TCP",           PH7_STREAM_IPPROTO_TCP_Const },` |
|       - | 2705 | `	{"STREAM_IPPROTO_UDP",           PH7_STREAM_IPPROTO_UDP_Const },` |
|       - | 2706 | `	{"STREAM_IPPROTO_ICMP",          PH7_STREAM_IPPROTO_ICMP_Const },` |
|       - | 2707 | `	{"STREAM_IPPROTO_RAW",           PH7_STREAM_IPPROTO_RAW_Const },` |
|       - | 2708 | `#endif` |
|       - | 2709 | `	{"MT_RAND_MT19937",              PH7_MT_RAND_MT19937_Const },` |
|       - | 2710 | `	{"MT_RAND_PHP",                  PH7_MT_RAND_PHP_Const  },` |
|       - | 2711 | `	{"PHP_OUTPUT_HANDLER_WRITE",     PH7_OB_WRITE_Const     },` |
|       - | 2712 | `	{"PHP_OUTPUT_HANDLER_CONT",      PH7_OB_WRITE_Const     },` |
|       - | 2713 | `	{"PHP_OUTPUT_HANDLER_START",     PH7_OB_START_Const     },` |
|       - | 2714 | `	{"PHP_OUTPUT_HANDLER_CLEAN",     PH7_OB_CLEAN_Const     },` |
|       - | 2715 | `	{"PHP_OUTPUT_HANDLER_FLUSH",     PH7_OB_FLUSH_Const     },` |
|       - | 2716 | `	{"PHP_OUTPUT_HANDLER_FINAL",     PH7_OB_FINAL_Const     },` |
|       - | 2717 | `	{"PHP_OUTPUT_HANDLER_END",       PH7_OB_FINAL_Const     },` |
|       - | 2718 | `	{"PHP_OUTPUT_HANDLER_CLEANABLE", PH7_OB_CLEANABLE_Const },` |
|       - | 2719 | `	{"PHP_OUTPUT_HANDLER_FLUSHABLE", PH7_OB_FLUSHABLE_Const },` |
|       - | 2720 | `	{"PHP_OUTPUT_HANDLER_REMOVABLE", PH7_OB_REMOVABLE_Const },` |
|       - | 2721 | `	{"PHP_OUTPUT_HANDLER_STDFLAGS",  PH7_OB_STDFLAGS_Const  },` |
|       - | 2722 | `	{"PHP_OUTPUT_HANDLER_STARTED",   PH7_OB_STARTED_Const   },` |
|       - | 2723 | `	{"PHP_OUTPUT_HANDLER_DISABLED",  PH7_OB_DISABLED_Const  },` |
|       - | 2724 | `	{"PHP_OUTPUT_HANDLER_PROCESSED", PH7_OB_PROCESSED_Const },` |
|       - | 2725 | `	{"ARRAY_FILTER_USE_KEY", PH7_ARRAY_FILTER_USE_KEY_Const },` |
|       - | 2726 | `	{"ARRAY_FILTER_USE_BOTH",PH7_ARRAY_FILTER_USE_BOTH_Const},` |
|       - | 2727 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|       - | 2728 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|       - | 2729 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|       - | 2730 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|       - | 2731 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|       - | 2732 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|       - | 2733 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|       - | 2734 | `	{"SORT_LOCALE_STRING",   PH7_SORT_LOCALE_STRING_Const },` |
|       - | 2735 | `	{"SORT_NATURAL",         PH7_SORT_NATURAL_Const },` |
|       - | 2736 | `	{"SORT_FLAG_CASE",       PH7_SORT_FLAG_CASE_Const },` |
|       - | 2737 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|       - | 2738 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|       - | 2739 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|       - | 2740 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|       - | 2741 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|       - | 2742 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|       - | 2743 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|       - | 2744 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|       - | 2745 | `	{"M_E",                  PH7_M_E_Const          },` |
|       - | 2746 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|       - | 2747 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|       - | 2748 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|       - | 2749 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|       - | 2750 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|       - | 2751 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|       - | 2752 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|       - | 2753 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|       - | 2754 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|       - | 2755 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|       - | 2756 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|       - | 2757 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|       - | 2758 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|       - | 2759 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|       - | 2760 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|       - | 2761 | `	{"NAN",                  PH7_NAN_Const          },` |
|       - | 2762 | `	{"INF",                  PH7_INF_Const          },` |
|       - | 2763 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|       - | 2764 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|       - | 2765 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|       - | 2766 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|       - | 2767 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|       - | 2768 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|       - | 2769 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|       - | 2770 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|       - | 2771 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|       - | 2772 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|       - | 2773 | `	{"DATE_RFC3339_EXTENDED",PH7_DATE_RFC3339_EXTENDED_Const },` |
|       - | 2774 | `	{"DATE_RFC7231",         PH7_DATE_RFC7231_Const },` |
|       - | 2775 | `	{"DATE_ISO8601_EXPANDED",PH7_DATE_ISO8601_EXPANDED_Const },` |
|       - | 2776 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|       - | 2777 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|       - | 2778 | `	{"FILE_TEXT",            PH7_FILE_TEXT_Const    },` |
|       - | 2779 | `	{"FILE_BINARY",          PH7_FILE_TEXT_Const    },` |
|       - | 2780 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|       - | 2781 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|       - | 2782 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|       - | 2783 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|       - | 2784 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|       - | 2785 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|       - | 2786 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|       - | 2787 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|       - | 2788 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|       - | 2789 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|       - | 2790 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|       - | 2791 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|       - | 2792 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|       - | 2793 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|       - | 2794 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|       - | 2795 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|       - | 2796 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|       - | 2797 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|       - | 2798 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|       - | 2799 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|       - | 2800 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|       - | 2801 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|       - | 2802 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|       - | 2803 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|       - | 2804 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|       - | 2805 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|       - | 2806 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|       - | 2807 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|       - | 2808 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|       - | 2809 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|       - | 2810 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|       - | 2811 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|       - | 2812 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|       - | 2813 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|       - | 2814 | `	{"PATHINFO_ALL",         PH7_PATHINFO_ALL_Const },` |
|       - | 2815 | `	/* ASSERT_QUIET_EVAL was REMOVED in php 8.0: referencing it is an Error there */` |
|       - | 2816 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|       - | 2817 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|       - | 2818 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|       - | 2819 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|       - | 2820 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|       - | 2821 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|       - | 2822 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|       - | 2823 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|       - | 2824 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|       - | 2825 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|       - | 2826 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|       - | 2827 | `	{"FILE_NO_DEFAULT_CONTEXT", PH7_FILE_NO_DEFAULT_CONTEXT_Const },` |
|       - | 2828 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|       - | 2829 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|       - | 2830 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|       - | 2831 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|       - | 2832 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|       - | 2833 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|       - | 2834 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|       - | 2835 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|       - | 2836 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|       - | 2837 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|       - | 2838 | `	{"GLOB_AVAILABLE_FLAGS", PH7_GLOB_AVAILABLE_FLAGS_Const },` |
|       - | 2839 | `	{"STDIN",                PH7_STDIN_Const        },` |
|       - | 2840 | `	{"stdin",                PH7_STDIN_Const        },` |
|       - | 2841 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|       - | 2842 | `	{"stdout",               PH7_STDOUT_Const       },` |
|       - | 2843 | `	{"STDERR",               PH7_STDERR_Const       },` |
|       - | 2844 | `	{"stderr",               PH7_STDERR_Const       },` |
|       - | 2845 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|       - | 2846 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|       - | 2847 | `	{"INI_SCANNER_TYPED",    PH7_INI_SCANNER_TYPED_Const  },` |
|       - | 2848 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|       - | 2849 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|       - | 2850 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|       - | 2851 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|       - | 2852 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|       - | 2853 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|       - | 2854 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|       - | 2855 | `	{"EXTR_REFS",            PH7_EXTR_REFS_Const        },` |
|       - | 2856 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|       - | 2857 | `	{"HASH_HMAC",              PH7_HASH_HMAC_Const},` |
|       - | 2858 | `	{"CRYPT_SALT_LENGTH",      PH7_CRYPT_SALT_LENGTH_Const},` |
|       - | 2859 | `	{"CRYPT_STD_DES",          PH7_CRYPT_ONE_Const},` |
|       - | 2860 | `	{"CRYPT_EXT_DES",          PH7_CRYPT_ONE_Const},` |
|       - | 2861 | `	{"CRYPT_MD5",              PH7_CRYPT_ONE_Const},` |
|       - | 2862 | `	{"CRYPT_BLOWFISH",         PH7_CRYPT_ONE_Const},` |
|       - | 2863 | `	{"CRYPT_SHA256",           PH7_CRYPT_ONE_Const},` |
|       - | 2864 | `	{"CRYPT_SHA512",           PH7_CRYPT_ONE_Const},` |
|       - | 2865 | `#endif` |
|       - | 2866 | `	{"ICONV_IMPL",             PH7_ICONV_IMPL_Const},` |
|       - | 2867 | `	{"ICONV_VERSION",          PH7_ICONV_VERSION_Const},` |
|       - | 2868 | `	{"ICONV_MIME_DECODE_STRICT", PH7_ICONV_MIME_DECODE_STRICT_Const},` |
|       - | 2869 | `	{"ICONV_MIME_DECODE_CONTINUE_ON_ERROR", PH7_ICONV_MIME_DECODE_CONTINUE_ON_ERROR_Const},` |
|       - | 2870 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|       - | 2871 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|       - | 2872 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|       - | 2873 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|       - | 2874 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|       - | 2875 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|       - | 2876 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|       - | 2877 | `	{"JSON_OBJECT_AS_ARRAY",   PH7_JSON_OBJECT_AS_ARRAY_Const},` |
|       - | 2878 | `	{"JSON_PARTIAL_OUTPUT_ON_ERROR", PH7_JSON_PARTIAL_OUTPUT_ON_ERROR_Const},` |
|       - | 2879 | `	{"JSON_PRESERVE_ZERO_FRACTION",  PH7_JSON_PRESERVE_ZERO_FRACTION_Const},` |
|       - | 2880 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|       - | 2881 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|       - | 2882 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|       - | 2883 | `	{"JSON_UNESCAPED_LINE_TERMINATORS", PH7_JSON_UNESCAPED_LINE_TERMINATORS_Const},` |
|       - | 2884 | `	{"JSON_INVALID_UTF8_IGNORE", PH7_JSON_INVALID_UTF8_IGNORE_Const},` |
|       - | 2885 | `	{"JSON_INVALID_UTF8_SUBSTITUTE", PH7_JSON_INVALID_UTF8_SUBSTITUTE_Const},` |
|       - | 2886 | `	{"JSON_THROW_ON_ERROR",    PH7_JSON_THROW_ON_ERROR_Const},` |
|       - | 2887 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|       - | 2888 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|       - | 2889 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|       - | 2890 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|       - | 2891 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|       - | 2892 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|       - | 2893 | `	{"JSON_ERROR_RECURSION", PH7_JSON_ERROR_RECURSION_Const},` |
|       - | 2894 | `	{"JSON_ERROR_UNSUPPORTED_TYPE", PH7_JSON_ERROR_UNSUPPORTED_TYPE_Const},` |
|       - | 2895 | `	{"JSON_ERROR_INVALID_PROPERTY_NAME", PH7_JSON_ERROR_INVALID_PROPERTY_NAME_Const},` |
|       - | 2896 | `	{"JSON_ERROR_UTF16",     PH7_JSON_ERROR_UTF16_Const},` |
|       - | 2897 | `	{"JSON_ERROR_NON_BACKED_ENUM", PH7_JSON_ERROR_NON_BACKED_ENUM_Const},` |
|       - | 2898 | `	{"JSON_ERROR_INF_OR_NAN", PH7_JSON_ERROR_INF_OR_NAN_Const},` |
|       - | 2899 | ``	/* `self`, `parent` and `static` are KEYWORDS in php, not constants: using one as a bare`` |
|       - | 2900 | ``	 * word is an "Undefined constant" Error (or a parse error for `static`). PH7 registered`` |
|       - | 2901 | `	 * them as constants that quietly expanded to the class name / NULL, so a typo'd bare` |
|       - | 2902 | ``	 * word silently produced a value. The `self::`/`parent::`/`static::` forms are handled`` |
|       - | 2903 | ``	 * by the `::` compile path and do not go through the constant table. */`` |
|       - | 2904 | `	{"__CLASS__",            PH7_class_magic_Const  }` |
|       - | 2905 | `};` |
|       - | 2906 | `/*` |
|       - | 2907 | ` * Register the built-in constants defined above.` |
|       - | 2908 | ` */` |
|    4962 | 2909 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|       5 | 2910 | `{` |
|       - | 2911 | `	sxu32 n;` |
|       - | 2912 | `	/*` |
|       - | 2913 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|       - | 2914 | `	 * that trigger the constant invocation as their private data.` |
|       - | 2915 | `	 */` |
| 1796249 | 2916 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 1791287 | 2917 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
|  895646 | 2918 | `	}` |
|    4967 | 2919 | `}` |
