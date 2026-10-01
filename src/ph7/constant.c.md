# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2037/2053 lines (99.22%)

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
|       - |    8 | `#ifdef PH7_ENABLE_OPENSSL` |
|       - |    9 | `/* The linked OpenSSL's own version, padding and purpose numbers: every one of` |
|       - |   10 | ` * these is bound by SYMBOL rather than by value, so a build against another` |
|       - |   11 | ` * 3.x answers that library's numbers exactly as php's build answers its. */` |
|       - |   12 | `#include <openssl/opensslv.h>` |
|       - |   13 | `#include <openssl/crypto.h>` |
|       - |   14 | `#include <openssl/rsa.h>` |
|       - |   15 | `#include <openssl/pkcs7.h>` |
|       - |   16 | `#include <openssl/cms.h>` |
|       - |   17 | `#include <openssl/x509v3.h>` |
|       - |   18 | `#endif` |
|       - |   19 | `#ifdef PH7_ENABLE_ZLIB` |
|       - |   20 | `#include <zlib.h> /* ZLIB_VERSION/ZLIB_VERNUM: the library this build links */` |
|       - |   21 | `#endif` |
|       - |   22 | `#ifndef __WINNT__` |
|       - |   23 | `/* ext/posix's constants are the platform's own macros -- see PH7_POSIX_*_Const. */` |
|       - |   24 | `#include <unistd.h>` |
|       - |   25 | `#include <sys/stat.h>` |
|       - |   26 | `#include <sys/resource.h>` |
|       - |   27 | `#endif` |
|       - |   28 | `/* This file implement built-in constants for the PH7 engine. */` |
|       - |   29 | `/*` |
|       - |   30 | ` * PH7_VERSION` |
|       - |   31 | ` * __PH7__` |
|       - |   32 | ` *   Expand the current version of the PH7 engine.` |
|       - |   33 | ` */` |
|     218 |   34 | `static void PH7_VER_Const(ph7_value *pVal,void *pUnused)` |
|       3 |   35 | `{` |
|     109 |   36 | `	SXUNUSED(pUnused);` |
|     221 |   37 | `	ph7_value_string(pVal,ph7_lib_signature(),-1/*Compute length automatically*/);` |
|     221 |   38 | `}` |
|       - |   39 | `/*` |
|       - |   40 | ` * PHP_VERSION, PHP_MAJOR_VERSION, PHP_MINOR_VERSION, PHP_RELEASE_VERSION,` |
|       - |   41 | ` * PHP_EXTRA_VERSION, PHP_VERSION_ID` |
|       - |   42 | ` *   Expand the PHP-compatibility version PHL advertises (see PHP_COMPAT_* in ph7.h).` |
|       - |   43 | ` */` |
|      84 |   44 | `static void PH7_PHPVerConst(ph7_value *pVal,void *pUnused)` |
|       4 |   45 | `{` |
|      42 |   46 | `	SXUNUSED(pUnused);` |
|      88 |   47 | `	ph7_value_string(pVal,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION)-1);` |
|      88 |   48 | `}` |
|      74 |   49 | `static void PH7_PHPMajorConst(ph7_value *pVal,void *pUnused)` |
|       3 |   50 | `{` |
|      37 |   51 | `	SXUNUSED(pUnused);` |
|      77 |   52 | `	ph7_value_int64(pVal,PHP_COMPAT_MAJOR_VERSION);` |
|      77 |   53 | `}` |
|      74 |   54 | `static void PH7_PHPMinorConst(ph7_value *pVal,void *pUnused)` |
|       3 |   55 | `{` |
|      37 |   56 | `	SXUNUSED(pUnused);` |
|      77 |   57 | `	ph7_value_int64(pVal,PHP_COMPAT_MINOR_VERSION);` |
|      77 |   58 | `}` |
|      74 |   59 | `static void PH7_PHPReleaseConst(ph7_value *pVal,void *pUnused)` |
|       3 |   60 | `{` |
|      37 |   61 | `	SXUNUSED(pUnused);` |
|      77 |   62 | `	ph7_value_int64(pVal,PHP_COMPAT_RELEASE_VERSION);` |
|      77 |   63 | `}` |
|      72 |   64 | `static void PH7_PHPExtraConst(ph7_value *pVal,void *pUnused)` |
|       3 |   65 | `{` |
|      36 |   66 | `	SXUNUSED(pUnused);` |
|      75 |   67 | `	ph7_value_string(pVal,PHP_COMPAT_EXTRA_VERSION,(int)sizeof(PHP_COMPAT_EXTRA_VERSION)-1);` |
|      75 |   68 | `}` |
|      74 |   69 | `static void PH7_PHPVerIdConst(ph7_value *pVal,void *pUnused)` |
|       3 |   70 | `{` |
|      37 |   71 | `	SXUNUSED(pUnused);` |
|      77 |   72 | `	ph7_value_int64(pVal,PHP_COMPAT_VERSION_ID);` |
|      77 |   73 | `}` |
|       - |   74 | `#ifdef __WINNT__` |
|       - |   75 | `#include <Windows.h>` |
|       - |   76 | `#elif defined(__UNIXES__)` |
|       - |   77 | `#include <sys/utsname.h>` |
|       - |   78 | `#endif` |
|       - |   79 | `/*` |
|       - |   80 | ` * PHP_OS` |
|       - |   81 | ` *  Expand the name of the host Operating System.` |
|       - |   82 | ` */` |
|    6270 |   83 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|       5 |   84 | `{` |
|       - |   85 | `#if defined(__WINNT__)` |
|       5 |   86 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|       - |   87 | `#elif defined(__UNIXES__)` |
|       - |   88 | `	struct utsname sInfo;` |
|    6270 |   89 | `	if( uname(&sInfo) != 0 ){` |
|     ! 0 |   90 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|     ! 0 |   91 | `	}else{` |
|    6270 |   92 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|       - |   93 | `	}` |
|       - |   94 | `#else` |
|       - |   95 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|       - |   96 | `#endif` |
|    3131 |   97 | `	SXUNUSED(pUnused);` |
|    6275 |   98 | `}` |
|       - |   99 | `/*` |
|       - |  100 | ` * PHP_OS_FAMILY (php 7.2)` |
|       - |  101 | ` *  One of 'Windows', 'BSD', 'Darwin', 'Solaris', 'Linux' or 'Unknown', derived` |
|       - |  102 | ` *  from the host's uname sysname (php maps the same set at build time).` |
|       - |  103 | ` */` |
|     125 |  104 | `static void PH7_OS_FAMILY_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  105 | `{` |
|      62 |  106 | `	SXUNUSED(pUnused);` |
|       - |  107 | `#if defined(__WINNT__)` |
|       5 |  108 | `	ph7_value_string(pVal,"Windows",(int)sizeof("Windows")-1);` |
|       - |  109 | `#elif defined(__UNIXES__)` |
|       - |  110 | `	struct utsname sInfo;` |
|     125 |  111 | `	const char *zFamily = "Unknown";` |
|     125 |  112 | `	if( uname(&sInfo) == 0 ){` |
|     125 |  113 | `		const char *z = sInfo.sysname;` |
|     125 |  114 | `		if( SyStrnicmp(z,"Darwin",sizeof("Darwin")-1) == 0 ){` |
|      62 |  115 | `			zFamily = "Darwin";` |
|     125 |  116 | `		}else if( SyStrnicmp(z,"Linux",sizeof("Linux")-1) == 0 ){` |
|      63 |  117 | `			zFamily = "Linux";` |
|     ! 0 |  118 | `		}else if( SyStrnicmp(z,"SunOS",sizeof("SunOS")-1) == 0 ){` |
|     ! 0 |  119 | `			zFamily = "Solaris";` |
|     ! 0 |  120 | `		}else{` |
|       - |  121 | `			/* FreeBSD/OpenBSD/NetBSD/DragonFly -> 'BSD' (scan for "BSD"). */` |
|     ! 0 |  122 | `			const char *p = z;` |
|     ! 0 |  123 | `			while( p[0] && p[1] && p[2] ){` |
|     ! 0 |  124 | `				if( (p[0]=='B'\|\|p[0]=='b') && (p[1]=='S'\|\|p[1]=='s') && (p[2]=='D'\|\|p[2]=='d') ){` |
|     ! 0 |  125 | `					zFamily = "BSD";` |
|     ! 0 |  126 | `					break;` |
|       - |  127 | `				}` |
|     ! 0 |  128 | `				p++;` |
|       - |  129 | `			}` |
|       - |  130 | `		}` |
|      62 |  131 | `	}` |
|     125 |  132 | `	ph7_value_string(pVal,zFamily,-1);` |
|       - |  133 | `#else` |
|       - |  134 | `	ph7_value_string(pVal,"Unknown",(int)sizeof("Unknown")-1);` |
|       - |  135 | `#endif` |
|     130 |  136 | `}` |
|       - |  137 | `/*` |
|       - |  138 | ` * PHP_SAPI` |
|       - |  139 | ` *  The interface between the interpreter and the host. PHL's host binary is a` |
|       - |  140 | ` *  command-line interpreter, so this is "cli" (matching the CLI default of` |
|       - |  141 | ` *  php_sapi_name(); the built-in -S server's per-request "cli-server" flavour is` |
|       - |  142 | ` *  only surfaced by php_sapi_name(), not this compile-time constant).` |
|       - |  143 | ` */` |
|      72 |  144 | `static void PH7_SAPI_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  145 | `{` |
|      36 |  146 | `	SXUNUSED(pUnused);` |
|      75 |  147 | `	ph7_value_string(pVal,"cli",(int)sizeof("cli")-1);` |
|      75 |  148 | `}` |
|       - |  149 | `/*` |
|       - |  150 | ` * PHP_EOL` |
|       - |  151 | ` *  Expand the correct 'End Of Line' symbol for this platform.` |
|       - |  152 | ` */` |
|     910 |  153 | `static void PH7_EOL_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  154 | `{` |
|     455 |  155 | `	SXUNUSED(pUnused);` |
|       - |  156 | `#ifdef __WINNT__` |
|       5 |  157 | `	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);` |
|       - |  158 | `#else` |
|     910 |  159 | `	ph7_value_string(pVal,"\n",(int)sizeof(char));` |
|       - |  160 | `#endif` |
|     915 |  161 | `}` |
|       - |  162 | `/*` |
|       - |  163 | ` * PHP_INT_MAX` |
|       - |  164 | ` * Expand the largest integer supported.` |
|       - |  165 | ` * Note that PH7 deals with 64-bit integer for all platforms.` |
|       - |  166 | ` */` |
|    1800 |  167 | `static void PH7_INTMAX_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  168 | `{` |
|     899 |  169 | `	SXUNUSED(pUnused);` |
|    1805 |  170 | `	ph7_value_int64(pVal,SXI64_HIGH);` |
|    1805 |  171 | `}` |
|       - |  172 | `/*` |
|       - |  173 | ` * ext/calendar: the four calendars, numbered in the order the conversion table` |
|       - |  174 | ` * holds them, and CAL_NUM_CALS as their count -- which is what makes the` |
|       - |  175 | ` * "valid calendar ID" screen a plain 0 <= id < CAL_NUM_CALS test.` |
|       - |  176 | ` */` |
|     116 |  177 | `static void PH7_CAL_GREGORIAN_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  178 | `{` |
|      58 |  179 | `	SXUNUSED(pUnused);` |
|     120 |  180 | `	ph7_value_int(pVal,0);` |
|     120 |  181 | `}` |
|      86 |  182 | `static void PH7_CAL_JULIAN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  183 | `{` |
|      43 |  184 | `	SXUNUSED(pUnused);` |
|      89 |  185 | `	ph7_value_int(pVal,1);` |
|      89 |  186 | `}` |
|     204 |  187 | `static void PH7_CAL_JEWISH_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  188 | `{` |
|     102 |  189 | `	SXUNUSED(pUnused);` |
|     207 |  190 | `	ph7_value_int(pVal,2);` |
|     207 |  191 | `}` |
|      96 |  192 | `static void PH7_CAL_FRENCH_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  193 | `{` |
|      48 |  194 | `	SXUNUSED(pUnused);` |
|      99 |  195 | `	ph7_value_int(pVal,3);` |
|      99 |  196 | `}` |
|      70 |  197 | `static void PH7_CAL_NUM_CALS_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  198 | `{` |
|      35 |  199 | `	SXUNUSED(pUnused);` |
|      73 |  200 | `	ph7_value_int(pVal,4);` |
|      73 |  201 | `}` |
|       - |  202 | `/*` |
|       - |  203 | ` * easter_days()/easter_date()'s $mode. DEFAULT is not a rule but a` |
|       - |  204 | ` * date-dependent choice between the two below it; ROMAN moves the 1583-1752` |
|       - |  205 | ` * window to the Gregorian rule, and the two ALWAYS_ modes pin one rule for` |
|       - |  206 | ` * every year.` |
|       - |  207 | ` */` |
|      90 |  208 | `static void PH7_CAL_EASTER_DEFAULT_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  209 | `{` |
|      45 |  210 | `	SXUNUSED(pUnused);` |
|      93 |  211 | `	ph7_value_int(pVal,0);` |
|      93 |  212 | `}` |
|      90 |  213 | `static void PH7_CAL_EASTER_ROMAN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  214 | `{` |
|      45 |  215 | `	SXUNUSED(pUnused);` |
|      93 |  216 | `	ph7_value_int(pVal,1);` |
|      93 |  217 | `}` |
|      90 |  218 | `static void PH7_CAL_EASTER_ALWAYS_GREGORIAN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  219 | `{` |
|      45 |  220 | `	SXUNUSED(pUnused);` |
|      93 |  221 | `	ph7_value_int(pVal,2);` |
|      93 |  222 | `}` |
|      94 |  223 | `static void PH7_CAL_EASTER_ALWAYS_JULIAN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  224 | `{` |
|      47 |  225 | `	SXUNUSED(pUnused);` |
|      97 |  226 | `	ph7_value_int(pVal,3);` |
|      97 |  227 | `}` |
|       - |  228 | `/* jddayofweek()'s three modes. Note that SHORT is 2 and LONG is 1: the numbers` |
|       - |  229 | ` * are not in the order the names suggest. */` |
|      70 |  230 | `static void PH7_CAL_DOW_DAYNO_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  231 | `{` |
|      35 |  232 | `	SXUNUSED(pUnused);` |
|      73 |  233 | `	ph7_value_int(pVal,0);` |
|      73 |  234 | `}` |
|     106 |  235 | `static void PH7_CAL_DOW_LONG_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  236 | `{` |
|      53 |  237 | `	SXUNUSED(pUnused);` |
|     109 |  238 | `	ph7_value_int(pVal,1);` |
|     109 |  239 | `}` |
|      96 |  240 | `static void PH7_CAL_DOW_SHORT_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  241 | `{` |
|      48 |  242 | `	SXUNUSED(pUnused);` |
|      99 |  243 | `	ph7_value_int(pVal,2);` |
|      99 |  244 | `}` |
|       - |  245 | `/* jdmonthname()'s six modes, which pick the CALENDAR as well as the spelling` |
|       - |  246 | ` * and are numbered independently of the CAL_* calendar ids above. */` |
|      72 |  247 | `static void PH7_CAL_MONTH_GREGORIAN_SHORT_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  248 | `{` |
|      36 |  249 | `	SXUNUSED(pUnused);` |
|      75 |  250 | `	ph7_value_int(pVal,0);` |
|      75 |  251 | `}` |
|      94 |  252 | `static void PH7_CAL_MONTH_GREGORIAN_LONG_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  253 | `{` |
|      47 |  254 | `	SXUNUSED(pUnused);` |
|      97 |  255 | `	ph7_value_int(pVal,1);` |
|      97 |  256 | `}` |
|      72 |  257 | `static void PH7_CAL_MONTH_JULIAN_SHORT_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  258 | `{` |
|      36 |  259 | `	SXUNUSED(pUnused);` |
|      75 |  260 | `	ph7_value_int(pVal,2);` |
|      75 |  261 | `}` |
|      72 |  262 | `static void PH7_CAL_MONTH_JULIAN_LONG_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  263 | `{` |
|      36 |  264 | `	SXUNUSED(pUnused);` |
|      75 |  265 | `	ph7_value_int(pVal,3);` |
|      75 |  266 | `}` |
|      78 |  267 | `static void PH7_CAL_MONTH_JEWISH_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  268 | `{` |
|      39 |  269 | `	SXUNUSED(pUnused);` |
|      81 |  270 | `	ph7_value_int(pVal,4);` |
|      81 |  271 | `}` |
|      72 |  272 | `static void PH7_CAL_MONTH_FRENCH_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  273 | `{` |
|      36 |  274 | `	SXUNUSED(pUnused);` |
|      75 |  275 | `	ph7_value_int(pVal,5);` |
|      75 |  276 | `}` |
|       - |  277 | `/*` |
|       - |  278 | ` * ext/calendar: the three flags jdtojewish()'s Hebrew spelling reads. They are` |
|       - |  279 | ` * a bit set, so a caller may ask for any combination of them.` |
|       - |  280 | ` */` |
|      74 |  281 | `static void PH7_CAL_JEWISH_ADD_ALAFIM_GERESH_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  282 | `{` |
|      37 |  283 | `	SXUNUSED(pUnused);` |
|      77 |  284 | `	ph7_value_int(pVal,2);` |
|      77 |  285 | `}` |
|      74 |  286 | `static void PH7_CAL_JEWISH_ADD_ALAFIM_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  287 | `{` |
|      37 |  288 | `	SXUNUSED(pUnused);` |
|      77 |  289 | `	ph7_value_int(pVal,4);` |
|      77 |  290 | `}` |
|      76 |  291 | `static void PH7_CAL_JEWISH_ADD_GERESHAYIM_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  292 | `{` |
|      38 |  293 | `	SXUNUSED(pUnused);` |
|      79 |  294 | `	ph7_value_int(pVal,8);` |
|      79 |  295 | `}` |
|       - |  296 | `/*` |
|       - |  297 | ` * ext/standard's IMAGETYPE_* space (php's image_filetype enum), in php's own` |
|       - |  298 | ` * numbering. Three of the names are not enum members at all:` |
|       - |  299 | ` * IMAGETYPE_JPEG2000 is a userland ALIAS for IMAGETYPE_JPC (9, the raw` |
|       - |  300 | ` * codestream) rather than a type of its own, IMAGETYPE_UNKNOWN is the zero` |
|       - |  301 | ` * every detection ladder falls out at, and IMAGETYPE_COUNT is the number of` |
|       - |  302 | ` * types -- which is the FIXED count plus one for every handler a build` |
|       - |  303 | ` * registers, so the SVG reader ext/libxml installs makes it 22 instead of 21.` |
|       - |  304 | ` */` |
|       - |  305 | `#define PH7_IMAGETYPE_CONST(fn,v)                     \` |
|       - |  306 | `	static void fn(ph7_value *pVal,void *pUnused)     \` |
|       - |  307 | `	{                                                 \` |
|       - |  308 | `		SXUNUSED(pUnused);                            \` |
|       - |  309 | `		ph7_value_int(pVal,v);                        \` |
|       - |  310 | `	}` |
|      74 |  311 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_UNKNOWN_Const,   0)` |
|      88 |  312 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_GIF_Const,       1)` |
|      74 |  313 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JPEG_Const,      2)` |
|      74 |  314 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_PNG_Const,       3)` |
|      74 |  315 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_SWF_Const,       4)` |
|      74 |  316 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_PSD_Const,       5)` |
|      74 |  317 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_BMP_Const,       6)` |
|      74 |  318 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_TIFF_II_Const,   7)` |
|      74 |  319 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_TIFF_MM_Const,   8)` |
|     149 |  320 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JPC_Const,       9)` |
|      74 |  321 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JP2_Const,      10)` |
|      74 |  322 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JPX_Const,      11)` |
|      74 |  323 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JB2_Const,      12)` |
|      81 |  324 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_SWC_Const,      13)` |
|      74 |  325 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_IFF_Const,      14)` |
|      74 |  326 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_WBMP_Const,     15)` |
|      74 |  327 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_XBM_Const,      16)` |
|      74 |  328 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_ICO_Const,      17)` |
|      74 |  329 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_WEBP_Const,     18)` |
|      74 |  330 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_AVIF_Const,     19)` |
|      74 |  331 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_HEIF_Const,     20)` |
|       - |  332 | `#ifdef PH7_ENABLE_LIBXML` |
|      76 |  333 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_SVG_Const,      21)` |
|      76 |  334 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_COUNT_Const,    22)` |
|       - |  335 | `#else` |
|       - |  336 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_COUNT_Const,    21)` |
|       - |  337 | `#endif` |
|       - |  338 | `/*` |
|       - |  339 | ` * PHP_INT_MIN (php 7.0)` |
|       - |  340 | ` * Expand the smallest integer supported.` |
|       - |  341 | ` */` |
|     232 |  342 | `static void PH7_INTMIN_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  343 | `{` |
|     116 |  344 | `	SXUNUSED(pUnused);` |
|     236 |  345 | `	ph7_value_int64(pVal,SMALLEST_INT64);` |
|     236 |  346 | `}` |
|       - |  347 | `/*` |
|       - |  348 | ` * PHP_INT_SIZE` |
|       - |  349 | ` * Expand the size in bytes of a 64-bit integer.` |
|       - |  350 | ` */` |
|      90 |  351 | `static void PH7_INTSIZE_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  352 | `{` |
|      45 |  353 | `	SXUNUSED(pUnused);` |
|      94 |  354 | `	ph7_value_int64(pVal,sizeof(sxi64));` |
|      94 |  355 | `}` |
|       - |  356 | `/*` |
|       - |  357 | ` * PHP_FLOAT_EPSILON / PHP_FLOAT_MAX / PHP_FLOAT_MIN / PHP_FLOAT_DIG (php 7.2)` |
|       - |  358 | ` * Double-precision characteristics, sourced from <float.h> exactly like php` |
|       - |  359 | ` * so they track the compiling platform's actual double representation.` |
|       - |  360 | ` */` |
|      76 |  361 | `static void PH7_FLOATEPSILON_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  362 | `{` |
|      38 |  363 | `	SXUNUSED(pUnused);` |
|      79 |  364 | `	ph7_value_double(pVal,DBL_EPSILON);` |
|      79 |  365 | `}` |
|      76 |  366 | `static void PH7_FLOATMAX_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  367 | `{` |
|      38 |  368 | `	SXUNUSED(pUnused);` |
|      79 |  369 | `	ph7_value_double(pVal,DBL_MAX);` |
|      79 |  370 | `}` |
|      72 |  371 | `static void PH7_FLOATMIN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  372 | `{` |
|      36 |  373 | `	SXUNUSED(pUnused);` |
|      75 |  374 | `	ph7_value_double(pVal,DBL_MIN);` |
|      75 |  375 | `}` |
|      72 |  376 | `static void PH7_FLOATDIG_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  377 | `{` |
|      36 |  378 | `	SXUNUSED(pUnused);` |
|      75 |  379 | `	ph7_value_int64(pVal,DBL_DIG);` |
|      75 |  380 | `}` |
|       - |  381 | `/*` |
|       - |  382 | ` * DIRECTORY_SEPARATOR.` |
|       - |  383 | ` * Expand the directory separator character.` |
|       - |  384 | ` */` |
|    2374 |  385 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  386 | `{` |
|    1186 |  387 | `	SXUNUSED(pUnused);` |
|       - |  388 | `#ifdef __WINNT__` |
|       5 |  389 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|       - |  390 | `#else` |
|    2374 |  391 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|       - |  392 | `#endif` |
|    2379 |  393 | `}` |
|       - |  394 | `/*` |
|       - |  395 | ` * PATH_SEPARATOR.` |
|       - |  396 | ` * Expand the path separator character.` |
|       - |  397 | ` */` |
|      77 |  398 | `static void PH7_PATHSEP_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  399 | `{` |
|      38 |  400 | `	SXUNUSED(pUnused);` |
|       - |  401 | `#ifdef __WINNT__` |
|       3 |  402 | `	ph7_value_string(pVal,";",(int)sizeof(char));` |
|       - |  403 | `#else` |
|      77 |  404 | `	ph7_value_string(pVal,":",(int)sizeof(char));` |
|       - |  405 | `#endif` |
|      80 |  406 | `}` |
|       - |  407 |  |
|       - |  408 | `#if defined(PH7_ENABLE_MATH_FUNC)` |
|       - |  409 | `/*` |
|       - |  410 | ` * NAN constant: floating-point Not-A-Number` |
|       - |  411 | ` */` |
|     205 |  412 | `static void PH7_NAN_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  413 | `{` |
|     102 |  414 | `	SXUNUSED(pUnused);` |
|     209 |  415 | `	ph7_value_double(pVal, PH7_NAN_VALUE());` |
|     209 |  416 | `}` |
|       - |  417 |  |
|       - |  418 | `/*` |
|       - |  419 | ` * INF constant: positive infinity` |
|       - |  420 | ` */` |
|     231 |  421 | `static void PH7_INF_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  422 | `{` |
|     115 |  423 | `	SXUNUSED(pUnused);` |
|       - |  424 | `	/* similarly avoid the INFINITY macro */` |
|     235 |  425 | `	ph7_value_double(pVal, PH7_INF_VALUE());` |
|     235 |  426 | `}` |
|       - |  427 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|       - |  428 |  |
|       - |  429 | `#ifndef __WINNT__` |
|       - |  430 | `#include <time.h>` |
|       - |  431 | `#endif` |
|       - |  432 | `/*` |
|       - |  433 | ` * __TIME__` |
|       - |  434 | ` *  Expand the current time (GMT).` |
|       - |  435 | ` */` |
|      72 |  436 | `static void PH7_TIME_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  437 | `{` |
|       - |  438 | `	Sytm sTm;` |
|       - |  439 | `#ifdef __WINNT__` |
|       - |  440 | `	SYSTEMTIME sOS;` |
|       3 |  441 | `	GetSystemTime(&sOS);` |
|       3 |  442 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|       - |  443 | `#else` |
|       - |  444 | `	struct tm *pTm;` |
|       - |  445 | `	time_t t;` |
|      72 |  446 | `	time(&t);` |
|      72 |  447 | `	pTm = gmtime(&t);` |
|      72 |  448 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|       - |  449 | `#endif` |
|      36 |  450 | `	SXUNUSED(pUnused); /* cc warning */` |
|       - |  451 | `	/* Expand */` |
|      75 |  452 | `	ph7_value_string_format(pVal,"%02d:%02d:%02d",sTm.tm_hour,sTm.tm_min,sTm.tm_sec);` |
|      75 |  453 | `}` |
|       - |  454 | `/*` |
|       - |  455 | ` * __DATE__` |
|       - |  456 | ` *  Expand the current date in the ISO-8601 format.` |
|       - |  457 | ` */` |
|      72 |  458 | `static void PH7_DATE_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  459 | `{` |
|       - |  460 | `	Sytm sTm;` |
|       - |  461 | `#ifdef __WINNT__` |
|       - |  462 | `	SYSTEMTIME sOS;` |
|       3 |  463 | `	GetSystemTime(&sOS);` |
|       3 |  464 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|       - |  465 | `#else` |
|       - |  466 | `	struct tm *pTm;` |
|       - |  467 | `	time_t t;` |
|      72 |  468 | `	time(&t);` |
|      72 |  469 | `	pTm = gmtime(&t);` |
|      72 |  470 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|       - |  471 | `#endif` |
|      36 |  472 | `	SXUNUSED(pUnused); /* cc warning */` |
|       - |  473 | `	/* Expand */` |
|      75 |  474 | `	ph7_value_string_format(pVal,"%04qd-%02d-%02d",sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|      75 |  475 | `}` |
|       - |  476 | `/*` |
|       - |  477 | ` * __FILE__` |
|       - |  478 | ` *  Path of the processed script.` |
|       - |  479 | ` */` |
|      70 |  480 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  481 | `{` |
|      73 |  482 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - |  483 | `	SyString *pFile;` |
|       - |  484 | `	/* The unit the LITERAL is written in, which php fixes at compile time: the` |
|       - |  485 | `	 * declared file of the function running, else the unit on top of the include` |
|       - |  486 | `	 * stack. Reading the stack top alone answered the unit currently being LOADED,` |
|       - |  487 | `	 * so a function defined in one file and called from an include (or from an` |
|       - |  488 | `	 * eval()'d chunk, whose own name is now such an entry) reported the caller's` |
|       - |  489 | `	 * file as its own. */` |
|      73 |  490 | `	pFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|      73 |  491 | `	if( pFile == 0 ){` |
|       - |  492 | `		/* Expand the magic word: ":MEMORY:" */` |
|     ! 0 |  493 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|     ! 0 |  494 | `	}else{` |
|      73 |  495 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|       - |  496 | `	}` |
|      73 |  497 | `}` |
|       - |  498 | `/*` |
|       - |  499 | ` * __DIR__` |
|       - |  500 | ` *  Directory holding the processed script.` |
|       - |  501 | ` */` |
|      70 |  502 | `static void PH7_DIR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  503 | `{` |
|      73 |  504 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - |  505 | `	SyString *pFile;` |
|       - |  506 | `	/* Same question as __FILE__, one directory up. */` |
|      73 |  507 | `	pFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|      73 |  508 | `	if( pFile == 0 ){` |
|       - |  509 | `		/* Expand the magic word: ":MEMORY:" */` |
|     ! 0 |  510 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|     ! 0 |  511 | `	}else{` |
|      73 |  512 | `		if( pFile->nByte > 0 ){` |
|       - |  513 | `			const char *zDir;` |
|       - |  514 | `			int nLen;` |
|      73 |  515 | `			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|      73 |  516 | `			ph7_value_string(pVal,zDir,nLen);` |
|      38 |  517 | `		}else{` |
|       - |  518 | `			/* Expand '.' as the current directory*/` |
|     ! 0 |  519 | `			ph7_value_string(pVal,".",(int)sizeof(char));` |
|       - |  520 | `		}` |
|       - |  521 | `	}` |
|      73 |  522 | `}` |
|       - |  523 | `/*` |
|       - |  524 | ` * PHP_SHLIB_SUFFIX` |
|       - |  525 | ` *  Expand shared library suffix.` |
|       - |  526 | ` */` |
|      72 |  527 | `static void PH7_PHP_SHLIB_SUFFIX_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  528 | `{` |
|       - |  529 | `#ifdef __WINNT__` |
|       3 |  530 | `	ph7_value_string(pVal,"dll",(int)sizeof("dll")-1);` |
|       - |  531 | `#else` |
|      72 |  532 | `	ph7_value_string(pVal,"so",(int)sizeof("so")-1);` |
|       - |  533 | `#endif` |
|      36 |  534 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 |  535 | `}` |
|       - |  536 | `/*` |
|       - |  537 | ` * E_ERROR` |
|       - |  538 | ` *  Expands 1` |
|       - |  539 | ` */` |
|      76 |  540 | `static void PH7_E_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  541 | `{` |
|      79 |  542 | `	ph7_value_int(pVal,1);` |
|      38 |  543 | `	SXUNUSED(pUserData);` |
|      79 |  544 | `}` |
|       - |  545 | `/*` |
|       - |  546 | ` * E_WARNING` |
|       - |  547 | ` *  Expands 2` |
|       - |  548 | ` */` |
|      82 |  549 | `static void PH7_E_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  550 | `{` |
|      85 |  551 | `	ph7_value_int(pVal,2);` |
|      41 |  552 | `	SXUNUSED(pUserData);` |
|      85 |  553 | `}` |
|       - |  554 | `/*` |
|       - |  555 | ` * E_PARSE` |
|       - |  556 | ` *  Expands 4` |
|       - |  557 | ` */` |
|      72 |  558 | `static void PH7_E_PARSE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  559 | `{` |
|      75 |  560 | `	ph7_value_int(pVal,4);` |
|      36 |  561 | `	SXUNUSED(pUserData);` |
|      75 |  562 | `}` |
|       - |  563 | `/*` |
|       - |  564 | ` * E_NOTICE` |
|       - |  565 | ` * Expands 8` |
|       - |  566 | ` */` |
|      76 |  567 | `static void PH7_E_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  568 | `{` |
|      79 |  569 | `	ph7_value_int(pVal,8);` |
|      38 |  570 | `	SXUNUSED(pUserData);` |
|      79 |  571 | `}` |
|       - |  572 | `/*` |
|       - |  573 | ` * E_CORE_ERROR` |
|       - |  574 | ` * Expands 16` |
|       - |  575 | ` */` |
|      72 |  576 | `static void PH7_E_CORE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  577 | `{` |
|      75 |  578 | `	ph7_value_int(pVal,16);` |
|      36 |  579 | `	SXUNUSED(pUserData);` |
|      75 |  580 | `}` |
|       - |  581 | `/*` |
|       - |  582 | ` * E_CORE_WARNING` |
|       - |  583 | ` * Expands 32` |
|       - |  584 | ` */` |
|      72 |  585 | `static void PH7_E_CORE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  586 | `{` |
|      75 |  587 | `	ph7_value_int(pVal,32);` |
|      36 |  588 | `	SXUNUSED(pUserData);` |
|      75 |  589 | `}` |
|       - |  590 | `/*` |
|       - |  591 | ` * E_COMPILE_ERROR` |
|       - |  592 | ` * Expands 64` |
|       - |  593 | ` */` |
|      72 |  594 | `static void PH7_E_COMPILE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  595 | `{` |
|      75 |  596 | `	ph7_value_int(pVal,64);` |
|      36 |  597 | `	SXUNUSED(pUserData);` |
|      75 |  598 | `}` |
|       - |  599 | `/*` |
|       - |  600 | ` * E_COMPILE_WARNING` |
|       - |  601 | ` * Expands 128` |
|       - |  602 | ` */` |
|      72 |  603 | `static void PH7_E_COMPILE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  604 | `{` |
|      75 |  605 | `	ph7_value_int(pVal,128);` |
|      36 |  606 | `	SXUNUSED(pUserData);` |
|      75 |  607 | `}` |
|       - |  608 | `/*` |
|       - |  609 | ` * E_USER_ERROR` |
|       - |  610 | ` * Expands 256` |
|       - |  611 | ` */` |
|      76 |  612 | `static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  613 | `{` |
|      80 |  614 | `	ph7_value_int(pVal,256);` |
|      38 |  615 | `	SXUNUSED(pUserData);` |
|      80 |  616 | `}` |
|       - |  617 | `/*` |
|       - |  618 | ` * E_USER_WARNING` |
|       - |  619 | ` * Expands 512` |
|       - |  620 | ` */` |
|     118 |  621 | `static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  622 | `{` |
|     122 |  623 | `	ph7_value_int(pVal,512);` |
|      59 |  624 | `	SXUNUSED(pUserData);` |
|     122 |  625 | `}` |
|       - |  626 | `/*` |
|       - |  627 | ` * E_USER_NOTICE` |
|       - |  628 | ` * Expands 1024` |
|       - |  629 | ` */` |
|     156 |  630 | `static void PH7_E_USER_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  631 | `{` |
|     161 |  632 | `	ph7_value_int(pVal,1024);` |
|      78 |  633 | `	SXUNUSED(pUserData);` |
|     161 |  634 | `}` |
|       - |  635 | `/*` |
|       - |  636 | ` * E_RECOVERABLE_ERROR` |
|       - |  637 | ` * Expands 4096` |
|       - |  638 | ` */` |
|      72 |  639 | `static void PH7_E_RECOVERABLE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  640 | `{` |
|      75 |  641 | `	ph7_value_int(pVal,4096);` |
|      36 |  642 | `	SXUNUSED(pUserData);` |
|      75 |  643 | `}` |
|       - |  644 | `/*` |
|       - |  645 | ` * E_DEPRECATED` |
|       - |  646 | ` * Expands 8192` |
|       - |  647 | ` */` |
|     100 |  648 | `static void PH7_E_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  649 | `{` |
|     103 |  650 | `	ph7_value_int(pVal,8192);` |
|      50 |  651 | `	SXUNUSED(pUserData);` |
|     103 |  652 | `}` |
|       - |  653 | `/*` |
|       - |  654 | ` * E_USER_DEPRECATED` |
|       - |  655 | ` *   Expands 16384.` |
|       - |  656 | ` */` |
|      80 |  657 | `static void PH7_E_USER_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  658 | `{` |
|      83 |  659 | `	ph7_value_int(pVal,16384);` |
|      40 |  660 | `	SXUNUSED(pUserData);` |
|      83 |  661 | `}` |
|       - |  662 | `/*` |
|       - |  663 | ` * E_ALL` |
|       - |  664 | ` *  Expands 30719 (php 8: E_STRICT is no longer part of E_ALL)` |
|       - |  665 | ` */` |
|     159 |  666 | `static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  667 | `{` |
|     164 |  668 | `	ph7_value_int(pVal,PH7_E_ALL_MASK);` |
|      79 |  669 | `	SXUNUSED(pUserData);` |
|     164 |  670 | `}` |
|       - |  671 | `/*` |
|       - |  672 | ` * CASE_LOWER` |
|       - |  673 | ` *  Expands 0.` |
|       - |  674 | ` */` |
|      73 |  675 | `static void PH7_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  676 | `{` |
|      76 |  677 | `	ph7_value_int(pVal,0);` |
|      36 |  678 | `	SXUNUSED(pUserData);` |
|      76 |  679 | `}` |
|       - |  680 | `/*` |
|       - |  681 | ` * CASE_UPPER` |
|       - |  682 | ` *  Expands 1.` |
|       - |  683 | ` */` |
|      79 |  684 | `static void PH7_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  685 | `{` |
|      82 |  686 | `	ph7_value_int(pVal,1);` |
|      39 |  687 | `	SXUNUSED(pUserData);` |
|      82 |  688 | `}` |
|       - |  689 | `/*` |
|       - |  690 | ` * STR_PAD_LEFT` |
|       - |  691 | ` *  Expands 0.` |
|       - |  692 | ` */` |
|     103 |  693 | `static void PH7_STR_PAD_LEFT_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  694 | `{` |
|     106 |  695 | `	ph7_value_int(pVal,0);` |
|      51 |  696 | `	SXUNUSED(pUserData);` |
|     106 |  697 | `}` |
|       - |  698 | `/*` |
|       - |  699 | ` * STR_PAD_RIGHT` |
|       - |  700 | ` *  Expands 1.` |
|       - |  701 | ` */` |
|      85 |  702 | `static void PH7_STR_PAD_RIGHT_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  703 | `{` |
|      89 |  704 | `	ph7_value_int(pVal,1);` |
|      42 |  705 | `	SXUNUSED(pUserData);` |
|      89 |  706 | `}` |
|       - |  707 | `/*` |
|       - |  708 | ` * STR_PAD_BOTH` |
|       - |  709 | ` *  Expands 2.` |
|       - |  710 | ` */` |
|      75 |  711 | `static void PH7_STR_PAD_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  712 | `{` |
|      78 |  713 | `	ph7_value_int(pVal,2);` |
|      37 |  714 | `	SXUNUSED(pUserData);` |
|      78 |  715 | `}` |
|       - |  716 | `/*` |
|       - |  717 | ` * stream_wrapper_register()'s $flags. php defines exactly this one bit: the` |
|       - |  718 | ` * wrapper speaks to the network, so allow_url_fopen gates opening it and` |
|       - |  719 | ` * allow_url_include gates INCLUDING it.` |
|       - |  720 | ` */` |
|      75 |  721 | `static void PH7_STREAM_IS_URL_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  722 | `{` |
|      79 |  723 | `	ph7_value_int(pVal,PH7_STREAM_IS_URL);` |
|      37 |  724 | `	SXUNUSED(pUserData);` |
|      79 |  725 | `}` |
|       - |  726 | `/*` |
|       - |  727 | ` * The rest of the streamWrapper PROTOCOL vocabulary: the numbers php hands a` |
|       - |  728 | ` * userland wrapper, and the ones a wrapper hands back. PHL registered wrappers` |
|       - |  729 | ` * without them, so the ordinary spellings every real wrapper is written against --` |
|       - |  730 | ``  * `$options & STREAM_USE_PATH` in stream_open(), `$flags & STREAM_URL_STAT_QUIET` `` |
|       - |  731 | ` * in url_stat(), the STREAM_META_* verb in stream_metadata() -- were an undefined` |
|       - |  732 | ` * constant, i.e. an Error, in code php runs.` |
|       - |  733 | ` */` |
|      68 |  734 | `static void PH7_STREAM_USE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  735 | `{` |
|      71 |  736 | `	ph7_value_int(pVal,PH7_STREAM_USE_PATH);` |
|      34 |  737 | `	SXUNUSED(pUserData);` |
|      71 |  738 | `}` |
|      68 |  739 | `static void PH7_STREAM_IGNORE_URL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  740 | `{` |
|      71 |  741 | `	ph7_value_int(pVal,PH7_STREAM_IGNORE_URL);` |
|      34 |  742 | `	SXUNUSED(pUserData);` |
|      71 |  743 | `}` |
|      68 |  744 | `static void PH7_STREAM_REPORT_ERRORS_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  745 | `{` |
|      71 |  746 | `	ph7_value_int(pVal,PH7_STREAM_REPORT_ERRORS);` |
|      34 |  747 | `	SXUNUSED(pUserData);` |
|      71 |  748 | `}` |
|      68 |  749 | `static void PH7_STREAM_MUST_SEEK_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  750 | `{` |
|      71 |  751 | `	ph7_value_int(pVal,PH7_STREAM_MUST_SEEK);` |
|      34 |  752 | `	SXUNUSED(pUserData);` |
|      71 |  753 | `}` |
|      68 |  754 | `static void PH7_STREAM_URL_STAT_LINK_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  755 | `{` |
|      71 |  756 | `	ph7_value_int(pVal,PH7_URL_STAT_LINK);` |
|      34 |  757 | `	SXUNUSED(pUserData);` |
|      71 |  758 | `}` |
|      68 |  759 | `static void PH7_STREAM_URL_STAT_QUIET_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  760 | `{` |
|      71 |  761 | `	ph7_value_int(pVal,PH7_URL_STAT_QUIET);` |
|      34 |  762 | `	SXUNUSED(pUserData);` |
|      71 |  763 | `}` |
|      68 |  764 | `static void PH7_STREAM_MKDIR_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  765 | `{` |
|      71 |  766 | `	ph7_value_int(pVal,PH7_STREAM_MKDIR_RECURSIVE);` |
|      34 |  767 | `	SXUNUSED(pUserData);` |
|      71 |  768 | `}` |
|      68 |  769 | `static void PH7_STREAM_META_TOUCH_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  770 | `{` |
|      71 |  771 | `	ph7_value_int(pVal,PH7_STREAM_META_TOUCH);` |
|      34 |  772 | `	SXUNUSED(pUserData);` |
|      71 |  773 | `}` |
|      68 |  774 | `static void PH7_STREAM_META_OWNER_NAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  775 | `{` |
|      71 |  776 | `	ph7_value_int(pVal,PH7_STREAM_META_OWNER_NAME);` |
|      34 |  777 | `	SXUNUSED(pUserData);` |
|      71 |  778 | `}` |
|      68 |  779 | `static void PH7_STREAM_META_OWNER_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  780 | `{` |
|      71 |  781 | `	ph7_value_int(pVal,PH7_STREAM_META_OWNER);` |
|      34 |  782 | `	SXUNUSED(pUserData);` |
|      71 |  783 | `}` |
|      68 |  784 | `static void PH7_STREAM_META_GROUP_NAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  785 | `{` |
|      71 |  786 | `	ph7_value_int(pVal,PH7_STREAM_META_GROUP_NAME);` |
|      34 |  787 | `	SXUNUSED(pUserData);` |
|      71 |  788 | `}` |
|      68 |  789 | `static void PH7_STREAM_META_GROUP_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  790 | `{` |
|      71 |  791 | `	ph7_value_int(pVal,PH7_STREAM_META_GROUP);` |
|      34 |  792 | `	SXUNUSED(pUserData);` |
|      71 |  793 | `}` |
|      68 |  794 | `static void PH7_STREAM_META_ACCESS_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  795 | `{` |
|      71 |  796 | `	ph7_value_int(pVal,PH7_STREAM_META_ACCESS);` |
|      34 |  797 | `	SXUNUSED(pUserData);` |
|      71 |  798 | `}` |
|      68 |  799 | `static void PH7_STREAM_OPTION_BLOCKING_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  800 | `{` |
|      71 |  801 | `	ph7_value_int(pVal,PH7_STREAM_OPTION_BLOCKING);` |
|      34 |  802 | `	SXUNUSED(pUserData);` |
|      71 |  803 | `}` |
|      68 |  804 | `static void PH7_STREAM_OPTION_READ_BUFFER_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  805 | `{` |
|      71 |  806 | `	ph7_value_int(pVal,PH7_STREAM_OPTION_READ_BUFFER);` |
|      34 |  807 | `	SXUNUSED(pUserData);` |
|      71 |  808 | `}` |
|      68 |  809 | `static void PH7_STREAM_OPTION_WRITE_BUFFER_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  810 | `{` |
|      71 |  811 | `	ph7_value_int(pVal,PH7_STREAM_OPTION_WRITE_BUFFER);` |
|      34 |  812 | `	SXUNUSED(pUserData);` |
|      71 |  813 | `}` |
|      68 |  814 | `static void PH7_STREAM_OPTION_READ_TIMEOUT_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  815 | `{` |
|      71 |  816 | `	ph7_value_int(pVal,PH7_STREAM_OPTION_READ_TIMEOUT);` |
|      34 |  817 | `	SXUNUSED(pUserData);` |
|      71 |  818 | `}` |
|      68 |  819 | `static void PH7_STREAM_BUFFER_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  820 | `{` |
|      71 |  821 | `	ph7_value_int(pVal,PH7_STREAM_BUFFER_NONE);` |
|      34 |  822 | `	SXUNUSED(pUserData);` |
|      71 |  823 | `}` |
|      68 |  824 | `static void PH7_STREAM_BUFFER_LINE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  825 | `{` |
|      71 |  826 | `	ph7_value_int(pVal,PH7_STREAM_BUFFER_LINE);` |
|      34 |  827 | `	SXUNUSED(pUserData);` |
|      71 |  828 | `}` |
|      68 |  829 | `static void PH7_STREAM_BUFFER_FULL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  830 | `{` |
|      71 |  831 | `	ph7_value_int(pVal,PH7_STREAM_BUFFER_FULL);` |
|      34 |  832 | `	SXUNUSED(pUserData);` |
|      71 |  833 | `}` |
|      68 |  834 | `static void PH7_STREAM_CAST_AS_STREAM_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  835 | `{` |
|      71 |  836 | `	ph7_value_int(pVal,PH7_STREAM_CAST_AS_STREAM);` |
|      34 |  837 | `	SXUNUSED(pUserData);` |
|      71 |  838 | `}` |
|      68 |  839 | `static void PH7_STREAM_CAST_FOR_SELECT_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  840 | `{` |
|      71 |  841 | `	ph7_value_int(pVal,PH7_STREAM_CAST_FOR_SELECT);` |
|      34 |  842 | `	SXUNUSED(pUserData);` |
|      71 |  843 | `}` |
|       - |  844 | `/*` |
|       - |  845 | ` * A userland filter's ANSWER, and which kind of call it is answering. FEED_ME` |
|       - |  846 | ` * says "I produced nothing, ask me again with more"; ERR_FATAL ends the stream.` |
|       - |  847 | ` */` |
|     103 |  848 | `static void PH7_PSFS_PASS_ON_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  849 | `{` |
|     106 |  850 | `	ph7_value_int(pVal,PHL_PSFS_PASS_ON);` |
|      51 |  851 | `	SXUNUSED(pUserData);` |
|     106 |  852 | `}` |
|      75 |  853 | `static void PH7_PSFS_FEED_ME_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  854 | `{` |
|      78 |  855 | `	ph7_value_int(pVal,PHL_PSFS_FEED_ME);` |
|      37 |  856 | `	SXUNUSED(pUserData);` |
|      78 |  857 | `}` |
|      73 |  858 | `static void PH7_PSFS_ERR_FATAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  859 | `{` |
|      76 |  860 | `	ph7_value_int(pVal,PHL_PSFS_ERR_FATAL);` |
|      36 |  861 | `	SXUNUSED(pUserData);` |
|      76 |  862 | `}` |
|      71 |  863 | `static void PH7_PSFS_FLAG_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  864 | `{` |
|      74 |  865 | `	ph7_value_int(pVal,PHL_PSFS_FLAG_NORMAL);` |
|      35 |  866 | `	SXUNUSED(pUserData);` |
|      74 |  867 | `}` |
|      71 |  868 | `static void PH7_PSFS_FLAG_FLUSH_INC_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  869 | `{` |
|      74 |  870 | `	ph7_value_int(pVal,PHL_PSFS_FLAG_FLUSH_INC);` |
|      35 |  871 | `	SXUNUSED(pUserData);` |
|      74 |  872 | `}` |
|      71 |  873 | `static void PH7_PSFS_FLAG_FLUSH_CLOSE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  874 | `{` |
|      74 |  875 | `	ph7_value_int(pVal,PHL_PSFS_FLAG_FLUSH_CLOSE);` |
|      35 |  876 | `	SXUNUSED(pUserData);` |
|      74 |  877 | `}` |
|       - |  878 | `/*` |
|       - |  879 | `` * The `notification` callback's first argument: WHICH event the stream layer is`` |
|       - |  880 | ` * reporting. php defines all ten whatever its build registered, so a script may` |
|       - |  881 | `` * name one no wrapper here raises -- an unmatched `case` is silent where a`` |
|       - |  882 | ` * missing constant is a fatal.` |
|       - |  883 | ` */` |
|      68 |  884 | `static void PH7_STREAM_NOTIFY_RESOLVE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  885 | `{` |
|      71 |  886 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_RESOLVE);` |
|      34 |  887 | `	SXUNUSED(pUserData);` |
|      71 |  888 | `}` |
|      68 |  889 | `static void PH7_STREAM_NOTIFY_CONNECT_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  890 | `{` |
|      71 |  891 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_CONNECT);` |
|      34 |  892 | `	SXUNUSED(pUserData);` |
|      71 |  893 | `}` |
|      68 |  894 | `static void PH7_STREAM_NOTIFY_AUTH_REQUIRED_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  895 | `{` |
|      71 |  896 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_AUTH_REQUIRED);` |
|      34 |  897 | `	SXUNUSED(pUserData);` |
|      71 |  898 | `}` |
|      68 |  899 | `static void PH7_STREAM_NOTIFY_MIME_TYPE_IS_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  900 | `{` |
|      71 |  901 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_MIME_TYPE_IS);` |
|      34 |  902 | `	SXUNUSED(pUserData);` |
|      71 |  903 | `}` |
|      68 |  904 | `static void PH7_STREAM_NOTIFY_FILE_SIZE_IS_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  905 | `{` |
|      71 |  906 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_FILE_SIZE_IS);` |
|      34 |  907 | `	SXUNUSED(pUserData);` |
|      71 |  908 | `}` |
|      68 |  909 | `static void PH7_STREAM_NOTIFY_REDIRECTED_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  910 | `{` |
|      71 |  911 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_REDIRECTED);` |
|      34 |  912 | `	SXUNUSED(pUserData);` |
|      71 |  913 | `}` |
|      68 |  914 | `static void PH7_STREAM_NOTIFY_PROGRESS_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  915 | `{` |
|      71 |  916 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_PROGRESS);` |
|      34 |  917 | `	SXUNUSED(pUserData);` |
|      71 |  918 | `}` |
|      68 |  919 | `static void PH7_STREAM_NOTIFY_COMPLETED_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  920 | `{` |
|      71 |  921 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_COMPLETED);` |
|      34 |  922 | `	SXUNUSED(pUserData);` |
|      71 |  923 | `}` |
|      68 |  924 | `static void PH7_STREAM_NOTIFY_FAILURE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  925 | `{` |
|      71 |  926 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_FAILURE);` |
|      34 |  927 | `	SXUNUSED(pUserData);` |
|      71 |  928 | `}` |
|      68 |  929 | `static void PH7_STREAM_NOTIFY_AUTH_RESULT_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  930 | `{` |
|      71 |  931 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_AUTH_RESULT);` |
|      34 |  932 | `	SXUNUSED(pUserData);` |
|      71 |  933 | `}` |
|       - |  934 | `/* And the callback's second argument: how bad the event is. */` |
|      68 |  935 | `static void PH7_STREAM_NOTIFY_SEVERITY_INFO_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  936 | `{` |
|      71 |  937 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_SEVERITY_INFO);` |
|      34 |  938 | `	SXUNUSED(pUserData);` |
|      71 |  939 | `}` |
|      68 |  940 | `static void PH7_STREAM_NOTIFY_SEVERITY_WARN_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  941 | `{` |
|      71 |  942 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_SEVERITY_WARN);` |
|      34 |  943 | `	SXUNUSED(pUserData);` |
|      71 |  944 | `}` |
|      68 |  945 | `static void PH7_STREAM_NOTIFY_SEVERITY_ERR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  946 | `{` |
|      71 |  947 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_SEVERITY_ERR);` |
|      34 |  948 | `	SXUNUSED(pUserData);` |
|      71 |  949 | `}` |
|       - |  950 | `/*` |
|       - |  951 | ` * stream_filter_append()'s $mode — WHICH chain the filter joins. php's 0 is not` |
|       - |  952 | ` * "neither": it means "whichever chains the handle's own mode makes sense for".` |
|       - |  953 | ` */` |
|     203 |  954 | `static void PH7_STREAM_FILTER_READ_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  955 | `{` |
|     206 |  956 | `	ph7_value_int(pVal,PHL_STREAM_FILTER_READ);` |
|     101 |  957 | `	SXUNUSED(pUserData);` |
|     206 |  958 | `}` |
|     111 |  959 | `static void PH7_STREAM_FILTER_WRITE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  960 | `{` |
|     114 |  961 | `	ph7_value_int(pVal,PHL_STREAM_FILTER_WRITE);` |
|      55 |  962 | `	SXUNUSED(pUserData);` |
|     114 |  963 | `}` |
|      71 |  964 | `static void PH7_STREAM_FILTER_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  965 | `{` |
|      74 |  966 | `	ph7_value_int(pVal,PHL_STREAM_FILTER_ALL);` |
|      35 |  967 | `	SXUNUSED(pUserData);` |
|      74 |  968 | `}` |
|       - |  969 | `/*` |
|       - |  970 | ` * stream_socket_client()'s $flags. CONNECT is the default it documents;` |
|       - |  971 | ` * PERSISTENT is what pfsockopen() means and the only one that changes what a` |
|       - |  972 | ` * second call to the same address ANSWERS.` |
|       - |  973 | ` */` |
|      99 |  974 | `static void PH7_STREAM_CLIENT_CONNECT_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  975 | `{` |
|     103 |  976 | `	ph7_value_int(pVal,PH7_STREAM_CLIENT_CONNECT);` |
|      49 |  977 | `	SXUNUSED(pUserData);` |
|     103 |  978 | `}` |
|      75 |  979 | `static void PH7_STREAM_CLIENT_ASYNC_CONNECT_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  980 | `{` |
|      79 |  981 | `	ph7_value_int(pVal,PH7_STREAM_CLIENT_ASYNC_CONNECT);` |
|      37 |  982 | `	SXUNUSED(pUserData);` |
|      79 |  983 | `}` |
|      83 |  984 | `static void PH7_STREAM_CLIENT_PERSISTENT_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  985 | `{` |
|      87 |  986 | `	ph7_value_int(pVal,PH7_STREAM_CLIENT_PERSISTENT);` |
|      41 |  987 | `	SXUNUSED(pUserData);` |
|      87 |  988 | `}` |
|       - |  989 | `/*` |
|       - |  990 | ` * The socket-family constants. Their VALUES are the platform's own — AF_INET6 is` |
|       - |  991 | ` * 10 on Linux, 23 on Windows and 30 on the BSDs — so they are asked for by id` |
|       - |  992 | ` * rather than written down here, and a program handing one to` |
|       - |  993 | ` * stream_socket_pair() is handing the OS its own number.` |
|       - |  994 | ` */` |
|       - |  995 | `#ifdef PH7_ENABLE_NET` |
|      69 |  996 | `static void PH7_STREAM_PF_INET_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  997 | `{` |
|      72 |  998 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_INET));` |
|      34 |  999 | `	SXUNUSED(pUserData);` |
|      72 | 1000 | `}` |
|      69 | 1001 | `static void PH7_STREAM_PF_INET6_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1002 | `{` |
|      72 | 1003 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_INET6));` |
|      34 | 1004 | `	SXUNUSED(pUserData);` |
|      72 | 1005 | `}` |
|      75 | 1006 | `static void PH7_STREAM_PF_UNIX_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1007 | `{` |
|      78 | 1008 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_UNIX));` |
|      37 | 1009 | `	SXUNUSED(pUserData);` |
|      78 | 1010 | `}` |
|      73 | 1011 | `static void PH7_STREAM_SOCK_STREAM_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1012 | `{` |
|      76 | 1013 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_STREAM));` |
|      36 | 1014 | `	SXUNUSED(pUserData);` |
|      76 | 1015 | `}` |
|      69 | 1016 | `static void PH7_STREAM_SOCK_DGRAM_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1017 | `{` |
|      72 | 1018 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_DGRAM));` |
|      34 | 1019 | `	SXUNUSED(pUserData);` |
|      72 | 1020 | `}` |
|      69 | 1021 | `static void PH7_STREAM_SOCK_RAW_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1022 | `{` |
|      72 | 1023 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_RAW));` |
|      34 | 1024 | `	SXUNUSED(pUserData);` |
|      72 | 1025 | `}` |
|      69 | 1026 | `static void PH7_STREAM_SOCK_SEQPACKET_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1027 | `{` |
|      72 | 1028 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_SEQPACKET));` |
|      34 | 1029 | `	SXUNUSED(pUserData);` |
|      72 | 1030 | `}` |
|      69 | 1031 | `static void PH7_STREAM_SOCK_RDM_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1032 | `{` |
|      72 | 1033 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_RDM));` |
|      34 | 1034 | `	SXUNUSED(pUserData);` |
|      72 | 1035 | `}` |
|      69 | 1036 | `static void PH7_STREAM_IPPROTO_IP_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1037 | `{` |
|      72 | 1038 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_IP));` |
|      34 | 1039 | `	SXUNUSED(pUserData);` |
|      72 | 1040 | `}` |
|      69 | 1041 | `static void PH7_STREAM_IPPROTO_TCP_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1042 | `{` |
|      72 | 1043 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_TCP));` |
|      34 | 1044 | `	SXUNUSED(pUserData);` |
|      72 | 1045 | `}` |
|      69 | 1046 | `static void PH7_STREAM_IPPROTO_UDP_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1047 | `{` |
|      72 | 1048 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_UDP));` |
|      34 | 1049 | `	SXUNUSED(pUserData);` |
|      72 | 1050 | `}` |
|      69 | 1051 | `static void PH7_STREAM_IPPROTO_ICMP_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1052 | `{` |
|      72 | 1053 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_ICMP));` |
|      34 | 1054 | `	SXUNUSED(pUserData);` |
|      72 | 1055 | `}` |
|      69 | 1056 | `static void PH7_STREAM_IPPROTO_RAW_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1057 | `{` |
|      72 | 1058 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_RAW));` |
|      34 | 1059 | `	SXUNUSED(pUserData);` |
|      72 | 1060 | `}` |
|       - | 1061 | `#endif /* PH7_ENABLE_NET */` |
|       - | 1062 | `/*` |
|       - | 1063 | ` * stream_socket_shutdown()'s $mode, and the two recvfrom/sendto flags. These` |
|       - | 1064 | ` * three ARE php's own numbers rather than the OS's: php maps STREAM_OOB and` |
|       - | 1065 | ` * STREAM_PEEK onto MSG_OOB/MSG_PEEK itself.` |
|       - | 1066 | ` */` |
|      73 | 1067 | `static void PH7_STREAM_SHUT_RD_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1068 | `{` |
|      76 | 1069 | `	ph7_value_int(pVal,PH7_STREAM_SHUT_RD);` |
|      36 | 1070 | `	SXUNUSED(pUserData);` |
|      76 | 1071 | `}` |
|      71 | 1072 | `static void PH7_STREAM_SHUT_WR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1073 | `{` |
|      74 | 1074 | `	ph7_value_int(pVal,PH7_STREAM_SHUT_WR);` |
|      35 | 1075 | `	SXUNUSED(pUserData);` |
|      74 | 1076 | `}` |
|      71 | 1077 | `static void PH7_STREAM_SHUT_RDWR_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1078 | `{` |
|      75 | 1079 | `	ph7_value_int(pVal,PH7_STREAM_SHUT_RDWR);` |
|      35 | 1080 | `	SXUNUSED(pUserData);` |
|      75 | 1081 | `}` |
|      69 | 1082 | `static void PH7_STREAM_OOB_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1083 | `{` |
|      72 | 1084 | `	ph7_value_int(pVal,PH7_STREAM_OOB);` |
|      34 | 1085 | `	SXUNUSED(pUserData);` |
|      72 | 1086 | `}` |
|      73 | 1087 | `static void PH7_STREAM_PEEK_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1088 | `{` |
|      77 | 1089 | `	ph7_value_int(pVal,PH7_STREAM_PEEK);` |
|      36 | 1090 | `	SXUNUSED(pUserData);` |
|      77 | 1091 | `}` |
|       - | 1092 | `/*` |
|       - | 1093 | ` * stream_socket_server()'s $flags. Its default is BIND\|LISTEN, and the two are` |
|       - | 1094 | ` * separate because binding is all a datagram server does.` |
|       - | 1095 | ` */` |
|      93 | 1096 | `static void PH7_STREAM_SERVER_BIND_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1097 | `{` |
|      97 | 1098 | `	ph7_value_int(pVal,PH7_STREAM_SERVER_BIND);` |
|      46 | 1099 | `	SXUNUSED(pUserData);` |
|      97 | 1100 | `}` |
|      81 | 1101 | `static void PH7_STREAM_SERVER_LISTEN_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1102 | `{` |
|      84 | 1103 | `	ph7_value_int(pVal,PH7_STREAM_SERVER_LISTEN);` |
|      40 | 1104 | `	SXUNUSED(pUserData);` |
|      84 | 1105 | `}` |
|       - | 1106 | `/*` |
|       - | 1107 | ` * mt_srand()'s $mode: which GENERATOR to seed. MT_RAND_PHP is php's pre-7.1` |
|       - | 1108 | ` * Mersenne Twister, whose twist reads the low bit of the wrong word — a` |
|       - | 1109 | ` * different sequence, which is the only reason to ask for it.` |
|       - | 1110 | ` */` |
|      80 | 1111 | `static void PH7_MT_RAND_MT19937_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1112 | `{` |
|      84 | 1113 | `	ph7_value_int(pVal,PH7_MT_RAND_MT19937);` |
|      40 | 1114 | `	SXUNUSED(pUserData);` |
|      84 | 1115 | `}` |
|      90 | 1116 | `static void PH7_MT_RAND_PHP_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1117 | `{` |
|       - | 1118 | `	/* php 8.3 deprecated the SYMBOL as well as the mode. The notice is` |
|       - | 1119 | `	 * aDeprecatedConst[]'s now, raised where every deprecated constant's is. */` |
|      94 | 1120 | `	ph7_value_int(pVal,PH7_MT_RAND_PHP);` |
|      45 | 1121 | `	SXUNUSED(pUserData);` |
|      94 | 1122 | `}` |
|       - | 1123 | `/*` |
|       - | 1124 | ` * Output-handler flags and phases (ob_start()'s $flags, and the $phase an output` |
|       - | 1125 | ` * handler is called with). The values are php's and are a public ABI: the phase` |
|       - | 1126 | ` * bits are OR'd together (a first FLUSH arrives as FLUSH\|START = 5), and the` |
|       - | 1127 | ` * three capability flags are the ones ob_clean()/ob_flush()/ob_end_*() test` |
|       - | 1128 | ` * before they will touch the buffer.` |
|       - | 1129 | ` */` |
|     144 | 1130 | `static void PH7_OB_WRITE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1131 | `{` |
|     148 | 1132 | `	ph7_value_int(pVal,PH7_OB_WRITE);` |
|      72 | 1133 | `	SXUNUSED(pUserData);` |
|     148 | 1134 | `}` |
|      72 | 1135 | `static void PH7_OB_START_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1136 | `{` |
|      76 | 1137 | `	ph7_value_int(pVal,PH7_OB_START);` |
|      36 | 1138 | `	SXUNUSED(pUserData);` |
|      76 | 1139 | `}` |
|      72 | 1140 | `static void PH7_OB_CLEAN_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1141 | `{` |
|      76 | 1142 | `	ph7_value_int(pVal,PH7_OB_CLEAN);` |
|      36 | 1143 | `	SXUNUSED(pUserData);` |
|      76 | 1144 | `}` |
|      72 | 1145 | `static void PH7_OB_FLUSH_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1146 | `{` |
|      76 | 1147 | `	ph7_value_int(pVal,PH7_OB_FLUSH);` |
|      36 | 1148 | `	SXUNUSED(pUserData);` |
|      76 | 1149 | `}` |
|     144 | 1150 | `static void PH7_OB_FINAL_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1151 | `{` |
|     148 | 1152 | `	ph7_value_int(pVal,PH7_OB_FINAL);` |
|      72 | 1153 | `	SXUNUSED(pUserData);` |
|     148 | 1154 | `}` |
|      76 | 1155 | `static void PH7_OB_CLEANABLE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1156 | `{` |
|      80 | 1157 | `	ph7_value_int(pVal,PH7_OB_CLEANABLE);` |
|      38 | 1158 | `	SXUNUSED(pUserData);` |
|      80 | 1159 | `}` |
|      76 | 1160 | `static void PH7_OB_FLUSHABLE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1161 | `{` |
|      80 | 1162 | `	ph7_value_int(pVal,PH7_OB_FLUSHABLE);` |
|      38 | 1163 | `	SXUNUSED(pUserData);` |
|      80 | 1164 | `}` |
|      78 | 1165 | `static void PH7_OB_REMOVABLE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1166 | `{` |
|      82 | 1167 | `	ph7_value_int(pVal,PH7_OB_REMOVABLE);` |
|      39 | 1168 | `	SXUNUSED(pUserData);` |
|      82 | 1169 | `}` |
|      86 | 1170 | `static void PH7_OB_STDFLAGS_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1171 | `{` |
|      91 | 1172 | `	ph7_value_int(pVal,PH7_OB_STDFLAGS);` |
|      43 | 1173 | `	SXUNUSED(pUserData);` |
|      91 | 1174 | `}` |
|      74 | 1175 | `static void PH7_OB_STARTED_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1176 | `{` |
|      79 | 1177 | `	ph7_value_int(pVal,PH7_OB_STARTED);` |
|      37 | 1178 | `	SXUNUSED(pUserData);` |
|      79 | 1179 | `}` |
|      74 | 1180 | `static void PH7_OB_DISABLED_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1181 | `{` |
|      79 | 1182 | `	ph7_value_int(pVal,PH7_OB_DISABLED);` |
|      37 | 1183 | `	SXUNUSED(pUserData);` |
|      79 | 1184 | `}` |
|      76 | 1185 | `static void PH7_OB_PROCESSED_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1186 | `{` |
|      81 | 1187 | `	ph7_value_int(pVal,PH7_OB_PROCESSED);` |
|      38 | 1188 | `	SXUNUSED(pUserData);` |
|      81 | 1189 | `}` |
|       - | 1190 | `/*` |
|       - | 1191 | ` * array_filter()'s $mode selector. The VALUES are php's and are a public ABI --` |
|       - | 1192 | ` * ARRAY_FILTER_USE_BOTH is 1 and ARRAY_FILTER_USE_KEY is 2, NOT the other way` |
|       - | 1193 | ` * round, and they are a selector rather than a bit mask (php reads the argument` |
|       - | 1194 | ` * with ==, so any other number is the default value mode).` |
|       - | 1195 | ` */` |
|       - | 1196 | `/*` |
|       - | 1197 | ` * CONNECTION_NORMAL / CONNECTION_ABORTED / CONNECTION_TIMEOUT` |
|       - | 1198 | ` *  The three states connection_status() reports. On a CLI there is no client` |
|       - | 1199 | ` *  to disconnect and no time limit to run out, so NORMAL is the only one that` |
|       - | 1200 | ` *  is ever answered -- but the names are what a program COMPARES against, and` |
|       - | 1201 | ` *  an undefined constant is a fatal.` |
|       - | 1202 | ` */` |
|      73 | 1203 | `static void PH7_CONNECTION_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1204 | `{` |
|      76 | 1205 | `	ph7_value_int(pVal,0);` |
|      36 | 1206 | `	SXUNUSED(pUserData);` |
|      76 | 1207 | `}` |
|      71 | 1208 | `static void PH7_CONNECTION_ABORTED_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1209 | `{` |
|      74 | 1210 | `	ph7_value_int(pVal,1);` |
|      35 | 1211 | `	SXUNUSED(pUserData);` |
|      74 | 1212 | `}` |
|      71 | 1213 | `static void PH7_CONNECTION_TIMEOUT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1214 | `{` |
|      74 | 1215 | `	ph7_value_int(pVal,2);` |
|      35 | 1216 | `	SXUNUSED(pUserData);` |
|      74 | 1217 | `}` |
|      87 | 1218 | `static void PH7_ARRAY_FILTER_USE_KEY_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1219 | `{` |
|      90 | 1220 | `	ph7_value_int(pVal,2);` |
|      43 | 1221 | `	SXUNUSED(pUserData);` |
|      90 | 1222 | `}` |
|      79 | 1223 | `static void PH7_ARRAY_FILTER_USE_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1224 | `{` |
|      82 | 1225 | `	ph7_value_int(pVal,1);` |
|      39 | 1226 | `	SXUNUSED(pUserData);` |
|      82 | 1227 | `}` |
|       - | 1228 | `/*` |
|       - | 1229 | ` * COUNT_NORMAL` |
|       - | 1230 | ` *  Expands 0` |
|       - | 1231 | ` */` |
|      81 | 1232 | `static void PH7_COUNT_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1233 | `{` |
|      85 | 1234 | `	ph7_value_int(pVal,0);` |
|      40 | 1235 | `	SXUNUSED(pUserData);` |
|      85 | 1236 | `}` |
|       - | 1237 | `/*` |
|       - | 1238 | ` * COUNT_RECURSIVE` |
|       - | 1239 | ` *  Expands 1.` |
|       - | 1240 | ` */` |
|      89 | 1241 | `static void PH7_COUNT_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1242 | `{` |
|      93 | 1243 | `	ph7_value_int(pVal,1);` |
|      44 | 1244 | `	SXUNUSED(pUserData);` |
|      93 | 1245 | `}` |
|       - | 1246 | `/*` |
|       - | 1247 | ` * php's sort-flag constants. The VALUES must match php exactly: they are a` |
|       - | 1248 | ` * public ABI (code passes literal ints, dumps them, and OR-combines the base` |
|       - | 1249 | ` * type with SORT_FLAG_CASE). SORT_ASC/SORT_DESC are the array_multisort` |
|       - | 1250 | ` * direction flags.` |
|       - | 1251 | ` * SORT_REGULAR 0 · SORT_NUMERIC 1 · SORT_STRING 2 · SORT_DESC 3 · SORT_ASC 4 ·` |
|       - | 1252 | ` * SORT_LOCALE_STRING 5 · SORT_NATURAL 6 · SORT_FLAG_CASE 8` |
|       - | 1253 | ` */` |
|      85 | 1254 | `static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1255 | `{` |
|      90 | 1256 | `	ph7_value_int(pVal,4);` |
|      42 | 1257 | `	SXUNUSED(pUserData);` |
|      90 | 1258 | `}` |
|      81 | 1259 | `static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1260 | `{` |
|      85 | 1261 | `	ph7_value_int(pVal,3);` |
|      40 | 1262 | `	SXUNUSED(pUserData);` |
|      85 | 1263 | `}` |
|     103 | 1264 | `static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1265 | `{` |
|     106 | 1266 | `	ph7_value_int(pVal,0);` |
|      51 | 1267 | `	SXUNUSED(pUserData);` |
|     106 | 1268 | `}` |
|     139 | 1269 | `static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1270 | `{` |
|     143 | 1271 | `	ph7_value_int(pVal,1);` |
|      69 | 1272 | `	SXUNUSED(pUserData);` |
|     143 | 1273 | `}` |
|    2030 | 1274 | `static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1275 | `{` |
|    2035 | 1276 | `	ph7_value_int(pVal,2);` |
|    1014 | 1277 | `	SXUNUSED(pUserData);` |
|    2035 | 1278 | `}` |
|      73 | 1279 | `static void PH7_SORT_LOCALE_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1280 | `{` |
|      76 | 1281 | `	ph7_value_int(pVal,5);` |
|      36 | 1282 | `	SXUNUSED(pUserData);` |
|      76 | 1283 | `}` |
|      89 | 1284 | `static void PH7_SORT_NATURAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1285 | `{` |
|      92 | 1286 | `	ph7_value_int(pVal,6);` |
|      44 | 1287 | `	SXUNUSED(pUserData);` |
|      92 | 1288 | `}` |
|      93 | 1289 | `static void PH7_SORT_FLAG_CASE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1290 | `{` |
|      98 | 1291 | `	ph7_value_int(pVal,8);` |
|      46 | 1292 | `	SXUNUSED(pUserData);` |
|      98 | 1293 | `}` |
|       - | 1294 | `/*` |
|       - | 1295 | ` * PHP_ROUND_HALF_UP` |
|       - | 1296 | ` *  Expands 1.` |
|       - | 1297 | ` */` |
|      73 | 1298 | `static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1299 | `{` |
|      76 | 1300 | `	ph7_value_int(pVal,1);` |
|      36 | 1301 | `	SXUNUSED(pUserData);` |
|      76 | 1302 | `}` |
|       - | 1303 | `/*` |
|       - | 1304 | ` * PHP_SESSION_DISABLED / PHP_SESSION_NONE / PHP_SESSION_ACTIVE` |
|       - | 1305 | ` *  session_status() states (0 / 1 / 2).` |
|       - | 1306 | ` */` |
|      70 | 1307 | `static void PH7_PHP_SESSION_DISABLED_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1308 | `{` |
|      73 | 1309 | `	ph7_value_int(pVal,0);` |
|      35 | 1310 | `	SXUNUSED(pUserData);` |
|      73 | 1311 | `}` |
|      70 | 1312 | `static void PH7_PHP_SESSION_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1313 | `{` |
|      73 | 1314 | `	ph7_value_int(pVal,1);` |
|      35 | 1315 | `	SXUNUSED(pUserData);` |
|      73 | 1316 | `}` |
|      84 | 1317 | `static void PH7_PHP_SESSION_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1318 | `{` |
|      87 | 1319 | `	ph7_value_int(pVal,2);` |
|      42 | 1320 | `	SXUNUSED(pUserData);` |
|      87 | 1321 | `}` |
|       - | 1322 | `/*` |
|       - | 1323 | ` * INI_USER / INI_PERDIR / INI_SYSTEM / INI_ALL` |
|       - | 1324 | ` *  php.ini access levels (1 / 2 / 4 / 7).` |
|       - | 1325 | ` */` |
|      71 | 1326 | `static void PH7_INI_USER_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1327 | `{` |
|      75 | 1328 | `	ph7_value_int(pVal,1);` |
|      35 | 1329 | `	SXUNUSED(pUserData);` |
|      75 | 1330 | `}` |
|      71 | 1331 | `static void PH7_INI_PERDIR_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1332 | `{` |
|      75 | 1333 | `	ph7_value_int(pVal,2);` |
|      35 | 1334 | `	SXUNUSED(pUserData);` |
|      75 | 1335 | `}` |
|      71 | 1336 | `static void PH7_INI_SYSTEM_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1337 | `{` |
|      75 | 1338 | `	ph7_value_int(pVal,4);` |
|      35 | 1339 | `	SXUNUSED(pUserData);` |
|      75 | 1340 | `}` |
|      71 | 1341 | `static void PH7_INI_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1342 | `{` |
|      75 | 1343 | `	ph7_value_int(pVal,7);` |
|      35 | 1344 | `	SXUNUSED(pUserData);` |
|      75 | 1345 | `}` |
|       - | 1346 | `/*` |
|       - | 1347 | ` * MB_CASE_UPPER / MB_CASE_LOWER / MB_CASE_TITLE (0 / 1 / 2)` |
|       - | 1348 | ` */` |
|      72 | 1349 | `static void PH7_MB_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1350 | `{` |
|      75 | 1351 | `	ph7_value_int(pVal,0);` |
|      36 | 1352 | `	SXUNUSED(pUserData);` |
|      75 | 1353 | `}` |
|      72 | 1354 | `static void PH7_MB_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1355 | `{` |
|      75 | 1356 | `	ph7_value_int(pVal,1);` |
|      36 | 1357 | `	SXUNUSED(pUserData);` |
|      75 | 1358 | `}` |
|     108 | 1359 | `static void PH7_MB_CASE_TITLE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1360 | `{` |
|     111 | 1361 | `	ph7_value_int(pVal,2);` |
|      54 | 1362 | `	SXUNUSED(pUserData);` |
|     111 | 1363 | `}` |
|       - | 1364 | `/*` |
|       - | 1365 | ` * SPHP_ROUND_HALF_DOWN` |
|       - | 1366 | ` *  Expands 2.` |
|       - | 1367 | ` */` |
|      73 | 1368 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1369 | `{` |
|      76 | 1370 | `	ph7_value_int(pVal,2);` |
|      36 | 1371 | `	SXUNUSED(pUserData);` |
|      76 | 1372 | `}` |
|       - | 1373 | `/*` |
|       - | 1374 | ` * PHP_ROUND_HALF_EVEN` |
|       - | 1375 | ` *  Expands 3.` |
|       - | 1376 | ` */` |
|      77 | 1377 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1378 | `{` |
|      80 | 1379 | `	ph7_value_int(pVal,3);` |
|      38 | 1380 | `	SXUNUSED(pUserData);` |
|      80 | 1381 | `}` |
|       - | 1382 | `/*` |
|       - | 1383 | ` * PHP_ROUND_HALF_ODD` |
|       - | 1384 | ` *  Expands 4.` |
|       - | 1385 | ` */` |
|      73 | 1386 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1387 | `{` |
|      76 | 1388 | `	ph7_value_int(pVal,4);` |
|      36 | 1389 | `	SXUNUSED(pUserData);` |
|      76 | 1390 | `}` |
|       - | 1391 | `/*` |
|       - | 1392 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|       - | 1393 | ` *  Expand 0x01` |
|       - | 1394 | ` * NOTE:` |
|       - | 1395 | ` *  The expanded value must be a power of two.` |
|       - | 1396 | ` */` |
|      90 | 1397 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1398 | `{` |
|      93 | 1399 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      45 | 1400 | `	SXUNUSED(pUserData);` |
|      93 | 1401 | `}` |
|       - | 1402 | `/*` |
|       - | 1403 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|       - | 1404 | ` *  Expand 0x02` |
|       - | 1405 | ` * NOTE:` |
|       - | 1406 | ` *  The expanded value must be a power of two.` |
|       - | 1407 | ` */` |
|     108 | 1408 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1409 | `{` |
|     111 | 1410 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      54 | 1411 | `	SXUNUSED(pUserData);` |
|     111 | 1412 | `}` |
|       - | 1413 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|       - | 1414 | `/*` |
|       - | 1415 | ` * M_PI` |
|       - | 1416 | ` *  Expand the value of pi.` |
|       - | 1417 | ` */` |
|      83 | 1418 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1419 | `{` |
|      41 | 1420 | `	SXUNUSED(pUserData); /* cc warning */` |
|      86 | 1421 | `	ph7_value_double(pVal,PH7_PI);` |
|      86 | 1422 | `}` |
|       - | 1423 | `/*` |
|       - | 1424 | ` * M_E` |
|       - | 1425 | ` *  Expand 2.7182818284590452354` |
|       - | 1426 | ` */` |
|      79 | 1427 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1428 | `{` |
|      39 | 1429 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 1430 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|      83 | 1431 | `}` |
|       - | 1432 | `/*` |
|       - | 1433 | ` * M_LOG2E` |
|       - | 1434 | ` *  Expand 2.7182818284590452354` |
|       - | 1435 | ` */` |
|      71 | 1436 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1437 | `{` |
|      35 | 1438 | `	SXUNUSED(pUserData); /* cc warning */` |
|      74 | 1439 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      74 | 1440 | `}` |
|       - | 1441 | `/*` |
|       - | 1442 | ` * M_LOG10E` |
|       - | 1443 | ` *  Expand 0.4342944819032518276` |
|       - | 1444 | ` */` |
|      71 | 1445 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1446 | `{` |
|      35 | 1447 | `	SXUNUSED(pUserData); /* cc warning */` |
|      74 | 1448 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      74 | 1449 | `}` |
|       - | 1450 | `/*` |
|       - | 1451 | ` * M_LN2` |
|       - | 1452 | ` *  Expand 	0.69314718055994530942` |
|       - | 1453 | ` */` |
|      71 | 1454 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1455 | `{` |
|      35 | 1456 | `	SXUNUSED(pUserData); /* cc warning */` |
|      74 | 1457 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      74 | 1458 | `}` |
|       - | 1459 | `/*` |
|       - | 1460 | ` * M_LN10` |
|       - | 1461 | ` *  Expand 	2.30258509299404568402` |
|       - | 1462 | ` */` |
|      71 | 1463 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1464 | `{` |
|      35 | 1465 | `	SXUNUSED(pUserData); /* cc warning */` |
|      74 | 1466 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      74 | 1467 | `}` |
|       - | 1468 | `/*` |
|       - | 1469 | ` * M_PI_2` |
|       - | 1470 | ` *  Expand 	1.57079632679489661923` |
|       - | 1471 | ` */` |
|      71 | 1472 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1473 | `{` |
|      35 | 1474 | `	SXUNUSED(pUserData); /* cc warning */` |
|      74 | 1475 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      74 | 1476 | `}` |
|       - | 1477 | `/*` |
|       - | 1478 | ` * M_PI_4` |
|       - | 1479 | ` *  Expand 	0.78539816339744830962` |
|       - | 1480 | ` */` |
|      71 | 1481 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1482 | `{` |
|      35 | 1483 | `	SXUNUSED(pUserData); /* cc warning */` |
|      74 | 1484 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      74 | 1485 | `}` |
|       - | 1486 | `/*` |
|       - | 1487 | ` * M_1_PI` |
|       - | 1488 | ` *  Expand 	0.31830988618379067154` |
|       - | 1489 | ` */` |
|      71 | 1490 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1491 | `{` |
|      35 | 1492 | `	SXUNUSED(pUserData); /* cc warning */` |
|      74 | 1493 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      74 | 1494 | `}` |
|       - | 1495 | `/*` |
|       - | 1496 | ` * M_2_PI` |
|       - | 1497 | ` *  Expand 0.63661977236758134308` |
|       - | 1498 | ` */` |
|      73 | 1499 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1500 | `{` |
|      36 | 1501 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1502 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      76 | 1503 | `}` |
|       - | 1504 | `/*` |
|       - | 1505 | ` * M_SQRTPI` |
|       - | 1506 | ` *  Expand 1.77245385090551602729` |
|       - | 1507 | ` */` |
|      71 | 1508 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1509 | `{` |
|      35 | 1510 | `	SXUNUSED(pUserData); /* cc warning */` |
|      74 | 1511 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      74 | 1512 | `}` |
|       - | 1513 | `/*` |
|       - | 1514 | ` * M_2_SQRTPI` |
|       - | 1515 | ` *  Expand 	1.12837916709551257390` |
|       - | 1516 | ` */` |
|      71 | 1517 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1518 | `{` |
|      35 | 1519 | `	SXUNUSED(pUserData); /* cc warning */` |
|      74 | 1520 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      74 | 1521 | `}` |
|       - | 1522 | `/*` |
|       - | 1523 | ` * M_SQRT2` |
|       - | 1524 | ` *  Expand 	1.41421356237309504880` |
|       - | 1525 | ` */` |
|      71 | 1526 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1527 | `{` |
|      35 | 1528 | `	SXUNUSED(pUserData); /* cc warning */` |
|      74 | 1529 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      74 | 1530 | `}` |
|       - | 1531 | `/*` |
|       - | 1532 | ` * M_SQRT3` |
|       - | 1533 | ` *  Expand 	1.73205080756887729352` |
|       - | 1534 | ` */` |
|      71 | 1535 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1536 | `{` |
|      35 | 1537 | `	SXUNUSED(pUserData); /* cc warning */` |
|      74 | 1538 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      74 | 1539 | `}` |
|       - | 1540 | `/*` |
|       - | 1541 | ` * M_SQRT1_2` |
|       - | 1542 | ` *  Expand 	0.70710678118654752440` |
|       - | 1543 | ` */` |
|      71 | 1544 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1545 | `{` |
|      35 | 1546 | `	SXUNUSED(pUserData); /* cc warning */` |
|      74 | 1547 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      74 | 1548 | `}` |
|       - | 1549 | `/*` |
|       - | 1550 | ` * M_LNPI` |
|       - | 1551 | ` *  Expand 	1.14472988584940017414` |
|       - | 1552 | ` */` |
|      71 | 1553 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1554 | `{` |
|      35 | 1555 | `	SXUNUSED(pUserData); /* cc warning */` |
|      74 | 1556 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      74 | 1557 | `}` |
|       - | 1558 | `/*` |
|       - | 1559 | ` * M_EULER` |
|       - | 1560 | ` *  Expand  0.57721566490153286061` |
|       - | 1561 | ` */` |
|      71 | 1562 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1563 | `{` |
|      35 | 1564 | `	SXUNUSED(pUserData); /* cc warning */` |
|      74 | 1565 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      74 | 1566 | `}` |
|       - | 1567 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|       - | 1568 | `/*` |
|       - | 1569 | ` * DATE_ATOM` |
|       - | 1570 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|       - | 1571 | ` */` |
|     140 | 1572 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1573 | `{` |
|      70 | 1574 | `	SXUNUSED(pUserData); /* cc warning */` |
|     143 | 1575 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|     143 | 1576 | `}` |
|       - | 1577 | `/*` |
|       - | 1578 | ` * DATE_COOKIE` |
|       - | 1579 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|       - | 1580 | ` */` |
|      70 | 1581 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1582 | `{` |
|      35 | 1583 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1584 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      73 | 1585 | `}` |
|       - | 1586 | `/*` |
|       - | 1587 | ` * DATE_ISO8601` |
|       - | 1588 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|       - | 1589 | ` */` |
|      70 | 1590 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1591 | `{` |
|      35 | 1592 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1593 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      73 | 1594 | `}` |
|       - | 1595 | `/*` |
|       - | 1596 | ` * DATE_RFC822` |
|       - | 1597 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|       - | 1598 | ` */` |
|      70 | 1599 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1600 | `{` |
|      35 | 1601 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1602 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      73 | 1603 | `}` |
|       - | 1604 | `/*` |
|       - | 1605 | ` * DATE_RFC850` |
|       - | 1606 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|       - | 1607 | ` */` |
|      70 | 1608 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1609 | `{` |
|      35 | 1610 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1611 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      73 | 1612 | `}` |
|       - | 1613 | `/*` |
|       - | 1614 | ` * DATE_RFC1036` |
|       - | 1615 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1616 | ` */` |
|      70 | 1617 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1618 | `{` |
|      35 | 1619 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1620 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      73 | 1621 | `}` |
|       - | 1622 | `/*` |
|       - | 1623 | ` * DATE_RFC1123` |
|       - | 1624 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1625 | ` */` |
|      70 | 1626 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1627 | `{` |
|      35 | 1628 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1629 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      73 | 1630 | `}` |
|       - | 1631 | `/*` |
|       - | 1632 | ` * DATE_RFC2822` |
|       - | 1633 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1634 | ` */` |
|      70 | 1635 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1636 | `{` |
|      35 | 1637 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1638 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      73 | 1639 | `}` |
|       - | 1640 | `/*` |
|       - | 1641 | ` * DATE_RSS` |
|       - | 1642 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1643 | ` */` |
|      70 | 1644 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1645 | `{` |
|      35 | 1646 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1647 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      73 | 1648 | `}` |
|       - | 1649 | `/*` |
|       - | 1650 | ` * DATE_W3C` |
|       - | 1651 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|       - | 1652 | ` */` |
|      70 | 1653 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1654 | `{` |
|      35 | 1655 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1656 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      73 | 1657 | `}` |
|       - | 1658 | `/*` |
|       - | 1659 | ` * The three format constants php added after the original set. Each is a plain` |
|       - | 1660 | ` * format STRING, so the whole of its behaviour is what date()/DateTime::format()` |
|       - | 1661 | ` * already do with those characters -- but each was a loud undefined-constant` |
|       - | 1662 | ` * fatal, which is a program that does not run rather than one that runs wrong.` |
|       - | 1663 | ` *` |
|       - | 1664 | ` * DATE_RFC7231 is the HTTP date (always GMT, so the zone letters are ESCAPED` |
|       - | 1665 | ` * rather than formatted -- php's own definition, and the reason it is not` |
|       - | 1666 | ` * DATE_RFC1123 with a T on the end). DATE_RFC3339_EXTENDED carries` |
|       - | 1667 | `` * milliseconds. DATE_ISO8601_EXPANDED uses `X`, the expanded-year field, where`` |
|       - | 1668 | `` * the plain DATE_ISO8601 uses `Y`.`` |
|       - | 1669 | ` */` |
|      76 | 1670 | `static void PH7_DATE_RFC7231_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1671 | `{` |
|      38 | 1672 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 1673 | `	ph7_value_string(pVal,"D, d M Y H:i:s \\G\\M\\T",-1/*Compute length automatically*/);` |
|      79 | 1674 | `}` |
|      68 | 1675 | `static void PH7_DATE_RFC3339_EXTENDED_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1676 | `{` |
|      34 | 1677 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1678 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:s.vP",-1/*Compute length automatically*/);` |
|      71 | 1679 | `}` |
|      68 | 1680 | `static void PH7_DATE_ISO8601_EXPANDED_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1681 | `{` |
|      34 | 1682 | `	SXUNUSED(pUserData); /* cc warning */` |
|      71 | 1683 | `	ph7_value_string(pVal,"X-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      71 | 1684 | `}` |
|       - | 1685 | `/*` |
|       - | 1686 | ` * FILE_TEXT / FILE_BINARY` |
|       - | 1687 | ` *  Both expand 0. php declares them for file()/file_put_contents()'s $flags and` |
|       - | 1688 | ` *  ignores them (the CLI has no text mode to select), but a program that names` |
|       - | 1689 | ` *  one still has to COMPILE, and an undefined constant is a fatal.` |
|       - | 1690 | ` */` |
|     154 | 1691 | `static void PH7_FILE_TEXT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1692 | `{` |
|     157 | 1693 | `	ph7_value_int(pVal,0);` |
|      76 | 1694 | `	SXUNUSED(pUserData);` |
|     157 | 1695 | `}` |
|       - | 1696 | `/*` |
|       - | 1697 | ` * The ENT_* values are PHP-exact (php 8.5.7). The low two bits are the quote` |
|       - | 1698 | ` * bits (1 = single, 2 = double), so ENT_QUOTES = ENT_COMPAT\|1 and` |
|       - | 1699 | ` * ENT_NOQUOTES = 0. Bits 16\|32 select the doctype (0 = HTML401, 16 = XML1,` |
|       - | 1700 | ` * 32 = XHTML, 48 = HTML5) — composites, not flags.` |
|       - | 1701 | ` */` |
|       - | 1702 | `/*` |
|       - | 1703 | ` * ENT_COMPAT` |
|       - | 1704 | ` *  Expand 2 (double-quote bit only)` |
|       - | 1705 | ` */` |
|      83 | 1706 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1707 | `{` |
|      41 | 1708 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1709 | `	ph7_value_int(pVal,PH7_ENT_QUOTE_DOUBLE);` |
|      87 | 1710 | `}` |
|       - | 1711 | `/*` |
|       - | 1712 | ` * ENT_QUOTES` |
|       - | 1713 | ` *  Expand 3 (double\|single quote bits)` |
|       - | 1714 | ` */` |
|     241 | 1715 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1716 | `{` |
|     120 | 1717 | `	SXUNUSED(pUserData); /* cc warning */` |
|     245 | 1718 | `	ph7_value_int(pVal,PH7_ENT_QUOTES);` |
|     245 | 1719 | `}` |
|       - | 1720 | `/*` |
|       - | 1721 | ` * ENT_NOQUOTES` |
|       - | 1722 | ` *  Expand 0 (no quote bits)` |
|       - | 1723 | ` */` |
|      91 | 1724 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1725 | `{` |
|      45 | 1726 | `	SXUNUSED(pUserData); /* cc warning */` |
|      95 | 1727 | `	ph7_value_int(pVal,0);` |
|      95 | 1728 | `}` |
|       - | 1729 | `/*` |
|       - | 1730 | ` * ENT_IGNORE` |
|       - | 1731 | ` *  Expand 4` |
|       - | 1732 | ` */` |
|      75 | 1733 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1734 | `{` |
|      37 | 1735 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 1736 | `	ph7_value_int(pVal,PH7_ENT_IGNORE);` |
|      78 | 1737 | `}` |
|       - | 1738 | `/*` |
|       - | 1739 | ` * ENT_SUBSTITUTE` |
|       - | 1740 | ` *  Expand 8` |
|       - | 1741 | ` */` |
|      93 | 1742 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1743 | `{` |
|      46 | 1744 | `	SXUNUSED(pUserData); /* cc warning */` |
|      96 | 1745 | `	ph7_value_int(pVal,PH7_ENT_SUBSTITUTE);` |
|      96 | 1746 | `}` |
|       - | 1747 | `/*` |
|       - | 1748 | ` * ENT_DISALLOWED` |
|       - | 1749 | ` *  Expand 128` |
|       - | 1750 | ` */` |
|     123 | 1751 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1752 | `{` |
|      61 | 1753 | `	SXUNUSED(pUserData); /* cc warning */` |
|     127 | 1754 | `	ph7_value_int(pVal,PH7_ENT_DISALLOWED);` |
|     127 | 1755 | `}` |
|       - | 1756 | `/*` |
|       - | 1757 | ` * ENT_HTML401` |
|       - | 1758 | ` *  Expand 0 (the default doctype)` |
|       - | 1759 | ` */` |
|      93 | 1760 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1761 | `{` |
|      46 | 1762 | `	SXUNUSED(pUserData); /* cc warning */` |
|      96 | 1763 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML401);` |
|      96 | 1764 | `}` |
|       - | 1765 | `/*` |
|       - | 1766 | ` * ENT_XML1` |
|       - | 1767 | ` *  Expand 16` |
|       - | 1768 | ` */` |
|      79 | 1769 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1770 | `{` |
|      39 | 1771 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 1772 | `	ph7_value_int(pVal,PH7_ENT_DOC_XML1);` |
|      83 | 1773 | `}` |
|       - | 1774 | `/*` |
|       - | 1775 | ` * ENT_XHTML` |
|       - | 1776 | ` *  Expand 32` |
|       - | 1777 | ` */` |
|      75 | 1778 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1779 | `{` |
|      37 | 1780 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 1781 | `	ph7_value_int(pVal,PH7_ENT_DOC_XHTML);` |
|      78 | 1782 | `}` |
|       - | 1783 | `/*` |
|       - | 1784 | ` * ENT_HTML5` |
|       - | 1785 | ` *  Expand 48 (16\|32 — a doctype composite, not a flag bit)` |
|       - | 1786 | ` */` |
|      77 | 1787 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1788 | `{` |
|      38 | 1789 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 1790 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML5);` |
|      80 | 1791 | `}` |
|       - | 1792 | `/*` |
|       - | 1793 | ` * ISO-8859-1` |
|       - | 1794 | ` * ISO_8859_1` |
|       - | 1795 | ` *   Expand 1` |
|       - | 1796 | ` */` |
|     142 | 1797 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1798 | `{` |
|      71 | 1799 | `	SXUNUSED(pUserData); /* cc warning */` |
|     145 | 1800 | `	ph7_value_int(pVal,1);` |
|     145 | 1801 | `}` |
|       - | 1802 | `/*` |
|       - | 1803 | ` * UTF-8` |
|       - | 1804 | ` * UTF8` |
|       - | 1805 | ` *  Expand 2` |
|       - | 1806 | ` */` |
|     142 | 1807 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1808 | `{` |
|      71 | 1809 | `	SXUNUSED(pUserData); /* cc warning */` |
|     145 | 1810 | `	ph7_value_int(pVal,1);` |
|     145 | 1811 | `}` |
|       - | 1812 | `/*` |
|       - | 1813 | ` * HTML_ENTITIES` |
|       - | 1814 | ` *  Expand 1` |
|       - | 1815 | ` */` |
|      97 | 1816 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1817 | `{` |
|      48 | 1818 | `	SXUNUSED(pUserData); /* cc warning */` |
|     101 | 1819 | `	ph7_value_int(pVal,1);` |
|     101 | 1820 | `}` |
|       - | 1821 | `/*` |
|       - | 1822 | ` * HTML_SPECIALCHARS` |
|       - | 1823 | ` *  Expand 0 (PHP-exact)` |
|       - | 1824 | ` */` |
|      85 | 1825 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1826 | `{` |
|      42 | 1827 | `	SXUNUSED(pUserData); /* cc warning */` |
|      89 | 1828 | `	ph7_value_int(pVal,0);` |
|      89 | 1829 | `}` |
|       - | 1830 | `/*` |
|       - | 1831 | ` * PHP_URL_SCHEME.` |
|       - | 1832 | ` * Expand 0` |
|       - | 1833 | ` */` |
|      73 | 1834 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1835 | `{` |
|      36 | 1836 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1837 | `	ph7_value_int(pVal,0);` |
|      76 | 1838 | `}` |
|       - | 1839 | `/*` |
|       - | 1840 | ` * PHP_URL_HOST.` |
|       - | 1841 | ` * Expand 1` |
|       - | 1842 | ` */` |
|      75 | 1843 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1844 | `{` |
|      37 | 1845 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 1846 | `	ph7_value_int(pVal,1);` |
|      78 | 1847 | `}` |
|       - | 1848 | `/*` |
|       - | 1849 | ` * PHP_URL_PORT.` |
|       - | 1850 | ` * Expand 2` |
|       - | 1851 | ` */` |
|      75 | 1852 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1853 | `{` |
|      37 | 1854 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 1855 | `	ph7_value_int(pVal,2);` |
|      78 | 1856 | `}` |
|       - | 1857 | `/*` |
|       - | 1858 | ` * PHP_URL_USER.` |
|       - | 1859 | ` * Expand 3` |
|       - | 1860 | ` */` |
|      73 | 1861 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1862 | `{` |
|      36 | 1863 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1864 | `	ph7_value_int(pVal,3);` |
|      76 | 1865 | `}` |
|       - | 1866 | `/*` |
|       - | 1867 | ` * PHP_URL_PASS.` |
|       - | 1868 | ` * Expand 4` |
|       - | 1869 | ` */` |
|      73 | 1870 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1871 | `{` |
|      36 | 1872 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1873 | `	ph7_value_int(pVal,4);` |
|      76 | 1874 | `}` |
|       - | 1875 | `/*` |
|       - | 1876 | ` * PHP_URL_PATH.` |
|       - | 1877 | ` * Expand 5` |
|       - | 1878 | ` */` |
|      73 | 1879 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1880 | `{` |
|      36 | 1881 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1882 | `	ph7_value_int(pVal,5);` |
|      76 | 1883 | `}` |
|       - | 1884 | `/*` |
|       - | 1885 | ` * PHP_URL_QUERY.` |
|       - | 1886 | ` * Expand 6` |
|       - | 1887 | ` */` |
|      75 | 1888 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1889 | `{` |
|      37 | 1890 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 1891 | `	ph7_value_int(pVal,6);` |
|      78 | 1892 | `}` |
|       - | 1893 | `/*` |
|       - | 1894 | ` * PHP_URL_FRAGMENT.` |
|       - | 1895 | ` * Expand 7` |
|       - | 1896 | ` */` |
|      75 | 1897 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1898 | `{` |
|      37 | 1899 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 1900 | `	ph7_value_int(pVal,7);` |
|      78 | 1901 | `}` |
|       - | 1902 | `/*` |
|       - | 1903 | ` * PHP_QUERY_RFC1738` |
|       - | 1904 | ` * Expand 1` |
|       - | 1905 | ` */` |
|      71 | 1906 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1907 | `{` |
|      35 | 1908 | `	SXUNUSED(pUserData); /* cc warning */` |
|      74 | 1909 | `	ph7_value_int(pVal,1);` |
|      74 | 1910 | `}` |
|       - | 1911 | `/*` |
|       - | 1912 | ` * PHP_QUERY_RFC3986` |
|       - | 1913 | ` * Expand 1` |
|       - | 1914 | ` */` |
|      73 | 1915 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1916 | `{` |
|      36 | 1917 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1918 | `	ph7_value_int(pVal,2);` |
|      76 | 1919 | `}` |
|       - | 1920 | `/* php's FNM_* values (ext/standard): PATHNAME=1, NOESCAPE=2, PERIOD=4, CASEFOLD=16.` |
|       - | 1921 | ` * PHL previously had PATHNAME/NOESCAPE swapped and CASEFOLD=8; fnmatch() reads these` |
|       - | 1922 | ` * bits, so PH7_builtin_fnmatch was updated to the same values. */` |
|       - | 1923 | `/*` |
|       - | 1924 | ` * FNM_PATHNAME` |
|       - | 1925 | ` *  Expand 1 (php value)` |
|       - | 1926 | ` */` |
|      69 | 1927 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1928 | `{` |
|      34 | 1929 | `	SXUNUSED(pUserData); /* cc warning */` |
|      72 | 1930 | `	ph7_value_int(pVal,1);` |
|      72 | 1931 | `}` |
|       - | 1932 | `/*` |
|       - | 1933 | ` * FNM_NOESCAPE` |
|       - | 1934 | ` *  Expand 2 (php value)` |
|       - | 1935 | ` */` |
|     119 | 1936 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1937 | `{` |
|      59 | 1938 | `	SXUNUSED(pUserData); /* cc warning */` |
|     122 | 1939 | `	ph7_value_int(pVal,2);` |
|     122 | 1940 | `}` |
|       - | 1941 | `/*` |
|       - | 1942 | ` * FNM_PERIOD` |
|       - | 1943 | ` *  Expand 4 (php value)` |
|       - | 1944 | ` */` |
|      75 | 1945 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1946 | `{` |
|      37 | 1947 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 1948 | `	ph7_value_int(pVal,4);` |
|      78 | 1949 | `}` |
|       - | 1950 | `/*` |
|       - | 1951 | ` * FNM_CASEFOLD` |
|       - | 1952 | ` *  Expand 16 (php value)` |
|       - | 1953 | ` */` |
|     115 | 1954 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1955 | `{` |
|      57 | 1956 | `	SXUNUSED(pUserData); /* cc warning */` |
|     118 | 1957 | `	ph7_value_int(pVal,16);` |
|     118 | 1958 | `}` |
|       - | 1959 | `/*` |
|       - | 1960 | ` * PATHINFO_DIRNAME` |
|       - | 1961 | ` *  Expand 1.` |
|       - | 1962 | ` */` |
|      91 | 1963 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1964 | `{` |
|      45 | 1965 | `	SXUNUSED(pUserData); /* cc warning */` |
|      94 | 1966 | `	ph7_value_int(pVal,PH7_PATHINFO_DIRNAME);` |
|      94 | 1967 | `}` |
|       - | 1968 | `/*` |
|       - | 1969 | ` * PATHINFO_BASENAME` |
|       - | 1970 | ` *  Expand 2.` |
|       - | 1971 | ` */` |
|      91 | 1972 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1973 | `{` |
|      45 | 1974 | `	SXUNUSED(pUserData); /* cc warning */` |
|      94 | 1975 | `	ph7_value_int(pVal,PH7_PATHINFO_BASENAME);` |
|      94 | 1976 | `}` |
|       - | 1977 | `/*` |
|       - | 1978 | ` * PATHINFO_EXTENSION` |
|       - | 1979 | ` *  Expand php's 4 (a POWER OF TWO: the components are a bitmask).` |
|       - | 1980 | ` */` |
|    9595 | 1981 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1982 | `{` |
|    4797 | 1983 | `	SXUNUSED(pUserData); /* cc warning */` |
|    9600 | 1984 | `	ph7_value_int(pVal,PH7_PATHINFO_EXTENSION);` |
|    9600 | 1985 | `}` |
|       - | 1986 | `/*` |
|       - | 1987 | ` * PATHINFO_FILENAME` |
|       - | 1988 | ` *  Expand php's 8 (a POWER OF TWO: the components are a bitmask).` |
|       - | 1989 | ` */` |
|    9563 | 1990 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1991 | `{` |
|    4781 | 1992 | `	SXUNUSED(pUserData); /* cc warning */` |
|    9568 | 1993 | `	ph7_value_int(pVal,PH7_PATHINFO_FILENAME);` |
|    9568 | 1994 | `}` |
|       - | 1995 | `/*` |
|       - | 1996 | ` * PATHINFO_ALL` |
|       - | 1997 | ` *  Expand php's 15 — the default, and the one value that answers with the ARRAY.` |
|       - | 1998 | ` */` |
|      79 | 1999 | `static void PH7_PATHINFO_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2000 | `{` |
|      39 | 2001 | `	SXUNUSED(pUserData); /* cc warning */` |
|      82 | 2002 | `	ph7_value_int(pVal,PH7_PATHINFO_ALL);` |
|      82 | 2003 | `}` |
|       - | 2004 | `#ifdef PH7_ENABLE_PCRE` |
|       - | 2005 | `/*` |
|       - | 2006 | ` * php's four PCRE build constants, asked of the linked library (see` |
|       - | 2007 | ` * PH7_PcreVersionInfo). Composer reads PCRE_VERSION before it loads a repository.` |
|       - | 2008 | ` */` |
|      72 | 2009 | `static void PH7_PCRE_VERSION_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2010 | `{` |
|       - | 2011 | `	char zVer[64];` |
|      36 | 2012 | `	SXUNUSED(pUserData);` |
|      75 | 2013 | `	PH7_PcreVersionInfo(zVer,(int)sizeof(zVer),0,0,0);` |
|      75 | 2014 | `	ph7_value_string(pVal,zVer,-1);` |
|      75 | 2015 | `}` |
|      70 | 2016 | `static void PH7_PCRE_VERSION_MAJOR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2017 | `{` |
|       - | 2018 | `	char zVer[64];` |
|      73 | 2019 | `	int iMaj = 0;` |
|      35 | 2020 | `	SXUNUSED(pUserData);` |
|      73 | 2021 | `	PH7_PcreVersionInfo(zVer,(int)sizeof(zVer),&iMaj,0,0);` |
|      73 | 2022 | `	ph7_value_int(pVal,iMaj);` |
|      73 | 2023 | `}` |
|      70 | 2024 | `static void PH7_PCRE_VERSION_MINOR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2025 | `{` |
|       - | 2026 | `	char zVer[64];` |
|      73 | 2027 | `	int iMin = 0;` |
|      35 | 2028 | `	SXUNUSED(pUserData);` |
|      73 | 2029 | `	PH7_PcreVersionInfo(zVer,(int)sizeof(zVer),0,&iMin,0);` |
|      73 | 2030 | `	ph7_value_int(pVal,iMin);` |
|      73 | 2031 | `}` |
|      70 | 2032 | `static void PH7_PCRE_JIT_SUPPORT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2033 | `{` |
|       - | 2034 | `	char zVer[64];` |
|      73 | 2035 | `	int iJit = 0;` |
|      35 | 2036 | `	SXUNUSED(pUserData);` |
|      73 | 2037 | `	PH7_PcreVersionInfo(zVer,(int)sizeof(zVer),0,0,&iJit);` |
|      73 | 2038 | `	ph7_value_bool(pVal,iJit);` |
|      73 | 2039 | `}` |
|       - | 2040 | `#endif /* PH7_ENABLE_PCRE */` |
|       - | 2041 | `/*` |
|       - | 2042 | ` * php's four BUILD-SHAPE booleans. A script reads them to decide what the engine` |
|       - | 2043 | ``  * can do, not what it is called: symfony/process asks `defined('ZEND_THREAD_SAFE')` `` |
|       - | 2044 | `` * to know whether `proc_open` needs an explicit cwd, and with the constant simply`` |
|       - | 2045 | `` * ABSENT it passed null and every subprocess Composer runs died in `is_dir(null)`.`` |
|       - | 2046 | ` * PHL runs one VM per thread with no shared globals, so it answers php's` |
|       - | 2047 | ` * non-ZTS, non-debug shape.` |
|       - | 2048 | ` */` |
|      72 | 2049 | `static void PH7_ZEND_THREAD_SAFE_Const(ph7_value *pVal,void *pUserData)` |
|      75 | 2050 | `{ SXUNUSED(pUserData); ph7_value_bool(pVal,0); }` |
|      72 | 2051 | `static void PH7_ZEND_DEBUG_BUILD_Const(ph7_value *pVal,void *pUserData)` |
|      75 | 2052 | `{ SXUNUSED(pUserData); ph7_value_bool(pVal,0); }` |
|      72 | 2053 | `static void PH7_PHP_ZTS_Const(ph7_value *pVal,void *pUserData)` |
|      75 | 2054 | `{ SXUNUSED(pUserData); ph7_value_bool(pVal,0); }` |
|      72 | 2055 | `static void PH7_PHP_DEBUG_Const(ph7_value *pVal,void *pUserData)` |
|      75 | 2056 | `{ SXUNUSED(pUserData); ph7_value_bool(pVal,0); }` |
|       - | 2057 | `/*` |
|       - | 2058 | ` * php's phpinfo() SECTION flags. A script passes one to say which part it wants;` |
|       - | 2059 | ` * symfony/process asks for INFO_GENERAL to read the build's configure line (it is` |
|       - | 2060 | `` * how it detects `--enable-sigchild`), so Composer needs them to start at all.`` |
|       - | 2061 | ` */` |
|      70 | 2062 | `static void PH7_INFO_GENERAL_Const(ph7_value *pVal,void *pUserData)` |
|      73 | 2063 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,1); }` |
|      70 | 2064 | `static void PH7_INFO_CREDITS_Const(ph7_value *pVal,void *pUserData)` |
|      73 | 2065 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,2); }` |
|      70 | 2066 | `static void PH7_INFO_CONFIGURATION_Const(ph7_value *pVal,void *pUserData)` |
|      73 | 2067 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,4); }` |
|      70 | 2068 | `static void PH7_INFO_MODULES_Const(ph7_value *pVal,void *pUserData)` |
|      73 | 2069 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,8); }` |
|      70 | 2070 | `static void PH7_INFO_ENVIRONMENT_Const(ph7_value *pVal,void *pUserData)` |
|      73 | 2071 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,16); }` |
|      70 | 2072 | `static void PH7_INFO_VARIABLES_Const(ph7_value *pVal,void *pUserData)` |
|      73 | 2073 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,32); }` |
|      70 | 2074 | `static void PH7_INFO_LICENSE_Const(ph7_value *pVal,void *pUserData)` |
|      73 | 2075 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,64); }` |
|      70 | 2076 | `static void PH7_INFO_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      73 | 2077 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,4294967295LL); }` |
|       - | 2078 | `/*` |
|       - | 2079 | ` * The LC_* CATEGORY numbers. php registers the C library's own macros, so its` |
|       - | 2080 | ` * numbers are the platform's (macOS and Windows put LC_ALL at 0); these are` |
|       - | 2081 | ` * glibc's on every platform -- a script that uses the names cannot tell, one` |
|       - | 2082 | ` * that prints the numbers can -- and setlocale() maps them to the platform's` |
|       - | 2083 | ` * macros.` |
|       - | 2084 | ` */` |
|      73 | 2085 | `static void PH7_LC_CTYPE_Const(ph7_value *pVal,void *pUserData)` |
|      76 | 2086 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,0); }` |
|      72 | 2087 | `static void PH7_LC_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|      75 | 2088 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,1); }` |
|      70 | 2089 | `static void PH7_LC_TIME_Const(ph7_value *pVal,void *pUserData)` |
|      73 | 2090 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,2); }` |
|      70 | 2091 | `static void PH7_LC_COLLATE_Const(ph7_value *pVal,void *pUserData)` |
|      73 | 2092 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,3); }` |
|      71 | 2093 | `static void PH7_LC_MONETARY_Const(ph7_value *pVal,void *pUserData)` |
|      75 | 2094 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,4); }` |
|      79 | 2095 | `static void PH7_LC_MESSAGES_Const(ph7_value *pVal,void *pUserData)` |
|      83 | 2096 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,5); }` |
|      84 | 2097 | `static void PH7_LC_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      87 | 2098 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,6); }` |
|       - | 2099 | `/*` |
|       - | 2100 | ` * ext/posix's constants. Every one of them is the PLATFORM's macro rather than` |
|       - | 2101 | `` * a number copied out of one build: `RLIMIT_AS` is 9 on Linux and something`` |
|       - | 2102 | ` * else elsewhere, and a script that stores one and hands it back to` |
|       - | 2103 | ` * posix_getrlimit() has to get its own system's answer. php builds no` |
|       - | 2104 | ` * ext/posix on Windows, so none of these is defined there either.` |
|       - | 2105 | ` */` |
|       - | 2106 | `#ifndef __WINNT__` |
|       - | 2107 | `#ifdef F_OK` |
|      68 | 2108 | `static void PH7_POSIX_F_OK_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2109 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)F_OK); }` |
|       - | 2110 | `#endif` |
|       - | 2111 | `#ifdef X_OK` |
|      68 | 2112 | `static void PH7_POSIX_X_OK_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2113 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)X_OK); }` |
|       - | 2114 | `#endif` |
|       - | 2115 | `#ifdef W_OK` |
|      68 | 2116 | `static void PH7_POSIX_W_OK_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2117 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)W_OK); }` |
|       - | 2118 | `#endif` |
|       - | 2119 | `#ifdef R_OK` |
|      68 | 2120 | `static void PH7_POSIX_R_OK_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2121 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)R_OK); }` |
|       - | 2122 | `#endif` |
|       - | 2123 | `#ifdef S_IFREG` |
|      68 | 2124 | `static void PH7_POSIX_S_IFREG_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2125 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFREG); }` |
|       - | 2126 | `#endif` |
|       - | 2127 | `#ifdef S_IFCHR` |
|      69 | 2128 | `static void PH7_POSIX_S_IFCHR_Const(ph7_value *pVal,void *pUserData)` |
|      69 | 2129 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFCHR); }` |
|       - | 2130 | `#endif` |
|       - | 2131 | `#ifdef S_IFBLK` |
|      69 | 2132 | `static void PH7_POSIX_S_IFBLK_Const(ph7_value *pVal,void *pUserData)` |
|      69 | 2133 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFBLK); }` |
|       - | 2134 | `#endif` |
|       - | 2135 | `#ifdef S_IFIFO` |
|      70 | 2136 | `static void PH7_POSIX_S_IFIFO_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2137 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFIFO); }` |
|       - | 2138 | `#endif` |
|       - | 2139 | `#ifdef S_IFSOCK` |
|      69 | 2140 | `static void PH7_POSIX_S_IFSOCK_Const(ph7_value *pVal,void *pUserData)` |
|      69 | 2141 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFSOCK); }` |
|       - | 2142 | `#endif` |
|       - | 2143 | `#ifdef RLIMIT_AS` |
|      68 | 2144 | `static void PH7_POSIX_RLIMIT_AS_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2145 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_AS); }` |
|       - | 2146 | `#endif` |
|       - | 2147 | `#ifdef RLIMIT_CORE` |
|      69 | 2148 | `static void PH7_POSIX_RLIMIT_CORE_Const(ph7_value *pVal,void *pUserData)` |
|      69 | 2149 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_CORE); }` |
|       - | 2150 | `#endif` |
|       - | 2151 | `#ifdef RLIMIT_CPU` |
|      68 | 2152 | `static void PH7_POSIX_RLIMIT_CPU_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2153 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_CPU); }` |
|       - | 2154 | `#endif` |
|       - | 2155 | `#ifdef RLIMIT_DATA` |
|      68 | 2156 | `static void PH7_POSIX_RLIMIT_DATA_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2157 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_DATA); }` |
|       - | 2158 | `#endif` |
|       - | 2159 | `#ifdef RLIMIT_FSIZE` |
|      68 | 2160 | `static void PH7_POSIX_RLIMIT_FSIZE_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2161 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_FSIZE); }` |
|       - | 2162 | `#endif` |
|       - | 2163 | `#ifdef RLIMIT_LOCKS` |
|      34 | 2164 | `static void PH7_POSIX_RLIMIT_LOCKS_Const(ph7_value *pVal,void *pUserData)` |
|      34 | 2165 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_LOCKS); }` |
|       - | 2166 | `#endif` |
|       - | 2167 | `#ifdef RLIMIT_MEMLOCK` |
|      68 | 2168 | `static void PH7_POSIX_RLIMIT_MEMLOCK_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2169 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_MEMLOCK); }` |
|       - | 2170 | `#endif` |
|       - | 2171 | `#ifdef RLIMIT_MSGQUEUE` |
|      34 | 2172 | `static void PH7_POSIX_RLIMIT_MSGQUEUE_Const(ph7_value *pVal,void *pUserData)` |
|      34 | 2173 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_MSGQUEUE); }` |
|       - | 2174 | `#endif` |
|       - | 2175 | `#ifdef RLIMIT_NICE` |
|      34 | 2176 | `static void PH7_POSIX_RLIMIT_NICE_Const(ph7_value *pVal,void *pUserData)` |
|      34 | 2177 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_NICE); }` |
|       - | 2178 | `#endif` |
|       - | 2179 | `#ifdef RLIMIT_NOFILE` |
|      68 | 2180 | `static void PH7_POSIX_RLIMIT_NOFILE_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2181 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_NOFILE); }` |
|       - | 2182 | `#endif` |
|       - | 2183 | `#ifdef RLIMIT_NPROC` |
|      68 | 2184 | `static void PH7_POSIX_RLIMIT_NPROC_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2185 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_NPROC); }` |
|       - | 2186 | `#endif` |
|       - | 2187 | `#ifdef RLIMIT_RSS` |
|      68 | 2188 | `static void PH7_POSIX_RLIMIT_RSS_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2189 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_RSS); }` |
|       - | 2190 | `#endif` |
|       - | 2191 | `#ifdef RLIMIT_RTPRIO` |
|      34 | 2192 | `static void PH7_POSIX_RLIMIT_RTPRIO_Const(ph7_value *pVal,void *pUserData)` |
|      34 | 2193 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_RTPRIO); }` |
|       - | 2194 | `#endif` |
|       - | 2195 | `#ifdef RLIMIT_RTTIME` |
|      34 | 2196 | `static void PH7_POSIX_RLIMIT_RTTIME_Const(ph7_value *pVal,void *pUserData)` |
|      34 | 2197 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_RTTIME); }` |
|       - | 2198 | `#endif` |
|       - | 2199 | `#ifdef RLIMIT_SIGPENDING` |
|      34 | 2200 | `static void PH7_POSIX_RLIMIT_SIGPENDING_Const(ph7_value *pVal,void *pUserData)` |
|      34 | 2201 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_SIGPENDING); }` |
|       - | 2202 | `#endif` |
|       - | 2203 | `#ifdef RLIMIT_STACK` |
|      68 | 2204 | `static void PH7_POSIX_RLIMIT_STACK_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2205 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_STACK); }` |
|       - | 2206 | `#endif` |
|       - | 2207 | `#ifdef _SC_ARG_MAX` |
|      68 | 2208 | `static void PH7_POSIX_SC_ARG_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2209 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_ARG_MAX); }` |
|       - | 2210 | `#endif` |
|       - | 2211 | `#ifdef _SC_CHILD_MAX` |
|      68 | 2212 | `static void PH7_POSIX_SC_CHILD_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2213 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_CHILD_MAX); }` |
|       - | 2214 | `#endif` |
|       - | 2215 | `#ifdef _SC_CLK_TCK` |
|      69 | 2216 | `static void PH7_POSIX_SC_CLK_TCK_Const(ph7_value *pVal,void *pUserData)` |
|      69 | 2217 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_CLK_TCK); }` |
|       - | 2218 | `#endif` |
|       - | 2219 | `#ifdef _SC_OPEN_MAX` |
|      68 | 2220 | `static void PH7_POSIX_SC_OPEN_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2221 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_OPEN_MAX); }` |
|       - | 2222 | `#endif` |
|       - | 2223 | `#ifdef _SC_PAGESIZE` |
|      69 | 2224 | `static void PH7_POSIX_SC_PAGESIZE_Const(ph7_value *pVal,void *pUserData)` |
|      69 | 2225 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_PAGESIZE); }` |
|       - | 2226 | `#endif` |
|       - | 2227 | `#ifdef _SC_NPROCESSORS_CONF` |
|      68 | 2228 | `static void PH7_POSIX_SC_NPROCESSORS_CONF_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2229 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_NPROCESSORS_CONF); }` |
|       - | 2230 | `#endif` |
|       - | 2231 | `#ifdef _SC_NPROCESSORS_ONLN` |
|      68 | 2232 | `static void PH7_POSIX_SC_NPROCESSORS_ONLN_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2233 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_NPROCESSORS_ONLN); }` |
|       - | 2234 | `#endif` |
|       - | 2235 | `#ifdef _PC_LINK_MAX` |
|      68 | 2236 | `static void PH7_POSIX_PC_LINK_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2237 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_LINK_MAX); }` |
|       - | 2238 | `#endif` |
|       - | 2239 | `#ifdef _PC_MAX_CANON` |
|      68 | 2240 | `static void PH7_POSIX_PC_MAX_CANON_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2241 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_MAX_CANON); }` |
|       - | 2242 | `#endif` |
|       - | 2243 | `#ifdef _PC_MAX_INPUT` |
|      68 | 2244 | `static void PH7_POSIX_PC_MAX_INPUT_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2245 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_MAX_INPUT); }` |
|       - | 2246 | `#endif` |
|       - | 2247 | `#ifdef _PC_NAME_MAX` |
|      74 | 2248 | `static void PH7_POSIX_PC_NAME_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      74 | 2249 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_NAME_MAX); }` |
|       - | 2250 | `#endif` |
|       - | 2251 | `#ifdef _PC_PATH_MAX` |
|      70 | 2252 | `static void PH7_POSIX_PC_PATH_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2253 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_PATH_MAX); }` |
|       - | 2254 | `#endif` |
|       - | 2255 | `#ifdef _PC_PIPE_BUF` |
|      68 | 2256 | `static void PH7_POSIX_PC_PIPE_BUF_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2257 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_PIPE_BUF); }` |
|       - | 2258 | `#endif` |
|       - | 2259 | `#ifdef _PC_CHOWN_RESTRICTED` |
|      68 | 2260 | `static void PH7_POSIX_PC_CHOWN_RESTRICTED_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2261 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_CHOWN_RESTRICTED); }` |
|       - | 2262 | `#endif` |
|       - | 2263 | `#ifdef _PC_NO_TRUNC` |
|      68 | 2264 | `static void PH7_POSIX_PC_NO_TRUNC_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2265 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_NO_TRUNC); }` |
|       - | 2266 | `#endif` |
|       - | 2267 | `#ifdef _PC_ALLOC_SIZE_MIN` |
|      68 | 2268 | `static void PH7_POSIX_PC_ALLOC_SIZE_MIN_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2269 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_ALLOC_SIZE_MIN); }` |
|       - | 2270 | `#endif` |
|       - | 2271 | `#ifdef _PC_SYMLINK_MAX` |
|      68 | 2272 | `static void PH7_POSIX_PC_SYMLINK_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2273 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_SYMLINK_MAX); }` |
|       - | 2274 | `#endif` |
|       - | 2275 | `/*` |
|       - | 2276 | ` * RLIM_INFINITY is a rlim_t, which is unsigned; php answers it as -1, which is` |
|       - | 2277 | ` * what posix_setrlimit() takes back for "no limit".` |
|       - | 2278 | ` */` |
|      68 | 2279 | `static void PH7_POSIX_RLIMIT_INFINITY_Const(ph7_value *pVal,void *pUserData)` |
|      68 | 2280 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,-1); }` |
|       - | 2281 | `#endif /* __WINNT__ */` |
|       - | 2282 | `/*` |
|       - | 2283 | ` * SEEK_SET.` |
|       - | 2284 | ` *  Expand 0` |
|       - | 2285 | ` */` |
|      95 | 2286 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2287 | `{` |
|      47 | 2288 | `	SXUNUSED(pUserData); /* cc warning */` |
|      99 | 2289 | `	ph7_value_int(pVal,0);` |
|      99 | 2290 | `}` |
|       - | 2291 | `/*` |
|       - | 2292 | ` * SEEK_CUR.` |
|       - | 2293 | ` *  Expand 1` |
|       - | 2294 | ` */` |
|      85 | 2295 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2296 | `{` |
|      42 | 2297 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 2298 | `	ph7_value_int(pVal,1);` |
|      88 | 2299 | `}` |
|       - | 2300 | `/*` |
|       - | 2301 | ` * SEEK_END.` |
|       - | 2302 | ` *  Expand 2` |
|       - | 2303 | ` */` |
|      87 | 2304 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2305 | `{` |
|      43 | 2306 | `	SXUNUSED(pUserData); /* cc warning */` |
|      90 | 2307 | `	ph7_value_int(pVal,2);` |
|      90 | 2308 | `}` |
|       - | 2309 | `/*` |
|       - | 2310 | ` * LOCK_SH.` |
|       - | 2311 | ` *  Expand 2` |
|       - | 2312 | ` */` |
|      85 | 2313 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2314 | `{` |
|      42 | 2315 | `	SXUNUSED(pUserData); /* cc warning */` |
|      89 | 2316 | `	ph7_value_int(pVal,1);` |
|      89 | 2317 | `}` |
|       - | 2318 | `/*` |
|       - | 2319 | ` * LOCK_NB.` |
|       - | 2320 | ` *  Expand 4 (php)` |
|       - | 2321 | ` */` |
|      89 | 2322 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2323 | `{` |
|      44 | 2324 | `	SXUNUSED(pUserData); /* cc warning */` |
|      92 | 2325 | `	ph7_value_int(pVal,4);` |
|      92 | 2326 | `}` |
|       - | 2327 | `/*` |
|       - | 2328 | ` * LOCK_EX.` |
|       - | 2329 | ` *  Expand 2 (php). PH7 used 1, which collided with LOCK_SH, and LOCK_UN was 0 — so` |
|       - | 2330 | ` *  flock($h, LOCK_UN) asked the stream for a SHARED lock instead of releasing one.` |
|       - | 2331 | ` */` |
|      87 | 2332 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2333 | `{` |
|      43 | 2334 | `	SXUNUSED(pUserData); /* cc warning */` |
|      90 | 2335 | `	ph7_value_int(pVal,2);` |
|      90 | 2336 | `}` |
|       - | 2337 | `/*` |
|       - | 2338 | ` * LOCK_UN.` |
|       - | 2339 | ` *  Expand 3 (php)` |
|       - | 2340 | ` */` |
|      81 | 2341 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2342 | `{` |
|      40 | 2343 | `	SXUNUSED(pUserData); /* cc warning */` |
|      84 | 2344 | `	ph7_value_int(pVal,3);` |
|      84 | 2345 | `}` |
|       - | 2346 | `/*` |
|       - | 2347 | ` * FILE_USE_INCLUDE_PATH` |
|       - | 2348 | ` *  Expand 0x01 (Must be a power of two)` |
|       - | 2349 | ` */` |
|      73 | 2350 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2351 | `{` |
|      36 | 2352 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 2353 | `	ph7_value_int(pVal,0x1);` |
|      76 | 2354 | `}` |
|       - | 2355 | `/*` |
|       - | 2356 | ` * FILE_IGNORE_NEW_LINES` |
|       - | 2357 | ` *  Expand 0x02 (Must be a power of two)` |
|       - | 2358 | ` */` |
|     169 | 2359 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2360 | `{` |
|      84 | 2361 | `	SXUNUSED(pUserData); /* cc warning */` |
|     172 | 2362 | `	ph7_value_int(pVal,0x2);` |
|     172 | 2363 | `}` |
|       - | 2364 | `/*` |
|       - | 2365 | ` * FILE_SKIP_EMPTY_LINES` |
|       - | 2366 | ` *  Expand 0x04 (Must be a power of two)` |
|       - | 2367 | ` */` |
|     161 | 2368 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2369 | `{` |
|      80 | 2370 | `	SXUNUSED(pUserData); /* cc warning */` |
|     164 | 2371 | `	ph7_value_int(pVal,0x4);` |
|     164 | 2372 | `}` |
|       - | 2373 | `/*` |
|       - | 2374 | ` * FILE_APPEND` |
|       - | 2375 | ` *  Expand 0x08 (Must be a power of two)` |
|       - | 2376 | ` */` |
|      75 | 2377 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2378 | `{` |
|      37 | 2379 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 2380 | `	ph7_value_int(pVal,0x08);` |
|      78 | 2381 | `}` |
|       - | 2382 | `/*` |
|       - | 2383 | ` * FILE_NO_DEFAULT_CONTEXT` |
|       - | 2384 | ` *  Expand php's 0x10. file()/file_put_contents() read it: it is what stops the` |
|       - | 2385 | `` *  `$context = null` argument from resolving to stream_context_get_default()'s`` |
|       - | 2386 | ` *  context. What a device then does with an open carrying no context is its own` |
|       - | 2387 | ` *  business — a userland wrapper's $this->context is a resource either way, in` |
|       - | 2388 | ` *  php as here — so the flag is only observable where an option is consumed.` |
|       - | 2389 | ` */` |
|      77 | 2390 | `static void PH7_FILE_NO_DEFAULT_CONTEXT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2391 | `{` |
|      38 | 2392 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 2393 | `	ph7_value_int(pVal,0x10);` |
|      80 | 2394 | `}` |
|       - | 2395 | `/*` |
|       - | 2396 | ` * SCANDIR_SORT_ASCENDING` |
|       - | 2397 | ` *  Expand 0` |
|       - | 2398 | ` */` |
|    2925 | 2399 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2400 | `{` |
|    1462 | 2401 | `	SXUNUSED(pUserData); /* cc warning */` |
|    2930 | 2402 | `	ph7_value_int(pVal,0);` |
|    2930 | 2403 | `}` |
|       - | 2404 | `/*` |
|       - | 2405 | ` * SCANDIR_SORT_DESCENDING` |
|       - | 2406 | ` *  Expand 1` |
|       - | 2407 | ` */` |
|      77 | 2408 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2409 | `{` |
|      38 | 2410 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 2411 | `	ph7_value_int(pVal,1);` |
|      80 | 2412 | `}` |
|       - | 2413 | `/*` |
|       - | 2414 | ` * SCANDIR_SORT_NONE` |
|       - | 2415 | ` *  Expand 2` |
|       - | 2416 | ` */` |
|    1513 | 2417 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2418 | `{` |
|     756 | 2419 | `	SXUNUSED(pUserData); /* cc warning */` |
|    1518 | 2420 | `	ph7_value_int(pVal,2);` |
|    1518 | 2421 | `}` |
|       - | 2422 | `/*` |
|       - | 2423 | ` * GLOB_MARK` |
|       - | 2424 | ` *  Expand php's 0x08 (php's own portable glob flag set)` |
|       - | 2425 | ` */` |
|     357 | 2426 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2427 | `{` |
|     178 | 2428 | `	SXUNUSED(pUserData); /* cc warning */` |
|     361 | 2429 | `	ph7_value_int(pVal,PH7_GLOB_MARK);` |
|     361 | 2430 | `}` |
|       - | 2431 | `/*` |
|       - | 2432 | ` * GLOB_NOSORT` |
|       - | 2433 | ` *  Expand php's 0x20` |
|       - | 2434 | ` */` |
|     502 | 2435 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2436 | `{` |
|     250 | 2437 | `	SXUNUSED(pUserData); /* cc warning */` |
|     507 | 2438 | `	ph7_value_int(pVal,PH7_GLOB_NOSORT);` |
|     507 | 2439 | `}` |
|       - | 2440 | `/*` |
|       - | 2441 | ` * GLOB_NOCHECK` |
|       - | 2442 | ` *  Expand php's 0x10` |
|       - | 2443 | ` */` |
|     652 | 2444 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2445 | `{` |
|     325 | 2446 | `	SXUNUSED(pUserData); /* cc warning */` |
|     657 | 2447 | `	ph7_value_int(pVal,PH7_GLOB_NOCHECK);` |
|     657 | 2448 | `}` |
|       - | 2449 | `/*` |
|       - | 2450 | ` * GLOB_NOESCAPE` |
|       - | 2451 | ` *  Expand php's 0x1000` |
|       - | 2452 | ` */` |
|      75 | 2453 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2454 | `{` |
|      37 | 2455 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 2456 | `	ph7_value_int(pVal,PH7_GLOB_NOESCAPE);` |
|      78 | 2457 | `}` |
|       - | 2458 | `/*` |
|       - | 2459 | ` * GLOB_BRACE` |
|       - | 2460 | ` *  Expand php's 0x80` |
|       - | 2461 | ` */` |
|     530 | 2462 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2463 | `{` |
|     264 | 2464 | `	SXUNUSED(pUserData); /* cc warning */` |
|     535 | 2465 | `	ph7_value_int(pVal,PH7_GLOB_BRACE);` |
|     535 | 2466 | `}` |
|       - | 2467 | `/*` |
|       - | 2468 | ` * GLOB_ONLYDIR` |
|       - | 2469 | ` *  Expand php's 0x40000000` |
|       - | 2470 | ` */` |
|     827 | 2471 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2472 | `{` |
|     411 | 2473 | `	SXUNUSED(pUserData); /* cc warning */` |
|     832 | 2474 | `	ph7_value_int(pVal,PH7_GLOB_ONLYDIR);` |
|     832 | 2475 | `}` |
|       - | 2476 | `/*` |
|       - | 2477 | ` * GLOB_ERR` |
|       - | 2478 | ` *  Expand php's 0x04` |
|       - | 2479 | ` */` |
|      77 | 2480 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2481 | `{` |
|      38 | 2482 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 2483 | `	ph7_value_int(pVal,PH7_GLOB_ERR);` |
|      80 | 2484 | `}` |
|       - | 2485 | `/*` |
|       - | 2486 | ` * GLOB_AVAILABLE_FLAGS` |
|       - | 2487 | ` *  Expand the OR of every glob flag php's portable glob accepts — the mask` |
|       - | 2488 | ` *  glob() itself validates against (1073746108 on every platform, since the` |
|       - | 2489 | ` *  GLOB_* values are php 8.5's own portable set).` |
|       - | 2490 | ` */` |
|     528 | 2491 | `static void PH7_GLOB_AVAILABLE_FLAGS_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2492 | `{` |
|     263 | 2493 | `	SXUNUSED(pUserData); /* cc warning */` |
|     533 | 2494 | `	ph7_value_int(pVal,PH7_GLOB_ERR\|PH7_GLOB_MARK\|PH7_GLOB_NOCHECK\|PH7_GLOB_NOSORT` |
|       - | 2495 | `		\|PH7_GLOB_BRACE\|PH7_GLOB_NOESCAPE\|PH7_GLOB_ONLYDIR);` |
|     533 | 2496 | `}` |
|       - | 2497 | `/*` |
|       - | 2498 | ` * STDIN` |
|       - | 2499 | ` *  Expand the STDIN handle as a resource.` |
|       - | 2500 | ` */` |
|     160 | 2501 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2502 | `{` |
|     163 | 2503 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 2504 | `	void *pResource;` |
|     163 | 2505 | `	pResource = PH7_ExportStdin(pVm);` |
|     163 | 2506 | `	ph7_value_resource(pVal,pResource);` |
|     163 | 2507 | `}` |
|       - | 2508 | `/*` |
|       - | 2509 | ` * STDOUT` |
|       - | 2510 | ` *   Expand the STDOUT handle as a resource.` |
|       - | 2511 | ` */` |
|     152 | 2512 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2513 | `{` |
|     155 | 2514 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 2515 | `	void *pResource;` |
|     155 | 2516 | `	pResource = PH7_ExportStdout(pVm);` |
|     155 | 2517 | `	ph7_value_resource(pVal,pResource);` |
|     155 | 2518 | `}` |
|       - | 2519 | `/*` |
|       - | 2520 | ` * STDERR` |
|       - | 2521 | ` *  Expand the STDERR handle as a resource.` |
|       - | 2522 | ` */` |
|     195 | 2523 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2524 | `{` |
|     198 | 2525 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 2526 | `	void *pResource;` |
|     198 | 2527 | `	pResource = PH7_ExportStderr(pVm);` |
|     198 | 2528 | `	ph7_value_resource(pVal,pResource);` |
|     198 | 2529 | `}` |
|       - | 2530 | `/*` |
|       - | 2531 | ` * INI_SCANNER_NORMAL` |
|       - | 2532 | ` *   Expand php's 0` |
|       - | 2533 | ` */` |
|      75 | 2534 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2535 | `{` |
|      37 | 2536 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 2537 | `	ph7_value_int(pVal,PH7_INI_SCANNER_NORMAL);` |
|      78 | 2538 | `}` |
|       - | 2539 | `/*` |
|       - | 2540 | ` * INI_SCANNER_RAW` |
|       - | 2541 | ` *   Expand php's 1` |
|       - | 2542 | ` */` |
|      77 | 2543 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2544 | `{` |
|      38 | 2545 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 2546 | `	ph7_value_int(pVal,PH7_INI_SCANNER_RAW);` |
|      80 | 2547 | `}` |
|       - | 2548 | `/*` |
|       - | 2549 | ` * INI_SCANNER_TYPED` |
|       - | 2550 | ` *   Expand 2 (php's value)` |
|       - | 2551 | ` */` |
|      77 | 2552 | `static void PH7_INI_SCANNER_TYPED_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2553 | `{` |
|      38 | 2554 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 2555 | `	ph7_value_int(pVal,PH7_INI_SCANNER_TYPED);` |
|      80 | 2556 | `}` |
|       - | 2557 | `/*` |
|       - | 2558 | ` * EXTR_OVERWRITE` |
|       - | 2559 | ` *   Expand 0 (php's enum value; see PH7_EXTR_* in ph7int.h)` |
|       - | 2560 | ` */` |
|      83 | 2561 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2562 | `{` |
|      41 | 2563 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 2564 | `	ph7_value_int(pVal,PH7_EXTR_OVERWRITE);` |
|      87 | 2565 | `}` |
|       - | 2566 | `/*` |
|       - | 2567 | ` * EXTR_SKIP` |
|       - | 2568 | ` *   Expand 1` |
|       - | 2569 | ` */` |
|      85 | 2570 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2571 | `{` |
|      42 | 2572 | `	SXUNUSED(pUserData); /* cc warning */` |
|      89 | 2573 | `	ph7_value_int(pVal,PH7_EXTR_SKIP);` |
|      89 | 2574 | `}` |
|       - | 2575 | `/*` |
|       - | 2576 | ` * EXTR_PREFIX_SAME` |
|       - | 2577 | ` *   Expand 2` |
|       - | 2578 | ` */` |
|     107 | 2579 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2580 | `{` |
|      53 | 2581 | `	SXUNUSED(pUserData); /* cc warning */` |
|     111 | 2582 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_SAME);` |
|     111 | 2583 | `}` |
|       - | 2584 | `/*` |
|       - | 2585 | ` * EXTR_PREFIX_ALL` |
|       - | 2586 | ` *   Expand 3` |
|       - | 2587 | ` */` |
|      93 | 2588 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2589 | `{` |
|      46 | 2590 | `	SXUNUSED(pUserData); /* cc warning */` |
|      96 | 2591 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_ALL);` |
|      96 | 2592 | `}` |
|       - | 2593 | `/*` |
|       - | 2594 | ` * EXTR_PREFIX_INVALID` |
|       - | 2595 | ` *   Expand 4` |
|       - | 2596 | ` */` |
|      77 | 2597 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2598 | `{` |
|      38 | 2599 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 2600 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_INVALID);` |
|      80 | 2601 | `}` |
|       - | 2602 | `/*` |
|       - | 2603 | ` * EXTR_IF_EXISTS` |
|       - | 2604 | ` *   Expand 6 (php orders IF_EXISTS after PREFIX_IF_EXISTS)` |
|       - | 2605 | ` */` |
|      85 | 2606 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2607 | `{` |
|      42 | 2608 | `	SXUNUSED(pUserData); /* cc warning */` |
|      89 | 2609 | `	ph7_value_int(pVal,PH7_EXTR_IF_EXISTS);` |
|      89 | 2610 | `}` |
|       - | 2611 | `/*` |
|       - | 2612 | ` * EXTR_REFS` |
|       - | 2613 | ` *   Expand 256 (the bit that rides above the mode: bind by REFERENCE)` |
|       - | 2614 | ` */` |
|      83 | 2615 | `static void PH7_EXTR_REFS_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2616 | `{` |
|      41 | 2617 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 2618 | `	ph7_value_int(pVal,PH7_EXTR_REFS);` |
|      87 | 2619 | `}` |
|       - | 2620 | `/*` |
|       - | 2621 | ` * EXTR_PREFIX_IF_EXISTS` |
|       - | 2622 | ` *   Expand 5` |
|       - | 2623 | ` */` |
|      89 | 2624 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2625 | `{` |
|      44 | 2626 | `	SXUNUSED(pUserData); /* cc warning */` |
|      92 | 2627 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_IF_EXISTS);` |
|      92 | 2628 | `}` |
|       - | 2629 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|       - | 2630 | `/*` |
|       - | 2631 | ` * HASH_HMAC.` |
|       - | 2632 | ` *   php's one hash_init() flag. Declared with the hash extension it belongs` |
|       - | 2633 | ` *   to, so a build without that extension has no constant either.` |
|       - | 2634 | ` */` |
|      80 | 2635 | `static void PH7_HASH_HMAC_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2636 | `{` |
|      40 | 2637 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 2638 | `	ph7_value_int(pVal,PH7_HASH_HMAC);` |
|      83 | 2639 | `}` |
|       - | 2640 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|       - | 2641 | `/*` |
|       - | 2642 | ` * ICONV_* — what the converter IS, and iconv_mime_decode()'s $mode bits.` |
|       - | 2643 | `` * php reports the C library behind its extension here (`glibc`, `libiconv`);`` |
|       - | 2644 | ` * PHL converts with its own code so that a Windows build answers what a POSIX` |
|       - | 2645 | ` * one does, and says so — the constants exist to be READ, and a program that` |
|       - | 2646 | ` * branches on them has to see something true.` |
|       - | 2647 | ` */` |
|      70 | 2648 | `static void PH7_ICONV_IMPL_Const(ph7_value *pVal,void *pUnused)` |
|       4 | 2649 | `{` |
|      35 | 2650 | `	SXUNUSED(pUnused);` |
|      74 | 2651 | `	ph7_value_string(pVal,"PHL",(int)sizeof("PHL")-1);` |
|      74 | 2652 | `}` |
|      70 | 2653 | `static void PH7_ICONV_VERSION_Const(ph7_value *pVal,void *pUnused)` |
|       4 | 2654 | `{` |
|      35 | 2655 | `	SXUNUSED(pUnused);` |
|      74 | 2656 | `	ph7_value_string(pVal,PH7_VERSION,(int)sizeof(PH7_VERSION)-1);` |
|      74 | 2657 | `}` |
|      72 | 2658 | `static void PH7_ICONV_MIME_DECODE_STRICT_Const(ph7_value *pVal,void *pUnused)` |
|       4 | 2659 | `{` |
|      36 | 2660 | `	SXUNUSED(pUnused);` |
|      76 | 2661 | `	ph7_value_int(pVal,1);` |
|      76 | 2662 | `}` |
|      72 | 2663 | `static void PH7_ICONV_MIME_DECODE_CONTINUE_ON_ERROR_Const(ph7_value *pVal,void *pUnused)` |
|       4 | 2664 | `{` |
|      36 | 2665 | `	SXUNUSED(pUnused);` |
|      76 | 2666 | `	ph7_value_int(pVal,2);` |
|      76 | 2667 | `}` |
|       - | 2668 | `/*` |
|       - | 2669 | ` * JSON_HEX_TAG.` |
|       - | 2670 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|       - | 2671 | ` */` |
|      82 | 2672 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2673 | `{` |
|      41 | 2674 | `	SXUNUSED(pUserData); /* cc warning */` |
|      85 | 2675 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      85 | 2676 | `}` |
|       - | 2677 | `/*` |
|       - | 2678 | ` * JSON_HEX_AMP.` |
|       - | 2679 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|       - | 2680 | ` */` |
|      80 | 2681 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2682 | `{` |
|      40 | 2683 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 2684 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      83 | 2685 | `}` |
|       - | 2686 | `/*` |
|       - | 2687 | ` * JSON_HEX_APOS.` |
|       - | 2688 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|       - | 2689 | ` */` |
|      80 | 2690 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2691 | `{` |
|      40 | 2692 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 2693 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      83 | 2694 | `}` |
|       - | 2695 | `/*` |
|       - | 2696 | ` * JSON_HEX_QUOT.` |
|       - | 2697 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|       - | 2698 | ` */` |
|      80 | 2699 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2700 | `{` |
|      40 | 2701 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 2702 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      83 | 2703 | `}` |
|       - | 2704 | `/*` |
|       - | 2705 | ` * JSON_FORCE_OBJECT.` |
|       - | 2706 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|       - | 2707 | ` */` |
|      80 | 2708 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2709 | `{` |
|      40 | 2710 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 2711 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      83 | 2712 | `}` |
|       - | 2713 | `/*` |
|       - | 2714 | ` * JSON_NUMERIC_CHECK.` |
|       - | 2715 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|       - | 2716 | ` */` |
|      84 | 2717 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2718 | `{` |
|      42 | 2719 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 2720 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      87 | 2721 | `}` |
|       - | 2722 | `/*` |
|       - | 2723 | ` * JSON_BIGINT_AS_STRING.` |
|       - | 2724 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|       - | 2725 | ` */` |
|      88 | 2726 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2727 | `{` |
|      44 | 2728 | `	SXUNUSED(pUserData); /* cc warning */` |
|      91 | 2729 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      91 | 2730 | `}` |
|       - | 2731 | `/*` |
|       - | 2732 | ` * JSON_PARTIAL_OUTPUT_ON_ERROR.` |
|       - | 2733 | ` *   Expand the value of JSON_PARTIAL_OUTPUT_ON_ERROR defined in ph7Int.h.` |
|       - | 2734 | ` */` |
|      98 | 2735 | `static void PH7_JSON_PARTIAL_OUTPUT_ON_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2736 | `{` |
|      49 | 2737 | `	SXUNUSED(pUserData); /* cc warning */` |
|     101 | 2738 | `	ph7_value_int(pVal,JSON_PARTIAL_OUTPUT_ON_ERROR);` |
|     101 | 2739 | `}` |
|       - | 2740 | `/*` |
|       - | 2741 | ` * JSON_PRESERVE_ZERO_FRACTION.` |
|       - | 2742 | ` *   Expand the value of JSON_PRESERVE_ZERO_FRACTION defined in ph7Int.h.` |
|       - | 2743 | ` */` |
|      88 | 2744 | `static void PH7_JSON_PRESERVE_ZERO_FRACTION_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2745 | `{` |
|      44 | 2746 | `	SXUNUSED(pUserData); /* cc warning */` |
|      91 | 2747 | `	ph7_value_int(pVal,JSON_PRESERVE_ZERO_FRACTION);` |
|      91 | 2748 | `}` |
|       - | 2749 | `/*` |
|       - | 2750 | ` * JSON_OBJECT_AS_ARRAY.` |
|       - | 2751 | ` *   Expand the value of JSON_OBJECT_AS_ARRAY defined in ph7Int.h.` |
|       - | 2752 | ` */` |
|      84 | 2753 | `static void PH7_JSON_OBJECT_AS_ARRAY_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2754 | `{` |
|      42 | 2755 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 2756 | `	ph7_value_int(pVal,JSON_OBJECT_AS_ARRAY);` |
|      87 | 2757 | `}` |
|       - | 2758 | `/*` |
|       - | 2759 | ` * JSON_PRETTY_PRINT.` |
|       - | 2760 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|       - | 2761 | ` */` |
|      88 | 2762 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2763 | `{` |
|      44 | 2764 | `	SXUNUSED(pUserData); /* cc warning */` |
|      92 | 2765 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      92 | 2766 | `}` |
|       - | 2767 | `/*` |
|       - | 2768 | ` * JSON_UNESCAPED_SLASHES.` |
|       - | 2769 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|       - | 2770 | ` */` |
|      84 | 2771 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2772 | `{` |
|      42 | 2773 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 2774 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      88 | 2775 | `}` |
|       - | 2776 | `/*` |
|       - | 2777 | ` * JSON_UNESCAPED_UNICODE.` |
|       - | 2778 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|       - | 2779 | ` */` |
|     590 | 2780 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2781 | `{` |
|     295 | 2782 | `	SXUNUSED(pUserData); /* cc warning */` |
|     593 | 2783 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|     593 | 2784 | `}` |
|       - | 2785 | `/*` |
|       - | 2786 | ` * JSON_UNESCAPED_LINE_TERMINATORS.` |
|       - | 2787 | ` *   Expand the value of JSON_UNESCAPED_LINE_TERMINATORS defined in ph7Int.h.` |
|       - | 2788 | ` */` |
|      80 | 2789 | `static void PH7_JSON_UNESCAPED_LINE_TERMINATORS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2790 | `{` |
|      40 | 2791 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 2792 | `	ph7_value_int(pVal,JSON_UNESCAPED_LINE_TERMINATORS);` |
|      83 | 2793 | `}` |
|       - | 2794 | `/*` |
|       - | 2795 | ` * JSON_INVALID_UTF8_IGNORE.` |
|       - | 2796 | ` *   Expand the value of JSON_INVALID_UTF8_IGNORE defined in ph7Int.h.` |
|       - | 2797 | ` */` |
|     100 | 2798 | `static void PH7_JSON_INVALID_UTF8_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2799 | `{` |
|      50 | 2800 | `	SXUNUSED(pUserData); /* cc warning */` |
|     104 | 2801 | `	ph7_value_int(pVal,JSON_INVALID_UTF8_IGNORE);` |
|     104 | 2802 | `}` |
|       - | 2803 | `/*` |
|       - | 2804 | ` * JSON_INVALID_UTF8_SUBSTITUTE.` |
|       - | 2805 | ` *   Expand the value of JSON_INVALID_UTF8_SUBSTITUTE defined in ph7Int.h.` |
|       - | 2806 | ` */` |
|     104 | 2807 | `static void PH7_JSON_INVALID_UTF8_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2808 | `{` |
|      52 | 2809 | `	SXUNUSED(pUserData); /* cc warning */` |
|     107 | 2810 | `	ph7_value_int(pVal,JSON_INVALID_UTF8_SUBSTITUTE);` |
|     107 | 2811 | `}` |
|       - | 2812 | `/*` |
|       - | 2813 | ` * JSON_THROW_ON_ERROR.` |
|       - | 2814 | ` *   Expand the value of JSON_THROW_ON_ERROR defined in ph7Int.h.` |
|       - | 2815 | ` */` |
|      94 | 2816 | `static void PH7_JSON_THROW_ON_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2817 | `{` |
|      47 | 2818 | `	SXUNUSED(pUserData); /* cc warning */` |
|      97 | 2819 | `	ph7_value_int(pVal,JSON_THROW_ON_ERROR);` |
|      97 | 2820 | `}` |
|       - | 2821 | `/*` |
|       - | 2822 | ` * JSON_ERROR_NONE.` |
|       - | 2823 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|       - | 2824 | ` */` |
|      78 | 2825 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2826 | `{` |
|      39 | 2827 | `	SXUNUSED(pUserData); /* cc warning */` |
|      81 | 2828 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      81 | 2829 | `}` |
|       - | 2830 | `/*` |
|       - | 2831 | ` * JSON_ERROR_DEPTH.` |
|       - | 2832 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|       - | 2833 | ` */` |
|      76 | 2834 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2835 | `{` |
|      38 | 2836 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 2837 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      79 | 2838 | `}` |
|       - | 2839 | `/*` |
|       - | 2840 | ` * JSON_ERROR_STATE_MISMATCH.` |
|       - | 2841 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|       - | 2842 | ` */` |
|      76 | 2843 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2844 | `{` |
|      38 | 2845 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 2846 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      79 | 2847 | `}` |
|       - | 2848 | `/*` |
|       - | 2849 | ` * JSON_ERROR_CTRL_CHAR.` |
|       - | 2850 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|       - | 2851 | ` */` |
|      76 | 2852 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2853 | `{` |
|      38 | 2854 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 2855 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      79 | 2856 | `}` |
|       - | 2857 | `/*` |
|       - | 2858 | ` * JSON_ERROR_SYNTAX.` |
|       - | 2859 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|       - | 2860 | ` */` |
|      78 | 2861 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2862 | `{` |
|      39 | 2863 | `	SXUNUSED(pUserData); /* cc warning */` |
|      81 | 2864 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      81 | 2865 | `}` |
|       - | 2866 | `/*` |
|       - | 2867 | ` * JSON_ERROR_UTF8.` |
|       - | 2868 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|       - | 2869 | ` */` |
|      76 | 2870 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2871 | `{` |
|      38 | 2872 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 2873 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      79 | 2874 | `}` |
|       - | 2875 | `/*` |
|       - | 2876 | ` * JSON_ERROR_RECURSION.` |
|       - | 2877 | ` *   Expand the value of JSON_ERROR_RECURSION defined in ph7Int.h.` |
|       - | 2878 | ` */` |
|      76 | 2879 | `static void PH7_JSON_ERROR_RECURSION_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2880 | `{` |
|      38 | 2881 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 2882 | `	ph7_value_int(pVal,JSON_ERROR_RECURSION);` |
|      79 | 2883 | `}` |
|       - | 2884 | `/*` |
|       - | 2885 | ` * JSON_ERROR_UNSUPPORTED_TYPE.` |
|       - | 2886 | ` *   Expand the value of JSON_ERROR_UNSUPPORTED_TYPE defined in ph7Int.h.` |
|       - | 2887 | ` */` |
|      76 | 2888 | `static void PH7_JSON_ERROR_UNSUPPORTED_TYPE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2889 | `{` |
|      38 | 2890 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 2891 | `	ph7_value_int(pVal,JSON_ERROR_UNSUPPORTED_TYPE);` |
|      79 | 2892 | `}` |
|       - | 2893 | `/*` |
|       - | 2894 | ` * JSON_ERROR_INVALID_PROPERTY_NAME.` |
|       - | 2895 | ` *   Expand the value of JSON_ERROR_INVALID_PROPERTY_NAME defined in ph7Int.h.` |
|       - | 2896 | ` */` |
|      76 | 2897 | `static void PH7_JSON_ERROR_INVALID_PROPERTY_NAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2898 | `{` |
|      38 | 2899 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 2900 | `	ph7_value_int(pVal,JSON_ERROR_INVALID_PROPERTY_NAME);` |
|      79 | 2901 | `}` |
|       - | 2902 | `/*` |
|       - | 2903 | ` * JSON_ERROR_UTF16.` |
|       - | 2904 | ` *   Expand the value of JSON_ERROR_UTF16 defined in ph7Int.h.` |
|       - | 2905 | ` */` |
|      78 | 2906 | `static void PH7_JSON_ERROR_UTF16_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2907 | `{` |
|      39 | 2908 | `	SXUNUSED(pUserData); /* cc warning */` |
|      81 | 2909 | `	ph7_value_int(pVal,JSON_ERROR_UTF16);` |
|      81 | 2910 | `}` |
|       - | 2911 | `/*` |
|       - | 2912 | ` * JSON_ERROR_NON_BACKED_ENUM.` |
|       - | 2913 | ` *   Expand the value of JSON_ERROR_NON_BACKED_ENUM defined in ph7Int.h (php 8.1).` |
|       - | 2914 | ` */` |
|      74 | 2915 | `static void PH7_JSON_ERROR_NON_BACKED_ENUM_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2916 | `{` |
|      37 | 2917 | `	SXUNUSED(pUserData); /* cc warning */` |
|      77 | 2918 | `	ph7_value_int(pVal,JSON_ERROR_NON_BACKED_ENUM);` |
|      77 | 2919 | `}` |
|       - | 2920 | `/*` |
|       - | 2921 | ` * JSON_ERROR_INF_OR_NAN.` |
|       - | 2922 | ` *   Expand the value of JSON_ERROR_INF_OR_NAN defined in ph7Int.h.` |
|       - | 2923 | ` */` |
|      76 | 2924 | `static void PH7_JSON_ERROR_INF_OR_NAN_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2925 | `{` |
|      38 | 2926 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 2927 | `	ph7_value_int(pVal,JSON_ERROR_INF_OR_NAN);` |
|      79 | 2928 | `}` |
|       - | 2929 | `/*` |
|       - | 2930 | ` * __CLASS__` |
|       - | 2931 | ` *  The current class name, or the EMPTY STRING outside any class — php answers "",` |
|       - | 2932 | `` *  not null (`__CLASS__ === ""` is true in global scope). `self` keeps its own`` |
|       - | 2933 | ` *  expander below because php treats IT differently outside a class scope.` |
|       - | 2934 | ` */` |
|      86 | 2935 | `static void PH7_class_magic_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2936 | `{` |
|      89 | 2937 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 2938 | `	ph7_class *pClass;` |
|       - | 2939 | `	/* php flattens a trait into the class that used it, so __CLASS__ inside a trait method` |
|       - | 2940 | `	 * is THAT class (where __TRAIT__ and __METHOD__ stay the trait's — php's own asymmetry). */` |
|      89 | 2941 | `	pClass = PH7_VmPeekSelfClass(pVm);` |
|      89 | 2942 | `	if( pClass == 0 ){` |
|      75 | 2943 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      36 | 2944 | `	}` |
|      89 | 2945 | `	if( pClass ){` |
|      15 | 2946 | `		SyString *pName = &pClass->sName;` |
|      15 | 2947 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|       8 | 2948 | `	}else{` |
|      75 | 2949 | `		ph7_value_string(pVal,"",0);` |
|       - | 2950 | `	}` |
|      89 | 2951 | `}` |
|       - | 2952 |  |
|       - | 2953 | `/*` |
|       - | 2954 | ` * PASSWORD_BCRYPT / PASSWORD_DEFAULT` |
|       - | 2955 | ` *  The bcrypt algorithm identifier (PHP 7.4+ exposes these as the string "2y").` |
|       - | 2956 | ` *  PASSWORD_DEFAULT tracks the recommended default, currently bcrypt.` |
|       - | 2957 | ` */` |
|     176 | 2958 | `static void PH7_PASSWORD_BCRYPT_Const(ph7_value *pVal,void *pUnused)` |
|       4 | 2959 | `{` |
|      87 | 2960 | `	SXUNUSED(pUnused);` |
|     180 | 2961 | `	ph7_value_string(pVal,"2y",(int)sizeof("2y")-1);` |
|     180 | 2962 | `}` |
|       - | 2963 | `/*` |
|       - | 2964 | ` * PASSWORD_BCRYPT_DEFAULT_COST` |
|       - | 2965 | ` *  The default bcrypt work factor used by password_hash() (currently 12).` |
|       - | 2966 | ` */` |
|      71 | 2967 | `static void PH7_PASSWORD_COST_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2968 | `{` |
|      35 | 2969 | `	SXUNUSED(pUnused);` |
|      74 | 2970 | `	ph7_value_int(pVal,12);` |
|      74 | 2971 | `}` |
|       - | 2972 | `/*` |
|       - | 2973 | ` * PASSWORD_ARGON2I / PASSWORD_ARGON2ID and the three argon2 option defaults` |
|       - | 2974 | ` * password_hash() reads when $options omits them.` |
|       - | 2975 | ` */` |
|      75 | 2976 | `static void PH7_PASSWORD_ARGON2I_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2977 | `{` |
|      37 | 2978 | `	SXUNUSED(pUnused);` |
|      78 | 2979 | `	ph7_value_string(pVal,"argon2i",(int)sizeof("argon2i")-1);` |
|      78 | 2980 | `}` |
|      93 | 2981 | `static void PH7_PASSWORD_ARGON2ID_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2982 | `{` |
|      46 | 2983 | `	SXUNUSED(pUnused);` |
|      96 | 2984 | `	ph7_value_string(pVal,"argon2id",(int)sizeof("argon2id")-1);` |
|      96 | 2985 | `}` |
|      71 | 2986 | `static void PH7_ARGON2_MEM_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2987 | `{` |
|      35 | 2988 | `	SXUNUSED(pUnused);` |
|      74 | 2989 | `	ph7_value_int(pVal,65536);` |
|      74 | 2990 | `}` |
|      71 | 2991 | `static void PH7_ARGON2_TIME_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2992 | `{` |
|      35 | 2993 | `	SXUNUSED(pUnused);` |
|      74 | 2994 | `	ph7_value_int(pVal,4);` |
|      74 | 2995 | `}` |
|      71 | 2996 | `static void PH7_ARGON2_THREADS_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2997 | `{` |
|      35 | 2998 | `	SXUNUSED(pUnused);` |
|      74 | 2999 | `	ph7_value_int(pVal,1);` |
|      74 | 3000 | `}` |
|       - | 3001 | `/*` |
|       - | 3002 | ` * CRYPT_* — the crypt() capability flags. Every scheme is compiled in, so all` |
|       - | 3003 | ` * six are 1, and CRYPT_SALT_LENGTH is php's 123 (the longest setting string a` |
|       - | 3004 | ` * SHA-512-crypt with an explicit rounds count can need).` |
|       - | 3005 | ` */` |
|       - | 3006 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|     426 | 3007 | `static void PH7_CRYPT_ONE_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 3008 | `{` |
|     210 | 3009 | `	SXUNUSED(pUnused);` |
|     429 | 3010 | `	ph7_value_int(pVal,1);` |
|     429 | 3011 | `}` |
|      71 | 3012 | `static void PH7_CRYPT_SALT_LENGTH_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 3013 | `{` |
|      35 | 3014 | `	SXUNUSED(pUnused);` |
|      74 | 3015 | `	ph7_value_int(pVal,123);` |
|      74 | 3016 | `}` |
|       - | 3017 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|       - | 3018 | `/*` |
|       - | 3019 | ` * filter_var() filter and flag identifiers (the ext/filter constants). Values` |
|       - | 3020 | ` * match PHP 8.5. One tiny int-returning callback per constant, generated by a` |
|       - | 3021 | ` * local macro to keep the ~25 near-identical definitions DRY.` |
|       - | 3022 | ` */` |
|       - | 3023 | `#define PH7_FILTER_INT_CONST(Name,Val) \` |
|       - | 3024 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|       - | 3025 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|       - | 3026 | `	}` |
|      89 | 3027 | `PH7_FILTER_INT_CONST(FILTER_DEFAULT,516)` |
|      87 | 3028 | `PH7_FILTER_INT_CONST(FILTER_UNSAFE_RAW,516)` |
|     232 | 3029 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_INT,257)` |
|     174 | 3030 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_BOOLEAN,258)` |
|     206 | 3031 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_FLOAT,259)` |
|      81 | 3032 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_REGEXP,272)` |
|     216 | 3033 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_DOMAIN,277)` |
|     392 | 3034 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_URL,273)` |
|     272 | 3035 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_EMAIL,274)` |
|     635 | 3036 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_IP,275)` |
|     107 | 3037 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_MAC,276)` |
|      98 | 3038 | `PH7_FILTER_INT_CONST(FILTER_CALLBACK,1024)` |
|     100 | 3039 | `PH7_FILTER_INT_CONST(FILTER_THROW_ON_FAILURE,268435456)` |
|      79 | 3040 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_ENCODED,514)` |
|      77 | 3041 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_ADD_SLASHES,523)` |
|      76 | 3042 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_INT,519)` |
|      75 | 3043 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_FLOAT,520)` |
|      85 | 3044 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_SPECIAL_CHARS,515)` |
|      95 | 3045 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_FULL_SPECIAL_CHARS,522)` |
|      73 | 3046 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_EMAIL,517)` |
|      73 | 3047 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_URL,518)` |
|      73 | 3048 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_OCTAL,1)` |
|      73 | 3049 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_HEX,2)` |
|      81 | 3050 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_LOW,4)` |
|      77 | 3051 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_HIGH,8)` |
|      77 | 3052 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_LOW,16)` |
|      75 | 3053 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_HIGH,32)` |
|      75 | 3054 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_AMP,64)` |
|      78 | 3055 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_ENCODE_QUOTES,128)` |
|      73 | 3056 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NONE,0)` |
|      81 | 3057 | `PH7_FILTER_INT_CONST(FILTER_FLAG_EMPTY_STRING_NULL,256)` |
|      75 | 3058 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_BACKTICK,512)` |
|      73 | 3059 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_FRACTION,4096)` |
|     108 | 3060 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_THOUSAND,8192)` |
|      73 | 3061 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_SCIENTIFIC,16384)` |
|      80 | 3062 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV4,1048576)` |
|      76 | 3063 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV6,2097152)` |
|     223 | 3064 | `PH7_FILTER_INT_CONST(FILTER_FLAG_PATH_REQUIRED,262144)` |
|     223 | 3065 | `PH7_FILTER_INT_CONST(FILTER_FLAG_QUERY_REQUIRED,524288)` |
|     136 | 3066 | `PH7_FILTER_INT_CONST(FILTER_FLAG_HOSTNAME,1048576)` |
|     155 | 3067 | `PH7_FILTER_INT_CONST(FILTER_FLAG_EMAIL_UNICODE,1048576)` |
|     284 | 3068 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_RES_RANGE,4194304)` |
|     288 | 3069 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_PRIV_RANGE,8388608)` |
|     180 | 3070 | `PH7_FILTER_INT_CONST(FILTER_FLAG_GLOBAL_RANGE,536870912)` |
|      98 | 3071 | `PH7_FILTER_INT_CONST(FILTER_REQUIRE_ARRAY,16777216)` |
|      77 | 3072 | `PH7_FILTER_INT_CONST(FILTER_REQUIRE_SCALAR,33554432)` |
|      81 | 3073 | `PH7_FILTER_INT_CONST(FILTER_FORCE_ARRAY,67108864)` |
|      94 | 3074 | `PH7_FILTER_INT_CONST(FILTER_NULL_ON_FAILURE,134217728)` |
|       - | 3075 | `/* filter_input() source selectors (php values; SESSION/REQUEST are undefined in 8.5) */` |
|      75 | 3076 | `PH7_FILTER_INT_CONST(INPUT_POST,0)` |
|      85 | 3077 | `PH7_FILTER_INT_CONST(INPUT_GET,1)` |
|      73 | 3078 | `PH7_FILTER_INT_CONST(INPUT_COOKIE,2)` |
|      73 | 3079 | `PH7_FILTER_INT_CONST(INPUT_ENV,4)` |
|      91 | 3080 | `PH7_FILTER_INT_CONST(INPUT_SERVER,5)` |
|       - | 3081 | `/*` |
|       - | 3082 | ` * ext/fileinfo's flags. libmagic's MAGIC_* values, which php re-exports under` |
|       - | 3083 | ` * its own names -- a script stores one and hands it back, so the NUMBERS are` |
|       - | 3084 | ` * the contract (see builtin_fileinfo.c).` |
|       - | 3085 | ` */` |
|       - | 3086 | `#define PH7_FILEINFO_INT_CONST(Name,Val) \` |
|       - | 3087 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|       - | 3088 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|       - | 3089 | `	}` |
|       - | 3090 | `/*` |
|       - | 3091 | ` * ext/zlib's constants. The three encodings are libz's own windowBits spelling` |
|       - | 3092 | ` * (negative = raw, +16 = gzip), which is why they are -15/15/31; the flush and` |
|       - | 3093 | ` * status numbers are libz's too. ZLIB_VERSION and ZLIB_VERNUM report the` |
|       - | 3094 | ` * library this build was compiled against, exactly as php's report theirs --` |
|       - | 3095 | ` * so they are the one pair here that is not the same on every box.` |
|       - | 3096 | ` */` |
|       - | 3097 | `#ifdef PH7_ENABLE_ZLIB` |
|       - | 3098 | `#define PH7_ZLIB_INT_CONST(Name,Val) \` |
|       - | 3099 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|       - | 3100 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|       - | 3101 | `	}` |
|      74 | 3102 | `PH7_ZLIB_INT_CONST(FORCE_GZIP,31)` |
|      74 | 3103 | `PH7_ZLIB_INT_CONST(FORCE_DEFLATE,15)` |
|     108 | 3104 | `PH7_ZLIB_INT_CONST(ZLIB_ENCODING_RAW,-15)` |
|      84 | 3105 | `PH7_ZLIB_INT_CONST(ZLIB_ENCODING_GZIP,31)` |
|      82 | 3106 | `PH7_ZLIB_INT_CONST(ZLIB_ENCODING_DEFLATE,15)` |
|      76 | 3107 | `PH7_ZLIB_INT_CONST(ZLIB_NO_FLUSH,0)` |
|      74 | 3108 | `PH7_ZLIB_INT_CONST(ZLIB_PARTIAL_FLUSH,1)` |
|      78 | 3109 | `PH7_ZLIB_INT_CONST(ZLIB_SYNC_FLUSH,2)` |
|      74 | 3110 | `PH7_ZLIB_INT_CONST(ZLIB_FULL_FLUSH,3)` |
|      74 | 3111 | `PH7_ZLIB_INT_CONST(ZLIB_BLOCK,5)` |
|      86 | 3112 | `PH7_ZLIB_INT_CONST(ZLIB_FINISH,4)` |
|      74 | 3113 | `PH7_ZLIB_INT_CONST(ZLIB_FILTERED,1)` |
|      74 | 3114 | `PH7_ZLIB_INT_CONST(ZLIB_HUFFMAN_ONLY,2)` |
|      74 | 3115 | `PH7_ZLIB_INT_CONST(ZLIB_RLE,3)` |
|      74 | 3116 | `PH7_ZLIB_INT_CONST(ZLIB_FIXED,4)` |
|      74 | 3117 | `PH7_ZLIB_INT_CONST(ZLIB_DEFAULT_STRATEGY,0)` |
|      74 | 3118 | `PH7_ZLIB_INT_CONST(ZLIB_VERNUM,ZLIB_VERNUM)` |
|      74 | 3119 | `PH7_ZLIB_INT_CONST(ZLIB_OK,0)` |
|      74 | 3120 | `PH7_ZLIB_INT_CONST(ZLIB_STREAM_END,1)` |
|      74 | 3121 | `PH7_ZLIB_INT_CONST(ZLIB_NEED_DICT,2)` |
|      74 | 3122 | `PH7_ZLIB_INT_CONST(ZLIB_ERRNO,-1)` |
|      74 | 3123 | `PH7_ZLIB_INT_CONST(ZLIB_STREAM_ERROR,-2)` |
|      74 | 3124 | `PH7_ZLIB_INT_CONST(ZLIB_DATA_ERROR,-3)` |
|      74 | 3125 | `PH7_ZLIB_INT_CONST(ZLIB_MEM_ERROR,-4)` |
|      74 | 3126 | `PH7_ZLIB_INT_CONST(ZLIB_BUF_ERROR,-5)` |
|      74 | 3127 | `PH7_ZLIB_INT_CONST(ZLIB_VERSION_ERROR,-6)` |
|      70 | 3128 | `static void PH7_ZLIB_VERSION_Const(ph7_value *pVal,void *pUnused)` |
|       4 | 3129 | `{` |
|      35 | 3130 | `	SXUNUSED(pUnused);` |
|      74 | 3131 | `	ph7_value_string(pVal,ZLIB_VERSION,-1);` |
|      74 | 3132 | `}` |
|       - | 3133 | `#endif /* PH7_ENABLE_ZLIB */` |
|       - | 3134 | `/*` |
|       - | 3135 | ` * ext/openssl's constants. THREE families with three different origins, and` |
|       - | 3136 | ` * telling them apart is the whole of the work here:` |
|       - | 3137 | ` *` |
|       - | 3138 | ` *  - the LIBRARY's numbers (the X509_PURPOSE_*, the PKCS7_* and CMS_* flag` |
|       - | 3139 | ` *    bits, the RSA padding modes, the version pair). Each is bound to the` |
|       - | 3140 | ` *    OpenSSL SYMBOL, never to the number this box happens to print: a flag` |
|       - | 3141 | ` *    that moved between 3.0 and 3.6 would otherwise be silently wrong on the` |
|       - | 3142 | ` *    Windows build, which links a different one. See the` |
|       - | 3143 | `` *    `bind-by-symbol-not-by-value` note.`` |
|       - | 3144 | ` *  - php's OWN numbering, which OpenSSL has no symbol for: OPENSSL_ALGO_*` |
|       - | 3145 | ` *    (a digest enum of php's, with the gap at 4/5 where php removed two DSS` |
|       - | 3146 | ` *    entries), OPENSSL_CIPHER_* (an enum the PKCS#7 doors take),` |
|       - | 3147 | ` *    OPENSSL_KEYTYPE_*, the three option bits and the three CMS encodings.` |
|       - | 3148 | ` *    These are literals because they ARE literals -- a script stores one and` |
|       - | 3149 | ` *    hands it back, so the numbers are the contract.` |
|       - | 3150 | ` *  - one STRING php composes itself: OPENSSL_DEFAULT_STREAM_CIPHERS, the` |
|       - | 3151 | ` *    cipher list php's own TLS streams start from. It is php's text, not` |
|       - | 3152 | ` *    OpenSSL's default, and it is byte-for-byte what php ships.` |
|       - | 3153 | ` */` |
|       - | 3154 | `#ifdef PH7_ENABLE_OPENSSL` |
|       - | 3155 | `#define PH7_SSL_INT_CONST(Name,Val) \` |
|       - | 3156 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|       - | 3157 | `		SXUNUSED(pUnused); ph7_value_int64(pVal,(sxi64)(Val)); \` |
|       - | 3158 | `	}` |
|      77 | 3159 | `PH7_SSL_INT_CONST(OPENSSL_VERSION_NUMBER,OPENSSL_VERSION_NUMBER)` |
|      73 | 3160 | `PH7_SSL_INT_CONST(X509_PURPOSE_SSL_CLIENT,X509_PURPOSE_SSL_CLIENT)` |
|      78 | 3161 | `PH7_SSL_INT_CONST(X509_PURPOSE_SSL_SERVER,X509_PURPOSE_SSL_SERVER)` |
|      73 | 3162 | `PH7_SSL_INT_CONST(X509_PURPOSE_NS_SSL_SERVER,X509_PURPOSE_NS_SSL_SERVER)` |
|      73 | 3163 | `PH7_SSL_INT_CONST(X509_PURPOSE_SMIME_SIGN,X509_PURPOSE_SMIME_SIGN)` |
|      73 | 3164 | `PH7_SSL_INT_CONST(X509_PURPOSE_SMIME_ENCRYPT,X509_PURPOSE_SMIME_ENCRYPT)` |
|      73 | 3165 | `PH7_SSL_INT_CONST(X509_PURPOSE_CRL_SIGN,X509_PURPOSE_CRL_SIGN)` |
|      73 | 3166 | `PH7_SSL_INT_CONST(X509_PURPOSE_ANY,X509_PURPOSE_ANY)` |
|      73 | 3167 | `PH7_SSL_INT_CONST(X509_PURPOSE_OCSP_HELPER,X509_PURPOSE_OCSP_HELPER)` |
|      73 | 3168 | `PH7_SSL_INT_CONST(X509_PURPOSE_TIMESTAMP_SIGN,X509_PURPOSE_TIMESTAMP_SIGN)` |
|      81 | 3169 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA1,1)` |
|      77 | 3170 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_MD5,2)` |
|      75 | 3171 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_MD4,3)` |
|      75 | 3172 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA224,6)` |
|      79 | 3173 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA256,7)` |
|      75 | 3174 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA384,8)` |
|      75 | 3175 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA512,9)` |
|      75 | 3176 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_RMD160,10)` |
|      75 | 3177 | `PH7_SSL_INT_CONST(PKCS7_DETACHED,PKCS7_DETACHED)` |
|      73 | 3178 | `PH7_SSL_INT_CONST(PKCS7_TEXT,PKCS7_TEXT)` |
|      73 | 3179 | `PH7_SSL_INT_CONST(PKCS7_NOINTERN,PKCS7_NOINTERN)` |
|      76 | 3180 | `PH7_SSL_INT_CONST(PKCS7_NOVERIFY,PKCS7_NOVERIFY)` |
|      73 | 3181 | `PH7_SSL_INT_CONST(PKCS7_NOCHAIN,PKCS7_NOCHAIN)` |
|      73 | 3182 | `PH7_SSL_INT_CONST(PKCS7_NOCERTS,PKCS7_NOCERTS)` |
|      73 | 3183 | `PH7_SSL_INT_CONST(PKCS7_NOATTR,PKCS7_NOATTR)` |
|      73 | 3184 | `PH7_SSL_INT_CONST(PKCS7_BINARY,PKCS7_BINARY)` |
|      73 | 3185 | `PH7_SSL_INT_CONST(PKCS7_NOSIGS,PKCS7_NOSIGS)` |
|      73 | 3186 | `PH7_SSL_INT_CONST(PKCS7_NOOLDMIMETYPE,PKCS7_NOOLDMIMETYPE)` |
|      73 | 3187 | `PH7_SSL_INT_CONST(PKCS7_NOSMIMECAP,PKCS7_NOSMIMECAP)` |
|      73 | 3188 | `PH7_SSL_INT_CONST(PKCS7_CRLFEOL,PKCS7_CRLFEOL)` |
|      73 | 3189 | `PH7_SSL_INT_CONST(PKCS7_NOCRL,PKCS7_NOCRL)` |
|      73 | 3190 | `PH7_SSL_INT_CONST(PKCS7_NO_DUAL_CONTENT,PKCS7_NO_DUAL_CONTENT)` |
|      73 | 3191 | `PH7_SSL_INT_CONST(OPENSSL_CMS_DETACHED,CMS_DETACHED)` |
|      73 | 3192 | `PH7_SSL_INT_CONST(OPENSSL_CMS_TEXT,CMS_TEXT)` |
|      73 | 3193 | `PH7_SSL_INT_CONST(OPENSSL_CMS_NOINTERN,CMS_NOINTERN)` |
|      76 | 3194 | `PH7_SSL_INT_CONST(OPENSSL_CMS_NOVERIFY,CMS_NO_SIGNER_CERT_VERIFY)` |
|      73 | 3195 | `PH7_SSL_INT_CONST(OPENSSL_CMS_NOCERTS,CMS_NOCERTS)` |
|      73 | 3196 | `PH7_SSL_INT_CONST(OPENSSL_CMS_NOATTR,CMS_NOATTR)` |
|      73 | 3197 | `PH7_SSL_INT_CONST(OPENSSL_CMS_BINARY,CMS_BINARY)` |
|      73 | 3198 | `PH7_SSL_INT_CONST(OPENSSL_CMS_NOSIGS,CMS_NOSIGS)` |
|      73 | 3199 | `PH7_SSL_INT_CONST(OPENSSL_CMS_OLDMIMETYPE,CMS_NOOLDMIMETYPE)` |
|      85 | 3200 | `PH7_SSL_INT_CONST(OPENSSL_PKCS1_PADDING,RSA_PKCS1_PADDING)` |
|      77 | 3201 | `PH7_SSL_INT_CONST(OPENSSL_NO_PADDING,RSA_NO_PADDING)` |
|      77 | 3202 | `PH7_SSL_INT_CONST(OPENSSL_PKCS1_OAEP_PADDING,RSA_PKCS1_OAEP_PADDING)` |
|      79 | 3203 | `PH7_SSL_INT_CONST(OPENSSL_PKCS1_PSS_PADDING,RSA_PKCS1_PSS_PADDING)` |
|      73 | 3204 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_RC2_40,0)` |
|      73 | 3205 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_RC2_128,1)` |
|      73 | 3206 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_RC2_64,2)` |
|      73 | 3207 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_DES,3)` |
|      73 | 3208 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_3DES,4)` |
|      77 | 3209 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_AES_128_CBC,5)` |
|      73 | 3210 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_AES_192_CBC,6)` |
|      73 | 3211 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_AES_256_CBC,7)` |
|      75 | 3212 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_RSA,0)` |
|      75 | 3213 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_DSA,1)` |
|      75 | 3214 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_DH,2)` |
|      88 | 3215 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_EC,3)` |
|      77 | 3216 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_X25519,4)` |
|      77 | 3217 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_ED25519,5)` |
|      77 | 3218 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_X448,6)` |
|      77 | 3219 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_ED448,7)` |
|     142 | 3220 | `PH7_SSL_INT_CONST(OPENSSL_RAW_DATA,1)` |
|      82 | 3221 | `PH7_SSL_INT_CONST(OPENSSL_ZERO_PADDING,2)` |
|      78 | 3222 | `PH7_SSL_INT_CONST(OPENSSL_DONT_ZERO_PAD_KEY,4)` |
|      73 | 3223 | `PH7_SSL_INT_CONST(OPENSSL_TLSEXT_SERVER_NAME,1)` |
|      75 | 3224 | `PH7_SSL_INT_CONST(OPENSSL_ENCODING_DER,0)` |
|      83 | 3225 | `PH7_SSL_INT_CONST(OPENSSL_ENCODING_SMIME,1)` |
|      75 | 3226 | `PH7_SSL_INT_CONST(OPENSSL_ENCODING_PEM,2)` |
|      72 | 3227 | `static void PH7_OPENSSL_VERSION_TEXT_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 3228 | `{` |
|      36 | 3229 | `	SXUNUSED(pUnused);` |
|      75 | 3230 | `	ph7_value_string(pVal,OPENSSL_VERSION_TEXT,-1);` |
|      75 | 3231 | `}` |
|      74 | 3232 | `static void PH7_OPENSSL_DEFAULT_STREAM_CIPHERS_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 3233 | `{` |
|      37 | 3234 | `	SXUNUSED(pUnused);` |
|      77 | 3235 | `	ph7_value_string(pVal,` |
|       - | 3236 | `		"ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:"` |
|       - | 3237 | `		"ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:"` |
|       - | 3238 | `		"DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:"` |
|       - | 3239 | `		"ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:"` |
|       - | 3240 | `		"ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:"` |
|       - | 3241 | `		"ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:"` |
|       - | 3242 | `		"DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:"` |
|       - | 3243 | `		"DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:"` |
|       - | 3244 | `		"AES128:AES256:HIGH:!SSLv2:!aNULL:!eNULL:!EXPORT:!DES:!MD5:!RC4:!ADH",-1);` |
|      77 | 3245 | `}` |
|       - | 3246 | `#endif /* PH7_ENABLE_OPENSSL */` |
|      94 | 3247 | `PH7_FILEINFO_INT_CONST(FILEINFO_NONE,0)` |
|      73 | 3248 | `PH7_FILEINFO_INT_CONST(FILEINFO_SYMLINK,2)` |
|      78 | 3249 | `PH7_FILEINFO_INT_CONST(FILEINFO_MIME,1040)` |
|      90 | 3250 | `PH7_FILEINFO_INT_CONST(FILEINFO_MIME_TYPE,16)` |
|      78 | 3251 | `PH7_FILEINFO_INT_CONST(FILEINFO_MIME_ENCODING,1024)` |
|      73 | 3252 | `PH7_FILEINFO_INT_CONST(FILEINFO_DEVICES,8)` |
|      73 | 3253 | `PH7_FILEINFO_INT_CONST(FILEINFO_CONTINUE,32)` |
|      73 | 3254 | `PH7_FILEINFO_INT_CONST(FILEINFO_PRESERVE_ATIME,128)` |
|      73 | 3255 | `PH7_FILEINFO_INT_CONST(FILEINFO_RAW,256)` |
|      73 | 3256 | `PH7_FILEINFO_INT_CONST(FILEINFO_APPLE,2048)` |
|      76 | 3257 | `PH7_FILEINFO_INT_CONST(FILEINFO_EXTENSION,16777216)` |
|       - | 3258 | `/*` |
|       - | 3259 | ` * Table of built-in constants.` |
|       - | 3260 | ` */` |
|       - | 3261 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|       - | 3262 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|       - | 3263 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|       - | 3264 | `	{"__PH7__",              PH7_VER_Const      },` |
|       - | 3265 | `	{"PHP_VERSION",          PH7_PHPVerConst    },` |
|       - | 3266 | `	{"PHP_MAJOR_VERSION",    PH7_PHPMajorConst  },` |
|       - | 3267 | `	{"PHP_MINOR_VERSION",    PH7_PHPMinorConst  },` |
|       - | 3268 | `	{"PHP_RELEASE_VERSION",  PH7_PHPReleaseConst},` |
|       - | 3269 | `	{"PHP_EXTRA_VERSION",    PH7_PHPExtraConst  },` |
|       - | 3270 | `	{"PHP_VERSION_ID",       PH7_PHPVerIdConst  },` |
|       - | 3271 | `	{"PHP_OS",               PH7_OS_Const       },` |
|       - | 3272 | `	{"PHP_OS_FAMILY",        PH7_OS_FAMILY_Const},` |
|       - | 3273 | `	{"PHP_SAPI",             PH7_SAPI_Const     },` |
|       - | 3274 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|       - | 3275 | `	{"PHP_SESSION_DISABLED", PH7_PHP_SESSION_DISABLED_Const },` |
|       - | 3276 | `	{"PHP_SESSION_NONE",     PH7_PHP_SESSION_NONE_Const },` |
|       - | 3277 | `	{"PHP_SESSION_ACTIVE",   PH7_PHP_SESSION_ACTIVE_Const },` |
|       - | 3278 | `	{"INI_USER",             PH7_INI_USER_Const },` |
|       - | 3279 | `	{"INI_PERDIR",           PH7_INI_PERDIR_Const },` |
|       - | 3280 | `	{"INI_SYSTEM",           PH7_INI_SYSTEM_Const },` |
|       - | 3281 | `	{"INI_ALL",              PH7_INI_ALL_Const },` |
|       - | 3282 | `	{"MB_CASE_UPPER",        PH7_MB_CASE_UPPER_Const },` |
|       - | 3283 | `	{"MB_CASE_LOWER",        PH7_MB_CASE_LOWER_Const },` |
|       - | 3284 | `	{"MB_CASE_TITLE",        PH7_MB_CASE_TITLE_Const },` |
|       - | 3285 | `	{"PASSWORD_BCRYPT",      PH7_PASSWORD_BCRYPT_Const },` |
|       - | 3286 | `	{"PASSWORD_DEFAULT",     PH7_PASSWORD_BCRYPT_Const },` |
|       - | 3287 | `	{"PASSWORD_BCRYPT_DEFAULT_COST", PH7_PASSWORD_COST_Const },` |
|       - | 3288 | `	{"PASSWORD_ARGON2I",     PH7_PASSWORD_ARGON2I_Const },` |
|       - | 3289 | `	{"PASSWORD_ARGON2ID",    PH7_PASSWORD_ARGON2ID_Const },` |
|       - | 3290 | `	{"PASSWORD_ARGON2_DEFAULT_MEMORY_COST", PH7_ARGON2_MEM_Const },` |
|       - | 3291 | `	{"PASSWORD_ARGON2_DEFAULT_TIME_COST",   PH7_ARGON2_TIME_Const },` |
|       - | 3292 | `	{"PASSWORD_ARGON2_DEFAULT_THREADS",     PH7_ARGON2_THREADS_Const },` |
|       - | 3293 | `	{"FILTER_DEFAULT",              PH7_FILTER_DEFAULT_Const },` |
|       - | 3294 | `	{"FILTER_UNSAFE_RAW",           PH7_FILTER_UNSAFE_RAW_Const },` |
|       - | 3295 | `	{"FILTER_VALIDATE_INT",         PH7_FILTER_VALIDATE_INT_Const },` |
|       - | 3296 | `	{"FILTER_VALIDATE_BOOLEAN",     PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|       - | 3297 | `	{"FILTER_VALIDATE_BOOL",        PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|       - | 3298 | `	{"FILTER_VALIDATE_FLOAT",       PH7_FILTER_VALIDATE_FLOAT_Const },` |
|       - | 3299 | `	{"FILTER_VALIDATE_REGEXP",      PH7_FILTER_VALIDATE_REGEXP_Const },` |
|       - | 3300 | `	{"FILTER_VALIDATE_DOMAIN",      PH7_FILTER_VALIDATE_DOMAIN_Const },` |
|       - | 3301 | `	{"FILTER_VALIDATE_URL",         PH7_FILTER_VALIDATE_URL_Const },` |
|       - | 3302 | `	{"FILTER_VALIDATE_EMAIL",       PH7_FILTER_VALIDATE_EMAIL_Const },` |
|       - | 3303 | `	{"FILTER_VALIDATE_IP",          PH7_FILTER_VALIDATE_IP_Const },` |
|       - | 3304 | `	{"FILTER_VALIDATE_MAC",         PH7_FILTER_VALIDATE_MAC_Const },` |
|       - | 3305 | `	{"FILTER_CALLBACK",             PH7_FILTER_CALLBACK_Const },` |
|       - | 3306 | `	{"FILTER_THROW_ON_FAILURE",     PH7_FILTER_THROW_ON_FAILURE_Const },` |
|       - | 3307 | `	{"FILTER_SANITIZE_ENCODED",     PH7_FILTER_SANITIZE_ENCODED_Const },` |
|       - | 3308 | `	{"FILTER_SANITIZE_ADD_SLASHES", PH7_FILTER_SANITIZE_ADD_SLASHES_Const },` |
|       - | 3309 | `	{"FILTER_FLAG_NONE",            PH7_FILTER_FLAG_NONE_Const },` |
|       - | 3310 | `	{"FILTER_FLAG_EMPTY_STRING_NULL", PH7_FILTER_FLAG_EMPTY_STRING_NULL_Const },` |
|       - | 3311 | `	{"FILTER_SANITIZE_NUMBER_INT",  PH7_FILTER_SANITIZE_NUMBER_INT_Const },` |
|       - | 3312 | `	{"FILTER_SANITIZE_NUMBER_FLOAT",PH7_FILTER_SANITIZE_NUMBER_FLOAT_Const },` |
|       - | 3313 | `	{"FILTER_SANITIZE_SPECIAL_CHARS",PH7_FILTER_SANITIZE_SPECIAL_CHARS_Const },` |
|       - | 3314 | `	{"FILTER_SANITIZE_FULL_SPECIAL_CHARS",PH7_FILTER_SANITIZE_FULL_SPECIAL_CHARS_Const },` |
|       - | 3315 | `	{"FILTER_SANITIZE_EMAIL",       PH7_FILTER_SANITIZE_EMAIL_Const },` |
|       - | 3316 | `	{"FILTER_SANITIZE_URL",         PH7_FILTER_SANITIZE_URL_Const },` |
|       - | 3317 | `	{"FILTER_FLAG_ALLOW_OCTAL",     PH7_FILTER_FLAG_ALLOW_OCTAL_Const },` |
|       - | 3318 | `	{"FILTER_FLAG_ALLOW_HEX",       PH7_FILTER_FLAG_ALLOW_HEX_Const },` |
|       - | 3319 | `	{"FILTER_FLAG_STRIP_LOW",       PH7_FILTER_FLAG_STRIP_LOW_Const },` |
|       - | 3320 | `	{"FILTER_FLAG_STRIP_HIGH",      PH7_FILTER_FLAG_STRIP_HIGH_Const },` |
|       - | 3321 | `	{"FILTER_FLAG_ENCODE_LOW",      PH7_FILTER_FLAG_ENCODE_LOW_Const },` |
|       - | 3322 | `	{"FILTER_FLAG_ENCODE_HIGH",     PH7_FILTER_FLAG_ENCODE_HIGH_Const },` |
|       - | 3323 | `	{"FILTER_FLAG_ENCODE_AMP",      PH7_FILTER_FLAG_ENCODE_AMP_Const },` |
|       - | 3324 | `	{"FILTER_FLAG_NO_ENCODE_QUOTES",PH7_FILTER_FLAG_NO_ENCODE_QUOTES_Const },` |
|       - | 3325 | `	{"FILTER_FLAG_STRIP_BACKTICK",  PH7_FILTER_FLAG_STRIP_BACKTICK_Const },` |
|       - | 3326 | `	{"FILTER_FLAG_ALLOW_FRACTION",  PH7_FILTER_FLAG_ALLOW_FRACTION_Const },` |
|       - | 3327 | `	{"FILTER_FLAG_ALLOW_THOUSAND",  PH7_FILTER_FLAG_ALLOW_THOUSAND_Const },` |
|       - | 3328 | `	{"FILTER_FLAG_ALLOW_SCIENTIFIC",PH7_FILTER_FLAG_ALLOW_SCIENTIFIC_Const },` |
|       - | 3329 | `	{"FILTER_FLAG_IPV4",            PH7_FILTER_FLAG_IPV4_Const },` |
|       - | 3330 | `	{"FILTER_FLAG_IPV6",            PH7_FILTER_FLAG_IPV6_Const },` |
|       - | 3331 | `	{"FILTER_FLAG_PATH_REQUIRED",   PH7_FILTER_FLAG_PATH_REQUIRED_Const },` |
|       - | 3332 | `	{"FILTER_FLAG_QUERY_REQUIRED",  PH7_FILTER_FLAG_QUERY_REQUIRED_Const },` |
|       - | 3333 | `	{"FILTER_FLAG_HOSTNAME",        PH7_FILTER_FLAG_HOSTNAME_Const },` |
|       - | 3334 | `	{"FILTER_FLAG_EMAIL_UNICODE",   PH7_FILTER_FLAG_EMAIL_UNICODE_Const },` |
|       - | 3335 | `	{"FILTER_FLAG_NO_RES_RANGE",    PH7_FILTER_FLAG_NO_RES_RANGE_Const },` |
|       - | 3336 | `	{"FILTER_FLAG_NO_PRIV_RANGE",   PH7_FILTER_FLAG_NO_PRIV_RANGE_Const },` |
|       - | 3337 | `	{"FILTER_FLAG_GLOBAL_RANGE",    PH7_FILTER_FLAG_GLOBAL_RANGE_Const },` |
|       - | 3338 | `	{"FILTER_REQUIRE_ARRAY",        PH7_FILTER_REQUIRE_ARRAY_Const },` |
|       - | 3339 | `	{"FILTER_REQUIRE_SCALAR",       PH7_FILTER_REQUIRE_SCALAR_Const },` |
|       - | 3340 | `	{"FILTER_FORCE_ARRAY",          PH7_FILTER_FORCE_ARRAY_Const },` |
|       - | 3341 | `	{"FILTER_NULL_ON_FAILURE",      PH7_FILTER_NULL_ON_FAILURE_Const },` |
|       - | 3342 | `	{"INPUT_POST",                  PH7_INPUT_POST_Const },` |
|       - | 3343 | `	{"INPUT_GET",                   PH7_INPUT_GET_Const },` |
|       - | 3344 | `	{"INPUT_COOKIE",                PH7_INPUT_COOKIE_Const },` |
|       - | 3345 | `	{"INPUT_ENV",                   PH7_INPUT_ENV_Const },` |
|       - | 3346 | `	{"INPUT_SERVER",                PH7_INPUT_SERVER_Const },` |
|       - | 3347 | `	{"CAL_GREGORIAN",        PH7_CAL_GREGORIAN_Const },` |
|       - | 3348 | `	{"CAL_JULIAN",           PH7_CAL_JULIAN_Const    },` |
|       - | 3349 | `	{"CAL_JEWISH",           PH7_CAL_JEWISH_Const    },` |
|       - | 3350 | `	{"CAL_FRENCH",           PH7_CAL_FRENCH_Const    },` |
|       - | 3351 | `	{"CAL_NUM_CALS",         PH7_CAL_NUM_CALS_Const  },` |
|       - | 3352 | `	{"CAL_DOW_DAYNO",        PH7_CAL_DOW_DAYNO_Const },` |
|       - | 3353 | `	{"CAL_DOW_LONG",         PH7_CAL_DOW_LONG_Const  },` |
|       - | 3354 | `	{"CAL_DOW_SHORT",        PH7_CAL_DOW_SHORT_Const },` |
|       - | 3355 | `	{"CAL_MONTH_GREGORIAN_SHORT", PH7_CAL_MONTH_GREGORIAN_SHORT_Const },` |
|       - | 3356 | `	{"CAL_MONTH_GREGORIAN_LONG",  PH7_CAL_MONTH_GREGORIAN_LONG_Const },` |
|       - | 3357 | `	{"CAL_MONTH_JULIAN_SHORT",    PH7_CAL_MONTH_JULIAN_SHORT_Const },` |
|       - | 3358 | `	{"CAL_MONTH_JULIAN_LONG",     PH7_CAL_MONTH_JULIAN_LONG_Const },` |
|       - | 3359 | `	{"CAL_MONTH_JEWISH",          PH7_CAL_MONTH_JEWISH_Const },` |
|       - | 3360 | `	{"CAL_MONTH_FRENCH",          PH7_CAL_MONTH_FRENCH_Const },` |
|       - | 3361 | `	{"CAL_EASTER_DEFAULT",   PH7_CAL_EASTER_DEFAULT_Const },` |
|       - | 3362 | `	{"CAL_EASTER_ROMAN",     PH7_CAL_EASTER_ROMAN_Const   },` |
|       - | 3363 | `	{"CAL_EASTER_ALWAYS_GREGORIAN", PH7_CAL_EASTER_ALWAYS_GREGORIAN_Const },` |
|       - | 3364 | `	{"CAL_EASTER_ALWAYS_JULIAN",    PH7_CAL_EASTER_ALWAYS_JULIAN_Const },` |
|       - | 3365 | `	{"CAL_JEWISH_ADD_ALAFIM_GERESH", PH7_CAL_JEWISH_ADD_ALAFIM_GERESH_Const },` |
|       - | 3366 | `	{"CAL_JEWISH_ADD_ALAFIM",        PH7_CAL_JEWISH_ADD_ALAFIM_Const },` |
|       - | 3367 | `	{"CAL_JEWISH_ADD_GERESHAYIM",    PH7_CAL_JEWISH_ADD_GERESHAYIM_Const },` |
|       - | 3368 | `	{"IMAGETYPE_GIF",        PH7_IMAGETYPE_GIF_Const     },` |
|       - | 3369 | `	{"IMAGETYPE_JPEG",       PH7_IMAGETYPE_JPEG_Const    },` |
|       - | 3370 | `	{"IMAGETYPE_PNG",        PH7_IMAGETYPE_PNG_Const     },` |
|       - | 3371 | `	{"IMAGETYPE_SWF",        PH7_IMAGETYPE_SWF_Const     },` |
|       - | 3372 | `	{"IMAGETYPE_PSD",        PH7_IMAGETYPE_PSD_Const     },` |
|       - | 3373 | `	{"IMAGETYPE_BMP",        PH7_IMAGETYPE_BMP_Const     },` |
|       - | 3374 | `	{"IMAGETYPE_TIFF_II",    PH7_IMAGETYPE_TIFF_II_Const },` |
|       - | 3375 | `	{"IMAGETYPE_TIFF_MM",    PH7_IMAGETYPE_TIFF_MM_Const },` |
|       - | 3376 | `	{"IMAGETYPE_JPC",        PH7_IMAGETYPE_JPC_Const     },` |
|       - | 3377 | `	{"IMAGETYPE_JP2",        PH7_IMAGETYPE_JP2_Const     },` |
|       - | 3378 | `	{"IMAGETYPE_JPX",        PH7_IMAGETYPE_JPX_Const     },` |
|       - | 3379 | `	{"IMAGETYPE_JB2",        PH7_IMAGETYPE_JB2_Const     },` |
|       - | 3380 | `	{"IMAGETYPE_SWC",        PH7_IMAGETYPE_SWC_Const     },` |
|       - | 3381 | `	{"IMAGETYPE_IFF",        PH7_IMAGETYPE_IFF_Const     },` |
|       - | 3382 | `	{"IMAGETYPE_WBMP",       PH7_IMAGETYPE_WBMP_Const    },` |
|       - | 3383 | `	/* php's own alias row: the same 9 the JPC name expands to. */` |
|       - | 3384 | `	{"IMAGETYPE_JPEG2000",   PH7_IMAGETYPE_JPC_Const     },` |
|       - | 3385 | `	{"IMAGETYPE_XBM",        PH7_IMAGETYPE_XBM_Const     },` |
|       - | 3386 | `	{"IMAGETYPE_ICO",        PH7_IMAGETYPE_ICO_Const     },` |
|       - | 3387 | `	{"IMAGETYPE_WEBP",       PH7_IMAGETYPE_WEBP_Const    },` |
|       - | 3388 | `	{"IMAGETYPE_AVIF",       PH7_IMAGETYPE_AVIF_Const    },` |
|       - | 3389 | `	{"IMAGETYPE_HEIF",       PH7_IMAGETYPE_HEIF_Const    },` |
|       - | 3390 | `	{"IMAGETYPE_UNKNOWN",    PH7_IMAGETYPE_UNKNOWN_Const },` |
|       - | 3391 | `	{"IMAGETYPE_COUNT",      PH7_IMAGETYPE_COUNT_Const   },` |
|       - | 3392 | `#ifdef PH7_ENABLE_LIBXML` |
|       - | 3393 | `	{"IMAGETYPE_SVG",        PH7_IMAGETYPE_SVG_Const     },` |
|       - | 3394 | `#endif` |
|       - | 3395 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|       - | 3396 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|       - | 3397 | `	{"PHP_INT_MIN",          PH7_INTMIN_Const   },` |
|       - | 3398 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|       - | 3399 | `	{"PHP_FLOAT_EPSILON",    PH7_FLOATEPSILON_Const },` |
|       - | 3400 | `	{"PHP_FLOAT_MAX",        PH7_FLOATMAX_Const },` |
|       - | 3401 | `	{"PHP_FLOAT_MIN",        PH7_FLOATMIN_Const },` |
|       - | 3402 | `	{"PHP_FLOAT_DIG",        PH7_FLOATDIG_Const },` |
|       - | 3403 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|       - | 3404 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|       - | 3405 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|       - | 3406 | `	{"__TIME__",             PH7_TIME_Const     },` |
|       - | 3407 | `	{"__DATE__",             PH7_DATE_Const     },` |
|       - | 3408 | `	{"__FILE__",             PH7_FILE_Const     },` |
|       - | 3409 | `	{"__DIR__",              PH7_DIR_Const      },` |
|       - | 3410 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|       - | 3411 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|       - | 3412 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|       - | 3413 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|       - | 3414 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|       - | 3415 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|       - | 3416 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|       - | 3417 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|       - | 3418 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|       - | 3419 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|       - | 3420 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|       - | 3421 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|       - | 3422 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|       - | 3423 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|       - | 3424 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|       - | 3425 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|       - | 3426 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|       - | 3427 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|       - | 3428 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|       - | 3429 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|       - | 3430 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|       - | 3431 | `	{"STREAM_IS_URL",                PH7_STREAM_IS_URL_Const },` |
|       - | 3432 | `	{"STREAM_USE_PATH",              PH7_STREAM_USE_PATH_Const },` |
|       - | 3433 | `	{"STREAM_IGNORE_URL",            PH7_STREAM_IGNORE_URL_Const },` |
|       - | 3434 | `	{"STREAM_REPORT_ERRORS",         PH7_STREAM_REPORT_ERRORS_Const },` |
|       - | 3435 | `	{"STREAM_MUST_SEEK",             PH7_STREAM_MUST_SEEK_Const },` |
|       - | 3436 | `	{"STREAM_URL_STAT_LINK",         PH7_STREAM_URL_STAT_LINK_Const },` |
|       - | 3437 | `	{"STREAM_URL_STAT_QUIET",        PH7_STREAM_URL_STAT_QUIET_Const },` |
|       - | 3438 | `	{"STREAM_MKDIR_RECURSIVE",       PH7_STREAM_MKDIR_RECURSIVE_Const },` |
|       - | 3439 | `	{"STREAM_META_TOUCH",            PH7_STREAM_META_TOUCH_Const },` |
|       - | 3440 | `	{"STREAM_META_OWNER_NAME",       PH7_STREAM_META_OWNER_NAME_Const },` |
|       - | 3441 | `	{"STREAM_META_OWNER",            PH7_STREAM_META_OWNER_Const },` |
|       - | 3442 | `	{"STREAM_META_GROUP_NAME",       PH7_STREAM_META_GROUP_NAME_Const },` |
|       - | 3443 | `	{"STREAM_META_GROUP",            PH7_STREAM_META_GROUP_Const },` |
|       - | 3444 | `	{"STREAM_META_ACCESS",           PH7_STREAM_META_ACCESS_Const },` |
|       - | 3445 | `	{"STREAM_OPTION_BLOCKING",       PH7_STREAM_OPTION_BLOCKING_Const },` |
|       - | 3446 | `	{"STREAM_OPTION_READ_BUFFER",    PH7_STREAM_OPTION_READ_BUFFER_Const },` |
|       - | 3447 | `	{"STREAM_OPTION_WRITE_BUFFER",   PH7_STREAM_OPTION_WRITE_BUFFER_Const },` |
|       - | 3448 | `	{"STREAM_OPTION_READ_TIMEOUT",   PH7_STREAM_OPTION_READ_TIMEOUT_Const },` |
|       - | 3449 | `	{"STREAM_BUFFER_NONE",           PH7_STREAM_BUFFER_NONE_Const },` |
|       - | 3450 | `	{"STREAM_BUFFER_LINE",           PH7_STREAM_BUFFER_LINE_Const },` |
|       - | 3451 | `	{"STREAM_BUFFER_FULL",           PH7_STREAM_BUFFER_FULL_Const },` |
|       - | 3452 | `	{"STREAM_CAST_AS_STREAM",        PH7_STREAM_CAST_AS_STREAM_Const },` |
|       - | 3453 | `	{"STREAM_CAST_FOR_SELECT",       PH7_STREAM_CAST_FOR_SELECT_Const },` |
|       - | 3454 | `	{"STREAM_SERVER_BIND",           PH7_STREAM_SERVER_BIND_Const },` |
|       - | 3455 | `	{"STREAM_SERVER_LISTEN",         PH7_STREAM_SERVER_LISTEN_Const },` |
|       - | 3456 | `	{"STREAM_CLIENT_CONNECT",        PH7_STREAM_CLIENT_CONNECT_Const },` |
|       - | 3457 | `	{"STREAM_CLIENT_ASYNC_CONNECT",  PH7_STREAM_CLIENT_ASYNC_CONNECT_Const },` |
|       - | 3458 | `	{"STREAM_CLIENT_PERSISTENT",     PH7_STREAM_CLIENT_PERSISTENT_Const },` |
|       - | 3459 | `	{"PSFS_PASS_ON",                 PH7_PSFS_PASS_ON_Const },` |
|       - | 3460 | `	{"PSFS_FEED_ME",                 PH7_PSFS_FEED_ME_Const },` |
|       - | 3461 | `	{"PSFS_ERR_FATAL",               PH7_PSFS_ERR_FATAL_Const },` |
|       - | 3462 | `	{"PSFS_FLAG_NORMAL",             PH7_PSFS_FLAG_NORMAL_Const },` |
|       - | 3463 | `	{"PSFS_FLAG_FLUSH_INC",          PH7_PSFS_FLAG_FLUSH_INC_Const },` |
|       - | 3464 | `	{"PSFS_FLAG_FLUSH_CLOSE",        PH7_PSFS_FLAG_FLUSH_CLOSE_Const },` |
|       - | 3465 | `	{"STREAM_NOTIFY_RESOLVE",        PH7_STREAM_NOTIFY_RESOLVE_Const },` |
|       - | 3466 | `	{"STREAM_NOTIFY_CONNECT",        PH7_STREAM_NOTIFY_CONNECT_Const },` |
|       - | 3467 | `	{"STREAM_NOTIFY_AUTH_REQUIRED",  PH7_STREAM_NOTIFY_AUTH_REQUIRED_Const },` |
|       - | 3468 | `	{"STREAM_NOTIFY_MIME_TYPE_IS",   PH7_STREAM_NOTIFY_MIME_TYPE_IS_Const },` |
|       - | 3469 | `	{"STREAM_NOTIFY_FILE_SIZE_IS",   PH7_STREAM_NOTIFY_FILE_SIZE_IS_Const },` |
|       - | 3470 | `	{"STREAM_NOTIFY_REDIRECTED",     PH7_STREAM_NOTIFY_REDIRECTED_Const },` |
|       - | 3471 | `	{"STREAM_NOTIFY_PROGRESS",       PH7_STREAM_NOTIFY_PROGRESS_Const },` |
|       - | 3472 | `	{"STREAM_NOTIFY_COMPLETED",      PH7_STREAM_NOTIFY_COMPLETED_Const },` |
|       - | 3473 | `	{"STREAM_NOTIFY_FAILURE",        PH7_STREAM_NOTIFY_FAILURE_Const },` |
|       - | 3474 | `	{"STREAM_NOTIFY_AUTH_RESULT",    PH7_STREAM_NOTIFY_AUTH_RESULT_Const },` |
|       - | 3475 | `	{"STREAM_NOTIFY_SEVERITY_INFO",  PH7_STREAM_NOTIFY_SEVERITY_INFO_Const },` |
|       - | 3476 | `	{"STREAM_NOTIFY_SEVERITY_WARN",  PH7_STREAM_NOTIFY_SEVERITY_WARN_Const },` |
|       - | 3477 | `	{"STREAM_NOTIFY_SEVERITY_ERR",   PH7_STREAM_NOTIFY_SEVERITY_ERR_Const },` |
|       - | 3478 | `	{"STREAM_FILTER_READ",           PH7_STREAM_FILTER_READ_Const },` |
|       - | 3479 | `	{"STREAM_FILTER_WRITE",          PH7_STREAM_FILTER_WRITE_Const },` |
|       - | 3480 | `	{"STREAM_FILTER_ALL",            PH7_STREAM_FILTER_ALL_Const },` |
|       - | 3481 | `	{"STREAM_SHUT_RD",               PH7_STREAM_SHUT_RD_Const },` |
|       - | 3482 | `	{"STREAM_SHUT_WR",               PH7_STREAM_SHUT_WR_Const },` |
|       - | 3483 | `	{"STREAM_SHUT_RDWR",             PH7_STREAM_SHUT_RDWR_Const },` |
|       - | 3484 | `	{"STREAM_OOB",                   PH7_STREAM_OOB_Const },` |
|       - | 3485 | `	{"STREAM_PEEK",                  PH7_STREAM_PEEK_Const },` |
|       - | 3486 | `#ifdef PH7_ENABLE_NET` |
|       - | 3487 | `	{"STREAM_PF_INET",               PH7_STREAM_PF_INET_Const },` |
|       - | 3488 | `	{"STREAM_PF_INET6",              PH7_STREAM_PF_INET6_Const },` |
|       - | 3489 | `	{"STREAM_PF_UNIX",               PH7_STREAM_PF_UNIX_Const },` |
|       - | 3490 | `	{"STREAM_SOCK_STREAM",           PH7_STREAM_SOCK_STREAM_Const },` |
|       - | 3491 | `	{"STREAM_SOCK_DGRAM",            PH7_STREAM_SOCK_DGRAM_Const },` |
|       - | 3492 | `	{"STREAM_SOCK_RAW",              PH7_STREAM_SOCK_RAW_Const },` |
|       - | 3493 | `	{"STREAM_SOCK_SEQPACKET",        PH7_STREAM_SOCK_SEQPACKET_Const },` |
|       - | 3494 | `	{"STREAM_SOCK_RDM",              PH7_STREAM_SOCK_RDM_Const },` |
|       - | 3495 | `	{"STREAM_IPPROTO_IP",            PH7_STREAM_IPPROTO_IP_Const },` |
|       - | 3496 | `	{"STREAM_IPPROTO_TCP",           PH7_STREAM_IPPROTO_TCP_Const },` |
|       - | 3497 | `	{"STREAM_IPPROTO_UDP",           PH7_STREAM_IPPROTO_UDP_Const },` |
|       - | 3498 | `	{"STREAM_IPPROTO_ICMP",          PH7_STREAM_IPPROTO_ICMP_Const },` |
|       - | 3499 | `	{"STREAM_IPPROTO_RAW",           PH7_STREAM_IPPROTO_RAW_Const },` |
|       - | 3500 | `#endif` |
|       - | 3501 | `	{"MT_RAND_MT19937",              PH7_MT_RAND_MT19937_Const },` |
|       - | 3502 | `	{"MT_RAND_PHP",                  PH7_MT_RAND_PHP_Const  },` |
|       - | 3503 | `	{"PHP_OUTPUT_HANDLER_WRITE",     PH7_OB_WRITE_Const     },` |
|       - | 3504 | `	{"PHP_OUTPUT_HANDLER_CONT",      PH7_OB_WRITE_Const     },` |
|       - | 3505 | `	{"PHP_OUTPUT_HANDLER_START",     PH7_OB_START_Const     },` |
|       - | 3506 | `	{"PHP_OUTPUT_HANDLER_CLEAN",     PH7_OB_CLEAN_Const     },` |
|       - | 3507 | `	{"PHP_OUTPUT_HANDLER_FLUSH",     PH7_OB_FLUSH_Const     },` |
|       - | 3508 | `	{"PHP_OUTPUT_HANDLER_FINAL",     PH7_OB_FINAL_Const     },` |
|       - | 3509 | `	{"PHP_OUTPUT_HANDLER_END",       PH7_OB_FINAL_Const     },` |
|       - | 3510 | `	{"PHP_OUTPUT_HANDLER_CLEANABLE", PH7_OB_CLEANABLE_Const },` |
|       - | 3511 | `	{"PHP_OUTPUT_HANDLER_FLUSHABLE", PH7_OB_FLUSHABLE_Const },` |
|       - | 3512 | `	{"PHP_OUTPUT_HANDLER_REMOVABLE", PH7_OB_REMOVABLE_Const },` |
|       - | 3513 | `	{"PHP_OUTPUT_HANDLER_STDFLAGS",  PH7_OB_STDFLAGS_Const  },` |
|       - | 3514 | `	{"PHP_OUTPUT_HANDLER_STARTED",   PH7_OB_STARTED_Const   },` |
|       - | 3515 | `	{"PHP_OUTPUT_HANDLER_DISABLED",  PH7_OB_DISABLED_Const  },` |
|       - | 3516 | `	{"PHP_OUTPUT_HANDLER_PROCESSED", PH7_OB_PROCESSED_Const },` |
|       - | 3517 | `	{"CONNECTION_NORMAL",    PH7_CONNECTION_NORMAL_Const  },` |
|       - | 3518 | `	{"CONNECTION_ABORTED",   PH7_CONNECTION_ABORTED_Const },` |
|       - | 3519 | `	{"CONNECTION_TIMEOUT",   PH7_CONNECTION_TIMEOUT_Const },` |
|       - | 3520 | `	{"ARRAY_FILTER_USE_KEY", PH7_ARRAY_FILTER_USE_KEY_Const },` |
|       - | 3521 | `	{"ARRAY_FILTER_USE_BOTH",PH7_ARRAY_FILTER_USE_BOTH_Const},` |
|       - | 3522 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|       - | 3523 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|       - | 3524 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|       - | 3525 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|       - | 3526 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|       - | 3527 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|       - | 3528 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|       - | 3529 | `	{"SORT_LOCALE_STRING",   PH7_SORT_LOCALE_STRING_Const },` |
|       - | 3530 | `	{"SORT_NATURAL",         PH7_SORT_NATURAL_Const },` |
|       - | 3531 | `	{"SORT_FLAG_CASE",       PH7_SORT_FLAG_CASE_Const },` |
|       - | 3532 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|       - | 3533 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|       - | 3534 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|       - | 3535 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|       - | 3536 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|       - | 3537 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|       - | 3538 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|       - | 3539 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|       - | 3540 | `	{"M_E",                  PH7_M_E_Const          },` |
|       - | 3541 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|       - | 3542 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|       - | 3543 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|       - | 3544 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|       - | 3545 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|       - | 3546 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|       - | 3547 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|       - | 3548 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|       - | 3549 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|       - | 3550 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|       - | 3551 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|       - | 3552 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|       - | 3553 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|       - | 3554 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|       - | 3555 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|       - | 3556 | `	{"NAN",                  PH7_NAN_Const          },` |
|       - | 3557 | `	{"INF",                  PH7_INF_Const          },` |
|       - | 3558 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|       - | 3559 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|       - | 3560 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|       - | 3561 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|       - | 3562 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|       - | 3563 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|       - | 3564 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|       - | 3565 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|       - | 3566 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|       - | 3567 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|       - | 3568 | `	{"DATE_RFC3339_EXTENDED",PH7_DATE_RFC3339_EXTENDED_Const },` |
|       - | 3569 | `	{"DATE_RFC7231",         PH7_DATE_RFC7231_Const },` |
|       - | 3570 | `	{"DATE_ISO8601_EXPANDED",PH7_DATE_ISO8601_EXPANDED_Const },` |
|       - | 3571 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|       - | 3572 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|       - | 3573 | `	{"FILE_TEXT",            PH7_FILE_TEXT_Const    },` |
|       - | 3574 | `	{"FILE_BINARY",          PH7_FILE_TEXT_Const    },` |
|       - | 3575 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|       - | 3576 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|       - | 3577 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|       - | 3578 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|       - | 3579 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|       - | 3580 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|       - | 3581 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|       - | 3582 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|       - | 3583 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|       - | 3584 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|       - | 3585 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|       - | 3586 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|       - | 3587 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|       - | 3588 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|       - | 3589 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|       - | 3590 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|       - | 3591 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|       - | 3592 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|       - | 3593 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|       - | 3594 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|       - | 3595 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|       - | 3596 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|       - | 3597 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|       - | 3598 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|       - | 3599 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|       - | 3600 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|       - | 3601 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|       - | 3602 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|       - | 3603 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|       - | 3604 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|       - | 3605 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|       - | 3606 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|       - | 3607 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|       - | 3608 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|       - | 3609 | `	{"PATHINFO_ALL",         PH7_PATHINFO_ALL_Const },` |
|       - | 3610 | `	/* ASSERT_QUIET_EVAL was REMOVED in php 8.0: referencing it is an Error there */` |
|       - | 3611 | `#ifdef PH7_ENABLE_PCRE` |
|       - | 3612 | `	{"PCRE_VERSION",         PH7_PCRE_VERSION_Const  },` |
|       - | 3613 | `	{"PCRE_VERSION_MAJOR",   PH7_PCRE_VERSION_MAJOR_Const },` |
|       - | 3614 | `	{"PCRE_VERSION_MINOR",   PH7_PCRE_VERSION_MINOR_Const },` |
|       - | 3615 | `	{"PCRE_JIT_SUPPORT",     PH7_PCRE_JIT_SUPPORT_Const },` |
|       - | 3616 | `#endif` |
|       - | 3617 | `	{"ZEND_THREAD_SAFE",     PH7_ZEND_THREAD_SAFE_Const },` |
|       - | 3618 | `	{"ZEND_DEBUG_BUILD",     PH7_ZEND_DEBUG_BUILD_Const },` |
|       - | 3619 | `	{"PHP_ZTS",              PH7_PHP_ZTS_Const       },` |
|       - | 3620 | `	{"PHP_DEBUG",            PH7_PHP_DEBUG_Const     },` |
|       - | 3621 | `	{"INFO_GENERAL",         PH7_INFO_GENERAL_Const  },` |
|       - | 3622 | `	{"INFO_CREDITS",         PH7_INFO_CREDITS_Const  },` |
|       - | 3623 | `	{"INFO_CONFIGURATION",   PH7_INFO_CONFIGURATION_Const },` |
|       - | 3624 | `	{"INFO_MODULES",         PH7_INFO_MODULES_Const  },` |
|       - | 3625 | `	{"INFO_ENVIRONMENT",     PH7_INFO_ENVIRONMENT_Const },` |
|       - | 3626 | `	{"INFO_VARIABLES",       PH7_INFO_VARIABLES_Const },` |
|       - | 3627 | `	{"INFO_LICENSE",         PH7_INFO_LICENSE_Const  },` |
|       - | 3628 | `	{"INFO_ALL",             PH7_INFO_ALL_Const      },` |
|       - | 3629 | `	{"LC_CTYPE",             PH7_LC_CTYPE_Const      },` |
|       - | 3630 | `	{"LC_NUMERIC",           PH7_LC_NUMERIC_Const    },` |
|       - | 3631 | `	{"LC_TIME",              PH7_LC_TIME_Const       },` |
|       - | 3632 | `	{"LC_COLLATE",           PH7_LC_COLLATE_Const    },` |
|       - | 3633 | `	{"LC_MONETARY",          PH7_LC_MONETARY_Const   },` |
|       - | 3634 | `	{"LC_MESSAGES",          PH7_LC_MESSAGES_Const   },` |
|       - | 3635 | `	{"LC_ALL",               PH7_LC_ALL_Const        },` |
|       - | 3636 | `#ifndef __WINNT__` |
|       - | 3637 | `	/* ext/posix */` |
|       - | 3638 | `#ifdef F_OK` |
|       - | 3639 | `	{"POSIX_F_OK", PH7_POSIX_F_OK_Const },` |
|       - | 3640 | `#endif` |
|       - | 3641 | `#ifdef X_OK` |
|       - | 3642 | `	{"POSIX_X_OK", PH7_POSIX_X_OK_Const },` |
|       - | 3643 | `#endif` |
|       - | 3644 | `#ifdef W_OK` |
|       - | 3645 | `	{"POSIX_W_OK", PH7_POSIX_W_OK_Const },` |
|       - | 3646 | `#endif` |
|       - | 3647 | `#ifdef R_OK` |
|       - | 3648 | `	{"POSIX_R_OK", PH7_POSIX_R_OK_Const },` |
|       - | 3649 | `#endif` |
|       - | 3650 | `#ifdef S_IFREG` |
|       - | 3651 | `	{"POSIX_S_IFREG", PH7_POSIX_S_IFREG_Const },` |
|       - | 3652 | `#endif` |
|       - | 3653 | `#ifdef S_IFCHR` |
|       - | 3654 | `	{"POSIX_S_IFCHR", PH7_POSIX_S_IFCHR_Const },` |
|       - | 3655 | `#endif` |
|       - | 3656 | `#ifdef S_IFBLK` |
|       - | 3657 | `	{"POSIX_S_IFBLK", PH7_POSIX_S_IFBLK_Const },` |
|       - | 3658 | `#endif` |
|       - | 3659 | `#ifdef S_IFIFO` |
|       - | 3660 | `	{"POSIX_S_IFIFO", PH7_POSIX_S_IFIFO_Const },` |
|       - | 3661 | `#endif` |
|       - | 3662 | `#ifdef S_IFSOCK` |
|       - | 3663 | `	{"POSIX_S_IFSOCK", PH7_POSIX_S_IFSOCK_Const },` |
|       - | 3664 | `#endif` |
|       - | 3665 | `#ifdef RLIMIT_AS` |
|       - | 3666 | `	{"POSIX_RLIMIT_AS", PH7_POSIX_RLIMIT_AS_Const },` |
|       - | 3667 | `#endif` |
|       - | 3668 | `#ifdef RLIMIT_CORE` |
|       - | 3669 | `	{"POSIX_RLIMIT_CORE", PH7_POSIX_RLIMIT_CORE_Const },` |
|       - | 3670 | `#endif` |
|       - | 3671 | `#ifdef RLIMIT_CPU` |
|       - | 3672 | `	{"POSIX_RLIMIT_CPU", PH7_POSIX_RLIMIT_CPU_Const },` |
|       - | 3673 | `#endif` |
|       - | 3674 | `#ifdef RLIMIT_DATA` |
|       - | 3675 | `	{"POSIX_RLIMIT_DATA", PH7_POSIX_RLIMIT_DATA_Const },` |
|       - | 3676 | `#endif` |
|       - | 3677 | `#ifdef RLIMIT_FSIZE` |
|       - | 3678 | `	{"POSIX_RLIMIT_FSIZE", PH7_POSIX_RLIMIT_FSIZE_Const },` |
|       - | 3679 | `#endif` |
|       - | 3680 | `#ifdef RLIMIT_LOCKS` |
|       - | 3681 | `	{"POSIX_RLIMIT_LOCKS", PH7_POSIX_RLIMIT_LOCKS_Const },` |
|       - | 3682 | `#endif` |
|       - | 3683 | `#ifdef RLIMIT_MEMLOCK` |
|       - | 3684 | `	{"POSIX_RLIMIT_MEMLOCK", PH7_POSIX_RLIMIT_MEMLOCK_Const },` |
|       - | 3685 | `#endif` |
|       - | 3686 | `#ifdef RLIMIT_MSGQUEUE` |
|       - | 3687 | `	{"POSIX_RLIMIT_MSGQUEUE", PH7_POSIX_RLIMIT_MSGQUEUE_Const },` |
|       - | 3688 | `#endif` |
|       - | 3689 | `#ifdef RLIMIT_NICE` |
|       - | 3690 | `	{"POSIX_RLIMIT_NICE", PH7_POSIX_RLIMIT_NICE_Const },` |
|       - | 3691 | `#endif` |
|       - | 3692 | `#ifdef RLIMIT_NOFILE` |
|       - | 3693 | `	{"POSIX_RLIMIT_NOFILE", PH7_POSIX_RLIMIT_NOFILE_Const },` |
|       - | 3694 | `#endif` |
|       - | 3695 | `#ifdef RLIMIT_NPROC` |
|       - | 3696 | `	{"POSIX_RLIMIT_NPROC", PH7_POSIX_RLIMIT_NPROC_Const },` |
|       - | 3697 | `#endif` |
|       - | 3698 | `#ifdef RLIMIT_RSS` |
|       - | 3699 | `	{"POSIX_RLIMIT_RSS", PH7_POSIX_RLIMIT_RSS_Const },` |
|       - | 3700 | `#endif` |
|       - | 3701 | `#ifdef RLIMIT_RTPRIO` |
|       - | 3702 | `	{"POSIX_RLIMIT_RTPRIO", PH7_POSIX_RLIMIT_RTPRIO_Const },` |
|       - | 3703 | `#endif` |
|       - | 3704 | `#ifdef RLIMIT_RTTIME` |
|       - | 3705 | `	{"POSIX_RLIMIT_RTTIME", PH7_POSIX_RLIMIT_RTTIME_Const },` |
|       - | 3706 | `#endif` |
|       - | 3707 | `#ifdef RLIMIT_SIGPENDING` |
|       - | 3708 | `	{"POSIX_RLIMIT_SIGPENDING", PH7_POSIX_RLIMIT_SIGPENDING_Const },` |
|       - | 3709 | `#endif` |
|       - | 3710 | `#ifdef RLIMIT_STACK` |
|       - | 3711 | `	{"POSIX_RLIMIT_STACK", PH7_POSIX_RLIMIT_STACK_Const },` |
|       - | 3712 | `#endif` |
|       - | 3713 | `#ifdef _SC_ARG_MAX` |
|       - | 3714 | `	{"POSIX_SC_ARG_MAX", PH7_POSIX_SC_ARG_MAX_Const },` |
|       - | 3715 | `#endif` |
|       - | 3716 | `#ifdef _SC_CHILD_MAX` |
|       - | 3717 | `	{"POSIX_SC_CHILD_MAX", PH7_POSIX_SC_CHILD_MAX_Const },` |
|       - | 3718 | `#endif` |
|       - | 3719 | `#ifdef _SC_CLK_TCK` |
|       - | 3720 | `	{"POSIX_SC_CLK_TCK", PH7_POSIX_SC_CLK_TCK_Const },` |
|       - | 3721 | `#endif` |
|       - | 3722 | `#ifdef _SC_OPEN_MAX` |
|       - | 3723 | `	{"POSIX_SC_OPEN_MAX", PH7_POSIX_SC_OPEN_MAX_Const },` |
|       - | 3724 | `#endif` |
|       - | 3725 | `#ifdef _SC_PAGESIZE` |
|       - | 3726 | `	{"POSIX_SC_PAGESIZE", PH7_POSIX_SC_PAGESIZE_Const },` |
|       - | 3727 | `#endif` |
|       - | 3728 | `#ifdef _SC_NPROCESSORS_CONF` |
|       - | 3729 | `	{"POSIX_SC_NPROCESSORS_CONF", PH7_POSIX_SC_NPROCESSORS_CONF_Const },` |
|       - | 3730 | `#endif` |
|       - | 3731 | `#ifdef _SC_NPROCESSORS_ONLN` |
|       - | 3732 | `	{"POSIX_SC_NPROCESSORS_ONLN", PH7_POSIX_SC_NPROCESSORS_ONLN_Const },` |
|       - | 3733 | `#endif` |
|       - | 3734 | `#ifdef _PC_LINK_MAX` |
|       - | 3735 | `	{"POSIX_PC_LINK_MAX", PH7_POSIX_PC_LINK_MAX_Const },` |
|       - | 3736 | `#endif` |
|       - | 3737 | `#ifdef _PC_MAX_CANON` |
|       - | 3738 | `	{"POSIX_PC_MAX_CANON", PH7_POSIX_PC_MAX_CANON_Const },` |
|       - | 3739 | `#endif` |
|       - | 3740 | `#ifdef _PC_MAX_INPUT` |
|       - | 3741 | `	{"POSIX_PC_MAX_INPUT", PH7_POSIX_PC_MAX_INPUT_Const },` |
|       - | 3742 | `#endif` |
|       - | 3743 | `#ifdef _PC_NAME_MAX` |
|       - | 3744 | `	{"POSIX_PC_NAME_MAX", PH7_POSIX_PC_NAME_MAX_Const },` |
|       - | 3745 | `#endif` |
|       - | 3746 | `#ifdef _PC_PATH_MAX` |
|       - | 3747 | `	{"POSIX_PC_PATH_MAX", PH7_POSIX_PC_PATH_MAX_Const },` |
|       - | 3748 | `#endif` |
|       - | 3749 | `#ifdef _PC_PIPE_BUF` |
|       - | 3750 | `	{"POSIX_PC_PIPE_BUF", PH7_POSIX_PC_PIPE_BUF_Const },` |
|       - | 3751 | `#endif` |
|       - | 3752 | `#ifdef _PC_CHOWN_RESTRICTED` |
|       - | 3753 | `	{"POSIX_PC_CHOWN_RESTRICTED", PH7_POSIX_PC_CHOWN_RESTRICTED_Const },` |
|       - | 3754 | `#endif` |
|       - | 3755 | `#ifdef _PC_NO_TRUNC` |
|       - | 3756 | `	{"POSIX_PC_NO_TRUNC", PH7_POSIX_PC_NO_TRUNC_Const },` |
|       - | 3757 | `#endif` |
|       - | 3758 | `#ifdef _PC_ALLOC_SIZE_MIN` |
|       - | 3759 | `	{"POSIX_PC_ALLOC_SIZE_MIN", PH7_POSIX_PC_ALLOC_SIZE_MIN_Const },` |
|       - | 3760 | `#endif` |
|       - | 3761 | `#ifdef _PC_SYMLINK_MAX` |
|       - | 3762 | `	{"POSIX_PC_SYMLINK_MAX", PH7_POSIX_PC_SYMLINK_MAX_Const },` |
|       - | 3763 | `#endif` |
|       - | 3764 | `	{"POSIX_RLIMIT_INFINITY", PH7_POSIX_RLIMIT_INFINITY_Const },` |
|       - | 3765 | `#endif /* __WINNT__ */` |
|       - | 3766 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|       - | 3767 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|       - | 3768 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|       - | 3769 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|       - | 3770 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|       - | 3771 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|       - | 3772 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|       - | 3773 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|       - | 3774 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|       - | 3775 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|       - | 3776 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|       - | 3777 | `	{"FILE_NO_DEFAULT_CONTEXT", PH7_FILE_NO_DEFAULT_CONTEXT_Const },` |
|       - | 3778 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|       - | 3779 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|       - | 3780 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|       - | 3781 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|       - | 3782 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|       - | 3783 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|       - | 3784 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|       - | 3785 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|       - | 3786 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|       - | 3787 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|       - | 3788 | `	{"GLOB_AVAILABLE_FLAGS", PH7_GLOB_AVAILABLE_FLAGS_Const },` |
|       - | 3789 | `	{"STDIN",                PH7_STDIN_Const        },` |
|       - | 3790 | `	{"stdin",                PH7_STDIN_Const        },` |
|       - | 3791 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|       - | 3792 | `	{"stdout",               PH7_STDOUT_Const       },` |
|       - | 3793 | `	{"STDERR",               PH7_STDERR_Const       },` |
|       - | 3794 | `	{"stderr",               PH7_STDERR_Const       },` |
|       - | 3795 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|       - | 3796 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|       - | 3797 | `	{"INI_SCANNER_TYPED",    PH7_INI_SCANNER_TYPED_Const  },` |
|       - | 3798 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|       - | 3799 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|       - | 3800 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|       - | 3801 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|       - | 3802 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|       - | 3803 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|       - | 3804 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|       - | 3805 | `	{"EXTR_REFS",            PH7_EXTR_REFS_Const        },` |
|       - | 3806 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|       - | 3807 | `	{"HASH_HMAC",              PH7_HASH_HMAC_Const},` |
|       - | 3808 | `	{"CRYPT_SALT_LENGTH",      PH7_CRYPT_SALT_LENGTH_Const},` |
|       - | 3809 | `	{"CRYPT_STD_DES",          PH7_CRYPT_ONE_Const},` |
|       - | 3810 | `	{"CRYPT_EXT_DES",          PH7_CRYPT_ONE_Const},` |
|       - | 3811 | `	{"CRYPT_MD5",              PH7_CRYPT_ONE_Const},` |
|       - | 3812 | `	{"CRYPT_BLOWFISH",         PH7_CRYPT_ONE_Const},` |
|       - | 3813 | `	{"CRYPT_SHA256",           PH7_CRYPT_ONE_Const},` |
|       - | 3814 | `	{"CRYPT_SHA512",           PH7_CRYPT_ONE_Const},` |
|       - | 3815 | `#endif` |
|       - | 3816 | `	{"ICONV_IMPL",             PH7_ICONV_IMPL_Const},` |
|       - | 3817 | `	{"ICONV_VERSION",          PH7_ICONV_VERSION_Const},` |
|       - | 3818 | `	{"ICONV_MIME_DECODE_STRICT", PH7_ICONV_MIME_DECODE_STRICT_Const},` |
|       - | 3819 | `	{"ICONV_MIME_DECODE_CONTINUE_ON_ERROR", PH7_ICONV_MIME_DECODE_CONTINUE_ON_ERROR_Const},` |
|       - | 3820 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|       - | 3821 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|       - | 3822 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|       - | 3823 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|       - | 3824 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|       - | 3825 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|       - | 3826 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|       - | 3827 | `	{"JSON_OBJECT_AS_ARRAY",   PH7_JSON_OBJECT_AS_ARRAY_Const},` |
|       - | 3828 | `	{"JSON_PARTIAL_OUTPUT_ON_ERROR", PH7_JSON_PARTIAL_OUTPUT_ON_ERROR_Const},` |
|       - | 3829 | `	{"JSON_PRESERVE_ZERO_FRACTION",  PH7_JSON_PRESERVE_ZERO_FRACTION_Const},` |
|       - | 3830 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|       - | 3831 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|       - | 3832 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|       - | 3833 | `	{"JSON_UNESCAPED_LINE_TERMINATORS", PH7_JSON_UNESCAPED_LINE_TERMINATORS_Const},` |
|       - | 3834 | `	{"JSON_INVALID_UTF8_IGNORE", PH7_JSON_INVALID_UTF8_IGNORE_Const},` |
|       - | 3835 | `	{"JSON_INVALID_UTF8_SUBSTITUTE", PH7_JSON_INVALID_UTF8_SUBSTITUTE_Const},` |
|       - | 3836 | `	{"JSON_THROW_ON_ERROR",    PH7_JSON_THROW_ON_ERROR_Const},` |
|       - | 3837 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|       - | 3838 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|       - | 3839 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|       - | 3840 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|       - | 3841 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|       - | 3842 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|       - | 3843 | `	{"JSON_ERROR_RECURSION", PH7_JSON_ERROR_RECURSION_Const},` |
|       - | 3844 | `	{"JSON_ERROR_UNSUPPORTED_TYPE", PH7_JSON_ERROR_UNSUPPORTED_TYPE_Const},` |
|       - | 3845 | `	{"JSON_ERROR_INVALID_PROPERTY_NAME", PH7_JSON_ERROR_INVALID_PROPERTY_NAME_Const},` |
|       - | 3846 | `	{"JSON_ERROR_UTF16",     PH7_JSON_ERROR_UTF16_Const},` |
|       - | 3847 | `	{"JSON_ERROR_NON_BACKED_ENUM", PH7_JSON_ERROR_NON_BACKED_ENUM_Const},` |
|       - | 3848 | `	{"JSON_ERROR_INF_OR_NAN", PH7_JSON_ERROR_INF_OR_NAN_Const},` |
|       - | 3849 | ``	/* `self`, `parent` and `static` are KEYWORDS in php, not constants: using one as a bare`` |
|       - | 3850 | ``	 * word is an "Undefined constant" Error (or a parse error for `static`). PH7 registered`` |
|       - | 3851 | `	 * them as constants that quietly expanded to the class name / NULL, so a typo'd bare` |
|       - | 3852 | ``	 * word silently produced a value. The `self::`/`parent::`/`static::` forms are handled`` |
|       - | 3853 | ``	 * by the `::` compile path and do not go through the constant table. */`` |
|       - | 3854 | `#ifdef PH7_ENABLE_ZLIB` |
|       - | 3855 | `	{"FORCE_GZIP",             PH7_FORCE_GZIP_Const },` |
|       - | 3856 | `	{"FORCE_DEFLATE",          PH7_FORCE_DEFLATE_Const },` |
|       - | 3857 | `	{"ZLIB_ENCODING_RAW",      PH7_ZLIB_ENCODING_RAW_Const },` |
|       - | 3858 | `	{"ZLIB_ENCODING_GZIP",     PH7_ZLIB_ENCODING_GZIP_Const },` |
|       - | 3859 | `	{"ZLIB_ENCODING_DEFLATE",  PH7_ZLIB_ENCODING_DEFLATE_Const },` |
|       - | 3860 | `	{"ZLIB_NO_FLUSH",          PH7_ZLIB_NO_FLUSH_Const },` |
|       - | 3861 | `	{"ZLIB_PARTIAL_FLUSH",     PH7_ZLIB_PARTIAL_FLUSH_Const },` |
|       - | 3862 | `	{"ZLIB_SYNC_FLUSH",        PH7_ZLIB_SYNC_FLUSH_Const },` |
|       - | 3863 | `	{"ZLIB_FULL_FLUSH",        PH7_ZLIB_FULL_FLUSH_Const },` |
|       - | 3864 | `	{"ZLIB_BLOCK",             PH7_ZLIB_BLOCK_Const },` |
|       - | 3865 | `	{"ZLIB_FINISH",            PH7_ZLIB_FINISH_Const },` |
|       - | 3866 | `	{"ZLIB_FILTERED",          PH7_ZLIB_FILTERED_Const },` |
|       - | 3867 | `	{"ZLIB_HUFFMAN_ONLY",      PH7_ZLIB_HUFFMAN_ONLY_Const },` |
|       - | 3868 | `	{"ZLIB_RLE",               PH7_ZLIB_RLE_Const },` |
|       - | 3869 | `	{"ZLIB_FIXED",             PH7_ZLIB_FIXED_Const },` |
|       - | 3870 | `	{"ZLIB_DEFAULT_STRATEGY",  PH7_ZLIB_DEFAULT_STRATEGY_Const },` |
|       - | 3871 | `	{"ZLIB_VERSION",           PH7_ZLIB_VERSION_Const },` |
|       - | 3872 | `	{"ZLIB_VERNUM",            PH7_ZLIB_VERNUM_Const },` |
|       - | 3873 | `	{"ZLIB_OK",                PH7_ZLIB_OK_Const },` |
|       - | 3874 | `	{"ZLIB_STREAM_END",        PH7_ZLIB_STREAM_END_Const },` |
|       - | 3875 | `	{"ZLIB_NEED_DICT",         PH7_ZLIB_NEED_DICT_Const },` |
|       - | 3876 | `	{"ZLIB_ERRNO",             PH7_ZLIB_ERRNO_Const },` |
|       - | 3877 | `	{"ZLIB_STREAM_ERROR",      PH7_ZLIB_STREAM_ERROR_Const },` |
|       - | 3878 | `	{"ZLIB_DATA_ERROR",        PH7_ZLIB_DATA_ERROR_Const },` |
|       - | 3879 | `	{"ZLIB_MEM_ERROR",         PH7_ZLIB_MEM_ERROR_Const },` |
|       - | 3880 | `	{"ZLIB_BUF_ERROR",         PH7_ZLIB_BUF_ERROR_Const },` |
|       - | 3881 | `	{"ZLIB_VERSION_ERROR",     PH7_ZLIB_VERSION_ERROR_Const },` |
|       - | 3882 | `#endif /* PH7_ENABLE_ZLIB */` |
|       - | 3883 | `#ifdef PH7_ENABLE_OPENSSL` |
|       - | 3884 | `	{"OPENSSL_VERSION_TEXT",              PH7_OPENSSL_VERSION_TEXT_Const },` |
|       - | 3885 | `	{"OPENSSL_VERSION_NUMBER",            PH7_OPENSSL_VERSION_NUMBER_Const },` |
|       - | 3886 | `	{"X509_PURPOSE_SSL_CLIENT",           PH7_X509_PURPOSE_SSL_CLIENT_Const },` |
|       - | 3887 | `	{"X509_PURPOSE_SSL_SERVER",           PH7_X509_PURPOSE_SSL_SERVER_Const },` |
|       - | 3888 | `	{"X509_PURPOSE_NS_SSL_SERVER",        PH7_X509_PURPOSE_NS_SSL_SERVER_Const },` |
|       - | 3889 | `	{"X509_PURPOSE_SMIME_SIGN",           PH7_X509_PURPOSE_SMIME_SIGN_Const },` |
|       - | 3890 | `	{"X509_PURPOSE_SMIME_ENCRYPT",        PH7_X509_PURPOSE_SMIME_ENCRYPT_Const },` |
|       - | 3891 | `	{"X509_PURPOSE_CRL_SIGN",             PH7_X509_PURPOSE_CRL_SIGN_Const },` |
|       - | 3892 | `	{"X509_PURPOSE_ANY",                  PH7_X509_PURPOSE_ANY_Const },` |
|       - | 3893 | `	{"X509_PURPOSE_OCSP_HELPER",          PH7_X509_PURPOSE_OCSP_HELPER_Const },` |
|       - | 3894 | `	{"X509_PURPOSE_TIMESTAMP_SIGN",       PH7_X509_PURPOSE_TIMESTAMP_SIGN_Const },` |
|       - | 3895 | `	{"OPENSSL_ALGO_SHA1",                 PH7_OPENSSL_ALGO_SHA1_Const },` |
|       - | 3896 | `	{"OPENSSL_ALGO_MD5",                  PH7_OPENSSL_ALGO_MD5_Const },` |
|       - | 3897 | `	{"OPENSSL_ALGO_MD4",                  PH7_OPENSSL_ALGO_MD4_Const },` |
|       - | 3898 | `	{"OPENSSL_ALGO_SHA224",               PH7_OPENSSL_ALGO_SHA224_Const },` |
|       - | 3899 | `	{"OPENSSL_ALGO_SHA256",               PH7_OPENSSL_ALGO_SHA256_Const },` |
|       - | 3900 | `	{"OPENSSL_ALGO_SHA384",               PH7_OPENSSL_ALGO_SHA384_Const },` |
|       - | 3901 | `	{"OPENSSL_ALGO_SHA512",               PH7_OPENSSL_ALGO_SHA512_Const },` |
|       - | 3902 | `	{"OPENSSL_ALGO_RMD160",               PH7_OPENSSL_ALGO_RMD160_Const },` |
|       - | 3903 | `	{"PKCS7_DETACHED",                    PH7_PKCS7_DETACHED_Const },` |
|       - | 3904 | `	{"PKCS7_TEXT",                        PH7_PKCS7_TEXT_Const },` |
|       - | 3905 | `	{"PKCS7_NOINTERN",                    PH7_PKCS7_NOINTERN_Const },` |
|       - | 3906 | `	{"PKCS7_NOVERIFY",                    PH7_PKCS7_NOVERIFY_Const },` |
|       - | 3907 | `	{"PKCS7_NOCHAIN",                     PH7_PKCS7_NOCHAIN_Const },` |
|       - | 3908 | `	{"PKCS7_NOCERTS",                     PH7_PKCS7_NOCERTS_Const },` |
|       - | 3909 | `	{"PKCS7_NOATTR",                      PH7_PKCS7_NOATTR_Const },` |
|       - | 3910 | `	{"PKCS7_BINARY",                      PH7_PKCS7_BINARY_Const },` |
|       - | 3911 | `	{"PKCS7_NOSIGS",                      PH7_PKCS7_NOSIGS_Const },` |
|       - | 3912 | `	{"PKCS7_NOOLDMIMETYPE",               PH7_PKCS7_NOOLDMIMETYPE_Const },` |
|       - | 3913 | `	{"PKCS7_NOSMIMECAP",                  PH7_PKCS7_NOSMIMECAP_Const },` |
|       - | 3914 | `	{"PKCS7_CRLFEOL",                     PH7_PKCS7_CRLFEOL_Const },` |
|       - | 3915 | `	{"PKCS7_NOCRL",                       PH7_PKCS7_NOCRL_Const },` |
|       - | 3916 | `	{"PKCS7_NO_DUAL_CONTENT",             PH7_PKCS7_NO_DUAL_CONTENT_Const },` |
|       - | 3917 | `	{"OPENSSL_CMS_DETACHED",              PH7_OPENSSL_CMS_DETACHED_Const },` |
|       - | 3918 | `	{"OPENSSL_CMS_TEXT",                  PH7_OPENSSL_CMS_TEXT_Const },` |
|       - | 3919 | `	{"OPENSSL_CMS_NOINTERN",              PH7_OPENSSL_CMS_NOINTERN_Const },` |
|       - | 3920 | `	{"OPENSSL_CMS_NOVERIFY",              PH7_OPENSSL_CMS_NOVERIFY_Const },` |
|       - | 3921 | `	{"OPENSSL_CMS_NOCERTS",               PH7_OPENSSL_CMS_NOCERTS_Const },` |
|       - | 3922 | `	{"OPENSSL_CMS_NOATTR",                PH7_OPENSSL_CMS_NOATTR_Const },` |
|       - | 3923 | `	{"OPENSSL_CMS_BINARY",                PH7_OPENSSL_CMS_BINARY_Const },` |
|       - | 3924 | `	{"OPENSSL_CMS_NOSIGS",                PH7_OPENSSL_CMS_NOSIGS_Const },` |
|       - | 3925 | `	{"OPENSSL_CMS_OLDMIMETYPE",           PH7_OPENSSL_CMS_OLDMIMETYPE_Const },` |
|       - | 3926 | `	{"OPENSSL_PKCS1_PADDING",             PH7_OPENSSL_PKCS1_PADDING_Const },` |
|       - | 3927 | `	{"OPENSSL_NO_PADDING",                PH7_OPENSSL_NO_PADDING_Const },` |
|       - | 3928 | `	{"OPENSSL_PKCS1_OAEP_PADDING",        PH7_OPENSSL_PKCS1_OAEP_PADDING_Const },` |
|       - | 3929 | `	{"OPENSSL_PKCS1_PSS_PADDING",         PH7_OPENSSL_PKCS1_PSS_PADDING_Const },` |
|       - | 3930 | `	{"OPENSSL_DEFAULT_STREAM_CIPHERS",    PH7_OPENSSL_DEFAULT_STREAM_CIPHERS_Const },` |
|       - | 3931 | `	{"OPENSSL_CIPHER_RC2_40",             PH7_OPENSSL_CIPHER_RC2_40_Const },` |
|       - | 3932 | `	{"OPENSSL_CIPHER_RC2_128",            PH7_OPENSSL_CIPHER_RC2_128_Const },` |
|       - | 3933 | `	{"OPENSSL_CIPHER_RC2_64",             PH7_OPENSSL_CIPHER_RC2_64_Const },` |
|       - | 3934 | `	{"OPENSSL_CIPHER_DES",                PH7_OPENSSL_CIPHER_DES_Const },` |
|       - | 3935 | `	{"OPENSSL_CIPHER_3DES",               PH7_OPENSSL_CIPHER_3DES_Const },` |
|       - | 3936 | `	{"OPENSSL_CIPHER_AES_128_CBC",        PH7_OPENSSL_CIPHER_AES_128_CBC_Const },` |
|       - | 3937 | `	{"OPENSSL_CIPHER_AES_192_CBC",        PH7_OPENSSL_CIPHER_AES_192_CBC_Const },` |
|       - | 3938 | `	{"OPENSSL_CIPHER_AES_256_CBC",        PH7_OPENSSL_CIPHER_AES_256_CBC_Const },` |
|       - | 3939 | `	{"OPENSSL_KEYTYPE_RSA",               PH7_OPENSSL_KEYTYPE_RSA_Const },` |
|       - | 3940 | `	{"OPENSSL_KEYTYPE_DSA",               PH7_OPENSSL_KEYTYPE_DSA_Const },` |
|       - | 3941 | `	{"OPENSSL_KEYTYPE_DH",                PH7_OPENSSL_KEYTYPE_DH_Const },` |
|       - | 3942 | `	{"OPENSSL_KEYTYPE_EC",                PH7_OPENSSL_KEYTYPE_EC_Const },` |
|       - | 3943 | `	{"OPENSSL_KEYTYPE_X25519",            PH7_OPENSSL_KEYTYPE_X25519_Const },` |
|       - | 3944 | `	{"OPENSSL_KEYTYPE_ED25519",           PH7_OPENSSL_KEYTYPE_ED25519_Const },` |
|       - | 3945 | `	{"OPENSSL_KEYTYPE_X448",              PH7_OPENSSL_KEYTYPE_X448_Const },` |
|       - | 3946 | `	{"OPENSSL_KEYTYPE_ED448",             PH7_OPENSSL_KEYTYPE_ED448_Const },` |
|       - | 3947 | `	{"OPENSSL_RAW_DATA",                  PH7_OPENSSL_RAW_DATA_Const },` |
|       - | 3948 | `	{"OPENSSL_ZERO_PADDING",              PH7_OPENSSL_ZERO_PADDING_Const },` |
|       - | 3949 | `	{"OPENSSL_DONT_ZERO_PAD_KEY",         PH7_OPENSSL_DONT_ZERO_PAD_KEY_Const },` |
|       - | 3950 | `	{"OPENSSL_TLSEXT_SERVER_NAME",        PH7_OPENSSL_TLSEXT_SERVER_NAME_Const },` |
|       - | 3951 | `	{"OPENSSL_ENCODING_DER",              PH7_OPENSSL_ENCODING_DER_Const },` |
|       - | 3952 | `	{"OPENSSL_ENCODING_SMIME",            PH7_OPENSSL_ENCODING_SMIME_Const },` |
|       - | 3953 | `	{"OPENSSL_ENCODING_PEM",              PH7_OPENSSL_ENCODING_PEM_Const },` |
|       - | 3954 | `#endif /* PH7_ENABLE_OPENSSL */` |
|       - | 3955 | `	{"FILEINFO_NONE",          PH7_FILEINFO_NONE_Const },` |
|       - | 3956 | `	{"FILEINFO_SYMLINK",       PH7_FILEINFO_SYMLINK_Const },` |
|       - | 3957 | `	{"FILEINFO_MIME",          PH7_FILEINFO_MIME_Const },` |
|       - | 3958 | `	{"FILEINFO_MIME_TYPE",     PH7_FILEINFO_MIME_TYPE_Const },` |
|       - | 3959 | `	{"FILEINFO_MIME_ENCODING", PH7_FILEINFO_MIME_ENCODING_Const },` |
|       - | 3960 | `	{"FILEINFO_DEVICES",       PH7_FILEINFO_DEVICES_Const },` |
|       - | 3961 | `	{"FILEINFO_CONTINUE",      PH7_FILEINFO_CONTINUE_Const },` |
|       - | 3962 | `	{"FILEINFO_PRESERVE_ATIME",PH7_FILEINFO_PRESERVE_ATIME_Const },` |
|       - | 3963 | `	{"FILEINFO_RAW",           PH7_FILEINFO_RAW_Const },` |
|       - | 3964 | `	{"FILEINFO_APPLE",         PH7_FILEINFO_APPLE_Const },` |
|       - | 3965 | `	{"FILEINFO_EXTENSION",     PH7_FILEINFO_EXTENSION_Const },` |
|       - | 3966 | `	{"__CLASS__",            PH7_class_magic_Const  }` |
|       - | 3967 | `};` |
|       - | 3968 | `/*` |
|       - | 3969 | ` * Register the built-in constants defined above.` |
|       - | 3970 | ` */` |
|    5619 | 3971 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|       5 | 3972 | `{` |
|       - | 3973 | `	sxu32 n;` |
|       - | 3974 | `	/*` |
|       - | 3975 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|       - | 3976 | `	 * that trigger the constant invocation as their private data.` |
|       - | 3977 | `	 */` |
| 3343337 | 3978 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 3337718 | 3979 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
| 1657760 | 3980 | `	}` |
|    5624 | 3981 | `}` |
|       - | 3982 | `/*` |
|       - | 3983 | ` * The constants php 8.x deprecated the SYMBOL of, and the reason clause it ends` |
|       - | 3984 | `` * the notice with. Naming one raises `Constant X is deprecated since <clause>`;`` |
|       - | 3985 | ` * LISTING the table does not, which is what pVm->bConstEnum is for.` |
|       - | 3986 | ` *` |
|       - | 3987 | ` * They are marked rather than raised from their own expanders because they are` |
|       - | 3988 | ` * registered in five different units -- date's, random's, the file flags',` |
|       - | 3989 | ``  * curl's and dom's -- and because the export format's `<persistent, deprecated>` `` |
|       - | 3990 | ` * tag has to read the same fact. The stamp runs once, after every extension has` |
|       - | 3991 | ` * installed, so a name a build does not carry is simply skipped.` |
|       - | 3992 | ` */` |
|       - | 3993 | `static const struct {` |
|       - | 3994 | `	const char *zName;` |
|       - | 3995 | `	const char *zWhy;` |
|       - | 3996 | `} aDeprecatedConst[] = {` |
|       - | 3997 | `	{ "DATE_RFC7231",` |
|       - | 3998 | `	  "8.5, as this format ignores the associated timezone and always uses GMT" },` |
|       - | 3999 | `	{ "FILE_TEXT",               "8.1, as the constant has no effect" },` |
|       - | 4000 | `	{ "FILE_BINARY",             "8.1, as the constant has no effect" },` |
|       - | 4001 | `	{ "MT_RAND_PHP",` |
|       - | 4002 | `	  "8.3, as it uses a biased non-standard variant of Mt19937" },` |
|       - | 4003 | `	{ "CURLOPT_BINARYTRANSFER",  "8.4, as it had no effect since 5.1.2" },` |
|       - | 4004 | `	{ "DOM_PHP_ERR",             "8.4, as it is no longer used" },` |
|       - | 4005 | `};` |
|    5619 | 4006 | `PH7_PRIVATE void PH7_MarkDeprecatedConstants(ph7_vm *pVm)` |
|       5 | 4007 | `{` |
|       - | 4008 | `	sxu32 n;` |
|   39338 | 4009 | `	for( n = 0 ; n < SX_ARRAYSIZE(aDeprecatedConst) ; ++n ){` |
|   67433 | 4010 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hConstant,` |
|   33714 | 4011 | `			(const void *)aDeprecatedConst[n].zName,` |
|   33714 | 4012 | `			SyStrlen(aDeprecatedConst[n].zName));` |
|   33719 | 4013 | `		if( pEntry ){` |
|   33719 | 4014 | `			((ph7_constant *)pEntry->pUserData)->zDeprecated = aDeprecatedConst[n].zWhy;` |
|   16830 | 4015 | `		}` |
|   16835 | 4016 | `	}` |
|    5624 | 4017 | `}` |
