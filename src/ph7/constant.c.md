# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1154/1174 lines (98.30%)

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
|   3976 |   63 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|      5 |   64 | `{` |
|      - |   65 | `#if defined(__WINNT__)` |
|      5 |   66 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|      - |   67 | `#elif defined(__UNIXES__)` |
|      - |   68 | `	struct utsname sInfo;` |
|   3976 |   69 | `	if( uname(&sInfo) != 0 ){` |
|    ! 0 |   70 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 |   71 | `	}else{` |
|   3976 |   72 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|      - |   73 | `	}` |
|      - |   74 | `#else` |
|      - |   75 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|      - |   76 | `#endif` |
|   1988 |   77 | `	SXUNUSED(pUnused);` |
|   3981 |   78 | `}` |
|      - |   79 | `/*` |
|      - |   80 | ` * PHP_EOL` |
|      - |   81 | ` *  Expand the correct 'End Of Line' symbol for this platform.` |
|      - |   82 | ` */` |
|    832 |   83 | `static void PH7_EOL_Const(ph7_value *pVal,void *pUnused)` |
|      4 |   84 | `{` |
|    416 |   85 | `	SXUNUSED(pUnused);` |
|      - |   86 | `#ifdef __WINNT__` |
|      4 |   87 | `	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);` |
|      - |   88 | `#else` |
|    832 |   89 | `	ph7_value_string(pVal,"\n",(int)sizeof(char));` |
|      - |   90 | `#endif` |
|    836 |   91 | `}` |
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
|      - |  102 | `/*` |
|      - |  103 | ` * PHP_INT_MIN (php 7.0)` |
|      - |  104 | ` * Expand the smallest integer supported.` |
|      - |  105 | ` */` |
|     46 |  106 | `static void PH7_INTMIN_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  107 | `{` |
|     23 |  108 | `	SXUNUSED(pUnused);` |
|     47 |  109 | `	ph7_value_int64(pVal,SMALLEST_INT64);` |
|     47 |  110 | `}` |
|      - |  111 | `/*` |
|      - |  112 | ` * PHP_INT_SIZE` |
|      - |  113 | ` * Expand the size in bytes of a 64-bit integer.` |
|      - |  114 | ` */` |
|      4 |  115 | `static void PH7_INTSIZE_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  116 | `{` |
|      2 |  117 | `	SXUNUSED(pUnused);` |
|      5 |  118 | `	ph7_value_int64(pVal,sizeof(sxi64));` |
|      5 |  119 | `}` |
|      - |  120 | `/*` |
|      - |  121 | ` * PHP_FLOAT_EPSILON / PHP_FLOAT_MAX / PHP_FLOAT_MIN / PHP_FLOAT_DIG (php 7.2)` |
|      - |  122 | ` * Double-precision characteristics, sourced from <float.h> exactly like php` |
|      - |  123 | ` * so they track the compiling platform's actual double representation.` |
|      - |  124 | ` */` |
|      4 |  125 | `static void PH7_FLOATEPSILON_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  126 | `{` |
|      2 |  127 | `	SXUNUSED(pUnused);` |
|      5 |  128 | `	ph7_value_double(pVal,DBL_EPSILON);` |
|      5 |  129 | `}` |
|      2 |  130 | `static void PH7_FLOATMAX_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  131 | `{` |
|      1 |  132 | `	SXUNUSED(pUnused);` |
|      3 |  133 | `	ph7_value_double(pVal,DBL_MAX);` |
|      3 |  134 | `}` |
|      2 |  135 | `static void PH7_FLOATMIN_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  136 | `{` |
|      1 |  137 | `	SXUNUSED(pUnused);` |
|      3 |  138 | `	ph7_value_double(pVal,DBL_MIN);` |
|      3 |  139 | `}` |
|      2 |  140 | `static void PH7_FLOATDIG_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  141 | `{` |
|      1 |  142 | `	SXUNUSED(pUnused);` |
|      3 |  143 | `	ph7_value_int64(pVal,DBL_DIG);` |
|      3 |  144 | `}` |
|      - |  145 | `/*` |
|      - |  146 | ` * DIRECTORY_SEPARATOR.` |
|      - |  147 | ` * Expand the directory separator character.` |
|      - |  148 | ` */` |
|    164 |  149 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|      3 |  150 | `{` |
|     82 |  151 | `	SXUNUSED(pUnused);` |
|      - |  152 | `#ifdef __WINNT__` |
|      3 |  153 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|      - |  154 | `#else` |
|    164 |  155 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|      - |  156 | `#endif` |
|    167 |  157 | `}` |
|      - |  158 | `/*` |
|      - |  159 | ` * PATH_SEPARATOR.` |
|      - |  160 | ` * Expand the path separator character.` |
|      - |  161 | ` */` |
|      2 |  162 | `static void PH7_PATHSEP_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  163 | `{` |
|      1 |  164 | `	SXUNUSED(pUnused);` |
|      - |  165 | `#ifdef __WINNT__` |
|      1 |  166 | `	ph7_value_string(pVal,";",(int)sizeof(char));` |
|      - |  167 | `#else` |
|      2 |  168 | `	ph7_value_string(pVal,":",(int)sizeof(char));` |
|      - |  169 | `#endif` |
|      3 |  170 | `}` |
|      - |  171 |  |
|      - |  172 | `#if defined(PH7_ENABLE_MATH_FUNC)` |
|      - |  173 | `/*` |
|      - |  174 | ` * NAN constant: floating-point Not-A-Number` |
|      - |  175 | ` */` |
|     82 |  176 | `static void PH7_NAN_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  177 | `{` |
|     41 |  178 | `	SXUNUSED(pUnused);` |
|     83 |  179 | `	ph7_value_double(pVal, PH7_NAN_VALUE());` |
|     83 |  180 | `}` |
|      - |  181 |  |
|      - |  182 | `/*` |
|      - |  183 | ` * INF constant: positive infinity` |
|      - |  184 | ` */` |
|     70 |  185 | `static void PH7_INF_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  186 | `{` |
|     35 |  187 | `	SXUNUSED(pUnused);` |
|      - |  188 | `	/* similarly avoid the INFINITY macro */` |
|     71 |  189 | `	ph7_value_double(pVal, PH7_INF_VALUE());` |
|     71 |  190 | `}` |
|      - |  191 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - |  192 |  |
|      - |  193 | `#ifndef __WINNT__` |
|      - |  194 | `#include <time.h>` |
|      - |  195 | `#endif` |
|      - |  196 | `/*` |
|      - |  197 | ` * __TIME__` |
|      - |  198 | ` *  Expand the current time (GMT).` |
|      - |  199 | ` */` |
|      2 |  200 | `static void PH7_TIME_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  201 | `{` |
|      - |  202 | `	Sytm sTm;` |
|      - |  203 | `#ifdef __WINNT__` |
|      - |  204 | `	SYSTEMTIME sOS;` |
|      1 |  205 | `	GetSystemTime(&sOS);` |
|      1 |  206 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - |  207 | `#else` |
|      - |  208 | `	struct tm *pTm;` |
|      - |  209 | `	time_t t;` |
|      2 |  210 | `	time(&t);` |
|      2 |  211 | `	pTm = gmtime(&t);` |
|      2 |  212 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - |  213 | `#endif` |
|      1 |  214 | `	SXUNUSED(pUnused); /* cc warning */` |
|      - |  215 | `	/* Expand */` |
|      3 |  216 | `	ph7_value_string_format(pVal,"%02d:%02d:%02d",sTm.tm_hour,sTm.tm_min,sTm.tm_sec);` |
|      3 |  217 | `}` |
|      - |  218 | `/*` |
|      - |  219 | ` * __DATE__` |
|      - |  220 | ` *  Expand the current date in the ISO-8601 format.` |
|      - |  221 | ` */` |
|      2 |  222 | `static void PH7_DATE_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  223 | `{` |
|      - |  224 | `	Sytm sTm;` |
|      - |  225 | `#ifdef __WINNT__` |
|      - |  226 | `	SYSTEMTIME sOS;` |
|      1 |  227 | `	GetSystemTime(&sOS);` |
|      1 |  228 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - |  229 | `#else` |
|      - |  230 | `	struct tm *pTm;` |
|      - |  231 | `	time_t t;` |
|      2 |  232 | `	time(&t);` |
|      2 |  233 | `	pTm = gmtime(&t);` |
|      2 |  234 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - |  235 | `#endif` |
|      1 |  236 | `	SXUNUSED(pUnused); /* cc warning */` |
|      - |  237 | `	/* Expand */` |
|      3 |  238 | `	ph7_value_string_format(pVal,"%04d-%02d-%02d",sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|      3 |  239 | `}` |
|      - |  240 | `/*` |
|      - |  241 | ` * __FILE__` |
|      - |  242 | ` *  Path of the processed script.` |
|      - |  243 | ` */` |
|   2484 |  244 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  245 | `{` |
|   2489 |  246 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  247 | `	SyString *pFile;` |
|      - |  248 | `	/* Peek the top entry */` |
|   2489 |  249 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|   2489 |  250 | `	if( pFile == 0 ){` |
|      - |  251 | `		/* Expand the magic word: ":MEMORY:" */` |
|    ! 0 |  252 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|    ! 0 |  253 | `	}else{` |
|   2489 |  254 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|      - |  255 | `	}` |
|   2489 |  256 | `}` |
|      - |  257 | `/*` |
|      - |  258 | ` * __DIR__` |
|      - |  259 | ` *  Directory holding the processed script.` |
|      - |  260 | ` */` |
|     40 |  261 | `static void PH7_DIR_Const(ph7_value *pVal,void *pUserData)` |
|      4 |  262 | `{` |
|     44 |  263 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  264 | `	SyString *pFile;` |
|      - |  265 | `	/* Peek the top entry */` |
|     44 |  266 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     44 |  267 | `	if( pFile == 0 ){` |
|      - |  268 | `		/* Expand the magic word: ":MEMORY:" */` |
|    ! 0 |  269 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|    ! 0 |  270 | `	}else{` |
|     44 |  271 | `		if( pFile->nByte > 0 ){` |
|      - |  272 | `			const char *zDir;` |
|      - |  273 | `			int nLen;` |
|     44 |  274 | `			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|     44 |  275 | `			ph7_value_string(pVal,zDir,nLen);` |
|     24 |  276 | `		}else{` |
|      - |  277 | `			/* Expand '.' as the current directory*/` |
|    ! 0 |  278 | `			ph7_value_string(pVal,".",(int)sizeof(char));` |
|      - |  279 | `		}` |
|      - |  280 | `	}` |
|     44 |  281 | `}` |
|      - |  282 | `/*` |
|      - |  283 | ` * PHP_SHLIB_SUFFIX` |
|      - |  284 | ` *  Expand shared library suffix.` |
|      - |  285 | ` */` |
|      2 |  286 | `static void PH7_PHP_SHLIB_SUFFIX_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 |  287 | `{` |
|      - |  288 | `#ifdef __WINNT__` |
|    ! 0 |  289 | `	ph7_value_string(pVal,"dll",(int)sizeof("dll")-1);` |
|      - |  290 | `#else` |
|      2 |  291 | `	ph7_value_string(pVal,"so",(int)sizeof("so")-1);` |
|      - |  292 | `#endif` |
|      1 |  293 | `	SXUNUSED(pUserData); /* cc warning */` |
|      2 |  294 | `}` |
|      - |  295 | `/*` |
|      - |  296 | ` * E_ERROR` |
|      - |  297 | ` *  Expands 1` |
|      - |  298 | ` */` |
|      2 |  299 | `static void PH7_E_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  300 | `{` |
|      3 |  301 | `	ph7_value_int(pVal,1);` |
|      1 |  302 | `	SXUNUSED(pUserData);` |
|      3 |  303 | `}` |
|      - |  304 | `/*` |
|      - |  305 | ` * E_WARNING` |
|      - |  306 | ` *  Expands 2` |
|      - |  307 | ` */` |
|      2 |  308 | `static void PH7_E_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  309 | `{` |
|      3 |  310 | `	ph7_value_int(pVal,2);` |
|      1 |  311 | `	SXUNUSED(pUserData);` |
|      3 |  312 | `}` |
|      - |  313 | `/*` |
|      - |  314 | ` * E_PARSE` |
|      - |  315 | ` *  Expands 4` |
|      - |  316 | ` */` |
|      2 |  317 | `static void PH7_E_PARSE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  318 | `{` |
|      3 |  319 | `	ph7_value_int(pVal,4);` |
|      1 |  320 | `	SXUNUSED(pUserData);` |
|      3 |  321 | `}` |
|      - |  322 | `/*` |
|      - |  323 | ` * E_NOTICE` |
|      - |  324 | ` * Expands 8` |
|      - |  325 | ` */` |
|      2 |  326 | `static void PH7_E_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  327 | `{` |
|      3 |  328 | `	ph7_value_int(pVal,8);` |
|      1 |  329 | `	SXUNUSED(pUserData);` |
|      3 |  330 | `}` |
|      - |  331 | `/*` |
|      - |  332 | ` * E_CORE_ERROR` |
|      - |  333 | ` * Expands 16` |
|      - |  334 | ` */` |
|      2 |  335 | `static void PH7_E_CORE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  336 | `{` |
|      3 |  337 | `	ph7_value_int(pVal,16);` |
|      1 |  338 | `	SXUNUSED(pUserData);` |
|      3 |  339 | `}` |
|      - |  340 | `/*` |
|      - |  341 | ` * E_CORE_WARNING` |
|      - |  342 | ` * Expands 32` |
|      - |  343 | ` */` |
|      2 |  344 | `static void PH7_E_CORE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  345 | `{` |
|      3 |  346 | `	ph7_value_int(pVal,32);` |
|      1 |  347 | `	SXUNUSED(pUserData);` |
|      3 |  348 | `}` |
|      - |  349 | `/*` |
|      - |  350 | ` * E_COMPILE_ERROR` |
|      - |  351 | ` * Expands 64` |
|      - |  352 | ` */` |
|      2 |  353 | `static void PH7_E_COMPILE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  354 | `{` |
|      3 |  355 | `	ph7_value_int(pVal,64);` |
|      1 |  356 | `	SXUNUSED(pUserData);` |
|      3 |  357 | `}` |
|      - |  358 | `/*` |
|      - |  359 | ` * E_COMPILE_WARNING` |
|      - |  360 | ` * Expands 128` |
|      - |  361 | ` */` |
|      2 |  362 | `static void PH7_E_COMPILE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  363 | `{` |
|      3 |  364 | `	ph7_value_int(pVal,128);` |
|      1 |  365 | `	SXUNUSED(pUserData);` |
|      3 |  366 | `}` |
|      - |  367 | `/*` |
|      - |  368 | ` * E_USER_ERROR` |
|      - |  369 | ` * Expands 256` |
|      - |  370 | ` */` |
|      2 |  371 | `static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  372 | `{` |
|      3 |  373 | `	ph7_value_int(pVal,256);` |
|      1 |  374 | `	SXUNUSED(pUserData);` |
|      3 |  375 | `}` |
|      - |  376 | `/*` |
|      - |  377 | ` * E_USER_WARNING` |
|      - |  378 | ` * Expands 512` |
|      - |  379 | ` */` |
|     12 |  380 | `static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      3 |  381 | `{` |
|     15 |  382 | `	ph7_value_int(pVal,512);` |
|      6 |  383 | `	SXUNUSED(pUserData);` |
|     15 |  384 | `}` |
|      - |  385 | `/*` |
|      - |  386 | ` * E_USER_NOTICE` |
|      - |  387 | ` * Expands 1024` |
|      - |  388 | ` */` |
|      6 |  389 | `static void PH7_E_USER_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|      3 |  390 | `{` |
|      9 |  391 | `	ph7_value_int(pVal,1024);` |
|      3 |  392 | `	SXUNUSED(pUserData);` |
|      9 |  393 | `}` |
|      - |  394 | `/*` |
|      - |  395 | ` * E_STRICT` |
|      - |  396 | ` * Expands 2048` |
|      - |  397 | ` */` |
|      2 |  398 | `static void PH7_E_STRICT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  399 | `{` |
|      3 |  400 | `	ph7_value_int(pVal,2048);` |
|      1 |  401 | `	SXUNUSED(pUserData);` |
|      3 |  402 | `}` |
|      - |  403 | `/*` |
|      - |  404 | ` * E_RECOVERABLE_ERROR` |
|      - |  405 | ` * Expands 4096` |
|      - |  406 | ` */` |
|      2 |  407 | `static void PH7_E_RECOVERABLE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  408 | `{` |
|      3 |  409 | `	ph7_value_int(pVal,4096);` |
|      1 |  410 | `	SXUNUSED(pUserData);` |
|      3 |  411 | `}` |
|      - |  412 | `/*` |
|      - |  413 | ` * E_DEPRECATED` |
|      - |  414 | ` * Expands 8192` |
|      - |  415 | ` */` |
|     22 |  416 | `static void PH7_E_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  417 | `{` |
|     27 |  418 | `	ph7_value_int(pVal,8192);` |
|     11 |  419 | `	SXUNUSED(pUserData);` |
|     27 |  420 | `}` |
|      - |  421 | `/*` |
|      - |  422 | ` * E_USER_DEPRECATED` |
|      - |  423 | ` *   Expands 16384.` |
|      - |  424 | ` */` |
|      2 |  425 | `static void PH7_E_USER_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  426 | `{` |
|      3 |  427 | `	ph7_value_int(pVal,16384);` |
|      1 |  428 | `	SXUNUSED(pUserData);` |
|      3 |  429 | `}` |
|      - |  430 | `/*` |
|      - |  431 | ` * E_ALL` |
|      - |  432 | ` *  Expands 32767` |
|      - |  433 | ` */` |
|     26 |  434 | `static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  435 | `{` |
|     31 |  436 | `	ph7_value_int(pVal,32767);` |
|     13 |  437 | `	SXUNUSED(pUserData);` |
|     31 |  438 | `}` |
|      - |  439 | `/*` |
|      - |  440 | ` * CASE_LOWER` |
|      - |  441 | ` *  Expands 0.` |
|      - |  442 | ` */` |
|      2 |  443 | `static void PH7_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  444 | `{` |
|      3 |  445 | `	ph7_value_int(pVal,0);` |
|      1 |  446 | `	SXUNUSED(pUserData);` |
|      3 |  447 | `}` |
|      - |  448 | `/*` |
|      - |  449 | ` * CASE_UPPER` |
|      - |  450 | ` *  Expands 1.` |
|      - |  451 | ` */` |
|      2 |  452 | `static void PH7_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  453 | `{` |
|      3 |  454 | `	ph7_value_int(pVal,1);` |
|      1 |  455 | `	SXUNUSED(pUserData);` |
|      3 |  456 | `}` |
|      - |  457 | `/*` |
|      - |  458 | ` * STR_PAD_LEFT` |
|      - |  459 | ` *  Expands 0.` |
|      - |  460 | ` */` |
|      4 |  461 | `static void PH7_STR_PAD_LEFT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  462 | `{` |
|      5 |  463 | `	ph7_value_int(pVal,0);` |
|      2 |  464 | `	SXUNUSED(pUserData);` |
|      5 |  465 | `}` |
|      - |  466 | `/*` |
|      - |  467 | ` * STR_PAD_RIGHT` |
|      - |  468 | ` *  Expands 1.` |
|      - |  469 | ` */` |
|      4 |  470 | `static void PH7_STR_PAD_RIGHT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  471 | `{` |
|      5 |  472 | `	ph7_value_int(pVal,1);` |
|      2 |  473 | `	SXUNUSED(pUserData);` |
|      5 |  474 | `}` |
|      - |  475 | `/*` |
|      - |  476 | ` * STR_PAD_BOTH` |
|      - |  477 | ` *  Expands 2.` |
|      - |  478 | ` */` |
|      2 |  479 | `static void PH7_STR_PAD_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  480 | `{` |
|      3 |  481 | `	ph7_value_int(pVal,2);` |
|      1 |  482 | `	SXUNUSED(pUserData);` |
|      3 |  483 | `}` |
|      - |  484 | `/*` |
|      - |  485 | ` * COUNT_NORMAL` |
|      - |  486 | ` *  Expands 0` |
|      - |  487 | ` */` |
|      6 |  488 | `static void PH7_COUNT_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  489 | `{` |
|      8 |  490 | `	ph7_value_int(pVal,0);` |
|      3 |  491 | `	SXUNUSED(pUserData);` |
|      8 |  492 | `}` |
|      - |  493 | `/*` |
|      - |  494 | ` * COUNT_RECURSIVE` |
|      - |  495 | ` *  Expands 1.` |
|      - |  496 | ` */` |
|     20 |  497 | `static void PH7_COUNT_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  498 | `{` |
|     22 |  499 | `	ph7_value_int(pVal,1);` |
|     10 |  500 | `	SXUNUSED(pUserData);` |
|     22 |  501 | `}` |
|      - |  502 | `/*` |
|      - |  503 | ` * SORT_ASC` |
|      - |  504 | ` *  Expands 1.` |
|      - |  505 | ` */` |
|      2 |  506 | `static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  507 | `{` |
|      3 |  508 | `	ph7_value_int(pVal,1);` |
|      1 |  509 | `	SXUNUSED(pUserData);` |
|      3 |  510 | `}` |
|      - |  511 | `/*` |
|      - |  512 | ` * SORT_DESC` |
|      - |  513 | ` *  Expands 2.` |
|      - |  514 | ` */` |
|      2 |  515 | `static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  516 | `{` |
|      3 |  517 | `	ph7_value_int(pVal,2);` |
|      1 |  518 | `	SXUNUSED(pUserData);` |
|      3 |  519 | `}` |
|      - |  520 | `/*` |
|      - |  521 | ` * SORT_REGULAR` |
|      - |  522 | ` *  Expands 3.` |
|      - |  523 | ` */` |
|      4 |  524 | `static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  525 | `{` |
|      5 |  526 | `	ph7_value_int(pVal,3);` |
|      2 |  527 | `	SXUNUSED(pUserData);` |
|      5 |  528 | `}` |
|      - |  529 | `/*` |
|      - |  530 | ` * SORT_NUMERIC` |
|      - |  531 | ` *  Expands 4.` |
|      - |  532 | ` */` |
|      6 |  533 | `static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  534 | `{` |
|      7 |  535 | `	ph7_value_int(pVal,4);` |
|      3 |  536 | `	SXUNUSED(pUserData);` |
|      7 |  537 | `}` |
|      - |  538 | `/*` |
|      - |  539 | ` * SORT_STRING` |
|      - |  540 | ` *  Expands 5.` |
|      - |  541 | ` */` |
|     10 |  542 | `static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  543 | `{` |
|     11 |  544 | `	ph7_value_int(pVal,5);` |
|      5 |  545 | `	SXUNUSED(pUserData);` |
|     11 |  546 | `}` |
|      - |  547 | `/*` |
|      - |  548 | ` * PHP_ROUND_HALF_UP` |
|      - |  549 | ` *  Expands 1.` |
|      - |  550 | ` */` |
|      4 |  551 | `static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  552 | `{` |
|      5 |  553 | `	ph7_value_int(pVal,1);` |
|      2 |  554 | `	SXUNUSED(pUserData);` |
|      5 |  555 | `}` |
|      - |  556 | `/*` |
|      - |  557 | ` * PHP_SESSION_DISABLED / PHP_SESSION_NONE / PHP_SESSION_ACTIVE` |
|      - |  558 | ` *  session_status() states (0 / 1 / 2).` |
|      - |  559 | ` */` |
|      2 |  560 | `static void PH7_PHP_SESSION_DISABLED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  561 | `{` |
|      3 |  562 | `	ph7_value_int(pVal,0);` |
|      1 |  563 | `	SXUNUSED(pUserData);` |
|      3 |  564 | `}` |
|      6 |  565 | `static void PH7_PHP_SESSION_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  566 | `{` |
|      7 |  567 | `	ph7_value_int(pVal,1);` |
|      3 |  568 | `	SXUNUSED(pUserData);` |
|      7 |  569 | `}` |
|     30 |  570 | `static void PH7_PHP_SESSION_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  571 | `{` |
|     32 |  572 | `	ph7_value_int(pVal,2);` |
|     15 |  573 | `	SXUNUSED(pUserData);` |
|     32 |  574 | `}` |
|      - |  575 | `/*` |
|      - |  576 | ` * INI_USER / INI_PERDIR / INI_SYSTEM / INI_ALL` |
|      - |  577 | ` *  php.ini access levels (1 / 2 / 4 / 7).` |
|      - |  578 | ` */` |
|     12 |  579 | `static void PH7_INI_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  580 | `{` |
|     13 |  581 | `	ph7_value_int(pVal,1);` |
|      6 |  582 | `	SXUNUSED(pUserData);` |
|     13 |  583 | `}` |
|      2 |  584 | `static void PH7_INI_PERDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  585 | `{` |
|      3 |  586 | `	ph7_value_int(pVal,2);` |
|      1 |  587 | `	SXUNUSED(pUserData);` |
|      3 |  588 | `}` |
|      2 |  589 | `static void PH7_INI_SYSTEM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  590 | `{` |
|      3 |  591 | `	ph7_value_int(pVal,4);` |
|      1 |  592 | `	SXUNUSED(pUserData);` |
|      3 |  593 | `}` |
|      2 |  594 | `static void PH7_INI_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  595 | `{` |
|      3 |  596 | `	ph7_value_int(pVal,7);` |
|      1 |  597 | `	SXUNUSED(pUserData);` |
|      3 |  598 | `}` |
|      - |  599 | `/*` |
|      - |  600 | ` * MB_CASE_UPPER / MB_CASE_LOWER / MB_CASE_TITLE (0 / 1 / 2)` |
|      - |  601 | ` */` |
|      4 |  602 | `static void PH7_MB_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  603 | `{` |
|      5 |  604 | `	ph7_value_int(pVal,0);` |
|      2 |  605 | `	SXUNUSED(pUserData);` |
|      5 |  606 | `}` |
|      4 |  607 | `static void PH7_MB_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  608 | `{` |
|      5 |  609 | `	ph7_value_int(pVal,1);` |
|      2 |  610 | `	SXUNUSED(pUserData);` |
|      5 |  611 | `}` |
|      4 |  612 | `static void PH7_MB_CASE_TITLE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  613 | `{` |
|      5 |  614 | `	ph7_value_int(pVal,2);` |
|      2 |  615 | `	SXUNUSED(pUserData);` |
|      5 |  616 | `}` |
|      - |  617 | `/*` |
|      - |  618 | ` * SPHP_ROUND_HALF_DOWN` |
|      - |  619 | ` *  Expands 2.` |
|      - |  620 | ` */` |
|      4 |  621 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  622 | `{` |
|      5 |  623 | `	ph7_value_int(pVal,2);` |
|      2 |  624 | `	SXUNUSED(pUserData);` |
|      5 |  625 | `}` |
|      - |  626 | `/*` |
|      - |  627 | ` * PHP_ROUND_HALF_EVEN` |
|      - |  628 | ` *  Expands 3.` |
|      - |  629 | ` */` |
|      8 |  630 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  631 | `{` |
|      9 |  632 | `	ph7_value_int(pVal,3);` |
|      4 |  633 | `	SXUNUSED(pUserData);` |
|      9 |  634 | `}` |
|      - |  635 | `/*` |
|      - |  636 | ` * PHP_ROUND_HALF_ODD` |
|      - |  637 | ` *  Expands 4.` |
|      - |  638 | ` */` |
|      4 |  639 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  640 | `{` |
|      5 |  641 | `	ph7_value_int(pVal,4);` |
|      2 |  642 | `	SXUNUSED(pUserData);` |
|      5 |  643 | `}` |
|      - |  644 | `/*` |
|      - |  645 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|      - |  646 | ` *  Expand 0x01` |
|      - |  647 | ` * NOTE:` |
|      - |  648 | ` *  The expanded value must be a power of two.` |
|      - |  649 | ` */` |
|      2 |  650 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  651 | `{` |
|      3 |  652 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      1 |  653 | `	SXUNUSED(pUserData);` |
|      3 |  654 | `}` |
|      - |  655 | `/*` |
|      - |  656 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|      - |  657 | ` *  Expand 0x02` |
|      - |  658 | ` * NOTE:` |
|      - |  659 | ` *  The expanded value must be a power of two.` |
|      - |  660 | ` */` |
|      2 |  661 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  662 | `{` |
|      3 |  663 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      1 |  664 | `	SXUNUSED(pUserData);` |
|      3 |  665 | `}` |
|      - |  666 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  667 | `/*` |
|      - |  668 | ` * M_PI` |
|      - |  669 | ` *  Expand the value of pi.` |
|      - |  670 | ` */` |
|      2 |  671 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  672 | `{` |
|      1 |  673 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  674 | `	ph7_value_double(pVal,PH7_PI);` |
|      3 |  675 | `}` |
|      - |  676 | `/*` |
|      - |  677 | ` * M_E` |
|      - |  678 | ` *  Expand 2.7182818284590452354` |
|      - |  679 | ` */` |
|      2 |  680 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  681 | `{` |
|      1 |  682 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  683 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|      3 |  684 | `}` |
|      - |  685 | `/*` |
|      - |  686 | ` * M_LOG2E` |
|      - |  687 | ` *  Expand 2.7182818284590452354` |
|      - |  688 | ` */` |
|      2 |  689 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  690 | `{` |
|      1 |  691 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  692 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      3 |  693 | `}` |
|      - |  694 | `/*` |
|      - |  695 | ` * M_LOG10E` |
|      - |  696 | ` *  Expand 0.4342944819032518276` |
|      - |  697 | ` */` |
|      2 |  698 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  699 | `{` |
|      1 |  700 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  701 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      3 |  702 | `}` |
|      - |  703 | `/*` |
|      - |  704 | ` * M_LN2` |
|      - |  705 | ` *  Expand 	0.69314718055994530942` |
|      - |  706 | ` */` |
|      2 |  707 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  708 | `{` |
|      1 |  709 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  710 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      3 |  711 | `}` |
|      - |  712 | `/*` |
|      - |  713 | ` * M_LN10` |
|      - |  714 | ` *  Expand 	2.30258509299404568402` |
|      - |  715 | ` */` |
|      2 |  716 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  717 | `{` |
|      1 |  718 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  719 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      3 |  720 | `}` |
|      - |  721 | `/*` |
|      - |  722 | ` * M_PI_2` |
|      - |  723 | ` *  Expand 	1.57079632679489661923` |
|      - |  724 | ` */` |
|      2 |  725 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  726 | `{` |
|      1 |  727 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  728 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      3 |  729 | `}` |
|      - |  730 | `/*` |
|      - |  731 | ` * M_PI_4` |
|      - |  732 | ` *  Expand 	0.78539816339744830962` |
|      - |  733 | ` */` |
|      2 |  734 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  735 | `{` |
|      1 |  736 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  737 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      3 |  738 | `}` |
|      - |  739 | `/*` |
|      - |  740 | ` * M_1_PI` |
|      - |  741 | ` *  Expand 	0.31830988618379067154` |
|      - |  742 | ` */` |
|      2 |  743 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  744 | `{` |
|      1 |  745 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  746 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      3 |  747 | `}` |
|      - |  748 | `/*` |
|      - |  749 | ` * M_2_PI` |
|      - |  750 | ` *  Expand 0.63661977236758134308` |
|      - |  751 | ` */` |
|      4 |  752 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  753 | `{` |
|      2 |  754 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  755 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      5 |  756 | `}` |
|      - |  757 | `/*` |
|      - |  758 | ` * M_SQRTPI` |
|      - |  759 | ` *  Expand 1.77245385090551602729` |
|      - |  760 | ` */` |
|      2 |  761 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  762 | `{` |
|      1 |  763 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  764 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      3 |  765 | `}` |
|      - |  766 | `/*` |
|      - |  767 | ` * M_2_SQRTPI` |
|      - |  768 | ` *  Expand 	1.12837916709551257390` |
|      - |  769 | ` */` |
|      2 |  770 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  771 | `{` |
|      1 |  772 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  773 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      3 |  774 | `}` |
|      - |  775 | `/*` |
|      - |  776 | ` * M_SQRT2` |
|      - |  777 | ` *  Expand 	1.41421356237309504880` |
|      - |  778 | ` */` |
|      2 |  779 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  780 | `{` |
|      1 |  781 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  782 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      3 |  783 | `}` |
|      - |  784 | `/*` |
|      - |  785 | ` * M_SQRT3` |
|      - |  786 | ` *  Expand 	1.73205080756887729352` |
|      - |  787 | ` */` |
|      2 |  788 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  789 | `{` |
|      1 |  790 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  791 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      3 |  792 | `}` |
|      - |  793 | `/*` |
|      - |  794 | ` * M_SQRT1_2` |
|      - |  795 | ` *  Expand 	0.70710678118654752440` |
|      - |  796 | ` */` |
|      2 |  797 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  798 | `{` |
|      1 |  799 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  800 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      3 |  801 | `}` |
|      - |  802 | `/*` |
|      - |  803 | ` * M_LNPI` |
|      - |  804 | ` *  Expand 	1.14472988584940017414` |
|      - |  805 | ` */` |
|      2 |  806 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  807 | `{` |
|      1 |  808 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  809 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      3 |  810 | `}` |
|      - |  811 | `/*` |
|      - |  812 | ` * M_EULER` |
|      - |  813 | ` *  Expand  0.57721566490153286061` |
|      - |  814 | ` */` |
|      2 |  815 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  816 | `{` |
|      1 |  817 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  818 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      3 |  819 | `}` |
|      - |  820 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|      - |  821 | `/*` |
|      - |  822 | ` * DATE_ATOM` |
|      - |  823 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|      - |  824 | ` */` |
|      2 |  825 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  826 | `{` |
|      1 |  827 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  828 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  829 | `}` |
|      - |  830 | `/*` |
|      - |  831 | ` * DATE_COOKIE` |
|      - |  832 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  833 | ` */` |
|      2 |  834 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  835 | `{` |
|      1 |  836 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  837 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  838 | `}` |
|      - |  839 | `/*` |
|      - |  840 | ` * DATE_ISO8601` |
|      - |  841 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|      - |  842 | ` */` |
|      2 |  843 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  844 | `{` |
|      1 |  845 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  846 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      3 |  847 | `}` |
|      - |  848 | `/*` |
|      - |  849 | ` * DATE_RFC822` |
|      - |  850 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|      - |  851 | ` */` |
|      2 |  852 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  853 | `{` |
|      1 |  854 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  855 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  856 | `}` |
|      - |  857 | `/*` |
|      - |  858 | ` * DATE_RFC850` |
|      - |  859 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  860 | ` */` |
|      2 |  861 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  862 | `{` |
|      1 |  863 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  864 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  865 | `}` |
|      - |  866 | `/*` |
|      - |  867 | ` * DATE_RFC1036` |
|      - |  868 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  869 | ` */` |
|      2 |  870 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  871 | `{` |
|      1 |  872 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  873 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  874 | `}` |
|      - |  875 | `/*` |
|      - |  876 | ` * DATE_RFC1123` |
|      - |  877 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  878 | ` */` |
|      2 |  879 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  880 | `{` |
|      1 |  881 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  882 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  883 | `}` |
|      - |  884 | `/*` |
|      - |  885 | ` * DATE_RFC2822` |
|      - |  886 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  887 | ` */` |
|      2 |  888 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  889 | `{` |
|      1 |  890 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  891 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  892 | `}` |
|      - |  893 | `/*` |
|      - |  894 | ` * DATE_RSS` |
|      - |  895 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  896 | ` */` |
|      2 |  897 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  898 | `{` |
|      1 |  899 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  900 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  901 | `}` |
|      - |  902 | `/*` |
|      - |  903 | ` * DATE_W3C` |
|      - |  904 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|      - |  905 | ` */` |
|      2 |  906 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  907 | `{` |
|      1 |  908 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  909 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  910 | `}` |
|      - |  911 | `/*` |
|      - |  912 | ` * The ENT_* values are PHP-exact (php 8.5.7). The low two bits are the quote` |
|      - |  913 | ` * bits (1 = single, 2 = double), so ENT_QUOTES = ENT_COMPAT\|1 and` |
|      - |  914 | ` * ENT_NOQUOTES = 0. Bits 16\|32 select the doctype (0 = HTML401, 16 = XML1,` |
|      - |  915 | ` * 32 = XHTML, 48 = HTML5) — composites, not flags.` |
|      - |  916 | ` */` |
|      - |  917 | `/*` |
|      - |  918 | ` * ENT_COMPAT` |
|      - |  919 | ` *  Expand 2 (double-quote bit only)` |
|      - |  920 | ` */` |
|     12 |  921 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  922 | `{` |
|      6 |  923 | `	SXUNUSED(pUserData); /* cc warning */` |
|     13 |  924 | `	ph7_value_int(pVal,PH7_ENT_QUOTE_DOUBLE);` |
|     13 |  925 | `}` |
|      - |  926 | `/*` |
|      - |  927 | ` * ENT_QUOTES` |
|      - |  928 | ` *  Expand 3 (double\|single quote bits)` |
|      - |  929 | ` */` |
|     60 |  930 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  931 | `{` |
|     30 |  932 | `	SXUNUSED(pUserData); /* cc warning */` |
|     61 |  933 | `	ph7_value_int(pVal,PH7_ENT_QUOTES);` |
|     61 |  934 | `}` |
|      - |  935 | `/*` |
|      - |  936 | ` * ENT_NOQUOTES` |
|      - |  937 | ` *  Expand 0 (no quote bits)` |
|      - |  938 | ` */` |
|     20 |  939 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  940 | `{` |
|     10 |  941 | `	SXUNUSED(pUserData); /* cc warning */` |
|     21 |  942 | `	ph7_value_int(pVal,0);` |
|     21 |  943 | `}` |
|      - |  944 | `/*` |
|      - |  945 | ` * ENT_IGNORE` |
|      - |  946 | ` *  Expand 4` |
|      - |  947 | ` */` |
|      6 |  948 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  949 | `{` |
|      3 |  950 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 |  951 | `	ph7_value_int(pVal,PH7_ENT_IGNORE);` |
|      7 |  952 | `}` |
|      - |  953 | `/*` |
|      - |  954 | ` * ENT_SUBSTITUTE` |
|      - |  955 | ` *  Expand 8` |
|      - |  956 | ` */` |
|      2 |  957 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  958 | `{` |
|      1 |  959 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  960 | `	ph7_value_int(pVal,PH7_ENT_SUBSTITUTE);` |
|      3 |  961 | `}` |
|      - |  962 | `/*` |
|      - |  963 | ` * ENT_DISALLOWED` |
|      - |  964 | ` *  Expand 128` |
|      - |  965 | ` */` |
|      2 |  966 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  967 | `{` |
|      1 |  968 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  969 | `	ph7_value_int(pVal,PH7_ENT_DISALLOWED);` |
|      3 |  970 | `}` |
|      - |  971 | `/*` |
|      - |  972 | ` * ENT_HTML401` |
|      - |  973 | ` *  Expand 0 (the default doctype)` |
|      - |  974 | ` */` |
|      2 |  975 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  976 | `{` |
|      1 |  977 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  978 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML401);` |
|      3 |  979 | `}` |
|      - |  980 | `/*` |
|      - |  981 | ` * ENT_XML1` |
|      - |  982 | ` *  Expand 16` |
|      - |  983 | ` */` |
|      8 |  984 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  985 | `{` |
|      4 |  986 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 |  987 | `	ph7_value_int(pVal,PH7_ENT_DOC_XML1);` |
|      9 |  988 | `}` |
|      - |  989 | `/*` |
|      - |  990 | ` * ENT_XHTML` |
|      - |  991 | ` *  Expand 32` |
|      - |  992 | ` */` |
|      6 |  993 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  994 | `{` |
|      3 |  995 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 |  996 | `	ph7_value_int(pVal,PH7_ENT_DOC_XHTML);` |
|      7 |  997 | `}` |
|      - |  998 | `/*` |
|      - |  999 | ` * ENT_HTML5` |
|      - | 1000 | ` *  Expand 48 (16\|32 — a doctype composite, not a flag bit)` |
|      - | 1001 | ` */` |
|      8 | 1002 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1003 | `{` |
|      4 | 1004 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 | 1005 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML5);` |
|      9 | 1006 | `}` |
|      - | 1007 | `/*` |
|      - | 1008 | ` * ISO-8859-1` |
|      - | 1009 | ` * ISO_8859_1` |
|      - | 1010 | ` *   Expand 1` |
|      - | 1011 | ` */` |
|      2 | 1012 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1013 | `{` |
|      1 | 1014 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1015 | `	ph7_value_int(pVal,1);` |
|      3 | 1016 | `}` |
|      - | 1017 | `/*` |
|      - | 1018 | ` * UTF-8` |
|      - | 1019 | ` * UTF8` |
|      - | 1020 | ` *  Expand 2` |
|      - | 1021 | ` */` |
|      2 | 1022 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1023 | `{` |
|      1 | 1024 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1025 | `	ph7_value_int(pVal,1);` |
|      3 | 1026 | `}` |
|      - | 1027 | `/*` |
|      - | 1028 | ` * HTML_ENTITIES` |
|      - | 1029 | ` *  Expand 1` |
|      - | 1030 | ` */` |
|      4 | 1031 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1032 | `{` |
|      2 | 1033 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1034 | `	ph7_value_int(pVal,1);` |
|      5 | 1035 | `}` |
|      - | 1036 | `/*` |
|      - | 1037 | ` * HTML_SPECIALCHARS` |
|      - | 1038 | ` *  Expand 0 (PHP-exact)` |
|      - | 1039 | ` */` |
|     10 | 1040 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1041 | `{` |
|      5 | 1042 | `	SXUNUSED(pUserData); /* cc warning */` |
|     11 | 1043 | `	ph7_value_int(pVal,0);` |
|     11 | 1044 | `}` |
|      - | 1045 | `/*` |
|      - | 1046 | ` * PHP_URL_SCHEME.` |
|      - | 1047 | ` * Expand 1` |
|      - | 1048 | ` */` |
|      2 | 1049 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1050 | `{` |
|      1 | 1051 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1052 | `	ph7_value_int(pVal,1);` |
|      3 | 1053 | `}` |
|      - | 1054 | `/*` |
|      - | 1055 | ` * PHP_URL_HOST.` |
|      - | 1056 | ` * Expand 2` |
|      - | 1057 | ` */` |
|      2 | 1058 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1059 | `{` |
|      1 | 1060 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1061 | `	ph7_value_int(pVal,2);` |
|      3 | 1062 | `}` |
|      - | 1063 | `/*` |
|      - | 1064 | ` * PHP_URL_PORT.` |
|      - | 1065 | ` * Expand 3` |
|      - | 1066 | ` */` |
|      2 | 1067 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1068 | `{` |
|      1 | 1069 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1070 | `	ph7_value_int(pVal,3);` |
|      3 | 1071 | `}` |
|      - | 1072 | `/*` |
|      - | 1073 | ` * PHP_URL_USER.` |
|      - | 1074 | ` * Expand 4` |
|      - | 1075 | ` */` |
|      2 | 1076 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1077 | `{` |
|      1 | 1078 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1079 | `	ph7_value_int(pVal,4);` |
|      3 | 1080 | `}` |
|      - | 1081 | `/*` |
|      - | 1082 | ` * PHP_URL_PASS.` |
|      - | 1083 | ` * Expand 5` |
|      - | 1084 | ` */` |
|      2 | 1085 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1086 | `{` |
|      1 | 1087 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1088 | `	ph7_value_int(pVal,5);` |
|      3 | 1089 | `}` |
|      - | 1090 | `/*` |
|      - | 1091 | ` * PHP_URL_PATH.` |
|      - | 1092 | ` * Expand 6` |
|      - | 1093 | ` */` |
|      2 | 1094 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1095 | `{` |
|      1 | 1096 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1097 | `	ph7_value_int(pVal,6);` |
|      3 | 1098 | `}` |
|      - | 1099 | `/*` |
|      - | 1100 | ` * PHP_URL_QUERY.` |
|      - | 1101 | ` * Expand 7` |
|      - | 1102 | ` */` |
|      2 | 1103 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1104 | `{` |
|      1 | 1105 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1106 | `	ph7_value_int(pVal,7);` |
|      3 | 1107 | `}` |
|      - | 1108 | `/*` |
|      - | 1109 | ` * PHP_URL_FRAGMENT.` |
|      - | 1110 | ` * Expand 8` |
|      - | 1111 | ` */` |
|      2 | 1112 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1113 | `{` |
|      1 | 1114 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1115 | `	ph7_value_int(pVal,8);` |
|      3 | 1116 | `}` |
|      - | 1117 | `/*` |
|      - | 1118 | ` * PHP_QUERY_RFC1738` |
|      - | 1119 | ` * Expand 1` |
|      - | 1120 | ` */` |
|      2 | 1121 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1122 | `{` |
|      1 | 1123 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1124 | `	ph7_value_int(pVal,1);` |
|      3 | 1125 | `}` |
|      - | 1126 | `/*` |
|      - | 1127 | ` * PHP_QUERY_RFC3986` |
|      - | 1128 | ` * Expand 1` |
|      - | 1129 | ` */` |
|      2 | 1130 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1131 | `{` |
|      1 | 1132 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1133 | `	ph7_value_int(pVal,2);` |
|      3 | 1134 | `}` |
|      - | 1135 | `/*` |
|      - | 1136 | ` * FNM_NOESCAPE` |
|      - | 1137 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1138 | ` */` |
|      2 | 1139 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1140 | `{` |
|      1 | 1141 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1142 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1143 | `}` |
|      - | 1144 | `/*` |
|      - | 1145 | ` * FNM_PATHNAME` |
|      - | 1146 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1147 | ` */` |
|      2 | 1148 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1149 | `{` |
|      1 | 1150 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1151 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1152 | `}` |
|      - | 1153 | `/*` |
|      - | 1154 | ` * FNM_PERIOD` |
|      - | 1155 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1156 | ` */` |
|      6 | 1157 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1158 | `{` |
|      3 | 1159 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1160 | `	ph7_value_int(pVal,0x04);` |
|      7 | 1161 | `}` |
|      - | 1162 | `/*` |
|      - | 1163 | ` * FNM_CASEFOLD` |
|      - | 1164 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1165 | ` */` |
|      4 | 1166 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1167 | `{` |
|      2 | 1168 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1169 | `	ph7_value_int(pVal,0x08);` |
|      5 | 1170 | `}` |
|      - | 1171 | `/*` |
|      - | 1172 | ` * PATHINFO_DIRNAME` |
|      - | 1173 | ` *  Expand 1.` |
|      - | 1174 | ` */` |
|      4 | 1175 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1176 | `{` |
|      2 | 1177 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1178 | `	ph7_value_int(pVal,1);` |
|      5 | 1179 | `}` |
|      - | 1180 | `/*` |
|      - | 1181 | ` * PATHINFO_BASENAME` |
|      - | 1182 | ` *  Expand 2.` |
|      - | 1183 | ` */` |
|      4 | 1184 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1185 | `{` |
|      2 | 1186 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1187 | `	ph7_value_int(pVal,2);` |
|      5 | 1188 | `}` |
|      - | 1189 | `/*` |
|      - | 1190 | ` * PATHINFO_EXTENSION` |
|      - | 1191 | ` *  Expand 3.` |
|      - | 1192 | ` */` |
|   6516 | 1193 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1194 | `{` |
|   3258 | 1195 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6521 | 1196 | `	ph7_value_int(pVal,3);` |
|   6521 | 1197 | `}` |
|      - | 1198 | `/*` |
|      - | 1199 | ` * PATHINFO_FILENAME` |
|      - | 1200 | ` *  Expand 4.` |
|      - | 1201 | ` */` |
|   6508 | 1202 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1203 | `{` |
|   3254 | 1204 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6513 | 1205 | `	ph7_value_int(pVal,4);` |
|   6513 | 1206 | `}` |
|      - | 1207 | `/*` |
|      - | 1208 | ` * ASSERT_ACTIVE.` |
|      - | 1209 | ` *  PHP ASSERT_ACTIVE = 1` |
|      - | 1210 | ` */` |
|     14 | 1211 | `static void PH7_ASSERT_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1212 | `{` |
|      7 | 1213 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1214 | `	ph7_value_int(pVal,1); /* PHP ASSERT_ACTIVE = 1 */` |
|     16 | 1215 | `}` |
|      - | 1216 | `/*` |
|      - | 1217 | ` * ASSERT_CALLBACK.` |
|      - | 1218 | ` *  PHP ASSERT_CALLBACK = 2` |
|      - | 1219 | ` */` |
|      6 | 1220 | `static void PH7_ASSERT_CALLBACK_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1221 | `{` |
|      3 | 1222 | `	SXUNUSED(pUserData); /* cc warning */` |
|      8 | 1223 | `	ph7_value_int(pVal,2); /* PHP ASSERT_CALLBACK = 2 */` |
|      8 | 1224 | `}` |
|      - | 1225 | `/*` |
|      - | 1226 | ` * ASSERT_BAIL.` |
|      - | 1227 | ` *  PHP ASSERT_BAIL = 3` |
|      - | 1228 | ` */` |
|     14 | 1229 | `static void PH7_ASSERT_BAIL_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1230 | `{` |
|      7 | 1231 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1232 | `	ph7_value_int(pVal,3); /* PHP ASSERT_BAIL = 3 */` |
|     16 | 1233 | `}` |
|      - | 1234 | `/*` |
|      - | 1235 | ` * ASSERT_WARNING.` |
|      - | 1236 | ` *  PHP ASSERT_WARNING = 4 (deprecated in PHP 8.3)` |
|      - | 1237 | ` */` |
|      4 | 1238 | `static void PH7_ASSERT_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1239 | `{` |
|      2 | 1240 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1241 | `	ph7_value_int(pVal,4); /* PHP ASSERT_WARNING = 4 */` |
|      5 | 1242 | `}` |
|      - | 1243 | `/*` |
|      - | 1244 | ` * ASSERT_EXCEPTION.` |
|      - | 1245 | ` *  PHP ASSERT_EXCEPTION = 5 (deprecated in PHP 8.3)` |
|      - | 1246 | ` */` |
|      4 | 1247 | `static void PH7_ASSERT_EXCEPTION_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1248 | `{` |
|      2 | 1249 | `	SXUNUSED(pUserData); /* cc warning */` |
|      6 | 1250 | `	ph7_value_int(pVal,5); /* PHP ASSERT_EXCEPTION = 5 */` |
|      6 | 1251 | `}` |
|      - | 1252 | `/*` |
|      - | 1253 | ` * ASSERT_QUIET_EVAL.` |
|      - | 1254 | ` *  Removed in PHP 8.0, kept for compatibility.` |
|      - | 1255 | ` */` |
|      2 | 1256 | `static void PH7_ASSERT_QUIET_EVAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1257 | `{` |
|      1 | 1258 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1259 | `	ph7_value_int(pVal,6); /* Arbitrary value, removed in PHP 8 */` |
|      3 | 1260 | `}` |
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
|      - | 1299 | ` *  Expand 5` |
|      - | 1300 | ` */` |
|      2 | 1301 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1302 | `{` |
|      1 | 1303 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1304 | `	ph7_value_int(pVal,5);` |
|      3 | 1305 | `}` |
|      - | 1306 | `/*` |
|      - | 1307 | ` * LOCK_EX.` |
|      - | 1308 | ` *  Expand 0x01 (MUST BE A POWER OF TWO)` |
|      - | 1309 | ` */` |
|      4 | 1310 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1311 | `{` |
|      2 | 1312 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1313 | `	ph7_value_int(pVal,0x01);` |
|      5 | 1314 | `}` |
|      - | 1315 | `/*` |
|      - | 1316 | ` * LOCK_UN.` |
|      - | 1317 | ` *  Expand 0` |
|      - | 1318 | ` */` |
|      4 | 1319 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1320 | `{` |
|      2 | 1321 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1322 | `	ph7_value_int(pVal,0);` |
|      5 | 1323 | `}` |
|      - | 1324 | `/*` |
|      - | 1325 | ` * FILE_USE_INCLUDE_PATH` |
|      - | 1326 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1327 | ` */` |
|      2 | 1328 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1329 | `{` |
|      1 | 1330 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1331 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1332 | `}` |
|      - | 1333 | `/*` |
|      - | 1334 | ` * FILE_IGNORE_NEW_LINES` |
|      - | 1335 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1336 | ` */` |
|      2 | 1337 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1338 | `{` |
|      1 | 1339 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1340 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1341 | `}` |
|      - | 1342 | `/*` |
|      - | 1343 | ` * FILE_SKIP_EMPTY_LINES` |
|      - | 1344 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1345 | ` */` |
|      2 | 1346 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1347 | `{` |
|      1 | 1348 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1349 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1350 | `}` |
|      - | 1351 | `/*` |
|      - | 1352 | ` * FILE_APPEND` |
|      - | 1353 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1354 | ` */` |
|      2 | 1355 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1356 | `{` |
|      1 | 1357 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1358 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1359 | `}` |
|      - | 1360 | `/*` |
|      - | 1361 | ` * SCANDIR_SORT_ASCENDING` |
|      - | 1362 | ` *  Expand 0` |
|      - | 1363 | ` */` |
|   1986 | 1364 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1365 | `{` |
|    993 | 1366 | `	SXUNUSED(pUserData); /* cc warning */` |
|   1991 | 1367 | `	ph7_value_int(pVal,0);` |
|   1991 | 1368 | `}` |
|      - | 1369 | `/*` |
|      - | 1370 | ` * SCANDIR_SORT_DESCENDING` |
|      - | 1371 | ` *  Expand 1` |
|      - | 1372 | ` */` |
|    994 | 1373 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1374 | `{` |
|    497 | 1375 | `	SXUNUSED(pUserData); /* cc warning */` |
|    999 | 1376 | `	ph7_value_int(pVal,1);` |
|    999 | 1377 | `}` |
|      - | 1378 | `/*` |
|      - | 1379 | ` * SCANDIR_SORT_NONE` |
|      - | 1380 | ` *  Expand 2` |
|      - | 1381 | ` */` |
|      2 | 1382 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1383 | `{` |
|      1 | 1384 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1385 | `	ph7_value_int(pVal,2);` |
|      3 | 1386 | `}` |
|      - | 1387 | `/*` |
|      - | 1388 | ` * GLOB_MARK` |
|      - | 1389 | ` *  Expand 0x01 (must be a power of two)` |
|      - | 1390 | ` */` |
|      2 | 1391 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1392 | `{` |
|      1 | 1393 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1394 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1395 | `}` |
|      - | 1396 | `/*` |
|      - | 1397 | ` * GLOB_NOSORT` |
|      - | 1398 | ` *  Expand 0x02 (must be a power of two)` |
|      - | 1399 | ` */` |
|      2 | 1400 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1401 | `{` |
|      1 | 1402 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1403 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1404 | `}` |
|      - | 1405 | `/*` |
|      - | 1406 | ` * GLOB_NOCHECK` |
|      - | 1407 | ` *  Expand 0x04 (must be a power of two)` |
|      - | 1408 | ` */` |
|      2 | 1409 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1410 | `{` |
|      1 | 1411 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1412 | `	ph7_value_int(pVal,0x04);` |
|      3 | 1413 | `}` |
|      - | 1414 | `/*` |
|      - | 1415 | ` * GLOB_NOESCAPE` |
|      - | 1416 | ` *  Expand 0x08 (must be a power of two)` |
|      - | 1417 | ` */` |
|      2 | 1418 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1419 | `{` |
|      1 | 1420 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1421 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1422 | `}` |
|      - | 1423 | `/*` |
|      - | 1424 | ` * GLOB_BRACE` |
|      - | 1425 | ` *  Expand 0x10 (must be a power of two)` |
|      - | 1426 | ` */` |
|      2 | 1427 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1428 | `{` |
|      1 | 1429 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1430 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1431 | `}` |
|      - | 1432 | `/*` |
|      - | 1433 | ` * GLOB_ONLYDIR` |
|      - | 1434 | ` *  Expand 0x20 (must be a power of two)` |
|      - | 1435 | ` */` |
|      2 | 1436 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1437 | `{` |
|      1 | 1438 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1439 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1440 | `}` |
|      - | 1441 | `/*` |
|      - | 1442 | ` * GLOB_ERR` |
|      - | 1443 | ` *  Expand 0x40 (must be a power of two)` |
|      - | 1444 | ` */` |
|      2 | 1445 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1446 | `{` |
|      1 | 1447 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1448 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1449 | `}` |
|      - | 1450 | `/*` |
|      - | 1451 | ` * STDIN` |
|      - | 1452 | ` *  Expand the STDIN handle as a resource.` |
|      - | 1453 | ` */` |
|      2 | 1454 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1455 | `{` |
|      3 | 1456 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1457 | `	void *pResource;` |
|      3 | 1458 | `	pResource = PH7_ExportStdin(pVm);` |
|      3 | 1459 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1460 | `}` |
|      - | 1461 | `/*` |
|      - | 1462 | ` * STDOUT` |
|      - | 1463 | ` *   Expand the STDOUT handle as a resource.` |
|      - | 1464 | ` */` |
|      2 | 1465 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1466 | `{` |
|      3 | 1467 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1468 | `	void *pResource;` |
|      3 | 1469 | `	pResource = PH7_ExportStdout(pVm);` |
|      3 | 1470 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1471 | `}` |
|      - | 1472 | `/*` |
|      - | 1473 | ` * STDERR` |
|      - | 1474 | ` *  Expand the STDERR handle as a resource.` |
|      - | 1475 | ` */` |
|      2 | 1476 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1477 | `{` |
|      3 | 1478 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1479 | `	void *pResource;` |
|      3 | 1480 | `	pResource = PH7_ExportStderr(pVm);` |
|      3 | 1481 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1482 | `}` |
|      - | 1483 | `/*` |
|      - | 1484 | ` * INI_SCANNER_NORMAL` |
|      - | 1485 | ` *   Expand 1` |
|      - | 1486 | ` */` |
|      2 | 1487 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1488 | `{` |
|      1 | 1489 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1490 | `	ph7_value_int(pVal,1);` |
|      3 | 1491 | `}` |
|      - | 1492 | `/*` |
|      - | 1493 | ` * INI_SCANNER_RAW` |
|      - | 1494 | ` *   Expand 2` |
|      - | 1495 | ` */` |
|      2 | 1496 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1497 | `{` |
|      1 | 1498 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1499 | `	ph7_value_int(pVal,2);` |
|      3 | 1500 | `}` |
|      - | 1501 | `/*` |
|      - | 1502 | ` * EXTR_OVERWRITE` |
|      - | 1503 | ` *   Expand 0x01 (Must be a power of two)` |
|      - | 1504 | ` */` |
|      2 | 1505 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1506 | `{` |
|      1 | 1507 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1508 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1509 | `}` |
|      - | 1510 | `/*` |
|      - | 1511 | ` * EXTR_SKIP` |
|      - | 1512 | ` *   Expand 0x02 (Must be a power of two)` |
|      - | 1513 | ` */` |
|      2 | 1514 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1515 | `{` |
|      1 | 1516 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1517 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1518 | `}` |
|      - | 1519 | `/*` |
|      - | 1520 | ` * EXTR_PREFIX_SAME` |
|      - | 1521 | ` *   Expand 0x04 (Must be a power of two)` |
|      - | 1522 | ` */` |
|      2 | 1523 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1524 | `{` |
|      1 | 1525 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1526 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1527 | `}` |
|      - | 1528 | `/*` |
|      - | 1529 | ` * EXTR_PREFIX_ALL` |
|      - | 1530 | ` *   Expand 0x08 (Must be a power of two)` |
|      - | 1531 | ` */` |
|      2 | 1532 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1533 | `{` |
|      1 | 1534 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1535 | `	ph7_value_int(pVal,0x8);` |
|      3 | 1536 | `}` |
|      - | 1537 | `/*` |
|      - | 1538 | ` * EXTR_PREFIX_INVALID` |
|      - | 1539 | ` *   Expand 0x10 (Must be a power of two)` |
|      - | 1540 | ` */` |
|      2 | 1541 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1542 | `{` |
|      1 | 1543 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1544 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1545 | `}` |
|      - | 1546 | `/*` |
|      - | 1547 | ` * EXTR_IF_EXISTS` |
|      - | 1548 | ` *   Expand 0x20 (Must be a power of two)` |
|      - | 1549 | ` */` |
|      2 | 1550 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1551 | `{` |
|      1 | 1552 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1553 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1554 | `}` |
|      - | 1555 | `/*` |
|      - | 1556 | ` * EXTR_PREFIX_IF_EXISTS` |
|      - | 1557 | ` *   Expand 0x40 (Must be a power of two)` |
|      - | 1558 | ` */` |
|      2 | 1559 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1560 | `{` |
|      1 | 1561 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1562 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1563 | `}` |
|      - | 1564 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1565 | `/*` |
|      - | 1566 | ` * XML_ERROR_NONE` |
|      - | 1567 | ` *   Expand the value of SXML_ERROR_NO_MEMORY defined in ph7Int.h` |
|      - | 1568 | ` */` |
|      2 | 1569 | `static void PH7_XML_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1570 | `{` |
|      1 | 1571 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1572 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1573 | `}` |
|      - | 1574 | `/*` |
|      - | 1575 | ` * XML_ERROR_NO_MEMORY` |
|      - | 1576 | ` *   Expand the value of SXML_ERROR_NONE defined in ph7Int.h` |
|      - | 1577 | ` */` |
|      2 | 1578 | `static void PH7_XML_ERROR_NO_MEMORY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1579 | `{` |
|      1 | 1580 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1581 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1582 | `}` |
|      - | 1583 | `/*` |
|      - | 1584 | ` * XML_ERROR_SYNTAX` |
|      - | 1585 | ` *   Expand the value of SXML_ERROR_SYNTAX defined in ph7Int.h` |
|      - | 1586 | ` */` |
|      2 | 1587 | `static void PH7_XML_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1588 | `{` |
|      1 | 1589 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1590 | `	ph7_value_int(pVal,SXML_ERROR_SYNTAX);` |
|      3 | 1591 | `}` |
|      - | 1592 | `/*` |
|      - | 1593 | ` * XML_ERROR_NO_ELEMENTS` |
|      - | 1594 | ` *   Expand the value of SXML_ERROR_NO_ELEMENTS defined in ph7Int.h` |
|      - | 1595 | ` */` |
|      2 | 1596 | `static void PH7_XML_ERROR_NO_ELEMENTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1597 | `{` |
|      1 | 1598 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1599 | `	ph7_value_int(pVal,SXML_ERROR_NO_ELEMENTS);` |
|      3 | 1600 | `}` |
|      - | 1601 | `/*` |
|      - | 1602 | ` * XML_ERROR_INVALID_TOKEN` |
|      - | 1603 | ` *   Expand the value of SXML_ERROR_INVALID_TOKEN defined in ph7Int.h` |
|      - | 1604 | ` */` |
|      2 | 1605 | `static void PH7_XML_ERROR_INVALID_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1606 | `{` |
|      1 | 1607 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1608 | `	ph7_value_int(pVal,SXML_ERROR_INVALID_TOKEN);` |
|      3 | 1609 | `}` |
|      - | 1610 | `/*` |
|      - | 1611 | ` * XML_ERROR_UNCLOSED_TOKEN` |
|      - | 1612 | ` *   Expand the value of SXML_ERROR_UNCLOSED_TOKEN defined in ph7Int.h` |
|      - | 1613 | ` */` |
|      2 | 1614 | `static void PH7_XML_ERROR_UNCLOSED_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1615 | `{` |
|      1 | 1616 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1617 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_TOKEN);` |
|      3 | 1618 | `}` |
|      - | 1619 | `/*` |
|      - | 1620 | ` * XML_ERROR_PARTIAL_CHAR` |
|      - | 1621 | ` *   Expand the value of SXML_ERROR_PARTIAL_CHAR defined in ph7Int.h` |
|      - | 1622 | ` */` |
|      2 | 1623 | `static void PH7_XML_ERROR_PARTIAL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1624 | `{` |
|      1 | 1625 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1626 | `	ph7_value_int(pVal,SXML_ERROR_PARTIAL_CHAR);` |
|      3 | 1627 | `}` |
|      - | 1628 | `/*` |
|      - | 1629 | ` * XML_ERROR_TAG_MISMATCH` |
|      - | 1630 | ` *   Expand the value of SXML_ERROR_TAG_MISMATCH defined in ph7Int.h` |
|      - | 1631 | ` */` |
|      2 | 1632 | `static void PH7_XML_ERROR_TAG_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1633 | `{` |
|      1 | 1634 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1635 | `	ph7_value_int(pVal,SXML_ERROR_TAG_MISMATCH);` |
|      3 | 1636 | `}` |
|      - | 1637 | `/*` |
|      - | 1638 | ` * XML_ERROR_DUPLICATE_ATTRIBUTE` |
|      - | 1639 | ` *   Expand the value of SXML_ERROR_DUPLICATE_ATTRIBUTE defined in ph7Int.h` |
|      - | 1640 | ` */` |
|      2 | 1641 | `static void PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1642 | `{` |
|      1 | 1643 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1644 | `	ph7_value_int(pVal,SXML_ERROR_DUPLICATE_ATTRIBUTE);` |
|      3 | 1645 | `}` |
|      - | 1646 | `/*` |
|      - | 1647 | ` * XML_ERROR_JUNK_AFTER_DOC_ELEMENT` |
|      - | 1648 | ` *   Expand the value of SXML_ERROR_JUNK_AFTER_DOC_ELEMENT defined in ph7Int.h` |
|      - | 1649 | ` */` |
|      2 | 1650 | `static void PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1651 | `{` |
|      1 | 1652 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1653 | `	ph7_value_int(pVal,SXML_ERROR_JUNK_AFTER_DOC_ELEMENT);` |
|      3 | 1654 | `}` |
|      - | 1655 | `/*` |
|      - | 1656 | ` * XML_ERROR_PARAM_ENTITY_REF` |
|      - | 1657 | ` *   Expand the value of SXML_ERROR_PARAM_ENTITY_REF defined in ph7Int.h` |
|      - | 1658 | ` */` |
|      2 | 1659 | `static void PH7_XML_ERROR_PARAM_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1660 | `{` |
|      1 | 1661 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1662 | `	ph7_value_int(pVal,SXML_ERROR_PARAM_ENTITY_REF);` |
|      3 | 1663 | `}` |
|      - | 1664 | `/*` |
|      - | 1665 | ` * XML_ERROR_UNDEFINED_ENTITY` |
|      - | 1666 | ` *   Expand the value of SXML_ERROR_UNDEFINED_ENTITY defined in ph7Int.h` |
|      - | 1667 | ` */` |
|      2 | 1668 | `static void PH7_XML_ERROR_UNDEFINED_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1669 | `{` |
|      1 | 1670 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1671 | `	ph7_value_int(pVal,SXML_ERROR_UNDEFINED_ENTITY);` |
|      3 | 1672 | `}` |
|      - | 1673 | `/*` |
|      - | 1674 | ` * XML_ERROR_RECURSIVE_ENTITY_REF` |
|      - | 1675 | ` *   Expand the value of SXML_ERROR_RECURSIVE_ENTITY_REF defined in ph7Int.h` |
|      - | 1676 | ` */` |
|      2 | 1677 | `static void PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1678 | `{` |
|      1 | 1679 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1680 | `	ph7_value_int(pVal,SXML_ERROR_RECURSIVE_ENTITY_REF);` |
|      3 | 1681 | `}` |
|      - | 1682 | `/*` |
|      - | 1683 | ` * XML_ERROR_ASYNC_ENTITY` |
|      - | 1684 | ` *   Expand the value of SXML_ERROR_ASYNC_ENTITY defined in ph7Int.h` |
|      - | 1685 | ` */` |
|      2 | 1686 | `static void PH7_XML_ERROR_ASYNC_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1687 | `{` |
|      1 | 1688 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1689 | `	ph7_value_int(pVal,SXML_ERROR_ASYNC_ENTITY);` |
|      3 | 1690 | `}` |
|      - | 1691 | `/*` |
|      - | 1692 | ` * XML_ERROR_BAD_CHAR_REF` |
|      - | 1693 | ` *   Expand the value of SXML_ERROR_BAD_CHAR_REF defined in ph7Int.h` |
|      - | 1694 | ` */` |
|      2 | 1695 | `static void PH7_XML_ERROR_BAD_CHAR_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1696 | `{` |
|      1 | 1697 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1698 | `	ph7_value_int(pVal,SXML_ERROR_BAD_CHAR_REF);` |
|      3 | 1699 | `}` |
|      - | 1700 | `/*` |
|      - | 1701 | ` * XML_ERROR_BINARY_ENTITY_REF` |
|      - | 1702 | ` *   Expand the value of SXML_ERROR_BINARY_ENTITY_REF defined in ph7Int.h` |
|      - | 1703 | ` */` |
|      2 | 1704 | `static void PH7_XML_ERROR_BINARY_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1705 | `{` |
|      1 | 1706 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1707 | `	ph7_value_int(pVal,SXML_ERROR_BINARY_ENTITY_REF);` |
|      3 | 1708 | `}` |
|      - | 1709 | `/*` |
|      - | 1710 | ` * XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF` |
|      - | 1711 | ` *   Expand the value of SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF defined in ph7Int.h` |
|      - | 1712 | ` */` |
|      2 | 1713 | `static void PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1714 | `{` |
|      1 | 1715 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1716 | `	ph7_value_int(pVal,SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF);` |
|      3 | 1717 | `}` |
|      - | 1718 | `/*` |
|      - | 1719 | ` * XML_ERROR_MISPLACED_XML_PI` |
|      - | 1720 | ` *   Expand the value of SXML_ERROR_MISPLACED_XML_PI defined in ph7Int.h` |
|      - | 1721 | ` */` |
|      2 | 1722 | `static void PH7_XML_ERROR_MISPLACED_XML_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1723 | `{` |
|      1 | 1724 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1725 | `	ph7_value_int(pVal,SXML_ERROR_MISPLACED_XML_PI);` |
|      3 | 1726 | `}` |
|      - | 1727 | `/*` |
|      - | 1728 | ` * XML_ERROR_UNKNOWN_ENCODING` |
|      - | 1729 | ` *   Expand the value of SXML_ERROR_UNKNOWN_ENCODING defined in ph7Int.h` |
|      - | 1730 | ` */` |
|      2 | 1731 | `static void PH7_XML_ERROR_UNKNOWN_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1732 | `{` |
|      1 | 1733 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1734 | `	ph7_value_int(pVal,SXML_ERROR_UNKNOWN_ENCODING);` |
|      3 | 1735 | `}` |
|      - | 1736 | `/*` |
|      - | 1737 | ` * XML_ERROR_INCORRECT_ENCODING` |
|      - | 1738 | ` *   Expand the value of SXML_ERROR_INCORRECT_ENCODING defined in ph7Int.h` |
|      - | 1739 | ` */` |
|      2 | 1740 | `static void PH7_XML_ERROR_INCORRECT_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1741 | `{` |
|      1 | 1742 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1743 | `	ph7_value_int(pVal,SXML_ERROR_INCORRECT_ENCODING);` |
|      3 | 1744 | `}` |
|      - | 1745 | `/*` |
|      - | 1746 | ` * XML_ERROR_UNCLOSED_CDATA_SECTION` |
|      - | 1747 | ` *   Expand the value of SXML_ERROR_UNCLOSED_CDATA_SECTION defined in ph7Int.h` |
|      - | 1748 | ` */` |
|      2 | 1749 | `static void PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1750 | `{` |
|      1 | 1751 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1752 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_CDATA_SECTION);` |
|      3 | 1753 | `}` |
|      - | 1754 | `/*` |
|      - | 1755 | ` * XML_ERROR_EXTERNAL_ENTITY_HANDLING` |
|      - | 1756 | ` *   Expand the value of SXML_ERROR_EXTERNAL_ENTITY_HANDLING defined in ph7Int.h` |
|      - | 1757 | ` */` |
|      2 | 1758 | `static void PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1759 | `{` |
|      1 | 1760 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1761 | `	ph7_value_int(pVal,SXML_ERROR_EXTERNAL_ENTITY_HANDLING);` |
|      3 | 1762 | `}` |
|      - | 1763 | `/*` |
|      - | 1764 | ` * XML_OPTION_CASE_FOLDING` |
|      - | 1765 | ` *   Expand the value of SXML_OPTION_CASE_FOLDING defined in ph7Int.h.` |
|      - | 1766 | ` */` |
|      2 | 1767 | `static void PH7_XML_OPTION_CASE_FOLDING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1768 | `{` |
|      1 | 1769 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1770 | `	ph7_value_int(pVal,SXML_OPTION_CASE_FOLDING);` |
|      3 | 1771 | `}` |
|      - | 1772 | `/*` |
|      - | 1773 | ` * XML_OPTION_TARGET_ENCODING` |
|      - | 1774 | ` *   Expand the value of SXML_OPTION_TARGET_ENCODING defined in ph7Int.h.` |
|      - | 1775 | ` */` |
|      4 | 1776 | `static void PH7_XML_OPTION_TARGET_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1777 | `{` |
|      2 | 1778 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1779 | `	ph7_value_int(pVal,SXML_OPTION_TARGET_ENCODING);` |
|      5 | 1780 | `}` |
|      - | 1781 | `/*` |
|      - | 1782 | ` * XML_OPTION_SKIP_TAGSTART` |
|      - | 1783 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1784 | ` */` |
|      2 | 1785 | `static void PH7_XML_OPTION_SKIP_TAGSTART_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1786 | `{` |
|      1 | 1787 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1788 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_TAGSTART);` |
|      3 | 1789 | `}` |
|      - | 1790 | `/*` |
|      - | 1791 | ` * XML_OPTION_SKIP_WHITE` |
|      - | 1792 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1793 | ` */` |
|      4 | 1794 | `static void PH7_XML_OPTION_SKIP_WHITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1795 | `{` |
|      2 | 1796 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1797 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_WHITE);` |
|      5 | 1798 | `}` |
|      - | 1799 | `/*` |
|      - | 1800 | ` * XML_SAX_IMPL.` |
|      - | 1801 | ` *   Expand the name of the underlying XML engine.` |
|      - | 1802 | ` */` |
|      2 | 1803 | `static void PH7_XML_SAX_IMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1804 | `{` |
|      1 | 1805 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1806 | `	ph7_value_string(pVal,"Symisc XML engine",(int)sizeof("Symisc XML engine")-1);` |
|      3 | 1807 | `}` |
|      - | 1808 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1809 | `/*` |
|      - | 1810 | ` * JSON_HEX_TAG.` |
|      - | 1811 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|      - | 1812 | ` */` |
|      2 | 1813 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1814 | `{` |
|      1 | 1815 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1816 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      3 | 1817 | `}` |
|      - | 1818 | `/*` |
|      - | 1819 | ` * JSON_HEX_AMP.` |
|      - | 1820 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|      - | 1821 | ` */` |
|      2 | 1822 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1823 | `{` |
|      1 | 1824 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1825 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      3 | 1826 | `}` |
|      - | 1827 | `/*` |
|      - | 1828 | ` * JSON_HEX_APOS.` |
|      - | 1829 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|      - | 1830 | ` */` |
|      2 | 1831 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1832 | `{` |
|      1 | 1833 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1834 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      3 | 1835 | `}` |
|      - | 1836 | `/*` |
|      - | 1837 | ` * JSON_HEX_QUOT.` |
|      - | 1838 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|      - | 1839 | ` */` |
|      2 | 1840 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1841 | `{` |
|      1 | 1842 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1843 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      3 | 1844 | `}` |
|      - | 1845 | `/*` |
|      - | 1846 | ` * JSON_FORCE_OBJECT.` |
|      - | 1847 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|      - | 1848 | ` */` |
|      4 | 1849 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1850 | `{` |
|      2 | 1851 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1852 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      5 | 1853 | `}` |
|      - | 1854 | `/*` |
|      - | 1855 | ` * JSON_NUMERIC_CHECK.` |
|      - | 1856 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|      - | 1857 | ` */` |
|      4 | 1858 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1859 | `{` |
|      2 | 1860 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1861 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      5 | 1862 | `}` |
|      - | 1863 | `/*` |
|      - | 1864 | ` * JSON_BIGINT_AS_STRING.` |
|      - | 1865 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|      - | 1866 | ` */` |
|      2 | 1867 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1868 | `{` |
|      1 | 1869 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1870 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      3 | 1871 | `}` |
|      - | 1872 | `/*` |
|      - | 1873 | ` * JSON_PRETTY_PRINT.` |
|      - | 1874 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|      - | 1875 | ` */` |
|      2 | 1876 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1877 | `{` |
|      1 | 1878 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1879 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      3 | 1880 | `}` |
|      - | 1881 | `/*` |
|      - | 1882 | ` * JSON_UNESCAPED_SLASHES.` |
|      - | 1883 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|      - | 1884 | ` */` |
|      4 | 1885 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1886 | `{` |
|      2 | 1887 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1888 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      5 | 1889 | `}` |
|      - | 1890 | `/*` |
|      - | 1891 | ` * JSON_UNESCAPED_UNICODE.` |
|      - | 1892 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|      - | 1893 | ` */` |
|      2 | 1894 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1895 | `{` |
|      1 | 1896 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1897 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|      3 | 1898 | `}` |
|      - | 1899 | `/*` |
|      - | 1900 | ` * JSON_ERROR_NONE.` |
|      - | 1901 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|      - | 1902 | ` */` |
|      4 | 1903 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1904 | `{` |
|      2 | 1905 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1906 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      5 | 1907 | `}` |
|      - | 1908 | `/*` |
|      - | 1909 | ` * JSON_ERROR_DEPTH.` |
|      - | 1910 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|      - | 1911 | ` */` |
|      2 | 1912 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1913 | `{` |
|      1 | 1914 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1915 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      3 | 1916 | `}` |
|      - | 1917 | `/*` |
|      - | 1918 | ` * JSON_ERROR_STATE_MISMATCH.` |
|      - | 1919 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|      - | 1920 | ` */` |
|      2 | 1921 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1922 | `{` |
|      1 | 1923 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1924 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      3 | 1925 | `}` |
|      - | 1926 | `/*` |
|      - | 1927 | ` * JSON_ERROR_CTRL_CHAR.` |
|      - | 1928 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|      - | 1929 | ` */` |
|      2 | 1930 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1931 | `{` |
|      1 | 1932 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1933 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      3 | 1934 | `}` |
|      - | 1935 | `/*` |
|      - | 1936 | ` * JSON_ERROR_SYNTAX.` |
|      - | 1937 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|      - | 1938 | ` */` |
|      4 | 1939 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1940 | `{` |
|      2 | 1941 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1942 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      5 | 1943 | `}` |
|      - | 1944 | `/*` |
|      - | 1945 | ` * JSON_ERROR_UTF8.` |
|      - | 1946 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|      - | 1947 | ` */` |
|      2 | 1948 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1949 | `{` |
|      1 | 1950 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1951 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      3 | 1952 | `}` |
|      - | 1953 | `/*` |
|      - | 1954 | ` * JSON_ERROR_NON_BACKED_ENUM.` |
|      - | 1955 | ` *   Expand the value of JSON_ERROR_NON_BACKED_ENUM defined in ph7Int.h (php 8.1).` |
|      - | 1956 | ` */` |
|    ! 0 | 1957 | `static void PH7_JSON_ERROR_NON_BACKED_ENUM_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 1958 | `{` |
|    ! 0 | 1959 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 1960 | `	ph7_value_int(pVal,JSON_ERROR_NON_BACKED_ENUM);` |
|    ! 0 | 1961 | `}` |
|      - | 1962 | `/*` |
|      - | 1963 | ` * static` |
|      - | 1964 | ` *  Expand the name of the current class. 'static' otherwise.` |
|      - | 1965 | ` */` |
|      6 | 1966 | `static void PH7_static_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1967 | `{` |
|      7 | 1968 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1969 | `	ph7_class *pClass;` |
|      - | 1970 | `	/* Extract the target class if available */` |
|      7 | 1971 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      7 | 1972 | `	if( pClass ){` |
|      3 | 1973 | `		SyString *pName = &pClass->sName;` |
|      - | 1974 | `		/* Expand class name */` |
|      3 | 1975 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      2 | 1976 | `	}else{` |
|      - | 1977 | `		/* Expand 'static' */` |
|      5 | 1978 | `		ph7_value_string(pVal,"static",sizeof("static")-1);` |
|      - | 1979 | `	}` |
|      7 | 1980 | `}` |
|      - | 1981 | `/*` |
|      - | 1982 | ` * self` |
|      - | 1983 | ` * __CLASS__` |
|      - | 1984 | ` *  Expand the name of the current class. NULL otherwise.` |
|      - | 1985 | ` */` |
|      2 | 1986 | `static void PH7_self_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1987 | `{` |
|      3 | 1988 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1989 | `	ph7_class *pClass;` |
|      - | 1990 |  |
|      - | 1991 | `	/* Get the declaring class of the current method */` |
|      3 | 1992 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      3 | 1993 | `	if( pClass == 0 ){` |
|      - | 1994 | `		/* Not in a method, fall back to runtime class */` |
|      3 | 1995 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      1 | 1996 | `	}` |
|      - | 1997 |  |
|      3 | 1998 | `	if( pClass ){` |
|    ! 0 | 1999 | `		SyString *pName = &pClass->sName;` |
|      - | 2000 | `		/* Expand class name */` |
|    ! 0 | 2001 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|    ! 0 | 2002 | `	}else{` |
|      - | 2003 | `		/* Expand null */` |
|      3 | 2004 | `		ph7_value_null(pVal);` |
|      - | 2005 | `	}` |
|      3 | 2006 | `}` |
|      - | 2007 | `/* parent` |
|      - | 2008 | ` *  Expand the name of the parent class. NULL otherwise.` |
|      - | 2009 | ` */` |
|      2 | 2010 | `static void PH7_parent_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 2011 | `{` |
|      3 | 2012 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 2013 | `	ph7_class *pClass;` |
|      - | 2014 |  |
|      - | 2015 | `	/* Get the declaring class, then its parent */` |
|      3 | 2016 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      3 | 2017 | `	if( pClass && pClass->pBase ){` |
|    ! 0 | 2018 | `		SyString *pName = &pClass->pBase->sName;` |
|      - | 2019 | `		/* Expand parent class name */` |
|    ! 0 | 2020 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|    ! 0 | 2021 | `	}else{` |
|      - | 2022 | `		/* Expand null */` |
|      3 | 2023 | `		ph7_value_null(pVal);` |
|      - | 2024 | `	}` |
|      3 | 2025 | `}` |
|      - | 2026 |  |
|      - | 2027 | `/*` |
|      - | 2028 | ` * PASSWORD_BCRYPT / PASSWORD_DEFAULT` |
|      - | 2029 | ` *  The bcrypt algorithm identifier (PHP 7.4+ exposes these as the string "2y").` |
|      - | 2030 | ` *  PASSWORD_DEFAULT tracks the recommended default, currently bcrypt.` |
|      - | 2031 | ` */` |
|     20 | 2032 | `static void PH7_PASSWORD_BCRYPT_Const(ph7_value *pVal,void *pUnused)` |
|      2 | 2033 | `{` |
|     10 | 2034 | `	SXUNUSED(pUnused);` |
|     22 | 2035 | `	ph7_value_string(pVal,"2y",(int)sizeof("2y")-1);` |
|     22 | 2036 | `}` |
|      - | 2037 | `/*` |
|      - | 2038 | ` * PASSWORD_BCRYPT_DEFAULT_COST` |
|      - | 2039 | ` *  The default bcrypt work factor used by password_hash() (currently 12).` |
|      - | 2040 | ` */` |
|      2 | 2041 | `static void PH7_PASSWORD_COST_Const(ph7_value *pVal,void *pUnused)` |
|      1 | 2042 | `{` |
|      1 | 2043 | `	SXUNUSED(pUnused);` |
|      3 | 2044 | `	ph7_value_int(pVal,12);` |
|      3 | 2045 | `}` |
|      - | 2046 | `/*` |
|      - | 2047 | ` * filter_var() filter and flag identifiers (the ext/filter constants). Values` |
|      - | 2048 | ` * match PHP 8.5. One tiny int-returning callback per constant, generated by a` |
|      - | 2049 | ` * local macro to keep the ~25 near-identical definitions DRY.` |
|      - | 2050 | ` */` |
|      - | 2051 | `#define PH7_FILTER_INT_CONST(Name,Val) \` |
|      - | 2052 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|      - | 2053 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|      - | 2054 | `	}` |
|     10 | 2055 | `PH7_FILTER_INT_CONST(FILTER_DEFAULT,516)` |
|     17 | 2056 | `PH7_FILTER_INT_CONST(FILTER_UNSAFE_RAW,516)` |
|     64 | 2057 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_INT,257)` |
|     29 | 2058 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_BOOLEAN,258)` |
|     69 | 2059 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_FLOAT,259)` |
|      8 | 2060 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_REGEXP,272)` |
|     11 | 2061 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_DOMAIN,277)` |
|     15 | 2062 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_URL,273)` |
|     28 | 2063 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_EMAIL,274)` |
|     25 | 2064 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_IP,275)` |
|     11 | 2065 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_MAC,276)` |
|      3 | 2066 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_INT,519)` |
|      5 | 2067 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_FLOAT,520)` |
|     13 | 2068 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_SPECIAL_CHARS,515)` |
|     25 | 2069 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_FULL_SPECIAL_CHARS,522)` |
|      3 | 2070 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_EMAIL,517)` |
|      3 | 2071 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_URL,518)` |
|      3 | 2072 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_OCTAL,1)` |
|      3 | 2073 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_HEX,2)` |
|      7 | 2074 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_LOW,4)` |
|      5 | 2075 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_HIGH,8)` |
|      5 | 2076 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_LOW,16)` |
|      5 | 2077 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_HIGH,32)` |
|      3 | 2078 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_AMP,64)` |
|      3 | 2079 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_ENCODE_QUOTES,128)` |
|      3 | 2080 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_BACKTICK,512)` |
|      3 | 2081 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_FRACTION,4096)` |
|     25 | 2082 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_THOUSAND,8192)` |
|      3 | 2083 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_SCIENTIFIC,16384)` |
|      5 | 2084 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV4,1048576)` |
|      3 | 2085 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV6,2097152)` |
|     14 | 2086 | `PH7_FILTER_INT_CONST(FILTER_NULL_ON_FAILURE,134217728)` |
|      - | 2087 | `/* filter_input() source selectors (php values; SESSION/REQUEST are undefined in 8.5) */` |
|      5 | 2088 | `PH7_FILTER_INT_CONST(INPUT_POST,0)` |
|      8 | 2089 | `PH7_FILTER_INT_CONST(INPUT_GET,1)` |
|      3 | 2090 | `PH7_FILTER_INT_CONST(INPUT_COOKIE,2)` |
|      3 | 2091 | `PH7_FILTER_INT_CONST(INPUT_ENV,4)` |
|     21 | 2092 | `PH7_FILTER_INT_CONST(INPUT_SERVER,5)` |
|      - | 2093 | `/*` |
|      - | 2094 | ` * Table of built-in constants.` |
|      - | 2095 | ` */` |
|      - | 2096 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|      - | 2097 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|      - | 2098 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|      - | 2099 | `	{"__PH7__",              PH7_VER_Const      },` |
|      - | 2100 | `	{"PHP_VERSION",          PH7_PHPVerConst    },` |
|      - | 2101 | `	{"PHP_MAJOR_VERSION",    PH7_PHPMajorConst  },` |
|      - | 2102 | `	{"PHP_MINOR_VERSION",    PH7_PHPMinorConst  },` |
|      - | 2103 | `	{"PHP_RELEASE_VERSION",  PH7_PHPReleaseConst},` |
|      - | 2104 | `	{"PHP_EXTRA_VERSION",    PH7_PHPExtraConst  },` |
|      - | 2105 | `	{"PHP_VERSION_ID",       PH7_PHPVerIdConst  },` |
|      - | 2106 | `	{"PHP_OS",               PH7_OS_Const       },` |
|      - | 2107 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|      - | 2108 | `	{"PHP_SESSION_DISABLED", PH7_PHP_SESSION_DISABLED_Const },` |
|      - | 2109 | `	{"PHP_SESSION_NONE",     PH7_PHP_SESSION_NONE_Const },` |
|      - | 2110 | `	{"PHP_SESSION_ACTIVE",   PH7_PHP_SESSION_ACTIVE_Const },` |
|      - | 2111 | `	{"INI_USER",             PH7_INI_USER_Const },` |
|      - | 2112 | `	{"INI_PERDIR",           PH7_INI_PERDIR_Const },` |
|      - | 2113 | `	{"INI_SYSTEM",           PH7_INI_SYSTEM_Const },` |
|      - | 2114 | `	{"INI_ALL",              PH7_INI_ALL_Const },` |
|      - | 2115 | `	{"MB_CASE_UPPER",        PH7_MB_CASE_UPPER_Const },` |
|      - | 2116 | `	{"MB_CASE_LOWER",        PH7_MB_CASE_LOWER_Const },` |
|      - | 2117 | `	{"MB_CASE_TITLE",        PH7_MB_CASE_TITLE_Const },` |
|      - | 2118 | `	{"PASSWORD_BCRYPT",      PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2119 | `	{"PASSWORD_DEFAULT",     PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2120 | `	{"PASSWORD_BCRYPT_DEFAULT_COST", PH7_PASSWORD_COST_Const },` |
|      - | 2121 | `	{"FILTER_DEFAULT",              PH7_FILTER_DEFAULT_Const },` |
|      - | 2122 | `	{"FILTER_UNSAFE_RAW",           PH7_FILTER_UNSAFE_RAW_Const },` |
|      - | 2123 | `	{"FILTER_VALIDATE_INT",         PH7_FILTER_VALIDATE_INT_Const },` |
|      - | 2124 | `	{"FILTER_VALIDATE_BOOLEAN",     PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2125 | `	{"FILTER_VALIDATE_BOOL",        PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2126 | `	{"FILTER_VALIDATE_FLOAT",       PH7_FILTER_VALIDATE_FLOAT_Const },` |
|      - | 2127 | `	{"FILTER_VALIDATE_REGEXP",      PH7_FILTER_VALIDATE_REGEXP_Const },` |
|      - | 2128 | `	{"FILTER_VALIDATE_DOMAIN",      PH7_FILTER_VALIDATE_DOMAIN_Const },` |
|      - | 2129 | `	{"FILTER_VALIDATE_URL",         PH7_FILTER_VALIDATE_URL_Const },` |
|      - | 2130 | `	{"FILTER_VALIDATE_EMAIL",       PH7_FILTER_VALIDATE_EMAIL_Const },` |
|      - | 2131 | `	{"FILTER_VALIDATE_IP",          PH7_FILTER_VALIDATE_IP_Const },` |
|      - | 2132 | `	{"FILTER_VALIDATE_MAC",         PH7_FILTER_VALIDATE_MAC_Const },` |
|      - | 2133 | `	{"FILTER_SANITIZE_NUMBER_INT",  PH7_FILTER_SANITIZE_NUMBER_INT_Const },` |
|      - | 2134 | `	{"FILTER_SANITIZE_NUMBER_FLOAT",PH7_FILTER_SANITIZE_NUMBER_FLOAT_Const },` |
|      - | 2135 | `	{"FILTER_SANITIZE_SPECIAL_CHARS",PH7_FILTER_SANITIZE_SPECIAL_CHARS_Const },` |
|      - | 2136 | `	{"FILTER_SANITIZE_FULL_SPECIAL_CHARS",PH7_FILTER_SANITIZE_FULL_SPECIAL_CHARS_Const },` |
|      - | 2137 | `	{"FILTER_SANITIZE_EMAIL",       PH7_FILTER_SANITIZE_EMAIL_Const },` |
|      - | 2138 | `	{"FILTER_SANITIZE_URL",         PH7_FILTER_SANITIZE_URL_Const },` |
|      - | 2139 | `	{"FILTER_FLAG_ALLOW_OCTAL",     PH7_FILTER_FLAG_ALLOW_OCTAL_Const },` |
|      - | 2140 | `	{"FILTER_FLAG_ALLOW_HEX",       PH7_FILTER_FLAG_ALLOW_HEX_Const },` |
|      - | 2141 | `	{"FILTER_FLAG_STRIP_LOW",       PH7_FILTER_FLAG_STRIP_LOW_Const },` |
|      - | 2142 | `	{"FILTER_FLAG_STRIP_HIGH",      PH7_FILTER_FLAG_STRIP_HIGH_Const },` |
|      - | 2143 | `	{"FILTER_FLAG_ENCODE_LOW",      PH7_FILTER_FLAG_ENCODE_LOW_Const },` |
|      - | 2144 | `	{"FILTER_FLAG_ENCODE_HIGH",     PH7_FILTER_FLAG_ENCODE_HIGH_Const },` |
|      - | 2145 | `	{"FILTER_FLAG_ENCODE_AMP",      PH7_FILTER_FLAG_ENCODE_AMP_Const },` |
|      - | 2146 | `	{"FILTER_FLAG_NO_ENCODE_QUOTES",PH7_FILTER_FLAG_NO_ENCODE_QUOTES_Const },` |
|      - | 2147 | `	{"FILTER_FLAG_STRIP_BACKTICK",  PH7_FILTER_FLAG_STRIP_BACKTICK_Const },` |
|      - | 2148 | `	{"FILTER_FLAG_ALLOW_FRACTION",  PH7_FILTER_FLAG_ALLOW_FRACTION_Const },` |
|      - | 2149 | `	{"FILTER_FLAG_ALLOW_THOUSAND",  PH7_FILTER_FLAG_ALLOW_THOUSAND_Const },` |
|      - | 2150 | `	{"FILTER_FLAG_ALLOW_SCIENTIFIC",PH7_FILTER_FLAG_ALLOW_SCIENTIFIC_Const },` |
|      - | 2151 | `	{"FILTER_FLAG_IPV4",            PH7_FILTER_FLAG_IPV4_Const },` |
|      - | 2152 | `	{"FILTER_FLAG_IPV6",            PH7_FILTER_FLAG_IPV6_Const },` |
|      - | 2153 | `	{"FILTER_NULL_ON_FAILURE",      PH7_FILTER_NULL_ON_FAILURE_Const },` |
|      - | 2154 | `	{"INPUT_POST",                  PH7_INPUT_POST_Const },` |
|      - | 2155 | `	{"INPUT_GET",                   PH7_INPUT_GET_Const },` |
|      - | 2156 | `	{"INPUT_COOKIE",                PH7_INPUT_COOKIE_Const },` |
|      - | 2157 | `	{"INPUT_ENV",                   PH7_INPUT_ENV_Const },` |
|      - | 2158 | `	{"INPUT_SERVER",                PH7_INPUT_SERVER_Const },` |
|      - | 2159 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|      - | 2160 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|      - | 2161 | `	{"PHP_INT_MIN",          PH7_INTMIN_Const   },` |
|      - | 2162 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|      - | 2163 | `	{"PHP_FLOAT_EPSILON",    PH7_FLOATEPSILON_Const },` |
|      - | 2164 | `	{"PHP_FLOAT_MAX",        PH7_FLOATMAX_Const },` |
|      - | 2165 | `	{"PHP_FLOAT_MIN",        PH7_FLOATMIN_Const },` |
|      - | 2166 | `	{"PHP_FLOAT_DIG",        PH7_FLOATDIG_Const },` |
|      - | 2167 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|      - | 2168 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|      - | 2169 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|      - | 2170 | `	{"__TIME__",             PH7_TIME_Const     },` |
|      - | 2171 | `	{"__DATE__",             PH7_DATE_Const     },` |
|      - | 2172 | `	{"__FILE__",             PH7_FILE_Const     },` |
|      - | 2173 | `	{"__DIR__",              PH7_DIR_Const      },` |
|      - | 2174 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|      - | 2175 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|      - | 2176 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|      - | 2177 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|      - | 2178 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|      - | 2179 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|      - | 2180 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|      - | 2181 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|      - | 2182 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|      - | 2183 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|      - | 2184 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|      - | 2185 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|      - | 2186 | `	{"E_STRICT",             PH7_E_STRICT_Const        },` |
|      - | 2187 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|      - | 2188 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|      - | 2189 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|      - | 2190 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|      - | 2191 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|      - | 2192 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|      - | 2193 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|      - | 2194 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|      - | 2195 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|      - | 2196 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|      - | 2197 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|      - | 2198 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|      - | 2199 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|      - | 2200 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|      - | 2201 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|      - | 2202 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|      - | 2203 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|      - | 2204 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|      - | 2205 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|      - | 2206 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|      - | 2207 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|      - | 2208 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|      - | 2209 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 2210 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|      - | 2211 | `	{"M_E",                  PH7_M_E_Const          },` |
|      - | 2212 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|      - | 2213 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|      - | 2214 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|      - | 2215 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|      - | 2216 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|      - | 2217 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|      - | 2218 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|      - | 2219 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|      - | 2220 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|      - | 2221 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|      - | 2222 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|      - | 2223 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|      - | 2224 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|      - | 2225 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|      - | 2226 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|      - | 2227 | `	{"NAN",                  PH7_NAN_Const          },` |
|      - | 2228 | `	{"INF",                  PH7_INF_Const          },` |
|      - | 2229 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 2230 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|      - | 2231 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|      - | 2232 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|      - | 2233 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|      - | 2234 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|      - | 2235 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|      - | 2236 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|      - | 2237 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|      - | 2238 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|      - | 2239 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|      - | 2240 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|      - | 2241 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|      - | 2242 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|      - | 2243 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|      - | 2244 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|      - | 2245 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|      - | 2246 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|      - | 2247 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|      - | 2248 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|      - | 2249 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|      - | 2250 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|      - | 2251 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|      - | 2252 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|      - | 2253 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|      - | 2254 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|      - | 2255 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|      - | 2256 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|      - | 2257 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|      - | 2258 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|      - | 2259 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|      - | 2260 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|      - | 2261 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|      - | 2262 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|      - | 2263 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|      - | 2264 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|      - | 2265 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|      - | 2266 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|      - | 2267 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|      - | 2268 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|      - | 2269 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|      - | 2270 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|      - | 2271 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|      - | 2272 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|      - | 2273 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|      - | 2274 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|      - | 2275 | `	{"ASSERT_ACTIVE",        PH7_ASSERT_ACTIVE_Const     },` |
|      - | 2276 | `	{"ASSERT_CALLBACK",      PH7_ASSERT_CALLBACK_Const   },` |
|      - | 2277 | `	{"ASSERT_BAIL",          PH7_ASSERT_BAIL_Const       },` |
|      - | 2278 | `	{"ASSERT_WARNING",       PH7_ASSERT_WARNING_Const    },` |
|      - | 2279 | `	{"ASSERT_EXCEPTION",     PH7_ASSERT_EXCEPTION_Const  },` |
|      - | 2280 | `	{"ASSERT_QUIET_EVAL",    PH7_ASSERT_QUIET_EVAL_Const },` |
|      - | 2281 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|      - | 2282 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|      - | 2283 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|      - | 2284 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|      - | 2285 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|      - | 2286 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|      - | 2287 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|      - | 2288 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|      - | 2289 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|      - | 2290 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|      - | 2291 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|      - | 2292 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|      - | 2293 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|      - | 2294 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|      - | 2295 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|      - | 2296 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|      - | 2297 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|      - | 2298 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|      - | 2299 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|      - | 2300 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|      - | 2301 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|      - | 2302 | `	{"STDIN",                PH7_STDIN_Const        },` |
|      - | 2303 | `	{"stdin",                PH7_STDIN_Const        },` |
|      - | 2304 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|      - | 2305 | `	{"stdout",               PH7_STDOUT_Const       },` |
|      - | 2306 | `	{"STDERR",               PH7_STDERR_Const       },` |
|      - | 2307 | `	{"stderr",               PH7_STDERR_Const       },` |
|      - | 2308 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|      - | 2309 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|      - | 2310 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|      - | 2311 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|      - | 2312 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|      - | 2313 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|      - | 2314 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|      - | 2315 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|      - | 2316 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|      - | 2317 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2318 | `	{"XML_ERROR_NONE",       PH7_XML_ERROR_NONE_Const},` |
|      - | 2319 | `	{"XML_ERROR_NO_MEMORY",  PH7_XML_ERROR_NO_MEMORY_Const},` |
|      - | 2320 | `	{"XML_ERROR_SYNTAX",     PH7_XML_ERROR_SYNTAX_Const},` |
|      - | 2321 | `	{"XML_ERROR_NO_ELEMENTS",PH7_XML_ERROR_NO_ELEMENTS_Const},` |
|      - | 2322 | `	{"XML_ERROR_INVALID_TOKEN", PH7_XML_ERROR_INVALID_TOKEN_Const},` |
|      - | 2323 | `	{"XML_ERROR_UNCLOSED_TOKEN",PH7_XML_ERROR_UNCLOSED_TOKEN_Const},` |
|      - | 2324 | `	{"XML_ERROR_PARTIAL_CHAR",  PH7_XML_ERROR_PARTIAL_CHAR_Const},` |
|      - | 2325 | `	{"XML_ERROR_TAG_MISMATCH",  PH7_XML_ERROR_TAG_MISMATCH_Const},` |
|      - | 2326 | `	{"XML_ERROR_DUPLICATE_ATTRIBUTE",   PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const},` |
|      - | 2327 | `	{"XML_ERROR_JUNK_AFTER_DOC_ELEMENT",PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const},` |
|      - | 2328 | `	{"XML_ERROR_PARAM_ENTITY_REF",      PH7_XML_ERROR_PARAM_ENTITY_REF_Const},` |
|      - | 2329 | `	{"XML_ERROR_UNDEFINED_ENTITY",      PH7_XML_ERROR_UNDEFINED_ENTITY_Const},` |
|      - | 2330 | `	{"XML_ERROR_RECURSIVE_ENTITY_REF",  PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const},` |
|      - | 2331 | `	{"XML_ERROR_ASYNC_ENTITY",          PH7_XML_ERROR_ASYNC_ENTITY_Const},` |
|      - | 2332 | `	{"XML_ERROR_BAD_CHAR_REF",          PH7_XML_ERROR_BAD_CHAR_REF_Const},` |
|      - | 2333 | `	{"XML_ERROR_BINARY_ENTITY_REF",     PH7_XML_ERROR_BINARY_ENTITY_REF_Const},` |
|      - | 2334 | `	{"XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF", PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const},` |
|      - | 2335 | `	{"XML_ERROR_MISPLACED_XML_PI",     PH7_XML_ERROR_MISPLACED_XML_PI_Const},` |
|      - | 2336 | `	{"XML_ERROR_UNKNOWN_ENCODING",     PH7_XML_ERROR_UNKNOWN_ENCODING_Const},` |
|      - | 2337 | `	{"XML_ERROR_INCORRECT_ENCODING",   PH7_XML_ERROR_INCORRECT_ENCODING_Const},` |
|      - | 2338 | `	{"XML_ERROR_UNCLOSED_CDATA_SECTION",  PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const},` |
|      - | 2339 | `	{"XML_ERROR_EXTERNAL_ENTITY_HANDLING",PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const},` |
|      - | 2340 | `	{"XML_OPTION_CASE_FOLDING",           PH7_XML_OPTION_CASE_FOLDING_Const},` |
|      - | 2341 | `	{"XML_OPTION_TARGET_ENCODING",        PH7_XML_OPTION_TARGET_ENCODING_Const},` |
|      - | 2342 | `	{"XML_OPTION_SKIP_TAGSTART",          PH7_XML_OPTION_SKIP_TAGSTART_Const},` |
|      - | 2343 | `	{"XML_OPTION_SKIP_WHITE",             PH7_XML_OPTION_SKIP_WHITE_Const},` |
|      - | 2344 | `	{"XML_SAX_IMPL",           PH7_XML_SAX_IMP_Const},` |
|      - | 2345 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2346 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|      - | 2347 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|      - | 2348 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|      - | 2349 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|      - | 2350 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|      - | 2351 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|      - | 2352 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|      - | 2353 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|      - | 2354 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|      - | 2355 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|      - | 2356 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|      - | 2357 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|      - | 2358 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|      - | 2359 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|      - | 2360 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|      - | 2361 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|      - | 2362 | `	{"JSON_ERROR_NON_BACKED_ENUM", PH7_JSON_ERROR_NON_BACKED_ENUM_Const},` |
|      - | 2363 | `	{"static",               PH7_static_Const       },` |
|      - | 2364 | `	{"self",                 PH7_self_Const         },` |
|      - | 2365 | `	{"__CLASS__",            PH7_self_Const         },` |
|      - | 2366 | `	{"parent",               PH7_parent_Const       }` |
|      - | 2367 | `};` |
|      - | 2368 | `/*` |
|      - | 2369 | ` * Register the built-in constants defined above.` |
|      - | 2370 | ` */` |
|   3550 | 2371 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|      5 | 2372 | `{` |
|      - | 2373 | `	sxu32 n;` |
|      - | 2374 | `	/*` |
|      - | 2375 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|      - | 2376 | `	 * that trigger the constant invocation as their private data.` |
|      - | 2377 | `	 */` |
| 947855 | 2378 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 944305 | 2379 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
| 472155 | 2380 | `	}` |
|   3555 | 2381 | `}` |
