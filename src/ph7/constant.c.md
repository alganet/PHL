# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2092/2109 lines (99.19%)

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
|     224 |   34 | `static void PH7_VER_Const(ph7_value *pVal,void *pUnused)` |
|       3 |   35 | `{` |
|     112 |   36 | `	SXUNUSED(pUnused);` |
|     227 |   37 | `	ph7_value_string(pVal,ph7_lib_signature(),-1/*Compute length automatically*/);` |
|     227 |   38 | `}` |
|       - |   39 | `/*` |
|       - |   40 | ` * PHP_VERSION, PHP_MAJOR_VERSION, PHP_MINOR_VERSION, PHP_RELEASE_VERSION,` |
|       - |   41 | ` * PHP_EXTRA_VERSION, PHP_VERSION_ID` |
|       - |   42 | ` *   Expand the PHP-compatibility version PHL advertises (see PHP_COMPAT_* in ph7.h).` |
|       - |   43 | ` */` |
|      86 |   44 | `static void PH7_PHPVerConst(ph7_value *pVal,void *pUnused)` |
|       5 |   45 | `{` |
|      43 |   46 | `	SXUNUSED(pUnused);` |
|      91 |   47 | `	ph7_value_string(pVal,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION)-1);` |
|      91 |   48 | `}` |
|      76 |   49 | `static void PH7_PHPMajorConst(ph7_value *pVal,void *pUnused)` |
|       3 |   50 | `{` |
|      38 |   51 | `	SXUNUSED(pUnused);` |
|      79 |   52 | `	ph7_value_int64(pVal,PHP_COMPAT_MAJOR_VERSION);` |
|      79 |   53 | `}` |
|      76 |   54 | `static void PH7_PHPMinorConst(ph7_value *pVal,void *pUnused)` |
|       3 |   55 | `{` |
|      38 |   56 | `	SXUNUSED(pUnused);` |
|      79 |   57 | `	ph7_value_int64(pVal,PHP_COMPAT_MINOR_VERSION);` |
|      79 |   58 | `}` |
|      76 |   59 | `static void PH7_PHPReleaseConst(ph7_value *pVal,void *pUnused)` |
|       3 |   60 | `{` |
|      38 |   61 | `	SXUNUSED(pUnused);` |
|      79 |   62 | `	ph7_value_int64(pVal,PHP_COMPAT_RELEASE_VERSION);` |
|      79 |   63 | `}` |
|      74 |   64 | `static void PH7_PHPExtraConst(ph7_value *pVal,void *pUnused)` |
|       3 |   65 | `{` |
|      37 |   66 | `	SXUNUSED(pUnused);` |
|      77 |   67 | `	ph7_value_string(pVal,PHP_COMPAT_EXTRA_VERSION,(int)sizeof(PHP_COMPAT_EXTRA_VERSION)-1);` |
|      77 |   68 | `}` |
|      76 |   69 | `static void PH7_PHPVerIdConst(ph7_value *pVal,void *pUnused)` |
|       3 |   70 | `{` |
|      38 |   71 | `	SXUNUSED(pUnused);` |
|      79 |   72 | `	ph7_value_int64(pVal,PHP_COMPAT_VERSION_ID);` |
|      79 |   73 | `}` |
|       - |   74 | `#ifdef __WINNT__` |
|       - |   75 | `#include <Windows.h>` |
|       - |   76 | `#elif defined(__UNIXES__)` |
|       - |   77 | `#include <sys/utsname.h>` |
|       - |   78 | `#endif` |
|       - |   79 | `/*` |
|       - |   80 | ` * PHP_OS` |
|       - |   81 | ` *  Expand the name of the host Operating System.` |
|       - |   82 | ` */` |
|    6834 |   83 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|       5 |   84 | `{` |
|       - |   85 | `#if defined(__WINNT__)` |
|       5 |   86 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|       - |   87 | `#elif defined(__UNIXES__)` |
|       - |   88 | `	struct utsname sInfo;` |
|    6834 |   89 | `	if( uname(&sInfo) != 0 ){` |
|     ! 0 |   90 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|     ! 0 |   91 | `	}else{` |
|    6834 |   92 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|       - |   93 | `	}` |
|       - |   94 | `#else` |
|       - |   95 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|       - |   96 | `#endif` |
|    3412 |   97 | `	SXUNUSED(pUnused);` |
|    6839 |   98 | `}` |
|       - |   99 | `/*` |
|       - |  100 | ` * PHP_OS_FAMILY (php 7.2)` |
|       - |  101 | ` *  One of 'Windows', 'BSD', 'Darwin', 'Solaris', 'Linux' or 'Unknown', derived` |
|       - |  102 | ` *  from the host's uname sysname (php maps the same set at build time).` |
|       - |  103 | ` */` |
|     137 |  104 | `static void PH7_OS_FAMILY_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  105 | `{` |
|      68 |  106 | `	SXUNUSED(pUnused);` |
|       - |  107 | `#if defined(__WINNT__)` |
|       5 |  108 | `	ph7_value_string(pVal,"Windows",(int)sizeof("Windows")-1);` |
|       - |  109 | `#elif defined(__UNIXES__)` |
|       - |  110 | `	struct utsname sInfo;` |
|     137 |  111 | `	const char *zFamily = "Unknown";` |
|     137 |  112 | `	if( uname(&sInfo) == 0 ){` |
|     137 |  113 | `		const char *z = sInfo.sysname;` |
|     137 |  114 | `		if( SyStrnicmp(z,"Darwin",sizeof("Darwin")-1) == 0 ){` |
|      68 |  115 | `			zFamily = "Darwin";` |
|     137 |  116 | `		}else if( SyStrnicmp(z,"Linux",sizeof("Linux")-1) == 0 ){` |
|      69 |  117 | `			zFamily = "Linux";` |
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
|      68 |  131 | `	}` |
|     137 |  132 | `	ph7_value_string(pVal,zFamily,-1);` |
|       - |  133 | `#else` |
|       - |  134 | `	ph7_value_string(pVal,"Unknown",(int)sizeof("Unknown")-1);` |
|       - |  135 | `#endif` |
|     142 |  136 | `}` |
|       - |  137 | `/*` |
|       - |  138 | ` * PHP_SAPI` |
|       - |  139 | ` *  The interface between the interpreter and the host. PHL's host binary is a` |
|       - |  140 | ` *  command-line interpreter, so this is "cli" (matching the CLI default of` |
|       - |  141 | ` *  php_sapi_name(); the built-in -S server's per-request "cli-server" flavour is` |
|       - |  142 | ` *  only surfaced by php_sapi_name(), not this compile-time constant).` |
|       - |  143 | ` */` |
|      74 |  144 | `static void PH7_SAPI_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  145 | `{` |
|      37 |  146 | `	SXUNUSED(pUnused);` |
|      77 |  147 | `	ph7_value_string(pVal,"cli",(int)sizeof("cli")-1);` |
|      77 |  148 | `}` |
|       - |  149 | `/*` |
|       - |  150 | ` * PHP_EOL` |
|       - |  151 | ` *  Expand the correct 'End Of Line' symbol for this platform.` |
|       - |  152 | ` */` |
|     916 |  153 | `static void PH7_EOL_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  154 | `{` |
|     458 |  155 | `	SXUNUSED(pUnused);` |
|       - |  156 | `#ifdef __WINNT__` |
|       4 |  157 | `	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);` |
|       - |  158 | `#else` |
|     916 |  159 | `	ph7_value_string(pVal,"\n",(int)sizeof(char));` |
|       - |  160 | `#endif` |
|     920 |  161 | `}` |
|       - |  162 | `/*` |
|       - |  163 | ` * PHP_INT_MAX` |
|       - |  164 | ` * Expand the largest integer supported.` |
|       - |  165 | ` * Note that PH7 deals with 64-bit integer for all platforms.` |
|       - |  166 | ` */` |
|    1804 |  167 | `static void PH7_INTMAX_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  168 | `{` |
|     901 |  169 | `	SXUNUSED(pUnused);` |
|    1809 |  170 | `	ph7_value_int64(pVal,SXI64_HIGH);` |
|    1809 |  171 | `}` |
|       - |  172 | `/*` |
|       - |  173 | ` * ext/calendar: the four calendars, numbered in the order the conversion table` |
|       - |  174 | ` * holds them, and CAL_NUM_CALS as their count -- which is what makes the` |
|       - |  175 | ` * "valid calendar ID" screen a plain 0 <= id < CAL_NUM_CALS test.` |
|       - |  176 | ` */` |
|     118 |  177 | `static void PH7_CAL_GREGORIAN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  178 | `{` |
|      59 |  179 | `	SXUNUSED(pUnused);` |
|     121 |  180 | `	ph7_value_int(pVal,0);` |
|     121 |  181 | `}` |
|      88 |  182 | `static void PH7_CAL_JULIAN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  183 | `{` |
|      44 |  184 | `	SXUNUSED(pUnused);` |
|      91 |  185 | `	ph7_value_int(pVal,1);` |
|      91 |  186 | `}` |
|     206 |  187 | `static void PH7_CAL_JEWISH_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  188 | `{` |
|     103 |  189 | `	SXUNUSED(pUnused);` |
|     209 |  190 | `	ph7_value_int(pVal,2);` |
|     209 |  191 | `}` |
|      98 |  192 | `static void PH7_CAL_FRENCH_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  193 | `{` |
|      49 |  194 | `	SXUNUSED(pUnused);` |
|     101 |  195 | `	ph7_value_int(pVal,3);` |
|     101 |  196 | `}` |
|      72 |  197 | `static void PH7_CAL_NUM_CALS_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  198 | `{` |
|      36 |  199 | `	SXUNUSED(pUnused);` |
|      75 |  200 | `	ph7_value_int(pVal,4);` |
|      75 |  201 | `}` |
|       - |  202 | `/*` |
|       - |  203 | ` * easter_days()/easter_date()'s $mode. DEFAULT is not a rule but a` |
|       - |  204 | ` * date-dependent choice between the two below it; ROMAN moves the 1583-1752` |
|       - |  205 | ` * window to the Gregorian rule, and the two ALWAYS_ modes pin one rule for` |
|       - |  206 | ` * every year.` |
|       - |  207 | ` */` |
|      92 |  208 | `static void PH7_CAL_EASTER_DEFAULT_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  209 | `{` |
|      46 |  210 | `	SXUNUSED(pUnused);` |
|      95 |  211 | `	ph7_value_int(pVal,0);` |
|      95 |  212 | `}` |
|      92 |  213 | `static void PH7_CAL_EASTER_ROMAN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  214 | `{` |
|      46 |  215 | `	SXUNUSED(pUnused);` |
|      95 |  216 | `	ph7_value_int(pVal,1);` |
|      95 |  217 | `}` |
|      92 |  218 | `static void PH7_CAL_EASTER_ALWAYS_GREGORIAN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  219 | `{` |
|      46 |  220 | `	SXUNUSED(pUnused);` |
|      95 |  221 | `	ph7_value_int(pVal,2);` |
|      95 |  222 | `}` |
|      96 |  223 | `static void PH7_CAL_EASTER_ALWAYS_JULIAN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  224 | `{` |
|      48 |  225 | `	SXUNUSED(pUnused);` |
|      99 |  226 | `	ph7_value_int(pVal,3);` |
|      99 |  227 | `}` |
|       - |  228 | `/* jddayofweek()'s three modes. Note that SHORT is 2 and LONG is 1: the numbers` |
|       - |  229 | ` * are not in the order the names suggest. */` |
|      72 |  230 | `static void PH7_CAL_DOW_DAYNO_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  231 | `{` |
|      36 |  232 | `	SXUNUSED(pUnused);` |
|      75 |  233 | `	ph7_value_int(pVal,0);` |
|      75 |  234 | `}` |
|     108 |  235 | `static void PH7_CAL_DOW_LONG_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  236 | `{` |
|      54 |  237 | `	SXUNUSED(pUnused);` |
|     111 |  238 | `	ph7_value_int(pVal,1);` |
|     111 |  239 | `}` |
|      98 |  240 | `static void PH7_CAL_DOW_SHORT_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  241 | `{` |
|      49 |  242 | `	SXUNUSED(pUnused);` |
|     101 |  243 | `	ph7_value_int(pVal,2);` |
|     101 |  244 | `}` |
|       - |  245 | `/* jdmonthname()'s six modes, which pick the CALENDAR as well as the spelling` |
|       - |  246 | ` * and are numbered independently of the CAL_* calendar ids above. */` |
|      74 |  247 | `static void PH7_CAL_MONTH_GREGORIAN_SHORT_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  248 | `{` |
|      37 |  249 | `	SXUNUSED(pUnused);` |
|      77 |  250 | `	ph7_value_int(pVal,0);` |
|      77 |  251 | `}` |
|      96 |  252 | `static void PH7_CAL_MONTH_GREGORIAN_LONG_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  253 | `{` |
|      48 |  254 | `	SXUNUSED(pUnused);` |
|      99 |  255 | `	ph7_value_int(pVal,1);` |
|      99 |  256 | `}` |
|      74 |  257 | `static void PH7_CAL_MONTH_JULIAN_SHORT_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  258 | `{` |
|      37 |  259 | `	SXUNUSED(pUnused);` |
|      77 |  260 | `	ph7_value_int(pVal,2);` |
|      77 |  261 | `}` |
|      74 |  262 | `static void PH7_CAL_MONTH_JULIAN_LONG_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  263 | `{` |
|      37 |  264 | `	SXUNUSED(pUnused);` |
|      77 |  265 | `	ph7_value_int(pVal,3);` |
|      77 |  266 | `}` |
|      80 |  267 | `static void PH7_CAL_MONTH_JEWISH_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  268 | `{` |
|      40 |  269 | `	SXUNUSED(pUnused);` |
|      83 |  270 | `	ph7_value_int(pVal,4);` |
|      83 |  271 | `}` |
|      74 |  272 | `static void PH7_CAL_MONTH_FRENCH_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  273 | `{` |
|      37 |  274 | `	SXUNUSED(pUnused);` |
|      77 |  275 | `	ph7_value_int(pVal,5);` |
|      77 |  276 | `}` |
|       - |  277 | `/*` |
|       - |  278 | ` * ext/calendar: the three flags jdtojewish()'s Hebrew spelling reads. They are` |
|       - |  279 | ` * a bit set, so a caller may ask for any combination of them.` |
|       - |  280 | ` */` |
|      76 |  281 | `static void PH7_CAL_JEWISH_ADD_ALAFIM_GERESH_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  282 | `{` |
|      38 |  283 | `	SXUNUSED(pUnused);` |
|      79 |  284 | `	ph7_value_int(pVal,2);` |
|      79 |  285 | `}` |
|      76 |  286 | `static void PH7_CAL_JEWISH_ADD_ALAFIM_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  287 | `{` |
|      38 |  288 | `	SXUNUSED(pUnused);` |
|      79 |  289 | `	ph7_value_int(pVal,4);` |
|      79 |  290 | `}` |
|      78 |  291 | `static void PH7_CAL_JEWISH_ADD_GERESHAYIM_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  292 | `{` |
|      39 |  293 | `	SXUNUSED(pUnused);` |
|      81 |  294 | `	ph7_value_int(pVal,8);` |
|      81 |  295 | `}` |
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
|      76 |  311 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_UNKNOWN_Const,   0)` |
|      90 |  312 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_GIF_Const,       1)` |
|      76 |  313 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JPEG_Const,      2)` |
|      76 |  314 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_PNG_Const,       3)` |
|      76 |  315 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_SWF_Const,       4)` |
|      76 |  316 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_PSD_Const,       5)` |
|      76 |  317 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_BMP_Const,       6)` |
|      76 |  318 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_TIFF_II_Const,   7)` |
|      76 |  319 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_TIFF_MM_Const,   8)` |
|     153 |  320 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JPC_Const,       9)` |
|      76 |  321 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JP2_Const,      10)` |
|      76 |  322 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JPX_Const,      11)` |
|      76 |  323 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JB2_Const,      12)` |
|      83 |  324 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_SWC_Const,      13)` |
|      76 |  325 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_IFF_Const,      14)` |
|      76 |  326 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_WBMP_Const,     15)` |
|      76 |  327 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_XBM_Const,      16)` |
|      76 |  328 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_ICO_Const,      17)` |
|      76 |  329 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_WEBP_Const,     18)` |
|      76 |  330 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_AVIF_Const,     19)` |
|      76 |  331 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_HEIF_Const,     20)` |
|       - |  332 | `#ifdef PH7_ENABLE_LIBXML` |
|      78 |  333 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_SVG_Const,      21)` |
|      78 |  334 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_COUNT_Const,    22)` |
|       - |  335 | `#else` |
|       - |  336 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_COUNT_Const,    21)` |
|       - |  337 | `#endif` |
|       - |  338 | `/*` |
|       - |  339 | ` * PHP_INT_MIN (php 7.0)` |
|       - |  340 | ` * Expand the smallest integer supported.` |
|       - |  341 | ` */` |
|     234 |  342 | `static void PH7_INTMIN_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  343 | `{` |
|     117 |  344 | `	SXUNUSED(pUnused);` |
|     238 |  345 | `	ph7_value_int64(pVal,SMALLEST_INT64);` |
|     238 |  346 | `}` |
|       - |  347 | `/*` |
|       - |  348 | ` * PHP_INT_SIZE` |
|       - |  349 | ` * Expand the size in bytes of a 64-bit integer.` |
|       - |  350 | ` */` |
|      94 |  351 | `static void PH7_INTSIZE_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  352 | `{` |
|      47 |  353 | `	SXUNUSED(pUnused);` |
|      98 |  354 | `	ph7_value_int64(pVal,sizeof(sxi64));` |
|      98 |  355 | `}` |
|       - |  356 | `/*` |
|       - |  357 | ` * PHP_FLOAT_EPSILON / PHP_FLOAT_MAX / PHP_FLOAT_MIN / PHP_FLOAT_DIG (php 7.2)` |
|       - |  358 | ` * Double-precision characteristics, sourced from <float.h> exactly like php` |
|       - |  359 | ` * so they track the compiling platform's actual double representation.` |
|       - |  360 | ` */` |
|      78 |  361 | `static void PH7_FLOATEPSILON_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  362 | `{` |
|      39 |  363 | `	SXUNUSED(pUnused);` |
|      81 |  364 | `	ph7_value_double(pVal,DBL_EPSILON);` |
|      81 |  365 | `}` |
|      78 |  366 | `static void PH7_FLOATMAX_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  367 | `{` |
|      39 |  368 | `	SXUNUSED(pUnused);` |
|      81 |  369 | `	ph7_value_double(pVal,DBL_MAX);` |
|      81 |  370 | `}` |
|      74 |  371 | `static void PH7_FLOATMIN_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  372 | `{` |
|      37 |  373 | `	SXUNUSED(pUnused);` |
|      77 |  374 | `	ph7_value_double(pVal,DBL_MIN);` |
|      77 |  375 | `}` |
|      76 |  376 | `static void PH7_FLOATDIG_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  377 | `{` |
|      38 |  378 | `	SXUNUSED(pUnused);` |
|      79 |  379 | `	ph7_value_int64(pVal,DBL_DIG);` |
|      79 |  380 | `}` |
|       - |  381 | `/*` |
|       - |  382 | ` * DIRECTORY_SEPARATOR.` |
|       - |  383 | ` * Expand the directory separator character.` |
|       - |  384 | ` */` |
|    2557 |  385 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  386 | `{` |
|    1276 |  387 | `	SXUNUSED(pUnused);` |
|       - |  388 | `#ifdef __WINNT__` |
|       5 |  389 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|       - |  390 | `#else` |
|    2557 |  391 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|       - |  392 | `#endif` |
|    2562 |  393 | `}` |
|       - |  394 | `/*` |
|       - |  395 | ` * PATH_SEPARATOR.` |
|       - |  396 | ` * Expand the path separator character.` |
|       - |  397 | ` */` |
|      79 |  398 | `static void PH7_PATHSEP_Const(ph7_value *pVal,void *pUnused)` |
|       4 |  399 | `{` |
|      39 |  400 | `	SXUNUSED(pUnused);` |
|       - |  401 | `#ifdef __WINNT__` |
|       4 |  402 | `	ph7_value_string(pVal,";",(int)sizeof(char));` |
|       - |  403 | `#else` |
|      79 |  404 | `	ph7_value_string(pVal,":",(int)sizeof(char));` |
|       - |  405 | `#endif` |
|      83 |  406 | `}` |
|       - |  407 |  |
|       - |  408 | `#if defined(PH7_ENABLE_MATH_FUNC)` |
|       - |  409 | `/*` |
|       - |  410 | ` * NAN constant: floating-point Not-A-Number` |
|       - |  411 | ` */` |
|     233 |  412 | `static void PH7_NAN_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  413 | `{` |
|     116 |  414 | `	SXUNUSED(pUnused);` |
|     238 |  415 | `	ph7_value_double(pVal, PH7_NAN_VALUE());` |
|     238 |  416 | `}` |
|       - |  417 |  |
|       - |  418 | `/*` |
|       - |  419 | ` * INF constant: positive infinity` |
|       - |  420 | ` */` |
|     253 |  421 | `static void PH7_INF_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  422 | `{` |
|     126 |  423 | `	SXUNUSED(pUnused);` |
|       - |  424 | `	/* similarly avoid the INFINITY macro */` |
|     258 |  425 | `	ph7_value_double(pVal, PH7_INF_VALUE());` |
|     258 |  426 | `}` |
|       - |  427 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|       - |  428 |  |
|       - |  429 | `#ifndef __WINNT__` |
|       - |  430 | `#include <time.h>` |
|       - |  431 | `#endif` |
|       - |  432 | `/*` |
|       - |  433 | ` * __TIME__` |
|       - |  434 | ` *  Expand the current time (GMT).` |
|       - |  435 | ` */` |
|      74 |  436 | `static void PH7_TIME_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  437 | `{` |
|       - |  438 | `	Sytm sTm;` |
|       - |  439 | `#ifdef __WINNT__` |
|       - |  440 | `	SYSTEMTIME sOS;` |
|       3 |  441 | `	GetSystemTime(&sOS);` |
|       3 |  442 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|       - |  443 | `#else` |
|       - |  444 | `	struct tm *pTm;` |
|       - |  445 | `	time_t t;` |
|      74 |  446 | `	time(&t);` |
|      74 |  447 | `	pTm = gmtime(&t);` |
|      74 |  448 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|       - |  449 | `#endif` |
|      37 |  450 | `	SXUNUSED(pUnused); /* cc warning */` |
|       - |  451 | `	/* Expand */` |
|      77 |  452 | `	ph7_value_string_format(pVal,"%02d:%02d:%02d",sTm.tm_hour,sTm.tm_min,sTm.tm_sec);` |
|      77 |  453 | `}` |
|       - |  454 | `/*` |
|       - |  455 | ` * __DATE__` |
|       - |  456 | ` *  Expand the current date in the ISO-8601 format.` |
|       - |  457 | ` */` |
|      74 |  458 | `static void PH7_DATE_Const(ph7_value *pVal,void *pUnused)` |
|       3 |  459 | `{` |
|       - |  460 | `	Sytm sTm;` |
|       - |  461 | `#ifdef __WINNT__` |
|       - |  462 | `	SYSTEMTIME sOS;` |
|       3 |  463 | `	GetSystemTime(&sOS);` |
|       3 |  464 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|       - |  465 | `#else` |
|       - |  466 | `	struct tm *pTm;` |
|       - |  467 | `	time_t t;` |
|      74 |  468 | `	time(&t);` |
|      74 |  469 | `	pTm = gmtime(&t);` |
|      74 |  470 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|       - |  471 | `#endif` |
|      37 |  472 | `	SXUNUSED(pUnused); /* cc warning */` |
|       - |  473 | `	/* Expand */` |
|      77 |  474 | `	ph7_value_string_format(pVal,"%04qd-%02d-%02d",sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|      77 |  475 | `}` |
|       - |  476 | `/*` |
|       - |  477 | ` * __FILE__` |
|       - |  478 | ` *  Path of the processed script.` |
|       - |  479 | ` */` |
|      72 |  480 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  481 | `{` |
|      75 |  482 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - |  483 | `	SyString *pFile;` |
|       - |  484 | `	/* The unit the LITERAL is written in, which php fixes at compile time: the` |
|       - |  485 | `	 * declared file of the function running, else the unit on top of the include` |
|       - |  486 | `	 * stack. Reading the stack top alone answered the unit currently being LOADED,` |
|       - |  487 | `	 * so a function defined in one file and called from an include (or from an` |
|       - |  488 | `	 * eval()'d chunk, whose own name is now such an entry) reported the caller's` |
|       - |  489 | `	 * file as its own. */` |
|      75 |  490 | `	pFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|      75 |  491 | `	if( pFile == 0 ){` |
|       - |  492 | `		/* Expand the magic word: ":MEMORY:" */` |
|     ! 0 |  493 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|     ! 0 |  494 | `	}else{` |
|      75 |  495 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|       - |  496 | `	}` |
|      75 |  497 | `}` |
|       - |  498 | `/*` |
|       - |  499 | ` * __DIR__` |
|       - |  500 | ` *  Directory holding the processed script.` |
|       - |  501 | ` */` |
|      72 |  502 | `static void PH7_DIR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  503 | `{` |
|      75 |  504 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - |  505 | `	SyString *pFile;` |
|       - |  506 | `	/* Same question as __FILE__, one directory up. */` |
|      75 |  507 | `	pFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|      75 |  508 | `	if( pFile == 0 ){` |
|       - |  509 | `		/* Expand the magic word: ":MEMORY:" */` |
|     ! 0 |  510 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|     ! 0 |  511 | `	}else{` |
|      75 |  512 | `		if( pFile->nByte > 0 ){` |
|       - |  513 | `			const char *zDir;` |
|       - |  514 | `			int nLen;` |
|      75 |  515 | `			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|      75 |  516 | `			ph7_value_string(pVal,zDir,nLen);` |
|      39 |  517 | `		}else{` |
|       - |  518 | `			/* Expand '.' as the current directory*/` |
|     ! 0 |  519 | `			ph7_value_string(pVal,".",(int)sizeof(char));` |
|       - |  520 | `		}` |
|       - |  521 | `	}` |
|      75 |  522 | `}` |
|       - |  523 | `/*` |
|       - |  524 | ` * PHP_SHLIB_SUFFIX` |
|       - |  525 | ` *  Expand shared library suffix.` |
|       - |  526 | ` */` |
|      74 |  527 | `static void PH7_PHP_SHLIB_SUFFIX_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  528 | `{` |
|       - |  529 | `#ifdef __WINNT__` |
|       3 |  530 | `	ph7_value_string(pVal,"dll",(int)sizeof("dll")-1);` |
|       - |  531 | `#else` |
|      74 |  532 | `	ph7_value_string(pVal,"so",(int)sizeof("so")-1);` |
|       - |  533 | `#endif` |
|      37 |  534 | `	SXUNUSED(pUserData); /* cc warning */` |
|      77 |  535 | `}` |
|       - |  536 | `/*` |
|       - |  537 | ` * E_ERROR` |
|       - |  538 | ` *  Expands 1` |
|       - |  539 | ` */` |
|      78 |  540 | `static void PH7_E_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  541 | `{` |
|      81 |  542 | `	ph7_value_int(pVal,1);` |
|      39 |  543 | `	SXUNUSED(pUserData);` |
|      81 |  544 | `}` |
|       - |  545 | `/*` |
|       - |  546 | ` * E_WARNING` |
|       - |  547 | ` *  Expands 2` |
|       - |  548 | ` */` |
|      96 |  549 | `static void PH7_E_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  550 | `{` |
|     101 |  551 | `	ph7_value_int(pVal,2);` |
|      48 |  552 | `	SXUNUSED(pUserData);` |
|     101 |  553 | `}` |
|       - |  554 | `/*` |
|       - |  555 | ` * E_PARSE` |
|       - |  556 | ` *  Expands 4` |
|       - |  557 | ` */` |
|      74 |  558 | `static void PH7_E_PARSE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  559 | `{` |
|      77 |  560 | `	ph7_value_int(pVal,4);` |
|      37 |  561 | `	SXUNUSED(pUserData);` |
|      77 |  562 | `}` |
|       - |  563 | `/*` |
|       - |  564 | ` * E_NOTICE` |
|       - |  565 | ` * Expands 8` |
|       - |  566 | ` */` |
|      88 |  567 | `static void PH7_E_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  568 | `{` |
|      92 |  569 | `	ph7_value_int(pVal,8);` |
|      44 |  570 | `	SXUNUSED(pUserData);` |
|      92 |  571 | `}` |
|       - |  572 | `/*` |
|       - |  573 | ` * E_CORE_ERROR` |
|       - |  574 | ` * Expands 16` |
|       - |  575 | ` */` |
|      74 |  576 | `static void PH7_E_CORE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  577 | `{` |
|      77 |  578 | `	ph7_value_int(pVal,16);` |
|      37 |  579 | `	SXUNUSED(pUserData);` |
|      77 |  580 | `}` |
|       - |  581 | `/*` |
|       - |  582 | ` * E_CORE_WARNING` |
|       - |  583 | ` * Expands 32` |
|       - |  584 | ` */` |
|      74 |  585 | `static void PH7_E_CORE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  586 | `{` |
|      77 |  587 | `	ph7_value_int(pVal,32);` |
|      37 |  588 | `	SXUNUSED(pUserData);` |
|      77 |  589 | `}` |
|       - |  590 | `/*` |
|       - |  591 | ` * E_COMPILE_ERROR` |
|       - |  592 | ` * Expands 64` |
|       - |  593 | ` */` |
|      74 |  594 | `static void PH7_E_COMPILE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  595 | `{` |
|      77 |  596 | `	ph7_value_int(pVal,64);` |
|      37 |  597 | `	SXUNUSED(pUserData);` |
|      77 |  598 | `}` |
|       - |  599 | `/*` |
|       - |  600 | ` * E_COMPILE_WARNING` |
|       - |  601 | ` * Expands 128` |
|       - |  602 | ` */` |
|      82 |  603 | `static void PH7_E_COMPILE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  604 | `{` |
|      85 |  605 | `	ph7_value_int(pVal,128);` |
|      41 |  606 | `	SXUNUSED(pUserData);` |
|      85 |  607 | `}` |
|       - |  608 | `/*` |
|       - |  609 | ` * E_USER_ERROR` |
|       - |  610 | ` * Expands 256` |
|       - |  611 | ` */` |
|      84 |  612 | `static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  613 | `{` |
|      88 |  614 | `	ph7_value_int(pVal,256);` |
|      42 |  615 | `	SXUNUSED(pUserData);` |
|      88 |  616 | `}` |
|       - |  617 | `/*` |
|       - |  618 | ` * E_USER_WARNING` |
|       - |  619 | ` * Expands 512` |
|       - |  620 | ` */` |
|     128 |  621 | `static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  622 | `{` |
|     133 |  623 | `	ph7_value_int(pVal,512);` |
|      64 |  624 | `	SXUNUSED(pUserData);` |
|     133 |  625 | `}` |
|       - |  626 | `/*` |
|       - |  627 | ` * E_USER_NOTICE` |
|       - |  628 | ` * Expands 1024` |
|       - |  629 | ` */` |
|     168 |  630 | `static void PH7_E_USER_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  631 | `{` |
|     173 |  632 | `	ph7_value_int(pVal,1024);` |
|      84 |  633 | `	SXUNUSED(pUserData);` |
|     173 |  634 | `}` |
|       - |  635 | `/*` |
|       - |  636 | ` * E_RECOVERABLE_ERROR` |
|       - |  637 | ` * Expands 4096` |
|       - |  638 | ` */` |
|      74 |  639 | `static void PH7_E_RECOVERABLE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  640 | `{` |
|      77 |  641 | `	ph7_value_int(pVal,4096);` |
|      37 |  642 | `	SXUNUSED(pUserData);` |
|      77 |  643 | `}` |
|       - |  644 | `/*` |
|       - |  645 | ` * E_STRICT` |
|       - |  646 | ` * Expands 2048. php 8.4 removed the error LEVEL but kept the constant, marked` |
|       - |  647 | ` * deprecated -- a program may still name it (and still gets 2048), it just says` |
|       - |  648 | ` * so. It is deliberately NOT part of E_ALL, which is why PH7_E_ALL_MASK is` |
|       - |  649 | ` * 30719 and not 32767.` |
|       - |  650 | ` */` |
|      82 |  651 | `static void PH7_E_STRICT_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  652 | `{` |
|      85 |  653 | `	ph7_value_int(pVal,2048);` |
|      41 |  654 | `	SXUNUSED(pUserData);` |
|      85 |  655 | `}` |
|       - |  656 | `/*` |
|       - |  657 | ` * E_DEPRECATED` |
|       - |  658 | ` * Expands 8192` |
|       - |  659 | ` */` |
|     116 |  660 | `static void PH7_E_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  661 | `{` |
|     121 |  662 | `	ph7_value_int(pVal,8192);` |
|      58 |  663 | `	SXUNUSED(pUserData);` |
|     121 |  664 | `}` |
|       - |  665 | `/*` |
|       - |  666 | ` * E_USER_DEPRECATED` |
|       - |  667 | ` *   Expands 16384.` |
|       - |  668 | ` */` |
|      88 |  669 | `static void PH7_E_USER_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  670 | `{` |
|      92 |  671 | `	ph7_value_int(pVal,16384);` |
|      44 |  672 | `	SXUNUSED(pUserData);` |
|      92 |  673 | `}` |
|       - |  674 | `/*` |
|       - |  675 | ` * E_ALL` |
|       - |  676 | ` *  Expands 30719 (php 8: E_STRICT is no longer part of E_ALL)` |
|       - |  677 | ` */` |
|     225 |  678 | `static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  679 | `{` |
|     230 |  680 | `	ph7_value_int(pVal,PH7_E_ALL_MASK);` |
|     112 |  681 | `	SXUNUSED(pUserData);` |
|     230 |  682 | `}` |
|       - |  683 | `/*` |
|       - |  684 | ` * CASE_LOWER` |
|       - |  685 | ` *  Expands 0.` |
|       - |  686 | ` */` |
|      75 |  687 | `static void PH7_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  688 | `{` |
|      78 |  689 | `	ph7_value_int(pVal,0);` |
|      37 |  690 | `	SXUNUSED(pUserData);` |
|      78 |  691 | `}` |
|       - |  692 | `/*` |
|       - |  693 | ` * CASE_UPPER` |
|       - |  694 | ` *  Expands 1.` |
|       - |  695 | ` */` |
|      81 |  696 | `static void PH7_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  697 | `{` |
|      84 |  698 | `	ph7_value_int(pVal,1);` |
|      40 |  699 | `	SXUNUSED(pUserData);` |
|      84 |  700 | `}` |
|       - |  701 | `/*` |
|       - |  702 | ` * STR_PAD_LEFT` |
|       - |  703 | ` *  Expands 0.` |
|       - |  704 | ` */` |
|     107 |  705 | `static void PH7_STR_PAD_LEFT_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  706 | `{` |
|     110 |  707 | `	ph7_value_int(pVal,0);` |
|      53 |  708 | `	SXUNUSED(pUserData);` |
|     110 |  709 | `}` |
|       - |  710 | `/*` |
|       - |  711 | ` * STR_PAD_RIGHT` |
|       - |  712 | ` *  Expands 1.` |
|       - |  713 | ` */` |
|      87 |  714 | `static void PH7_STR_PAD_RIGHT_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  715 | `{` |
|      91 |  716 | `	ph7_value_int(pVal,1);` |
|      43 |  717 | `	SXUNUSED(pUserData);` |
|      91 |  718 | `}` |
|       - |  719 | `/*` |
|       - |  720 | ` * STR_PAD_BOTH` |
|       - |  721 | ` *  Expands 2.` |
|       - |  722 | ` */` |
|      77 |  723 | `static void PH7_STR_PAD_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  724 | `{` |
|      80 |  725 | `	ph7_value_int(pVal,2);` |
|      38 |  726 | `	SXUNUSED(pUserData);` |
|      80 |  727 | `}` |
|       - |  728 | `/*` |
|       - |  729 | ` * stream_wrapper_register()'s $flags. php defines exactly this one bit: the` |
|       - |  730 | ` * wrapper speaks to the network, so allow_url_fopen gates opening it and` |
|       - |  731 | ` * allow_url_include gates INCLUDING it.` |
|       - |  732 | ` */` |
|      77 |  733 | `static void PH7_STREAM_IS_URL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  734 | `{` |
|      80 |  735 | `	ph7_value_int(pVal,PH7_STREAM_IS_URL);` |
|      38 |  736 | `	SXUNUSED(pUserData);` |
|      80 |  737 | `}` |
|       - |  738 | `/*` |
|       - |  739 | ` * The rest of the streamWrapper PROTOCOL vocabulary: the numbers php hands a` |
|       - |  740 | ` * userland wrapper, and the ones a wrapper hands back. PHL registered wrappers` |
|       - |  741 | ` * without them, so the ordinary spellings every real wrapper is written against --` |
|       - |  742 | ``  * `$options & STREAM_USE_PATH` in stream_open(), `$flags & STREAM_URL_STAT_QUIET` `` |
|       - |  743 | ` * in url_stat(), the STREAM_META_* verb in stream_metadata() -- were an undefined` |
|       - |  744 | ` * constant, i.e. an Error, in code php runs.` |
|       - |  745 | ` */` |
|      70 |  746 | `static void PH7_STREAM_USE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  747 | `{` |
|      73 |  748 | `	ph7_value_int(pVal,PH7_STREAM_USE_PATH);` |
|      35 |  749 | `	SXUNUSED(pUserData);` |
|      73 |  750 | `}` |
|      70 |  751 | `static void PH7_STREAM_IGNORE_URL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  752 | `{` |
|      73 |  753 | `	ph7_value_int(pVal,PH7_STREAM_IGNORE_URL);` |
|      35 |  754 | `	SXUNUSED(pUserData);` |
|      73 |  755 | `}` |
|      70 |  756 | `static void PH7_STREAM_REPORT_ERRORS_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  757 | `{` |
|      73 |  758 | `	ph7_value_int(pVal,PH7_STREAM_REPORT_ERRORS);` |
|      35 |  759 | `	SXUNUSED(pUserData);` |
|      73 |  760 | `}` |
|      70 |  761 | `static void PH7_STREAM_MUST_SEEK_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  762 | `{` |
|      73 |  763 | `	ph7_value_int(pVal,PH7_STREAM_MUST_SEEK);` |
|      35 |  764 | `	SXUNUSED(pUserData);` |
|      73 |  765 | `}` |
|      70 |  766 | `static void PH7_STREAM_URL_STAT_LINK_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  767 | `{` |
|      73 |  768 | `	ph7_value_int(pVal,PH7_URL_STAT_LINK);` |
|      35 |  769 | `	SXUNUSED(pUserData);` |
|      73 |  770 | `}` |
|      70 |  771 | `static void PH7_STREAM_URL_STAT_QUIET_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  772 | `{` |
|      73 |  773 | `	ph7_value_int(pVal,PH7_URL_STAT_QUIET);` |
|      35 |  774 | `	SXUNUSED(pUserData);` |
|      73 |  775 | `}` |
|      70 |  776 | `static void PH7_STREAM_MKDIR_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  777 | `{` |
|      73 |  778 | `	ph7_value_int(pVal,PH7_STREAM_MKDIR_RECURSIVE);` |
|      35 |  779 | `	SXUNUSED(pUserData);` |
|      73 |  780 | `}` |
|      70 |  781 | `static void PH7_STREAM_META_TOUCH_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  782 | `{` |
|      73 |  783 | `	ph7_value_int(pVal,PH7_STREAM_META_TOUCH);` |
|      35 |  784 | `	SXUNUSED(pUserData);` |
|      73 |  785 | `}` |
|      70 |  786 | `static void PH7_STREAM_META_OWNER_NAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  787 | `{` |
|      73 |  788 | `	ph7_value_int(pVal,PH7_STREAM_META_OWNER_NAME);` |
|      35 |  789 | `	SXUNUSED(pUserData);` |
|      73 |  790 | `}` |
|      70 |  791 | `static void PH7_STREAM_META_OWNER_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  792 | `{` |
|      73 |  793 | `	ph7_value_int(pVal,PH7_STREAM_META_OWNER);` |
|      35 |  794 | `	SXUNUSED(pUserData);` |
|      73 |  795 | `}` |
|      70 |  796 | `static void PH7_STREAM_META_GROUP_NAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  797 | `{` |
|      73 |  798 | `	ph7_value_int(pVal,PH7_STREAM_META_GROUP_NAME);` |
|      35 |  799 | `	SXUNUSED(pUserData);` |
|      73 |  800 | `}` |
|      70 |  801 | `static void PH7_STREAM_META_GROUP_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  802 | `{` |
|      73 |  803 | `	ph7_value_int(pVal,PH7_STREAM_META_GROUP);` |
|      35 |  804 | `	SXUNUSED(pUserData);` |
|      73 |  805 | `}` |
|      70 |  806 | `static void PH7_STREAM_META_ACCESS_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  807 | `{` |
|      73 |  808 | `	ph7_value_int(pVal,PH7_STREAM_META_ACCESS);` |
|      35 |  809 | `	SXUNUSED(pUserData);` |
|      73 |  810 | `}` |
|      70 |  811 | `static void PH7_STREAM_OPTION_BLOCKING_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  812 | `{` |
|      73 |  813 | `	ph7_value_int(pVal,PH7_STREAM_OPTION_BLOCKING);` |
|      35 |  814 | `	SXUNUSED(pUserData);` |
|      73 |  815 | `}` |
|      70 |  816 | `static void PH7_STREAM_OPTION_READ_BUFFER_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  817 | `{` |
|      73 |  818 | `	ph7_value_int(pVal,PH7_STREAM_OPTION_READ_BUFFER);` |
|      35 |  819 | `	SXUNUSED(pUserData);` |
|      73 |  820 | `}` |
|      70 |  821 | `static void PH7_STREAM_OPTION_WRITE_BUFFER_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  822 | `{` |
|      73 |  823 | `	ph7_value_int(pVal,PH7_STREAM_OPTION_WRITE_BUFFER);` |
|      35 |  824 | `	SXUNUSED(pUserData);` |
|      73 |  825 | `}` |
|      70 |  826 | `static void PH7_STREAM_OPTION_READ_TIMEOUT_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  827 | `{` |
|      73 |  828 | `	ph7_value_int(pVal,PH7_STREAM_OPTION_READ_TIMEOUT);` |
|      35 |  829 | `	SXUNUSED(pUserData);` |
|      73 |  830 | `}` |
|      70 |  831 | `static void PH7_STREAM_BUFFER_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  832 | `{` |
|      73 |  833 | `	ph7_value_int(pVal,PH7_STREAM_BUFFER_NONE);` |
|      35 |  834 | `	SXUNUSED(pUserData);` |
|      73 |  835 | `}` |
|      70 |  836 | `static void PH7_STREAM_BUFFER_LINE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  837 | `{` |
|      73 |  838 | `	ph7_value_int(pVal,PH7_STREAM_BUFFER_LINE);` |
|      35 |  839 | `	SXUNUSED(pUserData);` |
|      73 |  840 | `}` |
|      70 |  841 | `static void PH7_STREAM_BUFFER_FULL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  842 | `{` |
|      73 |  843 | `	ph7_value_int(pVal,PH7_STREAM_BUFFER_FULL);` |
|      35 |  844 | `	SXUNUSED(pUserData);` |
|      73 |  845 | `}` |
|      70 |  846 | `static void PH7_STREAM_CAST_AS_STREAM_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  847 | `{` |
|      73 |  848 | `	ph7_value_int(pVal,PH7_STREAM_CAST_AS_STREAM);` |
|      35 |  849 | `	SXUNUSED(pUserData);` |
|      73 |  850 | `}` |
|      70 |  851 | `static void PH7_STREAM_CAST_FOR_SELECT_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  852 | `{` |
|      73 |  853 | `	ph7_value_int(pVal,PH7_STREAM_CAST_FOR_SELECT);` |
|      35 |  854 | `	SXUNUSED(pUserData);` |
|      73 |  855 | `}` |
|       - |  856 | `/*` |
|       - |  857 | ` * A userland filter's ANSWER, and which kind of call it is answering. FEED_ME` |
|       - |  858 | ` * says "I produced nothing, ask me again with more"; ERR_FATAL ends the stream.` |
|       - |  859 | ` */` |
|     109 |  860 | `static void PH7_PSFS_PASS_ON_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  861 | `{` |
|     112 |  862 | `	ph7_value_int(pVal,PHL_PSFS_PASS_ON);` |
|      54 |  863 | `	SXUNUSED(pUserData);` |
|     112 |  864 | `}` |
|      77 |  865 | `static void PH7_PSFS_FEED_ME_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  866 | `{` |
|      80 |  867 | `	ph7_value_int(pVal,PHL_PSFS_FEED_ME);` |
|      38 |  868 | `	SXUNUSED(pUserData);` |
|      80 |  869 | `}` |
|      75 |  870 | `static void PH7_PSFS_ERR_FATAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  871 | `{` |
|      78 |  872 | `	ph7_value_int(pVal,PHL_PSFS_ERR_FATAL);` |
|      37 |  873 | `	SXUNUSED(pUserData);` |
|      78 |  874 | `}` |
|      73 |  875 | `static void PH7_PSFS_FLAG_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  876 | `{` |
|      76 |  877 | `	ph7_value_int(pVal,PHL_PSFS_FLAG_NORMAL);` |
|      36 |  878 | `	SXUNUSED(pUserData);` |
|      76 |  879 | `}` |
|      73 |  880 | `static void PH7_PSFS_FLAG_FLUSH_INC_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  881 | `{` |
|      76 |  882 | `	ph7_value_int(pVal,PHL_PSFS_FLAG_FLUSH_INC);` |
|      36 |  883 | `	SXUNUSED(pUserData);` |
|      76 |  884 | `}` |
|      73 |  885 | `static void PH7_PSFS_FLAG_FLUSH_CLOSE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  886 | `{` |
|      76 |  887 | `	ph7_value_int(pVal,PHL_PSFS_FLAG_FLUSH_CLOSE);` |
|      36 |  888 | `	SXUNUSED(pUserData);` |
|      76 |  889 | `}` |
|       - |  890 | `/*` |
|       - |  891 | `` * The `notification` callback's first argument: WHICH event the stream layer is`` |
|       - |  892 | ` * reporting. php defines all ten whatever its build registered, so a script may` |
|       - |  893 | `` * name one no wrapper here raises -- an unmatched `case` is silent where a`` |
|       - |  894 | ` * missing constant is a fatal.` |
|       - |  895 | ` */` |
|      70 |  896 | `static void PH7_STREAM_NOTIFY_RESOLVE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  897 | `{` |
|      73 |  898 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_RESOLVE);` |
|      35 |  899 | `	SXUNUSED(pUserData);` |
|      73 |  900 | `}` |
|     166 |  901 | `static void PH7_STREAM_NOTIFY_CONNECT_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  902 | `{` |
|     169 |  903 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_CONNECT);` |
|      83 |  904 | `	SXUNUSED(pUserData);` |
|     169 |  905 | `}` |
|      70 |  906 | `static void PH7_STREAM_NOTIFY_AUTH_REQUIRED_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  907 | `{` |
|      73 |  908 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_AUTH_REQUIRED);` |
|      35 |  909 | `	SXUNUSED(pUserData);` |
|      73 |  910 | `}` |
|      70 |  911 | `static void PH7_STREAM_NOTIFY_MIME_TYPE_IS_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  912 | `{` |
|      73 |  913 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_MIME_TYPE_IS);` |
|      35 |  914 | `	SXUNUSED(pUserData);` |
|      73 |  915 | `}` |
|      70 |  916 | `static void PH7_STREAM_NOTIFY_FILE_SIZE_IS_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  917 | `{` |
|      73 |  918 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_FILE_SIZE_IS);` |
|      35 |  919 | `	SXUNUSED(pUserData);` |
|      73 |  920 | `}` |
|      70 |  921 | `static void PH7_STREAM_NOTIFY_REDIRECTED_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  922 | `{` |
|      73 |  923 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_REDIRECTED);` |
|      35 |  924 | `	SXUNUSED(pUserData);` |
|      73 |  925 | `}` |
|      70 |  926 | `static void PH7_STREAM_NOTIFY_PROGRESS_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  927 | `{` |
|      73 |  928 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_PROGRESS);` |
|      35 |  929 | `	SXUNUSED(pUserData);` |
|      73 |  930 | `}` |
|      70 |  931 | `static void PH7_STREAM_NOTIFY_COMPLETED_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  932 | `{` |
|      73 |  933 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_COMPLETED);` |
|      35 |  934 | `	SXUNUSED(pUserData);` |
|      73 |  935 | `}` |
|      70 |  936 | `static void PH7_STREAM_NOTIFY_FAILURE_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  937 | `{` |
|      73 |  938 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_FAILURE);` |
|      35 |  939 | `	SXUNUSED(pUserData);` |
|      73 |  940 | `}` |
|      70 |  941 | `static void PH7_STREAM_NOTIFY_AUTH_RESULT_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  942 | `{` |
|      73 |  943 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_AUTH_RESULT);` |
|      35 |  944 | `	SXUNUSED(pUserData);` |
|      73 |  945 | `}` |
|       - |  946 | `/* And the callback's second argument: how bad the event is. */` |
|      70 |  947 | `static void PH7_STREAM_NOTIFY_SEVERITY_INFO_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  948 | `{` |
|      73 |  949 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_SEVERITY_INFO);` |
|      35 |  950 | `	SXUNUSED(pUserData);` |
|      73 |  951 | `}` |
|      70 |  952 | `static void PH7_STREAM_NOTIFY_SEVERITY_WARN_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  953 | `{` |
|      73 |  954 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_SEVERITY_WARN);` |
|      35 |  955 | `	SXUNUSED(pUserData);` |
|      73 |  956 | `}` |
|      70 |  957 | `static void PH7_STREAM_NOTIFY_SEVERITY_ERR_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  958 | `{` |
|      73 |  959 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_SEVERITY_ERR);` |
|      35 |  960 | `	SXUNUSED(pUserData);` |
|      73 |  961 | `}` |
|       - |  962 | `/*` |
|       - |  963 | ` * stream_filter_append()'s $mode — WHICH chain the filter joins. php's 0 is not` |
|       - |  964 | ` * "neither": it means "whichever chains the handle's own mode makes sense for".` |
|       - |  965 | ` */` |
|     205 |  966 | `static void PH7_STREAM_FILTER_READ_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  967 | `{` |
|     209 |  968 | `	ph7_value_int(pVal,PHL_STREAM_FILTER_READ);` |
|     102 |  969 | `	SXUNUSED(pUserData);` |
|     209 |  970 | `}` |
|   10119 |  971 | `static void PH7_STREAM_FILTER_WRITE_Const(ph7_value *pVal,void *pUserData)` |
|       4 |  972 | `{` |
|   10123 |  973 | `	ph7_value_int(pVal,PHL_STREAM_FILTER_WRITE);` |
|    5059 |  974 | `	SXUNUSED(pUserData);` |
|   10123 |  975 | `}` |
|      73 |  976 | `static void PH7_STREAM_FILTER_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       3 |  977 | `{` |
|      76 |  978 | `	ph7_value_int(pVal,PHL_STREAM_FILTER_ALL);` |
|      36 |  979 | `	SXUNUSED(pUserData);` |
|      76 |  980 | `}` |
|       - |  981 | `/*` |
|       - |  982 | ` * stream_socket_client()'s $flags. CONNECT is the default it documents;` |
|       - |  983 | ` * PERSISTENT is what pfsockopen() means and the only one that changes what a` |
|       - |  984 | ` * second call to the same address ANSWERS.` |
|       - |  985 | ` */` |
|     159 |  986 | `static void PH7_STREAM_CLIENT_CONNECT_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  987 | `{` |
|     164 |  988 | `	ph7_value_int(pVal,PH7_STREAM_CLIENT_CONNECT);` |
|      79 |  989 | `	SXUNUSED(pUserData);` |
|     164 |  990 | `}` |
|      77 |  991 | `static void PH7_STREAM_CLIENT_ASYNC_CONNECT_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  992 | `{` |
|      82 |  993 | `	ph7_value_int(pVal,PH7_STREAM_CLIENT_ASYNC_CONNECT);` |
|      38 |  994 | `	SXUNUSED(pUserData);` |
|      82 |  995 | `}` |
|      85 |  996 | `static void PH7_STREAM_CLIENT_PERSISTENT_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  997 | `{` |
|      90 |  998 | `	ph7_value_int(pVal,PH7_STREAM_CLIENT_PERSISTENT);` |
|      42 |  999 | `	SXUNUSED(pUserData);` |
|      90 | 1000 | `}` |
|       - | 1001 | `/*` |
|       - | 1002 | ` * The socket-family constants. Their VALUES are the platform's own — AF_INET6 is` |
|       - | 1003 | ` * 10 on Linux, 23 on Windows and 30 on the BSDs — so they are asked for by id` |
|       - | 1004 | ` * rather than written down here, and a program handing one to` |
|       - | 1005 | ` * stream_socket_pair() is handing the OS its own number.` |
|       - | 1006 | ` */` |
|       - | 1007 | `#ifdef PH7_ENABLE_NET` |
|      71 | 1008 | `static void PH7_STREAM_PF_INET_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1009 | `{` |
|      75 | 1010 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_INET));` |
|      35 | 1011 | `	SXUNUSED(pUserData);` |
|      75 | 1012 | `}` |
|      71 | 1013 | `static void PH7_STREAM_PF_INET6_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1014 | `{` |
|      74 | 1015 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_INET6));` |
|      35 | 1016 | `	SXUNUSED(pUserData);` |
|      74 | 1017 | `}` |
|      77 | 1018 | `static void PH7_STREAM_PF_UNIX_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1019 | `{` |
|      81 | 1020 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_UNIX));` |
|      38 | 1021 | `	SXUNUSED(pUserData);` |
|      81 | 1022 | `}` |
|      75 | 1023 | `static void PH7_STREAM_SOCK_STREAM_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1024 | `{` |
|      79 | 1025 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_STREAM));` |
|      37 | 1026 | `	SXUNUSED(pUserData);` |
|      79 | 1027 | `}` |
|      71 | 1028 | `static void PH7_STREAM_SOCK_DGRAM_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1029 | `{` |
|      74 | 1030 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_DGRAM));` |
|      35 | 1031 | `	SXUNUSED(pUserData);` |
|      74 | 1032 | `}` |
|      71 | 1033 | `static void PH7_STREAM_SOCK_RAW_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1034 | `{` |
|      74 | 1035 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_RAW));` |
|      35 | 1036 | `	SXUNUSED(pUserData);` |
|      74 | 1037 | `}` |
|      71 | 1038 | `static void PH7_STREAM_SOCK_SEQPACKET_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1039 | `{` |
|      74 | 1040 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_SEQPACKET));` |
|      35 | 1041 | `	SXUNUSED(pUserData);` |
|      74 | 1042 | `}` |
|      71 | 1043 | `static void PH7_STREAM_SOCK_RDM_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1044 | `{` |
|      74 | 1045 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_RDM));` |
|      35 | 1046 | `	SXUNUSED(pUserData);` |
|      74 | 1047 | `}` |
|      71 | 1048 | `static void PH7_STREAM_IPPROTO_IP_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1049 | `{` |
|      75 | 1050 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_IP));` |
|      35 | 1051 | `	SXUNUSED(pUserData);` |
|      75 | 1052 | `}` |
|      71 | 1053 | `static void PH7_STREAM_IPPROTO_TCP_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1054 | `{` |
|      74 | 1055 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_TCP));` |
|      35 | 1056 | `	SXUNUSED(pUserData);` |
|      74 | 1057 | `}` |
|      71 | 1058 | `static void PH7_STREAM_IPPROTO_UDP_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1059 | `{` |
|      74 | 1060 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_UDP));` |
|      35 | 1061 | `	SXUNUSED(pUserData);` |
|      74 | 1062 | `}` |
|      71 | 1063 | `static void PH7_STREAM_IPPROTO_ICMP_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1064 | `{` |
|      74 | 1065 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_ICMP));` |
|      35 | 1066 | `	SXUNUSED(pUserData);` |
|      74 | 1067 | `}` |
|      71 | 1068 | `static void PH7_STREAM_IPPROTO_RAW_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1069 | `{` |
|      74 | 1070 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_RAW));` |
|      35 | 1071 | `	SXUNUSED(pUserData);` |
|      74 | 1072 | `}` |
|       - | 1073 | `#endif /* PH7_ENABLE_NET */` |
|       - | 1074 | `/*` |
|       - | 1075 | ` * stream_socket_shutdown()'s $mode, and the two recvfrom/sendto flags. These` |
|       - | 1076 | ` * three ARE php's own numbers rather than the OS's: php maps STREAM_OOB and` |
|       - | 1077 | ` * STREAM_PEEK onto MSG_OOB/MSG_PEEK itself.` |
|       - | 1078 | ` */` |
|      75 | 1079 | `static void PH7_STREAM_SHUT_RD_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1080 | `{` |
|      78 | 1081 | `	ph7_value_int(pVal,PH7_STREAM_SHUT_RD);` |
|      37 | 1082 | `	SXUNUSED(pUserData);` |
|      78 | 1083 | `}` |
|      73 | 1084 | `static void PH7_STREAM_SHUT_WR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1085 | `{` |
|      76 | 1086 | `	ph7_value_int(pVal,PH7_STREAM_SHUT_WR);` |
|      36 | 1087 | `	SXUNUSED(pUserData);` |
|      76 | 1088 | `}` |
|      73 | 1089 | `static void PH7_STREAM_SHUT_RDWR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1090 | `{` |
|      76 | 1091 | `	ph7_value_int(pVal,PH7_STREAM_SHUT_RDWR);` |
|      36 | 1092 | `	SXUNUSED(pUserData);` |
|      76 | 1093 | `}` |
|      71 | 1094 | `static void PH7_STREAM_OOB_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1095 | `{` |
|      74 | 1096 | `	ph7_value_int(pVal,PH7_STREAM_OOB);` |
|      35 | 1097 | `	SXUNUSED(pUserData);` |
|      74 | 1098 | `}` |
|      75 | 1099 | `static void PH7_STREAM_PEEK_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1100 | `{` |
|      78 | 1101 | `	ph7_value_int(pVal,PH7_STREAM_PEEK);` |
|      37 | 1102 | `	SXUNUSED(pUserData);` |
|      78 | 1103 | `}` |
|       - | 1104 | `/*` |
|       - | 1105 | ` * stream_socket_server()'s $flags. Its default is BIND\|LISTEN, and the two are` |
|       - | 1106 | ` * separate because binding is all a datagram server does.` |
|       - | 1107 | ` */` |
|     113 | 1108 | `static void PH7_STREAM_SERVER_BIND_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1109 | `{` |
|     117 | 1110 | `	ph7_value_int(pVal,PH7_STREAM_SERVER_BIND);` |
|      56 | 1111 | `	SXUNUSED(pUserData);` |
|     117 | 1112 | `}` |
|     101 | 1113 | `static void PH7_STREAM_SERVER_LISTEN_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1114 | `{` |
|     105 | 1115 | `	ph7_value_int(pVal,PH7_STREAM_SERVER_LISTEN);` |
|      50 | 1116 | `	SXUNUSED(pUserData);` |
|     105 | 1117 | `}` |
|       - | 1118 | `/*` |
|       - | 1119 | ` * mt_srand()'s $mode: which GENERATOR to seed. MT_RAND_PHP is php's pre-7.1` |
|       - | 1120 | ` * Mersenne Twister, whose twist reads the low bit of the wrong word — a` |
|       - | 1121 | ` * different sequence, which is the only reason to ask for it.` |
|       - | 1122 | ` */` |
|      82 | 1123 | `static void PH7_MT_RAND_MT19937_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1124 | `{` |
|      85 | 1125 | `	ph7_value_int(pVal,PH7_MT_RAND_MT19937);` |
|      41 | 1126 | `	SXUNUSED(pUserData);` |
|      85 | 1127 | `}` |
|      92 | 1128 | `static void PH7_MT_RAND_PHP_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1129 | `{` |
|       - | 1130 | `	/* php 8.3 deprecated the SYMBOL as well as the mode. The notice is` |
|       - | 1131 | `	 * aDeprecatedConst[]'s now, raised where every deprecated constant's is. */` |
|      95 | 1132 | `	ph7_value_int(pVal,PH7_MT_RAND_PHP);` |
|      46 | 1133 | `	SXUNUSED(pUserData);` |
|      95 | 1134 | `}` |
|       - | 1135 | `/*` |
|       - | 1136 | ` * Output-handler flags and phases (ob_start()'s $flags, and the $phase an output` |
|       - | 1137 | ` * handler is called with). The values are php's and are a public ABI: the phase` |
|       - | 1138 | ` * bits are OR'd together (a first FLUSH arrives as FLUSH\|START = 5), and the` |
|       - | 1139 | ` * three capability flags are the ones ob_clean()/ob_flush()/ob_end_*() test` |
|       - | 1140 | ` * before they will touch the buffer.` |
|       - | 1141 | ` */` |
|     148 | 1142 | `static void PH7_OB_WRITE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1143 | `{` |
|     152 | 1144 | `	ph7_value_int(pVal,PH7_OB_WRITE);` |
|      74 | 1145 | `	SXUNUSED(pUserData);` |
|     152 | 1146 | `}` |
|      74 | 1147 | `static void PH7_OB_START_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1148 | `{` |
|      78 | 1149 | `	ph7_value_int(pVal,PH7_OB_START);` |
|      37 | 1150 | `	SXUNUSED(pUserData);` |
|      78 | 1151 | `}` |
|      74 | 1152 | `static void PH7_OB_CLEAN_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1153 | `{` |
|      78 | 1154 | `	ph7_value_int(pVal,PH7_OB_CLEAN);` |
|      37 | 1155 | `	SXUNUSED(pUserData);` |
|      78 | 1156 | `}` |
|      74 | 1157 | `static void PH7_OB_FLUSH_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1158 | `{` |
|      78 | 1159 | `	ph7_value_int(pVal,PH7_OB_FLUSH);` |
|      37 | 1160 | `	SXUNUSED(pUserData);` |
|      78 | 1161 | `}` |
|     148 | 1162 | `static void PH7_OB_FINAL_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1163 | `{` |
|     152 | 1164 | `	ph7_value_int(pVal,PH7_OB_FINAL);` |
|      74 | 1165 | `	SXUNUSED(pUserData);` |
|     152 | 1166 | `}` |
|      78 | 1167 | `static void PH7_OB_CLEANABLE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1168 | `{` |
|      82 | 1169 | `	ph7_value_int(pVal,PH7_OB_CLEANABLE);` |
|      39 | 1170 | `	SXUNUSED(pUserData);` |
|      82 | 1171 | `}` |
|      78 | 1172 | `static void PH7_OB_FLUSHABLE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1173 | `{` |
|      82 | 1174 | `	ph7_value_int(pVal,PH7_OB_FLUSHABLE);` |
|      39 | 1175 | `	SXUNUSED(pUserData);` |
|      82 | 1176 | `}` |
|      80 | 1177 | `static void PH7_OB_REMOVABLE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1178 | `{` |
|      84 | 1179 | `	ph7_value_int(pVal,PH7_OB_REMOVABLE);` |
|      40 | 1180 | `	SXUNUSED(pUserData);` |
|      84 | 1181 | `}` |
|      88 | 1182 | `static void PH7_OB_STDFLAGS_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1183 | `{` |
|      92 | 1184 | `	ph7_value_int(pVal,PH7_OB_STDFLAGS);` |
|      44 | 1185 | `	SXUNUSED(pUserData);` |
|      92 | 1186 | `}` |
|      76 | 1187 | `static void PH7_OB_STARTED_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1188 | `{` |
|      80 | 1189 | `	ph7_value_int(pVal,PH7_OB_STARTED);` |
|      38 | 1190 | `	SXUNUSED(pUserData);` |
|      80 | 1191 | `}` |
|      76 | 1192 | `static void PH7_OB_DISABLED_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1193 | `{` |
|      80 | 1194 | `	ph7_value_int(pVal,PH7_OB_DISABLED);` |
|      38 | 1195 | `	SXUNUSED(pUserData);` |
|      80 | 1196 | `}` |
|      78 | 1197 | `static void PH7_OB_PROCESSED_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1198 | `{` |
|      82 | 1199 | `	ph7_value_int(pVal,PH7_OB_PROCESSED);` |
|      39 | 1200 | `	SXUNUSED(pUserData);` |
|      82 | 1201 | `}` |
|       - | 1202 | `/*` |
|       - | 1203 | ` * array_filter()'s $mode selector. The VALUES are php's and are a public ABI --` |
|       - | 1204 | ` * ARRAY_FILTER_USE_BOTH is 1 and ARRAY_FILTER_USE_KEY is 2, NOT the other way` |
|       - | 1205 | ` * round, and they are a selector rather than a bit mask (php reads the argument` |
|       - | 1206 | ` * with ==, so any other number is the default value mode).` |
|       - | 1207 | ` */` |
|       - | 1208 | `/*` |
|       - | 1209 | ` * CONNECTION_NORMAL / CONNECTION_ABORTED / CONNECTION_TIMEOUT` |
|       - | 1210 | ` *  The three states connection_status() reports. On a CLI there is no client` |
|       - | 1211 | ` *  to disconnect and no time limit to run out, so NORMAL is the only one that` |
|       - | 1212 | ` *  is ever answered -- but the names are what a program COMPARES against, and` |
|       - | 1213 | ` *  an undefined constant is a fatal.` |
|       - | 1214 | ` */` |
|      75 | 1215 | `static void PH7_CONNECTION_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1216 | `{` |
|      78 | 1217 | `	ph7_value_int(pVal,0);` |
|      37 | 1218 | `	SXUNUSED(pUserData);` |
|      78 | 1219 | `}` |
|      73 | 1220 | `static void PH7_CONNECTION_ABORTED_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1221 | `{` |
|      76 | 1222 | `	ph7_value_int(pVal,1);` |
|      36 | 1223 | `	SXUNUSED(pUserData);` |
|      76 | 1224 | `}` |
|      73 | 1225 | `static void PH7_CONNECTION_TIMEOUT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1226 | `{` |
|      76 | 1227 | `	ph7_value_int(pVal,2);` |
|      36 | 1228 | `	SXUNUSED(pUserData);` |
|      76 | 1229 | `}` |
|      89 | 1230 | `static void PH7_ARRAY_FILTER_USE_KEY_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1231 | `{` |
|      92 | 1232 | `	ph7_value_int(pVal,2);` |
|      44 | 1233 | `	SXUNUSED(pUserData);` |
|      92 | 1234 | `}` |
|      81 | 1235 | `static void PH7_ARRAY_FILTER_USE_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1236 | `{` |
|      84 | 1237 | `	ph7_value_int(pVal,1);` |
|      40 | 1238 | `	SXUNUSED(pUserData);` |
|      84 | 1239 | `}` |
|       - | 1240 | `/*` |
|       - | 1241 | ` * COUNT_NORMAL` |
|       - | 1242 | ` *  Expands 0` |
|       - | 1243 | ` */` |
|      83 | 1244 | `static void PH7_COUNT_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1245 | `{` |
|      86 | 1246 | `	ph7_value_int(pVal,0);` |
|      41 | 1247 | `	SXUNUSED(pUserData);` |
|      86 | 1248 | `}` |
|       - | 1249 | `/*` |
|       - | 1250 | ` * COUNT_RECURSIVE` |
|       - | 1251 | ` *  Expands 1.` |
|       - | 1252 | ` */` |
|      93 | 1253 | `static void PH7_COUNT_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1254 | `{` |
|      96 | 1255 | `	ph7_value_int(pVal,1);` |
|      46 | 1256 | `	SXUNUSED(pUserData);` |
|      96 | 1257 | `}` |
|       - | 1258 | `/*` |
|       - | 1259 | ` * php's sort-flag constants. The VALUES must match php exactly: they are a` |
|       - | 1260 | ` * public ABI (code passes literal ints, dumps them, and OR-combines the base` |
|       - | 1261 | ` * type with SORT_FLAG_CASE). SORT_ASC/SORT_DESC are the array_multisort` |
|       - | 1262 | ` * direction flags.` |
|       - | 1263 | ` * SORT_REGULAR 0 · SORT_NUMERIC 1 · SORT_STRING 2 · SORT_DESC 3 · SORT_ASC 4 ·` |
|       - | 1264 | ` * SORT_LOCALE_STRING 5 · SORT_NATURAL 6 · SORT_FLAG_CASE 8` |
|       - | 1265 | ` */` |
|      87 | 1266 | `static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1267 | `{` |
|      92 | 1268 | `	ph7_value_int(pVal,4);` |
|      43 | 1269 | `	SXUNUSED(pUserData);` |
|      92 | 1270 | `}` |
|      83 | 1271 | `static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1272 | `{` |
|      87 | 1273 | `	ph7_value_int(pVal,3);` |
|      41 | 1274 | `	SXUNUSED(pUserData);` |
|      87 | 1275 | `}` |
|     105 | 1276 | `static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1277 | `{` |
|     109 | 1278 | `	ph7_value_int(pVal,0);` |
|      52 | 1279 | `	SXUNUSED(pUserData);` |
|     109 | 1280 | `}` |
|     143 | 1281 | `static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1282 | `{` |
|     147 | 1283 | `	ph7_value_int(pVal,1);` |
|      71 | 1284 | `	SXUNUSED(pUserData);` |
|     147 | 1285 | `}` |
|    2108 | 1286 | `static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1287 | `{` |
|    2113 | 1288 | `	ph7_value_int(pVal,2);` |
|    1051 | 1289 | `	SXUNUSED(pUserData);` |
|    2113 | 1290 | `}` |
|      75 | 1291 | `static void PH7_SORT_LOCALE_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1292 | `{` |
|      79 | 1293 | `	ph7_value_int(pVal,5);` |
|      37 | 1294 | `	SXUNUSED(pUserData);` |
|      79 | 1295 | `}` |
|      91 | 1296 | `static void PH7_SORT_NATURAL_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1297 | `{` |
|      95 | 1298 | `	ph7_value_int(pVal,6);` |
|      45 | 1299 | `	SXUNUSED(pUserData);` |
|      95 | 1300 | `}` |
|      97 | 1301 | `static void PH7_SORT_FLAG_CASE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1302 | `{` |
|     102 | 1303 | `	ph7_value_int(pVal,8);` |
|      48 | 1304 | `	SXUNUSED(pUserData);` |
|     102 | 1305 | `}` |
|       - | 1306 | `/*` |
|       - | 1307 | ` * PHP_ROUND_HALF_UP` |
|       - | 1308 | ` *  Expands 1.` |
|       - | 1309 | ` */` |
|      75 | 1310 | `static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1311 | `{` |
|      78 | 1312 | `	ph7_value_int(pVal,1);` |
|      37 | 1313 | `	SXUNUSED(pUserData);` |
|      78 | 1314 | `}` |
|       - | 1315 | `/*` |
|       - | 1316 | ` * PHP_SESSION_DISABLED / PHP_SESSION_NONE / PHP_SESSION_ACTIVE` |
|       - | 1317 | ` *  session_status() states (0 / 1 / 2).` |
|       - | 1318 | ` */` |
|      72 | 1319 | `static void PH7_PHP_SESSION_DISABLED_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1320 | `{` |
|      76 | 1321 | `	ph7_value_int(pVal,0);` |
|      36 | 1322 | `	SXUNUSED(pUserData);` |
|      76 | 1323 | `}` |
|      72 | 1324 | `static void PH7_PHP_SESSION_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1325 | `{` |
|      76 | 1326 | `	ph7_value_int(pVal,1);` |
|      36 | 1327 | `	SXUNUSED(pUserData);` |
|      76 | 1328 | `}` |
|      86 | 1329 | `static void PH7_PHP_SESSION_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1330 | `{` |
|      91 | 1331 | `	ph7_value_int(pVal,2);` |
|      43 | 1332 | `	SXUNUSED(pUserData);` |
|      91 | 1333 | `}` |
|       - | 1334 | `/*` |
|       - | 1335 | ` * INI_USER / INI_PERDIR / INI_SYSTEM / INI_ALL` |
|       - | 1336 | ` *  php.ini access levels (1 / 2 / 4 / 7).` |
|       - | 1337 | ` */` |
|      73 | 1338 | `static void PH7_INI_USER_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1339 | `{` |
|      77 | 1340 | `	ph7_value_int(pVal,1);` |
|      36 | 1341 | `	SXUNUSED(pUserData);` |
|      77 | 1342 | `}` |
|      73 | 1343 | `static void PH7_INI_PERDIR_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1344 | `{` |
|      77 | 1345 | `	ph7_value_int(pVal,2);` |
|      36 | 1346 | `	SXUNUSED(pUserData);` |
|      77 | 1347 | `}` |
|      73 | 1348 | `static void PH7_INI_SYSTEM_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1349 | `{` |
|      77 | 1350 | `	ph7_value_int(pVal,4);` |
|      36 | 1351 | `	SXUNUSED(pUserData);` |
|      77 | 1352 | `}` |
|      73 | 1353 | `static void PH7_INI_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1354 | `{` |
|      77 | 1355 | `	ph7_value_int(pVal,7);` |
|      36 | 1356 | `	SXUNUSED(pUserData);` |
|      77 | 1357 | `}` |
|       - | 1358 | `/*` |
|       - | 1359 | ` * MB_CASE_UPPER / MB_CASE_LOWER / MB_CASE_TITLE (0 / 1 / 2)` |
|       - | 1360 | ` */` |
|      74 | 1361 | `static void PH7_MB_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1362 | `{` |
|      77 | 1363 | `	ph7_value_int(pVal,0);` |
|      37 | 1364 | `	SXUNUSED(pUserData);` |
|      77 | 1365 | `}` |
|      74 | 1366 | `static void PH7_MB_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1367 | `{` |
|      77 | 1368 | `	ph7_value_int(pVal,1);` |
|      37 | 1369 | `	SXUNUSED(pUserData);` |
|      77 | 1370 | `}` |
|     110 | 1371 | `static void PH7_MB_CASE_TITLE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1372 | `{` |
|     113 | 1373 | `	ph7_value_int(pVal,2);` |
|      55 | 1374 | `	SXUNUSED(pUserData);` |
|     113 | 1375 | `}` |
|       - | 1376 | `/*` |
|       - | 1377 | ` * SPHP_ROUND_HALF_DOWN` |
|       - | 1378 | ` *  Expands 2.` |
|       - | 1379 | ` */` |
|      75 | 1380 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1381 | `{` |
|      78 | 1382 | `	ph7_value_int(pVal,2);` |
|      37 | 1383 | `	SXUNUSED(pUserData);` |
|      78 | 1384 | `}` |
|       - | 1385 | `/*` |
|       - | 1386 | ` * PHP_ROUND_HALF_EVEN` |
|       - | 1387 | ` *  Expands 3.` |
|       - | 1388 | ` */` |
|      79 | 1389 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1390 | `{` |
|      82 | 1391 | `	ph7_value_int(pVal,3);` |
|      39 | 1392 | `	SXUNUSED(pUserData);` |
|      82 | 1393 | `}` |
|       - | 1394 | `/*` |
|       - | 1395 | ` * PHP_ROUND_HALF_ODD` |
|       - | 1396 | ` *  Expands 4.` |
|       - | 1397 | ` */` |
|      75 | 1398 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1399 | `{` |
|      78 | 1400 | `	ph7_value_int(pVal,4);` |
|      37 | 1401 | `	SXUNUSED(pUserData);` |
|      78 | 1402 | `}` |
|       - | 1403 | `/*` |
|       - | 1404 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|       - | 1405 | ` *  Expand 0x01` |
|       - | 1406 | ` * NOTE:` |
|       - | 1407 | ` *  The expanded value must be a power of two.` |
|       - | 1408 | ` */` |
|      94 | 1409 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1410 | `{` |
|      97 | 1411 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      47 | 1412 | `	SXUNUSED(pUserData);` |
|      97 | 1413 | `}` |
|       - | 1414 | `/*` |
|       - | 1415 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|       - | 1416 | ` *  Expand 0x02` |
|       - | 1417 | ` * NOTE:` |
|       - | 1418 | ` *  The expanded value must be a power of two.` |
|       - | 1419 | ` */` |
|     130 | 1420 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1421 | `{` |
|     134 | 1422 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      65 | 1423 | `	SXUNUSED(pUserData);` |
|     134 | 1424 | `}` |
|       - | 1425 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|       - | 1426 | `/*` |
|       - | 1427 | ` * M_PI` |
|       - | 1428 | ` *  Expand the value of pi.` |
|       - | 1429 | ` */` |
|      85 | 1430 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1431 | `{` |
|      42 | 1432 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 1433 | `	ph7_value_double(pVal,PH7_PI);` |
|      88 | 1434 | `}` |
|       - | 1435 | `/*` |
|       - | 1436 | ` * M_E` |
|       - | 1437 | ` *  Expand 2.7182818284590452354` |
|       - | 1438 | ` */` |
|      85 | 1439 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1440 | `{` |
|      42 | 1441 | `	SXUNUSED(pUserData); /* cc warning */` |
|      89 | 1442 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|      89 | 1443 | `}` |
|       - | 1444 | `/*` |
|       - | 1445 | ` * M_LOG2E` |
|       - | 1446 | ` *  Expand 2.7182818284590452354` |
|       - | 1447 | ` */` |
|      73 | 1448 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1449 | `{` |
|      36 | 1450 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1451 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      76 | 1452 | `}` |
|       - | 1453 | `/*` |
|       - | 1454 | ` * M_LOG10E` |
|       - | 1455 | ` *  Expand 0.4342944819032518276` |
|       - | 1456 | ` */` |
|      73 | 1457 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1458 | `{` |
|      36 | 1459 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1460 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      76 | 1461 | `}` |
|       - | 1462 | `/*` |
|       - | 1463 | ` * M_LN2` |
|       - | 1464 | ` *  Expand 	0.69314718055994530942` |
|       - | 1465 | ` */` |
|      73 | 1466 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1467 | `{` |
|      36 | 1468 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1469 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      76 | 1470 | `}` |
|       - | 1471 | `/*` |
|       - | 1472 | ` * M_LN10` |
|       - | 1473 | ` *  Expand 	2.30258509299404568402` |
|       - | 1474 | ` */` |
|      73 | 1475 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1476 | `{` |
|      36 | 1477 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1478 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      76 | 1479 | `}` |
|       - | 1480 | `/*` |
|       - | 1481 | ` * M_PI_2` |
|       - | 1482 | ` *  Expand 	1.57079632679489661923` |
|       - | 1483 | ` */` |
|      73 | 1484 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1485 | `{` |
|      36 | 1486 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1487 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      76 | 1488 | `}` |
|       - | 1489 | `/*` |
|       - | 1490 | ` * M_PI_4` |
|       - | 1491 | ` *  Expand 	0.78539816339744830962` |
|       - | 1492 | ` */` |
|      73 | 1493 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1494 | `{` |
|      36 | 1495 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1496 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      76 | 1497 | `}` |
|       - | 1498 | `/*` |
|       - | 1499 | ` * M_1_PI` |
|       - | 1500 | ` *  Expand 	0.31830988618379067154` |
|       - | 1501 | ` */` |
|      73 | 1502 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1503 | `{` |
|      36 | 1504 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1505 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      76 | 1506 | `}` |
|       - | 1507 | `/*` |
|       - | 1508 | ` * M_2_PI` |
|       - | 1509 | ` *  Expand 0.63661977236758134308` |
|       - | 1510 | ` */` |
|      75 | 1511 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1512 | `{` |
|      37 | 1513 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 1514 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      78 | 1515 | `}` |
|       - | 1516 | `/*` |
|       - | 1517 | ` * M_SQRTPI` |
|       - | 1518 | ` *  Expand 1.77245385090551602729` |
|       - | 1519 | ` */` |
|      73 | 1520 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1521 | `{` |
|      36 | 1522 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1523 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      76 | 1524 | `}` |
|       - | 1525 | `/*` |
|       - | 1526 | ` * M_2_SQRTPI` |
|       - | 1527 | ` *  Expand 	1.12837916709551257390` |
|       - | 1528 | ` */` |
|      73 | 1529 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1530 | `{` |
|      36 | 1531 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1532 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      76 | 1533 | `}` |
|       - | 1534 | `/*` |
|       - | 1535 | ` * M_SQRT2` |
|       - | 1536 | ` *  Expand 	1.41421356237309504880` |
|       - | 1537 | ` */` |
|      73 | 1538 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1539 | `{` |
|      36 | 1540 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1541 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      76 | 1542 | `}` |
|       - | 1543 | `/*` |
|       - | 1544 | ` * M_SQRT3` |
|       - | 1545 | ` *  Expand 	1.73205080756887729352` |
|       - | 1546 | ` */` |
|      73 | 1547 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1548 | `{` |
|      36 | 1549 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1550 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      76 | 1551 | `}` |
|       - | 1552 | `/*` |
|       - | 1553 | ` * M_SQRT1_2` |
|       - | 1554 | ` *  Expand 	0.70710678118654752440` |
|       - | 1555 | ` */` |
|      73 | 1556 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1557 | `{` |
|      36 | 1558 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1559 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      76 | 1560 | `}` |
|       - | 1561 | `/*` |
|       - | 1562 | ` * M_LNPI` |
|       - | 1563 | ` *  Expand 	1.14472988584940017414` |
|       - | 1564 | ` */` |
|      73 | 1565 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1566 | `{` |
|      36 | 1567 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1568 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      76 | 1569 | `}` |
|       - | 1570 | `/*` |
|       - | 1571 | ` * M_EULER` |
|       - | 1572 | ` *  Expand  0.57721566490153286061` |
|       - | 1573 | ` */` |
|      73 | 1574 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1575 | `{` |
|      36 | 1576 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1577 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      76 | 1578 | `}` |
|       - | 1579 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|       - | 1580 | `/*` |
|       - | 1581 | ` * SUNFUNCS_RET_TIMESTAMP / SUNFUNCS_RET_STRING / SUNFUNCS_RET_DOUBLE` |
|       - | 1582 | ` *  The three shapes date_sunrise() and date_sunset() can answer in: an` |
|       - | 1583 | ` *  absolute Unix timestamp, an "H:i" clock face, or the hour as a float.` |
|       - | 1584 | ` *  STRING is the default, which is why it is 1 rather than 0.` |
|       - | 1585 | ` */` |
|      72 | 1586 | `static void PH7_SUNFUNCS_RET_TIMESTAMP_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1587 | `{` |
|      36 | 1588 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1589 | `	ph7_value_int(pVal,0);` |
|      76 | 1590 | `}` |
|      74 | 1591 | `static void PH7_SUNFUNCS_RET_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1592 | `{` |
|      37 | 1593 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 1594 | `	ph7_value_int(pVal,1);` |
|      78 | 1595 | `}` |
|      72 | 1596 | `static void PH7_SUNFUNCS_RET_DOUBLE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1597 | `{` |
|      36 | 1598 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1599 | `	ph7_value_int(pVal,2);` |
|      76 | 1600 | `}` |
|       - | 1601 | `/*` |
|       - | 1602 | ` * DATE_ATOM` |
|       - | 1603 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|       - | 1604 | ` */` |
|     144 | 1605 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1606 | `{` |
|      72 | 1607 | `	SXUNUSED(pUserData); /* cc warning */` |
|     147 | 1608 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|     147 | 1609 | `}` |
|       - | 1610 | `/*` |
|       - | 1611 | ` * DATE_COOKIE` |
|       - | 1612 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|       - | 1613 | ` */` |
|      72 | 1614 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1615 | `{` |
|      36 | 1616 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 1617 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      75 | 1618 | `}` |
|       - | 1619 | `/*` |
|       - | 1620 | ` * DATE_ISO8601` |
|       - | 1621 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|       - | 1622 | ` */` |
|      72 | 1623 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1624 | `{` |
|      36 | 1625 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 1626 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      75 | 1627 | `}` |
|       - | 1628 | `/*` |
|       - | 1629 | ` * DATE_RFC822` |
|       - | 1630 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|       - | 1631 | ` */` |
|      72 | 1632 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1633 | `{` |
|      36 | 1634 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 1635 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      75 | 1636 | `}` |
|       - | 1637 | `/*` |
|       - | 1638 | ` * DATE_RFC850` |
|       - | 1639 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|       - | 1640 | ` */` |
|      72 | 1641 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1642 | `{` |
|      36 | 1643 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 1644 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      75 | 1645 | `}` |
|       - | 1646 | `/*` |
|       - | 1647 | ` * DATE_RFC1036` |
|       - | 1648 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1649 | ` */` |
|      72 | 1650 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1651 | `{` |
|      36 | 1652 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 1653 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      75 | 1654 | `}` |
|       - | 1655 | `/*` |
|       - | 1656 | ` * DATE_RFC1123` |
|       - | 1657 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1658 | ` */` |
|      72 | 1659 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1660 | `{` |
|      36 | 1661 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 1662 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      75 | 1663 | `}` |
|       - | 1664 | `/*` |
|       - | 1665 | ` * DATE_RFC2822` |
|       - | 1666 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1667 | ` */` |
|      72 | 1668 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1669 | `{` |
|      36 | 1670 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 1671 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      75 | 1672 | `}` |
|       - | 1673 | `/*` |
|       - | 1674 | ` * DATE_RSS` |
|       - | 1675 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1676 | ` */` |
|      72 | 1677 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1678 | `{` |
|      36 | 1679 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 1680 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      75 | 1681 | `}` |
|       - | 1682 | `/*` |
|       - | 1683 | ` * DATE_W3C` |
|       - | 1684 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|       - | 1685 | ` */` |
|      72 | 1686 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1687 | `{` |
|      36 | 1688 | `	SXUNUSED(pUserData); /* cc warning */` |
|      75 | 1689 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      75 | 1690 | `}` |
|       - | 1691 | `/*` |
|       - | 1692 | ` * The three format constants php added after the original set. Each is a plain` |
|       - | 1693 | ` * format STRING, so the whole of its behaviour is what date()/DateTime::format()` |
|       - | 1694 | ` * already do with those characters -- but each was a loud undefined-constant` |
|       - | 1695 | ` * fatal, which is a program that does not run rather than one that runs wrong.` |
|       - | 1696 | ` *` |
|       - | 1697 | ` * DATE_RFC7231 is the HTTP date (always GMT, so the zone letters are ESCAPED` |
|       - | 1698 | ` * rather than formatted -- php's own definition, and the reason it is not` |
|       - | 1699 | ` * DATE_RFC1123 with a T on the end). DATE_RFC3339_EXTENDED carries` |
|       - | 1700 | `` * milliseconds. DATE_ISO8601_EXPANDED uses `X`, the expanded-year field, where`` |
|       - | 1701 | `` * the plain DATE_ISO8601 uses `Y`.`` |
|       - | 1702 | ` */` |
|      78 | 1703 | `static void PH7_DATE_RFC7231_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1704 | `{` |
|      39 | 1705 | `	SXUNUSED(pUserData); /* cc warning */` |
|      81 | 1706 | `	ph7_value_string(pVal,"D, d M Y H:i:s \\G\\M\\T",-1/*Compute length automatically*/);` |
|      81 | 1707 | `}` |
|      70 | 1708 | `static void PH7_DATE_RFC3339_EXTENDED_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1709 | `{` |
|      35 | 1710 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1711 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:s.vP",-1/*Compute length automatically*/);` |
|      73 | 1712 | `}` |
|      70 | 1713 | `static void PH7_DATE_ISO8601_EXPANDED_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1714 | `{` |
|      35 | 1715 | `	SXUNUSED(pUserData); /* cc warning */` |
|      73 | 1716 | `	ph7_value_string(pVal,"X-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      73 | 1717 | `}` |
|       - | 1718 | `/*` |
|       - | 1719 | ` * FILE_TEXT / FILE_BINARY` |
|       - | 1720 | ` *  Both expand 0. php declares them for file()/file_put_contents()'s $flags and` |
|       - | 1721 | ` *  ignores them (the CLI has no text mode to select), but a program that names` |
|       - | 1722 | ` *  one still has to COMPILE, and an undefined constant is a fatal.` |
|       - | 1723 | ` */` |
|     158 | 1724 | `static void PH7_FILE_TEXT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1725 | `{` |
|     161 | 1726 | `	ph7_value_int(pVal,0);` |
|      78 | 1727 | `	SXUNUSED(pUserData);` |
|     161 | 1728 | `}` |
|       - | 1729 | `/*` |
|       - | 1730 | ` * The ENT_* values are PHP-exact (php 8.5.7). The low two bits are the quote` |
|       - | 1731 | ` * bits (1 = single, 2 = double), so ENT_QUOTES = ENT_COMPAT\|1 and` |
|       - | 1732 | ` * ENT_NOQUOTES = 0. Bits 16\|32 select the doctype (0 = HTML401, 16 = XML1,` |
|       - | 1733 | ` * 32 = XHTML, 48 = HTML5) — composites, not flags.` |
|       - | 1734 | ` */` |
|       - | 1735 | `/*` |
|       - | 1736 | ` * ENT_COMPAT` |
|       - | 1737 | ` *  Expand 2 (double-quote bit only)` |
|       - | 1738 | ` */` |
|      85 | 1739 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1740 | `{` |
|      42 | 1741 | `	SXUNUSED(pUserData); /* cc warning */` |
|      89 | 1742 | `	ph7_value_int(pVal,PH7_ENT_QUOTE_DOUBLE);` |
|      89 | 1743 | `}` |
|       - | 1744 | `/*` |
|       - | 1745 | ` * ENT_QUOTES` |
|       - | 1746 | ` *  Expand 3 (double\|single quote bits)` |
|       - | 1747 | ` */` |
|     251 | 1748 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1749 | `{` |
|     125 | 1750 | `	SXUNUSED(pUserData); /* cc warning */` |
|     255 | 1751 | `	ph7_value_int(pVal,PH7_ENT_QUOTES);` |
|     255 | 1752 | `}` |
|       - | 1753 | `/*` |
|       - | 1754 | ` * ENT_NOQUOTES` |
|       - | 1755 | ` *  Expand 0 (no quote bits)` |
|       - | 1756 | ` */` |
|      93 | 1757 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1758 | `{` |
|      46 | 1759 | `	SXUNUSED(pUserData); /* cc warning */` |
|      97 | 1760 | `	ph7_value_int(pVal,0);` |
|      97 | 1761 | `}` |
|       - | 1762 | `/*` |
|       - | 1763 | ` * ENT_IGNORE` |
|       - | 1764 | ` *  Expand 4` |
|       - | 1765 | ` */` |
|      77 | 1766 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1767 | `{` |
|      38 | 1768 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 1769 | `	ph7_value_int(pVal,PH7_ENT_IGNORE);` |
|      80 | 1770 | `}` |
|       - | 1771 | `/*` |
|       - | 1772 | ` * ENT_SUBSTITUTE` |
|       - | 1773 | ` *  Expand 8` |
|       - | 1774 | ` */` |
|      99 | 1775 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1776 | `{` |
|      49 | 1777 | `	SXUNUSED(pUserData); /* cc warning */` |
|     102 | 1778 | `	ph7_value_int(pVal,PH7_ENT_SUBSTITUTE);` |
|     102 | 1779 | `}` |
|       - | 1780 | `/*` |
|       - | 1781 | ` * ENT_DISALLOWED` |
|       - | 1782 | ` *  Expand 128` |
|       - | 1783 | ` */` |
|     125 | 1784 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1785 | `{` |
|      62 | 1786 | `	SXUNUSED(pUserData); /* cc warning */` |
|     129 | 1787 | `	ph7_value_int(pVal,PH7_ENT_DISALLOWED);` |
|     129 | 1788 | `}` |
|       - | 1789 | `/*` |
|       - | 1790 | ` * ENT_HTML401` |
|       - | 1791 | ` *  Expand 0 (the default doctype)` |
|       - | 1792 | ` */` |
|      99 | 1793 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1794 | `{` |
|      49 | 1795 | `	SXUNUSED(pUserData); /* cc warning */` |
|     102 | 1796 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML401);` |
|     102 | 1797 | `}` |
|       - | 1798 | `/*` |
|       - | 1799 | ` * ENT_XML1` |
|       - | 1800 | ` *  Expand 16` |
|       - | 1801 | ` */` |
|      81 | 1802 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1803 | `{` |
|      40 | 1804 | `	SXUNUSED(pUserData); /* cc warning */` |
|      85 | 1805 | `	ph7_value_int(pVal,PH7_ENT_DOC_XML1);` |
|      85 | 1806 | `}` |
|       - | 1807 | `/*` |
|       - | 1808 | ` * ENT_XHTML` |
|       - | 1809 | ` *  Expand 32` |
|       - | 1810 | ` */` |
|      77 | 1811 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1812 | `{` |
|      38 | 1813 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 1814 | `	ph7_value_int(pVal,PH7_ENT_DOC_XHTML);` |
|      80 | 1815 | `}` |
|       - | 1816 | `/*` |
|       - | 1817 | ` * ENT_HTML5` |
|       - | 1818 | ` *  Expand 48 (16\|32 — a doctype composite, not a flag bit)` |
|       - | 1819 | ` */` |
|      81 | 1820 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1821 | `{` |
|      40 | 1822 | `	SXUNUSED(pUserData); /* cc warning */` |
|      84 | 1823 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML5);` |
|      84 | 1824 | `}` |
|       - | 1825 | `/*` |
|       - | 1826 | ` * ISO-8859-1` |
|       - | 1827 | ` * ISO_8859_1` |
|       - | 1828 | ` *   Expand 1` |
|       - | 1829 | ` */` |
|     146 | 1830 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1831 | `{` |
|      73 | 1832 | `	SXUNUSED(pUserData); /* cc warning */` |
|     149 | 1833 | `	ph7_value_int(pVal,1);` |
|     149 | 1834 | `}` |
|       - | 1835 | `/*` |
|       - | 1836 | ` * UTF-8` |
|       - | 1837 | ` * UTF8` |
|       - | 1838 | ` *  Expand 2` |
|       - | 1839 | ` */` |
|     146 | 1840 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1841 | `{` |
|      73 | 1842 | `	SXUNUSED(pUserData); /* cc warning */` |
|     149 | 1843 | `	ph7_value_int(pVal,1);` |
|     149 | 1844 | `}` |
|       - | 1845 | `/*` |
|       - | 1846 | ` * HTML_ENTITIES` |
|       - | 1847 | ` *  Expand 1` |
|       - | 1848 | ` */` |
|      99 | 1849 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1850 | `{` |
|      49 | 1851 | `	SXUNUSED(pUserData); /* cc warning */` |
|     103 | 1852 | `	ph7_value_int(pVal,1);` |
|     103 | 1853 | `}` |
|       - | 1854 | `/*` |
|       - | 1855 | ` * HTML_SPECIALCHARS` |
|       - | 1856 | ` *  Expand 0 (PHP-exact)` |
|       - | 1857 | ` */` |
|      89 | 1858 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 1859 | `{` |
|      44 | 1860 | `	SXUNUSED(pUserData); /* cc warning */` |
|      93 | 1861 | `	ph7_value_int(pVal,0);` |
|      93 | 1862 | `}` |
|       - | 1863 | `/*` |
|       - | 1864 | ` * PHP_URL_SCHEME.` |
|       - | 1865 | ` * Expand 0` |
|       - | 1866 | ` */` |
|      75 | 1867 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1868 | `{` |
|      37 | 1869 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 1870 | `	ph7_value_int(pVal,0);` |
|      78 | 1871 | `}` |
|       - | 1872 | `/*` |
|       - | 1873 | ` * PHP_URL_HOST.` |
|       - | 1874 | ` * Expand 1` |
|       - | 1875 | ` */` |
|      77 | 1876 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1877 | `{` |
|      38 | 1878 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 1879 | `	ph7_value_int(pVal,1);` |
|      80 | 1880 | `}` |
|       - | 1881 | `/*` |
|       - | 1882 | ` * PHP_URL_PORT.` |
|       - | 1883 | ` * Expand 2` |
|       - | 1884 | ` */` |
|      77 | 1885 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1886 | `{` |
|      38 | 1887 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 1888 | `	ph7_value_int(pVal,2);` |
|      80 | 1889 | `}` |
|       - | 1890 | `/*` |
|       - | 1891 | ` * PHP_URL_USER.` |
|       - | 1892 | ` * Expand 3` |
|       - | 1893 | ` */` |
|      75 | 1894 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1895 | `{` |
|      37 | 1896 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 1897 | `	ph7_value_int(pVal,3);` |
|      78 | 1898 | `}` |
|       - | 1899 | `/*` |
|       - | 1900 | ` * PHP_URL_PASS.` |
|       - | 1901 | ` * Expand 4` |
|       - | 1902 | ` */` |
|      75 | 1903 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1904 | `{` |
|      37 | 1905 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 1906 | `	ph7_value_int(pVal,4);` |
|      78 | 1907 | `}` |
|       - | 1908 | `/*` |
|       - | 1909 | ` * PHP_URL_PATH.` |
|       - | 1910 | ` * Expand 5` |
|       - | 1911 | ` */` |
|      75 | 1912 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1913 | `{` |
|      37 | 1914 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 1915 | `	ph7_value_int(pVal,5);` |
|      78 | 1916 | `}` |
|       - | 1917 | `/*` |
|       - | 1918 | ` * PHP_URL_QUERY.` |
|       - | 1919 | ` * Expand 6` |
|       - | 1920 | ` */` |
|      77 | 1921 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1922 | `{` |
|      38 | 1923 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 1924 | `	ph7_value_int(pVal,6);` |
|      80 | 1925 | `}` |
|       - | 1926 | `/*` |
|       - | 1927 | ` * PHP_URL_FRAGMENT.` |
|       - | 1928 | ` * Expand 7` |
|       - | 1929 | ` */` |
|      77 | 1930 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1931 | `{` |
|      38 | 1932 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 1933 | `	ph7_value_int(pVal,7);` |
|      80 | 1934 | `}` |
|       - | 1935 | `/*` |
|       - | 1936 | ` * PHP_QUERY_RFC1738` |
|       - | 1937 | ` * Expand 1` |
|       - | 1938 | ` */` |
|      73 | 1939 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1940 | `{` |
|      36 | 1941 | `	SXUNUSED(pUserData); /* cc warning */` |
|      76 | 1942 | `	ph7_value_int(pVal,1);` |
|      76 | 1943 | `}` |
|       - | 1944 | `/*` |
|       - | 1945 | ` * PHP_QUERY_RFC3986` |
|       - | 1946 | ` * Expand 1` |
|       - | 1947 | ` */` |
|      75 | 1948 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1949 | `{` |
|      37 | 1950 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 1951 | `	ph7_value_int(pVal,2);` |
|      78 | 1952 | `}` |
|       - | 1953 | `/* php's FNM_* values (ext/standard): PATHNAME=1, NOESCAPE=2, PERIOD=4, CASEFOLD=16.` |
|       - | 1954 | ` * PHL previously had PATHNAME/NOESCAPE swapped and CASEFOLD=8; fnmatch() reads these` |
|       - | 1955 | ` * bits, so PH7_builtin_fnmatch was updated to the same values. */` |
|       - | 1956 | `/*` |
|       - | 1957 | ` * FNM_PATHNAME` |
|       - | 1958 | ` *  Expand 1 (php value)` |
|       - | 1959 | ` */` |
|      71 | 1960 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1961 | `{` |
|      35 | 1962 | `	SXUNUSED(pUserData); /* cc warning */` |
|      74 | 1963 | `	ph7_value_int(pVal,1);` |
|      74 | 1964 | `}` |
|       - | 1965 | `/*` |
|       - | 1966 | ` * FNM_NOESCAPE` |
|       - | 1967 | ` *  Expand 2 (php value)` |
|       - | 1968 | ` */` |
|     121 | 1969 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1970 | `{` |
|      60 | 1971 | `	SXUNUSED(pUserData); /* cc warning */` |
|     124 | 1972 | `	ph7_value_int(pVal,2);` |
|     124 | 1973 | `}` |
|       - | 1974 | `/*` |
|       - | 1975 | ` * FNM_PERIOD` |
|       - | 1976 | ` *  Expand 4 (php value)` |
|       - | 1977 | ` */` |
|      77 | 1978 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1979 | `{` |
|      38 | 1980 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 1981 | `	ph7_value_int(pVal,4);` |
|      80 | 1982 | `}` |
|       - | 1983 | `/*` |
|       - | 1984 | ` * FNM_CASEFOLD` |
|       - | 1985 | ` *  Expand 16 (php value)` |
|       - | 1986 | ` */` |
|     117 | 1987 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1988 | `{` |
|      58 | 1989 | `	SXUNUSED(pUserData); /* cc warning */` |
|     120 | 1990 | `	ph7_value_int(pVal,16);` |
|     120 | 1991 | `}` |
|       - | 1992 | `/*` |
|       - | 1993 | ` * PATHINFO_DIRNAME` |
|       - | 1994 | ` *  Expand 1.` |
|       - | 1995 | ` */` |
|      93 | 1996 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 1997 | `{` |
|      46 | 1998 | `	SXUNUSED(pUserData); /* cc warning */` |
|      96 | 1999 | `	ph7_value_int(pVal,PH7_PATHINFO_DIRNAME);` |
|      96 | 2000 | `}` |
|       - | 2001 | `/*` |
|       - | 2002 | ` * PATHINFO_BASENAME` |
|       - | 2003 | ` *  Expand 2.` |
|       - | 2004 | ` */` |
|      93 | 2005 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2006 | `{` |
|      46 | 2007 | `	SXUNUSED(pUserData); /* cc warning */` |
|      96 | 2008 | `	ph7_value_int(pVal,PH7_PATHINFO_BASENAME);` |
|      96 | 2009 | `}` |
|       - | 2010 | `/*` |
|       - | 2011 | ` * PATHINFO_EXTENSION` |
|       - | 2012 | ` *  Expand php's 4 (a POWER OF TWO: the components are a bitmask).` |
|       - | 2013 | ` */` |
|    9909 | 2014 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2015 | `{` |
|    4954 | 2016 | `	SXUNUSED(pUserData); /* cc warning */` |
|    9914 | 2017 | `	ph7_value_int(pVal,PH7_PATHINFO_EXTENSION);` |
|    9914 | 2018 | `}` |
|       - | 2019 | `/*` |
|       - | 2020 | ` * PATHINFO_FILENAME` |
|       - | 2021 | ` *  Expand php's 8 (a POWER OF TWO: the components are a bitmask).` |
|       - | 2022 | ` */` |
|    9875 | 2023 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2024 | `{` |
|    4937 | 2025 | `	SXUNUSED(pUserData); /* cc warning */` |
|    9880 | 2026 | `	ph7_value_int(pVal,PH7_PATHINFO_FILENAME);` |
|    9880 | 2027 | `}` |
|       - | 2028 | `/*` |
|       - | 2029 | ` * PATHINFO_ALL` |
|       - | 2030 | ` *  Expand php's 15 — the default, and the one value that answers with the ARRAY.` |
|       - | 2031 | ` */` |
|      81 | 2032 | `static void PH7_PATHINFO_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2033 | `{` |
|      40 | 2034 | `	SXUNUSED(pUserData); /* cc warning */` |
|      84 | 2035 | `	ph7_value_int(pVal,PH7_PATHINFO_ALL);` |
|      84 | 2036 | `}` |
|       - | 2037 | `#ifdef PH7_ENABLE_PCRE` |
|       - | 2038 | `/*` |
|       - | 2039 | ` * php's four PCRE build constants, asked of the linked library (see` |
|       - | 2040 | ` * PH7_PcreVersionInfo). Composer reads PCRE_VERSION before it loads a repository.` |
|       - | 2041 | ` */` |
|      74 | 2042 | `static void PH7_PCRE_VERSION_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2043 | `{` |
|       - | 2044 | `	char zVer[64];` |
|      37 | 2045 | `	SXUNUSED(pUserData);` |
|      77 | 2046 | `	PH7_PcreVersionInfo(zVer,(int)sizeof(zVer),0,0,0);` |
|      77 | 2047 | `	ph7_value_string(pVal,zVer,-1);` |
|      77 | 2048 | `}` |
|      72 | 2049 | `static void PH7_PCRE_VERSION_MAJOR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2050 | `{` |
|       - | 2051 | `	char zVer[64];` |
|      75 | 2052 | `	int iMaj = 0;` |
|      36 | 2053 | `	SXUNUSED(pUserData);` |
|      75 | 2054 | `	PH7_PcreVersionInfo(zVer,(int)sizeof(zVer),&iMaj,0,0);` |
|      75 | 2055 | `	ph7_value_int(pVal,iMaj);` |
|      75 | 2056 | `}` |
|      72 | 2057 | `static void PH7_PCRE_VERSION_MINOR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2058 | `{` |
|       - | 2059 | `	char zVer[64];` |
|      75 | 2060 | `	int iMin = 0;` |
|      36 | 2061 | `	SXUNUSED(pUserData);` |
|      75 | 2062 | `	PH7_PcreVersionInfo(zVer,(int)sizeof(zVer),0,&iMin,0);` |
|      75 | 2063 | `	ph7_value_int(pVal,iMin);` |
|      75 | 2064 | `}` |
|      72 | 2065 | `static void PH7_PCRE_JIT_SUPPORT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2066 | `{` |
|       - | 2067 | `	char zVer[64];` |
|      75 | 2068 | `	int iJit = 0;` |
|      36 | 2069 | `	SXUNUSED(pUserData);` |
|      75 | 2070 | `	PH7_PcreVersionInfo(zVer,(int)sizeof(zVer),0,0,&iJit);` |
|      75 | 2071 | `	ph7_value_bool(pVal,iJit);` |
|      75 | 2072 | `}` |
|       - | 2073 | `#endif /* PH7_ENABLE_PCRE */` |
|       - | 2074 | `/*` |
|       - | 2075 | ` * php's four BUILD-SHAPE booleans. A script reads them to decide what the engine` |
|       - | 2076 | ``  * can do, not what it is called: symfony/process asks `defined('ZEND_THREAD_SAFE')` `` |
|       - | 2077 | `` * to know whether `proc_open` needs an explicit cwd, and with the constant simply`` |
|       - | 2078 | `` * ABSENT it passed null and every subprocess Composer runs died in `is_dir(null)`.`` |
|       - | 2079 | ` * PHL runs one VM per thread with no shared globals, so it answers php's` |
|       - | 2080 | ` * non-ZTS, non-debug shape.` |
|       - | 2081 | ` */` |
|      74 | 2082 | `static void PH7_ZEND_THREAD_SAFE_Const(ph7_value *pVal,void *pUserData)` |
|      77 | 2083 | `{ SXUNUSED(pUserData); ph7_value_bool(pVal,0); }` |
|      74 | 2084 | `static void PH7_ZEND_DEBUG_BUILD_Const(ph7_value *pVal,void *pUserData)` |
|      77 | 2085 | `{ SXUNUSED(pUserData); ph7_value_bool(pVal,0); }` |
|      74 | 2086 | `static void PH7_PHP_ZTS_Const(ph7_value *pVal,void *pUserData)` |
|      77 | 2087 | `{ SXUNUSED(pUserData); ph7_value_bool(pVal,0); }` |
|      74 | 2088 | `static void PH7_PHP_DEBUG_Const(ph7_value *pVal,void *pUserData)` |
|      77 | 2089 | `{ SXUNUSED(pUserData); ph7_value_bool(pVal,0); }` |
|       - | 2090 | `/*` |
|       - | 2091 | ` * php's phpinfo() SECTION flags. A script passes one to say which part it wants;` |
|       - | 2092 | ` * symfony/process asks for INFO_GENERAL to read the build's configure line (it is` |
|       - | 2093 | `` * how it detects `--enable-sigchild`), so Composer needs them to start at all.`` |
|       - | 2094 | ` */` |
|      72 | 2095 | `static void PH7_INFO_GENERAL_Const(ph7_value *pVal,void *pUserData)` |
|      75 | 2096 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,1); }` |
|      72 | 2097 | `static void PH7_INFO_CREDITS_Const(ph7_value *pVal,void *pUserData)` |
|      75 | 2098 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,2); }` |
|      72 | 2099 | `static void PH7_INFO_CONFIGURATION_Const(ph7_value *pVal,void *pUserData)` |
|      75 | 2100 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,4); }` |
|      72 | 2101 | `static void PH7_INFO_MODULES_Const(ph7_value *pVal,void *pUserData)` |
|      75 | 2102 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,8); }` |
|      72 | 2103 | `static void PH7_INFO_ENVIRONMENT_Const(ph7_value *pVal,void *pUserData)` |
|      75 | 2104 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,16); }` |
|      72 | 2105 | `static void PH7_INFO_VARIABLES_Const(ph7_value *pVal,void *pUserData)` |
|      75 | 2106 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,32); }` |
|      72 | 2107 | `static void PH7_INFO_LICENSE_Const(ph7_value *pVal,void *pUserData)` |
|      75 | 2108 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,64); }` |
|      72 | 2109 | `static void PH7_INFO_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      75 | 2110 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,4294967295LL); }` |
|       - | 2111 | `/*` |
|       - | 2112 | ` * The LC_* CATEGORY numbers. php registers the C library's own macros, so its` |
|       - | 2113 | ` * numbers are the platform's (macOS and Windows put LC_ALL at 0); these are` |
|       - | 2114 | ` * glibc's on every platform -- a script that uses the names cannot tell, one` |
|       - | 2115 | ` * that prints the numbers can -- and setlocale() maps them to the platform's` |
|       - | 2116 | ` * macros.` |
|       - | 2117 | ` */` |
|      75 | 2118 | `static void PH7_LC_CTYPE_Const(ph7_value *pVal,void *pUserData)` |
|      78 | 2119 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,0); }` |
|      74 | 2120 | `static void PH7_LC_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|      77 | 2121 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,1); }` |
|      72 | 2122 | `static void PH7_LC_TIME_Const(ph7_value *pVal,void *pUserData)` |
|      75 | 2123 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,2); }` |
|      72 | 2124 | `static void PH7_LC_COLLATE_Const(ph7_value *pVal,void *pUserData)` |
|      75 | 2125 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,3); }` |
|      73 | 2126 | `static void PH7_LC_MONETARY_Const(ph7_value *pVal,void *pUserData)` |
|      77 | 2127 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,4); }` |
|      81 | 2128 | `static void PH7_LC_MESSAGES_Const(ph7_value *pVal,void *pUserData)` |
|      85 | 2129 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,5); }` |
|      86 | 2130 | `static void PH7_LC_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      89 | 2131 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,6); }` |
|       - | 2132 | `/*` |
|       - | 2133 | ` * ext/posix's constants. Every one of them is the PLATFORM's macro rather than` |
|       - | 2134 | `` * a number copied out of one build: `RLIMIT_AS` is 9 on Linux and something`` |
|       - | 2135 | ` * else elsewhere, and a script that stores one and hands it back to` |
|       - | 2136 | ` * posix_getrlimit() has to get its own system's answer. php builds no` |
|       - | 2137 | ` * ext/posix on Windows, so none of these is defined there either.` |
|       - | 2138 | ` */` |
|       - | 2139 | `#ifndef __WINNT__` |
|       - | 2140 | `#ifdef F_OK` |
|      70 | 2141 | `static void PH7_POSIX_F_OK_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2142 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)F_OK); }` |
|       - | 2143 | `#endif` |
|       - | 2144 | `#ifdef X_OK` |
|      70 | 2145 | `static void PH7_POSIX_X_OK_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2146 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)X_OK); }` |
|       - | 2147 | `#endif` |
|       - | 2148 | `#ifdef W_OK` |
|      70 | 2149 | `static void PH7_POSIX_W_OK_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2150 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)W_OK); }` |
|       - | 2151 | `#endif` |
|       - | 2152 | `#ifdef R_OK` |
|      70 | 2153 | `static void PH7_POSIX_R_OK_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2154 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)R_OK); }` |
|       - | 2155 | `#endif` |
|       - | 2156 | `#ifdef S_IFREG` |
|      70 | 2157 | `static void PH7_POSIX_S_IFREG_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2158 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFREG); }` |
|       - | 2159 | `#endif` |
|       - | 2160 | `#ifdef S_IFCHR` |
|      71 | 2161 | `static void PH7_POSIX_S_IFCHR_Const(ph7_value *pVal,void *pUserData)` |
|      71 | 2162 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFCHR); }` |
|       - | 2163 | `#endif` |
|       - | 2164 | `#ifdef S_IFBLK` |
|      71 | 2165 | `static void PH7_POSIX_S_IFBLK_Const(ph7_value *pVal,void *pUserData)` |
|      71 | 2166 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFBLK); }` |
|       - | 2167 | `#endif` |
|       - | 2168 | `#ifdef S_IFIFO` |
|      72 | 2169 | `static void PH7_POSIX_S_IFIFO_Const(ph7_value *pVal,void *pUserData)` |
|      72 | 2170 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFIFO); }` |
|       - | 2171 | `#endif` |
|       - | 2172 | `#ifdef S_IFSOCK` |
|      71 | 2173 | `static void PH7_POSIX_S_IFSOCK_Const(ph7_value *pVal,void *pUserData)` |
|      71 | 2174 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFSOCK); }` |
|       - | 2175 | `#endif` |
|       - | 2176 | `#ifdef RLIMIT_AS` |
|      70 | 2177 | `static void PH7_POSIX_RLIMIT_AS_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2178 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_AS); }` |
|       - | 2179 | `#endif` |
|       - | 2180 | `#ifdef RLIMIT_CORE` |
|      71 | 2181 | `static void PH7_POSIX_RLIMIT_CORE_Const(ph7_value *pVal,void *pUserData)` |
|      71 | 2182 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_CORE); }` |
|       - | 2183 | `#endif` |
|       - | 2184 | `#ifdef RLIMIT_CPU` |
|      70 | 2185 | `static void PH7_POSIX_RLIMIT_CPU_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2186 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_CPU); }` |
|       - | 2187 | `#endif` |
|       - | 2188 | `#ifdef RLIMIT_DATA` |
|      70 | 2189 | `static void PH7_POSIX_RLIMIT_DATA_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2190 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_DATA); }` |
|       - | 2191 | `#endif` |
|       - | 2192 | `#ifdef RLIMIT_FSIZE` |
|      70 | 2193 | `static void PH7_POSIX_RLIMIT_FSIZE_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2194 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_FSIZE); }` |
|       - | 2195 | `#endif` |
|       - | 2196 | `#ifdef RLIMIT_LOCKS` |
|      35 | 2197 | `static void PH7_POSIX_RLIMIT_LOCKS_Const(ph7_value *pVal,void *pUserData)` |
|      35 | 2198 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_LOCKS); }` |
|       - | 2199 | `#endif` |
|       - | 2200 | `#ifdef RLIMIT_MEMLOCK` |
|      70 | 2201 | `static void PH7_POSIX_RLIMIT_MEMLOCK_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2202 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_MEMLOCK); }` |
|       - | 2203 | `#endif` |
|       - | 2204 | `#ifdef RLIMIT_MSGQUEUE` |
|      35 | 2205 | `static void PH7_POSIX_RLIMIT_MSGQUEUE_Const(ph7_value *pVal,void *pUserData)` |
|      35 | 2206 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_MSGQUEUE); }` |
|       - | 2207 | `#endif` |
|       - | 2208 | `#ifdef RLIMIT_NICE` |
|      35 | 2209 | `static void PH7_POSIX_RLIMIT_NICE_Const(ph7_value *pVal,void *pUserData)` |
|      35 | 2210 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_NICE); }` |
|       - | 2211 | `#endif` |
|       - | 2212 | `#ifdef RLIMIT_NOFILE` |
|      70 | 2213 | `static void PH7_POSIX_RLIMIT_NOFILE_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2214 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_NOFILE); }` |
|       - | 2215 | `#endif` |
|       - | 2216 | `#ifdef RLIMIT_NPROC` |
|      70 | 2217 | `static void PH7_POSIX_RLIMIT_NPROC_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2218 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_NPROC); }` |
|       - | 2219 | `#endif` |
|       - | 2220 | `#ifdef RLIMIT_RSS` |
|      70 | 2221 | `static void PH7_POSIX_RLIMIT_RSS_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2222 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_RSS); }` |
|       - | 2223 | `#endif` |
|       - | 2224 | `#ifdef RLIMIT_RTPRIO` |
|      35 | 2225 | `static void PH7_POSIX_RLIMIT_RTPRIO_Const(ph7_value *pVal,void *pUserData)` |
|      35 | 2226 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_RTPRIO); }` |
|       - | 2227 | `#endif` |
|       - | 2228 | `#ifdef RLIMIT_RTTIME` |
|      35 | 2229 | `static void PH7_POSIX_RLIMIT_RTTIME_Const(ph7_value *pVal,void *pUserData)` |
|      35 | 2230 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_RTTIME); }` |
|       - | 2231 | `#endif` |
|       - | 2232 | `#ifdef RLIMIT_SIGPENDING` |
|      35 | 2233 | `static void PH7_POSIX_RLIMIT_SIGPENDING_Const(ph7_value *pVal,void *pUserData)` |
|      35 | 2234 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_SIGPENDING); }` |
|       - | 2235 | `#endif` |
|       - | 2236 | `#ifdef RLIMIT_STACK` |
|      70 | 2237 | `static void PH7_POSIX_RLIMIT_STACK_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2238 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_STACK); }` |
|       - | 2239 | `#endif` |
|       - | 2240 | `#ifdef _SC_ARG_MAX` |
|      70 | 2241 | `static void PH7_POSIX_SC_ARG_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2242 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_ARG_MAX); }` |
|       - | 2243 | `#endif` |
|       - | 2244 | `#ifdef _SC_CHILD_MAX` |
|      70 | 2245 | `static void PH7_POSIX_SC_CHILD_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2246 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_CHILD_MAX); }` |
|       - | 2247 | `#endif` |
|       - | 2248 | `#ifdef _SC_CLK_TCK` |
|      71 | 2249 | `static void PH7_POSIX_SC_CLK_TCK_Const(ph7_value *pVal,void *pUserData)` |
|      71 | 2250 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_CLK_TCK); }` |
|       - | 2251 | `#endif` |
|       - | 2252 | `#ifdef _SC_OPEN_MAX` |
|      70 | 2253 | `static void PH7_POSIX_SC_OPEN_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2254 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_OPEN_MAX); }` |
|       - | 2255 | `#endif` |
|       - | 2256 | `#ifdef _SC_PAGESIZE` |
|      71 | 2257 | `static void PH7_POSIX_SC_PAGESIZE_Const(ph7_value *pVal,void *pUserData)` |
|      71 | 2258 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_PAGESIZE); }` |
|       - | 2259 | `#endif` |
|       - | 2260 | `#ifdef _SC_NPROCESSORS_CONF` |
|      70 | 2261 | `static void PH7_POSIX_SC_NPROCESSORS_CONF_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2262 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_NPROCESSORS_CONF); }` |
|       - | 2263 | `#endif` |
|       - | 2264 | `#ifdef _SC_NPROCESSORS_ONLN` |
|      70 | 2265 | `static void PH7_POSIX_SC_NPROCESSORS_ONLN_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2266 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_NPROCESSORS_ONLN); }` |
|       - | 2267 | `#endif` |
|       - | 2268 | `#ifdef _PC_LINK_MAX` |
|      70 | 2269 | `static void PH7_POSIX_PC_LINK_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2270 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_LINK_MAX); }` |
|       - | 2271 | `#endif` |
|       - | 2272 | `#ifdef _PC_MAX_CANON` |
|      70 | 2273 | `static void PH7_POSIX_PC_MAX_CANON_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2274 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_MAX_CANON); }` |
|       - | 2275 | `#endif` |
|       - | 2276 | `#ifdef _PC_MAX_INPUT` |
|      70 | 2277 | `static void PH7_POSIX_PC_MAX_INPUT_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2278 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_MAX_INPUT); }` |
|       - | 2279 | `#endif` |
|       - | 2280 | `#ifdef _PC_NAME_MAX` |
|      76 | 2281 | `static void PH7_POSIX_PC_NAME_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      76 | 2282 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_NAME_MAX); }` |
|       - | 2283 | `#endif` |
|       - | 2284 | `#ifdef _PC_PATH_MAX` |
|      72 | 2285 | `static void PH7_POSIX_PC_PATH_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      72 | 2286 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_PATH_MAX); }` |
|       - | 2287 | `#endif` |
|       - | 2288 | `#ifdef _PC_PIPE_BUF` |
|      70 | 2289 | `static void PH7_POSIX_PC_PIPE_BUF_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2290 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_PIPE_BUF); }` |
|       - | 2291 | `#endif` |
|       - | 2292 | `#ifdef _PC_CHOWN_RESTRICTED` |
|      70 | 2293 | `static void PH7_POSIX_PC_CHOWN_RESTRICTED_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2294 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_CHOWN_RESTRICTED); }` |
|       - | 2295 | `#endif` |
|       - | 2296 | `#ifdef _PC_NO_TRUNC` |
|      70 | 2297 | `static void PH7_POSIX_PC_NO_TRUNC_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2298 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_NO_TRUNC); }` |
|       - | 2299 | `#endif` |
|       - | 2300 | `#ifdef _PC_ALLOC_SIZE_MIN` |
|      70 | 2301 | `static void PH7_POSIX_PC_ALLOC_SIZE_MIN_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2302 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_ALLOC_SIZE_MIN); }` |
|       - | 2303 | `#endif` |
|       - | 2304 | `#ifdef _PC_SYMLINK_MAX` |
|      70 | 2305 | `static void PH7_POSIX_PC_SYMLINK_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2306 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_SYMLINK_MAX); }` |
|       - | 2307 | `#endif` |
|       - | 2308 | `/*` |
|       - | 2309 | ` * RLIM_INFINITY is a rlim_t, which is unsigned; php answers it as -1, which is` |
|       - | 2310 | ` * what posix_setrlimit() takes back for "no limit".` |
|       - | 2311 | ` */` |
|      70 | 2312 | `static void PH7_POSIX_RLIMIT_INFINITY_Const(ph7_value *pVal,void *pUserData)` |
|      70 | 2313 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,-1); }` |
|       - | 2314 | `#endif /* __WINNT__ */` |
|       - | 2315 | `/*` |
|       - | 2316 | ` * SEEK_SET.` |
|       - | 2317 | ` *  Expand 0` |
|       - | 2318 | ` */` |
|      97 | 2319 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2320 | `{` |
|      48 | 2321 | `	SXUNUSED(pUserData); /* cc warning */` |
|     101 | 2322 | `	ph7_value_int(pVal,0);` |
|     101 | 2323 | `}` |
|       - | 2324 | `/*` |
|       - | 2325 | ` * SEEK_CUR.` |
|       - | 2326 | ` *  Expand 1` |
|       - | 2327 | ` */` |
|      91 | 2328 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2329 | `{` |
|      45 | 2330 | `	SXUNUSED(pUserData); /* cc warning */` |
|      96 | 2331 | `	ph7_value_int(pVal,1);` |
|      96 | 2332 | `}` |
|       - | 2333 | `/*` |
|       - | 2334 | ` * SEEK_END.` |
|       - | 2335 | ` *  Expand 2` |
|       - | 2336 | ` */` |
|      93 | 2337 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2338 | `{` |
|      46 | 2339 | `	SXUNUSED(pUserData); /* cc warning */` |
|      98 | 2340 | `	ph7_value_int(pVal,2);` |
|      98 | 2341 | `}` |
|       - | 2342 | `/*` |
|       - | 2343 | ` * LOCK_SH.` |
|       - | 2344 | ` *  Expand 2` |
|       - | 2345 | ` */` |
|      87 | 2346 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2347 | `{` |
|      43 | 2348 | `	SXUNUSED(pUserData); /* cc warning */` |
|      91 | 2349 | `	ph7_value_int(pVal,1);` |
|      91 | 2350 | `}` |
|       - | 2351 | `/*` |
|       - | 2352 | ` * LOCK_NB.` |
|       - | 2353 | ` *  Expand 4 (php)` |
|       - | 2354 | ` */` |
|      91 | 2355 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2356 | `{` |
|      45 | 2357 | `	SXUNUSED(pUserData); /* cc warning */` |
|      94 | 2358 | `	ph7_value_int(pVal,4);` |
|      94 | 2359 | `}` |
|       - | 2360 | `/*` |
|       - | 2361 | ` * LOCK_EX.` |
|       - | 2362 | ` *  Expand 2 (php). PH7 used 1, which collided with LOCK_SH, and LOCK_UN was 0 — so` |
|       - | 2363 | ` *  flock($h, LOCK_UN) asked the stream for a SHARED lock instead of releasing one.` |
|       - | 2364 | ` */` |
|      89 | 2365 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2366 | `{` |
|      44 | 2367 | `	SXUNUSED(pUserData); /* cc warning */` |
|      92 | 2368 | `	ph7_value_int(pVal,2);` |
|      92 | 2369 | `}` |
|       - | 2370 | `/*` |
|       - | 2371 | ` * LOCK_UN.` |
|       - | 2372 | ` *  Expand 3 (php)` |
|       - | 2373 | ` */` |
|      83 | 2374 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2375 | `{` |
|      41 | 2376 | `	SXUNUSED(pUserData); /* cc warning */` |
|      86 | 2377 | `	ph7_value_int(pVal,3);` |
|      86 | 2378 | `}` |
|       - | 2379 | `/*` |
|       - | 2380 | ` * FILE_USE_INCLUDE_PATH` |
|       - | 2381 | ` *  Expand 0x01 (Must be a power of two)` |
|       - | 2382 | ` */` |
|      75 | 2383 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2384 | `{` |
|      37 | 2385 | `	SXUNUSED(pUserData); /* cc warning */` |
|      78 | 2386 | `	ph7_value_int(pVal,0x1);` |
|      78 | 2387 | `}` |
|       - | 2388 | `/*` |
|       - | 2389 | ` * FILE_IGNORE_NEW_LINES` |
|       - | 2390 | ` *  Expand 0x02 (Must be a power of two)` |
|       - | 2391 | ` */` |
|     195 | 2392 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2393 | `{` |
|      97 | 2394 | `	SXUNUSED(pUserData); /* cc warning */` |
|     198 | 2395 | `	ph7_value_int(pVal,0x2);` |
|     198 | 2396 | `}` |
|       - | 2397 | `/*` |
|       - | 2398 | ` * FILE_SKIP_EMPTY_LINES` |
|       - | 2399 | ` *  Expand 0x04 (Must be a power of two)` |
|       - | 2400 | ` */` |
|     187 | 2401 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2402 | `{` |
|      93 | 2403 | `	SXUNUSED(pUserData); /* cc warning */` |
|     190 | 2404 | `	ph7_value_int(pVal,0x4);` |
|     190 | 2405 | `}` |
|       - | 2406 | `/*` |
|       - | 2407 | ` * FILE_APPEND` |
|       - | 2408 | ` *  Expand 0x08 (Must be a power of two)` |
|       - | 2409 | ` */` |
|      77 | 2410 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2411 | `{` |
|      38 | 2412 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 2413 | `	ph7_value_int(pVal,0x08);` |
|      80 | 2414 | `}` |
|       - | 2415 | `/*` |
|       - | 2416 | ` * FILE_NO_DEFAULT_CONTEXT` |
|       - | 2417 | ` *  Expand php's 0x10. file()/file_put_contents() read it: it is what stops the` |
|       - | 2418 | `` *  `$context = null` argument from resolving to stream_context_get_default()'s`` |
|       - | 2419 | ` *  context. What a device then does with an open carrying no context is its own` |
|       - | 2420 | ` *  business — a userland wrapper's $this->context is a resource either way, in` |
|       - | 2421 | ` *  php as here — so the flag is only observable where an option is consumed.` |
|       - | 2422 | ` */` |
|      79 | 2423 | `static void PH7_FILE_NO_DEFAULT_CONTEXT_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2424 | `{` |
|      39 | 2425 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 2426 | `	ph7_value_int(pVal,0x10);` |
|      83 | 2427 | `}` |
|       - | 2428 | `/*` |
|       - | 2429 | ` * SCANDIR_SORT_ASCENDING` |
|       - | 2430 | ` *  Expand 0` |
|       - | 2431 | ` */` |
|    2971 | 2432 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2433 | `{` |
|    1481 | 2434 | `	SXUNUSED(pUserData); /* cc warning */` |
|    2976 | 2435 | `	ph7_value_int(pVal,0);` |
|    2976 | 2436 | `}` |
|       - | 2437 | `/*` |
|       - | 2438 | ` * SCANDIR_SORT_DESCENDING` |
|       - | 2439 | ` *  Expand 1` |
|       - | 2440 | ` */` |
|      79 | 2441 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2442 | `{` |
|      39 | 2443 | `	SXUNUSED(pUserData); /* cc warning */` |
|      82 | 2444 | `	ph7_value_int(pVal,1);` |
|      82 | 2445 | `}` |
|       - | 2446 | `/*` |
|       - | 2447 | ` * SCANDIR_SORT_NONE` |
|       - | 2448 | ` *  Expand 2` |
|       - | 2449 | ` */` |
|    1529 | 2450 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2451 | `{` |
|     762 | 2452 | `	SXUNUSED(pUserData); /* cc warning */` |
|    1534 | 2453 | `	ph7_value_int(pVal,2);` |
|    1534 | 2454 | `}` |
|       - | 2455 | `/*` |
|       - | 2456 | ` * GLOB_MARK` |
|       - | 2457 | ` *  Expand php's 0x08 (php's own portable glob flag set)` |
|       - | 2458 | ` */` |
|     389 | 2459 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2460 | `{` |
|     194 | 2461 | `	SXUNUSED(pUserData); /* cc warning */` |
|     392 | 2462 | `	ph7_value_int(pVal,PH7_GLOB_MARK);` |
|     392 | 2463 | `}` |
|       - | 2464 | `/*` |
|       - | 2465 | ` * GLOB_NOSORT` |
|       - | 2466 | ` *  Expand php's 0x20` |
|       - | 2467 | ` */` |
|     576 | 2468 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2469 | `{` |
|     287 | 2470 | `	SXUNUSED(pUserData); /* cc warning */` |
|     581 | 2471 | `	ph7_value_int(pVal,PH7_GLOB_NOSORT);` |
|     581 | 2472 | `}` |
|       - | 2473 | `/*` |
|       - | 2474 | ` * GLOB_NOCHECK` |
|       - | 2475 | ` *  Expand php's 0x10` |
|       - | 2476 | ` */` |
|     724 | 2477 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2478 | `{` |
|     361 | 2479 | `	SXUNUSED(pUserData); /* cc warning */` |
|     729 | 2480 | `	ph7_value_int(pVal,PH7_GLOB_NOCHECK);` |
|     729 | 2481 | `}` |
|       - | 2482 | `/*` |
|       - | 2483 | ` * GLOB_NOESCAPE` |
|       - | 2484 | ` *  Expand php's 0x1000` |
|       - | 2485 | ` */` |
|      77 | 2486 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2487 | `{` |
|      38 | 2488 | `	SXUNUSED(pUserData); /* cc warning */` |
|      80 | 2489 | `	ph7_value_int(pVal,PH7_GLOB_NOESCAPE);` |
|      80 | 2490 | `}` |
|       - | 2491 | `/*` |
|       - | 2492 | ` * GLOB_BRACE` |
|       - | 2493 | ` *  Expand php's 0x80` |
|       - | 2494 | ` */` |
|     596 | 2495 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2496 | `{` |
|     297 | 2497 | `	SXUNUSED(pUserData); /* cc warning */` |
|     601 | 2498 | `	ph7_value_int(pVal,PH7_GLOB_BRACE);` |
|     601 | 2499 | `}` |
|       - | 2500 | `/*` |
|       - | 2501 | ` * GLOB_ONLYDIR` |
|       - | 2502 | ` *  Expand php's 0x40000000` |
|       - | 2503 | ` */` |
|     887 | 2504 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2505 | `{` |
|     441 | 2506 | `	SXUNUSED(pUserData); /* cc warning */` |
|     892 | 2507 | `	ph7_value_int(pVal,PH7_GLOB_ONLYDIR);` |
|     892 | 2508 | `}` |
|       - | 2509 | `/*` |
|       - | 2510 | ` * GLOB_ERR` |
|       - | 2511 | ` *  Expand php's 0x04` |
|       - | 2512 | ` */` |
|      81 | 2513 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2514 | `{` |
|      40 | 2515 | `	SXUNUSED(pUserData); /* cc warning */` |
|      84 | 2516 | `	ph7_value_int(pVal,PH7_GLOB_ERR);` |
|      84 | 2517 | `}` |
|       - | 2518 | `/*` |
|       - | 2519 | ` * GLOB_AVAILABLE_FLAGS` |
|       - | 2520 | ` *  Expand the OR of every glob flag php's portable glob accepts — the mask` |
|       - | 2521 | ` *  glob() itself validates against (1073746108 on every platform, since the` |
|       - | 2522 | ` *  GLOB_* values are php 8.5's own portable set).` |
|       - | 2523 | ` */` |
|     592 | 2524 | `static void PH7_GLOB_AVAILABLE_FLAGS_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2525 | `{` |
|     295 | 2526 | `	SXUNUSED(pUserData); /* cc warning */` |
|     597 | 2527 | `	ph7_value_int(pVal,PH7_GLOB_ERR\|PH7_GLOB_MARK\|PH7_GLOB_NOCHECK\|PH7_GLOB_NOSORT` |
|       - | 2528 | `		\|PH7_GLOB_BRACE\|PH7_GLOB_NOESCAPE\|PH7_GLOB_ONLYDIR);` |
|     597 | 2529 | `}` |
|       - | 2530 | `/*` |
|       - | 2531 | ` * STDIN` |
|       - | 2532 | ` *  Expand the STDIN handle as a resource.` |
|       - | 2533 | ` */` |
|     164 | 2534 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2535 | `{` |
|     168 | 2536 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 2537 | `	void *pResource;` |
|     168 | 2538 | `	pResource = PH7_ExportStdin(pVm);` |
|     168 | 2539 | `	ph7_value_resource(pVal,pResource);` |
|     168 | 2540 | `}` |
|       - | 2541 | `/*` |
|       - | 2542 | ` * STDOUT` |
|       - | 2543 | ` *   Expand the STDOUT handle as a resource.` |
|       - | 2544 | ` */` |
|     157 | 2545 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2546 | `{` |
|     161 | 2547 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 2548 | `	void *pResource;` |
|     161 | 2549 | `	pResource = PH7_ExportStdout(pVm);` |
|     161 | 2550 | `	ph7_value_resource(pVal,pResource);` |
|     161 | 2551 | `}` |
|       - | 2552 | `/*` |
|       - | 2553 | ` * STDERR` |
|       - | 2554 | ` *  Expand the STDERR handle as a resource.` |
|       - | 2555 | ` */` |
|     199 | 2556 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2557 | `{` |
|     203 | 2558 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 2559 | `	void *pResource;` |
|     203 | 2560 | `	pResource = PH7_ExportStderr(pVm);` |
|     203 | 2561 | `	ph7_value_resource(pVal,pResource);` |
|     203 | 2562 | `}` |
|       - | 2563 | `/*` |
|       - | 2564 | ` * INI_SCANNER_NORMAL` |
|       - | 2565 | ` *   Expand php's 0` |
|       - | 2566 | ` */` |
|     399 | 2567 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2568 | `{` |
|     199 | 2569 | `	SXUNUSED(pUserData); /* cc warning */` |
|     404 | 2570 | `	ph7_value_int(pVal,PH7_INI_SCANNER_NORMAL);` |
|     404 | 2571 | `}` |
|       - | 2572 | `/*` |
|       - | 2573 | ` * INI_SCANNER_RAW` |
|       - | 2574 | ` *   Expand php's 1` |
|       - | 2575 | ` */` |
|      91 | 2576 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2577 | `{` |
|      45 | 2578 | `	SXUNUSED(pUserData); /* cc warning */` |
|      96 | 2579 | `	ph7_value_int(pVal,PH7_INI_SCANNER_RAW);` |
|      96 | 2580 | `}` |
|       - | 2581 | `/*` |
|       - | 2582 | ` * INI_SCANNER_TYPED` |
|       - | 2583 | ` *   Expand 2 (php's value)` |
|       - | 2584 | ` */` |
|     195 | 2585 | `static void PH7_INI_SCANNER_TYPED_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2586 | `{` |
|      97 | 2587 | `	SXUNUSED(pUserData); /* cc warning */` |
|     199 | 2588 | `	ph7_value_int(pVal,PH7_INI_SCANNER_TYPED);` |
|     199 | 2589 | `}` |
|       - | 2590 | `/*` |
|       - | 2591 | ` * EXTR_OVERWRITE` |
|       - | 2592 | ` *   Expand 0 (php's enum value; see PH7_EXTR_* in ph7int.h)` |
|       - | 2593 | ` */` |
|      85 | 2594 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2595 | `{` |
|      42 | 2596 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 2597 | `	ph7_value_int(pVal,PH7_EXTR_OVERWRITE);` |
|      88 | 2598 | `}` |
|       - | 2599 | `/*` |
|       - | 2600 | ` * EXTR_SKIP` |
|       - | 2601 | ` *   Expand 1` |
|       - | 2602 | ` */` |
|      87 | 2603 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2604 | `{` |
|      43 | 2605 | `	SXUNUSED(pUserData); /* cc warning */` |
|      91 | 2606 | `	ph7_value_int(pVal,PH7_EXTR_SKIP);` |
|      91 | 2607 | `}` |
|       - | 2608 | `/*` |
|       - | 2609 | ` * EXTR_PREFIX_SAME` |
|       - | 2610 | ` *   Expand 2` |
|       - | 2611 | ` */` |
|     109 | 2612 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2613 | `{` |
|      54 | 2614 | `	SXUNUSED(pUserData); /* cc warning */` |
|     113 | 2615 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_SAME);` |
|     113 | 2616 | `}` |
|       - | 2617 | `/*` |
|       - | 2618 | ` * EXTR_PREFIX_ALL` |
|       - | 2619 | ` *   Expand 3` |
|       - | 2620 | ` */` |
|      99 | 2621 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2622 | `{` |
|      49 | 2623 | `	SXUNUSED(pUserData); /* cc warning */` |
|     103 | 2624 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_ALL);` |
|     103 | 2625 | `}` |
|       - | 2626 | `/*` |
|       - | 2627 | ` * EXTR_PREFIX_INVALID` |
|       - | 2628 | ` *   Expand 4` |
|       - | 2629 | ` */` |
|      79 | 2630 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2631 | `{` |
|      39 | 2632 | `	SXUNUSED(pUserData); /* cc warning */` |
|      82 | 2633 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_INVALID);` |
|      82 | 2634 | `}` |
|       - | 2635 | `/*` |
|       - | 2636 | ` * EXTR_IF_EXISTS` |
|       - | 2637 | ` *   Expand 6 (php orders IF_EXISTS after PREFIX_IF_EXISTS)` |
|       - | 2638 | ` */` |
|      87 | 2639 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2640 | `{` |
|      43 | 2641 | `	SXUNUSED(pUserData); /* cc warning */` |
|      92 | 2642 | `	ph7_value_int(pVal,PH7_EXTR_IF_EXISTS);` |
|      92 | 2643 | `}` |
|       - | 2644 | `/*` |
|       - | 2645 | ` * EXTR_REFS` |
|       - | 2646 | ` *   Expand 256 (the bit that rides above the mode: bind by REFERENCE)` |
|       - | 2647 | ` */` |
|      85 | 2648 | `static void PH7_EXTR_REFS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2649 | `{` |
|      42 | 2650 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 2651 | `	ph7_value_int(pVal,PH7_EXTR_REFS);` |
|      88 | 2652 | `}` |
|       - | 2653 | `/*` |
|       - | 2654 | ` * EXTR_PREFIX_IF_EXISTS` |
|       - | 2655 | ` *   Expand 5` |
|       - | 2656 | ` */` |
|      91 | 2657 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2658 | `{` |
|      45 | 2659 | `	SXUNUSED(pUserData); /* cc warning */` |
|      95 | 2660 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_IF_EXISTS);` |
|      95 | 2661 | `}` |
|       - | 2662 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|       - | 2663 | `/*` |
|       - | 2664 | ` * HASH_HMAC.` |
|       - | 2665 | ` *   php's one hash_init() flag. Declared with the hash extension it belongs` |
|       - | 2666 | ` *   to, so a build without that extension has no constant either.` |
|       - | 2667 | ` */` |
|      82 | 2668 | `static void PH7_HASH_HMAC_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2669 | `{` |
|      41 | 2670 | `	SXUNUSED(pUserData); /* cc warning */` |
|      85 | 2671 | `	ph7_value_int(pVal,PH7_HASH_HMAC);` |
|      85 | 2672 | `}` |
|       - | 2673 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|       - | 2674 | `/*` |
|       - | 2675 | ` * ICONV_* — what the converter IS, and iconv_mime_decode()'s $mode bits.` |
|       - | 2676 | `` * php reports the C library behind its extension here (`glibc`, `libiconv`);`` |
|       - | 2677 | ` * PHL converts with its own code so that a Windows build answers what a POSIX` |
|       - | 2678 | ` * one does, and says so — the constants exist to be READ, and a program that` |
|       - | 2679 | ` * branches on them has to see something true.` |
|       - | 2680 | ` */` |
|      72 | 2681 | `static void PH7_ICONV_IMPL_Const(ph7_value *pVal,void *pUnused)` |
|       4 | 2682 | `{` |
|      36 | 2683 | `	SXUNUSED(pUnused);` |
|      76 | 2684 | `	ph7_value_string(pVal,"PHL",(int)sizeof("PHL")-1);` |
|      76 | 2685 | `}` |
|      72 | 2686 | `static void PH7_ICONV_VERSION_Const(ph7_value *pVal,void *pUnused)` |
|       4 | 2687 | `{` |
|      36 | 2688 | `	SXUNUSED(pUnused);` |
|      76 | 2689 | `	ph7_value_string(pVal,PH7_VERSION,(int)sizeof(PH7_VERSION)-1);` |
|      76 | 2690 | `}` |
|      74 | 2691 | `static void PH7_ICONV_MIME_DECODE_STRICT_Const(ph7_value *pVal,void *pUnused)` |
|       4 | 2692 | `{` |
|      37 | 2693 | `	SXUNUSED(pUnused);` |
|      78 | 2694 | `	ph7_value_int(pVal,1);` |
|      78 | 2695 | `}` |
|      74 | 2696 | `static void PH7_ICONV_MIME_DECODE_CONTINUE_ON_ERROR_Const(ph7_value *pVal,void *pUnused)` |
|       4 | 2697 | `{` |
|      37 | 2698 | `	SXUNUSED(pUnused);` |
|      78 | 2699 | `	ph7_value_int(pVal,2);` |
|      78 | 2700 | `}` |
|       - | 2701 | `/*` |
|       - | 2702 | ` * JSON_HEX_TAG.` |
|       - | 2703 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|       - | 2704 | ` */` |
|      84 | 2705 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2706 | `{` |
|      42 | 2707 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 2708 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      87 | 2709 | `}` |
|       - | 2710 | `/*` |
|       - | 2711 | ` * JSON_HEX_AMP.` |
|       - | 2712 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|       - | 2713 | ` */` |
|      82 | 2714 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2715 | `{` |
|      41 | 2716 | `	SXUNUSED(pUserData); /* cc warning */` |
|      85 | 2717 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      85 | 2718 | `}` |
|       - | 2719 | `/*` |
|       - | 2720 | ` * JSON_HEX_APOS.` |
|       - | 2721 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|       - | 2722 | ` */` |
|      82 | 2723 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2724 | `{` |
|      41 | 2725 | `	SXUNUSED(pUserData); /* cc warning */` |
|      85 | 2726 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      85 | 2727 | `}` |
|       - | 2728 | `/*` |
|       - | 2729 | ` * JSON_HEX_QUOT.` |
|       - | 2730 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|       - | 2731 | ` */` |
|      82 | 2732 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2733 | `{` |
|      41 | 2734 | `	SXUNUSED(pUserData); /* cc warning */` |
|      85 | 2735 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      85 | 2736 | `}` |
|       - | 2737 | `/*` |
|       - | 2738 | ` * JSON_FORCE_OBJECT.` |
|       - | 2739 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|       - | 2740 | ` */` |
|      82 | 2741 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2742 | `{` |
|      41 | 2743 | `	SXUNUSED(pUserData); /* cc warning */` |
|      85 | 2744 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      85 | 2745 | `}` |
|       - | 2746 | `/*` |
|       - | 2747 | ` * JSON_NUMERIC_CHECK.` |
|       - | 2748 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|       - | 2749 | ` */` |
|      86 | 2750 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2751 | `{` |
|      43 | 2752 | `	SXUNUSED(pUserData); /* cc warning */` |
|      89 | 2753 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      89 | 2754 | `}` |
|       - | 2755 | `/*` |
|       - | 2756 | ` * JSON_BIGINT_AS_STRING.` |
|       - | 2757 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|       - | 2758 | ` */` |
|      90 | 2759 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2760 | `{` |
|      45 | 2761 | `	SXUNUSED(pUserData); /* cc warning */` |
|      93 | 2762 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      93 | 2763 | `}` |
|       - | 2764 | `/*` |
|       - | 2765 | ` * JSON_PARTIAL_OUTPUT_ON_ERROR.` |
|       - | 2766 | ` *   Expand the value of JSON_PARTIAL_OUTPUT_ON_ERROR defined in ph7Int.h.` |
|       - | 2767 | ` */` |
|     100 | 2768 | `static void PH7_JSON_PARTIAL_OUTPUT_ON_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2769 | `{` |
|      50 | 2770 | `	SXUNUSED(pUserData); /* cc warning */` |
|     103 | 2771 | `	ph7_value_int(pVal,JSON_PARTIAL_OUTPUT_ON_ERROR);` |
|     103 | 2772 | `}` |
|       - | 2773 | `/*` |
|       - | 2774 | ` * JSON_PRESERVE_ZERO_FRACTION.` |
|       - | 2775 | ` *   Expand the value of JSON_PRESERVE_ZERO_FRACTION defined in ph7Int.h.` |
|       - | 2776 | ` */` |
|      90 | 2777 | `static void PH7_JSON_PRESERVE_ZERO_FRACTION_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2778 | `{` |
|      45 | 2779 | `	SXUNUSED(pUserData); /* cc warning */` |
|      93 | 2780 | `	ph7_value_int(pVal,JSON_PRESERVE_ZERO_FRACTION);` |
|      93 | 2781 | `}` |
|       - | 2782 | `/*` |
|       - | 2783 | ` * JSON_OBJECT_AS_ARRAY.` |
|       - | 2784 | ` *   Expand the value of JSON_OBJECT_AS_ARRAY defined in ph7Int.h.` |
|       - | 2785 | ` */` |
|      86 | 2786 | `static void PH7_JSON_OBJECT_AS_ARRAY_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2787 | `{` |
|      43 | 2788 | `	SXUNUSED(pUserData); /* cc warning */` |
|      89 | 2789 | `	ph7_value_int(pVal,JSON_OBJECT_AS_ARRAY);` |
|      89 | 2790 | `}` |
|       - | 2791 | `/*` |
|       - | 2792 | ` * JSON_PRETTY_PRINT.` |
|       - | 2793 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|       - | 2794 | ` */` |
|      90 | 2795 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2796 | `{` |
|      45 | 2797 | `	SXUNUSED(pUserData); /* cc warning */` |
|      93 | 2798 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      93 | 2799 | `}` |
|       - | 2800 | `/*` |
|       - | 2801 | ` * JSON_UNESCAPED_SLASHES.` |
|       - | 2802 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|       - | 2803 | ` */` |
|      86 | 2804 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2805 | `{` |
|      43 | 2806 | `	SXUNUSED(pUserData); /* cc warning */` |
|      89 | 2807 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      89 | 2808 | `}` |
|       - | 2809 | `/*` |
|       - | 2810 | ` * JSON_UNESCAPED_UNICODE.` |
|       - | 2811 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|       - | 2812 | ` */` |
|     592 | 2813 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2814 | `{` |
|     296 | 2815 | `	SXUNUSED(pUserData); /* cc warning */` |
|     595 | 2816 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|     595 | 2817 | `}` |
|       - | 2818 | `/*` |
|       - | 2819 | ` * JSON_UNESCAPED_LINE_TERMINATORS.` |
|       - | 2820 | ` *   Expand the value of JSON_UNESCAPED_LINE_TERMINATORS defined in ph7Int.h.` |
|       - | 2821 | ` */` |
|      82 | 2822 | `static void PH7_JSON_UNESCAPED_LINE_TERMINATORS_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2823 | `{` |
|      41 | 2824 | `	SXUNUSED(pUserData); /* cc warning */` |
|      85 | 2825 | `	ph7_value_int(pVal,JSON_UNESCAPED_LINE_TERMINATORS);` |
|      85 | 2826 | `}` |
|       - | 2827 | `/*` |
|       - | 2828 | ` * JSON_INVALID_UTF8_IGNORE.` |
|       - | 2829 | ` *   Expand the value of JSON_INVALID_UTF8_IGNORE defined in ph7Int.h.` |
|       - | 2830 | ` */` |
|     102 | 2831 | `static void PH7_JSON_INVALID_UTF8_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|       4 | 2832 | `{` |
|      51 | 2833 | `	SXUNUSED(pUserData); /* cc warning */` |
|     106 | 2834 | `	ph7_value_int(pVal,JSON_INVALID_UTF8_IGNORE);` |
|     106 | 2835 | `}` |
|       - | 2836 | `/*` |
|       - | 2837 | ` * JSON_INVALID_UTF8_SUBSTITUTE.` |
|       - | 2838 | ` *   Expand the value of JSON_INVALID_UTF8_SUBSTITUTE defined in ph7Int.h.` |
|       - | 2839 | ` */` |
|     106 | 2840 | `static void PH7_JSON_INVALID_UTF8_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2841 | `{` |
|      53 | 2842 | `	SXUNUSED(pUserData); /* cc warning */` |
|     109 | 2843 | `	ph7_value_int(pVal,JSON_INVALID_UTF8_SUBSTITUTE);` |
|     109 | 2844 | `}` |
|       - | 2845 | `/*` |
|       - | 2846 | ` * JSON_THROW_ON_ERROR.` |
|       - | 2847 | ` *   Expand the value of JSON_THROW_ON_ERROR defined in ph7Int.h.` |
|       - | 2848 | ` */` |
|      96 | 2849 | `static void PH7_JSON_THROW_ON_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2850 | `{` |
|      48 | 2851 | `	SXUNUSED(pUserData); /* cc warning */` |
|      99 | 2852 | `	ph7_value_int(pVal,JSON_THROW_ON_ERROR);` |
|      99 | 2853 | `}` |
|       - | 2854 | `/*` |
|       - | 2855 | ` * JSON_ERROR_NONE.` |
|       - | 2856 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|       - | 2857 | ` */` |
|      80 | 2858 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2859 | `{` |
|      40 | 2860 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 2861 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      83 | 2862 | `}` |
|       - | 2863 | `/*` |
|       - | 2864 | ` * JSON_ERROR_DEPTH.` |
|       - | 2865 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|       - | 2866 | ` */` |
|      78 | 2867 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2868 | `{` |
|      39 | 2869 | `	SXUNUSED(pUserData); /* cc warning */` |
|      81 | 2870 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      81 | 2871 | `}` |
|       - | 2872 | `/*` |
|       - | 2873 | ` * JSON_ERROR_STATE_MISMATCH.` |
|       - | 2874 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|       - | 2875 | ` */` |
|      78 | 2876 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2877 | `{` |
|      39 | 2878 | `	SXUNUSED(pUserData); /* cc warning */` |
|      81 | 2879 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      81 | 2880 | `}` |
|       - | 2881 | `/*` |
|       - | 2882 | ` * JSON_ERROR_CTRL_CHAR.` |
|       - | 2883 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|       - | 2884 | ` */` |
|      78 | 2885 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2886 | `{` |
|      39 | 2887 | `	SXUNUSED(pUserData); /* cc warning */` |
|      81 | 2888 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      81 | 2889 | `}` |
|       - | 2890 | `/*` |
|       - | 2891 | ` * JSON_ERROR_SYNTAX.` |
|       - | 2892 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|       - | 2893 | ` */` |
|      80 | 2894 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2895 | `{` |
|      40 | 2896 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 2897 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      83 | 2898 | `}` |
|       - | 2899 | `/*` |
|       - | 2900 | ` * JSON_ERROR_UTF8.` |
|       - | 2901 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|       - | 2902 | ` */` |
|      78 | 2903 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2904 | `{` |
|      39 | 2905 | `	SXUNUSED(pUserData); /* cc warning */` |
|      81 | 2906 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      81 | 2907 | `}` |
|       - | 2908 | `/*` |
|       - | 2909 | ` * JSON_ERROR_RECURSION.` |
|       - | 2910 | ` *   Expand the value of JSON_ERROR_RECURSION defined in ph7Int.h.` |
|       - | 2911 | ` */` |
|      78 | 2912 | `static void PH7_JSON_ERROR_RECURSION_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2913 | `{` |
|      39 | 2914 | `	SXUNUSED(pUserData); /* cc warning */` |
|      81 | 2915 | `	ph7_value_int(pVal,JSON_ERROR_RECURSION);` |
|      81 | 2916 | `}` |
|       - | 2917 | `/*` |
|       - | 2918 | ` * JSON_ERROR_UNSUPPORTED_TYPE.` |
|       - | 2919 | ` *   Expand the value of JSON_ERROR_UNSUPPORTED_TYPE defined in ph7Int.h.` |
|       - | 2920 | ` */` |
|      78 | 2921 | `static void PH7_JSON_ERROR_UNSUPPORTED_TYPE_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2922 | `{` |
|      39 | 2923 | `	SXUNUSED(pUserData); /* cc warning */` |
|      81 | 2924 | `	ph7_value_int(pVal,JSON_ERROR_UNSUPPORTED_TYPE);` |
|      81 | 2925 | `}` |
|       - | 2926 | `/*` |
|       - | 2927 | ` * JSON_ERROR_INVALID_PROPERTY_NAME.` |
|       - | 2928 | ` *   Expand the value of JSON_ERROR_INVALID_PROPERTY_NAME defined in ph7Int.h.` |
|       - | 2929 | ` */` |
|      78 | 2930 | `static void PH7_JSON_ERROR_INVALID_PROPERTY_NAME_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2931 | `{` |
|      39 | 2932 | `	SXUNUSED(pUserData); /* cc warning */` |
|      81 | 2933 | `	ph7_value_int(pVal,JSON_ERROR_INVALID_PROPERTY_NAME);` |
|      81 | 2934 | `}` |
|       - | 2935 | `/*` |
|       - | 2936 | ` * JSON_ERROR_UTF16.` |
|       - | 2937 | ` *   Expand the value of JSON_ERROR_UTF16 defined in ph7Int.h.` |
|       - | 2938 | ` */` |
|      80 | 2939 | `static void PH7_JSON_ERROR_UTF16_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2940 | `{` |
|      40 | 2941 | `	SXUNUSED(pUserData); /* cc warning */` |
|      83 | 2942 | `	ph7_value_int(pVal,JSON_ERROR_UTF16);` |
|      83 | 2943 | `}` |
|       - | 2944 | `/*` |
|       - | 2945 | ` * JSON_ERROR_NON_BACKED_ENUM.` |
|       - | 2946 | ` *   Expand the value of JSON_ERROR_NON_BACKED_ENUM defined in ph7Int.h (php 8.1).` |
|       - | 2947 | ` */` |
|      76 | 2948 | `static void PH7_JSON_ERROR_NON_BACKED_ENUM_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2949 | `{` |
|      38 | 2950 | `	SXUNUSED(pUserData); /* cc warning */` |
|      79 | 2951 | `	ph7_value_int(pVal,JSON_ERROR_NON_BACKED_ENUM);` |
|      79 | 2952 | `}` |
|       - | 2953 | `/*` |
|       - | 2954 | ` * JSON_ERROR_INF_OR_NAN.` |
|       - | 2955 | ` *   Expand the value of JSON_ERROR_INF_OR_NAN defined in ph7Int.h.` |
|       - | 2956 | ` */` |
|      78 | 2957 | `static void PH7_JSON_ERROR_INF_OR_NAN_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2958 | `{` |
|      39 | 2959 | `	SXUNUSED(pUserData); /* cc warning */` |
|      81 | 2960 | `	ph7_value_int(pVal,JSON_ERROR_INF_OR_NAN);` |
|      81 | 2961 | `}` |
|       - | 2962 | `/*` |
|       - | 2963 | ` * __CLASS__` |
|       - | 2964 | ` *  The current class name, or the EMPTY STRING outside any class — php answers "",` |
|       - | 2965 | `` *  not null (`__CLASS__ === ""` is true in global scope). `self` keeps its own`` |
|       - | 2966 | ` *  expander below because php treats IT differently outside a class scope.` |
|       - | 2967 | ` */` |
|      88 | 2968 | `static void PH7_class_magic_Const(ph7_value *pVal,void *pUserData)` |
|       3 | 2969 | `{` |
|      91 | 2970 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 2971 | `	ph7_class *pClass;` |
|       - | 2972 | `	/* php flattens a trait into the class that used it, so __CLASS__ inside a trait method` |
|       - | 2973 | `	 * is THAT class (where __TRAIT__ and __METHOD__ stay the trait's — php's own asymmetry). */` |
|      91 | 2974 | `	pClass = PH7_VmPeekSelfClass(pVm);` |
|      91 | 2975 | `	if( pClass == 0 ){` |
|      77 | 2976 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      37 | 2977 | `	}` |
|      91 | 2978 | `	if( pClass ){` |
|      15 | 2979 | `		SyString *pName = &pClass->sName;` |
|      15 | 2980 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|       8 | 2981 | `	}else{` |
|      77 | 2982 | `		ph7_value_string(pVal,"",0);` |
|       - | 2983 | `	}` |
|      91 | 2984 | `}` |
|       - | 2985 |  |
|       - | 2986 | `/*` |
|       - | 2987 | ` * PASSWORD_BCRYPT / PASSWORD_DEFAULT` |
|       - | 2988 | ` *  The bcrypt algorithm identifier (PHP 7.4+ exposes these as the string "2y").` |
|       - | 2989 | ` *  PASSWORD_DEFAULT tracks the recommended default, currently bcrypt.` |
|       - | 2990 | ` */` |
|     180 | 2991 | `static void PH7_PASSWORD_BCRYPT_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 2992 | `{` |
|      89 | 2993 | `	SXUNUSED(pUnused);` |
|     183 | 2994 | `	ph7_value_string(pVal,"2y",(int)sizeof("2y")-1);` |
|     183 | 2995 | `}` |
|       - | 2996 | `/*` |
|       - | 2997 | ` * PASSWORD_BCRYPT_DEFAULT_COST` |
|       - | 2998 | ` *  The default bcrypt work factor used by password_hash() (currently 12).` |
|       - | 2999 | ` */` |
|      73 | 3000 | `static void PH7_PASSWORD_COST_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 3001 | `{` |
|      36 | 3002 | `	SXUNUSED(pUnused);` |
|      76 | 3003 | `	ph7_value_int(pVal,12);` |
|      76 | 3004 | `}` |
|       - | 3005 | `/*` |
|       - | 3006 | ` * PASSWORD_ARGON2I / PASSWORD_ARGON2ID and the three argon2 option defaults` |
|       - | 3007 | ` * password_hash() reads when $options omits them.` |
|       - | 3008 | ` */` |
|      77 | 3009 | `static void PH7_PASSWORD_ARGON2I_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 3010 | `{` |
|      38 | 3011 | `	SXUNUSED(pUnused);` |
|      80 | 3012 | `	ph7_value_string(pVal,"argon2i",(int)sizeof("argon2i")-1);` |
|      80 | 3013 | `}` |
|      95 | 3014 | `static void PH7_PASSWORD_ARGON2ID_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 3015 | `{` |
|      47 | 3016 | `	SXUNUSED(pUnused);` |
|      98 | 3017 | `	ph7_value_string(pVal,"argon2id",(int)sizeof("argon2id")-1);` |
|      98 | 3018 | `}` |
|      73 | 3019 | `static void PH7_ARGON2_MEM_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 3020 | `{` |
|      36 | 3021 | `	SXUNUSED(pUnused);` |
|      76 | 3022 | `	ph7_value_int(pVal,65536);` |
|      76 | 3023 | `}` |
|      73 | 3024 | `static void PH7_ARGON2_TIME_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 3025 | `{` |
|      36 | 3026 | `	SXUNUSED(pUnused);` |
|      76 | 3027 | `	ph7_value_int(pVal,4);` |
|      76 | 3028 | `}` |
|      73 | 3029 | `static void PH7_ARGON2_THREADS_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 3030 | `{` |
|      36 | 3031 | `	SXUNUSED(pUnused);` |
|      76 | 3032 | `	ph7_value_int(pVal,1);` |
|      76 | 3033 | `}` |
|       - | 3034 | `/*` |
|       - | 3035 | ` * CRYPT_* — the crypt() capability flags. Every scheme is compiled in, so all` |
|       - | 3036 | ` * six are 1, and CRYPT_SALT_LENGTH is php's 123 (the longest setting string a` |
|       - | 3037 | ` * SHA-512-crypt with an explicit rounds count can need).` |
|       - | 3038 | ` */` |
|       - | 3039 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|     438 | 3040 | `static void PH7_CRYPT_ONE_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 3041 | `{` |
|     216 | 3042 | `	SXUNUSED(pUnused);` |
|     441 | 3043 | `	ph7_value_int(pVal,1);` |
|     441 | 3044 | `}` |
|      73 | 3045 | `static void PH7_CRYPT_SALT_LENGTH_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 3046 | `{` |
|      36 | 3047 | `	SXUNUSED(pUnused);` |
|      76 | 3048 | `	ph7_value_int(pVal,123);` |
|      76 | 3049 | `}` |
|       - | 3050 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|       - | 3051 | `/*` |
|       - | 3052 | ` * filter_var() filter and flag identifiers (the ext/filter constants). Values` |
|       - | 3053 | ` * match PHP 8.5. One tiny int-returning callback per constant, generated by a` |
|       - | 3054 | ` * local macro to keep the ~25 near-identical definitions DRY.` |
|       - | 3055 | ` */` |
|       - | 3056 | `#define PH7_FILTER_INT_CONST(Name,Val) \` |
|       - | 3057 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|       - | 3058 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|       - | 3059 | `	}` |
|      95 | 3060 | `PH7_FILTER_INT_CONST(FILTER_DEFAULT,516)` |
|      89 | 3061 | `PH7_FILTER_INT_CONST(FILTER_UNSAFE_RAW,516)` |
|     235 | 3062 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_INT,257)` |
|     178 | 3063 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_BOOLEAN,258)` |
|     207 | 3064 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_FLOAT,259)` |
|      84 | 3065 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_REGEXP,272)` |
|     217 | 3066 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_DOMAIN,277)` |
|     394 | 3067 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_URL,273)` |
|     275 | 3068 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_EMAIL,274)` |
|     635 | 3069 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_IP,275)` |
|     107 | 3070 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_MAC,276)` |
|      99 | 3071 | `PH7_FILTER_INT_CONST(FILTER_CALLBACK,1024)` |
|     101 | 3072 | `PH7_FILTER_INT_CONST(FILTER_THROW_ON_FAILURE,268435456)` |
|      82 | 3073 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_ENCODED,514)` |
|      80 | 3074 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_ADD_SLASHES,523)` |
|      77 | 3075 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_INT,519)` |
|      77 | 3076 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_FLOAT,520)` |
|      88 | 3077 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_SPECIAL_CHARS,515)` |
|      97 | 3078 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_FULL_SPECIAL_CHARS,522)` |
|      75 | 3079 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_EMAIL,517)` |
|      75 | 3080 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_URL,518)` |
|      75 | 3081 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_OCTAL,1)` |
|      75 | 3082 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_HEX,2)` |
|      84 | 3083 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_LOW,4)` |
|      80 | 3084 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_HIGH,8)` |
|      80 | 3085 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_LOW,16)` |
|      77 | 3086 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_HIGH,32)` |
|      78 | 3087 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_AMP,64)` |
|      80 | 3088 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_ENCODE_QUOTES,128)` |
|      76 | 3089 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NONE,0)` |
|      84 | 3090 | `PH7_FILTER_INT_CONST(FILTER_FLAG_EMPTY_STRING_NULL,256)` |
|      78 | 3091 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_BACKTICK,512)` |
|      75 | 3092 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_FRACTION,4096)` |
|     109 | 3093 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_THOUSAND,8192)` |
|      75 | 3094 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_SCIENTIFIC,16384)` |
|      81 | 3095 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV4,1048576)` |
|      77 | 3096 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV6,2097152)` |
|     226 | 3097 | `PH7_FILTER_INT_CONST(FILTER_FLAG_PATH_REQUIRED,262144)` |
|     226 | 3098 | `PH7_FILTER_INT_CONST(FILTER_FLAG_QUERY_REQUIRED,524288)` |
|     137 | 3099 | `PH7_FILTER_INT_CONST(FILTER_FLAG_HOSTNAME,1048576)` |
|     158 | 3100 | `PH7_FILTER_INT_CONST(FILTER_FLAG_EMAIL_UNICODE,1048576)` |
|     285 | 3101 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_RES_RANGE,4194304)` |
|     289 | 3102 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_PRIV_RANGE,8388608)` |
|     181 | 3103 | `PH7_FILTER_INT_CONST(FILTER_FLAG_GLOBAL_RANGE,536870912)` |
|     100 | 3104 | `PH7_FILTER_INT_CONST(FILTER_REQUIRE_ARRAY,16777216)` |
|      80 | 3105 | `PH7_FILTER_INT_CONST(FILTER_REQUIRE_SCALAR,33554432)` |
|      84 | 3106 | `PH7_FILTER_INT_CONST(FILTER_FORCE_ARRAY,67108864)` |
|      97 | 3107 | `PH7_FILTER_INT_CONST(FILTER_NULL_ON_FAILURE,134217728)` |
|       - | 3108 | `/* filter_input() source selectors (php values; SESSION/REQUEST are undefined in 8.5) */` |
|      78 | 3109 | `PH7_FILTER_INT_CONST(INPUT_POST,0)` |
|      87 | 3110 | `PH7_FILTER_INT_CONST(INPUT_GET,1)` |
|      76 | 3111 | `PH7_FILTER_INT_CONST(INPUT_COOKIE,2)` |
|      76 | 3112 | `PH7_FILTER_INT_CONST(INPUT_ENV,4)` |
|      94 | 3113 | `PH7_FILTER_INT_CONST(INPUT_SERVER,5)` |
|       - | 3114 | `/*` |
|       - | 3115 | ` * ext/fileinfo's flags. libmagic's MAGIC_* values, which php re-exports under` |
|       - | 3116 | ` * its own names -- a script stores one and hands it back, so the NUMBERS are` |
|       - | 3117 | ` * the contract (see builtin_fileinfo.c).` |
|       - | 3118 | ` */` |
|       - | 3119 | `#define PH7_FILEINFO_INT_CONST(Name,Val) \` |
|       - | 3120 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|       - | 3121 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|       - | 3122 | `	}` |
|       - | 3123 | `/*` |
|       - | 3124 | ` * ext/zlib's constants. The three encodings are libz's own windowBits spelling` |
|       - | 3125 | ` * (negative = raw, +16 = gzip), which is why they are -15/15/31; the flush and` |
|       - | 3126 | ` * status numbers are libz's too. ZLIB_VERSION and ZLIB_VERNUM report the` |
|       - | 3127 | ` * library this build was compiled against, exactly as php's report theirs --` |
|       - | 3128 | ` * so they are the one pair here that is not the same on every box.` |
|       - | 3129 | ` */` |
|       - | 3130 | `#ifdef PH7_ENABLE_ZLIB` |
|       - | 3131 | `#define PH7_ZLIB_INT_CONST(Name,Val) \` |
|       - | 3132 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|       - | 3133 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|       - | 3134 | `	}` |
|      76 | 3135 | `PH7_ZLIB_INT_CONST(FORCE_GZIP,31)` |
|      76 | 3136 | `PH7_ZLIB_INT_CONST(FORCE_DEFLATE,15)` |
|     110 | 3137 | `PH7_ZLIB_INT_CONST(ZLIB_ENCODING_RAW,-15)` |
|      86 | 3138 | `PH7_ZLIB_INT_CONST(ZLIB_ENCODING_GZIP,31)` |
|      84 | 3139 | `PH7_ZLIB_INT_CONST(ZLIB_ENCODING_DEFLATE,15)` |
|      78 | 3140 | `PH7_ZLIB_INT_CONST(ZLIB_NO_FLUSH,0)` |
|      76 | 3141 | `PH7_ZLIB_INT_CONST(ZLIB_PARTIAL_FLUSH,1)` |
|      80 | 3142 | `PH7_ZLIB_INT_CONST(ZLIB_SYNC_FLUSH,2)` |
|      76 | 3143 | `PH7_ZLIB_INT_CONST(ZLIB_FULL_FLUSH,3)` |
|      76 | 3144 | `PH7_ZLIB_INT_CONST(ZLIB_BLOCK,5)` |
|      88 | 3145 | `PH7_ZLIB_INT_CONST(ZLIB_FINISH,4)` |
|      76 | 3146 | `PH7_ZLIB_INT_CONST(ZLIB_FILTERED,1)` |
|      76 | 3147 | `PH7_ZLIB_INT_CONST(ZLIB_HUFFMAN_ONLY,2)` |
|      76 | 3148 | `PH7_ZLIB_INT_CONST(ZLIB_RLE,3)` |
|      76 | 3149 | `PH7_ZLIB_INT_CONST(ZLIB_FIXED,4)` |
|      76 | 3150 | `PH7_ZLIB_INT_CONST(ZLIB_DEFAULT_STRATEGY,0)` |
|      76 | 3151 | `PH7_ZLIB_INT_CONST(ZLIB_VERNUM,ZLIB_VERNUM)` |
|      76 | 3152 | `PH7_ZLIB_INT_CONST(ZLIB_OK,0)` |
|      76 | 3153 | `PH7_ZLIB_INT_CONST(ZLIB_STREAM_END,1)` |
|      76 | 3154 | `PH7_ZLIB_INT_CONST(ZLIB_NEED_DICT,2)` |
|      76 | 3155 | `PH7_ZLIB_INT_CONST(ZLIB_ERRNO,-1)` |
|      76 | 3156 | `PH7_ZLIB_INT_CONST(ZLIB_STREAM_ERROR,-2)` |
|      76 | 3157 | `PH7_ZLIB_INT_CONST(ZLIB_DATA_ERROR,-3)` |
|      76 | 3158 | `PH7_ZLIB_INT_CONST(ZLIB_MEM_ERROR,-4)` |
|      76 | 3159 | `PH7_ZLIB_INT_CONST(ZLIB_BUF_ERROR,-5)` |
|      76 | 3160 | `PH7_ZLIB_INT_CONST(ZLIB_VERSION_ERROR,-6)` |
|      72 | 3161 | `static void PH7_ZLIB_VERSION_Const(ph7_value *pVal,void *pUnused)` |
|       4 | 3162 | `{` |
|      36 | 3163 | `	SXUNUSED(pUnused);` |
|      76 | 3164 | `	ph7_value_string(pVal,ZLIB_VERSION,-1);` |
|      76 | 3165 | `}` |
|       - | 3166 | `#endif /* PH7_ENABLE_ZLIB */` |
|       - | 3167 | `/*` |
|       - | 3168 | ` * ext/openssl's constants. THREE families with three different origins, and` |
|       - | 3169 | ` * telling them apart is the whole of the work here:` |
|       - | 3170 | ` *` |
|       - | 3171 | ` *  - the LIBRARY's numbers (the X509_PURPOSE_*, the PKCS7_* and CMS_* flag` |
|       - | 3172 | ` *    bits, the RSA padding modes, the version pair). Each is bound to the` |
|       - | 3173 | ` *    OpenSSL SYMBOL, never to the number this box happens to print: a flag` |
|       - | 3174 | ` *    that moved between 3.0 and 3.6 would otherwise be silently wrong on the` |
|       - | 3175 | ` *    Windows build, which links a different one. See the` |
|       - | 3176 | `` *    `bind-by-symbol-not-by-value` note.`` |
|       - | 3177 | ` *  - php's OWN numbering, which OpenSSL has no symbol for: OPENSSL_ALGO_*` |
|       - | 3178 | ` *    (a digest enum of php's, with the gap at 4/5 where php removed two DSS` |
|       - | 3179 | ` *    entries), OPENSSL_CIPHER_* (an enum the PKCS#7 doors take),` |
|       - | 3180 | ` *    OPENSSL_KEYTYPE_*, the three option bits and the three CMS encodings.` |
|       - | 3181 | ` *    These are literals because they ARE literals -- a script stores one and` |
|       - | 3182 | ` *    hands it back, so the numbers are the contract.` |
|       - | 3183 | ` *  - one STRING php composes itself: OPENSSL_DEFAULT_STREAM_CIPHERS, the` |
|       - | 3184 | ` *    cipher list php's own TLS streams start from. It is php's text, not` |
|       - | 3185 | ` *    OpenSSL's default, and it is byte-for-byte what php ships.` |
|       - | 3186 | ` */` |
|       - | 3187 | `#ifdef PH7_ENABLE_OPENSSL` |
|       - | 3188 | `#define PH7_SSL_INT_CONST(Name,Val) \` |
|       - | 3189 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|       - | 3190 | `		SXUNUSED(pUnused); ph7_value_int64(pVal,(sxi64)(Val)); \` |
|       - | 3191 | `	}` |
|      79 | 3192 | `PH7_SSL_INT_CONST(OPENSSL_VERSION_NUMBER,OPENSSL_VERSION_NUMBER)` |
|      75 | 3193 | `PH7_SSL_INT_CONST(X509_PURPOSE_SSL_CLIENT,X509_PURPOSE_SSL_CLIENT)` |
|      80 | 3194 | `PH7_SSL_INT_CONST(X509_PURPOSE_SSL_SERVER,X509_PURPOSE_SSL_SERVER)` |
|      75 | 3195 | `PH7_SSL_INT_CONST(X509_PURPOSE_NS_SSL_SERVER,X509_PURPOSE_NS_SSL_SERVER)` |
|      75 | 3196 | `PH7_SSL_INT_CONST(X509_PURPOSE_SMIME_SIGN,X509_PURPOSE_SMIME_SIGN)` |
|      75 | 3197 | `PH7_SSL_INT_CONST(X509_PURPOSE_SMIME_ENCRYPT,X509_PURPOSE_SMIME_ENCRYPT)` |
|      75 | 3198 | `PH7_SSL_INT_CONST(X509_PURPOSE_CRL_SIGN,X509_PURPOSE_CRL_SIGN)` |
|      75 | 3199 | `PH7_SSL_INT_CONST(X509_PURPOSE_ANY,X509_PURPOSE_ANY)` |
|      75 | 3200 | `PH7_SSL_INT_CONST(X509_PURPOSE_OCSP_HELPER,X509_PURPOSE_OCSP_HELPER)` |
|      75 | 3201 | `PH7_SSL_INT_CONST(X509_PURPOSE_TIMESTAMP_SIGN,X509_PURPOSE_TIMESTAMP_SIGN)` |
|      84 | 3202 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA1,1)` |
|      79 | 3203 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_MD5,2)` |
|      77 | 3204 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_MD4,3)` |
|      77 | 3205 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA224,6)` |
|      82 | 3206 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA256,7)` |
|      77 | 3207 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA384,8)` |
|      77 | 3208 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA512,9)` |
|      77 | 3209 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_RMD160,10)` |
|      77 | 3210 | `PH7_SSL_INT_CONST(PKCS7_DETACHED,PKCS7_DETACHED)` |
|      75 | 3211 | `PH7_SSL_INT_CONST(PKCS7_TEXT,PKCS7_TEXT)` |
|      75 | 3212 | `PH7_SSL_INT_CONST(PKCS7_NOINTERN,PKCS7_NOINTERN)` |
|      78 | 3213 | `PH7_SSL_INT_CONST(PKCS7_NOVERIFY,PKCS7_NOVERIFY)` |
|      75 | 3214 | `PH7_SSL_INT_CONST(PKCS7_NOCHAIN,PKCS7_NOCHAIN)` |
|      75 | 3215 | `PH7_SSL_INT_CONST(PKCS7_NOCERTS,PKCS7_NOCERTS)` |
|      75 | 3216 | `PH7_SSL_INT_CONST(PKCS7_NOATTR,PKCS7_NOATTR)` |
|      75 | 3217 | `PH7_SSL_INT_CONST(PKCS7_BINARY,PKCS7_BINARY)` |
|      75 | 3218 | `PH7_SSL_INT_CONST(PKCS7_NOSIGS,PKCS7_NOSIGS)` |
|      75 | 3219 | `PH7_SSL_INT_CONST(PKCS7_NOOLDMIMETYPE,PKCS7_NOOLDMIMETYPE)` |
|      75 | 3220 | `PH7_SSL_INT_CONST(PKCS7_NOSMIMECAP,PKCS7_NOSMIMECAP)` |
|      75 | 3221 | `PH7_SSL_INT_CONST(PKCS7_CRLFEOL,PKCS7_CRLFEOL)` |
|      75 | 3222 | `PH7_SSL_INT_CONST(PKCS7_NOCRL,PKCS7_NOCRL)` |
|      75 | 3223 | `PH7_SSL_INT_CONST(PKCS7_NO_DUAL_CONTENT,PKCS7_NO_DUAL_CONTENT)` |
|      75 | 3224 | `PH7_SSL_INT_CONST(OPENSSL_CMS_DETACHED,CMS_DETACHED)` |
|      75 | 3225 | `PH7_SSL_INT_CONST(OPENSSL_CMS_TEXT,CMS_TEXT)` |
|      75 | 3226 | `PH7_SSL_INT_CONST(OPENSSL_CMS_NOINTERN,CMS_NOINTERN)` |
|      78 | 3227 | `PH7_SSL_INT_CONST(OPENSSL_CMS_NOVERIFY,CMS_NO_SIGNER_CERT_VERIFY)` |
|      75 | 3228 | `PH7_SSL_INT_CONST(OPENSSL_CMS_NOCERTS,CMS_NOCERTS)` |
|      75 | 3229 | `PH7_SSL_INT_CONST(OPENSSL_CMS_NOATTR,CMS_NOATTR)` |
|      75 | 3230 | `PH7_SSL_INT_CONST(OPENSSL_CMS_BINARY,CMS_BINARY)` |
|      75 | 3231 | `PH7_SSL_INT_CONST(OPENSSL_CMS_NOSIGS,CMS_NOSIGS)` |
|      75 | 3232 | `PH7_SSL_INT_CONST(OPENSSL_CMS_OLDMIMETYPE,CMS_NOOLDMIMETYPE)` |
|      88 | 3233 | `PH7_SSL_INT_CONST(OPENSSL_PKCS1_PADDING,RSA_PKCS1_PADDING)` |
|      80 | 3234 | `PH7_SSL_INT_CONST(OPENSSL_NO_PADDING,RSA_NO_PADDING)` |
|      80 | 3235 | `PH7_SSL_INT_CONST(OPENSSL_PKCS1_OAEP_PADDING,RSA_PKCS1_OAEP_PADDING)` |
|      82 | 3236 | `PH7_SSL_INT_CONST(OPENSSL_PKCS1_PSS_PADDING,RSA_PKCS1_PSS_PADDING)` |
|      75 | 3237 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_RC2_40,0)` |
|      75 | 3238 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_RC2_128,1)` |
|      75 | 3239 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_RC2_64,2)` |
|      75 | 3240 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_DES,3)` |
|      75 | 3241 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_3DES,4)` |
|      79 | 3242 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_AES_128_CBC,5)` |
|      75 | 3243 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_AES_192_CBC,6)` |
|      75 | 3244 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_AES_256_CBC,7)` |
|      87 | 3245 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_RSA,0)` |
|      77 | 3246 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_DSA,1)` |
|      77 | 3247 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_DH,2)` |
|      91 | 3248 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_EC,3)` |
|      80 | 3249 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_X25519,4)` |
|      80 | 3250 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_ED25519,5)` |
|      80 | 3251 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_X448,6)` |
|      80 | 3252 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_ED448,7)` |
|     143 | 3253 | `PH7_SSL_INT_CONST(OPENSSL_RAW_DATA,1)` |
|      83 | 3254 | `PH7_SSL_INT_CONST(OPENSSL_ZERO_PADDING,2)` |
|      79 | 3255 | `PH7_SSL_INT_CONST(OPENSSL_DONT_ZERO_PAD_KEY,4)` |
|      75 | 3256 | `PH7_SSL_INT_CONST(OPENSSL_TLSEXT_SERVER_NAME,1)` |
|       - | 3257 | `/*` |
|       - | 3258 | ` * stream_socket_enable_crypto()'s $crypto_method, and the ssl:// context's` |
|       - | 3259 | `` * `crypto_method`. php's OWN numbering, not OpenSSL's: bit 0 says CLIENT and`` |
|       - | 3260 | ` * the rest are one bit per protocol (2 SSLv2, 4 SSLv3, 8 TLSv1.0, 16 TLSv1.1,` |
|       - | 3261 | ` * 32 TLSv1.2, 64 TLSv1.3), so a method is a SET of protocols a handshake may` |
|       - | 3262 | ` * settle on and the engine turns it into OpenSSL's min/max version pair.` |
|       - | 3263 | `` * Two consequences a script can see: `TLS_SERVER` and `SSLv23_SERVER` are the`` |
|       - | 3264 | ` * SAME number (120) because php numbers the server side without the client` |
|       - | 3265 | ` * bit and the two sets coincide, and the SSLv2/SSLv3 bits still exist though` |
|       - | 3266 | ` * no OpenSSL 3 build will negotiate either.` |
|       - | 3267 | ` */` |
|      75 | 3268 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv2_CLIENT,3)` |
|      75 | 3269 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv2_SERVER,2)` |
|      75 | 3270 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv3_CLIENT,5)` |
|      75 | 3271 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv3_SERVER,4)` |
|      75 | 3272 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv23_CLIENT,57)` |
|      75 | 3273 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv23_SERVER,120)` |
|      85 | 3274 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLS_CLIENT,121)` |
|      75 | 3275 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLS_SERVER,120)` |
|      75 | 3276 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_0_CLIENT,9)` |
|      75 | 3277 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_0_SERVER,8)` |
|      75 | 3278 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_1_CLIENT,17)` |
|      75 | 3279 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_1_SERVER,16)` |
|      75 | 3280 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_2_CLIENT,33)` |
|      75 | 3281 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_2_SERVER,32)` |
|      75 | 3282 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_3_CLIENT,65)` |
|      75 | 3283 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_3_SERVER,64)` |
|      75 | 3284 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_ANY_CLIENT,127)` |
|      75 | 3285 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_ANY_SERVER,126)` |
|      75 | 3286 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_PROTO_SSLv3,4)` |
|      75 | 3287 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_PROTO_TLSv1_0,8)` |
|      75 | 3288 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_PROTO_TLSv1_1,16)` |
|      75 | 3289 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_PROTO_TLSv1_2,32)` |
|      75 | 3290 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_PROTO_TLSv1_3,64)` |
|      77 | 3291 | `PH7_SSL_INT_CONST(OPENSSL_ENCODING_DER,0)` |
|      85 | 3292 | `PH7_SSL_INT_CONST(OPENSSL_ENCODING_SMIME,1)` |
|      77 | 3293 | `PH7_SSL_INT_CONST(OPENSSL_ENCODING_PEM,2)` |
|      74 | 3294 | `static void PH7_OPENSSL_VERSION_TEXT_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 3295 | `{` |
|      37 | 3296 | `	SXUNUSED(pUnused);` |
|      77 | 3297 | `	ph7_value_string(pVal,OPENSSL_VERSION_TEXT,-1);` |
|      77 | 3298 | `}` |
|      76 | 3299 | `static void PH7_OPENSSL_DEFAULT_STREAM_CIPHERS_Const(ph7_value *pVal,void *pUnused)` |
|       3 | 3300 | `{` |
|      38 | 3301 | `	SXUNUSED(pUnused);` |
|      79 | 3302 | `	ph7_value_string(pVal,` |
|       - | 3303 | `		"ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:"` |
|       - | 3304 | `		"ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:"` |
|       - | 3305 | `		"DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:"` |
|       - | 3306 | `		"ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:"` |
|       - | 3307 | `		"ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:"` |
|       - | 3308 | `		"ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:"` |
|       - | 3309 | `		"DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:"` |
|       - | 3310 | `		"DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:"` |
|       - | 3311 | `		"AES128:AES256:HIGH:!SSLv2:!aNULL:!eNULL:!EXPORT:!DES:!MD5:!RC4:!ADH",-1);` |
|      79 | 3312 | `}` |
|       - | 3313 | `#endif /* PH7_ENABLE_OPENSSL */` |
|      99 | 3314 | `PH7_FILEINFO_INT_CONST(FILEINFO_NONE,0)` |
|      76 | 3315 | `PH7_FILEINFO_INT_CONST(FILEINFO_SYMLINK,2)` |
|      81 | 3316 | `PH7_FILEINFO_INT_CONST(FILEINFO_MIME,1040)` |
|      93 | 3317 | `PH7_FILEINFO_INT_CONST(FILEINFO_MIME_TYPE,16)` |
|      81 | 3318 | `PH7_FILEINFO_INT_CONST(FILEINFO_MIME_ENCODING,1024)` |
|      76 | 3319 | `PH7_FILEINFO_INT_CONST(FILEINFO_DEVICES,8)` |
|      76 | 3320 | `PH7_FILEINFO_INT_CONST(FILEINFO_CONTINUE,32)` |
|      76 | 3321 | `PH7_FILEINFO_INT_CONST(FILEINFO_PRESERVE_ATIME,128)` |
|      76 | 3322 | `PH7_FILEINFO_INT_CONST(FILEINFO_RAW,256)` |
|      76 | 3323 | `PH7_FILEINFO_INT_CONST(FILEINFO_APPLE,2048)` |
|      78 | 3324 | `PH7_FILEINFO_INT_CONST(FILEINFO_EXTENSION,16777216)` |
|       - | 3325 | `/*` |
|       - | 3326 | ` * Table of built-in constants.` |
|       - | 3327 | ` */` |
|       - | 3328 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|       - | 3329 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|       - | 3330 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|       - | 3331 | `	{"__PH7__",              PH7_VER_Const      },` |
|       - | 3332 | `	{"PHP_VERSION",          PH7_PHPVerConst    },` |
|       - | 3333 | `	{"PHP_MAJOR_VERSION",    PH7_PHPMajorConst  },` |
|       - | 3334 | `	{"PHP_MINOR_VERSION",    PH7_PHPMinorConst  },` |
|       - | 3335 | `	{"PHP_RELEASE_VERSION",  PH7_PHPReleaseConst},` |
|       - | 3336 | `	{"PHP_EXTRA_VERSION",    PH7_PHPExtraConst  },` |
|       - | 3337 | `	{"PHP_VERSION_ID",       PH7_PHPVerIdConst  },` |
|       - | 3338 | `	{"PHP_OS",               PH7_OS_Const       },` |
|       - | 3339 | `	{"PHP_OS_FAMILY",        PH7_OS_FAMILY_Const},` |
|       - | 3340 | `	{"PHP_SAPI",             PH7_SAPI_Const     },` |
|       - | 3341 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|       - | 3342 | `	{"PHP_SESSION_DISABLED", PH7_PHP_SESSION_DISABLED_Const },` |
|       - | 3343 | `	{"PHP_SESSION_NONE",     PH7_PHP_SESSION_NONE_Const },` |
|       - | 3344 | `	{"PHP_SESSION_ACTIVE",   PH7_PHP_SESSION_ACTIVE_Const },` |
|       - | 3345 | `	{"INI_USER",             PH7_INI_USER_Const },` |
|       - | 3346 | `	{"INI_PERDIR",           PH7_INI_PERDIR_Const },` |
|       - | 3347 | `	{"INI_SYSTEM",           PH7_INI_SYSTEM_Const },` |
|       - | 3348 | `	{"INI_ALL",              PH7_INI_ALL_Const },` |
|       - | 3349 | `	{"MB_CASE_UPPER",        PH7_MB_CASE_UPPER_Const },` |
|       - | 3350 | `	{"MB_CASE_LOWER",        PH7_MB_CASE_LOWER_Const },` |
|       - | 3351 | `	{"MB_CASE_TITLE",        PH7_MB_CASE_TITLE_Const },` |
|       - | 3352 | `	{"PASSWORD_BCRYPT",      PH7_PASSWORD_BCRYPT_Const },` |
|       - | 3353 | `	{"PASSWORD_DEFAULT",     PH7_PASSWORD_BCRYPT_Const },` |
|       - | 3354 | `	{"PASSWORD_BCRYPT_DEFAULT_COST", PH7_PASSWORD_COST_Const },` |
|       - | 3355 | `	{"PASSWORD_ARGON2I",     PH7_PASSWORD_ARGON2I_Const },` |
|       - | 3356 | `	{"PASSWORD_ARGON2ID",    PH7_PASSWORD_ARGON2ID_Const },` |
|       - | 3357 | `	{"PASSWORD_ARGON2_DEFAULT_MEMORY_COST", PH7_ARGON2_MEM_Const },` |
|       - | 3358 | `	{"PASSWORD_ARGON2_DEFAULT_TIME_COST",   PH7_ARGON2_TIME_Const },` |
|       - | 3359 | `	{"PASSWORD_ARGON2_DEFAULT_THREADS",     PH7_ARGON2_THREADS_Const },` |
|       - | 3360 | `	{"FILTER_DEFAULT",              PH7_FILTER_DEFAULT_Const },` |
|       - | 3361 | `	{"FILTER_UNSAFE_RAW",           PH7_FILTER_UNSAFE_RAW_Const },` |
|       - | 3362 | `	{"FILTER_VALIDATE_INT",         PH7_FILTER_VALIDATE_INT_Const },` |
|       - | 3363 | `	{"FILTER_VALIDATE_BOOLEAN",     PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|       - | 3364 | `	{"FILTER_VALIDATE_BOOL",        PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|       - | 3365 | `	{"FILTER_VALIDATE_FLOAT",       PH7_FILTER_VALIDATE_FLOAT_Const },` |
|       - | 3366 | `	{"FILTER_VALIDATE_REGEXP",      PH7_FILTER_VALIDATE_REGEXP_Const },` |
|       - | 3367 | `	{"FILTER_VALIDATE_DOMAIN",      PH7_FILTER_VALIDATE_DOMAIN_Const },` |
|       - | 3368 | `	{"FILTER_VALIDATE_URL",         PH7_FILTER_VALIDATE_URL_Const },` |
|       - | 3369 | `	{"FILTER_VALIDATE_EMAIL",       PH7_FILTER_VALIDATE_EMAIL_Const },` |
|       - | 3370 | `	{"FILTER_VALIDATE_IP",          PH7_FILTER_VALIDATE_IP_Const },` |
|       - | 3371 | `	{"FILTER_VALIDATE_MAC",         PH7_FILTER_VALIDATE_MAC_Const },` |
|       - | 3372 | `	{"FILTER_CALLBACK",             PH7_FILTER_CALLBACK_Const },` |
|       - | 3373 | `	{"FILTER_THROW_ON_FAILURE",     PH7_FILTER_THROW_ON_FAILURE_Const },` |
|       - | 3374 | `	{"FILTER_SANITIZE_ENCODED",     PH7_FILTER_SANITIZE_ENCODED_Const },` |
|       - | 3375 | `	{"FILTER_SANITIZE_ADD_SLASHES", PH7_FILTER_SANITIZE_ADD_SLASHES_Const },` |
|       - | 3376 | `	{"FILTER_FLAG_NONE",            PH7_FILTER_FLAG_NONE_Const },` |
|       - | 3377 | `	{"FILTER_FLAG_EMPTY_STRING_NULL", PH7_FILTER_FLAG_EMPTY_STRING_NULL_Const },` |
|       - | 3378 | `	{"FILTER_SANITIZE_NUMBER_INT",  PH7_FILTER_SANITIZE_NUMBER_INT_Const },` |
|       - | 3379 | `	{"FILTER_SANITIZE_NUMBER_FLOAT",PH7_FILTER_SANITIZE_NUMBER_FLOAT_Const },` |
|       - | 3380 | `	{"FILTER_SANITIZE_SPECIAL_CHARS",PH7_FILTER_SANITIZE_SPECIAL_CHARS_Const },` |
|       - | 3381 | `	{"FILTER_SANITIZE_FULL_SPECIAL_CHARS",PH7_FILTER_SANITIZE_FULL_SPECIAL_CHARS_Const },` |
|       - | 3382 | `	{"FILTER_SANITIZE_EMAIL",       PH7_FILTER_SANITIZE_EMAIL_Const },` |
|       - | 3383 | `	{"FILTER_SANITIZE_URL",         PH7_FILTER_SANITIZE_URL_Const },` |
|       - | 3384 | `	{"FILTER_FLAG_ALLOW_OCTAL",     PH7_FILTER_FLAG_ALLOW_OCTAL_Const },` |
|       - | 3385 | `	{"FILTER_FLAG_ALLOW_HEX",       PH7_FILTER_FLAG_ALLOW_HEX_Const },` |
|       - | 3386 | `	{"FILTER_FLAG_STRIP_LOW",       PH7_FILTER_FLAG_STRIP_LOW_Const },` |
|       - | 3387 | `	{"FILTER_FLAG_STRIP_HIGH",      PH7_FILTER_FLAG_STRIP_HIGH_Const },` |
|       - | 3388 | `	{"FILTER_FLAG_ENCODE_LOW",      PH7_FILTER_FLAG_ENCODE_LOW_Const },` |
|       - | 3389 | `	{"FILTER_FLAG_ENCODE_HIGH",     PH7_FILTER_FLAG_ENCODE_HIGH_Const },` |
|       - | 3390 | `	{"FILTER_FLAG_ENCODE_AMP",      PH7_FILTER_FLAG_ENCODE_AMP_Const },` |
|       - | 3391 | `	{"FILTER_FLAG_NO_ENCODE_QUOTES",PH7_FILTER_FLAG_NO_ENCODE_QUOTES_Const },` |
|       - | 3392 | `	{"FILTER_FLAG_STRIP_BACKTICK",  PH7_FILTER_FLAG_STRIP_BACKTICK_Const },` |
|       - | 3393 | `	{"FILTER_FLAG_ALLOW_FRACTION",  PH7_FILTER_FLAG_ALLOW_FRACTION_Const },` |
|       - | 3394 | `	{"FILTER_FLAG_ALLOW_THOUSAND",  PH7_FILTER_FLAG_ALLOW_THOUSAND_Const },` |
|       - | 3395 | `	{"FILTER_FLAG_ALLOW_SCIENTIFIC",PH7_FILTER_FLAG_ALLOW_SCIENTIFIC_Const },` |
|       - | 3396 | `	{"FILTER_FLAG_IPV4",            PH7_FILTER_FLAG_IPV4_Const },` |
|       - | 3397 | `	{"FILTER_FLAG_IPV6",            PH7_FILTER_FLAG_IPV6_Const },` |
|       - | 3398 | `	{"FILTER_FLAG_PATH_REQUIRED",   PH7_FILTER_FLAG_PATH_REQUIRED_Const },` |
|       - | 3399 | `	{"FILTER_FLAG_QUERY_REQUIRED",  PH7_FILTER_FLAG_QUERY_REQUIRED_Const },` |
|       - | 3400 | `	{"FILTER_FLAG_HOSTNAME",        PH7_FILTER_FLAG_HOSTNAME_Const },` |
|       - | 3401 | `	{"FILTER_FLAG_EMAIL_UNICODE",   PH7_FILTER_FLAG_EMAIL_UNICODE_Const },` |
|       - | 3402 | `	{"FILTER_FLAG_NO_RES_RANGE",    PH7_FILTER_FLAG_NO_RES_RANGE_Const },` |
|       - | 3403 | `	{"FILTER_FLAG_NO_PRIV_RANGE",   PH7_FILTER_FLAG_NO_PRIV_RANGE_Const },` |
|       - | 3404 | `	{"FILTER_FLAG_GLOBAL_RANGE",    PH7_FILTER_FLAG_GLOBAL_RANGE_Const },` |
|       - | 3405 | `	{"FILTER_REQUIRE_ARRAY",        PH7_FILTER_REQUIRE_ARRAY_Const },` |
|       - | 3406 | `	{"FILTER_REQUIRE_SCALAR",       PH7_FILTER_REQUIRE_SCALAR_Const },` |
|       - | 3407 | `	{"FILTER_FORCE_ARRAY",          PH7_FILTER_FORCE_ARRAY_Const },` |
|       - | 3408 | `	{"FILTER_NULL_ON_FAILURE",      PH7_FILTER_NULL_ON_FAILURE_Const },` |
|       - | 3409 | `	{"INPUT_POST",                  PH7_INPUT_POST_Const },` |
|       - | 3410 | `	{"INPUT_GET",                   PH7_INPUT_GET_Const },` |
|       - | 3411 | `	{"INPUT_COOKIE",                PH7_INPUT_COOKIE_Const },` |
|       - | 3412 | `	{"INPUT_ENV",                   PH7_INPUT_ENV_Const },` |
|       - | 3413 | `	{"INPUT_SERVER",                PH7_INPUT_SERVER_Const },` |
|       - | 3414 | `	{"CAL_GREGORIAN",        PH7_CAL_GREGORIAN_Const },` |
|       - | 3415 | `	{"CAL_JULIAN",           PH7_CAL_JULIAN_Const    },` |
|       - | 3416 | `	{"CAL_JEWISH",           PH7_CAL_JEWISH_Const    },` |
|       - | 3417 | `	{"CAL_FRENCH",           PH7_CAL_FRENCH_Const    },` |
|       - | 3418 | `	{"CAL_NUM_CALS",         PH7_CAL_NUM_CALS_Const  },` |
|       - | 3419 | `	{"CAL_DOW_DAYNO",        PH7_CAL_DOW_DAYNO_Const },` |
|       - | 3420 | `	{"CAL_DOW_LONG",         PH7_CAL_DOW_LONG_Const  },` |
|       - | 3421 | `	{"CAL_DOW_SHORT",        PH7_CAL_DOW_SHORT_Const },` |
|       - | 3422 | `	{"CAL_MONTH_GREGORIAN_SHORT", PH7_CAL_MONTH_GREGORIAN_SHORT_Const },` |
|       - | 3423 | `	{"CAL_MONTH_GREGORIAN_LONG",  PH7_CAL_MONTH_GREGORIAN_LONG_Const },` |
|       - | 3424 | `	{"CAL_MONTH_JULIAN_SHORT",    PH7_CAL_MONTH_JULIAN_SHORT_Const },` |
|       - | 3425 | `	{"CAL_MONTH_JULIAN_LONG",     PH7_CAL_MONTH_JULIAN_LONG_Const },` |
|       - | 3426 | `	{"CAL_MONTH_JEWISH",          PH7_CAL_MONTH_JEWISH_Const },` |
|       - | 3427 | `	{"CAL_MONTH_FRENCH",          PH7_CAL_MONTH_FRENCH_Const },` |
|       - | 3428 | `	{"CAL_EASTER_DEFAULT",   PH7_CAL_EASTER_DEFAULT_Const },` |
|       - | 3429 | `	{"CAL_EASTER_ROMAN",     PH7_CAL_EASTER_ROMAN_Const   },` |
|       - | 3430 | `	{"CAL_EASTER_ALWAYS_GREGORIAN", PH7_CAL_EASTER_ALWAYS_GREGORIAN_Const },` |
|       - | 3431 | `	{"CAL_EASTER_ALWAYS_JULIAN",    PH7_CAL_EASTER_ALWAYS_JULIAN_Const },` |
|       - | 3432 | `	{"CAL_JEWISH_ADD_ALAFIM_GERESH", PH7_CAL_JEWISH_ADD_ALAFIM_GERESH_Const },` |
|       - | 3433 | `	{"CAL_JEWISH_ADD_ALAFIM",        PH7_CAL_JEWISH_ADD_ALAFIM_Const },` |
|       - | 3434 | `	{"CAL_JEWISH_ADD_GERESHAYIM",    PH7_CAL_JEWISH_ADD_GERESHAYIM_Const },` |
|       - | 3435 | `	{"IMAGETYPE_GIF",        PH7_IMAGETYPE_GIF_Const     },` |
|       - | 3436 | `	{"IMAGETYPE_JPEG",       PH7_IMAGETYPE_JPEG_Const    },` |
|       - | 3437 | `	{"IMAGETYPE_PNG",        PH7_IMAGETYPE_PNG_Const     },` |
|       - | 3438 | `	{"IMAGETYPE_SWF",        PH7_IMAGETYPE_SWF_Const     },` |
|       - | 3439 | `	{"IMAGETYPE_PSD",        PH7_IMAGETYPE_PSD_Const     },` |
|       - | 3440 | `	{"IMAGETYPE_BMP",        PH7_IMAGETYPE_BMP_Const     },` |
|       - | 3441 | `	{"IMAGETYPE_TIFF_II",    PH7_IMAGETYPE_TIFF_II_Const },` |
|       - | 3442 | `	{"IMAGETYPE_TIFF_MM",    PH7_IMAGETYPE_TIFF_MM_Const },` |
|       - | 3443 | `	{"IMAGETYPE_JPC",        PH7_IMAGETYPE_JPC_Const     },` |
|       - | 3444 | `	{"IMAGETYPE_JP2",        PH7_IMAGETYPE_JP2_Const     },` |
|       - | 3445 | `	{"IMAGETYPE_JPX",        PH7_IMAGETYPE_JPX_Const     },` |
|       - | 3446 | `	{"IMAGETYPE_JB2",        PH7_IMAGETYPE_JB2_Const     },` |
|       - | 3447 | `	{"IMAGETYPE_SWC",        PH7_IMAGETYPE_SWC_Const     },` |
|       - | 3448 | `	{"IMAGETYPE_IFF",        PH7_IMAGETYPE_IFF_Const     },` |
|       - | 3449 | `	{"IMAGETYPE_WBMP",       PH7_IMAGETYPE_WBMP_Const    },` |
|       - | 3450 | `	/* php's own alias row: the same 9 the JPC name expands to. */` |
|       - | 3451 | `	{"IMAGETYPE_JPEG2000",   PH7_IMAGETYPE_JPC_Const     },` |
|       - | 3452 | `	{"IMAGETYPE_XBM",        PH7_IMAGETYPE_XBM_Const     },` |
|       - | 3453 | `	{"IMAGETYPE_ICO",        PH7_IMAGETYPE_ICO_Const     },` |
|       - | 3454 | `	{"IMAGETYPE_WEBP",       PH7_IMAGETYPE_WEBP_Const    },` |
|       - | 3455 | `	{"IMAGETYPE_AVIF",       PH7_IMAGETYPE_AVIF_Const    },` |
|       - | 3456 | `	{"IMAGETYPE_HEIF",       PH7_IMAGETYPE_HEIF_Const    },` |
|       - | 3457 | `	{"IMAGETYPE_UNKNOWN",    PH7_IMAGETYPE_UNKNOWN_Const },` |
|       - | 3458 | `	{"IMAGETYPE_COUNT",      PH7_IMAGETYPE_COUNT_Const   },` |
|       - | 3459 | `#ifdef PH7_ENABLE_LIBXML` |
|       - | 3460 | `	{"IMAGETYPE_SVG",        PH7_IMAGETYPE_SVG_Const     },` |
|       - | 3461 | `#endif` |
|       - | 3462 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|       - | 3463 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|       - | 3464 | `	{"PHP_INT_MIN",          PH7_INTMIN_Const   },` |
|       - | 3465 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|       - | 3466 | `	{"PHP_FLOAT_EPSILON",    PH7_FLOATEPSILON_Const },` |
|       - | 3467 | `	{"PHP_FLOAT_MAX",        PH7_FLOATMAX_Const },` |
|       - | 3468 | `	{"PHP_FLOAT_MIN",        PH7_FLOATMIN_Const },` |
|       - | 3469 | `	{"PHP_FLOAT_DIG",        PH7_FLOATDIG_Const },` |
|       - | 3470 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|       - | 3471 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|       - | 3472 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|       - | 3473 | `	{"__TIME__",             PH7_TIME_Const     },` |
|       - | 3474 | `	{"__DATE__",             PH7_DATE_Const     },` |
|       - | 3475 | `	{"__FILE__",             PH7_FILE_Const     },` |
|       - | 3476 | `	{"__DIR__",              PH7_DIR_Const      },` |
|       - | 3477 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|       - | 3478 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|       - | 3479 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|       - | 3480 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|       - | 3481 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|       - | 3482 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|       - | 3483 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|       - | 3484 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|       - | 3485 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|       - | 3486 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|       - | 3487 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|       - | 3488 | `	{"E_USER_NOTICE",        PH7_E_USER_NOTICE_Const   },` |
|       - | 3489 | `	{"E_STRICT",             PH7_E_STRICT_Const        },` |
|       - | 3490 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|       - | 3491 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|       - | 3492 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|       - | 3493 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|       - | 3494 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|       - | 3495 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|       - | 3496 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|       - | 3497 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|       - | 3498 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|       - | 3499 | `	{"STREAM_IS_URL",                PH7_STREAM_IS_URL_Const },` |
|       - | 3500 | `	{"STREAM_USE_PATH",              PH7_STREAM_USE_PATH_Const },` |
|       - | 3501 | `	{"STREAM_IGNORE_URL",            PH7_STREAM_IGNORE_URL_Const },` |
|       - | 3502 | `	{"STREAM_REPORT_ERRORS",         PH7_STREAM_REPORT_ERRORS_Const },` |
|       - | 3503 | `	{"STREAM_MUST_SEEK",             PH7_STREAM_MUST_SEEK_Const },` |
|       - | 3504 | `	{"STREAM_URL_STAT_LINK",         PH7_STREAM_URL_STAT_LINK_Const },` |
|       - | 3505 | `	{"STREAM_URL_STAT_QUIET",        PH7_STREAM_URL_STAT_QUIET_Const },` |
|       - | 3506 | `	{"STREAM_MKDIR_RECURSIVE",       PH7_STREAM_MKDIR_RECURSIVE_Const },` |
|       - | 3507 | `	{"STREAM_META_TOUCH",            PH7_STREAM_META_TOUCH_Const },` |
|       - | 3508 | `	{"STREAM_META_OWNER_NAME",       PH7_STREAM_META_OWNER_NAME_Const },` |
|       - | 3509 | `	{"STREAM_META_OWNER",            PH7_STREAM_META_OWNER_Const },` |
|       - | 3510 | `	{"STREAM_META_GROUP_NAME",       PH7_STREAM_META_GROUP_NAME_Const },` |
|       - | 3511 | `	{"STREAM_META_GROUP",            PH7_STREAM_META_GROUP_Const },` |
|       - | 3512 | `	{"STREAM_META_ACCESS",           PH7_STREAM_META_ACCESS_Const },` |
|       - | 3513 | `	{"STREAM_OPTION_BLOCKING",       PH7_STREAM_OPTION_BLOCKING_Const },` |
|       - | 3514 | `	{"STREAM_OPTION_READ_BUFFER",    PH7_STREAM_OPTION_READ_BUFFER_Const },` |
|       - | 3515 | `	{"STREAM_OPTION_WRITE_BUFFER",   PH7_STREAM_OPTION_WRITE_BUFFER_Const },` |
|       - | 3516 | `	{"STREAM_OPTION_READ_TIMEOUT",   PH7_STREAM_OPTION_READ_TIMEOUT_Const },` |
|       - | 3517 | `	{"STREAM_BUFFER_NONE",           PH7_STREAM_BUFFER_NONE_Const },` |
|       - | 3518 | `	{"STREAM_BUFFER_LINE",           PH7_STREAM_BUFFER_LINE_Const },` |
|       - | 3519 | `	{"STREAM_BUFFER_FULL",           PH7_STREAM_BUFFER_FULL_Const },` |
|       - | 3520 | `	{"STREAM_CAST_AS_STREAM",        PH7_STREAM_CAST_AS_STREAM_Const },` |
|       - | 3521 | `	{"STREAM_CAST_FOR_SELECT",       PH7_STREAM_CAST_FOR_SELECT_Const },` |
|       - | 3522 | `	{"STREAM_SERVER_BIND",           PH7_STREAM_SERVER_BIND_Const },` |
|       - | 3523 | `	{"STREAM_SERVER_LISTEN",         PH7_STREAM_SERVER_LISTEN_Const },` |
|       - | 3524 | `	{"STREAM_CLIENT_CONNECT",        PH7_STREAM_CLIENT_CONNECT_Const },` |
|       - | 3525 | `	{"STREAM_CLIENT_ASYNC_CONNECT",  PH7_STREAM_CLIENT_ASYNC_CONNECT_Const },` |
|       - | 3526 | `	{"STREAM_CLIENT_PERSISTENT",     PH7_STREAM_CLIENT_PERSISTENT_Const },` |
|       - | 3527 | `	{"PSFS_PASS_ON",                 PH7_PSFS_PASS_ON_Const },` |
|       - | 3528 | `	{"PSFS_FEED_ME",                 PH7_PSFS_FEED_ME_Const },` |
|       - | 3529 | `	{"PSFS_ERR_FATAL",               PH7_PSFS_ERR_FATAL_Const },` |
|       - | 3530 | `	{"PSFS_FLAG_NORMAL",             PH7_PSFS_FLAG_NORMAL_Const },` |
|       - | 3531 | `	{"PSFS_FLAG_FLUSH_INC",          PH7_PSFS_FLAG_FLUSH_INC_Const },` |
|       - | 3532 | `	{"PSFS_FLAG_FLUSH_CLOSE",        PH7_PSFS_FLAG_FLUSH_CLOSE_Const },` |
|       - | 3533 | `	{"STREAM_NOTIFY_RESOLVE",        PH7_STREAM_NOTIFY_RESOLVE_Const },` |
|       - | 3534 | `	{"STREAM_NOTIFY_CONNECT",        PH7_STREAM_NOTIFY_CONNECT_Const },` |
|       - | 3535 | `	{"STREAM_NOTIFY_AUTH_REQUIRED",  PH7_STREAM_NOTIFY_AUTH_REQUIRED_Const },` |
|       - | 3536 | `	{"STREAM_NOTIFY_MIME_TYPE_IS",   PH7_STREAM_NOTIFY_MIME_TYPE_IS_Const },` |
|       - | 3537 | `	{"STREAM_NOTIFY_FILE_SIZE_IS",   PH7_STREAM_NOTIFY_FILE_SIZE_IS_Const },` |
|       - | 3538 | `	{"STREAM_NOTIFY_REDIRECTED",     PH7_STREAM_NOTIFY_REDIRECTED_Const },` |
|       - | 3539 | `	{"STREAM_NOTIFY_PROGRESS",       PH7_STREAM_NOTIFY_PROGRESS_Const },` |
|       - | 3540 | `	{"STREAM_NOTIFY_COMPLETED",      PH7_STREAM_NOTIFY_COMPLETED_Const },` |
|       - | 3541 | `	{"STREAM_NOTIFY_FAILURE",        PH7_STREAM_NOTIFY_FAILURE_Const },` |
|       - | 3542 | `	{"STREAM_NOTIFY_AUTH_RESULT",    PH7_STREAM_NOTIFY_AUTH_RESULT_Const },` |
|       - | 3543 | `	{"STREAM_NOTIFY_SEVERITY_INFO",  PH7_STREAM_NOTIFY_SEVERITY_INFO_Const },` |
|       - | 3544 | `	{"STREAM_NOTIFY_SEVERITY_WARN",  PH7_STREAM_NOTIFY_SEVERITY_WARN_Const },` |
|       - | 3545 | `	{"STREAM_NOTIFY_SEVERITY_ERR",   PH7_STREAM_NOTIFY_SEVERITY_ERR_Const },` |
|       - | 3546 | `	{"STREAM_FILTER_READ",           PH7_STREAM_FILTER_READ_Const },` |
|       - | 3547 | `	{"STREAM_FILTER_WRITE",          PH7_STREAM_FILTER_WRITE_Const },` |
|       - | 3548 | `	{"STREAM_FILTER_ALL",            PH7_STREAM_FILTER_ALL_Const },` |
|       - | 3549 | `	{"STREAM_SHUT_RD",               PH7_STREAM_SHUT_RD_Const },` |
|       - | 3550 | `	{"STREAM_SHUT_WR",               PH7_STREAM_SHUT_WR_Const },` |
|       - | 3551 | `	{"STREAM_SHUT_RDWR",             PH7_STREAM_SHUT_RDWR_Const },` |
|       - | 3552 | `	{"STREAM_OOB",                   PH7_STREAM_OOB_Const },` |
|       - | 3553 | `	{"STREAM_PEEK",                  PH7_STREAM_PEEK_Const },` |
|       - | 3554 | `#ifdef PH7_ENABLE_NET` |
|       - | 3555 | `	{"STREAM_PF_INET",               PH7_STREAM_PF_INET_Const },` |
|       - | 3556 | `	{"STREAM_PF_INET6",              PH7_STREAM_PF_INET6_Const },` |
|       - | 3557 | `	{"STREAM_PF_UNIX",               PH7_STREAM_PF_UNIX_Const },` |
|       - | 3558 | `	{"STREAM_SOCK_STREAM",           PH7_STREAM_SOCK_STREAM_Const },` |
|       - | 3559 | `	{"STREAM_SOCK_DGRAM",            PH7_STREAM_SOCK_DGRAM_Const },` |
|       - | 3560 | `	{"STREAM_SOCK_RAW",              PH7_STREAM_SOCK_RAW_Const },` |
|       - | 3561 | `	{"STREAM_SOCK_SEQPACKET",        PH7_STREAM_SOCK_SEQPACKET_Const },` |
|       - | 3562 | `	{"STREAM_SOCK_RDM",              PH7_STREAM_SOCK_RDM_Const },` |
|       - | 3563 | `	{"STREAM_IPPROTO_IP",            PH7_STREAM_IPPROTO_IP_Const },` |
|       - | 3564 | `	{"STREAM_IPPROTO_TCP",           PH7_STREAM_IPPROTO_TCP_Const },` |
|       - | 3565 | `	{"STREAM_IPPROTO_UDP",           PH7_STREAM_IPPROTO_UDP_Const },` |
|       - | 3566 | `	{"STREAM_IPPROTO_ICMP",          PH7_STREAM_IPPROTO_ICMP_Const },` |
|       - | 3567 | `	{"STREAM_IPPROTO_RAW",           PH7_STREAM_IPPROTO_RAW_Const },` |
|       - | 3568 | `#endif` |
|       - | 3569 | `	{"MT_RAND_MT19937",              PH7_MT_RAND_MT19937_Const },` |
|       - | 3570 | `	{"MT_RAND_PHP",                  PH7_MT_RAND_PHP_Const  },` |
|       - | 3571 | `	{"PHP_OUTPUT_HANDLER_WRITE",     PH7_OB_WRITE_Const     },` |
|       - | 3572 | `	{"PHP_OUTPUT_HANDLER_CONT",      PH7_OB_WRITE_Const     },` |
|       - | 3573 | `	{"PHP_OUTPUT_HANDLER_START",     PH7_OB_START_Const     },` |
|       - | 3574 | `	{"PHP_OUTPUT_HANDLER_CLEAN",     PH7_OB_CLEAN_Const     },` |
|       - | 3575 | `	{"PHP_OUTPUT_HANDLER_FLUSH",     PH7_OB_FLUSH_Const     },` |
|       - | 3576 | `	{"PHP_OUTPUT_HANDLER_FINAL",     PH7_OB_FINAL_Const     },` |
|       - | 3577 | `	{"PHP_OUTPUT_HANDLER_END",       PH7_OB_FINAL_Const     },` |
|       - | 3578 | `	{"PHP_OUTPUT_HANDLER_CLEANABLE", PH7_OB_CLEANABLE_Const },` |
|       - | 3579 | `	{"PHP_OUTPUT_HANDLER_FLUSHABLE", PH7_OB_FLUSHABLE_Const },` |
|       - | 3580 | `	{"PHP_OUTPUT_HANDLER_REMOVABLE", PH7_OB_REMOVABLE_Const },` |
|       - | 3581 | `	{"PHP_OUTPUT_HANDLER_STDFLAGS",  PH7_OB_STDFLAGS_Const  },` |
|       - | 3582 | `	{"PHP_OUTPUT_HANDLER_STARTED",   PH7_OB_STARTED_Const   },` |
|       - | 3583 | `	{"PHP_OUTPUT_HANDLER_DISABLED",  PH7_OB_DISABLED_Const  },` |
|       - | 3584 | `	{"PHP_OUTPUT_HANDLER_PROCESSED", PH7_OB_PROCESSED_Const },` |
|       - | 3585 | `	{"CONNECTION_NORMAL",    PH7_CONNECTION_NORMAL_Const  },` |
|       - | 3586 | `	{"CONNECTION_ABORTED",   PH7_CONNECTION_ABORTED_Const },` |
|       - | 3587 | `	{"CONNECTION_TIMEOUT",   PH7_CONNECTION_TIMEOUT_Const },` |
|       - | 3588 | `	{"ARRAY_FILTER_USE_KEY", PH7_ARRAY_FILTER_USE_KEY_Const },` |
|       - | 3589 | `	{"ARRAY_FILTER_USE_BOTH",PH7_ARRAY_FILTER_USE_BOTH_Const},` |
|       - | 3590 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|       - | 3591 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|       - | 3592 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|       - | 3593 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|       - | 3594 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|       - | 3595 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|       - | 3596 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|       - | 3597 | `	{"SORT_LOCALE_STRING",   PH7_SORT_LOCALE_STRING_Const },` |
|       - | 3598 | `	{"SORT_NATURAL",         PH7_SORT_NATURAL_Const },` |
|       - | 3599 | `	{"SORT_FLAG_CASE",       PH7_SORT_FLAG_CASE_Const },` |
|       - | 3600 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|       - | 3601 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|       - | 3602 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|       - | 3603 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|       - | 3604 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|       - | 3605 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|       - | 3606 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|       - | 3607 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|       - | 3608 | `	{"M_E",                  PH7_M_E_Const          },` |
|       - | 3609 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|       - | 3610 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|       - | 3611 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|       - | 3612 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|       - | 3613 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|       - | 3614 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|       - | 3615 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|       - | 3616 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|       - | 3617 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|       - | 3618 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|       - | 3619 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|       - | 3620 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|       - | 3621 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|       - | 3622 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|       - | 3623 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|       - | 3624 | `	{"NAN",                  PH7_NAN_Const          },` |
|       - | 3625 | `	{"INF",                  PH7_INF_Const          },` |
|       - | 3626 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|       - | 3627 | `	{"SUNFUNCS_RET_TIMESTAMP", PH7_SUNFUNCS_RET_TIMESTAMP_Const },` |
|       - | 3628 | `	{"SUNFUNCS_RET_STRING",  PH7_SUNFUNCS_RET_STRING_Const },` |
|       - | 3629 | `	{"SUNFUNCS_RET_DOUBLE",  PH7_SUNFUNCS_RET_DOUBLE_Const },` |
|       - | 3630 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|       - | 3631 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|       - | 3632 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|       - | 3633 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|       - | 3634 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|       - | 3635 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|       - | 3636 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|       - | 3637 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|       - | 3638 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|       - | 3639 | `	{"DATE_RFC3339_EXTENDED",PH7_DATE_RFC3339_EXTENDED_Const },` |
|       - | 3640 | `	{"DATE_RFC7231",         PH7_DATE_RFC7231_Const },` |
|       - | 3641 | `	{"DATE_ISO8601_EXPANDED",PH7_DATE_ISO8601_EXPANDED_Const },` |
|       - | 3642 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|       - | 3643 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|       - | 3644 | `	{"FILE_TEXT",            PH7_FILE_TEXT_Const    },` |
|       - | 3645 | `	{"FILE_BINARY",          PH7_FILE_TEXT_Const    },` |
|       - | 3646 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|       - | 3647 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|       - | 3648 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|       - | 3649 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|       - | 3650 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|       - | 3651 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|       - | 3652 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|       - | 3653 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|       - | 3654 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|       - | 3655 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|       - | 3656 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|       - | 3657 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|       - | 3658 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|       - | 3659 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|       - | 3660 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|       - | 3661 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|       - | 3662 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|       - | 3663 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|       - | 3664 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|       - | 3665 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|       - | 3666 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|       - | 3667 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|       - | 3668 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|       - | 3669 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|       - | 3670 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|       - | 3671 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|       - | 3672 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|       - | 3673 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|       - | 3674 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|       - | 3675 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|       - | 3676 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|       - | 3677 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|       - | 3678 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|       - | 3679 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|       - | 3680 | `	{"PATHINFO_ALL",         PH7_PATHINFO_ALL_Const },` |
|       - | 3681 | `	/* ASSERT_QUIET_EVAL was REMOVED in php 8.0: referencing it is an Error there */` |
|       - | 3682 | `#ifdef PH7_ENABLE_PCRE` |
|       - | 3683 | `	{"PCRE_VERSION",         PH7_PCRE_VERSION_Const  },` |
|       - | 3684 | `	{"PCRE_VERSION_MAJOR",   PH7_PCRE_VERSION_MAJOR_Const },` |
|       - | 3685 | `	{"PCRE_VERSION_MINOR",   PH7_PCRE_VERSION_MINOR_Const },` |
|       - | 3686 | `	{"PCRE_JIT_SUPPORT",     PH7_PCRE_JIT_SUPPORT_Const },` |
|       - | 3687 | `#endif` |
|       - | 3688 | `	{"ZEND_THREAD_SAFE",     PH7_ZEND_THREAD_SAFE_Const },` |
|       - | 3689 | `	{"ZEND_DEBUG_BUILD",     PH7_ZEND_DEBUG_BUILD_Const },` |
|       - | 3690 | `	{"PHP_ZTS",              PH7_PHP_ZTS_Const       },` |
|       - | 3691 | `	{"PHP_DEBUG",            PH7_PHP_DEBUG_Const     },` |
|       - | 3692 | `	{"INFO_GENERAL",         PH7_INFO_GENERAL_Const  },` |
|       - | 3693 | `	{"INFO_CREDITS",         PH7_INFO_CREDITS_Const  },` |
|       - | 3694 | `	{"INFO_CONFIGURATION",   PH7_INFO_CONFIGURATION_Const },` |
|       - | 3695 | `	{"INFO_MODULES",         PH7_INFO_MODULES_Const  },` |
|       - | 3696 | `	{"INFO_ENVIRONMENT",     PH7_INFO_ENVIRONMENT_Const },` |
|       - | 3697 | `	{"INFO_VARIABLES",       PH7_INFO_VARIABLES_Const },` |
|       - | 3698 | `	{"INFO_LICENSE",         PH7_INFO_LICENSE_Const  },` |
|       - | 3699 | `	{"INFO_ALL",             PH7_INFO_ALL_Const      },` |
|       - | 3700 | `	{"LC_CTYPE",             PH7_LC_CTYPE_Const      },` |
|       - | 3701 | `	{"LC_NUMERIC",           PH7_LC_NUMERIC_Const    },` |
|       - | 3702 | `	{"LC_TIME",              PH7_LC_TIME_Const       },` |
|       - | 3703 | `	{"LC_COLLATE",           PH7_LC_COLLATE_Const    },` |
|       - | 3704 | `	{"LC_MONETARY",          PH7_LC_MONETARY_Const   },` |
|       - | 3705 | `	{"LC_MESSAGES",          PH7_LC_MESSAGES_Const   },` |
|       - | 3706 | `	{"LC_ALL",               PH7_LC_ALL_Const        },` |
|       - | 3707 | `#ifndef __WINNT__` |
|       - | 3708 | `	/* ext/posix */` |
|       - | 3709 | `#ifdef F_OK` |
|       - | 3710 | `	{"POSIX_F_OK", PH7_POSIX_F_OK_Const },` |
|       - | 3711 | `#endif` |
|       - | 3712 | `#ifdef X_OK` |
|       - | 3713 | `	{"POSIX_X_OK", PH7_POSIX_X_OK_Const },` |
|       - | 3714 | `#endif` |
|       - | 3715 | `#ifdef W_OK` |
|       - | 3716 | `	{"POSIX_W_OK", PH7_POSIX_W_OK_Const },` |
|       - | 3717 | `#endif` |
|       - | 3718 | `#ifdef R_OK` |
|       - | 3719 | `	{"POSIX_R_OK", PH7_POSIX_R_OK_Const },` |
|       - | 3720 | `#endif` |
|       - | 3721 | `#ifdef S_IFREG` |
|       - | 3722 | `	{"POSIX_S_IFREG", PH7_POSIX_S_IFREG_Const },` |
|       - | 3723 | `#endif` |
|       - | 3724 | `#ifdef S_IFCHR` |
|       - | 3725 | `	{"POSIX_S_IFCHR", PH7_POSIX_S_IFCHR_Const },` |
|       - | 3726 | `#endif` |
|       - | 3727 | `#ifdef S_IFBLK` |
|       - | 3728 | `	{"POSIX_S_IFBLK", PH7_POSIX_S_IFBLK_Const },` |
|       - | 3729 | `#endif` |
|       - | 3730 | `#ifdef S_IFIFO` |
|       - | 3731 | `	{"POSIX_S_IFIFO", PH7_POSIX_S_IFIFO_Const },` |
|       - | 3732 | `#endif` |
|       - | 3733 | `#ifdef S_IFSOCK` |
|       - | 3734 | `	{"POSIX_S_IFSOCK", PH7_POSIX_S_IFSOCK_Const },` |
|       - | 3735 | `#endif` |
|       - | 3736 | `#ifdef RLIMIT_AS` |
|       - | 3737 | `	{"POSIX_RLIMIT_AS", PH7_POSIX_RLIMIT_AS_Const },` |
|       - | 3738 | `#endif` |
|       - | 3739 | `#ifdef RLIMIT_CORE` |
|       - | 3740 | `	{"POSIX_RLIMIT_CORE", PH7_POSIX_RLIMIT_CORE_Const },` |
|       - | 3741 | `#endif` |
|       - | 3742 | `#ifdef RLIMIT_CPU` |
|       - | 3743 | `	{"POSIX_RLIMIT_CPU", PH7_POSIX_RLIMIT_CPU_Const },` |
|       - | 3744 | `#endif` |
|       - | 3745 | `#ifdef RLIMIT_DATA` |
|       - | 3746 | `	{"POSIX_RLIMIT_DATA", PH7_POSIX_RLIMIT_DATA_Const },` |
|       - | 3747 | `#endif` |
|       - | 3748 | `#ifdef RLIMIT_FSIZE` |
|       - | 3749 | `	{"POSIX_RLIMIT_FSIZE", PH7_POSIX_RLIMIT_FSIZE_Const },` |
|       - | 3750 | `#endif` |
|       - | 3751 | `#ifdef RLIMIT_LOCKS` |
|       - | 3752 | `	{"POSIX_RLIMIT_LOCKS", PH7_POSIX_RLIMIT_LOCKS_Const },` |
|       - | 3753 | `#endif` |
|       - | 3754 | `#ifdef RLIMIT_MEMLOCK` |
|       - | 3755 | `	{"POSIX_RLIMIT_MEMLOCK", PH7_POSIX_RLIMIT_MEMLOCK_Const },` |
|       - | 3756 | `#endif` |
|       - | 3757 | `#ifdef RLIMIT_MSGQUEUE` |
|       - | 3758 | `	{"POSIX_RLIMIT_MSGQUEUE", PH7_POSIX_RLIMIT_MSGQUEUE_Const },` |
|       - | 3759 | `#endif` |
|       - | 3760 | `#ifdef RLIMIT_NICE` |
|       - | 3761 | `	{"POSIX_RLIMIT_NICE", PH7_POSIX_RLIMIT_NICE_Const },` |
|       - | 3762 | `#endif` |
|       - | 3763 | `#ifdef RLIMIT_NOFILE` |
|       - | 3764 | `	{"POSIX_RLIMIT_NOFILE", PH7_POSIX_RLIMIT_NOFILE_Const },` |
|       - | 3765 | `#endif` |
|       - | 3766 | `#ifdef RLIMIT_NPROC` |
|       - | 3767 | `	{"POSIX_RLIMIT_NPROC", PH7_POSIX_RLIMIT_NPROC_Const },` |
|       - | 3768 | `#endif` |
|       - | 3769 | `#ifdef RLIMIT_RSS` |
|       - | 3770 | `	{"POSIX_RLIMIT_RSS", PH7_POSIX_RLIMIT_RSS_Const },` |
|       - | 3771 | `#endif` |
|       - | 3772 | `#ifdef RLIMIT_RTPRIO` |
|       - | 3773 | `	{"POSIX_RLIMIT_RTPRIO", PH7_POSIX_RLIMIT_RTPRIO_Const },` |
|       - | 3774 | `#endif` |
|       - | 3775 | `#ifdef RLIMIT_RTTIME` |
|       - | 3776 | `	{"POSIX_RLIMIT_RTTIME", PH7_POSIX_RLIMIT_RTTIME_Const },` |
|       - | 3777 | `#endif` |
|       - | 3778 | `#ifdef RLIMIT_SIGPENDING` |
|       - | 3779 | `	{"POSIX_RLIMIT_SIGPENDING", PH7_POSIX_RLIMIT_SIGPENDING_Const },` |
|       - | 3780 | `#endif` |
|       - | 3781 | `#ifdef RLIMIT_STACK` |
|       - | 3782 | `	{"POSIX_RLIMIT_STACK", PH7_POSIX_RLIMIT_STACK_Const },` |
|       - | 3783 | `#endif` |
|       - | 3784 | `#ifdef _SC_ARG_MAX` |
|       - | 3785 | `	{"POSIX_SC_ARG_MAX", PH7_POSIX_SC_ARG_MAX_Const },` |
|       - | 3786 | `#endif` |
|       - | 3787 | `#ifdef _SC_CHILD_MAX` |
|       - | 3788 | `	{"POSIX_SC_CHILD_MAX", PH7_POSIX_SC_CHILD_MAX_Const },` |
|       - | 3789 | `#endif` |
|       - | 3790 | `#ifdef _SC_CLK_TCK` |
|       - | 3791 | `	{"POSIX_SC_CLK_TCK", PH7_POSIX_SC_CLK_TCK_Const },` |
|       - | 3792 | `#endif` |
|       - | 3793 | `#ifdef _SC_OPEN_MAX` |
|       - | 3794 | `	{"POSIX_SC_OPEN_MAX", PH7_POSIX_SC_OPEN_MAX_Const },` |
|       - | 3795 | `#endif` |
|       - | 3796 | `#ifdef _SC_PAGESIZE` |
|       - | 3797 | `	{"POSIX_SC_PAGESIZE", PH7_POSIX_SC_PAGESIZE_Const },` |
|       - | 3798 | `#endif` |
|       - | 3799 | `#ifdef _SC_NPROCESSORS_CONF` |
|       - | 3800 | `	{"POSIX_SC_NPROCESSORS_CONF", PH7_POSIX_SC_NPROCESSORS_CONF_Const },` |
|       - | 3801 | `#endif` |
|       - | 3802 | `#ifdef _SC_NPROCESSORS_ONLN` |
|       - | 3803 | `	{"POSIX_SC_NPROCESSORS_ONLN", PH7_POSIX_SC_NPROCESSORS_ONLN_Const },` |
|       - | 3804 | `#endif` |
|       - | 3805 | `#ifdef _PC_LINK_MAX` |
|       - | 3806 | `	{"POSIX_PC_LINK_MAX", PH7_POSIX_PC_LINK_MAX_Const },` |
|       - | 3807 | `#endif` |
|       - | 3808 | `#ifdef _PC_MAX_CANON` |
|       - | 3809 | `	{"POSIX_PC_MAX_CANON", PH7_POSIX_PC_MAX_CANON_Const },` |
|       - | 3810 | `#endif` |
|       - | 3811 | `#ifdef _PC_MAX_INPUT` |
|       - | 3812 | `	{"POSIX_PC_MAX_INPUT", PH7_POSIX_PC_MAX_INPUT_Const },` |
|       - | 3813 | `#endif` |
|       - | 3814 | `#ifdef _PC_NAME_MAX` |
|       - | 3815 | `	{"POSIX_PC_NAME_MAX", PH7_POSIX_PC_NAME_MAX_Const },` |
|       - | 3816 | `#endif` |
|       - | 3817 | `#ifdef _PC_PATH_MAX` |
|       - | 3818 | `	{"POSIX_PC_PATH_MAX", PH7_POSIX_PC_PATH_MAX_Const },` |
|       - | 3819 | `#endif` |
|       - | 3820 | `#ifdef _PC_PIPE_BUF` |
|       - | 3821 | `	{"POSIX_PC_PIPE_BUF", PH7_POSIX_PC_PIPE_BUF_Const },` |
|       - | 3822 | `#endif` |
|       - | 3823 | `#ifdef _PC_CHOWN_RESTRICTED` |
|       - | 3824 | `	{"POSIX_PC_CHOWN_RESTRICTED", PH7_POSIX_PC_CHOWN_RESTRICTED_Const },` |
|       - | 3825 | `#endif` |
|       - | 3826 | `#ifdef _PC_NO_TRUNC` |
|       - | 3827 | `	{"POSIX_PC_NO_TRUNC", PH7_POSIX_PC_NO_TRUNC_Const },` |
|       - | 3828 | `#endif` |
|       - | 3829 | `#ifdef _PC_ALLOC_SIZE_MIN` |
|       - | 3830 | `	{"POSIX_PC_ALLOC_SIZE_MIN", PH7_POSIX_PC_ALLOC_SIZE_MIN_Const },` |
|       - | 3831 | `#endif` |
|       - | 3832 | `#ifdef _PC_SYMLINK_MAX` |
|       - | 3833 | `	{"POSIX_PC_SYMLINK_MAX", PH7_POSIX_PC_SYMLINK_MAX_Const },` |
|       - | 3834 | `#endif` |
|       - | 3835 | `	{"POSIX_RLIMIT_INFINITY", PH7_POSIX_RLIMIT_INFINITY_Const },` |
|       - | 3836 | `#endif /* __WINNT__ */` |
|       - | 3837 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|       - | 3838 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|       - | 3839 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|       - | 3840 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|       - | 3841 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|       - | 3842 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|       - | 3843 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|       - | 3844 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|       - | 3845 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|       - | 3846 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|       - | 3847 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|       - | 3848 | `	{"FILE_NO_DEFAULT_CONTEXT", PH7_FILE_NO_DEFAULT_CONTEXT_Const },` |
|       - | 3849 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|       - | 3850 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|       - | 3851 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|       - | 3852 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|       - | 3853 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|       - | 3854 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|       - | 3855 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|       - | 3856 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|       - | 3857 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|       - | 3858 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|       - | 3859 | `	{"GLOB_AVAILABLE_FLAGS", PH7_GLOB_AVAILABLE_FLAGS_Const },` |
|       - | 3860 | `	{"STDIN",                PH7_STDIN_Const        },` |
|       - | 3861 | `	{"stdin",                PH7_STDIN_Const        },` |
|       - | 3862 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|       - | 3863 | `	{"stdout",               PH7_STDOUT_Const       },` |
|       - | 3864 | `	{"STDERR",               PH7_STDERR_Const       },` |
|       - | 3865 | `	{"stderr",               PH7_STDERR_Const       },` |
|       - | 3866 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|       - | 3867 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|       - | 3868 | `	{"INI_SCANNER_TYPED",    PH7_INI_SCANNER_TYPED_Const  },` |
|       - | 3869 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|       - | 3870 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|       - | 3871 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|       - | 3872 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|       - | 3873 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|       - | 3874 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|       - | 3875 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|       - | 3876 | `	{"EXTR_REFS",            PH7_EXTR_REFS_Const        },` |
|       - | 3877 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|       - | 3878 | `	{"HASH_HMAC",              PH7_HASH_HMAC_Const},` |
|       - | 3879 | `	{"CRYPT_SALT_LENGTH",      PH7_CRYPT_SALT_LENGTH_Const},` |
|       - | 3880 | `	{"CRYPT_STD_DES",          PH7_CRYPT_ONE_Const},` |
|       - | 3881 | `	{"CRYPT_EXT_DES",          PH7_CRYPT_ONE_Const},` |
|       - | 3882 | `	{"CRYPT_MD5",              PH7_CRYPT_ONE_Const},` |
|       - | 3883 | `	{"CRYPT_BLOWFISH",         PH7_CRYPT_ONE_Const},` |
|       - | 3884 | `	{"CRYPT_SHA256",           PH7_CRYPT_ONE_Const},` |
|       - | 3885 | `	{"CRYPT_SHA512",           PH7_CRYPT_ONE_Const},` |
|       - | 3886 | `#endif` |
|       - | 3887 | `	{"ICONV_IMPL",             PH7_ICONV_IMPL_Const},` |
|       - | 3888 | `	{"ICONV_VERSION",          PH7_ICONV_VERSION_Const},` |
|       - | 3889 | `	{"ICONV_MIME_DECODE_STRICT", PH7_ICONV_MIME_DECODE_STRICT_Const},` |
|       - | 3890 | `	{"ICONV_MIME_DECODE_CONTINUE_ON_ERROR", PH7_ICONV_MIME_DECODE_CONTINUE_ON_ERROR_Const},` |
|       - | 3891 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|       - | 3892 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|       - | 3893 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|       - | 3894 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|       - | 3895 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|       - | 3896 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|       - | 3897 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|       - | 3898 | `	{"JSON_OBJECT_AS_ARRAY",   PH7_JSON_OBJECT_AS_ARRAY_Const},` |
|       - | 3899 | `	{"JSON_PARTIAL_OUTPUT_ON_ERROR", PH7_JSON_PARTIAL_OUTPUT_ON_ERROR_Const},` |
|       - | 3900 | `	{"JSON_PRESERVE_ZERO_FRACTION",  PH7_JSON_PRESERVE_ZERO_FRACTION_Const},` |
|       - | 3901 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|       - | 3902 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|       - | 3903 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|       - | 3904 | `	{"JSON_UNESCAPED_LINE_TERMINATORS", PH7_JSON_UNESCAPED_LINE_TERMINATORS_Const},` |
|       - | 3905 | `	{"JSON_INVALID_UTF8_IGNORE", PH7_JSON_INVALID_UTF8_IGNORE_Const},` |
|       - | 3906 | `	{"JSON_INVALID_UTF8_SUBSTITUTE", PH7_JSON_INVALID_UTF8_SUBSTITUTE_Const},` |
|       - | 3907 | `	{"JSON_THROW_ON_ERROR",    PH7_JSON_THROW_ON_ERROR_Const},` |
|       - | 3908 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|       - | 3909 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|       - | 3910 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|       - | 3911 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|       - | 3912 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|       - | 3913 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|       - | 3914 | `	{"JSON_ERROR_RECURSION", PH7_JSON_ERROR_RECURSION_Const},` |
|       - | 3915 | `	{"JSON_ERROR_UNSUPPORTED_TYPE", PH7_JSON_ERROR_UNSUPPORTED_TYPE_Const},` |
|       - | 3916 | `	{"JSON_ERROR_INVALID_PROPERTY_NAME", PH7_JSON_ERROR_INVALID_PROPERTY_NAME_Const},` |
|       - | 3917 | `	{"JSON_ERROR_UTF16",     PH7_JSON_ERROR_UTF16_Const},` |
|       - | 3918 | `	{"JSON_ERROR_NON_BACKED_ENUM", PH7_JSON_ERROR_NON_BACKED_ENUM_Const},` |
|       - | 3919 | `	{"JSON_ERROR_INF_OR_NAN", PH7_JSON_ERROR_INF_OR_NAN_Const},` |
|       - | 3920 | ``	/* `self`, `parent` and `static` are KEYWORDS in php, not constants: using one as a bare`` |
|       - | 3921 | ``	 * word is an "Undefined constant" Error (or a parse error for `static`). PH7 registered`` |
|       - | 3922 | `	 * them as constants that quietly expanded to the class name / NULL, so a typo'd bare` |
|       - | 3923 | ``	 * word silently produced a value. The `self::`/`parent::`/`static::` forms are handled`` |
|       - | 3924 | ``	 * by the `::` compile path and do not go through the constant table. */`` |
|       - | 3925 | `#ifdef PH7_ENABLE_ZLIB` |
|       - | 3926 | `	{"FORCE_GZIP",             PH7_FORCE_GZIP_Const },` |
|       - | 3927 | `	{"FORCE_DEFLATE",          PH7_FORCE_DEFLATE_Const },` |
|       - | 3928 | `	{"ZLIB_ENCODING_RAW",      PH7_ZLIB_ENCODING_RAW_Const },` |
|       - | 3929 | `	{"ZLIB_ENCODING_GZIP",     PH7_ZLIB_ENCODING_GZIP_Const },` |
|       - | 3930 | `	{"ZLIB_ENCODING_DEFLATE",  PH7_ZLIB_ENCODING_DEFLATE_Const },` |
|       - | 3931 | `	{"ZLIB_NO_FLUSH",          PH7_ZLIB_NO_FLUSH_Const },` |
|       - | 3932 | `	{"ZLIB_PARTIAL_FLUSH",     PH7_ZLIB_PARTIAL_FLUSH_Const },` |
|       - | 3933 | `	{"ZLIB_SYNC_FLUSH",        PH7_ZLIB_SYNC_FLUSH_Const },` |
|       - | 3934 | `	{"ZLIB_FULL_FLUSH",        PH7_ZLIB_FULL_FLUSH_Const },` |
|       - | 3935 | `	{"ZLIB_BLOCK",             PH7_ZLIB_BLOCK_Const },` |
|       - | 3936 | `	{"ZLIB_FINISH",            PH7_ZLIB_FINISH_Const },` |
|       - | 3937 | `	{"ZLIB_FILTERED",          PH7_ZLIB_FILTERED_Const },` |
|       - | 3938 | `	{"ZLIB_HUFFMAN_ONLY",      PH7_ZLIB_HUFFMAN_ONLY_Const },` |
|       - | 3939 | `	{"ZLIB_RLE",               PH7_ZLIB_RLE_Const },` |
|       - | 3940 | `	{"ZLIB_FIXED",             PH7_ZLIB_FIXED_Const },` |
|       - | 3941 | `	{"ZLIB_DEFAULT_STRATEGY",  PH7_ZLIB_DEFAULT_STRATEGY_Const },` |
|       - | 3942 | `	{"ZLIB_VERSION",           PH7_ZLIB_VERSION_Const },` |
|       - | 3943 | `	{"ZLIB_VERNUM",            PH7_ZLIB_VERNUM_Const },` |
|       - | 3944 | `	{"ZLIB_OK",                PH7_ZLIB_OK_Const },` |
|       - | 3945 | `	{"ZLIB_STREAM_END",        PH7_ZLIB_STREAM_END_Const },` |
|       - | 3946 | `	{"ZLIB_NEED_DICT",         PH7_ZLIB_NEED_DICT_Const },` |
|       - | 3947 | `	{"ZLIB_ERRNO",             PH7_ZLIB_ERRNO_Const },` |
|       - | 3948 | `	{"ZLIB_STREAM_ERROR",      PH7_ZLIB_STREAM_ERROR_Const },` |
|       - | 3949 | `	{"ZLIB_DATA_ERROR",        PH7_ZLIB_DATA_ERROR_Const },` |
|       - | 3950 | `	{"ZLIB_MEM_ERROR",         PH7_ZLIB_MEM_ERROR_Const },` |
|       - | 3951 | `	{"ZLIB_BUF_ERROR",         PH7_ZLIB_BUF_ERROR_Const },` |
|       - | 3952 | `	{"ZLIB_VERSION_ERROR",     PH7_ZLIB_VERSION_ERROR_Const },` |
|       - | 3953 | `#endif /* PH7_ENABLE_ZLIB */` |
|       - | 3954 | `#ifdef PH7_ENABLE_OPENSSL` |
|       - | 3955 | `	{"OPENSSL_VERSION_TEXT",              PH7_OPENSSL_VERSION_TEXT_Const },` |
|       - | 3956 | `	{"OPENSSL_VERSION_NUMBER",            PH7_OPENSSL_VERSION_NUMBER_Const },` |
|       - | 3957 | `	{"X509_PURPOSE_SSL_CLIENT",           PH7_X509_PURPOSE_SSL_CLIENT_Const },` |
|       - | 3958 | `	{"X509_PURPOSE_SSL_SERVER",           PH7_X509_PURPOSE_SSL_SERVER_Const },` |
|       - | 3959 | `	{"X509_PURPOSE_NS_SSL_SERVER",        PH7_X509_PURPOSE_NS_SSL_SERVER_Const },` |
|       - | 3960 | `	{"X509_PURPOSE_SMIME_SIGN",           PH7_X509_PURPOSE_SMIME_SIGN_Const },` |
|       - | 3961 | `	{"X509_PURPOSE_SMIME_ENCRYPT",        PH7_X509_PURPOSE_SMIME_ENCRYPT_Const },` |
|       - | 3962 | `	{"X509_PURPOSE_CRL_SIGN",             PH7_X509_PURPOSE_CRL_SIGN_Const },` |
|       - | 3963 | `	{"X509_PURPOSE_ANY",                  PH7_X509_PURPOSE_ANY_Const },` |
|       - | 3964 | `	{"X509_PURPOSE_OCSP_HELPER",          PH7_X509_PURPOSE_OCSP_HELPER_Const },` |
|       - | 3965 | `	{"X509_PURPOSE_TIMESTAMP_SIGN",       PH7_X509_PURPOSE_TIMESTAMP_SIGN_Const },` |
|       - | 3966 | `	{"OPENSSL_ALGO_SHA1",                 PH7_OPENSSL_ALGO_SHA1_Const },` |
|       - | 3967 | `	{"OPENSSL_ALGO_MD5",                  PH7_OPENSSL_ALGO_MD5_Const },` |
|       - | 3968 | `	{"OPENSSL_ALGO_MD4",                  PH7_OPENSSL_ALGO_MD4_Const },` |
|       - | 3969 | `	{"OPENSSL_ALGO_SHA224",               PH7_OPENSSL_ALGO_SHA224_Const },` |
|       - | 3970 | `	{"OPENSSL_ALGO_SHA256",               PH7_OPENSSL_ALGO_SHA256_Const },` |
|       - | 3971 | `	{"OPENSSL_ALGO_SHA384",               PH7_OPENSSL_ALGO_SHA384_Const },` |
|       - | 3972 | `	{"OPENSSL_ALGO_SHA512",               PH7_OPENSSL_ALGO_SHA512_Const },` |
|       - | 3973 | `	{"OPENSSL_ALGO_RMD160",               PH7_OPENSSL_ALGO_RMD160_Const },` |
|       - | 3974 | `	{"PKCS7_DETACHED",                    PH7_PKCS7_DETACHED_Const },` |
|       - | 3975 | `	{"PKCS7_TEXT",                        PH7_PKCS7_TEXT_Const },` |
|       - | 3976 | `	{"PKCS7_NOINTERN",                    PH7_PKCS7_NOINTERN_Const },` |
|       - | 3977 | `	{"PKCS7_NOVERIFY",                    PH7_PKCS7_NOVERIFY_Const },` |
|       - | 3978 | `	{"PKCS7_NOCHAIN",                     PH7_PKCS7_NOCHAIN_Const },` |
|       - | 3979 | `	{"PKCS7_NOCERTS",                     PH7_PKCS7_NOCERTS_Const },` |
|       - | 3980 | `	{"PKCS7_NOATTR",                      PH7_PKCS7_NOATTR_Const },` |
|       - | 3981 | `	{"PKCS7_BINARY",                      PH7_PKCS7_BINARY_Const },` |
|       - | 3982 | `	{"PKCS7_NOSIGS",                      PH7_PKCS7_NOSIGS_Const },` |
|       - | 3983 | `	{"PKCS7_NOOLDMIMETYPE",               PH7_PKCS7_NOOLDMIMETYPE_Const },` |
|       - | 3984 | `	{"PKCS7_NOSMIMECAP",                  PH7_PKCS7_NOSMIMECAP_Const },` |
|       - | 3985 | `	{"PKCS7_CRLFEOL",                     PH7_PKCS7_CRLFEOL_Const },` |
|       - | 3986 | `	{"PKCS7_NOCRL",                       PH7_PKCS7_NOCRL_Const },` |
|       - | 3987 | `	{"PKCS7_NO_DUAL_CONTENT",             PH7_PKCS7_NO_DUAL_CONTENT_Const },` |
|       - | 3988 | `	{"OPENSSL_CMS_DETACHED",              PH7_OPENSSL_CMS_DETACHED_Const },` |
|       - | 3989 | `	{"OPENSSL_CMS_TEXT",                  PH7_OPENSSL_CMS_TEXT_Const },` |
|       - | 3990 | `	{"OPENSSL_CMS_NOINTERN",              PH7_OPENSSL_CMS_NOINTERN_Const },` |
|       - | 3991 | `	{"OPENSSL_CMS_NOVERIFY",              PH7_OPENSSL_CMS_NOVERIFY_Const },` |
|       - | 3992 | `	{"OPENSSL_CMS_NOCERTS",               PH7_OPENSSL_CMS_NOCERTS_Const },` |
|       - | 3993 | `	{"OPENSSL_CMS_NOATTR",                PH7_OPENSSL_CMS_NOATTR_Const },` |
|       - | 3994 | `	{"OPENSSL_CMS_BINARY",                PH7_OPENSSL_CMS_BINARY_Const },` |
|       - | 3995 | `	{"OPENSSL_CMS_NOSIGS",                PH7_OPENSSL_CMS_NOSIGS_Const },` |
|       - | 3996 | `	{"OPENSSL_CMS_OLDMIMETYPE",           PH7_OPENSSL_CMS_OLDMIMETYPE_Const },` |
|       - | 3997 | `	{"OPENSSL_PKCS1_PADDING",             PH7_OPENSSL_PKCS1_PADDING_Const },` |
|       - | 3998 | `	{"OPENSSL_NO_PADDING",                PH7_OPENSSL_NO_PADDING_Const },` |
|       - | 3999 | `	{"OPENSSL_PKCS1_OAEP_PADDING",        PH7_OPENSSL_PKCS1_OAEP_PADDING_Const },` |
|       - | 4000 | `	{"OPENSSL_PKCS1_PSS_PADDING",         PH7_OPENSSL_PKCS1_PSS_PADDING_Const },` |
|       - | 4001 | `	{"OPENSSL_DEFAULT_STREAM_CIPHERS",    PH7_OPENSSL_DEFAULT_STREAM_CIPHERS_Const },` |
|       - | 4002 | `	{"OPENSSL_CIPHER_RC2_40",             PH7_OPENSSL_CIPHER_RC2_40_Const },` |
|       - | 4003 | `	{"OPENSSL_CIPHER_RC2_128",            PH7_OPENSSL_CIPHER_RC2_128_Const },` |
|       - | 4004 | `	{"OPENSSL_CIPHER_RC2_64",             PH7_OPENSSL_CIPHER_RC2_64_Const },` |
|       - | 4005 | `	{"OPENSSL_CIPHER_DES",                PH7_OPENSSL_CIPHER_DES_Const },` |
|       - | 4006 | `	{"OPENSSL_CIPHER_3DES",               PH7_OPENSSL_CIPHER_3DES_Const },` |
|       - | 4007 | `	{"OPENSSL_CIPHER_AES_128_CBC",        PH7_OPENSSL_CIPHER_AES_128_CBC_Const },` |
|       - | 4008 | `	{"OPENSSL_CIPHER_AES_192_CBC",        PH7_OPENSSL_CIPHER_AES_192_CBC_Const },` |
|       - | 4009 | `	{"OPENSSL_CIPHER_AES_256_CBC",        PH7_OPENSSL_CIPHER_AES_256_CBC_Const },` |
|       - | 4010 | `	{"OPENSSL_KEYTYPE_RSA",               PH7_OPENSSL_KEYTYPE_RSA_Const },` |
|       - | 4011 | `	{"OPENSSL_KEYTYPE_DSA",               PH7_OPENSSL_KEYTYPE_DSA_Const },` |
|       - | 4012 | `	{"OPENSSL_KEYTYPE_DH",                PH7_OPENSSL_KEYTYPE_DH_Const },` |
|       - | 4013 | `	{"OPENSSL_KEYTYPE_EC",                PH7_OPENSSL_KEYTYPE_EC_Const },` |
|       - | 4014 | `	{"OPENSSL_KEYTYPE_X25519",            PH7_OPENSSL_KEYTYPE_X25519_Const },` |
|       - | 4015 | `	{"OPENSSL_KEYTYPE_ED25519",           PH7_OPENSSL_KEYTYPE_ED25519_Const },` |
|       - | 4016 | `	{"OPENSSL_KEYTYPE_X448",              PH7_OPENSSL_KEYTYPE_X448_Const },` |
|       - | 4017 | `	{"OPENSSL_KEYTYPE_ED448",             PH7_OPENSSL_KEYTYPE_ED448_Const },` |
|       - | 4018 | `	{"OPENSSL_RAW_DATA",                  PH7_OPENSSL_RAW_DATA_Const },` |
|       - | 4019 | `	{"OPENSSL_ZERO_PADDING",              PH7_OPENSSL_ZERO_PADDING_Const },` |
|       - | 4020 | `	{"OPENSSL_DONT_ZERO_PAD_KEY",         PH7_OPENSSL_DONT_ZERO_PAD_KEY_Const },` |
|       - | 4021 | `	{"OPENSSL_TLSEXT_SERVER_NAME",        PH7_OPENSSL_TLSEXT_SERVER_NAME_Const },` |
|       - | 4022 | `	{"STREAM_CRYPTO_METHOD_SSLv2_CLIENT",              PH7_STREAM_CRYPTO_METHOD_SSLv2_CLIENT_Const },` |
|       - | 4023 | `	{"STREAM_CRYPTO_METHOD_SSLv2_SERVER",              PH7_STREAM_CRYPTO_METHOD_SSLv2_SERVER_Const },` |
|       - | 4024 | `	{"STREAM_CRYPTO_METHOD_SSLv3_CLIENT",              PH7_STREAM_CRYPTO_METHOD_SSLv3_CLIENT_Const },` |
|       - | 4025 | `	{"STREAM_CRYPTO_METHOD_SSLv3_SERVER",              PH7_STREAM_CRYPTO_METHOD_SSLv3_SERVER_Const },` |
|       - | 4026 | `	{"STREAM_CRYPTO_METHOD_SSLv23_CLIENT",             PH7_STREAM_CRYPTO_METHOD_SSLv23_CLIENT_Const },` |
|       - | 4027 | `	{"STREAM_CRYPTO_METHOD_SSLv23_SERVER",             PH7_STREAM_CRYPTO_METHOD_SSLv23_SERVER_Const },` |
|       - | 4028 | `	{"STREAM_CRYPTO_METHOD_TLS_CLIENT",                PH7_STREAM_CRYPTO_METHOD_TLS_CLIENT_Const },` |
|       - | 4029 | `	{"STREAM_CRYPTO_METHOD_TLS_SERVER",                PH7_STREAM_CRYPTO_METHOD_TLS_SERVER_Const },` |
|       - | 4030 | `	{"STREAM_CRYPTO_METHOD_TLSv1_0_CLIENT",            PH7_STREAM_CRYPTO_METHOD_TLSv1_0_CLIENT_Const },` |
|       - | 4031 | `	{"STREAM_CRYPTO_METHOD_TLSv1_0_SERVER",            PH7_STREAM_CRYPTO_METHOD_TLSv1_0_SERVER_Const },` |
|       - | 4032 | `	{"STREAM_CRYPTO_METHOD_TLSv1_1_CLIENT",            PH7_STREAM_CRYPTO_METHOD_TLSv1_1_CLIENT_Const },` |
|       - | 4033 | `	{"STREAM_CRYPTO_METHOD_TLSv1_1_SERVER",            PH7_STREAM_CRYPTO_METHOD_TLSv1_1_SERVER_Const },` |
|       - | 4034 | `	{"STREAM_CRYPTO_METHOD_TLSv1_2_CLIENT",            PH7_STREAM_CRYPTO_METHOD_TLSv1_2_CLIENT_Const },` |
|       - | 4035 | `	{"STREAM_CRYPTO_METHOD_TLSv1_2_SERVER",            PH7_STREAM_CRYPTO_METHOD_TLSv1_2_SERVER_Const },` |
|       - | 4036 | `	{"STREAM_CRYPTO_METHOD_TLSv1_3_CLIENT",            PH7_STREAM_CRYPTO_METHOD_TLSv1_3_CLIENT_Const },` |
|       - | 4037 | `	{"STREAM_CRYPTO_METHOD_TLSv1_3_SERVER",            PH7_STREAM_CRYPTO_METHOD_TLSv1_3_SERVER_Const },` |
|       - | 4038 | `	{"STREAM_CRYPTO_METHOD_ANY_CLIENT",                PH7_STREAM_CRYPTO_METHOD_ANY_CLIENT_Const },` |
|       - | 4039 | `	{"STREAM_CRYPTO_METHOD_ANY_SERVER",                PH7_STREAM_CRYPTO_METHOD_ANY_SERVER_Const },` |
|       - | 4040 | `	{"STREAM_CRYPTO_PROTO_SSLv3",                      PH7_STREAM_CRYPTO_PROTO_SSLv3_Const },` |
|       - | 4041 | `	{"STREAM_CRYPTO_PROTO_TLSv1_0",                    PH7_STREAM_CRYPTO_PROTO_TLSv1_0_Const },` |
|       - | 4042 | `	{"STREAM_CRYPTO_PROTO_TLSv1_1",                    PH7_STREAM_CRYPTO_PROTO_TLSv1_1_Const },` |
|       - | 4043 | `	{"STREAM_CRYPTO_PROTO_TLSv1_2",                    PH7_STREAM_CRYPTO_PROTO_TLSv1_2_Const },` |
|       - | 4044 | `	{"STREAM_CRYPTO_PROTO_TLSv1_3",                    PH7_STREAM_CRYPTO_PROTO_TLSv1_3_Const },` |
|       - | 4045 | `	{"OPENSSL_ENCODING_DER",              PH7_OPENSSL_ENCODING_DER_Const },` |
|       - | 4046 | `	{"OPENSSL_ENCODING_SMIME",            PH7_OPENSSL_ENCODING_SMIME_Const },` |
|       - | 4047 | `	{"OPENSSL_ENCODING_PEM",              PH7_OPENSSL_ENCODING_PEM_Const },` |
|       - | 4048 | `#endif /* PH7_ENABLE_OPENSSL */` |
|       - | 4049 | `	{"FILEINFO_NONE",          PH7_FILEINFO_NONE_Const },` |
|       - | 4050 | `	{"FILEINFO_SYMLINK",       PH7_FILEINFO_SYMLINK_Const },` |
|       - | 4051 | `	{"FILEINFO_MIME",          PH7_FILEINFO_MIME_Const },` |
|       - | 4052 | `	{"FILEINFO_MIME_TYPE",     PH7_FILEINFO_MIME_TYPE_Const },` |
|       - | 4053 | `	{"FILEINFO_MIME_ENCODING", PH7_FILEINFO_MIME_ENCODING_Const },` |
|       - | 4054 | `	{"FILEINFO_DEVICES",       PH7_FILEINFO_DEVICES_Const },` |
|       - | 4055 | `	{"FILEINFO_CONTINUE",      PH7_FILEINFO_CONTINUE_Const },` |
|       - | 4056 | `	{"FILEINFO_PRESERVE_ATIME",PH7_FILEINFO_PRESERVE_ATIME_Const },` |
|       - | 4057 | `	{"FILEINFO_RAW",           PH7_FILEINFO_RAW_Const },` |
|       - | 4058 | `	{"FILEINFO_APPLE",         PH7_FILEINFO_APPLE_Const },` |
|       - | 4059 | `	{"FILEINFO_EXTENSION",     PH7_FILEINFO_EXTENSION_Const },` |
|       - | 4060 | `	{"__CLASS__",            PH7_class_magic_Const  }` |
|       - | 4061 | `};` |
|       - | 4062 | `/*` |
|       - | 4063 | ` * Expand a built-in constant by name STRAIGHT OFF the table above, without` |
|       - | 4064 | ` * asking hConstant.` |
|       - | 4065 | ` *` |
|       - | 4066 | ` * php.ini is read before a line of the script is compiled, and PHL's equivalent` |
|       - | 4067 | ` * window -- PH7_VmApplyEngineIni -- opens between PH7_VmInit and` |
|       - | 4068 | ` * PH7_VmMakeReady, which is where the table below is installed. So a directive` |
|       - | 4069 | `` * that names a constant, and `error_reporting = E_ALL & ~E_DEPRECATED` is the`` |
|       - | 4070 | ` * one everybody writes, has no hash to look it up in yet and used to read every` |
|       - | 4071 | ` * name as 0. php has the same window and answers it the same way: the constants` |
|       - | 4072 | `` * that exist for an ini value are the engine's own, which is why `M_PI` there is`` |
|       - | 4073 | ` * the four letters and not 3.14159.` |
|       - | 4074 | ` *` |
|       - | 4075 | ` * Which constants php has by then was read off it directly, by asking an ini` |
|       - | 4076 | `` * value for `1\|X` and watching whether X moved the answer: the E_ and PHP_`` |
|       - | 4077 | ` * families resolve (E_USER_ERROR is 256, PHP_INT_SIZE is 8) and ext/standard's` |
|       - | 4078 | ` * do not (SORT_ASC and M_E are their own names, hence 0). That is the line drawn` |
|       - | 4079 | ` * here -- everything else in the table below belongs to an extension php starts` |
|       - | 4080 | ` * after it has read the file, and answering it would be answering MORE than php.` |
|       - | 4081 | ` * Returns TRUE when the name was one of those; pOut is the caller's, initialized.` |
|       - | 4082 | ` */` |
|     426 | 4083 | `PH7_PRIVATE int PH7_ExpandBuiltinConstant(ph7_vm *pVm,const char *zName,sxu32 nName,ph7_value *pOut)` |
|       4 | 4084 | `{` |
|       - | 4085 | `	sxu32 n;` |
|     456 | 4086 | `	if( !((nName > 2 && zName[0] == 'E' && zName[1] == '_')` |
|     239 | 4087 | `	   \|\| (nName > 4 && SyMemcmp(zName,"PHP_",4) == 0)) ){` |
|     386 | 4088 | `		return 0;` |
|       - | 4089 | `	}` |
|    6996 | 4090 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
|    6992 | 4091 | `		if( SyStrlen(aBuiltIn[n].zName) == nName` |
|    3605 | 4092 | `		 && SyMemcmp(aBuiltIn[n].zName,zName,nName) == 0 ){` |
|      48 | 4093 | `			aBuiltIn[n].xExpand(pOut,(void *)pVm);` |
|      48 | 4094 | `			return 1;` |
|       - | 4095 | `		}` |
|    3478 | 4096 | `	}` |
|     ! 0 | 4097 | `	return 0;` |
|     217 | 4098 | `}` |
|       - | 4099 | `/*` |
|       - | 4100 | ` * Register the built-in constants defined above.` |
|       - | 4101 | ` */` |
|    6691 | 4102 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|       5 | 4103 | `{` |
|       - | 4104 | `	sxu32 n;` |
|       - | 4105 | `	/*` |
|       - | 4106 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|       - | 4107 | `	 * that trigger the constant invocation as their private data.` |
|       - | 4108 | `	 */` |
| 4161840 | 4109 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 4155149 | 4110 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
| 2064125 | 4111 | `	}` |
|    6696 | 4112 | `}` |
|       - | 4113 | `/*` |
|       - | 4114 | ` * The constants php 8.x deprecated the SYMBOL of, and the reason clause it ends` |
|       - | 4115 | `` * the notice with. Naming one raises `Constant X is deprecated since <clause>`;`` |
|       - | 4116 | ` * LISTING the table does not, which is what pVm->bConstEnum is for.` |
|       - | 4117 | ` *` |
|       - | 4118 | ` * They are marked rather than raised from their own expanders because they are` |
|       - | 4119 | ` * registered in five different units -- date's, random's, the file flags',` |
|       - | 4120 | ``  * curl's and dom's -- and because the export format's `<persistent, deprecated>` `` |
|       - | 4121 | ` * tag has to read the same fact. The stamp runs once, after every extension has` |
|       - | 4122 | ` * installed, so a name a build does not carry is simply skipped.` |
|       - | 4123 | ` */` |
|       - | 4124 | `static const struct {` |
|       - | 4125 | `	const char *zName;` |
|       - | 4126 | `	const char *zWhy;` |
|       - | 4127 | `} aDeprecatedConst[] = {` |
|       - | 4128 | `	{ "E_STRICT",                "8.4, the error level was removed" },` |
|       - | 4129 | `	{ "DATE_RFC7231",` |
|       - | 4130 | `	  "8.5, as this format ignores the associated timezone and always uses GMT" },` |
|       - | 4131 | `	{ "FILE_TEXT",               "8.1, as the constant has no effect" },` |
|       - | 4132 | `	{ "FILE_BINARY",             "8.1, as the constant has no effect" },` |
|       - | 4133 | `	{ "MT_RAND_PHP",` |
|       - | 4134 | `	  "8.3, as it uses a biased non-standard variant of Mt19937" },` |
|       - | 4135 | `	{ "CURLOPT_BINARYTRANSFER",  "8.4, as it had no effect since 5.1.2" },` |
|       - | 4136 | `	{ "DOM_PHP_ERR",             "8.4, as it is no longer used" },` |
|       - | 4137 | `};` |
|    6691 | 4138 | `PH7_PRIVATE void PH7_MarkDeprecatedConstants(ph7_vm *pVm)` |
|       5 | 4139 | `{` |
|       - | 4140 | `	sxu32 n;` |
|   53533 | 4141 | `	for( n = 0 ; n < SX_ARRAYSIZE(aDeprecatedConst) ; ++n ){` |
|   93679 | 4142 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hConstant,` |
|   46837 | 4143 | `			(const void *)aDeprecatedConst[n].zName,` |
|   46837 | 4144 | `			SyStrlen(aDeprecatedConst[n].zName));` |
|   46842 | 4145 | `		if( pEntry ){` |
|   46842 | 4146 | `			((ph7_constant *)pEntry->pUserData)->zDeprecated = aDeprecatedConst[n].zWhy;` |
|   23380 | 4147 | `		}` |
|   23385 | 4148 | `	}` |
|    6696 | 4149 | `}` |
