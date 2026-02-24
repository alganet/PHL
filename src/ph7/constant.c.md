# src/ph7/constant.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1007/1012 lines (99.51%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/* This file implement built-in constants for the PH7 engine. */` |
|      - |    8 | `/*` |
|      - |    9 | ` * PH7_VERSION` |
|      - |   10 | ` * __PH7__` |
|      - |   11 | ` *   Expand the current version of the PH7 engine.` |
|      - |   12 | ` */` |
|      8 |   13 | `static void PH7_VER_Const(ph7_value *pVal,void *pUnused)` |
|      1 |   14 |  |
|      4 |   15 | `	SXUNUSED(pUnused);` |
|      9 |   16 | `	ph7_value_string(pVal,ph7_lib_signature(),-1/*Compute length automatically*/);` |
|      9 |   17 |  |
|      - |   18 | `#ifdef __WINNT__` |
|      - |   19 | `#include <Windows.h>` |
|      - |   20 | `#elif defined(__UNIXES__)` |
|      - |   21 | `#include <sys/utsname.h>` |
|      - |   22 | `#endif` |
|      - |   23 | `/*` |
|      - |   24 | ` * PHP_OS` |
|      - |   25 | ` *  Expand the name of the host Operating System.` |
|      - |   26 | ` */` |
|   1294 |   27 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|      2 |   28 |  |
|      - |   29 | `#if defined(__WINNT__)` |
|      2 |   30 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|      - |   31 | `#elif defined(__UNIXES__)` |
|      - |   32 | `	struct utsname sInfo;` |
|   1294 |   33 | `	if( uname(&sInfo) != 0 ){` |
|    ! 0 |   34 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 |   35 | `	}else{` |
|   1294 |   36 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|      - |   37 | `	}` |
|      - |   38 | `#else` |
|      - |   39 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|      - |   40 | `#endif` |
|    647 |   41 | `	SXUNUSED(pUnused);` |
|   1296 |   42 |  |
|      - |   43 | `/*` |
|      - |   44 | ` * PHP_EOL` |
|      - |   45 | ` *  Expand the correct 'End Of Line' symbol for this platform.` |
|      - |   46 | ` */` |
|    488 |   47 | `static void PH7_EOL_Const(ph7_value *pVal,void *pUnused)` |
|      2 |   48 |  |
|    244 |   49 | `	SXUNUSED(pUnused);` |
|      - |   50 | `#ifdef __WINNT__` |
|      2 |   51 | `	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);` |
|      - |   52 | `#else` |
|    488 |   53 | `	ph7_value_string(pVal,"\n",(int)sizeof(char));` |
|      - |   54 | `#endif` |
|    490 |   55 |  |
|      - |   56 | `/*` |
|      - |   57 | ` * PHP_INT_MAX` |
|      - |   58 | ` * Expand the largest integer supported.` |
|      - |   59 | ` * Note that PH7 deals with 64-bit integer for all platforms.` |
|      - |   60 | ` */` |
|      2 |   61 | `static void PH7_INTMAX_Const(ph7_value *pVal,void *pUnused)` |
|      1 |   62 |  |
|      1 |   63 | `	SXUNUSED(pUnused);` |
|      3 |   64 | `	ph7_value_int64(pVal,SXI64_HIGH);` |
|      3 |   65 |  |
|      - |   66 | `/*` |
|      - |   67 | ` * PHP_INT_SIZE` |
|      - |   68 | ` * Expand the size in bytes of a 64-bit integer.` |
|      - |   69 | ` */` |
|      2 |   70 | `static void PH7_INTSIZE_Const(ph7_value *pVal,void *pUnused)` |
|      1 |   71 |  |
|      1 |   72 | `	SXUNUSED(pUnused);` |
|      3 |   73 | `	ph7_value_int64(pVal,sizeof(sxi64));` |
|      3 |   74 |  |
|      - |   75 | `/*` |
|      - |   76 | ` * DIRECTORY_SEPARATOR.` |
|      - |   77 | ` * Expand the directory separator character.` |
|      - |   78 | ` */` |
|    156 |   79 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|      2 |   80 |  |
|     78 |   81 | `	SXUNUSED(pUnused);` |
|      - |   82 | `#ifdef __WINNT__` |
|      2 |   83 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|      - |   84 | `#else` |
|    156 |   85 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|      - |   86 | `#endif` |
|    158 |   87 |  |
|      - |   88 | `/*` |
|      - |   89 | ` * PATH_SEPARATOR.` |
|      - |   90 | ` * Expand the path separator character.` |
|      - |   91 | ` */` |
|      2 |   92 | `static void PH7_PATHSEP_Const(ph7_value *pVal,void *pUnused)` |
|      1 |   93 |  |
|      1 |   94 | `	SXUNUSED(pUnused);` |
|      - |   95 | `#ifdef __WINNT__` |
|      1 |   96 | `	ph7_value_string(pVal,";",(int)sizeof(char));` |
|      - |   97 | `#else` |
|      2 |   98 | `	ph7_value_string(pVal,":",(int)sizeof(char));` |
|      - |   99 | `#endif` |
|      3 |  100 |  |
|      - |  101 |  |
|      - |  102 | `#if defined(PH7_ENABLE_MATH_FUNC)` |
|      - |  103 | `/*` |
|      - |  104 | ` * NAN constant: floating-point Not-A-Number` |
|      - |  105 | ` */` |
|     36 |  106 | `static void PH7_NAN_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  107 |  |
|     18 |  108 | `	SXUNUSED(pUnused);` |
|      - |  109 | `	/* avoid the NAN macro (see https://github.com/ph7/phl/issues/...) */` |
|     37 |  110 | `	ph7_value_double(pVal, PH7_NAN_VALUE());` |
|     37 |  111 |  |
|      - |  112 |  |
|      - |  113 | `/*` |
|      - |  114 | ` * INF constant: positive infinity` |
|      - |  115 | ` */` |
|      4 |  116 | `static void PH7_INF_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  117 |  |
|      2 |  118 | `	SXUNUSED(pUnused);` |
|      - |  119 | `	/* similarly avoid the INFINITY macro */` |
|      5 |  120 | `	ph7_value_double(pVal, PH7_INF_VALUE());` |
|      5 |  121 |  |
|      - |  122 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - |  123 |  |
|      - |  124 | `#ifndef __WINNT__` |
|      - |  125 | `#include <time.h>` |
|      - |  126 | `#endif` |
|      - |  127 | `/*` |
|      - |  128 | ` * __TIME__` |
|      - |  129 | ` *  Expand the current time (GMT).` |
|      - |  130 | ` */` |
|      2 |  131 | `static void PH7_TIME_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  132 |  |
|      - |  133 | `	Sytm sTm;` |
|      - |  134 | `#ifdef __WINNT__` |
|      - |  135 | `	SYSTEMTIME sOS;` |
|      1 |  136 | `	GetSystemTime(&sOS);` |
|      1 |  137 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - |  138 | `#else` |
|      - |  139 | `	struct tm *pTm;` |
|      - |  140 | `	time_t t;` |
|      2 |  141 | `	time(&t);` |
|      2 |  142 | `	pTm = gmtime(&t);` |
|      2 |  143 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - |  144 | `#endif` |
|      1 |  145 | `	SXUNUSED(pUnused); /* cc warning */` |
|      - |  146 | `	/* Expand */` |
|      3 |  147 | `	ph7_value_string_format(pVal,"%02d:%02d:%02d",sTm.tm_hour,sTm.tm_min,sTm.tm_sec);` |
|      3 |  148 |  |
|      - |  149 | `/*` |
|      - |  150 | ` * __DATE__` |
|      - |  151 | ` *  Expand the current date in the ISO-8601 format.` |
|      - |  152 | ` */` |
|      2 |  153 | `static void PH7_DATE_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  154 |  |
|      - |  155 | `	Sytm sTm;` |
|      - |  156 | `#ifdef __WINNT__` |
|      - |  157 | `	SYSTEMTIME sOS;` |
|      1 |  158 | `	GetSystemTime(&sOS);` |
|      1 |  159 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - |  160 | `#else` |
|      - |  161 | `	struct tm *pTm;` |
|      - |  162 | `	time_t t;` |
|      2 |  163 | `	time(&t);` |
|      2 |  164 | `	pTm = gmtime(&t);` |
|      2 |  165 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - |  166 | `#endif` |
|      1 |  167 | `	SXUNUSED(pUnused); /* cc warning */` |
|      - |  168 | `	/* Expand */` |
|      3 |  169 | `	ph7_value_string_format(pVal,"%04d-%02d-%02d",sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|      3 |  170 |  |
|      - |  171 | `/*` |
|      - |  172 | ` * __FILE__` |
|      - |  173 | ` *  Path of the processed script.` |
|      - |  174 | ` */` |
|     56 |  175 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  176 |  |
|     58 |  177 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  178 | `	SyString *pFile;` |
|      - |  179 | `	/* Peek the top entry */` |
|     58 |  180 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     58 |  181 | `	if( pFile == 0 ){` |
|      - |  182 | `		/* Expand the magic word: ":MEMORY:" */` |
|      3 |  183 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|      2 |  184 | `	}else{` |
|     56 |  185 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|      - |  186 | `	}` |
|     58 |  187 |  |
|      - |  188 | `/*` |
|      - |  189 | ` * __DIR__` |
|      - |  190 | ` *  Directory holding the processed script.` |
|      - |  191 | ` */` |
|     20 |  192 | `static void PH7_DIR_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  193 |  |
|     22 |  194 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  195 | `	SyString *pFile;` |
|      - |  196 | `	/* Peek the top entry */` |
|     22 |  197 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     22 |  198 | `	if( pFile == 0 ){` |
|      - |  199 | `		/* Expand the magic word: ":MEMORY:" */` |
|      3 |  200 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|      2 |  201 | `	}else{` |
|     19 |  202 | `		if( pFile->nByte > 0 ){` |
|      - |  203 | `			const char *zDir;` |
|      - |  204 | `			int nLen;` |
|     19 |  205 | `			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|     19 |  206 | `			ph7_value_string(pVal,zDir,nLen);` |
|     10 |  207 | `		}else{` |
|      - |  208 | `			/* Expand '.' as the current directory*/` |
|    ! 0 |  209 | `			ph7_value_string(pVal,".",(int)sizeof(char));` |
|      - |  210 | `		}` |
|      - |  211 | `	}` |
|     22 |  212 |  |
|      - |  213 | `/*` |
|      - |  214 | ` * PHP_SHLIB_SUFFIX` |
|      - |  215 | ` *  Expand shared library suffix.` |
|      - |  216 | ` */` |
|      2 |  217 | `static void PH7_PHP_SHLIB_SUFFIX_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 |  218 |  |
|      - |  219 | `#ifdef __WINNT__` |
|    ! 0 |  220 | `	ph7_value_string(pVal,"dll",(int)sizeof("dll")-1);` |
|      - |  221 | `#else` |
|      2 |  222 | `	ph7_value_string(pVal,"so",(int)sizeof("so")-1);` |
|      - |  223 | `#endif` |
|      1 |  224 | `	SXUNUSED(pUserData); /* cc warning */` |
|      2 |  225 |  |
|      - |  226 | `/*` |
|      - |  227 | ` * E_ERROR` |
|      - |  228 | ` *  Expands 1` |
|      - |  229 | ` */` |
|      2 |  230 | `static void PH7_E_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  231 |  |
|      3 |  232 | `	ph7_value_int(pVal,1);` |
|      1 |  233 | `	SXUNUSED(pUserData);` |
|      3 |  234 |  |
|      - |  235 | `/*` |
|      - |  236 | ` * E_WARNING` |
|      - |  237 | ` *  Expands 2` |
|      - |  238 | ` */` |
|      2 |  239 | `static void PH7_E_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  240 |  |
|      3 |  241 | `	ph7_value_int(pVal,2);` |
|      1 |  242 | `	SXUNUSED(pUserData);` |
|      3 |  243 |  |
|      - |  244 | `/*` |
|      - |  245 | ` * E_PARSE` |
|      - |  246 | ` *  Expands 4` |
|      - |  247 | ` */` |
|      2 |  248 | `static void PH7_E_PARSE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  249 |  |
|      3 |  250 | `	ph7_value_int(pVal,4);` |
|      1 |  251 | `	SXUNUSED(pUserData);` |
|      3 |  252 |  |
|      - |  253 | `/*` |
|      - |  254 | ` * E_NOTICE` |
|      - |  255 | ` * Expands 8` |
|      - |  256 | ` */` |
|      2 |  257 | `static void PH7_E_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  258 |  |
|      3 |  259 | `	ph7_value_int(pVal,8);` |
|      1 |  260 | `	SXUNUSED(pUserData);` |
|      3 |  261 |  |
|      - |  262 | `/*` |
|      - |  263 | ` * E_CORE_ERROR` |
|      - |  264 | ` * Expands 16` |
|      - |  265 | ` */` |
|      2 |  266 | `static void PH7_E_CORE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  267 |  |
|      3 |  268 | `	ph7_value_int(pVal,16);` |
|      1 |  269 | `	SXUNUSED(pUserData);` |
|      3 |  270 |  |
|      - |  271 | `/*` |
|      - |  272 | ` * E_CORE_WARNING` |
|      - |  273 | ` * Expands 32` |
|      - |  274 | ` */` |
|      2 |  275 | `static void PH7_E_CORE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  276 |  |
|      3 |  277 | `	ph7_value_int(pVal,32);` |
|      1 |  278 | `	SXUNUSED(pUserData);` |
|      3 |  279 |  |
|      - |  280 | `/*` |
|      - |  281 | ` * E_COMPILE_ERROR` |
|      - |  282 | ` * Expands 64` |
|      - |  283 | ` */` |
|      2 |  284 | `static void PH7_E_COMPILE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  285 |  |
|      3 |  286 | `	ph7_value_int(pVal,64);` |
|      1 |  287 | `	SXUNUSED(pUserData);` |
|      3 |  288 |  |
|      - |  289 | `/*` |
|      - |  290 | ` * E_COMPILE_WARNING` |
|      - |  291 | ` * Expands 128` |
|      - |  292 | ` */` |
|      2 |  293 | `static void PH7_E_COMPILE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  294 |  |
|      3 |  295 | `	ph7_value_int(pVal,128);` |
|      1 |  296 | `	SXUNUSED(pUserData);` |
|      3 |  297 |  |
|      - |  298 | `/*` |
|      - |  299 | ` * E_USER_ERROR` |
|      - |  300 | ` * Expands 256` |
|      - |  301 | ` */` |
|      4 |  302 | `static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  303 |  |
|      6 |  304 | `	ph7_value_int(pVal,256);` |
|      2 |  305 | `	SXUNUSED(pUserData);` |
|      6 |  306 |  |
|      - |  307 | `/*` |
|      - |  308 | ` * E_USER_WARNING` |
|      - |  309 | ` * Expands 512` |
|      - |  310 | ` */` |
|      4 |  311 | `static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  312 |  |
|      6 |  313 | `	ph7_value_int(pVal,512);` |
|      2 |  314 | `	SXUNUSED(pUserData);` |
|      6 |  315 |  |
|      - |  316 | `/*` |
|      - |  317 | ` * E_USER_NOTICE` |
|      - |  318 | ` * Expands 1024` |
|      - |  319 | ` */` |
|      6 |  320 | `static void PH7_E_USER_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  321 |  |
|      8 |  322 | `	ph7_value_int(pVal,1024);` |
|      3 |  323 | `	SXUNUSED(pUserData);` |
|      8 |  324 |  |
|      - |  325 | `/*` |
|      - |  326 | ` * E_STRICT` |
|      - |  327 | ` * Expands 2048` |
|      - |  328 | ` */` |
|      2 |  329 | `static void PH7_E_STRICT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  330 |  |
|      3 |  331 | `	ph7_value_int(pVal,2048);` |
|      1 |  332 | `	SXUNUSED(pUserData);` |
|      3 |  333 |  |
|      - |  334 | `/*` |
|      - |  335 | ` * E_RECOVERABLE_ERROR` |
|      - |  336 | ` * Expands 4096` |
|      - |  337 | ` */` |
|      2 |  338 | `static void PH7_E_RECOVERABLE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  339 |  |
|      3 |  340 | `	ph7_value_int(pVal,4096);` |
|      1 |  341 | `	SXUNUSED(pUserData);` |
|      3 |  342 |  |
|      - |  343 | `/*` |
|      - |  344 | ` * E_DEPRECATED` |
|      - |  345 | ` * Expands 8192` |
|      - |  346 | ` */` |
|      4 |  347 | `static void PH7_E_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  348 |  |
|      5 |  349 | `	ph7_value_int(pVal,8192);` |
|      2 |  350 | `	SXUNUSED(pUserData);` |
|      5 |  351 |  |
|      - |  352 | `/*` |
|      - |  353 | ` * E_USER_DEPRECATED` |
|      - |  354 | ` *   Expands 16384.` |
|      - |  355 | ` */` |
|      2 |  356 | `static void PH7_E_USER_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  357 |  |
|      3 |  358 | `	ph7_value_int(pVal,16384);` |
|      1 |  359 | `	SXUNUSED(pUserData);` |
|      3 |  360 |  |
|      - |  361 | `/*` |
|      - |  362 | ` * E_ALL` |
|      - |  363 | ` *  Expands 32767` |
|      - |  364 | ` */` |
|      2 |  365 | `static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  366 |  |
|      3 |  367 | `	ph7_value_int(pVal,32767);` |
|      1 |  368 | `	SXUNUSED(pUserData);` |
|      3 |  369 |  |
|      - |  370 | `/*` |
|      - |  371 | ` * CASE_LOWER` |
|      - |  372 | ` *  Expands 0.` |
|      - |  373 | ` */` |
|      2 |  374 | `static void PH7_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  375 |  |
|      3 |  376 | `	ph7_value_int(pVal,0);` |
|      1 |  377 | `	SXUNUSED(pUserData);` |
|      3 |  378 |  |
|      - |  379 | `/*` |
|      - |  380 | ` * CASE_UPPER` |
|      - |  381 | ` *  Expands 1.` |
|      - |  382 | ` */` |
|      2 |  383 | `static void PH7_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  384 |  |
|      3 |  385 | `	ph7_value_int(pVal,1);` |
|      1 |  386 | `	SXUNUSED(pUserData);` |
|      3 |  387 |  |
|      - |  388 | `/*` |
|      - |  389 | ` * STR_PAD_LEFT` |
|      - |  390 | ` *  Expands 0.` |
|      - |  391 | ` */` |
|      4 |  392 | `static void PH7_STR_PAD_LEFT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  393 |  |
|      5 |  394 | `	ph7_value_int(pVal,0);` |
|      2 |  395 | `	SXUNUSED(pUserData);` |
|      5 |  396 |  |
|      - |  397 | `/*` |
|      - |  398 | ` * STR_PAD_RIGHT` |
|      - |  399 | ` *  Expands 1.` |
|      - |  400 | ` */` |
|      4 |  401 | `static void PH7_STR_PAD_RIGHT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  402 |  |
|      5 |  403 | `	ph7_value_int(pVal,1);` |
|      2 |  404 | `	SXUNUSED(pUserData);` |
|      5 |  405 |  |
|      - |  406 | `/*` |
|      - |  407 | ` * STR_PAD_BOTH` |
|      - |  408 | ` *  Expands 2.` |
|      - |  409 | ` */` |
|      2 |  410 | `static void PH7_STR_PAD_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  411 |  |
|      3 |  412 | `	ph7_value_int(pVal,2);` |
|      1 |  413 | `	SXUNUSED(pUserData);` |
|      3 |  414 |  |
|      - |  415 | `/*` |
|      - |  416 | ` * COUNT_NORMAL` |
|      - |  417 | ` *  Expands 0` |
|      - |  418 | ` */` |
|      2 |  419 | `static void PH7_COUNT_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  420 |  |
|      3 |  421 | `	ph7_value_int(pVal,0);` |
|      1 |  422 | `	SXUNUSED(pUserData);` |
|      3 |  423 |  |
|      - |  424 | `/*` |
|      - |  425 | ` * COUNT_RECURSIVE` |
|      - |  426 | ` *  Expands 1.` |
|      - |  427 | ` */` |
|     22 |  428 | `static void PH7_COUNT_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  429 |  |
|     23 |  430 | `	ph7_value_int(pVal,1);` |
|     11 |  431 | `	SXUNUSED(pUserData);` |
|     23 |  432 |  |
|      - |  433 | `/*` |
|      - |  434 | ` * SORT_ASC` |
|      - |  435 | ` *  Expands 1.` |
|      - |  436 | ` */` |
|      2 |  437 | `static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  438 |  |
|      3 |  439 | `	ph7_value_int(pVal,1);` |
|      1 |  440 | `	SXUNUSED(pUserData);` |
|      3 |  441 |  |
|      - |  442 | `/*` |
|      - |  443 | ` * SORT_DESC` |
|      - |  444 | ` *  Expands 2.` |
|      - |  445 | ` */` |
|      2 |  446 | `static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  447 |  |
|      3 |  448 | `	ph7_value_int(pVal,2);` |
|      1 |  449 | `	SXUNUSED(pUserData);` |
|      3 |  450 |  |
|      - |  451 | `/*` |
|      - |  452 | ` * SORT_REGULAR` |
|      - |  453 | ` *  Expands 3.` |
|      - |  454 | ` */` |
|      2 |  455 | `static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  456 |  |
|      3 |  457 | `	ph7_value_int(pVal,3);` |
|      1 |  458 | `	SXUNUSED(pUserData);` |
|      3 |  459 |  |
|      - |  460 | `/*` |
|      - |  461 | ` * SORT_NUMERIC` |
|      - |  462 | ` *  Expands 4.` |
|      - |  463 | ` */` |
|      2 |  464 | `static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  465 |  |
|      3 |  466 | `	ph7_value_int(pVal,4);` |
|      1 |  467 | `	SXUNUSED(pUserData);` |
|      3 |  468 |  |
|      - |  469 | `/*` |
|      - |  470 | ` * SORT_STRING` |
|      - |  471 | ` *  Expands 5.` |
|      - |  472 | ` */` |
|      4 |  473 | `static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  474 |  |
|      5 |  475 | `	ph7_value_int(pVal,5);` |
|      2 |  476 | `	SXUNUSED(pUserData);` |
|      5 |  477 |  |
|      - |  478 | `/*` |
|      - |  479 | ` * PHP_ROUND_HALF_UP` |
|      - |  480 | ` *  Expands 1.` |
|      - |  481 | ` */` |
|      2 |  482 | `static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  483 |  |
|      3 |  484 | `	ph7_value_int(pVal,1);` |
|      1 |  485 | `	SXUNUSED(pUserData);` |
|      3 |  486 |  |
|      - |  487 | `/*` |
|      - |  488 | ` * SPHP_ROUND_HALF_DOWN` |
|      - |  489 | ` *  Expands 2.` |
|      - |  490 | ` */` |
|      2 |  491 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  492 |  |
|      3 |  493 | `	ph7_value_int(pVal,2);` |
|      1 |  494 | `	SXUNUSED(pUserData);` |
|      3 |  495 |  |
|      - |  496 | `/*` |
|      - |  497 | ` * PHP_ROUND_HALF_EVEN` |
|      - |  498 | ` *  Expands 3.` |
|      - |  499 | ` */` |
|      2 |  500 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  501 |  |
|      3 |  502 | `	ph7_value_int(pVal,3);` |
|      1 |  503 | `	SXUNUSED(pUserData);` |
|      3 |  504 |  |
|      - |  505 | `/*` |
|      - |  506 | ` * PHP_ROUND_HALF_ODD` |
|      - |  507 | ` *  Expands 4.` |
|      - |  508 | ` */` |
|      2 |  509 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  510 |  |
|      3 |  511 | `	ph7_value_int(pVal,4);` |
|      1 |  512 | `	SXUNUSED(pUserData);` |
|      3 |  513 |  |
|      - |  514 | `/*` |
|      - |  515 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|      - |  516 | ` *  Expand 0x01` |
|      - |  517 | ` * NOTE:` |
|      - |  518 | ` *  The expanded value must be a power of two.` |
|      - |  519 | ` */` |
|      2 |  520 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  521 |  |
|      3 |  522 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      1 |  523 | `	SXUNUSED(pUserData);` |
|      3 |  524 |  |
|      - |  525 | `/*` |
|      - |  526 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|      - |  527 | ` *  Expand 0x02` |
|      - |  528 | ` * NOTE:` |
|      - |  529 | ` *  The expanded value must be a power of two.` |
|      - |  530 | ` */` |
|      2 |  531 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  532 |  |
|      3 |  533 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      1 |  534 | `	SXUNUSED(pUserData);` |
|      3 |  535 |  |
|      - |  536 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  537 | `/*` |
|      - |  538 | ` * M_PI` |
|      - |  539 | ` *  Expand the value of pi.` |
|      - |  540 | ` */` |
|      2 |  541 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  542 |  |
|      1 |  543 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  544 | `	ph7_value_double(pVal,PH7_PI);` |
|      3 |  545 |  |
|      - |  546 | `/*` |
|      - |  547 | ` * M_E` |
|      - |  548 | ` *  Expand 2.7182818284590452354` |
|      - |  549 | ` */` |
|      2 |  550 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  551 |  |
|      1 |  552 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  553 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|      3 |  554 |  |
|      - |  555 | `/*` |
|      - |  556 | ` * M_LOG2E` |
|      - |  557 | ` *  Expand 2.7182818284590452354` |
|      - |  558 | ` */` |
|      2 |  559 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  560 |  |
|      1 |  561 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  562 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      3 |  563 |  |
|      - |  564 | `/*` |
|      - |  565 | ` * M_LOG10E` |
|      - |  566 | ` *  Expand 0.4342944819032518276` |
|      - |  567 | ` */` |
|      2 |  568 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  569 |  |
|      1 |  570 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  571 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      3 |  572 |  |
|      - |  573 | `/*` |
|      - |  574 | ` * M_LN2` |
|      - |  575 | ` *  Expand 	0.69314718055994530942` |
|      - |  576 | ` */` |
|      2 |  577 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  578 |  |
|      1 |  579 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  580 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      3 |  581 |  |
|      - |  582 | `/*` |
|      - |  583 | ` * M_LN10` |
|      - |  584 | ` *  Expand 	2.30258509299404568402` |
|      - |  585 | ` */` |
|      2 |  586 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  587 |  |
|      1 |  588 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  589 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      3 |  590 |  |
|      - |  591 | `/*` |
|      - |  592 | ` * M_PI_2` |
|      - |  593 | ` *  Expand 	1.57079632679489661923` |
|      - |  594 | ` */` |
|      2 |  595 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  596 |  |
|      1 |  597 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  598 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      3 |  599 |  |
|      - |  600 | `/*` |
|      - |  601 | ` * M_PI_4` |
|      - |  602 | ` *  Expand 	0.78539816339744830962` |
|      - |  603 | ` */` |
|      2 |  604 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  605 |  |
|      1 |  606 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  607 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      3 |  608 |  |
|      - |  609 | `/*` |
|      - |  610 | ` * M_1_PI` |
|      - |  611 | ` *  Expand 	0.31830988618379067154` |
|      - |  612 | ` */` |
|      2 |  613 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  614 |  |
|      1 |  615 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  616 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      3 |  617 |  |
|      - |  618 | `/*` |
|      - |  619 | ` * M_2_PI` |
|      - |  620 | ` *  Expand 0.63661977236758134308` |
|      - |  621 | ` */` |
|      4 |  622 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  623 |  |
|      2 |  624 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  625 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      5 |  626 |  |
|      - |  627 | `/*` |
|      - |  628 | ` * M_SQRTPI` |
|      - |  629 | ` *  Expand 1.77245385090551602729` |
|      - |  630 | ` */` |
|      2 |  631 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  632 |  |
|      1 |  633 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  634 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      3 |  635 |  |
|      - |  636 | `/*` |
|      - |  637 | ` * M_2_SQRTPI` |
|      - |  638 | ` *  Expand 	1.12837916709551257390` |
|      - |  639 | ` */` |
|      2 |  640 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  641 |  |
|      1 |  642 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  643 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      3 |  644 |  |
|      - |  645 | `/*` |
|      - |  646 | ` * M_SQRT2` |
|      - |  647 | ` *  Expand 	1.41421356237309504880` |
|      - |  648 | ` */` |
|      2 |  649 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  650 |  |
|      1 |  651 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  652 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      3 |  653 |  |
|      - |  654 | `/*` |
|      - |  655 | ` * M_SQRT3` |
|      - |  656 | ` *  Expand 	1.73205080756887729352` |
|      - |  657 | ` */` |
|      2 |  658 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  659 |  |
|      1 |  660 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  661 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      3 |  662 |  |
|      - |  663 | `/*` |
|      - |  664 | ` * M_SQRT1_2` |
|      - |  665 | ` *  Expand 	0.70710678118654752440` |
|      - |  666 | ` */` |
|      2 |  667 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  668 |  |
|      1 |  669 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  670 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      3 |  671 |  |
|      - |  672 | `/*` |
|      - |  673 | ` * M_LNPI` |
|      - |  674 | ` *  Expand 	1.14472988584940017414` |
|      - |  675 | ` */` |
|      2 |  676 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  677 |  |
|      1 |  678 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  679 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      3 |  680 |  |
|      - |  681 | `/*` |
|      - |  682 | ` * M_EULER` |
|      - |  683 | ` *  Expand  0.57721566490153286061` |
|      - |  684 | ` */` |
|      2 |  685 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  686 |  |
|      1 |  687 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  688 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      3 |  689 |  |
|      - |  690 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|      - |  691 | `/*` |
|      - |  692 | ` * DATE_ATOM` |
|      - |  693 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|      - |  694 | ` */` |
|      2 |  695 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  696 |  |
|      1 |  697 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  698 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  699 |  |
|      - |  700 | `/*` |
|      - |  701 | ` * DATE_COOKIE` |
|      - |  702 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  703 | ` */` |
|      2 |  704 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  705 |  |
|      1 |  706 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  707 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  708 |  |
|      - |  709 | `/*` |
|      - |  710 | ` * DATE_ISO8601` |
|      - |  711 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|      - |  712 | ` */` |
|      2 |  713 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  714 |  |
|      1 |  715 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  716 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      3 |  717 |  |
|      - |  718 | `/*` |
|      - |  719 | ` * DATE_RFC822` |
|      - |  720 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|      - |  721 | ` */` |
|      2 |  722 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  723 |  |
|      1 |  724 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  725 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  726 |  |
|      - |  727 | `/*` |
|      - |  728 | ` * DATE_RFC850` |
|      - |  729 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  730 | ` */` |
|      2 |  731 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  732 |  |
|      1 |  733 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  734 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  735 |  |
|      - |  736 | `/*` |
|      - |  737 | ` * DATE_RFC1036` |
|      - |  738 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  739 | ` */` |
|      2 |  740 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  741 |  |
|      1 |  742 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  743 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  744 |  |
|      - |  745 | `/*` |
|      - |  746 | ` * DATE_RFC1123` |
|      - |  747 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  748 | ` */` |
|      2 |  749 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  750 |  |
|      1 |  751 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  752 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  753 |  |
|      - |  754 | `/*` |
|      - |  755 | ` * DATE_RFC2822` |
|      - |  756 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  757 | ` */` |
|      2 |  758 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  759 |  |
|      1 |  760 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  761 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  762 |  |
|      - |  763 | `/*` |
|      - |  764 | ` * DATE_RSS` |
|      - |  765 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  766 | ` */` |
|      2 |  767 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  768 |  |
|      1 |  769 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  770 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  771 |  |
|      - |  772 | `/*` |
|      - |  773 | ` * DATE_W3C` |
|      - |  774 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|      - |  775 | ` */` |
|      2 |  776 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  777 |  |
|      1 |  778 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  779 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  780 |  |
|      - |  781 | `/*` |
|      - |  782 | ` * ENT_COMPAT` |
|      - |  783 | ` *  Expand 0x01 (Must be a power of two)` |
|      - |  784 | ` */` |
|      2 |  785 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  786 |  |
|      1 |  787 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  788 | `	ph7_value_int(pVal,0x01);` |
|      3 |  789 |  |
|      - |  790 | `/*` |
|      - |  791 | ` * ENT_QUOTES` |
|      - |  792 | ` *  Expand 0x02 (Must be a power of two)` |
|      - |  793 | ` */` |
|     16 |  794 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  795 |  |
|      8 |  796 | `	SXUNUSED(pUserData); /* cc warning */` |
|     17 |  797 | `	ph7_value_int(pVal,0x02);` |
|     17 |  798 |  |
|      - |  799 | `/*` |
|      - |  800 | ` * ENT_NOQUOTES` |
|      - |  801 | ` *  Expand 0x04 (Must be a power of two)` |
|      - |  802 | ` */` |
|     12 |  803 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  804 |  |
|      6 |  805 | `	SXUNUSED(pUserData); /* cc warning */` |
|     13 |  806 | `	ph7_value_int(pVal,0x04);` |
|     13 |  807 |  |
|      - |  808 | `/*` |
|      - |  809 | ` * ENT_IGNORE` |
|      - |  810 | ` *  Expand 0x08 (Must be a power of two)` |
|      - |  811 | ` */` |
|      2 |  812 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  813 |  |
|      1 |  814 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  815 | `	ph7_value_int(pVal,0x08);` |
|      3 |  816 |  |
|      - |  817 | `/*` |
|      - |  818 | ` * ENT_SUBSTITUTE` |
|      - |  819 | ` *  Expand 0x10 (Must be a power of two)` |
|      - |  820 | ` */` |
|      2 |  821 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  822 |  |
|      1 |  823 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  824 | `	ph7_value_int(pVal,0x10);` |
|      3 |  825 |  |
|      - |  826 | `/*` |
|      - |  827 | ` * ENT_DISALLOWED` |
|      - |  828 | ` *  Expand 0x20 (Must be a power of two)` |
|      - |  829 | ` */` |
|      2 |  830 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  831 |  |
|      1 |  832 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  833 | `	ph7_value_int(pVal,0x20);` |
|      3 |  834 |  |
|      - |  835 | `/*` |
|      - |  836 | ` * ENT_HTML401` |
|      - |  837 | ` *  Expand 0x40 (Must be a power of two)` |
|      - |  838 | ` */` |
|      2 |  839 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  840 |  |
|      1 |  841 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  842 | `	ph7_value_int(pVal,0x40);` |
|      3 |  843 |  |
|      - |  844 | `/*` |
|      - |  845 | ` * ENT_XML1` |
|      - |  846 | ` *  Expand 0x80 (Must be a power of two)` |
|      - |  847 | ` */` |
|      2 |  848 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  849 |  |
|      1 |  850 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  851 | `	ph7_value_int(pVal,0x80);` |
|      3 |  852 |  |
|      - |  853 | `/*` |
|      - |  854 | ` * ENT_XHTML` |
|      - |  855 | ` *  Expand 0x100 (Must be a power of two)` |
|      - |  856 | ` */` |
|      2 |  857 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  858 |  |
|      1 |  859 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  860 | `	ph7_value_int(pVal,0x100);` |
|      3 |  861 |  |
|      - |  862 | `/*` |
|      - |  863 | ` * ENT_HTML5` |
|      - |  864 | ` *  Expand 0x200 (Must be a power of two)` |
|      - |  865 | ` */` |
|      2 |  866 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  867 |  |
|      1 |  868 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  869 | `	ph7_value_int(pVal,0x200);` |
|      3 |  870 |  |
|      - |  871 | `/*` |
|      - |  872 | ` * ISO-8859-1` |
|      - |  873 | ` * ISO_8859_1` |
|      - |  874 | ` *   Expand 1` |
|      - |  875 | ` */` |
|      2 |  876 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  877 |  |
|      1 |  878 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  879 | `	ph7_value_int(pVal,1);` |
|      3 |  880 |  |
|      - |  881 | `/*` |
|      - |  882 | ` * UTF-8` |
|      - |  883 | ` * UTF8` |
|      - |  884 | ` *  Expand 2` |
|      - |  885 | ` */` |
|      2 |  886 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  887 |  |
|      1 |  888 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  889 | `	ph7_value_int(pVal,1);` |
|      3 |  890 |  |
|      - |  891 | `/*` |
|      - |  892 | ` * HTML_ENTITIES` |
|      - |  893 | ` *  Expand 1` |
|      - |  894 | ` */` |
|      2 |  895 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  896 |  |
|      1 |  897 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  898 | `	ph7_value_int(pVal,1);` |
|      3 |  899 |  |
|      - |  900 | `/*` |
|      - |  901 | ` * HTML_SPECIALCHARS` |
|      - |  902 | ` *  Expand 2` |
|      - |  903 | ` */` |
|      2 |  904 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  905 |  |
|      1 |  906 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  907 | `	ph7_value_int(pVal,2);` |
|      3 |  908 |  |
|      - |  909 | `/*` |
|      - |  910 | ` * PHP_URL_SCHEME.` |
|      - |  911 | ` * Expand 1` |
|      - |  912 | ` */` |
|      2 |  913 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  914 |  |
|      1 |  915 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  916 | `	ph7_value_int(pVal,1);` |
|      3 |  917 |  |
|      - |  918 | `/*` |
|      - |  919 | ` * PHP_URL_HOST.` |
|      - |  920 | ` * Expand 2` |
|      - |  921 | ` */` |
|      2 |  922 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  923 |  |
|      1 |  924 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  925 | `	ph7_value_int(pVal,2);` |
|      3 |  926 |  |
|      - |  927 | `/*` |
|      - |  928 | ` * PHP_URL_PORT.` |
|      - |  929 | ` * Expand 3` |
|      - |  930 | ` */` |
|      2 |  931 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  932 |  |
|      1 |  933 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  934 | `	ph7_value_int(pVal,3);` |
|      3 |  935 |  |
|      - |  936 | `/*` |
|      - |  937 | ` * PHP_URL_USER.` |
|      - |  938 | ` * Expand 4` |
|      - |  939 | ` */` |
|      2 |  940 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  941 |  |
|      1 |  942 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  943 | `	ph7_value_int(pVal,4);` |
|      3 |  944 |  |
|      - |  945 | `/*` |
|      - |  946 | ` * PHP_URL_PASS.` |
|      - |  947 | ` * Expand 5` |
|      - |  948 | ` */` |
|      2 |  949 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  950 |  |
|      1 |  951 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  952 | `	ph7_value_int(pVal,5);` |
|      3 |  953 |  |
|      - |  954 | `/*` |
|      - |  955 | ` * PHP_URL_PATH.` |
|      - |  956 | ` * Expand 6` |
|      - |  957 | ` */` |
|      2 |  958 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  959 |  |
|      1 |  960 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  961 | `	ph7_value_int(pVal,6);` |
|      3 |  962 |  |
|      - |  963 | `/*` |
|      - |  964 | ` * PHP_URL_QUERY.` |
|      - |  965 | ` * Expand 7` |
|      - |  966 | ` */` |
|      2 |  967 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  968 |  |
|      1 |  969 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  970 | `	ph7_value_int(pVal,7);` |
|      3 |  971 |  |
|      - |  972 | `/*` |
|      - |  973 | ` * PHP_URL_FRAGMENT.` |
|      - |  974 | ` * Expand 8` |
|      - |  975 | ` */` |
|      2 |  976 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  977 |  |
|      1 |  978 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  979 | `	ph7_value_int(pVal,8);` |
|      3 |  980 |  |
|      - |  981 | `/*` |
|      - |  982 | ` * PHP_QUERY_RFC1738` |
|      - |  983 | ` * Expand 1` |
|      - |  984 | ` */` |
|      2 |  985 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  986 |  |
|      1 |  987 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  988 | `	ph7_value_int(pVal,1);` |
|      3 |  989 |  |
|      - |  990 | `/*` |
|      - |  991 | ` * PHP_QUERY_RFC3986` |
|      - |  992 | ` * Expand 1` |
|      - |  993 | ` */` |
|      2 |  994 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  995 |  |
|      1 |  996 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  997 | `	ph7_value_int(pVal,2);` |
|      3 |  998 |  |
|      - |  999 | `/*` |
|      - | 1000 | ` * FNM_NOESCAPE` |
|      - | 1001 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1002 | ` */` |
|      2 | 1003 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1004 |  |
|      1 | 1005 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1006 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1007 |  |
|      - | 1008 | `/*` |
|      - | 1009 | ` * FNM_PATHNAME` |
|      - | 1010 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1011 | ` */` |
|      2 | 1012 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1013 |  |
|      1 | 1014 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1015 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1016 |  |
|      - | 1017 | `/*` |
|      - | 1018 | ` * FNM_PERIOD` |
|      - | 1019 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1020 | ` */` |
|      6 | 1021 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1022 |  |
|      3 | 1023 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1024 | `	ph7_value_int(pVal,0x04);` |
|      7 | 1025 |  |
|      - | 1026 | `/*` |
|      - | 1027 | ` * FNM_CASEFOLD` |
|      - | 1028 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1029 | ` */` |
|      4 | 1030 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1031 |  |
|      2 | 1032 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1033 | `	ph7_value_int(pVal,0x08);` |
|      5 | 1034 |  |
|      - | 1035 | `/*` |
|      - | 1036 | ` * PATHINFO_DIRNAME` |
|      - | 1037 | ` *  Expand 1.` |
|      - | 1038 | ` */` |
|      4 | 1039 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1040 |  |
|      2 | 1041 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1042 | `	ph7_value_int(pVal,1);` |
|      5 | 1043 |  |
|      - | 1044 | `/*` |
|      - | 1045 | ` * PATHINFO_BASENAME` |
|      - | 1046 | ` *  Expand 2.` |
|      - | 1047 | ` */` |
|      4 | 1048 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1049 |  |
|      2 | 1050 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1051 | `	ph7_value_int(pVal,2);` |
|      5 | 1052 |  |
|      - | 1053 | `/*` |
|      - | 1054 | ` * PATHINFO_EXTENSION` |
|      - | 1055 | ` *  Expand 3.` |
|      - | 1056 | ` */` |
|   3080 | 1057 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1058 |  |
|   1540 | 1059 | `	SXUNUSED(pUserData); /* cc warning */` |
|   3082 | 1060 | `	ph7_value_int(pVal,3);` |
|   3082 | 1061 |  |
|      - | 1062 | `/*` |
|      - | 1063 | ` * PATHINFO_FILENAME` |
|      - | 1064 | ` *  Expand 4.` |
|      - | 1065 | ` */` |
|   3076 | 1066 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1067 |  |
|   1538 | 1068 | `	SXUNUSED(pUserData); /* cc warning */` |
|   3078 | 1069 | `	ph7_value_int(pVal,4);` |
|   3078 | 1070 |  |
|      - | 1071 | `/*` |
|      - | 1072 | ` * ASSERT_ACTIVE.` |
|      - | 1073 | ` *  Expand the value of PH7_ASSERT_ACTIVE defined in ph7Int.h` |
|      - | 1074 | ` */` |
|      2 | 1075 | `static void PH7_ASSERT_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1076 |  |
|      1 | 1077 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1078 | `	ph7_value_int(pVal,PH7_ASSERT_DISABLE);` |
|      3 | 1079 |  |
|      - | 1080 | `/*` |
|      - | 1081 | ` * ASSERT_WARNING.` |
|      - | 1082 | ` *  Expand the value of PH7_ASSERT_WARNING defined in ph7Int.h` |
|      - | 1083 | ` */` |
|      2 | 1084 | `static void PH7_ASSERT_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1085 |  |
|      1 | 1086 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1087 | `	ph7_value_int(pVal,PH7_ASSERT_WARNING);` |
|      3 | 1088 |  |
|      - | 1089 | `/*` |
|      - | 1090 | ` * ASSERT_BAIL.` |
|      - | 1091 | ` *  Expand the value of PH7_ASSERT_BAIL defined in ph7Int.h` |
|      - | 1092 | ` */` |
|      2 | 1093 | `static void PH7_ASSERT_BAIL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1094 |  |
|      1 | 1095 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1096 | `	ph7_value_int(pVal,PH7_ASSERT_BAIL);` |
|      3 | 1097 |  |
|      - | 1098 | `/*` |
|      - | 1099 | ` * ASSERT_QUIET_EVAL.` |
|      - | 1100 | ` *  Expand the value of PH7_ASSERT_QUIET_EVAL defined in ph7Int.h` |
|      - | 1101 | ` */` |
|      2 | 1102 | `static void PH7_ASSERT_QUIET_EVAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1103 |  |
|      1 | 1104 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1105 | `	ph7_value_int(pVal,PH7_ASSERT_QUIET_EVAL);` |
|      3 | 1106 |  |
|      - | 1107 | `/*` |
|      - | 1108 | ` * ASSERT_CALLBACK.` |
|      - | 1109 | ` *  Expand the value of PH7_ASSERT_CALLBACK defined in ph7Int.h` |
|      - | 1110 | ` */` |
|      2 | 1111 | `static void PH7_ASSERT_CALLBACK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1112 |  |
|      1 | 1113 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1114 | `	ph7_value_int(pVal,PH7_ASSERT_CALLBACK);` |
|      3 | 1115 |  |
|      - | 1116 | `/*` |
|      - | 1117 | ` * SEEK_SET.` |
|      - | 1118 | ` *  Expand 0` |
|      - | 1119 | ` */` |
|      2 | 1120 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1121 |  |
|      1 | 1122 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1123 | `	ph7_value_int(pVal,0);` |
|      3 | 1124 |  |
|      - | 1125 | `/*` |
|      - | 1126 | ` * SEEK_CUR.` |
|      - | 1127 | ` *  Expand 1` |
|      - | 1128 | ` */` |
|      2 | 1129 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1130 |  |
|      1 | 1131 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1132 | `	ph7_value_int(pVal,1);` |
|      3 | 1133 |  |
|      - | 1134 | `/*` |
|      - | 1135 | ` * SEEK_END.` |
|      - | 1136 | ` *  Expand 2` |
|      - | 1137 | ` */` |
|      2 | 1138 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1139 |  |
|      1 | 1140 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1141 | `	ph7_value_int(pVal,2);` |
|      3 | 1142 |  |
|      - | 1143 | `/*` |
|      - | 1144 | ` * LOCK_SH.` |
|      - | 1145 | ` *  Expand 2` |
|      - | 1146 | ` */` |
|      2 | 1147 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1148 |  |
|      1 | 1149 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1150 | `	ph7_value_int(pVal,1);` |
|      3 | 1151 |  |
|      - | 1152 | `/*` |
|      - | 1153 | ` * LOCK_NB.` |
|      - | 1154 | ` *  Expand 5` |
|      - | 1155 | ` */` |
|      2 | 1156 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1157 |  |
|      1 | 1158 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1159 | `	ph7_value_int(pVal,5);` |
|      3 | 1160 |  |
|      - | 1161 | `/*` |
|      - | 1162 | ` * LOCK_EX.` |
|      - | 1163 | ` *  Expand 0x01 (MUST BE A POWER OF TWO)` |
|      - | 1164 | ` */` |
|      4 | 1165 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1166 |  |
|      2 | 1167 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1168 | `	ph7_value_int(pVal,0x01);` |
|      5 | 1169 |  |
|      - | 1170 | `/*` |
|      - | 1171 | ` * LOCK_UN.` |
|      - | 1172 | ` *  Expand 0` |
|      - | 1173 | ` */` |
|      4 | 1174 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1175 |  |
|      2 | 1176 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1177 | `	ph7_value_int(pVal,0);` |
|      5 | 1178 |  |
|      - | 1179 | `/*` |
|      - | 1180 | ` * FILE_USE_INCLUDE_PATH` |
|      - | 1181 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1182 | ` */` |
|      2 | 1183 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1184 |  |
|      1 | 1185 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1186 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1187 |  |
|      - | 1188 | `/*` |
|      - | 1189 | ` * FILE_IGNORE_NEW_LINES` |
|      - | 1190 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1191 | ` */` |
|      2 | 1192 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1193 |  |
|      1 | 1194 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1195 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1196 |  |
|      - | 1197 | `/*` |
|      - | 1198 | ` * FILE_SKIP_EMPTY_LINES` |
|      - | 1199 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1200 | ` */` |
|      2 | 1201 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1202 |  |
|      1 | 1203 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1204 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1205 |  |
|      - | 1206 | `/*` |
|      - | 1207 | ` * FILE_APPEND` |
|      - | 1208 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1209 | ` */` |
|      2 | 1210 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1211 |  |
|      1 | 1212 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1213 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1214 |  |
|      - | 1215 | `/*` |
|      - | 1216 | ` * SCANDIR_SORT_ASCENDING` |
|      - | 1217 | ` *  Expand 0` |
|      - | 1218 | ` */` |
|   1498 | 1219 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1220 |  |
|    749 | 1221 | `	SXUNUSED(pUserData); /* cc warning */` |
|   1500 | 1222 | `	ph7_value_int(pVal,0);` |
|   1500 | 1223 |  |
|      - | 1224 | `/*` |
|      - | 1225 | ` * SCANDIR_SORT_DESCENDING` |
|      - | 1226 | ` *  Expand 1` |
|      - | 1227 | ` */` |
|    750 | 1228 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1229 |  |
|    375 | 1230 | `	SXUNUSED(pUserData); /* cc warning */` |
|    752 | 1231 | `	ph7_value_int(pVal,1);` |
|    752 | 1232 |  |
|      - | 1233 | `/*` |
|      - | 1234 | ` * SCANDIR_SORT_NONE` |
|      - | 1235 | ` *  Expand 2` |
|      - | 1236 | ` */` |
|      2 | 1237 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1238 |  |
|      1 | 1239 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1240 | `	ph7_value_int(pVal,2);` |
|      3 | 1241 |  |
|      - | 1242 | `/*` |
|      - | 1243 | ` * GLOB_MARK` |
|      - | 1244 | ` *  Expand 0x01 (must be a power of two)` |
|      - | 1245 | ` */` |
|      2 | 1246 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1247 |  |
|      1 | 1248 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1249 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1250 |  |
|      - | 1251 | `/*` |
|      - | 1252 | ` * GLOB_NOSORT` |
|      - | 1253 | ` *  Expand 0x02 (must be a power of two)` |
|      - | 1254 | ` */` |
|      2 | 1255 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1256 |  |
|      1 | 1257 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1258 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1259 |  |
|      - | 1260 | `/*` |
|      - | 1261 | ` * GLOB_NOCHECK` |
|      - | 1262 | ` *  Expand 0x04 (must be a power of two)` |
|      - | 1263 | ` */` |
|      2 | 1264 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1265 |  |
|      1 | 1266 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1267 | `	ph7_value_int(pVal,0x04);` |
|      3 | 1268 |  |
|      - | 1269 | `/*` |
|      - | 1270 | ` * GLOB_NOESCAPE` |
|      - | 1271 | ` *  Expand 0x08 (must be a power of two)` |
|      - | 1272 | ` */` |
|      2 | 1273 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1274 |  |
|      1 | 1275 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1276 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1277 |  |
|      - | 1278 | `/*` |
|      - | 1279 | ` * GLOB_BRACE` |
|      - | 1280 | ` *  Expand 0x10 (must be a power of two)` |
|      - | 1281 | ` */` |
|      2 | 1282 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1283 |  |
|      1 | 1284 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1285 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1286 |  |
|      - | 1287 | `/*` |
|      - | 1288 | ` * GLOB_ONLYDIR` |
|      - | 1289 | ` *  Expand 0x20 (must be a power of two)` |
|      - | 1290 | ` */` |
|      2 | 1291 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1292 |  |
|      1 | 1293 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1294 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1295 |  |
|      - | 1296 | `/*` |
|      - | 1297 | ` * GLOB_ERR` |
|      - | 1298 | ` *  Expand 0x40 (must be a power of two)` |
|      - | 1299 | ` */` |
|      2 | 1300 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1301 |  |
|      1 | 1302 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1303 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1304 |  |
|      - | 1305 | `/*` |
|      - | 1306 | ` * STDIN` |
|      - | 1307 | ` *  Expand the STDIN handle as a resource.` |
|      - | 1308 | ` */` |
|      2 | 1309 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1310 |  |
|      3 | 1311 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1312 | `	void *pResource;` |
|      3 | 1313 | `	pResource = PH7_ExportStdin(pVm);` |
|      3 | 1314 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1315 |  |
|      - | 1316 | `/*` |
|      - | 1317 | ` * STDOUT` |
|      - | 1318 | ` *   Expand the STDOUT handle as a resource.` |
|      - | 1319 | ` */` |
|      2 | 1320 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1321 |  |
|      3 | 1322 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1323 | `	void *pResource;` |
|      3 | 1324 | `	pResource = PH7_ExportStdout(pVm);` |
|      3 | 1325 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1326 |  |
|      - | 1327 | `/*` |
|      - | 1328 | ` * STDERR` |
|      - | 1329 | ` *  Expand the STDERR handle as a resource.` |
|      - | 1330 | ` */` |
|      2 | 1331 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1332 |  |
|      3 | 1333 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1334 | `	void *pResource;` |
|      3 | 1335 | `	pResource = PH7_ExportStderr(pVm);` |
|      3 | 1336 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1337 |  |
|      - | 1338 | `/*` |
|      - | 1339 | ` * INI_SCANNER_NORMAL` |
|      - | 1340 | ` *   Expand 1` |
|      - | 1341 | ` */` |
|      2 | 1342 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1343 |  |
|      1 | 1344 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1345 | `	ph7_value_int(pVal,1);` |
|      3 | 1346 |  |
|      - | 1347 | `/*` |
|      - | 1348 | ` * INI_SCANNER_RAW` |
|      - | 1349 | ` *   Expand 2` |
|      - | 1350 | ` */` |
|      2 | 1351 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1352 |  |
|      1 | 1353 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1354 | `	ph7_value_int(pVal,2);` |
|      3 | 1355 |  |
|      - | 1356 | `/*` |
|      - | 1357 | ` * EXTR_OVERWRITE` |
|      - | 1358 | ` *   Expand 0x01 (Must be a power of two)` |
|      - | 1359 | ` */` |
|      2 | 1360 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1361 |  |
|      1 | 1362 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1363 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1364 |  |
|      - | 1365 | `/*` |
|      - | 1366 | ` * EXTR_SKIP` |
|      - | 1367 | ` *   Expand 0x02 (Must be a power of two)` |
|      - | 1368 | ` */` |
|      2 | 1369 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1370 |  |
|      1 | 1371 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1372 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1373 |  |
|      - | 1374 | `/*` |
|      - | 1375 | ` * EXTR_PREFIX_SAME` |
|      - | 1376 | ` *   Expand 0x04 (Must be a power of two)` |
|      - | 1377 | ` */` |
|      2 | 1378 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1379 |  |
|      1 | 1380 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1381 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1382 |  |
|      - | 1383 | `/*` |
|      - | 1384 | ` * EXTR_PREFIX_ALL` |
|      - | 1385 | ` *   Expand 0x08 (Must be a power of two)` |
|      - | 1386 | ` */` |
|      2 | 1387 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1388 |  |
|      1 | 1389 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1390 | `	ph7_value_int(pVal,0x8);` |
|      3 | 1391 |  |
|      - | 1392 | `/*` |
|      - | 1393 | ` * EXTR_PREFIX_INVALID` |
|      - | 1394 | ` *   Expand 0x10 (Must be a power of two)` |
|      - | 1395 | ` */` |
|      2 | 1396 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1397 |  |
|      1 | 1398 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1399 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1400 |  |
|      - | 1401 | `/*` |
|      - | 1402 | ` * EXTR_IF_EXISTS` |
|      - | 1403 | ` *   Expand 0x20 (Must be a power of two)` |
|      - | 1404 | ` */` |
|      2 | 1405 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1406 |  |
|      1 | 1407 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1408 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1409 |  |
|      - | 1410 | `/*` |
|      - | 1411 | ` * EXTR_PREFIX_IF_EXISTS` |
|      - | 1412 | ` *   Expand 0x40 (Must be a power of two)` |
|      - | 1413 | ` */` |
|      2 | 1414 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1415 |  |
|      1 | 1416 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1417 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1418 |  |
|      - | 1419 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1420 | `/*` |
|      - | 1421 | ` * XML_ERROR_NONE` |
|      - | 1422 | ` *   Expand the value of SXML_ERROR_NO_MEMORY defined in ph7Int.h` |
|      - | 1423 | ` */` |
|      2 | 1424 | `static void PH7_XML_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1425 |  |
|      1 | 1426 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1427 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1428 |  |
|      - | 1429 | `/*` |
|      - | 1430 | ` * XML_ERROR_NO_MEMORY` |
|      - | 1431 | ` *   Expand the value of SXML_ERROR_NONE defined in ph7Int.h` |
|      - | 1432 | ` */` |
|      2 | 1433 | `static void PH7_XML_ERROR_NO_MEMORY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1434 |  |
|      1 | 1435 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1436 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1437 |  |
|      - | 1438 | `/*` |
|      - | 1439 | ` * XML_ERROR_SYNTAX` |
|      - | 1440 | ` *   Expand the value of SXML_ERROR_SYNTAX defined in ph7Int.h` |
|      - | 1441 | ` */` |
|      2 | 1442 | `static void PH7_XML_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1443 |  |
|      1 | 1444 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1445 | `	ph7_value_int(pVal,SXML_ERROR_SYNTAX);` |
|      3 | 1446 |  |
|      - | 1447 | `/*` |
|      - | 1448 | ` * XML_ERROR_NO_ELEMENTS` |
|      - | 1449 | ` *   Expand the value of SXML_ERROR_NO_ELEMENTS defined in ph7Int.h` |
|      - | 1450 | ` */` |
|      2 | 1451 | `static void PH7_XML_ERROR_NO_ELEMENTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1452 |  |
|      1 | 1453 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1454 | `	ph7_value_int(pVal,SXML_ERROR_NO_ELEMENTS);` |
|      3 | 1455 |  |
|      - | 1456 | `/*` |
|      - | 1457 | ` * XML_ERROR_INVALID_TOKEN` |
|      - | 1458 | ` *   Expand the value of SXML_ERROR_INVALID_TOKEN defined in ph7Int.h` |
|      - | 1459 | ` */` |
|      2 | 1460 | `static void PH7_XML_ERROR_INVALID_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1461 |  |
|      1 | 1462 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1463 | `	ph7_value_int(pVal,SXML_ERROR_INVALID_TOKEN);` |
|      3 | 1464 |  |
|      - | 1465 | `/*` |
|      - | 1466 | ` * XML_ERROR_UNCLOSED_TOKEN` |
|      - | 1467 | ` *   Expand the value of SXML_ERROR_UNCLOSED_TOKEN defined in ph7Int.h` |
|      - | 1468 | ` */` |
|      2 | 1469 | `static void PH7_XML_ERROR_UNCLOSED_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1470 |  |
|      1 | 1471 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1472 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_TOKEN);` |
|      3 | 1473 |  |
|      - | 1474 | `/*` |
|      - | 1475 | ` * XML_ERROR_PARTIAL_CHAR` |
|      - | 1476 | ` *   Expand the value of SXML_ERROR_PARTIAL_CHAR defined in ph7Int.h` |
|      - | 1477 | ` */` |
|      2 | 1478 | `static void PH7_XML_ERROR_PARTIAL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1479 |  |
|      1 | 1480 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1481 | `	ph7_value_int(pVal,SXML_ERROR_PARTIAL_CHAR);` |
|      3 | 1482 |  |
|      - | 1483 | `/*` |
|      - | 1484 | ` * XML_ERROR_TAG_MISMATCH` |
|      - | 1485 | ` *   Expand the value of SXML_ERROR_TAG_MISMATCH defined in ph7Int.h` |
|      - | 1486 | ` */` |
|      2 | 1487 | `static void PH7_XML_ERROR_TAG_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1488 |  |
|      1 | 1489 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1490 | `	ph7_value_int(pVal,SXML_ERROR_TAG_MISMATCH);` |
|      3 | 1491 |  |
|      - | 1492 | `/*` |
|      - | 1493 | ` * XML_ERROR_DUPLICATE_ATTRIBUTE` |
|      - | 1494 | ` *   Expand the value of SXML_ERROR_DUPLICATE_ATTRIBUTE defined in ph7Int.h` |
|      - | 1495 | ` */` |
|      2 | 1496 | `static void PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1497 |  |
|      1 | 1498 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1499 | `	ph7_value_int(pVal,SXML_ERROR_DUPLICATE_ATTRIBUTE);` |
|      3 | 1500 |  |
|      - | 1501 | `/*` |
|      - | 1502 | ` * XML_ERROR_JUNK_AFTER_DOC_ELEMENT` |
|      - | 1503 | ` *   Expand the value of SXML_ERROR_JUNK_AFTER_DOC_ELEMENT defined in ph7Int.h` |
|      - | 1504 | ` */` |
|      2 | 1505 | `static void PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1506 |  |
|      1 | 1507 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1508 | `	ph7_value_int(pVal,SXML_ERROR_JUNK_AFTER_DOC_ELEMENT);` |
|      3 | 1509 |  |
|      - | 1510 | `/*` |
|      - | 1511 | ` * XML_ERROR_PARAM_ENTITY_REF` |
|      - | 1512 | ` *   Expand the value of SXML_ERROR_PARAM_ENTITY_REF defined in ph7Int.h` |
|      - | 1513 | ` */` |
|      2 | 1514 | `static void PH7_XML_ERROR_PARAM_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1515 |  |
|      1 | 1516 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1517 | `	ph7_value_int(pVal,SXML_ERROR_PARAM_ENTITY_REF);` |
|      3 | 1518 |  |
|      - | 1519 | `/*` |
|      - | 1520 | ` * XML_ERROR_UNDEFINED_ENTITY` |
|      - | 1521 | ` *   Expand the value of SXML_ERROR_UNDEFINED_ENTITY defined in ph7Int.h` |
|      - | 1522 | ` */` |
|      2 | 1523 | `static void PH7_XML_ERROR_UNDEFINED_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1524 |  |
|      1 | 1525 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1526 | `	ph7_value_int(pVal,SXML_ERROR_UNDEFINED_ENTITY);` |
|      3 | 1527 |  |
|      - | 1528 | `/*` |
|      - | 1529 | ` * XML_ERROR_RECURSIVE_ENTITY_REF` |
|      - | 1530 | ` *   Expand the value of SXML_ERROR_RECURSIVE_ENTITY_REF defined in ph7Int.h` |
|      - | 1531 | ` */` |
|      2 | 1532 | `static void PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1533 |  |
|      1 | 1534 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1535 | `	ph7_value_int(pVal,SXML_ERROR_RECURSIVE_ENTITY_REF);` |
|      3 | 1536 |  |
|      - | 1537 | `/*` |
|      - | 1538 | ` * XML_ERROR_ASYNC_ENTITY` |
|      - | 1539 | ` *   Expand the value of SXML_ERROR_ASYNC_ENTITY defined in ph7Int.h` |
|      - | 1540 | ` */` |
|      2 | 1541 | `static void PH7_XML_ERROR_ASYNC_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1542 |  |
|      1 | 1543 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1544 | `	ph7_value_int(pVal,SXML_ERROR_ASYNC_ENTITY);` |
|      3 | 1545 |  |
|      - | 1546 | `/*` |
|      - | 1547 | ` * XML_ERROR_BAD_CHAR_REF` |
|      - | 1548 | ` *   Expand the value of SXML_ERROR_BAD_CHAR_REF defined in ph7Int.h` |
|      - | 1549 | ` */` |
|      2 | 1550 | `static void PH7_XML_ERROR_BAD_CHAR_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1551 |  |
|      1 | 1552 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1553 | `	ph7_value_int(pVal,SXML_ERROR_BAD_CHAR_REF);` |
|      3 | 1554 |  |
|      - | 1555 | `/*` |
|      - | 1556 | ` * XML_ERROR_BINARY_ENTITY_REF` |
|      - | 1557 | ` *   Expand the value of SXML_ERROR_BINARY_ENTITY_REF defined in ph7Int.h` |
|      - | 1558 | ` */` |
|      2 | 1559 | `static void PH7_XML_ERROR_BINARY_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1560 |  |
|      1 | 1561 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1562 | `	ph7_value_int(pVal,SXML_ERROR_BINARY_ENTITY_REF);` |
|      3 | 1563 |  |
|      - | 1564 | `/*` |
|      - | 1565 | ` * XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF` |
|      - | 1566 | ` *   Expand the value of SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF defined in ph7Int.h` |
|      - | 1567 | ` */` |
|      2 | 1568 | `static void PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1569 |  |
|      1 | 1570 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1571 | `	ph7_value_int(pVal,SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF);` |
|      3 | 1572 |  |
|      - | 1573 | `/*` |
|      - | 1574 | ` * XML_ERROR_MISPLACED_XML_PI` |
|      - | 1575 | ` *   Expand the value of SXML_ERROR_MISPLACED_XML_PI defined in ph7Int.h` |
|      - | 1576 | ` */` |
|      2 | 1577 | `static void PH7_XML_ERROR_MISPLACED_XML_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1578 |  |
|      1 | 1579 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1580 | `	ph7_value_int(pVal,SXML_ERROR_MISPLACED_XML_PI);` |
|      3 | 1581 |  |
|      - | 1582 | `/*` |
|      - | 1583 | ` * XML_ERROR_UNKNOWN_ENCODING` |
|      - | 1584 | ` *   Expand the value of SXML_ERROR_UNKNOWN_ENCODING defined in ph7Int.h` |
|      - | 1585 | ` */` |
|      2 | 1586 | `static void PH7_XML_ERROR_UNKNOWN_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1587 |  |
|      1 | 1588 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1589 | `	ph7_value_int(pVal,SXML_ERROR_UNKNOWN_ENCODING);` |
|      3 | 1590 |  |
|      - | 1591 | `/*` |
|      - | 1592 | ` * XML_ERROR_INCORRECT_ENCODING` |
|      - | 1593 | ` *   Expand the value of SXML_ERROR_INCORRECT_ENCODING defined in ph7Int.h` |
|      - | 1594 | ` */` |
|      2 | 1595 | `static void PH7_XML_ERROR_INCORRECT_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1596 |  |
|      1 | 1597 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1598 | `	ph7_value_int(pVal,SXML_ERROR_INCORRECT_ENCODING);` |
|      3 | 1599 |  |
|      - | 1600 | `/*` |
|      - | 1601 | ` * XML_ERROR_UNCLOSED_CDATA_SECTION` |
|      - | 1602 | ` *   Expand the value of SXML_ERROR_UNCLOSED_CDATA_SECTION defined in ph7Int.h` |
|      - | 1603 | ` */` |
|      2 | 1604 | `static void PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1605 |  |
|      1 | 1606 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1607 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_CDATA_SECTION);` |
|      3 | 1608 |  |
|      - | 1609 | `/*` |
|      - | 1610 | ` * XML_ERROR_EXTERNAL_ENTITY_HANDLING` |
|      - | 1611 | ` *   Expand the value of SXML_ERROR_EXTERNAL_ENTITY_HANDLING defined in ph7Int.h` |
|      - | 1612 | ` */` |
|      2 | 1613 | `static void PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1614 |  |
|      1 | 1615 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1616 | `	ph7_value_int(pVal,SXML_ERROR_EXTERNAL_ENTITY_HANDLING);` |
|      3 | 1617 |  |
|      - | 1618 | `/*` |
|      - | 1619 | ` * XML_OPTION_CASE_FOLDING` |
|      - | 1620 | ` *   Expand the value of SXML_OPTION_CASE_FOLDING defined in ph7Int.h.` |
|      - | 1621 | ` */` |
|      2 | 1622 | `static void PH7_XML_OPTION_CASE_FOLDING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1623 |  |
|      1 | 1624 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1625 | `	ph7_value_int(pVal,SXML_OPTION_CASE_FOLDING);` |
|      3 | 1626 |  |
|      - | 1627 | `/*` |
|      - | 1628 | ` * XML_OPTION_TARGET_ENCODING` |
|      - | 1629 | ` *   Expand the value of SXML_OPTION_TARGET_ENCODING defined in ph7Int.h.` |
|      - | 1630 | ` */` |
|      4 | 1631 | `static void PH7_XML_OPTION_TARGET_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1632 |  |
|      2 | 1633 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1634 | `	ph7_value_int(pVal,SXML_OPTION_TARGET_ENCODING);` |
|      5 | 1635 |  |
|      - | 1636 | `/*` |
|      - | 1637 | ` * XML_OPTION_SKIP_TAGSTART` |
|      - | 1638 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1639 | ` */` |
|      2 | 1640 | `static void PH7_XML_OPTION_SKIP_TAGSTART_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1641 |  |
|      1 | 1642 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1643 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_TAGSTART);` |
|      3 | 1644 |  |
|      - | 1645 | `/*` |
|      - | 1646 | ` * XML_OPTION_SKIP_WHITE` |
|      - | 1647 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1648 | ` */` |
|      4 | 1649 | `static void PH7_XML_OPTION_SKIP_WHITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1650 |  |
|      2 | 1651 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1652 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_WHITE);` |
|      5 | 1653 |  |
|      - | 1654 | `/*` |
|      - | 1655 | ` * XML_SAX_IMPL.` |
|      - | 1656 | ` *   Expand the name of the underlying XML engine.` |
|      - | 1657 | ` */` |
|      2 | 1658 | `static void PH7_XML_SAX_IMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1659 |  |
|      1 | 1660 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1661 | `	ph7_value_string(pVal,"Symisc XML engine",(int)sizeof("Symisc XML engine")-1);` |
|      3 | 1662 |  |
|      - | 1663 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1664 | `/*` |
|      - | 1665 | ` * JSON_HEX_TAG.` |
|      - | 1666 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|      - | 1667 | ` */` |
|      2 | 1668 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1669 |  |
|      1 | 1670 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1671 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      3 | 1672 |  |
|      - | 1673 | `/*` |
|      - | 1674 | ` * JSON_HEX_AMP.` |
|      - | 1675 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|      - | 1676 | ` */` |
|      2 | 1677 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1678 |  |
|      1 | 1679 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1680 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      3 | 1681 |  |
|      - | 1682 | `/*` |
|      - | 1683 | ` * JSON_HEX_APOS.` |
|      - | 1684 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|      - | 1685 | ` */` |
|      2 | 1686 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1687 |  |
|      1 | 1688 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1689 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      3 | 1690 |  |
|      - | 1691 | `/*` |
|      - | 1692 | ` * JSON_HEX_QUOT.` |
|      - | 1693 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|      - | 1694 | ` */` |
|      2 | 1695 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1696 |  |
|      1 | 1697 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1698 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      3 | 1699 |  |
|      - | 1700 | `/*` |
|      - | 1701 | ` * JSON_FORCE_OBJECT.` |
|      - | 1702 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|      - | 1703 | ` */` |
|      2 | 1704 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1705 |  |
|      1 | 1706 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1707 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      3 | 1708 |  |
|      - | 1709 | `/*` |
|      - | 1710 | ` * JSON_NUMERIC_CHECK.` |
|      - | 1711 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|      - | 1712 | ` */` |
|      2 | 1713 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1714 |  |
|      1 | 1715 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1716 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      3 | 1717 |  |
|      - | 1718 | `/*` |
|      - | 1719 | ` * JSON_BIGINT_AS_STRING.` |
|      - | 1720 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|      - | 1721 | ` */` |
|      2 | 1722 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1723 |  |
|      1 | 1724 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1725 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      3 | 1726 |  |
|      - | 1727 | `/*` |
|      - | 1728 | ` * JSON_PRETTY_PRINT.` |
|      - | 1729 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|      - | 1730 | ` */` |
|      2 | 1731 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1732 |  |
|      1 | 1733 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1734 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      3 | 1735 |  |
|      - | 1736 | `/*` |
|      - | 1737 | ` * JSON_UNESCAPED_SLASHES.` |
|      - | 1738 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|      - | 1739 | ` */` |
|      2 | 1740 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1741 |  |
|      1 | 1742 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1743 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      3 | 1744 |  |
|      - | 1745 | `/*` |
|      - | 1746 | ` * JSON_UNESCAPED_UNICODE.` |
|      - | 1747 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|      - | 1748 | ` */` |
|      2 | 1749 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1750 |  |
|      1 | 1751 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1752 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|      3 | 1753 |  |
|      - | 1754 | `/*` |
|      - | 1755 | ` * JSON_ERROR_NONE.` |
|      - | 1756 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|      - | 1757 | ` */` |
|      4 | 1758 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1759 |  |
|      2 | 1760 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1761 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      5 | 1762 |  |
|      - | 1763 | `/*` |
|      - | 1764 | ` * JSON_ERROR_DEPTH.` |
|      - | 1765 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|      - | 1766 | ` */` |
|      2 | 1767 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1768 |  |
|      1 | 1769 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1770 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      3 | 1771 |  |
|      - | 1772 | `/*` |
|      - | 1773 | ` * JSON_ERROR_STATE_MISMATCH.` |
|      - | 1774 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|      - | 1775 | ` */` |
|      2 | 1776 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1777 |  |
|      1 | 1778 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1779 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      3 | 1780 |  |
|      - | 1781 | `/*` |
|      - | 1782 | ` * JSON_ERROR_CTRL_CHAR.` |
|      - | 1783 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|      - | 1784 | ` */` |
|      2 | 1785 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1786 |  |
|      1 | 1787 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1788 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      3 | 1789 |  |
|      - | 1790 | `/*` |
|      - | 1791 | ` * JSON_ERROR_SYNTAX.` |
|      - | 1792 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|      - | 1793 | ` */` |
|      2 | 1794 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1795 |  |
|      1 | 1796 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1797 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      3 | 1798 |  |
|      - | 1799 | `/*` |
|      - | 1800 | ` * JSON_ERROR_UTF8.` |
|      - | 1801 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|      - | 1802 | ` */` |
|      2 | 1803 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1804 |  |
|      1 | 1805 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1806 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      3 | 1807 |  |
|      - | 1808 | `/*` |
|      - | 1809 | ` * static` |
|      - | 1810 | ` *  Expand the name of the current class. 'static' otherwise.` |
|      - | 1811 | ` */` |
|     12 | 1812 | `static void PH7_static_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1813 |  |
|     13 | 1814 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1815 | `	ph7_class *pClass;` |
|      - | 1816 | `	/* Extract the target class if available */` |
|     13 | 1817 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|     13 | 1818 | `	if( pClass ){` |
|      9 | 1819 | `		SyString *pName = &pClass->sName;` |
|      - | 1820 | `		/* Expand class name */` |
|      9 | 1821 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      5 | 1822 | `	}else{` |
|      - | 1823 | `		/* Expand 'static' */` |
|      5 | 1824 | `		ph7_value_string(pVal,"static",sizeof("static")-1);` |
|      - | 1825 | `	}` |
|     13 | 1826 |  |
|      - | 1827 | `/*` |
|      - | 1828 | ` * self` |
|      - | 1829 | ` * __CLASS__` |
|      - | 1830 | ` *  Expand the name of the current class. NULL otherwise.` |
|      - | 1831 | ` */` |
|      8 | 1832 | `static void PH7_self_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1833 |  |
|      9 | 1834 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1835 | `	ph7_class *pClass;` |
|      - | 1836 |  |
|      - | 1837 | `	/* Get the declaring class of the current method */` |
|      9 | 1838 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      9 | 1839 | `	if( pClass == 0 ){` |
|      - | 1840 | `		/* Not in a method, fall back to runtime class */` |
|      3 | 1841 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      1 | 1842 | `	}` |
|      - | 1843 |  |
|      9 | 1844 | `	if( pClass ){` |
|      7 | 1845 | `		SyString *pName = &pClass->sName;` |
|      - | 1846 | `		/* Expand class name */` |
|      7 | 1847 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      4 | 1848 | `	}else{` |
|      - | 1849 | `		/* Expand null */` |
|      3 | 1850 | `		ph7_value_null(pVal);` |
|      - | 1851 | `	}` |
|      9 | 1852 |  |
|      - | 1853 | `/* parent` |
|      - | 1854 | ` *  Expand the name of the parent class. NULL otherwise.` |
|      - | 1855 | ` */` |
|     10 | 1856 | `static void PH7_parent_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1857 |  |
|     11 | 1858 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1859 | `	ph7_class *pClass;` |
|      - | 1860 |  |
|      - | 1861 | `	/* Get the declaring class, then its parent */` |
|     11 | 1862 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|     15 | 1863 | `	if( pClass && pClass->pBase ){` |
|      9 | 1864 | `		SyString *pName = &pClass->pBase->sName;` |
|      - | 1865 | `		/* Expand parent class name */` |
|      9 | 1866 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      5 | 1867 | `	}else{` |
|      - | 1868 | `		/* Expand null */` |
|      3 | 1869 | `		ph7_value_null(pVal);` |
|      - | 1870 | `	}` |
|     11 | 1871 |  |
|      - | 1872 |  |
|      - | 1873 | `/*` |
|      - | 1874 | ` * Table of built-in constants.` |
|      - | 1875 | ` */` |
|      - | 1876 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|      - | 1877 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|      - | 1878 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|      - | 1879 | `	{"__PH7__",              PH7_VER_Const      },` |
|      - | 1880 | `	{"PHP_OS",               PH7_OS_Const       },` |
|      - | 1881 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|      - | 1882 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|      - | 1883 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|      - | 1884 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|      - | 1885 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|      - | 1886 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|      - | 1887 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|      - | 1888 | `	{"__TIME__",             PH7_TIME_Const     },` |
|      - | 1889 | `	{"__DATE__",             PH7_DATE_Const     },` |
|      - | 1890 | `	{"__FILE__",             PH7_FILE_Const     },` |
|      - | 1891 | `	{"__DIR__",              PH7_DIR_Const      },` |
|      - | 1892 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|      - | 1893 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|      - | 1894 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|      - | 1895 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|      - | 1896 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|      - | 1897 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|      - | 1898 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|      - | 1899 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|      - | 1900 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|      - | 1901 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|      - | 1902 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|      - | 1903 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|      - | 1904 | `	{"E_STRICT",             PH7_E_STRICT_Const        },` |
|      - | 1905 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|      - | 1906 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|      - | 1907 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|      - | 1908 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|      - | 1909 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|      - | 1910 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|      - | 1911 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|      - | 1912 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|      - | 1913 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|      - | 1914 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|      - | 1915 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|      - | 1916 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|      - | 1917 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|      - | 1918 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|      - | 1919 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|      - | 1920 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|      - | 1921 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|      - | 1922 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|      - | 1923 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|      - | 1924 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|      - | 1925 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|      - | 1926 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|      - | 1927 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 1928 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|      - | 1929 | `	{"M_E",                  PH7_M_E_Const          },` |
|      - | 1930 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|      - | 1931 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|      - | 1932 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|      - | 1933 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|      - | 1934 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|      - | 1935 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|      - | 1936 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|      - | 1937 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|      - | 1938 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|      - | 1939 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|      - | 1940 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|      - | 1941 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|      - | 1942 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|      - | 1943 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|      - | 1944 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|      - | 1945 | `	{"NAN",                  PH7_NAN_Const          },` |
|      - | 1946 | `	{"INF",                  PH7_INF_Const          },` |
|      - | 1947 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 1948 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|      - | 1949 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|      - | 1950 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|      - | 1951 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|      - | 1952 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|      - | 1953 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|      - | 1954 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|      - | 1955 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|      - | 1956 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|      - | 1957 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|      - | 1958 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|      - | 1959 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|      - | 1960 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|      - | 1961 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|      - | 1962 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|      - | 1963 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|      - | 1964 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|      - | 1965 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|      - | 1966 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|      - | 1967 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|      - | 1968 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|      - | 1969 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|      - | 1970 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|      - | 1971 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|      - | 1972 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|      - | 1973 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|      - | 1974 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|      - | 1975 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|      - | 1976 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|      - | 1977 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|      - | 1978 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|      - | 1979 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|      - | 1980 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|      - | 1981 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|      - | 1982 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|      - | 1983 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|      - | 1984 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|      - | 1985 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|      - | 1986 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|      - | 1987 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|      - | 1988 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|      - | 1989 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|      - | 1990 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|      - | 1991 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|      - | 1992 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|      - | 1993 | `	{"ASSERT_ACTIVE",        PH7_ASSERT_ACTIVE_Const     },` |
|      - | 1994 | `	{"ASSERT_WARNING",       PH7_ASSERT_WARNING_Const    },` |
|      - | 1995 | `	{"ASSERT_BAIL",          PH7_ASSERT_BAIL_Const       },` |
|      - | 1996 | `	{"ASSERT_QUIET_EVAL",    PH7_ASSERT_QUIET_EVAL_Const },` |
|      - | 1997 | `	{"ASSERT_CALLBACK",      PH7_ASSERT_CALLBACK_Const   },` |
|      - | 1998 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|      - | 1999 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|      - | 2000 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|      - | 2001 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|      - | 2002 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|      - | 2003 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|      - | 2004 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|      - | 2005 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|      - | 2006 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|      - | 2007 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|      - | 2008 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|      - | 2009 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|      - | 2010 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|      - | 2011 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|      - | 2012 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|      - | 2013 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|      - | 2014 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|      - | 2015 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|      - | 2016 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|      - | 2017 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|      - | 2018 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|      - | 2019 | `	{"STDIN",                PH7_STDIN_Const        },` |
|      - | 2020 | `	{"stdin",                PH7_STDIN_Const        },` |
|      - | 2021 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|      - | 2022 | `	{"stdout",               PH7_STDOUT_Const       },` |
|      - | 2023 | `	{"STDERR",               PH7_STDERR_Const       },` |
|      - | 2024 | `	{"stderr",               PH7_STDERR_Const       },` |
|      - | 2025 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|      - | 2026 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|      - | 2027 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|      - | 2028 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|      - | 2029 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|      - | 2030 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|      - | 2031 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|      - | 2032 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|      - | 2033 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|      - | 2034 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2035 | `	{"XML_ERROR_NONE",       PH7_XML_ERROR_NONE_Const},` |
|      - | 2036 | `	{"XML_ERROR_NO_MEMORY",  PH7_XML_ERROR_NO_MEMORY_Const},` |
|      - | 2037 | `	{"XML_ERROR_SYNTAX",     PH7_XML_ERROR_SYNTAX_Const},` |
|      - | 2038 | `	{"XML_ERROR_NO_ELEMENTS",PH7_XML_ERROR_NO_ELEMENTS_Const},` |
|      - | 2039 | `	{"XML_ERROR_INVALID_TOKEN", PH7_XML_ERROR_INVALID_TOKEN_Const},` |
|      - | 2040 | `	{"XML_ERROR_UNCLOSED_TOKEN",PH7_XML_ERROR_UNCLOSED_TOKEN_Const},` |
|      - | 2041 | `	{"XML_ERROR_PARTIAL_CHAR",  PH7_XML_ERROR_PARTIAL_CHAR_Const},` |
|      - | 2042 | `	{"XML_ERROR_TAG_MISMATCH",  PH7_XML_ERROR_TAG_MISMATCH_Const},` |
|      - | 2043 | `	{"XML_ERROR_DUPLICATE_ATTRIBUTE",   PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const},` |
|      - | 2044 | `	{"XML_ERROR_JUNK_AFTER_DOC_ELEMENT",PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const},` |
|      - | 2045 | `	{"XML_ERROR_PARAM_ENTITY_REF",      PH7_XML_ERROR_PARAM_ENTITY_REF_Const},` |
|      - | 2046 | `	{"XML_ERROR_UNDEFINED_ENTITY",      PH7_XML_ERROR_UNDEFINED_ENTITY_Const},` |
|      - | 2047 | `	{"XML_ERROR_RECURSIVE_ENTITY_REF",  PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const},` |
|      - | 2048 | `	{"XML_ERROR_ASYNC_ENTITY",          PH7_XML_ERROR_ASYNC_ENTITY_Const},` |
|      - | 2049 | `	{"XML_ERROR_BAD_CHAR_REF",          PH7_XML_ERROR_BAD_CHAR_REF_Const},` |
|      - | 2050 | `	{"XML_ERROR_BINARY_ENTITY_REF",     PH7_XML_ERROR_BINARY_ENTITY_REF_Const},` |
|      - | 2051 | `	{"XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF", PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const},` |
|      - | 2052 | `	{"XML_ERROR_MISPLACED_XML_PI",     PH7_XML_ERROR_MISPLACED_XML_PI_Const},` |
|      - | 2053 | `	{"XML_ERROR_UNKNOWN_ENCODING",     PH7_XML_ERROR_UNKNOWN_ENCODING_Const},` |
|      - | 2054 | `	{"XML_ERROR_INCORRECT_ENCODING",   PH7_XML_ERROR_INCORRECT_ENCODING_Const},` |
|      - | 2055 | `	{"XML_ERROR_UNCLOSED_CDATA_SECTION",  PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const},` |
|      - | 2056 | `	{"XML_ERROR_EXTERNAL_ENTITY_HANDLING",PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const},` |
|      - | 2057 | `	{"XML_OPTION_CASE_FOLDING",           PH7_XML_OPTION_CASE_FOLDING_Const},` |
|      - | 2058 | `	{"XML_OPTION_TARGET_ENCODING",        PH7_XML_OPTION_TARGET_ENCODING_Const},` |
|      - | 2059 | `	{"XML_OPTION_SKIP_TAGSTART",          PH7_XML_OPTION_SKIP_TAGSTART_Const},` |
|      - | 2060 | `	{"XML_OPTION_SKIP_WHITE",             PH7_XML_OPTION_SKIP_WHITE_Const},` |
|      - | 2061 | `	{"XML_SAX_IMPL",           PH7_XML_SAX_IMP_Const},` |
|      - | 2062 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2063 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|      - | 2064 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|      - | 2065 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|      - | 2066 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|      - | 2067 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|      - | 2068 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|      - | 2069 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|      - | 2070 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|      - | 2071 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|      - | 2072 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|      - | 2073 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|      - | 2074 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|      - | 2075 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|      - | 2076 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|      - | 2077 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|      - | 2078 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|      - | 2079 | `	{"static",               PH7_static_Const       },` |
|      - | 2080 | `	{"self",                 PH7_self_Const         },` |
|      - | 2081 | `	{"__CLASS__",            PH7_self_Const         },` |
|      - | 2082 | `	{"parent",               PH7_parent_Const       }` |
|      - | 2083 | `};` |
|      - | 2084 | `/*` |
|      - | 2085 | ` * Register the built-in constants defined above.` |
|      - | 2086 | ` */` |
|    974 | 2087 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|      2 | 2088 |  |
|      - | 2089 | `	sxu32 n;` |
|      - | 2090 | `	/*` |
|      - | 2091 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|      - | 2092 | `	 * that trigger the constant invocation as their private data.` |
|      - | 2093 | `	 */` |
| 197724 | 2094 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 196750 | 2095 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
|  98376 | 2096 | `	}` |
|    976 | 2097 |  |
