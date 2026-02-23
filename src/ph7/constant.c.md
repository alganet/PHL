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
|      - |    7 | `#include <math.h> /* For NAN/INFINITY macros */` |
|      - |    8 | `/* This file implement built-in constants for the PH7 engine. */` |
|      - |    9 | `/*` |
|      - |   10 | ` * PH7_VERSION` |
|      - |   11 | ` * __PH7__` |
|      - |   12 | ` *   Expand the current version of the PH7 engine.` |
|      - |   13 | ` */` |
|      8 |   14 | `static void PH7_VER_Const(ph7_value *pVal,void *pUnused)` |
|      1 |   15 |  |
|      4 |   16 | `	SXUNUSED(pUnused);` |
|      9 |   17 | `	ph7_value_string(pVal,ph7_lib_signature(),-1/*Compute length automatically*/);` |
|      9 |   18 |  |
|      - |   19 | `#ifdef __WINNT__` |
|      - |   20 | `#include <Windows.h>` |
|      - |   21 | `#elif defined(__UNIXES__)` |
|      - |   22 | `#include <sys/utsname.h>` |
|      - |   23 | `#endif` |
|      - |   24 | `/*` |
|      - |   25 | ` * PHP_OS` |
|      - |   26 | ` *  Expand the name of the host Operating System.` |
|      - |   27 | ` */` |
|   1294 |   28 | `static void PH7_OS_Const(ph7_value *pVal,void *pUnused)` |
|      2 |   29 |  |
|      - |   30 | `#if defined(__WINNT__)` |
|      2 |   31 | `	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);` |
|      - |   32 | `#elif defined(__UNIXES__)` |
|      - |   33 | `	struct utsname sInfo;` |
|   1294 |   34 | `	if( uname(&sInfo) != 0 ){` |
|    ! 0 |   35 | `		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);` |
|    ! 0 |   36 | `	}else{` |
|   1294 |   37 | `		ph7_value_string(pVal,sInfo.sysname,-1);` |
|      - |   38 | `	}` |
|      - |   39 | `#else` |
|      - |   40 | `	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);` |
|      - |   41 | `#endif` |
|    647 |   42 | `	SXUNUSED(pUnused);` |
|   1296 |   43 |  |
|      - |   44 | `/*` |
|      - |   45 | ` * PHP_EOL` |
|      - |   46 | ` *  Expand the correct 'End Of Line' symbol for this platform.` |
|      - |   47 | ` */` |
|    488 |   48 | `static void PH7_EOL_Const(ph7_value *pVal,void *pUnused)` |
|      2 |   49 |  |
|    244 |   50 | `	SXUNUSED(pUnused);` |
|      - |   51 | `#ifdef __WINNT__` |
|      2 |   52 | `	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);` |
|      - |   53 | `#else` |
|    488 |   54 | `	ph7_value_string(pVal,"\n",(int)sizeof(char));` |
|      - |   55 | `#endif` |
|    490 |   56 |  |
|      - |   57 | `/*` |
|      - |   58 | ` * PHP_INT_MAX` |
|      - |   59 | ` * Expand the largest integer supported.` |
|      - |   60 | ` * Note that PH7 deals with 64-bit integer for all platforms.` |
|      - |   61 | ` */` |
|      2 |   62 | `static void PH7_INTMAX_Const(ph7_value *pVal,void *pUnused)` |
|      1 |   63 |  |
|      1 |   64 | `	SXUNUSED(pUnused);` |
|      3 |   65 | `	ph7_value_int64(pVal,SXI64_HIGH);` |
|      3 |   66 |  |
|      - |   67 | `/*` |
|      - |   68 | ` * PHP_INT_SIZE` |
|      - |   69 | ` * Expand the size in bytes of a 64-bit integer.` |
|      - |   70 | ` */` |
|      2 |   71 | `static void PH7_INTSIZE_Const(ph7_value *pVal,void *pUnused)` |
|      1 |   72 |  |
|      1 |   73 | `	SXUNUSED(pUnused);` |
|      3 |   74 | `	ph7_value_int64(pVal,sizeof(sxi64));` |
|      3 |   75 |  |
|      - |   76 | `/*` |
|      - |   77 | ` * DIRECTORY_SEPARATOR.` |
|      - |   78 | ` * Expand the directory separator character.` |
|      - |   79 | ` */` |
|    156 |   80 | `static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)` |
|      2 |   81 |  |
|     78 |   82 | `	SXUNUSED(pUnused);` |
|      - |   83 | `#ifdef __WINNT__` |
|      2 |   84 | `	ph7_value_string(pVal,"\\",(int)sizeof(char));` |
|      - |   85 | `#else` |
|    156 |   86 | `	ph7_value_string(pVal,"/",(int)sizeof(char));` |
|      - |   87 | `#endif` |
|    158 |   88 |  |
|      - |   89 | `/*` |
|      - |   90 | ` * PATH_SEPARATOR.` |
|      - |   91 | ` * Expand the path separator character.` |
|      - |   92 | ` */` |
|      2 |   93 | `static void PH7_PATHSEP_Const(ph7_value *pVal,void *pUnused)` |
|      1 |   94 |  |
|      1 |   95 | `	SXUNUSED(pUnused);` |
|      - |   96 | `#ifdef __WINNT__` |
|      1 |   97 | `	ph7_value_string(pVal,";",(int)sizeof(char));` |
|      - |   98 | `#else` |
|      2 |   99 | `	ph7_value_string(pVal,":",(int)sizeof(char));` |
|      - |  100 | `#endif` |
|      3 |  101 |  |
|      - |  102 |  |
|      - |  103 | `/*` |
|      - |  104 | ` * NAN constant: floating-point Not-A-Number` |
|      - |  105 | ` */` |
|     36 |  106 | `static void PH7_NAN_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  107 |  |
|     18 |  108 | `	SXUNUSED(pUnused);` |
|     37 |  109 | `	ph7_value_double(pVal,(double)NAN);` |
|     37 |  110 |  |
|      - |  111 |  |
|      - |  112 | `/*` |
|      - |  113 | ` * INF constant: positive infinity` |
|      - |  114 | ` */` |
|      4 |  115 | `static void PH7_INF_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  116 |  |
|      2 |  117 | `	SXUNUSED(pUnused);` |
|      5 |  118 | `	ph7_value_double(pVal,(double)INFINITY);` |
|      5 |  119 |  |
|      - |  120 |  |
|      - |  121 | `#ifndef __WINNT__` |
|      - |  122 | `#include <time.h>` |
|      - |  123 | `#endif` |
|      - |  124 | `/*` |
|      - |  125 | ` * __TIME__` |
|      - |  126 | ` *  Expand the current time (GMT).` |
|      - |  127 | ` */` |
|      2 |  128 | `static void PH7_TIME_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  129 |  |
|      - |  130 | `	Sytm sTm;` |
|      - |  131 | `#ifdef __WINNT__` |
|      - |  132 | `	SYSTEMTIME sOS;` |
|      1 |  133 | `	GetSystemTime(&sOS);` |
|      1 |  134 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - |  135 | `#else` |
|      - |  136 | `	struct tm *pTm;` |
|      - |  137 | `	time_t t;` |
|      2 |  138 | `	time(&t);` |
|      2 |  139 | `	pTm = gmtime(&t);` |
|      2 |  140 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - |  141 | `#endif` |
|      1 |  142 | `	SXUNUSED(pUnused); /* cc warning */` |
|      - |  143 | `	/* Expand */` |
|      3 |  144 | `	ph7_value_string_format(pVal,"%02d:%02d:%02d",sTm.tm_hour,sTm.tm_min,sTm.tm_sec);` |
|      3 |  145 |  |
|      - |  146 | `/*` |
|      - |  147 | ` * __DATE__` |
|      - |  148 | ` *  Expand the current date in the ISO-8601 format.` |
|      - |  149 | ` */` |
|      2 |  150 | `static void PH7_DATE_Const(ph7_value *pVal,void *pUnused)` |
|      1 |  151 |  |
|      - |  152 | `	Sytm sTm;` |
|      - |  153 | `#ifdef __WINNT__` |
|      - |  154 | `	SYSTEMTIME sOS;` |
|      1 |  155 | `	GetSystemTime(&sOS);` |
|      1 |  156 | `	SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - |  157 | `#else` |
|      - |  158 | `	struct tm *pTm;` |
|      - |  159 | `	time_t t;` |
|      2 |  160 | `	time(&t);` |
|      2 |  161 | `	pTm = gmtime(&t);` |
|      2 |  162 | `	STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - |  163 | `#endif` |
|      1 |  164 | `	SXUNUSED(pUnused); /* cc warning */` |
|      - |  165 | `	/* Expand */` |
|      3 |  166 | `	ph7_value_string_format(pVal,"%04d-%02d-%02d",sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);` |
|      3 |  167 |  |
|      - |  168 | `/*` |
|      - |  169 | ` * __FILE__` |
|      - |  170 | ` *  Path of the processed script.` |
|      - |  171 | ` */` |
|     56 |  172 | `static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  173 |  |
|     58 |  174 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  175 | `	SyString *pFile;` |
|      - |  176 | `	/* Peek the top entry */` |
|     58 |  177 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     58 |  178 | `	if( pFile == 0 ){` |
|      - |  179 | `		/* Expand the magic word: ":MEMORY:" */` |
|      3 |  180 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|      2 |  181 | `	}else{` |
|     56 |  182 | `		ph7_value_string(pVal,pFile->zString,pFile->nByte);` |
|      - |  183 | `	}` |
|     58 |  184 |  |
|      - |  185 | `/*` |
|      - |  186 | ` * __DIR__` |
|      - |  187 | ` *  Directory holding the processed script.` |
|      - |  188 | ` */` |
|     20 |  189 | `static void PH7_DIR_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  190 |  |
|     22 |  191 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  192 | `	SyString *pFile;` |
|      - |  193 | `	/* Peek the top entry */` |
|     22 |  194 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     22 |  195 | `	if( pFile == 0 ){` |
|      - |  196 | `		/* Expand the magic word: ":MEMORY:" */` |
|      3 |  197 | `		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);` |
|      2 |  198 | `	}else{` |
|     19 |  199 | `		if( pFile->nByte > 0 ){` |
|      - |  200 | `			const char *zDir;` |
|      - |  201 | `			int nLen;` |
|     19 |  202 | `			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|     19 |  203 | `			ph7_value_string(pVal,zDir,nLen);` |
|     10 |  204 | `		}else{` |
|      - |  205 | `			/* Expand '.' as the current directory*/` |
|    ! 0 |  206 | `			ph7_value_string(pVal,".",(int)sizeof(char));` |
|      - |  207 | `		}` |
|      - |  208 | `	}` |
|     22 |  209 |  |
|      - |  210 | `/*` |
|      - |  211 | ` * PHP_SHLIB_SUFFIX` |
|      - |  212 | ` *  Expand shared library suffix.` |
|      - |  213 | ` */` |
|      2 |  214 | `static void PH7_PHP_SHLIB_SUFFIX_Const(ph7_value *pVal,void *pUserData)` |
|    ! 0 |  215 |  |
|      - |  216 | `#ifdef __WINNT__` |
|    ! 0 |  217 | `	ph7_value_string(pVal,"dll",(int)sizeof("dll")-1);` |
|      - |  218 | `#else` |
|      2 |  219 | `	ph7_value_string(pVal,"so",(int)sizeof("so")-1);` |
|      - |  220 | `#endif` |
|      1 |  221 | `	SXUNUSED(pUserData); /* cc warning */` |
|      2 |  222 |  |
|      - |  223 | `/*` |
|      - |  224 | ` * E_ERROR` |
|      - |  225 | ` *  Expands 1` |
|      - |  226 | ` */` |
|      2 |  227 | `static void PH7_E_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  228 |  |
|      3 |  229 | `	ph7_value_int(pVal,1);` |
|      1 |  230 | `	SXUNUSED(pUserData);` |
|      3 |  231 |  |
|      - |  232 | `/*` |
|      - |  233 | ` * E_WARNING` |
|      - |  234 | ` *  Expands 2` |
|      - |  235 | ` */` |
|      2 |  236 | `static void PH7_E_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  237 |  |
|      3 |  238 | `	ph7_value_int(pVal,2);` |
|      1 |  239 | `	SXUNUSED(pUserData);` |
|      3 |  240 |  |
|      - |  241 | `/*` |
|      - |  242 | ` * E_PARSE` |
|      - |  243 | ` *  Expands 4` |
|      - |  244 | ` */` |
|      2 |  245 | `static void PH7_E_PARSE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  246 |  |
|      3 |  247 | `	ph7_value_int(pVal,4);` |
|      1 |  248 | `	SXUNUSED(pUserData);` |
|      3 |  249 |  |
|      - |  250 | `/*` |
|      - |  251 | ` * E_NOTICE` |
|      - |  252 | ` * Expands 8` |
|      - |  253 | ` */` |
|      2 |  254 | `static void PH7_E_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  255 |  |
|      3 |  256 | `	ph7_value_int(pVal,8);` |
|      1 |  257 | `	SXUNUSED(pUserData);` |
|      3 |  258 |  |
|      - |  259 | `/*` |
|      - |  260 | ` * E_CORE_ERROR` |
|      - |  261 | ` * Expands 16` |
|      - |  262 | ` */` |
|      2 |  263 | `static void PH7_E_CORE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  264 |  |
|      3 |  265 | `	ph7_value_int(pVal,16);` |
|      1 |  266 | `	SXUNUSED(pUserData);` |
|      3 |  267 |  |
|      - |  268 | `/*` |
|      - |  269 | ` * E_CORE_WARNING` |
|      - |  270 | ` * Expands 32` |
|      - |  271 | ` */` |
|      2 |  272 | `static void PH7_E_CORE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  273 |  |
|      3 |  274 | `	ph7_value_int(pVal,32);` |
|      1 |  275 | `	SXUNUSED(pUserData);` |
|      3 |  276 |  |
|      - |  277 | `/*` |
|      - |  278 | ` * E_COMPILE_ERROR` |
|      - |  279 | ` * Expands 64` |
|      - |  280 | ` */` |
|      2 |  281 | `static void PH7_E_COMPILE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  282 |  |
|      3 |  283 | `	ph7_value_int(pVal,64);` |
|      1 |  284 | `	SXUNUSED(pUserData);` |
|      3 |  285 |  |
|      - |  286 | `/*` |
|      - |  287 | ` * E_COMPILE_WARNING` |
|      - |  288 | ` * Expands 128` |
|      - |  289 | ` */` |
|      2 |  290 | `static void PH7_E_COMPILE_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  291 |  |
|      3 |  292 | `	ph7_value_int(pVal,128);` |
|      1 |  293 | `	SXUNUSED(pUserData);` |
|      3 |  294 |  |
|      - |  295 | `/*` |
|      - |  296 | ` * E_USER_ERROR` |
|      - |  297 | ` * Expands 256` |
|      - |  298 | ` */` |
|      4 |  299 | `static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  300 |  |
|      6 |  301 | `	ph7_value_int(pVal,256);` |
|      2 |  302 | `	SXUNUSED(pUserData);` |
|      6 |  303 |  |
|      - |  304 | `/*` |
|      - |  305 | ` * E_USER_WARNING` |
|      - |  306 | ` * Expands 512` |
|      - |  307 | ` */` |
|      4 |  308 | `static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  309 |  |
|      6 |  310 | `	ph7_value_int(pVal,512);` |
|      2 |  311 | `	SXUNUSED(pUserData);` |
|      6 |  312 |  |
|      - |  313 | `/*` |
|      - |  314 | ` * E_USER_NOTICE` |
|      - |  315 | ` * Expands 1024` |
|      - |  316 | ` */` |
|      6 |  317 | `static void PH7_E_USER_NOTICE_Const(ph7_value *pVal,void *pUserData)` |
|      2 |  318 |  |
|      8 |  319 | `	ph7_value_int(pVal,1024);` |
|      3 |  320 | `	SXUNUSED(pUserData);` |
|      8 |  321 |  |
|      - |  322 | `/*` |
|      - |  323 | ` * E_STRICT` |
|      - |  324 | ` * Expands 2048` |
|      - |  325 | ` */` |
|      2 |  326 | `static void PH7_E_STRICT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  327 |  |
|      3 |  328 | `	ph7_value_int(pVal,2048);` |
|      1 |  329 | `	SXUNUSED(pUserData);` |
|      3 |  330 |  |
|      - |  331 | `/*` |
|      - |  332 | ` * E_RECOVERABLE_ERROR` |
|      - |  333 | ` * Expands 4096` |
|      - |  334 | ` */` |
|      2 |  335 | `static void PH7_E_RECOVERABLE_ERROR_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  336 |  |
|      3 |  337 | `	ph7_value_int(pVal,4096);` |
|      1 |  338 | `	SXUNUSED(pUserData);` |
|      3 |  339 |  |
|      - |  340 | `/*` |
|      - |  341 | ` * E_DEPRECATED` |
|      - |  342 | ` * Expands 8192` |
|      - |  343 | ` */` |
|      4 |  344 | `static void PH7_E_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  345 |  |
|      5 |  346 | `	ph7_value_int(pVal,8192);` |
|      2 |  347 | `	SXUNUSED(pUserData);` |
|      5 |  348 |  |
|      - |  349 | `/*` |
|      - |  350 | ` * E_USER_DEPRECATED` |
|      - |  351 | ` *   Expands 16384.` |
|      - |  352 | ` */` |
|      2 |  353 | `static void PH7_E_USER_DEPRECATED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  354 |  |
|      3 |  355 | `	ph7_value_int(pVal,16384);` |
|      1 |  356 | `	SXUNUSED(pUserData);` |
|      3 |  357 |  |
|      - |  358 | `/*` |
|      - |  359 | ` * E_ALL` |
|      - |  360 | ` *  Expands 32767` |
|      - |  361 | ` */` |
|      2 |  362 | `static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  363 |  |
|      3 |  364 | `	ph7_value_int(pVal,32767);` |
|      1 |  365 | `	SXUNUSED(pUserData);` |
|      3 |  366 |  |
|      - |  367 | `/*` |
|      - |  368 | ` * CASE_LOWER` |
|      - |  369 | ` *  Expands 0.` |
|      - |  370 | ` */` |
|      2 |  371 | `static void PH7_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  372 |  |
|      3 |  373 | `	ph7_value_int(pVal,0);` |
|      1 |  374 | `	SXUNUSED(pUserData);` |
|      3 |  375 |  |
|      - |  376 | `/*` |
|      - |  377 | ` * CASE_UPPER` |
|      - |  378 | ` *  Expands 1.` |
|      - |  379 | ` */` |
|      2 |  380 | `static void PH7_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  381 |  |
|      3 |  382 | `	ph7_value_int(pVal,1);` |
|      1 |  383 | `	SXUNUSED(pUserData);` |
|      3 |  384 |  |
|      - |  385 | `/*` |
|      - |  386 | ` * STR_PAD_LEFT` |
|      - |  387 | ` *  Expands 0.` |
|      - |  388 | ` */` |
|      4 |  389 | `static void PH7_STR_PAD_LEFT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  390 |  |
|      5 |  391 | `	ph7_value_int(pVal,0);` |
|      2 |  392 | `	SXUNUSED(pUserData);` |
|      5 |  393 |  |
|      - |  394 | `/*` |
|      - |  395 | ` * STR_PAD_RIGHT` |
|      - |  396 | ` *  Expands 1.` |
|      - |  397 | ` */` |
|      4 |  398 | `static void PH7_STR_PAD_RIGHT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  399 |  |
|      5 |  400 | `	ph7_value_int(pVal,1);` |
|      2 |  401 | `	SXUNUSED(pUserData);` |
|      5 |  402 |  |
|      - |  403 | `/*` |
|      - |  404 | ` * STR_PAD_BOTH` |
|      - |  405 | ` *  Expands 2.` |
|      - |  406 | ` */` |
|      2 |  407 | `static void PH7_STR_PAD_BOTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  408 |  |
|      3 |  409 | `	ph7_value_int(pVal,2);` |
|      1 |  410 | `	SXUNUSED(pUserData);` |
|      3 |  411 |  |
|      - |  412 | `/*` |
|      - |  413 | ` * COUNT_NORMAL` |
|      - |  414 | ` *  Expands 0` |
|      - |  415 | ` */` |
|      2 |  416 | `static void PH7_COUNT_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  417 |  |
|      3 |  418 | `	ph7_value_int(pVal,0);` |
|      1 |  419 | `	SXUNUSED(pUserData);` |
|      3 |  420 |  |
|      - |  421 | `/*` |
|      - |  422 | ` * COUNT_RECURSIVE` |
|      - |  423 | ` *  Expands 1.` |
|      - |  424 | ` */` |
|     22 |  425 | `static void PH7_COUNT_RECURSIVE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  426 |  |
|     23 |  427 | `	ph7_value_int(pVal,1);` |
|     11 |  428 | `	SXUNUSED(pUserData);` |
|     23 |  429 |  |
|      - |  430 | `/*` |
|      - |  431 | ` * SORT_ASC` |
|      - |  432 | ` *  Expands 1.` |
|      - |  433 | ` */` |
|      2 |  434 | `static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  435 |  |
|      3 |  436 | `	ph7_value_int(pVal,1);` |
|      1 |  437 | `	SXUNUSED(pUserData);` |
|      3 |  438 |  |
|      - |  439 | `/*` |
|      - |  440 | ` * SORT_DESC` |
|      - |  441 | ` *  Expands 2.` |
|      - |  442 | ` */` |
|      2 |  443 | `static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  444 |  |
|      3 |  445 | `	ph7_value_int(pVal,2);` |
|      1 |  446 | `	SXUNUSED(pUserData);` |
|      3 |  447 |  |
|      - |  448 | `/*` |
|      - |  449 | ` * SORT_REGULAR` |
|      - |  450 | ` *  Expands 3.` |
|      - |  451 | ` */` |
|      2 |  452 | `static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  453 |  |
|      3 |  454 | `	ph7_value_int(pVal,3);` |
|      1 |  455 | `	SXUNUSED(pUserData);` |
|      3 |  456 |  |
|      - |  457 | `/*` |
|      - |  458 | ` * SORT_NUMERIC` |
|      - |  459 | ` *  Expands 4.` |
|      - |  460 | ` */` |
|      2 |  461 | `static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  462 |  |
|      3 |  463 | `	ph7_value_int(pVal,4);` |
|      1 |  464 | `	SXUNUSED(pUserData);` |
|      3 |  465 |  |
|      - |  466 | `/*` |
|      - |  467 | ` * SORT_STRING` |
|      - |  468 | ` *  Expands 5.` |
|      - |  469 | ` */` |
|      4 |  470 | `static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  471 |  |
|      5 |  472 | `	ph7_value_int(pVal,5);` |
|      2 |  473 | `	SXUNUSED(pUserData);` |
|      5 |  474 |  |
|      - |  475 | `/*` |
|      - |  476 | ` * PHP_ROUND_HALF_UP` |
|      - |  477 | ` *  Expands 1.` |
|      - |  478 | ` */` |
|      2 |  479 | `static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  480 |  |
|      3 |  481 | `	ph7_value_int(pVal,1);` |
|      1 |  482 | `	SXUNUSED(pUserData);` |
|      3 |  483 |  |
|      - |  484 | `/*` |
|      - |  485 | ` * SPHP_ROUND_HALF_DOWN` |
|      - |  486 | ` *  Expands 2.` |
|      - |  487 | ` */` |
|      2 |  488 | `static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  489 |  |
|      3 |  490 | `	ph7_value_int(pVal,2);` |
|      1 |  491 | `	SXUNUSED(pUserData);` |
|      3 |  492 |  |
|      - |  493 | `/*` |
|      - |  494 | ` * PHP_ROUND_HALF_EVEN` |
|      - |  495 | ` *  Expands 3.` |
|      - |  496 | ` */` |
|      2 |  497 | `static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  498 |  |
|      3 |  499 | `	ph7_value_int(pVal,3);` |
|      1 |  500 | `	SXUNUSED(pUserData);` |
|      3 |  501 |  |
|      - |  502 | `/*` |
|      - |  503 | ` * PHP_ROUND_HALF_ODD` |
|      - |  504 | ` *  Expands 4.` |
|      - |  505 | ` */` |
|      2 |  506 | `static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  507 |  |
|      3 |  508 | `	ph7_value_int(pVal,4);` |
|      1 |  509 | `	SXUNUSED(pUserData);` |
|      3 |  510 |  |
|      - |  511 | `/*` |
|      - |  512 | ` * DEBUG_BACKTRACE_PROVIDE_OBJECT` |
|      - |  513 | ` *  Expand 0x01` |
|      - |  514 | ` * NOTE:` |
|      - |  515 | ` *  The expanded value must be a power of two.` |
|      - |  516 | ` */` |
|      2 |  517 | `static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  518 |  |
|      3 |  519 | `	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */` |
|      1 |  520 | `	SXUNUSED(pUserData);` |
|      3 |  521 |  |
|      - |  522 | `/*` |
|      - |  523 | ` * DEBUG_BACKTRACE_IGNORE_ARGS` |
|      - |  524 | ` *  Expand 0x02` |
|      - |  525 | ` * NOTE:` |
|      - |  526 | ` *  The expanded value must be a power of two.` |
|      - |  527 | ` */` |
|      2 |  528 | `static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  529 |  |
|      3 |  530 | `	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */` |
|      1 |  531 | `	SXUNUSED(pUserData);` |
|      3 |  532 |  |
|      - |  533 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  534 | `/*` |
|      - |  535 | ` * M_PI` |
|      - |  536 | ` *  Expand the value of pi.` |
|      - |  537 | ` */` |
|      2 |  538 | `static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  539 |  |
|      1 |  540 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  541 | `	ph7_value_double(pVal,PH7_PI);` |
|      3 |  542 |  |
|      - |  543 | `/*` |
|      - |  544 | ` * M_E` |
|      - |  545 | ` *  Expand 2.7182818284590452354` |
|      - |  546 | ` */` |
|      2 |  547 | `static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  548 |  |
|      1 |  549 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  550 | `	ph7_value_double(pVal,2.7182818284590452354);` |
|      3 |  551 |  |
|      - |  552 | `/*` |
|      - |  553 | ` * M_LOG2E` |
|      - |  554 | ` *  Expand 2.7182818284590452354` |
|      - |  555 | ` */` |
|      2 |  556 | `static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  557 |  |
|      1 |  558 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  559 | `	ph7_value_double(pVal,1.4426950408889634074);` |
|      3 |  560 |  |
|      - |  561 | `/*` |
|      - |  562 | ` * M_LOG10E` |
|      - |  563 | ` *  Expand 0.4342944819032518276` |
|      - |  564 | ` */` |
|      2 |  565 | `static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  566 |  |
|      1 |  567 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  568 | `	ph7_value_double(pVal,0.4342944819032518276);` |
|      3 |  569 |  |
|      - |  570 | `/*` |
|      - |  571 | ` * M_LN2` |
|      - |  572 | ` *  Expand 	0.69314718055994530942` |
|      - |  573 | ` */` |
|      2 |  574 | `static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  575 |  |
|      1 |  576 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  577 | `	ph7_value_double(pVal,0.69314718055994530942);` |
|      3 |  578 |  |
|      - |  579 | `/*` |
|      - |  580 | ` * M_LN10` |
|      - |  581 | ` *  Expand 	2.30258509299404568402` |
|      - |  582 | ` */` |
|      2 |  583 | `static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  584 |  |
|      1 |  585 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  586 | `	ph7_value_double(pVal,2.30258509299404568402);` |
|      3 |  587 |  |
|      - |  588 | `/*` |
|      - |  589 | ` * M_PI_2` |
|      - |  590 | ` *  Expand 	1.57079632679489661923` |
|      - |  591 | ` */` |
|      2 |  592 | `static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  593 |  |
|      1 |  594 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  595 | `	ph7_value_double(pVal,1.57079632679489661923);` |
|      3 |  596 |  |
|      - |  597 | `/*` |
|      - |  598 | ` * M_PI_4` |
|      - |  599 | ` *  Expand 	0.78539816339744830962` |
|      - |  600 | ` */` |
|      2 |  601 | `static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  602 |  |
|      1 |  603 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  604 | `	ph7_value_double(pVal,0.78539816339744830962);` |
|      3 |  605 |  |
|      - |  606 | `/*` |
|      - |  607 | ` * M_1_PI` |
|      - |  608 | ` *  Expand 	0.31830988618379067154` |
|      - |  609 | ` */` |
|      2 |  610 | `static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  611 |  |
|      1 |  612 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  613 | `	ph7_value_double(pVal,0.31830988618379067154);` |
|      3 |  614 |  |
|      - |  615 | `/*` |
|      - |  616 | ` * M_2_PI` |
|      - |  617 | ` *  Expand 0.63661977236758134308` |
|      - |  618 | ` */` |
|      4 |  619 | `static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  620 |  |
|      2 |  621 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 |  622 | `	ph7_value_double(pVal,0.63661977236758134308);` |
|      5 |  623 |  |
|      - |  624 | `/*` |
|      - |  625 | ` * M_SQRTPI` |
|      - |  626 | ` *  Expand 1.77245385090551602729` |
|      - |  627 | ` */` |
|      2 |  628 | `static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  629 |  |
|      1 |  630 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  631 | `	ph7_value_double(pVal,1.77245385090551602729);` |
|      3 |  632 |  |
|      - |  633 | `/*` |
|      - |  634 | ` * M_2_SQRTPI` |
|      - |  635 | ` *  Expand 	1.12837916709551257390` |
|      - |  636 | ` */` |
|      2 |  637 | `static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  638 |  |
|      1 |  639 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  640 | `	ph7_value_double(pVal,1.12837916709551257390);` |
|      3 |  641 |  |
|      - |  642 | `/*` |
|      - |  643 | ` * M_SQRT2` |
|      - |  644 | ` *  Expand 	1.41421356237309504880` |
|      - |  645 | ` */` |
|      2 |  646 | `static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  647 |  |
|      1 |  648 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  649 | `	ph7_value_double(pVal,1.41421356237309504880);` |
|      3 |  650 |  |
|      - |  651 | `/*` |
|      - |  652 | ` * M_SQRT3` |
|      - |  653 | ` *  Expand 	1.73205080756887729352` |
|      - |  654 | ` */` |
|      2 |  655 | `static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  656 |  |
|      1 |  657 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  658 | `	ph7_value_double(pVal,1.73205080756887729352);` |
|      3 |  659 |  |
|      - |  660 | `/*` |
|      - |  661 | ` * M_SQRT1_2` |
|      - |  662 | ` *  Expand 	0.70710678118654752440` |
|      - |  663 | ` */` |
|      2 |  664 | `static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  665 |  |
|      1 |  666 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  667 | `	ph7_value_double(pVal,0.70710678118654752440);` |
|      3 |  668 |  |
|      - |  669 | `/*` |
|      - |  670 | ` * M_LNPI` |
|      - |  671 | ` *  Expand 	1.14472988584940017414` |
|      - |  672 | ` */` |
|      2 |  673 | `static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  674 |  |
|      1 |  675 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  676 | `	ph7_value_double(pVal,1.14472988584940017414);` |
|      3 |  677 |  |
|      - |  678 | `/*` |
|      - |  679 | ` * M_EULER` |
|      - |  680 | ` *  Expand  0.57721566490153286061` |
|      - |  681 | ` */` |
|      2 |  682 | `static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  683 |  |
|      1 |  684 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  685 | `	ph7_value_double(pVal,0.57721566490153286061);` |
|      3 |  686 |  |
|      - |  687 | `#endif /* PH7_DISABLE_BUILTIN_MATH */` |
|      - |  688 | `/*` |
|      - |  689 | ` * DATE_ATOM` |
|      - |  690 | ` *  Expand Atom (example: 2005-08-15T15:52:01+00:00)` |
|      - |  691 | ` */` |
|      2 |  692 | `static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  693 |  |
|      1 |  694 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  695 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  696 |  |
|      - |  697 | `/*` |
|      - |  698 | ` * DATE_COOKIE` |
|      - |  699 | ` *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  700 | ` */` |
|      2 |  701 | `static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  702 |  |
|      1 |  703 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  704 | `	ph7_value_string(pVal,"l, d-M-Y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  705 |  |
|      - |  706 | `/*` |
|      - |  707 | ` * DATE_ISO8601` |
|      - |  708 | ` *  ISO-8601 (example: 2005-08-15T15:52:01+0000)` |
|      - |  709 | ` */` |
|      2 |  710 | `static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  711 |  |
|      1 |  712 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  713 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);` |
|      3 |  714 |  |
|      - |  715 | `/*` |
|      - |  716 | ` * DATE_RFC822` |
|      - |  717 | ` *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000)` |
|      - |  718 | ` */` |
|      2 |  719 | `static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  720 |  |
|      1 |  721 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  722 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  723 |  |
|      - |  724 | `/*` |
|      - |  725 | ` * DATE_RFC850` |
|      - |  726 | ` *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC)` |
|      - |  727 | ` */` |
|      2 |  728 | `static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  729 |  |
|      1 |  730 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  731 | `	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);` |
|      3 |  732 |  |
|      - |  733 | `/*` |
|      - |  734 | ` * DATE_RFC1036` |
|      - |  735 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  736 | ` */` |
|      2 |  737 | `static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  738 |  |
|      1 |  739 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  740 | `	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  741 |  |
|      - |  742 | `/*` |
|      - |  743 | ` * DATE_RFC1123` |
|      - |  744 | ` *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  745 | ` */` |
|      2 |  746 | `static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  747 |  |
|      1 |  748 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  749 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  750 |  |
|      - |  751 | `/*` |
|      - |  752 | ` * DATE_RFC2822` |
|      - |  753 | ` *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  754 | ` */` |
|      2 |  755 | `static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  756 |  |
|      1 |  757 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  758 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  759 |  |
|      - |  760 | `/*` |
|      - |  761 | ` * DATE_RSS` |
|      - |  762 | ` *  RSS (Mon, 15 Aug 2005 15:52:01 +0000)` |
|      - |  763 | ` */` |
|      2 |  764 | `static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  765 |  |
|      1 |  766 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  767 | `	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);` |
|      3 |  768 |  |
|      - |  769 | `/*` |
|      - |  770 | ` * DATE_W3C` |
|      - |  771 | ` *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00)` |
|      - |  772 | ` */` |
|      2 |  773 | `static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  774 |  |
|      1 |  775 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  776 | `	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);` |
|      3 |  777 |  |
|      - |  778 | `/*` |
|      - |  779 | ` * ENT_COMPAT` |
|      - |  780 | ` *  Expand 0x01 (Must be a power of two)` |
|      - |  781 | ` */` |
|      2 |  782 | `static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  783 |  |
|      1 |  784 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  785 | `	ph7_value_int(pVal,0x01);` |
|      3 |  786 |  |
|      - |  787 | `/*` |
|      - |  788 | ` * ENT_QUOTES` |
|      - |  789 | ` *  Expand 0x02 (Must be a power of two)` |
|      - |  790 | ` */` |
|     16 |  791 | `static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  792 |  |
|      8 |  793 | `	SXUNUSED(pUserData); /* cc warning */` |
|     17 |  794 | `	ph7_value_int(pVal,0x02);` |
|     17 |  795 |  |
|      - |  796 | `/*` |
|      - |  797 | ` * ENT_NOQUOTES` |
|      - |  798 | ` *  Expand 0x04 (Must be a power of two)` |
|      - |  799 | ` */` |
|     12 |  800 | `static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  801 |  |
|      6 |  802 | `	SXUNUSED(pUserData); /* cc warning */` |
|     13 |  803 | `	ph7_value_int(pVal,0x04);` |
|     13 |  804 |  |
|      - |  805 | `/*` |
|      - |  806 | ` * ENT_IGNORE` |
|      - |  807 | ` *  Expand 0x08 (Must be a power of two)` |
|      - |  808 | ` */` |
|      2 |  809 | `static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  810 |  |
|      1 |  811 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  812 | `	ph7_value_int(pVal,0x08);` |
|      3 |  813 |  |
|      - |  814 | `/*` |
|      - |  815 | ` * ENT_SUBSTITUTE` |
|      - |  816 | ` *  Expand 0x10 (Must be a power of two)` |
|      - |  817 | ` */` |
|      2 |  818 | `static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  819 |  |
|      1 |  820 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  821 | `	ph7_value_int(pVal,0x10);` |
|      3 |  822 |  |
|      - |  823 | `/*` |
|      - |  824 | ` * ENT_DISALLOWED` |
|      - |  825 | ` *  Expand 0x20 (Must be a power of two)` |
|      - |  826 | ` */` |
|      2 |  827 | `static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  828 |  |
|      1 |  829 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  830 | `	ph7_value_int(pVal,0x20);` |
|      3 |  831 |  |
|      - |  832 | `/*` |
|      - |  833 | ` * ENT_HTML401` |
|      - |  834 | ` *  Expand 0x40 (Must be a power of two)` |
|      - |  835 | ` */` |
|      2 |  836 | `static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  837 |  |
|      1 |  838 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  839 | `	ph7_value_int(pVal,0x40);` |
|      3 |  840 |  |
|      - |  841 | `/*` |
|      - |  842 | ` * ENT_XML1` |
|      - |  843 | ` *  Expand 0x80 (Must be a power of two)` |
|      - |  844 | ` */` |
|      2 |  845 | `static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  846 |  |
|      1 |  847 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  848 | `	ph7_value_int(pVal,0x80);` |
|      3 |  849 |  |
|      - |  850 | `/*` |
|      - |  851 | ` * ENT_XHTML` |
|      - |  852 | ` *  Expand 0x100 (Must be a power of two)` |
|      - |  853 | ` */` |
|      2 |  854 | `static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  855 |  |
|      1 |  856 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  857 | `	ph7_value_int(pVal,0x100);` |
|      3 |  858 |  |
|      - |  859 | `/*` |
|      - |  860 | ` * ENT_HTML5` |
|      - |  861 | ` *  Expand 0x200 (Must be a power of two)` |
|      - |  862 | ` */` |
|      2 |  863 | `static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  864 |  |
|      1 |  865 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  866 | `	ph7_value_int(pVal,0x200);` |
|      3 |  867 |  |
|      - |  868 | `/*` |
|      - |  869 | ` * ISO-8859-1` |
|      - |  870 | ` * ISO_8859_1` |
|      - |  871 | ` *   Expand 1` |
|      - |  872 | ` */` |
|      2 |  873 | `static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  874 |  |
|      1 |  875 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  876 | `	ph7_value_int(pVal,1);` |
|      3 |  877 |  |
|      - |  878 | `/*` |
|      - |  879 | ` * UTF-8` |
|      - |  880 | ` * UTF8` |
|      - |  881 | ` *  Expand 2` |
|      - |  882 | ` */` |
|      2 |  883 | `static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  884 |  |
|      1 |  885 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  886 | `	ph7_value_int(pVal,1);` |
|      3 |  887 |  |
|      - |  888 | `/*` |
|      - |  889 | ` * HTML_ENTITIES` |
|      - |  890 | ` *  Expand 1` |
|      - |  891 | ` */` |
|      2 |  892 | `static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  893 |  |
|      1 |  894 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  895 | `	ph7_value_int(pVal,1);` |
|      3 |  896 |  |
|      - |  897 | `/*` |
|      - |  898 | ` * HTML_SPECIALCHARS` |
|      - |  899 | ` *  Expand 2` |
|      - |  900 | ` */` |
|      2 |  901 | `static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  902 |  |
|      1 |  903 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  904 | `	ph7_value_int(pVal,2);` |
|      3 |  905 |  |
|      - |  906 | `/*` |
|      - |  907 | ` * PHP_URL_SCHEME.` |
|      - |  908 | ` * Expand 1` |
|      - |  909 | ` */` |
|      2 |  910 | `static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  911 |  |
|      1 |  912 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  913 | `	ph7_value_int(pVal,1);` |
|      3 |  914 |  |
|      - |  915 | `/*` |
|      - |  916 | ` * PHP_URL_HOST.` |
|      - |  917 | ` * Expand 2` |
|      - |  918 | ` */` |
|      2 |  919 | `static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  920 |  |
|      1 |  921 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  922 | `	ph7_value_int(pVal,2);` |
|      3 |  923 |  |
|      - |  924 | `/*` |
|      - |  925 | ` * PHP_URL_PORT.` |
|      - |  926 | ` * Expand 3` |
|      - |  927 | ` */` |
|      2 |  928 | `static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  929 |  |
|      1 |  930 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  931 | `	ph7_value_int(pVal,3);` |
|      3 |  932 |  |
|      - |  933 | `/*` |
|      - |  934 | ` * PHP_URL_USER.` |
|      - |  935 | ` * Expand 4` |
|      - |  936 | ` */` |
|      2 |  937 | `static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  938 |  |
|      1 |  939 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  940 | `	ph7_value_int(pVal,4);` |
|      3 |  941 |  |
|      - |  942 | `/*` |
|      - |  943 | ` * PHP_URL_PASS.` |
|      - |  944 | ` * Expand 5` |
|      - |  945 | ` */` |
|      2 |  946 | `static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  947 |  |
|      1 |  948 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  949 | `	ph7_value_int(pVal,5);` |
|      3 |  950 |  |
|      - |  951 | `/*` |
|      - |  952 | ` * PHP_URL_PATH.` |
|      - |  953 | ` * Expand 6` |
|      - |  954 | ` */` |
|      2 |  955 | `static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  956 |  |
|      1 |  957 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  958 | `	ph7_value_int(pVal,6);` |
|      3 |  959 |  |
|      - |  960 | `/*` |
|      - |  961 | ` * PHP_URL_QUERY.` |
|      - |  962 | ` * Expand 7` |
|      - |  963 | ` */` |
|      2 |  964 | `static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  965 |  |
|      1 |  966 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  967 | `	ph7_value_int(pVal,7);` |
|      3 |  968 |  |
|      - |  969 | `/*` |
|      - |  970 | ` * PHP_URL_FRAGMENT.` |
|      - |  971 | ` * Expand 8` |
|      - |  972 | ` */` |
|      2 |  973 | `static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  974 |  |
|      1 |  975 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  976 | `	ph7_value_int(pVal,8);` |
|      3 |  977 |  |
|      - |  978 | `/*` |
|      - |  979 | ` * PHP_QUERY_RFC1738` |
|      - |  980 | ` * Expand 1` |
|      - |  981 | ` */` |
|      2 |  982 | `static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  983 |  |
|      1 |  984 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  985 | `	ph7_value_int(pVal,1);` |
|      3 |  986 |  |
|      - |  987 | `/*` |
|      - |  988 | ` * PHP_QUERY_RFC3986` |
|      - |  989 | ` * Expand 1` |
|      - |  990 | ` */` |
|      2 |  991 | `static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)` |
|      1 |  992 |  |
|      1 |  993 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 |  994 | `	ph7_value_int(pVal,2);` |
|      3 |  995 |  |
|      - |  996 | `/*` |
|      - |  997 | ` * FNM_NOESCAPE` |
|      - |  998 | ` *  Expand 0x01 (Must be a power of two)` |
|      - |  999 | ` */` |
|      2 | 1000 | `static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1001 |  |
|      1 | 1002 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1003 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1004 |  |
|      - | 1005 | `/*` |
|      - | 1006 | ` * FNM_PATHNAME` |
|      - | 1007 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1008 | ` */` |
|      2 | 1009 | `static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1010 |  |
|      1 | 1011 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1012 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1013 |  |
|      - | 1014 | `/*` |
|      - | 1015 | ` * FNM_PERIOD` |
|      - | 1016 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1017 | ` */` |
|      6 | 1018 | `static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1019 |  |
|      3 | 1020 | `	SXUNUSED(pUserData); /* cc warning */` |
|      7 | 1021 | `	ph7_value_int(pVal,0x04);` |
|      7 | 1022 |  |
|      - | 1023 | `/*` |
|      - | 1024 | ` * FNM_CASEFOLD` |
|      - | 1025 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1026 | ` */` |
|      4 | 1027 | `static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1028 |  |
|      2 | 1029 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1030 | `	ph7_value_int(pVal,0x08);` |
|      5 | 1031 |  |
|      - | 1032 | `/*` |
|      - | 1033 | ` * PATHINFO_DIRNAME` |
|      - | 1034 | ` *  Expand 1.` |
|      - | 1035 | ` */` |
|      4 | 1036 | `static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1037 |  |
|      2 | 1038 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1039 | `	ph7_value_int(pVal,1);` |
|      5 | 1040 |  |
|      - | 1041 | `/*` |
|      - | 1042 | ` * PATHINFO_BASENAME` |
|      - | 1043 | ` *  Expand 2.` |
|      - | 1044 | ` */` |
|      4 | 1045 | `static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1046 |  |
|      2 | 1047 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1048 | `	ph7_value_int(pVal,2);` |
|      5 | 1049 |  |
|      - | 1050 | `/*` |
|      - | 1051 | ` * PATHINFO_EXTENSION` |
|      - | 1052 | ` *  Expand 3.` |
|      - | 1053 | ` */` |
|   3080 | 1054 | `static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1055 |  |
|   1540 | 1056 | `	SXUNUSED(pUserData); /* cc warning */` |
|   3082 | 1057 | `	ph7_value_int(pVal,3);` |
|   3082 | 1058 |  |
|      - | 1059 | `/*` |
|      - | 1060 | ` * PATHINFO_FILENAME` |
|      - | 1061 | ` *  Expand 4.` |
|      - | 1062 | ` */` |
|   3076 | 1063 | `static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1064 |  |
|   1538 | 1065 | `	SXUNUSED(pUserData); /* cc warning */` |
|   3078 | 1066 | `	ph7_value_int(pVal,4);` |
|   3078 | 1067 |  |
|      - | 1068 | `/*` |
|      - | 1069 | ` * ASSERT_ACTIVE.` |
|      - | 1070 | ` *  Expand the value of PH7_ASSERT_ACTIVE defined in ph7Int.h` |
|      - | 1071 | ` */` |
|      2 | 1072 | `static void PH7_ASSERT_ACTIVE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1073 |  |
|      1 | 1074 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1075 | `	ph7_value_int(pVal,PH7_ASSERT_DISABLE);` |
|      3 | 1076 |  |
|      - | 1077 | `/*` |
|      - | 1078 | ` * ASSERT_WARNING.` |
|      - | 1079 | ` *  Expand the value of PH7_ASSERT_WARNING defined in ph7Int.h` |
|      - | 1080 | ` */` |
|      2 | 1081 | `static void PH7_ASSERT_WARNING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1082 |  |
|      1 | 1083 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1084 | `	ph7_value_int(pVal,PH7_ASSERT_WARNING);` |
|      3 | 1085 |  |
|      - | 1086 | `/*` |
|      - | 1087 | ` * ASSERT_BAIL.` |
|      - | 1088 | ` *  Expand the value of PH7_ASSERT_BAIL defined in ph7Int.h` |
|      - | 1089 | ` */` |
|      2 | 1090 | `static void PH7_ASSERT_BAIL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1091 |  |
|      1 | 1092 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1093 | `	ph7_value_int(pVal,PH7_ASSERT_BAIL);` |
|      3 | 1094 |  |
|      - | 1095 | `/*` |
|      - | 1096 | ` * ASSERT_QUIET_EVAL.` |
|      - | 1097 | ` *  Expand the value of PH7_ASSERT_QUIET_EVAL defined in ph7Int.h` |
|      - | 1098 | ` */` |
|      2 | 1099 | `static void PH7_ASSERT_QUIET_EVAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1100 |  |
|      1 | 1101 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1102 | `	ph7_value_int(pVal,PH7_ASSERT_QUIET_EVAL);` |
|      3 | 1103 |  |
|      - | 1104 | `/*` |
|      - | 1105 | ` * ASSERT_CALLBACK.` |
|      - | 1106 | ` *  Expand the value of PH7_ASSERT_CALLBACK defined in ph7Int.h` |
|      - | 1107 | ` */` |
|      2 | 1108 | `static void PH7_ASSERT_CALLBACK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1109 |  |
|      1 | 1110 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1111 | `	ph7_value_int(pVal,PH7_ASSERT_CALLBACK);` |
|      3 | 1112 |  |
|      - | 1113 | `/*` |
|      - | 1114 | ` * SEEK_SET.` |
|      - | 1115 | ` *  Expand 0` |
|      - | 1116 | ` */` |
|      2 | 1117 | `static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1118 |  |
|      1 | 1119 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1120 | `	ph7_value_int(pVal,0);` |
|      3 | 1121 |  |
|      - | 1122 | `/*` |
|      - | 1123 | ` * SEEK_CUR.` |
|      - | 1124 | ` *  Expand 1` |
|      - | 1125 | ` */` |
|      2 | 1126 | `static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1127 |  |
|      1 | 1128 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1129 | `	ph7_value_int(pVal,1);` |
|      3 | 1130 |  |
|      - | 1131 | `/*` |
|      - | 1132 | ` * SEEK_END.` |
|      - | 1133 | ` *  Expand 2` |
|      - | 1134 | ` */` |
|      2 | 1135 | `static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1136 |  |
|      1 | 1137 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1138 | `	ph7_value_int(pVal,2);` |
|      3 | 1139 |  |
|      - | 1140 | `/*` |
|      - | 1141 | ` * LOCK_SH.` |
|      - | 1142 | ` *  Expand 2` |
|      - | 1143 | ` */` |
|      2 | 1144 | `static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1145 |  |
|      1 | 1146 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1147 | `	ph7_value_int(pVal,1);` |
|      3 | 1148 |  |
|      - | 1149 | `/*` |
|      - | 1150 | ` * LOCK_NB.` |
|      - | 1151 | ` *  Expand 5` |
|      - | 1152 | ` */` |
|      2 | 1153 | `static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1154 |  |
|      1 | 1155 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1156 | `	ph7_value_int(pVal,5);` |
|      3 | 1157 |  |
|      - | 1158 | `/*` |
|      - | 1159 | ` * LOCK_EX.` |
|      - | 1160 | ` *  Expand 0x01 (MUST BE A POWER OF TWO)` |
|      - | 1161 | ` */` |
|      4 | 1162 | `static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1163 |  |
|      2 | 1164 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1165 | `	ph7_value_int(pVal,0x01);` |
|      5 | 1166 |  |
|      - | 1167 | `/*` |
|      - | 1168 | ` * LOCK_UN.` |
|      - | 1169 | ` *  Expand 0` |
|      - | 1170 | ` */` |
|      4 | 1171 | `static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1172 |  |
|      2 | 1173 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1174 | `	ph7_value_int(pVal,0);` |
|      5 | 1175 |  |
|      - | 1176 | `/*` |
|      - | 1177 | ` * FILE_USE_INCLUDE_PATH` |
|      - | 1178 | ` *  Expand 0x01 (Must be a power of two)` |
|      - | 1179 | ` */` |
|      2 | 1180 | `static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1181 |  |
|      1 | 1182 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1183 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1184 |  |
|      - | 1185 | `/*` |
|      - | 1186 | ` * FILE_IGNORE_NEW_LINES` |
|      - | 1187 | ` *  Expand 0x02 (Must be a power of two)` |
|      - | 1188 | ` */` |
|      2 | 1189 | `static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1190 |  |
|      1 | 1191 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1192 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1193 |  |
|      - | 1194 | `/*` |
|      - | 1195 | ` * FILE_SKIP_EMPTY_LINES` |
|      - | 1196 | ` *  Expand 0x04 (Must be a power of two)` |
|      - | 1197 | ` */` |
|      2 | 1198 | `static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1199 |  |
|      1 | 1200 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1201 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1202 |  |
|      - | 1203 | `/*` |
|      - | 1204 | ` * FILE_APPEND` |
|      - | 1205 | ` *  Expand 0x08 (Must be a power of two)` |
|      - | 1206 | ` */` |
|      2 | 1207 | `static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1208 |  |
|      1 | 1209 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1210 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1211 |  |
|      - | 1212 | `/*` |
|      - | 1213 | ` * SCANDIR_SORT_ASCENDING` |
|      - | 1214 | ` *  Expand 0` |
|      - | 1215 | ` */` |
|   1498 | 1216 | `static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1217 |  |
|    749 | 1218 | `	SXUNUSED(pUserData); /* cc warning */` |
|   1500 | 1219 | `	ph7_value_int(pVal,0);` |
|   1500 | 1220 |  |
|      - | 1221 | `/*` |
|      - | 1222 | ` * SCANDIR_SORT_DESCENDING` |
|      - | 1223 | ` *  Expand 1` |
|      - | 1224 | ` */` |
|    750 | 1225 | `static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)` |
|      2 | 1226 |  |
|    375 | 1227 | `	SXUNUSED(pUserData); /* cc warning */` |
|    752 | 1228 | `	ph7_value_int(pVal,1);` |
|    752 | 1229 |  |
|      - | 1230 | `/*` |
|      - | 1231 | ` * SCANDIR_SORT_NONE` |
|      - | 1232 | ` *  Expand 2` |
|      - | 1233 | ` */` |
|      2 | 1234 | `static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1235 |  |
|      1 | 1236 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1237 | `	ph7_value_int(pVal,2);` |
|      3 | 1238 |  |
|      - | 1239 | `/*` |
|      - | 1240 | ` * GLOB_MARK` |
|      - | 1241 | ` *  Expand 0x01 (must be a power of two)` |
|      - | 1242 | ` */` |
|      2 | 1243 | `static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1244 |  |
|      1 | 1245 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1246 | `	ph7_value_int(pVal,0x01);` |
|      3 | 1247 |  |
|      - | 1248 | `/*` |
|      - | 1249 | ` * GLOB_NOSORT` |
|      - | 1250 | ` *  Expand 0x02 (must be a power of two)` |
|      - | 1251 | ` */` |
|      2 | 1252 | `static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1253 |  |
|      1 | 1254 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1255 | `	ph7_value_int(pVal,0x02);` |
|      3 | 1256 |  |
|      - | 1257 | `/*` |
|      - | 1258 | ` * GLOB_NOCHECK` |
|      - | 1259 | ` *  Expand 0x04 (must be a power of two)` |
|      - | 1260 | ` */` |
|      2 | 1261 | `static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1262 |  |
|      1 | 1263 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1264 | `	ph7_value_int(pVal,0x04);` |
|      3 | 1265 |  |
|      - | 1266 | `/*` |
|      - | 1267 | ` * GLOB_NOESCAPE` |
|      - | 1268 | ` *  Expand 0x08 (must be a power of two)` |
|      - | 1269 | ` */` |
|      2 | 1270 | `static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1271 |  |
|      1 | 1272 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1273 | `	ph7_value_int(pVal,0x08);` |
|      3 | 1274 |  |
|      - | 1275 | `/*` |
|      - | 1276 | ` * GLOB_BRACE` |
|      - | 1277 | ` *  Expand 0x10 (must be a power of two)` |
|      - | 1278 | ` */` |
|      2 | 1279 | `static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1280 |  |
|      1 | 1281 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1282 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1283 |  |
|      - | 1284 | `/*` |
|      - | 1285 | ` * GLOB_ONLYDIR` |
|      - | 1286 | ` *  Expand 0x20 (must be a power of two)` |
|      - | 1287 | ` */` |
|      2 | 1288 | `static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1289 |  |
|      1 | 1290 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1291 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1292 |  |
|      - | 1293 | `/*` |
|      - | 1294 | ` * GLOB_ERR` |
|      - | 1295 | ` *  Expand 0x40 (must be a power of two)` |
|      - | 1296 | ` */` |
|      2 | 1297 | `static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1298 |  |
|      1 | 1299 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1300 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1301 |  |
|      - | 1302 | `/*` |
|      - | 1303 | ` * STDIN` |
|      - | 1304 | ` *  Expand the STDIN handle as a resource.` |
|      - | 1305 | ` */` |
|      2 | 1306 | `static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1307 |  |
|      3 | 1308 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1309 | `	void *pResource;` |
|      3 | 1310 | `	pResource = PH7_ExportStdin(pVm);` |
|      3 | 1311 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1312 |  |
|      - | 1313 | `/*` |
|      - | 1314 | ` * STDOUT` |
|      - | 1315 | ` *   Expand the STDOUT handle as a resource.` |
|      - | 1316 | ` */` |
|      2 | 1317 | `static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1318 |  |
|      3 | 1319 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1320 | `	void *pResource;` |
|      3 | 1321 | `	pResource = PH7_ExportStdout(pVm);` |
|      3 | 1322 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1323 |  |
|      - | 1324 | `/*` |
|      - | 1325 | ` * STDERR` |
|      - | 1326 | ` *  Expand the STDERR handle as a resource.` |
|      - | 1327 | ` */` |
|      2 | 1328 | `static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1329 |  |
|      3 | 1330 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1331 | `	void *pResource;` |
|      3 | 1332 | `	pResource = PH7_ExportStderr(pVm);` |
|      3 | 1333 | `	ph7_value_resource(pVal,pResource);` |
|      3 | 1334 |  |
|      - | 1335 | `/*` |
|      - | 1336 | ` * INI_SCANNER_NORMAL` |
|      - | 1337 | ` *   Expand 1` |
|      - | 1338 | ` */` |
|      2 | 1339 | `static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1340 |  |
|      1 | 1341 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1342 | `	ph7_value_int(pVal,1);` |
|      3 | 1343 |  |
|      - | 1344 | `/*` |
|      - | 1345 | ` * INI_SCANNER_RAW` |
|      - | 1346 | ` *   Expand 2` |
|      - | 1347 | ` */` |
|      2 | 1348 | `static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1349 |  |
|      1 | 1350 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1351 | `	ph7_value_int(pVal,2);` |
|      3 | 1352 |  |
|      - | 1353 | `/*` |
|      - | 1354 | ` * EXTR_OVERWRITE` |
|      - | 1355 | ` *   Expand 0x01 (Must be a power of two)` |
|      - | 1356 | ` */` |
|      2 | 1357 | `static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1358 |  |
|      1 | 1359 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1360 | `	ph7_value_int(pVal,0x1);` |
|      3 | 1361 |  |
|      - | 1362 | `/*` |
|      - | 1363 | ` * EXTR_SKIP` |
|      - | 1364 | ` *   Expand 0x02 (Must be a power of two)` |
|      - | 1365 | ` */` |
|      2 | 1366 | `static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1367 |  |
|      1 | 1368 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1369 | `	ph7_value_int(pVal,0x2);` |
|      3 | 1370 |  |
|      - | 1371 | `/*` |
|      - | 1372 | ` * EXTR_PREFIX_SAME` |
|      - | 1373 | ` *   Expand 0x04 (Must be a power of two)` |
|      - | 1374 | ` */` |
|      2 | 1375 | `static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1376 |  |
|      1 | 1377 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1378 | `	ph7_value_int(pVal,0x4);` |
|      3 | 1379 |  |
|      - | 1380 | `/*` |
|      - | 1381 | ` * EXTR_PREFIX_ALL` |
|      - | 1382 | ` *   Expand 0x08 (Must be a power of two)` |
|      - | 1383 | ` */` |
|      2 | 1384 | `static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1385 |  |
|      1 | 1386 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1387 | `	ph7_value_int(pVal,0x8);` |
|      3 | 1388 |  |
|      - | 1389 | `/*` |
|      - | 1390 | ` * EXTR_PREFIX_INVALID` |
|      - | 1391 | ` *   Expand 0x10 (Must be a power of two)` |
|      - | 1392 | ` */` |
|      2 | 1393 | `static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1394 |  |
|      1 | 1395 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1396 | `	ph7_value_int(pVal,0x10);` |
|      3 | 1397 |  |
|      - | 1398 | `/*` |
|      - | 1399 | ` * EXTR_IF_EXISTS` |
|      - | 1400 | ` *   Expand 0x20 (Must be a power of two)` |
|      - | 1401 | ` */` |
|      2 | 1402 | `static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1403 |  |
|      1 | 1404 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1405 | `	ph7_value_int(pVal,0x20);` |
|      3 | 1406 |  |
|      - | 1407 | `/*` |
|      - | 1408 | ` * EXTR_PREFIX_IF_EXISTS` |
|      - | 1409 | ` *   Expand 0x40 (Must be a power of two)` |
|      - | 1410 | ` */` |
|      2 | 1411 | `static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1412 |  |
|      1 | 1413 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1414 | `	ph7_value_int(pVal,0x40);` |
|      3 | 1415 |  |
|      - | 1416 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1417 | `/*` |
|      - | 1418 | ` * XML_ERROR_NONE` |
|      - | 1419 | ` *   Expand the value of SXML_ERROR_NO_MEMORY defined in ph7Int.h` |
|      - | 1420 | ` */` |
|      2 | 1421 | `static void PH7_XML_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1422 |  |
|      1 | 1423 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1424 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1425 |  |
|      - | 1426 | `/*` |
|      - | 1427 | ` * XML_ERROR_NO_MEMORY` |
|      - | 1428 | ` *   Expand the value of SXML_ERROR_NONE defined in ph7Int.h` |
|      - | 1429 | ` */` |
|      2 | 1430 | `static void PH7_XML_ERROR_NO_MEMORY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1431 |  |
|      1 | 1432 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1433 | `	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);` |
|      3 | 1434 |  |
|      - | 1435 | `/*` |
|      - | 1436 | ` * XML_ERROR_SYNTAX` |
|      - | 1437 | ` *   Expand the value of SXML_ERROR_SYNTAX defined in ph7Int.h` |
|      - | 1438 | ` */` |
|      2 | 1439 | `static void PH7_XML_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1440 |  |
|      1 | 1441 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1442 | `	ph7_value_int(pVal,SXML_ERROR_SYNTAX);` |
|      3 | 1443 |  |
|      - | 1444 | `/*` |
|      - | 1445 | ` * XML_ERROR_NO_ELEMENTS` |
|      - | 1446 | ` *   Expand the value of SXML_ERROR_NO_ELEMENTS defined in ph7Int.h` |
|      - | 1447 | ` */` |
|      2 | 1448 | `static void PH7_XML_ERROR_NO_ELEMENTS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1449 |  |
|      1 | 1450 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1451 | `	ph7_value_int(pVal,SXML_ERROR_NO_ELEMENTS);` |
|      3 | 1452 |  |
|      - | 1453 | `/*` |
|      - | 1454 | ` * XML_ERROR_INVALID_TOKEN` |
|      - | 1455 | ` *   Expand the value of SXML_ERROR_INVALID_TOKEN defined in ph7Int.h` |
|      - | 1456 | ` */` |
|      2 | 1457 | `static void PH7_XML_ERROR_INVALID_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1458 |  |
|      1 | 1459 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1460 | `	ph7_value_int(pVal,SXML_ERROR_INVALID_TOKEN);` |
|      3 | 1461 |  |
|      - | 1462 | `/*` |
|      - | 1463 | ` * XML_ERROR_UNCLOSED_TOKEN` |
|      - | 1464 | ` *   Expand the value of SXML_ERROR_UNCLOSED_TOKEN defined in ph7Int.h` |
|      - | 1465 | ` */` |
|      2 | 1466 | `static void PH7_XML_ERROR_UNCLOSED_TOKEN_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1467 |  |
|      1 | 1468 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1469 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_TOKEN);` |
|      3 | 1470 |  |
|      - | 1471 | `/*` |
|      - | 1472 | ` * XML_ERROR_PARTIAL_CHAR` |
|      - | 1473 | ` *   Expand the value of SXML_ERROR_PARTIAL_CHAR defined in ph7Int.h` |
|      - | 1474 | ` */` |
|      2 | 1475 | `static void PH7_XML_ERROR_PARTIAL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1476 |  |
|      1 | 1477 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1478 | `	ph7_value_int(pVal,SXML_ERROR_PARTIAL_CHAR);` |
|      3 | 1479 |  |
|      - | 1480 | `/*` |
|      - | 1481 | ` * XML_ERROR_TAG_MISMATCH` |
|      - | 1482 | ` *   Expand the value of SXML_ERROR_TAG_MISMATCH defined in ph7Int.h` |
|      - | 1483 | ` */` |
|      2 | 1484 | `static void PH7_XML_ERROR_TAG_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1485 |  |
|      1 | 1486 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1487 | `	ph7_value_int(pVal,SXML_ERROR_TAG_MISMATCH);` |
|      3 | 1488 |  |
|      - | 1489 | `/*` |
|      - | 1490 | ` * XML_ERROR_DUPLICATE_ATTRIBUTE` |
|      - | 1491 | ` *   Expand the value of SXML_ERROR_DUPLICATE_ATTRIBUTE defined in ph7Int.h` |
|      - | 1492 | ` */` |
|      2 | 1493 | `static void PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1494 |  |
|      1 | 1495 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1496 | `	ph7_value_int(pVal,SXML_ERROR_DUPLICATE_ATTRIBUTE);` |
|      3 | 1497 |  |
|      - | 1498 | `/*` |
|      - | 1499 | ` * XML_ERROR_JUNK_AFTER_DOC_ELEMENT` |
|      - | 1500 | ` *   Expand the value of SXML_ERROR_JUNK_AFTER_DOC_ELEMENT defined in ph7Int.h` |
|      - | 1501 | ` */` |
|      2 | 1502 | `static void PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1503 |  |
|      1 | 1504 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1505 | `	ph7_value_int(pVal,SXML_ERROR_JUNK_AFTER_DOC_ELEMENT);` |
|      3 | 1506 |  |
|      - | 1507 | `/*` |
|      - | 1508 | ` * XML_ERROR_PARAM_ENTITY_REF` |
|      - | 1509 | ` *   Expand the value of SXML_ERROR_PARAM_ENTITY_REF defined in ph7Int.h` |
|      - | 1510 | ` */` |
|      2 | 1511 | `static void PH7_XML_ERROR_PARAM_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1512 |  |
|      1 | 1513 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1514 | `	ph7_value_int(pVal,SXML_ERROR_PARAM_ENTITY_REF);` |
|      3 | 1515 |  |
|      - | 1516 | `/*` |
|      - | 1517 | ` * XML_ERROR_UNDEFINED_ENTITY` |
|      - | 1518 | ` *   Expand the value of SXML_ERROR_UNDEFINED_ENTITY defined in ph7Int.h` |
|      - | 1519 | ` */` |
|      2 | 1520 | `static void PH7_XML_ERROR_UNDEFINED_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1521 |  |
|      1 | 1522 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1523 | `	ph7_value_int(pVal,SXML_ERROR_UNDEFINED_ENTITY);` |
|      3 | 1524 |  |
|      - | 1525 | `/*` |
|      - | 1526 | ` * XML_ERROR_RECURSIVE_ENTITY_REF` |
|      - | 1527 | ` *   Expand the value of SXML_ERROR_RECURSIVE_ENTITY_REF defined in ph7Int.h` |
|      - | 1528 | ` */` |
|      2 | 1529 | `static void PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1530 |  |
|      1 | 1531 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1532 | `	ph7_value_int(pVal,SXML_ERROR_RECURSIVE_ENTITY_REF);` |
|      3 | 1533 |  |
|      - | 1534 | `/*` |
|      - | 1535 | ` * XML_ERROR_ASYNC_ENTITY` |
|      - | 1536 | ` *   Expand the value of SXML_ERROR_ASYNC_ENTITY defined in ph7Int.h` |
|      - | 1537 | ` */` |
|      2 | 1538 | `static void PH7_XML_ERROR_ASYNC_ENTITY_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1539 |  |
|      1 | 1540 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1541 | `	ph7_value_int(pVal,SXML_ERROR_ASYNC_ENTITY);` |
|      3 | 1542 |  |
|      - | 1543 | `/*` |
|      - | 1544 | ` * XML_ERROR_BAD_CHAR_REF` |
|      - | 1545 | ` *   Expand the value of SXML_ERROR_BAD_CHAR_REF defined in ph7Int.h` |
|      - | 1546 | ` */` |
|      2 | 1547 | `static void PH7_XML_ERROR_BAD_CHAR_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1548 |  |
|      1 | 1549 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1550 | `	ph7_value_int(pVal,SXML_ERROR_BAD_CHAR_REF);` |
|      3 | 1551 |  |
|      - | 1552 | `/*` |
|      - | 1553 | ` * XML_ERROR_BINARY_ENTITY_REF` |
|      - | 1554 | ` *   Expand the value of SXML_ERROR_BINARY_ENTITY_REF defined in ph7Int.h` |
|      - | 1555 | ` */` |
|      2 | 1556 | `static void PH7_XML_ERROR_BINARY_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1557 |  |
|      1 | 1558 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1559 | `	ph7_value_int(pVal,SXML_ERROR_BINARY_ENTITY_REF);` |
|      3 | 1560 |  |
|      - | 1561 | `/*` |
|      - | 1562 | ` * XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF` |
|      - | 1563 | ` *   Expand the value of SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF defined in ph7Int.h` |
|      - | 1564 | ` */` |
|      2 | 1565 | `static void PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1566 |  |
|      1 | 1567 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1568 | `	ph7_value_int(pVal,SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF);` |
|      3 | 1569 |  |
|      - | 1570 | `/*` |
|      - | 1571 | ` * XML_ERROR_MISPLACED_XML_PI` |
|      - | 1572 | ` *   Expand the value of SXML_ERROR_MISPLACED_XML_PI defined in ph7Int.h` |
|      - | 1573 | ` */` |
|      2 | 1574 | `static void PH7_XML_ERROR_MISPLACED_XML_PI_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1575 |  |
|      1 | 1576 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1577 | `	ph7_value_int(pVal,SXML_ERROR_MISPLACED_XML_PI);` |
|      3 | 1578 |  |
|      - | 1579 | `/*` |
|      - | 1580 | ` * XML_ERROR_UNKNOWN_ENCODING` |
|      - | 1581 | ` *   Expand the value of SXML_ERROR_UNKNOWN_ENCODING defined in ph7Int.h` |
|      - | 1582 | ` */` |
|      2 | 1583 | `static void PH7_XML_ERROR_UNKNOWN_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1584 |  |
|      1 | 1585 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1586 | `	ph7_value_int(pVal,SXML_ERROR_UNKNOWN_ENCODING);` |
|      3 | 1587 |  |
|      - | 1588 | `/*` |
|      - | 1589 | ` * XML_ERROR_INCORRECT_ENCODING` |
|      - | 1590 | ` *   Expand the value of SXML_ERROR_INCORRECT_ENCODING defined in ph7Int.h` |
|      - | 1591 | ` */` |
|      2 | 1592 | `static void PH7_XML_ERROR_INCORRECT_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1593 |  |
|      1 | 1594 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1595 | `	ph7_value_int(pVal,SXML_ERROR_INCORRECT_ENCODING);` |
|      3 | 1596 |  |
|      - | 1597 | `/*` |
|      - | 1598 | ` * XML_ERROR_UNCLOSED_CDATA_SECTION` |
|      - | 1599 | ` *   Expand the value of SXML_ERROR_UNCLOSED_CDATA_SECTION defined in ph7Int.h` |
|      - | 1600 | ` */` |
|      2 | 1601 | `static void PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1602 |  |
|      1 | 1603 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1604 | `	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_CDATA_SECTION);` |
|      3 | 1605 |  |
|      - | 1606 | `/*` |
|      - | 1607 | ` * XML_ERROR_EXTERNAL_ENTITY_HANDLING` |
|      - | 1608 | ` *   Expand the value of SXML_ERROR_EXTERNAL_ENTITY_HANDLING defined in ph7Int.h` |
|      - | 1609 | ` */` |
|      2 | 1610 | `static void PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1611 |  |
|      1 | 1612 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1613 | `	ph7_value_int(pVal,SXML_ERROR_EXTERNAL_ENTITY_HANDLING);` |
|      3 | 1614 |  |
|      - | 1615 | `/*` |
|      - | 1616 | ` * XML_OPTION_CASE_FOLDING` |
|      - | 1617 | ` *   Expand the value of SXML_OPTION_CASE_FOLDING defined in ph7Int.h.` |
|      - | 1618 | ` */` |
|      2 | 1619 | `static void PH7_XML_OPTION_CASE_FOLDING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1620 |  |
|      1 | 1621 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1622 | `	ph7_value_int(pVal,SXML_OPTION_CASE_FOLDING);` |
|      3 | 1623 |  |
|      - | 1624 | `/*` |
|      - | 1625 | ` * XML_OPTION_TARGET_ENCODING` |
|      - | 1626 | ` *   Expand the value of SXML_OPTION_TARGET_ENCODING defined in ph7Int.h.` |
|      - | 1627 | ` */` |
|      4 | 1628 | `static void PH7_XML_OPTION_TARGET_ENCODING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1629 |  |
|      2 | 1630 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1631 | `	ph7_value_int(pVal,SXML_OPTION_TARGET_ENCODING);` |
|      5 | 1632 |  |
|      - | 1633 | `/*` |
|      - | 1634 | ` * XML_OPTION_SKIP_TAGSTART` |
|      - | 1635 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1636 | ` */` |
|      2 | 1637 | `static void PH7_XML_OPTION_SKIP_TAGSTART_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1638 |  |
|      1 | 1639 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1640 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_TAGSTART);` |
|      3 | 1641 |  |
|      - | 1642 | `/*` |
|      - | 1643 | ` * XML_OPTION_SKIP_WHITE` |
|      - | 1644 | ` *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.` |
|      - | 1645 | ` */` |
|      4 | 1646 | `static void PH7_XML_OPTION_SKIP_WHITE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1647 |  |
|      2 | 1648 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1649 | `	ph7_value_int(pVal,SXML_OPTION_SKIP_WHITE);` |
|      5 | 1650 |  |
|      - | 1651 | `/*` |
|      - | 1652 | ` * XML_SAX_IMPL.` |
|      - | 1653 | ` *   Expand the name of the underlying XML engine.` |
|      - | 1654 | ` */` |
|      2 | 1655 | `static void PH7_XML_SAX_IMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1656 |  |
|      1 | 1657 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1658 | `	ph7_value_string(pVal,"Symisc XML engine",(int)sizeof("Symisc XML engine")-1);` |
|      3 | 1659 |  |
|      - | 1660 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1661 | `/*` |
|      - | 1662 | ` * JSON_HEX_TAG.` |
|      - | 1663 | ` *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.` |
|      - | 1664 | ` */` |
|      2 | 1665 | `static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1666 |  |
|      1 | 1667 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1668 | `	ph7_value_int(pVal,JSON_HEX_TAG);` |
|      3 | 1669 |  |
|      - | 1670 | `/*` |
|      - | 1671 | ` * JSON_HEX_AMP.` |
|      - | 1672 | ` *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.` |
|      - | 1673 | ` */` |
|      2 | 1674 | `static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1675 |  |
|      1 | 1676 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1677 | `	ph7_value_int(pVal,JSON_HEX_AMP);` |
|      3 | 1678 |  |
|      - | 1679 | `/*` |
|      - | 1680 | ` * JSON_HEX_APOS.` |
|      - | 1681 | ` *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.` |
|      - | 1682 | ` */` |
|      2 | 1683 | `static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1684 |  |
|      1 | 1685 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1686 | `	ph7_value_int(pVal,JSON_HEX_APOS);` |
|      3 | 1687 |  |
|      - | 1688 | `/*` |
|      - | 1689 | ` * JSON_HEX_QUOT.` |
|      - | 1690 | ` *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.` |
|      - | 1691 | ` */` |
|      2 | 1692 | `static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1693 |  |
|      1 | 1694 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1695 | `	ph7_value_int(pVal,JSON_HEX_QUOT);` |
|      3 | 1696 |  |
|      - | 1697 | `/*` |
|      - | 1698 | ` * JSON_FORCE_OBJECT.` |
|      - | 1699 | ` *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.` |
|      - | 1700 | ` */` |
|      2 | 1701 | `static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1702 |  |
|      1 | 1703 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1704 | `	ph7_value_int(pVal,JSON_FORCE_OBJECT);` |
|      3 | 1705 |  |
|      - | 1706 | `/*` |
|      - | 1707 | ` * JSON_NUMERIC_CHECK.` |
|      - | 1708 | ` *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.` |
|      - | 1709 | ` */` |
|      2 | 1710 | `static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1711 |  |
|      1 | 1712 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1713 | `	ph7_value_int(pVal,JSON_NUMERIC_CHECK);` |
|      3 | 1714 |  |
|      - | 1715 | `/*` |
|      - | 1716 | ` * JSON_BIGINT_AS_STRING.` |
|      - | 1717 | ` *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.` |
|      - | 1718 | ` */` |
|      2 | 1719 | `static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1720 |  |
|      1 | 1721 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1722 | `	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);` |
|      3 | 1723 |  |
|      - | 1724 | `/*` |
|      - | 1725 | ` * JSON_PRETTY_PRINT.` |
|      - | 1726 | ` *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.` |
|      - | 1727 | ` */` |
|      2 | 1728 | `static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1729 |  |
|      1 | 1730 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1731 | `	ph7_value_int(pVal,JSON_PRETTY_PRINT);` |
|      3 | 1732 |  |
|      - | 1733 | `/*` |
|      - | 1734 | ` * JSON_UNESCAPED_SLASHES.` |
|      - | 1735 | ` *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.` |
|      - | 1736 | ` */` |
|      2 | 1737 | `static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1738 |  |
|      1 | 1739 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1740 | `	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);` |
|      3 | 1741 |  |
|      - | 1742 | `/*` |
|      - | 1743 | ` * JSON_UNESCAPED_UNICODE.` |
|      - | 1744 | ` *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.` |
|      - | 1745 | ` */` |
|      2 | 1746 | `static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1747 |  |
|      1 | 1748 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1749 | `	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);` |
|      3 | 1750 |  |
|      - | 1751 | `/*` |
|      - | 1752 | ` * JSON_ERROR_NONE.` |
|      - | 1753 | ` *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.` |
|      - | 1754 | ` */` |
|      4 | 1755 | `static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1756 |  |
|      2 | 1757 | `	SXUNUSED(pUserData); /* cc warning */` |
|      5 | 1758 | `	ph7_value_int(pVal,JSON_ERROR_NONE);` |
|      5 | 1759 |  |
|      - | 1760 | `/*` |
|      - | 1761 | ` * JSON_ERROR_DEPTH.` |
|      - | 1762 | ` *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.` |
|      - | 1763 | ` */` |
|      2 | 1764 | `static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1765 |  |
|      1 | 1766 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1767 | `	ph7_value_int(pVal,JSON_ERROR_DEPTH);` |
|      3 | 1768 |  |
|      - | 1769 | `/*` |
|      - | 1770 | ` * JSON_ERROR_STATE_MISMATCH.` |
|      - | 1771 | ` *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.` |
|      - | 1772 | ` */` |
|      2 | 1773 | `static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1774 |  |
|      1 | 1775 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1776 | `	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);` |
|      3 | 1777 |  |
|      - | 1778 | `/*` |
|      - | 1779 | ` * JSON_ERROR_CTRL_CHAR.` |
|      - | 1780 | ` *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.` |
|      - | 1781 | ` */` |
|      2 | 1782 | `static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1783 |  |
|      1 | 1784 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1785 | `	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);` |
|      3 | 1786 |  |
|      - | 1787 | `/*` |
|      - | 1788 | ` * JSON_ERROR_SYNTAX.` |
|      - | 1789 | ` *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.` |
|      - | 1790 | ` */` |
|      2 | 1791 | `static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1792 |  |
|      1 | 1793 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1794 | `	ph7_value_int(pVal,JSON_ERROR_SYNTAX);` |
|      3 | 1795 |  |
|      - | 1796 | `/*` |
|      - | 1797 | ` * JSON_ERROR_UTF8.` |
|      - | 1798 | ` *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.` |
|      - | 1799 | ` */` |
|      2 | 1800 | `static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1801 |  |
|      1 | 1802 | `	SXUNUSED(pUserData); /* cc warning */` |
|      3 | 1803 | `	ph7_value_int(pVal,JSON_ERROR_UTF8);` |
|      3 | 1804 |  |
|      - | 1805 | `/*` |
|      - | 1806 | ` * static` |
|      - | 1807 | ` *  Expand the name of the current class. 'static' otherwise.` |
|      - | 1808 | ` */` |
|     12 | 1809 | `static void PH7_static_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1810 |  |
|     13 | 1811 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1812 | `	ph7_class *pClass;` |
|      - | 1813 | `	/* Extract the target class if available */` |
|     13 | 1814 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|     13 | 1815 | `	if( pClass ){` |
|      9 | 1816 | `		SyString *pName = &pClass->sName;` |
|      - | 1817 | `		/* Expand class name */` |
|      9 | 1818 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      5 | 1819 | `	}else{` |
|      - | 1820 | `		/* Expand 'static' */` |
|      5 | 1821 | `		ph7_value_string(pVal,"static",sizeof("static")-1);` |
|      - | 1822 | `	}` |
|     13 | 1823 |  |
|      - | 1824 | `/*` |
|      - | 1825 | ` * self` |
|      - | 1826 | ` * __CLASS__` |
|      - | 1827 | ` *  Expand the name of the current class. NULL otherwise.` |
|      - | 1828 | ` */` |
|      8 | 1829 | `static void PH7_self_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1830 |  |
|      9 | 1831 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1832 | `	ph7_class *pClass;` |
|      - | 1833 |  |
|      - | 1834 | `	/* Get the declaring class of the current method */` |
|      9 | 1835 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|      9 | 1836 | `	if( pClass == 0 ){` |
|      - | 1837 | `		/* Not in a method, fall back to runtime class */` |
|      3 | 1838 | `		pClass = PH7_VmPeekTopClass(pVm);` |
|      1 | 1839 | `	}` |
|      - | 1840 |  |
|      9 | 1841 | `	if( pClass ){` |
|      7 | 1842 | `		SyString *pName = &pClass->sName;` |
|      - | 1843 | `		/* Expand class name */` |
|      7 | 1844 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      4 | 1845 | `	}else{` |
|      - | 1846 | `		/* Expand null */` |
|      3 | 1847 | `		ph7_value_null(pVal);` |
|      - | 1848 | `	}` |
|      9 | 1849 |  |
|      - | 1850 | `/* parent` |
|      - | 1851 | ` *  Expand the name of the parent class. NULL otherwise.` |
|      - | 1852 | ` */` |
|     10 | 1853 | `static void PH7_parent_Const(ph7_value *pVal,void *pUserData)` |
|      1 | 1854 |  |
|     11 | 1855 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - | 1856 | `	ph7_class *pClass;` |
|      - | 1857 |  |
|      - | 1858 | `	/* Get the declaring class, then its parent */` |
|     11 | 1859 | `	pClass = PH7_VmPeekDeclaringClass(pVm);` |
|     15 | 1860 | `	if( pClass && pClass->pBase ){` |
|      9 | 1861 | `		SyString *pName = &pClass->pBase->sName;` |
|      - | 1862 | `		/* Expand parent class name */` |
|      9 | 1863 | `		ph7_value_string(pVal,pName->zString,(int)pName->nByte);` |
|      5 | 1864 | `	}else{` |
|      - | 1865 | `		/* Expand null */` |
|      3 | 1866 | `		ph7_value_null(pVal);` |
|      - | 1867 | `	}` |
|     11 | 1868 |  |
|      - | 1869 |  |
|      - | 1870 | `/*` |
|      - | 1871 | ` * Table of built-in constants.` |
|      - | 1872 | ` */` |
|      - | 1873 | `static const ph7_builtin_constant aBuiltIn[] = {` |
|      - | 1874 | `	{"PH7_VERSION",          PH7_VER_Const      },` |
|      - | 1875 | `	{"PH7_ENGINE",           PH7_VER_Const      },` |
|      - | 1876 | `	{"__PH7__",              PH7_VER_Const      },` |
|      - | 1877 | `	{"PHP_OS",               PH7_OS_Const       },` |
|      - | 1878 | `	{"PHP_EOL",              PH7_EOL_Const      },` |
|      - | 1879 | `	{"PHP_INT_MAX",          PH7_INTMAX_Const   },` |
|      - | 1880 | `	{"MAXINT",               PH7_INTMAX_Const   },` |
|      - | 1881 | `	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },` |
|      - | 1882 | `	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },` |
|      - | 1883 | `	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },` |
|      - | 1884 | `	{"DIR_SEP",              PH7_DIRSEP_Const   },` |
|      - | 1885 | `	{"__TIME__",             PH7_TIME_Const     },` |
|      - | 1886 | `	{"__DATE__",             PH7_DATE_Const     },` |
|      - | 1887 | `	{"__FILE__",             PH7_FILE_Const     },` |
|      - | 1888 | `	{"__DIR__",              PH7_DIR_Const      },` |
|      - | 1889 | `	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },` |
|      - | 1890 | `	{"E_ERROR",              PH7_E_ERROR_Const  },` |
|      - | 1891 | `	{"E_WARNING",            PH7_E_WARNING_Const},` |
|      - | 1892 | `	{"E_PARSE",              PH7_E_PARSE_Const  },` |
|      - | 1893 | `	{"E_NOTICE",             PH7_E_NOTICE_Const },` |
|      - | 1894 | `	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },` |
|      - | 1895 | `	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },` |
|      - | 1896 | `	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },` |
|      - | 1897 | `	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },` |
|      - | 1898 | `	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },` |
|      - | 1899 | `	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },` |
|      - | 1900 | `	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },` |
|      - | 1901 | `	{"E_STRICT",             PH7_E_STRICT_Const        },` |
|      - | 1902 | `	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },` |
|      - | 1903 | `	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },` |
|      - | 1904 | `	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },` |
|      - | 1905 | `	{"E_ALL",                PH7_E_ALL_Const              },` |
|      - | 1906 | `	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },` |
|      - | 1907 | `	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },` |
|      - | 1908 | `	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },` |
|      - | 1909 | `	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},` |
|      - | 1910 | `	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },` |
|      - | 1911 | `	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },` |
|      - | 1912 | `	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },` |
|      - | 1913 | `	{"SORT_ASC",             PH7_SORT_ASC_Const     },` |
|      - | 1914 | `	{"SORT_DESC",            PH7_SORT_DESC_Const    },` |
|      - | 1915 | `	{"SORT_REGULAR",         PH7_SORT_REG_Const     },` |
|      - | 1916 | `	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },` |
|      - | 1917 | `	{"SORT_STRING",          PH7_SORT_STRING_Const  },` |
|      - | 1918 | `	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },` |
|      - | 1919 | `	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },` |
|      - | 1920 | `	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },` |
|      - | 1921 | `	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },` |
|      - | 1922 | `	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },` |
|      - | 1923 | `	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},` |
|      - | 1924 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 1925 | `	{"M_PI",                 PH7_M_PI_Const         },` |
|      - | 1926 | `	{"M_E",                  PH7_M_E_Const          },` |
|      - | 1927 | `	{"M_LOG2E",              PH7_M_LOG2E_Const      },` |
|      - | 1928 | `	{"M_LOG10E",             PH7_M_LOG10E_Const     },` |
|      - | 1929 | `	{"M_LN2",                PH7_M_LN2_Const        },` |
|      - | 1930 | `	{"M_LN10",               PH7_M_LN10_Const       },` |
|      - | 1931 | `	{"M_PI_2",               PH7_M_PI_2_Const       },` |
|      - | 1932 | `	{"M_PI_4",               PH7_M_PI_4_Const       },` |
|      - | 1933 | `	{"M_1_PI",               PH7_M_1_PI_Const       },` |
|      - | 1934 | `	{"M_2_PI",               PH7_M_2_PI_Const       },` |
|      - | 1935 | `	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },` |
|      - | 1936 | `	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },` |
|      - | 1937 | `	{"M_SQRT2",              PH7_M_SQRT2_Const      },` |
|      - | 1938 | `	{"M_SQRT3",              PH7_M_SQRT3_Const      },` |
|      - | 1939 | `	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },` |
|      - | 1940 | `	{"M_LNPI",               PH7_M_LNPI_Const       },` |
|      - | 1941 | `	{"M_EULER",              PH7_M_EULER_Const      },` |
|      - | 1942 | `	{"NAN",                  PH7_NAN_Const          },` |
|      - | 1943 | `	{"INF",                  PH7_INF_Const          },` |
|      - | 1944 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 1945 | `	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },` |
|      - | 1946 | `	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },` |
|      - | 1947 | `	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },` |
|      - | 1948 | `	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },` |
|      - | 1949 | `	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },` |
|      - | 1950 | `	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },` |
|      - | 1951 | `	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },` |
|      - | 1952 | `	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },` |
|      - | 1953 | `	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },` |
|      - | 1954 | `	{"DATE_RSS",             PH7_DATE_RSS_Const     },` |
|      - | 1955 | `	{"DATE_W3C",             PH7_DATE_W3C_Const     },` |
|      - | 1956 | `	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },` |
|      - | 1957 | `	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },` |
|      - | 1958 | `	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },` |
|      - | 1959 | `	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },` |
|      - | 1960 | `	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},` |
|      - | 1961 | `	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},` |
|      - | 1962 | `	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },` |
|      - | 1963 | `	{"ENT_XML1",             PH7_ENT_XML1_Const     },` |
|      - | 1964 | `	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },` |
|      - | 1965 | `	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },` |
|      - | 1966 | `	{"ISO-8859-1",           PH7_ISO88591_Const     },` |
|      - | 1967 | `	{"ISO_8859_1",           PH7_ISO88591_Const     },` |
|      - | 1968 | `	{"UTF-8",                PH7_UTF8_Const         },` |
|      - | 1969 | `	{"UTF8",                 PH7_UTF8_Const         },` |
|      - | 1970 | `	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},` |
|      - | 1971 | `	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },` |
|      - | 1972 | `	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},` |
|      - | 1973 | `	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},` |
|      - | 1974 | `	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},` |
|      - | 1975 | `	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},` |
|      - | 1976 | `	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},` |
|      - | 1977 | `	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},` |
|      - | 1978 | `	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},` |
|      - | 1979 | `	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},` |
|      - | 1980 | `	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},` |
|      - | 1981 | `	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},` |
|      - | 1982 | `	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },` |
|      - | 1983 | `	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },` |
|      - | 1984 | `	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },` |
|      - | 1985 | `	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },` |
|      - | 1986 | `	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },` |
|      - | 1987 | `	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },` |
|      - | 1988 | `	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},` |
|      - | 1989 | `	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },` |
|      - | 1990 | `	{"ASSERT_ACTIVE",        PH7_ASSERT_ACTIVE_Const     },` |
|      - | 1991 | `	{"ASSERT_WARNING",       PH7_ASSERT_WARNING_Const    },` |
|      - | 1992 | `	{"ASSERT_BAIL",          PH7_ASSERT_BAIL_Const       },` |
|      - | 1993 | `	{"ASSERT_QUIET_EVAL",    PH7_ASSERT_QUIET_EVAL_Const },` |
|      - | 1994 | `	{"ASSERT_CALLBACK",      PH7_ASSERT_CALLBACK_Const   },` |
|      - | 1995 | `	{"SEEK_SET",             PH7_SEEK_SET_Const      },` |
|      - | 1996 | `	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },` |
|      - | 1997 | `	{"SEEK_END",             PH7_SEEK_END_Const      },` |
|      - | 1998 | `	{"LOCK_EX",              PH7_LOCK_EX_Const      },` |
|      - | 1999 | `	{"LOCK_SH",              PH7_LOCK_SH_Const      },` |
|      - | 2000 | `	{"LOCK_NB",              PH7_LOCK_NB_Const      },` |
|      - | 2001 | `	{"LOCK_UN",              PH7_LOCK_UN_Const      },` |
|      - | 2002 | `	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},` |
|      - | 2003 | `	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},` |
|      - | 2004 | `	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},` |
|      - | 2005 | `	{"FILE_APPEND",           PH7_FILE_APPEND_Const },` |
|      - | 2006 | `	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },` |
|      - | 2007 | `	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },` |
|      - | 2008 | `	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },` |
|      - | 2009 | `	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },` |
|      - | 2010 | `	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },` |
|      - | 2011 | `	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },` |
|      - | 2012 | `	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},` |
|      - | 2013 | `	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },` |
|      - | 2014 | `	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },` |
|      - | 2015 | `	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },` |
|      - | 2016 | `	{"STDIN",                PH7_STDIN_Const        },` |
|      - | 2017 | `	{"stdin",                PH7_STDIN_Const        },` |
|      - | 2018 | `	{"STDOUT",               PH7_STDOUT_Const       },` |
|      - | 2019 | `	{"stdout",               PH7_STDOUT_Const       },` |
|      - | 2020 | `	{"STDERR",               PH7_STDERR_Const       },` |
|      - | 2021 | `	{"stderr",               PH7_STDERR_Const       },` |
|      - | 2022 | `	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },` |
|      - | 2023 | `	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },` |
|      - | 2024 | `	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },` |
|      - | 2025 | `	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },` |
|      - | 2026 | `	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },` |
|      - | 2027 | `	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },` |
|      - | 2028 | `	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },` |
|      - | 2029 | `	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },` |
|      - | 2030 | `	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},` |
|      - | 2031 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2032 | `	{"XML_ERROR_NONE",       PH7_XML_ERROR_NONE_Const},` |
|      - | 2033 | `	{"XML_ERROR_NO_MEMORY",  PH7_XML_ERROR_NO_MEMORY_Const},` |
|      - | 2034 | `	{"XML_ERROR_SYNTAX",     PH7_XML_ERROR_SYNTAX_Const},` |
|      - | 2035 | `	{"XML_ERROR_NO_ELEMENTS",PH7_XML_ERROR_NO_ELEMENTS_Const},` |
|      - | 2036 | `	{"XML_ERROR_INVALID_TOKEN", PH7_XML_ERROR_INVALID_TOKEN_Const},` |
|      - | 2037 | `	{"XML_ERROR_UNCLOSED_TOKEN",PH7_XML_ERROR_UNCLOSED_TOKEN_Const},` |
|      - | 2038 | `	{"XML_ERROR_PARTIAL_CHAR",  PH7_XML_ERROR_PARTIAL_CHAR_Const},` |
|      - | 2039 | `	{"XML_ERROR_TAG_MISMATCH",  PH7_XML_ERROR_TAG_MISMATCH_Const},` |
|      - | 2040 | `	{"XML_ERROR_DUPLICATE_ATTRIBUTE",   PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const},` |
|      - | 2041 | `	{"XML_ERROR_JUNK_AFTER_DOC_ELEMENT",PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const},` |
|      - | 2042 | `	{"XML_ERROR_PARAM_ENTITY_REF",      PH7_XML_ERROR_PARAM_ENTITY_REF_Const},` |
|      - | 2043 | `	{"XML_ERROR_UNDEFINED_ENTITY",      PH7_XML_ERROR_UNDEFINED_ENTITY_Const},` |
|      - | 2044 | `	{"XML_ERROR_RECURSIVE_ENTITY_REF",  PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const},` |
|      - | 2045 | `	{"XML_ERROR_ASYNC_ENTITY",          PH7_XML_ERROR_ASYNC_ENTITY_Const},` |
|      - | 2046 | `	{"XML_ERROR_BAD_CHAR_REF",          PH7_XML_ERROR_BAD_CHAR_REF_Const},` |
|      - | 2047 | `	{"XML_ERROR_BINARY_ENTITY_REF",     PH7_XML_ERROR_BINARY_ENTITY_REF_Const},` |
|      - | 2048 | `	{"XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF", PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const},` |
|      - | 2049 | `	{"XML_ERROR_MISPLACED_XML_PI",     PH7_XML_ERROR_MISPLACED_XML_PI_Const},` |
|      - | 2050 | `	{"XML_ERROR_UNKNOWN_ENCODING",     PH7_XML_ERROR_UNKNOWN_ENCODING_Const},` |
|      - | 2051 | `	{"XML_ERROR_INCORRECT_ENCODING",   PH7_XML_ERROR_INCORRECT_ENCODING_Const},` |
|      - | 2052 | `	{"XML_ERROR_UNCLOSED_CDATA_SECTION",  PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const},` |
|      - | 2053 | `	{"XML_ERROR_EXTERNAL_ENTITY_HANDLING",PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const},` |
|      - | 2054 | `	{"XML_OPTION_CASE_FOLDING",           PH7_XML_OPTION_CASE_FOLDING_Const},` |
|      - | 2055 | `	{"XML_OPTION_TARGET_ENCODING",        PH7_XML_OPTION_TARGET_ENCODING_Const},` |
|      - | 2056 | `	{"XML_OPTION_SKIP_TAGSTART",          PH7_XML_OPTION_SKIP_TAGSTART_Const},` |
|      - | 2057 | `	{"XML_OPTION_SKIP_WHITE",             PH7_XML_OPTION_SKIP_WHITE_Const},` |
|      - | 2058 | `	{"XML_SAX_IMPL",           PH7_XML_SAX_IMP_Const},` |
|      - | 2059 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2060 | `	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},` |
|      - | 2061 | `	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},` |
|      - | 2062 | `	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},` |
|      - | 2063 | `	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},` |
|      - | 2064 | `	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},` |
|      - | 2065 | `	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},` |
|      - | 2066 | `	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},` |
|      - | 2067 | `	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},` |
|      - | 2068 | `	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},` |
|      - | 2069 | `	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},` |
|      - | 2070 | `	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},` |
|      - | 2071 | `	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},` |
|      - | 2072 | `	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},` |
|      - | 2073 | `	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},` |
|      - | 2074 | `	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},` |
|      - | 2075 | `	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},` |
|      - | 2076 | `	{"static",               PH7_static_Const       },` |
|      - | 2077 | `	{"self",                 PH7_self_Const         },` |
|      - | 2078 | `	{"__CLASS__",            PH7_self_Const         },` |
|      - | 2079 | `	{"parent",               PH7_parent_Const       }` |
|      - | 2080 | `};` |
|      - | 2081 | `/*` |
|      - | 2082 | ` * Register the built-in constants defined above.` |
|      - | 2083 | ` */` |
|    974 | 2084 | `PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)` |
|      2 | 2085 |  |
|      - | 2086 | `	sxu32 n;` |
|      - | 2087 | `	/*` |
|      - | 2088 | `	 * Note that all built-in constants have access to the ph7 virtual machine` |
|      - | 2089 | `	 * that trigger the constant invocation as their private data.` |
|      - | 2090 | `	 */` |
| 197724 | 2091 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){` |
| 196750 | 2092 | `		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));` |
|  98376 | 2093 | `	}` |
|    976 | 2094 |  |
