# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1122/1149 lines (97.65%)

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
|   3812 |   63 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|      5 |   64 | `{` |
|      - |   65 | `#if defined(__WINNT__)` |
|      5 |   66 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|      - |   67 | `#elif defined(__UNIXES__)` |
|      - |   68 | `	struct utsname sInfo;` |
|   3812 |   69 | `	if( uname(&sInfo) != 0 ){` |
|    ! 0 |   70 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 |   71 | `	}else{` |
|   3812 |   72 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|      - |   73 | `	}` |
|      - |   74 | `#else` |
|      - |   75 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|      - |   76 | `#endif` |
|   1906 |   77 | `	SXUNUSED(pUnused);` |
|   3817 |   78 | `}` |
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
|      4 |  150 | `{` |
|     82 |  151 | `	SXUNUSED(pUnused);` |
|      - |  152 | `#ifdef __WINNT__` |
|      4 |  153 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|      - |  154 | `#else` |
|    164 |  155 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|      - |  156 | `#endif` |
|    168 |  157 | `}` |
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
|     42 |  244 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  245 | `{` |
|     47 |  246 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  247 | `	SyString *pFile;` |
|      - |  248 | `	/* Peek the top entry */` |
|     47 |  249 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     47 |  250 | `	if( pFile == 0 ){` |
|      - |  251 | `		/* Expand the magic word: ":MEMORY:" */` |
|    ! 0 |  252 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|    ! 0 |  253 | `	}else{` |
|     47 |  254 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|      - |  255 | `	}` |
|     47 |  256 | `}` |
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
|      - |  432 | ` *  Expands 30719 (php 8: E_STRICT is no longer part of E_ALL)` |
|      - |  433 | ` */` |
|     30 |  434 | `static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      5 |  435 | `{` |
|     35 |  436 | `	ph7_value_int(pVal,30719);` |
|     15 |  437 | `	SXUNUSED(pUserData);` |
|     35 |  438 | `}` |
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
|      - | 1135 | `/* php's FNM_* values (ext/standard): PATHNAME=1, NOESCAPE=2, PERIOD=4, CASEFOLD=16.` |
|      - | 1136 | ` * PHL previously had PATHNAME/NOESCAPE swapped and CASEFOLD=8; fnmatch() reads these` |
|      - | 1137 | ` * bits, so PH7_builtin_fnmatch was updated to the same values. */` |
|      - | 1138 | `/*` |
|      - | 1139 | ` * FNM_PATHNAME` |
|      - | 1140 | ` *  Expand 1 (php value)` |
|      - | 1141 | ` */` |
|    ! 0 | 1142 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 1143 | `{` |
|    ! 0 | 1144 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 1145 | `	ph7_value_int(pVal,1);` |
|    ! 0 | 1146 | `}` |
|      - | 1147 | `/*` |
|      - | 1148 | ` * FNM_NOESCAPE` |
|      - | 1149 | ` *  Expand 2 (php value)` |
|      - | 1150 | ` */` |
|    ! 0 | 1151 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 1152 | `{` |
|    ! 0 | 1153 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 1154 | `	ph7_value_int(pVal,2);` |
|    ! 0 | 1155 | `}` |
|      - | 1156 | `/*` |
|      - | 1157 | ` * FNM_PERIOD` |
|      - | 1158 | ` *  Expand 4 (php value)` |
|      - | 1159 | ` */` |
|      6 | 1160 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1161 | `{` |
|      3 | 1162 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1163 | `	ph7_value_int(pVal,4);` |
|      7 | 1164 | `}` |
|      - | 1165 | `/*` |
|      - | 1166 | ` * FNM_CASEFOLD` |
|      - | 1167 | ` *  Expand 16 (php value)` |
|      - | 1168 | ` */` |
|      4 | 1169 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1170 | `{` |
|      2 | 1171 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1172 | `	ph7_value_int(pVal,16);` |
|      5 | 1173 | `}` |
|      - | 1174 | `/*` |
|      - | 1175 | ` * PATHINFO_DIRNAME` |
|      - | 1176 | ` *  Expand 1.` |
|      - | 1177 | ` */` |
|      4 | 1178 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1179 | `{` |
|      2 | 1180 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1181 | `	ph7_value_int(pVal,1);` |
|      5 | 1182 | `}` |
|      - | 1183 | `/*` |
|      - | 1184 | ` * PATHINFO_BASENAME` |
|      - | 1185 | ` *  Expand 2.` |
|      - | 1186 | ` */` |
|      4 | 1187 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1188 | `{` |
|      2 | 1189 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1190 | `	ph7_value_int(pVal,2);` |
|      5 | 1191 | `}` |
|      - | 1192 | `/*` |
|      - | 1193 | ` * PATHINFO_EXTENSION` |
|      - | 1194 | ` *  Expand 3.` |
|      - | 1195 | ` */` |
|   6514 | 1196 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1197 | `{` |
|   3257 | 1198 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6519 | 1199 | `	ph7_value_int(pVal,3);` |
|   6519 | 1200 | `}` |
|      - | 1201 | `/*` |
|      - | 1202 | ` * PATHINFO_FILENAME` |
|      - | 1203 | ` *  Expand 4.` |
|      - | 1204 | ` */` |
|   6506 | 1205 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1206 | `{` |
|   3253 | 1207 | `	SXUNUSED(pUserData); /* cc warning */` |
|   6511 | 1208 | `	ph7_value_int(pVal,4);` |
|   6511 | 1209 | `}` |
|      - | 1210 | `/*` |
|      - | 1211 | ` * ASSERT_ACTIVE.` |
|      - | 1212 | ` *  PHP ASSERT_ACTIVE = 1` |
|      - | 1213 | ` */` |
|     14 | 1214 | `static void PH7_ASSERT_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1215 | `{` |
|      7 | 1216 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1217 | `	ph7_value_int(pVal,1); /* PHP ASSERT_ACTIVE = 1 */` |
|     16 | 1218 | `}` |
|      - | 1219 | `/*` |
|      - | 1220 | ` * ASSERT_CALLBACK.` |
|      - | 1221 | ` *  PHP ASSERT_CALLBACK = 2` |
|      - | 1222 | ` */` |
|      6 | 1223 | `static void PH7_ASSERT_CALLBACK_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1224 | `{` |
|      3 | 1225 | `	SXUNUSED(pUserData); /* cc warning */` |
|      8 | 1226 | `	ph7_value_int(pVal,2); /* PHP ASSERT_CALLBACK = 2 */` |
|      8 | 1227 | `}` |
|      - | 1228 | `/*` |
|      - | 1229 | ` * ASSERT_BAIL.` |
|      - | 1230 | ` *  PHP ASSERT_BAIL = 3` |
|      - | 1231 | ` */` |
|     14 | 1232 | `static void PH7_ASSERT_BAIL_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1233 | `{` |
|      7 | 1234 | `	SXUNUSED(pUserData); /* cc warning */` |
|     16 | 1235 | `	ph7_value_int(pVal,3); /* PHP ASSERT_BAIL = 3 */` |
|     16 | 1236 | `}` |
|      - | 1237 | `/*` |
|      - | 1238 | ` * ASSERT_WARNING.` |
|      - | 1239 | ` *  PHP ASSERT_WARNING = 4 (deprecated in PHP 8.3)` |
|      - | 1240 | ` */` |
|      4 | 1241 | `static void PH7_ASSERT_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1242 | `{` |
|      2 | 1243 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1244 | `	ph7_value_int(pVal,4); /* PHP ASSERT_WARNING = 4 */` |
|      5 | 1245 | `}` |
|      - | 1246 | `/*` |
|      - | 1247 | ` * ASSERT_EXCEPTION.` |
|      - | 1248 | ` *  PHP ASSERT_EXCEPTION = 5 (deprecated in PHP 8.3)` |
|      - | 1249 | ` */` |
|      4 | 1250 | `static void PH7_ASSERT_EXCEPTION_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1251 | `{` |
|      2 | 1252 | `	SXUNUSED(pUserData); /* cc warning */` |
|      6 | 1253 | `	ph7_value_int(pVal,5); /* PHP ASSERT_EXCEPTION = 5 */` |
|      6 | 1254 | `}` |
|      - | 1255 | `/*` |
|      - | 1256 | ` * SEEK_SET.` |
|      - | 1257 | ` *  Expand 0` |
|      - | 1258 | ` */` |
|      2 | 1259 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1260 | `{` |
|      1 | 1261 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1262 | `	ph7_value_int(pVal,0);` |
|      3 | 1263 | `}` |
|      - | 1264 | `/*` |
|      - | 1265 | ` * SEEK_CUR.` |
|      - | 1266 | ` *  Expand 1` |
|      - | 1267 | ` */` |
|      2 | 1268 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1269 | `{` |
|      1 | 1270 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1271 | `	ph7_value_int(pVal,1);` |
|      3 | 1272 | `}` |
|      - | 1273 | `/*` |
|      - | 1274 | ` * SEEK_END.` |
|      - | 1275 | ` *  Expand 2` |
|      - | 1276 | ` */` |
|      4 | 1277 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1278 | `{` |
|      2 | 1279 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1280 | `	ph7_value_int(pVal,2);` |
|      5 | 1281 | `}` |
|      - | 1282 | `/*` |
|      - | 1283 | ` * LOCK_SH.` |
|      - | 1284 | ` *  Expand 2` |
|      - | 1285 | ` */` |
|      2 | 1286 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1287 | `{` |
|      1 | 1288 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1289 | `	ph7_value_int(pVal,1);` |
|      3 | 1290 | `}` |
|      - | 1291 | `/*` |
|      - | 1292 | ` * LOCK_NB.` |
|      - | 1293 | ` *  Expand 4 (php)` |
|      - | 1294 | ` */` |
|      2 | 1295 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1296 | `{` |
|      1 | 1297 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1298 | `	ph7_value_int(pVal,4);` |
|      3 | 1299 | `}` |
|      - | 1300 | `/*` |
|      - | 1301 | ` * LOCK_EX.` |
|      - | 1302 | ` *  Expand 2 (php). PH7 used 1, which collided with LOCK_SH, and LOCK_UN was 0 — so` |
|      - | 1303 | ` *  flock($h, LOCK_UN) asked the stream for a SHARED lock instead of releasing one.` |
|      - | 1304 | ` */` |
|      4 | 1305 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1306 | `{` |
|      2 | 1307 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1308 | `	ph7_value_int(pVal,2);` |
|      5 | 1309 | `}` |
|      - | 1310 | `/*` |
|      - | 1311 | ` * LOCK_UN.` |
|      - | 1312 | ` *  Expand 3 (php)` |
|      - | 1313 | ` */` |
|      4 | 1314 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1315 | `{` |
|      2 | 1316 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1317 | `	ph7_value_int(pVal,3);` |
|      5 | 1318 | `}` |
|      - | 1319 | `/*` |
|      - | 1320 | ` * FILE_USE_INCLUDE_PATH` |
|      - | 1321 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1322 | ` */` |
|      2 | 1323 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1324 | `{` |
|      1 | 1325 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1326 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1327 | `}` |
|      - | 1328 | `/*` |
|      - | 1329 | ` * FILE_IGNORE_NEW_LINES` |
|      - | 1330 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1331 | ` */` |
|      2 | 1332 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1333 | `{` |
|      1 | 1334 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1335 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1336 | `}` |
|      - | 1337 | `/*` |
|      - | 1338 | ` * FILE_SKIP_EMPTY_LINES` |
|      - | 1339 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1340 | ` */` |
|      2 | 1341 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1342 | `{` |
|      1 | 1343 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1344 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1345 | `}` |
|      - | 1346 | `/*` |
|      - | 1347 | ` * FILE_APPEND` |
|      - | 1348 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1349 | ` */` |
|      2 | 1350 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1351 | `{` |
|      1 | 1352 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1353 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1354 | `}` |
|      - | 1355 | `/*` |
|      - | 1356 | ` * SCANDIR_SORT_ASCENDING` |
|      - | 1357 | ` *  Expand 0` |
|      - | 1358 | ` */` |
|   1994 | 1359 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1360 | `{` |
|    997 | 1361 | `	SXUNUSED(pUserData); /* cc warning */` |
|   1999 | 1362 | `	ph7_value_int(pVal,0);` |
|   1999 | 1363 | `}` |
|      - | 1364 | `/*` |
|      - | 1365 | ` * SCANDIR_SORT_DESCENDING` |
|      - | 1366 | ` *  Expand 1` |
|      - | 1367 | ` */` |
|    998 | 1368 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      5 | 1369 | `{` |
|    499 | 1370 | `	SXUNUSED(pUserData); /* cc warning */` |
|   1003 | 1371 | `	ph7_value_int(pVal,1);` |
|   1003 | 1372 | `}` |
|      - | 1373 | `/*` |
|      - | 1374 | ` * SCANDIR_SORT_NONE` |
|      - | 1375 | ` *  Expand 2` |
|      - | 1376 | ` */` |
|      2 | 1377 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1378 | `{` |
|      1 | 1379 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1380 | `	ph7_value_int(pVal,2);` |
|      3 | 1381 | `}` |
|      - | 1382 | `/*` |
|      - | 1383 | ` * GLOB_MARK` |
|      - | 1384 | ` *  Expand 0x01 (must be a power of two)` |
|      - | 1385 | ` */` |
|      2 | 1386 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1387 | `{` |
|      1 | 1388 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1389 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1390 | `}` |
|      - | 1391 | `/*` |
|      - | 1392 | ` * GLOB_NOSORT` |
|      - | 1393 | ` *  Expand 0x02 (must be a power of two)` |
|      - | 1394 | ` */` |
|      2 | 1395 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1396 | `{` |
|      1 | 1397 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1398 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1399 | `}` |
|      - | 1400 | `/*` |
|      - | 1401 | ` * GLOB_NOCHECK` |
|      - | 1402 | ` *  Expand 0x04 (must be a power of two)` |
|      - | 1403 | ` */` |
|      2 | 1404 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1405 | `{` |
|      1 | 1406 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1407 | `	ph7_value_int(pVal,0x04);` |
|      3 | 1408 | `}` |
|      - | 1409 | `/*` |
|      - | 1410 | ` * GLOB_NOESCAPE` |
|      - | 1411 | ` *  Expand 0x08 (must be a power of two)` |
|      - | 1412 | ` */` |
|      2 | 1413 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1414 | `{` |
|      1 | 1415 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1416 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1417 | `}` |
|      - | 1418 | `/*` |
|      - | 1419 | ` * GLOB_BRACE` |
|      - | 1420 | ` *  Expand 0x10 (must be a power of two)` |
|      - | 1421 | ` */` |
|      2 | 1422 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1423 | `{` |
|      1 | 1424 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1425 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1426 | `}` |
|      - | 1427 | `/*` |
|      - | 1428 | ` * GLOB_ONLYDIR` |
|      - | 1429 | ` *  Expand 0x20 (must be a power of two)` |
|      - | 1430 | ` */` |
|      2 | 1431 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1432 | `{` |
|      1 | 1433 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1434 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1435 | `}` |
|      - | 1436 | `/*` |
|      - | 1437 | ` * GLOB_ERR` |
|      - | 1438 | ` *  Expand 0x40 (must be a power of two)` |
|      - | 1439 | ` */` |
|      2 | 1440 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1441 | `{` |
|      1 | 1442 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1443 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1444 | `}` |
|      - | 1445 | `/*` |
|      - | 1446 | ` * STDIN` |
|      - | 1447 | ` *  Expand the STDIN handle as a resource.` |
|      - | 1448 | ` */` |
|      2 | 1449 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1450 | `{` |
|      3 | 1451 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1452 | `	void *pResource;` |
|      3 | 1453 | `	pResource = PH7_ExportStdin(pVm);` |
|      3 | 1454 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1455 | `}` |
|      - | 1456 | `/*` |
|      - | 1457 | ` * STDOUT` |
|      - | 1458 | ` *   Expand the STDOUT handle as a resource.` |
|      - | 1459 | ` */` |
|      2 | 1460 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1461 | `{` |
|      3 | 1462 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1463 | `	void *pResource;` |
|      3 | 1464 | `	pResource = PH7_ExportStdout(pVm);` |
|      3 | 1465 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1466 | `}` |
|      - | 1467 | `/*` |
|      - | 1468 | ` * STDERR` |
|      - | 1469 | ` *  Expand the STDERR handle as a resource.` |
|      - | 1470 | ` */` |
|      2 | 1471 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1472 | `{` |
|      3 | 1473 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1474 | `	void *pResource;` |
|      3 | 1475 | `	pResource = PH7_ExportStderr(pVm);` |
|      3 | 1476 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1477 | `}` |
|      - | 1478 | `/*` |
|      - | 1479 | ` * INI_SCANNER_NORMAL` |
|      - | 1480 | ` *   Expand 1` |
|      - | 1481 | ` */` |
|      2 | 1482 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1483 | `{` |
|      1 | 1484 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1485 | `	ph7_value_int(pVal,1);` |
|      3 | 1486 | `}` |
|      - | 1487 | `/*` |
|      - | 1488 | ` * INI_SCANNER_RAW` |
|      - | 1489 | ` *   Expand 2` |
|      - | 1490 | ` */` |
|      2 | 1491 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1492 | `{` |
|      1 | 1493 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1494 | `	ph7_value_int(pVal,2);` |
|      3 | 1495 | `}` |
|      - | 1496 | `/*` |
|      - | 1497 | ` * EXTR_OVERWRITE` |
|      - | 1498 | ` *   Expand 0x01 (Must be a power of two)` |
|      - | 1499 | ` */` |
|      2 | 1500 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1501 | `{` |
|      1 | 1502 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1503 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1504 | `}` |
|      - | 1505 | `/*` |
|      - | 1506 | ` * EXTR_SKIP` |
|      - | 1507 | ` *   Expand 0x02 (Must be a power of two)` |
|      - | 1508 | ` */` |
|      2 | 1509 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1510 | `{` |
|      1 | 1511 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1512 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1513 | `}` |
|      - | 1514 | `/*` |
|      - | 1515 | ` * EXTR_PREFIX_SAME` |
|      - | 1516 | ` *   Expand 0x04 (Must be a power of two)` |
|      - | 1517 | ` */` |
|      2 | 1518 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1519 | `{` |
|      1 | 1520 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1521 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1522 | `}` |
|      - | 1523 | `/*` |
|      - | 1524 | ` * EXTR_PREFIX_ALL` |
|      - | 1525 | ` *   Expand 0x08 (Must be a power of two)` |
|      - | 1526 | ` */` |
|      2 | 1527 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1528 | `{` |
|      1 | 1529 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1530 | `	ph7_value_int(pVal,0x8);` |
|      3 | 1531 | `}` |
|      - | 1532 | `/*` |
|      - | 1533 | ` * EXTR_PREFIX_INVALID` |
|      - | 1534 | ` *   Expand 0x10 (Must be a power of two)` |
|      - | 1535 | ` */` |
|      2 | 1536 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1537 | `{` |
|      1 | 1538 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1539 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1540 | `}` |
|      - | 1541 | `/*` |
|      - | 1542 | ` * EXTR_IF_EXISTS` |
|      - | 1543 | ` *   Expand 0x20 (Must be a power of two)` |
|      - | 1544 | ` */` |
|      2 | 1545 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1546 | `{` |
|      1 | 1547 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1548 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1549 | `}` |
|      - | 1550 | `/*` |
|      - | 1551 | ` * EXTR_PREFIX_IF_EXISTS` |
|      - | 1552 | ` *   Expand 0x40 (Must be a power of two)` |
|      - | 1553 | ` */` |
|      2 | 1554 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1555 | `{` |
|      1 | 1556 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1557 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1558 | `}` |
|      - | 1559 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1560 | `/*` |
|      - | 1561 | ` * XML_ERROR_NONE` |
|      - | 1562 | ` *   Expand the value of SXML_ERROR_NO_MEMORY defined in ph7Int.h` |
|      - | 1563 | ` */` |
|      2 | 1564 | `static void PH7_XML_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1565 | `{` |
|      1 | 1566 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1567 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1568 | `}` |
|      - | 1569 | `/*` |
|      - | 1570 | ` * XML_ERROR_NO_MEMORY` |
|      - | 1571 | ` *   Expand the value of SXML_ERROR_NONE defined in ph7Int.h` |
|      - | 1572 | ` */` |
|      2 | 1573 | `static void PH7_XML_ERROR_NO_MEMORY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1574 | `{` |
|      1 | 1575 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1576 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1577 | `}` |
|      - | 1578 | `/*` |
|      - | 1579 | ` * XML_ERROR_SYNTAX` |
|      - | 1580 | ` *   Expand the value of SXML_ERROR_SYNTAX defined in ph7Int.h` |
|      - | 1581 | ` */` |
|      2 | 1582 | `static void PH7_XML_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1583 | `{` |
|      1 | 1584 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1585 | `	ph7_value_int(pVal,SXML_ERROR_SYNTAX);` |
|      3 | 1586 | `}` |
|      - | 1587 | `/*` |
|      - | 1588 | ` * XML_ERROR_NO_ELEMENTS` |
|      - | 1589 | ` *   Expand the value of SXML_ERROR_NO_ELEMENTS defined in ph7Int.h` |
|      - | 1590 | ` */` |
|      2 | 1591 | `static void PH7_XML_ERROR_NO_ELEMENTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1592 | `{` |
|      1 | 1593 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1594 | `	ph7_value_int(pVal,SXML_ERROR_NO_ELEMENTS);` |
|      3 | 1595 | `}` |
|      - | 1596 | `/*` |
|      - | 1597 | ` * XML_ERROR_INVALID_TOKEN` |
|      - | 1598 | ` *   Expand the value of SXML_ERROR_INVALID_TOKEN defined in ph7Int.h` |
|      - | 1599 | ` */` |
|      2 | 1600 | `static void PH7_XML_ERROR_INVALID_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1601 | `{` |
|      1 | 1602 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1603 | `	ph7_value_int(pVal,SXML_ERROR_INVALID_TOKEN);` |
|      3 | 1604 | `}` |
|      - | 1605 | `/*` |
|      - | 1606 | ` * XML_ERROR_UNCLOSED_TOKEN` |
|      - | 1607 | ` *   Expand the value of SXML_ERROR_UNCLOSED_TOKEN defined in ph7Int.h` |
|      - | 1608 | ` */` |
|      2 | 1609 | `static void PH7_XML_ERROR_UNCLOSED_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1610 | `{` |
|      1 | 1611 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1612 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_TOKEN);` |
|      3 | 1613 | `}` |
|      - | 1614 | `/*` |
|      - | 1615 | ` * XML_ERROR_PARTIAL_CHAR` |
|      - | 1616 | ` *   Expand the value of SXML_ERROR_PARTIAL_CHAR defined in ph7Int.h` |
|      - | 1617 | ` */` |
|      2 | 1618 | `static void PH7_XML_ERROR_PARTIAL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1619 | `{` |
|      1 | 1620 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1621 | `	ph7_value_int(pVal,SXML_ERROR_PARTIAL_CHAR);` |
|      3 | 1622 | `}` |
|      - | 1623 | `/*` |
|      - | 1624 | ` * XML_ERROR_TAG_MISMATCH` |
|      - | 1625 | ` *   Expand the value of SXML_ERROR_TAG_MISMATCH defined in ph7Int.h` |
|      - | 1626 | ` */` |
|      2 | 1627 | `static void PH7_XML_ERROR_TAG_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1628 | `{` |
|      1 | 1629 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1630 | `	ph7_value_int(pVal,SXML_ERROR_TAG_MISMATCH);` |
|      3 | 1631 | `}` |
|      - | 1632 | `/*` |
|      - | 1633 | ` * XML_ERROR_DUPLICATE_ATTRIBUTE` |
|      - | 1634 | ` *   Expand the value of SXML_ERROR_DUPLICATE_ATTRIBUTE defined in ph7Int.h` |
|      - | 1635 | ` */` |
|      2 | 1636 | `static void PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1637 | `{` |
|      1 | 1638 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1639 | `	ph7_value_int(pVal,SXML_ERROR_DUPLICATE_ATTRIBUTE);` |
|      3 | 1640 | `}` |
|      - | 1641 | `/*` |
|      - | 1642 | ` * XML_ERROR_JUNK_AFTER_DOC_ELEMENT` |
|      - | 1643 | ` *   Expand the value of SXML_ERROR_JUNK_AFTER_DOC_ELEMENT defined in ph7Int.h` |
|      - | 1644 | ` */` |
|      2 | 1645 | `static void PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1646 | `{` |
|      1 | 1647 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1648 | `	ph7_value_int(pVal,SXML_ERROR_JUNK_AFTER_DOC_ELEMENT);` |
|      3 | 1649 | `}` |
|      - | 1650 | `/*` |
|      - | 1651 | ` * XML_ERROR_PARAM_ENTITY_REF` |
|      - | 1652 | ` *   Expand the value of SXML_ERROR_PARAM_ENTITY_REF defined in ph7Int.h` |
|      - | 1653 | ` */` |
|      2 | 1654 | `static void PH7_XML_ERROR_PARAM_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1655 | `{` |
|      1 | 1656 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1657 | `	ph7_value_int(pVal,SXML_ERROR_PARAM_ENTITY_REF);` |
|      3 | 1658 | `}` |
|      - | 1659 | `/*` |
|      - | 1660 | ` * XML_ERROR_UNDEFINED_ENTITY` |
|      - | 1661 | ` *   Expand the value of SXML_ERROR_UNDEFINED_ENTITY defined in ph7Int.h` |
|      - | 1662 | ` */` |
|      2 | 1663 | `static void PH7_XML_ERROR_UNDEFINED_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1664 | `{` |
|      1 | 1665 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1666 | `	ph7_value_int(pVal,SXML_ERROR_UNDEFINED_ENTITY);` |
|      3 | 1667 | `}` |
|      - | 1668 | `/*` |
|      - | 1669 | ` * XML_ERROR_RECURSIVE_ENTITY_REF` |
|      - | 1670 | ` *   Expand the value of SXML_ERROR_RECURSIVE_ENTITY_REF defined in ph7Int.h` |
|      - | 1671 | ` */` |
|      2 | 1672 | `static void PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1673 | `{` |
|      1 | 1674 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1675 | `	ph7_value_int(pVal,SXML_ERROR_RECURSIVE_ENTITY_REF);` |
|      3 | 1676 | `}` |
|      - | 1677 | `/*` |
|      - | 1678 | ` * XML_ERROR_ASYNC_ENTITY` |
|      - | 1679 | ` *   Expand the value of SXML_ERROR_ASYNC_ENTITY defined in ph7Int.h` |
|      - | 1680 | ` */` |
|      2 | 1681 | `static void PH7_XML_ERROR_ASYNC_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1682 | `{` |
|      1 | 1683 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1684 | `	ph7_value_int(pVal,SXML_ERROR_ASYNC_ENTITY);` |
|      3 | 1685 | `}` |
|      - | 1686 | `/*` |
|      - | 1687 | ` * XML_ERROR_BAD_CHAR_REF` |
|      - | 1688 | ` *   Expand the value of SXML_ERROR_BAD_CHAR_REF defined in ph7Int.h` |
|      - | 1689 | ` */` |
|      2 | 1690 | `static void PH7_XML_ERROR_BAD_CHAR_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1691 | `{` |
|      1 | 1692 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1693 | `	ph7_value_int(pVal,SXML_ERROR_BAD_CHAR_REF);` |
|      3 | 1694 | `}` |
|      - | 1695 | `/*` |
|      - | 1696 | ` * XML_ERROR_BINARY_ENTITY_REF` |
|      - | 1697 | ` *   Expand the value of SXML_ERROR_BINARY_ENTITY_REF defined in ph7Int.h` |
|      - | 1698 | ` */` |
|      2 | 1699 | `static void PH7_XML_ERROR_BINARY_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1700 | `{` |
|      1 | 1701 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1702 | `	ph7_value_int(pVal,SXML_ERROR_BINARY_ENTITY_REF);` |
|      3 | 1703 | `}` |
|      - | 1704 | `/*` |
|      - | 1705 | ` * XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF` |
|      - | 1706 | ` *   Expand the value of SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF defined in ph7Int.h` |
|      - | 1707 | ` */` |
|      2 | 1708 | `static void PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1709 | `{` |
|      1 | 1710 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1711 | `	ph7_value_int(pVal,SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF);` |
|      3 | 1712 | `}` |
|      - | 1713 | `/*` |
|      - | 1714 | ` * XML_ERROR_MISPLACED_XML_PI` |
|      - | 1715 | ` *   Expand the value of SXML_ERROR_MISPLACED_XML_PI defined in ph7Int.h` |
|      - | 1716 | ` */` |
|      2 | 1717 | `static void PH7_XML_ERROR_MISPLACED_XML_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1718 | `{` |
|      1 | 1719 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1720 | `	ph7_value_int(pVal,SXML_ERROR_MISPLACED_XML_PI);` |
|      3 | 1721 | `}` |
|      - | 1722 | `/*` |
|      - | 1723 | ` * XML_ERROR_UNKNOWN_ENCODING` |
|      - | 1724 | ` *   Expand the value of SXML_ERROR_UNKNOWN_ENCODING defined in ph7Int.h` |
|      - | 1725 | ` */` |
|      2 | 1726 | `static void PH7_XML_ERROR_UNKNOWN_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1727 | `{` |
|      1 | 1728 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1729 | `	ph7_value_int(pVal,SXML_ERROR_UNKNOWN_ENCODING);` |
|      3 | 1730 | `}` |
|      - | 1731 | `/*` |
|      - | 1732 | ` * XML_ERROR_INCORRECT_ENCODING` |
|      - | 1733 | ` *   Expand the value of SXML_ERROR_INCORRECT_ENCODING defined in ph7Int.h` |
|      - | 1734 | ` */` |
|      2 | 1735 | `static void PH7_XML_ERROR_INCORRECT_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1736 | `{` |
|      1 | 1737 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1738 | `	ph7_value_int(pVal,SXML_ERROR_INCORRECT_ENCODING);` |
|      3 | 1739 | `}` |
|      - | 1740 | `/*` |
|      - | 1741 | ` * XML_ERROR_UNCLOSED_CDATA_SECTION` |
|      - | 1742 | ` *   Expand the value of SXML_ERROR_UNCLOSED_CDATA_SECTION defined in ph7Int.h` |
|      - | 1743 | ` */` |
|      2 | 1744 | `static void PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1745 | `{` |
|      1 | 1746 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1747 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_CDATA_SECTION);` |
|      3 | 1748 | `}` |
|      - | 1749 | `/*` |
|      - | 1750 | ` * XML_ERROR_EXTERNAL_ENTITY_HANDLING` |
|      - | 1751 | ` *   Expand the value of SXML_ERROR_EXTERNAL_ENTITY_HANDLING defined in ph7Int.h` |
|      - | 1752 | ` */` |
|      2 | 1753 | `static void PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1754 | `{` |
|      1 | 1755 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1756 | `	ph7_value_int(pVal,SXML_ERROR_EXTERNAL_ENTITY_HANDLING);` |
|      3 | 1757 | `}` |
|      - | 1758 | `/*` |
|      - | 1759 | ` * XML_OPTION_CASE_FOLDING` |
|      - | 1760 | ` *   Expand the value of SXML_OPTION_CASE_FOLDING defined in ph7Int.h.` |
|      - | 1761 | ` */` |
|      2 | 1762 | `static void PH7_XML_OPTION_CASE_FOLDING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1763 | `{` |
|      1 | 1764 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1765 | `	ph7_value_int(pVal,SXML_OPTION_CASE_FOLDING);` |
|      3 | 1766 | `}` |
|      - | 1767 | `/*` |
|      - | 1768 | ` * XML_OPTION_TARGET_ENCODING` |
|      - | 1769 | ` *   Expand the value of SXML_OPTION_TARGET_ENCODING defined in ph7Int.h.` |
|      - | 1770 | ` */` |
|      4 | 1771 | `static void PH7_XML_OPTION_TARGET_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1772 | `{` |
|      2 | 1773 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1774 | `	ph7_value_int(pVal,SXML_OPTION_TARGET_ENCODING);` |
|      5 | 1775 | `}` |
|      - | 1776 | `/*` |
|      - | 1777 | ` * XML_OPTION_SKIP_TAGSTART` |
|      - | 1778 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1779 | ` */` |
|      2 | 1780 | `static void PH7_XML_OPTION_SKIP_TAGSTART_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1781 | `{` |
|      1 | 1782 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1783 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_TAGSTART);` |
|      3 | 1784 | `}` |
|      - | 1785 | `/*` |
|      - | 1786 | ` * XML_OPTION_SKIP_WHITE` |
|      - | 1787 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1788 | ` */` |
|      4 | 1789 | `static void PH7_XML_OPTION_SKIP_WHITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1790 | `{` |
|      2 | 1791 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1792 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_WHITE);` |
|      5 | 1793 | `}` |
|      - | 1794 | `/*` |
|      - | 1795 | ` * XML_SAX_IMPL.` |
|      - | 1796 | ` *   Expand the name of the underlying XML engine.` |
|      - | 1797 | ` */` |
|      2 | 1798 | `static void PH7_XML_SAX_IMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1799 | `{` |
|      1 | 1800 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1801 | `	ph7_value_string(pVal,"Symisc XML engine",(int)sizeof("Symisc XML engine")-1);` |
|      3 | 1802 | `}` |
|      - | 1803 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1804 | `/*` |
|      - | 1805 | ` * JSON_HEX_TAG.` |
|      - | 1806 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|      - | 1807 | ` */` |
|      2 | 1808 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1809 | `{` |
|      1 | 1810 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1811 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      3 | 1812 | `}` |
|      - | 1813 | `/*` |
|      - | 1814 | ` * JSON_HEX_AMP.` |
|      - | 1815 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|      - | 1816 | ` */` |
|      2 | 1817 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1818 | `{` |
|      1 | 1819 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1820 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      3 | 1821 | `}` |
|      - | 1822 | `/*` |
|      - | 1823 | ` * JSON_HEX_APOS.` |
|      - | 1824 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|      - | 1825 | ` */` |
|      2 | 1826 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1827 | `{` |
|      1 | 1828 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1829 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      3 | 1830 | `}` |
|      - | 1831 | `/*` |
|      - | 1832 | ` * JSON_HEX_QUOT.` |
|      - | 1833 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|      - | 1834 | ` */` |
|      2 | 1835 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1836 | `{` |
|      1 | 1837 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1838 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      3 | 1839 | `}` |
|      - | 1840 | `/*` |
|      - | 1841 | ` * JSON_FORCE_OBJECT.` |
|      - | 1842 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|      - | 1843 | ` */` |
|      4 | 1844 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1845 | `{` |
|      2 | 1846 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1847 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      5 | 1848 | `}` |
|      - | 1849 | `/*` |
|      - | 1850 | ` * JSON_NUMERIC_CHECK.` |
|      - | 1851 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|      - | 1852 | ` */` |
|      4 | 1853 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1854 | `{` |
|      2 | 1855 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1856 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      5 | 1857 | `}` |
|      - | 1858 | `/*` |
|      - | 1859 | ` * JSON_BIGINT_AS_STRING.` |
|      - | 1860 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|      - | 1861 | ` */` |
|      2 | 1862 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1863 | `{` |
|      1 | 1864 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1865 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      3 | 1866 | `}` |
|      - | 1867 | `/*` |
|      - | 1868 | ` * JSON_PRETTY_PRINT.` |
|      - | 1869 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|      - | 1870 | ` */` |
|      2 | 1871 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1872 | `{` |
|      1 | 1873 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1874 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      3 | 1875 | `}` |
|      - | 1876 | `/*` |
|      - | 1877 | ` * JSON_UNESCAPED_SLASHES.` |
|      - | 1878 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|      - | 1879 | ` */` |
|      4 | 1880 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1881 | `{` |
|      2 | 1882 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1883 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      5 | 1884 | `}` |
|      - | 1885 | `/*` |
|      - | 1886 | ` * JSON_UNESCAPED_UNICODE.` |
|      - | 1887 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|      - | 1888 | ` */` |
|      2 | 1889 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1890 | `{` |
|      1 | 1891 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1892 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|      3 | 1893 | `}` |
|      - | 1894 | `/*` |
|      - | 1895 | ` * JSON_ERROR_NONE.` |
|      - | 1896 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|      - | 1897 | ` */` |
|      4 | 1898 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1899 | `{` |
|      2 | 1900 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1901 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      5 | 1902 | `}` |
|      - | 1903 | `/*` |
|      - | 1904 | ` * JSON_ERROR_DEPTH.` |
|      - | 1905 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|      - | 1906 | ` */` |
|      2 | 1907 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1908 | `{` |
|      1 | 1909 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1910 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      3 | 1911 | `}` |
|      - | 1912 | `/*` |
|      - | 1913 | ` * JSON_ERROR_STATE_MISMATCH.` |
|      - | 1914 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|      - | 1915 | ` */` |
|      2 | 1916 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1917 | `{` |
|      1 | 1918 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1919 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      3 | 1920 | `}` |
|      - | 1921 | `/*` |
|      - | 1922 | ` * JSON_ERROR_CTRL_CHAR.` |
|      - | 1923 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|      - | 1924 | ` */` |
|      2 | 1925 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1926 | `{` |
|      1 | 1927 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1928 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      3 | 1929 | `}` |
|      - | 1930 | `/*` |
|      - | 1931 | ` * JSON_ERROR_SYNTAX.` |
|      - | 1932 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|      - | 1933 | ` */` |
|      4 | 1934 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1935 | `{` |
|      2 | 1936 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1937 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      5 | 1938 | `}` |
|      - | 1939 | `/*` |
|      - | 1940 | ` * JSON_ERROR_UTF8.` |
|      - | 1941 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|      - | 1942 | ` */` |
|      2 | 1943 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1944 | `{` |
|      1 | 1945 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1946 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      3 | 1947 | `}` |
|      - | 1948 | `/*` |
|      - | 1949 | ` * JSON_ERROR_NON_BACKED_ENUM.` |
|      - | 1950 | ` *   Expand the value of JSON_ERROR_NON_BACKED_ENUM defined in ph7Int.h (php 8.1).` |
|      - | 1951 | ` */` |
|    ! 0 | 1952 | `static void PH7_JSON_ERROR_NON_BACKED_ENUM_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 | 1953 | `{` |
|    ! 0 | 1954 | `	SXUNUSED(pUserData); /* cc warning */` |
|    ! 0 | 1955 | `	ph7_value_int(pVal,JSON_ERROR_NON_BACKED_ENUM);` |
|    ! 0 | 1956 | `}` |
|      - | 1957 | `/*` |
|      - | 1958 | ` * __CLASS__` |
|      - | 1959 | ` *  The current class name, or the EMPTY STRING outside any class — php answers "",` |
|      - | 1960 | `` *  not null (`__CLASS__ === ""` is true in global scope). `self` keeps its own`` |
|      - | 1961 | ` *  expander below because php treats IT differently outside a class scope.` |
|      - | 1962 | ` */` |
|      2 | 1963 | `static void PH7_class_magic_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1964 | `{` |
|      3 | 1965 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1966 | `	ph7_class *pClass;` |
|      3 | 1967 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      3 | 1968 | `	if( pClass == 0 ){` |
|      3 | 1969 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      1 | 1970 | `	}` |
|      3 | 1971 | `	if( pClass ){` |
|    ! 0 | 1972 | `		SyString *pName = &pClass->sName;` |
|    ! 0 | 1973 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|    ! 0 | 1974 | `	}else{` |
|      3 | 1975 | `		ph7_value_string(pVal,"",0);` |
|      - | 1976 | `	}` |
|      3 | 1977 | `}` |
|      - | 1978 |  |
|      - | 1979 | `/*` |
|      - | 1980 | ` * PASSWORD_BCRYPT / PASSWORD_DEFAULT` |
|      - | 1981 | ` *  The bcrypt algorithm identifier (PHP 7.4+ exposes these as the string "2y").` |
|      - | 1982 | ` *  PASSWORD_DEFAULT tracks the recommended default, currently bcrypt.` |
|      - | 1983 | ` */` |
|     20 | 1984 | `static void PH7_PASSWORD_BCRYPT_Const(ph7_value *pVal,void *pUnused)` |
|      2 | 1985 | `{` |
|     10 | 1986 | `	SXUNUSED(pUnused);` |
|     22 | 1987 | `	ph7_value_string(pVal,"2y",(int)sizeof("2y")-1);` |
|     22 | 1988 | `}` |
|      - | 1989 | `/*` |
|      - | 1990 | ` * PASSWORD_BCRYPT_DEFAULT_COST` |
|      - | 1991 | ` *  The default bcrypt work factor used by password_hash() (currently 12).` |
|      - | 1992 | ` */` |
|      2 | 1993 | `static void PH7_PASSWORD_COST_Const(ph7_value *pVal,void *pUnused)` |
|      1 | 1994 | `{` |
|      1 | 1995 | `	SXUNUSED(pUnused);` |
|      3 | 1996 | `	ph7_value_int(pVal,12);` |
|      3 | 1997 | `}` |
|      - | 1998 | `/*` |
|      - | 1999 | ` * filter_var() filter and flag identifiers (the ext/filter constants). Values` |
|      - | 2000 | ` * match PHP 8.5. One tiny int-returning callback per constant, generated by a` |
|      - | 2001 | ` * local macro to keep the ~25 near-identical definitions DRY.` |
|      - | 2002 | ` */` |
|      - | 2003 | `#define PH7_FILTER_INT_CONST(Name,Val) \` |
|      - | 2004 | `	static void PH7_##Name##_Const(ph7_value *pVal,void *pUnused){ \` |
|      - | 2005 | `		SXUNUSED(pUnused); ph7_value_int(pVal,Val); \` |
|      - | 2006 | `	}` |
|     10 | 2007 | `PH7_FILTER_INT_CONST(FILTER_DEFAULT,516)` |
|     17 | 2008 | `PH7_FILTER_INT_CONST(FILTER_UNSAFE_RAW,516)` |
|     64 | 2009 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_INT,257)` |
|     29 | 2010 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_BOOLEAN,258)` |
|     69 | 2011 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_FLOAT,259)` |
|      8 | 2012 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_REGEXP,272)` |
|     11 | 2013 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_DOMAIN,277)` |
|     15 | 2014 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_URL,273)` |
|     28 | 2015 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_EMAIL,274)` |
|     25 | 2016 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_IP,275)` |
|     11 | 2017 | `PH7_FILTER_INT_CONST(FILTER_VALIDATE_MAC,276)` |
|      3 | 2018 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_INT,519)` |
|      5 | 2019 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_NUMBER_FLOAT,520)` |
|     13 | 2020 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_SPECIAL_CHARS,515)` |
|     25 | 2021 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_FULL_SPECIAL_CHARS,522)` |
|      3 | 2022 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_EMAIL,517)` |
|      3 | 2023 | `PH7_FILTER_INT_CONST(FILTER_SANITIZE_URL,518)` |
|      3 | 2024 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_OCTAL,1)` |
|      3 | 2025 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_HEX,2)` |
|      7 | 2026 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_LOW,4)` |
|      5 | 2027 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_HIGH,8)` |
|      5 | 2028 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_LOW,16)` |
|      5 | 2029 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_HIGH,32)` |
|      3 | 2030 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ENCODE_AMP,64)` |
|      3 | 2031 | `PH7_FILTER_INT_CONST(FILTER_FLAG_NO_ENCODE_QUOTES,128)` |
|      3 | 2032 | `PH7_FILTER_INT_CONST(FILTER_FLAG_STRIP_BACKTICK,512)` |
|      3 | 2033 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_FRACTION,4096)` |
|     25 | 2034 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_THOUSAND,8192)` |
|      3 | 2035 | `PH7_FILTER_INT_CONST(FILTER_FLAG_ALLOW_SCIENTIFIC,16384)` |
|      5 | 2036 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV4,1048576)` |
|      3 | 2037 | `PH7_FILTER_INT_CONST(FILTER_FLAG_IPV6,2097152)` |
|     14 | 2038 | `PH7_FILTER_INT_CONST(FILTER_NULL_ON_FAILURE,134217728)` |
|      - | 2039 | `/* filter_input() source selectors (php values; SESSION/REQUEST are undefined in 8.5) */` |
|      5 | 2040 | `PH7_FILTER_INT_CONST(INPUT_POST,0)` |
|      8 | 2041 | `PH7_FILTER_INT_CONST(INPUT_GET,1)` |
|      3 | 2042 | `PH7_FILTER_INT_CONST(INPUT_COOKIE,2)` |
|      3 | 2043 | `PH7_FILTER_INT_CONST(INPUT_ENV,4)` |
|     21 | 2044 | `PH7_FILTER_INT_CONST(INPUT_SERVER,5)` |
|      - | 2045 | `/*` |
|      - | 2046 | ` * Table of built-in constants.` |
|      - | 2047 | ` */` |
|      - | 2048 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|      - | 2049 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|      - | 2050 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|      - | 2051 | `	{"__PH7__",              PH7_VER_Const      },` |
|      - | 2052 | `	{"PHP_VERSION",          PH7_PHPVerConst    },` |
|      - | 2053 | `	{"PHP_MAJOR_VERSION",    PH7_PHPMajorConst  },` |
|      - | 2054 | `	{"PHP_MINOR_VERSION",    PH7_PHPMinorConst  },` |
|      - | 2055 | `	{"PHP_RELEASE_VERSION",  PH7_PHPReleaseConst},` |
|      - | 2056 | `	{"PHP_EXTRA_VERSION",    PH7_PHPExtraConst  },` |
|      - | 2057 | `	{"PHP_VERSION_ID",       PH7_PHPVerIdConst  },` |
|      - | 2058 | `	{"PHP_OS",               PH7_OS_Const       },` |
|      - | 2059 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|      - | 2060 | `	{"PHP_SESSION_DISABLED", PH7_PHP_SESSION_DISABLED_Const },` |
|      - | 2061 | `	{"PHP_SESSION_NONE",     PH7_PHP_SESSION_NONE_Const },` |
|      - | 2062 | `	{"PHP_SESSION_ACTIVE",   PH7_PHP_SESSION_ACTIVE_Const },` |
|      - | 2063 | `	{"INI_USER",             PH7_INI_USER_Const },` |
|      - | 2064 | `	{"INI_PERDIR",           PH7_INI_PERDIR_Const },` |
|      - | 2065 | `	{"INI_SYSTEM",           PH7_INI_SYSTEM_Const },` |
|      - | 2066 | `	{"INI_ALL",              PH7_INI_ALL_Const },` |
|      - | 2067 | `	{"MB_CASE_UPPER",        PH7_MB_CASE_UPPER_Const },` |
|      - | 2068 | `	{"MB_CASE_LOWER",        PH7_MB_CASE_LOWER_Const },` |
|      - | 2069 | `	{"MB_CASE_TITLE",        PH7_MB_CASE_TITLE_Const },` |
|      - | 2070 | `	{"PASSWORD_BCRYPT",      PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2071 | `	{"PASSWORD_DEFAULT",     PH7_PASSWORD_BCRYPT_Const },` |
|      - | 2072 | `	{"PASSWORD_BCRYPT_DEFAULT_COST", PH7_PASSWORD_COST_Const },` |
|      - | 2073 | `	{"FILTER_DEFAULT",              PH7_FILTER_DEFAULT_Const },` |
|      - | 2074 | `	{"FILTER_UNSAFE_RAW",           PH7_FILTER_UNSAFE_RAW_Const },` |
|      - | 2075 | `	{"FILTER_VALIDATE_INT",         PH7_FILTER_VALIDATE_INT_Const },` |
|      - | 2076 | `	{"FILTER_VALIDATE_BOOLEAN",     PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2077 | `	{"FILTER_VALIDATE_BOOL",        PH7_FILTER_VALIDATE_BOOLEAN_Const },` |
|      - | 2078 | `	{"FILTER_VALIDATE_FLOAT",       PH7_FILTER_VALIDATE_FLOAT_Const },` |
|      - | 2079 | `	{"FILTER_VALIDATE_REGEXP",      PH7_FILTER_VALIDATE_REGEXP_Const },` |
|      - | 2080 | `	{"FILTER_VALIDATE_DOMAIN",      PH7_FILTER_VALIDATE_DOMAIN_Const },` |
|      - | 2081 | `	{"FILTER_VALIDATE_URL",         PH7_FILTER_VALIDATE_URL_Const },` |
|      - | 2082 | `	{"FILTER_VALIDATE_EMAIL",       PH7_FILTER_VALIDATE_EMAIL_Const },` |
|      - | 2083 | `	{"FILTER_VALIDATE_IP",          PH7_FILTER_VALIDATE_IP_Const },` |
|      - | 2084 | `	{"FILTER_VALIDATE_MAC",         PH7_FILTER_VALIDATE_MAC_Const },` |
|      - | 2085 | `	{"FILTER_SANITIZE_NUMBER_INT",  PH7_FILTER_SANITIZE_NUMBER_INT_Const },` |
|      - | 2086 | `	{"FILTER_SANITIZE_NUMBER_FLOAT",PH7_FILTER_SANITIZE_NUMBER_FLOAT_Const },` |
|      - | 2087 | `	{"FILTER_SANITIZE_SPECIAL_CHARS",PH7_FILTER_SANITIZE_SPECIAL_CHARS_Const },` |
|      - | 2088 | `	{"FILTER_SANITIZE_FULL_SPECIAL_CHARS",PH7_FILTER_SANITIZE_FULL_SPECIAL_CHARS_Const },` |
|      - | 2089 | `	{"FILTER_SANITIZE_EMAIL",       PH7_FILTER_SANITIZE_EMAIL_Const },` |
|      - | 2090 | `	{"FILTER_SANITIZE_URL",         PH7_FILTER_SANITIZE_URL_Const },` |
|      - | 2091 | `	{"FILTER_FLAG_ALLOW_OCTAL",     PH7_FILTER_FLAG_ALLOW_OCTAL_Const },` |
|      - | 2092 | `	{"FILTER_FLAG_ALLOW_HEX",       PH7_FILTER_FLAG_ALLOW_HEX_Const },` |
|      - | 2093 | `	{"FILTER_FLAG_STRIP_LOW",       PH7_FILTER_FLAG_STRIP_LOW_Const },` |
|      - | 2094 | `	{"FILTER_FLAG_STRIP_HIGH",      PH7_FILTER_FLAG_STRIP_HIGH_Const },` |
|      - | 2095 | `	{"FILTER_FLAG_ENCODE_LOW",      PH7_FILTER_FLAG_ENCODE_LOW_Const },` |
|      - | 2096 | `	{"FILTER_FLAG_ENCODE_HIGH",     PH7_FILTER_FLAG_ENCODE_HIGH_Const },` |
|      - | 2097 | `	{"FILTER_FLAG_ENCODE_AMP",      PH7_FILTER_FLAG_ENCODE_AMP_Const },` |
|      - | 2098 | `	{"FILTER_FLAG_NO_ENCODE_QUOTES",PH7_FILTER_FLAG_NO_ENCODE_QUOTES_Const },` |
|      - | 2099 | `	{"FILTER_FLAG_STRIP_BACKTICK",  PH7_FILTER_FLAG_STRIP_BACKTICK_Const },` |
|      - | 2100 | `	{"FILTER_FLAG_ALLOW_FRACTION",  PH7_FILTER_FLAG_ALLOW_FRACTION_Const },` |
|      - | 2101 | `	{"FILTER_FLAG_ALLOW_THOUSAND",  PH7_FILTER_FLAG_ALLOW_THOUSAND_Const },` |
|      - | 2102 | `	{"FILTER_FLAG_ALLOW_SCIENTIFIC",PH7_FILTER_FLAG_ALLOW_SCIENTIFIC_Const },` |
|      - | 2103 | `	{"FILTER_FLAG_IPV4",            PH7_FILTER_FLAG_IPV4_Const },` |
|      - | 2104 | `	{"FILTER_FLAG_IPV6",            PH7_FILTER_FLAG_IPV6_Const },` |
|      - | 2105 | `	{"FILTER_NULL_ON_FAILURE",      PH7_FILTER_NULL_ON_FAILURE_Const },` |
|      - | 2106 | `	{"INPUT_POST",                  PH7_INPUT_POST_Const },` |
|      - | 2107 | `	{"INPUT_GET",                   PH7_INPUT_GET_Const },` |
|      - | 2108 | `	{"INPUT_COOKIE",                PH7_INPUT_COOKIE_Const },` |
|      - | 2109 | `	{"INPUT_ENV",                   PH7_INPUT_ENV_Const },` |
|      - | 2110 | `	{"INPUT_SERVER",                PH7_INPUT_SERVER_Const },` |
|      - | 2111 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|      - | 2112 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|      - | 2113 | `	{"PHP_INT_MIN",          PH7_INTMIN_Const   },` |
|      - | 2114 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|      - | 2115 | `	{"PHP_FLOAT_EPSILON",    PH7_FLOATEPSILON_Const },` |
|      - | 2116 | `	{"PHP_FLOAT_MAX",        PH7_FLOATMAX_Const },` |
|      - | 2117 | `	{"PHP_FLOAT_MIN",        PH7_FLOATMIN_Const },` |
|      - | 2118 | `	{"PHP_FLOAT_DIG",        PH7_FLOATDIG_Const },` |
|      - | 2119 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|      - | 2120 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|      - | 2121 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|      - | 2122 | `	{"__TIME__",             PH7_TIME_Const     },` |
|      - | 2123 | `	{"__DATE__",             PH7_DATE_Const     },` |
|      - | 2124 | `	{"__FILE__",             PH7_FILE_Const     },` |
|      - | 2125 | `	{"__DIR__",              PH7_DIR_Const      },` |
|      - | 2126 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|      - | 2127 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|      - | 2128 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|      - | 2129 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|      - | 2130 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|      - | 2131 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|      - | 2132 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|      - | 2133 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|      - | 2134 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|      - | 2135 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|      - | 2136 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|      - | 2137 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|      - | 2138 | `	{"E_STRICT",             PH7_E_STRICT_Const        },` |
|      - | 2139 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|      - | 2140 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|      - | 2141 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|      - | 2142 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|      - | 2143 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|      - | 2144 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|      - | 2145 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|      - | 2146 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|      - | 2147 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|      - | 2148 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|      - | 2149 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|      - | 2150 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|      - | 2151 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|      - | 2152 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|      - | 2153 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|      - | 2154 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|      - | 2155 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|      - | 2156 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|      - | 2157 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|      - | 2158 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|      - | 2159 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|      - | 2160 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|      - | 2161 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 2162 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|      - | 2163 | `	{"M_E",                  PH7_M_E_Const          },` |
|      - | 2164 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|      - | 2165 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|      - | 2166 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|      - | 2167 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|      - | 2168 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|      - | 2169 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|      - | 2170 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|      - | 2171 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|      - | 2172 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|      - | 2173 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|      - | 2174 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|      - | 2175 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|      - | 2176 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|      - | 2177 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|      - | 2178 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|      - | 2179 | `	{"NAN",                  PH7_NAN_Const          },` |
|      - | 2180 | `	{"INF",                  PH7_INF_Const          },` |
|      - | 2181 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 2182 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|      - | 2183 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|      - | 2184 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|      - | 2185 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|      - | 2186 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|      - | 2187 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|      - | 2188 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|      - | 2189 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|      - | 2190 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|      - | 2191 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|      - | 2192 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|      - | 2193 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|      - | 2194 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|      - | 2195 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|      - | 2196 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|      - | 2197 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|      - | 2198 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|      - | 2199 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|      - | 2200 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|      - | 2201 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|      - | 2202 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|      - | 2203 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|      - | 2204 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|      - | 2205 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|      - | 2206 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|      - | 2207 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|      - | 2208 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|      - | 2209 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|      - | 2210 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|      - | 2211 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|      - | 2212 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|      - | 2213 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|      - | 2214 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|      - | 2215 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|      - | 2216 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|      - | 2217 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|      - | 2218 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|      - | 2219 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|      - | 2220 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|      - | 2221 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|      - | 2222 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|      - | 2223 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|      - | 2224 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|      - | 2225 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|      - | 2226 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|      - | 2227 | `	{"ASSERT_ACTIVE",        PH7_ASSERT_ACTIVE_Const     },` |
|      - | 2228 | `	{"ASSERT_CALLBACK",      PH7_ASSERT_CALLBACK_Const   },` |
|      - | 2229 | `	{"ASSERT_BAIL",          PH7_ASSERT_BAIL_Const       },` |
|      - | 2230 | `	{"ASSERT_WARNING",       PH7_ASSERT_WARNING_Const    },` |
|      - | 2231 | `	{"ASSERT_EXCEPTION",     PH7_ASSERT_EXCEPTION_Const  },` |
|      - | 2232 | `	/* ASSERT_QUIET_EVAL was REMOVED in php 8.0: referencing it is an Error there */` |
|      - | 2233 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|      - | 2234 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|      - | 2235 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|      - | 2236 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|      - | 2237 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|      - | 2238 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|      - | 2239 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|      - | 2240 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|      - | 2241 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|      - | 2242 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|      - | 2243 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|      - | 2244 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|      - | 2245 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|      - | 2246 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|      - | 2247 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|      - | 2248 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|      - | 2249 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|      - | 2250 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|      - | 2251 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|      - | 2252 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|      - | 2253 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|      - | 2254 | `	{"STDIN",                PH7_STDIN_Const        },` |
|      - | 2255 | `	{"stdin",                PH7_STDIN_Const        },` |
|      - | 2256 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|      - | 2257 | `	{"stdout",               PH7_STDOUT_Const       },` |
|      - | 2258 | `	{"STDERR",               PH7_STDERR_Const       },` |
|      - | 2259 | `	{"stderr",               PH7_STDERR_Const       },` |
|      - | 2260 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|      - | 2261 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|      - | 2262 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|      - | 2263 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|      - | 2264 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|      - | 2265 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|      - | 2266 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|      - | 2267 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|      - | 2268 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|      - | 2269 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2270 | `	{"XML_ERROR_NONE",       PH7_XML_ERROR_NONE_Const},` |
|      - | 2271 | `	{"XML_ERROR_NO_MEMORY",  PH7_XML_ERROR_NO_MEMORY_Const},` |
|      - | 2272 | `	{"XML_ERROR_SYNTAX",     PH7_XML_ERROR_SYNTAX_Const},` |
|      - | 2273 | `	{"XML_ERROR_NO_ELEMENTS",PH7_XML_ERROR_NO_ELEMENTS_Const},` |
|      - | 2274 | `	{"XML_ERROR_INVALID_TOKEN", PH7_XML_ERROR_INVALID_TOKEN_Const},` |
|      - | 2275 | `	{"XML_ERROR_UNCLOSED_TOKEN",PH7_XML_ERROR_UNCLOSED_TOKEN_Const},` |
|      - | 2276 | `	{"XML_ERROR_PARTIAL_CHAR",  PH7_XML_ERROR_PARTIAL_CHAR_Const},` |
|      - | 2277 | `	{"XML_ERROR_TAG_MISMATCH",  PH7_XML_ERROR_TAG_MISMATCH_Const},` |
|      - | 2278 | `	{"XML_ERROR_DUPLICATE_ATTRIBUTE",   PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const},` |
|      - | 2279 | `	{"XML_ERROR_JUNK_AFTER_DOC_ELEMENT",PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const},` |
|      - | 2280 | `	{"XML_ERROR_PARAM_ENTITY_REF",      PH7_XML_ERROR_PARAM_ENTITY_REF_Const},` |
|      - | 2281 | `	{"XML_ERROR_UNDEFINED_ENTITY",      PH7_XML_ERROR_UNDEFINED_ENTITY_Const},` |
|      - | 2282 | `	{"XML_ERROR_RECURSIVE_ENTITY_REF",  PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const},` |
|      - | 2283 | `	{"XML_ERROR_ASYNC_ENTITY",          PH7_XML_ERROR_ASYNC_ENTITY_Const},` |
|      - | 2284 | `	{"XML_ERROR_BAD_CHAR_REF",          PH7_XML_ERROR_BAD_CHAR_REF_Const},` |
|      - | 2285 | `	{"XML_ERROR_BINARY_ENTITY_REF",     PH7_XML_ERROR_BINARY_ENTITY_REF_Const},` |
|      - | 2286 | `	{"XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF", PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const},` |
|      - | 2287 | `	{"XML_ERROR_MISPLACED_XML_PI",     PH7_XML_ERROR_MISPLACED_XML_PI_Const},` |
|      - | 2288 | `	{"XML_ERROR_UNKNOWN_ENCODING",     PH7_XML_ERROR_UNKNOWN_ENCODING_Const},` |
|      - | 2289 | `	{"XML_ERROR_INCORRECT_ENCODING",   PH7_XML_ERROR_INCORRECT_ENCODING_Const},` |
|      - | 2290 | `	{"XML_ERROR_UNCLOSED_CDATA_SECTION",  PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const},` |
|      - | 2291 | `	{"XML_ERROR_EXTERNAL_ENTITY_HANDLING",PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const},` |
|      - | 2292 | `	{"XML_OPTION_CASE_FOLDING",           PH7_XML_OPTION_CASE_FOLDING_Const},` |
|      - | 2293 | `	{"XML_OPTION_TARGET_ENCODING",        PH7_XML_OPTION_TARGET_ENCODING_Const},` |
|      - | 2294 | `	{"XML_OPTION_SKIP_TAGSTART",          PH7_XML_OPTION_SKIP_TAGSTART_Const},` |
|      - | 2295 | `	{"XML_OPTION_SKIP_WHITE",             PH7_XML_OPTION_SKIP_WHITE_Const},` |
|      - | 2296 | `	{"XML_SAX_IMPL",           PH7_XML_SAX_IMP_Const},` |
|      - | 2297 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2298 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|      - | 2299 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|      - | 2300 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|      - | 2301 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|      - | 2302 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|      - | 2303 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|      - | 2304 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|      - | 2305 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|      - | 2306 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|      - | 2307 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|      - | 2308 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|      - | 2309 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|      - | 2310 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|      - | 2311 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|      - | 2312 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|      - | 2313 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|      - | 2314 | `	{"JSON_ERROR_NON_BACKED_ENUM", PH7_JSON_ERROR_NON_BACKED_ENUM_Const},` |
|      - | 2315 | ``	/* `self`, `parent` and `static` are KEYWORDS in php, not constants: using one as a bare`` |
|      - | 2316 | ``	 * word is an "Undefined constant" Error (or a parse error for `static`). PH7 registered`` |
|      - | 2317 | `	 * them as constants that quietly expanded to the class name / NULL, so a typo'd bare` |
|      - | 2318 | ``	 * word silently produced a value. The `self::`/`parent::`/`static::` forms are handled`` |
|      - | 2319 | ``	 * by the `::` compile path and do not go through the constant table. */`` |
|      - | 2320 | `	{"__CLASS__",            PH7_class_magic_Const  }` |
|      - | 2321 | `};` |
|      - | 2322 | `/*` |
|      - | 2323 | ` * Register the built-in constants defined above.` |
|      - | 2324 | ` */` |
|   3338 | 2325 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|      5 | 2326 | `{` |
|      - | 2327 | `	sxu32 n;` |
|      - | 2328 | `	/*` |
|      - | 2329 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|      - | 2330 | `	 * that trigger the constant invocation as their private data.` |
|      - | 2331 | `	 */` |
| 877899 | 2332 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 874561 | 2333 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
| 437283 | 2334 | `	}` |
|   3343 | 2335 | `}` |
