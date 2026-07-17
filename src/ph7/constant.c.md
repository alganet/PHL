# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1108/1119 lines (99.02%)

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
|   3904 |   63 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|      5 |   64 | `{` |
|      - |   65 | `#if defined(__WINNT__)` |
|      5 |   66 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|      - |   67 | `#elif defined(__UNIXES__)` |
|      - |   68 | `	struct utsname sInfo;` |
|   3904 |   69 | `	if( uname(&sInfo) != 0 ){` |
|    ! 0 |   70 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 |   71 | `	}else{` |
|   3904 |   72 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|      - |   73 | `	}` |
|      - |   74 | `#else` |
|      - |   75 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|      - |   76 | `#endif` |
|   1952 |   77 | `	SXUNUSED(pUnused);` |
|   3909 |   78 | `}` |
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
|     66 |   97 | `static void PH7_INTMAX_Const(ph7_value *pVal,void *pUnused)` |
|      3 |   98 | `{` |
|     33 |   99 | `	SXUNUSED(pUnused);` |
|     69 |  100 | `	ph7_value_int64(pVal,SXI64_HIGH);` |
|     69 |  101 | `}` |
|      - |  102 | `/*` |
|      - |  103 | ` * PHP_INT_MIN (php 7.0)` |
|      - |  104 | ` * Expand the smallest integer supported.` |
|      - |  105 | ` */` |
|     24 |  106 | `static void PH7_INTMIN_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  107 | `{` |
|     12 |  108 | `	SXUNUSED(pUnused);` |
|     25 |  109 | `	ph7_value_int64(pVal,SMALLEST_INT64);` |
|     25 |  110 | `}` |
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
|    156 |  149 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|      2 |  150 | `{` |
|     78 |  151 | `	SXUNUSED(pUnused);` |
|      - |  152 | `#ifdef __WINNT__` |
|      2 |  153 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|      - |  154 | `#else` |
|    156 |  155 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|      - |  156 | `#endif` |
|    158 |  157 | `}` |
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
|   2262 |  244 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  245 | `{` |
|   2267 |  246 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  247 | `	SyString *pFile;` |
|      - |  248 | `	/* Peek the top entry */` |
|   2267 |  249 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|   2267 |  250 | `	if( pFile == 0 ){` |
|      - |  251 | `		/* Expand the magic word: ":MEMORY:" */` |
|      3 |  252 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|      2 |  253 | `	}else{` |
|   2265 |  254 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|      - |  255 | `	}` |
|   2267 |  256 | `}` |
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
|      3 |  269 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|      2 |  270 | `	}else{` |
|     41 |  271 | `		if( pFile->nByte > 0 ){` |
|      - |  272 | `			const char *zDir;` |
|      - |  273 | `			int nLen;` |
|     41 |  274 | `			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|     41 |  275 | `			ph7_value_string(pVal,zDir,nLen);` |
|     22 |  276 | `		}else{` |
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
|      4 |  371 | `static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  372 | `{` |
|      6 |  373 | `	ph7_value_int(pVal,256);` |
|      2 |  374 | `	SXUNUSED(pUserData);` |
|      6 |  375 | `}` |
|      - |  376 | `/*` |
|      - |  377 | ` * E_USER_WARNING` |
|      - |  378 | ` * Expands 512` |
|      - |  379 | ` */` |
|      6 |  380 | `static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  381 | `{` |
|      8 |  382 | `	ph7_value_int(pVal,512);` |
|      3 |  383 | `	SXUNUSED(pUserData);` |
|      8 |  384 | `}` |
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
|     24 |  434 | `static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  435 | `{` |
|     29 |  436 | `	ph7_value_int(pVal,32767);` |
|     12 |  437 | `	SXUNUSED(pUserData);` |
|     29 |  438 | `}` |
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
|      - |  557 | ` * SPHP_ROUND_HALF_DOWN` |
|      - |  558 | ` *  Expands 2.` |
|      - |  559 | ` */` |
|      4 |  560 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  561 | `{` |
|      5 |  562 | `	ph7_value_int(pVal,2);` |
|      2 |  563 | `	SXUNUSED(pUserData);` |
|      5 |  564 | `}` |
|      - |  565 | `/*` |
|      - |  566 | ` * PHP_ROUND_HALF_EVEN` |
|      - |  567 | ` *  Expands 3.` |
|      - |  568 | ` */` |
|      8 |  569 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  570 | `{` |
|      9 |  571 | `	ph7_value_int(pVal,3);` |
|      4 |  572 | `	SXUNUSED(pUserData);` |
|      9 |  573 | `}` |
|      - |  574 | `/*` |
|      - |  575 | ` * PHP_ROUND_HALF_ODD` |
|      - |  576 | ` *  Expands 4.` |
|      - |  577 | ` */` |
|      4 |  578 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  579 | `{` |
|      5 |  580 | `	ph7_value_int(pVal,4);` |
|      2 |  581 | `	SXUNUSED(pUserData);` |
|      5 |  582 | `}` |
|      - |  583 | `/*` |
|      - |  584 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|      - |  585 | ` *  Expand 0x01` |
|      - |  586 | ` * NOTE:` |
|      - |  587 | ` *  The expanded value must be a power of two.` |
|      - |  588 | ` */` |
|      2 |  589 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  590 | `{` |
|      3 |  591 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      1 |  592 | `	SXUNUSED(pUserData);` |
|      3 |  593 | `}` |
|      - |  594 | `/*` |
|      - |  595 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|      - |  596 | ` *  Expand 0x02` |
|      - |  597 | ` * NOTE:` |
|      - |  598 | ` *  The expanded value must be a power of two.` |
|      - |  599 | ` */` |
|      2 |  600 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  601 | `{` |
|      3 |  602 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      1 |  603 | `	SXUNUSED(pUserData);` |
|      3 |  604 | `}` |
|      - |  605 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  606 | `/*` |
|      - |  607 | ` * M_PI` |
|      - |  608 | ` *  Expand the value of pi.` |
|      - |  609 | ` */` |
|      2 |  610 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  611 | `{` |
|      1 |  612 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  613 | `	ph7_value_double(pVal,PH7_PI);` |
|      3 |  614 | `}` |
|      - |  615 | `/*` |
|      - |  616 | ` * M_E` |
|      - |  617 | ` *  Expand 2.7182818284590452354` |
|      - |  618 | ` */` |
|      2 |  619 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  620 | `{` |
|      1 |  621 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  622 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|      3 |  623 | `}` |
|      - |  624 | `/*` |
|      - |  625 | ` * M_LOG2E` |
|      - |  626 | ` *  Expand 2.7182818284590452354` |
|      - |  627 | ` */` |
|      2 |  628 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  629 | `{` |
|      1 |  630 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  631 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      3 |  632 | `}` |
|      - |  633 | `/*` |
|      - |  634 | ` * M_LOG10E` |
|      - |  635 | ` *  Expand 0.4342944819032518276` |
|      - |  636 | ` */` |
|      2 |  637 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  638 | `{` |
|      1 |  639 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  640 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      3 |  641 | `}` |
|      - |  642 | `/*` |
|      - |  643 | ` * M_LN2` |
|      - |  644 | ` *  Expand 	0.69314718055994530942` |
|      - |  645 | ` */` |
|      2 |  646 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  647 | `{` |
|      1 |  648 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  649 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      3 |  650 | `}` |
|      - |  651 | `/*` |
|      - |  652 | ` * M_LN10` |
|      - |  653 | ` *  Expand 	2.30258509299404568402` |
|      - |  654 | ` */` |
|      2 |  655 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  656 | `{` |
|      1 |  657 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  658 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      3 |  659 | `}` |
|      - |  660 | `/*` |
|      - |  661 | ` * M_PI_2` |
|      - |  662 | ` *  Expand 	1.57079632679489661923` |
|      - |  663 | ` */` |
|      2 |  664 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  665 | `{` |
|      1 |  666 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  667 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      3 |  668 | `}` |
|      - |  669 | `/*` |
|      - |  670 | ` * M_PI_4` |
|      - |  671 | ` *  Expand 	0.78539816339744830962` |
|      - |  672 | ` */` |
|      2 |  673 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  674 | `{` |
|      1 |  675 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  676 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      3 |  677 | `}` |
|      - |  678 | `/*` |
|      - |  679 | ` * M_1_PI` |
|      - |  680 | ` *  Expand 	0.31830988618379067154` |
|      - |  681 | ` */` |
|      2 |  682 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  683 | `{` |
|      1 |  684 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  685 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      3 |  686 | `}` |
|      - |  687 | `/*` |
|      - |  688 | ` * M_2_PI` |
|      - |  689 | ` *  Expand 0.63661977236758134308` |
|      - |  690 | ` */` |
|      4 |  691 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  692 | `{` |
|      2 |  693 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  694 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      5 |  695 | `}` |
|      - |  696 | `/*` |
|      - |  697 | ` * M_SQRTPI` |
|      - |  698 | ` *  Expand 1.77245385090551602729` |
|      - |  699 | ` */` |
|      2 |  700 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  701 | `{` |
|      1 |  702 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  703 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      3 |  704 | `}` |
|      - |  705 | `/*` |
|      - |  706 | ` * M_2_SQRTPI` |
|      - |  707 | ` *  Expand 	1.12837916709551257390` |
|      - |  708 | ` */` |
|      2 |  709 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  710 | `{` |
|      1 |  711 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  712 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      3 |  713 | `}` |
|      - |  714 | `/*` |
|      - |  715 | ` * M_SQRT2` |
|      - |  716 | ` *  Expand 	1.41421356237309504880` |
|      - |  717 | ` */` |
|      2 |  718 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  719 | `{` |
|      1 |  720 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  721 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      3 |  722 | `}` |
|      - |  723 | `/*` |
|      - |  724 | ` * M_SQRT3` |
|      - |  725 | ` *  Expand 	1.73205080756887729352` |
|      - |  726 | ` */` |
|      2 |  727 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  728 | `{` |
|      1 |  729 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  730 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      3 |  731 | `}` |
|      - |  732 | `/*` |
|      - |  733 | ` * M_SQRT1_2` |
|      - |  734 | ` *  Expand 	0.70710678118654752440` |
|      - |  735 | ` */` |
|      2 |  736 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  737 | `{` |
|      1 |  738 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  739 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      3 |  740 | `}` |
|      - |  741 | `/*` |
|      - |  742 | ` * M_LNPI` |
|      - |  743 | ` *  Expand 	1.14472988584940017414` |
|      - |  744 | ` */` |
|      2 |  745 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  746 | `{` |
|      1 |  747 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  748 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      3 |  749 | `}` |
|      - |  750 | `/*` |
|      - |  751 | ` * M_EULER` |
|      - |  752 | ` *  Expand  0.57721566490153286061` |
|      - |  753 | ` */` |
|      2 |  754 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  755 | `{` |
|      1 |  756 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  757 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      3 |  758 | `}` |
|      - |  759 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|      - |  760 | `/*` |
|      - |  761 | ` * DATE_ATOM` |
|      - |  762 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|      - |  763 | ` */` |
|      2 |  764 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  765 | `{` |
|      1 |  766 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  767 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  768 | `}` |
|      - |  769 | `/*` |
|      - |  770 | ` * DATE_COOKIE` |
|      - |  771 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  772 | ` */` |
|      2 |  773 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  774 | `{` |
|      1 |  775 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  776 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  777 | `}` |
|      - |  778 | `/*` |
|      - |  779 | ` * DATE_ISO8601` |
|      - |  780 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|      - |  781 | ` */` |
|      2 |  782 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  783 | `{` |
|      1 |  784 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  785 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      3 |  786 | `}` |
|      - |  787 | `/*` |
|      - |  788 | ` * DATE_RFC822` |
|      - |  789 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|      - |  790 | ` */` |
|      2 |  791 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  792 | `{` |
|      1 |  793 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  794 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  795 | `}` |
|      - |  796 | `/*` |
|      - |  797 | ` * DATE_RFC850` |
|      - |  798 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  799 | ` */` |
|      2 |  800 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  801 | `{` |
|      1 |  802 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  803 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  804 | `}` |
|      - |  805 | `/*` |
|      - |  806 | ` * DATE_RFC1036` |
|      - |  807 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  808 | ` */` |
|      2 |  809 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  810 | `{` |
|      1 |  811 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  812 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  813 | `}` |
|      - |  814 | `/*` |
|      - |  815 | ` * DATE_RFC1123` |
|      - |  816 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  817 | ` */` |
|      2 |  818 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  819 | `{` |
|      1 |  820 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  821 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  822 | `}` |
|      - |  823 | `/*` |
|      - |  824 | ` * DATE_RFC2822` |
|      - |  825 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  826 | ` */` |
|      2 |  827 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  828 | `{` |
|      1 |  829 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  830 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  831 | `}` |
|      - |  832 | `/*` |
|      - |  833 | ` * DATE_RSS` |
|      - |  834 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  835 | ` */` |
|      2 |  836 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  837 | `{` |
|      1 |  838 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  839 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  840 | `}` |
|      - |  841 | `/*` |
|      - |  842 | ` * DATE_W3C` |
|      - |  843 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|      - |  844 | ` */` |
|      2 |  845 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  846 | `{` |
|      1 |  847 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  848 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  849 | `}` |
|      - |  850 | `/*` |
|      - |  851 | ` * The ENT_* values are PHP-exact (php 8.5.7). The low two bits are the quote` |
|      - |  852 | ` * bits (1 = single, 2 = double), so ENT_QUOTES = ENT_COMPAT\|1 and` |
|      - |  853 | ` * ENT_NOQUOTES = 0. Bits 16\|32 select the doctype (0 = HTML401, 16 = XML1,` |
|      - |  854 | ` * 32 = XHTML, 48 = HTML5) — composites, not flags.` |
|      - |  855 | ` */` |
|      - |  856 | `/*` |
|      - |  857 | ` * ENT_COMPAT` |
|      - |  858 | ` *  Expand 2 (double-quote bit only)` |
|      - |  859 | ` */` |
|     12 |  860 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  861 | `{` |
|      6 |  862 | `	SXUNUSED(pUserData); /* cc warning */` |
|     13 |  863 | `	ph7_value_int(pVal,PH7_ENT_QUOTE_DOUBLE);` |
|     13 |  864 | `}` |
|      - |  865 | `/*` |
|      - |  866 | ` * ENT_QUOTES` |
|      - |  867 | ` *  Expand 3 (double\|single quote bits)` |
|      - |  868 | ` */` |
|     60 |  869 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  870 | `{` |
|     30 |  871 | `	SXUNUSED(pUserData); /* cc warning */` |
|     61 |  872 | `	ph7_value_int(pVal,PH7_ENT_QUOTES);` |
|     61 |  873 | `}` |
|      - |  874 | `/*` |
|      - |  875 | ` * ENT_NOQUOTES` |
|      - |  876 | ` *  Expand 0 (no quote bits)` |
|      - |  877 | ` */` |
|     20 |  878 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  879 | `{` |
|     10 |  880 | `	SXUNUSED(pUserData); /* cc warning */` |
|     21 |  881 | `	ph7_value_int(pVal,0);` |
|     21 |  882 | `}` |
|      - |  883 | `/*` |
|      - |  884 | ` * ENT_IGNORE` |
|      - |  885 | ` *  Expand 4` |
|      - |  886 | ` */` |
|      6 |  887 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  888 | `{` |
|      3 |  889 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 |  890 | `	ph7_value_int(pVal,PH7_ENT_IGNORE);` |
|      7 |  891 | `}` |
|      - |  892 | `/*` |
|      - |  893 | ` * ENT_SUBSTITUTE` |
|      - |  894 | ` *  Expand 8` |
|      - |  895 | ` */` |
|      2 |  896 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  897 | `{` |
|      1 |  898 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  899 | `	ph7_value_int(pVal,PH7_ENT_SUBSTITUTE);` |
|      3 |  900 | `}` |
|      - |  901 | `/*` |
|      - |  902 | ` * ENT_DISALLOWED` |
|      - |  903 | ` *  Expand 128` |
|      - |  904 | ` */` |
|      2 |  905 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  906 | `{` |
|      1 |  907 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  908 | `	ph7_value_int(pVal,PH7_ENT_DISALLOWED);` |
|      3 |  909 | `}` |
|      - |  910 | `/*` |
|      - |  911 | ` * ENT_HTML401` |
|      - |  912 | ` *  Expand 0 (the default doctype)` |
|      - |  913 | ` */` |
|      2 |  914 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  915 | `{` |
|      1 |  916 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  917 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML401);` |
|      3 |  918 | `}` |
|      - |  919 | `/*` |
|      - |  920 | ` * ENT_XML1` |
|      - |  921 | ` *  Expand 16` |
|      - |  922 | ` */` |
|      8 |  923 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  924 | `{` |
|      4 |  925 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 |  926 | `	ph7_value_int(pVal,PH7_ENT_DOC_XML1);` |
|      9 |  927 | `}` |
|      - |  928 | `/*` |
|      - |  929 | ` * ENT_XHTML` |
|      - |  930 | ` *  Expand 32` |
|      - |  931 | ` */` |
|      6 |  932 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  933 | `{` |
|      3 |  934 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 |  935 | `	ph7_value_int(pVal,PH7_ENT_DOC_XHTML);` |
|      7 |  936 | `}` |
|      - |  937 | `/*` |
|      - |  938 | ` * ENT_HTML5` |
|      - |  939 | ` *  Expand 48 (16\|32 — a doctype composite, not a flag bit)` |
|      - |  940 | ` */` |
|      8 |  941 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  942 | `{` |
|      4 |  943 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 |  944 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML5);` |
|      9 |  945 | `}` |
|      - |  946 | `/*` |
|      - |  947 | ` * ISO-8859-1` |
|      - |  948 | ` * ISO_8859_1` |
|      - |  949 | ` *   Expand 1` |
|      - |  950 | ` */` |
|      2 |  951 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  952 | `{` |
|      1 |  953 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  954 | `	ph7_value_int(pVal,1);` |
|      3 |  955 | `}` |
|      - |  956 | `/*` |
|      - |  957 | ` * UTF-8` |
|      - |  958 | ` * UTF8` |
|      - |  959 | ` *  Expand 2` |
|      - |  960 | ` */` |
|      2 |  961 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  962 | `{` |
|      1 |  963 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  964 | `	ph7_value_int(pVal,1);` |
|      3 |  965 | `}` |
|      - |  966 | `/*` |
|      - |  967 | ` * HTML_ENTITIES` |
|      - |  968 | ` *  Expand 1` |
|      - |  969 | ` */` |
|      4 |  970 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  971 | `{` |
|      2 |  972 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  973 | `	ph7_value_int(pVal,1);` |
|      5 |  974 | `}` |
|      - |  975 | `/*` |
|      - |  976 | ` * HTML_SPECIALCHARS` |
|      - |  977 | ` *  Expand 0 (PHP-exact)` |
|      - |  978 | ` */` |
|     10 |  979 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  980 | `{` |
|      5 |  981 | `	SXUNUSED(pUserData); /* cc warning */` |
|     11 |  982 | `	ph7_value_int(pVal,0);` |
|     11 |  983 | `}` |
|      - |  984 | `/*` |
|      - |  985 | ` * PHP_URL_SCHEME.` |
|      - |  986 | ` * Expand 1` |
|      - |  987 | ` */` |
|      2 |  988 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  989 | `{` |
|      1 |  990 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  991 | `	ph7_value_int(pVal,1);` |
|      3 |  992 | `}` |
|      - |  993 | `/*` |
|      - |  994 | ` * PHP_URL_HOST.` |
|      - |  995 | ` * Expand 2` |
|      - |  996 | ` */` |
|      2 |  997 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  998 | `{` |
|      1 |  999 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1000 | `	ph7_value_int(pVal,2);` |
|      3 | 1001 | `}` |
|      - | 1002 | `/*` |
|      - | 1003 | ` * PHP_URL_PORT.` |
|      - | 1004 | ` * Expand 3` |
|      - | 1005 | ` */` |
|      2 | 1006 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1007 | `{` |
|      1 | 1008 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1009 | `	ph7_value_int(pVal,3);` |
|      3 | 1010 | `}` |
|      - | 1011 | `/*` |
|      - | 1012 | ` * PHP_URL_USER.` |
|      - | 1013 | ` * Expand 4` |
|      - | 1014 | ` */` |
|      2 | 1015 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1016 | `{` |
|      1 | 1017 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1018 | `	ph7_value_int(pVal,4);` |
|      3 | 1019 | `}` |
|      - | 1020 | `/*` |
|      - | 1021 | ` * PHP_URL_PASS.` |
|      - | 1022 | ` * Expand 5` |
|      - | 1023 | ` */` |
|      2 | 1024 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1025 | `{` |
|      1 | 1026 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1027 | `	ph7_value_int(pVal,5);` |
|      3 | 1028 | `}` |
|      - | 1029 | `/*` |
|      - | 1030 | ` * PHP_URL_PATH.` |
|      - | 1031 | ` * Expand 6` |
|      - | 1032 | ` */` |
|      2 | 1033 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1034 | `{` |
|      1 | 1035 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1036 | `	ph7_value_int(pVal,6);` |
|      3 | 1037 | `}` |
|      - | 1038 | `/*` |
|      - | 1039 | ` * PHP_URL_QUERY.` |
|      - | 1040 | ` * Expand 7` |
|      - | 1041 | ` */` |
|      2 | 1042 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1043 | `{` |
|      1 | 1044 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1045 | `	ph7_value_int(pVal,7);` |
|      3 | 1046 | `}` |
|      - | 1047 | `/*` |
|      - | 1048 | ` * PHP_URL_FRAGMENT.` |
|      - | 1049 | ` * Expand 8` |
|      - | 1050 | ` */` |
|      2 | 1051 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1052 | `{` |
|      1 | 1053 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1054 | `	ph7_value_int(pVal,8);` |
|      3 | 1055 | `}` |
|      - | 1056 | `/*` |
|      - | 1057 | ` * PHP_QUERY_RFC1738` |
|      - | 1058 | ` * Expand 1` |
|      - | 1059 | ` */` |
|      2 | 1060 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1061 | `{` |
|      1 | 1062 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1063 | `	ph7_value_int(pVal,1);` |
|      3 | 1064 | `}` |
|      - | 1065 | `/*` |
|      - | 1066 | ` * PHP_QUERY_RFC3986` |
|      - | 1067 | ` * Expand 1` |
|      - | 1068 | ` */` |
|      2 | 1069 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1070 | `{` |
|      1 | 1071 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1072 | `	ph7_value_int(pVal,2);` |
|      3 | 1073 | `}` |
|      - | 1074 | `/*` |
|      - | 1075 | ` * FNM_NOESCAPE` |
|      - | 1076 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1077 | ` */` |
|      2 | 1078 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1079 | `{` |
|      1 | 1080 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1081 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1082 | `}` |
|      - | 1083 | `/*` |
|      - | 1084 | ` * FNM_PATHNAME` |
|      - | 1085 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1086 | ` */` |
|      2 | 1087 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1088 | `{` |
|      1 | 1089 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1090 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1091 | `}` |
|      - | 1092 | `/*` |
|      - | 1093 | ` * FNM_PERIOD` |
|      - | 1094 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1095 | ` */` |
|      6 | 1096 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1097 | `{` |
|      3 | 1098 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1099 | `	ph7_value_int(pVal,0x04);` |
|      7 | 1100 | `}` |
|      - | 1101 | `/*` |
|      - | 1102 | ` * FNM_CASEFOLD` |
|      - | 1103 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1104 | ` */` |
|      4 | 1105 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1106 | `{` |
|      2 | 1107 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1108 | `	ph7_value_int(pVal,0x08);` |
|      5 | 1109 | `}` |
|      - | 1110 | `/*` |
|      - | 1111 | ` * PATHINFO_DIRNAME` |
|      - | 1112 | ` *  Expand 1.` |
|      - | 1113 | ` */` |
|      4 | 1114 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1115 | `{` |
|      2 | 1116 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1117 | `	ph7_value_int(pVal,1);` |
|      5 | 1118 | `}` |
|      - | 1119 | `/*` |
|      - | 1120 | ` * PATHINFO_BASENAME` |
|      - | 1121 | ` *  Expand 2.` |
|      - | 1122 | ` */` |
|      4 | 1123 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1124 | `{` |
|      2 | 1125 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1126 | `	ph7_value_int(pVal,2);` |
|      5 | 1127 | `}` |
|      - | 1128 | `/*` |
|      - | 1129 | ` * PATHINFO_EXTENSION` |
|      - | 1130 | ` *  Expand 3.` |
|      - | 1131 | ` */` |
|   6382 | 1132 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1133 | `{` |
|   3191 | 1134 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6387 | 1135 | `	ph7_value_int(pVal,3);` |
|   6387 | 1136 | `}` |
|      - | 1137 | `/*` |
|      - | 1138 | ` * PATHINFO_FILENAME` |
|      - | 1139 | ` *  Expand 4.` |
|      - | 1140 | ` */` |
|   6374 | 1141 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1142 | `{` |
|   3187 | 1143 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6379 | 1144 | `	ph7_value_int(pVal,4);` |
|   6379 | 1145 | `}` |
|      - | 1146 | `/*` |
|      - | 1147 | ` * ASSERT_ACTIVE.` |
|      - | 1148 | ` *  PHP ASSERT_ACTIVE = 1` |
|      - | 1149 | ` */` |
|     14 | 1150 | `static void PH7_ASSERT_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1151 | `{` |
|      7 | 1152 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1153 | `	ph7_value_int(pVal,1); /* PHP ASSERT_ACTIVE = 1 */` |
|     16 | 1154 | `}` |
|      - | 1155 | `/*` |
|      - | 1156 | ` * ASSERT_CALLBACK.` |
|      - | 1157 | ` *  PHP ASSERT_CALLBACK = 2` |
|      - | 1158 | ` */` |
|      6 | 1159 | `static void PH7_ASSERT_CALLBACK_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1160 | `{` |
|      3 | 1161 | `	SXUNUSED(pUserData); /* cc warning */` |
|      8 | 1162 | `	ph7_value_int(pVal,2); /* PHP ASSERT_CALLBACK = 2 */` |
|      8 | 1163 | `}` |
|      - | 1164 | `/*` |
|      - | 1165 | ` * ASSERT_BAIL.` |
|      - | 1166 | ` *  PHP ASSERT_BAIL = 3` |
|      - | 1167 | ` */` |
|     14 | 1168 | `static void PH7_ASSERT_BAIL_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1169 | `{` |
|      7 | 1170 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1171 | `	ph7_value_int(pVal,3); /* PHP ASSERT_BAIL = 3 */` |
|     16 | 1172 | `}` |
|      - | 1173 | `/*` |
|      - | 1174 | ` * ASSERT_WARNING.` |
|      - | 1175 | ` *  PHP ASSERT_WARNING = 4 (deprecated in PHP 8.3)` |
|      - | 1176 | ` */` |
|      4 | 1177 | `static void PH7_ASSERT_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1178 | `{` |
|      2 | 1179 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1180 | `	ph7_value_int(pVal,4); /* PHP ASSERT_WARNING = 4 */` |
|      5 | 1181 | `}` |
|      - | 1182 | `/*` |
|      - | 1183 | ` * ASSERT_EXCEPTION.` |
|      - | 1184 | ` *  PHP ASSERT_EXCEPTION = 5 (deprecated in PHP 8.3)` |
|      - | 1185 | ` */` |
|      4 | 1186 | `static void PH7_ASSERT_EXCEPTION_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1187 | `{` |
|      2 | 1188 | `	SXUNUSED(pUserData); /* cc warning */` |
|      6 | 1189 | `	ph7_value_int(pVal,5); /* PHP ASSERT_EXCEPTION = 5 */` |
|      6 | 1190 | `}` |
|      - | 1191 | `/*` |
|      - | 1192 | ` * ASSERT_QUIET_EVAL.` |
|      - | 1193 | ` *  Removed in PHP 8.0, kept for compatibility.` |
|      - | 1194 | ` */` |
|      2 | 1195 | `static void PH7_ASSERT_QUIET_EVAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1196 | `{` |
|      1 | 1197 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1198 | `	ph7_value_int(pVal,6); /* Arbitrary value, removed in PHP 8 */` |
|      3 | 1199 | `}` |
|      - | 1200 | `/*` |
|      - | 1201 | ` * SEEK_SET.` |
|      - | 1202 | ` *  Expand 0` |
|      - | 1203 | ` */` |
|      2 | 1204 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1205 | `{` |
|      1 | 1206 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1207 | `	ph7_value_int(pVal,0);` |
|      3 | 1208 | `}` |
|      - | 1209 | `/*` |
|      - | 1210 | ` * SEEK_CUR.` |
|      - | 1211 | ` *  Expand 1` |
|      - | 1212 | ` */` |
|      2 | 1213 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1214 | `{` |
|      1 | 1215 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1216 | `	ph7_value_int(pVal,1);` |
|      3 | 1217 | `}` |
|      - | 1218 | `/*` |
|      - | 1219 | ` * SEEK_END.` |
|      - | 1220 | ` *  Expand 2` |
|      - | 1221 | ` */` |
|      2 | 1222 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1223 | `{` |
|      1 | 1224 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1225 | `	ph7_value_int(pVal,2);` |
|      3 | 1226 | `}` |
|      - | 1227 | `/*` |
|      - | 1228 | ` * LOCK_SH.` |
|      - | 1229 | ` *  Expand 2` |
|      - | 1230 | ` */` |
|      2 | 1231 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1232 | `{` |
|      1 | 1233 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1234 | `	ph7_value_int(pVal,1);` |
|      3 | 1235 | `}` |
|      - | 1236 | `/*` |
|      - | 1237 | ` * LOCK_NB.` |
|      - | 1238 | ` *  Expand 5` |
|      - | 1239 | ` */` |
|      2 | 1240 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1241 | `{` |
|      1 | 1242 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1243 | `	ph7_value_int(pVal,5);` |
|      3 | 1244 | `}` |
|      - | 1245 | `/*` |
|      - | 1246 | ` * LOCK_EX.` |
|      - | 1247 | ` *  Expand 0x01 (MUST BE A POWER OF TWO)` |
|      - | 1248 | ` */` |
|      4 | 1249 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1250 | `{` |
|      2 | 1251 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1252 | `	ph7_value_int(pVal,0x01);` |
|      5 | 1253 | `}` |
|      - | 1254 | `/*` |
|      - | 1255 | ` * LOCK_UN.` |
|      - | 1256 | ` *  Expand 0` |
|      - | 1257 | ` */` |
|      4 | 1258 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1259 | `{` |
|      2 | 1260 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1261 | `	ph7_value_int(pVal,0);` |
|      5 | 1262 | `}` |
|      - | 1263 | `/*` |
|      - | 1264 | ` * FILE_USE_INCLUDE_PATH` |
|      - | 1265 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1266 | ` */` |
|      2 | 1267 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1268 | `{` |
|      1 | 1269 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1270 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1271 | `}` |
|      - | 1272 | `/*` |
|      - | 1273 | ` * FILE_IGNORE_NEW_LINES` |
|      - | 1274 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1275 | ` */` |
|      2 | 1276 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1277 | `{` |
|      1 | 1278 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1279 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1280 | `}` |
|      - | 1281 | `/*` |
|      - | 1282 | ` * FILE_SKIP_EMPTY_LINES` |
|      - | 1283 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1284 | ` */` |
|      2 | 1285 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1286 | `{` |
|      1 | 1287 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1288 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1289 | `}` |
|      - | 1290 | `/*` |
|      - | 1291 | ` * FILE_APPEND` |
|      - | 1292 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1293 | ` */` |
|      2 | 1294 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1295 | `{` |
|      1 | 1296 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1297 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1298 | `}` |
|      - | 1299 | `/*` |
|      - | 1300 | ` * SCANDIR_SORT_ASCENDING` |
|      - | 1301 | ` *  Expand 0` |
|      - | 1302 | ` */` |
|   1962 | 1303 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1304 | `{` |
|    981 | 1305 | `	SXUNUSED(pUserData); /* cc warning */` |
|   1967 | 1306 | `	ph7_value_int(pVal,0);` |
|   1967 | 1307 | `}` |
|      - | 1308 | `/*` |
|      - | 1309 | ` * SCANDIR_SORT_DESCENDING` |
|      - | 1310 | ` *  Expand 1` |
|      - | 1311 | ` */` |
|    982 | 1312 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1313 | `{` |
|    491 | 1314 | `	SXUNUSED(pUserData); /* cc warning */` |
|    987 | 1315 | `	ph7_value_int(pVal,1);` |
|    987 | 1316 | `}` |
|      - | 1317 | `/*` |
|      - | 1318 | ` * SCANDIR_SORT_NONE` |
|      - | 1319 | ` *  Expand 2` |
|      - | 1320 | ` */` |
|      2 | 1321 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1322 | `{` |
|      1 | 1323 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1324 | `	ph7_value_int(pVal,2);` |
|      3 | 1325 | `}` |
|      - | 1326 | `/*` |
|      - | 1327 | ` * GLOB_MARK` |
|      - | 1328 | ` *  Expand 0x01 (must be a power of two)` |
|      - | 1329 | ` */` |
|      2 | 1330 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1331 | `{` |
|      1 | 1332 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1333 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1334 | `}` |
|      - | 1335 | `/*` |
|      - | 1336 | ` * GLOB_NOSORT` |
|      - | 1337 | ` *  Expand 0x02 (must be a power of two)` |
|      - | 1338 | ` */` |
|      2 | 1339 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1340 | `{` |
|      1 | 1341 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1342 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1343 | `}` |
|      - | 1344 | `/*` |
|      - | 1345 | ` * GLOB_NOCHECK` |
|      - | 1346 | ` *  Expand 0x04 (must be a power of two)` |
|      - | 1347 | ` */` |
|      2 | 1348 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1349 | `{` |
|      1 | 1350 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1351 | `	ph7_value_int(pVal,0x04);` |
|      3 | 1352 | `}` |
|      - | 1353 | `/*` |
|      - | 1354 | ` * GLOB_NOESCAPE` |
|      - | 1355 | ` *  Expand 0x08 (must be a power of two)` |
|      - | 1356 | ` */` |
|      2 | 1357 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1358 | `{` |
|      1 | 1359 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1360 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1361 | `}` |
|      - | 1362 | `/*` |
|      - | 1363 | ` * GLOB_BRACE` |
|      - | 1364 | ` *  Expand 0x10 (must be a power of two)` |
|      - | 1365 | ` */` |
|      2 | 1366 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1367 | `{` |
|      1 | 1368 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1369 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1370 | `}` |
|      - | 1371 | `/*` |
|      - | 1372 | ` * GLOB_ONLYDIR` |
|      - | 1373 | ` *  Expand 0x20 (must be a power of two)` |
|      - | 1374 | ` */` |
|      2 | 1375 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1376 | `{` |
|      1 | 1377 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1378 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1379 | `}` |
|      - | 1380 | `/*` |
|      - | 1381 | ` * GLOB_ERR` |
|      - | 1382 | ` *  Expand 0x40 (must be a power of two)` |
|      - | 1383 | ` */` |
|      2 | 1384 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1385 | `{` |
|      1 | 1386 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1387 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1388 | `}` |
|      - | 1389 | `/*` |
|      - | 1390 | ` * STDIN` |
|      - | 1391 | ` *  Expand the STDIN handle as a resource.` |
|      - | 1392 | ` */` |
|      2 | 1393 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1394 | `{` |
|      3 | 1395 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1396 | `	void *pResource;` |
|      3 | 1397 | `	pResource = PH7_ExportStdin(pVm);` |
|      3 | 1398 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1399 | `}` |
|      - | 1400 | `/*` |
|      - | 1401 | ` * STDOUT` |
|      - | 1402 | ` *   Expand the STDOUT handle as a resource.` |
|      - | 1403 | ` */` |
|      2 | 1404 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1405 | `{` |
|      3 | 1406 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1407 | `	void *pResource;` |
|      3 | 1408 | `	pResource = PH7_ExportStdout(pVm);` |
|      3 | 1409 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1410 | `}` |
|      - | 1411 | `/*` |
|      - | 1412 | ` * STDERR` |
|      - | 1413 | ` *  Expand the STDERR handle as a resource.` |
|      - | 1414 | ` */` |
|      2 | 1415 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1416 | `{` |
|      3 | 1417 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1418 | `	void *pResource;` |
|      3 | 1419 | `	pResource = PH7_ExportStderr(pVm);` |
|      3 | 1420 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1421 | `}` |
|      - | 1422 | `/*` |
|      - | 1423 | ` * INI_SCANNER_NORMAL` |
|      - | 1424 | ` *   Expand 1` |
|      - | 1425 | ` */` |
|      2 | 1426 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1427 | `{` |
|      1 | 1428 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1429 | `	ph7_value_int(pVal,1);` |
|      3 | 1430 | `}` |
|      - | 1431 | `/*` |
|      - | 1432 | ` * INI_SCANNER_RAW` |
|      - | 1433 | ` *   Expand 2` |
|      - | 1434 | ` */` |
|      2 | 1435 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1436 | `{` |
|      1 | 1437 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1438 | `	ph7_value_int(pVal,2);` |
|      3 | 1439 | `}` |
|      - | 1440 | `/*` |
|      - | 1441 | ` * EXTR_OVERWRITE` |
|      - | 1442 | ` *   Expand 0x01 (Must be a power of two)` |
|      - | 1443 | ` */` |
|      2 | 1444 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1445 | `{` |
|      1 | 1446 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1447 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1448 | `}` |
|      - | 1449 | `/*` |
|      - | 1450 | ` * EXTR_SKIP` |
|      - | 1451 | ` *   Expand 0x02 (Must be a power of two)` |
|      - | 1452 | ` */` |
|      2 | 1453 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1454 | `{` |
|      1 | 1455 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1456 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1457 | `}` |
|      - | 1458 | `/*` |
|      - | 1459 | ` * EXTR_PREFIX_SAME` |
|      - | 1460 | ` *   Expand 0x04 (Must be a power of two)` |
|      - | 1461 | ` */` |
|      2 | 1462 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1463 | `{` |
|      1 | 1464 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1465 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1466 | `}` |
|      - | 1467 | `/*` |
|      - | 1468 | ` * EXTR_PREFIX_ALL` |
|      - | 1469 | ` *   Expand 0x08 (Must be a power of two)` |
|      - | 1470 | ` */` |
|      2 | 1471 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1472 | `{` |
|      1 | 1473 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1474 | `	ph7_value_int(pVal,0x8);` |
|      3 | 1475 | `}` |
|      - | 1476 | `/*` |
|      - | 1477 | ` * EXTR_PREFIX_INVALID` |
|      - | 1478 | ` *   Expand 0x10 (Must be a power of two)` |
|      - | 1479 | ` */` |
|      2 | 1480 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1481 | `{` |
|      1 | 1482 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1483 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1484 | `}` |
|      - | 1485 | `/*` |
|      - | 1486 | ` * EXTR_IF_EXISTS` |
|      - | 1487 | ` *   Expand 0x20 (Must be a power of two)` |
|      - | 1488 | ` */` |
|      2 | 1489 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1490 | `{` |
|      1 | 1491 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1492 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1493 | `}` |
|      - | 1494 | `/*` |
|      - | 1495 | ` * EXTR_PREFIX_IF_EXISTS` |
|      - | 1496 | ` *   Expand 0x40 (Must be a power of two)` |
|      - | 1497 | ` */` |
|      2 | 1498 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1499 | `{` |
|      1 | 1500 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1501 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1502 | `}` |
|      - | 1503 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1504 | `/*` |
|      - | 1505 | ` * XML_ERROR_NONE` |
|      - | 1506 | ` *   Expand the value of SXML_ERROR_NO_MEMORY defined in ph7Int.h` |
|      - | 1507 | ` */` |
|      2 | 1508 | `static void PH7_XML_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1509 | `{` |
|      1 | 1510 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1511 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1512 | `}` |
|      - | 1513 | `/*` |
|      - | 1514 | ` * XML_ERROR_NO_MEMORY` |
|      - | 1515 | ` *   Expand the value of SXML_ERROR_NONE defined in ph7Int.h` |
|      - | 1516 | ` */` |
|      2 | 1517 | `static void PH7_XML_ERROR_NO_MEMORY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1518 | `{` |
|      1 | 1519 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1520 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1521 | `}` |
|      - | 1522 | `/*` |
|      - | 1523 | ` * XML_ERROR_SYNTAX` |
|      - | 1524 | ` *   Expand the value of SXML_ERROR_SYNTAX defined in ph7Int.h` |
|      - | 1525 | ` */` |
|      2 | 1526 | `static void PH7_XML_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1527 | `{` |
|      1 | 1528 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1529 | `	ph7_value_int(pVal,SXML_ERROR_SYNTAX);` |
|      3 | 1530 | `}` |
|      - | 1531 | `/*` |
|      - | 1532 | ` * XML_ERROR_NO_ELEMENTS` |
|      - | 1533 | ` *   Expand the value of SXML_ERROR_NO_ELEMENTS defined in ph7Int.h` |
|      - | 1534 | ` */` |
|      2 | 1535 | `static void PH7_XML_ERROR_NO_ELEMENTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1536 | `{` |
|      1 | 1537 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1538 | `	ph7_value_int(pVal,SXML_ERROR_NO_ELEMENTS);` |
|      3 | 1539 | `}` |
|      - | 1540 | `/*` |
|      - | 1541 | ` * XML_ERROR_INVALID_TOKEN` |
|      - | 1542 | ` *   Expand the value of SXML_ERROR_INVALID_TOKEN defined in ph7Int.h` |
|      - | 1543 | ` */` |
|      2 | 1544 | `static void PH7_XML_ERROR_INVALID_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1545 | `{` |
|      1 | 1546 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1547 | `	ph7_value_int(pVal,SXML_ERROR_INVALID_TOKEN);` |
|      3 | 1548 | `}` |
|      - | 1549 | `/*` |
|      - | 1550 | ` * XML_ERROR_UNCLOSED_TOKEN` |
|      - | 1551 | ` *   Expand the value of SXML_ERROR_UNCLOSED_TOKEN defined in ph7Int.h` |
|      - | 1552 | ` */` |
|      2 | 1553 | `static void PH7_XML_ERROR_UNCLOSED_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1554 | `{` |
|      1 | 1555 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1556 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_TOKEN);` |
|      3 | 1557 | `}` |
|      - | 1558 | `/*` |
|      - | 1559 | ` * XML_ERROR_PARTIAL_CHAR` |
|      - | 1560 | ` *   Expand the value of SXML_ERROR_PARTIAL_CHAR defined in ph7Int.h` |
|      - | 1561 | ` */` |
|      2 | 1562 | `static void PH7_XML_ERROR_PARTIAL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1563 | `{` |
|      1 | 1564 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1565 | `	ph7_value_int(pVal,SXML_ERROR_PARTIAL_CHAR);` |
|      3 | 1566 | `}` |
|      - | 1567 | `/*` |
|      - | 1568 | ` * XML_ERROR_TAG_MISMATCH` |
|      - | 1569 | ` *   Expand the value of SXML_ERROR_TAG_MISMATCH defined in ph7Int.h` |
|      - | 1570 | ` */` |
|      2 | 1571 | `static void PH7_XML_ERROR_TAG_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1572 | `{` |
|      1 | 1573 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1574 | `	ph7_value_int(pVal,SXML_ERROR_TAG_MISMATCH);` |
|      3 | 1575 | `}` |
|      - | 1576 | `/*` |
|      - | 1577 | ` * XML_ERROR_DUPLICATE_ATTRIBUTE` |
|      - | 1578 | ` *   Expand the value of SXML_ERROR_DUPLICATE_ATTRIBUTE defined in ph7Int.h` |
|      - | 1579 | ` */` |
|      2 | 1580 | `static void PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1581 | `{` |
|      1 | 1582 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1583 | `	ph7_value_int(pVal,SXML_ERROR_DUPLICATE_ATTRIBUTE);` |
|      3 | 1584 | `}` |
|      - | 1585 | `/*` |
|      - | 1586 | ` * XML_ERROR_JUNK_AFTER_DOC_ELEMENT` |
|      - | 1587 | ` *   Expand the value of SXML_ERROR_JUNK_AFTER_DOC_ELEMENT defined in ph7Int.h` |
|      - | 1588 | ` */` |
|      2 | 1589 | `static void PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1590 | `{` |
|      1 | 1591 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1592 | `	ph7_value_int(pVal,SXML_ERROR_JUNK_AFTER_DOC_ELEMENT);` |
|      3 | 1593 | `}` |
|      - | 1594 | `/*` |
|      - | 1595 | ` * XML_ERROR_PARAM_ENTITY_REF` |
|      - | 1596 | ` *   Expand the value of SXML_ERROR_PARAM_ENTITY_REF defined in ph7Int.h` |
|      - | 1597 | ` */` |
|      2 | 1598 | `static void PH7_XML_ERROR_PARAM_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1599 | `{` |
|      1 | 1600 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1601 | `	ph7_value_int(pVal,SXML_ERROR_PARAM_ENTITY_REF);` |
|      3 | 1602 | `}` |
|      - | 1603 | `/*` |
|      - | 1604 | ` * XML_ERROR_UNDEFINED_ENTITY` |
|      - | 1605 | ` *   Expand the value of SXML_ERROR_UNDEFINED_ENTITY defined in ph7Int.h` |
|      - | 1606 | ` */` |
|      2 | 1607 | `static void PH7_XML_ERROR_UNDEFINED_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1608 | `{` |
|      1 | 1609 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1610 | `	ph7_value_int(pVal,SXML_ERROR_UNDEFINED_ENTITY);` |
|      3 | 1611 | `}` |
|      - | 1612 | `/*` |
|      - | 1613 | ` * XML_ERROR_RECURSIVE_ENTITY_REF` |
|      - | 1614 | ` *   Expand the value of SXML_ERROR_RECURSIVE_ENTITY_REF defined in ph7Int.h` |
|      - | 1615 | ` */` |
|      2 | 1616 | `static void PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1617 | `{` |
|      1 | 1618 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1619 | `	ph7_value_int(pVal,SXML_ERROR_RECURSIVE_ENTITY_REF);` |
|      3 | 1620 | `}` |
|      - | 1621 | `/*` |
|      - | 1622 | ` * XML_ERROR_ASYNC_ENTITY` |
|      - | 1623 | ` *   Expand the value of SXML_ERROR_ASYNC_ENTITY defined in ph7Int.h` |
|      - | 1624 | ` */` |
|      2 | 1625 | `static void PH7_XML_ERROR_ASYNC_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1626 | `{` |
|      1 | 1627 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1628 | `	ph7_value_int(pVal,SXML_ERROR_ASYNC_ENTITY);` |
|      3 | 1629 | `}` |
|      - | 1630 | `/*` |
|      - | 1631 | ` * XML_ERROR_BAD_CHAR_REF` |
|      - | 1632 | ` *   Expand the value of SXML_ERROR_BAD_CHAR_REF defined in ph7Int.h` |
|      - | 1633 | ` */` |
|      2 | 1634 | `static void PH7_XML_ERROR_BAD_CHAR_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1635 | `{` |
|      1 | 1636 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1637 | `	ph7_value_int(pVal,SXML_ERROR_BAD_CHAR_REF);` |
|      3 | 1638 | `}` |
|      - | 1639 | `/*` |
|      - | 1640 | ` * XML_ERROR_BINARY_ENTITY_REF` |
|      - | 1641 | ` *   Expand the value of SXML_ERROR_BINARY_ENTITY_REF defined in ph7Int.h` |
|      - | 1642 | ` */` |
|      2 | 1643 | `static void PH7_XML_ERROR_BINARY_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1644 | `{` |
|      1 | 1645 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1646 | `	ph7_value_int(pVal,SXML_ERROR_BINARY_ENTITY_REF);` |
|      3 | 1647 | `}` |
|      - | 1648 | `/*` |
|      - | 1649 | ` * XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF` |
|      - | 1650 | ` *   Expand the value of SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF defined in ph7Int.h` |
|      - | 1651 | ` */` |
|      2 | 1652 | `static void PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1653 | `{` |
|      1 | 1654 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1655 | `	ph7_value_int(pVal,SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF);` |
|      3 | 1656 | `}` |
|      - | 1657 | `/*` |
|      - | 1658 | ` * XML_ERROR_MISPLACED_XML_PI` |
|      - | 1659 | ` *   Expand the value of SXML_ERROR_MISPLACED_XML_PI defined in ph7Int.h` |
|      - | 1660 | ` */` |
|      2 | 1661 | `static void PH7_XML_ERROR_MISPLACED_XML_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1662 | `{` |
|      1 | 1663 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1664 | `	ph7_value_int(pVal,SXML_ERROR_MISPLACED_XML_PI);` |
|      3 | 1665 | `}` |
|      - | 1666 | `/*` |
|      - | 1667 | ` * XML_ERROR_UNKNOWN_ENCODING` |
|      - | 1668 | ` *   Expand the value of SXML_ERROR_UNKNOWN_ENCODING defined in ph7Int.h` |
|      - | 1669 | ` */` |
|      2 | 1670 | `static void PH7_XML_ERROR_UNKNOWN_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1671 | `{` |
|      1 | 1672 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1673 | `	ph7_value_int(pVal,SXML_ERROR_UNKNOWN_ENCODING);` |
|      3 | 1674 | `}` |
|      - | 1675 | `/*` |
|      - | 1676 | ` * XML_ERROR_INCORRECT_ENCODING` |
|      - | 1677 | ` *   Expand the value of SXML_ERROR_INCORRECT_ENCODING defined in ph7Int.h` |
|      - | 1678 | ` */` |
|      2 | 1679 | `static void PH7_XML_ERROR_INCORRECT_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1680 | `{` |
|      1 | 1681 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1682 | `	ph7_value_int(pVal,SXML_ERROR_INCORRECT_ENCODING);` |
|      3 | 1683 | `}` |
|      - | 1684 | `/*` |
|      - | 1685 | ` * XML_ERROR_UNCLOSED_CDATA_SECTION` |
|      - | 1686 | ` *   Expand the value of SXML_ERROR_UNCLOSED_CDATA_SECTION defined in ph7Int.h` |
|      - | 1687 | ` */` |
|      2 | 1688 | `static void PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1689 | `{` |
|      1 | 1690 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1691 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_CDATA_SECTION);` |
|      3 | 1692 | `}` |
|      - | 1693 | `/*` |
|      - | 1694 | ` * XML_ERROR_EXTERNAL_ENTITY_HANDLING` |
|      - | 1695 | ` *   Expand the value of SXML_ERROR_EXTERNAL_ENTITY_HANDLING defined in ph7Int.h` |
|      - | 1696 | ` */` |
|      2 | 1697 | `static void PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1698 | `{` |
|      1 | 1699 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1700 | `	ph7_value_int(pVal,SXML_ERROR_EXTERNAL_ENTITY_HANDLING);` |
|      3 | 1701 | `}` |
|      - | 1702 | `/*` |
|      - | 1703 | ` * XML_OPTION_CASE_FOLDING` |
|      - | 1704 | ` *   Expand the value of SXML_OPTION_CASE_FOLDING defined in ph7Int.h.` |
|      - | 1705 | ` */` |
|      2 | 1706 | `static void PH7_XML_OPTION_CASE_FOLDING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1707 | `{` |
|      1 | 1708 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1709 | `	ph7_value_int(pVal,SXML_OPTION_CASE_FOLDING);` |
|      3 | 1710 | `}` |
|      - | 1711 | `/*` |
|      - | 1712 | ` * XML_OPTION_TARGET_ENCODING` |
|      - | 1713 | ` *   Expand the value of SXML_OPTION_TARGET_ENCODING defined in ph7Int.h.` |
|      - | 1714 | ` */` |
|      4 | 1715 | `static void PH7_XML_OPTION_TARGET_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1716 | `{` |
|      2 | 1717 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1718 | `	ph7_value_int(pVal,SXML_OPTION_TARGET_ENCODING);` |
|      5 | 1719 | `}` |
|      - | 1720 | `/*` |
|      - | 1721 | ` * XML_OPTION_SKIP_TAGSTART` |
|      - | 1722 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1723 | ` */` |
|      2 | 1724 | `static void PH7_XML_OPTION_SKIP_TAGSTART_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1725 | `{` |
|      1 | 1726 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1727 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_TAGSTART);` |
|      3 | 1728 | `}` |
|      - | 1729 | `/*` |
|      - | 1730 | ` * XML_OPTION_SKIP_WHITE` |
|      - | 1731 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1732 | ` */` |
|      4 | 1733 | `static void PH7_XML_OPTION_SKIP_WHITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1734 | `{` |
|      2 | 1735 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1736 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_WHITE);` |
|      5 | 1737 | `}` |
|      - | 1738 | `/*` |
|      - | 1739 | ` * XML_SAX_IMPL.` |
|      - | 1740 | ` *   Expand the name of the underlying XML engine.` |
|      - | 1741 | ` */` |
|      2 | 1742 | `static void PH7_XML_SAX_IMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1743 | `{` |
|      1 | 1744 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1745 | `	ph7_value_string(pVal,"Symisc XML engine",(int)sizeof("Symisc XML engine")-1);` |
|      3 | 1746 | `}` |
|      - | 1747 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1748 | `/*` |
|      - | 1749 | ` * JSON_HEX_TAG.` |
|      - | 1750 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|      - | 1751 | ` */` |
|      2 | 1752 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1753 | `{` |
|      1 | 1754 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1755 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      3 | 1756 | `}` |
|      - | 1757 | `/*` |
|      - | 1758 | ` * JSON_HEX_AMP.` |
|      - | 1759 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|      - | 1760 | ` */` |
|      2 | 1761 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1762 | `{` |
|      1 | 1763 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1764 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      3 | 1765 | `}` |
|      - | 1766 | `/*` |
|      - | 1767 | ` * JSON_HEX_APOS.` |
|      - | 1768 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|      - | 1769 | ` */` |
|      2 | 1770 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1771 | `{` |
|      1 | 1772 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1773 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      3 | 1774 | `}` |
|      - | 1775 | `/*` |
|      - | 1776 | ` * JSON_HEX_QUOT.` |
|      - | 1777 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|      - | 1778 | ` */` |
|      2 | 1779 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1780 | `{` |
|      1 | 1781 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1782 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      3 | 1783 | `}` |
|      - | 1784 | `/*` |
|      - | 1785 | ` * JSON_FORCE_OBJECT.` |
|      - | 1786 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|      - | 1787 | ` */` |
|      4 | 1788 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1789 | `{` |
|      2 | 1790 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1791 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      5 | 1792 | `}` |
|      - | 1793 | `/*` |
|      - | 1794 | ` * JSON_NUMERIC_CHECK.` |
|      - | 1795 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|      - | 1796 | ` */` |
|      4 | 1797 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1798 | `{` |
|      2 | 1799 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1800 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      5 | 1801 | `}` |
|      - | 1802 | `/*` |
|      - | 1803 | ` * JSON_BIGINT_AS_STRING.` |
|      - | 1804 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|      - | 1805 | ` */` |
|      2 | 1806 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1807 | `{` |
|      1 | 1808 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1809 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      3 | 1810 | `}` |
|      - | 1811 | `/*` |
|      - | 1812 | ` * JSON_PRETTY_PRINT.` |
|      - | 1813 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|      - | 1814 | ` */` |
|      2 | 1815 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1816 | `{` |
|      1 | 1817 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1818 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      3 | 1819 | `}` |
|      - | 1820 | `/*` |
|      - | 1821 | ` * JSON_UNESCAPED_SLASHES.` |
|      - | 1822 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|      - | 1823 | ` */` |
|      4 | 1824 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1825 | `{` |
|      2 | 1826 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1827 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      5 | 1828 | `}` |
|      - | 1829 | `/*` |
|      - | 1830 | ` * JSON_UNESCAPED_UNICODE.` |
|      - | 1831 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|      - | 1832 | ` */` |
|      2 | 1833 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1834 | `{` |
|      1 | 1835 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1836 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|      3 | 1837 | `}` |
|      - | 1838 | `/*` |
|      - | 1839 | ` * JSON_ERROR_NONE.` |
|      - | 1840 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|      - | 1841 | ` */` |
|      4 | 1842 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1843 | `{` |
|      2 | 1844 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1845 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      5 | 1846 | `}` |
|      - | 1847 | `/*` |
|      - | 1848 | ` * JSON_ERROR_DEPTH.` |
|      - | 1849 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|      - | 1850 | ` */` |
|      2 | 1851 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1852 | `{` |
|      1 | 1853 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1854 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      3 | 1855 | `}` |
|      - | 1856 | `/*` |
|      - | 1857 | ` * JSON_ERROR_STATE_MISMATCH.` |
|      - | 1858 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|      - | 1859 | ` */` |
|      2 | 1860 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1861 | `{` |
|      1 | 1862 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1863 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      3 | 1864 | `}` |
|      - | 1865 | `/*` |
|      - | 1866 | ` * JSON_ERROR_CTRL_CHAR.` |
|      - | 1867 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|      - | 1868 | ` */` |
|      2 | 1869 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1870 | `{` |
|      1 | 1871 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1872 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      3 | 1873 | `}` |
|      - | 1874 | `/*` |
|      - | 1875 | ` * JSON_ERROR_SYNTAX.` |
|      - | 1876 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|      - | 1877 | ` */` |
|      4 | 1878 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1879 | `{` |
|      2 | 1880 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1881 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      5 | 1882 | `}` |
|      - | 1883 | `/*` |
|      - | 1884 | ` * JSON_ERROR_UTF8.` |
|      - | 1885 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|      - | 1886 | ` */` |
|      2 | 1887 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1888 | `{` |
|      1 | 1889 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1890 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      3 | 1891 | `}` |
|      - | 1892 | `/*` |
|      - | 1893 | ` * static` |
|      - | 1894 | ` *  Expand the name of the current class. 'static' otherwise.` |
|      - | 1895 | ` */` |
|      6 | 1896 | `static void PH7_static_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1897 | `{` |
|      7 | 1898 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1899 | `	ph7_class *pClass;` |
|      - | 1900 | `	/* Extract the target class if available */` |
|      7 | 1901 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      7 | 1902 | `	if( pClass ){` |
|      3 | 1903 | `		SyString *pName = &pClass->sName;` |
|      - | 1904 | `		/* Expand class name */` |
|      3 | 1905 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      2 | 1906 | `	}else{` |
|      - | 1907 | `		/* Expand 'static' */` |
|      5 | 1908 | `		ph7_value_string(pVal,"static",sizeof("static")-1);` |
|      - | 1909 | `	}` |
|      7 | 1910 | `}` |
|      - | 1911 | `/*` |
|      - | 1912 | ` * self` |
|      - | 1913 | ` * __CLASS__` |
|      - | 1914 | ` *  Expand the name of the current class. NULL otherwise.` |
|      - | 1915 | ` */` |
|      2 | 1916 | `static void PH7_self_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1917 | `{` |
|      3 | 1918 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1919 | `	ph7_class *pClass;` |
|      - | 1920 |  |
|      - | 1921 | `	/* Get the declaring class of the current method */` |
|      3 | 1922 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      3 | 1923 | `	if( pClass == 0 ){` |
|      - | 1924 | `		/* Not in a method, fall back to runtime class */` |
|      3 | 1925 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      1 | 1926 | `	}` |
|      - | 1927 |  |
|      3 | 1928 | `	if( pClass ){` |
|    ! 0 | 1929 | `		SyString *pName = &pClass->sName;` |
|      - | 1930 | `		/* Expand class name */` |
|    ! 0 | 1931 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|    ! 0 | 1932 | `	}else{` |
|      - | 1933 | `		/* Expand null */` |
|      3 | 1934 | `		ph7_value_null(pVal);` |
|      - | 1935 | `	}` |
|      3 | 1936 | `}` |
|      - | 1937 | `/* parent` |
|      - | 1938 | ` *  Expand the name of the parent class. NULL otherwise.` |
|      - | 1939 | ` */` |
|      2 | 1940 | `static void PH7_parent_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1941 | `{` |
|      3 | 1942 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1943 | `	ph7_class *pClass;` |
|      - | 1944 |  |
|      - | 1945 | `	/* Get the declaring class, then its parent */` |
|      3 | 1946 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      3 | 1947 | `	if( pClass && pClass->pBase ){` |
|    ! 0 | 1948 | `		SyString *pName = &pClass->pBase->sName;` |
|      - | 1949 | `		/* Expand parent class name */` |
|    ! 0 | 1950 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|    ! 0 | 1951 | `	}else{` |
|      - | 1952 | `		/* Expand null */` |
|      3 | 1953 | `		ph7_value_null(pVal);` |
|      - | 1954 | `	}` |
|      3 | 1955 | `}` |
|      - | 1956 |  |
|      - | 1957 | `/*` |
|      - | 1958 | ` * PASSWORD_BCRYPT / PASSWORD_DEFAULT` |
|      - | 1959 | ` *  The bcrypt algorithm identifier (PHP 7.4+ exposes these as the string "2y").` |
|      - | 1960 | ` *  PASSWORD_DEFAULT tracks the recommended default, currently bcrypt.` |
|      - | 1961 | ` */` |
|     20 | 1962 | `static void PH7_PASSWORD_BCRYPT_Const(ph7_value *pVal,void *pUnused)` |
|      2 | 1963 | `{` |
|     10 | 1964 | `	SXUNUSED(pUnused);` |
|     22 | 1965 | `	ph7_value_string(pVal,"2y",(int)sizeof("2y")-1);` |
|     22 | 1966 | `}` |
|      - | 1967 | `/*` |
|      - | 1968 | ` * PASSWORD_BCRYPT_DEFAULT_COST` |
|      - | 1969 | ` *  The default bcrypt work factor used by password_hash() (currently 12).` |
|      - | 1970 | ` */` |
|      2 | 1971 | `static void PH7_PASSWORD_COST_Const(ph7_value *pVal,void *pUnused)` |
|      1 | 1972 | `{` |
|      1 | 1973 | `	SXUNUSED(pUnused);` |
|      3 | 1974 | `	ph7_value_int(pVal,12);` |
|      3 | 1975 | `}` |
|      - | 1976 | `/*` |
|      - | 1977 | ` * filter_var() filter and flag identifiers (the ext/filter constants). Values` |
|      - | 1978 | ` * match PHP 8.5. One tiny int-returning callback per constant, generated by a` |
|      - | 1979 | ` * local macro to keep the ~25 near-identical definitions DRY.` |
|      - | 1980 | ` */` |
|      - | 1981 | `#define PH7_FILTER_INT_CONST(Name,Val) \` |
|      - | 1982 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|      - | 1983 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|      - | 1984 | `	}` |
|     10 | 1985 | `PH7_FILTER_INT_CONST(FILTER_DEFAULT,516)` |
|     17 | 1986 | `PH7_FILTER_INT_CONST(FILTER_UNSAFE_RAW,516)` |
|     64 | 1987 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_INT,257)` |
|     29 | 1988 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_BOOLEAN,258)` |
|     69 | 1989 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_FLOAT,259)` |
|      8 | 1990 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_REGEXP,272)` |
|     11 | 1991 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_DOMAIN,277)` |
|     15 | 1992 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_URL,273)` |
|     28 | 1993 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_EMAIL,274)` |
|     25 | 1994 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_IP,275)` |
|     11 | 1995 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_MAC,276)` |
|      3 | 1996 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_INT,519)` |
|      5 | 1997 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_FLOAT,520)` |
|     13 | 1998 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_SPECIAL_CHARS,515)` |
|     25 | 1999 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_FULL_SPECIAL_CHARS,522)` |
|      3 | 2000 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_EMAIL,517)` |
|      3 | 2001 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_URL,518)` |
|      3 | 2002 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_OCTAL,1)` |
|      3 | 2003 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_HEX,2)` |
|      7 | 2004 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_LOW,4)` |
|      5 | 2005 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_HIGH,8)` |
|      5 | 2006 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_LOW,16)` |
|      5 | 2007 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_HIGH,32)` |
|      3 | 2008 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_AMP,64)` |
|      3 | 2009 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_ENCODE_QUOTES,128)` |
|      3 | 2010 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_BACKTICK,512)` |
|      3 | 2011 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_FRACTION,4096)` |
|     25 | 2012 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_THOUSAND,8192)` |
|      3 | 2013 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_SCIENTIFIC,16384)` |
|      5 | 2014 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV4,1048576)` |
|      3 | 2015 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV6,2097152)` |
|     14 | 2016 | `PH7_FILTER_INT_CONST(FILTER_NULL_ON_FAILURE,134217728)` |
|      - | 2017 | `/* filter_input() source selectors (php values; SESSION/REQUEST are undefined in 8.5) */` |
|      5 | 2018 | `PH7_FILTER_INT_CONST(INPUT_POST,0)` |
|      8 | 2019 | `PH7_FILTER_INT_CONST(INPUT_GET,1)` |
|      3 | 2020 | `PH7_FILTER_INT_CONST(INPUT_COOKIE,2)` |
|      3 | 2021 | `PH7_FILTER_INT_CONST(INPUT_ENV,4)` |
|     21 | 2022 | `PH7_FILTER_INT_CONST(INPUT_SERVER,5)` |
|      - | 2023 | `/*` |
|      - | 2024 | ` * Table of built-in constants.` |
|      - | 2025 | ` */` |
|      - | 2026 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|      - | 2027 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|      - | 2028 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|      - | 2029 | `	{"__PH7__",              PH7_VER_Const      },` |
|      - | 2030 | `	{"PHP_VERSION",          PH7_PHPVerConst    },` |
|      - | 2031 | `	{"PHP_MAJOR_VERSION",    PH7_PHPMajorConst  },` |
|      - | 2032 | `	{"PHP_MINOR_VERSION",    PH7_PHPMinorConst  },` |
|      - | 2033 | `	{"PHP_RELEASE_VERSION",  PH7_PHPReleaseConst},` |
|      - | 2034 | `	{"PHP_EXTRA_VERSION",    PH7_PHPExtraConst  },` |
|      - | 2035 | `	{"PHP_VERSION_ID",       PH7_PHPVerIdConst  },` |
|      - | 2036 | `	{"PHP_OS",               PH7_OS_Const       },` |
|      - | 2037 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|      - | 2038 | `	{"PASSWORD_BCRYPT",      PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2039 | `	{"PASSWORD_DEFAULT",     PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2040 | `	{"PASSWORD_BCRYPT_DEFAULT_COST", PH7_PASSWORD_COST_Const },` |
|      - | 2041 | `	{"FILTER_DEFAULT",              PH7_FILTER_DEFAULT_Const },` |
|      - | 2042 | `	{"FILTER_UNSAFE_RAW",           PH7_FILTER_UNSAFE_RAW_Const },` |
|      - | 2043 | `	{"FILTER_VALIDATE_INT",         PH7_FILTER_VALIDATE_INT_Const },` |
|      - | 2044 | `	{"FILTER_VALIDATE_BOOLEAN",     PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2045 | `	{"FILTER_VALIDATE_BOOL",        PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2046 | `	{"FILTER_VALIDATE_FLOAT",       PH7_FILTER_VALIDATE_FLOAT_Const },` |
|      - | 2047 | `	{"FILTER_VALIDATE_REGEXP",      PH7_FILTER_VALIDATE_REGEXP_Const },` |
|      - | 2048 | `	{"FILTER_VALIDATE_DOMAIN",      PH7_FILTER_VALIDATE_DOMAIN_Const },` |
|      - | 2049 | `	{"FILTER_VALIDATE_URL",         PH7_FILTER_VALIDATE_URL_Const },` |
|      - | 2050 | `	{"FILTER_VALIDATE_EMAIL",       PH7_FILTER_VALIDATE_EMAIL_Const },` |
|      - | 2051 | `	{"FILTER_VALIDATE_IP",          PH7_FILTER_VALIDATE_IP_Const },` |
|      - | 2052 | `	{"FILTER_VALIDATE_MAC",         PH7_FILTER_VALIDATE_MAC_Const },` |
|      - | 2053 | `	{"FILTER_SANITIZE_NUMBER_INT",  PH7_FILTER_SANITIZE_NUMBER_INT_Const },` |
|      - | 2054 | `	{"FILTER_SANITIZE_NUMBER_FLOAT",PH7_FILTER_SANITIZE_NUMBER_FLOAT_Const },` |
|      - | 2055 | `	{"FILTER_SANITIZE_SPECIAL_CHARS",PH7_FILTER_SANITIZE_SPECIAL_CHARS_Const },` |
|      - | 2056 | `	{"FILTER_SANITIZE_FULL_SPECIAL_CHARS",PH7_FILTER_SANITIZE_FULL_SPECIAL_CHARS_Const },` |
|      - | 2057 | `	{"FILTER_SANITIZE_EMAIL",       PH7_FILTER_SANITIZE_EMAIL_Const },` |
|      - | 2058 | `	{"FILTER_SANITIZE_URL",         PH7_FILTER_SANITIZE_URL_Const },` |
|      - | 2059 | `	{"FILTER_FLAG_ALLOW_OCTAL",     PH7_FILTER_FLAG_ALLOW_OCTAL_Const },` |
|      - | 2060 | `	{"FILTER_FLAG_ALLOW_HEX",       PH7_FILTER_FLAG_ALLOW_HEX_Const },` |
|      - | 2061 | `	{"FILTER_FLAG_STRIP_LOW",       PH7_FILTER_FLAG_STRIP_LOW_Const },` |
|      - | 2062 | `	{"FILTER_FLAG_STRIP_HIGH",      PH7_FILTER_FLAG_STRIP_HIGH_Const },` |
|      - | 2063 | `	{"FILTER_FLAG_ENCODE_LOW",      PH7_FILTER_FLAG_ENCODE_LOW_Const },` |
|      - | 2064 | `	{"FILTER_FLAG_ENCODE_HIGH",     PH7_FILTER_FLAG_ENCODE_HIGH_Const },` |
|      - | 2065 | `	{"FILTER_FLAG_ENCODE_AMP",      PH7_FILTER_FLAG_ENCODE_AMP_Const },` |
|      - | 2066 | `	{"FILTER_FLAG_NO_ENCODE_QUOTES",PH7_FILTER_FLAG_NO_ENCODE_QUOTES_Const },` |
|      - | 2067 | `	{"FILTER_FLAG_STRIP_BACKTICK",  PH7_FILTER_FLAG_STRIP_BACKTICK_Const },` |
|      - | 2068 | `	{"FILTER_FLAG_ALLOW_FRACTION",  PH7_FILTER_FLAG_ALLOW_FRACTION_Const },` |
|      - | 2069 | `	{"FILTER_FLAG_ALLOW_THOUSAND",  PH7_FILTER_FLAG_ALLOW_THOUSAND_Const },` |
|      - | 2070 | `	{"FILTER_FLAG_ALLOW_SCIENTIFIC",PH7_FILTER_FLAG_ALLOW_SCIENTIFIC_Const },` |
|      - | 2071 | `	{"FILTER_FLAG_IPV4",            PH7_FILTER_FLAG_IPV4_Const },` |
|      - | 2072 | `	{"FILTER_FLAG_IPV6",            PH7_FILTER_FLAG_IPV6_Const },` |
|      - | 2073 | `	{"FILTER_NULL_ON_FAILURE",      PH7_FILTER_NULL_ON_FAILURE_Const },` |
|      - | 2074 | `	{"INPUT_POST",                  PH7_INPUT_POST_Const },` |
|      - | 2075 | `	{"INPUT_GET",                   PH7_INPUT_GET_Const },` |
|      - | 2076 | `	{"INPUT_COOKIE",                PH7_INPUT_COOKIE_Const },` |
|      - | 2077 | `	{"INPUT_ENV",                   PH7_INPUT_ENV_Const },` |
|      - | 2078 | `	{"INPUT_SERVER",                PH7_INPUT_SERVER_Const },` |
|      - | 2079 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|      - | 2080 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|      - | 2081 | `	{"PHP_INT_MIN",          PH7_INTMIN_Const   },` |
|      - | 2082 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|      - | 2083 | `	{"PHP_FLOAT_EPSILON",    PH7_FLOATEPSILON_Const },` |
|      - | 2084 | `	{"PHP_FLOAT_MAX",        PH7_FLOATMAX_Const },` |
|      - | 2085 | `	{"PHP_FLOAT_MIN",        PH7_FLOATMIN_Const },` |
|      - | 2086 | `	{"PHP_FLOAT_DIG",        PH7_FLOATDIG_Const },` |
|      - | 2087 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|      - | 2088 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|      - | 2089 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|      - | 2090 | `	{"__TIME__",             PH7_TIME_Const     },` |
|      - | 2091 | `	{"__DATE__",             PH7_DATE_Const     },` |
|      - | 2092 | `	{"__FILE__",             PH7_FILE_Const     },` |
|      - | 2093 | `	{"__DIR__",              PH7_DIR_Const      },` |
|      - | 2094 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|      - | 2095 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|      - | 2096 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|      - | 2097 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|      - | 2098 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|      - | 2099 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|      - | 2100 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|      - | 2101 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|      - | 2102 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|      - | 2103 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|      - | 2104 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|      - | 2105 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|      - | 2106 | `	{"E_STRICT",             PH7_E_STRICT_Const        },` |
|      - | 2107 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|      - | 2108 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|      - | 2109 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|      - | 2110 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|      - | 2111 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|      - | 2112 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|      - | 2113 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|      - | 2114 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|      - | 2115 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|      - | 2116 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|      - | 2117 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|      - | 2118 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|      - | 2119 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|      - | 2120 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|      - | 2121 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|      - | 2122 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|      - | 2123 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|      - | 2124 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|      - | 2125 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|      - | 2126 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|      - | 2127 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|      - | 2128 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|      - | 2129 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 2130 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|      - | 2131 | `	{"M_E",                  PH7_M_E_Const          },` |
|      - | 2132 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|      - | 2133 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|      - | 2134 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|      - | 2135 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|      - | 2136 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|      - | 2137 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|      - | 2138 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|      - | 2139 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|      - | 2140 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|      - | 2141 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|      - | 2142 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|      - | 2143 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|      - | 2144 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|      - | 2145 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|      - | 2146 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|      - | 2147 | `	{"NAN",                  PH7_NAN_Const          },` |
|      - | 2148 | `	{"INF",                  PH7_INF_Const          },` |
|      - | 2149 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 2150 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|      - | 2151 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|      - | 2152 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|      - | 2153 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|      - | 2154 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|      - | 2155 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|      - | 2156 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|      - | 2157 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|      - | 2158 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|      - | 2159 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|      - | 2160 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|      - | 2161 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|      - | 2162 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|      - | 2163 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|      - | 2164 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|      - | 2165 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|      - | 2166 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|      - | 2167 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|      - | 2168 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|      - | 2169 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|      - | 2170 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|      - | 2171 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|      - | 2172 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|      - | 2173 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|      - | 2174 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|      - | 2175 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|      - | 2176 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|      - | 2177 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|      - | 2178 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|      - | 2179 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|      - | 2180 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|      - | 2181 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|      - | 2182 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|      - | 2183 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|      - | 2184 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|      - | 2185 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|      - | 2186 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|      - | 2187 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|      - | 2188 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|      - | 2189 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|      - | 2190 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|      - | 2191 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|      - | 2192 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|      - | 2193 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|      - | 2194 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|      - | 2195 | `	{"ASSERT_ACTIVE",        PH7_ASSERT_ACTIVE_Const     },` |
|      - | 2196 | `	{"ASSERT_CALLBACK",      PH7_ASSERT_CALLBACK_Const   },` |
|      - | 2197 | `	{"ASSERT_BAIL",          PH7_ASSERT_BAIL_Const       },` |
|      - | 2198 | `	{"ASSERT_WARNING",       PH7_ASSERT_WARNING_Const    },` |
|      - | 2199 | `	{"ASSERT_EXCEPTION",     PH7_ASSERT_EXCEPTION_Const  },` |
|      - | 2200 | `	{"ASSERT_QUIET_EVAL",    PH7_ASSERT_QUIET_EVAL_Const },` |
|      - | 2201 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|      - | 2202 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|      - | 2203 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|      - | 2204 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|      - | 2205 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|      - | 2206 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|      - | 2207 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|      - | 2208 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|      - | 2209 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|      - | 2210 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|      - | 2211 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|      - | 2212 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|      - | 2213 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|      - | 2214 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|      - | 2215 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|      - | 2216 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|      - | 2217 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|      - | 2218 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|      - | 2219 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|      - | 2220 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|      - | 2221 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|      - | 2222 | `	{"STDIN",                PH7_STDIN_Const        },` |
|      - | 2223 | `	{"stdin",                PH7_STDIN_Const        },` |
|      - | 2224 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|      - | 2225 | `	{"stdout",               PH7_STDOUT_Const       },` |
|      - | 2226 | `	{"STDERR",               PH7_STDERR_Const       },` |
|      - | 2227 | `	{"stderr",               PH7_STDERR_Const       },` |
|      - | 2228 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|      - | 2229 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|      - | 2230 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|      - | 2231 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|      - | 2232 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|      - | 2233 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|      - | 2234 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|      - | 2235 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|      - | 2236 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|      - | 2237 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2238 | `	{"XML_ERROR_NONE",       PH7_XML_ERROR_NONE_Const},` |
|      - | 2239 | `	{"XML_ERROR_NO_MEMORY",  PH7_XML_ERROR_NO_MEMORY_Const},` |
|      - | 2240 | `	{"XML_ERROR_SYNTAX",     PH7_XML_ERROR_SYNTAX_Const},` |
|      - | 2241 | `	{"XML_ERROR_NO_ELEMENTS",PH7_XML_ERROR_NO_ELEMENTS_Const},` |
|      - | 2242 | `	{"XML_ERROR_INVALID_TOKEN", PH7_XML_ERROR_INVALID_TOKEN_Const},` |
|      - | 2243 | `	{"XML_ERROR_UNCLOSED_TOKEN",PH7_XML_ERROR_UNCLOSED_TOKEN_Const},` |
|      - | 2244 | `	{"XML_ERROR_PARTIAL_CHAR",  PH7_XML_ERROR_PARTIAL_CHAR_Const},` |
|      - | 2245 | `	{"XML_ERROR_TAG_MISMATCH",  PH7_XML_ERROR_TAG_MISMATCH_Const},` |
|      - | 2246 | `	{"XML_ERROR_DUPLICATE_ATTRIBUTE",   PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const},` |
|      - | 2247 | `	{"XML_ERROR_JUNK_AFTER_DOC_ELEMENT",PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const},` |
|      - | 2248 | `	{"XML_ERROR_PARAM_ENTITY_REF",      PH7_XML_ERROR_PARAM_ENTITY_REF_Const},` |
|      - | 2249 | `	{"XML_ERROR_UNDEFINED_ENTITY",      PH7_XML_ERROR_UNDEFINED_ENTITY_Const},` |
|      - | 2250 | `	{"XML_ERROR_RECURSIVE_ENTITY_REF",  PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const},` |
|      - | 2251 | `	{"XML_ERROR_ASYNC_ENTITY",          PH7_XML_ERROR_ASYNC_ENTITY_Const},` |
|      - | 2252 | `	{"XML_ERROR_BAD_CHAR_REF",          PH7_XML_ERROR_BAD_CHAR_REF_Const},` |
|      - | 2253 | `	{"XML_ERROR_BINARY_ENTITY_REF",     PH7_XML_ERROR_BINARY_ENTITY_REF_Const},` |
|      - | 2254 | `	{"XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF", PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const},` |
|      - | 2255 | `	{"XML_ERROR_MISPLACED_XML_PI",     PH7_XML_ERROR_MISPLACED_XML_PI_Const},` |
|      - | 2256 | `	{"XML_ERROR_UNKNOWN_ENCODING",     PH7_XML_ERROR_UNKNOWN_ENCODING_Const},` |
|      - | 2257 | `	{"XML_ERROR_INCORRECT_ENCODING",   PH7_XML_ERROR_INCORRECT_ENCODING_Const},` |
|      - | 2258 | `	{"XML_ERROR_UNCLOSED_CDATA_SECTION",  PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const},` |
|      - | 2259 | `	{"XML_ERROR_EXTERNAL_ENTITY_HANDLING",PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const},` |
|      - | 2260 | `	{"XML_OPTION_CASE_FOLDING",           PH7_XML_OPTION_CASE_FOLDING_Const},` |
|      - | 2261 | `	{"XML_OPTION_TARGET_ENCODING",        PH7_XML_OPTION_TARGET_ENCODING_Const},` |
|      - | 2262 | `	{"XML_OPTION_SKIP_TAGSTART",          PH7_XML_OPTION_SKIP_TAGSTART_Const},` |
|      - | 2263 | `	{"XML_OPTION_SKIP_WHITE",             PH7_XML_OPTION_SKIP_WHITE_Const},` |
|      - | 2264 | `	{"XML_SAX_IMPL",           PH7_XML_SAX_IMP_Const},` |
|      - | 2265 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2266 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|      - | 2267 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|      - | 2268 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|      - | 2269 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|      - | 2270 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|      - | 2271 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|      - | 2272 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|      - | 2273 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|      - | 2274 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|      - | 2275 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|      - | 2276 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|      - | 2277 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|      - | 2278 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|      - | 2279 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|      - | 2280 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|      - | 2281 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|      - | 2282 | `	{"static",               PH7_static_Const       },` |
|      - | 2283 | `	{"self",                 PH7_self_Const         },` |
|      - | 2284 | `	{"__CLASS__",            PH7_self_Const         },` |
|      - | 2285 | `	{"parent",               PH7_parent_Const       }` |
|      - | 2286 | `};` |
|      - | 2287 | `/*` |
|      - | 2288 | ` * Register the built-in constants defined above.` |
|      - | 2289 | ` */` |
|   3474 | 2290 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|      5 | 2291 | `{` |
|      - | 2292 | `	sxu32 n;` |
|      - | 2293 | `	/*` |
|      - | 2294 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|      - | 2295 | `	 * that trigger the constant invocation as their private data.` |
|      - | 2296 | `	 */` |
| 889349 | 2297 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 885875 | 2298 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
| 442940 | 2299 | `	}` |
|   3479 | 2300 | `}` |
