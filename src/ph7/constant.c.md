# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1142/1169 lines (97.69%)

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
|   3830 |   63 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|      5 |   64 | `{` |
|      - |   65 | `#if defined(__WINNT__)` |
|      5 |   66 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|      - |   67 | `#elif defined(__UNIXES__)` |
|      - |   68 | `	struct utsname sInfo;` |
|   3830 |   69 | `	if( uname(&sInfo) != 0 ){` |
|    ! 0 |   70 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 |   71 | `	}else{` |
|   3830 |   72 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|      - |   73 | `	}` |
|      - |   74 | `#else` |
|      - |   75 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|      - |   76 | `#endif` |
|   1915 |   77 | `	SXUNUSED(pUnused);` |
|   3835 |   78 | `}` |
|      - |   79 | `/*` |
|      - |   80 | ` * PHP_EOL` |
|      - |   81 | ` *  Expand the correct 'End Of Line' symbol for this platform.` |
|      - |   82 | ` */` |
|    840 |   83 | `static void PH7_EOL_Const(ph7_value *pVal,void *pUnused)` |
|      5 |   84 | `{` |
|    420 |   85 | `	SXUNUSED(pUnused);` |
|      - |   86 | `#ifdef __WINNT__` |
|      5 |   87 | `	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);` |
|      - |   88 | `#else` |
|    840 |   89 | `	ph7_value_string(pVal,"\n",(int)sizeof(char));` |
|      - |   90 | `#endif` |
|    845 |   91 | `}` |
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
|    298 |  155 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|      4 |  156 | `{` |
|    149 |  157 | `	SXUNUSED(pUnused);` |
|      - |  158 | `#ifdef __WINNT__` |
|      4 |  159 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|      - |  160 | `#else` |
|    298 |  161 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|      - |  162 | `#endif` |
|    302 |  163 | `}` |
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
|      - |  509 | ` * php's sort-flag constants. The VALUES must match php exactly: they are a` |
|      - |  510 | ` * public ABI (code passes literal ints, dumps them, and OR-combines the base` |
|      - |  511 | ` * type with SORT_FLAG_CASE). SORT_ASC/SORT_DESC are the array_multisort` |
|      - |  512 | ` * direction flags.` |
|      - |  513 | ` * SORT_REGULAR 0 · SORT_NUMERIC 1 · SORT_STRING 2 · SORT_DESC 3 · SORT_ASC 4 ·` |
|      - |  514 | ` * SORT_LOCALE_STRING 5 · SORT_NATURAL 6 · SORT_FLAG_CASE 8` |
|      - |  515 | ` */` |
|      4 |  516 | `static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  517 | `{` |
|      5 |  518 | `	ph7_value_int(pVal,4);` |
|      2 |  519 | `	SXUNUSED(pUserData);` |
|      5 |  520 | `}` |
|      4 |  521 | `static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  522 | `{` |
|      5 |  523 | `	ph7_value_int(pVal,3);` |
|      2 |  524 | `	SXUNUSED(pUserData);` |
|      5 |  525 | `}` |
|      8 |  526 | `static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  527 | `{` |
|      9 |  528 | `	ph7_value_int(pVal,0);` |
|      4 |  529 | `	SXUNUSED(pUserData);` |
|      9 |  530 | `}` |
|     18 |  531 | `static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  532 | `{` |
|     19 |  533 | `	ph7_value_int(pVal,1);` |
|      9 |  534 | `	SXUNUSED(pUserData);` |
|     19 |  535 | `}` |
|     28 |  536 | `static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  537 | `{` |
|     29 |  538 | `	ph7_value_int(pVal,2);` |
|     14 |  539 | `	SXUNUSED(pUserData);` |
|     29 |  540 | `}` |
|      2 |  541 | `static void PH7_SORT_LOCALE_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  542 | `{` |
|      3 |  543 | `	ph7_value_int(pVal,5);` |
|      1 |  544 | `	SXUNUSED(pUserData);` |
|      3 |  545 | `}` |
|     12 |  546 | `static void PH7_SORT_NATURAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  547 | `{` |
|     13 |  548 | `	ph7_value_int(pVal,6);` |
|      6 |  549 | `	SXUNUSED(pUserData);` |
|     13 |  550 | `}` |
|     10 |  551 | `static void PH7_SORT_FLAG_CASE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  552 | `{` |
|     11 |  553 | `	ph7_value_int(pVal,8);` |
|      5 |  554 | `	SXUNUSED(pUserData);` |
|     11 |  555 | `}` |
|      - |  556 | `/*` |
|      - |  557 | ` * PHP_ROUND_HALF_UP` |
|      - |  558 | ` *  Expands 1.` |
|      - |  559 | ` */` |
|      4 |  560 | `static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  561 | `{` |
|      5 |  562 | `	ph7_value_int(pVal,1);` |
|      2 |  563 | `	SXUNUSED(pUserData);` |
|      5 |  564 | `}` |
|      - |  565 | `/*` |
|      - |  566 | ` * PHP_SESSION_DISABLED / PHP_SESSION_NONE / PHP_SESSION_ACTIVE` |
|      - |  567 | ` *  session_status() states (0 / 1 / 2).` |
|      - |  568 | ` */` |
|      2 |  569 | `static void PH7_PHP_SESSION_DISABLED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  570 | `{` |
|      3 |  571 | `	ph7_value_int(pVal,0);` |
|      1 |  572 | `	SXUNUSED(pUserData);` |
|      3 |  573 | `}` |
|      6 |  574 | `static void PH7_PHP_SESSION_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  575 | `{` |
|      7 |  576 | `	ph7_value_int(pVal,1);` |
|      3 |  577 | `	SXUNUSED(pUserData);` |
|      7 |  578 | `}` |
|     30 |  579 | `static void PH7_PHP_SESSION_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  580 | `{` |
|     32 |  581 | `	ph7_value_int(pVal,2);` |
|     15 |  582 | `	SXUNUSED(pUserData);` |
|     32 |  583 | `}` |
|      - |  584 | `/*` |
|      - |  585 | ` * INI_USER / INI_PERDIR / INI_SYSTEM / INI_ALL` |
|      - |  586 | ` *  php.ini access levels (1 / 2 / 4 / 7).` |
|      - |  587 | ` */` |
|     12 |  588 | `static void PH7_INI_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  589 | `{` |
|     13 |  590 | `	ph7_value_int(pVal,1);` |
|      6 |  591 | `	SXUNUSED(pUserData);` |
|     13 |  592 | `}` |
|      2 |  593 | `static void PH7_INI_PERDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  594 | `{` |
|      3 |  595 | `	ph7_value_int(pVal,2);` |
|      1 |  596 | `	SXUNUSED(pUserData);` |
|      3 |  597 | `}` |
|      2 |  598 | `static void PH7_INI_SYSTEM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  599 | `{` |
|      3 |  600 | `	ph7_value_int(pVal,4);` |
|      1 |  601 | `	SXUNUSED(pUserData);` |
|      3 |  602 | `}` |
|      2 |  603 | `static void PH7_INI_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  604 | `{` |
|      3 |  605 | `	ph7_value_int(pVal,7);` |
|      1 |  606 | `	SXUNUSED(pUserData);` |
|      3 |  607 | `}` |
|      - |  608 | `/*` |
|      - |  609 | ` * MB_CASE_UPPER / MB_CASE_LOWER / MB_CASE_TITLE (0 / 1 / 2)` |
|      - |  610 | ` */` |
|      4 |  611 | `static void PH7_MB_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  612 | `{` |
|      5 |  613 | `	ph7_value_int(pVal,0);` |
|      2 |  614 | `	SXUNUSED(pUserData);` |
|      5 |  615 | `}` |
|      4 |  616 | `static void PH7_MB_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  617 | `{` |
|      5 |  618 | `	ph7_value_int(pVal,1);` |
|      2 |  619 | `	SXUNUSED(pUserData);` |
|      5 |  620 | `}` |
|      4 |  621 | `static void PH7_MB_CASE_TITLE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  622 | `{` |
|      5 |  623 | `	ph7_value_int(pVal,2);` |
|      2 |  624 | `	SXUNUSED(pUserData);` |
|      5 |  625 | `}` |
|      - |  626 | `/*` |
|      - |  627 | ` * SPHP_ROUND_HALF_DOWN` |
|      - |  628 | ` *  Expands 2.` |
|      - |  629 | ` */` |
|      4 |  630 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  631 | `{` |
|      5 |  632 | `	ph7_value_int(pVal,2);` |
|      2 |  633 | `	SXUNUSED(pUserData);` |
|      5 |  634 | `}` |
|      - |  635 | `/*` |
|      - |  636 | ` * PHP_ROUND_HALF_EVEN` |
|      - |  637 | ` *  Expands 3.` |
|      - |  638 | ` */` |
|      8 |  639 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  640 | `{` |
|      9 |  641 | `	ph7_value_int(pVal,3);` |
|      4 |  642 | `	SXUNUSED(pUserData);` |
|      9 |  643 | `}` |
|      - |  644 | `/*` |
|      - |  645 | ` * PHP_ROUND_HALF_ODD` |
|      - |  646 | ` *  Expands 4.` |
|      - |  647 | ` */` |
|      4 |  648 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  649 | `{` |
|      5 |  650 | `	ph7_value_int(pVal,4);` |
|      2 |  651 | `	SXUNUSED(pUserData);` |
|      5 |  652 | `}` |
|      - |  653 | `/*` |
|      - |  654 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|      - |  655 | ` *  Expand 0x01` |
|      - |  656 | ` * NOTE:` |
|      - |  657 | ` *  The expanded value must be a power of two.` |
|      - |  658 | ` */` |
|      2 |  659 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  660 | `{` |
|      3 |  661 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      1 |  662 | `	SXUNUSED(pUserData);` |
|      3 |  663 | `}` |
|      - |  664 | `/*` |
|      - |  665 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|      - |  666 | ` *  Expand 0x02` |
|      - |  667 | ` * NOTE:` |
|      - |  668 | ` *  The expanded value must be a power of two.` |
|      - |  669 | ` */` |
|      2 |  670 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  671 | `{` |
|      3 |  672 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      1 |  673 | `	SXUNUSED(pUserData);` |
|      3 |  674 | `}` |
|      - |  675 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  676 | `/*` |
|      - |  677 | ` * M_PI` |
|      - |  678 | ` *  Expand the value of pi.` |
|      - |  679 | ` */` |
|      8 |  680 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  681 | `{` |
|      4 |  682 | `	SXUNUSED(pUserData); /* cc warning */` |
|     10 |  683 | `	ph7_value_double(pVal,PH7_PI);` |
|     10 |  684 | `}` |
|      - |  685 | `/*` |
|      - |  686 | ` * M_E` |
|      - |  687 | ` *  Expand 2.7182818284590452354` |
|      - |  688 | ` */` |
|      2 |  689 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  690 | `{` |
|      1 |  691 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  692 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|      3 |  693 | `}` |
|      - |  694 | `/*` |
|      - |  695 | ` * M_LOG2E` |
|      - |  696 | ` *  Expand 2.7182818284590452354` |
|      - |  697 | ` */` |
|      2 |  698 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  699 | `{` |
|      1 |  700 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  701 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      3 |  702 | `}` |
|      - |  703 | `/*` |
|      - |  704 | ` * M_LOG10E` |
|      - |  705 | ` *  Expand 0.4342944819032518276` |
|      - |  706 | ` */` |
|      2 |  707 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  708 | `{` |
|      1 |  709 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  710 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      3 |  711 | `}` |
|      - |  712 | `/*` |
|      - |  713 | ` * M_LN2` |
|      - |  714 | ` *  Expand 	0.69314718055994530942` |
|      - |  715 | ` */` |
|      2 |  716 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  717 | `{` |
|      1 |  718 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  719 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      3 |  720 | `}` |
|      - |  721 | `/*` |
|      - |  722 | ` * M_LN10` |
|      - |  723 | ` *  Expand 	2.30258509299404568402` |
|      - |  724 | ` */` |
|      2 |  725 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  726 | `{` |
|      1 |  727 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  728 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      3 |  729 | `}` |
|      - |  730 | `/*` |
|      - |  731 | ` * M_PI_2` |
|      - |  732 | ` *  Expand 	1.57079632679489661923` |
|      - |  733 | ` */` |
|      2 |  734 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  735 | `{` |
|      1 |  736 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  737 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      3 |  738 | `}` |
|      - |  739 | `/*` |
|      - |  740 | ` * M_PI_4` |
|      - |  741 | ` *  Expand 	0.78539816339744830962` |
|      - |  742 | ` */` |
|      2 |  743 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  744 | `{` |
|      1 |  745 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  746 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      3 |  747 | `}` |
|      - |  748 | `/*` |
|      - |  749 | ` * M_1_PI` |
|      - |  750 | ` *  Expand 	0.31830988618379067154` |
|      - |  751 | ` */` |
|      2 |  752 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  753 | `{` |
|      1 |  754 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  755 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      3 |  756 | `}` |
|      - |  757 | `/*` |
|      - |  758 | ` * M_2_PI` |
|      - |  759 | ` *  Expand 0.63661977236758134308` |
|      - |  760 | ` */` |
|      4 |  761 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  762 | `{` |
|      2 |  763 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  764 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      5 |  765 | `}` |
|      - |  766 | `/*` |
|      - |  767 | ` * M_SQRTPI` |
|      - |  768 | ` *  Expand 1.77245385090551602729` |
|      - |  769 | ` */` |
|      2 |  770 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  771 | `{` |
|      1 |  772 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  773 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      3 |  774 | `}` |
|      - |  775 | `/*` |
|      - |  776 | ` * M_2_SQRTPI` |
|      - |  777 | ` *  Expand 	1.12837916709551257390` |
|      - |  778 | ` */` |
|      2 |  779 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  780 | `{` |
|      1 |  781 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  782 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      3 |  783 | `}` |
|      - |  784 | `/*` |
|      - |  785 | ` * M_SQRT2` |
|      - |  786 | ` *  Expand 	1.41421356237309504880` |
|      - |  787 | ` */` |
|      2 |  788 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  789 | `{` |
|      1 |  790 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  791 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      3 |  792 | `}` |
|      - |  793 | `/*` |
|      - |  794 | ` * M_SQRT3` |
|      - |  795 | ` *  Expand 	1.73205080756887729352` |
|      - |  796 | ` */` |
|      2 |  797 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  798 | `{` |
|      1 |  799 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  800 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      3 |  801 | `}` |
|      - |  802 | `/*` |
|      - |  803 | ` * M_SQRT1_2` |
|      - |  804 | ` *  Expand 	0.70710678118654752440` |
|      - |  805 | ` */` |
|      2 |  806 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  807 | `{` |
|      1 |  808 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  809 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      3 |  810 | `}` |
|      - |  811 | `/*` |
|      - |  812 | ` * M_LNPI` |
|      - |  813 | ` *  Expand 	1.14472988584940017414` |
|      - |  814 | ` */` |
|      2 |  815 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  816 | `{` |
|      1 |  817 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  818 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      3 |  819 | `}` |
|      - |  820 | `/*` |
|      - |  821 | ` * M_EULER` |
|      - |  822 | ` *  Expand  0.57721566490153286061` |
|      - |  823 | ` */` |
|      2 |  824 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  825 | `{` |
|      1 |  826 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  827 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      3 |  828 | `}` |
|      - |  829 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|      - |  830 | `/*` |
|      - |  831 | ` * DATE_ATOM` |
|      - |  832 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|      - |  833 | ` */` |
|      2 |  834 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  835 | `{` |
|      1 |  836 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  837 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  838 | `}` |
|      - |  839 | `/*` |
|      - |  840 | ` * DATE_COOKIE` |
|      - |  841 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  842 | ` */` |
|      2 |  843 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  844 | `{` |
|      1 |  845 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  846 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  847 | `}` |
|      - |  848 | `/*` |
|      - |  849 | ` * DATE_ISO8601` |
|      - |  850 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|      - |  851 | ` */` |
|      2 |  852 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  853 | `{` |
|      1 |  854 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  855 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      3 |  856 | `}` |
|      - |  857 | `/*` |
|      - |  858 | ` * DATE_RFC822` |
|      - |  859 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|      - |  860 | ` */` |
|      2 |  861 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  862 | `{` |
|      1 |  863 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  864 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  865 | `}` |
|      - |  866 | `/*` |
|      - |  867 | ` * DATE_RFC850` |
|      - |  868 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  869 | ` */` |
|      2 |  870 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  871 | `{` |
|      1 |  872 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  873 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  874 | `}` |
|      - |  875 | `/*` |
|      - |  876 | ` * DATE_RFC1036` |
|      - |  877 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  878 | ` */` |
|      2 |  879 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  880 | `{` |
|      1 |  881 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  882 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  883 | `}` |
|      - |  884 | `/*` |
|      - |  885 | ` * DATE_RFC1123` |
|      - |  886 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  887 | ` */` |
|      2 |  888 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  889 | `{` |
|      1 |  890 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  891 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  892 | `}` |
|      - |  893 | `/*` |
|      - |  894 | ` * DATE_RFC2822` |
|      - |  895 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  896 | ` */` |
|      2 |  897 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  898 | `{` |
|      1 |  899 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  900 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  901 | `}` |
|      - |  902 | `/*` |
|      - |  903 | ` * DATE_RSS` |
|      - |  904 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  905 | ` */` |
|      2 |  906 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  907 | `{` |
|      1 |  908 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  909 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  910 | `}` |
|      - |  911 | `/*` |
|      - |  912 | ` * DATE_W3C` |
|      - |  913 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|      - |  914 | ` */` |
|      2 |  915 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  916 | `{` |
|      1 |  917 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  918 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  919 | `}` |
|      - |  920 | `/*` |
|      - |  921 | ` * The ENT_* values are PHP-exact (php 8.5.7). The low two bits are the quote` |
|      - |  922 | ` * bits (1 = single, 2 = double), so ENT_QUOTES = ENT_COMPAT\|1 and` |
|      - |  923 | ` * ENT_NOQUOTES = 0. Bits 16\|32 select the doctype (0 = HTML401, 16 = XML1,` |
|      - |  924 | ` * 32 = XHTML, 48 = HTML5) — composites, not flags.` |
|      - |  925 | ` */` |
|      - |  926 | `/*` |
|      - |  927 | ` * ENT_COMPAT` |
|      - |  928 | ` *  Expand 2 (double-quote bit only)` |
|      - |  929 | ` */` |
|     12 |  930 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  931 | `{` |
|      6 |  932 | `	SXUNUSED(pUserData); /* cc warning */` |
|     13 |  933 | `	ph7_value_int(pVal,PH7_ENT_QUOTE_DOUBLE);` |
|     13 |  934 | `}` |
|      - |  935 | `/*` |
|      - |  936 | ` * ENT_QUOTES` |
|      - |  937 | ` *  Expand 3 (double\|single quote bits)` |
|      - |  938 | ` */` |
|     60 |  939 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  940 | `{` |
|     30 |  941 | `	SXUNUSED(pUserData); /* cc warning */` |
|     61 |  942 | `	ph7_value_int(pVal,PH7_ENT_QUOTES);` |
|     61 |  943 | `}` |
|      - |  944 | `/*` |
|      - |  945 | ` * ENT_NOQUOTES` |
|      - |  946 | ` *  Expand 0 (no quote bits)` |
|      - |  947 | ` */` |
|     20 |  948 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  949 | `{` |
|     10 |  950 | `	SXUNUSED(pUserData); /* cc warning */` |
|     21 |  951 | `	ph7_value_int(pVal,0);` |
|     21 |  952 | `}` |
|      - |  953 | `/*` |
|      - |  954 | ` * ENT_IGNORE` |
|      - |  955 | ` *  Expand 4` |
|      - |  956 | ` */` |
|      6 |  957 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  958 | `{` |
|      3 |  959 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 |  960 | `	ph7_value_int(pVal,PH7_ENT_IGNORE);` |
|      7 |  961 | `}` |
|      - |  962 | `/*` |
|      - |  963 | ` * ENT_SUBSTITUTE` |
|      - |  964 | ` *  Expand 8` |
|      - |  965 | ` */` |
|      2 |  966 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  967 | `{` |
|      1 |  968 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  969 | `	ph7_value_int(pVal,PH7_ENT_SUBSTITUTE);` |
|      3 |  970 | `}` |
|      - |  971 | `/*` |
|      - |  972 | ` * ENT_DISALLOWED` |
|      - |  973 | ` *  Expand 128` |
|      - |  974 | ` */` |
|      2 |  975 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  976 | `{` |
|      1 |  977 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  978 | `	ph7_value_int(pVal,PH7_ENT_DISALLOWED);` |
|      3 |  979 | `}` |
|      - |  980 | `/*` |
|      - |  981 | ` * ENT_HTML401` |
|      - |  982 | ` *  Expand 0 (the default doctype)` |
|      - |  983 | ` */` |
|      2 |  984 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  985 | `{` |
|      1 |  986 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  987 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML401);` |
|      3 |  988 | `}` |
|      - |  989 | `/*` |
|      - |  990 | ` * ENT_XML1` |
|      - |  991 | ` *  Expand 16` |
|      - |  992 | ` */` |
|      8 |  993 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  994 | `{` |
|      4 |  995 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 |  996 | `	ph7_value_int(pVal,PH7_ENT_DOC_XML1);` |
|      9 |  997 | `}` |
|      - |  998 | `/*` |
|      - |  999 | ` * ENT_XHTML` |
|      - | 1000 | ` *  Expand 32` |
|      - | 1001 | ` */` |
|      6 | 1002 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1003 | `{` |
|      3 | 1004 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1005 | `	ph7_value_int(pVal,PH7_ENT_DOC_XHTML);` |
|      7 | 1006 | `}` |
|      - | 1007 | `/*` |
|      - | 1008 | ` * ENT_HTML5` |
|      - | 1009 | ` *  Expand 48 (16\|32 — a doctype composite, not a flag bit)` |
|      - | 1010 | ` */` |
|      8 | 1011 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1012 | `{` |
|      4 | 1013 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 | 1014 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML5);` |
|      9 | 1015 | `}` |
|      - | 1016 | `/*` |
|      - | 1017 | ` * ISO-8859-1` |
|      - | 1018 | ` * ISO_8859_1` |
|      - | 1019 | ` *   Expand 1` |
|      - | 1020 | ` */` |
|      2 | 1021 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1022 | `{` |
|      1 | 1023 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1024 | `	ph7_value_int(pVal,1);` |
|      3 | 1025 | `}` |
|      - | 1026 | `/*` |
|      - | 1027 | ` * UTF-8` |
|      - | 1028 | ` * UTF8` |
|      - | 1029 | ` *  Expand 2` |
|      - | 1030 | ` */` |
|      2 | 1031 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1032 | `{` |
|      1 | 1033 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1034 | `	ph7_value_int(pVal,1);` |
|      3 | 1035 | `}` |
|      - | 1036 | `/*` |
|      - | 1037 | ` * HTML_ENTITIES` |
|      - | 1038 | ` *  Expand 1` |
|      - | 1039 | ` */` |
|      4 | 1040 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1041 | `{` |
|      2 | 1042 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1043 | `	ph7_value_int(pVal,1);` |
|      5 | 1044 | `}` |
|      - | 1045 | `/*` |
|      - | 1046 | ` * HTML_SPECIALCHARS` |
|      - | 1047 | ` *  Expand 0 (PHP-exact)` |
|      - | 1048 | ` */` |
|     10 | 1049 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1050 | `{` |
|      5 | 1051 | `	SXUNUSED(pUserData); /* cc warning */` |
|     11 | 1052 | `	ph7_value_int(pVal,0);` |
|     11 | 1053 | `}` |
|      - | 1054 | `/*` |
|      - | 1055 | ` * PHP_URL_SCHEME.` |
|      - | 1056 | ` * Expand 1` |
|      - | 1057 | ` */` |
|      2 | 1058 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1059 | `{` |
|      1 | 1060 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1061 | `	ph7_value_int(pVal,1);` |
|      3 | 1062 | `}` |
|      - | 1063 | `/*` |
|      - | 1064 | ` * PHP_URL_HOST.` |
|      - | 1065 | ` * Expand 2` |
|      - | 1066 | ` */` |
|      2 | 1067 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1068 | `{` |
|      1 | 1069 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1070 | `	ph7_value_int(pVal,2);` |
|      3 | 1071 | `}` |
|      - | 1072 | `/*` |
|      - | 1073 | ` * PHP_URL_PORT.` |
|      - | 1074 | ` * Expand 3` |
|      - | 1075 | ` */` |
|      2 | 1076 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1077 | `{` |
|      1 | 1078 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1079 | `	ph7_value_int(pVal,3);` |
|      3 | 1080 | `}` |
|      - | 1081 | `/*` |
|      - | 1082 | ` * PHP_URL_USER.` |
|      - | 1083 | ` * Expand 4` |
|      - | 1084 | ` */` |
|      2 | 1085 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1086 | `{` |
|      1 | 1087 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1088 | `	ph7_value_int(pVal,4);` |
|      3 | 1089 | `}` |
|      - | 1090 | `/*` |
|      - | 1091 | ` * PHP_URL_PASS.` |
|      - | 1092 | ` * Expand 5` |
|      - | 1093 | ` */` |
|      2 | 1094 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1095 | `{` |
|      1 | 1096 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1097 | `	ph7_value_int(pVal,5);` |
|      3 | 1098 | `}` |
|      - | 1099 | `/*` |
|      - | 1100 | ` * PHP_URL_PATH.` |
|      - | 1101 | ` * Expand 6` |
|      - | 1102 | ` */` |
|      2 | 1103 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1104 | `{` |
|      1 | 1105 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1106 | `	ph7_value_int(pVal,6);` |
|      3 | 1107 | `}` |
|      - | 1108 | `/*` |
|      - | 1109 | ` * PHP_URL_QUERY.` |
|      - | 1110 | ` * Expand 7` |
|      - | 1111 | ` */` |
|      2 | 1112 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1113 | `{` |
|      1 | 1114 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1115 | `	ph7_value_int(pVal,7);` |
|      3 | 1116 | `}` |
|      - | 1117 | `/*` |
|      - | 1118 | ` * PHP_URL_FRAGMENT.` |
|      - | 1119 | ` * Expand 8` |
|      - | 1120 | ` */` |
|      2 | 1121 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1122 | `{` |
|      1 | 1123 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1124 | `	ph7_value_int(pVal,8);` |
|      3 | 1125 | `}` |
|      - | 1126 | `/*` |
|      - | 1127 | ` * PHP_QUERY_RFC1738` |
|      - | 1128 | ` * Expand 1` |
|      - | 1129 | ` */` |
|     22 | 1130 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1131 | `{` |
|     11 | 1132 | `	SXUNUSED(pUserData); /* cc warning */` |
|     23 | 1133 | `	ph7_value_int(pVal,1);` |
|     23 | 1134 | `}` |
|      - | 1135 | `/*` |
|      - | 1136 | ` * PHP_QUERY_RFC3986` |
|      - | 1137 | ` * Expand 1` |
|      - | 1138 | ` */` |
|     96 | 1139 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1140 | `{` |
|     48 | 1141 | `	SXUNUSED(pUserData); /* cc warning */` |
|     97 | 1142 | `	ph7_value_int(pVal,2);` |
|     97 | 1143 | `}` |
|      - | 1144 | `/* php's FNM_* values (ext/standard): PATHNAME=1, NOESCAPE=2, PERIOD=4, CASEFOLD=16.` |
|      - | 1145 | ` * PHL previously had PATHNAME/NOESCAPE swapped and CASEFOLD=8; fnmatch() reads these` |
|      - | 1146 | ` * bits, so PH7_builtin_fnmatch was updated to the same values. */` |
|      - | 1147 | `/*` |
|      - | 1148 | ` * FNM_PATHNAME` |
|      - | 1149 | ` *  Expand 1 (php value)` |
|      - | 1150 | ` */` |
|    ! 0 | 1151 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 1152 | `{` |
|    ! 0 | 1153 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 1154 | `	ph7_value_int(pVal,1);` |
|    ! 0 | 1155 | `}` |
|      - | 1156 | `/*` |
|      - | 1157 | ` * FNM_NOESCAPE` |
|      - | 1158 | ` *  Expand 2 (php value)` |
|      - | 1159 | ` */` |
|    ! 0 | 1160 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 1161 | `{` |
|    ! 0 | 1162 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 1163 | `	ph7_value_int(pVal,2);` |
|    ! 0 | 1164 | `}` |
|      - | 1165 | `/*` |
|      - | 1166 | ` * FNM_PERIOD` |
|      - | 1167 | ` *  Expand 4 (php value)` |
|      - | 1168 | ` */` |
|      6 | 1169 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1170 | `{` |
|      3 | 1171 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1172 | `	ph7_value_int(pVal,4);` |
|      7 | 1173 | `}` |
|      - | 1174 | `/*` |
|      - | 1175 | ` * FNM_CASEFOLD` |
|      - | 1176 | ` *  Expand 16 (php value)` |
|      - | 1177 | ` */` |
|      4 | 1178 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1179 | `{` |
|      2 | 1180 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1181 | `	ph7_value_int(pVal,16);` |
|      5 | 1182 | `}` |
|      - | 1183 | `/*` |
|      - | 1184 | ` * PATHINFO_DIRNAME` |
|      - | 1185 | ` *  Expand 1.` |
|      - | 1186 | ` */` |
|      4 | 1187 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1188 | `{` |
|      2 | 1189 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1190 | `	ph7_value_int(pVal,1);` |
|      5 | 1191 | `}` |
|      - | 1192 | `/*` |
|      - | 1193 | ` * PATHINFO_BASENAME` |
|      - | 1194 | ` *  Expand 2.` |
|      - | 1195 | ` */` |
|      4 | 1196 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1197 | `{` |
|      2 | 1198 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1199 | `	ph7_value_int(pVal,2);` |
|      5 | 1200 | `}` |
|      - | 1201 | `/*` |
|      - | 1202 | ` * PATHINFO_EXTENSION` |
|      - | 1203 | ` *  Expand 3.` |
|      - | 1204 | ` */` |
|   6568 | 1205 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1206 | `{` |
|   3284 | 1207 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6573 | 1208 | `	ph7_value_int(pVal,3);` |
|   6573 | 1209 | `}` |
|      - | 1210 | `/*` |
|      - | 1211 | ` * PATHINFO_FILENAME` |
|      - | 1212 | ` *  Expand 4.` |
|      - | 1213 | ` */` |
|   6560 | 1214 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1215 | `{` |
|   3280 | 1216 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6565 | 1217 | `	ph7_value_int(pVal,4);` |
|   6565 | 1218 | `}` |
|      - | 1219 | `/*` |
|      - | 1220 | ` * ASSERT_ACTIVE.` |
|      - | 1221 | ` *  PHP ASSERT_ACTIVE = 1` |
|      - | 1222 | ` */` |
|     14 | 1223 | `static void PH7_ASSERT_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1224 | `{` |
|      7 | 1225 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1226 | `	ph7_value_int(pVal,1); /* PHP ASSERT_ACTIVE = 1 */` |
|     16 | 1227 | `}` |
|      - | 1228 | `/*` |
|      - | 1229 | ` * ASSERT_CALLBACK.` |
|      - | 1230 | ` *  PHP ASSERT_CALLBACK = 2` |
|      - | 1231 | ` */` |
|      6 | 1232 | `static void PH7_ASSERT_CALLBACK_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1233 | `{` |
|      3 | 1234 | `	SXUNUSED(pUserData); /* cc warning */` |
|      8 | 1235 | `	ph7_value_int(pVal,2); /* PHP ASSERT_CALLBACK = 2 */` |
|      8 | 1236 | `}` |
|      - | 1237 | `/*` |
|      - | 1238 | ` * ASSERT_BAIL.` |
|      - | 1239 | ` *  PHP ASSERT_BAIL = 3` |
|      - | 1240 | ` */` |
|     14 | 1241 | `static void PH7_ASSERT_BAIL_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1242 | `{` |
|      7 | 1243 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1244 | `	ph7_value_int(pVal,3); /* PHP ASSERT_BAIL = 3 */` |
|     16 | 1245 | `}` |
|      - | 1246 | `/*` |
|      - | 1247 | ` * ASSERT_WARNING.` |
|      - | 1248 | ` *  PHP ASSERT_WARNING = 4 (deprecated in PHP 8.3)` |
|      - | 1249 | ` */` |
|      4 | 1250 | `static void PH7_ASSERT_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1251 | `{` |
|      2 | 1252 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1253 | `	ph7_value_int(pVal,4); /* PHP ASSERT_WARNING = 4 */` |
|      5 | 1254 | `}` |
|      - | 1255 | `/*` |
|      - | 1256 | ` * ASSERT_EXCEPTION.` |
|      - | 1257 | ` *  PHP ASSERT_EXCEPTION = 5 (deprecated in PHP 8.3)` |
|      - | 1258 | ` */` |
|      4 | 1259 | `static void PH7_ASSERT_EXCEPTION_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1260 | `{` |
|      2 | 1261 | `	SXUNUSED(pUserData); /* cc warning */` |
|      6 | 1262 | `	ph7_value_int(pVal,5); /* PHP ASSERT_EXCEPTION = 5 */` |
|      6 | 1263 | `}` |
|      - | 1264 | `/*` |
|      - | 1265 | ` * SEEK_SET.` |
|      - | 1266 | ` *  Expand 0` |
|      - | 1267 | ` */` |
|      2 | 1268 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1269 | `{` |
|      1 | 1270 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1271 | `	ph7_value_int(pVal,0);` |
|      3 | 1272 | `}` |
|      - | 1273 | `/*` |
|      - | 1274 | ` * SEEK_CUR.` |
|      - | 1275 | ` *  Expand 1` |
|      - | 1276 | ` */` |
|      2 | 1277 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1278 | `{` |
|      1 | 1279 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1280 | `	ph7_value_int(pVal,1);` |
|      3 | 1281 | `}` |
|      - | 1282 | `/*` |
|      - | 1283 | ` * SEEK_END.` |
|      - | 1284 | ` *  Expand 2` |
|      - | 1285 | ` */` |
|      4 | 1286 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1287 | `{` |
|      2 | 1288 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1289 | `	ph7_value_int(pVal,2);` |
|      5 | 1290 | `}` |
|      - | 1291 | `/*` |
|      - | 1292 | ` * LOCK_SH.` |
|      - | 1293 | ` *  Expand 2` |
|      - | 1294 | ` */` |
|      2 | 1295 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1296 | `{` |
|      1 | 1297 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1298 | `	ph7_value_int(pVal,1);` |
|      3 | 1299 | `}` |
|      - | 1300 | `/*` |
|      - | 1301 | ` * LOCK_NB.` |
|      - | 1302 | ` *  Expand 4 (php)` |
|      - | 1303 | ` */` |
|      2 | 1304 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1305 | `{` |
|      1 | 1306 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1307 | `	ph7_value_int(pVal,4);` |
|      3 | 1308 | `}` |
|      - | 1309 | `/*` |
|      - | 1310 | ` * LOCK_EX.` |
|      - | 1311 | ` *  Expand 2 (php). PH7 used 1, which collided with LOCK_SH, and LOCK_UN was 0 — so` |
|      - | 1312 | ` *  flock($h, LOCK_UN) asked the stream for a SHARED lock instead of releasing one.` |
|      - | 1313 | ` */` |
|      4 | 1314 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1315 | `{` |
|      2 | 1316 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1317 | `	ph7_value_int(pVal,2);` |
|      5 | 1318 | `}` |
|      - | 1319 | `/*` |
|      - | 1320 | ` * LOCK_UN.` |
|      - | 1321 | ` *  Expand 3 (php)` |
|      - | 1322 | ` */` |
|      4 | 1323 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1324 | `{` |
|      2 | 1325 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1326 | `	ph7_value_int(pVal,3);` |
|      5 | 1327 | `}` |
|      - | 1328 | `/*` |
|      - | 1329 | ` * FILE_USE_INCLUDE_PATH` |
|      - | 1330 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1331 | ` */` |
|      2 | 1332 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1333 | `{` |
|      1 | 1334 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1335 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1336 | `}` |
|      - | 1337 | `/*` |
|      - | 1338 | ` * FILE_IGNORE_NEW_LINES` |
|      - | 1339 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1340 | ` */` |
|      2 | 1341 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1342 | `{` |
|      1 | 1343 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1344 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1345 | `}` |
|      - | 1346 | `/*` |
|      - | 1347 | ` * FILE_SKIP_EMPTY_LINES` |
|      - | 1348 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1349 | ` */` |
|      2 | 1350 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1351 | `{` |
|      1 | 1352 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1353 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1354 | `}` |
|      - | 1355 | `/*` |
|      - | 1356 | ` * FILE_APPEND` |
|      - | 1357 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1358 | ` */` |
|      2 | 1359 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1360 | `{` |
|      1 | 1361 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1362 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1363 | `}` |
|      - | 1364 | `/*` |
|      - | 1365 | ` * SCANDIR_SORT_ASCENDING` |
|      - | 1366 | ` *  Expand 0` |
|      - | 1367 | ` */` |
|   2074 | 1368 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1369 | `{` |
|   1037 | 1370 | `	SXUNUSED(pUserData); /* cc warning */` |
|   2079 | 1371 | `	ph7_value_int(pVal,0);` |
|   2079 | 1372 | `}` |
|      - | 1373 | `/*` |
|      - | 1374 | ` * SCANDIR_SORT_DESCENDING` |
|      - | 1375 | ` *  Expand 1` |
|      - | 1376 | ` */` |
|   1038 | 1377 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1378 | `{` |
|    519 | 1379 | `	SXUNUSED(pUserData); /* cc warning */` |
|   1043 | 1380 | `	ph7_value_int(pVal,1);` |
|   1043 | 1381 | `}` |
|      - | 1382 | `/*` |
|      - | 1383 | ` * SCANDIR_SORT_NONE` |
|      - | 1384 | ` *  Expand 2` |
|      - | 1385 | ` */` |
|      2 | 1386 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1387 | `{` |
|      1 | 1388 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1389 | `	ph7_value_int(pVal,2);` |
|      3 | 1390 | `}` |
|      - | 1391 | `/*` |
|      - | 1392 | ` * GLOB_MARK` |
|      - | 1393 | ` *  Expand 0x01 (must be a power of two)` |
|      - | 1394 | ` */` |
|      2 | 1395 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1396 | `{` |
|      1 | 1397 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1398 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1399 | `}` |
|      - | 1400 | `/*` |
|      - | 1401 | ` * GLOB_NOSORT` |
|      - | 1402 | ` *  Expand 0x02 (must be a power of two)` |
|      - | 1403 | ` */` |
|      2 | 1404 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1405 | `{` |
|      1 | 1406 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1407 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1408 | `}` |
|      - | 1409 | `/*` |
|      - | 1410 | ` * GLOB_NOCHECK` |
|      - | 1411 | ` *  Expand 0x04 (must be a power of two)` |
|      - | 1412 | ` */` |
|      2 | 1413 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1414 | `{` |
|      1 | 1415 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1416 | `	ph7_value_int(pVal,0x04);` |
|      3 | 1417 | `}` |
|      - | 1418 | `/*` |
|      - | 1419 | ` * GLOB_NOESCAPE` |
|      - | 1420 | ` *  Expand 0x08 (must be a power of two)` |
|      - | 1421 | ` */` |
|      2 | 1422 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1423 | `{` |
|      1 | 1424 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1425 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1426 | `}` |
|      - | 1427 | `/*` |
|      - | 1428 | ` * GLOB_BRACE` |
|      - | 1429 | ` *  Expand 0x10 (must be a power of two)` |
|      - | 1430 | ` */` |
|      2 | 1431 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1432 | `{` |
|      1 | 1433 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1434 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1435 | `}` |
|      - | 1436 | `/*` |
|      - | 1437 | ` * GLOB_ONLYDIR` |
|      - | 1438 | ` *  Expand 0x20 (must be a power of two)` |
|      - | 1439 | ` */` |
|      2 | 1440 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1441 | `{` |
|      1 | 1442 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1443 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1444 | `}` |
|      - | 1445 | `/*` |
|      - | 1446 | ` * GLOB_ERR` |
|      - | 1447 | ` *  Expand 0x40 (must be a power of two)` |
|      - | 1448 | ` */` |
|      2 | 1449 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1450 | `{` |
|      1 | 1451 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1452 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1453 | `}` |
|      - | 1454 | `/*` |
|      - | 1455 | ` * STDIN` |
|      - | 1456 | ` *  Expand the STDIN handle as a resource.` |
|      - | 1457 | ` */` |
|      2 | 1458 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1459 | `{` |
|      3 | 1460 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1461 | `	void *pResource;` |
|      3 | 1462 | `	pResource = PH7_ExportStdin(pVm);` |
|      3 | 1463 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1464 | `}` |
|      - | 1465 | `/*` |
|      - | 1466 | ` * STDOUT` |
|      - | 1467 | ` *   Expand the STDOUT handle as a resource.` |
|      - | 1468 | ` */` |
|      2 | 1469 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1470 | `{` |
|      3 | 1471 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1472 | `	void *pResource;` |
|      3 | 1473 | `	pResource = PH7_ExportStdout(pVm);` |
|      3 | 1474 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1475 | `}` |
|      - | 1476 | `/*` |
|      - | 1477 | ` * STDERR` |
|      - | 1478 | ` *  Expand the STDERR handle as a resource.` |
|      - | 1479 | ` */` |
|      2 | 1480 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1481 | `{` |
|      3 | 1482 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1483 | `	void *pResource;` |
|      3 | 1484 | `	pResource = PH7_ExportStderr(pVm);` |
|      3 | 1485 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1486 | `}` |
|      - | 1487 | `/*` |
|      - | 1488 | ` * INI_SCANNER_NORMAL` |
|      - | 1489 | ` *   Expand 1` |
|      - | 1490 | ` */` |
|      2 | 1491 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1492 | `{` |
|      1 | 1493 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1494 | `	ph7_value_int(pVal,1);` |
|      3 | 1495 | `}` |
|      - | 1496 | `/*` |
|      - | 1497 | ` * INI_SCANNER_RAW` |
|      - | 1498 | ` *   Expand 2` |
|      - | 1499 | ` */` |
|      2 | 1500 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1501 | `{` |
|      1 | 1502 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1503 | `	ph7_value_int(pVal,2);` |
|      3 | 1504 | `}` |
|      - | 1505 | `/*` |
|      - | 1506 | ` * EXTR_OVERWRITE` |
|      - | 1507 | ` *   Expand 0x01 (Must be a power of two)` |
|      - | 1508 | ` */` |
|      2 | 1509 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1510 | `{` |
|      1 | 1511 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1512 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1513 | `}` |
|      - | 1514 | `/*` |
|      - | 1515 | ` * EXTR_SKIP` |
|      - | 1516 | ` *   Expand 0x02 (Must be a power of two)` |
|      - | 1517 | ` */` |
|      2 | 1518 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1519 | `{` |
|      1 | 1520 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1521 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1522 | `}` |
|      - | 1523 | `/*` |
|      - | 1524 | ` * EXTR_PREFIX_SAME` |
|      - | 1525 | ` *   Expand 0x04 (Must be a power of two)` |
|      - | 1526 | ` */` |
|      2 | 1527 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1528 | `{` |
|      1 | 1529 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1530 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1531 | `}` |
|      - | 1532 | `/*` |
|      - | 1533 | ` * EXTR_PREFIX_ALL` |
|      - | 1534 | ` *   Expand 0x08 (Must be a power of two)` |
|      - | 1535 | ` */` |
|      2 | 1536 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1537 | `{` |
|      1 | 1538 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1539 | `	ph7_value_int(pVal,0x8);` |
|      3 | 1540 | `}` |
|      - | 1541 | `/*` |
|      - | 1542 | ` * EXTR_PREFIX_INVALID` |
|      - | 1543 | ` *   Expand 0x10 (Must be a power of two)` |
|      - | 1544 | ` */` |
|      2 | 1545 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1546 | `{` |
|      1 | 1547 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1548 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1549 | `}` |
|      - | 1550 | `/*` |
|      - | 1551 | ` * EXTR_IF_EXISTS` |
|      - | 1552 | ` *   Expand 0x20 (Must be a power of two)` |
|      - | 1553 | ` */` |
|      2 | 1554 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1555 | `{` |
|      1 | 1556 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1557 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1558 | `}` |
|      - | 1559 | `/*` |
|      - | 1560 | ` * EXTR_PREFIX_IF_EXISTS` |
|      - | 1561 | ` *   Expand 0x40 (Must be a power of two)` |
|      - | 1562 | ` */` |
|      2 | 1563 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1564 | `{` |
|      1 | 1565 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1566 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1567 | `}` |
|      - | 1568 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1569 | `/*` |
|      - | 1570 | ` * XML_ERROR_NONE` |
|      - | 1571 | ` *   Expand the value of SXML_ERROR_NO_MEMORY defined in ph7Int.h` |
|      - | 1572 | ` */` |
|      2 | 1573 | `static void PH7_XML_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1574 | `{` |
|      1 | 1575 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1576 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1577 | `}` |
|      - | 1578 | `/*` |
|      - | 1579 | ` * XML_ERROR_NO_MEMORY` |
|      - | 1580 | ` *   Expand the value of SXML_ERROR_NONE defined in ph7Int.h` |
|      - | 1581 | ` */` |
|      2 | 1582 | `static void PH7_XML_ERROR_NO_MEMORY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1583 | `{` |
|      1 | 1584 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1585 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1586 | `}` |
|      - | 1587 | `/*` |
|      - | 1588 | ` * XML_ERROR_SYNTAX` |
|      - | 1589 | ` *   Expand the value of SXML_ERROR_SYNTAX defined in ph7Int.h` |
|      - | 1590 | ` */` |
|      2 | 1591 | `static void PH7_XML_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1592 | `{` |
|      1 | 1593 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1594 | `	ph7_value_int(pVal,SXML_ERROR_SYNTAX);` |
|      3 | 1595 | `}` |
|      - | 1596 | `/*` |
|      - | 1597 | ` * XML_ERROR_NO_ELEMENTS` |
|      - | 1598 | ` *   Expand the value of SXML_ERROR_NO_ELEMENTS defined in ph7Int.h` |
|      - | 1599 | ` */` |
|      2 | 1600 | `static void PH7_XML_ERROR_NO_ELEMENTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1601 | `{` |
|      1 | 1602 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1603 | `	ph7_value_int(pVal,SXML_ERROR_NO_ELEMENTS);` |
|      3 | 1604 | `}` |
|      - | 1605 | `/*` |
|      - | 1606 | ` * XML_ERROR_INVALID_TOKEN` |
|      - | 1607 | ` *   Expand the value of SXML_ERROR_INVALID_TOKEN defined in ph7Int.h` |
|      - | 1608 | ` */` |
|      2 | 1609 | `static void PH7_XML_ERROR_INVALID_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1610 | `{` |
|      1 | 1611 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1612 | `	ph7_value_int(pVal,SXML_ERROR_INVALID_TOKEN);` |
|      3 | 1613 | `}` |
|      - | 1614 | `/*` |
|      - | 1615 | ` * XML_ERROR_UNCLOSED_TOKEN` |
|      - | 1616 | ` *   Expand the value of SXML_ERROR_UNCLOSED_TOKEN defined in ph7Int.h` |
|      - | 1617 | ` */` |
|      2 | 1618 | `static void PH7_XML_ERROR_UNCLOSED_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1619 | `{` |
|      1 | 1620 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1621 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_TOKEN);` |
|      3 | 1622 | `}` |
|      - | 1623 | `/*` |
|      - | 1624 | ` * XML_ERROR_PARTIAL_CHAR` |
|      - | 1625 | ` *   Expand the value of SXML_ERROR_PARTIAL_CHAR defined in ph7Int.h` |
|      - | 1626 | ` */` |
|      2 | 1627 | `static void PH7_XML_ERROR_PARTIAL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1628 | `{` |
|      1 | 1629 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1630 | `	ph7_value_int(pVal,SXML_ERROR_PARTIAL_CHAR);` |
|      3 | 1631 | `}` |
|      - | 1632 | `/*` |
|      - | 1633 | ` * XML_ERROR_TAG_MISMATCH` |
|      - | 1634 | ` *   Expand the value of SXML_ERROR_TAG_MISMATCH defined in ph7Int.h` |
|      - | 1635 | ` */` |
|      2 | 1636 | `static void PH7_XML_ERROR_TAG_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1637 | `{` |
|      1 | 1638 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1639 | `	ph7_value_int(pVal,SXML_ERROR_TAG_MISMATCH);` |
|      3 | 1640 | `}` |
|      - | 1641 | `/*` |
|      - | 1642 | ` * XML_ERROR_DUPLICATE_ATTRIBUTE` |
|      - | 1643 | ` *   Expand the value of SXML_ERROR_DUPLICATE_ATTRIBUTE defined in ph7Int.h` |
|      - | 1644 | ` */` |
|      2 | 1645 | `static void PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1646 | `{` |
|      1 | 1647 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1648 | `	ph7_value_int(pVal,SXML_ERROR_DUPLICATE_ATTRIBUTE);` |
|      3 | 1649 | `}` |
|      - | 1650 | `/*` |
|      - | 1651 | ` * XML_ERROR_JUNK_AFTER_DOC_ELEMENT` |
|      - | 1652 | ` *   Expand the value of SXML_ERROR_JUNK_AFTER_DOC_ELEMENT defined in ph7Int.h` |
|      - | 1653 | ` */` |
|      2 | 1654 | `static void PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1655 | `{` |
|      1 | 1656 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1657 | `	ph7_value_int(pVal,SXML_ERROR_JUNK_AFTER_DOC_ELEMENT);` |
|      3 | 1658 | `}` |
|      - | 1659 | `/*` |
|      - | 1660 | ` * XML_ERROR_PARAM_ENTITY_REF` |
|      - | 1661 | ` *   Expand the value of SXML_ERROR_PARAM_ENTITY_REF defined in ph7Int.h` |
|      - | 1662 | ` */` |
|      2 | 1663 | `static void PH7_XML_ERROR_PARAM_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1664 | `{` |
|      1 | 1665 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1666 | `	ph7_value_int(pVal,SXML_ERROR_PARAM_ENTITY_REF);` |
|      3 | 1667 | `}` |
|      - | 1668 | `/*` |
|      - | 1669 | ` * XML_ERROR_UNDEFINED_ENTITY` |
|      - | 1670 | ` *   Expand the value of SXML_ERROR_UNDEFINED_ENTITY defined in ph7Int.h` |
|      - | 1671 | ` */` |
|      2 | 1672 | `static void PH7_XML_ERROR_UNDEFINED_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1673 | `{` |
|      1 | 1674 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1675 | `	ph7_value_int(pVal,SXML_ERROR_UNDEFINED_ENTITY);` |
|      3 | 1676 | `}` |
|      - | 1677 | `/*` |
|      - | 1678 | ` * XML_ERROR_RECURSIVE_ENTITY_REF` |
|      - | 1679 | ` *   Expand the value of SXML_ERROR_RECURSIVE_ENTITY_REF defined in ph7Int.h` |
|      - | 1680 | ` */` |
|      2 | 1681 | `static void PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1682 | `{` |
|      1 | 1683 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1684 | `	ph7_value_int(pVal,SXML_ERROR_RECURSIVE_ENTITY_REF);` |
|      3 | 1685 | `}` |
|      - | 1686 | `/*` |
|      - | 1687 | ` * XML_ERROR_ASYNC_ENTITY` |
|      - | 1688 | ` *   Expand the value of SXML_ERROR_ASYNC_ENTITY defined in ph7Int.h` |
|      - | 1689 | ` */` |
|      2 | 1690 | `static void PH7_XML_ERROR_ASYNC_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1691 | `{` |
|      1 | 1692 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1693 | `	ph7_value_int(pVal,SXML_ERROR_ASYNC_ENTITY);` |
|      3 | 1694 | `}` |
|      - | 1695 | `/*` |
|      - | 1696 | ` * XML_ERROR_BAD_CHAR_REF` |
|      - | 1697 | ` *   Expand the value of SXML_ERROR_BAD_CHAR_REF defined in ph7Int.h` |
|      - | 1698 | ` */` |
|      2 | 1699 | `static void PH7_XML_ERROR_BAD_CHAR_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1700 | `{` |
|      1 | 1701 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1702 | `	ph7_value_int(pVal,SXML_ERROR_BAD_CHAR_REF);` |
|      3 | 1703 | `}` |
|      - | 1704 | `/*` |
|      - | 1705 | ` * XML_ERROR_BINARY_ENTITY_REF` |
|      - | 1706 | ` *   Expand the value of SXML_ERROR_BINARY_ENTITY_REF defined in ph7Int.h` |
|      - | 1707 | ` */` |
|      2 | 1708 | `static void PH7_XML_ERROR_BINARY_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1709 | `{` |
|      1 | 1710 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1711 | `	ph7_value_int(pVal,SXML_ERROR_BINARY_ENTITY_REF);` |
|      3 | 1712 | `}` |
|      - | 1713 | `/*` |
|      - | 1714 | ` * XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF` |
|      - | 1715 | ` *   Expand the value of SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF defined in ph7Int.h` |
|      - | 1716 | ` */` |
|      2 | 1717 | `static void PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1718 | `{` |
|      1 | 1719 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1720 | `	ph7_value_int(pVal,SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF);` |
|      3 | 1721 | `}` |
|      - | 1722 | `/*` |
|      - | 1723 | ` * XML_ERROR_MISPLACED_XML_PI` |
|      - | 1724 | ` *   Expand the value of SXML_ERROR_MISPLACED_XML_PI defined in ph7Int.h` |
|      - | 1725 | ` */` |
|      2 | 1726 | `static void PH7_XML_ERROR_MISPLACED_XML_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1727 | `{` |
|      1 | 1728 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1729 | `	ph7_value_int(pVal,SXML_ERROR_MISPLACED_XML_PI);` |
|      3 | 1730 | `}` |
|      - | 1731 | `/*` |
|      - | 1732 | ` * XML_ERROR_UNKNOWN_ENCODING` |
|      - | 1733 | ` *   Expand the value of SXML_ERROR_UNKNOWN_ENCODING defined in ph7Int.h` |
|      - | 1734 | ` */` |
|      2 | 1735 | `static void PH7_XML_ERROR_UNKNOWN_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1736 | `{` |
|      1 | 1737 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1738 | `	ph7_value_int(pVal,SXML_ERROR_UNKNOWN_ENCODING);` |
|      3 | 1739 | `}` |
|      - | 1740 | `/*` |
|      - | 1741 | ` * XML_ERROR_INCORRECT_ENCODING` |
|      - | 1742 | ` *   Expand the value of SXML_ERROR_INCORRECT_ENCODING defined in ph7Int.h` |
|      - | 1743 | ` */` |
|      2 | 1744 | `static void PH7_XML_ERROR_INCORRECT_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1745 | `{` |
|      1 | 1746 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1747 | `	ph7_value_int(pVal,SXML_ERROR_INCORRECT_ENCODING);` |
|      3 | 1748 | `}` |
|      - | 1749 | `/*` |
|      - | 1750 | ` * XML_ERROR_UNCLOSED_CDATA_SECTION` |
|      - | 1751 | ` *   Expand the value of SXML_ERROR_UNCLOSED_CDATA_SECTION defined in ph7Int.h` |
|      - | 1752 | ` */` |
|      2 | 1753 | `static void PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1754 | `{` |
|      1 | 1755 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1756 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_CDATA_SECTION);` |
|      3 | 1757 | `}` |
|      - | 1758 | `/*` |
|      - | 1759 | ` * XML_ERROR_EXTERNAL_ENTITY_HANDLING` |
|      - | 1760 | ` *   Expand the value of SXML_ERROR_EXTERNAL_ENTITY_HANDLING defined in ph7Int.h` |
|      - | 1761 | ` */` |
|      2 | 1762 | `static void PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1763 | `{` |
|      1 | 1764 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1765 | `	ph7_value_int(pVal,SXML_ERROR_EXTERNAL_ENTITY_HANDLING);` |
|      3 | 1766 | `}` |
|      - | 1767 | `/*` |
|      - | 1768 | ` * XML_OPTION_CASE_FOLDING` |
|      - | 1769 | ` *   Expand the value of SXML_OPTION_CASE_FOLDING defined in ph7Int.h.` |
|      - | 1770 | ` */` |
|      2 | 1771 | `static void PH7_XML_OPTION_CASE_FOLDING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1772 | `{` |
|      1 | 1773 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1774 | `	ph7_value_int(pVal,SXML_OPTION_CASE_FOLDING);` |
|      3 | 1775 | `}` |
|      - | 1776 | `/*` |
|      - | 1777 | ` * XML_OPTION_TARGET_ENCODING` |
|      - | 1778 | ` *   Expand the value of SXML_OPTION_TARGET_ENCODING defined in ph7Int.h.` |
|      - | 1779 | ` */` |
|      4 | 1780 | `static void PH7_XML_OPTION_TARGET_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1781 | `{` |
|      2 | 1782 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1783 | `	ph7_value_int(pVal,SXML_OPTION_TARGET_ENCODING);` |
|      5 | 1784 | `}` |
|      - | 1785 | `/*` |
|      - | 1786 | ` * XML_OPTION_SKIP_TAGSTART` |
|      - | 1787 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1788 | ` */` |
|      2 | 1789 | `static void PH7_XML_OPTION_SKIP_TAGSTART_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1790 | `{` |
|      1 | 1791 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1792 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_TAGSTART);` |
|      3 | 1793 | `}` |
|      - | 1794 | `/*` |
|      - | 1795 | ` * XML_OPTION_SKIP_WHITE` |
|      - | 1796 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1797 | ` */` |
|      4 | 1798 | `static void PH7_XML_OPTION_SKIP_WHITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1799 | `{` |
|      2 | 1800 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1801 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_WHITE);` |
|      5 | 1802 | `}` |
|      - | 1803 | `/*` |
|      - | 1804 | ` * XML_SAX_IMPL.` |
|      - | 1805 | ` *   Expand the name of the underlying XML engine.` |
|      - | 1806 | ` */` |
|      2 | 1807 | `static void PH7_XML_SAX_IMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1808 | `{` |
|      1 | 1809 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1810 | `	ph7_value_string(pVal,"Symisc XML engine",(int)sizeof("Symisc XML engine")-1);` |
|      3 | 1811 | `}` |
|      - | 1812 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1813 | `/*` |
|      - | 1814 | ` * JSON_HEX_TAG.` |
|      - | 1815 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|      - | 1816 | ` */` |
|      2 | 1817 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1818 | `{` |
|      1 | 1819 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1820 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      3 | 1821 | `}` |
|      - | 1822 | `/*` |
|      - | 1823 | ` * JSON_HEX_AMP.` |
|      - | 1824 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|      - | 1825 | ` */` |
|      2 | 1826 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1827 | `{` |
|      1 | 1828 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1829 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      3 | 1830 | `}` |
|      - | 1831 | `/*` |
|      - | 1832 | ` * JSON_HEX_APOS.` |
|      - | 1833 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|      - | 1834 | ` */` |
|      2 | 1835 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1836 | `{` |
|      1 | 1837 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1838 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      3 | 1839 | `}` |
|      - | 1840 | `/*` |
|      - | 1841 | ` * JSON_HEX_QUOT.` |
|      - | 1842 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|      - | 1843 | ` */` |
|      2 | 1844 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1845 | `{` |
|      1 | 1846 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1847 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      3 | 1848 | `}` |
|      - | 1849 | `/*` |
|      - | 1850 | ` * JSON_FORCE_OBJECT.` |
|      - | 1851 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|      - | 1852 | ` */` |
|      4 | 1853 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1854 | `{` |
|      2 | 1855 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1856 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      5 | 1857 | `}` |
|      - | 1858 | `/*` |
|      - | 1859 | ` * JSON_NUMERIC_CHECK.` |
|      - | 1860 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|      - | 1861 | ` */` |
|      4 | 1862 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1863 | `{` |
|      2 | 1864 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1865 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      5 | 1866 | `}` |
|      - | 1867 | `/*` |
|      - | 1868 | ` * JSON_BIGINT_AS_STRING.` |
|      - | 1869 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|      - | 1870 | ` */` |
|      2 | 1871 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1872 | `{` |
|      1 | 1873 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1874 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      3 | 1875 | `}` |
|      - | 1876 | `/*` |
|      - | 1877 | ` * JSON_PRETTY_PRINT.` |
|      - | 1878 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|      - | 1879 | ` */` |
|      2 | 1880 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1881 | `{` |
|      1 | 1882 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1883 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      3 | 1884 | `}` |
|      - | 1885 | `/*` |
|      - | 1886 | ` * JSON_UNESCAPED_SLASHES.` |
|      - | 1887 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|      - | 1888 | ` */` |
|      4 | 1889 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1890 | `{` |
|      2 | 1891 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1892 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      5 | 1893 | `}` |
|      - | 1894 | `/*` |
|      - | 1895 | ` * JSON_UNESCAPED_UNICODE.` |
|      - | 1896 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|      - | 1897 | ` */` |
|      2 | 1898 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1899 | `{` |
|      1 | 1900 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1901 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|      3 | 1902 | `}` |
|      - | 1903 | `/*` |
|      - | 1904 | ` * JSON_ERROR_NONE.` |
|      - | 1905 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|      - | 1906 | ` */` |
|      4 | 1907 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1908 | `{` |
|      2 | 1909 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1910 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      5 | 1911 | `}` |
|      - | 1912 | `/*` |
|      - | 1913 | ` * JSON_ERROR_DEPTH.` |
|      - | 1914 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|      - | 1915 | ` */` |
|      2 | 1916 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1917 | `{` |
|      1 | 1918 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1919 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      3 | 1920 | `}` |
|      - | 1921 | `/*` |
|      - | 1922 | ` * JSON_ERROR_STATE_MISMATCH.` |
|      - | 1923 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|      - | 1924 | ` */` |
|      2 | 1925 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1926 | `{` |
|      1 | 1927 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1928 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      3 | 1929 | `}` |
|      - | 1930 | `/*` |
|      - | 1931 | ` * JSON_ERROR_CTRL_CHAR.` |
|      - | 1932 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|      - | 1933 | ` */` |
|      2 | 1934 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1935 | `{` |
|      1 | 1936 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1937 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      3 | 1938 | `}` |
|      - | 1939 | `/*` |
|      - | 1940 | ` * JSON_ERROR_SYNTAX.` |
|      - | 1941 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|      - | 1942 | ` */` |
|      4 | 1943 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1944 | `{` |
|      2 | 1945 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1946 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      5 | 1947 | `}` |
|      - | 1948 | `/*` |
|      - | 1949 | ` * JSON_ERROR_UTF8.` |
|      - | 1950 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|      - | 1951 | ` */` |
|      2 | 1952 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1953 | `{` |
|      1 | 1954 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1955 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      3 | 1956 | `}` |
|      - | 1957 | `/*` |
|      - | 1958 | ` * JSON_ERROR_NON_BACKED_ENUM.` |
|      - | 1959 | ` *   Expand the value of JSON_ERROR_NON_BACKED_ENUM defined in ph7Int.h (php 8.1).` |
|      - | 1960 | ` */` |
|    ! 0 | 1961 | `static void PH7_JSON_ERROR_NON_BACKED_ENUM_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 1962 | `{` |
|    ! 0 | 1963 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 1964 | `	ph7_value_int(pVal,JSON_ERROR_NON_BACKED_ENUM);` |
|    ! 0 | 1965 | `}` |
|      - | 1966 | `/*` |
|      - | 1967 | ` * __CLASS__` |
|      - | 1968 | ` *  The current class name, or the EMPTY STRING outside any class — php answers "",` |
|      - | 1969 | `` *  not null (`__CLASS__ === ""` is true in global scope). `self` keeps its own`` |
|      - | 1970 | ` *  expander below because php treats IT differently outside a class scope.` |
|      - | 1971 | ` */` |
|      2 | 1972 | `static void PH7_class_magic_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1973 | `{` |
|      3 | 1974 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1975 | `	ph7_class *pClass;` |
|      3 | 1976 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      3 | 1977 | `	if( pClass == 0 ){` |
|      3 | 1978 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      1 | 1979 | `	}` |
|      3 | 1980 | `	if( pClass ){` |
|    ! 0 | 1981 | `		SyString *pName = &pClass->sName;` |
|    ! 0 | 1982 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|    ! 0 | 1983 | `	}else{` |
|      3 | 1984 | `		ph7_value_string(pVal,"",0);` |
|      - | 1985 | `	}` |
|      3 | 1986 | `}` |
|      - | 1987 |  |
|      - | 1988 | `/*` |
|      - | 1989 | ` * PASSWORD_BCRYPT / PASSWORD_DEFAULT` |
|      - | 1990 | ` *  The bcrypt algorithm identifier (PHP 7.4+ exposes these as the string "2y").` |
|      - | 1991 | ` *  PASSWORD_DEFAULT tracks the recommended default, currently bcrypt.` |
|      - | 1992 | ` */` |
|     20 | 1993 | `static void PH7_PASSWORD_BCRYPT_Const(ph7_value *pVal,void *pUnused)` |
|      2 | 1994 | `{` |
|     10 | 1995 | `	SXUNUSED(pUnused);` |
|     22 | 1996 | `	ph7_value_string(pVal,"2y",(int)sizeof("2y")-1);` |
|     22 | 1997 | `}` |
|      - | 1998 | `/*` |
|      - | 1999 | ` * PASSWORD_BCRYPT_DEFAULT_COST` |
|      - | 2000 | ` *  The default bcrypt work factor used by password_hash() (currently 12).` |
|      - | 2001 | ` */` |
|      2 | 2002 | `static void PH7_PASSWORD_COST_Const(ph7_value *pVal,void *pUnused)` |
|      1 | 2003 | `{` |
|      1 | 2004 | `	SXUNUSED(pUnused);` |
|      3 | 2005 | `	ph7_value_int(pVal,12);` |
|      3 | 2006 | `}` |
|      - | 2007 | `/*` |
|      - | 2008 | ` * filter_var() filter and flag identifiers (the ext/filter constants). Values` |
|      - | 2009 | ` * match PHP 8.5. One tiny int-returning callback per constant, generated by a` |
|      - | 2010 | ` * local macro to keep the ~25 near-identical definitions DRY.` |
|      - | 2011 | ` */` |
|      - | 2012 | `#define PH7_FILTER_INT_CONST(Name,Val) \` |
|      - | 2013 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|      - | 2014 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|      - | 2015 | `	}` |
|     10 | 2016 | `PH7_FILTER_INT_CONST(FILTER_DEFAULT,516)` |
|     17 | 2017 | `PH7_FILTER_INT_CONST(FILTER_UNSAFE_RAW,516)` |
|     64 | 2018 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_INT,257)` |
|     29 | 2019 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_BOOLEAN,258)` |
|     69 | 2020 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_FLOAT,259)` |
|      8 | 2021 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_REGEXP,272)` |
|     11 | 2022 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_DOMAIN,277)` |
|     15 | 2023 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_URL,273)` |
|     28 | 2024 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_EMAIL,274)` |
|     25 | 2025 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_IP,275)` |
|     11 | 2026 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_MAC,276)` |
|      3 | 2027 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_INT,519)` |
|      5 | 2028 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_FLOAT,520)` |
|     13 | 2029 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_SPECIAL_CHARS,515)` |
|     25 | 2030 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_FULL_SPECIAL_CHARS,522)` |
|      3 | 2031 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_EMAIL,517)` |
|      3 | 2032 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_URL,518)` |
|      3 | 2033 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_OCTAL,1)` |
|      3 | 2034 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_HEX,2)` |
|      7 | 2035 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_LOW,4)` |
|      5 | 2036 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_HIGH,8)` |
|      5 | 2037 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_LOW,16)` |
|      5 | 2038 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_HIGH,32)` |
|      3 | 2039 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_AMP,64)` |
|      3 | 2040 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_ENCODE_QUOTES,128)` |
|      3 | 2041 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_BACKTICK,512)` |
|      3 | 2042 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_FRACTION,4096)` |
|     25 | 2043 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_THOUSAND,8192)` |
|      3 | 2044 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_SCIENTIFIC,16384)` |
|      5 | 2045 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV4,1048576)` |
|      3 | 2046 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV6,2097152)` |
|     14 | 2047 | `PH7_FILTER_INT_CONST(FILTER_NULL_ON_FAILURE,134217728)` |
|      - | 2048 | `/* filter_input() source selectors (php values; SESSION/REQUEST are undefined in 8.5) */` |
|      5 | 2049 | `PH7_FILTER_INT_CONST(INPUT_POST,0)` |
|      8 | 2050 | `PH7_FILTER_INT_CONST(INPUT_GET,1)` |
|      3 | 2051 | `PH7_FILTER_INT_CONST(INPUT_COOKIE,2)` |
|      3 | 2052 | `PH7_FILTER_INT_CONST(INPUT_ENV,4)` |
|     21 | 2053 | `PH7_FILTER_INT_CONST(INPUT_SERVER,5)` |
|      - | 2054 | `/*` |
|      - | 2055 | ` * Table of built-in constants.` |
|      - | 2056 | ` */` |
|      - | 2057 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|      - | 2058 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|      - | 2059 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|      - | 2060 | `	{"__PH7__",              PH7_VER_Const      },` |
|      - | 2061 | `	{"PHP_VERSION",          PH7_PHPVerConst    },` |
|      - | 2062 | `	{"PHP_MAJOR_VERSION",    PH7_PHPMajorConst  },` |
|      - | 2063 | `	{"PHP_MINOR_VERSION",    PH7_PHPMinorConst  },` |
|      - | 2064 | `	{"PHP_RELEASE_VERSION",  PH7_PHPReleaseConst},` |
|      - | 2065 | `	{"PHP_EXTRA_VERSION",    PH7_PHPExtraConst  },` |
|      - | 2066 | `	{"PHP_VERSION_ID",       PH7_PHPVerIdConst  },` |
|      - | 2067 | `	{"PHP_OS",               PH7_OS_Const       },` |
|      - | 2068 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|      - | 2069 | `	{"PHP_SESSION_DISABLED", PH7_PHP_SESSION_DISABLED_Const },` |
|      - | 2070 | `	{"PHP_SESSION_NONE",     PH7_PHP_SESSION_NONE_Const },` |
|      - | 2071 | `	{"PHP_SESSION_ACTIVE",   PH7_PHP_SESSION_ACTIVE_Const },` |
|      - | 2072 | `	{"INI_USER",             PH7_INI_USER_Const },` |
|      - | 2073 | `	{"INI_PERDIR",           PH7_INI_PERDIR_Const },` |
|      - | 2074 | `	{"INI_SYSTEM",           PH7_INI_SYSTEM_Const },` |
|      - | 2075 | `	{"INI_ALL",              PH7_INI_ALL_Const },` |
|      - | 2076 | `	{"MB_CASE_UPPER",        PH7_MB_CASE_UPPER_Const },` |
|      - | 2077 | `	{"MB_CASE_LOWER",        PH7_MB_CASE_LOWER_Const },` |
|      - | 2078 | `	{"MB_CASE_TITLE",        PH7_MB_CASE_TITLE_Const },` |
|      - | 2079 | `	{"PASSWORD_BCRYPT",      PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2080 | `	{"PASSWORD_DEFAULT",     PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2081 | `	{"PASSWORD_BCRYPT_DEFAULT_COST", PH7_PASSWORD_COST_Const },` |
|      - | 2082 | `	{"FILTER_DEFAULT",              PH7_FILTER_DEFAULT_Const },` |
|      - | 2083 | `	{"FILTER_UNSAFE_RAW",           PH7_FILTER_UNSAFE_RAW_Const },` |
|      - | 2084 | `	{"FILTER_VALIDATE_INT",         PH7_FILTER_VALIDATE_INT_Const },` |
|      - | 2085 | `	{"FILTER_VALIDATE_BOOLEAN",     PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2086 | `	{"FILTER_VALIDATE_BOOL",        PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2087 | `	{"FILTER_VALIDATE_FLOAT",       PH7_FILTER_VALIDATE_FLOAT_Const },` |
|      - | 2088 | `	{"FILTER_VALIDATE_REGEXP",      PH7_FILTER_VALIDATE_REGEXP_Const },` |
|      - | 2089 | `	{"FILTER_VALIDATE_DOMAIN",      PH7_FILTER_VALIDATE_DOMAIN_Const },` |
|      - | 2090 | `	{"FILTER_VALIDATE_URL",         PH7_FILTER_VALIDATE_URL_Const },` |
|      - | 2091 | `	{"FILTER_VALIDATE_EMAIL",       PH7_FILTER_VALIDATE_EMAIL_Const },` |
|      - | 2092 | `	{"FILTER_VALIDATE_IP",          PH7_FILTER_VALIDATE_IP_Const },` |
|      - | 2093 | `	{"FILTER_VALIDATE_MAC",         PH7_FILTER_VALIDATE_MAC_Const },` |
|      - | 2094 | `	{"FILTER_SANITIZE_NUMBER_INT",  PH7_FILTER_SANITIZE_NUMBER_INT_Const },` |
|      - | 2095 | `	{"FILTER_SANITIZE_NUMBER_FLOAT",PH7_FILTER_SANITIZE_NUMBER_FLOAT_Const },` |
|      - | 2096 | `	{"FILTER_SANITIZE_SPECIAL_CHARS",PH7_FILTER_SANITIZE_SPECIAL_CHARS_Const },` |
|      - | 2097 | `	{"FILTER_SANITIZE_FULL_SPECIAL_CHARS",PH7_FILTER_SANITIZE_FULL_SPECIAL_CHARS_Const },` |
|      - | 2098 | `	{"FILTER_SANITIZE_EMAIL",       PH7_FILTER_SANITIZE_EMAIL_Const },` |
|      - | 2099 | `	{"FILTER_SANITIZE_URL",         PH7_FILTER_SANITIZE_URL_Const },` |
|      - | 2100 | `	{"FILTER_FLAG_ALLOW_OCTAL",     PH7_FILTER_FLAG_ALLOW_OCTAL_Const },` |
|      - | 2101 | `	{"FILTER_FLAG_ALLOW_HEX",       PH7_FILTER_FLAG_ALLOW_HEX_Const },` |
|      - | 2102 | `	{"FILTER_FLAG_STRIP_LOW",       PH7_FILTER_FLAG_STRIP_LOW_Const },` |
|      - | 2103 | `	{"FILTER_FLAG_STRIP_HIGH",      PH7_FILTER_FLAG_STRIP_HIGH_Const },` |
|      - | 2104 | `	{"FILTER_FLAG_ENCODE_LOW",      PH7_FILTER_FLAG_ENCODE_LOW_Const },` |
|      - | 2105 | `	{"FILTER_FLAG_ENCODE_HIGH",     PH7_FILTER_FLAG_ENCODE_HIGH_Const },` |
|      - | 2106 | `	{"FILTER_FLAG_ENCODE_AMP",      PH7_FILTER_FLAG_ENCODE_AMP_Const },` |
|      - | 2107 | `	{"FILTER_FLAG_NO_ENCODE_QUOTES",PH7_FILTER_FLAG_NO_ENCODE_QUOTES_Const },` |
|      - | 2108 | `	{"FILTER_FLAG_STRIP_BACKTICK",  PH7_FILTER_FLAG_STRIP_BACKTICK_Const },` |
|      - | 2109 | `	{"FILTER_FLAG_ALLOW_FRACTION",  PH7_FILTER_FLAG_ALLOW_FRACTION_Const },` |
|      - | 2110 | `	{"FILTER_FLAG_ALLOW_THOUSAND",  PH7_FILTER_FLAG_ALLOW_THOUSAND_Const },` |
|      - | 2111 | `	{"FILTER_FLAG_ALLOW_SCIENTIFIC",PH7_FILTER_FLAG_ALLOW_SCIENTIFIC_Const },` |
|      - | 2112 | `	{"FILTER_FLAG_IPV4",            PH7_FILTER_FLAG_IPV4_Const },` |
|      - | 2113 | `	{"FILTER_FLAG_IPV6",            PH7_FILTER_FLAG_IPV6_Const },` |
|      - | 2114 | `	{"FILTER_NULL_ON_FAILURE",      PH7_FILTER_NULL_ON_FAILURE_Const },` |
|      - | 2115 | `	{"INPUT_POST",                  PH7_INPUT_POST_Const },` |
|      - | 2116 | `	{"INPUT_GET",                   PH7_INPUT_GET_Const },` |
|      - | 2117 | `	{"INPUT_COOKIE",                PH7_INPUT_COOKIE_Const },` |
|      - | 2118 | `	{"INPUT_ENV",                   PH7_INPUT_ENV_Const },` |
|      - | 2119 | `	{"INPUT_SERVER",                PH7_INPUT_SERVER_Const },` |
|      - | 2120 | `	{"CAL_GREGORIAN",        PH7_CAL_GREGORIAN_Const },` |
|      - | 2121 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|      - | 2122 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|      - | 2123 | `	{"PHP_INT_MIN",          PH7_INTMIN_Const   },` |
|      - | 2124 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|      - | 2125 | `	{"PHP_FLOAT_EPSILON",    PH7_FLOATEPSILON_Const },` |
|      - | 2126 | `	{"PHP_FLOAT_MAX",        PH7_FLOATMAX_Const },` |
|      - | 2127 | `	{"PHP_FLOAT_MIN",        PH7_FLOATMIN_Const },` |
|      - | 2128 | `	{"PHP_FLOAT_DIG",        PH7_FLOATDIG_Const },` |
|      - | 2129 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|      - | 2130 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|      - | 2131 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|      - | 2132 | `	{"__TIME__",             PH7_TIME_Const     },` |
|      - | 2133 | `	{"__DATE__",             PH7_DATE_Const     },` |
|      - | 2134 | `	{"__FILE__",             PH7_FILE_Const     },` |
|      - | 2135 | `	{"__DIR__",              PH7_DIR_Const      },` |
|      - | 2136 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|      - | 2137 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|      - | 2138 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|      - | 2139 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|      - | 2140 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|      - | 2141 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|      - | 2142 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|      - | 2143 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|      - | 2144 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|      - | 2145 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|      - | 2146 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|      - | 2147 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|      - | 2148 | `	{"E_STRICT",             PH7_E_STRICT_Const        },` |
|      - | 2149 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|      - | 2150 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|      - | 2151 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|      - | 2152 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|      - | 2153 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|      - | 2154 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|      - | 2155 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|      - | 2156 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|      - | 2157 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|      - | 2158 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|      - | 2159 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|      - | 2160 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|      - | 2161 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|      - | 2162 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|      - | 2163 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|      - | 2164 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|      - | 2165 | `	{"SORT_LOCALE_STRING",   PH7_SORT_LOCALE_STRING_Const },` |
|      - | 2166 | `	{"SORT_NATURAL",         PH7_SORT_NATURAL_Const },` |
|      - | 2167 | `	{"SORT_FLAG_CASE",       PH7_SORT_FLAG_CASE_Const },` |
|      - | 2168 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|      - | 2169 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|      - | 2170 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|      - | 2171 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|      - | 2172 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|      - | 2173 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|      - | 2174 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 2175 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|      - | 2176 | `	{"M_E",                  PH7_M_E_Const          },` |
|      - | 2177 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|      - | 2178 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|      - | 2179 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|      - | 2180 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|      - | 2181 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|      - | 2182 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|      - | 2183 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|      - | 2184 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|      - | 2185 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|      - | 2186 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|      - | 2187 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|      - | 2188 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|      - | 2189 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|      - | 2190 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|      - | 2191 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|      - | 2192 | `	{"NAN",                  PH7_NAN_Const          },` |
|      - | 2193 | `	{"INF",                  PH7_INF_Const          },` |
|      - | 2194 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 2195 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|      - | 2196 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|      - | 2197 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|      - | 2198 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|      - | 2199 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|      - | 2200 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|      - | 2201 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|      - | 2202 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|      - | 2203 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|      - | 2204 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|      - | 2205 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|      - | 2206 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|      - | 2207 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|      - | 2208 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|      - | 2209 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|      - | 2210 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|      - | 2211 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|      - | 2212 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|      - | 2213 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|      - | 2214 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|      - | 2215 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|      - | 2216 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|      - | 2217 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|      - | 2218 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|      - | 2219 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|      - | 2220 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|      - | 2221 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|      - | 2222 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|      - | 2223 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|      - | 2224 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|      - | 2225 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|      - | 2226 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|      - | 2227 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|      - | 2228 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|      - | 2229 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|      - | 2230 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|      - | 2231 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|      - | 2232 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|      - | 2233 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|      - | 2234 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|      - | 2235 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|      - | 2236 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|      - | 2237 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|      - | 2238 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|      - | 2239 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|      - | 2240 | `	{"ASSERT_ACTIVE",        PH7_ASSERT_ACTIVE_Const     },` |
|      - | 2241 | `	{"ASSERT_CALLBACK",      PH7_ASSERT_CALLBACK_Const   },` |
|      - | 2242 | `	{"ASSERT_BAIL",          PH7_ASSERT_BAIL_Const       },` |
|      - | 2243 | `	{"ASSERT_WARNING",       PH7_ASSERT_WARNING_Const    },` |
|      - | 2244 | `	{"ASSERT_EXCEPTION",     PH7_ASSERT_EXCEPTION_Const  },` |
|      - | 2245 | `	/* ASSERT_QUIET_EVAL was REMOVED in php 8.0: referencing it is an Error there */` |
|      - | 2246 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|      - | 2247 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|      - | 2248 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|      - | 2249 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|      - | 2250 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|      - | 2251 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|      - | 2252 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|      - | 2253 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|      - | 2254 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|      - | 2255 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|      - | 2256 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|      - | 2257 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|      - | 2258 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|      - | 2259 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|      - | 2260 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|      - | 2261 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|      - | 2262 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|      - | 2263 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|      - | 2264 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|      - | 2265 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|      - | 2266 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|      - | 2267 | `	{"STDIN",                PH7_STDIN_Const        },` |
|      - | 2268 | `	{"stdin",                PH7_STDIN_Const        },` |
|      - | 2269 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|      - | 2270 | `	{"stdout",               PH7_STDOUT_Const       },` |
|      - | 2271 | `	{"STDERR",               PH7_STDERR_Const       },` |
|      - | 2272 | `	{"stderr",               PH7_STDERR_Const       },` |
|      - | 2273 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|      - | 2274 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|      - | 2275 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|      - | 2276 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|      - | 2277 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|      - | 2278 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|      - | 2279 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|      - | 2280 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|      - | 2281 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|      - | 2282 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2283 | `	{"XML_ERROR_NONE",       PH7_XML_ERROR_NONE_Const},` |
|      - | 2284 | `	{"XML_ERROR_NO_MEMORY",  PH7_XML_ERROR_NO_MEMORY_Const},` |
|      - | 2285 | `	{"XML_ERROR_SYNTAX",     PH7_XML_ERROR_SYNTAX_Const},` |
|      - | 2286 | `	{"XML_ERROR_NO_ELEMENTS",PH7_XML_ERROR_NO_ELEMENTS_Const},` |
|      - | 2287 | `	{"XML_ERROR_INVALID_TOKEN", PH7_XML_ERROR_INVALID_TOKEN_Const},` |
|      - | 2288 | `	{"XML_ERROR_UNCLOSED_TOKEN",PH7_XML_ERROR_UNCLOSED_TOKEN_Const},` |
|      - | 2289 | `	{"XML_ERROR_PARTIAL_CHAR",  PH7_XML_ERROR_PARTIAL_CHAR_Const},` |
|      - | 2290 | `	{"XML_ERROR_TAG_MISMATCH",  PH7_XML_ERROR_TAG_MISMATCH_Const},` |
|      - | 2291 | `	{"XML_ERROR_DUPLICATE_ATTRIBUTE",   PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const},` |
|      - | 2292 | `	{"XML_ERROR_JUNK_AFTER_DOC_ELEMENT",PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const},` |
|      - | 2293 | `	{"XML_ERROR_PARAM_ENTITY_REF",      PH7_XML_ERROR_PARAM_ENTITY_REF_Const},` |
|      - | 2294 | `	{"XML_ERROR_UNDEFINED_ENTITY",      PH7_XML_ERROR_UNDEFINED_ENTITY_Const},` |
|      - | 2295 | `	{"XML_ERROR_RECURSIVE_ENTITY_REF",  PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const},` |
|      - | 2296 | `	{"XML_ERROR_ASYNC_ENTITY",          PH7_XML_ERROR_ASYNC_ENTITY_Const},` |
|      - | 2297 | `	{"XML_ERROR_BAD_CHAR_REF",          PH7_XML_ERROR_BAD_CHAR_REF_Const},` |
|      - | 2298 | `	{"XML_ERROR_BINARY_ENTITY_REF",     PH7_XML_ERROR_BINARY_ENTITY_REF_Const},` |
|      - | 2299 | `	{"XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF", PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const},` |
|      - | 2300 | `	{"XML_ERROR_MISPLACED_XML_PI",     PH7_XML_ERROR_MISPLACED_XML_PI_Const},` |
|      - | 2301 | `	{"XML_ERROR_UNKNOWN_ENCODING",     PH7_XML_ERROR_UNKNOWN_ENCODING_Const},` |
|      - | 2302 | `	{"XML_ERROR_INCORRECT_ENCODING",   PH7_XML_ERROR_INCORRECT_ENCODING_Const},` |
|      - | 2303 | `	{"XML_ERROR_UNCLOSED_CDATA_SECTION",  PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const},` |
|      - | 2304 | `	{"XML_ERROR_EXTERNAL_ENTITY_HANDLING",PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const},` |
|      - | 2305 | `	{"XML_OPTION_CASE_FOLDING",           PH7_XML_OPTION_CASE_FOLDING_Const},` |
|      - | 2306 | `	{"XML_OPTION_TARGET_ENCODING",        PH7_XML_OPTION_TARGET_ENCODING_Const},` |
|      - | 2307 | `	{"XML_OPTION_SKIP_TAGSTART",          PH7_XML_OPTION_SKIP_TAGSTART_Const},` |
|      - | 2308 | `	{"XML_OPTION_SKIP_WHITE",             PH7_XML_OPTION_SKIP_WHITE_Const},` |
|      - | 2309 | `	{"XML_SAX_IMPL",           PH7_XML_SAX_IMP_Const},` |
|      - | 2310 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2311 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|      - | 2312 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|      - | 2313 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|      - | 2314 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|      - | 2315 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|      - | 2316 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|      - | 2317 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|      - | 2318 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|      - | 2319 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|      - | 2320 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|      - | 2321 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|      - | 2322 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|      - | 2323 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|      - | 2324 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|      - | 2325 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|      - | 2326 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|      - | 2327 | `	{"JSON_ERROR_NON_BACKED_ENUM", PH7_JSON_ERROR_NON_BACKED_ENUM_Const},` |
|      - | 2328 | ``	/* `self`, `parent` and `static` are KEYWORDS in php, not constants: using one as a bare`` |
|      - | 2329 | ``	 * word is an "Undefined constant" Error (or a parse error for `static`). PH7 registered`` |
|      - | 2330 | `	 * them as constants that quietly expanded to the class name / NULL, so a typo'd bare` |
|      - | 2331 | ``	 * word silently produced a value. The `self::`/`parent::`/`static::` forms are handled`` |
|      - | 2332 | ``	 * by the `::` compile path and do not go through the constant table. */`` |
|      - | 2333 | `	{"__CLASS__",            PH7_class_magic_Const  }` |
|      - | 2334 | `};` |
|      - | 2335 | `/*` |
|      - | 2336 | ` * Register the built-in constants defined above.` |
|      - | 2337 | ` */` |
|   3356 | 2338 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|      5 | 2339 | `{` |
|      - | 2340 | `	sxu32 n;` |
|      - | 2341 | `	/*` |
|      - | 2342 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|      - | 2343 | `	 * that trigger the constant invocation as their private data.` |
|      - | 2344 | `	 */` |
| 896057 | 2345 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 892701 | 2346 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
| 446353 | 2347 | `	}` |
|   3361 | 2348 | `}` |
