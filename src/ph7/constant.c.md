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
|     254 |   34 | `static void PH7_VER_Const(ph7_value *pVal,void *pUnused)` |
|       5 |   35 | `{` |
|     127 |   36 | `	SXUNUSED(pUnused);` |
|     259 |   37 | `	ph7_value_string(pVal,ph7_lib_signature(),-1/*Compute length automatically*/);` |
|     259 |   38 | `}` |
|       - |   39 | `/*` |
|       - |   40 | ` * PHP_VERSION, PHP_MAJOR_VERSION, PHP_MINOR_VERSION, PHP_RELEASE_VERSION,` |
|       - |   41 | ` * PHP_EXTRA_VERSION, PHP_VERSION_ID` |
|       - |   42 | ` *   Expand the PHP-compatibility version PHL advertises (see PHP_COMPAT_* in ph7.h).` |
|       - |   43 | ` */` |
|      96 |   44 | `static void PH7_PHPVerConst(ph7_value *pVal,void *pUnused)` |
|       5 |   45 | `{` |
|      48 |   46 | `	SXUNUSED(pUnused);` |
|     101 |   47 | `	ph7_value_string(pVal,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION)-1);` |
|     101 |   48 | `}` |
|      86 |   49 | `static void PH7_PHPMajorConst(ph7_value *pVal,void *pUnused)` |
|       5 |   50 | `{` |
|      43 |   51 | `	SXUNUSED(pUnused);` |
|      91 |   52 | `	ph7_value_int64(pVal,PHP_COMPAT_MAJOR_VERSION);` |
|      91 |   53 | `}` |
|      86 |   54 | `static void PH7_PHPMinorConst(ph7_value *pVal,void *pUnused)` |
|       5 |   55 | `{` |
|      43 |   56 | `	SXUNUSED(pUnused);` |
|      91 |   57 | `	ph7_value_int64(pVal,PHP_COMPAT_MINOR_VERSION);` |
|      91 |   58 | `}` |
|      86 |   59 | `static void PH7_PHPReleaseConst(ph7_value *pVal,void *pUnused)` |
|       5 |   60 | `{` |
|      43 |   61 | `	SXUNUSED(pUnused);` |
|      91 |   62 | `	ph7_value_int64(pVal,PHP_COMPAT_RELEASE_VERSION);` |
|      91 |   63 | `}` |
|      84 |   64 | `static void PH7_PHPExtraConst(ph7_value *pVal,void *pUnused)` |
|       5 |   65 | `{` |
|      42 |   66 | `	SXUNUSED(pUnused);` |
|      89 |   67 | `	ph7_value_string(pVal,PHP_COMPAT_EXTRA_VERSION,(int)sizeof(PHP_COMPAT_EXTRA_VERSION)-1);` |
|      89 |   68 | `}` |
|      86 |   69 | `static void PH7_PHPVerIdConst(ph7_value *pVal,void *pUnused)` |
|       5 |   70 | `{` |
|      43 |   71 | `	SXUNUSED(pUnused);` |
|      91 |   72 | `	ph7_value_int64(pVal,PHP_COMPAT_VERSION_ID);` |
|      91 |   73 | `}` |
|       - |   74 | `#ifdef __WINNT__` |
|       - |   75 | `#include <Windows.h>` |
|       - |   76 | `#elif defined(__UNIXES__)` |
|       - |   77 | `#include <sys/utsname.h>` |
|       - |   78 | `#endif` |
|       - |   79 | `/*` |
|       - |   80 | ` * PHP_OS` |
|       - |   81 | ` *  Expand the name of the host Operating System.` |
|       - |   82 | ` */` |
|    7252 |   83 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|       5 |   84 | `{` |
|       - |   85 | `#if defined(__WINNT__)` |
|       5 |   86 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|       - |   87 | `#elif defined(__UNIXES__)` |
|       - |   88 | `	struct utsname sInfo;` |
|    7252 |   89 | `	if( uname(&sInfo) != 0 ){` |
|     ! 0 |   90 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|     ! 0 |   91 | `	}else{` |
|    7252 |   92 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|       - |   93 | `	}` |
|       - |   94 | `#else` |
|       - |   95 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|       - |   96 | `#endif` |
|    3621 |   97 | `	SXUNUSED(pUnused);` |
|    7257 |   98 | `}` |
|       - |   99 | `/*` |
|       - |  100 | ` * PHP_OS_FAMILY (php 7.2)` |
|       - |  101 | ` *  One of 'Windows', 'BSD', 'Darwin', 'Solaris', 'Linux' or 'Unknown', derived` |
|       - |  102 | ` *  from the host's uname sysname (php maps the same set at build time).` |
|       - |  103 | ` */` |
|     147 |  104 | `static void PH7_OS_FAMILY_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  105 | `{` |
|      73 |  106 | `	SXUNUSED(pUnused);` |
|       - |  107 | `#if defined(__WINNT__)` |
|       5 |  108 | `	ph7_value_string(pVal,"Windows",(int)sizeof("Windows")-1);` |
|       - |  109 | `#elif defined(__UNIXES__)` |
|       - |  110 | `	struct utsname sInfo;` |
|     147 |  111 | `	const char *zFamily = "Unknown";` |
|     147 |  112 | `	if( uname(&sInfo) == 0 ){` |
|     147 |  113 | `		const char *z = sInfo.sysname;` |
|     147 |  114 | `		if( SyStrnicmp(z,"Darwin",sizeof("Darwin")-1) == 0 ){` |
|      73 |  115 | `			zFamily = "Darwin";` |
|     147 |  116 | `		}else if( SyStrnicmp(z,"Linux",sizeof("Linux")-1) == 0 ){` |
|      74 |  117 | `			zFamily = "Linux";` |
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
|      73 |  131 | `	}` |
|     147 |  132 | `	ph7_value_string(pVal,zFamily,-1);` |
|       - |  133 | `#else` |
|       - |  134 | `	ph7_value_string(pVal,"Unknown",(int)sizeof("Unknown")-1);` |
|       - |  135 | `#endif` |
|     152 |  136 | `}` |
|       - |  137 | `/*` |
|       - |  138 | ` * PHP_SAPI` |
|       - |  139 | ` *  The interface between the interpreter and the host. PHL's host binary is a` |
|       - |  140 | ` *  command-line interpreter, so this is "cli" (matching the CLI default of` |
|       - |  141 | ` *  php_sapi_name(); the built-in -S server's per-request "cli-server" flavour is` |
|       - |  142 | ` *  only surfaced by php_sapi_name(), not this compile-time constant).` |
|       - |  143 | ` */` |
|      84 |  144 | `static void PH7_SAPI_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  145 | `{` |
|      42 |  146 | `	SXUNUSED(pUnused);` |
|      89 |  147 | `	ph7_value_string(pVal,"cli",(int)sizeof("cli")-1);` |
|      89 |  148 | `}` |
|       - |  149 | `/*` |
|       - |  150 | ` * PHP_EOL` |
|       - |  151 | ` *  Expand the correct 'End Of Line' symbol for this platform.` |
|       - |  152 | ` */` |
|     928 |  153 | `static void PH7_EOL_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  154 | `{` |
|     464 |  155 | `	SXUNUSED(pUnused);` |
|       - |  156 | `#ifdef __WINNT__` |
|       5 |  157 | `	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);` |
|       - |  158 | `#else` |
|     928 |  159 | `	ph7_value_string(pVal,"\n",(int)sizeof(char));` |
|       - |  160 | `#endif` |
|     933 |  161 | `}` |
|       - |  162 | `/*` |
|       - |  163 | ` * PHP_INT_MAX` |
|       - |  164 | ` * Expand the largest integer supported.` |
|       - |  165 | ` * Note that PH7 deals with 64-bit integer for all platforms.` |
|       - |  166 | ` */` |
|    1838 |  167 | `static void PH7_INTMAX_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  168 | `{` |
|     918 |  169 | `	SXUNUSED(pUnused);` |
|    1843 |  170 | `	ph7_value_int64(pVal,SXI64_HIGH);` |
|    1843 |  171 | `}` |
|       - |  172 | `/*` |
|       - |  173 | ` * ext/calendar: the four calendars, numbered in the order the conversion table` |
|       - |  174 | ` * holds them, and CAL_NUM_CALS as their count -- which is what makes the` |
|       - |  175 | ` * "valid calendar ID" screen a plain 0 <= id < CAL_NUM_CALS test.` |
|       - |  176 | ` */` |
|     128 |  177 | `static void PH7_CAL_GREGORIAN_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  178 | `{` |
|      64 |  179 | `	SXUNUSED(pUnused);` |
|     133 |  180 | `	ph7_value_int(pVal,0);` |
|     133 |  181 | `}` |
|      98 |  182 | `static void PH7_CAL_JULIAN_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  183 | `{` |
|      49 |  184 | `	SXUNUSED(pUnused);` |
|     103 |  185 | `	ph7_value_int(pVal,1);` |
|     103 |  186 | `}` |
|     216 |  187 | `static void PH7_CAL_JEWISH_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  188 | `{` |
|     108 |  189 | `	SXUNUSED(pUnused);` |
|     221 |  190 | `	ph7_value_int(pVal,2);` |
|     221 |  191 | `}` |
|     108 |  192 | `static void PH7_CAL_FRENCH_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  193 | `{` |
|      54 |  194 | `	SXUNUSED(pUnused);` |
|     113 |  195 | `	ph7_value_int(pVal,3);` |
|     113 |  196 | `}` |
|      82 |  197 | `static void PH7_CAL_NUM_CALS_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  198 | `{` |
|      41 |  199 | `	SXUNUSED(pUnused);` |
|      87 |  200 | `	ph7_value_int(pVal,4);` |
|      87 |  201 | `}` |
|       - |  202 | `/*` |
|       - |  203 | ` * easter_days()/easter_date()'s $mode. DEFAULT is not a rule but a` |
|       - |  204 | ` * date-dependent choice between the two below it; ROMAN moves the 1583-1752` |
|       - |  205 | ` * window to the Gregorian rule, and the two ALWAYS_ modes pin one rule for` |
|       - |  206 | ` * every year.` |
|       - |  207 | ` */` |
|     102 |  208 | `static void PH7_CAL_EASTER_DEFAULT_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  209 | `{` |
|      51 |  210 | `	SXUNUSED(pUnused);` |
|     107 |  211 | `	ph7_value_int(pVal,0);` |
|     107 |  212 | `}` |
|     102 |  213 | `static void PH7_CAL_EASTER_ROMAN_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  214 | `{` |
|      51 |  215 | `	SXUNUSED(pUnused);` |
|     107 |  216 | `	ph7_value_int(pVal,1);` |
|     107 |  217 | `}` |
|     102 |  218 | `static void PH7_CAL_EASTER_ALWAYS_GREGORIAN_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  219 | `{` |
|      51 |  220 | `	SXUNUSED(pUnused);` |
|     107 |  221 | `	ph7_value_int(pVal,2);` |
|     107 |  222 | `}` |
|     106 |  223 | `static void PH7_CAL_EASTER_ALWAYS_JULIAN_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  224 | `{` |
|      53 |  225 | `	SXUNUSED(pUnused);` |
|     111 |  226 | `	ph7_value_int(pVal,3);` |
|     111 |  227 | `}` |
|       - |  228 | `/* jddayofweek()'s three modes. Note that SHORT is 2 and LONG is 1: the numbers` |
|       - |  229 | ` * are not in the order the names suggest. */` |
|      82 |  230 | `static void PH7_CAL_DOW_DAYNO_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  231 | `{` |
|      41 |  232 | `	SXUNUSED(pUnused);` |
|      87 |  233 | `	ph7_value_int(pVal,0);` |
|      87 |  234 | `}` |
|     118 |  235 | `static void PH7_CAL_DOW_LONG_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  236 | `{` |
|      59 |  237 | `	SXUNUSED(pUnused);` |
|     123 |  238 | `	ph7_value_int(pVal,1);` |
|     123 |  239 | `}` |
|     108 |  240 | `static void PH7_CAL_DOW_SHORT_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  241 | `{` |
|      54 |  242 | `	SXUNUSED(pUnused);` |
|     113 |  243 | `	ph7_value_int(pVal,2);` |
|     113 |  244 | `}` |
|       - |  245 | `/* jdmonthname()'s six modes, which pick the CALENDAR as well as the spelling` |
|       - |  246 | ` * and are numbered independently of the CAL_* calendar ids above. */` |
|      84 |  247 | `static void PH7_CAL_MONTH_GREGORIAN_SHORT_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  248 | `{` |
|      42 |  249 | `	SXUNUSED(pUnused);` |
|      89 |  250 | `	ph7_value_int(pVal,0);` |
|      89 |  251 | `}` |
|     106 |  252 | `static void PH7_CAL_MONTH_GREGORIAN_LONG_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  253 | `{` |
|      53 |  254 | `	SXUNUSED(pUnused);` |
|     111 |  255 | `	ph7_value_int(pVal,1);` |
|     111 |  256 | `}` |
|      84 |  257 | `static void PH7_CAL_MONTH_JULIAN_SHORT_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  258 | `{` |
|      42 |  259 | `	SXUNUSED(pUnused);` |
|      89 |  260 | `	ph7_value_int(pVal,2);` |
|      89 |  261 | `}` |
|      84 |  262 | `static void PH7_CAL_MONTH_JULIAN_LONG_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  263 | `{` |
|      42 |  264 | `	SXUNUSED(pUnused);` |
|      89 |  265 | `	ph7_value_int(pVal,3);` |
|      89 |  266 | `}` |
|      90 |  267 | `static void PH7_CAL_MONTH_JEWISH_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  268 | `{` |
|      45 |  269 | `	SXUNUSED(pUnused);` |
|      95 |  270 | `	ph7_value_int(pVal,4);` |
|      95 |  271 | `}` |
|      84 |  272 | `static void PH7_CAL_MONTH_FRENCH_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  273 | `{` |
|      42 |  274 | `	SXUNUSED(pUnused);` |
|      89 |  275 | `	ph7_value_int(pVal,5);` |
|      89 |  276 | `}` |
|       - |  277 | `/*` |
|       - |  278 | ` * ext/calendar: the three flags jdtojewish()'s Hebrew spelling reads. They are` |
|       - |  279 | ` * a bit set, so a caller may ask for any combination of them.` |
|       - |  280 | ` */` |
|      86 |  281 | `static void PH7_CAL_JEWISH_ADD_ALAFIM_GERESH_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  282 | `{` |
|      43 |  283 | `	SXUNUSED(pUnused);` |
|      91 |  284 | `	ph7_value_int(pVal,2);` |
|      91 |  285 | `}` |
|      86 |  286 | `static void PH7_CAL_JEWISH_ADD_ALAFIM_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  287 | `{` |
|      43 |  288 | `	SXUNUSED(pUnused);` |
|      91 |  289 | `	ph7_value_int(pVal,4);` |
|      91 |  290 | `}` |
|      88 |  291 | `static void PH7_CAL_JEWISH_ADD_GERESHAYIM_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  292 | `{` |
|      44 |  293 | `	SXUNUSED(pUnused);` |
|      93 |  294 | `	ph7_value_int(pVal,8);` |
|      93 |  295 | `}` |
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
|      88 |  311 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_UNKNOWN_Const,   0)` |
|     102 |  312 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_GIF_Const,       1)` |
|      88 |  313 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JPEG_Const,      2)` |
|      88 |  314 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_PNG_Const,       3)` |
|      88 |  315 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_SWF_Const,       4)` |
|      88 |  316 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_PSD_Const,       5)` |
|      88 |  317 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_BMP_Const,       6)` |
|      88 |  318 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_TIFF_II_Const,   7)` |
|      88 |  319 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_TIFF_MM_Const,   8)` |
|     175 |  320 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JPC_Const,       9)` |
|      88 |  321 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JP2_Const,      10)` |
|      88 |  322 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JPX_Const,      11)` |
|      88 |  323 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_JB2_Const,      12)` |
|      94 |  324 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_SWC_Const,      13)` |
|      88 |  325 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_IFF_Const,      14)` |
|      88 |  326 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_WBMP_Const,     15)` |
|      88 |  327 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_XBM_Const,      16)` |
|      88 |  328 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_ICO_Const,      17)` |
|      88 |  329 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_WEBP_Const,     18)` |
|      88 |  330 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_AVIF_Const,     19)` |
|      88 |  331 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_HEIF_Const,     20)` |
|       - |  332 | `#ifdef PH7_ENABLE_LIBXML` |
|      90 |  333 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_SVG_Const,      21)` |
|      90 |  334 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_COUNT_Const,    22)` |
|       - |  335 | `#else` |
|       - |  336 | `PH7_IMAGETYPE_CONST(PH7_IMAGETYPE_COUNT_Const,    21)` |
|       - |  337 | `#endif` |
|       - |  338 | `/*` |
|       - |  339 | ` * PHP_INT_MIN (php 7.0)` |
|       - |  340 | ` * Expand the smallest integer supported.` |
|       - |  341 | ` */` |
|     246 |  342 | `static void PH7_INTMIN_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  343 | `{` |
|     123 |  344 | `	SXUNUSED(pUnused);` |
|     251 |  345 | `	ph7_value_int64(pVal,SMALLEST_INT64);` |
|     251 |  346 | `}` |
|       - |  347 | `/*` |
|       - |  348 | ` * PHP_INT_SIZE` |
|       - |  349 | ` * Expand the size in bytes of a 64-bit integer.` |
|       - |  350 | ` */` |
|     106 |  351 | `static void PH7_INTSIZE_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  352 | `{` |
|      53 |  353 | `	SXUNUSED(pUnused);` |
|     111 |  354 | `	ph7_value_int64(pVal,sizeof(sxi64));` |
|     111 |  355 | `}` |
|       - |  356 | `/*` |
|       - |  357 | ` * PHP_FLOAT_EPSILON / PHP_FLOAT_MAX / PHP_FLOAT_MIN / PHP_FLOAT_DIG (php 7.2)` |
|       - |  358 | ` * Double-precision characteristics, sourced from <float.h> exactly like php` |
|       - |  359 | ` * so they track the compiling platform's actual double representation.` |
|       - |  360 | ` */` |
|      88 |  361 | `static void PH7_FLOATEPSILON_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  362 | `{` |
|      44 |  363 | `	SXUNUSED(pUnused);` |
|      93 |  364 | `	ph7_value_double(pVal,DBL_EPSILON);` |
|      93 |  365 | `}` |
|      88 |  366 | `static void PH7_FLOATMAX_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  367 | `{` |
|      44 |  368 | `	SXUNUSED(pUnused);` |
|      93 |  369 | `	ph7_value_double(pVal,DBL_MAX);` |
|      93 |  370 | `}` |
|      84 |  371 | `static void PH7_FLOATMIN_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  372 | `{` |
|      42 |  373 | `	SXUNUSED(pUnused);` |
|      89 |  374 | `	ph7_value_double(pVal,DBL_MIN);` |
|      89 |  375 | `}` |
|      86 |  376 | `static void PH7_FLOATDIG_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  377 | `{` |
|      43 |  378 | `	SXUNUSED(pUnused);` |
|      91 |  379 | `	ph7_value_int64(pVal,DBL_DIG);` |
|      91 |  380 | `}` |
|       - |  381 | `/*` |
|       - |  382 | ` * DIRECTORY_SEPARATOR.` |
|       - |  383 | ` * Expand the directory separator character.` |
|       - |  384 | ` */` |
|    2907 |  385 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  386 | `{` |
|    1451 |  387 | `	SXUNUSED(pUnused);` |
|       - |  388 | `#ifdef __WINNT__` |
|       5 |  389 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|       - |  390 | `#else` |
|    2907 |  391 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|       - |  392 | `#endif` |
|    2912 |  393 | `}` |
|       - |  394 | `/*` |
|       - |  395 | ` * PATH_SEPARATOR.` |
|       - |  396 | ` * Expand the path separator character.` |
|       - |  397 | ` */` |
|      89 |  398 | `static void PH7_PATHSEP_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  399 | `{` |
|      44 |  400 | `	SXUNUSED(pUnused);` |
|       - |  401 | `#ifdef __WINNT__` |
|       5 |  402 | `	ph7_value_string(pVal,";",(int)sizeof(char));` |
|       - |  403 | `#else` |
|      89 |  404 | `	ph7_value_string(pVal,":",(int)sizeof(char));` |
|       - |  405 | `#endif` |
|      94 |  406 | `}` |
|       - |  407 |  |
|       - |  408 | `#if defined(PH7_ENABLE_MATH_FUNC)` |
|       - |  409 | `/*` |
|       - |  410 | ` * NAN constant: floating-point Not-A-Number` |
|       - |  411 | ` */` |
|     245 |  412 | `static void PH7_NAN_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  413 | `{` |
|     122 |  414 | `	SXUNUSED(pUnused);` |
|     250 |  415 | `	ph7_value_double(pVal, PH7_NAN_VALUE());` |
|     250 |  416 | `}` |
|       - |  417 |  |
|       - |  418 | `/*` |
|       - |  419 | ` * INF constant: positive infinity` |
|       - |  420 | ` */` |
|     271 |  421 | `static void PH7_INF_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  422 | `{` |
|     135 |  423 | `	SXUNUSED(pUnused);` |
|       - |  424 | `	/* similarly avoid the INFINITY macro */` |
|     276 |  425 | `	ph7_value_double(pVal, PH7_INF_VALUE());` |
|     276 |  426 | `}` |
|       - |  427 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|       - |  428 |  |
|       - |  429 | `#ifndef __WINNT__` |
|       - |  430 | `#include <time.h>` |
|       - |  431 | `#endif` |
|       - |  432 | `/*` |
|       - |  433 | ` * __TIME__` |
|       - |  434 | ` *  Expand the current time (GMT).` |
|       - |  435 | ` */` |
|      84 |  436 | `static void PH7_TIME_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  437 | `{` |
|       - |  438 | `	Sytm sTm;` |
|       - |  439 | `#ifdef __WINNT__` |
|       - |  440 | `	SYSTEMTIME sOS;` |
|       5 |  441 | `	GetSystemTime(&sOS);` |
|       5 |  442 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|       - |  443 | `#else` |
|       - |  444 | `	struct tm *pTm;` |
|       - |  445 | `	time_t t;` |
|      84 |  446 | `	time(&t);` |
|      84 |  447 | `	pTm = gmtime(&t);` |
|      84 |  448 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|       - |  449 | `#endif` |
|      42 |  450 | `	SXUNUSED(pUnused); /* cc warning */` |
|       - |  451 | `	/* Expand */` |
|      89 |  452 | `	ph7_value_string_format(pVal,"%02d:%02d:%02d",sTm.tm_hour,sTm.tm_min,sTm.tm_sec);` |
|      89 |  453 | `}` |
|       - |  454 | `/*` |
|       - |  455 | ` * __DATE__` |
|       - |  456 | ` *  Expand the current date in the ISO-8601 format.` |
|       - |  457 | ` */` |
|      84 |  458 | `static void PH7_DATE_Const(ph7_value *pVal,void *pUnused)` |
|       5 |  459 | `{` |
|       - |  460 | `	Sytm sTm;` |
|       - |  461 | `#ifdef __WINNT__` |
|       - |  462 | `	SYSTEMTIME sOS;` |
|       5 |  463 | `	GetSystemTime(&sOS);` |
|       5 |  464 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|       - |  465 | `#else` |
|       - |  466 | `	struct tm *pTm;` |
|       - |  467 | `	time_t t;` |
|      84 |  468 | `	time(&t);` |
|      84 |  469 | `	pTm = gmtime(&t);` |
|      84 |  470 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|       - |  471 | `#endif` |
|      42 |  472 | `	SXUNUSED(pUnused); /* cc warning */` |
|       - |  473 | `	/* Expand */` |
|      89 |  474 | `	ph7_value_string_format(pVal,"%04qd-%02d-%02d",sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|      89 |  475 | `}` |
|       - |  476 | `/*` |
|       - |  477 | ` * __FILE__` |
|       - |  478 | ` *  Path of the processed script.` |
|       - |  479 | ` */` |
|      82 |  480 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  481 | `{` |
|      87 |  482 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - |  483 | `	SyString *pFile;` |
|       - |  484 | `	/* The unit the LITERAL is written in, which php fixes at compile time: the` |
|       - |  485 | `	 * declared file of the function running, else the unit on top of the include` |
|       - |  486 | `	 * stack. Reading the stack top alone answered the unit currently being LOADED,` |
|       - |  487 | `	 * so a function defined in one file and called from an include (or from an` |
|       - |  488 | `	 * eval()'d chunk, whose own name is now such an entry) reported the caller's` |
|       - |  489 | `	 * file as its own. */` |
|      87 |  490 | `	pFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|      87 |  491 | `	if( pFile == 0 ){` |
|       - |  492 | `		/* Expand the magic word: ":MEMORY:" */` |
|     ! 0 |  493 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|     ! 0 |  494 | `	}else{` |
|      87 |  495 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|       - |  496 | `	}` |
|      87 |  497 | `}` |
|       - |  498 | `/*` |
|       - |  499 | ` * __DIR__` |
|       - |  500 | ` *  Directory holding the processed script.` |
|       - |  501 | ` */` |
|      82 |  502 | `static void PH7_DIR_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  503 | `{` |
|      87 |  504 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - |  505 | `	SyString *pFile;` |
|       - |  506 | `	/* Same question as __FILE__, one directory up. */` |
|      87 |  507 | `	pFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|      87 |  508 | `	if( pFile == 0 ){` |
|       - |  509 | `		/* Expand the magic word: ":MEMORY:" */` |
|     ! 0 |  510 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|     ! 0 |  511 | `	}else{` |
|      87 |  512 | `		if( pFile->nByte > 0 ){` |
|       - |  513 | `			const char *zDir;` |
|       - |  514 | `			int nLen;` |
|      87 |  515 | `			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|      87 |  516 | `			ph7_value_string(pVal,zDir,nLen);` |
|      46 |  517 | `		}else{` |
|       - |  518 | `			/* Expand '.' as the current directory*/` |
|     ! 0 |  519 | `			ph7_value_string(pVal,".",(int)sizeof(char));` |
|       - |  520 | `		}` |
|       - |  521 | `	}` |
|      87 |  522 | `}` |
|       - |  523 | `/*` |
|       - |  524 | ` * PHP_SHLIB_SUFFIX` |
|       - |  525 | ` *  Expand shared library suffix.` |
|       - |  526 | ` */` |
|      84 |  527 | `static void PH7_PHP_SHLIB_SUFFIX_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  528 | `{` |
|       - |  529 | `#ifdef __WINNT__` |
|       5 |  530 | `	ph7_value_string(pVal,"dll",(int)sizeof("dll")-1);` |
|       - |  531 | `#else` |
|      84 |  532 | `	ph7_value_string(pVal,"so",(int)sizeof("so")-1);` |
|       - |  533 | `#endif` |
|      42 |  534 | `	SXUNUSED(pUserData); /* cc warning */` |
|      89 |  535 | `}` |
|       - |  536 | `/*` |
|       - |  537 | ` * E_ERROR` |
|       - |  538 | ` *  Expands 1` |
|       - |  539 | ` */` |
|      90 |  540 | `static void PH7_E_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  541 | `{` |
|      95 |  542 | `	ph7_value_int(pVal,1);` |
|      45 |  543 | `	SXUNUSED(pUserData);` |
|      95 |  544 | `}` |
|       - |  545 | `/*` |
|       - |  546 | ` * E_WARNING` |
|       - |  547 | ` *  Expands 2` |
|       - |  548 | ` */` |
|     108 |  549 | `static void PH7_E_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  550 | `{` |
|     113 |  551 | `	ph7_value_int(pVal,2);` |
|      54 |  552 | `	SXUNUSED(pUserData);` |
|     113 |  553 | `}` |
|       - |  554 | `/*` |
|       - |  555 | ` * E_PARSE` |
|       - |  556 | ` *  Expands 4` |
|       - |  557 | ` */` |
|      84 |  558 | `static void PH7_E_PARSE_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  559 | `{` |
|      89 |  560 | `	ph7_value_int(pVal,4);` |
|      42 |  561 | `	SXUNUSED(pUserData);` |
|      89 |  562 | `}` |
|       - |  563 | `/*` |
|       - |  564 | ` * E_NOTICE` |
|       - |  565 | ` * Expands 8` |
|       - |  566 | ` */` |
|      98 |  567 | `static void PH7_E_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  568 | `{` |
|     103 |  569 | `	ph7_value_int(pVal,8);` |
|      49 |  570 | `	SXUNUSED(pUserData);` |
|     103 |  571 | `}` |
|       - |  572 | `/*` |
|       - |  573 | ` * E_CORE_ERROR` |
|       - |  574 | ` * Expands 16` |
|       - |  575 | ` */` |
|      84 |  576 | `static void PH7_E_CORE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  577 | `{` |
|      89 |  578 | `	ph7_value_int(pVal,16);` |
|      42 |  579 | `	SXUNUSED(pUserData);` |
|      89 |  580 | `}` |
|       - |  581 | `/*` |
|       - |  582 | ` * E_CORE_WARNING` |
|       - |  583 | ` * Expands 32` |
|       - |  584 | ` */` |
|      84 |  585 | `static void PH7_E_CORE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  586 | `{` |
|      89 |  587 | `	ph7_value_int(pVal,32);` |
|      42 |  588 | `	SXUNUSED(pUserData);` |
|      89 |  589 | `}` |
|       - |  590 | `/*` |
|       - |  591 | ` * E_COMPILE_ERROR` |
|       - |  592 | ` * Expands 64` |
|       - |  593 | ` */` |
|      84 |  594 | `static void PH7_E_COMPILE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  595 | `{` |
|      89 |  596 | `	ph7_value_int(pVal,64);` |
|      42 |  597 | `	SXUNUSED(pUserData);` |
|      89 |  598 | `}` |
|       - |  599 | `/*` |
|       - |  600 | ` * E_COMPILE_WARNING` |
|       - |  601 | ` * Expands 128` |
|       - |  602 | ` */` |
|      92 |  603 | `static void PH7_E_COMPILE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  604 | `{` |
|      97 |  605 | `	ph7_value_int(pVal,128);` |
|      46 |  606 | `	SXUNUSED(pUserData);` |
|      97 |  607 | `}` |
|       - |  608 | `/*` |
|       - |  609 | ` * E_USER_ERROR` |
|       - |  610 | ` * Expands 256` |
|       - |  611 | ` */` |
|      94 |  612 | `static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  613 | `{` |
|      99 |  614 | `	ph7_value_int(pVal,256);` |
|      47 |  615 | `	SXUNUSED(pUserData);` |
|      99 |  616 | `}` |
|       - |  617 | `/*` |
|       - |  618 | ` * E_USER_WARNING` |
|       - |  619 | ` * Expands 512` |
|       - |  620 | ` */` |
|     156 |  621 | `static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  622 | `{` |
|     161 |  623 | `	ph7_value_int(pVal,512);` |
|      78 |  624 | `	SXUNUSED(pUserData);` |
|     161 |  625 | `}` |
|       - |  626 | `/*` |
|       - |  627 | ` * E_USER_NOTICE` |
|       - |  628 | ` * Expands 1024` |
|       - |  629 | ` */` |
|     180 |  630 | `static void PH7_E_USER_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  631 | `{` |
|     185 |  632 | `	ph7_value_int(pVal,1024);` |
|      90 |  633 | `	SXUNUSED(pUserData);` |
|     185 |  634 | `}` |
|       - |  635 | `/*` |
|       - |  636 | ` * E_RECOVERABLE_ERROR` |
|       - |  637 | ` * Expands 4096` |
|       - |  638 | ` */` |
|      84 |  639 | `static void PH7_E_RECOVERABLE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  640 | `{` |
|      89 |  641 | `	ph7_value_int(pVal,4096);` |
|      42 |  642 | `	SXUNUSED(pUserData);` |
|      89 |  643 | `}` |
|       - |  644 | `/*` |
|       - |  645 | ` * E_STRICT` |
|       - |  646 | ` * Expands 2048. php 8.4 removed the error LEVEL but kept the constant, marked` |
|       - |  647 | ` * deprecated -- a program may still name it (and still gets 2048), it just says` |
|       - |  648 | ` * so. It is deliberately NOT part of E_ALL, which is why PH7_E_ALL_MASK is` |
|       - |  649 | ` * 30719 and not 32767.` |
|       - |  650 | ` */` |
|      94 |  651 | `static void PH7_E_STRICT_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  652 | `{` |
|      99 |  653 | `	ph7_value_int(pVal,2048);` |
|      47 |  654 | `	SXUNUSED(pUserData);` |
|      99 |  655 | `}` |
|       - |  656 | `/*` |
|       - |  657 | ` * E_DEPRECATED` |
|       - |  658 | ` * Expands 8192` |
|       - |  659 | ` */` |
|     392 |  660 | `static void PH7_E_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  661 | `{` |
|     397 |  662 | `	ph7_value_int(pVal,8192);` |
|     196 |  663 | `	SXUNUSED(pUserData);` |
|     397 |  664 | `}` |
|       - |  665 | `/*` |
|       - |  666 | ` * E_USER_DEPRECATED` |
|       - |  667 | ` *   Expands 16384.` |
|       - |  668 | ` */` |
|      98 |  669 | `static void PH7_E_USER_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  670 | `{` |
|     103 |  671 | `	ph7_value_int(pVal,16384);` |
|      49 |  672 | `	SXUNUSED(pUserData);` |
|     103 |  673 | `}` |
|       - |  674 | `/*` |
|       - |  675 | ` * E_ALL` |
|       - |  676 | ` *  Expands 30719 (php 8: E_STRICT is no longer part of E_ALL)` |
|       - |  677 | ` */` |
|     251 |  678 | `static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  679 | `{` |
|     256 |  680 | `	ph7_value_int(pVal,PH7_E_ALL_MASK);` |
|     125 |  681 | `	SXUNUSED(pUserData);` |
|     256 |  682 | `}` |
|       - |  683 | `/*` |
|       - |  684 | ` * CASE_LOWER` |
|       - |  685 | ` *  Expands 0.` |
|       - |  686 | ` */` |
|      85 |  687 | `static void PH7_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  688 | `{` |
|      90 |  689 | `	ph7_value_int(pVal,0);` |
|      42 |  690 | `	SXUNUSED(pUserData);` |
|      90 |  691 | `}` |
|       - |  692 | `/*` |
|       - |  693 | ` * CASE_UPPER` |
|       - |  694 | ` *  Expands 1.` |
|       - |  695 | ` */` |
|      91 |  696 | `static void PH7_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  697 | `{` |
|      96 |  698 | `	ph7_value_int(pVal,1);` |
|      45 |  699 | `	SXUNUSED(pUserData);` |
|      96 |  700 | `}` |
|       - |  701 | `/*` |
|       - |  702 | ` * STR_PAD_LEFT` |
|       - |  703 | ` *  Expands 0.` |
|       - |  704 | ` */` |
|     117 |  705 | `static void PH7_STR_PAD_LEFT_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  706 | `{` |
|     122 |  707 | `	ph7_value_int(pVal,0);` |
|      58 |  708 | `	SXUNUSED(pUserData);` |
|     122 |  709 | `}` |
|       - |  710 | `/*` |
|       - |  711 | ` * STR_PAD_RIGHT` |
|       - |  712 | ` *  Expands 1.` |
|       - |  713 | ` */` |
|      97 |  714 | `static void PH7_STR_PAD_RIGHT_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  715 | `{` |
|     102 |  716 | `	ph7_value_int(pVal,1);` |
|      48 |  717 | `	SXUNUSED(pUserData);` |
|     102 |  718 | `}` |
|       - |  719 | `/*` |
|       - |  720 | ` * STR_PAD_BOTH` |
|       - |  721 | ` *  Expands 2.` |
|       - |  722 | ` */` |
|      87 |  723 | `static void PH7_STR_PAD_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  724 | `{` |
|      92 |  725 | `	ph7_value_int(pVal,2);` |
|      43 |  726 | `	SXUNUSED(pUserData);` |
|      92 |  727 | `}` |
|       - |  728 | `/*` |
|       - |  729 | ` * stream_wrapper_register()'s $flags. php defines exactly this one bit: the` |
|       - |  730 | ` * wrapper speaks to the network, so allow_url_fopen gates opening it and` |
|       - |  731 | ` * allow_url_include gates INCLUDING it.` |
|       - |  732 | ` */` |
|      87 |  733 | `static void PH7_STREAM_IS_URL_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  734 | `{` |
|      92 |  735 | `	ph7_value_int(pVal,PH7_STREAM_IS_URL);` |
|      43 |  736 | `	SXUNUSED(pUserData);` |
|      92 |  737 | `}` |
|       - |  738 | `/*` |
|       - |  739 | ` * The rest of the streamWrapper PROTOCOL vocabulary: the numbers php hands a` |
|       - |  740 | ` * userland wrapper, and the ones a wrapper hands back. PHL registered wrappers` |
|       - |  741 | ` * without them, so the ordinary spellings every real wrapper is written against --` |
|       - |  742 | ``  * `$options & STREAM_USE_PATH` in stream_open(), `$flags & STREAM_URL_STAT_QUIET` `` |
|       - |  743 | ` * in url_stat(), the STREAM_META_* verb in stream_metadata() -- were an undefined` |
|       - |  744 | ` * constant, i.e. an Error, in code php runs.` |
|       - |  745 | ` */` |
|      80 |  746 | `static void PH7_STREAM_USE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  747 | `{` |
|      85 |  748 | `	ph7_value_int(pVal,PH7_STREAM_USE_PATH);` |
|      40 |  749 | `	SXUNUSED(pUserData);` |
|      85 |  750 | `}` |
|      80 |  751 | `static void PH7_STREAM_IGNORE_URL_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  752 | `{` |
|      85 |  753 | `	ph7_value_int(pVal,PH7_STREAM_IGNORE_URL);` |
|      40 |  754 | `	SXUNUSED(pUserData);` |
|      85 |  755 | `}` |
|      80 |  756 | `static void PH7_STREAM_REPORT_ERRORS_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  757 | `{` |
|      85 |  758 | `	ph7_value_int(pVal,PH7_STREAM_REPORT_ERRORS);` |
|      40 |  759 | `	SXUNUSED(pUserData);` |
|      85 |  760 | `}` |
|      80 |  761 | `static void PH7_STREAM_MUST_SEEK_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  762 | `{` |
|      85 |  763 | `	ph7_value_int(pVal,PH7_STREAM_MUST_SEEK);` |
|      40 |  764 | `	SXUNUSED(pUserData);` |
|      85 |  765 | `}` |
|      80 |  766 | `static void PH7_STREAM_URL_STAT_LINK_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  767 | `{` |
|      85 |  768 | `	ph7_value_int(pVal,PH7_URL_STAT_LINK);` |
|      40 |  769 | `	SXUNUSED(pUserData);` |
|      85 |  770 | `}` |
|      80 |  771 | `static void PH7_STREAM_URL_STAT_QUIET_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  772 | `{` |
|      85 |  773 | `	ph7_value_int(pVal,PH7_URL_STAT_QUIET);` |
|      40 |  774 | `	SXUNUSED(pUserData);` |
|      85 |  775 | `}` |
|      80 |  776 | `static void PH7_STREAM_MKDIR_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  777 | `{` |
|      85 |  778 | `	ph7_value_int(pVal,PH7_STREAM_MKDIR_RECURSIVE);` |
|      40 |  779 | `	SXUNUSED(pUserData);` |
|      85 |  780 | `}` |
|      80 |  781 | `static void PH7_STREAM_META_TOUCH_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  782 | `{` |
|      85 |  783 | `	ph7_value_int(pVal,PH7_STREAM_META_TOUCH);` |
|      40 |  784 | `	SXUNUSED(pUserData);` |
|      85 |  785 | `}` |
|      80 |  786 | `static void PH7_STREAM_META_OWNER_NAME_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  787 | `{` |
|      85 |  788 | `	ph7_value_int(pVal,PH7_STREAM_META_OWNER_NAME);` |
|      40 |  789 | `	SXUNUSED(pUserData);` |
|      85 |  790 | `}` |
|      80 |  791 | `static void PH7_STREAM_META_OWNER_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  792 | `{` |
|      85 |  793 | `	ph7_value_int(pVal,PH7_STREAM_META_OWNER);` |
|      40 |  794 | `	SXUNUSED(pUserData);` |
|      85 |  795 | `}` |
|      80 |  796 | `static void PH7_STREAM_META_GROUP_NAME_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  797 | `{` |
|      85 |  798 | `	ph7_value_int(pVal,PH7_STREAM_META_GROUP_NAME);` |
|      40 |  799 | `	SXUNUSED(pUserData);` |
|      85 |  800 | `}` |
|      80 |  801 | `static void PH7_STREAM_META_GROUP_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  802 | `{` |
|      85 |  803 | `	ph7_value_int(pVal,PH7_STREAM_META_GROUP);` |
|      40 |  804 | `	SXUNUSED(pUserData);` |
|      85 |  805 | `}` |
|      80 |  806 | `static void PH7_STREAM_META_ACCESS_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  807 | `{` |
|      85 |  808 | `	ph7_value_int(pVal,PH7_STREAM_META_ACCESS);` |
|      40 |  809 | `	SXUNUSED(pUserData);` |
|      85 |  810 | `}` |
|      80 |  811 | `static void PH7_STREAM_OPTION_BLOCKING_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  812 | `{` |
|      85 |  813 | `	ph7_value_int(pVal,PH7_STREAM_OPTION_BLOCKING);` |
|      40 |  814 | `	SXUNUSED(pUserData);` |
|      85 |  815 | `}` |
|      80 |  816 | `static void PH7_STREAM_OPTION_READ_BUFFER_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  817 | `{` |
|      85 |  818 | `	ph7_value_int(pVal,PH7_STREAM_OPTION_READ_BUFFER);` |
|      40 |  819 | `	SXUNUSED(pUserData);` |
|      85 |  820 | `}` |
|      80 |  821 | `static void PH7_STREAM_OPTION_WRITE_BUFFER_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  822 | `{` |
|      85 |  823 | `	ph7_value_int(pVal,PH7_STREAM_OPTION_WRITE_BUFFER);` |
|      40 |  824 | `	SXUNUSED(pUserData);` |
|      85 |  825 | `}` |
|      80 |  826 | `static void PH7_STREAM_OPTION_READ_TIMEOUT_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  827 | `{` |
|      85 |  828 | `	ph7_value_int(pVal,PH7_STREAM_OPTION_READ_TIMEOUT);` |
|      40 |  829 | `	SXUNUSED(pUserData);` |
|      85 |  830 | `}` |
|      80 |  831 | `static void PH7_STREAM_BUFFER_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  832 | `{` |
|      85 |  833 | `	ph7_value_int(pVal,PH7_STREAM_BUFFER_NONE);` |
|      40 |  834 | `	SXUNUSED(pUserData);` |
|      85 |  835 | `}` |
|      80 |  836 | `static void PH7_STREAM_BUFFER_LINE_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  837 | `{` |
|      85 |  838 | `	ph7_value_int(pVal,PH7_STREAM_BUFFER_LINE);` |
|      40 |  839 | `	SXUNUSED(pUserData);` |
|      85 |  840 | `}` |
|      80 |  841 | `static void PH7_STREAM_BUFFER_FULL_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  842 | `{` |
|      85 |  843 | `	ph7_value_int(pVal,PH7_STREAM_BUFFER_FULL);` |
|      40 |  844 | `	SXUNUSED(pUserData);` |
|      85 |  845 | `}` |
|      80 |  846 | `static void PH7_STREAM_CAST_AS_STREAM_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  847 | `{` |
|      85 |  848 | `	ph7_value_int(pVal,PH7_STREAM_CAST_AS_STREAM);` |
|      40 |  849 | `	SXUNUSED(pUserData);` |
|      85 |  850 | `}` |
|      80 |  851 | `static void PH7_STREAM_CAST_FOR_SELECT_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  852 | `{` |
|      85 |  853 | `	ph7_value_int(pVal,PH7_STREAM_CAST_FOR_SELECT);` |
|      40 |  854 | `	SXUNUSED(pUserData);` |
|      85 |  855 | `}` |
|       - |  856 | `/*` |
|       - |  857 | ` * A userland filter's ANSWER, and which kind of call it is answering. FEED_ME` |
|       - |  858 | ` * says "I produced nothing, ask me again with more"; ERR_FATAL ends the stream.` |
|       - |  859 | ` */` |
|     119 |  860 | `static void PH7_PSFS_PASS_ON_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  861 | `{` |
|     124 |  862 | `	ph7_value_int(pVal,PHL_PSFS_PASS_ON);` |
|      59 |  863 | `	SXUNUSED(pUserData);` |
|     124 |  864 | `}` |
|      87 |  865 | `static void PH7_PSFS_FEED_ME_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  866 | `{` |
|      92 |  867 | `	ph7_value_int(pVal,PHL_PSFS_FEED_ME);` |
|      43 |  868 | `	SXUNUSED(pUserData);` |
|      92 |  869 | `}` |
|      85 |  870 | `static void PH7_PSFS_ERR_FATAL_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  871 | `{` |
|      90 |  872 | `	ph7_value_int(pVal,PHL_PSFS_ERR_FATAL);` |
|      42 |  873 | `	SXUNUSED(pUserData);` |
|      90 |  874 | `}` |
|      83 |  875 | `static void PH7_PSFS_FLAG_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  876 | `{` |
|      88 |  877 | `	ph7_value_int(pVal,PHL_PSFS_FLAG_NORMAL);` |
|      41 |  878 | `	SXUNUSED(pUserData);` |
|      88 |  879 | `}` |
|      83 |  880 | `static void PH7_PSFS_FLAG_FLUSH_INC_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  881 | `{` |
|      88 |  882 | `	ph7_value_int(pVal,PHL_PSFS_FLAG_FLUSH_INC);` |
|      41 |  883 | `	SXUNUSED(pUserData);` |
|      88 |  884 | `}` |
|      83 |  885 | `static void PH7_PSFS_FLAG_FLUSH_CLOSE_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  886 | `{` |
|      88 |  887 | `	ph7_value_int(pVal,PHL_PSFS_FLAG_FLUSH_CLOSE);` |
|      41 |  888 | `	SXUNUSED(pUserData);` |
|      88 |  889 | `}` |
|       - |  890 | `/*` |
|       - |  891 | `` * The `notification` callback's first argument: WHICH event the stream layer is`` |
|       - |  892 | ` * reporting. php defines all ten whatever its build registered, so a script may` |
|       - |  893 | `` * name one no wrapper here raises -- an unmatched `case` is silent where a`` |
|       - |  894 | ` * missing constant is a fatal.` |
|       - |  895 | ` */` |
|      80 |  896 | `static void PH7_STREAM_NOTIFY_RESOLVE_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  897 | `{` |
|      85 |  898 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_RESOLVE);` |
|      40 |  899 | `	SXUNUSED(pUserData);` |
|      85 |  900 | `}` |
|     176 |  901 | `static void PH7_STREAM_NOTIFY_CONNECT_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  902 | `{` |
|     181 |  903 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_CONNECT);` |
|      88 |  904 | `	SXUNUSED(pUserData);` |
|     181 |  905 | `}` |
|      80 |  906 | `static void PH7_STREAM_NOTIFY_AUTH_REQUIRED_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  907 | `{` |
|      85 |  908 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_AUTH_REQUIRED);` |
|      40 |  909 | `	SXUNUSED(pUserData);` |
|      85 |  910 | `}` |
|      80 |  911 | `static void PH7_STREAM_NOTIFY_MIME_TYPE_IS_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  912 | `{` |
|      85 |  913 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_MIME_TYPE_IS);` |
|      40 |  914 | `	SXUNUSED(pUserData);` |
|      85 |  915 | `}` |
|      80 |  916 | `static void PH7_STREAM_NOTIFY_FILE_SIZE_IS_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  917 | `{` |
|      85 |  918 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_FILE_SIZE_IS);` |
|      40 |  919 | `	SXUNUSED(pUserData);` |
|      85 |  920 | `}` |
|      80 |  921 | `static void PH7_STREAM_NOTIFY_REDIRECTED_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  922 | `{` |
|      85 |  923 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_REDIRECTED);` |
|      40 |  924 | `	SXUNUSED(pUserData);` |
|      85 |  925 | `}` |
|      80 |  926 | `static void PH7_STREAM_NOTIFY_PROGRESS_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  927 | `{` |
|      85 |  928 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_PROGRESS);` |
|      40 |  929 | `	SXUNUSED(pUserData);` |
|      85 |  930 | `}` |
|      80 |  931 | `static void PH7_STREAM_NOTIFY_COMPLETED_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  932 | `{` |
|      85 |  933 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_COMPLETED);` |
|      40 |  934 | `	SXUNUSED(pUserData);` |
|      85 |  935 | `}` |
|      80 |  936 | `static void PH7_STREAM_NOTIFY_FAILURE_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  937 | `{` |
|      85 |  938 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_FAILURE);` |
|      40 |  939 | `	SXUNUSED(pUserData);` |
|      85 |  940 | `}` |
|      80 |  941 | `static void PH7_STREAM_NOTIFY_AUTH_RESULT_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  942 | `{` |
|      85 |  943 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_AUTH_RESULT);` |
|      40 |  944 | `	SXUNUSED(pUserData);` |
|      85 |  945 | `}` |
|       - |  946 | `/* And the callback's second argument: how bad the event is. */` |
|      80 |  947 | `static void PH7_STREAM_NOTIFY_SEVERITY_INFO_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  948 | `{` |
|      85 |  949 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_SEVERITY_INFO);` |
|      40 |  950 | `	SXUNUSED(pUserData);` |
|      85 |  951 | `}` |
|      80 |  952 | `static void PH7_STREAM_NOTIFY_SEVERITY_WARN_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  953 | `{` |
|      85 |  954 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_SEVERITY_WARN);` |
|      40 |  955 | `	SXUNUSED(pUserData);` |
|      85 |  956 | `}` |
|      80 |  957 | `static void PH7_STREAM_NOTIFY_SEVERITY_ERR_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  958 | `{` |
|      85 |  959 | `	ph7_value_int(pVal,PHL_STREAM_NOTIFY_SEVERITY_ERR);` |
|      40 |  960 | `	SXUNUSED(pUserData);` |
|      85 |  961 | `}` |
|       - |  962 | `/*` |
|       - |  963 | ` * stream_filter_append()'s $mode — WHICH chain the filter joins. php's 0 is not` |
|       - |  964 | ` * "neither": it means "whichever chains the handle's own mode makes sense for".` |
|       - |  965 | ` */` |
|     215 |  966 | `static void PH7_STREAM_FILTER_READ_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  967 | `{` |
|     220 |  968 | `	ph7_value_int(pVal,PHL_STREAM_FILTER_READ);` |
|     107 |  969 | `	SXUNUSED(pUserData);` |
|     220 |  970 | `}` |
|   10129 |  971 | `static void PH7_STREAM_FILTER_WRITE_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  972 | `{` |
|   10134 |  973 | `	ph7_value_int(pVal,PHL_STREAM_FILTER_WRITE);` |
|    5064 |  974 | `	SXUNUSED(pUserData);` |
|   10134 |  975 | `}` |
|      83 |  976 | `static void PH7_STREAM_FILTER_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  977 | `{` |
|      88 |  978 | `	ph7_value_int(pVal,PHL_STREAM_FILTER_ALL);` |
|      41 |  979 | `	SXUNUSED(pUserData);` |
|      88 |  980 | `}` |
|       - |  981 | `/*` |
|       - |  982 | ` * stream_socket_client()'s $flags. CONNECT is the default it documents;` |
|       - |  983 | ` * PERSISTENT is what pfsockopen() means and the only one that changes what a` |
|       - |  984 | ` * second call to the same address ANSWERS.` |
|       - |  985 | ` */` |
|     169 |  986 | `static void PH7_STREAM_CLIENT_CONNECT_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  987 | `{` |
|     174 |  988 | `	ph7_value_int(pVal,PH7_STREAM_CLIENT_CONNECT);` |
|      84 |  989 | `	SXUNUSED(pUserData);` |
|     174 |  990 | `}` |
|      87 |  991 | `static void PH7_STREAM_CLIENT_ASYNC_CONNECT_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  992 | `{` |
|      92 |  993 | `	ph7_value_int(pVal,PH7_STREAM_CLIENT_ASYNC_CONNECT);` |
|      43 |  994 | `	SXUNUSED(pUserData);` |
|      92 |  995 | `}` |
|      95 |  996 | `static void PH7_STREAM_CLIENT_PERSISTENT_Const(ph7_value *pVal,void *pUserData)` |
|       5 |  997 | `{` |
|     100 |  998 | `	ph7_value_int(pVal,PH7_STREAM_CLIENT_PERSISTENT);` |
|      47 |  999 | `	SXUNUSED(pUserData);` |
|     100 | 1000 | `}` |
|       - | 1001 | `/*` |
|       - | 1002 | ` * The socket-family constants. Their VALUES are the platform's own — AF_INET6 is` |
|       - | 1003 | ` * 10 on Linux, 23 on Windows and 30 on the BSDs — so they are asked for by id` |
|       - | 1004 | ` * rather than written down here, and a program handing one to` |
|       - | 1005 | ` * stream_socket_pair() is handing the OS its own number.` |
|       - | 1006 | ` */` |
|       - | 1007 | `#ifdef PH7_ENABLE_NET` |
|      81 | 1008 | `static void PH7_STREAM_PF_INET_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1009 | `{` |
|      86 | 1010 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_INET));` |
|      40 | 1011 | `	SXUNUSED(pUserData);` |
|      86 | 1012 | `}` |
|      81 | 1013 | `static void PH7_STREAM_PF_INET6_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1014 | `{` |
|      86 | 1015 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_INET6));` |
|      40 | 1016 | `	SXUNUSED(pUserData);` |
|      86 | 1017 | `}` |
|      87 | 1018 | `static void PH7_STREAM_PF_UNIX_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1019 | `{` |
|      92 | 1020 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_PF_UNIX));` |
|      43 | 1021 | `	SXUNUSED(pUserData);` |
|      92 | 1022 | `}` |
|      85 | 1023 | `static void PH7_STREAM_SOCK_STREAM_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1024 | `{` |
|      90 | 1025 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_STREAM));` |
|      42 | 1026 | `	SXUNUSED(pUserData);` |
|      90 | 1027 | `}` |
|      81 | 1028 | `static void PH7_STREAM_SOCK_DGRAM_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1029 | `{` |
|      86 | 1030 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_DGRAM));` |
|      40 | 1031 | `	SXUNUSED(pUserData);` |
|      86 | 1032 | `}` |
|      81 | 1033 | `static void PH7_STREAM_SOCK_RAW_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1034 | `{` |
|      86 | 1035 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_RAW));` |
|      40 | 1036 | `	SXUNUSED(pUserData);` |
|      86 | 1037 | `}` |
|      81 | 1038 | `static void PH7_STREAM_SOCK_SEQPACKET_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1039 | `{` |
|      86 | 1040 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_SEQPACKET));` |
|      40 | 1041 | `	SXUNUSED(pUserData);` |
|      86 | 1042 | `}` |
|      81 | 1043 | `static void PH7_STREAM_SOCK_RDM_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1044 | `{` |
|      86 | 1045 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_SOCK_RDM));` |
|      40 | 1046 | `	SXUNUSED(pUserData);` |
|      86 | 1047 | `}` |
|      81 | 1048 | `static void PH7_STREAM_IPPROTO_IP_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1049 | `{` |
|      86 | 1050 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_IP));` |
|      40 | 1051 | `	SXUNUSED(pUserData);` |
|      86 | 1052 | `}` |
|      81 | 1053 | `static void PH7_STREAM_IPPROTO_TCP_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1054 | `{` |
|      86 | 1055 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_TCP));` |
|      40 | 1056 | `	SXUNUSED(pUserData);` |
|      86 | 1057 | `}` |
|      81 | 1058 | `static void PH7_STREAM_IPPROTO_UDP_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1059 | `{` |
|      86 | 1060 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_UDP));` |
|      40 | 1061 | `	SXUNUSED(pUserData);` |
|      86 | 1062 | `}` |
|      81 | 1063 | `static void PH7_STREAM_IPPROTO_ICMP_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1064 | `{` |
|      86 | 1065 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_ICMP));` |
|      40 | 1066 | `	SXUNUSED(pUserData);` |
|      86 | 1067 | `}` |
|      81 | 1068 | `static void PH7_STREAM_IPPROTO_RAW_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1069 | `{` |
|      86 | 1070 | `	ph7_value_int64(pVal,PH7_NetSocketConst(PH7_NETC_IPPROTO_RAW));` |
|      40 | 1071 | `	SXUNUSED(pUserData);` |
|      86 | 1072 | `}` |
|       - | 1073 | `#endif /* PH7_ENABLE_NET */` |
|       - | 1074 | `/*` |
|       - | 1075 | ` * stream_socket_shutdown()'s $mode, and the two recvfrom/sendto flags. These` |
|       - | 1076 | ` * three ARE php's own numbers rather than the OS's: php maps STREAM_OOB and` |
|       - | 1077 | ` * STREAM_PEEK onto MSG_OOB/MSG_PEEK itself.` |
|       - | 1078 | ` */` |
|      85 | 1079 | `static void PH7_STREAM_SHUT_RD_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1080 | `{` |
|      90 | 1081 | `	ph7_value_int(pVal,PH7_STREAM_SHUT_RD);` |
|      42 | 1082 | `	SXUNUSED(pUserData);` |
|      90 | 1083 | `}` |
|      83 | 1084 | `static void PH7_STREAM_SHUT_WR_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1085 | `{` |
|      88 | 1086 | `	ph7_value_int(pVal,PH7_STREAM_SHUT_WR);` |
|      41 | 1087 | `	SXUNUSED(pUserData);` |
|      88 | 1088 | `}` |
|      83 | 1089 | `static void PH7_STREAM_SHUT_RDWR_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1090 | `{` |
|      88 | 1091 | `	ph7_value_int(pVal,PH7_STREAM_SHUT_RDWR);` |
|      41 | 1092 | `	SXUNUSED(pUserData);` |
|      88 | 1093 | `}` |
|      81 | 1094 | `static void PH7_STREAM_OOB_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1095 | `{` |
|      86 | 1096 | `	ph7_value_int(pVal,PH7_STREAM_OOB);` |
|      40 | 1097 | `	SXUNUSED(pUserData);` |
|      86 | 1098 | `}` |
|      85 | 1099 | `static void PH7_STREAM_PEEK_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1100 | `{` |
|      90 | 1101 | `	ph7_value_int(pVal,PH7_STREAM_PEEK);` |
|      42 | 1102 | `	SXUNUSED(pUserData);` |
|      90 | 1103 | `}` |
|       - | 1104 | `/*` |
|       - | 1105 | ` * stream_socket_server()'s $flags. Its default is BIND\|LISTEN, and the two are` |
|       - | 1106 | ` * separate because binding is all a datagram server does.` |
|       - | 1107 | ` */` |
|     123 | 1108 | `static void PH7_STREAM_SERVER_BIND_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1109 | `{` |
|     128 | 1110 | `	ph7_value_int(pVal,PH7_STREAM_SERVER_BIND);` |
|      61 | 1111 | `	SXUNUSED(pUserData);` |
|     128 | 1112 | `}` |
|     111 | 1113 | `static void PH7_STREAM_SERVER_LISTEN_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1114 | `{` |
|     116 | 1115 | `	ph7_value_int(pVal,PH7_STREAM_SERVER_LISTEN);` |
|      55 | 1116 | `	SXUNUSED(pUserData);` |
|     116 | 1117 | `}` |
|       - | 1118 | `/*` |
|       - | 1119 | ` * mt_srand()'s $mode: which GENERATOR to seed. MT_RAND_PHP is php's pre-7.1` |
|       - | 1120 | ` * Mersenne Twister, whose twist reads the low bit of the wrong word — a` |
|       - | 1121 | ` * different sequence, which is the only reason to ask for it.` |
|       - | 1122 | ` */` |
|      92 | 1123 | `static void PH7_MT_RAND_MT19937_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1124 | `{` |
|      97 | 1125 | `	ph7_value_int(pVal,PH7_MT_RAND_MT19937);` |
|      46 | 1126 | `	SXUNUSED(pUserData);` |
|      97 | 1127 | `}` |
|     102 | 1128 | `static void PH7_MT_RAND_PHP_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1129 | `{` |
|       - | 1130 | `	/* php 8.3 deprecated the SYMBOL as well as the mode. The notice is` |
|       - | 1131 | `	 * aDeprecatedConst[]'s now, raised where every deprecated constant's is. */` |
|     107 | 1132 | `	ph7_value_int(pVal,PH7_MT_RAND_PHP);` |
|      51 | 1133 | `	SXUNUSED(pUserData);` |
|     107 | 1134 | `}` |
|       - | 1135 | `/*` |
|       - | 1136 | ` * Output-handler flags and phases (ob_start()'s $flags, and the $phase an output` |
|       - | 1137 | ` * handler is called with). The values are php's and are a public ABI: the phase` |
|       - | 1138 | ` * bits are OR'd together (a first FLUSH arrives as FLUSH\|START = 5), and the` |
|       - | 1139 | ` * three capability flags are the ones ob_clean()/ob_flush()/ob_end_*() test` |
|       - | 1140 | ` * before they will touch the buffer.` |
|       - | 1141 | ` */` |
|     168 | 1142 | `static void PH7_OB_WRITE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1143 | `{` |
|     173 | 1144 | `	ph7_value_int(pVal,PH7_OB_WRITE);` |
|      84 | 1145 | `	SXUNUSED(pUserData);` |
|     173 | 1146 | `}` |
|      84 | 1147 | `static void PH7_OB_START_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1148 | `{` |
|      89 | 1149 | `	ph7_value_int(pVal,PH7_OB_START);` |
|      42 | 1150 | `	SXUNUSED(pUserData);` |
|      89 | 1151 | `}` |
|      84 | 1152 | `static void PH7_OB_CLEAN_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1153 | `{` |
|      89 | 1154 | `	ph7_value_int(pVal,PH7_OB_CLEAN);` |
|      42 | 1155 | `	SXUNUSED(pUserData);` |
|      89 | 1156 | `}` |
|      84 | 1157 | `static void PH7_OB_FLUSH_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1158 | `{` |
|      89 | 1159 | `	ph7_value_int(pVal,PH7_OB_FLUSH);` |
|      42 | 1160 | `	SXUNUSED(pUserData);` |
|      89 | 1161 | `}` |
|     168 | 1162 | `static void PH7_OB_FINAL_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1163 | `{` |
|     173 | 1164 | `	ph7_value_int(pVal,PH7_OB_FINAL);` |
|      84 | 1165 | `	SXUNUSED(pUserData);` |
|     173 | 1166 | `}` |
|      88 | 1167 | `static void PH7_OB_CLEANABLE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1168 | `{` |
|      93 | 1169 | `	ph7_value_int(pVal,PH7_OB_CLEANABLE);` |
|      44 | 1170 | `	SXUNUSED(pUserData);` |
|      93 | 1171 | `}` |
|      88 | 1172 | `static void PH7_OB_FLUSHABLE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1173 | `{` |
|      93 | 1174 | `	ph7_value_int(pVal,PH7_OB_FLUSHABLE);` |
|      44 | 1175 | `	SXUNUSED(pUserData);` |
|      93 | 1176 | `}` |
|      90 | 1177 | `static void PH7_OB_REMOVABLE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1178 | `{` |
|      95 | 1179 | `	ph7_value_int(pVal,PH7_OB_REMOVABLE);` |
|      45 | 1180 | `	SXUNUSED(pUserData);` |
|      95 | 1181 | `}` |
|      98 | 1182 | `static void PH7_OB_STDFLAGS_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1183 | `{` |
|     103 | 1184 | `	ph7_value_int(pVal,PH7_OB_STDFLAGS);` |
|      49 | 1185 | `	SXUNUSED(pUserData);` |
|     103 | 1186 | `}` |
|      86 | 1187 | `static void PH7_OB_STARTED_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1188 | `{` |
|      91 | 1189 | `	ph7_value_int(pVal,PH7_OB_STARTED);` |
|      43 | 1190 | `	SXUNUSED(pUserData);` |
|      91 | 1191 | `}` |
|      86 | 1192 | `static void PH7_OB_DISABLED_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1193 | `{` |
|      91 | 1194 | `	ph7_value_int(pVal,PH7_OB_DISABLED);` |
|      43 | 1195 | `	SXUNUSED(pUserData);` |
|      91 | 1196 | `}` |
|      88 | 1197 | `static void PH7_OB_PROCESSED_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1198 | `{` |
|      93 | 1199 | `	ph7_value_int(pVal,PH7_OB_PROCESSED);` |
|      44 | 1200 | `	SXUNUSED(pUserData);` |
|      93 | 1201 | `}` |
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
|      85 | 1215 | `static void PH7_CONNECTION_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1216 | `{` |
|      90 | 1217 | `	ph7_value_int(pVal,0);` |
|      42 | 1218 | `	SXUNUSED(pUserData);` |
|      90 | 1219 | `}` |
|      83 | 1220 | `static void PH7_CONNECTION_ABORTED_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1221 | `{` |
|      88 | 1222 | `	ph7_value_int(pVal,1);` |
|      41 | 1223 | `	SXUNUSED(pUserData);` |
|      88 | 1224 | `}` |
|      83 | 1225 | `static void PH7_CONNECTION_TIMEOUT_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1226 | `{` |
|      88 | 1227 | `	ph7_value_int(pVal,2);` |
|      41 | 1228 | `	SXUNUSED(pUserData);` |
|      88 | 1229 | `}` |
|      99 | 1230 | `static void PH7_ARRAY_FILTER_USE_KEY_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1231 | `{` |
|     104 | 1232 | `	ph7_value_int(pVal,2);` |
|      49 | 1233 | `	SXUNUSED(pUserData);` |
|     104 | 1234 | `}` |
|      93 | 1235 | `static void PH7_ARRAY_FILTER_USE_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1236 | `{` |
|      98 | 1237 | `	ph7_value_int(pVal,1);` |
|      46 | 1238 | `	SXUNUSED(pUserData);` |
|      98 | 1239 | `}` |
|       - | 1240 | `/*` |
|       - | 1241 | ` * COUNT_NORMAL` |
|       - | 1242 | ` *  Expands 0` |
|       - | 1243 | ` */` |
|      93 | 1244 | `static void PH7_COUNT_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1245 | `{` |
|      98 | 1246 | `	ph7_value_int(pVal,0);` |
|      46 | 1247 | `	SXUNUSED(pUserData);` |
|      98 | 1248 | `}` |
|       - | 1249 | `/*` |
|       - | 1250 | ` * COUNT_RECURSIVE` |
|       - | 1251 | ` *  Expands 1.` |
|       - | 1252 | ` */` |
|     105 | 1253 | `static void PH7_COUNT_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1254 | `{` |
|     110 | 1255 | `	ph7_value_int(pVal,1);` |
|      52 | 1256 | `	SXUNUSED(pUserData);` |
|     110 | 1257 | `}` |
|       - | 1258 | `/*` |
|       - | 1259 | ` * php's sort-flag constants. The VALUES must match php exactly: they are a` |
|       - | 1260 | ` * public ABI (code passes literal ints, dumps them, and OR-combines the base` |
|       - | 1261 | ` * type with SORT_FLAG_CASE). SORT_ASC/SORT_DESC are the array_multisort` |
|       - | 1262 | ` * direction flags.` |
|       - | 1263 | ` * SORT_REGULAR 0 · SORT_NUMERIC 1 · SORT_STRING 2 · SORT_DESC 3 · SORT_ASC 4 ·` |
|       - | 1264 | ` * SORT_LOCALE_STRING 5 · SORT_NATURAL 6 · SORT_FLAG_CASE 8` |
|       - | 1265 | ` */` |
|      97 | 1266 | `static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1267 | `{` |
|     102 | 1268 | `	ph7_value_int(pVal,4);` |
|      48 | 1269 | `	SXUNUSED(pUserData);` |
|     102 | 1270 | `}` |
|      93 | 1271 | `static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1272 | `{` |
|      98 | 1273 | `	ph7_value_int(pVal,3);` |
|      46 | 1274 | `	SXUNUSED(pUserData);` |
|      98 | 1275 | `}` |
|     115 | 1276 | `static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1277 | `{` |
|     120 | 1278 | `	ph7_value_int(pVal,0);` |
|      57 | 1279 | `	SXUNUSED(pUserData);` |
|     120 | 1280 | `}` |
|     153 | 1281 | `static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1282 | `{` |
|     158 | 1283 | `	ph7_value_int(pVal,1);` |
|      76 | 1284 | `	SXUNUSED(pUserData);` |
|     158 | 1285 | `}` |
|    2134 | 1286 | `static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1287 | `{` |
|    2139 | 1288 | `	ph7_value_int(pVal,2);` |
|    1064 | 1289 | `	SXUNUSED(pUserData);` |
|    2139 | 1290 | `}` |
|      85 | 1291 | `static void PH7_SORT_LOCALE_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1292 | `{` |
|      90 | 1293 | `	ph7_value_int(pVal,5);` |
|      42 | 1294 | `	SXUNUSED(pUserData);` |
|      90 | 1295 | `}` |
|     101 | 1296 | `static void PH7_SORT_NATURAL_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1297 | `{` |
|     106 | 1298 | `	ph7_value_int(pVal,6);` |
|      50 | 1299 | `	SXUNUSED(pUserData);` |
|     106 | 1300 | `}` |
|     107 | 1301 | `static void PH7_SORT_FLAG_CASE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1302 | `{` |
|     112 | 1303 | `	ph7_value_int(pVal,8);` |
|      53 | 1304 | `	SXUNUSED(pUserData);` |
|     112 | 1305 | `}` |
|       - | 1306 | `/*` |
|       - | 1307 | ` * PHP_ROUND_HALF_UP` |
|       - | 1308 | ` *  Expands 1.` |
|       - | 1309 | ` */` |
|      85 | 1310 | `static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1311 | `{` |
|      90 | 1312 | `	ph7_value_int(pVal,1);` |
|      42 | 1313 | `	SXUNUSED(pUserData);` |
|      90 | 1314 | `}` |
|       - | 1315 | `/*` |
|       - | 1316 | ` * PHP_SESSION_DISABLED / PHP_SESSION_NONE / PHP_SESSION_ACTIVE` |
|       - | 1317 | ` *  session_status() states (0 / 1 / 2).` |
|       - | 1318 | ` */` |
|      82 | 1319 | `static void PH7_PHP_SESSION_DISABLED_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1320 | `{` |
|      87 | 1321 | `	ph7_value_int(pVal,0);` |
|      41 | 1322 | `	SXUNUSED(pUserData);` |
|      87 | 1323 | `}` |
|      82 | 1324 | `static void PH7_PHP_SESSION_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1325 | `{` |
|      87 | 1326 | `	ph7_value_int(pVal,1);` |
|      41 | 1327 | `	SXUNUSED(pUserData);` |
|      87 | 1328 | `}` |
|      96 | 1329 | `static void PH7_PHP_SESSION_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1330 | `{` |
|     101 | 1331 | `	ph7_value_int(pVal,2);` |
|      48 | 1332 | `	SXUNUSED(pUserData);` |
|     101 | 1333 | `}` |
|       - | 1334 | `/*` |
|       - | 1335 | ` * INI_USER / INI_PERDIR / INI_SYSTEM / INI_ALL` |
|       - | 1336 | ` *  php.ini access levels (1 / 2 / 4 / 7).` |
|       - | 1337 | ` */` |
|      83 | 1338 | `static void PH7_INI_USER_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1339 | `{` |
|      88 | 1340 | `	ph7_value_int(pVal,1);` |
|      41 | 1341 | `	SXUNUSED(pUserData);` |
|      88 | 1342 | `}` |
|      83 | 1343 | `static void PH7_INI_PERDIR_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1344 | `{` |
|      88 | 1345 | `	ph7_value_int(pVal,2);` |
|      41 | 1346 | `	SXUNUSED(pUserData);` |
|      88 | 1347 | `}` |
|      83 | 1348 | `static void PH7_INI_SYSTEM_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1349 | `{` |
|      88 | 1350 | `	ph7_value_int(pVal,4);` |
|      41 | 1351 | `	SXUNUSED(pUserData);` |
|      88 | 1352 | `}` |
|      83 | 1353 | `static void PH7_INI_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1354 | `{` |
|      88 | 1355 | `	ph7_value_int(pVal,7);` |
|      41 | 1356 | `	SXUNUSED(pUserData);` |
|      88 | 1357 | `}` |
|       - | 1358 | `/*` |
|       - | 1359 | ` * MB_CASE_UPPER / MB_CASE_LOWER / MB_CASE_TITLE (0 / 1 / 2)` |
|       - | 1360 | ` */` |
|      84 | 1361 | `static void PH7_MB_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1362 | `{` |
|      89 | 1363 | `	ph7_value_int(pVal,0);` |
|      42 | 1364 | `	SXUNUSED(pUserData);` |
|      89 | 1365 | `}` |
|      84 | 1366 | `static void PH7_MB_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1367 | `{` |
|      89 | 1368 | `	ph7_value_int(pVal,1);` |
|      42 | 1369 | `	SXUNUSED(pUserData);` |
|      89 | 1370 | `}` |
|     120 | 1371 | `static void PH7_MB_CASE_TITLE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1372 | `{` |
|     125 | 1373 | `	ph7_value_int(pVal,2);` |
|      60 | 1374 | `	SXUNUSED(pUserData);` |
|     125 | 1375 | `}` |
|       - | 1376 | `/*` |
|       - | 1377 | ` * SPHP_ROUND_HALF_DOWN` |
|       - | 1378 | ` *  Expands 2.` |
|       - | 1379 | ` */` |
|      85 | 1380 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1381 | `{` |
|      90 | 1382 | `	ph7_value_int(pVal,2);` |
|      42 | 1383 | `	SXUNUSED(pUserData);` |
|      90 | 1384 | `}` |
|       - | 1385 | `/*` |
|       - | 1386 | ` * PHP_ROUND_HALF_EVEN` |
|       - | 1387 | ` *  Expands 3.` |
|       - | 1388 | ` */` |
|      89 | 1389 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1390 | `{` |
|      94 | 1391 | `	ph7_value_int(pVal,3);` |
|      44 | 1392 | `	SXUNUSED(pUserData);` |
|      94 | 1393 | `}` |
|       - | 1394 | `/*` |
|       - | 1395 | ` * PHP_ROUND_HALF_ODD` |
|       - | 1396 | ` *  Expands 4.` |
|       - | 1397 | ` */` |
|      85 | 1398 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1399 | `{` |
|      90 | 1400 | `	ph7_value_int(pVal,4);` |
|      42 | 1401 | `	SXUNUSED(pUserData);` |
|      90 | 1402 | `}` |
|       - | 1403 | `/*` |
|       - | 1404 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|       - | 1405 | ` *  Expand 0x01` |
|       - | 1406 | ` * NOTE:` |
|       - | 1407 | ` *  The expanded value must be a power of two.` |
|       - | 1408 | ` */` |
|     128 | 1409 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1410 | `{` |
|     133 | 1411 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      64 | 1412 | `	SXUNUSED(pUserData);` |
|     133 | 1413 | `}` |
|       - | 1414 | `/*` |
|       - | 1415 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|       - | 1416 | ` *  Expand 0x02` |
|       - | 1417 | ` * NOTE:` |
|       - | 1418 | ` *  The expanded value must be a power of two.` |
|       - | 1419 | ` */` |
|     156 | 1420 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1421 | `{` |
|     161 | 1422 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      78 | 1423 | `	SXUNUSED(pUserData);` |
|     161 | 1424 | `}` |
|       - | 1425 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|       - | 1426 | `/*` |
|       - | 1427 | ` * M_PI` |
|       - | 1428 | ` *  Expand the value of pi.` |
|       - | 1429 | ` */` |
|      99 | 1430 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1431 | `{` |
|      49 | 1432 | `	SXUNUSED(pUserData); /* cc warning */` |
|     104 | 1433 | `	ph7_value_double(pVal,PH7_PI);` |
|     104 | 1434 | `}` |
|       - | 1435 | `/*` |
|       - | 1436 | ` * M_E` |
|       - | 1437 | ` *  Expand 2.7182818284590452354` |
|       - | 1438 | ` */` |
|      95 | 1439 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1440 | `{` |
|      47 | 1441 | `	SXUNUSED(pUserData); /* cc warning */` |
|     100 | 1442 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|     100 | 1443 | `}` |
|       - | 1444 | `/*` |
|       - | 1445 | ` * M_LOG2E` |
|       - | 1446 | ` *  Expand 2.7182818284590452354` |
|       - | 1447 | ` */` |
|      83 | 1448 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1449 | `{` |
|      41 | 1450 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 1451 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      88 | 1452 | `}` |
|       - | 1453 | `/*` |
|       - | 1454 | ` * M_LOG10E` |
|       - | 1455 | ` *  Expand 0.4342944819032518276` |
|       - | 1456 | ` */` |
|      83 | 1457 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1458 | `{` |
|      41 | 1459 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 1460 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      88 | 1461 | `}` |
|       - | 1462 | `/*` |
|       - | 1463 | ` * M_LN2` |
|       - | 1464 | ` *  Expand 	0.69314718055994530942` |
|       - | 1465 | ` */` |
|      83 | 1466 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1467 | `{` |
|      41 | 1468 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 1469 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      88 | 1470 | `}` |
|       - | 1471 | `/*` |
|       - | 1472 | ` * M_LN10` |
|       - | 1473 | ` *  Expand 	2.30258509299404568402` |
|       - | 1474 | ` */` |
|      83 | 1475 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1476 | `{` |
|      41 | 1477 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 1478 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      88 | 1479 | `}` |
|       - | 1480 | `/*` |
|       - | 1481 | ` * M_PI_2` |
|       - | 1482 | ` *  Expand 	1.57079632679489661923` |
|       - | 1483 | ` */` |
|      83 | 1484 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1485 | `{` |
|      41 | 1486 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 1487 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      88 | 1488 | `}` |
|       - | 1489 | `/*` |
|       - | 1490 | ` * M_PI_4` |
|       - | 1491 | ` *  Expand 	0.78539816339744830962` |
|       - | 1492 | ` */` |
|      83 | 1493 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1494 | `{` |
|      41 | 1495 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 1496 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      88 | 1497 | `}` |
|       - | 1498 | `/*` |
|       - | 1499 | ` * M_1_PI` |
|       - | 1500 | ` *  Expand 	0.31830988618379067154` |
|       - | 1501 | ` */` |
|      83 | 1502 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1503 | `{` |
|      41 | 1504 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 1505 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      88 | 1506 | `}` |
|       - | 1507 | `/*` |
|       - | 1508 | ` * M_2_PI` |
|       - | 1509 | ` *  Expand 0.63661977236758134308` |
|       - | 1510 | ` */` |
|      85 | 1511 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1512 | `{` |
|      42 | 1513 | `	SXUNUSED(pUserData); /* cc warning */` |
|      90 | 1514 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      90 | 1515 | `}` |
|       - | 1516 | `/*` |
|       - | 1517 | ` * M_SQRTPI` |
|       - | 1518 | ` *  Expand 1.77245385090551602729` |
|       - | 1519 | ` */` |
|      83 | 1520 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1521 | `{` |
|      41 | 1522 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 1523 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      88 | 1524 | `}` |
|       - | 1525 | `/*` |
|       - | 1526 | ` * M_2_SQRTPI` |
|       - | 1527 | ` *  Expand 	1.12837916709551257390` |
|       - | 1528 | ` */` |
|      83 | 1529 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1530 | `{` |
|      41 | 1531 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 1532 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      88 | 1533 | `}` |
|       - | 1534 | `/*` |
|       - | 1535 | ` * M_SQRT2` |
|       - | 1536 | ` *  Expand 	1.41421356237309504880` |
|       - | 1537 | ` */` |
|      83 | 1538 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1539 | `{` |
|      41 | 1540 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 1541 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      88 | 1542 | `}` |
|       - | 1543 | `/*` |
|       - | 1544 | ` * M_SQRT3` |
|       - | 1545 | ` *  Expand 	1.73205080756887729352` |
|       - | 1546 | ` */` |
|      83 | 1547 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1548 | `{` |
|      41 | 1549 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 1550 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      88 | 1551 | `}` |
|       - | 1552 | `/*` |
|       - | 1553 | ` * M_SQRT1_2` |
|       - | 1554 | ` *  Expand 	0.70710678118654752440` |
|       - | 1555 | ` */` |
|      83 | 1556 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1557 | `{` |
|      41 | 1558 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 1559 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      88 | 1560 | `}` |
|       - | 1561 | `/*` |
|       - | 1562 | ` * M_LNPI` |
|       - | 1563 | ` *  Expand 	1.14472988584940017414` |
|       - | 1564 | ` */` |
|      83 | 1565 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1566 | `{` |
|      41 | 1567 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 1568 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      88 | 1569 | `}` |
|       - | 1570 | `/*` |
|       - | 1571 | ` * M_EULER` |
|       - | 1572 | ` *  Expand  0.57721566490153286061` |
|       - | 1573 | ` */` |
|      83 | 1574 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1575 | `{` |
|      41 | 1576 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 1577 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      88 | 1578 | `}` |
|       - | 1579 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|       - | 1580 | `/*` |
|       - | 1581 | ` * SUNFUNCS_RET_TIMESTAMP / SUNFUNCS_RET_STRING / SUNFUNCS_RET_DOUBLE` |
|       - | 1582 | ` *  The three shapes date_sunrise() and date_sunset() can answer in: an` |
|       - | 1583 | ` *  absolute Unix timestamp, an "H:i" clock face, or the hour as a float.` |
|       - | 1584 | ` *  STRING is the default, which is why it is 1 rather than 0.` |
|       - | 1585 | ` */` |
|      82 | 1586 | `static void PH7_SUNFUNCS_RET_TIMESTAMP_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1587 | `{` |
|      41 | 1588 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1589 | `	ph7_value_int(pVal,0);` |
|      87 | 1590 | `}` |
|      84 | 1591 | `static void PH7_SUNFUNCS_RET_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1592 | `{` |
|      42 | 1593 | `	SXUNUSED(pUserData); /* cc warning */` |
|      89 | 1594 | `	ph7_value_int(pVal,1);` |
|      89 | 1595 | `}` |
|      82 | 1596 | `static void PH7_SUNFUNCS_RET_DOUBLE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1597 | `{` |
|      41 | 1598 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1599 | `	ph7_value_int(pVal,2);` |
|      87 | 1600 | `}` |
|       - | 1601 | `/*` |
|       - | 1602 | ` * DATE_ATOM` |
|       - | 1603 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|       - | 1604 | ` */` |
|     164 | 1605 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1606 | `{` |
|      82 | 1607 | `	SXUNUSED(pUserData); /* cc warning */` |
|     169 | 1608 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|     169 | 1609 | `}` |
|       - | 1610 | `/*` |
|       - | 1611 | ` * DATE_COOKIE` |
|       - | 1612 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|       - | 1613 | ` */` |
|      82 | 1614 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1615 | `{` |
|      41 | 1616 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1617 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      87 | 1618 | `}` |
|       - | 1619 | `/*` |
|       - | 1620 | ` * DATE_ISO8601` |
|       - | 1621 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|       - | 1622 | ` */` |
|      82 | 1623 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1624 | `{` |
|      41 | 1625 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1626 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      87 | 1627 | `}` |
|       - | 1628 | `/*` |
|       - | 1629 | ` * DATE_RFC822` |
|       - | 1630 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|       - | 1631 | ` */` |
|      82 | 1632 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1633 | `{` |
|      41 | 1634 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1635 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      87 | 1636 | `}` |
|       - | 1637 | `/*` |
|       - | 1638 | ` * DATE_RFC850` |
|       - | 1639 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|       - | 1640 | ` */` |
|      82 | 1641 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1642 | `{` |
|      41 | 1643 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1644 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      87 | 1645 | `}` |
|       - | 1646 | `/*` |
|       - | 1647 | ` * DATE_RFC1036` |
|       - | 1648 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1649 | ` */` |
|      82 | 1650 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1651 | `{` |
|      41 | 1652 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1653 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      87 | 1654 | `}` |
|       - | 1655 | `/*` |
|       - | 1656 | ` * DATE_RFC1123` |
|       - | 1657 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1658 | ` */` |
|      82 | 1659 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1660 | `{` |
|      41 | 1661 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1662 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      87 | 1663 | `}` |
|       - | 1664 | `/*` |
|       - | 1665 | ` * DATE_RFC2822` |
|       - | 1666 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1667 | ` */` |
|      82 | 1668 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1669 | `{` |
|      41 | 1670 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1671 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      87 | 1672 | `}` |
|       - | 1673 | `/*` |
|       - | 1674 | ` * DATE_RSS` |
|       - | 1675 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|       - | 1676 | ` */` |
|      82 | 1677 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1678 | `{` |
|      41 | 1679 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1680 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      87 | 1681 | `}` |
|       - | 1682 | `/*` |
|       - | 1683 | ` * DATE_W3C` |
|       - | 1684 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|       - | 1685 | ` */` |
|      82 | 1686 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1687 | `{` |
|      41 | 1688 | `	SXUNUSED(pUserData); /* cc warning */` |
|      87 | 1689 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      87 | 1690 | `}` |
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
|      88 | 1703 | `static void PH7_DATE_RFC7231_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1704 | `{` |
|      44 | 1705 | `	SXUNUSED(pUserData); /* cc warning */` |
|      93 | 1706 | `	ph7_value_string(pVal,"D, d M Y H:i:s \\G\\M\\T",-1/*Compute length automatically*/);` |
|      93 | 1707 | `}` |
|      80 | 1708 | `static void PH7_DATE_RFC3339_EXTENDED_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1709 | `{` |
|      40 | 1710 | `	SXUNUSED(pUserData); /* cc warning */` |
|      85 | 1711 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:s.vP",-1/*Compute length automatically*/);` |
|      85 | 1712 | `}` |
|      80 | 1713 | `static void PH7_DATE_ISO8601_EXPANDED_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1714 | `{` |
|      40 | 1715 | `	SXUNUSED(pUserData); /* cc warning */` |
|      85 | 1716 | `	ph7_value_string(pVal,"X-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      85 | 1717 | `}` |
|       - | 1718 | `/*` |
|       - | 1719 | ` * FILE_TEXT / FILE_BINARY` |
|       - | 1720 | ` *  Both expand 0. php declares them for file()/file_put_contents()'s $flags and` |
|       - | 1721 | ` *  ignores them (the CLI has no text mode to select), but a program that names` |
|       - | 1722 | ` *  one still has to COMPILE, and an undefined constant is a fatal.` |
|       - | 1723 | ` */` |
|     178 | 1724 | `static void PH7_FILE_TEXT_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1725 | `{` |
|     183 | 1726 | `	ph7_value_int(pVal,0);` |
|      88 | 1727 | `	SXUNUSED(pUserData);` |
|     183 | 1728 | `}` |
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
|      95 | 1739 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1740 | `{` |
|      47 | 1741 | `	SXUNUSED(pUserData); /* cc warning */` |
|     100 | 1742 | `	ph7_value_int(pVal,PH7_ENT_QUOTE_DOUBLE);` |
|     100 | 1743 | `}` |
|       - | 1744 | `/*` |
|       - | 1745 | ` * ENT_QUOTES` |
|       - | 1746 | ` *  Expand 3 (double\|single quote bits)` |
|       - | 1747 | ` */` |
|     261 | 1748 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1749 | `{` |
|     130 | 1750 | `	SXUNUSED(pUserData); /* cc warning */` |
|     266 | 1751 | `	ph7_value_int(pVal,PH7_ENT_QUOTES);` |
|     266 | 1752 | `}` |
|       - | 1753 | `/*` |
|       - | 1754 | ` * ENT_NOQUOTES` |
|       - | 1755 | ` *  Expand 0 (no quote bits)` |
|       - | 1756 | ` */` |
|     103 | 1757 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1758 | `{` |
|      51 | 1759 | `	SXUNUSED(pUserData); /* cc warning */` |
|     108 | 1760 | `	ph7_value_int(pVal,0);` |
|     108 | 1761 | `}` |
|       - | 1762 | `/*` |
|       - | 1763 | ` * ENT_IGNORE` |
|       - | 1764 | ` *  Expand 4` |
|       - | 1765 | ` */` |
|      87 | 1766 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1767 | `{` |
|      43 | 1768 | `	SXUNUSED(pUserData); /* cc warning */` |
|      92 | 1769 | `	ph7_value_int(pVal,PH7_ENT_IGNORE);` |
|      92 | 1770 | `}` |
|       - | 1771 | `/*` |
|       - | 1772 | ` * ENT_SUBSTITUTE` |
|       - | 1773 | ` *  Expand 8` |
|       - | 1774 | ` */` |
|     109 | 1775 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1776 | `{` |
|      54 | 1777 | `	SXUNUSED(pUserData); /* cc warning */` |
|     114 | 1778 | `	ph7_value_int(pVal,PH7_ENT_SUBSTITUTE);` |
|     114 | 1779 | `}` |
|       - | 1780 | `/*` |
|       - | 1781 | ` * ENT_DISALLOWED` |
|       - | 1782 | ` *  Expand 128` |
|       - | 1783 | ` */` |
|     135 | 1784 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1785 | `{` |
|      67 | 1786 | `	SXUNUSED(pUserData); /* cc warning */` |
|     140 | 1787 | `	ph7_value_int(pVal,PH7_ENT_DISALLOWED);` |
|     140 | 1788 | `}` |
|       - | 1789 | `/*` |
|       - | 1790 | ` * ENT_HTML401` |
|       - | 1791 | ` *  Expand 0 (the default doctype)` |
|       - | 1792 | ` */` |
|     109 | 1793 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1794 | `{` |
|      54 | 1795 | `	SXUNUSED(pUserData); /* cc warning */` |
|     114 | 1796 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML401);` |
|     114 | 1797 | `}` |
|       - | 1798 | `/*` |
|       - | 1799 | ` * ENT_XML1` |
|       - | 1800 | ` *  Expand 16` |
|       - | 1801 | ` */` |
|      91 | 1802 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1803 | `{` |
|      45 | 1804 | `	SXUNUSED(pUserData); /* cc warning */` |
|      96 | 1805 | `	ph7_value_int(pVal,PH7_ENT_DOC_XML1);` |
|      96 | 1806 | `}` |
|       - | 1807 | `/*` |
|       - | 1808 | ` * ENT_XHTML` |
|       - | 1809 | ` *  Expand 32` |
|       - | 1810 | ` */` |
|      87 | 1811 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1812 | `{` |
|      43 | 1813 | `	SXUNUSED(pUserData); /* cc warning */` |
|      92 | 1814 | `	ph7_value_int(pVal,PH7_ENT_DOC_XHTML);` |
|      92 | 1815 | `}` |
|       - | 1816 | `/*` |
|       - | 1817 | ` * ENT_HTML5` |
|       - | 1818 | ` *  Expand 48 (16\|32 — a doctype composite, not a flag bit)` |
|       - | 1819 | ` */` |
|      91 | 1820 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1821 | `{` |
|      45 | 1822 | `	SXUNUSED(pUserData); /* cc warning */` |
|      96 | 1823 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML5);` |
|      96 | 1824 | `}` |
|       - | 1825 | `/*` |
|       - | 1826 | ` * ISO-8859-1` |
|       - | 1827 | ` * ISO_8859_1` |
|       - | 1828 | ` *   Expand 1` |
|       - | 1829 | ` */` |
|     166 | 1830 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1831 | `{` |
|      83 | 1832 | `	SXUNUSED(pUserData); /* cc warning */` |
|     171 | 1833 | `	ph7_value_int(pVal,1);` |
|     171 | 1834 | `}` |
|       - | 1835 | `/*` |
|       - | 1836 | ` * UTF-8` |
|       - | 1837 | ` * UTF8` |
|       - | 1838 | ` *  Expand 2` |
|       - | 1839 | ` */` |
|     166 | 1840 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1841 | `{` |
|      83 | 1842 | `	SXUNUSED(pUserData); /* cc warning */` |
|     171 | 1843 | `	ph7_value_int(pVal,1);` |
|     171 | 1844 | `}` |
|       - | 1845 | `/*` |
|       - | 1846 | ` * HTML_ENTITIES` |
|       - | 1847 | ` *  Expand 1` |
|       - | 1848 | ` */` |
|     109 | 1849 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1850 | `{` |
|      54 | 1851 | `	SXUNUSED(pUserData); /* cc warning */` |
|     114 | 1852 | `	ph7_value_int(pVal,1);` |
|     114 | 1853 | `}` |
|       - | 1854 | `/*` |
|       - | 1855 | ` * HTML_SPECIALCHARS` |
|       - | 1856 | ` *  Expand 0 (PHP-exact)` |
|       - | 1857 | ` */` |
|      99 | 1858 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1859 | `{` |
|      49 | 1860 | `	SXUNUSED(pUserData); /* cc warning */` |
|     104 | 1861 | `	ph7_value_int(pVal,0);` |
|     104 | 1862 | `}` |
|       - | 1863 | `/*` |
|       - | 1864 | ` * PHP_URL_SCHEME.` |
|       - | 1865 | ` * Expand 0` |
|       - | 1866 | ` */` |
|      85 | 1867 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1868 | `{` |
|      42 | 1869 | `	SXUNUSED(pUserData); /* cc warning */` |
|      90 | 1870 | `	ph7_value_int(pVal,0);` |
|      90 | 1871 | `}` |
|       - | 1872 | `/*` |
|       - | 1873 | ` * PHP_URL_HOST.` |
|       - | 1874 | ` * Expand 1` |
|       - | 1875 | ` */` |
|      87 | 1876 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1877 | `{` |
|      43 | 1878 | `	SXUNUSED(pUserData); /* cc warning */` |
|      92 | 1879 | `	ph7_value_int(pVal,1);` |
|      92 | 1880 | `}` |
|       - | 1881 | `/*` |
|       - | 1882 | ` * PHP_URL_PORT.` |
|       - | 1883 | ` * Expand 2` |
|       - | 1884 | ` */` |
|      87 | 1885 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1886 | `{` |
|      43 | 1887 | `	SXUNUSED(pUserData); /* cc warning */` |
|      92 | 1888 | `	ph7_value_int(pVal,2);` |
|      92 | 1889 | `}` |
|       - | 1890 | `/*` |
|       - | 1891 | ` * PHP_URL_USER.` |
|       - | 1892 | ` * Expand 3` |
|       - | 1893 | ` */` |
|      85 | 1894 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1895 | `{` |
|      42 | 1896 | `	SXUNUSED(pUserData); /* cc warning */` |
|      90 | 1897 | `	ph7_value_int(pVal,3);` |
|      90 | 1898 | `}` |
|       - | 1899 | `/*` |
|       - | 1900 | ` * PHP_URL_PASS.` |
|       - | 1901 | ` * Expand 4` |
|       - | 1902 | ` */` |
|      85 | 1903 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1904 | `{` |
|      42 | 1905 | `	SXUNUSED(pUserData); /* cc warning */` |
|      90 | 1906 | `	ph7_value_int(pVal,4);` |
|      90 | 1907 | `}` |
|       - | 1908 | `/*` |
|       - | 1909 | ` * PHP_URL_PATH.` |
|       - | 1910 | ` * Expand 5` |
|       - | 1911 | ` */` |
|      85 | 1912 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1913 | `{` |
|      42 | 1914 | `	SXUNUSED(pUserData); /* cc warning */` |
|      90 | 1915 | `	ph7_value_int(pVal,5);` |
|      90 | 1916 | `}` |
|       - | 1917 | `/*` |
|       - | 1918 | ` * PHP_URL_QUERY.` |
|       - | 1919 | ` * Expand 6` |
|       - | 1920 | ` */` |
|      87 | 1921 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1922 | `{` |
|      43 | 1923 | `	SXUNUSED(pUserData); /* cc warning */` |
|      92 | 1924 | `	ph7_value_int(pVal,6);` |
|      92 | 1925 | `}` |
|       - | 1926 | `/*` |
|       - | 1927 | ` * PHP_URL_FRAGMENT.` |
|       - | 1928 | ` * Expand 7` |
|       - | 1929 | ` */` |
|      87 | 1930 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1931 | `{` |
|      43 | 1932 | `	SXUNUSED(pUserData); /* cc warning */` |
|      92 | 1933 | `	ph7_value_int(pVal,7);` |
|      92 | 1934 | `}` |
|       - | 1935 | `/*` |
|       - | 1936 | ` * PHP_QUERY_RFC1738` |
|       - | 1937 | ` * Expand 1` |
|       - | 1938 | ` */` |
|      83 | 1939 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1940 | `{` |
|      41 | 1941 | `	SXUNUSED(pUserData); /* cc warning */` |
|      88 | 1942 | `	ph7_value_int(pVal,1);` |
|      88 | 1943 | `}` |
|       - | 1944 | `/*` |
|       - | 1945 | ` * PHP_QUERY_RFC3986` |
|       - | 1946 | ` * Expand 1` |
|       - | 1947 | ` */` |
|      85 | 1948 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1949 | `{` |
|      42 | 1950 | `	SXUNUSED(pUserData); /* cc warning */` |
|      90 | 1951 | `	ph7_value_int(pVal,2);` |
|      90 | 1952 | `}` |
|       - | 1953 | `/* php's FNM_* values (ext/standard): PATHNAME=1, NOESCAPE=2, PERIOD=4, CASEFOLD=16.` |
|       - | 1954 | ` * PHL previously had PATHNAME/NOESCAPE swapped and CASEFOLD=8; fnmatch() reads these` |
|       - | 1955 | ` * bits, so PH7_builtin_fnmatch was updated to the same values. */` |
|       - | 1956 | `/*` |
|       - | 1957 | ` * FNM_PATHNAME` |
|       - | 1958 | ` *  Expand 1 (php value)` |
|       - | 1959 | ` */` |
|      81 | 1960 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1961 | `{` |
|      40 | 1962 | `	SXUNUSED(pUserData); /* cc warning */` |
|      86 | 1963 | `	ph7_value_int(pVal,1);` |
|      86 | 1964 | `}` |
|       - | 1965 | `/*` |
|       - | 1966 | ` * FNM_NOESCAPE` |
|       - | 1967 | ` *  Expand 2 (php value)` |
|       - | 1968 | ` */` |
|     131 | 1969 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1970 | `{` |
|      65 | 1971 | `	SXUNUSED(pUserData); /* cc warning */` |
|     136 | 1972 | `	ph7_value_int(pVal,2);` |
|     136 | 1973 | `}` |
|       - | 1974 | `/*` |
|       - | 1975 | ` * FNM_PERIOD` |
|       - | 1976 | ` *  Expand 4 (php value)` |
|       - | 1977 | ` */` |
|      87 | 1978 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1979 | `{` |
|      43 | 1980 | `	SXUNUSED(pUserData); /* cc warning */` |
|      92 | 1981 | `	ph7_value_int(pVal,4);` |
|      92 | 1982 | `}` |
|       - | 1983 | `/*` |
|       - | 1984 | ` * FNM_CASEFOLD` |
|       - | 1985 | ` *  Expand 16 (php value)` |
|       - | 1986 | ` */` |
|     127 | 1987 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1988 | `{` |
|      63 | 1989 | `	SXUNUSED(pUserData); /* cc warning */` |
|     132 | 1990 | `	ph7_value_int(pVal,16);` |
|     132 | 1991 | `}` |
|       - | 1992 | `/*` |
|       - | 1993 | ` * PATHINFO_DIRNAME` |
|       - | 1994 | ` *  Expand 1.` |
|       - | 1995 | ` */` |
|     103 | 1996 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 1997 | `{` |
|      51 | 1998 | `	SXUNUSED(pUserData); /* cc warning */` |
|     108 | 1999 | `	ph7_value_int(pVal,PH7_PATHINFO_DIRNAME);` |
|     108 | 2000 | `}` |
|       - | 2001 | `/*` |
|       - | 2002 | ` * PATHINFO_BASENAME` |
|       - | 2003 | ` *  Expand 2.` |
|       - | 2004 | ` */` |
|     103 | 2005 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2006 | `{` |
|      51 | 2007 | `	SXUNUSED(pUserData); /* cc warning */` |
|     108 | 2008 | `	ph7_value_int(pVal,PH7_PATHINFO_BASENAME);` |
|     108 | 2009 | `}` |
|       - | 2010 | `/*` |
|       - | 2011 | ` * PATHINFO_EXTENSION` |
|       - | 2012 | ` *  Expand php's 4 (a POWER OF TWO: the components are a bitmask).` |
|       - | 2013 | ` */` |
|   10505 | 2014 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2015 | `{` |
|    5252 | 2016 | `	SXUNUSED(pUserData); /* cc warning */` |
|   10510 | 2017 | `	ph7_value_int(pVal,PH7_PATHINFO_EXTENSION);` |
|   10510 | 2018 | `}` |
|       - | 2019 | `/*` |
|       - | 2020 | ` * PATHINFO_FILENAME` |
|       - | 2021 | ` *  Expand php's 8 (a POWER OF TWO: the components are a bitmask).` |
|       - | 2022 | ` */` |
|   10471 | 2023 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2024 | `{` |
|    5235 | 2025 | `	SXUNUSED(pUserData); /* cc warning */` |
|   10476 | 2026 | `	ph7_value_int(pVal,PH7_PATHINFO_FILENAME);` |
|   10476 | 2027 | `}` |
|       - | 2028 | `/*` |
|       - | 2029 | ` * PATHINFO_ALL` |
|       - | 2030 | ` *  Expand php's 15 — the default, and the one value that answers with the ARRAY.` |
|       - | 2031 | ` */` |
|      91 | 2032 | `static void PH7_PATHINFO_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2033 | `{` |
|      45 | 2034 | `	SXUNUSED(pUserData); /* cc warning */` |
|      96 | 2035 | `	ph7_value_int(pVal,PH7_PATHINFO_ALL);` |
|      96 | 2036 | `}` |
|       - | 2037 | `#ifdef PH7_ENABLE_PCRE` |
|       - | 2038 | `/*` |
|       - | 2039 | ` * php's four PCRE build constants, asked of the linked library (see` |
|       - | 2040 | ` * PH7_PcreVersionInfo). Composer reads PCRE_VERSION before it loads a repository.` |
|       - | 2041 | ` */` |
|      84 | 2042 | `static void PH7_PCRE_VERSION_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2043 | `{` |
|       - | 2044 | `	char zVer[64];` |
|      42 | 2045 | `	SXUNUSED(pUserData);` |
|      89 | 2046 | `	PH7_PcreVersionInfo(zVer,(int)sizeof(zVer),0,0,0);` |
|      89 | 2047 | `	ph7_value_string(pVal,zVer,-1);` |
|      89 | 2048 | `}` |
|      82 | 2049 | `static void PH7_PCRE_VERSION_MAJOR_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2050 | `{` |
|       - | 2051 | `	char zVer[64];` |
|      87 | 2052 | `	int iMaj = 0;` |
|      41 | 2053 | `	SXUNUSED(pUserData);` |
|      87 | 2054 | `	PH7_PcreVersionInfo(zVer,(int)sizeof(zVer),&iMaj,0,0);` |
|      87 | 2055 | `	ph7_value_int(pVal,iMaj);` |
|      87 | 2056 | `}` |
|      82 | 2057 | `static void PH7_PCRE_VERSION_MINOR_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2058 | `{` |
|       - | 2059 | `	char zVer[64];` |
|      87 | 2060 | `	int iMin = 0;` |
|      41 | 2061 | `	SXUNUSED(pUserData);` |
|      87 | 2062 | `	PH7_PcreVersionInfo(zVer,(int)sizeof(zVer),0,&iMin,0);` |
|      87 | 2063 | `	ph7_value_int(pVal,iMin);` |
|      87 | 2064 | `}` |
|      82 | 2065 | `static void PH7_PCRE_JIT_SUPPORT_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2066 | `{` |
|       - | 2067 | `	char zVer[64];` |
|      87 | 2068 | `	int iJit = 0;` |
|      41 | 2069 | `	SXUNUSED(pUserData);` |
|      87 | 2070 | `	PH7_PcreVersionInfo(zVer,(int)sizeof(zVer),0,0,&iJit);` |
|      87 | 2071 | `	ph7_value_bool(pVal,iJit);` |
|      87 | 2072 | `}` |
|       - | 2073 | `#endif /* PH7_ENABLE_PCRE */` |
|       - | 2074 | `/*` |
|       - | 2075 | ` * php's four BUILD-SHAPE booleans. A script reads them to decide what the engine` |
|       - | 2076 | ``  * can do, not what it is called: symfony/process asks `defined('ZEND_THREAD_SAFE')` `` |
|       - | 2077 | `` * to know whether `proc_open` needs an explicit cwd, and with the constant simply`` |
|       - | 2078 | `` * ABSENT it passed null and every subprocess Composer runs died in `is_dir(null)`.`` |
|       - | 2079 | ` * PHL runs one VM per thread with no shared globals, so it answers php's` |
|       - | 2080 | ` * non-ZTS, non-debug shape.` |
|       - | 2081 | ` */` |
|      84 | 2082 | `static void PH7_ZEND_THREAD_SAFE_Const(ph7_value *pVal,void *pUserData)` |
|      89 | 2083 | `{ SXUNUSED(pUserData); ph7_value_bool(pVal,0); }` |
|      84 | 2084 | `static void PH7_ZEND_DEBUG_BUILD_Const(ph7_value *pVal,void *pUserData)` |
|      89 | 2085 | `{ SXUNUSED(pUserData); ph7_value_bool(pVal,0); }` |
|      84 | 2086 | `static void PH7_PHP_ZTS_Const(ph7_value *pVal,void *pUserData)` |
|      89 | 2087 | `{ SXUNUSED(pUserData); ph7_value_bool(pVal,0); }` |
|      84 | 2088 | `static void PH7_PHP_DEBUG_Const(ph7_value *pVal,void *pUserData)` |
|      89 | 2089 | `{ SXUNUSED(pUserData); ph7_value_bool(pVal,0); }` |
|       - | 2090 | `/*` |
|       - | 2091 | ` * php's phpinfo() SECTION flags. A script passes one to say which part it wants;` |
|       - | 2092 | ` * symfony/process asks for INFO_GENERAL to read the build's configure line (it is` |
|       - | 2093 | `` * how it detects `--enable-sigchild`), so Composer needs them to start at all.`` |
|       - | 2094 | ` */` |
|      82 | 2095 | `static void PH7_INFO_GENERAL_Const(ph7_value *pVal,void *pUserData)` |
|      87 | 2096 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,1); }` |
|      82 | 2097 | `static void PH7_INFO_CREDITS_Const(ph7_value *pVal,void *pUserData)` |
|      87 | 2098 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,2); }` |
|      82 | 2099 | `static void PH7_INFO_CONFIGURATION_Const(ph7_value *pVal,void *pUserData)` |
|      87 | 2100 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,4); }` |
|      82 | 2101 | `static void PH7_INFO_MODULES_Const(ph7_value *pVal,void *pUserData)` |
|      87 | 2102 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,8); }` |
|      82 | 2103 | `static void PH7_INFO_ENVIRONMENT_Const(ph7_value *pVal,void *pUserData)` |
|      87 | 2104 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,16); }` |
|      82 | 2105 | `static void PH7_INFO_VARIABLES_Const(ph7_value *pVal,void *pUserData)` |
|      87 | 2106 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,32); }` |
|      82 | 2107 | `static void PH7_INFO_LICENSE_Const(ph7_value *pVal,void *pUserData)` |
|      87 | 2108 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,64); }` |
|      82 | 2109 | `static void PH7_INFO_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      87 | 2110 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,4294967295LL); }` |
|       - | 2111 | `/*` |
|       - | 2112 | ` * The LC_* CATEGORY numbers. php registers the C library's own macros, so its` |
|       - | 2113 | ` * numbers are the platform's (macOS and Windows put LC_ALL at 0); these are` |
|       - | 2114 | ` * glibc's on every platform -- a script that uses the names cannot tell, one` |
|       - | 2115 | ` * that prints the numbers can -- and setlocale() maps them to the platform's` |
|       - | 2116 | ` * macros.` |
|       - | 2117 | ` */` |
|      85 | 2118 | `static void PH7_LC_CTYPE_Const(ph7_value *pVal,void *pUserData)` |
|      90 | 2119 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,0); }` |
|      84 | 2120 | `static void PH7_LC_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|      89 | 2121 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,1); }` |
|      82 | 2122 | `static void PH7_LC_TIME_Const(ph7_value *pVal,void *pUserData)` |
|      87 | 2123 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,2); }` |
|      82 | 2124 | `static void PH7_LC_COLLATE_Const(ph7_value *pVal,void *pUserData)` |
|      87 | 2125 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,3); }` |
|      83 | 2126 | `static void PH7_LC_MONETARY_Const(ph7_value *pVal,void *pUserData)` |
|      88 | 2127 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,4); }` |
|      91 | 2128 | `static void PH7_LC_MESSAGES_Const(ph7_value *pVal,void *pUserData)` |
|      96 | 2129 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,5); }` |
|      96 | 2130 | `static void PH7_LC_ALL_Const(ph7_value *pVal,void *pUserData)` |
|     101 | 2131 | `{ SXUNUSED(pUserData); ph7_value_int(pVal,6); }` |
|       - | 2132 | `/*` |
|       - | 2133 | ` * ext/posix's constants. Every one of them is the PLATFORM's macro rather than` |
|       - | 2134 | `` * a number copied out of one build: `RLIMIT_AS` is 9 on Linux and something`` |
|       - | 2135 | ` * else elsewhere, and a script that stores one and hands it back to` |
|       - | 2136 | ` * posix_getrlimit() has to get its own system's answer. php builds no` |
|       - | 2137 | ` * ext/posix on Windows, so none of these is defined there either.` |
|       - | 2138 | ` */` |
|       - | 2139 | `#ifndef __WINNT__` |
|       - | 2140 | `#ifdef F_OK` |
|      80 | 2141 | `static void PH7_POSIX_F_OK_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2142 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)F_OK); }` |
|       - | 2143 | `#endif` |
|       - | 2144 | `#ifdef X_OK` |
|      80 | 2145 | `static void PH7_POSIX_X_OK_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2146 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)X_OK); }` |
|       - | 2147 | `#endif` |
|       - | 2148 | `#ifdef W_OK` |
|      80 | 2149 | `static void PH7_POSIX_W_OK_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2150 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)W_OK); }` |
|       - | 2151 | `#endif` |
|       - | 2152 | `#ifdef R_OK` |
|      80 | 2153 | `static void PH7_POSIX_R_OK_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2154 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)R_OK); }` |
|       - | 2155 | `#endif` |
|       - | 2156 | `#ifdef S_IFREG` |
|      80 | 2157 | `static void PH7_POSIX_S_IFREG_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2158 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFREG); }` |
|       - | 2159 | `#endif` |
|       - | 2160 | `#ifdef S_IFCHR` |
|      81 | 2161 | `static void PH7_POSIX_S_IFCHR_Const(ph7_value *pVal,void *pUserData)` |
|      81 | 2162 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFCHR); }` |
|       - | 2163 | `#endif` |
|       - | 2164 | `#ifdef S_IFBLK` |
|      81 | 2165 | `static void PH7_POSIX_S_IFBLK_Const(ph7_value *pVal,void *pUserData)` |
|      81 | 2166 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFBLK); }` |
|       - | 2167 | `#endif` |
|       - | 2168 | `#ifdef S_IFIFO` |
|      82 | 2169 | `static void PH7_POSIX_S_IFIFO_Const(ph7_value *pVal,void *pUserData)` |
|      82 | 2170 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFIFO); }` |
|       - | 2171 | `#endif` |
|       - | 2172 | `#ifdef S_IFSOCK` |
|      81 | 2173 | `static void PH7_POSIX_S_IFSOCK_Const(ph7_value *pVal,void *pUserData)` |
|      81 | 2174 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)S_IFSOCK); }` |
|       - | 2175 | `#endif` |
|       - | 2176 | `#ifdef RLIMIT_AS` |
|      80 | 2177 | `static void PH7_POSIX_RLIMIT_AS_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2178 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_AS); }` |
|       - | 2179 | `#endif` |
|       - | 2180 | `#ifdef RLIMIT_CORE` |
|      81 | 2181 | `static void PH7_POSIX_RLIMIT_CORE_Const(ph7_value *pVal,void *pUserData)` |
|      81 | 2182 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_CORE); }` |
|       - | 2183 | `#endif` |
|       - | 2184 | `#ifdef RLIMIT_CPU` |
|      80 | 2185 | `static void PH7_POSIX_RLIMIT_CPU_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2186 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_CPU); }` |
|       - | 2187 | `#endif` |
|       - | 2188 | `#ifdef RLIMIT_DATA` |
|      80 | 2189 | `static void PH7_POSIX_RLIMIT_DATA_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2190 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_DATA); }` |
|       - | 2191 | `#endif` |
|       - | 2192 | `#ifdef RLIMIT_FSIZE` |
|      80 | 2193 | `static void PH7_POSIX_RLIMIT_FSIZE_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2194 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_FSIZE); }` |
|       - | 2195 | `#endif` |
|       - | 2196 | `#ifdef RLIMIT_LOCKS` |
|      40 | 2197 | `static void PH7_POSIX_RLIMIT_LOCKS_Const(ph7_value *pVal,void *pUserData)` |
|      40 | 2198 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_LOCKS); }` |
|       - | 2199 | `#endif` |
|       - | 2200 | `#ifdef RLIMIT_MEMLOCK` |
|      80 | 2201 | `static void PH7_POSIX_RLIMIT_MEMLOCK_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2202 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_MEMLOCK); }` |
|       - | 2203 | `#endif` |
|       - | 2204 | `#ifdef RLIMIT_MSGQUEUE` |
|      40 | 2205 | `static void PH7_POSIX_RLIMIT_MSGQUEUE_Const(ph7_value *pVal,void *pUserData)` |
|      40 | 2206 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_MSGQUEUE); }` |
|       - | 2207 | `#endif` |
|       - | 2208 | `#ifdef RLIMIT_NICE` |
|      40 | 2209 | `static void PH7_POSIX_RLIMIT_NICE_Const(ph7_value *pVal,void *pUserData)` |
|      40 | 2210 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_NICE); }` |
|       - | 2211 | `#endif` |
|       - | 2212 | `#ifdef RLIMIT_NOFILE` |
|      80 | 2213 | `static void PH7_POSIX_RLIMIT_NOFILE_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2214 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_NOFILE); }` |
|       - | 2215 | `#endif` |
|       - | 2216 | `#ifdef RLIMIT_NPROC` |
|      80 | 2217 | `static void PH7_POSIX_RLIMIT_NPROC_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2218 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_NPROC); }` |
|       - | 2219 | `#endif` |
|       - | 2220 | `#ifdef RLIMIT_RSS` |
|      80 | 2221 | `static void PH7_POSIX_RLIMIT_RSS_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2222 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_RSS); }` |
|       - | 2223 | `#endif` |
|       - | 2224 | `#ifdef RLIMIT_RTPRIO` |
|      40 | 2225 | `static void PH7_POSIX_RLIMIT_RTPRIO_Const(ph7_value *pVal,void *pUserData)` |
|      40 | 2226 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_RTPRIO); }` |
|       - | 2227 | `#endif` |
|       - | 2228 | `#ifdef RLIMIT_RTTIME` |
|      40 | 2229 | `static void PH7_POSIX_RLIMIT_RTTIME_Const(ph7_value *pVal,void *pUserData)` |
|      40 | 2230 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_RTTIME); }` |
|       - | 2231 | `#endif` |
|       - | 2232 | `#ifdef RLIMIT_SIGPENDING` |
|      40 | 2233 | `static void PH7_POSIX_RLIMIT_SIGPENDING_Const(ph7_value *pVal,void *pUserData)` |
|      40 | 2234 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_SIGPENDING); }` |
|       - | 2235 | `#endif` |
|       - | 2236 | `#ifdef RLIMIT_STACK` |
|      80 | 2237 | `static void PH7_POSIX_RLIMIT_STACK_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2238 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)RLIMIT_STACK); }` |
|       - | 2239 | `#endif` |
|       - | 2240 | `#ifdef _SC_ARG_MAX` |
|      80 | 2241 | `static void PH7_POSIX_SC_ARG_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2242 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_ARG_MAX); }` |
|       - | 2243 | `#endif` |
|       - | 2244 | `#ifdef _SC_CHILD_MAX` |
|      80 | 2245 | `static void PH7_POSIX_SC_CHILD_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2246 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_CHILD_MAX); }` |
|       - | 2247 | `#endif` |
|       - | 2248 | `#ifdef _SC_CLK_TCK` |
|      81 | 2249 | `static void PH7_POSIX_SC_CLK_TCK_Const(ph7_value *pVal,void *pUserData)` |
|      81 | 2250 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_CLK_TCK); }` |
|       - | 2251 | `#endif` |
|       - | 2252 | `#ifdef _SC_OPEN_MAX` |
|      80 | 2253 | `static void PH7_POSIX_SC_OPEN_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2254 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_OPEN_MAX); }` |
|       - | 2255 | `#endif` |
|       - | 2256 | `#ifdef _SC_PAGESIZE` |
|      81 | 2257 | `static void PH7_POSIX_SC_PAGESIZE_Const(ph7_value *pVal,void *pUserData)` |
|      81 | 2258 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_PAGESIZE); }` |
|       - | 2259 | `#endif` |
|       - | 2260 | `#ifdef _SC_NPROCESSORS_CONF` |
|      80 | 2261 | `static void PH7_POSIX_SC_NPROCESSORS_CONF_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2262 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_NPROCESSORS_CONF); }` |
|       - | 2263 | `#endif` |
|       - | 2264 | `#ifdef _SC_NPROCESSORS_ONLN` |
|      80 | 2265 | `static void PH7_POSIX_SC_NPROCESSORS_ONLN_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2266 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_SC_NPROCESSORS_ONLN); }` |
|       - | 2267 | `#endif` |
|       - | 2268 | `#ifdef _PC_LINK_MAX` |
|      80 | 2269 | `static void PH7_POSIX_PC_LINK_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2270 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_LINK_MAX); }` |
|       - | 2271 | `#endif` |
|       - | 2272 | `#ifdef _PC_MAX_CANON` |
|      80 | 2273 | `static void PH7_POSIX_PC_MAX_CANON_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2274 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_MAX_CANON); }` |
|       - | 2275 | `#endif` |
|       - | 2276 | `#ifdef _PC_MAX_INPUT` |
|      80 | 2277 | `static void PH7_POSIX_PC_MAX_INPUT_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2278 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_MAX_INPUT); }` |
|       - | 2279 | `#endif` |
|       - | 2280 | `#ifdef _PC_NAME_MAX` |
|      86 | 2281 | `static void PH7_POSIX_PC_NAME_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      86 | 2282 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_NAME_MAX); }` |
|       - | 2283 | `#endif` |
|       - | 2284 | `#ifdef _PC_PATH_MAX` |
|      82 | 2285 | `static void PH7_POSIX_PC_PATH_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      82 | 2286 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_PATH_MAX); }` |
|       - | 2287 | `#endif` |
|       - | 2288 | `#ifdef _PC_PIPE_BUF` |
|      80 | 2289 | `static void PH7_POSIX_PC_PIPE_BUF_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2290 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_PIPE_BUF); }` |
|       - | 2291 | `#endif` |
|       - | 2292 | `#ifdef _PC_CHOWN_RESTRICTED` |
|      80 | 2293 | `static void PH7_POSIX_PC_CHOWN_RESTRICTED_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2294 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_CHOWN_RESTRICTED); }` |
|       - | 2295 | `#endif` |
|       - | 2296 | `#ifdef _PC_NO_TRUNC` |
|      80 | 2297 | `static void PH7_POSIX_PC_NO_TRUNC_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2298 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_NO_TRUNC); }` |
|       - | 2299 | `#endif` |
|       - | 2300 | `#ifdef _PC_ALLOC_SIZE_MIN` |
|      80 | 2301 | `static void PH7_POSIX_PC_ALLOC_SIZE_MIN_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2302 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_ALLOC_SIZE_MIN); }` |
|       - | 2303 | `#endif` |
|       - | 2304 | `#ifdef _PC_SYMLINK_MAX` |
|      80 | 2305 | `static void PH7_POSIX_PC_SYMLINK_MAX_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2306 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,(ph7_int64)_PC_SYMLINK_MAX); }` |
|       - | 2307 | `#endif` |
|       - | 2308 | `/*` |
|       - | 2309 | ` * RLIM_INFINITY is a rlim_t, which is unsigned; php answers it as -1, which is` |
|       - | 2310 | ` * what posix_setrlimit() takes back for "no limit".` |
|       - | 2311 | ` */` |
|      80 | 2312 | `static void PH7_POSIX_RLIMIT_INFINITY_Const(ph7_value *pVal,void *pUserData)` |
|      80 | 2313 | `{ SXUNUSED(pUserData); ph7_value_int64(pVal,-1); }` |
|       - | 2314 | `#endif /* __WINNT__ */` |
|       - | 2315 | `/*` |
|       - | 2316 | ` * SEEK_SET.` |
|       - | 2317 | ` *  Expand 0` |
|       - | 2318 | ` */` |
|     107 | 2319 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2320 | `{` |
|      53 | 2321 | `	SXUNUSED(pUserData); /* cc warning */` |
|     112 | 2322 | `	ph7_value_int(pVal,0);` |
|     112 | 2323 | `}` |
|       - | 2324 | `/*` |
|       - | 2325 | ` * SEEK_CUR.` |
|       - | 2326 | ` *  Expand 1` |
|       - | 2327 | ` */` |
|     101 | 2328 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2329 | `{` |
|      50 | 2330 | `	SXUNUSED(pUserData); /* cc warning */` |
|     106 | 2331 | `	ph7_value_int(pVal,1);` |
|     106 | 2332 | `}` |
|       - | 2333 | `/*` |
|       - | 2334 | ` * SEEK_END.` |
|       - | 2335 | ` *  Expand 2` |
|       - | 2336 | ` */` |
|     103 | 2337 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2338 | `{` |
|      51 | 2339 | `	SXUNUSED(pUserData); /* cc warning */` |
|     108 | 2340 | `	ph7_value_int(pVal,2);` |
|     108 | 2341 | `}` |
|       - | 2342 | `/*` |
|       - | 2343 | ` * LOCK_SH.` |
|       - | 2344 | ` *  Expand 2` |
|       - | 2345 | ` */` |
|      97 | 2346 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2347 | `{` |
|      48 | 2348 | `	SXUNUSED(pUserData); /* cc warning */` |
|     102 | 2349 | `	ph7_value_int(pVal,1);` |
|     102 | 2350 | `}` |
|       - | 2351 | `/*` |
|       - | 2352 | ` * LOCK_NB.` |
|       - | 2353 | ` *  Expand 4 (php)` |
|       - | 2354 | ` */` |
|     101 | 2355 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2356 | `{` |
|      50 | 2357 | `	SXUNUSED(pUserData); /* cc warning */` |
|     106 | 2358 | `	ph7_value_int(pVal,4);` |
|     106 | 2359 | `}` |
|       - | 2360 | `/*` |
|       - | 2361 | ` * LOCK_EX.` |
|       - | 2362 | ` *  Expand 2 (php). PH7 used 1, which collided with LOCK_SH, and LOCK_UN was 0 — so` |
|       - | 2363 | ` *  flock($h, LOCK_UN) asked the stream for a SHARED lock instead of releasing one.` |
|       - | 2364 | ` */` |
|      99 | 2365 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2366 | `{` |
|      49 | 2367 | `	SXUNUSED(pUserData); /* cc warning */` |
|     104 | 2368 | `	ph7_value_int(pVal,2);` |
|     104 | 2369 | `}` |
|       - | 2370 | `/*` |
|       - | 2371 | ` * LOCK_UN.` |
|       - | 2372 | ` *  Expand 3 (php)` |
|       - | 2373 | ` */` |
|      93 | 2374 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2375 | `{` |
|      46 | 2376 | `	SXUNUSED(pUserData); /* cc warning */` |
|      98 | 2377 | `	ph7_value_int(pVal,3);` |
|      98 | 2378 | `}` |
|       - | 2379 | `/*` |
|       - | 2380 | ` * FILE_USE_INCLUDE_PATH` |
|       - | 2381 | ` *  Expand 0x01 (Must be a power of two)` |
|       - | 2382 | ` */` |
|      85 | 2383 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2384 | `{` |
|      42 | 2385 | `	SXUNUSED(pUserData); /* cc warning */` |
|      90 | 2386 | `	ph7_value_int(pVal,0x1);` |
|      90 | 2387 | `}` |
|       - | 2388 | `/*` |
|       - | 2389 | ` * FILE_IGNORE_NEW_LINES` |
|       - | 2390 | ` *  Expand 0x02 (Must be a power of two)` |
|       - | 2391 | ` */` |
|     205 | 2392 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2393 | `{` |
|     102 | 2394 | `	SXUNUSED(pUserData); /* cc warning */` |
|     210 | 2395 | `	ph7_value_int(pVal,0x2);` |
|     210 | 2396 | `}` |
|       - | 2397 | `/*` |
|       - | 2398 | ` * FILE_SKIP_EMPTY_LINES` |
|       - | 2399 | ` *  Expand 0x04 (Must be a power of two)` |
|       - | 2400 | ` */` |
|     197 | 2401 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2402 | `{` |
|      98 | 2403 | `	SXUNUSED(pUserData); /* cc warning */` |
|     202 | 2404 | `	ph7_value_int(pVal,0x4);` |
|     202 | 2405 | `}` |
|       - | 2406 | `/*` |
|       - | 2407 | ` * FILE_APPEND` |
|       - | 2408 | ` *  Expand 0x08 (Must be a power of two)` |
|       - | 2409 | ` */` |
|      87 | 2410 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2411 | `{` |
|      43 | 2412 | `	SXUNUSED(pUserData); /* cc warning */` |
|      92 | 2413 | `	ph7_value_int(pVal,0x08);` |
|      92 | 2414 | `}` |
|       - | 2415 | `/*` |
|       - | 2416 | ` * FILE_NO_DEFAULT_CONTEXT` |
|       - | 2417 | ` *  Expand php's 0x10. file()/file_put_contents() read it: it is what stops the` |
|       - | 2418 | `` *  `$context = null` argument from resolving to stream_context_get_default()'s`` |
|       - | 2419 | ` *  context. What a device then does with an open carrying no context is its own` |
|       - | 2420 | ` *  business — a userland wrapper's $this->context is a resource either way, in` |
|       - | 2421 | ` *  php as here — so the flag is only observable where an option is consumed.` |
|       - | 2422 | ` */` |
|      89 | 2423 | `static void PH7_FILE_NO_DEFAULT_CONTEXT_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2424 | `{` |
|      44 | 2425 | `	SXUNUSED(pUserData); /* cc warning */` |
|      94 | 2426 | `	ph7_value_int(pVal,0x10);` |
|      94 | 2427 | `}` |
|       - | 2428 | `/*` |
|       - | 2429 | ` * SCANDIR_SORT_ASCENDING` |
|       - | 2430 | ` *  Expand 0` |
|       - | 2431 | ` */` |
|    2997 | 2432 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2433 | `{` |
|    1494 | 2434 | `	SXUNUSED(pUserData); /* cc warning */` |
|    3002 | 2435 | `	ph7_value_int(pVal,0);` |
|    3002 | 2436 | `}` |
|       - | 2437 | `/*` |
|       - | 2438 | ` * SCANDIR_SORT_DESCENDING` |
|       - | 2439 | ` *  Expand 1` |
|       - | 2440 | ` */` |
|      89 | 2441 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2442 | `{` |
|      44 | 2443 | `	SXUNUSED(pUserData); /* cc warning */` |
|      94 | 2444 | `	ph7_value_int(pVal,1);` |
|      94 | 2445 | `}` |
|       - | 2446 | `/*` |
|       - | 2447 | ` * SCANDIR_SORT_NONE` |
|       - | 2448 | ` *  Expand 2` |
|       - | 2449 | ` */` |
|    1547 | 2450 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2451 | `{` |
|     771 | 2452 | `	SXUNUSED(pUserData); /* cc warning */` |
|    1552 | 2453 | `	ph7_value_int(pVal,2);` |
|    1552 | 2454 | `}` |
|       - | 2455 | `/*` |
|       - | 2456 | ` * GLOB_MARK` |
|       - | 2457 | ` *  Expand php's 0x08 (php's own portable glob flag set)` |
|       - | 2458 | ` */` |
|     399 | 2459 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2460 | `{` |
|     199 | 2461 | `	SXUNUSED(pUserData); /* cc warning */` |
|     404 | 2462 | `	ph7_value_int(pVal,PH7_GLOB_MARK);` |
|     404 | 2463 | `}` |
|       - | 2464 | `/*` |
|       - | 2465 | ` * GLOB_NOSORT` |
|       - | 2466 | ` *  Expand php's 0x20` |
|       - | 2467 | ` */` |
|     586 | 2468 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2469 | `{` |
|     292 | 2470 | `	SXUNUSED(pUserData); /* cc warning */` |
|     591 | 2471 | `	ph7_value_int(pVal,PH7_GLOB_NOSORT);` |
|     591 | 2472 | `}` |
|       - | 2473 | `/*` |
|       - | 2474 | ` * GLOB_NOCHECK` |
|       - | 2475 | ` *  Expand php's 0x10` |
|       - | 2476 | ` */` |
|     734 | 2477 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2478 | `{` |
|     366 | 2479 | `	SXUNUSED(pUserData); /* cc warning */` |
|     739 | 2480 | `	ph7_value_int(pVal,PH7_GLOB_NOCHECK);` |
|     739 | 2481 | `}` |
|       - | 2482 | `/*` |
|       - | 2483 | ` * GLOB_NOESCAPE` |
|       - | 2484 | ` *  Expand php's 0x1000` |
|       - | 2485 | ` */` |
|      87 | 2486 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2487 | `{` |
|      43 | 2488 | `	SXUNUSED(pUserData); /* cc warning */` |
|      92 | 2489 | `	ph7_value_int(pVal,PH7_GLOB_NOESCAPE);` |
|      92 | 2490 | `}` |
|       - | 2491 | `/*` |
|       - | 2492 | ` * GLOB_BRACE` |
|       - | 2493 | ` *  Expand php's 0x80` |
|       - | 2494 | ` */` |
|     606 | 2495 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2496 | `{` |
|     302 | 2497 | `	SXUNUSED(pUserData); /* cc warning */` |
|     611 | 2498 | `	ph7_value_int(pVal,PH7_GLOB_BRACE);` |
|     611 | 2499 | `}` |
|       - | 2500 | `/*` |
|       - | 2501 | ` * GLOB_ONLYDIR` |
|       - | 2502 | ` *  Expand php's 0x40000000` |
|       - | 2503 | ` */` |
|     897 | 2504 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2505 | `{` |
|     446 | 2506 | `	SXUNUSED(pUserData); /* cc warning */` |
|     902 | 2507 | `	ph7_value_int(pVal,PH7_GLOB_ONLYDIR);` |
|     902 | 2508 | `}` |
|       - | 2509 | `/*` |
|       - | 2510 | ` * GLOB_ERR` |
|       - | 2511 | ` *  Expand php's 0x04` |
|       - | 2512 | ` */` |
|      91 | 2513 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2514 | `{` |
|      45 | 2515 | `	SXUNUSED(pUserData); /* cc warning */` |
|      96 | 2516 | `	ph7_value_int(pVal,PH7_GLOB_ERR);` |
|      96 | 2517 | `}` |
|       - | 2518 | `/*` |
|       - | 2519 | ` * GLOB_AVAILABLE_FLAGS` |
|       - | 2520 | ` *  Expand the OR of every glob flag php's portable glob accepts — the mask` |
|       - | 2521 | ` *  glob() itself validates against (1073746108 on every platform, since the` |
|       - | 2522 | ` *  GLOB_* values are php 8.5's own portable set).` |
|       - | 2523 | ` */` |
|     602 | 2524 | `static void PH7_GLOB_AVAILABLE_FLAGS_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2525 | `{` |
|     300 | 2526 | `	SXUNUSED(pUserData); /* cc warning */` |
|     607 | 2527 | `	ph7_value_int(pVal,PH7_GLOB_ERR\|PH7_GLOB_MARK\|PH7_GLOB_NOCHECK\|PH7_GLOB_NOSORT` |
|       - | 2528 | `		\|PH7_GLOB_BRACE\|PH7_GLOB_NOESCAPE\|PH7_GLOB_ONLYDIR);` |
|     607 | 2529 | `}` |
|       - | 2530 | `/*` |
|       - | 2531 | ` * STDIN` |
|       - | 2532 | ` *  Expand the STDIN handle as a resource.` |
|       - | 2533 | ` */` |
|     186 | 2534 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2535 | `{` |
|     191 | 2536 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 2537 | `	void *pResource;` |
|     191 | 2538 | `	pResource = PH7_ExportStdin(pVm);` |
|     191 | 2539 | `	ph7_value_resource(pVal,pResource);` |
|     191 | 2540 | `}` |
|       - | 2541 | `/*` |
|       - | 2542 | ` * STDOUT` |
|       - | 2543 | ` *   Expand the STDOUT handle as a resource.` |
|       - | 2544 | ` */` |
|     177 | 2545 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2546 | `{` |
|     182 | 2547 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 2548 | `	void *pResource;` |
|     182 | 2549 | `	pResource = PH7_ExportStdout(pVm);` |
|     182 | 2550 | `	ph7_value_resource(pVal,pResource);` |
|     182 | 2551 | `}` |
|       - | 2552 | `/*` |
|       - | 2553 | ` * STDERR` |
|       - | 2554 | ` *  Expand the STDERR handle as a resource.` |
|       - | 2555 | ` */` |
|     219 | 2556 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2557 | `{` |
|     224 | 2558 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 2559 | `	void *pResource;` |
|     224 | 2560 | `	pResource = PH7_ExportStderr(pVm);` |
|     224 | 2561 | `	ph7_value_resource(pVal,pResource);` |
|     224 | 2562 | `}` |
|       - | 2563 | `/*` |
|       - | 2564 | ` * INI_SCANNER_NORMAL` |
|       - | 2565 | ` *   Expand php's 0` |
|       - | 2566 | ` */` |
|     409 | 2567 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2568 | `{` |
|     204 | 2569 | `	SXUNUSED(pUserData); /* cc warning */` |
|     414 | 2570 | `	ph7_value_int(pVal,PH7_INI_SCANNER_NORMAL);` |
|     414 | 2571 | `}` |
|       - | 2572 | `/*` |
|       - | 2573 | ` * INI_SCANNER_RAW` |
|       - | 2574 | ` *   Expand php's 1` |
|       - | 2575 | ` */` |
|     101 | 2576 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2577 | `{` |
|      50 | 2578 | `	SXUNUSED(pUserData); /* cc warning */` |
|     106 | 2579 | `	ph7_value_int(pVal,PH7_INI_SCANNER_RAW);` |
|     106 | 2580 | `}` |
|       - | 2581 | `/*` |
|       - | 2582 | ` * INI_SCANNER_TYPED` |
|       - | 2583 | ` *   Expand 2 (php's value)` |
|       - | 2584 | ` */` |
|     205 | 2585 | `static void PH7_INI_SCANNER_TYPED_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2586 | `{` |
|     102 | 2587 | `	SXUNUSED(pUserData); /* cc warning */` |
|     210 | 2588 | `	ph7_value_int(pVal,PH7_INI_SCANNER_TYPED);` |
|     210 | 2589 | `}` |
|       - | 2590 | `/*` |
|       - | 2591 | ` * EXTR_OVERWRITE` |
|       - | 2592 | ` *   Expand 0 (php's enum value; see PH7_EXTR_* in ph7int.h)` |
|       - | 2593 | ` */` |
|      95 | 2594 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2595 | `{` |
|      47 | 2596 | `	SXUNUSED(pUserData); /* cc warning */` |
|     100 | 2597 | `	ph7_value_int(pVal,PH7_EXTR_OVERWRITE);` |
|     100 | 2598 | `}` |
|       - | 2599 | `/*` |
|       - | 2600 | ` * EXTR_SKIP` |
|       - | 2601 | ` *   Expand 1` |
|       - | 2602 | ` */` |
|      97 | 2603 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2604 | `{` |
|      48 | 2605 | `	SXUNUSED(pUserData); /* cc warning */` |
|     102 | 2606 | `	ph7_value_int(pVal,PH7_EXTR_SKIP);` |
|     102 | 2607 | `}` |
|       - | 2608 | `/*` |
|       - | 2609 | ` * EXTR_PREFIX_SAME` |
|       - | 2610 | ` *   Expand 2` |
|       - | 2611 | ` */` |
|     119 | 2612 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2613 | `{` |
|      59 | 2614 | `	SXUNUSED(pUserData); /* cc warning */` |
|     124 | 2615 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_SAME);` |
|     124 | 2616 | `}` |
|       - | 2617 | `/*` |
|       - | 2618 | ` * EXTR_PREFIX_ALL` |
|       - | 2619 | ` *   Expand 3` |
|       - | 2620 | ` */` |
|     109 | 2621 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2622 | `{` |
|      54 | 2623 | `	SXUNUSED(pUserData); /* cc warning */` |
|     114 | 2624 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_ALL);` |
|     114 | 2625 | `}` |
|       - | 2626 | `/*` |
|       - | 2627 | ` * EXTR_PREFIX_INVALID` |
|       - | 2628 | ` *   Expand 4` |
|       - | 2629 | ` */` |
|      89 | 2630 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2631 | `{` |
|      44 | 2632 | `	SXUNUSED(pUserData); /* cc warning */` |
|      94 | 2633 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_INVALID);` |
|      94 | 2634 | `}` |
|       - | 2635 | `/*` |
|       - | 2636 | ` * EXTR_IF_EXISTS` |
|       - | 2637 | ` *   Expand 6 (php orders IF_EXISTS after PREFIX_IF_EXISTS)` |
|       - | 2638 | ` */` |
|      97 | 2639 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2640 | `{` |
|      48 | 2641 | `	SXUNUSED(pUserData); /* cc warning */` |
|     102 | 2642 | `	ph7_value_int(pVal,PH7_EXTR_IF_EXISTS);` |
|     102 | 2643 | `}` |
|       - | 2644 | `/*` |
|       - | 2645 | ` * EXTR_REFS` |
|       - | 2646 | ` *   Expand 256 (the bit that rides above the mode: bind by REFERENCE)` |
|       - | 2647 | ` */` |
|      95 | 2648 | `static void PH7_EXTR_REFS_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2649 | `{` |
|      47 | 2650 | `	SXUNUSED(pUserData); /* cc warning */` |
|     100 | 2651 | `	ph7_value_int(pVal,PH7_EXTR_REFS);` |
|     100 | 2652 | `}` |
|       - | 2653 | `/*` |
|       - | 2654 | ` * EXTR_PREFIX_IF_EXISTS` |
|       - | 2655 | ` *   Expand 5` |
|       - | 2656 | ` */` |
|     101 | 2657 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2658 | `{` |
|      50 | 2659 | `	SXUNUSED(pUserData); /* cc warning */` |
|     106 | 2660 | `	ph7_value_int(pVal,PH7_EXTR_PREFIX_IF_EXISTS);` |
|     106 | 2661 | `}` |
|       - | 2662 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|       - | 2663 | `/*` |
|       - | 2664 | ` * HASH_HMAC.` |
|       - | 2665 | ` *   php's one hash_init() flag. Declared with the hash extension it belongs` |
|       - | 2666 | ` *   to, so a build without that extension has no constant either.` |
|       - | 2667 | ` */` |
|      92 | 2668 | `static void PH7_HASH_HMAC_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2669 | `{` |
|      46 | 2670 | `	SXUNUSED(pUserData); /* cc warning */` |
|      97 | 2671 | `	ph7_value_int(pVal,PH7_HASH_HMAC);` |
|      97 | 2672 | `}` |
|       - | 2673 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|       - | 2674 | `/*` |
|       - | 2675 | ` * ICONV_* — what the converter IS, and iconv_mime_decode()'s $mode bits.` |
|       - | 2676 | `` * php reports the C library behind its extension here (`glibc`, `libiconv`);`` |
|       - | 2677 | ` * PHL converts with its own code so that a Windows build answers what a POSIX` |
|       - | 2678 | ` * one does, and says so — the constants exist to be READ, and a program that` |
|       - | 2679 | ` * branches on them has to see something true.` |
|       - | 2680 | ` */` |
|      82 | 2681 | `static void PH7_ICONV_IMPL_Const(ph7_value *pVal,void *pUnused)` |
|       5 | 2682 | `{` |
|      41 | 2683 | `	SXUNUSED(pUnused);` |
|      87 | 2684 | `	ph7_value_string(pVal,"PHL",(int)sizeof("PHL")-1);` |
|      87 | 2685 | `}` |
|      82 | 2686 | `static void PH7_ICONV_VERSION_Const(ph7_value *pVal,void *pUnused)` |
|       5 | 2687 | `{` |
|      41 | 2688 | `	SXUNUSED(pUnused);` |
|      87 | 2689 | `	ph7_value_string(pVal,PH7_VERSION,(int)sizeof(PH7_VERSION)-1);` |
|      87 | 2690 | `}` |
|      84 | 2691 | `static void PH7_ICONV_MIME_DECODE_STRICT_Const(ph7_value *pVal,void *pUnused)` |
|       5 | 2692 | `{` |
|      42 | 2693 | `	SXUNUSED(pUnused);` |
|      89 | 2694 | `	ph7_value_int(pVal,1);` |
|      89 | 2695 | `}` |
|      84 | 2696 | `static void PH7_ICONV_MIME_DECODE_CONTINUE_ON_ERROR_Const(ph7_value *pVal,void *pUnused)` |
|       5 | 2697 | `{` |
|      42 | 2698 | `	SXUNUSED(pUnused);` |
|      89 | 2699 | `	ph7_value_int(pVal,2);` |
|      89 | 2700 | `}` |
|       - | 2701 | `/*` |
|       - | 2702 | ` * JSON_HEX_TAG.` |
|       - | 2703 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|       - | 2704 | ` */` |
|      94 | 2705 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2706 | `{` |
|      47 | 2707 | `	SXUNUSED(pUserData); /* cc warning */` |
|      99 | 2708 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      99 | 2709 | `}` |
|       - | 2710 | `/*` |
|       - | 2711 | ` * JSON_HEX_AMP.` |
|       - | 2712 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|       - | 2713 | ` */` |
|      92 | 2714 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2715 | `{` |
|      46 | 2716 | `	SXUNUSED(pUserData); /* cc warning */` |
|      97 | 2717 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      97 | 2718 | `}` |
|       - | 2719 | `/*` |
|       - | 2720 | ` * JSON_HEX_APOS.` |
|       - | 2721 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|       - | 2722 | ` */` |
|      92 | 2723 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2724 | `{` |
|      46 | 2725 | `	SXUNUSED(pUserData); /* cc warning */` |
|      97 | 2726 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      97 | 2727 | `}` |
|       - | 2728 | `/*` |
|       - | 2729 | ` * JSON_HEX_QUOT.` |
|       - | 2730 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|       - | 2731 | ` */` |
|      92 | 2732 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2733 | `{` |
|      46 | 2734 | `	SXUNUSED(pUserData); /* cc warning */` |
|      97 | 2735 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      97 | 2736 | `}` |
|       - | 2737 | `/*` |
|       - | 2738 | ` * JSON_FORCE_OBJECT.` |
|       - | 2739 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|       - | 2740 | ` */` |
|      92 | 2741 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2742 | `{` |
|      46 | 2743 | `	SXUNUSED(pUserData); /* cc warning */` |
|      97 | 2744 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      97 | 2745 | `}` |
|       - | 2746 | `/*` |
|       - | 2747 | ` * JSON_NUMERIC_CHECK.` |
|       - | 2748 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|       - | 2749 | ` */` |
|      96 | 2750 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2751 | `{` |
|      48 | 2752 | `	SXUNUSED(pUserData); /* cc warning */` |
|     101 | 2753 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|     101 | 2754 | `}` |
|       - | 2755 | `/*` |
|       - | 2756 | ` * JSON_BIGINT_AS_STRING.` |
|       - | 2757 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|       - | 2758 | ` */` |
|     100 | 2759 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2760 | `{` |
|      50 | 2761 | `	SXUNUSED(pUserData); /* cc warning */` |
|     105 | 2762 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|     105 | 2763 | `}` |
|       - | 2764 | `/*` |
|       - | 2765 | ` * JSON_PARTIAL_OUTPUT_ON_ERROR.` |
|       - | 2766 | ` *   Expand the value of JSON_PARTIAL_OUTPUT_ON_ERROR defined in ph7Int.h.` |
|       - | 2767 | ` */` |
|     110 | 2768 | `static void PH7_JSON_PARTIAL_OUTPUT_ON_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2769 | `{` |
|      55 | 2770 | `	SXUNUSED(pUserData); /* cc warning */` |
|     115 | 2771 | `	ph7_value_int(pVal,JSON_PARTIAL_OUTPUT_ON_ERROR);` |
|     115 | 2772 | `}` |
|       - | 2773 | `/*` |
|       - | 2774 | ` * JSON_PRESERVE_ZERO_FRACTION.` |
|       - | 2775 | ` *   Expand the value of JSON_PRESERVE_ZERO_FRACTION defined in ph7Int.h.` |
|       - | 2776 | ` */` |
|     100 | 2777 | `static void PH7_JSON_PRESERVE_ZERO_FRACTION_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2778 | `{` |
|      50 | 2779 | `	SXUNUSED(pUserData); /* cc warning */` |
|     105 | 2780 | `	ph7_value_int(pVal,JSON_PRESERVE_ZERO_FRACTION);` |
|     105 | 2781 | `}` |
|       - | 2782 | `/*` |
|       - | 2783 | ` * JSON_OBJECT_AS_ARRAY.` |
|       - | 2784 | ` *   Expand the value of JSON_OBJECT_AS_ARRAY defined in ph7Int.h.` |
|       - | 2785 | ` */` |
|      96 | 2786 | `static void PH7_JSON_OBJECT_AS_ARRAY_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2787 | `{` |
|      48 | 2788 | `	SXUNUSED(pUserData); /* cc warning */` |
|     101 | 2789 | `	ph7_value_int(pVal,JSON_OBJECT_AS_ARRAY);` |
|     101 | 2790 | `}` |
|       - | 2791 | `/*` |
|       - | 2792 | ` * JSON_PRETTY_PRINT.` |
|       - | 2793 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|       - | 2794 | ` */` |
|     100 | 2795 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2796 | `{` |
|      50 | 2797 | `	SXUNUSED(pUserData); /* cc warning */` |
|     105 | 2798 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|     105 | 2799 | `}` |
|       - | 2800 | `/*` |
|       - | 2801 | ` * JSON_UNESCAPED_SLASHES.` |
|       - | 2802 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|       - | 2803 | ` */` |
|      96 | 2804 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2805 | `{` |
|      48 | 2806 | `	SXUNUSED(pUserData); /* cc warning */` |
|     101 | 2807 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|     101 | 2808 | `}` |
|       - | 2809 | `/*` |
|       - | 2810 | ` * JSON_UNESCAPED_UNICODE.` |
|       - | 2811 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|       - | 2812 | ` */` |
|     602 | 2813 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2814 | `{` |
|     301 | 2815 | `	SXUNUSED(pUserData); /* cc warning */` |
|     607 | 2816 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|     607 | 2817 | `}` |
|       - | 2818 | `/*` |
|       - | 2819 | ` * JSON_UNESCAPED_LINE_TERMINATORS.` |
|       - | 2820 | ` *   Expand the value of JSON_UNESCAPED_LINE_TERMINATORS defined in ph7Int.h.` |
|       - | 2821 | ` */` |
|      92 | 2822 | `static void PH7_JSON_UNESCAPED_LINE_TERMINATORS_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2823 | `{` |
|      46 | 2824 | `	SXUNUSED(pUserData); /* cc warning */` |
|      97 | 2825 | `	ph7_value_int(pVal,JSON_UNESCAPED_LINE_TERMINATORS);` |
|      97 | 2826 | `}` |
|       - | 2827 | `/*` |
|       - | 2828 | ` * JSON_INVALID_UTF8_IGNORE.` |
|       - | 2829 | ` *   Expand the value of JSON_INVALID_UTF8_IGNORE defined in ph7Int.h.` |
|       - | 2830 | ` */` |
|     112 | 2831 | `static void PH7_JSON_INVALID_UTF8_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2832 | `{` |
|      56 | 2833 | `	SXUNUSED(pUserData); /* cc warning */` |
|     117 | 2834 | `	ph7_value_int(pVal,JSON_INVALID_UTF8_IGNORE);` |
|     117 | 2835 | `}` |
|       - | 2836 | `/*` |
|       - | 2837 | ` * JSON_INVALID_UTF8_SUBSTITUTE.` |
|       - | 2838 | ` *   Expand the value of JSON_INVALID_UTF8_SUBSTITUTE defined in ph7Int.h.` |
|       - | 2839 | ` */` |
|     116 | 2840 | `static void PH7_JSON_INVALID_UTF8_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2841 | `{` |
|      58 | 2842 | `	SXUNUSED(pUserData); /* cc warning */` |
|     121 | 2843 | `	ph7_value_int(pVal,JSON_INVALID_UTF8_SUBSTITUTE);` |
|     121 | 2844 | `}` |
|       - | 2845 | `/*` |
|       - | 2846 | ` * JSON_THROW_ON_ERROR.` |
|       - | 2847 | ` *   Expand the value of JSON_THROW_ON_ERROR defined in ph7Int.h.` |
|       - | 2848 | ` */` |
|     108 | 2849 | `static void PH7_JSON_THROW_ON_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2850 | `{` |
|      54 | 2851 | `	SXUNUSED(pUserData); /* cc warning */` |
|     113 | 2852 | `	ph7_value_int(pVal,JSON_THROW_ON_ERROR);` |
|     113 | 2853 | `}` |
|       - | 2854 | `/*` |
|       - | 2855 | ` * JSON_ERROR_NONE.` |
|       - | 2856 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|       - | 2857 | ` */` |
|      90 | 2858 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2859 | `{` |
|      45 | 2860 | `	SXUNUSED(pUserData); /* cc warning */` |
|      95 | 2861 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      95 | 2862 | `}` |
|       - | 2863 | `/*` |
|       - | 2864 | ` * JSON_ERROR_DEPTH.` |
|       - | 2865 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|       - | 2866 | ` */` |
|      88 | 2867 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2868 | `{` |
|      44 | 2869 | `	SXUNUSED(pUserData); /* cc warning */` |
|      93 | 2870 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      93 | 2871 | `}` |
|       - | 2872 | `/*` |
|       - | 2873 | ` * JSON_ERROR_STATE_MISMATCH.` |
|       - | 2874 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|       - | 2875 | ` */` |
|      88 | 2876 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2877 | `{` |
|      44 | 2878 | `	SXUNUSED(pUserData); /* cc warning */` |
|      93 | 2879 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      93 | 2880 | `}` |
|       - | 2881 | `/*` |
|       - | 2882 | ` * JSON_ERROR_CTRL_CHAR.` |
|       - | 2883 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|       - | 2884 | ` */` |
|      88 | 2885 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2886 | `{` |
|      44 | 2887 | `	SXUNUSED(pUserData); /* cc warning */` |
|      93 | 2888 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      93 | 2889 | `}` |
|       - | 2890 | `/*` |
|       - | 2891 | ` * JSON_ERROR_SYNTAX.` |
|       - | 2892 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|       - | 2893 | ` */` |
|      90 | 2894 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2895 | `{` |
|      45 | 2896 | `	SXUNUSED(pUserData); /* cc warning */` |
|      95 | 2897 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      95 | 2898 | `}` |
|       - | 2899 | `/*` |
|       - | 2900 | ` * JSON_ERROR_UTF8.` |
|       - | 2901 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|       - | 2902 | ` */` |
|      88 | 2903 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2904 | `{` |
|      44 | 2905 | `	SXUNUSED(pUserData); /* cc warning */` |
|      93 | 2906 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      93 | 2907 | `}` |
|       - | 2908 | `/*` |
|       - | 2909 | ` * JSON_ERROR_RECURSION.` |
|       - | 2910 | ` *   Expand the value of JSON_ERROR_RECURSION defined in ph7Int.h.` |
|       - | 2911 | ` */` |
|      88 | 2912 | `static void PH7_JSON_ERROR_RECURSION_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2913 | `{` |
|      44 | 2914 | `	SXUNUSED(pUserData); /* cc warning */` |
|      93 | 2915 | `	ph7_value_int(pVal,JSON_ERROR_RECURSION);` |
|      93 | 2916 | `}` |
|       - | 2917 | `/*` |
|       - | 2918 | ` * JSON_ERROR_UNSUPPORTED_TYPE.` |
|       - | 2919 | ` *   Expand the value of JSON_ERROR_UNSUPPORTED_TYPE defined in ph7Int.h.` |
|       - | 2920 | ` */` |
|      88 | 2921 | `static void PH7_JSON_ERROR_UNSUPPORTED_TYPE_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2922 | `{` |
|      44 | 2923 | `	SXUNUSED(pUserData); /* cc warning */` |
|      93 | 2924 | `	ph7_value_int(pVal,JSON_ERROR_UNSUPPORTED_TYPE);` |
|      93 | 2925 | `}` |
|       - | 2926 | `/*` |
|       - | 2927 | ` * JSON_ERROR_INVALID_PROPERTY_NAME.` |
|       - | 2928 | ` *   Expand the value of JSON_ERROR_INVALID_PROPERTY_NAME defined in ph7Int.h.` |
|       - | 2929 | ` */` |
|      88 | 2930 | `static void PH7_JSON_ERROR_INVALID_PROPERTY_NAME_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2931 | `{` |
|      44 | 2932 | `	SXUNUSED(pUserData); /* cc warning */` |
|      93 | 2933 | `	ph7_value_int(pVal,JSON_ERROR_INVALID_PROPERTY_NAME);` |
|      93 | 2934 | `}` |
|       - | 2935 | `/*` |
|       - | 2936 | ` * JSON_ERROR_UTF16.` |
|       - | 2937 | ` *   Expand the value of JSON_ERROR_UTF16 defined in ph7Int.h.` |
|       - | 2938 | ` */` |
|      90 | 2939 | `static void PH7_JSON_ERROR_UTF16_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2940 | `{` |
|      45 | 2941 | `	SXUNUSED(pUserData); /* cc warning */` |
|      95 | 2942 | `	ph7_value_int(pVal,JSON_ERROR_UTF16);` |
|      95 | 2943 | `}` |
|       - | 2944 | `/*` |
|       - | 2945 | ` * JSON_ERROR_NON_BACKED_ENUM.` |
|       - | 2946 | ` *   Expand the value of JSON_ERROR_NON_BACKED_ENUM defined in ph7Int.h (php 8.1).` |
|       - | 2947 | ` */` |
|      86 | 2948 | `static void PH7_JSON_ERROR_NON_BACKED_ENUM_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2949 | `{` |
|      43 | 2950 | `	SXUNUSED(pUserData); /* cc warning */` |
|      91 | 2951 | `	ph7_value_int(pVal,JSON_ERROR_NON_BACKED_ENUM);` |
|      91 | 2952 | `}` |
|       - | 2953 | `/*` |
|       - | 2954 | ` * JSON_ERROR_INF_OR_NAN.` |
|       - | 2955 | ` *   Expand the value of JSON_ERROR_INF_OR_NAN defined in ph7Int.h.` |
|       - | 2956 | ` */` |
|      88 | 2957 | `static void PH7_JSON_ERROR_INF_OR_NAN_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2958 | `{` |
|      44 | 2959 | `	SXUNUSED(pUserData); /* cc warning */` |
|      93 | 2960 | `	ph7_value_int(pVal,JSON_ERROR_INF_OR_NAN);` |
|      93 | 2961 | `}` |
|       - | 2962 | `/*` |
|       - | 2963 | ` * __CLASS__` |
|       - | 2964 | ` *  The current class name, or the EMPTY STRING outside any class — php answers "",` |
|       - | 2965 | `` *  not null (`__CLASS__ === ""` is true in global scope). `self` keeps its own`` |
|       - | 2966 | ` *  expander below because php treats IT differently outside a class scope.` |
|       - | 2967 | ` */` |
|      98 | 2968 | `static void PH7_class_magic_Const(ph7_value *pVal,void *pUserData)` |
|       5 | 2969 | `{` |
|     103 | 2970 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|       - | 2971 | `	ph7_class *pClass;` |
|       - | 2972 | `	/* php flattens a trait into the class that used it, so __CLASS__ inside a trait method` |
|       - | 2973 | `	 * is THAT class (where __TRAIT__ and __METHOD__ stay the trait's — php's own asymmetry). */` |
|     103 | 2974 | `	pClass = PH7_VmPeekSelfClass(pVm);` |
|     103 | 2975 | `	if( pClass == 0 ){` |
|      89 | 2976 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      42 | 2977 | `	}` |
|     103 | 2978 | `	if( pClass ){` |
|      15 | 2979 | `		SyString *pName = &pClass->sName;` |
|      15 | 2980 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|       8 | 2981 | `	}else{` |
|      89 | 2982 | `		ph7_value_string(pVal,"",0);` |
|       - | 2983 | `	}` |
|     103 | 2984 | `}` |
|       - | 2985 |  |
|       - | 2986 | `/*` |
|       - | 2987 | ` * PASSWORD_BCRYPT / PASSWORD_DEFAULT` |
|       - | 2988 | ` *  The bcrypt algorithm identifier (PHP 7.4+ exposes these as the string "2y").` |
|       - | 2989 | ` *  PASSWORD_DEFAULT tracks the recommended default, currently bcrypt.` |
|       - | 2990 | ` */` |
|     200 | 2991 | `static void PH7_PASSWORD_BCRYPT_Const(ph7_value *pVal,void *pUnused)` |
|       5 | 2992 | `{` |
|      99 | 2993 | `	SXUNUSED(pUnused);` |
|     205 | 2994 | `	ph7_value_string(pVal,"2y",(int)sizeof("2y")-1);` |
|     205 | 2995 | `}` |
|       - | 2996 | `/*` |
|       - | 2997 | ` * PASSWORD_BCRYPT_DEFAULT_COST` |
|       - | 2998 | ` *  The default bcrypt work factor used by password_hash() (currently 12).` |
|       - | 2999 | ` */` |
|      83 | 3000 | `static void PH7_PASSWORD_COST_Const(ph7_value *pVal,void *pUnused)` |
|       5 | 3001 | `{` |
|      41 | 3002 | `	SXUNUSED(pUnused);` |
|      88 | 3003 | `	ph7_value_int(pVal,12);` |
|      88 | 3004 | `}` |
|       - | 3005 | `/*` |
|       - | 3006 | ` * PASSWORD_ARGON2I / PASSWORD_ARGON2ID and the three argon2 option defaults` |
|       - | 3007 | ` * password_hash() reads when $options omits them.` |
|       - | 3008 | ` */` |
|      87 | 3009 | `static void PH7_PASSWORD_ARGON2I_Const(ph7_value *pVal,void *pUnused)` |
|       5 | 3010 | `{` |
|      43 | 3011 | `	SXUNUSED(pUnused);` |
|      92 | 3012 | `	ph7_value_string(pVal,"argon2i",(int)sizeof("argon2i")-1);` |
|      92 | 3013 | `}` |
|     105 | 3014 | `static void PH7_PASSWORD_ARGON2ID_Const(ph7_value *pVal,void *pUnused)` |
|       5 | 3015 | `{` |
|      52 | 3016 | `	SXUNUSED(pUnused);` |
|     110 | 3017 | `	ph7_value_string(pVal,"argon2id",(int)sizeof("argon2id")-1);` |
|     110 | 3018 | `}` |
|      83 | 3019 | `static void PH7_ARGON2_MEM_Const(ph7_value *pVal,void *pUnused)` |
|       5 | 3020 | `{` |
|      41 | 3021 | `	SXUNUSED(pUnused);` |
|      88 | 3022 | `	ph7_value_int(pVal,65536);` |
|      88 | 3023 | `}` |
|      83 | 3024 | `static void PH7_ARGON2_TIME_Const(ph7_value *pVal,void *pUnused)` |
|       5 | 3025 | `{` |
|      41 | 3026 | `	SXUNUSED(pUnused);` |
|      88 | 3027 | `	ph7_value_int(pVal,4);` |
|      88 | 3028 | `}` |
|      83 | 3029 | `static void PH7_ARGON2_THREADS_Const(ph7_value *pVal,void *pUnused)` |
|       5 | 3030 | `{` |
|      41 | 3031 | `	SXUNUSED(pUnused);` |
|      88 | 3032 | `	ph7_value_int(pVal,1);` |
|      88 | 3033 | `}` |
|       - | 3034 | `/*` |
|       - | 3035 | ` * CRYPT_* — the crypt() capability flags. Every scheme is compiled in, so all` |
|       - | 3036 | ` * six are 1, and CRYPT_SALT_LENGTH is php's 123 (the longest setting string a` |
|       - | 3037 | ` * SHA-512-crypt with an explicit rounds count can need).` |
|       - | 3038 | ` */` |
|       - | 3039 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|     498 | 3040 | `static void PH7_CRYPT_ONE_Const(ph7_value *pVal,void *pUnused)` |
|       5 | 3041 | `{` |
|     246 | 3042 | `	SXUNUSED(pUnused);` |
|     503 | 3043 | `	ph7_value_int(pVal,1);` |
|     503 | 3044 | `}` |
|      83 | 3045 | `static void PH7_CRYPT_SALT_LENGTH_Const(ph7_value *pVal,void *pUnused)` |
|       5 | 3046 | `{` |
|      41 | 3047 | `	SXUNUSED(pUnused);` |
|      88 | 3048 | `	ph7_value_int(pVal,123);` |
|      88 | 3049 | `}` |
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
|     105 | 3060 | `PH7_FILTER_INT_CONST(FILTER_DEFAULT,516)` |
|     101 | 3061 | `PH7_FILTER_INT_CONST(FILTER_UNSAFE_RAW,516)` |
|     245 | 3062 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_INT,257)` |
|     199 | 3063 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_BOOLEAN,258)` |
|     219 | 3064 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_FLOAT,259)` |
|      95 | 3065 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_REGEXP,272)` |
|     229 | 3066 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_DOMAIN,277)` |
|     405 | 3067 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_URL,273)` |
|     285 | 3068 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_EMAIL,274)` |
|     647 | 3069 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_IP,275)` |
|     119 | 3070 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_MAC,276)` |
|     111 | 3071 | `PH7_FILTER_INT_CONST(FILTER_CALLBACK,1024)` |
|     113 | 3072 | `PH7_FILTER_INT_CONST(FILTER_THROW_ON_FAILURE,268435456)` |
|      93 | 3073 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_ENCODED,514)` |
|      91 | 3074 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_ADD_SLASHES,523)` |
|      89 | 3075 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_INT,519)` |
|      89 | 3076 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_FLOAT,520)` |
|      99 | 3077 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_SPECIAL_CHARS,515)` |
|     109 | 3078 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_FULL_SPECIAL_CHARS,522)` |
|      87 | 3079 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_EMAIL,517)` |
|      87 | 3080 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_URL,518)` |
|      87 | 3081 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_OCTAL,1)` |
|      87 | 3082 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_HEX,2)` |
|      95 | 3083 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_LOW,4)` |
|      91 | 3084 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_HIGH,8)` |
|      91 | 3085 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_LOW,16)` |
|      89 | 3086 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_HIGH,32)` |
|      89 | 3087 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_AMP,64)` |
|      91 | 3088 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_ENCODE_QUOTES,128)` |
|      87 | 3089 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NONE,0)` |
|      95 | 3090 | `PH7_FILTER_INT_CONST(FILTER_FLAG_EMPTY_STRING_NULL,256)` |
|      89 | 3091 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_BACKTICK,512)` |
|      87 | 3092 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_FRACTION,4096)` |
|     121 | 3093 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_THOUSAND,8192)` |
|      87 | 3094 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_SCIENTIFIC,16384)` |
|      93 | 3095 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV4,1048576)` |
|      89 | 3096 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV6,2097152)` |
|     237 | 3097 | `PH7_FILTER_INT_CONST(FILTER_FLAG_PATH_REQUIRED,262144)` |
|     237 | 3098 | `PH7_FILTER_INT_CONST(FILTER_FLAG_QUERY_REQUIRED,524288)` |
|     149 | 3099 | `PH7_FILTER_INT_CONST(FILTER_FLAG_HOSTNAME,1048576)` |
|     169 | 3100 | `PH7_FILTER_INT_CONST(FILTER_FLAG_EMAIL_UNICODE,1048576)` |
|     297 | 3101 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_RES_RANGE,4194304)` |
|     301 | 3102 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_PRIV_RANGE,8388608)` |
|     193 | 3103 | `PH7_FILTER_INT_CONST(FILTER_FLAG_GLOBAL_RANGE,536870912)` |
|     111 | 3104 | `PH7_FILTER_INT_CONST(FILTER_REQUIRE_ARRAY,16777216)` |
|      91 | 3105 | `PH7_FILTER_INT_CONST(FILTER_REQUIRE_SCALAR,33554432)` |
|      95 | 3106 | `PH7_FILTER_INT_CONST(FILTER_FORCE_ARRAY,67108864)` |
|     107 | 3107 | `PH7_FILTER_INT_CONST(FILTER_NULL_ON_FAILURE,134217728)` |
|       - | 3108 | `/* filter_input() source selectors (php values; SESSION/REQUEST are undefined in 8.5) */` |
|      89 | 3109 | `PH7_FILTER_INT_CONST(INPUT_POST,0)` |
|      97 | 3110 | `PH7_FILTER_INT_CONST(INPUT_GET,1)` |
|      87 | 3111 | `PH7_FILTER_INT_CONST(INPUT_COOKIE,2)` |
|      87 | 3112 | `PH7_FILTER_INT_CONST(INPUT_ENV,4)` |
|     105 | 3113 | `PH7_FILTER_INT_CONST(INPUT_SERVER,5)` |
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
|      87 | 3135 | `PH7_ZLIB_INT_CONST(FORCE_GZIP,31)` |
|      87 | 3136 | `PH7_ZLIB_INT_CONST(FORCE_DEFLATE,15)` |
|     121 | 3137 | `PH7_ZLIB_INT_CONST(ZLIB_ENCODING_RAW,-15)` |
|      97 | 3138 | `PH7_ZLIB_INT_CONST(ZLIB_ENCODING_GZIP,31)` |
|      95 | 3139 | `PH7_ZLIB_INT_CONST(ZLIB_ENCODING_DEFLATE,15)` |
|      89 | 3140 | `PH7_ZLIB_INT_CONST(ZLIB_NO_FLUSH,0)` |
|      87 | 3141 | `PH7_ZLIB_INT_CONST(ZLIB_PARTIAL_FLUSH,1)` |
|      91 | 3142 | `PH7_ZLIB_INT_CONST(ZLIB_SYNC_FLUSH,2)` |
|      87 | 3143 | `PH7_ZLIB_INT_CONST(ZLIB_FULL_FLUSH,3)` |
|      87 | 3144 | `PH7_ZLIB_INT_CONST(ZLIB_BLOCK,5)` |
|      99 | 3145 | `PH7_ZLIB_INT_CONST(ZLIB_FINISH,4)` |
|      87 | 3146 | `PH7_ZLIB_INT_CONST(ZLIB_FILTERED,1)` |
|      87 | 3147 | `PH7_ZLIB_INT_CONST(ZLIB_HUFFMAN_ONLY,2)` |
|      87 | 3148 | `PH7_ZLIB_INT_CONST(ZLIB_RLE,3)` |
|      87 | 3149 | `PH7_ZLIB_INT_CONST(ZLIB_FIXED,4)` |
|      87 | 3150 | `PH7_ZLIB_INT_CONST(ZLIB_DEFAULT_STRATEGY,0)` |
|      87 | 3151 | `PH7_ZLIB_INT_CONST(ZLIB_VERNUM,ZLIB_VERNUM)` |
|      87 | 3152 | `PH7_ZLIB_INT_CONST(ZLIB_OK,0)` |
|      87 | 3153 | `PH7_ZLIB_INT_CONST(ZLIB_STREAM_END,1)` |
|      87 | 3154 | `PH7_ZLIB_INT_CONST(ZLIB_NEED_DICT,2)` |
|      87 | 3155 | `PH7_ZLIB_INT_CONST(ZLIB_ERRNO,-1)` |
|      87 | 3156 | `PH7_ZLIB_INT_CONST(ZLIB_STREAM_ERROR,-2)` |
|      87 | 3157 | `PH7_ZLIB_INT_CONST(ZLIB_DATA_ERROR,-3)` |
|      87 | 3158 | `PH7_ZLIB_INT_CONST(ZLIB_MEM_ERROR,-4)` |
|      87 | 3159 | `PH7_ZLIB_INT_CONST(ZLIB_BUF_ERROR,-5)` |
|      87 | 3160 | `PH7_ZLIB_INT_CONST(ZLIB_VERSION_ERROR,-6)` |
|      82 | 3161 | `static void PH7_ZLIB_VERSION_Const(ph7_value *pVal,void *pUnused)` |
|       5 | 3162 | `{` |
|      41 | 3163 | `	SXUNUSED(pUnused);` |
|      87 | 3164 | `	ph7_value_string(pVal,ZLIB_VERSION,-1);` |
|      87 | 3165 | `}` |
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
|      91 | 3192 | `PH7_SSL_INT_CONST(OPENSSL_VERSION_NUMBER,OPENSSL_VERSION_NUMBER)` |
|      87 | 3193 | `PH7_SSL_INT_CONST(X509_PURPOSE_SSL_CLIENT,X509_PURPOSE_SSL_CLIENT)` |
|      91 | 3194 | `PH7_SSL_INT_CONST(X509_PURPOSE_SSL_SERVER,X509_PURPOSE_SSL_SERVER)` |
|      87 | 3195 | `PH7_SSL_INT_CONST(X509_PURPOSE_NS_SSL_SERVER,X509_PURPOSE_NS_SSL_SERVER)` |
|      87 | 3196 | `PH7_SSL_INT_CONST(X509_PURPOSE_SMIME_SIGN,X509_PURPOSE_SMIME_SIGN)` |
|      87 | 3197 | `PH7_SSL_INT_CONST(X509_PURPOSE_SMIME_ENCRYPT,X509_PURPOSE_SMIME_ENCRYPT)` |
|      87 | 3198 | `PH7_SSL_INT_CONST(X509_PURPOSE_CRL_SIGN,X509_PURPOSE_CRL_SIGN)` |
|      87 | 3199 | `PH7_SSL_INT_CONST(X509_PURPOSE_ANY,X509_PURPOSE_ANY)` |
|      87 | 3200 | `PH7_SSL_INT_CONST(X509_PURPOSE_OCSP_HELPER,X509_PURPOSE_OCSP_HELPER)` |
|      87 | 3201 | `PH7_SSL_INT_CONST(X509_PURPOSE_TIMESTAMP_SIGN,X509_PURPOSE_TIMESTAMP_SIGN)` |
|      95 | 3202 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA1,1)` |
|      91 | 3203 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_MD5,2)` |
|      89 | 3204 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_MD4,3)` |
|      89 | 3205 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA224,6)` |
|      93 | 3206 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA256,7)` |
|      89 | 3207 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA384,8)` |
|      89 | 3208 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_SHA512,9)` |
|      89 | 3209 | `PH7_SSL_INT_CONST(OPENSSL_ALGO_RMD160,10)` |
|      89 | 3210 | `PH7_SSL_INT_CONST(PKCS7_DETACHED,PKCS7_DETACHED)` |
|      87 | 3211 | `PH7_SSL_INT_CONST(PKCS7_TEXT,PKCS7_TEXT)` |
|      87 | 3212 | `PH7_SSL_INT_CONST(PKCS7_NOINTERN,PKCS7_NOINTERN)` |
|      89 | 3213 | `PH7_SSL_INT_CONST(PKCS7_NOVERIFY,PKCS7_NOVERIFY)` |
|      87 | 3214 | `PH7_SSL_INT_CONST(PKCS7_NOCHAIN,PKCS7_NOCHAIN)` |
|      87 | 3215 | `PH7_SSL_INT_CONST(PKCS7_NOCERTS,PKCS7_NOCERTS)` |
|      87 | 3216 | `PH7_SSL_INT_CONST(PKCS7_NOATTR,PKCS7_NOATTR)` |
|      87 | 3217 | `PH7_SSL_INT_CONST(PKCS7_BINARY,PKCS7_BINARY)` |
|      87 | 3218 | `PH7_SSL_INT_CONST(PKCS7_NOSIGS,PKCS7_NOSIGS)` |
|      87 | 3219 | `PH7_SSL_INT_CONST(PKCS7_NOOLDMIMETYPE,PKCS7_NOOLDMIMETYPE)` |
|      87 | 3220 | `PH7_SSL_INT_CONST(PKCS7_NOSMIMECAP,PKCS7_NOSMIMECAP)` |
|      87 | 3221 | `PH7_SSL_INT_CONST(PKCS7_CRLFEOL,PKCS7_CRLFEOL)` |
|      87 | 3222 | `PH7_SSL_INT_CONST(PKCS7_NOCRL,PKCS7_NOCRL)` |
|      87 | 3223 | `PH7_SSL_INT_CONST(PKCS7_NO_DUAL_CONTENT,PKCS7_NO_DUAL_CONTENT)` |
|      87 | 3224 | `PH7_SSL_INT_CONST(OPENSSL_CMS_DETACHED,CMS_DETACHED)` |
|      87 | 3225 | `PH7_SSL_INT_CONST(OPENSSL_CMS_TEXT,CMS_TEXT)` |
|      87 | 3226 | `PH7_SSL_INT_CONST(OPENSSL_CMS_NOINTERN,CMS_NOINTERN)` |
|      89 | 3227 | `PH7_SSL_INT_CONST(OPENSSL_CMS_NOVERIFY,CMS_NO_SIGNER_CERT_VERIFY)` |
|      87 | 3228 | `PH7_SSL_INT_CONST(OPENSSL_CMS_NOCERTS,CMS_NOCERTS)` |
|      87 | 3229 | `PH7_SSL_INT_CONST(OPENSSL_CMS_NOATTR,CMS_NOATTR)` |
|      87 | 3230 | `PH7_SSL_INT_CONST(OPENSSL_CMS_BINARY,CMS_BINARY)` |
|      87 | 3231 | `PH7_SSL_INT_CONST(OPENSSL_CMS_NOSIGS,CMS_NOSIGS)` |
|      87 | 3232 | `PH7_SSL_INT_CONST(OPENSSL_CMS_OLDMIMETYPE,CMS_NOOLDMIMETYPE)` |
|      99 | 3233 | `PH7_SSL_INT_CONST(OPENSSL_PKCS1_PADDING,RSA_PKCS1_PADDING)` |
|      91 | 3234 | `PH7_SSL_INT_CONST(OPENSSL_NO_PADDING,RSA_NO_PADDING)` |
|      91 | 3235 | `PH7_SSL_INT_CONST(OPENSSL_PKCS1_OAEP_PADDING,RSA_PKCS1_OAEP_PADDING)` |
|      93 | 3236 | `PH7_SSL_INT_CONST(OPENSSL_PKCS1_PSS_PADDING,RSA_PKCS1_PSS_PADDING)` |
|      87 | 3237 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_RC2_40,0)` |
|      87 | 3238 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_RC2_128,1)` |
|      87 | 3239 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_RC2_64,2)` |
|      87 | 3240 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_DES,3)` |
|      87 | 3241 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_3DES,4)` |
|      91 | 3242 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_AES_128_CBC,5)` |
|      87 | 3243 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_AES_192_CBC,6)` |
|      87 | 3244 | `PH7_SSL_INT_CONST(OPENSSL_CIPHER_AES_256_CBC,7)` |
|      99 | 3245 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_RSA,0)` |
|      89 | 3246 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_DSA,1)` |
|      89 | 3247 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_DH,2)` |
|     101 | 3248 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_EC,3)` |
|      91 | 3249 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_X25519,4)` |
|      91 | 3250 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_ED25519,5)` |
|      91 | 3251 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_X448,6)` |
|      91 | 3252 | `PH7_SSL_INT_CONST(OPENSSL_KEYTYPE_ED448,7)` |
|     155 | 3253 | `PH7_SSL_INT_CONST(OPENSSL_RAW_DATA,1)` |
|      95 | 3254 | `PH7_SSL_INT_CONST(OPENSSL_ZERO_PADDING,2)` |
|      91 | 3255 | `PH7_SSL_INT_CONST(OPENSSL_DONT_ZERO_PAD_KEY,4)` |
|      87 | 3256 | `PH7_SSL_INT_CONST(OPENSSL_TLSEXT_SERVER_NAME,1)` |
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
|      87 | 3268 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv2_CLIENT,3)` |
|      87 | 3269 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv2_SERVER,2)` |
|      87 | 3270 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv3_CLIENT,5)` |
|      87 | 3271 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv3_SERVER,4)` |
|      87 | 3272 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv23_CLIENT,57)` |
|      87 | 3273 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_SSLv23_SERVER,120)` |
|      97 | 3274 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLS_CLIENT,121)` |
|      87 | 3275 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLS_SERVER,120)` |
|      87 | 3276 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_0_CLIENT,9)` |
|      87 | 3277 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_0_SERVER,8)` |
|      87 | 3278 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_1_CLIENT,17)` |
|      87 | 3279 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_1_SERVER,16)` |
|      87 | 3280 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_2_CLIENT,33)` |
|      87 | 3281 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_2_SERVER,32)` |
|      87 | 3282 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_3_CLIENT,65)` |
|      87 | 3283 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_TLSv1_3_SERVER,64)` |
|      87 | 3284 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_ANY_CLIENT,127)` |
|      87 | 3285 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_METHOD_ANY_SERVER,126)` |
|      87 | 3286 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_PROTO_SSLv3,4)` |
|      87 | 3287 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_PROTO_TLSv1_0,8)` |
|      87 | 3288 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_PROTO_TLSv1_1,16)` |
|      87 | 3289 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_PROTO_TLSv1_2,32)` |
|      87 | 3290 | `PH7_SSL_INT_CONST(STREAM_CRYPTO_PROTO_TLSv1_3,64)` |
|      89 | 3291 | `PH7_SSL_INT_CONST(OPENSSL_ENCODING_DER,0)` |
|      97 | 3292 | `PH7_SSL_INT_CONST(OPENSSL_ENCODING_SMIME,1)` |
|      89 | 3293 | `PH7_SSL_INT_CONST(OPENSSL_ENCODING_PEM,2)` |
|      84 | 3294 | `static void PH7_OPENSSL_VERSION_TEXT_Const(ph7_value *pVal,void *pUnused)` |
|       5 | 3295 | `{` |
|      42 | 3296 | `	SXUNUSED(pUnused);` |
|      89 | 3297 | `	ph7_value_string(pVal,OPENSSL_VERSION_TEXT,-1);` |
|      89 | 3298 | `}` |
|      86 | 3299 | `static void PH7_OPENSSL_DEFAULT_STREAM_CIPHERS_Const(ph7_value *pVal,void *pUnused)` |
|       5 | 3300 | `{` |
|      43 | 3301 | `	SXUNUSED(pUnused);` |
|      91 | 3302 | `	ph7_value_string(pVal,` |
|       - | 3303 | `		"ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:"` |
|       - | 3304 | `		"ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:"` |
|       - | 3305 | `		"DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:"` |
|       - | 3306 | `		"ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:"` |
|       - | 3307 | `		"ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:"` |
|       - | 3308 | `		"ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:"` |
|       - | 3309 | `		"DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:"` |
|       - | 3310 | `		"DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:"` |
|       - | 3311 | `		"AES128:AES256:HIGH:!SSLv2:!aNULL:!eNULL:!EXPORT:!DES:!MD5:!RC4:!ADH",-1);` |
|      91 | 3312 | `}` |
|       - | 3313 | `#endif /* PH7_ENABLE_OPENSSL */` |
|     109 | 3314 | `PH7_FILEINFO_INT_CONST(FILEINFO_NONE,0)` |
|      87 | 3315 | `PH7_FILEINFO_INT_CONST(FILEINFO_SYMLINK,2)` |
|      91 | 3316 | `PH7_FILEINFO_INT_CONST(FILEINFO_MIME,1040)` |
|     103 | 3317 | `PH7_FILEINFO_INT_CONST(FILEINFO_MIME_TYPE,16)` |
|      91 | 3318 | `PH7_FILEINFO_INT_CONST(FILEINFO_MIME_ENCODING,1024)` |
|      87 | 3319 | `PH7_FILEINFO_INT_CONST(FILEINFO_DEVICES,8)` |
|      87 | 3320 | `PH7_FILEINFO_INT_CONST(FILEINFO_CONTINUE,32)` |
|      87 | 3321 | `PH7_FILEINFO_INT_CONST(FILEINFO_PRESERVE_ATIME,128)` |
|      87 | 3322 | `PH7_FILEINFO_INT_CONST(FILEINFO_RAW,256)` |
|      87 | 3323 | `PH7_FILEINFO_INT_CONST(FILEINFO_APPLE,2048)` |
|      89 | 3324 | `PH7_FILEINFO_INT_CONST(FILEINFO_EXTENSION,16777216)` |
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
|     432 | 4083 | `PH7_PRIVATE int PH7_ExpandBuiltinConstant(ph7_vm *pVm,const char *zName,sxu32 nName,ph7_value *pOut)` |
|       4 | 4084 | `{` |
|       - | 4085 | `	sxu32 n;` |
|     463 | 4086 | `	if( !((nName > 2 && zName[0] == 'E' && zName[1] == '_')` |
|     243 | 4087 | `	   \|\| (nName > 4 && SyMemcmp(zName,"PHP_",4) == 0)) ){` |
|     388 | 4088 | `		return 0;` |
|       - | 4089 | `	}` |
|    7640 | 4090 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
|    7636 | 4091 | `		if( SyStrlen(aBuiltIn[n].zName) == nName` |
|    3936 | 4092 | `		 && SyMemcmp(aBuiltIn[n].zName,zName,nName) == 0 ){` |
|      52 | 4093 | `			aBuiltIn[n].xExpand(pOut,(void *)pVm);` |
|      52 | 4094 | `			return 1;` |
|       - | 4095 | `		}` |
|    3798 | 4096 | `	}` |
|     ! 0 | 4097 | `	return 0;` |
|     220 | 4098 | `}` |
|       - | 4099 | `/*` |
|       - | 4100 | ` * Register the built-in constants defined above.` |
|       - | 4101 | ` */` |
|    6985 | 4102 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|       5 | 4103 | `{` |
|       - | 4104 | `	sxu32 n;` |
|       - | 4105 | `	/*` |
|       - | 4106 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|       - | 4107 | `	 * that trigger the constant invocation as their private data.` |
|       - | 4108 | `	 */` |
| 4344708 | 4109 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 4337723 | 4110 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
| 2154971 | 4111 | `	}` |
|    6990 | 4112 | `}` |
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
|    6985 | 4138 | `PH7_PRIVATE void PH7_MarkDeprecatedConstants(ph7_vm *pVm)` |
|       5 | 4139 | `{` |
|       - | 4140 | `	sxu32 n;` |
|   55885 | 4141 | `	for( n = 0 ; n < SX_ARRAYSIZE(aDeprecatedConst) ; ++n ){` |
|   97795 | 4142 | `		SyHashEntry *pEntry = PH7_VmConstantFetch(pVm,` |
|   48895 | 4143 | `			aDeprecatedConst[n].zName,` |
|   48895 | 4144 | `			SyStrlen(aDeprecatedConst[n].zName),1);` |
|   48900 | 4145 | `		if( pEntry ){` |
|   48900 | 4146 | `			((ph7_constant *)pEntry->pUserData)->zDeprecated = aDeprecatedConst[n].zWhy;` |
|   24409 | 4147 | `		}` |
|   24414 | 4148 | `	}` |
|    6990 | 4149 | `}` |
