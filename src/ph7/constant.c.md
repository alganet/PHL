# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1119/1139 lines (98.24%)

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
|   3936 |   63 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|      5 |   64 | `{` |
|      - |   65 | `#if defined(__WINNT__)` |
|      5 |   66 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|      - |   67 | `#elif defined(__UNIXES__)` |
|      - |   68 | `	struct utsname sInfo;` |
|   3936 |   69 | `	if( uname(&sInfo) != 0 ){` |
|    ! 0 |   70 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 |   71 | `	}else{` |
|   3936 |   72 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|      - |   73 | `	}` |
|      - |   74 | `#else` |
|      - |   75 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|      - |   76 | `#endif` |
|   1968 |   77 | `	SXUNUSED(pUnused);` |
|   3941 |   78 | `}` |
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
|    158 |  149 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|      3 |  150 | `{` |
|     79 |  151 | `	SXUNUSED(pUnused);` |
|      - |  152 | `#ifdef __WINNT__` |
|      3 |  153 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|      - |  154 | `#else` |
|    158 |  155 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|      - |  156 | `#endif` |
|    161 |  157 | `}` |
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
|   2472 |  244 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  245 | `{` |
|   2477 |  246 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  247 | `	SyString *pFile;` |
|      - |  248 | `	/* Peek the top entry */` |
|   2477 |  249 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|   2477 |  250 | `	if( pFile == 0 ){` |
|      - |  251 | `		/* Expand the magic word: ":MEMORY:" */` |
|    ! 0 |  252 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|    ! 0 |  253 | `	}else{` |
|   2477 |  254 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|      - |  255 | `	}` |
|   2477 |  256 | `}` |
|      - |  257 | `/*` |
|      - |  258 | ` * __DIR__` |
|      - |  259 | ` *  Directory holding the processed script.` |
|      - |  260 | ` */` |
|     40 |  261 | `static void PH7_DIR_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  262 | `{` |
|     45 |  263 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  264 | `	SyString *pFile;` |
|      - |  265 | `	/* Peek the top entry */` |
|     45 |  266 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     45 |  267 | `	if( pFile == 0 ){` |
|      - |  268 | `		/* Expand the magic word: ":MEMORY:" */` |
|    ! 0 |  269 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|    ! 0 |  270 | `	}else{` |
|     45 |  271 | `		if( pFile->nByte > 0 ){` |
|      - |  272 | `			const char *zDir;` |
|      - |  273 | `			int nLen;` |
|     45 |  274 | `			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|     45 |  275 | `			ph7_value_string(pVal,zDir,nLen);` |
|     25 |  276 | `		}else{` |
|      - |  277 | `			/* Expand '.' as the current directory*/` |
|    ! 0 |  278 | `			ph7_value_string(pVal,".",(int)sizeof(char));` |
|      - |  279 | `		}` |
|      - |  280 | `	}` |
|     45 |  281 | `}` |
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
|      - |  576 | ` * SPHP_ROUND_HALF_DOWN` |
|      - |  577 | ` *  Expands 2.` |
|      - |  578 | ` */` |
|      4 |  579 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  580 | `{` |
|      5 |  581 | `	ph7_value_int(pVal,2);` |
|      2 |  582 | `	SXUNUSED(pUserData);` |
|      5 |  583 | `}` |
|      - |  584 | `/*` |
|      - |  585 | ` * PHP_ROUND_HALF_EVEN` |
|      - |  586 | ` *  Expands 3.` |
|      - |  587 | ` */` |
|      8 |  588 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  589 | `{` |
|      9 |  590 | `	ph7_value_int(pVal,3);` |
|      4 |  591 | `	SXUNUSED(pUserData);` |
|      9 |  592 | `}` |
|      - |  593 | `/*` |
|      - |  594 | ` * PHP_ROUND_HALF_ODD` |
|      - |  595 | ` *  Expands 4.` |
|      - |  596 | ` */` |
|      4 |  597 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  598 | `{` |
|      5 |  599 | `	ph7_value_int(pVal,4);` |
|      2 |  600 | `	SXUNUSED(pUserData);` |
|      5 |  601 | `}` |
|      - |  602 | `/*` |
|      - |  603 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|      - |  604 | ` *  Expand 0x01` |
|      - |  605 | ` * NOTE:` |
|      - |  606 | ` *  The expanded value must be a power of two.` |
|      - |  607 | ` */` |
|      2 |  608 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  609 | `{` |
|      3 |  610 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      1 |  611 | `	SXUNUSED(pUserData);` |
|      3 |  612 | `}` |
|      - |  613 | `/*` |
|      - |  614 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|      - |  615 | ` *  Expand 0x02` |
|      - |  616 | ` * NOTE:` |
|      - |  617 | ` *  The expanded value must be a power of two.` |
|      - |  618 | ` */` |
|      2 |  619 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  620 | `{` |
|      3 |  621 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      1 |  622 | `	SXUNUSED(pUserData);` |
|      3 |  623 | `}` |
|      - |  624 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  625 | `/*` |
|      - |  626 | ` * M_PI` |
|      - |  627 | ` *  Expand the value of pi.` |
|      - |  628 | ` */` |
|      2 |  629 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  630 | `{` |
|      1 |  631 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  632 | `	ph7_value_double(pVal,PH7_PI);` |
|      3 |  633 | `}` |
|      - |  634 | `/*` |
|      - |  635 | ` * M_E` |
|      - |  636 | ` *  Expand 2.7182818284590452354` |
|      - |  637 | ` */` |
|      2 |  638 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  639 | `{` |
|      1 |  640 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  641 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|      3 |  642 | `}` |
|      - |  643 | `/*` |
|      - |  644 | ` * M_LOG2E` |
|      - |  645 | ` *  Expand 2.7182818284590452354` |
|      - |  646 | ` */` |
|      2 |  647 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  648 | `{` |
|      1 |  649 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  650 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      3 |  651 | `}` |
|      - |  652 | `/*` |
|      - |  653 | ` * M_LOG10E` |
|      - |  654 | ` *  Expand 0.4342944819032518276` |
|      - |  655 | ` */` |
|      2 |  656 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  657 | `{` |
|      1 |  658 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  659 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      3 |  660 | `}` |
|      - |  661 | `/*` |
|      - |  662 | ` * M_LN2` |
|      - |  663 | ` *  Expand 	0.69314718055994530942` |
|      - |  664 | ` */` |
|      2 |  665 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  666 | `{` |
|      1 |  667 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  668 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      3 |  669 | `}` |
|      - |  670 | `/*` |
|      - |  671 | ` * M_LN10` |
|      - |  672 | ` *  Expand 	2.30258509299404568402` |
|      - |  673 | ` */` |
|      2 |  674 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  675 | `{` |
|      1 |  676 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  677 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      3 |  678 | `}` |
|      - |  679 | `/*` |
|      - |  680 | ` * M_PI_2` |
|      - |  681 | ` *  Expand 	1.57079632679489661923` |
|      - |  682 | ` */` |
|      2 |  683 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  684 | `{` |
|      1 |  685 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  686 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      3 |  687 | `}` |
|      - |  688 | `/*` |
|      - |  689 | ` * M_PI_4` |
|      - |  690 | ` *  Expand 	0.78539816339744830962` |
|      - |  691 | ` */` |
|      2 |  692 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  693 | `{` |
|      1 |  694 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  695 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      3 |  696 | `}` |
|      - |  697 | `/*` |
|      - |  698 | ` * M_1_PI` |
|      - |  699 | ` *  Expand 	0.31830988618379067154` |
|      - |  700 | ` */` |
|      2 |  701 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  702 | `{` |
|      1 |  703 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  704 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      3 |  705 | `}` |
|      - |  706 | `/*` |
|      - |  707 | ` * M_2_PI` |
|      - |  708 | ` *  Expand 0.63661977236758134308` |
|      - |  709 | ` */` |
|      4 |  710 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  711 | `{` |
|      2 |  712 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  713 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      5 |  714 | `}` |
|      - |  715 | `/*` |
|      - |  716 | ` * M_SQRTPI` |
|      - |  717 | ` *  Expand 1.77245385090551602729` |
|      - |  718 | ` */` |
|      2 |  719 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  720 | `{` |
|      1 |  721 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  722 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      3 |  723 | `}` |
|      - |  724 | `/*` |
|      - |  725 | ` * M_2_SQRTPI` |
|      - |  726 | ` *  Expand 	1.12837916709551257390` |
|      - |  727 | ` */` |
|      2 |  728 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  729 | `{` |
|      1 |  730 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  731 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      3 |  732 | `}` |
|      - |  733 | `/*` |
|      - |  734 | ` * M_SQRT2` |
|      - |  735 | ` *  Expand 	1.41421356237309504880` |
|      - |  736 | ` */` |
|      2 |  737 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  738 | `{` |
|      1 |  739 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  740 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      3 |  741 | `}` |
|      - |  742 | `/*` |
|      - |  743 | ` * M_SQRT3` |
|      - |  744 | ` *  Expand 	1.73205080756887729352` |
|      - |  745 | ` */` |
|      2 |  746 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  747 | `{` |
|      1 |  748 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  749 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      3 |  750 | `}` |
|      - |  751 | `/*` |
|      - |  752 | ` * M_SQRT1_2` |
|      - |  753 | ` *  Expand 	0.70710678118654752440` |
|      - |  754 | ` */` |
|      2 |  755 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  756 | `{` |
|      1 |  757 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  758 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      3 |  759 | `}` |
|      - |  760 | `/*` |
|      - |  761 | ` * M_LNPI` |
|      - |  762 | ` *  Expand 	1.14472988584940017414` |
|      - |  763 | ` */` |
|      2 |  764 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  765 | `{` |
|      1 |  766 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  767 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      3 |  768 | `}` |
|      - |  769 | `/*` |
|      - |  770 | ` * M_EULER` |
|      - |  771 | ` *  Expand  0.57721566490153286061` |
|      - |  772 | ` */` |
|      2 |  773 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  774 | `{` |
|      1 |  775 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  776 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      3 |  777 | `}` |
|      - |  778 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|      - |  779 | `/*` |
|      - |  780 | ` * DATE_ATOM` |
|      - |  781 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|      - |  782 | ` */` |
|      2 |  783 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  784 | `{` |
|      1 |  785 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  786 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  787 | `}` |
|      - |  788 | `/*` |
|      - |  789 | ` * DATE_COOKIE` |
|      - |  790 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  791 | ` */` |
|      2 |  792 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  793 | `{` |
|      1 |  794 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  795 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  796 | `}` |
|      - |  797 | `/*` |
|      - |  798 | ` * DATE_ISO8601` |
|      - |  799 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|      - |  800 | ` */` |
|      2 |  801 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  802 | `{` |
|      1 |  803 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  804 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      3 |  805 | `}` |
|      - |  806 | `/*` |
|      - |  807 | ` * DATE_RFC822` |
|      - |  808 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|      - |  809 | ` */` |
|      2 |  810 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  811 | `{` |
|      1 |  812 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  813 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  814 | `}` |
|      - |  815 | `/*` |
|      - |  816 | ` * DATE_RFC850` |
|      - |  817 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  818 | ` */` |
|      2 |  819 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  820 | `{` |
|      1 |  821 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  822 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  823 | `}` |
|      - |  824 | `/*` |
|      - |  825 | ` * DATE_RFC1036` |
|      - |  826 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  827 | ` */` |
|      2 |  828 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  829 | `{` |
|      1 |  830 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  831 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  832 | `}` |
|      - |  833 | `/*` |
|      - |  834 | ` * DATE_RFC1123` |
|      - |  835 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  836 | ` */` |
|      2 |  837 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  838 | `{` |
|      1 |  839 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  840 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  841 | `}` |
|      - |  842 | `/*` |
|      - |  843 | ` * DATE_RFC2822` |
|      - |  844 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  845 | ` */` |
|      2 |  846 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  847 | `{` |
|      1 |  848 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  849 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  850 | `}` |
|      - |  851 | `/*` |
|      - |  852 | ` * DATE_RSS` |
|      - |  853 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  854 | ` */` |
|      2 |  855 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  856 | `{` |
|      1 |  857 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  858 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  859 | `}` |
|      - |  860 | `/*` |
|      - |  861 | ` * DATE_W3C` |
|      - |  862 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|      - |  863 | ` */` |
|      2 |  864 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  865 | `{` |
|      1 |  866 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  867 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  868 | `}` |
|      - |  869 | `/*` |
|      - |  870 | ` * The ENT_* values are PHP-exact (php 8.5.7). The low two bits are the quote` |
|      - |  871 | ` * bits (1 = single, 2 = double), so ENT_QUOTES = ENT_COMPAT\|1 and` |
|      - |  872 | ` * ENT_NOQUOTES = 0. Bits 16\|32 select the doctype (0 = HTML401, 16 = XML1,` |
|      - |  873 | ` * 32 = XHTML, 48 = HTML5) — composites, not flags.` |
|      - |  874 | ` */` |
|      - |  875 | `/*` |
|      - |  876 | ` * ENT_COMPAT` |
|      - |  877 | ` *  Expand 2 (double-quote bit only)` |
|      - |  878 | ` */` |
|     12 |  879 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  880 | `{` |
|      6 |  881 | `	SXUNUSED(pUserData); /* cc warning */` |
|     13 |  882 | `	ph7_value_int(pVal,PH7_ENT_QUOTE_DOUBLE);` |
|     13 |  883 | `}` |
|      - |  884 | `/*` |
|      - |  885 | ` * ENT_QUOTES` |
|      - |  886 | ` *  Expand 3 (double\|single quote bits)` |
|      - |  887 | ` */` |
|     60 |  888 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  889 | `{` |
|     30 |  890 | `	SXUNUSED(pUserData); /* cc warning */` |
|     61 |  891 | `	ph7_value_int(pVal,PH7_ENT_QUOTES);` |
|     61 |  892 | `}` |
|      - |  893 | `/*` |
|      - |  894 | ` * ENT_NOQUOTES` |
|      - |  895 | ` *  Expand 0 (no quote bits)` |
|      - |  896 | ` */` |
|     20 |  897 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  898 | `{` |
|     10 |  899 | `	SXUNUSED(pUserData); /* cc warning */` |
|     21 |  900 | `	ph7_value_int(pVal,0);` |
|     21 |  901 | `}` |
|      - |  902 | `/*` |
|      - |  903 | ` * ENT_IGNORE` |
|      - |  904 | ` *  Expand 4` |
|      - |  905 | ` */` |
|      6 |  906 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  907 | `{` |
|      3 |  908 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 |  909 | `	ph7_value_int(pVal,PH7_ENT_IGNORE);` |
|      7 |  910 | `}` |
|      - |  911 | `/*` |
|      - |  912 | ` * ENT_SUBSTITUTE` |
|      - |  913 | ` *  Expand 8` |
|      - |  914 | ` */` |
|      2 |  915 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  916 | `{` |
|      1 |  917 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  918 | `	ph7_value_int(pVal,PH7_ENT_SUBSTITUTE);` |
|      3 |  919 | `}` |
|      - |  920 | `/*` |
|      - |  921 | ` * ENT_DISALLOWED` |
|      - |  922 | ` *  Expand 128` |
|      - |  923 | ` */` |
|      2 |  924 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  925 | `{` |
|      1 |  926 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  927 | `	ph7_value_int(pVal,PH7_ENT_DISALLOWED);` |
|      3 |  928 | `}` |
|      - |  929 | `/*` |
|      - |  930 | ` * ENT_HTML401` |
|      - |  931 | ` *  Expand 0 (the default doctype)` |
|      - |  932 | ` */` |
|      2 |  933 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  934 | `{` |
|      1 |  935 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  936 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML401);` |
|      3 |  937 | `}` |
|      - |  938 | `/*` |
|      - |  939 | ` * ENT_XML1` |
|      - |  940 | ` *  Expand 16` |
|      - |  941 | ` */` |
|      8 |  942 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  943 | `{` |
|      4 |  944 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 |  945 | `	ph7_value_int(pVal,PH7_ENT_DOC_XML1);` |
|      9 |  946 | `}` |
|      - |  947 | `/*` |
|      - |  948 | ` * ENT_XHTML` |
|      - |  949 | ` *  Expand 32` |
|      - |  950 | ` */` |
|      6 |  951 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  952 | `{` |
|      3 |  953 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 |  954 | `	ph7_value_int(pVal,PH7_ENT_DOC_XHTML);` |
|      7 |  955 | `}` |
|      - |  956 | `/*` |
|      - |  957 | ` * ENT_HTML5` |
|      - |  958 | ` *  Expand 48 (16\|32 — a doctype composite, not a flag bit)` |
|      - |  959 | ` */` |
|      8 |  960 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  961 | `{` |
|      4 |  962 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 |  963 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML5);` |
|      9 |  964 | `}` |
|      - |  965 | `/*` |
|      - |  966 | ` * ISO-8859-1` |
|      - |  967 | ` * ISO_8859_1` |
|      - |  968 | ` *   Expand 1` |
|      - |  969 | ` */` |
|      2 |  970 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  971 | `{` |
|      1 |  972 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  973 | `	ph7_value_int(pVal,1);` |
|      3 |  974 | `}` |
|      - |  975 | `/*` |
|      - |  976 | ` * UTF-8` |
|      - |  977 | ` * UTF8` |
|      - |  978 | ` *  Expand 2` |
|      - |  979 | ` */` |
|      2 |  980 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  981 | `{` |
|      1 |  982 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  983 | `	ph7_value_int(pVal,1);` |
|      3 |  984 | `}` |
|      - |  985 | `/*` |
|      - |  986 | ` * HTML_ENTITIES` |
|      - |  987 | ` *  Expand 1` |
|      - |  988 | ` */` |
|      4 |  989 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  990 | `{` |
|      2 |  991 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  992 | `	ph7_value_int(pVal,1);` |
|      5 |  993 | `}` |
|      - |  994 | `/*` |
|      - |  995 | ` * HTML_SPECIALCHARS` |
|      - |  996 | ` *  Expand 0 (PHP-exact)` |
|      - |  997 | ` */` |
|     10 |  998 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  999 | `{` |
|      5 | 1000 | `	SXUNUSED(pUserData); /* cc warning */` |
|     11 | 1001 | `	ph7_value_int(pVal,0);` |
|     11 | 1002 | `}` |
|      - | 1003 | `/*` |
|      - | 1004 | ` * PHP_URL_SCHEME.` |
|      - | 1005 | ` * Expand 1` |
|      - | 1006 | ` */` |
|      2 | 1007 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1008 | `{` |
|      1 | 1009 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1010 | `	ph7_value_int(pVal,1);` |
|      3 | 1011 | `}` |
|      - | 1012 | `/*` |
|      - | 1013 | ` * PHP_URL_HOST.` |
|      - | 1014 | ` * Expand 2` |
|      - | 1015 | ` */` |
|      2 | 1016 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1017 | `{` |
|      1 | 1018 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1019 | `	ph7_value_int(pVal,2);` |
|      3 | 1020 | `}` |
|      - | 1021 | `/*` |
|      - | 1022 | ` * PHP_URL_PORT.` |
|      - | 1023 | ` * Expand 3` |
|      - | 1024 | ` */` |
|      2 | 1025 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1026 | `{` |
|      1 | 1027 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1028 | `	ph7_value_int(pVal,3);` |
|      3 | 1029 | `}` |
|      - | 1030 | `/*` |
|      - | 1031 | ` * PHP_URL_USER.` |
|      - | 1032 | ` * Expand 4` |
|      - | 1033 | ` */` |
|      2 | 1034 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1035 | `{` |
|      1 | 1036 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1037 | `	ph7_value_int(pVal,4);` |
|      3 | 1038 | `}` |
|      - | 1039 | `/*` |
|      - | 1040 | ` * PHP_URL_PASS.` |
|      - | 1041 | ` * Expand 5` |
|      - | 1042 | ` */` |
|      2 | 1043 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1044 | `{` |
|      1 | 1045 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1046 | `	ph7_value_int(pVal,5);` |
|      3 | 1047 | `}` |
|      - | 1048 | `/*` |
|      - | 1049 | ` * PHP_URL_PATH.` |
|      - | 1050 | ` * Expand 6` |
|      - | 1051 | ` */` |
|      2 | 1052 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1053 | `{` |
|      1 | 1054 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1055 | `	ph7_value_int(pVal,6);` |
|      3 | 1056 | `}` |
|      - | 1057 | `/*` |
|      - | 1058 | ` * PHP_URL_QUERY.` |
|      - | 1059 | ` * Expand 7` |
|      - | 1060 | ` */` |
|      2 | 1061 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1062 | `{` |
|      1 | 1063 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1064 | `	ph7_value_int(pVal,7);` |
|      3 | 1065 | `}` |
|      - | 1066 | `/*` |
|      - | 1067 | ` * PHP_URL_FRAGMENT.` |
|      - | 1068 | ` * Expand 8` |
|      - | 1069 | ` */` |
|      2 | 1070 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1071 | `{` |
|      1 | 1072 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1073 | `	ph7_value_int(pVal,8);` |
|      3 | 1074 | `}` |
|      - | 1075 | `/*` |
|      - | 1076 | ` * PHP_QUERY_RFC1738` |
|      - | 1077 | ` * Expand 1` |
|      - | 1078 | ` */` |
|      2 | 1079 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1080 | `{` |
|      1 | 1081 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1082 | `	ph7_value_int(pVal,1);` |
|      3 | 1083 | `}` |
|      - | 1084 | `/*` |
|      - | 1085 | ` * PHP_QUERY_RFC3986` |
|      - | 1086 | ` * Expand 1` |
|      - | 1087 | ` */` |
|      2 | 1088 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1089 | `{` |
|      1 | 1090 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1091 | `	ph7_value_int(pVal,2);` |
|      3 | 1092 | `}` |
|      - | 1093 | `/*` |
|      - | 1094 | ` * FNM_NOESCAPE` |
|      - | 1095 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1096 | ` */` |
|      2 | 1097 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1098 | `{` |
|      1 | 1099 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1100 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1101 | `}` |
|      - | 1102 | `/*` |
|      - | 1103 | ` * FNM_PATHNAME` |
|      - | 1104 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1105 | ` */` |
|      2 | 1106 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1107 | `{` |
|      1 | 1108 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1109 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1110 | `}` |
|      - | 1111 | `/*` |
|      - | 1112 | ` * FNM_PERIOD` |
|      - | 1113 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1114 | ` */` |
|      6 | 1115 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1116 | `{` |
|      3 | 1117 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1118 | `	ph7_value_int(pVal,0x04);` |
|      7 | 1119 | `}` |
|      - | 1120 | `/*` |
|      - | 1121 | ` * FNM_CASEFOLD` |
|      - | 1122 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1123 | ` */` |
|      4 | 1124 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1125 | `{` |
|      2 | 1126 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1127 | `	ph7_value_int(pVal,0x08);` |
|      5 | 1128 | `}` |
|      - | 1129 | `/*` |
|      - | 1130 | ` * PATHINFO_DIRNAME` |
|      - | 1131 | ` *  Expand 1.` |
|      - | 1132 | ` */` |
|      4 | 1133 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1134 | `{` |
|      2 | 1135 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1136 | `	ph7_value_int(pVal,1);` |
|      5 | 1137 | `}` |
|      - | 1138 | `/*` |
|      - | 1139 | ` * PATHINFO_BASENAME` |
|      - | 1140 | ` *  Expand 2.` |
|      - | 1141 | ` */` |
|      4 | 1142 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1143 | `{` |
|      2 | 1144 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1145 | `	ph7_value_int(pVal,2);` |
|      5 | 1146 | `}` |
|      - | 1147 | `/*` |
|      - | 1148 | ` * PATHINFO_EXTENSION` |
|      - | 1149 | ` *  Expand 3.` |
|      - | 1150 | ` */` |
|   6490 | 1151 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1152 | `{` |
|   3245 | 1153 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6495 | 1154 | `	ph7_value_int(pVal,3);` |
|   6495 | 1155 | `}` |
|      - | 1156 | `/*` |
|      - | 1157 | ` * PATHINFO_FILENAME` |
|      - | 1158 | ` *  Expand 4.` |
|      - | 1159 | ` */` |
|   6482 | 1160 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1161 | `{` |
|   3241 | 1162 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6487 | 1163 | `	ph7_value_int(pVal,4);` |
|   6487 | 1164 | `}` |
|      - | 1165 | `/*` |
|      - | 1166 | ` * ASSERT_ACTIVE.` |
|      - | 1167 | ` *  PHP ASSERT_ACTIVE = 1` |
|      - | 1168 | ` */` |
|     14 | 1169 | `static void PH7_ASSERT_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1170 | `{` |
|      7 | 1171 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1172 | `	ph7_value_int(pVal,1); /* PHP ASSERT_ACTIVE = 1 */` |
|     16 | 1173 | `}` |
|      - | 1174 | `/*` |
|      - | 1175 | ` * ASSERT_CALLBACK.` |
|      - | 1176 | ` *  PHP ASSERT_CALLBACK = 2` |
|      - | 1177 | ` */` |
|      6 | 1178 | `static void PH7_ASSERT_CALLBACK_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1179 | `{` |
|      3 | 1180 | `	SXUNUSED(pUserData); /* cc warning */` |
|      8 | 1181 | `	ph7_value_int(pVal,2); /* PHP ASSERT_CALLBACK = 2 */` |
|      8 | 1182 | `}` |
|      - | 1183 | `/*` |
|      - | 1184 | ` * ASSERT_BAIL.` |
|      - | 1185 | ` *  PHP ASSERT_BAIL = 3` |
|      - | 1186 | ` */` |
|     14 | 1187 | `static void PH7_ASSERT_BAIL_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1188 | `{` |
|      7 | 1189 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1190 | `	ph7_value_int(pVal,3); /* PHP ASSERT_BAIL = 3 */` |
|     16 | 1191 | `}` |
|      - | 1192 | `/*` |
|      - | 1193 | ` * ASSERT_WARNING.` |
|      - | 1194 | ` *  PHP ASSERT_WARNING = 4 (deprecated in PHP 8.3)` |
|      - | 1195 | ` */` |
|      4 | 1196 | `static void PH7_ASSERT_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1197 | `{` |
|      2 | 1198 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1199 | `	ph7_value_int(pVal,4); /* PHP ASSERT_WARNING = 4 */` |
|      5 | 1200 | `}` |
|      - | 1201 | `/*` |
|      - | 1202 | ` * ASSERT_EXCEPTION.` |
|      - | 1203 | ` *  PHP ASSERT_EXCEPTION = 5 (deprecated in PHP 8.3)` |
|      - | 1204 | ` */` |
|      4 | 1205 | `static void PH7_ASSERT_EXCEPTION_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1206 | `{` |
|      2 | 1207 | `	SXUNUSED(pUserData); /* cc warning */` |
|      6 | 1208 | `	ph7_value_int(pVal,5); /* PHP ASSERT_EXCEPTION = 5 */` |
|      6 | 1209 | `}` |
|      - | 1210 | `/*` |
|      - | 1211 | ` * ASSERT_QUIET_EVAL.` |
|      - | 1212 | ` *  Removed in PHP 8.0, kept for compatibility.` |
|      - | 1213 | ` */` |
|      2 | 1214 | `static void PH7_ASSERT_QUIET_EVAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1215 | `{` |
|      1 | 1216 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1217 | `	ph7_value_int(pVal,6); /* Arbitrary value, removed in PHP 8 */` |
|      3 | 1218 | `}` |
|      - | 1219 | `/*` |
|      - | 1220 | ` * SEEK_SET.` |
|      - | 1221 | ` *  Expand 0` |
|      - | 1222 | ` */` |
|      2 | 1223 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1224 | `{` |
|      1 | 1225 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1226 | `	ph7_value_int(pVal,0);` |
|      3 | 1227 | `}` |
|      - | 1228 | `/*` |
|      - | 1229 | ` * SEEK_CUR.` |
|      - | 1230 | ` *  Expand 1` |
|      - | 1231 | ` */` |
|      2 | 1232 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1233 | `{` |
|      1 | 1234 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1235 | `	ph7_value_int(pVal,1);` |
|      3 | 1236 | `}` |
|      - | 1237 | `/*` |
|      - | 1238 | ` * SEEK_END.` |
|      - | 1239 | ` *  Expand 2` |
|      - | 1240 | ` */` |
|      2 | 1241 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1242 | `{` |
|      1 | 1243 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1244 | `	ph7_value_int(pVal,2);` |
|      3 | 1245 | `}` |
|      - | 1246 | `/*` |
|      - | 1247 | ` * LOCK_SH.` |
|      - | 1248 | ` *  Expand 2` |
|      - | 1249 | ` */` |
|      2 | 1250 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1251 | `{` |
|      1 | 1252 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1253 | `	ph7_value_int(pVal,1);` |
|      3 | 1254 | `}` |
|      - | 1255 | `/*` |
|      - | 1256 | ` * LOCK_NB.` |
|      - | 1257 | ` *  Expand 5` |
|      - | 1258 | ` */` |
|      2 | 1259 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1260 | `{` |
|      1 | 1261 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1262 | `	ph7_value_int(pVal,5);` |
|      3 | 1263 | `}` |
|      - | 1264 | `/*` |
|      - | 1265 | ` * LOCK_EX.` |
|      - | 1266 | ` *  Expand 0x01 (MUST BE A POWER OF TWO)` |
|      - | 1267 | ` */` |
|      4 | 1268 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1269 | `{` |
|      2 | 1270 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1271 | `	ph7_value_int(pVal,0x01);` |
|      5 | 1272 | `}` |
|      - | 1273 | `/*` |
|      - | 1274 | ` * LOCK_UN.` |
|      - | 1275 | ` *  Expand 0` |
|      - | 1276 | ` */` |
|      4 | 1277 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1278 | `{` |
|      2 | 1279 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1280 | `	ph7_value_int(pVal,0);` |
|      5 | 1281 | `}` |
|      - | 1282 | `/*` |
|      - | 1283 | ` * FILE_USE_INCLUDE_PATH` |
|      - | 1284 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1285 | ` */` |
|      2 | 1286 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1287 | `{` |
|      1 | 1288 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1289 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1290 | `}` |
|      - | 1291 | `/*` |
|      - | 1292 | ` * FILE_IGNORE_NEW_LINES` |
|      - | 1293 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1294 | ` */` |
|      2 | 1295 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1296 | `{` |
|      1 | 1297 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1298 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1299 | `}` |
|      - | 1300 | `/*` |
|      - | 1301 | ` * FILE_SKIP_EMPTY_LINES` |
|      - | 1302 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1303 | ` */` |
|      2 | 1304 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1305 | `{` |
|      1 | 1306 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1307 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1308 | `}` |
|      - | 1309 | `/*` |
|      - | 1310 | ` * FILE_APPEND` |
|      - | 1311 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1312 | ` */` |
|      2 | 1313 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1314 | `{` |
|      1 | 1315 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1316 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1317 | `}` |
|      - | 1318 | `/*` |
|      - | 1319 | ` * SCANDIR_SORT_ASCENDING` |
|      - | 1320 | ` *  Expand 0` |
|      - | 1321 | ` */` |
|   1970 | 1322 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1323 | `{` |
|    985 | 1324 | `	SXUNUSED(pUserData); /* cc warning */` |
|   1975 | 1325 | `	ph7_value_int(pVal,0);` |
|   1975 | 1326 | `}` |
|      - | 1327 | `/*` |
|      - | 1328 | ` * SCANDIR_SORT_DESCENDING` |
|      - | 1329 | ` *  Expand 1` |
|      - | 1330 | ` */` |
|    986 | 1331 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1332 | `{` |
|    493 | 1333 | `	SXUNUSED(pUserData); /* cc warning */` |
|    991 | 1334 | `	ph7_value_int(pVal,1);` |
|    991 | 1335 | `}` |
|      - | 1336 | `/*` |
|      - | 1337 | ` * SCANDIR_SORT_NONE` |
|      - | 1338 | ` *  Expand 2` |
|      - | 1339 | ` */` |
|      2 | 1340 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1341 | `{` |
|      1 | 1342 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1343 | `	ph7_value_int(pVal,2);` |
|      3 | 1344 | `}` |
|      - | 1345 | `/*` |
|      - | 1346 | ` * GLOB_MARK` |
|      - | 1347 | ` *  Expand 0x01 (must be a power of two)` |
|      - | 1348 | ` */` |
|      2 | 1349 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1350 | `{` |
|      1 | 1351 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1352 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1353 | `}` |
|      - | 1354 | `/*` |
|      - | 1355 | ` * GLOB_NOSORT` |
|      - | 1356 | ` *  Expand 0x02 (must be a power of two)` |
|      - | 1357 | ` */` |
|      2 | 1358 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1359 | `{` |
|      1 | 1360 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1361 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1362 | `}` |
|      - | 1363 | `/*` |
|      - | 1364 | ` * GLOB_NOCHECK` |
|      - | 1365 | ` *  Expand 0x04 (must be a power of two)` |
|      - | 1366 | ` */` |
|      2 | 1367 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1368 | `{` |
|      1 | 1369 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1370 | `	ph7_value_int(pVal,0x04);` |
|      3 | 1371 | `}` |
|      - | 1372 | `/*` |
|      - | 1373 | ` * GLOB_NOESCAPE` |
|      - | 1374 | ` *  Expand 0x08 (must be a power of two)` |
|      - | 1375 | ` */` |
|      2 | 1376 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1377 | `{` |
|      1 | 1378 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1379 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1380 | `}` |
|      - | 1381 | `/*` |
|      - | 1382 | ` * GLOB_BRACE` |
|      - | 1383 | ` *  Expand 0x10 (must be a power of two)` |
|      - | 1384 | ` */` |
|      2 | 1385 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1386 | `{` |
|      1 | 1387 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1388 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1389 | `}` |
|      - | 1390 | `/*` |
|      - | 1391 | ` * GLOB_ONLYDIR` |
|      - | 1392 | ` *  Expand 0x20 (must be a power of two)` |
|      - | 1393 | ` */` |
|      2 | 1394 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1395 | `{` |
|      1 | 1396 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1397 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1398 | `}` |
|      - | 1399 | `/*` |
|      - | 1400 | ` * GLOB_ERR` |
|      - | 1401 | ` *  Expand 0x40 (must be a power of two)` |
|      - | 1402 | ` */` |
|      2 | 1403 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1404 | `{` |
|      1 | 1405 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1406 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1407 | `}` |
|      - | 1408 | `/*` |
|      - | 1409 | ` * STDIN` |
|      - | 1410 | ` *  Expand the STDIN handle as a resource.` |
|      - | 1411 | ` */` |
|      2 | 1412 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1413 | `{` |
|      3 | 1414 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1415 | `	void *pResource;` |
|      3 | 1416 | `	pResource = PH7_ExportStdin(pVm);` |
|      3 | 1417 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1418 | `}` |
|      - | 1419 | `/*` |
|      - | 1420 | ` * STDOUT` |
|      - | 1421 | ` *   Expand the STDOUT handle as a resource.` |
|      - | 1422 | ` */` |
|      2 | 1423 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1424 | `{` |
|      3 | 1425 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1426 | `	void *pResource;` |
|      3 | 1427 | `	pResource = PH7_ExportStdout(pVm);` |
|      3 | 1428 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1429 | `}` |
|      - | 1430 | `/*` |
|      - | 1431 | ` * STDERR` |
|      - | 1432 | ` *  Expand the STDERR handle as a resource.` |
|      - | 1433 | ` */` |
|      2 | 1434 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1435 | `{` |
|      3 | 1436 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1437 | `	void *pResource;` |
|      3 | 1438 | `	pResource = PH7_ExportStderr(pVm);` |
|      3 | 1439 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1440 | `}` |
|      - | 1441 | `/*` |
|      - | 1442 | ` * INI_SCANNER_NORMAL` |
|      - | 1443 | ` *   Expand 1` |
|      - | 1444 | ` */` |
|      2 | 1445 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1446 | `{` |
|      1 | 1447 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1448 | `	ph7_value_int(pVal,1);` |
|      3 | 1449 | `}` |
|      - | 1450 | `/*` |
|      - | 1451 | ` * INI_SCANNER_RAW` |
|      - | 1452 | ` *   Expand 2` |
|      - | 1453 | ` */` |
|      2 | 1454 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1455 | `{` |
|      1 | 1456 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1457 | `	ph7_value_int(pVal,2);` |
|      3 | 1458 | `}` |
|      - | 1459 | `/*` |
|      - | 1460 | ` * EXTR_OVERWRITE` |
|      - | 1461 | ` *   Expand 0x01 (Must be a power of two)` |
|      - | 1462 | ` */` |
|      2 | 1463 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1464 | `{` |
|      1 | 1465 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1466 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1467 | `}` |
|      - | 1468 | `/*` |
|      - | 1469 | ` * EXTR_SKIP` |
|      - | 1470 | ` *   Expand 0x02 (Must be a power of two)` |
|      - | 1471 | ` */` |
|      2 | 1472 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1473 | `{` |
|      1 | 1474 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1475 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1476 | `}` |
|      - | 1477 | `/*` |
|      - | 1478 | ` * EXTR_PREFIX_SAME` |
|      - | 1479 | ` *   Expand 0x04 (Must be a power of two)` |
|      - | 1480 | ` */` |
|      2 | 1481 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1482 | `{` |
|      1 | 1483 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1484 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1485 | `}` |
|      - | 1486 | `/*` |
|      - | 1487 | ` * EXTR_PREFIX_ALL` |
|      - | 1488 | ` *   Expand 0x08 (Must be a power of two)` |
|      - | 1489 | ` */` |
|      2 | 1490 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1491 | `{` |
|      1 | 1492 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1493 | `	ph7_value_int(pVal,0x8);` |
|      3 | 1494 | `}` |
|      - | 1495 | `/*` |
|      - | 1496 | ` * EXTR_PREFIX_INVALID` |
|      - | 1497 | ` *   Expand 0x10 (Must be a power of two)` |
|      - | 1498 | ` */` |
|      2 | 1499 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1500 | `{` |
|      1 | 1501 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1502 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1503 | `}` |
|      - | 1504 | `/*` |
|      - | 1505 | ` * EXTR_IF_EXISTS` |
|      - | 1506 | ` *   Expand 0x20 (Must be a power of two)` |
|      - | 1507 | ` */` |
|      2 | 1508 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1509 | `{` |
|      1 | 1510 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1511 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1512 | `}` |
|      - | 1513 | `/*` |
|      - | 1514 | ` * EXTR_PREFIX_IF_EXISTS` |
|      - | 1515 | ` *   Expand 0x40 (Must be a power of two)` |
|      - | 1516 | ` */` |
|      2 | 1517 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1518 | `{` |
|      1 | 1519 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1520 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1521 | `}` |
|      - | 1522 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1523 | `/*` |
|      - | 1524 | ` * XML_ERROR_NONE` |
|      - | 1525 | ` *   Expand the value of SXML_ERROR_NO_MEMORY defined in ph7Int.h` |
|      - | 1526 | ` */` |
|      2 | 1527 | `static void PH7_XML_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1528 | `{` |
|      1 | 1529 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1530 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1531 | `}` |
|      - | 1532 | `/*` |
|      - | 1533 | ` * XML_ERROR_NO_MEMORY` |
|      - | 1534 | ` *   Expand the value of SXML_ERROR_NONE defined in ph7Int.h` |
|      - | 1535 | ` */` |
|      2 | 1536 | `static void PH7_XML_ERROR_NO_MEMORY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1537 | `{` |
|      1 | 1538 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1539 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1540 | `}` |
|      - | 1541 | `/*` |
|      - | 1542 | ` * XML_ERROR_SYNTAX` |
|      - | 1543 | ` *   Expand the value of SXML_ERROR_SYNTAX defined in ph7Int.h` |
|      - | 1544 | ` */` |
|      2 | 1545 | `static void PH7_XML_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1546 | `{` |
|      1 | 1547 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1548 | `	ph7_value_int(pVal,SXML_ERROR_SYNTAX);` |
|      3 | 1549 | `}` |
|      - | 1550 | `/*` |
|      - | 1551 | ` * XML_ERROR_NO_ELEMENTS` |
|      - | 1552 | ` *   Expand the value of SXML_ERROR_NO_ELEMENTS defined in ph7Int.h` |
|      - | 1553 | ` */` |
|      2 | 1554 | `static void PH7_XML_ERROR_NO_ELEMENTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1555 | `{` |
|      1 | 1556 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1557 | `	ph7_value_int(pVal,SXML_ERROR_NO_ELEMENTS);` |
|      3 | 1558 | `}` |
|      - | 1559 | `/*` |
|      - | 1560 | ` * XML_ERROR_INVALID_TOKEN` |
|      - | 1561 | ` *   Expand the value of SXML_ERROR_INVALID_TOKEN defined in ph7Int.h` |
|      - | 1562 | ` */` |
|      2 | 1563 | `static void PH7_XML_ERROR_INVALID_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1564 | `{` |
|      1 | 1565 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1566 | `	ph7_value_int(pVal,SXML_ERROR_INVALID_TOKEN);` |
|      3 | 1567 | `}` |
|      - | 1568 | `/*` |
|      - | 1569 | ` * XML_ERROR_UNCLOSED_TOKEN` |
|      - | 1570 | ` *   Expand the value of SXML_ERROR_UNCLOSED_TOKEN defined in ph7Int.h` |
|      - | 1571 | ` */` |
|      2 | 1572 | `static void PH7_XML_ERROR_UNCLOSED_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1573 | `{` |
|      1 | 1574 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1575 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_TOKEN);` |
|      3 | 1576 | `}` |
|      - | 1577 | `/*` |
|      - | 1578 | ` * XML_ERROR_PARTIAL_CHAR` |
|      - | 1579 | ` *   Expand the value of SXML_ERROR_PARTIAL_CHAR defined in ph7Int.h` |
|      - | 1580 | ` */` |
|      2 | 1581 | `static void PH7_XML_ERROR_PARTIAL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1582 | `{` |
|      1 | 1583 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1584 | `	ph7_value_int(pVal,SXML_ERROR_PARTIAL_CHAR);` |
|      3 | 1585 | `}` |
|      - | 1586 | `/*` |
|      - | 1587 | ` * XML_ERROR_TAG_MISMATCH` |
|      - | 1588 | ` *   Expand the value of SXML_ERROR_TAG_MISMATCH defined in ph7Int.h` |
|      - | 1589 | ` */` |
|      2 | 1590 | `static void PH7_XML_ERROR_TAG_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1591 | `{` |
|      1 | 1592 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1593 | `	ph7_value_int(pVal,SXML_ERROR_TAG_MISMATCH);` |
|      3 | 1594 | `}` |
|      - | 1595 | `/*` |
|      - | 1596 | ` * XML_ERROR_DUPLICATE_ATTRIBUTE` |
|      - | 1597 | ` *   Expand the value of SXML_ERROR_DUPLICATE_ATTRIBUTE defined in ph7Int.h` |
|      - | 1598 | ` */` |
|      2 | 1599 | `static void PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1600 | `{` |
|      1 | 1601 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1602 | `	ph7_value_int(pVal,SXML_ERROR_DUPLICATE_ATTRIBUTE);` |
|      3 | 1603 | `}` |
|      - | 1604 | `/*` |
|      - | 1605 | ` * XML_ERROR_JUNK_AFTER_DOC_ELEMENT` |
|      - | 1606 | ` *   Expand the value of SXML_ERROR_JUNK_AFTER_DOC_ELEMENT defined in ph7Int.h` |
|      - | 1607 | ` */` |
|      2 | 1608 | `static void PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1609 | `{` |
|      1 | 1610 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1611 | `	ph7_value_int(pVal,SXML_ERROR_JUNK_AFTER_DOC_ELEMENT);` |
|      3 | 1612 | `}` |
|      - | 1613 | `/*` |
|      - | 1614 | ` * XML_ERROR_PARAM_ENTITY_REF` |
|      - | 1615 | ` *   Expand the value of SXML_ERROR_PARAM_ENTITY_REF defined in ph7Int.h` |
|      - | 1616 | ` */` |
|      2 | 1617 | `static void PH7_XML_ERROR_PARAM_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1618 | `{` |
|      1 | 1619 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1620 | `	ph7_value_int(pVal,SXML_ERROR_PARAM_ENTITY_REF);` |
|      3 | 1621 | `}` |
|      - | 1622 | `/*` |
|      - | 1623 | ` * XML_ERROR_UNDEFINED_ENTITY` |
|      - | 1624 | ` *   Expand the value of SXML_ERROR_UNDEFINED_ENTITY defined in ph7Int.h` |
|      - | 1625 | ` */` |
|      2 | 1626 | `static void PH7_XML_ERROR_UNDEFINED_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1627 | `{` |
|      1 | 1628 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1629 | `	ph7_value_int(pVal,SXML_ERROR_UNDEFINED_ENTITY);` |
|      3 | 1630 | `}` |
|      - | 1631 | `/*` |
|      - | 1632 | ` * XML_ERROR_RECURSIVE_ENTITY_REF` |
|      - | 1633 | ` *   Expand the value of SXML_ERROR_RECURSIVE_ENTITY_REF defined in ph7Int.h` |
|      - | 1634 | ` */` |
|      2 | 1635 | `static void PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1636 | `{` |
|      1 | 1637 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1638 | `	ph7_value_int(pVal,SXML_ERROR_RECURSIVE_ENTITY_REF);` |
|      3 | 1639 | `}` |
|      - | 1640 | `/*` |
|      - | 1641 | ` * XML_ERROR_ASYNC_ENTITY` |
|      - | 1642 | ` *   Expand the value of SXML_ERROR_ASYNC_ENTITY defined in ph7Int.h` |
|      - | 1643 | ` */` |
|      2 | 1644 | `static void PH7_XML_ERROR_ASYNC_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1645 | `{` |
|      1 | 1646 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1647 | `	ph7_value_int(pVal,SXML_ERROR_ASYNC_ENTITY);` |
|      3 | 1648 | `}` |
|      - | 1649 | `/*` |
|      - | 1650 | ` * XML_ERROR_BAD_CHAR_REF` |
|      - | 1651 | ` *   Expand the value of SXML_ERROR_BAD_CHAR_REF defined in ph7Int.h` |
|      - | 1652 | ` */` |
|      2 | 1653 | `static void PH7_XML_ERROR_BAD_CHAR_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1654 | `{` |
|      1 | 1655 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1656 | `	ph7_value_int(pVal,SXML_ERROR_BAD_CHAR_REF);` |
|      3 | 1657 | `}` |
|      - | 1658 | `/*` |
|      - | 1659 | ` * XML_ERROR_BINARY_ENTITY_REF` |
|      - | 1660 | ` *   Expand the value of SXML_ERROR_BINARY_ENTITY_REF defined in ph7Int.h` |
|      - | 1661 | ` */` |
|      2 | 1662 | `static void PH7_XML_ERROR_BINARY_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1663 | `{` |
|      1 | 1664 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1665 | `	ph7_value_int(pVal,SXML_ERROR_BINARY_ENTITY_REF);` |
|      3 | 1666 | `}` |
|      - | 1667 | `/*` |
|      - | 1668 | ` * XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF` |
|      - | 1669 | ` *   Expand the value of SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF defined in ph7Int.h` |
|      - | 1670 | ` */` |
|      2 | 1671 | `static void PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1672 | `{` |
|      1 | 1673 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1674 | `	ph7_value_int(pVal,SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF);` |
|      3 | 1675 | `}` |
|      - | 1676 | `/*` |
|      - | 1677 | ` * XML_ERROR_MISPLACED_XML_PI` |
|      - | 1678 | ` *   Expand the value of SXML_ERROR_MISPLACED_XML_PI defined in ph7Int.h` |
|      - | 1679 | ` */` |
|      2 | 1680 | `static void PH7_XML_ERROR_MISPLACED_XML_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1681 | `{` |
|      1 | 1682 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1683 | `	ph7_value_int(pVal,SXML_ERROR_MISPLACED_XML_PI);` |
|      3 | 1684 | `}` |
|      - | 1685 | `/*` |
|      - | 1686 | ` * XML_ERROR_UNKNOWN_ENCODING` |
|      - | 1687 | ` *   Expand the value of SXML_ERROR_UNKNOWN_ENCODING defined in ph7Int.h` |
|      - | 1688 | ` */` |
|      2 | 1689 | `static void PH7_XML_ERROR_UNKNOWN_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1690 | `{` |
|      1 | 1691 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1692 | `	ph7_value_int(pVal,SXML_ERROR_UNKNOWN_ENCODING);` |
|      3 | 1693 | `}` |
|      - | 1694 | `/*` |
|      - | 1695 | ` * XML_ERROR_INCORRECT_ENCODING` |
|      - | 1696 | ` *   Expand the value of SXML_ERROR_INCORRECT_ENCODING defined in ph7Int.h` |
|      - | 1697 | ` */` |
|      2 | 1698 | `static void PH7_XML_ERROR_INCORRECT_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1699 | `{` |
|      1 | 1700 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1701 | `	ph7_value_int(pVal,SXML_ERROR_INCORRECT_ENCODING);` |
|      3 | 1702 | `}` |
|      - | 1703 | `/*` |
|      - | 1704 | ` * XML_ERROR_UNCLOSED_CDATA_SECTION` |
|      - | 1705 | ` *   Expand the value of SXML_ERROR_UNCLOSED_CDATA_SECTION defined in ph7Int.h` |
|      - | 1706 | ` */` |
|      2 | 1707 | `static void PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1708 | `{` |
|      1 | 1709 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1710 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_CDATA_SECTION);` |
|      3 | 1711 | `}` |
|      - | 1712 | `/*` |
|      - | 1713 | ` * XML_ERROR_EXTERNAL_ENTITY_HANDLING` |
|      - | 1714 | ` *   Expand the value of SXML_ERROR_EXTERNAL_ENTITY_HANDLING defined in ph7Int.h` |
|      - | 1715 | ` */` |
|      2 | 1716 | `static void PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1717 | `{` |
|      1 | 1718 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1719 | `	ph7_value_int(pVal,SXML_ERROR_EXTERNAL_ENTITY_HANDLING);` |
|      3 | 1720 | `}` |
|      - | 1721 | `/*` |
|      - | 1722 | ` * XML_OPTION_CASE_FOLDING` |
|      - | 1723 | ` *   Expand the value of SXML_OPTION_CASE_FOLDING defined in ph7Int.h.` |
|      - | 1724 | ` */` |
|      2 | 1725 | `static void PH7_XML_OPTION_CASE_FOLDING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1726 | `{` |
|      1 | 1727 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1728 | `	ph7_value_int(pVal,SXML_OPTION_CASE_FOLDING);` |
|      3 | 1729 | `}` |
|      - | 1730 | `/*` |
|      - | 1731 | ` * XML_OPTION_TARGET_ENCODING` |
|      - | 1732 | ` *   Expand the value of SXML_OPTION_TARGET_ENCODING defined in ph7Int.h.` |
|      - | 1733 | ` */` |
|      4 | 1734 | `static void PH7_XML_OPTION_TARGET_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1735 | `{` |
|      2 | 1736 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1737 | `	ph7_value_int(pVal,SXML_OPTION_TARGET_ENCODING);` |
|      5 | 1738 | `}` |
|      - | 1739 | `/*` |
|      - | 1740 | ` * XML_OPTION_SKIP_TAGSTART` |
|      - | 1741 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1742 | ` */` |
|      2 | 1743 | `static void PH7_XML_OPTION_SKIP_TAGSTART_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1744 | `{` |
|      1 | 1745 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1746 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_TAGSTART);` |
|      3 | 1747 | `}` |
|      - | 1748 | `/*` |
|      - | 1749 | ` * XML_OPTION_SKIP_WHITE` |
|      - | 1750 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1751 | ` */` |
|      4 | 1752 | `static void PH7_XML_OPTION_SKIP_WHITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1753 | `{` |
|      2 | 1754 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1755 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_WHITE);` |
|      5 | 1756 | `}` |
|      - | 1757 | `/*` |
|      - | 1758 | ` * XML_SAX_IMPL.` |
|      - | 1759 | ` *   Expand the name of the underlying XML engine.` |
|      - | 1760 | ` */` |
|      2 | 1761 | `static void PH7_XML_SAX_IMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1762 | `{` |
|      1 | 1763 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1764 | `	ph7_value_string(pVal,"Symisc XML engine",(int)sizeof("Symisc XML engine")-1);` |
|      3 | 1765 | `}` |
|      - | 1766 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1767 | `/*` |
|      - | 1768 | ` * JSON_HEX_TAG.` |
|      - | 1769 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|      - | 1770 | ` */` |
|      2 | 1771 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1772 | `{` |
|      1 | 1773 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1774 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      3 | 1775 | `}` |
|      - | 1776 | `/*` |
|      - | 1777 | ` * JSON_HEX_AMP.` |
|      - | 1778 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|      - | 1779 | ` */` |
|      2 | 1780 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1781 | `{` |
|      1 | 1782 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1783 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      3 | 1784 | `}` |
|      - | 1785 | `/*` |
|      - | 1786 | ` * JSON_HEX_APOS.` |
|      - | 1787 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|      - | 1788 | ` */` |
|      2 | 1789 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1790 | `{` |
|      1 | 1791 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1792 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      3 | 1793 | `}` |
|      - | 1794 | `/*` |
|      - | 1795 | ` * JSON_HEX_QUOT.` |
|      - | 1796 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|      - | 1797 | ` */` |
|      2 | 1798 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1799 | `{` |
|      1 | 1800 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1801 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      3 | 1802 | `}` |
|      - | 1803 | `/*` |
|      - | 1804 | ` * JSON_FORCE_OBJECT.` |
|      - | 1805 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|      - | 1806 | ` */` |
|      4 | 1807 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1808 | `{` |
|      2 | 1809 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1810 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      5 | 1811 | `}` |
|      - | 1812 | `/*` |
|      - | 1813 | ` * JSON_NUMERIC_CHECK.` |
|      - | 1814 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|      - | 1815 | ` */` |
|      4 | 1816 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1817 | `{` |
|      2 | 1818 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1819 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      5 | 1820 | `}` |
|      - | 1821 | `/*` |
|      - | 1822 | ` * JSON_BIGINT_AS_STRING.` |
|      - | 1823 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|      - | 1824 | ` */` |
|      2 | 1825 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1826 | `{` |
|      1 | 1827 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1828 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      3 | 1829 | `}` |
|      - | 1830 | `/*` |
|      - | 1831 | ` * JSON_PRETTY_PRINT.` |
|      - | 1832 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|      - | 1833 | ` */` |
|      2 | 1834 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1835 | `{` |
|      1 | 1836 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1837 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      3 | 1838 | `}` |
|      - | 1839 | `/*` |
|      - | 1840 | ` * JSON_UNESCAPED_SLASHES.` |
|      - | 1841 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|      - | 1842 | ` */` |
|      4 | 1843 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1844 | `{` |
|      2 | 1845 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1846 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      5 | 1847 | `}` |
|      - | 1848 | `/*` |
|      - | 1849 | ` * JSON_UNESCAPED_UNICODE.` |
|      - | 1850 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|      - | 1851 | ` */` |
|      2 | 1852 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1853 | `{` |
|      1 | 1854 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1855 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|      3 | 1856 | `}` |
|      - | 1857 | `/*` |
|      - | 1858 | ` * JSON_ERROR_NONE.` |
|      - | 1859 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|      - | 1860 | ` */` |
|      4 | 1861 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1862 | `{` |
|      2 | 1863 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1864 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      5 | 1865 | `}` |
|      - | 1866 | `/*` |
|      - | 1867 | ` * JSON_ERROR_DEPTH.` |
|      - | 1868 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|      - | 1869 | ` */` |
|      2 | 1870 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1871 | `{` |
|      1 | 1872 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1873 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      3 | 1874 | `}` |
|      - | 1875 | `/*` |
|      - | 1876 | ` * JSON_ERROR_STATE_MISMATCH.` |
|      - | 1877 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|      - | 1878 | ` */` |
|      2 | 1879 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1880 | `{` |
|      1 | 1881 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1882 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      3 | 1883 | `}` |
|      - | 1884 | `/*` |
|      - | 1885 | ` * JSON_ERROR_CTRL_CHAR.` |
|      - | 1886 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|      - | 1887 | ` */` |
|      2 | 1888 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1889 | `{` |
|      1 | 1890 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1891 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      3 | 1892 | `}` |
|      - | 1893 | `/*` |
|      - | 1894 | ` * JSON_ERROR_SYNTAX.` |
|      - | 1895 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|      - | 1896 | ` */` |
|      4 | 1897 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1898 | `{` |
|      2 | 1899 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1900 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      5 | 1901 | `}` |
|      - | 1902 | `/*` |
|      - | 1903 | ` * JSON_ERROR_UTF8.` |
|      - | 1904 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|      - | 1905 | ` */` |
|      2 | 1906 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1907 | `{` |
|      1 | 1908 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1909 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      3 | 1910 | `}` |
|      - | 1911 | `/*` |
|      - | 1912 | ` * JSON_ERROR_NON_BACKED_ENUM.` |
|      - | 1913 | ` *   Expand the value of JSON_ERROR_NON_BACKED_ENUM defined in ph7Int.h (php 8.1).` |
|      - | 1914 | ` */` |
|    ! 0 | 1915 | `static void PH7_JSON_ERROR_NON_BACKED_ENUM_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 1916 | `{` |
|    ! 0 | 1917 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 1918 | `	ph7_value_int(pVal,JSON_ERROR_NON_BACKED_ENUM);` |
|    ! 0 | 1919 | `}` |
|      - | 1920 | `/*` |
|      - | 1921 | ` * static` |
|      - | 1922 | ` *  Expand the name of the current class. 'static' otherwise.` |
|      - | 1923 | ` */` |
|      6 | 1924 | `static void PH7_static_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1925 | `{` |
|      7 | 1926 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1927 | `	ph7_class *pClass;` |
|      - | 1928 | `	/* Extract the target class if available */` |
|      7 | 1929 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      7 | 1930 | `	if( pClass ){` |
|      3 | 1931 | `		SyString *pName = &pClass->sName;` |
|      - | 1932 | `		/* Expand class name */` |
|      3 | 1933 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      2 | 1934 | `	}else{` |
|      - | 1935 | `		/* Expand 'static' */` |
|      5 | 1936 | `		ph7_value_string(pVal,"static",sizeof("static")-1);` |
|      - | 1937 | `	}` |
|      7 | 1938 | `}` |
|      - | 1939 | `/*` |
|      - | 1940 | ` * self` |
|      - | 1941 | ` * __CLASS__` |
|      - | 1942 | ` *  Expand the name of the current class. NULL otherwise.` |
|      - | 1943 | ` */` |
|      2 | 1944 | `static void PH7_self_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1945 | `{` |
|      3 | 1946 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1947 | `	ph7_class *pClass;` |
|      - | 1948 |  |
|      - | 1949 | `	/* Get the declaring class of the current method */` |
|      3 | 1950 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      3 | 1951 | `	if( pClass == 0 ){` |
|      - | 1952 | `		/* Not in a method, fall back to runtime class */` |
|      3 | 1953 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      1 | 1954 | `	}` |
|      - | 1955 |  |
|      3 | 1956 | `	if( pClass ){` |
|    ! 0 | 1957 | `		SyString *pName = &pClass->sName;` |
|      - | 1958 | `		/* Expand class name */` |
|    ! 0 | 1959 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|    ! 0 | 1960 | `	}else{` |
|      - | 1961 | `		/* Expand null */` |
|      3 | 1962 | `		ph7_value_null(pVal);` |
|      - | 1963 | `	}` |
|      3 | 1964 | `}` |
|      - | 1965 | `/* parent` |
|      - | 1966 | ` *  Expand the name of the parent class. NULL otherwise.` |
|      - | 1967 | ` */` |
|      2 | 1968 | `static void PH7_parent_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1969 | `{` |
|      3 | 1970 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1971 | `	ph7_class *pClass;` |
|      - | 1972 |  |
|      - | 1973 | `	/* Get the declaring class, then its parent */` |
|      3 | 1974 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      3 | 1975 | `	if( pClass && pClass->pBase ){` |
|    ! 0 | 1976 | `		SyString *pName = &pClass->pBase->sName;` |
|      - | 1977 | `		/* Expand parent class name */` |
|    ! 0 | 1978 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|    ! 0 | 1979 | `	}else{` |
|      - | 1980 | `		/* Expand null */` |
|      3 | 1981 | `		ph7_value_null(pVal);` |
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
|      - | 2069 | `	{"PASSWORD_BCRYPT",      PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2070 | `	{"PASSWORD_DEFAULT",     PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2071 | `	{"PASSWORD_BCRYPT_DEFAULT_COST", PH7_PASSWORD_COST_Const },` |
|      - | 2072 | `	{"FILTER_DEFAULT",              PH7_FILTER_DEFAULT_Const },` |
|      - | 2073 | `	{"FILTER_UNSAFE_RAW",           PH7_FILTER_UNSAFE_RAW_Const },` |
|      - | 2074 | `	{"FILTER_VALIDATE_INT",         PH7_FILTER_VALIDATE_INT_Const },` |
|      - | 2075 | `	{"FILTER_VALIDATE_BOOLEAN",     PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2076 | `	{"FILTER_VALIDATE_BOOL",        PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2077 | `	{"FILTER_VALIDATE_FLOAT",       PH7_FILTER_VALIDATE_FLOAT_Const },` |
|      - | 2078 | `	{"FILTER_VALIDATE_REGEXP",      PH7_FILTER_VALIDATE_REGEXP_Const },` |
|      - | 2079 | `	{"FILTER_VALIDATE_DOMAIN",      PH7_FILTER_VALIDATE_DOMAIN_Const },` |
|      - | 2080 | `	{"FILTER_VALIDATE_URL",         PH7_FILTER_VALIDATE_URL_Const },` |
|      - | 2081 | `	{"FILTER_VALIDATE_EMAIL",       PH7_FILTER_VALIDATE_EMAIL_Const },` |
|      - | 2082 | `	{"FILTER_VALIDATE_IP",          PH7_FILTER_VALIDATE_IP_Const },` |
|      - | 2083 | `	{"FILTER_VALIDATE_MAC",         PH7_FILTER_VALIDATE_MAC_Const },` |
|      - | 2084 | `	{"FILTER_SANITIZE_NUMBER_INT",  PH7_FILTER_SANITIZE_NUMBER_INT_Const },` |
|      - | 2085 | `	{"FILTER_SANITIZE_NUMBER_FLOAT",PH7_FILTER_SANITIZE_NUMBER_FLOAT_Const },` |
|      - | 2086 | `	{"FILTER_SANITIZE_SPECIAL_CHARS",PH7_FILTER_SANITIZE_SPECIAL_CHARS_Const },` |
|      - | 2087 | `	{"FILTER_SANITIZE_FULL_SPECIAL_CHARS",PH7_FILTER_SANITIZE_FULL_SPECIAL_CHARS_Const },` |
|      - | 2088 | `	{"FILTER_SANITIZE_EMAIL",       PH7_FILTER_SANITIZE_EMAIL_Const },` |
|      - | 2089 | `	{"FILTER_SANITIZE_URL",         PH7_FILTER_SANITIZE_URL_Const },` |
|      - | 2090 | `	{"FILTER_FLAG_ALLOW_OCTAL",     PH7_FILTER_FLAG_ALLOW_OCTAL_Const },` |
|      - | 2091 | `	{"FILTER_FLAG_ALLOW_HEX",       PH7_FILTER_FLAG_ALLOW_HEX_Const },` |
|      - | 2092 | `	{"FILTER_FLAG_STRIP_LOW",       PH7_FILTER_FLAG_STRIP_LOW_Const },` |
|      - | 2093 | `	{"FILTER_FLAG_STRIP_HIGH",      PH7_FILTER_FLAG_STRIP_HIGH_Const },` |
|      - | 2094 | `	{"FILTER_FLAG_ENCODE_LOW",      PH7_FILTER_FLAG_ENCODE_LOW_Const },` |
|      - | 2095 | `	{"FILTER_FLAG_ENCODE_HIGH",     PH7_FILTER_FLAG_ENCODE_HIGH_Const },` |
|      - | 2096 | `	{"FILTER_FLAG_ENCODE_AMP",      PH7_FILTER_FLAG_ENCODE_AMP_Const },` |
|      - | 2097 | `	{"FILTER_FLAG_NO_ENCODE_QUOTES",PH7_FILTER_FLAG_NO_ENCODE_QUOTES_Const },` |
|      - | 2098 | `	{"FILTER_FLAG_STRIP_BACKTICK",  PH7_FILTER_FLAG_STRIP_BACKTICK_Const },` |
|      - | 2099 | `	{"FILTER_FLAG_ALLOW_FRACTION",  PH7_FILTER_FLAG_ALLOW_FRACTION_Const },` |
|      - | 2100 | `	{"FILTER_FLAG_ALLOW_THOUSAND",  PH7_FILTER_FLAG_ALLOW_THOUSAND_Const },` |
|      - | 2101 | `	{"FILTER_FLAG_ALLOW_SCIENTIFIC",PH7_FILTER_FLAG_ALLOW_SCIENTIFIC_Const },` |
|      - | 2102 | `	{"FILTER_FLAG_IPV4",            PH7_FILTER_FLAG_IPV4_Const },` |
|      - | 2103 | `	{"FILTER_FLAG_IPV6",            PH7_FILTER_FLAG_IPV6_Const },` |
|      - | 2104 | `	{"FILTER_NULL_ON_FAILURE",      PH7_FILTER_NULL_ON_FAILURE_Const },` |
|      - | 2105 | `	{"INPUT_POST",                  PH7_INPUT_POST_Const },` |
|      - | 2106 | `	{"INPUT_GET",                   PH7_INPUT_GET_Const },` |
|      - | 2107 | `	{"INPUT_COOKIE",                PH7_INPUT_COOKIE_Const },` |
|      - | 2108 | `	{"INPUT_ENV",                   PH7_INPUT_ENV_Const },` |
|      - | 2109 | `	{"INPUT_SERVER",                PH7_INPUT_SERVER_Const },` |
|      - | 2110 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|      - | 2111 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|      - | 2112 | `	{"PHP_INT_MIN",          PH7_INTMIN_Const   },` |
|      - | 2113 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|      - | 2114 | `	{"PHP_FLOAT_EPSILON",    PH7_FLOATEPSILON_Const },` |
|      - | 2115 | `	{"PHP_FLOAT_MAX",        PH7_FLOATMAX_Const },` |
|      - | 2116 | `	{"PHP_FLOAT_MIN",        PH7_FLOATMIN_Const },` |
|      - | 2117 | `	{"PHP_FLOAT_DIG",        PH7_FLOATDIG_Const },` |
|      - | 2118 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|      - | 2119 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|      - | 2120 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|      - | 2121 | `	{"__TIME__",             PH7_TIME_Const     },` |
|      - | 2122 | `	{"__DATE__",             PH7_DATE_Const     },` |
|      - | 2123 | `	{"__FILE__",             PH7_FILE_Const     },` |
|      - | 2124 | `	{"__DIR__",              PH7_DIR_Const      },` |
|      - | 2125 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|      - | 2126 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|      - | 2127 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|      - | 2128 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|      - | 2129 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|      - | 2130 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|      - | 2131 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|      - | 2132 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|      - | 2133 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|      - | 2134 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|      - | 2135 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|      - | 2136 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|      - | 2137 | `	{"E_STRICT",             PH7_E_STRICT_Const        },` |
|      - | 2138 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|      - | 2139 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|      - | 2140 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|      - | 2141 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|      - | 2142 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|      - | 2143 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|      - | 2144 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|      - | 2145 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|      - | 2146 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|      - | 2147 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|      - | 2148 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|      - | 2149 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|      - | 2150 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|      - | 2151 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|      - | 2152 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|      - | 2153 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|      - | 2154 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|      - | 2155 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|      - | 2156 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|      - | 2157 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|      - | 2158 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|      - | 2159 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|      - | 2160 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 2161 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|      - | 2162 | `	{"M_E",                  PH7_M_E_Const          },` |
|      - | 2163 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|      - | 2164 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|      - | 2165 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|      - | 2166 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|      - | 2167 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|      - | 2168 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|      - | 2169 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|      - | 2170 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|      - | 2171 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|      - | 2172 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|      - | 2173 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|      - | 2174 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|      - | 2175 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|      - | 2176 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|      - | 2177 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|      - | 2178 | `	{"NAN",                  PH7_NAN_Const          },` |
|      - | 2179 | `	{"INF",                  PH7_INF_Const          },` |
|      - | 2180 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 2181 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|      - | 2182 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|      - | 2183 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|      - | 2184 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|      - | 2185 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|      - | 2186 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|      - | 2187 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|      - | 2188 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|      - | 2189 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|      - | 2190 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|      - | 2191 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|      - | 2192 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|      - | 2193 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|      - | 2194 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|      - | 2195 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|      - | 2196 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|      - | 2197 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|      - | 2198 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|      - | 2199 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|      - | 2200 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|      - | 2201 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|      - | 2202 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|      - | 2203 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|      - | 2204 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|      - | 2205 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|      - | 2206 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|      - | 2207 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|      - | 2208 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|      - | 2209 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|      - | 2210 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|      - | 2211 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|      - | 2212 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|      - | 2213 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|      - | 2214 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|      - | 2215 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|      - | 2216 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|      - | 2217 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|      - | 2218 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|      - | 2219 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|      - | 2220 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|      - | 2221 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|      - | 2222 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|      - | 2223 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|      - | 2224 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|      - | 2225 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|      - | 2226 | `	{"ASSERT_ACTIVE",        PH7_ASSERT_ACTIVE_Const     },` |
|      - | 2227 | `	{"ASSERT_CALLBACK",      PH7_ASSERT_CALLBACK_Const   },` |
|      - | 2228 | `	{"ASSERT_BAIL",          PH7_ASSERT_BAIL_Const       },` |
|      - | 2229 | `	{"ASSERT_WARNING",       PH7_ASSERT_WARNING_Const    },` |
|      - | 2230 | `	{"ASSERT_EXCEPTION",     PH7_ASSERT_EXCEPTION_Const  },` |
|      - | 2231 | `	{"ASSERT_QUIET_EVAL",    PH7_ASSERT_QUIET_EVAL_Const },` |
|      - | 2232 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|      - | 2233 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|      - | 2234 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|      - | 2235 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|      - | 2236 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|      - | 2237 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|      - | 2238 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|      - | 2239 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|      - | 2240 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|      - | 2241 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|      - | 2242 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|      - | 2243 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|      - | 2244 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|      - | 2245 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|      - | 2246 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|      - | 2247 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|      - | 2248 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|      - | 2249 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|      - | 2250 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|      - | 2251 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|      - | 2252 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|      - | 2253 | `	{"STDIN",                PH7_STDIN_Const        },` |
|      - | 2254 | `	{"stdin",                PH7_STDIN_Const        },` |
|      - | 2255 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|      - | 2256 | `	{"stdout",               PH7_STDOUT_Const       },` |
|      - | 2257 | `	{"STDERR",               PH7_STDERR_Const       },` |
|      - | 2258 | `	{"stderr",               PH7_STDERR_Const       },` |
|      - | 2259 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|      - | 2260 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|      - | 2261 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|      - | 2262 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|      - | 2263 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|      - | 2264 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|      - | 2265 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|      - | 2266 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|      - | 2267 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|      - | 2268 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2269 | `	{"XML_ERROR_NONE",       PH7_XML_ERROR_NONE_Const},` |
|      - | 2270 | `	{"XML_ERROR_NO_MEMORY",  PH7_XML_ERROR_NO_MEMORY_Const},` |
|      - | 2271 | `	{"XML_ERROR_SYNTAX",     PH7_XML_ERROR_SYNTAX_Const},` |
|      - | 2272 | `	{"XML_ERROR_NO_ELEMENTS",PH7_XML_ERROR_NO_ELEMENTS_Const},` |
|      - | 2273 | `	{"XML_ERROR_INVALID_TOKEN", PH7_XML_ERROR_INVALID_TOKEN_Const},` |
|      - | 2274 | `	{"XML_ERROR_UNCLOSED_TOKEN",PH7_XML_ERROR_UNCLOSED_TOKEN_Const},` |
|      - | 2275 | `	{"XML_ERROR_PARTIAL_CHAR",  PH7_XML_ERROR_PARTIAL_CHAR_Const},` |
|      - | 2276 | `	{"XML_ERROR_TAG_MISMATCH",  PH7_XML_ERROR_TAG_MISMATCH_Const},` |
|      - | 2277 | `	{"XML_ERROR_DUPLICATE_ATTRIBUTE",   PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const},` |
|      - | 2278 | `	{"XML_ERROR_JUNK_AFTER_DOC_ELEMENT",PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const},` |
|      - | 2279 | `	{"XML_ERROR_PARAM_ENTITY_REF",      PH7_XML_ERROR_PARAM_ENTITY_REF_Const},` |
|      - | 2280 | `	{"XML_ERROR_UNDEFINED_ENTITY",      PH7_XML_ERROR_UNDEFINED_ENTITY_Const},` |
|      - | 2281 | `	{"XML_ERROR_RECURSIVE_ENTITY_REF",  PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const},` |
|      - | 2282 | `	{"XML_ERROR_ASYNC_ENTITY",          PH7_XML_ERROR_ASYNC_ENTITY_Const},` |
|      - | 2283 | `	{"XML_ERROR_BAD_CHAR_REF",          PH7_XML_ERROR_BAD_CHAR_REF_Const},` |
|      - | 2284 | `	{"XML_ERROR_BINARY_ENTITY_REF",     PH7_XML_ERROR_BINARY_ENTITY_REF_Const},` |
|      - | 2285 | `	{"XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF", PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const},` |
|      - | 2286 | `	{"XML_ERROR_MISPLACED_XML_PI",     PH7_XML_ERROR_MISPLACED_XML_PI_Const},` |
|      - | 2287 | `	{"XML_ERROR_UNKNOWN_ENCODING",     PH7_XML_ERROR_UNKNOWN_ENCODING_Const},` |
|      - | 2288 | `	{"XML_ERROR_INCORRECT_ENCODING",   PH7_XML_ERROR_INCORRECT_ENCODING_Const},` |
|      - | 2289 | `	{"XML_ERROR_UNCLOSED_CDATA_SECTION",  PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const},` |
|      - | 2290 | `	{"XML_ERROR_EXTERNAL_ENTITY_HANDLING",PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const},` |
|      - | 2291 | `	{"XML_OPTION_CASE_FOLDING",           PH7_XML_OPTION_CASE_FOLDING_Const},` |
|      - | 2292 | `	{"XML_OPTION_TARGET_ENCODING",        PH7_XML_OPTION_TARGET_ENCODING_Const},` |
|      - | 2293 | `	{"XML_OPTION_SKIP_TAGSTART",          PH7_XML_OPTION_SKIP_TAGSTART_Const},` |
|      - | 2294 | `	{"XML_OPTION_SKIP_WHITE",             PH7_XML_OPTION_SKIP_WHITE_Const},` |
|      - | 2295 | `	{"XML_SAX_IMPL",           PH7_XML_SAX_IMP_Const},` |
|      - | 2296 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2297 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|      - | 2298 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|      - | 2299 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|      - | 2300 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|      - | 2301 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|      - | 2302 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|      - | 2303 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|      - | 2304 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|      - | 2305 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|      - | 2306 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|      - | 2307 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|      - | 2308 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|      - | 2309 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|      - | 2310 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|      - | 2311 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|      - | 2312 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|      - | 2313 | `	{"JSON_ERROR_NON_BACKED_ENUM", PH7_JSON_ERROR_NON_BACKED_ENUM_Const},` |
|      - | 2314 | `	{"static",               PH7_static_Const       },` |
|      - | 2315 | `	{"self",                 PH7_self_Const         },` |
|      - | 2316 | `	{"__CLASS__",            PH7_self_Const         },` |
|      - | 2317 | `	{"parent",               PH7_parent_Const       }` |
|      - | 2318 | `};` |
|      - | 2319 | `/*` |
|      - | 2320 | ` * Register the built-in constants defined above.` |
|      - | 2321 | ` */` |
|   3496 | 2322 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|      5 | 2323 | `{` |
|      - | 2324 | `	sxu32 n;` |
|      - | 2325 | `	/*` |
|      - | 2326 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|      - | 2327 | `	 * that trigger the constant invocation as their private data.` |
|      - | 2328 | `	 */` |
| 908965 | 2329 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 905469 | 2330 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
| 452737 | 2331 | `	}` |
|   3501 | 2332 | `}` |
