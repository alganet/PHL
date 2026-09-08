# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1127/1154 lines (97.66%)

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
|   3826 |   63 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|      5 |   64 | `{` |
|      - |   65 | `#if defined(__WINNT__)` |
|      5 |   66 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|      - |   67 | `#elif defined(__UNIXES__)` |
|      - |   68 | `	struct utsname sInfo;` |
|   3826 |   69 | `	if( uname(&sInfo) != 0 ){` |
|    ! 0 |   70 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 |   71 | `	}else{` |
|   3826 |   72 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|      - |   73 | `	}` |
|      - |   74 | `#else` |
|      - |   75 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|      - |   76 | `#endif` |
|   1913 |   77 | `	SXUNUSED(pUnused);` |
|   3831 |   78 | `}` |
|      - |   79 | `/*` |
|      - |   80 | ` * PHP_EOL` |
|      - |   81 | ` *  Expand the correct 'End Of Line' symbol for this platform.` |
|      - |   82 | ` */` |
|    840 |   83 | `static void PH7_EOL_Const(ph7_value *pVal,void *pUnused)` |
|      4 |   84 | `{` |
|    420 |   85 | `	SXUNUSED(pUnused);` |
|      - |   86 | `#ifdef __WINNT__` |
|      4 |   87 | `	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);` |
|      - |   88 | `#else` |
|    840 |   89 | `	ph7_value_string(pVal,"\n",(int)sizeof(char));` |
|      - |   90 | `#endif` |
|    844 |   91 | `}` |
|      - |   92 | `/*` |
|      - |   93 | ` * PHP_INT_MAX` |
|      - |   94 | ` * Expand the largest integer supported.` |
|      - |   95 | ` * Note that PH7 deals with 64-bit integer for all platforms.` |
|      - |   96 | ` */` |
|    106 |   97 | `static void PH7_INTMAX_Const(ph7_value *pVal,void *pUnused)` |
|      3 |   98 | `{` |
|     53 |   99 | `	SXUNUSED(pUnused);` |
|    109 |  100 | `	ph7_value_int64(pVal,SXI64_HIGH);` |
|    109 |  101 | `}` |
|      - |  102 | `/* ext/calendar: the only calendar cal_days_in_month() is asked for in practice. */` |
|      4 |  103 | `static void PH7_CAL_GREGORIAN_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  104 | `{` |
|      2 |  105 | `	SXUNUSED(pUnused);` |
|      5 |  106 | `	ph7_value_int(pVal,0);` |
|      5 |  107 | `}` |
|      - |  108 | `/*` |
|      - |  109 | ` * PHP_INT_MIN (php 7.0)` |
|      - |  110 | ` * Expand the smallest integer supported.` |
|      - |  111 | ` */` |
|     46 |  112 | `static void PH7_INTMIN_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  113 | `{` |
|     23 |  114 | `	SXUNUSED(pUnused);` |
|     47 |  115 | `	ph7_value_int64(pVal,SMALLEST_INT64);` |
|     47 |  116 | `}` |
|      - |  117 | `/*` |
|      - |  118 | ` * PHP_INT_SIZE` |
|      - |  119 | ` * Expand the size in bytes of a 64-bit integer.` |
|      - |  120 | ` */` |
|      4 |  121 | `static void PH7_INTSIZE_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  122 | `{` |
|      2 |  123 | `	SXUNUSED(pUnused);` |
|      5 |  124 | `	ph7_value_int64(pVal,sizeof(sxi64));` |
|      5 |  125 | `}` |
|      - |  126 | `/*` |
|      - |  127 | ` * PHP_FLOAT_EPSILON / PHP_FLOAT_MAX / PHP_FLOAT_MIN / PHP_FLOAT_DIG (php 7.2)` |
|      - |  128 | ` * Double-precision characteristics, sourced from <float.h> exactly like php` |
|      - |  129 | ` * so they track the compiling platform's actual double representation.` |
|      - |  130 | ` */` |
|      4 |  131 | `static void PH7_FLOATEPSILON_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  132 | `{` |
|      2 |  133 | `	SXUNUSED(pUnused);` |
|      5 |  134 | `	ph7_value_double(pVal,DBL_EPSILON);` |
|      5 |  135 | `}` |
|      2 |  136 | `static void PH7_FLOATMAX_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  137 | `{` |
|      1 |  138 | `	SXUNUSED(pUnused);` |
|      3 |  139 | `	ph7_value_double(pVal,DBL_MAX);` |
|      3 |  140 | `}` |
|      2 |  141 | `static void PH7_FLOATMIN_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  142 | `{` |
|      1 |  143 | `	SXUNUSED(pUnused);` |
|      3 |  144 | `	ph7_value_double(pVal,DBL_MIN);` |
|      3 |  145 | `}` |
|      2 |  146 | `static void PH7_FLOATDIG_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  147 | `{` |
|      1 |  148 | `	SXUNUSED(pUnused);` |
|      3 |  149 | `	ph7_value_int64(pVal,DBL_DIG);` |
|      3 |  150 | `}` |
|      - |  151 | `/*` |
|      - |  152 | ` * DIRECTORY_SEPARATOR.` |
|      - |  153 | ` * Expand the directory separator character.` |
|      - |  154 | ` */` |
|    292 |  155 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|      4 |  156 | `{` |
|    146 |  157 | `	SXUNUSED(pUnused);` |
|      - |  158 | `#ifdef __WINNT__` |
|      4 |  159 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|      - |  160 | `#else` |
|    292 |  161 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|      - |  162 | `#endif` |
|    296 |  163 | `}` |
|      - |  164 | `/*` |
|      - |  165 | ` * PATH_SEPARATOR.` |
|      - |  166 | ` * Expand the path separator character.` |
|      - |  167 | ` */` |
|      2 |  168 | `static void PH7_PATHSEP_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  169 | `{` |
|      1 |  170 | `	SXUNUSED(pUnused);` |
|      - |  171 | `#ifdef __WINNT__` |
|      1 |  172 | `	ph7_value_string(pVal,";",(int)sizeof(char));` |
|      - |  173 | `#else` |
|      2 |  174 | `	ph7_value_string(pVal,":",(int)sizeof(char));` |
|      - |  175 | `#endif` |
|      3 |  176 | `}` |
|      - |  177 |  |
|      - |  178 | `#if defined(PH7_ENABLE_MATH_FUNC)` |
|      - |  179 | `/*` |
|      - |  180 | ` * NAN constant: floating-point Not-A-Number` |
|      - |  181 | ` */` |
|     86 |  182 | `static void PH7_NAN_Const(ph7_value *pVal,void *pUnused)` |
|      2 |  183 | `{` |
|     43 |  184 | `	SXUNUSED(pUnused);` |
|     88 |  185 | `	ph7_value_double(pVal, PH7_NAN_VALUE());` |
|     88 |  186 | `}` |
|      - |  187 |  |
|      - |  188 | `/*` |
|      - |  189 | ` * INF constant: positive infinity` |
|      - |  190 | ` */` |
|     88 |  191 | `static void PH7_INF_Const(ph7_value *pVal,void *pUnused)` |
|      2 |  192 | `{` |
|     44 |  193 | `	SXUNUSED(pUnused);` |
|      - |  194 | `	/* similarly avoid the INFINITY macro */` |
|     90 |  195 | `	ph7_value_double(pVal, PH7_INF_VALUE());` |
|     90 |  196 | `}` |
|      - |  197 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - |  198 |  |
|      - |  199 | `#ifndef __WINNT__` |
|      - |  200 | `#include <time.h>` |
|      - |  201 | `#endif` |
|      - |  202 | `/*` |
|      - |  203 | ` * __TIME__` |
|      - |  204 | ` *  Expand the current time (GMT).` |
|      - |  205 | ` */` |
|      2 |  206 | `static void PH7_TIME_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  207 | `{` |
|      - |  208 | `	Sytm sTm;` |
|      - |  209 | `#ifdef __WINNT__` |
|      - |  210 | `	SYSTEMTIME sOS;` |
|      1 |  211 | `	GetSystemTime(&sOS);` |
|      1 |  212 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - |  213 | `#else` |
|      - |  214 | `	struct tm *pTm;` |
|      - |  215 | `	time_t t;` |
|      2 |  216 | `	time(&t);` |
|      2 |  217 | `	pTm = gmtime(&t);` |
|      2 |  218 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - |  219 | `#endif` |
|      1 |  220 | `	SXUNUSED(pUnused); /* cc warning */` |
|      - |  221 | `	/* Expand */` |
|      3 |  222 | `	ph7_value_string_format(pVal,"%02d:%02d:%02d",sTm.tm_hour,sTm.tm_min,sTm.tm_sec);` |
|      3 |  223 | `}` |
|      - |  224 | `/*` |
|      - |  225 | ` * __DATE__` |
|      - |  226 | ` *  Expand the current date in the ISO-8601 format.` |
|      - |  227 | ` */` |
|      2 |  228 | `static void PH7_DATE_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  229 | `{` |
|      - |  230 | `	Sytm sTm;` |
|      - |  231 | `#ifdef __WINNT__` |
|      - |  232 | `	SYSTEMTIME sOS;` |
|      1 |  233 | `	GetSystemTime(&sOS);` |
|      1 |  234 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - |  235 | `#else` |
|      - |  236 | `	struct tm *pTm;` |
|      - |  237 | `	time_t t;` |
|      2 |  238 | `	time(&t);` |
|      2 |  239 | `	pTm = gmtime(&t);` |
|      2 |  240 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - |  241 | `#endif` |
|      1 |  242 | `	SXUNUSED(pUnused); /* cc warning */` |
|      - |  243 | `	/* Expand */` |
|      3 |  244 | `	ph7_value_string_format(pVal,"%04d-%02d-%02d",sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|      3 |  245 | `}` |
|      - |  246 | `/*` |
|      - |  247 | ` * __FILE__` |
|      - |  248 | ` *  Path of the processed script.` |
|      - |  249 | ` */` |
|     44 |  250 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  251 | `{` |
|     49 |  252 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  253 | `	SyString *pFile;` |
|      - |  254 | `	/* Peek the top entry */` |
|     49 |  255 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     49 |  256 | `	if( pFile == 0 ){` |
|      - |  257 | `		/* Expand the magic word: ":MEMORY:" */` |
|    ! 0 |  258 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|    ! 0 |  259 | `	}else{` |
|     49 |  260 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|      - |  261 | `	}` |
|     49 |  262 | `}` |
|      - |  263 | `/*` |
|      - |  264 | ` * __DIR__` |
|      - |  265 | ` *  Directory holding the processed script.` |
|      - |  266 | ` */` |
|     40 |  267 | `static void PH7_DIR_Const(ph7_value *pVal,void *pUserData)` |
|      3 |  268 | `{` |
|     43 |  269 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  270 | `	SyString *pFile;` |
|      - |  271 | `	/* Peek the top entry */` |
|     43 |  272 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     43 |  273 | `	if( pFile == 0 ){` |
|      - |  274 | `		/* Expand the magic word: ":MEMORY:" */` |
|    ! 0 |  275 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|    ! 0 |  276 | `	}else{` |
|     43 |  277 | `		if( pFile->nByte > 0 ){` |
|      - |  278 | `			const char *zDir;` |
|      - |  279 | `			int nLen;` |
|     43 |  280 | `			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|     43 |  281 | `			ph7_value_string(pVal,zDir,nLen);` |
|     23 |  282 | `		}else{` |
|      - |  283 | `			/* Expand '.' as the current directory*/` |
|    ! 0 |  284 | `			ph7_value_string(pVal,".",(int)sizeof(char));` |
|      - |  285 | `		}` |
|      - |  286 | `	}` |
|     43 |  287 | `}` |
|      - |  288 | `/*` |
|      - |  289 | ` * PHP_SHLIB_SUFFIX` |
|      - |  290 | ` *  Expand shared library suffix.` |
|      - |  291 | ` */` |
|      2 |  292 | `static void PH7_PHP_SHLIB_SUFFIX_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 |  293 | `{` |
|      - |  294 | `#ifdef __WINNT__` |
|    ! 0 |  295 | `	ph7_value_string(pVal,"dll",(int)sizeof("dll")-1);` |
|      - |  296 | `#else` |
|      2 |  297 | `	ph7_value_string(pVal,"so",(int)sizeof("so")-1);` |
|      - |  298 | `#endif` |
|      1 |  299 | `	SXUNUSED(pUserData); /* cc warning */` |
|      2 |  300 | `}` |
|      - |  301 | `/*` |
|      - |  302 | ` * E_ERROR` |
|      - |  303 | ` *  Expands 1` |
|      - |  304 | ` */` |
|      2 |  305 | `static void PH7_E_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  306 | `{` |
|      3 |  307 | `	ph7_value_int(pVal,1);` |
|      1 |  308 | `	SXUNUSED(pUserData);` |
|      3 |  309 | `}` |
|      - |  310 | `/*` |
|      - |  311 | ` * E_WARNING` |
|      - |  312 | ` *  Expands 2` |
|      - |  313 | ` */` |
|      2 |  314 | `static void PH7_E_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  315 | `{` |
|      3 |  316 | `	ph7_value_int(pVal,2);` |
|      1 |  317 | `	SXUNUSED(pUserData);` |
|      3 |  318 | `}` |
|      - |  319 | `/*` |
|      - |  320 | ` * E_PARSE` |
|      - |  321 | ` *  Expands 4` |
|      - |  322 | ` */` |
|      2 |  323 | `static void PH7_E_PARSE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  324 | `{` |
|      3 |  325 | `	ph7_value_int(pVal,4);` |
|      1 |  326 | `	SXUNUSED(pUserData);` |
|      3 |  327 | `}` |
|      - |  328 | `/*` |
|      - |  329 | ` * E_NOTICE` |
|      - |  330 | ` * Expands 8` |
|      - |  331 | ` */` |
|      2 |  332 | `static void PH7_E_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  333 | `{` |
|      3 |  334 | `	ph7_value_int(pVal,8);` |
|      1 |  335 | `	SXUNUSED(pUserData);` |
|      3 |  336 | `}` |
|      - |  337 | `/*` |
|      - |  338 | ` * E_CORE_ERROR` |
|      - |  339 | ` * Expands 16` |
|      - |  340 | ` */` |
|      2 |  341 | `static void PH7_E_CORE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  342 | `{` |
|      3 |  343 | `	ph7_value_int(pVal,16);` |
|      1 |  344 | `	SXUNUSED(pUserData);` |
|      3 |  345 | `}` |
|      - |  346 | `/*` |
|      - |  347 | ` * E_CORE_WARNING` |
|      - |  348 | ` * Expands 32` |
|      - |  349 | ` */` |
|      2 |  350 | `static void PH7_E_CORE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  351 | `{` |
|      3 |  352 | `	ph7_value_int(pVal,32);` |
|      1 |  353 | `	SXUNUSED(pUserData);` |
|      3 |  354 | `}` |
|      - |  355 | `/*` |
|      - |  356 | ` * E_COMPILE_ERROR` |
|      - |  357 | ` * Expands 64` |
|      - |  358 | ` */` |
|      2 |  359 | `static void PH7_E_COMPILE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  360 | `{` |
|      3 |  361 | `	ph7_value_int(pVal,64);` |
|      1 |  362 | `	SXUNUSED(pUserData);` |
|      3 |  363 | `}` |
|      - |  364 | `/*` |
|      - |  365 | ` * E_COMPILE_WARNING` |
|      - |  366 | ` * Expands 128` |
|      - |  367 | ` */` |
|      2 |  368 | `static void PH7_E_COMPILE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  369 | `{` |
|      3 |  370 | `	ph7_value_int(pVal,128);` |
|      1 |  371 | `	SXUNUSED(pUserData);` |
|      3 |  372 | `}` |
|      - |  373 | `/*` |
|      - |  374 | ` * E_USER_ERROR` |
|      - |  375 | ` * Expands 256` |
|      - |  376 | ` */` |
|      2 |  377 | `static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  378 | `{` |
|      3 |  379 | `	ph7_value_int(pVal,256);` |
|      1 |  380 | `	SXUNUSED(pUserData);` |
|      3 |  381 | `}` |
|      - |  382 | `/*` |
|      - |  383 | ` * E_USER_WARNING` |
|      - |  384 | ` * Expands 512` |
|      - |  385 | ` */` |
|     16 |  386 | `static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      4 |  387 | `{` |
|     20 |  388 | `	ph7_value_int(pVal,512);` |
|      8 |  389 | `	SXUNUSED(pUserData);` |
|     20 |  390 | `}` |
|      - |  391 | `/*` |
|      - |  392 | ` * E_USER_NOTICE` |
|      - |  393 | ` * Expands 1024` |
|      - |  394 | ` */` |
|      6 |  395 | `static void PH7_E_USER_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|      3 |  396 | `{` |
|      9 |  397 | `	ph7_value_int(pVal,1024);` |
|      3 |  398 | `	SXUNUSED(pUserData);` |
|      9 |  399 | `}` |
|      - |  400 | `/*` |
|      - |  401 | ` * E_STRICT` |
|      - |  402 | ` * Expands 2048` |
|      - |  403 | ` */` |
|      2 |  404 | `static void PH7_E_STRICT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  405 | `{` |
|      3 |  406 | `	ph7_value_int(pVal,2048);` |
|      1 |  407 | `	SXUNUSED(pUserData);` |
|      3 |  408 | `}` |
|      - |  409 | `/*` |
|      - |  410 | ` * E_RECOVERABLE_ERROR` |
|      - |  411 | ` * Expands 4096` |
|      - |  412 | ` */` |
|      2 |  413 | `static void PH7_E_RECOVERABLE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  414 | `{` |
|      3 |  415 | `	ph7_value_int(pVal,4096);` |
|      1 |  416 | `	SXUNUSED(pUserData);` |
|      3 |  417 | `}` |
|      - |  418 | `/*` |
|      - |  419 | ` * E_DEPRECATED` |
|      - |  420 | ` * Expands 8192` |
|      - |  421 | ` */` |
|     22 |  422 | `static void PH7_E_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  423 | `{` |
|     27 |  424 | `	ph7_value_int(pVal,8192);` |
|     11 |  425 | `	SXUNUSED(pUserData);` |
|     27 |  426 | `}` |
|      - |  427 | `/*` |
|      - |  428 | ` * E_USER_DEPRECATED` |
|      - |  429 | ` *   Expands 16384.` |
|      - |  430 | ` */` |
|      2 |  431 | `static void PH7_E_USER_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  432 | `{` |
|      3 |  433 | `	ph7_value_int(pVal,16384);` |
|      1 |  434 | `	SXUNUSED(pUserData);` |
|      3 |  435 | `}` |
|      - |  436 | `/*` |
|      - |  437 | ` * E_ALL` |
|      - |  438 | ` *  Expands 30719 (php 8: E_STRICT is no longer part of E_ALL)` |
|      - |  439 | ` */` |
|     30 |  440 | `static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  441 | `{` |
|     35 |  442 | `	ph7_value_int(pVal,30719);` |
|     15 |  443 | `	SXUNUSED(pUserData);` |
|     35 |  444 | `}` |
|      - |  445 | `/*` |
|      - |  446 | ` * CASE_LOWER` |
|      - |  447 | ` *  Expands 0.` |
|      - |  448 | ` */` |
|      2 |  449 | `static void PH7_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  450 | `{` |
|      3 |  451 | `	ph7_value_int(pVal,0);` |
|      1 |  452 | `	SXUNUSED(pUserData);` |
|      3 |  453 | `}` |
|      - |  454 | `/*` |
|      - |  455 | ` * CASE_UPPER` |
|      - |  456 | ` *  Expands 1.` |
|      - |  457 | ` */` |
|      8 |  458 | `static void PH7_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  459 | `{` |
|     10 |  460 | `	ph7_value_int(pVal,1);` |
|      4 |  461 | `	SXUNUSED(pUserData);` |
|     10 |  462 | `}` |
|      - |  463 | `/*` |
|      - |  464 | ` * STR_PAD_LEFT` |
|      - |  465 | ` *  Expands 0.` |
|      - |  466 | ` */` |
|      4 |  467 | `static void PH7_STR_PAD_LEFT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  468 | `{` |
|      5 |  469 | `	ph7_value_int(pVal,0);` |
|      2 |  470 | `	SXUNUSED(pUserData);` |
|      5 |  471 | `}` |
|      - |  472 | `/*` |
|      - |  473 | ` * STR_PAD_RIGHT` |
|      - |  474 | ` *  Expands 1.` |
|      - |  475 | ` */` |
|      4 |  476 | `static void PH7_STR_PAD_RIGHT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  477 | `{` |
|      5 |  478 | `	ph7_value_int(pVal,1);` |
|      2 |  479 | `	SXUNUSED(pUserData);` |
|      5 |  480 | `}` |
|      - |  481 | `/*` |
|      - |  482 | ` * STR_PAD_BOTH` |
|      - |  483 | ` *  Expands 2.` |
|      - |  484 | ` */` |
|      2 |  485 | `static void PH7_STR_PAD_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  486 | `{` |
|      3 |  487 | `	ph7_value_int(pVal,2);` |
|      1 |  488 | `	SXUNUSED(pUserData);` |
|      3 |  489 | `}` |
|      - |  490 | `/*` |
|      - |  491 | ` * COUNT_NORMAL` |
|      - |  492 | ` *  Expands 0` |
|      - |  493 | ` */` |
|      6 |  494 | `static void PH7_COUNT_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  495 | `{` |
|      8 |  496 | `	ph7_value_int(pVal,0);` |
|      3 |  497 | `	SXUNUSED(pUserData);` |
|      8 |  498 | `}` |
|      - |  499 | `/*` |
|      - |  500 | ` * COUNT_RECURSIVE` |
|      - |  501 | ` *  Expands 1.` |
|      - |  502 | ` */` |
|     20 |  503 | `static void PH7_COUNT_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  504 | `{` |
|     22 |  505 | `	ph7_value_int(pVal,1);` |
|     10 |  506 | `	SXUNUSED(pUserData);` |
|     22 |  507 | `}` |
|      - |  508 | `/*` |
|      - |  509 | ` * SORT_ASC` |
|      - |  510 | ` *  Expands 1.` |
|      - |  511 | ` */` |
|      2 |  512 | `static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  513 | `{` |
|      3 |  514 | `	ph7_value_int(pVal,1);` |
|      1 |  515 | `	SXUNUSED(pUserData);` |
|      3 |  516 | `}` |
|      - |  517 | `/*` |
|      - |  518 | ` * SORT_DESC` |
|      - |  519 | ` *  Expands 2.` |
|      - |  520 | ` */` |
|      2 |  521 | `static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  522 | `{` |
|      3 |  523 | `	ph7_value_int(pVal,2);` |
|      1 |  524 | `	SXUNUSED(pUserData);` |
|      3 |  525 | `}` |
|      - |  526 | `/*` |
|      - |  527 | ` * SORT_REGULAR` |
|      - |  528 | ` *  Expands 3.` |
|      - |  529 | ` */` |
|      4 |  530 | `static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  531 | `{` |
|      5 |  532 | `	ph7_value_int(pVal,3);` |
|      2 |  533 | `	SXUNUSED(pUserData);` |
|      5 |  534 | `}` |
|      - |  535 | `/*` |
|      - |  536 | ` * SORT_NUMERIC` |
|      - |  537 | ` *  Expands 4.` |
|      - |  538 | ` */` |
|      8 |  539 | `static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  540 | `{` |
|      9 |  541 | `	ph7_value_int(pVal,4);` |
|      4 |  542 | `	SXUNUSED(pUserData);` |
|      9 |  543 | `}` |
|      - |  544 | `/*` |
|      - |  545 | ` * SORT_STRING` |
|      - |  546 | ` *  Expands 5.` |
|      - |  547 | ` */` |
|     12 |  548 | `static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  549 | `{` |
|     13 |  550 | `	ph7_value_int(pVal,5);` |
|      6 |  551 | `	SXUNUSED(pUserData);` |
|     13 |  552 | `}` |
|      - |  553 | `/*` |
|      - |  554 | ` * PHP_ROUND_HALF_UP` |
|      - |  555 | ` *  Expands 1.` |
|      - |  556 | ` */` |
|      4 |  557 | `static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  558 | `{` |
|      5 |  559 | `	ph7_value_int(pVal,1);` |
|      2 |  560 | `	SXUNUSED(pUserData);` |
|      5 |  561 | `}` |
|      - |  562 | `/*` |
|      - |  563 | ` * PHP_SESSION_DISABLED / PHP_SESSION_NONE / PHP_SESSION_ACTIVE` |
|      - |  564 | ` *  session_status() states (0 / 1 / 2).` |
|      - |  565 | ` */` |
|      2 |  566 | `static void PH7_PHP_SESSION_DISABLED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  567 | `{` |
|      3 |  568 | `	ph7_value_int(pVal,0);` |
|      1 |  569 | `	SXUNUSED(pUserData);` |
|      3 |  570 | `}` |
|      6 |  571 | `static void PH7_PHP_SESSION_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  572 | `{` |
|      7 |  573 | `	ph7_value_int(pVal,1);` |
|      3 |  574 | `	SXUNUSED(pUserData);` |
|      7 |  575 | `}` |
|     30 |  576 | `static void PH7_PHP_SESSION_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  577 | `{` |
|     32 |  578 | `	ph7_value_int(pVal,2);` |
|     15 |  579 | `	SXUNUSED(pUserData);` |
|     32 |  580 | `}` |
|      - |  581 | `/*` |
|      - |  582 | ` * INI_USER / INI_PERDIR / INI_SYSTEM / INI_ALL` |
|      - |  583 | ` *  php.ini access levels (1 / 2 / 4 / 7).` |
|      - |  584 | ` */` |
|     12 |  585 | `static void PH7_INI_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  586 | `{` |
|     13 |  587 | `	ph7_value_int(pVal,1);` |
|      6 |  588 | `	SXUNUSED(pUserData);` |
|     13 |  589 | `}` |
|      2 |  590 | `static void PH7_INI_PERDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  591 | `{` |
|      3 |  592 | `	ph7_value_int(pVal,2);` |
|      1 |  593 | `	SXUNUSED(pUserData);` |
|      3 |  594 | `}` |
|      2 |  595 | `static void PH7_INI_SYSTEM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  596 | `{` |
|      3 |  597 | `	ph7_value_int(pVal,4);` |
|      1 |  598 | `	SXUNUSED(pUserData);` |
|      3 |  599 | `}` |
|      2 |  600 | `static void PH7_INI_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  601 | `{` |
|      3 |  602 | `	ph7_value_int(pVal,7);` |
|      1 |  603 | `	SXUNUSED(pUserData);` |
|      3 |  604 | `}` |
|      - |  605 | `/*` |
|      - |  606 | ` * MB_CASE_UPPER / MB_CASE_LOWER / MB_CASE_TITLE (0 / 1 / 2)` |
|      - |  607 | ` */` |
|      4 |  608 | `static void PH7_MB_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  609 | `{` |
|      5 |  610 | `	ph7_value_int(pVal,0);` |
|      2 |  611 | `	SXUNUSED(pUserData);` |
|      5 |  612 | `}` |
|      4 |  613 | `static void PH7_MB_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  614 | `{` |
|      5 |  615 | `	ph7_value_int(pVal,1);` |
|      2 |  616 | `	SXUNUSED(pUserData);` |
|      5 |  617 | `}` |
|      4 |  618 | `static void PH7_MB_CASE_TITLE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  619 | `{` |
|      5 |  620 | `	ph7_value_int(pVal,2);` |
|      2 |  621 | `	SXUNUSED(pUserData);` |
|      5 |  622 | `}` |
|      - |  623 | `/*` |
|      - |  624 | ` * SPHP_ROUND_HALF_DOWN` |
|      - |  625 | ` *  Expands 2.` |
|      - |  626 | ` */` |
|      4 |  627 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  628 | `{` |
|      5 |  629 | `	ph7_value_int(pVal,2);` |
|      2 |  630 | `	SXUNUSED(pUserData);` |
|      5 |  631 | `}` |
|      - |  632 | `/*` |
|      - |  633 | ` * PHP_ROUND_HALF_EVEN` |
|      - |  634 | ` *  Expands 3.` |
|      - |  635 | ` */` |
|      8 |  636 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  637 | `{` |
|      9 |  638 | `	ph7_value_int(pVal,3);` |
|      4 |  639 | `	SXUNUSED(pUserData);` |
|      9 |  640 | `}` |
|      - |  641 | `/*` |
|      - |  642 | ` * PHP_ROUND_HALF_ODD` |
|      - |  643 | ` *  Expands 4.` |
|      - |  644 | ` */` |
|      4 |  645 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  646 | `{` |
|      5 |  647 | `	ph7_value_int(pVal,4);` |
|      2 |  648 | `	SXUNUSED(pUserData);` |
|      5 |  649 | `}` |
|      - |  650 | `/*` |
|      - |  651 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|      - |  652 | ` *  Expand 0x01` |
|      - |  653 | ` * NOTE:` |
|      - |  654 | ` *  The expanded value must be a power of two.` |
|      - |  655 | ` */` |
|      2 |  656 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  657 | `{` |
|      3 |  658 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      1 |  659 | `	SXUNUSED(pUserData);` |
|      3 |  660 | `}` |
|      - |  661 | `/*` |
|      - |  662 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|      - |  663 | ` *  Expand 0x02` |
|      - |  664 | ` * NOTE:` |
|      - |  665 | ` *  The expanded value must be a power of two.` |
|      - |  666 | ` */` |
|      2 |  667 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  668 | `{` |
|      3 |  669 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      1 |  670 | `	SXUNUSED(pUserData);` |
|      3 |  671 | `}` |
|      - |  672 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  673 | `/*` |
|      - |  674 | ` * M_PI` |
|      - |  675 | ` *  Expand the value of pi.` |
|      - |  676 | ` */` |
|      8 |  677 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  678 | `{` |
|      4 |  679 | `	SXUNUSED(pUserData); /* cc warning */` |
|     10 |  680 | `	ph7_value_double(pVal,PH7_PI);` |
|     10 |  681 | `}` |
|      - |  682 | `/*` |
|      - |  683 | ` * M_E` |
|      - |  684 | ` *  Expand 2.7182818284590452354` |
|      - |  685 | ` */` |
|      2 |  686 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  687 | `{` |
|      1 |  688 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  689 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|      3 |  690 | `}` |
|      - |  691 | `/*` |
|      - |  692 | ` * M_LOG2E` |
|      - |  693 | ` *  Expand 2.7182818284590452354` |
|      - |  694 | ` */` |
|      2 |  695 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  696 | `{` |
|      1 |  697 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  698 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      3 |  699 | `}` |
|      - |  700 | `/*` |
|      - |  701 | ` * M_LOG10E` |
|      - |  702 | ` *  Expand 0.4342944819032518276` |
|      - |  703 | ` */` |
|      2 |  704 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  705 | `{` |
|      1 |  706 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  707 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      3 |  708 | `}` |
|      - |  709 | `/*` |
|      - |  710 | ` * M_LN2` |
|      - |  711 | ` *  Expand 	0.69314718055994530942` |
|      - |  712 | ` */` |
|      2 |  713 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  714 | `{` |
|      1 |  715 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  716 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      3 |  717 | `}` |
|      - |  718 | `/*` |
|      - |  719 | ` * M_LN10` |
|      - |  720 | ` *  Expand 	2.30258509299404568402` |
|      - |  721 | ` */` |
|      2 |  722 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  723 | `{` |
|      1 |  724 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  725 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      3 |  726 | `}` |
|      - |  727 | `/*` |
|      - |  728 | ` * M_PI_2` |
|      - |  729 | ` *  Expand 	1.57079632679489661923` |
|      - |  730 | ` */` |
|      2 |  731 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  732 | `{` |
|      1 |  733 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  734 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      3 |  735 | `}` |
|      - |  736 | `/*` |
|      - |  737 | ` * M_PI_4` |
|      - |  738 | ` *  Expand 	0.78539816339744830962` |
|      - |  739 | ` */` |
|      2 |  740 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  741 | `{` |
|      1 |  742 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  743 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      3 |  744 | `}` |
|      - |  745 | `/*` |
|      - |  746 | ` * M_1_PI` |
|      - |  747 | ` *  Expand 	0.31830988618379067154` |
|      - |  748 | ` */` |
|      2 |  749 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  750 | `{` |
|      1 |  751 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  752 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      3 |  753 | `}` |
|      - |  754 | `/*` |
|      - |  755 | ` * M_2_PI` |
|      - |  756 | ` *  Expand 0.63661977236758134308` |
|      - |  757 | ` */` |
|      4 |  758 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  759 | `{` |
|      2 |  760 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  761 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      5 |  762 | `}` |
|      - |  763 | `/*` |
|      - |  764 | ` * M_SQRTPI` |
|      - |  765 | ` *  Expand 1.77245385090551602729` |
|      - |  766 | ` */` |
|      2 |  767 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  768 | `{` |
|      1 |  769 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  770 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      3 |  771 | `}` |
|      - |  772 | `/*` |
|      - |  773 | ` * M_2_SQRTPI` |
|      - |  774 | ` *  Expand 	1.12837916709551257390` |
|      - |  775 | ` */` |
|      2 |  776 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  777 | `{` |
|      1 |  778 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  779 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      3 |  780 | `}` |
|      - |  781 | `/*` |
|      - |  782 | ` * M_SQRT2` |
|      - |  783 | ` *  Expand 	1.41421356237309504880` |
|      - |  784 | ` */` |
|      2 |  785 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  786 | `{` |
|      1 |  787 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  788 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      3 |  789 | `}` |
|      - |  790 | `/*` |
|      - |  791 | ` * M_SQRT3` |
|      - |  792 | ` *  Expand 	1.73205080756887729352` |
|      - |  793 | ` */` |
|      2 |  794 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  795 | `{` |
|      1 |  796 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  797 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      3 |  798 | `}` |
|      - |  799 | `/*` |
|      - |  800 | ` * M_SQRT1_2` |
|      - |  801 | ` *  Expand 	0.70710678118654752440` |
|      - |  802 | ` */` |
|      2 |  803 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  804 | `{` |
|      1 |  805 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  806 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      3 |  807 | `}` |
|      - |  808 | `/*` |
|      - |  809 | ` * M_LNPI` |
|      - |  810 | ` *  Expand 	1.14472988584940017414` |
|      - |  811 | ` */` |
|      2 |  812 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  813 | `{` |
|      1 |  814 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  815 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      3 |  816 | `}` |
|      - |  817 | `/*` |
|      - |  818 | ` * M_EULER` |
|      - |  819 | ` *  Expand  0.57721566490153286061` |
|      - |  820 | ` */` |
|      2 |  821 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  822 | `{` |
|      1 |  823 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  824 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      3 |  825 | `}` |
|      - |  826 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|      - |  827 | `/*` |
|      - |  828 | ` * DATE_ATOM` |
|      - |  829 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|      - |  830 | ` */` |
|      2 |  831 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  832 | `{` |
|      1 |  833 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  834 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  835 | `}` |
|      - |  836 | `/*` |
|      - |  837 | ` * DATE_COOKIE` |
|      - |  838 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  839 | ` */` |
|      2 |  840 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  841 | `{` |
|      1 |  842 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  843 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  844 | `}` |
|      - |  845 | `/*` |
|      - |  846 | ` * DATE_ISO8601` |
|      - |  847 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|      - |  848 | ` */` |
|      2 |  849 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  850 | `{` |
|      1 |  851 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  852 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      3 |  853 | `}` |
|      - |  854 | `/*` |
|      - |  855 | ` * DATE_RFC822` |
|      - |  856 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|      - |  857 | ` */` |
|      2 |  858 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  859 | `{` |
|      1 |  860 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  861 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  862 | `}` |
|      - |  863 | `/*` |
|      - |  864 | ` * DATE_RFC850` |
|      - |  865 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  866 | ` */` |
|      2 |  867 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  868 | `{` |
|      1 |  869 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  870 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  871 | `}` |
|      - |  872 | `/*` |
|      - |  873 | ` * DATE_RFC1036` |
|      - |  874 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  875 | ` */` |
|      2 |  876 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  877 | `{` |
|      1 |  878 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  879 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  880 | `}` |
|      - |  881 | `/*` |
|      - |  882 | ` * DATE_RFC1123` |
|      - |  883 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  884 | ` */` |
|      2 |  885 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  886 | `{` |
|      1 |  887 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  888 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  889 | `}` |
|      - |  890 | `/*` |
|      - |  891 | ` * DATE_RFC2822` |
|      - |  892 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  893 | ` */` |
|      2 |  894 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  895 | `{` |
|      1 |  896 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  897 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  898 | `}` |
|      - |  899 | `/*` |
|      - |  900 | ` * DATE_RSS` |
|      - |  901 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  902 | ` */` |
|      2 |  903 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  904 | `{` |
|      1 |  905 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  906 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  907 | `}` |
|      - |  908 | `/*` |
|      - |  909 | ` * DATE_W3C` |
|      - |  910 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|      - |  911 | ` */` |
|      2 |  912 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  913 | `{` |
|      1 |  914 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  915 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  916 | `}` |
|      - |  917 | `/*` |
|      - |  918 | ` * The ENT_* values are PHP-exact (php 8.5.7). The low two bits are the quote` |
|      - |  919 | ` * bits (1 = single, 2 = double), so ENT_QUOTES = ENT_COMPAT\|1 and` |
|      - |  920 | ` * ENT_NOQUOTES = 0. Bits 16\|32 select the doctype (0 = HTML401, 16 = XML1,` |
|      - |  921 | ` * 32 = XHTML, 48 = HTML5) — composites, not flags.` |
|      - |  922 | ` */` |
|      - |  923 | `/*` |
|      - |  924 | ` * ENT_COMPAT` |
|      - |  925 | ` *  Expand 2 (double-quote bit only)` |
|      - |  926 | ` */` |
|     12 |  927 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  928 | `{` |
|      6 |  929 | `	SXUNUSED(pUserData); /* cc warning */` |
|     13 |  930 | `	ph7_value_int(pVal,PH7_ENT_QUOTE_DOUBLE);` |
|     13 |  931 | `}` |
|      - |  932 | `/*` |
|      - |  933 | ` * ENT_QUOTES` |
|      - |  934 | ` *  Expand 3 (double\|single quote bits)` |
|      - |  935 | ` */` |
|     60 |  936 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  937 | `{` |
|     30 |  938 | `	SXUNUSED(pUserData); /* cc warning */` |
|     61 |  939 | `	ph7_value_int(pVal,PH7_ENT_QUOTES);` |
|     61 |  940 | `}` |
|      - |  941 | `/*` |
|      - |  942 | ` * ENT_NOQUOTES` |
|      - |  943 | ` *  Expand 0 (no quote bits)` |
|      - |  944 | ` */` |
|     20 |  945 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  946 | `{` |
|     10 |  947 | `	SXUNUSED(pUserData); /* cc warning */` |
|     21 |  948 | `	ph7_value_int(pVal,0);` |
|     21 |  949 | `}` |
|      - |  950 | `/*` |
|      - |  951 | ` * ENT_IGNORE` |
|      - |  952 | ` *  Expand 4` |
|      - |  953 | ` */` |
|      6 |  954 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  955 | `{` |
|      3 |  956 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 |  957 | `	ph7_value_int(pVal,PH7_ENT_IGNORE);` |
|      7 |  958 | `}` |
|      - |  959 | `/*` |
|      - |  960 | ` * ENT_SUBSTITUTE` |
|      - |  961 | ` *  Expand 8` |
|      - |  962 | ` */` |
|      2 |  963 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  964 | `{` |
|      1 |  965 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  966 | `	ph7_value_int(pVal,PH7_ENT_SUBSTITUTE);` |
|      3 |  967 | `}` |
|      - |  968 | `/*` |
|      - |  969 | ` * ENT_DISALLOWED` |
|      - |  970 | ` *  Expand 128` |
|      - |  971 | ` */` |
|      2 |  972 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  973 | `{` |
|      1 |  974 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  975 | `	ph7_value_int(pVal,PH7_ENT_DISALLOWED);` |
|      3 |  976 | `}` |
|      - |  977 | `/*` |
|      - |  978 | ` * ENT_HTML401` |
|      - |  979 | ` *  Expand 0 (the default doctype)` |
|      - |  980 | ` */` |
|      2 |  981 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  982 | `{` |
|      1 |  983 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  984 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML401);` |
|      3 |  985 | `}` |
|      - |  986 | `/*` |
|      - |  987 | ` * ENT_XML1` |
|      - |  988 | ` *  Expand 16` |
|      - |  989 | ` */` |
|      8 |  990 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  991 | `{` |
|      4 |  992 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 |  993 | `	ph7_value_int(pVal,PH7_ENT_DOC_XML1);` |
|      9 |  994 | `}` |
|      - |  995 | `/*` |
|      - |  996 | ` * ENT_XHTML` |
|      - |  997 | ` *  Expand 32` |
|      - |  998 | ` */` |
|      6 |  999 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1000 | `{` |
|      3 | 1001 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1002 | `	ph7_value_int(pVal,PH7_ENT_DOC_XHTML);` |
|      7 | 1003 | `}` |
|      - | 1004 | `/*` |
|      - | 1005 | ` * ENT_HTML5` |
|      - | 1006 | ` *  Expand 48 (16\|32 — a doctype composite, not a flag bit)` |
|      - | 1007 | ` */` |
|      8 | 1008 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1009 | `{` |
|      4 | 1010 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 | 1011 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML5);` |
|      9 | 1012 | `}` |
|      - | 1013 | `/*` |
|      - | 1014 | ` * ISO-8859-1` |
|      - | 1015 | ` * ISO_8859_1` |
|      - | 1016 | ` *   Expand 1` |
|      - | 1017 | ` */` |
|      2 | 1018 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1019 | `{` |
|      1 | 1020 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1021 | `	ph7_value_int(pVal,1);` |
|      3 | 1022 | `}` |
|      - | 1023 | `/*` |
|      - | 1024 | ` * UTF-8` |
|      - | 1025 | ` * UTF8` |
|      - | 1026 | ` *  Expand 2` |
|      - | 1027 | ` */` |
|      2 | 1028 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1029 | `{` |
|      1 | 1030 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1031 | `	ph7_value_int(pVal,1);` |
|      3 | 1032 | `}` |
|      - | 1033 | `/*` |
|      - | 1034 | ` * HTML_ENTITIES` |
|      - | 1035 | ` *  Expand 1` |
|      - | 1036 | ` */` |
|      4 | 1037 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1038 | `{` |
|      2 | 1039 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1040 | `	ph7_value_int(pVal,1);` |
|      5 | 1041 | `}` |
|      - | 1042 | `/*` |
|      - | 1043 | ` * HTML_SPECIALCHARS` |
|      - | 1044 | ` *  Expand 0 (PHP-exact)` |
|      - | 1045 | ` */` |
|     10 | 1046 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1047 | `{` |
|      5 | 1048 | `	SXUNUSED(pUserData); /* cc warning */` |
|     11 | 1049 | `	ph7_value_int(pVal,0);` |
|     11 | 1050 | `}` |
|      - | 1051 | `/*` |
|      - | 1052 | ` * PHP_URL_SCHEME.` |
|      - | 1053 | ` * Expand 1` |
|      - | 1054 | ` */` |
|      2 | 1055 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1056 | `{` |
|      1 | 1057 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1058 | `	ph7_value_int(pVal,1);` |
|      3 | 1059 | `}` |
|      - | 1060 | `/*` |
|      - | 1061 | ` * PHP_URL_HOST.` |
|      - | 1062 | ` * Expand 2` |
|      - | 1063 | ` */` |
|      2 | 1064 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1065 | `{` |
|      1 | 1066 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1067 | `	ph7_value_int(pVal,2);` |
|      3 | 1068 | `}` |
|      - | 1069 | `/*` |
|      - | 1070 | ` * PHP_URL_PORT.` |
|      - | 1071 | ` * Expand 3` |
|      - | 1072 | ` */` |
|      2 | 1073 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1074 | `{` |
|      1 | 1075 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1076 | `	ph7_value_int(pVal,3);` |
|      3 | 1077 | `}` |
|      - | 1078 | `/*` |
|      - | 1079 | ` * PHP_URL_USER.` |
|      - | 1080 | ` * Expand 4` |
|      - | 1081 | ` */` |
|      2 | 1082 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1083 | `{` |
|      1 | 1084 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1085 | `	ph7_value_int(pVal,4);` |
|      3 | 1086 | `}` |
|      - | 1087 | `/*` |
|      - | 1088 | ` * PHP_URL_PASS.` |
|      - | 1089 | ` * Expand 5` |
|      - | 1090 | ` */` |
|      2 | 1091 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1092 | `{` |
|      1 | 1093 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1094 | `	ph7_value_int(pVal,5);` |
|      3 | 1095 | `}` |
|      - | 1096 | `/*` |
|      - | 1097 | ` * PHP_URL_PATH.` |
|      - | 1098 | ` * Expand 6` |
|      - | 1099 | ` */` |
|      2 | 1100 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1101 | `{` |
|      1 | 1102 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1103 | `	ph7_value_int(pVal,6);` |
|      3 | 1104 | `}` |
|      - | 1105 | `/*` |
|      - | 1106 | ` * PHP_URL_QUERY.` |
|      - | 1107 | ` * Expand 7` |
|      - | 1108 | ` */` |
|      2 | 1109 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1110 | `{` |
|      1 | 1111 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1112 | `	ph7_value_int(pVal,7);` |
|      3 | 1113 | `}` |
|      - | 1114 | `/*` |
|      - | 1115 | ` * PHP_URL_FRAGMENT.` |
|      - | 1116 | ` * Expand 8` |
|      - | 1117 | ` */` |
|      2 | 1118 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1119 | `{` |
|      1 | 1120 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1121 | `	ph7_value_int(pVal,8);` |
|      3 | 1122 | `}` |
|      - | 1123 | `/*` |
|      - | 1124 | ` * PHP_QUERY_RFC1738` |
|      - | 1125 | ` * Expand 1` |
|      - | 1126 | ` */` |
|      2 | 1127 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1128 | `{` |
|      1 | 1129 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1130 | `	ph7_value_int(pVal,1);` |
|      3 | 1131 | `}` |
|      - | 1132 | `/*` |
|      - | 1133 | ` * PHP_QUERY_RFC3986` |
|      - | 1134 | ` * Expand 1` |
|      - | 1135 | ` */` |
|      2 | 1136 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1137 | `{` |
|      1 | 1138 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1139 | `	ph7_value_int(pVal,2);` |
|      3 | 1140 | `}` |
|      - | 1141 | `/* php's FNM_* values (ext/standard): PATHNAME=1, NOESCAPE=2, PERIOD=4, CASEFOLD=16.` |
|      - | 1142 | ` * PHL previously had PATHNAME/NOESCAPE swapped and CASEFOLD=8; fnmatch() reads these` |
|      - | 1143 | ` * bits, so PH7_builtin_fnmatch was updated to the same values. */` |
|      - | 1144 | `/*` |
|      - | 1145 | ` * FNM_PATHNAME` |
|      - | 1146 | ` *  Expand 1 (php value)` |
|      - | 1147 | ` */` |
|    ! 0 | 1148 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 1149 | `{` |
|    ! 0 | 1150 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 1151 | `	ph7_value_int(pVal,1);` |
|    ! 0 | 1152 | `}` |
|      - | 1153 | `/*` |
|      - | 1154 | ` * FNM_NOESCAPE` |
|      - | 1155 | ` *  Expand 2 (php value)` |
|      - | 1156 | ` */` |
|    ! 0 | 1157 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 1158 | `{` |
|    ! 0 | 1159 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 1160 | `	ph7_value_int(pVal,2);` |
|    ! 0 | 1161 | `}` |
|      - | 1162 | `/*` |
|      - | 1163 | ` * FNM_PERIOD` |
|      - | 1164 | ` *  Expand 4 (php value)` |
|      - | 1165 | ` */` |
|      6 | 1166 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1167 | `{` |
|      3 | 1168 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1169 | `	ph7_value_int(pVal,4);` |
|      7 | 1170 | `}` |
|      - | 1171 | `/*` |
|      - | 1172 | ` * FNM_CASEFOLD` |
|      - | 1173 | ` *  Expand 16 (php value)` |
|      - | 1174 | ` */` |
|      4 | 1175 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1176 | `{` |
|      2 | 1177 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1178 | `	ph7_value_int(pVal,16);` |
|      5 | 1179 | `}` |
|      - | 1180 | `/*` |
|      - | 1181 | ` * PATHINFO_DIRNAME` |
|      - | 1182 | ` *  Expand 1.` |
|      - | 1183 | ` */` |
|      4 | 1184 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1185 | `{` |
|      2 | 1186 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1187 | `	ph7_value_int(pVal,1);` |
|      5 | 1188 | `}` |
|      - | 1189 | `/*` |
|      - | 1190 | ` * PATHINFO_BASENAME` |
|      - | 1191 | ` *  Expand 2.` |
|      - | 1192 | ` */` |
|      4 | 1193 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1194 | `{` |
|      2 | 1195 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1196 | `	ph7_value_int(pVal,2);` |
|      5 | 1197 | `}` |
|      - | 1198 | `/*` |
|      - | 1199 | ` * PATHINFO_EXTENSION` |
|      - | 1200 | ` *  Expand 3.` |
|      - | 1201 | ` */` |
|   6526 | 1202 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1203 | `{` |
|   3263 | 1204 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6531 | 1205 | `	ph7_value_int(pVal,3);` |
|   6531 | 1206 | `}` |
|      - | 1207 | `/*` |
|      - | 1208 | ` * PATHINFO_FILENAME` |
|      - | 1209 | ` *  Expand 4.` |
|      - | 1210 | ` */` |
|   6518 | 1211 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1212 | `{` |
|   3259 | 1213 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6523 | 1214 | `	ph7_value_int(pVal,4);` |
|   6523 | 1215 | `}` |
|      - | 1216 | `/*` |
|      - | 1217 | ` * ASSERT_ACTIVE.` |
|      - | 1218 | ` *  PHP ASSERT_ACTIVE = 1` |
|      - | 1219 | ` */` |
|     14 | 1220 | `static void PH7_ASSERT_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1221 | `{` |
|      7 | 1222 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1223 | `	ph7_value_int(pVal,1); /* PHP ASSERT_ACTIVE = 1 */` |
|     16 | 1224 | `}` |
|      - | 1225 | `/*` |
|      - | 1226 | ` * ASSERT_CALLBACK.` |
|      - | 1227 | ` *  PHP ASSERT_CALLBACK = 2` |
|      - | 1228 | ` */` |
|      6 | 1229 | `static void PH7_ASSERT_CALLBACK_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1230 | `{` |
|      3 | 1231 | `	SXUNUSED(pUserData); /* cc warning */` |
|      8 | 1232 | `	ph7_value_int(pVal,2); /* PHP ASSERT_CALLBACK = 2 */` |
|      8 | 1233 | `}` |
|      - | 1234 | `/*` |
|      - | 1235 | ` * ASSERT_BAIL.` |
|      - | 1236 | ` *  PHP ASSERT_BAIL = 3` |
|      - | 1237 | ` */` |
|     14 | 1238 | `static void PH7_ASSERT_BAIL_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1239 | `{` |
|      7 | 1240 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1241 | `	ph7_value_int(pVal,3); /* PHP ASSERT_BAIL = 3 */` |
|     16 | 1242 | `}` |
|      - | 1243 | `/*` |
|      - | 1244 | ` * ASSERT_WARNING.` |
|      - | 1245 | ` *  PHP ASSERT_WARNING = 4 (deprecated in PHP 8.3)` |
|      - | 1246 | ` */` |
|      4 | 1247 | `static void PH7_ASSERT_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1248 | `{` |
|      2 | 1249 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1250 | `	ph7_value_int(pVal,4); /* PHP ASSERT_WARNING = 4 */` |
|      5 | 1251 | `}` |
|      - | 1252 | `/*` |
|      - | 1253 | ` * ASSERT_EXCEPTION.` |
|      - | 1254 | ` *  PHP ASSERT_EXCEPTION = 5 (deprecated in PHP 8.3)` |
|      - | 1255 | ` */` |
|      4 | 1256 | `static void PH7_ASSERT_EXCEPTION_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1257 | `{` |
|      2 | 1258 | `	SXUNUSED(pUserData); /* cc warning */` |
|      6 | 1259 | `	ph7_value_int(pVal,5); /* PHP ASSERT_EXCEPTION = 5 */` |
|      6 | 1260 | `}` |
|      - | 1261 | `/*` |
|      - | 1262 | ` * SEEK_SET.` |
|      - | 1263 | ` *  Expand 0` |
|      - | 1264 | ` */` |
|      2 | 1265 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1266 | `{` |
|      1 | 1267 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1268 | `	ph7_value_int(pVal,0);` |
|      3 | 1269 | `}` |
|      - | 1270 | `/*` |
|      - | 1271 | ` * SEEK_CUR.` |
|      - | 1272 | ` *  Expand 1` |
|      - | 1273 | ` */` |
|      2 | 1274 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1275 | `{` |
|      1 | 1276 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1277 | `	ph7_value_int(pVal,1);` |
|      3 | 1278 | `}` |
|      - | 1279 | `/*` |
|      - | 1280 | ` * SEEK_END.` |
|      - | 1281 | ` *  Expand 2` |
|      - | 1282 | ` */` |
|      4 | 1283 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1284 | `{` |
|      2 | 1285 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1286 | `	ph7_value_int(pVal,2);` |
|      5 | 1287 | `}` |
|      - | 1288 | `/*` |
|      - | 1289 | ` * LOCK_SH.` |
|      - | 1290 | ` *  Expand 2` |
|      - | 1291 | ` */` |
|      2 | 1292 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1293 | `{` |
|      1 | 1294 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1295 | `	ph7_value_int(pVal,1);` |
|      3 | 1296 | `}` |
|      - | 1297 | `/*` |
|      - | 1298 | ` * LOCK_NB.` |
|      - | 1299 | ` *  Expand 4 (php)` |
|      - | 1300 | ` */` |
|      2 | 1301 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1302 | `{` |
|      1 | 1303 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1304 | `	ph7_value_int(pVal,4);` |
|      3 | 1305 | `}` |
|      - | 1306 | `/*` |
|      - | 1307 | ` * LOCK_EX.` |
|      - | 1308 | ` *  Expand 2 (php). PH7 used 1, which collided with LOCK_SH, and LOCK_UN was 0 — so` |
|      - | 1309 | ` *  flock($h, LOCK_UN) asked the stream for a SHARED lock instead of releasing one.` |
|      - | 1310 | ` */` |
|      4 | 1311 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1312 | `{` |
|      2 | 1313 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1314 | `	ph7_value_int(pVal,2);` |
|      5 | 1315 | `}` |
|      - | 1316 | `/*` |
|      - | 1317 | ` * LOCK_UN.` |
|      - | 1318 | ` *  Expand 3 (php)` |
|      - | 1319 | ` */` |
|      4 | 1320 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1321 | `{` |
|      2 | 1322 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1323 | `	ph7_value_int(pVal,3);` |
|      5 | 1324 | `}` |
|      - | 1325 | `/*` |
|      - | 1326 | ` * FILE_USE_INCLUDE_PATH` |
|      - | 1327 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1328 | ` */` |
|      2 | 1329 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1330 | `{` |
|      1 | 1331 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1332 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1333 | `}` |
|      - | 1334 | `/*` |
|      - | 1335 | ` * FILE_IGNORE_NEW_LINES` |
|      - | 1336 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1337 | ` */` |
|      2 | 1338 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1339 | `{` |
|      1 | 1340 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1341 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1342 | `}` |
|      - | 1343 | `/*` |
|      - | 1344 | ` * FILE_SKIP_EMPTY_LINES` |
|      - | 1345 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1346 | ` */` |
|      2 | 1347 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1348 | `{` |
|      1 | 1349 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1350 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1351 | `}` |
|      - | 1352 | `/*` |
|      - | 1353 | ` * FILE_APPEND` |
|      - | 1354 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1355 | ` */` |
|      2 | 1356 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1357 | `{` |
|      1 | 1358 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1359 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1360 | `}` |
|      - | 1361 | `/*` |
|      - | 1362 | ` * SCANDIR_SORT_ASCENDING` |
|      - | 1363 | ` *  Expand 0` |
|      - | 1364 | ` */` |
|   1990 | 1365 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1366 | `{` |
|    995 | 1367 | `	SXUNUSED(pUserData); /* cc warning */` |
|   1995 | 1368 | `	ph7_value_int(pVal,0);` |
|   1995 | 1369 | `}` |
|      - | 1370 | `/*` |
|      - | 1371 | ` * SCANDIR_SORT_DESCENDING` |
|      - | 1372 | ` *  Expand 1` |
|      - | 1373 | ` */` |
|    996 | 1374 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1375 | `{` |
|    498 | 1376 | `	SXUNUSED(pUserData); /* cc warning */` |
|   1001 | 1377 | `	ph7_value_int(pVal,1);` |
|   1001 | 1378 | `}` |
|      - | 1379 | `/*` |
|      - | 1380 | ` * SCANDIR_SORT_NONE` |
|      - | 1381 | ` *  Expand 2` |
|      - | 1382 | ` */` |
|      2 | 1383 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1384 | `{` |
|      1 | 1385 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1386 | `	ph7_value_int(pVal,2);` |
|      3 | 1387 | `}` |
|      - | 1388 | `/*` |
|      - | 1389 | ` * GLOB_MARK` |
|      - | 1390 | ` *  Expand 0x01 (must be a power of two)` |
|      - | 1391 | ` */` |
|      2 | 1392 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1393 | `{` |
|      1 | 1394 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1395 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1396 | `}` |
|      - | 1397 | `/*` |
|      - | 1398 | ` * GLOB_NOSORT` |
|      - | 1399 | ` *  Expand 0x02 (must be a power of two)` |
|      - | 1400 | ` */` |
|      2 | 1401 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1402 | `{` |
|      1 | 1403 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1404 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1405 | `}` |
|      - | 1406 | `/*` |
|      - | 1407 | ` * GLOB_NOCHECK` |
|      - | 1408 | ` *  Expand 0x04 (must be a power of two)` |
|      - | 1409 | ` */` |
|      2 | 1410 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1411 | `{` |
|      1 | 1412 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1413 | `	ph7_value_int(pVal,0x04);` |
|      3 | 1414 | `}` |
|      - | 1415 | `/*` |
|      - | 1416 | ` * GLOB_NOESCAPE` |
|      - | 1417 | ` *  Expand 0x08 (must be a power of two)` |
|      - | 1418 | ` */` |
|      2 | 1419 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1420 | `{` |
|      1 | 1421 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1422 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1423 | `}` |
|      - | 1424 | `/*` |
|      - | 1425 | ` * GLOB_BRACE` |
|      - | 1426 | ` *  Expand 0x10 (must be a power of two)` |
|      - | 1427 | ` */` |
|      2 | 1428 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1429 | `{` |
|      1 | 1430 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1431 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1432 | `}` |
|      - | 1433 | `/*` |
|      - | 1434 | ` * GLOB_ONLYDIR` |
|      - | 1435 | ` *  Expand 0x20 (must be a power of two)` |
|      - | 1436 | ` */` |
|      2 | 1437 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1438 | `{` |
|      1 | 1439 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1440 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1441 | `}` |
|      - | 1442 | `/*` |
|      - | 1443 | ` * GLOB_ERR` |
|      - | 1444 | ` *  Expand 0x40 (must be a power of two)` |
|      - | 1445 | ` */` |
|      2 | 1446 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1447 | `{` |
|      1 | 1448 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1449 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1450 | `}` |
|      - | 1451 | `/*` |
|      - | 1452 | ` * STDIN` |
|      - | 1453 | ` *  Expand the STDIN handle as a resource.` |
|      - | 1454 | ` */` |
|      2 | 1455 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1456 | `{` |
|      3 | 1457 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1458 | `	void *pResource;` |
|      3 | 1459 | `	pResource = PH7_ExportStdin(pVm);` |
|      3 | 1460 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1461 | `}` |
|      - | 1462 | `/*` |
|      - | 1463 | ` * STDOUT` |
|      - | 1464 | ` *   Expand the STDOUT handle as a resource.` |
|      - | 1465 | ` */` |
|      2 | 1466 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1467 | `{` |
|      3 | 1468 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1469 | `	void *pResource;` |
|      3 | 1470 | `	pResource = PH7_ExportStdout(pVm);` |
|      3 | 1471 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1472 | `}` |
|      - | 1473 | `/*` |
|      - | 1474 | ` * STDERR` |
|      - | 1475 | ` *  Expand the STDERR handle as a resource.` |
|      - | 1476 | ` */` |
|      2 | 1477 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1478 | `{` |
|      3 | 1479 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1480 | `	void *pResource;` |
|      3 | 1481 | `	pResource = PH7_ExportStderr(pVm);` |
|      3 | 1482 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1483 | `}` |
|      - | 1484 | `/*` |
|      - | 1485 | ` * INI_SCANNER_NORMAL` |
|      - | 1486 | ` *   Expand 1` |
|      - | 1487 | ` */` |
|      2 | 1488 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1489 | `{` |
|      1 | 1490 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1491 | `	ph7_value_int(pVal,1);` |
|      3 | 1492 | `}` |
|      - | 1493 | `/*` |
|      - | 1494 | ` * INI_SCANNER_RAW` |
|      - | 1495 | ` *   Expand 2` |
|      - | 1496 | ` */` |
|      2 | 1497 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1498 | `{` |
|      1 | 1499 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1500 | `	ph7_value_int(pVal,2);` |
|      3 | 1501 | `}` |
|      - | 1502 | `/*` |
|      - | 1503 | ` * EXTR_OVERWRITE` |
|      - | 1504 | ` *   Expand 0x01 (Must be a power of two)` |
|      - | 1505 | ` */` |
|      2 | 1506 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1507 | `{` |
|      1 | 1508 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1509 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1510 | `}` |
|      - | 1511 | `/*` |
|      - | 1512 | ` * EXTR_SKIP` |
|      - | 1513 | ` *   Expand 0x02 (Must be a power of two)` |
|      - | 1514 | ` */` |
|      2 | 1515 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1516 | `{` |
|      1 | 1517 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1518 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1519 | `}` |
|      - | 1520 | `/*` |
|      - | 1521 | ` * EXTR_PREFIX_SAME` |
|      - | 1522 | ` *   Expand 0x04 (Must be a power of two)` |
|      - | 1523 | ` */` |
|      2 | 1524 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1525 | `{` |
|      1 | 1526 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1527 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1528 | `}` |
|      - | 1529 | `/*` |
|      - | 1530 | ` * EXTR_PREFIX_ALL` |
|      - | 1531 | ` *   Expand 0x08 (Must be a power of two)` |
|      - | 1532 | ` */` |
|      2 | 1533 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1534 | `{` |
|      1 | 1535 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1536 | `	ph7_value_int(pVal,0x8);` |
|      3 | 1537 | `}` |
|      - | 1538 | `/*` |
|      - | 1539 | ` * EXTR_PREFIX_INVALID` |
|      - | 1540 | ` *   Expand 0x10 (Must be a power of two)` |
|      - | 1541 | ` */` |
|      2 | 1542 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1543 | `{` |
|      1 | 1544 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1545 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1546 | `}` |
|      - | 1547 | `/*` |
|      - | 1548 | ` * EXTR_IF_EXISTS` |
|      - | 1549 | ` *   Expand 0x20 (Must be a power of two)` |
|      - | 1550 | ` */` |
|      2 | 1551 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1552 | `{` |
|      1 | 1553 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1554 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1555 | `}` |
|      - | 1556 | `/*` |
|      - | 1557 | ` * EXTR_PREFIX_IF_EXISTS` |
|      - | 1558 | ` *   Expand 0x40 (Must be a power of two)` |
|      - | 1559 | ` */` |
|      2 | 1560 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1561 | `{` |
|      1 | 1562 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1563 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1564 | `}` |
|      - | 1565 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1566 | `/*` |
|      - | 1567 | ` * XML_ERROR_NONE` |
|      - | 1568 | ` *   Expand the value of SXML_ERROR_NO_MEMORY defined in ph7Int.h` |
|      - | 1569 | ` */` |
|      2 | 1570 | `static void PH7_XML_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1571 | `{` |
|      1 | 1572 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1573 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1574 | `}` |
|      - | 1575 | `/*` |
|      - | 1576 | ` * XML_ERROR_NO_MEMORY` |
|      - | 1577 | ` *   Expand the value of SXML_ERROR_NONE defined in ph7Int.h` |
|      - | 1578 | ` */` |
|      2 | 1579 | `static void PH7_XML_ERROR_NO_MEMORY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1580 | `{` |
|      1 | 1581 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1582 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1583 | `}` |
|      - | 1584 | `/*` |
|      - | 1585 | ` * XML_ERROR_SYNTAX` |
|      - | 1586 | ` *   Expand the value of SXML_ERROR_SYNTAX defined in ph7Int.h` |
|      - | 1587 | ` */` |
|      2 | 1588 | `static void PH7_XML_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1589 | `{` |
|      1 | 1590 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1591 | `	ph7_value_int(pVal,SXML_ERROR_SYNTAX);` |
|      3 | 1592 | `}` |
|      - | 1593 | `/*` |
|      - | 1594 | ` * XML_ERROR_NO_ELEMENTS` |
|      - | 1595 | ` *   Expand the value of SXML_ERROR_NO_ELEMENTS defined in ph7Int.h` |
|      - | 1596 | ` */` |
|      2 | 1597 | `static void PH7_XML_ERROR_NO_ELEMENTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1598 | `{` |
|      1 | 1599 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1600 | `	ph7_value_int(pVal,SXML_ERROR_NO_ELEMENTS);` |
|      3 | 1601 | `}` |
|      - | 1602 | `/*` |
|      - | 1603 | ` * XML_ERROR_INVALID_TOKEN` |
|      - | 1604 | ` *   Expand the value of SXML_ERROR_INVALID_TOKEN defined in ph7Int.h` |
|      - | 1605 | ` */` |
|      2 | 1606 | `static void PH7_XML_ERROR_INVALID_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1607 | `{` |
|      1 | 1608 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1609 | `	ph7_value_int(pVal,SXML_ERROR_INVALID_TOKEN);` |
|      3 | 1610 | `}` |
|      - | 1611 | `/*` |
|      - | 1612 | ` * XML_ERROR_UNCLOSED_TOKEN` |
|      - | 1613 | ` *   Expand the value of SXML_ERROR_UNCLOSED_TOKEN defined in ph7Int.h` |
|      - | 1614 | ` */` |
|      2 | 1615 | `static void PH7_XML_ERROR_UNCLOSED_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1616 | `{` |
|      1 | 1617 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1618 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_TOKEN);` |
|      3 | 1619 | `}` |
|      - | 1620 | `/*` |
|      - | 1621 | ` * XML_ERROR_PARTIAL_CHAR` |
|      - | 1622 | ` *   Expand the value of SXML_ERROR_PARTIAL_CHAR defined in ph7Int.h` |
|      - | 1623 | ` */` |
|      2 | 1624 | `static void PH7_XML_ERROR_PARTIAL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1625 | `{` |
|      1 | 1626 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1627 | `	ph7_value_int(pVal,SXML_ERROR_PARTIAL_CHAR);` |
|      3 | 1628 | `}` |
|      - | 1629 | `/*` |
|      - | 1630 | ` * XML_ERROR_TAG_MISMATCH` |
|      - | 1631 | ` *   Expand the value of SXML_ERROR_TAG_MISMATCH defined in ph7Int.h` |
|      - | 1632 | ` */` |
|      2 | 1633 | `static void PH7_XML_ERROR_TAG_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1634 | `{` |
|      1 | 1635 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1636 | `	ph7_value_int(pVal,SXML_ERROR_TAG_MISMATCH);` |
|      3 | 1637 | `}` |
|      - | 1638 | `/*` |
|      - | 1639 | ` * XML_ERROR_DUPLICATE_ATTRIBUTE` |
|      - | 1640 | ` *   Expand the value of SXML_ERROR_DUPLICATE_ATTRIBUTE defined in ph7Int.h` |
|      - | 1641 | ` */` |
|      2 | 1642 | `static void PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1643 | `{` |
|      1 | 1644 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1645 | `	ph7_value_int(pVal,SXML_ERROR_DUPLICATE_ATTRIBUTE);` |
|      3 | 1646 | `}` |
|      - | 1647 | `/*` |
|      - | 1648 | ` * XML_ERROR_JUNK_AFTER_DOC_ELEMENT` |
|      - | 1649 | ` *   Expand the value of SXML_ERROR_JUNK_AFTER_DOC_ELEMENT defined in ph7Int.h` |
|      - | 1650 | ` */` |
|      2 | 1651 | `static void PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1652 | `{` |
|      1 | 1653 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1654 | `	ph7_value_int(pVal,SXML_ERROR_JUNK_AFTER_DOC_ELEMENT);` |
|      3 | 1655 | `}` |
|      - | 1656 | `/*` |
|      - | 1657 | ` * XML_ERROR_PARAM_ENTITY_REF` |
|      - | 1658 | ` *   Expand the value of SXML_ERROR_PARAM_ENTITY_REF defined in ph7Int.h` |
|      - | 1659 | ` */` |
|      2 | 1660 | `static void PH7_XML_ERROR_PARAM_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1661 | `{` |
|      1 | 1662 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1663 | `	ph7_value_int(pVal,SXML_ERROR_PARAM_ENTITY_REF);` |
|      3 | 1664 | `}` |
|      - | 1665 | `/*` |
|      - | 1666 | ` * XML_ERROR_UNDEFINED_ENTITY` |
|      - | 1667 | ` *   Expand the value of SXML_ERROR_UNDEFINED_ENTITY defined in ph7Int.h` |
|      - | 1668 | ` */` |
|      2 | 1669 | `static void PH7_XML_ERROR_UNDEFINED_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1670 | `{` |
|      1 | 1671 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1672 | `	ph7_value_int(pVal,SXML_ERROR_UNDEFINED_ENTITY);` |
|      3 | 1673 | `}` |
|      - | 1674 | `/*` |
|      - | 1675 | ` * XML_ERROR_RECURSIVE_ENTITY_REF` |
|      - | 1676 | ` *   Expand the value of SXML_ERROR_RECURSIVE_ENTITY_REF defined in ph7Int.h` |
|      - | 1677 | ` */` |
|      2 | 1678 | `static void PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1679 | `{` |
|      1 | 1680 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1681 | `	ph7_value_int(pVal,SXML_ERROR_RECURSIVE_ENTITY_REF);` |
|      3 | 1682 | `}` |
|      - | 1683 | `/*` |
|      - | 1684 | ` * XML_ERROR_ASYNC_ENTITY` |
|      - | 1685 | ` *   Expand the value of SXML_ERROR_ASYNC_ENTITY defined in ph7Int.h` |
|      - | 1686 | ` */` |
|      2 | 1687 | `static void PH7_XML_ERROR_ASYNC_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1688 | `{` |
|      1 | 1689 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1690 | `	ph7_value_int(pVal,SXML_ERROR_ASYNC_ENTITY);` |
|      3 | 1691 | `}` |
|      - | 1692 | `/*` |
|      - | 1693 | ` * XML_ERROR_BAD_CHAR_REF` |
|      - | 1694 | ` *   Expand the value of SXML_ERROR_BAD_CHAR_REF defined in ph7Int.h` |
|      - | 1695 | ` */` |
|      2 | 1696 | `static void PH7_XML_ERROR_BAD_CHAR_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1697 | `{` |
|      1 | 1698 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1699 | `	ph7_value_int(pVal,SXML_ERROR_BAD_CHAR_REF);` |
|      3 | 1700 | `}` |
|      - | 1701 | `/*` |
|      - | 1702 | ` * XML_ERROR_BINARY_ENTITY_REF` |
|      - | 1703 | ` *   Expand the value of SXML_ERROR_BINARY_ENTITY_REF defined in ph7Int.h` |
|      - | 1704 | ` */` |
|      2 | 1705 | `static void PH7_XML_ERROR_BINARY_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1706 | `{` |
|      1 | 1707 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1708 | `	ph7_value_int(pVal,SXML_ERROR_BINARY_ENTITY_REF);` |
|      3 | 1709 | `}` |
|      - | 1710 | `/*` |
|      - | 1711 | ` * XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF` |
|      - | 1712 | ` *   Expand the value of SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF defined in ph7Int.h` |
|      - | 1713 | ` */` |
|      2 | 1714 | `static void PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1715 | `{` |
|      1 | 1716 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1717 | `	ph7_value_int(pVal,SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF);` |
|      3 | 1718 | `}` |
|      - | 1719 | `/*` |
|      - | 1720 | ` * XML_ERROR_MISPLACED_XML_PI` |
|      - | 1721 | ` *   Expand the value of SXML_ERROR_MISPLACED_XML_PI defined in ph7Int.h` |
|      - | 1722 | ` */` |
|      2 | 1723 | `static void PH7_XML_ERROR_MISPLACED_XML_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1724 | `{` |
|      1 | 1725 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1726 | `	ph7_value_int(pVal,SXML_ERROR_MISPLACED_XML_PI);` |
|      3 | 1727 | `}` |
|      - | 1728 | `/*` |
|      - | 1729 | ` * XML_ERROR_UNKNOWN_ENCODING` |
|      - | 1730 | ` *   Expand the value of SXML_ERROR_UNKNOWN_ENCODING defined in ph7Int.h` |
|      - | 1731 | ` */` |
|      2 | 1732 | `static void PH7_XML_ERROR_UNKNOWN_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1733 | `{` |
|      1 | 1734 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1735 | `	ph7_value_int(pVal,SXML_ERROR_UNKNOWN_ENCODING);` |
|      3 | 1736 | `}` |
|      - | 1737 | `/*` |
|      - | 1738 | ` * XML_ERROR_INCORRECT_ENCODING` |
|      - | 1739 | ` *   Expand the value of SXML_ERROR_INCORRECT_ENCODING defined in ph7Int.h` |
|      - | 1740 | ` */` |
|      2 | 1741 | `static void PH7_XML_ERROR_INCORRECT_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1742 | `{` |
|      1 | 1743 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1744 | `	ph7_value_int(pVal,SXML_ERROR_INCORRECT_ENCODING);` |
|      3 | 1745 | `}` |
|      - | 1746 | `/*` |
|      - | 1747 | ` * XML_ERROR_UNCLOSED_CDATA_SECTION` |
|      - | 1748 | ` *   Expand the value of SXML_ERROR_UNCLOSED_CDATA_SECTION defined in ph7Int.h` |
|      - | 1749 | ` */` |
|      2 | 1750 | `static void PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1751 | `{` |
|      1 | 1752 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1753 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_CDATA_SECTION);` |
|      3 | 1754 | `}` |
|      - | 1755 | `/*` |
|      - | 1756 | ` * XML_ERROR_EXTERNAL_ENTITY_HANDLING` |
|      - | 1757 | ` *   Expand the value of SXML_ERROR_EXTERNAL_ENTITY_HANDLING defined in ph7Int.h` |
|      - | 1758 | ` */` |
|      2 | 1759 | `static void PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1760 | `{` |
|      1 | 1761 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1762 | `	ph7_value_int(pVal,SXML_ERROR_EXTERNAL_ENTITY_HANDLING);` |
|      3 | 1763 | `}` |
|      - | 1764 | `/*` |
|      - | 1765 | ` * XML_OPTION_CASE_FOLDING` |
|      - | 1766 | ` *   Expand the value of SXML_OPTION_CASE_FOLDING defined in ph7Int.h.` |
|      - | 1767 | ` */` |
|      2 | 1768 | `static void PH7_XML_OPTION_CASE_FOLDING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1769 | `{` |
|      1 | 1770 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1771 | `	ph7_value_int(pVal,SXML_OPTION_CASE_FOLDING);` |
|      3 | 1772 | `}` |
|      - | 1773 | `/*` |
|      - | 1774 | ` * XML_OPTION_TARGET_ENCODING` |
|      - | 1775 | ` *   Expand the value of SXML_OPTION_TARGET_ENCODING defined in ph7Int.h.` |
|      - | 1776 | ` */` |
|      4 | 1777 | `static void PH7_XML_OPTION_TARGET_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1778 | `{` |
|      2 | 1779 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1780 | `	ph7_value_int(pVal,SXML_OPTION_TARGET_ENCODING);` |
|      5 | 1781 | `}` |
|      - | 1782 | `/*` |
|      - | 1783 | ` * XML_OPTION_SKIP_TAGSTART` |
|      - | 1784 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1785 | ` */` |
|      2 | 1786 | `static void PH7_XML_OPTION_SKIP_TAGSTART_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1787 | `{` |
|      1 | 1788 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1789 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_TAGSTART);` |
|      3 | 1790 | `}` |
|      - | 1791 | `/*` |
|      - | 1792 | ` * XML_OPTION_SKIP_WHITE` |
|      - | 1793 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1794 | ` */` |
|      4 | 1795 | `static void PH7_XML_OPTION_SKIP_WHITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1796 | `{` |
|      2 | 1797 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1798 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_WHITE);` |
|      5 | 1799 | `}` |
|      - | 1800 | `/*` |
|      - | 1801 | ` * XML_SAX_IMPL.` |
|      - | 1802 | ` *   Expand the name of the underlying XML engine.` |
|      - | 1803 | ` */` |
|      2 | 1804 | `static void PH7_XML_SAX_IMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1805 | `{` |
|      1 | 1806 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1807 | `	ph7_value_string(pVal,"Symisc XML engine",(int)sizeof("Symisc XML engine")-1);` |
|      3 | 1808 | `}` |
|      - | 1809 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1810 | `/*` |
|      - | 1811 | ` * JSON_HEX_TAG.` |
|      - | 1812 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|      - | 1813 | ` */` |
|      2 | 1814 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1815 | `{` |
|      1 | 1816 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1817 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      3 | 1818 | `}` |
|      - | 1819 | `/*` |
|      - | 1820 | ` * JSON_HEX_AMP.` |
|      - | 1821 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|      - | 1822 | ` */` |
|      2 | 1823 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1824 | `{` |
|      1 | 1825 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1826 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      3 | 1827 | `}` |
|      - | 1828 | `/*` |
|      - | 1829 | ` * JSON_HEX_APOS.` |
|      - | 1830 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|      - | 1831 | ` */` |
|      2 | 1832 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1833 | `{` |
|      1 | 1834 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1835 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      3 | 1836 | `}` |
|      - | 1837 | `/*` |
|      - | 1838 | ` * JSON_HEX_QUOT.` |
|      - | 1839 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|      - | 1840 | ` */` |
|      2 | 1841 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1842 | `{` |
|      1 | 1843 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1844 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      3 | 1845 | `}` |
|      - | 1846 | `/*` |
|      - | 1847 | ` * JSON_FORCE_OBJECT.` |
|      - | 1848 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|      - | 1849 | ` */` |
|      4 | 1850 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1851 | `{` |
|      2 | 1852 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1853 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      5 | 1854 | `}` |
|      - | 1855 | `/*` |
|      - | 1856 | ` * JSON_NUMERIC_CHECK.` |
|      - | 1857 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|      - | 1858 | ` */` |
|      4 | 1859 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1860 | `{` |
|      2 | 1861 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1862 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      5 | 1863 | `}` |
|      - | 1864 | `/*` |
|      - | 1865 | ` * JSON_BIGINT_AS_STRING.` |
|      - | 1866 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|      - | 1867 | ` */` |
|      2 | 1868 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1869 | `{` |
|      1 | 1870 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1871 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      3 | 1872 | `}` |
|      - | 1873 | `/*` |
|      - | 1874 | ` * JSON_PRETTY_PRINT.` |
|      - | 1875 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|      - | 1876 | ` */` |
|      2 | 1877 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1878 | `{` |
|      1 | 1879 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1880 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      3 | 1881 | `}` |
|      - | 1882 | `/*` |
|      - | 1883 | ` * JSON_UNESCAPED_SLASHES.` |
|      - | 1884 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|      - | 1885 | ` */` |
|      4 | 1886 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1887 | `{` |
|      2 | 1888 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1889 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      5 | 1890 | `}` |
|      - | 1891 | `/*` |
|      - | 1892 | ` * JSON_UNESCAPED_UNICODE.` |
|      - | 1893 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|      - | 1894 | ` */` |
|      2 | 1895 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1896 | `{` |
|      1 | 1897 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1898 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|      3 | 1899 | `}` |
|      - | 1900 | `/*` |
|      - | 1901 | ` * JSON_ERROR_NONE.` |
|      - | 1902 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|      - | 1903 | ` */` |
|      4 | 1904 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1905 | `{` |
|      2 | 1906 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1907 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      5 | 1908 | `}` |
|      - | 1909 | `/*` |
|      - | 1910 | ` * JSON_ERROR_DEPTH.` |
|      - | 1911 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|      - | 1912 | ` */` |
|      2 | 1913 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1914 | `{` |
|      1 | 1915 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1916 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      3 | 1917 | `}` |
|      - | 1918 | `/*` |
|      - | 1919 | ` * JSON_ERROR_STATE_MISMATCH.` |
|      - | 1920 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|      - | 1921 | ` */` |
|      2 | 1922 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1923 | `{` |
|      1 | 1924 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1925 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      3 | 1926 | `}` |
|      - | 1927 | `/*` |
|      - | 1928 | ` * JSON_ERROR_CTRL_CHAR.` |
|      - | 1929 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|      - | 1930 | ` */` |
|      2 | 1931 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1932 | `{` |
|      1 | 1933 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1934 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      3 | 1935 | `}` |
|      - | 1936 | `/*` |
|      - | 1937 | ` * JSON_ERROR_SYNTAX.` |
|      - | 1938 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|      - | 1939 | ` */` |
|      4 | 1940 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1941 | `{` |
|      2 | 1942 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1943 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      5 | 1944 | `}` |
|      - | 1945 | `/*` |
|      - | 1946 | ` * JSON_ERROR_UTF8.` |
|      - | 1947 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|      - | 1948 | ` */` |
|      2 | 1949 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1950 | `{` |
|      1 | 1951 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1952 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      3 | 1953 | `}` |
|      - | 1954 | `/*` |
|      - | 1955 | ` * JSON_ERROR_NON_BACKED_ENUM.` |
|      - | 1956 | ` *   Expand the value of JSON_ERROR_NON_BACKED_ENUM defined in ph7Int.h (php 8.1).` |
|      - | 1957 | ` */` |
|    ! 0 | 1958 | `static void PH7_JSON_ERROR_NON_BACKED_ENUM_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 1959 | `{` |
|    ! 0 | 1960 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 1961 | `	ph7_value_int(pVal,JSON_ERROR_NON_BACKED_ENUM);` |
|    ! 0 | 1962 | `}` |
|      - | 1963 | `/*` |
|      - | 1964 | ` * __CLASS__` |
|      - | 1965 | ` *  The current class name, or the EMPTY STRING outside any class — php answers "",` |
|      - | 1966 | `` *  not null (`__CLASS__ === ""` is true in global scope). `self` keeps its own`` |
|      - | 1967 | ` *  expander below because php treats IT differently outside a class scope.` |
|      - | 1968 | ` */` |
|      2 | 1969 | `static void PH7_class_magic_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1970 | `{` |
|      3 | 1971 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1972 | `	ph7_class *pClass;` |
|      3 | 1973 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      3 | 1974 | `	if( pClass == 0 ){` |
|      3 | 1975 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      1 | 1976 | `	}` |
|      3 | 1977 | `	if( pClass ){` |
|    ! 0 | 1978 | `		SyString *pName = &pClass->sName;` |
|    ! 0 | 1979 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|    ! 0 | 1980 | `	}else{` |
|      3 | 1981 | `		ph7_value_string(pVal,"",0);` |
|      - | 1982 | `	}` |
|      3 | 1983 | `}` |
|      - | 1984 |  |
|      - | 1985 | `/*` |
|      - | 1986 | ` * PASSWORD_BCRYPT / PASSWORD_DEFAULT` |
|      - | 1987 | ` *  The bcrypt algorithm identifier (PHP 7.4+ exposes these as the string "2y").` |
|      - | 1988 | ` *  PASSWORD_DEFAULT tracks the recommended default, currently bcrypt.` |
|      - | 1989 | ` */` |
|     20 | 1990 | `static void PH7_PASSWORD_BCRYPT_Const(ph7_value *pVal,void *pUnused)` |
|      2 | 1991 | `{` |
|     10 | 1992 | `	SXUNUSED(pUnused);` |
|     22 | 1993 | `	ph7_value_string(pVal,"2y",(int)sizeof("2y")-1);` |
|     22 | 1994 | `}` |
|      - | 1995 | `/*` |
|      - | 1996 | ` * PASSWORD_BCRYPT_DEFAULT_COST` |
|      - | 1997 | ` *  The default bcrypt work factor used by password_hash() (currently 12).` |
|      - | 1998 | ` */` |
|      2 | 1999 | `static void PH7_PASSWORD_COST_Const(ph7_value *pVal,void *pUnused)` |
|      1 | 2000 | `{` |
|      1 | 2001 | `	SXUNUSED(pUnused);` |
|      3 | 2002 | `	ph7_value_int(pVal,12);` |
|      3 | 2003 | `}` |
|      - | 2004 | `/*` |
|      - | 2005 | ` * filter_var() filter and flag identifiers (the ext/filter constants). Values` |
|      - | 2006 | ` * match PHP 8.5. One tiny int-returning callback per constant, generated by a` |
|      - | 2007 | ` * local macro to keep the ~25 near-identical definitions DRY.` |
|      - | 2008 | ` */` |
|      - | 2009 | `#define PH7_FILTER_INT_CONST(Name,Val) \` |
|      - | 2010 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|      - | 2011 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|      - | 2012 | `	}` |
|     10 | 2013 | `PH7_FILTER_INT_CONST(FILTER_DEFAULT,516)` |
|     17 | 2014 | `PH7_FILTER_INT_CONST(FILTER_UNSAFE_RAW,516)` |
|     64 | 2015 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_INT,257)` |
|     29 | 2016 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_BOOLEAN,258)` |
|     69 | 2017 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_FLOAT,259)` |
|      8 | 2018 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_REGEXP,272)` |
|     11 | 2019 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_DOMAIN,277)` |
|     15 | 2020 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_URL,273)` |
|     28 | 2021 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_EMAIL,274)` |
|     25 | 2022 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_IP,275)` |
|     11 | 2023 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_MAC,276)` |
|      3 | 2024 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_INT,519)` |
|      5 | 2025 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_FLOAT,520)` |
|     13 | 2026 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_SPECIAL_CHARS,515)` |
|     25 | 2027 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_FULL_SPECIAL_CHARS,522)` |
|      3 | 2028 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_EMAIL,517)` |
|      3 | 2029 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_URL,518)` |
|      3 | 2030 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_OCTAL,1)` |
|      3 | 2031 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_HEX,2)` |
|      7 | 2032 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_LOW,4)` |
|      5 | 2033 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_HIGH,8)` |
|      5 | 2034 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_LOW,16)` |
|      5 | 2035 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_HIGH,32)` |
|      3 | 2036 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_AMP,64)` |
|      3 | 2037 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_ENCODE_QUOTES,128)` |
|      3 | 2038 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_BACKTICK,512)` |
|      3 | 2039 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_FRACTION,4096)` |
|     25 | 2040 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_THOUSAND,8192)` |
|      3 | 2041 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_SCIENTIFIC,16384)` |
|      5 | 2042 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV4,1048576)` |
|      3 | 2043 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV6,2097152)` |
|     14 | 2044 | `PH7_FILTER_INT_CONST(FILTER_NULL_ON_FAILURE,134217728)` |
|      - | 2045 | `/* filter_input() source selectors (php values; SESSION/REQUEST are undefined in 8.5) */` |
|      5 | 2046 | `PH7_FILTER_INT_CONST(INPUT_POST,0)` |
|      8 | 2047 | `PH7_FILTER_INT_CONST(INPUT_GET,1)` |
|      3 | 2048 | `PH7_FILTER_INT_CONST(INPUT_COOKIE,2)` |
|      3 | 2049 | `PH7_FILTER_INT_CONST(INPUT_ENV,4)` |
|     21 | 2050 | `PH7_FILTER_INT_CONST(INPUT_SERVER,5)` |
|      - | 2051 | `/*` |
|      - | 2052 | ` * Table of built-in constants.` |
|      - | 2053 | ` */` |
|      - | 2054 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|      - | 2055 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|      - | 2056 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|      - | 2057 | `	{"__PH7__",              PH7_VER_Const      },` |
|      - | 2058 | `	{"PHP_VERSION",          PH7_PHPVerConst    },` |
|      - | 2059 | `	{"PHP_MAJOR_VERSION",    PH7_PHPMajorConst  },` |
|      - | 2060 | `	{"PHP_MINOR_VERSION",    PH7_PHPMinorConst  },` |
|      - | 2061 | `	{"PHP_RELEASE_VERSION",  PH7_PHPReleaseConst},` |
|      - | 2062 | `	{"PHP_EXTRA_VERSION",    PH7_PHPExtraConst  },` |
|      - | 2063 | `	{"PHP_VERSION_ID",       PH7_PHPVerIdConst  },` |
|      - | 2064 | `	{"PHP_OS",               PH7_OS_Const       },` |
|      - | 2065 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|      - | 2066 | `	{"PHP_SESSION_DISABLED", PH7_PHP_SESSION_DISABLED_Const },` |
|      - | 2067 | `	{"PHP_SESSION_NONE",     PH7_PHP_SESSION_NONE_Const },` |
|      - | 2068 | `	{"PHP_SESSION_ACTIVE",   PH7_PHP_SESSION_ACTIVE_Const },` |
|      - | 2069 | `	{"INI_USER",             PH7_INI_USER_Const },` |
|      - | 2070 | `	{"INI_PERDIR",           PH7_INI_PERDIR_Const },` |
|      - | 2071 | `	{"INI_SYSTEM",           PH7_INI_SYSTEM_Const },` |
|      - | 2072 | `	{"INI_ALL",              PH7_INI_ALL_Const },` |
|      - | 2073 | `	{"MB_CASE_UPPER",        PH7_MB_CASE_UPPER_Const },` |
|      - | 2074 | `	{"MB_CASE_LOWER",        PH7_MB_CASE_LOWER_Const },` |
|      - | 2075 | `	{"MB_CASE_TITLE",        PH7_MB_CASE_TITLE_Const },` |
|      - | 2076 | `	{"PASSWORD_BCRYPT",      PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2077 | `	{"PASSWORD_DEFAULT",     PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2078 | `	{"PASSWORD_BCRYPT_DEFAULT_COST", PH7_PASSWORD_COST_Const },` |
|      - | 2079 | `	{"FILTER_DEFAULT",              PH7_FILTER_DEFAULT_Const },` |
|      - | 2080 | `	{"FILTER_UNSAFE_RAW",           PH7_FILTER_UNSAFE_RAW_Const },` |
|      - | 2081 | `	{"FILTER_VALIDATE_INT",         PH7_FILTER_VALIDATE_INT_Const },` |
|      - | 2082 | `	{"FILTER_VALIDATE_BOOLEAN",     PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2083 | `	{"FILTER_VALIDATE_BOOL",        PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2084 | `	{"FILTER_VALIDATE_FLOAT",       PH7_FILTER_VALIDATE_FLOAT_Const },` |
|      - | 2085 | `	{"FILTER_VALIDATE_REGEXP",      PH7_FILTER_VALIDATE_REGEXP_Const },` |
|      - | 2086 | `	{"FILTER_VALIDATE_DOMAIN",      PH7_FILTER_VALIDATE_DOMAIN_Const },` |
|      - | 2087 | `	{"FILTER_VALIDATE_URL",         PH7_FILTER_VALIDATE_URL_Const },` |
|      - | 2088 | `	{"FILTER_VALIDATE_EMAIL",       PH7_FILTER_VALIDATE_EMAIL_Const },` |
|      - | 2089 | `	{"FILTER_VALIDATE_IP",          PH7_FILTER_VALIDATE_IP_Const },` |
|      - | 2090 | `	{"FILTER_VALIDATE_MAC",         PH7_FILTER_VALIDATE_MAC_Const },` |
|      - | 2091 | `	{"FILTER_SANITIZE_NUMBER_INT",  PH7_FILTER_SANITIZE_NUMBER_INT_Const },` |
|      - | 2092 | `	{"FILTER_SANITIZE_NUMBER_FLOAT",PH7_FILTER_SANITIZE_NUMBER_FLOAT_Const },` |
|      - | 2093 | `	{"FILTER_SANITIZE_SPECIAL_CHARS",PH7_FILTER_SANITIZE_SPECIAL_CHARS_Const },` |
|      - | 2094 | `	{"FILTER_SANITIZE_FULL_SPECIAL_CHARS",PH7_FILTER_SANITIZE_FULL_SPECIAL_CHARS_Const },` |
|      - | 2095 | `	{"FILTER_SANITIZE_EMAIL",       PH7_FILTER_SANITIZE_EMAIL_Const },` |
|      - | 2096 | `	{"FILTER_SANITIZE_URL",         PH7_FILTER_SANITIZE_URL_Const },` |
|      - | 2097 | `	{"FILTER_FLAG_ALLOW_OCTAL",     PH7_FILTER_FLAG_ALLOW_OCTAL_Const },` |
|      - | 2098 | `	{"FILTER_FLAG_ALLOW_HEX",       PH7_FILTER_FLAG_ALLOW_HEX_Const },` |
|      - | 2099 | `	{"FILTER_FLAG_STRIP_LOW",       PH7_FILTER_FLAG_STRIP_LOW_Const },` |
|      - | 2100 | `	{"FILTER_FLAG_STRIP_HIGH",      PH7_FILTER_FLAG_STRIP_HIGH_Const },` |
|      - | 2101 | `	{"FILTER_FLAG_ENCODE_LOW",      PH7_FILTER_FLAG_ENCODE_LOW_Const },` |
|      - | 2102 | `	{"FILTER_FLAG_ENCODE_HIGH",     PH7_FILTER_FLAG_ENCODE_HIGH_Const },` |
|      - | 2103 | `	{"FILTER_FLAG_ENCODE_AMP",      PH7_FILTER_FLAG_ENCODE_AMP_Const },` |
|      - | 2104 | `	{"FILTER_FLAG_NO_ENCODE_QUOTES",PH7_FILTER_FLAG_NO_ENCODE_QUOTES_Const },` |
|      - | 2105 | `	{"FILTER_FLAG_STRIP_BACKTICK",  PH7_FILTER_FLAG_STRIP_BACKTICK_Const },` |
|      - | 2106 | `	{"FILTER_FLAG_ALLOW_FRACTION",  PH7_FILTER_FLAG_ALLOW_FRACTION_Const },` |
|      - | 2107 | `	{"FILTER_FLAG_ALLOW_THOUSAND",  PH7_FILTER_FLAG_ALLOW_THOUSAND_Const },` |
|      - | 2108 | `	{"FILTER_FLAG_ALLOW_SCIENTIFIC",PH7_FILTER_FLAG_ALLOW_SCIENTIFIC_Const },` |
|      - | 2109 | `	{"FILTER_FLAG_IPV4",            PH7_FILTER_FLAG_IPV4_Const },` |
|      - | 2110 | `	{"FILTER_FLAG_IPV6",            PH7_FILTER_FLAG_IPV6_Const },` |
|      - | 2111 | `	{"FILTER_NULL_ON_FAILURE",      PH7_FILTER_NULL_ON_FAILURE_Const },` |
|      - | 2112 | `	{"INPUT_POST",                  PH7_INPUT_POST_Const },` |
|      - | 2113 | `	{"INPUT_GET",                   PH7_INPUT_GET_Const },` |
|      - | 2114 | `	{"INPUT_COOKIE",                PH7_INPUT_COOKIE_Const },` |
|      - | 2115 | `	{"INPUT_ENV",                   PH7_INPUT_ENV_Const },` |
|      - | 2116 | `	{"INPUT_SERVER",                PH7_INPUT_SERVER_Const },` |
|      - | 2117 | `	{"CAL_GREGORIAN",        PH7_CAL_GREGORIAN_Const },` |
|      - | 2118 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|      - | 2119 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|      - | 2120 | `	{"PHP_INT_MIN",          PH7_INTMIN_Const   },` |
|      - | 2121 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|      - | 2122 | `	{"PHP_FLOAT_EPSILON",    PH7_FLOATEPSILON_Const },` |
|      - | 2123 | `	{"PHP_FLOAT_MAX",        PH7_FLOATMAX_Const },` |
|      - | 2124 | `	{"PHP_FLOAT_MIN",        PH7_FLOATMIN_Const },` |
|      - | 2125 | `	{"PHP_FLOAT_DIG",        PH7_FLOATDIG_Const },` |
|      - | 2126 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|      - | 2127 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|      - | 2128 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|      - | 2129 | `	{"__TIME__",             PH7_TIME_Const     },` |
|      - | 2130 | `	{"__DATE__",             PH7_DATE_Const     },` |
|      - | 2131 | `	{"__FILE__",             PH7_FILE_Const     },` |
|      - | 2132 | `	{"__DIR__",              PH7_DIR_Const      },` |
|      - | 2133 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|      - | 2134 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|      - | 2135 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|      - | 2136 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|      - | 2137 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|      - | 2138 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|      - | 2139 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|      - | 2140 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|      - | 2141 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|      - | 2142 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|      - | 2143 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|      - | 2144 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|      - | 2145 | `	{"E_STRICT",             PH7_E_STRICT_Const        },` |
|      - | 2146 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|      - | 2147 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|      - | 2148 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|      - | 2149 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|      - | 2150 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|      - | 2151 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|      - | 2152 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|      - | 2153 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|      - | 2154 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|      - | 2155 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|      - | 2156 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|      - | 2157 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|      - | 2158 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|      - | 2159 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|      - | 2160 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|      - | 2161 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|      - | 2162 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|      - | 2163 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|      - | 2164 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|      - | 2165 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|      - | 2166 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|      - | 2167 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|      - | 2168 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 2169 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|      - | 2170 | `	{"M_E",                  PH7_M_E_Const          },` |
|      - | 2171 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|      - | 2172 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|      - | 2173 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|      - | 2174 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|      - | 2175 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|      - | 2176 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|      - | 2177 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|      - | 2178 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|      - | 2179 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|      - | 2180 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|      - | 2181 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|      - | 2182 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|      - | 2183 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|      - | 2184 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|      - | 2185 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|      - | 2186 | `	{"NAN",                  PH7_NAN_Const          },` |
|      - | 2187 | `	{"INF",                  PH7_INF_Const          },` |
|      - | 2188 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 2189 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|      - | 2190 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|      - | 2191 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|      - | 2192 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|      - | 2193 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|      - | 2194 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|      - | 2195 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|      - | 2196 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|      - | 2197 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|      - | 2198 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|      - | 2199 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|      - | 2200 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|      - | 2201 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|      - | 2202 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|      - | 2203 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|      - | 2204 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|      - | 2205 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|      - | 2206 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|      - | 2207 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|      - | 2208 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|      - | 2209 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|      - | 2210 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|      - | 2211 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|      - | 2212 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|      - | 2213 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|      - | 2214 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|      - | 2215 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|      - | 2216 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|      - | 2217 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|      - | 2218 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|      - | 2219 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|      - | 2220 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|      - | 2221 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|      - | 2222 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|      - | 2223 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|      - | 2224 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|      - | 2225 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|      - | 2226 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|      - | 2227 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|      - | 2228 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|      - | 2229 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|      - | 2230 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|      - | 2231 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|      - | 2232 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|      - | 2233 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|      - | 2234 | `	{"ASSERT_ACTIVE",        PH7_ASSERT_ACTIVE_Const     },` |
|      - | 2235 | `	{"ASSERT_CALLBACK",      PH7_ASSERT_CALLBACK_Const   },` |
|      - | 2236 | `	{"ASSERT_BAIL",          PH7_ASSERT_BAIL_Const       },` |
|      - | 2237 | `	{"ASSERT_WARNING",       PH7_ASSERT_WARNING_Const    },` |
|      - | 2238 | `	{"ASSERT_EXCEPTION",     PH7_ASSERT_EXCEPTION_Const  },` |
|      - | 2239 | `	/* ASSERT_QUIET_EVAL was REMOVED in php 8.0: referencing it is an Error there */` |
|      - | 2240 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|      - | 2241 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|      - | 2242 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|      - | 2243 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|      - | 2244 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|      - | 2245 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|      - | 2246 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|      - | 2247 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|      - | 2248 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|      - | 2249 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|      - | 2250 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|      - | 2251 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|      - | 2252 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|      - | 2253 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|      - | 2254 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|      - | 2255 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|      - | 2256 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|      - | 2257 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|      - | 2258 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|      - | 2259 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|      - | 2260 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|      - | 2261 | `	{"STDIN",                PH7_STDIN_Const        },` |
|      - | 2262 | `	{"stdin",                PH7_STDIN_Const        },` |
|      - | 2263 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|      - | 2264 | `	{"stdout",               PH7_STDOUT_Const       },` |
|      - | 2265 | `	{"STDERR",               PH7_STDERR_Const       },` |
|      - | 2266 | `	{"stderr",               PH7_STDERR_Const       },` |
|      - | 2267 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|      - | 2268 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|      - | 2269 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|      - | 2270 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|      - | 2271 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|      - | 2272 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|      - | 2273 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|      - | 2274 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|      - | 2275 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|      - | 2276 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2277 | `	{"XML_ERROR_NONE",       PH7_XML_ERROR_NONE_Const},` |
|      - | 2278 | `	{"XML_ERROR_NO_MEMORY",  PH7_XML_ERROR_NO_MEMORY_Const},` |
|      - | 2279 | `	{"XML_ERROR_SYNTAX",     PH7_XML_ERROR_SYNTAX_Const},` |
|      - | 2280 | `	{"XML_ERROR_NO_ELEMENTS",PH7_XML_ERROR_NO_ELEMENTS_Const},` |
|      - | 2281 | `	{"XML_ERROR_INVALID_TOKEN", PH7_XML_ERROR_INVALID_TOKEN_Const},` |
|      - | 2282 | `	{"XML_ERROR_UNCLOSED_TOKEN",PH7_XML_ERROR_UNCLOSED_TOKEN_Const},` |
|      - | 2283 | `	{"XML_ERROR_PARTIAL_CHAR",  PH7_XML_ERROR_PARTIAL_CHAR_Const},` |
|      - | 2284 | `	{"XML_ERROR_TAG_MISMATCH",  PH7_XML_ERROR_TAG_MISMATCH_Const},` |
|      - | 2285 | `	{"XML_ERROR_DUPLICATE_ATTRIBUTE",   PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const},` |
|      - | 2286 | `	{"XML_ERROR_JUNK_AFTER_DOC_ELEMENT",PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const},` |
|      - | 2287 | `	{"XML_ERROR_PARAM_ENTITY_REF",      PH7_XML_ERROR_PARAM_ENTITY_REF_Const},` |
|      - | 2288 | `	{"XML_ERROR_UNDEFINED_ENTITY",      PH7_XML_ERROR_UNDEFINED_ENTITY_Const},` |
|      - | 2289 | `	{"XML_ERROR_RECURSIVE_ENTITY_REF",  PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const},` |
|      - | 2290 | `	{"XML_ERROR_ASYNC_ENTITY",          PH7_XML_ERROR_ASYNC_ENTITY_Const},` |
|      - | 2291 | `	{"XML_ERROR_BAD_CHAR_REF",          PH7_XML_ERROR_BAD_CHAR_REF_Const},` |
|      - | 2292 | `	{"XML_ERROR_BINARY_ENTITY_REF",     PH7_XML_ERROR_BINARY_ENTITY_REF_Const},` |
|      - | 2293 | `	{"XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF", PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const},` |
|      - | 2294 | `	{"XML_ERROR_MISPLACED_XML_PI",     PH7_XML_ERROR_MISPLACED_XML_PI_Const},` |
|      - | 2295 | `	{"XML_ERROR_UNKNOWN_ENCODING",     PH7_XML_ERROR_UNKNOWN_ENCODING_Const},` |
|      - | 2296 | `	{"XML_ERROR_INCORRECT_ENCODING",   PH7_XML_ERROR_INCORRECT_ENCODING_Const},` |
|      - | 2297 | `	{"XML_ERROR_UNCLOSED_CDATA_SECTION",  PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const},` |
|      - | 2298 | `	{"XML_ERROR_EXTERNAL_ENTITY_HANDLING",PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const},` |
|      - | 2299 | `	{"XML_OPTION_CASE_FOLDING",           PH7_XML_OPTION_CASE_FOLDING_Const},` |
|      - | 2300 | `	{"XML_OPTION_TARGET_ENCODING",        PH7_XML_OPTION_TARGET_ENCODING_Const},` |
|      - | 2301 | `	{"XML_OPTION_SKIP_TAGSTART",          PH7_XML_OPTION_SKIP_TAGSTART_Const},` |
|      - | 2302 | `	{"XML_OPTION_SKIP_WHITE",             PH7_XML_OPTION_SKIP_WHITE_Const},` |
|      - | 2303 | `	{"XML_SAX_IMPL",           PH7_XML_SAX_IMP_Const},` |
|      - | 2304 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2305 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|      - | 2306 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|      - | 2307 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|      - | 2308 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|      - | 2309 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|      - | 2310 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|      - | 2311 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|      - | 2312 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|      - | 2313 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|      - | 2314 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|      - | 2315 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|      - | 2316 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|      - | 2317 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|      - | 2318 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|      - | 2319 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|      - | 2320 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|      - | 2321 | `	{"JSON_ERROR_NON_BACKED_ENUM", PH7_JSON_ERROR_NON_BACKED_ENUM_Const},` |
|      - | 2322 | ``	/* `self`, `parent` and `static` are KEYWORDS in php, not constants: using one as a bare`` |
|      - | 2323 | ``	 * word is an "Undefined constant" Error (or a parse error for `static`). PH7 registered`` |
|      - | 2324 | `	 * them as constants that quietly expanded to the class name / NULL, so a typo'd bare` |
|      - | 2325 | ``	 * word silently produced a value. The `self::`/`parent::`/`static::` forms are handled`` |
|      - | 2326 | ``	 * by the `::` compile path and do not go through the constant table. */`` |
|      - | 2327 | `	{"__CLASS__",            PH7_class_magic_Const  }` |
|      - | 2328 | `};` |
|      - | 2329 | `/*` |
|      - | 2330 | ` * Register the built-in constants defined above.` |
|      - | 2331 | ` */` |
|   3352 | 2332 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|      5 | 2333 | `{` |
|      - | 2334 | `	sxu32 n;` |
|      - | 2335 | `	/*` |
|      - | 2336 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|      - | 2337 | `	 * that trigger the constant invocation as their private data.` |
|      - | 2338 | `	 */` |
| 884933 | 2339 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 881581 | 2340 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
| 440793 | 2341 | `	}` |
|   3357 | 2342 | `}` |
