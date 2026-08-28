# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1139/1159 lines (98.27%)

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
|   3946 |   63 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|      5 |   64 | `{` |
|      - |   65 | `#if defined(__WINNT__)` |
|      5 |   66 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|      - |   67 | `#elif defined(__UNIXES__)` |
|      - |   68 | `	struct utsname sInfo;` |
|   3946 |   69 | `	if( uname(&sInfo) != 0 ){` |
|    ! 0 |   70 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 |   71 | `	}else{` |
|   3946 |   72 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|      - |   73 | `	}` |
|      - |   74 | `#else` |
|      - |   75 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|      - |   76 | `#endif` |
|   1973 |   77 | `	SXUNUSED(pUnused);` |
|   3951 |   78 | `}` |
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
|    162 |  149 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|      4 |  150 | `{` |
|     81 |  151 | `	SXUNUSED(pUnused);` |
|      - |  152 | `#ifdef __WINNT__` |
|      4 |  153 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|      - |  154 | `#else` |
|    162 |  155 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|      - |  156 | `#endif` |
|    166 |  157 | `}` |
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
|      - |  600 | ` * SPHP_ROUND_HALF_DOWN` |
|      - |  601 | ` *  Expands 2.` |
|      - |  602 | ` */` |
|      4 |  603 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  604 | `{` |
|      5 |  605 | `	ph7_value_int(pVal,2);` |
|      2 |  606 | `	SXUNUSED(pUserData);` |
|      5 |  607 | `}` |
|      - |  608 | `/*` |
|      - |  609 | ` * PHP_ROUND_HALF_EVEN` |
|      - |  610 | ` *  Expands 3.` |
|      - |  611 | ` */` |
|      8 |  612 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  613 | `{` |
|      9 |  614 | `	ph7_value_int(pVal,3);` |
|      4 |  615 | `	SXUNUSED(pUserData);` |
|      9 |  616 | `}` |
|      - |  617 | `/*` |
|      - |  618 | ` * PHP_ROUND_HALF_ODD` |
|      - |  619 | ` *  Expands 4.` |
|      - |  620 | ` */` |
|      4 |  621 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  622 | `{` |
|      5 |  623 | `	ph7_value_int(pVal,4);` |
|      2 |  624 | `	SXUNUSED(pUserData);` |
|      5 |  625 | `}` |
|      - |  626 | `/*` |
|      - |  627 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|      - |  628 | ` *  Expand 0x01` |
|      - |  629 | ` * NOTE:` |
|      - |  630 | ` *  The expanded value must be a power of two.` |
|      - |  631 | ` */` |
|      2 |  632 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  633 | `{` |
|      3 |  634 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      1 |  635 | `	SXUNUSED(pUserData);` |
|      3 |  636 | `}` |
|      - |  637 | `/*` |
|      - |  638 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|      - |  639 | ` *  Expand 0x02` |
|      - |  640 | ` * NOTE:` |
|      - |  641 | ` *  The expanded value must be a power of two.` |
|      - |  642 | ` */` |
|      2 |  643 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  644 | `{` |
|      3 |  645 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      1 |  646 | `	SXUNUSED(pUserData);` |
|      3 |  647 | `}` |
|      - |  648 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  649 | `/*` |
|      - |  650 | ` * M_PI` |
|      - |  651 | ` *  Expand the value of pi.` |
|      - |  652 | ` */` |
|      2 |  653 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  654 | `{` |
|      1 |  655 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  656 | `	ph7_value_double(pVal,PH7_PI);` |
|      3 |  657 | `}` |
|      - |  658 | `/*` |
|      - |  659 | ` * M_E` |
|      - |  660 | ` *  Expand 2.7182818284590452354` |
|      - |  661 | ` */` |
|      2 |  662 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  663 | `{` |
|      1 |  664 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  665 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|      3 |  666 | `}` |
|      - |  667 | `/*` |
|      - |  668 | ` * M_LOG2E` |
|      - |  669 | ` *  Expand 2.7182818284590452354` |
|      - |  670 | ` */` |
|      2 |  671 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  672 | `{` |
|      1 |  673 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  674 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      3 |  675 | `}` |
|      - |  676 | `/*` |
|      - |  677 | ` * M_LOG10E` |
|      - |  678 | ` *  Expand 0.4342944819032518276` |
|      - |  679 | ` */` |
|      2 |  680 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  681 | `{` |
|      1 |  682 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  683 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      3 |  684 | `}` |
|      - |  685 | `/*` |
|      - |  686 | ` * M_LN2` |
|      - |  687 | ` *  Expand 	0.69314718055994530942` |
|      - |  688 | ` */` |
|      2 |  689 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  690 | `{` |
|      1 |  691 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  692 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      3 |  693 | `}` |
|      - |  694 | `/*` |
|      - |  695 | ` * M_LN10` |
|      - |  696 | ` *  Expand 	2.30258509299404568402` |
|      - |  697 | ` */` |
|      2 |  698 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  699 | `{` |
|      1 |  700 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  701 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      3 |  702 | `}` |
|      - |  703 | `/*` |
|      - |  704 | ` * M_PI_2` |
|      - |  705 | ` *  Expand 	1.57079632679489661923` |
|      - |  706 | ` */` |
|      2 |  707 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  708 | `{` |
|      1 |  709 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  710 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      3 |  711 | `}` |
|      - |  712 | `/*` |
|      - |  713 | ` * M_PI_4` |
|      - |  714 | ` *  Expand 	0.78539816339744830962` |
|      - |  715 | ` */` |
|      2 |  716 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  717 | `{` |
|      1 |  718 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  719 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      3 |  720 | `}` |
|      - |  721 | `/*` |
|      - |  722 | ` * M_1_PI` |
|      - |  723 | ` *  Expand 	0.31830988618379067154` |
|      - |  724 | ` */` |
|      2 |  725 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  726 | `{` |
|      1 |  727 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  728 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      3 |  729 | `}` |
|      - |  730 | `/*` |
|      - |  731 | ` * M_2_PI` |
|      - |  732 | ` *  Expand 0.63661977236758134308` |
|      - |  733 | ` */` |
|      4 |  734 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  735 | `{` |
|      2 |  736 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  737 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      5 |  738 | `}` |
|      - |  739 | `/*` |
|      - |  740 | ` * M_SQRTPI` |
|      - |  741 | ` *  Expand 1.77245385090551602729` |
|      - |  742 | ` */` |
|      2 |  743 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  744 | `{` |
|      1 |  745 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  746 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      3 |  747 | `}` |
|      - |  748 | `/*` |
|      - |  749 | ` * M_2_SQRTPI` |
|      - |  750 | ` *  Expand 	1.12837916709551257390` |
|      - |  751 | ` */` |
|      2 |  752 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  753 | `{` |
|      1 |  754 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  755 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      3 |  756 | `}` |
|      - |  757 | `/*` |
|      - |  758 | ` * M_SQRT2` |
|      - |  759 | ` *  Expand 	1.41421356237309504880` |
|      - |  760 | ` */` |
|      2 |  761 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  762 | `{` |
|      1 |  763 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  764 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      3 |  765 | `}` |
|      - |  766 | `/*` |
|      - |  767 | ` * M_SQRT3` |
|      - |  768 | ` *  Expand 	1.73205080756887729352` |
|      - |  769 | ` */` |
|      2 |  770 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  771 | `{` |
|      1 |  772 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  773 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      3 |  774 | `}` |
|      - |  775 | `/*` |
|      - |  776 | ` * M_SQRT1_2` |
|      - |  777 | ` *  Expand 	0.70710678118654752440` |
|      - |  778 | ` */` |
|      2 |  779 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  780 | `{` |
|      1 |  781 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  782 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      3 |  783 | `}` |
|      - |  784 | `/*` |
|      - |  785 | ` * M_LNPI` |
|      - |  786 | ` *  Expand 	1.14472988584940017414` |
|      - |  787 | ` */` |
|      2 |  788 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  789 | `{` |
|      1 |  790 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  791 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      3 |  792 | `}` |
|      - |  793 | `/*` |
|      - |  794 | ` * M_EULER` |
|      - |  795 | ` *  Expand  0.57721566490153286061` |
|      - |  796 | ` */` |
|      2 |  797 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  798 | `{` |
|      1 |  799 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  800 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      3 |  801 | `}` |
|      - |  802 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|      - |  803 | `/*` |
|      - |  804 | ` * DATE_ATOM` |
|      - |  805 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|      - |  806 | ` */` |
|      2 |  807 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  808 | `{` |
|      1 |  809 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  810 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  811 | `}` |
|      - |  812 | `/*` |
|      - |  813 | ` * DATE_COOKIE` |
|      - |  814 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  815 | ` */` |
|      2 |  816 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  817 | `{` |
|      1 |  818 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  819 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  820 | `}` |
|      - |  821 | `/*` |
|      - |  822 | ` * DATE_ISO8601` |
|      - |  823 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|      - |  824 | ` */` |
|      2 |  825 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  826 | `{` |
|      1 |  827 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  828 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      3 |  829 | `}` |
|      - |  830 | `/*` |
|      - |  831 | ` * DATE_RFC822` |
|      - |  832 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|      - |  833 | ` */` |
|      2 |  834 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  835 | `{` |
|      1 |  836 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  837 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  838 | `}` |
|      - |  839 | `/*` |
|      - |  840 | ` * DATE_RFC850` |
|      - |  841 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  842 | ` */` |
|      2 |  843 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  844 | `{` |
|      1 |  845 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  846 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  847 | `}` |
|      - |  848 | `/*` |
|      - |  849 | ` * DATE_RFC1036` |
|      - |  850 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  851 | ` */` |
|      2 |  852 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  853 | `{` |
|      1 |  854 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  855 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  856 | `}` |
|      - |  857 | `/*` |
|      - |  858 | ` * DATE_RFC1123` |
|      - |  859 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  860 | ` */` |
|      2 |  861 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  862 | `{` |
|      1 |  863 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  864 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  865 | `}` |
|      - |  866 | `/*` |
|      - |  867 | ` * DATE_RFC2822` |
|      - |  868 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  869 | ` */` |
|      2 |  870 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  871 | `{` |
|      1 |  872 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  873 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  874 | `}` |
|      - |  875 | `/*` |
|      - |  876 | ` * DATE_RSS` |
|      - |  877 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  878 | ` */` |
|      2 |  879 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  880 | `{` |
|      1 |  881 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  882 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  883 | `}` |
|      - |  884 | `/*` |
|      - |  885 | ` * DATE_W3C` |
|      - |  886 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|      - |  887 | ` */` |
|      2 |  888 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  889 | `{` |
|      1 |  890 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  891 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  892 | `}` |
|      - |  893 | `/*` |
|      - |  894 | ` * The ENT_* values are PHP-exact (php 8.5.7). The low two bits are the quote` |
|      - |  895 | ` * bits (1 = single, 2 = double), so ENT_QUOTES = ENT_COMPAT\|1 and` |
|      - |  896 | ` * ENT_NOQUOTES = 0. Bits 16\|32 select the doctype (0 = HTML401, 16 = XML1,` |
|      - |  897 | ` * 32 = XHTML, 48 = HTML5) — composites, not flags.` |
|      - |  898 | ` */` |
|      - |  899 | `/*` |
|      - |  900 | ` * ENT_COMPAT` |
|      - |  901 | ` *  Expand 2 (double-quote bit only)` |
|      - |  902 | ` */` |
|     12 |  903 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  904 | `{` |
|      6 |  905 | `	SXUNUSED(pUserData); /* cc warning */` |
|     13 |  906 | `	ph7_value_int(pVal,PH7_ENT_QUOTE_DOUBLE);` |
|     13 |  907 | `}` |
|      - |  908 | `/*` |
|      - |  909 | ` * ENT_QUOTES` |
|      - |  910 | ` *  Expand 3 (double\|single quote bits)` |
|      - |  911 | ` */` |
|     60 |  912 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  913 | `{` |
|     30 |  914 | `	SXUNUSED(pUserData); /* cc warning */` |
|     61 |  915 | `	ph7_value_int(pVal,PH7_ENT_QUOTES);` |
|     61 |  916 | `}` |
|      - |  917 | `/*` |
|      - |  918 | ` * ENT_NOQUOTES` |
|      - |  919 | ` *  Expand 0 (no quote bits)` |
|      - |  920 | ` */` |
|     20 |  921 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  922 | `{` |
|     10 |  923 | `	SXUNUSED(pUserData); /* cc warning */` |
|     21 |  924 | `	ph7_value_int(pVal,0);` |
|     21 |  925 | `}` |
|      - |  926 | `/*` |
|      - |  927 | ` * ENT_IGNORE` |
|      - |  928 | ` *  Expand 4` |
|      - |  929 | ` */` |
|      6 |  930 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  931 | `{` |
|      3 |  932 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 |  933 | `	ph7_value_int(pVal,PH7_ENT_IGNORE);` |
|      7 |  934 | `}` |
|      - |  935 | `/*` |
|      - |  936 | ` * ENT_SUBSTITUTE` |
|      - |  937 | ` *  Expand 8` |
|      - |  938 | ` */` |
|      2 |  939 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  940 | `{` |
|      1 |  941 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  942 | `	ph7_value_int(pVal,PH7_ENT_SUBSTITUTE);` |
|      3 |  943 | `}` |
|      - |  944 | `/*` |
|      - |  945 | ` * ENT_DISALLOWED` |
|      - |  946 | ` *  Expand 128` |
|      - |  947 | ` */` |
|      2 |  948 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  949 | `{` |
|      1 |  950 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  951 | `	ph7_value_int(pVal,PH7_ENT_DISALLOWED);` |
|      3 |  952 | `}` |
|      - |  953 | `/*` |
|      - |  954 | ` * ENT_HTML401` |
|      - |  955 | ` *  Expand 0 (the default doctype)` |
|      - |  956 | ` */` |
|      2 |  957 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  958 | `{` |
|      1 |  959 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  960 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML401);` |
|      3 |  961 | `}` |
|      - |  962 | `/*` |
|      - |  963 | ` * ENT_XML1` |
|      - |  964 | ` *  Expand 16` |
|      - |  965 | ` */` |
|      8 |  966 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  967 | `{` |
|      4 |  968 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 |  969 | `	ph7_value_int(pVal,PH7_ENT_DOC_XML1);` |
|      9 |  970 | `}` |
|      - |  971 | `/*` |
|      - |  972 | ` * ENT_XHTML` |
|      - |  973 | ` *  Expand 32` |
|      - |  974 | ` */` |
|      6 |  975 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  976 | `{` |
|      3 |  977 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 |  978 | `	ph7_value_int(pVal,PH7_ENT_DOC_XHTML);` |
|      7 |  979 | `}` |
|      - |  980 | `/*` |
|      - |  981 | ` * ENT_HTML5` |
|      - |  982 | ` *  Expand 48 (16\|32 — a doctype composite, not a flag bit)` |
|      - |  983 | ` */` |
|      8 |  984 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  985 | `{` |
|      4 |  986 | `	SXUNUSED(pUserData); /* cc warning */` |
|      9 |  987 | `	ph7_value_int(pVal,PH7_ENT_DOC_HTML5);` |
|      9 |  988 | `}` |
|      - |  989 | `/*` |
|      - |  990 | ` * ISO-8859-1` |
|      - |  991 | ` * ISO_8859_1` |
|      - |  992 | ` *   Expand 1` |
|      - |  993 | ` */` |
|      2 |  994 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  995 | `{` |
|      1 |  996 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  997 | `	ph7_value_int(pVal,1);` |
|      3 |  998 | `}` |
|      - |  999 | `/*` |
|      - | 1000 | ` * UTF-8` |
|      - | 1001 | ` * UTF8` |
|      - | 1002 | ` *  Expand 2` |
|      - | 1003 | ` */` |
|      2 | 1004 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1005 | `{` |
|      1 | 1006 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1007 | `	ph7_value_int(pVal,1);` |
|      3 | 1008 | `}` |
|      - | 1009 | `/*` |
|      - | 1010 | ` * HTML_ENTITIES` |
|      - | 1011 | ` *  Expand 1` |
|      - | 1012 | ` */` |
|      4 | 1013 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1014 | `{` |
|      2 | 1015 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1016 | `	ph7_value_int(pVal,1);` |
|      5 | 1017 | `}` |
|      - | 1018 | `/*` |
|      - | 1019 | ` * HTML_SPECIALCHARS` |
|      - | 1020 | ` *  Expand 0 (PHP-exact)` |
|      - | 1021 | ` */` |
|     10 | 1022 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1023 | `{` |
|      5 | 1024 | `	SXUNUSED(pUserData); /* cc warning */` |
|     11 | 1025 | `	ph7_value_int(pVal,0);` |
|     11 | 1026 | `}` |
|      - | 1027 | `/*` |
|      - | 1028 | ` * PHP_URL_SCHEME.` |
|      - | 1029 | ` * Expand 1` |
|      - | 1030 | ` */` |
|      2 | 1031 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1032 | `{` |
|      1 | 1033 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1034 | `	ph7_value_int(pVal,1);` |
|      3 | 1035 | `}` |
|      - | 1036 | `/*` |
|      - | 1037 | ` * PHP_URL_HOST.` |
|      - | 1038 | ` * Expand 2` |
|      - | 1039 | ` */` |
|      2 | 1040 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1041 | `{` |
|      1 | 1042 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1043 | `	ph7_value_int(pVal,2);` |
|      3 | 1044 | `}` |
|      - | 1045 | `/*` |
|      - | 1046 | ` * PHP_URL_PORT.` |
|      - | 1047 | ` * Expand 3` |
|      - | 1048 | ` */` |
|      2 | 1049 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1050 | `{` |
|      1 | 1051 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1052 | `	ph7_value_int(pVal,3);` |
|      3 | 1053 | `}` |
|      - | 1054 | `/*` |
|      - | 1055 | ` * PHP_URL_USER.` |
|      - | 1056 | ` * Expand 4` |
|      - | 1057 | ` */` |
|      2 | 1058 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1059 | `{` |
|      1 | 1060 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1061 | `	ph7_value_int(pVal,4);` |
|      3 | 1062 | `}` |
|      - | 1063 | `/*` |
|      - | 1064 | ` * PHP_URL_PASS.` |
|      - | 1065 | ` * Expand 5` |
|      - | 1066 | ` */` |
|      2 | 1067 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1068 | `{` |
|      1 | 1069 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1070 | `	ph7_value_int(pVal,5);` |
|      3 | 1071 | `}` |
|      - | 1072 | `/*` |
|      - | 1073 | ` * PHP_URL_PATH.` |
|      - | 1074 | ` * Expand 6` |
|      - | 1075 | ` */` |
|      2 | 1076 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1077 | `{` |
|      1 | 1078 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1079 | `	ph7_value_int(pVal,6);` |
|      3 | 1080 | `}` |
|      - | 1081 | `/*` |
|      - | 1082 | ` * PHP_URL_QUERY.` |
|      - | 1083 | ` * Expand 7` |
|      - | 1084 | ` */` |
|      2 | 1085 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1086 | `{` |
|      1 | 1087 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1088 | `	ph7_value_int(pVal,7);` |
|      3 | 1089 | `}` |
|      - | 1090 | `/*` |
|      - | 1091 | ` * PHP_URL_FRAGMENT.` |
|      - | 1092 | ` * Expand 8` |
|      - | 1093 | ` */` |
|      2 | 1094 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1095 | `{` |
|      1 | 1096 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1097 | `	ph7_value_int(pVal,8);` |
|      3 | 1098 | `}` |
|      - | 1099 | `/*` |
|      - | 1100 | ` * PHP_QUERY_RFC1738` |
|      - | 1101 | ` * Expand 1` |
|      - | 1102 | ` */` |
|      2 | 1103 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1104 | `{` |
|      1 | 1105 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1106 | `	ph7_value_int(pVal,1);` |
|      3 | 1107 | `}` |
|      - | 1108 | `/*` |
|      - | 1109 | ` * PHP_QUERY_RFC3986` |
|      - | 1110 | ` * Expand 1` |
|      - | 1111 | ` */` |
|      2 | 1112 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1113 | `{` |
|      1 | 1114 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1115 | `	ph7_value_int(pVal,2);` |
|      3 | 1116 | `}` |
|      - | 1117 | `/*` |
|      - | 1118 | ` * FNM_NOESCAPE` |
|      - | 1119 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1120 | ` */` |
|      2 | 1121 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1122 | `{` |
|      1 | 1123 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1124 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1125 | `}` |
|      - | 1126 | `/*` |
|      - | 1127 | ` * FNM_PATHNAME` |
|      - | 1128 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1129 | ` */` |
|      2 | 1130 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1131 | `{` |
|      1 | 1132 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1133 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1134 | `}` |
|      - | 1135 | `/*` |
|      - | 1136 | ` * FNM_PERIOD` |
|      - | 1137 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1138 | ` */` |
|      6 | 1139 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1140 | `{` |
|      3 | 1141 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1142 | `	ph7_value_int(pVal,0x04);` |
|      7 | 1143 | `}` |
|      - | 1144 | `/*` |
|      - | 1145 | ` * FNM_CASEFOLD` |
|      - | 1146 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1147 | ` */` |
|      4 | 1148 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1149 | `{` |
|      2 | 1150 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1151 | `	ph7_value_int(pVal,0x08);` |
|      5 | 1152 | `}` |
|      - | 1153 | `/*` |
|      - | 1154 | ` * PATHINFO_DIRNAME` |
|      - | 1155 | ` *  Expand 1.` |
|      - | 1156 | ` */` |
|      4 | 1157 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1158 | `{` |
|      2 | 1159 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1160 | `	ph7_value_int(pVal,1);` |
|      5 | 1161 | `}` |
|      - | 1162 | `/*` |
|      - | 1163 | ` * PATHINFO_BASENAME` |
|      - | 1164 | ` *  Expand 2.` |
|      - | 1165 | ` */` |
|      4 | 1166 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1167 | `{` |
|      2 | 1168 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1169 | `	ph7_value_int(pVal,2);` |
|      5 | 1170 | `}` |
|      - | 1171 | `/*` |
|      - | 1172 | ` * PATHINFO_EXTENSION` |
|      - | 1173 | ` *  Expand 3.` |
|      - | 1174 | ` */` |
|   6494 | 1175 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1176 | `{` |
|   3247 | 1177 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6499 | 1178 | `	ph7_value_int(pVal,3);` |
|   6499 | 1179 | `}` |
|      - | 1180 | `/*` |
|      - | 1181 | ` * PATHINFO_FILENAME` |
|      - | 1182 | ` *  Expand 4.` |
|      - | 1183 | ` */` |
|   6486 | 1184 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1185 | `{` |
|   3243 | 1186 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6491 | 1187 | `	ph7_value_int(pVal,4);` |
|   6491 | 1188 | `}` |
|      - | 1189 | `/*` |
|      - | 1190 | ` * ASSERT_ACTIVE.` |
|      - | 1191 | ` *  PHP ASSERT_ACTIVE = 1` |
|      - | 1192 | ` */` |
|     14 | 1193 | `static void PH7_ASSERT_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1194 | `{` |
|      7 | 1195 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1196 | `	ph7_value_int(pVal,1); /* PHP ASSERT_ACTIVE = 1 */` |
|     16 | 1197 | `}` |
|      - | 1198 | `/*` |
|      - | 1199 | ` * ASSERT_CALLBACK.` |
|      - | 1200 | ` *  PHP ASSERT_CALLBACK = 2` |
|      - | 1201 | ` */` |
|      6 | 1202 | `static void PH7_ASSERT_CALLBACK_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1203 | `{` |
|      3 | 1204 | `	SXUNUSED(pUserData); /* cc warning */` |
|      8 | 1205 | `	ph7_value_int(pVal,2); /* PHP ASSERT_CALLBACK = 2 */` |
|      8 | 1206 | `}` |
|      - | 1207 | `/*` |
|      - | 1208 | ` * ASSERT_BAIL.` |
|      - | 1209 | ` *  PHP ASSERT_BAIL = 3` |
|      - | 1210 | ` */` |
|     14 | 1211 | `static void PH7_ASSERT_BAIL_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1212 | `{` |
|      7 | 1213 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1214 | `	ph7_value_int(pVal,3); /* PHP ASSERT_BAIL = 3 */` |
|     16 | 1215 | `}` |
|      - | 1216 | `/*` |
|      - | 1217 | ` * ASSERT_WARNING.` |
|      - | 1218 | ` *  PHP ASSERT_WARNING = 4 (deprecated in PHP 8.3)` |
|      - | 1219 | ` */` |
|      4 | 1220 | `static void PH7_ASSERT_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1221 | `{` |
|      2 | 1222 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1223 | `	ph7_value_int(pVal,4); /* PHP ASSERT_WARNING = 4 */` |
|      5 | 1224 | `}` |
|      - | 1225 | `/*` |
|      - | 1226 | ` * ASSERT_EXCEPTION.` |
|      - | 1227 | ` *  PHP ASSERT_EXCEPTION = 5 (deprecated in PHP 8.3)` |
|      - | 1228 | ` */` |
|      4 | 1229 | `static void PH7_ASSERT_EXCEPTION_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1230 | `{` |
|      2 | 1231 | `	SXUNUSED(pUserData); /* cc warning */` |
|      6 | 1232 | `	ph7_value_int(pVal,5); /* PHP ASSERT_EXCEPTION = 5 */` |
|      6 | 1233 | `}` |
|      - | 1234 | `/*` |
|      - | 1235 | ` * ASSERT_QUIET_EVAL.` |
|      - | 1236 | ` *  Removed in PHP 8.0, kept for compatibility.` |
|      - | 1237 | ` */` |
|      2 | 1238 | `static void PH7_ASSERT_QUIET_EVAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1239 | `{` |
|      1 | 1240 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1241 | `	ph7_value_int(pVal,6); /* Arbitrary value, removed in PHP 8 */` |
|      3 | 1242 | `}` |
|      - | 1243 | `/*` |
|      - | 1244 | ` * SEEK_SET.` |
|      - | 1245 | ` *  Expand 0` |
|      - | 1246 | ` */` |
|      2 | 1247 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1248 | `{` |
|      1 | 1249 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1250 | `	ph7_value_int(pVal,0);` |
|      3 | 1251 | `}` |
|      - | 1252 | `/*` |
|      - | 1253 | ` * SEEK_CUR.` |
|      - | 1254 | ` *  Expand 1` |
|      - | 1255 | ` */` |
|      2 | 1256 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1257 | `{` |
|      1 | 1258 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1259 | `	ph7_value_int(pVal,1);` |
|      3 | 1260 | `}` |
|      - | 1261 | `/*` |
|      - | 1262 | ` * SEEK_END.` |
|      - | 1263 | ` *  Expand 2` |
|      - | 1264 | ` */` |
|      2 | 1265 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1266 | `{` |
|      1 | 1267 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1268 | `	ph7_value_int(pVal,2);` |
|      3 | 1269 | `}` |
|      - | 1270 | `/*` |
|      - | 1271 | ` * LOCK_SH.` |
|      - | 1272 | ` *  Expand 2` |
|      - | 1273 | ` */` |
|      2 | 1274 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1275 | `{` |
|      1 | 1276 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1277 | `	ph7_value_int(pVal,1);` |
|      3 | 1278 | `}` |
|      - | 1279 | `/*` |
|      - | 1280 | ` * LOCK_NB.` |
|      - | 1281 | ` *  Expand 5` |
|      - | 1282 | ` */` |
|      2 | 1283 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1284 | `{` |
|      1 | 1285 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1286 | `	ph7_value_int(pVal,5);` |
|      3 | 1287 | `}` |
|      - | 1288 | `/*` |
|      - | 1289 | ` * LOCK_EX.` |
|      - | 1290 | ` *  Expand 0x01 (MUST BE A POWER OF TWO)` |
|      - | 1291 | ` */` |
|      4 | 1292 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1293 | `{` |
|      2 | 1294 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1295 | `	ph7_value_int(pVal,0x01);` |
|      5 | 1296 | `}` |
|      - | 1297 | `/*` |
|      - | 1298 | ` * LOCK_UN.` |
|      - | 1299 | ` *  Expand 0` |
|      - | 1300 | ` */` |
|      4 | 1301 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1302 | `{` |
|      2 | 1303 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1304 | `	ph7_value_int(pVal,0);` |
|      5 | 1305 | `}` |
|      - | 1306 | `/*` |
|      - | 1307 | ` * FILE_USE_INCLUDE_PATH` |
|      - | 1308 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1309 | ` */` |
|      2 | 1310 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1311 | `{` |
|      1 | 1312 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1313 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1314 | `}` |
|      - | 1315 | `/*` |
|      - | 1316 | ` * FILE_IGNORE_NEW_LINES` |
|      - | 1317 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1318 | ` */` |
|      2 | 1319 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1320 | `{` |
|      1 | 1321 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1322 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1323 | `}` |
|      - | 1324 | `/*` |
|      - | 1325 | ` * FILE_SKIP_EMPTY_LINES` |
|      - | 1326 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1327 | ` */` |
|      2 | 1328 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1329 | `{` |
|      1 | 1330 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1331 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1332 | `}` |
|      - | 1333 | `/*` |
|      - | 1334 | ` * FILE_APPEND` |
|      - | 1335 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1336 | ` */` |
|      2 | 1337 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1338 | `{` |
|      1 | 1339 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1340 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1341 | `}` |
|      - | 1342 | `/*` |
|      - | 1343 | ` * SCANDIR_SORT_ASCENDING` |
|      - | 1344 | ` *  Expand 0` |
|      - | 1345 | ` */` |
|   1974 | 1346 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1347 | `{` |
|    987 | 1348 | `	SXUNUSED(pUserData); /* cc warning */` |
|   1979 | 1349 | `	ph7_value_int(pVal,0);` |
|   1979 | 1350 | `}` |
|      - | 1351 | `/*` |
|      - | 1352 | ` * SCANDIR_SORT_DESCENDING` |
|      - | 1353 | ` *  Expand 1` |
|      - | 1354 | ` */` |
|    988 | 1355 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1356 | `{` |
|    494 | 1357 | `	SXUNUSED(pUserData); /* cc warning */` |
|    993 | 1358 | `	ph7_value_int(pVal,1);` |
|    993 | 1359 | `}` |
|      - | 1360 | `/*` |
|      - | 1361 | ` * SCANDIR_SORT_NONE` |
|      - | 1362 | ` *  Expand 2` |
|      - | 1363 | ` */` |
|      2 | 1364 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1365 | `{` |
|      1 | 1366 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1367 | `	ph7_value_int(pVal,2);` |
|      3 | 1368 | `}` |
|      - | 1369 | `/*` |
|      - | 1370 | ` * GLOB_MARK` |
|      - | 1371 | ` *  Expand 0x01 (must be a power of two)` |
|      - | 1372 | ` */` |
|      2 | 1373 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1374 | `{` |
|      1 | 1375 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1376 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1377 | `}` |
|      - | 1378 | `/*` |
|      - | 1379 | ` * GLOB_NOSORT` |
|      - | 1380 | ` *  Expand 0x02 (must be a power of two)` |
|      - | 1381 | ` */` |
|      2 | 1382 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1383 | `{` |
|      1 | 1384 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1385 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1386 | `}` |
|      - | 1387 | `/*` |
|      - | 1388 | ` * GLOB_NOCHECK` |
|      - | 1389 | ` *  Expand 0x04 (must be a power of two)` |
|      - | 1390 | ` */` |
|      2 | 1391 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1392 | `{` |
|      1 | 1393 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1394 | `	ph7_value_int(pVal,0x04);` |
|      3 | 1395 | `}` |
|      - | 1396 | `/*` |
|      - | 1397 | ` * GLOB_NOESCAPE` |
|      - | 1398 | ` *  Expand 0x08 (must be a power of two)` |
|      - | 1399 | ` */` |
|      2 | 1400 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1401 | `{` |
|      1 | 1402 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1403 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1404 | `}` |
|      - | 1405 | `/*` |
|      - | 1406 | ` * GLOB_BRACE` |
|      - | 1407 | ` *  Expand 0x10 (must be a power of two)` |
|      - | 1408 | ` */` |
|      2 | 1409 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1410 | `{` |
|      1 | 1411 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1412 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1413 | `}` |
|      - | 1414 | `/*` |
|      - | 1415 | ` * GLOB_ONLYDIR` |
|      - | 1416 | ` *  Expand 0x20 (must be a power of two)` |
|      - | 1417 | ` */` |
|      2 | 1418 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1419 | `{` |
|      1 | 1420 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1421 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1422 | `}` |
|      - | 1423 | `/*` |
|      - | 1424 | ` * GLOB_ERR` |
|      - | 1425 | ` *  Expand 0x40 (must be a power of two)` |
|      - | 1426 | ` */` |
|      2 | 1427 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1428 | `{` |
|      1 | 1429 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1430 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1431 | `}` |
|      - | 1432 | `/*` |
|      - | 1433 | ` * STDIN` |
|      - | 1434 | ` *  Expand the STDIN handle as a resource.` |
|      - | 1435 | ` */` |
|      2 | 1436 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1437 | `{` |
|      3 | 1438 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1439 | `	void *pResource;` |
|      3 | 1440 | `	pResource = PH7_ExportStdin(pVm);` |
|      3 | 1441 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1442 | `}` |
|      - | 1443 | `/*` |
|      - | 1444 | ` * STDOUT` |
|      - | 1445 | ` *   Expand the STDOUT handle as a resource.` |
|      - | 1446 | ` */` |
|      2 | 1447 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1448 | `{` |
|      3 | 1449 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1450 | `	void *pResource;` |
|      3 | 1451 | `	pResource = PH7_ExportStdout(pVm);` |
|      3 | 1452 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1453 | `}` |
|      - | 1454 | `/*` |
|      - | 1455 | ` * STDERR` |
|      - | 1456 | ` *  Expand the STDERR handle as a resource.` |
|      - | 1457 | ` */` |
|      2 | 1458 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1459 | `{` |
|      3 | 1460 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1461 | `	void *pResource;` |
|      3 | 1462 | `	pResource = PH7_ExportStderr(pVm);` |
|      3 | 1463 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1464 | `}` |
|      - | 1465 | `/*` |
|      - | 1466 | ` * INI_SCANNER_NORMAL` |
|      - | 1467 | ` *   Expand 1` |
|      - | 1468 | ` */` |
|      2 | 1469 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1470 | `{` |
|      1 | 1471 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1472 | `	ph7_value_int(pVal,1);` |
|      3 | 1473 | `}` |
|      - | 1474 | `/*` |
|      - | 1475 | ` * INI_SCANNER_RAW` |
|      - | 1476 | ` *   Expand 2` |
|      - | 1477 | ` */` |
|      2 | 1478 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1479 | `{` |
|      1 | 1480 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1481 | `	ph7_value_int(pVal,2);` |
|      3 | 1482 | `}` |
|      - | 1483 | `/*` |
|      - | 1484 | ` * EXTR_OVERWRITE` |
|      - | 1485 | ` *   Expand 0x01 (Must be a power of two)` |
|      - | 1486 | ` */` |
|      2 | 1487 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1488 | `{` |
|      1 | 1489 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1490 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1491 | `}` |
|      - | 1492 | `/*` |
|      - | 1493 | ` * EXTR_SKIP` |
|      - | 1494 | ` *   Expand 0x02 (Must be a power of two)` |
|      - | 1495 | ` */` |
|      2 | 1496 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1497 | `{` |
|      1 | 1498 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1499 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1500 | `}` |
|      - | 1501 | `/*` |
|      - | 1502 | ` * EXTR_PREFIX_SAME` |
|      - | 1503 | ` *   Expand 0x04 (Must be a power of two)` |
|      - | 1504 | ` */` |
|      2 | 1505 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1506 | `{` |
|      1 | 1507 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1508 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1509 | `}` |
|      - | 1510 | `/*` |
|      - | 1511 | ` * EXTR_PREFIX_ALL` |
|      - | 1512 | ` *   Expand 0x08 (Must be a power of two)` |
|      - | 1513 | ` */` |
|      2 | 1514 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1515 | `{` |
|      1 | 1516 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1517 | `	ph7_value_int(pVal,0x8);` |
|      3 | 1518 | `}` |
|      - | 1519 | `/*` |
|      - | 1520 | ` * EXTR_PREFIX_INVALID` |
|      - | 1521 | ` *   Expand 0x10 (Must be a power of two)` |
|      - | 1522 | ` */` |
|      2 | 1523 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1524 | `{` |
|      1 | 1525 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1526 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1527 | `}` |
|      - | 1528 | `/*` |
|      - | 1529 | ` * EXTR_IF_EXISTS` |
|      - | 1530 | ` *   Expand 0x20 (Must be a power of two)` |
|      - | 1531 | ` */` |
|      2 | 1532 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1533 | `{` |
|      1 | 1534 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1535 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1536 | `}` |
|      - | 1537 | `/*` |
|      - | 1538 | ` * EXTR_PREFIX_IF_EXISTS` |
|      - | 1539 | ` *   Expand 0x40 (Must be a power of two)` |
|      - | 1540 | ` */` |
|      2 | 1541 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1542 | `{` |
|      1 | 1543 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1544 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1545 | `}` |
|      - | 1546 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1547 | `/*` |
|      - | 1548 | ` * XML_ERROR_NONE` |
|      - | 1549 | ` *   Expand the value of SXML_ERROR_NO_MEMORY defined in ph7Int.h` |
|      - | 1550 | ` */` |
|      2 | 1551 | `static void PH7_XML_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1552 | `{` |
|      1 | 1553 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1554 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1555 | `}` |
|      - | 1556 | `/*` |
|      - | 1557 | ` * XML_ERROR_NO_MEMORY` |
|      - | 1558 | ` *   Expand the value of SXML_ERROR_NONE defined in ph7Int.h` |
|      - | 1559 | ` */` |
|      2 | 1560 | `static void PH7_XML_ERROR_NO_MEMORY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1561 | `{` |
|      1 | 1562 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1563 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1564 | `}` |
|      - | 1565 | `/*` |
|      - | 1566 | ` * XML_ERROR_SYNTAX` |
|      - | 1567 | ` *   Expand the value of SXML_ERROR_SYNTAX defined in ph7Int.h` |
|      - | 1568 | ` */` |
|      2 | 1569 | `static void PH7_XML_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1570 | `{` |
|      1 | 1571 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1572 | `	ph7_value_int(pVal,SXML_ERROR_SYNTAX);` |
|      3 | 1573 | `}` |
|      - | 1574 | `/*` |
|      - | 1575 | ` * XML_ERROR_NO_ELEMENTS` |
|      - | 1576 | ` *   Expand the value of SXML_ERROR_NO_ELEMENTS defined in ph7Int.h` |
|      - | 1577 | ` */` |
|      2 | 1578 | `static void PH7_XML_ERROR_NO_ELEMENTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1579 | `{` |
|      1 | 1580 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1581 | `	ph7_value_int(pVal,SXML_ERROR_NO_ELEMENTS);` |
|      3 | 1582 | `}` |
|      - | 1583 | `/*` |
|      - | 1584 | ` * XML_ERROR_INVALID_TOKEN` |
|      - | 1585 | ` *   Expand the value of SXML_ERROR_INVALID_TOKEN defined in ph7Int.h` |
|      - | 1586 | ` */` |
|      2 | 1587 | `static void PH7_XML_ERROR_INVALID_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1588 | `{` |
|      1 | 1589 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1590 | `	ph7_value_int(pVal,SXML_ERROR_INVALID_TOKEN);` |
|      3 | 1591 | `}` |
|      - | 1592 | `/*` |
|      - | 1593 | ` * XML_ERROR_UNCLOSED_TOKEN` |
|      - | 1594 | ` *   Expand the value of SXML_ERROR_UNCLOSED_TOKEN defined in ph7Int.h` |
|      - | 1595 | ` */` |
|      2 | 1596 | `static void PH7_XML_ERROR_UNCLOSED_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1597 | `{` |
|      1 | 1598 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1599 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_TOKEN);` |
|      3 | 1600 | `}` |
|      - | 1601 | `/*` |
|      - | 1602 | ` * XML_ERROR_PARTIAL_CHAR` |
|      - | 1603 | ` *   Expand the value of SXML_ERROR_PARTIAL_CHAR defined in ph7Int.h` |
|      - | 1604 | ` */` |
|      2 | 1605 | `static void PH7_XML_ERROR_PARTIAL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1606 | `{` |
|      1 | 1607 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1608 | `	ph7_value_int(pVal,SXML_ERROR_PARTIAL_CHAR);` |
|      3 | 1609 | `}` |
|      - | 1610 | `/*` |
|      - | 1611 | ` * XML_ERROR_TAG_MISMATCH` |
|      - | 1612 | ` *   Expand the value of SXML_ERROR_TAG_MISMATCH defined in ph7Int.h` |
|      - | 1613 | ` */` |
|      2 | 1614 | `static void PH7_XML_ERROR_TAG_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1615 | `{` |
|      1 | 1616 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1617 | `	ph7_value_int(pVal,SXML_ERROR_TAG_MISMATCH);` |
|      3 | 1618 | `}` |
|      - | 1619 | `/*` |
|      - | 1620 | ` * XML_ERROR_DUPLICATE_ATTRIBUTE` |
|      - | 1621 | ` *   Expand the value of SXML_ERROR_DUPLICATE_ATTRIBUTE defined in ph7Int.h` |
|      - | 1622 | ` */` |
|      2 | 1623 | `static void PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1624 | `{` |
|      1 | 1625 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1626 | `	ph7_value_int(pVal,SXML_ERROR_DUPLICATE_ATTRIBUTE);` |
|      3 | 1627 | `}` |
|      - | 1628 | `/*` |
|      - | 1629 | ` * XML_ERROR_JUNK_AFTER_DOC_ELEMENT` |
|      - | 1630 | ` *   Expand the value of SXML_ERROR_JUNK_AFTER_DOC_ELEMENT defined in ph7Int.h` |
|      - | 1631 | ` */` |
|      2 | 1632 | `static void PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1633 | `{` |
|      1 | 1634 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1635 | `	ph7_value_int(pVal,SXML_ERROR_JUNK_AFTER_DOC_ELEMENT);` |
|      3 | 1636 | `}` |
|      - | 1637 | `/*` |
|      - | 1638 | ` * XML_ERROR_PARAM_ENTITY_REF` |
|      - | 1639 | ` *   Expand the value of SXML_ERROR_PARAM_ENTITY_REF defined in ph7Int.h` |
|      - | 1640 | ` */` |
|      2 | 1641 | `static void PH7_XML_ERROR_PARAM_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1642 | `{` |
|      1 | 1643 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1644 | `	ph7_value_int(pVal,SXML_ERROR_PARAM_ENTITY_REF);` |
|      3 | 1645 | `}` |
|      - | 1646 | `/*` |
|      - | 1647 | ` * XML_ERROR_UNDEFINED_ENTITY` |
|      - | 1648 | ` *   Expand the value of SXML_ERROR_UNDEFINED_ENTITY defined in ph7Int.h` |
|      - | 1649 | ` */` |
|      2 | 1650 | `static void PH7_XML_ERROR_UNDEFINED_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1651 | `{` |
|      1 | 1652 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1653 | `	ph7_value_int(pVal,SXML_ERROR_UNDEFINED_ENTITY);` |
|      3 | 1654 | `}` |
|      - | 1655 | `/*` |
|      - | 1656 | ` * XML_ERROR_RECURSIVE_ENTITY_REF` |
|      - | 1657 | ` *   Expand the value of SXML_ERROR_RECURSIVE_ENTITY_REF defined in ph7Int.h` |
|      - | 1658 | ` */` |
|      2 | 1659 | `static void PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1660 | `{` |
|      1 | 1661 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1662 | `	ph7_value_int(pVal,SXML_ERROR_RECURSIVE_ENTITY_REF);` |
|      3 | 1663 | `}` |
|      - | 1664 | `/*` |
|      - | 1665 | ` * XML_ERROR_ASYNC_ENTITY` |
|      - | 1666 | ` *   Expand the value of SXML_ERROR_ASYNC_ENTITY defined in ph7Int.h` |
|      - | 1667 | ` */` |
|      2 | 1668 | `static void PH7_XML_ERROR_ASYNC_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1669 | `{` |
|      1 | 1670 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1671 | `	ph7_value_int(pVal,SXML_ERROR_ASYNC_ENTITY);` |
|      3 | 1672 | `}` |
|      - | 1673 | `/*` |
|      - | 1674 | ` * XML_ERROR_BAD_CHAR_REF` |
|      - | 1675 | ` *   Expand the value of SXML_ERROR_BAD_CHAR_REF defined in ph7Int.h` |
|      - | 1676 | ` */` |
|      2 | 1677 | `static void PH7_XML_ERROR_BAD_CHAR_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1678 | `{` |
|      1 | 1679 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1680 | `	ph7_value_int(pVal,SXML_ERROR_BAD_CHAR_REF);` |
|      3 | 1681 | `}` |
|      - | 1682 | `/*` |
|      - | 1683 | ` * XML_ERROR_BINARY_ENTITY_REF` |
|      - | 1684 | ` *   Expand the value of SXML_ERROR_BINARY_ENTITY_REF defined in ph7Int.h` |
|      - | 1685 | ` */` |
|      2 | 1686 | `static void PH7_XML_ERROR_BINARY_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1687 | `{` |
|      1 | 1688 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1689 | `	ph7_value_int(pVal,SXML_ERROR_BINARY_ENTITY_REF);` |
|      3 | 1690 | `}` |
|      - | 1691 | `/*` |
|      - | 1692 | ` * XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF` |
|      - | 1693 | ` *   Expand the value of SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF defined in ph7Int.h` |
|      - | 1694 | ` */` |
|      2 | 1695 | `static void PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1696 | `{` |
|      1 | 1697 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1698 | `	ph7_value_int(pVal,SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF);` |
|      3 | 1699 | `}` |
|      - | 1700 | `/*` |
|      - | 1701 | ` * XML_ERROR_MISPLACED_XML_PI` |
|      - | 1702 | ` *   Expand the value of SXML_ERROR_MISPLACED_XML_PI defined in ph7Int.h` |
|      - | 1703 | ` */` |
|      2 | 1704 | `static void PH7_XML_ERROR_MISPLACED_XML_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1705 | `{` |
|      1 | 1706 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1707 | `	ph7_value_int(pVal,SXML_ERROR_MISPLACED_XML_PI);` |
|      3 | 1708 | `}` |
|      - | 1709 | `/*` |
|      - | 1710 | ` * XML_ERROR_UNKNOWN_ENCODING` |
|      - | 1711 | ` *   Expand the value of SXML_ERROR_UNKNOWN_ENCODING defined in ph7Int.h` |
|      - | 1712 | ` */` |
|      2 | 1713 | `static void PH7_XML_ERROR_UNKNOWN_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1714 | `{` |
|      1 | 1715 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1716 | `	ph7_value_int(pVal,SXML_ERROR_UNKNOWN_ENCODING);` |
|      3 | 1717 | `}` |
|      - | 1718 | `/*` |
|      - | 1719 | ` * XML_ERROR_INCORRECT_ENCODING` |
|      - | 1720 | ` *   Expand the value of SXML_ERROR_INCORRECT_ENCODING defined in ph7Int.h` |
|      - | 1721 | ` */` |
|      2 | 1722 | `static void PH7_XML_ERROR_INCORRECT_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1723 | `{` |
|      1 | 1724 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1725 | `	ph7_value_int(pVal,SXML_ERROR_INCORRECT_ENCODING);` |
|      3 | 1726 | `}` |
|      - | 1727 | `/*` |
|      - | 1728 | ` * XML_ERROR_UNCLOSED_CDATA_SECTION` |
|      - | 1729 | ` *   Expand the value of SXML_ERROR_UNCLOSED_CDATA_SECTION defined in ph7Int.h` |
|      - | 1730 | ` */` |
|      2 | 1731 | `static void PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1732 | `{` |
|      1 | 1733 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1734 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_CDATA_SECTION);` |
|      3 | 1735 | `}` |
|      - | 1736 | `/*` |
|      - | 1737 | ` * XML_ERROR_EXTERNAL_ENTITY_HANDLING` |
|      - | 1738 | ` *   Expand the value of SXML_ERROR_EXTERNAL_ENTITY_HANDLING defined in ph7Int.h` |
|      - | 1739 | ` */` |
|      2 | 1740 | `static void PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1741 | `{` |
|      1 | 1742 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1743 | `	ph7_value_int(pVal,SXML_ERROR_EXTERNAL_ENTITY_HANDLING);` |
|      3 | 1744 | `}` |
|      - | 1745 | `/*` |
|      - | 1746 | ` * XML_OPTION_CASE_FOLDING` |
|      - | 1747 | ` *   Expand the value of SXML_OPTION_CASE_FOLDING defined in ph7Int.h.` |
|      - | 1748 | ` */` |
|      2 | 1749 | `static void PH7_XML_OPTION_CASE_FOLDING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1750 | `{` |
|      1 | 1751 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1752 | `	ph7_value_int(pVal,SXML_OPTION_CASE_FOLDING);` |
|      3 | 1753 | `}` |
|      - | 1754 | `/*` |
|      - | 1755 | ` * XML_OPTION_TARGET_ENCODING` |
|      - | 1756 | ` *   Expand the value of SXML_OPTION_TARGET_ENCODING defined in ph7Int.h.` |
|      - | 1757 | ` */` |
|      4 | 1758 | `static void PH7_XML_OPTION_TARGET_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1759 | `{` |
|      2 | 1760 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1761 | `	ph7_value_int(pVal,SXML_OPTION_TARGET_ENCODING);` |
|      5 | 1762 | `}` |
|      - | 1763 | `/*` |
|      - | 1764 | ` * XML_OPTION_SKIP_TAGSTART` |
|      - | 1765 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1766 | ` */` |
|      2 | 1767 | `static void PH7_XML_OPTION_SKIP_TAGSTART_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1768 | `{` |
|      1 | 1769 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1770 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_TAGSTART);` |
|      3 | 1771 | `}` |
|      - | 1772 | `/*` |
|      - | 1773 | ` * XML_OPTION_SKIP_WHITE` |
|      - | 1774 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1775 | ` */` |
|      4 | 1776 | `static void PH7_XML_OPTION_SKIP_WHITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1777 | `{` |
|      2 | 1778 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1779 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_WHITE);` |
|      5 | 1780 | `}` |
|      - | 1781 | `/*` |
|      - | 1782 | ` * XML_SAX_IMPL.` |
|      - | 1783 | ` *   Expand the name of the underlying XML engine.` |
|      - | 1784 | ` */` |
|      2 | 1785 | `static void PH7_XML_SAX_IMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1786 | `{` |
|      1 | 1787 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1788 | `	ph7_value_string(pVal,"Symisc XML engine",(int)sizeof("Symisc XML engine")-1);` |
|      3 | 1789 | `}` |
|      - | 1790 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1791 | `/*` |
|      - | 1792 | ` * JSON_HEX_TAG.` |
|      - | 1793 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|      - | 1794 | ` */` |
|      2 | 1795 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1796 | `{` |
|      1 | 1797 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1798 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      3 | 1799 | `}` |
|      - | 1800 | `/*` |
|      - | 1801 | ` * JSON_HEX_AMP.` |
|      - | 1802 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|      - | 1803 | ` */` |
|      2 | 1804 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1805 | `{` |
|      1 | 1806 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1807 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      3 | 1808 | `}` |
|      - | 1809 | `/*` |
|      - | 1810 | ` * JSON_HEX_APOS.` |
|      - | 1811 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|      - | 1812 | ` */` |
|      2 | 1813 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1814 | `{` |
|      1 | 1815 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1816 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      3 | 1817 | `}` |
|      - | 1818 | `/*` |
|      - | 1819 | ` * JSON_HEX_QUOT.` |
|      - | 1820 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|      - | 1821 | ` */` |
|      2 | 1822 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1823 | `{` |
|      1 | 1824 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1825 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      3 | 1826 | `}` |
|      - | 1827 | `/*` |
|      - | 1828 | ` * JSON_FORCE_OBJECT.` |
|      - | 1829 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|      - | 1830 | ` */` |
|      4 | 1831 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1832 | `{` |
|      2 | 1833 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1834 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      5 | 1835 | `}` |
|      - | 1836 | `/*` |
|      - | 1837 | ` * JSON_NUMERIC_CHECK.` |
|      - | 1838 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|      - | 1839 | ` */` |
|      4 | 1840 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1841 | `{` |
|      2 | 1842 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1843 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      5 | 1844 | `}` |
|      - | 1845 | `/*` |
|      - | 1846 | ` * JSON_BIGINT_AS_STRING.` |
|      - | 1847 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|      - | 1848 | ` */` |
|      2 | 1849 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1850 | `{` |
|      1 | 1851 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1852 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      3 | 1853 | `}` |
|      - | 1854 | `/*` |
|      - | 1855 | ` * JSON_PRETTY_PRINT.` |
|      - | 1856 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|      - | 1857 | ` */` |
|      2 | 1858 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1859 | `{` |
|      1 | 1860 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1861 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      3 | 1862 | `}` |
|      - | 1863 | `/*` |
|      - | 1864 | ` * JSON_UNESCAPED_SLASHES.` |
|      - | 1865 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|      - | 1866 | ` */` |
|      4 | 1867 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1868 | `{` |
|      2 | 1869 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1870 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      5 | 1871 | `}` |
|      - | 1872 | `/*` |
|      - | 1873 | ` * JSON_UNESCAPED_UNICODE.` |
|      - | 1874 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|      - | 1875 | ` */` |
|      2 | 1876 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1877 | `{` |
|      1 | 1878 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1879 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|      3 | 1880 | `}` |
|      - | 1881 | `/*` |
|      - | 1882 | ` * JSON_ERROR_NONE.` |
|      - | 1883 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|      - | 1884 | ` */` |
|      4 | 1885 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1886 | `{` |
|      2 | 1887 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1888 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      5 | 1889 | `}` |
|      - | 1890 | `/*` |
|      - | 1891 | ` * JSON_ERROR_DEPTH.` |
|      - | 1892 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|      - | 1893 | ` */` |
|      2 | 1894 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1895 | `{` |
|      1 | 1896 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1897 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      3 | 1898 | `}` |
|      - | 1899 | `/*` |
|      - | 1900 | ` * JSON_ERROR_STATE_MISMATCH.` |
|      - | 1901 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|      - | 1902 | ` */` |
|      2 | 1903 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1904 | `{` |
|      1 | 1905 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1906 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      3 | 1907 | `}` |
|      - | 1908 | `/*` |
|      - | 1909 | ` * JSON_ERROR_CTRL_CHAR.` |
|      - | 1910 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|      - | 1911 | ` */` |
|      2 | 1912 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1913 | `{` |
|      1 | 1914 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1915 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      3 | 1916 | `}` |
|      - | 1917 | `/*` |
|      - | 1918 | ` * JSON_ERROR_SYNTAX.` |
|      - | 1919 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|      - | 1920 | ` */` |
|      4 | 1921 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1922 | `{` |
|      2 | 1923 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1924 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      5 | 1925 | `}` |
|      - | 1926 | `/*` |
|      - | 1927 | ` * JSON_ERROR_UTF8.` |
|      - | 1928 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|      - | 1929 | ` */` |
|      2 | 1930 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1931 | `{` |
|      1 | 1932 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1933 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      3 | 1934 | `}` |
|      - | 1935 | `/*` |
|      - | 1936 | ` * JSON_ERROR_NON_BACKED_ENUM.` |
|      - | 1937 | ` *   Expand the value of JSON_ERROR_NON_BACKED_ENUM defined in ph7Int.h (php 8.1).` |
|      - | 1938 | ` */` |
|    ! 0 | 1939 | `static void PH7_JSON_ERROR_NON_BACKED_ENUM_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 1940 | `{` |
|    ! 0 | 1941 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 1942 | `	ph7_value_int(pVal,JSON_ERROR_NON_BACKED_ENUM);` |
|    ! 0 | 1943 | `}` |
|      - | 1944 | `/*` |
|      - | 1945 | ` * static` |
|      - | 1946 | ` *  Expand the name of the current class. 'static' otherwise.` |
|      - | 1947 | ` */` |
|      6 | 1948 | `static void PH7_static_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1949 | `{` |
|      7 | 1950 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1951 | `	ph7_class *pClass;` |
|      - | 1952 | `	/* Extract the target class if available */` |
|      7 | 1953 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      7 | 1954 | `	if( pClass ){` |
|      3 | 1955 | `		SyString *pName = &pClass->sName;` |
|      - | 1956 | `		/* Expand class name */` |
|      3 | 1957 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      2 | 1958 | `	}else{` |
|      - | 1959 | `		/* Expand 'static' */` |
|      5 | 1960 | `		ph7_value_string(pVal,"static",sizeof("static")-1);` |
|      - | 1961 | `	}` |
|      7 | 1962 | `}` |
|      - | 1963 | `/*` |
|      - | 1964 | ` * self` |
|      - | 1965 | ` * __CLASS__` |
|      - | 1966 | ` *  Expand the name of the current class. NULL otherwise.` |
|      - | 1967 | ` */` |
|      2 | 1968 | `static void PH7_self_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1969 | `{` |
|      3 | 1970 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1971 | `	ph7_class *pClass;` |
|      - | 1972 |  |
|      - | 1973 | `	/* Get the declaring class of the current method */` |
|      3 | 1974 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      3 | 1975 | `	if( pClass == 0 ){` |
|      - | 1976 | `		/* Not in a method, fall back to runtime class */` |
|      3 | 1977 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      1 | 1978 | `	}` |
|      - | 1979 |  |
|      3 | 1980 | `	if( pClass ){` |
|    ! 0 | 1981 | `		SyString *pName = &pClass->sName;` |
|      - | 1982 | `		/* Expand class name */` |
|    ! 0 | 1983 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|    ! 0 | 1984 | `	}else{` |
|      - | 1985 | `		/* Expand null */` |
|      3 | 1986 | `		ph7_value_null(pVal);` |
|      - | 1987 | `	}` |
|      3 | 1988 | `}` |
|      - | 1989 | `/* parent` |
|      - | 1990 | ` *  Expand the name of the parent class. NULL otherwise.` |
|      - | 1991 | ` */` |
|      2 | 1992 | `static void PH7_parent_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1993 | `{` |
|      3 | 1994 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1995 | `	ph7_class *pClass;` |
|      - | 1996 |  |
|      - | 1997 | `	/* Get the declaring class, then its parent */` |
|      3 | 1998 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      3 | 1999 | `	if( pClass && pClass->pBase ){` |
|    ! 0 | 2000 | `		SyString *pName = &pClass->pBase->sName;` |
|      - | 2001 | `		/* Expand parent class name */` |
|    ! 0 | 2002 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|    ! 0 | 2003 | `	}else{` |
|      - | 2004 | `		/* Expand null */` |
|      3 | 2005 | `		ph7_value_null(pVal);` |
|      - | 2006 | `	}` |
|      3 | 2007 | `}` |
|      - | 2008 |  |
|      - | 2009 | `/*` |
|      - | 2010 | ` * PASSWORD_BCRYPT / PASSWORD_DEFAULT` |
|      - | 2011 | ` *  The bcrypt algorithm identifier (PHP 7.4+ exposes these as the string "2y").` |
|      - | 2012 | ` *  PASSWORD_DEFAULT tracks the recommended default, currently bcrypt.` |
|      - | 2013 | ` */` |
|     20 | 2014 | `static void PH7_PASSWORD_BCRYPT_Const(ph7_value *pVal,void *pUnused)` |
|      2 | 2015 | `{` |
|     10 | 2016 | `	SXUNUSED(pUnused);` |
|     22 | 2017 | `	ph7_value_string(pVal,"2y",(int)sizeof("2y")-1);` |
|     22 | 2018 | `}` |
|      - | 2019 | `/*` |
|      - | 2020 | ` * PASSWORD_BCRYPT_DEFAULT_COST` |
|      - | 2021 | ` *  The default bcrypt work factor used by password_hash() (currently 12).` |
|      - | 2022 | ` */` |
|      2 | 2023 | `static void PH7_PASSWORD_COST_Const(ph7_value *pVal,void *pUnused)` |
|      1 | 2024 | `{` |
|      1 | 2025 | `	SXUNUSED(pUnused);` |
|      3 | 2026 | `	ph7_value_int(pVal,12);` |
|      3 | 2027 | `}` |
|      - | 2028 | `/*` |
|      - | 2029 | ` * filter_var() filter and flag identifiers (the ext/filter constants). Values` |
|      - | 2030 | ` * match PHP 8.5. One tiny int-returning callback per constant, generated by a` |
|      - | 2031 | ` * local macro to keep the ~25 near-identical definitions DRY.` |
|      - | 2032 | ` */` |
|      - | 2033 | `#define PH7_FILTER_INT_CONST(Name,Val) \` |
|      - | 2034 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|      - | 2035 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|      - | 2036 | `	}` |
|     10 | 2037 | `PH7_FILTER_INT_CONST(FILTER_DEFAULT,516)` |
|     17 | 2038 | `PH7_FILTER_INT_CONST(FILTER_UNSAFE_RAW,516)` |
|     64 | 2039 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_INT,257)` |
|     29 | 2040 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_BOOLEAN,258)` |
|     69 | 2041 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_FLOAT,259)` |
|      8 | 2042 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_REGEXP,272)` |
|     11 | 2043 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_DOMAIN,277)` |
|     15 | 2044 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_URL,273)` |
|     28 | 2045 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_EMAIL,274)` |
|     25 | 2046 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_IP,275)` |
|     11 | 2047 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_MAC,276)` |
|      3 | 2048 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_INT,519)` |
|      5 | 2049 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_FLOAT,520)` |
|     13 | 2050 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_SPECIAL_CHARS,515)` |
|     25 | 2051 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_FULL_SPECIAL_CHARS,522)` |
|      3 | 2052 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_EMAIL,517)` |
|      3 | 2053 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_URL,518)` |
|      3 | 2054 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_OCTAL,1)` |
|      3 | 2055 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_HEX,2)` |
|      7 | 2056 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_LOW,4)` |
|      5 | 2057 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_HIGH,8)` |
|      5 | 2058 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_LOW,16)` |
|      5 | 2059 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_HIGH,32)` |
|      3 | 2060 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_AMP,64)` |
|      3 | 2061 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_ENCODE_QUOTES,128)` |
|      3 | 2062 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_BACKTICK,512)` |
|      3 | 2063 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_FRACTION,4096)` |
|     25 | 2064 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_THOUSAND,8192)` |
|      3 | 2065 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_SCIENTIFIC,16384)` |
|      5 | 2066 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV4,1048576)` |
|      3 | 2067 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV6,2097152)` |
|     14 | 2068 | `PH7_FILTER_INT_CONST(FILTER_NULL_ON_FAILURE,134217728)` |
|      - | 2069 | `/* filter_input() source selectors (php values; SESSION/REQUEST are undefined in 8.5) */` |
|      5 | 2070 | `PH7_FILTER_INT_CONST(INPUT_POST,0)` |
|      8 | 2071 | `PH7_FILTER_INT_CONST(INPUT_GET,1)` |
|      3 | 2072 | `PH7_FILTER_INT_CONST(INPUT_COOKIE,2)` |
|      3 | 2073 | `PH7_FILTER_INT_CONST(INPUT_ENV,4)` |
|     21 | 2074 | `PH7_FILTER_INT_CONST(INPUT_SERVER,5)` |
|      - | 2075 | `/*` |
|      - | 2076 | ` * Table of built-in constants.` |
|      - | 2077 | ` */` |
|      - | 2078 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|      - | 2079 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|      - | 2080 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|      - | 2081 | `	{"__PH7__",              PH7_VER_Const      },` |
|      - | 2082 | `	{"PHP_VERSION",          PH7_PHPVerConst    },` |
|      - | 2083 | `	{"PHP_MAJOR_VERSION",    PH7_PHPMajorConst  },` |
|      - | 2084 | `	{"PHP_MINOR_VERSION",    PH7_PHPMinorConst  },` |
|      - | 2085 | `	{"PHP_RELEASE_VERSION",  PH7_PHPReleaseConst},` |
|      - | 2086 | `	{"PHP_EXTRA_VERSION",    PH7_PHPExtraConst  },` |
|      - | 2087 | `	{"PHP_VERSION_ID",       PH7_PHPVerIdConst  },` |
|      - | 2088 | `	{"PHP_OS",               PH7_OS_Const       },` |
|      - | 2089 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|      - | 2090 | `	{"PHP_SESSION_DISABLED", PH7_PHP_SESSION_DISABLED_Const },` |
|      - | 2091 | `	{"PHP_SESSION_NONE",     PH7_PHP_SESSION_NONE_Const },` |
|      - | 2092 | `	{"PHP_SESSION_ACTIVE",   PH7_PHP_SESSION_ACTIVE_Const },` |
|      - | 2093 | `	{"INI_USER",             PH7_INI_USER_Const },` |
|      - | 2094 | `	{"INI_PERDIR",           PH7_INI_PERDIR_Const },` |
|      - | 2095 | `	{"INI_SYSTEM",           PH7_INI_SYSTEM_Const },` |
|      - | 2096 | `	{"INI_ALL",              PH7_INI_ALL_Const },` |
|      - | 2097 | `	{"PASSWORD_BCRYPT",      PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2098 | `	{"PASSWORD_DEFAULT",     PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2099 | `	{"PASSWORD_BCRYPT_DEFAULT_COST", PH7_PASSWORD_COST_Const },` |
|      - | 2100 | `	{"FILTER_DEFAULT",              PH7_FILTER_DEFAULT_Const },` |
|      - | 2101 | `	{"FILTER_UNSAFE_RAW",           PH7_FILTER_UNSAFE_RAW_Const },` |
|      - | 2102 | `	{"FILTER_VALIDATE_INT",         PH7_FILTER_VALIDATE_INT_Const },` |
|      - | 2103 | `	{"FILTER_VALIDATE_BOOLEAN",     PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2104 | `	{"FILTER_VALIDATE_BOOL",        PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2105 | `	{"FILTER_VALIDATE_FLOAT",       PH7_FILTER_VALIDATE_FLOAT_Const },` |
|      - | 2106 | `	{"FILTER_VALIDATE_REGEXP",      PH7_FILTER_VALIDATE_REGEXP_Const },` |
|      - | 2107 | `	{"FILTER_VALIDATE_DOMAIN",      PH7_FILTER_VALIDATE_DOMAIN_Const },` |
|      - | 2108 | `	{"FILTER_VALIDATE_URL",         PH7_FILTER_VALIDATE_URL_Const },` |
|      - | 2109 | `	{"FILTER_VALIDATE_EMAIL",       PH7_FILTER_VALIDATE_EMAIL_Const },` |
|      - | 2110 | `	{"FILTER_VALIDATE_IP",          PH7_FILTER_VALIDATE_IP_Const },` |
|      - | 2111 | `	{"FILTER_VALIDATE_MAC",         PH7_FILTER_VALIDATE_MAC_Const },` |
|      - | 2112 | `	{"FILTER_SANITIZE_NUMBER_INT",  PH7_FILTER_SANITIZE_NUMBER_INT_Const },` |
|      - | 2113 | `	{"FILTER_SANITIZE_NUMBER_FLOAT",PH7_FILTER_SANITIZE_NUMBER_FLOAT_Const },` |
|      - | 2114 | `	{"FILTER_SANITIZE_SPECIAL_CHARS",PH7_FILTER_SANITIZE_SPECIAL_CHARS_Const },` |
|      - | 2115 | `	{"FILTER_SANITIZE_FULL_SPECIAL_CHARS",PH7_FILTER_SANITIZE_FULL_SPECIAL_CHARS_Const },` |
|      - | 2116 | `	{"FILTER_SANITIZE_EMAIL",       PH7_FILTER_SANITIZE_EMAIL_Const },` |
|      - | 2117 | `	{"FILTER_SANITIZE_URL",         PH7_FILTER_SANITIZE_URL_Const },` |
|      - | 2118 | `	{"FILTER_FLAG_ALLOW_OCTAL",     PH7_FILTER_FLAG_ALLOW_OCTAL_Const },` |
|      - | 2119 | `	{"FILTER_FLAG_ALLOW_HEX",       PH7_FILTER_FLAG_ALLOW_HEX_Const },` |
|      - | 2120 | `	{"FILTER_FLAG_STRIP_LOW",       PH7_FILTER_FLAG_STRIP_LOW_Const },` |
|      - | 2121 | `	{"FILTER_FLAG_STRIP_HIGH",      PH7_FILTER_FLAG_STRIP_HIGH_Const },` |
|      - | 2122 | `	{"FILTER_FLAG_ENCODE_LOW",      PH7_FILTER_FLAG_ENCODE_LOW_Const },` |
|      - | 2123 | `	{"FILTER_FLAG_ENCODE_HIGH",     PH7_FILTER_FLAG_ENCODE_HIGH_Const },` |
|      - | 2124 | `	{"FILTER_FLAG_ENCODE_AMP",      PH7_FILTER_FLAG_ENCODE_AMP_Const },` |
|      - | 2125 | `	{"FILTER_FLAG_NO_ENCODE_QUOTES",PH7_FILTER_FLAG_NO_ENCODE_QUOTES_Const },` |
|      - | 2126 | `	{"FILTER_FLAG_STRIP_BACKTICK",  PH7_FILTER_FLAG_STRIP_BACKTICK_Const },` |
|      - | 2127 | `	{"FILTER_FLAG_ALLOW_FRACTION",  PH7_FILTER_FLAG_ALLOW_FRACTION_Const },` |
|      - | 2128 | `	{"FILTER_FLAG_ALLOW_THOUSAND",  PH7_FILTER_FLAG_ALLOW_THOUSAND_Const },` |
|      - | 2129 | `	{"FILTER_FLAG_ALLOW_SCIENTIFIC",PH7_FILTER_FLAG_ALLOW_SCIENTIFIC_Const },` |
|      - | 2130 | `	{"FILTER_FLAG_IPV4",            PH7_FILTER_FLAG_IPV4_Const },` |
|      - | 2131 | `	{"FILTER_FLAG_IPV6",            PH7_FILTER_FLAG_IPV6_Const },` |
|      - | 2132 | `	{"FILTER_NULL_ON_FAILURE",      PH7_FILTER_NULL_ON_FAILURE_Const },` |
|      - | 2133 | `	{"INPUT_POST",                  PH7_INPUT_POST_Const },` |
|      - | 2134 | `	{"INPUT_GET",                   PH7_INPUT_GET_Const },` |
|      - | 2135 | `	{"INPUT_COOKIE",                PH7_INPUT_COOKIE_Const },` |
|      - | 2136 | `	{"INPUT_ENV",                   PH7_INPUT_ENV_Const },` |
|      - | 2137 | `	{"INPUT_SERVER",                PH7_INPUT_SERVER_Const },` |
|      - | 2138 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|      - | 2139 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|      - | 2140 | `	{"PHP_INT_MIN",          PH7_INTMIN_Const   },` |
|      - | 2141 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|      - | 2142 | `	{"PHP_FLOAT_EPSILON",    PH7_FLOATEPSILON_Const },` |
|      - | 2143 | `	{"PHP_FLOAT_MAX",        PH7_FLOATMAX_Const },` |
|      - | 2144 | `	{"PHP_FLOAT_MIN",        PH7_FLOATMIN_Const },` |
|      - | 2145 | `	{"PHP_FLOAT_DIG",        PH7_FLOATDIG_Const },` |
|      - | 2146 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|      - | 2147 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|      - | 2148 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|      - | 2149 | `	{"__TIME__",             PH7_TIME_Const     },` |
|      - | 2150 | `	{"__DATE__",             PH7_DATE_Const     },` |
|      - | 2151 | `	{"__FILE__",             PH7_FILE_Const     },` |
|      - | 2152 | `	{"__DIR__",              PH7_DIR_Const      },` |
|      - | 2153 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|      - | 2154 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|      - | 2155 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|      - | 2156 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|      - | 2157 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|      - | 2158 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|      - | 2159 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|      - | 2160 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|      - | 2161 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|      - | 2162 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|      - | 2163 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|      - | 2164 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|      - | 2165 | `	{"E_STRICT",             PH7_E_STRICT_Const        },` |
|      - | 2166 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|      - | 2167 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|      - | 2168 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|      - | 2169 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|      - | 2170 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|      - | 2171 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|      - | 2172 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|      - | 2173 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|      - | 2174 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|      - | 2175 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|      - | 2176 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|      - | 2177 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|      - | 2178 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|      - | 2179 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|      - | 2180 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|      - | 2181 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|      - | 2182 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|      - | 2183 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|      - | 2184 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|      - | 2185 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|      - | 2186 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|      - | 2187 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|      - | 2188 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 2189 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|      - | 2190 | `	{"M_E",                  PH7_M_E_Const          },` |
|      - | 2191 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|      - | 2192 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|      - | 2193 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|      - | 2194 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|      - | 2195 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|      - | 2196 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|      - | 2197 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|      - | 2198 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|      - | 2199 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|      - | 2200 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|      - | 2201 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|      - | 2202 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|      - | 2203 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|      - | 2204 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|      - | 2205 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|      - | 2206 | `	{"NAN",                  PH7_NAN_Const          },` |
|      - | 2207 | `	{"INF",                  PH7_INF_Const          },` |
|      - | 2208 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 2209 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|      - | 2210 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|      - | 2211 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|      - | 2212 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|      - | 2213 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|      - | 2214 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|      - | 2215 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|      - | 2216 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|      - | 2217 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|      - | 2218 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|      - | 2219 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|      - | 2220 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|      - | 2221 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|      - | 2222 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|      - | 2223 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|      - | 2224 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|      - | 2225 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|      - | 2226 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|      - | 2227 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|      - | 2228 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|      - | 2229 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|      - | 2230 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|      - | 2231 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|      - | 2232 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|      - | 2233 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|      - | 2234 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|      - | 2235 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|      - | 2236 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|      - | 2237 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|      - | 2238 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|      - | 2239 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|      - | 2240 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|      - | 2241 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|      - | 2242 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|      - | 2243 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|      - | 2244 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|      - | 2245 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|      - | 2246 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|      - | 2247 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|      - | 2248 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|      - | 2249 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|      - | 2250 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|      - | 2251 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|      - | 2252 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|      - | 2253 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|      - | 2254 | `	{"ASSERT_ACTIVE",        PH7_ASSERT_ACTIVE_Const     },` |
|      - | 2255 | `	{"ASSERT_CALLBACK",      PH7_ASSERT_CALLBACK_Const   },` |
|      - | 2256 | `	{"ASSERT_BAIL",          PH7_ASSERT_BAIL_Const       },` |
|      - | 2257 | `	{"ASSERT_WARNING",       PH7_ASSERT_WARNING_Const    },` |
|      - | 2258 | `	{"ASSERT_EXCEPTION",     PH7_ASSERT_EXCEPTION_Const  },` |
|      - | 2259 | `	{"ASSERT_QUIET_EVAL",    PH7_ASSERT_QUIET_EVAL_Const },` |
|      - | 2260 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|      - | 2261 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|      - | 2262 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|      - | 2263 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|      - | 2264 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|      - | 2265 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|      - | 2266 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|      - | 2267 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|      - | 2268 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|      - | 2269 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|      - | 2270 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|      - | 2271 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|      - | 2272 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|      - | 2273 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|      - | 2274 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|      - | 2275 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|      - | 2276 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|      - | 2277 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|      - | 2278 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|      - | 2279 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|      - | 2280 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|      - | 2281 | `	{"STDIN",                PH7_STDIN_Const        },` |
|      - | 2282 | `	{"stdin",                PH7_STDIN_Const        },` |
|      - | 2283 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|      - | 2284 | `	{"stdout",               PH7_STDOUT_Const       },` |
|      - | 2285 | `	{"STDERR",               PH7_STDERR_Const       },` |
|      - | 2286 | `	{"stderr",               PH7_STDERR_Const       },` |
|      - | 2287 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|      - | 2288 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|      - | 2289 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|      - | 2290 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|      - | 2291 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|      - | 2292 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|      - | 2293 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|      - | 2294 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|      - | 2295 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|      - | 2296 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2297 | `	{"XML_ERROR_NONE",       PH7_XML_ERROR_NONE_Const},` |
|      - | 2298 | `	{"XML_ERROR_NO_MEMORY",  PH7_XML_ERROR_NO_MEMORY_Const},` |
|      - | 2299 | `	{"XML_ERROR_SYNTAX",     PH7_XML_ERROR_SYNTAX_Const},` |
|      - | 2300 | `	{"XML_ERROR_NO_ELEMENTS",PH7_XML_ERROR_NO_ELEMENTS_Const},` |
|      - | 2301 | `	{"XML_ERROR_INVALID_TOKEN", PH7_XML_ERROR_INVALID_TOKEN_Const},` |
|      - | 2302 | `	{"XML_ERROR_UNCLOSED_TOKEN",PH7_XML_ERROR_UNCLOSED_TOKEN_Const},` |
|      - | 2303 | `	{"XML_ERROR_PARTIAL_CHAR",  PH7_XML_ERROR_PARTIAL_CHAR_Const},` |
|      - | 2304 | `	{"XML_ERROR_TAG_MISMATCH",  PH7_XML_ERROR_TAG_MISMATCH_Const},` |
|      - | 2305 | `	{"XML_ERROR_DUPLICATE_ATTRIBUTE",   PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const},` |
|      - | 2306 | `	{"XML_ERROR_JUNK_AFTER_DOC_ELEMENT",PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const},` |
|      - | 2307 | `	{"XML_ERROR_PARAM_ENTITY_REF",      PH7_XML_ERROR_PARAM_ENTITY_REF_Const},` |
|      - | 2308 | `	{"XML_ERROR_UNDEFINED_ENTITY",      PH7_XML_ERROR_UNDEFINED_ENTITY_Const},` |
|      - | 2309 | `	{"XML_ERROR_RECURSIVE_ENTITY_REF",  PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const},` |
|      - | 2310 | `	{"XML_ERROR_ASYNC_ENTITY",          PH7_XML_ERROR_ASYNC_ENTITY_Const},` |
|      - | 2311 | `	{"XML_ERROR_BAD_CHAR_REF",          PH7_XML_ERROR_BAD_CHAR_REF_Const},` |
|      - | 2312 | `	{"XML_ERROR_BINARY_ENTITY_REF",     PH7_XML_ERROR_BINARY_ENTITY_REF_Const},` |
|      - | 2313 | `	{"XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF", PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const},` |
|      - | 2314 | `	{"XML_ERROR_MISPLACED_XML_PI",     PH7_XML_ERROR_MISPLACED_XML_PI_Const},` |
|      - | 2315 | `	{"XML_ERROR_UNKNOWN_ENCODING",     PH7_XML_ERROR_UNKNOWN_ENCODING_Const},` |
|      - | 2316 | `	{"XML_ERROR_INCORRECT_ENCODING",   PH7_XML_ERROR_INCORRECT_ENCODING_Const},` |
|      - | 2317 | `	{"XML_ERROR_UNCLOSED_CDATA_SECTION",  PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const},` |
|      - | 2318 | `	{"XML_ERROR_EXTERNAL_ENTITY_HANDLING",PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const},` |
|      - | 2319 | `	{"XML_OPTION_CASE_FOLDING",           PH7_XML_OPTION_CASE_FOLDING_Const},` |
|      - | 2320 | `	{"XML_OPTION_TARGET_ENCODING",        PH7_XML_OPTION_TARGET_ENCODING_Const},` |
|      - | 2321 | `	{"XML_OPTION_SKIP_TAGSTART",          PH7_XML_OPTION_SKIP_TAGSTART_Const},` |
|      - | 2322 | `	{"XML_OPTION_SKIP_WHITE",             PH7_XML_OPTION_SKIP_WHITE_Const},` |
|      - | 2323 | `	{"XML_SAX_IMPL",           PH7_XML_SAX_IMP_Const},` |
|      - | 2324 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2325 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|      - | 2326 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|      - | 2327 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|      - | 2328 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|      - | 2329 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|      - | 2330 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|      - | 2331 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|      - | 2332 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|      - | 2333 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|      - | 2334 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|      - | 2335 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|      - | 2336 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|      - | 2337 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|      - | 2338 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|      - | 2339 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|      - | 2340 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|      - | 2341 | `	{"JSON_ERROR_NON_BACKED_ENUM", PH7_JSON_ERROR_NON_BACKED_ENUM_Const},` |
|      - | 2342 | `	{"static",               PH7_static_Const       },` |
|      - | 2343 | `	{"self",                 PH7_self_Const         },` |
|      - | 2344 | `	{"__CLASS__",            PH7_self_Const         },` |
|      - | 2345 | `	{"parent",               PH7_parent_Const       }` |
|      - | 2346 | `};` |
|      - | 2347 | `/*` |
|      - | 2348 | ` * Register the built-in constants defined above.` |
|      - | 2349 | ` */` |
|   3516 | 2350 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|      5 | 2351 | `{` |
|      - | 2352 | `	sxu32 n;` |
|      - | 2353 | `	/*` |
|      - | 2354 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|      - | 2355 | `	 * that trigger the constant invocation as their private data.` |
|      - | 2356 | `	 */` |
| 928229 | 2357 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 924713 | 2358 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
| 462359 | 2359 | `	}` |
|   3521 | 2360 | `}` |
